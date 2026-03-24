#include "Motor_Task.h"

#include "main.h"

#include "APP_ALTITUDE.h"
#include "APP_CLOCK.h"
#include "APP_SD.h"
#include "Audio_App.h"
#include "Audio_Driver.h"
#include "BACKLIGHT_App.h"
#include "Bluetooth.h"
#include "Brightness_Sensor.h"
#include "DEBUG_UART.h"
#include "DS18B20_DRIVER.h"
#include "FW_AppGuard.h"
#include "GY86_IMU.h"
#include "LED_App.h"
#include "POWER_STATE.h"
#include "SPI_Flash.h"
#include "Ublox_GPS.h"
#include "button.h"

#include "Motor_Buttons.h"
#include "Motor_Dynamics.h"
#include "Motor_Maintenance.h"
#include "Motor_Navigation.h"
#include "Motor_Record.h"
#include "Motor_Panel.h"
#include "Motor_Settings.h"
#include "Motor_State.h"
#include "Motor_UI.h"
#include "Motor_Vehicle.h"

#ifndef MOTOR_APP_BOOT_CONFIRM_DELAY_MS
#define MOTOR_APP_BOOT_CONFIRM_DELAY_MS 2000u
#endif

/* -------------------------------------------------------------------------- */
/*  Background cadence constants                                               */
/*                                                                            */
/*  왜 필요한가                                                                 */
/*  - Motor_State_Task() 는 APP_STATE snapshot 을 매 루프 최신으로 복사한다.    */
/*  - 따라서 상위 파생 task 들까지 같은 주기로 모두 매번 돌릴 필요는 없다.      */
/*  - 특히 menu/settings/stub 같은 비주행 화면에서는                           */
/*    breadcrumb / maintenance / vehicle stub / logger 제어만 유지하면 된다.    */
/*                                                                            */
/*  주의                                                                       */
/*  - record task 는 session 거리 적분과 auto-start/stop 판정 때문에            */
/*    완전히 멈추지 않고 50ms cadence 를 유지한다.                              */
/*  - dynamics task 는 이미 "새 IMU sample 이 없으면 즉시 return" 방어가       */
/*    있으므로 drive 계열 화면에서는 계속 호출해도 안전하다.                    */
/* -------------------------------------------------------------------------- */
#define MOTOR_TASK_RECORD_PERIOD_MS            50u
#define MOTOR_TASK_NAV_DRIVE_PERIOD_MS        200u
#define MOTOR_TASK_NAV_BACKGROUND_PERIOD_MS   500u
#define MOTOR_TASK_VEHICLE_DRIVE_PERIOD_MS    100u
#define MOTOR_TASK_VEHICLE_BACKGROUND_PERIOD_MS 200u
#define MOTOR_TASK_MAINT_DRIVE_PERIOD_MS      250u
#define MOTOR_TASK_MAINT_BACKGROUND_PERIOD_MS 500u

static uint32_t s_motor_boot_confirm_arm_ms;
static uint8_t  s_motor_boot_confirm_done;
static uint8_t  s_board_debug_irq_pending;

static uint32_t s_last_nav_drive_ms;
static uint32_t s_last_nav_background_ms;
static uint32_t s_last_vehicle_drive_ms;
static uint32_t s_last_vehicle_background_ms;
static uint32_t s_last_maintenance_drive_ms;
static uint32_t s_last_maintenance_background_ms;
static uint32_t s_last_record_ms;

/* -------------------------------------------------------------------------- */
/* 공용 하위 플랫폼 서비스 구간                                                */
/*                                                                            */
/* 규칙                                                                       */
/* - 이 구간은 screen mode와 무관하게 항상 돌아야 하는 저수준 service만 둔다.  */
/* - Motor_App는 여기서 raw 센서 레지스터를 직접 읽지 않는다.                  */
/* - 각 서비스는 APP_STATE를 최신값으로 publish 하고,                         */
/*   Motor_State_Task()가 그 snapshot을 memcpy해 온다.                        */
/* -------------------------------------------------------------------------- */
static void motor_task_run_shared_platform_services(uint32_t now_ms)
{
    APP_SD_Task(now_ms);
    Audio_Driver_Task(now_ms);
    Audio_App_Task(now_ms);
    Ublox_GPS_Task(now_ms);
    APP_CLOCK_Task(now_ms);
    Bluetooth_Task(now_ms);
    DEBUG_UART_Task(now_ms);
    GY86_IMU_Task(now_ms);
    APP_ALTITUDE_Task(now_ms);
    DS18B20_DRIVER_Task(now_ms);
    Brightness_Sensor_Task(now_ms);
    SPI_Flash_Task(now_ms);
    Backlight_App_Task(now_ms);
    Button_Task(now_ms);
}

static uint8_t motor_task_period_elapsed(uint32_t now_ms, uint32_t *last_ms, uint32_t period_ms)
{
    if ((last_ms == 0) || (period_ms == 0u))
    {
        return 1u;
    }

    if ((*last_ms == 0u) || ((uint32_t)(now_ms - *last_ms) >= period_ms))
    {
        *last_ms = now_ms;
        return 1u;
    }

    return 0u;
}

static uint8_t motor_task_record_due(uint32_t now_ms, const motor_state_t *state)
{
    if (state == 0)
    {
        return 0u;
    }

    /* ---------------------------------------------------------------------- */
    /* 사용자의 start / stop / marker 요청은 cadence 지연 없이 즉시 반영한다. */
    /* ---------------------------------------------------------------------- */
    if ((state->record.start_requested != false) ||
        (state->record.stop_requested != false) ||
        (state->record.marker_requested != false))
    {
        return 1u;
    }

    return motor_task_period_elapsed(now_ms, &s_last_record_ms, MOTOR_TASK_RECORD_PERIOD_MS);
}

void Motor_App_Init(void)
{
    /* ---------------------------------------------------------------------- */
    /* Motor_App 초기화 순서                                                   */
    /* 1) 설정 저장소                                                          */
    /* 2) snapshot 기반 런타임 state                                           */
    /* 3) 앱별 계산 모듈                                                       */
    /* 4) 입력 해석 레이어                                                     */
    /* 5) UI facade / panel apply                                              */
    /* ---------------------------------------------------------------------- */
    Motor_Settings_Init();
    Motor_State_Init();
    Motor_Dynamics_Init();
    Motor_Navigation_Init();
    Motor_Vehicle_Init();
    Motor_Maintenance_Init();
    Motor_Record_Init();
    Motor_Buttons_Init();
    Motor_UI_Init();
    Motor_Panel_Init();
    POWER_STATE_Init();

    s_motor_boot_confirm_arm_ms = HAL_GetTick();
    s_motor_boot_confirm_done = 0u;
    s_board_debug_irq_pending = 0u;

    s_last_nav_drive_ms = 0u;
    s_last_nav_background_ms = 0u;
    s_last_vehicle_drive_ms = 0u;
    s_last_vehicle_background_ms = 0u;
    s_last_maintenance_drive_ms = 0u;
    s_last_maintenance_background_ms = 0u;
    s_last_record_ms = 0u;
}

void Motor_App_EarlyBootDraw(void)
{
    Motor_UI_EarlyBootDraw();
}

void Motor_App_OnBoardDebugButtonIrq(uint32_t now_ms)
{
    (void)now_ms;
    s_board_debug_irq_pending = 1u;
}

void Motor_App_Task(void)
{
    uint32_t now_ms;
    const motor_state_t *state;

    now_ms = HAL_GetTick();

    /* watchdog kick 은 항상 상위 루프 최상단에서 수행한다. */
    FW_AppGuard_Kick();

    /* 공용 하위 플랫폼 서비스 구간 */
    motor_task_run_shared_platform_services(now_ms);

    /* ---------------------------------------------------------------------- */
    /* Soft power overlay는 기존 구조상 LCD / button queue 우선권을 갖는다.    */
    /* 따라서 blocking 상태에서는 Motor 상태머신보다 먼저 return 한다.         */
    /* ---------------------------------------------------------------------- */
    POWER_STATE_Task(now_ms);
    if (POWER_STATE_IsUiBlocking() != false)
    {
        return;
    }

    /* APP_STATE snapshot -> Motor runtime snapshot */
    Motor_State_Task(now_ms);
    state = Motor_State_Get();
    if (state == 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /* board debug button은 메뉴 토글에만 사용한다.                            */
    /* ---------------------------------------------------------------------- */
    if (s_board_debug_irq_pending != 0u)
    {
        s_board_debug_irq_pending = 0u;
        if ((motor_screen_t)state->ui.screen == MOTOR_SCREEN_MENU)
        {
            Motor_State_SetScreen((motor_screen_t)state->ui.previous_drive_screen);
        }
        else
        {
            Motor_State_SetScreen(MOTOR_SCREEN_MENU);
        }
    }

    /* ---------------------------------------------------------------------- */
    /* 사용자 요구사항 핵심 구조                                               */
    /* - screen mode를 한 개의 통짜 switch-case 상위 상태머신으로 관리한다.    */
    /* - mode 별로 어떤 app task 묶음을 돌릴지 여기서 명시한다.                */
    /* - 단, breadcrumb / vehicle stub / maintenance / recorder 같은           */
    /*   background 성격 task는 이 switch 내부에서 cadence 를 한 번 더 건다.   */
    /* ---------------------------------------------------------------------- */
    switch ((motor_screen_t)state->ui.screen)
    {
    case MOTOR_SCREEN_MAIN:
        /* ------------------------------------------------------------------ */
        /* 메인 계기판 화면                                                    */
        /* - dynamics 는 새 IMU sample 이 들어올 때만 실질 계산이 일어난다.    */
        /* - nav / vehicle / maintenance / record 는 목적에 맞는 cadence 로    */
        /*   줄여도 UI 의도와 logger 의도는 유지된다.                           */
        /* ------------------------------------------------------------------ */
        Motor_Dynamics_Task(now_ms);
        if (motor_task_period_elapsed(now_ms, &s_last_nav_drive_ms, MOTOR_TASK_NAV_DRIVE_PERIOD_MS) != 0u)
        {
            Motor_Navigation_Task(now_ms);
        }
        if (motor_task_period_elapsed(now_ms, &s_last_vehicle_drive_ms, MOTOR_TASK_VEHICLE_DRIVE_PERIOD_MS) != 0u)
        {
            Motor_Vehicle_Task(now_ms);
        }
        if (motor_task_period_elapsed(now_ms, &s_last_maintenance_drive_ms, MOTOR_TASK_MAINT_DRIVE_PERIOD_MS) != 0u)
        {
            Motor_Maintenance_Task(now_ms);
        }
        if (motor_task_record_due(now_ms, state) != 0u)
        {
            Motor_Record_Task(now_ms);
        }
        Motor_Buttons_Task(now_ms);
        Motor_UI_Task(now_ms);
        LED_App_Task(now_ms);
        break;

    case MOTOR_SCREEN_DATA_FIELD_1:
    case MOTOR_SCREEN_DATA_FIELD_2:
        /* ------------------------------------------------------------------ */
        /* 데이터 필드 페이지                                                   */
        /* ------------------------------------------------------------------ */
        Motor_Dynamics_Task(now_ms);
        if (motor_task_period_elapsed(now_ms, &s_last_nav_drive_ms, MOTOR_TASK_NAV_DRIVE_PERIOD_MS) != 0u)
        {
            Motor_Navigation_Task(now_ms);
        }
        if (motor_task_period_elapsed(now_ms, &s_last_vehicle_drive_ms, MOTOR_TASK_VEHICLE_DRIVE_PERIOD_MS) != 0u)
        {
            Motor_Vehicle_Task(now_ms);
        }
        if (motor_task_period_elapsed(now_ms, &s_last_maintenance_drive_ms, MOTOR_TASK_MAINT_DRIVE_PERIOD_MS) != 0u)
        {
            Motor_Maintenance_Task(now_ms);
        }
        if (motor_task_record_due(now_ms, state) != 0u)
        {
            Motor_Record_Task(now_ms);
        }
        Motor_Buttons_Task(now_ms);
        Motor_UI_Task(now_ms);
        LED_App_Task(now_ms);
        break;

    case MOTOR_SCREEN_CORNER:
        /* ------------------------------------------------------------------ */
        /* 코너링 중심 화면                                                    */
        /* ------------------------------------------------------------------ */
        Motor_Dynamics_Task(now_ms);
        if (motor_task_period_elapsed(now_ms, &s_last_nav_drive_ms, MOTOR_TASK_NAV_DRIVE_PERIOD_MS) != 0u)
        {
            Motor_Navigation_Task(now_ms);
        }
        if (motor_task_record_due(now_ms, state) != 0u)
        {
            Motor_Record_Task(now_ms);
        }
        Motor_Buttons_Task(now_ms);
        Motor_UI_Task(now_ms);
        LED_App_Task(now_ms);
        break;

    case MOTOR_SCREEN_COMPASS:
    case MOTOR_SCREEN_BREADCRUMB:
    case MOTOR_SCREEN_ALTITUDE:
    case MOTOR_SCREEN_HORIZON:
        /* ------------------------------------------------------------------ */
        /* 항법 / 고도 / 자세 중심 화면                                        */
        /* ------------------------------------------------------------------ */
        Motor_Dynamics_Task(now_ms);
        if (motor_task_period_elapsed(now_ms, &s_last_nav_drive_ms, MOTOR_TASK_NAV_DRIVE_PERIOD_MS) != 0u)
        {
            Motor_Navigation_Task(now_ms);
        }
        if (motor_task_record_due(now_ms, state) != 0u)
        {
            Motor_Record_Task(now_ms);
        }
        Motor_Buttons_Task(now_ms);
        Motor_UI_Task(now_ms);
        LED_App_Task(now_ms);
        break;

    case MOTOR_SCREEN_VEHICLE_SUMMARY:
        /* ------------------------------------------------------------------ */
        /* 차량 요약 / trip computer                                           */
        /* ------------------------------------------------------------------ */
        if (motor_task_period_elapsed(now_ms, &s_last_nav_drive_ms, MOTOR_TASK_NAV_DRIVE_PERIOD_MS) != 0u)
        {
            Motor_Navigation_Task(now_ms);
        }
        if (motor_task_period_elapsed(now_ms, &s_last_vehicle_drive_ms, MOTOR_TASK_VEHICLE_DRIVE_PERIOD_MS) != 0u)
        {
            Motor_Vehicle_Task(now_ms);
        }
        if (motor_task_period_elapsed(now_ms, &s_last_maintenance_drive_ms, MOTOR_TASK_MAINT_DRIVE_PERIOD_MS) != 0u)
        {
            Motor_Maintenance_Task(now_ms);
        }
        if (motor_task_record_due(now_ms, state) != 0u)
        {
            Motor_Record_Task(now_ms);
        }
        Motor_Buttons_Task(now_ms);
        Motor_UI_Task(now_ms);
        LED_App_Task(now_ms);
        break;

    case MOTOR_SCREEN_MENU:
        /* ------------------------------------------------------------------ */
        /* 메인 메뉴                                                           */
        /* - breadcrumb 생성 / vehicle stub / maintenance 는 백그라운드로 유지 */
        /* - 다만 메뉴 화면에서는 매 superloop 전부 돌리지 않는다.              */
        /* ------------------------------------------------------------------ */
        if (motor_task_period_elapsed(now_ms, &s_last_nav_background_ms, MOTOR_TASK_NAV_BACKGROUND_PERIOD_MS) != 0u)
        {
            Motor_Navigation_Task(now_ms);
        }
        if (motor_task_period_elapsed(now_ms, &s_last_vehicle_background_ms, MOTOR_TASK_VEHICLE_BACKGROUND_PERIOD_MS) != 0u)
        {
            Motor_Vehicle_Task(now_ms);
        }
        if (motor_task_period_elapsed(now_ms, &s_last_maintenance_background_ms, MOTOR_TASK_MAINT_BACKGROUND_PERIOD_MS) != 0u)
        {
            Motor_Maintenance_Task(now_ms);
        }
        if (motor_task_record_due(now_ms, state) != 0u)
        {
            Motor_Record_Task(now_ms);
        }
        Motor_Buttons_Task(now_ms);
        Motor_UI_Task(now_ms);
        LED_App_Task(now_ms);
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
        /* ------------------------------------------------------------------ */
        /* 환경설정                                                            */
        /* ------------------------------------------------------------------ */
        if (motor_task_period_elapsed(now_ms, &s_last_nav_background_ms, MOTOR_TASK_NAV_BACKGROUND_PERIOD_MS) != 0u)
        {
            Motor_Navigation_Task(now_ms);
        }
        if (motor_task_period_elapsed(now_ms, &s_last_vehicle_background_ms, MOTOR_TASK_VEHICLE_BACKGROUND_PERIOD_MS) != 0u)
        {
            Motor_Vehicle_Task(now_ms);
        }
        if (motor_task_period_elapsed(now_ms, &s_last_maintenance_background_ms, MOTOR_TASK_MAINT_BACKGROUND_PERIOD_MS) != 0u)
        {
            Motor_Maintenance_Task(now_ms);
        }
        if (motor_task_record_due(now_ms, state) != 0u)
        {
            Motor_Record_Task(now_ms);
        }
        Motor_Buttons_Task(now_ms);
        Motor_UI_Task(now_ms);
        LED_App_Task(now_ms);
        break;

    case MOTOR_SCREEN_ACCEL_TEST_STUB:
    case MOTOR_SCREEN_LAP_TIMER_STUB:
    case MOTOR_SCREEN_LOG_VIEW_STUB:
    case MOTOR_SCREEN_OBD_CONNECT_STUB:
    case MOTOR_SCREEN_OBD_DTC_STUB:
    default:
        /* ------------------------------------------------------------------ */
        /* 아직 본기능이 올라오지 않은 stub page                                */
        /* ------------------------------------------------------------------ */
        if (motor_task_period_elapsed(now_ms, &s_last_nav_background_ms, MOTOR_TASK_NAV_BACKGROUND_PERIOD_MS) != 0u)
        {
            Motor_Navigation_Task(now_ms);
        }
        if (motor_task_period_elapsed(now_ms, &s_last_vehicle_background_ms, MOTOR_TASK_VEHICLE_BACKGROUND_PERIOD_MS) != 0u)
        {
            Motor_Vehicle_Task(now_ms);
        }
        if (motor_task_period_elapsed(now_ms, &s_last_maintenance_background_ms, MOTOR_TASK_MAINT_BACKGROUND_PERIOD_MS) != 0u)
        {
            Motor_Maintenance_Task(now_ms);
        }
        if (motor_task_record_due(now_ms, state) != 0u)
        {
            Motor_Record_Task(now_ms);
        }
        Motor_Buttons_Task(now_ms);
        Motor_UI_Task(now_ms);
        LED_App_Task(now_ms);
        break;
    }

    /* ---------------------------------------------------------------------- */
    /* 안정화 시간이 지난 뒤에만 boot confirmed를 세운다.                     */
    /* ---------------------------------------------------------------------- */
    if ((s_motor_boot_confirm_done == 0u) &&
        ((uint32_t)(now_ms - s_motor_boot_confirm_arm_ms) >= MOTOR_APP_BOOT_CONFIRM_DELAY_MS))
    {
        FW_AppGuard_ConfirmBootOk();
        s_motor_boot_confirm_done = 1u;
    }
}
