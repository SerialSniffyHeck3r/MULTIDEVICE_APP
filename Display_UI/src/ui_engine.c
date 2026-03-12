#include "../inc/ui_engine.h"

#include "../inc/ui_types.h"
#include "../inc/ui_statusbar.h"
#include "../inc/ui_bottombar.h"
#include "../inc/ui_popup.h"
#include "../inc/ui_toast.h"
#include "../inc/ui_boot.h"
#include "../inc/ui_screen_test.h"
#include "../Resources/Common_Icons/ui_common_icons.h"
#include "../Debug/ui_debug_legacy.h"

#include "../../Platform/inc/APP_STATE.h"
#include "../../Platform/inc/button.h"
#include "../../../Core/Inc/u8g2_uc1608_stm32.h"

#include <string.h>

/* -------------------------------------------------------------------------- */
/*  Public blink variables                                                     */
/*                                                                            */
/*  사용자가 기존 이름을 그대로 유지해 달라고 했으므로,                         */
/*  SlowToggle2Hz / FastToggle5Hz를 전역으로 정의한다.                         */
/* -------------------------------------------------------------------------- */
volatile bool SlowToggle2Hz = false;
volatile bool FastToggle5Hz = false;

/* -------------------------------------------------------------------------- */
/*  20Hz timing counters                                                       */
/*                                                                            */
/*  TIM7 ISR가 20Hz frame tick을 만들어 주고,                                  */
/*  여기서 slow/fast blink를 같은 하드웨어 타이머로 재사용한다.                */
/* -------------------------------------------------------------------------- */
static volatile uint32_t s_ui_tick_20hz = 0u;
static volatile uint8_t  s_slow_divider = 0u;
static volatile uint8_t  s_fast_divider = 0u;

static ui_screen_id_t           s_current_screen = UI_SCREEN_TEST;
static ui_layout_mode_t         s_layout_mode = UI_LAYOUT_MODE_TOP_BOTTOM_FIXED;
static ui_record_state_t        s_record_state = UI_RECORD_STATE_STOP;
static ui_bluetooth_stub_state_t s_bt_stub_state = UI_BT_STUB_ON;

static uint8_t                  s_imperial_units = 0u;
static uint8_t                  s_test_blink_paused = 0u;
static uint8_t                  s_test_cute_icon_index = 0u;
static uint32_t                 s_bottom_overlay_until_ms = 0u;
static uint32_t                 s_pressed_mask = 0u;
static uint8_t                  s_force_redraw = 1u;

/* -------------------------------------------------------------------------- */
/*  Local helpers                                                              */
/* -------------------------------------------------------------------------- */
static void ui_engine_request_overlay(uint32_t now_ms);
static void ui_engine_configure_test_bottom_bar(void);
static void ui_engine_handle_test_events(uint32_t now_ms);
static void ui_engine_handle_test_button_event(const button_event_t *event, uint32_t now_ms);
static void ui_engine_build_status_model(ui_statusbar_model_t *out_model);
static void ui_engine_compute_viewport(ui_rect_t *out_viewport, bool *out_status_visible, bool *out_bottom_visible);
static void ui_engine_draw_test_root(u8g2_t *u8g2, uint32_t now_ms);

/* -------------------------------------------------------------------------- */
/*  Public init                                                                */
/* -------------------------------------------------------------------------- */
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
    s_test_blink_paused = 0u;
    s_test_cute_icon_index = 0u;
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

/* -------------------------------------------------------------------------- */
/*  TIM7 ISR hook                                                              */
/* -------------------------------------------------------------------------- */
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

/* -------------------------------------------------------------------------- */
/*  Optional board debug button hook                                           */
/*                                                                            */
/*  - 테스트 화면에서는 legacy debug로 바로 진입                               */
/*  - legacy debug 화면에서는 기존 main.c 동작처럼 페이지를 넘긴다             */
/* -------------------------------------------------------------------------- */
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

/* -------------------------------------------------------------------------- */
/*  Root task                                                                  */
/* -------------------------------------------------------------------------- */
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
        ui_engine_handle_test_events(now_ms);
    }

    UI_Popup_Task(now_ms);
    UI_Toast_Task(now_ms);

    if (U8G2_UC1608_TryAcquireFrameToken() == 0u)
    {
        return;
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
/*  Bottom bar labels for test screen                                          */
/* -------------------------------------------------------------------------- */
static void ui_engine_configure_test_bottom_bar(void)
{
    UI_BottomBar_SetMode(UI_BOTTOMBAR_MODE_BUTTONS);

    UI_BottomBar_SetButton(UI_FKEY_1, "DBG", UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButton(UI_FKEY_2,
                           (s_test_blink_paused != 0u) ? "RUN" : "PAUSE",
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
/*  Test-mode button dispatch                                                  */
/*                                                                            */
/*  요구된 F1~F5 동작을 그대로 구현하고,                                       */
/*  F6는 추가 테스트용으로 cute icon 순환에 할당했다.                           */
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
        s_test_blink_paused = (s_test_blink_paused == 0u) ? 1u : 0u;
        ui_engine_configure_test_bottom_bar();
        UI_Toast_Show((s_test_blink_paused != 0u) ? "BLINK PAUSED" : "BLINK RUN",
                      icon_cute_heart_8x8,
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
                      icon_cute_star_8x8,
                      ICON8_W,
                      ICON8_H,
                      now_ms,
                      900u);
        break;

    case BUTTON_ID_4:
        UI_Toast_Show("TOAST MESSAGE TEST",
                      icon_cute_cat_8x8,
                      ICON8_W,
                      ICON8_H,
                      now_ms,
                      UI_TOAST_DEFAULT_TIMEOUT_MS);
        break;

    case BUTTON_ID_5:
        UI_Popup_Show("POPUP",
                      "RIGHT TEXT FIXED",
                      "FIRST LINE NO LONGER JUMPS",
                      icon_cute_bike_8x8,
                      ICON8_W,
                      ICON8_H,
                      now_ms,
                      UI_POPUP_DEFAULT_TIMEOUT_MS);
        break;

    case BUTTON_ID_6:
        s_test_cute_icon_index = (uint8_t)((s_test_cute_icon_index + 1u) & 0x03u);
        UI_Toast_Show("CUTE ICON NEXT",
                      icon_arrow_right_7x4,
                      ICON7X4_W,
                      ICON7X4_H,
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
/*  Build lightweight status bar model                                         */
/*                                                                            */
/*  APP_STATE를 수정하지 않고, volatile 저장소에서 필요한 값만 뽑아 경량 모델을  */
/*  만든다.                                                                    */
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

    out_model->clock_valid = g_app_state.clock.rtc_time_valid;
    out_model->clock_year = g_app_state.clock.local.year;
    out_model->clock_month = g_app_state.clock.local.month;
    out_model->clock_day = g_app_state.clock.local.day;
    out_model->clock_hour = g_app_state.clock.local.hour;
    out_model->clock_minute = g_app_state.clock.local.min;
    out_model->clock_second = g_app_state.clock.local.sec;
    out_model->clock_weekday = g_app_state.clock.local.weekday;

    out_model->record_state = s_record_state;
    out_model->imperial_units = s_imperial_units;
    out_model->bluetooth_stub_state = s_bt_stub_state;
    out_model->bluetooth_aux_visible = 1u;
}

/* -------------------------------------------------------------------------- */
/*  Viewport computation                                                       */
/*                                                                            */
/*  4가지 레이아웃 모드를 여기 한 곳에서만 계산한다.                            */
/* -------------------------------------------------------------------------- */
static void ui_engine_compute_viewport(ui_rect_t *out_viewport, bool *out_status_visible, bool *out_bottom_visible)
{
    bool status_visible = true;
    bool bottom_visible = false;
    ui_rect_t view;

    view.x = 0;
    view.y = 0;
    view.w = UI_LCD_W;
    view.h = UI_LCD_H;

    switch (s_layout_mode)
    {
    case UI_LAYOUT_MODE_TOP_EXTENDED_NO_BOTTOM:
        status_visible = true;
        bottom_visible = false;
        view.y = UI_STATUSBAR_H + UI_STATUSBAR_GAP_H;
        view.h = UI_LCD_H - view.y;
        break;

    case UI_LAYOUT_MODE_TOP_EXTENDED_OVERLAY:
        status_visible = true;
        bottom_visible = ((s_pressed_mask != 0u) ||
                          ((int32_t)(s_bottom_overlay_until_ms - HAL_GetTick()) > 0));
        view.y = UI_STATUSBAR_H + UI_STATUSBAR_GAP_H;
        view.h = UI_LCD_H - view.y;
        break;

    case UI_LAYOUT_MODE_TOP_BOTTOM_FIXED:
        status_visible = true;
        bottom_visible = true;
        view.y = UI_STATUSBAR_H + UI_STATUSBAR_GAP_H;
        view.h = UI_LCD_H - UI_STATUSBAR_H - UI_STATUSBAR_GAP_H -
                 UI_BOTTOMBAR_H - UI_BOTTOMBAR_GAP_H;
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
/*  Draw test root                                                             */
/* -------------------------------------------------------------------------- */
static void ui_engine_draw_test_root(u8g2_t *u8g2, uint32_t now_ms)
{
    ui_rect_t viewport;
    bool status_visible;
    bool bottom_visible;
    ui_statusbar_model_t status_model;

    ui_engine_compute_viewport(&viewport, &status_visible, &bottom_visible);

    /* Priority 1: main screen */
    UI_ScreenTest_Draw(u8g2,
                       &viewport,
                       s_ui_tick_20hz,
                       (s_test_blink_paused != 0u),
                       s_test_cute_icon_index,
                       s_layout_mode);

    /* Priority 2: status bar */
    if (status_visible != false)
    {
        ui_engine_build_status_model(&status_model);
        UI_StatusBar_Draw(u8g2, &status_model, now_ms);
    }

    /* Priority 3: bottom bar */
    if (bottom_visible != false)
    {
        UI_BottomBar_Draw(u8g2, s_pressed_mask);
    }

    /* Priority 4: toast */
    if (UI_Toast_IsVisible() != false)
    {
        UI_Toast_Draw(u8g2, bottom_visible);
    }

    /* Priority 5: popup */
    if (UI_Popup_IsVisible() != false)
    {
        UI_Popup_Draw(u8g2);
    }
}
