#include "BACKLIGHT_DRIVER.h"

#include <math.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*  GCC/ARMCC 공통 LUT 저장소 정렬                                            */
/*                                                                            */
/*  감마 LUT는 16-bit 엔트리를 많이 담고 있으므로,                             */
/*  일부 툴체인에서 32-bit 경계 정렬을 강제해 두면 접근 효율과                 */
/*  정렬 안정성이 좋아진다.                                                   */
/* -------------------------------------------------------------------------- */
#if defined(__GNUC__)
#define BACKLIGHT_DRIVER_ALIGNED __attribute__((aligned(4)))
#else
#define BACKLIGHT_DRIVER_ALIGNED
#endif

/* -------------------------------------------------------------------------- */
/*  백라이트 PWM 목표 주파수                                                   */
/*                                                                            */
/*  긴급 패치 배경                                                            */
/*  - 현재 IOC가 만든 TIM3 기본 Period=65535 설정은                            */
/*    저 kHz 대역 PWM을 만든다.                                               */
/*  - PB1/TIM3_CH4 백라이트 PWM이 이 주파수로 스위칭되면                      */
/*    전원/그라운드/아날로그 경로로 누설된 성분이                              */
/*    스피커 출력에 섞여 가청 비프음으로 들릴 수 있다.                        */
/*  - 특히 "밝기에 비례"하고 "완전 소등이면 사라지는" 증상은                 */
/*    PWM duty 기반 누설 노이즈의 전형적인 패턴이다.                          */
/*                                                                            */
/*  대응 전략                                                                  */
/*  - generated tim.c는 CubeMX 재생성 시 덮어써지므로 직접 수정하지 않는다.   */
/*  - 대신 이 non-generated driver에서 TIM3의 ARR를 런타임에                  */
/*    가청 대역 밖으로 재설정한다.                                            */
/*  - TIM3_CH1~CH3를 같이 쓰는 LED 채널 duty도 동일 비율로 재스케일해서       */
/*    기존 LED 밝기 체감이 틀어지지 않게 만든다.                              */
/*                                                                            */
/*  20kHz 선택 이유                                                            */
/*  - 대부분의 사용자에게 가청 상한 밖이다.                                   */
/*  - 84MHz TIM 입력 기준 ARR가 4199가 되어 duty 분해능도 충분하다.           */
/*  - 스위칭 손실을 과도하게 늘리지 않으면서 응급 차단용으로 적절하다.        */
/* -------------------------------------------------------------------------- */
#ifndef BACKLIGHT_DRIVER_TARGET_PWM_HZ
#define BACKLIGHT_DRIVER_TARGET_PWM_HZ 20000u
#endif

/* -------------------------------------------------------------------------- */
/*  공개 상태 저장소                                                           */
/*                                                                            */
/*  상위 App / 디버그 화면 / 현장 로그에서                                    */
/*  마지막 요청 밝기, 실제 적용 duty, 현재 ARR snapshot 등을                  */
/*  확인할 수 있도록 전역 상태를 유지한다.                                    */
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
/*  내부 유틸: 선택된 PWM timer 입력 클록 읽기                                 */
/*                                                                            */
/*  현재 repo 기준                                                             */
/*  - BACKLIGHT_DRIVER_TIM_HANDLE 는 htim3 이고,                              */
/*    TIM3는 APB1 타이머다.                                                   */
/*                                                                            */
/*  하지만 이 helper는 약간 더 일반화해 둔다.                                */
/*  - 나중에 IOC에서 백라이트 timer를 TIM1/TIM8/TIM9 같은 APB2 타이머로       */
/*    옮기더라도, 이 함수가 현재 handle의 Instance를 보고                      */
/*    어느 버스 클록을 써야 하는지 계산한다.                                  */
/*                                                                            */
/*  STM32F4 timer clock 규칙                                                   */
/*  - APB prescaler가 1이면 timer input = PCLK                                */
/*  - APB prescaler가 1이 아니면 timer input = 2 * PCLK                       */
/* -------------------------------------------------------------------------- */
static uint32_t Backlight_Driver_GetPwmTimerInputClockHz(void)
{
    TIM_TypeDef *timer_instance;
    uint32_t peripheral_clock_hz;
    uint32_t timer_input_clock_hz;

    timer_instance = BACKLIGHT_DRIVER_TIM_HANDLE.Instance;

    if ((timer_instance == TIM1)
#if defined(TIM8)
        || (timer_instance == TIM8)
#endif
#if defined(TIM9)
        || (timer_instance == TIM9)
#endif
#if defined(TIM10)
        || (timer_instance == TIM10)
#endif
#if defined(TIM11)
        || (timer_instance == TIM11)
#endif
       )
    {
        peripheral_clock_hz = HAL_RCC_GetPCLK2Freq();
        timer_input_clock_hz = peripheral_clock_hz;

        if ((RCC->CFGR & RCC_CFGR_PPRE2) != RCC_CFGR_PPRE2_DIV1)
        {
            timer_input_clock_hz = peripheral_clock_hz * 2u;
        }
    }
    else
    {
        peripheral_clock_hz = HAL_RCC_GetPCLK1Freq();
        timer_input_clock_hz = peripheral_clock_hz;

        if ((RCC->CFGR & RCC_CFGR_PPRE1) != RCC_CFGR_PPRE1_DIV1)
        {
            timer_input_clock_hz = peripheral_clock_hz * 2u;
        }
    }

    return timer_input_clock_hz;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 현재 PSC 읽기                                                   */
/*                                                                            */
/*  HAL handle의 Init 값은 초기 생성값이고,                                   */
/*  런타임 중에는 실제 레지스터 값이 진실이다.                                */
/*  따라서 PSC는 instance 레지스터를 직접 읽는다.                             */
/* -------------------------------------------------------------------------- */
static uint32_t Backlight_Driver_ReadPrescaler(void)
{
    return (uint32_t)BACKLIGHT_DRIVER_TIM_HANDLE.Instance->PSC;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 현재 ARR 읽기                                                   */
/* -------------------------------------------------------------------------- */
static uint32_t Backlight_Driver_ReadArr(void)
{
    return __HAL_TIM_GET_AUTORELOAD(&BACKLIGHT_DRIVER_TIM_HANDLE);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: ARR 변경 전/후 compare 재스케일                                 */
/*                                                                            */
/*  목적                                                                      */
/*  - TIM3_CH1~CH4는 모두 같은 ARR를 공유한다.                                */
/*  - 그래서 ARR를 65535 -> 4199 같은 식으로 줄이면                           */
/*    기존 CCR 값은 그대로 둘 수 없다.                                        */
/*  - 그대로 두면 CH1~CH3 LED duty와 CH4 백라이트 duty가                      */
/*    전부 다른 밝기로 튀어 버린다.                                           */
/*                                                                            */
/*  이 함수는 "기존 duty 비율"을 유지하도록                                   */
/*      new_compare = old_compare / old_arr * new_arr                         */
/*  를 반올림 포함해 계산한다.                                                 */
/* -------------------------------------------------------------------------- */
static uint32_t Backlight_Driver_ScaleCompareForNewArr(uint32_t old_compare,
                                                        uint32_t old_arr,
                                                        uint32_t new_arr)
{
    uint64_t scaled_compare;

    if (old_arr == 0u)
    {
        return 0u;
    }

    scaled_compare = (((uint64_t)old_compare * (uint64_t)new_arr) +
                      ((uint64_t)old_arr / 2u)) /
                     (uint64_t)old_arr;

    if (scaled_compare > (uint64_t)new_arr)
    {
        scaled_compare = (uint64_t)new_arr;
    }

    return (uint32_t)scaled_compare;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: TIM3 PWM base frequency를 목표 주파수로 재설정                  */
/*                                                                            */
/*  왜 BACKLIGHT_DRIVER에서 하는가                                             */
/*  - generated tim.c의 TIM3.Init.Period는 USER CODE 블록 바깥이라            */
/*    CubeMX 재생성에 의해 언제든 덮어써진다.                                 */
/*  - 반면 이 파일은 non-generated 이므로                                      */
/*    런타임 재설정을 여기서 수행하면 Cube 재생성 내성이 생긴다.              */
/*                                                                            */
/*  왜 TIM3 전체를 같이 만지는가                                               */
/*  - PB1 백라이트는 TIM3_CH4다.                                               */
/*  - LED2/LED3/LED4는 TIM3_CH1/CH2/CH3를 사용한다.                           */
/*  - 즉 TIM3의 ARR는 채널별로 따로 가질 수 없고,                              */
/*    반드시 timer base 전체를 같이 바꿔야 한다.                              */
/*                                                                            */
/*  안전 처리                                                                  */
/*  - 현재 timer가 이미 LED 쪽에서 enable 되었을 수 있으므로                   */
/*    CEN만 잠깐 내리고, CCR1~CCR4를 모두 새 ARR 기준으로 재계산한 뒤          */
/*    update event를 발생시키고 다시 enable 한다.                             */
/*  - 이 작업은 시스템 초기화 시 1회 수행되는 응급 패치 경로다.                */
/* -------------------------------------------------------------------------- */
static void Backlight_Driver_RetuneSharedTim3PwmBase(void)
{
    static const uint32_t s_tim3_channels[4u] =
    {
        TIM_CHANNEL_1,
        TIM_CHANNEL_2,
        TIM_CHANNEL_3,
        TIM_CHANNEL_4
    };

    uint32_t timer_input_clock_hz;
    uint32_t prescaler_value;
    uint64_t timer_divider;
    uint64_t target_period_counts;
    uint32_t old_arr;
    uint32_t new_arr;
    uint32_t old_compare[4u];
    uint32_t new_compare[4u];
    uint32_t channel_index;
    uint8_t  timer_was_enabled;

    timer_input_clock_hz = Backlight_Driver_GetPwmTimerInputClockHz();
    prescaler_value = Backlight_Driver_ReadPrescaler();
    timer_divider = (uint64_t)prescaler_value + 1u;

    if ((timer_input_clock_hz == 0u) || (BACKLIGHT_DRIVER_TARGET_PWM_HZ == 0u))
    {
        return;
    }

    target_period_counts =
        (((uint64_t)timer_input_clock_hz) +
         ((timer_divider * (uint64_t)BACKLIGHT_DRIVER_TARGET_PWM_HZ) / 2u)) /
        (timer_divider * (uint64_t)BACKLIGHT_DRIVER_TARGET_PWM_HZ);

    if (target_period_counts < 2u)
    {
        target_period_counts = 2u;
    }

    new_arr = (uint32_t)(target_period_counts - 1u);
    old_arr = Backlight_Driver_ReadArr();

    if (old_arr == new_arr)
    {
        g_backlight_driver_state.timer_arr = new_arr;
        return;
    }

    for (channel_index = 0u; channel_index < 4u; ++channel_index)
    {
        old_compare[channel_index] =
            __HAL_TIM_GET_COMPARE(&BACKLIGHT_DRIVER_TIM_HANDLE,
                                  s_tim3_channels[channel_index]);

        new_compare[channel_index] =
            Backlight_Driver_ScaleCompareForNewArr(old_compare[channel_index],
                                                   old_arr,
                                                   new_arr);
    }

    timer_was_enabled =
        ((BACKLIGHT_DRIVER_TIM_HANDLE.Instance->CR1 & TIM_CR1_CEN) != 0u) ? 1u : 0u;

    if (timer_was_enabled != 0u)
    {
        __HAL_TIM_DISABLE(&BACKLIGHT_DRIVER_TIM_HANDLE);
    }

    BACKLIGHT_DRIVER_TIM_HANDLE.Init.Period = new_arr;
    __HAL_TIM_SET_AUTORELOAD(&BACKLIGHT_DRIVER_TIM_HANDLE, new_arr);
    __HAL_TIM_SET_COUNTER(&BACKLIGHT_DRIVER_TIM_HANDLE, 0u);

    for (channel_index = 0u; channel_index < 4u; ++channel_index)
    {
        __HAL_TIM_SET_COMPARE(&BACKLIGHT_DRIVER_TIM_HANDLE,
                              s_tim3_channels[channel_index],
                              new_compare[channel_index]);
    }

    /* 강제로 UPDATE 이벤트(UG)를 발생시켜,
     * 새 ARR/CCR 값이 타이머 프리로드 레지스터에서 실제 동작 레지스터로 즉시 반영되게 한다.
     * 일부 HAL 버전에는 __HAL_TIM_GENERATE_EVENT() 매크로가 없으므로,
     * HAL 의존성을 줄이기 위해 EGR 레지스터에 직접 UG 비트를 기록한다.
     */
    BACKLIGHT_DRIVER_TIM_HANDLE.Instance->EGR = TIM_EGR_UG;

    if (timer_was_enabled != 0u)
    {
        __HAL_TIM_ENABLE(&BACKLIGHT_DRIVER_TIM_HANDLE);
    }

    g_backlight_driver_state.timer_arr = new_arr;
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
/*  내부 유틸: electrical Q16 -> CCR counts                                    */
/*                                                                            */
/*  - 0%   -> CCR 0 또는 active-low 반전값                                     */
/*  - 100% -> CCR ARR 또는 active-low 반전값                                   */
/*                                                                            */
/*  이 함수는 항상 "현재 ARR"를 읽고 그 기준으로 compare를 만든다.            */
/*  따라서 init 시 TIM3 ARR를 20kHz용 값으로 바꾼 뒤에도                      */
/*  밝기 계산은 자동으로 새 분해능을 따라간다.                                */
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
/*                                                                            */
/*  실제 PB1/TIM3_CH4 compare 레지스터에 값을 기록하고,                        */
/*  디버그 상태 구조체에도 마지막 적용 결과를 남긴다.                          */
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
/*  - 따라서 여기서는 pin mux를 다시 건드리지 않는다.                         */
/*  - 대신 Cube가 만든 TIM3 base를 runtime에서 20kHz급으로 재설정한 뒤,       */
/*    CH4 PWM start와 초기 duty 적용만 수행한다.                              */
/*                                                                            */
/*  긴급 패치 핵심 순서                                                        */
/*  1) 상태 구조체 초기화                                                      */
/*  2) 감마 LUT 준비                                                           */
/*  3) TIM3 shared base를 가청 밖으로 retune                                   */
/*  4) CH4 compare 0으로 초기화                                                */
/*  5) CH4 PWM start                                                           */
/*  6) 최종 백라이트를 완전 소등 상태로 맞춤                                   */
/* -------------------------------------------------------------------------- */
void Backlight_Driver_Init(void)
{
    HAL_StatusTypeDef hal_status;

    memset((void *)&g_backlight_driver_state, 0, sizeof(g_backlight_driver_state));
    Backlight_Driver_BuildGammaLutIfNeeded();

    Backlight_Driver_RetuneSharedTim3PwmBase();

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
/*                                                                            */
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
