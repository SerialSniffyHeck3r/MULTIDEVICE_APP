
#include "Motor_UI_Internal.h"

#include <stdio.h>
#include <math.h>
#include <stdlib.h>

void Motor_UI_DrawScreen_Horizon(u8g2_t *u8g2, const motor_state_t *state)
{
    int16_t center_x;
    int16_t center_y;
    int16_t horizon_offset_y;
    int16_t bank_deg;
    int16_t pitch_deg;
    int16_t line_x1;
    int16_t line_y1;
    int16_t line_x2;
    int16_t line_y2;
    float rad;
    char alt_text[24];

    if ((u8g2 == 0) || (state == 0))
    {
        return;
    }

    center_x = 76;
    center_y = 62;
    bank_deg = (int16_t)(state->dyn.bank_deg_x10 / 10);
    pitch_deg = (int16_t)(state->dyn.grade_deg_x10 / 10);
    horizon_offset_y = (int16_t)(pitch_deg * 2);
    rad = (float)bank_deg * 0.0174532925f;

    line_x1 = (int16_t)(center_x - 34 * cosf(rad));
    line_y1 = (int16_t)(center_y + horizon_offset_y - 34 * sinf(rad));
    line_x2 = (int16_t)(center_x + 34 * cosf(rad));
    line_y2 = (int16_t)(center_y + horizon_offset_y + 34 * sinf(rad));

    Motor_UI_DrawStatusBar(u8g2, state);

    /* ---------------------------------------------------------------------- */
    /*  좌측 artificial horizon                                                 */
    /*  - Pajero/off-road info 화면 느낌의 간단한 attitude 인디케이터           */
    /*  - 원형 테두리 + 중앙 고정 기체표식 + 회전된 horizon line                */
    /* ---------------------------------------------------------------------- */
    u8g2_DrawCircle(u8g2, (uint8_t)center_x, (uint8_t)center_y, 36, U8G2_DRAW_ALL);
    u8g2_DrawLine(u8g2, (uint8_t)line_x1, (uint8_t)line_y1, (uint8_t)line_x2, (uint8_t)line_y2);
    u8g2_DrawLine(u8g2, center_x - 10, center_y, center_x - 2, center_y);
    u8g2_DrawLine(u8g2, center_x + 2, center_y, center_x + 10, center_y);
    u8g2_DrawLine(u8g2, center_x, center_y - 4, center_x, center_y + 4);

    /* 우측 정보 패널 */
    u8g2_SetFont(u8g2, u8g2_font_6x10_tr);
    (void)snprintf(alt_text, sizeof(alt_text), "ALT %ld m", (long)(state->nav.altitude_cm / 100));
    u8g2_DrawStr(u8g2, 130, 28, alt_text);
    {
        char buf[32];
        (void)snprintf(buf, sizeof(buf), "BANK %+d deg", (int)bank_deg);
        u8g2_DrawStr(u8g2, 130, 46, buf);
        (void)snprintf(buf, sizeof(buf), "PITCH %+d deg", (int)pitch_deg);
        u8g2_DrawStr(u8g2, 130, 62, buf);
        (void)snprintf(buf, sizeof(buf), "GRADE %+ld.%01ld%%",
                       (long)(state->snapshot.altitude.grade_noimu_x10 / 10),
                       (long)abs((int)(state->snapshot.altitude.grade_noimu_x10 % 10)));
        u8g2_DrawStr(u8g2, 130, 78, buf);
        (void)snprintf(buf, sizeof(buf), "VACC %lum", (unsigned long)(state->nav.vacc_mm / 1000u));
        u8g2_DrawStr(u8g2, 130, 94, buf);
    }

    Motor_UI_DrawBottomHint(u8g2, "AHRS", "OFFROAD", "6 MENU");
}
