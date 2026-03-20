#include "Vario_Screen2.h"

#include "Vario_Display_Common.h"
#include "../Vario_Settings/Vario_Settings.h"
#include "../Vario_State/Vario_State.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>

#define VARIO_SCREEN2_MAP_MARGIN_L  18
#define VARIO_SCREEN2_MAP_MARGIN_R  18
#define VARIO_SCREEN2_MAP_MARGIN_T  22
#define VARIO_SCREEN2_MAP_MARGIN_B  22
#define VARIO_SCREEN2_TEXT_TOP_Y    18
#define VARIO_SCREEN2_TEXT_BOTTOM_Y 126

#ifndef VARIO_SCREEN2_PI
#define VARIO_SCREEN2_PI 3.14159265358979323846f
#endif

static float vario_screen2_deg_to_rad(float deg)
{
    return deg * (VARIO_SCREEN2_PI / 180.0f);
}

static void vario_screen2_ll_to_local_m(int32_t ref_lat_e7,
                                        int32_t ref_lon_e7,
                                        int32_t lat_e7,
                                        int32_t lon_e7,
                                        float *out_dx_m,
                                        float *out_dy_m)
{
    float ref_lat_deg;
    float ref_lon_deg;
    float lat_deg;
    float lon_deg;
    float mean_lat_rad;

    ref_lat_deg = ((float)ref_lat_e7) * 1.0e-7f;
    ref_lon_deg = ((float)ref_lon_e7) * 1.0e-7f;
    lat_deg     = ((float)lat_e7) * 1.0e-7f;
    lon_deg     = ((float)lon_e7) * 1.0e-7f;
    mean_lat_rad = vario_screen2_deg_to_rad((ref_lat_deg + lat_deg) * 0.5f);

    if (out_dx_m != NULL)
    {
        *out_dx_m = (lon_deg - ref_lon_deg) * (111319.5f * cosf(mean_lat_rad));
    }

    if (out_dy_m != NULL)
    {
        *out_dy_m = (lat_deg - ref_lat_deg) * 111132.0f;
    }
}

static void vario_screen2_format_altitude(char *buf, size_t buf_len, float altitude_m)
{
    snprintf(buf,
             buf_len,
             "%ld",
             (long)Vario_Settings_AltitudeMetersToDisplayRounded(altitude_m));
}

static void vario_screen2_format_speed(char *buf, size_t buf_len, float speed_kmh)
{
    snprintf(buf,
             buf_len,
             "%ld",
             (long)Vario_Settings_SpeedToDisplayRounded(speed_kmh));
}

static void vario_screen2_format_vspeed(char *buf, size_t buf_len, float vspd_mps)
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

static void vario_screen2_format_flight_time(char *buf, size_t buf_len, uint32_t t_s)
{
    uint32_t hh;
    uint32_t mm;
    uint32_t ss;

    hh = t_s / 3600u;
    mm = (t_s % 3600u) / 60u;
    ss = t_s % 60u;

    snprintf(buf, buf_len, "%02lu:%02lu:%02lu", (unsigned long)hh, (unsigned long)mm, (unsigned long)ss);
}

static void vario_screen2_draw_heading_arrow(u8g2_t *u8g2,
                                             int16_t center_x,
                                             int16_t center_y,
                                             float heading_deg,
                                             uint8_t size_px)
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

    rad = vario_screen2_deg_to_rad(heading_deg);

    tip_x = (int16_t)lroundf((float)center_x + (sinf(rad) * (float)size_px));
    tip_y = (int16_t)lroundf((float)center_y - (cosf(rad) * (float)size_px));
    tail_x = (int16_t)lroundf((float)center_x - (sinf(rad) * ((float)size_px * 0.35f)));
    tail_y = (int16_t)lroundf((float)center_y + (cosf(rad) * ((float)size_px * 0.35f)));

    left_x = (int16_t)lroundf((float)center_x + (sinf(rad + 2.55f) * ((float)size_px * 0.55f)));
    left_y = (int16_t)lroundf((float)center_y - (cosf(rad + 2.55f) * ((float)size_px * 0.55f)));
    right_x = (int16_t)lroundf((float)center_x + (sinf(rad - 2.55f) * ((float)size_px * 0.55f)));
    right_y = (int16_t)lroundf((float)center_y - (cosf(rad - 2.55f) * ((float)size_px * 0.55f)));

    u8g2_DrawLine(u8g2, (uint8_t)tail_x, (uint8_t)tail_y, (uint8_t)tip_x, (uint8_t)tip_y);
    u8g2_DrawLine(u8g2, (uint8_t)left_x, (uint8_t)left_y, (uint8_t)tip_x, (uint8_t)tip_y);
    u8g2_DrawLine(u8g2, (uint8_t)right_x, (uint8_t)right_y, (uint8_t)tip_x, (uint8_t)tip_y);
}

static void vario_screen2_draw_trail(u8g2_t *u8g2,
                                     const vario_viewport_t *map_v,
                                     const vario_settings_t *settings,
                                     const vario_runtime_t *rt)
{
    uint8_t start_index;
    uint8_t i;
    int16_t center_x;
    int16_t center_y;
    float   usable_radius_px;
    float   meters_per_px;

    if ((u8g2 == NULL) || (map_v == NULL) || (settings == NULL) || (rt == NULL))
    {
        return;
    }

    center_x = (int16_t)(map_v->x + (map_v->w / 2));
    center_y = (int16_t)(map_v->y + (map_v->h / 2));
    usable_radius_px = (float)((map_v->w < map_v->h) ? (map_v->w / 2) : (map_v->h / 2));
    meters_per_px = ((float)settings->trail_range_m) / usable_radius_px;

    /* map boundary and north marker */
    u8g2_DrawFrame(u8g2,
                   (uint8_t)map_v->x,
                   (uint8_t)map_v->y,
                   (uint8_t)map_v->w,
                   (uint8_t)map_v->h);
    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    Vario_Display_DrawTextCentered(u8g2,
                                   center_x,
                                   (int16_t)(map_v->y - 2),
                                   "N");
    u8g2_DrawLine(u8g2,
                  (uint8_t)center_x,
                  (uint8_t)(map_v->y + 2),
                  (uint8_t)center_x,
                  (uint8_t)(map_v->y + 8));

    if ((rt->gps_valid == false) || (rt->trail_count == 0u))
    {
        u8g2_SetFont(u8g2, u8g2_font_6x12_mf);
        Vario_Display_DrawTextCentered(u8g2,
                                       center_x,
                                       center_y,
                                       "NO GPS / TRAIL");
        return;
    }

    start_index = (uint8_t)((rt->trail_head + VARIO_TRAIL_MAX_POINTS - rt->trail_count) % VARIO_TRAIL_MAX_POINTS);

    for (i = 0u; i < rt->trail_count; ++i)
    {
        uint8_t idx;
        float   dx_m;
        float   dy_m;
        int16_t px;
        int16_t py;
        uint8_t dot_size;

        idx = (uint8_t)((start_index + i) % VARIO_TRAIL_MAX_POINTS);
        vario_screen2_ll_to_local_m(rt->gps.fix.lat,
                                    rt->gps.fix.lon,
                                    rt->trail_lat_e7[idx],
                                    rt->trail_lon_e7[idx],
                                    &dx_m,
                                    &dy_m);

        px = (int16_t)lroundf((float)center_x + (dx_m / meters_per_px));
        py = (int16_t)lroundf((float)center_y - (dy_m / meters_per_px));

        if ((px < map_v->x) || (px >= (map_v->x + map_v->w)) ||
            (py < map_v->y) || (py >= (map_v->y + map_v->h)))
        {
            continue;
        }

        dot_size = settings->trail_dot_size_px;
        if (dot_size < 1u)
        {
            dot_size = 1u;
        }
        if (dot_size > 3u)
        {
            dot_size = 3u;
        }

        u8g2_DrawBox(u8g2,
                     (uint8_t)(px - (int16_t)(dot_size / 2u)),
                     (uint8_t)(py - (int16_t)(dot_size / 2u)),
                     dot_size,
                     dot_size);
    }

    /* current position center marker */
    u8g2_DrawCircle(u8g2, (uint8_t)center_x, (uint8_t)center_y, 3u, U8G2_DRAW_ALL);
    u8g2_DrawHLine(u8g2, (uint8_t)(center_x - 5), (uint8_t)center_y, 11u);
    u8g2_DrawVLine(u8g2, (uint8_t)center_x, (uint8_t)(center_y - 5), 11u);

    if (rt->heading_valid != false)
    {
        vario_screen2_draw_heading_arrow(u8g2,
                                         center_x,
                                         center_y,
                                         rt->heading_deg,
                                         settings->arrow_size_px);
    }
}

void Vario_Screen2_Render(u8g2_t *u8g2, const vario_buttonbar_t *buttonbar)
{
    const vario_viewport_t *v;
    const vario_runtime_t  *rt;
    const vario_settings_t *settings;
    vario_viewport_t        map_v;
    char                    alt_text[20];
    char                    vario_text[20];
    char                    gs_text[20];
    char                    flt_text[24];

    (void)buttonbar;

    v        = Vario_Display_GetFullViewport();
    rt       = Vario_State_GetRuntime();
    settings = Vario_Settings_Get();

    map_v.x = (int16_t)(v->x + VARIO_SCREEN2_MAP_MARGIN_L);
    map_v.y = (int16_t)(v->y + VARIO_SCREEN2_MAP_MARGIN_T);
    map_v.w = (int16_t)(v->w - VARIO_SCREEN2_MAP_MARGIN_L - VARIO_SCREEN2_MAP_MARGIN_R);
    map_v.h = (int16_t)(v->h - VARIO_SCREEN2_MAP_MARGIN_T - VARIO_SCREEN2_MAP_MARGIN_B);

    vario_screen2_format_altitude(alt_text, sizeof(alt_text), rt->alt1_absolute_m);
    vario_screen2_format_vspeed(vario_text, sizeof(vario_text), rt->baro_vario_mps);
    vario_screen2_format_speed(gs_text, sizeof(gs_text), rt->ground_speed_kmh);
    vario_screen2_format_flight_time(flt_text, sizeof(flt_text), rt->flight_time_s);

    /* ---------------------------------------------------------------------- */
    /*  corner telemetry                                                       */
    /*  - 좌상단 : current vario                                               */
    /*  - 우상단 : current altitude                                            */
    /*  - 좌하단 : flight time                                                 */
    /*  - 우하단 : GS                                                          */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFont(u8g2, u8g2_font_4x6_tf);
    u8g2_DrawStr(u8g2, 2u, 6u, "VARIO");
    Vario_Display_DrawTextRight(u8g2, (int16_t)(v->x + v->w - 2), 6, "ALT");
    u8g2_DrawStr(u8g2, 2u, 111u, "FLT");
    Vario_Display_DrawTextRight(u8g2, (int16_t)(v->x + v->w - 2), 111, "GS");

    u8g2_SetFont(u8g2, u8g2_font_9x15_mf);
    u8g2_DrawStr(u8g2, 2u, (uint8_t)VARIO_SCREEN2_TEXT_TOP_Y, vario_text);
    Vario_Display_DrawTextRight(u8g2,
                                (int16_t)(v->x + v->w - 2),
                                VARIO_SCREEN2_TEXT_TOP_Y,
                                alt_text);
    u8g2_DrawStr(u8g2, 2u, (uint8_t)VARIO_SCREEN2_TEXT_BOTTOM_Y, flt_text);
    Vario_Display_DrawTextRight(u8g2,
                                (int16_t)(v->x + v->w - 2),
                                VARIO_SCREEN2_TEXT_BOTTOM_Y,
                                gs_text);

    u8g2_SetFont(u8g2, u8g2_font_4x6_tf);
    u8g2_DrawStr(u8g2, 44u, 6u, Vario_Settings_GetVSpeedUnitText());
    Vario_Display_DrawTextRight(u8g2,
                                (int16_t)(v->x + v->w - 2),
                                6,
                                Vario_Settings_GetAltitudeUnitText());
    Vario_Display_DrawTextRight(u8g2,
                                (int16_t)(v->x + v->w - 2),
                                111,
                                Vario_Settings_GetSpeedUnitText());

    vario_screen2_draw_trail(u8g2, &map_v, settings, rt);

    Vario_Display_DrawRawOverlay(u8g2, Vario_Display_GetFullViewport());
}
