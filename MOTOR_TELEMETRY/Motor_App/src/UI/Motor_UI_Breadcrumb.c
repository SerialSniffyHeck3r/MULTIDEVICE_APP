#include "Motor_UI_Internal.h"
#include "Motor_UI_Xbm.h"
#include "Motor_Units.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#define MOTOR_BREADCRUMB_FONT_SPEED_LARGE  u8g2_font_logisoso22_tn
#define MOTOR_BREADCRUMB_FONT_ANGLE_LARGE  u8g2_font_logisoso22_tn
#define MOTOR_BREADCRUMB_FONT_MEDIUM       u8g2_font_7x14B_tf
#define MOTOR_BREADCRUMB_FONT_SMALL        u8g2_font_5x7_tf

#define MOTOR_BREADCRUMB_SIDE_BAR_W        14
#define MOTOR_BREADCRUMB_SIDE_BAR_INST_W    8
#define MOTOR_BREADCRUMB_SIDE_BAR_AVG_W     4
#define MOTOR_BREADCRUMB_TOP_GAUGE_H       14
#define MOTOR_BREADCRUMB_TOP_GAUGE_PAD_X    4
#define MOTOR_BREADCRUMB_TOP_GAUGE_TEXT_Y   1
#define MOTOR_BREADCRUMB_TOP_GAP_Y          3
#define MOTOR_BREADCRUMB_BOTTOM_LINE_GAP_Y  3
#define MOTOR_BREADCRUMB_BOTTOM_INFO_PAD_Y  2
#define MOTOR_BREADCRUMB_ANGLE_MAX_DEG_X10 600
#define MOTOR_BREADCRUMB_ANGLE_FINE1_X10   350
#define MOTOR_BREADCRUMB_ANGLE_FINE2_X10   500
#define MOTOR_BREADCRUMB_ANGLE_BAR_H         7
#define MOTOR_BREADCRUMB_LONG_MAX_MG      1000
#define MOTOR_BREADCRUMB_GAUGE_CACHE_MAX_HALF_SPAN 112

static const uint16_t s_motor_breadcrumb_scale_presets_m[] = { 100u, 250u, 500u, 750u, 1000u };

typedef struct
{
    uint8_t init;
    uint8_t scale_idx;
    uint8_t heading_up;
    int32_t avg_long_mg_x10;
} motor_breadcrumb_ui_t;

typedef struct
{
    int32_t org_lat_e7;
    int32_t org_lon_e7;
    int16_t cx;
    int16_t cy;
    float lat_m_per_e7;
    float lon_m_per_e7;
    float inv_m_per_px;
    float sin_h;
    float cos_h;
    uint8_t head_up;
} motor_breadcrumb_project_ctx_t;

typedef struct
{
    uint8_t init;
    int16_t medium_h;
    int16_t large_h;
    int16_t small_h;
    int16_t large_angle_w;
    int16_t medium_angle_w;
    int16_t speed_unit_w;
} motor_breadcrumb_layout_cache_t;

typedef struct
{
    uint8_t valid;
    uint8_t home_projected;
    uint16_t point_count;
    uint16_t head;
    uint16_t count;
    int16_t rect_x;
    int16_t rect_y;
    int16_t rect_w;
    int16_t rect_h;
    int16_t center_x;
    int16_t center_y;
    int32_t org_lat_e7;
    int32_t org_lon_e7;
    int32_t home_lat_e7;
    int32_t home_lon_e7;
    uint8_t home_valid;
    uint16_t scale_m;
    uint8_t heading_up;
    int32_t heading_deg_x10;
    int16_t point_x[MOTOR_BREADCRUMB_POINT_COUNT];
    int16_t point_y[MOTOR_BREADCRUMB_POINT_COUNT];
    uint8_t point_connected[MOTOR_BREADCRUMB_POINT_COUNT];
    int16_t home_x;
    int16_t home_y;
} motor_breadcrumb_trail_cache_t;

typedef struct
{
    uint8_t valid;
    int16_t half_span_px;
    int16_t inner_h;
    int16_t outer_h;
    int16_t span_by_offset[MOTOR_BREADCRUMB_GAUGE_CACHE_MAX_HALF_SPAN + 1];
    int16_t tick_offset_px[(MOTOR_BREADCRUMB_ANGLE_MAX_DEG_X10 / 100) + 1];
} motor_breadcrumb_gauge_cache_t;

static motor_breadcrumb_ui_t s_motor_breadcrumb_ui;
static motor_breadcrumb_layout_cache_t s_motor_breadcrumb_layout_cache;
static motor_breadcrumb_trail_cache_t  s_motor_breadcrumb_trail_cache;
static motor_breadcrumb_gauge_cache_t  s_motor_breadcrumb_gauge_cache;

static void motor_breadcrumb_init(void)
{
    if (s_motor_breadcrumb_ui.init != 0u)
    {
        return;
    }

    s_motor_breadcrumb_ui.init = 1u;
    s_motor_breadcrumb_ui.scale_idx = 1u;
}

void Motor_UI_Breadcrumb_CycleScale(void)
{
    motor_breadcrumb_init();
    s_motor_breadcrumb_ui.scale_idx =
        (uint8_t)((s_motor_breadcrumb_ui.scale_idx + 1u) %
                  (sizeof(s_motor_breadcrumb_scale_presets_m) / sizeof(s_motor_breadcrumb_scale_presets_m[0])));
}

void Motor_UI_Breadcrumb_ToggleHeadingUpMode(void)
{
    motor_breadcrumb_init();
    s_motor_breadcrumb_ui.heading_up = (s_motor_breadcrumb_ui.heading_up == 0u) ? 1u : 0u;
}

uint16_t Motor_UI_Breadcrumb_GetScaleMeters(void)
{
    motor_breadcrumb_init();
    return s_motor_breadcrumb_scale_presets_m[s_motor_breadcrumb_ui.scale_idx];
}

uint8_t Motor_UI_Breadcrumb_IsHeadingUpMode(void)
{
    motor_breadcrumb_init();
    return s_motor_breadcrumb_ui.heading_up;
}

static int32_t motor_breadcrumb_clamp_i32(int32_t value, int32_t lo, int32_t hi)
{
    if (value < lo)
    {
        return lo;
    }
    if (value > hi)
    {
        return hi;
    }
    return value;
}

static int32_t motor_breadcrumb_abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
}

static float motor_breadcrumb_deg_to_rad(float deg)
{
    return deg * (3.14159265358979323846f / 180.0f);
}

static int16_t motor_breadcrumb_font_h(u8g2_t *u8g2, const uint8_t *font)
{
    u8g2_SetFont(u8g2, font);
    return (int16_t)(u8g2_GetAscent(u8g2) - u8g2_GetDescent(u8g2));
}

static uint16_t motor_breadcrumb_text_w(u8g2_t *u8g2, const uint8_t *font, const char *text)
{
    u8g2_SetFont(u8g2, font);
    return u8g2_GetStrWidth(u8g2, text);
}

static void motor_breadcrumb_text_top(u8g2_t *u8g2,
                                      const uint8_t *font,
                                      int16_t x,
                                      int16_t y,
                                      const char *text)
{
    u8g2_SetFontPosTop(u8g2);
    u8g2_SetFont(u8g2, font);
    u8g2_DrawStr(u8g2, x, y, text);
    u8g2_SetFontPosBaseline(u8g2);
}

static const motor_breadcrumb_layout_cache_t *motor_breadcrumb_get_layout_cache(u8g2_t *u8g2)
{
    if (u8g2 == 0)
    {
        return 0;
    }

    if (s_motor_breadcrumb_layout_cache.init == 0u)
    {
        /* ------------------------------------------------------------------ */
        /*  고정 폰트 레이아웃 캐시                                            */
        /*  - breadcrumb 화면은 매 프레임 같은 폰트와 같은 슬롯 폭을 반복해서  */
        /*    사용한다.                                                        */
        /*  - ascent/descent, "00" 폭, "km/h" 폭을 매번 다시 질의하면           */
        /*    bank 각도처럼 자주 바뀌는 화면에서 불필요한 font metric 조회가     */
        /*    누적된다.                                                        */
        /*  - 이 값들은 폰트가 바뀌지 않는 한 사실상 상수이므로, 첫 사용 시      */
        /*    한 번만 계산해 두고 이후 프레임에서는 그대로 재사용한다.          */
        /* ------------------------------------------------------------------ */
        s_motor_breadcrumb_layout_cache.medium_h =
            motor_breadcrumb_font_h(u8g2, MOTOR_BREADCRUMB_FONT_MEDIUM);
        s_motor_breadcrumb_layout_cache.large_h =
            motor_breadcrumb_font_h(u8g2, MOTOR_BREADCRUMB_FONT_ANGLE_LARGE);
        s_motor_breadcrumb_layout_cache.small_h =
            motor_breadcrumb_font_h(u8g2, MOTOR_BREADCRUMB_FONT_SMALL);
        s_motor_breadcrumb_layout_cache.large_angle_w =
            (int16_t)motor_breadcrumb_text_w(u8g2, MOTOR_BREADCRUMB_FONT_ANGLE_LARGE, "00");
        s_motor_breadcrumb_layout_cache.medium_angle_w =
            (int16_t)motor_breadcrumb_text_w(u8g2, MOTOR_BREADCRUMB_FONT_MEDIUM, "00");
        s_motor_breadcrumb_layout_cache.speed_unit_w =
            (int16_t)motor_breadcrumb_text_w(u8g2, MOTOR_BREADCRUMB_FONT_SMALL, "km/h");
        s_motor_breadcrumb_layout_cache.init = 1u;
    }

    return &s_motor_breadcrumb_layout_cache;
}

static void motor_breadcrumb_format_hms(char *out_text, size_t out_size, uint32_t seconds)
{
    uint32_t h;
    uint32_t m;
    uint32_t s;

    if ((out_text == 0) || (out_size == 0u))
    {
        return;
    }

    h = seconds / 3600u;
    if (h > 99u)
    {
        h = 99u;
    }
    m = (seconds % 3600u) / 60u;
    s = seconds % 60u;
    (void)snprintf(out_text,
                   out_size,
                   "%02lu:%02lu:%02lu",
                   (unsigned long)h,
                   (unsigned long)m,
                   (unsigned long)s);
}

static void motor_breadcrumb_format_fixed_angle_2(char *out_text, size_t out_size, int32_t value_x10)
{
    int32_t abs_deg;

    if ((out_text == 0) || (out_size == 0u))
    {
        return;
    }

    abs_deg = motor_breadcrumb_abs_i32(value_x10) / 10;
    if (abs_deg > 99)
    {
        abs_deg = 99;
    }
    (void)snprintf(out_text, out_size, "%02ld", (long)abs_deg);
}

static void motor_breadcrumb_format_fixed_speed_000_0(char *out_text,
                                                      size_t out_size,
                                                      int32_t speed_kmh_x10,
                                                      const motor_unit_settings_t *units)
{
    int32_t converted;
    int32_t whole;
    int32_t frac;

    if ((out_text == 0) || (out_size == 0u))
    {
        return;
    }

    converted = Motor_Units_ConvertSpeedX10(speed_kmh_x10, units);
    if (converted < 0)
    {
        converted = 0;
    }
    if (converted > 9999)
    {
        converted = 9999;
    }

    whole = converted / 10;
    frac = converted % 10;
    (void)snprintf(out_text, out_size, "%ld.%01ld", (long)whole, (long)frac);
}

static void motor_breadcrumb_draw_info_with_icon(u8g2_t *u8g2,
                                                 int16_t x,
                                                 int16_t y,
                                                 const unsigned char *bits,
                                                 const char *text)
{
    const motor_breadcrumb_layout_cache_t *layout_cache;
    int16_t icon_y;
    int16_t text_x;

    layout_cache = motor_breadcrumb_get_layout_cache(u8g2);
    icon_y = (int16_t)(y + ((((layout_cache != 0) ? layout_cache->medium_h : 0) - MOTOR_UI_XBM_INFO_AVG_H) / 2));
    u8g2_DrawXBMP(u8g2,
                  x,
                  icon_y,
                  MOTOR_UI_XBM_INFO_AVG_W,
                  MOTOR_UI_XBM_INFO_AVG_H,
                  bits);
    text_x = (int16_t)(x + MOTOR_UI_XBM_INFO_AVG_W + 3);
    motor_breadcrumb_text_top(u8g2, MOTOR_BREADCRUMB_FONT_MEDIUM, text_x, y, text);
}

static void motor_breadcrumb_draw_centered_text_top(u8g2_t *u8g2,
                                                    const uint8_t *font,
                                                    const ui_rect_t *rect,
                                                    int16_t y,
                                                    const char *text)
{
    int16_t x;

    if ((u8g2 == 0) || (font == 0) || (rect == 0) || (text == 0))
    {
        return;
    }

    x = (int16_t)(rect->x + ((rect->w - (int16_t)motor_breadcrumb_text_w(u8g2, font, text)) / 2));
    motor_breadcrumb_text_top(u8g2, font, x, y, text);
}

static int32_t motor_breadcrumb_avg_speed_x10(const motor_state_t *state)
{
    uint64_t scaled_distance;
    uint32_t seconds;

    seconds = state->record_session.moving_seconds;
    if (seconds == 0u)
    {
        seconds = state->record_session.ride_seconds;
    }
    if (seconds == 0u)
    {
        return 0;
    }

    scaled_distance = ((uint64_t)state->record_session.distance_m * 36ull) + ((uint64_t)seconds / 2ull);
    return (int32_t)(scaled_distance / (uint64_t)seconds);
}

static int32_t motor_breadcrumb_avg_long_mg(const motor_state_t *state)
{
    int32_t current_x10;

    motor_breadcrumb_init();
    current_x10 = state->dyn.lon_accel_mg * 10;
    if (s_motor_breadcrumb_ui.avg_long_mg_x10 == 0)
    {
        s_motor_breadcrumb_ui.avg_long_mg_x10 = current_x10;
    }
    else
    {
        s_motor_breadcrumb_ui.avg_long_mg_x10 +=
            ((current_x10 - s_motor_breadcrumb_ui.avg_long_mg_x10) / 8);
    }
    return s_motor_breadcrumb_ui.avg_long_mg_x10 / 10;
}

static int32_t motor_breadcrumb_speed_scale_max_x10(const motor_state_t *state)
{
    switch ((motor_main_speed_scale_t)state->settings.display.main_speed_scale)
    {
    case MOTOR_MAIN_SPEED_SCALE_100:
        return 1000;
    case MOTOR_MAIN_SPEED_SCALE_300:
        return 3000;
    case MOTOR_MAIN_SPEED_SCALE_200:
    default:
        return 2000;
    }
}

static void motor_breadcrumb_speed_tick_steps(const motor_state_t *state,
                                              float *minor_kmh,
                                              float *major_kmh)
{
    switch ((motor_main_speed_scale_t)state->settings.display.main_speed_scale)
    {
    case MOTOR_MAIN_SPEED_SCALE_100:
        *minor_kmh = 10.0f;
        *major_kmh = 20.0f;
        break;
    case MOTOR_MAIN_SPEED_SCALE_300:
        *minor_kmh = 25.0f;
        *major_kmh = 50.0f;
        break;
    case MOTOR_MAIN_SPEED_SCALE_200:
    default:
        *minor_kmh = 20.0f;
        *major_kmh = 50.0f;
        break;
    }
}

static int16_t motor_breadcrumb_scale_angle_to_half_span(int32_t abs_value_x10, int16_t half_span_px)
{
    int16_t span_35_px;
    int16_t span_50_px;

    if (half_span_px <= 0)
    {
        return 0;
    }

    abs_value_x10 = motor_breadcrumb_clamp_i32(abs_value_x10, 0, MOTOR_BREADCRUMB_ANGLE_MAX_DEG_X10);
    span_35_px = (int16_t)((half_span_px * 6) / 10);
    span_50_px = (int16_t)((half_span_px * 9) / 10);

    if (abs_value_x10 <= MOTOR_BREADCRUMB_ANGLE_FINE1_X10)
    {
        return (int16_t)(((int64_t)abs_value_x10 * span_35_px) / MOTOR_BREADCRUMB_ANGLE_FINE1_X10);
    }

    if (abs_value_x10 <= MOTOR_BREADCRUMB_ANGLE_FINE2_X10)
    {
        return (int16_t)(span_35_px +
                         (((int64_t)(abs_value_x10 - MOTOR_BREADCRUMB_ANGLE_FINE1_X10) *
                           (span_50_px - span_35_px)) /
                          (MOTOR_BREADCRUMB_ANGLE_FINE2_X10 - MOTOR_BREADCRUMB_ANGLE_FINE1_X10)));
    }

    return (int16_t)(span_50_px +
                     (((int64_t)(abs_value_x10 - MOTOR_BREADCRUMB_ANGLE_FINE2_X10) *
                       (half_span_px - span_50_px)) /
                      (MOTOR_BREADCRUMB_ANGLE_MAX_DEG_X10 - MOTOR_BREADCRUMB_ANGLE_FINE2_X10)));
}

static int16_t motor_breadcrumb_variable_tick_span(int16_t offset_px,
                                                   int16_t half_span_px,
                                                   int16_t inner_span,
                                                   int16_t outer_span)
{
    int32_t denom;
    int32_t grow_span;
    int32_t numerator;

    if (outer_span < inner_span)
    {
        return inner_span;
    }
    if (half_span_px <= 0)
    {
        return outer_span;
    }

    grow_span = (int32_t)(outer_span - inner_span);
    denom = (int32_t)half_span_px * (int32_t)half_span_px;
    numerator = (grow_span * (int32_t)offset_px * (int32_t)offset_px) + (denom / 2);
    return (int16_t)(inner_span + (numerator / denom));
}

static const motor_breadcrumb_gauge_cache_t *motor_breadcrumb_get_gauge_cache(int16_t half_span_px,
                                                                               int16_t inner_h,
                                                                               int16_t outer_h)
{
    int16_t i;
    uint16_t tick_idx;

    if (half_span_px <= 0)
    {
        return 0;
    }

    if (half_span_px > MOTOR_BREADCRUMB_GAUGE_CACHE_MAX_HALF_SPAN)
    {
        half_span_px = MOTOR_BREADCRUMB_GAUGE_CACHE_MAX_HALF_SPAN;
    }

    if ((s_motor_breadcrumb_gauge_cache.valid != 0u) &&
        (s_motor_breadcrumb_gauge_cache.half_span_px == half_span_px) &&
        (s_motor_breadcrumb_gauge_cache.inner_h == inner_h) &&
        (s_motor_breadcrumb_gauge_cache.outer_h == outer_h))
    {
        return &s_motor_breadcrumb_gauge_cache;
    }

    /* ---------------------------------------------------------------------- */
    /*  상단 bank gauge의 기하 계산 캐시                                       */
    /*  - 기존 구현은 매 프레임마다 각 column 높이와 tick 위치를 다시 계산했다. */
    /*  - breadcrumb 화면을 20fps로 올리면 이 계산 비용도 같이 늘어나므로,      */
    /*    viewport 폭이 같을 때는 "offset_px -> 높이" 테이블을 재사용한다.      */
    /*  - 이렇게 하면 실제 draw 단계는 lookup + draw call만 수행하면 된다.      */
    /* ---------------------------------------------------------------------- */
    s_motor_breadcrumb_gauge_cache.valid = 1u;
    s_motor_breadcrumb_gauge_cache.half_span_px = half_span_px;
    s_motor_breadcrumb_gauge_cache.inner_h = inner_h;
    s_motor_breadcrumb_gauge_cache.outer_h = outer_h;

    for (i = 0; i <= half_span_px; ++i)
    {
        s_motor_breadcrumb_gauge_cache.span_by_offset[i] =
            motor_breadcrumb_variable_tick_span(i, half_span_px, inner_h, outer_h);
    }

    for (tick_idx = 0u; tick_idx <= (MOTOR_BREADCRUMB_ANGLE_MAX_DEG_X10 / 100); ++tick_idx)
    {
        s_motor_breadcrumb_gauge_cache.tick_offset_px[tick_idx] =
            motor_breadcrumb_scale_angle_to_half_span((int32_t)tick_idx * 100,
                                                      half_span_px);
    }

    return &s_motor_breadcrumb_gauge_cache;
}

static uint8_t motor_breadcrumb_get_origin(const motor_state_t *state,
                                           int32_t *lat_e7,
                                           int32_t *lon_e7)
{
    int32_t i;

    if (state->nav.valid != false)
    {
        *lat_e7 = state->nav.lat_e7;
        *lon_e7 = state->nav.lon_e7;
        return 1u;
    }

    for (i = (int32_t)state->breadcrumb.count - 1; i >= 0; --i)
    {
        uint16_t idx;
        const motor_breadcrumb_point_t *point;

        idx = (uint16_t)((state->breadcrumb.head + MOTOR_BREADCRUMB_POINT_COUNT -
                          state->breadcrumb.count + (uint16_t)i) % MOTOR_BREADCRUMB_POINT_COUNT);
        point = &state->breadcrumb.points[idx];
        if (point->valid == 0u)
        {
            continue;
        }

        *lat_e7 = point->lat_e7;
        *lon_e7 = point->lon_e7;
        return 1u;
    }

    return 0u;
}

static uint8_t motor_breadcrumb_trail_cache_matches(const ui_rect_t *trail_rect,
                                                    const motor_state_t *state,
                                                    int32_t org_lat_e7,
                                                    int32_t org_lon_e7,
                                                    uint16_t scale_m,
                                                    uint8_t heading_up,
                                                    int32_t heading_deg_x10)
{
    if ((trail_rect == 0) || (state == 0))
    {
        return 0u;
    }

    if (s_motor_breadcrumb_trail_cache.valid == 0u)
    {
        return 0u;
    }

    if ((s_motor_breadcrumb_trail_cache.rect_x != trail_rect->x) ||
        (s_motor_breadcrumb_trail_cache.rect_y != trail_rect->y) ||
        (s_motor_breadcrumb_trail_cache.rect_w != trail_rect->w) ||
        (s_motor_breadcrumb_trail_cache.rect_h != trail_rect->h) ||
        (s_motor_breadcrumb_trail_cache.head != state->breadcrumb.head) ||
        (s_motor_breadcrumb_trail_cache.count != state->breadcrumb.count) ||
        (s_motor_breadcrumb_trail_cache.home_valid != state->breadcrumb.home_valid) ||
        (s_motor_breadcrumb_trail_cache.home_lat_e7 != state->breadcrumb.home_lat_e7) ||
        (s_motor_breadcrumb_trail_cache.home_lon_e7 != state->breadcrumb.home_lon_e7) ||
        (s_motor_breadcrumb_trail_cache.org_lat_e7 != org_lat_e7) ||
        (s_motor_breadcrumb_trail_cache.org_lon_e7 != org_lon_e7) ||
        (s_motor_breadcrumb_trail_cache.scale_m != scale_m) ||
        (s_motor_breadcrumb_trail_cache.heading_up != heading_up))
    {
        return 0u;
    }

    if ((heading_up != 0u) &&
        (s_motor_breadcrumb_trail_cache.heading_deg_x10 != heading_deg_x10))
    {
        return 0u;
    }

    return 1u;
}

static void motor_breadcrumb_project_ctx_init(motor_breadcrumb_project_ctx_t *ctx,
                                              int32_t org_lat_e7,
                                              int32_t org_lon_e7,
                                              uint8_t head_up,
                                              float heading_deg,
                                              int16_t cx,
                                              int16_t cy,
                                              float m_per_px)
{
    float mean_lat_rad;

    if (ctx == 0)
    {
        return;
    }

    mean_lat_rad = motor_breadcrumb_deg_to_rad(((float)org_lat_e7) * 1.0e-7f);
    ctx->org_lat_e7 = org_lat_e7;
    ctx->org_lon_e7 = org_lon_e7;
    ctx->cx = cx;
    ctx->cy = cy;
    ctx->lat_m_per_e7 = 0.0111132f;
    ctx->lon_m_per_e7 = 0.01113195f * cosf(mean_lat_rad);
    ctx->inv_m_per_px = (m_per_px > 0.0001f) ? (1.0f / m_per_px) : 1.0f;
    ctx->head_up = head_up;

    if (head_up != 0u)
    {
        float rad;

        rad = motor_breadcrumb_deg_to_rad(heading_deg);
        ctx->sin_h = sinf(rad);
        ctx->cos_h = cosf(rad);
    }
    else
    {
        ctx->sin_h = 0.0f;
        ctx->cos_h = 1.0f;
    }
}

static uint8_t motor_breadcrumb_project_fast(const motor_breadcrumb_project_ctx_t *ctx,
                                             int32_t lat_e7,
                                             int32_t lon_e7,
                                             int16_t *out_x,
                                             int16_t *out_y)
{
    float dx_m;
    float dy_m;
    float px_m;
    float py_m;

    if ((ctx == 0) || (out_x == 0) || (out_y == 0))
    {
        return 0u;
    }

    dx_m = ((float)(lon_e7 - ctx->org_lon_e7)) * ctx->lon_m_per_e7;
    dy_m = ((float)(lat_e7 - ctx->org_lat_e7)) * ctx->lat_m_per_e7;
    px_m = dx_m;
    py_m = dy_m;

    if (ctx->head_up != 0u)
    {
        float rot_x;
        float rot_y;

        rot_x = (dx_m * ctx->cos_h) - (dy_m * ctx->sin_h);
        rot_y = (dx_m * ctx->sin_h) + (dy_m * ctx->cos_h);
        px_m = rot_x;
        py_m = rot_y;
    }

    *out_x = (int16_t)lroundf((float)ctx->cx + (px_m * ctx->inv_m_per_px));
    *out_y = (int16_t)lroundf((float)ctx->cy - (py_m * ctx->inv_m_per_px));
    return 1u;
}

static void motor_breadcrumb_rebuild_trail_cache(const ui_rect_t *trail_rect,
                                                 const motor_state_t *state,
                                                 int32_t org_lat_e7,
                                                 int32_t org_lon_e7,
                                                 uint16_t scale_m,
                                                 uint8_t heading_up,
                                                 int32_t heading_deg_x10)
{
    motor_breadcrumb_project_ctx_t proj;
    uint16_t i;
    uint16_t start_idx;
    uint16_t point_count;
    int16_t cx;
    int16_t cy;
    float half_w;
    float half_h;
    float m_per_px;
    float heading_deg;
    uint8_t have_prev;
    int16_t prev_x;
    int16_t prev_y;
    uint32_t prev_tick_ms;

    if ((trail_rect == 0) || (state == 0))
    {
        return;
    }

    s_motor_breadcrumb_trail_cache.valid = 0u;
    s_motor_breadcrumb_trail_cache.point_count = 0u;
    s_motor_breadcrumb_trail_cache.home_projected = 0u;

    cx = (int16_t)(trail_rect->x + (trail_rect->w / 2));
    cy = (int16_t)(trail_rect->y + (trail_rect->h / 2));
    half_w = (float)(trail_rect->w / 2);
    half_h = (float)(trail_rect->h / 2);
    m_per_px = ((float)scale_m) / ((half_w < half_h) ? half_w : half_h);
    heading_deg = ((float)heading_deg_x10) * 0.1f;

    motor_breadcrumb_project_ctx_init(&proj,
                                      org_lat_e7,
                                      org_lon_e7,
                                      heading_up,
                                      heading_deg,
                                      cx,
                                      cy,
                                      m_per_px);

    s_motor_breadcrumb_trail_cache.rect_x = trail_rect->x;
    s_motor_breadcrumb_trail_cache.rect_y = trail_rect->y;
    s_motor_breadcrumb_trail_cache.rect_w = trail_rect->w;
    s_motor_breadcrumb_trail_cache.rect_h = trail_rect->h;
    s_motor_breadcrumb_trail_cache.center_x = cx;
    s_motor_breadcrumb_trail_cache.center_y = cy;
    s_motor_breadcrumb_trail_cache.head = state->breadcrumb.head;
    s_motor_breadcrumb_trail_cache.count = state->breadcrumb.count;
    s_motor_breadcrumb_trail_cache.org_lat_e7 = org_lat_e7;
    s_motor_breadcrumb_trail_cache.org_lon_e7 = org_lon_e7;
    s_motor_breadcrumb_trail_cache.home_lat_e7 = state->breadcrumb.home_lat_e7;
    s_motor_breadcrumb_trail_cache.home_lon_e7 = state->breadcrumb.home_lon_e7;
    s_motor_breadcrumb_trail_cache.home_valid = state->breadcrumb.home_valid;
    s_motor_breadcrumb_trail_cache.scale_m = scale_m;
    s_motor_breadcrumb_trail_cache.heading_up = heading_up;
    s_motor_breadcrumb_trail_cache.heading_deg_x10 = heading_deg_x10;

    point_count = 0u;
    have_prev = 0u;
    prev_x = 0;
    prev_y = 0;
    prev_tick_ms = 0u;
    start_idx = (uint16_t)((state->breadcrumb.head + MOTOR_BREADCRUMB_POINT_COUNT - state->breadcrumb.count) %
                           MOTOR_BREADCRUMB_POINT_COUNT);

    /* ---------------------------------------------------------------------- */
    /*  Trail 투영 캐시                                                        */
    /*  - breadcrumb trail은 bank 각도와 무관하지만, 기존 구현은 bank가 바뀔 때 */
    /*    도 모든 포인트를 다시 위경도->화면좌표로 투영했다.                   */
    /*  - 현재 위치/scale/heading-up/viewport/breadcrumb ring이 같으면         */
    /*    이전 투영 결과를 그대로 써도 된다.                                  */
    /*  - 이 캐시는 "화면에 그릴 좌표열" 자체를 저장해서, bank 각도처럼         */
    /*    trail과 무관한 변화에서는 draw 단계가 거의 선분 출력만 하게 만든다.   */
    /* ---------------------------------------------------------------------- */
    for (i = 0u; i < state->breadcrumb.count; ++i)
    {
        uint16_t idx;
        const motor_breadcrumb_point_t *point;
        int16_t px;
        int16_t py;
        uint8_t connected;

        idx = (uint16_t)((start_idx + i) % MOTOR_BREADCRUMB_POINT_COUNT);
        point = &state->breadcrumb.points[idx];
        if (point->valid == 0u)
        {
            continue;
        }

        (void)motor_breadcrumb_project_fast(&proj,
                                            point->lat_e7,
                                            point->lon_e7,
                                            &px,
                                            &py);

        connected = 0u;
        if (have_prev != 0u)
        {
            if (((uint32_t)(point->tick_ms - prev_tick_ms) <= 5000u) &&
                (motor_breadcrumb_abs_i32((int32_t)px - prev_x) <= trail_rect->w) &&
                (motor_breadcrumb_abs_i32((int32_t)py - prev_y) <= trail_rect->h))
            {
                connected = 1u;
            }
        }

        if (point_count < MOTOR_BREADCRUMB_POINT_COUNT)
        {
            s_motor_breadcrumb_trail_cache.point_x[point_count] = px;
            s_motor_breadcrumb_trail_cache.point_y[point_count] = py;
            s_motor_breadcrumb_trail_cache.point_connected[point_count] = connected;
            ++point_count;
        }

        prev_x = px;
        prev_y = py;
        prev_tick_ms = point->tick_ms;
        have_prev = 1u;
    }

    s_motor_breadcrumb_trail_cache.point_count = point_count;

    if (state->breadcrumb.home_valid != 0u)
    {
        int16_t hx;
        int16_t hy;

        (void)motor_breadcrumb_project_fast(&proj,
                                            state->breadcrumb.home_lat_e7,
                                            state->breadcrumb.home_lon_e7,
                                            &hx,
                                            &hy);
        s_motor_breadcrumb_trail_cache.home_x = hx;
        s_motor_breadcrumb_trail_cache.home_y = hy;
        s_motor_breadcrumb_trail_cache.home_projected = 1u;
    }

    s_motor_breadcrumb_trail_cache.valid = 1u;
}

static void motor_breadcrumb_draw_arrow(u8g2_t *u8g2,
                                        int16_t cx,
                                        int16_t cy,
                                        float heading_deg)
{
    float rad;
    int16_t tip_x;
    int16_t tip_y;
    int16_t left_x;
    int16_t left_y;
    int16_t right_x;
    int16_t right_y;

    rad = motor_breadcrumb_deg_to_rad(heading_deg);
    tip_x = (int16_t)lroundf((float)cx + (sinf(rad) * 5.0f));
    tip_y = (int16_t)lroundf((float)cy - (cosf(rad) * 5.0f));
    left_x = (int16_t)lroundf((float)cx + (sinf(rad + 2.5f) * 3.0f));
    left_y = (int16_t)lroundf((float)cy - (cosf(rad + 2.5f) * 3.0f));
    right_x = (int16_t)lroundf((float)cx + (sinf(rad - 2.5f) * 3.0f));
    right_y = (int16_t)lroundf((float)cy - (cosf(rad - 2.5f) * 3.0f));

    u8g2_DrawLine(u8g2, tip_x, tip_y, left_x, left_y);
    u8g2_DrawLine(u8g2, tip_x, tip_y, right_x, right_y);
    u8g2_DrawLine(u8g2, left_x, left_y, right_x, right_y);
}

static void motor_breadcrumb_draw_top_angle(u8g2_t *u8g2,
                                            const ui_rect_t *section,
                                            const motor_state_t *state)
{
    const motor_breadcrumb_gauge_cache_t *gauge_cache;
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    int16_t center_x;
    int16_t half_span_px;
    int16_t inner_h;
    int16_t outer_h;
    int16_t i;
    int16_t offset_px;
    int16_t draw_h;
    int32_t clamped_value;
    int32_t abs_value;
    x = (int16_t)(section->x + MOTOR_BREADCRUMB_SIDE_BAR_W + MOTOR_BREADCRUMB_TOP_GAUGE_PAD_X);
    y = section->y;
    w = (int16_t)(section->w - (MOTOR_BREADCRUMB_SIDE_BAR_W * 2) - (MOTOR_BREADCRUMB_TOP_GAUGE_PAD_X * 2));
    h = MOTOR_BREADCRUMB_TOP_GAUGE_H;
    center_x = (int16_t)(x + (w / 2));
    half_span_px = (int16_t)((w - 1) / 2);
    clamped_value = motor_breadcrumb_clamp_i32((int32_t)state->dyn.bank_deg_x10,
                                               -MOTOR_BREADCRUMB_ANGLE_MAX_DEG_X10,
                                               MOTOR_BREADCRUMB_ANGLE_MAX_DEG_X10);
    abs_value = motor_breadcrumb_abs_i32(clamped_value);
    outer_h = (int16_t)(h + 2);
    inner_h = (MOTOR_BREADCRUMB_ANGLE_BAR_H > 3) ? (int16_t)((MOTOR_BREADCRUMB_ANGLE_BAR_H / 3) + 3 + 2) : 6;
    if (inner_h > outer_h)
    {
        inner_h = outer_h;
    }
    gauge_cache = motor_breadcrumb_get_gauge_cache(half_span_px, inner_h, (int16_t)(h + 4));

    if (abs_value > 0)
    {
        int16_t fill_px;

        fill_px = motor_breadcrumb_scale_angle_to_half_span(abs_value, half_span_px);
        for (offset_px = 0; offset_px <= fill_px; offset_px++)
        {
            int16_t draw_x;

            draw_x = (clamped_value >= 0) ? (int16_t)(center_x + offset_px) : (int16_t)(center_x - offset_px);
            draw_h = (gauge_cache != 0) ? gauge_cache->span_by_offset[offset_px] : inner_h;
            if ((draw_x >= x) && (draw_x < (x + w)))
            {
                u8g2_DrawVLine(u8g2, draw_x, y, (uint8_t)draw_h);
            }
        }
    }

    u8g2_DrawVLine(u8g2, center_x, y, (uint8_t)inner_h);

    for (i = 100; i < MOTOR_BREADCRUMB_ANGLE_MAX_DEG_X10; i += 100)
    {
        offset_px = (gauge_cache != 0) ? gauge_cache->tick_offset_px[i / 100] :
                                         motor_breadcrumb_scale_angle_to_half_span(i, half_span_px);
        draw_h = (gauge_cache != 0) ? gauge_cache->span_by_offset[offset_px] : inner_h;
        u8g2_DrawVLine(u8g2, (int16_t)(center_x - offset_px), y, (uint8_t)draw_h);
        u8g2_DrawVLine(u8g2, (int16_t)(center_x + offset_px), y, (uint8_t)draw_h);
    }

    for (i = 0; i <= half_span_px; i++)
    {
        int16_t outline_y;

        draw_h = (gauge_cache != 0) ? gauge_cache->span_by_offset[i] : inner_h;
        outline_y = (int16_t)(y + draw_h - 1);
        if ((center_x - i) >= x)
        {
            u8g2_DrawPixel(u8g2, (int16_t)(center_x - i), outline_y);
        }
        if (((center_x + i) < (x + w)) && (i != 0))
        {
            u8g2_DrawPixel(u8g2, (int16_t)(center_x + i), outline_y);
        }
    }

}

static void motor_breadcrumb_draw_left_bar(u8g2_t *u8g2,
                                           const ui_rect_t *bar_rect,
                                           const motor_state_t *state)
{
    int16_t zero_top;
    int16_t zero_bottom;
    int16_t top_span;
    int16_t bottom_span;
    int16_t i;
    int32_t inst;
    int32_t avg;

    zero_top = (int16_t)(bar_rect->y + ((bar_rect->h - 3) / 2));
    zero_bottom = (int16_t)(zero_top + 2);
    top_span = (int16_t)(zero_top - bar_rect->y);
    bottom_span = (int16_t)((bar_rect->y + bar_rect->h - 1) - zero_bottom);
    inst = motor_breadcrumb_clamp_i32(state->dyn.lon_accel_mg, -MOTOR_BREADCRUMB_LONG_MAX_MG, MOTOR_BREADCRUMB_LONG_MAX_MG);
    avg = motor_breadcrumb_clamp_i32(motor_breadcrumb_avg_long_mg(state), -MOTOR_BREADCRUMB_LONG_MAX_MG, MOTOR_BREADCRUMB_LONG_MAX_MG);

    for (i = 1; i <= 5; ++i)
    {
        int16_t up_y;
        int16_t down_y;

        up_y = (int16_t)(zero_top - lroundf((((float)i) / 5.0f) * (float)top_span));
        down_y = (int16_t)(zero_bottom + lroundf((((float)i) / 5.0f) * (float)bottom_span));
        u8g2_DrawHLine(u8g2, bar_rect->x, up_y, (i == 5) ? 14u : 8u);
        u8g2_DrawHLine(u8g2, bar_rect->x, down_y, (i == 5) ? 14u : 8u);
    }
    u8g2_DrawHLine(u8g2, bar_rect->x, zero_top, 14u);
    u8g2_DrawHLine(u8g2, bar_rect->x, (int16_t)(zero_top + 1), 14u);
    u8g2_DrawHLine(u8g2, bar_rect->x, zero_bottom, 14u);

    if (inst > 0)
    {
        int16_t fill;

        fill = (int16_t)lroundf((((float)inst) / 1000.0f) * (float)top_span);
        if (fill > 0)
        {
            u8g2_DrawBox(u8g2,
                         (int16_t)(bar_rect->x + 1),
                         (int16_t)(zero_top - fill + 1),
                         MOTOR_BREADCRUMB_SIDE_BAR_INST_W,
                         fill);
        }
    }
    else if (inst < 0)
    {
        int16_t fill;

        fill = (int16_t)lroundf((((float)(-inst)) / 1000.0f) * (float)bottom_span);
        if (fill > 0)
        {
            u8g2_DrawBox(u8g2,
                         (int16_t)(bar_rect->x + 1),
                         zero_bottom,
                         MOTOR_BREADCRUMB_SIDE_BAR_INST_W,
                         fill);
        }
    }

    if (avg > 0)
    {
        int16_t fill;

        fill = (int16_t)lroundf((((float)avg) / 1000.0f) * (float)top_span);
        if (fill > 0)
        {
            u8g2_DrawBox(u8g2,
                         (int16_t)(bar_rect->x + 10),
                         (int16_t)(zero_top - fill + 1),
                         MOTOR_BREADCRUMB_SIDE_BAR_AVG_W,
                         fill);
        }
    }
    else if (avg < 0)
    {
        int16_t fill;

        fill = (int16_t)lroundf((((float)(-avg)) / 1000.0f) * (float)bottom_span);
        if (fill > 0)
        {
            u8g2_DrawBox(u8g2,
                         (int16_t)(bar_rect->x + 10),
                         zero_bottom,
                         MOTOR_BREADCRUMB_SIDE_BAR_AVG_W,
                         fill);
        }
    }
}

static void motor_breadcrumb_draw_right_bar(u8g2_t *u8g2,
                                            const ui_rect_t *bar_rect,
                                            const motor_state_t *state)
{
    float minor_kmh;
    float major_kmh;
    float scale_top;
    float speed_kmh;
    float avg_kmh;
    uint16_t ticks;
    uint16_t i;

    motor_breadcrumb_speed_tick_steps(state, &minor_kmh, &major_kmh);
    scale_top = (float)(motor_breadcrumb_speed_scale_max_x10(state) / 10);
    speed_kmh = ((float)motor_breadcrumb_clamp_i32((int32_t)state->nav.speed_kmh_x10, 0, 4000)) * 0.1f;
    avg_kmh = ((float)motor_breadcrumb_avg_speed_x10(state)) * 0.1f;
    ticks = (uint16_t)lroundf(scale_top / minor_kmh);

    for (i = 1u; i <= ticks; ++i)
    {
        float tick_val;
        float ratio;
        float major_ratio;
        uint8_t tick_w;
        int16_t tick_y;

        tick_val = ((float)i) * minor_kmh;
        ratio = tick_val / scale_top;
        major_ratio = tick_val / major_kmh;
        tick_w = (fabsf(major_ratio - roundf(major_ratio)) < 0.001f) ? 14u : 8u;
        tick_y = (int16_t)(bar_rect->y + bar_rect->h - 1 - lroundf(ratio * (float)(bar_rect->h - 1)));
        u8g2_DrawHLine(u8g2, (int16_t)(bar_rect->x + 14 - tick_w), tick_y, tick_w);
    }

    if (speed_kmh > 0.0f)
    {
        int16_t fill;

        fill = (int16_t)lroundf((speed_kmh / scale_top) * (float)bar_rect->h);
        if (fill > 0)
        {
            u8g2_DrawBox(u8g2,
                         (int16_t)(bar_rect->x + 5),
                         (int16_t)(bar_rect->y + bar_rect->h - fill),
                         MOTOR_BREADCRUMB_SIDE_BAR_INST_W,
                         fill);
        }
    }

    if (avg_kmh > 0.0f)
    {
        int16_t cy;
        int16_t iy;

        cy = (int16_t)(bar_rect->y + bar_rect->h - 1 - lroundf((avg_kmh / scale_top) * (float)(bar_rect->h - 1)));
        iy = (int16_t)(cy - (MOTOR_UI_XBM_BREADCRUMB_SPEED_AVG_H / 2));
        u8g2_DrawXBMP(u8g2,
                      bar_rect->x,
                      iy,
                      MOTOR_UI_XBM_BREADCRUMB_SPEED_AVG_W,
                      MOTOR_UI_XBM_BREADCRUMB_SPEED_AVG_H,
                      motor_ui_xbm_breadcrumb_speed_avg_bits);
    }
}

static void motor_breadcrumb_draw_trail(u8g2_t *u8g2,
                                        const ui_rect_t *trail_rect,
                                        const motor_state_t *state)
{
    uint16_t i;
    int32_t org_lat_e7;
    int32_t org_lon_e7;
    float heading_deg;
    uint16_t scale_m;
    uint8_t heading_up;
    int32_t heading_deg_x10;

    if (motor_breadcrumb_get_origin(state, &org_lat_e7, &org_lon_e7) == 0u)
    {
        int16_t center_y;

        center_y = (int16_t)(trail_rect->y + ((trail_rect->h - motor_breadcrumb_font_h(u8g2, MOTOR_BREADCRUMB_FONT_MEDIUM)) / 2));
        motor_breadcrumb_draw_centered_text_top(u8g2,
                                                MOTOR_BREADCRUMB_FONT_MEDIUM,
                                                trail_rect,
                                                center_y,
                                                "TRACK EMPTY");
        return;
    }

    heading_deg = (state->nav.heading_valid != false) ? (((float)state->nav.heading_deg_x10) * 0.1f) : 0.0f;
    scale_m = Motor_UI_Breadcrumb_GetScaleMeters();
    heading_up = Motor_UI_Breadcrumb_IsHeadingUpMode();
    heading_deg_x10 = (state->nav.heading_valid != false) ? state->nav.heading_deg_x10 : 0;

    if (motor_breadcrumb_trail_cache_matches(trail_rect,
                                             state,
                                             org_lat_e7,
                                             org_lon_e7,
                                             scale_m,
                                             heading_up,
                                             heading_deg_x10) == 0u)
    {
        motor_breadcrumb_rebuild_trail_cache(trail_rect,
                                             state,
                                             org_lat_e7,
                                             org_lon_e7,
                                             scale_m,
                                             heading_up,
                                             heading_deg_x10);
    }

    u8g2_SetClipWindow(u8g2,
                       (u8g2_uint_t)trail_rect->x,
                       (u8g2_uint_t)trail_rect->y,
                       (u8g2_uint_t)(trail_rect->x + trail_rect->w),
                       (u8g2_uint_t)(trail_rect->y + trail_rect->h));

    for (i = 0u; i < s_motor_breadcrumb_trail_cache.point_count; ++i)
    {
        if (s_motor_breadcrumb_trail_cache.point_connected[i] != 0u)
        {
            u8g2_DrawLine(u8g2,
                          s_motor_breadcrumb_trail_cache.point_x[i - 1u],
                          s_motor_breadcrumb_trail_cache.point_y[i - 1u],
                          s_motor_breadcrumb_trail_cache.point_x[i],
                          s_motor_breadcrumb_trail_cache.point_y[i]);
        }
        else
        {
            u8g2_DrawPixel(u8g2,
                           s_motor_breadcrumb_trail_cache.point_x[i],
                           s_motor_breadcrumb_trail_cache.point_y[i]);
        }
    }

    if (s_motor_breadcrumb_trail_cache.home_projected != 0u)
    {
        u8g2_DrawLine(u8g2,
                      (int16_t)(s_motor_breadcrumb_trail_cache.home_x - 3),
                      s_motor_breadcrumb_trail_cache.home_y,
                      (int16_t)(s_motor_breadcrumb_trail_cache.home_x + 3),
                      s_motor_breadcrumb_trail_cache.home_y);
        u8g2_DrawLine(u8g2,
                      s_motor_breadcrumb_trail_cache.home_x,
                      (int16_t)(s_motor_breadcrumb_trail_cache.home_y - 3),
                      s_motor_breadcrumb_trail_cache.home_x,
                      (int16_t)(s_motor_breadcrumb_trail_cache.home_y + 3));
    }

    motor_breadcrumb_draw_arrow(u8g2,
                                s_motor_breadcrumb_trail_cache.center_x,
                                s_motor_breadcrumb_trail_cache.center_y,
                                (heading_up != 0u) ? 0.0f : heading_deg);

    u8g2_SetMaxClipWindow(u8g2);
}

static void motor_breadcrumb_draw_overlay_info(u8g2_t *u8g2,
                                               const ui_rect_t *viewport,
                                               const motor_state_t *state)
{
    const motor_breadcrumb_layout_cache_t *layout_cache;
    int16_t content_x;
    int16_t content_w;
    int16_t medium_h;
    int16_t large_h;
    int16_t small_h;
    int16_t line_y;
    int16_t medium_y;
    int16_t angle_y;
    int16_t speed_y;
    int16_t large_angle_w;
    int16_t medium_angle_w;
    char avg_buf[24];
    char max_buf[24];
    char ride_buf[24];
    char bank_buf[16];
    char left_max_buf[16];
    char right_max_buf[16];
    char speed_buf[16];
    int16_t angle_x;
    int16_t speed_x;
    int16_t left_x;
    int16_t right_x;
    int16_t info_y;
    int16_t col0_x;
    int16_t col1_x;
    int16_t col2_x;
    int16_t speed_unit_w;
    int16_t speed_unit_gap;
    int16_t speed_unit_x;
    int16_t speed_right_limit_x;

    layout_cache = motor_breadcrumb_get_layout_cache(u8g2);
    content_x = (int16_t)(viewport->x + MOTOR_BREADCRUMB_SIDE_BAR_W + 4);
    content_w = (int16_t)(viewport->w - (MOTOR_BREADCRUMB_SIDE_BAR_W * 2) - 8);
    medium_h = (layout_cache != 0) ? layout_cache->medium_h :
                                   motor_breadcrumb_font_h(u8g2, MOTOR_BREADCRUMB_FONT_MEDIUM);
    large_h = (layout_cache != 0) ? layout_cache->large_h :
                                  motor_breadcrumb_font_h(u8g2, MOTOR_BREADCRUMB_FONT_ANGLE_LARGE);
    small_h = (layout_cache != 0) ? layout_cache->small_h :
                                  motor_breadcrumb_font_h(u8g2, MOTOR_BREADCRUMB_FONT_SMALL);
    large_angle_w = (layout_cache != 0) ? layout_cache->large_angle_w :
                                          (int16_t)motor_breadcrumb_text_w(u8g2, MOTOR_BREADCRUMB_FONT_ANGLE_LARGE, "00");
    medium_angle_w = (layout_cache != 0) ? layout_cache->medium_angle_w :
                                           (int16_t)motor_breadcrumb_text_w(u8g2, MOTOR_BREADCRUMB_FONT_MEDIUM, "00");
    speed_unit_w = (layout_cache != 0) ? layout_cache->speed_unit_w :
                                         (int16_t)motor_breadcrumb_text_w(u8g2, MOTOR_BREADCRUMB_FONT_SMALL, "km/h");
    speed_unit_gap = 8;
    line_y = (int16_t)(viewport->y + viewport->h - medium_h - MOTOR_BREADCRUMB_BOTTOM_INFO_PAD_Y - 1);
    medium_y = (int16_t)(line_y + MOTOR_BREADCRUMB_BOTTOM_INFO_PAD_Y + 1);
    angle_y = (int16_t)(line_y - large_h - 1);
    speed_y = angle_y;

    u8g2_DrawHLine(u8g2, content_x, line_y, content_w);

    motor_breadcrumb_format_fixed_speed_000_0(avg_buf,
                                              sizeof(avg_buf),
                                              motor_breadcrumb_avg_speed_x10(state),
                                              &state->settings.units);
    motor_breadcrumb_format_fixed_speed_000_0(max_buf,
                                              sizeof(max_buf),
                                              (int32_t)state->record_session.max_speed_kmh_x10,
                                              &state->settings.units);
    motor_breadcrumb_format_hms(ride_buf, sizeof(ride_buf), state->record_session.ride_seconds);
    motor_breadcrumb_format_fixed_angle_2(bank_buf, sizeof(bank_buf), (int32_t)state->dyn.bank_deg_x10);
    motor_breadcrumb_format_fixed_angle_2(left_max_buf, sizeof(left_max_buf), state->dyn.max_left_bank_deg_x10);
    motor_breadcrumb_format_fixed_angle_2(right_max_buf, sizeof(right_max_buf), state->dyn.max_right_bank_deg_x10);
    (void)snprintf(speed_buf,
                   sizeof(speed_buf),
                   "%ld",
                   (long)(motor_breadcrumb_clamp_i32((int32_t)state->nav.speed_kmh_x10, 0, 4000) / 10));
    left_x = (int16_t)(content_x + 2);
    angle_x = (int16_t)(left_x + medium_angle_w + 2);
    right_x = (int16_t)(angle_x + large_angle_w + 2);

    motor_breadcrumb_text_top(u8g2, MOTOR_BREADCRUMB_FONT_MEDIUM, left_x, (int16_t)(angle_y + large_h - medium_h), left_max_buf);
    motor_breadcrumb_text_top(u8g2, MOTOR_BREADCRUMB_FONT_ANGLE_LARGE, angle_x, angle_y, bank_buf);
    motor_breadcrumb_text_top(u8g2, MOTOR_BREADCRUMB_FONT_MEDIUM, right_x, (int16_t)(angle_y + large_h - medium_h), right_max_buf);

    speed_unit_x = (int16_t)(content_x + content_w - speed_unit_w);
    speed_right_limit_x = (int16_t)(speed_unit_x - speed_unit_gap);
    speed_x = (int16_t)(speed_right_limit_x -
                        (int16_t)motor_breadcrumb_text_w(u8g2, MOTOR_BREADCRUMB_FONT_SPEED_LARGE, speed_buf));
    motor_breadcrumb_text_top(u8g2,
                              MOTOR_BREADCRUMB_FONT_SPEED_LARGE,
                              speed_x,
                              speed_y,
                              speed_buf);
    motor_breadcrumb_text_top(u8g2,
                              MOTOR_BREADCRUMB_FONT_SMALL,
                              speed_unit_x,
                              (int16_t)(speed_y + large_h - small_h - 1),
                              "km/h");

    info_y = medium_y;
    col0_x = content_x;
    col1_x = (int16_t)(content_x + (content_w / 3));
    col2_x = (int16_t)(content_x + ((content_w * 2) / 3));
    motor_breadcrumb_draw_info_with_icon(u8g2, col0_x, info_y, motor_ui_xbm_info_avg_bits, avg_buf);
    motor_breadcrumb_draw_info_with_icon(u8g2, col1_x, info_y, motor_ui_xbm_info_max_bits, max_buf);
    motor_breadcrumb_draw_info_with_icon(u8g2, col2_x, info_y, motor_ui_xbm_info_time_bits, ride_buf);
}

void Motor_UI_DrawScreen_Breadcrumb(u8g2_t *u8g2,
                                    const ui_rect_t *viewport,
                                    const motor_state_t *state)
{
    ui_rect_t bar_rect;
    ui_rect_t trail_rect;

    if ((u8g2 == 0) || (viewport == 0) || (state == 0))
    {
        return;
    }

    motor_breadcrumb_init();
    bar_rect.x = viewport->x;
    bar_rect.y = viewport->y;
    bar_rect.w = MOTOR_BREADCRUMB_SIDE_BAR_W;
    bar_rect.h = viewport->h;
    trail_rect.x = (int16_t)(viewport->x + MOTOR_BREADCRUMB_SIDE_BAR_W);
    trail_rect.y = (int16_t)(viewport->y + MOTOR_BREADCRUMB_TOP_GAUGE_H + MOTOR_BREADCRUMB_TOP_GAP_Y);
    trail_rect.w = (int16_t)(viewport->w - (MOTOR_BREADCRUMB_SIDE_BAR_W * 2));
    trail_rect.h = (int16_t)(viewport->h - (trail_rect.y - viewport->y));

    motor_breadcrumb_draw_top_angle(u8g2, viewport, state);
    motor_breadcrumb_draw_trail(u8g2, &trail_rect, state);
    motor_breadcrumb_draw_left_bar(u8g2, &bar_rect, state);
    bar_rect.x = (int16_t)(viewport->x + viewport->w - MOTOR_BREADCRUMB_SIDE_BAR_W);
    motor_breadcrumb_draw_right_bar(u8g2, &bar_rect, state);
    motor_breadcrumb_draw_overlay_info(u8g2, viewport, state);
}
