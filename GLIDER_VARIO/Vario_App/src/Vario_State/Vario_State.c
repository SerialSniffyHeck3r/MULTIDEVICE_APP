#include "Vario_State.h"
#include "APP_MEMORY_SECTIONS.h"

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
/*  low-level APP_ALTITUDE???쇱꽌 ?섑뵆留덈떎 fast/slow vario瑜?怨꾩냽 ?좎??쒕떎.     */
/*  ?ш린?쒕뒗 洹?寃곌낵瑜??ㅼ떆 ???④퀎 ?쒗뭹?뷀빐??                               */
/*  - 20Hz fast path : vario gauge redraw + fast bar                                       */
/*  -  5Hz slow path : ?レ옄 display / publish                                  */
/*  濡??섎닠 ?댁슜?쒕떎.                                                          */
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

#ifndef VARIO_STATE_GPS_FIX_MIN_FIX_TYPE
#define VARIO_STATE_GPS_FIX_MIN_FIX_TYPE 3u
#endif

#ifndef VARIO_STATE_GPS_FIX_MAX_AGE_MS
#define VARIO_STATE_GPS_FIX_MAX_AGE_MS 2500u
#endif

#ifndef VARIO_STATE_GPS_FIX_MAX_PDOP_X100
#define VARIO_STATE_GPS_FIX_MAX_PDOP_X100 400u
#endif

#ifndef VARIO_STATE_GPS_FIX_MAX_ABS_LAT_E7
#define VARIO_STATE_GPS_FIX_MAX_ABS_LAT_E7 900000000
#endif

#ifndef VARIO_STATE_GPS_FIX_MAX_ABS_LON_E7
#define VARIO_STATE_GPS_FIX_MAX_ABS_LON_E7 1800000000
#endif

static float vario_state_distance_between_ll_deg_e7(int32_t lat1_e7,
                                                    int32_t lon1_e7,
                                                    int32_t lat2_e7,
                                                    int32_t lon2_e7);
static float vario_state_wrap_360(float deg);
static float vario_state_deg_to_rad(float deg);
static float vario_state_rad_to_deg(float rad);
static bool  vario_state_has_plausible_gps_ll(int32_t lat_e7, int32_t lon_e7);
static bool  vario_state_has_usable_gps_fix(const app_gps_state_t *gps, uint32_t now_ms);

/* -------------------------------------------------------------------------- */
/*  TRAINER synthetic scenario tables                                          */
/*                                                                            */
/*  ?ㅼ젣 ?쇱꽌媛믪쓣 吏곸젒 嫄대뱶由щ뒗 ??? Vario_State ?대???蹂꾨룄 scenario table ?? */
/*  ?먭퀬 洹?寃곌낵瑜?runtime snapshot staging 援ъ“泥댁뿉 ??뼱?대떎.                */
/*                                                                            */
/*  踰꾪듉? TRAINER ?대? synthetic 異?speed / altitude / heading)留?議곗옉?쒕떎. */
/*  ?ㅼ젣 ?쇱꽌/APP_STATE canonical ??μ냼??嫄대뱶由ъ? ?딅뒗??                    */
/*  trainer altitude??synthetic pressure altitude濡?諛섏쁺?섍퀬,                 */
/*  altitude 踰꾪듉? 吏㏃? vario impulse???④퍡 嫄몄뼱 ?ъ슜?먭? 利됱떆 泥닿컧?섍쾶 ?쒕떎. */
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
    /* - ?섏씠吏 ?뚮뜑?ш? 吏곸젒 ?쎈뒗 怨듦컻 runtime                                 */
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
    /* Alt1? user-facing source ?좏깮???꾨땲??legal barometric path濡?       */
    /* 怨좎젙?섏뿀吏留? trainer ?좉? / cold-start / 媛뺤젣 rebase ?쒖젏?먮뒗          */
    /* presentation staging???꾩옱 痢≪젙媛믪쑝濡??ㅼ떆 留욎떠 以?private tracking?? */
    /* ?ъ쟾???꾩슂?섎떎.                                                        */
    /* ---------------------------------------------------------------------- */
    bool altitude_source_tracking_valid;
    vario_alt_source_t last_altitude_source;

    /* ---------------------------------------------------------------------- */
    /*  private flight-decision anchors                                        */
    /*                                                                        */
    /*  takeoff / landing ?먮떒? ?⑥닚 ?띾룄 ?꾧퀎移섎쭔?쇰줈??嫄곗쭞 寃異쒖씠 ?앷린湲?   */
    /*  ?쎈떎. 洹몃옒??candidate ?쒖옉 ?쒖젏??GPS/altitude anchor 瑜?private      */
    /*  ?곹깭濡??≪븘 ?먭퀬, ?ㅼ젣 ?대룞嫄곕━ / 怨좊룄蹂?붾? ?④퍡 ?뺤씤?쒕떎.            */
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
} s_vario_state APP_CCMRAM_BSS;

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
    s_vario_state.runtime.gs_bar_speed_kmh = 0.0f;
    s_vario_state.runtime.filtered_ground_speed_kmh = 0.0f;
    s_vario_state.runtime.ground_speed_kmh = 0.0f;
    s_vario_state.runtime.last_published_ground_speed_kmh = 0.0f;
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
    /* altitude 踰꾪듉 ?낅젰? trainer ?꾩슜 synthetic impulse 濡?泥섎━?쒕떎.    */
    /* ?ㅼ젣 ?쇱꽌 濡쒖쭅???곗? ?딅뒗 紐⑤뱶?대?濡? ?쒓컙?곸씤 climb/sink command */
    /* 瑜??쇱젙 ?쒓컙 ?뷀빐 ?ъ슜?먭? altitude/vario 蹂?붾? 利됱떆 蹂닿쾶 ?쒕떎.   */
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

    /* ------------------------------------------------------------------ */
    /* trainer 紐⑤뱶???ㅼ젣 鍮꾪뻾 ?ы쁽湲곕씪湲곕낫??                             */
    /* UI / ??쾿 / ?먯꽭 ?붾㈃??諛섎났?곸쑝濡?寃利앺븯湲??꾪븳 synthetic source ?? */
    /*                                                                    */
    /* 湲곗〈 援ы쁽? trainer ??true wind drift 瑜??뷀빐 GPS ?꾩튂瑜?吏꾪뻾?쒖섟?? */
    /* ??寃쎌슦 north-up trail page ?먯꽌??                                 */
    /*   - 湲곗껜 ?붿궡?쒕뒗 trainer heading ??蹂닿퀬                            */
    /*   - 諛곌꼍 breadcrumb ??wind 媛 ?욎씤 ground track ?쇰줈 ?吏곸뿬         */
    /* ???붿냼媛 ?쒕줈 ?ㅻⅨ 湲곗???蹂닿쾶 ?쒕떎.                                */
    /*                                                                    */
    /* ?뱁엳 ?⑥そ 怨꾩뿴 heading ?먯꽌 ?ъ슜?먭? 吏?곹븳                           */
    /* "?ъ씤?곕뒗 ?⑥そ??蹂대뒗??諛곌꼍??湲곕???諛⑺뼢?쇰줈 異⑸텇???대젮媛吏 ?딅뒗" */
    /* ?꾩긽??洹쇰낯 ?먯씤??諛붾줈 ??湲곗? 遺덉씪移섏???                            */
    /*                                                                    */
    /* ?섏젙 諛⑹묠                                                            */
    /* - trainer ??synthetic GPS 吏꾪뻾 諛⑺뼢??trainer heading 怨??쇱튂?쒗궓?? */
    /* - 利?breadcrumb 諛곌꼍 ?대룞??議곗쥌??heading 怨?媛숈? 湲곗????ъ슜?쒕떎.  */
    /* - ?ㅽ뭾 drift ?ы쁽蹂대떎, ?붾㈃ 寃利앹슜 ?쇨??깆쓣 ?곗꽑?쒕떎.                */
    /*                                                                    */
    /* 異뷀썑 ??drift 瑜??ㅼ떆 蹂닿퀬 ?띕떎硫?                                  */
    /* - trainer ?꾩슜 option ??異붽???                                    */
    /*   heading-locked / wind-drifted 瑜??좏깮?섎룄濡??뺤옣?????덈떎.        */
    /* ------------------------------------------------------------------ */
    ground_n_mps = air_n_mps;
    ground_e_mps = air_e_mps;
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

static bool vario_state_has_plausible_gps_ll(int32_t lat_e7, int32_t lon_e7)
{
    /* ------------------------------------------------------------------ */
    /* GPS ?꾩튂媛 "??쾿?⑹쑝濡?留먯씠 ?섎뒗 媛믪씤吏" ?뺤씤?쒕떎.                 */
    /*                                                                    */
    /* ???⑥닔??low-level GPS ?쒕씪?대쾭瑜?諛붽씀吏 ?딄퀬,                    */
    /* Vario_State ?곸쐞 怨꾩링?먯꽌 ?ъ슜??怨듯넻 寃뚯씠????븷??留〓뒗??       */
    /*                                                                    */
    /* ?꾩옱 湲곗?                                                            */
    /* - ?꾨룄??[-90, +90] deg 踰붿쐞?ъ빞 ?쒕떎.                             */
    /* - 寃쎈룄??[-180, +180] deg 踰붿쐞?ъ빞 ?쒕떎.                           */
    /* - (0,0) ? ?ㅼ젣 鍮꾪뻾??醫뚰몴濡?蹂닿린 留ㅼ슦 遺?먯뿰?ㅻ윭?곕?濡?          */
    /*   fix 珥덇린???붿긽 / 誘몄닔??湲곕낯媛믪쓣 留됯린 ?꾪빐 嫄곕??쒕떎.             */
    /*                                                                    */
    /* 異뷀썑 low-level ??DOP / accuracy / satellite count 瑜??몄텧?섎㈃,     */
    /* "GPS usable" ?뺤콉? ??helper ?섎굹?먮쭔 異붽??섎㈃ ?쒕떎.            */
    /* ------------------------------------------------------------------ */
    if ((lat_e7 > VARIO_STATE_GPS_FIX_MAX_ABS_LAT_E7) ||
        (lat_e7 < -VARIO_STATE_GPS_FIX_MAX_ABS_LAT_E7))
    {
        return false;
    }

    if ((lon_e7 > VARIO_STATE_GPS_FIX_MAX_ABS_LON_E7) ||
        (lon_e7 < -VARIO_STATE_GPS_FIX_MAX_ABS_LON_E7))
    {
        return false;
    }

    if ((lat_e7 == 0) && (lon_e7 == 0))
    {
        return false;
    }

    return true;
}

static bool vario_state_has_usable_gps_fix(const app_gps_state_t *gps, uint32_t now_ms)
{
    /* ------------------------------------------------------------------ */
    /* 諛붾━???꾩껜?먯꽌 怨듯넻?쇰줈 ?곕뒗 "usable GPS fix" ?뺤쓽??            */
    /*                                                                    */
    /* ?ъ슜?먯쓽 理쒖떊 ?붽뎄?ы빆                                              */
    /* - GPS connected / disconnected ?덉쇅 泥섎━瑜?媛뺥븯寃?遺꾨━?쒕떎.         */
    /* - 留먯씠 ?섎뒗 ?꾧꼍?꾩? fix quality 媛 ?뺣낫??寃쎌슦?먮쭔                */
    /*   GPS 愿???붾㈃/??쾿 湲곕뒫???쒖꽦?뷀븳??                            */
    /*                                                                    */
    /* ?꾩옱 usable 湲곗?                                                     */
    /* 1) low-level fix.valid = true                                       */
    /* 2) low-level fixOk  = true                                          */
    /* 3) fixType >= 3 (3D fix ?댁긽)                                       */
    /* 4) ??寃쎈룄媛 吏援?踰붿쐞 ?덉씠硫?(0,0) 湲곕낯媛믪씠 ?꾨떂                    */
    /* 5) host time 湲곗??쇰줈 ?덈Т ?ㅻ옒??fix 媛 ?꾨떂                       */
    /* 6) pDOP 媛 梨꾩썙???덈떎硫??쇱젙 ?댄븯??寃?                             */
    /*                                                                    */
    /* pDOP ???뷀엳 x100 ?ㅼ????? 120 = 1.20)濡??ㅼ뼱?ㅻ?濡?               */
    /* 4.00 ?댄븯???뚮쭔 usable 濡?蹂몃떎. ?ㅻ쭔 0 ? ??섏? 誘몄콈? 媛?μ꽦??  */
    /* 怨좊젮??"?뺣낫 ?놁쓬" ?쇰줈 蹂닿퀬 ?ш린?쒕뒗 嫄곕??섏? ?딅뒗??            */
    /* ------------------------------------------------------------------ */
    if (gps == NULL)
    {
        return false;
    }

    if ((gps->fix.valid == false) ||
        (gps->fix.fixOk == false) ||
        (gps->fix.fixType < VARIO_STATE_GPS_FIX_MIN_FIX_TYPE))
    {
        return false;
    }

    if (vario_state_has_plausible_gps_ll(gps->fix.lat, gps->fix.lon) == false)
    {
        return false;
    }

    if (gps->fix.last_update_ms == 0u)
    {
        return false;
    }

    if ((now_ms >= gps->fix.last_update_ms) &&
        ((now_ms - gps->fix.last_update_ms) > VARIO_STATE_GPS_FIX_MAX_AGE_MS))
    {
        return false;
    }

    if ((gps->fix.pDOP != 0u) && (gps->fix.pDOP > VARIO_STATE_GPS_FIX_MAX_PDOP_X100))
    {
        return false;
    }

    return true;
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
/* ???⑥닔??APP_STATE 怨듦컻 API留??ъ슜?쒕떎.                                    */
/* ?쇱꽌 ?쒕씪?대쾭 raw register ????꾩쟾???덉뿰?섏뼱 ?덈떎.                       */
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
static void vario_state_update_sensor_caches(uint32_t now_ms)
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

    /* ------------------------------------------------------------------ */
    /* GPS usable ?먯젙? ??吏?먯뿉???꾩뿭 runtime flag 濡??뺤젙?쒕떎.       */
    /*                                                                    */
    /* renderer / navigation / glide / trail ? ?留덈떎 raw gps field 瑜?  */
    /* ?ㅼ떆 ?댁꽍?섏? ?딄퀬, ??flag 瑜?怨듯넻 contract 濡??ъ슜?댁빞            */
    /* GPS disconnected / weak fix / stale fix ?덉쇅 泥섎━媛 ??諛⑺뼢?쇰줈     */
    /* ?좎??쒕떎.                                                           */
    /* ------------------------------------------------------------------ */
    s_vario_state.runtime.gps_valid = vario_state_has_usable_gps_fix(gps, now_ms);

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
    else
    {
        s_vario_state.runtime.gps_altitude_m = 0.0f;
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
    /* vario source??Alt1 altitude path? ?꾩쟾??遺꾨━?쒕떎.                    */
    /*                                                                        */
    /* ??섏? APP_ALTITUDE媛                                                   */
    /* - vario_fast_noimu_cms : ?꾪넻?곸씤 pressure/GPS anchor 湲곕컲 諛붾━??      */
    /* - vario_fast_imu_cms   : quaternion INS + baro anchor sensor-fusion    */
    /*   異쒕젰                                                                  */
    /* ???대? 留뚮뱾???붾떎.                                                   */
    /*                                                                        */
    /* 洹몃━怨?IMU aid媛 爰쇱졇 ?덇굅???좊ː?꾧? ??쑝硫?low-level?먯꽌              */
    /* vario_fast_imu_cms瑜??먮룞?쇰줈 no-IMU anchor 履쎌쑝濡??섎룎由곕떎.            */
    /*                                                                        */
    /* ?곕씪???곸쐞 Vario_State??altitude source瑜?蹂댁? ?딄퀬                   */
    /* "sensor-fusion fast vario ?섎굹"留??쎌쑝硫??쒕떎.                         */
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
    /* ?レ옄 current vario ??떆 Alt1 absolute path? ?곌껐?섏? ?딅뒗??           */
    /*                                                                        */
    /* ??寃쎈줈????섏? ?쇱꽌?⑥쟾 ?붿쭊??留뚮뱺 "?κ린?곸쑝濡??덉젙??slow vario"瑜? */
    /* 洹몃?濡?諛쏅뒗??                                                          */
    /*                                                                        */
    /* 寃곌낵?곸쑝濡?                                                             */
    /* - Alt1 absolute altitude??legal baro path濡?怨좎젙?섍퀬                   */
    /* - vario source??IMU on/off 諛??쇱꽌 ?좊ː?꾩뿉留?諛섏쓳?쒕떎.                */
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
/* Alt1? ?댁젣 user-facing source ?좏깮???꾨땲??                              */
/* "踰뺤쟻 / ???洹쒖젙??QNH-only barometric absolute altitude" 濡?怨좎젙?쒕떎.    */
/*                                                                            */
/* 援ы쁽 ?뺤콉                                                                  */
/* - raw manual-QNH altitude(alt_qnh_manual_cm)???대?/debug?⑹쑝濡쒕쭔 ?좎??쒕떎. */
/* - ?뚯씪?우뿉寃?蹂댁뿬 以?Alt1/硫붿씤 absolute path??                            */
/*   APP_ALTITUDE媛 manual-QNH baro留뚯쑝濡?留뚮뱺 alt_display_cm ???ъ슜?쒕떎.    */
/* - 利? fused/GPS path??Alt1???쇱뼱?ㅼ? ?딅뒗??                             */
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
    /* ?곸쐞 app layer?먯꽌??fast/slow瑜??ㅼ떆 ?욎? ?딅뒗??                      */
    /*                                                                        */
    /* - absolute altitude??Alt1 legal baro path濡?怨좎젙?쒕떎.                  */
    /* - ?レ옄 current vario??backing source????섏? slow path ?섎굹留??대떎.    */
    /* - 醫뚯륫 bar / audio??fast path???몄텧 痢≪뿉??蹂꾨룄濡?吏곸젒 吏묒뼱?⑤떎.       */
    /*                                                                        */
    /* 利? legacy debug?먯꽌 ?대? 異⑸텇??sane ?섍쾶 留뚮뱾?댁쭊 APP_ALTITUDE 寃곌낵瑜?*/
    /* ?ш린???ㅼ떆 ?됯퇏?닿굅??observer ?낅젰?쇰줈 瑗ъ? ?딅룄濡?遺꾨━?쒕떎.          */
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

    if (s_vario_state.runtime.gps_valid == false)
    {
        /* -------------------------------------------------------------- */
        /* GPS usable fix 媛 ?딄린硫??띾룄 愿??cache 瑜?利됱떆 0?쇰줈 ?섎룎由곕떎. */
        /*                                                              */
        /* ?대젃寃??댁빞 TRAINER 醫낅즺 吏곹썑 ?먮뒗 ?ㅼ젣 GPS loss 吏곹썑?먮룄      */
        /* - ??GS ?レ옄                                                  */
        /* - 5Hz publish speed                                           */
        /* - fast GS bar                                                 */
        /* 媛 吏곸쟾 媛믪쓣 遺숈옟怨??⑥? ?딅뒗??                              */
        /* -------------------------------------------------------------- */
        s_vario_state.runtime.gs_bar_speed_kmh = 0.0f;
        s_vario_state.runtime.filtered_ground_speed_kmh = 0.0f;
        s_vario_state.runtime.ground_speed_kmh = 0.0f;
        s_vario_state.runtime.last_published_ground_speed_kmh = 0.0f;
        s_vario_state.runtime.last_gps_host_time_ms = 0u;
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
        if ((s_vario_state.runtime.gps_valid != false) &&
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
/* ?ㅺ퀎 遺꾨━                                                                   */
/* 1) altitude                                                                 */
/*    - Alt1 legal barometric absolute altitude留?staging ?쒕떎.               */
/* 2) current vario                                                            */
/*    - ??섏? sensor-fusion slow vario瑜??レ옄 publish backing?쇰줈 ?대떎.      */
/* 3) fast vario side bar                                                      */
/*    - ??섏? sensor-fusion fast vario瑜?20Hz gauge redraw濡?諛섏쁺?쒕떎.       */
/*                                                                            */
/* 以묒슂??蹂寃쎌젏                                                              */
/* - user-facing altitude source ?꾪솚 媛쒕뀗???놁븷怨?Alt1??baro fixed濡??붾떎. */
/* - ???trainer ?좉? / cold-start / ??jump ?뚮쭔 rebase瑜??섑뻾?쒕떎.         */
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
    /* ??踰덉㎏ app-layer ?꾪꽣瑜??쒓굅???ㅼ뿉??                                 */
    /* low-level APP_ALTITUDE 寃곌낵瑜?runtime staging field??洹몃?濡??ш린以??  */
    /* ?댁빞 ?쒕떎.                                                              */
    /*                                                                        */
    /* - filtered_altitude_m : ?レ옄 publish 吏곸쟾 staging altitude             */
    /* - filtered_vario_mps   : slow-path current vario staging               */
    /* - fast_vario_bar_mps   : fast-path vario bar / audio direct source     */
    /*                                                                        */
    /* 利? ???꾨뱶紐낆? legacy ?명솚 ?뚮Ц???좎??섏?留?                          */
    /* ???댁긽 2李?LPF 異쒕젰?대씪???섎????꾨땲??                              */
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
    /* ?ш린???ㅼ떆 嫄곕뒗 second filter瑜??쒓굅?쒕떎.                              */
    /*                                                                        */
    /* APP_ALTITUDE媛 怨꾩궛??absolute altitude / slow vario / fast vario瑜?   */
    /* 洹몃?濡?presentation staging field???ｊ퀬,                              */
    /* ?レ옄???덉젙?붾뒗 publish ?④퀎??quantization+hysteresis留??ъ슜?쒕떎.      */
    /*                                                                        */
    /* 寃곌낵                                                                    */
    /* - audio??fast path瑜?利됱떆 ?곕씪媛꾨떎.                                   */
    /* - 醫뚯륫 fast bar??媛숈? fast path瑜?諛붾줈 ?ъ슜?쒕떎.                      */
    /* - ???レ옄 altitude/current vario留?5Hz publish?먯꽌 step hysteresis濡?  */
    /*   ?뺣━?쒕떎.                                                             */
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
    /*  ???レ옄 publish??5Hz瑜??좎??섎릺, 醫뚯륫 vario gauge??20Hz cadence濡?   */
    /*  redraw request瑜??몄썙 fast path 諛섏쓳?깆쓣 ???먯＜ ?붾㈃??諛섏쁺?쒕떎.      */
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

    gps_motion_valid = (s_vario_state.runtime.gps_valid != false);

    /* ---------------------------------------------------------------------- */
    /*  takeoff detection                                                     */
    /*                                                                        */
    /*  湲곗〈 援ы쁽? |vario| >= 0.8 留뚯쑝濡쒕룄 takeoff candidate瑜??몄썱??         */
    /*  ??寃쎈줈???뚰뭾 / ?먯쑝濡??붾뱺 ?곹솴 / ?뺤? ?곹깭 ?뺣젰?붾룞?먯꽌??false       */
    /*  positive瑜?留뚮뱾 ???덉쑝誘濡? ?댁젣??GPS motion evidence瑜?湲곕낯 議곌굔?쇰줈 */
    /*  ?쇨퀬 displacement/altitude gain ?뺤씤源뚯? 遺숈씤??                      */
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
        /*  湲곗〈 援ы쁽??"?꾩쟻 ?묒쓽 ?곸듅怨좊룄" ???                             */
        /*  Flytec / Digifly / Naviter 怨꾩뿴???ъ슜 媛먭컖??留욎떠                */
        /*  takeoff ?먮뒗 ?섎룞 reset 湲곗???resettable relative altitude濡??댁슜?쒕떎.*/
        /*                                                                    */
        /*  利?ALT3??                                                        */
        /*  - ?대쪠 ???먮룞 0                                                   */
        /*  - 鍮꾪뻾 以??섎룞 reset 媛??                                         */
        /*  - ?댄썑 ?꾩옱 Alt1 legal baro altitude???李⑥씠瑜?signed 媛믪쑝濡??쒖떆 */
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

    if (s_vario_state.runtime.gps_valid == false)
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
    /* ??踰덉㎏ ?꾪꽣瑜??놁븻 ????レ옄 ?쒖떆???덉젙?깆? publish ?④퀎?먯꽌留??뺣낫?쒕떎.*/
    /*                                                                        */
    /* - display response knob : step hysteresis ??                          */
    /* - average seconds      : current vario ?レ옄??hold ??                  */
    /*                                                                        */
    /* 利? 諛??ㅻ뵒?ㅻ뒗 raw-ish fast path瑜?洹몃?濡??곌퀬, ?レ옄留?蹂닿린 醫뗪쾶       */
    /* step latch瑜?嫄대떎.                                                      */
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

    /* ------------------------------------------------------------------ */
    /* GPS usable fix 媛 ?놁쓣 ?뚮뒗 publish speed 瑜?0?쇰줈 怨좎젙?쒕떎.        */
    /*                                                                    */
    /* renderer 履쎌뿉?쒕뒗 ???곹깭瑜?蹂닿퀬 ?レ옄瑜?'--' / '-' 濡?諛붽씀怨?      */
    /* ?곗륫 GS instant lane ??dither ?⑦꽩?쇰줈 梨꾩썙 INOP 瑜??쒖떆?쒕떎.     */
    /* ?ш린?쒕뒗 stale ?띾룄 ?レ옄媛 publish 寃쎈줈???⑥? ?딄쾶 0?쇰줈 李⑤떒?쒕떎. */
    /* ------------------------------------------------------------------ */
    if (s_vario_state.runtime.gps_valid != false)
    {
        quant_gs_kmh = roundf(s_vario_state.runtime.filtered_ground_speed_kmh * 10.0f) * 0.1f;
    }
    else
    {
        quant_gs_kmh = 0.0f;
    }

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

    vario_state_update_sensor_caches(now_ms);
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
    Vario_Settings_Task(now_ms);

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
    /*  ALT3 ?섎룞 reset                                                        */
    /*                                                                        */
    /*  ALT3???꾩쟻 gain???꾨땲??resettable delta altitude ?대?濡?            */
    /*  reset ???꾩옱 Alt1 湲곗? barometric altitude瑜???reference濡??〓뒗??   */
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
    s_vario_state.runtime.ground_speed_kmh = 0.0f;
    s_vario_state.runtime.gs_bar_speed_kmh = 0.0f;
    s_vario_state.runtime.filtered_ground_speed_kmh = 0.0f;
    s_vario_state.runtime.last_published_ground_speed_kmh = 0.0f;
    s_vario_state.runtime.last_gps_host_time_ms = 0u;
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
    /*  high-level VARIO 怨꾩링? sensor driver瑜?吏곸젒 嫄대뱶由ъ? ?딅뒗??         */
    /*  ???Platform??怨듦컻??BIKE_DYNAMICS service API瑜??듯빐               */
    /*  attitude observer ?ъ젙?ш낵 zero capture ?붿껌留??щ┛??                */
    /*                                                                        */
    /*  - HardRezero      : quaternion / gravity observer runtime ?ъ큹湲고솕     */
    /*  - ZeroCapture     : ?꾩옱 ?먯꽭瑜?bank=0 / grade=0 湲곗??쇰줈 ?덈줈 罹≪쿂    */
    /*                                                                        */
    /*  ?ㅼ젣 zero ?곸슜 ?쒖젏? BIKE_DYNAMICS ?대????좏슚 ?섑뵆/?덉쟾 議곌굔??     */
    /*  ?섑빐 寃곗젙?쒕떎.                                                         */
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
