#include "Motor_UI_Internal.h"

#include "Motor_Units.h"

#include <stdio.h>
#include <stdlib.h>

void Motor_UI_DrawScreen_Altitude(u8g2_t *u8g2, const ui_rect_t *viewport, const motor_state_t *state)
{
    int16_t graph_x;
    int16_t graph_y;
    int16_t graph_w;
    int16_t graph_h;
    int16_t min_alt;
    int16_t max_alt;
    uint16_t i;
    uint16_t head;
    char alt_text[20];
    char rel_text[20];
    char grade_text[20];
    char vario_text[20];

    if ((u8g2 == 0) || (viewport == 0) || (state == 0))
    {
        return;
    }

    Motor_UI_DrawViewportTitle(u8g2, viewport, "ALTITUDE / GRADE");

    Motor_Units_FormatAltitude(alt_text, sizeof(alt_text), state->nav.altitude_cm, &state->settings.units);
    Motor_Units_FormatAltitude(rel_text, sizeof(rel_text), state->nav.rel_altitude_cm, &state->settings.units);
    (void)snprintf(grade_text, sizeof(grade_text), "%+ld.%01ld%%",
                   (long)(state->snapshot.altitude.grade_noimu_x10 / 10),
                   (long)labs((long)(state->snapshot.altitude.grade_noimu_x10 % 10)));
    (void)snprintf(vario_text, sizeof(vario_text), "%+ld.%01ldm/s",
                   (long)(state->snapshot.altitude.vario_fast_noimu_cms / 100),
                   (long)labs((long)((state->snapshot.altitude.vario_fast_noimu_cms / 10) % 10)));

    /* ---------------------------------------------------------------------- */
    /*  상단 계기 박스 4개                                                      */
    /*  - ALT, REL, GRADE, VARIO를 각각 작은 박스로 배치한다.                  */
    /* ---------------------------------------------------------------------- */
    Motor_UI_DrawLabeledValueBox(u8g2, viewport->x + 4,   viewport->y + 16, 56, 22, "ALT",   alt_text,   0u);
    Motor_UI_DrawLabeledValueBox(u8g2, viewport->x + 62,  viewport->y + 16, 56, 22, "REL",   rel_text,   0u);
    Motor_UI_DrawLabeledValueBox(u8g2, viewport->x + 120, viewport->y + 16, 56, 22, "GRADE", grade_text, 0u);
    Motor_UI_DrawLabeledValueBox(u8g2, viewport->x + 178, viewport->y + 16, 58, 22, "VARIO", vario_text, 0u);

    /* ---------------------------------------------------------------------- */
    /*  하단 그래프                                                            */
    /*  - alt history를 실선 점열로, grade history를 얕은 보조 trace로 그린다. */
    /* ---------------------------------------------------------------------- */
    graph_x = (int16_t)(viewport->x + 6);
    graph_y = (int16_t)(viewport->y + 44);
    graph_w = (int16_t)(viewport->w - 12);
    graph_h = (int16_t)(viewport->h - 50);

    u8g2_DrawFrame(u8g2, graph_x, graph_y, graph_w, graph_h);
    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(u8g2, graph_x + 4, graph_y + 7, "ALT HISTORY");
    u8g2_DrawStr(u8g2, graph_x + graph_w - 44, graph_y + 7, "GRADE");

    min_alt = 32767;
    max_alt = -32768;
    for (i = 0u; i < MOTOR_HISTORY_SAMPLE_COUNT; i++)
    {
        if (state->dyn.alt_history_m[i] < min_alt) min_alt = state->dyn.alt_history_m[i];
        if (state->dyn.alt_history_m[i] > max_alt) max_alt = state->dyn.alt_history_m[i];
    }
    if (max_alt <= min_alt)
    {
        max_alt = (int16_t)(min_alt + 1);
    }

    head = state->dyn.history_head;
    for (i = 0u; i < MOTOR_HISTORY_SAMPLE_COUNT; i++)
    {
        uint16_t idx;
        int16_t x;
        int16_t y_alt;
        int16_t y_grade;
        int16_t alt_m;
        int16_t grade_x10;

        idx = (uint16_t)((head + i) % MOTOR_HISTORY_SAMPLE_COUNT);
        alt_m = state->dyn.alt_history_m[idx];
        grade_x10 = state->dyn.grade_history_x10[idx];

        x = (int16_t)(graph_x + 2 + (i * (graph_w - 4)) / MOTOR_HISTORY_SAMPLE_COUNT);
        y_alt = (int16_t)(graph_y + graph_h - 3 - ((int32_t)(alt_m - min_alt) * (graph_h - 10)) / (max_alt - min_alt));
        y_grade = (int16_t)(graph_y + (graph_h / 2) - (grade_x10 / 6));

        if (y_alt < graph_y + 9) y_alt = graph_y + 9;
        if (y_alt > graph_y + graph_h - 3) y_alt = graph_y + graph_h - 3;
        if (y_grade < graph_y + 9) y_grade = graph_y + 9;
        if (y_grade > graph_y + graph_h - 3) y_grade = graph_y + graph_h - 3;

        u8g2_DrawPixel(u8g2, x, y_alt);
        if ((i & 1u) == 0u)
        {
            u8g2_DrawPixel(u8g2, x, y_grade);
        }
    }
}
