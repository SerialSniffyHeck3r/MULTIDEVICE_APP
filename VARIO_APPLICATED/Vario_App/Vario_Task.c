#include "Vario_Task.h"

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

#include "Vario_Audio.h"
#include "Vario_Button.h"
#include "Vario_Dev.h"
#include "Vario_Display.h"
#include "Vario_Settings.h"
#include "Vario_State.h"

#include <stdint.h>

#ifndef VARIO_APP_BOOT_CONFIRM_DELAY_MS
#define VARIO_APP_BOOT_CONFIRM_DELAY_MS 2000u
#endif

/* -------------------------------------------------------------------------- */
/*  부팅 확정 지연 latch                                                       */
/*                                                                            */
/*  main.c 가 기존에 하던 "정상 부팅 확정 지연" 정책을                         */
/*  Vario_App_Task() 안으로 그대로 흡수한다.                                   */
/* -------------------------------------------------------------------------- */
static uint32_t s_vario_boot_confirm_arm_ms;
static uint8_t  s_vario_boot_confirm_done;

void Vario_App_Init(void)
{
    /* ---------------------------------------------------------------------- */
    /*  바리오 앱 로컬 상태 초기화 순서                                        */
    /*                                                                        */
    /*  1) 사용자 설정 저장소                                                  */
    /*  2) 개발/오버레이 플래그                                                */
    /*  3) APP_STATE 기반 런타임/파생값 저장소                                  */
    /*  4) 버튼 이벤트 소비 레이어                                              */
    /*  5) 오디오 UI 게이트                                                     */
    /*  6) 화면 렌더러                                                          */
    /*                                                                        */
    /*  이 순서는 "데이터 → 입력 → 출력" 흐름을 맞춰                          */
    /*  화면 첫 프레임이 안정적으로 나오게 하기 위한 것이다.                    */
    /* ---------------------------------------------------------------------- */
    Vario_Settings_Init();
    Vario_Dev_Init();
    Vario_State_Init();
    Vario_Button_Init();
    Vario_Audio_Init();
    Vario_Display_Init();

    s_vario_boot_confirm_arm_ms = HAL_GetTick();
    s_vario_boot_confirm_done   = 0u;
}

void Vario_App_Task(void)
{
    uint32_t now_ms;

    now_ms = HAL_GetTick();

    /* ---------------------------------------------------------------------- */
    /*  watchdog 은 루프 최상단에서 항상 먼저 kick 한다.                        */
    /* ---------------------------------------------------------------------- */
    FW_AppGuard_Kick();

    /* ---------------------------------------------------------------------- */
    /*  기존 main.c 서비스 루프를 여기로 흡수한다.                              */
    /*                                                                        */
    /*  중요 원칙                                                               */
    /*  - 바리오 앱이 센서 드라이버를 직접 읽는 것은 금지한다.                  */
    /*  - 대신 아래 공용 서비스 task 들이 APP_STATE 를 최신으로 갱신하고,       */
    /*    Vario_State 가 그 APP_STATE snapshot 을 복사해서 사용한다.            */
    /* ---------------------------------------------------------------------- */
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
    BIKE_DYNAMICS_Task(now_ms);
    Backlight_App_Task(now_ms);
    Button_Task(now_ms);

    /* ---------------------------------------------------------------------- */
    /*  Soft Power 오버레이 상태머신도 공용 서비스이므로                          */
    /*  앱 내부에서 함께 돌린다.                                                */
    /* ---------------------------------------------------------------------- */
    POWER_STATE_Task(now_ms);

    /* ---------------------------------------------------------------------- */
    /*  바리오 앱 상위 상태 갱신                                                */
    /*  - APP_STATE snapshot 복사                                               */
    /*  - baro altitude / vario / GS 파생                                       */
    /*  - redraw request 갱신                                                   */
    /* ---------------------------------------------------------------------- */
    Vario_State_Task(now_ms);

    /* ---------------------------------------------------------------------- */
    /*  전원 오버레이가 화면을 점유 중이면                                      */
    /*  바리오 UI 입력과 렌더링은 잠시 멈춘다.                                  */
    /* ---------------------------------------------------------------------- */
    if (POWER_STATE_IsUiBlocking() == false)
    {
        Vario_Button_Task(now_ms);
        Vario_Audio_Task(now_ms);
        Vario_Display_Task(now_ms);
    }
    else
    {
        /* ------------------------------------------------------------------ */
        /*  전원 오버레이가 떠 있는 동안에는                                     */
        /*  바리오 비프를 즉시 비활성화해서                                      */
        /*  UI ownership 충돌을 피한다.                                          */
        /* ------------------------------------------------------------------ */
        APP_ALTITUDE_DebugSetUiActive(false, now_ms);
    }

    /* ---------------------------------------------------------------------- */
    /*  동일 펌웨어 안에서 Moto_App 를 다시 붙일 때는                           */
    /*  여기 아래에 조건부 호출 블록을 되살리면 된다.                           */
    /* ---------------------------------------------------------------------- */
    /* Moto_App_Task(now_ms); */

    /* ---------------------------------------------------------------------- */
    /*  마지막으로 LED 정책 task                                                */
    /* ---------------------------------------------------------------------- */
    LED_App_Task(now_ms);

    /* ---------------------------------------------------------------------- */
    /*  짧은 안정화 시간이 지난 뒤에만 "정상 부팅 완료" 를 확정한다.            */
    /* ---------------------------------------------------------------------- */
    if ((s_vario_boot_confirm_done == 0u) &&
        ((uint32_t)(now_ms - s_vario_boot_confirm_arm_ms) >= VARIO_APP_BOOT_CONFIRM_DELAY_MS))
    {
        FW_AppGuard_ConfirmBootOk();
        s_vario_boot_confirm_done = 1u;
    }
}
