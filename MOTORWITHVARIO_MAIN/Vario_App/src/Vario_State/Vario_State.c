#include "Vario_State.h"

#include "Vario_Settings.h"
#include "Vario_UiVarioFilter.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#ifndef VARIO_STATE_PI
#define VARIO_STATE_PI 3.14159265358979323846f
#endif

#ifndef VARIO_STATE_PRESSURE_TO_ALT_FACTOR_M
#define VARIO_STATE_PRESSURE_TO_ALT_FACTOR_M 44330.76923f
#endif

#ifndef VARIO_STATE_PRESSURE_TO_ALT_EXPONENT
#define VARIO_STATE_PRESSURE_TO_ALT_EXPONENT 0.190263f
#endif

#ifndef VARIO_STATE_MIN_DT_S
#define VARIO_STATE_MIN_DT_S 0.020f
#endif

#ifndef VARIO_STATE_MAX_DT_S
#define VARIO_STATE_MAX_DT_S 0.250f
#endif

#ifndef VARIO_STATE_GS_TAU_S
#define VARIO_STATE_GS_TAU_S 0.45f
#endif

#ifndef VARIO_STATE_HEADING_TAU_S
#define VARIO_STATE_HEADING_TAU_S 0.25f
#endif

#ifndef VARIO_STATE_PUBLISH_PERIOD_MS
#define VARIO_STATE_PUBLISH_PERIOD_MS 200u
#endif

/* -------------------------------------------------------------------------- */
/*  presentation update cadences                                               */
/*                                                                            */
/*  low-level APP_ALTITUDE는 센서 샘플마다 fast/slow vario를 계속 유지한다.     */
/*  여기서는 그 결과를 다시 한 단계 제품화해서                                */
/*  - 10Hz fast path : vario bar + sound                                       */
/*  -  5Hz slow path : 숫자 display / publish                                  */
/*  로 나눠 운용한다.                                                          */
/* -------------------------------------------------------------------------- */
#ifndef VARIO_STATE_FAST_PRESENT_PERIOD_MS
#define VARIO_STATE_FAST_PRESENT_PERIOD_MS 100u
#endif

#ifndef VARIO_STATE_HEADING_GPS_MIN_SPEED_KMH
#define VARIO_STATE_HEADING_GPS_MIN_SPEED_KMH 4.0f
#endif

#ifndef VARIO_STATE_FLIGHT_START_CONFIRM_MS
#define VARIO_STATE_FLIGHT_START_CONFIRM_MS 2500u
#endif

#ifndef VARIO_STATE_FLIGHT_LAND_CONFIRM_MS
#define VARIO_STATE_FLIGHT_LAND_CONFIRM_MS 60000u
#endif

#ifndef VARIO_STATE_FLIGHT_END_SPEED_KMH
#define VARIO_STATE_FLIGHT_END_SPEED_KMH 2.0f
#endif

#ifndef VARIO_STATE_FLIGHT_END_VARIO_MPS
#define VARIO_STATE_FLIGHT_END_VARIO_MPS 0.30f
#endif

#ifndef VARIO_STATE_ALTITUDE_JUMP_LIMIT_M
#define VARIO_STATE_ALTITUDE_JUMP_LIMIT_M 40.0f
#endif

#ifndef VARIO_STATE_MAX_VARIO_MPS
#define VARIO_STATE_MAX_VARIO_MPS 15.0f
#endif

#ifndef VARIO_STATE_EARTH_METERS_PER_DEG_LAT
#define VARIO_STATE_EARTH_METERS_PER_DEG_LAT 111132.0f
#endif

#ifndef VARIO_STATE_EARTH_METERS_PER_DEG_LON
#define VARIO_STATE_EARTH_METERS_PER_DEG_LON 111319.5f
#endif

#ifndef VARIO_STATE_FAST_BAR_RAW_WINDOW
#define VARIO_STATE_FAST_BAR_RAW_WINDOW 3u
#endif

/* -------------------------------------------------------------------------- */
/* fast vario bar 전용 고속 필터                                               */
/*                                                                            */
/* 목적                                                                       */
/* - 좌측 14px 바는 숫자 표시보다 빠르게 움직여야 한다.                         */
/* - 하지만 raw fast vario 를 그대로 꽂으면 고주파 떨림이 너무 크다.            */
/*                                                                            */
/* 그래서 이 구조체는 "숫자용 robust filter" 와 별도로                         */
/* "bar 전용 fast path filter" 상태만 따로 가진다.                             */
/*                                                                            */
/* 핵심 아이디어                                                               */
/* 1) 3-sample median 으로 단발 spike 를 한 번 자른다.                         */
/* 2) innovation 크기에 따라 alpha 가 달라지는 adaptive LPF 를 쓴다.           */
/* 3) 0 근처는 hysteresis latch 로 떨림을 죽인다.                              */
/*                                                                            */
/* 즉, 단순 게이팅이 아니라                                                     */
/* - spike 억제                                                                */
/* - 빠른 attack                                                                */
/* - 더 느린 release                                                            */
/* - zero hysteresis                                                           */
/* 를 조합한 fast UI 전용 필터다.                                               */
/* -------------------------------------------------------------------------- */
typedef struct
{
    bool     initialized;
    bool     zero_latched;
    uint8_t  hist_count;
    uint8_t  hist_head;
    uint32_t last_update_ms;
    float    raw_hist_mps[VARIO_STATE_FAST_BAR_RAW_WINDOW];
    float    output_mps;
} vario_fast_bar_filter_t;

static struct
{
    vario_mode_t current_mode;
    vario_mode_t previous_main_mode;
    uint8_t settings_cursor;
    uint8_t quickset_cursor;
    uint8_t valuesetting_cursor;
    vario_settings_category_t settings_category;
    uint8_t redraw_request;

    /* ---------------------------------------------------------------------- */
    /* runtime                                                                 */
    /* - 페이지 렌더러가 직접 읽는 공개 runtime                                 */
    /* ---------------------------------------------------------------------- */
    vario_runtime_t runtime;

    /* ---------------------------------------------------------------------- */
    /* UI current vario 전용 robust filter                                     */
    /* ---------------------------------------------------------------------- */
    vario_ui_vario_filter_t ui_vario_filter;

    /* ---------------------------------------------------------------------- */
    /* 좌측 side bar fast vario 전용 filter                                    */
    /* ---------------------------------------------------------------------- */
    vario_fast_bar_filter_t fast_bar_filter;

    /* ---------------------------------------------------------------------- */
    /* presentation scheduler                                                  */
    /*                                                                        */
    /*  last_fast_present_ms : 10Hz bar/audio update cadence                   */
    /*  last_slow_present_ms :  5Hz display current-vario cadence              */
    /* ---------------------------------------------------------------------- */
    uint32_t last_fast_present_ms;
    uint32_t last_slow_present_ms;
} s_vario_state;

static float vario_state_absf(float value)
{
    return (value < 0.0f) ? -value : value;
}

static float vario_state_clampf(float value, float min_v, float max_v)
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

static float vario_state_lpf_alpha(float dt_s, float tau_s)
{
    if (tau_s <= 0.0f)
    {
        return 1.0f;
    }

    return dt_s / (tau_s + dt_s);
}

static float vario_state_wrap_360(float deg)
{
    while (deg < 0.0f)
    {
        deg += 360.0f;
    }

    while (deg >= 360.0f)
    {
        deg -= 360.0f;
    }

    return deg;
}

static float vario_state_wrap_pm180(float deg)
{
    while (deg > 180.0f)
    {
        deg -= 360.0f;
    }

    while (deg < -180.0f)
    {
        deg += 360.0f;
    }

    return deg;
}

static float vario_state_pressure_to_altitude_m(float pressure_hpa, float qnh_hpa)
{
    if ((pressure_hpa <= 0.0f) || (qnh_hpa <= 0.0f))
    {
        return 0.0f;
    }

    return VARIO_STATE_PRESSURE_TO_ALT_FACTOR_M *
           (1.0f - powf((pressure_hpa / qnh_hpa), VARIO_STATE_PRESSURE_TO_ALT_EXPONENT));
}

static float vario_state_deg_to_rad(float deg)
{
    return deg * (VARIO_STATE_PI / 180.0f);
}

static uint8_t vario_state_wrap_cursor(uint8_t cursor, uint8_t count, int8_t direction)
{
    int32_t next;

    next = (int32_t)cursor + (int32_t)direction;

    if (next < 0)
    {
        next = (int32_t)count - 1;
    }
    else if (next >= (int32_t)count)
    {
        next = 0;
    }

    return (uint8_t)next;
}

/* -------------------------------------------------------------------------- */
/* snapshot capture                                                            */
/*                                                                            */
/* 이 함수는 APP_STATE 공개 API만 사용한다.                                    */
/* 센서 드라이버 raw register 와는 완전히 절연되어 있다.                       */
/* -------------------------------------------------------------------------- */
static void vario_state_capture_snapshots(void)
{
    APP_STATE_CopyGy86Snapshot(&s_vario_state.runtime.gy86);
    APP_STATE_CopyGpsSnapshot(&s_vario_state.runtime.gps);
    APP_STATE_CopyDs18b20Snapshot(&s_vario_state.runtime.ds18b20);
    APP_STATE_CopyAltitudeSnapshot(&s_vario_state.runtime.altitude);
    APP_STATE_CopyBikeSnapshot(&s_vario_state.runtime.bike);
    APP_STATE_CopyClockSnapshot(&s_vario_state.runtime.clock);
}

/* -------------------------------------------------------------------------- */
/* pressure / temperature visible cache                                        */
/* -------------------------------------------------------------------------- */
static void vario_state_update_sensor_caches(void)
{
    const app_altitude_state_t *alt;
    const app_gy86_state_t     *gy86;
    const app_ds18b20_state_t  *ds;
    const app_gps_state_t      *gps;

    alt = &s_vario_state.runtime.altitude;
    gy86 = &s_vario_state.runtime.gy86;
    ds = &s_vario_state.runtime.ds18b20;
    gps = &s_vario_state.runtime.gps;

    s_vario_state.runtime.baro_valid = ((alt->initialized != false) && (alt->baro_valid != false));
    s_vario_state.runtime.gps_valid = ((gps->fix.valid != false) &&
                                       (gps->fix.fixOk != false) &&
                                       (gps->fix.fixType != 0u));

    s_vario_state.runtime.gps_time_valid = ((gps->fix.valid_time != 0u) && (gps->fix.valid != false));
    s_vario_state.runtime.gps_hour = gps->fix.hour;
    s_vario_state.runtime.gps_min  = gps->fix.min;
    s_vario_state.runtime.gps_sec  = gps->fix.sec;

    if (s_vario_state.runtime.baro_valid != false)
    {
        s_vario_state.runtime.pressure_hpa_x100 = alt->pressure_filt_hpa_x100;
        s_vario_state.runtime.pressure_hpa = ((float)alt->pressure_filt_hpa_x100) * 0.01f;
    }
    else if ((gy86->status_flags & APP_GY86_STATUS_BARO_VALID) != 0u)
    {
        s_vario_state.runtime.pressure_hpa_x100 = gy86->baro.pressure_hpa_x100;
        s_vario_state.runtime.pressure_hpa = ((float)gy86->baro.pressure_hpa_x100) * 0.01f;
    }

    if (((ds->status_flags & APP_DS18B20_STATUS_VALID) != 0u) &&
        (ds->raw.temp_c_x100 != APP_DS18B20_TEMP_INVALID))
    {
        s_vario_state.runtime.temp_valid = true;
        s_vario_state.runtime.temperature_c_x100 = ds->raw.temp_c_x100;
        s_vario_state.runtime.temperature_c = ((float)ds->raw.temp_c_x100) * 0.01f;
    }
    else if ((gy86->status_flags & APP_GY86_STATUS_BARO_VALID) != 0u)
    {
        s_vario_state.runtime.temp_valid = true;
        s_vario_state.runtime.temperature_c_x100 = gy86->baro.temp_cdeg;
        s_vario_state.runtime.temperature_c = ((float)gy86->baro.temp_cdeg) * 0.01f;
    }
    else
    {
        s_vario_state.runtime.temp_valid = false;
    }

    if (s_vario_state.runtime.gps_valid != false)
    {
        s_vario_state.runtime.gps_altitude_m = ((float)gps->fix.hMSL) * 0.001f;
    }
}

static float vario_state_pick_fast_vario_mps(const app_altitude_state_t *alt,
                                             const vario_settings_t *settings)
{
    if ((alt == NULL) || (settings == NULL))
    {
        return 0.0f;
    }

    switch (settings->altitude_source)
    {
        case VARIO_ALT_SOURCE_FUSED_NOIMU:
            return ((float)alt->vario_fast_noimu_cms) * 0.01f;

        case VARIO_ALT_SOURCE_FUSED_IMU:
            if (alt->imu_vector_valid != false)
            {
                return ((float)alt->vario_fast_imu_cms) * 0.01f;
            }
            return ((float)alt->vario_fast_noimu_cms) * 0.01f;

        case VARIO_ALT_SOURCE_QNH_MANUAL:
            return ((float)alt->vario_fast_noimu_cms) * 0.01f;

        case VARIO_ALT_SOURCE_DISPLAY:
        case VARIO_ALT_SOURCE_GPS_HMSL:
        case VARIO_ALT_SOURCE_COUNT:
        default:
            if (alt->imu_vector_valid != false)
            {
                return ((float)alt->vario_fast_imu_cms) * 0.01f;
            }
            return ((float)alt->vario_fast_noimu_cms) * 0.01f;
    }
}

static float vario_state_pick_slow_vario_mps(const app_altitude_state_t *alt,
                                             const vario_settings_t *settings)
{
    if ((alt == NULL) || (settings == NULL))
    {
        return 0.0f;
    }

    switch (settings->altitude_source)
    {
        case VARIO_ALT_SOURCE_FUSED_NOIMU:
            return ((float)alt->vario_slow_noimu_cms) * 0.01f;

        case VARIO_ALT_SOURCE_FUSED_IMU:
            if (alt->imu_vector_valid != false)
            {
                return ((float)alt->vario_slow_imu_cms) * 0.01f;
            }
            return ((float)alt->vario_slow_noimu_cms) * 0.01f;

        case VARIO_ALT_SOURCE_QNH_MANUAL:
            return ((float)alt->vario_slow_noimu_cms) * 0.01f;

        case VARIO_ALT_SOURCE_DISPLAY:
        case VARIO_ALT_SOURCE_GPS_HMSL:
        case VARIO_ALT_SOURCE_COUNT:
        default:
            if (alt->imu_vector_valid != false)
            {
                return ((float)alt->vario_slow_imu_cms) * 0.01f;
            }
            return ((float)alt->vario_slow_noimu_cms) * 0.01f;
    }
}

/* -------------------------------------------------------------------------- */
/* altitude / vario measurement selection                                      */
/*                                                                            */
/* 화면용 고도는 반드시 APP_STATE 고수준 결과를 기반으로 한다.                */
/* manual QNH 경로도 예외가 아니다.                                           */
/*                                                                            */
/* 이전 구현은 VARIO local settings에 저장된 QNH로 다시 pressure->altitude    */
/* 재계산을 수행했다.                                                         */
/* 그 방식은 low-level APP_ALTITUDE가 실제로 쓰는                             */
/* APP_STATE.settings.altitude.manual_qnh_hpa_x100 과                         */
/* upper VARIO local mirror가 어긋나면 split-brain을 만들 수 있었다.          */
/*                                                                            */
/* 그래서 이제 manual QNH source도                                            */
/*   APP_STATE.altitude.alt_qnh_manual_cm                                     */
/* 을 그대로 사용한다.                                                        */
/* 즉, "manual QNH로 계산된 결과" 의 canonical owner 역시 APP_ALTITUDE다.    */
/* -------------------------------------------------------------------------- */
static bool vario_state_select_source_altitude_cm(const app_altitude_state_t *alt,
                                                const vario_settings_t *settings,
                                                int32_t *out_altitude_cm)
{
    if ((alt == NULL) || (settings == NULL) || (out_altitude_cm == NULL))
    {
        return false;
    }

    if (alt->initialized == false)
    {
        return false;
    }

    switch (settings->altitude_source)
    {
        case VARIO_ALT_SOURCE_QNH_MANUAL:
            if ((alt->baro_valid == false) ||
                (alt->qnh_manual_hpa_x100 <= 0))
            {
                return false;
            }

            *out_altitude_cm = alt->alt_qnh_manual_cm;
            return true;

        case VARIO_ALT_SOURCE_FUSED_NOIMU:
            if (alt->baro_valid == false)
            {
                return false;
            }

            *out_altitude_cm = alt->alt_fused_noimu_cm;
            return true;

        case VARIO_ALT_SOURCE_FUSED_IMU:
            if ((alt->baro_valid == false) && (alt->imu_vector_valid == false))
            {
                return false;
            }

            if (alt->imu_vector_valid != false)
            {
                *out_altitude_cm = alt->alt_fused_imu_cm;
            }
            else
            {
                *out_altitude_cm = alt->alt_fused_noimu_cm;
            }
            return true;

        case VARIO_ALT_SOURCE_GPS_HMSL:
            if (alt->gps_valid == false)
            {
                return false;
            }

            *out_altitude_cm = alt->alt_gps_hmsl_cm;
            return true;

        case VARIO_ALT_SOURCE_DISPLAY:
        case VARIO_ALT_SOURCE_COUNT:
        default:
            if (alt->baro_valid == false)
            {
                return false;
            }

            *out_altitude_cm = alt->alt_display_cm;
            return true;
    }
}

static bool vario_state_select_measurement(float *out_altitude_m, float *out_vario_mps)
{
    const vario_settings_t     *settings;
    const app_altitude_state_t *alt;
    float                       fast_vario_mps;
    float                       slow_vario_mps;
    float                       slow_weight;
    int32_t                     selected_altitude_cm;

    settings = Vario_Settings_Get();
    alt = &s_vario_state.runtime.altitude;

    if ((out_altitude_m == NULL) || (out_vario_mps == NULL))
    {
        return false;
    }

    if ((settings == NULL) || (alt->initialized == false))
    {
        return false;
    }

    if (vario_state_select_source_altitude_cm(alt,
                                              settings,
                                              &selected_altitude_cm) == false)
    {
        return false;
    }

    fast_vario_mps = vario_state_pick_fast_vario_mps(alt, settings);
    slow_vario_mps = vario_state_pick_slow_vario_mps(alt, settings);

    slow_weight = ((float)(settings->digital_vario_average_seconds - 1u)) / 7.0f;
    slow_weight = vario_state_clampf(slow_weight, 0.0f, 1.0f);

    *out_altitude_m = ((float)selected_altitude_cm) * 0.01f;
    *out_vario_mps = (fast_vario_mps * (1.0f - slow_weight)) + (slow_vario_mps * slow_weight);
    return true;
}

/* -------------------------------------------------------------------------- */
/* fast bar filter helper                                                      */
/* -------------------------------------------------------------------------- */
static void vario_state_fast_bar_push(vario_fast_bar_filter_t *filter, float value)
{
    if (filter == NULL)
    {
        return;
    }

    filter->raw_hist_mps[filter->hist_head] = value;
    filter->hist_head = (uint8_t)((filter->hist_head + 1u) % VARIO_STATE_FAST_BAR_RAW_WINDOW);
    if (filter->hist_count < VARIO_STATE_FAST_BAR_RAW_WINDOW)
    {
        ++filter->hist_count;
    }
}

static float vario_state_median_small(float *values, uint8_t count)
{
    uint8_t i;
    uint8_t j;
    float   key;

    if ((values == NULL) || (count == 0u))
    {
        return 0.0f;
    }

    for (i = 1u; i < count; ++i)
    {
        key = values[i];
        j = i;
        while ((j > 0u) && (values[j - 1u] > key))
        {
            values[j] = values[j - 1u];
            --j;
        }
        values[j] = key;
    }

    if ((count & 1u) != 0u)
    {
        return values[count / 2u];
    }

    return (values[(count / 2u) - 1u] + values[count / 2u]) * 0.5f;
}

static void vario_state_fast_bar_reset(vario_fast_bar_filter_t *filter,
                                       float input_mps,
                                       uint32_t timestamp_ms)
{
    uint8_t i;

    if (filter == NULL)
    {
        return;
    }

    memset(filter, 0, sizeof(*filter));
    filter->initialized = true;
    filter->last_update_ms = timestamp_ms;
    filter->output_mps = input_mps;
    filter->zero_latched = (vario_state_absf(input_mps) < 0.08f) ? true : false;

    for (i = 0u; i < VARIO_STATE_FAST_BAR_RAW_WINDOW; ++i)
    {
        filter->raw_hist_mps[i] = input_mps;
    }
    filter->hist_count = VARIO_STATE_FAST_BAR_RAW_WINDOW;
    filter->hist_head = 0u;
}

static float vario_state_fast_bar_update(vario_fast_bar_filter_t *filter,
                                         float input_mps,
                                         uint32_t timestamp_ms)
{
    float hist[VARIO_STATE_FAST_BAR_RAW_WINDOW];
    float robust_input_mps;
    float dt_s;
    float innovation_mps;
    float tau_s;
    float alpha;
    float max_step_mps;
    uint8_t i;

    if (filter == NULL)
    {
        return input_mps;
    }

    input_mps = vario_state_clampf(input_mps, -20.0f, 20.0f);

    if ((filter->initialized == false) || (timestamp_ms == 0u))
    {
        vario_state_fast_bar_reset(filter, input_mps, timestamp_ms);
        return filter->output_mps;
    }

    if (timestamp_ms == filter->last_update_ms)
    {
        return filter->output_mps;
    }

    dt_s = ((float)(timestamp_ms - filter->last_update_ms)) * 0.001f;
    dt_s = vario_state_clampf(dt_s, 0.010f, 0.250f);

    vario_state_fast_bar_push(filter, input_mps);
    for (i = 0u; i < filter->hist_count; ++i)
    {
        hist[i] = filter->raw_hist_mps[i];
    }
    robust_input_mps = vario_state_median_small(hist, filter->hist_count);

    innovation_mps = robust_input_mps - filter->output_mps;

    /* ---------------------------------------------------------------------- */
    /* attack / release                                                        */
    /* - 같은 부호 방향으로 더 멀어질 때는 빠르게 따라간다.                     */
    /* - 반대로 되돌아올 때는 조금 더 천천히 움직인다.                          */
    /* - 0 crossing 부근은 별도 tau 로 너무 날카로운 뒤집힘을 누른다.            */
    /* ---------------------------------------------------------------------- */
    if (((robust_input_mps >= 0.0f) && (filter->output_mps >= 0.0f) && (robust_input_mps > filter->output_mps)) ||
        ((robust_input_mps <= 0.0f) && (filter->output_mps <= 0.0f) && (robust_input_mps < filter->output_mps)))
    {
        tau_s = 0.045f;
    }
    else
    {
        tau_s = 0.110f;
    }

    if (((robust_input_mps > 0.0f) && (filter->output_mps < 0.0f)) ||
        ((robust_input_mps < 0.0f) && (filter->output_mps > 0.0f)))
    {
        tau_s = 0.070f;
    }

    alpha = vario_state_lpf_alpha(dt_s, tau_s);

    /* ---------------------------------------------------------------------- */
    /* innovation clamp                                                        */
    /* - 완전한 게이팅이 아니라, 한 step 에 허용할 변화량만 제한한다.            */
    /* - 따라서 스파이크는 줄이되 실제 상승 진입/하강 진입은 빠르게 보인다.      */
    /* ---------------------------------------------------------------------- */
    max_step_mps = 0.20f + (9.0f * dt_s) + (0.15f * vario_state_absf(innovation_mps));
    innovation_mps = vario_state_clampf(innovation_mps, -max_step_mps, +max_step_mps);

    filter->output_mps += alpha * innovation_mps;

    if (filter->zero_latched != false)
    {
        if (vario_state_absf(robust_input_mps) > 0.18f)
        {
            filter->zero_latched = false;
        }
        else
        {
            filter->output_mps = 0.0f;
        }
    }
    else if ((vario_state_absf(robust_input_mps) < 0.06f) &&
             (vario_state_absf(filter->output_mps) < 0.12f))
    {
        filter->zero_latched = true;
        filter->output_mps = 0.0f;
    }

    filter->output_mps = vario_state_clampf(filter->output_mps, -20.0f, 20.0f);
    filter->last_update_ms = timestamp_ms;
    return filter->output_mps;
}

static void vario_state_update_ground_speed(void)
{
    const app_gps_state_t *gps;
    float                  raw_ground_speed_kmh;
    float                  dt_s;
    float                  alpha;

    gps = &s_vario_state.runtime.gps;

    if ((gps->fix.valid == false) ||
        (gps->fix.fixOk == false) ||
        (gps->fix.fixType == 0u))
    {
        s_vario_state.runtime.gps_valid = false;
        s_vario_state.runtime.gs_bar_speed_kmh = 0.0f;
        return;
    }

    raw_ground_speed_kmh = ((float)gps->fix.gSpeed) * 0.0036f;
    raw_ground_speed_kmh = vario_state_clampf(raw_ground_speed_kmh, 0.0f, 250.0f);
    s_vario_state.runtime.gs_bar_speed_kmh = raw_ground_speed_kmh;

    if (gps->fix.last_update_ms == 0u)
    {
        return;
    }

    if (s_vario_state.runtime.last_gps_host_time_ms == 0u)
    {
        s_vario_state.runtime.filtered_ground_speed_kmh = raw_ground_speed_kmh;
        s_vario_state.runtime.last_gps_host_time_ms = gps->fix.last_update_ms;
        return;
    }

    if (gps->fix.last_update_ms == s_vario_state.runtime.last_gps_host_time_ms)
    {
        return;
    }

    dt_s = ((float)(gps->fix.last_update_ms - s_vario_state.runtime.last_gps_host_time_ms)) * 0.001f;
    dt_s = vario_state_clampf(dt_s, VARIO_STATE_MIN_DT_S, 0.300f);

    alpha = vario_state_lpf_alpha(dt_s, VARIO_STATE_GS_TAU_S);
    s_vario_state.runtime.filtered_ground_speed_kmh +=
        alpha * (raw_ground_speed_kmh - s_vario_state.runtime.filtered_ground_speed_kmh);

    s_vario_state.runtime.last_gps_host_time_ms = gps->fix.last_update_ms;
}

static bool vario_state_select_heading_measurement(float *out_heading_deg, uint8_t *out_source)
{
    const vario_settings_t *settings;
    const app_bike_state_t *bike;
    const app_gps_state_t  *gps;
    float                   heading_deg;
    float                   speed_kmh;

    settings = Vario_Settings_Get();
    bike = &s_vario_state.runtime.bike;
    gps = &s_vario_state.runtime.gps;
    speed_kmh = s_vario_state.runtime.filtered_ground_speed_kmh;

    if ((out_heading_deg == NULL) || (out_source == NULL))
    {
        return false;
    }

    if ((settings->heading_source == VARIO_HEADING_SOURCE_AUTO) ||
        (settings->heading_source == VARIO_HEADING_SOURCE_BIKE))
    {
        if (bike->heading_valid != false)
        {
            heading_deg = ((float)bike->heading_deg_x10) * 0.1f;
            *out_heading_deg = vario_state_wrap_360(heading_deg);
            *out_source = 1u;
            return true;
        }

        if (settings->heading_source == VARIO_HEADING_SOURCE_BIKE)
        {
            return false;
        }
    }

    if ((settings->heading_source == VARIO_HEADING_SOURCE_AUTO) ||
        (settings->heading_source == VARIO_HEADING_SOURCE_GPS))
    {
        if ((gps->fix.valid != false) &&
            (gps->fix.fixOk != false) &&
            (gps->fix.fixType != 0u) &&
            (speed_kmh >= VARIO_STATE_HEADING_GPS_MIN_SPEED_KMH))
        {
            if (gps->fix.head_veh_valid != 0u)
            {
                heading_deg = ((float)gps->fix.headVeh) * 0.00001f;
                *out_heading_deg = vario_state_wrap_360(heading_deg);
                *out_source = 2u;
                return true;
            }

            heading_deg = ((float)gps->fix.headMot) * 0.00001f;
            *out_heading_deg = vario_state_wrap_360(heading_deg);
            *out_source = 3u;
            return true;
        }
    }

    return false;
}

static void vario_state_update_heading(uint32_t now_ms)
{
    float   measured_heading_deg;
    uint8_t source;
    float   dt_s;
    float   alpha;
    float   delta;

    if (vario_state_select_heading_measurement(&measured_heading_deg, &source) == false)
    {
        return;
    }

    if (s_vario_state.runtime.heading_valid == false)
    {
        s_vario_state.runtime.heading_deg = measured_heading_deg;
        s_vario_state.runtime.last_heading_deg = measured_heading_deg;
        s_vario_state.runtime.heading_source = source;
        s_vario_state.runtime.heading_valid = true;
        return;
    }

    dt_s = ((float)(now_ms - s_vario_state.runtime.last_task_ms)) * 0.001f;
    dt_s = vario_state_clampf(dt_s, VARIO_STATE_MIN_DT_S, 0.150f);
    alpha = vario_state_lpf_alpha(dt_s, VARIO_STATE_HEADING_TAU_S);

    delta = vario_state_wrap_pm180(measured_heading_deg - s_vario_state.runtime.heading_deg);
    s_vario_state.runtime.heading_deg = vario_state_wrap_360(s_vario_state.runtime.heading_deg + (alpha * delta));
    s_vario_state.runtime.last_heading_deg = s_vario_state.runtime.heading_deg;
    s_vario_state.runtime.heading_source = source;
    s_vario_state.runtime.heading_valid = true;
}

/* -------------------------------------------------------------------------- */
/* app-layer display filter                                                    */
/*                                                                            */
/* 설계 분리                                                                   */
/* 1) altitude                                                                 */
/*    - observer 기반 altitude filter 유지                                    */
/* 2) current vario                                                            */
/*    - slow vario -> robust UI filter -> 5Hz publish                         */
/* 3) fast vario side bar                                                      */
/*    - fast vario -> fast_bar_filter -> raw-ish high update                  */
/* -------------------------------------------------------------------------- */
static void vario_state_update_display_filter(uint32_t now_ms)
{
    const vario_settings_t     *settings;
    const app_altitude_state_t *alt;
    float measured_altitude_m;
    float measured_vario_mps;
    float ui_vario_input_mps;
    float fast_bar_input_mps;
    uint32_t filter_timestamp_ms;
    float dt_s;
    float damping_norm;
    float alt_tau_s;
    float vel_tau_s;
    float alt_alpha;
    float vel_alpha;
    float predicted_altitude_m;
    float residual_m;
    bool fast_present_due;
    bool slow_present_due;

    settings = Vario_Settings_Get();
    alt = &s_vario_state.runtime.altitude;

    if (vario_state_select_measurement(&measured_altitude_m, &measured_vario_mps) == false)
    {
        s_vario_state.runtime.altitude_valid = false;
        return;
    }

    s_vario_state.runtime.altitude_valid = true;
    s_vario_state.runtime.raw_selected_altitude_m = measured_altitude_m;
    s_vario_state.runtime.raw_selected_vario_mps = measured_vario_mps;

    ui_vario_input_mps = vario_state_pick_slow_vario_mps(alt, settings);
    fast_bar_input_mps = vario_state_pick_fast_vario_mps(alt, settings);
    filter_timestamp_ms = (alt->last_update_ms != 0u) ? alt->last_update_ms : now_ms;

    if ((s_vario_state.runtime.last_altitude_update_ms != 0u) &&
        (alt->last_update_ms == s_vario_state.runtime.last_altitude_update_ms))
    {
        return;
    }

    if ((s_vario_state.runtime.derived_valid == false) ||
        (s_vario_state.runtime.last_altitude_update_ms == 0u) ||
        (alt->last_update_ms == 0u))
    {
        s_vario_state.runtime.filtered_altitude_m = measured_altitude_m;
        s_vario_state.runtime.filtered_vario_mps = ui_vario_input_mps;
        s_vario_state.runtime.fast_vario_bar_mps = fast_bar_input_mps;
        s_vario_state.runtime.observer_velocity_mps = measured_vario_mps;
        s_vario_state.runtime.last_measured_altitude_m = measured_altitude_m;
        s_vario_state.runtime.last_accum_altitude_m = measured_altitude_m;
        s_vario_state.runtime.last_altitude_update_ms = alt->last_update_ms;
        s_vario_state.runtime.derived_valid = true;
        s_vario_state.last_fast_present_ms = filter_timestamp_ms;
        s_vario_state.last_slow_present_ms = filter_timestamp_ms;

        Vario_UiVarioFilter_Reset(&s_vario_state.ui_vario_filter,
                                  ui_vario_input_mps,
                                  filter_timestamp_ms);
        vario_state_fast_bar_reset(&s_vario_state.fast_bar_filter,
                                   fast_bar_input_mps,
                                   filter_timestamp_ms);
        return;
    }

    dt_s = ((float)(alt->last_update_ms - s_vario_state.runtime.last_altitude_update_ms)) * 0.001f;
    dt_s = vario_state_clampf(dt_s, VARIO_STATE_MIN_DT_S, VARIO_STATE_MAX_DT_S);

    damping_norm = ((float)(settings->vario_damping_level - 1u)) / 9.0f;
    damping_norm = vario_state_clampf(damping_norm, 0.0f, 1.0f);

    alt_tau_s = 0.18f + (damping_norm * 1.00f);
    vel_tau_s = 0.12f + (damping_norm * 0.60f);

    predicted_altitude_m = s_vario_state.runtime.filtered_altitude_m +
                           (s_vario_state.runtime.observer_velocity_mps * dt_s);

    residual_m = measured_altitude_m - predicted_altitude_m;
    residual_m = vario_state_clampf(residual_m,
                                    -VARIO_STATE_ALTITUDE_JUMP_LIMIT_M,
                                    +VARIO_STATE_ALTITUDE_JUMP_LIMIT_M);

    alt_alpha = vario_state_lpf_alpha(dt_s, alt_tau_s);
    vel_alpha = vario_state_lpf_alpha(dt_s, vel_tau_s);

    s_vario_state.runtime.filtered_altitude_m = predicted_altitude_m + (alt_alpha * residual_m);
    s_vario_state.runtime.observer_velocity_mps += (alt_alpha * 0.75f) * (residual_m / dt_s);
    s_vario_state.runtime.observer_velocity_mps +=
        vel_alpha * (measured_vario_mps - s_vario_state.runtime.observer_velocity_mps);

    s_vario_state.runtime.observer_velocity_mps =
        vario_state_clampf(s_vario_state.runtime.observer_velocity_mps,
                           -VARIO_STATE_MAX_VARIO_MPS,
                           +VARIO_STATE_MAX_VARIO_MPS);

    fast_present_due = false;
    slow_present_due = false;

    if ((s_vario_state.last_fast_present_ms == 0u) ||
        ((uint32_t)(filter_timestamp_ms - s_vario_state.last_fast_present_ms) >= VARIO_STATE_FAST_PRESENT_PERIOD_MS))
    {
        fast_present_due = true;
    }

    if ((s_vario_state.last_slow_present_ms == 0u) ||
        ((uint32_t)(filter_timestamp_ms - s_vario_state.last_slow_present_ms) >= VARIO_STATE_PUBLISH_PERIOD_MS))
    {
        slow_present_due = true;
    }

    if (fast_present_due != false)
    {
        s_vario_state.runtime.fast_vario_bar_mps =
            vario_state_fast_bar_update(&s_vario_state.fast_bar_filter,
                                        fast_bar_input_mps,
                                        filter_timestamp_ms);
        s_vario_state.last_fast_present_ms = filter_timestamp_ms;
    }

    if (slow_present_due != false)
    {
        s_vario_state.runtime.filtered_vario_mps =
            Vario_UiVarioFilter_Update(&s_vario_state.ui_vario_filter,
                                       ui_vario_input_mps,
                                       filter_timestamp_ms,
                                       settings->vario_damping_level,
                                       settings->digital_vario_average_seconds);
        s_vario_state.last_slow_present_ms = filter_timestamp_ms;
    }

    s_vario_state.runtime.last_measured_altitude_m = measured_altitude_m;
    s_vario_state.runtime.last_altitude_update_ms = alt->last_update_ms;
}

static void vario_state_update_flight_logic(uint32_t now_ms)
{
    const vario_settings_t *settings;
    float                   start_speed_kmh;
    float                   positive_gain_step_m;

    settings = Vario_Settings_Get();
    start_speed_kmh = ((float)settings->flight_start_speed_kmh_x10) * 0.1f;

    if ((s_vario_state.runtime.flight_active == false) &&
        ((s_vario_state.runtime.filtered_ground_speed_kmh >= start_speed_kmh) ||
         (vario_state_absf(s_vario_state.runtime.filtered_vario_mps) >= 0.8f)))
    {
        if (s_vario_state.runtime.flight_takeoff_candidate_ms == 0u)
        {
            s_vario_state.runtime.flight_takeoff_candidate_ms = now_ms;
        }
        else if ((now_ms - s_vario_state.runtime.flight_takeoff_candidate_ms) >=
                 VARIO_STATE_FLIGHT_START_CONFIRM_MS)
        {
            s_vario_state.runtime.flight_active = true;
            s_vario_state.runtime.flight_start_ms = now_ms;
            s_vario_state.runtime.flight_landing_candidate_ms = 0u;
            s_vario_state.runtime.flight_time_s = 0u;
            s_vario_state.runtime.alt3_accum_gain_m = 0.0f;
            s_vario_state.runtime.max_top_vario_mps = 0.0f;
            s_vario_state.runtime.max_speed_kmh = 0.0f;
            s_vario_state.runtime.last_accum_altitude_m = s_vario_state.runtime.filtered_altitude_m;
            s_vario_state.redraw_request = 1u;
        }
    }
    else if ((s_vario_state.runtime.flight_active == false) &&
             (s_vario_state.runtime.filtered_ground_speed_kmh < start_speed_kmh) &&
             (vario_state_absf(s_vario_state.runtime.filtered_vario_mps) < 0.8f))
    {
        s_vario_state.runtime.flight_takeoff_candidate_ms = 0u;
    }

    if (s_vario_state.runtime.flight_active != false)
    {
        s_vario_state.runtime.flight_time_s = (now_ms - s_vario_state.runtime.flight_start_ms) / 1000u;

        if (s_vario_state.runtime.filtered_vario_mps > s_vario_state.runtime.max_top_vario_mps)
        {
            s_vario_state.runtime.max_top_vario_mps = s_vario_state.runtime.filtered_vario_mps;
        }

        if (s_vario_state.runtime.filtered_ground_speed_kmh > s_vario_state.runtime.max_speed_kmh)
        {
            s_vario_state.runtime.max_speed_kmh = s_vario_state.runtime.filtered_ground_speed_kmh;
        }

        positive_gain_step_m = s_vario_state.runtime.filtered_altitude_m - s_vario_state.runtime.last_accum_altitude_m;
        if (positive_gain_step_m > 0.0f)
        {
            s_vario_state.runtime.alt3_accum_gain_m += positive_gain_step_m;
        }
        s_vario_state.runtime.last_accum_altitude_m = s_vario_state.runtime.filtered_altitude_m;

        if ((s_vario_state.runtime.filtered_ground_speed_kmh <= VARIO_STATE_FLIGHT_END_SPEED_KMH) &&
            (vario_state_absf(s_vario_state.runtime.filtered_vario_mps) <= VARIO_STATE_FLIGHT_END_VARIO_MPS))
        {
            if (s_vario_state.runtime.flight_landing_candidate_ms == 0u)
            {
                s_vario_state.runtime.flight_landing_candidate_ms = now_ms;
            }
            else if ((now_ms - s_vario_state.runtime.flight_landing_candidate_ms) >=
                     VARIO_STATE_FLIGHT_LAND_CONFIRM_MS)
            {
                s_vario_state.runtime.flight_active = false;
                s_vario_state.runtime.flight_takeoff_candidate_ms = 0u;
                s_vario_state.runtime.flight_landing_candidate_ms = 0u;
                s_vario_state.redraw_request = 1u;
            }
        }
        else
        {
            s_vario_state.runtime.flight_landing_candidate_ms = 0u;
        }
    }
}

static float vario_state_distance_between_ll_deg_e7(int32_t lat1_e7,
                                                    int32_t lon1_e7,
                                                    int32_t lat2_e7,
                                                    int32_t lon2_e7)
{
    float lat1_deg;
    float lat2_deg;
    float lon1_deg;
    float lon2_deg;
    float mean_lat_rad;
    float dx_m;
    float dy_m;

    lat1_deg = ((float)lat1_e7) * 1.0e-7f;
    lat2_deg = ((float)lat2_e7) * 1.0e-7f;
    lon1_deg = ((float)lon1_e7) * 1.0e-7f;
    lon2_deg = ((float)lon2_e7) * 1.0e-7f;

    mean_lat_rad = vario_state_deg_to_rad((lat1_deg + lat2_deg) * 0.5f);
    dx_m = (lon2_deg - lon1_deg) * (VARIO_STATE_EARTH_METERS_PER_DEG_LON * cosf(mean_lat_rad));
    dy_m = (lat2_deg - lat1_deg) * VARIO_STATE_EARTH_METERS_PER_DEG_LAT;

    return sqrtf((dx_m * dx_m) + (dy_m * dy_m));
}

static void vario_state_push_trail_point(int32_t lat_e7, int32_t lon_e7, uint32_t stamp_ms)
{
    uint8_t write_index;

    write_index = s_vario_state.runtime.trail_head;
    s_vario_state.runtime.trail_lat_e7[write_index] = lat_e7;
    s_vario_state.runtime.trail_lon_e7[write_index] = lon_e7;
    s_vario_state.runtime.trail_stamp_ms[write_index] = stamp_ms;

    s_vario_state.runtime.trail_head = (uint8_t)((write_index + 1u) % VARIO_TRAIL_MAX_POINTS);
    if (s_vario_state.runtime.trail_count < VARIO_TRAIL_MAX_POINTS)
    {
        ++s_vario_state.runtime.trail_count;
    }

    s_vario_state.runtime.trail_valid = (s_vario_state.runtime.trail_count > 1u) ? true : false;
    s_vario_state.redraw_request = 1u;
}

static void vario_state_update_trail(void)
{
    const vario_settings_t *settings;
    const app_gps_state_t  *gps;
    uint8_t                 newest_index;
    float                   dist_m;
    uint32_t                age_ms;
    uint32_t                interval_ms;

    settings = Vario_Settings_Get();
    gps = &s_vario_state.runtime.gps;

    if ((gps->fix.valid == false) ||
        (gps->fix.fixOk == false) ||
        (gps->fix.fixType == 0u))
    {
        return;
    }

    if (s_vario_state.runtime.trail_count == 0u)
    {
        vario_state_push_trail_point(gps->fix.lat, gps->fix.lon, gps->fix.last_update_ms);
        return;
    }

    newest_index = (s_vario_state.runtime.trail_head == 0u) ?
                       (VARIO_TRAIL_MAX_POINTS - 1u) :
                       (uint8_t)(s_vario_state.runtime.trail_head - 1u);

    dist_m = vario_state_distance_between_ll_deg_e7(s_vario_state.runtime.trail_lat_e7[newest_index],
                                                    s_vario_state.runtime.trail_lon_e7[newest_index],
                                                    gps->fix.lat,
                                                    gps->fix.lon);
    age_ms = gps->fix.last_update_ms - s_vario_state.runtime.trail_stamp_ms[newest_index];
    interval_ms = 5000u;
    if ((settings != NULL) && (settings->log_interval_seconds != 0u))
    {
        interval_ms = (uint32_t)settings->log_interval_seconds * 1000u;
    }
    if ((settings != NULL) && (settings->log_enabled == 0u))
    {
        interval_ms = 0xFFFFFFFFu;
    }

    if ((dist_m >= (float)settings->trail_spacing_m) || (age_ms >= interval_ms))
    {
        vario_state_push_trail_point(gps->fix.lat, gps->fix.lon, gps->fix.last_update_ms);
    }
}

static void vario_state_update_clock(void)
{
    uint8_t prev_hour;
    uint8_t prev_min;
    uint8_t prev_sec;

    prev_hour = s_vario_state.runtime.local_hour;
    prev_min  = s_vario_state.runtime.local_min;
    prev_sec  = s_vario_state.runtime.local_sec;

    if ((s_vario_state.runtime.clock.rtc_time_valid != false) ||
        (s_vario_state.runtime.clock.rtc_read_valid != false))
    {
        s_vario_state.runtime.clock_valid = true;
        s_vario_state.runtime.local_hour = s_vario_state.runtime.clock.local.hour;
        s_vario_state.runtime.local_min  = s_vario_state.runtime.clock.local.min;
        s_vario_state.runtime.local_sec  = s_vario_state.runtime.clock.local.sec;
    }
    else if (s_vario_state.runtime.gps_valid != false)
    {
        s_vario_state.runtime.clock_valid = true;
        s_vario_state.runtime.local_hour = s_vario_state.runtime.gps.fix.hour;
        s_vario_state.runtime.local_min  = s_vario_state.runtime.gps.fix.min;
        s_vario_state.runtime.local_sec  = s_vario_state.runtime.gps.fix.sec;
    }
    else
    {
        s_vario_state.runtime.clock_valid = false;
    }

    if ((prev_hour != s_vario_state.runtime.local_hour) ||
        (prev_min  != s_vario_state.runtime.local_min)  ||
        (prev_sec  != s_vario_state.runtime.local_sec))
    {
        s_vario_state.redraw_request = 1u;
    }
}

static void vario_state_history_push(float altitude_m, float vario_mps, float speed_kmh)
{
    uint16_t idx;

    idx = s_vario_state.runtime.history_head;
    s_vario_state.runtime.history_altitude_m[idx] = altitude_m;
    s_vario_state.runtime.history_vario_mps[idx] = vario_mps;
    s_vario_state.runtime.history_speed_kmh[idx] = speed_kmh;

    s_vario_state.runtime.history_head = (uint16_t)((idx + 1u) % VARIO_HISTORY_MAX_SAMPLES);
    if (s_vario_state.runtime.history_count < VARIO_HISTORY_MAX_SAMPLES)
    {
        ++s_vario_state.runtime.history_count;
    }
}

static float vario_state_average_recent(const float *history,
                                        uint16_t count,
                                        uint16_t head,
                                        uint16_t want_samples)
{
    uint16_t used;
    uint16_t i;
    float    sum;

    if ((history == NULL) || (count == 0u))
    {
        return 0.0f;
    }

    used = (want_samples < count) ? want_samples : count;
    if (used == 0u)
    {
        return 0.0f;
    }

    sum = 0.0f;
    for (i = 0u; i < used; ++i)
    {
        uint16_t idx;
        idx = (uint16_t)((head + VARIO_HISTORY_MAX_SAMPLES - 1u - i) % VARIO_HISTORY_MAX_SAMPLES);
        sum += history[idx];
    }

    return sum / (float)used;
}

static void vario_state_update_integrated_metrics(void)
{
    const vario_settings_t *settings;
    uint16_t                samples;
    float                   avg_vario_mps;
    float                   avg_speed_kmh;
    float                   sink_mps;

    settings = Vario_Settings_Get();

    samples = (uint16_t)(((uint32_t)settings->digital_vario_average_seconds * 1000u) /
                         VARIO_STATE_PUBLISH_PERIOD_MS);
    if (samples == 0u)
    {
        samples = 1u;
    }

    avg_vario_mps = vario_state_average_recent(s_vario_state.runtime.history_vario_mps,
                                               s_vario_state.runtime.history_count,
                                               s_vario_state.runtime.history_head,
                                               samples);
    avg_speed_kmh = vario_state_average_recent(s_vario_state.runtime.history_speed_kmh,
                                               s_vario_state.runtime.history_count,
                                               s_vario_state.runtime.history_head,
                                               samples);

    s_vario_state.runtime.average_vario_mps = avg_vario_mps;

    sink_mps = -avg_vario_mps;
    if ((sink_mps > 0.15f) && (avg_speed_kmh > 1.0f))
    {
        s_vario_state.runtime.glide_ratio = (avg_speed_kmh / 3.6f) / sink_mps;
        s_vario_state.runtime.glide_ratio = vario_state_clampf(s_vario_state.runtime.glide_ratio, 0.0f, 99.9f);
        s_vario_state.runtime.glide_ratio_valid = true;
    }
    else
    {
        s_vario_state.runtime.glide_ratio = 0.0f;
        s_vario_state.runtime.glide_ratio_valid = false;
    }
}

static void vario_state_publish_5hz(uint32_t now_ms)
{
    const vario_settings_t     *settings;
    const app_altitude_state_t *alt;
    float                       quant_alt_m;
    float                       quant_vario_mps;
    float                       quant_gs_kmh;
    float                       speed_delta_kmh;
    int32_t                     selected_altitude_cm;
    int32_t                     alt2_relative_cm;

    if (s_vario_state.runtime.derived_valid == false)
    {
        return;
    }

    if ((s_vario_state.runtime.last_publish_ms != 0u) &&
        ((now_ms - s_vario_state.runtime.last_publish_ms) < VARIO_STATE_PUBLISH_PERIOD_MS))
    {
        return;
    }

    settings = Vario_Settings_Get();
    alt = &s_vario_state.runtime.altitude;

    quant_alt_m = roundf(s_vario_state.runtime.filtered_altitude_m);
    quant_vario_mps = roundf(s_vario_state.runtime.filtered_vario_mps * 10.0f) * 0.1f;
    quant_gs_kmh = roundf(s_vario_state.runtime.filtered_ground_speed_kmh * 10.0f) * 0.1f;

    /* ---------------------------------------------------------------------- */
    /*  absolute / relative altitude publish path                              */
    /*                                                                        */
    /*  핵심 수정                                                              */
    /*  - 숫자 UI cadence는 계속 5Hz 이지만                                     */
    /*  - ALT1 / ALT2 backing field의 source는 1m 양자화된 quant_alt_m 가      */
    /*    아니라 canonical centimeter source다.                               */
    /*                                                                        */
    /*  즉, feet 표시가 meter 표시의 1m 계단을 뒤따라가는 문제가 여기서 끊긴다. */
    /* ---------------------------------------------------------------------- */
    if ((settings != NULL) &&
        (vario_state_select_source_altitude_cm(alt,
                                               settings,
                                               &selected_altitude_cm) != false))
    {
        alt2_relative_cm = selected_altitude_cm - settings->alt2_reference_cm;
    }
    else
    {
        selected_altitude_cm = (int32_t)lroundf(s_vario_state.runtime.filtered_altitude_m * 100.0f);
        alt2_relative_cm = selected_altitude_cm;
    }

    s_vario_state.runtime.baro_altitude_m = quant_alt_m;
    s_vario_state.runtime.alt1_absolute_m = ((float)selected_altitude_cm) * 0.01f;
    s_vario_state.runtime.baro_vario_mps = quant_vario_mps;
    s_vario_state.runtime.ground_speed_kmh = quant_gs_kmh;
    s_vario_state.runtime.alt2_relative_m = ((float)alt2_relative_cm) * 0.01f;

    if (alt->baro_valid != false)
    {
        s_vario_state.runtime.pressure_altitude_std_m =
            ((float)alt->alt_pressure_std_cm) * 0.01f;
    }
    else
    {
        s_vario_state.runtime.pressure_altitude_std_m = 0.0f;
    }

    s_vario_state.runtime.last_publish_ms = now_ms;

    speed_delta_kmh = quant_gs_kmh - s_vario_state.runtime.last_published_ground_speed_kmh;
    if (speed_delta_kmh > 0.2f)
    {
        s_vario_state.runtime.speed_trend = 1;
    }
    else if (speed_delta_kmh < -0.2f)
    {
        s_vario_state.runtime.speed_trend = -1;
    }
    else
    {
        s_vario_state.runtime.speed_trend = 0;
    }
    s_vario_state.runtime.last_published_ground_speed_kmh = quant_gs_kmh;

    vario_state_history_push(quant_alt_m, quant_vario_mps, quant_gs_kmh);
    vario_state_update_integrated_metrics();

    s_vario_state.redraw_request = 1u;
}

void Vario_State_Init(void)
{
    memset(&s_vario_state, 0, sizeof(s_vario_state));

    Vario_UiVarioFilter_Init(&s_vario_state.ui_vario_filter);

    s_vario_state.current_mode       = VARIO_MODE_SCREEN_1;
    s_vario_state.previous_main_mode = VARIO_MODE_SCREEN_1;
    s_vario_state.settings_category  = VARIO_SETTINGS_CATEGORY_SYSTEM;
    s_vario_state.redraw_request     = 1u;
}

void Vario_State_Task(uint32_t now_ms)
{
    vario_state_capture_snapshots();
    vario_state_update_sensor_caches();
    vario_state_update_ground_speed();
    vario_state_update_display_filter(now_ms);
    vario_state_update_heading(now_ms);
    vario_state_update_clock();
    vario_state_update_flight_logic(now_ms);
    vario_state_update_trail();
    vario_state_publish_5hz(now_ms);

    s_vario_state.runtime.last_task_ms = now_ms;
}

vario_mode_t Vario_State_GetMode(void)
{
    return s_vario_state.current_mode;
}

void Vario_State_SetMode(vario_mode_t mode)
{
    if (mode >= VARIO_MODE_COUNT)
    {
        return;
    }

    s_vario_state.current_mode = mode;

    if ((mode == VARIO_MODE_SCREEN_1) ||
        (mode == VARIO_MODE_SCREEN_2) ||
        (mode == VARIO_MODE_SCREEN_3))
    {
        s_vario_state.previous_main_mode = mode;
    }

    s_vario_state.redraw_request = 1u;
}

void Vario_State_ReturnToMain(void)
{
    Vario_State_SetMode(s_vario_state.previous_main_mode);
}

void Vario_State_EnterSettings(void)
{
    Vario_State_SetMode(VARIO_MODE_SETTING);
}

void Vario_State_EnterQuickSet(void)
{
    if (s_vario_state.quickset_cursor >= (uint8_t)VARIO_SETTINGS_CATEGORY_COUNT)
    {
        s_vario_state.quickset_cursor = 0u;
    }

    s_vario_state.settings_category = (vario_settings_category_t)s_vario_state.quickset_cursor;
    Vario_State_SetMode(VARIO_MODE_QUICKSET);
}

void Vario_State_EnterValueSetting(void)
{
    uint8_t count;

    if (s_vario_state.quickset_cursor >= (uint8_t)VARIO_SETTINGS_CATEGORY_COUNT)
    {
        s_vario_state.quickset_cursor = 0u;
    }

    s_vario_state.settings_category = (vario_settings_category_t)s_vario_state.quickset_cursor;
    count = Vario_Settings_GetCategoryItemCount(s_vario_state.settings_category);
    if ((count == 0u) || (s_vario_state.valuesetting_cursor >= count))
    {
        s_vario_state.valuesetting_cursor = 0u;
    }

    Vario_State_SetMode(VARIO_MODE_VALUESETTING);
}

void Vario_State_SelectPreviousMainScreen(void)
{
    switch (s_vario_state.previous_main_mode)
    {
        case VARIO_MODE_SCREEN_1:
            Vario_State_SetMode(VARIO_MODE_SCREEN_3);
            break;

        case VARIO_MODE_SCREEN_2:
            Vario_State_SetMode(VARIO_MODE_SCREEN_1);
            break;

        case VARIO_MODE_SCREEN_3:
        default:
            Vario_State_SetMode(VARIO_MODE_SCREEN_2);
            break;
    }
}

void Vario_State_SelectNextMainScreen(void)
{
    switch (s_vario_state.previous_main_mode)
    {
        case VARIO_MODE_SCREEN_1:
            Vario_State_SetMode(VARIO_MODE_SCREEN_2);
            break;

        case VARIO_MODE_SCREEN_2:
            Vario_State_SetMode(VARIO_MODE_SCREEN_3);
            break;

        case VARIO_MODE_SCREEN_3:
        default:
            Vario_State_SetMode(VARIO_MODE_SCREEN_1);
            break;
    }
}

uint8_t Vario_State_GetSettingsCursor(void)
{
    return s_vario_state.settings_cursor;
}

uint8_t Vario_State_GetQuickSetCursor(void)
{
    return s_vario_state.quickset_cursor;
}

uint8_t Vario_State_GetValueSettingCursor(void)
{
    return s_vario_state.valuesetting_cursor;
}

vario_settings_category_t Vario_State_GetSettingsCategory(void)
{
    return s_vario_state.settings_category;
}

void Vario_State_SetSettingsCategory(vario_settings_category_t category)
{
    uint8_t count;

    if (category >= VARIO_SETTINGS_CATEGORY_COUNT)
    {
        return;
    }

    s_vario_state.settings_category = category;
    s_vario_state.quickset_cursor = (uint8_t)category;

    count = Vario_Settings_GetCategoryItemCount(category);
    if (count == 0u)
    {
        s_vario_state.valuesetting_cursor = 0u;
    }
    else if (s_vario_state.valuesetting_cursor >= count)
    {
        s_vario_state.valuesetting_cursor = 0u;
    }

    s_vario_state.redraw_request = 1u;
}

void Vario_State_MoveSettingsCursor(int8_t direction)
{
    s_vario_state.settings_cursor =
        vario_state_wrap_cursor(s_vario_state.settings_cursor,
                                (uint8_t)VARIO_SETTING_MENU_COUNT,
                                direction);
    s_vario_state.redraw_request = 1u;
}

void Vario_State_MoveQuickSetCursor(int8_t direction)
{
    s_vario_state.quickset_cursor =
        vario_state_wrap_cursor(s_vario_state.quickset_cursor,
                                (uint8_t)VARIO_SETTINGS_CATEGORY_COUNT,
                                direction);
    s_vario_state.settings_category = (vario_settings_category_t)s_vario_state.quickset_cursor;
    s_vario_state.valuesetting_cursor = 0u;
    s_vario_state.redraw_request = 1u;
}

void Vario_State_MoveValueSettingCursor(int8_t direction)
{
    uint8_t count;

    count = Vario_Settings_GetCategoryItemCount(s_vario_state.settings_category);
    if (count == 0u)
    {
        s_vario_state.valuesetting_cursor = 0u;
    }
    else
    {
        s_vario_state.valuesetting_cursor =
            vario_state_wrap_cursor(s_vario_state.valuesetting_cursor,
                                    count,
                                    direction);
    }
    s_vario_state.redraw_request = 1u;
}

const vario_runtime_t *Vario_State_GetRuntime(void)
{
    return &s_vario_state.runtime;
}

bool Vario_State_GetSelectedAltitudeCm(int32_t *out_altitude_cm)
{
    const vario_settings_t *settings;

    settings = Vario_Settings_Get();
    if (out_altitude_cm == NULL)
    {
        return false;
    }

    return vario_state_select_source_altitude_cm(&s_vario_state.runtime.altitude,
                                                 settings,
                                                 out_altitude_cm);
}

void Vario_State_ResetAccumulatedGain(void)
{
    s_vario_state.runtime.alt3_accum_gain_m = 0.0f;
    s_vario_state.runtime.max_top_vario_mps = 0.0f;
    s_vario_state.runtime.last_accum_altitude_m = s_vario_state.runtime.filtered_altitude_m;
    s_vario_state.redraw_request = 1u;
}

void Vario_State_ResetFlightMetrics(void)
{
    memset(s_vario_state.runtime.trail_lat_e7, 0, sizeof(s_vario_state.runtime.trail_lat_e7));
    memset(s_vario_state.runtime.trail_lon_e7, 0, sizeof(s_vario_state.runtime.trail_lon_e7));
    memset(s_vario_state.runtime.trail_stamp_ms, 0, sizeof(s_vario_state.runtime.trail_stamp_ms));
    memset(s_vario_state.runtime.history_altitude_m, 0, sizeof(s_vario_state.runtime.history_altitude_m));
    memset(s_vario_state.runtime.history_vario_mps, 0, sizeof(s_vario_state.runtime.history_vario_mps));
    memset(s_vario_state.runtime.history_speed_kmh, 0, sizeof(s_vario_state.runtime.history_speed_kmh));

    s_vario_state.runtime.flight_active = false;
    s_vario_state.runtime.flight_takeoff_candidate_ms = 0u;
    s_vario_state.runtime.flight_landing_candidate_ms = 0u;
    s_vario_state.runtime.flight_start_ms = 0u;
    s_vario_state.runtime.flight_time_s = 0u;
    s_vario_state.runtime.trail_head = 0u;
    s_vario_state.runtime.trail_count = 0u;
    s_vario_state.runtime.trail_valid = false;
    s_vario_state.runtime.history_head = 0u;
    s_vario_state.runtime.history_count = 0u;
    s_vario_state.runtime.alt3_accum_gain_m = 0.0f;
    s_vario_state.runtime.max_top_vario_mps = 0.0f;
    s_vario_state.runtime.max_speed_kmh = 0.0f;
    s_vario_state.runtime.average_vario_mps = 0.0f;
    s_vario_state.runtime.glide_ratio = 0.0f;
    s_vario_state.runtime.glide_ratio_valid = false;
    s_vario_state.runtime.last_accum_altitude_m = s_vario_state.runtime.filtered_altitude_m;
    s_vario_state.redraw_request = 1u;
}

void Vario_State_RequestRedraw(void)
{
    s_vario_state.redraw_request = 1u;
}

void Vario_State_ClearRedrawRequest(void)
{
    s_vario_state.redraw_request = 0u;
}

bool Vario_State_IsRedrawRequested(void)
{
    return (s_vario_state.redraw_request != 0u) ? true : false;
}
