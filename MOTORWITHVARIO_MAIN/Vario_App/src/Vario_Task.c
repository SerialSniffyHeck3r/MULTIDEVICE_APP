#include "Vario_Task.h"

#include "main.h"

#include "APP_ALTITUDE.h"
#include "APP_CLOCK.h"
#include "APP_SD.h"
#include "APP_STATE.h"
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
#include "u8g2_uc1608_stm32.h"
#include "ui_engine.h"

#include "Vario_Audio.h"
#include "Vario_Button.h"
#include "Vario_Dev.h"
#include "Vario_Display.h"
#include "Vario_Settings.h"
#include "Vario_State.h"

#include <stdbool.h>

#ifndef VARIO_APP_BOOT_CONFIRM_DELAY_MS
#define VARIO_APP_BOOT_CONFIRM_DELAY_MS 2000u
#endif

/* -------------------------------------------------------------------------- */
/* 부팅 확정 지연 latch                                                        */
/*                                                                            */
/* main.c 에 남아 있는 기존 app_boot_confirm 변수와는 별개로,                    */
/* 실제 현재 런타임 while(1) 을 대체하는 Vario_App_Task() 안에서                 */
/* 부팅 확정을 관리한다.                                                        */
/* -------------------------------------------------------------------------- */
static uint32_t s_vario_boot_confirm_arm_ms;
static uint8_t  s_vario_boot_confirm_done;
static uint8_t  s_vario_last_backlight_mode = 0xFFu;
static uint16_t s_vario_last_backlight_permille = 0xFFFFu;
static uint8_t  s_vario_last_contrast_raw = 0xFFu;
static uint8_t  s_vario_last_temp_comp = 0xFFu;
static uint8_t  s_vario_last_bt_echo = 0xFFu;
static uint8_t  s_vario_last_bt_autoping = 0xFFu;

/* -------------------------------------------------------------------------- */
/* 내부 helper 선언                                                            */
/* -------------------------------------------------------------------------- */
static void vario_task_run_shared_platform_services(uint32_t now_ms);
static void vario_task_run_legacy_profile(uint32_t now_ms);
static void vario_task_run_mode_state_machine(uint32_t now_ms);
static void vario_task_apply_platform_settings(void);

/* -------------------------------------------------------------------------- */
/* 공용 플랫폼 서비스 구간                                                     */
/*                                                                            */
/* 여기에는 mode 와 무관하게 항상 돌아야 하는 하위 task 들만 둔다.               */
/*                                                                            */
/* 중요한 계층 규칙                                                              */
/* - 상위 VARIO 상태머신이 센서 레지스터를 직접 읽지 않는다.                    */
/* - 아래 공용 서비스 task 들이 APP_STATE 를 최신으로 publish 한다.             */
/* - VARIO 는 그 APP_STATE snapshot 을 Vario_State_Task() 에서 복사해 쓴다.      */
/* -------------------------------------------------------------------------- */
static void vario_task_run_shared_platform_services(uint32_t now_ms)
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
    BIKE_DYNAMICS_Task(now_ms);
    Backlight_App_Task(now_ms);

    /* ---------------------------------------------------------------------- */
    /* Button_Task() 는 EXTI raw edge 를                                        */
    /* debounce 된 short/long event queue 로 승격시키는 공용 서비스다.          */
    /*                                                                          */
    /* 이후                                                                        */
    /* - legacy boot profile 에서는 UI engine 이 event queue 를 소비하고        */
    /* - normal VARIO boot profile 에서는 Vario_Button_Task() 가 소비한다.      */
    /* ---------------------------------------------------------------------- */
    Button_Task(now_ms);
}

/* -------------------------------------------------------------------------- */
/* 플랫폼 반영 helper                                                          */
/*                                                                            */
/* 설정 저장소 자체는 하위 출력 드라이버를 직접 건드리지 않는다.               */
/* 현재 task 가 settings snapshot 을 읽어 플랫폼 API 로 반영한다.              */
/* -------------------------------------------------------------------------- */
static void vario_task_apply_platform_settings(void)
{
    const vario_settings_t *settings;
    app_settings_t          platform_settings;
    uint16_t                permille;
    uint8_t                 bt_echo;
    uint8_t                 bt_autoping;
    uint8_t                 platform_settings_dirty;
    uint8_t                 uc1608_dirty;

    settings = Vario_Settings_Get();
    if (settings == NULL)
    {
        return;
    }

    APP_STATE_CopySettingsSnapshot(&platform_settings);

    platform_settings_dirty = 0u;
    uc1608_dirty = 0u;

    switch (settings->display_backlight_mode)
    {
        case VARIO_BACKLIGHT_MODE_AUTO_DAY_NIGHT:
            if (platform_settings.backlight.auto_mode != (uint8_t)APP_BACKLIGHT_AUTO_MODE_DIMMER)
            {
                platform_settings.backlight.auto_mode = (uint8_t)APP_BACKLIGHT_AUTO_MODE_DIMMER;
                platform_settings_dirty = 1u;
            }
            break;

        case VARIO_BACKLIGHT_MODE_MANUAL:
            break;

        case VARIO_BACKLIGHT_MODE_AUTO_CONTINUOUS:
        case VARIO_BACKLIGHT_MODE_COUNT:
        default:
            if (platform_settings.backlight.auto_mode != (uint8_t)APP_BACKLIGHT_AUTO_MODE_CONTINUOUS)
            {
                platform_settings.backlight.auto_mode = (uint8_t)APP_BACKLIGHT_AUTO_MODE_CONTINUOUS;
                platform_settings_dirty = 1u;
            }
            break;
    }

    if (platform_settings.uc1608.contrast != settings->display_contrast_raw)
    {
        platform_settings.uc1608.contrast = settings->display_contrast_raw;
        platform_settings_dirty = 1u;
        uc1608_dirty = 1u;
    }

    if (platform_settings.uc1608.temperature_compensation != settings->display_temp_compensation)
    {
        platform_settings.uc1608.temperature_compensation = settings->display_temp_compensation;
        platform_settings_dirty = 1u;
        uc1608_dirty = 1u;
    }

    if (platform_settings_dirty != 0u)
    {
        APP_STATE_StoreSettingsSnapshot(&platform_settings);
    }

    if (settings->display_backlight_mode == VARIO_BACKLIGHT_MODE_MANUAL)
    {
        permille = (uint16_t)settings->display_brightness_percent * 10u;
        if (permille > 1000u)
        {
            permille = 1000u;
        }

        if ((s_vario_last_backlight_mode != (uint8_t)settings->display_backlight_mode) ||
            (permille != s_vario_last_backlight_permille))
        {
            Backlight_App_SetManualBrightnessPermille(permille);
            s_vario_last_backlight_permille = permille;
        }
    }
    else if (s_vario_last_backlight_mode != (uint8_t)settings->display_backlight_mode)
    {
        Backlight_App_SetAutoEnabled(true);
    }

    s_vario_last_backlight_mode = (uint8_t)settings->display_backlight_mode;

    if ((uc1608_dirty != 0u) ||
        (settings->display_contrast_raw != s_vario_last_contrast_raw) ||
        (settings->display_temp_compensation != s_vario_last_temp_comp))
    {
        U8G2_UC1608_LoadAndApplySettingsFromAppState();
        s_vario_last_contrast_raw = settings->display_contrast_raw;
        s_vario_last_temp_comp = settings->display_temp_compensation;
    }

    bt_echo = (settings->bluetooth_echo_enabled != 0u) ? 1u : 0u;
    if (bt_echo != s_vario_last_bt_echo)
    {
        Bluetooth_SetEchoEnabled(bt_echo != 0u);
        s_vario_last_bt_echo = bt_echo;
    }

    bt_autoping = (settings->bluetooth_auto_ping_enabled != 0u) ? 1u : 0u;
    if (bt_autoping != s_vario_last_bt_autoping)
    {
        Bluetooth_SetAutoPingEnabled(bt_autoping != 0u);
        s_vario_last_bt_autoping = bt_autoping;
    }
}

/* -------------------------------------------------------------------------- */
/* Legacy boot profile                                                         */
/*                                                                            */
/* F3 held boot 로 들어온 경우                                                  */
/* - TEST / DEBUG / ENGINE OIL / GPS 같은 기존 UI 묶음만 살린다.               */
/* - VARIO mode state machine 은 버튼 이벤트를 절대 소비하지 않는다.            */
/* - variometer tone ownership 도 끈다.                                        */
/* -------------------------------------------------------------------------- */
static void vario_task_run_legacy_profile(uint32_t now_ms)
{
    APP_ALTITUDE_DebugSetUiActive(false, now_ms);
    UI_Engine_RequestRedraw();
    UI_Engine_Task(now_ms);
}

/* -------------------------------------------------------------------------- */
/* VARIO 상위 상태머신                                                          */
/*                                                                            */
/* 사용자가 요구한 핵심 구조                                                     */
/* - "현재 mode 하나가 자기 책임 task 묶음을 가진다"                            */
/* - "screen mode 는 상태머신 switch-case 가 결정한다"                          */
/* - 실제 픽셀 draw 는 UI engine root compose 가 수행한다"                      */
/*                                                                            */
/* 현재 호출 흐름                                                                */
/*   Vario_App_Task()                                                           */
/*      -> Vario_State_Task()                                                   */
/*      -> vario_task_run_mode_state_machine()                                  */
/*         -> mode 별 버튼/오디오/부가 task                                     */
/*      -> UI_Engine_Task()                                                     */
/*         -> ui_engine_draw_root()                                             */
/*            -> UI_ScreenVario_Draw()                                          */
/*               -> Vario_Display_RenderCurrent()                               */
/*                  -> 각 Vario_Screen renderer                                 */
/* -------------------------------------------------------------------------- */
static void vario_task_run_mode_state_machine(uint32_t now_ms)
{
    switch (Vario_State_GetMode())
    {
    case VARIO_MODE_SCREEN_1:
        /* ------------------------------------------------------------------ */
        /* SCREEN 1                                                            */
        /* - 바리오 메인 full-screen page                                      */
        /* - status bar 없음                                                    */
        /* - bottom bar 없음                                                    */
        /* - tone ownership 있음                                                */
        /* - 실제 renderer 는 UI_ScreenVario_GetLayoutMode() 가                */
        /*   FULLSCREEN 을 반환함으로써 자동 결정된다.                         */
        /* ------------------------------------------------------------------ */
        Vario_Button_Task(now_ms);
        Vario_Audio_Task(now_ms);
        UI_Engine_RequestRedraw();
        break;

    case VARIO_MODE_SCREEN_2:
        /* ------------------------------------------------------------------ */
        /* SCREEN 2                                                            */
        /* - 바리오 메인 full-screen page                                      */
        /* - status bar 없음                                                    */
        /* - bottom bar 없음                                                    */
        /* - tone ownership 있음                                                */
        /* ------------------------------------------------------------------ */
        Vario_Button_Task(now_ms);
        Vario_Audio_Task(now_ms);
        UI_Engine_RequestRedraw();
        break;

    case VARIO_MODE_SCREEN_3:
        /* ------------------------------------------------------------------ */
        /* SCREEN 3                                                            */
        /* - 바리오 메인 full-screen page                                      */
        /* - status bar 없음                                                    */
        /* - bottom bar 없음                                                    */
        /* - tone ownership 있음                                                */
        /* ------------------------------------------------------------------ */
        Vario_Button_Task(now_ms);
        Vario_Audio_Task(now_ms);
        UI_Engine_RequestRedraw();
        break;

    case VARIO_MODE_SETTING:
        /* ------------------------------------------------------------------ */
        /* SETTING                                                             */
        /* - status bar 있음                                                    */
        /* - bottom bar 있음                                                    */
        /* - 하단 문자열은 UI_ScreenVario -> UI_BottomBar API 로 정의됨         */
        /* - 설정 화면에서는 variometer tone ownership 해제                     */
        /* ------------------------------------------------------------------ */
        Vario_Button_Task(now_ms);
        Vario_Audio_Task(now_ms);
        UI_Engine_RequestRedraw();
        break;

    case VARIO_MODE_QUICKSET:
        /* ------------------------------------------------------------------ */
        /* QUICKSET                                                            */
        /* - status bar 있음                                                    */
        /* - bottom bar 있음                                                    */
        /* - 하단 문자열은 UI_ScreenVario -> UI_BottomBar API 로 정의됨         */
        /* - 설정 계열이므로 tone ownership 해제                                */
        /* ------------------------------------------------------------------ */
        Vario_Button_Task(now_ms);
        Vario_Audio_Task(now_ms);
        UI_Engine_RequestRedraw();
        break;

    case VARIO_MODE_VALUESETTING:
        /* ------------------------------------------------------------------ */
        /* VALUESETTING                                                        */
        /* - status bar 있음                                                    */
        /* - bottom bar 있음                                                    */
        /* - 하단 문자열은 UI_ScreenVario -> UI_BottomBar API 로 정의됨         */
        /* - 설정 계열이므로 tone ownership 해제                                */
        /* ------------------------------------------------------------------ */
        Vario_Button_Task(now_ms);
        Vario_Audio_Task(now_ms);
        UI_Engine_RequestRedraw();
        break;

    case VARIO_MODE_COUNT:
    default:
        /* ------------------------------------------------------------------ */
        /* 비정상 mode 방어                                                    */
        /* - screen1 로 되돌리고 한 프레임 redraw 요청                          */
        /* ------------------------------------------------------------------ */
        Vario_State_SetMode(VARIO_MODE_SCREEN_1);
        Vario_Audio_Task(now_ms);
        UI_Engine_RequestRedraw();
        break;
    }
}

void Vario_App_Init(void)
{
    /* ---------------------------------------------------------------------- */
    /* VARIO 앱 로컬 초기화 순서                                                */
    /*                                                                          */
    /* 1) 사용자 설정 저장소                                                    */
    /* 2) 개발/오버레이 플래그                                                  */
    /* 3) APP_STATE snapshot 기반 런타임 저장소                                 */
    /* 4) 버튼 이벤트 소비 레이어                                               */
    /* 5) 오디오 ownership / 볼륨 동기화 레이어                                */
    /* 6) display facade                                                        */
    /*                                                                          */
    /* 이 순서는 "데이터 -> 입력 -> 출력" 흐름을 맞춰                           */
    /* 첫 정상 프레임이 안정적으로 나오게 하기 위한 것이다.                     */
    /* ---------------------------------------------------------------------- */
    Vario_Settings_Init();
    Vario_Dev_Init();
    Vario_State_Init();
    Vario_Button_Init();
    Vario_Audio_Init();
    Vario_Display_Init();

    s_vario_boot_confirm_arm_ms = HAL_GetTick();
    s_vario_boot_confirm_done = 0u;
}

void Vario_App_Task(void)
{
    uint32_t now_ms;

    now_ms = HAL_GetTick();

    /* watchdog kick 은 루프 최상단 */
    FW_AppGuard_Kick();

    /* 공용 하위 서비스 구간 */
    vario_task_run_shared_platform_services(now_ms);
    vario_task_apply_platform_settings();

    /* ---------------------------------------------------------------------- */
    /* Soft Power overlay 상태머신                                              */
    /*                                                                          */
    /* 이 모듈은 현재 구조상 자기 오버레이를 직접 draw/consume 한다.            */
    /* 따라서 blocking 상태일 때는 UI engine / VARIO mode state machine 보다    */
    /* 우선권을 가진다.                                                          */
    /* ---------------------------------------------------------------------- */
    POWER_STATE_Task(now_ms);

    if (POWER_STATE_IsUiBlocking() != false)
    {
        /* ------------------------------------------------------------------ */
        /* 전원 오버레이가 LCD / 버튼 queue 를 점유하는 동안에는                */
        /* VARIO tone ownership 을 반드시 해제한다.                            */
        /* ------------------------------------------------------------------ */
        APP_ALTITUDE_DebugSetUiActive(false, now_ms);
    }
    else if (UI_Engine_IsLegacyBootMode() != 0u)
    {
        /* ------------------------------------------------------------------ */
        /* F3 held boot legacy profile                                         */
        /* ------------------------------------------------------------------ */
        vario_task_run_legacy_profile(now_ms);
    }
    else
    {
        /* ------------------------------------------------------------------ */
        /* 정상 VARIO boot profile                                             */
        /*                                                                          */
        /* Vario_State_Task() 는                                                 */
        /* - APP_STATE snapshot 복사                                            */
        /* - baro altitude / vario / GS 파생 계산                               */
        /* - redraw request 갱신                                                */
        /* 을 수행한다.                                                         */
        /* ------------------------------------------------------------------ */
        Vario_State_Task(now_ms);

        /* ------------------------------------------------------------------ */
        /* 사용자가 요청한 "통짜 상위 상태머신 switch-case" 구간                */
        /* ------------------------------------------------------------------ */
        vario_task_run_mode_state_machine(now_ms);

        /* ------------------------------------------------------------------ */
        /* 실제 root compose + buffer commit                                   */
        /*                                                                          */
        /* 중요                                                                  */
        /* - 이 호출 한 번만이 LCD 프레임을 최종 완성한다.                      */
        /* - VARIO renderer 는 더 이상 직접 clear/commit 하지 않는다.          */
        /* ------------------------------------------------------------------ */
        UI_Engine_Task(now_ms);
    }

    /* 마지막으로 LED 정책 */
    LED_App_Task(now_ms);

    /* ---------------------------------------------------------------------- */
    /* 안정화 시간이 지난 뒤에만 boot confirmed latch 를 올린다.                */
    /* ---------------------------------------------------------------------- */
    if ((s_vario_boot_confirm_done == 0u) &&
        ((uint32_t)(now_ms - s_vario_boot_confirm_arm_ms) >= VARIO_APP_BOOT_CONFIRM_DELAY_MS))
    {
        FW_AppGuard_ConfirmBootOk();
        s_vario_boot_confirm_done = 1u;
    }
}
