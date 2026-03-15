#include "BACKLIGHT_DRIVER.h"

#include <math.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*  GCC/ARMCC 공통 LUT 저장소 정렬                                            */
/* -------------------------------------------------------------------------- */
#if defined(__GNUC__)
#define BACKLIGHT_DRIVER_ALIGNED __attribute__((aligned(4)))
#else
#define BACKLIGHT_DRIVER_ALIGNED
#endif

/* -------------------------------------------------------------------------- */
/*  공개 상태 저장소                                                           */
/* -------------------------------------------------------------------------- */
volatile backlight_driver_state_t g_backlight_driver_state;

/* -------------------------------------------------------------------------- */
/*  감마 LUT                                                                   */
/*                                                                            */
/*  index 0..4096  ->  입력 linear 밝기 0..1                                  */
/*  value 0..65535 ->  출력 electrical duty 0..1                              */
/*                                                                            */
/*  LUT는 init 시 1회만 powf()로 생성하고,                                    */
/*  실시간 경로에서는 보간만 수행한다.                                        */
/* -------------------------------------------------------------------------- */
static BACKLIGHT_DRIVER_ALIGNED uint16_t
    s_backlight_gamma_lut_q16[BACKLIGHT_DRIVER_GAMMA_LUT_RESOLUTION + 1u];
static uint8_t s_backlight_gamma_lut_ready = 0u;

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 0..1000 permille clamp                                           */
/* -------------------------------------------------------------------------- */
static uint16_t Backlight_Driver_ClampPermille(uint32_t value_permille)
{
    if (value_permille > 1000u)
    {
        return 1000u;
    }

    return (uint16_t)value_permille;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: Q16 clamp                                                       */
/* -------------------------------------------------------------------------- */
static uint16_t Backlight_Driver_ClampQ16(uint32_t value_q16)
{
    if (value_q16 > BACKLIGHT_DRIVER_Q16_MAX)
    {
        return (uint16_t)BACKLIGHT_DRIVER_Q16_MAX;
    }

    return (uint16_t)value_q16;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: LUT 생성                                                        */
/*                                                                            */
/*  사람 눈 기준의 선형 밝기를 electrical duty로 바꾸는 감마 커브를           */
/*  4096축으로 미리 구워 둔다.                                                */
/* -------------------------------------------------------------------------- */
static void Backlight_Driver_BuildGammaLutIfNeeded(void)
{
    uint32_t index;

    if (s_backlight_gamma_lut_ready != 0u)
    {
        return;
    }

    for (index = 0u; index <= BACKLIGHT_DRIVER_GAMMA_LUT_RESOLUTION; ++index)
    {
        float x_linear_0_to_1;
        float y_electrical_0_to_1;
        uint32_t q16_value;

        x_linear_0_to_1 =
            (float)index / (float)BACKLIGHT_DRIVER_GAMMA_LUT_RESOLUTION;
        y_electrical_0_to_1 = powf(x_linear_0_to_1, BACKLIGHT_DRIVER_GAMMA_VALUE);

        if (y_electrical_0_to_1 < 0.0f)
        {
            y_electrical_0_to_1 = 0.0f;
        }
        else if (y_electrical_0_to_1 > 1.0f)
        {
            y_electrical_0_to_1 = 1.0f;
        }

        q16_value = (uint32_t)lroundf(y_electrical_0_to_1 * 65535.0f);
        s_backlight_gamma_lut_q16[index] = Backlight_Driver_ClampQ16(q16_value);
    }

    s_backlight_gamma_lut_ready = 1u;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: linear Q16 -> electrical Q16                                    */
/*                                                                            */
/*  4096축 LUT 인접 두 포인트를 선형 보간해서 계단감을 줄인다.                */
/* -------------------------------------------------------------------------- */
static uint16_t Backlight_Driver_MapLinearQ16ToElectricalQ16(uint16_t linear_q16)
{
    uint32_t scaled_index_q16;
    uint32_t index;
    uint32_t frac_q16;
    uint32_t y0;
    uint32_t y1;
    uint32_t interpolated;

    Backlight_Driver_BuildGammaLutIfNeeded();

    if (linear_q16 >= BACKLIGHT_DRIVER_Q16_MAX)
    {
        return BACKLIGHT_DRIVER_Q16_MAX;
    }

    scaled_index_q16 =
        ((uint32_t)linear_q16 * BACKLIGHT_DRIVER_GAMMA_LUT_RESOLUTION);
    index = scaled_index_q16 / BACKLIGHT_DRIVER_Q16_MAX;
    frac_q16 = scaled_index_q16 % BACKLIGHT_DRIVER_Q16_MAX;

    if (index >= BACKLIGHT_DRIVER_GAMMA_LUT_RESOLUTION)
    {
        index = BACKLIGHT_DRIVER_GAMMA_LUT_RESOLUTION - 1u;
        frac_q16 = BACKLIGHT_DRIVER_Q16_MAX;
    }

    y0 = s_backlight_gamma_lut_q16[index];
    y1 = s_backlight_gamma_lut_q16[index + 1u];

    interpolated =
        (uint32_t)(y0 + (((uint64_t)(y1 - y0) * (uint64_t)frac_q16 +
                          (BACKLIGHT_DRIVER_Q16_MAX / 2u)) /
                         BACKLIGHT_DRIVER_Q16_MAX));

    if ((interpolated != 0u) &&
        (interpolated < BACKLIGHT_DRIVER_MIN_NONZERO_ELECTRICAL_Q16))
    {
        interpolated = BACKLIGHT_DRIVER_MIN_NONZERO_ELECTRICAL_Q16;
    }

    return Backlight_Driver_ClampQ16(interpolated);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: Q16 -> permille                                                  */
/* -------------------------------------------------------------------------- */
static uint16_t Backlight_Driver_Q16ToPermille(uint16_t value_q16)
{
    return (uint16_t)((((uint32_t)value_q16) * 1000u +
                       (BACKLIGHT_DRIVER_Q16_MAX / 2u)) /
                      BACKLIGHT_DRIVER_Q16_MAX);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: permille -> Q16                                                  */
/* -------------------------------------------------------------------------- */
static uint16_t Backlight_Driver_PermilleToQ16(uint16_t value_permille)
{
    uint32_t clamped_permille;

    clamped_permille = Backlight_Driver_ClampPermille(value_permille);
    return (uint16_t)((clamped_permille * BACKLIGHT_DRIVER_Q16_MAX + 500u) /
                      1000u);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 현재 ARR 읽기                                                   */
/* -------------------------------------------------------------------------- */
static uint32_t Backlight_Driver_ReadArr(void)
{
    return __HAL_TIM_GET_AUTORELOAD(&BACKLIGHT_DRIVER_TIM_HANDLE);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: electrical Q16 -> CCR counts                                    */
/*                                                                            */
/*  - 0%  -> CCR 0                                                            */
/*  - 100% -> CCR ARR                                                         */
/* -------------------------------------------------------------------------- */
static uint32_t Backlight_Driver_ElectricalQ16ToCompare(uint16_t electrical_q16)
{
    uint32_t arr_value;
    uint32_t compare_value;

    arr_value = Backlight_Driver_ReadArr();
    g_backlight_driver_state.timer_arr = arr_value;

    compare_value =
        (uint32_t)((((uint64_t)electrical_q16 * (uint64_t)arr_value) +
                    (BACKLIGHT_DRIVER_Q16_MAX / 2u)) /
                   BACKLIGHT_DRIVER_Q16_MAX);

    if (compare_value > arr_value)
    {
        compare_value = arr_value;
    }

#if (BACKLIGHT_DRIVER_OUTPUT_ACTIVE_HIGH == 1u)
    return compare_value;
#else
    return (arr_value - compare_value);
#endif
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: CCR 기록                                                        */
/* -------------------------------------------------------------------------- */
static void Backlight_Driver_WriteCompare(uint32_t compare_value,
                                          uint16_t electrical_q16)
{
    __HAL_TIM_SET_COMPARE(&BACKLIGHT_DRIVER_TIM_HANDLE,
                          BACKLIGHT_DRIVER_TIM_CHANNEL,
                          compare_value);

    g_backlight_driver_state.last_compare_counts = compare_value;
    g_backlight_driver_state.applied_electrical_q16 = electrical_q16;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: ready 여부                                                       */
/* -------------------------------------------------------------------------- */
bool Backlight_Driver_IsReady(void)
{
    return ((g_backlight_driver_state.initialized == true) &&
            (g_backlight_driver_state.pwm_running == true));
}

/* -------------------------------------------------------------------------- */
/*  공개 API: init                                                             */
/*                                                                            */
/*  중요                                                                      */
/*  - CubeMX가 이미 TIM3 CH4와 PB1 alternate function을 생성했다고 가정한다.  */
/*  - 따라서 여기서는 HAL_TIM_PWM_Start만 호출한다.                           */
/*  - 이전 코드처럼 다른 AF로 재바인딩하거나 PWMN으로 시작하지 않는다.        */
/* -------------------------------------------------------------------------- */
void Backlight_Driver_Init(void)
{
    HAL_StatusTypeDef hal_status;

    memset((void *)&g_backlight_driver_state, 0, sizeof(g_backlight_driver_state));
    Backlight_Driver_BuildGammaLutIfNeeded();

    __HAL_TIM_SET_COMPARE(&BACKLIGHT_DRIVER_TIM_HANDLE,
                          BACKLIGHT_DRIVER_TIM_CHANNEL,
                          0u);

    hal_status = HAL_TIM_PWM_Start(&BACKLIGHT_DRIVER_TIM_HANDLE,
                                   BACKLIGHT_DRIVER_TIM_CHANNEL);
    g_backlight_driver_state.last_hal_status = (uint32_t)hal_status;

    if (hal_status != HAL_OK)
    {
        g_backlight_driver_state.initialized = false;
        g_backlight_driver_state.pwm_running = false;
        return;
    }

    g_backlight_driver_state.initialized = true;
    g_backlight_driver_state.pwm_running = true;
    g_backlight_driver_state.timer_arr = Backlight_Driver_ReadArr();

    Backlight_Driver_SetRawPwmQ16(0u);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: deinit                                                           */
/*                                                                            */
/*  이름은 기존 호환을 위해 유지하지만,                                       */
/*  더 이상 PB1을 GPIO로 강제 재설정하지 않는다.                              */
/*  이유                                                                      */
/*  - PB1 소유권은 Cube TIM3_CH4 PWM에 맡긴다.                                */
/*  - GPIO 재설정은 다시 Cube/runtime 충돌을 만들 수 있다.                    */
/* -------------------------------------------------------------------------- */
void Backlight_Driver_DeInitToGpioLow(void)
{
    (void)HAL_TIM_PWM_Stop(&BACKLIGHT_DRIVER_TIM_HANDLE,
                           BACKLIGHT_DRIVER_TIM_CHANNEL);

    __HAL_TIM_SET_COMPARE(&BACKLIGHT_DRIVER_TIM_HANDLE,
                          BACKLIGHT_DRIVER_TIM_CHANNEL,
                          0u);

    g_backlight_driver_state.initialized = false;
    g_backlight_driver_state.pwm_running = false;
    g_backlight_driver_state.requested_linear_q16 = 0u;
    g_backlight_driver_state.applied_electrical_q16 = 0u;
    g_backlight_driver_state.last_compare_counts = 0u;
    g_backlight_driver_state.timer_arr = Backlight_Driver_ReadArr();
}

/* -------------------------------------------------------------------------- */
/*  공개 API: raw electrical Q16 직접 적용                                    */
/* -------------------------------------------------------------------------- */
void Backlight_Driver_SetRawPwmQ16(uint16_t electrical_q16)
{
    uint16_t clamped_q16;
    uint32_t compare_value;

    if (Backlight_Driver_IsReady() == false)
    {
        return;
    }

    clamped_q16 = electrical_q16;

    if ((clamped_q16 != 0u) &&
        (clamped_q16 < BACKLIGHT_DRIVER_MIN_NONZERO_ELECTRICAL_Q16))
    {
        clamped_q16 = BACKLIGHT_DRIVER_MIN_NONZERO_ELECTRICAL_Q16;
    }

    compare_value = Backlight_Driver_ElectricalQ16ToCompare(clamped_q16);
    Backlight_Driver_WriteCompare(compare_value, clamped_q16);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: linear Q16 적용                                                  */
/* -------------------------------------------------------------------------- */
void Backlight_Driver_SetLinearQ16(uint16_t linear_q16)
{
    uint16_t electrical_q16;
    uint32_t compare_value;

    if (Backlight_Driver_IsReady() == false)
    {
        return;
    }

    g_backlight_driver_state.requested_linear_q16 = linear_q16;

    electrical_q16 = Backlight_Driver_MapLinearQ16ToElectricalQ16(linear_q16);
    compare_value = Backlight_Driver_ElectricalQ16ToCompare(electrical_q16);
    Backlight_Driver_WriteCompare(compare_value, electrical_q16);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 호환용 permille wrapper                                           */
/* -------------------------------------------------------------------------- */
void Backlight_Driver_SetLinearPermille(uint16_t linear_permille)
{
    Backlight_Driver_SetLinearQ16(
        Backlight_Driver_PermilleToQ16(linear_permille));
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 호환용 raw PWM permille wrapper                                  */
/* -------------------------------------------------------------------------- */
void Backlight_Driver_SetRawPwmPermille(uint16_t pwm_permille)
{
    Backlight_Driver_SetRawPwmQ16(
        Backlight_Driver_PermilleToQ16(pwm_permille));
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 호환용 매핑 query                                                 */
/* -------------------------------------------------------------------------- */
uint16_t Backlight_Driver_MapLinearToGammaPermille(uint16_t linear_permille)
{
    uint16_t electrical_q16;

    electrical_q16 = Backlight_Driver_MapLinearQ16ToElectricalQ16(
        Backlight_Driver_PermilleToQ16(linear_permille));

    return Backlight_Driver_Q16ToPermille(electrical_q16);
}
