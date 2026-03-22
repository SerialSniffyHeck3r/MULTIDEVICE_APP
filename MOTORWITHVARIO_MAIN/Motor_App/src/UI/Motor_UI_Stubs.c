
#include "Motor_UI_Internal.h"

void Motor_UI_DrawScreen_Stub(u8g2_t *u8g2, const motor_state_t *state, const char *title, const char *line1, const char *line2)
{
    if ((u8g2 == 0) || (state == 0))
    {
        return;
    }

    Motor_UI_DrawStatusBar(u8g2, state);
    Motor_UI_DrawCenteredTextBlock(u8g2, 40, title, line1, line2);
    Motor_UI_DrawBottomHint(u8g2, "", "", "6 BACK");
}
