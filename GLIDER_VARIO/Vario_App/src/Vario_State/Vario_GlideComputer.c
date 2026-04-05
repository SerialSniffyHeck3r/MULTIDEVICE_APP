#include "Vario_GlideComputer.h"
#include "APP_MEMORY_SECTIONS.h"
#include "Vario_Navigation.h"

#include <math.h>
#include <stddef.h>
#include <string.h>

#ifndef VARIO_GC_PI
#define VARIO_GC_PI 3.14159265358979323846f
#endif

#ifndef VARIO_GC_GRAVITY_MPS2
#define VARIO_GC_GRAVITY_MPS2 9.80665f
#endif

#ifndef VARIO_GC_EARTH_METERS_PER_DEG_LAT
#define VARIO_GC_EARTH_METERS_PER_DEG_LAT 111132.0f
#endif

#ifndef VARIO_GC_EARTH_METERS_PER_DEG_LON
#define VARIO_GC_EARTH_METERS_PER_DEG_LON 111319.5f
#endif

#ifndef VARIO_GC_WIND_RING_SIZE
#define VARIO_GC_WIND_RING_SIZE 32u
#endif

#ifndef VARIO_GC_WIND_MIN_SAMPLES
#define VARIO_GC_WIND_MIN_SAMPLES 10u
#endif

#ifndef VARIO_GC_WIND_MIN_CIRCLE_DEG
#define VARIO_GC_WIND_MIN_CIRCLE_DEG 300.0f
#endif

#ifndef VARIO_GC_WIND_MIN_DURATION_S
#define VARIO_GC_WIND_MIN_DURATION_S 8.0f
#endif

#ifndef VARIO_GC_WIND_MAX_DURATION_S
#define VARIO_GC_WIND_MAX_DURATION_S 75.0f
#endif

#ifndef VARIO_GC_WIND_MIN_SPEED_MPS
#define VARIO_GC_WIND_MIN_SPEED_MPS 4.0f
#endif

#ifndef VARIO_GC_WIND_MAX_SPEED_STD_MPS
#define VARIO_GC_WIND_MAX_SPEED_STD_MPS 5.0f
#endif

#ifndef VARIO_GC_POLAR_SEARCH_STEPS
#define VARIO_GC_POLAR_SEARCH_STEPS 48u
#endif

#ifndef VARIO_GC_STF_TAIL_EXTEND_FRACTION
#define VARIO_GC_STF_TAIL_EXTEND_FRACTION 0.50f
#endif

#ifndef VARIO_GC_STF_TAIL_EXTEND_MIN_MPS
#define VARIO_GC_STF_TAIL_EXTEND_MIN_MPS (10.0f / 3.6f)
#endif

#ifndef VARIO_GC_STF_TAIL_EXTEND_MAX_MPS
#define VARIO_GC_STF_TAIL_EXTEND_MAX_MPS (30.0f / 3.6f)
#endif

#ifndef VARIO_GC_TE_TAU_S
#define VARIO_GC_TE_TAU_S 1.20f
#endif

typedef struct
{
    bool     valid;
    uint32_t stamp_ms;
    float    vel_n_mps;
    float    vel_e_mps;
    float    speed_mps;
    float    track_deg;
} vario_gc_wind_sample_t;

typedef struct
{
    float v1_mps;
    float v2_mps;
    float v3_mps;
    float sink1_mps;
    float sink2_mps;
    float sink3_mps;
} vario_gc_polar_t;

static struct
{
    bool     last_flight_active;

    bool     home_valid;
    int32_t  home_lat_e7;
    int32_t  home_lon_e7;
    float    home_altitude_m;

    bool     wind_valid;
    float    wind_n_mps;
    float    wind_e_mps;

    uint32_t last_gps_sample_ms;
    uint8_t  wind_ring_head;
    uint8_t  wind_ring_count;
    vario_gc_wind_sample_t wind_ring[VARIO_GC_WIND_RING_SIZE];

    uint32_t last_te_update_ms;
    float    last_estimated_airspeed_mps;
    float    filtered_te_vario_mps;
    bool     te_filter_initialized;
} s_vario_gc APP_CCMRAM_BSS;

static float vario_gc_absf(float value)
{
    return (value < 0.0f) ? -value : value;
}

static float vario_gc_clampf(float value, float min_v, float max_v)
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

static float vario_gc_wrap_360(float deg)
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

static float vario_gc_wrap_pm180(float deg)
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

static float vario_gc_deg_to_rad(float deg)
{
    return deg * (VARIO_GC_PI / 180.0f);
}

static float vario_gc_rad_to_deg(float rad)
{
    return rad * (180.0f / VARIO_GC_PI);
}

static float vario_gc_vector_speed_mps(float north_mps, float east_mps)
{
    return sqrtf((north_mps * north_mps) + (east_mps * east_mps));
}

static float vario_gc_vector_bearing_deg(float north_mps, float east_mps)
{
    return vario_gc_wrap_360(vario_gc_rad_to_deg(atan2f(east_mps, north_mps)));
}

static void vario_gc_unit_vector_from_bearing(float bearing_deg, float *out_n, float *out_e)
{
    float rad;

    if ((out_n == NULL) || (out_e == NULL))
    {
        return;
    }

    rad = vario_gc_deg_to_rad(bearing_deg);
    *out_n = cosf(rad);
    *out_e = sinf(rad);
}

static void vario_gc_copy_text(char *dst, size_t dst_size, const char *src)
{
    if ((dst == NULL) || (dst_size == 0u))
    {
        return;
    }

    if (src == NULL)
    {
        dst[0] = '\0';
        return;
    }

    strncpy(dst, src, dst_size - 1u);
    dst[dst_size - 1u] = '\0';
}

static bool vario_gc_runtime_has_valid_gps(const vario_runtime_t *rt)
{
    if (rt == NULL)
    {
        return false;
    }

    if ((rt->gps_valid == false) ||
        (rt->gps.fix.valid == false) ||
        (rt->gps.fix.fixOk == false) ||
        (rt->gps.fix.fixType == 0u))
    {
        return false;
    }

    return true;
}

static void vario_gc_reset_wind_ring(void)
{
    memset(s_vario_gc.wind_ring, 0, sizeof(s_vario_gc.wind_ring));
    s_vario_gc.wind_ring_head = 0u;
    s_vario_gc.wind_ring_count = 0u;
    s_vario_gc.last_gps_sample_ms = 0u;
}

static void vario_gc_load_polar(const vario_settings_t *settings, vario_gc_polar_t *out_polar)
{
    if ((settings == NULL) || (out_polar == NULL))
    {
        return;
    }

    out_polar->v1_mps = ((float)settings->polar_speed1_kmh_x10) * (0.1f / 3.6f);
    out_polar->v2_mps = ((float)settings->polar_speed2_kmh_x10) * (0.1f / 3.6f);
    out_polar->v3_mps = ((float)settings->polar_speed3_kmh_x10) * (0.1f / 3.6f);

    out_polar->sink1_mps = ((float)settings->polar_sink1_cms) * 0.01f;
    out_polar->sink2_mps = ((float)settings->polar_sink2_cms) * 0.01f;
    out_polar->sink3_mps = ((float)settings->polar_sink3_cms) * 0.01f;
}

static float vario_gc_polar_sink_mps(const vario_gc_polar_t *polar, float airspeed_mps)
{
    float denom1;
    float denom2;
    float denom3;
    float l1;
    float l2;
    float l3;
    float sink_mps;
    float slope12;
    float slope23;

    if (polar == NULL)
    {
        return 1.0f;
    }

    if (airspeed_mps <= polar->v1_mps)
    {
        slope12 = (polar->sink2_mps - polar->sink1_mps) / (polar->v2_mps - polar->v1_mps);
        sink_mps = polar->sink1_mps + (slope12 * (airspeed_mps - polar->v1_mps));
        return vario_gc_clampf(sink_mps, 0.10f, 10.0f);
    }

    if (airspeed_mps >= polar->v3_mps)
    {
        slope23 = (polar->sink3_mps - polar->sink2_mps) / (polar->v3_mps - polar->v2_mps);
        sink_mps = polar->sink3_mps + (slope23 * (airspeed_mps - polar->v3_mps));
        return vario_gc_clampf(sink_mps, 0.10f, 12.0f);
    }

    denom1 = (polar->v1_mps - polar->v2_mps) * (polar->v1_mps - polar->v3_mps);
    denom2 = (polar->v2_mps - polar->v1_mps) * (polar->v2_mps - polar->v3_mps);
    denom3 = (polar->v3_mps - polar->v1_mps) * (polar->v3_mps - polar->v2_mps);

    if ((vario_gc_absf(denom1) < 1.0e-6f) ||
        (vario_gc_absf(denom2) < 1.0e-6f) ||
        (vario_gc_absf(denom3) < 1.0e-6f))
    {
        return vario_gc_clampf(polar->sink2_mps, 0.10f, 10.0f);
    }

    l1 = ((airspeed_mps - polar->v2_mps) * (airspeed_mps - polar->v3_mps)) / denom1;
    l2 = ((airspeed_mps - polar->v1_mps) * (airspeed_mps - polar->v3_mps)) / denom2;
    l3 = ((airspeed_mps - polar->v1_mps) * (airspeed_mps - polar->v2_mps)) / denom3;

    sink_mps = (polar->sink1_mps * l1) + (polar->sink2_mps * l2) + (polar->sink3_mps * l3);
    return vario_gc_clampf(sink_mps, 0.10f, 12.0f);
}

static float vario_gc_polar_min_sink_speed_mps(const vario_gc_polar_t *polar)
{
    uint16_t step;
    float    best_speed_mps;
    float    best_sink_mps;
    float    v;
    float    sink_mps;

    if (polar == NULL)
    {
        return 15.0f;
    }

    best_speed_mps = polar->v1_mps;
    best_sink_mps = 999.0f;

    for (step = 0u; step <= VARIO_GC_POLAR_SEARCH_STEPS; ++step)
    {
        v = polar->v1_mps + (((float)step) * (polar->v3_mps - polar->v1_mps) /
                             (float)VARIO_GC_POLAR_SEARCH_STEPS);
        sink_mps = vario_gc_polar_sink_mps(polar, v);
        if (sink_mps < best_sink_mps)
        {
            best_sink_mps = sink_mps;
            best_speed_mps = v;
        }
    }

    return best_speed_mps;
}

static void vario_gc_clear_target_outputs(vario_runtime_t *rt)
{
    if (rt == NULL)
    {
        return;
    }

    rt->target_valid = false;
    rt->target_distance_m = 0.0f;
    rt->target_bearing_deg = 0.0f;
    rt->target_altitude_m = 0.0f;
    rt->target_has_elevation = false;
    rt->target_kind = 0u;
    memset(rt->target_name, 0, sizeof(rt->target_name));
    rt->required_glide_ratio = 0.0f;
    rt->arrival_height_m = 0.0f;
    rt->final_glide_valid = false;
}

static bool vario_gc_distance_bearing_from_ll_e7(int32_t from_lat_e7,
                                                 int32_t from_lon_e7,
                                                 int32_t to_lat_e7,
                                                 int32_t to_lon_e7,
                                                 float *out_distance_m,
                                                 float *out_bearing_deg)
{
    float from_lat_deg;
    float from_lon_deg;
    float to_lat_deg;
    float to_lon_deg;
    float mean_lat_rad;
    float dx_m;
    float dy_m;

    if ((out_distance_m == NULL) || (out_bearing_deg == NULL))
    {
        return false;
    }

    from_lat_deg = ((float)from_lat_e7) * 1.0e-7f;
    from_lon_deg = ((float)from_lon_e7) * 1.0e-7f;
    to_lat_deg = ((float)to_lat_e7) * 1.0e-7f;
    to_lon_deg = ((float)to_lon_e7) * 1.0e-7f;

    mean_lat_rad = vario_gc_deg_to_rad((from_lat_deg + to_lat_deg) * 0.5f);
    dx_m = (to_lon_deg - from_lon_deg) * (VARIO_GC_EARTH_METERS_PER_DEG_LON * cosf(mean_lat_rad));
    dy_m = (to_lat_deg - from_lat_deg) * VARIO_GC_EARTH_METERS_PER_DEG_LAT;

    *out_distance_m = sqrtf((dx_m * dx_m) + (dy_m * dy_m));
    *out_bearing_deg = vario_gc_vector_bearing_deg(dy_m, dx_m);
    return true;
}

static void vario_gc_push_wind_sample(const vario_runtime_t *rt)
{
    vario_gc_wind_sample_t *sample;
    float                   vel_n_mps;
    float                   vel_e_mps;

    if (vario_gc_runtime_has_valid_gps(rt) == false)
    {
        return;
    }

    if ((rt->gps.fix.last_update_ms == 0u) ||
        (rt->gps.fix.last_update_ms == s_vario_gc.last_gps_sample_ms))
    {
        return;
    }

    vel_n_mps = ((float)rt->gps.fix.velN) * 0.001f;
    vel_e_mps = ((float)rt->gps.fix.velE) * 0.001f;

    sample = &s_vario_gc.wind_ring[s_vario_gc.wind_ring_head];
    sample->valid = true;
    sample->stamp_ms = rt->gps.fix.last_update_ms;
    sample->vel_n_mps = vel_n_mps;
    sample->vel_e_mps = vel_e_mps;
    sample->speed_mps = vario_gc_vector_speed_mps(vel_n_mps, vel_e_mps);
    sample->track_deg = vario_gc_vector_bearing_deg(vel_n_mps, vel_e_mps);

    s_vario_gc.wind_ring_head = (uint8_t)((s_vario_gc.wind_ring_head + 1u) % VARIO_GC_WIND_RING_SIZE);
    if (s_vario_gc.wind_ring_count < VARIO_GC_WIND_RING_SIZE)
    {
        ++s_vario_gc.wind_ring_count;
    }

    s_vario_gc.last_gps_sample_ms = rt->gps.fix.last_update_ms;
}

static bool vario_gc_try_estimate_circling_wind(float *out_wind_n_mps,
                                                 float *out_wind_e_mps,
                                                 float *out_quality)
{
    uint8_t newest_index;
    uint8_t used_samples;
    uint8_t step;
    uint8_t idx;
    float   cumulative_turn_deg;
    float   prev_track_deg;
    float   mean_n_mps;
    float   mean_e_mps;
    float   mean_speed_mps;
    float   sum_n_mps;
    float   sum_e_mps;
    float   sum_speed_mps;
    float   variance_speed;
    float   speed_std_mps;
    float   duration_s;
    float   quality_turn;
    float   quality_samples;
    float   quality_consistency;
    float   quality_duration;
    uint32_t oldest_stamp_ms;
    const vario_gc_wind_sample_t *sample;
    const vario_gc_wind_sample_t *newest;

    if ((out_wind_n_mps == NULL) || (out_wind_e_mps == NULL) || (out_quality == NULL))
    {
        return false;
    }

    if (s_vario_gc.wind_ring_count < VARIO_GC_WIND_MIN_SAMPLES)
    {
        return false;
    }

    newest_index = (s_vario_gc.wind_ring_head == 0u) ?
                       (VARIO_GC_WIND_RING_SIZE - 1u) :
                       (uint8_t)(s_vario_gc.wind_ring_head - 1u);
    newest = &s_vario_gc.wind_ring[newest_index];
    if (newest->valid == false)
    {
        return false;
    }

    used_samples = 1u;
    cumulative_turn_deg = 0.0f;
    prev_track_deg = newest->track_deg;
    oldest_stamp_ms = newest->stamp_ms;
    sum_n_mps = newest->vel_n_mps;
    sum_e_mps = newest->vel_e_mps;
    sum_speed_mps = newest->speed_mps;

    for (step = 1u; step < s_vario_gc.wind_ring_count; ++step)
    {
        idx = (uint8_t)((newest_index + VARIO_GC_WIND_RING_SIZE - step) % VARIO_GC_WIND_RING_SIZE);
        sample = &s_vario_gc.wind_ring[idx];
        if (sample->valid == false)
        {
            break;
        }

        cumulative_turn_deg += vario_gc_absf(vario_gc_wrap_pm180(prev_track_deg - sample->track_deg));
        prev_track_deg = sample->track_deg;
        oldest_stamp_ms = sample->stamp_ms;

        sum_n_mps += sample->vel_n_mps;
        sum_e_mps += sample->vel_e_mps;
        sum_speed_mps += sample->speed_mps;
        ++used_samples;

        if ((cumulative_turn_deg >= VARIO_GC_WIND_MIN_CIRCLE_DEG) &&
            (used_samples >= VARIO_GC_WIND_MIN_SAMPLES))
        {
            break;
        }
    }

    if ((cumulative_turn_deg < VARIO_GC_WIND_MIN_CIRCLE_DEG) ||
        (used_samples < VARIO_GC_WIND_MIN_SAMPLES) ||
        (newest->stamp_ms <= oldest_stamp_ms))
    {
        return false;
    }

    duration_s = ((float)(newest->stamp_ms - oldest_stamp_ms)) * 0.001f;
    if ((duration_s < VARIO_GC_WIND_MIN_DURATION_S) || (duration_s > VARIO_GC_WIND_MAX_DURATION_S))
    {
        return false;
    }

    mean_n_mps = sum_n_mps / (float)used_samples;
    mean_e_mps = sum_e_mps / (float)used_samples;
    mean_speed_mps = sum_speed_mps / (float)used_samples;
    if (mean_speed_mps < VARIO_GC_WIND_MIN_SPEED_MPS)
    {
        return false;
    }

    variance_speed = 0.0f;
    for (step = 0u; step < used_samples; ++step)
    {
        float speed_error;

        idx = (uint8_t)((newest_index + VARIO_GC_WIND_RING_SIZE - step) % VARIO_GC_WIND_RING_SIZE);
        sample = &s_vario_gc.wind_ring[idx];
        speed_error = sample->speed_mps - mean_speed_mps;
        variance_speed += (speed_error * speed_error);
    }
    variance_speed /= (float)used_samples;
    speed_std_mps = sqrtf(variance_speed);
    if (speed_std_mps > VARIO_GC_WIND_MAX_SPEED_STD_MPS)
    {
        return false;
    }

    quality_turn = vario_gc_clampf((cumulative_turn_deg - VARIO_GC_WIND_MIN_CIRCLE_DEG) / 120.0f, 0.0f, 1.0f);
    quality_samples = vario_gc_clampf(((float)used_samples - (float)VARIO_GC_WIND_MIN_SAMPLES) /
                                      (float)VARIO_GC_WIND_MIN_SAMPLES,
                                      0.0f,
                                      1.0f);
    quality_consistency = 1.0f - vario_gc_clampf(speed_std_mps / VARIO_GC_WIND_MAX_SPEED_STD_MPS, 0.0f, 1.0f);
    quality_duration = 1.0f - vario_gc_clampf(vario_gc_absf(duration_s - 20.0f) / 20.0f, 0.0f, 1.0f);

    *out_wind_n_mps = mean_n_mps;
    *out_wind_e_mps = mean_e_mps;
    *out_quality = vario_gc_clampf(0.15f +
                                   (0.30f * quality_turn) +
                                   (0.20f * quality_samples) +
                                   (0.20f * quality_consistency) +
                                   (0.15f * quality_duration),
                                   0.0f,
                                   1.0f);
    return true;
}

static void vario_gc_update_home_reference(const vario_runtime_t *rt)
{
    if (rt == NULL)
    {
        return;
    }

    s_vario_gc.last_flight_active = rt->flight_active;
}

static void vario_gc_publish_wind_to_runtime(vario_runtime_t *rt)
{
    if (rt == NULL)
    {
        return;
    }

    if (s_vario_gc.wind_valid != false)
    {
        rt->wind_valid = true;
        rt->wind_speed_kmh = vario_gc_vector_speed_mps(s_vario_gc.wind_n_mps, s_vario_gc.wind_e_mps) * 3.6f;
        rt->wind_from_deg = vario_gc_wrap_360(vario_gc_vector_bearing_deg(s_vario_gc.wind_n_mps,
                                                                          s_vario_gc.wind_e_mps) + 180.0f);
    }
    else
    {
        rt->wind_valid = false;
        rt->wind_speed_kmh = 0.0f;
        rt->wind_from_deg = 0.0f;
    }
}

static void vario_gc_update_wind(vario_runtime_t *rt)
{
    float estimated_wind_n_mps;
    float estimated_wind_e_mps;
    float quality;
    float alpha;

    if (rt == NULL)
    {
        return;
    }

    vario_gc_push_wind_sample(rt);

    if (vario_gc_try_estimate_circling_wind(&estimated_wind_n_mps,
                                            &estimated_wind_e_mps,
                                            &quality) != false)
    {
        alpha = 0.10f + (0.35f * quality);
        alpha = vario_gc_clampf(alpha, 0.10f, 0.45f);

        if (s_vario_gc.wind_valid == false)
        {
            s_vario_gc.wind_n_mps = estimated_wind_n_mps;
            s_vario_gc.wind_e_mps = estimated_wind_e_mps;
        }
        else
        {
            s_vario_gc.wind_n_mps += alpha * (estimated_wind_n_mps - s_vario_gc.wind_n_mps);
            s_vario_gc.wind_e_mps += alpha * (estimated_wind_e_mps - s_vario_gc.wind_e_mps);
        }

        s_vario_gc.wind_valid = true;
    }

    vario_gc_publish_wind_to_runtime(rt);
}

static void vario_gc_update_target_geometry(vario_runtime_t *rt)
{
    float distance_m;
    float bearing_deg;
    vario_nav_active_target_t target;

    if (rt == NULL)
    {
        return;
    }

    memset(&target, 0, sizeof(target));

    if ((vario_gc_runtime_has_valid_gps(rt) == false) ||
        (Vario_Navigation_GetActiveTarget(&target) == false))
    {
        vario_gc_clear_target_outputs(rt);
        return;
    }

    if (vario_gc_distance_bearing_from_ll_e7(rt->gps.fix.lat,
                                             rt->gps.fix.lon,
                                             target.lat_e7,
                                             target.lon_e7,
                                             &distance_m,
                                             &bearing_deg) == false)
    {
        vario_gc_clear_target_outputs(rt);
        return;
    }

    rt->target_valid = true;
    rt->target_distance_m = distance_m;
    rt->target_bearing_deg = bearing_deg;
    rt->target_altitude_m = target.altitude_m;
    rt->target_has_elevation = target.has_elevation;
    rt->target_kind = target.kind;
    vario_gc_copy_text(rt->target_name, sizeof(rt->target_name), target.name);
}

static void vario_gc_update_estimated_airspeed(vario_runtime_t *rt)
{
    float ground_n_mps;
    float ground_e_mps;
    float air_n_mps;
    float air_e_mps;

    if (rt == NULL)
    {
        return;
    }

    if (vario_gc_runtime_has_valid_gps(rt) == false)
    {
        rt->estimated_airspeed_kmh = rt->ground_speed_kmh;
        return;
    }

    ground_n_mps = ((float)rt->gps.fix.velN) * 0.001f;
    ground_e_mps = ((float)rt->gps.fix.velE) * 0.001f;

    if (s_vario_gc.wind_valid != false)
    {
        air_n_mps = ground_n_mps - s_vario_gc.wind_n_mps;
        air_e_mps = ground_e_mps - s_vario_gc.wind_e_mps;
        rt->estimated_airspeed_kmh = vario_gc_vector_speed_mps(air_n_mps, air_e_mps) * 3.6f;
    }
    else
    {
        rt->estimated_airspeed_kmh = rt->gs_bar_speed_kmh;
    }

    rt->estimated_airspeed_kmh = vario_gc_clampf(rt->estimated_airspeed_kmh, 0.0f, 250.0f);
}

static float vario_gc_select_command_track_deg(const vario_runtime_t *rt)
{
    float track_deg;

    if (rt == NULL)
    {
        return 0.0f;
    }

    if (rt->target_valid != false)
    {
        return rt->target_bearing_deg;
    }

    if (rt->heading_valid != false)
    {
        return rt->heading_deg;
    }

    if (vario_gc_runtime_has_valid_gps(rt) != false)
    {
        track_deg = vario_gc_vector_bearing_deg(((float)rt->gps.fix.velN) * 0.001f,
                                                ((float)rt->gps.fix.velE) * 0.001f);
        return track_deg;
    }

    return 0.0f;
}

static void vario_gc_update_speed_to_fly(vario_runtime_t *rt,
                                         const vario_settings_t *settings,
                                         const vario_gc_polar_t *polar)
{
    float   manual_mc_mps;
    float   track_deg;
    float   track_n;
    float   track_e;
    float   along_wind_mps;
    float   search_min_mps;
    float   search_max_mps;
    float   best_speed_mps;
    float   best_metric;
    bool    have_solution;
    uint16_t step;

    if ((rt == NULL) || (settings == NULL) || (polar == NULL))
    {
        return;
    }

    manual_mc_mps = ((float)settings->manual_mccready_cms) * 0.01f;
    manual_mc_mps = vario_gc_clampf(manual_mc_mps, 0.0f, 5.0f);
    rt->manual_mccready_mps = manual_mc_mps;

    track_deg = vario_gc_select_command_track_deg(rt);
    vario_gc_unit_vector_from_bearing(track_deg, &track_n, &track_e);

    along_wind_mps = 0.0f;
    if (s_vario_gc.wind_valid != false)
    {
        along_wind_mps = (s_vario_gc.wind_n_mps * track_n) + (s_vario_gc.wind_e_mps * track_e);
    }

    if (manual_mc_mps < 0.05f)
    {
        best_speed_mps = vario_gc_polar_min_sink_speed_mps(polar);
        rt->speed_to_fly_kmh = best_speed_mps * 3.6f;
        rt->speed_command_delta_kmh = rt->speed_to_fly_kmh - rt->estimated_airspeed_kmh;
        rt->speed_to_fly_valid = true;
        return;
    }

    /* ---------------------------------------------------------------------- */
    /* STF search envelope                                                     */
    /*                                                                        */
    /* ?꾩옱 ?ъ슜?먮뒗 "130.0 km/h ??怨좎젙?섎뒗 ??븳" ?꾩긽??蹂닿퀬?덈떎.            */
    /* ?ㅼ젣 ?먯씤? ?댁쟾 ?④퀎?먯꽌 STF ?먯깋??[v1, v3] 濡쒕쭔 臾띠쑝硫댁꽌,            */
    /* polar ??high-speed sample point(v3)媛 ?섎룄移??딄쾶                      */
    /* "紐낅졊 ?띾룄???덈? ?곹븳" ??븷源뚯? 留≪븘 踰꾨┛ ???덈떎.                      */
    /*                                                                        */
    /* ?섏?留?3-point polar ??v3 ??                                          */
    /* - pilot ???낅젰??????섑뵆 ?ъ씤?몄씠吏                                  */
    /* - 諛섎뱶??湲곗껜???덈? STF ?쒓퀎 ?띾룄?쇰뒗 ?살? ?꾨땲??                     */
    /*                                                                        */
    /* 洹몃옒???대쾲 援ы쁽?                                                      */
    /* - v3 ?댄썑??sink ??湲곗〈怨??숈씪?섍쾶 留덉?留?援ш컙 slope 濡??좏삎 ?몄궫?섎릺  */
    /* - search envelope ? "留덉?留?polar interval ???덈컲" 留뚰겮留?            */
    /*   modest ?섍쾶 ?곗옣?쒕떎.                                                 */
    /*                                                                        */
    /* ?대젃寃??섎㈃                                                             */
    /* - v3 hard-cap ?뚮Ц??130.0 ?쇰줈留?蹂댁씠???꾩긽? ?꾪솕?섍퀬                */
    /* - ?덉쟾泥섎읆 tail ??怨쇰룄?섍쾶 ?볧? 150+ km/h ?멸났 ?곹븳??                 */
    /*   怨꾩냽 peg ?섎뒗 臾몄젣??以꾩뼱?좊떎.                                        */
    /*                                                                        */
    /* ?쒖떆 ?덉씠?대뒗 蹂꾨룄濡?GS bar top ??marker 瑜?clamp ?섎?濡?              */
    /* 怨꾩궛 speed 媛 graph ?곷떒???섏뼱??UI cue ??怨꾩냽 ?좎??쒕떎.              */
    /* ---------------------------------------------------------------------- */
    search_min_mps = polar->v1_mps;
    if (search_min_mps < 5.0f)
    {
        search_min_mps = 5.0f;
    }

    search_max_mps = polar->v3_mps;
    if (polar->v3_mps > polar->v2_mps)
    {
        float tail_extend_mps;

        tail_extend_mps = (polar->v3_mps - polar->v2_mps) * VARIO_GC_STF_TAIL_EXTEND_FRACTION;
        tail_extend_mps = vario_gc_clampf(tail_extend_mps,
                                          VARIO_GC_STF_TAIL_EXTEND_MIN_MPS,
                                          VARIO_GC_STF_TAIL_EXTEND_MAX_MPS);
        search_max_mps += tail_extend_mps;
    }

    if (search_max_mps < search_min_mps)
    {
        search_max_mps = search_min_mps;
    }

    best_metric = 1.0e9f;
    best_speed_mps = vario_gc_polar_min_sink_speed_mps(polar);
    have_solution = false;

    for (step = 0u; step <= VARIO_GC_POLAR_SEARCH_STEPS; ++step)
    {
        float candidate_speed_mps;
        float candidate_sink_mps;
        float candidate_ground_along_mps;
        float candidate_metric;

        candidate_speed_mps = search_min_mps +
                              (((float)step) * (search_max_mps - search_min_mps) /
                               (float)VARIO_GC_POLAR_SEARCH_STEPS);
        candidate_ground_along_mps = candidate_speed_mps + along_wind_mps;
        if (candidate_ground_along_mps < 3.0f)
        {
            continue;
        }

        candidate_sink_mps = vario_gc_polar_sink_mps(polar, candidate_speed_mps);
        candidate_metric = (candidate_sink_mps + manual_mc_mps) / candidate_ground_along_mps;
        if (candidate_metric < best_metric)
        {
            best_metric = candidate_metric;
            best_speed_mps = candidate_speed_mps;
            have_solution = true;
        }
    }

    if (have_solution != false)
    {
        rt->speed_to_fly_kmh = best_speed_mps * 3.6f;
        rt->speed_command_delta_kmh = rt->speed_to_fly_kmh - rt->estimated_airspeed_kmh;
        rt->speed_to_fly_valid = true;
    }
    else
    {
        rt->speed_to_fly_kmh = 0.0f;
        rt->speed_command_delta_kmh = 0.0f;
        rt->speed_to_fly_valid = false;
    }
}

static void vario_gc_update_final_glide(vario_runtime_t *rt,
                                        const vario_settings_t *settings,
                                        const vario_gc_polar_t *polar)
{
    float track_n;
    float track_e;
    float along_wind_mps;
    float cruise_airspeed_mps;
    float cruise_sink_mps;
    float ground_along_mps;
    float height_loss_m;
    float available_height_m;

    if ((rt == NULL) || (settings == NULL) || (polar == NULL))
    {
        return;
    }

    if ((rt->target_valid == false) ||
        (rt->target_has_elevation == false) ||
        (rt->target_distance_m <= 1.0f))
    {
        rt->required_glide_ratio = 0.0f;
        rt->arrival_height_m = 0.0f;
        rt->final_glide_valid = false;
        return;
    }

    vario_gc_unit_vector_from_bearing(rt->target_bearing_deg, &track_n, &track_e);
    along_wind_mps = 0.0f;
    if (s_vario_gc.wind_valid != false)
    {
        along_wind_mps = (s_vario_gc.wind_n_mps * track_n) + (s_vario_gc.wind_e_mps * track_e);
    }

    if (rt->speed_to_fly_valid != false)
    {
        cruise_airspeed_mps = rt->speed_to_fly_kmh / 3.6f;
    }
    else
    {
        cruise_airspeed_mps = rt->estimated_airspeed_kmh / 3.6f;
        if (cruise_airspeed_mps < polar->v1_mps)
        {
            cruise_airspeed_mps = polar->v1_mps;
        }
    }

    cruise_sink_mps = vario_gc_polar_sink_mps(polar, cruise_airspeed_mps);
    ground_along_mps = cruise_airspeed_mps + along_wind_mps;
    if (ground_along_mps < 2.0f)
    {
        rt->required_glide_ratio = 0.0f;
        rt->arrival_height_m = 0.0f;
        rt->final_glide_valid = false;
        return;
    }

    available_height_m = rt->filtered_altitude_m - rt->target_altitude_m -
                         (float)settings->final_glide_safety_margin_m;
    height_loss_m = cruise_sink_mps * (rt->target_distance_m / ground_along_mps);

    if (available_height_m > 1.0f)
    {
        rt->required_glide_ratio = vario_gc_clampf(rt->target_distance_m / available_height_m,
                                                   0.0f,
                                                   999.0f);
    }
    else
    {
        rt->required_glide_ratio = 999.0f;
    }

    rt->arrival_height_m = available_height_m - height_loss_m;
    rt->final_glide_valid = true;
}

static void vario_gc_update_estimated_te(vario_runtime_t *rt, uint32_t now_ms)
{
    float airspeed_mps;
    float dt_s;
    float dv_dt_mps2;
    float raw_te_vario_mps;
    float alpha;

    if (rt == NULL)
    {
        return;
    }

    airspeed_mps = rt->estimated_airspeed_kmh / 3.6f;
    if (airspeed_mps < 1.0f)
    {
        rt->estimated_te_vario_mps = 0.0f;
        rt->estimated_te_valid = false;
        s_vario_gc.te_filter_initialized = false;
        s_vario_gc.last_te_update_ms = now_ms;
        s_vario_gc.last_estimated_airspeed_mps = airspeed_mps;
        return;
    }

    if ((s_vario_gc.te_filter_initialized == false) ||
        (s_vario_gc.last_te_update_ms == 0u) ||
        (now_ms <= s_vario_gc.last_te_update_ms))
    {
        s_vario_gc.filtered_te_vario_mps = rt->baro_vario_mps;
        s_vario_gc.te_filter_initialized = true;
    }
    else
    {
        dt_s = ((float)(now_ms - s_vario_gc.last_te_update_ms)) * 0.001f;
        dt_s = vario_gc_clampf(dt_s, 0.05f, 1.00f);
        dv_dt_mps2 = (airspeed_mps - s_vario_gc.last_estimated_airspeed_mps) / dt_s;

        /* ------------------------------------------------------------------ */
        /*  Estimated TE                                                      */
        /*                                                                    */
        /*  TE variometer??湲곕낯??                                           */
        /*      dh/dt + v/g * dv/dt                                           */
        /*  瑜??곕릺, ?ш린??v??pitot TAS媛 ?꾨땲??                           */
        /*  GPS ground vector - circling wind vector 濡?蹂듭썝??異붿젙 airspeed??*/
        /*  ?곕씪??媛믪? "true TE" 媛 ?꾨땲??pilot cue ??estimated TE??      */
        /* ------------------------------------------------------------------ */
        raw_te_vario_mps = rt->baro_vario_mps + ((airspeed_mps / VARIO_GC_GRAVITY_MPS2) * dv_dt_mps2);
        alpha = dt_s / (VARIO_GC_TE_TAU_S + dt_s);
        s_vario_gc.filtered_te_vario_mps += alpha * (raw_te_vario_mps - s_vario_gc.filtered_te_vario_mps);
    }

    s_vario_gc.last_te_update_ms = now_ms;
    s_vario_gc.last_estimated_airspeed_mps = airspeed_mps;
    rt->estimated_te_vario_mps = s_vario_gc.filtered_te_vario_mps;
    rt->estimated_te_valid = true;
}

void Vario_GlideComputer_Init(void)
{
    memset(&s_vario_gc, 0, sizeof(s_vario_gc));
}

void Vario_GlideComputer_ResetReference(void)
{
    s_vario_gc.last_flight_active = false;
    s_vario_gc.home_valid = false;
    s_vario_gc.home_lat_e7 = 0;
    s_vario_gc.home_lon_e7 = 0;
    s_vario_gc.home_altitude_m = 0.0f;
    s_vario_gc.wind_valid = false;
    s_vario_gc.wind_n_mps = 0.0f;
    s_vario_gc.wind_e_mps = 0.0f;
    s_vario_gc.last_te_update_ms = 0u;
    s_vario_gc.last_estimated_airspeed_mps = 0.0f;
    s_vario_gc.filtered_te_vario_mps = 0.0f;
    s_vario_gc.te_filter_initialized = false;
    vario_gc_reset_wind_ring();
}

void Vario_GlideComputer_Update(vario_runtime_t *rt,
                                const vario_settings_t *settings,
                                uint32_t now_ms)
{
    vario_gc_polar_t polar;

    if ((rt == NULL) || (settings == NULL))
    {
        return;
    }

    memset(&polar, 0, sizeof(polar));
    vario_gc_load_polar(settings, &polar);

    vario_gc_update_home_reference(rt);
    vario_gc_update_wind(rt);
    vario_gc_update_target_geometry(rt);
    vario_gc_update_estimated_airspeed(rt);
    vario_gc_update_speed_to_fly(rt, settings, &polar);
    vario_gc_update_final_glide(rt, settings, &polar);
    vario_gc_update_estimated_te(rt, now_ms);
}
