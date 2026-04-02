#ifndef VARIO_FLIGHT_CHROME_H
#define VARIO_FLIGHT_CHROME_H

#include "Vario_Display_Common.h"
#include "Vario_FlightLayout.h"
#include "Vario_Icon_Resources.h"
#include "../Vario_Settings/Vario_Settings.h"
#include "../Vario_State/Vario_State.h"

#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef VARIO_FLIGHT_PI
#define VARIO_FLIGHT_PI 3.14159265358979323846f
#endif

static float vario_flight_wrap_360(float deg)
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

static float vario_flight_clampf(float value, float min_v, float max_v)
{
    if (value < min_v)
    {
        return min_v;
    }

    if (value > max_v)
    {
        return max_v;
    }

    return value;
}

static void vario_flight_format_clock(char *buf, size_t buf_len, const vario_runtime_t *rt)
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

static void vario_flight_format_flight_time(char *buf, size_t buf_len, const vario_runtime_t *rt)
{
    uint32_t t;
    uint32_t hh;
    uint32_t mm;
    uint32_t ss;

    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    if (rt == NULL)
    {
        snprintf(buf, buf_len, "00:00:00");
        return;
    }

    t = rt->flight_time_s;
    hh = t / 3600u;
    mm = (t % 3600u) / 60u;
    ss = t % 60u;

    snprintf(buf,
             buf_len,
             "%02lu:%02lu:%02lu",
             (unsigned long)hh,
             (unsigned long)mm,
             (unsigned long)ss);
}

static void vario_flight_format_altitude(char *buf, size_t buf_len, float altitude_m)
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

static void vario_flight_format_speed_small(char *buf, size_t buf_len, float speed_kmh)
{
    float disp;

    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    disp = Vario_Settings_SpeedKmhToDisplayFloat(speed_kmh);
    snprintf(buf, buf_len, "%.1f", (double)disp);
}

static void vario_flight_format_vspeed_small(char *buf, size_t buf_len, float vspd_mps)
{
    float disp;

    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    disp = Vario_Settings_VSpeedMpsToDisplayFloat(vspd_mps);

    if (Vario_Settings_Get()->vspeed_unit == VARIO_VSPEED_UNIT_FPM)
    {
        snprintf(buf, buf_len, "%+ld", (long)lroundf(disp));
    }
    else
    {
        snprintf(buf, buf_len, "%+.1f", (double)disp);
    }
}

static void vario_flight_format_ld(char *buf, size_t buf_len, const vario_runtime_t *rt)
{
    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    if ((rt != NULL) && (rt->glide_ratio_valid != false))
    {
        snprintf(buf, buf_len, "%.1f", (double)rt->glide_ratio);
    }
    else
    {
        snprintf(buf, buf_len, "--.-");
    }
}

static float vario_flight_deg_to_rad(float deg)
{
    return deg * (VARIO_FLIGHT_PI / 180.0f);
}

/* -------------------------------------------------------------------------- */
/* split-number helper                                                         */
/*                                                                            */
/* 목적                                                                       */
/* - 큰 숫자의 정수부는 굵고 크게                                              */
/* - 소수 한 자리는 오른쪽 위에 더 작은 글꼴로 배치                           */
/* - label, unit, align, box width 를 전부 파라미터화                         */
/*                                                                            */
/* 조절 포인트                                                                 */
/* - X / W            : 고정 박스 위치와 폭                                    */
/* - *_BASELINE       : label/value/unit 세로 위치                             */
/* - *_FONT           : 각 글자 크기/굵기                                      */
/* - align            : left/center/right                                      */
/* -------------------------------------------------------------------------- */
static void vario_flight_draw_split_value(u8g2_t *u8g2,
                                          int16_t box_x,
                                          int16_t box_w,
                                          int16_t label_baseline,
                                          int16_t value_baseline,
                                          int16_t unit_baseline,
                                          vario_text_align_t align,
                                          const uint8_t *label_font,
                                          const uint8_t *major_font,
                                          const uint8_t *sign_font,
                                          const uint8_t *minor_font,
                                          const uint8_t *unit_font,
                                          const char *label,
                                          const char *unit,
                                          float value,
                                          bool show_sign,
                                          bool show_decimal)
{
    char major_text[8];
    char minor_text[4];
    char sign_text[2];
    int32_t scaled_x10;
    int32_t whole;
    int32_t frac;
    float abs_value;
    int16_t sign_w;
    int16_t major_w;
    int16_t minor_w;
    int16_t total_w;
    int16_t draw_x;
    int16_t major_ascent;
    int16_t minor_ascent;
    int16_t minor_baseline;

    if ((u8g2 == NULL) || (box_w <= 0))
    {
        return;
    }

    abs_value = (value < 0.0f) ? -value : value;
    scaled_x10 = (int32_t)lroundf(abs_value * 10.0f);
    whole = scaled_x10 / 10;
    frac  = scaled_x10 % 10;

    snprintf(major_text, sizeof(major_text), "%ld", (long)whole);
    snprintf(minor_text, sizeof(minor_text), "%ld", (long)frac);
    sign_text[0] = '\0';
    sign_text[1] = '\0';

    if (show_sign != false)
    {
        sign_text[0] = (value < 0.0f) ? '-' : '+';
    }

    if ((label != NULL) && (label[0] != '\0'))
    {
        u8g2_SetFont(u8g2, label_font);
        Vario_Display_DrawTextInFixedBox(u8g2,
                                         box_x,
                                         box_w,
                                         label_baseline,
                                         align,
                                         label);
    }

    sign_w = 0;
    if (show_sign != false)
    {
        u8g2_SetFont(u8g2, sign_font);
        sign_w = (int16_t)u8g2_GetStrWidth(u8g2, sign_text);
    }

    u8g2_SetFont(u8g2, major_font);
    major_w = (int16_t)u8g2_GetStrWidth(u8g2, major_text);
    major_ascent = (int16_t)u8g2_GetAscent(u8g2);

    minor_w = 0;
    minor_ascent = 0;
    if (show_decimal != false)
    {
        u8g2_SetFont(u8g2, minor_font);
        minor_w = (int16_t)u8g2_GetStrWidth(u8g2, minor_text);
        minor_ascent = (int16_t)u8g2_GetAscent(u8g2);
    }

    total_w = sign_w + major_w + ((show_decimal != false) ? (minor_w + 1) : 0);

    switch (align)
    {
        case VARIO_TEXT_ALIGN_RIGHT:
            draw_x = (int16_t)(box_x + box_w - total_w);
            break;

        case VARIO_TEXT_ALIGN_CENTER:
            draw_x = (int16_t)(box_x + ((box_w - total_w) / 2));
            break;

        case VARIO_TEXT_ALIGN_LEFT:
        default:
            draw_x = box_x;
            break;
    }

    if (draw_x < 0)
    {
        draw_x = 0;
    }

    if (show_sign != false)
    {
        u8g2_SetFont(u8g2, sign_font);
        u8g2_DrawStr(u8g2, (uint8_t)draw_x, (uint8_t)value_baseline, sign_text);
        draw_x = (int16_t)(draw_x + sign_w);
    }

    u8g2_SetFont(u8g2, major_font);
    u8g2_DrawStr(u8g2, (uint8_t)draw_x, (uint8_t)value_baseline, major_text);

    if (show_decimal != false)
    {
        minor_baseline = (int16_t)((value_baseline - major_ascent) + minor_ascent + 1);
        u8g2_SetFont(u8g2, minor_font);
        u8g2_DrawStr(u8g2,
                     (uint8_t)(draw_x + major_w + 1),
                     (uint8_t)minor_baseline,
                     minor_text);
    }

    if ((unit != NULL) && (unit[0] != '\0'))
    {
        u8g2_SetFont(u8g2, unit_font);
        Vario_Display_DrawTextInFixedBox(u8g2,
                                         box_x,
                                         box_w,
                                         unit_baseline,
                                         align,
                                         unit);
    }
}

static void vario_flight_compass_label_for_heading(int16_t heading_deg, char *buf, size_t buf_len)
{
    int16_t norm;

    norm = heading_deg;
    while (norm < 0)
    {
        norm += 360;
    }
    while (norm >= 360)
    {
        norm -= 360;
    }

    if (norm == 0)
    {
        snprintf(buf, buf_len, "N");
    }
    else if (norm == 90)
    {
        snprintf(buf, buf_len, "E");
    }
    else if (norm == 180)
    {
        snprintf(buf, buf_len, "S");
    }
    else if (norm == 270)
    {
        snprintf(buf, buf_len, "W");
    }
    else
    {
        snprintf(buf, buf_len, "%d", norm);
    }
}

static void vario_flight_draw_left_vario_bar(u8g2_t *u8g2,
                                             const vario_viewport_t *v,
                                             const vario_settings_t *settings,
                                             const vario_runtime_t *rt)
{
    int16_t x;
    int16_t y;
    int16_t h;
    int16_t center_y;
    float   range_mps;
    float   inst_mps;
    float   avg_mps;
    int32_t tick_mps_x10;
    int16_t inst_top_y;
    int16_t inst_bottom_y;
    int16_t avg_top_y;
    int16_t avg_bottom_y;

    if ((u8g2 == NULL) || (v == NULL) || (settings == NULL) || (rt == NULL))
    {
        return;
    }

    x = (int16_t)(v->x + VARIO_FLIGHT_SIDE_BAR_LEFT_X_OFF);
    y = v->y;
    h = v->h;
    center_y = (int16_t)(y + (h / 2));
    range_mps = ((float)settings->vario_range_mps_x10) * 0.1f;

    if (range_mps < 1.0f)
    {
        range_mps = 1.0f;
    }

    inst_mps = vario_flight_clampf(rt->baro_vario_mps, -range_mps, range_mps);
    avg_mps  = vario_flight_clampf(rt->average_vario_mps, -range_mps, range_mps);

    for (tick_mps_x10 = -(int32_t)settings->vario_range_mps_x10;
         tick_mps_x10 <= (int32_t)settings->vario_range_mps_x10;
         tick_mps_x10 += 5)
    {
        float   tick_ratio;
        int16_t tick_y;
        uint8_t tick_len;
        int16_t tick_x;

        tick_ratio = ((float)tick_mps_x10) / ((float)settings->vario_range_mps_x10);
        tick_y = (int16_t)(center_y - (tick_ratio * ((float)h * 0.5f)));
        tick_len = ((tick_mps_x10 % 10) == 0) ?
                       (uint8_t)VARIO_FLIGHT_VARIO_BAR_MAJOR_TICK_W :
                       (uint8_t)VARIO_FLIGHT_VARIO_BAR_MINOR_TICK_W;

        tick_x = (int16_t)(x + VARIO_FLIGHT_SIDE_BAR_W - tick_len);
        u8g2_DrawHLine(u8g2, (uint8_t)tick_x, (uint8_t)tick_y, tick_len);
    }

    if (inst_mps >= 0.0f)
    {
        inst_top_y = (int16_t)(center_y - ((inst_mps / range_mps) * ((float)h * 0.5f)));
        inst_bottom_y = center_y;
    }
    else
    {
        inst_top_y = center_y;
        inst_bottom_y = (int16_t)(center_y - ((inst_mps / range_mps) * ((float)h * 0.5f)));
    }

    if (avg_mps >= 0.0f)
    {
        avg_top_y = (int16_t)(center_y - ((avg_mps / range_mps) * ((float)h * 0.5f)));
        avg_bottom_y = center_y;
    }
    else
    {
        avg_top_y = center_y;
        avg_bottom_y = (int16_t)(center_y - ((avg_mps / range_mps) * ((float)h * 0.5f)));
    }

    if (inst_bottom_y < inst_top_y)
    {
        int16_t tmp;
        tmp = inst_top_y;
        inst_top_y = inst_bottom_y;
        inst_bottom_y = tmp;
    }

    if (avg_bottom_y < avg_top_y)
    {
        int16_t tmp;
        tmp = avg_top_y;
        avg_top_y = avg_bottom_y;
        avg_bottom_y = tmp;
    }

    u8g2_DrawBox(u8g2,
                 (uint8_t)(x + VARIO_FLIGHT_VARIO_BAR_FILL_X_OFF),
                 (uint8_t)inst_top_y,
                 VARIO_FLIGHT_SIDE_BAR_FILL_W,
                 (uint8_t)(inst_bottom_y - inst_top_y));

    u8g2_DrawBox(u8g2,
                 (uint8_t)(x + VARIO_FLIGHT_VARIO_BAR_AVG_X_OFF),
                 (uint8_t)avg_top_y,
                 VARIO_FLIGHT_SIDE_BAR_AVG_W,
                 (uint8_t)(avg_bottom_y - avg_top_y));
}

static void vario_flight_draw_right_gs_bar(u8g2_t *u8g2,
                                           const vario_viewport_t *v,
                                           const vario_settings_t *settings,
                                           const vario_runtime_t *rt)
{
    int16_t x;
    int16_t y;
    int16_t h;
    float   gs_max;
    float   gs_kmh;
    float   ratio;
    int16_t fill_top_y;
    uint8_t tick;

    if ((u8g2 == NULL) || (v == NULL) || (settings == NULL) || (rt == NULL))
    {
        return;
    }

    if (settings->show_gs_bar == 0u)
    {
        return;
    }

    x = (int16_t)(v->x + v->w - VARIO_FLIGHT_SIDE_BAR_W - VARIO_FLIGHT_SIDE_BAR_RIGHT_MARGIN);
    y = v->y;
    h = v->h;
    gs_max = (float)settings->gs_range_kmh;
    if (gs_max < 10.0f)
    {
        gs_max = 10.0f;
    }

    gs_kmh = vario_flight_clampf(rt->ground_speed_kmh, 0.0f, gs_max);
    ratio = gs_kmh / gs_max;
    fill_top_y = (int16_t)(y + h - (ratio * (float)h));

    for (tick = 0u; tick <= 10u; ++tick)
    {
        int16_t tick_y;
        uint8_t tick_len;

        tick_y = (int16_t)(y + ((h * (int16_t)tick) / 10));
        tick_len = ((tick % 2u) == 0u) ?
                       (uint8_t)VARIO_FLIGHT_GS_BAR_MAJOR_TICK_W :
                       (uint8_t)VARIO_FLIGHT_GS_BAR_MINOR_TICK_W;
        u8g2_DrawHLine(u8g2, (uint8_t)x, (uint8_t)tick_y, tick_len);
    }

    u8g2_DrawBox(u8g2,
                 (uint8_t)(x + VARIO_FLIGHT_GS_BAR_FILL_X_OFF),
                 (uint8_t)fill_top_y,
                 VARIO_FLIGHT_SIDE_BAR_FILL_W,
                 (uint8_t)((y + h) - fill_top_y));
}

static void vario_flight_draw_clock(u8g2_t *u8g2,
                                    const vario_viewport_t *v,
                                    const vario_settings_t *settings,
                                    const vario_runtime_t *rt)
{
    char clock_text[20];

    if ((u8g2 == NULL) || (v == NULL) || (settings == NULL))
    {
        return;
    }

    if (settings->show_current_time == 0u)
    {
        return;
    }

    vario_flight_format_clock(clock_text, sizeof(clock_text), rt);
    u8g2_SetFont(u8g2, VARIO_FLIGHT_CLOCK_FONT);
    Vario_Display_DrawTextCentered(u8g2,
                                   (int16_t)(v->x + VARIO_FLIGHT_CLOCK_CENTER_X_OFF),
                                   (int16_t)(v->y + VARIO_FLIGHT_CLOCK_BASELINE_Y_OFF),
                                   clock_text);
}

static void vario_flight_draw_right_alt_block(u8g2_t *u8g2,
                                              const vario_viewport_t *v,
                                              const vario_runtime_t *rt)
{
    int16_t right_x;
    int16_t box_x;
    int16_t alt1_value_box_x;
    int16_t alt1_unit_box_x;
    int16_t row_text_box_x;
    int16_t row_text_box_w;
    char    alt1_text[20];
    char    alt2_text[20];
    char    alt3_text[20];

    if ((u8g2 == NULL) || (v == NULL) || (rt == NULL))
    {
        return;
    }

    right_x = (int16_t)(v->x + v->w - VARIO_FLIGHT_ALT_BLOCK_RIGHT_MARGIN);
    box_x   = (int16_t)(right_x - VARIO_FLIGHT_ALT_BLOCK_W);
    alt1_unit_box_x = (int16_t)(right_x - VARIO_FLIGHT_ALT1_UNIT_BOX_W);
    alt1_value_box_x = (int16_t)(alt1_unit_box_x - VARIO_FLIGHT_ALT1_VALUE_UNIT_GAP - VARIO_FLIGHT_ALT1_VALUE_BOX_W);
    row_text_box_x = (int16_t)(box_x + VARIO_ICON_ALT2_WIDTH + VARIO_FLIGHT_ALT_ROW_ICON_GAP);
    row_text_box_w = (int16_t)(right_x - row_text_box_x);

    vario_flight_format_altitude(alt1_text, sizeof(alt1_text), rt->alt1_absolute_m);
    vario_flight_format_altitude(alt2_text, sizeof(alt2_text), rt->alt2_relative_m);
    vario_flight_format_altitude(alt3_text, sizeof(alt3_text), rt->alt3_accum_gain_m);

    u8g2_SetFont(u8g2, VARIO_FLIGHT_ALT_LABEL_FONT);
    Vario_Display_DrawTextInFixedBox(u8g2,
                                     box_x,
                                     VARIO_FLIGHT_ALT_BLOCK_W,
                                     (int16_t)(v->y + VARIO_FLIGHT_ALT1_LABEL_BASELINE_Y_OFF),
                                     VARIO_FLIGHT_ALT1_ALIGN,
                                     "ALT1");

    u8g2_SetFont(u8g2, VARIO_FLIGHT_ALT_VALUE_FONT);
    Vario_Display_DrawTextInFixedBox(u8g2,
                                     alt1_value_box_x,
                                     VARIO_FLIGHT_ALT1_VALUE_BOX_W,
                                     (int16_t)(v->y + VARIO_FLIGHT_ALT1_VALUE_BASELINE_Y_OFF),
                                     VARIO_FLIGHT_ALT1_ALIGN,
                                     alt1_text);

    u8g2_SetFont(u8g2, VARIO_FLIGHT_ALT_UNIT_FONT);
    Vario_Display_DrawTextInFixedBox(u8g2,
                                     alt1_unit_box_x,
                                     VARIO_FLIGHT_ALT1_UNIT_BOX_W,
                                     (int16_t)(v->y + VARIO_FLIGHT_ALT1_UNIT_BASELINE_Y_OFF),
                                     VARIO_FLIGHT_ALT1_ALIGN,
                                     Vario_Settings_GetAltitudeUnitText());

    u8g2_DrawXBM(u8g2,
                 (uint8_t)box_x,
                 (uint8_t)(v->y + VARIO_FLIGHT_ALT2_ROW_BASELINE_Y_OFF - VARIO_ICON_ALT2_HEIGHT + 1),
                 VARIO_ICON_ALT2_WIDTH,
                 VARIO_ICON_ALT2_HEIGHT,
                 vario_icon_alt2_bits);
    u8g2_DrawXBM(u8g2,
                 (uint8_t)box_x,
                 (uint8_t)(v->y + VARIO_FLIGHT_ALT3_ROW_BASELINE_Y_OFF - VARIO_ICON_ALT3_HEIGHT + 1),
                 VARIO_ICON_ALT3_WIDTH,
                 VARIO_ICON_ALT3_HEIGHT,
                 vario_icon_alt3_bits);

    u8g2_SetFont(u8g2, VARIO_FLIGHT_ALT_ROW_FONT);
    Vario_Display_DrawTextInFixedBox(u8g2,
                                     row_text_box_x,
                                     row_text_box_w,
                                     (int16_t)(v->y + VARIO_FLIGHT_ALT2_ROW_BASELINE_Y_OFF),
                                     VARIO_FLIGHT_ALT_ROW_ALIGN,
                                     alt2_text);
    Vario_Display_DrawTextInFixedBox(u8g2,
                                     row_text_box_x,
                                     row_text_box_w,
                                     (int16_t)(v->y + VARIO_FLIGHT_ALT3_ROW_BASELINE_Y_OFF),
                                     VARIO_FLIGHT_ALT_ROW_ALIGN,
                                     alt3_text);
}

static void vario_flight_draw_left_info_block(u8g2_t *u8g2,
                                              const vario_viewport_t *v,
                                              const vario_settings_t *settings,
                                              const vario_runtime_t *rt)
{
    int16_t label_x;
    int16_t value_x;
    char    avg_text[20];
    char    max_text[20];
    char    flt_text[24];
    char    ld_text[16];

    if ((u8g2 == NULL) || (v == NULL) || (settings == NULL) || (rt == NULL))
    {
        return;
    }

    label_x = (int16_t)(v->x + VARIO_FLIGHT_INFO_LEFT_X_OFF);
    value_x = (int16_t)(label_x + VARIO_FLIGHT_INFO_LABEL_W);

    vario_flight_format_vspeed_small(avg_text, sizeof(avg_text), rt->average_vario_mps);
    vario_flight_format_vspeed_small(max_text, sizeof(max_text), rt->max_top_vario_mps);
    vario_flight_format_flight_time(flt_text, sizeof(flt_text), rt);
    vario_flight_format_ld(ld_text, sizeof(ld_text), rt);

    u8g2_SetFont(u8g2, VARIO_FLIGHT_INFO_LABEL_FONT);
    Vario_Display_DrawTextInFixedBox(u8g2,
                                     label_x,
                                     VARIO_FLIGHT_INFO_LABEL_W,
                                     (int16_t)(v->y + VARIO_FLIGHT_INFO_AVG_BASELINE_Y_OFF),
                                     VARIO_FLIGHT_INFO_LABEL_ALIGN,
                                     "AVG");
    Vario_Display_DrawTextInFixedBox(u8g2,
                                     label_x,
                                     VARIO_FLIGHT_INFO_LABEL_W,
                                     (int16_t)(v->y + VARIO_FLIGHT_INFO_MAX_BASELINE_Y_OFF),
                                     VARIO_FLIGHT_INFO_LABEL_ALIGN,
                                     "MAX");
    Vario_Display_DrawTextInFixedBox(u8g2,
                                     label_x,
                                     VARIO_FLIGHT_INFO_LABEL_W,
                                     (int16_t)(v->y + VARIO_FLIGHT_INFO_FLT_BASELINE_Y_OFF),
                                     VARIO_FLIGHT_INFO_LABEL_ALIGN,
                                     "FLT");
    Vario_Display_DrawTextInFixedBox(u8g2,
                                     label_x,
                                     VARIO_FLIGHT_INFO_LABEL_W,
                                     (int16_t)(v->y + VARIO_FLIGHT_INFO_LD_BASELINE_Y_OFF),
                                     VARIO_FLIGHT_INFO_LABEL_ALIGN,
                                     "L/D");

    u8g2_SetFont(u8g2, VARIO_FLIGHT_INFO_VALUE_FONT);
    Vario_Display_DrawTextInFixedBox(u8g2,
                                     value_x,
                                     VARIO_FLIGHT_INFO_VALUE_W,
                                     (int16_t)(v->y + VARIO_FLIGHT_INFO_AVG_BASELINE_Y_OFF),
                                     VARIO_FLIGHT_INFO_VALUE_ALIGN,
                                     avg_text);
    if (settings->show_max_vario != 0u)
    {
        Vario_Display_DrawTextInFixedBox(u8g2,
                                         value_x,
                                         VARIO_FLIGHT_INFO_VALUE_W,
                                         (int16_t)(v->y + VARIO_FLIGHT_INFO_MAX_BASELINE_Y_OFF),
                                         VARIO_FLIGHT_INFO_VALUE_ALIGN,
                                         max_text);
    }
    if (settings->show_flight_time != 0u)
    {
        Vario_Display_DrawTextInFixedBox(u8g2,
                                         value_x,
                                         VARIO_FLIGHT_INFO_VALUE_W,
                                         (int16_t)(v->y + VARIO_FLIGHT_INFO_FLT_BASELINE_Y_OFF),
                                         VARIO_FLIGHT_INFO_VALUE_ALIGN,
                                         flt_text);
    }
    Vario_Display_DrawTextInFixedBox(u8g2,
                                     value_x,
                                     VARIO_FLIGHT_INFO_VALUE_W,
                                     (int16_t)(v->y + VARIO_FLIGHT_INFO_LD_BASELINE_Y_OFF),
                                     VARIO_FLIGHT_INFO_VALUE_ALIGN,
                                     ld_text);
}

static void vario_flight_draw_left_vario_box(u8g2_t *u8g2,
                                             const vario_viewport_t *v,
                                             const vario_runtime_t *rt)
{
    if ((u8g2 == NULL) || (v == NULL) || (rt == NULL))
    {
        return;
    }

    if (Vario_Settings_Get()->vspeed_unit == VARIO_VSPEED_UNIT_FPM)
    {
        char value_text[20];
        vario_flight_format_vspeed_small(value_text, sizeof(value_text), rt->baro_vario_mps);
        u8g2_SetFont(u8g2, VARIO_FLIGHT_VARIO_LABEL_FONT);
        Vario_Display_DrawTextInFixedBox(u8g2,
                                         (int16_t)(v->x + VARIO_FLIGHT_VARIO_BOX_X_OFF),
                                         VARIO_FLIGHT_VARIO_BOX_W,
                                         (int16_t)(v->y + VARIO_FLIGHT_VARIO_LABEL_BASELINE_Y_OFF),
                                         VARIO_FLIGHT_VARIO_ALIGN,
                                         "VARIO");
        u8g2_SetFont(u8g2, VARIO_FLIGHT_VARIO_MAJOR_FONT);
        Vario_Display_DrawTextInFixedBox(u8g2,
                                         (int16_t)(v->x + VARIO_FLIGHT_VARIO_BOX_X_OFF),
                                         VARIO_FLIGHT_VARIO_BOX_W,
                                         (int16_t)(v->y + VARIO_FLIGHT_VARIO_VALUE_BASELINE_Y_OFF),
                                         VARIO_FLIGHT_VARIO_ALIGN,
                                         value_text);
        u8g2_SetFont(u8g2, VARIO_FLIGHT_VARIO_UNIT_FONT);
        Vario_Display_DrawTextInFixedBox(u8g2,
                                         (int16_t)(v->x + VARIO_FLIGHT_VARIO_BOX_X_OFF),
                                         VARIO_FLIGHT_VARIO_BOX_W,
                                         (int16_t)(v->y + VARIO_FLIGHT_VARIO_UNIT_BASELINE_Y_OFF),
                                         VARIO_FLIGHT_VARIO_ALIGN,
                                         Vario_Settings_GetVSpeedUnitText());
        return;
    }

    vario_flight_draw_split_value(u8g2,
                                  (int16_t)(v->x + VARIO_FLIGHT_VARIO_BOX_X_OFF),
                                  VARIO_FLIGHT_VARIO_BOX_W,
                                  (int16_t)(v->y + VARIO_FLIGHT_VARIO_LABEL_BASELINE_Y_OFF),
                                  (int16_t)(v->y + VARIO_FLIGHT_VARIO_VALUE_BASELINE_Y_OFF),
                                  (int16_t)(v->y + VARIO_FLIGHT_VARIO_UNIT_BASELINE_Y_OFF),
                                  VARIO_FLIGHT_VARIO_ALIGN,
                                  VARIO_FLIGHT_VARIO_LABEL_FONT,
                                  VARIO_FLIGHT_VARIO_MAJOR_FONT,
                                  VARIO_FLIGHT_VARIO_SIGN_FONT,
                                  VARIO_FLIGHT_VARIO_MINOR_FONT,
                                  VARIO_FLIGHT_VARIO_UNIT_FONT,
                                  "VARIO",
                                  Vario_Settings_GetVSpeedUnitText(),
                                  vario_flight_clampf(rt->baro_vario_mps, -19.9f, 19.9f),
                                  true,
                                  true);
}

static void vario_flight_draw_right_gs_box(u8g2_t *u8g2,
                                           const vario_viewport_t *v,
                                           const vario_runtime_t *rt)
{
    float display_speed;
    int16_t box_x;

    if ((u8g2 == NULL) || (v == NULL) || (rt == NULL))
    {
        return;
    }

    box_x = (int16_t)(v->x + v->w - VARIO_FLIGHT_GS_BOX_RIGHT_MARGIN - VARIO_FLIGHT_GS_BOX_W);
    display_speed = Vario_Settings_SpeedKmhToDisplayFloat(rt->ground_speed_kmh);

    vario_flight_draw_split_value(u8g2,
                                  box_x,
                                  VARIO_FLIGHT_GS_BOX_W,
                                  (int16_t)(v->y + VARIO_FLIGHT_GS_LABEL_BASELINE_Y_OFF),
                                  (int16_t)(v->y + VARIO_FLIGHT_GS_VALUE_BASELINE_Y_OFF),
                                  (int16_t)(v->y + VARIO_FLIGHT_GS_UNIT_BASELINE_Y_OFF),
                                  VARIO_FLIGHT_GS_ALIGN,
                                  VARIO_FLIGHT_GS_LABEL_FONT,
                                  VARIO_FLIGHT_GS_MAJOR_FONT,
                                  VARIO_FLIGHT_GS_MAJOR_FONT,
                                  VARIO_FLIGHT_GS_MINOR_FONT,
                                  VARIO_FLIGHT_GS_UNIT_FONT,
                                  "GS",
                                  Vario_Settings_GetSpeedUnitText(),
                                  display_speed,
                                  false,
                                  true);
}

static void vario_flight_draw_altitude_trend(u8g2_t *u8g2,
                                             const vario_viewport_t *v,
                                             const vario_runtime_t *rt)
{
    int16_t graph_x;
    int16_t graph_y;
    int16_t graph_w;
    int16_t graph_h;
    float   min_alt;
    float   max_alt;
    float   span_alt;
    uint16_t i;
    int16_t prev_x;
    int16_t prev_y;
    bool    prev_valid;

    if ((u8g2 == NULL) || (v == NULL) || (rt == NULL))
    {
        return;
    }

    if (rt->history_count < 2u)
    {
        return;
    }

    graph_x = (int16_t)(v->x + VARIO_FLIGHT_ALT_TREND_X_OFF);
    graph_y = (int16_t)(v->y + VARIO_FLIGHT_ALT_TREND_Y_OFF);
    graph_w = VARIO_FLIGHT_ALT_TREND_W;
    graph_h = VARIO_FLIGHT_ALT_TREND_H;

    {
        uint16_t oldest_idx;
        oldest_idx = (uint16_t)((rt->history_head + VARIO_HISTORY_MAX_SAMPLES - rt->history_count) % VARIO_HISTORY_MAX_SAMPLES);
        min_alt = rt->history_altitude_m[oldest_idx];
        max_alt = rt->history_altitude_m[oldest_idx];
    }

    for (i = 0u; i < rt->history_count; ++i)
    {
        uint16_t idx;
        idx = (uint16_t)((rt->history_head + VARIO_HISTORY_MAX_SAMPLES - rt->history_count + i) % VARIO_HISTORY_MAX_SAMPLES);
        if (rt->history_altitude_m[idx] < min_alt)
        {
            min_alt = rt->history_altitude_m[idx];
        }
        if (rt->history_altitude_m[idx] > max_alt)
        {
            max_alt = rt->history_altitude_m[idx];
        }
    }

    span_alt = max_alt - min_alt;
    if (span_alt < 4.0f)
    {
        min_alt -= 2.0f;
        max_alt += 2.0f;
        span_alt = max_alt - min_alt;
    }

    prev_valid = false;
    prev_x = 0;
    prev_y = 0;

    for (i = 0u; i < rt->history_count; ++i)
    {
        uint16_t idx;
        float    altitude_m;
        int16_t  px;
        int16_t  py;

        idx = (uint16_t)((rt->history_head + VARIO_HISTORY_MAX_SAMPLES - rt->history_count + i) % VARIO_HISTORY_MAX_SAMPLES);
        altitude_m = rt->history_altitude_m[idx];
        px = (int16_t)(graph_x + ((i * (graph_w - 1)) / (rt->history_count - 1u)));
        py = (int16_t)(graph_y + graph_h - 1 -
                       (((altitude_m - min_alt) / span_alt) * (float)(graph_h - 1)));

        if (prev_valid != false)
        {
            u8g2_DrawLine(u8g2, (uint8_t)prev_x, (uint8_t)prev_y, (uint8_t)px, (uint8_t)py);
        }

        prev_x = px;
        prev_y = py;
        prev_valid = true;
    }

    u8g2_DrawPixel(u8g2, (uint8_t)prev_x, (uint8_t)prev_y);
    u8g2_DrawPixel(u8g2, (uint8_t)prev_x, (uint8_t)(prev_y - 1));
    u8g2_DrawPixel(u8g2, (uint8_t)prev_x, (uint8_t)(prev_y + 1));
}

static void vario_flight_draw_compass_tape(u8g2_t *u8g2,
                                           const vario_viewport_t *v,
                                           const vario_settings_t *settings,
                                           const vario_runtime_t *rt)
{
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
    int16_t center_x;
    int16_t center_heading;
    int16_t span_deg;
    int16_t deg;
    char    label[8];

    if ((u8g2 == NULL) || (v == NULL) || (settings == NULL) || (rt == NULL))
    {
        return;
    }

    x = (int16_t)(v->x + VARIO_FLIGHT_COMPASS_X_OFF);
    y = (int16_t)(v->y + VARIO_FLIGHT_COMPASS_Y_OFF);
    w = VARIO_FLIGHT_COMPASS_W;
    h = VARIO_FLIGHT_COMPASS_H;
    center_x = (int16_t)(x + (w / 2));
    center_heading = (rt->heading_valid != false) ? (int16_t)lroundf(rt->heading_deg) : 0;
    span_deg = (int16_t)settings->compass_span_deg;

    u8g2_DrawFrame(u8g2, (uint8_t)x, (uint8_t)y, (uint8_t)w, (uint8_t)h);

    for (deg = center_heading - (span_deg / 2); deg <= center_heading + (span_deg / 2); deg += 5)
    {
        int16_t diff_deg;
        int16_t tick_x;
        uint8_t tick_h;

        diff_deg = (int16_t)(deg - center_heading);
        tick_x = (int16_t)(center_x + (((float)diff_deg / (float)span_deg) * (float)w));
        if ((tick_x <= x) || (tick_x >= (x + w - 1)))
        {
            continue;
        }

        tick_h = ((deg % 30) == 0) ? (uint8_t)(h - 5) : (((deg % 15) == 0) ? (uint8_t)(h - 8) : 4u);
        u8g2_DrawVLine(u8g2, (uint8_t)tick_x, (uint8_t)(y + h - tick_h - 1), tick_h);

        if ((deg % 30) == 0)
        {
            vario_flight_compass_label_for_heading(deg, label, sizeof(label));
            u8g2_SetFont(u8g2, VARIO_FLIGHT_COMPASS_LABEL_FONT);
            Vario_Display_DrawTextCentered(u8g2, tick_x, (int16_t)(y + 6), label);
        }
    }

    u8g2_DrawVLine(u8g2, (uint8_t)center_x, (uint8_t)(y + 1), (uint8_t)(h - 2));
    u8g2_DrawLine(u8g2, (uint8_t)(center_x - 3), (uint8_t)(y + 3), (uint8_t)center_x, (uint8_t)(y + 1));
    u8g2_DrawLine(u8g2, (uint8_t)(center_x + 3), (uint8_t)(y + 3), (uint8_t)center_x, (uint8_t)(y + 1));
}

static void Vario_FlightChrome_GetPage2MapViewport(const vario_viewport_t *full_v,
                                                   vario_viewport_t *map_v)
{
    if ((full_v == NULL) || (map_v == NULL))
    {
        return;
    }

    map_v->x = (int16_t)(full_v->x + VARIO_FLIGHT_PAGE2_MAP_X_OFF);
    map_v->y = (int16_t)(full_v->y + VARIO_FLIGHT_PAGE2_MAP_Y_OFF);
    map_v->w = (int16_t)(full_v->w - (VARIO_FLIGHT_SIDE_BAR_W * 2));
    map_v->h = full_v->h;
}

static void Vario_FlightChrome_DrawPage1(u8g2_t *u8g2,
                                         const vario_viewport_t *v,
                                         const vario_settings_t *settings,
                                         const vario_runtime_t *rt)
{
    if ((u8g2 == NULL) || (v == NULL) || (settings == NULL) || (rt == NULL))
    {
        return;
    }

    vario_flight_draw_left_vario_bar(u8g2, v, settings, rt);
    vario_flight_draw_right_gs_bar(u8g2, v, settings, rt);
    vario_flight_draw_clock(u8g2, v, settings, rt);
    vario_flight_draw_right_alt_block(u8g2, v, rt);
    vario_flight_draw_left_info_block(u8g2, v, settings, rt);
    vario_flight_draw_left_vario_box(u8g2, v, rt);
    vario_flight_draw_right_gs_box(u8g2, v, rt);
    vario_flight_draw_altitude_trend(u8g2, v, rt);
    vario_flight_draw_compass_tape(u8g2, v, settings, rt);
}

static void Vario_FlightChrome_DrawPage2Overlays(u8g2_t *u8g2,
                                                 const vario_viewport_t *v,
                                                 const vario_settings_t *settings,
                                                 const vario_runtime_t *rt)
{
    char flt_text[24];

    if ((u8g2 == NULL) || (v == NULL) || (settings == NULL) || (rt == NULL))
    {
        return;
    }

    vario_flight_draw_left_vario_bar(u8g2, v, settings, rt);
    vario_flight_draw_right_gs_bar(u8g2, v, settings, rt);
    vario_flight_draw_clock(u8g2, v, settings, rt);
    vario_flight_draw_right_alt_block(u8g2, v, rt);
    vario_flight_draw_left_vario_box(u8g2, v, rt);
    vario_flight_draw_right_gs_box(u8g2, v, rt);

    if (settings->show_flight_time != 0u)
    {
        vario_flight_format_flight_time(flt_text, sizeof(flt_text), rt);
        u8g2_SetFont(u8g2, VARIO_FLIGHT_PAGE2_FLT_FONT);
        u8g2_DrawStr(u8g2,
                     (uint8_t)(v->x + VARIO_FLIGHT_PAGE2_FLT_X_OFF),
                     (uint8_t)(v->y + VARIO_FLIGHT_PAGE2_FLT_BASELINE_Y_OFF),
                     flt_text);
    }
}

#ifdef __cplusplus
}
#endif

#endif /* VARIO_FLIGHT_CHROME_H */
