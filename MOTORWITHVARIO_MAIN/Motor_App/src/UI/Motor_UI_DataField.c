
#include "Motor_UI_Internal.h"

#include "Motor_DataField.h"

#include <string.h>

void Motor_UI_DrawScreen_DataField(u8g2_t *u8g2, const motor_state_t *state, uint8_t page_index)
{
    uint8_t row;
    uint8_t col;
    uint8_t slot;

    if ((u8g2 == 0) || (state == 0) || (page_index >= MOTOR_DATA_FIELD_PAGE_COUNT))
    {
        return;
    }

    Motor_UI_DrawStatusBar(u8g2, state);

    /* ---------------------------------------------------------------------- */
    /*  15개 데이터 필드 페이지                                                 */
    /*  - 3행 x 5열 격자                                                        */
    /*  - 각 셀은 짧은 label + value 2줄 구성                                    */
    /*  - edit mode일 때 선택 slot 둘레를 추가 frame으로 표시                   */
    /* ---------------------------------------------------------------------- */
    for (row = 0u; row < 3u; row++)
    {
        for (col = 0u; col < 5u; col++)
        {
            motor_data_field_text_t text;
            motor_data_field_id_t field_id;
            uint8_t x;
            uint8_t y;

            slot = (uint8_t)(row * 5u + col);
            field_id = (motor_data_field_id_t)state->settings.data_fields[page_index][slot];
            Motor_DataField_Format(field_id, state, &text);

            x = (uint8_t)(4u + col * 47u);
            y = (uint8_t)(14u + row * 34u);

            u8g2_DrawFrame(u8g2, x, y, 44, 30);
            if ((state->ui.editing != 0u) && (state->ui.selected_slot == slot))
            {
                u8g2_DrawFrame(u8g2, x - 1u, y - 1u, 46, 32);
            }

            u8g2_SetFont(u8g2, u8g2_font_4x6_tr);
            u8g2_DrawStr(u8g2, x + 2u, y + 6u, text.label);
            u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
            u8g2_DrawStr(u8g2, x + 2u, y + 16u, text.value);
            u8g2_SetFont(u8g2, u8g2_font_4x6_tr);
            u8g2_DrawStr(u8g2, x + 2u, y + 26u, Motor_DataField_GetFieldName(field_id));
        }
    }

    Motor_UI_DrawBottomHint(u8g2,
                            (state->ui.editing != 0u) ? "1/2 SLOT" : "1< 2>",
                            (state->ui.editing != 0u) ? "3/4 FIELD" : "5 EDIT",
                            "6 MENU");
}
