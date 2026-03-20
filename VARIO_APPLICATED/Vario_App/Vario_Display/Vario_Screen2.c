#include "Vario_Screen2.h"

#include "Vario_Display_Common.h"
#include "Vario_State.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

static void vario_screen2_format_signed_lat(char *buf, size_t buf_len, int32_t lat)
{
    char    hemi;
    int32_t value;
    int32_t deg;
    int32_t frac;

    hemi = 'N';
    value = lat;
    if (value < 0)
    {
        hemi = 'S';
        value = -value;
    }

    deg  = value / 10000000L;
    frac = value % 10000000L;

    snprintf(buf, buf_len, "%c%ld.%07ld", hemi, (long)deg, (long)frac);
}

static void vario_screen2_format_signed_lon(char *buf, size_t buf_len, int32_t lon)
{
    char    hemi;
    int32_t value;
    int32_t deg;
    int32_t frac;

    hemi = 'E';
    value = lon;
    if (value < 0)
    {
        hemi = 'W';
        value = -value;
    }

    deg  = value / 10000000L;
    frac = value % 10000000L;

    snprintf(buf, buf_len, "%c%ld.%07ld", hemi, (long)deg, (long)frac);
}

static void vario_screen2_format_gs(char *buf, size_t buf_len, float gs_kmh)
{
    int32_t tenths;

    tenths = (int32_t)lroundf(gs_kmh * 10.0f);
    if (tenths < 0)
    {
        tenths = 0;
    }

    snprintf(buf,
             buf_len,
             "%ld.%1ld",
             (long)(tenths / 10),
             (long)(tenths % 10));
}

void Vario_Screen2_Render(u8g2_t *u8g2, const vario_buttonbar_t *buttonbar)
{
    const vario_runtime_t *rt;
    char                   gps_alt_text[24];
    char                   gs_text[24];
    char                   sat_text[24];
    char                   fix_text[24];
    char                   lat_text[32];
    char                   lon_text[32];
    char                   acc_text[32];

    (void)buttonbar;

    rt = Vario_State_GetRuntime();

    snprintf(gps_alt_text, sizeof(gps_alt_text), "%ld", (long)lroundf(rt->gps_altitude_m));
    vario_screen2_format_gs(gs_text, sizeof(gs_text), rt->ground_speed_kmh);

    snprintf(sat_text,
             sizeof(sat_text),
             "%u/%u SV",
             (unsigned)rt->gps.fix.numSV_used,
             (unsigned)rt->gps.fix.numSV_visible);

    snprintf(fix_text,
             sizeof(fix_text),
             "FIX %u  OK %u",
             (unsigned)rt->gps.fix.fixType,
             (unsigned)(rt->gps.fix.fixOk ? 1u : 0u));

    vario_screen2_format_signed_lat(lat_text, sizeof(lat_text), rt->gps.fix.lat);
    vario_screen2_format_signed_lon(lon_text, sizeof(lon_text), rt->gps.fix.lon);

    snprintf(acc_text,
             sizeof(acc_text),
             "HACC %ldm  VACC %ldm",
             (long)(rt->gps.fix.hAcc / 1000),
             (long)(rt->gps.fix.vAcc / 1000));

    /* ---------------------------------------------------------------------- */
    /*  Screen2 역시 full-screen 240x128                                       */
    /*  - GPS / NAV 전용 레이아웃                                               */
    /* ---------------------------------------------------------------------- */

    /* 제목 */
    u8g2_SetFont(u8g2, u8g2_font_6x12_mf);
    u8g2_DrawStr(u8g2, 4u, 12u, "GPS NAV");

    /* 현재 날짜/시간: 상단 우측 */
    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    if (rt->gps.fix.valid)
    {
        char date_text[24];
        snprintf(date_text,
                 sizeof(date_text),
                 "%04u-%02u-%02u %02u:%02u:%02u",
                 (unsigned)rt->gps.fix.year,
                 (unsigned)rt->gps.fix.month,
                 (unsigned)rt->gps.fix.day,
                 (unsigned)rt->gps.fix.hour,
                 (unsigned)rt->gps.fix.min,
                 (unsigned)rt->gps.fix.sec);
        Vario_Display_DrawTextRight(u8g2, 236, 10, date_text);
    }

    /* GPS ALT 라벨 */
    u8g2_SetFont(u8g2, u8g2_font_6x10_mf);
    Vario_Display_DrawTextCentered(u8g2, 120, 30, "GPS ALT");

    /* GPS ALT 대형 값: 화면 중앙 대형 */
    u8g2_SetFont(u8g2, u8g2_font_logisoso20_tf);
    Vario_Display_DrawTextCentered(u8g2, 96, 62, gps_alt_text);
    u8g2_SetFont(u8g2, u8g2_font_6x10_mf);
    u8g2_DrawStr(u8g2, 146u, 62u, "m");

    /* GS 라벨과 값: 우측 중단 */
    u8g2_SetFont(u8g2, u8g2_font_6x10_mf);
    u8g2_DrawStr(u8g2, 168u, 34u, "GS");
    u8g2_SetFont(u8g2, u8g2_font_10x20_mf);
    Vario_Display_DrawTextRight(u8g2, 234, 56, gs_text);
    u8g2_SetFont(u8g2, u8g2_font_4x6_tf);
    u8g2_DrawStr(u8g2, 208u, 62u, "km/h");

    /* sat / fix 카드: 좌하단 */
    u8g2_DrawFrame(u8g2, 6u, 70u, 108u, 46u);
    u8g2_SetFont(u8g2, u8g2_font_6x10_mf);
    u8g2_DrawStr(u8g2, 12u, 82u, sat_text);
    u8g2_DrawStr(u8g2, 12u, 95u, fix_text);
    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    u8g2_DrawStr(u8g2, 12u, 108u, acc_text);

    /* 좌표 카드: 우하단 */
    u8g2_DrawFrame(u8g2, 122u, 70u, 112u, 46u);
    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    u8g2_DrawStr(u8g2, 126u, 82u, "LAT");
    u8g2_DrawStr(u8g2, 152u, 82u, lat_text);
    u8g2_DrawStr(u8g2, 126u, 96u, "LON");
    u8g2_DrawStr(u8g2, 152u, 96u, lon_text);

    /* 개발용 raw overlay: full-screen 하단 좌측 */
    Vario_Display_DrawRawOverlay(u8g2, Vario_Display_GetFullViewport());
}
