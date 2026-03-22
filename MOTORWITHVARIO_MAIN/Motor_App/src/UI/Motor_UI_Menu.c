
#include "Motor_UI_Internal.h"

static const char *const s_menu_items[MOTOR_MENU_ITEM_COUNT] =
{
    "0-100 / ACCEL TEST",
    "CIRCUIT LAP TIMER",
    "MY RIDE LOGS",
    "OBD CONNECT",
    "OBD ERROR CODE",
    "SETTINGS"
};

void Motor_UI_DrawScreen_Menu(u8g2_t *u8g2, const motor_state_t *state)
{
    uint8_t i;
    uint8_t y;

    if ((u8g2 == 0) || (state == 0))
    {
        return;
    }

    Motor_UI_DrawStatusBar(u8g2, state);
    u8g2_SetFont(u8g2, u8g2_font_6x10_tr);
    u8g2_DrawStr(u8g2, 6, 22, "MAIN MENU");

    for (i = 0u; i < MOTOR_MENU_ITEM_COUNT; i++)
    {
        y = (uint8_t)(32u + i * 14u);
        if (i == state->ui.selected_index)
        {
            u8g2_DrawBox(u8g2, 6, y - 9u, 228, 12);
            u8g2_SetDrawColor(u8g2, 0);
            u8g2_DrawStr(u8g2, 10, y, s_menu_items[i]);
            u8g2_SetDrawColor(u8g2, 1);
        }
        else
        {
            u8g2_DrawStr(u8g2, 10, y, s_menu_items[i]);
        }
    }

    Motor_UI_DrawBottomHint(u8g2, "1/2 MOVE", "5 ENTER", "6 BACK");
}
