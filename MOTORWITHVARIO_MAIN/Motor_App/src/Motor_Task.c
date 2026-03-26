#include "Motor_Task.h"

#include "main.h"

#include "APP_ALTITUDE.h"
#include "APP_CLOCK.h"
#include "APP_SD.h"
#include "Audio_App.h"
#include "Audio_Driver.h"
#include "BACKLIGHT_App.h"
#include "BIKE_DYNAMICS.h"
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
/*  핵심 정책                                                                  */
/*  - screen switch-case는 계속 유지한다.                                      */
/*  - 하지만 navigation / vehicle / maintenance / recorder / dynamics 같은    */
/*    계산계는 화면 종류와 무관하게 상위 공통 경로에서 먼저 돌린다.            */
/*  - 그 위에 case별로 UI / 버튼 / 향후 page-specific hook 만 둔다.           */
/*                                                                            */
/*  이 구조가 필요한 이유                                                      */
/*  - settings/menu 화면으로 들어가도 dynamics가 절대 멈추지 않는다.          */
/*  - recorder는 항상 최신 dynamics/state를 먹는다.                           */
/*  - "top-level case screen_x" 구조는 유지하면서도,                          */
/*    계산 truth가 화면 전환에 종속되지 않게 만든다.                          */
/* -------------------------------------------------------------------------- */
#define MOTOR_TASK_RECORD_PERIOD_MS               50u
#define MOTOR_TASK_NAV_DRIVE_PERIOD_MS           200u
#define MOTOR_TASK_NAV_BACKGROUND_PERIOD_MS      500u
#define MOTOR_TASK_VEHICLE_DRIVE_PERIOD_MS       100u
#define MOTOR_TASK_VEHICLE_BACKGROUND_PERIOD_MS  200u
#define MOTOR_TASK_MAINT_DRIVE_PERIOD_MS         250u
#define MOTOR_TASK_MAINT_BACKGROUND_PERIOD_MS    500u

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

static uint8_t motor_task_is_drive_screen(motor_screen_t screen)
{
    return (screen < MOTOR_SCREEN_MENU) ? 1u : 0u;
}

/* -------------------------------------------------------------------------- */
/* 공용 하위 플랫폼 서비스 구간                                                */
/*                                                                            */
/* 규칙                                                                       */
/* - 이 구간은 screen mode와 무관하게 항상 돌아야 하는 저수준 service만 둔다.  */
/* - Motor_App는 여기서 raw 센서 레지스터를 직접 읽지 않는다.                  */
/* - 각 서비스는 APP_STATE를 최신값으로 publish 하고,                         */
/*   Motor_State_Task()가 그 snapshot을 memcpy해 온다.                        */
/*                                                                            */
/*  BIKE_DYNAMICS 위치가 중요한 이유                                          */
/*  - Ublox_GPS / GY86_IMU가 raw를 publish 한 다음                            */
/*  - shared BIKE_DYNAMICS가 그 raw를 읽어 g_app_state.bike를 publish 하고    */
/*  - 그 다음 Motor_State_Task()가 "완성된 snapshot" 한 벌을 복사한다.      */
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
    BIKE_DYNAMICS_Task(now_ms);
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
    /*  start / stop / marker는 cadence 지연 없이 즉시 반영한다.               */
    /* ---------------------------------------------------------------------- */
    if ((state->record.start_requested != false) ||
        (state->record.stop_requested != false) ||
        (state->record.marker_requested != false))
    {
        return 1u;
    }

    return motor_task_period_elapsed(now_ms, &s_last_record_ms, MOTOR_TASK_RECORD_PERIOD_MS);
}

static void motor_task_run_common_app_services(uint32_t now_ms, const motor_state_t *state)
{
    uint8_t drive_screen;

    if (state == 0)
    {
        return;
    }

    drive_screen = motor_task_is_drive_screen((motor_screen_t)state->ui.screen);

    /* ---------------------------------------------------------------------- */
    /*  1) Dynamics adapter는 어떤 screen에서도 절대 멈추지 않는다.            */
    /*     - low-level truth는 이미 snapshot.bike에 들어 있다.                */
    /*     - 여기서는 high-level mirror / peak / history만 유지한다.          */
    /* ---------------------------------------------------------------------- */
    Motor_Dynamics_Task(now_ms);

    /* ---------------------------------------------------------------------- */
    /*  2) 나머지 app 계산계도 screen-independent common path 에 둔다.         */
    /*     - 단, cadence는 drive / background에 따라 다르게 적용한다.         */
    /* ---------------------------------------------------------------------- */
    if (drive_screen != 0u)
    {
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
    }
    else
    {
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
    }

    if (motor_task_record_due(now_ms, state) != 0u)
    {
        Motor_Record_Task(now_ms);
    }
}

void Motor_App_Init(void)
{
    /* ---------------------------------------------------------------------- */
    /* Motor_App 초기화 순서                                                   */
    /* 1) 설정 저장소                                                          */
    /* 2) snapshot 기반 런타임 state                                           */
    /* 3) shared low-level bike dynamics                                       */
    /* 4) app-level adapter / feature module                                   */
    /* 5) 입력 해석 레이어                                                     */
    /* 6) UI facade / panel apply                                              */
    /* ---------------------------------------------------------------------- */
    Motor_Settings_Init();
    Motor_State_Init();
    BIKE_DYNAMICS_Init(HAL_GetTick());
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
    /*  Soft power overlay는 기존 구조상 LCD / button queue 우선권을 갖는다.   */
    /*  blocking 상태에서는 Motor 상태머신보다 먼저 return 한다.               */
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
    /*  board debug button은 메뉴 토글에만 사용한다.                           */
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
        state = Motor_State_Get();
    }

    /* ---------------------------------------------------------------------- */
    /*  screen-independent common telemetry / recorder path                    */
    /* ---------------------------------------------------------------------- */
    motor_task_run_common_app_services(now_ms, state);
    state = Motor_State_Get();
    if (state == 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  사용자 요구사항 핵심 구조                                               */
    /*  - screen mode는 한 개의 통짜 switch-case 상위 상태머신으로 유지한다.   */
    /*  - case 안에는 "화면별 조작 / UI hook"만 남긴다.                        */
    /*  - 계산 truth는 이미 common path에서 완료되어 case와 무관하게 갱신된다.  */
    /* ---------------------------------------------------------------------- */
    switch ((motor_screen_t)state->ui.screen)
    {
    case MOTOR_SCREEN_MAIN:
    case MOTOR_SCREEN_DATA_FIELD_1:
    case MOTOR_SCREEN_DATA_FIELD_2:
    case MOTOR_SCREEN_CORNER:
    case MOTOR_SCREEN_COMPASS:
    case MOTOR_SCREEN_BREADCRUMB:
    case MOTOR_SCREEN_ALTITUDE:
    case MOTOR_SCREEN_HORIZON:
    case MOTOR_SCREEN_VEHICLE_SUMMARY:
        /* ------------------------------------------------------------------ */
        /*  drive page                                                         */
        /*  - telemetry는 이미 common path에서 모두 계산 완료                  */
        /*  - 여기서는 운전자 interaction / draw / LED만 screen context에 맞춰 */
        /*    처리한다.                                                         */
        /* ------------------------------------------------------------------ */
        Motor_Buttons_Task(now_ms);
        Motor_UI_Task(now_ms);
        LED_App_Task(now_ms);
        break;

    case MOTOR_SCREEN_MENU:
        /* ------------------------------------------------------------------ */
        /*  메인 메뉴                                                          */
        /* ------------------------------------------------------------------ */
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
        /*  환경설정                                                           */
        /* ------------------------------------------------------------------ */
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
        /*  아직 본기능이 올라오지 않은 stub page                               */
        /* ------------------------------------------------------------------ */
        Motor_Buttons_Task(now_ms);
        Motor_UI_Task(now_ms);
        LED_App_Task(now_ms);
        break;
    }

    /* ---------------------------------------------------------------------- */
    /*  안정화 시간이 지난 뒤에만 boot confirmed를 세운다.                     */
    /* ---------------------------------------------------------------------- */
    if ((s_motor_boot_confirm_done == 0u) &&
        ((uint32_t)(now_ms - s_motor_boot_confirm_arm_ms) >= MOTOR_APP_BOOT_CONFIRM_DELAY_MS))
    {
        FW_AppGuard_ConfirmBootOk();
        s_motor_boot_confirm_done = 1u;
    }
}
