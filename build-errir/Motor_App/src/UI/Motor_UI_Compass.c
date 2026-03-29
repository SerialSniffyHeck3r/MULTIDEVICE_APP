#include "Motor_UI_Internal.h"

#include "Motor_Units.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

static int16_t motor_ui_compass_clamp_i16(int16_t v, int16_t min_v, int16_t max_v)
{
    if (v < min_v)
    {
        return min_v;
    }
    if (v > max_v)
    {
        return max_v;
    }
    return v;
}

void Motor_UI_DrawScreen_Compass(u8g2_t *u8g2, const ui_rect_t *viewport, const motor_state_t *state)
{
    int16_t center_x;
    int16_t center_y;
    int16_t radius;
    int16_t angle_deg;
    float rad;
    int16_t tip_x;
    int16_t tip_y;
    char buf[32];

    if ((u8g2 == 0) || (viewport == 0) || (state == 0))
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  화면 상단 제목                                                         */
    /*  - viewport 안에서만 제목과 구분선을 그린다.                            */
    /* ---------------------------------------------------------------------- */
    Motor_UI_DrawViewportTitle(u8g2, viewport, "COMPASS / COORD");

    /* ---------------------------------------------------------------------- */
    /*  좌측 원형 나침반                                                       */
    /*  - viewport 좌측 90px 폭 정도를 사용한다.                               */
    /*  - 원형 테두리와 N/E/S/W 문자를 그리고, 현재 heading 방향 화살표를 표시 */
    /* ---------------------------------------------------------------------- */
    radius = (int16_t)((viewport->h - 26) / 2);
    if (radius > 34)
    {
        radius = 34;
    }
    if (radius < 20)
    {
        radius = 20;
    }

    center_x = (int16_t)(viewport->x + 8 + radius);
    center_y = (int16_t)(viewport->y + 18 + radius);

    u8g2_DrawCircle(u8g2, center_x, center_y, radius, U8G2_DRAW_ALL);
    u8g2_DrawCircle(u8g2, center_x, center_y, (uint8_t)(radius - 8), U8G2_DRAW_ALL);

    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(u8g2, center_x - 2, center_y - radius - 2, "N");
    u8g2_DrawStr(u8g2, center_x - 2, center_y + radius + 7, "S");
    u8g2_DrawStr(u8g2, center_x + radius + 4, center_y + 2, "E");
    u8g2_DrawStr(u8g2, center_x - radius - 8, center_y + 2, "W");

    angle_deg = (int16_t)(state->dyn.heading_deg_x10 / 10);
    rad = (float)angle_deg * (float)(M_PI / 180.0);
    tip_x = (int16_t)(center_x + (int16_t)((float)(radius - 6) * sinf(rad)));
    tip_y = (int16_t)(center_y - (int16_t)((float)(radius - 6) * cosf(rad)));
    tip_x = motor_ui_compass_clamp_i16(tip_x, (int16_t)(viewport->x + 2), (int16_t)(viewport->x + viewport->w - 2));
    tip_y = motor_ui_compass_clamp_i16(tip_y, (int16_t)(viewport->y + 14), (int16_t)(viewport->y + viewport->h - 2));

    u8g2_DrawLine(u8g2, center_x, center_y, tip_x, tip_y);
    u8g2_DrawDisc(u8g2, center_x, center_y, 2, U8G2_DRAW_ALL);

    /* 기체 기준 고정 마크 */
    u8g2_DrawLine(u8g2, center_x - 8, center_y, center_x - 2, center_y);
    u8g2_DrawLine(u8g2, center_x + 2, center_y, center_x + 8, center_y);
    u8g2_DrawLine(u8g2, center_x, center_y - 5, center_x, center_y + 5);

    u8g2_SetFont(u8g2, u8g2_font_6x10_tr);
    (void)snprintf(buf, sizeof(buf), "%03d DEG", (int)((angle_deg + 360) % 360));
    u8g2_DrawStr(u8g2, viewport->x + 12, viewport->y + viewport->h - 8, buf);

    /* ---------------------------------------------------------------------- */
    /*  우측 텍스트 패널                                                       */
    /*  - 위도/경도, 위성 수, 정확도, breadcrumb count 를 계기처럼 배치한다.    */
    /* ---------------------------------------------------------------------- */
    u8g2_DrawFrame(u8g2, viewport->x + 96, viewport->y + 16, viewport->w - 102, viewport->h - 20);
    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(u8g2, viewport->x + 100, viewport->y + 24, "LAT");
    u8g2_DrawStr(u8g2, viewport->x + 100, viewport->y + 40, "LON");
    u8g2_DrawStr(u8g2, viewport->x + 100, viewport->y + 56, "FIX");
    u8g2_DrawStr(u8g2, viewport->x + 100, viewport->y + 72, "HACC");
    u8g2_DrawStr(u8g2, viewport->x + 100, viewport->y + 88, "TRACK");

    u8g2_SetFont(u8g2, u8g2_font_6x10_tr);
    (void)snprintf(buf, sizeof(buf), "%+ld.%07ld",
                   (long)(state->nav.lat_e7 / 10000000L),
                   (long)labs((long)(state->nav.lat_e7 % 10000000L)));
    u8g2_DrawStr(u8g2, viewport->x + 128, viewport->y + 24, buf);

    (void)snprintf(buf, sizeof(buf), "%+ld.%07ld",
                   (long)(state->nav.lon_e7 / 10000000L),
                   (long)labs((long)(state->nav.lon_e7 % 10000000L)));
    u8g2_DrawStr(u8g2, viewport->x + 128, viewport->y + 40, buf);

    (void)snprintf(buf, sizeof(buf), "%uD / %u SAT",
                   (unsigned)state->nav.fix_type,
                   (unsigned)state->nav.sats_used);
    u8g2_DrawStr(u8g2, viewport->x + 128, viewport->y + 56, buf);

    (void)snprintf(buf, sizeof(buf), "%lum / %lum",
                   (unsigned long)(state->nav.hacc_mm / 1000u),
                   (unsigned long)(state->nav.vacc_mm / 1000u));
    u8g2_DrawStr(u8g2, viewport->x + 128, viewport->y + 72, buf);

    (void)snprintf(buf, sizeof(buf), "%u PTS", (unsigned)state->breadcrumb.count);
    u8g2_DrawStr(u8g2, viewport->x + 128, viewport->y + 88, buf);

    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(u8g2,
                 viewport->x + 100,
                 viewport->y + viewport->h - 8,
                 (state->nav.heading_valid != false) ? "Course valid" : "Course invalid");
}
