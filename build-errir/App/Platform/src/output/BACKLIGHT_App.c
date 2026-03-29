#include "BACKLIGHT_App.h"

#include <string.h>

#include "APP_STATE.h"
#include "BACKLIGHT_DRIVER.h"
#include "Brightness_Sensor.h"

/* -------------------------------------------------------------------------- */
/*  기본 fallback 정책                                                        */
/*                                                                            */
/*  주의                                                                      */
/*  - Backlight_App_Init()는 현재 main.c에서 APP_STATE_Init()보다 먼저 호출된다.*/
/*  - 따라서 init 시점에는 APP_STATE를 믿지 않고,                             */
/*    여기 fallback 값으로 먼저 시작한다.                                     */
/*  - 이후 main loop의 Backlight_App_Task()가 APP_STATE 설정을 읽어            */
/*    자동으로 실제 정책으로 넘어간다.                                        */
/* -------------------------------------------------------------------------- */
#ifndef BACKLIGHT_APP_STARTUP_FALLBACK_PERCENT
#define BACKLIGHT_APP_STARTUP_FALLBACK_PERCENT      60u
#endif

#ifndef BACKLIGHT_APP_SENSOR_FILTER_TAU_MS
#define BACKLIGHT_APP_SENSOR_FILTER_TAU_MS          120u
#endif

#ifndef BACKLIGHT_APP_SENSOR_STALE_TIMEOUT_MS
#define BACKLIGHT_APP_SENSOR_STALE_TIMEOUT_MS       450u
#endif

#ifndef BACKLIGHT_APP_DIMMER_HYSTERESIS_PERCENT
#define BACKLIGHT_APP_DIMMER_HYSTERESIS_PERCENT     3u
#endif

#ifndef BACKLIGHT_APP_CONTINUOUS_BIAS_PERCENT_STEP
#define BACKLIGHT_APP_CONTINUOUS_BIAS_PERCENT_STEP  8
#endif

#ifndef BACKLIGHT_APP_FALLBACK_SMOOTHNESS
#define BACKLIGHT_APP_FALLBACK_SMOOTHNESS           3u
#endif

/* -------------------------------------------------------------------------- */
/*  AUTO-CONT 연속 곡선                                                        */
/*                                                                            */
/*  x축: 주변광 0..100%                                                       */
/*  y축: 화면 밝기 0..100%                                                    */
/*                                                                            */
/*  각 점 사이를 선형 보간하므로 출력은 연속적이다.                           */
/*  즉, zone jump가 아니라 "곡선 위를 부드럽게 타고 이동"하는 방식이다.      */
/* -------------------------------------------------------------------------- */
#define BACKLIGHT_APP_CONT_CURVE_COUNT  21u

static const uint8_t s_backlight_cont_sensor_percent[BACKLIGHT_APP_CONT_CURVE_COUNT] =
{
      0u,   5u,  10u,  15u,  20u,
     25u,  30u,  35u,  40u,  45u,
     50u,  55u,  60u,  65u,  70u,
     75u,  80u,  85u,  90u,  95u,
    100u
};

static const uint8_t s_backlight_cont_brightness_percent[BACKLIGHT_APP_CONT_CURVE_COUNT] =
{
     12u,  13u,  15u,  18u,  22u,
     27u,  33u,  40u,  48u,  56u,
     64u,  71u,  77u,  83u,  88u,
     92u,  95u,  97u,  98u,  99u,
    100u
};

/* -------------------------------------------------------------------------- */
/*  smoothness 1..5 -> 출력 time constant(ms)                                 */
/*                                                                            */
/*  값이 클수록 target을 더 천천히 따라간다.                                 */
/* -------------------------------------------------------------------------- */
static const uint16_t s_backlight_smoothness_tau_ms[5] =
{
     180u,
     350u,
     700u,
    1400u,
    2600u
};

/* -------------------------------------------------------------------------- */
/*  공개 runtime                                                               */
/* -------------------------------------------------------------------------- */
volatile backlight_app_runtime_t g_backlight_app_runtime;

/* -------------------------------------------------------------------------- */
/*  호환용 override 상태                                                       */
/* -------------------------------------------------------------------------- */
static int8_t  s_backlight_bias_override_steps = 0;
static uint8_t s_backlight_bias_override_valid = 0u;

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 0..100 clamp                                                    */
/* -------------------------------------------------------------------------- */
static uint8_t Backlight_App_ClampPercentS32(int32_t value_percent)
{
    if (value_percent < 0)
    {
        return 0u;
    }

    if (value_percent > 100)
    {
        return 100u;
    }

    return (uint8_t)value_percent;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 0..1000 clamp                                                   */
/* -------------------------------------------------------------------------- */
static uint16_t Backlight_App_ClampPermilleS32(int32_t value_permille)
{
    if (value_permille < 0)
    {
        return 0u;
    }

    if (value_permille > 1000)
    {
        return 1000u;
    }

    return (uint16_t)value_permille;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: -2..+2 clamp                                                    */
/* -------------------------------------------------------------------------- */
static int8_t Backlight_App_ClampBiasStepsS32(int32_t value_steps)
{
    if (value_steps < -2)
    {
        return -2;
    }

    if (value_steps > 2)
    {
        return 2;
    }

    return (int8_t)value_steps;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: smoothness 1..5 clamp                                           */
/* -------------------------------------------------------------------------- */
static uint8_t Backlight_App_ClampSmoothnessS32(int32_t value_smoothness)
{
    if (value_smoothness < 1)
    {
        return 1u;
    }

    if (value_smoothness > 5)
    {
        return 5u;
    }

    return (uint8_t)value_smoothness;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: percent -> Q16                                                  */
/* -------------------------------------------------------------------------- */
static uint16_t Backlight_App_PercentToQ16(uint8_t percent)
{
    return (uint16_t)((((uint32_t)percent) * 65535u + 50u) / 100u);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: permille -> Q16                                                 */
/* -------------------------------------------------------------------------- */
static uint16_t Backlight_App_PermilleToQ16(uint16_t permille)
{
    return (uint16_t)((((uint32_t)permille) * 65535u + 500u) / 1000u);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: Q16 -> percent                                                  */
/* -------------------------------------------------------------------------- */
static uint8_t Backlight_App_Q16ToPercent(uint16_t q16)
{
    return (uint8_t)((((uint32_t)q16) * 100u + (65535u / 2u)) / 65535u);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 절댓값                                                          */
/* -------------------------------------------------------------------------- */
static uint32_t Backlight_App_AbsS32(int32_t value)
{
    if (value < 0)
    {
        return (uint32_t)(-value);
    }

    return (uint32_t)value;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: dt/tau 기반 연속 접근                                           */
/*                                                                            */
/*  current += (target-current) * dt / tau                                    */
/*                                                                            */
/*  특징                                                                      */
/*  - dt가 커지면 더 많이 이동하고,                                            */
/*  - dt가 1ms 수준이면 아주 촘촘히 조금씩 이동한다.                           */
/*  - 매우 작은 diff에서도 step=0이 되지 않게 1LSB 보장한다.                  */
/* -------------------------------------------------------------------------- */
static uint16_t Backlight_App_ApproachQ16(uint16_t current_q16,
                                          uint16_t target_q16,
                                          uint32_t dt_ms,
                                          uint32_t tau_ms)
{
    int32_t diff_q16;
    int32_t step_q16;
    int32_t next_q16;

    if ((dt_ms == 0u) || (current_q16 == target_q16))
    {
        return current_q16;
    }

    if (tau_ms <= 1u)
    {
        return target_q16;
    }

    diff_q16 = (int32_t)target_q16 - (int32_t)current_q16;
    step_q16 = (int32_t)(((int64_t)diff_q16 * (int64_t)dt_ms) /
                         (int64_t)tau_ms);

    if (step_q16 == 0)
    {
        step_q16 = (diff_q16 > 0) ? 1 : -1;
    }

    next_q16 = (int32_t)current_q16 + step_q16;

    if (((diff_q16 > 0) && (next_q16 > (int32_t)target_q16)) ||
        ((diff_q16 < 0) && (next_q16 < (int32_t)target_q16)))
    {
        next_q16 = (int32_t)target_q16;
    }

    if (next_q16 < 0)
    {
        next_q16 = 0;
    }
    else if (next_q16 > 65535)
    {
        next_q16 = 65535;
    }

    return (uint16_t)next_q16;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: AUTO-CONT 곡선 평가                                              */
/* -------------------------------------------------------------------------- */
static uint8_t Backlight_App_MapContinuousAmbientToBrightnessPercent(uint8_t sensor_percent)
{
    uint32_t index;

    if (sensor_percent <= s_backlight_cont_sensor_percent[0])
    {
        return s_backlight_cont_brightness_percent[0];
    }

    for (index = 0u; index < (BACKLIGHT_APP_CONT_CURVE_COUNT - 1u); ++index)
    {
        uint8_t x0;
        uint8_t x1;
        uint8_t y0;
        uint8_t y1;
        uint32_t numerator;
        uint32_t denominator;
        uint32_t interpolated;

        x0 = s_backlight_cont_sensor_percent[index];
        x1 = s_backlight_cont_sensor_percent[index + 1u];

        if (sensor_percent > x1)
        {
            continue;
        }

        y0 = s_backlight_cont_brightness_percent[index];
        y1 = s_backlight_cont_brightness_percent[index + 1u];

        numerator = ((uint32_t)(sensor_percent - x0) * (uint32_t)(y1 - y0));
        denominator = (uint32_t)(x1 - x0);
        interpolated = (uint32_t)y0 + ((numerator + (denominator / 2u)) / denominator);

        if (interpolated > 100u)
        {
            interpolated = 100u;
        }

        return (uint8_t)interpolated;
    }

    return s_backlight_cont_brightness_percent[BACKLIGHT_APP_CONT_CURVE_COUNT - 1u];
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: smoothness -> tau                                               */
/* -------------------------------------------------------------------------- */
static uint16_t Backlight_App_GetTauFromSmoothness(uint8_t smoothness)
{
    uint8_t index;

    index = Backlight_App_ClampSmoothnessS32(smoothness);
    return s_backlight_smoothness_tau_ms[index - 1u];
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: AUTO-DIMMER zone 계산                                            */
/*                                                                            */
/*  zone 경계는 3% 히스테리시스를 넣어 flicker를 방지한다.                     */
/* -------------------------------------------------------------------------- */
static uint8_t Backlight_App_DetermineDimmerZone(
    uint8_t previous_zone,
    uint8_t sensor_percent,
    uint8_t night_threshold_percent,
    uint8_t super_night_threshold_percent)
{
    uint8_t hysteresis;
    uint8_t night_thr;
    uint8_t super_thr;

    hysteresis = BACKLIGHT_APP_DIMMER_HYSTERESIS_PERCENT;
    night_thr = Backlight_App_ClampPercentS32((int32_t)night_threshold_percent);
    super_thr = Backlight_App_ClampPercentS32((int32_t)super_night_threshold_percent);

    if (super_thr > night_thr)
    {
        super_thr = night_thr;
    }

    switch (previous_zone)
    {
        case BACKLIGHT_APP_DIMMER_ZONE_SUPER_NIGHT:
            if (sensor_percent > (uint8_t)(super_thr + hysteresis))
            {
                if (sensor_percent > (uint8_t)(night_thr + hysteresis))
                {
                    return BACKLIGHT_APP_DIMMER_ZONE_DAY;
                }
                return BACKLIGHT_APP_DIMMER_ZONE_NIGHT;
            }
            return BACKLIGHT_APP_DIMMER_ZONE_SUPER_NIGHT;

        case BACKLIGHT_APP_DIMMER_ZONE_NIGHT:
            if (sensor_percent <= (uint8_t)((super_thr > hysteresis) ?
                                            (super_thr - hysteresis) : 0u))
            {
                return BACKLIGHT_APP_DIMMER_ZONE_SUPER_NIGHT;
            }
            if (sensor_percent > (uint8_t)(night_thr + hysteresis))
            {
                return BACKLIGHT_APP_DIMMER_ZONE_DAY;
            }
            return BACKLIGHT_APP_DIMMER_ZONE_NIGHT;

        case BACKLIGHT_APP_DIMMER_ZONE_DAY:
        default:
            if (sensor_percent <= (uint8_t)((super_thr > hysteresis) ?
                                            (super_thr - hysteresis) : 0u))
            {
                return BACKLIGHT_APP_DIMMER_ZONE_SUPER_NIGHT;
            }
            if (sensor_percent <= (uint8_t)((night_thr > hysteresis) ?
                                            (night_thr - hysteresis) : 0u))
            {
                return BACKLIGHT_APP_DIMMER_ZONE_NIGHT;
            }
            return BACKLIGHT_APP_DIMMER_ZONE_DAY;
    }
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: settings 정규화                                                 */
/* -------------------------------------------------------------------------- */
static void Backlight_App_NormalizeSettings(app_settings_t *settings)
{
    if (settings == 0)
    {
        return;
    }

    if (settings->backlight.auto_mode > (uint8_t)APP_BACKLIGHT_AUTO_MODE_DIMMER)
    {
        settings->backlight.auto_mode = (uint8_t)APP_BACKLIGHT_AUTO_MODE_CONTINUOUS;
    }

    settings->backlight.continuous_bias_steps =
        Backlight_App_ClampBiasStepsS32(settings->backlight.continuous_bias_steps);
    settings->backlight.transition_smoothness =
        Backlight_App_ClampSmoothnessS32(settings->backlight.transition_smoothness);
    settings->backlight.night_threshold_percent =
        Backlight_App_ClampPercentS32(settings->backlight.night_threshold_percent);
    settings->backlight.super_night_threshold_percent =
        Backlight_App_ClampPercentS32(settings->backlight.super_night_threshold_percent);
    settings->backlight.night_brightness_percent =
        Backlight_App_ClampPercentS32(settings->backlight.night_brightness_percent);
    settings->backlight.super_night_brightness_percent =
        Backlight_App_ClampPercentS32(settings->backlight.super_night_brightness_percent);

    if (settings->backlight.super_night_threshold_percent >
        settings->backlight.night_threshold_percent)
    {
        settings->backlight.super_night_threshold_percent =
            settings->backlight.night_threshold_percent;
    }
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 현재 settings에서 bias 결정                                     */
/* -------------------------------------------------------------------------- */
static int8_t Backlight_App_GetEffectiveBiasSteps(
    const app_settings_t *settings)
{
    if (s_backlight_bias_override_valid != 0u)
    {
        return Backlight_App_ClampBiasStepsS32(s_backlight_bias_override_steps);
    }

    if (settings == 0)
    {
        return 0;
    }

    return Backlight_App_ClampBiasStepsS32(
        settings->backlight.continuous_bias_steps);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: first valid sensor / stale 판정                                 */
/* -------------------------------------------------------------------------- */
static bool Backlight_App_IsSensorFresh(const app_brightness_state_t *brightness,
                                        uint32_t now_ms)
{
    if (brightness == 0)
    {
        return false;
    }

    if (brightness->valid == false)
    {
        return false;
    }

    if ((now_ms - brightness->last_update_ms) > BACKLIGHT_APP_SENSOR_STALE_TIMEOUT_MS)
    {
        return false;
    }

    return true;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: runtime에 sensor snapshot 반영                                  */
/* -------------------------------------------------------------------------- */
static void Backlight_App_UpdateSensorRuntime(const app_brightness_state_t *brightness,
                                              uint32_t now_ms,
                                              uint32_t dt_ms)
{
    uint16_t raw_sensor_permille;
    uint16_t current_filtered_q16;
    uint16_t target_filtered_q16;
    uint16_t next_filtered_q16;

    if ((brightness == 0) ||
        (Backlight_App_IsSensorFresh(brightness, now_ms) == false))
    {
        g_backlight_app_runtime.sensor_valid = false;
        return;
    }

    raw_sensor_permille = brightness->normalized_permille;
    if (raw_sensor_permille > 1000u)
    {
        raw_sensor_permille = 1000u;
    }

    g_backlight_app_runtime.sensor_valid = true;
    g_backlight_app_runtime.last_sensor_update_ms = brightness->last_update_ms;
    g_backlight_app_runtime.sensor_raw_permille = raw_sensor_permille;
    g_backlight_app_runtime.sensor_percent = brightness->brightness_percent;

    current_filtered_q16 = Backlight_App_PermilleToQ16(
        g_backlight_app_runtime.sensor_filtered_permille);
    target_filtered_q16 = Backlight_App_PermilleToQ16(raw_sensor_permille);

    if (g_backlight_app_runtime.sensor_filtered_permille == 0u &&
        g_backlight_app_runtime.last_update_ms == 0u)
    {
        next_filtered_q16 = target_filtered_q16;
    }
    else
    {
        next_filtered_q16 = Backlight_App_ApproachQ16(current_filtered_q16,
                                                      target_filtered_q16,
                                                      dt_ms,
                                                      BACKLIGHT_APP_SENSOR_FILTER_TAU_MS);
    }

    g_backlight_app_runtime.sensor_filtered_permille =
        (uint16_t)((((uint32_t)next_filtered_q16) * 1000u + (65535u / 2u)) / 65535u);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: AUTO-CONT target 계산                                           */
/* -------------------------------------------------------------------------- */
static uint16_t Backlight_App_ComputeContinuousTargetQ16(const app_settings_t *settings)
{
    uint8_t filtered_sensor_percent;
    uint8_t base_brightness_percent;
    int8_t bias_steps;
    int32_t biased_percent;

    filtered_sensor_percent =
        (uint8_t)((g_backlight_app_runtime.sensor_filtered_permille + 5u) / 10u);
    base_brightness_percent =
        Backlight_App_MapContinuousAmbientToBrightnessPercent(filtered_sensor_percent);

    bias_steps = Backlight_App_GetEffectiveBiasSteps(settings);
    biased_percent = (int32_t)base_brightness_percent +
                     ((int32_t)bias_steps * BACKLIGHT_APP_CONTINUOUS_BIAS_PERCENT_STEP);

    g_backlight_app_runtime.active_bias_steps = bias_steps;
    return Backlight_App_PercentToQ16(Backlight_App_ClampPercentS32(biased_percent));
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: AUTO-DIMMER target 계산                                          */
/* -------------------------------------------------------------------------- */
static uint16_t Backlight_App_ComputeDimmerTargetQ16(const app_settings_t *settings)
{
    uint8_t sensor_percent;
    uint8_t zone;
    uint8_t brightness_percent;

    sensor_percent =
        (uint8_t)((g_backlight_app_runtime.sensor_filtered_permille + 5u) / 10u);

    zone = Backlight_App_DetermineDimmerZone(
        g_backlight_app_runtime.active_zone,
        sensor_percent,
        settings->backlight.night_threshold_percent,
        settings->backlight.super_night_threshold_percent);

    g_backlight_app_runtime.active_zone = zone;
    g_backlight_app_runtime.active_bias_steps = 0;

    switch (zone)
    {
        case BACKLIGHT_APP_DIMMER_ZONE_SUPER_NIGHT:
            brightness_percent = settings->backlight.super_night_brightness_percent;
            break;

        case BACKLIGHT_APP_DIMMER_ZONE_NIGHT:
            brightness_percent = settings->backlight.night_brightness_percent;
            break;

        case BACKLIGHT_APP_DIMMER_ZONE_DAY:
        default:
            brightness_percent = 100u;
            break;
    }

    return Backlight_App_PercentToQ16(brightness_percent);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: init                                                             */
/* -------------------------------------------------------------------------- */
void Backlight_App_Init(void)
{
    uint16_t startup_q16;

    memset((void *)&g_backlight_app_runtime, 0, sizeof(g_backlight_app_runtime));

    Backlight_Driver_Init();

    startup_q16 = Backlight_App_PercentToQ16(BACKLIGHT_APP_STARTUP_FALLBACK_PERCENT);

    g_backlight_app_runtime.initialized = true;
    g_backlight_app_runtime.sensor_valid = false;
    g_backlight_app_runtime.manual_override_enabled = false;
    g_backlight_app_runtime.active_mode = (uint8_t)BACKLIGHT_APP_ACTIVE_MODE_AUTO_CONTINUOUS;
    g_backlight_app_runtime.active_zone = (uint8_t)BACKLIGHT_APP_DIMMER_ZONE_DAY;
    g_backlight_app_runtime.active_bias_steps = 0;
    g_backlight_app_runtime.active_smoothness = BACKLIGHT_APP_FALLBACK_SMOOTHNESS;
    g_backlight_app_runtime.last_update_ms = 0u;
    g_backlight_app_runtime.last_sensor_update_ms = 0u;
    g_backlight_app_runtime.sensor_raw_permille = 0u;
    g_backlight_app_runtime.sensor_filtered_permille = 0u;
    g_backlight_app_runtime.sensor_percent = 0u;
    g_backlight_app_runtime.target_linear_q16 = startup_q16;
    g_backlight_app_runtime.applied_linear_q16 = startup_q16;
    g_backlight_app_runtime.manual_override_q16 = startup_q16;
    g_backlight_app_runtime.target_linear_percent =
        Backlight_App_Q16ToPercent(startup_q16);
    g_backlight_app_runtime.applied_linear_percent =
        Backlight_App_Q16ToPercent(startup_q16);

    Backlight_Driver_SetLinearQ16(startup_q16);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: auto on/off                                                      */
/* -------------------------------------------------------------------------- */
void Backlight_App_SetAutoEnabled(bool enable)
{
    if (enable != false)
    {
        g_backlight_app_runtime.manual_override_enabled = false;
        return;
    }

    g_backlight_app_runtime.manual_override_enabled = true;
    g_backlight_app_runtime.active_mode =
        (uint8_t)BACKLIGHT_APP_ACTIVE_MODE_MANUAL_OVERRIDE;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: manual brightness                                                */
/* -------------------------------------------------------------------------- */
void Backlight_App_SetManualBrightnessPermille(uint16_t linear_permille)
{
    g_backlight_app_runtime.manual_override_q16 =
        Backlight_App_PermilleToQ16(Backlight_App_ClampPermilleS32(linear_permille));
    g_backlight_app_runtime.manual_override_enabled = true;
    g_backlight_app_runtime.active_mode =
        (uint8_t)BACKLIGHT_APP_ACTIVE_MODE_MANUAL_OVERRIDE;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: user bias override                                               */
/* -------------------------------------------------------------------------- */
void Backlight_App_SetUserBiasSteps(int8_t bias_steps)
{
    s_backlight_bias_override_steps = Backlight_App_ClampBiasStepsS32(bias_steps);
    s_backlight_bias_override_valid = 1u;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: task                                                             */
/* -------------------------------------------------------------------------- */
void Backlight_App_Task(uint32_t now_ms)
{
    app_settings_t         settings_snapshot;
    app_brightness_state_t brightness_snapshot;
    uint32_t               dt_ms;
    uint16_t               tau_ms;
    uint16_t               target_q16;

    if (g_backlight_app_runtime.initialized == false)
    {
        Backlight_App_Init();
    }

    if (g_backlight_app_runtime.last_update_ms == 0u)
    {
        dt_ms = 1u;
    }
    else
    {
        dt_ms = now_ms - g_backlight_app_runtime.last_update_ms;
        if (dt_ms == 0u)
        {
            return;
        }
    }

    APP_STATE_CopySettingsSnapshot(&settings_snapshot);
    APP_STATE_CopyBrightnessSnapshot(&brightness_snapshot);

    Backlight_App_NormalizeSettings(&settings_snapshot);
    Backlight_App_UpdateSensorRuntime(&brightness_snapshot, now_ms, dt_ms);

    if (g_backlight_app_runtime.manual_override_enabled != false)
    {
        target_q16 = g_backlight_app_runtime.manual_override_q16;
        g_backlight_app_runtime.active_mode =
            (uint8_t)BACKLIGHT_APP_ACTIVE_MODE_MANUAL_OVERRIDE;
        g_backlight_app_runtime.active_smoothness = BACKLIGHT_APP_FALLBACK_SMOOTHNESS;
    }
    else if (g_backlight_app_runtime.sensor_valid == false)
    {
        /* ------------------------------------------------------------------ */
        /*  센서가 아직 없거나 stale이면 현재 목표를 유지한다.                  */
        /*  초기 부팅 직후라면 startup fallback이 그대로 유지된다.             */
        /* ------------------------------------------------------------------ */
        target_q16 = g_backlight_app_runtime.target_linear_q16;
        g_backlight_app_runtime.active_mode =
            (settings_snapshot.backlight.auto_mode == (uint8_t)APP_BACKLIGHT_AUTO_MODE_DIMMER) ?
            (uint8_t)BACKLIGHT_APP_ACTIVE_MODE_AUTO_DIMMER :
            (uint8_t)BACKLIGHT_APP_ACTIVE_MODE_AUTO_CONTINUOUS;
        g_backlight_app_runtime.active_smoothness =
            settings_snapshot.backlight.transition_smoothness;
    }
    else if (settings_snapshot.backlight.auto_mode ==
             (uint8_t)APP_BACKLIGHT_AUTO_MODE_DIMMER)
    {
        target_q16 = Backlight_App_ComputeDimmerTargetQ16(&settings_snapshot);
        g_backlight_app_runtime.active_mode =
            (uint8_t)BACKLIGHT_APP_ACTIVE_MODE_AUTO_DIMMER;
        g_backlight_app_runtime.active_smoothness =
            settings_snapshot.backlight.transition_smoothness;
    }
    else
    {
        target_q16 = Backlight_App_ComputeContinuousTargetQ16(&settings_snapshot);
        g_backlight_app_runtime.active_mode =
            (uint8_t)BACKLIGHT_APP_ACTIVE_MODE_AUTO_CONTINUOUS;
        g_backlight_app_runtime.active_smoothness =
            settings_snapshot.backlight.transition_smoothness;
        g_backlight_app_runtime.active_zone =
            (uint8_t)BACKLIGHT_APP_DIMMER_ZONE_DAY;
    }

    g_backlight_app_runtime.target_linear_q16 = target_q16;
    g_backlight_app_runtime.target_linear_percent = Backlight_App_Q16ToPercent(target_q16);

    tau_ms = Backlight_App_GetTauFromSmoothness(
        g_backlight_app_runtime.active_smoothness);
    g_backlight_app_runtime.applied_linear_q16 =
        Backlight_App_ApproachQ16(g_backlight_app_runtime.applied_linear_q16,
                                  g_backlight_app_runtime.target_linear_q16,
                                  dt_ms,
                                  tau_ms);
    g_backlight_app_runtime.applied_linear_percent =
        Backlight_App_Q16ToPercent(g_backlight_app_runtime.applied_linear_q16);

    Backlight_Driver_SetLinearQ16(g_backlight_app_runtime.applied_linear_q16);
    g_backlight_app_runtime.last_update_ms = now_ms;
}
