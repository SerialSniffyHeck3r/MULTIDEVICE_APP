
#include "Motor_UI_Internal.h"

#include "Motor_Units.h"

#include <stdio.h>
#include <stdlib.h>

void Motor_UI_DrawScreen_Altitude(u8g2_t *u8g2, const motor_state_t *state)
{
    uint16_t i;
    uint16_t head;
    int16_t min_alt;
    int16_t max_alt;
    char alt_text[16];
    char rel_text[16];

    if ((u8g2 == 0) || (state == 0))
    {
        return;
    }

    Motor_UI_DrawStatusBar(u8g2, state);

    Motor_Units_FormatAltitude(alt_text, sizeof(alt_text), state->nav.altitude_cm, &state->settings.units);
    Motor_Units_FormatAltitude(rel_text, sizeof(rel_text), state->nav.rel_altitude_cm, &state->settings.units);

    Motor_UI_DrawLabeledValueBox(u8g2, 4, 14, 74, 30, "ALT", alt_text, 0u);
    Motor_UI_DrawLabeledValueBox(u8g2, 82, 14, 74, 30, "REL", rel_text, 0u);
    {
        char grade_text[16];
        (void)snprintf(grade_text, sizeof(grade_text), "%+ld.%01ld%%",
                       (long)(state->snapshot.altitude.grade_noimu_x10 / 10),
                       (long)abs((int)(state->snapshot.altitude.grade_noimu_x10 % 10)));
        Motor_UI_DrawLabeledValueBox(u8g2, 160, 14, 76, 30, "GRADE", grade_text, 0u);
    }

    /* ---------------------------------------------------------------------- */
    /*  고도 / 경사도 그래프                                                    */
    /*  - 중하단 6,50 ~ 234,112 영역을 그래프 캔버스로 사용                     */
    /*  - alt_history를 bounding scale로 그리고                                */
    /*  - grade_history는 점선 대용 pixel strip로 겹쳐 표시                    */
    /* ---------------------------------------------------------------------- */
    u8g2_DrawFrame(u8g2, 6, 50, 228, 62);
    min_alt = 32767;
    max_alt = -32768;
    for (i = 0u; i < MOTOR_HISTORY_SAMPLE_COUNT; i++)
    {
        int16_t alt_m;
        alt_m = state->dyn.alt_history_m[i];
        if (alt_m < min_alt) min_alt = alt_m;
        if (alt_m > max_alt) max_alt = alt_m;
    }
    if (max_alt <= min_alt)
    {
        max_alt = (int16_t)(min_alt + 1);
    }

    head = state->dyn.history_head;
    for (i = 0u; i < MOTOR_HISTORY_SAMPLE_COUNT; i++)
    {
        uint16_t idx;
        uint8_t x;
        uint8_t y_alt;
        uint8_t y_grade;
        int16_t alt_m;
        int16_t grade_x10;

        idx = (uint16_t)((head + i) % MOTOR_HISTORY_SAMPLE_COUNT);
        alt_m = state->dyn.alt_history_m[idx];
        grade_x10 = state->dyn.grade_history_x10[idx];
        x = (uint8_t)(8u + (i * 224u) / MOTOR_HISTORY_SAMPLE_COUNT);
        y_alt = (uint8_t)(108u - (((int32_t)(alt_m - min_alt) * 54) / (max_alt - min_alt)));
        y_grade = (uint8_t)(81u - (grade_x10 / 5));
        if (y_grade < 52u) y_grade = 52u;
        if (y_grade > 110u) y_grade = 110u;
        u8g2_DrawPixel(u8g2, x, y_alt);
        if ((i & 1u) == 0u)
        {
            u8g2_DrawPixel(u8g2, x, y_grade);
        }
    }

    Motor_UI_DrawBottomHint(u8g2, "ALT", "GRADE", "6 MENU");
}
