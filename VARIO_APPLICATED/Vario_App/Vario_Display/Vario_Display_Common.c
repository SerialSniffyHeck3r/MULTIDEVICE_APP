#include "Vario_Display_Common.h"

#include "Vario_Dev.h"
#include "Vario_State.h"
#include "../Vario_Settings/Vario_Settings.h"

#include <math.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>

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
/* 사용자가 추후 위치/폰트를 쉽게 옮길 수 있도록 주요 UI 블록을 전부 상수화했다. */
/* 아래 숫자만 바꾸면 요소가 바로 이동하도록 의도된 좌표계다.                  */
/* -------------------------------------------------------------------------- */
#define VARIO_UI_SIDE_BAR_W              14
#define VARIO_UI_GAUGE_INSTANT_W          8
#define VARIO_UI_GAUGE_AVG_W              5
#define VARIO_UI_GAUGE_GAP_W              1

#define VARIO_UI_TOP_LEFT_PAD_X           4
#define VARIO_UI_TOP_RIGHT_PAD_X          0
#define VARIO_UI_TOP_FLT_BASELINE_Y      11
#define VARIO_UI_TOP_GLD_BASELINE_Y      22
#define VARIO_UI_TOP_CLOCK_BASELINE_Y    15
#define VARIO_UI_TOP_ALT1_TOP_Y           0
#define VARIO_UI_TOP_ALT_ROW_TOP_Y       23

#define VARIO_UI_NAV_LABEL_BASELINE_Y    32
#define VARIO_UI_NAV_LABEL_TO_COMPASS_GAP 3
#define VARIO_UI_COMPASS_MAX_RADIUS      32
#define VARIO_UI_COMPASS_SIDE_MARGIN      8
#define VARIO_UI_COMPASS_LABEL_INSET      6
#define VARIO_UI_COMPASS_BOTTOM_GAP       4

#define VARIO_UI_BOTTOM_BOX_W            92
#define VARIO_UI_BOTTOM_BOX_H            24
#define VARIO_UI_BOTTOM_BOX_BOTTOM_PAD    1
#define VARIO_UI_BOTTOM_LABEL_BASELINE_Y 99
#define VARIO_UI_BOTTOM_VARIO_X_PAD       4
#define VARIO_UI_BOTTOM_GS_X_PAD          4
#define VARIO_UI_BOTTOM_ICON_GAP_Y        3

#define VARIO_UI_STUB_TITLE_BASELINE_DY  -2
#define VARIO_UI_STUB_SUB_BASELINE_DY    14

/* -------------------------------------------------------------------------- */
/* Bar scale definition                                                        */
/* -------------------------------------------------------------------------- */
#define VARIO_UI_VARIO_HALFSTEP_COUNT     8u
#define VARIO_UI_VARIO_HALFSTEP_MPS       0.5f
#define VARIO_UI_VARIO_MAX_ABS_MPS        (VARIO_UI_VARIO_HALFSTEP_COUNT * VARIO_UI_VARIO_HALFSTEP_MPS)

#define VARIO_UI_GS_STEP_COUNT           10u
#define VARIO_UI_GS_STEP_KMH              5.0f
#define VARIO_UI_GS_MIN_VISIBLE_KMH      10.0f
#define VARIO_UI_GS_MAX_KMH              60.0f

#define VARIO_UI_SCALE_MAJOR_W           14u
#define VARIO_UI_SCALE_MINOR_W            8u

/* -------------------------------------------------------------------------- */
/* Flight average / peak cache                                                 */
/*                                                                            */
/* APP_STATE snapshot 구조는 그대로 유지하고,                                  */
/* display 계층이 publish 값(5 Hz)을 다시 누적해서 평균/Top Speed 를 만든다.    */
/* -------------------------------------------------------------------------- */
#define VARIO_UI_AVG_BUFFER_SIZE         96u

/* -------------------------------------------------------------------------- */
/* 수동 WP 기본값                                                              */
/*                                                                            */
/* 현재 repo 에 waypoint 저장소가 따로 없어서,                                 */
/* WP mode 는 display 계층의 수동 좌표를 사용한다.                             */
/* 나중에 실제 waypoint 저장소가 생기면 setter 구현만 교체하면 된다.           */
/* -------------------------------------------------------------------------- */
#define VARIO_UI_DEFAULT_WP_VALID        0u
#define VARIO_UI_DEFAULT_WP_LAT_E7       0
#define VARIO_UI_DEFAULT_WP_LON_E7       0

/* -------------------------------------------------------------------------- */
/* 아이콘 bitmap                                                               */
/*                                                                            */
/* - ALT1 icon : 사용자 제공 XBM                                              */
/* - VARIO up/down icon : 부호 대신 방향성을 전달                              */
/* - GS avg arrow : 우측 평균 속도 위치 indicator                              */
/* -------------------------------------------------------------------------- */
#define VARIO_UI_ICON_ALT1_W 9
#define VARIO_UI_ICON_ALT1_H 9
static const unsigned char s_vario_ui_icon_alt1_bits[] = {
    0x10,0xfe,0x38,0xfe,0x6c,0xfe,0xc6,0xfe,0x00,0xfe,0x00,0xfe,
    0xaa,0xfe,0xff,0xff,0xff,0xff
};

#define VARIO_UI_ICON_VARIO_UP_W 9
#define VARIO_UI_ICON_VARIO_UP_H 5
static const unsigned char s_vario_ui_icon_vario_up_bits[] = {
    0x10,0xfe,0x38,0xfe,0x7c,0xfe,0xfe,0xfe,0xff,0xff
};

#define VARIO_UI_ICON_VARIO_DOWN_W 9
#define VARIO_UI_ICON_VARIO_DOWN_H 5
static const unsigned char s_vario_ui_icon_vario_down_bits[] = {
    0xff,0xff,0xfe,0xfe,0x7c,0xfe,0x38,0xfe,0x10,0xfe
};

#define VARIO_UI_ICON_GS_AVG_W 5
#define VARIO_UI_ICON_GS_AVG_H 5
static const unsigned char s_vario_ui_icon_gs_avg_bits[] = {
    0xe7,0xee,0xfc,0xee,0xe7
};

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
    float    top_speed_kmh;
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
    char text[2];

    if (u8g2 == NULL)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /* ALT2 / ALT3 inline badge                                                */
    /* - 사용자 제공 XBM 은 ALT1 만 있었으므로,                                 */
    /*   ALT2/ALT3 는 작은 badge 형태로 그려 교체 포인트를 명확히 남긴다.       */
    /* - badge 전체 좌표는 이 함수의 x/y_top 만 바꾸면 이동된다.                */
    /* ---------------------------------------------------------------------- */
    text[0] = digit_ch;
    text[1] = '\0';

    u8g2_DrawFrame(u8g2, x, y_top, 9u, 9u);
    u8g2_SetFontPosTop(u8g2);
    u8g2_SetFont(u8g2, u8g2_font_4x6_tf);
    u8g2_DrawStr(u8g2, x + 2, y_top + 1, text);
    u8g2_SetFontPosBaseline(u8g2);
}

static void vario_display_format_clock(char *buf, size_t buf_len, const vario_runtime_t *rt)
{
    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    if ((rt != NULL) && (rt->clock_valid != false))
    {
        snprintf(buf,
                 buf_len,
                 "%02u:%02u:%02u",
                 (unsigned)rt->local_hour,
                 (unsigned)rt->local_min,
                 (unsigned)rt->local_sec);
    }
    else
    {
        snprintf(buf, buf_len, "--:--:--");
    }
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
    float distance_km;
    float distance_mi;

    if ((buf == NULL) || (buf_len == 0u) || (label == NULL))
    {
        return;
    }

    if (valid == false)
    {
        snprintf(buf, buf_len, "to %s ---.-km / ---.-mi", label);
        return;
    }

    distance_km = distance_m * 0.001f;
    distance_km = vario_display_clampf(distance_km, 0.0f, 999.9f);
    distance_mi = distance_km * 0.621371f;

    snprintf(buf,
             buf_len,
             "to %s %.1fkm / %.1fmi",
             label,
             (double)distance_km,
             (double)distance_mi);
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
        snprintf(out_solution->label, sizeof(out_solution->label), "WP");
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
    char    whole[16];
    char    frac[16];
    const char *dot_pos;
    int16_t whole_w;
    int16_t frac_w;
    int16_t total_w;
    int16_t draw_x;
    int16_t bottom_y;

    if ((u8g2 == NULL) || (main_font == NULL) || (frac_font == NULL) || (value_text == NULL))
    {
        return;
    }

    memset(whole, 0, sizeof(whole));
    memset(frac, 0, sizeof(frac));

    dot_pos = strchr(value_text, '.');
    if (dot_pos == NULL)
    {
        snprintf(whole, sizeof(whole), "%s", value_text);
    }
    else
    {
        size_t whole_len;

        whole_len = (size_t)(dot_pos - value_text);
        if (whole_len >= sizeof(whole))
        {
            whole_len = sizeof(whole) - 1u;
        }
        memcpy(whole, value_text, whole_len);
        whole[whole_len] = '\0';
        snprintf(frac, sizeof(frac), "%s", dot_pos);
    }

    whole_w = vario_display_measure_text(u8g2, main_font, whole);
    frac_w = (frac[0] != '\0') ? vario_display_measure_text(u8g2, frac_font, frac) : 0;
    total_w = (int16_t)(whole_w + frac_w);

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

    bottom_y = (int16_t)(box_y + box_h - 1);

    u8g2_SetFontPosBottom(u8g2);
    u8g2_SetFont(u8g2, main_font);
    u8g2_DrawStr(u8g2, draw_x, bottom_y, whole);

    if (frac[0] != '\0')
    {
        u8g2_SetFont(u8g2, frac_font);
        u8g2_DrawStr(u8g2, (int16_t)(draw_x + whole_w), bottom_y, frac);
    }

    u8g2_SetFontPosBaseline(u8g2);
}

static void vario_display_draw_top_left_metrics(u8g2_t *u8g2,
                                                const vario_viewport_t *v,
                                                const vario_runtime_t *rt)
{
    char flight_time[24];
    char glide_text[12];
    int16_t x;
    int16_t gld_value_x;
    int16_t suffix_x;

    if ((u8g2 == NULL) || (v == NULL) || (rt == NULL))
    {
        return;
    }

    x = (int16_t)(v->x + VARIO_UI_SIDE_BAR_W + VARIO_UI_TOP_LEFT_PAD_X);

    /* ---------------------------------------------------------------------- */
    /* 좌상단 FLT TIME value only                                              */
    /* - 폰트      : 6x12 medium                                                */
    /* - 기준점    : full viewport 기준 left + 14px bar 이후 4px                */
    /* - 수정 포인트: VARIO_UI_TOP_FLT_BASELINE_Y / VARIO_UI_TOP_LEFT_PAD_X     */
    /* ---------------------------------------------------------------------- */
    vario_display_format_flight_time(flight_time, sizeof(flight_time), rt);
    u8g2_SetFont(u8g2, u8g2_font_6x12_mf);
    u8g2_DrawStr(u8g2,
                 x,
                 (int16_t)(v->y + VARIO_UI_TOP_FLT_BASELINE_Y),
                 flight_time);

    /* ---------------------------------------------------------------------- */
    /* 좌상단 GLD row                                                          */
    /* - GLD label  : 4x6 small                                                 */
    /* - value      : 6x12 medium                                               */
    /* - suffix :1  : 4x6 small                                                 */
    /* - 수정 포인트: VARIO_UI_TOP_GLD_BASELINE_Y                               */
    /* ---------------------------------------------------------------------- */
    vario_display_format_glide_ratio(glide_text, sizeof(glide_text), rt);
    u8g2_SetFont(u8g2, u8g2_font_4x6_tf);
    u8g2_DrawStr(u8g2,
                 x,
                 (int16_t)(v->y + VARIO_UI_TOP_GLD_BASELINE_Y),
                 "GLD");

    gld_value_x = (int16_t)(x + 16);
    u8g2_SetFont(u8g2, u8g2_font_6x12_mf);
    u8g2_DrawStr(u8g2,
                 gld_value_x,
                 (int16_t)(v->y + VARIO_UI_TOP_GLD_BASELINE_Y),
                 glide_text);

    suffix_x = (int16_t)(gld_value_x + u8g2_GetStrWidth(u8g2, glide_text) + 2);
    u8g2_SetFont(u8g2, u8g2_font_4x6_tf);
    u8g2_DrawStr(u8g2,
                 suffix_x,
                 (int16_t)(v->y + VARIO_UI_TOP_GLD_BASELINE_Y),
                 ":1");
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
    /* - 폰트      : 9x15 medium-large                                          */
    /* - 기준점    : 화면 정중앙 x / y + 15 baseline                            */
    /* - FLT TIME 과 겹치지 않도록 top-right ALT1 폭과 top-left block 사이      */
    /*   중앙에만 배치한다.                                                     */
    /* ---------------------------------------------------------------------- */
    vario_display_format_clock(clock_text, sizeof(clock_text), rt);
    u8g2_SetFont(u8g2, u8g2_font_9x15_mf);
    Vario_Display_DrawTextCentered(u8g2,
                                   (int16_t)(v->x + (v->w / 2)),
                                   (int16_t)(v->y + VARIO_UI_TOP_CLOCK_BASELINE_Y),
                                   clock_text);
}

static void vario_display_draw_top_right_altitudes(u8g2_t *u8g2,
                                                   const vario_viewport_t *v,
                                                   const vario_runtime_t *rt)
{
    char    alt1_text[24];
    char    alt2_text[24];
    char    alt3_text[24];
    int16_t right_limit_x;
    int16_t alt1_w;
    int16_t alt1_x;
    int16_t alt1_icon_x;
    int16_t row_y;
    int16_t a3_icon_x;
    int16_t a3_value_x;
    int16_t a2_icon_x;
    int16_t a2_value_x;
    int16_t a3_value_w;
    int16_t a2_value_w;

    if ((u8g2 == NULL) || (v == NULL) || (rt == NULL))
    {
        return;
    }

    right_limit_x = (int16_t)(v->x + v->w - VARIO_UI_SIDE_BAR_W - VARIO_UI_TOP_RIGHT_PAD_X);

    vario_display_format_altitude(alt1_text, sizeof(alt1_text), rt->alt1_absolute_m);
    vario_display_format_signed_altitude(alt2_text, sizeof(alt2_text), rt->alt2_relative_m);
    vario_display_format_signed_altitude(alt3_text, sizeof(alt3_text), rt->alt3_accum_gain_m);

    /* ---------------------------------------------------------------------- */
    /* ALT1 big block                                                          */
    /* - 폰트      : logisoso20                                                */
    /* - 정렬      : top aligned + right aligned                               */
    /* - 수정 포인트: VARIO_UI_TOP_ALT1_TOP_Y / VARIO_UI_TOP_RIGHT_PAD_X       */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFontPosTop(u8g2);
    u8g2_SetFont(u8g2, u8g2_font_logisoso20_tf);
    alt1_w = (int16_t)u8g2_GetStrWidth(u8g2, alt1_text);
    alt1_x = (int16_t)(right_limit_x - alt1_w);
    alt1_icon_x = (int16_t)(alt1_x - VARIO_UI_ICON_ALT1_W - 3);

    vario_display_draw_xbm(u8g2,
                           alt1_icon_x,
                           (int16_t)(v->y + VARIO_UI_TOP_ALT1_TOP_Y),
                           VARIO_UI_ICON_ALT1_W,
                           VARIO_UI_ICON_ALT1_H,
                           s_vario_ui_icon_alt1_bits);
    u8g2_DrawStr(u8g2,
                 alt1_x,
                 (int16_t)(v->y + VARIO_UI_TOP_ALT1_TOP_Y),
                 alt1_text);
    u8g2_SetFontPosBaseline(u8g2);

    /* ---------------------------------------------------------------------- */
    /* ALT2 / ALT3 inline row                                                  */
    /* - 한 줄 배치                                                            */
    /* - ALT2 / ALT3 숫자는 나중에 badge/icon만 바꾸면 즉시 교체 가능           */
    /* - 수정 포인트: VARIO_UI_TOP_ALT_ROW_TOP_Y                               */
    /* ---------------------------------------------------------------------- */
    row_y = (int16_t)(v->y + VARIO_UI_TOP_ALT_ROW_TOP_Y);
    u8g2_SetFontPosTop(u8g2);
    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    a3_value_w = (int16_t)u8g2_GetStrWidth(u8g2, alt3_text);
    a2_value_w = (int16_t)u8g2_GetStrWidth(u8g2, alt2_text);

    a3_value_x = (int16_t)(right_limit_x - a3_value_w);
    a3_icon_x = (int16_t)(a3_value_x - 11);
    a2_value_x = (int16_t)(a3_icon_x - 12 - a2_value_w);
    a2_icon_x = (int16_t)(a2_value_x - 11);

    vario_display_draw_alt_badge(u8g2, a2_icon_x, row_y, '2');
    u8g2_DrawStr(u8g2, a2_value_x, row_y, alt2_text);
    vario_display_draw_alt_badge(u8g2, a3_icon_x, row_y, '3');
    u8g2_DrawStr(u8g2, a3_value_x, row_y, alt3_text);
    u8g2_SetFontPosBaseline(u8g2);
}

static void vario_display_draw_vario_side_bar(u8g2_t *u8g2,
                                              const vario_viewport_t *v,
                                              float instant_vario_mps,
                                              float average_vario_mps)
{
    uint8_t first_level;
    uint8_t count;
    bool    positive;
    uint8_t level;
    int16_t slot_y;
    int16_t slot_h;
    int16_t left_bar_x;
    int16_t instant_x;
    int16_t avg_x;

    if ((u8g2 == NULL) || (v == NULL))
    {
        return;
    }

    left_bar_x = v->x;
    instant_x = left_bar_x;
    avg_x = (int16_t)(left_bar_x + VARIO_UI_GAUGE_INSTANT_W + VARIO_UI_GAUGE_GAP_W);

    /* ---------------------------------------------------------------------- */
    /* VARIO left side scale                                                   */
    /* - 14px 전체 폭을 scale background 로 유지                               */
    /* - 0~7px  : instantaneous fill                                            */
    /* - 9~13px : n초 average fill                                              */
    /* - major tick : 1.0 m/s                                                   */
    /* - minor tick : 0.5 m/s                                                   */
    /* ---------------------------------------------------------------------- */
    for (level = 0u; level < VARIO_UI_VARIO_HALFSTEP_COUNT; ++level)
    {
        uint8_t tick_w;

        tick_w = ((level % 2u) != 0u) ? VARIO_UI_SCALE_MAJOR_W : VARIO_UI_SCALE_MINOR_W;

        vario_display_get_vario_slot_rect(v, true, level, &slot_y, &slot_h);
        u8g2_DrawHLine(u8g2, left_bar_x, slot_y, tick_w);

        vario_display_get_vario_slot_rect(v, false, level, &slot_y, &slot_h);
        u8g2_DrawHLine(u8g2, left_bar_x, slot_y, tick_w);
    }

    vario_display_compute_vario_fill(instant_vario_mps, &first_level, &count, &positive);
    for (level = first_level; level < (uint8_t)(first_level + count); ++level)
    {
        vario_display_get_vario_slot_rect(v, positive, level, &slot_y, &slot_h);
        u8g2_DrawBox(u8g2,
                     instant_x,
                     (slot_h > 1) ? (slot_y + 1) : slot_y,
                     VARIO_UI_GAUGE_INSTANT_W,
                     (slot_h > 1) ? (slot_h - 1) : slot_h);
    }

    vario_display_compute_vario_fill(average_vario_mps, &first_level, &count, &positive);
    for (level = first_level; level < (uint8_t)(first_level + count); ++level)
    {
        vario_display_get_vario_slot_rect(v, positive, level, &slot_y, &slot_h);
        u8g2_DrawBox(u8g2,
                     avg_x,
                     (slot_h > 1) ? (slot_y + 1) : slot_y,
                     VARIO_UI_GAUGE_AVG_W,
                     (slot_h > 1) ? (slot_h - 1) : slot_h);
    }
}

static void vario_display_draw_gs_side_bar(u8g2_t *u8g2,
                                           const vario_viewport_t *v,
                                           float instant_speed_kmh,
                                           float average_speed_kmh)
{
    uint8_t fill_steps;
    uint8_t level;
    int16_t slot_y;
    int16_t slot_h;
    int16_t right_bar_x;
    int16_t instant_x;
    int16_t avg_x;
    int16_t arrow_y;
    float   clamped_avg_speed;
    float   ratio;

    if ((u8g2 == NULL) || (v == NULL))
    {
        return;
    }

    right_bar_x = (int16_t)(v->x + v->w - VARIO_UI_SIDE_BAR_W);
    avg_x = right_bar_x;
    instant_x = (int16_t)(right_bar_x + VARIO_UI_GAUGE_AVG_W + VARIO_UI_GAUGE_GAP_W);

    /* ---------------------------------------------------------------------- */
    /* GS right side scale                                                     */
    /* - 14px 전체 폭을 scale background 로 유지                               */
    /* - 우측 8px  : instantaneous fill                                         */
    /* - 좌측 5px  : average speed arrow                                        */
    /* - 10 km/h major, 5 km/h minor 성격을 half-step 으로 표현                */
    /* ---------------------------------------------------------------------- */
    for (level = 0u; level < VARIO_UI_GS_STEP_COUNT; ++level)
    {
        uint8_t tick_w;
        int16_t tick_x;

        tick_w = ((level % 2u) == 0u) ? VARIO_UI_SCALE_MAJOR_W : VARIO_UI_SCALE_MINOR_W;
        tick_x = (int16_t)(right_bar_x + VARIO_UI_SIDE_BAR_W - tick_w);
        vario_display_get_gs_slot_rect(v, level, &slot_y, &slot_h);
        u8g2_DrawHLine(u8g2, tick_x, slot_y, tick_w);
    }

    fill_steps = vario_display_compute_gs_fill_steps(instant_speed_kmh);
    for (level = 0u; level < fill_steps; ++level)
    {
        vario_display_get_gs_slot_rect(v, level, &slot_y, &slot_h);
        u8g2_DrawBox(u8g2,
                     instant_x,
                     (slot_h > 1) ? (slot_y + 1) : slot_y,
                     VARIO_UI_GAUGE_INSTANT_W,
                     (slot_h > 1) ? (slot_h - 1) : slot_h);
    }

    if (average_speed_kmh >= VARIO_UI_GS_MIN_VISIBLE_KMH)
    {
        clamped_avg_speed = vario_display_clampf(average_speed_kmh,
                                                 VARIO_UI_GS_MIN_VISIBLE_KMH,
                                                 VARIO_UI_GS_MAX_KMH);
        ratio = (clamped_avg_speed - VARIO_UI_GS_MIN_VISIBLE_KMH) /
                (VARIO_UI_GS_MAX_KMH - VARIO_UI_GS_MIN_VISIBLE_KMH);
        arrow_y = (int16_t)(v->y + v->h - 1 - lroundf(ratio * (float)(v->h - VARIO_UI_ICON_GS_AVG_H)) - (VARIO_UI_ICON_GS_AVG_H / 2));
        if (arrow_y < v->y)
        {
            arrow_y = v->y;
        }
        if ((arrow_y + VARIO_UI_ICON_GS_AVG_H) > (v->y + v->h))
        {
            arrow_y = (int16_t)(v->y + v->h - VARIO_UI_ICON_GS_AVG_H);
        }

        vario_display_draw_xbm(u8g2,
                               avg_x,
                               arrow_y,
                               VARIO_UI_ICON_GS_AVG_W,
                               VARIO_UI_ICON_GS_AVG_H,
                               s_vario_ui_icon_gs_avg_bits);
    }
}

static void vario_display_draw_vario_value_block(u8g2_t *u8g2,
                                                 const vario_viewport_t *v,
                                                 const vario_runtime_t *rt)
{
    char    value_text[20];
    char    top_text[24];
    int16_t box_x;
    int16_t box_y;
    int16_t icon_x;
    int16_t icon_y;
    char    top_vario_value[20];

    if ((u8g2 == NULL) || (v == NULL) || (rt == NULL))
    {
        return;
    }

    box_x = (int16_t)(v->x + VARIO_UI_SIDE_BAR_W + VARIO_UI_BOTTOM_VARIO_X_PAD);
    box_y = (int16_t)(v->y + v->h - VARIO_UI_BOTTOM_BOX_H - VARIO_UI_BOTTOM_BOX_BOTTOM_PAD);

    vario_display_format_vario_value_abs(value_text, sizeof(value_text), rt->baro_vario_mps);
    vario_display_format_vario_small_abs(top_vario_value, sizeof(top_vario_value), rt->max_top_vario_mps);
    snprintf(top_text, sizeof(top_text), "Top Vario %s", top_vario_value);

    /* ---------------------------------------------------------------------- */
    /* 좌하단 VARIO top line                                                   */
    /* - 폰트      : 4x6 small                                                  */
    /* - 기준점    : left number block 상단                                     */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFont(u8g2, u8g2_font_4x6_tf);
    u8g2_DrawStr(u8g2,
                 box_x,
                 (int16_t)(v->y + VARIO_UI_BOTTOM_LABEL_BASELINE_Y),
                 top_text);

    icon_x = (int16_t)(box_x + ((VARIO_UI_BOTTOM_BOX_W - VARIO_UI_ICON_VARIO_UP_W) / 2));
    icon_y = (int16_t)(box_y - VARIO_UI_ICON_VARIO_UP_H - VARIO_UI_BOTTOM_ICON_GAP_Y);

    if (rt->baro_vario_mps > 0.05f)
    {
        vario_display_draw_xbm(u8g2,
                               icon_x,
                               icon_y,
                               VARIO_UI_ICON_VARIO_UP_W,
                               VARIO_UI_ICON_VARIO_UP_H,
                               s_vario_ui_icon_vario_up_bits);
    }
    else if (rt->baro_vario_mps < -0.05f)
    {
        vario_display_draw_xbm(u8g2,
                               icon_x,
                               icon_y,
                               VARIO_UI_ICON_VARIO_DOWN_W,
                               VARIO_UI_ICON_VARIO_DOWN_H,
                               s_vario_ui_icon_vario_down_bits);
    }

    /* ---------------------------------------------------------------------- */
    /* 좌하단 VARIO numeric box                                                */
    /* - 가상 box 크기 : 19.9 급 숫자 + 여유분                                  */
    /* - 하단 bottom 정렬                                                      */
    /* - 정렬      : 좌측 고정                                                  */
    /* - 부호는 icon 으로 전달하므로 숫자는 절대값만 표시                       */
    /* ---------------------------------------------------------------------- */
    vario_display_draw_decimal_value(u8g2,
                                     box_x,
                                     box_y,
                                     VARIO_UI_BOTTOM_BOX_W,
                                     VARIO_UI_BOTTOM_BOX_H,
                                     VARIO_UI_ALIGN_LEFT,
                                     u8g2_font_10x20_mf,
                                     u8g2_font_9x15_mf,
                                     value_text);
}

static void vario_display_draw_speed_value_block(u8g2_t *u8g2,
                                                 const vario_viewport_t *v,
                                                 const vario_runtime_t *rt)
{
    char    value_text[20];
    char    top_text[24];
    char    top_speed_value[20];
    int16_t box_x;
    int16_t box_y;

    if ((u8g2 == NULL) || (v == NULL) || (rt == NULL))
    {
        return;
    }

    box_x = (int16_t)(v->x + v->w - VARIO_UI_SIDE_BAR_W - VARIO_UI_BOTTOM_GS_X_PAD - VARIO_UI_BOTTOM_BOX_W);
    box_y = (int16_t)(v->y + v->h - VARIO_UI_BOTTOM_BOX_H - VARIO_UI_BOTTOM_BOX_BOTTOM_PAD);

    vario_display_format_speed_value(value_text, sizeof(value_text), rt->ground_speed_kmh);
    vario_display_format_speed_small(top_speed_value, sizeof(top_speed_value), s_vario_ui_dynamic.top_speed_kmh);
    snprintf(top_text, sizeof(top_text), "Top Speed %s", top_speed_value);

    u8g2_SetFont(u8g2, u8g2_font_4x6_tf);
    Vario_Display_DrawTextRight(u8g2,
                                (int16_t)(box_x + VARIO_UI_BOTTOM_BOX_W),
                                (int16_t)(v->y + VARIO_UI_BOTTOM_LABEL_BASELINE_Y),
                                top_text);

    /* ---------------------------------------------------------------------- */
    /* 우하단 GS numeric box                                                   */
    /* - 가상 box 크기 : 99.9 급 숫자 + 여유분                                  */
    /* - 하단 bottom 정렬                                                      */
    /* - 정렬      : 우측 고정                                                  */
    /* ---------------------------------------------------------------------- */
    vario_display_draw_decimal_value(u8g2,
                                     box_x,
                                     box_y,
                                     VARIO_UI_BOTTOM_BOX_W,
                                     VARIO_UI_BOTTOM_BOX_H,
                                     VARIO_UI_ALIGN_RIGHT,
                                     u8g2_font_10x20_mf,
                                     u8g2_font_9x15_mf,
                                     value_text);
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

    u8g2_SetFont(u8g2, u8g2_font_10x20_mf);
    Vario_Display_DrawTextCentered(u8g2,
                                   center_x,
                                   (int16_t)(center_y + VARIO_UI_STUB_TITLE_BASELINE_DY),
                                   title);

    u8g2_SetFont(u8g2, u8g2_font_6x12_mf);
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
    bottom_limit_y = (int16_t)(v->y + v->h - VARIO_UI_BOTTOM_BOX_H - VARIO_UI_BOTTOM_BOX_BOTTOM_PAD - VARIO_UI_COMPASS_BOTTOM_GAP);
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

    if (radius < 18)
    {
        radius = 18;
    }

    center_y = (int16_t)(top_limit_y + radius);
    if ((center_y + radius) > bottom_limit_y)
    {
        center_y = (int16_t)(bottom_limit_y - radius);
    }

    vario_display_format_nav_distance(nav_text,
                                      sizeof(nav_text),
                                      nav->label,
                                      nav->current_valid && nav->target_valid,
                                      nav->distance_m);

    /* ---------------------------------------------------------------------- */
    /* compass title / distance line                                           */
    /* - 폰트      : 6x12 medium                                                */
    /* - 기준점    : 나침반 원 바로 위 중심                                     */
    /* - 수정 포인트: VARIO_UI_NAV_LABEL_BASELINE_Y                             */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFont(u8g2, u8g2_font_6x12_mf);
    Vario_Display_DrawTextCentered(u8g2,
                                   center_x,
                                   label_baseline_y,
                                   nav_text);

    /* compass circle */
    u8g2_DrawCircle(u8g2, center_x, center_y, radius, U8G2_DRAW_ALL);
    u8g2_DrawBox(u8g2, center_x - 1, center_y - 1, 3u, 3u);

    heading_deg = (rt->heading_valid != false) ? rt->heading_deg : 0.0f;

    /* ---------------------------------------------------------------------- */
    /* rotating compass tick / cardinal                                        */
    /* - 화면 top 은 항상 현재 진행 방향(heading)                               */
    /* - heading invalid 이면 north-up 으로 동작                                */
    /* ---------------------------------------------------------------------- */
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

        u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
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

    /* aircraft heading marker fixed to top */
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
    /* target arrow                                                            */
    /* - nav target 이 있으면 circle 내부에 bearing arrow 를 그림               */
    /* - heading 이 valid 면 현재 진행방향 기준 상대 bearing                    */
    /* - invalid 면 north-up bearing                                            */
    /* ---------------------------------------------------------------------- */
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

    title_baseline = (int16_t)(v->y + 10);
    rule_y = (int16_t)(v->y + 12);

    u8g2_SetFont(u8g2, u8g2_font_6x12_mf);
    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawStr(u8g2, (uint8_t)(v->x + 2), (uint8_t)title_baseline, title);

    if ((subtitle != NULL) && (subtitle[0] != '\0'))
    {
        u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
        Vario_Display_DrawTextRight(u8g2,
                                    (int16_t)(v->x + v->w - 2),
                                    (int16_t)(v->y + 9),
                                    subtitle);
    }

    u8g2_DrawHLine(u8g2, (uint8_t)v->x, (uint8_t)rule_y, (uint8_t)v->w);
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

    row_height = 12;
    row_top_y = (int16_t)(y_baseline - 9);

    if (selected)
    {
        u8g2_SetDrawColor(u8g2, 1);
        u8g2_DrawBox(u8g2,
                     (uint8_t)(v->x + 1),
                     (uint8_t)row_top_y,
                     (uint8_t)(v->w - 2),
                     (uint8_t)row_height);
        u8g2_SetDrawColor(u8g2, 0);
    }
    else
    {
        u8g2_SetDrawColor(u8g2, 1);
    }

    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    u8g2_DrawStr(u8g2,
                 (uint8_t)(v->x + 3),
                 (uint8_t)y_baseline,
                 selected ? ">" : " ");
    u8g2_DrawStr(u8g2,
                 (uint8_t)(v->x + 12),
                 (uint8_t)y_baseline,
                 label);
    Vario_Display_DrawTextRight(u8g2,
                                (int16_t)(v->x + v->w - 5),
                                y_baseline,
                                value);
    u8g2_SetDrawColor(u8g2, 1);
}

void Vario_Display_RenderFlightPage(u8g2_t *u8g2, vario_flight_page_mode_t mode)
{
    const vario_viewport_t *v;
    const vario_runtime_t  *rt;
    const vario_settings_t *settings;
    vario_display_page_cfg_t cfg;
    float avg_vario_mps;
    float avg_speed_kmh;
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

    vario_display_update_dynamic_metrics(rt, settings);
    vario_display_get_average_values(rt, settings, &avg_vario_mps, &avg_speed_kmh);
    vario_display_compute_nav_solution(rt, &nav);

    if (cfg.show_trail_background != false)
    {
        trail_v.x = (int16_t)(v->x + VARIO_UI_SIDE_BAR_W);
        trail_v.y = v->y;
        trail_v.w = (int16_t)(v->w - (2 * VARIO_UI_SIDE_BAR_W));
        trail_v.h = v->h;
        vario_display_draw_trail_background(u8g2, &trail_v, rt, settings);
    }

    vario_display_draw_vario_side_bar(u8g2, v, rt->baro_vario_mps, avg_vario_mps);
    vario_display_draw_gs_side_bar(u8g2, v, rt->ground_speed_kmh, avg_speed_kmh);

    vario_display_draw_top_left_metrics(u8g2, v, rt);
    vario_display_draw_top_center_clock(u8g2, v, rt, settings);
    vario_display_draw_top_right_altitudes(u8g2, v, rt);

    if (cfg.show_compass != false)
    {
        vario_display_draw_compass(u8g2, v, rt, &nav);
    }

    if (cfg.show_stub_overlay != false)
    {
        vario_display_draw_stub_overlay(u8g2,
                                        v,
                                        (cfg.stub_title != NULL) ? cfg.stub_title : "PAGE 3",
                                        (cfg.stub_subtitle != NULL) ? cfg.stub_subtitle : "UI STUB");
    }

    vario_display_draw_vario_value_block(u8g2, v, rt);
    vario_display_draw_speed_value_block(u8g2, v, rt);

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
