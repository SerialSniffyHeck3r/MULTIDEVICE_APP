#include "Vario_State.h"

#include "Vario_Settings.h"
#include "Vario_GlideComputer.h"

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

#ifndef VARIO_STATE_TAKEOFF_SOFT_SPEED_RATIO
#define VARIO_STATE_TAKEOFF_SOFT_SPEED_RATIO 0.70f
#endif

#ifndef VARIO_STATE_TAKEOFF_MIN_DISPLACEMENT_M
#define VARIO_STATE_TAKEOFF_MIN_DISPLACEMENT_M 35.0f
#endif

#ifndef VARIO_STATE_TAKEOFF_MIN_ALT_GAIN_M
#define VARIO_STATE_TAKEOFF_MIN_ALT_GAIN_M 12.0f
#endif

#ifndef VARIO_STATE_TAKEOFF_FALLBACK_VARIO_MPS
#define VARIO_STATE_TAKEOFF_FALLBACK_VARIO_MPS 0.90f
#endif

#ifndef VARIO_STATE_LANDING_MAX_DRIFT_M
#define VARIO_STATE_LANDING_MAX_DRIFT_M 35.0f
#endif

#ifndef VARIO_STATE_LANDING_MAX_ALT_DELTA_M
#define VARIO_STATE_LANDING_MAX_ALT_DELTA_M 12.0f
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

static float vario_state_distance_between_ll_deg_e7(int32_t lat1_e7,
                                                    int32_t lon1_e7,
                                                    int32_t lat2_e7,
                                                    int32_t lon2_e7);

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
    /* presentation scheduler                                                  */
    /*                                                                        */
    /*  last_fast_present_ms : 10Hz bar/audio update cadence                   */
    /*  last_slow_present_ms :  5Hz display current-vario cadence              */
    /* ---------------------------------------------------------------------- */
    uint32_t last_fast_present_ms;
    uint32_t last_slow_present_ms;

    /* ---------------------------------------------------------------------- */
    /* altitude source tracking                                                */
    /*                                                                        */
    /* altitude source 전환은 사용자의 quickset/setting 입력으로 즉시 일어날    */
    /* 수 있다. 이때 display filter가 이전 source의 상태를 그대로 끌고 가면    */
    /* QNH 모드 진입 직후 한 번 꺼졌다 튀어오르는 딥/팝 현상이 생길 수 있다.    */
    /*                                                                        */
    /* 그래서 source가 바뀌는 순간 app-layer altitude / vario presentation    */
    /* filter를 현재 측정값으로 재기준화(rebase)하기 위한 private tracking만     */
    /* 여기서 유지한다.                                                        */
    /* ---------------------------------------------------------------------- */
    bool altitude_source_tracking_valid;
    vario_alt_source_t last_altitude_source;

    /* ---------------------------------------------------------------------- */
    /*  private flight-decision anchors                                        */
    /*                                                                        */
    /*  takeoff / landing 판단은 단순 속도 임계치만으로는 거짓 검출이 생기기    */
    /*  쉽다. 그래서 candidate 시작 시점의 GPS/altitude anchor 를 private      */
    /*  상태로 잡아 두고, 실제 이동거리 / 고도변화를 함께 확인한다.            */
    /* ---------------------------------------------------------------------- */
    bool    takeoff_anchor_valid;
    int32_t takeoff_anchor_lat_e7;
    int32_t takeoff_anchor_lon_e7;
    float   takeoff_anchor_altitude_m;

    bool    landing_anchor_valid;
    int32_t landing_anchor_lat_e7;
    int32_t landing_anchor_lon_e7;
    float   landing_anchor_altitude_m;
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

static uint8_t vario_state_clamp_u8(uint8_t value, uint8_t min_v, uint8_t max_v)
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

static float vario_state_deg_to_rad(float deg)
{
    return deg * (VARIO_STATE_PI / 180.0f);
}

static bool vario_state_compute_glide_ratio(float speed_kmh, float vario_mps, float *out_ratio)
{
    float sink_mps;
    float glide_ratio;

    if (out_ratio == NULL)
    {
        return false;
    }

    sink_mps = -vario_mps;
    if ((sink_mps <= 0.15f) || (speed_kmh <= 1.0f))
    {
        *out_ratio = 0.0f;
        return false;
    }

    glide_ratio = (speed_kmh / 3.6f) / sink_mps;
    *out_ratio = vario_state_clampf(glide_ratio, 0.0f, 99.9f);
    return true;
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
    (void)settings;

    if (alt == NULL)
    {
        return 0.0f;
    }

    /* ---------------------------------------------------------------------- */
    /* vario source는 altitude source와 완전히 분리한다.                       */
    /*                                                                        */
    /* 저수준 APP_ALTITUDE가                                                   */
    /* - vario_fast_noimu_cms : 전통적인 pressure/GPS anchor 기반 바리오       */
    /* - vario_fast_imu_cms   : quaternion INS + baro anchor sensor-fusion    */
    /*   출력                                                                  */
    /* 을 이미 만들어 둔다.                                                   */
    /*                                                                        */
    /* 그리고 IMU aid가 꺼져 있거나 신뢰도가 낮으면 low-level에서              */
    /* vario_fast_imu_cms를 자동으로 no-IMU anchor 쪽으로 되돌린다.            */
    /*                                                                        */
    /* 따라서 상위 Vario_State는 altitude source를 보지 않고                   */
    /* "sensor-fusion fast vario 하나"만 읽으면 된다.                         */
    /* ---------------------------------------------------------------------- */
    if (alt->initialized == false)
    {
        return ((float)alt->vario_fast_noimu_cms) * 0.01f;
    }

    return ((float)alt->vario_fast_imu_cms) * 0.01f;
}

static float vario_state_pick_slow_vario_mps(const app_altitude_state_t *alt,
                                             const vario_settings_t *settings)
{
    (void)settings;

    if (alt == NULL)
    {
        return 0.0f;
    }

    /* ---------------------------------------------------------------------- */
    /* 숫자 current vario 역시 altitude source와 연결하지 않는다.              */
    /*                                                                        */
    /* 이 경로는 저수준 센서퓨전 엔진이 만든 "장기적으로 안정된 slow vario"를  */
    /* 그대로 받는다.                                                          */
    /*                                                                        */
    /* 결과적으로                                                              */
    /* - altitude source는 ALT 숫자만 바꾸고                                   */
    /* - vario source는 IMU on/off 및 센서 신뢰도에만 반응한다.                */
    /* ---------------------------------------------------------------------- */
    if (alt->initialized == false)
    {
        return ((float)alt->vario_slow_noimu_cms) * 0.01f;
    }

    return ((float)alt->vario_slow_imu_cms) * 0.01f;
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
    int32_t                     selected_altitude_cm;

    settings = Vario_Settings_Get();
    alt = &s_vario_state.runtime.altitude;

    if ((out_altitude_m == NULL) || (out_vario_mps == NULL) || (settings == NULL))
    {
        return false;
    }

    if (vario_state_select_source_altitude_cm(alt, settings, &selected_altitude_cm) == false)
    {
        return false;
    }

    /* ---------------------------------------------------------------------- */
    /* 상위 app layer에서는 fast/slow를 다시 섞지 않는다.                      */
    /*                                                                        */
    /* - absolute altitude source 선택은 그대로 유지한다.                      */
    /* - 숫자 current vario의 backing source는 저수준 slow path 하나만 쓴다.    */
    /* - 좌측 bar / audio용 fast path는 호출 측에서 별도로 직접 집어온다.       */
    /*                                                                        */
    /* 즉, legacy debug에서 이미 충분히 sane 하게 만들어진 APP_ALTITUDE 결과를 */
    /* 여기서 다시 평균내거나 observer 입력으로 꼬지 않도록 분리한다.          */
    /* ---------------------------------------------------------------------- */
    *out_altitude_m = ((float)selected_altitude_cm) * 0.01f;
    *out_vario_mps = vario_state_pick_slow_vario_mps(alt, settings);
    return true;
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
/*    - selected source의 absolute altitude 만 부드럽게 표시                  */
/* 2) current vario                                                            */
/*    - 저수준 sensor-fusion slow vario -> UI 숫자 필터                       */
/* 3) fast vario side bar                                                      */
/*    - 저수준 sensor-fusion fast vario -> fast bar 전용 robust filter        */
/*                                                                            */
/* 중요한 변경점                                                              */
/* - altitude source 전환 시 필터를 즉시 rebase 한다.                         */
/* - 따라서 QNH 진입/복귀 시 이전 source 상태를 끌고 가며 생기던               */
/*   dip / pop 현상을 상위 presentation 계층에서 차단한다.                    */
/* -------------------------------------------------------------------------- */
static void vario_state_rebase_display_paths(float altitude_m,
                                             float slow_vario_input_mps,
                                             float fast_vario_input_mps,
                                             uint32_t filter_timestamp_ms,
                                             uint32_t altitude_update_ms,
                                             vario_alt_source_t altitude_source)
{
    uint32_t effective_altitude_update_ms;

    effective_altitude_update_ms = (altitude_update_ms != 0u) ?
                                   altitude_update_ms :
                                   filter_timestamp_ms;

    /* ---------------------------------------------------------------------- */
    /* 두 번째 app-layer 필터를 제거한 뒤에는                                  */
    /* low-level APP_ALTITUDE 결과를 runtime staging field에 그대로 재기준화   */
    /* 해야 한다.                                                              */
    /*                                                                        */
    /* - filtered_altitude_m : 숫자 publish 직전 staging altitude             */
    /* - filtered_vario_mps   : slow-path current vario staging               */
    /* - fast_vario_bar_mps   : fast-path vario bar / audio direct source     */
    /*                                                                        */
    /* 즉, 이 필드명은 legacy 호환 때문에 유지하지만                           */
    /* 더 이상 2차 LPF 출력이라는 의미는 아니다.                              */
    /* ---------------------------------------------------------------------- */
    s_vario_state.runtime.filtered_altitude_m = altitude_m;
    s_vario_state.runtime.filtered_vario_mps = slow_vario_input_mps;
    s_vario_state.runtime.fast_vario_bar_mps = fast_vario_input_mps;
    s_vario_state.runtime.observer_velocity_mps = slow_vario_input_mps;
    s_vario_state.runtime.last_measured_altitude_m = altitude_m;
    s_vario_state.runtime.last_accum_altitude_m = altitude_m;
    s_vario_state.runtime.last_altitude_update_ms = effective_altitude_update_ms;
    s_vario_state.runtime.derived_valid = true;

    s_vario_state.last_fast_present_ms = filter_timestamp_ms;
    s_vario_state.last_slow_present_ms = filter_timestamp_ms;
    s_vario_state.altitude_source_tracking_valid = true;
    s_vario_state.last_altitude_source = altitude_source;
}

static void vario_state_update_display_filter(uint32_t now_ms)
{
    const vario_settings_t     *settings;
    const app_altitude_state_t *alt;
    float                       measured_altitude_m;
    float                       slow_vario_input_mps;
    float                       fast_vario_input_mps;
    uint32_t                    filter_timestamp_ms;
    bool                        source_changed;
    bool                        altitude_jump;

    settings = Vario_Settings_Get();
    alt = &s_vario_state.runtime.altitude;

    if ((settings == NULL) ||
        (vario_state_select_measurement(&measured_altitude_m, &slow_vario_input_mps) == false))
    {
        s_vario_state.runtime.altitude_valid = false;
        return;
    }

    fast_vario_input_mps = vario_state_pick_fast_vario_mps(alt, settings);
    filter_timestamp_ms = (alt->last_update_ms != 0u) ? alt->last_update_ms : now_ms;

    s_vario_state.runtime.altitude_valid = true;
    s_vario_state.runtime.raw_selected_altitude_m = measured_altitude_m;
    s_vario_state.runtime.raw_selected_vario_mps = slow_vario_input_mps;

    source_changed = (s_vario_state.altitude_source_tracking_valid == false) ||
                     (settings->altitude_source != s_vario_state.last_altitude_source);
    altitude_jump = false;

    if ((s_vario_state.runtime.derived_valid != false) &&
        (source_changed == false) &&
        (vario_state_absf(measured_altitude_m - s_vario_state.runtime.filtered_altitude_m) >=
         VARIO_STATE_ALTITUDE_JUMP_LIMIT_M))
    {
        altitude_jump = true;
    }

    if ((s_vario_state.runtime.derived_valid == false) ||
        (s_vario_state.runtime.last_altitude_update_ms == 0u) ||
        (alt->last_update_ms == 0u) ||
        (source_changed != false) ||
        (altitude_jump != false))
    {
        vario_state_rebase_display_paths(measured_altitude_m,
                                         slow_vario_input_mps,
                                         fast_vario_input_mps,
                                         filter_timestamp_ms,
                                         alt->last_update_ms,
                                         settings->altitude_source);
        return;
    }

    if ((s_vario_state.runtime.last_altitude_update_ms != 0u) &&
        (alt->last_update_ms == s_vario_state.runtime.last_altitude_update_ms))
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /* 여기서 다시 거는 second filter를 제거한다.                              */
    /*                                                                        */
    /* APP_ALTITUDE가 계산한 absolute altitude / slow vario / fast vario를    */
    /* 그대로 presentation staging field에 싣고,                              */
    /* 숫자용 안정화는 publish 단계의 quantization+hysteresis만 사용한다.      */
    /*                                                                        */
    /* 결과                                                                    */
    /* - audio는 fast path를 즉시 따라간다.                                   */
    /* - 좌측 fast bar도 같은 fast path를 바로 사용한다.                      */
    /* - 큰 숫자 altitude/current vario만 5Hz publish에서 step hysteresis로   */
    /*   정리된다.                                                             */
    /* ---------------------------------------------------------------------- */
    s_vario_state.runtime.filtered_altitude_m = measured_altitude_m;
    s_vario_state.runtime.filtered_vario_mps = slow_vario_input_mps;
    s_vario_state.runtime.fast_vario_bar_mps = fast_vario_input_mps;
    s_vario_state.runtime.observer_velocity_mps = slow_vario_input_mps;
    s_vario_state.runtime.last_measured_altitude_m = measured_altitude_m;
    s_vario_state.runtime.last_altitude_update_ms = alt->last_update_ms;
    s_vario_state.altitude_source_tracking_valid = true;
    s_vario_state.last_altitude_source = settings->altitude_source;
}

static void vario_state_update_flight_logic(uint32_t now_ms)

{
    const vario_settings_t *settings;
    const app_gps_state_t  *gps;
    float                   start_speed_kmh;
    float                   soft_start_speed_kmh;
    float                   positive_gain_step_m;
    bool                    gps_motion_valid;
    bool                    takeoff_candidate_active;
    bool                    landing_candidate_active;
    float                   anchor_distance_m;
    float                   anchor_altitude_delta_m;

    settings = Vario_Settings_Get();
    gps = &s_vario_state.runtime.gps;
    start_speed_kmh = ((float)settings->flight_start_speed_kmh_x10) * 0.1f;
    soft_start_speed_kmh = start_speed_kmh * VARIO_STATE_TAKEOFF_SOFT_SPEED_RATIO;

    gps_motion_valid = (s_vario_state.runtime.gps_valid != false) &&
                       (gps->fix.valid != false) &&
                       (gps->fix.fixOk != false) &&
                       (gps->fix.fixType != 0u);

    /* ---------------------------------------------------------------------- */
    /*  takeoff detection                                                     */
    /*                                                                        */
    /*  기존 구현은 |vario| >= 0.8 만으로도 takeoff candidate를 세웠다.         */
    /*  이 경로는 돌풍 / 손으로 흔든 상황 / 정지 상태 압력요동에서도 false       */
    /*  positive를 만들 수 있으므로, 이제는 GPS motion evidence를 기본 조건으로 */
    /*  삼고 displacement/altitude gain 확인까지 붙인다.                      */
    /* ---------------------------------------------------------------------- */
    if (s_vario_state.runtime.flight_active == false)
    {
        takeoff_candidate_active = false;

        if (gps_motion_valid != false)
        {
            takeoff_candidate_active =
                (s_vario_state.runtime.filtered_ground_speed_kmh >= soft_start_speed_kmh) ||
                (s_vario_state.runtime.gs_bar_speed_kmh >= start_speed_kmh) ||
                ((vario_state_absf(s_vario_state.runtime.filtered_vario_mps) >= VARIO_STATE_TAKEOFF_FALLBACK_VARIO_MPS) &&
                 (s_vario_state.runtime.filtered_ground_speed_kmh >= (soft_start_speed_kmh * 0.8f)));

            if (takeoff_candidate_active != false)
            {
                if (s_vario_state.runtime.flight_takeoff_candidate_ms == 0u)
                {
                    s_vario_state.runtime.flight_takeoff_candidate_ms = now_ms;
                    s_vario_state.takeoff_anchor_valid = true;
                    s_vario_state.takeoff_anchor_lat_e7 = gps->fix.lat;
                    s_vario_state.takeoff_anchor_lon_e7 = gps->fix.lon;
                    s_vario_state.takeoff_anchor_altitude_m = s_vario_state.runtime.filtered_altitude_m;
                }
                else if (s_vario_state.takeoff_anchor_valid != false)
                {
                    anchor_distance_m = vario_state_distance_between_ll_deg_e7(s_vario_state.takeoff_anchor_lat_e7,
                                                                              s_vario_state.takeoff_anchor_lon_e7,
                                                                              gps->fix.lat,
                                                                              gps->fix.lon);
                    anchor_altitude_delta_m = s_vario_state.runtime.filtered_altitude_m -
                                              s_vario_state.takeoff_anchor_altitude_m;

                    if (((now_ms - s_vario_state.runtime.flight_takeoff_candidate_ms) >=
                         VARIO_STATE_FLIGHT_START_CONFIRM_MS) &&
                        ((s_vario_state.runtime.filtered_ground_speed_kmh >= start_speed_kmh) ||
                         (s_vario_state.runtime.gs_bar_speed_kmh >= (start_speed_kmh + 2.0f)) ||
                         (anchor_distance_m >= VARIO_STATE_TAKEOFF_MIN_DISPLACEMENT_M) ||
                         ((anchor_altitude_delta_m >= VARIO_STATE_TAKEOFF_MIN_ALT_GAIN_M) &&
                          (s_vario_state.runtime.filtered_ground_speed_kmh >= soft_start_speed_kmh))))
                    {
                        Vario_GlideComputer_ResetReference();

                        s_vario_state.runtime.flight_active = true;
                        s_vario_state.runtime.flight_start_ms = now_ms;
                        s_vario_state.runtime.flight_takeoff_candidate_ms = 0u;
                        s_vario_state.runtime.flight_landing_candidate_ms = 0u;
                        s_vario_state.runtime.flight_time_s = 0u;
                        s_vario_state.runtime.alt3_accum_gain_m = 0.0f;
                        s_vario_state.runtime.max_top_vario_mps = 0.0f;
                        s_vario_state.runtime.max_speed_kmh = 0.0f;
                        s_vario_state.runtime.last_accum_altitude_m = s_vario_state.runtime.filtered_altitude_m;
                        s_vario_state.takeoff_anchor_valid = false;
                        s_vario_state.landing_anchor_valid = false;
                        s_vario_state.redraw_request = 1u;
                    }
                }
            }
            else
            {
                s_vario_state.runtime.flight_takeoff_candidate_ms = 0u;
                s_vario_state.takeoff_anchor_valid = false;
            }
        }
        else if ((s_vario_state.runtime.filtered_ground_speed_kmh >= start_speed_kmh) &&
                 (vario_state_absf(s_vario_state.runtime.filtered_vario_mps) >= VARIO_STATE_TAKEOFF_FALLBACK_VARIO_MPS))
        {
            if (s_vario_state.runtime.flight_takeoff_candidate_ms == 0u)
            {
                s_vario_state.runtime.flight_takeoff_candidate_ms = now_ms;
            }
            else if ((now_ms - s_vario_state.runtime.flight_takeoff_candidate_ms) >=
                     VARIO_STATE_FLIGHT_START_CONFIRM_MS)
            {
                Vario_GlideComputer_ResetReference();

                s_vario_state.runtime.flight_active = true;
                s_vario_state.runtime.flight_start_ms = now_ms;
                s_vario_state.runtime.flight_takeoff_candidate_ms = 0u;
                s_vario_state.runtime.flight_landing_candidate_ms = 0u;
                s_vario_state.runtime.flight_time_s = 0u;
                s_vario_state.runtime.alt3_accum_gain_m = 0.0f;
                s_vario_state.runtime.max_top_vario_mps = 0.0f;
                s_vario_state.runtime.max_speed_kmh = 0.0f;
                s_vario_state.runtime.last_accum_altitude_m = s_vario_state.runtime.filtered_altitude_m;
                s_vario_state.takeoff_anchor_valid = false;
                s_vario_state.landing_anchor_valid = false;
                s_vario_state.redraw_request = 1u;
            }
        }
        else
        {
            s_vario_state.runtime.flight_takeoff_candidate_ms = 0u;
            s_vario_state.takeoff_anchor_valid = false;
        }
    }

    /* ---------------------------------------------------------------------- */
    /*  in-flight metrics + landing detection                                 */
    /* ---------------------------------------------------------------------- */
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

        landing_candidate_active =
            (s_vario_state.runtime.filtered_ground_speed_kmh <= VARIO_STATE_FLIGHT_END_SPEED_KMH) &&
            (vario_state_absf(s_vario_state.runtime.filtered_vario_mps) <= VARIO_STATE_FLIGHT_END_VARIO_MPS);

        if (landing_candidate_active != false)
        {
            if (s_vario_state.runtime.flight_landing_candidate_ms == 0u)
            {
                s_vario_state.runtime.flight_landing_candidate_ms = now_ms;

                if (gps_motion_valid != false)
                {
                    s_vario_state.landing_anchor_valid = true;
                    s_vario_state.landing_anchor_lat_e7 = gps->fix.lat;
                    s_vario_state.landing_anchor_lon_e7 = gps->fix.lon;
                    s_vario_state.landing_anchor_altitude_m = s_vario_state.runtime.filtered_altitude_m;
                }
                else
                {
                    s_vario_state.landing_anchor_valid = false;
                }
            }
            else if ((now_ms - s_vario_state.runtime.flight_landing_candidate_ms) >=
                     VARIO_STATE_FLIGHT_LAND_CONFIRM_MS)
            {
                if ((s_vario_state.landing_anchor_valid != false) && (gps_motion_valid != false))
                {
                    anchor_distance_m = vario_state_distance_between_ll_deg_e7(s_vario_state.landing_anchor_lat_e7,
                                                                              s_vario_state.landing_anchor_lon_e7,
                                                                              gps->fix.lat,
                                                                              gps->fix.lon);
                    anchor_altitude_delta_m = vario_state_absf(s_vario_state.runtime.filtered_altitude_m -
                                                               s_vario_state.landing_anchor_altitude_m);

                    if ((anchor_distance_m <= VARIO_STATE_LANDING_MAX_DRIFT_M) &&
                        (anchor_altitude_delta_m <= VARIO_STATE_LANDING_MAX_ALT_DELTA_M))
                    {
                        s_vario_state.runtime.flight_active = false;
                        s_vario_state.runtime.flight_takeoff_candidate_ms = 0u;
                        s_vario_state.runtime.flight_landing_candidate_ms = 0u;
                        s_vario_state.takeoff_anchor_valid = false;
                        s_vario_state.landing_anchor_valid = false;
                        s_vario_state.redraw_request = 1u;
                    }
                }
                else
                {
                    s_vario_state.runtime.flight_active = false;
                    s_vario_state.runtime.flight_takeoff_candidate_ms = 0u;
                    s_vario_state.runtime.flight_landing_candidate_ms = 0u;
                    s_vario_state.takeoff_anchor_valid = false;
                    s_vario_state.landing_anchor_valid = false;
                    s_vario_state.redraw_request = 1u;
                }
            }
        }
        else
        {
            s_vario_state.runtime.flight_landing_candidate_ms = 0u;
            s_vario_state.landing_anchor_valid = false;
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

static void vario_state_update_glide_ratio_fast_path(void)
{
    if (vario_state_compute_glide_ratio(s_vario_state.runtime.gs_bar_speed_kmh,
                                        s_vario_state.runtime.fast_vario_bar_mps,
                                        &s_vario_state.runtime.glide_ratio_instant) != false)
    {
        s_vario_state.runtime.glide_ratio_instant_valid = true;
    }
    else
    {
        s_vario_state.runtime.glide_ratio_instant = 0.0f;
        s_vario_state.runtime.glide_ratio_instant_valid = false;
    }
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
    s_vario_state.runtime.average_speed_kmh = avg_speed_kmh;

    sink_mps = -avg_vario_mps;
    (void)sink_mps;
    if (vario_state_compute_glide_ratio(avg_speed_kmh,
                                        avg_vario_mps,
                                        &s_vario_state.runtime.glide_ratio_average) != false)
    {
        s_vario_state.runtime.glide_ratio_average_valid = true;
    }
    else
    {
        s_vario_state.runtime.glide_ratio_average = 0.0f;
        s_vario_state.runtime.glide_ratio_average_valid = false;
    }
}

static float vario_state_quantize_with_hysteresis(float value,
                                                 float last_output,
                                                 float quantum,
                                                 float trigger)
{
    float output;

    if (quantum <= 0.0f)
    {
        return value;
    }

    output = last_output;
    if (trigger < (quantum * 0.5f))
    {
        trigger = quantum * 0.5f;
    }

    while (value >= (output + trigger))
    {
        output += quantum;
    }

    while (value <= (output - trigger))
    {
        output -= quantum;
    }

    return output;
}

static void vario_state_publish_5hz(uint32_t now_ms)
{
    const vario_settings_t     *settings;
    const app_altitude_state_t *alt;
    float                       quant_alt_m;
    float                       quant_vario_mps;
    float                       quant_gs_kmh;
    float                       speed_delta_kmh;
    float                       display_response_norm;
    float                       average_norm;
    float                       altitude_trigger_m;
    float                       vario_trigger_mps;
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

    display_response_norm = 0.0f;
    average_norm = 0.0f;
    if (settings != NULL)
    {
        display_response_norm = ((float)(vario_state_clamp_u8(settings->vario_damping_level, 1u, 10u) - 1u)) / 9.0f;
        average_norm = ((float)(vario_state_clamp_u8(settings->digital_vario_average_seconds, 1u, 8u) - 1u)) / 7.0f;
    }

    /* ---------------------------------------------------------------------- */
    /* 두 번째 필터를 없앤 대신 숫자 표시의 안정성은 publish 단계에서만 확보한다.*/
    /*                                                                        */
    /* - display response knob : step hysteresis 폭                           */
    /* - average seconds      : current vario 숫자의 hold 폭                   */
    /*                                                                        */
    /* 즉, 바/오디오는 raw-ish fast path를 그대로 쓰고, 숫자만 보기 좋게       */
    /* step latch를 건다.                                                      */
    /* ---------------------------------------------------------------------- */
    altitude_trigger_m = 0.58f + (0.26f * display_response_norm);
    vario_trigger_mps = 0.055f + (0.020f * display_response_norm) + (0.018f * average_norm);

    if (s_vario_state.runtime.last_publish_ms == 0u)
    {
        quant_alt_m = roundf(s_vario_state.runtime.filtered_altitude_m);
        quant_vario_mps = roundf(s_vario_state.runtime.filtered_vario_mps * 10.0f) * 0.1f;
    }
    else
    {
        quant_alt_m = vario_state_quantize_with_hysteresis(s_vario_state.runtime.filtered_altitude_m,
                                                           s_vario_state.runtime.baro_altitude_m,
                                                           1.0f,
                                                           altitude_trigger_m);
        quant_vario_mps = vario_state_quantize_with_hysteresis(s_vario_state.runtime.filtered_vario_mps,
                                                               s_vario_state.runtime.baro_vario_mps,
                                                               0.1f,
                                                               vario_trigger_mps);
    }

    quant_gs_kmh = roundf(s_vario_state.runtime.filtered_ground_speed_kmh * 10.0f) * 0.1f;

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

    if (vario_state_compute_glide_ratio(quant_gs_kmh, quant_vario_mps, &s_vario_state.runtime.glide_ratio_slow) != false)
    {
        s_vario_state.runtime.glide_ratio_slow_valid = true;
    }
    else
    {
        s_vario_state.runtime.glide_ratio_slow = 0.0f;
        s_vario_state.runtime.glide_ratio_slow_valid = false;
    }

    s_vario_state.runtime.glide_ratio = s_vario_state.runtime.glide_ratio_slow;
    s_vario_state.runtime.glide_ratio_valid = s_vario_state.runtime.glide_ratio_slow_valid;

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

    s_vario_state.current_mode       = VARIO_MODE_SCREEN_1;
    s_vario_state.previous_main_mode = VARIO_MODE_SCREEN_1;
    s_vario_state.settings_category  = VARIO_SETTINGS_CATEGORY_SYSTEM;
    s_vario_state.redraw_request     = 1u;

    Vario_GlideComputer_Init();
}

void Vario_State_Task(uint32_t now_ms)
{
    vario_state_capture_snapshots();
    vario_state_update_sensor_caches();
    vario_state_update_ground_speed();
    vario_state_update_display_filter(now_ms);
    vario_state_update_glide_ratio_fast_path();
    vario_state_update_heading(now_ms);
    vario_state_update_clock();
    vario_state_update_flight_logic(now_ms);
    vario_state_update_trail();
    vario_state_publish_5hz(now_ms);
    Vario_GlideComputer_Update(&s_vario_state.runtime, Vario_Settings_Get(), now_ms);

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
    s_vario_state.runtime.average_speed_kmh = 0.0f;
    s_vario_state.runtime.glide_ratio = 0.0f;
    s_vario_state.runtime.glide_ratio_instant = 0.0f;
    s_vario_state.runtime.glide_ratio_slow = 0.0f;
    s_vario_state.runtime.glide_ratio_average = 0.0f;
    s_vario_state.runtime.glide_ratio_valid = false;
    s_vario_state.runtime.glide_ratio_instant_valid = false;
    s_vario_state.runtime.glide_ratio_slow_valid = false;
    s_vario_state.runtime.glide_ratio_average_valid = false;
    s_vario_state.runtime.wind_valid = false;
    s_vario_state.runtime.target_valid = false;
    s_vario_state.runtime.speed_to_fly_valid = false;
    s_vario_state.runtime.final_glide_valid = false;
    s_vario_state.runtime.estimated_te_valid = false;
    s_vario_state.runtime.wind_speed_kmh = 0.0f;
    s_vario_state.runtime.wind_from_deg = 0.0f;
    s_vario_state.runtime.estimated_airspeed_kmh = 0.0f;
    s_vario_state.runtime.manual_mccready_mps = 0.0f;
    s_vario_state.runtime.speed_to_fly_kmh = 0.0f;
    s_vario_state.runtime.speed_command_delta_kmh = 0.0f;
    s_vario_state.runtime.target_distance_m = 0.0f;
    s_vario_state.runtime.target_bearing_deg = 0.0f;
    s_vario_state.runtime.target_altitude_m = 0.0f;
    s_vario_state.runtime.required_glide_ratio = 0.0f;
    s_vario_state.runtime.arrival_height_m = 0.0f;
    s_vario_state.runtime.estimated_te_vario_mps = 0.0f;
    s_vario_state.runtime.last_accum_altitude_m = s_vario_state.runtime.filtered_altitude_m;
    s_vario_state.takeoff_anchor_valid = false;
    s_vario_state.landing_anchor_valid = false;

    Vario_GlideComputer_ResetReference();
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
