#include "Vario_Screen1.h"

#include "Vario_Display_Common.h"
#include "../Vario_Settings/Vario_Settings.h"
#include "../Vario_State/Vario_State.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*  Page 1 layout constants                                                    */
/*                                                                            */
/*  사용자가 요구한 핵심 레이아웃                                              */
/*  - 좌측 14px flush bar   : GS bar                                           */
/*  - 우측 14px flush bar   : VARIO bar                                        */
/*  - 우측 상단             : ALT1/ALT2/ALT3/GS                                */
/*  - 좌측 상단             : VARIO / MAX VARIO / FLIGHT TIME                  */
/*  - 하단 중앙 16px 안팎   : compass tape box                                 */
/*                                                                            */
/*  이 숫자들을 조절하면 각 UI 블록의 위치/비율이 바뀐다.                       */
/* -------------------------------------------------------------------------- */
#define VARIO_SCREEN1_SIDE_BAR_W         14
#define VARIO_SCREEN1_SIDE_BAR_INNER_W    8
#define VARIO_SCREEN1_SIDE_BAR_FILL_OFF   3
#define VARIO_SCREEN1_CONTENT_L_PAD       4
#define VARIO_SCREEN1_CONTENT_R_PAD       4
#define VARIO_SCREEN1_TOP_TIME_Y          7
#define VARIO_SCREEN1_LEFT_BLOCK_X       18
#define VARIO_SCREEN1_LEFT_BLOCK_Y       18
#define VARIO_SCREEN1_RIGHT_BLOCK_W      92
#define VARIO_SCREEN1_RIGHT_BLOCK_TOP_Y  18
#define VARIO_SCREEN1_BIG_VARIO_Y        92
#define VARIO_SCREEN1_HEADING_LINE_Y     96
#define VARIO_SCREEN1_COMPASS_MARGIN_X   22
#define VARIO_SCREEN1_COMPASS_BOTTOM_GAP  0

static int32_t vario_screen1_round_tenths(float value)
{
    return (int32_t)lroundf(value * 10.0f);
}

static void vario_screen1_format_clock(char *buf, size_t buf_len, const vario_runtime_t *rt)
{
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

static void vario_screen1_format_flight_time(char *buf, size_t buf_len, const vario_runtime_t *rt)
{
    uint32_t t;
    uint32_t hh;
    uint32_t mm;
    uint32_t ss;

    if (rt == NULL)
    {
        snprintf(buf, buf_len, "00:00:00");
        return;
    }

    t = rt->flight_time_s;
    hh = t / 3600u;
    mm = (t % 3600u) / 60u;
    ss = t % 60u;

    snprintf(buf, buf_len, "%02lu:%02lu:%02lu", (unsigned long)hh, (unsigned long)mm, (unsigned long)ss);
}

static void vario_screen1_format_altitude(char *buf, size_t buf_len, float altitude_m)
{
    snprintf(buf,
             buf_len,
             "%ld",
             (long)Vario_Settings_AltitudeMetersToDisplayRounded(altitude_m));
}

static void vario_screen1_format_speed(char *buf, size_t buf_len, float speed_kmh)
{
    snprintf(buf,
             buf_len,
             "%ld",
             (long)Vario_Settings_SpeedToDisplayRounded(speed_kmh));
}

static void vario_screen1_format_signed_vspeed(char *buf, size_t buf_len, float vspd_mps)
{
    float disp;

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

static void vario_screen1_format_heading(char *buf, size_t buf_len, const vario_runtime_t *rt)
{
    if ((rt != NULL) && (rt->heading_valid != false))
    {
        snprintf(buf, buf_len, "%03ld", (long)lroundf(rt->heading_deg));
    }
    else
    {
        snprintf(buf, buf_len, "---");
    }
}

static void vario_screen1_draw_gs_bar(u8g2_t *u8g2,
                                      const vario_viewport_t *v,
                                      const vario_settings_t *settings,
                                      float gs_kmh)
{
    int16_t x;
    int16_t y;
    int16_t h;
    int16_t center_x;
    int16_t fill_top_y;
    float   clamped_gs;
    float   ratio;
    uint8_t tick;

    if ((u8g2 == NULL) || (v == NULL) || (settings == NULL) || (settings->show_gs_bar == 0u))
    {
        return;
    }

    x = v->x;
    y = v->y;
    h = v->h;
    center_x = (int16_t)(x + (VARIO_SCREEN1_SIDE_BAR_W / 2));

    clamped_gs = gs_kmh;
    if (clamped_gs < 0.0f)
    {
        clamped_gs = 0.0f;
    }
    if (clamped_gs > (float)settings->gs_range_kmh)
    {
        clamped_gs = (float)settings->gs_range_kmh;
    }

    ratio = clamped_gs / (float)settings->gs_range_kmh;
    fill_top_y = (int16_t)(y + h - (ratio * (float)h));

    /* ---------------------------------------------------------------------- */
    /*  GS bar scale                                                           */
    /*  - 바깥 frame 없이, 좌측 벽에 붙은 14px 폭 scale                         */
    /*  - 가로 눈금을 위에서 아래까지 채운다.                                   */
    /* ---------------------------------------------------------------------- */
    for (tick = 0u; tick <= 10u; ++tick)
    {
        int16_t tick_y;
        uint8_t tick_len;

        tick_y = (int16_t)(y + ((h * (int16_t)tick) / 10));
        tick_len = ((tick % 2u) == 0u) ? (uint8_t)(VARIO_SCREEN1_SIDE_BAR_W - 2) : 8u;
        u8g2_DrawHLine(u8g2,
                       (uint8_t)x,
                       (uint8_t)tick_y,
                       tick_len);
    }

    /* ---------------------------------------------------------------------- */
    /*  GS fill column                                                         */
    /*  - 하단에서 위로 차오르는 막대                                           */
    /* ---------------------------------------------------------------------- */
    u8g2_DrawBox(u8g2,
                 (uint8_t)(x + VARIO_SCREEN1_SIDE_BAR_FILL_OFF),
                 (uint8_t)fill_top_y,
                 VARIO_SCREEN1_SIDE_BAR_INNER_W,
                 (uint8_t)((y + h) - fill_top_y));

    /* 작은 GS 라벨 */
    u8g2_SetFont(u8g2, u8g2_font_4x6_tf);
    Vario_Display_DrawTextCentered(u8g2,
                                   center_x,
                                   (int16_t)(y + h - 2),
                                   "GS");
}

static void vario_screen1_draw_vario_bar(u8g2_t *u8g2,
                                         const vario_viewport_t *v,
                                         const vario_settings_t *settings,
                                         float vario_mps)
{
    int16_t x;
    int16_t y;
    int16_t h;
    int16_t center_y;
    int16_t bar_top_y;
    int16_t bar_bottom_y;
    float   range_mps;
    float   clamped_vario;
    float   ratio;
    int32_t tick_mps_x10;

    if ((u8g2 == NULL) || (v == NULL) || (settings == NULL))
    {
        return;
    }

    x = (int16_t)(v->x + v->w - VARIO_SCREEN1_SIDE_BAR_W);
    y = v->y;
    h = v->h;
    center_y = (int16_t)(y + (h / 2));
    range_mps = ((float)settings->vario_range_mps_x10) * 0.1f;

    clamped_vario = vario_mps;
    if (clamped_vario > range_mps)
    {
        clamped_vario = range_mps;
    }
    if (clamped_vario < -range_mps)
    {
        clamped_vario = -range_mps;
    }

    ratio = clamped_vario / range_mps;
    if (ratio >= 0.0f)
    {
        bar_top_y = (int16_t)(center_y - (ratio * ((float)h * 0.5f)));
        bar_bottom_y = center_y;
    }
    else
    {
        bar_top_y = center_y;
        bar_bottom_y = (int16_t)(center_y - (ratio * ((float)h * 0.5f)));
    }

    /* ---------------------------------------------------------------------- */
    /*  VARIO bar scale                                                        */
    /*  - 우측 벽에 딱 붙는 14px bar                                            */
    /*  - frame 없이 full-height tick 만 그림                                  */
    /*  - 0m/s 는 중앙, +는 위, -는 아래                                        */
    /* ---------------------------------------------------------------------- */
    for (tick_mps_x10 = -(int32_t)settings->vario_range_mps_x10;
         tick_mps_x10 <= (int32_t)settings->vario_range_mps_x10;
         tick_mps_x10 += 5)
    {
        int16_t tick_y;
        uint8_t tick_len;
        float   tick_ratio;

        tick_ratio = ((float)tick_mps_x10) / ((float)settings->vario_range_mps_x10);
        tick_y = (int16_t)(center_y - (tick_ratio * ((float)h * 0.5f)));
        tick_len = ((tick_mps_x10 % 10) == 0) ? (uint8_t)(VARIO_SCREEN1_SIDE_BAR_W - 1) : 7u;

        u8g2_DrawHLine(u8g2,
                       (uint8_t)x,
                       (uint8_t)tick_y,
                       tick_len);
    }

    if (bar_bottom_y < bar_top_y)
    {
        int16_t tmp;
        tmp = bar_top_y;
        bar_top_y = bar_bottom_y;
        bar_bottom_y = tmp;
    }

    u8g2_DrawBox(u8g2,
                 (uint8_t)(x + VARIO_SCREEN1_SIDE_BAR_FILL_OFF),
                 (uint8_t)bar_top_y,
                 VARIO_SCREEN1_SIDE_BAR_INNER_W,
                 (uint8_t)(bar_bottom_y - bar_top_y));

    u8g2_SetFont(u8g2, u8g2_font_4x6_tf);
    Vario_Display_DrawTextCentered(u8g2,
                                   (int16_t)(x + (VARIO_SCREEN1_SIDE_BAR_W / 2)),
                                   (int16_t)(y + 6),
                                   "V");
}

static void vario_screen1_compass_label_for_heading(int16_t heading_deg, char *buf, size_t buf_len)
{
    int16_t norm;

    norm = (int16_t)heading_deg;
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

static void vario_screen1_draw_compass_tape(u8g2_t *u8g2,
                                            const vario_viewport_t *v,
                                            const vario_settings_t *settings,
                                            const vario_runtime_t *rt)
{
    int16_t box_h;
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t center_x;
    int16_t center_heading;
    int16_t span_deg;
    int16_t deg;
    char    label[8];

    if ((u8g2 == NULL) || (v == NULL) || (settings == NULL) || (rt == NULL))
    {
        return;
    }

    box_h = (int16_t)settings->compass_box_height_px;
    if (box_h < 14)
    {
        box_h = 14;
    }
    if (box_h > 22)
    {
        box_h = 22;
    }

    x = (int16_t)(v->x + VARIO_SCREEN1_COMPASS_MARGIN_X + VARIO_SCREEN1_SIDE_BAR_W);
    w = (int16_t)(v->w - (2 * VARIO_SCREEN1_COMPASS_MARGIN_X) - (2 * VARIO_SCREEN1_SIDE_BAR_W));
    y = (int16_t)(v->y + v->h - box_h - VARIO_SCREEN1_COMPASS_BOTTOM_GAP);
    center_x = (int16_t)(x + (w / 2));
    center_heading = (rt->heading_valid != false) ? (int16_t)lroundf(rt->heading_deg) : 0;
    span_deg = (int16_t)settings->compass_span_deg;

    /* ---------------------------------------------------------------------- */
    /*  하단 compass tape box                                                  */
    /*  - 사용자가 요구한 "툭 튀어나온" 하단 방위 그래픽 본체                  */
    /*  - box 높이/폭은 settings 와 매크로로 조절 가능                         */
    /* ---------------------------------------------------------------------- */
    u8g2_DrawFrame(u8g2, (uint8_t)x, (uint8_t)y, (uint8_t)w, (uint8_t)box_h);

    for (deg = center_heading - (span_deg / 2);
         deg <= center_heading + (span_deg / 2);
         deg += 5)
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

        tick_h = ((deg % 30) == 0) ? (uint8_t)(box_h - 5) : (((deg % 15) == 0) ? (uint8_t)(box_h - 8) : 4u);
        u8g2_DrawVLine(u8g2,
                       (uint8_t)tick_x,
                       (uint8_t)(y + box_h - tick_h - 1),
                       tick_h);

        if ((deg % 30) == 0)
        {
            vario_screen1_compass_label_for_heading(deg, label, sizeof(label));
            u8g2_SetFont(u8g2, u8g2_font_4x6_tf);
            Vario_Display_DrawTextCentered(u8g2,
                                           tick_x,
                                           (int16_t)(y + 6),
                                           label);
        }
    }

    /* center marker */
    u8g2_DrawVLine(u8g2, (uint8_t)center_x, (uint8_t)(y + 1), (uint8_t)(box_h - 2));
    u8g2_DrawLine(u8g2, (uint8_t)(center_x - 3), (uint8_t)(y + 3), (uint8_t)center_x, (uint8_t)(y + 1));
    u8g2_DrawLine(u8g2, (uint8_t)(center_x + 3), (uint8_t)(y + 3), (uint8_t)center_x, (uint8_t)(y + 1));
}

void Vario_Screen1_Render(u8g2_t *u8g2, const vario_buttonbar_t *buttonbar)
{
    const vario_viewport_t *v;
    const vario_runtime_t  *rt;
    const vario_settings_t *settings;
    int16_t                 content_right_x;
    int16_t                 right_block_x;
    char                    clock_text[20];
    char                    flight_text[24];
    char                    alt1_text[20];
    char                    alt2_text[20];
    char                    alt3_text[20];
    char                    gs_text[20];
    char                    vario_big_text[24];
    char                    vario_small_text[24];
    char                    max_vario_text[24];
    char                    heading_text[12];

    (void)buttonbar;

    v        = Vario_Display_GetFullViewport();
    rt       = Vario_State_GetRuntime();
    settings = Vario_Settings_Get();

    content_right_x = (int16_t)(v->x + v->w - VARIO_SCREEN1_SIDE_BAR_W - VARIO_SCREEN1_CONTENT_R_PAD);
    right_block_x   = (int16_t)(content_right_x - VARIO_SCREEN1_RIGHT_BLOCK_W);

    vario_screen1_format_clock(clock_text, sizeof(clock_text), rt);
    vario_screen1_format_flight_time(flight_text, sizeof(flight_text), rt);
    vario_screen1_format_altitude(alt1_text, sizeof(alt1_text), rt->alt1_absolute_m);
    vario_screen1_format_altitude(alt2_text, sizeof(alt2_text), rt->alt2_relative_m);
    vario_screen1_format_altitude(alt3_text, sizeof(alt3_text), rt->alt3_accum_gain_m);
    vario_screen1_format_speed(gs_text, sizeof(gs_text), rt->ground_speed_kmh);
    vario_screen1_format_signed_vspeed(vario_big_text, sizeof(vario_big_text), rt->baro_vario_mps);
    vario_screen1_format_signed_vspeed(vario_small_text, sizeof(vario_small_text), rt->baro_vario_mps);
    vario_screen1_format_signed_vspeed(max_vario_text, sizeof(max_vario_text), rt->max_top_vario_mps);
    vario_screen1_format_heading(heading_text, sizeof(heading_text), rt);

    /* side bars */
    vario_screen1_draw_gs_bar(u8g2, v, settings, rt->ground_speed_kmh);
    vario_screen1_draw_vario_bar(u8g2, v, settings, rt->baro_vario_mps);

    /* ---------------------------------------------------------------------- */
    /*  상단 중앙 현재 시각                                                     */
    /*  - settings.show_current_time 으로 on/off 가능                           */
    /* ---------------------------------------------------------------------- */
    if (settings->show_current_time != 0u)
    {
        u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
        Vario_Display_DrawTextCentered(u8g2,
                                       (int16_t)(v->x + (v->w / 2)),
                                       (int16_t)(v->y + VARIO_SCREEN1_TOP_TIME_Y),
                                       clock_text);
    }

    /* ---------------------------------------------------------------------- */
    /*  좌상단 VARIO information block                                         */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    u8g2_DrawStr(u8g2,
                 (uint8_t)VARIO_SCREEN1_LEFT_BLOCK_X,
                 (uint8_t)VARIO_SCREEN1_LEFT_BLOCK_Y,
                 "VARIO");

    u8g2_SetFont(u8g2, u8g2_font_10x20_mf);
    u8g2_DrawStr(u8g2,
                 (uint8_t)VARIO_SCREEN1_LEFT_BLOCK_X,
                 (uint8_t)(VARIO_SCREEN1_LEFT_BLOCK_Y + 18),
                 vario_small_text);

    if (settings->show_max_vario != 0u)
    {
        char max_line[30];
        snprintf(max_line, sizeof(max_line), "MAX %s", max_vario_text);
        u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
        u8g2_DrawStr(u8g2,
                     (uint8_t)VARIO_SCREEN1_LEFT_BLOCK_X,
                     (uint8_t)(VARIO_SCREEN1_LEFT_BLOCK_Y + 31),
                     max_line);
    }

    if (settings->show_flight_time != 0u)
    {
        char flt_line[32];
        snprintf(flt_line, sizeof(flt_line), "FLT %s", flight_text);
        u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
        u8g2_DrawStr(u8g2,
                     (uint8_t)VARIO_SCREEN1_LEFT_BLOCK_X,
                     (uint8_t)(VARIO_SCREEN1_LEFT_BLOCK_Y + 41),
                     flt_line);
    }

    /* ---------------------------------------------------------------------- */
    /*  우상단 ALT1 / ALT2 / ALT3 / GS block                                   */
    /*                                                                            */
    /*  ALT1 은 가장 크게, ALT2/ALT3 은 소형 reference/gain 정보로 아래에 둔다. */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    u8g2_DrawStr(u8g2,
                 (uint8_t)right_block_x,
                 (uint8_t)VARIO_SCREEN1_RIGHT_BLOCK_TOP_Y,
                 "ALT1");

    u8g2_SetFont(u8g2, u8g2_font_logisoso20_tf);
    u8g2_DrawStr(u8g2,
                 (uint8_t)right_block_x,
                 (uint8_t)(VARIO_SCREEN1_RIGHT_BLOCK_TOP_Y + 22),
                 alt1_text);

    u8g2_SetFont(u8g2, u8g2_font_4x6_tf);
    u8g2_DrawStr(u8g2,
                 (uint8_t)(right_block_x + 54),
                 (uint8_t)(VARIO_SCREEN1_RIGHT_BLOCK_TOP_Y + 22),
                 Vario_Settings_GetAltitudeUnitText());

    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    {
        char line[28];

        snprintf(line, sizeof(line), "ALT2 %s", alt2_text);
        u8g2_DrawStr(u8g2,
                     (uint8_t)right_block_x,
                     (uint8_t)(VARIO_SCREEN1_RIGHT_BLOCK_TOP_Y + 31),
                     line);

        snprintf(line, sizeof(line), "ALT3 %s", alt3_text);
        u8g2_DrawStr(u8g2,
                     (uint8_t)right_block_x,
                     (uint8_t)(VARIO_SCREEN1_RIGHT_BLOCK_TOP_Y + 40),
                     line);

        snprintf(line, sizeof(line), "GS   %s %s", gs_text, Vario_Settings_GetSpeedUnitText());
        u8g2_DrawStr(u8g2,
                     (uint8_t)right_block_x,
                     (uint8_t)(VARIO_SCREEN1_RIGHT_BLOCK_TOP_Y + 49),
                     line);
    }

    /* ---------------------------------------------------------------------- */
    /*  우측 중앙 큰 vario 값                                                   */
    /*  - 사용자가 요구한 "우측 bar 옆, 세로 중앙에 조금 크게" 구현부          */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFont(u8g2, u8g2_font_10x20_mf);
    Vario_Display_DrawTextRight(u8g2,
                                (int16_t)(v->x + v->w - VARIO_SCREEN1_SIDE_BAR_W - 8),
                                (int16_t)VARIO_SCREEN1_BIG_VARIO_Y,
                                vario_big_text);

    u8g2_SetFont(u8g2, u8g2_font_4x6_tf);
    Vario_Display_DrawTextRight(u8g2,
                                (int16_t)(v->x + v->w - VARIO_SCREEN1_SIDE_BAR_W - 8),
                                (int16_t)(VARIO_SCREEN1_BIG_VARIO_Y + 7),
                                Vario_Settings_GetVSpeedUnitText());

    /* heading numeric just above compass */
    {
        char hdg_line[20];
        snprintf(hdg_line, sizeof(hdg_line), "HDG %s", heading_text);
        u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
        Vario_Display_DrawTextCentered(u8g2,
                                       (int16_t)(v->x + (v->w / 2)),
                                       VARIO_SCREEN1_HEADING_LINE_Y,
                                       hdg_line);
    }

    vario_screen1_draw_compass_tape(u8g2, v, settings, rt);

    Vario_Display_DrawRawOverlay(u8g2, Vario_Display_GetFullViewport());
}
