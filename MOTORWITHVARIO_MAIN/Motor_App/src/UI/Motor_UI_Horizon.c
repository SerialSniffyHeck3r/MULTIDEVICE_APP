#include "Motor_UI_Internal.h"

#include "Motor_Units.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

void Motor_UI_DrawScreen_Horizon(u8g2_t *u8g2, const ui_rect_t *viewport, const motor_state_t *state)
{
    int16_t cx;
    int16_t cy;
    int16_t radius;
    int16_t bank_deg;
    int16_t pitch_deg;
    int16_t horizon_offset_y;
    float rad;
    int16_t dx;
    int16_t dy;
    int16_t x1;
    int16_t y1;
    int16_t x2;
    int16_t y2;
    char buf[32];
    char alt_text[20];

    if ((u8g2 == 0) || (viewport == 0) || (state == 0))
    {
        return;
    }

    Motor_UI_DrawViewportTitle(u8g2, viewport, "ARTIFICIAL HORIZON");

    /* ---------------------------------------------------------------------- */
    /*  좌측 attitude 인디케이터                                               */
    /*  - 오프로드 차량의 simple inclinometer 느낌으로 원형 horizon을 그린다.  */
    /* ---------------------------------------------------------------------- */
    radius = (int16_t)((viewport->h - 26) / 2);
    if (radius > 34) radius = 34;
    if (radius < 20) radius = 20;

    cx = (int16_t)(viewport->x + 10 + radius);
    cy = (int16_t)(viewport->y + 18 + radius);

    bank_deg = (int16_t)(state->dyn.bank_deg_x10 / 10);
    pitch_deg = (int16_t)(state->dyn.grade_deg_x10 / 10);
    horizon_offset_y = (int16_t)(pitch_deg * 2);
    rad = (float)bank_deg * (float)(M_PI / 180.0);
    dx = (int16_t)((float)(radius - 4) * cosf(rad));
    dy = (int16_t)((float)(radius - 4) * sinf(rad));

    x1 = (int16_t)(cx - dx);
    y1 = (int16_t)(cy + horizon_offset_y - dy);
    x2 = (int16_t)(cx + dx);
    y2 = (int16_t)(cy + horizon_offset_y + dy);

    u8g2_DrawCircle(u8g2, cx, cy, radius, U8G2_DRAW_ALL);
    u8g2_DrawCircle(u8g2, cx, cy, (uint8_t)(radius - 10), U8G2_DRAW_ALL);
    u8g2_DrawLine(u8g2, x1, y1, x2, y2);
    u8g2_DrawLine(u8g2, cx - 12, cy, cx - 3, cy);
    u8g2_DrawLine(u8g2, cx + 3, cy, cx + 12, cy);
    u8g2_DrawLine(u8g2, cx, cy - 5, cx, cy + 5);

    /* ---------------------------------------------------------------------- */
    /*  우측 정보 패널                                                         */
    /*  - BANK / PITCH / ALT / VACC / GRADE를 텍스트로 요약 표시               */
    /* ---------------------------------------------------------------------- */
    Motor_Units_FormatAltitude(alt_text, sizeof(alt_text), state->nav.altitude_cm, &state->settings.units);
    u8g2_DrawFrame(u8g2, viewport->x + 96, viewport->y + 16, viewport->w - 102, viewport->h - 20);
    u8g2_SetFont(u8g2, u8g2_font_6x10_tr);

    (void)snprintf(buf, sizeof(buf), "BANK %+d deg", (int)bank_deg);
    u8g2_DrawStr(u8g2, viewport->x + 102, viewport->y + 28, buf);

    (void)snprintf(buf, sizeof(buf), "PITCH %+d deg", (int)pitch_deg);
    u8g2_DrawStr(u8g2, viewport->x + 102, viewport->y + 44, buf);

    (void)snprintf(buf, sizeof(buf), "ALT %s %s", alt_text, Motor_Units_GetAltitudeSuffix(&state->settings.units));
    u8g2_DrawStr(u8g2, viewport->x + 102, viewport->y + 60, buf);

    (void)snprintf(buf, sizeof(buf), "GRADE %+ld.%01ld%%",
                   (long)(state->snapshot.altitude.grade_noimu_x10 / 10),
                   (long)labs((long)(state->snapshot.altitude.grade_noimu_x10 % 10)));
    u8g2_DrawStr(u8g2, viewport->x + 102, viewport->y + 76, buf);

    (void)snprintf(buf, sizeof(buf), "VACC %lum", (unsigned long)(state->nav.vacc_mm / 1000u));
    u8g2_DrawStr(u8g2, viewport->x + 102, viewport->y + 92, buf);

    (void)snprintf(buf, sizeof(buf), "TRUST %u%%", (unsigned)(state->dyn.confidence_permille / 10u));
    u8g2_DrawStr(u8g2, viewport->x + 102, viewport->y + 108, buf);
}
