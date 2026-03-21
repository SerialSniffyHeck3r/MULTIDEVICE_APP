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

#ifndef VARIO_STATE_VARIO_NEAR_ZERO_MPS
#define VARIO_STATE_VARIO_NEAR_ZERO_MPS 0.03f
#endif

#ifndef VARIO_STATE_EARTH_METERS_PER_DEG_LAT
#define VARIO_STATE_EARTH_METERS_PER_DEG_LAT 111132.0f
#endif

#ifndef VARIO_STATE_EARTH_METERS_PER_DEG_LON
#define VARIO_STATE_EARTH_METERS_PER_DEG_LON 111319.5f
#endif

static struct
{
    vario_mode_t current_mode;
    vario_mode_t previous_main_mode;
    uint8_t settings_cursor;
    uint8_t quickset_cursor;
    uint8_t valuesetting_cursor;
    uint8_t redraw_request;

    /* ---------------------------------------------------------------------- */
    /* runtime                                                                */
    /* - 페이지 렌더러가 직접 읽는 공개 runtime                                */
    /* ---------------------------------------------------------------------- */
    vario_runtime_t runtime;

    /* ---------------------------------------------------------------------- */
    /* UI vario 전용 robust filter 상태                                        */
    /* - main screen 의 큰/작은 vario 숫자와 오른쪽 side bar 에                */
    /*   최종적으로 반영될 display vario 를 만든다.                             */
    /* - APP_STATE slow vario snapshot 만 입력으로 사용한다.                   */
    /* ---------------------------------------------------------------------- */
    vario_ui_vario_filter_t ui_vario_filter;
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
/*  snapshot capture                                                          */
/*                                                                            */
/*  이 함수는 APP_STATE 공개 API만 사용한다.                                   */
/*  센서 드라이버 raw register 와는 완전히 절연되어 있다.                      */
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
/*  pressure / temperature visible cache                                       */
/*                                                                            */
/*  화면 자체는 고도 계산에 APP_ALTITUDE snapshot 을 주로 사용하지만,            */
/*  raw overlay 및 진단 문자열을 위해 pressure/temperature cache 는 유지한다.    */
/* -------------------------------------------------------------------------- */
static void vario_state_update_sensor_caches(void)
{
    const app_altitude_state_t *alt;
    const app_gy86_state_t     *gy86;
    const app_ds18b20_state_t  *ds;

    alt = &s_vario_state.runtime.altitude;
    gy86 = &s_vario_state.runtime.gy86;
    ds = &s_vario_state.runtime.ds18b20;

    s_vario_state.runtime.baro_valid = ((alt->initialized != false) && (alt->baro_valid != false));
    s_vario_state.runtime.gps_valid = ((s_vario_state.runtime.gps.fix.valid != false) &&
                                       (s_vario_state.runtime.gps.fix.fixOk != false) &&
                                       (s_vario_state.runtime.gps.fix.fixType != 0u));

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
        /* ------------------------------------------------------------------ */
        /* 외부 온도 프로브가 없을 때는 baro 온도를 임시 진단값으로만 사용한다. */
        /* ------------------------------------------------------------------ */
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
        s_vario_state.runtime.gps_altitude_m = ((float)s_vario_state.runtime.gps.fix.hMSL) * 0.001f;
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

        case VARIO_ALT_SOURCE_DISPLAY:
        case VARIO_ALT_SOURCE_QNH_MANUAL:
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

        case VARIO_ALT_SOURCE_DISPLAY:
        case VARIO_ALT_SOURCE_QNH_MANUAL:
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
/*  altitude / vario measurement selection                                    */
/*                                                                            */
/*  화면용 고도는 반드시 APP_STATE 고수준 결과를 기반으로 한다.                */
/*  단, Flytec 스타일의 manual QNH 고도를 위해 pressure_filt_hpa_x100 을       */
/*  이용한 재계산 경로를 허용한다.                                              */
/* -------------------------------------------------------------------------- */
static bool vario_state_select_measurement(float *out_altitude_m, float *out_vario_mps)
{
    const vario_settings_t   *settings;
    const app_altitude_state_t *alt;
    float                     fast_vario_mps;
    float                     slow_vario_mps;
    float                     slow_weight;
    float                     altitude_m;

    settings = Vario_Settings_Get();
    alt = &s_vario_state.runtime.altitude;

    if ((out_altitude_m == NULL) || (out_vario_mps == NULL))
    {
        return false;
    }

    if (alt->initialized == false)
    {
        return false;
    }

    fast_vario_mps = vario_state_pick_fast_vario_mps(alt, settings);
    slow_vario_mps = vario_state_pick_slow_vario_mps(alt, settings);

    /* ---------------------------------------------------------------------- */
    /* integrated vario 개념                                                   */
    /* - average seconds 가 1에 가까울수록 fast 쪽 비중이 커진다.              */
    /* - average seconds 가 클수록 slow 쪽 비중이 커진다.                      */
    /* ---------------------------------------------------------------------- */
    slow_weight = ((float)(settings->digital_vario_average_seconds - 1u)) / 7.0f;
    slow_weight = vario_state_clampf(slow_weight, 0.0f, 1.0f);

    switch (settings->altitude_source)
    {
        case VARIO_ALT_SOURCE_QNH_MANUAL:
            if ((alt->baro_valid == false) ||
                (alt->pressure_filt_hpa_x100 <= 0) ||
                (settings->qnh_hpa_x100 <= 0))
            {
                return false;
            }

            altitude_m = vario_state_pressure_to_altitude_m(((float)alt->pressure_filt_hpa_x100) * 0.01f,
                                                            ((float)settings->qnh_hpa_x100) * 0.01f);
            *out_altitude_m = altitude_m;
            *out_vario_mps = ((float)alt->baro_vario_filt_cms) * 0.01f;
            return true;

        case VARIO_ALT_SOURCE_FUSED_NOIMU:
            if (alt->baro_valid == false)
            {
                return false;
            }
            *out_altitude_m = ((float)alt->alt_fused_noimu_cm) * 0.01f;
            *out_vario_mps = (fast_vario_mps * (1.0f - slow_weight)) + (slow_vario_mps * slow_weight);
            return true;

        case VARIO_ALT_SOURCE_FUSED_IMU:
            if ((alt->baro_valid == false) && (alt->imu_vector_valid == false))
            {
                return false;
            }
            if (alt->imu_vector_valid != false)
            {
                *out_altitude_m = ((float)alt->alt_fused_imu_cm) * 0.01f;
            }
            else
            {
                *out_altitude_m = ((float)alt->alt_fused_noimu_cm) * 0.01f;
            }
            *out_vario_mps = (fast_vario_mps * (1.0f - slow_weight)) + (slow_vario_mps * slow_weight);
            return true;

        case VARIO_ALT_SOURCE_GPS_HMSL:
            if (alt->gps_valid == false)
            {
                return false;
            }
            *out_altitude_m = ((float)alt->alt_gps_hmsl_cm) * 0.01f;
            *out_vario_mps = (fast_vario_mps * (1.0f - slow_weight)) + (slow_vario_mps * slow_weight);
            return true;

        case VARIO_ALT_SOURCE_DISPLAY:
        case VARIO_ALT_SOURCE_COUNT:
        default:
            if (alt->baro_valid == false)
            {
                return false;
            }
            *out_altitude_m = ((float)alt->alt_display_cm) * 0.01f;
            *out_vario_mps = (fast_vario_mps * (1.0f - slow_weight)) + (slow_vario_mps * slow_weight);
            return true;
    }
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
        return;
    }

    raw_ground_speed_kmh = ((float)gps->fix.gSpeed) * 0.0036f;
    raw_ground_speed_kmh = vario_state_clampf(raw_ground_speed_kmh, 0.0f, 250.0f);

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
    float    measured_heading_deg;
    uint8_t  source;
    float    dt_s;
    float    alpha;
    float    delta;

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
/* app-layer display filter                                                   */
/*                                                                            */
/* 설계 분리                                                                  */
/* 1) altitude                                                                 */
/*    - 기존 observer 기반 altitude filter 를 유지한다.                       */
/*    - 이유: altitude 쪽은 누적고도/상대고도 계산과도 연결되어 있기 때문.   */
/*                                                                            */
/* 2) current vario                                                            */
/*    - legacy debug 에서 이미 sane 했던 APP_ALTITUDE slow_vario 를           */
/*      별도의 robust UI filter 에 태운다.                                    */
/*    - 즉, 현재 화면에 보이는 baro_vario_mps 는                               */
/*      "selected measurement 의 직통값" 이 아니라                             */
/*      "slow_vario + outlier reject + adaptive smooth + zero hysteresis"     */
/*      경로를 탄 display 전용 값이 된다.                                     */
/*                                                                            */
/* 3) 경계 규칙                                                                */
/*    - 이 함수는 APP_STATE snapshot 복사본만 사용한다.                        */
/*    - 저수준 sensor driver / register / bus 데이터는 직접 읽지 않는다.      */
/* -------------------------------------------------------------------------- */
static void vario_state_update_display_filter(uint32_t now_ms)
{
    const vario_settings_t *settings;
    const app_altitude_state_t *alt;
    float measured_altitude_m;
    float measured_vario_mps;
    float ui_vario_input_mps;
    uint32_t filter_timestamp_ms;
    float dt_s;
    float damping_norm;
    float alt_tau_s;
    float vel_tau_s;
    float alt_alpha;
    float vel_alpha;
    float predicted_altitude_m;
    float residual_m;

    settings = Vario_Settings_Get();
    alt = &s_vario_state.runtime.altitude;

    /* ---------------------------------------------------------------------- */
    /* 1) selected altitude / legacy selected vario 확보                       */
    /* - altitude observer 는 기존 selected measurement 경로를 그대로 쓴다.    */
    /* - raw_selected_* 는 진단 관찰용으로 그대로 보존한다.                    */
    /* ---------------------------------------------------------------------- */
    if (vario_state_select_measurement(&measured_altitude_m, &measured_vario_mps) == false)
    {
        s_vario_state.runtime.altitude_valid = false;
        return;
    }

    s_vario_state.runtime.altitude_valid = true;
    s_vario_state.runtime.raw_selected_altitude_m = measured_altitude_m;
    s_vario_state.runtime.raw_selected_vario_mps = measured_vario_mps;

    /* ---------------------------------------------------------------------- */
    /* 2) UI current vario 입력원                                              */
    /* - main UI 가 사용할 current vario 는 오직 slow_vario 를 쓴다.           */
    /* - QNH manual 이든 fused/noimu 이든 altitude source 선택을 따라          */
    /*   적절한 slow path 를 vario_state_pick_slow_vario_mps() 가 고른다.      */
    /* ---------------------------------------------------------------------- */
    ui_vario_input_mps = vario_state_pick_slow_vario_mps(alt, settings);
    filter_timestamp_ms = (alt->last_update_ms != 0u) ? alt->last_update_ms : now_ms;

    /* ---------------------------------------------------------------------- */
    /* 3) 같은 altitude snapshot 에 대해 중복 계산하지 않음                    */
    /* ---------------------------------------------------------------------- */
    if ((s_vario_state.runtime.last_altitude_update_ms != 0u) &&
        (alt->last_update_ms == s_vario_state.runtime.last_altitude_update_ms))
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /* 4) 첫 유효 샘플 진입                                                     */
    /* - altitude / observer / ui filter 모두 같은 시작점으로 맞춘다.          */
    /* - 시작 프레임에서 숫자가 튀지 않도록 UI filter ring buffer 를            */
    /*   첫 샘플 값으로 채운다.                                                */
    /* ---------------------------------------------------------------------- */
    if ((s_vario_state.runtime.derived_valid == false) ||
        (s_vario_state.runtime.last_altitude_update_ms == 0u) ||
        (alt->last_update_ms == 0u))
    {
        s_vario_state.runtime.filtered_altitude_m = measured_altitude_m;

        /* ------------------------------------------------------------------ */
        /* filtered_vario_mps 는 이제 UI display 전용 robust current vario 다. */
        /* 이 값은 이후 5Hz publish 에서 round(0.1m/s) 되어                    */
        /* Screen 1 의 큰 숫자/작은 숫자/세로 막대에 직접 쓰인다.              */
        /* ------------------------------------------------------------------ */
        s_vario_state.runtime.filtered_vario_mps = ui_vario_input_mps;

        /* ------------------------------------------------------------------ */
        /* altitude observer 의 내부 속도 상태는 기존 selected vario 로 초기화 */
        /* - altitude prediction 안정성을 위해 기존 의미를 유지한다.           */
        /* ------------------------------------------------------------------ */
        s_vario_state.runtime.observer_velocity_mps = measured_vario_mps;

        s_vario_state.runtime.last_measured_altitude_m = measured_altitude_m;
        s_vario_state.runtime.last_accum_altitude_m = measured_altitude_m;
        s_vario_state.runtime.last_altitude_update_ms = alt->last_update_ms;
        s_vario_state.runtime.derived_valid = true;

        Vario_UiVarioFilter_Reset(&s_vario_state.ui_vario_filter,
                                  ui_vario_input_mps,
                                  filter_timestamp_ms);
        return;
    }

    /* ---------------------------------------------------------------------- */
    /* 5) altitude observer update                                              */
    /* - altitude / accumulated gain 경로는 기존 observer 수식을 유지한다.     */
    /* ---------------------------------------------------------------------- */
    dt_s = ((float)(alt->last_update_ms - s_vario_state.runtime.last_altitude_update_ms)) * 0.001f;
    dt_s = vario_state_clampf(dt_s, VARIO_STATE_MIN_DT_S, VARIO_STATE_MAX_DT_S);

    /* ---------------------------------------------------------------------- */
    /* damping level -> time constant                                           */
    /* - level 1  : 민감                                                        */
    /* - level 10 : 묵직                                                        */
    /* ---------------------------------------------------------------------- */
    damping_norm = ((float)(settings->vario_damping_level - 1u)) / 9.0f;
    damping_norm = vario_state_clampf(damping_norm, 0.0f, 1.0f);

    alt_tau_s = 0.18f + (damping_norm * 1.00f);
    vel_tau_s = 0.12f + (damping_norm * 0.60f);

    predicted_altitude_m = s_vario_state.runtime.filtered_altitude_m +
                           (s_vario_state.runtime.observer_velocity_mps * dt_s);

    residual_m = measured_altitude_m - predicted_altitude_m;

    /* ---------------------------------------------------------------------- */
    /* gross jump 방어                                                          */
    /* - GPS source 전환                                                        */
    /* - fix glitch                                                             */
    /* - QNH 급변                                                               */
    /* ---------------------------------------------------------------------- */
    residual_m = vario_state_clampf(residual_m,
                                    -VARIO_STATE_ALTITUDE_JUMP_LIMIT_M,
                                    +VARIO_STATE_ALTITUDE_JUMP_LIMIT_M);

    alt_alpha = vario_state_lpf_alpha(dt_s, alt_tau_s);
    vel_alpha = vario_state_lpf_alpha(dt_s, vel_tau_s);

    /* ---------------------------------------------------------------------- */
    /* position correction                                                      */
    /* ---------------------------------------------------------------------- */
    s_vario_state.runtime.filtered_altitude_m = predicted_altitude_m + (alt_alpha * residual_m);

    /* ---------------------------------------------------------------------- */
    /* observer 내부 속도 상태 갱신                                             */
    /* - altitude prediction 용 내부 상태는 계속 유지한다.                     */
    /* - 하지만 current vario 표시값 자체는 아래의 UI robust filter 가 맡는다. */
    /* ---------------------------------------------------------------------- */
    s_vario_state.runtime.observer_velocity_mps += (alt_alpha * 0.75f) * (residual_m / dt_s);
    s_vario_state.runtime.observer_velocity_mps += vel_alpha *
                                                   (measured_vario_mps - s_vario_state.runtime.observer_velocity_mps);

    s_vario_state.runtime.observer_velocity_mps =
        vario_state_clampf(s_vario_state.runtime.observer_velocity_mps,
                           -VARIO_STATE_MAX_VARIO_MPS,
                           +VARIO_STATE_MAX_VARIO_MPS);

    /* ---------------------------------------------------------------------- */
    /* 6) UI current vario robust filter                                        */
    /* - 입력  : APP_ALTITUDE slow_vario                                        */
    /* - 출력  : Screen 1 current vario 로 publish 될 값                        */
    /* - 특징  : outlier reject + adaptive EMA + short median + zero latch      */
    /* ---------------------------------------------------------------------- */
    s_vario_state.runtime.filtered_vario_mps =
        Vario_UiVarioFilter_Update(&s_vario_state.ui_vario_filter,
                                   ui_vario_input_mps,
                                   filter_timestamp_ms,
                                   settings->vario_damping_level,
                                   settings->digital_vario_average_seconds);

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

    if ((dist_m >= (float)settings->trail_spacing_m) || (age_ms >= 5000u))
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

static void vario_state_publish_5hz(uint32_t now_ms)
{
    float quant_alt_m;
    float quant_vario_mps;
    float quant_gs_kmh;
    float alt2_m;

    if (s_vario_state.runtime.derived_valid == false)
    {
        return;
    }

    if ((s_vario_state.runtime.last_publish_ms != 0u) &&
        ((now_ms - s_vario_state.runtime.last_publish_ms) < VARIO_STATE_PUBLISH_PERIOD_MS))
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  5Hz publish resolution                                                  */
    /*  - altitude : 1m                                                        */
    /*  - vario    : 0.1m/s                                                    */
    /*  - GS       : 0.1km/h 내부 quantize                                      */
    /* ---------------------------------------------------------------------- */
    quant_alt_m = roundf(s_vario_state.runtime.filtered_altitude_m);
    quant_vario_mps = roundf(s_vario_state.runtime.filtered_vario_mps * 10.0f) * 0.1f;
    quant_gs_kmh = roundf(s_vario_state.runtime.filtered_ground_speed_kmh * 10.0f) * 0.1f;

    alt2_m = quant_alt_m - (((float)Vario_Settings_Get()->alt2_reference_cm) * 0.01f);
    alt2_m = roundf(alt2_m);

    s_vario_state.runtime.baro_altitude_m = quant_alt_m;
    s_vario_state.runtime.alt1_absolute_m = quant_alt_m;
    s_vario_state.runtime.baro_vario_mps = quant_vario_mps;
    s_vario_state.runtime.ground_speed_kmh = quant_gs_kmh;
    s_vario_state.runtime.alt2_relative_m = alt2_m;
    s_vario_state.runtime.last_publish_ms = now_ms;

    s_vario_state.redraw_request = 1u;
}

void Vario_State_Init(void)
{
    memset(&s_vario_state, 0, sizeof(s_vario_state));

    Vario_UiVarioFilter_Init(&s_vario_state.ui_vario_filter);

    s_vario_state.current_mode       = VARIO_MODE_SCREEN_1;
    s_vario_state.previous_main_mode = VARIO_MODE_SCREEN_1;
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

    /* ---------------------------------------------------------------------- */
    /* last_task_ms 갱신 시점                                                   */
    /* - heading LPF 의 dt 는 "직전 task 와 현재 task 사이 시간" 이어야 한다.    */
    /* - 따라서 task 시작 시점이 아니라 모든 계산이 끝난 뒤 now_ms 를 저장한다. */
    /* ---------------------------------------------------------------------- */
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
    Vario_State_SetMode(VARIO_MODE_QUICKSET);
}

void Vario_State_EnterValueSetting(void)
{
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
                                (uint8_t)VARIO_QUICKSET_ITEM_COUNT,
                                direction);
    s_vario_state.redraw_request = 1u;
}

void Vario_State_MoveValueSettingCursor(int8_t direction)
{
    s_vario_state.valuesetting_cursor =
        vario_state_wrap_cursor(s_vario_state.valuesetting_cursor,
                                (uint8_t)VARIO_VALUE_ITEM_COUNT,
                                direction);
    s_vario_state.redraw_request = 1u;
}

const vario_runtime_t *Vario_State_GetRuntime(void)
{
    return &s_vario_state.runtime;
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

    s_vario_state.runtime.flight_active = false;
    s_vario_state.runtime.flight_takeoff_candidate_ms = 0u;
    s_vario_state.runtime.flight_landing_candidate_ms = 0u;
    s_vario_state.runtime.flight_start_ms = 0u;
    s_vario_state.runtime.flight_time_s = 0u;
    s_vario_state.runtime.trail_head = 0u;
    s_vario_state.runtime.trail_count = 0u;
    s_vario_state.runtime.trail_valid = false;
    s_vario_state.runtime.alt3_accum_gain_m = 0.0f;
    s_vario_state.runtime.max_top_vario_mps = 0.0f;
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
