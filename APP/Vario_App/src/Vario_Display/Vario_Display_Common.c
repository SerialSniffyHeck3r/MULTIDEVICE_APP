#include "Vario_Display_Common.h"

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
/* 아이콘 fallback                                                              */
/*                                                                            */
/* 일부 로컬 프로젝트의 Vario_Icon_Resources.h 는 저장소 최신본보다 오래되어      */
/* ALT1 / GS AVG 아이콘 심볼이 없을 수 있다.                                    */
/*                                                                            */
/* 사용자가 이 .c 파일 하나만 교체해도 빌드되게 하려고,                          */
/* "관련 매크로가 없을 때만" 동일 비트맵을 여기서 fallback 으로 선언한다.         */
/* 헤더가 최신이면 아래 블록은 전부 건너뛴다.                                   */
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
/* 기본 viewport 규격                                                          */
/*                                                                            */
/* 이 값들은 "아직 UI 엔진 bridge 가 실제 viewport 를 주입하기 전" 의 fallback   */
/* 이다.                                                                        */
/*                                                                            */
/* 정상 런타임에서는 매 프레임                                                   */
/*   UI_ScreenVario_Draw() -> Vario_Display_SetViewports()                      */
/* 호출이 먼저 일어나므로, renderer 는 실제 root compose viewport 를 보게 된다.  */
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
/* 공통 Flight UI layout                                                       */
/*                                                                            */
/* 이 섹션이 Screen 1/2/3 공용 shell 의 "치수 창고" 다.                         */
/*                                                                            */
/* 유지 규칙                                                                   */
/* 1) 화면 위치/폰트/간격은 이 섹션의 매크로부터 먼저 바꾼다.                   */
/* 2) renderer 는 APP_STATE 를 직접 읽지 않고                                 */
/*    Vario_State_GetRuntime() 가 만든 rt field 만 사용한다.                   */
/* 3) 표시 단위는 Vario_Settings.* helper 로만 바꾼다.                         */
/*    즉, ALT2 를 ft 로 따로 표시하고 싶으면 Vario_Settings 쪽 단위를 바꾸고,   */
/*    여기서는 unit text 와 rounded value helper 만 호출한다.                  */
/*                                                                            */
/* 좌표 기준                                                                   */
/* - 모든 Y 는 full viewport top 기준 offset 이다.                             */
/* - right aligned 항목은 right margin 매크로를 바꾸면 전체가 같이 이동한다.   */
/* - bottom value 영역은 meta(MAX/value) 와 main value box 를 분리해서         */
/*   숫자가 서로 겹치지 않게 고정 폭으로 유지한다.                              */
/* -------------------------------------------------------------------------- */
#define VARIO_UI_SIDE_BAR_W                       14
#define VARIO_UI_GAUGE_INSTANT_W                   8
#define VARIO_UI_GAUGE_AVG_W                       4
#define VARIO_UI_GAUGE_GAP_W                       1

/* -------------------------------------------------------------------------- */
/* 상단 좌측 INST Glide ratio gauge                                            */
/*                                                                            */
/* 사용자의 최신 요구사항                                                      */
/* - 좌측 세로 VARIO bar(14 px)와 2 px gap 뒤, 즉 x=16부터 시작한다.          */
/* - gauge 우측 끝은 "화면 1/3 지점 + 16 px" 이다.                            */
/* - gauge 자체는 화면 맨 위에서 시작하는 6 px 높이 horizontal bar 이고,       */
/*   눈금 역시 top edge 에서 아래로 내려온다.                                  */
/* - generic sailplane UI 스케일로 60:1 을 full scale 로 잡아                 */
/*   5:1 minor / 10:1 major tick 을 배치한다.                                  */
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
/* 하단 중앙 CLOCK                                                             */
/*                                                                            */
/* 기존 상단 중앙 시계는 새 glide gauge 와 시각적으로 부딪히므로,              */
/* 사용자가 지정한 대로 FLT TIME 바로 위 2 px 위치로 옮긴다.                  */
/* -------------------------------------------------------------------------- */
#define VARIO_UI_BOTTOM_CLOCK_GAP_Y                2

/* -------------------------------------------------------------------------- */
/* 상단 우측 ALT block                                                         */
/* - ALT1은 화면 제일 위에 붙인다.                                              */
/* - ALT2는 ALT1 바로 아래에 "중간 크기" row 로 내린다.                        */
/* - ALT3는 ALT2 아래에 독립 row 로 둔다.                                       */
/* - 세 row 모두 right aligned 로 계산해서 벽과 겹치지 않게 한다.              */
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
/* 중앙 NAV / COMPASS                                                          */
/* - 지름을 5px 키우라는 요구를 반영하기 위해 diameter grow 상수를 분리했다.    */
/* - 실제 반지름 증가는 정수 픽셀 좌표계라 ceil(5/2)=3 px 로 반올림한다.       */
/* - 원의 맨 아래가 자기 영역의 최하단에 닿도록 center_y 를 계산한다.          */
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
/* 하단 MAX/meta + main value block                                            */
/* - meta label/value 는 main number box 와 분리된 Y 를 써서 겹침을 방지한다.   */
/* - main number box 는 고정 폭/고정 높이이며 decimal 은 dot 없이 top-frac 로   */
/*   그린다.                                                                   */
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
/* - left vario  : inside edge(right edge) 에 tick 가 붙는다.                   */
/* - right GS    : inside edge(left edge) 에 tick 가 붙는다.                    */
/* - 0.0 m/s line 은 3px 두께 + 최장 길이로 그려 중심을 강하게 표시한다.        */
/*                                                                            */
/* 이번 변경의 핵심                                                            */
/* - 좌측 VARIO 는 설정값 4.0 / 5.0 에 따라 full-scale 이 바뀐다.              */
/* - 우측 GS 는 설정된 top speed 를 full-scale 로 쓰되,                        */
/*   50 km/h 미만이면 5 / 2.5 tick, 그 이상이면 10 / 5 tick 을 사용한다.      */
/* - tick 의 X 위치 / 길이 / bar X 위치 / bar 폭은 기존 그대로 유지한다.      */
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
/* - 소수점은 '.' 을 찍지 않는다.                                               */
/* - frac digit 은 정수부 오른쪽 위에 작은 폰트로 top-align 한다.               */
/* - gap/top bias 를 바꾸면 frac digit 의 떠 있는 느낌을 조절할 수 있다.       */
/* -------------------------------------------------------------------------- */
#define VARIO_UI_DECIMAL_FRAC_GAP_X                1
#define VARIO_UI_DECIMAL_SIGN_GAP_X                1
#define VARIO_UI_DECIMAL_FRAC_TOP_BIAS_Y           0

/* -------------------------------------------------------------------------- */
/* Flight average / peak cache                                                 */
/*                                                                            */
/* APP_STATE snapshot 구조는 그대로 유지하고,                                  */
/* display 계층이 publish 값(5 Hz)을 다시 누적해서 평균/Top Speed 를 만든다.    */
/* -------------------------------------------------------------------------- */
#define VARIO_UI_AVG_BUFFER_SIZE                  640u
#define VARIO_UI_SLOW_SPEED_TAU_S                    1.10f
#define VARIO_UI_SLOW_GLIDE_UPDATE_MS                2000u

/* -------------------------------------------------------------------------- */
/* 수동 WP 기본값                                                              */
/* -------------------------------------------------------------------------- */
#define VARIO_UI_DEFAULT_WP_VALID                 0u
#define VARIO_UI_DEFAULT_WP_LAT_E7                0
#define VARIO_UI_DEFAULT_WP_LON_E7                0

/* -------------------------------------------------------------------------- */
/* 폰트 지정                                                                   */
/*                                                                            */
/* 사용법                                                                     */
/* - 글꼴을 바꾸고 싶으면 아래 FONT 매크로만 교체한다.                          */
/* - 좌표와 폰트를 함께 손보면 충돌 없이 레이아웃을 재배치하기 쉽다.            */
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
    /*  display layer 가 유지하는 lightweight cache                            */
    /*                                                                        */
    /*  - top_speed_kmh       : flight 중 peak GS                              */
    /*  - filtered_gs_bar_kmh : GS side bar 전용 smoothing cache               */
    /*                                                                        */
    /*  예전 renderer 가 쓰던                                                  */
    /*  - filtered_vario_bar_mps                                               */
    /*  - vario_bar_zero_latched                                               */
    /*  는 현재 경로에서 더 이상 사용되지 않으므로 제거한다.                  */
    /*  좌측 VARIO bar 는 runtime snapshot 이 준 fast path 값을               */
    /*  그대로 받아 스케일링만 수행한다.                                      */
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
    /* 실제 바리오류의 ALT history graph 처럼                                  */
    /* - scale 확장은 즉시 반영                                                */
    /* - scale 축소는 여러 frame 관찰 후 천천히 반영                           */
    /* - center 는 low-pass 로 따라가되, history 가 경계 근처까지 오면         */
    /*   즉시 recenter 해서 graph 밖으로 튀지 않게 한다.                      */
    /* ---------------------------------------------------------------------- */
    bool     altitude_graph_initialized;
    uint8_t  altitude_graph_shrink_votes;
    float    altitude_graph_center_m;
    float    altitude_graph_span_m;
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
/* bar smoothing / runtime selection 로직이 helper 정의보다 먼저 등장하므로,     */
/* 설정 기반 scale helper 의 prototype 만 앞쪽에 배치한다.                     */
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

static vario_display_dynamic_t s_vario_ui_dynamic =
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
/* 내부 helper: viewport sanity clamp                                          */
/*                                                                            */
/* 이유                                                                          */
/* - bridge 가 잘못된 값을 주더라도 renderer 가 음수 width/height 를 잡지 않게   */
/*   방어한다.                                                                  */
/* - 기존 고정 240x128 좌표계를 벗어나는 값을 잘라낸다.                         */
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
    /* - 아이콘 비트맵은 Vario_Icon_Resources.h 에서만 관리한다.                */
    /* - 여기서는 digit_ch 로 어떤 리소스를 찍을지만 결정한다.                  */
    /* - 위치를 옮기고 싶으면 caller 의 x / y_top 계산식만 수정하면 된다.       */
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
/* ALT1 / ALT2 / ALT3 가 서로 다른 단위를 가질 수 있게 하기 위한 helper 다.      */
/* 현재 common shell 에서는                                                   */
/* - ALT1 : settings->altitude_unit                                            */
/* - ALT2 : settings->alt2_unit                                                */
/* - ALT3 : settings->altitude_unit                                            */
/* 조합을 사용한다.                                                            */
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
    /*  Alt2의 SMART FUSE는 내부 파이프 이름을 직접 노출하지 않고,             */
    /*  현재 low-level이 제공하는 assisted absolute altitude 중                */
    /*  가장 도움이 되는 경로를 자동 선택해서 보여 주는 user-facing 모드다.   */
    /*                                                                        */
    /*  우선순위                                                              */
    /*  1) IMU aided fused altitude                                            */
    /*  2) baro + GPS anchor fused altitude                                    */
    /*  3) baro가 없으면 GPS altitude fallback                                 */
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
    if ((buf == NULL) || (buf_len == 0u) || (rt == NULL))
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /* 큰 ALT1 숫자는 APP_STATE unit bank를 그대로 다시 읽지 않고,             */
    /* Vario_State가 5Hz cadence로 publish한 "표시 전용 고도 숫자"를 쓴다.     */
    /*                                                                        */
    /* 이유                                                                   */
    /* - low-level source selection은 그대로 존중한다.                         */
    /* - 하지만 화면에 보이는 큰 숫자만은 upper layer의 단순 필터/hysteresis   */
    /*   결과를 사용해서 과도한 떨림 없이 읽기 쉽게 만든다.                    */
    /* - feet/meter 환산은 기존 helper를 그대로 사용한다.                      */
    /* ---------------------------------------------------------------------- */
    vario_display_format_altitude_with_unit(buf,
                                            buf_len,
                                            rt->baro_altitude_m,
                                            unit);
}

static void vario_display_format_speed_value(char *buf, size_t buf_len, float speed_kmh)
{
    float display_value;

    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    display_value = Vario_Settings_SpeedKmhToDisplayFloat(speed_kmh);
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
        if (rt->gs_bar_speed_kmh > s_vario_ui_dynamic.top_speed_kmh)
        {
            s_vario_ui_dynamic.top_speed_kmh = rt->gs_bar_speed_kmh;
        }
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
    target_speed_kmh = rt->filtered_ground_speed_kmh;
    if (target_speed_kmh < 0.0f)
    {
        target_speed_kmh = 0.0f;
    }

    /* ------------------------------------------------------------------ */
    /* 큰 GS 숫자만 느리게 보이게 만든다.                                  */
    /*                                                                    */
    /* - source : Vario_State 가 10 Hz GPS 샘플로 만든 filtered GS        */
    /* - cadence: display layer 에서는 1초에 1번만 latch 한다.            */
    /* - trainer mode 도 같은 runtime.filtered_ground_speed_kmh 경로를   */
    /*   사용하므로 별도 분기 없이 trainer synthetic speed 가 표시된다.   */
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
    /*  over-range 삭제 패턴을 표시하려면                                       */
    /*  좌측 VARIO bar 입력이 full-scale 을 약간 넘을 수 있어야 한다.          */
    /*                                                                        */
    /*  따라서 renderer 입력 clamp 는                                         */
    /*  - vario : 현재 scale 의 2배까지                                        */
    /*  - GS    : 현재 설정 top speed 까지                                     */
    /*  로 맞춘다.                                                             */
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
    /* 좌측 VARIO bar는 display layer에서 추가 필터를 넣지 않는다.            */
    /*                                                                        */
    /* 우측 GS bar 역시 raw path 를 그대로 쓴다.                             */
    /* - 큰 GS 숫자만 1초 cadence / filtered path 를 타고,                    */
    /* - 옆 그래프 bar 는 날것 속도를 즉시 보여 준다.                        */
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
    /*  좌측 VARIO gauge 는 현재 4.0 / 5.0 두 값만 지원한다.                  */
    /*                                                                        */
    /*  저장된 값이 옛 firmware 의 흔적으로 다른 값이어도                    */
    /*  draw layer 에서는 4.0 또는 5.0 계약으로 snap 해서 사용한다.           */
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
    /*  over-range 표시 규칙                                                   */
    /*                                                                        */
    /*  full scale 를 넘어가면 gauge 전체를 꽉 채우는 대신,                   */
    /*  중심축(0.0 근처) 쪽 slot 부터 0.5 m/s 단위로 하나씩 비운다.            */
    /*                                                                        */
    /*  예) 5.0 scale 에서 +5.5 => 0.0~0.5 slot 이 비워지고                    */
    /*      바는 위쪽 4.5 m/s 구간만 남는다.                                  */
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
    /*  1) 정상 범위(0 ~ full scale)                                            */
    /*                                                                         */
    /*  기존과 동일하게 중심축에서 바깥 방향으로 "연속적으로" 채운다.           */
    /*  이 구간의 그래픽/스케일 동작은 건드리지 않는다.                        */
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
    /*  2) 오버레인지(full scale 초과)                                          */
    /*                                                                         */
    /*  기존 버그: 0.5 m/s slot 단위로 한 칸씩 뚝뚝 지워져서                    */
    /*  연속적이지 않게 보였다.                                                */
    /*                                                                         */
    /*  수정: 초과분(over_range_mps)을 "정상 범위와 완전히 같은 스케일 식"으로  */
    /*  픽셀로 환산해서, 중심축 쪽부터 연속적으로 지운다.                      */
    /*                                                                         */
    /*  - + 방향: 꽉 찬 바의 아래쪽(0.0 쪽)부터 연속적으로 지움                */
    /*  - - 방향: 꽉 찬 바의 위쪽(0.0 쪽)부터 연속적으로 지움                  */
    /*                                                                         */
    /*  즉, 0 -> 4.0/5.0 까지 올라갈 때의 부드러움과 동일한 방식으로            */
    /*  4.0/5.0 초과분도 부드럽게 "지워지며" 표현된다.                         */
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

static void vario_display_draw_decimal_value(u8g2_t *u8g2,
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
/* 현재 VARIO 큰 숫자 전용 fixed-slot renderer                                  */
/*                                                                            */
/* 사용자가 요구한 규칙                                                        */
/* 1) 10의 자리 칸은 "항상 예약"해 둔다.                                       */
/* 2) 값이 1.0 이어도 tens 칸은 남아 있고, 실제 숫자는 ones 칸에만 그린다.       */
/* 3) 값이 0.x 일 때는 ones 칸에 반드시 '0' 을 그린다.                         */
/* 4) 즉, "leading zero 제거"는 tens 자리만 숨기고, decimal 직전 0 은 숨기지   */
/*    않는다.                                                                  */
/*                                                                            */
/* 이 helper 는 current VARIO 큰 숫자 블록에만 쓰고, 다른 숫자 UI 는 건드리지   */
/* 않는다.                                                                     */
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
    /* current VARIO block 는 absolute value 만 그리므로 sign 은 사용하지 않는다.*/
    /* decimal 이 없거나 digits 가 box 설계보다 길면, 기존 generic renderer 로   */
    /* 안전하게 되돌린다.                                                      */
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

    u8g2_SetFontPosTop(u8g2);
    u8g2_SetFont(u8g2, VARIO_UI_FONT_BOTTOM_MAIN);

    /* tens slot: 10 이상일 때만 그림. 1.x / 0.x 에서는 빈칸만 예약해 둔다. */
    if (whole_len >= 2u)
    {
        digit_ch[0] = whole[0];
        digit_ch[1] = '\0';
        u8g2_DrawStr(u8g2, draw_x, whole_top, digit_ch);
    }

    /* ones slot: 0.x 일 때도 반드시 '0' 을 보여야 하므로 항상 마지막 digit 을 그림. */
    digit_ch[0] = whole[whole_len - 1u];
    digit_ch[1] = '\0';
    u8g2_DrawStr(u8g2, (int16_t)(draw_x + digit_w), whole_top, digit_ch);

    u8g2_SetFont(u8g2, VARIO_UI_FONT_BOTTOM_FRAC);
    u8g2_DrawStr(u8g2, frac_x, frac_top, frac);

    /* ---------------------------------------------------------------------- */
    /* current VARIO 값 옆 LED-style 방향 아이콘                               */
    /*                                                                        */
    /* 사용자의 최신 요구사항                                                  */
    /* - 큰 VARIO 값의 작은 소수 digit 과 동일한 top Y 를 기준으로            */
    /*   우측 2 px 옆에 상승 LED icon slot 을 예약한다.                       */
    /* - 하강 LED icon 은 그 바로 아래 2 px 간격으로 고정 배치한다.           */
    /* - 값이 상승/하강/0 인지에 따라 해당 slot 을 켜거나 끄기만 한다.         */
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
    /* 사용자가 요청한 대로, 기존 major / minor tick 은 그대로 둔다.       */
    /*                                                                    */
    /* 기존 tick 배치는 0 / 5 / 10 / 15 / 20 이고, 이 상태에서는           */
    /* major 사이에 작은 눈금이 1개만 존재한다.                            */
    /*                                                                    */
    /* 여기서는 기존 눈금을 변경하지 않고, 그 "사이" 에만 동일한 minor    */
    /* tick 을 하나 더 추가해서                                            */
    /*   major - minor - minor - minor - major                            */
    /* 패턴이 되도록 만든다.                                               */
    /*                                                                    */
    /* 즉 0..20 스케일 기준으로 2.5 / 7.5 / 12.5 / 17.5 위치에             */
    /* 기존 minor 와 동일한 높이의 tick 을 추가한다.                      */
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
        /* instant glide bar 는 왼쪽에서 오른쪽으로 차오르는 fill bar다.      */
        /* 이동 포인터처럼 보이지 않도록, gauge left 기준 누적 width 로 그린다.*/
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
    /*  상단 좌측 glide section                                                */
    /*                                                                        */
    /*  최신 요구사항                                                          */
    /*  - 상단 gauge 는 그대로 instant bar + average marker 를 유지한다.       */
    /*  - 작게 붙여 두던 평균 활공비 수치는 완전히 제거한다.                   */
    /*  - 크게 보이는 주 수치는 더 이상 instantaneous 가 아니라               */
    /*    average glide ratio 를 표시한다.                                    */
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
    /* 현재 시각은 더 이상 top-center 에 두지 않는다.                          */
    /*                                                                        */
    /* 이유                                                                   */
    /* - 상단의 새 INST glide gauge 가 화면 top edge 를 사용한다.               */
    /* - 사용자가 지정한 새 위치는 FLT TIME 바로 위 2 px 이다.                 */
    /* - 따라서 flight-time box의 top Y를 공용 helper로 구하고,                */
    /*   그 위에 clock font height 만큼 올린 뒤 2 px gap 을 남긴다.           */
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
    /* 음수 부호는 항상 10k digit slot 에 고정한다.                           */
    /*                                                                        */
    /* ALT2 value font 는 고정폭 계열이라,                                     */
    /* - 양수  : " 1234" / "12345"                                            */
    /* - 음수  : "-1234" / "- 123"                                            */
    /* 형태로 formatting 하면 sign column 이 항상 좌측 slot 에 고정된다.       */
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
    /* 좌하단 glide block 상단 context label                               */
    /* - target name 이 있으면 그 이름을 우선 사용한다.                    */
    /* - 이름이 없으면 source short label(HME/LND/WPT/...) 를 쓴다.        */
    /* - 길이는 최대 8자로 잘라, 기존 숫자 블록 위에 작게 얹는다.          */
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
    /* 좌하단 glide-computer 3중 세트                                          */
    /*                                                                        */
    /* 이번 수정 요구사항                                                       */
    /* - Final Glide / Arr H / Distance 세 row 를 "묶어서" Y축 아래로 내린다.  */
    /* - 단, Distance value row 의 top Y 는 FLT TIME value row 와 정확히 맞춘다.*/
    /* - Arr H / Final Glide 는 그로부터 기존 row 간격 규칙을 그대로 유지한다. */
    /*                                                                        */
    /* 즉, anchor 는 Distance value row 이고,                                  */
    /* 나머지 두 row 는 상대 위치만 그대로 유지한 채 같이 이동한다.            */
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
    /* Distance value row 가 FLT TIME value 와 동일 top Y 를 쓰도록            */
    /* block 전체의 anchor(top_y)를 역산한다.                                  */
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
    /* 새 context label 좌표                                                */
    /* - X 는 Final Glide XBM 의 실제 시작 X                               */
    /* - Y 는 Final Glide 숫자 top 보다 2 px 위                            */
    /* - font 는 unit label 과 동일                                         */
    /* - 좌측 정렬                                                          */
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
    }
    else
    {
        snprintf(distance_text, sizeof(distance_text), "---.-");
    }

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
            /* ALT2가 absolute mode일 때도 ALT1과 같은 5Hz filtered absolute       */
            /* 숫자를 사용한다.                                                    */
            /*                                                                    */
            /* 현재 Alt1은 legal/competition용 barometric path로 고정이므로        */
            /* 여기서는 그 숫자를 그대로 복제해서 보여 주기만 한다.              */
            /* ------------------------------------------------------------------ */
            vario_display_format_filtered_selected_altitude(value_buf,
                                                            value_len,
                                                            rt,
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
                vario_display_format_altitude_from_unit_bank(value_buf,
                                                             value_len,
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
                vario_display_format_altitude_from_unit_bank(value_buf,
                                                             value_len,
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
            /*  ALT2 relative는 기준점이 VARIO app setting 에 있으므로             */
            /*  APP_STATE의 정적 unit bank에 그대로 존재할 수는 없다.              */
            /*                                                                    */
            /*  대신 backing field 자체를 Vario_State.c 에서                      */
            /*  centimeter 해상도로 계산해 두었기 때문에                           */
            /*  여기서 feet로 변환해도 1m 계단 해상도에 묶이지 않는다.             */
            /* ------------------------------------------------------------------ */
            vario_display_format_altitude_with_unit(value_buf,
                                                    value_len,
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
    vario_display_format_altitude_with_unit(alt3_text, sizeof(alt3_text), rt->alt3_accum_gain_m, settings->altitude_unit);
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
    value_x = (int16_t)(unit_x - VARIO_UI_TOP_ALT1_VALUE_UNIT_GAP - alt1_value_box_w);
    vario_display_draw_text_box_top(u8g2,
                                    value_x,
                                    (int16_t)(alt1_top_y + ((alt1_row_h - alt1_value_h) / 2)),
                                    alt1_value_box_w,
                                    VARIO_UI_ALIGN_RIGHT,
                                    VARIO_UI_FONT_ALT1_VALUE,
                                    alt1_text);
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
    instant_x  = (int16_t)(left_bar_x + 1);   /* 기존 instant bar X 유지 */
    avg_x      = (int16_t)(left_bar_x + 10);  /* 기존 average bar X 유지 */
    tick_x     = left_bar_x;                  /* 기존 tick 시작 X 유지 */

    /* ---------------------------------------------------------------------- */
    /*  좌측 VARIO 는 viewport height 를 쓰지 않고 실제 LCD 전체 높이를 쓴다.  */
    /*                                                                        */
    /*  - 4.0 scale : 화면 맨 위 +4.0 / 맨 아래 -4.0                          */
    /*  - 5.0 scale : 화면 맨 위 +5.0 / 맨 아래 -5.0                          */
    /*  - 가운데 3 px zero band 는 기존 그대로 유지                           */
    /* ---------------------------------------------------------------------- */
    vario_display_get_vario_screen_geometry(&top_limit_y,
                                            &zero_top_y,
                                            &zero_bottom_y,
                                            &bottom_limit_y,
                                            &top_span_px,
                                            &bottom_span_px);

    /* ---------------------------------------------------------------------- */
    /*  tick Y 역시 bar fill 과 같은 scale 식을 공유해야                      */
    /*  숫자 스케일과 실제 fill 이 서로 틀어지지 않는다.                      */
    /*                                                                        */
    /*  - 0.5 단위 : small tick                                                */
    /*  - 1.0 단위 : major tick                                                */
    /*  - tick 의 X 시작점 / 길이 / 모양은 기존 그대로 유지                   */
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
    /*  - scale 안쪽 : 연속값 -> pixel 로 환산                                 */
    /*  - scale 초과 : 중심축부터 0.5 m/s slot 을 한 칸씩 지우는 패턴         */
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

    /* center zero line 은 fill 후 다시 덮어 그려 기준선이 항상 살아 있게 유지 */
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
    if (clamped_speed > 0.0f)
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
    /* 평균 속도 marker 는 기존과 동일하게 speed bar 의 좌측 lane 을 쓴다.     */
    /* ---------------------------------------------------------------------- */
    clamped_avg_speed = vario_display_clampf(average_speed_kmh, 0.0f, gs_top_kmh);
    if (clamped_avg_speed > 0.0f)
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
    /* 사용자가 요청한 점검 결과                                              */
    /* - 전용 XBM 은 정의만 되어 있고 실제 draw path 가 빠져 있었다.           */
    /* - 이제 speed_to_fly_valid 가 살아 있으면 항상 같은 좌측 lane 에 draw 한다.*/
    /* - speed_to_fly_kmh 가 GS graph 상단값을 넘으면 marker 위치만            */
    /*   top tick 에 clamp 하여 "안 보이는" 현상을 막는다.                    */
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
    /* 현재 큰 VARIO 값은 leading zero 전체를 지우지 않는다.                    */
    /* - 사용자가 원하는 건 ".5" 가 아니라 "[blank]0 5" 구조다.               */
    /* - 즉, tens slot 만 빈칸으로 두고 decimal 직전 ones zero 는 살린다.       */
    /* - 그래서 여기서는 trim helper 를 호출하지 않고, draw 단계에서            */
    /*   fixed-slot renderer 가 tens slot 만 숨기게 한다.                      */
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
        /* 최신 요구사항                                                        */
        /* - 라벨 "Est.TE" 는 기존 absolute 위치를 그대로 유지한다.            */
        /* - 실제 값은 MAX VARIO 와 동일한 top Y / 동일한 폰트로 표시한다.      */
        /* - 값 블록 폭도 MAX VARIO 와 같은 고정 폭을 사용하고, right align 한다.*/
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
        /* show_max_vario 가 꺼져 있어도                                        */
        /* Est.TE 값의 폰트 / Y / 고정폭 계약은 동일하게 유지한다.              */
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
    /* 사용자가 지정한 4개 요소를 하나의 speed block 으로 같이 4px 내린다. */
    /*                                                                    */
    /* 이동 대상                                                            */
    /* - "MC" 라벨                                                         */
    /* - MC 값                                                             */
    /* - 최대 속도                                                         */
    /* - 현재 속도 본값/소수값                                              */
    /*                                                                    */
    /* 구현 방침                                                            */
    /* - 별도 shift 상수나 런타임 보정 로직을 추가하지 않는다.             */
    /* - 이 speed block 의 기준 절대 좌표식 자체를 교체한다.              */
    /*                                                                    */
    /* 기존 식은 bottom pad(4px)를 빼고 있어서, 화면 하단에서 4px 떠 있었다.*/
    /* 그 -4 성격의 보정을 제거해서 기준 y 를 그대로 아래로 4px 내린다.   */
    /* max / MC 는 value_box_y 를 기준으로 위쪽에 붙어 있으므로 함께 이동한다.*/
    /* ------------------------------------------------------------------ */
    value_box_y = (int16_t)(v->y + v->h - value_box_h);

    vario_display_format_speed_value(value_text,
                                     sizeof(value_text),
                                     (s_vario_ui_dynamic.displayed_speed_valid != false) ?
                                         s_vario_ui_dynamic.displayed_speed_kmh :
                                         rt->ground_speed_kmh);
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
    /* - top speed max block 의 왼쪽에 별도 fixed box 를 유지한다.              */
    /* - 사용자가 요청한 대로, 이 박스의 왼쪽에는 작은 "MC" 라벨을             */
    /*   ALT2/ALT3 unit 과 동일한 폰트로 붙인다.                               */
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

    vario_display_draw_decimal_value(u8g2,
                                     value_box_x,
                                     value_box_y,
                                     VARIO_UI_BOTTOM_BOX_W,
                                     value_box_h,
                                     VARIO_UI_ALIGN_RIGHT,
                                     VARIO_UI_FONT_BOTTOM_MAIN,
                                     VARIO_UI_FONT_BOTTOM_FRAC,
                                     value_text);
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
    /* 실제 바리오류 ALT graph 가 scale 을 단계적으로 확장하는 느낌을 따라      */
    /* raw span 에 headroom 을 얹은 뒤 50/100/200/500... m 단위로 quantize.   */
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
    /* trail breadcrumb 와 overlay marker 는 완전히 같은 local tangent      */
    /* 투영식을 써야 한다. 그래야 north-up / trail-up 어느 모드에서도         */
    /* 저장된 START / LAND / WPT 기호가 breadcrumb 배경과 정확히 맞물린다.   */
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
    /* START / LAND / WPT overlay 기호                                      */
    /* - START : hollow triangle                                            */
    /* - LAND  : hollow inverted triangle                                   */
    /* - WPT   : hollow circle                                              */
    /*                                                                    */
    /* 반지름 4 px 는 breadcrumb dot(1~3 px) 보다 분명히 크고,             */
    /* 240x128 trail page 에서 과하게 시야를 가리지 않는 절충값이다.         */
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
    /* trail page 는 heading-up / north-up 모두 "현재 기체 중심 고정" 이다. */
    /* 즉, 배경 trail 이 움직이고 화살표는 중심에 남는다.                  */
    /* - heading-up : 배경을 -heading 만큼 회전, 화살표는 위를 본다.        */
    /* - north-up   : 배경은 회전하지 않고, 화살표만 현재 heading 을 본다.  */
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
            /* trail dot 와 overlay marker 는 같은 화면 투영 helper 를 쓴다. */
            /* north-up / heading-up 전환 시에도 같은 기준을 유지해          */
            /* marker 와 breadcrumb 간 상대 위치가 흔들리지 않게 한다.       */
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
    /* 사용자가 미리 저장한 출발점 / 착륙지 / 포인트를 trail 위에 겹쳐 보면   */
    /* 현재 flight breadcrumb 과 site 기준점의 상대 관계를 빠르게 읽을 수 있다.*/
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
    /* 상단의 START ---.-Km 문구는 사용자가 삭제를 요구했으므로 더 이상        */
    /* draw 하지 않는다. 다만 distance formatter 는 다른 빌드에서              */
    /* static inline 최적화 경고를 피하려고 호출만 유지한다.                    */
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
    /* 화면 정북(상단)은 항상 현재 heading 을 가리키는 heading-up compass 다.  */
    /* 실제 바리오 계열처럼 top bug 를 고정하고, 장미판만 회전시킨다.          */
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
    /* - 별도 글씨는 그리지 않는다.                                            */
    /* - 4x4 diamond 점을 circle 내부에 배치한다.                              */
    /* - wind_from_deg 기준이므로, 현재 heading 을 빼서 heading-up 화면에서    */
    /*   상대위치로 회전시킨다.                                                */
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
    /* bank 는 world horizon 이 aircraft symbol 반대방향으로 기울도록          */
    /* -bank 를 사용한다.                                                      */
    /* pitch 는 nose-up(+)일 때 horizon 이 아래로 내려가도록 screen y + 로     */
    /* 옮긴다.                                                                 */
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
/* 공통 Flight renderer                                                        */
/*                                                                            */
/* 데이터 연결 방법                                                             */
/* 1) APP_STATE 의 원본 필드를 화면 코드에서 직접 읽지 않는다.                 */
/* 2) Vario_State.c 가 APP_STATE snapshot 을 memcpy/가공해서                    */
/*    vario_runtime_t 로 공개한다.                                             */
/* 3) 여기 renderer 는                                                         */
/*       const vario_runtime_t *rt = Vario_State_GetRuntime();                 */
/*    로 rt 포인터를 얻은 뒤,                                                  */
/*       rt->alt1_absolute_m                                                   */
/*       rt->alt2_relative_m                                                   */
/*       rt->alt3_accum_gain_m                                                 */
/*       rt->baro_vario_mps                                                    */
/*       rt->ground_speed_kmh                                                  */
/*       rt->heading_deg                                                       */
/*    같은 "상위 레이어 공개 필드" 만 draw 한다.                              */
/* 4) 새 UI 항목이 필요하면 이 파일에서 APP_STATE 를 뒤지지 말고,              */
/*    Vario_State.h/.c 의 vario_runtime_t 에 field 를 추가하고                 */
/*    여기서는 그 field 를 읽어 그리기만 한다.                                 */
/*                                                                            */
/* 폰트/좌표 조정 방법                                                          */
/* - 상단 매크로 블록의 FONT / *_X / *_Y / *_PAD / *_GAP 값만 조정한다.        */
/* - right aligned block 은 right_limit_x 계산 하나로 같이 움직인다.           */
/* - decimal 숫자 모양은 vario_display_draw_decimal_value() 가 담당한다.       */
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
        /* PAGE 2 trail 은 좌/우 14px bar 를 제외한 창이 아니라                */
        /* flight full viewport 전체를 배경 canvas 로 사용한다.                */
        /* side bar / top / bottom block 은 이후 overlay 로 덧그려진다.        */
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
