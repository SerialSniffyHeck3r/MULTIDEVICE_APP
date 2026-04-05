#include "ui_engine.h"

#include "ui_types.h"
#include "ui_statusbar.h"
#include "ui_bottombar.h"
#include "ui_popup.h"
#include "ui_toast.h"
#include "ui_menu.h"
#include "ui_confirm.h"
#include "ui_boot.h"
#include "ui_screen_test.h"
#include "ui_screen_engine_oil.h"
#include "ui_screen_gps.h"
#include "ui_screen_vario.h"
#include "ui_debug_legacy.h"

#include "Vario_State.h"

#include "APP_STATE.h"
#include "button.h"
#include "u8g2_uc1608_stm32.h"

#include <stdbool.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/* Public blink variables                                                      */
/*                                                                            */
/* status bar / 각 root screen / legacy debug 는 이 global phase 를 읽어서       */
/* blink 위상을 통일한다.                                                       */
/* -------------------------------------------------------------------------- */
volatile bool SlowToggle2Hz = false;
volatile bool FastToggle5Hz = false;

/* -------------------------------------------------------------------------- */
/* 20Hz source tick / blink divider                                            */
/* -------------------------------------------------------------------------- */
#define UI_ENGINE_FRAME_TICK_HZ              20u
#define UI_ENGINE_SLOW_TOGGLE_HZ              2u
#define UI_ENGINE_FAST_TOGGLE_HZ              5u
#define UI_ENGINE_SLOW_TOGGLE_PERIOD_TICKS   (UI_ENGINE_FRAME_TICK_HZ / UI_ENGINE_SLOW_TOGGLE_HZ)
#define UI_ENGINE_FAST_TOGGLE_PERIOD_TICKS   (UI_ENGINE_FRAME_TICK_HZ / UI_ENGINE_FAST_TOGGLE_HZ)

#if ((UI_ENGINE_FRAME_TICK_HZ % UI_ENGINE_SLOW_TOGGLE_HZ) != 0u)
#error "UI_ENGINE_SLOW_TOGGLE_HZ must divide UI_ENGINE_FRAME_TICK_HZ exactly."
#endif

#if ((UI_ENGINE_FRAME_TICK_HZ % UI_ENGINE_FAST_TOGGLE_HZ) != 0u)
#error "UI_ENGINE_FAST_TOGGLE_HZ must divide UI_ENGINE_FRAME_TICK_HZ exactly."
#endif

static volatile uint32_t s_ui_tick_20hz = 0u;
static volatile uint8_t s_slow_divider = 0u;
static volatile uint8_t s_fast_divider = 0u;

/* -------------------------------------------------------------------------- */
/* Engine-global selection / stub state                                        */
/* -------------------------------------------------------------------------- */
static ui_engine_boot_mode_t s_boot_mode = UI_ENGINE_BOOT_MODE_VARIO;
static ui_screen_id_t s_current_screen = UI_SCREEN_TEST;
static ui_record_state_t s_record_state = UI_RECORD_STATE_STOP;
static ui_bluetooth_stub_state_t s_bt_stub_state = UI_BT_STUB_ON;
static uint8_t s_imperial_units = 0u;
static uint32_t s_pressed_mask = 0u;
static uint8_t s_force_redraw = 1u;
static ui_screen_id_t s_return_screen_from_gps = UI_SCREEN_TEST;

/* -------------------------------------------------------------------------- */
/* Last drawn frame snapshot                                                   */
/*                                                                            */
/* redraw gate 가 도입된 뒤에는 "직전 실제로 그린 프레임" 과 현재 상태를 비교해  */
/* draw 필요성을 판정해야 한다.                                                */
/* -------------------------------------------------------------------------- */
static uint8_t s_last_draw_valid = 0u;
static ui_screen_id_t s_last_draw_screen = UI_SCREEN_COUNT;
static uint32_t s_last_draw_pressed_mask = 0u;
static ui_statusbar_model_t s_last_draw_status_model;
static bool s_last_draw_fast_toggle = false;
static bool s_last_draw_slow_toggle = false;

/* -------------------------------------------------------------------------- */
/* Legacy debug F1 long-press return tracking                                  */
/* -------------------------------------------------------------------------- */
static uint32_t s_debug_f1_hold_start_ms = 0u;
static uint8_t s_debug_f1_hold_latched = 0u;

/* -------------------------------------------------------------------------- */
/* Per-frame compose plan                                                      */
/* -------------------------------------------------------------------------- */
typedef struct
{
    ui_layout_mode_t layout_mode;
    bool draw_statusbar;
    bool draw_bottombar;
    bool draw_popup;
    bool draw_toast;
    bool draw_menu;
    bool draw_confirm;
} ui_engine_compose_plan_t;

/* -------------------------------------------------------------------------- */
/* Local helper declarations                                                   */
/* -------------------------------------------------------------------------- */
static ui_screen_id_t ui_engine_get_default_screen_for_boot_mode(void);
static void ui_engine_reset_debug_f1_hold(void);
static void ui_engine_activate_boot_default_screen(void);
static void ui_engine_activate_vario_screen(void);
static void ui_engine_activate_test_screen(void);
static void ui_engine_activate_debug_legacy_screen(void);
static void ui_engine_activate_engine_oil_screen(void);
static void ui_engine_activate_gps_screen(ui_screen_id_t return_screen);
static void ui_engine_return_from_gps(void);

static void ui_engine_process_vario_screen(uint32_t now_ms);
static void ui_engine_process_test_screen(uint32_t now_ms);
static void ui_engine_process_gps_screen(uint32_t now_ms);
static void ui_engine_process_engine_oil_screen(uint32_t now_ms);
static void ui_engine_process_debug_legacy_screen(uint32_t now_ms);

static void ui_engine_handle_test_screen_action(ui_screen_test_action_t action);
static void ui_engine_handle_engine_oil_screen_action(ui_screen_engine_oil_action_t action);
static void ui_engine_handle_gps_screen_action(ui_screen_gps_action_t action);
static void ui_engine_update_debug_f1_hold_return(uint32_t now_ms);

static void ui_engine_build_status_model(ui_statusbar_model_t *out_model);
static bool ui_engine_status_model_changed(const ui_statusbar_model_t *status_model);
static bool ui_engine_should_draw(const ui_engine_compose_plan_t *plan, const ui_statusbar_model_t *status_model);
static void ui_engine_capture_draw_snapshot(const ui_engine_compose_plan_t *plan, const ui_statusbar_model_t *status_model);
static void ui_engine_build_compose_plan(uint32_t now_ms, ui_engine_compose_plan_t *out_plan);
static int16_t ui_engine_get_statusbar_body_top_y(u8g2_t *u8g2, bool statusbar_visible);
static int16_t ui_engine_get_fixed_bottom_body_bottom_y(void);
static void ui_engine_compute_viewport(u8g2_t *u8g2,
                                       const ui_engine_compose_plan_t *plan,
                                       ui_rect_t *out_viewport);
static void ui_engine_draw_main_viewport(u8g2_t *u8g2, const ui_rect_t *viewport);
static void ui_engine_draw_root(u8g2_t *u8g2, uint32_t now_ms);

/* -------------------------------------------------------------------------- */
/* helper: default screen for current boot profile                             */
/* -------------------------------------------------------------------------- */
static ui_screen_id_t ui_engine_get_default_screen_for_boot_mode(void)
{
    if (s_boot_mode == UI_ENGINE_BOOT_MODE_LEGACY)
    {
        return UI_SCREEN_TEST;
    }

    return UI_SCREEN_VARIO;
}

/* -------------------------------------------------------------------------- */
/* helper: reset legacy debug F1 hold                                          */
/* -------------------------------------------------------------------------- */
static void ui_engine_reset_debug_f1_hold(void)
{
    s_debug_f1_hold_start_ms = 0u;
    s_debug_f1_hold_latched = 0u;
}

/* -------------------------------------------------------------------------- */
/* Activate current boot profile default root                                  */
/* -------------------------------------------------------------------------- */
static void ui_engine_activate_boot_default_screen(void)
{
    if (s_boot_mode == UI_ENGINE_BOOT_MODE_LEGACY)
    {
        ui_engine_activate_test_screen();
    }
    else
    {
        ui_engine_activate_vario_screen();
    }
}

/* -------------------------------------------------------------------------- */
/* Activate VARIO root                                                         */
/*                                                                            */
/* normal boot profile 의 실제 runtime root screen 이다.                        */
/* -------------------------------------------------------------------------- */
static void ui_engine_activate_vario_screen(void)
{
    s_current_screen = UI_SCREEN_VARIO;
    UI_ScreenVario_OnEnter();
    UI_Toast_Hide();
    UI_Popup_Hide();
    UI_Menu_Hide();
    UI_Confirm_Hide();
    s_pressed_mask = Button_GetPressedMask();
    ui_engine_reset_debug_f1_hold();
    s_force_redraw = 1u;
}

/* -------------------------------------------------------------------------- */
/* Activate TEST home                                                          */
/*                                                                            */
/* normal VARIO boot profile 에서는 legacy TEST screen 을 절대 활성화하지 않는다.*/
/* stray call 이 들어오면 곧바로 VARIO root 로 되돌린다.                        */
/* -------------------------------------------------------------------------- */
static void ui_engine_activate_test_screen(void)
{
    if (s_boot_mode != UI_ENGINE_BOOT_MODE_LEGACY)
    {
        ui_engine_activate_vario_screen();
        return;
    }

    s_current_screen = UI_SCREEN_TEST;
    UI_ScreenTest_OnEnter();
    UI_Toast_Hide();
    UI_Popup_Hide();
    UI_Menu_Hide();
    UI_Confirm_Hide();
    s_pressed_mask = Button_GetPressedMask();
    ui_engine_reset_debug_f1_hold();
    s_force_redraw = 1u;
}

/* -------------------------------------------------------------------------- */
/* Activate legacy debug screen                                                */
/*                                                                            */
/* normal VARIO boot profile 에서는 보드 debug button 으로도 진입하지 못하게     */
/* 막는다.                                                                     */
/* -------------------------------------------------------------------------- */
static void ui_engine_activate_debug_legacy_screen(void)
{
    if (s_boot_mode != UI_ENGINE_BOOT_MODE_LEGACY)
    {
        return;
    }

    s_current_screen = UI_SCREEN_DEBUG_LEGACY;
    UI_DebugLegacy_Init();
    ui_engine_reset_debug_f1_hold();
    UI_Toast_Hide();
    UI_Popup_Hide();
    UI_Menu_Hide();
    UI_Confirm_Hide();
    s_force_redraw = 1u;
}

/* -------------------------------------------------------------------------- */
/* Activate engine-oil screen                                                  */
/*                                                                            */
/* legacy TEST family 전용.                                                    */
/* -------------------------------------------------------------------------- */
static void ui_engine_activate_engine_oil_screen(void)
{
    if (s_boot_mode != UI_ENGINE_BOOT_MODE_LEGACY)
    {
        return;
    }

    s_current_screen = UI_SCREEN_ENGINE_OIL_INTERVAL;
    UI_ScreenEngineOil_OnEnter();
    UI_Toast_Hide();
    UI_Popup_Hide();
    UI_Menu_Hide();
    UI_Confirm_Hide();
    s_pressed_mask = Button_GetPressedMask();
    s_force_redraw = 1u;
}

/* -------------------------------------------------------------------------- */
/* Activate GPS screen                                                         */
/*                                                                            */
/* legacy TEST family 전용.                                                    */
/* -------------------------------------------------------------------------- */
static void ui_engine_activate_gps_screen(ui_screen_id_t return_screen)
{
    if (s_boot_mode != UI_ENGINE_BOOT_MODE_LEGACY)
    {
        return;
    }

    if (return_screen == UI_SCREEN_GPS)
    {
        return_screen = ui_engine_get_default_screen_for_boot_mode();
    }

    s_return_screen_from_gps = return_screen;
    s_current_screen = UI_SCREEN_GPS;
    UI_ScreenGps_OnEnter();
    UI_Toast_Hide();
    UI_Popup_Hide();
    UI_Menu_Hide();
    UI_Confirm_Hide();
    s_pressed_mask = Button_GetPressedMask();
    s_force_redraw = 1u;
}

/* -------------------------------------------------------------------------- */
/* Return from GPS screen                                                      */
/* -------------------------------------------------------------------------- */
static void ui_engine_return_from_gps(void)
{
    switch (s_return_screen_from_gps)
    {
    case UI_SCREEN_ENGINE_OIL_INTERVAL:
        if (s_boot_mode == UI_ENGINE_BOOT_MODE_LEGACY)
        {
            s_current_screen = UI_SCREEN_ENGINE_OIL_INTERVAL;
            UI_ScreenEngineOil_OnResume();
            s_pressed_mask = Button_GetPressedMask();
            s_force_redraw = 1u;
        }
        else
        {
            ui_engine_activate_vario_screen();
        }
        break;

    case UI_SCREEN_DEBUG_LEGACY:
        ui_engine_activate_debug_legacy_screen();
        break;

    case UI_SCREEN_VARIO:
        ui_engine_activate_vario_screen();
        break;

    case UI_SCREEN_TEST:
    case UI_SCREEN_GPS:
    case UI_SCREEN_COUNT:
    default:
        ui_engine_activate_boot_default_screen();
        break;
    }
}

/* -------------------------------------------------------------------------- */
/* Public: boot mode setter                                                    */
/* -------------------------------------------------------------------------- */
void UI_Engine_SetBootMode(ui_engine_boot_mode_t mode)
{
    if ((mode != UI_ENGINE_BOOT_MODE_VARIO) &&
        (mode != UI_ENGINE_BOOT_MODE_LEGACY))
    {
        mode = UI_ENGINE_BOOT_MODE_VARIO;
    }

    s_boot_mode = mode;
}

ui_engine_boot_mode_t UI_Engine_GetBootMode(void)
{
    return s_boot_mode;
}

uint8_t UI_Engine_IsLegacyBootMode(void)
{
    return (s_boot_mode == UI_ENGINE_BOOT_MODE_LEGACY) ? 1u : 0u;
}

void UI_Engine_Init(void)
{
    UI_BottomBar_Init();
    UI_Popup_Init();
    UI_Toast_Init();
    UI_Menu_Init();
    UI_Confirm_Init();
    UI_DebugLegacy_Init();

    s_record_state = UI_RECORD_STATE_STOP;
    s_bt_stub_state = UI_BT_STUB_ON;
    s_imperial_units = 0u;
    s_pressed_mask = Button_GetPressedMask();
    s_force_redraw = 1u;
    s_return_screen_from_gps = ui_engine_get_default_screen_for_boot_mode();
    s_last_draw_valid = 0u;
    s_last_draw_screen = UI_SCREEN_COUNT;
    s_last_draw_pressed_mask = 0u;
    memset(&s_last_draw_status_model, 0, sizeof(s_last_draw_status_model));
    s_last_draw_fast_toggle = false;
    s_last_draw_slow_toggle = false;

    ui_engine_reset_debug_f1_hold();

    /* legacy root family */
    UI_ScreenTest_Init((uint32_t)s_ui_tick_20hz);
    UI_ScreenEngineOil_Init();
    UI_ScreenGps_Init();

    /* normal VARIO root family */
    UI_ScreenVario_Init();

    s_current_screen = ui_engine_get_default_screen_for_boot_mode();

    if (s_current_screen == UI_SCREEN_VARIO)
    {
        UI_ScreenVario_OnEnter();
    }
    else
    {
        UI_ScreenTest_OnEnter();
    }
}

void UI_Engine_EarlyBootDraw(void)
{
    u8g2_t *u8g2 = U8G2_UC1608_GetHandle();

    if (u8g2 == NULL)
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

void UI_Engine_EnterGpsScreen(void)
{
    if (s_boot_mode != UI_ENGINE_BOOT_MODE_LEGACY)
    {
        return;
    }

    ui_engine_activate_gps_screen(s_current_screen);
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
    /* raw 20Hz tick */
    s_ui_tick_20hz++;

    /* 5Hz toggle phase */
    s_fast_divider++;
    if (s_fast_divider >= UI_ENGINE_FAST_TOGGLE_PERIOD_TICKS)
    {
        s_fast_divider = 0u;
        FastToggle5Hz = (FastToggle5Hz == false);
    }

    /* 2Hz toggle phase */
    s_slow_divider++;
    if (s_slow_divider >= UI_ENGINE_SLOW_TOGGLE_PERIOD_TICKS)
    {
        s_slow_divider = 0u;
        SlowToggle2Hz = (SlowToggle2Hz == false);
    }
}

void UI_Engine_OnBoardDebugButtonIrq(uint32_t now_ms)
{
    /* ---------------------------------------------------------------------- */
    /* normal VARIO boot profile 에서는 legacy debug 진입을 봉인한다.           */
    /* ---------------------------------------------------------------------- */
    if (s_boot_mode != UI_ENGINE_BOOT_MODE_LEGACY)
    {
        (void)now_ms;
        return;
    }

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
/* Process VARIO root                                                          */
/*                                                                            */
/* 중요                                                                         */
/* - VARIO mode 의 버튼 이벤트 소비는 여기서 하지 않는다.                       */
/* - 이벤트는 Vario_Task.c 의 상위 상태머신에서 먼저 소비한다.                  */
/* - UI engine 은 pressed mask 갱신과 root compose scheduling 만 맡는다.       */
/* -------------------------------------------------------------------------- */
static void ui_engine_process_vario_screen(uint32_t now_ms)
{
    s_pressed_mask = Button_GetPressedMask();
    UI_ScreenVario_Task(now_ms);
    s_pressed_mask = Button_GetPressedMask();

    /* ---------------------------------------------------------------------- */
    /* VARIO draw cadence ownership                                            */
    /*                                                                          */
    /* - fast / slow presentation 주기는 Vario_State_Task() 가 관리한다.       */
    /* - mode 변경, 설정 이동, publish 5Hz 갱신 등은                           */
    /*   Vario_State_RequestRedraw() 경로로 올라온다.                          */
    /* - 여기서는 pressed-mask 동기화와 screen bridge task 만 수행한다.         */
    /* ---------------------------------------------------------------------- */
    (void)now_ms;
}

/* -------------------------------------------------------------------------- */
/* Process TEST screen                                                         */
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
/* Process engine-oil screen                                                   */
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
/* Process GPS screen                                                          */
/* -------------------------------------------------------------------------- */
static void ui_engine_process_gps_screen(uint32_t now_ms)
{
    button_event_t event;

    s_pressed_mask = Button_GetPressedMask();

    while (Button_PopEvent(&event) != false)
    {
        ui_screen_gps_action_t action;

        action = UI_ScreenGps_HandleButtonEvent(&event, now_ms);
        s_force_redraw = 1u;

        if (action != UI_SCREEN_GPS_ACTION_NONE)
        {
            ui_engine_handle_gps_screen_action(action);
            break;
        }
    }

    s_pressed_mask = Button_GetPressedMask();
}

/* -------------------------------------------------------------------------- */
/* Process legacy debug screen                                                 */
/* -------------------------------------------------------------------------- */
static void ui_engine_process_debug_legacy_screen(uint32_t now_ms)
{
    UI_DebugLegacy_Task(now_ms);
    ui_engine_update_debug_f1_hold_return(now_ms);
}

/* -------------------------------------------------------------------------- */
/* Handle TEST action                                                          */
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

    case UI_SCREEN_TEST_ACTION_ENTER_GPS:
        UI_Engine_EnterGpsScreen();
        break;

    case UI_SCREEN_TEST_ACTION_NONE:
    default:
        break;
    }
}

/* -------------------------------------------------------------------------- */
/* Handle engine-oil action                                                    */
/* -------------------------------------------------------------------------- */
static void ui_engine_handle_engine_oil_screen_action(ui_screen_engine_oil_action_t action)
{
    switch (action)
    {
    case UI_SCREEN_ENGINE_OIL_ACTION_BACK_TO_TEST:
        UI_Toast_Hide();
        UI_Popup_Hide();
        UI_Menu_Hide();
        UI_Confirm_Hide();
        ui_engine_activate_test_screen();
        break;

    case UI_SCREEN_ENGINE_OIL_ACTION_SAVE_AND_BACK_TO_TEST:
        UI_Popup_Hide();
        UI_Menu_Hide();
        UI_Confirm_Hide();
        ui_engine_activate_test_screen();
        break;

    case UI_SCREEN_ENGINE_OIL_ACTION_NONE:
    default:
        break;
    }
}

/* -------------------------------------------------------------------------- */
/* Handle GPS action                                                           */
/* -------------------------------------------------------------------------- */
static void ui_engine_handle_gps_screen_action(ui_screen_gps_action_t action)
{
    switch (action)
    {
    case UI_SCREEN_GPS_ACTION_BACK_TO_PREVIOUS:
        UI_Toast_Hide();
        UI_Popup_Hide();
        UI_Menu_Hide();
        UI_Confirm_Hide();
        ui_engine_return_from_gps();
        break;

    case UI_SCREEN_GPS_ACTION_NONE:
    default:
        break;
    }
}

/* -------------------------------------------------------------------------- */
/* Legacy debug F1 long-press return                                           */
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
        UI_Menu_Hide();
        UI_Confirm_Hide();
        ui_engine_activate_test_screen();
    }
}


static bool ui_engine_status_model_changed(const ui_statusbar_model_t *status_model)
{
    if (status_model == NULL)
    {
        return false;
    }

    if (s_last_draw_valid == 0u)
    {
        return true;
    }

    if (memcmp(&s_last_draw_status_model, status_model, sizeof(*status_model)) != 0)
    {
        return true;
    }

    return false;
}

static bool ui_engine_should_draw(const ui_engine_compose_plan_t *plan,
                                  const ui_statusbar_model_t *status_model)
{
    if (plan == NULL)
    {
        return false;
    }

    if (s_force_redraw != 0u)
    {
        return true;
    }

    if (s_last_draw_valid == 0u)
    {
        return true;
    }

    if (s_current_screen != s_last_draw_screen)
    {
        return true;
    }

    if ((plan->draw_bottombar != false) && (s_pressed_mask != s_last_draw_pressed_mask))
    {
        return true;
    }

    if ((plan->draw_statusbar != false) && (ui_engine_status_model_changed(status_model) != false))
    {
        return true;
    }

    /* ---------------------------------------------------------------------- */
    /* blink phase 는 status bar / legacy root screen 들이 공유하는 전역 위상. */
    /* 따라서 위상이 바뀌면 다음 프레임 redraw 후보가 된다.                    */
    /* ---------------------------------------------------------------------- */
    /* ---------------------------------------------------------------------- */
    /* legacy root family 는 20Hz 기반 blink phase 를 화면 갱신 이유로 쓴다.   */
    /*                                                                          */
    /* normal VARIO profile 에서는 이 위상 변화만으로 redraw 를 일으키지 않는다.*/
    /* 그렇지 않으면 상태 publish/redraw cadence 와 무관하게                    */
    /* 5Hz / 2Hz redraw 가 다시 살아나기 때문이다.                             */
    /* ---------------------------------------------------------------------- */
    if ((s_current_screen != UI_SCREEN_VARIO) &&
        ((FastToggle5Hz != s_last_draw_fast_toggle) ||
         (SlowToggle2Hz != s_last_draw_slow_toggle)))
    {
        return true;
    }

    if ((s_current_screen == UI_SCREEN_VARIO) &&
        (Vario_State_IsRedrawRequested() != false))
    {
        return true;
    }

    return false;
}

static void ui_engine_capture_draw_snapshot(const ui_engine_compose_plan_t *plan,
                                            const ui_statusbar_model_t *status_model)
{
    if (plan == NULL)
    {
        return;
    }

    s_last_draw_valid = 1u;
    s_last_draw_screen = s_current_screen;
    s_last_draw_pressed_mask = s_pressed_mask;
    s_last_draw_fast_toggle = FastToggle5Hz;
    s_last_draw_slow_toggle = SlowToggle2Hz;

    if ((plan->draw_statusbar != false) && (status_model != NULL))
    {
        s_last_draw_status_model = *status_model;
    }
    else
    {
        memset(&s_last_draw_status_model, 0, sizeof(s_last_draw_status_model));
    }
}

void UI_Engine_Task(uint32_t now_ms)
{
    u8g2_t *u8g2;
    ui_engine_compose_plan_t plan;
    ui_statusbar_model_t status_model;
    bool popup_visible_before;
    bool toast_visible_before;
    bool menu_visible_before;
    bool confirm_visible_before;

    switch (s_current_screen)
    {
    case UI_SCREEN_VARIO:
        ui_engine_process_vario_screen(now_ms);
        break;

    case UI_SCREEN_TEST:
        ui_engine_process_test_screen(now_ms);
        break;

    case UI_SCREEN_ENGINE_OIL_INTERVAL:
        ui_engine_process_engine_oil_screen(now_ms);
        break;

    case UI_SCREEN_GPS:
        ui_engine_process_gps_screen(now_ms);
        break;

    case UI_SCREEN_DEBUG_LEGACY:
        ui_engine_process_debug_legacy_screen(now_ms);
        break;

    case UI_SCREEN_COUNT:
    default:
        ui_engine_activate_boot_default_screen();
        break;
    }

    popup_visible_before = UI_Popup_IsVisible();
    toast_visible_before = UI_Toast_IsVisible();
    menu_visible_before = UI_Menu_IsVisible();
    confirm_visible_before = UI_Confirm_IsVisible();

    UI_Popup_Task(now_ms);
    UI_Toast_Task(now_ms);

    if (popup_visible_before != UI_Popup_IsVisible())
    {
        s_force_redraw = 1u;
    }
    if (toast_visible_before != UI_Toast_IsVisible())
    {
        s_force_redraw = 1u;
    }
    if (menu_visible_before != UI_Menu_IsVisible())
    {
        s_force_redraw = 1u;
    }
    if (confirm_visible_before != UI_Confirm_IsVisible())
    {
        s_force_redraw = 1u;
    }

    memset(&plan, 0, sizeof(plan));
    ui_engine_build_compose_plan(now_ms, &plan);

    memset(&status_model, 0, sizeof(status_model));
    if (plan.draw_statusbar != false)
    {
        ui_engine_build_status_model(&status_model);
    }

    if (ui_engine_should_draw(&plan, &status_model) == false)
    {
        return;
    }

    if (U8G2_UC1608_TryAcquireFrameToken() == 0u)
    {
        return;
    }

    u8g2 = U8G2_UC1608_GetHandle();
    if (u8g2 == NULL)
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
    ui_engine_capture_draw_snapshot(&plan, &status_model);
    s_force_redraw = 0u;
}

/* -------------------------------------------------------------------------- */
/* Build status model                                                          */
/* -------------------------------------------------------------------------- */
static void ui_engine_build_status_model(ui_statusbar_model_t *out_model)
{
    if (out_model == NULL)
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

    out_model->time_valid = g_app_state.clock.rtc_time_valid;
    out_model->time_year = g_app_state.clock.local.year;
    out_model->time_month = g_app_state.clock.local.month;
    out_model->time_day = g_app_state.clock.local.day;
    out_model->time_hour = g_app_state.clock.local.hour;
    out_model->time_minute = g_app_state.clock.local.min;
    out_model->time_weekday = g_app_state.clock.local.weekday;

    out_model->record_state = s_record_state;
    out_model->imperial_units = s_imperial_units;
    out_model->bluetooth_stub_state = s_bt_stub_state;
}

/* -------------------------------------------------------------------------- */
/* Build compose plan                                                          */
/* -------------------------------------------------------------------------- */
static void ui_engine_build_compose_plan(uint32_t now_ms, ui_engine_compose_plan_t *out_plan)
{
    if (out_plan == NULL)
    {
        return;
    }

    memset(out_plan, 0, sizeof(*out_plan));
    out_plan->layout_mode = UI_LAYOUT_MODE_FULLSCREEN;

    switch (s_current_screen)
    {
    case UI_SCREEN_VARIO:
        out_plan->layout_mode = UI_ScreenVario_GetLayoutMode();
        out_plan->draw_statusbar = UI_ScreenVario_IsStatusBarVisible();
        out_plan->draw_bottombar = UI_ScreenVario_IsBottomBarVisible();
        break;

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

    case UI_SCREEN_GPS:
        out_plan->layout_mode = UI_ScreenGps_GetLayoutMode();
        out_plan->draw_statusbar = UI_ScreenGps_IsStatusBarVisible();
        out_plan->draw_bottombar = UI_ScreenGps_IsBottomBarVisible();
        break;

    case UI_SCREEN_DEBUG_LEGACY:
    case UI_SCREEN_COUNT:
    default:
        break;
    }

    out_plan->draw_popup = UI_Popup_IsVisible();
    out_plan->draw_toast = UI_Toast_IsVisible();
    out_plan->draw_menu = UI_Menu_IsVisible();
    out_plan->draw_confirm = UI_Confirm_IsVisible();

    if ((out_plan->draw_menu != false) || (out_plan->draw_confirm != false))
    {
        out_plan->draw_bottombar = true;
    }
}

/* -------------------------------------------------------------------------- */
/* Get main-body top Y when status bar is visible                              */
/* -------------------------------------------------------------------------- */
static int16_t ui_engine_get_statusbar_body_top_y(u8g2_t *u8g2, bool statusbar_visible)
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
/* Get fixed-bottom-mode body bottom Y                                         */
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
/* Compute main viewport                                                       */
/* -------------------------------------------------------------------------- */
static void ui_engine_compute_viewport(u8g2_t *u8g2,
                                       const ui_engine_compose_plan_t *plan,
                                       ui_rect_t *out_viewport)
{
    ui_rect_t view;
    int16_t top_y;
    int16_t bottom_y;

    if ((plan == NULL) || (out_viewport == NULL))
    {
        return;
    }

    view.x = 0;
    view.y = 0;
    view.w = UI_LCD_W;
    view.h = UI_LCD_H;

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
/* Draw current main viewport                                                  */
/* -------------------------------------------------------------------------- */
static void ui_engine_draw_main_viewport(u8g2_t *u8g2, const ui_rect_t *viewport)
{
    if ((u8g2 == NULL) || (viewport == NULL))
    {
        return;
    }

    switch (s_current_screen)
    {
    case UI_SCREEN_VARIO:
        UI_ScreenVario_Draw(u8g2, viewport);
        break;

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

    case UI_SCREEN_GPS:
        UI_ScreenGps_Draw(u8g2, viewport, s_imperial_units);
        break;

    case UI_SCREEN_DEBUG_LEGACY:
    case UI_SCREEN_COUNT:
    default:
        break;
    }
}

/* -------------------------------------------------------------------------- */
/* Draw root                                                                   */
/*                                                                            */
/* 실제 raster 순서                                                             */
/*   1) main viewport                                                          */
/*   2) status bar                                                             */
/*   3) bottom bar                                                             */
/*   4) toast                                                                  */
/*   5) popup                                                                  */
/*   6) menu                                                                   */
/*   7) confirm                                                                */
/* -------------------------------------------------------------------------- */
static void ui_engine_draw_root(u8g2_t *u8g2, uint32_t now_ms)
{
    ui_engine_compose_plan_t plan;
    ui_rect_t viewport;
    ui_statusbar_model_t status_model;

    if (u8g2 == NULL)
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

    if (plan.draw_menu != false)
    {
        UI_Menu_Draw(u8g2);
    }

    if (plan.draw_confirm != false)
    {
        UI_Confirm_Draw(u8g2);
    }
}
