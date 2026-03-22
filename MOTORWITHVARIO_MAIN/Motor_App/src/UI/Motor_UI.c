
#include "Motor_UI.h"

#include "Motor_State.h"
#include "Motor_UI_Internal.h"
#include "u8g2_uc1608_stm32.h"

#include <string.h>

void Motor_UI_Init(void)
{
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
    /*  상단 큰 타이틀                                                          */
    /*  - 화면 중앙 상단에 제품명                                                */
    /*  - 부팅 splash 단계이므로 아직 실제 센서값은 표시하지 않는다.            */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFont(u8g2, u8g2_font_9x15_mn);
    u8g2_DrawStr(u8g2, 42, 40, "MOTOR APP");

    /* ---------------------------------------------------------------------- */
    /*  중앙 설명 문구                                                          */
    /*  - 이 기기가 네비게이션이 아니라 ride computer 임을 보여 주는 짧은 문구 */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFont(u8g2, u8g2_font_6x10_tr);
    u8g2_DrawStr(u8g2, 38, 62, "Ride Telemetry Computer");
    u8g2_DrawStr(u8g2, 30, 78, "Logging / Dynamics / Vehicle");

    /* ---------------------------------------------------------------------- */
    /*  하단 진행 라인                                                          */
    /*  - 화면 맨 아래에 좌우 여백을 둔 얇은 선                                 */
    /* ---------------------------------------------------------------------- */
    u8g2_DrawFrame(u8g2, 20, 100, 200, 12);
    u8g2_DrawBox(u8g2, 22, 102, 80, 8);
    U8G2_UC1608_CommitBuffer();
}

void Motor_UI_Task(uint32_t now_ms)
{
    const motor_state_t *state;
    u8g2_t *u8g2;

    (void)now_ms;
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

    u8g2_ClearBuffer(u8g2);

    switch ((motor_screen_t)state->ui.screen)
    {
    case MOTOR_SCREEN_MAIN:
        Motor_UI_DrawScreen_Main(u8g2, state);
        break;
    case MOTOR_SCREEN_DATA_FIELD_1:
        Motor_UI_DrawScreen_DataField(u8g2, state, 0u);
        break;
    case MOTOR_SCREEN_DATA_FIELD_2:
        Motor_UI_DrawScreen_DataField(u8g2, state, 1u);
        break;
    case MOTOR_SCREEN_CORNER:
        Motor_UI_DrawScreen_Corner(u8g2, state);
        break;
    case MOTOR_SCREEN_COMPASS:
        Motor_UI_DrawScreen_Compass(u8g2, state);
        break;
    case MOTOR_SCREEN_BREADCRUMB:
        Motor_UI_DrawScreen_Breadcrumb(u8g2, state);
        break;
    case MOTOR_SCREEN_ALTITUDE:
        Motor_UI_DrawScreen_Altitude(u8g2, state);
        break;
    case MOTOR_SCREEN_HORIZON:
        Motor_UI_DrawScreen_Horizon(u8g2, state);
        break;
    case MOTOR_SCREEN_VEHICLE_SUMMARY:
        Motor_UI_DrawScreen_Vehicle(u8g2, state);
        break;
    case MOTOR_SCREEN_MENU:
        Motor_UI_DrawScreen_Menu(u8g2, state);
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
        Motor_UI_DrawScreen_Settings(u8g2, state);
        break;
    case MOTOR_SCREEN_ACCEL_TEST_STUB:
        Motor_UI_DrawScreen_Stub(u8g2, state, "0-100 TEST", "Stub prepared", "Use MENU to return");
        break;
    case MOTOR_SCREEN_LAP_TIMER_STUB:
        Motor_UI_DrawScreen_Stub(u8g2, state, "LAP TIMER", "Circuit mode stub", "Ready for future mode");
        break;
    case MOTOR_SCREEN_LOG_VIEW_STUB:
        Motor_UI_DrawScreen_Stub(u8g2, state, "RIDE LOGS", "Browser stub", "Decoder comes later");
        break;
    case MOTOR_SCREEN_OBD_CONNECT_STUB:
        Motor_UI_DrawScreen_Stub(u8g2, state, "OBD LINK", (state->vehicle.connected != false) ? "Connected" : "Waiting / stub", "BT UART module later");
        break;
    case MOTOR_SCREEN_OBD_DTC_STUB:
        Motor_UI_DrawScreen_Stub(u8g2, state, "OBD DTC", "DTC page stub", "Use MENU to return");
        break;
    default:
        Motor_UI_DrawScreen_Stub(u8g2, state, "MOTOR APP", "Undefined screen", "Fallback page");
        break;
    }

    Motor_UI_DrawToast(u8g2, state);
    U8G2_UC1608_CommitBuffer();
}
