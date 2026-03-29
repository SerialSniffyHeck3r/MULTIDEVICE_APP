#include "Motor_UI_Internal.h"

#include <stdio.h>

static const char *const s_menu_items[MOTOR_MENU_ITEM_COUNT] =
{
    "0-100 / ACCEL TEST",
    "CIRCUIT LAP TIMER",
    "MY RIDE LOGS",
    "OBD CONNECT",
    "OBD ERROR CODE",
    "SETTINGS"
};

void Motor_UI_DrawScreen_Menu(u8g2_t *u8g2, const ui_rect_t *viewport, const motor_state_t *state)
{
    uint8_t i;
    uint8_t first_row;
    uint8_t visible_rows;
    int16_t y;

    if ((u8g2 == 0) || (viewport == 0) || (state == 0))
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  메뉴 화면                                                              */
    /*  - 상단바와 하단바는 공용 엔진이 그린다.                                 */
    /*  - 여기서는 viewport 안에 list만 배치한다.                              */
    /* ---------------------------------------------------------------------- */
    Motor_UI_DrawViewportTitle(u8g2, viewport, "MAIN MENU");
    u8g2_SetFont(u8g2, u8g2_font_6x10_tr);

    visible_rows = 6u;
    first_row = 0u;
    if (state->ui.selected_index >= visible_rows)
    {
        first_row = (uint8_t)(state->ui.selected_index - (visible_rows - 1u));
    }

    for (i = 0u; (i < visible_rows) && ((first_row + i) < MOTOR_MENU_ITEM_COUNT); i++)
    {
        y = (int16_t)(viewport->y + 24 + (i * 14));
        if ((first_row + i) == state->ui.selected_index)
        {
            u8g2_DrawBox(u8g2, viewport->x + 4, y - 9, viewport->w - 8, 12);
            u8g2_SetDrawColor(u8g2, 0);
            u8g2_DrawStr(u8g2, viewport->x + 8, y, s_menu_items[first_row + i]);
            u8g2_SetDrawColor(u8g2, 1);
        }
        else
        {
            u8g2_DrawStr(u8g2, viewport->x + 8, y, s_menu_items[first_row + i]);
        }
    }

    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(u8g2, viewport->x + 6, viewport->y + viewport->h - 4, "Use B1/B2 to move, B5 to enter");
}
