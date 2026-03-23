#include "Motor_UI.h"

#include "Motor_State.h"
#include "Motor_UI_Internal.h"
#include "button.h"
#include "ui_bottombar.h"
#include "ui_popup.h"
#include "ui_statusbar.h"
#include "ui_toast.h"
#include "u8g2_uc1608_stm32.h"

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

void Motor_UI_Init(void)
{
    UI_BottomBar_Init();
    UI_Popup_Init();
    UI_Toast_Init();
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
    const motor_state_t *state;
    u8g2_t *u8g2;
    ui_layout_mode_t layout_mode;
    ui_rect_t viewport;
    ui_statusbar_model_t status_model;
    uint8_t bottom_visible;

    state = Motor_State_Get();
    if (state == 0)
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
        return;
    }

    layout_mode = motor_ui_get_layout_mode(state);
    bottom_visible = (layout_mode != UI_LAYOUT_MODE_TOP_EXTENDED_NO_BOTTOM) ? 1u : 0u;

    UI_Toast_Task(now_ms);
    UI_Popup_Task(now_ms);

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

    Motor_UI_BuildStatusBarModel(state, &status_model);
    UI_StatusBar_Draw(u8g2, &status_model, now_ms);

    if (bottom_visible != 0u)
    {
        UI_BottomBar_Draw(u8g2, Button_GetPressedMask());
    }

    UI_Toast_Draw(u8g2, (bottom_visible != 0u) ? true : false);
    if (UI_Popup_IsVisible() != false)
    {
        UI_Popup_Draw(u8g2);
    }

    U8G2_UC1608_CommitBuffer();

    ((motor_state_t *)state)->ui.redraw_requested = 0u;
}
