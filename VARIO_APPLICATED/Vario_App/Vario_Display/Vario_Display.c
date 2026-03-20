#include "Vario_Display.h"

#include "u8g2_uc1608_stm32.h"
#include "ui_bottombar.h"

#include "Vario_Display_Common.h"
#include "Vario_QuickSet.h"
#include "Vario_Screen1.h"
#include "Vario_Screen2.h"
#include "Vario_Screen3.h"
#include "Vario_Setting.h"
#include "Vario_ValueSetting.h"
#include "Vario_Dev.h"
#include "Vario_State.h"

#include <stdbool.h>

void Vario_Display_Init(void)
{
    Vario_State_RequestRedraw();
}

void Vario_Display_Task(uint32_t now_ms)
{
    u8g2_t                 *u8g2;
    vario_mode_t            mode;
    const vario_dev_settings_t *dev;
    bool                    always_refresh;

    (void)now_ms;

    mode = Vario_State_GetMode();
    dev  = Vario_Dev_Get();

    /* ---------------------------------------------------------------------- */
    /*  settings / quickset / valuesetting 화면은                               */
    /*  status bar 및 bottom bar 가 포함되므로                                  */
    /*  깜빡임/토글 요소를 위해 매 프레임 갱신한다.                              */
    /* ---------------------------------------------------------------------- */
    always_refresh = ((mode == VARIO_MODE_SETTING) ||
                      (mode == VARIO_MODE_QUICKSET) ||
                      (mode == VARIO_MODE_VALUESETTING));

    if ((always_refresh == false) &&
        (Vario_State_IsRedrawRequested() == false) &&
        ((dev == 0) || (dev->force_full_redraw == 0u)))
    {
        return;
    }

    if (U8G2_UC1608_TryAcquireFrameToken() == 0u)
    {
        return;
    }

    u8g2 = U8G2_UC1608_GetHandle();
    if (u8g2 == 0)
    {
        Vario_State_RequestRedraw();
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  공용 frame token 을 얻은 뒤 버퍼를 전부 clear 하고                      */
    /*  현재 mode 에 맞는 renderer 하나만 호출한다.                            */
    /* ---------------------------------------------------------------------- */
    u8g2_ClearBuffer(u8g2);

    switch (mode)
    {
        case VARIO_MODE_SCREEN_1:
            Vario_Screen1_Render(u8g2, 0);
            break;

        case VARIO_MODE_SCREEN_2:
            Vario_Screen2_Render(u8g2, 0);
            break;

        case VARIO_MODE_SCREEN_3:
            Vario_Screen3_Render(u8g2, 0);
            break;

        case VARIO_MODE_SETTING:
            Vario_Display_ConfigureBottomBar(mode);
            Vario_Setting_Render(u8g2, 0);
            Vario_Display_DrawSharedBars();
            break;

        case VARIO_MODE_QUICKSET:
            Vario_Display_ConfigureBottomBar(mode);
            Vario_QuickSet_Render(u8g2, 0);
            Vario_Display_DrawSharedBars();
            break;

        case VARIO_MODE_VALUESETTING:
            Vario_Display_ConfigureBottomBar(mode);
            Vario_ValueSetting_Render(u8g2, 0);
            Vario_Display_DrawSharedBars();
            break;

        case VARIO_MODE_COUNT:
        default:
            Vario_Screen1_Render(u8g2, 0);
            break;
    }

    U8G2_UC1608_CommitBuffer();
    Vario_State_ClearRedrawRequest();

    if (dev != 0)
    {
        Vario_Dev_ClearForceFullRedraw();
    }
}
