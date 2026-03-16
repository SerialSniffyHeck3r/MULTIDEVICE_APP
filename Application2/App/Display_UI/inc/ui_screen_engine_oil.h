#ifndef UI_SCREEN_ENGINE_OIL_MODULE_H
#define UI_SCREEN_ENGINE_OIL_MODULE_H

#include "ui_types.h"
#include "button.h"
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  Engine oil screen action                                                  */
/*                                                                            */
/*  UI 엔진은 이 action만 보고 TEST 홈으로 복귀할지 결정한다.                  */
/*  자릿수 이동 / 값 편집 / 저장 여부는 모두 이 화면 파일 안에서 끝낸다.       */
/* -------------------------------------------------------------------------- */
typedef enum
{
    UI_SCREEN_ENGINE_OIL_ACTION_NONE = 0,
    UI_SCREEN_ENGINE_OIL_ACTION_BACK_TO_TEST,
    UI_SCREEN_ENGINE_OIL_ACTION_SAVE_AND_BACK_TO_TEST
} ui_screen_engine_oil_action_t;

/* -------------------------------------------------------------------------- */
/*  Init                                                                      */
/*                                                                            */
/*  부팅 시 1회 호출한다.                                                     */
/*  - 저장 완료 기준값(saved)을 기본값으로 초기화한다.                         */
/*  - 편집값(edit)과 선택 자릿수도 기본 위치로 맞춘다.                         */
/* -------------------------------------------------------------------------- */
void UI_ScreenEngineOil_Init(void);

/* -------------------------------------------------------------------------- */
/*  On enter                                                                  */
/*                                                                            */
/*  TEST 홈에서 이 화면으로 진입할 때 호출한다.                                */
/*  - 편집 시작값을 저장값으로부터 다시 로드한다.                              */
/*  - 선택 자릿수를 가장 왼쪽 자리로 보낸다.                                   */
/*  - 이 화면 전용 하단바를 세팅한다.                                          */
/* -------------------------------------------------------------------------- */
void UI_ScreenEngineOil_OnEnter(void);

/* -------------------------------------------------------------------------- */
/*  On resume                                                                 */
/*                                                                            */
/*  다른 화면(GPS)으로 잠깐 나갔다가                                            */
/*  "현재 편집 상태 그대로" 돌아와야 할 때 호출한다.                          */
/*  - 편집값과 선택 자릿수는 건드리지 않는다.                                   */
/*  - 이 화면 전용 하단바만 다시 세팅한다.                                      */
/* -------------------------------------------------------------------------- */
void UI_ScreenEngineOil_OnResume(void);

/* -------------------------------------------------------------------------- */
/*  Button event handler                                                      */
/*                                                                            */
/*  short press 기준 버튼 매핑                                                */
/*  - F1 : 저장하지 않고 TEST 홈으로 복귀                                     */
/*  - F2 : 선택 자릿수 왼쪽 이동                                              */
/*  - F3 : 선택 자릿수 오른쪽 이동                                            */
/*  - F4 : 현재 자릿수 +1                                                     */
/*  - F5 : 현재 자릿수 -1                                                     */
/*  - F6 : 현재 편집값 저장 후 TEST 홈 복귀                                   */
/* -------------------------------------------------------------------------- */
ui_screen_engine_oil_action_t UI_ScreenEngineOil_HandleButtonEvent(const button_event_t *event,
                                                                   uint32_t now_ms);

/* -------------------------------------------------------------------------- */
/*  Compose information                                                       */
/*                                                                            */
/*  이 화면은 항상                                                            */
/*  - status bar 표시                                                         */
/*  - bottom bar 표시                                                         */
/*  - main viewport = TOP+BTM fixed mode                                      */
/*  로 동작한다.                                                              */
/* -------------------------------------------------------------------------- */
ui_layout_mode_t UI_ScreenEngineOil_GetLayoutMode(void);
bool UI_ScreenEngineOil_IsStatusBarVisible(void);
bool UI_ScreenEngineOil_IsBottomBarVisible(void);

/* -------------------------------------------------------------------------- */
/*  Draw-state getters                                                        */
/* -------------------------------------------------------------------------- */
uint32_t UI_ScreenEngineOil_GetIntervalValue(void);
uint8_t UI_ScreenEngineOil_GetSelectedDigitIndex(void);

/* -------------------------------------------------------------------------- */
/*  Engine oil interval screen renderer                                       */
/*                                                                            */
/*  이 함수는 오직 main viewport 안만 그린다.                                  */
/*  status bar / bottom bar / popup / toast는 UI 엔진이 따로 합성한다.        */
/* -------------------------------------------------------------------------- */
void UI_ScreenEngineOil_Draw(u8g2_t *u8g2,
                             const ui_rect_t *viewport,
                             uint32_t interval_value,
                             uint8_t selected_digit_index);

#ifdef __cplusplus
}
#endif

#endif /* UI_SCREEN_ENGINE_OIL_MODULE_H */
