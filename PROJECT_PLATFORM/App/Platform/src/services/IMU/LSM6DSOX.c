#include "LSM6DSOX.h"

static int32_t LSM6DSOX_ReadReg(const lsm6dsox_device_t *dev,
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

static int32_t LSM6DSOX_WriteReg(const lsm6dsox_device_t *dev,
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

static int32_t LSM6DSOX_WriteRegU8(const lsm6dsox_device_t *dev,
                                   uint8_t reg,
                                   uint8_t value)
{
    return LSM6DSOX_WriteReg(dev, reg, &value, 1u);
}

static int32_t LSM6DSOX_ReadRegU8(const lsm6dsox_device_t *dev,
                                  uint8_t reg,
                                  uint8_t *value)
{
    return LSM6DSOX_ReadReg(dev, reg, value, 1u);
}

int32_t LSM6DSOX_Init(lsm6dsox_device_t *dev)
{
    uint8_t reg;

    if (dev == 0)
    {
        return -1;
    }

    dev->who_am_i         = 0u;
    dev->ctrl1_xl_shadow  = 0u;
    dev->ctrl2_g_shadow   = 0u;
    dev->ctrl3_c_shadow   = 0u;
    dev->ctrl9_xl_shadow  = 0u;
    dev->last_status_reg  = 0u;

    if (LSM6DSOX_ReadRegU8(dev, LSM6DSOX_REG_WHO_AM_I, &reg) != 0)
    {
        return -1;
    }

    dev->who_am_i = reg;
    if (reg != LSM6DSOX_WHO_AM_I_VALUE)
    {
        return -1;
    }

    /* ---------------------------------------------------------------------- */
    /*  SW reset                                                               */
    /*  CTRL3_C bit0 = SW_RESET                                                */
    /* ---------------------------------------------------------------------- */
    if (LSM6DSOX_WriteRegU8(dev, LSM6DSOX_REG_CTRL3_C, 0x01u) != 0)
    {
        return -1;
    }

    if (dev->io.delay_ms != 0)
    {
        dev->io.delay_ms(dev->io.ctx, 10u);
    }

    /* ---------------------------------------------------------------------- */
    /*  CTRL3_C                                                                 */
    /*  bit2 IF_INC  : multi-byte auto increment                               */
    /*  bit6 BDU     : high/low byte tearing 방지                               */
    /* ---------------------------------------------------------------------- */
    reg = 0u;
    if (dev->cfg.enable_auto_increment != 0u)
    {
        reg |= 0x04u;
    }
    if (dev->cfg.enable_block_data_update != 0u)
    {
        reg |= 0x40u;
    }

    if (LSM6DSOX_WriteRegU8(dev, LSM6DSOX_REG_CTRL3_C, reg) != 0)
    {
        return -1;
    }
    dev->ctrl3_c_shadow = reg;

    /* ---------------------------------------------------------------------- */
    /*  CTRL9_XL bit1 = I3C_DISABLE                                            */
    /* ---------------------------------------------------------------------- */
    reg = 0u;
    if (dev->cfg.disable_i3c != 0u)
    {
        reg |= 0x02u;
    }

    if (LSM6DSOX_WriteRegU8(dev, LSM6DSOX_REG_CTRL9_XL, reg) != 0)
    {
        return -1;
    }
    dev->ctrl9_xl_shadow = reg;

    /* ---------------------------------------------------------------------- */
    /*  CTRL1_XL                                                                */
    /*  [7:4] ODR_XL, [3:2] FS_XL, bit1 LPF2_XL_EN                             */
    /* ---------------------------------------------------------------------- */
    reg = (uint8_t)(((dev->cfg.accel_odr & 0x0Fu) << 4) |
                    ((dev->cfg.accel_fs  & 0x03u) << 2));
    if (dev->cfg.enable_accel_lpf2 != 0u)
    {
        reg |= 0x02u;
    }

    if (LSM6DSOX_WriteRegU8(dev, LSM6DSOX_REG_CTRL1_XL, reg) != 0)
    {
        return -1;
    }
    dev->ctrl1_xl_shadow = reg;

    /* ---------------------------------------------------------------------- */
    /*  CTRL2_G                                                                 */
    /*  [7:4] ODR_G, [3:1] FS_G                                                */
    /* ---------------------------------------------------------------------- */
    reg = (uint8_t)(((dev->cfg.gyro_odr & 0x0Fu) << 4) |
                    ((dev->cfg.gyro_fs  & 0x07u) << 1));

    if (LSM6DSOX_WriteRegU8(dev, LSM6DSOX_REG_CTRL2_G, reg) != 0)
    {
        return -1;
    }
    dev->ctrl2_g_shadow = reg;

    (void)LSM6DSOX_ReadRegU8(dev, LSM6DSOX_REG_STATUS, &dev->last_status_reg);

    return 0;
}

int32_t LSM6DSOX_ReadSample(lsm6dsox_device_t *dev,
                            lsm6dsox_sample_t *sample,
                            uint8_t *new_sample)
{
    uint8_t status_reg;
    uint8_t raw[14];

    if (new_sample != 0)
    {
        *new_sample = 0u;
    }

    if ((dev == 0) || (sample == 0))
    {
        return -1;
    }

    if (LSM6DSOX_ReadRegU8(dev, LSM6DSOX_REG_STATUS, &status_reg) != 0)
    {
        return -1;
    }
    dev->last_status_reg = status_reg;

    /* accel + gyro 둘 다 새 데이터가 준비된 순간만 publish 해서                 */
    /* 동일 샘플 재중복 카운트를 줄인다.                                          */
    if ((status_reg & (LSM6DSOX_STATUS_XLDA | LSM6DSOX_STATUS_GDA)) !=
        (LSM6DSOX_STATUS_XLDA | LSM6DSOX_STATUS_GDA))
    {
        return 0;
    }

    if (LSM6DSOX_ReadReg(dev, LSM6DSOX_REG_OUT_TEMP_L, raw, 14u) != 0)
    {
        return -1;
    }

    sample->who_am_i        = dev->who_am_i;
    sample->ctrl1_xl_shadow = dev->ctrl1_xl_shadow;
    sample->ctrl2_g_shadow  = dev->ctrl2_g_shadow;
    sample->ctrl3_c_shadow  = dev->ctrl3_c_shadow;
    sample->ctrl9_xl_shadow = dev->ctrl9_xl_shadow;
    sample->status_reg      = status_reg;

    sample->raw_temp    = (int16_t)(((uint16_t)raw[1]  << 8) | raw[0]);
    sample->raw_gyro_x  = (int16_t)(((uint16_t)raw[3]  << 8) | raw[2]);
    sample->raw_gyro_y  = (int16_t)(((uint16_t)raw[5]  << 8) | raw[4]);
    sample->raw_gyro_z  = (int16_t)(((uint16_t)raw[7]  << 8) | raw[6]);
    sample->raw_accel_x = (int16_t)(((uint16_t)raw[9]  << 8) | raw[8]);
    sample->raw_accel_y = (int16_t)(((uint16_t)raw[11] << 8) | raw[10]);
    sample->raw_accel_z = (int16_t)(((uint16_t)raw[13] << 8) | raw[12]);

    /* ST 공식 driver: degC = raw / 256 + 25 */
    sample->temp_cdeg = 2500 + (int32_t)((sample->raw_temp * 100) / 256);

    if (new_sample != 0)
    {
        *new_sample = 1u;
    }

    return 0;
}
