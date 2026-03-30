#include "ui_screen_vario.h"

#include "ui_bottombar.h"
#include "ui_types.h"

#include "Vario_Button.h"
#include "Vario_Display.h"
#include "Vario_Display_Common.h"
#include "Vario_State.h"
#include "ui_menu.h"
#include "ui_confirm.h"

#include <string.h>

/* -------------------------------------------------------------------------- */
/* 내부 helper: mode 가 full-screen main page 인가?                             */
/*                                                                            */
/* 화면 의미                                                                    */
/* - SCREEN_1 / SCREEN_2 / SCREEN_3 는 사용자가 요구한 "바리오 창" 이다.         */
/* - 이 3개는 status bar / bottom bar 없이 240x128 전체를 바로 써야 한다.       */
/* -------------------------------------------------------------------------- */
static bool ui_screen_vario_mode_uses_fullscreen(vario_mode_t mode)
{
    switch (mode)
    {
    case VARIO_MODE_SCREEN_1:
    case VARIO_MODE_SCREEN_2:
    case VARIO_MODE_SCREEN_3:
        return true;

    case VARIO_MODE_SETTING:
    case VARIO_MODE_QUICKSET:
    case VARIO_MODE_VALUESETTING:
    case VARIO_MODE_COUNT:
    default:
        return false;
    }
}

/* -------------------------------------------------------------------------- */
/* 내부 helper: mode 가 status+bottom fixed mode 인가?                         */
/*                                                                            */
/* 화면 의미                                                                    */
/* - SETTING / QUICKSET / VALUESETTING 은                                      */
/*   상단 status bar 와 하단 bottom bar 를 모두 가진 "설정 계열" 화면이다.       */
/* - 따라서 main viewport 는 UI 엔진이 계산한 content 영역만 사용해야 한다.      */
/* -------------------------------------------------------------------------- */
static bool ui_screen_vario_mode_uses_fixed_bars(vario_mode_t mode)
{
    switch (mode)
    {
    case VARIO_MODE_SETTING:
    case VARIO_MODE_QUICKSET:
    case VARIO_MODE_VALUESETTING:
        return true;

    case VARIO_MODE_SCREEN_1:
    case VARIO_MODE_SCREEN_2:
    case VARIO_MODE_SCREEN_3:
    case VARIO_MODE_COUNT:
    default:
        return false;
    }
}

/* -------------------------------------------------------------------------- */
/* 내부 helper: UI bottom bar 라벨 정의                                         */
/*                                                                            */
/* 여기서는 하단바를 "그리지" 않는다.                                           */
/* draw 는 UI 엔진이 root compose 순서(main -> status -> bottom) 로 수행한다.   */
/* 이 helper 는 단지 현재 mode 에 맞는 각 세그먼트의 문자열만 정의한다.          */
/*                                                                            */
/* 세그먼트 위치                                                                 */
/* - F1 : 화면 맨 왼쪽 1/6 폭                                                    */
/* - F2 : 왼쪽에서 두 번째 1/6 폭                                                */
/* - F3 : 왼쪽에서 세 번째 1/6 폭                                                */
/* - F4 : 오른쪽에서 세 번째 1/6 폭                                              */
/* - F5 : 오른쪽에서 두 번째 1/6 폭                                              */
/* - F6 : 화면 맨 오른쪽 1/6 폭                                                  */
/* -------------------------------------------------------------------------- */
static void ui_screen_vario_configure_bottom_bar(vario_mode_t mode)
{
    vario_buttonbar_t bar;

    memset(&bar, 0, sizeof(bar));
    Vario_Button_GetButtonBar(mode, &bar);

    UI_BottomBar_SetMode(UI_BOTTOMBAR_MODE_BUTTONS);
    UI_BottomBar_SetMessage(NULL);

    UI_BottomBar_SetButton(UI_FKEY_1, (bar.f1 != NULL) ? bar.f1 : "", UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButton(UI_FKEY_2, (bar.f2 != NULL) ? bar.f2 : "", UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButton(UI_FKEY_3, (bar.f3 != NULL) ? bar.f3 : "", UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButton(UI_FKEY_4, (bar.f4 != NULL) ? bar.f4 : "", UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButton(UI_FKEY_5, (bar.f5 != NULL) ? bar.f5 : "", UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButton(UI_FKEY_6, (bar.f6 != NULL) ? bar.f6 : "", 0u);
}

/* -------------------------------------------------------------------------- */
/* 내부 helper: UI 엔진 viewport -> VARIO viewport 변환                         */
/*                                                                            */
/* 전달 규칙                                                                    */
/* 1) full-screen main 화면                                                     */
/*    - full viewport  : UI 엔진이 준 viewport 그대로                           */
/*    - content viewport: 동일값으로 맞춰 둔다.                                 */
/*                                                                            */
/* 2) setting / quickset / valuesetting                                         */
/*    - full viewport  : LCD 전체 240x128                                       */
/*    - content viewport: UI 엔진이 status/bottom 를 제외하고 계산한 viewport    */
/*                                                                            */
/* 이렇게 해 두면 기존 VARIO screen renderer 가                                 */
/* - main 화면에서는 GetFullViewport()                                          */
/* - 설정 화면에서는 GetContentViewport()                                       */
/* 를 그대로 쓰더라도 의도한 픽셀 영역으로 그려진다.                            */
/* -------------------------------------------------------------------------- */
static void ui_screen_vario_publish_viewports(const ui_rect_t *viewport, vario_mode_t mode)
{
    vario_viewport_t full_viewport;
    vario_viewport_t content_viewport;

    if (viewport == NULL)
    {
        Vario_Display_SetViewports(NULL, NULL);
        return;
    }

    full_viewport.x = 0;
    full_viewport.y = 0;
    full_viewport.w = UI_LCD_W;
    full_viewport.h = UI_LCD_H;

    content_viewport.x = viewport->x;
    content_viewport.y = viewport->y;
    content_viewport.w = viewport->w;
    content_viewport.h = viewport->h;

    if (ui_screen_vario_mode_uses_fullscreen(mode))
    {
        full_viewport = content_viewport;
    }

    Vario_Display_SetViewports(&full_viewport, &content_viewport);
}

void UI_ScreenVario_Init(void)
{
    /* ---------------------------------------------------------------------- */
    /* 현재 bridge 는 별도 런타임 내부 상태를 유지하지 않는다.                 */
    /*                                                                          */
    /* 여기 함수를 남겨 두는 이유                                                */
    /* - 다른 root screen 들과 API 모양을 맞추기 위해서다.                     */
    /* - 나중에 "VARIO root 진입 애니메이션" 이나 "마지막 mode 캐시" 가          */
    /*   필요해져도 public API 를 바꾸지 않고 확장할 수 있다.                   */
    /* ---------------------------------------------------------------------- */
}

void UI_ScreenVario_OnEnter(void)
{
    /* ---------------------------------------------------------------------- */
    /* VARIO root 진입 시 즉시 필요한 작업                                      */
    /*                                                                          */
    /* 현재는 renderer 상태를 리셋할 것이 없으므로                               */
    /* 별도 동작은 하지 않는다.                                                 */
    /*                                                                          */
    /* bottom bar 라벨은 mode 가 setting 계열일 때만 의미가 있으므로            */
    /* 실제 세팅은 Task()/Draw() 시점에 현재 mode 를 보고 매번 공급한다.        */
    /* ---------------------------------------------------------------------- */
}

void UI_ScreenVario_Task(uint32_t now_ms)
{
    vario_mode_t mode;

    (void)now_ms;

    mode = Vario_State_GetMode();

    /* ---------------------------------------------------------------------- */
    /* setting 계열 화면일 때는                                                 */
    /* 하단 바 문자열이 언제 mode change 되어도 즉시 갱신되게 여기서 한 번      */
    /* 미리 정의해 둔다.                                                        */
    /*                                                                          */
    /* draw 시점에도 다시 동일한 내용을 넣으므로,                                */
    /* 화면 갱신 타이밍이 약간 바뀌더라도 stale label 이 남지 않는다.            */
    /* ---------------------------------------------------------------------- */
    if (ui_screen_vario_mode_uses_fixed_bars(mode) ||
        (UI_Menu_IsVisible() != false) ||
        (UI_Confirm_IsVisible() != false))
    {
        ui_screen_vario_configure_bottom_bar(mode);
    }
}

ui_layout_mode_t UI_ScreenVario_GetLayoutMode(void)
{
    if (ui_screen_vario_mode_uses_fixed_bars(Vario_State_GetMode()))
    {
        return UI_LAYOUT_MODE_TOP_BOTTOM_FIXED;
    }

    return UI_LAYOUT_MODE_FULLSCREEN;
}

bool UI_ScreenVario_IsStatusBarVisible(void)
{
    return ui_screen_vario_mode_uses_fixed_bars(Vario_State_GetMode());
}

bool UI_ScreenVario_IsBottomBarVisible(void)
{
    return ui_screen_vario_mode_uses_fixed_bars(Vario_State_GetMode());
}

void UI_ScreenVario_Draw(u8g2_t *u8g2, const ui_rect_t *viewport)
{
    vario_mode_t mode;

    if ((u8g2 == NULL) || (viewport == NULL))
    {
        return;
    }

    mode = Vario_State_GetMode();

    /* ---------------------------------------------------------------------- */
    /* setting 계열 화면일 때만 하단바 라벨을 정의한다.                        */
    /*                                                                          */
    /* 실제 픽셀 draw 는 UI 엔진의                                              */
    /*   UI_BottomBar_Draw(u8g2, pressed_mask)                                  */
    /* 가 이 함수 뒤에서 수행한다.                                              */
    /* ---------------------------------------------------------------------- */
    if (ui_screen_vario_mode_uses_fixed_bars(mode) ||
        (UI_Menu_IsVisible() != false) ||
        (UI_Confirm_IsVisible() != false))
    {
        ui_screen_vario_configure_bottom_bar(mode);
    }

    /* ---------------------------------------------------------------------- */
    /* UI 엔진이 계산한 viewport 를 VARIO renderer 가 이해하는 형식으로 전달     */
    /* ---------------------------------------------------------------------- */
    ui_screen_vario_publish_viewports(viewport, mode);

    /* ---------------------------------------------------------------------- */
    /* 본문 draw 위임                                                            */
    /*                                                                          */
    /* 호출 경로                                                                  */
    /*   Vario_App_Task()                                                        */
    /*      -> UI_Engine_Task()                                                  */
    /*         -> ui_engine_draw_root()                                          */
    /*            -> UI_ScreenVario_Draw()                                       */
    /*               -> Vario_Display_RenderCurrent()                            */
    /*                  -> Vario_ScreenX_Render() / Vario_Setting_Render()       */
    /*                                                                          */
    /* 즉, 이제 VARIO 화면도 "반드시 UI 엔진 root compose" 안에서 그려진다.      */
    /* ---------------------------------------------------------------------- */
    Vario_Display_RenderCurrent(u8g2);
}
