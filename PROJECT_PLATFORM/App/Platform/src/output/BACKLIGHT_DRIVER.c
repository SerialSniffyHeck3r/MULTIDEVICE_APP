#include "BACKLIGHT_DRIVER.h"
#include "APP_MEMORY_SECTIONS.h"

#include <math.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*  GCC/ARMCC 怨듯넻 LUT ??μ냼 ?뺣젹                                            */
/*                                                                            */
/*  媛먮쭏 LUT??16-bit ?뷀듃由щ? 留롮씠 ?닿퀬 ?덉쑝誘濡?                             */
/*  ?쇰? ?댁껜?몄뿉??32-bit 寃쎄퀎 ?뺣젹??媛뺤젣???먮㈃ ?묎렐 ?⑥쑉怨?                */
/*  ?뺣젹 ?덉젙?깆씠 醫뗭븘吏꾨떎.                                                   */
/* -------------------------------------------------------------------------- */
#if defined(__GNUC__)
#define BACKLIGHT_DRIVER_ALIGNED __attribute__((aligned(4)))
#else
#define BACKLIGHT_DRIVER_ALIGNED
#endif

/* -------------------------------------------------------------------------- */
/*  諛깅씪?댄듃 PWM 紐⑺몴 二쇳뙆??                                                  */
/*                                                                            */
/*  湲닿툒 ?⑥튂 諛곌꼍                                                            */
/*  - ?꾩옱 IOC媛 留뚮뱺 TIM3 湲곕낯 Period=65535 ?ㅼ젙?                            */
/*    ? kHz ???PWM??留뚮뱺??                                               */
/*  - PB1/TIM3_CH4 諛깅씪?댄듃 PWM????二쇳뙆?섎줈 ?ㅼ쐞移?릺硫?                     */
/*    ?꾩썝/洹몃씪?대뱶/?꾨궇濡쒓렇 寃쎈줈濡??꾩꽕???깅텇??                             */
/*    ?ㅽ뵾而?異쒕젰???욎뿬 媛泥?鍮꾪봽?뚯쑝濡??ㅻ┫ ???덈떎.                        */
/*  - ?뱁엳 "諛앷린??鍮꾨?"?섍퀬 "?꾩쟾 ?뚮벑?대㈃ ?щ씪吏?? 利앹긽?                 */
/*    PWM duty 湲곕컲 ?꾩꽕 ?몄씠利덉쓽 ?꾪삎?곸씤 ?⑦꽩?대떎.                          */
/*                                                                            */
/*  ????꾨왂                                                                  */
/*  - generated tim.c??CubeMX ?ъ깮??????뼱?⑥?誘濡?吏곸젒 ?섏젙?섏? ?딅뒗??   */
/*  - ?????non-generated driver?먯꽌 TIM3??ARR瑜??고??꾩뿉                  */
/*    媛泥????諛뽰쑝濡??ъ꽕?뺥븳??                                            */
/*  - TIM3_CH1~CH3瑜?媛숈씠 ?곕뒗 LED 梨꾨꼸 duty???숈씪 鍮꾩쑉濡??ъ뒪耳?쇳빐??      */
/*    湲곗〈 LED 諛앷린 泥닿컧????댁?吏 ?딄쾶 留뚮뱺??                              */
/*                                                                            */
/*  20kHz ?좏깮 ?댁쑀                                                            */
/*  - ?遺遺꾩쓽 ?ъ슜?먯뿉寃?媛泥??곹븳 諛뽰씠??                                   */
/*  - 84MHz TIM ?낅젰 湲곗? ARR媛 4199媛 ?섏뼱 duty 遺꾪빐?λ룄 異⑸텇?섎떎.           */
/*  - ?ㅼ쐞移??먯떎??怨쇰룄?섍쾶 ?섎━吏 ?딆쑝硫댁꽌 ?묎툒 李⑤떒?⑹쑝濡??곸젅?섎떎.        */
/* -------------------------------------------------------------------------- */
#ifndef BACKLIGHT_DRIVER_TARGET_PWM_HZ
#define BACKLIGHT_DRIVER_TARGET_PWM_HZ 20000u
#endif

/* -------------------------------------------------------------------------- */
/*  怨듦컻 ?곹깭 ??μ냼                                                           */
/*                                                                            */
/*  ?곸쐞 App / ?붾쾭洹??붾㈃ / ?꾩옣 濡쒓렇?먯꽌                                    */
/*  留덉?留??붿껌 諛앷린, ?ㅼ젣 ?곸슜 duty, ?꾩옱 ARR snapshot ?깆쓣                  */
/*  ?뺤씤?????덈룄濡??꾩뿭 ?곹깭瑜??좎??쒕떎.                                    */
/* -------------------------------------------------------------------------- */
volatile backlight_driver_state_t g_backlight_driver_state;

/* -------------------------------------------------------------------------- */
/*  媛먮쭏 LUT                                                                   */
/*                                                                            */
/*  index 0..4096  ->  ?낅젰 linear 諛앷린 0..1                                  */
/*  value 0..65535 ->  異쒕젰 electrical duty 0..1                              */
/*                                                                            */
/*  LUT??init ??1?뚮쭔 powf()濡??앹꽦?섍퀬,                                    */
/*  ?ㅼ떆媛?寃쎈줈?먯꽌??蹂닿컙留??섑뻾?쒕떎.                                        */
/* -------------------------------------------------------------------------- */
static BACKLIGHT_DRIVER_ALIGNED uint16_t
    s_backlight_gamma_lut_q16[BACKLIGHT_DRIVER_GAMMA_LUT_RESOLUTION + 1u] APP_CCMRAM_BSS;
static uint8_t s_backlight_gamma_lut_ready = 0u;

/* -------------------------------------------------------------------------- */
/*  ?대? ?좏떥: 0..1000 permille clamp                                           */
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
/*  ?대? ?좏떥: Q16 clamp                                                       */
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
/*  ?대? ?좏떥: ?좏깮??PWM timer ?낅젰 ?대줉 ?쎄린                                 */
/*                                                                            */
/*  ?꾩옱 repo 湲곗?                                                             */
/*  - BACKLIGHT_DRIVER_TIM_HANDLE ??htim3 ?닿퀬,                              */
/*    TIM3??APB1 ??대㉧??                                                   */
/*                                                                            */
/*  ?섏?留???helper???쎄컙 ???쇰컲?뷀빐 ?붾떎.                                */
/*  - ?섏쨷??IOC?먯꽌 諛깅씪?댄듃 timer瑜?TIM1/TIM8/TIM9 媛숈? APB2 ??대㉧濡?      */
/*    ??린?붾씪?? ???⑥닔媛 ?꾩옱 handle??Instance瑜?蹂닿퀬                      */
/*    ?대뒓 踰꾩뒪 ?대줉???⑥빞 ?섎뒗吏 怨꾩궛?쒕떎.                                  */
/*                                                                            */
/*  STM32F4 timer clock 洹쒖튃                                                   */
/*  - APB prescaler媛 1?대㈃ timer input = PCLK                                */
/*  - APB prescaler媛 1???꾨땲硫?timer input = 2 * PCLK                       */
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
/*  ?대? ?좏떥: ?꾩옱 PSC ?쎄린                                                   */
/*                                                                            */
/*  HAL handle??Init 媛믪? 珥덇린 ?앹꽦媛믪씠怨?                                   */
/*  ?고???以묒뿉???ㅼ젣 ?덉??ㅽ꽣 媛믪씠 吏꾩떎?대떎.                                */
/*  ?곕씪??PSC??instance ?덉??ㅽ꽣瑜?吏곸젒 ?쎈뒗??                             */
/* -------------------------------------------------------------------------- */
static uint32_t Backlight_Driver_ReadPrescaler(void)
{
    return (uint32_t)BACKLIGHT_DRIVER_TIM_HANDLE.Instance->PSC;
}

/* -------------------------------------------------------------------------- */
/*  ?대? ?좏떥: ?꾩옱 ARR ?쎄린                                                   */
/* -------------------------------------------------------------------------- */
static uint32_t Backlight_Driver_ReadArr(void)
{
    return __HAL_TIM_GET_AUTORELOAD(&BACKLIGHT_DRIVER_TIM_HANDLE);
}

/* -------------------------------------------------------------------------- */
/*  ?대? ?좏떥: ARR 蹂寃?????compare ?ъ뒪耳??                                */
/*                                                                            */
/*  紐⑹쟻                                                                      */
/*  - TIM3_CH1~CH4??紐⑤몢 媛숈? ARR瑜?怨듭쑀?쒕떎.                                */
/*  - 洹몃옒??ARR瑜?65535 -> 4199 媛숈? ?앹쑝濡?以꾩씠硫?                          */
/*    湲곗〈 CCR 媛믪? 洹몃?濡??????녿떎.                                        */
/*  - 洹몃?濡??먮㈃ CH1~CH3 LED duty? CH4 諛깅씪?댄듃 duty媛                      */
/*    ?꾨? ?ㅻⅨ 諛앷린濡????踰꾨┛??                                           */
/*                                                                            */
/*  ???⑥닔??"湲곗〈 duty 鍮꾩쑉"???좎??섎룄濡?                                  */
/*      new_compare = old_compare / old_arr * new_arr                         */
/*  瑜?諛섏삱由??ы븿??怨꾩궛?쒕떎.                                                 */
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
/*  ?대? ?좏떥: TIM3 PWM base frequency瑜?紐⑺몴 二쇳뙆?섎줈 ?ъ꽕??                 */
/*                                                                            */
/*  ??BACKLIGHT_DRIVER?먯꽌 ?섎뒗媛                                             */
/*  - generated tim.c??TIM3.Init.Period??USER CODE 釉붾줉 諛붽묑?대씪            */
/*    CubeMX ?ъ깮?깆뿉 ?섑빐 ?몄젣????뼱?⑥쭊??                                 */
/*  - 諛섎㈃ ???뚯씪? non-generated ?대?濡?                                     */
/*    ?고????ъ꽕?뺤쓣 ?ш린???섑뻾?섎㈃ Cube ?ъ깮???댁꽦???앷릿??              */
/*                                                                            */
/*  ??TIM3 ?꾩껜瑜?媛숈씠 留뚯??붽?                                               */
/*  - PB1 諛깅씪?댄듃??TIM3_CH4??                                               */
/*  - LED2/LED3/LED4??TIM3_CH1/CH2/CH3瑜??ъ슜?쒕떎.                           */
/*  - 利?TIM3??ARR??梨꾨꼸蹂꾨줈 ?곕줈 媛吏????녾퀬,                              */
/*    諛섎뱶??timer base ?꾩껜瑜?媛숈씠 諛붽퓭???쒕떎.                              */
/*                                                                            */
/*  ?덉쟾 泥섎━                                                                  */
/*  - ?꾩옱 timer媛 ?대? LED 履쎌뿉??enable ?섏뿀?????덉쑝誘濡?                  */
/*    CEN留??좉퉸 ?대━怨? CCR1~CCR4瑜?紐⑤몢 ??ARR 湲곗??쇰줈 ?ш퀎?고븳 ??         */
/*    update event瑜?諛쒖깮?쒗궎怨??ㅼ떆 enable ?쒕떎.                             */
/*  - ???묒뾽? ?쒖뒪??珥덇린????1???섑뻾?섎뒗 ?묎툒 ?⑥튂 寃쎈줈??                */
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

    /* 媛뺤젣濡?UPDATE ?대깽??UG)瑜?諛쒖깮?쒖폒,
     * ??ARR/CCR 媛믪씠 ??대㉧ ?꾨━濡쒕뱶 ?덉??ㅽ꽣?먯꽌 ?ㅼ젣 ?숈옉 ?덉??ㅽ꽣濡?利됱떆 諛섏쁺?섍쾶 ?쒕떎.
     * ?쇰? HAL 踰꾩쟾?먮뒗 __HAL_TIM_GENERATE_EVENT() 留ㅽ겕濡쒓? ?놁쑝誘濡?
     * HAL ?섏〈?깆쓣 以꾩씠湲??꾪빐 EGR ?덉??ㅽ꽣??吏곸젒 UG 鍮꾪듃瑜?湲곕줉?쒕떎.
     */
    BACKLIGHT_DRIVER_TIM_HANDLE.Instance->EGR = TIM_EGR_UG;

    if (timer_was_enabled != 0u)
    {
        __HAL_TIM_ENABLE(&BACKLIGHT_DRIVER_TIM_HANDLE);
    }

    g_backlight_driver_state.timer_arr = new_arr;
}

/* -------------------------------------------------------------------------- */
/*  ?대? ?좏떥: LUT ?앹꽦                                                        */
/*                                                                            */
/*  ?щ엺 ??湲곗????좏삎 諛앷린瑜?electrical duty濡?諛붽씀??媛먮쭏 而ㅻ툕瑜?          */
/*  4096異뺤쑝濡?誘몃━ 援ъ썙 ?붾떎.                                                */
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
/*  ?대? ?좏떥: linear Q16 -> electrical Q16                                    */
/*                                                                            */
/*  4096異?LUT ?몄젒 ???ъ씤?몃? ?좏삎 蹂닿컙?댁꽌 怨꾨떒媛먯쓣 以꾩씤??                */
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
/*  ?대? ?좏떥: Q16 -> permille                                                  */
/* -------------------------------------------------------------------------- */
static uint16_t Backlight_Driver_Q16ToPermille(uint16_t value_q16)
{
    return (uint16_t)((((uint32_t)value_q16) * 1000u +
                       (BACKLIGHT_DRIVER_Q16_MAX / 2u)) /
                      BACKLIGHT_DRIVER_Q16_MAX);
}

/* -------------------------------------------------------------------------- */
/*  ?대? ?좏떥: permille -> Q16                                                  */
/* -------------------------------------------------------------------------- */
static uint16_t Backlight_Driver_PermilleToQ16(uint16_t value_permille)
{
    uint32_t clamped_permille;

    clamped_permille = Backlight_Driver_ClampPermille(value_permille);
    return (uint16_t)((clamped_permille * BACKLIGHT_DRIVER_Q16_MAX + 500u) /
                      1000u);
}

/* -------------------------------------------------------------------------- */
/*  ?대? ?좏떥: electrical Q16 -> CCR counts                                    */
/*                                                                            */
/*  - 0%   -> CCR 0 ?먮뒗 active-low 諛섏쟾媛?                                    */
/*  - 100% -> CCR ARR ?먮뒗 active-low 諛섏쟾媛?                                  */
/*                                                                            */
/*  ???⑥닔????긽 "?꾩옱 ARR"瑜??쎄퀬 洹?湲곗??쇰줈 compare瑜?留뚮뱺??            */
/*  ?곕씪??init ??TIM3 ARR瑜?20kHz??媛믪쑝濡?諛붽씔 ?ㅼ뿉??                     */
/*  諛앷린 怨꾩궛? ?먮룞?쇰줈 ??遺꾪빐?μ쓣 ?곕씪媛꾨떎.                                */
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
/*  ?대? ?좏떥: CCR 湲곕줉                                                        */
/*                                                                            */
/*  ?ㅼ젣 PB1/TIM3_CH4 compare ?덉??ㅽ꽣??媛믪쓣 湲곕줉?섍퀬,                        */
/*  ?붾쾭洹??곹깭 援ъ“泥댁뿉??留덉?留??곸슜 寃곌낵瑜??④릿??                          */
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
/*  怨듦컻 API: ready ?щ?                                                       */
/* -------------------------------------------------------------------------- */
bool Backlight_Driver_IsReady(void)
{
    return ((g_backlight_driver_state.initialized == true) &&
            (g_backlight_driver_state.pwm_running == true));
}

/* -------------------------------------------------------------------------- */
/*  怨듦컻 API: init                                                             */
/*                                                                            */
/*  以묒슂                                                                      */
/*  - CubeMX媛 ?대? TIM3 CH4? PB1 alternate function???앹꽦?덈떎怨?媛?뺥븳??  */
/*  - ?곕씪???ш린?쒕뒗 pin mux瑜??ㅼ떆 嫄대뱶由ъ? ?딅뒗??                         */
/*  - ???Cube媛 留뚮뱺 TIM3 base瑜?runtime?먯꽌 20kHz湲됱쑝濡??ъ꽕?뺥븳 ??       */
/*    CH4 PWM start? 珥덇린 duty ?곸슜留??섑뻾?쒕떎.                              */
/*                                                                            */
/*  湲닿툒 ?⑥튂 ?듭떖 ?쒖꽌                                                        */
/*  1) ?곹깭 援ъ“泥?珥덇린??                                                     */
/*  2) 媛먮쭏 LUT 以鍮?                                                          */
/*  3) TIM3 shared base瑜?媛泥?諛뽰쑝濡?retune                                   */
/*  4) CH4 compare 0?쇰줈 珥덇린??                                               */
/*  5) CH4 PWM start                                                           */
/*  6) 理쒖쥌 諛깅씪?댄듃瑜??꾩쟾 ?뚮벑 ?곹깭濡?留욎땄                                   */
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
/*  怨듦컻 API: deinit                                                           */
/*                                                                            */
/*  ?대쫫? 湲곗〈 ?명솚???꾪빐 ?좎??섏?留?                                       */
/*  ???댁긽 PB1??GPIO濡?媛뺤젣 ?ъ꽕?뺥븯吏 ?딅뒗??                              */
/*                                                                            */
/*  ?댁쑀                                                                      */
/*  - PB1 ?뚯쑀沅뚯? Cube TIM3_CH4 PWM??留↔릿??                                */
/*  - GPIO ?ъ꽕?뺤? ?ㅼ떆 Cube/runtime 異⑸룎??留뚮뱾 ???덈떎.                    */
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
/*  怨듦컻 API: raw electrical Q16 吏곸젒 ?곸슜                                    */
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
/*  怨듦컻 API: linear Q16 ?곸슜                                                  */
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
/*  怨듦컻 API: ?명솚??permille wrapper                                           */
/* -------------------------------------------------------------------------- */
void Backlight_Driver_SetLinearPermille(uint16_t linear_permille)
{
    Backlight_Driver_SetLinearQ16(
        Backlight_Driver_PermilleToQ16(linear_permille));
}

/* -------------------------------------------------------------------------- */
/*  怨듦컻 API: ?명솚??raw PWM permille wrapper                                  */
/* -------------------------------------------------------------------------- */
void Backlight_Driver_SetRawPwmPermille(uint16_t pwm_permille)
{
    Backlight_Driver_SetRawPwmQ16(
        Backlight_Driver_PermilleToQ16(pwm_permille));
}

/* -------------------------------------------------------------------------- */
/*  怨듦컻 API: ?명솚??留ㅽ븨 query                                                 */
/* -------------------------------------------------------------------------- */
uint16_t Backlight_Driver_MapLinearToGammaPermille(uint16_t linear_permille)
{
    uint16_t electrical_q16;

    electrical_q16 = Backlight_Driver_MapLinearQ16ToElectricalQ16(
        Backlight_Driver_PermilleToQ16(linear_permille));

    return Backlight_Driver_Q16ToPermille(electrical_q16);
}
