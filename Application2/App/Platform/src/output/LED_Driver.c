#include "LED_Driver.h"
#include "tim.h"

#include <math.h>
#include <string.h>

/* ========================================================================== */
/*  LED_Driver 내부 설계 메모                                                  */
/*                                                                            */
/*  1) 이 프로젝트는 IOC / CubeMX를 적극적으로 사용 중이므로,                 */
/*     generated tim.c 안의 PSC / ARR / mode 설정을 이 파일이 덮지 않는다.    */
/*                                                                            */
/*  2) 이 파일은 PWM 채널 start + compare 갱신만 수행한다.                     */
/*     즉, "이미 생성된 PWM 엔진의 핸들만 사용한다"는 구조다.                 */
/*                                                                            */
/*  3) TIM9는 특별 주의 대상이다.                                              */
/*     - TIM9_CH1 = BEEP_SOUND                                                */
/*     - TIM9_CH2 = LED11                                                     */
/*     따라서 TIM9의 base frequency를 LED 코드가 손대면 안 된다.              */
/*                                                                            */
/*  4) 상위 App 계층은 0~1000 선형 밝기만 주면 된다.                           */
/*     체감 밝기 보정(감마)은 여기서 처리한다.                                 */
/* ========================================================================== */

#ifndef LED_DRIVER_OUTPUT_ACTIVE_HIGH
#define LED_DRIVER_OUTPUT_ACTIVE_HIGH    (1u)
#endif

#ifndef LED_DRIVER_GAMMA_VALUE
#define LED_DRIVER_GAMMA_VALUE           (2.2f)
#endif

#ifndef LED_DRIVER_LUT_POINT_COUNT
#define LED_DRIVER_LUT_POINT_COUNT       (1001u)
#endif

typedef struct
{
    TIM_HandleTypeDef *htim;
    uint32_t           tim_channel;
    const char        *label;
} LED_DriverChannelMap_t;

/* -------------------------------------------------------------------------- */
/*  논리 순서 -> 실제 PWM 채널 매핑                                             */
/*                                                                            */
/*  이 순서가 곧 UI 논리 순서다.                                               */
/*                                                                            */
/*  logical 0  -> LED1  -> PE9  -> TIM1_CH1                                    */
/*  logical 1  -> LED2  -> PA6  -> TIM3_CH1                                    */
/*  logical 2  -> LED3  -> PA7  -> TIM3_CH2                                    */
/*  logical 3  -> LED4  -> PB0  -> TIM3_CH3                                    */
/*  logical 4  -> LED5  -> PC9  -> TIM3_CH4                                    */
/*  logical 5  -> LED6  -> PD12 -> TIM4_CH1                                    */
/*  logical 6  -> LED7  -> PD13 -> TIM4_CH2                                    */
/*  logical 7  -> LED8  -> PD14 -> TIM4_CH3                                    */
/*  logical 8  -> LED9  -> PD15 -> TIM4_CH4                                    */
/*  logical 9  -> LED10 -> PC6  -> TIM8_CH1                                    */
/*  logical 10 -> LED11 -> PE6  -> TIM9_CH2                                    */
/* -------------------------------------------------------------------------- */
static const LED_DriverChannelMap_t s_led_channel_map[LED_DRIVER_CHANNEL_COUNT] =
{
    { &htim1, TIM_CHANNEL_1, "LED1  / PE9  / TIM1_CH1" },
    { &htim3, TIM_CHANNEL_1, "LED2  / PA6  / TIM3_CH1" },
    { &htim3, TIM_CHANNEL_2, "LED3  / PA7  / TIM3_CH2" },
    { &htim3, TIM_CHANNEL_3, "LED4  / PB0  / TIM3_CH3" },
    { &htim3, TIM_CHANNEL_4, "LED5  / PC9  / TIM3_CH4" },
    { &htim4, TIM_CHANNEL_1, "LED6  / PD12 / TIM4_CH1" },
    { &htim4, TIM_CHANNEL_2, "LED7  / PD13 / TIM4_CH2" },
    { &htim4, TIM_CHANNEL_3, "LED8  / PD14 / TIM4_CH3" },
    { &htim4, TIM_CHANNEL_4, "LED9  / PD15 / TIM4_CH4" },
    { &htim8, TIM_CHANNEL_1, "LED10 / PC6  / TIM8_CH1" },
    { &htim9, TIM_CHANNEL_2, "LED11 / PE6  / TIM9_CH2" }
};

/* -------------------------------------------------------------------------- */
/*  내부 버퍼                                                                   */
/*                                                                            */
/*  s_gamma_q16_from_permille[index]                                           */
/*    - index = 0~1000                                                        */
/*    - 값     = 감마 보정된 duty를 0~65535 정규화한 값                        */
/*                                                                            */
/*  s_last_linear_permille[]                                                   */
/*    - 상위 App이 마지막으로 요청한 선형 값                                  */
/*                                                                            */
/*  s_last_pwm_compare[]                                                       */
/*    - 실제 CCR에 마지막으로 기록한 compare 값                               */
/* -------------------------------------------------------------------------- */
static uint16_t s_gamma_q16_from_permille[LED_DRIVER_LUT_POINT_COUNT];
static uint16_t s_last_linear_permille[LED_DRIVER_CHANNEL_COUNT];
static uint32_t s_last_pwm_compare[LED_DRIVER_CHANNEL_COUNT];
static uint8_t  s_driver_initialized = 0u;

static uint16_t LED_Driver_ClampPermille(uint16_t value_permille)
{
    if (value_permille > 1000u)
    {
        return 1000u;
    }

    return value_permille;
}

/* -------------------------------------------------------------------------- */
/*  감마 LUT 생성                                                               */
/*                                                                            */
/*  출력은 compare 값이 아니라 "정규화 duty"다.                              */
/*  이렇게 해 두면 채널별 ARR가 달라도, 현재 ARR를 읽어서 다시 스케일하면 된다. */
/* -------------------------------------------------------------------------- */
static void LED_Driver_BuildGammaLut(void)
{
    uint32_t index;

    for (index = 0u; index < LED_DRIVER_LUT_POINT_COUNT; ++index)
    {
        float    linear_0_to_1;
        float    gamma_corrected_0_to_1;
        uint32_t duty_q16;

        linear_0_to_1 = (float)index / 1000.0f;
        gamma_corrected_0_to_1 = powf(linear_0_to_1, LED_DRIVER_GAMMA_VALUE);
        duty_q16 = (uint32_t)(gamma_corrected_0_to_1 * 65535.0f + 0.5f);

        if (duty_q16 > 65535u)
        {
            duty_q16 = 65535u;
        }

        s_gamma_q16_from_permille[index] = (uint16_t)duty_q16;
    }
}

/* -------------------------------------------------------------------------- */
/*  현재 logical channel의 ARR를 읽어 실제 compare 값으로 변환                  */
/* -------------------------------------------------------------------------- */
static uint32_t LED_Driver_LinearPermilleToCompare(uint8_t logical_index,
                                                   uint16_t brightness_permille)
{
    const LED_DriverChannelMap_t *map;
    uint16_t                      clamped_permille;
    uint32_t                      arr_value;
    uint32_t                      compare_value;
    uint32_t                      gamma_q16;

    map = &s_led_channel_map[logical_index];
    clamped_permille = LED_Driver_ClampPermille(brightness_permille);
    gamma_q16 = (uint32_t)s_gamma_q16_from_permille[clamped_permille];

    /* ---------------------------------------------------------------------- */
    /*  ARR는 0-based auto-reload 값이다.                                      */
    /*  따라서 compare도 0~ARR 범위에서 계산한다.                              */
    /* ---------------------------------------------------------------------- */
    arr_value = __HAL_TIM_GET_AUTORELOAD(map->htim);
    compare_value = (gamma_q16 * arr_value + 32767u) / 65535u;

    if (compare_value > arr_value)
    {
        compare_value = arr_value;
    }

#if (LED_DRIVER_OUTPUT_ACTIVE_HIGH == 1u)
    return compare_value;
#else
    return (arr_value - compare_value);
#endif
}

/* -------------------------------------------------------------------------- */
/*  실제 CCR 기록                                                               */
/* -------------------------------------------------------------------------- */
static void LED_Driver_WriteCompare(uint8_t logical_index,
                                    uint32_t compare_value)
{
    const LED_DriverChannelMap_t *map;

    map = &s_led_channel_map[logical_index];
    __HAL_TIM_SET_COMPARE(map->htim, map->tim_channel, compare_value);
    s_last_pwm_compare[logical_index] = compare_value;
}

/* -------------------------------------------------------------------------- */
/*  채널 시작 helper                                                            */
/*                                                                            */
/*  주의                                                                       */
/*  - TIM9는 CH1이 아니라 CH2만 start 한다.                                    */
/*  - 이 함수는 timer base 재설정 없이 HAL_TIM_PWM_Start만 수행한다.           */
/* -------------------------------------------------------------------------- */
static void LED_Driver_StartChannel(uint8_t logical_index)
{
    const LED_DriverChannelMap_t *map;

    map = &s_led_channel_map[logical_index];

#if (LED_DRIVER_OUTPUT_ACTIVE_HIGH == 1u)
    __HAL_TIM_SET_COMPARE(map->htim, map->tim_channel, 0u);
#else
    __HAL_TIM_SET_COMPARE(map->htim,
                          map->tim_channel,
                          __HAL_TIM_GET_AUTORELOAD(map->htim));
#endif

    (void)HAL_TIM_PWM_Start(map->htim, map->tim_channel);
}

void LED_Driver_Init(void)
{
    uint8_t logical_index;

    if (s_driver_initialized != 0u)
    {
        LED_Driver_AllOff();
        return;
    }

    LED_Driver_BuildGammaLut();
    memset(s_last_linear_permille, 0, sizeof(s_last_linear_permille));
    memset(s_last_pwm_compare, 0, sizeof(s_last_pwm_compare));

    for (logical_index = 0u;
         logical_index < LED_DRIVER_CHANNEL_COUNT;
         ++logical_index)
    {
        LED_Driver_StartChannel(logical_index);
    }

    s_driver_initialized = 1u;
    LED_Driver_AllOff();
}

void LED_Driver_AllOff(void)
{
    uint16_t off_frame[LED_DRIVER_CHANNEL_COUNT];

    memset(off_frame, 0, sizeof(off_frame));
    LED_Driver_WriteLinearFramePermille(off_frame);
}

void LED_Driver_WriteLinearOnePermille(uint8_t logical_index,
                                       uint16_t brightness_permille)
{
    uint16_t clamped_permille;
    uint32_t compare_value;

    if (s_driver_initialized == 0u)
    {
        return;
    }

    if (logical_index >= LED_DRIVER_CHANNEL_COUNT)
    {
        return;
    }

    clamped_permille = LED_Driver_ClampPermille(brightness_permille);
    compare_value = LED_Driver_LinearPermilleToCompare(logical_index,
                                                       clamped_permille);

    s_last_linear_permille[logical_index] = clamped_permille;
    LED_Driver_WriteCompare(logical_index, compare_value);
}

void LED_Driver_WriteLinearFramePermille(
    const uint16_t frame_permille[LED_DRIVER_CHANNEL_COUNT])
{
    uint8_t logical_index;

    if (s_driver_initialized == 0u)
    {
        return;
    }

    if (frame_permille == NULL)
    {
        return;
    }

    for (logical_index = 0u;
         logical_index < LED_DRIVER_CHANNEL_COUNT;
         ++logical_index)
    {
        LED_Driver_WriteLinearOnePermille(logical_index,
                                          frame_permille[logical_index]);
    }
}

uint16_t LED_Driver_GetLastLinearPermille(uint8_t logical_index)
{
    if (logical_index >= LED_DRIVER_CHANNEL_COUNT)
    {
        return 0u;
    }

    return s_last_linear_permille[logical_index];
}

uint32_t LED_Driver_GetLastPwmCompare(uint8_t logical_index)
{
    if (logical_index >= LED_DRIVER_CHANNEL_COUNT)
    {
        return 0u;
    }

    return s_last_pwm_compare[logical_index];
}
