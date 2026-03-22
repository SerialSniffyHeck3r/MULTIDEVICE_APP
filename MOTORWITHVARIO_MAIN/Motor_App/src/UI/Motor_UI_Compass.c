
#include "Motor_UI_Internal.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>

void Motor_UI_DrawScreen_Compass(u8g2_t *u8g2, const motor_state_t *state)
{
    uint8_t center_x;
    uint8_t center_y;
    int16_t angle_deg;
    float rad;
    int16_t tip_x;
    int16_t tip_y;
    char lat_text[24];
    char lon_text[24];

    if ((u8g2 == 0) || (state == 0))
    {
        return;
    }

    center_x = 52u;
    center_y = 60u;
    angle_deg = state->dyn.heading_deg_x10 / 10;
    rad = (float)angle_deg * 0.0174532925f;
    tip_x = (int16_t)(center_x + sinf(rad) * 20.0f);
    tip_y = (int16_t)(center_y - cosf(rad) * 20.0f);

    Motor_UI_DrawStatusBar(u8g2, state);

    /* ---------------------------------------------------------------------- */
    /*  좌측 원형 나침반                                                        */
    /*  - 원형 프레임과 N/E/S/W 마크                                             */
    /*  - 중앙에서 heading 방향으로 뻗는 화살표                                 */
    /* ---------------------------------------------------------------------- */
    u8g2_DrawCircle(u8g2, center_x, center_y, 24, U8G2_DRAW_ALL);
    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(u8g2, center_x - 2u, center_y - 28u, "N");
    u8g2_DrawStr(u8g2, center_x + 26u, center_y + 2u, "E");
    u8g2_DrawStr(u8g2, center_x - 2u, center_y + 34u, "S");
    u8g2_DrawStr(u8g2, center_x - 32u, center_y + 2u, "W");
    u8g2_DrawLine(u8g2, center_x, center_y, (uint8_t)tip_x, (uint8_t)tip_y);
    u8g2_DrawDisc(u8g2, center_x, center_y, 2, U8G2_DRAW_ALL);

    /* 우측 정보 영역 */
    u8g2_SetFont(u8g2, u8g2_font_6x10_tr);
    (void)snprintf(lat_text, sizeof(lat_text), "LAT %+ld.%07ld",
                   (long)(state->nav.lat_e7 / 10000000),
                   (long)abs(state->nav.lat_e7 % 10000000));
    (void)snprintf(lon_text, sizeof(lon_text), "LON %+ld.%07ld",
                   (long)(state->nav.lon_e7 / 10000000),
                   (long)abs(state->nav.lon_e7 % 10000000));
    u8g2_DrawStr(u8g2, 100, 26, lat_text);
    u8g2_DrawStr(u8g2, 100, 40, lon_text);

    {
        char buf[24];
        (void)snprintf(buf, sizeof(buf), "HEAD %d deg", (int)angle_deg);
        u8g2_DrawStr(u8g2, 100, 56, buf);
        (void)snprintf(buf, sizeof(buf), "FIX %u  SAT %u", (unsigned)state->nav.fix_type, (unsigned)state->nav.sats_used);
        u8g2_DrawStr(u8g2, 100, 70, buf);
        (void)snprintf(buf, sizeof(buf), "HACC %lum", (unsigned long)(state->nav.hacc_mm / 1000u));
        u8g2_DrawStr(u8g2, 100, 84, buf);
        (void)snprintf(buf, sizeof(buf), "VACC %lum", (unsigned long)(state->nav.vacc_mm / 1000u));
        u8g2_DrawStr(u8g2, 100, 98, buf);
    }

    Motor_UI_DrawBottomHint(u8g2, "1<", "2>", "6 MENU");
}
