#include "Vario_Display.h"

#include "Vario_Display_Common.h"
#include "Vario_QuickSet.h"
#include "Vario_Screen1.h"
#include "Vario_Screen2.h"
#include "Vario_Screen3.h"
#include "Vario_Setting.h"
#include "Vario_ValueSetting.h"
#include "Vario_Dev.h"
#include "Vario_State.h"

void Vario_Display_Init(void)
{
    /* ---------------------------------------------------------------------- */
    /* 첫 정상 프레임에서 VARIO renderer 가 한 번 그려지도록 redraw latch 를     */
    /* 세운다.                                                                  */
    /* ---------------------------------------------------------------------- */
    Vario_State_RequestRedraw();
}

void Vario_Display_Task(uint32_t now_ms)
{
    /* ---------------------------------------------------------------------- */
    /* deprecated no-op                                                         */
    /*                                                                          */
    /* 예전 구현은 이 함수가 직접                                               */
    /*   frame token 획득 -> clear -> renderer -> commit                       */
    /* 을 수행했지만, 이제는 그 경로를 완전히 폐기했다.                          */
    /*                                                                          */
    /* 남겨 두는 이유                                                            */
    /* - 혹시 아직 수정되지 않은 다른 파일이 이 심볼을 참조해도                 */
    /*   링크 에러 없이 "아무 것도 하지 않음" 으로 안전하게 넘어가게 하기 위함.  */
    /* ---------------------------------------------------------------------- */
    (void)now_ms;
}

void Vario_Display_RenderCurrent(u8g2_t *u8g2)
{
    vario_mode_t mode;
    const vario_dev_settings_t *dev;

    if (u8g2 == NULL)
    {
        return;
    }

    mode = Vario_State_GetMode();
    dev = Vario_Dev_Get();

    /* ---------------------------------------------------------------------- */
    /* mode 별 본문 renderer dispatch                                           */
    /*                                                                          */
    /* 중요                                                                         */
    /* - status bar / bottom bar draw 금지                                      */
    /* - buffer clear 금지                                                      */
    /* - commit 금지                                                            */
    /*                                                                          */
    /* 오직 "지금 mode 의 main content" 한 장면만 그린다.                       */
    /* ---------------------------------------------------------------------- */
    switch (mode)
    {
    case VARIO_MODE_SCREEN_1:
        Vario_Screen1_Render(u8g2, NULL);
        break;

    case VARIO_MODE_SCREEN_2:
        Vario_Screen2_Render(u8g2, NULL);
        break;

    case VARIO_MODE_SCREEN_3:
        Vario_Screen3_Render(u8g2, NULL);
        break;

    case VARIO_MODE_SETTING:
        Vario_Setting_Render(u8g2, NULL);
        break;

    case VARIO_MODE_QUICKSET:
        Vario_QuickSet_Render(u8g2, NULL);
        break;

    case VARIO_MODE_VALUESETTING:
        Vario_ValueSetting_Render(u8g2, NULL);
        break;

    case VARIO_MODE_COUNT:
    default:
        Vario_Screen1_Render(u8g2, NULL);
        break;
    }

    /* ---------------------------------------------------------------------- */
    /* 현재 프레임이 실제로 renderer 를 통과했으므로                             */
    /* redraw / force redraw latch 를 이 시점에 내린다.                         */
    /*                                                                          */
    /* 기존 direct-display 경로에서는 commit 직후 내렸지만,                     */
    /* 지금은 UI 엔진 root compose 내부에서 이 함수가 호출되므로                 */
    /* "본문 draw 완료" 시점에 내려도 의미가 충분하다.                          */
    /* ---------------------------------------------------------------------- */
    Vario_State_ClearRedrawRequest();

    if (dev != NULL)
    {
        Vario_Dev_ClearForceFullRedraw();
    }
}
