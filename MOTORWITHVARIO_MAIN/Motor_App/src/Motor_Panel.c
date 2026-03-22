
#include "Motor_Panel.h"

#include "APP_STATE.h"
#include "BACKLIGHT_App.h"
#include "u8g2_uc1608_stm32.h"

#include <string.h>

/* -------------------------------------------------------------------------- */
/*  Display settings 를 shared platform API 에 반영하는 helper                */
/*                                                                            */
/*  규칙                                                                       */
/*  - panel 레지스터는 직접 raw GPIO/SPI로 건드리지 않는다.                   */
/*  - 반드시 u8g2_uc1608_stm32.h 에 공개된 wrapper API를 사용한다.             */
/*  - backlight 역시 APP_STATE.settings.backlight + Backlight_App API를        */
/*    통해서만 조절한다.                                                       */
/* -------------------------------------------------------------------------- */
void Motor_Panel_Init(void)
{
    /* 현재 구현에서는 별도 private runtime 이 필요하지 않다. */
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
    /*  UC1608 panel 설정을 shared APP_STATE에도 mirror 한다.                  */
    /*  이렇게 해 두면 다른 디버그 페이지나 재부팅 후 저장 backend가 붙을 때     */
    /*  설정 source of truth를 하나로 유지하기 쉽다.                            */
    /* ---------------------------------------------------------------------- */
    shared_settings.uc1608.flip_mode                = (settings->display.screen_flip_enabled != 0u) ? 1u : 0u;
    shared_settings.uc1608.contrast                 = settings->display.contrast_raw;
    shared_settings.uc1608.temperature_compensation = settings->display.temperature_compensation_raw;

    /* ---------------------------------------------------------------------- */
    /*  AUTO CONTINUOUS / AUTO DAY-NITE / MANUAL 을 shared backlight 정책으로  */
    /*  번역한다.                                                              */
    /* ---------------------------------------------------------------------- */
    switch ((motor_display_brightness_mode_t)settings->display.brightness_mode)
    {
    case MOTOR_DISPLAY_BRIGHTNESS_MANUAL_PERCENT:
        Backlight_App_SetAutoEnabled(false);
        Backlight_App_SetManualBrightnessPermille((uint16_t)(settings->display.manual_brightness_percent * 10u));
        break;

    case MOTOR_DISPLAY_BRIGHTNESS_AUTO_DAY_NITE:
        shared_settings.backlight.auto_mode = (uint8_t)APP_BACKLIGHT_AUTO_MODE_DIMMER;
        shared_settings.backlight.night_threshold_percent = settings->display.auto_day_night_night_threshold_percent;
        shared_settings.backlight.super_night_threshold_percent = settings->display.auto_day_night_super_night_threshold_percent;
        shared_settings.backlight.night_brightness_percent = settings->display.auto_day_night_night_brightness_percent;
        shared_settings.backlight.super_night_brightness_percent = settings->display.auto_day_night_super_night_brightness_percent;
        Backlight_App_SetAutoEnabled(true);
        break;

    case MOTOR_DISPLAY_BRIGHTNESS_AUTO_CONTINUOUS:
    default:
        shared_settings.backlight.auto_mode = (uint8_t)APP_BACKLIGHT_AUTO_MODE_CONTINUOUS;
        shared_settings.backlight.continuous_bias_steps = settings->display.auto_continuous_bias_steps;
        Backlight_App_SetAutoEnabled(true);
        break;
    }

    APP_STATE_StoreSettingsSnapshot(&shared_settings);

    /* ---------------------------------------------------------------------- */
    /*  실제 패널 레지스터 적용                                                 */
    /* ---------------------------------------------------------------------- */
    U8G2_UC1608_SetFlipModeRaw(shared_settings.uc1608.flip_mode);
    U8G2_UC1608_SetContrastRaw(shared_settings.uc1608.contrast);
    U8G2_UC1608_SetTemperatureCompensation(shared_settings.uc1608.temperature_compensation);
    U8G2_UC1608_InvalidateSmartUpdateCache();
}
