#ifndef UI_SCREEN_VARIO_MODULE_H
#define UI_SCREEN_VARIO_MODULE_H

#include "ui_types.h"
#include "u8g2.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* VARIO root screen bridge                                                    */
/*                                                                            */
/* 이 파일은 "UI 엔진" 과 "VARIO 앱 내부 renderer" 사이의 단 하나의 bridge 이다. */
/*                                                                            */
/* 책임                                                                          */
/* 1) 현재 VARIO mode 를 보고                                                   */
/*    - full screen 인가                                                        */
/*    - status bar + bottom bar fixed screen 인가                               */
/*    를 UI 엔진에 알려 준다.                                                   */
/* 2) status bar / bottom bar 가 보이는 mode 에서는                            */
/*    기존 UI_BottomBar API 로 하단 라벨을 정의한다.                            */
/* 3) UI 엔진이 계산한 viewport 를 VARIO renderer 가 이해하는                    */
/*    vario_viewport_t 로 변환해 공급한다.                                      */
/* 4) 실제 pixel draw 는 VARIO 폴더의 Vario_Display_RenderCurrent() 에 넘긴다.  */
/*                                                                            */
/* 중요                                                                         */
/* - VARIO 폴더 안쪽 renderer 들은 ui_engine.c / ui_bottombar.c 를 모른다.      */
/* - 반대로 이 bridge 는 UI 엔진 쪽에 있으므로 양쪽 API 를 안전하게 접착할 수     */
/*   있다.                                                                      */
/* -------------------------------------------------------------------------- */
void UI_ScreenVario_Init(void);
void UI_ScreenVario_OnEnter(void);
void UI_ScreenVario_Task(uint32_t now_ms);

ui_layout_mode_t UI_ScreenVario_GetLayoutMode(void);
bool UI_ScreenVario_IsStatusBarVisible(void);
bool UI_ScreenVario_IsBottomBarVisible(void);

/* -------------------------------------------------------------------------- */
/* Draw                                                                         */
/*                                                                            */
/* 이 함수는 오직 "main viewport 안쪽" 의 VARIO 본문만 그린다.                  */
/* status bar / bottom bar / popup / toast 합성은 UI 엔진이 따로 한다.         */
/* -------------------------------------------------------------------------- */
void UI_ScreenVario_Draw(u8g2_t *u8g2, const ui_rect_t *viewport);

#ifdef __cplusplus
}
#endif

#endif /* UI_SCREEN_VARIO_MODULE_H */
