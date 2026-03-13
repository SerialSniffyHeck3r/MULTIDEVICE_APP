#include "DS18B20_DRIVER.h"

#include <string.h>

/* -------------------------------------------------------------------------- */
/*  1-Wire / DS18B20 command set                                               */
/* -------------------------------------------------------------------------- */

#define DS18B20_CMD_READ_ROM          0x33u
#define DS18B20_CMD_SKIP_ROM          0xCCu
#define DS18B20_CMD_CONVERT_T         0x44u
#define DS18B20_CMD_WRITE_SCRATCHPAD  0x4Eu
#define DS18B20_CMD_READ_SCRATCHPAD   0xBEu

/* -------------------------------------------------------------------------- */
/*  DS18B20 기본 config 값                                                     */
/* -------------------------------------------------------------------------- */

#define DS18B20_DEFAULT_TH_C          125
#define DS18B20_DEFAULT_TL_C          (-55)

/* -------------------------------------------------------------------------- */
/*  GPIO direct access macro                                                   */
/*                                                                            */
/*  CubeMX가 이미 이 핀을 output open-drain 으로 만들어 두었기 때문에           */
/*  mode 전환 없이 "낮춤 / release" 만 해도 1-Wire 동작이 가능하다.            */
/* -------------------------------------------------------------------------- */

#define DS18B20_BUS_LOW() \
    (DS18B20_DRIVER_GPIO_Port->BSRR = ((uint32_t)DS18B20_DRIVER_Pin << 16u))

#define DS18B20_BUS_RELEASE() \
    (DS18B20_DRIVER_GPIO_Port->BSRR = (uint32_t)DS18B20_DRIVER_Pin)

#define DS18B20_BUS_READ() \
    (((DS18B20_DRIVER_GPIO_Port->IDR & (uint32_t)DS18B20_DRIVER_Pin) != 0u) ? 1u : 0u)

/* -------------------------------------------------------------------------- */
/*  드라이버 내부 런타임 상태                                                   */
/* -------------------------------------------------------------------------- */

typedef struct
{
    uint8_t  dwt_ready;             /* DWT cycle counter 사용 가능 여부           */
    uint32_t next_init_retry_ms;    /* 초기화 재시도 예정 시각                    */
} ds18b20_driver_runtime_t;

static ds18b20_driver_runtime_t s_ds18b20_rt;

/* -------------------------------------------------------------------------- */
/*  GPIO self-configuration helper                                            */
/*                                                                            */
/*  이번 수정의 핵심                                                           */
/*  - 기존 코드는 CubeMX가 PE0를 올바른 mode/pull로 잡아 줬다고 가정했다.     */
/*  - 그런데 사용자는 ".ioc 재생성에도 안전" 하길 원했고,                     */
/*    지금 증상도 사실상 "line이 high로 못 올라오는 상태" 와 맞는다.          */
/*  - 그래서 driver init가 스스로 핀 모드와 pull 상태를 다시 잡는다.          */
/* -------------------------------------------------------------------------- */

static void DS18B20_EnablePortClock(GPIO_TypeDef *port)
{
    /* ---------------------------------------------------------------------- */
    /*  DS18B20_DRIVER_GPIO_Port 가 어느 포트로 옮겨가더라도                   */
    /*  최소한 clock enable helper 하나만 바꾸면 되도록 일반화했다.           */
    /* ---------------------------------------------------------------------- */
    if (port == GPIOA) { __HAL_RCC_GPIOA_CLK_ENABLE(); }
    else if (port == GPIOB) { __HAL_RCC_GPIOB_CLK_ENABLE(); }
    else if (port == GPIOC) { __HAL_RCC_GPIOC_CLK_ENABLE(); }
    else if (port == GPIOD) { __HAL_RCC_GPIOD_CLK_ENABLE(); }
    else if (port == GPIOE) { __HAL_RCC_GPIOE_CLK_ENABLE(); }
#if defined(GPIOF)
    else if (port == GPIOF) { __HAL_RCC_GPIOF_CLK_ENABLE(); }
#endif
#if defined(GPIOG)
    else if (port == GPIOG) { __HAL_RCC_GPIOG_CLK_ENABLE(); }
#endif
#if defined(GPIOH)
    else if (port == GPIOH) { __HAL_RCC_GPIOH_CLK_ENABLE(); }
#endif
#if defined(GPIOI)
    else if (port == GPIOI) { __HAL_RCC_GPIOI_CLK_ENABLE(); }
#endif
}

static void DS18B20_ConfigureOwPin(void)
{
    GPIO_InitTypeDef gpio_init;

    /* ---------------------------------------------------------------------- */
    /*  1) 해당 GPIO port clock를 확실히 켠다.                                 */
    /* ---------------------------------------------------------------------- */
    DS18B20_EnablePortClock(DS18B20_DRIVER_GPIO_Port);

    /* ---------------------------------------------------------------------- */
    /*  2) DS18B20 line은 open-drain output 으로 사용한다.                     */
    /*                                                                        */
    /*     release = '1을 써서 라인을 놓아줌'                                  */
    /*     drive low = '0을 써서 라인을 낮춤'                                  */
    /*                                                                        */
    /*  3) 외부 pull-up이 없는 최악을 피하기 위해                              */
    /*     내부 pull-up을 약한 fallback 으로 같이 걸 수 있게 했다.             */
    /* ---------------------------------------------------------------------- */
    memset(&gpio_init, 0, sizeof(gpio_init));
    gpio_init.Pin   = DS18B20_DRIVER_Pin;
    gpio_init.Mode  = GPIO_MODE_OUTPUT_OD;
#if (DS18B20_DRIVER_USE_INTERNAL_PULLUP_FALLBACK != 0u)
    gpio_init.Pull  = GPIO_PULLUP;
#else
    gpio_init.Pull  = GPIO_NOPULL;
#endif
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(DS18B20_DRIVER_GPIO_Port, &gpio_init);

    /* ---------------------------------------------------------------------- */
    /*  핀을 잡자마자 bus를 idle-high 상태로 release 해 둔다.                   */
    /* ---------------------------------------------------------------------- */
    DS18B20_BUS_RELEASE();

    /* ---------------------------------------------------------------------- */
    /*  pull-up과 stray capacitance가 line을 high로 끌어올릴 시간을 조금 준다. */
    /* ---------------------------------------------------------------------- */
    DS18B20_DelayUs(20u);
}

/* -------------------------------------------------------------------------- */
/*  시간 helper                                                                */
/* -------------------------------------------------------------------------- */

static uint8_t DS18B20_TimeDue(uint32_t now_ms, uint32_t due_ms)
{
    return ((int32_t)(now_ms - due_ms) >= 0) ? 1u : 0u;
}

/* -------------------------------------------------------------------------- */
/*  DWT microsecond delay helper                                               */
/* -------------------------------------------------------------------------- */

/* 대체 delay.
 * DWT 사용이 불가능한 상황에서도 아주 거친 busy-wait 로 최소 동작은 하게 한다. */
static void DS18B20_FallbackDelayUs(uint32_t delay_us)
{
    volatile uint32_t loops;
    uint32_t us;

    /* 168MHz 근방에서 대충 1us 내외가 되도록 아주 러프하게 맞춘 값 */
    for (us = 0u; us < delay_us; us++)
    {
        for (loops = 0u; loops < 28u; loops++)
        {
            __NOP();
        }
    }
}

static void DS18B20_DwtInit(void)
{
    /* TRCENA를 켜야 DWT CYCCNT가 돈다. */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* 일부 환경에서는 lock register가 있을 수 있으나 STM32F4 기본 DWT는
     * 여기서 CYCCNTENA만 켜면 충분하다. */
    DWT->CYCCNT = 0u;
    DWT->CTRL  |= DWT_CTRL_CYCCNTENA_Msk;

    s_ds18b20_rt.dwt_ready = ((DWT->CTRL & DWT_CTRL_CYCCNTENA_Msk) != 0u) ? 1u : 0u;
}

static uint32_t DS18B20_CyclesPerUs(void)
{
    uint32_t hclk_hz;

    hclk_hz = HAL_RCC_GetHCLKFreq();
    if (hclk_hz == 0u)
    {
        return 0u;
    }

    return (hclk_hz / 1000000u);
}

void DS18B20_DelayUs(uint32_t delay_us)
{
    uint32_t cycles_per_us;
    uint32_t start_cycle;
    uint32_t target_cycles;

    if (delay_us == 0u)
    {
        return;
    }

    if (s_ds18b20_rt.dwt_ready == 0u)
    {
        DS18B20_FallbackDelayUs(delay_us);
        return;
    }

    cycles_per_us = DS18B20_CyclesPerUs();
    if (cycles_per_us == 0u)
    {
        DS18B20_FallbackDelayUs(delay_us);
        return;
    }

    start_cycle   = DWT->CYCCNT;
    target_cycles = delay_us * cycles_per_us;

    while ((uint32_t)(DWT->CYCCNT - start_cycle) < target_cycles)
    {
        /* busy wait */
    }
}

static uint32_t DS18B20_CyclesToUs(uint32_t cycle_count)
{
    uint32_t cycles_per_us;

    cycles_per_us = DS18B20_CyclesPerUs();
    if (cycles_per_us == 0u)
    {
        return 0u;
    }

    return (cycle_count / cycles_per_us);
}

/* -------------------------------------------------------------------------- */
/*  resolution / config helper                                                 */
/* -------------------------------------------------------------------------- */

static uint8_t DS18B20_ClampResolutionBits(uint8_t resolution_bits)
{
    if (resolution_bits < 9u)
    {
        return 9u;
    }

    if (resolution_bits > 12u)
    {
        return 12u;
    }

    return resolution_bits;
}

static uint8_t DS18B20_ConfigRegFromResolution(uint8_t resolution_bits)
{
    switch (DS18B20_ClampResolutionBits(resolution_bits))
    {
    case 9u:  return 0x1Fu;
    case 10u: return 0x3Fu;
    case 11u: return 0x5Fu;
    case 12u:
    default:
        return 0x7Fu;
    }
}

static uint16_t DS18B20_ConversionTimeMsFromResolution(uint8_t resolution_bits)
{
    switch (DS18B20_ClampResolutionBits(resolution_bits))
    {
    case 9u:  return 94u;
    case 10u: return 188u;
    case 11u: return 375u;
    case 12u:
    default:
        return 750u;
    }
}

/* -------------------------------------------------------------------------- */
/*  CRC8 helper                                                                */
/* -------------------------------------------------------------------------- */

static uint8_t DS18B20_Crc8(const uint8_t *data, uint8_t length)
{
    uint8_t crc;
    uint8_t byte_index;
    uint8_t bit_index;

    if ((data == 0) || (length == 0u))
    {
        return 0u;
    }

    crc = 0u;

    for (byte_index = 0u; byte_index < length; byte_index++)
    {
        uint8_t in_byte = data[byte_index];

        for (bit_index = 0u; bit_index < 8u; bit_index++)
        {
            uint8_t mix = (uint8_t)((crc ^ in_byte) & 0x01u);

            crc >>= 1;
            if (mix != 0u)
            {
                crc ^= 0x8Cu;
            }

            in_byte >>= 1;
        }
    }

    return crc;
}

/* -------------------------------------------------------------------------- */
/*  1-Wire bit timing primitive                                                */
/*                                                                            */
/*  중요                                                                      */
/*  - GPS UART IRQ를 완전히 오래 막지 않기 위해                                */
/*    "슬롯 전체" 가 아니라 "정말 timing이 민감한 몇 us" 만 IRQ를 잠깐 막는다. */
/*  - 긴 delay는 IRQ를 허용한 상태로 보낸다.                                   */
/* -------------------------------------------------------------------------- */

static uint8_t DS18B20_ResetPulse(app_ds18b20_state_t *ds)
{
    uint8_t presence_detected;

    if (ds == 0)
    {
        return 0u;
    }

    ds->debug.bus_reset_count++;

    /* ---------------------------------------------------------------------- */
    /*  1) reset pulse를 넣기 전에                                             */
    /*     bus가 "idle-high" 상태인지 먼저 확인한다.                           */
    /*                                                                        */
    /*  기존 5us 체크는 브레드보드에서 너무 공격적이었다.                      */
    /*  지금은 약간 더 여유를 준 뒤 검사한다.                                  */
    /*                                                                        */
    /*  여기서 low가 읽히면:                                                   */
    /*    - 외부 pull-up이 없음                                                */
    /*    - pull-up이 너무 약함                                                */
    /*    - line이 떠서 low처럼 보임                                           */
    /*    - 센서/배선이 line을 붙잡고 있음                                     */
    /*  중 하나라고 보는 편이 안전하다.                                        */
    /* ---------------------------------------------------------------------- */
    DS18B20_BUS_RELEASE();
    DS18B20_DelayUs(DS18B20_DRIVER_BUS_HIGH_CHECK_US);

    if (DS18B20_BUS_READ() == 0u)
    {
        ds->status_flags &= (uint8_t)~APP_DS18B20_STATUS_PRESENT;
        ds->debug.presence_fail_count++;
        ds->debug.last_error = APP_DS18B20_ERR_BUS_STUCK_LOW;
        return 0u;
    }

    /* ---------------------------------------------------------------------- */
    /*  2) DS18B20 datasheet reset pulse.                                      */
    /*     최소 480us low가 필요하므로 약간 여유 있게 520us를 준다.            */
    /* ---------------------------------------------------------------------- */
    DS18B20_BUS_LOW();
    DS18B20_DelayUs(520u);

    /* ---------------------------------------------------------------------- */
    /*  3) line release 후 약 70us 지점에서 presence를 샘플한다.               */
    /*                                                                        */
    /*  DS18B20은 reset release 이후 15~60us 기다렸다가                        */
    /*  60~240us 정도 low pulse를 보낼 수 있으므로,                            */
    /*  70us sample은 충분히 안전한 편이다.                                    */
    /* ---------------------------------------------------------------------- */
    __disable_irq();
    DS18B20_BUS_RELEASE();
    DS18B20_DelayUs(70u);
    presence_detected = (DS18B20_BUS_READ() == 0u) ? 1u : 0u;
    __enable_irq();

    /* ---------------------------------------------------------------------- */
    /*  4) reset slot의 나머지 시간을 채워서 slave 측 state machine과          */
    /*     timing을 충분히 정렬해 준다.                                        */
    /* ---------------------------------------------------------------------- */
    DS18B20_DelayUs(410u);

    if (presence_detected != 0u)
    {
        ds->status_flags |= APP_DS18B20_STATUS_PRESENT;
        ds->debug.last_error = APP_DS18B20_ERR_NONE;
        return 1u;
    }

    ds->status_flags &= (uint8_t)~APP_DS18B20_STATUS_PRESENT;
    ds->debug.presence_fail_count++;
    ds->debug.last_error = APP_DS18B20_ERR_NO_PRESENCE;
    return 0u;
}

static void DS18B20_WriteBit(uint8_t bit_value)
{
    if (bit_value != 0u)
    {
        /* write-1:
         * low 1~15us, release 후 나머지 slot time */
        __disable_irq();
        DS18B20_BUS_LOW();
        DS18B20_DelayUs(6u);
        DS18B20_BUS_RELEASE();
        DS18B20_DelayUs(9u);
        __enable_irq();

        DS18B20_DelayUs(55u);
    }
    else
    {
        /* write-0:
         * 초반 10us만 보호하고, 나머지는 low를 유지한 채 IRQ 허용 */
        __disable_irq();
        DS18B20_BUS_LOW();
        DS18B20_DelayUs(10u);
        __enable_irq();

        DS18B20_DelayUs(55u);
        DS18B20_BUS_RELEASE();
        DS18B20_DelayUs(5u);
    }
}

static uint8_t DS18B20_ReadBit(void)
{
    uint8_t sampled_bit;

    __disable_irq();
    DS18B20_BUS_LOW();
    DS18B20_DelayUs(6u);
    DS18B20_BUS_RELEASE();
    DS18B20_DelayUs(9u);
    sampled_bit = DS18B20_BUS_READ();
    __enable_irq();

    DS18B20_DelayUs(55u);

    return sampled_bit;
}

static void DS18B20_WriteByte(uint8_t byte_value)
{
    uint8_t bit_index;

    for (bit_index = 0u; bit_index < 8u; bit_index++)
    {
        DS18B20_WriteBit((uint8_t)(byte_value & 0x01u));
        byte_value >>= 1;
    }
}

static uint8_t DS18B20_ReadByte(void)
{
    uint8_t bit_index;
    uint8_t byte_value;

    byte_value = 0u;

    for (bit_index = 0u; bit_index < 8u; bit_index++)
    {
        if (DS18B20_ReadBit() != 0u)
        {
            byte_value |= (uint8_t)(1u << bit_index);
        }
    }

    return byte_value;
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: APP_STATE slice 초기화                                         */
/* -------------------------------------------------------------------------- */

static void DS18B20_ResetAppStateSlice(app_ds18b20_state_t *ds)
{
    if (ds == 0)
    {
        return;
    }

    memset(ds, 0, sizeof(*ds));

    ds->initialized = false;
    ds->status_flags = 0u;
    ds->last_update_ms = 0u;

    ds->raw.temp_c_x100 = APP_DS18B20_TEMP_INVALID;
    ds->raw.temp_f_x100 = APP_DS18B20_TEMP_INVALID;
    ds->raw.resolution_bits = DS18B20_ClampResolutionBits((uint8_t)DS18B20_DRIVER_RESOLUTION_BITS);
    ds->raw.alarm_high_c = DS18B20_DEFAULT_TH_C;
    ds->raw.alarm_low_c  = DS18B20_DEFAULT_TL_C;
    ds->raw.config_reg   = DS18B20_ConfigRegFromResolution((uint8_t)DS18B20_DRIVER_RESOLUTION_BITS);

    ds->debug.phase = APP_DS18B20_PHASE_UNINIT;
    ds->debug.last_error = APP_DS18B20_ERR_NONE;
    ds->debug.conversion_time_ms =
        DS18B20_ConversionTimeMsFromResolution((uint8_t)DS18B20_DRIVER_RESOLUTION_BITS);
}

/* -------------------------------------------------------------------------- */
/*  blocking transaction helper                                                */
/* -------------------------------------------------------------------------- */

static uint8_t DS18B20_ReadRom(app_ds18b20_state_t *ds)
{
    uint8_t rom_code[8];
    uint8_t index;
    uint8_t crc_value;

    if (ds == 0)
    {
        return 0u;
    }

    /* ---------------------------------------------------------------------- */
    /*  READ ROM은 "버스에 센서가 정확히 1개" 라고 가정하는 명령이다.           */
    /*  현재 구조도 단일 DS18B20 전제이므로 이 명령을 그대로 쓴다.             */
    /* ---------------------------------------------------------------------- */
    if (DS18B20_ResetPulse(ds) == 0u)
    {
        return 0u;
    }

    DS18B20_WriteByte(DS18B20_CMD_READ_ROM);

    for (index = 0u; index < 8u; index++)
    {
        rom_code[index] = DS18B20_ReadByte();
    }

    memcpy(ds->raw.rom_code, rom_code, sizeof(rom_code));

    /* ---------------------------------------------------------------------- */
    /*  ROM CRC 검증.                                                          */
    /*                                                                        */
    /*  기존 코드는 CRC가 틀려도 "return 1" 로 성공 취급했다.                  */
    /*  그건 noisy bus에서 init이 거짓 성공으로 보이는 2차 버그다.             */
    /* ---------------------------------------------------------------------- */
    crc_value = DS18B20_Crc8(rom_code, 7u);

    if (crc_value != rom_code[7])
    {
        ds->status_flags &= (uint8_t)~APP_DS18B20_STATUS_ROM_VALID;
        ds->debug.last_error = APP_DS18B20_ERR_ROM_CRC;
        return 0u;
    }

    /* ---------------------------------------------------------------------- */
    /*  family code 확인.                                                      */
    /*                                                                        */
    /*  DS18B20은 보통 0x28 이다.                                              */
    /*  여기서 다른 값이 나오면                                                */
    /*    - line 노이즈                                                        */
    /*    - 다른 1-Wire 디바이스 연결                                          */
    /*    - 배선 문제                                                          */
    /*  가능성을 먼저 의심하는 편이 낫다.                                      */
    /* ---------------------------------------------------------------------- */
    if (rom_code[0] != DS18B20_DRIVER_EXPECT_FAMILY_CODE)
    {
        ds->status_flags &= (uint8_t)~APP_DS18B20_STATUS_ROM_VALID;

        /* ------------------------------------------------------------------ */
        /*  APP_STATE enum을 최소 수정으로 유지하기 위해                        */
        /*  family mismatch도 일단 ROM_CRC bucket에 같이 넣는다.               */
        /* ------------------------------------------------------------------ */
        ds->debug.last_error = APP_DS18B20_ERR_ROM_CRC;
        return 0u;
    }

    ds->status_flags |= APP_DS18B20_STATUS_ROM_VALID;
    ds->debug.last_error = APP_DS18B20_ERR_NONE;
    return 1u;
}

static uint8_t DS18B20_WriteDefaultConfig(app_ds18b20_state_t *ds)
{
    if (ds == 0)
    {
        return 0u;
    }

    if (DS18B20_ResetPulse(ds) == 0u)
    {
        return 0u;
    }

    DS18B20_WriteByte(DS18B20_CMD_SKIP_ROM);
    DS18B20_WriteByte(DS18B20_CMD_WRITE_SCRATCHPAD);
    DS18B20_WriteByte((uint8_t)DS18B20_DEFAULT_TH_C);
    DS18B20_WriteByte((uint8_t)DS18B20_DEFAULT_TL_C);
    DS18B20_WriteByte(DS18B20_ConfigRegFromResolution((uint8_t)DS18B20_DRIVER_RESOLUTION_BITS));

    ds->raw.alarm_high_c  = DS18B20_DEFAULT_TH_C;
    ds->raw.alarm_low_c   = DS18B20_DEFAULT_TL_C;
    ds->raw.config_reg    = DS18B20_ConfigRegFromResolution((uint8_t)DS18B20_DRIVER_RESOLUTION_BITS);
    ds->raw.resolution_bits = DS18B20_ClampResolutionBits((uint8_t)DS18B20_DRIVER_RESOLUTION_BITS);
    ds->debug.conversion_time_ms =
        DS18B20_ConversionTimeMsFromResolution((uint8_t)DS18B20_DRIVER_RESOLUTION_BITS);

    return 1u;
}

static uint8_t DS18B20_StartConversion(app_ds18b20_state_t *ds)
{
    uint32_t start_cycle;

    if (ds == 0)
    {
        return 0u;
    }

    start_cycle = DWT->CYCCNT;

    if (DS18B20_ResetPulse(ds) == 0u)
    {
        ds->debug.transaction_fail_count++;
        ds->debug.last_transaction_us = DS18B20_CyclesToUs((uint32_t)(DWT->CYCCNT - start_cycle));
        return 0u;
    }

    DS18B20_WriteByte(DS18B20_CMD_SKIP_ROM);
    DS18B20_WriteByte(DS18B20_CMD_CONVERT_T);

    ds->debug.last_transaction_us = DS18B20_CyclesToUs((uint32_t)(DWT->CYCCNT - start_cycle));
    ds->debug.last_error = APP_DS18B20_ERR_NONE;

    return 1u;
}

static uint8_t DS18B20_ReadScratchpad(app_ds18b20_state_t *ds, uint32_t now_ms)
{
    uint32_t start_cycle;
    uint8_t scratchpad[9];
    uint8_t index;
    uint8_t crc_expected;
    uint8_t crc_computed;
    int16_t raw_temp;
    int32_t temp_c_x100;
    int16_t temp_f_x100;
    uint8_t resolution_bits;

    if (ds == 0)
    {
        return 0u;
    }

    start_cycle = DWT->CYCCNT;

    if (DS18B20_ResetPulse(ds) == 0u)
    {
        ds->debug.transaction_fail_count++;
        ds->debug.last_error = APP_DS18B20_ERR_READ_TRANSACTION;
        ds->debug.last_transaction_us = DS18B20_CyclesToUs((uint32_t)(DWT->CYCCNT - start_cycle));
        return 0u;
    }

    DS18B20_WriteByte(DS18B20_CMD_SKIP_ROM);
    DS18B20_WriteByte(DS18B20_CMD_READ_SCRATCHPAD);

    for (index = 0u; index < 9u; index++)
    {
        scratchpad[index] = DS18B20_ReadByte();
    }

    ds->debug.last_transaction_us = DS18B20_CyclesToUs((uint32_t)(DWT->CYCCNT - start_cycle));

    crc_expected = scratchpad[8];
    crc_computed = DS18B20_Crc8(scratchpad, 8u);

    memcpy(ds->raw.scratchpad, scratchpad, sizeof(scratchpad));
    ds->raw.crc_expected = crc_expected;
    ds->raw.crc_computed = crc_computed;

    if (crc_expected != crc_computed)
    {
        ds->status_flags &= (uint8_t)~(APP_DS18B20_STATUS_CRC_OK | APP_DS18B20_STATUS_VALID);
        ds->debug.crc_fail_count++;
        ds->debug.last_error = APP_DS18B20_ERR_SCRATCHPAD_CRC;
        return 0u;
    }

    raw_temp = (int16_t)((scratchpad[1] << 8) | scratchpad[0]);

    /* DS18B20 LSB = 1/16 degC */
    temp_c_x100 = ((int32_t)raw_temp * 100) / 16;
    temp_f_x100 = (int16_t)(((temp_c_x100 * 9) / 5) + 3200);

    resolution_bits = (uint8_t)(((scratchpad[4] >> 5) & 0x03u) + 9u);

    ds->raw.timestamp_ms    = now_ms;
    ds->raw.sample_count++;
    ds->raw.raw_temp_lsb    = raw_temp;
    ds->raw.temp_c_x100     = (int16_t)temp_c_x100;
    ds->raw.temp_f_x100     = temp_f_x100;
    ds->raw.alarm_high_c    = (int8_t)scratchpad[2];
    ds->raw.alarm_low_c     = (int8_t)scratchpad[3];
    ds->raw.config_reg      = scratchpad[4];
    ds->raw.resolution_bits = resolution_bits;

    ds->debug.phase              = APP_DS18B20_PHASE_IDLE;
    ds->debug.last_read_complete_ms = now_ms;
    ds->debug.read_complete_count++;
    ds->debug.conversion_time_ms =
        DS18B20_ConversionTimeMsFromResolution(resolution_bits);

    ds->status_flags |= (APP_DS18B20_STATUS_PRESENT |
                         APP_DS18B20_STATUS_VALID   |
                         APP_DS18B20_STATUS_CRC_OK);

    ds->last_update_ms = now_ms;
    ds->debug.last_error = APP_DS18B20_ERR_NONE;

    return 1u;
}

/* -------------------------------------------------------------------------- */
/*  init / re-probe helper                                                     */
/* -------------------------------------------------------------------------- */

static void DS18B20_TryInit(uint32_t now_ms, app_ds18b20_state_t *ds)
{
    uint32_t start_cycle;

    if (ds == 0)
    {
        return;
    }

    ds->debug.init_attempt_count++;
    ds->debug.last_init_attempt_ms = now_ms;

    ds->status_flags &= (uint8_t)~(APP_DS18B20_STATUS_PRESENT |
                                   APP_DS18B20_STATUS_VALID   |
                                   APP_DS18B20_STATUS_BUSY    |
                                   APP_DS18B20_STATUS_CRC_OK);

    start_cycle = DWT->CYCCNT;

    if (DS18B20_ReadRom(ds) == 0u)
    {
        ds->initialized = false;
        ds->debug.phase = APP_DS18B20_PHASE_UNINIT;
        ds->debug.transaction_fail_count++;
        ds->debug.last_transaction_us = DS18B20_CyclesToUs((uint32_t)(DWT->CYCCNT - start_cycle));
        s_ds18b20_rt.next_init_retry_ms = now_ms + DS18B20_DRIVER_RETRY_MS;
        return;
    }

    if (DS18B20_WriteDefaultConfig(ds) == 0u)
    {
        ds->initialized = false;
        ds->debug.phase = APP_DS18B20_PHASE_UNINIT;
        ds->debug.last_error = APP_DS18B20_ERR_CONFIG_WRITE;
        ds->debug.transaction_fail_count++;
        ds->debug.last_transaction_us = DS18B20_CyclesToUs((uint32_t)(DWT->CYCCNT - start_cycle));
        s_ds18b20_rt.next_init_retry_ms = now_ms + DS18B20_DRIVER_RETRY_MS;
        return;
    }

    ds->initialized = true;
    ds->debug.phase = APP_DS18B20_PHASE_IDLE;
    ds->debug.next_action_ms = now_ms;
    ds->last_update_ms = now_ms;
    ds->debug.last_transaction_us = DS18B20_CyclesToUs((uint32_t)(DWT->CYCCNT - start_cycle));
    s_ds18b20_rt.next_init_retry_ms = now_ms + DS18B20_DRIVER_RETRY_MS;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: init                                                              */
/* -------------------------------------------------------------------------- */

void DS18B20_DRIVER_Init(void)
{
    app_ds18b20_state_t *ds;

    ds = (app_ds18b20_state_t *)&g_app_state.ds18b20;

    /* ---------------------------------------------------------------------- */
    /*  드라이버 내부 런타임 상태 초기화                                        */
    /* ---------------------------------------------------------------------- */
    memset(&s_ds18b20_rt, 0, sizeof(s_ds18b20_rt));

    /* ---------------------------------------------------------------------- */
    /*  microsecond delay 기반 확보                                             */
    /* ---------------------------------------------------------------------- */
    DS18B20_DwtInit();

    /* ---------------------------------------------------------------------- */
    /*  APP_STATE.ds18b20 slice를 초기 상태로 되돌린다.                        */
    /* ---------------------------------------------------------------------- */
    DS18B20_ResetAppStateSlice(ds);

    /* ---------------------------------------------------------------------- */
    /*  가장 중요한 변경점:                                                     */
    /*  driver가 1-Wire GPIO를 스스로 다시 잡는다.                             */
    /*                                                                        */
    /*  이렇게 해야 CubeMX / .ioc 재생성으로 GPIO mode/pull 이 바뀌어도         */
    /*  driver init 시점에 다시 "우리가 원하는 형태"로 정렬된다.               */
    /* ---------------------------------------------------------------------- */
    DS18B20_ConfigureOwPin();

    /* ---------------------------------------------------------------------- */
    /*  next retry 시각 초기화                                                  */
    /* ---------------------------------------------------------------------- */
    s_ds18b20_rt.next_init_retry_ms = HAL_GetTick();

    /* ---------------------------------------------------------------------- */
    /*  가능한 경우 즉시 1회 probe                                              */
    /* ---------------------------------------------------------------------- */
    DS18B20_TryInit(HAL_GetTick(), ds);
}



/* -------------------------------------------------------------------------- */
/*  공개 API: SysTick 기반 task                                                 */
/* -------------------------------------------------------------------------- */

void DS18B20_DRIVER_Task(uint32_t now_ms)
{
    app_ds18b20_state_t *ds;

    ds = (app_ds18b20_state_t *)&g_app_state.ds18b20;

    /* ---------------------------------------------------------------------- */
    /*  아직 init가 안 되었으면 일정 주기로 재시도                             */
    /* ---------------------------------------------------------------------- */
    if (ds->initialized == false)
    {
        if (DS18B20_TimeDue(now_ms, s_ds18b20_rt.next_init_retry_ms) != 0u)
        {
            DS18B20_TryInit(now_ms, ds);
        }
        return;
    }

    switch (ds->debug.phase)
    {
    case APP_DS18B20_PHASE_IDLE:
        ds->status_flags &= (uint8_t)~APP_DS18B20_STATUS_BUSY;

        if (DS18B20_TimeDue(now_ms, ds->debug.next_action_ms) != 0u)
        {
            if (DS18B20_StartConversion(ds) != 0u)
            {
                ds->debug.phase = APP_DS18B20_PHASE_WAIT_CONVERSION;
                ds->debug.next_action_ms = now_ms + ds->debug.conversion_time_ms;
                ds->debug.last_conversion_start_ms = now_ms;
                ds->debug.conversion_start_count++;
                ds->status_flags |= APP_DS18B20_STATUS_BUSY;
                ds->last_update_ms = now_ms;
            }
            else
            {
                ds->initialized = false;
                ds->debug.phase = APP_DS18B20_PHASE_UNINIT;
                ds->status_flags &= (uint8_t)~APP_DS18B20_STATUS_BUSY;
                s_ds18b20_rt.next_init_retry_ms = now_ms + DS18B20_DRIVER_RETRY_MS;
            }
        }
        break;

    case APP_DS18B20_PHASE_WAIT_CONVERSION:
        if (DS18B20_TimeDue(now_ms, ds->debug.next_action_ms) != 0u)
        {
            if (DS18B20_ReadScratchpad(ds, now_ms) != 0u)
            {
                ds->debug.phase = APP_DS18B20_PHASE_IDLE;
                ds->debug.next_action_ms = now_ms + DS18B20_DRIVER_PERIOD_MS;
                ds->status_flags &= (uint8_t)~APP_DS18B20_STATUS_BUSY;
            }
            else
            {
                ds->status_flags &= (uint8_t)~APP_DS18B20_STATUS_BUSY;

                /* CRC 실패는 버스 자체가 죽었다고 보지 않고 다음 주기로 넘긴다. */
                if (ds->debug.last_error == APP_DS18B20_ERR_SCRATCHPAD_CRC)
                {
                    ds->debug.phase = APP_DS18B20_PHASE_IDLE;
                    ds->debug.next_action_ms = now_ms + DS18B20_DRIVER_PERIOD_MS;
                }
                else
                {
                    ds->initialized = false;
                    ds->debug.phase = APP_DS18B20_PHASE_UNINIT;
                    s_ds18b20_rt.next_init_retry_ms = now_ms + DS18B20_DRIVER_RETRY_MS;
                }
            }
        }
        break;

    case APP_DS18B20_PHASE_UNINIT:
    default:
        ds->initialized = false;
        ds->debug.phase = APP_DS18B20_PHASE_UNINIT;
        s_ds18b20_rt.next_init_retry_ms = now_ms + DS18B20_DRIVER_RETRY_MS;
        break;
    }
}
