#include "Vario_Display_Common.h"

#include "Vario_Dev.h"
#include "Vario_State.h"
#include "Vario_Settings.h"
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
/* 상단 좌측 FLT/GLD block                                                     */
/* -------------------------------------------------------------------------- */
#define VARIO_UI_TOP_LEFT_PAD_X                    4
#define VARIO_UI_TOP_FLT_BASELINE_Y               14
#define VARIO_UI_TOP_GLD_BASELINE_Y               29

/* -------------------------------------------------------------------------- */
/* 상단 중앙 CLOCK                                                             */
/* - 사용자가 요구한 대로 현재 시각 폰트는 한 단계 낮춰 둔다.                  */
/* -------------------------------------------------------------------------- */
#define VARIO_UI_TOP_CLOCK_BASELINE_Y             12

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
/* -------------------------------------------------------------------------- */
#define VARIO_UI_VARIO_HALFSTEP_COUNT              8u
#define VARIO_UI_VARIO_HALFSTEP_MPS                0.5f
#define VARIO_UI_VARIO_MAX_ABS_MPS                 (VARIO_UI_VARIO_HALFSTEP_COUNT * VARIO_UI_VARIO_HALFSTEP_MPS)
#define VARIO_UI_VARIO_ZERO_LINE_W                 14u
#define VARIO_UI_VARIO_ZERO_LINE_THICKNESS          3u
#define VARIO_UI_GS_STEP_COUNT                    10u
#define VARIO_UI_GS_STEP_KMH                       5.0f
#define VARIO_UI_GS_MIN_VISIBLE_KMH               10.0f
#define VARIO_UI_GS_MAX_KMH                       70.0f
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
#define VARIO_UI_AVG_BUFFER_SIZE                  96u

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
    uint8_t  head;
    uint8_t  count;
    uint32_t last_publish_ms;
    uint32_t last_flight_start_ms;
    uint32_t last_bar_update_ms;
    float    top_speed_kmh;
    float    filtered_vario_bar_mps;
    float    filtered_gs_bar_kmh;
    bool     vario_bar_zero_latched;
    vario_nav_target_mode_t nav_mode;
    int32_t  wp_lat_e7;
    int32_t  wp_lon_e7;
    bool     wp_valid;
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
    bool        show_stub_overlay;
    const char *stub_title;
    const char *stub_subtitle;
} vario_display_page_cfg_t;

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
    0.0f,
    VARIO_NAV_TARGET_START,
    VARIO_UI_DEFAULT_WP_LAT_E7,
    VARIO_UI_DEFAULT_WP_LON_E7,
    (VARIO_UI_DEFAULT_WP_VALID != 0u) ? true : false
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

static void vario_display_format_altitude(char *buf, size_t buf_len, float altitude_m)
{
    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    snprintf(buf,
             buf_len,
             "%ld",
             (long)Vario_Settings_AltitudeMetersToDisplayRounded(altitude_m));
}

static void vario_display_format_signed_altitude(char *buf, size_t buf_len, float altitude_m)
{
    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    snprintf(buf,
             buf_len,
             "%ld",
             (long)Vario_Settings_AltitudeMetersToDisplayRounded(altitude_m));
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

static void vario_display_format_vario_value_signed(char *buf, size_t buf_len, float vario_mps)
{
    float display_value;

    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    display_value = Vario_Settings_VSpeedMpsToDisplayFloat(vario_mps);
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
    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    if (vario_display_absf(vario_mps) <= 0.05f)
    {
        snprintf(buf, buf_len, "--.-");
        return;
    }

    vario_display_format_vario_small_abs(buf, buf_len, vario_mps);
    if (Vario_Settings_Get()->vspeed_unit != VARIO_VSPEED_UNIT_FPM)
    {
        vario_display_trim_leading_zero(buf);
    }
}

static void vario_display_format_glide_ratio(char *buf, size_t buf_len, const vario_runtime_t *rt)
{
    float sink_mps;
    float glide_ratio;

    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    if (rt == NULL)
    {
        snprintf(buf, buf_len, "--.-");
        return;
    }

    sink_mps = -rt->baro_vario_mps;
    if ((sink_mps <= 0.15f) || (rt->ground_speed_kmh <= 1.0f))
    {
        snprintf(buf, buf_len, "--.-");
        return;
    }

    glide_ratio = (rt->ground_speed_kmh / 3.6f) / sink_mps;
    glide_ratio = vario_display_clampf(glide_ratio, 0.0f, 99.9f);
    snprintf(buf, buf_len, "%.1f", (double)glide_ratio);
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
                                         uint8_t count,
                                         uint8_t head,
                                         uint32_t latest_stamp_ms,
                                         uint32_t window_ms,
                                         float fallback_value,
                                         float *out_average)
{
    uint8_t used;
    uint8_t i;
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
        uint8_t idx;
        uint8_t reverse_index;

        reverse_index = (uint8_t)(count - 1u - i);
        idx = (uint8_t)((head + VARIO_UI_AVG_BUFFER_SIZE - 1u - reverse_index) % VARIO_UI_AVG_BUFFER_SIZE);

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
}

static void vario_display_update_dynamic_metrics(const vario_runtime_t *rt,
                                                 const vario_settings_t *settings)
{
    uint32_t sample_ms;
    uint8_t  idx;

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

    sample_ms = rt->last_publish_ms;
    if ((sample_ms == 0u) || (sample_ms == s_vario_ui_dynamic.last_publish_ms))
    {
        goto update_peak_only;
    }

    idx = s_vario_ui_dynamic.head;
    s_vario_ui_dynamic.sample_stamp_ms[idx] = sample_ms;
    s_vario_ui_dynamic.vario_mps[idx] = rt->baro_vario_mps;
    s_vario_ui_dynamic.speed_kmh[idx] = rt->ground_speed_kmh;

    s_vario_ui_dynamic.head = (uint8_t)((idx + 1u) % VARIO_UI_AVG_BUFFER_SIZE);
    if (s_vario_ui_dynamic.count < VARIO_UI_AVG_BUFFER_SIZE)
    {
        ++s_vario_ui_dynamic.count;
    }

    s_vario_ui_dynamic.last_publish_ms = sample_ms;

update_peak_only:
    if ((rt->flight_active != false) || (rt->flight_time_s > 0u) || (rt->trail_count > 0u))
    {
        if (rt->ground_speed_kmh > s_vario_ui_dynamic.top_speed_kmh)
        {
            s_vario_ui_dynamic.top_speed_kmh = rt->ground_speed_kmh;
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
                                 rt->baro_vario_mps,
                                 out_avg_vario_mps);

    vario_display_recent_average(s_vario_ui_dynamic.speed_kmh,
                                 s_vario_ui_dynamic.sample_stamp_ms,
                                 s_vario_ui_dynamic.count,
                                 s_vario_ui_dynamic.head,
                                 s_vario_ui_dynamic.last_publish_ms,
                                 window_ms,
                                 rt->ground_speed_kmh,
                                 out_avg_speed_kmh);
}


static void vario_display_get_bar_display_values(const vario_runtime_t *rt,
                                                 float average_vario_mps,
                                                 float average_speed_kmh,
                                                 float *out_vario_bar_mps,
                                                 float *out_avg_vario_mps,
                                                 float *out_gs_bar_kmh,
                                                 float *out_avg_speed_kmh)
{
    float target_vario_mps;
    float target_gs_kmh;
    uint32_t now_ms;
    float dt_s;
    float innovation;
    float tau_s;
    float alpha;

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

    target_vario_mps = vario_display_clampf(rt->fast_vario_bar_mps, -8.0f, 8.0f);
    target_gs_kmh = vario_display_clampf(rt->gs_bar_speed_kmh, 0.0f, 120.0f);
    now_ms = (rt->last_task_ms != 0u) ? rt->last_task_ms : rt->last_publish_ms;

    if ((s_vario_ui_dynamic.last_bar_update_ms == 0u) || (now_ms == 0u))
    {
        s_vario_ui_dynamic.filtered_vario_bar_mps = target_vario_mps;
        s_vario_ui_dynamic.filtered_gs_bar_kmh = target_gs_kmh;
        s_vario_ui_dynamic.vario_bar_zero_latched =
            (vario_display_absf(target_vario_mps) < 0.08f) ? true : false;
        s_vario_ui_dynamic.last_bar_update_ms = now_ms;
    }
    else if (now_ms != s_vario_ui_dynamic.last_bar_update_ms)
    {
        /* ------------------------------------------------------------------ */
        /* side bar 는 숫자와 별도 경로                                        */
        /* - 10 Hz 주기로만 갱신해서 프레임마다 덜 흔들리게 하고                */
        /* - fast vario path 에 attack/release + zero hysteresis 를 추가한다.   */
        /* ------------------------------------------------------------------ */
        if ((now_ms - s_vario_ui_dynamic.last_bar_update_ms) >= 100u)
        {
            dt_s = ((float)(now_ms - s_vario_ui_dynamic.last_bar_update_ms)) * 0.001f;
            dt_s = vario_display_clampf(dt_s, 0.010f, 0.250f);

            innovation = target_vario_mps - s_vario_ui_dynamic.filtered_vario_bar_mps;
            if (((target_vario_mps >= 0.0f) && (s_vario_ui_dynamic.filtered_vario_bar_mps >= 0.0f) &&
                 (target_vario_mps > s_vario_ui_dynamic.filtered_vario_bar_mps)) ||
                ((target_vario_mps <= 0.0f) && (s_vario_ui_dynamic.filtered_vario_bar_mps <= 0.0f) &&
                 (target_vario_mps < s_vario_ui_dynamic.filtered_vario_bar_mps)))
            {
                tau_s = 0.045f;
            }
            else
            {
                tau_s = 0.110f;
            }

            alpha = dt_s / (tau_s + dt_s);
            s_vario_ui_dynamic.filtered_vario_bar_mps += alpha * innovation;

            if (s_vario_ui_dynamic.vario_bar_zero_latched != false)
            {
                if (vario_display_absf(target_vario_mps) > 0.18f)
                {
                    s_vario_ui_dynamic.vario_bar_zero_latched = false;
                }
                else
                {
                    s_vario_ui_dynamic.filtered_vario_bar_mps = 0.0f;
                }
            }
            else if ((vario_display_absf(target_vario_mps) < 0.06f) &&
                     (vario_display_absf(s_vario_ui_dynamic.filtered_vario_bar_mps) < 0.12f))
            {
                s_vario_ui_dynamic.vario_bar_zero_latched = true;
                s_vario_ui_dynamic.filtered_vario_bar_mps = 0.0f;
            }

            dt_s = ((float)(now_ms - s_vario_ui_dynamic.last_bar_update_ms)) * 0.001f;
            dt_s = vario_display_clampf(dt_s, 0.010f, 0.250f);
            alpha = dt_s / (0.12f + dt_s);
            s_vario_ui_dynamic.filtered_gs_bar_kmh += alpha *
                (target_gs_kmh - s_vario_ui_dynamic.filtered_gs_bar_kmh);

            s_vario_ui_dynamic.filtered_vario_bar_mps =
                vario_display_clampf(s_vario_ui_dynamic.filtered_vario_bar_mps, -8.0f, 8.0f);
            s_vario_ui_dynamic.filtered_gs_bar_kmh =
                vario_display_clampf(s_vario_ui_dynamic.filtered_gs_bar_kmh, 0.0f, 120.0f);
            s_vario_ui_dynamic.last_bar_update_ms = now_ms;
        }
    }

    *out_vario_bar_mps = s_vario_ui_dynamic.filtered_vario_bar_mps;
    *out_avg_vario_mps = average_vario_mps;
    *out_gs_bar_kmh = s_vario_ui_dynamic.filtered_gs_bar_kmh;
    *out_avg_speed_kmh = average_speed_kmh;
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
    int32_t target_lat_e7;
    int32_t target_lon_e7;

    if (out_solution == NULL)
    {
        return;
    }

    memset(out_solution, 0, sizeof(*out_solution));

    if (s_vario_ui_dynamic.nav_mode == VARIO_NAV_TARGET_WP)
    {
        snprintf(out_solution->label, sizeof(out_solution->label), "WPT1");
    }
    else
    {
        snprintf(out_solution->label, sizeof(out_solution->label), "START");
    }

    if (rt == NULL)
    {
        return;
    }

    out_solution->current_valid = ((rt->gps_valid != false) &&
                                   (rt->gps.fix.valid != false) &&
                                   (rt->gps.fix.fixOk != false) &&
                                   (rt->gps.fix.fixType != 0u));
    out_solution->heading_valid = (rt->heading_valid != false);

    if (out_solution->current_valid == false)
    {
        return;
    }

    if (s_vario_ui_dynamic.nav_mode == VARIO_NAV_TARGET_WP)
    {
        if (s_vario_ui_dynamic.wp_valid == false)
        {
            return;
        }
        target_lat_e7 = s_vario_ui_dynamic.wp_lat_e7;
        target_lon_e7 = s_vario_ui_dynamic.wp_lon_e7;
    }
    else
    {
        if (vario_display_get_oldest_trail_point(rt, &target_lat_e7, &target_lon_e7) == false)
        {
            return;
        }
    }

    if (vario_display_distance_bearing(rt->gps.fix.lat,
                                       rt->gps.fix.lon,
                                       target_lat_e7,
                                       target_lon_e7,
                                       &out_solution->distance_m,
                                       &out_solution->bearing_deg) == false)
    {
        return;
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

    out_solution->target_valid = true;
}

static void vario_display_get_vario_slot_rect(const vario_viewport_t *v,
                                              bool positive,
                                              uint8_t level_from_center,
                                              int16_t *out_y,
                                              int16_t *out_h)
{
    uint8_t slot_index;
    int32_t top_y;
    int32_t bottom_y;

    if ((v == NULL) || (out_y == NULL) || (out_h == NULL))
    {
        return;
    }

    if (positive != false)
    {
        slot_index = (uint8_t)(VARIO_UI_VARIO_HALFSTEP_COUNT - 1u - level_from_center);
    }
    else
    {
        slot_index = (uint8_t)(VARIO_UI_VARIO_HALFSTEP_COUNT + level_from_center);
    }

    top_y = v->y + (((int32_t)slot_index * v->h) / ((int32_t)VARIO_UI_VARIO_HALFSTEP_COUNT * 2));
    bottom_y = v->y + ((((int32_t)slot_index + 1) * v->h) / ((int32_t)VARIO_UI_VARIO_HALFSTEP_COUNT * 2)) - 1;

    if (bottom_y < top_y)
    {
        bottom_y = top_y;
    }

    *out_y = (int16_t)top_y;
    *out_h = (int16_t)(bottom_y - top_y + 1);
}

static void vario_display_get_gs_slot_rect(const vario_viewport_t *v,
                                           uint8_t level_from_bottom,
                                           int16_t *out_y,
                                           int16_t *out_h)
{
    uint8_t slot_index;
    int32_t top_y;
    int32_t bottom_y;

    if ((v == NULL) || (out_y == NULL) || (out_h == NULL))
    {
        return;
    }

    slot_index = (uint8_t)(VARIO_UI_GS_STEP_COUNT - 1u - level_from_bottom);
    top_y = v->y + (((int32_t)slot_index * v->h) / (int32_t)VARIO_UI_GS_STEP_COUNT);
    bottom_y = v->y + ((((int32_t)slot_index + 1) * v->h) / (int32_t)VARIO_UI_GS_STEP_COUNT) - 1;

    if (bottom_y < top_y)
    {
        bottom_y = top_y;
    }

    *out_y = (int16_t)top_y;
    *out_h = (int16_t)(bottom_y - top_y + 1);
}

static void vario_display_compute_vario_fill(float vario_mps,
                                             uint8_t *out_first_level,
                                             uint8_t *out_count,
                                             bool *out_positive)
{
    float   abs_value;
    uint8_t overflow_steps;
    uint8_t visible_steps;

    if ((out_first_level == NULL) || (out_count == NULL) || (out_positive == NULL))
    {
        return;
    }

    *out_first_level = 0u;
    *out_count = 0u;
    *out_positive = (vario_mps >= 0.0f) ? true : false;

    abs_value = vario_display_absf(vario_mps);
    if (abs_value <= 0.0f)
    {
        return;
    }

    if (abs_value <= VARIO_UI_VARIO_MAX_ABS_MPS)
    {
        visible_steps = (uint8_t)ceilf(abs_value / VARIO_UI_VARIO_HALFSTEP_MPS);
        if (visible_steps > VARIO_UI_VARIO_HALFSTEP_COUNT)
        {
            visible_steps = VARIO_UI_VARIO_HALFSTEP_COUNT;
        }
        *out_first_level = 0u;
        *out_count = visible_steps;
        return;
    }

    overflow_steps = (uint8_t)ceilf((abs_value - VARIO_UI_VARIO_MAX_ABS_MPS) / VARIO_UI_VARIO_HALFSTEP_MPS);
    if (overflow_steps >= VARIO_UI_VARIO_HALFSTEP_COUNT)
    {
        *out_first_level = VARIO_UI_VARIO_HALFSTEP_COUNT;
        *out_count = 0u;
        return;
    }

    *out_first_level = overflow_steps;
    *out_count = (uint8_t)(VARIO_UI_VARIO_HALFSTEP_COUNT - overflow_steps);
}

static uint8_t vario_display_compute_gs_fill_steps(float speed_kmh)
{
    if (speed_kmh < VARIO_UI_GS_MIN_VISIBLE_KMH)
    {
        return 0u;
    }

    if (speed_kmh >= VARIO_UI_GS_MAX_KMH)
    {
        return VARIO_UI_GS_STEP_COUNT;
    }

    return (uint8_t)(floorf((speed_kmh - VARIO_UI_GS_MIN_VISIBLE_KMH) / VARIO_UI_GS_STEP_KMH) + 1.0f);
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
                                                         const char *value_text)
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
    u8g2_SetFontPosBaseline(u8g2);

    (void)box_w;
}



static void vario_display_draw_top_left_metrics(u8g2_t *u8g2,
                                                const vario_viewport_t *v,
                                                const vario_runtime_t *rt)
{
    char    glide_text[12];
    int16_t x;
    int16_t top_y;
    int16_t suffix_x;

    if ((u8g2 == NULL) || (v == NULL) || (rt == NULL))
    {
        return;
    }

    x = (int16_t)(v->x + VARIO_UI_SIDE_BAR_W + VARIO_UI_TOP_LEFT_PAD_X);
    top_y = v->y;

    /* ---------------------------------------------------------------------- */
    /* 좌상단 활공비                                                           */
    /* - 다른 UI 는 그대로 두고, 활공비 블록만 "상단에 딱 붙는" top align 으로    */
    /*   다시 그린다.                                                          */
    /* - 값/":1" 둘 다 ALT2/ALT3 와 같은 계열 글꼴을 유지한다.                  */
    /* - baseline draw 를 쓰면 글꼴 ascent 만큼 아래로 내려가 보이므로,           */
    /*   여기서는 font position 을 top 으로 바꿔 정확히 화면 상단에 맞춘다.      */
    /* ---------------------------------------------------------------------- */
    vario_display_format_glide_ratio(glide_text, sizeof(glide_text), rt);

    u8g2_SetFontPosTop(u8g2);
    u8g2_SetFont(u8g2, VARIO_UI_FONT_ALT2_VALUE);
    u8g2_DrawStr(u8g2, x, top_y, glide_text);

    suffix_x = (int16_t)(x + u8g2_GetStrWidth(u8g2, glide_text) + 2);
    u8g2_SetFont(u8g2, VARIO_UI_FONT_ALT2_UNIT);
    u8g2_DrawStr(u8g2, suffix_x, top_y, ":1");
    u8g2_SetFontPosBaseline(u8g2);
}


static void vario_display_draw_top_center_clock(u8g2_t *u8g2,
                                                const vario_viewport_t *v,
                                                const vario_runtime_t *rt,
                                                const vario_settings_t *settings)
{
    char clock_text[20];

    if ((u8g2 == NULL) || (v == NULL) || (rt == NULL) || (settings == NULL) || (settings->show_current_time == 0u))
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /* 상단 중앙 현재 시각                                                     */
    /* - 사용자 요구대로 기존보다 한 단계 작은 폰트를 사용한다.                 */
    /* ---------------------------------------------------------------------- */
    vario_display_format_clock(clock_text, sizeof(clock_text), rt, settings);
    u8g2_SetFont(u8g2, VARIO_UI_FONT_TOP_CLOCK);
    Vario_Display_DrawTextCentered(u8g2,
                                   (int16_t)(v->x + (v->w / 2)),
                                   (int16_t)(v->y + VARIO_UI_TOP_CLOCK_BASELINE_Y),
                                   clock_text);
}



static void vario_display_draw_bottom_center_flight_time(u8g2_t *u8g2,
                                                         const vario_viewport_t *v,
                                                         const vario_runtime_t *rt)
{
    char    flight_time[24];
    int16_t text_h;
    int16_t top_y;
    int16_t text_w;
    int16_t draw_x;

    if ((u8g2 == NULL) || (v == NULL) || (rt == NULL))
    {
        return;
    }

    vario_display_format_flight_time(flight_time, sizeof(flight_time), rt);

    text_h = vario_display_get_font_height(u8g2, VARIO_UI_FONT_ALT2_VALUE);
    if (text_h <= 0)
    {
        return;
    }

    top_y = (int16_t)(v->y + v->h - 2 - text_h);
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
    long fl_value;

    if ((value_buf == NULL) || (value_len == 0u) || (unit_buf == NULL) || (unit_len == 0u) ||
        (rt == NULL) || (settings == NULL))
    {
        return;
    }

    switch (settings->alt2_mode)
    {
        case VARIO_ALT2_MODE_ABSOLUTE:
            vario_display_format_altitude_with_unit(value_buf,
                                                    value_len,
                                                    rt->alt1_absolute_m,
                                                    settings->alt2_unit);
            snprintf(unit_buf,
                     unit_len,
                     "%s",
                     Vario_Settings_GetAltitudeUnitTextForUnit(settings->alt2_unit));
            break;

        case VARIO_ALT2_MODE_GPS:
            if (rt->gps_valid != false)
            {
                vario_display_format_altitude_with_unit(value_buf,
                                                        value_len,
                                                        rt->gps_altitude_m,
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
                fl_value = lroundf((rt->pressure_altitude_std_m * 3.2808399f) / 100.0f);
                if (fl_value < 0)
                {
                    fl_value = 0;
                }
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

    vario_display_format_altitude_with_unit(alt1_text, sizeof(alt1_text), rt->alt1_absolute_m, settings->altitude_unit);
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
    if (alt23_row_h < VARIO_ICON_ALT2_HEIGHT)
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
    uint8_t                 level;
    uint8_t                 skip_slots;
    uint8_t                 active_slots;
    uint8_t                 instant_steps;
    uint8_t                 avg_steps;
    int16_t                 slot_y;
    int16_t                 slot_h;
    int16_t                 left_bar_x;
    int16_t                 instant_x;
    int16_t                 avg_x;
    int16_t                 tick_x;
    int16_t                 center_y;
    int16_t                 zero_top_y;
    uint8_t                 thick_i;
    uint8_t                 top_scale_mps;
    float                   top_scale;
    float                   instant_abs;
    float                   avg_abs;
    bool                    instant_positive;
    bool                    avg_positive;

    if ((u8g2 == NULL) || (v == NULL))
    {
        return;
    }

    settings = Vario_Settings_Get();
    top_scale_mps = 4u;
    if (settings != NULL)
    {
        top_scale_mps = (uint8_t)(settings->vario_range_mps_x10 / 10u);
        if (top_scale_mps < 4u)
        {
            top_scale_mps = 4u;
        }
        else if (top_scale_mps > 8u)
        {
            top_scale_mps = 8u;
        }
    }
    skip_slots = (top_scale_mps > 4u) ? (uint8_t)(top_scale_mps - 4u) : 0u;
    if (skip_slots > 4u)
    {
        skip_slots = 4u;
    }
    active_slots = (uint8_t)(VARIO_UI_VARIO_HALFSTEP_COUNT - skip_slots);
    if (active_slots == 0u)
    {
        active_slots = 1u;
    }
    top_scale = (float)top_scale_mps;

    left_bar_x = v->x;
    instant_x = (int16_t)(left_bar_x + 1);
    avg_x = (int16_t)(left_bar_x + 10);

    for (level = 0u; level < VARIO_UI_VARIO_HALFSTEP_COUNT; ++level)
    {
        uint8_t tick_w;

        tick_w = ((level % 2u) != 0u) ? VARIO_UI_SCALE_MAJOR_W : VARIO_UI_SCALE_MINOR_W;
        tick_x = left_bar_x;

        vario_display_get_vario_slot_rect(v, true, level, &slot_y, &slot_h);
        u8g2_DrawHLine(u8g2, tick_x, slot_y, tick_w);

        vario_display_get_vario_slot_rect(v, false, level, &slot_y, &slot_h);
        u8g2_DrawHLine(u8g2, tick_x, slot_y, tick_w);
    }

    instant_positive = (instant_vario_mps >= 0.0f) ? true : false;
    avg_positive = (average_vario_mps >= 0.0f) ? true : false;
    instant_abs = vario_display_absf(vario_display_clampf(instant_vario_mps, -top_scale, top_scale));
    avg_abs = vario_display_absf(vario_display_clampf(average_vario_mps, -top_scale, top_scale));

    instant_steps = (uint8_t)ceilf((instant_abs / top_scale) * (float)active_slots);
    avg_steps = (uint8_t)ceilf((avg_abs / top_scale) * (float)active_slots);

    if ((instant_abs > 0.0f) && (instant_steps == 0u))
    {
        instant_steps = 1u;
    }
    if ((avg_abs > 0.0f) && (avg_steps == 0u))
    {
        avg_steps = 1u;
    }
    if (instant_steps > active_slots)
    {
        instant_steps = active_slots;
    }
    if (avg_steps > active_slots)
    {
        avg_steps = active_slots;
    }

    for (level = 0u; level < instant_steps; ++level)
    {
        uint8_t slot_level;

        slot_level = (uint8_t)(skip_slots + level);
        vario_display_get_vario_slot_rect(v, instant_positive, slot_level, &slot_y, &slot_h);
        u8g2_DrawBox(u8g2, instant_x, slot_y, VARIO_UI_GAUGE_INSTANT_W, slot_h);
    }

    for (level = 0u; level < avg_steps; ++level)
    {
        uint8_t slot_level;

        slot_level = (uint8_t)(skip_slots + level);
        vario_display_get_vario_slot_rect(v, avg_positive, slot_level, &slot_y, &slot_h);
        u8g2_DrawBox(u8g2, avg_x, slot_y, VARIO_UI_GAUGE_AVG_W, slot_h);
    }

    center_y = (int16_t)(v->y + (v->h / 2));
    zero_top_y = (int16_t)(center_y - (VARIO_UI_VARIO_ZERO_LINE_THICKNESS / 2));

    for (thick_i = 0u; thick_i < VARIO_UI_VARIO_ZERO_LINE_THICKNESS; ++thick_i)
    {
        int16_t zero_y;
        zero_y = (int16_t)(zero_top_y + (int16_t)thick_i);
        u8g2_DrawHLine(u8g2, left_bar_x, zero_y, VARIO_UI_VARIO_ZERO_LINE_W);
    }
}

static void vario_display_draw_gs_side_bar(u8g2_t *u8g2,
                                           const vario_viewport_t *v,
                                           float instant_speed_kmh,
                                           float average_speed_kmh)
{
    const vario_settings_t *settings;
    uint8_t                 level;
    int16_t                 slot_y;
    int16_t                 slot_h;
    int16_t                 right_bar_x;
    int16_t                 instant_x;
    int16_t                 avg_x;
    int16_t                 arrow_y;
    int16_t                 fill_h;
    float                   clamped_speed;
    float                   clamped_avg_speed;
    float                   ratio;
    float                   gs_top_kmh;
    uint8_t                 tick_w;
    int16_t                 tick_x;

    if ((u8g2 == NULL) || (v == NULL))
    {
        return;
    }

    settings = Vario_Settings_Get();
    gs_top_kmh = 80.0f;
    if (settings != NULL)
    {
        gs_top_kmh = (float)settings->gs_range_kmh;
        if (gs_top_kmh < 30.0f)
        {
            gs_top_kmh = 30.0f;
        }
        else if (gs_top_kmh > 150.0f)
        {
            gs_top_kmh = 150.0f;
        }
    }

    right_bar_x = (int16_t)(v->x + v->w - VARIO_UI_SIDE_BAR_W);
    instant_x = (int16_t)(right_bar_x + 5);
    avg_x = (int16_t)(right_bar_x + 9);

    for (level = 0u; level < VARIO_UI_GS_STEP_COUNT; ++level)
    {
        tick_w = ((level % 2u) == 0u) ? VARIO_UI_SCALE_MAJOR_W : VARIO_UI_SCALE_MINOR_W;
        tick_x = (int16_t)(right_bar_x + VARIO_UI_SIDE_BAR_W - tick_w);
        vario_display_get_gs_slot_rect(v, level, &slot_y, &slot_h);
        u8g2_DrawHLine(u8g2, tick_x, slot_y, tick_w);
    }

    clamped_speed = vario_display_clampf(instant_speed_kmh, 0.0f, gs_top_kmh);
    if (clamped_speed > 0.0f)
    {
        ratio = clamped_speed / gs_top_kmh;
        fill_h = (int16_t)lroundf(ratio * (float)v->h);
        if ((ratio > 0.0f) && (fill_h <= 0))
        {
            fill_h = 1;
        }
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

    clamped_avg_speed = vario_display_clampf(average_speed_kmh, 0.0f, gs_top_kmh);
    if (clamped_avg_speed > 0.0f)
    {
        ratio = clamped_avg_speed / gs_top_kmh;
        arrow_y = (int16_t)(v->y + v->h - 1 -
                            lroundf(ratio * (float)(v->h - VARIO_ICON_BAR_MARK_RIGHT_HEIGHT)) -
                            (VARIO_ICON_BAR_MARK_RIGHT_HEIGHT / 2));
        if (arrow_y < v->y)
        {
            arrow_y = v->y;
        }
        if ((arrow_y + VARIO_ICON_BAR_MARK_RIGHT_HEIGHT) > (v->y + v->h))
        {
            arrow_y = (int16_t)(v->y + v->h - VARIO_ICON_BAR_MARK_RIGHT_HEIGHT);
        }

        vario_display_draw_xbm(u8g2,
                               avg_x,
                               arrow_y,
                               VARIO_ICON_BAR_MARK_RIGHT_WIDTH,
                               VARIO_ICON_BAR_MARK_RIGHT_HEIGHT,
                               vario_icon_bar_mark_right_bits);
    }
}

static void vario_display_draw_vario_value_block(u8g2_t *u8g2,
                                                 const vario_viewport_t *v,
                                                 const vario_runtime_t *rt)
{
    const vario_settings_t *settings;
    char    value_text[20];
    char    max_text[20];
    int16_t value_box_x;
    int16_t value_box_y;
    int16_t value_box_h;
    int16_t max_box_x;
    int16_t max_box_y;
    int16_t max_box_h;
    int16_t arrow_x;
    int16_t arrow_y;

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
    value_box_y = (int16_t)(v->y + ((v->h - value_box_h) / 2));

    vario_display_format_vario_value_abs(value_text, sizeof(value_text), rt->baro_vario_mps);

    /* ---------------------------------------------------------------------- */
    /* 현재 큰 VARIO 값은 leading zero 전체를 지우지 않는다.                    */
    /* - 사용자가 원하는 건 ".5" 가 아니라 "[blank]0 5" 구조다.               */
    /* - 즉, tens slot 만 빈칸으로 두고 decimal 직전 ones zero 는 살린다.       */
    /* - 그래서 여기서는 trim helper 를 호출하지 않고, draw 단계에서            */
    /*   fixed-slot renderer 가 tens slot 만 숨기게 한다.                      */
    /* ---------------------------------------------------------------------- */
    vario_display_format_peak_vario(max_text, sizeof(max_text), rt->max_top_vario_mps);

    if ((settings == NULL) || (settings->show_max_vario != 0u))
    {
        max_box_h = vario_display_get_font_height(u8g2, VARIO_UI_FONT_BOTTOM_MAX_VALUE);
        if (max_box_h <= 0)
        {
            max_box_h = 8;
        }
        max_box_x = (int16_t)(v->x + VARIO_UI_SIDE_BAR_W + 2);
        max_box_y = (int16_t)(value_box_y - max_box_h - VARIO_UI_BOTTOM_META_GAP_Y);
        vario_display_draw_text_box_top(u8g2,
                                        max_box_x,
                                        max_box_y,
                                        VARIO_UI_BOTTOM_META_BOX_W,
                                        VARIO_UI_ALIGN_LEFT,
                                        VARIO_UI_FONT_BOTTOM_MAX_VALUE,
                                        max_text);

        arrow_x = (int16_t)(max_box_x + VARIO_UI_BOTTOM_META_BOX_W + 3);
        arrow_y = (int16_t)(max_box_y + ((max_box_h - VARIO_ICON_TREND_UP_HEIGHT) / 2));
        if (rt->baro_vario_mps > 0.05f)
        {
            vario_display_draw_xbm(u8g2,
                                   arrow_x,
                                   arrow_y,
                                   VARIO_ICON_TREND_UP_WIDTH,
                                   VARIO_ICON_TREND_UP_HEIGHT,
                                   vario_icon_trend_up_bits);
        }
        else if (rt->baro_vario_mps < -0.05f)
        {
            vario_display_draw_xbm(u8g2,
                                   arrow_x,
                                   arrow_y,
                                   VARIO_ICON_TREND_DOWN_WIDTH,
                                   VARIO_ICON_TREND_DOWN_HEIGHT,
                                   vario_icon_trend_down_bits);
        }
    }

    vario_display_draw_fixed_vario_current_value(u8g2,
                                                 value_box_x,
                                                 value_box_y,
                                                 VARIO_UI_BOTTOM_BOX_W,
                                                 value_box_h,
                                                 value_text);
}


static void vario_display_draw_speed_value_block(u8g2_t *u8g2,
                                                 const vario_viewport_t *v,
                                                 const vario_runtime_t *rt)
{
    char    value_text[20];
    char    max_text[20];
    int16_t value_box_x;
    int16_t value_box_y;
    int16_t value_box_h;
    int16_t max_box_x;
    int16_t max_box_y;
    int16_t max_box_h;

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
    value_box_y = (int16_t)(v->y + v->h - VARIO_UI_BOTTOM_BOX_BOTTOM_PAD - value_box_h);

    vario_display_format_speed_value(value_text, sizeof(value_text), rt->ground_speed_kmh);
    vario_display_format_peak_speed(max_text, sizeof(max_text), s_vario_ui_dynamic.top_speed_kmh);

    max_box_h = vario_display_get_font_height(u8g2, VARIO_UI_FONT_BOTTOM_MAX_VALUE);
    if (max_box_h <= 0)
    {
        max_box_h = 8;
    }
    max_box_x = (int16_t)(v->x + v->w - VARIO_UI_SIDE_BAR_W - 2 - VARIO_UI_BOTTOM_META_BOX_W);
    max_box_y = (int16_t)(value_box_y - max_box_h - VARIO_UI_BOTTOM_META_GAP_Y);
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

static void vario_display_draw_trail_background(u8g2_t *u8g2,
                                                const vario_viewport_t *v,
                                                const vario_runtime_t *rt,
                                                const vario_settings_t *settings)
{
    uint8_t start_index;
    uint8_t i;
    int16_t center_x;
    int16_t center_y;
    float   half_w_px;
    float   half_h_px;
    float   meters_per_px;

    if ((u8g2 == NULL) || (v == NULL) || (rt == NULL) || (settings == NULL))
    {
        return;
    }

    if ((rt->gps_valid == false) || (rt->trail_count == 0u))
    {
        return;
    }

    center_x = (int16_t)(v->x + (v->w / 2));
    center_y = (int16_t)(v->y + (v->h / 2));
    half_w_px = (float)(v->w / 2);
    half_h_px = (float)(v->h / 2);
    meters_per_px = ((float)settings->trail_range_m) / ((half_w_px < half_h_px) ? half_w_px : half_h_px);
    if (meters_per_px <= 0.0f)
    {
        return;
    }

    start_index = (uint8_t)((rt->trail_head + VARIO_TRAIL_MAX_POINTS - rt->trail_count) % VARIO_TRAIL_MAX_POINTS);
    for (i = 0u; i < rt->trail_count; ++i)
    {
        uint8_t idx;
        float   ref_lat_deg;
        float   cur_lat_deg;
        float   cur_lon_deg;
        float   pt_lat_deg;
        float   pt_lon_deg;
        float   mean_lat_rad;
        float   dx_m;
        float   dy_m;
        int16_t px;
        int16_t py;

        idx = (uint8_t)((start_index + i) % VARIO_TRAIL_MAX_POINTS);

        ref_lat_deg = ((float)rt->gps.fix.lat) * 1.0e-7f;
        cur_lat_deg = ref_lat_deg;
        cur_lon_deg = ((float)rt->gps.fix.lon) * 1.0e-7f;
        pt_lat_deg  = ((float)rt->trail_lat_e7[idx]) * 1.0e-7f;
        pt_lon_deg  = ((float)rt->trail_lon_e7[idx]) * 1.0e-7f;
        mean_lat_rad = vario_display_deg_to_rad((cur_lat_deg + pt_lat_deg) * 0.5f);

        dx_m = (pt_lon_deg - cur_lon_deg) * (111319.5f * cosf(mean_lat_rad));
        dy_m = (pt_lat_deg - ref_lat_deg) * 111132.0f;

        px = (int16_t)lroundf((float)center_x + (dx_m / meters_per_px));
        py = (int16_t)lroundf((float)center_y - (dy_m / meters_per_px));

        if ((px < v->x) || (px >= (v->x + v->w)) || (py < v->y) || (py >= (v->y + v->h)))
        {
            continue;
        }

        u8g2_DrawPixel(u8g2, px, py);
    }

    /* 현재 위치 center marker */
    u8g2_DrawHLine(u8g2, (int16_t)(center_x - 3), center_y, 7u);
    u8g2_DrawVLine(u8g2, center_x, (int16_t)(center_y - 3), 7u);
    u8g2_DrawCircle(u8g2, center_x, center_y, 3u, U8G2_DRAW_ALL);
}

static void vario_display_draw_compass(u8g2_t *u8g2,
                                       const vario_viewport_t *v,
                                       const vario_runtime_t *rt,
                                       const vario_display_nav_solution_t *nav)
{
    char    nav_text[48];
    int16_t content_left_x;
    int16_t content_right_x;
    int16_t center_x;
    int16_t label_baseline_y;
    int16_t top_limit_y;
    int16_t bottom_limit_y;
    int16_t usable_half_w;
    int16_t radius;
    int16_t center_y;
    int16_t i;
    float   heading_deg;

    if ((u8g2 == NULL) || (v == NULL) || (rt == NULL) || (nav == NULL))
    {
        return;
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

    /* ---------------------------------------------------------------------- */
    /* 사용자가 요청한 위치 규칙                                               */
    /* - X는 화면 정중앙 고정                                                  */
    /* - Y는 원의 가장 밑부분이 자기 영역 최하단에 딱 닿도록 아래로 내린다.      */
    /* ---------------------------------------------------------------------- */
    center_y = (int16_t)(bottom_limit_y - radius);
    if ((center_y - radius) < top_limit_y)
    {
        center_y = (int16_t)(top_limit_y + radius);
    }

    vario_display_format_nav_distance(nav_text,
                                      sizeof(nav_text),
                                      nav->label,
                                      nav->current_valid && nav->target_valid,
                                      nav->distance_m);

    u8g2_SetFont(u8g2, VARIO_UI_FONT_NAV_LINE);
    Vario_Display_DrawTextCentered(u8g2,
                                   center_x,
                                   label_baseline_y,
                                   nav_text);

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

    if ((nav->current_valid != false) && (nav->target_valid != false))
    {
        float   rad;
        int16_t tip_x;
        int16_t tip_y;
        int16_t tail_x;
        int16_t tail_y;
        int16_t left_x;
        int16_t left_y;
        int16_t right_x;
        int16_t right_y;
        int16_t arrow_len;

        rad = vario_display_deg_to_rad(nav->relative_bearing_deg);
        arrow_len = (int16_t)(radius - 10);
        if (arrow_len < 10)
        {
            arrow_len = 10;
        }

        tip_x = (int16_t)lroundf((float)center_x + (sinf(rad) * (float)arrow_len));
        tip_y = (int16_t)lroundf((float)center_y - (cosf(rad) * (float)arrow_len));
        tail_x = (int16_t)lroundf((float)center_x - (sinf(rad) * 5.0f));
        tail_y = (int16_t)lroundf((float)center_y + (cosf(rad) * 5.0f));
        left_x = (int16_t)lroundf((float)tip_x - (sinf(rad - 0.5f) * 6.0f));
        left_y = (int16_t)lroundf((float)tip_y + (cosf(rad - 0.5f) * 6.0f));
        right_x = (int16_t)lroundf((float)tip_x - (sinf(rad + 0.5f) * 6.0f));
        right_y = (int16_t)lroundf((float)tip_y + (cosf(rad + 0.5f) * 6.0f));

        u8g2_DrawLine(u8g2, tail_x, tail_y, tip_x, tip_y);
        u8g2_DrawLine(u8g2, left_x, left_y, tip_x, tip_y);
        u8g2_DrawLine(u8g2, right_x, right_y, tip_x, tip_y);
    }
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
            cfg.show_stub_overlay = true;
            cfg.stub_title = "PAGE 3";
            cfg.stub_subtitle = "UI STUB";
            break;
    }

    u8g2_SetFontMode(u8g2, 1);

    vario_display_update_dynamic_metrics(rt, settings);
    vario_display_get_average_values(rt, settings, &avg_vario_mps, &avg_speed_kmh);
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
        trail_v.x = (int16_t)(v->x + VARIO_UI_SIDE_BAR_W);
        trail_v.y = v->y;
        trail_v.w = (int16_t)(v->w - (2 * VARIO_UI_SIDE_BAR_W));
        trail_v.h = v->h;
        vario_display_draw_trail_background(u8g2, &trail_v, rt, settings);
    }

    vario_display_draw_vario_side_bar(u8g2, v, bar_vario_mps, bar_avg_vario_mps);
    vario_display_draw_gs_side_bar(u8g2, v, bar_speed_kmh, bar_avg_speed_kmh);

    vario_display_draw_top_left_metrics(u8g2, v, rt);
    vario_display_draw_top_center_clock(u8g2, v, rt, settings);
    vario_display_draw_top_right_altitudes(u8g2, v, rt);

    if (cfg.show_compass != false)
    {
        vario_display_draw_compass(u8g2, v, rt, &nav);
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

    nav_mode = s_vario_ui_dynamic.nav_mode;
    wp_lat_e7 = s_vario_ui_dynamic.wp_lat_e7;
    wp_lon_e7 = s_vario_ui_dynamic.wp_lon_e7;
    wp_valid = s_vario_ui_dynamic.wp_valid;

    memset(&s_vario_ui_dynamic, 0, sizeof(s_vario_ui_dynamic));
    s_vario_ui_dynamic.nav_mode = nav_mode;
    s_vario_ui_dynamic.wp_lat_e7 = wp_lat_e7;
    s_vario_ui_dynamic.wp_lon_e7 = wp_lon_e7;
    s_vario_ui_dynamic.wp_valid = wp_valid;
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
