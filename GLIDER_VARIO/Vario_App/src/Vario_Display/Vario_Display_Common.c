#include "Vario_Display_Common.h"
#include "APP_MEMORY_SECTIONS.h"

#include "Vario_Dev.h"
#include "Vario_State.h"
#include "Vario_Settings.h"
#include "Vario_Navigation.h"
#include "Vario_Icon_Resources.h"

#include <math.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/* ?꾩씠肄?fallback                                                              */
/*                                                                            */
/* ?쇰? 濡쒖뺄 ?꾨줈?앺듃??Vario_Icon_Resources.h ????μ냼 理쒖떊蹂몃낫???ㅻ옒?섏뼱      */
/* ALT1 / GS AVG ?꾩씠肄??щ낵???놁쓣 ???덈떎.                                    */
/*                                                                            */
/* ?ъ슜?먭? ??.c ?뚯씪 ?섎굹留?援먯껜?대룄 鍮뚮뱶?섍쾶 ?섎젮怨?                          */
/* "愿??留ㅽ겕濡쒓? ?놁쓣 ?뚮쭔" ?숈씪 鍮꾪듃留듭쓣 ?ш린??fallback ?쇰줈 ?좎뼵?쒕떎.         */
/* ?ㅻ뜑媛 理쒖떊?대㈃ ?꾨옒 釉붾줉? ?꾨? 嫄대꼫?대떎.                                   */
/* -------------------------------------------------------------------------- */
#ifndef VARIO_ICON_ALT1_WIDTH
#define VARIO_ICON_ALT1_WIDTH  9
#define VARIO_ICON_ALT1_HEIGHT 9
static const unsigned char vario_icon_alt1_bits[] = {
    0x10,0xfe,0x38,0xfe,0x6c,0xfe,0xc6,0xfe,0x00,0xfe,0x00,0xfe,
    0xaa,0xfe,0xff,0xff,0xff,0xff
};
#endif

#ifndef VARIO_ICON_GS_AVG_WIDTH
#define VARIO_ICON_GS_AVG_WIDTH  5
#define VARIO_ICON_GS_AVG_HEIGHT 5
static const unsigned char vario_icon_gs_avg_bits[] = {
    0xe7,0xee,0xfc,0xee,0xe7
};
#endif

#ifndef VARIO_ICON_MC_SPEED_WIDTH
#define VARIO_ICON_MC_SPEED_WIDTH  5
#define VARIO_ICON_MC_SPEED_HEIGHT 5
static const unsigned char vario_icon_mc_speed_bits[] = {
    0xe4,0xe8,0xff,0xe8,0xe4
};
#endif

#ifndef VARIO_ICON_AVG_GLD_MARK_WIDTH
#define VARIO_ICON_AVG_GLD_MARK_WIDTH  5
#define VARIO_ICON_AVG_GLD_MARK_HEIGHT 5
static const unsigned char vario_icon_avg_gld_mark_bits[] = {
    0xe4,0xee,0xff,0xfb,0xf1
};
#endif

#ifndef VARIO_ICON_FINAL_GLIDE_WIDTH
#define VARIO_ICON_FINAL_GLIDE_WIDTH  7
#define VARIO_ICON_FINAL_GLIDE_HEIGHT 7
static const unsigned char vario_icon_final_glide_bits[] = {
    0xbe,0xe3,0xfb,0xe3,0xfb,0xfb,0xbe
};
#endif

#ifndef VARIO_ICON_ARRIVAL_HEIGHT_WIDTH
#define VARIO_ICON_ARRIVAL_HEIGHT_WIDTH  7
#define VARIO_ICON_ARRIVAL_HEIGHT_HEIGHT 7
static const unsigned char vario_icon_arrival_height_bits[] = {
    0x88,0x94,0xa2,0x8c,0x94,0x8c,0x84
};
#endif

#ifndef VARIO_ICON_WIND_DIAMOND_WIDTH
#define VARIO_ICON_WIND_DIAMOND_WIDTH  4
#define VARIO_ICON_WIND_DIAMOND_HEIGHT 4
static const unsigned char vario_icon_wind_diamond_bits[] = {
    0x06,0x0f,0x0f,0x06
};
#endif

#ifndef VARIO_ICON_VARIO_LED_UP_WIDTH
#define VARIO_ICON_VARIO_LED_UP_WIDTH  7
#define VARIO_ICON_VARIO_LED_UP_HEIGHT 10
static const unsigned char vario_icon_vario_led_up_bits[] = {
    0x88,0x88,0x9c,0x9c,0x9c,0xbe,0xbe,0xbe,0xff,0xff
};
#endif

#ifndef VARIO_ICON_VARIO_LED_DOWN_WIDTH
#define VARIO_ICON_VARIO_LED_DOWN_WIDTH  7
#define VARIO_ICON_VARIO_LED_DOWN_HEIGHT 10
static const unsigned char vario_icon_vario_led_down_bits[] = {
    0xff,0xff,0xbe,0xbe,0xbe,0x9c,0x9c,0x9c,0x88,0x88
};
#endif

/* -------------------------------------------------------------------------- */
/* 湲곕낯 viewport 洹쒓꺽                                                          */
/*                                                                            */
/* ??媛믩뱾? "?꾩쭅 UI ?붿쭊 bridge 媛 ?ㅼ젣 viewport 瑜?二쇱엯?섍린 ?? ??fallback   */
/* ?대떎.                                                                        */
/*                                                                            */
/* ?뺤긽 ?고??꾩뿉?쒕뒗 留??꾨젅??                                                  */
/*   UI_ScreenVario_Draw() -> Vario_Display_SetViewports()                      */
/* ?몄텧??癒쇱? ?쇱뼱?섎?濡? renderer ???ㅼ젣 root compose viewport 瑜?蹂닿쾶 ?쒕떎.  */
/* -------------------------------------------------------------------------- */
#define VARIO_LCD_W       240
#define VARIO_LCD_H       128
#define VARIO_STATUSBAR_H 7
#define VARIO_BOTTOMBAR_H 8
#define VARIO_CONTENT_X   0
#define VARIO_CONTENT_Y   (VARIO_STATUSBAR_H + 1)
#define VARIO_CONTENT_W   VARIO_LCD_W
#define VARIO_CONTENT_H   (VARIO_LCD_H - VARIO_STATUSBAR_H - VARIO_BOTTOMBAR_H - 2)

#ifndef VARIO_DISPLAY_PI
#define VARIO_DISPLAY_PI 3.14159265358979323846f
#endif

/* -------------------------------------------------------------------------- */
/* 怨듯넻 Flight UI layout                                                       */
/*                                                                            */
/* ???뱀뀡??Screen 1/2/3 怨듭슜 shell ??"移섏닔 李쎄퀬" ??                         */
/*                                                                            */
/* ?좎? 洹쒖튃                                                                   */
/* 1) ?붾㈃ ?꾩튂/?고듃/媛꾧꺽? ???뱀뀡??留ㅽ겕濡쒕???癒쇱? 諛붽씔??                   */
/* 2) renderer ??APP_STATE 瑜?吏곸젒 ?쎌? ?딄퀬                                 */
/*    Vario_State_GetRuntime() 媛 留뚮뱺 rt field 留??ъ슜?쒕떎.                   */
/* 3) ?쒖떆 ?⑥쐞??Vario_Settings.* helper 濡쒕쭔 諛붽씔??                         */
/*    利? ALT2 瑜?ft 濡??곕줈 ?쒖떆?섍퀬 ?띠쑝硫?Vario_Settings 履??⑥쐞瑜?諛붽씀怨?   */
/*    ?ш린?쒕뒗 unit text ? rounded value helper 留??몄텧?쒕떎.                  */
/*                                                                            */
/* 醫뚰몴 湲곗?                                                                   */
/* - 紐⑤뱺 Y ??full viewport top 湲곗? offset ?대떎.                             */
/* - right aligned ??ぉ? right margin 留ㅽ겕濡쒕? 諛붽씀硫??꾩껜媛 媛숈씠 ?대룞?쒕떎.   */
/* - bottom value ?곸뿭? meta(MAX/value) ? main value box 瑜?遺꾨━?댁꽌         */
/*   ?レ옄媛 ?쒕줈 寃뱀튂吏 ?딄쾶 怨좎젙 ??쑝濡??좎??쒕떎.                              */
/* -------------------------------------------------------------------------- */
#define VARIO_UI_SIDE_BAR_W                       14
#define VARIO_UI_GAUGE_INSTANT_W                   8
#define VARIO_UI_GAUGE_AVG_W                       4
#define VARIO_UI_GAUGE_GAP_W                       1

/* -------------------------------------------------------------------------- */
/* ?곷떒 醫뚯륫 INST Glide ratio gauge                                            */
/*                                                                            */
/* ?ъ슜?먯쓽 理쒖떊 ?붽뎄?ы빆                                                      */
/* - 醫뚯륫 ?몃줈 VARIO bar(14 px)? 2 px gap ?? 利?x=16遺???쒖옉?쒕떎.          */
/* - gauge ?곗륫 ?앹? "?붾㈃ 1/3 吏??+ 16 px" ?대떎.                            */
/* - gauge ?먯껜???붾㈃ 留??꾩뿉???쒖옉?섎뒗 6 px ?믪씠 horizontal bar ?닿퀬,       */
/*   ?덇툑 ??떆 top edge ?먯꽌 ?꾨옒濡??대젮?⑤떎.                                  */
/* - generic sailplane UI ?ㅼ??쇰줈 60:1 ??full scale 濡??≪븘                 */
/*   5:1 minor / 10:1 major tick ??諛곗튂?쒕떎.                                  */
/* -------------------------------------------------------------------------- */
#define VARIO_UI_TOP_GLD_GAUGE_X_OFF              16
#define VARIO_UI_TOP_GLD_GAUGE_RIGHT_EXTRA        16
#define VARIO_UI_TOP_GLD_GAUGE_TOP_Y               0
#define VARIO_UI_TOP_GLD_GAUGE_H                  10
#define VARIO_UI_TOP_GLD_GAUGE_TEXT_GAP_Y          2
#define VARIO_UI_TOP_GLD_GAUGE_MAX_RATIO          20.0f
#define VARIO_UI_TOP_GLD_GAUGE_MINOR_STEP          5.0f
#define VARIO_UI_TOP_GLD_GAUGE_MAJOR_STEP         10.0f
#define VARIO_UI_TOP_GLD_GAUGE_MINOR_TICK_H        5
#define VARIO_UI_TOP_GLD_GAUGE_MAJOR_TICK_H       10
#define VARIO_UI_TOP_GLD_VALUE_UNIT_GAP            2
#define VARIO_UI_TOP_GLD_AVG_TEXT_GAP_X             4
#define VARIO_UI_TOP_GLD_AVG_BAR_TOP_DY             1
#define VARIO_UI_TOP_GLD_AVG_BAR_H                  4
#define VARIO_UI_TOP_GLD_AVG_MARK_CENTER_DY         7
#define VARIO_UI_LOWER_LEFT_GLIDE_BLOCK_SHIFT_UP_PX 8

/* -------------------------------------------------------------------------- */
/* ?섎떒 以묒븰 CLOCK                                                             */
/*                                                                            */
/* 湲곗〈 ?곷떒 以묒븰 ?쒓퀎????glide gauge ? ?쒓컖?곸쑝濡?遺?ろ엳誘濡?              */
/* ?ъ슜?먭? 吏?뺥븳 ?濡?FLT TIME 諛붾줈 ??2 px ?꾩튂濡???릿??                  */
/* -------------------------------------------------------------------------- */
#define VARIO_UI_BOTTOM_CLOCK_GAP_Y                2

/* -------------------------------------------------------------------------- */
/* ?곷떒 ?곗륫 ALT block                                                         */
/* - ALT1? ?붾㈃ ?쒖씪 ?꾩뿉 遺숈씤??                                              */
/* - ALT2??ALT1 諛붾줈 ?꾨옒??"以묎컙 ?ш린" row 濡??대┛??                        */
/* - ALT3??ALT2 ?꾨옒???낅┰ row 濡??붾떎.                                       */
/* - ??row 紐⑤몢 right aligned 濡?怨꾩궛?댁꽌 踰쎄낵 寃뱀튂吏 ?딄쾶 ?쒕떎.              */
/* -------------------------------------------------------------------------- */
#define VARIO_UI_TOP_RIGHT_PAD_X                   4
#define VARIO_UI_TOP_ALT1_TOP_Y                    0
#define VARIO_UI_TOP_ALT1_ICON_TOP_DY              4
#define VARIO_UI_TOP_ALT1_ICON_VALUE_GAP           3
#define VARIO_UI_TOP_ALT1_VALUE_UNIT_GAP           2
#define VARIO_UI_TOP_ALT1_UNIT_TOP_DY             12
#define VARIO_UI_TOP_ALT2_TOP_Y                   28
#define VARIO_UI_TOP_ALT2_ICON_TOP_DY              4
#define VARIO_UI_TOP_ALT3_TOP_Y                   50
#define VARIO_UI_TOP_ALT3_ICON_TOP_DY              2
#define VARIO_UI_TOP_ALT_ROW_ICON_GAP              3
#define VARIO_UI_TOP_ALT_ROW_VALUE_UNIT_GAP        2

/* -------------------------------------------------------------------------- */
/* 以묒븰 NAV / COMPASS                                                          */
/* - 吏由꾩쓣 5px ?ㅼ슦?쇰뒗 ?붽뎄瑜?諛섏쁺?섍린 ?꾪빐 diameter grow ?곸닔瑜?遺꾨━?덈떎.    */
/* - ?ㅼ젣 諛섏?由?利앷????뺤닔 ?쎌? 醫뚰몴怨꾨씪 ceil(5/2)=3 px 濡?諛섏삱由쇳븳??       */
/* - ?먯쓽 留??꾨옒媛 ?먭린 ?곸뿭??理쒗븯?⑥뿉 ?용룄濡?center_y 瑜?怨꾩궛?쒕떎.          */
/* -------------------------------------------------------------------------- */
#define VARIO_UI_NAV_LABEL_BASELINE_Y             33
#define VARIO_UI_NAV_LABEL_TO_COMPASS_GAP          2
#define VARIO_UI_COMPASS_MAX_RADIUS               35
#define VARIO_UI_COMPASS_DIAMETER_GROW_PX          5
#define VARIO_UI_COMPASS_SIDE_MARGIN               8
#define VARIO_UI_COMPASS_LABEL_INSET               7
#define VARIO_UI_COMPASS_BOTTOM_GAP                0
#define VARIO_UI_COMPASS_RADIUS_EXTRA_PX           2
#define VARIO_UI_COMPASS_CENTER_Y_SHIFT_PX        -4

/* -------------------------------------------------------------------------- */
/* ?섎떒 MAX/meta + main value block                                            */
/* - meta label/value ??main number box ? 遺꾨━??Y 瑜??⑥꽌 寃뱀묠??諛⑹??쒕떎.   */
/* - main number box ??怨좎젙 ??怨좎젙 ?믪씠?대ŉ decimal ? dot ?놁씠 top-frac 濡?  */
/*   洹몃┛??                                                                   */
/* -------------------------------------------------------------------------- */
#define VARIO_UI_BOTTOM_BOX_W                     64
#define VARIO_UI_BOTTOM_BOX_H                     24
#define VARIO_UI_BOTTOM_BOX_BOTTOM_PAD             4
#define VARIO_UI_BOTTOM_META_LABEL_BASELINE_Y     94
#define VARIO_UI_BOTTOM_META_VALUE_BASELINE_Y    102
#define VARIO_UI_BOTTOM_VALUE_BASELINE_DY         21
#define VARIO_UI_BOTTOM_META_BOX_W                32
#define VARIO_UI_BOTTOM_META_GAP_Y                 2
#define VARIO_UI_BOTTOM_VARIO_X_PAD                4
#define VARIO_UI_BOTTOM_GS_X_PAD                   4
#define VARIO_UI_BOTTOM_ICON_GAP_Y                 1
#define VARIO_UI_ALT_ROW_GAP_Y                     2
#define VARIO_UI_ALT_GRAPH_W                      54
#define VARIO_UI_ALT_GRAPH_H                      14
#define VARIO_UI_ALT_GRAPH_RIGHT_PAD               2
#define VARIO_UI_ALT_GRAPH_GAP_Y                   2
#define VARIO_UI_ALT_GRAPH_MIN_SPAN_M             50.0f
#define VARIO_UI_ALT_GRAPH_CENTER_MARGIN_RATIO     0.18f
#define VARIO_UI_ALT_GRAPH_CENTER_LP_ALPHA         0.18f
#define VARIO_UI_ALT_GRAPH_SHRINK_THRESHOLD_RATIO  0.70f
#define VARIO_UI_ALT_GRAPH_SHRINK_HOLD_FRAMES      8u

/* -------------------------------------------------------------------------- */
/* page 3 stub                                                                 */
/* -------------------------------------------------------------------------- */
#define VARIO_UI_STUB_TITLE_BASELINE_DY           -2
#define VARIO_UI_STUB_SUB_BASELINE_DY             14

/* -------------------------------------------------------------------------- */
/* side bar scale definition                                                   */
/* - left vario  : inside edge(right edge) ??tick 媛 遺숇뒗??                   */
/* - right GS    : inside edge(left edge) ??tick 媛 遺숇뒗??                    */
/* - 0.0 m/s line ? 3px ?먭퍡 + 理쒖옣 湲몄씠濡?洹몃젮 以묒떖??媛뺥븯寃??쒖떆?쒕떎.        */
/*                                                                            */
/* ?대쾲 蹂寃쎌쓽 ?듭떖                                                            */
/* - 醫뚯륫 VARIO ???ㅼ젙媛?4.0 / 5.0 ???곕씪 full-scale ??諛붾먮떎.              */
/* - ?곗륫 GS ???ㅼ젙??top speed 瑜?full-scale 濡??곕릺,                        */
/*   50 km/h 誘몃쭔?대㈃ 5 / 2.5 tick, 洹??댁긽?대㈃ 10 / 5 tick ???ъ슜?쒕떎.      */
/* - tick ??X ?꾩튂 / 湲몄씠 / bar X ?꾩튂 / bar ??? 湲곗〈 洹몃?濡??좎??쒕떎.      */
/* -------------------------------------------------------------------------- */
#define VARIO_UI_VARIO_MINOR_STEP_MPS             0.5f
#define VARIO_UI_VARIO_MAJOR_STEP_MPS             1.0f
#define VARIO_UI_VARIO_SCALE_MIN_X10               40u
#define VARIO_UI_VARIO_SCALE_MAX_X10               50u
#define VARIO_UI_VARIO_ZERO_LINE_W                 14u
#define VARIO_UI_VARIO_ZERO_LINE_THICKNESS          3u
#define VARIO_UI_GS_SCALE_MIN_KMH                 30.0f
#define VARIO_UI_GS_SCALE_MAX_KMH                150.0f
#define VARIO_UI_GS_LOW_RANGE_SWITCH_KMH          50.0f
#define VARIO_UI_GS_LOW_MAJOR_STEP_KMH             5.0f
#define VARIO_UI_GS_LOW_MINOR_STEP_KMH             2.5f
#define VARIO_UI_GS_HIGH_MAJOR_STEP_KMH           10.0f
#define VARIO_UI_GS_HIGH_MINOR_STEP_KMH            5.0f
#define VARIO_UI_SCALE_MAJOR_W                    14u
#define VARIO_UI_SCALE_MINOR_W                     8u

/* -------------------------------------------------------------------------- */
/* decimal typography                                                          */
/* - ?뚯닔?먯? '.' ??李띿? ?딅뒗??                                               */
/* - frac digit ? ?뺤닔遺 ?ㅻⅨ履??꾩뿉 ?묒? ?고듃濡?top-align ?쒕떎.               */
/* - gap/top bias 瑜?諛붽씀硫?frac digit ?????덈뒗 ?먮굦??議곗젅?????덈떎.       */
/* -------------------------------------------------------------------------- */
#define VARIO_UI_DECIMAL_FRAC_GAP_X                1
#define VARIO_UI_DECIMAL_SIGN_GAP_X                1
#define VARIO_UI_DECIMAL_FRAC_TOP_BIAS_Y           0

/* -------------------------------------------------------------------------- */
/* Flight average / peak cache                                                 */
/*                                                                            */
/* APP_STATE snapshot 援ъ“??洹몃?濡??좎??섍퀬,                                  */
/* display 怨꾩링??publish 媛?5 Hz)???ㅼ떆 ?꾩쟻?댁꽌 ?됯퇏/Top Speed 瑜?留뚮뱺??    */
/* -------------------------------------------------------------------------- */
#define VARIO_UI_AVG_BUFFER_SIZE                  640u
#define VARIO_UI_SLOW_SPEED_TAU_S                    1.10f
#define VARIO_UI_SLOW_GLIDE_UPDATE_MS                2000u

/* -------------------------------------------------------------------------- */
/* ?섎룞 WP 湲곕낯媛?                                                             */
/* -------------------------------------------------------------------------- */
#define VARIO_UI_DEFAULT_WP_VALID                 0u
#define VARIO_UI_DEFAULT_WP_LAT_E7                0
#define VARIO_UI_DEFAULT_WP_LON_E7                0

/* -------------------------------------------------------------------------- */
/* ?고듃 吏??                                                                  */
/*                                                                            */
/* ?ъ슜踰?                                                                    */
/* - 湲瑗댁쓣 諛붽씀怨??띠쑝硫??꾨옒 FONT 留ㅽ겕濡쒕쭔 援먯껜?쒕떎.                          */
/* - 醫뚰몴? ?고듃瑜??④퍡 ?먮낫硫?異⑸룎 ?놁씠 ?덉씠?꾩썐???щ같移섑븯湲??쎈떎.            */
/* -------------------------------------------------------------------------- */
#define VARIO_UI_FONT_TOP_FLT_VALUE               u8g2_font_9x15_mf
#define VARIO_UI_FONT_TOP_GLD_LABEL               u8g2_font_6x12_mf
#define VARIO_UI_FONT_TOP_GLD_VALUE               u8g2_font_helvB10_tf
#define VARIO_UI_FONT_TOP_GLD_SUFFIX              u8g2_font_5x8_tr
#define VARIO_UI_FONT_TOP_CLOCK                   u8g2_font_6x12_mf

#define VARIO_UI_FONT_ALT1_VALUE                  u8g2_font_logisoso24_tn
#define VARIO_UI_FONT_ALT1_UNIT                   u8g2_font_5x7_tf
#define VARIO_UI_FONT_ALT2_VALUE                  u8g2_font_7x14B_tf
#define VARIO_UI_FONT_ALT2_UNIT                   u8g2_font_5x7_tf
#define VARIO_UI_FONT_ALT3_VALUE                  u8g2_font_7x14B_tf
#define VARIO_UI_FONT_ALT3_UNIT                   u8g2_font_5x7_tf

#define VARIO_UI_FONT_NAV_LINE                    u8g2_font_6x12_mf
#define VARIO_UI_FONT_COMPASS_CARDINAL            u8g2_font_5x8_tr

#define VARIO_UI_FONT_BOTTOM_MAIN                 u8g2_font_logisoso24_tn
#define VARIO_UI_FONT_BOTTOM_SIGN                 u8g2_font_7x14B_tf
#define VARIO_UI_FONT_BOTTOM_FRAC                 u8g2_font_7x14B_tf
#define VARIO_UI_FONT_BOTTOM_MAX_LABEL            u8g2_font_4x6_tf
#define VARIO_UI_FONT_BOTTOM_MAX_VALUE            u8g2_font_helvB08_tf

/* -------------------------------------------------------------------------- */
/* Logisoso fixed-slot digit layout                                            */
/*                                                                            */
/* u8g2??logisoso 怨꾩뿴? ?レ옄 '1' ??씠 ?ㅻⅨ ?レ옄蹂대떎 醫곷떎.                   */
/* ?곕씪??臾몄옄???꾩껜 ??쓣 留??꾨젅??怨꾩궛?댁꽌 right-align ?섎㈃               */
/* 411 / 412 / 413 媛숈? 媛믪뿉??留덉?留??먮━??glyph ??李⑥씠留뚰겮                */
/* ?レ옄 釉붾줉 ?꾩껜媛 醫뚯슦濡??붾뱾??蹂댁씤??                                      */
/*                                                                            */
/* ?꾨옒 slot count ??"媛??먮━??湲곗? ?덈? ?꾩튂" 瑜?怨좎젙?섍린 ?꾪빐             */
/* ?대떎. ?ㅼ젣 glyph ??slot ?덉쓽 ?쇱そ??怨좎젙?댁꽌, '1' ???ㅼ뼱???            */
/* 媛숈? ?먮━(column)??洹몃?濡?癒몃Ъ寃?留뚮뱺??                                 */
/* -------------------------------------------------------------------------- */
#define VARIO_UI_LOGISOSO_SLOT_SAMPLE            "0"
#define VARIO_UI_ALT1_FIXED_SLOT_COUNT           5u
#define VARIO_UI_SPEED_FIXED_SLOT_COUNT          3u
/* ---------------------------------------------------------------------- */
/* bottom big-digit spacing tuning                                        */
/*                                                                        */
/* ?ъ슜?먯쓽 ?대쾲 ?섏젙 ?붽뎄?ы빆                                             */
/* - ???レ옄? ?묒? 0.1 digit ???꾩옱 ?덈Т 媛源뚯썙 蹂댁씤??                  */
/* - bar gauge / other UI ???덈? ?꾩튂??嫄대뱶由ъ? ?딄퀬,                     */
/*   ???レ옄 履쎈쭔 ?쇱そ?쇰줈 2 px ?대룞?쒖폒 visual gap ???섎┛??              */
/* - 利?small digit(frac) ? 洹몃?濡??먭퀬, main logisoso digits 留?        */
/*   absolute X 瑜?-2 px ??媛믪쑝濡?洹몃┛??                                 */
/* ---------------------------------------------------------------------- */
#define VARIO_UI_BOTTOM_MAIN_TO_FRAC_EXTRA_GAP_PX 2

/* ---------------------------------------------------------------------- */
/* ALT2 / ALT3 signed five-slot altitude contract                         */
/*                                                                        */
/* ?ъ슜?먯쓽 理쒖떊 ?붽뎄?ы빆                                                  */
/* - ALT2 / ALT3 ??unit 醫낅쪟? 臾닿??섍쾶                                   */
/*   -9999 ~ 99999 踰붿쐞 ?덉뿉?쒕쭔 洹몃┛??                                   */
/* - ?뚯닔 遺?몃뒗 ??긽 媛???쇱そ sign slot ??怨좎젙?쒕떎.                      */
/* - ?묒닔??5-slot right align, ?뚯닔??"-1234" ?뺤떇?쇰줈 slot 怨좎젙?쒕떎. */
/* ---------------------------------------------------------------------- */
#define VARIO_UI_ALT23_SIGNED_MIN_DISPLAY      (-9999L)
#define VARIO_UI_ALT23_SIGNED_MAX_DISPLAY      (99999L)

/* -------------------------------------------------------------------------- */
/* ALT1 signed five-slot Logisoso contract                                      */
/*                                                                            */
/* ?ъ슜?먭? ?댁쟾??吏?뺥븳 怨좊룄 怨꾩빟??ALT1 ???レ옄?먮룄 洹몃?濡??곸슜?쒕떎.      */
/* - ?쒖떆 踰붿쐞 : -9999 ~ 99999                                                */
/* - ?꾩껜 ??column)? 5媛?怨좎젙                                               */
/* - ?뚯닔 遺?몃뒗 ??긽 媛???쇱そ sign slot ??怨좎젙                             */
/* - ?묒닔??5-digit right align                                               */
/*                                                                            */
/* ?대쾲 ?섏젙?먯꽌???뚯닔 怨좊룄?먯꽌 ???レ옄媛 源⑥???踰꾧렇瑜?諛붾줈 ??怨꾩빟?쇰줈      */
/* ?닿껐?쒕떎.                                                                  */
/* -------------------------------------------------------------------------- */
#define VARIO_UI_ALT1_SIGNED_MIN_DISPLAY       (-9999L)
#define VARIO_UI_ALT1_SIGNED_MAX_DISPLAY       (99999L)

/* -------------------------------------------------------------------------- */
/* ALT1 Logisoso fixed-slot numeric area X shift                              */
/*                                                                            */
/* ?ъ슜?먯쓽 理쒖떊 ?붽뎄?ы빆: 怨좎젙 ??怨좎젙 ?レ옄 怨좊룄 ?곸뿭??X異?湲곗? 4 px ?쇱そ?쇰줈 */
/* ?대룞?쒕떎.                                                                  */
/*                                                                            */
/* 援ы쁽 諛⑹묠                                                                   */
/* - ?⑥쐞(unit) ?띿뒪?몄쓽 ?덈? ?꾩튂??洹몃?濡??붾떎.                             */
/* - ALT1 ?レ옄 諛뺤뒪??left edge 留?4 px ?쇱そ?쇰줈 ?대룞?쒕떎.                    */
/* - 利?value_x 怨꾩궛 ??蹂꾨룄 ?고???shift 濡쒖쭅???먯? ?딄퀬,                   */
/*   理쒖쥌 ?덈? 醫뚰몴???곸닔 ?ㅽ봽?뗭쓣 吏곸젒 諛섏쁺?쒕떎.                             */
/* -------------------------------------------------------------------------- */
#define VARIO_UI_ALT1_FIXED_VALUE_X_SHIFT_PX   4

#define VARIO_UI_FONT_STUB_TITLE                  u8g2_font_10x20_mf
#define VARIO_UI_FONT_STUB_SUB                    u8g2_font_6x12_mf

typedef enum
{
    VARIO_UI_ALIGN_LEFT = 0u,
    VARIO_UI_ALIGN_CENTER,
    VARIO_UI_ALIGN_RIGHT
} vario_ui_align_t;

typedef struct
{
    uint32_t sample_stamp_ms[VARIO_UI_AVG_BUFFER_SIZE];
    float    vario_mps[VARIO_UI_AVG_BUFFER_SIZE];
    float    speed_kmh[VARIO_UI_AVG_BUFFER_SIZE];
    uint16_t head;
    uint16_t count;
    uint32_t last_publish_ms;
    uint32_t last_flight_start_ms;
    uint32_t last_bar_update_ms;

    /* ---------------------------------------------------------------------- */
    /*  display layer 媛 ?좎??섎뒗 lightweight cache                            */
    /*                                                                        */
    /*  - top_speed_kmh       : flight 以?peak GS                              */
    /*  - filtered_gs_bar_kmh : GS side bar ?꾩슜 smoothing cache               */
    /*                                                                        */
    /*  ?덉쟾 renderer 媛 ?곕뜕                                                  */
    /*  - filtered_vario_bar_mps                                               */
    /*  - vario_bar_zero_latched                                               */
    /*  ???꾩옱 寃쎈줈?먯꽌 ???댁긽 ?ъ슜?섏? ?딆쑝誘濡??쒓굅?쒕떎.                  */
    /*  醫뚯륫 VARIO bar ??runtime snapshot ??以 fast path 媛믪쓣               */
    /*  洹몃?濡?諛쏆븘 ?ㅼ??쇰쭅留??섑뻾?쒕떎.                                      */
    /* ---------------------------------------------------------------------- */
    float    top_speed_kmh;
    float    filtered_gs_bar_kmh;
    bool     displayed_speed_valid;
    float    displayed_speed_kmh;
    uint32_t last_display_smoothing_ms;
    bool     displayed_glide_valid;
    float    displayed_glide_ratio;
    uint32_t last_displayed_glide_update_ms;
    bool     fast_average_glide_valid;
    float    fast_average_glide_ratio;
    bool     trail_heading_up_mode;
    vario_nav_target_mode_t nav_mode;
    int32_t  wp_lat_e7;
    int32_t  wp_lon_e7;
    bool     wp_valid;

    /* ---------------------------------------------------------------------- */
    /* altitude sparkline dynamic scale cache                                  */
    /*                                                                        */
    /* ?ㅼ젣 諛붾━?ㅻ쪟??ALT history graph 泥섎읆                                  */
    /* - scale ?뺤옣? 利됱떆 諛섏쁺                                                */
    /* - scale 異뺤냼???щ윭 frame 愿李???泥쒖쿇??諛섏쁺                           */
    /* - center ??low-pass 濡??곕씪媛?? history 媛 寃쎄퀎 洹쇱쿂源뚯? ?ㅻ㈃         */
    /*   利됱떆 recenter ?댁꽌 graph 諛뽰쑝濡??吏 ?딄쾶 ?쒕떎.                      */
    /* ---------------------------------------------------------------------- */
    bool     altitude_graph_initialized;
    uint8_t  altitude_graph_shrink_votes;
    float    altitude_graph_center_m;
    float    altitude_graph_span_m;

    /* ------------------------------------------------------------------ */
    /* display-local flight distance accumulator                           */
    /*                                                                    */
    /* ?대쾲 ?붽뎄?ы빆?먯꽌??醫뚰븯??glide block ??target slot ??鍮꾩뿀????  */
    /* ?숈씪???꾩튂??"吏湲덇퉴吏 鍮꾪뻾??嫄곕━" 瑜?蹂댁뿬 以??                 */
    /*                                                                    */
    /* runtime struct ????public field 瑜?異붽??섏? ?딄퀬,                 */
    /* display layer 媛 ?대? 蹂댁쑀??publish cadence sample ???댁슜??       */
    /* ground distance 瑜??곷텇?쒕떎.                                        */
    /*                                                                    */
    /* - accumulated_flight_distance_m : ?꾩옱 鍮꾪뻾 ?꾩쟻 嫄곕━               */
    /* - last_distance_sample_ms       : 留덉?留??곷텇 sample ?쒓컖           */
    /* - last_distance_speed_kmh       : trapezoid ?곷텇???댁쟾 ?띾룄        */
    /* ------------------------------------------------------------------ */
    float    accumulated_flight_distance_m;
    uint32_t last_distance_sample_ms;
    float    last_distance_speed_kmh;
} vario_display_dynamic_t;

typedef struct
{
    bool  current_valid;
    bool  target_valid;
    bool  heading_valid;
    char  label[8];
    float distance_m;
    float bearing_deg;
    float relative_bearing_deg;
} vario_display_nav_solution_t;

typedef struct
{
    bool        show_compass;
    bool        show_trail_background;
    bool        show_attitude_indicator;
    bool        show_stub_overlay;
    const char *stub_title;
    const char *stub_subtitle;
} vario_display_page_cfg_t;

/* -------------------------------------------------------------------------- */
/* side bar scale helper forward declaration                                   */
/*                                                                            */
/* bar smoothing / runtime selection 濡쒖쭅??helper ?뺤쓽蹂대떎 癒쇱? ?깆옣?섎?濡?     */
/* ?ㅼ젙 湲곕컲 scale helper ??prototype 留??욎そ??諛곗튂?쒕떎.                     */
/* -------------------------------------------------------------------------- */
static float vario_display_get_vario_scale_mps(const vario_settings_t *settings);
static float vario_display_get_gs_scale_kmh(const vario_settings_t *settings);
static bool  vario_display_compute_glide_ratio(float speed_kmh, float vario_mps, float *out_ratio);
static void  vario_display_update_slow_number_caches(const vario_runtime_t *rt);

static vario_viewport_t s_vario_full_viewport =
{
    0,
    0,
    VARIO_LCD_W,
    VARIO_LCD_H
};

static vario_viewport_t s_vario_content_viewport =
{
    VARIO_CONTENT_X,
    VARIO_CONTENT_Y,
    VARIO_CONTENT_W,
    VARIO_CONTENT_H
};

static vario_display_dynamic_t s_vario_ui_dynamic APP_CCMRAM_DATA =
{
    { 0u },
    { 0.0f },
    { 0.0f },
    0u,
    0u,
    0u,
    0u,
    0u,
    0.0f,
    0.0f,
    VARIO_NAV_TARGET_START,
    VARIO_UI_DEFAULT_WP_LAT_E7,
    VARIO_UI_DEFAULT_WP_LON_E7,
    (VARIO_UI_DEFAULT_WP_VALID != 0u) ? true : false,
    false,
    0u,
    0.0f,
    0.0f
};

/* -------------------------------------------------------------------------- */
/* ?대? helper: viewport sanity clamp                                          */
/*                                                                            */
/* ?댁쑀                                                                          */
/* - bridge 媛 ?섎せ??媛믪쓣 二쇰뜑?쇰룄 renderer 媛 ?뚯닔 width/height 瑜??≪? ?딄쾶   */
/*   諛⑹뼱?쒕떎.                                                                  */
/* - 湲곗〈 怨좎젙 240x128 醫뚰몴怨꾨? 踰쀬뼱?섎뒗 媛믪쓣 ?섎씪?몃떎.                         */
/* -------------------------------------------------------------------------- */
static void vario_display_common_sanitize_viewport(vario_viewport_t *v)
{
    if (v == NULL)
    {
        return;
    }

    if (v->x < 0)
    {
        v->x = 0;
    }

    if (v->y < 0)
    {
        v->y = 0;
    }

    if (v->x > VARIO_LCD_W)
    {
        v->x = VARIO_LCD_W;
    }

    if (v->y > VARIO_LCD_H)
    {
        v->y = VARIO_LCD_H;
    }

    if (v->w < 0)
    {
        v->w = 0;
    }

    if (v->h < 0)
    {
        v->h = 0;
    }

    if ((v->x + v->w) > VARIO_LCD_W)
    {
        v->w = (int16_t)(VARIO_LCD_W - v->x);
    }

    if ((v->y + v->h) > VARIO_LCD_H)
    {
        v->h = (int16_t)(VARIO_LCD_H - v->y);
    }
}

static float vario_display_absf(float value)
{
    return (value < 0.0f) ? -value : value;
}

static float vario_display_clampf(float value, float min_value, float max_value)
{
    if (value < min_value)
    {
        return min_value;
    }
    if (value > max_value)
    {
        return max_value;
    }
    return value;
}

static float vario_display_deg_to_rad(float deg)
{
    return deg * (VARIO_DISPLAY_PI / 180.0f);
}

static float vario_display_rad_to_deg(float rad)
{
    return rad * (180.0f / VARIO_DISPLAY_PI);
}

static float vario_display_wrap_360(float deg)
{
    while (deg < 0.0f)
    {
        deg += 360.0f;
    }
    while (deg >= 360.0f)
    {
        deg -= 360.0f;
    }
    return deg;
}

static float vario_display_wrap_pm180(float deg)
{
    while (deg > 180.0f)
    {
        deg -= 360.0f;
    }
    while (deg < -180.0f)
    {
        deg += 360.0f;
    }
    return deg;
}

static int16_t vario_display_measure_text(u8g2_t *u8g2, const uint8_t *font, const char *text)
{
    if ((u8g2 == NULL) || (font == NULL) || (text == NULL))
    {
        return 0;
    }

    u8g2_SetFont(u8g2, font);
    return (int16_t)u8g2_GetStrWidth(u8g2, text);
}

static int16_t vario_display_get_font_height(u8g2_t *u8g2, const uint8_t *font)
{
    int16_t ascent;
    int16_t descent;

    if ((u8g2 == NULL) || (font == NULL))
    {
        return 0;
    }

    u8g2_SetFont(u8g2, font);
    ascent = (int16_t)u8g2_GetAscent(u8g2);
    descent = (int16_t)u8g2_GetDescent(u8g2);
    return (int16_t)(ascent - descent);
}

static void vario_display_draw_text_box_top(u8g2_t *u8g2,
                                            int16_t box_x,
                                            int16_t top_y,
                                            int16_t box_w,
                                            vario_ui_align_t align,
                                            const uint8_t *font,
                                            const char *text)
{
    int16_t text_w;
    int16_t draw_x;

    if ((u8g2 == NULL) || (font == NULL) || (text == NULL))
    {
        return;
    }

    u8g2_SetFontPosTop(u8g2);
    u8g2_SetFont(u8g2, font);
    text_w = (int16_t)u8g2_GetStrWidth(u8g2, text);

    switch (align)
    {
        case VARIO_UI_ALIGN_LEFT:
            draw_x = box_x;
            break;

        case VARIO_UI_ALIGN_CENTER:
            draw_x = (int16_t)(box_x + ((box_w - text_w) / 2));
            break;

        case VARIO_UI_ALIGN_RIGHT:
        default:
            draw_x = (int16_t)(box_x + box_w - text_w);
            break;
    }

    if (draw_x < box_x)
    {
        draw_x = box_x;
    }

    u8g2_DrawStr(u8g2, draw_x, top_y, text);
    u8g2_SetFontPosBaseline(u8g2);
}

static void vario_display_trim_leading_zero(char *text)
{
    if (text == NULL)
    {
        return;
    }

    if ((text[0] == '0') && (text[1] == '.'))
    {
        memmove(text, text + 1, strlen(text));
    }
    else if (((text[0] == '-') || (text[0] == '+')) &&
             (text[1] == '0') &&
             (text[2] == '.'))
    {
        memmove(text + 1, text + 2, strlen(text + 2) + 1u);
    }
}

static void vario_display_split_decimal_value(const char *value_text,
                                              char *sign_buf,
                                              size_t sign_len,
                                              char *whole_buf,
                                              size_t whole_len,
                                              char *frac_buf,
                                              size_t frac_len)
{
    const char *digits;
    const char *dot_pos;
    size_t      whole_chars;

    if ((value_text == NULL) ||
        (sign_buf == NULL) || (sign_len == 0u) ||
        (whole_buf == NULL) || (whole_len == 0u) ||
        (frac_buf == NULL) || (frac_len == 0u))
    {
        return;
    }

    sign_buf[0] = '\0';
    whole_buf[0] = '\0';
    frac_buf[0] = '\0';

    digits = value_text;
    if ((*digits == '-') || (*digits == '+'))
    {
        sign_buf[0] = *digits;
        if (sign_len > 1u)
        {
            sign_buf[1] = '\0';
        }
        ++digits;
    }

    dot_pos = strchr(digits, '.');
    if (dot_pos == NULL)
    {
        snprintf(whole_buf, whole_len, "%s", digits);
    }
    else
    {
        whole_chars = (size_t)(dot_pos - digits);
        if (whole_chars >= whole_len)
        {
            whole_chars = whole_len - 1u;
        }
        memcpy(whole_buf, digits, whole_chars);
        whole_buf[whole_chars] = '\0';

        if ((dot_pos[1] != '\0') && (frac_len > 1u))
        {
            frac_buf[0] = dot_pos[1];
            frac_buf[1] = '\0';
        }
    }

}

static bool vario_display_runtime_has_usable_gps_fix(const vario_runtime_t *rt)
{
    /* ------------------------------------------------------------------ */
    /* display 怨꾩링??蹂대뒗 GPS usable ?먯젙                                */
    /*                                                                    */
    /* Vario_State 媛 留뚮뱺 runtime.gps_valid 瑜?1李?寃뚯씠?몃줈 ?곌퀬,         */
    /* trail / nav / speed block ???숈씪??湲곗???蹂닿쾶 ?섎젮怨?             */
    /* 醫뚰몴 踰붿쐞? 3D fix 議곌굔????踰???諛⑹뼱?곸쑝濡??뺤씤?쒕떎.             */
    /* ------------------------------------------------------------------ */
    if (rt == NULL)
    {
        return false;
    }

    if ((rt->gps_valid == false) ||
        (rt->gps.fix.valid == false) ||
        (rt->gps.fix.fixOk == false) ||
        (rt->gps.fix.fixType < 3u))
    {
        return false;
    }

    if ((rt->gps.fix.lat > 900000000) || (rt->gps.fix.lat < -900000000) ||
        (rt->gps.fix.lon > 1800000000) || (rt->gps.fix.lon < -1800000000) ||
        ((rt->gps.fix.lat == 0) && (rt->gps.fix.lon == 0)))
    {
        return false;
    }

    return true;
}

static void vario_display_draw_checker_dither_box(u8g2_t *u8g2,
                                                  int16_t x,
                                                  int16_t y,
                                                  int16_t w,
                                                  int16_t h)
{
    int16_t px;
    int16_t py;

    if ((u8g2 == NULL) || (w <= 0) || (h <= 0))
    {
        return;
    }

    /* ------------------------------------------------------------------ */
    /* GPS INOP ?⑦꽩                                                        */
    /*                                                                    */
    /* ?ъ슜?먭? 吏?뺥븳 諛⑹떇 洹몃?濡?                                        */
    /* - ???X column ?먯꽌?????Y pixel 留?李띾뒗??                      */
    /* - 吏앹닔 X column ?먯꽌??吏앹닔 Y pixel 留?李띾뒗??                      */
    /*                                                                    */
    /* 利? x/y parity 媛 媛숈? ?먮쭔 李띾뒗 checker dither 濡?                */
    /* ?곗륫 ?먭볼??GS instant lane ???쏀븳 ?뚯깋泥섎읆 蹂댁씠寃?留뚮뱺??         */
    /* ------------------------------------------------------------------ */
    for (py = y; py < (int16_t)(y + h); ++py)
    {
        for (px = x; px < (int16_t)(x + w); ++px)
        {
            if (((px ^ py) & 0x01) == 0)
            {
                u8g2_DrawPixel(u8g2, px, py);
            }
        }
    }
}

static bool vario_display_text_is_unsigned_digits(const char *text)
{
    size_t i;

    if ((text == NULL) || (text[0] == '\0'))
    {
        return false;
    }

    for (i = 0u; text[i] != '\0'; ++i)
    {
        if ((text[i] < '0') || (text[i] > '9'))
        {
            return false;
        }
    }

    return true;
}

static bool vario_display_text_is_decimal_unsigned(const char *whole, const char *frac)
{
    if ((whole == NULL) || (frac == NULL))
    {
        return false;
    }

    if ((frac[0] == '\0') || (frac[1] != '\0'))
    {
        return false;
    }

    if ((frac[0] < '0') || (frac[0] > '9'))
    {
        return false;
    }

    return vario_display_text_is_unsigned_digits(whole);
}

static void vario_display_draw_logisoso_slots(u8g2_t *u8g2,
                                              const uint8_t *font,
                                              int16_t box_x,
                                              int16_t top_y,
                                              int16_t box_w,
                                              vario_ui_align_t align,
                                              const char *digits,
                                              uint8_t slot_count)
{
    int16_t digit_slot_w;
    int16_t draw_x;
    int16_t total_w;
    size_t  digit_len;
    uint8_t start_slot;
    uint8_t i;
    char    digit_ch[2];

    if ((u8g2 == NULL) || (font == NULL) || (digits == NULL) || (slot_count == 0u))
    {
        return;
    }

    digit_len = strlen(digits);
    if ((digit_len == 0u) || (digit_len > slot_count) ||
        (vario_display_text_is_unsigned_digits(digits) == false))
    {
        vario_display_draw_text_box_top(u8g2,
                                        box_x,
                                        top_y,
                                        box_w,
                                        align,
                                        font,
                                        digits);
        return;
    }

    digit_slot_w = vario_display_measure_text(u8g2, font, VARIO_UI_LOGISOSO_SLOT_SAMPLE);
    if (digit_slot_w <= 0)
    {
        vario_display_draw_text_box_top(u8g2,
                                        box_x,
                                        top_y,
                                        box_w,
                                        align,
                                        font,
                                        digits);
        return;
    }

    total_w = (int16_t)((int16_t)slot_count * digit_slot_w);
    switch (align)
    {
        case VARIO_UI_ALIGN_LEFT:
            draw_x = box_x;
            break;

        case VARIO_UI_ALIGN_CENTER:
            draw_x = (int16_t)(box_x + ((box_w - total_w) / 2));
            break;

        case VARIO_UI_ALIGN_RIGHT:
        default:
            draw_x = (int16_t)(box_x + box_w - total_w);
            break;
    }

    if (draw_x < box_x)
    {
        draw_x = box_x;
    }

    start_slot = (uint8_t)(slot_count - (uint8_t)digit_len);
    digit_ch[1] = '\0';

    u8g2_SetFontPosTop(u8g2);
    u8g2_SetFont(u8g2, font);
    for (i = 0u; i < digit_len; ++i)
    {
        digit_ch[0] = digits[i];
        u8g2_DrawStr(u8g2,
                     (int16_t)(draw_x + ((int16_t)(start_slot + i) * digit_slot_w)),
                     top_y,
                     digit_ch);
    }
    u8g2_SetFontPosBaseline(u8g2);
}

static void vario_display_draw_logisoso_integer_value(u8g2_t *u8g2,
                                                      int16_t box_x,
                                                      int16_t top_y,
                                                      int16_t box_w,
                                                      vario_ui_align_t align,
                                                      const char *digits,
                                                      uint8_t slot_count)
{
    vario_display_draw_logisoso_slots(u8g2,
                                      VARIO_UI_FONT_ALT1_VALUE,
                                      box_x,
                                      top_y,
                                      box_w,
                                      align,
                                      digits,
                                      slot_count);
}

static void vario_display_draw_logisoso_signed_altitude_value(u8g2_t *u8g2,
                                                              int16_t box_x,
                                                              int16_t top_y,
                                                              int16_t box_w,
                                                              vario_ui_align_t align,
                                                              const char *value_text,
                                                              uint8_t slot_count)
{
    int16_t digit_slot_w;
    int16_t draw_x;
    int16_t total_w;
    size_t  text_len;
    size_t  digit_len;
    uint8_t start_slot;
    uint8_t i;
    char    digit_ch[2];
    const char *digits;
    bool    negative;

    if ((u8g2 == NULL) || (value_text == NULL) || (slot_count == 0u))
    {
        return;
    }

    text_len = strlen(value_text);
    if (text_len == 0u)
    {
        return;
    }

    /* ------------------------------------------------------------------ */
    /* ALT1 ?꾩슜 signed fixed-slot renderer                                */
    /*                                                                    */
    /* 湲곕? ?낅젰                                                            */
    /* - ?묒닔 : "    0" ~ "99999"                                        */
    /* - ?뚯닔 : "-   1" ~ "-9999"                                        */
    /*                                                                    */
    /* 利? 臾몄옄?댁? ?대? formatter ?④퀎?먯꽌 5-column 怨꾩빟??留뚯”?쒕떎怨?      */
    /* 媛?뺥븳?? ?ш린?쒕뒗 sign slot 怨?digit slot ??X 湲곗?留?怨좎젙?쒕떎.     */
    /* ------------------------------------------------------------------ */
    negative = (value_text[0] == '-');
    digits = negative ? (value_text + 1) : value_text;

    while ((*digits == ' ') && (*digits != '\0'))
    {
        ++digits;
    }

    digit_len = strlen(digits);

    if ((digit_len == 0u) || (digit_len > slot_count) ||
        (vario_display_text_is_unsigned_digits(digits) == false))
    {
        vario_display_draw_text_box_top(u8g2,
                                        box_x,
                                        top_y,
                                        box_w,
                                        align,
                                        VARIO_UI_FONT_ALT1_VALUE,
                                        value_text);
        return;
    }

    digit_slot_w = vario_display_measure_text(u8g2, VARIO_UI_FONT_ALT1_VALUE, VARIO_UI_LOGISOSO_SLOT_SAMPLE);
    if (digit_slot_w <= 0)
    {
        vario_display_draw_text_box_top(u8g2,
                                        box_x,
                                        top_y,
                                        box_w,
                                        align,
                                        VARIO_UI_FONT_ALT1_VALUE,
                                        value_text);
        return;
    }

    total_w = (int16_t)((int16_t)slot_count * digit_slot_w);

    switch (align)
    {
        case VARIO_UI_ALIGN_LEFT:
            draw_x = box_x;
            break;

        case VARIO_UI_ALIGN_CENTER:
            draw_x = (int16_t)(box_x + ((box_w - total_w) / 2));
            break;

        case VARIO_UI_ALIGN_RIGHT:
        default:
            draw_x = (int16_t)(box_x + box_w - total_w);
            break;
    }

    if (draw_x < box_x)
    {
        draw_x = box_x;
    }

    /* ------------------------------------------------------------------ */
    /* slot 諛곗튂 洹쒖튃                                                       */
    /* - ?뚯닔 遺?몃뒗 ??긽 slot 0 ??怨좎젙                                    */
    /* - ?섎㉧吏 digit ? ??긽 ?ㅻⅨ履??뺣젹                                    */
    /* ------------------------------------------------------------------ */
    if (negative != false)
    {
        start_slot = (uint8_t)(slot_count - (uint8_t)digit_len);
        if (start_slot == 0u)
        {
            /* 蹂댄샇: -99999 媛숈? 遺덇???議고빀? formatter ?④퀎?먯꽌 ?ㅼ? ?딆?留? */
            /* ?뱀떆 ???sign slot ?섎굹瑜?蹂댁〈?쒕떎.                           */
            start_slot = 1u;
        }
    }
    else
    {
        start_slot = (uint8_t)(slot_count - (uint8_t)digit_len);
    }

    digit_ch[1] = '\0';

    u8g2_SetFontPosTop(u8g2);
    u8g2_SetFont(u8g2, VARIO_UI_FONT_ALT1_VALUE);

    if (negative != false)
    {
        digit_ch[0] = '-';
        u8g2_DrawStr(u8g2, draw_x, top_y, digit_ch);
    }

    for (i = 0u; i < digit_len; ++i)
    {
        digit_ch[0] = digits[i];
        u8g2_DrawStr(u8g2,
                     (int16_t)(draw_x + ((int16_t)(start_slot + i) * digit_slot_w)),
                     top_y,
                     digit_ch);
    }

    u8g2_SetFontPosBaseline(u8g2);
}

static void vario_display_draw_fixed_speed_value(u8g2_t *u8g2,
                                                 int16_t box_x,
                                                 int16_t box_y,
                                                 int16_t box_w,
                                                 int16_t box_h,
                                                 vario_ui_align_t align,
                                                 const char *value_text)
{
    char    sign[4];
    char    whole[16];
    char    frac[4];
    char    digit_ch[2];
    size_t  whole_len;
    int16_t digit_slot_w;
    int16_t frac_w;
    int16_t main_h;
    int16_t frac_h;
    int16_t whole_top;
    int16_t frac_top;
    int16_t draw_x;
    int16_t total_w;
    int16_t frac_x;
    uint8_t start_slot;
    uint8_t i;

    if ((u8g2 == NULL) || (value_text == NULL))
    {
        return;
    }

    memset(sign, 0, sizeof(sign));
    memset(whole, 0, sizeof(whole));
    memset(frac, 0, sizeof(frac));
    memset(digit_ch, 0, sizeof(digit_ch));

    vario_display_split_decimal_value(value_text,
                                      sign,
                                      sizeof(sign),
                                      whole,
                                      sizeof(whole),
                                      frac,
                                      sizeof(frac));

    if ((sign[0] != '\0') ||
        (vario_display_text_is_decimal_unsigned(whole, frac) == false) ||
        (strlen(whole) > VARIO_UI_SPEED_FIXED_SLOT_COUNT))
    {
        vario_display_draw_decimal_value(u8g2,
                                         box_x,
                                         box_y,
                                         box_w,
                                         box_h,
                                         align,
                                         VARIO_UI_FONT_BOTTOM_MAIN,
                                         VARIO_UI_FONT_BOTTOM_FRAC,
                                         value_text);
        return;
    }

    digit_slot_w = vario_display_measure_text(u8g2, VARIO_UI_FONT_BOTTOM_MAIN, VARIO_UI_LOGISOSO_SLOT_SAMPLE);
    frac_w = vario_display_measure_text(u8g2, VARIO_UI_FONT_BOTTOM_FRAC, frac);
    if ((digit_slot_w <= 0) || (frac_w <= 0))
    {
        vario_display_draw_decimal_value(u8g2,
                                         box_x,
                                         box_y,
                                         box_w,
                                         box_h,
                                         align,
                                         VARIO_UI_FONT_BOTTOM_MAIN,
                                         VARIO_UI_FONT_BOTTOM_FRAC,
                                         value_text);
        return;
    }

    total_w = (int16_t)((VARIO_UI_SPEED_FIXED_SLOT_COUNT * digit_slot_w) +
                        VARIO_UI_DECIMAL_FRAC_GAP_X + frac_w);
    switch (align)
    {
        case VARIO_UI_ALIGN_LEFT:
            draw_x = box_x;
            break;

        case VARIO_UI_ALIGN_CENTER:
            draw_x = (int16_t)(box_x + ((box_w - total_w) / 2));
            break;

        case VARIO_UI_ALIGN_RIGHT:
        default:
            draw_x = (int16_t)(box_x + box_w - total_w);
            break;
    }

    if (draw_x < box_x)
    {
        draw_x = box_x;
    }

    main_h = vario_display_get_font_height(u8g2, VARIO_UI_FONT_BOTTOM_MAIN);
    frac_h = vario_display_get_font_height(u8g2, VARIO_UI_FONT_BOTTOM_FRAC);
    if (main_h <= 0)
    {
        main_h = box_h;
    }
    if (frac_h <= 0)
    {
        frac_h = box_h;
    }

    whole_top = box_y;
    if (box_h > main_h)
    {
        whole_top = (int16_t)(box_y + ((box_h - main_h) / 2));
    }
    frac_top = (int16_t)(whole_top + VARIO_UI_DECIMAL_FRAC_TOP_BIAS_Y);
    frac_x = (int16_t)(draw_x + (VARIO_UI_SPEED_FIXED_SLOT_COUNT * digit_slot_w) + VARIO_UI_DECIMAL_FRAC_GAP_X);

    whole_len = strlen(whole);
    start_slot = (uint8_t)(VARIO_UI_SPEED_FIXED_SLOT_COUNT - (uint8_t)whole_len);
    digit_ch[1] = '\0';

    /* ------------------------------------------------------------------ */
    /* ???띾룄 ?レ옄???묒? 0.1 digit 怨쇱쓽 ?쒓컖 媛꾧꺽???섎━湲??꾪빐            */
    /* whole digit 履쎈쭔 2 px ?쇱そ?쇰줈 ?대룞?쒗궓??                           */
    /*                                                                    */
    /* ?먰븳 speed format ?④퀎?먯꽌 ?대? 999.9 濡?clamp ?덉쑝誘濡?              */
    /* 100~999 km/h ??3-digit whole ????fixed-slot 寃쎈줈瑜?洹몃?濡??꾨떎.  */
    /* ------------------------------------------------------------------ */
    if ((draw_x - VARIO_UI_BOTTOM_MAIN_TO_FRAC_EXTRA_GAP_PX) >= box_x)
    {
        draw_x = (int16_t)(draw_x - VARIO_UI_BOTTOM_MAIN_TO_FRAC_EXTRA_GAP_PX);
    }
    else
    {
        draw_x = box_x;
    }

    u8g2_SetFontPosTop(u8g2);
    u8g2_SetFont(u8g2, VARIO_UI_FONT_BOTTOM_MAIN);
    for (i = 0u; i < whole_len; ++i)
    {
        digit_ch[0] = whole[i];
        u8g2_DrawStr(u8g2,
                     (int16_t)(draw_x + ((int16_t)(start_slot + i) * digit_slot_w)),
                     whole_top,
                     digit_ch);
    }

    u8g2_SetFont(u8g2, VARIO_UI_FONT_BOTTOM_FRAC);
    u8g2_DrawStr(u8g2, frac_x, frac_top, frac);
    u8g2_SetFontPosBaseline(u8g2);
}

static void vario_display_draw_speed_inop_value(u8g2_t *u8g2,
                                                int16_t box_x,
                                                int16_t box_y,
                                                int16_t box_w,
                                                int16_t box_h,
                                                vario_ui_align_t align)
{
    int16_t digit_slot_w;
    int16_t frac_w;
    int16_t main_h;
    int16_t frac_h;
    int16_t total_w;
    int16_t draw_x;
    int16_t frac_base_x;
    int16_t whole_top;
    int16_t frac_top;
    int16_t frac_x;

    if (u8g2 == NULL)
    {
        return;
    }

    digit_slot_w = vario_display_measure_text(u8g2, VARIO_UI_FONT_BOTTOM_MAIN, VARIO_UI_LOGISOSO_SLOT_SAMPLE);
    frac_w = vario_display_measure_text(u8g2, VARIO_UI_FONT_BOTTOM_FRAC, "-");
    if ((digit_slot_w <= 0) || (frac_w <= 0))
    {
        vario_display_draw_decimal_value(u8g2,
                                         box_x,
                                         box_y,
                                         box_w,
                                         box_h,
                                         align,
                                         VARIO_UI_FONT_BOTTOM_MAIN,
                                         VARIO_UI_FONT_BOTTOM_FRAC,
                                         "--.-");
        return;
    }

    total_w = (int16_t)((VARIO_UI_SPEED_FIXED_SLOT_COUNT * digit_slot_w) +
                        VARIO_UI_DECIMAL_FRAC_GAP_X + frac_w);
    switch (align)
    {
        case VARIO_UI_ALIGN_LEFT:
            draw_x = box_x;
            break;

        case VARIO_UI_ALIGN_CENTER:
            draw_x = (int16_t)(box_x + ((box_w - total_w) / 2));
            break;

        case VARIO_UI_ALIGN_RIGHT:
        default:
            draw_x = (int16_t)(box_x + box_w - total_w);
            break;
    }

    frac_base_x = draw_x;
    if ((draw_x - VARIO_UI_BOTTOM_MAIN_TO_FRAC_EXTRA_GAP_PX) >= box_x)
    {
        draw_x = (int16_t)(draw_x - VARIO_UI_BOTTOM_MAIN_TO_FRAC_EXTRA_GAP_PX);
    }
    else if (draw_x < box_x)
    {
        draw_x = box_x;
    }

    main_h = vario_display_get_font_height(u8g2, VARIO_UI_FONT_BOTTOM_MAIN);
    frac_h = vario_display_get_font_height(u8g2, VARIO_UI_FONT_BOTTOM_FRAC);
    if (main_h <= 0)
    {
        main_h = box_h;
    }
    if (frac_h <= 0)
    {
        frac_h = box_h;
    }

    whole_top = box_y;
    if (box_h > main_h)
    {
        whole_top = (int16_t)(box_y + ((box_h - main_h) / 2));
    }
    frac_top = (int16_t)(whole_top + VARIO_UI_DECIMAL_FRAC_TOP_BIAS_Y);
    frac_x = (int16_t)(frac_base_x + (VARIO_UI_SPEED_FIXED_SLOT_COUNT * digit_slot_w) + VARIO_UI_DECIMAL_FRAC_GAP_X);

    u8g2_SetFontPosTop(u8g2);
    u8g2_SetFont(u8g2, VARIO_UI_FONT_BOTTOM_MAIN);
    u8g2_DrawStr(u8g2, (int16_t)(draw_x + digit_slot_w), whole_top, "-");
    u8g2_DrawStr(u8g2, (int16_t)(draw_x + (digit_slot_w * 2)), whole_top, "-");

    u8g2_SetFont(u8g2, VARIO_UI_FONT_BOTTOM_FRAC);
    u8g2_DrawStr(u8g2, frac_x, frac_top, "-");
    u8g2_SetFontPosBaseline(u8g2);
}

static void vario_display_draw_xbm(u8g2_t *u8g2,
                                   int16_t x,
                                   int16_t y,
                                   uint8_t w,
                                   uint8_t h,
                                   const unsigned char *bits)
{
    if ((u8g2 == NULL) || (bits == NULL))
    {
        return;
    }

    if ((x + (int16_t)w) <= 0)
    {
        return;
    }
    if ((y + (int16_t)h) <= 0)
    {
        return;
    }

    u8g2_DrawXBMP(u8g2, x, y, w, h, bits);
}

static void vario_display_draw_alt_badge(u8g2_t *u8g2, int16_t x, int16_t y_top, char digit_ch)
{
    if (u8g2 == NULL)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /* ALT1 / ALT2 / ALT3 icon resource draw                                   */
    /* - ?꾩씠肄?鍮꾪듃留듭? Vario_Icon_Resources.h ?먯꽌留?愿由ы븳??                */
    /* - ?ш린?쒕뒗 digit_ch 濡??대뼡 由ъ냼?ㅻ? 李띿쓣吏留?寃곗젙?쒕떎.                  */
    /* - ?꾩튂瑜???린怨??띠쑝硫?caller ??x / y_top 怨꾩궛?앸쭔 ?섏젙?섎㈃ ?쒕떎.       */
    /* ---------------------------------------------------------------------- */
    switch (digit_ch)
    {
        case '1':
            vario_display_draw_xbm(u8g2,
                                   x,
                                   y_top,
                                   VARIO_ICON_ALT1_WIDTH,
                                   VARIO_ICON_ALT1_HEIGHT,
                                   vario_icon_alt1_bits);
            break;

        case '2':
            vario_display_draw_xbm(u8g2,
                                   x,
                                   y_top,
                                   VARIO_ICON_ALT2_WIDTH,
                                   VARIO_ICON_ALT2_HEIGHT,
                                   vario_icon_alt2_bits);
            break;

        case '3':
            vario_display_draw_xbm(u8g2,
                                   x,
                                   y_top,
                                   VARIO_ICON_ALT3_WIDTH,
                                   VARIO_ICON_ALT3_HEIGHT,
                                   vario_icon_alt3_bits);
            break;

        default:
            break;
    }
}


static void vario_display_format_clock(char *buf,
                                       size_t buf_len,
                                       const vario_runtime_t *rt,
                                       const vario_settings_t *settings)
{
    uint8_t hour;
    char    suffix;

    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    if ((rt == NULL) || (rt->clock_valid == false))
    {
        snprintf(buf, buf_len, "--:--:--");
        return;
    }

    if ((settings != NULL) && (settings->time_format == VARIO_TIME_FORMAT_12H))
    {
        hour = (uint8_t)(rt->local_hour % 12u);
        if (hour == 0u)
        {
            hour = 12u;
        }

        suffix = (rt->local_hour < 12u) ? 'A' : 'P';
        snprintf(buf,
                 buf_len,
                 "%02u:%02u:%02u%c",
                 (unsigned)hour,
                 (unsigned)rt->local_min,
                 (unsigned)rt->local_sec,
                 suffix);
        return;
    }

    snprintf(buf,
             buf_len,
             "%02u:%02u:%02u",
             (unsigned)rt->local_hour,
             (unsigned)rt->local_min,
             (unsigned)rt->local_sec);
}

static void vario_display_format_flight_time(char *buf, size_t buf_len, const vario_runtime_t *rt)
{
    uint32_t total_s;
    uint32_t hh;
    uint32_t mm;
    uint32_t ss;

    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    total_s = (rt != NULL) ? rt->flight_time_s : 0u;
    hh = total_s / 3600u;
    mm = (total_s % 3600u) / 60u;
    ss = total_s % 60u;

    snprintf(buf,
             buf_len,
             "%02lu:%02lu:%02lu",
             (unsigned long)hh,
             (unsigned long)mm,
             (unsigned long)ss);
}

/* -------------------------------------------------------------------------- */
/* per-row altitude formatter                                                 */
/*                                                                            */
/* ALT1 / ALT2 / ALT3 媛 ?쒕줈 ?ㅻⅨ ?⑥쐞瑜?媛吏????덇쾶 ?섍린 ?꾪븳 helper ??      */
/* ?꾩옱 common shell ?먯꽌??                                                  */
/* - ALT1 : settings->altitude_unit                                            */
/* - ALT2 : settings->alt2_unit                                                */
/* - ALT3 : settings->altitude_unit                                            */
/* 議고빀???ъ슜?쒕떎.                                                            */
/* -------------------------------------------------------------------------- */

static int32_t vario_display_select_altitude_from_unit_bank(const app_altitude_linear_units_t *units,
                                                            vario_alt_unit_t unit)
{
    if (units == NULL)
    {
        return 0;
    }

    return (unit == VARIO_ALT_UNIT_FEET) ? units->feet_rounded : units->meters_rounded;
}

static void vario_display_format_altitude_from_unit_bank(char *buf,
                                                         size_t buf_len,
                                                         const app_altitude_linear_units_t *units,
                                                         vario_alt_unit_t unit)
{
    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    snprintf(buf,
             buf_len,
             "%ld",
             (long)vario_display_select_altitude_from_unit_bank(units, unit));
}

static long vario_display_get_flight_level_from_unit_bank(const vario_runtime_t *rt)
{
    float fl_value_ft;

    if (rt == NULL)
    {
        return 0;
    }

    fl_value_ft = ((float)rt->altitude.units.alt_pressure_std.feet_rounded) * 0.01f;
    if (fl_value_ft < 0.0f)
    {
        fl_value_ft = 0.0f;
    }

    return lroundf(fl_value_ft);
}

static const app_altitude_linear_units_t *vario_display_select_smart_fuse_units(const vario_runtime_t *rt)
{
    if (rt == NULL)
    {
        return NULL;
    }

    /* ---------------------------------------------------------------------- */
    /*  SMART FUSE                                                            */
    /*                                                                        */
    /*  Alt2??SMART FUSE???대? ?뚯씠???대쫫??吏곸젒 ?몄텧?섏? ?딄퀬,             */
    /*  ?꾩옱 low-level???쒓났?섎뒗 assisted absolute altitude 以?               */
    /*  媛???꾩????섎뒗 寃쎈줈瑜??먮룞 ?좏깮?댁꽌 蹂댁뿬 二쇰뒗 user-facing 紐⑤뱶??   */
    /*                                                                        */
    /*  ?곗꽑?쒖쐞                                                              */
    /*  1) IMU aided fused altitude                                            */
    /*  2) baro + GPS anchor fused altitude                                    */
    /*  3) baro媛 ?놁쑝硫?GPS altitude fallback                                 */
    /* ---------------------------------------------------------------------- */
    if ((rt->baro_valid != false) && (rt->altitude.imu_vector_valid != false))
    {
        return &rt->altitude.units.alt_fused_imu;
    }

    if (rt->baro_valid != false)
    {
        return &rt->altitude.units.alt_fused_noimu;
    }

    if (rt->gps_valid != false)
    {
        return &rt->altitude.units.alt_gps_hmsl;
    }

    return NULL;
}

static void vario_display_format_altitude_with_unit(char *buf,
                                                    size_t buf_len,
                                                    float altitude_m,
                                                    vario_alt_unit_t unit)
{
    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    snprintf(buf,
             buf_len,
             "%ld",
             (long)Vario_Settings_AltitudeMetersToDisplayRoundedWithUnit(altitude_m, unit));
}

static void vario_display_format_filtered_selected_altitude(char *buf,
                                                            size_t buf_len,
                                                            const vario_runtime_t *rt,
                                                            vario_alt_unit_t unit)
{
    long display_value;

    if ((buf == NULL) || (buf_len == 0u) || (rt == NULL))
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /* ??ALT1 ?レ옄??APP_STATE unit bank瑜?洹몃?濡??ㅼ떆 ?쎌? ?딄퀬,             */
    /* Vario_State媛 5Hz cadence濡?publish??"?쒖떆 ?꾩슜 怨좊룄 ?レ옄"瑜??대떎.     */
    /*                                                                        */
    /* ?댁쑀                                                                   */
    /* - low-level source selection? 洹몃?濡?議댁쨷?쒕떎.                         */
    /* - ?섏?留??붾㈃??蹂댁씠?????レ옄留뚯? upper layer???⑥닚 ?꾪꽣/hysteresis   */
    /*   寃곌낵瑜??ъ슜?댁꽌 怨쇰룄???⑤┝ ?놁씠 ?쎄린 ?쎄쾶 留뚮뱺??                    */
    /* - feet/meter ?섏궛? 湲곗〈 helper瑜?洹몃?濡??ъ슜?쒕떎.                      */
    /*                                                                        */
    /* 異붽? 怨꾩빟                                                               */
    /* - ALT1 ???レ옄???댁쟾???ъ슜?먭? 吏?뺥븳 怨좊룄 ?쒓린 踰붿쐞瑜?洹몃?濡??곕Ⅸ?? */
    /* - 利?-9999 ~ 99999 踰붿쐞?먯꽌留??쒖떆?섎ŉ,                                 */
    /*   ?뚯닔???뚮뒗 sign slot 怨좎젙 寃쎈줈媛 ?숈옉?섎룄濡?臾몄옄???먯껜瑜?            */
    /*   5-column signed format ?쇰줈 留뚮뱺??                                   */
    /* ---------------------------------------------------------------------- */
    display_value = (long)Vario_Settings_AltitudeMetersToDisplayRoundedWithUnit(rt->baro_altitude_m, unit);

    if (display_value > VARIO_UI_ALT1_SIGNED_MAX_DISPLAY)
    {
        display_value = VARIO_UI_ALT1_SIGNED_MAX_DISPLAY;
    }
    if (display_value < VARIO_UI_ALT1_SIGNED_MIN_DISPLAY)
    {
        display_value = VARIO_UI_ALT1_SIGNED_MIN_DISPLAY;
    }

    if (display_value < 0L)
    {
        snprintf(buf, buf_len, "-%4ld", (long)(-display_value));
    }
    else
    {
        snprintf(buf, buf_len, "%5ld", display_value);
    }
}

static void vario_display_format_speed_value(char *buf, size_t buf_len, float speed_kmh)
{
    float display_value;

    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    display_value = Vario_Settings_SpeedKmhToDisplayFloat(speed_kmh);

    /* ------------------------------------------------------------------ */
    /* speed big/small numeric contract                                    */
    /*                                                                    */
    /* ?ъ슜?먭? speed ???レ옄 fixed-slot ??999.9 源뚯? 蹂댁옣???щ씪怨??덉쑝誘濡?*/
    /* format ?④퀎?먯꽌 癒쇱? 踰붿쐞瑜?clamp ?쒕떎.                              */
    /* ?대젃寃??섎㈃ 3?먮━ whole digit(100~999) ????긽 媛숈? renderer 寃쎈줈瑜? */
    /* ?꾨떎.                                                                */
    /* ------------------------------------------------------------------ */
    display_value = vario_display_clampf(display_value, 0.0f, 999.9f);
    snprintf(buf, buf_len, "%.1f", (double)display_value);
}

static void vario_display_format_vario_value_abs(char *buf, size_t buf_len, float vario_mps)
{
    float display_value;

    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    display_value = Vario_Settings_VSpeedMpsToDisplayFloat(vario_display_absf(vario_mps));
    if (Vario_Settings_Get()->vspeed_unit == VARIO_VSPEED_UNIT_FPM)
    {
        snprintf(buf, buf_len, "%ld", (long)lroundf(display_value));
    }
    else
    {
        snprintf(buf, buf_len, "%.1f", (double)display_value);
    }
}

static void vario_display_format_speed_small(char *buf, size_t buf_len, float speed_kmh)
{
    float display_value;

    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    display_value = Vario_Settings_SpeedKmhToDisplayFloat(speed_kmh);

    /* ------------------------------------------------------------------ */
    /* speed big/small numeric contract                                    */
    /*                                                                    */
    /* ?ъ슜?먭? speed ???レ옄 fixed-slot ??999.9 源뚯? 蹂댁옣???щ씪怨??덉쑝誘濡?*/
    /* format ?④퀎?먯꽌 癒쇱? 踰붿쐞瑜?clamp ?쒕떎.                              */
    /* ?대젃寃??섎㈃ 3?먮━ whole digit(100~999) ????긽 媛숈? renderer 寃쎈줈瑜? */
    /* ?꾨떎.                                                                */
    /* ------------------------------------------------------------------ */
    display_value = vario_display_clampf(display_value, 0.0f, 999.9f);
    snprintf(buf, buf_len, "%.1f", (double)display_value);
}

static void vario_display_format_vario_small_abs(char *buf, size_t buf_len, float vario_mps)
{
    float display_value;

    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    display_value = Vario_Settings_VSpeedMpsToDisplayFloat(vario_display_absf(vario_mps));
    if (Vario_Settings_Get()->vspeed_unit == VARIO_VSPEED_UNIT_FPM)
    {
        snprintf(buf, buf_len, "%ld", (long)lroundf(display_value));
    }
    else
    {
        snprintf(buf, buf_len, "%.1f", (double)display_value);
    }
}

static void vario_display_format_peak_speed(char *buf, size_t buf_len, float speed_kmh)
{
    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    if (speed_kmh <= 0.05f)
    {
        snprintf(buf, buf_len, "--.-");
        return;
    }

    vario_display_format_speed_small(buf, buf_len, speed_kmh);
    vario_display_trim_leading_zero(buf);
}

static void vario_display_format_peak_vario(char *buf, size_t buf_len, float vario_mps)
{
    float display_value;

    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    display_value = Vario_Settings_VSpeedMpsToDisplayFloat(vario_display_absf(vario_mps));
    if (display_value < 0.0f)
    {
        display_value = 0.0f;
    }

    if (Vario_Settings_Get()->vspeed_unit == VARIO_VSPEED_UNIT_FPM)
    {
        snprintf(buf, buf_len, "%4ld", (long)lroundf(display_value));
    }
    else
    {
        snprintf(buf,
                 buf_len,
                 "%4.1f",
                 (double)vario_display_clampf(display_value, 0.0f, 99.9f));
    }
}

static void vario_display_format_glide_ratio(char *buf, size_t buf_len, const vario_runtime_t *rt)
{
    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    if ((rt == NULL) || (rt->glide_ratio_slow_valid == false))
    {
        snprintf(buf, buf_len, "--.-");
        return;
    }

    snprintf(buf,
             buf_len,
             "%.1f",
             (double)vario_display_clampf(rt->glide_ratio_slow, 0.0f, 99.9f));
}

static void vario_display_format_glide_ratio_value(char *buf,
                                                   size_t buf_len,
                                                   bool valid,
                                                   float glide_ratio)
{
    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    if (valid == false)
    {
        snprintf(buf, buf_len, "--.-");
        return;
    }

    snprintf(buf,
             buf_len,
             "%.1f",
             (double)vario_display_clampf(glide_ratio, 0.0f, 99.9f));
}

static void vario_display_format_nav_distance(char *buf,
                                              size_t buf_len,
                                              const char *label,
                                              bool valid,
                                              float distance_m)
{
    float display_distance;

    if ((buf == NULL) || (buf_len == 0u) || (label == NULL))
    {
        return;
    }

    if (valid == false)
    {
        snprintf(buf,
                 buf_len,
                 "%s ---.-%s",
                 label,
                 Vario_Settings_GetNavDistanceUnitText());
        return;
    }

    display_distance = Vario_Settings_NavDistanceMetersToDisplayFloat(distance_m);
    display_distance = vario_display_clampf(display_distance, 0.0f, 999.9f);

    snprintf(buf,
             buf_len,
             "%s %.1f%s",
             label,
             (double)display_distance,
             Vario_Settings_GetNavDistanceUnitText());
}


static bool vario_display_recent_average(const float *samples,
                                         const uint32_t *stamps,
                                         uint16_t count,
                                         uint16_t head,
                                         uint32_t latest_stamp_ms,
                                         uint32_t window_ms,
                                         float fallback_value,
                                         float *out_average)
{
    uint16_t used;
    uint16_t i;
    float   sum;

    if (out_average == NULL)
    {
        return false;
    }

    if ((samples == NULL) || (stamps == NULL) || (count == 0u) || (latest_stamp_ms == 0u))
    {
        *out_average = fallback_value;
        return false;
    }

    used = 0u;
    sum  = 0.0f;

    for (i = 0u; i < count; ++i)
    {
        uint16_t idx;
        uint16_t reverse_index;

        reverse_index = (uint16_t)(count - 1u - i);
        idx = (uint16_t)((head + VARIO_UI_AVG_BUFFER_SIZE - 1u - reverse_index) % VARIO_UI_AVG_BUFFER_SIZE);

        if ((latest_stamp_ms - stamps[idx]) > window_ms)
        {
            continue;
        }

        sum += samples[idx];
        ++used;
    }

    if (used == 0u)
    {
        *out_average = fallback_value;
        return false;
    }

    *out_average = sum / (float)used;
    return true;
}

static void vario_display_reset_sample_buffer_only(void)
{
    memset(s_vario_ui_dynamic.sample_stamp_ms, 0, sizeof(s_vario_ui_dynamic.sample_stamp_ms));
    memset(s_vario_ui_dynamic.vario_mps, 0, sizeof(s_vario_ui_dynamic.vario_mps));
    memset(s_vario_ui_dynamic.speed_kmh, 0, sizeof(s_vario_ui_dynamic.speed_kmh));
    s_vario_ui_dynamic.head = 0u;
    s_vario_ui_dynamic.count = 0u;
    s_vario_ui_dynamic.last_publish_ms = 0u;
    s_vario_ui_dynamic.top_speed_kmh = 0.0f;
    s_vario_ui_dynamic.displayed_speed_valid = false;
    s_vario_ui_dynamic.displayed_speed_kmh = 0.0f;
    s_vario_ui_dynamic.last_display_smoothing_ms = 0u;
    s_vario_ui_dynamic.displayed_glide_valid = false;
    s_vario_ui_dynamic.displayed_glide_ratio = 0.0f;
    s_vario_ui_dynamic.last_displayed_glide_update_ms = 0u;
    s_vario_ui_dynamic.fast_average_glide_valid = false;
    s_vario_ui_dynamic.fast_average_glide_ratio = 0.0f;
    s_vario_ui_dynamic.accumulated_flight_distance_m = 0.0f;
    s_vario_ui_dynamic.last_distance_sample_ms = 0u;
    s_vario_ui_dynamic.last_distance_speed_kmh = 0.0f;
}

static void vario_display_update_dynamic_metrics(const vario_runtime_t *rt,
                                                 const vario_settings_t *settings)
{
    uint32_t sample_ms;
    uint16_t idx;

    (void)settings;

    if (rt == NULL)
    {
        return;
    }

    if (((rt->flight_time_s == 0u) && (rt->trail_count == 0u) && (s_vario_ui_dynamic.last_flight_start_ms != 0u)) ||
        (rt->flight_start_ms != s_vario_ui_dynamic.last_flight_start_ms))
    {
        s_vario_ui_dynamic.last_flight_start_ms = rt->flight_start_ms;
        vario_display_reset_sample_buffer_only();
    }

    sample_ms = (rt->last_task_ms != 0u) ? rt->last_task_ms : rt->last_publish_ms;
    if ((sample_ms == 0u) || (sample_ms == s_vario_ui_dynamic.last_publish_ms))
    {
        goto update_peak_only;
    }

    idx = s_vario_ui_dynamic.head;
    s_vario_ui_dynamic.sample_stamp_ms[idx] = sample_ms;
    s_vario_ui_dynamic.vario_mps[idx] = rt->fast_vario_bar_mps;
    s_vario_ui_dynamic.speed_kmh[idx] = rt->gs_bar_speed_kmh;

    s_vario_ui_dynamic.head = (uint16_t)((idx + 1u) % VARIO_UI_AVG_BUFFER_SIZE);
    if (s_vario_ui_dynamic.count < VARIO_UI_AVG_BUFFER_SIZE)
    {
        ++s_vario_ui_dynamic.count;
    }

    s_vario_ui_dynamic.last_publish_ms = sample_ms;

update_peak_only:
    if ((rt->flight_active != false) || (rt->flight_time_s > 0u) || (rt->trail_count > 0u))
    {
        /* ------------------------------------------------------------------ */
        /* display-local flight distance integration                           */
        /*                                                                    */
        /* 醫뚰븯??target-less slot ?먯꽌 "FLT" 媛믪쓣 蹂댁뿬 二쇨린 ?꾪빐,         */
        /* GPS usable ?쒖젏??ground speed 瑜?publish cadence 湲곗??쇰줈 ?곷텇?쒕떎.*/
        /*                                                                    */
        /* ?ㅺ퀎 ?먯튃                                                           */
        /* - high-level display ??runtime ??留뚮뱺 gps usability gate 留?蹂몃떎.*/
        /* - GPS fix 媛 ?딄린硫??곷텇 ?쒓퀎?댁쓣 ?딆뼱, ?섏쨷??蹂듦??덉쓣 ??         */
        /*   gap ?꾩껜瑜???踰덉뿉 癒뱀? ?딄쾶 ?쒕떎.                               */
        /* - ?곷텇? trapezoid 諛⑹떇?쇰줈 ?댁쟾/?꾩옱 speed ?됯퇏???ъ슜?쒕떎.        */
        /* - ?쒖떆 ??clamp ??draw ?④퀎?먯꽌留??섑뻾?섍퀬, ?대? ?꾩쟻媛믪?          */
        /*   raw meter float 濡??좎??쒕떎.                                      */
        /* ------------------------------------------------------------------ */
        if ((sample_ms != 0u) && (vario_display_runtime_has_usable_gps_fix(rt) != false))
        {
            float current_speed_kmh;

            current_speed_kmh = rt->ground_speed_kmh;
            if (current_speed_kmh < 0.0f)
            {
                current_speed_kmh = 0.0f;
            }

            if ((s_vario_ui_dynamic.last_distance_sample_ms != 0u) &&
                (sample_ms > s_vario_ui_dynamic.last_distance_sample_ms))
            {
                uint32_t dt_ms;
                float    avg_speed_kmh;

                dt_ms = sample_ms - s_vario_ui_dynamic.last_distance_sample_ms;
                avg_speed_kmh = (s_vario_ui_dynamic.last_distance_speed_kmh + current_speed_kmh) * 0.5f;

                /* km/h * ms / 3600 = meters */
                s_vario_ui_dynamic.accumulated_flight_distance_m +=
                    avg_speed_kmh * ((float)dt_ms / 3600.0f);
            }

            s_vario_ui_dynamic.last_distance_sample_ms = sample_ms;
            s_vario_ui_dynamic.last_distance_speed_kmh = current_speed_kmh;
        }
        else
        {
            /* -------------------------------------------------------------- */
            /* GPS usable ?섏? ?딆? 援ш컙? distance integration ???딅뒗??     */
            /* ?ㅼ쓬 valid sample ?먯꽌??洹??쒖젏遺???ㅼ떆 ?댁뼱 遺숈씤??         */
            /* -------------------------------------------------------------- */
            s_vario_ui_dynamic.last_distance_sample_ms = 0u;
            s_vario_ui_dynamic.last_distance_speed_kmh = 0.0f;
        }

        if (rt->gs_bar_speed_kmh > s_vario_ui_dynamic.top_speed_kmh)
        {
            s_vario_ui_dynamic.top_speed_kmh = rt->gs_bar_speed_kmh;
        }
    }
    else
    {
        s_vario_ui_dynamic.last_distance_sample_ms = 0u;
        s_vario_ui_dynamic.last_distance_speed_kmh = 0.0f;
    }
}

static void vario_display_get_average_values(const vario_runtime_t *rt,
                                             const vario_settings_t *settings,
                                             float *out_avg_vario_mps,
                                             float *out_avg_speed_kmh)
{
    uint32_t window_ms;

    if ((out_avg_vario_mps == NULL) || (out_avg_speed_kmh == NULL))
    {
        return;
    }

    if ((rt == NULL) || (settings == NULL))
    {
        *out_avg_vario_mps = 0.0f;
        *out_avg_speed_kmh = 0.0f;
        return;
    }

    window_ms = ((uint32_t)settings->digital_vario_average_seconds) * 1000u;
    if (window_ms == 0u)
    {
        window_ms = 1000u;
    }

    vario_display_recent_average(s_vario_ui_dynamic.vario_mps,
                                 s_vario_ui_dynamic.sample_stamp_ms,
                                 s_vario_ui_dynamic.count,
                                 s_vario_ui_dynamic.head,
                                 s_vario_ui_dynamic.last_publish_ms,
                                 window_ms,
                                 rt->fast_vario_bar_mps,
                                 out_avg_vario_mps);

    vario_display_recent_average(s_vario_ui_dynamic.speed_kmh,
                                 s_vario_ui_dynamic.sample_stamp_ms,
                                 s_vario_ui_dynamic.count,
                                 s_vario_ui_dynamic.head,
                                 s_vario_ui_dynamic.last_publish_ms,
                                 window_ms,
                                 rt->gs_bar_speed_kmh,
                                 out_avg_speed_kmh);
}


static bool vario_display_compute_glide_ratio(float speed_kmh,
                                              float vario_mps,
                                              float *out_ratio)
{
    float sink_mps;
    float glide_ratio;

    if (out_ratio == NULL)
    {
        return false;
    }

    sink_mps = -vario_mps;
    if ((sink_mps <= 0.15f) || (speed_kmh <= 1.0f))
    {
        *out_ratio = 0.0f;
        return false;
    }

    glide_ratio = (speed_kmh / 3.6f) / sink_mps;
    *out_ratio = vario_display_clampf(glide_ratio, 0.0f, 99.9f);
    return true;
}

static void vario_display_update_slow_number_caches(const vario_runtime_t *rt)
{
    uint32_t now_ms;
    float    target_speed_kmh;

    if (rt == NULL)
    {
        return;
    }

    now_ms = (rt->last_task_ms != 0u) ? rt->last_task_ms : rt->last_publish_ms;

    /* ------------------------------------------------------------------ */
    /* GPS usable ?щ?瑜?display 怨꾩링?먯꽌???ㅼ떆 蹂몃떎.                     */
    /*                                                                    */
    /* ?댁쑀                                                               */
    /* - trainer 醫낅즺 吏곹썑??GPS ?ы쉷??寃쎄퀎?먯꽌 stale GS ?レ옄媛          */
    /*   ??踰????⑤뒗 寃껋쓣 留됰뒗??                                       */
    /* - usable fix 媛 ?꾨땲硫???GS ?レ옄 latch ?먯껜瑜?invalid 濡??뚮젮     */
    /*   ?뚮뜑?ш? "--.-" / dither lane ??洹몃━寃?留뚮뱺??                 */
    /* ------------------------------------------------------------------ */
    if (vario_display_runtime_has_usable_gps_fix(rt) == false)
    {
        s_vario_ui_dynamic.displayed_speed_valid = false;
        s_vario_ui_dynamic.displayed_speed_kmh = 0.0f;
        s_vario_ui_dynamic.last_display_smoothing_ms = 0u;
    }
    else
    {
        target_speed_kmh = rt->filtered_ground_speed_kmh;
        if (target_speed_kmh < 0.0f)
        {
            target_speed_kmh = 0.0f;
        }

        /* ------------------------------------------------------------------ */
        /* ??GS ?レ옄留??먮━寃?蹂댁씠寃?留뚮뱺??                                  */
        /*                                                                    */
        /* - source : Vario_State 媛 10 Hz GPS ?섑뵆濡?留뚮뱺 filtered GS        */
        /* - cadence: display layer ?먯꽌??1珥덉뿉 1踰덈쭔 latch ?쒕떎.            */
        /* - trainer mode ??媛숈? runtime.filtered_ground_speed_kmh 寃쎈줈瑜?  */
        /*   ?ъ슜?섎?濡?蹂꾨룄 遺꾧린 ?놁씠 trainer synthetic speed 媛 ?쒖떆?쒕떎.   */
        /* ------------------------------------------------------------------ */
        if ((s_vario_ui_dynamic.displayed_speed_valid == false) ||
            (s_vario_ui_dynamic.last_display_smoothing_ms == 0u))
        {
            s_vario_ui_dynamic.displayed_speed_valid = true;
            s_vario_ui_dynamic.displayed_speed_kmh = target_speed_kmh;
            s_vario_ui_dynamic.last_display_smoothing_ms = now_ms;
        }
        else if ((now_ms - s_vario_ui_dynamic.last_display_smoothing_ms) >= 1000u)
        {
            s_vario_ui_dynamic.displayed_speed_kmh = target_speed_kmh;
            s_vario_ui_dynamic.last_display_smoothing_ms = now_ms;
        }
    }

    if (s_vario_ui_dynamic.fast_average_glide_valid == false)
    {
        s_vario_ui_dynamic.displayed_glide_valid = false;
        return;
    }

    if ((s_vario_ui_dynamic.displayed_glide_valid == false) ||
        (s_vario_ui_dynamic.last_displayed_glide_update_ms == 0u))
    {
        s_vario_ui_dynamic.displayed_glide_valid = true;
        s_vario_ui_dynamic.displayed_glide_ratio = s_vario_ui_dynamic.fast_average_glide_ratio;
        s_vario_ui_dynamic.last_displayed_glide_update_ms = now_ms;
        return;
    }

    if ((now_ms - s_vario_ui_dynamic.last_displayed_glide_update_ms) >= VARIO_UI_SLOW_GLIDE_UPDATE_MS)
    {
        s_vario_ui_dynamic.displayed_glide_ratio = s_vario_ui_dynamic.fast_average_glide_ratio;
        s_vario_ui_dynamic.last_displayed_glide_update_ms = now_ms;
    }
}


static void vario_display_get_bar_display_values(const vario_runtime_t *rt,
                                                 float average_vario_mps,
                                                 float average_speed_kmh,
                                                 float *out_vario_bar_mps,
                                                 float *out_avg_vario_mps,
                                                 float *out_gs_bar_kmh,
                                                 float *out_avg_speed_kmh)
{
    const vario_settings_t *settings;
    float                   vario_scale_mps;
    float                   vario_bar_limit_mps;
    float                   gs_bar_limit_kmh;
    float                   target_vario_mps;
    float                   target_gs_kmh;
    uint32_t                now_ms;
    float                   dt_s;
    float                   alpha;

    if ((out_vario_bar_mps == NULL) || (out_avg_vario_mps == NULL) ||
        (out_gs_bar_kmh == NULL) || (out_avg_speed_kmh == NULL))
    {
        return;
    }

    *out_vario_bar_mps = 0.0f;
    *out_avg_vario_mps = average_vario_mps;
    *out_gs_bar_kmh = 0.0f;
    *out_avg_speed_kmh = average_speed_kmh;

    if (rt == NULL)
    {
        return;
    }

    settings = Vario_Settings_Get();
    vario_scale_mps = vario_display_get_vario_scale_mps(settings);
    gs_bar_limit_kmh = vario_display_get_gs_scale_kmh(settings);

    /* ---------------------------------------------------------------------- */
    /*  over-range ??젣 ?⑦꽩???쒖떆?섎젮硫?                                      */
    /*  醫뚯륫 VARIO bar ?낅젰??full-scale ???쎄컙 ?섏쓣 ???덉뼱???쒕떎.          */
    /*                                                                        */
    /*  ?곕씪??renderer ?낅젰 clamp ??                                        */
    /*  - vario : ?꾩옱 scale ??2諛곌퉴吏                                        */
    /*  - GS    : ?꾩옱 ?ㅼ젙 top speed 源뚯?                                     */
    /*  濡?留욎텣??                                                             */
    /* ---------------------------------------------------------------------- */
    vario_bar_limit_mps = vario_scale_mps * 2.0f;
    if (vario_bar_limit_mps < VARIO_UI_VARIO_MAJOR_STEP_MPS)
    {
        vario_bar_limit_mps = VARIO_UI_VARIO_MAJOR_STEP_MPS;
    }

    target_vario_mps = vario_display_clampf(rt->fast_vario_bar_mps,
                                            -vario_bar_limit_mps,
                                            vario_bar_limit_mps);
    target_gs_kmh = vario_display_clampf(rt->gs_bar_speed_kmh,
                                         0.0f,
                                         gs_bar_limit_kmh);
    now_ms = (rt->last_task_ms != 0u) ? rt->last_task_ms : rt->last_publish_ms;

    /* ---------------------------------------------------------------------- */
    /* 醫뚯륫 VARIO bar??display layer?먯꽌 異붽? ?꾪꽣瑜??ｌ? ?딅뒗??            */
    /*                                                                        */
    /* ?곗륫 GS bar ??떆 raw path 瑜?洹몃?濡??대떎.                             */
    /* - ??GS ?レ옄留?1珥?cadence / filtered path 瑜??怨?                    */
    /* - ??洹몃옒??bar ???좉쾬 ?띾룄瑜?利됱떆 蹂댁뿬 以??                        */
    /* ---------------------------------------------------------------------- */
    *out_vario_bar_mps = target_vario_mps;
    *out_avg_vario_mps = average_vario_mps;
    *out_gs_bar_kmh = target_gs_kmh;
    *out_avg_speed_kmh = average_speed_kmh;

    (void)now_ms;
    (void)dt_s;
    (void)alpha;
}

static bool vario_display_get_oldest_trail_point(const vario_runtime_t *rt,
                                                 int32_t *out_lat_e7,
                                                 int32_t *out_lon_e7)
{
    uint8_t oldest_index;

    if ((rt == NULL) || (out_lat_e7 == NULL) || (out_lon_e7 == NULL) || (rt->trail_count == 0u))
    {
        return false;
    }

    oldest_index = (uint8_t)((rt->trail_head + VARIO_TRAIL_MAX_POINTS - rt->trail_count) % VARIO_TRAIL_MAX_POINTS);
    *out_lat_e7 = rt->trail_lat_e7[oldest_index];
    *out_lon_e7 = rt->trail_lon_e7[oldest_index];
    return true;
}

static bool vario_display_distance_bearing(int32_t from_lat_e7,
                                           int32_t from_lon_e7,
                                           int32_t to_lat_e7,
                                           int32_t to_lon_e7,
                                           float *out_distance_m,
                                           float *out_bearing_deg)
{
    double lat1;
    double lat2;
    double lon1;
    double lon2;
    double dlat;
    double dlon;
    double sin_dlat;
    double sin_dlon;
    double a;
    double c;
    double y;
    double x;
    double bearing_deg;

    if ((out_distance_m == NULL) || (out_bearing_deg == NULL))
    {
        return false;
    }

    lat1 = ((double)from_lat_e7) * 1.0e-7;
    lat2 = ((double)to_lat_e7) * 1.0e-7;
    lon1 = ((double)from_lon_e7) * 1.0e-7;
    lon2 = ((double)to_lon_e7) * 1.0e-7;

    lat1 = (double)vario_display_deg_to_rad((float)lat1);
    lat2 = (double)vario_display_deg_to_rad((float)lat2);
    lon1 = (double)vario_display_deg_to_rad((float)lon1);
    lon2 = (double)vario_display_deg_to_rad((float)lon2);

    dlat = lat2 - lat1;
    dlon = lon2 - lon1;

    sin_dlat = sin(dlat * 0.5);
    sin_dlon = sin(dlon * 0.5);
    a = (sin_dlat * sin_dlat) + (cos(lat1) * cos(lat2) * sin_dlon * sin_dlon);
    if (a > 1.0)
    {
        a = 1.0;
    }

    c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    *out_distance_m = (float)(6371000.0 * c);

    y = sin(dlon) * cos(lat2);
    x = (cos(lat1) * sin(lat2)) - (sin(lat1) * cos(lat2) * cos(dlon));
    bearing_deg = vario_display_rad_to_deg((float)atan2(y, x));
    *out_bearing_deg = vario_display_wrap_360((float)bearing_deg);
    return true;
}

static void vario_display_compute_nav_solution(const vario_runtime_t *rt,
                                               vario_display_nav_solution_t *out_solution)
{
    if (out_solution == NULL)
    {
        return;
    }

    memset(out_solution, 0, sizeof(*out_solution));
    snprintf(out_solution->label, sizeof(out_solution->label), "DST");

    if (rt == NULL)
    {
        return;
    }

    out_solution->current_valid = ((rt->gps_valid != false) &&
                                   (rt->gps.fix.valid != false) &&
                                   (rt->gps.fix.fixOk != false) &&
                                   (rt->gps.fix.fixType != 0u));
    out_solution->heading_valid = (rt->heading_valid != false);

    if ((out_solution->current_valid == false) || (rt->target_valid == false))
    {
        return;
    }

    out_solution->target_valid = true;
    out_solution->distance_m = (rt->target_distance_m >= 0.0f) ? rt->target_distance_m : 0.0f;
    out_solution->bearing_deg = vario_display_wrap_360(rt->target_bearing_deg);

    if (rt->target_name[0] != '\0')
    {
        snprintf(out_solution->label, sizeof(out_solution->label), "%s", rt->target_name);
    }
    else
    {
        snprintf(out_solution->label,
                 sizeof(out_solution->label),
                 "%s",
                 Vario_Navigation_GetShortSourceLabel(rt->target_kind));
    }

    if (out_solution->heading_valid != false)
    {
        out_solution->relative_bearing_deg =
            vario_display_wrap_pm180(out_solution->bearing_deg - rt->heading_deg);
    }
    else
    {
        out_solution->relative_bearing_deg = out_solution->bearing_deg;
    }
}

static uint8_t vario_display_get_vario_scale_x10(const vario_settings_t *settings)
{
    uint8_t scale_x10;

    /* ---------------------------------------------------------------------- */
    /*  醫뚯륫 VARIO gauge ???꾩옱 4.0 / 5.0 ??媛믩쭔 吏?먰븳??                  */
    /*                                                                        */
    /*  ??λ맂 媛믪씠 ??firmware ???붿쟻?쇰줈 ?ㅻⅨ 媛믪씠?대룄                    */
    /*  draw layer ?먯꽌??4.0 ?먮뒗 5.0 怨꾩빟?쇰줈 snap ?댁꽌 ?ъ슜?쒕떎.           */
    /* ---------------------------------------------------------------------- */
    scale_x10 = VARIO_UI_VARIO_SCALE_MAX_X10;
    if (settings != NULL)
    {
        scale_x10 = settings->vario_range_mps_x10;
    }

    if (scale_x10 <= VARIO_UI_VARIO_SCALE_MIN_X10)
    {
        return VARIO_UI_VARIO_SCALE_MIN_X10;
    }

    return VARIO_UI_VARIO_SCALE_MAX_X10;
}

static float vario_display_get_vario_scale_mps(const vario_settings_t *settings)
{
    return ((float)vario_display_get_vario_scale_x10(settings)) * 0.1f;
}

static uint8_t vario_display_get_vario_halfstep_count(const vario_settings_t *settings)
{
    return (uint8_t)(vario_display_get_vario_scale_x10(settings) / 5u);
}

static float vario_display_get_gs_scale_kmh(const vario_settings_t *settings)
{
    float gs_top_kmh;

    gs_top_kmh = 80.0f;
    if (settings != NULL)
    {
        gs_top_kmh = (float)settings->gs_range_kmh;
    }

    return vario_display_clampf(gs_top_kmh,
                                VARIO_UI_GS_SCALE_MIN_KMH,
                                VARIO_UI_GS_SCALE_MAX_KMH);
}

static float vario_display_get_gs_minor_step_kmh(float gs_top_kmh)
{
    return (gs_top_kmh < VARIO_UI_GS_LOW_RANGE_SWITCH_KMH) ?
        VARIO_UI_GS_LOW_MINOR_STEP_KMH :
        VARIO_UI_GS_HIGH_MINOR_STEP_KMH;
}

static float vario_display_get_gs_major_step_kmh(float gs_top_kmh)
{
    return (gs_top_kmh < VARIO_UI_GS_LOW_RANGE_SWITCH_KMH) ?
        VARIO_UI_GS_LOW_MAJOR_STEP_KMH :
        VARIO_UI_GS_HIGH_MAJOR_STEP_KMH;
}

static void vario_display_get_vario_screen_geometry(int16_t *out_top_limit_y,
                                                    int16_t *out_zero_top_y,
                                                    int16_t *out_zero_bottom_y,
                                                    int16_t *out_bottom_limit_y,
                                                    int16_t *out_top_span_px,
                                                    int16_t *out_bottom_span_px)
{
    int16_t center_y;
    int16_t top_limit_y;
    int16_t zero_top_y;
    int16_t zero_bottom_y;
    int16_t bottom_limit_y;
    int16_t top_span_px;
    int16_t bottom_span_px;

    center_y = (int16_t)(VARIO_LCD_H / 2);
    top_limit_y = 0;
    zero_top_y = (int16_t)(center_y - 1);
    zero_bottom_y = (int16_t)(center_y + 1);
    bottom_limit_y = (int16_t)(VARIO_LCD_H - 1);
    top_span_px = (int16_t)(zero_top_y - top_limit_y);
    bottom_span_px = (int16_t)(bottom_limit_y - zero_bottom_y);

    if (top_span_px < 0)
    {
        top_span_px = 0;
    }

    if (bottom_span_px < 0)
    {
        bottom_span_px = 0;
    }

    if (out_top_limit_y != NULL)
    {
        *out_top_limit_y = top_limit_y;
    }
    if (out_zero_top_y != NULL)
    {
        *out_zero_top_y = zero_top_y;
    }
    if (out_zero_bottom_y != NULL)
    {
        *out_zero_bottom_y = zero_bottom_y;
    }
    if (out_bottom_limit_y != NULL)
    {
        *out_bottom_limit_y = bottom_limit_y;
    }
    if (out_top_span_px != NULL)
    {
        *out_top_span_px = top_span_px;
    }
    if (out_bottom_span_px != NULL)
    {
        *out_bottom_span_px = bottom_span_px;
    }
}

static int16_t vario_display_scale_value_to_fill_px(float abs_value,
                                                    float full_scale,
                                                    int16_t span_px)
{
    int16_t fill_px;

    if ((abs_value <= 0.0f) || (full_scale <= 0.0f) || (span_px <= 0))
    {
        return 0;
    }

    fill_px = (int16_t)lroundf((abs_value / full_scale) * (float)span_px);
    if ((abs_value > 0.0f) && (fill_px <= 0))
    {
        fill_px = 1;
    }
    if (fill_px > span_px)
    {
        fill_px = span_px;
    }

    return fill_px;
}

static void vario_display_get_vario_slot_rect(const vario_settings_t *settings,
                                              bool positive,
                                              uint8_t level_from_center,
                                              int16_t *out_y,
                                              int16_t *out_h)
{
    uint8_t halfstep_count;
    int16_t top_limit_y;
    int16_t zero_top_y;
    int16_t zero_bottom_y;
    int16_t bottom_limit_y;
    int16_t top_span_px;
    int16_t bottom_span_px;
    int16_t start_y;
    int16_t end_y;

    if ((out_y == NULL) || (out_h == NULL))
    {
        return;
    }

    halfstep_count = vario_display_get_vario_halfstep_count(settings);
    vario_display_get_vario_screen_geometry(&top_limit_y,
                                            &zero_top_y,
                                            &zero_bottom_y,
                                            &bottom_limit_y,
                                            &top_span_px,
                                            &bottom_span_px);

    if ((halfstep_count == 0u) || (level_from_center >= halfstep_count))
    {
        *out_y = zero_top_y;
        *out_h = 0;
        return;
    }

    if (positive != false)
    {
        start_y = (int16_t)(zero_top_y -
                            (int16_t)lroundf((((float)level_from_center) / (float)halfstep_count) *
                                             (float)top_span_px));
        end_y = (int16_t)(zero_top_y -
                          (int16_t)lroundf((((float)(level_from_center + 1u)) / (float)halfstep_count) *
                                           (float)top_span_px) +
                          1);

        if (start_y > zero_top_y)
        {
            start_y = zero_top_y;
        }
        if (end_y < top_limit_y)
        {
            end_y = top_limit_y;
        }
        if (end_y > start_y)
        {
            end_y = start_y;
        }

        *out_y = end_y;
        *out_h = (int16_t)(start_y - end_y + 1);
    }
    else
    {
        start_y = (int16_t)(zero_bottom_y + 1 +
                            (int16_t)lroundf((((float)level_from_center) / (float)halfstep_count) *
                                             (float)bottom_span_px));
        end_y = (int16_t)(zero_bottom_y + 1 +
                          (int16_t)lroundf((((float)(level_from_center + 1u)) / (float)halfstep_count) *
                                           (float)bottom_span_px) -
                          1);

        if (start_y < (zero_bottom_y + 1))
        {
            start_y = (int16_t)(zero_bottom_y + 1);
        }
        if (end_y > bottom_limit_y)
        {
            end_y = bottom_limit_y;
        }
        if (end_y < start_y)
        {
            end_y = start_y;
        }

        *out_y = start_y;
        *out_h = (int16_t)(end_y - start_y + 1);
    }

    if (*out_h < 0)
    {
        *out_h = 0;
    }
}

static void vario_display_compute_vario_overrange_window(const vario_settings_t *settings,
                                                         float abs_value_mps,
                                                         uint8_t *out_first_level,
                                                         uint8_t *out_count)
{
    float   full_scale_mps;
    uint8_t halfstep_count;
    uint8_t overflow_steps;

    if ((out_first_level == NULL) || (out_count == NULL))
    {
        return;
    }

    full_scale_mps = vario_display_get_vario_scale_mps(settings);
    halfstep_count = vario_display_get_vario_halfstep_count(settings);
    *out_first_level = 0u;
    *out_count = 0u;

    if ((full_scale_mps <= 0.0f) || (halfstep_count == 0u) || (abs_value_mps <= 0.0f))
    {
        return;
    }

    if (abs_value_mps <= full_scale_mps)
    {
        *out_count = halfstep_count;
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  over-range ?쒖떆 洹쒖튃                                                   */
    /*                                                                        */
    /*  full scale 瑜??섏뼱媛硫?gauge ?꾩껜瑜?苑?梨꾩슦?????                   */
    /*  以묒떖異?0.0 洹쇱쿂) 履?slot 遺??0.5 m/s ?⑥쐞濡??섎굹??鍮꾩슫??            */
    /*                                                                        */
    /*  ?? 5.0 scale ?먯꽌 +5.5 => 0.0~0.5 slot ??鍮꾩썙吏怨?                   */
    /*      諛붾뒗 ?꾩そ 4.5 m/s 援ш컙留??⑤뒗??                                  */
    /* ---------------------------------------------------------------------- */
    overflow_steps = (uint8_t)ceilf((abs_value_mps - full_scale_mps) /
                                    VARIO_UI_VARIO_MINOR_STEP_MPS);
    if (overflow_steps >= halfstep_count)
    {
        *out_first_level = halfstep_count;
        *out_count = 0u;
        return;
    }

    *out_first_level = overflow_steps;
    *out_count = (uint8_t)(halfstep_count - overflow_steps);
}

static void vario_display_draw_vario_column(u8g2_t *u8g2,
                                            int16_t bar_x,
                                            uint8_t bar_w,
                                            float vario_mps,
                                            const vario_settings_t *settings)
{
    float   full_scale_mps;
    float   abs_value_mps;
    float   over_range_mps;
    bool    positive;
    int16_t top_limit_y;
    int16_t zero_top_y;
    int16_t zero_bottom_y;
    int16_t bottom_limit_y;
    int16_t top_span_px;
    int16_t bottom_span_px;
    int16_t fill_px;
    int16_t erase_px;
    int16_t visible_px;

    if (u8g2 == NULL)
    {
        return;
    }

    full_scale_mps = vario_display_get_vario_scale_mps(settings);
    abs_value_mps = vario_display_absf(vario_mps);
    positive = (vario_mps >= 0.0f) ? true : false;

    vario_display_get_vario_screen_geometry(&top_limit_y,
                                            &zero_top_y,
                                            &zero_bottom_y,
                                            &bottom_limit_y,
                                            &top_span_px,
                                            &bottom_span_px);

    if ((full_scale_mps <= 0.0f) || (abs_value_mps <= 0.0f))
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  1) ?뺤긽 踰붿쐞(0 ~ full scale)                                            */
    /*                                                                         */
    /*  湲곗〈怨??숈씪?섍쾶 以묒떖異뺤뿉??諛붽묑 諛⑺뼢?쇰줈 "?곗냽?곸쑝濡? 梨꾩슫??           */
    /*  ??援ш컙??洹몃옒???ㅼ????숈옉? 嫄대뱶由ъ? ?딅뒗??                        */
    /* ---------------------------------------------------------------------- */
    if (abs_value_mps <= full_scale_mps)
    {
        if (positive != false)
        {
            fill_px = vario_display_scale_value_to_fill_px(abs_value_mps,
                                                           full_scale_mps,
                                                           top_span_px);
            if (fill_px > 0)
            {
                u8g2_DrawBox(u8g2,
                             bar_x,
                             (int16_t)(zero_top_y - fill_px),
                             bar_w,
                             fill_px);
            }
        }
        else
        {
            fill_px = vario_display_scale_value_to_fill_px(abs_value_mps,
                                                           full_scale_mps,
                                                           bottom_span_px);
            if (fill_px > 0)
            {
                u8g2_DrawBox(u8g2,
                             bar_x,
                             (int16_t)(zero_bottom_y + 1),
                             bar_w,
                             fill_px);
            }
        }
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  2) ?ㅻ쾭?덉씤吏(full scale 珥덇낵)                                          */
    /*                                                                         */
    /*  湲곗〈 踰꾧렇: 0.5 m/s slot ?⑥쐞濡???移몄뵫 ?앸슍 吏?뚯졇??                   */
    /*  ?곗냽?곸씠吏 ?딄쾶 蹂댁???                                                */
    /*                                                                         */
    /*  ?섏젙: 珥덇낵遺?over_range_mps)??"?뺤긽 踰붿쐞? ?꾩쟾??媛숈? ?ㅼ??????쇰줈  */
    /*  ?쎌?濡??섏궛?댁꽌, 以묒떖異?履쎈????곗냽?곸쑝濡?吏?대떎.                      */
    /*                                                                         */
    /*  - + 諛⑺뼢: 苑?李?諛붿쓽 ?꾨옒履?0.0 履?遺???곗냽?곸쑝濡?吏?                */
    /*  - - 諛⑺뼢: 苑?李?諛붿쓽 ?꾩そ(0.0 履?遺???곗냽?곸쑝濡?吏?                  */
    /*                                                                         */
    /*  利? 0 -> 4.0/5.0 源뚯? ?щ씪媛??뚯쓽 遺?쒕윭?怨??숈씪??諛⑹떇?쇰줈            */
    /*  4.0/5.0 珥덇낵遺꾨룄 遺?쒕읇寃?"吏?뚯?硫? ?쒗쁽?쒕떎.                         */
    /* ---------------------------------------------------------------------- */
    over_range_mps = abs_value_mps - full_scale_mps;
    if (over_range_mps > full_scale_mps)
    {
        over_range_mps = full_scale_mps;
    }

    if (positive != false)
    {
        erase_px = vario_display_scale_value_to_fill_px(over_range_mps,
                                                        full_scale_mps,
                                                        top_span_px);
        visible_px = (int16_t)(top_span_px - erase_px);
        if (visible_px > 0)
        {
            u8g2_DrawBox(u8g2,
                         bar_x,
                         top_limit_y,
                         bar_w,
                         visible_px);
        }
    }
    else
    {
        erase_px = vario_display_scale_value_to_fill_px(over_range_mps,
                                                        full_scale_mps,
                                                        bottom_span_px);
        visible_px = (int16_t)(bottom_span_px - erase_px);
        if (visible_px > 0)
        {
            u8g2_DrawBox(u8g2,
                         bar_x,
                         (int16_t)(bottom_limit_y - visible_px + 1),
                         bar_w,
                         visible_px);
        }
    }
}

static uint16_t vario_display_get_gs_minor_tick_count(float gs_top_kmh,
                                                      float gs_minor_step_kmh)
{
    if ((gs_top_kmh <= 0.0f) || (gs_minor_step_kmh <= 0.0f))
    {
        return 0u;
    }

    return (uint16_t)lroundf(gs_top_kmh / gs_minor_step_kmh);
}

static int16_t vario_display_get_gs_value_center_y(const vario_viewport_t *v,
                                                   float value_kmh,
                                                   float gs_top_kmh)
{
    float ratio;

    if ((v == NULL) || (v->h <= 0) || (gs_top_kmh <= 0.0f))
    {
        return 0;
    }

    ratio = vario_display_clampf(value_kmh / gs_top_kmh, 0.0f, 1.0f);
    return (int16_t)(v->y + v->h - 1 - lroundf(ratio * (float)(v->h - 1)));
}

static int16_t vario_display_get_marker_top_y(int16_t center_y,
                                              int16_t marker_h,
                                              int16_t min_y,
                                              int16_t max_y)
{
    int16_t top_y;

    top_y = (int16_t)(center_y - (marker_h / 2));
    if (top_y < min_y)
    {
        top_y = min_y;
    }
    if ((top_y + marker_h) > max_y)
    {
        top_y = (int16_t)(max_y - marker_h);
    }

    return top_y;
}

void vario_display_draw_decimal_value(u8g2_t *u8g2,
                                             int16_t box_x,
                                             int16_t box_y,
                                             int16_t box_w,
                                             int16_t box_h,
                                             vario_ui_align_t align,
                                             const uint8_t *main_font,
                                             const uint8_t *frac_font,
                                             const char *value_text)
{
    char    sign[4];
    char    whole[16];
    char    frac[4];
    int16_t sign_w;
    int16_t whole_w;
    int16_t frac_w;
    int16_t total_w;
    int16_t draw_x;
    int16_t main_h;
    int16_t frac_h;
    int16_t sign_top;
    int16_t whole_top;
    int16_t frac_top;
    int16_t sign_x;
    int16_t whole_x;
    int16_t frac_x;

    if ((u8g2 == NULL) || (main_font == NULL) || (frac_font == NULL) || (value_text == NULL))
    {
        return;
    }

    memset(sign, 0, sizeof(sign));
    memset(whole, 0, sizeof(whole));
    memset(frac, 0, sizeof(frac));

    vario_display_split_decimal_value(value_text,
                                      sign,
                                      sizeof(sign),
                                      whole,
                                      sizeof(whole),
                                      frac,
                                      sizeof(frac));

    sign_w = (sign[0] != '\0') ? vario_display_measure_text(u8g2, VARIO_UI_FONT_BOTTOM_SIGN, sign) : 0;
    whole_w = vario_display_measure_text(u8g2, main_font, whole);
    frac_w = (frac[0] != '\0') ? vario_display_measure_text(u8g2, frac_font, frac) : 0;
    total_w = whole_w;
    if (sign_w > 0)
    {
        total_w = (int16_t)(total_w + sign_w + VARIO_UI_DECIMAL_SIGN_GAP_X);
    }
    if (frac_w > 0)
    {
        total_w = (int16_t)(total_w + VARIO_UI_DECIMAL_FRAC_GAP_X + frac_w);
    }

    switch (align)
    {
        case VARIO_UI_ALIGN_LEFT:
            draw_x = box_x;
            break;

        case VARIO_UI_ALIGN_CENTER:
            draw_x = (int16_t)(box_x + ((box_w - total_w) / 2));
            break;

        case VARIO_UI_ALIGN_RIGHT:
        default:
            draw_x = (int16_t)(box_x + box_w - total_w);
            break;
    }

    if (draw_x < box_x)
    {
        draw_x = box_x;
    }

    main_h = vario_display_get_font_height(u8g2, main_font);
    frac_h = vario_display_get_font_height(u8g2, frac_font);
    if (main_h <= 0)
    {
        main_h = box_h;
    }
    if (frac_h <= 0)
    {
        frac_h = box_h;
    }

    whole_top = box_y;
    if (box_h > main_h)
    {
        whole_top = (int16_t)(box_y + ((box_h - main_h) / 2));
    }
    sign_top = box_y;
    if (box_h > frac_h)
    {
        sign_top = (int16_t)(box_y + ((box_h - frac_h) / 2));
    }
    frac_top = (int16_t)(whole_top + VARIO_UI_DECIMAL_FRAC_TOP_BIAS_Y);

    sign_x = draw_x;
    whole_x = draw_x;
    if (sign_w > 0)
    {
        whole_x = (int16_t)(draw_x + sign_w + VARIO_UI_DECIMAL_SIGN_GAP_X);
    }
    frac_x = (int16_t)(whole_x + whole_w + VARIO_UI_DECIMAL_FRAC_GAP_X);

    u8g2_SetFontPosTop(u8g2);
    if (sign_w > 0)
    {
        u8g2_SetFont(u8g2, VARIO_UI_FONT_BOTTOM_SIGN);
        u8g2_DrawStr(u8g2, sign_x, sign_top, sign);
    }

    u8g2_SetFont(u8g2, main_font);
    u8g2_DrawStr(u8g2, whole_x, whole_top, whole);

    if (frac_w > 0)
    {
        u8g2_SetFont(u8g2, frac_font);
        u8g2_DrawStr(u8g2, frac_x, frac_top, frac);
    }

    u8g2_SetFontPosBaseline(u8g2);
}

/* -------------------------------------------------------------------------- */
/* ?꾩옱 VARIO ???レ옄 ?꾩슜 fixed-slot renderer                                  */
/*                                                                            */
/* ?ъ슜?먭? ?붽뎄??洹쒖튃                                                        */
/* 1) 10???먮━ 移몄? "??긽 ?덉빟"???붾떎.                                       */
/* 2) 媛믪씠 1.0 ?댁뼱??tens 移몄? ?⑥븘 ?덇퀬, ?ㅼ젣 ?レ옄??ones 移몄뿉留?洹몃┛??       */
/* 3) 媛믪씠 0.x ???뚮뒗 ones 移몄뿉 諛섎뱶??'0' ??洹몃┛??                         */
/* 4) 利? "leading zero ?쒓굅"??tens ?먮━留??④린怨? decimal 吏곸쟾 0 ? ?④린吏   */
/*    ?딅뒗??                                                                  */
/*                                                                            */
/* ??helper ??current VARIO ???レ옄 釉붾줉?먮쭔 ?곌퀬, ?ㅻⅨ ?レ옄 UI ??嫄대뱶由ъ?   */
/* ?딅뒗??                                                                     */
/* -------------------------------------------------------------------------- */
static void vario_display_draw_fixed_vario_current_value(u8g2_t *u8g2,
                                                         int16_t box_x,
                                                         int16_t box_y,
                                                         int16_t box_w,
                                                         int16_t box_h,
                                                         const char *value_text,
                                                         float signed_vario_mps)
{
    char    sign[4];
    char    whole[16];
    char    frac[4];
    char    digit_ch[2];
    size_t  whole_len;
    int16_t digit_w;
    int16_t frac_w;
    int16_t main_h;
    int16_t frac_h;
    int16_t whole_top;
    int16_t frac_top;
    int16_t draw_x;
    int16_t frac_x;

    if ((u8g2 == NULL) || (value_text == NULL))
    {
        return;
    }

    memset(sign, 0, sizeof(sign));
    memset(whole, 0, sizeof(whole));
    memset(frac, 0, sizeof(frac));
    memset(digit_ch, 0, sizeof(digit_ch));

    vario_display_split_decimal_value(value_text,
                                      sign,
                                      sizeof(sign),
                                      whole,
                                      sizeof(whole),
                                      frac,
                                      sizeof(frac));

    /* ---------------------------------------------------------------------- */
    /* current VARIO block ??absolute value 留?洹몃━誘濡?sign ? ?ъ슜?섏? ?딅뒗??*/
    /* decimal ???녾굅??digits 媛 box ?ㅺ퀎蹂대떎 湲몃㈃, 湲곗〈 generic renderer 濡?  */
    /* ?덉쟾?섍쾶 ?섎룎由곕떎.                                                      */
    /* ---------------------------------------------------------------------- */
    if ((sign[0] != '\0') || (frac[0] == '\0'))
    {
        vario_display_draw_decimal_value(u8g2,
                                         box_x,
                                         box_y,
                                         box_w,
                                         box_h,
                                         VARIO_UI_ALIGN_LEFT,
                                         VARIO_UI_FONT_BOTTOM_MAIN,
                                         VARIO_UI_FONT_BOTTOM_FRAC,
                                         value_text);
        return;
    }

    whole_len = strlen(whole);
    if (whole_len == 0u)
    {
        snprintf(whole, sizeof(whole), "0");
        whole_len = 1u;
    }

    if (whole_len > 2u)
    {
        vario_display_draw_decimal_value(u8g2,
                                         box_x,
                                         box_y,
                                         box_w,
                                         box_h,
                                         VARIO_UI_ALIGN_LEFT,
                                         VARIO_UI_FONT_BOTTOM_MAIN,
                                         VARIO_UI_FONT_BOTTOM_FRAC,
                                         value_text);
        return;
    }

    digit_w = vario_display_measure_text(u8g2, VARIO_UI_FONT_BOTTOM_MAIN, "0");
    frac_w = vario_display_measure_text(u8g2, VARIO_UI_FONT_BOTTOM_FRAC, frac);
    if ((digit_w <= 0) || (frac_w <= 0))
    {
        vario_display_draw_decimal_value(u8g2,
                                         box_x,
                                         box_y,
                                         box_w,
                                         box_h,
                                         VARIO_UI_ALIGN_LEFT,
                                         VARIO_UI_FONT_BOTTOM_MAIN,
                                         VARIO_UI_FONT_BOTTOM_FRAC,
                                         value_text);
        return;
    }

    draw_x = box_x;
    main_h = vario_display_get_font_height(u8g2, VARIO_UI_FONT_BOTTOM_MAIN);
    frac_h = vario_display_get_font_height(u8g2, VARIO_UI_FONT_BOTTOM_FRAC);
    if (main_h <= 0)
    {
        main_h = box_h;
    }
    if (frac_h <= 0)
    {
        frac_h = box_h;
    }

    whole_top = box_y;
    if (box_h > main_h)
    {
        whole_top = (int16_t)(box_y + ((box_h - main_h) / 2));
    }
    frac_top = (int16_t)(whole_top + VARIO_UI_DECIMAL_FRAC_TOP_BIAS_Y);
    frac_x = (int16_t)(draw_x + (digit_w * 2) + VARIO_UI_DECIMAL_FRAC_GAP_X);

    /* ------------------------------------------------------------------ */
    /* current VARIO ???レ옄??speed block 怨??숈씪?섍쾶,                     */
    /* whole digit 留?2 px ?쇱そ?쇰줈 ?대룞?쒖폒 frac digit 怨쇱쓽 媛꾧꺽??踰뚮┛??*/
    /* frac / LED / bar gauge ???덈? ?꾩튂???좎??쒕떎.                      */
    /* ------------------------------------------------------------------ */
    if ((draw_x - VARIO_UI_BOTTOM_MAIN_TO_FRAC_EXTRA_GAP_PX) >= box_x)
    {
        draw_x = (int16_t)(draw_x - VARIO_UI_BOTTOM_MAIN_TO_FRAC_EXTRA_GAP_PX);
    }
    else
    {
        draw_x = box_x;
    }

    u8g2_SetFontPosTop(u8g2);
    u8g2_SetFont(u8g2, VARIO_UI_FONT_BOTTOM_MAIN);

    /* tens slot: 10 ?댁긽???뚮쭔 洹몃┝. 1.x / 0.x ?먯꽌??鍮덉뭏留??덉빟???붾떎. */
    if (whole_len >= 2u)
    {
        digit_ch[0] = whole[0];
        digit_ch[1] = '\0';
        u8g2_DrawStr(u8g2, draw_x, whole_top, digit_ch);
    }

    /* ones slot: 0.x ???뚮룄 諛섎뱶??'0' ??蹂댁뿬???섎?濡???긽 留덉?留?digit ??洹몃┝. */
    digit_ch[0] = whole[whole_len - 1u];
    digit_ch[1] = '\0';
    u8g2_DrawStr(u8g2, (int16_t)(draw_x + digit_w), whole_top, digit_ch);

    u8g2_SetFont(u8g2, VARIO_UI_FONT_BOTTOM_FRAC);
    u8g2_DrawStr(u8g2, frac_x, frac_top, frac);

    /* ---------------------------------------------------------------------- */
    /* current VARIO 媛???LED-style 諛⑺뼢 ?꾩씠肄?                              */
    /*                                                                        */
    /* ?ъ슜?먯쓽 理쒖떊 ?붽뎄?ы빆                                                  */
    /* - ??VARIO 媛믪쓽 ?묒? ?뚯닔 digit 怨??숈씪??top Y 瑜?湲곗??쇰줈            */
    /*   ?곗륫 2 px ?놁뿉 ?곸듅 LED icon slot ???덉빟?쒕떎.                       */
    /* - ?섍컯 LED icon ? 洹?諛붾줈 ?꾨옒 2 px 媛꾧꺽?쇰줈 怨좎젙 諛곗튂?쒕떎.           */
    /* - 媛믪씠 ?곸듅/?섍컯/0 ?몄????곕씪 ?대떦 slot ??耳쒓굅???꾧린留??쒕떎.         */
    /* ---------------------------------------------------------------------- */
    {
        int16_t icon_x;
        int16_t up_icon_y;
        int16_t down_icon_y;

        icon_x = (int16_t)(frac_x + frac_w + 2);
        up_icon_y = frac_top;
        down_icon_y = (int16_t)(up_icon_y + VARIO_ICON_VARIO_LED_DOWN_HEIGHT + 2);

        if (signed_vario_mps > 0.05f)
        {
            vario_display_draw_xbm(u8g2,
                                   icon_x,
                                   up_icon_y,
                                   VARIO_ICON_VARIO_LED_UP_WIDTH,
                                   VARIO_ICON_VARIO_LED_UP_HEIGHT,
                                   vario_icon_vario_led_up_bits);
        }
        else if (signed_vario_mps < -0.05f)
        {
            vario_display_draw_xbm(u8g2,
                                   icon_x,
                                   down_icon_y,
                                   VARIO_ICON_VARIO_LED_DOWN_WIDTH,
                                   VARIO_ICON_VARIO_LED_DOWN_HEIGHT,
                                   vario_icon_vario_led_down_bits);
        }
    }

    u8g2_SetFontPosBaseline(u8g2);

    (void)box_w;
}



static void vario_display_format_optional_speed(char *buf,
                                                size_t buf_len,
                                                bool valid,
                                                float speed_kmh)
{
    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    if (valid == false)
    {
        snprintf(buf, buf_len, "--.-");
        return;
    }

    vario_display_format_speed_small(buf, buf_len, speed_kmh);
}

static void vario_display_format_estimated_te_value(char *buf,
                                                    size_t buf_len,
                                                    const vario_runtime_t *rt)
{
    float display_value;

    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    if ((rt == NULL) || (rt->estimated_te_valid == false))
    {
        snprintf(buf, buf_len, "--.-");
        return;
    }

    display_value = Vario_Settings_VSpeedMpsToDisplayFloat(rt->estimated_te_vario_mps);
    snprintf(buf,
             buf_len,
             "%.1f",
             (double)vario_display_clampf(display_value, -99.9f, 99.9f));
}

static int16_t vario_display_get_bottom_center_flight_time_top_y(u8g2_t *u8g2,
                                                                 const vario_viewport_t *v)
{
    int16_t text_h;

    if ((u8g2 == NULL) || (v == NULL))
    {
        return 0;
    }

    text_h = vario_display_get_font_height(u8g2, VARIO_UI_FONT_ALT2_VALUE);
    if (text_h <= 0)
    {
        text_h = 14;
    }

    return (int16_t)(v->y + v->h - 2 - text_h);
}

static int16_t vario_display_get_vario_main_value_box_y(u8g2_t *u8g2,
                                                        const vario_viewport_t *v)
{
    int16_t value_box_h;

    if ((u8g2 == NULL) || (v == NULL))
    {
        return 0;
    }

    value_box_h = vario_display_get_font_height(u8g2, VARIO_UI_FONT_BOTTOM_MAIN);
    if (value_box_h <= 0)
    {
        value_box_h = VARIO_UI_BOTTOM_BOX_H;
    }

    return (int16_t)(v->y + ((v->h - value_box_h) / 2));
}

static int16_t vario_display_get_vario_estimated_te_top_y(u8g2_t *u8g2,
                                                          const vario_viewport_t *v)
{
    int16_t value_box_h;

    if ((u8g2 == NULL) || (v == NULL))
    {
        return 0;
    }

    value_box_h = vario_display_get_font_height(u8g2, VARIO_UI_FONT_BOTTOM_MAIN);
    if (value_box_h <= 0)
    {
        value_box_h = VARIO_UI_BOTTOM_BOX_H;
    }

    return (int16_t)(vario_display_get_vario_main_value_box_y(u8g2, v) +
                     value_box_h +
                     VARIO_UI_BOTTOM_META_GAP_Y);
}

static int16_t vario_display_get_vario_estimated_te_bottom_y(u8g2_t *u8g2,
                                                             const vario_viewport_t *v)
{
    int16_t top_y;
    int16_t value_h;
    int16_t unit_h;

    if ((u8g2 == NULL) || (v == NULL))
    {
        return 0;
    }

    top_y = vario_display_get_vario_estimated_te_top_y(u8g2, v);
    value_h = vario_display_get_font_height(u8g2, VARIO_UI_FONT_ALT2_UNIT);
    unit_h = vario_display_get_font_height(u8g2, VARIO_UI_FONT_ALT2_UNIT);
    if (value_h <= 0)
    {
        value_h = 7;
    }
    if (unit_h > value_h)
    {
        value_h = unit_h;
    }

    return (int16_t)(top_y + value_h);
}

static void vario_display_draw_top_left_glide_ratio_gauge(u8g2_t *u8g2,
                                                          const vario_viewport_t *v,
                                                          const vario_runtime_t *rt)
{
    int16_t gauge_left_x;
    int16_t gauge_right_x;
    int16_t gauge_w;
    int16_t gauge_top_y;
    float   inst_ratio;
    float   avg_ratio;
    int16_t inst_mark_x;
    int16_t inst_bar_top_y;
    int16_t avg_mark_x;
    int16_t avg_mark_top_y;
    float   tick_ratio;
    float   extra_minor_ratio;

    if ((u8g2 == NULL) || (v == NULL) || (rt == NULL))
    {
        return;
    }

    gauge_left_x = (int16_t)(v->x + VARIO_UI_TOP_GLD_GAUGE_X_OFF);
    gauge_right_x = (int16_t)(v->x + (v->w / 3) + VARIO_UI_TOP_GLD_GAUGE_RIGHT_EXTRA);
    gauge_w = (int16_t)(gauge_right_x - gauge_left_x);
    gauge_top_y = (int16_t)(v->y + VARIO_UI_TOP_GLD_GAUGE_TOP_Y);
    if (gauge_w <= 0)
    {
        return;
    }

    inst_ratio = 0.0f;
    if (rt->glide_ratio_instant_valid != false)
    {
        inst_ratio = vario_display_clampf(rt->glide_ratio_instant,
                                          0.0f,
                                          VARIO_UI_TOP_GLD_GAUGE_MAX_RATIO);
    }

    avg_ratio = 0.0f;
    if (s_vario_ui_dynamic.fast_average_glide_valid != false)
    {
        avg_ratio = vario_display_clampf(s_vario_ui_dynamic.fast_average_glide_ratio,
                                         0.0f,
                                         VARIO_UI_TOP_GLD_GAUGE_MAX_RATIO);
    }

    for (tick_ratio = 0.0f;
         tick_ratio <= VARIO_UI_TOP_GLD_GAUGE_MAX_RATIO;
         tick_ratio += VARIO_UI_TOP_GLD_GAUGE_MINOR_STEP)
    {
        bool    is_major;
        int16_t tick_x;
        int16_t tick_h;

        is_major = (fmodf(tick_ratio, VARIO_UI_TOP_GLD_GAUGE_MAJOR_STEP) < 0.001f) ||
                   (fabsf(tick_ratio - VARIO_UI_TOP_GLD_GAUGE_MAX_RATIO) < 0.001f);
        tick_h = is_major ?
            VARIO_UI_TOP_GLD_GAUGE_MAJOR_TICK_H :
            VARIO_UI_TOP_GLD_GAUGE_MINOR_TICK_H;
        tick_x = (int16_t)(gauge_left_x +
                           lroundf((tick_ratio / VARIO_UI_TOP_GLD_GAUGE_MAX_RATIO) * (float)(gauge_w - 1)));
        if (tick_x < gauge_left_x)
        {
            tick_x = gauge_left_x;
        }
        if (tick_x > (int16_t)(gauge_left_x + gauge_w - 1))
        {
            tick_x = (int16_t)(gauge_left_x + gauge_w - 1);
        }

        u8g2_DrawVLine(u8g2, tick_x, gauge_top_y, tick_h);
    }

    /* ------------------------------------------------------------------ */
    /* ?ъ슜?먭? ?붿껌???濡? 湲곗〈 major / minor tick ? 洹몃?濡??붾떎.       */
    /*                                                                    */
    /* 湲곗〈 tick 諛곗튂??0 / 5 / 10 / 15 / 20 ?닿퀬, ???곹깭?먯꽌??          */
    /* major ?ъ씠???묒? ?덇툑??1媛쒕쭔 議댁옱?쒕떎.                            */
    /*                                                                    */
    /* ?ш린?쒕뒗 湲곗〈 ?덇툑??蹂寃쏀븯吏 ?딄퀬, 洹?"?ъ씠" ?먮쭔 ?숈씪??minor    */
    /* tick ???섎굹 ??異붽??댁꽌                                            */
    /*   major - minor - minor - minor - major                            */
    /* ?⑦꽩???섎룄濡?留뚮뱺??                                               */
    /*                                                                    */
    /* 利?0..20 ?ㅼ???湲곗??쇰줈 2.5 / 7.5 / 12.5 / 17.5 ?꾩튂??            */
    /* 湲곗〈 minor ? ?숈씪???믪씠??tick ??異붽??쒕떎.                      */
    /* ------------------------------------------------------------------ */
    for (extra_minor_ratio = (VARIO_UI_TOP_GLD_GAUGE_MINOR_STEP * 0.5f);
         extra_minor_ratio < VARIO_UI_TOP_GLD_GAUGE_MAX_RATIO;
         extra_minor_ratio += VARIO_UI_TOP_GLD_GAUGE_MINOR_STEP)
    {
        int16_t tick_x;

        tick_x = (int16_t)(gauge_left_x +
                           lroundf((extra_minor_ratio / VARIO_UI_TOP_GLD_GAUGE_MAX_RATIO) * (float)(gauge_w - 1)));
        if (tick_x < gauge_left_x)
        {
            tick_x = gauge_left_x;
        }
        if (tick_x > (int16_t)(gauge_left_x + gauge_w - 1))
        {
            tick_x = (int16_t)(gauge_left_x + gauge_w - 1);
        }

        u8g2_DrawVLine(u8g2,
                       tick_x,
                       gauge_top_y,
                       VARIO_UI_TOP_GLD_GAUGE_MINOR_TICK_H);
    }

    if (rt->glide_ratio_instant_valid != false)
    {
        int16_t inst_fill_w;

        inst_mark_x = (int16_t)(gauge_left_x +
                                lroundf((inst_ratio / VARIO_UI_TOP_GLD_GAUGE_MAX_RATIO) * (float)(gauge_w - 1)));
        inst_bar_top_y = (int16_t)(gauge_top_y + VARIO_UI_TOP_GLD_AVG_BAR_TOP_DY);
        if (inst_mark_x < gauge_left_x)
        {
            inst_mark_x = gauge_left_x;
        }
        if (inst_mark_x > (int16_t)(gauge_left_x + gauge_w - 1))
        {
            inst_mark_x = (int16_t)(gauge_left_x + gauge_w - 1);
        }

        /* ------------------------------------------------------------------ */
        /* instant glide bar ???쇱そ?먯꽌 ?ㅻⅨ履쎌쑝濡?李⑥삤瑜대뒗 fill bar??      */
        /* ?대룞 ?ъ씤?곗쿂??蹂댁씠吏 ?딅룄濡? gauge left 湲곗? ?꾩쟻 width 濡?洹몃┛??*/
        /* ------------------------------------------------------------------ */
        inst_fill_w = (int16_t)(inst_mark_x - gauge_left_x + 1);
        if (inst_fill_w > gauge_w)
        {
            inst_fill_w = gauge_w;
        }
        if (inst_fill_w > 0)
        {
            u8g2_DrawBox(u8g2,
                         gauge_left_x,
                         inst_bar_top_y,
                         inst_fill_w,
                         VARIO_UI_TOP_GLD_AVG_BAR_H);
        }
    }

    if (s_vario_ui_dynamic.fast_average_glide_valid != false)
    {
        avg_mark_x = (int16_t)(gauge_left_x +
                               lroundf((avg_ratio / VARIO_UI_TOP_GLD_GAUGE_MAX_RATIO) * (float)(gauge_w - 1)));
        avg_mark_top_y = (int16_t)(gauge_top_y + VARIO_UI_TOP_GLD_AVG_MARK_CENTER_DY -
                                   (VARIO_ICON_AVG_GLD_MARK_HEIGHT / 2));
        vario_display_draw_xbm(u8g2,
                               (int16_t)(avg_mark_x - (VARIO_ICON_AVG_GLD_MARK_WIDTH / 2)),
                               avg_mark_top_y,
                               VARIO_ICON_AVG_GLD_MARK_WIDTH,
                               VARIO_ICON_AVG_GLD_MARK_HEIGHT,
                               vario_icon_avg_gld_mark_bits);
    }
}

static void vario_display_draw_top_left_metrics(u8g2_t *u8g2,
                                                const vario_viewport_t *v,
                                                const vario_runtime_t *rt)
{
    char    glide_text[12];
    int16_t gauge_left_x;
    int16_t gauge_right_x;
    int16_t gauge_w;
    int16_t value_box_w;
    int16_t value_x;
    int16_t value_y;
    int16_t unit_x;
    int16_t unit_w;

    if ((u8g2 == NULL) || (v == NULL) || (rt == NULL))
    {
        return;
    }

    gauge_left_x = (int16_t)(v->x + VARIO_UI_TOP_GLD_GAUGE_X_OFF);
    gauge_right_x = (int16_t)(v->x + (v->w / 3) + VARIO_UI_TOP_GLD_GAUGE_RIGHT_EXTRA);
    gauge_w = (int16_t)(gauge_right_x - gauge_left_x);
    if (gauge_w <= 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  ?곷떒 醫뚯륫 glide section                                                */
    /*                                                                        */
    /*  理쒖떊 ?붽뎄?ы빆                                                          */
    /*  - ?곷떒 gauge ??洹몃?濡?instant bar + average marker 瑜??좎??쒕떎.       */
    /*  - ?묎쾶 遺숈뿬 ?먮뜕 ?됯퇏 ?쒓났鍮??섏튂???꾩쟾???쒓굅?쒕떎.                   */
    /*  - ?ш쾶 蹂댁씠??二??섏튂?????댁긽 instantaneous 媛 ?꾨땲??              */
    /*    average glide ratio 瑜??쒖떆?쒕떎.                                    */
    /* ---------------------------------------------------------------------- */
    vario_display_draw_top_left_glide_ratio_gauge(u8g2, v, rt);

    vario_display_format_glide_ratio_value(glide_text,
                                           sizeof(glide_text),
                                           s_vario_ui_dynamic.displayed_glide_valid,
                                           s_vario_ui_dynamic.displayed_glide_ratio);

    value_box_w = vario_display_measure_text(u8g2, VARIO_UI_FONT_ALT2_VALUE, "99.9");
    value_x = gauge_left_x;
    value_y = (int16_t)(v->y + VARIO_UI_TOP_GLD_GAUGE_TOP_Y +
                        VARIO_UI_TOP_GLD_GAUGE_H +
                        VARIO_UI_TOP_GLD_GAUGE_TEXT_GAP_Y);
    unit_w = vario_display_measure_text(u8g2, VARIO_UI_FONT_ALT2_UNIT, ":1");
    unit_x = (int16_t)(value_x + value_box_w + VARIO_UI_TOP_GLD_VALUE_UNIT_GAP);

    vario_display_draw_text_box_top(u8g2,
                                    value_x,
                                    value_y,
                                    value_box_w,
                                    VARIO_UI_ALIGN_RIGHT,
                                    VARIO_UI_FONT_ALT2_VALUE,
                                    glide_text);
    vario_display_draw_text_box_top(u8g2,
                                    unit_x,
                                    value_y,
                                    unit_w,
                                    VARIO_UI_ALIGN_LEFT,
                                    VARIO_UI_FONT_ALT2_UNIT,
                                    ":1");
}


static void vario_display_draw_top_center_clock(u8g2_t *u8g2,
                                                const vario_viewport_t *v,
                                                const vario_runtime_t *rt,
                                                const vario_settings_t *settings)
{
    char    clock_text[20];
    int16_t flight_time_top_y;
    int16_t clock_h;
    int16_t clock_top_y;

    if ((u8g2 == NULL) || (v == NULL) || (rt == NULL) || (settings == NULL) || (settings->show_current_time == 0u))
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /* ?꾩옱 ?쒓컖? ???댁긽 top-center ???먯? ?딅뒗??                          */
    /*                                                                        */
    /* ?댁쑀                                                                   */
    /* - ?곷떒????INST glide gauge 媛 ?붾㈃ top edge 瑜??ъ슜?쒕떎.               */
    /* - ?ъ슜?먭? 吏?뺥븳 ???꾩튂??FLT TIME 諛붾줈 ??2 px ?대떎.                 */
    /* - ?곕씪??flight-time box??top Y瑜?怨듭슜 helper濡?援ы븯怨?                */
    /*   洹??꾩뿉 clock font height 留뚰겮 ?щ┛ ??2 px gap ???④릿??           */
    /* ---------------------------------------------------------------------- */
    vario_display_format_clock(clock_text, sizeof(clock_text), rt, settings);

    flight_time_top_y = vario_display_get_bottom_center_flight_time_top_y(u8g2, v);
    clock_h = vario_display_get_font_height(u8g2, VARIO_UI_FONT_TOP_CLOCK);
    if (clock_h <= 0)
    {
        clock_h = 12;
    }
    clock_top_y = (int16_t)(flight_time_top_y - VARIO_UI_BOTTOM_CLOCK_GAP_Y - clock_h);
    if (clock_top_y < v->y)
    {
        clock_top_y = v->y;
    }

    u8g2_SetFontPosTop(u8g2);
    u8g2_SetFont(u8g2, VARIO_UI_FONT_TOP_CLOCK);
    Vario_Display_DrawTextCentered(u8g2,
                                   (int16_t)(v->x + (v->w / 2)),
                                   clock_top_y,
                                   clock_text);
    u8g2_SetFontPosBaseline(u8g2);
}


static void vario_display_format_arrival_height_value(char *buf,
                                                      size_t buf_len,
                                                      bool valid,
                                                      float arrival_height_m)
{
    long display_value;

    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    if (valid == false)
    {
        snprintf(buf, buf_len, "-----");
        return;
    }

    display_value = (long)Vario_Settings_AltitudeMetersToDisplayRounded(arrival_height_m);
    if (display_value > 99999L)
    {
        display_value = 99999L;
    }
    if (display_value < -9999L)
    {
        display_value = -9999L;
    }

    /* ---------------------------------------------------------------------- */
    /* ?뚯닔 遺?몃뒗 ??긽 10k digit slot ??怨좎젙?쒕떎.                           */
    /*                                                                        */
    /* ALT2 value font ??怨좎젙??怨꾩뿴?대씪,                                     */
    /* - ?묒닔  : " 1234" / "12345"                                            */
    /* - ?뚯닔  : "-1234" / "- 123"                                            */
    /* ?뺥깭濡?formatting ?섎㈃ sign column ????긽 醫뚯륫 slot ??怨좎젙?쒕떎.       */
    /* ---------------------------------------------------------------------- */
    if (display_value < 0L)
    {
        snprintf(buf, buf_len, "-%4ld", (long)(-display_value));
    }
    else
    {
        snprintf(buf, buf_len, "%5ld", display_value);
    }
}



static void vario_display_format_signed_altitude_5slot(char *buf,
                                                       size_t buf_len,
                                                       bool valid,
                                                       long display_value)
{
    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    if (valid == false)
    {
        snprintf(buf, buf_len, "-----");
        return;
    }

    if (display_value > VARIO_UI_ALT23_SIGNED_MAX_DISPLAY)
    {
        display_value = VARIO_UI_ALT23_SIGNED_MAX_DISPLAY;
    }
    if (display_value < VARIO_UI_ALT23_SIGNED_MIN_DISPLAY)
    {
        display_value = VARIO_UI_ALT23_SIGNED_MIN_DISPLAY;
    }

    /* ------------------------------------------------------------------ */
    /* ALT2 / ALT3 signed five-slot formatter                              */
    /*                                                                    */
    /* ?ъ슜?먯쓽 理쒖떊 ?붽뎄?ы빆 洹몃?濡? ?뚯닔 遺?몃뒗 ??긽 媛???쇱そ sign slot   */
    /* ??怨좎젙?쒕떎. ?묒닔??5-column right align ?쇰줈 異쒕젰?쒕떎.               */
    /* ------------------------------------------------------------------ */
    if (display_value < 0L)
    {
        snprintf(buf, buf_len, "-%4ld", (long)(-display_value));
    }
    else
    {
        snprintf(buf, buf_len, "%5ld", display_value);
    }
}

static void vario_display_format_signed_altitude_5slot_from_unit_bank(char *buf,
                                                                      size_t buf_len,
                                                                      bool valid,
                                                                      const app_altitude_linear_units_t *units,
                                                                      vario_alt_unit_t unit)
{
    long display_value;

    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    if ((valid == false) || (units == NULL))
    {
        snprintf(buf, buf_len, "-----");
        return;
    }

    display_value = (long)vario_display_select_altitude_from_unit_bank(units, unit);
    vario_display_format_signed_altitude_5slot(buf, buf_len, true, display_value);
}

static void vario_display_format_signed_altitude_5slot_from_meters(char *buf,
                                                                   size_t buf_len,
                                                                   bool valid,
                                                                   float altitude_m,
                                                                   vario_alt_unit_t unit)
{
    long display_value;

    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    if (valid == false)
    {
        snprintf(buf, buf_len, "-----");
        return;
    }

    display_value = (long)Vario_Settings_AltitudeMetersToDisplayRoundedWithUnit(altitude_m, unit);
    vario_display_format_signed_altitude_5slot(buf, buf_len, true, display_value);
}

static void vario_display_format_flight_distance_value(char *buf,
                                                       size_t buf_len,
                                                       float distance_m)
{
    float display_distance;

    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    display_distance = Vario_Settings_NavDistanceMetersToDisplayFloat(distance_m);
    display_distance = vario_display_clampf(display_distance, 0.0f, 999.9f);
    snprintf(buf, buf_len, "%.1f", (double)display_distance);
}


static void vario_display_format_lower_left_context_label(char *buf,
                                                          size_t buf_len,
                                                          const vario_runtime_t *rt)
{
    const char *src;
    size_t src_len;

    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    buf[0] = '\0';
    if (rt == NULL)
    {
        return;
    }

    /* ------------------------------------------------------------------ */
    /* 醫뚰븯??glide block ?곷떒 context label                               */
    /* - target name ???덉쑝硫?洹??대쫫???곗꽑 ?ъ슜?쒕떎.                    */
    /* - ?대쫫???놁쑝硫?source short label(HME/LND/WPT/...) 瑜??대떎.        */
    /* - 湲몄씠??理쒕? 8?먮줈 ?섎씪, 湲곗〈 ?レ옄 釉붾줉 ?꾩뿉 ?묎쾶 ?밸뒗??          */
    /* ------------------------------------------------------------------ */
    src = (rt->target_name[0] != '\0') ? rt->target_name
                                         : Vario_Navigation_GetShortSourceLabel(rt->target_kind);
    src_len = strlen(src);
    if (src_len > 8u)
    {
        src_len = 8u;
    }

    memcpy(buf, src, src_len);
    buf[src_len] = '\0';
}

static void vario_display_draw_lower_left_glide_computer(u8g2_t *u8g2,
                                                           const vario_viewport_t *v,
                                                           const vario_runtime_t *rt)
{
    char    final_glide_text[16];
    char    arrival_text[16];
    char    distance_text[16];
    char    context_label[9];
    const char *distance_label;
    char    distance_unit[8];
    int16_t left_x;
    int16_t top_y;
    int16_t icon_lane_w;
    int16_t value_box_w;
    int16_t unit_box_w;
    int16_t value_h;
    int16_t unit_h;
    int16_t row_h;
    int16_t row_gap;
    int16_t row0_y;
    int16_t row1_y;
    int16_t row2_y;
    int16_t icon_x;
    int16_t context_x;
    int16_t context_y;
    int16_t context_box_w;
    int16_t value_x;
    int16_t unit_x;
    long    arrival_disp;
    float   display_distance;
    const char *alt_unit;

    if ((u8g2 == NULL) || (v == NULL) || (rt == NULL))
    {
        return;
    }

    distance_label = Vario_Navigation_GetShortSourceLabel(rt->target_kind);
    vario_display_format_lower_left_context_label(context_label, sizeof(context_label), rt);

    /* ---------------------------------------------------------------------- */
    /* 醫뚰븯??glide-computer 3以??명듃                                          */
    /*                                                                        */
    /* ?대쾲 ?섏젙 ?붽뎄?ы빆                                                       */
    /* - Final Glide / Arr H / Distance ??row 瑜?"臾띠뼱?? Y異??꾨옒濡??대┛??  */
    /* - ?? Distance value row ??top Y ??FLT TIME value row ? ?뺥솗??留욎텣??*/
    /* - Arr H / Final Glide ??洹몃줈遺??湲곗〈 row 媛꾧꺽 洹쒖튃??洹몃?濡??좎??쒕떎. */
    /*                                                                        */
    /* 利? anchor ??Distance value row ?닿퀬,                                  */
    /* ?섎㉧吏 ??row ???곷? ?꾩튂留?洹몃?濡??좎???梨?媛숈씠 ?대룞?쒕떎.            */
    /* ---------------------------------------------------------------------- */
    left_x = (int16_t)(v->x + VARIO_UI_SIDE_BAR_W + 4);

    value_h = vario_display_get_font_height(u8g2, VARIO_UI_FONT_ALT2_VALUE);
    unit_h = vario_display_get_font_height(u8g2, VARIO_UI_FONT_ALT2_UNIT);
    if (value_h <= 0)
    {
        value_h = 10;
    }
    if (unit_h <= 0)
    {
        unit_h = 7;
    }

    icon_lane_w = VARIO_ICON_FINAL_GLIDE_WIDTH;
    {
        int16_t dst_w;
        dst_w = vario_display_measure_text(u8g2,
                                           VARIO_UI_FONT_ALT2_UNIT,
                                           Vario_Navigation_GetShortSourceLabel(rt->target_kind));
        if (icon_lane_w < dst_w)
        {
            icon_lane_w = dst_w;
        }
    }

    value_box_w = vario_display_measure_text(u8g2, VARIO_UI_FONT_ALT2_VALUE, "99999");
    {
        int16_t ratio_w;
        int16_t dist_w;
        ratio_w = vario_display_measure_text(u8g2, VARIO_UI_FONT_ALT2_VALUE, "99.9");
        dist_w = vario_display_measure_text(u8g2, VARIO_UI_FONT_ALT2_VALUE, "999.9");
        if (value_box_w < ratio_w)
        {
            value_box_w = ratio_w;
        }
        if (value_box_w < dist_w)
        {
            value_box_w = dist_w;
        }
    }

    alt_unit = Vario_Settings_GetAltitudeUnitText();
    unit_box_w = vario_display_measure_text(u8g2, VARIO_UI_FONT_ALT2_UNIT, ":1");
    {
        int16_t alt_w;
        int16_t dst_unit_w;
        alt_w = vario_display_measure_text(u8g2, VARIO_UI_FONT_ALT2_UNIT, alt_unit);
        snprintf(distance_unit, sizeof(distance_unit), "%s", Vario_Settings_GetNavDistanceUnitText());
        dst_unit_w = vario_display_measure_text(u8g2, VARIO_UI_FONT_ALT2_UNIT, distance_unit);
        if (unit_box_w < alt_w)
        {
            unit_box_w = alt_w;
        }
        if (unit_box_w < dst_unit_w)
        {
            unit_box_w = dst_unit_w;
        }
    }

    row_h = value_h;
    if (row_h < unit_h)
    {
        row_h = unit_h;
    }
    if (row_h < VARIO_ICON_FINAL_GLIDE_HEIGHT)
    {
        row_h = VARIO_ICON_FINAL_GLIDE_HEIGHT;
    }
    if (row_h < VARIO_ICON_ARRIVAL_HEIGHT_HEIGHT)
    {
        row_h = VARIO_ICON_ARRIVAL_HEIGHT_HEIGHT;
    }

    row_gap = 2;

    /* ---------------------------------------------------------------------- */
    /* Distance value row 媛 FLT TIME value ? ?숈씪 top Y 瑜??곕룄濡?           */
    /* block ?꾩껜??anchor(top_y)瑜???궛?쒕떎.                                  */
    /* ---------------------------------------------------------------------- */
    top_y = (int16_t)(vario_display_get_bottom_center_flight_time_top_y(u8g2, v) -
                      (2 * (row_h + row_gap)) -
                      ((row_h - value_h) / 2));

    row0_y = top_y;
    row1_y = (int16_t)(row0_y + row_h + row_gap);
    row2_y = (int16_t)(row1_y + row_h + row_gap);

    icon_x = left_x;
    value_x = (int16_t)(icon_x + icon_lane_w + VARIO_UI_TOP_ALT_ROW_ICON_GAP);
    unit_x = (int16_t)(value_x + value_box_w + VARIO_UI_TOP_ALT_ROW_VALUE_UNIT_GAP);

    /* ------------------------------------------------------------------ */
    /* ??context label 醫뚰몴                                                */
    /* - X ??Final Glide XBM ???ㅼ젣 ?쒖옉 X                               */
    /* - Y ??Final Glide ?レ옄 top 蹂대떎 2 px ??                           */
    /* - font ??unit label 怨??숈씪                                         */
    /* - 醫뚯륫 ?뺣젹                                                          */
    /* ------------------------------------------------------------------ */
    context_x = (int16_t)(icon_x + ((icon_lane_w - VARIO_ICON_FINAL_GLIDE_WIDTH) / 2));
    context_y = (int16_t)(row0_y + ((row_h - value_h) / 2) - unit_h - 2);
    context_box_w = (int16_t)((value_x + value_box_w) - context_x);
    if (context_box_w < 8)
    {
        context_box_w = 8;
    }

    if (rt->final_glide_valid != false)
    {
        snprintf(final_glide_text,
                 sizeof(final_glide_text),
                 "%.1f",
                 (double)vario_display_clampf(rt->required_glide_ratio, 0.0f, 99.9f));
    }
    else
    {
        snprintf(final_glide_text, sizeof(final_glide_text), "--.-");
    }

    if (rt->final_glide_valid != false)
    {
        arrival_disp = (long)Vario_Settings_AltitudeMetersToDisplayRounded(rt->arrival_height_m);
    }
    else
    {
        arrival_disp = 0L;
    }
    (void)arrival_disp;
    vario_display_format_arrival_height_value(arrival_text,
                                              sizeof(arrival_text),
                                              rt->final_glide_valid,
                                              rt->arrival_height_m);

    if (rt->target_valid != false)
    {
        display_distance = Vario_Settings_NavDistanceMetersToDisplayFloat(rt->target_distance_m);
        display_distance = vario_display_clampf(display_distance, 0.0f, 999.9f);
        snprintf(distance_text, sizeof(distance_text), "%.1f", (double)display_distance);

        if (context_label[0] != '\0')
        {
            vario_display_draw_text_box_top(u8g2,
                                            context_x,
                                            context_y,
                                            context_box_w,
                                            VARIO_UI_ALIGN_LEFT,
                                            VARIO_UI_FONT_ALT2_UNIT,
                                            context_label);
        }

        /* -------------------------------------------------------------- */
        /* slot #1                                                        */
        /* - 紐⑹쟻吏媛 ?ㅼ젙??寃쎌슦, 湲곗〈 glide computer 3以??명듃瑜?          */
        /*   洹몃?濡??쒖떆?쒕떎.                                              */
        /* -------------------------------------------------------------- */
        vario_display_draw_xbm(u8g2,
                               (int16_t)(icon_x + ((icon_lane_w - VARIO_ICON_FINAL_GLIDE_WIDTH) / 2)),
                               (int16_t)(row0_y + ((row_h - VARIO_ICON_FINAL_GLIDE_HEIGHT) / 2)),
                               VARIO_ICON_FINAL_GLIDE_WIDTH,
                               VARIO_ICON_FINAL_GLIDE_HEIGHT,
                               vario_icon_final_glide_bits);
        vario_display_draw_text_box_top(u8g2,
                                        value_x,
                                        (int16_t)(row0_y + ((row_h - value_h) / 2)),
                                        value_box_w,
                                        VARIO_UI_ALIGN_RIGHT,
                                        VARIO_UI_FONT_ALT2_VALUE,
                                        final_glide_text);
        vario_display_draw_text_box_top(u8g2,
                                        unit_x,
                                        (int16_t)(row0_y + ((row_h - unit_h) / 2)),
                                        unit_box_w,
                                        VARIO_UI_ALIGN_LEFT,
                                        VARIO_UI_FONT_ALT2_UNIT,
                                        ":1");

        vario_display_draw_xbm(u8g2,
                               (int16_t)(icon_x + ((icon_lane_w - VARIO_ICON_ARRIVAL_HEIGHT_WIDTH) / 2)),
                               (int16_t)(row1_y + ((row_h - VARIO_ICON_ARRIVAL_HEIGHT_HEIGHT) / 2)),
                               VARIO_ICON_ARRIVAL_HEIGHT_WIDTH,
                               VARIO_ICON_ARRIVAL_HEIGHT_HEIGHT,
                               vario_icon_arrival_height_bits);
        vario_display_draw_text_box_top(u8g2,
                                        value_x,
                                        (int16_t)(row1_y + ((row_h - value_h) / 2)),
                                        value_box_w,
                                        VARIO_UI_ALIGN_RIGHT,
                                        VARIO_UI_FONT_ALT2_VALUE,
                                        arrival_text);
        vario_display_draw_text_box_top(u8g2,
                                        unit_x,
                                        (int16_t)(row1_y + ((row_h - unit_h) / 2)),
                                        unit_box_w,
                                        VARIO_UI_ALIGN_LEFT,
                                        VARIO_UI_FONT_ALT2_UNIT,
                                        alt_unit);

        vario_display_draw_text_box_top(u8g2,
                                        icon_x,
                                        (int16_t)(row2_y + ((row_h - unit_h) / 2)),
                                        icon_lane_w,
                                        VARIO_UI_ALIGN_LEFT,
                                        VARIO_UI_FONT_ALT2_UNIT,
                                        distance_label);
        vario_display_draw_text_box_top(u8g2,
                                        value_x,
                                        (int16_t)(row2_y + ((row_h - value_h) / 2)),
                                        value_box_w,
                                        VARIO_UI_ALIGN_RIGHT,
                                        VARIO_UI_FONT_ALT2_VALUE,
                                        distance_text);
        vario_display_draw_text_box_top(u8g2,
                                        unit_x,
                                        (int16_t)(row2_y + ((row_h - unit_h) / 2)),
                                        unit_box_w,
                                        VARIO_UI_ALIGN_LEFT,
                                        VARIO_UI_FONT_ALT2_UNIT,
                                        distance_unit);
    }
    else
    {
        /* -------------------------------------------------------------- */
        /* slot #2                                                        */
        /* - 紐⑹쟻吏媛 ?ㅼ젙?섏? ?딆? 寃쎌슦, 媛숈? DIST row ?꾩튂??              */
        /*   "吏湲덇퉴吏 鍮꾪뻾??嫄곕━" 留??쒖떆?쒕떎.                           */
        /* - ?붿껌?ы빆?濡?Final Glide / Arr H / context label ?            */
        /*   ?꾪? 洹몃━吏 ?딅뒗??                                            */
        /* -------------------------------------------------------------- */
        vario_display_format_flight_distance_value(distance_text,
                                                   sizeof(distance_text),
                                                   s_vario_ui_dynamic.accumulated_flight_distance_m);

        vario_display_draw_text_box_top(u8g2,
                                        icon_x,
                                        (int16_t)(row2_y + ((row_h - unit_h) / 2)),
                                        icon_lane_w,
                                        VARIO_UI_ALIGN_LEFT,
                                        VARIO_UI_FONT_ALT2_UNIT,
                                        "FLT");
        vario_display_draw_text_box_top(u8g2,
                                        value_x,
                                        (int16_t)(row2_y + ((row_h - value_h) / 2)),
                                        value_box_w,
                                        VARIO_UI_ALIGN_RIGHT,
                                        VARIO_UI_FONT_ALT2_VALUE,
                                        distance_text);
        vario_display_draw_text_box_top(u8g2,
                                        unit_x,
                                        (int16_t)(row2_y + ((row_h - unit_h) / 2)),
                                        unit_box_w,
                                        VARIO_UI_ALIGN_LEFT,
                                        VARIO_UI_FONT_ALT2_UNIT,
                                        distance_unit);
    }
}



static void vario_display_draw_bottom_center_flight_time(u8g2_t *u8g2,
                                                         const vario_viewport_t *v,
                                                         const vario_runtime_t *rt)
{
    char    flight_time[24];
    int16_t text_w;
    int16_t draw_x;
    int16_t top_y;

    if ((u8g2 == NULL) || (v == NULL) || (rt == NULL))
    {
        return;
    }

    vario_display_format_flight_time(flight_time, sizeof(flight_time), rt);
    top_y = vario_display_get_bottom_center_flight_time_top_y(u8g2, v);

    u8g2_SetFontPosTop(u8g2);
    u8g2_SetFont(u8g2, VARIO_UI_FONT_ALT2_VALUE);
    text_w = (int16_t)u8g2_GetStrWidth(u8g2, flight_time);
    draw_x = (int16_t)(v->x + (v->w / 2) - (text_w / 2));
    u8g2_DrawStr(u8g2, draw_x, top_y, flight_time);
    u8g2_SetFontPosBaseline(u8g2);
}


static void vario_display_format_alt2_text(char *value_buf,
                                           size_t value_len,
                                           char *unit_buf,
                                           size_t unit_len,
                                           const vario_runtime_t *rt,
                                           const vario_settings_t *settings)
{
    const app_altitude_linear_units_t *gps_units;
    long                               fl_value;

    if ((value_buf == NULL) || (value_len == 0u) || (unit_buf == NULL) || (unit_len == 0u) ||
        (rt == NULL) || (settings == NULL))
    {
        return;
    }

    gps_units = &rt->altitude.units.alt_gps_hmsl;

    switch (settings->alt2_mode)
    {
        case VARIO_ALT2_MODE_ABSOLUTE:
            /* ------------------------------------------------------------------ */
            /* ALT2媛 absolute mode???뚮룄 ALT1怨?媛숈? 5Hz filtered absolute       */
            /* ?レ옄瑜??ъ슜?쒕떎.                                                    */
            /*                                                                    */
            /* ?꾩옱 Alt1? legal/competition??barometric path濡?怨좎젙?대?濡?       */
            /* ?ш린?쒕뒗 洹??レ옄瑜?洹몃?濡?蹂듭젣?댁꽌 蹂댁뿬 二쇨린留??쒕떎.              */
            /* ------------------------------------------------------------------ */
            vario_display_format_signed_altitude_5slot_from_meters(value_buf,
                                                                   value_len,
                                                                   true,
                                                                   rt->baro_altitude_m,
                                                                   settings->alt2_unit);
            snprintf(unit_buf,
                     unit_len,
                     "%s",
                     Vario_Settings_GetAltitudeUnitTextForUnit(settings->alt2_unit));
            break;

        case VARIO_ALT2_MODE_SMART_FUSE:
        {
            const app_altitude_linear_units_t *smart_fuse_units;

            smart_fuse_units = vario_display_select_smart_fuse_units(rt);
            if (smart_fuse_units != NULL)
            {
                vario_display_format_signed_altitude_5slot_from_unit_bank(value_buf,
                                                                              value_len,
                                                                              true,
                                                                              smart_fuse_units,
                                                                              settings->alt2_unit);
            }
            else
            {
                snprintf(value_buf, value_len, "-----");
            }
            snprintf(unit_buf,
                     unit_len,
                     "%s",
                     Vario_Settings_GetAltitudeUnitTextForUnit(settings->alt2_unit));
            break;
        }

        case VARIO_ALT2_MODE_GPS:
            if (rt->gps_valid != false)
            {
                vario_display_format_signed_altitude_5slot_from_unit_bank(value_buf,
                                                                              value_len,
                                                                              true,
                                                                              gps_units,
                                                                              settings->alt2_unit);
            }
            else
            {
                snprintf(value_buf, value_len, "-----");
            }
            snprintf(unit_buf,
                     unit_len,
                     "%s",
                     Vario_Settings_GetAltitudeUnitTextForUnit(settings->alt2_unit));
            break;

        case VARIO_ALT2_MODE_FLIGHT_LEVEL:
            if (rt->baro_valid != false)
            {
                fl_value = vario_display_get_flight_level_from_unit_bank(rt);
                snprintf(value_buf, value_len, "%03ld", fl_value);
            }
            else
            {
                snprintf(value_buf, value_len, "---");
            }
            snprintf(unit_buf, unit_len, "FL");
            break;

        case VARIO_ALT2_MODE_RELATIVE:
        case VARIO_ALT2_MODE_COUNT:
        default:
            /* ------------------------------------------------------------------ */
            /*  ALT2 relative??湲곗??먯씠 VARIO app setting ???덉쑝誘濡?            */
            /*  APP_STATE???뺤쟻 unit bank??洹몃?濡?議댁옱???섎뒗 ?녿떎.              */
            /*                                                                    */
            /*  ???backing field ?먯껜瑜?Vario_State.c ?먯꽌                      */
            /*  centimeter ?댁긽?꾨줈 怨꾩궛???먯뿀湲??뚮Ц??                          */
            /*  ?ш린??feet濡?蹂?섑빐??1m 怨꾨떒 ?댁긽?꾩뿉 臾띠씠吏 ?딅뒗??             */
            /* ------------------------------------------------------------------ */
            vario_display_format_signed_altitude_5slot_from_meters(value_buf,
                                                                   value_len,
                                                                   true,
                                                                   rt->alt2_relative_m,
                                                                   settings->alt2_unit);
            snprintf(unit_buf,
                     unit_len,
                     "%s",
                     Vario_Settings_GetAltitudeUnitTextForUnit(settings->alt2_unit));
            break;
    }
}

static void vario_display_draw_top_right_altitudes(u8g2_t *u8g2,
                                                   const vario_viewport_t *v,
                                                   const vario_runtime_t *rt)
{
    const vario_settings_t *settings;
    char    alt1_text[24];
    char    alt2_text[24];
    char    alt3_text[24];
    char    alt2_unit_buf[8];
    const char *alt1_unit;
    const char *alt2_unit;
    const char *alt3_unit;
    int16_t right_limit_x;
    int16_t unit_box_w;
    int16_t alt1_value_box_w;
    int16_t alt23_value_box_w;
    int16_t alt1_value_h;
    int16_t alt23_value_h;
    int16_t unit_h;
    int16_t alt1_row_h;
    int16_t alt23_row_h;
    int16_t alt1_top_y;
    int16_t alt2_top_y;
    int16_t alt3_top_y;
    int16_t value_x;
    int16_t unit_x;
    int16_t icon_x;

    if ((u8g2 == NULL) || (v == NULL) || (rt == NULL))
    {
        return;
    }

    settings = Vario_Settings_Get();
    if (settings == NULL)
    {
        return;
    }

    right_limit_x = (int16_t)(v->x + v->w - VARIO_UI_SIDE_BAR_W - VARIO_UI_TOP_RIGHT_PAD_X);

    vario_display_format_filtered_selected_altitude(alt1_text,
                                                    sizeof(alt1_text),
                                                    rt,
                                                    settings->altitude_unit);
    vario_display_format_alt2_text(alt2_text, sizeof(alt2_text), alt2_unit_buf, sizeof(alt2_unit_buf), rt, settings);
    vario_display_format_signed_altitude_5slot_from_meters(alt3_text,
                                                       sizeof(alt3_text),
                                                       true,
                                                       rt->alt3_accum_gain_m,
                                                       settings->altitude_unit);
    alt1_unit = Vario_Settings_GetAltitudeUnitTextForUnit(settings->altitude_unit);
    alt2_unit = alt2_unit_buf;
    alt3_unit = Vario_Settings_GetAltitudeUnitTextForUnit(settings->altitude_unit);

    unit_box_w = vario_display_measure_text(u8g2, VARIO_UI_FONT_ALT1_UNIT, "ft");
    {
        int16_t meter_w;
        int16_t fl_w;
        meter_w = vario_display_measure_text(u8g2, VARIO_UI_FONT_ALT1_UNIT, "m");
        fl_w = vario_display_measure_text(u8g2, VARIO_UI_FONT_ALT1_UNIT, "FL");
        if (unit_box_w < meter_w)
        {
            unit_box_w = meter_w;
        }
        if (unit_box_w < fl_w)
        {
            unit_box_w = fl_w;
        }
    }

    alt1_value_box_w = vario_display_measure_text(u8g2, VARIO_UI_FONT_ALT1_VALUE, "88888");
    alt23_value_box_w = vario_display_measure_text(u8g2, VARIO_UI_FONT_ALT2_VALUE, "-8888");
    {
        int16_t unsigned_w;
        int16_t fl_w;
        unsigned_w = vario_display_measure_text(u8g2, VARIO_UI_FONT_ALT2_VALUE, "88888");
        fl_w = vario_display_measure_text(u8g2, VARIO_UI_FONT_ALT2_VALUE, "000");
        if (alt23_value_box_w < unsigned_w)
        {
            alt23_value_box_w = unsigned_w;
        }
        if (alt23_value_box_w < fl_w)
        {
            alt23_value_box_w = fl_w;
        }
    }

    alt1_value_h = vario_display_get_font_height(u8g2, VARIO_UI_FONT_ALT1_VALUE);
    alt23_value_h = vario_display_get_font_height(u8g2, VARIO_UI_FONT_ALT2_VALUE);
    unit_h = vario_display_get_font_height(u8g2, VARIO_UI_FONT_ALT1_UNIT);

    alt1_row_h = alt1_value_h;
    if (alt1_row_h < unit_h)
    {
        alt1_row_h = unit_h;
    }

    alt23_row_h = alt23_value_h;
    if (alt23_row_h < (int16_t)VARIO_ICON_ALT2_HEIGHT)
    {
        alt23_row_h = VARIO_ICON_ALT2_HEIGHT;
    }
    if (alt23_row_h < unit_h)
    {
        alt23_row_h = unit_h;
    }

    alt1_top_y = (int16_t)(v->y + VARIO_UI_TOP_ALT1_TOP_Y);
    alt2_top_y = (int16_t)(alt1_top_y + alt1_row_h + VARIO_UI_ALT_ROW_GAP_Y);
    alt3_top_y = (int16_t)(alt2_top_y + alt23_row_h + VARIO_UI_ALT_ROW_GAP_Y);

    unit_x = (int16_t)(right_limit_x - unit_box_w);
    value_x = (int16_t)(unit_x - VARIO_UI_TOP_ALT1_VALUE_UNIT_GAP - alt1_value_box_w - VARIO_UI_ALT1_FIXED_VALUE_X_SHIFT_PX);
    /* ------------------------------------------------------------------ */
    /* ALT1 ???レ옄 fixed-slot render                                       */
    /*                                                                    */
    /* 理쒖떊 ?붽뎄?ы빆                                                         */
    /* 1) ?뚯닔 怨좊룄?먯꽌??5-column 怨꾩빟??源⑥?吏 ?딄쾶 ?쒕떎.                  */
    /* 2) 怨좎젙 ??怨좎젙 ?レ옄 ?쒖떆 ?곸뿭 ?먯껜瑜?X異?湲곗? 4 px ?쇱そ?쇰줈 類??     */
    /*    ?⑥쐞(unit) absolute position ? 洹몃?濡??붾떎.                      */
    /*                                                                    */
    /* ?곕씪??ALT1? unsigned-only helper 媛 ?꾨땲??                         */
    /* signed altitude ?꾩슜 slot renderer 濡?洹몃┛??                         */
    /* ------------------------------------------------------------------ */
    vario_display_draw_logisoso_signed_altitude_value(u8g2,
                                                      value_x,
                                                      (int16_t)(alt1_top_y + ((alt1_row_h - alt1_value_h) / 2)),
                                                      alt1_value_box_w,
                                                      VARIO_UI_ALIGN_RIGHT,
                                                      alt1_text,
                                                      VARIO_UI_ALT1_FIXED_SLOT_COUNT);
    vario_display_draw_text_box_top(u8g2,
                                    unit_x,
                                    (int16_t)(alt1_top_y + ((alt1_row_h - unit_h) / 2)),
                                    unit_box_w,
                                    VARIO_UI_ALIGN_LEFT,
                                    VARIO_UI_FONT_ALT1_UNIT,
                                    alt1_unit);

    unit_x = (int16_t)(right_limit_x - unit_box_w);
    value_x = (int16_t)(unit_x - VARIO_UI_TOP_ALT_ROW_VALUE_UNIT_GAP - alt23_value_box_w);
    icon_x = (int16_t)(value_x - VARIO_UI_TOP_ALT_ROW_ICON_GAP - VARIO_ICON_ALT2_WIDTH);

    vario_display_draw_alt_badge(u8g2,
                                 icon_x,
                                 (int16_t)(alt2_top_y + ((alt23_row_h - VARIO_ICON_ALT2_HEIGHT) / 2)),
                                 '2');
    vario_display_draw_text_box_top(u8g2,
                                    value_x,
                                    (int16_t)(alt2_top_y + ((alt23_row_h - alt23_value_h) / 2)),
                                    alt23_value_box_w,
                                    VARIO_UI_ALIGN_RIGHT,
                                    VARIO_UI_FONT_ALT2_VALUE,
                                    alt2_text);
    vario_display_draw_text_box_top(u8g2,
                                    unit_x,
                                    (int16_t)(alt2_top_y + ((alt23_row_h - unit_h) / 2)),
                                    unit_box_w,
                                    VARIO_UI_ALIGN_LEFT,
                                    VARIO_UI_FONT_ALT2_UNIT,
                                    alt2_unit);

    unit_x = (int16_t)(right_limit_x - unit_box_w);
    value_x = (int16_t)(unit_x - VARIO_UI_TOP_ALT_ROW_VALUE_UNIT_GAP - alt23_value_box_w);
    icon_x = (int16_t)(value_x - VARIO_UI_TOP_ALT_ROW_ICON_GAP - VARIO_ICON_ALT3_WIDTH);

    vario_display_draw_alt_badge(u8g2,
                                 icon_x,
                                 (int16_t)(alt3_top_y + ((alt23_row_h - VARIO_ICON_ALT3_HEIGHT) / 2)),
                                 '3');
    vario_display_draw_text_box_top(u8g2,
                                    value_x,
                                    (int16_t)(alt3_top_y + ((alt23_row_h - alt23_value_h) / 2)),
                                    alt23_value_box_w,
                                    VARIO_UI_ALIGN_RIGHT,
                                    VARIO_UI_FONT_ALT3_VALUE,
                                    alt3_text);
    vario_display_draw_text_box_top(u8g2,
                                    unit_x,
                                    (int16_t)(alt3_top_y + ((alt23_row_h - unit_h) / 2)),
                                    unit_box_w,
                                    VARIO_UI_ALIGN_LEFT,
                                    VARIO_UI_FONT_ALT3_UNIT,
                                    alt3_unit);
}

static void vario_display_draw_vario_side_bar(u8g2_t *u8g2,
                                              const vario_viewport_t *v,
                                              float instant_vario_mps,
                                              float average_vario_mps)
{
    const vario_settings_t *settings;
    uint8_t                 tick_halfstep_index;
    uint8_t                 halfstep_count;
    int16_t                 left_bar_x;
    int16_t                 instant_x;
    int16_t                 avg_x;
    int16_t                 tick_x;
    int16_t                 top_limit_y;
    int16_t                 zero_top_y;
    int16_t                 zero_bottom_y;
    int16_t                 bottom_limit_y;
    int16_t                 top_span_px;
    int16_t                 bottom_span_px;
    uint8_t                 thick_i;

    if ((u8g2 == NULL) || (v == NULL))
    {
        return;
    }

    settings = Vario_Settings_Get();
    halfstep_count = vario_display_get_vario_halfstep_count(settings);

    left_bar_x = v->x;
    instant_x  = (int16_t)(left_bar_x + 1);   /* 湲곗〈 instant bar X ?좎? */
    avg_x      = (int16_t)(left_bar_x + 10);  /* 湲곗〈 average bar X ?좎? */
    tick_x     = left_bar_x;                  /* 湲곗〈 tick ?쒖옉 X ?좎? */

    /* ---------------------------------------------------------------------- */
    /*  醫뚯륫 VARIO ??viewport height 瑜??곗? ?딄퀬 ?ㅼ젣 LCD ?꾩껜 ?믪씠瑜??대떎.  */
    /*                                                                        */
    /*  - 4.0 scale : ?붾㈃ 留???+4.0 / 留??꾨옒 -4.0                          */
    /*  - 5.0 scale : ?붾㈃ 留???+5.0 / 留??꾨옒 -5.0                          */
    /*  - 媛?대뜲 3 px zero band ??湲곗〈 洹몃?濡??좎?                           */
    /* ---------------------------------------------------------------------- */
    vario_display_get_vario_screen_geometry(&top_limit_y,
                                            &zero_top_y,
                                            &zero_bottom_y,
                                            &bottom_limit_y,
                                            &top_span_px,
                                            &bottom_span_px);

    /* ---------------------------------------------------------------------- */
    /*  tick Y ??떆 bar fill 怨?媛숈? scale ?앹쓣 怨듭쑀?댁빞                      */
    /*  ?レ옄 ?ㅼ??쇨낵 ?ㅼ젣 fill ???쒕줈 ??댁?吏 ?딅뒗??                      */
    /*                                                                        */
    /*  - 0.5 ?⑥쐞 : small tick                                                */
    /*  - 1.0 ?⑥쐞 : major tick                                                */
    /*  - tick ??X ?쒖옉??/ 湲몄씠 / 紐⑥뼇? 湲곗〈 洹몃?濡??좎?                   */
    /* ---------------------------------------------------------------------- */
    for (tick_halfstep_index = 1u; tick_halfstep_index <= halfstep_count; ++tick_halfstep_index)
    {
        uint8_t tick_w;
        int16_t up_offset_px;
        int16_t down_offset_px;
        int16_t up_y;
        int16_t down_y;

        tick_w = ((tick_halfstep_index % 2u) == 0u) ? VARIO_UI_SCALE_MAJOR_W
                                                    : VARIO_UI_SCALE_MINOR_W;

        up_offset_px = (int16_t)lroundf((((float)tick_halfstep_index) / (float)halfstep_count) *
                                        (float)top_span_px);
        down_offset_px = (int16_t)lroundf((((float)tick_halfstep_index) / (float)halfstep_count) *
                                          (float)bottom_span_px);

        up_y = (int16_t)(zero_top_y - up_offset_px);
        down_y = (int16_t)(zero_bottom_y + down_offset_px);

        if (up_y < top_limit_y)
        {
            up_y = top_limit_y;
        }
        if (up_y > bottom_limit_y)
        {
            up_y = bottom_limit_y;
        }
        if (down_y < top_limit_y)
        {
            down_y = top_limit_y;
        }
        if (down_y > bottom_limit_y)
        {
            down_y = bottom_limit_y;
        }

        u8g2_DrawHLine(u8g2, tick_x, up_y, tick_w);
        u8g2_DrawHLine(u8g2, tick_x, down_y, tick_w);
    }

    /* ---------------------------------------------------------------------- */
    /*  fill draw                                                              */
    /*                                                                        */
    /*  - scale ?덉そ : ?곗냽媛?-> pixel 濡??섏궛                                 */
    /*  - scale 珥덇낵 : 以묒떖異뺣???0.5 m/s slot ????移몄뵫 吏?곕뒗 ?⑦꽩         */
    /* ---------------------------------------------------------------------- */
    vario_display_draw_vario_column(u8g2,
                                    instant_x,
                                    VARIO_UI_GAUGE_INSTANT_W,
                                    instant_vario_mps,
                                    settings);
    vario_display_draw_vario_column(u8g2,
                                    avg_x,
                                    VARIO_UI_GAUGE_AVG_W,
                                    average_vario_mps,
                                    settings);

    /* center zero line ? fill ???ㅼ떆 ??뼱 洹몃젮 湲곗??좎씠 ??긽 ?댁븘 ?덇쾶 ?좎? */
    for (thick_i = 0u; thick_i < VARIO_UI_VARIO_ZERO_LINE_THICKNESS; ++thick_i)
    {
        int16_t zero_y;

        zero_y = (int16_t)(zero_top_y + (int16_t)thick_i);
        if ((zero_y >= top_limit_y) && (zero_y <= bottom_limit_y))
        {
            u8g2_DrawHLine(u8g2, left_bar_x, zero_y, VARIO_UI_VARIO_ZERO_LINE_W);
        }
    }
}

static void vario_display_draw_gs_side_bar(u8g2_t *u8g2,
                                           const vario_viewport_t *v,
                                           const vario_runtime_t *rt,
                                           float instant_speed_kmh,
                                           float average_speed_kmh)
{
    const vario_settings_t *settings;
    uint16_t                tick_index;
    int16_t                 right_bar_x;
    int16_t                 instant_x;
    int16_t                 avg_icon_x;
    int16_t                 fill_h;
    int16_t                 tick_y;
    int16_t                 avg_center_y;
    int16_t                 avg_icon_y;
    int16_t                 mc_center_y;
    int16_t                 mc_icon_y;
    float                   clamped_speed;
    float                   clamped_avg_speed;
    float                   clamped_mc_speed;
    float                   gs_top_kmh;
    float                   minor_step_kmh;
    float                   major_step_kmh;
    uint16_t                tick_count;
    uint8_t                 tick_w;
    int16_t                 tick_x;
    bool                    gps_fix_usable;

    if ((u8g2 == NULL) || (v == NULL))
    {
        return;
    }

    settings = Vario_Settings_Get();
    gs_top_kmh = vario_display_get_gs_scale_kmh(settings);
    minor_step_kmh = vario_display_get_gs_minor_step_kmh(gs_top_kmh);
    major_step_kmh = vario_display_get_gs_major_step_kmh(gs_top_kmh);
    tick_count = vario_display_get_gs_minor_tick_count(gs_top_kmh, minor_step_kmh);

    right_bar_x = (int16_t)(v->x + v->w - VARIO_UI_SIDE_BAR_W);
    instant_x = (int16_t)(right_bar_x + 5);
    avg_icon_x = right_bar_x;
    gps_fix_usable = vario_display_runtime_has_usable_gps_fix(rt);

    for (tick_index = 1u; tick_index <= tick_count; ++tick_index)
    {
        float   tick_value_kmh;
        float   ratio;
        float   major_ratio;

        tick_value_kmh = ((float)tick_index) * minor_step_kmh;
        ratio = tick_value_kmh / gs_top_kmh;
        major_ratio = tick_value_kmh / major_step_kmh;

        tick_w = (fabsf(major_ratio - roundf(major_ratio)) < 0.001f) ?
            VARIO_UI_SCALE_MAJOR_W :
            VARIO_UI_SCALE_MINOR_W;
        tick_x = (int16_t)(right_bar_x + VARIO_UI_SIDE_BAR_W - tick_w);
        tick_y = (int16_t)(v->y + v->h - 1 - lroundf(ratio * (float)(v->h - 1)));
        if (tick_y < v->y)
        {
            tick_y = v->y;
        }
        if (tick_y > (v->y + v->h - 1))
        {
            tick_y = (int16_t)(v->y + v->h - 1);
        }

        u8g2_DrawHLine(u8g2, tick_x, tick_y, tick_w);
    }

    clamped_speed = vario_display_clampf(instant_speed_kmh, 0.0f, gs_top_kmh);
    if (gps_fix_usable == false)
    {
        /* ------------------------------------------------------------------ */
        /* GPS INOP ?쒖뿉???ㅼ냽??instant lane ?꾩껜瑜?checker dither 濡?梨꾩슫?? */
        /*                                                                    */
        /* ?ъ슜?먭? ?붽뎄???곸뿭? "?ㅼ젣 GS instant bar 媛 plot ?섎뒗 ?뺥솗??   */
        /* X lane" ?대?濡? 湲곗〈 instant_x / width 瑜?洹몃?濡??ъ슜?섍퀬         */
        /* viewport Y ???믪씠瑜?苑?梨꾩슫??                                     */
        /* ------------------------------------------------------------------ */
        vario_display_draw_checker_dither_box(u8g2,
                                              instant_x,
                                              v->y,
                                              VARIO_UI_GAUGE_INSTANT_W,
                                              v->h);
    }
    else if (clamped_speed > 0.0f)
    {
        fill_h = vario_display_scale_value_to_fill_px(clamped_speed,
                                                      gs_top_kmh,
                                                      v->h);
        if (fill_h > v->h)
        {
            fill_h = v->h;
        }
        if (fill_h > 0)
        {
            u8g2_DrawBox(u8g2,
                         instant_x,
                         (int16_t)(v->y + v->h - fill_h),
                         VARIO_UI_GAUGE_INSTANT_W,
                         fill_h);
        }
    }

    /* ---------------------------------------------------------------------- */
    /* average GS marker                                                       */
    /*                                                                        */
    /* ?됯퇏 ?띾룄 marker ??湲곗〈怨??숈씪?섍쾶 speed bar ??醫뚯륫 lane ???대떎.     */
    /* ---------------------------------------------------------------------- */
    clamped_avg_speed = vario_display_clampf(average_speed_kmh, 0.0f, gs_top_kmh);
    if ((gps_fix_usable != false) && (clamped_avg_speed > 0.0f))
    {
        avg_center_y = vario_display_get_gs_value_center_y(v,
                                                           clamped_avg_speed,
                                                           gs_top_kmh);
        avg_icon_y = vario_display_get_marker_top_y(avg_center_y,
                                                    VARIO_ICON_GS_AVG_HEIGHT,
                                                    v->y,
                                                    (int16_t)(v->y + v->h));
        vario_display_draw_xbm(u8g2,
                               avg_icon_x,
                               avg_icon_y,
                               VARIO_ICON_GS_AVG_WIDTH,
                               VARIO_ICON_GS_AVG_HEIGHT,
                               vario_icon_gs_avg_bits);
    }

    /* ---------------------------------------------------------------------- */
    /* McCready / STF marker                                                   */
    /*                                                                        */
    /* ?ъ슜?먭? ?붿껌???먭? 寃곌낵                                              */
    /* - ?꾩슜 XBM ? ?뺤쓽留??섏뼱 ?덇퀬 ?ㅼ젣 draw path 媛 鍮좎졇 ?덉뿀??           */
    /* - ?댁젣 speed_to_fly_valid 媛 ?댁븘 ?덉쑝硫???긽 媛숈? 醫뚯륫 lane ??draw ?쒕떎.*/
    /* - speed_to_fly_kmh 媛 GS graph ?곷떒媛믪쓣 ?섏쑝硫?marker ?꾩튂留?           */
    /*   top tick ??clamp ?섏뿬 "??蹂댁씠?? ?꾩긽??留됰뒗??                    */
    /* ---------------------------------------------------------------------- */
    if ((rt != NULL) && (rt->speed_to_fly_valid != false))
    {
        clamped_mc_speed = vario_display_clampf(rt->speed_to_fly_kmh, 0.0f, gs_top_kmh);
        if (clamped_mc_speed > 0.0f)
        {
            mc_center_y = vario_display_get_gs_value_center_y(v,
                                                              clamped_mc_speed,
                                                              gs_top_kmh);
            mc_icon_y = vario_display_get_marker_top_y(mc_center_y,
                                                       VARIO_ICON_MC_SPEED_HEIGHT,
                                                       v->y,
                                                       (int16_t)(v->y + v->h));
            vario_display_draw_xbm(u8g2,
                                   avg_icon_x,
                                   mc_icon_y,
                                   VARIO_ICON_MC_SPEED_WIDTH,
                                   VARIO_ICON_MC_SPEED_HEIGHT,
                                   vario_icon_mc_speed_bits);
        }
    }
}

static void vario_display_draw_vario_value_block(u8g2_t *u8g2,
                                                 const vario_viewport_t *v,
                                                 const vario_runtime_t *rt)
{
    const vario_settings_t *settings;
    char    value_text[20];
    char    max_text[20];
    char    te_value_text[20];
    int16_t value_box_x;
    int16_t value_box_y;
    int16_t value_box_h;
    int16_t max_box_x;
    int16_t max_box_y;
    int16_t max_box_h;
    int16_t te_meta_x;
    int16_t te_meta_y;
    int16_t te_value_x;
    int16_t te_value_y;
    int16_t te_value_w;
    int16_t te_label_y;
    int16_t te_label_h;
    int16_t te_label_box_w;
    uint8_t te_value_in_meta_row;

    if ((u8g2 == NULL) || (v == NULL) || (rt == NULL))
    {
        return;
    }

    settings = Vario_Settings_Get();

    value_box_x = (int16_t)(v->x + VARIO_UI_SIDE_BAR_W + VARIO_UI_BOTTOM_VARIO_X_PAD);
    value_box_h = vario_display_get_font_height(u8g2, VARIO_UI_FONT_BOTTOM_MAIN);
    if (value_box_h <= 0)
    {
        value_box_h = VARIO_UI_BOTTOM_BOX_H;
    }
    value_box_y = vario_display_get_vario_main_value_box_y(u8g2, v);

    vario_display_format_vario_value_abs(value_text, sizeof(value_text), rt->baro_vario_mps);

    /* ---------------------------------------------------------------------- */
    /* ?꾩옱 ??VARIO 媛믪? leading zero ?꾩껜瑜?吏?곗? ?딅뒗??                    */
    /* - ?ъ슜?먭? ?먰븯??嫄?".5" 媛 ?꾨땲??"[blank]0 5" 援ъ“??               */
    /* - 利? tens slot 留?鍮덉뭏?쇰줈 ?먭퀬 decimal 吏곸쟾 ones zero ???대┛??       */
    /* - 洹몃옒???ш린?쒕뒗 trim helper 瑜??몄텧?섏? ?딄퀬, draw ?④퀎?먯꽌            */
    /*   fixed-slot renderer 媛 tens slot 留??④린寃??쒕떎.                      */
    /* ---------------------------------------------------------------------- */
    vario_display_format_peak_vario(max_text, sizeof(max_text), rt->max_top_vario_mps);

    te_meta_x = (int16_t)(v->x + VARIO_UI_SIDE_BAR_W + 2 + VARIO_UI_BOTTOM_META_BOX_W + 3);
    te_value_in_meta_row = 0u;

    max_box_h = vario_display_get_font_height(u8g2, VARIO_UI_FONT_BOTTOM_MAX_VALUE);
    if (max_box_h <= 0)
    {
        max_box_h = 8;
    }
    max_box_x = (int16_t)(v->x + VARIO_UI_SIDE_BAR_W + 2);
    max_box_y = (int16_t)(value_box_y - max_box_h - VARIO_UI_BOTTOM_META_GAP_Y);

    te_value_w = VARIO_UI_BOTTOM_META_BOX_W;
    te_label_box_w = vario_display_measure_text(u8g2, VARIO_UI_FONT_ALT2_UNIT, "-99.9");
    te_meta_x = (int16_t)(max_box_x + VARIO_UI_BOTTOM_META_BOX_W + 3);
    te_meta_y = max_box_y;
    te_label_h = vario_display_get_font_height(u8g2, VARIO_UI_FONT_ALT2_UNIT);
    if (te_label_h <= 0)
    {
        te_label_h = 8;
    }
    te_label_y = (int16_t)(te_meta_y - te_label_h - 2);

    if ((settings == NULL) || (settings->show_max_vario != 0u))
    {
        vario_display_draw_text_box_top(u8g2,
                                        max_box_x,
                                        max_box_y,
                                        VARIO_UI_BOTTOM_META_BOX_W,
                                        VARIO_UI_ALIGN_LEFT,
                                        VARIO_UI_FONT_BOTTOM_MAX_VALUE,
                                        max_text);

        /* ------------------------------------------------------------------ */
        /* Est.TE compact block                                                */
        /*                                                                      */
        /* 理쒖떊 ?붽뎄?ы빆                                                        */
        /* - ?쇰꺼 "Est.TE" ??湲곗〈 absolute ?꾩튂瑜?洹몃?濡??좎??쒕떎.            */
        /* - ?ㅼ젣 媛믪? MAX VARIO ? ?숈씪??top Y / ?숈씪???고듃濡??쒖떆?쒕떎.      */
        /* - 媛?釉붾줉 ??룄 MAX VARIO ? 媛숈? 怨좎젙 ??쓣 ?ъ슜?섍퀬, right align ?쒕떎.*/
        /* ------------------------------------------------------------------ */
        vario_display_draw_text_box_top(u8g2,
                                        te_meta_x,
                                        te_label_y,
                                        te_label_box_w,
                                        VARIO_UI_ALIGN_RIGHT,
                                        VARIO_UI_FONT_ALT2_UNIT,
                                        "Est.TE");

        vario_display_format_estimated_te_value(te_value_text,
                                                sizeof(te_value_text),
                                                rt);
        vario_display_draw_text_box_top(u8g2,
                                        (int16_t)(te_meta_x - 10),
                                        te_meta_y,
                                        te_value_w,
                                        VARIO_UI_ALIGN_RIGHT,
                                        VARIO_UI_FONT_BOTTOM_MAX_VALUE,
                                        te_value_text);
        te_value_in_meta_row = 1u;
    }

    vario_display_draw_fixed_vario_current_value(u8g2,
                                                 value_box_x,
                                                 value_box_y,
                                                 VARIO_UI_BOTTOM_BOX_W,
                                                 value_box_h,
                                                 value_text,
                                                 rt->baro_vario_mps);

    if (te_value_in_meta_row == 0u)
    {
        /* ------------------------------------------------------------------ */
        /* show_max_vario 媛 爰쇱졇 ?덉뼱??                                       */
        /* Est.TE 媛믪쓽 ?고듃 / Y / 怨좎젙??怨꾩빟? ?숈씪?섍쾶 ?좎??쒕떎.              */
        /* ------------------------------------------------------------------ */
        vario_display_draw_text_box_top(u8g2,
                                        te_meta_x,
                                        te_label_y,
                                        te_label_box_w,
                                        VARIO_UI_ALIGN_RIGHT,
                                        VARIO_UI_FONT_ALT2_UNIT,
                                        "Est.TE");
        vario_display_format_estimated_te_value(te_value_text,
                                                sizeof(te_value_text),
                                                rt);
        te_value_x = (int16_t)(te_meta_x - 10);
        te_value_y = max_box_y;
        vario_display_draw_text_box_top(u8g2,
                                        te_value_x,
                                        te_value_y,
                                        te_value_w,
                                        VARIO_UI_ALIGN_RIGHT,
                                        VARIO_UI_FONT_BOTTOM_MAX_VALUE,
                                        te_value_text);
    }

    (void)settings;
}



static void vario_display_draw_speed_value_block(u8g2_t *u8g2,
                                                 const vario_viewport_t *v,
                                                 const vario_runtime_t *rt)
{
    char    value_text[20];
    char    max_text[20];
    char    mc_text[20];
    int16_t value_box_x;
    int16_t value_box_y;
    int16_t value_box_h;
    int16_t max_box_x;
    int16_t max_box_y;
    int16_t max_box_h;
    int16_t mc_box_x;
    int16_t mc_box_w;
    int16_t mc_label_w;
    int16_t mc_label_h;
    int16_t mc_label_x;
    int16_t mc_label_y;

    if ((u8g2 == NULL) || (v == NULL) || (rt == NULL))
    {
        return;
    }

    value_box_h = vario_display_get_font_height(u8g2, VARIO_UI_FONT_BOTTOM_MAIN);
    if (value_box_h <= 0)
    {
        value_box_h = VARIO_UI_BOTTOM_BOX_H;
    }

    value_box_x = (int16_t)(v->x + v->w - VARIO_UI_SIDE_BAR_W - VARIO_UI_BOTTOM_GS_X_PAD - VARIO_UI_BOTTOM_BOX_W);

    /* ------------------------------------------------------------------ */
    /* ?ъ슜?먭? 吏?뺥븳 4媛??붿냼瑜??섎굹??speed block ?쇰줈 媛숈씠 4px ?대┛?? */
    /*                                                                    */
    /* ?대룞 ???                                                           */
    /* - "MC" ?쇰꺼                                                         */
    /* - MC 媛?                                                            */
    /* - 理쒕? ?띾룄                                                         */
    /* - ?꾩옱 ?띾룄 蹂멸컪/?뚯닔媛?                                             */
    /*                                                                    */
    /* 援ы쁽 諛⑹묠                                                            */
    /* - 蹂꾨룄 shift ?곸닔???고???蹂댁젙 濡쒖쭅??異붽??섏? ?딅뒗??             */
    /* - ??speed block ??湲곗? ?덈? 醫뚰몴???먯껜瑜?援먯껜?쒕떎.              */
    /*                                                                    */
    /* 湲곗〈 ?앹? bottom pad(4px)瑜?鍮쇨퀬 ?덉뼱?? ?붾㈃ ?섎떒?먯꽌 4px ???덉뿀??*/
    /* 洹?-4 ?깃꺽??蹂댁젙???쒓굅?댁꽌 湲곗? y 瑜?洹몃?濡??꾨옒濡?4px ?대┛??   */
    /* max / MC ??value_box_y 瑜?湲곗??쇰줈 ?꾩そ??遺숈뼱 ?덉쑝誘濡??④퍡 ?대룞?쒕떎.*/
    /* ------------------------------------------------------------------ */
    value_box_y = (int16_t)(v->y + v->h - value_box_h);

    if (vario_display_runtime_has_usable_gps_fix(rt) != false)
    {
        vario_display_format_speed_value(value_text,
                                         sizeof(value_text),
                                         (s_vario_ui_dynamic.displayed_speed_valid != false) ?
                                             s_vario_ui_dynamic.displayed_speed_kmh :
                                             rt->ground_speed_kmh);
    }
    else
    {
        snprintf(value_text, sizeof(value_text), "--.-");
    }
    vario_display_format_peak_speed(max_text, sizeof(max_text), s_vario_ui_dynamic.top_speed_kmh);
    vario_display_format_optional_speed(mc_text,
                                        sizeof(mc_text),
                                        rt->speed_to_fly_valid,
                                        rt->speed_to_fly_kmh);

    max_box_h = vario_display_get_font_height(u8g2, VARIO_UI_FONT_BOTTOM_MAX_VALUE);
    if (max_box_h <= 0)
    {
        max_box_h = 8;
    }
    max_box_x = (int16_t)(v->x + v->w - VARIO_UI_SIDE_BAR_W - 2 - VARIO_UI_BOTTOM_META_BOX_W);
    max_box_y = (int16_t)(value_box_y - max_box_h - VARIO_UI_BOTTOM_META_GAP_Y);

    /* ---------------------------------------------------------------------- */
    /* McCready/STF speed text                                                 */
    /*                                                                        */
    /* - top speed max block ???쇱そ??蹂꾨룄 fixed box 瑜??좎??쒕떎.              */
    /* - ?ъ슜?먭? ?붿껌???濡? ??諛뺤뒪???쇱そ?먮뒗 ?묒? "MC" ?쇰꺼??            */
    /*   ALT2/ALT3 unit 怨??숈씪???고듃濡?遺숈씤??                               */
    /* ---------------------------------------------------------------------- */
    mc_box_w = VARIO_UI_BOTTOM_META_BOX_W;
    mc_box_x = (int16_t)(max_box_x - 3 - mc_box_w);
    mc_label_w = vario_display_measure_text(u8g2, VARIO_UI_FONT_ALT2_UNIT, "MC");
    mc_label_h = vario_display_get_font_height(u8g2, VARIO_UI_FONT_ALT2_UNIT);
    if (mc_label_h <= 0)
    {
        mc_label_h = 7;
    }
    mc_label_x = (int16_t)(mc_box_x - 2 - mc_label_w + 4);
    mc_label_y = (int16_t)(max_box_y + ((max_box_h - mc_label_h) / 2));

    vario_display_draw_text_box_top(u8g2,
                                    mc_label_x,
                                    mc_label_y,
                                    mc_label_w,
                                    VARIO_UI_ALIGN_LEFT,
                                    VARIO_UI_FONT_ALT2_UNIT,
                                    "MC");
    vario_display_draw_text_box_top(u8g2,
                                    mc_box_x,
                                    max_box_y,
                                    mc_box_w,
                                    VARIO_UI_ALIGN_RIGHT,
                                    VARIO_UI_FONT_BOTTOM_MAX_VALUE,
                                    mc_text);

    vario_display_draw_text_box_top(u8g2,
                                    max_box_x,
                                    max_box_y,
                                    VARIO_UI_BOTTOM_META_BOX_W,
                                    VARIO_UI_ALIGN_RIGHT,
                                    VARIO_UI_FONT_BOTTOM_MAX_VALUE,
                                    max_text);

    if (vario_display_runtime_has_usable_gps_fix(rt) != false)
    {
        vario_display_draw_fixed_speed_value(u8g2,
                                             value_box_x,
                                             value_box_y,
                                             VARIO_UI_BOTTOM_BOX_W,
                                             value_box_h,
                                             VARIO_UI_ALIGN_RIGHT,
                                             value_text);
    }
    else
    {
        vario_display_draw_speed_inop_value(u8g2,
                                            value_box_x,
                                            value_box_y,
                                            VARIO_UI_BOTTOM_BOX_W,
                                            value_box_h,
                                            VARIO_UI_ALIGN_RIGHT);
    }
}


static float vario_display_quantize_alt_graph_span_m(float span_m)
{
    static const float k_spans_m[] = {50.0f, 100.0f, 200.0f, 500.0f, 1000.0f, 2000.0f, 5000.0f, 10000.0f};
    uint32_t i;
    float    target_span_m;

    target_span_m = span_m;
    if (target_span_m < VARIO_UI_ALT_GRAPH_MIN_SPAN_M)
    {
        target_span_m = VARIO_UI_ALT_GRAPH_MIN_SPAN_M;
    }

    for (i = 0u; i < (sizeof(k_spans_m) / sizeof(k_spans_m[0])); ++i)
    {
        if (target_span_m <= k_spans_m[i])
        {
            return k_spans_m[i];
        }
    }

    return k_spans_m[(sizeof(k_spans_m) / sizeof(k_spans_m[0])) - 1u];
}

static void vario_display_update_altitude_graph_window(float raw_min_alt_m,
                                                       float raw_max_alt_m,
                                                       float *window_min_alt_m,
                                                       float *window_max_alt_m)
{
    float raw_range_m;
    float raw_center_m;
    float target_span_m;
    float half_span_m;
    float margin_m;

    if ((window_min_alt_m == NULL) || (window_max_alt_m == NULL))
    {
        return;
    }

    raw_range_m = raw_max_alt_m - raw_min_alt_m;
    if (raw_range_m < 0.5f)
    {
        raw_range_m = 0.5f;
    }
    raw_center_m = 0.5f * (raw_min_alt_m + raw_max_alt_m);

    /* ---------------------------------------------------------------------- */
    /* ?ㅼ젣 諛붾━?ㅻ쪟 ALT graph 媛 scale ???④퀎?곸쑝濡??뺤옣?섎뒗 ?먮굦???곕씪      */
    /* raw span ??headroom ???뱀? ??50/100/200/500... m ?⑥쐞濡?quantize.   */
    /* ---------------------------------------------------------------------- */
    target_span_m = vario_display_quantize_alt_graph_span_m(raw_range_m * 1.25f);

    if (s_vario_ui_dynamic.altitude_graph_initialized == false)
    {
        s_vario_ui_dynamic.altitude_graph_center_m = raw_center_m;
        s_vario_ui_dynamic.altitude_graph_span_m = target_span_m;
        s_vario_ui_dynamic.altitude_graph_shrink_votes = 0u;
        s_vario_ui_dynamic.altitude_graph_initialized = true;
    }
    else
    {
        if (target_span_m > s_vario_ui_dynamic.altitude_graph_span_m)
        {
            s_vario_ui_dynamic.altitude_graph_span_m = target_span_m;
            s_vario_ui_dynamic.altitude_graph_shrink_votes = 0u;
        }
        else if (target_span_m < (s_vario_ui_dynamic.altitude_graph_span_m *
                                  VARIO_UI_ALT_GRAPH_SHRINK_THRESHOLD_RATIO))
        {
            if (s_vario_ui_dynamic.altitude_graph_shrink_votes < 255u)
            {
                s_vario_ui_dynamic.altitude_graph_shrink_votes++;
            }

            if (s_vario_ui_dynamic.altitude_graph_shrink_votes >= VARIO_UI_ALT_GRAPH_SHRINK_HOLD_FRAMES)
            {
                s_vario_ui_dynamic.altitude_graph_span_m = target_span_m;
                s_vario_ui_dynamic.altitude_graph_shrink_votes = 0u;
            }
        }
        else
        {
            s_vario_ui_dynamic.altitude_graph_shrink_votes = 0u;
        }

        half_span_m = 0.5f * s_vario_ui_dynamic.altitude_graph_span_m;
        margin_m = s_vario_ui_dynamic.altitude_graph_span_m * VARIO_UI_ALT_GRAPH_CENTER_MARGIN_RATIO;

        if ((raw_min_alt_m < (s_vario_ui_dynamic.altitude_graph_center_m - half_span_m + margin_m)) ||
            (raw_max_alt_m > (s_vario_ui_dynamic.altitude_graph_center_m + half_span_m - margin_m)))
        {
            s_vario_ui_dynamic.altitude_graph_center_m = raw_center_m;
        }
        else
        {
            s_vario_ui_dynamic.altitude_graph_center_m +=
                (raw_center_m - s_vario_ui_dynamic.altitude_graph_center_m) *
                VARIO_UI_ALT_GRAPH_CENTER_LP_ALPHA;
        }
    }

    half_span_m = 0.5f * s_vario_ui_dynamic.altitude_graph_span_m;
    *window_min_alt_m = s_vario_ui_dynamic.altitude_graph_center_m - half_span_m;
    *window_max_alt_m = s_vario_ui_dynamic.altitude_graph_center_m + half_span_m;

    if (raw_min_alt_m < *window_min_alt_m)
    {
        *window_min_alt_m = raw_min_alt_m;
    }
    if (raw_max_alt_m > *window_max_alt_m)
    {
        *window_max_alt_m = raw_max_alt_m;
    }
}



static void vario_display_draw_altitude_sparkline(u8g2_t *u8g2,
                                                  const vario_viewport_t *v,
                                                  const vario_runtime_t *rt)
{
    uint16_t sample_count;
    uint16_t history_cap;
    uint16_t start_index;
    uint16_t i;
    uint16_t hist_index;
    int16_t  graph_x;
    int16_t  graph_y;
    int16_t  graph_top_limit;
    int16_t  current_value_h;
    int16_t  max_value_h;
    int16_t  prev_x;
    int16_t  prev_y;
    bool     have_prev;
    float    min_alt;
    float    max_alt;
    float    range_alt;

    if ((u8g2 == NULL) || (v == NULL) || (rt == NULL))
    {
        return;
    }

    sample_count = rt->history_count;
    history_cap = (uint16_t)(sizeof(rt->history_altitude_m) / sizeof(rt->history_altitude_m[0]));
    if (history_cap == 0u)
    {
        return;
    }
    if (sample_count < 2u)
    {
        return;
    }
    if (sample_count > history_cap)
    {
        sample_count = history_cap;
    }

    current_value_h = vario_display_get_font_height(u8g2, VARIO_UI_FONT_BOTTOM_MAIN);
    if (current_value_h <= 0)
    {
        current_value_h = VARIO_UI_BOTTOM_BOX_H;
    }
    max_value_h = vario_display_get_font_height(u8g2, VARIO_UI_FONT_BOTTOM_MAX_VALUE);
    if (max_value_h <= 0)
    {
        max_value_h = 8;
    }

    graph_x = (int16_t)(v->x + v->w - VARIO_UI_SIDE_BAR_W - VARIO_UI_ALT_GRAPH_RIGHT_PAD - VARIO_UI_ALT_GRAPH_W);
    graph_y = (int16_t)(v->y + v->h - VARIO_UI_BOTTOM_BOX_BOTTOM_PAD - current_value_h -
                        max_value_h - VARIO_UI_BOTTOM_META_GAP_Y - VARIO_UI_ALT_GRAPH_H - VARIO_UI_ALT_GRAPH_GAP_Y);
    graph_top_limit = (int16_t)(v->y + 60);
    if (graph_y < graph_top_limit)
    {
        graph_y = graph_top_limit;
    }

    start_index = (uint16_t)((rt->history_head + history_cap - sample_count) % history_cap);
    min_alt = rt->history_altitude_m[start_index];
    max_alt = min_alt;
    for (i = 0u; i < sample_count; ++i)
    {
        float sample_alt;

        hist_index = (uint16_t)((start_index + i) % history_cap);
        sample_alt = rt->history_altitude_m[hist_index];
        if (sample_alt < min_alt)
        {
            min_alt = sample_alt;
        }
        if (sample_alt > max_alt)
        {
            max_alt = sample_alt;
        }
    }

    vario_display_update_altitude_graph_window(min_alt,
                                              max_alt,
                                              &min_alt,
                                              &max_alt);
    range_alt = max_alt - min_alt;
    if (range_alt < 0.5f)
    {
        min_alt -= 0.25f;
        max_alt += 0.25f;
        range_alt = max_alt - min_alt;
    }

    have_prev = false;
    for (i = 0u; i < (uint16_t)VARIO_UI_ALT_GRAPH_W; ++i)
    {
        uint16_t sample_pos;
        float    sample_alt;
        float    ratio;
        int16_t  draw_x;
        int16_t  draw_y;

        if (VARIO_UI_ALT_GRAPH_W <= 1)
        {
            sample_pos = 0u;
        }
        else
        {
            sample_pos = (uint16_t)(((uint32_t)i * (uint32_t)(sample_count - 1u)) / (uint32_t)(VARIO_UI_ALT_GRAPH_W - 1));
        }

        hist_index = (uint16_t)((start_index + sample_pos) % history_cap);
        sample_alt = rt->history_altitude_m[hist_index];
        ratio = (sample_alt - min_alt) / range_alt;
        ratio = vario_display_clampf(ratio, 0.0f, 1.0f);

        draw_x = (int16_t)(graph_x + (int16_t)i);
        draw_y = (int16_t)(graph_y + VARIO_UI_ALT_GRAPH_H - 1 - lroundf(ratio * (float)(VARIO_UI_ALT_GRAPH_H - 1)));

        if (have_prev != false)
        {
            u8g2_DrawLine(u8g2, prev_x, prev_y, draw_x, draw_y);
        }
        prev_x = draw_x;
        prev_y = draw_y;
        have_prev = true;
    }
}


static void vario_display_draw_stub_overlay(u8g2_t *u8g2,
                                            const vario_viewport_t *v,
                                            const char *title,
                                            const char *subtitle)
{
    int16_t center_x;
    int16_t center_y;

    if ((u8g2 == NULL) || (v == NULL) || (title == NULL) || (subtitle == NULL))
    {
        return;
    }

    center_x = (int16_t)(v->x + (v->w / 2));
    center_y = (int16_t)(v->y + (v->h / 2));

    u8g2_SetFont(u8g2, VARIO_UI_FONT_STUB_TITLE);
    Vario_Display_DrawTextCentered(u8g2,
                                   center_x,
                                   (int16_t)(center_y + VARIO_UI_STUB_TITLE_BASELINE_DY),
                                   title);

    u8g2_SetFont(u8g2, VARIO_UI_FONT_STUB_SUB);
    Vario_Display_DrawTextCentered(u8g2,
                                   center_x,
                                   (int16_t)(center_y + VARIO_UI_STUB_SUB_BASELINE_DY),
                                   subtitle);
}

static bool vario_display_compute_compass_circle_geometry(u8g2_t *u8g2,
                                                          const vario_viewport_t *v,
                                                          int16_t *out_center_x,
                                                          int16_t *out_center_y,
                                                          int16_t *out_radius)
{
    int16_t content_left_x;
    int16_t content_right_x;
    int16_t center_x;
    int16_t label_baseline_y;
    int16_t top_limit_y;
    int16_t bottom_limit_y;
    int16_t usable_half_w;
    int16_t radius;
    int16_t center_y;

    if ((u8g2 == NULL) || (v == NULL) || (out_center_x == NULL) ||
        (out_center_y == NULL) || (out_radius == NULL))
    {
        return false;
    }

    content_left_x = (int16_t)(v->x + VARIO_UI_SIDE_BAR_W);
    content_right_x = (int16_t)(v->x + v->w - VARIO_UI_SIDE_BAR_W - 1);
    center_x = (int16_t)(v->x + (v->w / 2));
    label_baseline_y = (int16_t)(v->y + VARIO_UI_NAV_LABEL_BASELINE_Y);
    top_limit_y = (int16_t)(label_baseline_y + VARIO_UI_NAV_LABEL_TO_COMPASS_GAP);
    bottom_limit_y = (int16_t)(v->y + v->h -
                               vario_display_get_font_height(u8g2, VARIO_UI_FONT_BOTTOM_MAIN) -
                               VARIO_UI_BOTTOM_BOX_BOTTOM_PAD -
                               VARIO_UI_COMPASS_BOTTOM_GAP);
    usable_half_w = (int16_t)(((content_right_x - content_left_x + 1) / 2) - VARIO_UI_COMPASS_SIDE_MARGIN);
    radius = usable_half_w;

    if (radius > VARIO_UI_COMPASS_MAX_RADIUS)
    {
        radius = VARIO_UI_COMPASS_MAX_RADIUS;
    }

    if ((bottom_limit_y - top_limit_y) > 0)
    {
        int16_t max_r_from_height;
        max_r_from_height = (int16_t)((bottom_limit_y - top_limit_y) / 2);
        if (radius > max_r_from_height)
        {
            radius = max_r_from_height;
        }
    }

    radius = (int16_t)(radius + ((VARIO_UI_COMPASS_DIAMETER_GROW_PX + 1) / 2));

    if ((bottom_limit_y - top_limit_y) > 0)
    {
        int16_t max_r_from_height;
        max_r_from_height = (int16_t)((bottom_limit_y - top_limit_y) / 2);
        if (radius > max_r_from_height)
        {
            radius = max_r_from_height;
        }
    }

    if (radius < 18)
    {
        radius = 18;
    }

    radius = (int16_t)(radius + VARIO_UI_COMPASS_RADIUS_EXTRA_PX);
    center_y = (int16_t)(bottom_limit_y - radius);
    if ((center_y - radius) < top_limit_y)
    {
        center_y = (int16_t)(top_limit_y + radius);
    }

    center_y = (int16_t)(center_y + VARIO_UI_COMPASS_CENTER_Y_SHIFT_PX);
    if ((center_y - radius) < top_limit_y)
    {
        center_y = (int16_t)(top_limit_y + radius);
    }
    if ((center_y + radius) > bottom_limit_y)
    {
        center_y = (int16_t)(bottom_limit_y - radius);
    }

    *out_center_x = center_x;
    *out_center_y = center_y;
    *out_radius = radius;
    return true;
}

static void vario_display_draw_heading_arrow_at(u8g2_t *u8g2,
                                               int16_t center_x,
                                               int16_t center_y,
                                               float heading_deg,
                                               const vario_settings_t *settings)
{
    int16_t arrow_size_px;
    int16_t wing_half_px;
    float   rad;
    float   dir_x;
    float   dir_y;
    float   side_x;
    float   side_y;
    int16_t tip_x;
    int16_t tip_y;
    int16_t tail_x;
    int16_t tail_y;
    int16_t base_x;
    int16_t base_y;
    int16_t left_x;
    int16_t left_y;
    int16_t right_x;
    int16_t right_y;

    if (u8g2 == NULL)
    {
        return;
    }

    arrow_size_px = 9;
    if (settings != NULL)
    {
        arrow_size_px = (int16_t)settings->arrow_size_px;
    }
    if (arrow_size_px < 6)
    {
        arrow_size_px = 6;
    }
    if (arrow_size_px > 18)
    {
        arrow_size_px = 18;
    }

    wing_half_px = (int16_t)(arrow_size_px / 3);
    if (wing_half_px < 3)
    {
        wing_half_px = 3;
    }

    rad = vario_display_deg_to_rad(heading_deg);
    dir_x = sinf(rad);
    dir_y = -cosf(rad);
    side_x = cosf(rad);
    side_y = sinf(rad);

    tip_x = (int16_t)lroundf((float)center_x + (dir_x * (float)arrow_size_px));
    tip_y = (int16_t)lroundf((float)center_y + (dir_y * (float)arrow_size_px));
    tail_x = (int16_t)lroundf((float)center_x - (dir_x * (float)(arrow_size_px - 2)));
    tail_y = (int16_t)lroundf((float)center_y - (dir_y * (float)(arrow_size_px - 2)));
    base_x = (int16_t)lroundf((float)center_x - (dir_x * ((float)arrow_size_px * 0.25f)));
    base_y = (int16_t)lroundf((float)center_y - (dir_y * ((float)arrow_size_px * 0.25f)));
    left_x = (int16_t)lroundf((float)base_x - (side_x * (float)wing_half_px));
    left_y = (int16_t)lroundf((float)base_y - (side_y * (float)wing_half_px));
    right_x = (int16_t)lroundf((float)base_x + (side_x * (float)wing_half_px));
    right_y = (int16_t)lroundf((float)base_y + (side_y * (float)wing_half_px));

    u8g2_DrawLine(u8g2, tail_x, tail_y, tip_x, tip_y);
    u8g2_DrawLine(u8g2, tip_x, tip_y, left_x, left_y);
    u8g2_DrawLine(u8g2, tip_x, tip_y, right_x, right_y);
    u8g2_DrawLine(u8g2, left_x, left_y, right_x, right_y);
}

static void vario_display_draw_center_heading_arrow(u8g2_t *u8g2,
                                                    const vario_viewport_t *v,
                                                    const vario_runtime_t *rt,
                                                    const vario_settings_t *settings)
{
    int16_t center_x;
    int16_t center_y;
    float   heading_deg;

    if ((u8g2 == NULL) || (v == NULL) || (rt == NULL))
    {
        return;
    }

    center_x = (int16_t)(v->x + (v->w / 2));
    center_y = (int16_t)(v->y + (v->h / 2));
    heading_deg = (rt->heading_valid != false) ? rt->heading_deg : 0.0f;
    vario_display_draw_heading_arrow_at(u8g2, center_x, center_y, heading_deg, settings);
}

static bool vario_display_compute_circle_line_endpoints(int16_t center_x,
                                                        int16_t center_y,
                                                        int16_t radius,
                                                        float line_angle_deg,
                                                        float y_offset_px,
                                                        int16_t *out_x1,
                                                        int16_t *out_y1,
                                                        int16_t *out_x2,
                                                        int16_t *out_y2)
{
    float rad;
    float dir_x;
    float dir_y;
    float p0x;
    float p0y;
    float b;
    float c;
    float disc;
    float sqrt_disc;
    float t1;
    float t2;

    if ((out_x1 == NULL) || (out_y1 == NULL) || (out_x2 == NULL) || (out_y2 == NULL) || (radius <= 0))
    {
        return false;
    }

    rad = vario_display_deg_to_rad(line_angle_deg);
    dir_x = cosf(rad);
    dir_y = sinf(rad);
    p0x = 0.0f;
    p0y = y_offset_px;
    b = 2.0f * ((p0x * dir_x) + (p0y * dir_y));
    c = (p0x * p0x) + (p0y * p0y) - ((float)radius * (float)radius);
    disc = (b * b) - (4.0f * c);
    if (disc < 0.0f)
    {
        return false;
    }

    sqrt_disc = sqrtf(disc);
    t1 = (-b - sqrt_disc) * 0.5f;
    t2 = (-b + sqrt_disc) * 0.5f;

    *out_x1 = (int16_t)lroundf((float)center_x + p0x + (dir_x * t1));
    *out_y1 = (int16_t)lroundf((float)center_y + p0y + (dir_y * t1));
    *out_x2 = (int16_t)lroundf((float)center_x + p0x + (dir_x * t2));
    *out_y2 = (int16_t)lroundf((float)center_y + p0y + (dir_y * t2));
    return true;
}

static void vario_display_draw_circle_clipped_segment(u8g2_t *u8g2,
                                                      int16_t center_x,
                                                      int16_t center_y,
                                                      int16_t radius,
                                                      float line_angle_deg,
                                                      float y_offset_px,
                                                      float shorten_ratio)
{
    int16_t x1;
    int16_t y1;
    int16_t x2;
    int16_t y2;

    if ((u8g2 == NULL) || (radius <= 0))
    {
        return;
    }

    if (vario_display_compute_circle_line_endpoints(center_x,
                                                    center_y,
                                                    radius,
                                                    line_angle_deg,
                                                    y_offset_px,
                                                    &x1,
                                                    &y1,
                                                    &x2,
                                                    &y2) == false)
    {
        return;
    }

    if (shorten_ratio < 1.0f)
    {
        float mid_x;
        float mid_y;

        if (shorten_ratio < 0.0f)
        {
            shorten_ratio = 0.0f;
        }

        mid_x = ((float)x1 + (float)x2) * 0.5f;
        mid_y = ((float)y1 + (float)y2) * 0.5f;
        x1 = (int16_t)lroundf(mid_x + (((float)x1 - mid_x) * shorten_ratio));
        y1 = (int16_t)lroundf(mid_y + (((float)y1 - mid_y) * shorten_ratio));
        x2 = (int16_t)lroundf(mid_x + (((float)x2 - mid_x) * shorten_ratio));
        y2 = (int16_t)lroundf(mid_y + (((float)y2 - mid_y) * shorten_ratio));
    }

    u8g2_DrawLine(u8g2, x1, y1, x2, y2);
}

#ifndef VARIO_UI_TRAIL_MARKER_RADIUS_PX
#define VARIO_UI_TRAIL_MARKER_RADIUS_PX 4u
#endif

static bool vario_display_project_trail_point_px(int32_t origin_lat_e7,
                                                 int32_t origin_lon_e7,
                                                 int32_t point_lat_e7,
                                                 int32_t point_lon_e7,
                                                 bool heading_up_mode,
                                                 float current_heading_deg,
                                                 int16_t draw_center_x,
                                                 int16_t draw_center_y,
                                                 float meters_per_px,
                                                 int16_t *out_px,
                                                 int16_t *out_py)
{
    float org_lat_deg;
    float org_lon_deg;
    float pt_lat_deg;
    float pt_lon_deg;
    float mean_lat_rad;
    float dx_m;
    float dy_m;
    float plot_dx_m;
    float plot_dy_m;

    if ((out_px == NULL) || (out_py == NULL) || (meters_per_px <= 0.0f))
    {
        return false;
    }

    /* ------------------------------------------------------------------ */
    /* trail breadcrumb ? overlay marker ???꾩쟾??媛숈? local tangent      */
    /* ?ъ쁺?앹쓣 ?⑥빞 ?쒕떎. 洹몃옒??north-up / trail-up ?대뒓 紐⑤뱶?먯꽌??        */
    /* ??λ맂 START / LAND / WPT 湲고샇媛 breadcrumb 諛곌꼍怨??뺥솗??留욌Ъ由곕떎.   */
    /* ------------------------------------------------------------------ */
    org_lat_deg = ((float)origin_lat_e7) * 1.0e-7f;
    org_lon_deg = ((float)origin_lon_e7) * 1.0e-7f;
    pt_lat_deg  = ((float)point_lat_e7) * 1.0e-7f;
    pt_lon_deg  = ((float)point_lon_e7) * 1.0e-7f;
    mean_lat_rad = vario_display_deg_to_rad((org_lat_deg + pt_lat_deg) * 0.5f);

    dx_m = (pt_lon_deg - org_lon_deg) * (111319.5f * cosf(mean_lat_rad));
    dy_m = (pt_lat_deg - org_lat_deg) * 111132.0f;
    plot_dx_m = dx_m;
    plot_dy_m = dy_m;

    if (heading_up_mode != false)
    {
        float heading_rad;
        float rot_x;
        float rot_y;

        heading_rad = vario_display_deg_to_rad(current_heading_deg);
        rot_x = (dx_m * cosf(heading_rad)) - (dy_m * sinf(heading_rad));
        rot_y = (dx_m * sinf(heading_rad)) + (dy_m * cosf(heading_rad));
        plot_dx_m = rot_x;
        plot_dy_m = rot_y;
    }

    *out_px = (int16_t)lroundf((float)draw_center_x + (plot_dx_m / meters_per_px));
    *out_py = (int16_t)lroundf((float)draw_center_y - (plot_dy_m / meters_per_px));
    return true;
}

static void vario_display_draw_hollow_triangle(u8g2_t *u8g2,
                                               int16_t center_x,
                                               int16_t center_y,
                                               uint8_t radius_px,
                                               bool inverted)
{
    int16_t x_left;
    int16_t x_right;
    int16_t y_top;
    int16_t y_bottom;

    if ((u8g2 == NULL) || (radius_px == 0u))
    {
        return;
    }

    x_left = (int16_t)(center_x - (int16_t)radius_px);
    x_right = (int16_t)(center_x + (int16_t)radius_px);
    y_top = (int16_t)(center_y - (int16_t)radius_px);
    y_bottom = (int16_t)(center_y + (int16_t)radius_px);

    if (inverted == false)
    {
        u8g2_DrawLine(u8g2, center_x, y_top, x_left, y_bottom);
        u8g2_DrawLine(u8g2, center_x, y_top, x_right, y_bottom);
        u8g2_DrawLine(u8g2, x_left, y_bottom, x_right, y_bottom);
    }
    else
    {
        u8g2_DrawLine(u8g2, x_left, y_top, x_right, y_top);
        u8g2_DrawLine(u8g2, x_left, y_top, center_x, y_bottom);
        u8g2_DrawLine(u8g2, x_right, y_top, center_x, y_bottom);
    }
}

static void vario_display_draw_trail_overlay_marker(u8g2_t *u8g2,
                                                    int16_t center_x,
                                                    int16_t center_y,
                                                    uint8_t marker_kind)
{
    if (u8g2 == NULL)
    {
        return;
    }

    /* ------------------------------------------------------------------ */
    /* START / LAND / WPT overlay 湲고샇                                      */
    /* - START : hollow triangle                                            */
    /* - LAND  : hollow inverted triangle                                   */
    /* - WPT   : hollow circle                                              */
    /*                                                                    */
    /* 諛섏?由?4 px ??breadcrumb dot(1~3 px) 蹂대떎 遺꾨챸???ш퀬,             */
    /* 240x128 trail page ?먯꽌 怨쇳븯寃??쒖빞瑜?媛由ъ? ?딅뒗 ?덉땐媛믪씠??         */
    /* ------------------------------------------------------------------ */
    switch ((vario_nav_trail_marker_kind_t)marker_kind)
    {
        case VARIO_NAV_TRAIL_MARKER_START:
            vario_display_draw_hollow_triangle(u8g2,
                                               center_x,
                                               center_y,
                                               VARIO_UI_TRAIL_MARKER_RADIUS_PX,
                                               false);
            break;

        case VARIO_NAV_TRAIL_MARKER_LANDABLE:
            vario_display_draw_hollow_triangle(u8g2,
                                               center_x,
                                               center_y,
                                               VARIO_UI_TRAIL_MARKER_RADIUS_PX,
                                               true);
            break;

        case VARIO_NAV_TRAIL_MARKER_WAYPOINT:
        default:
            u8g2_DrawCircle(u8g2,
                            center_x,
                            center_y,
                            VARIO_UI_TRAIL_MARKER_RADIUS_PX,
                            U8G2_DRAW_ALL);
            break;
    }
}

static uint8_t vario_display_get_trail_dot_size_px(const vario_settings_t *settings,
                                                  int16_t vario_cms)
{
    uint8_t base_size_px;

    base_size_px = 1u;
    if ((settings != NULL) && (settings->trail_dot_size_px != 0u))
    {
        base_size_px = settings->trail_dot_size_px;
    }
    if (base_size_px > 3u)
    {
        base_size_px = 3u;
    }

    if (vario_cms >= 320)
    {
        return (uint8_t)(base_size_px + 2u);
    }
    if (vario_cms >= 120)
    {
        return (uint8_t)(base_size_px + 1u);
    }
    if (vario_cms <= -150)
    {
        return 1u;
    }
    if (vario_cms <= -40)
    {
        return (base_size_px > 1u) ? (uint8_t)(base_size_px - 1u) : 1u;
    }

    return base_size_px;
}

static void vario_display_draw_trail_dot(u8g2_t *u8g2,
                                         int16_t px,
                                         int16_t py,
                                         uint8_t size_px)
{
    int16_t top_left_x;
    int16_t top_left_y;

    if (size_px <= 1u)
    {
        u8g2_DrawPixel(u8g2, px, py);
        return;
    }

    top_left_x = (int16_t)(px - ((int16_t)size_px / 2));
    top_left_y = (int16_t)(py - ((int16_t)size_px / 2));
    u8g2_DrawBox(u8g2, top_left_x, top_left_y, size_px, size_px);
}

static void vario_display_draw_trail_background(u8g2_t *u8g2,
                                                const vario_viewport_t *v,
                                                const vario_runtime_t *rt,
                                                const vario_settings_t *settings)
{
    uint8_t start_index;
    uint8_t i;
    int16_t draw_center_x;
    int16_t draw_center_y;
    float   half_w_px;
    float   half_h_px;
    float   meters_per_px;
    bool    can_draw_points;
    bool    heading_up_mode;
    int32_t origin_lat_e7;
    int32_t origin_lon_e7;
    float   current_heading_deg;
    bool    origin_valid;
    int16_t aircraft_px;
    int16_t aircraft_py;

    if ((u8g2 == NULL) || (v == NULL) || (rt == NULL) || (settings == NULL))
    {
        return;
    }

    draw_center_x = (int16_t)(v->x + (v->w / 2));
    draw_center_y = (int16_t)(v->y + (v->h / 2));
    half_w_px = (float)(v->w / 2);
    half_h_px = (float)(v->h / 2);
    meters_per_px = 0.0f;
    if ((half_w_px > 0.0f) && (half_h_px > 0.0f) && (settings->trail_range_m > 0u))
    {
        meters_per_px = ((float)settings->trail_range_m) /
                        ((half_w_px < half_h_px) ? half_w_px : half_h_px);
    }

    heading_up_mode = (s_vario_ui_dynamic.trail_heading_up_mode != false);
    current_heading_deg = (rt->heading_valid != false) ? rt->heading_deg : 0.0f;
    can_draw_points = ((rt->gps_valid != false) &&
                       (rt->trail_count > 0u) &&
                       (meters_per_px > 0.0f));
    /* ------------------------------------------------------------------ */
    /* trail page ??heading-up / north-up 紐⑤몢 "?꾩옱 湲곗껜 以묒떖 怨좎젙" ?대떎. */
    /* 利? 諛곌꼍 trail ???吏곸씠怨??붿궡?쒕뒗 以묒떖???⑤뒗??                  */
    /* - heading-up : 諛곌꼍??-heading 留뚰겮 ?뚯쟾, ?붿궡?쒕뒗 ?꾨? 蹂몃떎.        */
    /* - north-up   : 諛곌꼍? ?뚯쟾?섏? ?딄퀬, ?붿궡?쒕쭔 ?꾩옱 heading ??蹂몃떎.  */
    /* ------------------------------------------------------------------ */
    origin_valid = (rt->gps_valid != false);
    origin_lat_e7 = rt->gps.fix.lat;
    origin_lon_e7 = rt->gps.fix.lon;
    aircraft_px = draw_center_x;
    aircraft_py = draw_center_y;

    if ((can_draw_points != false) && (origin_valid != false))
    {
        start_index = (uint8_t)((rt->trail_head + VARIO_TRAIL_MAX_POINTS - rt->trail_count) % VARIO_TRAIL_MAX_POINTS);
        for (i = 0u; i < rt->trail_count; ++i)
        {
            uint8_t idx;
            int16_t px;
            int16_t py;

            idx = (uint8_t)((start_index + i) % VARIO_TRAIL_MAX_POINTS);
            /* ---------------------------------------------------------- */
            /* trail dot ? overlay marker ??媛숈? ?붾㈃ ?ъ쁺 helper 瑜??대떎. */
            /* north-up / heading-up ?꾪솚 ?쒖뿉??媛숈? 湲곗????좎???         */
            /* marker ? breadcrumb 媛??곷? ?꾩튂媛 ?붾뱾由ъ? ?딄쾶 ?쒕떎.       */
            /* ---------------------------------------------------------- */
            if (vario_display_project_trail_point_px(origin_lat_e7,
                                                     origin_lon_e7,
                                                     rt->trail_lat_e7[idx],
                                                     rt->trail_lon_e7[idx],
                                                     heading_up_mode,
                                                     current_heading_deg,
                                                     draw_center_x,
                                                     draw_center_y,
                                                     meters_per_px,
                                                     &px,
                                                     &py) == false)
            {
                continue;
            }

            if ((px < v->x) || (px >= (v->x + v->w)) || (py < v->y) || (py >= (v->y + v->h)))
            {
                continue;
            }

            vario_display_draw_trail_dot(u8g2,
                                         px,
                                         py,
                                         vario_display_get_trail_dot_size_px(settings,
                                                                             rt->trail_vario_cms[idx]));
        }
    }

    /* ------------------------------------------------------------------ */
    /* START / LAND / WPT overlay marker                                   */
    /*                                                                    */
    /* ?ъ슜?먭? 誘몃━ ??ν븳 異쒕컻??/ 李⑸쪠吏 / ?ъ씤?몃? trail ?꾩뿉 寃뱀퀜 蹂대㈃   */
    /* ?꾩옱 flight breadcrumb 怨?site 湲곗??먯쓽 ?곷? 愿怨꾨? 鍮좊Ⅴ寃??쎌쓣 ???덈떎.*/
    /* ------------------------------------------------------------------ */
    if ((origin_valid != false) && (meters_per_px > 0.0f))
    {
        uint8_t marker_count;
        uint8_t marker_i;

        marker_count = Vario_Navigation_GetTrailMarkerCount();
        for (marker_i = 0u; marker_i < marker_count; ++marker_i)
        {
            vario_nav_trail_marker_t marker;
            int16_t marker_px;
            int16_t marker_py;

            if (Vario_Navigation_GetTrailMarker(marker_i, &marker) == false)
            {
                continue;
            }
            if (marker.valid == false)
            {
                continue;
            }

            if (vario_display_project_trail_point_px(origin_lat_e7,
                                                     origin_lon_e7,
                                                     marker.lat_e7,
                                                     marker.lon_e7,
                                                     heading_up_mode,
                                                     current_heading_deg,
                                                     draw_center_x,
                                                     draw_center_y,
                                                     meters_per_px,
                                                     &marker_px,
                                                     &marker_py) == false)
            {
                continue;
            }

            if ((marker_px < v->x) || (marker_px >= (v->x + v->w)) ||
                (marker_py < v->y) || (marker_py >= (v->y + v->h)))
            {
                continue;
            }

            vario_display_draw_trail_overlay_marker(u8g2,
                                                    marker_px,
                                                    marker_py,
                                                    marker.kind);
        }
    }

    if ((aircraft_px >= v->x) && (aircraft_px < (v->x + v->w)) &&
        (aircraft_py >= v->y) && (aircraft_py < (v->y + v->h)))
    {
        if (heading_up_mode != false)
        {
            vario_display_draw_heading_arrow_at(u8g2,
                                                aircraft_px,
                                                aircraft_py,
                                                0.0f,
                                                settings);
        }
        else
        {
            vario_display_draw_heading_arrow_at(u8g2,
                                                aircraft_px,
                                                aircraft_py,
                                                current_heading_deg,
                                                settings);
        }
    }
}

static void vario_display_draw_compass(u8g2_t *u8g2,
                                       const vario_viewport_t *v,
                                       const vario_runtime_t *rt,
                                       const vario_display_nav_solution_t *nav)
{
    char    nav_text[48];
    int16_t center_x;
    int16_t center_y;
    int16_t radius;
    int16_t i;
    float   heading_deg;

    if ((u8g2 == NULL) || (v == NULL) || (rt == NULL) || (nav == NULL))
    {
        return;
    }

    if (vario_display_compute_compass_circle_geometry(u8g2, v, &center_x, &center_y, &radius) == false)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /* ?곷떒??START ---.-Km 臾멸뎄???ъ슜?먭? ??젣瑜??붽뎄?덉쑝誘濡????댁긽        */
    /* draw ?섏? ?딅뒗?? ?ㅻ쭔 distance formatter ???ㅻⅨ 鍮뚮뱶?먯꽌              */
    /* static inline 理쒖쟻??寃쎄퀬瑜??쇳븯?ㅺ퀬 ?몄텧留??좎??쒕떎.                    */
    /* ---------------------------------------------------------------------- */
    vario_display_format_nav_distance(nav_text,
                                      sizeof(nav_text),
                                      nav->label,
                                      nav->current_valid && nav->target_valid,
                                      nav->distance_m);
    (void)nav_text;

    u8g2_DrawCircle(u8g2, center_x, center_y, radius, U8G2_DRAW_ALL);
    u8g2_DrawBox(u8g2, center_x - 1, center_y - 1, 3u, 3u);

    heading_deg = (rt->heading_valid != false) ? rt->heading_deg : 0.0f;

    for (i = 0; i < 360; i += 30)
    {
        float   rel_deg;
        float   rad;
        int16_t inner_r;
        int16_t outer_x;
        int16_t outer_y;
        int16_t inner_x;
        int16_t inner_y;

        rel_deg = (float)i - heading_deg;
        rad = vario_display_deg_to_rad(rel_deg);
        inner_r = ((i % 90) == 0) ? (radius - 6) : (radius - 3);
        outer_x = (int16_t)lroundf((float)center_x + (sinf(rad) * (float)radius));
        outer_y = (int16_t)lroundf((float)center_y - (cosf(rad) * (float)radius));
        inner_x = (int16_t)lroundf((float)center_x + (sinf(rad) * (float)inner_r));
        inner_y = (int16_t)lroundf((float)center_y - (cosf(rad) * (float)inner_r));
        u8g2_DrawLine(u8g2, inner_x, inner_y, outer_x, outer_y);
    }

    {
        static const struct
        {
            int16_t deg;
            const char *text;
        } cards[] = {
            { 0,   "N" },
            { 90,  "E" },
            { 180, "S" },
            { 270, "W" }
        };
        uint8_t ci;

        u8g2_SetFont(u8g2, VARIO_UI_FONT_COMPASS_CARDINAL);
        for (ci = 0u; ci < (uint8_t)(sizeof(cards) / sizeof(cards[0])); ++ci)
        {
            float   rel_deg;
            float   rad;
            int16_t text_x;
            int16_t text_y;

            rel_deg = (float)cards[ci].deg - heading_deg;
            rad = vario_display_deg_to_rad(rel_deg);
            text_x = (int16_t)lroundf((float)center_x + (sinf(rad) * (float)(radius - VARIO_UI_COMPASS_LABEL_INSET)));
            text_y = (int16_t)lroundf((float)center_y - (cosf(rad) * (float)(radius - VARIO_UI_COMPASS_LABEL_INSET)));
            Vario_Display_DrawTextCentered(u8g2, text_x, text_y + 3, cards[ci].text);
        }
    }

    /* ---------------------------------------------------------------------- */
    /* ?붾㈃ ?뺣턿(?곷떒)? ??긽 ?꾩옱 heading ??媛由ы궎??heading-up compass ??  */
    /* ?ㅼ젣 諛붾━??怨꾩뿴泥섎읆 top bug 瑜?怨좎젙?섍퀬, ?λ??먮쭔 ?뚯쟾?쒗궓??          */
    /* ---------------------------------------------------------------------- */
    u8g2_DrawLine(u8g2, center_x, (int16_t)(center_y - radius + 2), center_x, (int16_t)(center_y - radius + 10));
    u8g2_DrawLine(u8g2,
                  (int16_t)(center_x - 4),
                  (int16_t)(center_y - radius + 8),
                  center_x,
                  (int16_t)(center_y - radius + 2));
    u8g2_DrawLine(u8g2,
                  (int16_t)(center_x + 4),
                  (int16_t)(center_y - radius + 8),
                  center_x,
                  (int16_t)(center_y - radius + 2));

    /* ---------------------------------------------------------------------- */
    /* Wind direction marker                                                   */
    /*                                                                        */
    /* - 蹂꾨룄 湲?⑤뒗 洹몃━吏 ?딅뒗??                                            */
    /* - 4x4 diamond ?먯쓣 circle ?대???諛곗튂?쒕떎.                              */
    /* - wind_from_deg 湲곗??대?濡? ?꾩옱 heading ??鍮쇱꽌 heading-up ?붾㈃?먯꽌    */
    /*   ?곷??꾩튂濡??뚯쟾?쒗궓??                                                */
    /* ---------------------------------------------------------------------- */
    if (rt->wind_valid != false)
    {
        float   wind_rel_deg;
        float   wind_rad;
        int16_t wind_r;
        int16_t wind_x;
        int16_t wind_y;

        wind_rel_deg = rt->wind_from_deg - heading_deg;
        wind_rad = vario_display_deg_to_rad(wind_rel_deg);
        wind_r = (int16_t)(radius - 8);
        if (wind_r < 8)
        {
            wind_r = 8;
        }

        wind_x = (int16_t)lroundf((float)center_x + (sinf(wind_rad) * (float)wind_r));
        wind_y = (int16_t)lroundf((float)center_y - (cosf(wind_rad) * (float)wind_r));
        vario_display_draw_xbm(u8g2,
                               (int16_t)(wind_x - (VARIO_ICON_WIND_DIAMOND_WIDTH / 2)),
                               (int16_t)(wind_y - (VARIO_ICON_WIND_DIAMOND_HEIGHT / 2)),
                               VARIO_ICON_WIND_DIAMOND_WIDTH,
                               VARIO_ICON_WIND_DIAMOND_HEIGHT,
                               vario_icon_wind_diamond_bits);
    }

    if ((nav->current_valid != false) && (nav->target_valid != false))
    {
        float   rad;
        float   nx;
        float   ny;
        int16_t tip_r;
        int16_t base_r;
        int16_t tail_r;
        int16_t tip_x;
        int16_t tip_y;
        int16_t base_x;
        int16_t base_y;
        int16_t tail_x;
        int16_t tail_y;
        int16_t left_x;
        int16_t left_y;
        int16_t right_x;
        int16_t right_y;

        rad = vario_display_deg_to_rad(nav->relative_bearing_deg);
        nx = cosf(rad);
        ny = sinf(rad);
        tip_r = (int16_t)(radius - 6);
        base_r = (int16_t)(tip_r - 8);
        tail_r = 4;
        if (base_r < 8)
        {
            base_r = 8;
        }

        tip_x = (int16_t)lroundf((float)center_x + (sinf(rad) * (float)tip_r));
        tip_y = (int16_t)lroundf((float)center_y - (cosf(rad) * (float)tip_r));
        base_x = (int16_t)lroundf((float)center_x + (sinf(rad) * (float)base_r));
        base_y = (int16_t)lroundf((float)center_y - (cosf(rad) * (float)base_r));
        tail_x = (int16_t)lroundf((float)center_x - (sinf(rad) * (float)tail_r));
        tail_y = (int16_t)lroundf((float)center_y + (cosf(rad) * (float)tail_r));
        left_x = (int16_t)lroundf((float)base_x - (nx * 4.0f));
        left_y = (int16_t)lroundf((float)base_y - (ny * 4.0f));
        right_x = (int16_t)lroundf((float)base_x + (nx * 4.0f));
        right_y = (int16_t)lroundf((float)base_y + (ny * 4.0f));

        u8g2_DrawLine(u8g2, tail_x, tail_y, base_x, base_y);
        u8g2_DrawLine(u8g2, left_x, left_y, tip_x, tip_y);
        u8g2_DrawLine(u8g2, right_x, right_y, tip_x, tip_y);
        u8g2_DrawLine(u8g2, left_x, left_y, right_x, right_y);
    }
}

static void vario_display_draw_attitude_indicator(u8g2_t *u8g2,
                                                  const vario_viewport_t *v,
                                                  const vario_runtime_t *rt)
{
    int16_t center_x;
    int16_t center_y;
    int16_t radius;
    float   bank_deg;
    float   grade_deg;
    float   horizon_angle_deg;
    float   pitch_px_per_deg;
    float   pitch_offset_px;
    int16_t i;

    if ((u8g2 == NULL) || (v == NULL) || (rt == NULL))
    {
        return;
    }

    if (vario_display_compute_compass_circle_geometry(u8g2, v, &center_x, &center_y, &radius) == false)
    {
        return;
    }

    bank_deg = 0.0f;
    grade_deg = 0.0f;
    if ((rt->bike.initialized != false) || (rt->bike.last_update_ms != 0u))
    {
        bank_deg = ((float)rt->bike.banking_angle_deg_x10) * 0.1f;
        grade_deg = ((float)rt->bike.grade_deg_x10) * 0.1f;
    }

    horizon_angle_deg = -bank_deg;
    pitch_px_per_deg = ((float)(radius - 8)) / 30.0f;
    if (pitch_px_per_deg < 0.5f)
    {
        pitch_px_per_deg = 0.5f;
    }
    pitch_offset_px = vario_display_clampf(grade_deg * pitch_px_per_deg,
                                           -(float)(radius - 6),
                                           (float)(radius - 6));

    u8g2_DrawCircle(u8g2, center_x, center_y, radius, U8G2_DRAW_ALL);

    /* ---------------------------------------------------------------------- */
    /* upper bank reference ticks + top pointer                                */
    /* ---------------------------------------------------------------------- */
    for (i = -60; i <= 60; i += 30)
    {
        float   rad;
        int16_t outer_x;
        int16_t outer_y;
        int16_t inner_r;
        int16_t inner_x;
        int16_t inner_y;

        rad = vario_display_deg_to_rad((float)i);
        inner_r = (i == 0) ? (radius - 7) : (radius - 4);
        outer_x = (int16_t)lroundf((float)center_x + (sinf(rad) * (float)(radius - 1)));
        outer_y = (int16_t)lroundf((float)center_y - (cosf(rad) * (float)(radius - 1)));
        inner_x = (int16_t)lroundf((float)center_x + (sinf(rad) * (float)inner_r));
        inner_y = (int16_t)lroundf((float)center_y - (cosf(rad) * (float)inner_r));
        u8g2_DrawLine(u8g2, inner_x, inner_y, outer_x, outer_y);
    }

    u8g2_DrawLine(u8g2, center_x, (int16_t)(center_y - radius + 2), center_x, (int16_t)(center_y - radius + 8));
    u8g2_DrawLine(u8g2,
                  (int16_t)(center_x - 3),
                  (int16_t)(center_y - radius + 6),
                  center_x,
                  (int16_t)(center_y - radius + 2));
    u8g2_DrawLine(u8g2,
                  (int16_t)(center_x + 3),
                  (int16_t)(center_y - radius + 6),
                  center_x,
                  (int16_t)(center_y - radius + 2));

    /* ---------------------------------------------------------------------- */
    /* rigid-body horizon / pitch ladder                                      */
    /*                                                                        */
    /* bank ??world horizon ??aircraft symbol 諛섎?諛⑺뼢?쇰줈 湲곗슱?꾨줉          */
    /* -bank 瑜??ъ슜?쒕떎.                                                      */
    /* pitch ??nose-up(+)????horizon ???꾨옒濡??대젮媛?꾨줉 screen y + 濡?    */
    /* ??릿??                                                                 */
    /* ---------------------------------------------------------------------- */
    vario_display_draw_circle_clipped_segment(u8g2,
                                              center_x,
                                              center_y,
                                              radius - 2,
                                              horizon_angle_deg,
                                              pitch_offset_px,
                                              0.92f);
    vario_display_draw_circle_clipped_segment(u8g2,
                                              center_x,
                                              center_y,
                                              radius - 4,
                                              horizon_angle_deg,
                                              pitch_offset_px - (10.0f * pitch_px_per_deg),
                                              0.45f);
    vario_display_draw_circle_clipped_segment(u8g2,
                                              center_x,
                                              center_y,
                                              radius - 4,
                                              horizon_angle_deg,
                                              pitch_offset_px + (10.0f * pitch_px_per_deg),
                                              0.45f);
    vario_display_draw_circle_clipped_segment(u8g2,
                                              center_x,
                                              center_y,
                                              radius - 5,
                                              horizon_angle_deg,
                                              pitch_offset_px - (20.0f * pitch_px_per_deg),
                                              0.28f);
    vario_display_draw_circle_clipped_segment(u8g2,
                                              center_x,
                                              center_y,
                                              radius - 5,
                                              horizon_angle_deg,
                                              pitch_offset_px + (20.0f * pitch_px_per_deg),
                                              0.28f);

    /* aircraft reference symbol */
    u8g2_DrawLine(u8g2, (int16_t)(center_x - 10), center_y, (int16_t)(center_x - 3), center_y);
    u8g2_DrawLine(u8g2, (int16_t)(center_x + 3), center_y, (int16_t)(center_x + 10), center_y);
    u8g2_DrawLine(u8g2, (int16_t)(center_x - 3), center_y, center_x, (int16_t)(center_y + 3));
    u8g2_DrawLine(u8g2, (int16_t)(center_x + 3), center_y, center_x, (int16_t)(center_y + 3));
    u8g2_DrawBox(u8g2, center_x - 1, center_y - 1, 3u, 3u);
}


void Vario_Display_SetViewports(const vario_viewport_t *full_viewport,
                                const vario_viewport_t *content_viewport)
{
    if (full_viewport != NULL)
    {
        s_vario_full_viewport = *full_viewport;
    }
    else
    {
        s_vario_full_viewport.x = 0;
        s_vario_full_viewport.y = 0;
        s_vario_full_viewport.w = VARIO_LCD_W;
        s_vario_full_viewport.h = VARIO_LCD_H;
    }

    if (content_viewport != NULL)
    {
        s_vario_content_viewport = *content_viewport;
    }
    else
    {
        s_vario_content_viewport.x = VARIO_CONTENT_X;
        s_vario_content_viewport.y = VARIO_CONTENT_Y;
        s_vario_content_viewport.w = VARIO_CONTENT_W;
        s_vario_content_viewport.h = VARIO_CONTENT_H;
    }

    vario_display_common_sanitize_viewport(&s_vario_full_viewport);
    vario_display_common_sanitize_viewport(&s_vario_content_viewport);
}

const vario_viewport_t *Vario_Display_GetFullViewport(void)
{
    return &s_vario_full_viewport;
}

const vario_viewport_t *Vario_Display_GetContentViewport(void)
{
    return &s_vario_content_viewport;
}

void Vario_Display_DrawTextRight(u8g2_t *u8g2,
                                 int16_t right_x,
                                 int16_t y_baseline,
                                 const char *text)
{
    int16_t width;
    int16_t draw_x;

    if ((u8g2 == NULL) || (text == NULL))
    {
        return;
    }

    width = (int16_t)u8g2_GetStrWidth(u8g2, text);
    draw_x = (int16_t)(right_x - width);

    if (draw_x < 0)
    {
        draw_x = 0;
    }

    u8g2_DrawStr(u8g2, (uint8_t)draw_x, (uint8_t)y_baseline, text);
}

void Vario_Display_DrawTextCentered(u8g2_t *u8g2,
                                    int16_t center_x,
                                    int16_t y_baseline,
                                    const char *text)
{
    int16_t width;
    int16_t draw_x;

    if ((u8g2 == NULL) || (text == NULL))
    {
        return;
    }

    width = (int16_t)u8g2_GetStrWidth(u8g2, text);
    draw_x = (int16_t)(center_x - (width / 2));

    if (draw_x < 0)
    {
        draw_x = 0;
    }

    u8g2_DrawStr(u8g2, (uint8_t)draw_x, (uint8_t)y_baseline, text);
}

void Vario_Display_DrawPageTitle(u8g2_t *u8g2,
                                 const vario_viewport_t *v,
                                 const char *title,
                                 const char *subtitle)
{
    int16_t title_baseline;
    int16_t rule_y;

    if ((u8g2 == NULL) || (v == NULL) || (title == NULL))
    {
        return;
    }

    title_baseline = (int16_t)(v->y + 12);
    rule_y = (int16_t)(v->y + 15);

    u8g2_SetFont(u8g2, u8g2_font_helvB10_tf);
    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawStr(u8g2, (uint8_t)(v->x + 3), (uint8_t)title_baseline, title);

    if ((subtitle != NULL) && (subtitle[0] != 0))
    {
        u8g2_SetFont(u8g2, u8g2_font_6x10_mf);
        Vario_Display_DrawTextRight(u8g2,
                                    (int16_t)(v->x + v->w - 3),
                                    (int16_t)(v->y + 11),
                                    subtitle);
    }

    u8g2_DrawHLine(u8g2, (uint8_t)(v->x + 1), (uint8_t)rule_y, (uint8_t)(v->w - 2));
}

void Vario_Display_DrawKeyValueRow(u8g2_t *u8g2,
                                   const vario_viewport_t *v,
                                   int16_t y_baseline,
                                   const char *label,
                                   const char *value)
{
    if ((u8g2 == NULL) || (v == NULL) || (label == NULL) || (value == NULL))
    {
        return;
    }

    u8g2_SetDrawColor(u8g2, 1);
    u8g2_SetFont(u8g2, u8g2_font_6x10_mf);
    u8g2_DrawStr(u8g2, (uint8_t)(v->x + 4), (uint8_t)y_baseline, label);

    Vario_Display_DrawTextRight(u8g2,
                                (int16_t)(v->x + v->w - 4),
                                y_baseline,
                                value);
}

void Vario_Display_DrawMenuRow(u8g2_t *u8g2,
                               const vario_viewport_t *v,
                               int16_t y_baseline,
                               bool selected,
                               const char *label,
                               const char *value)
{
    int16_t row_top_y;
    int16_t row_height;

    if ((u8g2 == NULL) || (v == NULL) || (label == NULL) || (value == NULL))
    {
        return;
    }

    row_height = 15;
    row_top_y = (int16_t)(y_baseline - 11);

    u8g2_SetDrawColor(u8g2, 1);
    if (selected)
    {
        u8g2_DrawFrame(u8g2,
                       (uint8_t)(v->x + 2),
                       (uint8_t)row_top_y,
                       (uint8_t)(v->w - 4),
                       (uint8_t)row_height);
        u8g2_DrawBox(u8g2,
                     (uint8_t)(v->x + 4),
                     (uint8_t)(row_top_y + 3),
                     3u,
                     (uint8_t)(row_height - 6));
    }

    u8g2_SetFont(u8g2, u8g2_font_6x12_mf);
    u8g2_DrawStr(u8g2,
                 (uint8_t)(v->x + (selected ? 12 : 8)),
                 (uint8_t)y_baseline,
                 label);
    Vario_Display_DrawTextRight(u8g2,
                                (int16_t)(v->x + v->w - 8),
                                y_baseline,
                                value);
}

void Vario_Display_DrawBarRow(u8g2_t *u8g2,
                              const vario_viewport_t *v,
                              int16_t y_top,
                              bool selected,
                              const char *label,
                              const char *value,
                              uint8_t percent)
{
    int16_t row_h;
    int16_t bar_x;
    int16_t bar_y;
    int16_t bar_w;
    int16_t fill_w;

    if ((u8g2 == NULL) || (v == NULL) || (label == NULL) || (value == NULL))
    {
        return;
    }

    if (percent > 100u)
    {
        percent = 100u;
    }

    row_h = 18;
    bar_x = (int16_t)(v->x + 12);
    bar_y = (int16_t)(y_top + 10);
    bar_w = (int16_t)(v->w - 24);
    fill_w = (int16_t)(((int32_t)(bar_w - 2) * (int32_t)percent) / 100);

    u8g2_SetDrawColor(u8g2, 1);
    if (selected)
    {
        u8g2_DrawFrame(u8g2,
                       (uint8_t)(v->x + 2),
                       (uint8_t)y_top,
                       (uint8_t)(v->w - 4),
                       (uint8_t)row_h);
        u8g2_DrawBox(u8g2,
                     (uint8_t)(v->x + 4),
                     (uint8_t)(y_top + 4),
                     3u,
                     (uint8_t)(row_h - 8));
    }

    u8g2_SetFont(u8g2, u8g2_font_6x12_mf);
    u8g2_DrawStr(u8g2,
                 (uint8_t)(v->x + (selected ? 12 : 8)),
                 (uint8_t)(y_top + 8),
                 label);
    Vario_Display_DrawTextRight(u8g2,
                                (int16_t)(v->x + v->w - 8),
                                (int16_t)(y_top + 8),
                                value);

    u8g2_DrawFrame(u8g2,
                   (uint8_t)bar_x,
                   (uint8_t)bar_y,
                   (uint8_t)bar_w,
                   6u);

    if (fill_w > 0)
    {
        u8g2_DrawBox(u8g2,
                     (uint8_t)(bar_x + 1),
                     (uint8_t)(bar_y + 1),
                     (uint8_t)fill_w,
                     4u);
    }
}

/* -------------------------------------------------------------------------- */
/* 怨듯넻 Flight renderer                                                        */
/*                                                                            */
/* ?곗씠???곌껐 諛⑸쾿                                                             */
/* 1) APP_STATE ???먮낯 ?꾨뱶瑜??붾㈃ 肄붾뱶?먯꽌 吏곸젒 ?쎌? ?딅뒗??                 */
/* 2) Vario_State.c 媛 APP_STATE snapshot ??memcpy/媛怨듯빐??                   */
/*    vario_runtime_t 濡?怨듦컻?쒕떎.                                             */
/* 3) ?ш린 renderer ??                                                        */
/*       const vario_runtime_t *rt = Vario_State_GetRuntime();                 */
/*    濡?rt ?ъ씤?곕? ?살? ??                                                  */
/*       rt->alt1_absolute_m                                                   */
/*       rt->alt2_relative_m                                                   */
/*       rt->alt3_accum_gain_m                                                 */
/*       rt->baro_vario_mps                                                    */
/*       rt->ground_speed_kmh                                                  */
/*       rt->heading_deg                                                       */
/*    媛숈? "?곸쐞 ?덉씠??怨듦컻 ?꾨뱶" 留?draw ?쒕떎.                              */
/* 4) ??UI ??ぉ???꾩슂?섎㈃ ???뚯씪?먯꽌 APP_STATE 瑜??ㅼ?吏 留먭퀬,              */
/*    Vario_State.h/.c ??vario_runtime_t ??field 瑜?異붽??섍퀬                 */
/*    ?ш린?쒕뒗 洹?field 瑜??쎌뼱 洹몃━湲곕쭔 ?쒕떎.                                 */
/*                                                                            */
/* ?고듃/醫뚰몴 議곗젙 諛⑸쾿                                                          */
/* - ?곷떒 留ㅽ겕濡?釉붾줉??FONT / *_X / *_Y / *_PAD / *_GAP 媛믩쭔 議곗젙?쒕떎.        */
/* - right aligned block ? right_limit_x 怨꾩궛 ?섎굹濡?媛숈씠 ?吏곸씤??           */
/* - decimal ?レ옄 紐⑥뼇? vario_display_draw_decimal_value() 媛 ?대떦?쒕떎.       */
/* -------------------------------------------------------------------------- */
void Vario_Display_RenderFlightPage(u8g2_t *u8g2, vario_flight_page_mode_t mode)
{
    const vario_viewport_t *v;
    const vario_runtime_t  *rt;
    const vario_settings_t *settings;
    vario_display_page_cfg_t cfg;
    float avg_vario_mps;
    float avg_speed_kmh;
    float bar_vario_mps;
    float bar_avg_vario_mps;
    float bar_speed_kmh;
    float bar_avg_speed_kmh;
    vario_display_nav_solution_t nav;
    vario_viewport_t trail_v;

    if (u8g2 == NULL)
    {
        return;
    }

    v = Vario_Display_GetFullViewport();
    rt = Vario_State_GetRuntime();
    settings = Vario_Settings_Get();

    if ((v == NULL) || (rt == NULL) || (settings == NULL))
    {
        return;
    }

    memset(&cfg, 0, sizeof(cfg));
    switch (mode)
    {
        case VARIO_FLIGHT_PAGE_SCREEN_1:
            cfg.show_compass = true;
            break;

        case VARIO_FLIGHT_PAGE_SCREEN_2_TRAIL:
            cfg.show_trail_background = true;
            break;

        case VARIO_FLIGHT_PAGE_SCREEN_3_STUB:
        default:
            cfg.show_attitude_indicator = true;
            break;
    }

    u8g2_SetFontMode(u8g2, 1);

    vario_display_update_dynamic_metrics(rt, settings);
    vario_display_get_average_values(rt, settings, &avg_vario_mps, &avg_speed_kmh);
    s_vario_ui_dynamic.fast_average_glide_valid =
        vario_display_compute_glide_ratio(avg_speed_kmh,
                                          avg_vario_mps,
                                          &s_vario_ui_dynamic.fast_average_glide_ratio);
    vario_display_update_slow_number_caches(rt);
    vario_display_get_bar_display_values(rt,
                                         avg_vario_mps,
                                         avg_speed_kmh,
                                         &bar_vario_mps,
                                         &bar_avg_vario_mps,
                                         &bar_speed_kmh,
                                         &bar_avg_speed_kmh);
    vario_display_compute_nav_solution(rt, &nav);

    if (cfg.show_trail_background != false)
    {
        /* ------------------------------------------------------------------ */
        /* PAGE 2 trail ? 醫???14px bar 瑜??쒖쇅??李쎌씠 ?꾨땲??               */
        /* flight full viewport ?꾩껜瑜?諛곌꼍 canvas 濡??ъ슜?쒕떎.                */
        /* side bar / top / bottom block ? ?댄썑 overlay 濡??㏐렇?ㅼ쭊??        */
        /* ------------------------------------------------------------------ */
        trail_v = *v;
        vario_display_draw_trail_background(u8g2, &trail_v, rt, settings);
    }

    vario_display_draw_vario_side_bar(u8g2, v, bar_vario_mps, bar_avg_vario_mps);
    vario_display_draw_gs_side_bar(u8g2, v, rt, bar_speed_kmh, bar_avg_speed_kmh);

    vario_display_draw_top_left_metrics(u8g2, v, rt);
    vario_display_draw_top_center_clock(u8g2, v, rt, settings);
    vario_display_draw_top_right_altitudes(u8g2, v, rt);

    if (cfg.show_compass != false)
    {
        vario_display_draw_compass(u8g2, v, rt, &nav);
    }
    else if (cfg.show_attitude_indicator != false)
    {
        vario_display_draw_attitude_indicator(u8g2, v, rt);
    }

    vario_display_draw_altitude_sparkline(u8g2, v, rt);

    if (cfg.show_stub_overlay != false)
    {
        vario_display_draw_stub_overlay(u8g2,
                                        v,
                                        (cfg.stub_title != NULL) ? cfg.stub_title : "PAGE 3",
                                        (cfg.stub_subtitle != NULL) ? cfg.stub_subtitle : "UI STUB");
    }

    vario_display_draw_vario_value_block(u8g2, v, rt);
    vario_display_draw_speed_value_block(u8g2, v, rt);
    vario_display_draw_lower_left_glide_computer(u8g2, v, rt);
    vario_display_draw_bottom_center_flight_time(u8g2, v, rt);

    Vario_Display_DrawRawOverlay(u8g2, v);
}

void Vario_Display_SetNavTargetMode(vario_nav_target_mode_t mode)
{
    if (mode > VARIO_NAV_TARGET_WP)
    {
        return;
    }

    s_vario_ui_dynamic.nav_mode = mode;
}

vario_nav_target_mode_t Vario_Display_GetNavTargetMode(void)
{
    return s_vario_ui_dynamic.nav_mode;
}

void Vario_Display_SetWaypointManual(int32_t lat_e7, int32_t lon_e7, bool valid)
{
    s_vario_ui_dynamic.wp_lat_e7 = lat_e7;
    s_vario_ui_dynamic.wp_lon_e7 = lon_e7;
    s_vario_ui_dynamic.wp_valid = valid;
}

void Vario_Display_ResetDynamicMetrics(void)
{
    vario_nav_target_mode_t nav_mode;
    int32_t wp_lat_e7;
    int32_t wp_lon_e7;
    bool wp_valid;
    bool trail_heading_up_mode;

    nav_mode = s_vario_ui_dynamic.nav_mode;
    wp_lat_e7 = s_vario_ui_dynamic.wp_lat_e7;
    wp_lon_e7 = s_vario_ui_dynamic.wp_lon_e7;
    wp_valid = s_vario_ui_dynamic.wp_valid;
    trail_heading_up_mode = s_vario_ui_dynamic.trail_heading_up_mode;

    memset(&s_vario_ui_dynamic, 0, sizeof(s_vario_ui_dynamic));
    s_vario_ui_dynamic.nav_mode = nav_mode;
    s_vario_ui_dynamic.wp_lat_e7 = wp_lat_e7;
    s_vario_ui_dynamic.wp_lon_e7 = wp_lon_e7;
    s_vario_ui_dynamic.wp_valid = wp_valid;
    s_vario_ui_dynamic.trail_heading_up_mode = trail_heading_up_mode;
}

void Vario_Display_ToggleTrailHeadingUpMode(void)
{
    s_vario_ui_dynamic.trail_heading_up_mode =
        (s_vario_ui_dynamic.trail_heading_up_mode == false) ? true : false;
}

bool Vario_Display_IsTrailHeadingUpMode(void)
{
    return (s_vario_ui_dynamic.trail_heading_up_mode != false) ? true : false;
}

void Vario_Display_DrawRawOverlay(u8g2_t *u8g2, const vario_viewport_t *v)
{
    const vario_dev_settings_t *dev;
    const vario_runtime_t *rt;
    char text[48];

    if ((u8g2 == NULL) || (v == NULL))
    {
        return;
    }

    dev = Vario_Dev_Get();
    if ((dev == NULL) || (dev->raw_overlay_enabled == 0u))
    {
        return;
    }

    rt = Vario_State_GetRuntime();
    snprintf(text,
             sizeof(text),
             "P%ld T%ld BC%lu",
             (long)rt->pressure_hpa_x100,
             (long)rt->temperature_c_x100,
             (unsigned long)rt->gy86.baro.sample_count);

    u8g2_SetFont(u8g2, u8g2_font_4x6_tf);
    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawStr(u8g2,
                 (uint8_t)(v->x + 2),
                 (uint8_t)(v->y + v->h - 2),
                 text);
}
