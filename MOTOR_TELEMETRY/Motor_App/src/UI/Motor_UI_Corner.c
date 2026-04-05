#include "Motor_UI_Internal.h"

#include <stdio.h>
#include <stdlib.h>

#ifndef MOTOR_CORNER_FONT_SPEED_LARGE
#define MOTOR_CORNER_FONT_SPEED_LARGE  u8g2_font_logisoso26_tn
#endif

#ifndef MOTOR_CORNER_FONT_SMALL
#define MOTOR_CORNER_FONT_SMALL        u8g2_font_5x7_tf
#endif

#define MOTOR_CORNER_FRICTION_MAX_MG   1000

static int32_t motor_corner_clamp_i32(int32_t value, int32_t lo, int32_t hi)
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

static uint32_t motor_corner_isqrt_u32(uint32_t value)
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

static int16_t motor_corner_get_font_height(u8g2_t *u8g2, const uint8_t *font)
{
    if ((u8g2 == 0) || (font == 0))
    {
        return 0;
    }

    u8g2_SetFont(u8g2, font);
    return (int16_t)(u8g2_GetAscent(u8g2) - u8g2_GetDescent(u8g2));
}

static uint16_t motor_corner_measure_text(u8g2_t *u8g2, const uint8_t *font, const char *text)
{
    if ((u8g2 == 0) || (font == 0) || (text == 0))
    {
        return 0u;
    }

    u8g2_SetFont(u8g2, font);
    return u8g2_GetStrWidth(u8g2, text);
}

static int16_t motor_corner_right_text_x(u8g2_t *u8g2,
                                         const uint8_t *font,
                                         int16_t right_x,
                                         const char *text)
{
    uint16_t text_w;

    if ((u8g2 == 0) || (font == 0) || (text == 0))
    {
        return right_x;
    }

    text_w = motor_corner_measure_text(u8g2, font, text);
    if (text_w >= (uint16_t)right_x)
    {
        return 0;
    }

    return (int16_t)(right_x - (int16_t)text_w);
}

static void motor_corner_draw_text_top(u8g2_t *u8g2,
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

static uint8_t motor_corner_left_is_negative(const motor_state_t *state)
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

static int32_t motor_corner_get_display_lat_mg(const motor_state_t *state)
{
    if (state == 0)
    {
        return 0;
    }

    return (motor_corner_left_is_negative(state) != 0u) ? state->dyn.lat_accel_mg
                                                         : -state->dyn.lat_accel_mg;
}

static void motor_corner_format_speed(char *out_text, size_t out_size, int32_t speed_x10)
{
    int32_t display_speed_x10;

    if ((out_text == 0) || (out_size == 0u))
    {
        return;
    }

    display_speed_x10 = motor_corner_clamp_i32(speed_x10, 0, 4000);
    (void)snprintf(out_text, out_size, "%ld", (long)(display_speed_x10 / 10));
}

static void motor_corner_draw_friction_circle(u8g2_t *u8g2,
                                              const ui_rect_t *viewport,
                                              const motor_state_t *state)
{
    int16_t center_x;
    int16_t center_y;
    int16_t radius;
    int16_t minor_radius;
    int16_t dot_x;
    int16_t dot_y;
    int16_t mag_y;
    int16_t mag_x;
    int16_t small_h;
    int32_t lat_mg;
    int32_t lon_mg;
    uint32_t total_g_mg;
    char mag_buf[16];

    if ((u8g2 == 0) || (viewport == 0) || (state == 0))
    {
        return;
    }

    radius = (int16_t)((viewport->h < 110) ? ((viewport->h - 12) / 2) : 49);
    if (radius > 54)
    {
        radius = 54;
    }
    if (radius < 24)
    {
        radius = 24;
    }

    center_x = (int16_t)(viewport->x + ((viewport->w * 42) / 100));
    center_y = (int16_t)(viewport->y + (viewport->h / 2) - 2);
    if ((center_x - radius) < (viewport->x + 6))
    {
        center_x = (int16_t)(viewport->x + radius + 6);
    }

    lat_mg = motor_corner_get_display_lat_mg(state);
    lon_mg = state->dyn.lon_accel_mg;
    lat_mg = motor_corner_clamp_i32(lat_mg, -MOTOR_CORNER_FRICTION_MAX_MG, MOTOR_CORNER_FRICTION_MAX_MG);
    lon_mg = motor_corner_clamp_i32(lon_mg, -MOTOR_CORNER_FRICTION_MAX_MG, MOTOR_CORNER_FRICTION_MAX_MG);

    u8g2_DrawCircle(u8g2, center_x, center_y, radius, U8G2_DRAW_ALL);
    minor_radius = (int16_t)(radius / 2);
    if (minor_radius > 4)
    {
        u8g2_DrawCircle(u8g2, center_x, center_y, minor_radius, U8G2_DRAW_ALL);
    }
    u8g2_DrawHLine(u8g2, (int16_t)(center_x - radius), center_y, (uint8_t)(radius * 2));
    u8g2_DrawVLine(u8g2, center_x, (int16_t)(center_y - radius), (uint8_t)(radius * 2));

    dot_x = (int16_t)(center_x + ((lat_mg * radius) / MOTOR_CORNER_FRICTION_MAX_MG));
    dot_y = (int16_t)(center_y - ((lon_mg * radius) / MOTOR_CORNER_FRICTION_MAX_MG));
    u8g2_DrawDisc(u8g2, dot_x, dot_y, 2, U8G2_DRAW_ALL);

    total_g_mg = motor_corner_isqrt_u32((uint32_t)(((int64_t)lat_mg * (int64_t)lat_mg) +
                                                   ((int64_t)lon_mg * (int64_t)lon_mg)));
    (void)snprintf(mag_buf,
                   sizeof(mag_buf),
                   "%lu.%01luG",
                   (unsigned long)(total_g_mg / 1000u),
                   (unsigned long)((total_g_mg % 1000u) / 100u));
    small_h = motor_corner_get_font_height(u8g2, MOTOR_CORNER_FONT_SMALL);
    mag_x = (int16_t)(center_x - ((int16_t)motor_corner_measure_text(u8g2, MOTOR_CORNER_FONT_SMALL, mag_buf) / 2));
    mag_y = (int16_t)(center_y + radius + 4);
    if ((mag_y + small_h) < (viewport->y + viewport->h - 2))
    {
        motor_corner_draw_text_top(u8g2, MOTOR_CORNER_FONT_SMALL, mag_x, mag_y, mag_buf);
    }
}

static void motor_corner_draw_speed(u8g2_t *u8g2,
                                    const ui_rect_t *viewport,
                                    const motor_state_t *state)
{
    char speed_buf[16];
    int16_t big_h;
    int16_t small_h;
    int16_t speed_x;
    int16_t speed_y;
    int16_t unit_x;
    int16_t unit_y;

    if ((u8g2 == 0) || (viewport == 0) || (state == 0))
    {
        return;
    }

    motor_corner_format_speed(speed_buf, sizeof(speed_buf), (int32_t)state->nav.speed_kmh_x10);
    big_h = motor_corner_get_font_height(u8g2, MOTOR_CORNER_FONT_SPEED_LARGE);
    small_h = motor_corner_get_font_height(u8g2, MOTOR_CORNER_FONT_SMALL);
    speed_x = motor_corner_right_text_x(u8g2,
                                        MOTOR_CORNER_FONT_SPEED_LARGE,
                                        (int16_t)(viewport->x + viewport->w - 6),
                                        speed_buf);
    speed_y = (int16_t)(viewport->y + viewport->h - big_h - 9);
    unit_x = motor_corner_right_text_x(u8g2,
                                       MOTOR_CORNER_FONT_SMALL,
                                       (int16_t)(viewport->x + viewport->w - 6),
                                       "km/h");
    unit_y = (int16_t)(speed_y + big_h - small_h - 1);

    motor_corner_draw_text_top(u8g2, MOTOR_CORNER_FONT_SPEED_LARGE, speed_x, speed_y, speed_buf);
    motor_corner_draw_text_top(u8g2, MOTOR_CORNER_FONT_SMALL, unit_x, unit_y, "km/h");
}

void Motor_UI_DrawScreen_Corner(u8g2_t *u8g2, const ui_rect_t *viewport, const motor_state_t *state)
{
    if ((u8g2 == 0) || (viewport == 0) || (state == 0))
    {
        return;
    }

    motor_corner_draw_friction_circle(u8g2, viewport, state);
    motor_corner_draw_speed(u8g2, viewport, state);
}
