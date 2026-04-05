#include "Motor_UI_Internal.h"
#include "Motor_UI_Xbm.h"
#include "Motor_DataField.h"
#include "Motor_Units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MOTOR_MAIN_FONT_SMALL
#define MOTOR_MAIN_FONT_SMALL             u8g2_font_5x7_tf
#endif

#ifndef MOTOR_MAIN_FONT_MEDIUM
#define MOTOR_MAIN_FONT_MEDIUM            u8g2_font_7x14B_tf
#endif

#ifndef MOTOR_MAIN_FONT_LARGE
#define MOTOR_MAIN_FONT_LARGE             u8g2_font_logisoso24_tn
#endif

#ifndef MOTOR_MAIN_FONT_SPEED_LARGE
#define MOTOR_MAIN_FONT_SPEED_LARGE       MOTOR_MAIN_FONT_ANGLE_LARGE
#endif

#ifndef MOTOR_MAIN_FONT_ANGLE_LARGE
#define MOTOR_MAIN_FONT_ANGLE_LARGE       u8g2_font_logisoso26_tn
#endif

#ifndef MOTOR_MAIN_TOP_GAUGE_H
#define MOTOR_MAIN_TOP_GAUGE_H            18
#endif

#ifndef MOTOR_MAIN_BOTTOM_RIBBON_H
#define MOTOR_MAIN_BOTTOM_RIBBON_H        26
#endif

#ifndef MOTOR_MAIN_TOP_SECTION_OFFSET_Y
#define MOTOR_MAIN_TOP_SECTION_OFFSET_Y    1
#endif

#ifndef MOTOR_MAIN_TOP_MIDDLE_GAP_Y
#define MOTOR_MAIN_TOP_MIDDLE_GAP_Y        3
#endif

#ifndef MOTOR_MAIN_MIDDLE_BOTTOM_GAP_Y
#define MOTOR_MAIN_MIDDLE_BOTTOM_GAP_Y     5
#endif

#ifndef MOTOR_MAIN_OUTER_PAD_X
#define MOTOR_MAIN_OUTER_PAD_X             4
#endif

#ifndef MOTOR_MAIN_INNER_GAP_X
#define MOTOR_MAIN_INNER_GAP_X             6
#endif

#ifndef MOTOR_MAIN_TOP_GAUGE_MAJOR_TICK_H
#define MOTOR_MAIN_TOP_GAUGE_MAJOR_TICK_H 14
#endif

#ifndef MOTOR_MAIN_TOP_GAUGE_MINOR_TICK_H
#define MOTOR_MAIN_TOP_GAUGE_MINOR_TICK_H  9
#endif

#ifndef MOTOR_MAIN_TOP_GAUGE_BAR_TOP_DY
#define MOTOR_MAIN_TOP_GAUGE_BAR_TOP_DY    1
#endif

#ifndef MOTOR_MAIN_TOP_GAUGE_BAR_H
#define MOTOR_MAIN_TOP_GAUGE_BAR_H         8
#endif

#ifndef MOTOR_MAIN_LEFT_SCALE_H
#define MOTOR_MAIN_LEFT_SCALE_H           14
#endif

#ifndef MOTOR_MAIN_LEFT_BOTTOM_GAUGE_H
#define MOTOR_MAIN_LEFT_BOTTOM_GAUGE_H    12
#endif

#ifndef MOTOR_MAIN_SIDE_BAR_TOP_W
#define MOTOR_MAIN_SIDE_BAR_TOP_W          3
#endif

#ifndef MOTOR_MAIN_SIDE_BAR_BOTTOM_W
#define MOTOR_MAIN_SIDE_BAR_BOTTOM_W       8
#endif

#ifndef MOTOR_MAIN_SIDE_BAR_TOP_GAP_Y
#define MOTOR_MAIN_SIDE_BAR_TOP_GAP_Y     14
#endif

#ifndef MOTOR_MAIN_SIDE_BAR_BOTTOM_GAP_Y
#define MOTOR_MAIN_SIDE_BAR_BOTTOM_GAP_Y   2
#endif

#ifndef MOTOR_MAIN_SIDE_BAR_INSET_X
#define MOTOR_MAIN_SIDE_BAR_INSET_X        2
#endif

#ifndef MOTOR_MAIN_SIDE_TICK_STEP_Y
#define MOTOR_MAIN_SIDE_TICK_STEP_Y        2
#endif

#ifndef MOTOR_MAIN_LONG_G_TOP_W
#define MOTOR_MAIN_LONG_G_TOP_W            4
#endif

#ifndef MOTOR_MAIN_LONG_G_BOTTOM_W
#define MOTOR_MAIN_LONG_G_BOTTOM_W        11
#endif

#ifndef MOTOR_MAIN_LAT_GAUGE_SIDE_INSET
#define MOTOR_MAIN_LAT_GAUGE_SIDE_INSET    3
#endif

#ifndef MOTOR_MAIN_ANGLE_MAX_DEG_X10
#define MOTOR_MAIN_ANGLE_MAX_DEG_X10     600
#endif

#ifndef MOTOR_MAIN_ANGLE_FINE1_MAX_DEG_X10
#define MOTOR_MAIN_ANGLE_FINE1_MAX_DEG_X10 350
#endif

#ifndef MOTOR_MAIN_ANGLE_FINE2_MAX_DEG_X10
#define MOTOR_MAIN_ANGLE_FINE2_MAX_DEG_X10 500
#endif

#ifndef MOTOR_MAIN_ANGLE_TICK_STEP_X10
#define MOTOR_MAIN_ANGLE_TICK_STEP_X10    100
#endif

#ifndef MOTOR_MAIN_ANGLE_MAJOR_STEP_X10
#define MOTOR_MAIN_ANGLE_MAJOR_STEP_X10  150
#endif

#ifndef MOTOR_MAIN_ANGLE_MINOR_STEP_X10
#define MOTOR_MAIN_ANGLE_MINOR_STEP_X10   75
#endif

#ifndef MOTOR_MAIN_ANGLE_GAUGE_MAJOR_TICK_H
#define MOTOR_MAIN_ANGLE_GAUGE_MAJOR_TICK_H 12
#endif

#ifndef MOTOR_MAIN_ANGLE_GAUGE_MINOR_TICK_H
#define MOTOR_MAIN_ANGLE_GAUGE_MINOR_TICK_H 7
#endif

#ifndef MOTOR_MAIN_ANGLE_GAUGE_BAR_H
#define MOTOR_MAIN_ANGLE_GAUGE_BAR_H       7
#endif

#ifndef MOTOR_MAIN_ANGLE_GAUGE_CENTER_TICK_W
#define MOTOR_MAIN_ANGLE_GAUGE_CENTER_TICK_W 3
#endif

#ifndef MOTOR_MAIN_LAT_G_MAX_MG
#define MOTOR_MAIN_LAT_G_MAX_MG         1000
#endif

#ifndef MOTOR_MAIN_LAT_G_MAJOR_STEP_MG
#define MOTOR_MAIN_LAT_G_MAJOR_STEP_MG   300
#endif

#ifndef MOTOR_MAIN_LAT_G_MINOR_STEP_MG
#define MOTOR_MAIN_LAT_G_MINOR_STEP_MG   300
#endif

#ifndef MOTOR_MAIN_LONG_G_TICK_STEP_MG
#define MOTOR_MAIN_LONG_G_TICK_STEP_MG    200
#endif

#ifndef MOTOR_MAIN_LONG_G_MAX_MG
#define MOTOR_MAIN_LONG_G_MAX_MG         1000
#endif

#ifndef MOTOR_MAIN_GRADE_GAUGE_W
#define MOTOR_MAIN_GRADE_GAUGE_W          14
#endif

#ifndef MOTOR_MAIN_GRADE_GAUGE_GAP_X
#define MOTOR_MAIN_GRADE_GAUGE_GAP_X       4
#endif

#ifndef MOTOR_MAIN_GRADE_MAX_X10
#define MOTOR_MAIN_GRADE_MAX_X10         150
#endif

#ifndef MOTOR_MAIN_GRADE_MAJOR_STEP_X10
#define MOTOR_MAIN_GRADE_MAJOR_STEP_X10   40
#endif

#ifndef MOTOR_MAIN_GRADE_MINOR_STEP_X10
#define MOTOR_MAIN_GRADE_MINOR_STEP_X10    0
#endif

#ifndef MOTOR_MAIN_VERTICAL_GAUGE_MAJOR_TICK_W
#define MOTOR_MAIN_VERTICAL_GAUGE_MAJOR_TICK_W 12
#endif

#ifndef MOTOR_MAIN_VERTICAL_GAUGE_MINOR_TICK_W
#define MOTOR_MAIN_VERTICAL_GAUGE_MINOR_TICK_W 7
#endif

#ifndef MOTOR_MAIN_VERTICAL_GAUGE_BAR_W
#define MOTOR_MAIN_VERTICAL_GAUGE_BAR_W   7
#endif

#ifndef MOTOR_MAIN_VERTICAL_GAUGE_CENTER_TICK_H
#define MOTOR_MAIN_VERTICAL_GAUGE_CENTER_TICK_H 3
#endif

#ifndef MOTOR_MAIN_GAUGE_MAJOR_TICK_H
#define MOTOR_MAIN_GAUGE_MAJOR_TICK_H     10
#endif

#ifndef MOTOR_MAIN_GAUGE_MINOR_TICK_H
#define MOTOR_MAIN_GAUGE_MINOR_TICK_H      5
#endif

#ifndef MOTOR_MAIN_GAUGE_CENTER_TICK_W
#define MOTOR_MAIN_GAUGE_CENTER_TICK_W     3
#endif

#ifndef MOTOR_MAIN_LAT_GAUGE_MAJOR_TICK_H
#define MOTOR_MAIN_LAT_GAUGE_MAJOR_TICK_H 11
#endif

#ifndef MOTOR_MAIN_LAT_GAUGE_MINOR_TICK_H
#define MOTOR_MAIN_LAT_GAUGE_MINOR_TICK_H 11
#endif

#ifndef MOTOR_MAIN_LAT_GAUGE_BAR_H
#define MOTOR_MAIN_LAT_GAUGE_BAR_H         6
#endif

#ifndef MOTOR_MAIN_LAT_GAUGE_CENTER_TICK_W
#define MOTOR_MAIN_LAT_GAUGE_CENTER_TICK_W 3
#endif

#ifndef MOTOR_MAIN_VALUE_UNIT_GAP_X
#define MOTOR_MAIN_VALUE_UNIT_GAP_X        3
#endif

#ifndef MOTOR_MAIN_USER_CELL_COUNT
#define MOTOR_MAIN_USER_CELL_COUNT         5u
#endif

typedef enum
{
    MOTOR_MAIN_BOTTOM_TRIP_A = 0u,
    MOTOR_MAIN_BOTTOM_TRIP_B,
    MOTOR_MAIN_BOTTOM_CURRENT_RECORD,
    MOTOR_MAIN_BOTTOM_REFUEL,
    MOTOR_MAIN_BOTTOM_TODAY,
    MOTOR_MAIN_BOTTOM_IGNITION,
    MOTOR_MAIN_BOTTOM_USER_1,
    MOTOR_MAIN_BOTTOM_USER_2,
    MOTOR_MAIN_BOTTOM_USER_3,
    MOTOR_MAIN_BOTTOM_COUNT
} motor_main_bottom_mode_t;

typedef struct
{
    int16_t brake_edge_x;
    int16_t accel_edge_x;
    int16_t content_x;
    int16_t content_w;
} motor_main_left_geometry_t;

#define MOTOR_MAIN_SPAN_CACHE_MAX_HALF_SPAN   128
#define MOTOR_MAIN_TRAPEZOID_CACHE_MAX_H      128
#define MOTOR_MAIN_VERTICAL_CACHE_MAX_H       128

typedef struct
{
    uint8_t init;
    int16_t half_span_px;
    int16_t inner_span;
    int16_t outer_span;
    int16_t span_by_offset[MOTOR_MAIN_SPAN_CACHE_MAX_HALF_SPAN + 1];
    int16_t angle_tick_offset[(MOTOR_MAIN_ANGLE_MAX_DEG_X10 / MOTOR_MAIN_ANGLE_TICK_STEP_X10) + 1];
} motor_main_span_cache_t;

typedef struct
{
    uint8_t init;
    int16_t h;
    int16_t top_w;
    int16_t bottom_w;
    int16_t row_width[MOTOR_MAIN_TRAPEZOID_CACHE_MAX_H];
} motor_main_trapezoid_cache_t;

typedef struct
{
    uint8_t init;
    int16_t h;
    int16_t half_span_px;
    int16_t inner_w;
    int16_t outer_w;
    int16_t row_width[MOTOR_MAIN_VERTICAL_CACHE_MAX_H];
} motor_main_vertical_cache_t;

typedef struct
{
    uint8_t init;
    int16_t small_h;
    int16_t medium_h;
    int16_t angle_large_h;
    int16_t speed_large_h;
    int16_t speed_unit_w;
    int16_t percent_w;
    int16_t angle_slot_w;
    int16_t angle_medium_slot_w;
    int16_t altitude_slot_w;
    int16_t grade_slot_w;
} motor_main_text_cache_t;

static motor_main_span_cache_t s_motor_main_angle_cache;
static motor_main_span_cache_t s_motor_main_bottom_gauge_cache;
static motor_main_trapezoid_cache_t s_motor_main_trapezoid_cache;
static motor_main_vertical_cache_t s_motor_main_vertical_cache;
static motor_main_text_cache_t s_motor_main_text_cache;

uint8_t Motor_UI_Main_GetBottomPageCount(void)
{
    return (uint8_t)MOTOR_MAIN_BOTTOM_COUNT;
}

const char *Motor_UI_Main_GetBottomPageName(uint8_t page_index)
{
    switch ((motor_main_bottom_mode_t)page_index)
    {
    case MOTOR_MAIN_BOTTOM_TRIP_A:      return "TRIP A";
    case MOTOR_MAIN_BOTTOM_TRIP_B:      return "TRIP B";
    case MOTOR_MAIN_BOTTOM_CURRENT_RECORD:return "TRIP REC";
    case MOTOR_MAIN_BOTTOM_REFUEL:      return "TRIP REFUEL";
    case MOTOR_MAIN_BOTTOM_TODAY:       return "TRIP TODAY";
    case MOTOR_MAIN_BOTTOM_IGNITION:    return "TRIP IGNITION";
    case MOTOR_MAIN_BOTTOM_USER_1:      return "PAGE 1";
    case MOTOR_MAIN_BOTTOM_USER_2:      return "PAGE 2";
    case MOTOR_MAIN_BOTTOM_USER_3:      return "PAGE 3";
    default:                            return "PAGE";
    }
}

uint8_t Motor_UI_Main_ResetBottomPage(motor_state_t *state)
{
    if (state == 0)
    {
        return 0u;
    }

    switch ((motor_main_bottom_mode_t)state->ui.bottom_page)
    {
    case MOTOR_MAIN_BOTTOM_TRIP_A:
        memset(&state->session.trip_a_stats, 0, sizeof(state->session.trip_a_stats));
        state->session.trip_a_m = 0u;
        return 1u;
    case MOTOR_MAIN_BOTTOM_TRIP_B:
        memset(&state->session.trip_b_stats, 0, sizeof(state->session.trip_b_stats));
        state->session.trip_b_m = 0u;
        return 1u;
    case MOTOR_MAIN_BOTTOM_REFUEL:
        memset(&state->session.trip_refuel, 0, sizeof(state->session.trip_refuel));
        return 1u;
    case MOTOR_MAIN_BOTTOM_TODAY:
        memset(&state->session.trip_today, 0, sizeof(state->session.trip_today));
        state->session.today_anchor_year = state->snapshot.clock.local.year;
        state->session.today_anchor_month = state->snapshot.clock.local.month;
        state->session.today_anchor_day = state->snapshot.clock.local.day;
        state->session.today_reset_pending = 0u;
        return 1u;
    case MOTOR_MAIN_BOTTOM_IGNITION:
        memset(&state->session.trip_ignition, 0, sizeof(state->session.trip_ignition));
        return 1u;
    default:
        break;
    }

    return 0u;
}

static int32_t motor_main_abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
}

static int16_t motor_main_abs_i16(int16_t value)
{
    return (value < 0) ? (int16_t)-value : value;
}

static int32_t motor_main_clamp_i32(int32_t value, int32_t lo, int32_t hi)
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

static int16_t motor_main_scale_to_span(int32_t value, int32_t max_value, int16_t span_px)
{
    int32_t scaled;

    if ((value <= 0) || (max_value <= 0) || (span_px <= 0))
    {
        return 0;
    }

    value = motor_main_clamp_i32(value, 0, max_value);
    scaled = ((value * (int32_t)span_px) + (max_value / 2)) / max_value;
    if ((scaled == 0) && (value > 0))
    {
        scaled = 1;
    }
    if (scaled > (int32_t)span_px)
    {
        scaled = span_px;
    }

    return (int16_t)scaled;
}

static uint16_t motor_main_measure_text(u8g2_t *u8g2, const uint8_t *font, const char *text)
{
    if ((u8g2 == 0) || (font == 0) || (text == 0))
    {
        return 0u;
    }

    u8g2_SetFont(u8g2, font);
    return u8g2_GetStrWidth(u8g2, text);
}

static int16_t motor_main_get_font_height(u8g2_t *u8g2, const uint8_t *font)
{
    if ((u8g2 == 0) || (font == 0))
    {
        return 0;
    }

    u8g2_SetFont(u8g2, font);
    return (int16_t)(u8g2_GetAscent(u8g2) - u8g2_GetDescent(u8g2));
}

static int16_t motor_main_center_text_x(u8g2_t *u8g2,
                                        const uint8_t *font,
                                        int16_t x,
                                        int16_t w,
                                        const char *text)
{
    uint16_t text_w;

    if ((u8g2 == 0) || (font == 0) || (text == 0))
    {
        return x;
    }

    text_w = motor_main_measure_text(u8g2, font, text);
    if (text_w >= (uint16_t)w)
    {
        return x;
    }

    return (int16_t)(x + ((w - (int16_t)text_w) / 2));
}

static int16_t motor_main_right_text_x(u8g2_t *u8g2,
                                       const uint8_t *font,
                                       int16_t right_x,
                                       const char *text)
{
    uint16_t text_w;

    if ((u8g2 == 0) || (font == 0) || (text == 0))
    {
        return right_x;
    }

    text_w = motor_main_measure_text(u8g2, font, text);
    return (int16_t)(right_x - (int16_t)text_w);
}

static void motor_main_format_seconds(char *out_text, size_t out_size, uint32_t seconds)
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


static void motor_main_copy_fit(u8g2_t *u8g2,
                                const char *src,
                                char *dst,
                                size_t dst_size,
                                int16_t max_width_px)
{
    size_t src_len;
    size_t lo;
    size_t hi;
    size_t best_len;

    if ((u8g2 == 0) || (dst == 0) || (dst_size == 0u))
    {
        return;
    }

    if (src == 0)
    {
        dst[0] = '\0';
        return;
    }

    (void)strncpy(dst, src, dst_size - 1u);
    dst[dst_size - 1u] = '\0';

    if (u8g2_GetStrWidth(u8g2, dst) <= (uint16_t)max_width_px)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  Fit-to-width trimming by binary search                                */
    /*                                                                        */
    /*  The bottom trip/user cells call this helper frequently, and the old   */
    /*  implementation removed one character at a time while re-measuring the */
    /*  full string on every iteration. That creates avoidable O(n^2)-style   */
    /*  width probing on a page that already draws many text cells.           */
    /*                                                                        */
    /*  The new version finds the longest fitting prefix with logarithmic     */
    /*  width probes, preserving behavior while cutting repeated font-width    */
    /*  work substantially.                                                   */
    /* ---------------------------------------------------------------------- */
    src_len = strlen(dst);
    lo = 0u;
    hi = src_len;
    best_len = 0u;

    while (lo <= hi)
    {
        size_t mid;

        mid = lo + ((hi - lo) / 2u);
        dst[mid] = '\0';

        if (u8g2_GetStrWidth(u8g2, dst) <= (uint16_t)max_width_px)
        {
            best_len = mid;
            lo = mid + 1u;
        }
        else
        {
            if (mid == 0u)
            {
                break;
            }
            hi = mid - 1u;
        }
    }

    (void)strncpy(dst, src, dst_size - 1u);
    dst[best_len] = '\0';
}


static void motor_main_draw_text_top(u8g2_t *u8g2,
                                     const uint8_t *font,
                                     int16_t x,
                                     int16_t y,
                                     const char *text)
{
    if ((u8g2 == 0) || (font == 0) || (text == 0))
    {
        return;
    }

    u8g2_SetFontPosTop(u8g2);
    u8g2_SetFont(u8g2, font);
    u8g2_DrawStr(u8g2, x, y, text);
    u8g2_SetFontPosBaseline(u8g2);
}

static uint8_t motor_main_left_is_negative(const motor_state_t *state)
{
    if (state == 0)
    {
        return 1u;
    }

    if ((state->dyn.max_left_bank_deg_x10 < 0) && (state->dyn.max_right_bank_deg_x10 >= 0))
    {
        return 1u;
    }

    if ((state->dyn.max_left_bank_deg_x10 > 0) && (state->dyn.max_right_bank_deg_x10 <= 0))
    {
        return 0u;
    }

    return 1u;
}

static int32_t motor_main_get_display_bank_x10(const motor_state_t *state)
{
    if (state == 0)
    {
        return 0;
    }

    return (motor_main_left_is_negative(state) != 0u) ? (int32_t)state->dyn.bank_deg_x10
                                                       : -(int32_t)state->dyn.bank_deg_x10;
}

static int32_t motor_main_get_display_lat_mg(const motor_state_t *state)
{
    if (state == 0)
    {
        return 0;
    }

    return (motor_main_left_is_negative(state) != 0u) ? state->dyn.lat_accel_mg
                                                       : -state->dyn.lat_accel_mg;
}

static motor_main_top_mode_t motor_main_get_top_mode(const motor_state_t *state)
{
    if (state == 0)
    {
        return MOTOR_MAIN_TOP_MODE_SPEED;
    }

    switch ((motor_main_top_mode_t)state->settings.display.main_top_mode)
    {
    case MOTOR_MAIN_TOP_MODE_RPM:
        return MOTOR_MAIN_TOP_MODE_RPM;
    case MOTOR_MAIN_TOP_MODE_SPEED:
    default:
        return MOTOR_MAIN_TOP_MODE_SPEED;
    }
}

static int32_t motor_main_get_speed_scale_max_x10(const motor_state_t *state)
{
    if (state == 0)
    {
        return 2000;
    }

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

static int32_t motor_main_get_rpm_scale_max(const motor_state_t *state)
{
    if (state == 0)
    {
        return 14000;
    }

    switch ((motor_main_rpm_scale_t)state->settings.display.main_rpm_scale)
    {
    case MOTOR_MAIN_RPM_SCALE_6K:  return 6000;
    case MOTOR_MAIN_RPM_SCALE_8K:  return 8000;
    case MOTOR_MAIN_RPM_SCALE_10K: return 10000;
    case MOTOR_MAIN_RPM_SCALE_12K: return 12000;
    case MOTOR_MAIN_RPM_SCALE_16K: return 16000;
    case MOTOR_MAIN_RPM_SCALE_14K:
    default:                       return 14000;
    }
}

static int32_t motor_main_get_g_scale_max_mg(const motor_state_t *state)
{
    if (state == 0)
    {
        return 1000;
    }

    switch ((motor_main_g_scale_t)state->settings.display.main_g_scale)
    {
    case MOTOR_MAIN_G_SCALE_0P5:
        return 500;
    case MOTOR_MAIN_G_SCALE_1P5:
        return 1500;
    case MOTOR_MAIN_G_SCALE_1P0:
    default:
        return 1000;
    }
}

static void motor_main_get_lat_ticks_mg(int32_t max_mg,
                                        int32_t *out_major_step,
                                        int32_t *out_minor_step)
{
    if ((out_major_step == 0) || (out_minor_step == 0))
    {
        return;
    }

    (void)max_mg;
    *out_major_step = 300;
    *out_minor_step = 300;
}

static int32_t motor_main_get_top_gauge_value(const motor_state_t *state,
                                              motor_main_top_mode_t mode)
{
    if (state == 0)
    {
        return 0;
    }

    if (mode == MOTOR_MAIN_TOP_MODE_RPM)
    {
        return (state->vehicle.rpm_valid != false) ? (int32_t)state->vehicle.rpm : 0;
    }

    return (int32_t)state->nav.speed_kmh_x10;
}

static int32_t motor_main_get_top_gauge_max(const motor_state_t *state,
                                            motor_main_top_mode_t mode)
{
    if (mode == MOTOR_MAIN_TOP_MODE_RPM)
    {
        return motor_main_get_rpm_scale_max(state);
    }

    return motor_main_get_speed_scale_max_x10(state);
}

static void motor_main_get_top_tick_steps(const motor_state_t *state,
                                         motor_main_top_mode_t mode,
                                         int32_t *out_major_step,
                                         int32_t *out_minor_step)
{
    if ((out_major_step == 0) || (out_minor_step == 0))
    {
        return;
    }

    if (mode == MOTOR_MAIN_TOP_MODE_RPM)
    {
        *out_major_step = 2000;
        *out_minor_step = 1000;
        return;
    }

    if (state == 0)
    {
        *out_major_step = 200;
        *out_minor_step = 100;
        return;
    }

    switch ((motor_main_speed_scale_t)state->settings.display.main_speed_scale)
    {
    case MOTOR_MAIN_SPEED_SCALE_100:
        *out_major_step = 200;
        *out_minor_step = 100;
        break;
    case MOTOR_MAIN_SPEED_SCALE_300:
        *out_major_step = 500;
        *out_minor_step = 250;
        break;
    case MOTOR_MAIN_SPEED_SCALE_200:
    default:
        *out_major_step = 200;
        *out_minor_step = 100;
        break;
    }
}

static int32_t motor_main_get_average_speed_kmh_x10(const motor_state_t *state)
{
    uint64_t scaled_distance;
    uint32_t seconds;

    if (state == 0)
    {
        return 0;
    }

    seconds = state->session.moving_seconds;
    if (seconds == 0u)
    {
        seconds = state->session.ride_seconds;
    }
    if (seconds == 0u)
    {
        return 0;
    }

    scaled_distance = ((uint64_t)state->session.distance_m * 36ull) + ((uint64_t)seconds / 2ull);
    return (int32_t)(scaled_distance / (uint64_t)seconds);
}

static int32_t motor_main_get_average_speed_from_trip(uint32_t distance_m,
                                                      uint32_t moving_seconds,
                                                      uint32_t ride_seconds)
{
    uint64_t scaled_distance;
    uint32_t seconds;

    seconds = moving_seconds;
    if (seconds == 0u)
    {
        seconds = ride_seconds;
    }
    if (seconds == 0u)
    {
        return 0;
    }

    scaled_distance = ((uint64_t)distance_m * 36ull) + ((uint64_t)seconds / 2ull);
    return (int32_t)(scaled_distance / (uint64_t)seconds);
}

static void motor_main_get_top_speed_scale_labels(const motor_state_t *state,
                                                  char *out_mid,
                                                  size_t out_mid_size,
                                                  char *out_max,
                                                  size_t out_max_size)
{
    int32_t max_speed_x10;

    if ((out_mid == 0) || (out_mid_size == 0u) || (out_max == 0) || (out_max_size == 0u))
    {
        return;
    }

    max_speed_x10 = motor_main_get_speed_scale_max_x10(state);
    (void)snprintf(out_mid, out_mid_size, "%ld", (long)(max_speed_x10 / 20));
    (void)snprintf(out_max, out_max_size, "%ld", (long)(max_speed_x10 / 10));
}

static void motor_main_draw_top_speed_labels_and_markers(u8g2_t *u8g2,
                                                         int16_t x,
                                                         int16_t y,
                                                         int16_t w,
                                                         int16_t h,
                                                         const motor_state_t *state)
{
    int16_t minor_h;
    int16_t small_h;
    int16_t center_tick_x;
    int16_t max_tick_x;
    int16_t label_y;
    int16_t label_mid_x;
    int16_t label_max_x;
    int16_t icon_y;
    int16_t avg_icon_x;
    int16_t max_icon_x;
    int16_t half_icon_w;
    int32_t scale_max_x10;
    int32_t avg_speed_x10;
    int32_t max_speed_x10;
    char mid_buf[8];
    char max_buf[8];

    if ((u8g2 == 0) || (state == 0) || (w < 12) || (h <= 0))
    {
        return;
    }

    scale_max_x10 = motor_main_get_speed_scale_max_x10(state);
    if (scale_max_x10 <= 0)
    {
        return;
    }

    minor_h = (h < MOTOR_MAIN_TOP_GAUGE_MINOR_TICK_H) ? h : MOTOR_MAIN_TOP_GAUGE_MINOR_TICK_H;
    if (minor_h < 1)
    {
        minor_h = 1;
    }
    small_h = motor_main_get_font_height(u8g2, MOTOR_MAIN_FONT_SMALL);
    center_tick_x = (int16_t)(x + motor_main_scale_to_span(scale_max_x10 / 2, scale_max_x10, (int16_t)(w - 1)));
    max_tick_x = (int16_t)(x + w - 1);
    label_y = (int16_t)(y + h - small_h + 2);
    icon_y = (int16_t)(y + minor_h + 1);
    half_icon_w = (int16_t)(MOTOR_UI_XBM_SPEED_AVG_MARK_W / 2);

    motor_main_get_top_speed_scale_labels(state, mid_buf, sizeof(mid_buf), max_buf, sizeof(max_buf));
    label_mid_x = motor_main_center_text_x(u8g2,
                                           MOTOR_MAIN_FONT_SMALL,
                                           (int16_t)(center_tick_x - 10),
                                           21,
                                           mid_buf);
    label_max_x = motor_main_right_text_x(u8g2,
                                          MOTOR_MAIN_FONT_SMALL,
                                          (int16_t)(x + w - 2),
                                          max_buf);
    motor_main_draw_text_top(u8g2,
                             MOTOR_MAIN_FONT_SMALL,
                             label_mid_x,
                             label_y,
                             mid_buf);
    motor_main_draw_text_top(u8g2,
                             MOTOR_MAIN_FONT_SMALL,
                             label_max_x,
                             label_y,
                             max_buf);

    avg_speed_x10 = motor_main_clamp_i32(motor_main_get_average_speed_kmh_x10(state), 0, scale_max_x10);
    max_speed_x10 = motor_main_clamp_i32((int32_t)state->session.max_speed_kmh_x10, 0, scale_max_x10);

    avg_icon_x = (int16_t)(x + motor_main_scale_to_span(avg_speed_x10, scale_max_x10, (int16_t)(w - 1)) - half_icon_w);
    max_icon_x = (int16_t)(x + motor_main_scale_to_span(max_speed_x10, scale_max_x10, (int16_t)(w - 1)) - half_icon_w);
    if (avg_icon_x < x)
    {
        avg_icon_x = x;
    }
    if ((avg_icon_x + MOTOR_UI_XBM_SPEED_AVG_MARK_W) > (x + w))
    {
        avg_icon_x = (int16_t)((x + w) - MOTOR_UI_XBM_SPEED_AVG_MARK_W);
    }
    if (max_icon_x < x)
    {
        max_icon_x = x;
    }
    if ((max_icon_x + MOTOR_UI_XBM_SPEED_MAX_MARK_W) > (x + w))
    {
        max_icon_x = (int16_t)((x + w) - MOTOR_UI_XBM_SPEED_MAX_MARK_W);
    }

    if ((icon_y + MOTOR_UI_XBM_SPEED_AVG_MARK_H) <= (y + h))
    {
        u8g2_DrawXBM(u8g2,
                     (u8g2_uint_t)avg_icon_x,
                     (u8g2_uint_t)icon_y,
                     MOTOR_UI_XBM_SPEED_AVG_MARK_W,
                     MOTOR_UI_XBM_SPEED_AVG_MARK_H,
                     motor_ui_xbm_speed_avg_mark_bits);
        u8g2_DrawXBM(u8g2,
                     (u8g2_uint_t)max_icon_x,
                     (u8g2_uint_t)icon_y,
                     MOTOR_UI_XBM_SPEED_MAX_MARK_W,
                     MOTOR_UI_XBM_SPEED_MAX_MARK_H,
                     motor_ui_xbm_speed_max_mark_bits);
    }
}

static void motor_main_format_signed_x10(char *out_text, size_t out_size, int32_t value_x10)
{
    int32_t abs_value;
    char sign;

    if ((out_text == 0) || (out_size == 0u))
    {
        return;
    }

    sign = (value_x10 < 0) ? '-' : '+';
    abs_value = motor_main_abs_i32(value_x10);
    (void)snprintf(out_text,
                   out_size,
                   "%c%ld.%01ld",
                   sign,
                   (long)(abs_value / 10),
                   (long)(abs_value % 10));
}

static void motor_main_format_abs_x10_compact(char *out_text, size_t out_size, int32_t value_x10)
{
    int32_t abs_value;

    if ((out_text == 0) || (out_size == 0u))
    {
        return;
    }

    abs_value = motor_main_abs_i32(value_x10);
    (void)snprintf(out_text,
                   out_size,
                   "%ld.%01ld",
                   (long)(abs_value / 10),
                   (long)(abs_value % 10));
}

static void motor_main_format_unsigned_x10(char *out_text, size_t out_size, int32_t value_x10)
{
    int32_t clamped_value;

    if ((out_text == 0) || (out_size == 0u))
    {
        return;
    }

    clamped_value = (value_x10 < 0) ? 0 : value_x10;
    (void)snprintf(out_text,
                   out_size,
                   "%ld.%01ld",
                   (long)(clamped_value / 10),
                   (long)(clamped_value % 10));
}

static void motor_main_format_fixed_unsigned_x10_3(char *out_text, size_t out_size, int32_t value_x10)
{
    int32_t clamped_value;

    if ((out_text == 0) || (out_size == 0u))
    {
        return;
    }

    clamped_value = motor_main_clamp_i32(value_x10, 0, 9999);
    (void)snprintf(out_text,
                   out_size,
                   "%03ld.%01ld",
                   (long)(clamped_value / 10),
                   (long)(clamped_value % 10));
}

static void motor_main_format_unsigned_x10_compact(char *out_text, size_t out_size, int32_t value_x10)
{
    int32_t clamped_value;

    if ((out_text == 0) || (out_size == 0u))
    {
        return;
    }

    clamped_value = motor_main_clamp_i32(value_x10, 0, 9999);
    (void)snprintf(out_text,
                   out_size,
                   "%ld.%01ld",
                   (long)(clamped_value / 10),
                   (long)(clamped_value % 10));
}

static void motor_main_format_trip_distance(char *out_text,
                                            size_t out_size,
                                            uint32_t distance_m,
                                            const motor_unit_settings_t *units)
{
    int32_t distance_x10;

    if ((out_text == 0) || (out_size == 0u))
    {
        return;
    }

    if ((units != 0) && (units->distance == (uint8_t)MOTOR_DISTANCE_UNIT_MI))
    {
        distance_x10 = (int32_t)((((uint64_t)distance_m * 6214ull) + 500000ull) / 1000000ull);
    }
    else
    {
        distance_x10 = (int32_t)((distance_m + 50u) / 100u);
    }

    distance_x10 = motor_main_clamp_i32(distance_x10, 0, 99999);
    (void)snprintf(out_text,
                   out_size,
                   "%ld.%01ld",
                   (long)(distance_x10 / 10),
                   (long)(distance_x10 % 10));
}

static void motor_main_format_abs_deg_2digit(char *out_text, size_t out_size, int32_t value_deg_x10)
{
    int32_t degrees;

    if ((out_text == 0) || (out_size == 0u))
    {
        return;
    }

    degrees = motor_main_abs_i32(value_deg_x10) / 10;
    if (degrees > 99)
    {
        degrees = 99;
    }

    (void)snprintf(out_text, out_size, "%02ld", (long)degrees);
}

static uint32_t motor_main_isqrt_u32(uint32_t value)
{
    uint32_t bit;
    uint32_t result;

    result = 0u;
    bit = 1uL << 30;
    while (bit > value)
    {
        bit >>= 2;
    }

    while (bit != 0u)
    {
        if (value >= (result + bit))
        {
            value -= (result + bit);
            result = (result >> 1) + bit;
        }
        else
        {
            result >>= 1;
        }
        bit >>= 2;
    }

    return result;
}

static void motor_main_format_abs_deg(char *out_text, size_t out_size, int32_t value_deg_x10)
{
    int32_t degrees;

    if ((out_text == 0) || (out_size == 0u))
    {
        return;
    }

    degrees = motor_main_abs_i32(value_deg_x10) / 10;
    if (degrees > 99)
    {
        degrees = 99;
    }

    (void)snprintf(out_text, out_size, "%ld", (long)degrees);
}

static void motor_main_format_speed_integer(char *out_text,
                                            size_t out_size,
                                            int32_t speed_x10)
{
    if ((out_text == 0) || (out_size == 0u))
    {
        return;
    }

    (void)snprintf(out_text, out_size, "%ld", (long)(speed_x10 / 10));
}

static void motor_main_get_left_geometry(int16_t x,
                                         int16_t y,
                                         int16_t w,
                                         int16_t h,
                                         motor_main_left_geometry_t *out_geo)
{
    int16_t brake_edge_x;
    int16_t accel_edge_x;
    int16_t content_x;
    int16_t content_w;

    (void)y;
    (void)h;

    if (out_geo == 0)
    {
        return;
    }

    brake_edge_x = x;
    accel_edge_x = (int16_t)(x + w - 1);
    content_x = (int16_t)(brake_edge_x + MOTOR_MAIN_LONG_G_BOTTOM_W + 6);
    content_w = (int16_t)(accel_edge_x - MOTOR_MAIN_LONG_G_BOTTOM_W - 6 - content_x + 1);

    if (content_w < 28)
    {
        content_x = (int16_t)(x + 12);
        content_w = (int16_t)(w - 24);
    }

    out_geo->brake_edge_x = brake_edge_x;
    out_geo->accel_edge_x = accel_edge_x;
    out_geo->content_x = content_x;
    out_geo->content_w = content_w;
}

static int16_t motor_main_trapezoid_row_width(int16_t row,
                                              int16_t h,
                                              int16_t top_w,
                                              int16_t bottom_w)
{
    if (h <= 1)
    {
        return top_w;
    }

    return (int16_t)(top_w + (((bottom_w - top_w) * row) / (h - 1)));
}

static int16_t motor_main_scale_angle_to_half_span(int32_t abs_value_x10,
                                                   int16_t half_span_px);

static int16_t motor_main_variable_tick_span(int16_t offset_px,
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
    if (grow_span <= 0)
    {
        return inner_span;
    }

    denom = (int32_t)half_span_px * (int32_t)half_span_px;
    if (denom <= 0)
    {
        return outer_span;
    }

    numerator = (grow_span * (int32_t)offset_px * (int32_t)offset_px) + (denom / 2);
    return (int16_t)(inner_span + (numerator / denom));
}

static const motor_main_span_cache_t *motor_main_get_span_cache(motor_main_span_cache_t *cache,
                                                                int16_t half_span_px,
                                                                int16_t inner_span,
                                                                int16_t outer_span,
                                                                uint8_t build_angle_offsets)
{
    int16_t i;

    if ((cache == 0) || (half_span_px <= 0))
    {
        return 0;
    }

    if (half_span_px > MOTOR_MAIN_SPAN_CACHE_MAX_HALF_SPAN)
    {
        half_span_px = MOTOR_MAIN_SPAN_CACHE_MAX_HALF_SPAN;
    }

    if ((cache->init != 0u) &&
        (cache->half_span_px == half_span_px) &&
        (cache->inner_span == inner_span) &&
        (cache->outer_span == outer_span))
    {
        return cache;
    }

    /* ---------------------------------------------------------------------- */
    /*  Gauge-span cache                                                      */
    /*                                                                        */
    /*  Main screen angle/G force gauges use the same "offset -> visible      */
    /*  height" mapping over and over. Without caching, every frame rebuilds  */
    /*  the same quadratic profile pixel by pixel.                            */
    /*                                                                        */
    /*  The cache converts that repeated math into a single table lookup so   */
    /*  the hot draw path spends its time issuing draw calls instead of       */
    /*  recalculating geometry that only depends on viewport size.            */
    /* ---------------------------------------------------------------------- */
    cache->init = 1u;
    cache->half_span_px = half_span_px;
    cache->inner_span = inner_span;
    cache->outer_span = outer_span;

    for (i = 0; i <= half_span_px; ++i)
    {
        cache->span_by_offset[i] = motor_main_variable_tick_span(i, half_span_px, inner_span, outer_span);
    }

    if (build_angle_offsets != 0u)
    {
        for (i = 0; i <= (MOTOR_MAIN_ANGLE_MAX_DEG_X10 / MOTOR_MAIN_ANGLE_TICK_STEP_X10); ++i)
        {
            cache->angle_tick_offset[i] =
                motor_main_scale_angle_to_half_span((int32_t)i * MOTOR_MAIN_ANGLE_TICK_STEP_X10,
                                                    half_span_px);
        }
    }

    return cache;
}

static const motor_main_trapezoid_cache_t *motor_main_get_trapezoid_cache(int16_t h,
                                                                           int16_t top_w,
                                                                           int16_t bottom_w)
{
    int16_t row;

    if (h <= 0)
    {
        return 0;
    }

    if (h > MOTOR_MAIN_TRAPEZOID_CACHE_MAX_H)
    {
        h = MOTOR_MAIN_TRAPEZOID_CACHE_MAX_H;
    }

    if ((s_motor_main_trapezoid_cache.init != 0u) &&
        (s_motor_main_trapezoid_cache.h == h) &&
        (s_motor_main_trapezoid_cache.top_w == top_w) &&
        (s_motor_main_trapezoid_cache.bottom_w == bottom_w))
    {
        return &s_motor_main_trapezoid_cache;
    }

    s_motor_main_trapezoid_cache.init = 1u;
    s_motor_main_trapezoid_cache.h = h;
    s_motor_main_trapezoid_cache.top_w = top_w;
    s_motor_main_trapezoid_cache.bottom_w = bottom_w;

    for (row = 0; row < h; ++row)
    {
        s_motor_main_trapezoid_cache.row_width[row] =
            motor_main_trapezoid_row_width(row, h, top_w, bottom_w);
    }

    return &s_motor_main_trapezoid_cache;
}

static const motor_main_vertical_cache_t *motor_main_get_vertical_cache(int16_t h,
                                                                        int16_t half_span_px,
                                                                        int16_t inner_w,
                                                                        int16_t outer_w)
{
    int16_t row;

    if (h <= 0)
    {
        return 0;
    }

    if (h > MOTOR_MAIN_VERTICAL_CACHE_MAX_H)
    {
        h = MOTOR_MAIN_VERTICAL_CACHE_MAX_H;
    }

    if ((s_motor_main_vertical_cache.init != 0u) &&
        (s_motor_main_vertical_cache.h == h) &&
        (s_motor_main_vertical_cache.half_span_px == half_span_px) &&
        (s_motor_main_vertical_cache.inner_w == inner_w) &&
        (s_motor_main_vertical_cache.outer_w == outer_w))
    {
        return &s_motor_main_vertical_cache;
    }

    s_motor_main_vertical_cache.init = 1u;
    s_motor_main_vertical_cache.h = h;
    s_motor_main_vertical_cache.half_span_px = half_span_px;
    s_motor_main_vertical_cache.inner_w = inner_w;
    s_motor_main_vertical_cache.outer_w = outer_w;

    for (row = 0; row < h; ++row)
    {
        int16_t rel_top;
        int16_t rel_bottom;
        int16_t offset_px;

        rel_top = row;
        rel_bottom = (int16_t)((h - 1) - row);
        offset_px = (int16_t)(half_span_px - ((rel_top < rel_bottom) ? rel_top : rel_bottom));
        if (offset_px < 0)
        {
            offset_px = 0;
        }

        s_motor_main_vertical_cache.row_width[row] =
            motor_main_variable_tick_span(offset_px, half_span_px, inner_w, outer_w);
    }

    return &s_motor_main_vertical_cache;
}

static const motor_main_text_cache_t *motor_main_get_text_cache(u8g2_t *u8g2)
{
    if (u8g2 == 0)
    {
        return 0;
    }

    if (s_motor_main_text_cache.init == 0u)
    {
        s_motor_main_text_cache.small_h = motor_main_get_font_height(u8g2, MOTOR_MAIN_FONT_SMALL);
        s_motor_main_text_cache.medium_h = motor_main_get_font_height(u8g2, MOTOR_MAIN_FONT_MEDIUM);
        s_motor_main_text_cache.angle_large_h = motor_main_get_font_height(u8g2, MOTOR_MAIN_FONT_ANGLE_LARGE);
        s_motor_main_text_cache.speed_large_h = motor_main_get_font_height(u8g2, MOTOR_MAIN_FONT_SPEED_LARGE);
        s_motor_main_text_cache.speed_unit_w = (int16_t)motor_main_measure_text(u8g2, MOTOR_MAIN_FONT_SMALL, "km/h");
        s_motor_main_text_cache.percent_w = (int16_t)motor_main_measure_text(u8g2, MOTOR_MAIN_FONT_SMALL, "%");
        s_motor_main_text_cache.angle_slot_w = (int16_t)motor_main_measure_text(u8g2, MOTOR_MAIN_FONT_ANGLE_LARGE, "00");
        s_motor_main_text_cache.angle_medium_slot_w = (int16_t)motor_main_measure_text(u8g2, MOTOR_MAIN_FONT_MEDIUM, "00");
        s_motor_main_text_cache.grade_slot_w = (int16_t)motor_main_measure_text(u8g2, MOTOR_MAIN_FONT_MEDIUM, "00.0");
        s_motor_main_text_cache.altitude_slot_w = (int16_t)motor_main_measure_text(u8g2, MOTOR_MAIN_FONT_MEDIUM, "9999");
        if ((int16_t)motor_main_measure_text(u8g2, MOTOR_MAIN_FONT_MEDIUM, "-999") >
            s_motor_main_text_cache.altitude_slot_w)
        {
            s_motor_main_text_cache.altitude_slot_w =
                (int16_t)motor_main_measure_text(u8g2, MOTOR_MAIN_FONT_MEDIUM, "-999");
        }
        s_motor_main_text_cache.init = 1u;
    }

    return &s_motor_main_text_cache;
}

static int16_t motor_main_scale_angle_to_half_span(int32_t abs_value_x10,
                                                   int16_t half_span_px)
{
    int16_t span_35_px;
    int16_t span_50_px;
    int16_t span_60_px;

    if (half_span_px <= 0)
    {
        return 0;
    }

    abs_value_x10 = motor_main_clamp_i32(abs_value_x10, 0, MOTOR_MAIN_ANGLE_MAX_DEG_X10);
    span_35_px = (int16_t)((half_span_px * 6) / 10);
    span_50_px = (int16_t)((half_span_px * 9) / 10);
    span_60_px = half_span_px;

    if (abs_value_x10 <= MOTOR_MAIN_ANGLE_FINE1_MAX_DEG_X10)
    {
        return motor_main_scale_to_span(abs_value_x10,
                                        MOTOR_MAIN_ANGLE_FINE1_MAX_DEG_X10,
                                        span_35_px);
    }

    if (abs_value_x10 <= MOTOR_MAIN_ANGLE_FINE2_MAX_DEG_X10)
    {
        return (int16_t)(span_35_px +
                         motor_main_scale_to_span((int32_t)(abs_value_x10 - MOTOR_MAIN_ANGLE_FINE1_MAX_DEG_X10),
                                                  (int32_t)(MOTOR_MAIN_ANGLE_FINE2_MAX_DEG_X10 - MOTOR_MAIN_ANGLE_FINE1_MAX_DEG_X10),
                                                  (int16_t)(span_50_px - span_35_px)));
    }

    return (int16_t)(span_50_px +
                     motor_main_scale_to_span((int32_t)(abs_value_x10 - MOTOR_MAIN_ANGLE_FINE2_MAX_DEG_X10),
                                              (int32_t)(MOTOR_MAIN_ANGLE_MAX_DEG_X10 - MOTOR_MAIN_ANGLE_FINE2_MAX_DEG_X10),
                                             (int16_t)(span_60_px - span_50_px)));
}

static int16_t motor_main_scale_grade_to_half_span(int32_t abs_value_x10,
                                                   int16_t half_span_px)
{
    int16_t span_8_px;
    int16_t span_15_px;

    if (half_span_px <= 0)
    {
        return 0;
    }

    abs_value_x10 = motor_main_clamp_i32(abs_value_x10, 0, MOTOR_MAIN_GRADE_MAX_X10);
    span_8_px = (int16_t)((half_span_px * 3) / 4);
    span_15_px = half_span_px;

    if (abs_value_x10 <= 80)
    {
        return motor_main_scale_to_span(abs_value_x10, 80, span_8_px);
    }

    return (int16_t)(span_8_px +
                     motor_main_scale_to_span((int32_t)(abs_value_x10 - 80),
                                              (int32_t)(MOTOR_MAIN_GRADE_MAX_X10 - 80),
                                              (int16_t)(span_15_px - span_8_px)));
}

static int16_t motor_main_get_bottom_anchor_y(int16_t section_y,
                                              int16_t section_h,
                                              int16_t extra_bottom_px)
{
    return (int16_t)(section_y + section_h - 1 + extra_bottom_px);
}

static void motor_main_make_bottom_aligned_rect(int16_t x,
                                                int16_t w,
                                                int16_t bottom_y,
                                                int16_t rect_h,
                                                ui_rect_t *out_rect)
{
    if (out_rect == 0)
    {
        return;
    }

    out_rect->x = x;
    out_rect->w = w;
    out_rect->h = rect_h;
    out_rect->y = (int16_t)(bottom_y - rect_h + 1);
}

static int32_t motor_main_apply_left_display_polarity(const motor_state_t *state,
                                                      int32_t raw_value)
{
    return (motor_main_left_is_negative(state) != 0u) ? raw_value : -raw_value;
}

static void motor_main_draw_angle_peak_marker(u8g2_t *u8g2,
                                              int16_t x,
                                              int16_t y,
                                              int16_t w,
                                              int16_t h,
                                              int32_t peak_value_x10)
{
    int16_t center_x;
    int16_t half_span_px;
    int16_t inner_h;
    int16_t end_outer_h;
    int16_t offset_px;
    int16_t draw_h;
    int16_t draw_x;
    int16_t marker_x;
    int16_t marker_w;

    if ((u8g2 == 0) || (w < 3) || (h <= 0))
    {
        return;
    }

    peak_value_x10 = motor_main_clamp_i32(peak_value_x10,
                                          -MOTOR_MAIN_ANGLE_MAX_DEG_X10,
                                          MOTOR_MAIN_ANGLE_MAX_DEG_X10);
    if (peak_value_x10 == 0)
    {
        return;
    }

    center_x = (int16_t)(x + (w / 2));
    half_span_px = (int16_t)((w - 1) / 2);
    inner_h = (MOTOR_MAIN_ANGLE_GAUGE_BAR_H > 3) ? (int16_t)((MOTOR_MAIN_ANGLE_GAUGE_BAR_H / 3) + 3) : 4;
    if (inner_h > h)
    {
        inner_h = h;
    }
    if (inner_h < 1)
    {
        inner_h = 1;
    }

    end_outer_h = (int16_t)(h + 2);
    offset_px = motor_main_scale_angle_to_half_span(motor_main_abs_i32(peak_value_x10), half_span_px);
    draw_h = motor_main_variable_tick_span(offset_px, half_span_px, inner_h, end_outer_h);
    draw_x = (peak_value_x10 >= 0) ? (int16_t)(center_x + offset_px) : (int16_t)(center_x - offset_px);
    marker_w = (w >= 3) ? 3 : 1;
    marker_x = (int16_t)(draw_x - 1);
    if (marker_x < x)
    {
        marker_x = x;
    }
    if ((marker_x + marker_w) > (x + w))
    {
        marker_x = (int16_t)((x + w) - marker_w);
    }
    if ((draw_h > 0) && (marker_x >= x) && ((marker_x + marker_w) <= (x + w)))
    {
        u8g2_DrawBox(u8g2, marker_x, y, (uint8_t)marker_w, (uint8_t)draw_h);
    }
}

static void motor_main_draw_lat_peak_marker(u8g2_t *u8g2,
                                            int16_t x,
                                            int16_t y,
                                            int16_t w,
                                            int16_t h,
                                            int32_t peak_value_mg,
                                            int32_t max_abs_value)
{
    int16_t center_x;
    int16_t half_span_px;
    int16_t outer_h;
    int16_t inner_h;
    int16_t offset_px;
    int16_t draw_h;
    int16_t draw_x;
    int16_t marker_x;
    int16_t marker_w;
    int16_t marker_y;

    if ((u8g2 == 0) || (w < 3) || (h <= 0) || (max_abs_value <= 0))
    {
        return;
    }

    peak_value_mg = motor_main_clamp_i32(peak_value_mg, -max_abs_value, max_abs_value);
    if (peak_value_mg == 0)
    {
        return;
    }

    center_x = (int16_t)(x + (w / 2));
    half_span_px = (int16_t)((w - 1) / 2);
    outer_h = (MOTOR_MAIN_LAT_GAUGE_MAJOR_TICK_H < h) ? MOTOR_MAIN_LAT_GAUGE_MAJOR_TICK_H : h;
    inner_h = MOTOR_MAIN_LAT_GAUGE_BAR_H;
    if (inner_h > outer_h)
    {
        inner_h = outer_h;
    }
    if (inner_h < 1)
    {
        inner_h = 1;
    }

    offset_px = motor_main_scale_to_span(motor_main_abs_i32(peak_value_mg), max_abs_value, half_span_px);
    draw_h = motor_main_variable_tick_span(offset_px, half_span_px, inner_h, outer_h);
    draw_x = (peak_value_mg >= 0) ? (int16_t)(center_x + offset_px) : (int16_t)(center_x - offset_px);
    marker_y = (int16_t)(y + h - draw_h);
    marker_w = (w >= 3) ? 3 : 1;
    marker_x = (int16_t)(draw_x - 1);
    if (marker_x < x)
    {
        marker_x = x;
    }
    if ((marker_x + marker_w) > (x + w))
    {
        marker_x = (int16_t)((x + w) - marker_w);
    }
    if ((draw_h > 0) && (marker_x >= x) && ((marker_x + marker_w) <= (x + w)))
    {
        u8g2_DrawBox(u8g2, marker_x, marker_y, (uint8_t)marker_w, (uint8_t)draw_h);
    }
}

static void motor_main_draw_long_peak_marker(u8g2_t *u8g2,
                                             int16_t edge_x,
                                             int16_t y,
                                             int16_t h,
                                             uint8_t align_right,
                                             uint8_t fill_from_top,
                                             int32_t peak_value,
                                             int32_t max_value)
{
    const motor_main_trapezoid_cache_t *trap_cache;
    int16_t gauge_top_w;
    int16_t gauge_bottom_w;
    int16_t draw_row;
    int16_t width;
    int16_t draw_x;
    int16_t marker_y;
    int16_t marker_h;

    if ((u8g2 == 0) || (h <= 0) || (max_value <= 0))
    {
        return;
    }

    peak_value = motor_main_clamp_i32(peak_value, 0, max_value);
    if (peak_value == 0)
    {
        return;
    }

    gauge_top_w = MOTOR_MAIN_LONG_G_TOP_W;
    gauge_bottom_w = MOTOR_MAIN_LONG_G_BOTTOM_W;
    if (fill_from_top == 0u)
    {
        gauge_top_w = MOTOR_MAIN_LONG_G_BOTTOM_W;
        gauge_bottom_w = MOTOR_MAIN_LONG_G_TOP_W;
    }

    trap_cache = motor_main_get_trapezoid_cache(h, gauge_top_w, gauge_bottom_w);

    draw_row = motor_main_scale_to_span(peak_value, max_value, (int16_t)(h - 1));
    if (fill_from_top == 0u)
    {
        draw_row = (int16_t)((h - 1) - draw_row);
    }
    width = (trap_cache != 0) ? trap_cache->row_width[draw_row] :
                                motor_main_trapezoid_row_width(draw_row, h, gauge_top_w, gauge_bottom_w);
    draw_x = (align_right != 0u) ? (int16_t)(edge_x - width + 1) : edge_x;
    marker_h = (h >= 3) ? 3 : 1;
    marker_y = (int16_t)(y + draw_row - 1);
    if (width <= 0)
    {
        return;
    }
    if (marker_y < y)
    {
        marker_y = y;
    }
    if ((marker_y + marker_h) > (y + h))
    {
        marker_y = (int16_t)((y + h) - marker_h);
    }
    u8g2_DrawBox(u8g2, draw_x, marker_y, (uint8_t)width, (uint8_t)marker_h);
}

static void motor_main_draw_trapezoid_gauge(u8g2_t *u8g2,
                                            int16_t edge_x,
                                            int16_t y,
                                            int16_t h,
                                            uint8_t align_right,
                                            uint8_t fill_from_top,
                                            int32_t value,
                                            int32_t max_value)
{
    const motor_main_trapezoid_cache_t *trap_cache;
    int16_t gauge_top_w;
    int16_t gauge_bottom_w;
    int16_t row;
    int16_t fill_rows;
    int32_t tick_value;

    if ((u8g2 == 0) || (h <= 0) || (max_value <= 0))
    {
        return;
    }

    value = motor_main_clamp_i32(value, 0, max_value);
    fill_rows = (int16_t)((value * h) / max_value);
    if (fill_rows > h)
    {
        fill_rows = h;
    }

    gauge_top_w = MOTOR_MAIN_LONG_G_TOP_W;
    gauge_bottom_w = MOTOR_MAIN_LONG_G_BOTTOM_W;
    if (fill_from_top == 0u)
    {
        gauge_top_w = MOTOR_MAIN_LONG_G_BOTTOM_W;
        gauge_bottom_w = MOTOR_MAIN_LONG_G_TOP_W;
    }

    trap_cache = motor_main_get_trapezoid_cache(h, gauge_top_w, gauge_bottom_w);

    for (tick_value = 0; tick_value <= max_value; tick_value += MOTOR_MAIN_LONG_G_TICK_STEP_MG)
    {
        int16_t width;
        int16_t draw_x;
        int16_t draw_row;

        draw_row = motor_main_scale_to_span(tick_value, max_value, (int16_t)(h - 1));
        if (fill_from_top == 0u)
        {
            draw_row = (int16_t)((h - 1) - draw_row);
        }
        if (draw_row >= (h - 1))
        {
            continue;
        }

        width = (trap_cache != 0) ? trap_cache->row_width[draw_row] :
                                    motor_main_trapezoid_row_width(draw_row, h, gauge_top_w, gauge_bottom_w);
        draw_x = (align_right != 0u) ? (int16_t)(edge_x - width + 1) : edge_x;
        if (width > 0)
        {
            u8g2_DrawHLine(u8g2, draw_x, (int16_t)(y + draw_row), (uint8_t)width);
        }
    }

    if ((max_value % MOTOR_MAIN_LONG_G_TICK_STEP_MG) != 0)
    {
        int16_t width;
        int16_t draw_x;
        int16_t draw_row;

        draw_row = (fill_from_top != 0u) ? (int16_t)(h - 1) : 0;
        if (draw_row < (h - 1))
        {
            width = (trap_cache != 0) ? trap_cache->row_width[draw_row] :
                                        motor_main_trapezoid_row_width(draw_row, h, gauge_top_w, gauge_bottom_w);
            draw_x = (align_right != 0u) ? (int16_t)(edge_x - width + 1) : edge_x;
            if (width > 0)
            {
                u8g2_DrawHLine(u8g2, draw_x, (int16_t)(y + draw_row), (uint8_t)width);
            }
        }
    }

    if (fill_rows > 0)
    {
        int16_t start_row;
        int16_t end_row;

        if (fill_from_top != 0u)
        {
            start_row = 0;
            end_row = fill_rows;
        }
        else
        {
            start_row = (int16_t)(h - fill_rows);
            end_row = h;
        }

        for (row = start_row; row < end_row; row++)
        {
            int16_t width;
            int16_t draw_x;

            width = (trap_cache != 0) ? trap_cache->row_width[row] :
                                        motor_main_trapezoid_row_width(row, h, gauge_top_w, gauge_bottom_w);
            draw_x = (align_right != 0u) ? (int16_t)(edge_x - width + 1) : edge_x;
            if (width > 0)
            {
                u8g2_DrawHLine(u8g2, draw_x, (int16_t)(y + row), (uint8_t)width);
            }
        }
    }

    for (row = 0; row < h; row++)
    {
        int16_t width;
        int16_t draw_x;
        int16_t outline_x;

        width = (trap_cache != 0) ? trap_cache->row_width[row] :
                                    motor_main_trapezoid_row_width(row, h, gauge_top_w, gauge_bottom_w);
        draw_x = (align_right != 0u) ? (int16_t)(edge_x - width + 1) : edge_x;
        outline_x = (align_right != 0u) ? draw_x : (int16_t)(draw_x + width - 1);
        if ((width > 0) && (outline_x >= 0))
        {
            u8g2_DrawPixel(u8g2, outline_x, (int16_t)(y + row));
        }
    }
}

static void motor_main_draw_unipolar_top_gauge(u8g2_t *u8g2,
                                               int16_t x,
                                               int16_t y,
                                               int16_t w,
                                               int16_t h,
                                               int32_t value,
                                               int32_t max_value,
                                               int32_t major_step,
                                               int32_t minor_step)
{
    int16_t major_h;
    int16_t minor_h;
    int16_t bar_y;
    int16_t bar_h;
    int16_t fill_w;
    int32_t step;

    if ((u8g2 == 0) || (w < 20) || (h <= 0) || (max_value <= 0))
    {
        return;
    }

    value = motor_main_clamp_i32(value, 0, max_value);
    major_h = (h < MOTOR_MAIN_TOP_GAUGE_MAJOR_TICK_H) ? h : MOTOR_MAIN_TOP_GAUGE_MAJOR_TICK_H;
    minor_h = (h < MOTOR_MAIN_TOP_GAUGE_MINOR_TICK_H) ? h : MOTOR_MAIN_TOP_GAUGE_MINOR_TICK_H;
    if (minor_h > major_h)
    {
        minor_h = major_h;
    }
    if (minor_h < 1)
    {
        minor_h = 1;
    }

    bar_y = (int16_t)(y + MOTOR_MAIN_TOP_GAUGE_BAR_TOP_DY);
    if (bar_y < y)
    {
        bar_y = y;
    }

    bar_h = MOTOR_MAIN_TOP_GAUGE_BAR_H;
    if ((bar_y + bar_h) > (y + major_h))
    {
        bar_h = (int16_t)((y + major_h) - bar_y);
    }
    if (bar_h < 1)
    {
        bar_h = 1;
    }

    if ((minor_step > 0) && (minor_step < max_value))
    {
        for (step = minor_step; step < max_value; step += minor_step)
        {
            int16_t tick_x;

            if ((major_step > 0) && ((step % major_step) == 0))
            {
                continue;
            }

            tick_x = (int16_t)(x + motor_main_scale_to_span(step, max_value, (int16_t)(w - 1)));
            u8g2_DrawVLine(u8g2, tick_x, y, (uint8_t)minor_h);
        }
    }

    if (major_step > 0)
    {
        for (step = 0; step <= max_value; step += major_step)
        {
            int16_t tick_x;

            tick_x = (int16_t)(x + motor_main_scale_to_span(step, max_value, (int16_t)(w - 1)));
            u8g2_DrawVLine(u8g2, tick_x, y, (uint8_t)major_h);
        }
    }

    fill_w = motor_main_scale_to_span(value, max_value, w);
    if (fill_w > 0)
    {
        u8g2_DrawBox(u8g2, x, bar_y, (uint8_t)fill_w, (uint8_t)bar_h);
    }
}

static void motor_main_draw_bipolar_gauge(u8g2_t *u8g2,
                                          int16_t x,
                                          int16_t y,
                                          int16_t w,
                                          int16_t h,
                                          int32_t value,
                                          int32_t max_abs_value,
                                          int32_t major_step,
                                          int32_t minor_step,
                                          int16_t major_tick_h,
                                          int16_t minor_tick_h,
                                          int16_t bar_h,
                                          int16_t center_tick_w)
{
    const motor_main_span_cache_t *span_cache;
    int16_t center_x;
    int16_t half_span_px;
    int16_t outer_h;
    int16_t end_outer_h;
    int16_t inner_h;
    int16_t i;
    int32_t clamped_value;
    int32_t abs_value;
    int32_t step;

    if ((u8g2 == 0) || (w < 3) || (h <= 0) || (max_abs_value <= 0))
    {
        return;
    }

    clamped_value = motor_main_clamp_i32(value, -max_abs_value, max_abs_value);
    abs_value = motor_main_abs_i32(clamped_value);
    center_x = (int16_t)(x + (w / 2));
    half_span_px = (int16_t)((w - 1) / 2);

    (void)major_step;
    (void)minor_step;
    (void)minor_tick_h;

    outer_h = h;
    end_outer_h = (int16_t)(h + 2);
    inner_h = (bar_h > 3) ? (int16_t)((bar_h / 3) + 3) : 4;
    if (inner_h > outer_h)
    {
        inner_h = outer_h;
    }
    if (inner_h < 1)
    {
        inner_h = 1;
    }

    span_cache = motor_main_get_span_cache(&s_motor_main_angle_cache,
                                           half_span_px,
                                           inner_h,
                                           end_outer_h,
                                           1u);

    if (abs_value > 0)
    {
        int16_t fill_px;

        fill_px = motor_main_scale_angle_to_half_span(abs_value, half_span_px);
        if (fill_px > 0)
        {
            int16_t offset_px;

            for (offset_px = 0; offset_px <= fill_px; offset_px++)
            {
                int16_t draw_x;
                int16_t draw_h;

                draw_x = (clamped_value >= 0)
                         ? (int16_t)(center_x + offset_px)
                         : (int16_t)(center_x - offset_px);
                if ((draw_x < x) || (draw_x >= (x + w)))
                {
                    continue;
                }

                draw_h = (span_cache != 0) ? span_cache->span_by_offset[offset_px] :
                                             motor_main_variable_tick_span(offset_px, half_span_px, inner_h, end_outer_h);
                u8g2_DrawVLine(u8g2, draw_x, y, (uint8_t)draw_h);
            }
        }
    }

    if (center_tick_w < 1)
    {
        center_tick_w = 1;
    }

    for (i = 0; i < center_tick_w; i++)
    {
        int16_t line_x;

        line_x = (int16_t)(center_x - (center_tick_w / 2) + i);
        if ((line_x >= x) && (line_x < (x + w)))
        {
            u8g2_DrawVLine(u8g2, line_x, y, (uint8_t)inner_h);
        }
    }

    for (step = MOTOR_MAIN_ANGLE_TICK_STEP_X10; step < MOTOR_MAIN_ANGLE_MAX_DEG_X10; step += MOTOR_MAIN_ANGLE_TICK_STEP_X10)
    {
        int16_t offset_px;
        int16_t draw_h;

        offset_px = (span_cache != 0) ? span_cache->angle_tick_offset[step / MOTOR_MAIN_ANGLE_TICK_STEP_X10] :
                                        motor_main_scale_angle_to_half_span(step, half_span_px);
        draw_h = (span_cache != 0) ? span_cache->span_by_offset[offset_px] :
                                     motor_main_variable_tick_span(offset_px, half_span_px, inner_h, end_outer_h);
        u8g2_DrawVLine(u8g2, (int16_t)(center_x - offset_px), y, (uint8_t)draw_h);
        u8g2_DrawVLine(u8g2, (int16_t)(center_x + offset_px), y, (uint8_t)draw_h);
    }

    for (i = 0; i <= half_span_px; i++)
    {
        int16_t draw_h;
        int16_t outline_y;

        draw_h = (span_cache != 0) ? span_cache->span_by_offset[i] :
                                     motor_main_variable_tick_span(i, half_span_px, inner_h, end_outer_h);
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

static void motor_main_draw_bipolar_gauge_bottom(u8g2_t *u8g2,
                                                 int16_t x,
                                                 int16_t y,
                                                 int16_t w,
                                                 int16_t h,
                                                 int32_t value,
                                                 int32_t max_abs_value,
                                                 int32_t major_step,
                                                 int32_t minor_step,
                                                 int16_t major_tick_h,
                                                 int16_t minor_tick_h,
                                                 int16_t bar_h,
                                                 int16_t center_tick_w)
{
    const motor_main_span_cache_t *span_cache;
    int16_t center_x;
    int16_t half_span_px;
    int16_t outer_h;
    int16_t minor_h;
    int16_t inner_h;
    int16_t i;
    int32_t clamped_value;
    int32_t abs_value;
    int32_t step;

    if ((u8g2 == 0) || (w < 3) || (h <= 0) || (max_abs_value <= 0))
    {
        return;
    }

    clamped_value = motor_main_clamp_i32(value, -max_abs_value, max_abs_value);
    abs_value = motor_main_abs_i32(clamped_value);
    center_x = (int16_t)(x + (w / 2));
    half_span_px = (int16_t)((w - 1) / 2);

    outer_h = (major_tick_h < h) ? major_tick_h : h;
    minor_h = (minor_tick_h < outer_h) ? minor_tick_h : outer_h;
    if (minor_h < 1)
    {
        minor_h = 1;
    }

    inner_h = bar_h;
    if (inner_h > outer_h)
    {
        inner_h = outer_h;
    }
    if (inner_h < 1)
    {
        inner_h = 1;
    }

    span_cache = motor_main_get_span_cache(&s_motor_main_bottom_gauge_cache,
                                           half_span_px,
                                           inner_h,
                                           outer_h,
                                           0u);

    if ((minor_step > 0) && (minor_step < max_abs_value))
    {
        for (step = minor_step; step < max_abs_value; step += minor_step)
        {
            int16_t offset_px;
            int16_t draw_h;

            if ((major_step > 0) && ((step % major_step) == 0))
            {
                continue;
            }

            offset_px = motor_main_scale_to_span(step, max_abs_value, half_span_px);
            draw_h = motor_main_variable_tick_span(offset_px, half_span_px, inner_h, minor_h);
            u8g2_DrawVLine(u8g2, (int16_t)(center_x - offset_px), (int16_t)(y + h - draw_h), (uint8_t)draw_h);
            u8g2_DrawVLine(u8g2, (int16_t)(center_x + offset_px), (int16_t)(y + h - draw_h), (uint8_t)draw_h);
        }
    }

    if (major_step > 0)
    {
        for (step = major_step; step <= max_abs_value; step += major_step)
        {
            int16_t offset_px;
            int16_t draw_h;

            offset_px = motor_main_scale_to_span(step, max_abs_value, half_span_px);
            draw_h = (span_cache != 0) ? span_cache->span_by_offset[offset_px] :
                                         motor_main_variable_tick_span(offset_px, half_span_px, inner_h, outer_h);
            u8g2_DrawVLine(u8g2, (int16_t)(center_x - offset_px), (int16_t)(y + h - draw_h), (uint8_t)draw_h);
            u8g2_DrawVLine(u8g2, (int16_t)(center_x + offset_px), (int16_t)(y + h - draw_h), (uint8_t)draw_h);
        }
    }

    if (abs_value > 0)
    {
        int16_t fill_px;

        fill_px = motor_main_scale_to_span(abs_value, max_abs_value, half_span_px);
        if (fill_px > 0)
        {
            int16_t offset_px;

            for (offset_px = 0; offset_px <= fill_px; offset_px++)
            {
                int16_t draw_x;
                int16_t draw_h;

                draw_x = (clamped_value >= 0)
                         ? (int16_t)(center_x + offset_px)
                         : (int16_t)(center_x - offset_px);
                if ((draw_x < x) || (draw_x >= (x + w)))
                {
                    continue;
                }

                draw_h = (span_cache != 0) ? span_cache->span_by_offset[offset_px] :
                                             motor_main_variable_tick_span(offset_px, half_span_px, inner_h, outer_h);
                u8g2_DrawVLine(u8g2, draw_x, (int16_t)(y + h - draw_h), (uint8_t)draw_h);
            }
        }
    }

    if (center_tick_w < 1)
    {
        center_tick_w = 1;
    }

    for (i = 0; i < center_tick_w; i++)
    {
        int16_t line_x;

        line_x = (int16_t)(center_x - (center_tick_w / 2) + i);
        if ((line_x >= x) && (line_x < (x + w)))
        {
            u8g2_DrawVLine(u8g2, line_x, (int16_t)(y + h - inner_h), (uint8_t)inner_h);
        }
    }

    for (i = 0; i <= half_span_px; i++)
    {
        int16_t draw_h;
        int16_t outline_y;

        draw_h = (span_cache != 0) ? span_cache->span_by_offset[i] :
                                     motor_main_variable_tick_span(i, half_span_px, inner_h, outer_h);
        outline_y = (int16_t)(y + h - draw_h);
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

static void motor_main_draw_vertical_bipolar_gauge(u8g2_t *u8g2,
                                                   int16_t x,
                                                   int16_t y,
                                                   int16_t w,
                                                   int16_t h,
                                                   int32_t value,
                                                   int32_t max_abs_value,
                                                   int32_t major_step,
                                                   int32_t minor_step)
{
    const motor_main_vertical_cache_t *vertical_cache;
    int16_t wall_x;
    int16_t center_top_row;
    int16_t center_bottom_row;
    int16_t half_span_px;
    int16_t row;
    int16_t outer_w;
    int16_t inner_w;
    int16_t i;
    int32_t clamped_value;
    int32_t abs_value;
    int32_t step;

    if ((u8g2 == 0) || (w <= 0) || (h < 3) || (max_abs_value <= 0))
    {
        return;
    }

    clamped_value = motor_main_clamp_i32(value, -max_abs_value, max_abs_value);
    abs_value = motor_main_abs_i32(clamped_value);
    wall_x = x;
    half_span_px = (int16_t)((h - 1) / 2);
    center_top_row = half_span_px;
    center_bottom_row = (int16_t)((h - 1) - half_span_px);

    outer_w = w;
    inner_w = (w < MOTOR_MAIN_VERTICAL_GAUGE_BAR_W) ? w : MOTOR_MAIN_VERTICAL_GAUGE_BAR_W;
    if (inner_w > outer_w)
    {
        inner_w = outer_w;
    }
    if (inner_w < 1)
    {
        inner_w = 1;
    }

    vertical_cache = motor_main_get_vertical_cache(h, half_span_px, inner_w, outer_w);

    for (row = 0; row < h; row++)
    {
        int16_t draw_w;

        draw_w = (vertical_cache != 0) ? vertical_cache->row_width[row] :
                                         motor_main_variable_tick_span((int16_t)(half_span_px - (((row) < ((h - 1) - row)) ? (row) : ((h - 1) - row))),
                                                                      half_span_px,
                                                                      inner_w,
                                                                      outer_w);
        if (draw_w > 0)
        {
            if ((row == 0) || (row == (h - 1)))
            {
                u8g2_DrawHLine(u8g2, wall_x, (int16_t)(y + row), (uint8_t)draw_w);
            }
            else
            {
                u8g2_DrawPixel(u8g2, wall_x, (int16_t)(y + row));
                u8g2_DrawPixel(u8g2, (int16_t)(wall_x + draw_w - 1), (int16_t)(y + row));
            }
        }
    }

    if ((minor_step > 0) && (minor_step < max_abs_value))
    {
        for (step = minor_step; step < max_abs_value; step += minor_step)
        {
            int16_t offset_px;
            int16_t row_a;
            int16_t row_b;
            int16_t draw_w;

            if ((major_step > 0) && ((step % major_step) == 0))
            {
                continue;
            }

            offset_px = motor_main_scale_grade_to_half_span(step, half_span_px);
            row_a = (int16_t)(center_top_row - offset_px);
            row_b = (int16_t)(center_bottom_row + offset_px);
            draw_w = motor_main_variable_tick_span(offset_px, half_span_px, inner_w, outer_w);
            if ((row_a >= 0) && (row_a < h))
            {
                u8g2_DrawHLine(u8g2, wall_x, (int16_t)(y + row_a), (uint8_t)draw_w);
            }
            if ((row_b >= 0) && (row_b < h) && (row_b != row_a))
            {
                u8g2_DrawHLine(u8g2, wall_x, (int16_t)(y + row_b), (uint8_t)draw_w);
            }
        }
    }

    if (major_step > 0)
    {
        for (step = major_step; step < max_abs_value; step += major_step)
        {
            int16_t offset_px;
            int16_t row_a;
            int16_t row_b;
            int16_t draw_w;

            offset_px = motor_main_scale_grade_to_half_span(step, half_span_px);
            row_a = (int16_t)(center_top_row - offset_px);
            row_b = (int16_t)(center_bottom_row + offset_px);
            draw_w = motor_main_variable_tick_span(offset_px, half_span_px, inner_w, outer_w);
            if ((row_a >= 0) && (row_a < h))
            {
                u8g2_DrawHLine(u8g2, wall_x, (int16_t)(y + row_a), (uint8_t)draw_w);
            }
            if ((row_b >= 0) && (row_b < h) && (row_b != row_a))
            {
                u8g2_DrawHLine(u8g2, wall_x, (int16_t)(y + row_b), (uint8_t)draw_w);
            }
        }
    }

    if (abs_value > 0)
    {
        int16_t fill_px;

        fill_px = motor_main_scale_grade_to_half_span(abs_value, half_span_px);
        if (fill_px > 0)
        {
            if (clamped_value >= 0)
            {
                for (row = (int16_t)(center_top_row - fill_px); row <= center_top_row; row++)
                {
                    int16_t offset_px;
                    int16_t draw_row;
                    int16_t draw_w;

                    draw_row = row;
                    if ((draw_row < 0) || (draw_row >= h))
                    {
                        continue;
                    }

                    offset_px = (int16_t)(center_top_row - draw_row);
                    draw_w = (vertical_cache != 0) ? vertical_cache->row_width[draw_row] :
                                                     motor_main_variable_tick_span(offset_px, half_span_px, inner_w, outer_w);
                    u8g2_DrawHLine(u8g2, wall_x, (int16_t)(y + draw_row), (uint8_t)draw_w);
                }
            }
            else
            {
                int16_t start_row;

                start_row = center_bottom_row;
                if (start_row < 0)
                {
                    start_row = 0;
                }
                for (row = start_row; row <= (int16_t)(center_bottom_row + fill_px); row++)
                {
                    int16_t offset_px;
                    int16_t draw_w;

                    if ((row < 0) || (row >= h))
                    {
                        continue;
                    }

                    offset_px = (int16_t)(row - center_bottom_row);
                    draw_w = (vertical_cache != 0) ? vertical_cache->row_width[row] :
                                                     motor_main_variable_tick_span(offset_px, half_span_px, inner_w, outer_w);
                    u8g2_DrawHLine(u8g2, wall_x, (int16_t)(y + row), (uint8_t)draw_w);
                }
            }
        }
    }

    if (MOTOR_MAIN_VERTICAL_GAUGE_CENTER_TICK_H > 0)
    {
        int16_t start_y;

        start_y = (int16_t)(y + ((h - MOTOR_MAIN_VERTICAL_GAUGE_CENTER_TICK_H) / 2));
        for (i = 0; i < MOTOR_MAIN_VERTICAL_GAUGE_CENTER_TICK_H; i++)
        {
            int16_t line_y;

            line_y = (int16_t)(start_y + i);
            if ((line_y >= y) && (line_y < (y + h)))
            {
                u8g2_DrawHLine(u8g2,
                               wall_x,
                               line_y,
                               (uint8_t)inner_w);
            }
        }
    }
}

static void motor_main_draw_right_stat_row(u8g2_t *u8g2,
                                           int16_t x,
                                           int16_t y,
                                           int16_t w,
                                           const char *label,
                                           const char *value,
                                           const char *unit)
{
    const motor_main_text_cache_t *text_cache;
    int16_t mid_h;
    int16_t small_h;
    int16_t unit_w;
    int16_t value_x;
    int16_t unit_x;
    int16_t unit_y;

    if ((u8g2 == 0) || (value == 0))
    {
        return;
    }

    text_cache = motor_main_get_text_cache(u8g2);
    mid_h = (text_cache != 0) ? text_cache->medium_h : motor_main_get_font_height(u8g2, MOTOR_MAIN_FONT_MEDIUM);
    small_h = (text_cache != 0) ? text_cache->small_h : motor_main_get_font_height(u8g2, MOTOR_MAIN_FONT_SMALL);
    unit_w = (int16_t)motor_main_measure_text(u8g2, MOTOR_MAIN_FONT_SMALL, (unit != 0) ? unit : "");
    unit_x = (int16_t)(x + w - unit_w);
    value_x = motor_main_right_text_x(u8g2,
                                      MOTOR_MAIN_FONT_MEDIUM,
                                      (int16_t)(unit_x - ((unit_w > 0) ? MOTOR_MAIN_VALUE_UNIT_GAP_X : 0)),
                                      value);
    unit_y = (int16_t)(y + ((mid_h > small_h) ? (mid_h - small_h) : 0));

    if (label != 0)
    {
        motor_main_draw_text_top(u8g2, MOTOR_MAIN_FONT_SMALL, x, y + 4, label);
    }

    motor_main_draw_text_top(u8g2, MOTOR_MAIN_FONT_MEDIUM, value_x, y, value);
    if ((unit != 0) && (unit[0] != '\0'))
    {
        motor_main_draw_text_top(u8g2, MOTOR_MAIN_FONT_SMALL, unit_x, unit_y, unit);
    }
}

static void motor_main_draw_top_section(u8g2_t *u8g2,
                                        int16_t x,
                                        int16_t y,
                                        int16_t w,
                                        int16_t h,
                                        const motor_state_t *state)
{
    motor_main_top_mode_t mode;
    int32_t value;
    int32_t max_value;
    int32_t major_step;
    int32_t minor_step;

    if ((u8g2 == 0) || (state == 0))
    {
        return;
    }

    mode = motor_main_get_top_mode(state);
    value = motor_main_get_top_gauge_value(state, mode);
    max_value = motor_main_get_top_gauge_max(state, mode);
    motor_main_get_top_tick_steps(state, mode, &major_step, &minor_step);

    motor_main_draw_unipolar_top_gauge(u8g2,
                                       x,
                                       y,
                                       w,
                                       h,
                                       value,
                                       max_value,
                                       major_step,
                                       minor_step);

    if (mode == MOTOR_MAIN_TOP_MODE_SPEED)
    {
        motor_main_draw_top_speed_labels_and_markers(u8g2, x, y, w, h, state);
    }
}

static void motor_main_draw_left_section(u8g2_t *u8g2,
                                         int16_t x,
                                         int16_t y,
                                         int16_t w,
                                         int16_t h,
                                         const motor_state_t *state)
{
    const motor_main_text_cache_t *text_cache;
    motor_main_left_geometry_t geo;
    ui_rect_t lat_gauge_rect;
    int16_t bottom_anchor_y;
    int16_t side_y;
    int16_t side_h;
    int16_t angle_h;
    int16_t mid_h;
    int16_t lat_gauge_x;
    int16_t lat_gauge_w;
    int16_t lat_gauge_y;
    int16_t max_row_y;
    int16_t angle_value_y;
    int32_t angle_value_x10;
    int32_t long_g_max_mg;
    int32_t lat_value_mg;
    int32_t g_max_mg;
    int32_t g_major_step_mg;
    int32_t g_minor_step_mg;
    int32_t brake_value_mg;
    int32_t accel_value_mg;
    int32_t sum_g_x10;
    int32_t left_angle_peak_x10;
    int32_t right_angle_peak_x10;
    int32_t left_lat_peak_mg;
    int32_t right_lat_peak_mg;
    int32_t brake_peak_mg;
    int32_t accel_peak_mg;
    char current_angle_buf[8];
    char left_max_buf[8];
    char right_max_buf[8];
    char lat_value_buf[8];

    if ((u8g2 == 0) || (state == 0) || (w < 56) || (h < 56))
    {
        return;
    }

    text_cache = motor_main_get_text_cache(u8g2);

    motor_main_get_left_geometry(x, y, w, h, &geo);

    angle_value_x10 = motor_main_get_display_bank_x10(state);
    long_g_max_mg = MOTOR_MAIN_LONG_G_MAX_MG;
    lat_value_mg = motor_main_get_display_lat_mg(state);
    g_max_mg = MOTOR_MAIN_LAT_G_MAX_MG;
    g_major_step_mg = MOTOR_MAIN_LAT_G_MAJOR_STEP_MG;
    g_minor_step_mg = MOTOR_MAIN_LAT_G_MINOR_STEP_MG;

    brake_value_mg = (state->dyn.lon_accel_mg < 0) ? motor_main_abs_i32(state->dyn.lon_accel_mg) : 0;
    accel_value_mg = (state->dyn.lon_accel_mg > 0) ? state->dyn.lon_accel_mg : 0;
    left_angle_peak_x10 = motor_main_apply_left_display_polarity(state, state->dyn.max_left_bank_deg_x10);
    right_angle_peak_x10 = motor_main_apply_left_display_polarity(state, state->dyn.max_right_bank_deg_x10);
    left_lat_peak_mg = motor_main_apply_left_display_polarity(state, state->dyn.max_left_lat_mg);
    right_lat_peak_mg = motor_main_apply_left_display_polarity(state, state->dyn.max_right_lat_mg);
    brake_peak_mg = motor_main_abs_i32(state->dyn.max_brake_mg);
    accel_peak_mg = state->dyn.max_accel_mg;

    motor_main_draw_bipolar_gauge(u8g2,
                                  x,
                                  y,
                                  w,
                                  MOTOR_MAIN_LEFT_SCALE_H,
                                  angle_value_x10,
                                  MOTOR_MAIN_ANGLE_MAX_DEG_X10,
                                  MOTOR_MAIN_ANGLE_MAJOR_STEP_X10,
                                  MOTOR_MAIN_ANGLE_MINOR_STEP_X10,
                                  MOTOR_MAIN_ANGLE_GAUGE_MAJOR_TICK_H,
                                  MOTOR_MAIN_ANGLE_GAUGE_MINOR_TICK_H,
                                  MOTOR_MAIN_ANGLE_GAUGE_BAR_H,
                                  MOTOR_MAIN_ANGLE_GAUGE_CENTER_TICK_W);
    motor_main_draw_angle_peak_marker(u8g2, x, y, w, MOTOR_MAIN_LEFT_SCALE_H, left_angle_peak_x10);
    motor_main_draw_angle_peak_marker(u8g2, x, y, w, MOTOR_MAIN_LEFT_SCALE_H, right_angle_peak_x10);

    angle_h = (text_cache != 0) ? text_cache->angle_large_h : motor_main_get_font_height(u8g2, MOTOR_MAIN_FONT_ANGLE_LARGE);
    mid_h = (text_cache != 0) ? text_cache->medium_h : motor_main_get_font_height(u8g2, MOTOR_MAIN_FONT_MEDIUM);
    bottom_anchor_y = motor_main_get_bottom_anchor_y(y,
                                                     h,
                                                     MOTOR_MAIN_MIDDLE_BOTTOM_GAP_Y);
    lat_gauge_x = (int16_t)(geo.brake_edge_x + MOTOR_MAIN_LONG_G_BOTTOM_W + MOTOR_MAIN_LAT_GAUGE_SIDE_INSET);
    lat_gauge_w = (int16_t)((geo.accel_edge_x - MOTOR_MAIN_LONG_G_BOTTOM_W - MOTOR_MAIN_LAT_GAUGE_SIDE_INSET) - lat_gauge_x + 1);
    motor_main_make_bottom_aligned_rect(lat_gauge_x,
                                        lat_gauge_w,
                                        bottom_anchor_y,
                                        MOTOR_MAIN_LEFT_BOTTOM_GAUGE_H,
                                        &lat_gauge_rect);
    lat_gauge_y = lat_gauge_rect.y;
    side_y = (int16_t)(y + MOTOR_MAIN_LEFT_SCALE_H + 6);
    side_h = (int16_t)(bottom_anchor_y - side_y + 1);
    if (side_h < 12)
    {
        side_h = 12;
    }
    if ((side_y + side_h - 1) > bottom_anchor_y)
    {
        side_h = (int16_t)(bottom_anchor_y - side_y + 1);
    }
    max_row_y = (int16_t)(lat_gauge_y - mid_h - 6);
    angle_value_y = (int16_t)(y + MOTOR_MAIN_LEFT_SCALE_H + ((max_row_y - (y + MOTOR_MAIN_LEFT_SCALE_H) - angle_h) / 2));
    if (angle_value_y < (int16_t)(y + MOTOR_MAIN_LEFT_SCALE_H + 2))
    {
        angle_value_y = (int16_t)(y + MOTOR_MAIN_LEFT_SCALE_H + 2);
    }
    angle_value_y = (int16_t)(angle_value_y + 4);

    motor_main_format_abs_deg_2digit(current_angle_buf, sizeof(current_angle_buf), angle_value_x10);
    motor_main_format_abs_deg(left_max_buf, sizeof(left_max_buf), state->dyn.max_left_bank_deg_x10);
    motor_main_format_abs_deg(right_max_buf, sizeof(right_max_buf), state->dyn.max_right_bank_deg_x10);
    sum_g_x10 = (int32_t)((motor_main_isqrt_u32((uint32_t)(((int64_t)motor_main_abs_i32(lat_value_mg) * (int64_t)motor_main_abs_i32(lat_value_mg)) +
                                                            ((int64_t)motor_main_abs_i32(state->dyn.lon_accel_mg) * (int64_t)motor_main_abs_i32(state->dyn.lon_accel_mg)))) + 50u) / 100u);
    sum_g_x10 = motor_main_clamp_i32(sum_g_x10, 0, 99);
    (void)snprintf(lat_value_buf,
                   sizeof(lat_value_buf),
                   "%1ld.%01ld",
                   (long)(sum_g_x10 / 10),
                   (long)(sum_g_x10 % 10));

    motor_main_draw_trapezoid_gauge(u8g2,
                                    geo.brake_edge_x,
                                    side_y,
                                    side_h,
                                    0u,
                                    1u,
                                    brake_value_mg,
                                    long_g_max_mg);
    motor_main_draw_long_peak_marker(u8g2,
                                     geo.brake_edge_x,
                                     side_y,
                                     side_h,
                                     0u,
                                     1u,
                                     brake_peak_mg,
                                     long_g_max_mg);
    motor_main_draw_trapezoid_gauge(u8g2,
                                    geo.accel_edge_x,
                                    side_y,
                                    side_h,
                                    1u,
                                    0u,
                                    accel_value_mg,
                                    long_g_max_mg);
    motor_main_draw_long_peak_marker(u8g2,
                                     geo.accel_edge_x,
                                     side_y,
                                     side_h,
                                     1u,
                                     0u,
                                     accel_peak_mg,
                                     long_g_max_mg);

    motor_main_draw_text_top(u8g2,
                             MOTOR_MAIN_FONT_ANGLE_LARGE,
                             motor_main_center_text_x(u8g2,
                                                      MOTOR_MAIN_FONT_ANGLE_LARGE,
                                                      geo.content_x,
                                                      geo.content_w,
                                                      current_angle_buf),
                             angle_value_y,
                             current_angle_buf);

    motor_main_draw_text_top(u8g2,
                             MOTOR_MAIN_FONT_MEDIUM,
                             geo.content_x,
                             max_row_y,
                             left_max_buf);
    motor_main_draw_text_top(u8g2,
                             MOTOR_MAIN_FONT_MEDIUM,
                             motor_main_right_text_x(u8g2,
                                                     MOTOR_MAIN_FONT_MEDIUM,
                                                     (int16_t)(geo.content_x + geo.content_w),
                                                     right_max_buf),
                             max_row_y,
                             right_max_buf);

    if ((lat_gauge_y - mid_h + 3) > max_row_y)
    {
        motor_main_draw_text_top(u8g2,
                                 MOTOR_MAIN_FONT_MEDIUM,
                                 motor_main_center_text_x(u8g2,
                                                          MOTOR_MAIN_FONT_MEDIUM,
                                                          lat_gauge_x,
                                                          lat_gauge_w,
                                                          lat_value_buf),
                                 (int16_t)(lat_gauge_y - mid_h + 3),
                                 lat_value_buf);
    }

    motor_main_draw_bipolar_gauge_bottom(u8g2,
                                         lat_gauge_rect.x,
                                         lat_gauge_rect.y,
                                         lat_gauge_rect.w,
                                         lat_gauge_rect.h,
                                         lat_value_mg,
                                         g_max_mg,
                                         g_major_step_mg,
                                         g_minor_step_mg,
                                         MOTOR_MAIN_LAT_GAUGE_MAJOR_TICK_H,
                                         MOTOR_MAIN_LAT_GAUGE_MINOR_TICK_H,
                                         MOTOR_MAIN_LAT_GAUGE_BAR_H,
                                         MOTOR_MAIN_LAT_GAUGE_CENTER_TICK_W);
    motor_main_draw_lat_peak_marker(u8g2,
                                    lat_gauge_rect.x,
                                    lat_gauge_rect.y,
                                    lat_gauge_rect.w,
                                    lat_gauge_rect.h,
                                    left_lat_peak_mg,
                                    g_max_mg);
    motor_main_draw_lat_peak_marker(u8g2,
                                    lat_gauge_rect.x,
                                    lat_gauge_rect.y,
                                    lat_gauge_rect.w,
                                    lat_gauge_rect.h,
                                    right_lat_peak_mg,
                                    g_max_mg);
}

static void motor_main_draw_right_section(u8g2_t *u8g2,
                                          int16_t x,
                                          int16_t y,
                                          int16_t w,
                                          int16_t h,
                                          const motor_state_t *state)
{
    const motor_main_text_cache_t *text_cache;
    int32_t raw_speed_x10;
    int32_t raw_max_speed_x10;
    int32_t alt_value;
    int32_t grade_value_x10;
    int32_t display_speed_x10;
    int32_t display_max_speed_x10;
    int32_t display_alt_value;
    int32_t display_grade_x10;
    char speed_buf[16];
    char max_buf[16];
    char alt_buf[24];
    char grade_buf[24];
    const char *speed_unit;
    const char *alt_unit;
    int16_t big_h;
    int16_t mid_h;
    int16_t small_h;
    int16_t unit_w;
    int16_t alt_unit_w;
    int16_t content_x;
    int16_t content_w;
    int16_t content_right_x;
    int16_t top_row_y;
    int16_t stat_y;
    int16_t max_y;
    int16_t max_unit_x;
    int16_t max_unit_y;
    int16_t max_value_right_x;
    int16_t max_value_x;
    int16_t max_icon_x;
    int16_t max_icon_y;
    int16_t speed_max_row_y;
    int16_t speed_right_limit_x;
    int16_t speed_y;
    int16_t speed_x;
    int16_t unit_x;
    int16_t unit_y;
    int16_t left_stat_w;
    int16_t right_stat_x;
    int16_t grade_unit_x;
    int16_t grade_unit_y;
    int16_t grade_slot_w;
    int16_t grade_value_right_x;
    int16_t grade_icon_x;
    int16_t grade_icon_y;
    int16_t percent_w;
    int16_t alt_value_x;
    int16_t alt_value_right_x;
    int16_t alt_value_min_x;
    int16_t alt_unit_x;
    int16_t alt_unit_y;
    int16_t alt_icon_y;
    int16_t alt_icon_x;
    int16_t alt_slot_w;

    if ((u8g2 == 0) || (state == 0) || (w < 56) || (h < 56))
    {
        return;
    }

    text_cache = motor_main_get_text_cache(u8g2);

    raw_speed_x10 = (int32_t)state->nav.speed_kmh_x10;
    raw_max_speed_x10 = (int32_t)state->session.max_speed_kmh_x10;
    grade_value_x10 = state->snapshot.altitude.grade_noimu_x10;
    speed_unit = "km/h";
    alt_unit = Motor_Units_GetAltitudeSuffix(&state->settings.units);
    alt_value = Motor_Units_ConvertAltitudeCm(state->nav.altitude_cm, &state->settings.units);

    display_speed_x10 = motor_main_clamp_i32(raw_speed_x10, 0, 4000);
    display_max_speed_x10 = motor_main_clamp_i32(raw_max_speed_x10, 0, 4000);
    display_alt_value = motor_main_clamp_i32(alt_value, -999, 9999);
    display_grade_x10 = motor_main_clamp_i32(grade_value_x10, -300, 300);

    motor_main_format_speed_integer(speed_buf, sizeof(speed_buf), display_speed_x10);
    (void)snprintf(alt_buf, sizeof(alt_buf), "%ld", (long)display_alt_value);
    motor_main_format_abs_x10_compact(grade_buf, sizeof(grade_buf), display_grade_x10);
    motor_main_format_fixed_unsigned_x10_3(max_buf, sizeof(max_buf), display_max_speed_x10);

    content_x = (int16_t)(x + MOTOR_MAIN_GRADE_GAUGE_W + MOTOR_MAIN_GRADE_GAUGE_GAP_X);
    content_w = (int16_t)(w - MOTOR_MAIN_GRADE_GAUGE_W - MOTOR_MAIN_GRADE_GAUGE_GAP_X);
    if (content_w < 44)
    {
        content_x = x;
        content_w = w;
    }

    big_h = (text_cache != 0) ? text_cache->speed_large_h : motor_main_get_font_height(u8g2, MOTOR_MAIN_FONT_SPEED_LARGE);
    mid_h = (text_cache != 0) ? text_cache->medium_h : motor_main_get_font_height(u8g2, MOTOR_MAIN_FONT_MEDIUM);
    small_h = (text_cache != 0) ? text_cache->small_h : motor_main_get_font_height(u8g2, MOTOR_MAIN_FONT_SMALL);
    unit_w = (text_cache != 0) ? text_cache->speed_unit_w : (int16_t)motor_main_measure_text(u8g2, MOTOR_MAIN_FONT_SMALL, speed_unit);
    alt_unit_w = (int16_t)motor_main_measure_text(u8g2, MOTOR_MAIN_FONT_SMALL, (alt_unit != 0) ? alt_unit : "");
    percent_w = (text_cache != 0) ? text_cache->percent_w : (int16_t)motor_main_measure_text(u8g2, MOTOR_MAIN_FONT_SMALL, "%");
    content_right_x = (int16_t)(content_x + content_w);
    top_row_y = y;
    max_y = (int16_t)(y + 2);
    stat_y = (int16_t)(y + h - mid_h + 1);
    speed_max_row_y = (int16_t)(motor_main_get_bottom_anchor_y(y, h, MOTOR_MAIN_MIDDLE_BOTTOM_GAP_Y) -
                                MOTOR_MAIN_LEFT_BOTTOM_GAUGE_H -
                                mid_h -
                                3);
    speed_y = (int16_t)(y + MOTOR_MAIN_LEFT_SCALE_H + ((speed_max_row_y - (y + MOTOR_MAIN_LEFT_SCALE_H) - big_h) / 2));
    if (speed_y < (int16_t)(y + MOTOR_MAIN_LEFT_SCALE_H + 2))
    {
        speed_y = (int16_t)(y + MOTOR_MAIN_LEFT_SCALE_H + 2);
    }
    speed_y = (int16_t)(speed_y + 4);
    unit_x = (int16_t)(content_right_x - unit_w);
    speed_right_limit_x = (int16_t)(unit_x - 4);
    speed_x = motor_main_right_text_x(u8g2,
                                      MOTOR_MAIN_FONT_SPEED_LARGE,
                                      speed_right_limit_x,
                                      speed_buf);
    unit_y = (int16_t)(speed_y + big_h - small_h - 1);
    if ((unit_y + small_h) > stat_y)
    {
        unit_y = (int16_t)(stat_y - small_h - 1);
    }
    if (unit_y < top_row_y)
    {
        unit_y = top_row_y;
    }
    max_unit_x = (int16_t)(content_right_x - unit_w);
    max_value_right_x = (int16_t)(max_unit_x - 6);
    max_unit_y = (int16_t)(max_y + ((mid_h > small_h) ? (mid_h - small_h) : 0));
    max_value_x = motor_main_right_text_x(u8g2,
                                          MOTOR_MAIN_FONT_MEDIUM,
                                          max_value_right_x,
                                          max_buf);
    max_icon_x = (int16_t)(max_value_x - MOTOR_UI_XBM_SPEED_MAX_MARK_W - 2);
    max_icon_y = (int16_t)(max_y + ((mid_h - MOTOR_UI_XBM_SPEED_MAX_MARK_H) / 2));
    if (max_icon_x < content_x)
    {
        max_icon_x = content_x;
    }
    if ((max_icon_x + MOTOR_UI_XBM_SPEED_MAX_MARK_W + 2) > max_value_x)
    {
        max_icon_x = (int16_t)(max_value_x - MOTOR_UI_XBM_SPEED_MAX_MARK_W - 2);
    }

    if (content_x > x)
    {
        motor_main_draw_vertical_bipolar_gauge(u8g2,
                                               x,
                                               (int16_t)(y - 1),
                                               MOTOR_MAIN_GRADE_GAUGE_W,
                                               (int16_t)(h + MOTOR_MAIN_MIDDLE_BOTTOM_GAP_Y + 2),
                                               display_grade_x10,
                                               MOTOR_MAIN_GRADE_MAX_X10,
                                               MOTOR_MAIN_GRADE_MAJOR_STEP_X10,
                                               MOTOR_MAIN_GRADE_MINOR_STEP_X10);
    }

    u8g2_DrawXBM(u8g2,
                 (u8g2_uint_t)max_icon_x,
                 (u8g2_uint_t)max_icon_y,
                 MOTOR_UI_XBM_SPEED_MAX_MARK_W,
                 MOTOR_UI_XBM_SPEED_MAX_MARK_H,
                 motor_ui_xbm_speed_max_mark_bits);
    motor_main_draw_text_top(u8g2,
                             MOTOR_MAIN_FONT_MEDIUM,
                             max_value_x,
                             max_y,
                             max_buf);
    motor_main_draw_text_top(u8g2, MOTOR_MAIN_FONT_SMALL, max_unit_x, max_unit_y, speed_unit);
    motor_main_draw_text_top(u8g2, MOTOR_MAIN_FONT_SPEED_LARGE, speed_x, speed_y, speed_buf);
    if (unit_y >= top_row_y)
    {
        motor_main_draw_text_top(u8g2, MOTOR_MAIN_FONT_SMALL, unit_x, unit_y, speed_unit);
    }

    left_stat_w = (int16_t)(content_w / 2);
    right_stat_x = (int16_t)(content_x + left_stat_w);
    grade_slot_w = (text_cache != 0) ? text_cache->grade_slot_w :
                                       (int16_t)motor_main_measure_text(u8g2, MOTOR_MAIN_FONT_MEDIUM, "00.0");
    grade_value_right_x = (int16_t)(right_stat_x - percent_w - MOTOR_MAIN_VALUE_UNIT_GAP_X - 2);
    grade_icon_x = (int16_t)((grade_value_right_x - grade_slot_w) - MOTOR_UI_XBM_GRADE_W - 2);
    grade_icon_y = (int16_t)(stat_y + ((mid_h - MOTOR_UI_XBM_GRADE_H) / 2));
    grade_unit_x = (int16_t)(grade_value_right_x + MOTOR_MAIN_VALUE_UNIT_GAP_X);
    grade_unit_y = (int16_t)(stat_y + ((mid_h > small_h) ? (mid_h - small_h) : 0));
    alt_icon_x = right_stat_x;
    alt_icon_y = (int16_t)(stat_y + ((mid_h - MOTOR_UI_XBM_ALTITUDE_H) / 2));
    alt_unit_x = (int16_t)(content_right_x - alt_unit_w);
    alt_value_right_x = (int16_t)(alt_unit_x - ((alt_unit_w > 0) ? MOTOR_MAIN_VALUE_UNIT_GAP_X : 0));
    alt_slot_w = (text_cache != 0) ? text_cache->altitude_slot_w :
                                     (int16_t)motor_main_measure_text(u8g2, MOTOR_MAIN_FONT_MEDIUM, "9999");
    alt_value_x = (int16_t)(alt_value_right_x - alt_slot_w);
    alt_icon_x = (int16_t)(alt_value_x - MOTOR_UI_XBM_ALTITUDE_W - 4);
    alt_value_min_x = (int16_t)(alt_icon_x + MOTOR_UI_XBM_ALTITUDE_W + 2);
    if (alt_value_x < alt_value_min_x)
    {
        alt_value_x = alt_value_min_x;
    }
    alt_unit_y = (int16_t)(stat_y + ((mid_h > small_h) ? (mid_h - small_h) : 0));

    u8g2_DrawXBM(u8g2,
                 (u8g2_uint_t)grade_icon_x,
                 (u8g2_uint_t)grade_icon_y,
                 MOTOR_UI_XBM_GRADE_W,
                 MOTOR_UI_XBM_GRADE_H,
                 motor_ui_xbm_grade_bits);
    motor_main_draw_text_top(u8g2,
                             MOTOR_MAIN_FONT_MEDIUM,
                             motor_main_right_text_x(u8g2,
                                                     MOTOR_MAIN_FONT_MEDIUM,
                                                     grade_value_right_x,
                                                     grade_buf),
                             stat_y,
                             grade_buf);
    motor_main_draw_text_top(u8g2, MOTOR_MAIN_FONT_SMALL, grade_unit_x, grade_unit_y, "%");
    u8g2_DrawXBM(u8g2,
                 (u8g2_uint_t)alt_icon_x,
                 (u8g2_uint_t)alt_icon_y,
                 MOTOR_UI_XBM_ALTITUDE_W,
                 MOTOR_UI_XBM_ALTITUDE_H,
                 motor_ui_xbm_altitude_bits);
    motor_main_draw_text_top(u8g2,
                             MOTOR_MAIN_FONT_MEDIUM,
                             motor_main_right_text_x(u8g2,
                                                     MOTOR_MAIN_FONT_MEDIUM,
                                                     (int16_t)(alt_value_x + alt_slot_w),
                                                     alt_buf),
                             stat_y,
                             alt_buf);
    if ((alt_unit != 0) && (alt_unit[0] != '\0'))
    {
        motor_main_draw_text_top(u8g2, MOTOR_MAIN_FONT_SMALL, alt_unit_x, alt_unit_y, alt_unit);
    }
}


static motor_main_bottom_mode_t motor_main_get_bottom_mode(const motor_state_t *state)
{
    if (state == 0)
    {
        return MOTOR_MAIN_BOTTOM_TRIP_A;
    }

    if (state->ui.bottom_page >= (uint8_t)MOTOR_MAIN_BOTTOM_COUNT)
    {
        return MOTOR_MAIN_BOTTOM_TRIP_A;
    }

    return (motor_main_bottom_mode_t)state->ui.bottom_page;
}

static uint8_t motor_main_get_bottom_window_map(motor_main_bottom_mode_t mode,
                                                uint8_t *out_page_index,
                                                uint8_t *out_slot_start)
{
    if ((out_page_index == 0) || (out_slot_start == 0))
    {
        return 0u;
    }

    switch (mode)
    {
    case MOTOR_MAIN_BOTTOM_USER_1:
        *out_page_index = 0u;
        *out_slot_start = 0u;
        return 1u;
    case MOTOR_MAIN_BOTTOM_USER_2:
        *out_page_index = 0u;
        *out_slot_start = 5u;
        return 1u;
    case MOTOR_MAIN_BOTTOM_USER_3:
        *out_page_index = 0u;
        *out_slot_start = 10u;
        return 1u;
    default:
        break;
    }

    return 0u;
}

static uint8_t motor_main_load_trip_metrics(const motor_state_t *state,
                                            motor_main_bottom_mode_t mode,
                                            motor_trip_metrics_t *out_trip)
{
    if ((state == 0) || (out_trip == 0))
    {
        return 0u;
    }

    memset(out_trip, 0, sizeof(*out_trip));

    switch (mode)
    {
    case MOTOR_MAIN_BOTTOM_TRIP_A:
        *out_trip = state->session.trip_a_stats;
        out_trip->distance_m = state->session.trip_a_m;
        return 1u;
    case MOTOR_MAIN_BOTTOM_TRIP_B:
        *out_trip = state->session.trip_b_stats;
        out_trip->distance_m = state->session.trip_b_m;
        return 1u;
    case MOTOR_MAIN_BOTTOM_CURRENT_RECORD:
        out_trip->distance_m = state->record_session.distance_m;
        out_trip->moving_seconds = state->record_session.moving_seconds;
        out_trip->ride_seconds = state->record_session.ride_seconds;
        out_trip->max_speed_kmh_x10 = state->record_session.max_speed_kmh_x10;
        return 1u;
    case MOTOR_MAIN_BOTTOM_REFUEL:
        *out_trip = state->session.trip_refuel;
        return 1u;
    case MOTOR_MAIN_BOTTOM_TODAY:
        *out_trip = state->session.trip_today;
        return 1u;
    case MOTOR_MAIN_BOTTOM_IGNITION:
        *out_trip = state->session.trip_ignition;
        return 1u;
    default:
        break;
    }

    return 0u;
}

static void motor_main_draw_bottom_metric(u8g2_t *u8g2,
                                          int16_t x,
                                          int16_t y,
                                          int16_t w,
                                          int16_t h,
                                          const char *label,
                                          const char *value)
{
    const motor_main_text_cache_t *text_cache;
    char label_buf[16];
    char value_buf[24];
    int16_t label_y;
    int16_t value_y;
    int16_t small_h;
    int16_t mid_h;

    if ((u8g2 == 0) || (w < 16) || (h < 14))
    {
        return;
    }

    text_cache = motor_main_get_text_cache(u8g2);
    small_h = (text_cache != 0) ? text_cache->small_h : motor_main_get_font_height(u8g2, MOTOR_MAIN_FONT_SMALL);
    mid_h = (text_cache != 0) ? text_cache->medium_h : motor_main_get_font_height(u8g2, MOTOR_MAIN_FONT_MEDIUM);
    label_y = (int16_t)(y - 1);
    value_y = (int16_t)(y + small_h + 2);
    if ((value_y + mid_h) > (y + h))
    {
        value_y = (int16_t)(y + h - mid_h);
    }

    u8g2_SetFont(u8g2, MOTOR_MAIN_FONT_SMALL);
    motor_main_copy_fit(u8g2, (label != 0) ? label : "", label_buf, sizeof(label_buf), (int16_t)(w - 4));
    u8g2_SetFont(u8g2, MOTOR_MAIN_FONT_MEDIUM);
    motor_main_copy_fit(u8g2, (value != 0) ? value : "", value_buf, sizeof(value_buf), (int16_t)(w - 4));

    if (small_h > 0)
    {
        u8g2_DrawBox(u8g2, x, y, (uint8_t)w, (uint8_t)small_h);
        u8g2_SetDrawColor(u8g2, 0);
    }

    motor_main_draw_text_top(u8g2,
                             MOTOR_MAIN_FONT_SMALL,
                             motor_main_center_text_x(u8g2, MOTOR_MAIN_FONT_SMALL, x, w, label_buf),
                             label_y,
                             label_buf);
    u8g2_SetDrawColor(u8g2, 1);
    motor_main_draw_text_top(u8g2,
                             MOTOR_MAIN_FONT_MEDIUM,
                             motor_main_center_text_x(u8g2, MOTOR_MAIN_FONT_MEDIUM, x, w, value_buf),
                             value_y,
                             value_buf);
}

static void motor_main_draw_bottom_metric_icon(u8g2_t *u8g2,
                                               int16_t x,
                                               int16_t y,
                                               int16_t w,
                                               int16_t h,
                                               const unsigned char *icon_bits,
                                               uint8_t icon_w,
                                               uint8_t icon_h,
                                               const char *value)
{
    const motor_main_text_cache_t *text_cache;
    char value_buf[24];
    int16_t icon_x;
    int16_t icon_y;
    int16_t value_y;
    int16_t small_h;
    int16_t mid_h;

    if ((u8g2 == 0) || (w < 16) || (h < 14))
    {
        return;
    }

    text_cache = motor_main_get_text_cache(u8g2);
    small_h = (text_cache != 0) ? text_cache->small_h : motor_main_get_font_height(u8g2, MOTOR_MAIN_FONT_SMALL);
    mid_h = (text_cache != 0) ? text_cache->medium_h : motor_main_get_font_height(u8g2, MOTOR_MAIN_FONT_MEDIUM);
    value_y = (int16_t)(y + small_h + 2);
    if ((value_y + mid_h) > (y + h))
    {
        value_y = (int16_t)(y + h - mid_h);
    }

    u8g2_SetFont(u8g2, MOTOR_MAIN_FONT_MEDIUM);
    motor_main_copy_fit(u8g2, (value != 0) ? value : "", value_buf, sizeof(value_buf), (int16_t)(w - 4));

    if (small_h > 0)
    {
        u8g2_DrawBox(u8g2, x, y, (uint8_t)w, (uint8_t)small_h);
        if ((icon_bits != 0) && (icon_w != 0u) && (icon_h != 0u))
        {
            icon_x = (int16_t)(x + ((w - (int16_t)icon_w) / 2));
            icon_y = (int16_t)(y + ((small_h - (int16_t)icon_h) / 2));
            u8g2_SetDrawColor(u8g2, 0);
            u8g2_DrawXBM(u8g2, (u8g2_uint_t)icon_x, (u8g2_uint_t)icon_y, icon_w, icon_h, icon_bits);
        }
        u8g2_SetDrawColor(u8g2, 1);
    }

    motor_main_draw_text_top(u8g2,
                             MOTOR_MAIN_FONT_MEDIUM,
                             motor_main_center_text_x(u8g2, MOTOR_MAIN_FONT_MEDIUM, x, w, value_buf),
                             value_y,
                             value_buf);
}

static void motor_main_draw_info_with_icon(u8g2_t *u8g2,
                                           int16_t x,
                                           int16_t y,
                                           const unsigned char *bits,
                                           uint8_t icon_w,
                                           uint8_t icon_h,
                                           const char *text)
{
    const motor_main_text_cache_t *text_cache;
    int16_t icon_y;
    int16_t text_x;
    int16_t mid_h;

    if ((u8g2 == 0) || (text == 0))
    {
        return;
    }

    text_cache = motor_main_get_text_cache(u8g2);
    mid_h = (text_cache != 0) ? text_cache->medium_h : motor_main_get_font_height(u8g2, MOTOR_MAIN_FONT_MEDIUM);
    icon_y = (int16_t)(y + ((mid_h - (int16_t)icon_h) / 2));
    if ((bits != 0) && (icon_w != 0u) && (icon_h != 0u))
    {
        u8g2_DrawXBM(u8g2, (u8g2_uint_t)x, (u8g2_uint_t)icon_y, icon_w, icon_h, bits);
    }
    text_x = (int16_t)(x + icon_w + 3);
    motor_main_draw_text_top(u8g2, MOTOR_MAIN_FONT_MEDIUM, text_x, y, text);
}

static void motor_main_make_fixed_slot_string(char *out_text,
                                              size_t out_size,
                                              const char *value,
                                              const char *slot_text)
{
    size_t value_len;
    size_t slot_len;
    size_t pad_len;
    size_t i;

    if ((out_text == 0) || (out_size == 0u))
    {
        return;
    }

    out_text[0] = '\0';
    if (value == 0)
    {
        return;
    }

    if ((slot_text == 0) || (slot_text[0] == '\0'))
    {
        (void)snprintf(out_text, out_size, "%s", value);
        return;
    }

    value_len = strlen(value);
    slot_len = strlen(slot_text);
    pad_len = (slot_len > value_len) ? (slot_len - value_len) : 0u;

    if ((pad_len + value_len + 1u) > out_size)
    {
        (void)snprintf(out_text, out_size, "%s", value);
        return;
    }

    for (i = 0u; i < pad_len; ++i)
    {
        out_text[i] = ' ';
    }
    (void)snprintf(&out_text[pad_len], out_size - pad_len, "%s", value);
}

static int16_t motor_main_measure_trip_info_required_w(u8g2_t *u8g2,
                                                       const char *slot_text,
                                                       const char *unit,
                                                       uint8_t icon_w)
{
    int16_t slot_w;
    int16_t unit_w;

    if (u8g2 == 0)
    {
        return 0;
    }

    slot_w = (int16_t)motor_main_measure_text(u8g2,
                                              MOTOR_MAIN_FONT_MEDIUM,
                                              (slot_text != 0) ? slot_text : "");
    unit_w = (int16_t)motor_main_measure_text(u8g2,
                                              MOTOR_MAIN_FONT_SMALL,
                                              (unit != 0) ? unit : "");
    return (int16_t)(2 + (int16_t)icon_w + 2 + slot_w +
                     ((unit_w > 0) ? (MOTOR_MAIN_VALUE_UNIT_GAP_X + unit_w) : 0) + 2);
}

static void motor_main_draw_trip_info_fixed(u8g2_t *u8g2,
                                            int16_t x,
                                            int16_t y,
                                            int16_t w,
                                            const unsigned char *bits,
                                            uint8_t icon_w,
                                            uint8_t icon_h,
                                            const char *value,
                                            const char *unit,
                                            const char *slot_text)
{
    const motor_main_text_cache_t *text_cache;
    char draw_buf[24];
    int16_t mid_h;
    int16_t small_h;
    int16_t icon_y;
    int16_t unit_w;
    int16_t unit_x;
    int16_t unit_y;
    int16_t right_x;
    int16_t value_x;

    if ((u8g2 == 0) || (value == 0) || (w < 12))
    {
        return;
    }

    text_cache = motor_main_get_text_cache(u8g2);
    motor_main_make_fixed_slot_string(draw_buf, sizeof(draw_buf), value, slot_text);
    mid_h = (text_cache != 0) ? text_cache->medium_h : motor_main_get_font_height(u8g2, MOTOR_MAIN_FONT_MEDIUM);
    small_h = (text_cache != 0) ? text_cache->small_h : motor_main_get_font_height(u8g2, MOTOR_MAIN_FONT_SMALL);
    icon_y = (int16_t)(y + ((mid_h - (int16_t)icon_h) / 2));
    if ((bits != 0) && (icon_w != 0u) && (icon_h != 0u))
    {
        u8g2_DrawXBM(u8g2, (u8g2_uint_t)(x + 2), (u8g2_uint_t)icon_y, icon_w, icon_h, bits);
    }

    right_x = (int16_t)(x + w - 2);
    unit_w = (int16_t)motor_main_measure_text(u8g2, MOTOR_MAIN_FONT_SMALL, (unit != 0) ? unit : "");
    if ((unit != 0) && (unit[0] != '\0') && (unit_w > 0))
    {
        unit_x = (int16_t)(right_x - unit_w);
        unit_y = (int16_t)(y + ((mid_h - small_h) / 2));
        motor_main_draw_text_top(u8g2, MOTOR_MAIN_FONT_SMALL, unit_x, unit_y, unit);
        right_x = (int16_t)(unit_x - MOTOR_MAIN_VALUE_UNIT_GAP_X);
    }

    value_x = motor_main_right_text_x(u8g2, MOTOR_MAIN_FONT_MEDIUM, right_x, draw_buf);
    motor_main_draw_text_top(u8g2, MOTOR_MAIN_FONT_MEDIUM, value_x, y, draw_buf);
}

static const unsigned char *motor_main_get_trip_big_icon_bits(motor_main_bottom_mode_t mode,
                                                              uint8_t *out_w,
                                                              uint8_t *out_h)
{
    if ((out_w == 0) || (out_h == 0))
    {
        return 0;
    }

    switch (mode)
    {
    case MOTOR_MAIN_BOTTOM_TRIP_A:
        *out_w = MOTOR_UI_XBM_TRIP_A_BIG_W;
        *out_h = MOTOR_UI_XBM_TRIP_A_BIG_H;
        return motor_ui_xbm_trip_a_big_bits;
    case MOTOR_MAIN_BOTTOM_TRIP_B:
        *out_w = MOTOR_UI_XBM_TRIP_B_BIG_W;
        *out_h = MOTOR_UI_XBM_TRIP_B_BIG_H;
        return motor_ui_xbm_trip_b_big_bits;
    case MOTOR_MAIN_BOTTOM_CURRENT_RECORD:
        *out_w = MOTOR_UI_XBM_TRIP_REC_BIG_W;
        *out_h = MOTOR_UI_XBM_TRIP_REC_BIG_H;
        return motor_ui_xbm_trip_rec_big_bits;
    case MOTOR_MAIN_BOTTOM_REFUEL:
        *out_w = MOTOR_UI_XBM_TRIP_REFUEL_BIG_W;
        *out_h = MOTOR_UI_XBM_TRIP_REFUEL_BIG_H;
        return motor_ui_xbm_trip_refuel_big_bits;
    case MOTOR_MAIN_BOTTOM_TODAY:
        *out_w = MOTOR_UI_XBM_TRIP_TODAY_BIG_W;
        *out_h = MOTOR_UI_XBM_TRIP_TODAY_BIG_H;
        return motor_ui_xbm_trip_today_big_bits;
    case MOTOR_MAIN_BOTTOM_IGNITION:
        *out_w = MOTOR_UI_XBM_TRIP_IGN_BIG_W;
        *out_h = MOTOR_UI_XBM_TRIP_IGN_BIG_H;
        return motor_ui_xbm_trip_ign_big_bits;
    default:
        break;
    }

    *out_w = 0u;
    *out_h = 0u;
    return 0;
}

 void motor_main_draw_small_line(u8g2_t *u8g2,
                                       int16_t x,
                                       int16_t y,
                                       int16_t w,
                                       const char *text,
                                       uint8_t align_right)
{
    char draw_buf[32];
    int16_t draw_x;

    if ((u8g2 == 0) || (text == 0) || (w < 8))
    {
        return;
    }

    u8g2_SetFont(u8g2, MOTOR_MAIN_FONT_SMALL);
    motor_main_copy_fit(u8g2, text, draw_buf, sizeof(draw_buf), (int16_t)(w - 2));

    draw_x = x;
    if (align_right != 0u)
    {
        draw_x = motor_main_right_text_x(u8g2,
                                         MOTOR_MAIN_FONT_SMALL,
                                         (int16_t)(x + w),
                                         draw_buf);
    }

    motor_main_draw_text_top(u8g2, MOTOR_MAIN_FONT_SMALL, draw_x, y, draw_buf);
}

static void motor_main_draw_configurable_bottom_window(u8g2_t *u8g2,
                                                       int16_t x,
                                                       int16_t y,
                                                       int16_t w,
                                                       int16_t h,
                                                       const motor_state_t *state,
                                                       motor_main_bottom_mode_t mode)
{
    uint8_t page_index;
    uint8_t slot_start;
    uint8_t i;
    int16_t content_y;
    int16_t content_h;
    int16_t cell_w;
    int16_t cell_x;
    int16_t right_x;

    if ((u8g2 == 0) || (state == 0))
    {
        return;
    }

    if (motor_main_get_bottom_window_map(mode, &page_index, &slot_start) == 0u)
    {
        return;
    }

    content_y = (int16_t)(y + 1);
    content_h = (int16_t)(h - 2);
    if (content_h < 14)
    {
        return;
    }

    cell_w = (int16_t)(w / 5);
    if (cell_w < 20)
    {
        cell_w = 20;
    }

    for (i = 0u; i < MOTOR_MAIN_USER_CELL_COUNT; i++)
    {
        motor_data_field_text_t text;
        motor_data_field_id_t field_id;
        int16_t draw_w;

        cell_x = (int16_t)(x + (cell_w * i));
        right_x = (i == (MOTOR_MAIN_USER_CELL_COUNT - 1u)) ? (int16_t)(x + w) : (int16_t)(cell_x + cell_w);
        draw_w = (int16_t)(right_x - cell_x);
        field_id = (motor_data_field_id_t)state->settings.data_fields[page_index][slot_start + i];
        Motor_DataField_Format(field_id, state, &text);

        motor_main_draw_bottom_metric(u8g2,
                                      cell_x,
                                      content_y,
                                      draw_w,
                                      content_h,
                                      text.label,
                                      text.value);
    }

    u8g2_DrawHLine(u8g2, x, y, (uint8_t)w);
    if (h > 1)
    {
        u8g2_DrawVLine(u8g2, x, y, (uint8_t)h);
        u8g2_DrawVLine(u8g2, (int16_t)(x + w - 1), y, (uint8_t)h);
    }

    for (i = 1u; i < MOTOR_MAIN_USER_CELL_COUNT; i++)
    {
        cell_x = (int16_t)(x + (cell_w * i));
        if (cell_x < (int16_t)(x + w - 1))
        {
            u8g2_DrawVLine(u8g2, cell_x, y, (uint8_t)h);
        }
    }
}

static void motor_main_draw_trip_bottom_window(u8g2_t *u8g2,
                                               int16_t x,
                                               int16_t y,
                                               int16_t w,
                                               int16_t h,
                                               const motor_state_t *state,
                                               motor_main_bottom_mode_t mode)
{
    motor_trip_metrics_t trip;
    char dist_text[24];
    char avg_text[16];
    char ride_text[16];
    int16_t line_y;
    int16_t info_y;
    int16_t icon_x;
    int16_t icon_y;
    int16_t icon_w_px;
    int16_t icon_h_px;
    int16_t content_x;
    int16_t col0_x;
    int16_t col1_x;
    int16_t col2_x;
    int16_t col0_w;
    int16_t col1_w;
    int16_t col2_w;
    int16_t col_total_w;
    int16_t req0_w;
    int16_t req1_w;
    int16_t req2_w;
    int16_t line0_x;
    int16_t line1_x;
    int16_t line2_x;
    int16_t line3_x;
    int16_t line4_x;
    int16_t icon_area_w;
    uint8_t big_icon_w;
    uint8_t big_icon_h;
    const unsigned char *big_icon_bits;
    const char *distance_unit;
    uint8_t top_band_h;
    int16_t border_w;
    int16_t avg_pad_w;
    int16_t time_pad_w;

    if ((u8g2 == 0) || (state == 0))
    {
        return;
    }

    if (motor_main_load_trip_metrics(state, mode, &trip) == 0u)
    {
        return;
    }

    motor_main_format_trip_distance(dist_text, sizeof(dist_text), trip.distance_m, &state->settings.units);
    motor_main_format_unsigned_x10_compact(avg_text,
                                           sizeof(avg_text),
                                           motor_main_clamp_i32(motor_main_get_average_speed_from_trip(trip.distance_m,
                                                                                                       trip.moving_seconds,
                                                                                                       trip.ride_seconds),
                                                                0,
                                                                4000));
    motor_main_format_seconds(ride_text, sizeof(ride_text), trip.ride_seconds);
    big_icon_bits = motor_main_get_trip_big_icon_bits(mode, &big_icon_w, &big_icon_h);
    distance_unit = Motor_Units_GetDistanceSuffix(&state->settings.units);
    top_band_h = 5u;
    border_w = 1;
    avg_pad_w = 4;
    time_pad_w = 4;
    line_y = y;
    u8g2_DrawBox(u8g2, x, line_y, (uint8_t)w, top_band_h);

    icon_w_px = (int16_t)big_icon_w;
    icon_h_px = (int16_t)big_icon_h;
    line0_x = x;
    icon_x = (int16_t)(x + border_w + 4);
    icon_y = (int16_t)(y + top_band_h + ((h - (int16_t)top_band_h - icon_h_px) / 2));
    if ((big_icon_bits != 0) && (big_icon_w != 0u) && (big_icon_h != 0u))
    {
        u8g2_DrawXBM(u8g2,
                     (u8g2_uint_t)icon_x,
                     (u8g2_uint_t)icon_y,
                     big_icon_w,
                     big_icon_h,
                     big_icon_bits);
    }

    icon_area_w = (int16_t)(icon_w_px + 8);
    line1_x = (int16_t)(x + border_w + icon_area_w);
    content_x = (int16_t)(line1_x + border_w);
    line4_x = (int16_t)(x + w - border_w);
    col_total_w = (int16_t)(line4_x - content_x - (border_w * 2));
    req0_w = motor_main_measure_trip_info_required_w(u8g2, "9999.9", distance_unit, MOTOR_UI_XBM_DISTANCE_W);
    req1_w = (int16_t)(motor_main_measure_trip_info_required_w(u8g2, "400.0", "", MOTOR_UI_XBM_INFO_AVG_W) + avg_pad_w);
    req2_w = (int16_t)(motor_main_measure_trip_info_required_w(u8g2, "00:00:00", "", MOTOR_UI_XBM_INFO_TIME_W) + time_pad_w);

    if (col_total_w < (int16_t)(req0_w + req1_w + req2_w))
    {
        col_total_w = (int16_t)(req0_w + req1_w + req2_w);
    }

    col1_w = req1_w;
    col2_w = req2_w;
    col0_w = (int16_t)(col_total_w - col1_w - col2_w);
    if (col0_w < req0_w)
    {
        col0_w = req0_w;
    }

    line2_x = (int16_t)(content_x + col0_w);
    line3_x = (int16_t)(line2_x + border_w + col1_w);
    info_y = (int16_t)(y + 10);
    col0_x = content_x;
    col1_x = (int16_t)(line2_x + border_w);
    col2_x = (int16_t)(line3_x + border_w);

    u8g2_DrawBox(u8g2, line0_x, y, (uint8_t)border_w, (uint8_t)h);
    u8g2_DrawBox(u8g2, line1_x, y, (uint8_t)border_w, (uint8_t)h);
    u8g2_DrawBox(u8g2, line2_x, y, (uint8_t)border_w, (uint8_t)h);
    u8g2_DrawBox(u8g2, line3_x, y, (uint8_t)border_w, (uint8_t)h);
    u8g2_DrawBox(u8g2, line4_x, y, (uint8_t)border_w, (uint8_t)h);

    motor_main_draw_trip_info_fixed(u8g2,
                                    col0_x,
                                    info_y,
                                    col0_w,
                                    motor_ui_xbm_distance_bits,
                                    MOTOR_UI_XBM_DISTANCE_W,
                                    MOTOR_UI_XBM_DISTANCE_H,
                                    dist_text,
                                    distance_unit,
                                    "9999.9");
    motor_main_draw_trip_info_fixed(u8g2,
                                    col1_x,
                                    info_y,
                                    col1_w,
                                    motor_ui_xbm_info_avg_bits,
                                    MOTOR_UI_XBM_INFO_AVG_W,
                                    MOTOR_UI_XBM_INFO_AVG_H,
                                    avg_text,
                                    "",
                                    "400.0");
    motor_main_draw_trip_info_fixed(u8g2,
                                    col2_x,
                                    info_y,
                                    col2_w,
                                    motor_ui_xbm_info_time_bits,
                                    MOTOR_UI_XBM_INFO_TIME_W,
                                    MOTOR_UI_XBM_INFO_TIME_H,
                                    ride_text,
                                    "",
                                    0);
}

static void motor_main_draw_bottom_strip(u8g2_t *u8g2,
                                         int16_t x,
                                         int16_t y,
                                         int16_t w,
                                         int16_t h,
                                         const motor_state_t *state)
{
    motor_main_bottom_mode_t mode;

    if ((u8g2 == 0) || (state == 0) || (w < 40) || (h < 16))
    {
        return;
    }

    mode = motor_main_get_bottom_mode(state);
    if ((mode == MOTOR_MAIN_BOTTOM_USER_1) ||
        (mode == MOTOR_MAIN_BOTTOM_USER_2) ||
        (mode == MOTOR_MAIN_BOTTOM_USER_3))
    {
        motor_main_draw_configurable_bottom_window(u8g2, x, y, w, h, state, mode);
    }
    else
    {
        motor_main_draw_trip_bottom_window(u8g2, x, y, w, h, state, mode);
    }
}


static void motor_main_draw_page1_top_area(u8g2_t *u8g2,
                                           const ui_rect_t *section,
                                           const motor_state_t *state)
{
    if (section == 0)
    {
        return;
    }

    motor_main_draw_top_section(u8g2, section->x, section->y, section->w, section->h, state);
}

static void motor_main_draw_page1_left_area(u8g2_t *u8g2,
                                            const ui_rect_t *section,
                                            const motor_state_t *state)
{
    if (section == 0)
    {
        return;
    }

    motor_main_draw_left_section(u8g2, section->x, section->y, section->w, section->h, state);
}

static void motor_main_draw_page1_right_area(u8g2_t *u8g2,
                                             const ui_rect_t *section,
                                             const motor_state_t *state)
{
    if (section == 0)
    {
        return;
    }

    motor_main_draw_right_section(u8g2, section->x, section->y, section->w, section->h, state);
}

static void motor_main_draw_page1_bottom_area(u8g2_t *u8g2,
                                              const ui_rect_t *section,
                                              const motor_state_t *state)
{
    if (section == 0)
    {
        return;
    }

    motor_main_draw_bottom_strip(u8g2, section->x, section->y, section->w, section->h, state);
}

void Motor_UI_Main_GetTopGaugeLayout(const ui_rect_t *viewport,
                                     ui_rect_t *out_top_rect,
                                     int16_t *out_divider_y)
{
    int16_t top_h;
    int16_t gauge_y;

    if ((viewport == 0) || (out_top_rect == 0))
    {
        return;
    }

    top_h = (int16_t)(MOTOR_MAIN_TOP_GAUGE_MAJOR_TICK_H + 1);
    gauge_y = (int16_t)(viewport->y + MOTOR_MAIN_TOP_SECTION_OFFSET_Y);

    out_top_rect->x = viewport->x;
    out_top_rect->y = gauge_y;
    out_top_rect->w = viewport->w;
    out_top_rect->h = top_h;

    if (out_divider_y != 0)
    {
        *out_divider_y = (int16_t)(gauge_y + top_h + MOTOR_MAIN_TOP_MIDDLE_GAP_Y);
    }
}

void Motor_UI_Main_DrawTopGauge(u8g2_t *u8g2,
                                const ui_rect_t *section,
                                const motor_state_t *state)
{
    if (section == 0)
    {
        return;
    }

    motor_main_draw_top_section(u8g2, section->x, section->y, section->w, section->h, state);
}

void Motor_UI_DrawScreen_Main(u8g2_t *u8g2,
                              const ui_rect_t *viewport,
                              const motor_state_t *state)
{
    int16_t outer_x;
    int16_t outer_y;
    int16_t outer_w;
    int16_t gauge_y;
    int16_t top_h;
    int16_t middle_divider_y;
    int16_t middle_y;
    int16_t middle_h;
    int16_t bottom_y;
    int16_t bottom_h;
    int16_t split_x;
    int16_t left_x;
    int16_t left_w;
    int16_t right_x;
    int16_t right_w;
    ui_rect_t top_rect;
    ui_rect_t left_rect;
    ui_rect_t right_rect;
    ui_rect_t bottom_rect;

    if ((u8g2 == 0) || (viewport == 0) || (state == 0))
    {
        return;
    }

    top_h = (int16_t)(MOTOR_MAIN_TOP_GAUGE_MAJOR_TICK_H + 1);
    if (viewport->h < (MOTOR_MAIN_TOP_SECTION_OFFSET_Y +
                       top_h +
                       MOTOR_MAIN_TOP_MIDDLE_GAP_Y +
                       MOTOR_MAIN_MIDDLE_BOTTOM_GAP_Y +
                       MOTOR_MAIN_BOTTOM_RIBBON_H +
                       30))
    {
        Motor_UI_DrawCenteredTextBlock(u8g2,
                                       viewport,
                                       (int16_t)((viewport->h / 2) - 14),
                                       "PAGE 1",
                                       "Viewport too small",
                                       "Need taller layout");
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  MAIN root viewport itself is owned elsewhere.                          */
    /*  Here we only redistribute the internal sections so they span the full  */
    /*  width of the already-assigned viewport and remove the old vertical     */
    /*  dead space between sections.                                           */
    /* ---------------------------------------------------------------------- */
    outer_x = viewport->x;
    outer_y = viewport->y;
    outer_w = viewport->w;

    Motor_UI_Main_GetTopGaugeLayout(viewport, &top_rect, &middle_divider_y);
    gauge_y = top_rect.y;
    middle_y = (int16_t)(middle_divider_y + 1);

    /* Preserve the existing dynamics/speed section height while moving it up. */
    middle_h = (int16_t)(viewport->h - MOTOR_MAIN_TOP_GAUGE_H - MOTOR_MAIN_BOTTOM_RIBBON_H - 7);
    bottom_y = (int16_t)(middle_y + middle_h + MOTOR_MAIN_MIDDLE_BOTTOM_GAP_Y);
    bottom_h = (int16_t)((viewport->y + viewport->h) - bottom_y);

    split_x = (int16_t)(viewport->x + (viewport->w / 2));
    left_x = outer_x;
    left_w = (int16_t)(split_x - outer_x);
    right_x = split_x;
    right_w = (int16_t)((outer_x + outer_w) - right_x);

    left_rect.x = (int16_t)(left_x - 1);
    left_rect.y = middle_y;
    left_rect.w = (int16_t)(left_w + 1);
    left_rect.h = middle_h;

    right_rect.x = right_x;
    right_rect.y = middle_y;
    right_rect.w = right_w;
    right_rect.h = middle_h;

    bottom_rect.x = outer_x;
    bottom_rect.y = bottom_y;
    bottom_rect.w = outer_w;
    bottom_rect.h = bottom_h;

    motor_main_draw_page1_top_area(u8g2, &top_rect, state);
    u8g2_DrawHLine(u8g2, outer_x, middle_divider_y, (uint8_t)outer_w);

    u8g2_DrawVLine(u8g2, split_x, middle_y, (int16_t)(bottom_y - middle_y + 1));

    motor_main_draw_page1_left_area(u8g2, &left_rect, state);
    motor_main_draw_page1_right_area(u8g2, &right_rect, state);
    motor_main_draw_page1_bottom_area(u8g2, &bottom_rect, state);
}
