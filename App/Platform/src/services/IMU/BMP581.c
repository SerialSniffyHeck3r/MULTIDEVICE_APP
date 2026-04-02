#include "BMP581.h"

/* -------------------------------------------------------------------------- */
/*  local helpers                                                              */
/* -------------------------------------------------------------------------- */
static int32_t BMP581_ReadReg(const bmp581_device_t *dev,
                              uint8_t reg,
                              uint8_t *data,
                              uint16_t len)
{
    if ((dev == 0) || (dev->io.read == 0) || (data == 0))
    {
        return -1;
    }

    return dev->io.read(dev->io.ctx, dev->cfg.i2c_addr, reg, data, len);
}

static int32_t BMP581_WriteReg(const bmp581_device_t *dev,
                               uint8_t reg,
                               const uint8_t *data,
                               uint16_t len)
{
    if ((dev == 0) || (dev->io.write == 0) || (data == 0))
    {
        return -1;
    }

    return dev->io.write(dev->io.ctx, dev->cfg.i2c_addr, reg, data, len);
}

static int32_t BMP581_WriteRegU8(const bmp581_device_t *dev,
                                 uint8_t reg,
                                 uint8_t value)
{
    return BMP581_WriteReg(dev, reg, &value, 1u);
}

static int32_t BMP581_ReadRegU8(const bmp581_device_t *dev,
                                uint8_t reg,
                                uint8_t *value)
{
    return BMP581_ReadReg(dev, reg, value, 1u);
}

static int32_t BMP581_IsAcceptedChipId(const bmp581_device_t *dev, uint8_t chip_id)
{
    if (chip_id == BMP581_CHIP_ID_PRIMARY)
    {
        return 1;
    }

    if ((chip_id == BMP581_CHIP_ID_SECONDARY) && (dev->cfg.accept_chip_id_0x51 != 0u))
    {
        return 1;
    }

    return 0;
}

static int32_t BMP581_ReadbackConfigShadows(bmp581_device_t *dev)
{
    uint8_t reg_data[2];

    if (BMP581_ReadReg(dev, BMP581_REG_DSP_CONFIG, reg_data, 2u) != 0)
    {
        return -1;
    }

    dev->dsp_config_shadow = reg_data[0];
    dev->dsp_iir_shadow    = reg_data[1];

    if (BMP581_ReadReg(dev, BMP581_REG_OSR_CONFIG, reg_data, 2u) != 0)
    {
        return -1;
    }

    dev->osr_config_shadow = reg_data[0];
    dev->odr_config_shadow = reg_data[1];

    return 0;
}

/* -------------------------------------------------------------------------- */
/*  Public API                                                                 */
/* -------------------------------------------------------------------------- */
int32_t BMP581_Init(bmp581_device_t *dev)
{
    uint8_t reg;
    uint8_t reg_pair[2];

    if (dev == 0)
    {
        return -1;
    }

    dev->chip_id           = 0u;
    dev->rev_id            = 0u;
    dev->last_int_status   = 0u;
    dev->last_status       = 0u;
    dev->last_chip_status  = 0u;
    dev->osr_config_shadow = 0u;
    dev->odr_config_shadow = 0u;
    dev->dsp_config_shadow = 0u;
    dev->dsp_iir_shadow    = 0u;

    /* ---------------------------------------------------------------------- */
    /*  1) 칩 ID 확인                                                           */
    /* ---------------------------------------------------------------------- */
    if (BMP581_ReadRegU8(dev, BMP581_REG_CHIP_ID, &reg) != 0)
    {
        return -1;
    }

    dev->chip_id = reg;
    if (BMP581_IsAcceptedChipId(dev, reg) == 0)
    {
        return -1;
    }

    if (BMP581_ReadRegU8(dev, BMP581_REG_REV_ID, &reg) != 0)
    {
        return -1;
    }
    dev->rev_id = reg;

    /* ---------------------------------------------------------------------- */
    /*  2) 소프트 리셋                                                          */
    /* ---------------------------------------------------------------------- */
    if (BMP581_WriteRegU8(dev, BMP581_REG_CMD, BMP581_CMD_SOFT_RESET) != 0)
    {
        return -1;
    }

    if (dev->io.delay_ms != 0)
    {
        /* Bosch SensorAPI 는 2ms 이상을 사용한다. 여유를 조금 더 둔다. */
        dev->io.delay_ms(dev->io.ctx, 3u);
    }

    (void)BMP581_ReadRegU8(dev, BMP581_REG_CHIP_STATUS, &dev->last_chip_status);

    /* ---------------------------------------------------------------------- */
    /*  3) 먼저 standby + deep-standby disable 로 정렬                         */
    /*     - IIR / OSR / ODR 설정은 standby 에서 건드리는 편이 안전하다.       */
    /* ---------------------------------------------------------------------- */
    reg = BMP581_DEEP_DISABLE_MASK | BMP581_POWERMODE_STANDBY;
    if (BMP581_WriteRegU8(dev, BMP581_REG_ODR_CONFIG, reg) != 0)
    {
        return -1;
    }

    /* ---------------------------------------------------------------------- */
    /*  4) DSP/IIR 설정                                                        */
    /*     - reg 0x30: shadow/filter route 관련 비트                           */
    /*       여기서는 모두 default(0) 로 둔다.                                 */
    /*     - reg 0x31: temp/pressure IIR coefficient                           */
    /* ---------------------------------------------------------------------- */
    reg_pair[0] = 0u;
    reg_pair[1] = (uint8_t)(((dev->cfg.iir_temp  & 0x07u) << BMP581_TEMP_IIR_SHIFT) |
                            ((dev->cfg.iir_press & 0x07u) << BMP581_PRESS_IIR_SHIFT));
    if (BMP581_WriteReg(dev, BMP581_REG_DSP_CONFIG, reg_pair, 2u) != 0)
    {
        return -1;
    }

    /* ---------------------------------------------------------------------- */
    /*  5) OSR / pressure enable / ODR 설정                                    */
    /* ---------------------------------------------------------------------- */
    reg_pair[0] = (uint8_t)(((dev->cfg.osr_temp  & 0x07u) << BMP581_TEMP_OS_SHIFT) |
                            ((dev->cfg.osr_press & 0x07u) << BMP581_PRESS_OS_SHIFT) |
                            BMP581_PRESS_ENABLE_MASK);

    reg_pair[1] = (uint8_t)(BMP581_DEEP_DISABLE_MASK |
                            ((dev->cfg.odr & 0x1Fu) << BMP581_ODR_SHIFT) |
                            BMP581_POWERMODE_CONTINUOUS);

    if (BMP581_WriteReg(dev, BMP581_REG_OSR_CONFIG, reg_pair, 2u) != 0)
    {
        return -1;
    }

    if (BMP581_ReadbackConfigShadows(dev) != 0)
    {
        return -1;
    }

    (void)BMP581_ReadRegU8(dev, BMP581_REG_CHIP_STATUS, &dev->last_chip_status);
    (void)BMP581_ReadRegU8(dev, BMP581_REG_INT_STATUS, &dev->last_int_status);
    (void)BMP581_ReadRegU8(dev, BMP581_REG_STATUS, &dev->last_status);

    return 0;
}

int32_t BMP581_ReadSample(bmp581_device_t *dev,
                          bmp581_sample_t *sample,
                          uint8_t *new_sample)
{
    uint8_t int_status;
    uint8_t status_reg;
    uint8_t data[6];
    uint32_t raw_press_u24;
    uint32_t raw_temp_u24;
    int32_t raw_temp_s24;
    int32_t temp_cdeg;
    int32_t pressure_pa;

    if (new_sample != 0)
    {
        *new_sample = 0u;
    }

    if ((dev == 0) || (sample == 0))
    {
        return -1;
    }

    /* ---------------------------------------------------------------------- */
    /*  Continuous mode 에서는 DRDY bit 로 "이번 poll 에 새 sample 이 있는가"   */
    /*  를 확인해서 같은 샘플 중복 publish 를 피한다.                           */
    /* ---------------------------------------------------------------------- */
    if (BMP581_ReadRegU8(dev, BMP581_REG_INT_STATUS, &int_status) != 0)
    {
        return -1;
    }
    dev->last_int_status = int_status;

    if (BMP581_ReadRegU8(dev, BMP581_REG_STATUS, &status_reg) != 0)
    {
        return -1;
    }
    dev->last_status = status_reg;

    if ((int_status & BMP581_INT_STATUS_DRDY) == 0u)
    {
        return 0;
    }

    if (BMP581_ReadReg(dev, BMP581_REG_TEMP_DATA_XLSB, data, 6u) != 0)
    {
        return -1;
    }

    /* Temperature: signed 24-bit, degC = raw / 65536 */
    raw_temp_u24 = ((uint32_t)data[2] << 16) | ((uint32_t)data[1] << 8) | (uint32_t)data[0];
    raw_temp_s24 = (int32_t)(raw_temp_u24 & 0x00FFFFFFu);
    if ((raw_temp_s24 & 0x00800000L) != 0)
    {
        raw_temp_s24 |= (int32_t)0xFF000000L;
    }

    /* Pressure: unsigned 24-bit, Pa = raw / 64 */
    raw_press_u24 = ((uint32_t)data[5] << 16) | ((uint32_t)data[4] << 8) | (uint32_t)data[3];

    temp_cdeg   = (int32_t)((raw_temp_s24 * 100L) / 65536L);
    pressure_pa = (int32_t)(raw_press_u24 / 64u);

    sample->chip_id            = dev->chip_id;
    sample->rev_id             = dev->rev_id;
    sample->last_int_status    = int_status;
    sample->last_status        = status_reg;
    sample->last_chip_status   = dev->last_chip_status;
    sample->osr_config_shadow  = dev->osr_config_shadow;
    sample->odr_config_shadow  = dev->odr_config_shadow;
    sample->dsp_config_shadow  = dev->dsp_config_shadow;
    sample->dsp_iir_shadow     = dev->dsp_iir_shadow;
    sample->raw_pressure_u24   = raw_press_u24;
    sample->raw_temperature_s24 = raw_temp_s24;
    sample->pressure_pa        = pressure_pa;
    sample->pressure_hpa_x100  = pressure_pa; /* hPa*100 == Pa numerically */
    sample->temp_cdeg          = temp_cdeg;

    if (new_sample != 0)
    {
        *new_sample = 1u;
    }

    return 0;
}
