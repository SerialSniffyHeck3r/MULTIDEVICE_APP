#include "LED_Driver.h"
#include "APP_MEMORY_SECTIONS.h"
#include "tim.h"

#include <math.h>
#include <string.h>

/* ========================================================================== */
/*  LED_Driver ?대? 援ы쁽 硫붾え                                                  */
/*                                                                            */
/*  1) CubeMX / IOC ?덉쟾??                                                    */
/*     - ???뚯씪? generated tim.c??PSC / ARR / OC mode瑜???? ?딅뒗??       */
/*     - HAL_TIM_PWM_Start()? CCR(compare) 媛깆떊留??섑뻾?쒕떎.                  */
/*                                                                            */
/*  2) 諛앷린 ?댁꽍                                                               */
/*     - ?곸쐞 怨꾩링? 0~65535??"??湲곗? ?좏삎 諛앷린"瑜??섍릿??                 */
/*     - ??driver??inverse perceptual mapping???곸슜???꾧린??duty濡?諛붽씔??*/
/*                                                                            */
/*  3) frame rate ?쒗븳                                                         */
/*     - ?꾩옱 ?꾨줈?앺듃??怨듯넻 tick? HAL_GetTick() 1ms 湲곕컲?대떎.              */
/*     - 湲곗〈 17ms(??58.8fps)??motion pattern?먮뒗 異⑸텇?덉?留?               */
/*       BR ALL 媛숈? ?꾩껜 諛앷린 ?ㅼ쐲?먯꽌??怨꾨떒媛먯씠 ?덉뿉 ?????덈떎.           */
/*     - 洹몃옒??異붽? ??대㉧??嫄대뱶由ъ? ?딄퀬, render 媛꾧꺽??1ms濡??뚯뼱?대젮        */
/*       諛앷린 time-axis ?묒옄?붾? 以꾩씤??                                      */
/*                                                                            */
/*  4) ?뚯뒪???⑦꽩 泥좏븰                                                         */
/*     - ?앹궛 / ?섎━ / bring-up ??PWM duty/brightness ?쒖뼱媛 ?뺤긽?몄?         */
/*       諛붾줈 ?덉쑝濡??뺤씤?????덈뒗 ?⑦꽩???곗꽑 ?쒓났?쒕떎.                     */
/*     - 怨좎젙 duty, raw PWM sweep, ??湲곗? linear sweep, BR ALL???ы븿?쒕떎.   */
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

#ifndef LED_DRIVER_VISUAL_LUT_RESOLUTION
#define LED_DRIVER_VISUAL_LUT_RESOLUTION    (4096u)
#endif

#ifndef LED_DRIVER_FRAME_INTERVAL_MS
#define LED_DRIVER_FRAME_INTERVAL_MS        (1u)
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
/*  ?쇰━ LED ?쒖꽌 -> ?ㅼ젣 PWM 梨꾨꼸 留ㅽ븨                                         */
/*                                                                            */
/*  ???쒖꽌媛 怨?UI 湲곗? 醫?-> ???쇰━ ?쒖꽌??                                */
/*                                                                            */
/*  留ㅼ슦 以묒슂??? ?뚯쑀沅??뺣━                                                */
/*  - Application2 IOC 湲곗? PB1 / TIM3_CH4 ??LCD backlight ?꾩슜 寃쎈줈??      */
/*  - ?곕씪??LED driver??PB1 / TIM3_CH4 瑜??덈?濡?LED 梨꾨꼸濡??≪쑝硫????쒕떎.*/
/*  - LED5???꾩옱 Application2 IOC 湲곗??쇰줈 PC9 / TIM8_CH4 ?대ŉ,              */
/*    logical 4??諛섎뱶??洹?寃쎈줈瑜??곕씪???쒕떎.                               */
/*  - 留뚯빟 logical 4瑜?TIM3_CH4濡??먮㈃, F2 湲멸쾶 ?꾨쫫?쇰줈 LED test pattern??   */
/*    ?섍만 ??LED 寃쎈줈媛 TIM3 CCR4瑜???뼱??PB1 諛깅씪?댄듃媛 LED ?곹깭瑜?        */
/*    ?곕씪媛??異⑸룎???ㅼ떆 諛쒖깮?쒕떎.                                          */
/*                                                                            */
/*  logical 0  -> LED1  -> PE9  -> TIM1_CH1                                    */
/*  logical 1  -> LED2  -> PA6  -> TIM3_CH1                                    */
/*  logical 2  -> LED3  -> PA7  -> TIM3_CH2                                    */
/*  logical 3  -> LED4  -> PB0  -> TIM3_CH3                                    */
/*  logical 4  -> LED5  -> PC9  -> TIM8_CH4                                    */
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
    { &htim8, TIM_CHANNEL_4, "LED5  / PC9  / TIM8_CH4" },
    { &htim4, TIM_CHANNEL_1, "LED6  / PD12 / TIM4_CH1" },
    { &htim4, TIM_CHANNEL_2, "LED7  / PD13 / TIM4_CH2" },
    { &htim4, TIM_CHANNEL_3, "LED8  / PD14 / TIM4_CH3" },
    { &htim4, TIM_CHANNEL_4, "LED9  / PD15 / TIM4_CH4" },
    { &htim8, TIM_CHANNEL_1, "LED10 / PC6  / TIM8_CH1" },
    { &htim9, TIM_CHANNEL_2, "LED11 / PE6  / TIM9_CH2" }
};

/* -------------------------------------------------------------------------- */
/*  媛?대뜲 媛뺤“???⑦꽩?먯꽌 ?ъ슜?섎뒗 怨듦컙 媛以묒튂                                */
/*                                                                            */
/*  ?붾㈃ ?섎?                                                                  */
/*  - LED6??媛??諛앹? 以묒떖??                                                 */
/*  - LED5/7, LED4/8, ... 濡?媛덉닔濡??꾨쭔?섍쾶 ??븘吏?                          */
/*                                                                            */
/*  UI ?꾩튂 ?ㅻ챸                                                               */
/*  [LED1][LED2][LED3][LED4][LED5][LED6][LED7][LED8][LED9][LED10][LED11]     */
/*                                      ??                                    */
/*                                   以묒떖 媛뺤“                                 */
/* -------------------------------------------------------------------------- */
static const uint16_t s_center_spatial_weight_q16[LED_DRIVER_CHANNEL_COUNT] =
{
     3277u,  6554u, 11796u, 20971u, 36700u,
    65535u,
    36700u, 20971u, 11796u,  6554u,  3277u
};

static LED_DriverState_t s_led_driver;

/* -------------------------------------------------------------------------- */
/*  visual -> electrical LUT                                                  */
/*                                                                            */
/*  render path瑜?1ms濡??뚯뼱?대━硫?perceptual remap ?⑥닔 ?몄텧 ?잛닔???섏뼱?쒕떎. */
/*  洹몃옒??float / cbrtf / powf ?곗궛? init ??LUT ?앹꽦 1?뚮줈 紐곗븘 ?먭퀬,     */
/*  ?ㅼ떆媛??뚮뜑 寃쎈줈?먯꽌??蹂닿컙留??섑뻾?쒕떎.                                    */
/* -------------------------------------------------------------------------- */
static uint16_t s_visual_to_electrical_lut_q16[LED_DRIVER_VISUAL_LUT_RESOLUTION + 1u] APP_CCMRAM_BSS;
static uint8_t  s_visual_to_electrical_lut_ready = 0u;

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
/*  Q16 怨깆뀍 helper                                                             */
/*                                                                            */
/*  a, b媛 紐⑤몢 0~65535 踰붿쐞????                                             */
/*      result = a * b / 65535                                                */
/*  瑜?諛섏삱由??ы븿?댁꽌 怨꾩궛?쒕떎.                                               */
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
/*  ?낅젰 媛??섎?                                                               */
/*  - brightness_q16 = "?щ엺 ?덉뿉 ?좏삎?곸쑝濡?蹂댁씠湲??먰븯??紐⑺몴 諛앷린"         */
/*                                                                            */
/*  異쒕젰 媛??섎?                                                               */
/*  - ?ㅼ젣 LED duty???대떦?섎뒗 ?꾧린??諛앷린                                    */
/*                                                                            */
/*  湲곕낯 諛⑹떇                                                                  */
/*  - inverse CIE L* piecewise function                                       */
/*  - ?꾩＜ ??? 諛앷린 援ш컙? 吏곸꽑, ?섎㉧吏??cubic 愿怨꾨? ?ъ슜?쒕떎.             */
/*                                                                            */
/*  ???                                                                      */
/*  - LED_DRIVER_USE_CIE_LSTAR == 0 ?대㈃                                      */
/*    ?⑥닚 power-law gamma(湲곕낯 2.2) 諛⑹떇?쇰줈 fallback 媛??                  */
/* -------------------------------------------------------------------------- */
static void LED_Driver_BuildVisualToElectricalLutIfNeeded(void)
{
    uint32_t index;

    if (s_visual_to_electrical_lut_ready != 0u)
    {
        return;
    }

    for (index = 0u; index <= LED_DRIVER_VISUAL_LUT_RESOLUTION; ++index)
    {
        float visual_0_to_1;
        float electrical_0_to_1;

        visual_0_to_1 = (float)index / (float)LED_DRIVER_VISUAL_LUT_RESOLUTION;

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

        s_visual_to_electrical_lut_q16[index] =
            (uint16_t)(electrical_0_to_1 * 65535.0f + 0.5f);
    }

    s_visual_to_electrical_lut_ready = 1u;
}

static uint16_t LED_Driver_VisualQ16ToElectricalQ16(uint16_t brightness_q16)
{
    uint32_t scaled_index_q16;
    uint32_t index;
    uint32_t frac_q16;
    uint32_t y0;
    uint32_t y1;

    LED_Driver_BuildVisualToElectricalLutIfNeeded();

    if (brightness_q16 >= LED_DRIVER_Q16_MAX)
    {
        return (uint16_t)LED_DRIVER_Q16_MAX;
    }

    scaled_index_q16 = ((uint32_t)brightness_q16 * LED_DRIVER_VISUAL_LUT_RESOLUTION);
    index = scaled_index_q16 / LED_DRIVER_Q16_MAX;
    frac_q16 = scaled_index_q16 % LED_DRIVER_Q16_MAX;

    if (index >= LED_DRIVER_VISUAL_LUT_RESOLUTION)
    {
        index = LED_DRIVER_VISUAL_LUT_RESOLUTION - 1u;
        frac_q16 = LED_DRIVER_Q16_MAX;
    }

    y0 = s_visual_to_electrical_lut_q16[index];
    y1 = s_visual_to_electrical_lut_q16[index + 1u];

    return (uint16_t)(y0 + (((uint64_t)(y1 - y0) * (uint64_t)frac_q16 +
                             (LED_DRIVER_Q16_MAX / 2u)) /
                            LED_DRIVER_Q16_MAX));
}

/* -------------------------------------------------------------------------- */
/*  raw electrical duty -> visual Q16                                          */
/*                                                                            */
/*  PWM duty n% 怨좎젙 ?뚯뒪?몃뒗 perceptual remap ?댄썑???ㅼ젣 ?꾧린??duty媛       */
/*  n%媛 ?섎룄濡?留욎떠???쒕떎.                                                   */
/*  ?곕씪???뚯뒪???⑦꽩 ?꾩슜?쇰줈??inverse mapping??諛섎? 諛⑺뼢 helper瑜??대떎.  */
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
/*  ?꾩옱 logical channel??ARR瑜??쎌뼱 compare 媛믪쑝濡?蹂??                      */
/*                                                                            */
/*  ??蹂?섏? channel蹂?ARR媛 媛숈? ?딆븘???숈옉?쒕떎.                            */
/*  利? duty????긽 ?꾩옱 timer???ㅼ젣 ARR 湲곗??쇰줈 ?ㅼ??쇰맂??                */
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
/*  以묒슂????                                                                 */
/*  - timer base ?ъ꽕???놁씠 channel start留??섑뻾?쒕떎.                         */
/*  - TIM9??LED11???대떦?섎뒗 CH2留??ъ슜?쒕떎.                                  */
/*  - 珥덇린 compare??OFF ?곹깭??留욎떠 0 ?먮뒗 ARR濡??뺣젹?쒕떎.                   */
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
/*  遺?쒕윭???꾪솚                                                               */
/*                                                                            */
/*  target frame??媛묒옄湲?諛붾뚮뜑?쇰룄 current frame??議곌툑???곕씪媛寃?留뚮뱺??  */
/*                                                                            */
/*  transition_ms = 0                                                          */
/*  - 利됱떆 target怨??숈씪?섍쾶 留욎땄                                              */
/*                                                                            */
/*  transition_ms > 0                                                          */
/*  - ?ㅼ젣 elapsed_ms留뚰겮 diff???쇰?留??대룞                                   */
/*  - step??0???섎뒗 誘몄꽭 diff 援ш컙? 理쒖냼 1 LSB ?대룞 蹂댁옣                   */
/* -------------------------------------------------------------------------- */
static void LED_Driver_AdvanceCurrentFrameTowardTarget(uint16_t transition_ms,
                                                     uint32_t elapsed_ms)
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

        step_value = (int32_t)(((int64_t)diff_value * (int64_t)elapsed_ms) /
                                  (int64_t)transition_ms);

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
/*  怨듭슜 ?⑦꽩 helper                                                            */
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
/*  ?쇰컲 紐⑤뱶 ?뚮뜑??                                                           */
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
/*  UI 洹몃┝ ?ㅻ챸                                                               */
/*  - LED1 諛붽묑 ?쇱そ?먯꽌 鍮?癒몃━(head)媛 ?ㅼ뼱?⑤떎.                             */
/*  - LED11 諛붽묑 ?ㅻⅨ履쎄퉴吏 吏?섍컙??                                          */
/*  - head ?ㅼ뿉??瑗щ━(tail)媛 ?⑥븘 ?쒖꽦 媛숈? 紐⑥뼇?쇰줈 蹂댁씤??                */
/*                                                                            */
/*  ?꾩튂 ?ㅻ챸                                                                  */
/*  - ?쒖옉??: LED1??諛붽묑 ?쇱そ                                                */
/*  - 吏꾪뻾   : LED1 -> LED11                                                   */
/*  - 醫낅즺   : LED11??諛붽묑 ?ㅻⅨ履?                                            */
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
/*  UI 洹몃┝ ?ㅻ챸                                                               */
/*  - 諛앹? ???섎굹媛 LED1 -> LED11 -> LED1濡??뺣났?쒕떎.                        */
/*  - ?꾪듃??以묒떖??梨꾨꼸 ?ъ씠瑜?吏?섍컝 ????????LED??遺?쒕읇寃?遺꾩궛?쒕떎.   */
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
/*  ?뚯뒪???⑦꽩 ?뚮뜑??                                                         */
/* -------------------------------------------------------------------------- */
static void LED_Driver_BuildPattern_TestWalkLeftToRight(
    uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
    uint32_t elapsed_ms)
{
    (void)elapsed_ms;

    /* legacy slot ?ъ궗?? PWM duty 1% 怨좎젙 */
    LED_Driver_BuildPattern_TestFixedElectricalDuty(frame_q16,
                                                    LED_Driver_PermilleToQ16(10u));
}

static void LED_Driver_BuildPattern_TestWalkRightToLeft(
    uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
    uint32_t elapsed_ms)
{
    (void)elapsed_ms;

    /* legacy slot ?ъ궗?? PWM duty 5% 怨좎젙 */
    LED_Driver_BuildPattern_TestFixedElectricalDuty(frame_q16,
                                                    LED_Driver_PermilleToQ16(50u));
}

static void LED_Driver_BuildPattern_TestCenterOut(uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
                                                  uint32_t elapsed_ms)
{
    (void)elapsed_ms;

    /* legacy slot ?ъ궗?? PWM duty 25% 怨좎젙 */
    LED_Driver_BuildPattern_TestFixedElectricalDuty(frame_q16,
                                                    LED_Driver_PermilleToQ16(250u));
}

static void LED_Driver_BuildPattern_TestEdgeIn(uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
                                               uint32_t elapsed_ms)
{
    (void)elapsed_ms;

    /* legacy slot ?ъ궗?? PWM duty 50% 怨좎젙 */
    LED_Driver_BuildPattern_TestFixedElectricalDuty(frame_q16,
                                                    LED_Driver_PermilleToQ16(500u));
}

static void LED_Driver_BuildPattern_TestOddEven(uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
                                                uint32_t elapsed_ms)
{
    (void)elapsed_ms;

    /* legacy slot ?ъ궗?? PWM duty 75% 怨좎젙 */
    LED_Driver_BuildPattern_TestFixedElectricalDuty(frame_q16,
                                                    LED_Driver_PermilleToQ16(750u));
}

static void LED_Driver_BuildPattern_TestHalfSwap(uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
                                                 uint32_t elapsed_ms)
{
    (void)elapsed_ms;

    /* legacy slot ?ъ궗?? PWM duty 90% 怨좎젙 */
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
    /* legacy slot ?ъ궗?? ?щ엺 ??湲곗? linear brightness sweep */
    LED_Driver_BuildPattern_TestLinearBrightnessSweep(frame_q16, elapsed_ms);
}

static void LED_Driver_BuildPattern_TestStrobeSlow(uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT],
                                                   uint32_t elapsed_ms)
{
    (void)elapsed_ms;

    /* legacy slot ?ъ궗?? PWM duty 99% 怨좎젙 */
    LED_Driver_BuildPattern_TestFixedElectricalDuty(frame_q16,
                                                    LED_Driver_PermilleToQ16(990u));
}

/* -------------------------------------------------------------------------- */
/*  command -> target frame 蹂??                                              */
/*                                                                            */
/*  ??switch媛 ?ㅼ젣 洹몃┝ ?앹꽦??以묒떖?대떎.                                     */
/*  app 怨꾩링? ?ш린 ?ㅼ뼱??pattern怨?parameter留?寃곗젙?쒕떎.                     */
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

    LED_Driver_BuildVisualToElectricalLutIfNeeded();

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
    uint32_t elapsed_ms;

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
    /*  now_ms??HAL_GetTick() 1ms ?⑥쐞?대?濡?                                  */
    /*  1ms 誘몃쭔?대㈃ ?대쾲 loop?먯꽌???뚮뜑留곸쓣 ?앸왂?쒕떎.                         */
    /* ---------------------------------------------------------------------- */
    elapsed_ms = (now_ms - s_led_driver.last_frame_ms);
    if (elapsed_ms < LED_DRIVER_FRAME_INTERVAL_MS)
    {
        return;
    }

    s_led_driver.last_frame_ms = now_ms;
    s_led_driver.last_pattern = command->pattern;
    s_led_driver.last_mode_started_ms = command->mode_started_ms;

    LED_Driver_BuildTargetFrame(s_led_driver.target_frame_q16, now_ms, command);
    LED_Driver_AdvanceCurrentFrameTowardTarget(command->transition_ms, elapsed_ms);
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
