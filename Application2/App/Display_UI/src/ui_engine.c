#include "ui_engine.h"

#include "ui_types.h"
#include "ui_statusbar.h"
#include "ui_bottombar.h"
#include "ui_popup.h"
#include "ui_toast.h"
#include "ui_boot.h"
#include "ui_screen_test.h"
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
/*  Local helpers                                                              */
/* -------------------------------------------------------------------------- */
static void ui_engine_request_overlay(uint32_t now_ms);
static void ui_engine_configure_test_bottom_bar(void);
static void ui_engine_update_test_dynamics(void);
static void ui_engine_handle_test_events(uint32_t now_ms);
static void ui_engine_handle_test_button_event(const button_event_t *event, uint32_t now_ms);
static void ui_engine_build_status_model(ui_statusbar_model_t *out_model);
static void ui_engine_compute_viewport(u8g2_t *u8g2,
                                       uint32_t now_ms,
                                       ui_rect_t *out_viewport,
                                       bool *out_status_visible,
                                       bool *out_bottom_visible);
static void ui_engine_draw_test_root(u8g2_t *u8g2, uint32_t now_ms);
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
        ui_engine_draw_test_root(u8g2, now_ms);
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

    if (event->type != BUTTON_EVENT_SHORT_PRESS)
    {
        return;
    }

    switch (event->id)
    {
    case BUTTON_ID_1:
        s_current_screen = UI_SCREEN_DEBUG_LEGACY;
        UI_DebugLegacy_Init();
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
static void ui_engine_draw_test_root(u8g2_t *u8g2, uint32_t now_ms)
{
    ui_rect_t viewport;
    bool status_visible;
    bool bottom_visible;
    ui_statusbar_model_t status_model;

    ui_engine_compute_viewport(u8g2, now_ms, &viewport, &status_visible, &bottom_visible);

    UI_ScreenTest_Draw(u8g2,
                       &viewport,
                       s_test_live_counter_20hz,
                       (s_test_updates_paused != 0u),
                       s_test_blink_phase_on,
                       s_test_cute_icon_index,
                       s_layout_mode);

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
