#include "Motor_UI.h"

#include "Motor_State.h"
#include "Motor_UI_Internal.h"
#include "button.h"
#include "ui_bottombar.h"
#include "ui_popup.h"
#include "ui_statusbar.h"
#include "ui_toast.h"
#include "u8g2_uc1608_stm32.h"

#include <stdbool.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*  Motor UI local frame scheduler                                             */
/*                                                                            */
/*  목적                                                                       */
/*  - Motor_State.ui.redraw_requested 는 "UI 상태 변화"에는 잘 반응하지만,      */
/*    ride telemetry 값의 주기적 화면 갱신 주파수까지 표현하지는 않는다.       */
/*  - 따라서 여기서는                                                           */
/*      1) 화면 종류별 최소 redraw cadence                                     */
/*      2) status bar 모델 변화                                                 */
/*      3) bottom bar pressed-mask 변화                                         */
/*      4) popup / toast visibility 변화                                        */
/*    를 함께 묶어서 "정말 지금 다시 그릴 이유가 있는가" 를 판정한다.          */
/*                                                                            */
/*  중요한 계층 규칙                                                            */
/*  - 저수준 UC1608 드라이버를 Motor_App가 직접 두드리지 않는다.               */
/*  - frame token 획득 / commit 은 기존 U8G2_UC1608 API만 사용한다.            */
/* -------------------------------------------------------------------------- */
typedef struct
{
    uint32_t             last_draw_ms;
    uint32_t             last_pressed_mask;
    ui_statusbar_model_t last_status_model;
    uint8_t              last_status_model_valid;
    uint8_t              last_bottom_visible;
    uint8_t              last_popup_visible;
    uint8_t              last_toast_visible;
    uint8_t              last_screen;
} motor_ui_runtime_t;

static motor_ui_runtime_t s_motor_ui_runtime;

static ui_layout_mode_t motor_ui_get_layout_mode(const motor_state_t *state)
{
    motor_screen_t screen;

    if (state == 0)
    {
        return UI_LAYOUT_MODE_TOP_EXTENDED_NO_BOTTOM;
    }

    screen = (motor_screen_t)state->ui.screen;
    if (screen < MOTOR_SCREEN_MENU)
    {
        if ((state->ui.overlay_visible != 0u) && (state->ui.overlay_until_ms > state->now_ms))
        {
            return UI_LAYOUT_MODE_TOP_EXTENDED_OVERLAY;
        }
        return UI_LAYOUT_MODE_TOP_EXTENDED_NO_BOTTOM;
    }

    return UI_LAYOUT_MODE_TOP_BOTTOM_FIXED;
}

/* -------------------------------------------------------------------------- */
/*  화면 종류별 주기적 redraw cadence                                           */
/*                                                                            */
/*  선택 기준                                                                   */
/*  - Main / DataField / Corner / Horizon : 체감상 빠르게 변하는 값이 많다.     */
/*    -> 10Hz                                                                   */
/*  - Compass / Altitude / Vehicle / Breadcrumb : 값이 상대적으로 천천히 변한다. */
/*    -> 5Hz                                                                    */
/*  - Menu / Settings / Stub : telemetry 본문보다 정적이다.                     */
/*    -> 1Hz + explicit redraw request                                         */
/* -------------------------------------------------------------------------- */
static uint32_t motor_ui_get_periodic_refresh_ms(motor_screen_t screen)
{
    switch (screen)
    {
    case MOTOR_SCREEN_MAIN:
    case MOTOR_SCREEN_DATA_FIELD_1:
    case MOTOR_SCREEN_DATA_FIELD_2:
    case MOTOR_SCREEN_CORNER:
        return 100u;

    case MOTOR_SCREEN_COMPASS:
    case MOTOR_SCREEN_BREADCRUMB:
    case MOTOR_SCREEN_HORIZON:
        /* ------------------------------------------------------------------ */
        /*  Motion-heavy pages                                                 */
        /*  - compass / breadcrumb / horizon 은 heading, bank, IMU 기반으로    */
        /*    연속적으로 움직이는 화면이다.                                    */
        /*  - 기존 100~200ms cadence 에서는 값은 계속 바뀌는데 draw 가 5~10Hz로  */
        /*    묶여서, 특히 breadcrumb 상단 bank 표시가 끊겨 보였다.             */
        /*  - 이 세 화면은 명시적으로 20fps(50ms) 주기로 올려서 motion UI 가     */
        /*    부드럽게 따라오도록 한다.                                        */
        /* ------------------------------------------------------------------ */
        return 50u;

    case MOTOR_SCREEN_ALTITUDE:
    case MOTOR_SCREEN_VEHICLE_SUMMARY:
        return 200u;

    case MOTOR_SCREEN_MENU:
    case MOTOR_SCREEN_SETTINGS_ROOT:
    case MOTOR_SCREEN_SETTINGS_DISPLAY:
    case MOTOR_SCREEN_SETTINGS_GPS:
    case MOTOR_SCREEN_SETTINGS_UNITS:
    case MOTOR_SCREEN_SETTINGS_RECORDING:
    case MOTOR_SCREEN_SETTINGS_DYNAMICS:
    case MOTOR_SCREEN_SETTINGS_MAINTENANCE:
    case MOTOR_SCREEN_SETTINGS_OBD:
    case MOTOR_SCREEN_SETTINGS_SYSTEM:
    case MOTOR_SCREEN_ACCEL_TEST_STUB:
    case MOTOR_SCREEN_LAP_TIMER_STUB:
    case MOTOR_SCREEN_LOG_VIEW_STUB:
    case MOTOR_SCREEN_OBD_CONNECT_STUB:
    case MOTOR_SCREEN_OBD_DTC_STUB:
    case MOTOR_SCREEN_COUNT:
    default:
        return 1000u;
    }
}

static bool motor_ui_status_model_changed(const ui_statusbar_model_t *status_model)
{
    if (status_model == 0)
    {
        return false;
    }

    if (s_motor_ui_runtime.last_status_model_valid == 0u)
    {
        return true;
    }

    if (memcmp(&s_motor_ui_runtime.last_status_model, status_model, sizeof(*status_model)) != 0)
    {
        return true;
    }

    return false;
}

static bool motor_ui_should_draw(const motor_state_t *state,
                                 const ui_statusbar_model_t *status_model,
                                 uint8_t bottom_visible,
                                 uint32_t pressed_mask,
                                 uint8_t popup_changed,
                                 uint8_t toast_changed)
{
    uint32_t refresh_period_ms;

    if (state == 0)
    {
        return false;
    }

    /* ---------------------------------------------------------------------- */
    /*  1) 명시적 redraw request                                                */
    /* ---------------------------------------------------------------------- */
    if (state->ui.redraw_requested != 0u)
    {
        return true;
    }

    /* ---------------------------------------------------------------------- */
    /*  2) overlay / toast / popup visibility 변화                              */
    /* ---------------------------------------------------------------------- */
    if ((popup_changed != 0u) || (toast_changed != 0u))
    {
        return true;
    }

    /* ---------------------------------------------------------------------- */
    /*  3) status bar 모델 변화                                                 */
    /* ---------------------------------------------------------------------- */
    if (motor_ui_status_model_changed(status_model) != false)
    {
        return true;
    }

    /* ---------------------------------------------------------------------- */
    /*  4) bottom bar pressed-mask 변화                                         */
    /* ---------------------------------------------------------------------- */
    if ((bottom_visible != 0u) && (pressed_mask != s_motor_ui_runtime.last_pressed_mask))
    {
        return true;
    }

    /* ---------------------------------------------------------------------- */
    /*  5) layout / screen 전환                                                 */
    /* ---------------------------------------------------------------------- */
    if ((s_motor_ui_runtime.last_screen != state->ui.screen) ||
        (s_motor_ui_runtime.last_bottom_visible != bottom_visible))
    {
        return true;
    }

    /* ---------------------------------------------------------------------- */
    /*  6) 화면 종류별 주기적 telemetry refresh                                 */
    /* ---------------------------------------------------------------------- */
    refresh_period_ms = motor_ui_get_periodic_refresh_ms((motor_screen_t)state->ui.screen);
    if ((s_motor_ui_runtime.last_draw_ms == 0u) ||
        ((uint32_t)(state->now_ms - s_motor_ui_runtime.last_draw_ms) >= refresh_period_ms))
    {
        return true;
    }

    return false;
}

static void motor_ui_capture_draw_state(const motor_state_t *state,
                                        const ui_statusbar_model_t *status_model,
                                        uint8_t bottom_visible,
                                        uint32_t pressed_mask)
{
    if ((state == 0) || (status_model == 0))
    {
        return;
    }

    s_motor_ui_runtime.last_draw_ms = state->now_ms;
    s_motor_ui_runtime.last_pressed_mask = pressed_mask;
    s_motor_ui_runtime.last_bottom_visible = bottom_visible;
    s_motor_ui_runtime.last_popup_visible = (UI_Popup_IsVisible() != false) ? 1u : 0u;
    s_motor_ui_runtime.last_toast_visible = (UI_Toast_IsVisible() != false) ? 1u : 0u;
    s_motor_ui_runtime.last_screen = state->ui.screen;
    s_motor_ui_runtime.last_status_model = *status_model;
    s_motor_ui_runtime.last_status_model_valid = 1u;
}

void Motor_UI_Init(void)
{
    UI_BottomBar_Init();
    UI_Popup_Init();
    UI_Toast_Init();
    memset(&s_motor_ui_runtime, 0, sizeof(s_motor_ui_runtime));
    s_motor_ui_runtime.last_screen = (uint8_t)MOTOR_SCREEN_COUNT;
}

void Motor_UI_EarlyBootDraw(void)
{
    u8g2_t *u8g2;

    u8g2 = U8G2_UC1608_GetHandle();
    if (u8g2 == 0)
    {
        return;
    }

    u8g2_ClearBuffer(u8g2);

    /* ---------------------------------------------------------------------- */
    /*  부트 splash                                                            */
    /*  - 화면 중앙 상단에 Motor App 타이틀                                    */
    /*  - 실제 ride computer 성격을 짧은 문구로 보여 준다.                     */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFont(u8g2, u8g2_font_9x15B_tr);
    u8g2_DrawStr(u8g2, 42, 40, "MOTOR APP");

    u8g2_SetFont(u8g2, u8g2_font_6x10_tr);
    u8g2_DrawStr(u8g2, 36, 62, "Ride Telemetry Computer");
    u8g2_DrawStr(u8g2, 31, 78, "Logging / Dynamics / Vehicle");

    u8g2_DrawFrame(u8g2, 20, 100, 200, 12);
    u8g2_DrawBox(u8g2, 22, 102, 96, 8);
    U8G2_UC1608_CommitBuffer();
}

void Motor_UI_Task(uint32_t now_ms)
{
    motor_state_t *state_mut;
    const motor_state_t *state;
    u8g2_t *u8g2;
    ui_layout_mode_t layout_mode;
    ui_rect_t viewport;
    ui_statusbar_model_t status_model;
    uint8_t bottom_visible;
    uint32_t pressed_mask;
    uint8_t popup_visible_before;
    uint8_t popup_visible_after;
    uint8_t toast_visible_before;
    uint8_t toast_visible_after;
    uint8_t popup_changed;
    uint8_t toast_changed;

    (void)now_ms;

    state_mut = Motor_State_GetMutable();
    state = state_mut;
    if (state == 0)
    {
        return;
    }

    layout_mode = motor_ui_get_layout_mode(state);
    bottom_visible = (layout_mode != UI_LAYOUT_MODE_TOP_EXTENDED_NO_BOTTOM) ? 1u : 0u;
    pressed_mask = Button_GetPressedMask();

    /* ---------------------------------------------------------------------- */
    /*  popup / toast runtime는 draw 전에 항상 한 번 진행한다.                 */
    /*                                                                          */
    /*  이유                                                                     */
    /*  - timeout 만료가 draw 여부와 무관하게 진행되어야 한다.                  */
    /*  - visibility 변화가 발생하면 그것 자체가 redraw reason 이 된다.         */
    /* ---------------------------------------------------------------------- */
    popup_visible_before = (UI_Popup_IsVisible() != false) ? 1u : 0u;
    toast_visible_before = (UI_Toast_IsVisible() != false) ? 1u : 0u;

    UI_Toast_Task(now_ms);
    UI_Popup_Task(now_ms);

    popup_visible_after = (UI_Popup_IsVisible() != false) ? 1u : 0u;
    toast_visible_after = (UI_Toast_IsVisible() != false) ? 1u : 0u;
    popup_changed = (popup_visible_before != popup_visible_after) ? 1u : 0u;
    toast_changed = (toast_visible_before != toast_visible_after) ? 1u : 0u;

    if ((popup_changed != 0u) || (toast_changed != 0u))
    {
        state_mut->ui.redraw_requested = 1u;
    }

    memset(&status_model, 0, sizeof(status_model));
    Motor_UI_BuildStatusBarModel(state, &status_model);

    if (motor_ui_should_draw(state,
                             &status_model,
                             bottom_visible,
                             pressed_mask,
                             popup_changed,
                             toast_changed) == false)
    {
        return;
    }

    if (U8G2_UC1608_TryAcquireFrameToken() == 0u)
    {
        return;
    }

    u8g2 = U8G2_UC1608_GetHandle();
    if (u8g2 == 0)
    {
        state_mut->ui.redraw_requested = 1u;
        return;
    }

    u8g2_ClearBuffer(u8g2);
    Motor_UI_ConfigureBottomBar(state);
    Motor_UI_ComputeViewport(u8g2, state, layout_mode, &viewport);

    switch ((motor_screen_t)state->ui.screen)
    {
    case MOTOR_SCREEN_MAIN:
        Motor_UI_DrawScreen_Main(u8g2, &viewport, state);
        break;
    case MOTOR_SCREEN_DATA_FIELD_1:
        Motor_UI_DrawScreen_DataField(u8g2, &viewport, state, 0u);
        break;
    case MOTOR_SCREEN_DATA_FIELD_2:
        Motor_UI_DrawScreen_DataField(u8g2, &viewport, state, 1u);
        break;
    case MOTOR_SCREEN_CORNER:
        Motor_UI_DrawScreen_Corner(u8g2, &viewport, state);
        break;
    case MOTOR_SCREEN_COMPASS:
        Motor_UI_DrawScreen_Compass(u8g2, &viewport, state);
        break;
    case MOTOR_SCREEN_BREADCRUMB:
        Motor_UI_DrawScreen_Breadcrumb(u8g2, &viewport, state);
        break;
    case MOTOR_SCREEN_ALTITUDE:
        Motor_UI_DrawScreen_Altitude(u8g2, &viewport, state);
        break;
    case MOTOR_SCREEN_HORIZON:
        Motor_UI_DrawScreen_Horizon(u8g2, &viewport, state);
        break;
    case MOTOR_SCREEN_VEHICLE_SUMMARY:
        Motor_UI_DrawScreen_Vehicle(u8g2, &viewport, state);
        break;
    case MOTOR_SCREEN_MENU:
        Motor_UI_DrawScreen_Menu(u8g2, &viewport, state);
        break;
    case MOTOR_SCREEN_SETTINGS_ROOT:
    case MOTOR_SCREEN_SETTINGS_DISPLAY:
    case MOTOR_SCREEN_SETTINGS_GPS:
    case MOTOR_SCREEN_SETTINGS_UNITS:
    case MOTOR_SCREEN_SETTINGS_RECORDING:
    case MOTOR_SCREEN_SETTINGS_DYNAMICS:
    case MOTOR_SCREEN_SETTINGS_MAINTENANCE:
    case MOTOR_SCREEN_SETTINGS_OBD:
    case MOTOR_SCREEN_SETTINGS_SYSTEM:
        Motor_UI_DrawScreen_Settings(u8g2, &viewport, state);
        break;
    case MOTOR_SCREEN_ACCEL_TEST_STUB:
        Motor_UI_DrawScreen_Stub(u8g2, &viewport, state, "0-100 TEST", "Stub prepared", "Use MENU to return");
        break;
    case MOTOR_SCREEN_LAP_TIMER_STUB:
        Motor_UI_DrawScreen_Stub(u8g2, &viewport, state, "LAP TIMER", "Circuit mode stub", "Ready for future mode");
        break;
    case MOTOR_SCREEN_LOG_VIEW_STUB:
        Motor_UI_DrawScreen_Stub(u8g2, &viewport, state, "RIDE LOGS", "Browser stub", "Decoder comes later");
        break;
    case MOTOR_SCREEN_OBD_CONNECT_STUB:
        Motor_UI_DrawScreen_Stub(u8g2,
                                 &viewport,
                                 state,
                                 "OBD LINK",
                                 (state->vehicle.connected != false) ? "Connected" : "Waiting / stub",
                                 "BT UART module later");
        break;
    case MOTOR_SCREEN_OBD_DTC_STUB:
        Motor_UI_DrawScreen_Stub(u8g2, &viewport, state, "OBD DTC", "DTC page stub", "Use MENU to return");
        break;
    default:
        Motor_UI_DrawScreen_Stub(u8g2, &viewport, state, "MOTOR APP", "Undefined screen", "Fallback page");
        break;
    }

    UI_StatusBar_Draw(u8g2, &status_model, now_ms);

    if (bottom_visible != 0u)
    {
        UI_BottomBar_Draw(u8g2, pressed_mask);
    }

    UI_Toast_Draw(u8g2, (bottom_visible != 0u) ? true : false);
    if (UI_Popup_IsVisible() != false)
    {
        UI_Popup_Draw(u8g2);
    }

    U8G2_UC1608_CommitBuffer();

    motor_ui_capture_draw_state(state, &status_model, bottom_visible, pressed_mask);
    state_mut->ui.redraw_requested = 0u;
}
