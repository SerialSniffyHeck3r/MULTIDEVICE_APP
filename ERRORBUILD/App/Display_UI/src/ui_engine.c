#include "ui_engine.h"

#include "ui_types.h"
#include "ui_statusbar.h"
#include "ui_bottombar.h"
#include "ui_popup.h"
#include "ui_toast.h"
#include "ui_boot.h"
#include "ui_screen_test.h"
#include "ui_screen_engine_oil.h"
#include "ui_debug_legacy.h"
#include "APP_STATE.h"
#include "button.h"
#include "u8g2_uc1608_stm32.h"

#include <string.h>

/* -------------------------------------------------------------------------- */
/*  Public blink variables                                                     */
/*                                                                            */
/*  status bar와 각 화면 파일은 이 blink phase를 읽어서                         */
/*  아이콘 점멸 / 반전 패널 / 경고 표시를 동일 위상으로 맞춘다.                  */
/* -------------------------------------------------------------------------- */
volatile bool SlowToggle2Hz = false;
volatile bool FastToggle5Hz = false;

/* -------------------------------------------------------------------------- */
/*  20Hz source tick                                                           */
/*                                                                            */
/*  TIM7 frame token timer를 재사용한다.                                       */
/*  - raw 20Hz tick count : 화면 내부 live counter 증가용                      */
/*  - slow / fast blink   : 화면 전반 blink phase 토글 생성                    */
/*                                                                            */
/*  이번 정리에서 아주 중요했던 기준                                           */
/*  - 사용자 요구의 "2Hz / 5Hz blink" 는 "완전한 한 주기" 가 아니라          */
/*    "위상이 뒤집히는 토글 이벤트 빈도" 로 해석한다.                         */
/*  - 즉 SlowToggle2Hz 는 1초에 2번만 토글되어야 하므로                        */
/*    ON -> OFF 0.5초 + OFF -> ON 0.5초 = 전체 한 사이클 1.0초가 된다.         */
/*  - FastToggle5Hz 는 1초에 5번 토글되어야 하므로                             */
/*    토글 간격 0.2초, 전체 ON/OFF 한 사이클은 0.4초가 된다.                  */
/*                                                                            */
/*  중요                                                                      */
/*  - 이 파일은 raw tick과 공용 blink phase만 배포한다.                        */
/*  - 어떤 화면이 그 위상을 어떤 그림에 쓸지는 각 화면 파일이 결정한다.       */
/* -------------------------------------------------------------------------- */
#define UI_ENGINE_FRAME_TICK_HZ              20u
#define UI_ENGINE_SLOW_TOGGLE_HZ             2u
#define UI_ENGINE_FAST_TOGGLE_HZ             5u
#define UI_ENGINE_SLOW_TOGGLE_PERIOD_TICKS   (UI_ENGINE_FRAME_TICK_HZ / UI_ENGINE_SLOW_TOGGLE_HZ)
#define UI_ENGINE_FAST_TOGGLE_PERIOD_TICKS   (UI_ENGINE_FRAME_TICK_HZ / UI_ENGINE_FAST_TOGGLE_HZ)

#if ((UI_ENGINE_FRAME_TICK_HZ % UI_ENGINE_SLOW_TOGGLE_HZ) != 0u)
#error "UI_ENGINE_SLOW_TOGGLE_HZ must divide UI_ENGINE_FRAME_TICK_HZ exactly."
#endif

#if ((UI_ENGINE_FRAME_TICK_HZ % UI_ENGINE_FAST_TOGGLE_HZ) != 0u)
#error "UI_ENGINE_FAST_TOGGLE_HZ must divide UI_ENGINE_FRAME_TICK_HZ exactly."
#endif

static volatile uint32_t s_ui_tick_20hz = 0u;
static volatile uint8_t  s_slow_divider = 0u;
static volatile uint8_t  s_fast_divider = 0u;

/* -------------------------------------------------------------------------- */
/*  Engine-global screen selection and status-bar stub state                   */
/*                                                                            */
/*  UI 엔진은                                                                  */
/*  - 현재 어떤 root screen을 합성할지                                         */
/*  - status bar에 공급할 최소 표시 상태                                       */
/*  - 현재 눌린 버튼 bitmask                                                    */
/*  - redraw request flag                                                      */
/*  만 보관한다.                                                               */
/*                                                                            */
/*  TEST 화면의 layout mode / pause 상태 / cute icon index 같은                */
/*  "기능 로직 상태" 는 각 화면 파일로 분리했다.                               */
/* -------------------------------------------------------------------------- */
static ui_screen_id_t            s_current_screen = UI_SCREEN_TEST;
static ui_record_state_t         s_record_state = UI_RECORD_STATE_STOP;
static ui_bluetooth_stub_state_t s_bt_stub_state = UI_BT_STUB_ON;
static uint8_t                   s_imperial_units = 0u;
static uint32_t                  s_pressed_mask = 0u;
static uint8_t                   s_force_redraw = 1u;

/* -------------------------------------------------------------------------- */
/*  Legacy debug screen F1 long-press return tracking                          */
/*                                                                            */
/*  레거시 debug 화면은 내부에서 Button_PopEvent()를 직접 소비하므로,           */
/*  TEST 홈으로 복귀하는 "F1 길게 누름" 은                                     */
/*  event queue가 아니라 현재 안정 눌림 상태(Button_IsPressed) + 시간 누적으로   */
/*  여기서 따로 추적한다.                                                      */
/* -------------------------------------------------------------------------- */
static uint32_t s_debug_f1_hold_start_ms = 0u;
static uint8_t  s_debug_f1_hold_latched = 0u;

/* -------------------------------------------------------------------------- */
/*  Per-frame compose plan                                                     */
/*                                                                            */
/*  draw root는 매 프레임 아래 5가지만 확정한다.                                */
/*  - viewport mode                                                            */
/*  - status bar draw 여부                                                     */
/*  - bottom bar draw 여부                                                     */
/*  - popup draw 여부                                                          */
/*  - toast draw 여부                                                          */
/*                                                                            */
/*  실제 raster draw 순서는                                                    */
/*  main viewport -> status bar -> bottom bar -> toast -> popup               */
/*  로 유지한다.                                                               */
/*  이유는 overlay bottom bar와 기존 toast/popup 겹침 순서를                    */
/*  현재 UI와 완전히 동일하게 유지해야 하기 때문이다.                          */
/* -------------------------------------------------------------------------- */
typedef struct
{
    ui_layout_mode_t layout_mode;
    bool draw_statusbar;
    bool draw_bottombar;
    bool draw_popup;
    bool draw_toast;
} ui_engine_compose_plan_t;

/* -------------------------------------------------------------------------- */
/*  Local helpers                                                              */
/* -------------------------------------------------------------------------- */
static void ui_engine_reset_debug_f1_hold(void);
static void ui_engine_activate_test_screen(void);
static void ui_engine_activate_debug_legacy_screen(void);
static void ui_engine_activate_engine_oil_screen(void);
static void ui_engine_process_test_screen(uint32_t now_ms);
static void ui_engine_process_engine_oil_screen(uint32_t now_ms);
static void ui_engine_process_debug_legacy_screen(uint32_t now_ms);
static void ui_engine_handle_test_screen_action(ui_screen_test_action_t action);
static void ui_engine_handle_engine_oil_screen_action(ui_screen_engine_oil_action_t action);
static void ui_engine_update_debug_f1_hold_return(uint32_t now_ms);
static void ui_engine_build_status_model(ui_statusbar_model_t *out_model);
static void ui_engine_build_compose_plan(uint32_t now_ms,
                                         ui_engine_compose_plan_t *out_plan);
static int16_t ui_engine_get_statusbar_body_top_y(u8g2_t *u8g2,
                                                  bool statusbar_visible);
static int16_t ui_engine_get_fixed_bottom_body_bottom_y(void);
static void ui_engine_compute_viewport(u8g2_t *u8g2,
                                       const ui_engine_compose_plan_t *plan,
                                       ui_rect_t *out_viewport);
static void ui_engine_draw_main_viewport(u8g2_t *u8g2,
                                         const ui_rect_t *viewport);
static void ui_engine_draw_root(u8g2_t *u8g2, uint32_t now_ms);

/* -------------------------------------------------------------------------- */
/*  Small helper: reset debug F1 hold state                                    */
/* -------------------------------------------------------------------------- */
static void ui_engine_reset_debug_f1_hold(void)
{
    s_debug_f1_hold_start_ms = 0u;
    s_debug_f1_hold_latched = 0u;
}

/* -------------------------------------------------------------------------- */
/*  Activate TEST home                                                         */
/*                                                                            */
/*  이 함수는 오직 root screen 전환만 담당한다.                                */
/*  TEST 화면 내부 layout mode / counter / cute icon state는                   */
/*  ui_screen_test.c 안에 그대로 유지된다.                                     */
/* -------------------------------------------------------------------------- */
static void ui_engine_activate_test_screen(void)
{
    s_current_screen = UI_SCREEN_TEST;
    UI_ScreenTest_OnEnter();
    s_pressed_mask = Button_GetPressedMask();
    ui_engine_reset_debug_f1_hold();
    s_force_redraw = 1u;
}

/* -------------------------------------------------------------------------- */
/*  Activate legacy debug screen                                               */
/*                                                                            */
/*  레거시 debug 화면은 현재 엔진 외부의 기존 동작을 최대한 그대로 유지한다.    */
/*  따라서 진입 시 toast / popup은 정리하고, draw 역시 debug 모듈에 맡긴다.    */
/* -------------------------------------------------------------------------- */
static void ui_engine_activate_debug_legacy_screen(void)
{
    s_current_screen = UI_SCREEN_DEBUG_LEGACY;
    UI_DebugLegacy_Init();
    ui_engine_reset_debug_f1_hold();
    UI_Toast_Hide();
    UI_Popup_Hide();
    s_force_redraw = 1u;
}

/* -------------------------------------------------------------------------- */
/*  Activate engine-oil screen                                                 */
/*                                                                            */
/*  엔진 오일 화면 진입 시에는 이전 화면의 toast / popup을 정리한다.            */
/*  이후 bottom bar 내용과 편집 상태 준비는                                   */
/*  ui_screen_engine_oil.c가 맡는다.                                           */
/* -------------------------------------------------------------------------- */
static void ui_engine_activate_engine_oil_screen(void)
{
    s_current_screen = UI_SCREEN_ENGINE_OIL_INTERVAL;
    UI_ScreenEngineOil_OnEnter();
    UI_Toast_Hide();
    UI_Popup_Hide();
    s_pressed_mask = Button_GetPressedMask();
    s_force_redraw = 1u;
}

void UI_Engine_Init(void)
{
    UI_BottomBar_Init();
    UI_Popup_Init();
    UI_Toast_Init();
    UI_DebugLegacy_Init();

    s_current_screen = UI_SCREEN_TEST;
    s_record_state = UI_RECORD_STATE_STOP;
    s_bt_stub_state = UI_BT_STUB_ON;
    s_imperial_units = 0u;
    s_pressed_mask = Button_GetPressedMask();
    s_force_redraw = 1u;

    ui_engine_reset_debug_f1_hold();

    UI_ScreenTest_Init((uint32_t)s_ui_tick_20hz);
    UI_ScreenEngineOil_Init();
    UI_ScreenTest_OnEnter();
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
    s_force_redraw = 1u;
}

void UI_Engine_SetImperialUnits(uint8_t enabled)
{
    s_imperial_units = (enabled != 0u) ? 1u : 0u;
    s_force_redraw = 1u;
}

void UI_Engine_SetBluetoothStubState(uint8_t state)
{
    if (state > (uint8_t)UI_BT_STUB_BLINK)
    {
        state = (uint8_t)UI_BT_STUB_ON;
    }

    s_bt_stub_state = (ui_bluetooth_stub_state_t)state;
    s_force_redraw = 1u;
}

void UI_Engine_OnFrameTickFromISR(void)
{
    /* ---------------------------------------------------------------------- */
    /*  TIM7 20Hz frame tick 누적                                               */
    /*                                                                          */
    /*  이 값은 TEST 화면 등의 live counter가 읽는 원본 tick이다.               */
    /*  blink divider 계산과 별도로 항상 1tick씩 증가한다.                      */
    /* ---------------------------------------------------------------------- */
    s_ui_tick_20hz++;

    /* ---------------------------------------------------------------------- */
    /*  FastToggle5Hz 생성                                                      */
    /*                                                                          */
    /*  요구 의미                                                                */
    /*  - 5Hz는 "1초에 5번 위상 반전" 이다.                                     */
    /*  - 20Hz source tick 기준으로 4tick마다 토글해야 하므로                   */
    /*    토글 간격은 200ms가 된다.                                             */
    /*  - 따라서 보이는 전체 ON/OFF 한 주기는 400ms다.                          */
    /* ---------------------------------------------------------------------- */
    s_fast_divider++;
    if (s_fast_divider >= UI_ENGINE_FAST_TOGGLE_PERIOD_TICKS)
    {
        s_fast_divider = 0u;
        FastToggle5Hz = (FastToggle5Hz == false);
    }

    /* ---------------------------------------------------------------------- */
    /*  SlowToggle2Hz 생성                                                      */
    /*                                                                          */
    /*  요구 의미                                                                */
    /*  - 2Hz는 "1초에 2번 위상 반전" 이다.                                     */
    /*  - 20Hz source tick 기준으로 10tick마다 토글해야 하므로                  */
    /*    토글 간격은 500ms가 된다.                                             */
    /*  - 따라서 보이는 전체 ON/OFF 한 주기는 1초가 된다.                       */
    /* ---------------------------------------------------------------------- */
    s_slow_divider++;
    if (s_slow_divider >= UI_ENGINE_SLOW_TOGGLE_PERIOD_TICKS)
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
        ui_engine_activate_debug_legacy_screen();
    }
}

/* -------------------------------------------------------------------------- */
/*  Process TEST screen                                                        */
/*                                                                            */
/*  UI 엔진은 event queue를 꺼내서 TEST 화면 파일에 넘겨 준다.                 */
/*  - 어떤 toast를 띄울지                                                      */
/*  - bottom bar 문구를 어떻게 바꿀지                                          */
/*  - 어느 화면으로 넘어갈지                                                   */
/*  는 TEST 화면 파일이 결정한다.                                              */
/* -------------------------------------------------------------------------- */
static void ui_engine_process_test_screen(uint32_t now_ms)
{
    button_event_t event;

    s_pressed_mask = Button_GetPressedMask();
    UI_ScreenTest_Task((uint32_t)s_ui_tick_20hz);

    while (Button_PopEvent(&event) != false)
    {
        ui_screen_test_action_t action;

        action = UI_ScreenTest_HandleButtonEvent(&event, now_ms);
        s_force_redraw = 1u;

        if (action != UI_SCREEN_TEST_ACTION_NONE)
        {
            ui_engine_handle_test_screen_action(action);
            break;
        }
    }

    s_pressed_mask = Button_GetPressedMask();
}

/* -------------------------------------------------------------------------- */
/*  Process engine-oil screen                                                  */
/*                                                                            */
/*  이 화면 역시 event queue 소비는 UI 엔진이 하고,                             */
/*  실제 버튼 의미 해석은 화면 파일이 담당한다.                                */
/* -------------------------------------------------------------------------- */
static void ui_engine_process_engine_oil_screen(uint32_t now_ms)
{
    button_event_t event;

    s_pressed_mask = Button_GetPressedMask();

    while (Button_PopEvent(&event) != false)
    {
        ui_screen_engine_oil_action_t action;

        action = UI_ScreenEngineOil_HandleButtonEvent(&event, now_ms);
        s_force_redraw = 1u;

        if (action != UI_SCREEN_ENGINE_OIL_ACTION_NONE)
        {
            ui_engine_handle_engine_oil_screen_action(action);
            break;
        }
    }

    s_pressed_mask = Button_GetPressedMask();
}

/* -------------------------------------------------------------------------- */
/*  Process legacy debug screen                                                */
/*                                                                            */
/*  debug 모듈은 기존처럼 자체 task / 자체 event 소비를 유지한다.               */
/*  여기서는 TEST 홈 복귀용 F1 long press만 별도 추적한다.                     */
/* -------------------------------------------------------------------------- */
static void ui_engine_process_debug_legacy_screen(uint32_t now_ms)
{
    UI_DebugLegacy_Task(now_ms);
    ui_engine_update_debug_f1_hold_return(now_ms);
}

/* -------------------------------------------------------------------------- */
/*  Handle TEST screen action                                                  */
/* -------------------------------------------------------------------------- */
static void ui_engine_handle_test_screen_action(ui_screen_test_action_t action)
{
    switch (action)
    {
    case UI_SCREEN_TEST_ACTION_ENTER_DEBUG_LEGACY:
        ui_engine_activate_debug_legacy_screen();
        break;

    case UI_SCREEN_TEST_ACTION_ENTER_ENGINE_OIL:
        ui_engine_activate_engine_oil_screen();
        break;

    case UI_SCREEN_TEST_ACTION_NONE:
    default:
        break;
    }
}

/* -------------------------------------------------------------------------- */
/*  Handle engine-oil screen action                                            */
/*                                                                            */
/*  저장하고 나가는 경우에는                                                   */
/*  ui_screen_engine_oil.c가 방금 만든 toast를 그대로 살려 둔다.                */
/*  저장하지 않고 나가는 경우에는 popup / toast 모두 정리한다.                 */
/* -------------------------------------------------------------------------- */
static void ui_engine_handle_engine_oil_screen_action(ui_screen_engine_oil_action_t action)
{
    switch (action)
    {
    case UI_SCREEN_ENGINE_OIL_ACTION_BACK_TO_TEST:
        UI_Toast_Hide();
        UI_Popup_Hide();
        ui_engine_activate_test_screen();
        break;

    case UI_SCREEN_ENGINE_OIL_ACTION_SAVE_AND_BACK_TO_TEST:
        UI_Popup_Hide();
        ui_engine_activate_test_screen();
        break;

    case UI_SCREEN_ENGINE_OIL_ACTION_NONE:
    default:
        break;
    }
}

/* -------------------------------------------------------------------------- */
/*  Legacy debug F1 long-press return                                          */
/* -------------------------------------------------------------------------- */
static void ui_engine_update_debug_f1_hold_return(uint32_t now_ms)
{
    if (s_current_screen != UI_SCREEN_DEBUG_LEGACY)
    {
        ui_engine_reset_debug_f1_hold();
        return;
    }

    if (Button_IsPressed(BUTTON_ID_1) == false)
    {
        ui_engine_reset_debug_f1_hold();
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
        UI_Toast_Hide();
        UI_Popup_Hide();
        ui_engine_activate_test_screen();
    }
}

void UI_Engine_Task(uint32_t now_ms)
{
    u8g2_t *u8g2;

    switch (s_current_screen)
    {
    case UI_SCREEN_TEST:
        ui_engine_process_test_screen(now_ms);
        break;

    case UI_SCREEN_ENGINE_OIL_INTERVAL:
        ui_engine_process_engine_oil_screen(now_ms);
        break;

    case UI_SCREEN_DEBUG_LEGACY:
        ui_engine_process_debug_legacy_screen(now_ms);
        break;

    case UI_SCREEN_COUNT:
    default:
        ui_engine_activate_test_screen();
        break;
    }

    UI_Popup_Task(now_ms);
    UI_Toast_Task(now_ms);

    if (U8G2_UC1608_TryAcquireFrameToken() == 0u)
    {
        return;
    }

    if (s_force_redraw != 0u)
    {
        /* 현재 구현은 frame token이 오면 전체 프레임을 다시 그린다. */
        /* 다만 추후 dirty-region 최적화가 들어가더라도 */
        /* public API를 바꾸지 않기 위해 redraw request 플래그는 유지한다. */
    }

    u8g2 = U8G2_UC1608_GetHandle();
    if (u8g2 == 0)
    {
        s_force_redraw = 1u;
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
/*  Build lightweight status model                                             */
/*                                                                            */
/*  status bar는 APP_STATE 전체를 직접 읽지 않고,                              */
/*  화면 표시에 필요한 최소 필드만 담은 경량 model만 받아서 그린다.             */
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
/*  Build per-frame compose plan                                               */
/*                                                                            */
/*  status bar / bottom bar draw flag와 viewport mode는                        */
/*  각 화면 파일의 getter를 통해 받아온다.                                     */
/* -------------------------------------------------------------------------- */
static void ui_engine_build_compose_plan(uint32_t now_ms,
                                         ui_engine_compose_plan_t *out_plan)
{
    if (out_plan == 0)
    {
        return;
    }

    memset(out_plan, 0, sizeof(*out_plan));
    out_plan->layout_mode = UI_LAYOUT_MODE_FULLSCREEN;

    switch (s_current_screen)
    {
    case UI_SCREEN_TEST:
        out_plan->layout_mode = UI_ScreenTest_GetLayoutMode();
        out_plan->draw_statusbar = UI_ScreenTest_IsStatusBarVisible();
        out_plan->draw_bottombar = UI_ScreenTest_IsBottomBarVisible(now_ms, s_pressed_mask);
        break;

    case UI_SCREEN_ENGINE_OIL_INTERVAL:
        out_plan->layout_mode = UI_ScreenEngineOil_GetLayoutMode();
        out_plan->draw_statusbar = UI_ScreenEngineOil_IsStatusBarVisible();
        out_plan->draw_bottombar = UI_ScreenEngineOil_IsBottomBarVisible();
        break;

    case UI_SCREEN_DEBUG_LEGACY:
    case UI_SCREEN_COUNT:
    default:
        break;
    }

    out_plan->draw_popup = UI_Popup_IsVisible();
    out_plan->draw_toast = UI_Toast_IsVisible();
}

/* -------------------------------------------------------------------------- */
/*  Get main-body top Y when status bar is visible                             */
/*                                                                            */
/*  status bar는 절대 건드리지 않는다.                                          */
/*  따라서 main viewport의 top은                                               */
/*  "현재 status bar가 실제로 점유하는 높이" + "그 아래 1px gap"              */
/*  로만 계산한다.                                                             */
/* -------------------------------------------------------------------------- */
static int16_t ui_engine_get_statusbar_body_top_y(u8g2_t *u8g2,
                                                  bool statusbar_visible)
{
    int16_t top_y = 0;

    if (statusbar_visible != false)
    {
        top_y = (int16_t)UI_StatusBar_GetReservedHeight(u8g2);
        top_y = (int16_t)(top_y + UI_STATUSBAR_GAP_H);
    }

    if (top_y < 0)
    {
        top_y = 0;
    }
    if (top_y > UI_LCD_H)
    {
        top_y = UI_LCD_H;
    }

    return top_y;
}

/* -------------------------------------------------------------------------- */
/*  Get fixed-bottom-mode body bottom Y                                        */
/*                                                                            */
/*  TOP+BTM fixed mode에서는 main viewport의 bottom을                          */
/*  bottom gap의 바로 위까지로 제한한다.                                       */
/*                                                                            */
/*  즉, LCD 128px 기준 bottom bar가 8px, gap이 1px라면                         */
/*  main viewport의 마지막 usable Y는                                          */
/*  128 - 8 - 1 = 119 line 바로 위 영역이 된다.                                */
/* -------------------------------------------------------------------------- */
static int16_t ui_engine_get_fixed_bottom_body_bottom_y(void)
{
    int16_t bottom_y;

    bottom_y = (int16_t)(UI_LCD_H - UI_BOTTOMBAR_GAP_H - UI_BOTTOMBAR_H);

    if (bottom_y < 0)
    {
        bottom_y = 0;
    }
    if (bottom_y > UI_LCD_H)
    {
        bottom_y = UI_LCD_H;
    }

    return bottom_y;
}

/* -------------------------------------------------------------------------- */
/*  Compute main viewport                                                      */
/*                                                                            */
/*  정리된 규칙                                                                */
/*  - TOP+BTM fixed      : status 아래 + bottom gap 위                         */
/*  - TOP only           : status 아래부터 LCD 맨 아래까지                      */
/*  - TOP + bottom overlay : TOP only와 같은 viewport                           */
/*  - FULL               : 240x128 전체                                        */
/*                                                                            */
/*  중요                                                                      */
/*  예전 코드처럼 viewport를 1px 위로 억지 확장하지 않는다.                    */
/*  status bar의 실제 배치는 status bar 모듈이 그대로 책임지고,                 */
/*  main viewport는 그 결과를 깨끗하게 받아 계산만 한다.                       */
/* -------------------------------------------------------------------------- */
static void ui_engine_compute_viewport(u8g2_t *u8g2,
                                       const ui_engine_compose_plan_t *plan,
                                       ui_rect_t *out_viewport)
{
    ui_rect_t view;
    int16_t top_y;
    int16_t bottom_y;

    view.x = 0;
    view.y = 0;
    view.w = UI_LCD_W;
    view.h = UI_LCD_H;

    if ((plan == 0) || (out_viewport == 0))
    {
        return;
    }

    top_y = ui_engine_get_statusbar_body_top_y(u8g2, plan->draw_statusbar);

    switch (plan->layout_mode)
    {
    case UI_LAYOUT_MODE_TOP_BOTTOM_FIXED:
        bottom_y = ui_engine_get_fixed_bottom_body_bottom_y();
        view.y = top_y;
        view.h = (int16_t)(bottom_y - top_y);
        break;

    case UI_LAYOUT_MODE_TOP_EXTENDED_NO_BOTTOM:
    case UI_LAYOUT_MODE_TOP_EXTENDED_OVERLAY:
        view.y = top_y;
        view.h = (int16_t)(UI_LCD_H - top_y);
        break;

    case UI_LAYOUT_MODE_FULLSCREEN:
    case UI_LAYOUT_MODE_COUNT:
    default:
        view.y = 0;
        view.h = UI_LCD_H;
        break;
    }

    if (view.y < 0)
    {
        view.y = 0;
    }
    if (view.y > UI_LCD_H)
    {
        view.y = UI_LCD_H;
    }
    if (view.h < 0)
    {
        view.h = 0;
    }
    if ((view.y + view.h) > UI_LCD_H)
    {
        view.h = (int16_t)(UI_LCD_H - view.y);
    }

    *out_viewport = view;
}

/* -------------------------------------------------------------------------- */
/*  Draw current main viewport                                                 */
/*                                                                            */
/*  여기서는 오직 본문 renderer만 호출한다.                                    */
/*  status bar / bottom bar / popup / toast는 여기서 그리지 않는다.            */
/* -------------------------------------------------------------------------- */
static void ui_engine_draw_main_viewport(u8g2_t *u8g2,
                                         const ui_rect_t *viewport)
{
    if ((u8g2 == 0) || (viewport == 0))
    {
        return;
    }

    switch (s_current_screen)
    {
    case UI_SCREEN_TEST:
        UI_ScreenTest_Draw(u8g2,
                           viewport,
                           UI_ScreenTest_GetLiveCounter20Hz(),
                           UI_ScreenTest_GetUpdatesPaused(),
                           UI_ScreenTest_GetBlinkPhase(),
                           UI_ScreenTest_GetCuteIconIndex(),
                           UI_ScreenTest_GetLayoutMode());
        break;

    case UI_SCREEN_ENGINE_OIL_INTERVAL:
        UI_ScreenEngineOil_Draw(u8g2,
                                viewport,
                                UI_ScreenEngineOil_GetIntervalValue(),
                                UI_ScreenEngineOil_GetSelectedDigitIndex());
        break;

    case UI_SCREEN_DEBUG_LEGACY:
    case UI_SCREEN_COUNT:
    default:
        break;
    }
}

/* -------------------------------------------------------------------------- */
/*  Draw root                                                                  */
/*                                                                            */
/*  논리적 compose plan은 status/bottom/main/popup/toast 순으로 분리해도,      */
/*  실제 raster draw는 현재 UI 픽셀 결과를 보존하기 위해                       */
/*  main -> status -> bottom -> toast -> popup 순으로 넣는다.                  */
/*                                                                            */
/*  이렇게 해야                                                                */
/*  - overlay bottom bar가 main viewport 위에 정확히 얹히고                    */
/*  - popup / toast가 동시에 살아 있을 때도 현재와 같은 겹침 순서               */
/*    (popup이 더 위) 를 유지한다.                                             */
/* -------------------------------------------------------------------------- */
static void ui_engine_draw_root(u8g2_t *u8g2, uint32_t now_ms)
{
    ui_engine_compose_plan_t plan;
    ui_rect_t viewport;
    ui_statusbar_model_t status_model;

    if (u8g2 == 0)
    {
        return;
    }

    ui_engine_build_compose_plan(now_ms, &plan);
    ui_engine_compute_viewport(u8g2, &plan, &viewport);

    ui_engine_draw_main_viewport(u8g2, &viewport);

    if (plan.draw_statusbar != false)
    {
        ui_engine_build_status_model(&status_model);
        UI_StatusBar_Draw(u8g2, &status_model, now_ms);
    }

    if (plan.draw_bottombar != false)
    {
        UI_BottomBar_Draw(u8g2, s_pressed_mask);
    }

    if (plan.draw_toast != false)
    {
        UI_Toast_Draw(u8g2, plan.draw_bottombar);
    }

    if (plan.draw_popup != false)
    {
        UI_Popup_Draw(u8g2);
    }
}
