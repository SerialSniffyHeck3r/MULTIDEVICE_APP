#include "Motor_UI_Internal.h"
#include "Motor_DataField.h"
#include "Motor_Units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MOTOR_MAIN_TOP_GAUGE_H
#define MOTOR_MAIN_TOP_GAUGE_H           18
#endif

#ifndef MOTOR_MAIN_BOTTOM_RIBBON_H
#define MOTOR_MAIN_BOTTOM_RIBBON_H       26
#endif

#ifndef MOTOR_MAIN_OUTER_PAD_X
#define MOTOR_MAIN_OUTER_PAD_X            4
#endif

#ifndef MOTOR_MAIN_INNER_GAP_X
#define MOTOR_MAIN_INNER_GAP_X            6
#endif

#ifndef MOTOR_MAIN_LAT_BAR_ABS_MAX_X10
#define MOTOR_MAIN_LAT_BAR_ABS_MAX_X10   15   /* 1.5 g */
#endif

#ifndef MOTOR_MAIN_BANK_BAR_ABS_MAX_X10
#define MOTOR_MAIN_BANK_BAR_ABS_MAX_X10 600   /* 60.0 deg */
#endif

#ifndef MOTOR_MAIN_USER_CELL_COUNT
#define MOTOR_MAIN_USER_CELL_COUNT        5u
#endif

typedef enum
{
    MOTOR_MAIN_GAUGE_SPEED = 0u,
    MOTOR_MAIN_GAUGE_RPM
} motor_main_gauge_mode_t;

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

static int16_t motor_main_center_text_x(u8g2_t *u8g2,
                                        int16_t x,
                                        int16_t w,
                                        const char *text)
{
    uint16_t text_w;

    if ((u8g2 == 0) || (text == 0))
    {
        return x;
    }

    text_w = u8g2_GetStrWidth(u8g2, text);
    if (text_w >= (uint16_t)w)
    {
        return x;
    }

    return (int16_t)(x + ((w - (int16_t)text_w) / 2));
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

static int16_t motor_main_find_recent_side_bank_x10(const motor_state_t *state, uint8_t want_left)
{
    uint8_t left_is_negative;
    uint16_t i;

    if (state == 0)
    {
        return 0;
    }

    left_is_negative = motor_main_left_is_negative(state);

    for (i = 0u; i < MOTOR_HISTORY_SAMPLE_COUNT; i++)
    {
        uint16_t index;
        int16_t bank_x10;

        index = (uint16_t)((state->dyn.history_head + MOTOR_HISTORY_SAMPLE_COUNT - 1u - i) % MOTOR_HISTORY_SAMPLE_COUNT);
        bank_x10 = state->dyn.bank_history_x10[index];

        if (motor_main_abs_i16(bank_x10) < 30)
        {
            continue;
        }

        if (want_left != 0u)
        {
            if ((left_is_negative != 0u) && (bank_x10 < 0))
            {
                return bank_x10;
            }
            if ((left_is_negative == 0u) && (bank_x10 > 0))
            {
                return bank_x10;
            }
        }
        else
        {
            if ((left_is_negative != 0u) && (bank_x10 > 0))
            {
                return bank_x10;
            }
            if ((left_is_negative == 0u) && (bank_x10 < 0))
            {
                return bank_x10;
            }
        }
    }

    return 0;
}

static motor_main_gauge_mode_t motor_main_get_gauge_mode(const motor_state_t *state)
{
    (void)state;

    /* ---------------------------------------------------------------------- */
    /*  현재 레포에는 page-1 전용 gauge setting field가 아직 없다.               */
    /*  그래서 렌더러는 일단 SPEED를 기본으로 쓰고,                             */
    /*  추후 dedicated setting이 생기면 이 accessor만 바꾸면 된다.             */
    /* ---------------------------------------------------------------------- */
    return MOTOR_MAIN_GAUGE_SPEED;
}

static void motor_main_get_gauge_range(const motor_state_t *state,
                                       motor_main_gauge_mode_t mode,
                                       int32_t *out_min,
                                       int32_t *out_max)
{
    if ((out_min == 0) || (out_max == 0))
    {
        return;
    }

    if (mode == MOTOR_MAIN_GAUGE_RPM)
    {
        *out_min = 0;
        *out_max = 14000;
        return;
    }

    if ((state != 0) && (state->settings.units.speed == (uint8_t)MOTOR_SPEED_UNIT_MPH))
    {
        *out_min = 0;
        *out_max = 1200;
    }
    else
    {
        *out_min = 0;
        *out_max = 2000;
    }
}

static int32_t motor_main_get_gauge_value(const motor_state_t *state,
                                          motor_main_gauge_mode_t mode)
{
    if (state == 0)
    {
        return 0;
    }

    if (mode == MOTOR_MAIN_GAUGE_RPM)
    {
        return (state->vehicle.rpm_valid != false) ? (int32_t)state->vehicle.rpm : 0;
    }

    return Motor_Units_ConvertSpeedX10((int32_t)state->nav.speed_kmh_x10, &state->settings.units);
}

static const char *motor_main_get_gauge_label(motor_main_gauge_mode_t mode)
{
    return (mode == MOTOR_MAIN_GAUGE_RPM) ? "RPM" : "SPEED";
}

static const char *motor_main_get_gauge_suffix(const motor_state_t *state,
                                               motor_main_gauge_mode_t mode)
{
    if (mode == MOTOR_MAIN_GAUGE_RPM)
    {
        return "rpm";
    }

    return (state != 0) ? Motor_Units_GetSpeedSuffix(&state->settings.units) : "km/h";
}

static void motor_main_format_gauge_value(char *out_text,
                                          size_t out_size,
                                          const motor_state_t *state,
                                          motor_main_gauge_mode_t mode,
                                          int32_t value)
{
    if ((out_text == 0) || (out_size == 0u))
    {
        return;
    }

    if (mode == MOTOR_MAIN_GAUGE_RPM)
    {
        (void)snprintf(out_text, out_size, "%ld", (long)value);
        return;
    }

    (void)state;
    (void)snprintf(out_text, out_size, "%ld", (long)(value / 10));
}

static void motor_main_draw_abs_bar(u8g2_t *u8g2,
                                    int16_t x,
                                    int16_t y,
                                    int16_t w,
                                    int16_t h,
                                    int32_t value_x10,
                                    int32_t abs_max_x10,
                                    const char *caption)
{
    int16_t mid_x;
    int16_t fill_px;
    int32_t clamped_x10;
    char value_buf[12];

    if ((u8g2 == 0) || (w < 6) || (h < 6) || (abs_max_x10 <= 0))
    {
        return;
    }

    clamped_x10 = motor_main_clamp_i32(value_x10, -abs_max_x10, abs_max_x10);
    mid_x = (int16_t)(x + (w / 2));
    fill_px = (int16_t)((motor_main_abs_i32(clamped_x10) * (int32_t)(w / 2 - 2)) / abs_max_x10);

    u8g2_DrawFrame(u8g2, x, y, w, h);
    u8g2_DrawVLine(u8g2, mid_x, y + 1, h - 2);

    if (clamped_x10 >= 0)
    {
        if (fill_px > 0)
        {
            u8g2_DrawBox(u8g2, mid_x + 1, y + 1, (uint8_t)fill_px, (uint8_t)(h - 2));
        }
    }
    else
    {
        if (fill_px > 0)
        {
            u8g2_DrawBox(u8g2, mid_x - fill_px, y + 1, (uint8_t)fill_px, (uint8_t)(h - 2));
        }
    }

    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    if (caption != 0)
    {
        u8g2_DrawStr(u8g2, x + 2, y - 1, caption);
    }

    motor_main_format_signed_x10(value_buf, sizeof(value_buf), clamped_x10);
    u8g2_DrawStr(u8g2,
                 (int16_t)(x + w - (int16_t)u8g2_GetStrWidth(u8g2, value_buf) - 2),
                 y - 1,
                 value_buf);
}

static void motor_main_draw_top_gauge(u8g2_t *u8g2,
                                      int16_t x,
                                      int16_t y,
                                      int16_t w,
                                      int16_t h,
                                      const motor_state_t *state)
{
    motor_main_gauge_mode_t mode;
    int32_t value;
    int32_t range_min;
    int32_t range_max;
    int16_t bar_x;
    int16_t bar_y;
    int16_t bar_w;
    int16_t bar_h;
    uint16_t fill_w;
    char value_buf[20];
    char min_buf[16];
    char max_buf[16];

    if ((u8g2 == 0) || (state == 0) || (w < 40) || (h < 10))
    {
        return;
    }

    mode = motor_main_get_gauge_mode(state);
    value = motor_main_get_gauge_value(state, mode);
    motor_main_get_gauge_range(state, mode, &range_min, &range_max);
    value = motor_main_clamp_i32(value, range_min, range_max);

    u8g2_DrawFrame(u8g2, x, y, w, h);

    bar_x = (int16_t)(x + 54);
    bar_y = (int16_t)(y + h - 7);
    bar_w = (int16_t)(w - 84);
    bar_h = 5;
    if (bar_w < 20)
    {
        bar_x = (int16_t)(x + 34);
        bar_w = (int16_t)(w - 60);
    }

    fill_w = (uint16_t)(((uint32_t)(value - range_min) * (uint32_t)(bar_w - 2)) /
                        (uint32_t)((range_max > range_min) ? (range_max - range_min) : 1));

    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(u8g2, x + 3, y + 7, motor_main_get_gauge_label(mode));

    motor_main_format_gauge_value(value_buf, sizeof(value_buf), state, mode, value);
    u8g2_DrawStr(u8g2,
                 (int16_t)(x + w - (int16_t)u8g2_GetStrWidth(u8g2, value_buf) - 20),
                 y + 7,
                 value_buf);
    u8g2_DrawStr(u8g2,
                 (int16_t)(x + w - (int16_t)u8g2_GetStrWidth(u8g2, motor_main_get_gauge_suffix(state, mode)) - 3),
                 y + 7,
                 motor_main_get_gauge_suffix(state, mode));

    u8g2_DrawFrame(u8g2, bar_x, bar_y, bar_w, bar_h);
    if (fill_w > 0u)
    {
        u8g2_DrawBox(u8g2, bar_x + 1, bar_y + 1, fill_w, (uint8_t)(bar_h - 2));
    }

    if (mode == MOTOR_MAIN_GAUGE_RPM)
    {
        (void)snprintf(min_buf, sizeof(min_buf), "%ld", (long)range_min);
        (void)snprintf(max_buf, sizeof(max_buf), "%ld", (long)range_max);
    }
    else
    {
        (void)snprintf(min_buf, sizeof(min_buf), "%ld", (long)(range_min / 10));
        (void)snprintf(max_buf, sizeof(max_buf), "%ld", (long)(range_max / 10));
    }

    u8g2_DrawStr(u8g2, x + 3, y + h - 2, min_buf);
    u8g2_DrawStr(u8g2,
                 (int16_t)(x + w - (int16_t)u8g2_GetStrWidth(u8g2, max_buf) - 3),
                 y + h - 2,
                 max_buf);
}

static void motor_main_draw_left_panel(u8g2_t *u8g2,
                                       int16_t x,
                                       int16_t y,
                                       int16_t w,
                                       int16_t h,
                                       const motor_state_t *state)
{
    int16_t bar_y;
    int16_t bar_w;
    int16_t bank_center_y;
    int16_t footer_y;
    int16_t left_recent_x10;
    int16_t right_recent_x10;
    int32_t brake_g_x10;
    int32_t accel_g_x10;
    char bank_now[20];
    char left_recent[16];
    char right_recent[16];
    char brake_text[16];
    char accel_text[16];

    if ((u8g2 == 0) || (state == 0) || (w < 60) || (h < 30))
    {
        return;
    }

    bar_y = (int16_t)(y + 12);
    bar_w = (int16_t)((w - 6) / 2);
    if (bar_w < 24)
    {
        bar_w = 24;
    }

    motor_main_draw_abs_bar(u8g2,
                            x,
                            bar_y,
                            bar_w,
                            8,
                            state->dyn.lat_accel_mg / 100,
                            MOTOR_MAIN_LAT_BAR_ABS_MAX_X10,
                            "LAT");
    motor_main_draw_abs_bar(u8g2,
                            (int16_t)(x + w - bar_w),
                            bar_y,
                            bar_w,
                            8,
                            state->dyn.bank_deg_x10,
                            MOTOR_MAIN_BANK_BAR_ABS_MAX_X10,
                            "BANK");

    bank_center_y = (int16_t)(bar_y + 30);
    left_recent_x10 = motor_main_find_recent_side_bank_x10(state, 1u);
    right_recent_x10 = motor_main_find_recent_side_bank_x10(state, 0u);
    brake_g_x10 = (state->dyn.lon_accel_mg < 0) ? (motor_main_abs_i32(state->dyn.lon_accel_mg) / 100) : 0;
    accel_g_x10 = (state->dyn.lon_accel_mg > 0) ? (state->dyn.lon_accel_mg / 100) : 0;

    (void)snprintf(bank_now,
                   sizeof(bank_now),
                   "%+ld",
                   (long)(state->dyn.bank_deg_x10 / 10));
    (void)snprintf(left_recent,
                   sizeof(left_recent),
                   "L %d",
                   (int)(motor_main_abs_i16(left_recent_x10) / 10));
    (void)snprintf(right_recent,
                   sizeof(right_recent),
                   "R %d",
                   (int)(motor_main_abs_i16(right_recent_x10) / 10));
    motor_main_format_signed_x10(brake_text, sizeof(brake_text), brake_g_x10);
    motor_main_format_signed_x10(accel_text, sizeof(accel_text), accel_g_x10);

    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(u8g2, x + 2, (int16_t)(bank_center_y - 14), left_recent);
    u8g2_DrawStr(u8g2,
                 (int16_t)(x + w - (int16_t)u8g2_GetStrWidth(u8g2, right_recent) - 2),
                 (int16_t)(bank_center_y - 14),
                 right_recent);

    u8g2_SetFont(u8g2, u8g2_font_9x15B_tr);
    u8g2_DrawStr(u8g2,
                 motor_main_center_text_x(u8g2, x, w, bank_now),
                 bank_center_y,
                 bank_now);

    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(u8g2,
                 motor_main_center_text_x(u8g2, x, w, "ANGLE"),
                 (int16_t)(bank_center_y - 22),
                 "ANGLE");

    footer_y = (int16_t)(y + h - 8);
    u8g2_DrawStr(u8g2, x + 2, footer_y - 8, "BRK");
    u8g2_DrawStr(u8g2, x + 2, footer_y, brake_text);
    u8g2_DrawStr(u8g2,
                 (int16_t)(x + w - (int16_t)u8g2_GetStrWidth(u8g2, "ACC") - 2),
                 footer_y - 8,
                 "ACC");
    u8g2_DrawStr(u8g2,
                 (int16_t)(x + w - (int16_t)u8g2_GetStrWidth(u8g2, accel_text) - 2),
                 footer_y,
                 accel_text);
}

static void motor_main_draw_right_panel(u8g2_t *u8g2,
                                        int16_t x,
                                        int16_t y,
                                        int16_t w,
                                        int16_t h,
                                        const motor_state_t *state)
{
    int32_t speed_x10_conv;
    int32_t max_speed_x10_conv;
    char speed_big[16];
    char speed_max[24];
    char alt_text[24];
    char grade_text[24];
    const char *speed_suffix;
    int16_t speed_y;
    int16_t bottom_y;

    if ((u8g2 == 0) || (state == 0) || (w < 60) || (h < 30))
    {
        return;
    }

    speed_x10_conv = Motor_Units_ConvertSpeedX10((int32_t)state->nav.speed_kmh_x10, &state->settings.units);
    max_speed_x10_conv = Motor_Units_ConvertSpeedX10((int32_t)state->session.max_speed_kmh_x10, &state->settings.units);
    speed_suffix = Motor_Units_GetSpeedSuffix(&state->settings.units);

    (void)snprintf(speed_big, sizeof(speed_big), "%ld", (long)(speed_x10_conv / 10));
    (void)snprintf(speed_max,
                   sizeof(speed_max),
                   "MAX %ld %s",
                   (long)(max_speed_x10_conv / 10),
                   speed_suffix);
    Motor_Units_FormatAltitude(alt_text, sizeof(alt_text), state->nav.altitude_cm, &state->settings.units);
    motor_main_format_signed_x10(grade_text,
                                 sizeof(grade_text),
                                 state->snapshot.altitude.grade_noimu_x10);

    speed_y = (int16_t)(y + 30);
    bottom_y = (int16_t)(y + h - 10);

    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(u8g2, x + 2, y + 8, speed_max);

    u8g2_SetFont(u8g2, u8g2_font_9x15B_tr);
    u8g2_DrawStr(u8g2,
                 motor_main_center_text_x(u8g2, x, w, speed_big),
                 speed_y,
                 speed_big);

    u8g2_SetFont(u8g2, u8g2_font_6x10_tr);
    u8g2_DrawStr(u8g2,
                 motor_main_center_text_x(u8g2, x, w, speed_suffix),
                 (int16_t)(speed_y + 12),
                 speed_suffix);

    u8g2_DrawHLine(u8g2, x + 2, (int16_t)(speed_y + 16), (uint8_t)(w - 4));

    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(u8g2, x + 2, bottom_y - 8, "ALT");
    u8g2_DrawStr(u8g2, x + 2, bottom_y, alt_text);

    u8g2_DrawStr(u8g2,
                 (int16_t)(x + w - (int16_t)u8g2_GetStrWidth(u8g2, "GRADE") - 2),
                 bottom_y - 8,
                 "GRADE");
    u8g2_DrawStr(u8g2,
                 (int16_t)(x + w - (int16_t)u8g2_GetStrWidth(u8g2, grade_text) - 2),
                 bottom_y,
                 grade_text);
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

    motor_main_draw_top_gauge(u8g2,
                              outer_x,
                              gauge_y,
                              outer_w,
                              MOTOR_MAIN_TOP_GAUGE_H,
                              state);

    u8g2_DrawVLine(u8g2, split_x, middle_y, middle_h);

    motor_main_draw_left_panel(u8g2,
                               left_x,
                               middle_y,
                               left_w,
                               middle_h,
                               state);

    motor_main_draw_right_panel(u8g2,
                                right_x,
                                middle_y,
                                right_w,
                                middle_h,
                                state);

    motor_main_draw_bottom_strip(u8g2,
                                 outer_x,
                                 bottom_y,
                                 outer_w,
                                 MOTOR_MAIN_BOTTOM_RIBBON_H,
                                 state);
}
