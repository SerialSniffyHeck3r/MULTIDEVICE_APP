#ifndef UI_SCREEN_TEST_MODULE_H
#define UI_SCREEN_TEST_MODULE_H

#include "ui_types.h"
#include "button.h"
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  Test screen action                                                        */
/*                                                                            */
/*  UI 엔진은 이 enum만 보고 화면 전환만 담당한다.                              */
/*  실제 버튼 해석은 이 파일(ui_screen_test.c) 안에서 끝낸다.                  */
/* -------------------------------------------------------------------------- */
typedef enum
{
    UI_SCREEN_TEST_ACTION_NONE = 0,
    UI_SCREEN_TEST_ACTION_ENTER_DEBUG_LEGACY,
    UI_SCREEN_TEST_ACTION_ENTER_ENGINE_OIL,
    UI_SCREEN_TEST_ACTION_ENTER_GPS
} ui_screen_test_action_t;

/* -------------------------------------------------------------------------- */
/*  Init                                                                      */
/*                                                                            */
/*  부팅 시 1회 호출한다.                                                     */
/*  - 테스트 화면 내부 상태를 기본값으로 초기화한다.                           */
/*  - 20Hz 카운터 기준 tick을 현재 값으로 맞춘다.                              */
/*  - 테스트 화면 전용 하단바 내용을 준비한다.                                 */
/* -------------------------------------------------------------------------- */
void UI_ScreenTest_Init(uint32_t ui_tick_20hz);

/* -------------------------------------------------------------------------- */
/*  On enter                                                                  */
/*                                                                            */
/*  다른 화면에서 TEST 홈으로 복귀할 때 호출한다.                              */
/*  - 테스트 화면의 진행 상태(live counter, layout mode)는 유지한다.           */
/*  - 대신 현재 화면에 보여야 할 테스트 전용 하단바만 다시 세팅한다.           */
/* -------------------------------------------------------------------------- */
void UI_ScreenTest_OnEnter(void);

/* -------------------------------------------------------------------------- */
/*  20Hz dynamic update                                                       */
/*                                                                            */
/*  현재 화면이 TEST일 때만 UI 엔진이 호출한다.                                */
/*  - 큰 숫자 카운터 증가                                                     */
/*  - 느린 깜빡임 패널 위상 반영                                               */
/*  - pause 상태일 때는 숫자와 깜빡임 상태를 고정                              */
/* -------------------------------------------------------------------------- */
void UI_ScreenTest_Task(uint32_t ui_tick_20hz);

/* -------------------------------------------------------------------------- */
/*  Button event handler                                                      */
/*                                                                            */
/*  Button_PopEvent()로 꺼낸 이벤트를 TEST 화면 규칙에 맞춰 처리한다.          */
/*  - toast / popup 표시                                                     */
/*  - layout mode 변경                                                        */
/*  - pause / run 전환                                                        */
/*  - 다음 화면으로 넘어가야 할 경우 action enum으로 알려 준다.                */
/* -------------------------------------------------------------------------- */
ui_screen_test_action_t UI_ScreenTest_HandleButtonEvent(const button_event_t *event,
                                                        uint32_t now_ms);

/* -------------------------------------------------------------------------- */
/*  Compose information                                                       */
/*                                                                            */
/*  UI 엔진은 이 getter만 사용해서                                             */
/*  - status bar를 그릴지                                                     */
/*  - bottom bar를 그릴지                                                     */
/*  - main viewport를 어떤 모드로 계산할지                                    */
/*  를 결정한다.                                                              */
/* -------------------------------------------------------------------------- */
ui_layout_mode_t UI_ScreenTest_GetLayoutMode(void);
bool UI_ScreenTest_IsStatusBarVisible(void);
bool UI_ScreenTest_IsBottomBarVisible(uint32_t now_ms, uint32_t pressed_mask);

/* -------------------------------------------------------------------------- */
/*  Draw-state getters                                                        */
/*                                                                            */
/*  draw 함수에 필요한 현재 표시 상태를 UI 엔진이 읽을 때 사용한다.            */
/* -------------------------------------------------------------------------- */
uint32_t UI_ScreenTest_GetLiveCounter20Hz(void);
bool UI_ScreenTest_GetUpdatesPaused(void);
bool UI_ScreenTest_GetBlinkPhase(void);
uint8_t UI_ScreenTest_GetCuteIconIndex(void);

/* -------------------------------------------------------------------------- */
/*  Test screen renderer                                                      */
/*                                                                            */
/*  이 함수는 오직 main viewport 안쪽만 그린다.                                */
/*  status bar / bottom bar / popup / toast는 절대 여기서 그리지 않는다.      */
/* -------------------------------------------------------------------------- */
void UI_ScreenTest_Draw(u8g2_t *u8g2,
                        const ui_rect_t *viewport,
                        uint32_t live_counter_20hz,
                        bool updates_paused,
                        bool blink_phase_on,
                        uint8_t cute_icon_index,
                        ui_layout_mode_t layout_mode);

#ifdef __cplusplus
}
#endif

#endif /* UI_SCREEN_TEST_MODULE_H */
