#include "Motor_UI_Internal.h"

void Motor_UI_DrawScreen_Stub(u8g2_t *u8g2,
                              const ui_rect_t *viewport,
                              const motor_state_t *state,
                              const char *title,
                              const char *line1,
                              const char *line2)
{
    (void)state;

    if ((u8g2 == 0) || (viewport == 0))
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  스텁 화면                                                              */
    /*  - 메뉴에서 아직 구현되지 않은 모드로 들어왔을 때,                      */
    /*    중앙 안내 문구만 깔끔하게 표시한다.                                  */
    /* ---------------------------------------------------------------------- */
    Motor_UI_DrawCenteredTextBlock(u8g2,
                                   viewport,
                                   (int16_t)((viewport->h / 2) - 18),
                                   title,
                                   line1,
                                   line2);
}
