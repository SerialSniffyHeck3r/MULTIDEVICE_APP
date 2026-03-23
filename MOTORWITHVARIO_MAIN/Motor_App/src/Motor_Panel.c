#include "Motor_Panel.h"

#include "APP_STATE.h"
#include "BACKLIGHT_App.h"
#include "u8g2_uc1608_stm32.h"

/* -------------------------------------------------------------------------- */
/*  Display / backlight 설정을 shared platform API에 반영하는 facade          */
/*                                                                            */
/*  계층 규칙                                                                  */
/*  - Motor_App는 UC1608 레지스터를 직접 때리지 않는다.                        */
/*  - 반드시 app/platform wrapper API와 APP_STATE settings snapshot만 쓴다.    */
/* -------------------------------------------------------------------------- */
void Motor_Panel_Init(void)
{
    /* 현재 구조에서는 별도 private runtime이 필요하지 않다. */
}

void Motor_Panel_ApplyDisplaySettings(const motor_settings_t *settings)
{
    app_settings_t shared_settings;

    if (settings == 0)
    {
        return;
    }

    APP_STATE_CopySettingsSnapshot(&shared_settings);

    /* ---------------------------------------------------------------------- */
    /*  APP_STATE.settings mirror                                               */
    /*  - backlight 정책은 shared setting에도 같은 값으로 유지한다.             */
    /*  - 다른 디버그 페이지와의 일관성을 위해 panel raw 값도 mirror 한다.      */
    /* ---------------------------------------------------------------------- */
    shared_settings.backlight.continuous_bias_steps = settings->display.auto_continuous_bias_steps;
    shared_settings.backlight.night_threshold_percent = settings->display.auto_day_night_night_threshold_percent;
    shared_settings.backlight.super_night_threshold_percent = settings->display.auto_day_night_super_night_threshold_percent;
    shared_settings.backlight.night_brightness_percent = settings->display.auto_day_night_night_brightness_percent;
    shared_settings.backlight.super_night_brightness_percent = settings->display.auto_day_night_super_night_brightness_percent;

    shared_settings.uc1608.contrast = settings->display.contrast_raw;
    shared_settings.uc1608.temperature_compensation = settings->display.temperature_compensation_raw;

    switch ((motor_display_brightness_mode_t)settings->display.brightness_mode)
    {
    case MOTOR_DISPLAY_BRIGHTNESS_MANUAL_PERCENT:
        /* ------------------------------------------------------------------ */
        /*  Manual %                                                           */
        /*  - shared backlight 정책은 유지하되 출력은 manual override로 전환    */
        /* ------------------------------------------------------------------ */
        Backlight_App_SetAutoEnabled(false);
        Backlight_App_SetManualBrightnessPermille((uint16_t)settings->display.manual_brightness_percent * 10u);
        break;

    case MOTOR_DISPLAY_BRIGHTNESS_AUTO_DAY_NITE:
        /* ------------------------------------------------------------------ */
        /*  Auto Day/Nite                                                      */
        /*  - app/platform의 DIMMER zone 정책으로 번역                         */
        /* ------------------------------------------------------------------ */
        shared_settings.backlight.auto_mode = (uint8_t)APP_BACKLIGHT_AUTO_MODE_DIMMER;
        Backlight_App_SetAutoEnabled(true);
        break;

    case MOTOR_DISPLAY_BRIGHTNESS_AUTO_CONTINUOUS:
    default:
        /* ------------------------------------------------------------------ */
        /*  Auto Continuous                                                    */
        /*  - 주변광 연속 곡선 추종                                            */
        /* ------------------------------------------------------------------ */
        shared_settings.backlight.auto_mode = (uint8_t)APP_BACKLIGHT_AUTO_MODE_CONTINUOUS;
        Backlight_App_SetAutoEnabled(true);
        Backlight_App_SetUserBiasSteps(settings->display.auto_continuous_bias_steps);
        break;
    }

    APP_STATE_StoreSettingsSnapshot(&shared_settings);

    /* ---------------------------------------------------------------------- */
    /*  실제 panel wrapper API 적용                                             */
    /*  - screen flip은 사용자 요구에 따라 Motor_App에서 다루지 않는다.        */
    /*  - contrast / temperature compensation / smart update / frame limit만   */
    /*    Motor display setting에서 직접 만진다.                               */
    /* ---------------------------------------------------------------------- */
    U8G2_UC1608_SetContrastRaw(shared_settings.uc1608.contrast);
    U8G2_UC1608_SetTemperatureCompensation(shared_settings.uc1608.temperature_compensation);
    U8G2_UC1608_EnableSmartUpdate((settings->display.smart_update_enabled != 0u) ? 1u : 0u);
    U8G2_UC1608_EnableFrameLimit((settings->display.frame_limit_enabled != 0u) ? 1u : 0u);
    U8G2_UC1608_InvalidateSmartUpdateCache();
}
