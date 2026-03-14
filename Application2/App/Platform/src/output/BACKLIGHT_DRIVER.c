#include "BACKLIGHT_DRIVER.h"

#include <string.h>

/* -------------------------------------------------------------------------- */
/*  내부 감마 LUT                                                              */
/*                                                                            */
/*  이 테이블은 linear perceived brightness(0~1000)를                          */
/*  실제 PWM duty(0~1000)로 재매핑한다.                                       */
/*                                                                            */
/*  해석 방법                                                                  */
/*  - 인덱스 0   : 0%                                                           */
/*  - 인덱스 100 : 100%                                                         */
/*  - 중간값은 1% 간격                                                          */
/*  - 실제 입력은 0~1000 permille 이므로,                                      */
/*    함수에서 인접 두 포인트를 선형 보간해서 부드럽게 쓴다.                  */
/*                                                                            */
/*  장점                                                                      */
/*  - powf() 없이도 감마 커브를 재현하므로                                     */
/*    bare-metal / MCU 환경에서 코드 크기와 의존성을 줄일 수 있다.            */
/* -------------------------------------------------------------------------- */
static const uint16_t s_backlight_gamma_lut_permille[101] =
{
       0u,    0u,    0u,    0u,    1u,    1u,    2u,    3u,    4u,    5u,
       6u,    8u,    9u,   11u,   13u,   15u,   18u,   20u,   23u,   26u,
      29u,   32u,   36u,   39u,   43u,   47u,   52u,   56u,   61u,   66u,
      71u,   76u,   82u,   87u,   93u,   99u,  106u,  112u,  119u,  126u,
     133u,  141u,  148u,  156u,  164u,  173u,  181u,  190u,  199u,  208u,
     218u,  227u,  237u,  247u,  258u,  268u,  279u,  290u,  302u,  313u,
     325u,  337u,  349u,  362u,  375u,  388u,  401u,  414u,  428u,  442u,
     456u,  471u,  485u,  500u,  516u,  531u,  547u,  563u,  579u,  595u,
     612u,  629u,  646u,  664u,  681u,  699u,  718u,  736u,  755u,  774u,
     793u,  813u,  832u,  852u,  873u,  893u,  914u,  935u,  957u,  978u,
    1000u
};

/* -------------------------------------------------------------------------- */
/*  공개 상태 저장소                                                           */
/* -------------------------------------------------------------------------- */
volatile backlight_driver_state_t g_backlight_driver_state;

/* -------------------------------------------------------------------------- */
/*  내부 유틸: GPIO port clock enable                                          */
/*                                                                            */
/*  CubeMX가 이미 대체로 enable해 주지만,                                      */
/*  이 드라이버 단독 재사용성 및 안전성을 위해                                 */
/*  필요한 포트 clock를 한 번 더 보장한다.                                     */
/* -------------------------------------------------------------------------- */
static void Backlight_Driver_EnableGpioClock(void)
{
    if (BACKLIGHT_DRIVER_GPIO_PORT == GPIOA)
    {
        __HAL_RCC_GPIOA_CLK_ENABLE();
    }
    else if (BACKLIGHT_DRIVER_GPIO_PORT == GPIOB)
    {
        __HAL_RCC_GPIOB_CLK_ENABLE();
    }
    else if (BACKLIGHT_DRIVER_GPIO_PORT == GPIOC)
    {
        __HAL_RCC_GPIOC_CLK_ENABLE();
    }
    else if (BACKLIGHT_DRIVER_GPIO_PORT == GPIOD)
    {
        __HAL_RCC_GPIOD_CLK_ENABLE();
    }
    else if (BACKLIGHT_DRIVER_GPIO_PORT == GPIOE)
    {
        __HAL_RCC_GPIOE_CLK_ENABLE();
    }
    else if (BACKLIGHT_DRIVER_GPIO_PORT == GPIOF)
    {
        __HAL_RCC_GPIOF_CLK_ENABLE();
    }
    else if (BACKLIGHT_DRIVER_GPIO_PORT == GPIOG)
    {
        __HAL_RCC_GPIOG_CLK_ENABLE();
    }
    else if (BACKLIGHT_DRIVER_GPIO_PORT == GPIOH)
    {
        __HAL_RCC_GPIOH_CLK_ENABLE();
    }
#if defined(GPIOI)
    else if (BACKLIGHT_DRIVER_GPIO_PORT == GPIOI)
    {
        __HAL_RCC_GPIOI_CLK_ENABLE();
    }
#endif
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: PWM alternate-function 핀 구성                                   */
/*                                                                            */
/*  이 블록이 핵심이다.                                                        */
/*  - Cube가 gpio.c에서 PC9을 일반 GPIO output low로 만들어도                 */
/*  - 여기서 다시 PWM alternate function으로 덮어쓴다.                         */
/*                                                                            */
/*  따라서 "백라이트 핀은 이미 있는데 IOC 재생성 때문에 설정이 날아간다" 는    */
/*  문제를 런타임 재구성으로 회피한다.                                         */
/* -------------------------------------------------------------------------- */
static void Backlight_Driver_ConfigurePwmPin(void)
{
    GPIO_InitTypeDef gpio_init;

    memset(&gpio_init, 0, sizeof(gpio_init));

    Backlight_Driver_EnableGpioClock();

    gpio_init.Pin       = BACKLIGHT_DRIVER_GPIO_PIN;
    gpio_init.Mode      = GPIO_MODE_AF_PP;
    gpio_init.Pull      = GPIO_NOPULL;
    gpio_init.Speed     = GPIO_SPEED_FREQ_LOW;
    gpio_init.Alternate = BACKLIGHT_DRIVER_GPIO_AF;

    HAL_GPIO_Init(BACKLIGHT_DRIVER_GPIO_PORT, &gpio_init);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: PWM 실패/비상 정지 시 핀을 안전한 OFF 상태로 복귀              */
/*                                                                            */
/*  LCD 백라이트는 패널 전체를 뒤에서 비추는 조명이다.                         */
/*  즉, 이 핀이 high에 고정되면 화면 전체가 계속 밝게 켜진다.                 */
/*  오류 시에는 사용자가 즉시 인지할 수 있도록,                                */
/*  회로가 active-high / active-low 어느 쪽이든 OFF가 되도록 복귀시킨다.      */
/* -------------------------------------------------------------------------- */
static void Backlight_Driver_ConfigureAsGpioSafeOff(void)
{
    GPIO_InitTypeDef gpio_init;

    memset(&gpio_init, 0, sizeof(gpio_init));

    Backlight_Driver_EnableGpioClock();

    gpio_init.Pin   = BACKLIGHT_DRIVER_GPIO_PIN;
    gpio_init.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull  = GPIO_NOPULL;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;

    HAL_GPIO_Init(BACKLIGHT_DRIVER_GPIO_PORT, &gpio_init);

#if (BACKLIGHT_DRIVER_OUTPUT_ACTIVE_HIGH != 0u)
    HAL_GPIO_WritePin(BACKLIGHT_DRIVER_GPIO_PORT,
                      BACKLIGHT_DRIVER_GPIO_PIN,
                      GPIO_PIN_RESET);
#else
    HAL_GPIO_WritePin(BACKLIGHT_DRIVER_GPIO_PORT,
                      BACKLIGHT_DRIVER_GPIO_PIN,
                      GPIO_PIN_SET);
#endif
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 0~1000 permille clamp                                            */
/* -------------------------------------------------------------------------- */
static uint16_t Backlight_Driver_ClampPermille(uint32_t value)
{
    if (value > 1000u)
    {
        return 1000u;
    }

    return (uint16_t)value;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: duty 상하한 적용                                                */
/*                                                                            */
/*  규칙                                                                      */
/*  - 0 요청은 완전 소등이므로 그대로 0 유지                                    */
/*  - 0이 아닌 값은 최소 duty 아래로 너무 낮아지면 MIN으로 올림                */
/*  - 상한은 MAX_DUTY_PERMILLE로 제한                                           */
/* -------------------------------------------------------------------------- */
static uint16_t Backlight_Driver_ApplyDutyLimits(uint16_t pwm_permille)
{
    uint16_t limited;

    limited = pwm_permille;

    if (limited == 0u)
    {
        return 0u;
    }

    if (limited < BACKLIGHT_DRIVER_MIN_DUTY_PERMILLE)
    {
        limited = BACKLIGHT_DRIVER_MIN_DUTY_PERMILLE;
    }

    if (limited > BACKLIGHT_DRIVER_MAX_DUTY_PERMILLE)
    {
        limited = BACKLIGHT_DRIVER_MAX_DUTY_PERMILLE;
    }

    return limited;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 1% LUT 포인트 사이 보간                                          */
/*                                                                            */
/*  linear_permille는 0~1000이므로                                             */
/*  index = value / 10, frac = value % 10 으로 분해한 뒤                        */
/*  인접 두 LUT 값을 선형 보간한다.                                            */
/* -------------------------------------------------------------------------- */
static uint16_t Backlight_Driver_LerpGammaLut(uint16_t linear_permille)
{
    uint16_t index;
    uint16_t frac;
    uint16_t y0;
    uint16_t y1;
    uint32_t interpolated;

    if (linear_permille >= 1000u)
    {
        return 1000u;
    }

    index = (uint16_t)(linear_permille / 10u);
    frac  = (uint16_t)(linear_permille % 10u);

    y0 = s_backlight_gamma_lut_permille[index];
    y1 = s_backlight_gamma_lut_permille[index + 1u];

    interpolated = ((uint32_t)y0 * (uint32_t)(10u - frac)) +
                   ((uint32_t)y1 * (uint32_t)frac);

    interpolated = (interpolated + 5u) / 10u;
    return (uint16_t)interpolated;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 현재 ARR 읽기                                                   */
/*                                                                            */
/*  CubeMX가 TIM period를 바꿔도,                                               */
/*  이 드라이버는 runtime ARR를 읽어서 compare를 계산한다.                     */
/*  따라서 PWM 해상도 변경에도 자동 적응한다.                                  */
/* -------------------------------------------------------------------------- */
static uint32_t Backlight_Driver_ReadArr(void)
{
    return __HAL_TIM_GET_AUTORELOAD(&BACKLIGHT_DRIVER_TIM_HANDLE);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: TIM channel 구성                                                */
/*                                                                            */
/*  현재 보드에서는 TIM8 CH3N(PB1)을 사용하므로                                 */
/*  CH3 자체를 PWM으로 구성하고, 실제 start는 N출력으로 건다.                  */
/* -------------------------------------------------------------------------- */
static HAL_StatusTypeDef Backlight_Driver_ConfigureTimerChannel(void)
{
    TIM_OC_InitTypeDef config_oc;

    memset(&config_oc, 0, sizeof(config_oc));

    config_oc.OCMode       = TIM_OCMODE_PWM1;
    config_oc.Pulse        = 0u;
#if (BACKLIGHT_DRIVER_OUTPUT_ACTIVE_HIGH != 0u)
    config_oc.OCPolarity   = TIM_OCPOLARITY_HIGH;
    config_oc.OCNPolarity  = TIM_OCNPOLARITY_HIGH;
#else
    config_oc.OCPolarity   = TIM_OCPOLARITY_LOW;
    config_oc.OCNPolarity  = TIM_OCNPOLARITY_LOW;
#endif
    config_oc.OCFastMode   = TIM_OCFAST_DISABLE;
    config_oc.OCIdleState  = TIM_OCIDLESTATE_RESET;
    config_oc.OCNIdleState = TIM_OCNIDLESTATE_RESET;

    return HAL_TIM_PWM_ConfigChannel(&BACKLIGHT_DRIVER_TIM_HANDLE,
                                     &config_oc,
                                     BACKLIGHT_DRIVER_TIM_CHANNEL);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: PWM/NPWM start                                                  */
/*                                                                            */
/*  BACKLIGHT_DRIVER_USE_COMPLEMENTARY_OUTPUT == 1 인 경우                      */
/*  - HAL_TIMEx_PWMN_Start()를 사용한다.                                        */
/*                                                                            */
/*  == 0 인 경우                                                               */
/*  - 일반 HAL_TIM_PWM_Start()를 사용한다.                                      */
/* -------------------------------------------------------------------------- */
static HAL_StatusTypeDef Backlight_Driver_StartOutput(void)
{
#if (BACKLIGHT_DRIVER_USE_COMPLEMENTARY_OUTPUT != 0u)
    return HAL_TIMEx_PWMN_Start(&BACKLIGHT_DRIVER_TIM_HANDLE,
                                BACKLIGHT_DRIVER_TIM_CHANNEL);
#else
    return HAL_TIM_PWM_Start(&BACKLIGHT_DRIVER_TIM_HANDLE,
                             BACKLIGHT_DRIVER_TIM_CHANNEL);
#endif
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: PWM/NPWM stop                                                   */
/* -------------------------------------------------------------------------- */
static void Backlight_Driver_StopOutput(void)
{
#if (BACKLIGHT_DRIVER_USE_COMPLEMENTARY_OUTPUT != 0u)
    (void)HAL_TIMEx_PWMN_Stop(&BACKLIGHT_DRIVER_TIM_HANDLE,
                              BACKLIGHT_DRIVER_TIM_CHANNEL);
#else
    (void)HAL_TIM_PWM_Stop(&BACKLIGHT_DRIVER_TIM_HANDLE,
                           BACKLIGHT_DRIVER_TIM_CHANNEL);
#endif
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: compare 레지스터 직접 갱신                                      */
/*                                                                            */
/*  퍼밀레 duty -> CCR counts 변환                                             */
/*  - 0      -> 0                                                              */
/*  - 1000   -> ARR에 최대한 가깝게                                             */
/*                                                                            */
/*  주의                                                                      */
/*  - PWM 모드에서 100%를 완전한 DC high로 만들려면 구현에 따라 ARR+1이         */
/*    필요할 수 있으나, HAL CCR는 ARR 범위를 쓰므로 ARR clamp를 사용한다.      */
/* -------------------------------------------------------------------------- */
static void Backlight_Driver_WriteCompareFromPermille(uint16_t pwm_permille)
{
    uint32_t arr;
    uint32_t compare_counts;
    uint64_t scaled_counts;

    arr = Backlight_Driver_ReadArr();
    g_backlight_driver_state.timer_arr = arr;

    if (pwm_permille == 0u)
    {
        compare_counts = 0u;
    }
    else
    {
        scaled_counts = ((uint64_t)(arr + 1u) * (uint64_t)pwm_permille);
        scaled_counts = scaled_counts / 1000u;

        if (scaled_counts == 0u)
        {
            scaled_counts = 1u;
        }

        if (scaled_counts > (uint64_t)arr)
        {
            scaled_counts = (uint64_t)arr;
        }

        compare_counts = (uint32_t)scaled_counts;
    }

    __HAL_TIM_SET_COMPARE(&BACKLIGHT_DRIVER_TIM_HANDLE,
                          BACKLIGHT_DRIVER_TIM_CHANNEL,
                          compare_counts);

    g_backlight_driver_state.last_compare_counts = compare_counts;
    g_backlight_driver_state.applied_pwm_permille = pwm_permille;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 감마 매핑 계산                                                   */
/* -------------------------------------------------------------------------- */
uint16_t Backlight_Driver_MapLinearToGammaPermille(uint16_t linear_permille)
{
    uint16_t clamped;
    uint16_t gamma_pwm;

    clamped = Backlight_Driver_ClampPermille(linear_permille);
    gamma_pwm = Backlight_Driver_LerpGammaLut(clamped);
    gamma_pwm = Backlight_Driver_ApplyDutyLimits(gamma_pwm);

    return gamma_pwm;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: ready 여부                                                       */
/* -------------------------------------------------------------------------- */
bool Backlight_Driver_IsReady(void)
{
    if ((g_backlight_driver_state.initialized == true) &&
        (g_backlight_driver_state.pwm_running == true))
    {
        return true;
    }

    return false;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: init                                                             */
/*                                                                            */
/*  실행 순서                                                                  */
/*  1) 상태 저장소 초기화                                                       */
/*  2) PWM 핀 alternate function 구성                                          */
/*  3) TIM 채널 재구성                                                         */
/*  4) 실제 PWM start                                                          */
/*  5) 초깃값 0 duty 기록                                                      */
/*                                                                            */
/*  실패 시                                                                    */
/*  - 핀을 회로 기준 OFF GPIO 상태로 되돌리고                                  */
/*  - initialized / pwm_running을 false로 남긴다.                              */
/* -------------------------------------------------------------------------- */
void Backlight_Driver_Init(void)
{
    HAL_StatusTypeDef status;

    memset((void *)&g_backlight_driver_state, 0, sizeof(g_backlight_driver_state));
    g_backlight_driver_state.last_hal_status = (uint32_t)HAL_OK;

    /* ---------------------------------------------------------------------- */
    /*  먼저 기존 GPIO OFF 상태를 PWM AF로 전환한다.                          */
    /* ---------------------------------------------------------------------- */
    Backlight_Driver_ConfigurePwmPin();

    /* ---------------------------------------------------------------------- */
    /*  TIM channel을 runtime에서 재구성한다.                                  */
    /*  CubeMX가 TIM8 CH1만 생성해도,                                          */
    /*  여기서 CH3를 따로 살릴 수 있다.                                        */
    /* ---------------------------------------------------------------------- */
    status = Backlight_Driver_ConfigureTimerChannel();
    g_backlight_driver_state.last_hal_status = (uint32_t)status;
    if (status != HAL_OK)
    {
        Backlight_Driver_ConfigureAsGpioSafeOff();
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  실제 PWM 출력을 시작한다.                                              */
    /* ---------------------------------------------------------------------- */
    status = Backlight_Driver_StartOutput();
    g_backlight_driver_state.last_hal_status = (uint32_t)status;
    if (status != HAL_OK)
    {
        Backlight_Driver_ConfigureAsGpioSafeOff();
        return;
    }

    g_backlight_driver_state.initialized = true;
    g_backlight_driver_state.pwm_running = true;

    /* ---------------------------------------------------------------------- */
    /*  부팅 직후에는 일단 0 duty로 시작한다.                                   */
    /*  상위 App가 startup brightness를 곧바로 써 줄 것이다.                   */
    /* ---------------------------------------------------------------------- */
    Backlight_Driver_SetRawPwmPermille(0u);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 비상 해제                                                        */
/* -------------------------------------------------------------------------- */
void Backlight_Driver_DeInitToGpioLow(void)
{
    Backlight_Driver_StopOutput();
    Backlight_Driver_ConfigureAsGpioSafeOff();

    g_backlight_driver_state.initialized            = false;
    g_backlight_driver_state.pwm_running            = false;
    g_backlight_driver_state.requested_linear_permille = 0u;
    g_backlight_driver_state.applied_pwm_permille   = 0u;
    g_backlight_driver_state.last_compare_counts    = 0u;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: raw PWM duty 직접 적용                                           */
/*                                                                            */
/*  용도                                                                      */
/*  - 드라이버 단위 bring-up                                                   */
/*  - 생산/검사 단계에서 특정 duty 강제                                         */
/*  - 감마 보정 없이 하드웨어가 실제로 살아 있는지 확인                         */
/* -------------------------------------------------------------------------- */
void Backlight_Driver_SetRawPwmPermille(uint16_t pwm_permille)
{
    uint16_t limited_pwm;

    if (Backlight_Driver_IsReady() == false)
    {
        return;
    }

    limited_pwm = Backlight_Driver_ClampPermille(pwm_permille);
    limited_pwm = Backlight_Driver_ApplyDutyLimits(limited_pwm);

    Backlight_Driver_WriteCompareFromPermille(limited_pwm);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 사람이 느끼는 선형 밝기 적용                                     */
/*                                                                            */
/*  단계                                                                      */
/*  1) 입력 clamp                                                             */
/*  2) 감마 LUT 보정                                                           */
/*  3) duty 하한/상한 적용                                                     */
/*  4) CCR 갱신                                                                */
/* -------------------------------------------------------------------------- */
void Backlight_Driver_SetLinearPermille(uint16_t linear_permille)
{
    uint16_t clamped_linear;
    uint16_t pwm_permille;

    if (Backlight_Driver_IsReady() == false)
    {
        return;
    }

    clamped_linear = Backlight_Driver_ClampPermille(linear_permille);
    pwm_permille   = Backlight_Driver_MapLinearToGammaPermille(clamped_linear);

    g_backlight_driver_state.requested_linear_permille = clamped_linear;
    Backlight_Driver_WriteCompareFromPermille(pwm_permille);
}
