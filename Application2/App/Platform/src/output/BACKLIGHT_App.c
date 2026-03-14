#include "BACKLIGHT_App.h"

#include <string.h>

#include "APP_STATE.h"
#include "Brightness_Sensor.h"
#include "BACKLIGHT_DRIVER.h"

/* -------------------------------------------------------------------------- */
/*  기본 tuning 값                                                             */
/*                                                                            */
/*  현재 값의 의미                                                             */
/*  - sensor_filter_alpha_permille                                             */
/*      센서 burst 결과가 들어올 때 어느 정도 반영할지 결정                    */
/*  - ambient_change_threshold_*                                               */
/*      주변광 변화가 이 정도 이상 누적되어야 retarget 후보로 인정             */
/*  - target_change_threshold_permille                                         */
/*      실제 화면 밝기 목표 변화량이 너무 작으면 무시                          */
/*  - hold_ms                                                                  */
/*      스마트폰처럼 "유의미한 변화가 잠깐이 아니라 일정 시간 유지되었는가"를   */
/*      확인하는 시간                                                          */
/*  - slew_per_sec                                                             */
/*      목표가 확정된 뒤 화면 밝기가 실제로 따라가는 속도                      */
/* -------------------------------------------------------------------------- */
#ifndef BACKLIGHT_APP_DEFAULT_AUTO_ENABLED
#define BACKLIGHT_APP_DEFAULT_AUTO_ENABLED                    1u
#endif

#ifndef BACKLIGHT_APP_DEFAULT_USER_BIAS_STEPS
#define BACKLIGHT_APP_DEFAULT_USER_BIAS_STEPS                0
#endif

#ifndef BACKLIGHT_APP_DEFAULT_SENSOR_BIAS_COUNTS
#define BACKLIGHT_APP_DEFAULT_SENSOR_BIAS_COUNTS             0
#endif

#ifndef BACKLIGHT_APP_DEFAULT_SENSOR_BIAS_PERMILLE
#define BACKLIGHT_APP_DEFAULT_SENSOR_BIAS_PERMILLE           0
#endif

#ifndef BACKLIGHT_APP_DEFAULT_SENSOR_FILTER_ALPHA_PERMILLE
#define BACKLIGHT_APP_DEFAULT_SENSOR_FILTER_ALPHA_PERMILLE   180u
#endif

#ifndef BACKLIGHT_APP_DEFAULT_AMBIENT_THRESHOLD_UP_PERMILLE
#define BACKLIGHT_APP_DEFAULT_AMBIENT_THRESHOLD_UP_PERMILLE  70u
#endif

#ifndef BACKLIGHT_APP_DEFAULT_AMBIENT_THRESHOLD_DOWN_PERMILLE
#define BACKLIGHT_APP_DEFAULT_AMBIENT_THRESHOLD_DOWN_PERMILLE 55u
#endif

#ifndef BACKLIGHT_APP_DEFAULT_TARGET_THRESHOLD_PERMILLE
#define BACKLIGHT_APP_DEFAULT_TARGET_THRESHOLD_PERMILLE      25u
#endif

#ifndef BACKLIGHT_APP_DEFAULT_BRIGHTEN_HOLD_MS
#define BACKLIGHT_APP_DEFAULT_BRIGHTEN_HOLD_MS               350u
#endif

#ifndef BACKLIGHT_APP_DEFAULT_DARKEN_HOLD_MS
#define BACKLIGHT_APP_DEFAULT_DARKEN_HOLD_MS                 900u
#endif

#ifndef BACKLIGHT_APP_DEFAULT_BRIGHTEN_SLEW_PER_SEC
#define BACKLIGHT_APP_DEFAULT_BRIGHTEN_SLEW_PER_SEC          700u
#endif

#ifndef BACKLIGHT_APP_DEFAULT_DARKEN_SLEW_PER_SEC
#define BACKLIGHT_APP_DEFAULT_DARKEN_SLEW_PER_SEC            450u
#endif

#ifndef BACKLIGHT_APP_DEFAULT_MIN_LINEAR_PERMILLE
#define BACKLIGHT_APP_DEFAULT_MIN_LINEAR_PERMILLE            28u
#endif

#ifndef BACKLIGHT_APP_DEFAULT_MAX_LINEAR_PERMILLE
#define BACKLIGHT_APP_DEFAULT_MAX_LINEAR_PERMILLE            1000u
#endif

#ifndef BACKLIGHT_APP_DEFAULT_STARTUP_LINEAR_PERMILLE
#define BACKLIGHT_APP_DEFAULT_STARTUP_LINEAR_PERMILLE        220u
#endif

#ifndef BACKLIGHT_APP_DEFAULT_INVALID_SENSOR_FALLBACK_PERMILLE
#define BACKLIGHT_APP_DEFAULT_INVALID_SENSOR_FALLBACK_PERMILLE 220u
#endif

#ifndef BACKLIGHT_APP_DEFAULT_SAMPLE_STALE_TIMEOUT_MS
#define BACKLIGHT_APP_DEFAULT_SAMPLE_STALE_TIMEOUT_MS        1500u
#endif

#ifndef BACKLIGHT_APP_DEFAULT_POST_COMMIT_IGNORE_MS
#define BACKLIGHT_APP_DEFAULT_POST_COMMIT_IGNORE_MS          300u
#endif

#ifndef BACKLIGHT_APP_DEFAULT_MANUAL_LINEAR_PERMILLE
#define BACKLIGHT_APP_DEFAULT_MANUAL_LINEAR_PERMILLE         220u
#endif

/* -------------------------------------------------------------------------- */
/*  공개 튜닝 전역                                                             */
/*                                                                            */
/*  향후 settings UI가 붙으면 이 구조체와 테이블을 직접 수정하면 된다.         */
/* -------------------------------------------------------------------------- */
volatile backlight_app_tuning_t g_backlight_app_tuning =
{
    BACKLIGHT_APP_DEFAULT_AUTO_ENABLED,                   /* auto_enabled */
    0u,                                                   /* reserved0 */
    BACKLIGHT_APP_DEFAULT_USER_BIAS_STEPS,                /* user_bias_steps */
    0,                                                    /* reserved1 */

    BACKLIGHT_APP_DEFAULT_SENSOR_BIAS_COUNTS,             /* sensor_bias_counts */
    BACKLIGHT_APP_DEFAULT_SENSOR_BIAS_PERMILLE,           /* sensor_bias_permille */

    BACKLIGHT_APP_DEFAULT_SENSOR_FILTER_ALPHA_PERMILLE,   /* sensor_filter_alpha_permille */
    BACKLIGHT_APP_DEFAULT_AMBIENT_THRESHOLD_UP_PERMILLE,  /* ambient_change_threshold_up_permille */
    BACKLIGHT_APP_DEFAULT_AMBIENT_THRESHOLD_DOWN_PERMILLE,/* ambient_change_threshold_down_permille */
    BACKLIGHT_APP_DEFAULT_TARGET_THRESHOLD_PERMILLE,      /* target_change_threshold_permille */

    BACKLIGHT_APP_DEFAULT_BRIGHTEN_HOLD_MS,               /* brighten_hold_ms */
    BACKLIGHT_APP_DEFAULT_DARKEN_HOLD_MS,                 /* darken_hold_ms */

    BACKLIGHT_APP_DEFAULT_BRIGHTEN_SLEW_PER_SEC,          /* brighten_slew_permille_per_sec */
    BACKLIGHT_APP_DEFAULT_DARKEN_SLEW_PER_SEC,            /* darken_slew_permille_per_sec */

    BACKLIGHT_APP_DEFAULT_MIN_LINEAR_PERMILLE,            /* min_linear_permille */
    BACKLIGHT_APP_DEFAULT_MAX_LINEAR_PERMILLE,            /* max_linear_permille */
    BACKLIGHT_APP_DEFAULT_STARTUP_LINEAR_PERMILLE,        /* startup_linear_permille */
    BACKLIGHT_APP_DEFAULT_INVALID_SENSOR_FALLBACK_PERMILLE,/* invalid_sensor_fallback_permille */

    BACKLIGHT_APP_DEFAULT_SAMPLE_STALE_TIMEOUT_MS,        /* sample_stale_timeout_ms */
    BACKLIGHT_APP_DEFAULT_POST_COMMIT_IGNORE_MS           /* post_commit_ignore_ms */
};

volatile backlight_app_runtime_t g_backlight_app_runtime;

/* -------------------------------------------------------------------------- */
/*  사용자 bias 단계별 실제 permille 오프셋                                    */
/*                                                                            */
/*  인덱스 매핑                                                                */
/*  - 0 -> user bias -2                                                        */
/*  - 1 -> user bias -1                                                        */
/*  - 2 -> user bias  0                                                        */
/*  - 3 -> user bias +1                                                        */
/*  - 4 -> user bias +2                                                        */
/* -------------------------------------------------------------------------- */
volatile int16_t g_backlight_app_bias_step_offsets_permille[5] =
{
    -180, -90, 0, 90, 180
};

/* -------------------------------------------------------------------------- */
/*  주변광 -> 화면 밝기 매핑 커브                                              */
/*                                                                            */
/*  x축: 조도 센서 정규화 결과(0~1000)                                         */
/*  y축: 사람 눈 기준 목표 화면 밝기(0~1000)                                  */
/*                                                                            */
/*  의도                                                                      */
/*  - 완전 저조도에서는 화면을 과도하게 밝게 만들지 않는다.                    */
/*  - 중간 조도에서는 비교적 천천히 올라간다.                                  */
/*  - 고조도 구간에 들어서면 가독성을 위해 상승량을 더 크게 준다.              */
/*                                                                            */
/*  이는 "주변광 변화가 있다고 항상 즉시 선형 추종"하는 방식이 아니라,         */
/*  일단 적당한 목표 레벨을 고른 뒤, 그 목표를 hold + hysteresis + slew로      */
/*  제어하는 구조의 기반 곡선이다.                                             */
/* -------------------------------------------------------------------------- */
volatile uint16_t g_backlight_app_curve_sensor_permille[BACKLIGHT_APP_CURVE_POINT_COUNT] =
{
      0u,   15u,   50u,  110u,  190u,  300u,  430u,  580u,  730u,  850u,  930u, 1000u
};

volatile uint16_t g_backlight_app_curve_target_permille[BACKLIGHT_APP_CURVE_POINT_COUNT] =
{
     32u,   38u,   50u,   72u,  110u,  180u,  300u,  470u,  650u,  790u,  910u, 1000u
};

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 부호 있는 값을 0~4095 ADC count 범위로 clamp                    */
/* -------------------------------------------------------------------------- */
static uint16_t Backlight_App_ClampToU12(int32_t value)
{
    if (value < 0)
    {
        return 0u;
    }

    if (value > 4095)
    {
        return 4095u;
    }

    return (uint16_t)value;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 0~1000 permille clamp                                           */
/* -------------------------------------------------------------------------- */
static uint16_t Backlight_App_ClampPermille(int32_t value)
{
    if (value < 0)
    {
        return 0u;
    }

    if (value > 1000)
    {
        return 1000u;
    }

    return (uint16_t)value;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: min/max 범위 clamp                                              */
/* -------------------------------------------------------------------------- */
static uint16_t Backlight_App_ClampLinearRange(int32_t value)
{
    uint16_t min_value;
    uint16_t max_value;

    min_value = g_backlight_app_tuning.min_linear_permille;
    max_value = g_backlight_app_tuning.max_linear_permille;

    if (max_value < min_value)
    {
        uint16_t temp_value;

        temp_value = min_value;
        min_value  = max_value;
        max_value  = temp_value;
    }

    if (value < (int32_t)min_value)
    {
        return min_value;
    }

    if (value > (int32_t)max_value)
    {
        return max_value;
    }

    return (uint16_t)value;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 두 uint16 차이의 절댓값                                         */
/* -------------------------------------------------------------------------- */
static uint16_t Backlight_App_AbsDiffU16(uint16_t a, uint16_t b)
{
    if (a >= b)
    {
        return (uint16_t)(a - b);
    }

    return (uint16_t)(b - a);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: bias step -> bias table index                                   */
/* -------------------------------------------------------------------------- */
static uint8_t Backlight_App_BiasIndexFromSteps(int8_t steps)
{
    int32_t index;

    index = (int32_t)steps + 2;

    if (index < 0)
    {
        index = 0;
    }

    if (index > 4)
    {
        index = 4;
    }

    return (uint8_t)index;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 센서 counts -> normalized permille                              */
/*                                                                            */
/*  Brightness_Sensor 드라이버와 동일한 기준점 매크로를 사용한다.              */
/*  즉, App 단에서 counts bias를 넣고 나서 다시 0~1000 정규화를 수행한다.      */
/* -------------------------------------------------------------------------- */
static uint16_t Backlight_App_NormalizeCountsToPermille(uint16_t calibrated_counts)
{
    int32_t range;
    int32_t delta;
    int32_t permille;

    if (BRIGHTNESS_SENSOR_CAL_RAW_AT_100_PERCENT > BRIGHTNESS_SENSOR_CAL_RAW_AT_0_PERCENT)
    {
        range = (int32_t)BRIGHTNESS_SENSOR_CAL_RAW_AT_100_PERCENT -
                (int32_t)BRIGHTNESS_SENSOR_CAL_RAW_AT_0_PERCENT;
        delta = (int32_t)calibrated_counts -
                (int32_t)BRIGHTNESS_SENSOR_CAL_RAW_AT_0_PERCENT;
    }
    else if (BRIGHTNESS_SENSOR_CAL_RAW_AT_100_PERCENT < BRIGHTNESS_SENSOR_CAL_RAW_AT_0_PERCENT)
    {
        range = (int32_t)BRIGHTNESS_SENSOR_CAL_RAW_AT_0_PERCENT -
                (int32_t)BRIGHTNESS_SENSOR_CAL_RAW_AT_100_PERCENT;
        delta = (int32_t)BRIGHTNESS_SENSOR_CAL_RAW_AT_0_PERCENT -
                (int32_t)calibrated_counts;
    }
    else
    {
        return 0u;
    }

    permille = (delta * 1000) / range;
    return Backlight_App_ClampPermille(permille);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: App 단 센서 보정 적용                                           */
/*                                                                            */
/*  순서                                                                      */
/*  1) APP_STATE.brightness.calibrated_counts를 읽는다.                        */
/*  2) App-level count bias를 더한다.                                          */
/*  3) 다시 0~1000 permille로 정규화한다.                                      */
/*  4) App-level permille bias를 한 번 더 더한다.                              */
/*                                                                            */
/*  이렇게 두 단계를 분리해 둔 이유                                            */
/*  - 현장/기구/광학 편차로 인해 raw counts 기준의 밀어주기가 필요한 경우      */
/*  - 사용자 설정 메뉴에서 "조금 더 밝게 / 조금 더 어둡게" 같은 UX bias가      */
/*    필요한 경우                                                             */
/*  를 서로 분리해서 다루기 위해서다.                                          */
/* -------------------------------------------------------------------------- */
static uint16_t Backlight_App_ComputeCorrectedSensorPermille(uint16_t calibrated_counts)
{
    uint16_t biased_counts;
    uint16_t normalized_permille;
    int32_t  biased_permille;

    biased_counts = Backlight_App_ClampToU12((int32_t)calibrated_counts +
                                             (int32_t)g_backlight_app_tuning.sensor_bias_counts);

    normalized_permille = Backlight_App_NormalizeCountsToPermille(biased_counts);

    biased_permille = (int32_t)normalized_permille +
                      (int32_t)g_backlight_app_tuning.sensor_bias_permille;

    return Backlight_App_ClampPermille(biased_permille);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 주변광 mapping curve 평가                                       */
/*                                                                            */
/*  두 테이블 사이를 선형 보간한다.                                            */
/*  curve 포인트를 나중에 settings/calibration 메뉴에서 바꾸고 싶다면          */
/*  이 함수는 그대로 두고 테이블 값만 바꾸면 된다.                             */
/* -------------------------------------------------------------------------- */
static uint16_t Backlight_App_MapAmbientToLinearBrightness(uint16_t sensor_permille)
{
    uint32_t index;

    if (sensor_permille <= g_backlight_app_curve_sensor_permille[0])
    {
        return g_backlight_app_curve_target_permille[0];
    }

    for (index = 0u; index < (BACKLIGHT_APP_CURVE_POINT_COUNT - 1u); index++)
    {
        uint16_t x0;
        uint16_t x1;
        uint16_t y0;
        uint16_t y1;
        uint32_t fraction_num;
        uint32_t fraction_den;
        uint32_t interpolated;

        x0 = g_backlight_app_curve_sensor_permille[index];
        x1 = g_backlight_app_curve_sensor_permille[index + 1u];

        if (sensor_permille > x1)
        {
            continue;
        }

        y0 = g_backlight_app_curve_target_permille[index];
        y1 = g_backlight_app_curve_target_permille[index + 1u];

        if (x1 <= x0)
        {
            return y0;
        }

        fraction_num = (uint32_t)(sensor_permille - x0);
        fraction_den = (uint32_t)(x1 - x0);

        interpolated = ((uint32_t)y0 * (fraction_den - fraction_num)) +
                       ((uint32_t)y1 * fraction_num);
        interpolated = (interpolated + (fraction_den / 2u)) / fraction_den;

        return (uint16_t)interpolated;
    }

    return g_backlight_app_curve_target_permille[BACKLIGHT_APP_CURVE_POINT_COUNT - 1u];
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: user bias와 min/max를 최종 반영                                  */
/* -------------------------------------------------------------------------- */
static uint16_t Backlight_App_ApplyUserBiasAndLimits(uint16_t base_linear_permille)
{
    uint8_t index;
    int32_t biased_value;

    index = Backlight_App_BiasIndexFromSteps(g_backlight_app_tuning.user_bias_steps);

    biased_value = (int32_t)base_linear_permille +
                   (int32_t)g_backlight_app_bias_step_offsets_permille[index];

    return Backlight_App_ClampLinearRange(biased_value);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 센서 필터                                                       */
/*                                                                            */
/*  alpha = 0   -> 이전값 유지                                                 */
/*  alpha = 1000-> 새 값 즉시 반영                                             */
/*                                                                            */
/*  여기서 필터는 "노이즈를 줄이는 1차 필터"일 뿐이다.                         */
/*  실제로 화면 밝기가 바뀌는지 여부는 아래 target decision 로직이             */
/*  따로 한 번 더 판단한다.                                                    */
/* -------------------------------------------------------------------------- */
static uint16_t Backlight_App_FilterSensorPermille(uint16_t previous_permille,
                                                   uint16_t current_permille)
{
    uint16_t alpha;
    int32_t  delta;
    int32_t  filtered;

    alpha = g_backlight_app_tuning.sensor_filter_alpha_permille;

    if (alpha >= 1000u)
    {
        return current_permille;
    }

    delta = (int32_t)current_permille - (int32_t)previous_permille;
    filtered = (int32_t)previous_permille + ((delta * (int32_t)alpha) / 1000);

    return Backlight_App_ClampPermille(filtered);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 실제 화면 밝기 slew                                             */
/*                                                                            */
/*  target이 이미 확정된 뒤에도, 화면 밝기가 순간이동처럼 바뀌지 않도록         */
/*  별도의 올라감/내려감 속도를 둔다.                                          */
/* -------------------------------------------------------------------------- */
static uint16_t Backlight_App_SlewLinearBrightness(uint16_t current_linear,
                                                   uint16_t target_linear,
                                                   uint32_t dt_ms)
{
    uint32_t rate_per_sec;
    uint32_t step;

    if (current_linear == target_linear)
    {
        return current_linear;
    }

    if (dt_ms == 0u)
    {
        dt_ms = 1u;
    }

    /* ---------------------------------------------------------------------- */
    /*  디버깅 중 breakpoint로 멈췄다가 돌아오면 dt가 매우 커질 수 있으므로      */
    /*  한 번에 과도하게 점프하지 않게 상한을 둔다.                             */
    /* ---------------------------------------------------------------------- */
    if (dt_ms > 250u)
    {
        dt_ms = 250u;
    }

    if (target_linear > current_linear)
    {
        rate_per_sec = g_backlight_app_tuning.brighten_slew_permille_per_sec;
    }
    else
    {
        rate_per_sec = g_backlight_app_tuning.darken_slew_permille_per_sec;
    }

    step = (rate_per_sec * dt_ms) / 1000u;
    if (step == 0u)
    {
        step = 1u;
    }

    if (target_linear > current_linear)
    {
        uint32_t next_value;

        next_value = (uint32_t)current_linear + step;
        if (next_value > (uint32_t)target_linear)
        {
            next_value = (uint32_t)target_linear;
        }

        return (uint16_t)next_value;
    }
    else
    {
        int32_t next_value;

        next_value = (int32_t)current_linear - (int32_t)step;
        if (next_value < (int32_t)target_linear)
        {
            next_value = (int32_t)target_linear;
        }

        return (uint16_t)next_value;
    }
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 첫 유효 센서값으로 상태 seed                                     */
/*                                                                            */
/*  중요                                                                      */
/*  - "첫 valid sample"은 noise 억제용 기준점이 아직 없으므로                   */
/*    그 값을 바로 filter/commit의 출발점으로 사용한다.                        */
/*  - 다만 applied_linear_permille은 startup brightness에서 출발해도 되므로,    */
/*    여기서는 target만 확정하고 실제 적용은 slew가 맡는다.                    */
/* -------------------------------------------------------------------------- */
static void Backlight_App_SeedFromFirstValidSensor(uint32_t now_ms,
                                                   uint16_t corrected_sensor_permille)
{
    uint16_t candidate_linear;
    uint16_t target_linear;

    g_backlight_app_runtime.sensor_seeded             = true;
    g_backlight_app_runtime.sensor_valid              = true;
    g_backlight_app_runtime.last_sensor_sample_ms     = now_ms;

    g_backlight_app_runtime.sensor_corrected_permille = corrected_sensor_permille;
    g_backlight_app_runtime.sensor_filtered_permille  = corrected_sensor_permille;
    g_backlight_app_runtime.committed_ambient_permille = corrected_sensor_permille;
    g_backlight_app_runtime.pending_ambient_permille   = corrected_sensor_permille;

    candidate_linear = Backlight_App_MapAmbientToLinearBrightness(corrected_sensor_permille);
    target_linear    = Backlight_App_ApplyUserBiasAndLimits(candidate_linear);

    g_backlight_app_runtime.candidate_linear_permille      = target_linear;
    g_backlight_app_runtime.pending_target_linear_permille = target_linear;
    g_backlight_app_runtime.target_linear_permille         = target_linear;
    g_backlight_app_runtime.pending_retarget               = false;
    g_backlight_app_runtime.pending_direction              = 0;
    g_backlight_app_runtime.last_target_commit_ms          = now_ms;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 현재 filtered ambient 기준으로 후보 밝기 계산                    */
/* -------------------------------------------------------------------------- */
static uint16_t Backlight_App_RebuildCandidateFromFilteredAmbient(void)
{
    uint16_t curve_target;

    curve_target = Backlight_App_MapAmbientToLinearBrightness(
                       g_backlight_app_runtime.sensor_filtered_permille);

    return Backlight_App_ApplyUserBiasAndLimits(curve_target);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 자동 밝기 target 결정 정책                                       */
/*                                                                            */
/*  이 함수가 스마트폰류 UX와 가장 가까운 부분이다.                            */
/*                                                                            */
/*  규칙                                                                      */
/*  1) 작은 주변광 변화는 무시                                                  */
/*  2) 의미 있는 변화라도 hold time 동안 유지되어야 함                         */
/*  3) 밝아질 때 / 어두워질 때 임계치와 hold를 분리                            */
/*  4) target 확정 직후에는 잠깐 재판정을 쉬어 chattering 방지                 */
/* -------------------------------------------------------------------------- */
static void Backlight_App_UpdateAutoTargetPolicy(uint32_t now_ms)
{
    uint16_t candidate_linear;
    uint16_t target_delta;
    uint16_t ambient_delta;
    uint16_t threshold_permille;
    uint16_t hold_ms;
    int8_t   direction;

    candidate_linear = Backlight_App_RebuildCandidateFromFilteredAmbient();
    g_backlight_app_runtime.candidate_linear_permille = candidate_linear;

    direction = 0;
    if (candidate_linear > g_backlight_app_runtime.target_linear_permille)
    {
        direction = 1;
    }
    else if (candidate_linear < g_backlight_app_runtime.target_linear_permille)
    {
        direction = -1;
    }

    /* ---------------------------------------------------------------------- */
    /*  사용자가 settings에서 bias 단계를 바꾼 경우                             */
    /*  - 주변광 변화와 무관하게 현재 ambient 기준 후보를 즉시 반영한다.        */
    /* ---------------------------------------------------------------------- */
    if (g_backlight_app_runtime.applied_user_bias_steps !=
        g_backlight_app_tuning.user_bias_steps)
    {
        g_backlight_app_runtime.target_linear_permille          = candidate_linear;
        g_backlight_app_runtime.pending_target_linear_permille  = candidate_linear;
        g_backlight_app_runtime.pending_retarget                = false;
        g_backlight_app_runtime.pending_direction               = 0;
        g_backlight_app_runtime.last_target_commit_ms           = now_ms;
        g_backlight_app_runtime.committed_ambient_permille      =
            g_backlight_app_runtime.sensor_filtered_permille;
        return;
    }

    if ((now_ms - g_backlight_app_runtime.last_target_commit_ms) <
        g_backlight_app_tuning.post_commit_ignore_ms)
    {
        g_backlight_app_runtime.pending_retarget  = false;
        g_backlight_app_runtime.pending_direction = 0;
        return;
    }

    target_delta = Backlight_App_AbsDiffU16(candidate_linear,
                                            g_backlight_app_runtime.target_linear_permille);
    ambient_delta = Backlight_App_AbsDiffU16(g_backlight_app_runtime.sensor_filtered_permille,
                                             g_backlight_app_runtime.committed_ambient_permille);

    if (direction > 0)
    {
        threshold_permille = g_backlight_app_tuning.ambient_change_threshold_up_permille;
        hold_ms            = g_backlight_app_tuning.brighten_hold_ms;
    }
    else if (direction < 0)
    {
        threshold_permille = g_backlight_app_tuning.ambient_change_threshold_down_permille;
        hold_ms            = g_backlight_app_tuning.darken_hold_ms;
    }
    else
    {
        g_backlight_app_runtime.pending_retarget  = false;
        g_backlight_app_runtime.pending_direction = 0;
        return;
    }

    if ((ambient_delta < threshold_permille) ||
        (target_delta < g_backlight_app_tuning.target_change_threshold_permille))
    {
        g_backlight_app_runtime.pending_retarget  = false;
        g_backlight_app_runtime.pending_direction = 0;
        return;
    }

    if ((g_backlight_app_runtime.pending_retarget == false) ||
        (g_backlight_app_runtime.pending_direction != direction))
    {
        g_backlight_app_runtime.pending_retarget  = true;
        g_backlight_app_runtime.pending_direction = direction;
        g_backlight_app_runtime.pending_since_ms  = now_ms;
    }

    g_backlight_app_runtime.pending_ambient_permille       =
        g_backlight_app_runtime.sensor_filtered_permille;
    g_backlight_app_runtime.pending_target_linear_permille = candidate_linear;

    if ((now_ms - g_backlight_app_runtime.pending_since_ms) >= hold_ms)
    {
        g_backlight_app_runtime.target_linear_permille    =
            g_backlight_app_runtime.pending_target_linear_permille;
        g_backlight_app_runtime.committed_ambient_permille =
            g_backlight_app_runtime.pending_ambient_permille;
        g_backlight_app_runtime.pending_retarget          = false;
        g_backlight_app_runtime.pending_direction         = 0;
        g_backlight_app_runtime.last_target_commit_ms     = now_ms;
    }
}

/* -------------------------------------------------------------------------- */
/*  공개 API: init                                                             */
/*                                                                            */
/*  main.c에서 가능한 한 이른 시점에 호출하는 것을 권장한다.                   */
/*  이유                                                                      */
/*  - gpio.c가 LCD_BACKLIGHT 핀을 low로 내려둔 상태이므로                       */
/*  - 화면 초기화/부트 로고/에러 화면이 실제로 보이려면                         */
/*    백라이트 PWM이 먼저 올라와야 한다.                                       */
/* -------------------------------------------------------------------------- */
void Backlight_App_Init(void)
{
    uint16_t startup_linear;

    memset((void *)&g_backlight_app_runtime, 0, sizeof(g_backlight_app_runtime));

    startup_linear = Backlight_App_ClampLinearRange(
                         (int32_t)g_backlight_app_tuning.startup_linear_permille);

    g_backlight_app_runtime.initialized            = true;
    g_backlight_app_runtime.sensor_seeded          = false;
    g_backlight_app_runtime.sensor_valid           = false;
    g_backlight_app_runtime.pending_retarget       = false;
    g_backlight_app_runtime.pending_direction      = 0;
    g_backlight_app_runtime.auto_mode_active       =
        (g_backlight_app_tuning.auto_enabled != 0u) ? 1u : 0u;
    g_backlight_app_runtime.manual_linear_permille =
        Backlight_App_ClampLinearRange((int32_t)BACKLIGHT_APP_DEFAULT_MANUAL_LINEAR_PERMILLE);

    g_backlight_app_runtime.target_linear_permille  = startup_linear;
    g_backlight_app_runtime.applied_linear_permille = startup_linear;
    g_backlight_app_runtime.applied_user_bias_steps = g_backlight_app_tuning.user_bias_steps;
    g_backlight_app_runtime.last_update_ms          = HAL_GetTick();
    g_backlight_app_runtime.last_target_commit_ms   = HAL_GetTick();

    Backlight_Driver_Init();
    Backlight_Driver_SetLinearPermille(startup_linear);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: auto on/off                                                      */
/* -------------------------------------------------------------------------- */
void Backlight_App_SetAutoEnabled(bool enable)
{
    g_backlight_app_tuning.auto_enabled = (enable == true) ? 1u : 0u;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: manual brightness 설정                                           */
/* -------------------------------------------------------------------------- */
void Backlight_App_SetManualBrightnessPermille(uint16_t linear_permille)
{
    g_backlight_app_runtime.manual_linear_permille =
        Backlight_App_ClampLinearRange((int32_t)linear_permille);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: user bias 단계 설정                                              */
/* -------------------------------------------------------------------------- */
void Backlight_App_SetUserBiasSteps(int8_t bias_steps)
{
    if (bias_steps < -2)
    {
        bias_steps = -2;
    }

    if (bias_steps > 2)
    {
        bias_steps = 2;
    }

    g_backlight_app_tuning.user_bias_steps = bias_steps;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: periodic task                                                    */
/*                                                                            */
/*  main loop 권장 호출 위치                                                   */
/*  - Brightness_Sensor_Task(now_ms) 바로 뒤                                   */
/*  이유                                                                      */
/*  - 같은 loop에서 갱신된 APP_STATE.brightness를                              */
/*    최대한 신선한 상태로 읽기 위함                                           */
/* -------------------------------------------------------------------------- */
void Backlight_App_Task(uint32_t now_ms)
{
    app_brightness_state_t brightness_snapshot;
    uint32_t dt_ms;
    bool     auto_enabled;
    bool     sensor_fresh;

    if (g_backlight_app_runtime.initialized == false)
    {
        Backlight_App_Init();
    }

    dt_ms = now_ms - g_backlight_app_runtime.last_update_ms;
    g_backlight_app_runtime.last_update_ms = now_ms;

    auto_enabled = (g_backlight_app_tuning.auto_enabled != 0u) ? true : false;
    g_backlight_app_runtime.auto_mode_active = auto_enabled ? 1u : 0u;

    sensor_fresh = false;
    memset(&brightness_snapshot, 0, sizeof(brightness_snapshot));

    if (auto_enabled == true)
    {
        APP_STATE_CopyBrightnessSnapshot(&brightness_snapshot);

        if ((brightness_snapshot.valid == true) &&
            ((now_ms - brightness_snapshot.last_update_ms) <=
             g_backlight_app_tuning.sample_stale_timeout_ms))
        {
            uint16_t corrected_sensor_permille;
            uint16_t filtered_sensor_permille;

            sensor_fresh = true;
            g_backlight_app_runtime.sensor_valid = true;
            g_backlight_app_runtime.last_sensor_sample_ms = brightness_snapshot.last_update_ms;
            g_backlight_app_runtime.sensor_raw_counts = brightness_snapshot.calibrated_counts;

            corrected_sensor_permille =
                Backlight_App_ComputeCorrectedSensorPermille(
                    brightness_snapshot.calibrated_counts);

            g_backlight_app_runtime.sensor_corrected_permille = corrected_sensor_permille;

            if (g_backlight_app_runtime.sensor_seeded == false)
            {
                Backlight_App_SeedFromFirstValidSensor(now_ms,
                                                      corrected_sensor_permille);
            }
            else
            {
                filtered_sensor_permille =
                    Backlight_App_FilterSensorPermille(
                        g_backlight_app_runtime.sensor_filtered_permille,
                        corrected_sensor_permille);

                g_backlight_app_runtime.sensor_filtered_permille = filtered_sensor_permille;
            }
        }
        else
        {
            g_backlight_app_runtime.sensor_valid = false;
        }

        if (g_backlight_app_runtime.sensor_seeded == true)
        {
            /* ------------------------------------------------------------------ */
            /*  첫 유효 센서값을 한 번이라도 받은 뒤에는                           */
            /*  새 샘플이 잠깐 끊기더라도 마지막 filtered ambient를 유지한다.      */
            /*  즉, 센서 공백 때문에 화면이 갑자기 fallback으로 튀지 않는다.       */
            /* ------------------------------------------------------------------ */
            Backlight_App_UpdateAutoTargetPolicy(now_ms);
        }
        else
        {
            /* ------------------------------------------------------------------ */
            /*  아직 센서를 한 번도 믿을 수 없을 때의 임시 목표 밝기               */
            /* ------------------------------------------------------------------ */
            g_backlight_app_runtime.target_linear_permille =
                Backlight_App_ApplyUserBiasAndLimits(
                    g_backlight_app_tuning.invalid_sensor_fallback_permille);
            g_backlight_app_runtime.candidate_linear_permille =
                g_backlight_app_runtime.target_linear_permille;
            g_backlight_app_runtime.pending_retarget  = false;
            g_backlight_app_runtime.pending_direction = 0;
        }
    }
    else
    {
        /* ---------------------------------------------------------------------- */
        /*  auto off면 주변광 정책을 모두 멈추고                                  */
        /*  manual brightness를 목표값으로 사용한다.                              */
        /* ---------------------------------------------------------------------- */
        g_backlight_app_runtime.sensor_valid              = false;
        g_backlight_app_runtime.pending_retarget          = false;
        g_backlight_app_runtime.pending_direction         = 0;
        g_backlight_app_runtime.candidate_linear_permille =
            Backlight_App_ClampLinearRange(
                (int32_t)g_backlight_app_runtime.manual_linear_permille);
        g_backlight_app_runtime.target_linear_permille =
            g_backlight_app_runtime.candidate_linear_permille;
    }

    g_backlight_app_runtime.applied_linear_permille =
        Backlight_App_SlewLinearBrightness(
            g_backlight_app_runtime.applied_linear_permille,
            g_backlight_app_runtime.target_linear_permille,
            dt_ms);

    Backlight_Driver_SetLinearPermille(
        g_backlight_app_runtime.applied_linear_permille);

    g_backlight_app_runtime.applied_user_bias_steps =
        g_backlight_app_tuning.user_bias_steps;

    /* -------------------------------------------------------------------------- */
    /*  sensor_fresh 변수는 현재 로직에서 분기 보조용으로만 쓰인다.              */
    /*  나중에 디버그 카운터/통계가 필요하면 여기서 활용하면 된다.               */
    /* -------------------------------------------------------------------------- */
    (void)sensor_fresh;
}
