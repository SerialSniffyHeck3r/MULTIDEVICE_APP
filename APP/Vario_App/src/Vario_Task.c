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
#include <stddef.h>

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

static float vario_task_clampf(float value, float min_v, float max_v)
{
    if (value < min_v)
    {
        return min_v;
    }

    if (value > max_v)
    {
        return max_v;
    }

    return value;
}

static uint16_t vario_task_lerp_u16(uint16_t slow_value,
                                    uint16_t fast_value,
                                    float response_norm)
{
    float value;

    response_norm = vario_task_clampf(response_norm, 0.0f, 1.0f);
    value = ((float)slow_value) + (((float)fast_value - (float)slow_value) * response_norm);

    if (value < 0.0f)
    {
        return 0u;
    }

    return (uint16_t)(value + 0.5f);
}

static float vario_task_damping_norm_from_settings(const vario_settings_t *settings)
{
    uint8_t damping_level;

    if (settings == NULL)
    {
        return 0.6667f;
    }

    damping_level = settings->vario_damping_level;
    if (damping_level < 1u)
    {
        damping_level = 1u;
    }
    else if (damping_level > 10u)
    {
        damping_level = 10u;
    }

    return ((float)(damping_level - 1u)) / 9.0f;
}

static float vario_task_audio_response_norm_from_settings(const vario_settings_t *settings)
{
    uint8_t response_level;

    if (settings == NULL)
    {
        return 0.6667f;
    }

    response_level = settings->audio_response_level;
    if (response_level < 1u)
    {
        response_level = 1u;
    }
    else if (response_level > 10u)
    {
        response_level = 10u;
    }

    return ((float)(response_level - 1u)) / 9.0f;
}

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
    float                   damping_norm;
    float                   audio_response_norm;
    uint16_t                fast_tau_ms;
    uint16_t                baro_vario_tau_ms;
    uint16_t                baro_vario_noise_cms;
    uint16_t                audio_deadband_cms;
    uint16_t                audio_min_freq_hz;
    uint16_t                audio_max_freq_hz;
    uint16_t                audio_repeat_ms;
    uint16_t                audio_beep_ms;

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

    /* ---------------------------------------------------------------------- */
    /*  VARIO high-level settings -> APP_STATE low-level mirror               */
    /*                                                                        */
    /*  APP_STATE는 low-level 자료창고이고,                                   */
    /*  Vario_Settings는 사용자 의도 저장소다.                                */
    /*                                                                        */
    /*  따라서 여기서는 "상위 사용자의 의도"를                               */
    /*  - low-level fast/slow tau                                              */
    /*  - debug/audio snapshot                                                 */
    /*  - driver-adjacent mirror 값                                            */
    /*  으로 번역해서 APP_STATE에 보관만 한다.                                 */
    /*                                                                        */
    /*  실제 제품용 바리오 오디오는 Vario_Audio가 직접 settings를 읽어         */
    /*  Audio_App를 호출하지만, APP_STATE mirror를 같이 맞춰 두면             */
    /*  - debug page                                                          */
    /*  - snapshot dump                                                       */
    /*  - 향후 하위 레이어 진단                                               */
    /*  이 전부 같은 값을 보게 된다.                                          */
    /* ---------------------------------------------------------------------- */
    /* ------------------------------------------------------------------ */
    /*  의미 정리                                                            */
    /*                                                                      */
    /*  - damping         : display / publish / low-level tau-noise mirror  */
    /*  - audio response  : audio cadence / follow-up mirror                */
    /*                                                                      */
    /*  이전 구현은 두 knob를 둘 다 노출해 놓고도 APP_STATE mirror 갱신 시     */
    /*  vario_damping_level 하나로 전부 번역해 버려 semantic mismatch가       */
    /*  있었다. 여기서는 두 의미를 분리해 mirror도 실제 제품 설정 의미와      */
    /*  일치시키도록 정리한다.                                                */
    /* ------------------------------------------------------------------ */
    damping_norm = vario_task_damping_norm_from_settings(settings);
    audio_response_norm = vario_task_audio_response_norm_from_settings(settings);
    fast_tau_ms = vario_task_lerp_u16(170u, 70u, damping_norm);
    baro_vario_tau_ms = vario_task_lerp_u16(95u, 38u, damping_norm);
    baro_vario_noise_cms = vario_task_lerp_u16(72u, 50u, damping_norm);

    audio_deadband_cms = (uint16_t)((settings->climb_tone_threshold_cms > 0) ?
                                    settings->climb_tone_threshold_cms :
                                    8);
    if (audio_deadband_cms < 8u)
    {
        audio_deadband_cms = 8u;
    }
    else if (audio_deadband_cms > 25u)
    {
        audio_deadband_cms = 25u;
    }

    audio_min_freq_hz = 225u;
    audio_max_freq_hz = 1820u;
    audio_repeat_ms   = vario_task_lerp_u16(310u, 140u, audio_response_norm);
    audio_beep_ms     = vario_task_lerp_u16(105u, 78u, audio_response_norm);

    if (platform_settings.altitude.vario_fast_tau_ms != fast_tau_ms)
    {
        platform_settings.altitude.vario_fast_tau_ms = fast_tau_ms;
        platform_settings_dirty = 1u;
    }

    if (platform_settings.altitude.baro_vario_lpf_tau_ms != baro_vario_tau_ms)
    {
        platform_settings.altitude.baro_vario_lpf_tau_ms = baro_vario_tau_ms;
        platform_settings_dirty = 1u;
    }

    if (platform_settings.altitude.baro_vario_measurement_noise_cms != baro_vario_noise_cms)
    {
        platform_settings.altitude.baro_vario_measurement_noise_cms = baro_vario_noise_cms;
        platform_settings_dirty = 1u;
    }

    if (platform_settings.altitude.pressure_correction_hpa_x100 != settings->pressure_correction_hpa_x100)
    {
        platform_settings.altitude.pressure_correction_hpa_x100 = settings->pressure_correction_hpa_x100;
        platform_settings_dirty = 1u;
    }

    if (platform_settings.altitude.imu_aid_enabled !=
        ((settings->imu_assist_mode == VARIO_IMU_ASSIST_AUTO) ? 1u : 0u))
    {
        platform_settings.altitude.imu_aid_enabled =
            (settings->imu_assist_mode == VARIO_IMU_ASSIST_AUTO) ? 1u : 0u;
        platform_settings_dirty = 1u;
    }

    if (platform_settings.altitude.debug_audio_enabled != ((settings->audio_enabled != 0u) ? 1u : 0u))
    {
        platform_settings.altitude.debug_audio_enabled = (settings->audio_enabled != 0u) ? 1u : 0u;
        platform_settings_dirty = 1u;
    }

    if (platform_settings.altitude.debug_audio_source !=
        ((settings->imu_assist_mode == VARIO_IMU_ASSIST_AUTO) ? 1u : 0u))
    {
        platform_settings.altitude.debug_audio_source =
            (settings->imu_assist_mode == VARIO_IMU_ASSIST_AUTO) ? 1u : 0u;
        platform_settings_dirty = 1u;
    }

    if (platform_settings.altitude.audio_deadband_cms != audio_deadband_cms)
    {
        platform_settings.altitude.audio_deadband_cms = audio_deadband_cms;
        platform_settings_dirty = 1u;
    }

    if (platform_settings.altitude.audio_min_freq_hz != audio_min_freq_hz)
    {
        platform_settings.altitude.audio_min_freq_hz = audio_min_freq_hz;
        platform_settings_dirty = 1u;
    }

    if (platform_settings.altitude.audio_max_freq_hz != audio_max_freq_hz)
    {
        platform_settings.altitude.audio_max_freq_hz = audio_max_freq_hz;
        platform_settings_dirty = 1u;
    }

    if (platform_settings.altitude.audio_repeat_ms != audio_repeat_ms)
    {
        platform_settings.altitude.audio_repeat_ms = audio_repeat_ms;
        platform_settings_dirty = 1u;
    }

    if (platform_settings.altitude.audio_beep_ms != audio_beep_ms)
    {
        platform_settings.altitude.audio_beep_ms = audio_beep_ms;
        platform_settings_dirty = 1u;
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
    /* ---------------------------------------------------------------------- */
    /* legacy root family 는 버튼 이벤트 / blink phase / status 변화에 따라     */
    /* UI engine 내부에서 자체 redraw 판단을 한다.                             */
    /*                                                                          */
    /* 따라서 여기서는 더 이상 매 루프 강제 redraw 를 넣지 않는다.             */
    /* variometer ownership 해제만 유지하고, 실제 draw 여부는 엔진에게 맡긴다. */
    /* ---------------------------------------------------------------------- */
    Audio_App_ReleaseVariometer(now_ms);
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
        /*                                                                          */
        /* redraw 정책                                                           */
        /* - baro / GS / filtered altitude 의 실질 publish cadence 는           */
        /*   Vario_State_Task() 내부 10Hz / 5Hz scheduler 가 소유한다.          */
        /* - 따라서 상위 상태머신은 더 이상 매 루프 UI_Engine_RequestRedraw()   */
        /*   를 던지지 않는다.                                                  */
        /* ------------------------------------------------------------------ */
        Vario_Button_Task(now_ms);
        Vario_Audio_Task(now_ms);
        break;

    case VARIO_MODE_SCREEN_2:
        /* ------------------------------------------------------------------ */
        /* SCREEN 2                                                            */
        /* ------------------------------------------------------------------ */
        Vario_Button_Task(now_ms);
        Vario_Audio_Task(now_ms);
        break;

    case VARIO_MODE_SCREEN_3:
        /* ------------------------------------------------------------------ */
        /* SCREEN 3                                                            */
        /* ------------------------------------------------------------------ */
        Vario_Button_Task(now_ms);
        Vario_Audio_Task(now_ms);
        break;

    case VARIO_MODE_SETTING:
        /* ------------------------------------------------------------------ */
        /* SETTING                                                             */
        /* ------------------------------------------------------------------ */
        Vario_Button_Task(now_ms);
        Vario_Audio_Task(now_ms);
        break;

    case VARIO_MODE_QUICKSET:
        /* ------------------------------------------------------------------ */
        /* QUICKSET                                                            */
        /* ------------------------------------------------------------------ */
        Vario_Button_Task(now_ms);
        Vario_Audio_Task(now_ms);
        break;

    case VARIO_MODE_VALUESETTING:
        /* ------------------------------------------------------------------ */
        /* VALUESETTING                                                        */
        /* ------------------------------------------------------------------ */
        Vario_Button_Task(now_ms);
        Vario_Audio_Task(now_ms);
        break;

    case VARIO_MODE_COUNT:
    default:
        /* ------------------------------------------------------------------ */
        /* 비정상 mode 방어                                                    */
        /* - screen1 로 되돌리는 순간 Vario_State_SetMode() 가 redraw request  */
        /*   를 이미 세운다.                                                    */
        /* ------------------------------------------------------------------ */
        Vario_State_SetMode(VARIO_MODE_SCREEN_1);
        Vario_Audio_Task(now_ms);
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
        Audio_App_ReleaseVariometer(now_ms);
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
