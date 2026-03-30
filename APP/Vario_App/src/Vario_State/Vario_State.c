#include "Vario_State.h"

#include "Vario_Settings.h"
#include "Vario_GlideComputer.h"
#include "Vario_Navigation.h"
#include "BIKE_DYNAMICS.h"

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
/*  - 20Hz fast path : vario gauge redraw + fast bar                                       */
/*  -  5Hz slow path : 숫자 display / publish                                  */
/*  로 나눠 운용한다.                                                          */
/* -------------------------------------------------------------------------- */
#ifndef VARIO_STATE_FAST_PRESENT_PERIOD_MS
#define VARIO_STATE_FAST_PRESENT_PERIOD_MS 50u
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

#ifndef VARIO_STATE_TRAINER_SPEED_STEP_KMH
#define VARIO_STATE_TRAINER_SPEED_STEP_KMH 5.0f
#endif

#ifndef VARIO_STATE_TRAINER_HEADING_STEP_DEG
#define VARIO_STATE_TRAINER_HEADING_STEP_DEG 15.0f
#endif

#ifndef VARIO_STATE_TRAINER_MIN_SPEED_KMH
#define VARIO_STATE_TRAINER_MIN_SPEED_KMH 25.0f
#endif

#ifndef VARIO_STATE_TRAINER_MAX_SPEED_KMH
#define VARIO_STATE_TRAINER_MAX_SPEED_KMH 140.0f
#endif

#ifndef VARIO_STATE_TRAINER_DEFAULT_LAT_E7
#define VARIO_STATE_TRAINER_DEFAULT_LAT_E7 375665000
#endif

#ifndef VARIO_STATE_TRAINER_DEFAULT_LON_E7
#define VARIO_STATE_TRAINER_DEFAULT_LON_E7 1269780000
#endif

#ifndef VARIO_STATE_TRAINER_TRUE_WIND_FROM_DEG
#define VARIO_STATE_TRAINER_TRUE_WIND_FROM_DEG 240.0f
#endif

#ifndef VARIO_STATE_TRAINER_TRUE_WIND_SPEED_KMH
#define VARIO_STATE_TRAINER_TRUE_WIND_SPEED_KMH 18.0f
#endif

#ifndef VARIO_STATE_TRAIL_PERIOD_MS
#define VARIO_STATE_TRAIL_PERIOD_MS 1000u
#endif

#ifndef VARIO_STATE_BIKE_PRESENT_PERIOD_MS
#define VARIO_STATE_BIKE_PRESENT_PERIOD_MS 50u
#endif

static float vario_state_distance_between_ll_deg_e7(int32_t lat1_e7,
                                                    int32_t lon1_e7,
                                                    int32_t lat2_e7,
                                                    int32_t lon2_e7);
static float vario_state_wrap_360(float deg);
static float vario_state_deg_to_rad(float deg);
static float vario_state_rad_to_deg(float rad);

/* -------------------------------------------------------------------------- */
/*  TRAINER synthetic scenario tables                                          */
/*                                                                            */
/*  실제 센서값을 직접 건드리는 대신, Vario_State 내부에 별도 scenario table 을  */
/*  두고 그 결과를 runtime snapshot staging 구조체에 덮어쓴다.                */
/*                                                                            */
/*  버튼은 TRAINER 내부 synthetic 축(speed / altitude / heading)만 조작한다. */
/*  실제 센서/APP_STATE canonical 저장소는 건드리지 않는다.                    */
/*  trainer altitude는 synthetic pressure altitude로 반영되고,                 */
/*  altitude 버튼은 짧은 vario impulse도 함께 걸어 사용자가 즉시 체감하게 한다. */
/* -------------------------------------------------------------------------- */
typedef struct
{
    uint16_t speed_kmh_x10;
    int16_t  vario_bias_cms;
} vario_trainer_speed_row_t;

typedef struct
{
    bool     initialized;
    uint32_t last_update_ms;
    uint32_t start_ms;
    float    pressure_altitude_m;
    float    airspeed_kmh;
    float    heading_deg;
    float    vario_mps;
    float    command_vario_mps;
    uint32_t command_vario_until_ms;
    int32_t  lat_e7;
    int32_t  lon_e7;
} vario_trainer_state_t;

static const int16_t s_vario_trainer_heading_lift_cms[12] = {
    -150, -90, -30, 20, 150, 260, 190, 90, -10, -90, -170, -110
};

static const vario_trainer_speed_row_t s_vario_trainer_speed_rows[] = {
    { 350u,  55 },
    { 450u,  30 },
    { 550u,  12 },
    { 650u,   0 },
    { 750u, -10 },
    { 850u, -25 },
    { 950u, -45 },
    { 1050u, -70 }
};

static int32_t vario_state_trainer_pressure_from_altitude_hpa_x100(float altitude_m,
                                                                    int32_t qnh_hpa_x100);
static float   vario_state_trainer_altitude_from_pressure_m(int32_t pressure_hpa_x100,
                                                            int32_t qnh_hpa_x100);
static void    vario_state_trainer_fill_linear_units(app_altitude_linear_units_t *dst,
                                                     int32_t altitude_cm);
static void    vario_state_trainer_fill_vspeed_units(app_altitude_vspeed_units_t *dst,
                                                     int32_t vario_cms);
static void    vario_state_trainer_init(uint32_t now_ms);
static void    vario_state_trainer_handle_toggle(bool enabled, uint32_t now_ms);
static float   vario_state_trainer_compute_vario_mps(uint32_t now_ms);
static void    vario_state_trainer_step(uint32_t now_ms);
static void    vario_state_trainer_apply_snapshots(uint32_t now_ms);
static void    vario_state_update_bike_present(uint32_t now_ms);

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
    /*  last_fast_present_ms : 20Hz vario gauge redraw cadence                 */
    /*  last_slow_present_ms :  5Hz display current-vario cadence              */
    /* ---------------------------------------------------------------------- */
    uint32_t last_fast_present_ms;
    uint32_t last_slow_present_ms;
    uint32_t last_bike_snapshot_ms;
    uint32_t last_bike_present_ms;

    /* ---------------------------------------------------------------------- */
    /* altitude path tracking                                                  */
    /*                                                                        */
    /* Alt1은 user-facing source 선택이 아니라 legal barometric path로        */
    /* 고정되었지만, trainer 토글 / cold-start / 강제 rebase 시점에는          */
    /* presentation staging을 현재 측정값으로 다시 맞춰 줄 private tracking이  */
    /* 여전히 필요하다.                                                        */
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

    /* ---------------------------------------------------------------------- */
    /*  TRAINER mode private state                                             */
    /* ---------------------------------------------------------------------- */
    bool                   trainer_mode_latched;
    vario_trainer_state_t  trainer;
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

static int32_t vario_state_trainer_pressure_from_altitude_hpa_x100(float altitude_m,
                                                                    int32_t qnh_hpa_x100)
{
    float qnh_hpa;
    float ratio;
    float pressure_hpa;

    qnh_hpa = ((float)qnh_hpa_x100) * 0.01f;
    if (qnh_hpa < 800.0f)
    {
        qnh_hpa = 1013.25f;
    }

    ratio = 1.0f - (altitude_m / VARIO_STATE_PRESSURE_TO_ALT_FACTOR_M);
    if (ratio < 0.05f)
    {
        ratio = 0.05f;
    }

    pressure_hpa = qnh_hpa * powf(ratio, 1.0f / VARIO_STATE_PRESSURE_TO_ALT_EXPONENT);
    if (pressure_hpa < 100.0f)
    {
        pressure_hpa = 100.0f;
    }

    return (int32_t)lroundf(pressure_hpa * 100.0f);
}

static float vario_state_trainer_altitude_from_pressure_m(int32_t pressure_hpa_x100,
                                                          int32_t qnh_hpa_x100)
{
    float pressure_hpa;
    float qnh_hpa;
    float ratio;

    pressure_hpa = ((float)pressure_hpa_x100) * 0.01f;
    qnh_hpa = ((float)qnh_hpa_x100) * 0.01f;

    if (pressure_hpa <= 0.0f)
    {
        pressure_hpa = 1013.25f;
    }
    if (qnh_hpa < 800.0f)
    {
        qnh_hpa = 1013.25f;
    }

    ratio = pressure_hpa / qnh_hpa;
    if (ratio < 0.05f)
    {
        ratio = 0.05f;
    }
    if (ratio > 1.50f)
    {
        ratio = 1.50f;
    }

    return VARIO_STATE_PRESSURE_TO_ALT_FACTOR_M *
           (1.0f - powf(ratio, VARIO_STATE_PRESSURE_TO_ALT_EXPONENT));
}

static void vario_state_trainer_fill_linear_units(app_altitude_linear_units_t *dst,
                                                  int32_t altitude_cm)
{
    float altitude_m;
    float altitude_ft;

    if (dst == NULL)
    {
        return;
    }

    altitude_m = ((float)altitude_cm) * 0.01f;
    altitude_ft = altitude_m * 3.2808399f;
    dst->meters_rounded = (int32_t)lroundf(altitude_m);
    dst->feet_rounded = (int32_t)lroundf(altitude_ft);
}

static void vario_state_trainer_fill_vspeed_units(app_altitude_vspeed_units_t *dst,
                                                  int32_t vario_cms)
{
    float vario_mps;

    if (dst == NULL)
    {
        return;
    }

    vario_mps = ((float)vario_cms) * 0.01f;
    dst->mps_x10_rounded = (int32_t)lroundf(vario_mps * 10.0f);
    dst->fpm_rounded = (int32_t)lroundf(vario_mps * 196.8504f);
}

static void vario_state_trainer_init(uint32_t now_ms)
{
    memset(&s_vario_state.trainer, 0, sizeof(s_vario_state.trainer));
    s_vario_state.trainer.initialized = true;
    s_vario_state.trainer.last_update_ms = now_ms;
    s_vario_state.trainer.start_ms = now_ms;
    s_vario_state.trainer.pressure_altitude_m = 420.0f;
    s_vario_state.trainer.airspeed_kmh = 72.0f;
    s_vario_state.trainer.heading_deg = 35.0f;
    s_vario_state.trainer.vario_mps = 0.0f;
    s_vario_state.trainer.lat_e7 = VARIO_STATE_TRAINER_DEFAULT_LAT_E7;
    s_vario_state.trainer.lon_e7 = VARIO_STATE_TRAINER_DEFAULT_LON_E7;
}

static void vario_state_trainer_handle_toggle(bool enabled, uint32_t now_ms)
{
    if (enabled == s_vario_state.trainer_mode_latched)
    {
        return;
    }

    s_vario_state.trainer_mode_latched = enabled;
    s_vario_state.altitude_source_tracking_valid = false;
    s_vario_state.runtime.heading_valid = false;
    s_vario_state.runtime.last_gps_host_time_ms = 0u;
    s_vario_state.last_fast_present_ms = 0u;
    s_vario_state.last_slow_present_ms = 0u;

    if (enabled != false)
    {
        vario_state_trainer_init(now_ms);
    }
    else
    {
        memset(&s_vario_state.trainer, 0, sizeof(s_vario_state.trainer));
    }

    Vario_State_ResetFlightMetrics();
}

static float vario_state_trainer_compute_vario_mps(uint32_t now_ms)
{
    uint8_t speed_idx;
    uint8_t heading_idx;
    float   speed_kmh;
    float   base_vario_mps;
    float   wave_fast;
    float   wave_slow;
    float   elapsed_s;

    speed_kmh = s_vario_state.trainer.airspeed_kmh;
    speed_idx = 0u;
    for (uint8_t i = 0u; i < (uint8_t)(sizeof(s_vario_trainer_speed_rows) / sizeof(s_vario_trainer_speed_rows[0])); ++i)
    {
        speed_idx = i;
        if ((speed_kmh * 10.0f) < (float)s_vario_trainer_speed_rows[i].speed_kmh_x10)
        {
            break;
        }
    }

    heading_idx = (uint8_t)(((int32_t)lroundf(vario_state_wrap_360(s_vario_state.trainer.heading_deg) / 30.0f)) % 12);
    base_vario_mps = ((float)s_vario_trainer_heading_lift_cms[heading_idx]) * 0.01f;
    base_vario_mps += ((float)s_vario_trainer_speed_rows[speed_idx].vario_bias_cms) * 0.01f;

    /* ------------------------------------------------------------------ */
    /* altitude 버튼 입력은 trainer 전용 synthetic impulse 로 처리한다.    */
    /* 실제 센서 로직을 쓰지 않는 모드이므로, 순간적인 climb/sink command */
    /* 를 일정 시간 더해 사용자가 altitude/vario 변화를 즉시 보게 한다.   */
    /* ------------------------------------------------------------------ */
    if ((s_vario_state.trainer.command_vario_until_ms != 0u) &&
        (now_ms <= s_vario_state.trainer.command_vario_until_ms))
    {
        base_vario_mps += s_vario_state.trainer.command_vario_mps;
    }
    else if ((s_vario_state.trainer.command_vario_until_ms != 0u) &&
             (now_ms > s_vario_state.trainer.command_vario_until_ms))
    {
        s_vario_state.trainer.command_vario_until_ms = 0u;
        s_vario_state.trainer.command_vario_mps = 0.0f;
    }

    elapsed_s = ((float)(now_ms - s_vario_state.trainer.start_ms)) * 0.001f;
    wave_fast = 0.18f * sinf((elapsed_s * 1.7f) + (vario_state_wrap_360(s_vario_state.trainer.heading_deg) * 0.05f));
    wave_slow = 0.10f * sinf(elapsed_s * 0.37f);

    return vario_state_clampf(base_vario_mps + wave_fast + wave_slow, -4.5f, 4.5f);
}

static void vario_state_trainer_step(uint32_t now_ms)
{
    const vario_settings_t *settings;
    uint32_t                last_ms;
    float                   dt_s;
    float                   airspeed_mps;
    float                   air_n_mps;
    float                   air_e_mps;
    float                   wind_to_deg;
    float                   wind_rad;
    float                   wind_n_mps;
    float                   wind_e_mps;
    float                   ground_n_mps;
    float                   ground_e_mps;
    float                   ground_speed_kmh;
    float                   heading_rad;
    float                   lat_deg;
    float                   cos_lat;
    int32_t                 pressure_hpa_x100;
    int32_t                 qnh_hpa_x100;
    int32_t                 alt_std_cm;
    int32_t                 alt_qnh_cm;
    int32_t                 alt_gps_cm;
    int32_t                 alt_display_cm;
    int32_t                 vario_cms;

    if (s_vario_state.trainer.initialized == false)
    {
        vario_state_trainer_init(now_ms);
    }

    settings = Vario_Settings_Get();
    last_ms = s_vario_state.trainer.last_update_ms;
    if (last_ms == 0u)
    {
        last_ms = now_ms;
    }

    dt_s = ((float)(now_ms - last_ms)) * 0.001f;
    dt_s = vario_state_clampf(dt_s, VARIO_STATE_MIN_DT_S, VARIO_STATE_MAX_DT_S);

    s_vario_state.trainer.vario_mps = vario_state_trainer_compute_vario_mps(now_ms);
    s_vario_state.trainer.pressure_altitude_m += (s_vario_state.trainer.vario_mps * dt_s);
    s_vario_state.trainer.pressure_altitude_m = vario_state_clampf(s_vario_state.trainer.pressure_altitude_m, -100.0f, 4500.0f);

    airspeed_mps = s_vario_state.trainer.airspeed_kmh / 3.6f;
    heading_rad = vario_state_deg_to_rad(s_vario_state.trainer.heading_deg);
    air_n_mps = cosf(heading_rad) * airspeed_mps;
    air_e_mps = sinf(heading_rad) * airspeed_mps;

    wind_to_deg = vario_state_wrap_360(VARIO_STATE_TRAINER_TRUE_WIND_FROM_DEG + 180.0f);
    wind_rad = vario_state_deg_to_rad(wind_to_deg);
    wind_n_mps = cosf(wind_rad) * (VARIO_STATE_TRAINER_TRUE_WIND_SPEED_KMH / 3.6f);
    wind_e_mps = sinf(wind_rad) * (VARIO_STATE_TRAINER_TRUE_WIND_SPEED_KMH / 3.6f);

    ground_n_mps = air_n_mps + wind_n_mps;
    ground_e_mps = air_e_mps + wind_e_mps;
    ground_speed_kmh = sqrtf((ground_n_mps * ground_n_mps) + (ground_e_mps * ground_e_mps)) * 3.6f;

    lat_deg = ((float)s_vario_state.trainer.lat_e7) * 1.0e-7f;
    cos_lat = cosf(vario_state_deg_to_rad(lat_deg));
    if (vario_state_absf(cos_lat) < 0.1f)
    {
        cos_lat = (cos_lat < 0.0f) ? -0.1f : 0.1f;
    }

    lat_deg += (ground_n_mps * dt_s) / VARIO_STATE_EARTH_METERS_PER_DEG_LAT;
    s_vario_state.trainer.lat_e7 = (int32_t)lroundf(lat_deg * 1.0e7f);
    s_vario_state.trainer.lon_e7 += (int32_t)lroundf(((ground_e_mps * dt_s) /
                                                      (VARIO_STATE_EARTH_METERS_PER_DEG_LON * cos_lat)) * 1.0e7f);

    qnh_hpa_x100 = Vario_Settings_GetManualQnhHpaX100();
    pressure_hpa_x100 = vario_state_trainer_pressure_from_altitude_hpa_x100(s_vario_state.trainer.pressure_altitude_m,
                                                                             101325);
    alt_std_cm = (int32_t)lroundf(vario_state_trainer_altitude_from_pressure_m(pressure_hpa_x100, 101325) * 100.0f);
    alt_qnh_cm = (int32_t)lroundf(vario_state_trainer_altitude_from_pressure_m(pressure_hpa_x100, qnh_hpa_x100) * 100.0f);
    alt_gps_cm = (int32_t)lroundf(s_vario_state.trainer.pressure_altitude_m * 100.0f);
    alt_display_cm = alt_qnh_cm;
    vario_cms = (int32_t)lroundf(s_vario_state.trainer.vario_mps * 100.0f);

    memset(&s_vario_state.runtime.altitude, 0, sizeof(s_vario_state.runtime.altitude));
    memset(&s_vario_state.runtime.gps, 0, sizeof(s_vario_state.runtime.gps));
    memset(&s_vario_state.runtime.bike, 0, sizeof(s_vario_state.runtime.bike));

    s_vario_state.runtime.altitude.initialized = true;
    s_vario_state.runtime.altitude.baro_valid = true;
    s_vario_state.runtime.altitude.gps_valid = true;
    s_vario_state.runtime.altitude.imu_vector_valid = true;
    s_vario_state.runtime.altitude.last_update_ms = now_ms;
    s_vario_state.runtime.altitude.last_baro_update_ms = now_ms;
    s_vario_state.runtime.altitude.last_gps_update_ms = now_ms;
    s_vario_state.runtime.altitude.pressure_raw_hpa_x100 = pressure_hpa_x100;
    s_vario_state.runtime.altitude.pressure_prefilt_hpa_x100 = pressure_hpa_x100;
    s_vario_state.runtime.altitude.pressure_filt_hpa_x100 = pressure_hpa_x100;
    s_vario_state.runtime.altitude.pressure_residual_hpa_x100 = 0;
    s_vario_state.runtime.altitude.qnh_manual_hpa_x100 = qnh_hpa_x100;
    s_vario_state.runtime.altitude.qnh_equiv_gps_hpa_x100 = 101325;
    s_vario_state.runtime.altitude.alt_pressure_std_cm = alt_std_cm;
    s_vario_state.runtime.altitude.alt_qnh_manual_cm = alt_qnh_cm;
    s_vario_state.runtime.altitude.alt_gps_hmsl_cm = alt_gps_cm;
    s_vario_state.runtime.altitude.alt_fused_noimu_cm = alt_display_cm;
    s_vario_state.runtime.altitude.alt_fused_imu_cm = alt_display_cm;
    s_vario_state.runtime.altitude.alt_display_cm = alt_display_cm;
    s_vario_state.runtime.altitude.vario_fast_noimu_cms = vario_cms;
    s_vario_state.runtime.altitude.vario_slow_noimu_cms = vario_cms;
    s_vario_state.runtime.altitude.vario_fast_imu_cms = vario_cms;
    s_vario_state.runtime.altitude.vario_slow_imu_cms = vario_cms;
    s_vario_state.runtime.altitude.baro_vario_raw_cms = vario_cms;
    s_vario_state.runtime.altitude.baro_vario_filt_cms = vario_cms;

    vario_state_trainer_fill_linear_units(&s_vario_state.runtime.altitude.units.alt_pressure_std, alt_std_cm);
    vario_state_trainer_fill_linear_units(&s_vario_state.runtime.altitude.units.alt_qnh_manual, alt_qnh_cm);
    vario_state_trainer_fill_linear_units(&s_vario_state.runtime.altitude.units.alt_gps_hmsl, alt_gps_cm);
    vario_state_trainer_fill_linear_units(&s_vario_state.runtime.altitude.units.alt_fused_noimu, alt_display_cm);
    vario_state_trainer_fill_linear_units(&s_vario_state.runtime.altitude.units.alt_fused_imu, alt_display_cm);
    vario_state_trainer_fill_linear_units(&s_vario_state.runtime.altitude.units.alt_display, alt_display_cm);
    vario_state_trainer_fill_linear_units(&s_vario_state.runtime.altitude.units.alt_rel_home_noimu, 0);
    vario_state_trainer_fill_linear_units(&s_vario_state.runtime.altitude.units.alt_rel_home_imu, 0);
    vario_state_trainer_fill_linear_units(&s_vario_state.runtime.altitude.units.home_alt_noimu, alt_display_cm);
    vario_state_trainer_fill_linear_units(&s_vario_state.runtime.altitude.units.home_alt_imu, alt_display_cm);
    vario_state_trainer_fill_linear_units(&s_vario_state.runtime.altitude.units.baro_bias_noimu, 0);
    vario_state_trainer_fill_linear_units(&s_vario_state.runtime.altitude.units.baro_bias_imu, 0);
    s_vario_state.runtime.altitude.units.pressure_raw.hpa_x100 = pressure_hpa_x100;
    s_vario_state.runtime.altitude.units.pressure_filt.hpa_x100 = pressure_hpa_x100;
    s_vario_state.runtime.altitude.units.pressure_prefilt.hpa_x100 = pressure_hpa_x100;
    s_vario_state.runtime.altitude.units.pressure_residual.hpa_x100 = 0;
    s_vario_state.runtime.altitude.units.qnh_manual.hpa_x100 = qnh_hpa_x100;
    s_vario_state.runtime.altitude.units.qnh_equiv_gps.hpa_x100 = 101325;
    s_vario_state.runtime.altitude.units.pressure_raw.inhg_x1000 = (int32_t)lroundf((((float)pressure_hpa_x100) * 0.01f) * 29.529983f);
    s_vario_state.runtime.altitude.units.pressure_filt.inhg_x1000 = s_vario_state.runtime.altitude.units.pressure_raw.inhg_x1000;
    s_vario_state.runtime.altitude.units.pressure_prefilt.inhg_x1000 = s_vario_state.runtime.altitude.units.pressure_raw.inhg_x1000;
    s_vario_state.runtime.altitude.units.qnh_manual.inhg_x1000 = (int32_t)lroundf((((float)qnh_hpa_x100) * 0.01f) * 29.529983f);
    s_vario_state.runtime.altitude.units.qnh_equiv_gps.inhg_x1000 = (int32_t)lroundf(1013.25f * 29.529983f);
    vario_state_trainer_fill_vspeed_units(&s_vario_state.runtime.altitude.units.debug_audio_vario, vario_cms);
    vario_state_trainer_fill_vspeed_units(&s_vario_state.runtime.altitude.units.vario_fast_noimu, vario_cms);
    vario_state_trainer_fill_vspeed_units(&s_vario_state.runtime.altitude.units.vario_slow_noimu, vario_cms);
    vario_state_trainer_fill_vspeed_units(&s_vario_state.runtime.altitude.units.vario_fast_imu, vario_cms);
    vario_state_trainer_fill_vspeed_units(&s_vario_state.runtime.altitude.units.vario_slow_imu, vario_cms);
    vario_state_trainer_fill_vspeed_units(&s_vario_state.runtime.altitude.units.baro_vario_raw, vario_cms);
    vario_state_trainer_fill_vspeed_units(&s_vario_state.runtime.altitude.units.baro_vario_filt, vario_cms);

    s_vario_state.runtime.gps.fix.valid = true;
    s_vario_state.runtime.gps.fix.fixOk = true;
    s_vario_state.runtime.gps.fix.fixType = 3u;
    s_vario_state.runtime.gps.fix.head_veh_valid = 1u;
    s_vario_state.runtime.gps.fix.lat = s_vario_state.trainer.lat_e7;
    s_vario_state.runtime.gps.fix.lon = s_vario_state.trainer.lon_e7;
    s_vario_state.runtime.gps.fix.hMSL = alt_gps_cm * 10;
    s_vario_state.runtime.gps.fix.height = alt_gps_cm * 10;
    s_vario_state.runtime.gps.fix.velN = (int32_t)lroundf(ground_n_mps * 1000.0f);
    s_vario_state.runtime.gps.fix.velE = (int32_t)lroundf(ground_e_mps * 1000.0f);
    s_vario_state.runtime.gps.fix.velD = (int32_t)lroundf((-s_vario_state.trainer.vario_mps) * 1000.0f);
    s_vario_state.runtime.gps.fix.gSpeed = (int32_t)lroundf(ground_speed_kmh / 3.6f * 1000.0f);
    s_vario_state.runtime.gps.fix.headMot = (int32_t)lroundf(vario_state_wrap_360(vario_state_rad_to_deg(atan2f(ground_e_mps, ground_n_mps))) * 100000.0f);
    s_vario_state.runtime.gps.fix.headVeh = s_vario_state.runtime.gps.fix.headMot;
    s_vario_state.runtime.gps.fix.hAcc = 2500u;
    s_vario_state.runtime.gps.fix.vAcc = 3000u;
    s_vario_state.runtime.gps.fix.sAcc = 700u;
    s_vario_state.runtime.gps.fix.headAcc = 30000u;
    s_vario_state.runtime.gps.fix.pDOP = 120u;
    s_vario_state.runtime.gps.fix.numSV_nav_pvt = 14u;
    s_vario_state.runtime.gps.fix.numSV_used = 14u;
    s_vario_state.runtime.gps.fix.last_update_ms = now_ms;
    s_vario_state.runtime.gps.fix.last_fix_ms = now_ms;

    s_vario_state.runtime.bike.initialized = true;
    s_vario_state.runtime.bike.zero_valid = true;
    s_vario_state.runtime.bike.imu_valid = true;
    s_vario_state.runtime.bike.heading_valid = true;
    s_vario_state.runtime.bike.confidence_permille = 950u;
    s_vario_state.runtime.bike.last_update_ms = now_ms;
    s_vario_state.runtime.bike.heading_deg_x10 =
        (int16_t)lroundf(vario_state_wrap_360(s_vario_state.trainer.heading_deg) * 10.0f);
    s_vario_state.runtime.bike.banking_angle_deg_x10 =
        (int16_t)lroundf((18.0f * sinf((((float)(now_ms - s_vario_state.trainer.start_ms)) * 0.001f * 0.55f) +
                                      vario_state_deg_to_rad(s_vario_state.trainer.heading_deg * 0.5f))) * 10.0f);
    s_vario_state.runtime.bike.grade_deg_x10 =
        (int16_t)lroundf(vario_state_clampf((s_vario_state.trainer.vario_mps * 5.0f) +
                                            (2.5f * sinf(((float)(now_ms - s_vario_state.trainer.start_ms)) * 0.001f * 0.35f)),
                                            -15.0f,
                                            15.0f) * 10.0f);
    s_vario_state.runtime.bike.banking_angle_display_deg =
        (int8_t)lroundf(((float)s_vario_state.runtime.bike.banking_angle_deg_x10) * 0.1f);
    s_vario_state.runtime.bike.grade_display_deg =
        (int8_t)lroundf(((float)s_vario_state.runtime.bike.grade_deg_x10) * 0.1f);

    (void)settings;
    s_vario_state.trainer.last_update_ms = now_ms;
}

static void vario_state_trainer_apply_snapshots(uint32_t now_ms)
{
    vario_state_trainer_step(now_ms);
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

static float vario_state_rad_to_deg(float rad)
{
    return rad * (180.0f / VARIO_STATE_PI);
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

static void vario_state_update_bike_present(uint32_t now_ms)
{
    uint32_t bike_update_ms;

    bike_update_ms = s_vario_state.runtime.bike.last_update_ms;
    if (bike_update_ms == 0u)
    {
        return;
    }

    if (bike_update_ms == s_vario_state.last_bike_snapshot_ms)
    {
        return;
    }

    s_vario_state.last_bike_snapshot_ms = bike_update_ms;
    if ((s_vario_state.last_bike_present_ms == 0u) ||
        ((now_ms - s_vario_state.last_bike_present_ms) >= VARIO_STATE_BIKE_PRESENT_PERIOD_MS))
    {
        s_vario_state.last_bike_present_ms = now_ms;
        s_vario_state.redraw_request = 1u;
    }
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
    /* vario source는 Alt1 altitude path와 완전히 분리한다.                    */
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
    /* 숫자 current vario 역시 Alt1 absolute path와 연결하지 않는다.           */
    /*                                                                        */
    /* 이 경로는 저수준 센서퓨전 엔진이 만든 "장기적으로 안정된 slow vario"를  */
    /* 그대로 받는다.                                                          */
    /*                                                                        */
    /* 결과적으로                                                              */
    /* - Alt1 absolute altitude는 legal baro path로 고정되고                   */
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
/* Alt1은 이제 user-facing source 선택이 아니라                               */
/* "법적 / 대회 규정용 QNH-only barometric absolute altitude" 로 고정한다.    */
/*                                                                            */
/* 구현 정책                                                                  */
/* - raw manual-QNH altitude(alt_qnh_manual_cm)는 내부/debug용으로만 유지한다. */
/* - 파일럿에게 보여 줄 Alt1/메인 absolute path는                             */
/*   APP_ALTITUDE가 manual-QNH baro만으로 만든 alt_display_cm 을 사용한다.    */
/* - 즉, fused/GPS path는 Alt1에 끼어들지 않는다.                             */
/* -------------------------------------------------------------------------- */
static bool vario_state_select_alt1_altitude_cm(const app_altitude_state_t *alt,
                                                int32_t *out_altitude_cm)
{
    if ((alt == NULL) || (out_altitude_cm == NULL))
    {
        return false;
    }

    if (alt->initialized == false)
    {
        return false;
    }

    if ((alt->baro_valid == false) ||
        (alt->qnh_manual_hpa_x100 <= 0))
    {
        return false;
    }

    *out_altitude_cm = alt->alt_display_cm;
    return true;
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

    if (vario_state_select_alt1_altitude_cm(alt, &selected_altitude_cm) == false)
    {
        return false;
    }

    /* ---------------------------------------------------------------------- */
    /* 상위 app layer에서는 fast/slow를 다시 섞지 않는다.                      */
    /*                                                                        */
    /* - absolute altitude는 Alt1 legal baro path로 고정한다.                  */
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
/*    - Alt1 legal barometric absolute altitude만 staging 한다.               */
/* 2) current vario                                                            */
/*    - 저수준 sensor-fusion slow vario를 숫자 publish backing으로 쓴다.      */
/* 3) fast vario side bar                                                      */
/*    - 저수준 sensor-fusion fast vario를 20Hz gauge redraw로 반영한다.       */
/*                                                                            */
/* 중요한 변경점                                                              */
/* - user-facing altitude source 전환 개념을 없애고 Alt1을 baro fixed로 둔다. */
/* - 대신 trainer 토글 / cold-start / 큰 jump 때만 rebase를 수행한다.         */
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
    s_vario_state.runtime.last_altitude_update_ms = effective_altitude_update_ms;
    s_vario_state.runtime.derived_valid = true;

    s_vario_state.last_fast_present_ms = filter_timestamp_ms;
    s_vario_state.last_slow_present_ms = filter_timestamp_ms;
    s_vario_state.altitude_source_tracking_valid = true;
    s_vario_state.last_altitude_source = altitude_source;
    s_vario_state.redraw_request = 1u;
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
                     (s_vario_state.last_altitude_source != VARIO_ALT_SOURCE_DISPLAY);
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
                                         VARIO_ALT_SOURCE_DISPLAY);
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
    s_vario_state.last_altitude_source = VARIO_ALT_SOURCE_DISPLAY;

    /* ---------------------------------------------------------------------- */
    /*  vario gauge fast redraw cadence                                        */
    /*                                                                        */
    /*  큰 숫자 publish는 5Hz를 유지하되, 좌측 vario gauge는 20Hz cadence로    */
    /*  redraw request를 세워 fast path 반응성을 더 자주 화면에 반영한다.      */
    /* ---------------------------------------------------------------------- */
    if ((s_vario_state.last_fast_present_ms == 0u) ||
        ((uint32_t)(filter_timestamp_ms - s_vario_state.last_fast_present_ms) >=
         VARIO_STATE_FAST_PRESENT_PERIOD_MS))
    {
        s_vario_state.last_fast_present_ms = filter_timestamp_ms;
        s_vario_state.redraw_request = 1u;
    }
}

static void vario_state_update_flight_logic(uint32_t now_ms)

{
    const vario_settings_t *settings;
    const app_gps_state_t  *gps;
    float                   start_speed_kmh;
    float                   soft_start_speed_kmh;
    float                   alt3_delta_m;
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
                        Vario_Navigation_OnTakeoff(&s_vario_state.runtime);
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
                Vario_Navigation_OnTakeoff(&s_vario_state.runtime);
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

        /* ------------------------------------------------------------------ */
        /*  ALT3                                                               */
        /*                                                                    */
        /*  기존 구현의 "누적 양의 상승고도" 대신,                             */
        /*  Flytec / Digifly / Naviter 계열의 사용 감각에 맞춰                */
        /*  takeoff 또는 수동 reset 기준의 resettable relative altitude로 운용한다.*/
        /*                                                                    */
        /*  즉 ALT3는                                                         */
        /*  - 이륙 시 자동 0                                                   */
        /*  - 비행 중 수동 reset 가능                                          */
        /*  - 이후 현재 Alt1 legal baro altitude와의 차이를 signed 값으로 표시 */
        /* ------------------------------------------------------------------ */
        alt3_delta_m = s_vario_state.runtime.filtered_altitude_m - s_vario_state.runtime.last_accum_altitude_m;
        s_vario_state.runtime.alt3_accum_gain_m = alt3_delta_m;

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
                        Vario_Navigation_OnLanding(&s_vario_state.runtime, now_ms);
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
                    Vario_Navigation_OnLanding(&s_vario_state.runtime, now_ms);
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

static void vario_state_push_trail_point(int32_t lat_e7, int32_t lon_e7, uint32_t stamp_ms, int16_t vario_cms)
{
    uint8_t write_index;

    write_index = s_vario_state.runtime.trail_head;
    s_vario_state.runtime.trail_lat_e7[write_index] = lat_e7;
    s_vario_state.runtime.trail_lon_e7[write_index] = lon_e7;
    s_vario_state.runtime.trail_stamp_ms[write_index] = stamp_ms;
    s_vario_state.runtime.trail_vario_cms[write_index] = vario_cms;

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
    const app_gps_state_t *gps;
    uint8_t                newest_index;
    uint32_t               age_ms;
    int16_t                trail_vario_cms;

    gps = &s_vario_state.runtime.gps;

    if ((gps->fix.valid == false) ||
        (gps->fix.fixOk == false) ||
        (gps->fix.fixType == 0u))
    {
        return;
    }

    trail_vario_cms = (int16_t)lroundf(vario_state_clampf(s_vario_state.runtime.fast_vario_bar_mps,
                                                          -9.99f,
                                                          9.99f) * 100.0f);

    if (s_vario_state.runtime.trail_count == 0u)
    {
        vario_state_push_trail_point(gps->fix.lat,
                                     gps->fix.lon,
                                     gps->fix.last_update_ms,
                                     trail_vario_cms);
        return;
    }

    newest_index = (s_vario_state.runtime.trail_head == 0u) ?
                       (VARIO_TRAIL_MAX_POINTS - 1u) :
                       (uint8_t)(s_vario_state.runtime.trail_head - 1u);
    age_ms = gps->fix.last_update_ms - s_vario_state.runtime.trail_stamp_ms[newest_index];

    if (age_ms >= VARIO_STATE_TRAIL_PERIOD_MS)
    {
        vario_state_push_trail_point(gps->fix.lat,
                                     gps->fix.lon,
                                     gps->fix.last_update_ms,
                                     trail_vario_cms);
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
        (vario_state_select_alt1_altitude_cm(alt,
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
    Vario_Navigation_Init();
}

void Vario_State_Task(uint32_t now_ms)
{
    const vario_settings_t *settings;

    settings = Vario_Settings_Get();

    vario_state_capture_snapshots();
    vario_state_trainer_handle_toggle((settings != NULL) && (settings->trainer_enabled != 0u), now_ms);
    if ((settings != NULL) && (settings->trainer_enabled != 0u))
    {
        vario_state_trainer_apply_snapshots(now_ms);
    }

    vario_state_update_sensor_caches();
    vario_state_update_ground_speed();
    vario_state_update_display_filter(now_ms);
    vario_state_update_glide_ratio_fast_path();
    vario_state_update_heading(now_ms);
    vario_state_update_bike_present(now_ms);
    vario_state_update_clock();
    vario_state_update_flight_logic(now_ms);
    vario_state_update_trail();
    vario_state_publish_5hz(now_ms);
    Vario_GlideComputer_Update(&s_vario_state.runtime, Vario_Settings_Get(), now_ms);

    s_vario_state.runtime.last_task_ms = now_ms;
}

void Vario_State_TrainerAdjustSpeed(int8_t direction)
{
    if (direction == 0)
    {
        return;
    }

    if (s_vario_state.trainer.initialized == false)
    {
        vario_state_trainer_init(s_vario_state.runtime.last_task_ms);
    }

    s_vario_state.trainer.airspeed_kmh =
        vario_state_clampf(s_vario_state.trainer.airspeed_kmh +
                           (((float)direction) * VARIO_STATE_TRAINER_SPEED_STEP_KMH),
                           VARIO_STATE_TRAINER_MIN_SPEED_KMH,
                           VARIO_STATE_TRAINER_MAX_SPEED_KMH);
}

void Vario_State_TrainerAdjustAltitude(int8_t direction)
{
    uint32_t now_ms;
    float    altitude_step_m;
    float    command_vario_mps;

    if (direction == 0)
    {
        return;
    }

    if (s_vario_state.trainer.initialized == false)
    {
        vario_state_trainer_init(s_vario_state.runtime.last_task_ms);
    }

    now_ms = s_vario_state.runtime.last_task_ms;
    if (now_ms == 0u)
    {
        now_ms = s_vario_state.trainer.last_update_ms;
    }

    altitude_step_m = 10.0f;
    command_vario_mps = (direction > 0) ? 1.8f : -1.8f;

    s_vario_state.trainer.pressure_altitude_m =
        vario_state_clampf(s_vario_state.trainer.pressure_altitude_m +
                           (((float)direction) * altitude_step_m),
                           -100.0f,
                           4500.0f);
    s_vario_state.trainer.command_vario_mps = command_vario_mps;
    s_vario_state.trainer.command_vario_until_ms = now_ms + 1200u;
    s_vario_state.trainer.vario_mps = command_vario_mps;
    s_vario_state.redraw_request = 1u;
}

void Vario_State_TrainerAdjustHeading(int8_t direction)
{
    if (direction == 0)
    {
        return;
    }

    if (s_vario_state.trainer.initialized == false)
    {
        vario_state_trainer_init(s_vario_state.runtime.last_task_ms);
    }

    s_vario_state.trainer.heading_deg =
        vario_state_wrap_360(s_vario_state.trainer.heading_deg +
                             (((float)direction) * VARIO_STATE_TRAINER_HEADING_STEP_DEG));
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

void Vario_State_SelectNextMainScreen(void)
{
    switch (s_vario_state.previous_main_mode)
    {
        case VARIO_MODE_SCREEN_1:
            Vario_State_SetMode(VARIO_MODE_SCREEN_3);
            break;

        case VARIO_MODE_SCREEN_3:
            Vario_State_SetMode(VARIO_MODE_SCREEN_2);
            break;

        case VARIO_MODE_SCREEN_2:
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
    if (out_altitude_cm == NULL)
    {
        return false;
    }

    return vario_state_select_alt1_altitude_cm(&s_vario_state.runtime.altitude,
                                               out_altitude_cm);
}

void Vario_State_ResetAccumulatedGain(void)
{
    /* ---------------------------------------------------------------------- */
    /*  ALT3 수동 reset                                                        */
    /*                                                                        */
    /*  ALT3는 누적 gain이 아니라 resettable delta altitude 이므로             */
    /*  reset 시 현재 Alt1 기준 barometric altitude를 새 reference로 잡는다.   */
    /* ---------------------------------------------------------------------- */
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
    memset(s_vario_state.runtime.trail_vario_cms, 0, sizeof(s_vario_state.runtime.trail_vario_cms));
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
    s_vario_state.runtime.target_has_elevation = false;
    s_vario_state.runtime.target_kind = 0u;
    memset(s_vario_state.runtime.target_name, 0, sizeof(s_vario_state.runtime.target_name));
    s_vario_state.runtime.required_glide_ratio = 0.0f;
    s_vario_state.runtime.arrival_height_m = 0.0f;
    s_vario_state.runtime.estimated_te_vario_mps = 0.0f;
    s_vario_state.runtime.last_accum_altitude_m = s_vario_state.runtime.filtered_altitude_m;
    s_vario_state.takeoff_anchor_valid = false;
    s_vario_state.landing_anchor_valid = false;

    Vario_GlideComputer_ResetReference();
    Vario_Navigation_ResetFlightRam();
    s_vario_state.redraw_request = 1u;
}

void Vario_State_ResetAttitudeIndicator(void)
{
    /* ---------------------------------------------------------------------- */
    /*  PAGE 3 attitude indicator reset                                       */
    /*                                                                        */
    /*  high-level VARIO 계층은 sensor driver를 직접 건드리지 않는다.         */
    /*  대신 Platform이 공개한 BIKE_DYNAMICS service API를 통해               */
    /*  attitude observer 재정렬과 zero capture 요청만 올린다.                */
    /*                                                                        */
    /*  - HardRezero      : quaternion / gravity observer runtime 재초기화     */
    /*  - ZeroCapture     : 현재 자세를 bank=0 / grade=0 기준으로 새로 캡처    */
    /*                                                                        */
    /*  실제 zero 적용 시점은 BIKE_DYNAMICS 내부의 유효 샘플/안전 조건에      */
    /*  의해 결정된다.                                                         */
    /* ---------------------------------------------------------------------- */
    BIKE_DYNAMICS_RequestHardRezero();
    BIKE_DYNAMICS_RequestZeroCapture();
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
