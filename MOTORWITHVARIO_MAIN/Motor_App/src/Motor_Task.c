
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

static uint32_t s_motor_boot_confirm_arm_ms;
static uint8_t  s_motor_boot_confirm_done;
static uint8_t  s_board_debug_irq_pending;

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

void Motor_App_Init(void)
{
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
    motor_state_t *state;

    now_ms = HAL_GetTick();
    FW_AppGuard_Kick();

    motor_task_run_shared_platform_services(now_ms);
    POWER_STATE_Task(now_ms);
    if (POWER_STATE_IsUiBlocking() != false)
    {
        return;
    }

    Motor_State_Task(now_ms);
    Motor_Dynamics_Task(now_ms);
    Motor_Vehicle_Task(now_ms);
    Motor_Navigation_Task(now_ms);
    Motor_Maintenance_Task(now_ms);
    Motor_Record_Task(now_ms);

    state = Motor_State_GetMutable();
    if ((state != 0) && (s_board_debug_irq_pending != 0u))
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

    Motor_Buttons_Task(now_ms);
    Motor_UI_Task(now_ms);
    LED_App_Task(now_ms);

    if ((s_motor_boot_confirm_done == 0u) &&
        ((uint32_t)(now_ms - s_motor_boot_confirm_arm_ms) >= MOTOR_APP_BOOT_CONFIRM_DELAY_MS))
    {
        FW_AppGuard_ConfirmBootOk();
        s_motor_boot_confirm_done = 1u;
    }
}
