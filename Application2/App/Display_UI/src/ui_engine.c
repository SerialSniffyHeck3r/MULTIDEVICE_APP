#include "ui_engine.h"

#include "ui_types.h"
#include "ui_statusbar.h"
#include "ui_bottombar.h"
#include "ui_popup.h"
#include "ui_toast.h"
#include "ui_boot.h"
#include "ui_screen_test.h"
#include "ui_screen_engine_oil.h"
#include "ui_common_icons.h"
#include "ui_debug_legacy.h"
#include "APP_STATE.h"
#include "button.h"
#include "u8g2_uc1608_stm32.h"

#include <string.h>

/*
 * UI_TOAST_POPUP_USAGE.txt
========================

1) 토스트를 아이콘과 함께 띄우는 방법
-------------------------------------
아래 코드를 그대로 사용하면 된다.

UI_Toast_Show("BT CONNECTED",
              icon_ui_info_8x8,
              ICON8_W,
              ICON8_H,
              now_ms,
              UI_TOAST_DEFAULT_TIMEOUT_MS);

2) 토스트를 아이콘 없이 띄우는 방법
-----------------------------------
icon 포인터와 width / height를 0으로 넣으면 된다.

UI_Toast_Show("PROFILE SAVED",
              0,
              0u,
              0u,
              now_ms,
              UI_TOAST_DEFAULT_TIMEOUT_MS);

3) 팝업을 아이콘과 함께 띄우는 방법
-----------------------------------

UI_Popup_Show("WARNING",
              "SD CARD ERROR",
              "CHECK CARD STATE",
              icon_ui_warn_8x8,
              ICON8_W,
              ICON8_H,
              now_ms,
              UI_POPUP_DEFAULT_TIMEOUT_MS);

4) 팝업을 아이콘 없이 띄우는 방법
---------------------------------

UI_Popup_Show("NOTICE",
              "NO ICON MODE",
              "TEXT ONLY POPUP",
              0,
              0u,
              0u,
              now_ms,
              UI_POPUP_DEFAULT_TIMEOUT_MS);

5) 현재 테스트 화면의 기본 동작
------------------------------
- F4는 icon이 있는 toast 예시를 띄운다.
- F5는 icon이 있는 popup 예시를 띄운다.
- 필요하면 위 예시를 그대로 복붙해서 icon 없는 버전으로 바꾸면 된다.
 *
 *
 *
 */





/* -------------------------------------------------------------------------- */
/*  Public blink variables                                                     */
/* -------------------------------------------------------------------------- */
volatile bool SlowToggle2Hz = false;
volatile bool FastToggle5Hz = false;

/* -------------------------------------------------------------------------- */
/*  20Hz source tick                                                           */
/*                                                                            */
/*  TIM7의 frame token timer를 재사용한다.                                     */
/*  - raw 20Hz tick count는 테스트 카운터 증가에 사용                           */
/*  - slow / fast blink 토글도 여기서 같이 만든다                              */
/* -------------------------------------------------------------------------- */
static volatile uint32_t s_ui_tick_20hz = 0u;
static volatile uint8_t  s_slow_divider = 0u;
static volatile uint8_t  s_fast_divider = 0u;

static ui_screen_id_t            s_current_screen = UI_SCREEN_TEST;
static ui_layout_mode_t          s_layout_mode = UI_LAYOUT_MODE_TOP_BOTTOM_FIXED;
static ui_record_state_t         s_record_state = UI_RECORD_STATE_STOP;
static ui_bluetooth_stub_state_t s_bt_stub_state = UI_BT_STUB_ON;

static uint8_t  s_imperial_units = 0u;
static uint8_t  s_test_updates_paused = 0u;
static uint8_t  s_test_cute_icon_index = 0u;
static bool     s_test_blink_phase_on = true;
static uint32_t s_test_live_counter_20hz = 0u;
static uint32_t s_test_last_processed_tick_20hz = 0u;
static uint32_t s_bottom_overlay_until_ms = 0u;
static uint32_t s_pressed_mask = 0u;
static uint8_t  s_force_redraw = 1u;

/* -------------------------------------------------------------------------- */
/*  Viewport / engine-oil setting constants                                    */
/*                                                                            */
/*  UI 본문 뷰포트는 status bar가 있는 모드에서만 위로 정확히 2px 확장한다.    */
/*  이때 왼쪽 / 오른쪽 / 아래쪽 경계는 그대로 둔다.                           */
/*                                                                            */
/*  엔진 오일 교체 주기 스텁 화면은 5자리 숫자를 편집하므로,                   */
/*  자릿수 개수와 최대값도 이 파일에서 함께 고정한다.                          */
/* -------------------------------------------------------------------------- */
#define UI_VIEWPORT_STATUSBAR_TOP_EXPAND_PX  1u
#define UI_ENGINE_OIL_DIGIT_COUNT            5u
#define UI_ENGINE_OIL_INTERVAL_MAX           99999u

/* -------------------------------------------------------------------------- */
/*  Engine oil interval stub screen state                                      */
/*                                                                            */
/*  - saved : 저장 완료된 값                                                   */
/*  - edit  : 현재 편집 중인 값                                                */
/*  - digit : 현재 선택된 자릿수 (0 = 가장 왼쪽 10^4 자리)                    */
/*  - saved_layout_mode_before_oil : 설정 화면 진입 전 TEST 홈 레이아웃 보관   */
/* -------------------------------------------------------------------------- */
static uint32_t s_engine_oil_interval_saved = 5000u;
static uint32_t s_engine_oil_interval_edit = 5000u;
static uint8_t  s_engine_oil_digit_index = 0u;
static ui_layout_mode_t s_saved_layout_mode_before_oil = UI_LAYOUT_MODE_TOP_BOTTOM_FIXED;

/* -------------------------------------------------------------------------- */
/*  Legacy debug screen F1 long-press return tracking                          */
/*                                                                            */
/*  레거시 디버그 화면은 내부에서 Button_PopEvent()를 직접 소비하므로,         */
/*  TEST 홈으로 복귀하는 "F1 길게 누름" 은 event queue가 아니라                */
/*  현재 안정 눌림 상태(Button_IsPressed) + 시간 누적으로 별도 추적한다.       */
/* -------------------------------------------------------------------------- */
static uint32_t s_debug_f1_hold_start_ms = 0u;
static uint8_t  s_debug_f1_hold_latched = 0u;

/* -------------------------------------------------------------------------- */
/*  Local helpers                                                              */
/* -------------------------------------------------------------------------- */
static void ui_engine_request_overlay(uint32_t now_ms);
static void ui_engine_configure_test_bottom_bar(void);
static void ui_engine_configure_engine_oil_bottom_bar(void);
static void ui_engine_enter_test_home(void);
static void ui_engine_enter_engine_oil_screen(void);
static void ui_engine_update_test_dynamics(void);
static void ui_engine_handle_test_events(uint32_t now_ms);
static void ui_engine_handle_engine_oil_events(uint32_t now_ms);
static void ui_engine_handle_test_button_event(const button_event_t *event, uint32_t now_ms);
static void ui_engine_handle_engine_oil_button_event(const button_event_t *event, uint32_t now_ms);
static void ui_engine_adjust_engine_oil_digit(int8_t delta);
static uint32_t ui_engine_get_engine_oil_place_value(uint8_t digit_index);
static void ui_engine_update_debug_f1_hold_return(uint32_t now_ms);
static void ui_engine_build_status_model(ui_statusbar_model_t *out_model);
static void ui_engine_compute_viewport(u8g2_t *u8g2,
                                       uint32_t now_ms,
                                       ui_rect_t *out_viewport,
                                       bool *out_status_visible,
                                       bool *out_bottom_visible);
static void ui_engine_draw_root(u8g2_t *u8g2, uint32_t now_ms);
static const uint8_t *ui_engine_get_cute_icon(uint8_t index);

void UI_Engine_Init(void)
{
    UI_BottomBar_Init();
    UI_Popup_Init();
    UI_Toast_Init();
    UI_DebugLegacy_Init();

    s_current_screen = UI_SCREEN_TEST;
    s_layout_mode = UI_LAYOUT_MODE_TOP_BOTTOM_FIXED;
    s_record_state = UI_RECORD_STATE_STOP;
    s_bt_stub_state = UI_BT_STUB_ON;
    s_imperial_units = 0u;
    s_test_updates_paused = 0u;
    s_test_cute_icon_index = 0u;
    s_test_blink_phase_on = true;
    s_test_live_counter_20hz = 0u;
    s_test_last_processed_tick_20hz = s_ui_tick_20hz;
    s_bottom_overlay_until_ms = 0u;
    s_pressed_mask = Button_GetPressedMask();
    s_force_redraw = 1u;

    s_engine_oil_interval_saved = 5000u;
    s_engine_oil_interval_edit = s_engine_oil_interval_saved;
    s_engine_oil_digit_index = 0u;
    s_saved_layout_mode_before_oil = s_layout_mode;

    s_debug_f1_hold_start_ms = 0u;
    s_debug_f1_hold_latched = 0u;

    ui_engine_configure_test_bottom_bar();
}

void UI_Engine_EarlyBootDraw(void)
{
    u8g2_t *u8g2 = U8G2_UC1608_GetHandle();

    if (u8g2 == 0)
    {
        return;
    }

    u8g2_ClearBuffer(u8g2);
    UI_Boot_Draw(u8g2);
    U8G2_UC1608_CommitBuffer();
}

void UI_Engine_RequestRedraw(void)
{
    s_force_redraw = 1u;
}


void UI_Engine_SetRecordState(uint8_t record_state)
{
    if (record_state > (uint8_t)UI_RECORD_STATE_PAUSE)
    {
        record_state = (uint8_t)UI_RECORD_STATE_STOP;
    }

    s_record_state = (ui_record_state_t)record_state;
}

void UI_Engine_SetImperialUnits(uint8_t enabled)
{
    s_imperial_units = (enabled != 0u) ? 1u : 0u;
}

void UI_Engine_SetBluetoothStubState(uint8_t state)
{
    if (state > (uint8_t)UI_BT_STUB_BLINK)
    {
        state = (uint8_t)UI_BT_STUB_ON;
    }

    s_bt_stub_state = (ui_bluetooth_stub_state_t)state;
}

void UI_Engine_OnFrameTickFromISR(void)
{
    s_ui_tick_20hz++;

    s_fast_divider++;
    if (s_fast_divider >= 2u)
    {
        s_fast_divider = 0u;
        FastToggle5Hz = (FastToggle5Hz == false);
    }

    s_slow_divider++;
    if (s_slow_divider >= 5u)
    {
        s_slow_divider = 0u;
        SlowToggle2Hz = (SlowToggle2Hz == false);
    }
}

void UI_Engine_OnBoardDebugButtonIrq(uint32_t now_ms)
{
    if (s_current_screen == UI_SCREEN_DEBUG_LEGACY)
    {
        UI_DebugLegacy_OnBoardButtonIrq(now_ms);
    }
    else
    {
        s_current_screen = UI_SCREEN_DEBUG_LEGACY;
        UI_DebugLegacy_Init();
    }

    s_debug_f1_hold_start_ms = 0u;
    s_debug_f1_hold_latched = 0u;

    UI_Toast_Hide();
    UI_Popup_Hide();
    s_force_redraw = 1u;
}

void UI_Engine_Task(uint32_t now_ms)
{
    u8g2_t *u8g2;

    if (s_current_screen == UI_SCREEN_DEBUG_LEGACY)
    {
        UI_DebugLegacy_Task(now_ms);
        ui_engine_update_debug_f1_hold_return(now_ms);
    }
    else if (s_current_screen == UI_SCREEN_ENGINE_OIL_INTERVAL)
    {
        s_pressed_mask = Button_GetPressedMask();
        ui_engine_handle_engine_oil_events(now_ms);
    }
    else
    {
        s_pressed_mask = Button_GetPressedMask();
        ui_engine_update_test_dynamics();
        ui_engine_handle_test_events(now_ms);
    }

    UI_Popup_Task(now_ms);
    UI_Toast_Task(now_ms);

    if (U8G2_UC1608_TryAcquireFrameToken() == 0u)
    {
        return;
    }

    if (s_force_redraw != 0u)
    {
        /* 현재는 frame token이 오면 항상 전체 프레임을 다시 그리지만, */
        /* 추후 dirty-region 최적화가 들어가더라도 API를 그대로 유지하기 */
        /* 위해 redraw request 플래그를 남겨 둔다. */
    }

    u8g2 = U8G2_UC1608_GetHandle();
    if (u8g2 == 0)
    {
        return;
    }

    u8g2_ClearBuffer(u8g2);

    if (s_current_screen == UI_SCREEN_DEBUG_LEGACY)
    {
        UI_DebugLegacy_Draw(u8g2, now_ms);
    }
    else
    {
        ui_engine_draw_root(u8g2, now_ms);
    }

    U8G2_UC1608_CommitBuffer();
    s_force_redraw = 0u;
}

/* -------------------------------------------------------------------------- */
/*  Overlay visibility helper                                                  */
/* -------------------------------------------------------------------------- */
static void ui_engine_request_overlay(uint32_t now_ms)
{
    s_bottom_overlay_until_ms = now_ms + UI_BOTTOMBAR_OVERLAY_TIMEOUT_MS;
}

/* -------------------------------------------------------------------------- */
/*  Test bottom bar                                                            */
/* -------------------------------------------------------------------------- */
static void ui_engine_configure_test_bottom_bar(void)
{
    UI_BottomBar_SetMode(UI_BOTTOMBAR_MODE_BUTTONS);

    UI_BottomBar_SetButton(UI_FKEY_1, "DBG", UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButton(UI_FKEY_2,
                           (s_test_updates_paused != 0u) ? "RUN" : "PAUSE",
                           UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButton(UI_FKEY_3, "MODE", UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButton(UI_FKEY_4, "TOAST", UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButton(UI_FKEY_5, "POPUP", UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButtonIcon4(UI_FKEY_6,
                                icon_arrow_right_7x4,
                                ICON7X4_W,
                                0u);
}


/* -------------------------------------------------------------------------- */
/*  Engine oil interval bottom bar                                             */
/*                                                                            */
/*  이 하단바는 6개의 물리 버튼과 1:1로 대응한다.                              */
/*  - F1 : BACK 텍스트                                                        */
/*  - F2 : 왼쪽 화살표 (자릿수 왼쪽 이동)                                      */
/*  - F3 : 오른쪽 화살표 (자릿수 오른쪽 이동)                                  */
/*  - F4 : 위쪽 화살표 (현재 자릿수 값 +1)                                     */
/*  - F5 : 아래쪽 화살표 (현재 자릿수 값 -1)                                   */
/*  - F6 : DONE 텍스트                                                        */
/*                                                                            */
/*  아이콘 높이 4px 규격은 기존 bottom bar 렌더러 제약을 그대로 따른다.       */
/* -------------------------------------------------------------------------- */
static void ui_engine_configure_engine_oil_bottom_bar(void)
{
    UI_BottomBar_SetMode(UI_BOTTOMBAR_MODE_BUTTONS);

    UI_BottomBar_SetButton(UI_FKEY_1, "BACK", UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButtonIcon4(UI_FKEY_2,
                                icon_arrow_left_7x4,
                                ICON7X4_W,
                                UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButtonIcon4(UI_FKEY_3,
                                icon_arrow_right_7x4,
                                ICON7X4_W,
                                UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButtonIcon4(UI_FKEY_4,
                                icon_arrow_up_7x4,
                                ICON7X4_W,
                                UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButtonIcon4(UI_FKEY_5,
                                icon_arrow_down_7x4,
                                ICON7X4_W,
                                UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButton(UI_FKEY_6, "DONE", 0u);
}

/* -------------------------------------------------------------------------- */
/*  Enter TEST home                                                            */
/*                                                                            */
/*  TEST 홈으로 복귀할 때 공통으로 정리해야 하는 UI 상태를 한 곳에 모은다.     */
/*  - root screen을 TEST로 되돌린다.                                           */
/*  - TEST 홈 하단바를 복원한다.                                               */
/*  - toast / popup 잔상을 지운다.                                             */
/*  - debug 화면용 F1 long-press 추적 상태를 초기화한다.                       */
/* -------------------------------------------------------------------------- */
static void ui_engine_enter_test_home(void)
{
    s_current_screen = UI_SCREEN_TEST;
    ui_engine_configure_test_bottom_bar();

    UI_Toast_Hide();
    UI_Popup_Hide();

    s_pressed_mask = Button_GetPressedMask();
    s_debug_f1_hold_start_ms = 0u;
    s_debug_f1_hold_latched = 0u;
    s_force_redraw = 1u;
}

/* -------------------------------------------------------------------------- */
/*  Enter engine oil interval stub screen                                      */
/*                                                                            */
/*  요구사항대로 이 화면은 status bar + bottom bar가 동시에 있는               */
/*  고정 레이아웃에서 동작하게 만든다.                                         */
/*                                                                            */
/*  진입 시 하는 일                                                            */
/*  - TEST 홈에서 사용 중이던 레이아웃 모드를 보관                             */
/*  - 현재 편집값을 저장값으로부터 로드                                         */
/*  - 선택 자릿수를 가장 왼쪽(만 단위)부터 시작                                */
/*  - 전용 bottom bar를 세팅                                                   */
/* -------------------------------------------------------------------------- */
static void ui_engine_enter_engine_oil_screen(void)
{
    s_saved_layout_mode_before_oil = s_layout_mode;
    s_layout_mode = UI_LAYOUT_MODE_TOP_BOTTOM_FIXED;

    s_current_screen = UI_SCREEN_ENGINE_OIL_INTERVAL;
    s_engine_oil_interval_edit = s_engine_oil_interval_saved;
    s_engine_oil_digit_index = 0u;

    ui_engine_configure_engine_oil_bottom_bar();

    UI_Toast_Hide();
    UI_Popup_Hide();

    s_pressed_mask = Button_GetPressedMask();
    s_force_redraw = 1u;
}

/* -------------------------------------------------------------------------- */
/*  Engine oil digit place value helper                                        */
/*                                                                            */
/*  화면에서 선택 자릿수 index는 왼쪽에서 오른쪽 순서다.                       */
/*  - 0 -> 10000 자리                                                          */
/*  - 1 -> 1000 자리                                                           */
/*  - 2 -> 100 자리                                                            */
/*  - 3 -> 10 자리                                                             */
/*  - 4 -> 1 자리                                                              */
/* -------------------------------------------------------------------------- */
static uint32_t ui_engine_get_engine_oil_place_value(uint8_t digit_index)
{
    static const uint32_t place_table[UI_ENGINE_OIL_DIGIT_COUNT] =
    {
        10000u, 1000u, 100u, 10u, 1u
    };

    if (digit_index >= UI_ENGINE_OIL_DIGIT_COUNT)
    {
        return 1u;
    }

    return place_table[digit_index];
}

/* -------------------------------------------------------------------------- */
/*  Engine oil current digit +/- 1                                             */
/*                                                                            */
/*  현재 선택된 자릿수만 0~9 범위에서 순환시킨다.                              */
/*  다른 자릿수는 절대 건드리지 않는다.                                         */
/*                                                                            */
/*  예) 현재 값이 05400이고, 선택 자릿수가 100 자리라면                         */
/*      +1 -> 05500                                                            */
/*      -1 -> 05300                                                            */
/* -------------------------------------------------------------------------- */
static void ui_engine_adjust_engine_oil_digit(int8_t delta)
{
    uint32_t place_value;
    uint32_t current_digit;
    uint32_t new_digit;

    place_value = ui_engine_get_engine_oil_place_value(s_engine_oil_digit_index);
    current_digit = (s_engine_oil_interval_edit / place_value) % 10u;

    if (delta >= 0)
    {
        new_digit = (current_digit + 1u) % 10u;
    }
    else
    {
        new_digit = (current_digit == 0u) ? 9u : (current_digit - 1u);
    }

    s_engine_oil_interval_edit -= (current_digit * place_value);
    s_engine_oil_interval_edit += (new_digit * place_value);

    if (s_engine_oil_interval_edit > UI_ENGINE_OIL_INTERVAL_MAX)
    {
        s_engine_oil_interval_edit = UI_ENGINE_OIL_INTERVAL_MAX;
    }
}

/* -------------------------------------------------------------------------- */
/*  Legacy debug F1 long-press return                                          */
/*                                                                            */
/*  레거시 디버그 화면에서는 BUTTON1 short press가 원래 기능대로                */
/*  "다음 디버그 페이지" 로 남아 있어야 한다.                                  */
/*                                                                            */
/*  그래서 short press 로직은 debug 파일 내부에 그대로 두고,                   */
/*  여기서는 Button_IsPressed() 기반으로 F1 hold time만 별도로 본다.           */
/*  일정 시간 이상 길게 눌리면 TEST 홈으로 즉시 복귀한다.                      */
/* -------------------------------------------------------------------------- */
static void ui_engine_update_debug_f1_hold_return(uint32_t now_ms)
{
    if (s_current_screen != UI_SCREEN_DEBUG_LEGACY)
    {
        s_debug_f1_hold_start_ms = 0u;
        s_debug_f1_hold_latched = 0u;
        return;
    }

    if (Button_IsPressed(BUTTON_ID_1) == false)
    {
        s_debug_f1_hold_start_ms = 0u;
        s_debug_f1_hold_latched = 0u;
        return;
    }

    if (s_debug_f1_hold_start_ms == 0u)
    {
        s_debug_f1_hold_start_ms = now_ms;
    }

    if ((s_debug_f1_hold_latched == 0u) &&
        ((now_ms - s_debug_f1_hold_start_ms) >= BUTTON_LONG_PRESS_MS))
    {
        s_debug_f1_hold_latched = 1u;
        ui_engine_enter_test_home();
    }
}

/* -------------------------------------------------------------------------- */
/*  20Hz dynamic test update                                                   */
/*                                                                            */
/*  이전 패키지에서는 draw 함수가 raw 20Hz frame parity를 직접 써서             */
/*  체감 blink가 너무 빨랐다.                                                  */
/*                                                                            */
/*  이제는                                                                   */
/*  - 20Hz raw tick -> 큰 숫자 ++ 동적 테스트                                   */
/*  - SlowToggle2Hz -> 눈으로 보는 깜빡임 패널                                 */
/*  로 역할을 분리한다.                                                        */
/* -------------------------------------------------------------------------- */
static void ui_engine_update_test_dynamics(void)
{
    uint32_t tick_now;
    uint32_t delta;

    tick_now = s_ui_tick_20hz;
    delta = (tick_now - s_test_last_processed_tick_20hz);
    if (delta == 0u)
    {
        return;
    }

    s_test_last_processed_tick_20hz = tick_now;

    if (s_test_updates_paused == 0u)
    {
        s_test_live_counter_20hz += delta;
        s_test_blink_phase_on = (SlowToggle2Hz != false);
    }
}

/* -------------------------------------------------------------------------- */
/*  Event dispatch                                                             */
/* -------------------------------------------------------------------------- */
static void ui_engine_handle_test_events(uint32_t now_ms)
{
    button_event_t event;

    while (Button_PopEvent(&event) != false)
    {
        s_pressed_mask = Button_GetPressedMask();

        if ((s_layout_mode == UI_LAYOUT_MODE_TOP_EXTENDED_OVERLAY) &&
            ((event.type == BUTTON_EVENT_PRESS) ||
             (event.type == BUTTON_EVENT_RELEASE) ||
             (event.type == BUTTON_EVENT_SHORT_PRESS) ||
             (event.type == BUTTON_EVENT_LONG_PRESS)))
        {
            ui_engine_request_overlay(now_ms);
        }

        ui_engine_handle_test_button_event(&event, now_ms);

        if (s_current_screen != UI_SCREEN_TEST)
        {
            break;
        }
    }
}

static void ui_engine_handle_test_button_event(const button_event_t *event, uint32_t now_ms)
{
    if (event == 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  TEST 홈 F1 long press -> 엔진 오일 교체 주기 설정 스텁 화면 진입        */
    /*                                                                            */
    /*  short press의 기존 디버그 진입 기능은 그대로 유지해야 하므로,            */
    /*  long press만 먼저 가로채서 별도 화면으로 보낸다.                         */
    /* ---------------------------------------------------------------------- */
    if ((event->id == BUTTON_ID_1) &&
        (event->type == BUTTON_EVENT_LONG_PRESS))
    {
        ui_engine_enter_engine_oil_screen();
        return;
    }

    if (event->type != BUTTON_EVENT_SHORT_PRESS)
    {
        return;
    }

    switch (event->id)
    {
    case BUTTON_ID_1:
        s_current_screen = UI_SCREEN_DEBUG_LEGACY;
        UI_DebugLegacy_Init();
        s_debug_f1_hold_start_ms = 0u;
        s_debug_f1_hold_latched = 0u;
        UI_Toast_Hide();
        UI_Popup_Hide();
        break;

    case BUTTON_ID_2:
        s_test_updates_paused = (s_test_updates_paused == 0u) ? 1u : 0u;
        if (s_test_updates_paused != 0u)
        {
            s_test_blink_phase_on = (SlowToggle2Hz != false);
        }
        ui_engine_configure_test_bottom_bar();
        UI_Toast_Show((s_test_updates_paused != 0u) ? "TEST HOLD" : "TEST RUN",
                      icon_ui_bell_8x8,
                      ICON8_W,
                      ICON8_H,
                      now_ms,
                      900u);
        break;

    case BUTTON_ID_3:
        s_layout_mode = (ui_layout_mode_t)(((uint32_t)s_layout_mode + 1u) % (uint32_t)UI_LAYOUT_MODE_COUNT);
        if (s_layout_mode == UI_LAYOUT_MODE_TOP_EXTENDED_OVERLAY)
        {
            ui_engine_request_overlay(now_ms);
        }
        UI_Toast_Show("LAYOUT CHANGED",
                      icon_ui_folder_8x8,
                      ICON8_W,
                      ICON8_H,
                      now_ms,
                      900u);
        break;

    case BUTTON_ID_4:
        UI_Toast_Show("TOAST WITH ICON",
                      icon_ui_info_8x8,
                      ICON8_W,
                      ICON8_H,
                      now_ms,
                      UI_TOAST_DEFAULT_TIMEOUT_MS);
        break;

    case BUTTON_ID_5:
        UI_Popup_Show("POPUP",
                      "OPAQUE BODY ENABLED",
                      "RIGHT TEXT ALIGN FIXED",
                      icon_ui_warn_8x8,
                      ICON8_W,
                      ICON8_H,
                      now_ms,
                      UI_POPUP_DEFAULT_TIMEOUT_MS);
        break;

    case BUTTON_ID_6:
        s_test_cute_icon_index = (uint8_t)((s_test_cute_icon_index + 1u) & 0x03u);
        UI_Toast_Show("CUTE ICON NEXT",
                      ui_engine_get_cute_icon(s_test_cute_icon_index),
                      ICON8_W,
                      ICON8_H,
                      now_ms,
                      800u);
        break;

    case BUTTON_ID_NONE:
    default:
        break;
    }

    s_force_redraw = 1u;
}


/* -------------------------------------------------------------------------- */
/*  Engine oil stub event dispatch                                             */
/*                                                                            */
/*  이 화면에서는 Button event를 새 루트 화면이 직접 소비한다.                 */
/*  debug 레거시 화면처럼 별도 모듈 내부에서 queue를 먹지 않으므로             */
/*  일반 TEST 홈과 같은 방식으로 여기서 while(pop) 처리한다.                  */
/* -------------------------------------------------------------------------- */
static void ui_engine_handle_engine_oil_events(uint32_t now_ms)
{
    button_event_t event;

    while (Button_PopEvent(&event) != false)
    {
        s_pressed_mask = Button_GetPressedMask();
        ui_engine_handle_engine_oil_button_event(&event, now_ms);

        if (s_current_screen != UI_SCREEN_ENGINE_OIL_INTERVAL)
        {
            break;
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  Engine oil stub button mapping                                             */
/*                                                                            */
/*  short press 매핑                                                           */
/*  - F1 : 저장하지 않고 TEST 홈으로 복귀                                      */
/*  - F2 : 선택 자릿수 왼쪽 이동                                               */
/*  - F3 : 선택 자릿수 오른쪽 이동                                             */
/*  - F4 : 현재 자릿수 값 +1                                                   */
/*  - F5 : 현재 자릿수 값 -1                                                   */
/*  - F6 : 현재 편집값 저장 후 TEST 홈 복귀                                    */
/* -------------------------------------------------------------------------- */
static void ui_engine_handle_engine_oil_button_event(const button_event_t *event, uint32_t now_ms)
{
    if (event == 0)
    {
        return;
    }

    if (event->type != BUTTON_EVENT_SHORT_PRESS)
    {
        return;
    }

    switch (event->id)
    {
    case BUTTON_ID_1:
        s_layout_mode = s_saved_layout_mode_before_oil;
        ui_engine_enter_test_home();
        break;

    case BUTTON_ID_2:
        if (s_engine_oil_digit_index > 0u)
        {
            s_engine_oil_digit_index--;
        }
        s_force_redraw = 1u;
        break;

    case BUTTON_ID_3:
        if (s_engine_oil_digit_index + 1u < UI_ENGINE_OIL_DIGIT_COUNT)
        {
            s_engine_oil_digit_index++;
        }
        s_force_redraw = 1u;
        break;

    case BUTTON_ID_4:
        ui_engine_adjust_engine_oil_digit(+1);
        s_force_redraw = 1u;
        break;

    case BUTTON_ID_5:
        ui_engine_adjust_engine_oil_digit(-1);
        s_force_redraw = 1u;
        break;

    case BUTTON_ID_6:
        s_engine_oil_interval_saved = s_engine_oil_interval_edit;
        s_layout_mode = s_saved_layout_mode_before_oil;
        ui_engine_enter_test_home();
        UI_Toast_Show("OIL INT SAVED",
                      icon_ui_ok_8x8,
                      ICON8_W,
                      ICON8_H,
                      now_ms,
                      900u);
        break;

    case BUTTON_ID_NONE:
    default:
        break;
    }
}


/* -------------------------------------------------------------------------- */
/*  Small icon helper                                                          */
/* -------------------------------------------------------------------------- */
static const uint8_t *ui_engine_get_cute_icon(uint8_t index)
{
    switch (index & 0x03u)
    {
    case 0u: return icon_cute_cat_8x8;
    case 1u: return icon_cute_heart_8x8;
    case 2u: return icon_cute_star_8x8;
    case 3u:
    default: return icon_cute_bike_8x8;
    }
}

/* -------------------------------------------------------------------------- */
/*  Build lightweight status model                                             */
/* -------------------------------------------------------------------------- */
static void ui_engine_build_status_model(ui_statusbar_model_t *out_model)
{
    if (out_model == 0)
    {
        return;
    }

    memset(out_model, 0, sizeof(*out_model));

    out_model->gps_fix = g_app_state.gps.fix;

    out_model->temp_status_flags = g_app_state.ds18b20.status_flags;
    out_model->temp_c_x100 = g_app_state.ds18b20.raw.temp_c_x100;
    out_model->temp_f_x100 = g_app_state.ds18b20.raw.temp_f_x100;
    out_model->temp_last_error = g_app_state.ds18b20.debug.last_error;

    out_model->sd_inserted = g_app_state.sd.detect_stable_present;
    out_model->sd_mounted = g_app_state.sd.mounted;
    out_model->sd_initialized = g_app_state.sd.initialized;

    out_model->time_valid   = g_app_state.clock.rtc_time_valid;
    out_model->time_year    = g_app_state.clock.local.year;
    out_model->time_month   = g_app_state.clock.local.month;
    out_model->time_day     = g_app_state.clock.local.day;
    out_model->time_hour    = g_app_state.clock.local.hour;
    out_model->time_minute  = g_app_state.clock.local.min;
    out_model->time_weekday = g_app_state.clock.local.weekday;

    out_model->record_state = s_record_state;
    out_model->imperial_units = s_imperial_units;
    out_model->bluetooth_stub_state = s_bt_stub_state;
}

/* -------------------------------------------------------------------------- */
/*  Viewport computation                                                       */
/* -------------------------------------------------------------------------- */
static void ui_engine_compute_viewport(u8g2_t *u8g2,
                                       uint32_t now_ms,
                                       ui_rect_t *out_viewport,
                                       bool *out_status_visible,
                                       bool *out_bottom_visible)
{
    bool status_visible = true;
    bool bottom_visible = false;
    ui_rect_t view;
    uint8_t status_reserved_h = UI_StatusBar_GetReservedHeight(u8g2);

    view.x = 0;
    view.y = 0;
    view.w = UI_LCD_W;
    view.h = UI_LCD_H;

    switch (s_layout_mode)
    {
    case UI_LAYOUT_MODE_TOP_EXTENDED_NO_BOTTOM:
        status_visible = true;
        bottom_visible = false;
        view.y = (int16_t)(status_reserved_h + UI_STATUSBAR_GAP_H);
        view.h = (int16_t)(UI_LCD_H - view.y);
        break;

    case UI_LAYOUT_MODE_TOP_EXTENDED_OVERLAY:
        status_visible = true;
        bottom_visible = ((s_pressed_mask != 0u) ||
                          ((int32_t)(s_bottom_overlay_until_ms - now_ms) > 0));
        view.y = (int16_t)(status_reserved_h + UI_STATUSBAR_GAP_H);
        view.h = (int16_t)(UI_LCD_H - view.y);
        break;

    case UI_LAYOUT_MODE_TOP_BOTTOM_FIXED:
        status_visible = true;
        bottom_visible = true;
        view.y = (int16_t)(status_reserved_h + UI_STATUSBAR_GAP_H);
        view.h = (int16_t)(UI_LCD_H - view.y - UI_BOTTOMBAR_H - UI_BOTTOMBAR_GAP_H);
        break;

    case UI_LAYOUT_MODE_FULLSCREEN:
    default:
        status_visible = false;
        bottom_visible = false;
        view.y = 0;
        view.h = UI_LCD_H;
        break;
    }

    /* ---------------------------------------------------------------------- */
    /*  Status-visible mode viewport top expansion                             */
    /*                                                                            */
    /*  요구사항                                                                */
    /*  - status bar가 보이는 모드에서만 본문 뷰포트의 top을 정확히 2px 올린다.  */
    /*  - left / right / bottom 경계는 그대로 유지한다.                         */
    /*                                                                            */
    /*  구현 방법                                                                */
    /*  - y를 2 줄이고                                                          */
    /*  - 같은 양만큼 h를 늘려서                                                */
    /*    bottom edge가 절대로 움직이지 않게 만든다.                           */
    /* ---------------------------------------------------------------------- */
    if (status_visible != false)
    {
        int16_t grow_up_px = (int16_t)UI_VIEWPORT_STATUSBAR_TOP_EXPAND_PX;

        if (view.y < grow_up_px)
        {
            grow_up_px = view.y;
        }

        view.y = (int16_t)(view.y - grow_up_px);
        view.h = (int16_t)(view.h + grow_up_px);
    }

    if (view.h < 0)
    {
        view.h = 0;
    }

    if (out_viewport != 0)
    {
        *out_viewport = view;
    }
    if (out_status_visible != 0)
    {
        *out_status_visible = status_visible;
    }
    if (out_bottom_visible != 0)
    {
        *out_bottom_visible = bottom_visible;
    }
}

/* -------------------------------------------------------------------------- */
/*  Draw root                                                                  */
/* -------------------------------------------------------------------------- */
static void ui_engine_draw_root(u8g2_t *u8g2, uint32_t now_ms)
{
    ui_rect_t viewport;
    bool status_visible;
    bool bottom_visible;
    ui_statusbar_model_t status_model;

    ui_engine_compute_viewport(u8g2, now_ms, &viewport, &status_visible, &bottom_visible);

    /* ---------------------------------------------------------------------- */
    /*  Root content selection                                                 */
    /*                                                                            */
    /*  - TEST 홈이면 기존 TEST renderer를 그대로 호출한다.                    */
    /*  - 엔진 오일 교체 주기 화면이면 새 stub renderer를 호출한다.            */
    /*                                                                            */
    /*  status bar / bottom bar / toast / popup은 그 이후에 공통으로 얹는다.   */
    /* ---------------------------------------------------------------------- */
    if (s_current_screen == UI_SCREEN_ENGINE_OIL_INTERVAL)
    {
        UI_ScreenEngineOil_Draw(u8g2,
                                &viewport,
                                s_engine_oil_interval_edit,
                                s_engine_oil_digit_index);
    }
    else
    {
        UI_ScreenTest_Draw(u8g2,
                           &viewport,
                           s_test_live_counter_20hz,
                           (s_test_updates_paused != 0u),
                           s_test_blink_phase_on,
                           s_test_cute_icon_index,
                           s_layout_mode);
    }

    if (status_visible != false)
    {
        ui_engine_build_status_model(&status_model);
        UI_StatusBar_Draw(u8g2, &status_model, now_ms);
    }

    if (bottom_visible != false)
    {
        UI_BottomBar_Draw(u8g2, s_pressed_mask);
    }

    if (UI_Toast_IsVisible() != false)
    {
        UI_Toast_Draw(u8g2, bottom_visible);
    }

    if (UI_Popup_IsVisible() != false)
    {
        UI_Popup_Draw(u8g2);
    }
}
