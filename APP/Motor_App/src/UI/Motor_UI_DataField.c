#include "Motor_UI_Internal.h"

#include "Motor_DataField.h"

void Motor_UI_DrawScreen_DataField(u8g2_t *u8g2, const ui_rect_t *viewport, const motor_state_t *state, uint8_t page_index)
{
    uint8_t row;
    uint8_t col;
    uint8_t slot;
    int16_t x;
    int16_t y;

    if ((u8g2 == 0) || (viewport == 0) || (state == 0) || (page_index >= MOTOR_DATA_FIELD_PAGE_COUNT))
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  15개 데이터 필드 페이지                                                 */
    /*  - viewport 안에 3행 x 5열 격자를 배치한다.                              */
    /*  - 각 셀은 label / value / field name 3층 구조다.                        */
    /*  - edit mode일 때 선택 slot 둘레를 이중 frame으로 강조한다.             */
    /* ---------------------------------------------------------------------- */
    Motor_UI_DrawViewportTitle(u8g2, viewport, (page_index == 0u) ? "DATA FIELD 1" : "DATA FIELD 2");

    for (row = 0u; row < 3u; row++)
    {
        for (col = 0u; col < 5u; col++)
        {
            motor_data_field_text_t text;
            motor_data_field_id_t field_id;

            slot = (uint8_t)(row * 5u + col);
            field_id = (motor_data_field_id_t)state->settings.data_fields[page_index][slot];
            Motor_DataField_Format(field_id, state, &text);

            x = viewport->x + 4 + (col * 47);
            y = viewport->y + 16 + (row * 34);

            u8g2_DrawFrame(u8g2, x, y, 44, 30);
            if ((state->ui.editing != 0u) && (state->ui.selected_slot == slot))
            {
                u8g2_DrawFrame(u8g2, x - 1, y - 1, 46, 32);
            }
            else if ((state->ui.editing == 0u) && (state->ui.selected_slot == slot))
            {
                u8g2_DrawRFrame(u8g2, x - 1, y - 1, 46, 32, 2);
            }

            u8g2_SetFont(u8g2, u8g2_font_4x6_tr);
            u8g2_DrawStr(u8g2, x + 2, y + 6, text.label);
            u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
            u8g2_DrawStr(u8g2, x + 2, y + 16, text.value);
            u8g2_SetFont(u8g2, u8g2_font_4x6_tr);
            u8g2_DrawStr(u8g2, x + 2, y + 26, Motor_DataField_GetFieldName(field_id));
        }
    }
}
