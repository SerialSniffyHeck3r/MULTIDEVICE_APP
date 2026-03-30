#include "Motor_UI_Internal.h"
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

#ifndef MOTOR_MAIN_TOP_GAUGE_H
#define MOTOR_MAIN_TOP_GAUGE_H            18
#endif

#ifndef MOTOR_MAIN_BOTTOM_RIBBON_H
#define MOTOR_MAIN_BOTTOM_RIBBON_H        26
#endif

#ifndef MOTOR_MAIN_OUTER_PAD_X
#define MOTOR_MAIN_OUTER_PAD_X             4
#endif

#ifndef MOTOR_MAIN_INNER_GAP_X
#define MOTOR_MAIN_INNER_GAP_X             6
#endif

#ifndef MOTOR_MAIN_TOP_GAUGE_BAR_H
#define MOTOR_MAIN_TOP_GAUGE_BAR_H         5
#endif

#ifndef MOTOR_MAIN_LEFT_SCALE_H
#define MOTOR_MAIN_LEFT_SCALE_H           14
#endif

#ifndef MOTOR_MAIN_LEFT_BOTTOM_GAUGE_H
#define MOTOR_MAIN_LEFT_BOTTOM_GAUGE_H    14
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

#ifndef MOTOR_MAIN_ANGLE_MAX_DEG_X10
#define MOTOR_MAIN_ANGLE_MAX_DEG_X10     600
#endif

#ifndef MOTOR_MAIN_ANGLE_MAJOR_STEP_X10
#define MOTOR_MAIN_ANGLE_MAJOR_STEP_X10  150
#endif

#ifndef MOTOR_MAIN_ANGLE_MINOR_STEP_X10
#define MOTOR_MAIN_ANGLE_MINOR_STEP_X10   75
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
    MOTOR_MAIN_BOTTOM_TODAY,
    MOTOR_MAIN_BOTTOM_FUEL,
    MOTOR_MAIN_BOTTOM_CURRENT_RECORD,
    MOTOR_MAIN_BOTTOM_USER_1,
    MOTOR_MAIN_BOTTOM_USER_2,
    MOTOR_MAIN_BOTTOM_USER_3,
    MOTOR_MAIN_BOTTOM_USER_4,
    MOTOR_MAIN_BOTTOM_USER_5
} motor_main_bottom_mode_t;

typedef struct
{
    int16_t brake_edge_x;
    int16_t accel_edge_x;
    int16_t content_x;
    int16_t content_w;
} motor_main_left_geometry_t;

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
    size_t len;

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

    while ((dst[0] != '\0') && (u8g2_GetStrWidth(u8g2, dst) > (uint16_t)max_width_px))
    {
        len = strlen(dst);
        if (len == 0u)
        {
            break;
        }
        dst[len - 1u] = '\0';
    }
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

    if (max_mg <= 500)
    {
        *out_major_step = 250;
        *out_minor_step = 125;
        return;
    }

    *out_major_step = 500;
    *out_minor_step = 250;
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

    brake_edge_x = (int16_t)(x + MOTOR_MAIN_SIDE_BAR_INSET_X);
    accel_edge_x = (int16_t)(x + w - 1 - MOTOR_MAIN_SIDE_BAR_INSET_X);
    content_x = (int16_t)(brake_edge_x + MOTOR_MAIN_SIDE_BAR_BOTTOM_W + 6);
    content_w = (int16_t)(accel_edge_x - MOTOR_MAIN_SIDE_BAR_BOTTOM_W - 6 - content_x + 1);

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

static void motor_main_draw_trapezoid_gauge(u8g2_t *u8g2,
                                            int16_t edge_x,
                                            int16_t y,
                                            int16_t h,
                                            uint8_t align_right,
                                            uint8_t fill_from_top,
                                            int32_t value,
                                            int32_t max_value)
{
    int16_t row;
    int16_t fill_rows;

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

            width = motor_main_trapezoid_row_width(row,
                                                   h,
                                                   MOTOR_MAIN_SIDE_BAR_TOP_W,
                                                   MOTOR_MAIN_SIDE_BAR_BOTTOM_W);
            draw_x = (align_right != 0u) ? (int16_t)(edge_x - width + 1) : edge_x;
            if (width > 0)
            {
                u8g2_DrawHLine(u8g2, draw_x, (int16_t)(y + row), (uint8_t)width);
            }
        }
    }

    if (align_right != 0u)
    {
        u8g2_DrawVLine(u8g2, edge_x, y, (uint8_t)h);
        u8g2_DrawLine(u8g2,
                      (int16_t)(edge_x - MOTOR_MAIN_SIDE_BAR_TOP_W + 1),
                      y,
                      (int16_t)(edge_x - MOTOR_MAIN_SIDE_BAR_BOTTOM_W + 1),
                      (int16_t)(y + h - 1));
        u8g2_DrawHLine(u8g2,
                       (int16_t)(edge_x - MOTOR_MAIN_SIDE_BAR_TOP_W + 1),
                       y,
                       MOTOR_MAIN_SIDE_BAR_TOP_W);
        u8g2_DrawHLine(u8g2,
                       (int16_t)(edge_x - MOTOR_MAIN_SIDE_BAR_BOTTOM_W + 1),
                       (int16_t)(y + h - 1),
                       MOTOR_MAIN_SIDE_BAR_BOTTOM_W);
    }
    else
    {
        u8g2_DrawVLine(u8g2, edge_x, y, (uint8_t)h);
        u8g2_DrawLine(u8g2,
                      (int16_t)(edge_x + MOTOR_MAIN_SIDE_BAR_TOP_W - 1),
                      y,
                      (int16_t)(edge_x + MOTOR_MAIN_SIDE_BAR_BOTTOM_W - 1),
                      (int16_t)(y + h - 1));
        u8g2_DrawHLine(u8g2, edge_x, y, MOTOR_MAIN_SIDE_BAR_TOP_W);
        u8g2_DrawHLine(u8g2,
                       edge_x,
                       (int16_t)(y + h - 1),
                       MOTOR_MAIN_SIDE_BAR_BOTTOM_W);
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
    int16_t bar_y;
    int16_t tick_y;
    int16_t major_h;
    int16_t minor_h;
    int16_t fill_w;
    int32_t step;

    if ((u8g2 == 0) || (w < 20) || (h <= MOTOR_MAIN_TOP_GAUGE_BAR_H) || (max_value <= 0))
    {
        return;
    }

    value = motor_main_clamp_i32(value, 0, max_value);
    bar_y = y;
    tick_y = (int16_t)(y + MOTOR_MAIN_TOP_GAUGE_BAR_H);
    major_h = (int16_t)(h - MOTOR_MAIN_TOP_GAUGE_BAR_H);
    if (major_h > MOTOR_MAIN_GAUGE_MAJOR_TICK_H)
    {
        major_h = MOTOR_MAIN_GAUGE_MAJOR_TICK_H;
    }
    if (major_h < 4)
    {
        major_h = (int16_t)(h - MOTOR_MAIN_TOP_GAUGE_BAR_H);
    }

    minor_h = major_h;
    if (minor_h > MOTOR_MAIN_GAUGE_MINOR_TICK_H)
    {
        minor_h = MOTOR_MAIN_GAUGE_MINOR_TICK_H;
    }
    if (minor_h < 2)
    {
        minor_h = 2;
    }

    fill_w = (int16_t)((value * w) / max_value);
    if (fill_w > w)
    {
        fill_w = w;
    }
    if (fill_w > 0)
    {
        u8g2_DrawBox(u8g2, x, bar_y, (uint8_t)fill_w, MOTOR_MAIN_TOP_GAUGE_BAR_H);
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

            tick_x = (int16_t)(x + ((step * (int32_t)(w - 1)) / max_value));
            u8g2_DrawVLine(u8g2, tick_x, tick_y, (uint8_t)minor_h);
        }
    }

    if (major_step > 0)
    {
        for (step = 0; step <= max_value; step += major_step)
        {
            int16_t tick_x;

            tick_x = (int16_t)(x + ((step * (int32_t)(w - 1)) / max_value));
            u8g2_DrawVLine(u8g2, tick_x, tick_y, (uint8_t)major_h);
        }
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
                                          int32_t minor_step)
{
    int16_t bar_y;
    int16_t tick_y;
    int16_t center_x;
    int16_t half_span_px;
    int16_t fill_px;
    int16_t major_h;
    int16_t minor_h;
    int32_t clamped_value;
    int32_t step;

    if ((u8g2 == 0) || (w < 24) || (h <= MOTOR_MAIN_TOP_GAUGE_BAR_H) || (max_abs_value <= 0))
    {
        return;
    }

    clamped_value = motor_main_clamp_i32(value, -max_abs_value, max_abs_value);
    bar_y = y;
    tick_y = (int16_t)(y + MOTOR_MAIN_TOP_GAUGE_BAR_H);
    center_x = (int16_t)(x + (w / 2));
    half_span_px = (int16_t)(w / 2);

    major_h = (int16_t)(h - MOTOR_MAIN_TOP_GAUGE_BAR_H);
    if (major_h > MOTOR_MAIN_GAUGE_MAJOR_TICK_H)
    {
        major_h = MOTOR_MAIN_GAUGE_MAJOR_TICK_H;
    }
    if (major_h < 4)
    {
        major_h = (int16_t)(h - MOTOR_MAIN_TOP_GAUGE_BAR_H);
    }

    minor_h = major_h;
    if (minor_h > MOTOR_MAIN_GAUGE_MINOR_TICK_H)
    {
        minor_h = MOTOR_MAIN_GAUGE_MINOR_TICK_H;
    }
    if (minor_h < 2)
    {
        minor_h = 2;
    }

    fill_px = (int16_t)((motor_main_abs_i32(clamped_value) * half_span_px) / max_abs_value);
    if (fill_px > half_span_px)
    {
        fill_px = half_span_px;
    }

    if (fill_px > 0)
    {
        if (clamped_value >= 0)
        {
            u8g2_DrawBox(u8g2,
                         center_x,
                         bar_y,
                         (uint8_t)fill_px,
                         MOTOR_MAIN_TOP_GAUGE_BAR_H);
        }
        else
        {
            u8g2_DrawBox(u8g2,
                         (int16_t)(center_x - fill_px),
                         bar_y,
                         (uint8_t)fill_px,
                         MOTOR_MAIN_TOP_GAUGE_BAR_H);
        }
    }

    if ((minor_step > 0) && (minor_step < max_abs_value))
    {
        for (step = minor_step; step < max_abs_value; step += minor_step)
        {
            int16_t offset_px;

            if ((major_step > 0) && ((step % major_step) == 0))
            {
                continue;
            }

            offset_px = (int16_t)((step * (int32_t)(w - 1)) / (2 * max_abs_value));
            u8g2_DrawVLine(u8g2, (int16_t)(center_x - offset_px), tick_y, (uint8_t)minor_h);
            u8g2_DrawVLine(u8g2, (int16_t)(center_x + offset_px), tick_y, (uint8_t)minor_h);
        }
    }

    if (major_step > 0)
    {
        for (step = major_step; step <= max_abs_value; step += major_step)
        {
            int16_t offset_px;

            offset_px = (int16_t)((step * (int32_t)(w - 1)) / (2 * max_abs_value));
            u8g2_DrawVLine(u8g2, (int16_t)(center_x - offset_px), tick_y, (uint8_t)major_h);
            u8g2_DrawVLine(u8g2, (int16_t)(center_x + offset_px), tick_y, (uint8_t)major_h);
        }
    }

    u8g2_DrawVLine(u8g2,
                   (int16_t)(center_x - 1),
                   tick_y,
                   (uint8_t)major_h);
    u8g2_DrawVLine(u8g2,
                   center_x,
                   tick_y,
                   (uint8_t)major_h);
    u8g2_DrawVLine(u8g2,
                   (int16_t)(center_x + 1),
                   tick_y,
                   (uint8_t)major_h);
}

static void motor_main_draw_right_stat_row(u8g2_t *u8g2,
                                           int16_t x,
                                           int16_t y,
                                           int16_t w,
                                           const char *label,
                                           const char *value,
                                           const char *unit)
{
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

    mid_h = motor_main_get_font_height(u8g2, MOTOR_MAIN_FONT_MEDIUM);
    small_h = motor_main_get_font_height(u8g2, MOTOR_MAIN_FONT_SMALL);
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
}

static void motor_main_draw_left_section(u8g2_t *u8g2,
                                         int16_t x,
                                         int16_t y,
                                         int16_t w,
                                         int16_t h,
                                         const motor_state_t *state)
{
    motor_main_left_geometry_t geo;
    int16_t side_y;
    int16_t side_h;
    int16_t big_h;
    int16_t mid_h;
    int16_t small_h;
    int16_t lat_gauge_y;
    int16_t max_row_y;
    int16_t angle_value_y;
    int32_t angle_value_x10;
    int32_t lat_value_mg;
    int32_t g_max_mg;
    int32_t g_major_step_mg;
    int32_t g_minor_step_mg;
    int32_t brake_value_mg;
    int32_t accel_value_mg;
    char current_angle_buf[8];
    char left_max_buf[8];
    char right_max_buf[8];

    if ((u8g2 == 0) || (state == 0) || (w < 56) || (h < 56))
    {
        return;
    }

    motor_main_get_left_geometry(x, y, w, h, &geo);

    side_y = (int16_t)(y + MOTOR_MAIN_SIDE_BAR_TOP_GAP_Y);
    side_h = (int16_t)(h - MOTOR_MAIN_SIDE_BAR_TOP_GAP_Y - MOTOR_MAIN_SIDE_BAR_BOTTOM_GAP_Y);
    if (side_h < 10)
    {
        side_h = 10;
    }

    angle_value_x10 = motor_main_get_display_bank_x10(state);
    lat_value_mg = motor_main_get_display_lat_mg(state);
    g_max_mg = motor_main_get_g_scale_max_mg(state);
    motor_main_get_lat_ticks_mg(g_max_mg, &g_major_step_mg, &g_minor_step_mg);

    brake_value_mg = (state->dyn.lon_accel_mg < 0) ? motor_main_abs_i32(state->dyn.lon_accel_mg) : 0;
    accel_value_mg = (state->dyn.lon_accel_mg > 0) ? state->dyn.lon_accel_mg : 0;

    motor_main_draw_trapezoid_gauge(u8g2,
                                    geo.brake_edge_x,
                                    side_y,
                                    side_h,
                                    0u,
                                    1u,
                                    brake_value_mg,
                                    g_max_mg);
    motor_main_draw_trapezoid_gauge(u8g2,
                                    geo.accel_edge_x,
                                    side_y,
                                    side_h,
                                    1u,
                                    0u,
                                    accel_value_mg,
                                    g_max_mg);

    motor_main_draw_bipolar_gauge(u8g2,
                                  geo.content_x,
                                  y,
                                  geo.content_w,
                                  MOTOR_MAIN_LEFT_SCALE_H,
                                  angle_value_x10,
                                  MOTOR_MAIN_ANGLE_MAX_DEG_X10,
                                  MOTOR_MAIN_ANGLE_MAJOR_STEP_X10,
                                  MOTOR_MAIN_ANGLE_MINOR_STEP_X10);

    big_h = motor_main_get_font_height(u8g2, MOTOR_MAIN_FONT_LARGE);
    mid_h = motor_main_get_font_height(u8g2, MOTOR_MAIN_FONT_MEDIUM);
    small_h = motor_main_get_font_height(u8g2, MOTOR_MAIN_FONT_SMALL);
    lat_gauge_y = (int16_t)(y + h - MOTOR_MAIN_LEFT_BOTTOM_GAUGE_H);
    max_row_y = (int16_t)(lat_gauge_y - mid_h - 3);
    angle_value_y = (int16_t)(y + MOTOR_MAIN_LEFT_SCALE_H + ((max_row_y - (y + MOTOR_MAIN_LEFT_SCALE_H) - big_h) / 2));
    if (angle_value_y < (int16_t)(y + MOTOR_MAIN_LEFT_SCALE_H + 2))
    {
        angle_value_y = (int16_t)(y + MOTOR_MAIN_LEFT_SCALE_H + 2);
    }

    motor_main_format_abs_deg_2digit(current_angle_buf, sizeof(current_angle_buf), angle_value_x10);
    motor_main_format_abs_deg(left_max_buf, sizeof(left_max_buf), state->dyn.max_left_bank_deg_x10);
    motor_main_format_abs_deg(right_max_buf, sizeof(right_max_buf), state->dyn.max_right_bank_deg_x10);

    motor_main_draw_text_top(u8g2,
                             MOTOR_MAIN_FONT_LARGE,
                             motor_main_center_text_x(u8g2,
                                                      MOTOR_MAIN_FONT_LARGE,
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

    if ((lat_gauge_y - small_h - 1) > max_row_y)
    {
        motor_main_draw_text_top(u8g2,
                                 MOTOR_MAIN_FONT_SMALL,
                                 motor_main_center_text_x(u8g2,
                                                          MOTOR_MAIN_FONT_SMALL,
                                                          geo.content_x,
                                                          geo.content_w,
                                                          "LAT G"),
                                 (int16_t)(lat_gauge_y - small_h - 1),
                                 "LAT G");
    }

    motor_main_draw_bipolar_gauge(u8g2,
                                  geo.content_x,
                                  lat_gauge_y,
                                  geo.content_w,
                                  MOTOR_MAIN_LEFT_BOTTOM_GAUGE_H,
                                  lat_value_mg,
                                  g_max_mg,
                                  g_major_step_mg,
                                  g_minor_step_mg);
}

static void motor_main_draw_right_section(u8g2_t *u8g2,
                                          int16_t x,
                                          int16_t y,
                                          int16_t w,
                                          int16_t h,
                                          const motor_state_t *state)
{
    int32_t raw_speed_x10;
    int32_t raw_max_speed_x10;
    int32_t alt_value;
    char speed_buf[16];
    char max_buf[24];
    char alt_buf[24];
    char grade_buf[24];
    const char *speed_unit;
    const char *alt_unit;
    int16_t big_h;
    int16_t mid_h;
    int16_t small_h;
    int16_t top_row_y;
    int16_t bottom_rows_y;
    int16_t speed_y;
    int16_t unit_w;
    int16_t speed_x;
    int16_t unit_x;
    int16_t unit_y;

    if ((u8g2 == 0) || (state == 0) || (w < 56) || (h < 56))
    {
        return;
    }

    raw_speed_x10 = (int32_t)state->nav.speed_kmh_x10;
    raw_max_speed_x10 = (int32_t)state->session.max_speed_kmh_x10;
    speed_unit = "km/h";
    alt_unit = Motor_Units_GetAltitudeSuffix(&state->settings.units);
    alt_value = Motor_Units_ConvertAltitudeCm(state->nav.altitude_cm, &state->settings.units);

    motor_main_format_speed_integer(speed_buf, sizeof(speed_buf), raw_speed_x10);
    (void)snprintf(max_buf, sizeof(max_buf), "MAX %ld", (long)(raw_max_speed_x10 / 10));
    (void)snprintf(alt_buf, sizeof(alt_buf), "%ld", (long)alt_value);
    motor_main_format_signed_x10(grade_buf, sizeof(grade_buf), state->snapshot.altitude.grade_noimu_x10);

    big_h = motor_main_get_font_height(u8g2, MOTOR_MAIN_FONT_LARGE);
    mid_h = motor_main_get_font_height(u8g2, MOTOR_MAIN_FONT_MEDIUM);
    small_h = motor_main_get_font_height(u8g2, MOTOR_MAIN_FONT_SMALL);
    top_row_y = y;
    bottom_rows_y = (int16_t)(y + h - ((mid_h * 2) + 4));
    speed_y = (int16_t)(top_row_y + mid_h + 4 + ((bottom_rows_y - (top_row_y + mid_h + 4) - big_h) / 2));
    if (speed_y < (int16_t)(top_row_y + mid_h + 4))
    {
        speed_y = (int16_t)(top_row_y + mid_h + 4);
    }

    unit_w = (int16_t)motor_main_measure_text(u8g2, MOTOR_MAIN_FONT_SMALL, speed_unit);
    unit_x = (int16_t)(x + w - unit_w);
    speed_x = motor_main_right_text_x(u8g2,
                                      MOTOR_MAIN_FONT_LARGE,
                                      (int16_t)(unit_x - MOTOR_MAIN_VALUE_UNIT_GAP_X),
                                      speed_buf);
    unit_y = (int16_t)(speed_y + ((big_h > small_h) ? (big_h - small_h) : 0));

    motor_main_draw_text_top(u8g2,
                             MOTOR_MAIN_FONT_MEDIUM,
                             motor_main_right_text_x(u8g2,
                                                     MOTOR_MAIN_FONT_MEDIUM,
                                                     (int16_t)(x + w),
                                                     max_buf),
                             top_row_y,
                             max_buf);

    motor_main_draw_text_top(u8g2, MOTOR_MAIN_FONT_LARGE, speed_x, speed_y, speed_buf);
    motor_main_draw_text_top(u8g2, MOTOR_MAIN_FONT_SMALL, unit_x, unit_y, speed_unit);

    motor_main_draw_right_stat_row(u8g2,
                                   x,
                                   bottom_rows_y,
                                   w,
                                   "ALT",
                                   alt_buf,
                                   alt_unit);
    motor_main_draw_right_stat_row(u8g2,
                                   x,
                                   (int16_t)(bottom_rows_y + mid_h + 4),
                                   w,
                                   "GRADE",
                                   grade_buf,
                                   "%");
}


static motor_main_bottom_mode_t motor_main_get_bottom_mode(const motor_state_t *state)
{
    motor_record_state_t record_state;

    if (state == 0)
    {
        return MOTOR_MAIN_BOTTOM_CURRENT_RECORD;
    }

    record_state = (motor_record_state_t)state->record.state;
    if ((record_state == MOTOR_RECORD_STATE_RECORDING) ||
        (record_state == MOTOR_RECORD_STATE_PAUSED) ||
        (record_state == MOTOR_RECORD_STATE_CLOSING))
    {
        return MOTOR_MAIN_BOTTOM_CURRENT_RECORD;
    }

    if (state->session.trip_a_m != 0u)
    {
        return MOTOR_MAIN_BOTTOM_TRIP_A;
    }

    return MOTOR_MAIN_BOTTOM_CURRENT_RECORD;
}

static const char *motor_main_get_bottom_title(motor_main_bottom_mode_t mode)
{
    switch (mode)
    {
    case MOTOR_MAIN_BOTTOM_TRIP_A:         return "TRIP A";
    case MOTOR_MAIN_BOTTOM_TRIP_B:         return "TRIP B";
    case MOTOR_MAIN_BOTTOM_TODAY:          return "TODAY";
    case MOTOR_MAIN_BOTTOM_FUEL:           return "FUEL";
    case MOTOR_MAIN_BOTTOM_CURRENT_RECORD: return "CURRENT REC";
    case MOTOR_MAIN_BOTTOM_USER_1:         return "USER 1";
    case MOTOR_MAIN_BOTTOM_USER_2:         return "USER 2";
    case MOTOR_MAIN_BOTTOM_USER_3:         return "USER 3";
    case MOTOR_MAIN_BOTTOM_USER_4:         return "USER 4";
    case MOTOR_MAIN_BOTTOM_USER_5:         return "USER 5";
    default:                               return "BOTTOM";
    }
}

static uint8_t motor_main_get_user_window_map(motor_main_bottom_mode_t mode,
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
    case MOTOR_MAIN_BOTTOM_USER_4:
        *out_page_index = 1u;
        *out_slot_start = 0u;
        return 1u;
    case MOTOR_MAIN_BOTTOM_USER_5:
        *out_page_index = 1u;
        *out_slot_start = 5u;
        return 1u;
    default:
        break;
    }

    return 0u;
}

static uint32_t motor_main_get_trip_distance_m(const motor_state_t *state,
                                               motor_main_bottom_mode_t mode)
{
    if (state == 0)
    {
        return 0u;
    }

    switch (mode)
    {
    case MOTOR_MAIN_BOTTOM_TRIP_A:
        return (state->session.trip_a_m != 0u) ? state->session.trip_a_m : state->session.distance_m;
    case MOTOR_MAIN_BOTTOM_TRIP_B:
        return (state->session.trip_b_m != 0u) ? state->session.trip_b_m : state->session.distance_m;
    case MOTOR_MAIN_BOTTOM_TODAY:
    case MOTOR_MAIN_BOTTOM_FUEL:
    case MOTOR_MAIN_BOTTOM_CURRENT_RECORD:
    default:
        return state->session.distance_m;
    }
}

static uint32_t motor_main_get_trip_ride_seconds(const motor_state_t *state,
                                                 motor_main_bottom_mode_t mode)
{
    (void)mode;
    return (state != 0) ? state->session.ride_seconds : 0u;
}

static uint16_t motor_main_get_trip_max_speed_kmh_x10(const motor_state_t *state,
                                                      motor_main_bottom_mode_t mode)
{
    (void)mode;
    return (state != 0) ? state->session.max_speed_kmh_x10 : 0u;
}

static void motor_main_draw_trip_summary(u8g2_t *u8g2,
                                         int16_t x,
                                         int16_t y,
                                         int16_t w,
                                         int16_t h,
                                         const motor_state_t *state,
                                         motor_main_bottom_mode_t mode)
{
    char dist_text[24];
    char time_text[16];
    char max_text[24];
    int16_t cell_w;
    const char *title;

    if ((u8g2 == 0) || (state == 0))
    {
        return;
    }

    title = motor_main_get_bottom_title(mode);
    Motor_Units_FormatDistance(dist_text,
                               sizeof(dist_text),
                               (int32_t)motor_main_get_trip_distance_m(state, mode),
                               &state->settings.units);
    motor_main_format_seconds(time_text,
                              sizeof(time_text),
                              motor_main_get_trip_ride_seconds(state, mode));
    Motor_Units_FormatSpeed(max_text,
                            sizeof(max_text),
                            motor_main_get_trip_max_speed_kmh_x10(state, mode),
                            &state->settings.units);

    u8g2_DrawFrame(u8g2, x, y, w, h);
    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(u8g2, x + 3, y + 7, title);

    cell_w = (int16_t)((w - 6) / 3);
    if (cell_w < 20)
    {
        cell_w = 20;
    }

    u8g2_DrawVLine(u8g2, (int16_t)(x + cell_w), y + 9, h - 10);
    u8g2_DrawVLine(u8g2, (int16_t)(x + (cell_w * 2)), y + 9, h - 10);

    u8g2_DrawStr(u8g2, x + 4, y + 14, "DST");
    u8g2_DrawStr(u8g2, x + 4, y + 23, dist_text);

    u8g2_DrawStr(u8g2, (int16_t)(x + cell_w + 4), y + 14, "RIDE");
    u8g2_DrawStr(u8g2, (int16_t)(x + cell_w + 4), y + 23, time_text);

    u8g2_DrawStr(u8g2, (int16_t)(x + (cell_w * 2) + 4), y + 14, "MAX");
    u8g2_DrawStr(u8g2, (int16_t)(x + (cell_w * 2) + 4), y + 23, max_text);
}

static void motor_main_draw_user_window(u8g2_t *u8g2,
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
    int16_t cell_w;

    if ((u8g2 == 0) || (state == 0))
    {
        return;
    }

    if (motor_main_get_user_window_map(mode, &page_index, &slot_start) == 0u)
    {
        return;
    }

    u8g2_DrawFrame(u8g2, x, y, w, h);
    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(u8g2, x + 3, y + 7, motor_main_get_bottom_title(mode));

    cell_w = (int16_t)((w - 2) / MOTOR_MAIN_USER_CELL_COUNT);

    for (i = 0u; i < MOTOR_MAIN_USER_CELL_COUNT; i++)
    {
        motor_data_field_text_t text;
        char label_buf[16];
        char value_buf[24];
        int16_t cell_x;
        motor_data_field_id_t field_id;

        cell_x = (int16_t)(x + 1 + (i * cell_w));
        field_id = (motor_data_field_id_t)state->settings.data_fields[page_index][slot_start + i];
        Motor_DataField_Format(field_id, state, &text);

        if (i != 0u)
        {
            u8g2_DrawVLine(u8g2, cell_x, y + 9, h - 10);
        }

        motor_main_copy_fit(u8g2, text.label, label_buf, sizeof(label_buf), cell_w - 4);
        u8g2_DrawStr(u8g2, cell_x + 2, y + 14, label_buf);

        u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
        motor_main_copy_fit(u8g2, text.value, value_buf, sizeof(value_buf), cell_w - 4);
        u8g2_DrawStr(u8g2, cell_x + 2, y + 23, value_buf);
    }
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

    if ((mode >= MOTOR_MAIN_BOTTOM_USER_1) && (mode <= MOTOR_MAIN_BOTTOM_USER_5))
    {
        motor_main_draw_user_window(u8g2, x, y, w, h, state, mode);
    }
    else
    {
        motor_main_draw_trip_summary(u8g2, x, y, w, h, state, mode);
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

void Motor_UI_DrawScreen_Main(u8g2_t *u8g2,
                              const ui_rect_t *viewport,
                              const motor_state_t *state)
{
    int16_t outer_x;
    int16_t outer_y;
    int16_t outer_w;
    int16_t gauge_y;
    int16_t middle_y;
    int16_t middle_h;
    int16_t bottom_y;
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

    if (viewport->h < (MOTOR_MAIN_TOP_GAUGE_H + MOTOR_MAIN_BOTTOM_RIBBON_H + 30))
    {
        Motor_UI_DrawCenteredTextBlock(u8g2,
                                       viewport,
                                       (int16_t)((viewport->h / 2) - 14),
                                       "PAGE 1",
                                       "Viewport too small",
                                       "Need taller layout");
        return;
    }

    outer_x = (int16_t)(viewport->x + MOTOR_MAIN_OUTER_PAD_X);
    outer_y = viewport->y;
    outer_w = (int16_t)(viewport->w - (MOTOR_MAIN_OUTER_PAD_X * 2));

    gauge_y = outer_y;
    middle_y = (int16_t)(gauge_y + MOTOR_MAIN_TOP_GAUGE_H + 3);
    bottom_y = (int16_t)(viewport->y + viewport->h - MOTOR_MAIN_BOTTOM_RIBBON_H);
    middle_h = (int16_t)(bottom_y - middle_y - 2);

    split_x = (int16_t)(viewport->x + (viewport->w / 2));
    left_x = outer_x;
    left_w = (int16_t)(split_x - outer_x - (MOTOR_MAIN_INNER_GAP_X / 2));
    right_x = (int16_t)(split_x + (MOTOR_MAIN_INNER_GAP_X / 2));
    right_w = (int16_t)((outer_x + outer_w) - right_x);

    top_rect.x = outer_x;
    top_rect.y = gauge_y;
    top_rect.w = outer_w;
    top_rect.h = MOTOR_MAIN_TOP_GAUGE_H;

    left_rect.x = left_x;
    left_rect.y = middle_y;
    left_rect.w = left_w;
    left_rect.h = middle_h;

    right_rect.x = right_x;
    right_rect.y = middle_y;
    right_rect.w = right_w;
    right_rect.h = middle_h;

    bottom_rect.x = outer_x;
    bottom_rect.y = bottom_y;
    bottom_rect.w = outer_w;
    bottom_rect.h = MOTOR_MAIN_BOTTOM_RIBBON_H;

    motor_main_draw_page1_top_area(u8g2, &top_rect, state);

    u8g2_DrawVLine(u8g2, split_x, middle_y, middle_h);

    motor_main_draw_page1_left_area(u8g2, &left_rect, state);
    motor_main_draw_page1_right_area(u8g2, &right_rect, state);
    motor_main_draw_page1_bottom_area(u8g2, &bottom_rect, state);
}
