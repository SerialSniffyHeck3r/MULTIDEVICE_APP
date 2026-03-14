#include "LED_Driver.h"
#include "tim.h"

#include <math.h>
#include <string.h>

/* ========================================================================== */
/*  LED_Driver 내부 구현 메모                                                  */
/*                                                                            */
/*  1) CubeMX / IOC 안전성                                                     */
/*     - 이 파일은 generated tim.c의 PSC / ARR / OC mode를 덮지 않는다.       */
/*     - HAL_TIM_PWM_Start()와 CCR(compare) 갱신만 수행한다.                  */
/*                                                                            */
/*  2) 밝기 해석                                                               */
/*     - 상위 계층은 0~65535의 "눈 기준 선형 밝기"를 넘긴다.                 */
/*     - 이 driver는 inverse perceptual mapping을 적용해 전기적 duty로 바꾼다.*/
/*                                                                            */
/*  3) frame rate 제한                                                         */
/*     - 현재 프로젝트의 공통 tick은 HAL_GetTick() 1ms 기반이다.              */
/*     - 기존 17ms(약 58.8fps)는 motion pattern에는 충분했지만,               */
/*       BR ALL 같은 전체 밝기 스윕에서는 계단감이 눈에 띌 수 있다.           */
/*     - 그래서 추가 타이머는 건드리지 않고, render 간격만 5ms로 낮춰        */
/*       밝기 time-axis 양자화를 줄인다.                                      */
/*                                                                            */
/*  4) 테스트 패턴 철학                                                         */
/*     - 생산 / 수리 / bring-up 시 PWM duty/brightness 제어가 정상인지         */
/*       바로 눈으로 확인할 수 있는 패턴을 우선 제공한다.                     */
/*     - 고정 duty, raw PWM sweep, 눈 기준 linear sweep, BR ALL을 포함한다.   */
/* ========================================================================== */

#ifndef LED_DRIVER_OUTPUT_ACTIVE_HIGH
#define LED_DRIVER_OUTPUT_ACTIVE_HIGH       (1u)
#endif

#ifndef LED_DRIVER_USE_CIE_LSTAR
#define LED_DRIVER_USE_CIE_LSTAR            (1u)
#endif

#ifndef LED_DRIVER_GAMMA_VALUE
#define LED_DRIVER_GAMMA_VALUE              (2.20f)
#endif

#ifndef LED_DRIVER_FRAME_INTERVAL_MS
#define LED_DRIVER_FRAME_INTERVAL_MS        (5u)
#endif

#ifndef LED_DRIVER_BREATH_PERIOD_MS
#define LED_DRIVER_BREATH_PERIOD_MS         (2600u)
#endif

#ifndef LED_DRIVER_WELCOME_SWEEP_MS
#define LED_DRIVER_WELCOME_SWEEP_MS         (1200u)
#endif

#ifndef LED_DRIVER_SCANNER_ROUND_TRIP_MS
#define LED_DRIVER_SCANNER_ROUND_TRIP_MS    (1400u)
#endif

#ifndef LED_DRIVER_ALERT_ON_MS
#define LED_DRIVER_ALERT_ON_MS              (180u)
#endif

#ifndef LED_DRIVER_ALERT_OFF_MS
#define LED_DRIVER_ALERT_OFF_MS             (180u)
#endif

#ifndef LED_DRIVER_TEST_STEP_MS
#define LED_DRIVER_TEST_STEP_MS             (140u)
#endif

#ifndef LED_DRIVER_TEST_PHASE_MS
#define LED_DRIVER_TEST_PHASE_MS            (350u)
#endif

#ifndef LED_DRIVER_TEST_STROBE_MS
#define LED_DRIVER_TEST_STROBE_MS           (250u)
#endif

#define LED_DRIVER_CENTER_INDEX             (5u)
#define LED_DRIVER_MIN_BREATH_Q16           ((uint16_t)11796u)

typedef struct
{
    TIM_HandleTypeDef *htim;
    uint32_t           tim_channel;
    const char        *label;
} LED_DriverChannelMap_t;

typedef struct
{
    uint8_t            initialized;
    uint32_t           last_frame_ms;
    LED_DriverPattern_t last_pattern;
    uint32_t           last_mode_started_ms;
    uint16_t           current_frame_q16[LED_DRIVER_CHANNEL_COUNT];
    uint16_t           target_frame_q16[LED_DRIVER_CHANNEL_COUNT];
    uint16_t           last_linear_q16[LED_DRIVER_CHANNEL_COUNT];
    uint32_t           last_pwm_compare[LED_DRIVER_CHANNEL_COUNT];
} LED_DriverState_t;

/* -------------------------------------------------------------------------- */
/*  논리 LED 순서 -> 실제 PWM 채널 매핑                                         */
/*                                                                            */
/*  이 순서가 곧 UI 기준 좌 -> 우 논리 순서다.                                */
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
/*  가운데 강조형 패턴에서 사용하는 공간 가중치                                */
/*                                                                            */
/*  화면 의미                                                                  */
/*  - LED6이 가장 밝은 중심점                                                  */
/*  - LED5/7, LED4/8, ... 로 갈수록 완만하게 낮아짐                           */
/*                                                                            */
/*  UI 위치 설명                                                               */
/*  [LED1][LED2][LED3][LED4][LED5][LED6][LED7][LED8][LED9][LED10][LED11]     */
/*                                      ↑                                     */
/*                                   중심 강조                                 */
/* -------------------------------------------------------------------------- */
static const uint16_t s_center_spatial_weight_q16[LED_DRIVER_CHANNEL_COUNT] =
{
     3277u,  6554u, 11796u, 20971u, 36700u,
    65535u,
    36700u, 20971u, 11796u,  6554u,  3277u
};

static LED_DriverState_t s_led_driver;

static uint16_t LED_Driver_ClampQ16(uint32_t value_q16)
{
    if (value_q16 > LED_DRIVER_Q16_MAX)
    {
        return (uint16_t)LED_DRIVER_Q16_MAX;
    }

    return (uint16_t)value_q16;
}

static uint16_t LED_Driver_ClampPermille(uint16_t value_permille)
{
    if (value_permille > LED_DRIVER_PERMILLE_MAX)
    {
        return (uint16_t)LED_DRIVER_PERMILLE_MAX;
    }

    return value_permille;
}

static uint16_t LED_Driver_PermilleToQ16(uint16_t value_permille)
{
    uint32_t clamped_permille;

    clamped_permille = (uint32_t)LED_Driver_ClampPermille(value_permille);
    return (uint16_t)((clamped_permille * LED_DRIVER_Q16_MAX + 500u) /
                      LED_DRIVER_PERMILLE_MAX);
}

static uint16_t LED_Driver_Q16ToPermille(uint16_t value_q16)
{
    return (uint16_t)((((uint32_t)value_q16) * LED_DRIVER_PERMILLE_MAX +
                       (LED_DRIVER_Q16_MAX / 2u)) / LED_DRIVER_Q16_MAX);
}

/* -------------------------------------------------------------------------- */
/*  Q16 곱셈 helper                                                             */
/*                                                                            */
/*  a, b가 모두 0~65535 범위일 때                                              */
/*      result = a * b / 65535                                                */
/*  를 반올림 포함해서 계산한다.                                               */
/* -------------------------------------------------------------------------- */
static uint16_t LED_Driver_MulQ16(uint16_t a_q16, uint16_t b_q16)
{
    return (uint16_t)((((uint32_t)a_q16 * (uint32_t)b_q16) +
                       (LED_DRIVER_Q16_MAX / 2u)) / LED_DRIVER_Q16_MAX);
}

static void LED_Driver_ClearFrame(uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT])
{
    memset(frame_q16, 0, sizeof(uint16_t) * LED_DRIVER_CHANNEL_COUNT);
}

static void LED_Driver_FillFrame(uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
                                 uint16_t level_q16)
{
    uint8_t index;

    for (index = 0u; index < LED_DRIVER_CHANNEL_COUNT; ++index)
    {
        frame_q16[index] = level_q16;
    }
}

static void LED_Driver_CopyFrame(uint16_t dst_q16[LED_DRIVER_CHANNEL_COUNT],
                                 const uint16_t src_q16[LED_DRIVER_CHANNEL_COUNT])
{
    memcpy(dst_q16,
           src_q16,
           sizeof(uint16_t) * LED_DRIVER_CHANNEL_COUNT);
}

static void LED_Driver_ApplyGlobalScale(uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
                                        uint16_t scale_q16)
{
    uint8_t index;

    for (index = 0u; index < LED_DRIVER_CHANNEL_COUNT; ++index)
    {
        frame_q16[index] = LED_Driver_MulQ16(frame_q16[index], scale_q16);
    }
}

static uint16_t LED_Driver_EaseInOutQuadQ16(uint16_t x_q16)
{
    if (x_q16 < 32768u)
    {
        uint16_t squared_q16;
        uint32_t doubled_q16;

        squared_q16 = LED_Driver_MulQ16(x_q16, x_q16);
        doubled_q16 = (uint32_t)squared_q16 * 2u;
        return LED_Driver_ClampQ16(doubled_q16);
    }
    else
    {
        uint16_t mirrored_x_q16;
        uint16_t squared_q16;
        uint32_t doubled_q16;

        mirrored_x_q16 = (uint16_t)(LED_DRIVER_Q16_MAX - x_q16);
        squared_q16 = LED_Driver_MulQ16(mirrored_x_q16, mirrored_x_q16);
        doubled_q16 = (uint32_t)squared_q16 * 2u;

        if (doubled_q16 > LED_DRIVER_Q16_MAX)
        {
            doubled_q16 = LED_DRIVER_Q16_MAX;
        }

        return (uint16_t)(LED_DRIVER_Q16_MAX - doubled_q16);
    }
}

/* -------------------------------------------------------------------------- */
/*  inverse perceptual mapping                                                  */
/*                                                                            */
/*  입력 값 의미                                                               */
/*  - brightness_q16 = "사람 눈에 선형적으로 보이길 원하는 목표 밝기"         */
/*                                                                            */
/*  출력 값 의미                                                               */
/*  - 실제 LED duty에 해당하는 전기적 밝기                                    */
/*                                                                            */
/*  기본 방식                                                                  */
/*  - inverse CIE L* piecewise function                                       */
/*  - 아주 낮은 밝기 구간은 직선, 나머지는 cubic 관계를 사용한다.             */
/*                                                                            */
/*  대안                                                                       */
/*  - LED_DRIVER_USE_CIE_LSTAR == 0 이면                                      */
/*    단순 power-law gamma(기본 2.2) 방식으로 fallback 가능                   */
/* -------------------------------------------------------------------------- */
static uint16_t LED_Driver_VisualQ16ToElectricalQ16(uint16_t brightness_q16)
{
    float visual_0_to_1;
    float electrical_0_to_1;

    visual_0_to_1 = (float)brightness_q16 / 65535.0f;

#if (LED_DRIVER_USE_CIE_LSTAR == 1u)
    {
        float l_star;

        l_star = visual_0_to_1 * 100.0f;

        if (l_star <= 8.0f)
        {
            electrical_0_to_1 = l_star / 903.3f;
        }
        else
        {
            float f_value;

            f_value = (l_star + 16.0f) / 116.0f;
            electrical_0_to_1 = f_value * f_value * f_value;
        }
    }
#else
    electrical_0_to_1 = powf(visual_0_to_1, LED_DRIVER_GAMMA_VALUE);
#endif

    if (electrical_0_to_1 < 0.0f)
    {
        electrical_0_to_1 = 0.0f;
    }
    else if (electrical_0_to_1 > 1.0f)
    {
        electrical_0_to_1 = 1.0f;
    }

    return (uint16_t)(electrical_0_to_1 * 65535.0f + 0.5f);
}

/* -------------------------------------------------------------------------- */
/*  raw electrical duty -> visual Q16                                          */
/*                                                                            */
/*  PWM duty n% 고정 테스트는 perceptual remap 이후의 실제 전기적 duty가       */
/*  n%가 되도록 맞춰야 한다.                                                   */
/*  따라서 테스트 패턴 전용으로는 inverse mapping의 반대 방향 helper를 쓴다.  */
/* -------------------------------------------------------------------------- */
static uint16_t LED_Driver_ElectricalQ16ToVisualQ16(uint16_t electrical_q16)
{
    float electrical_0_to_1;
    float visual_0_to_1;

    electrical_0_to_1 = (float)electrical_q16 / 65535.0f;

#if (LED_DRIVER_USE_CIE_LSTAR == 1u)
    if (electrical_0_to_1 <= (216.0f / 24389.0f))
    {
        visual_0_to_1 = (903.3f * electrical_0_to_1) / 100.0f;
    }
    else
    {
        visual_0_to_1 = (116.0f * cbrtf(electrical_0_to_1) - 16.0f) / 100.0f;
    }
#else
    if (electrical_0_to_1 <= 0.0f)
    {
        visual_0_to_1 = 0.0f;
    }
    else
    {
        visual_0_to_1 = powf(electrical_0_to_1, 1.0f / LED_DRIVER_GAMMA_VALUE);
    }
#endif

    if (visual_0_to_1 < 0.0f)
    {
        visual_0_to_1 = 0.0f;
    }
    else if (visual_0_to_1 > 1.0f)
    {
        visual_0_to_1 = 1.0f;
    }

    return (uint16_t)(visual_0_to_1 * 65535.0f + 0.5f);
}

/* -------------------------------------------------------------------------- */
/*  현재 logical channel의 ARR를 읽어 compare 값으로 변환                       */
/*                                                                            */
/*  이 변환은 channel별 ARR가 같지 않아도 동작한다.                            */
/*  즉, duty는 항상 현재 timer의 실제 ARR 기준으로 스케일된다.                */
/* -------------------------------------------------------------------------- */
static uint32_t LED_Driver_LinearQ16ToCompare(uint8_t logical_index,
                                              uint16_t brightness_q16)
{
    const LED_DriverChannelMap_t *map;
    uint32_t                      arr_value;
    uint32_t                      compare_value;
    uint16_t                      electrical_q16;

    map = &s_led_channel_map[logical_index];
    electrical_q16 = LED_Driver_VisualQ16ToElectricalQ16(brightness_q16);
    arr_value = __HAL_TIM_GET_AUTORELOAD(map->htim);
    compare_value = (((uint32_t)electrical_q16 * arr_value) +
                     (LED_DRIVER_Q16_MAX / 2u)) / LED_DRIVER_Q16_MAX;

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

static void LED_Driver_WriteCompare(uint8_t logical_index, uint32_t compare_value)
{
    const LED_DriverChannelMap_t *map;

    map = &s_led_channel_map[logical_index];
    __HAL_TIM_SET_COMPARE(map->htim, map->tim_channel, compare_value);
    s_led_driver.last_pwm_compare[logical_index] = compare_value;
}

/* -------------------------------------------------------------------------- */
/*  PWM channel start helper                                                   */
/*                                                                            */
/*  중요한 점                                                                  */
/*  - timer base 재설정 없이 channel start만 수행한다.                         */
/*  - TIM9는 LED11에 해당하는 CH2만 사용한다.                                  */
/*  - 초기 compare는 OFF 상태에 맞춰 0 또는 ARR로 정렬한다.                   */
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

/* -------------------------------------------------------------------------- */
/*  부드러운 전환                                                               */
/*                                                                            */
/*  target frame이 갑자기 바뀌더라도 current frame이 조금씩 따라가게 만든다.  */
/*                                                                            */
/*  transition_ms = 0                                                          */
/*  - 즉시 target과 동일하게 맞춤                                              */
/*                                                                            */
/*  transition_ms > 0                                                          */
/*  - 17ms tick마다 diff의 일부만 이동                                         */
/*  - step이 0이 되는 미세 diff 구간은 최소 1 LSB 이동 보장                   */
/* -------------------------------------------------------------------------- */
static void LED_Driver_AdvanceCurrentFrameTowardTarget(uint16_t transition_ms)
{
    uint8_t index;

    if (transition_ms == 0u)
    {
        LED_Driver_CopyFrame(s_led_driver.current_frame_q16,
                             s_led_driver.target_frame_q16);
        return;
    }

    for (index = 0u; index < LED_DRIVER_CHANNEL_COUNT; ++index)
    {
        int32_t current_value;
        int32_t target_value;
        int32_t diff_value;
        int32_t step_value;

        current_value = (int32_t)s_led_driver.current_frame_q16[index];
        target_value  = (int32_t)s_led_driver.target_frame_q16[index];
        diff_value    = target_value - current_value;

        if (diff_value == 0)
        {
            continue;
        }

        step_value = (diff_value * (int32_t)LED_DRIVER_FRAME_INTERVAL_MS) /
                     (int32_t)transition_ms;

        if (step_value == 0)
        {
            step_value = (diff_value > 0) ? 1 : -1;
        }

        if (((diff_value > 0) && (current_value + step_value > target_value)) ||
            ((diff_value < 0) && (current_value + step_value < target_value)))
        {
            current_value = target_value;
        }
        else
        {
            current_value += step_value;
        }

        if (current_value < 0)
        {
            current_value = 0;
        }
        else if (current_value > (int32_t)LED_DRIVER_Q16_MAX)
        {
            current_value = (int32_t)LED_DRIVER_Q16_MAX;
        }

        s_led_driver.current_frame_q16[index] = (uint16_t)current_value;
    }
}

/* -------------------------------------------------------------------------- */
/*  공용 패턴 helper                                                            */
/* -------------------------------------------------------------------------- */
static void LED_Driver_SetOneLed(uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
                                 uint8_t logical_index,
                                 uint16_t level_q16)
{
    if (logical_index < LED_DRIVER_CHANNEL_COUNT)
    {
        frame_q16[logical_index] = level_q16;
    }
}

static void LED_Driver_SetSymmetricPair(uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
                                        uint8_t left_index,
                                        uint8_t right_index,
                                        uint16_t level_q16)
{
    LED_Driver_SetOneLed(frame_q16, left_index, level_q16);
    LED_Driver_SetOneLed(frame_q16, right_index, level_q16);
}

static uint16_t LED_Driver_BuildTriangleQ16(uint32_t elapsed_ms,
                                           uint32_t period_ms)
{
    uint32_t phase_ms;
    uint32_t phase_q16;

    if (period_ms == 0u)
    {
        return 0u;
    }

    phase_ms = elapsed_ms % period_ms;
    phase_q16 = (phase_ms * LED_DRIVER_Q16_MAX) / period_ms;

    if (phase_q16 < 32768u)
    {
        return LED_Driver_ClampQ16(phase_q16 * 2u);
    }

    return LED_Driver_ClampQ16((LED_DRIVER_Q16_MAX - phase_q16) * 2u);
}

static void LED_Driver_BuildPattern_TestFixedElectricalDuty(
    uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
    uint16_t duty_q16)
{
    LED_Driver_FillFrame(frame_q16,
                         LED_Driver_ElectricalQ16ToVisualQ16(duty_q16));
}

static void LED_Driver_BuildPattern_TestDutySweep(
    uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
    uint32_t elapsed_ms)
{
    uint16_t duty_q16;

    duty_q16 = LED_Driver_BuildTriangleQ16(elapsed_ms,
                                           LED_DRIVER_BREATH_PERIOD_MS);
    LED_Driver_BuildPattern_TestFixedElectricalDuty(frame_q16, duty_q16);
}

static void LED_Driver_BuildPattern_TestLinearBrightnessSweep(
    uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
    uint32_t elapsed_ms)
{
    LED_Driver_FillFrame(frame_q16,
                         LED_Driver_BuildTriangleQ16(elapsed_ms,
                                                     LED_DRIVER_BREATH_PERIOD_MS));
}

static uint16_t LED_Driver_BuildBreathEnvelopeQ16(uint32_t elapsed_ms)
{
    uint32_t phase_ms;
    uint32_t phase_q16;
    uint16_t triangle_q16;
    uint16_t eased_q16;
    uint32_t range_q16;

    phase_ms = elapsed_ms % LED_DRIVER_BREATH_PERIOD_MS;
    phase_q16 = (phase_ms * LED_DRIVER_Q16_MAX) / LED_DRIVER_BREATH_PERIOD_MS;

    if (phase_q16 < 32768u)
    {
        triangle_q16 = LED_Driver_ClampQ16(phase_q16 * 2u);
    }
    else
    {
        triangle_q16 = LED_Driver_ClampQ16((LED_DRIVER_Q16_MAX - phase_q16) * 2u);
    }

    eased_q16 = LED_Driver_EaseInOutQuadQ16(triangle_q16);
    range_q16 = LED_DRIVER_Q16_MAX - (uint32_t)LED_DRIVER_MIN_BREATH_Q16;

    return (uint16_t)(LED_DRIVER_MIN_BREATH_Q16 +
                      ((((uint32_t)eased_q16) * range_q16 +
                        (LED_DRIVER_Q16_MAX / 2u)) / LED_DRIVER_Q16_MAX));
}

/* -------------------------------------------------------------------------- */
/*  일반 모드 렌더러                                                            */
/* -------------------------------------------------------------------------- */
static void LED_Driver_BuildPattern_Off(uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT])
{
    LED_Driver_ClearFrame(frame_q16);
}

static void LED_Driver_BuildPattern_SolidAll(uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
                                             uint16_t level_q16)
{
    LED_Driver_FillFrame(frame_q16, level_q16);
}

static void LED_Driver_BuildPattern_BreathCenter(uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
                                                 uint32_t elapsed_ms)
{
    uint16_t envelope_q16;
    uint8_t  index;

    envelope_q16 = LED_Driver_BuildBreathEnvelopeQ16(elapsed_ms);

    for (index = 0u; index < LED_DRIVER_CHANNEL_COUNT; ++index)
    {
        frame_q16[index] = LED_Driver_MulQ16(s_center_spatial_weight_q16[index],
                                             envelope_q16);
    }
}

/* -------------------------------------------------------------------------- */
/*  Welcome sweep                                                              */
/*                                                                            */
/*  UI 그림 설명                                                               */
/*  - LED1 바깥 왼쪽에서 빛 머리(head)가 들어온다.                             */
/*  - LED11 바깥 오른쪽까지 지나간다.                                          */
/*  - head 뒤에는 꼬리(tail)가 남아 혜성 같은 모양으로 보인다.                */
/*                                                                            */
/*  위치 설명                                                                  */
/*  - 시작점 : LED1의 바깥 왼쪽                                                */
/*  - 진행   : LED1 -> LED11                                                   */
/*  - 종료   : LED11의 바깥 오른쪽                                             */
/* -------------------------------------------------------------------------- */
static void LED_Driver_BuildPattern_WelcomeSweep(uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
                                                 uint32_t elapsed_ms)
{
    int32_t  travel_start_x1000;
    int32_t  travel_end_x1000;
    uint32_t total_travel_x1000;
    int32_t  head_position_x1000;
    uint32_t tail_length_x1000;
    uint8_t  index;

    if (elapsed_ms >= LED_DRIVER_WELCOME_SWEEP_MS)
    {
        LED_Driver_ClearFrame(frame_q16);
        return;
    }

    travel_start_x1000 = -2000;
    travel_end_x1000 = (int32_t)((LED_DRIVER_CHANNEL_COUNT - 1u) * 1000u) + 2000;
    total_travel_x1000 = (uint32_t)(travel_end_x1000 - travel_start_x1000);
    head_position_x1000 = travel_start_x1000 +
                          (int32_t)(((uint64_t)elapsed_ms * total_travel_x1000) /
                                    LED_DRIVER_WELCOME_SWEEP_MS);
    tail_length_x1000 = 2800u;

    for (index = 0u; index < LED_DRIVER_CHANNEL_COUNT; ++index)
    {
        int32_t led_position_x1000;

        led_position_x1000 = (int32_t)index * 1000;

        if (led_position_x1000 > head_position_x1000)
        {
            frame_q16[index] = 0u;
            continue;
        }

        if ((uint32_t)(head_position_x1000 - led_position_x1000) <= tail_length_x1000)
        {
            uint32_t distance_x1000;
            uint32_t brightness_q16;

            distance_x1000 = (uint32_t)(head_position_x1000 - led_position_x1000);
            brightness_q16 = (((tail_length_x1000 - distance_x1000) * LED_DRIVER_Q16_MAX) +
                              (tail_length_x1000 / 2u)) / tail_length_x1000;
            frame_q16[index] = LED_Driver_ClampQ16(brightness_q16);
        }
        else
        {
            frame_q16[index] = 0u;
        }
    }
}

static uint32_t LED_Driver_AbsDiffU32(uint32_t a, uint32_t b)
{
    if (a >= b)
    {
        return (a - b);
    }

    return (b - a);
}

/* -------------------------------------------------------------------------- */
/*  Scanner                                                                    */
/*                                                                            */
/*  UI 그림 설명                                                               */
/*  - 밝은 점 하나가 LED1 -> LED11 -> LED1로 왕복한다.                        */
/*  - 도트의 중심이 채널 사이를 지나갈 때 양 옆 두 LED에 부드럽게 분산된다.   */
/* -------------------------------------------------------------------------- */
static void LED_Driver_BuildPattern_Scanner(uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
                                            uint32_t elapsed_ms)
{
    uint32_t phase_in_round_trip_ms;
    uint32_t half_trip_ms;
    uint32_t travel_x1000;
    uint32_t head_position_x1000;
    uint8_t  index;

    phase_in_round_trip_ms = elapsed_ms % LED_DRIVER_SCANNER_ROUND_TRIP_MS;
    half_trip_ms = LED_DRIVER_SCANNER_ROUND_TRIP_MS / 2u;
    travel_x1000 = (LED_DRIVER_CHANNEL_COUNT - 1u) * 1000u;

    if (phase_in_round_trip_ms <= half_trip_ms)
    {
        head_position_x1000 = ((uint64_t)phase_in_round_trip_ms * travel_x1000) /
                              half_trip_ms;
    }
    else
    {
        uint32_t reverse_phase_ms;

        reverse_phase_ms = phase_in_round_trip_ms - half_trip_ms;
        head_position_x1000 = travel_x1000 -
                              (((uint64_t)reverse_phase_ms * travel_x1000) / half_trip_ms);
    }

    for (index = 0u; index < LED_DRIVER_CHANNEL_COUNT; ++index)
    {
        uint32_t led_position_x1000;
        uint32_t distance_x1000;

        led_position_x1000 = (uint32_t)index * 1000u;
        distance_x1000 = LED_Driver_AbsDiffU32(led_position_x1000, head_position_x1000);

        if (distance_x1000 >= 1000u)
        {
            frame_q16[index] = 0u;
        }
        else
        {
            frame_q16[index] = (uint16_t)((((1000u - distance_x1000) * LED_DRIVER_Q16_MAX) +
                                           500u) / 1000u);
        }
    }
}

static void LED_Driver_BuildPattern_AlertBlink(uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
                                               uint32_t elapsed_ms)
{
    uint32_t cycle_ms;
    uint32_t phase_ms;

    cycle_ms = LED_DRIVER_ALERT_ON_MS + LED_DRIVER_ALERT_OFF_MS;
    phase_ms = elapsed_ms % cycle_ms;

    if (phase_ms < LED_DRIVER_ALERT_ON_MS)
    {
        LED_Driver_FillFrame(frame_q16, LED_DRIVER_Q16_MAX);
    }
    else
    {
        LED_Driver_ClearFrame(frame_q16);
    }
}

static void LED_Driver_BuildPattern_SingleDot(uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
                                              uint8_t logical_index,
                                              uint16_t level_q16)
{
    if (logical_index >= LED_DRIVER_CHANNEL_COUNT)
    {
        logical_index = (uint8_t)(LED_DRIVER_CHANNEL_COUNT - 1u);
    }

    LED_Driver_ClearFrame(frame_q16);
    LED_Driver_SetOneLed(frame_q16, logical_index, level_q16);
}

static void LED_Driver_BuildPattern_CustomFrame(uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
                                                const uint16_t *custom_frame_q16)
{
    if (custom_frame_q16 == NULL)
    {
        LED_Driver_ClearFrame(frame_q16);
        return;
    }

    LED_Driver_CopyFrame(frame_q16, custom_frame_q16);
}

/* -------------------------------------------------------------------------- */
/*  테스트 패턴 렌더러                                                          */
/* -------------------------------------------------------------------------- */
static void LED_Driver_BuildPattern_TestWalkLeftToRight(
    uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
    uint32_t elapsed_ms)
{
    (void)elapsed_ms;

    /* legacy slot 재사용: PWM duty 1% 고정 */
    LED_Driver_BuildPattern_TestFixedElectricalDuty(frame_q16,
                                                    LED_Driver_PermilleToQ16(10u));
}

static void LED_Driver_BuildPattern_TestWalkRightToLeft(
    uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
    uint32_t elapsed_ms)
{
    (void)elapsed_ms;

    /* legacy slot 재사용: PWM duty 5% 고정 */
    LED_Driver_BuildPattern_TestFixedElectricalDuty(frame_q16,
                                                    LED_Driver_PermilleToQ16(50u));
}

static void LED_Driver_BuildPattern_TestCenterOut(uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
                                                  uint32_t elapsed_ms)
{
    (void)elapsed_ms;

    /* legacy slot 재사용: PWM duty 25% 고정 */
    LED_Driver_BuildPattern_TestFixedElectricalDuty(frame_q16,
                                                    LED_Driver_PermilleToQ16(250u));
}

static void LED_Driver_BuildPattern_TestEdgeIn(uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
                                               uint32_t elapsed_ms)
{
    (void)elapsed_ms;

    /* legacy slot 재사용: PWM duty 50% 고정 */
    LED_Driver_BuildPattern_TestFixedElectricalDuty(frame_q16,
                                                    LED_Driver_PermilleToQ16(500u));
}

static void LED_Driver_BuildPattern_TestOddEven(uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
                                                uint32_t elapsed_ms)
{
    (void)elapsed_ms;

    /* legacy slot 재사용: PWM duty 75% 고정 */
    LED_Driver_BuildPattern_TestFixedElectricalDuty(frame_q16,
                                                    LED_Driver_PermilleToQ16(750u));
}

static void LED_Driver_BuildPattern_TestHalfSwap(uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
                                                 uint32_t elapsed_ms)
{
    (void)elapsed_ms;

    /* legacy slot 재사용: PWM duty 90% 고정 */
    LED_Driver_BuildPattern_TestFixedElectricalDuty(frame_q16,
                                                    LED_Driver_PermilleToQ16(900u));
}

static void LED_Driver_BuildPattern_TestBreathAll(uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
                                                  uint32_t elapsed_ms)
{
    uint16_t envelope_q16;

    envelope_q16 = LED_Driver_BuildBreathEnvelopeQ16(elapsed_ms);
    LED_Driver_FillFrame(frame_q16, envelope_q16);
}

static void LED_Driver_BuildPattern_TestBarFill(uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
                                                uint32_t elapsed_ms)
{
    /* legacy slot 재사용: 사람 눈 기준 linear brightness sweep */
    LED_Driver_BuildPattern_TestLinearBrightnessSweep(frame_q16, elapsed_ms);
}

static void LED_Driver_BuildPattern_TestStrobeSlow(uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
                                                   uint32_t elapsed_ms)
{
    (void)elapsed_ms;

    /* legacy slot 재사용: PWM duty 99% 고정 */
    LED_Driver_BuildPattern_TestFixedElectricalDuty(frame_q16,
                                                    LED_Driver_PermilleToQ16(990u));
}

/* -------------------------------------------------------------------------- */
/*  command -> target frame 변환                                               */
/*                                                                            */
/*  이 switch가 실제 그림 생성의 중심이다.                                     */
/*  app 계층은 여기 들어올 pattern과 parameter만 결정한다.                     */
/* -------------------------------------------------------------------------- */
static void LED_Driver_BuildTargetFrame(uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
                                        uint32_t now_ms,
                                        const LED_DriverCommand_t *command)
{
    uint32_t elapsed_ms;

    elapsed_ms = now_ms - command->mode_started_ms;
    LED_Driver_ClearFrame(frame_q16);

    switch (command->pattern)
    {
        case LED_DRIVER_PATTERN_OFF:
            LED_Driver_BuildPattern_Off(frame_q16);
            break;

        case LED_DRIVER_PATTERN_SOLID_ALL:
            LED_Driver_BuildPattern_SolidAll(frame_q16, command->primary_level_q16);
            break;

        case LED_DRIVER_PATTERN_BREATH_CENTER:
            LED_Driver_BuildPattern_BreathCenter(frame_q16, elapsed_ms);
            break;

        case LED_DRIVER_PATTERN_WELCOME_SWEEP:
            LED_Driver_BuildPattern_WelcomeSweep(frame_q16, elapsed_ms);
            break;

        case LED_DRIVER_PATTERN_SCANNER:
            LED_Driver_BuildPattern_Scanner(frame_q16, elapsed_ms);
            break;

        case LED_DRIVER_PATTERN_ALERT_BLINK:
            LED_Driver_BuildPattern_AlertBlink(frame_q16, elapsed_ms);
            break;

        case LED_DRIVER_PATTERN_SINGLE_DOT:
            LED_Driver_BuildPattern_SingleDot(frame_q16,
                                              command->logical_index,
                                              command->primary_level_q16);
            break;

        case LED_DRIVER_PATTERN_CUSTOM_FRAME:
            LED_Driver_BuildPattern_CustomFrame(frame_q16, command->custom_frame_q16);
            break;

        case LED_DRIVER_PATTERN_TEST_ALL_ON:
            LED_Driver_BuildPattern_TestFixedElectricalDuty(frame_q16,
                                                            LED_DRIVER_Q16_MAX);
            break;

        case LED_DRIVER_PATTERN_TEST_WALK_LEFT_TO_RIGHT:
            LED_Driver_BuildPattern_TestWalkLeftToRight(frame_q16, elapsed_ms);
            break;

        case LED_DRIVER_PATTERN_TEST_WALK_RIGHT_TO_LEFT:
            LED_Driver_BuildPattern_TestWalkRightToLeft(frame_q16, elapsed_ms);
            break;

        case LED_DRIVER_PATTERN_TEST_SCANNER:
            LED_Driver_BuildPattern_TestFixedElectricalDuty(frame_q16,
                                                            LED_Driver_PermilleToQ16(100u));
            break;

        case LED_DRIVER_PATTERN_TEST_CENTER_OUT:
            LED_Driver_BuildPattern_TestCenterOut(frame_q16, elapsed_ms);
            break;

        case LED_DRIVER_PATTERN_TEST_EDGE_IN:
            LED_Driver_BuildPattern_TestEdgeIn(frame_q16, elapsed_ms);
            break;

        case LED_DRIVER_PATTERN_TEST_ODD_EVEN:
            LED_Driver_BuildPattern_TestOddEven(frame_q16, elapsed_ms);
            break;

        case LED_DRIVER_PATTERN_TEST_HALF_SWAP:
            LED_Driver_BuildPattern_TestHalfSwap(frame_q16, elapsed_ms);
            break;

        case LED_DRIVER_PATTERN_TEST_BREATH_ALL:
            LED_Driver_BuildPattern_TestBreathAll(frame_q16, elapsed_ms);
            break;

        case LED_DRIVER_PATTERN_TEST_BREATH_CENTER:
            LED_Driver_BuildPattern_TestDutySweep(frame_q16, elapsed_ms);
            break;

        case LED_DRIVER_PATTERN_TEST_BAR_FILL:
            LED_Driver_BuildPattern_TestBarFill(frame_q16, elapsed_ms);
            break;

        case LED_DRIVER_PATTERN_TEST_STROBE_SLOW:
            LED_Driver_BuildPattern_TestStrobeSlow(frame_q16, elapsed_ms);
            break;

        default:
            LED_Driver_BuildPattern_Off(frame_q16);
            break;
    }

    LED_Driver_ApplyGlobalScale(frame_q16, command->global_scale_q16);
}

void LED_Driver_Init(void)
{
    uint8_t logical_index;

    if (s_led_driver.initialized != 0u)
    {
        LED_Driver_AllOff();
        return;
    }

    memset(&s_led_driver, 0, sizeof(s_led_driver));
    s_led_driver.last_pattern = LED_DRIVER_PATTERN_OFF;

    for (logical_index = 0u; logical_index < LED_DRIVER_CHANNEL_COUNT; ++logical_index)
    {
        LED_Driver_StartChannel(logical_index);
    }

    s_led_driver.initialized = 1u;
    s_led_driver.last_frame_ms = HAL_GetTick();
    LED_Driver_AllOff();
}

void LED_Driver_RenderCommand(uint32_t now_ms, const LED_DriverCommand_t *command)
{
    if (s_led_driver.initialized == 0u)
    {
        return;
    }

    if (command == NULL)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  frame rate limiter                                                     */
    /*                                                                        */
    /*  now_ms는 HAL_GetTick() 1ms 단위이므로                                   */
    /*  5ms 미만이면 이번 loop에서는 렌더링을 생략한다.                         */
    /* ---------------------------------------------------------------------- */
    if ((now_ms - s_led_driver.last_frame_ms) < LED_DRIVER_FRAME_INTERVAL_MS)
    {
        return;
    }

    s_led_driver.last_frame_ms = now_ms;
    s_led_driver.last_pattern = command->pattern;
    s_led_driver.last_mode_started_ms = command->mode_started_ms;

    LED_Driver_BuildTargetFrame(s_led_driver.target_frame_q16, now_ms, command);
    LED_Driver_AdvanceCurrentFrameTowardTarget(command->transition_ms);
    LED_Driver_WriteLinearFrameQ16(s_led_driver.current_frame_q16);
}

void LED_Driver_AllOff(void)
{
    uint16_t off_frame_q16[LED_DRIVER_CHANNEL_COUNT];

    LED_Driver_ClearFrame(off_frame_q16);
    LED_Driver_WriteLinearFrameQ16(off_frame_q16);
    LED_Driver_ClearFrame(s_led_driver.current_frame_q16);
    LED_Driver_ClearFrame(s_led_driver.target_frame_q16);
}

void LED_Driver_WriteLinearOneQ16(uint8_t logical_index, uint16_t brightness_q16)
{
    uint16_t clamped_q16;
    uint32_t compare_value;

    if (s_led_driver.initialized == 0u)
    {
        return;
    }

    if (logical_index >= LED_DRIVER_CHANNEL_COUNT)
    {
        return;
    }

    clamped_q16 = LED_Driver_ClampQ16(brightness_q16);
    compare_value = LED_Driver_LinearQ16ToCompare(logical_index, clamped_q16);

    s_led_driver.last_linear_q16[logical_index] = clamped_q16;
    LED_Driver_WriteCompare(logical_index, compare_value);
}

void LED_Driver_WriteLinearFrameQ16(
    const uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT])
{
    uint8_t logical_index;

    if (s_led_driver.initialized == 0u)
    {
        return;
    }

    if (frame_q16 == NULL)
    {
        return;
    }

    for (logical_index = 0u; logical_index < LED_DRIVER_CHANNEL_COUNT; ++logical_index)
    {
        LED_Driver_WriteLinearOneQ16(logical_index, frame_q16[logical_index]);
    }
}

void LED_Driver_WriteLinearOnePermille(uint8_t logical_index,
                                       uint16_t brightness_permille)
{
    LED_Driver_WriteLinearOneQ16(logical_index,
                                 LED_Driver_PermilleToQ16(brightness_permille));
}

void LED_Driver_WriteLinearFramePermille(
    const uint16_t frame_permille[LED_DRIVER_CHANNEL_COUNT])
{
    uint16_t converted_frame_q16[LED_DRIVER_CHANNEL_COUNT];
    uint8_t  index;

    if (frame_permille == NULL)
    {
        return;
    }

    for (index = 0u; index < LED_DRIVER_CHANNEL_COUNT; ++index)
    {
        converted_frame_q16[index] = LED_Driver_PermilleToQ16(frame_permille[index]);
    }

    LED_Driver_WriteLinearFrameQ16(converted_frame_q16);
}

uint16_t LED_Driver_GetLastLinearQ16(uint8_t logical_index)
{
    if (logical_index >= LED_DRIVER_CHANNEL_COUNT)
    {
        return 0u;
    }

    return s_led_driver.last_linear_q16[logical_index];
}

uint16_t LED_Driver_GetLastLinearPermille(uint8_t logical_index)
{
    if (logical_index >= LED_DRIVER_CHANNEL_COUNT)
    {
        return 0u;
    }

    return LED_Driver_Q16ToPermille(s_led_driver.last_linear_q16[logical_index]);
}

uint32_t LED_Driver_GetLastPwmCompare(uint8_t logical_index)
{
    if (logical_index >= LED_DRIVER_CHANNEL_COUNT)
    {
        return 0u;
    }

    return s_led_driver.last_pwm_compare[logical_index];
}
