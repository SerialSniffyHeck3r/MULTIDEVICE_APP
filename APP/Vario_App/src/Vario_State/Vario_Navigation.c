#include "Vario_Navigation.h"

#include "Vario_Display_Common.h"
#include "Vario_Settings.h"
#include "ui_confirm.h"
#include "ui_toast.h"
#include "Vario_State.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef VARIO_NAV_MAX_LANDABLES
#define VARIO_NAV_MAX_LANDABLES 8u
#endif

#ifndef VARIO_NAV_MAX_USER_WAYPOINTS
#define VARIO_NAV_MAX_USER_WAYPOINTS 12u
#endif

#ifndef VARIO_NAV_MAX_MARKS
#define VARIO_NAV_MAX_MARKS 8u
#endif

#ifndef VARIO_NAV_LIST_VISIBLE_ROWS
#define VARIO_NAV_LIST_VISIBLE_ROWS 5u
#endif

#ifndef VARIO_NAV_TITLE_ROW_Y0
#define VARIO_NAV_TITLE_ROW_Y0 20
#endif

#ifndef VARIO_NAV_TITLE_ROW_STEP
#define VARIO_NAV_TITLE_ROW_STEP 16
#endif

#ifndef VARIO_NAV_EARTH_METERS_PER_DEG_LAT
#define VARIO_NAV_EARTH_METERS_PER_DEG_LAT 111132.0f
#endif

#ifndef VARIO_NAV_EARTH_METERS_PER_DEG_LON
#define VARIO_NAV_EARTH_METERS_PER_DEG_LON 111319.5f
#endif

#ifndef VARIO_NAV_PI
#define VARIO_NAV_PI 3.14159265358979323846f
#endif

typedef struct
{
    bool valid;
    vario_nav_page_t open_page;
    uint8_t cursor;
    uint32_t next_id;
    uint8_t next_wp_index;
    uint8_t next_field_index;
    uint8_t next_mark_index;

    vario_nav_target_source_t active_source;
    uint8_t active_landable_index;
    uint8_t active_waypoint_index;
    uint8_t active_mark_index;

    vario_nav_point_t home;
    vario_nav_point_t launch;
    vario_nav_point_t last_landing;
    vario_nav_point_t landables[VARIO_NAV_MAX_LANDABLES];
    vario_nav_point_t waypoints[VARIO_NAV_MAX_USER_WAYPOINTS];
    vario_nav_point_t marks[VARIO_NAV_MAX_MARKS];
} vario_navigation_state_t;

static vario_navigation_state_t s_nav;

static float vario_nav_wrap_360(float deg)
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

static float vario_nav_wrap_pm180(float deg)
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

static float vario_nav_deg_to_rad(float deg)
{
    return deg * (VARIO_NAV_PI / 180.0f);
}

static float vario_nav_vector_bearing_deg(float north_m, float east_m)
{
    return vario_nav_wrap_360((float)(atan2f(east_m, north_m) * (180.0f / VARIO_NAV_PI)));
}

static void vario_nav_copy_name(char *dst, size_t dst_size, const char *src)
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

static void vario_nav_make_name(char *dst, size_t dst_size, const char *prefix, uint8_t index)
{
    if ((dst == NULL) || (dst_size == 0u))
    {
        return;
    }

    snprintf(dst, dst_size, "%s%02u", (prefix != NULL) ? prefix : "PT", (unsigned)index);
}

static bool vario_nav_runtime_has_valid_gps(const vario_runtime_t *rt)
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

static bool vario_nav_capture_runtime_point(const vario_runtime_t *rt,
                                            vario_nav_point_t *out_point,
                                            vario_nav_target_kind_t kind,
                                            bool has_elevation,
                                            const char *name)
{
    if ((rt == NULL) || (out_point == NULL) || (vario_nav_runtime_has_valid_gps(rt) == false))
    {
        return false;
    }

    memset(out_point, 0, sizeof(*out_point));
    out_point->valid = true;
    out_point->has_elevation = has_elevation;
    out_point->kind = kind;
    out_point->id = s_nav.next_id++;
    out_point->lat_e7 = rt->gps.fix.lat;
    out_point->lon_e7 = rt->gps.fix.lon;
    out_point->altitude_m = rt->filtered_altitude_m;
    vario_nav_copy_name(out_point->name, sizeof(out_point->name), name);
    return true;
}

static uint8_t vario_nav_count_valid_points(const vario_nav_point_t *points, uint8_t capacity)
{
    uint8_t i;
    uint8_t count;

    count = 0u;
    for (i = 0u; i < capacity; ++i)
    {
        if (points[i].valid != false)
        {
            ++count;
        }
    }

    return count;
}

static int16_t vario_nav_find_next_valid_index(const vario_nav_point_t *points,
                                               uint8_t capacity,
                                               int16_t start_index,
                                               int8_t direction)
{
    int16_t i;
    int16_t idx;

    if ((points == NULL) || (capacity == 0u))
    {
        return -1;
    }

    if (direction == 0)
    {
        direction = +1;
    }

    idx = start_index;
    for (i = 0; i < (int16_t)capacity; ++i)
    {
        idx += (direction > 0) ? 1 : -1;
        if (idx < 0)
        {
            idx = (int16_t)capacity - 1;
        }
        else if (idx >= (int16_t)capacity)
        {
            idx = 0;
        }

        if (points[idx].valid != false)
        {
            return idx;
        }
    }

    return -1;
}

static int16_t vario_nav_first_valid_index(const vario_nav_point_t *points, uint8_t capacity)
{
    return vario_nav_find_next_valid_index(points, capacity, -1, +1);
}

static bool vario_nav_store_point(vario_nav_point_t *points,
                                  uint8_t capacity,
                                  const vario_nav_point_t *point,
                                  uint8_t *io_active_index)
{
    uint8_t i;
    uint8_t slot;

    if ((points == NULL) || (capacity == 0u) || (point == NULL) || (point->valid == false))
    {
        return false;
    }

    slot = capacity;
    for (i = 0u; i < capacity; ++i)
    {
        if (points[i].valid == false)
        {
            slot = i;
            break;
        }
    }

    if (slot >= capacity)
    {
        slot = 0u;
        for (i = 1u; i < capacity; ++i)
        {
            if (points[i].id < points[slot].id)
            {
                slot = i;
            }
        }
    }

    points[slot] = *point;
    if (io_active_index != NULL)
    {
        *io_active_index = slot;
    }
    return true;
}

static void vario_nav_fill_runtime_target(vario_nav_active_target_t *out_target,
                                          const vario_nav_point_t *point)
{
    if (out_target == NULL)
    {
        return;
    }

    memset(out_target, 0, sizeof(*out_target));
    if ((point == NULL) || (point->valid == false))
    {
        return;
    }

    out_target->valid = true;
    out_target->has_elevation = point->has_elevation;
    out_target->id = point->id;
    out_target->kind = (uint8_t)point->kind;
    out_target->lat_e7 = point->lat_e7;
    out_target->lon_e7 = point->lon_e7;
    out_target->altitude_m = point->altitude_m;
    vario_nav_copy_name(out_target->name, sizeof(out_target->name), point->name);
}

static const vario_nav_point_t *vario_nav_get_point_for_source(vario_nav_target_source_t source)
{
    if (source == VARIO_NAV_SOURCE_HOME)
    {
        return (s_nav.home.valid != false) ? &s_nav.home : NULL;
    }

    if (source == VARIO_NAV_SOURCE_LAUNCH)
    {
        return (s_nav.launch.valid != false) ? &s_nav.launch : NULL;
    }

    if (source == VARIO_NAV_SOURCE_LANDABLE)
    {
        if ((s_nav.active_landable_index < VARIO_NAV_MAX_LANDABLES) &&
            (s_nav.landables[s_nav.active_landable_index].valid != false))
        {
            return &s_nav.landables[s_nav.active_landable_index];
        }
        return NULL;
    }

    if (source == VARIO_NAV_SOURCE_USER_WP)
    {
        if ((s_nav.active_waypoint_index < VARIO_NAV_MAX_USER_WAYPOINTS) &&
            (s_nav.waypoints[s_nav.active_waypoint_index].valid != false))
        {
            return &s_nav.waypoints[s_nav.active_waypoint_index];
        }
        return NULL;
    }

    if (source == VARIO_NAV_SOURCE_MARK)
    {
        if ((s_nav.active_mark_index < VARIO_NAV_MAX_MARKS) &&
            (s_nav.marks[s_nav.active_mark_index].valid != false))
        {
            return &s_nav.marks[s_nav.active_mark_index];
        }
        return NULL;
    }

    return NULL;
}

static bool vario_nav_source_has_valid_target(vario_nav_target_source_t source)
{
    return (vario_nav_get_point_for_source(source) != NULL) ? true : false;
}

static void vario_nav_sync_active_indexes(void)
{
    if ((s_nav.active_landable_index >= VARIO_NAV_MAX_LANDABLES) ||
        (s_nav.landables[s_nav.active_landable_index].valid == false))
    {
        int16_t idx = vario_nav_first_valid_index(s_nav.landables, VARIO_NAV_MAX_LANDABLES);
        s_nav.active_landable_index = (idx >= 0) ? (uint8_t)idx : 0u;
    }

    if ((s_nav.active_waypoint_index >= VARIO_NAV_MAX_USER_WAYPOINTS) ||
        (s_nav.waypoints[s_nav.active_waypoint_index].valid == false))
    {
        int16_t idx = vario_nav_first_valid_index(s_nav.waypoints, VARIO_NAV_MAX_USER_WAYPOINTS);
        s_nav.active_waypoint_index = (idx >= 0) ? (uint8_t)idx : 0u;
    }

    if ((s_nav.active_mark_index >= VARIO_NAV_MAX_MARKS) ||
        (s_nav.marks[s_nav.active_mark_index].valid == false))
    {
        int16_t idx = vario_nav_first_valid_index(s_nav.marks, VARIO_NAV_MAX_MARKS);
        s_nav.active_mark_index = (idx >= 0) ? (uint8_t)idx : 0u;
    }
}

static float vario_nav_polar_sink_mps(const vario_settings_t *settings, float airspeed_mps)
{
    float v1_mps;
    float v2_mps;
    float v3_mps;
    float s1_mps;
    float s2_mps;
    float s3_mps;

    if (settings == NULL)
    {
        return 1.0f;
    }

    v1_mps = ((float)settings->polar_speed1_kmh_x10) * (0.1f / 3.6f);
    v2_mps = ((float)settings->polar_speed2_kmh_x10) * (0.1f / 3.6f);
    v3_mps = ((float)settings->polar_speed3_kmh_x10) * (0.1f / 3.6f);
    s1_mps = ((float)settings->polar_sink1_cms) * 0.01f;
    s2_mps = ((float)settings->polar_sink2_cms) * 0.01f;
    s3_mps = ((float)settings->polar_sink3_cms) * 0.01f;

    if ((v2_mps <= v1_mps) || (v3_mps <= v2_mps))
    {
        return 1.0f;
    }

    if (airspeed_mps <= v1_mps)
    {
        return s1_mps + ((airspeed_mps - v1_mps) * (s2_mps - s1_mps) / (v2_mps - v1_mps));
    }

    if (airspeed_mps <= v2_mps)
    {
        return s1_mps + ((airspeed_mps - v1_mps) * (s2_mps - s1_mps) / (v2_mps - v1_mps));
    }

    if (airspeed_mps <= v3_mps)
    {
        return s2_mps + ((airspeed_mps - v2_mps) * (s3_mps - s2_mps) / (v3_mps - v2_mps));
    }

    return s3_mps + ((airspeed_mps - v3_mps) * (s3_mps - s2_mps) / (v3_mps - v2_mps));
}

static bool vario_nav_distance_bearing_from_ll_e7(int32_t from_lat_e7,
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

    mean_lat_rad = vario_nav_deg_to_rad((from_lat_deg + to_lat_deg) * 0.5f);
    dx_m = (to_lon_deg - from_lon_deg) * (VARIO_NAV_EARTH_METERS_PER_DEG_LON * cosf(mean_lat_rad));
    dy_m = (to_lat_deg - from_lat_deg) * VARIO_NAV_EARTH_METERS_PER_DEG_LAT;

    *out_distance_m = sqrtf((dx_m * dx_m) + (dy_m * dy_m));
    *out_bearing_deg = vario_nav_vector_bearing_deg(dy_m, dx_m);
    return true;
}

static const char *vario_nav_cardinal16(float bearing_deg)
{
    static const char *k_dirs[16] = {
        "N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE",
        "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"
    };
    int index;

    bearing_deg = vario_nav_wrap_360(bearing_deg);
    index = (int)floorf((bearing_deg + 11.25f) / 22.5f);
    index &= 0x0F;
    return k_dirs[index];
}

const char *Vario_Navigation_GetShortSourceLabel(uint8_t target_kind)
{
    switch ((vario_nav_target_kind_t)target_kind)
    {
        case VARIO_NAV_TARGET_KIND_HOME:
            return "HME";
        case VARIO_NAV_TARGET_KIND_LAUNCH:
            return "LCH";
        case VARIO_NAV_TARGET_KIND_LANDABLE:
            return "LND";
        case VARIO_NAV_TARGET_KIND_USER_WP:
            return "WPT";
        case VARIO_NAV_TARGET_KIND_MARK:
            return "MRK";
        case VARIO_NAV_TARGET_KIND_LANDING:
            return "LDG";
        case VARIO_NAV_TARGET_KIND_NONE:
        default:
            return "DST";
    }
}

static const char *vario_nav_source_name(vario_nav_target_source_t source)
{
    switch (source)
    {
        case VARIO_NAV_SOURCE_HOME:
            return "HOME";
        case VARIO_NAV_SOURCE_LAUNCH:
            return "LAUNCH";
        case VARIO_NAV_SOURCE_LANDABLE:
            return "LANDABLE";
        case VARIO_NAV_SOURCE_USER_WP:
            return "WAYPOINT";
        case VARIO_NAV_SOURCE_MARK:
            return "MARK";
        case VARIO_NAV_SOURCE_NONE:
        default:
            return "NONE";
    }
}

static uint8_t vario_nav_page_item_count(vario_nav_page_t page)
{
    switch (page)
    {
        case VARIO_NAV_PAGE_HOME:
            return 4u;
        case VARIO_NAV_PAGE_LAUNCH:
            return 3u;
        case VARIO_NAV_PAGE_MARK_HERE:
            return 4u;
        case VARIO_NAV_PAGE_CLEAR_TARGET:
            return 1u;
        case VARIO_NAV_PAGE_NEARBY_LANDABLE:
            return vario_nav_count_valid_points(s_nav.landables, VARIO_NAV_MAX_LANDABLES);
        case VARIO_NAV_PAGE_USER_WAYPOINTS:
            return (uint8_t)(vario_nav_count_valid_points(s_nav.waypoints, VARIO_NAV_MAX_USER_WAYPOINTS) +
                             vario_nav_count_valid_points(s_nav.marks, VARIO_NAV_MAX_MARKS));
        case VARIO_NAV_PAGE_NONE:
        default:
            return 0u;
    }
}

static void vario_nav_clamp_cursor_to_page(void)
{
    uint8_t count;

    count = vario_nav_page_item_count(s_nav.open_page);
    if (count == 0u)
    {
        s_nav.cursor = 0u;
    }
    else if (s_nav.cursor >= count)
    {
        s_nav.cursor = (uint8_t)(count - 1u);
    }
}

static bool vario_nav_get_list_point_by_page(vario_nav_page_t page,
                                             uint8_t row,
                                             vario_nav_point_t *out_point,
                                             uint8_t *out_backing_index,
                                             vario_nav_target_source_t *out_source)
{
    uint8_t i;
    uint8_t seen;

    if ((out_point == NULL) || (row >= vario_nav_page_item_count(page)))
    {
        return false;
    }

    memset(out_point, 0, sizeof(*out_point));
    if (out_backing_index != NULL)
    {
        *out_backing_index = 0u;
    }
    if (out_source != NULL)
    {
        *out_source = VARIO_NAV_SOURCE_NONE;
    }

    seen = 0u;

    if (page == VARIO_NAV_PAGE_NEARBY_LANDABLE)
    {
        for (i = 0u; i < VARIO_NAV_MAX_LANDABLES; ++i)
        {
            if (s_nav.landables[i].valid == false)
            {
                continue;
            }
            if (seen == row)
            {
                *out_point = s_nav.landables[i];
                if (out_backing_index != NULL)
                {
                    *out_backing_index = i;
                }
                if (out_source != NULL)
                {
                    *out_source = VARIO_NAV_SOURCE_LANDABLE;
                }
                return true;
            }
            ++seen;
        }
    }
    else if (page == VARIO_NAV_PAGE_USER_WAYPOINTS)
    {
        for (i = 0u; i < VARIO_NAV_MAX_USER_WAYPOINTS; ++i)
        {
            if (s_nav.waypoints[i].valid == false)
            {
                continue;
            }
            if (seen == row)
            {
                *out_point = s_nav.waypoints[i];
                if (out_backing_index != NULL)
                {
                    *out_backing_index = i;
                }
                if (out_source != NULL)
                {
                    *out_source = VARIO_NAV_SOURCE_USER_WP;
                }
                return true;
            }
            ++seen;
        }
        for (i = 0u; i < VARIO_NAV_MAX_MARKS; ++i)
        {
            if (s_nav.marks[i].valid == false)
            {
                continue;
            }
            if (seen == row)
            {
                *out_point = s_nav.marks[i];
                if (out_backing_index != NULL)
                {
                    *out_backing_index = i;
                }
                if (out_source != NULL)
                {
                    *out_source = VARIO_NAV_SOURCE_MARK;
                }
                return true;
            }
            ++seen;
        }
    }

    return false;
}

static void vario_nav_sort_landable_rows(const vario_runtime_t *rt, uint8_t *order, uint8_t *out_count)
{
    uint8_t count;
    uint8_t i;
    uint8_t j;

    count = 0u;
    for (i = 0u; i < VARIO_NAV_MAX_LANDABLES; ++i)
    {
        if (s_nav.landables[i].valid != false)
        {
            order[count++] = i;
        }
    }

    for (i = 0u; i < count; ++i)
    {
        for (j = (uint8_t)(i + 1u); j < count; ++j)
        {
            vario_nav_active_target_t target_a;
            vario_nav_active_target_t target_b;
            vario_nav_solution_t sol_a;
            vario_nav_solution_t sol_b;
            bool swap;

            vario_nav_fill_runtime_target(&target_a, &s_nav.landables[order[i]]);
            vario_nav_fill_runtime_target(&target_b, &s_nav.landables[order[j]]);
            memset(&sol_a, 0, sizeof(sol_a));
            memset(&sol_b, 0, sizeof(sol_b));
            Vario_Navigation_ComputeSolutionForTarget(rt, &target_a, &sol_a);
            Vario_Navigation_ComputeSolutionForTarget(rt, &target_b, &sol_b);

            swap = false;
            if ((sol_b.final_glide_valid != false) && (sol_a.final_glide_valid == false))
            {
                swap = true;
            }
            else if ((sol_b.final_glide_valid == sol_a.final_glide_valid) &&
                     (sol_b.final_glide_valid != false) &&
                     (sol_b.arrival_height_m > sol_a.arrival_height_m))
            {
                swap = true;
            }
            else if ((sol_b.final_glide_valid == sol_a.final_glide_valid) &&
                     (fabsf(sol_b.arrival_height_m - sol_a.arrival_height_m) < 1.0f) &&
                     (sol_b.distance_m < sol_a.distance_m))
            {
                swap = true;
            }
            else if ((sol_b.final_glide_valid == sol_a.final_glide_valid) &&
                     (sol_b.valid != false) && (sol_a.valid != false) &&
                     (sol_b.distance_m + 5.0f < sol_a.distance_m))
            {
                swap = true;
            }

            if (swap != false)
            {
                uint8_t tmp;
                tmp = order[i];
                order[i] = order[j];
                order[j] = tmp;
            }
        }
    }

    if (out_count != NULL)
    {
        *out_count = count;
    }
}

static bool vario_nav_get_page_row_point(vario_nav_page_t page,
                                         const vario_runtime_t *rt,
                                         uint8_t row,
                                         vario_nav_point_t *out_point,
                                         uint8_t *out_backing_index,
                                         vario_nav_target_source_t *out_source)
{
    uint8_t order[VARIO_NAV_MAX_LANDABLES];
    uint8_t count;

    if (page == VARIO_NAV_PAGE_NEARBY_LANDABLE)
    {
        uint8_t sorted_index;
        vario_nav_sort_landable_rows(rt, order, &count);
        if ((row >= count) || (out_point == NULL))
        {
            return false;
        }
        sorted_index = order[row];
        *out_point = s_nav.landables[sorted_index];
        if (out_backing_index != NULL)
        {
            *out_backing_index = sorted_index;
        }
        if (out_source != NULL)
        {
            *out_source = VARIO_NAV_SOURCE_LANDABLE;
        }
        return true;
    }

    return vario_nav_get_list_point_by_page(page, row, out_point, out_backing_index, out_source);
}

bool Vario_Navigation_ComputeSolutionForTarget(const vario_runtime_t *rt,
                                               const vario_nav_active_target_t *target,
                                               vario_nav_solution_t *out_solution)
{
    const vario_settings_t *settings;
    float track_n;
    float track_e;
    float wind_n_mps;
    float wind_e_mps;
    float along_wind_mps;
    float cruise_airspeed_mps;
    float ground_along_mps;
    float cruise_sink_mps;
    float available_height_m;
    float height_loss_m;

    if (out_solution == NULL)
    {
        return false;
    }

    memset(out_solution, 0, sizeof(*out_solution));
    if ((rt == NULL) || (target == NULL) || (target->valid == false) ||
        (vario_nav_runtime_has_valid_gps(rt) == false))
    {
        return false;
    }

    if (vario_nav_distance_bearing_from_ll_e7(rt->gps.fix.lat,
                                              rt->gps.fix.lon,
                                              target->lat_e7,
                                              target->lon_e7,
                                              &out_solution->distance_m,
                                              &out_solution->bearing_deg) == false)
    {
        return false;
    }

    out_solution->valid = true;
    out_solution->heading_valid = rt->heading_valid;
    if (rt->heading_valid != false)
    {
        out_solution->relative_bearing_deg = vario_nav_wrap_pm180(out_solution->bearing_deg - rt->heading_deg);
    }
    else
    {
        out_solution->relative_bearing_deg = out_solution->bearing_deg;
    }

    if ((target->has_elevation == false) || (out_solution->distance_m <= 1.0f))
    {
        return true;
    }

    settings = Vario_Settings_Get();
    if (settings == NULL)
    {
        return true;
    }

    track_n = cosf(vario_nav_deg_to_rad(out_solution->bearing_deg));
    track_e = sinf(vario_nav_deg_to_rad(out_solution->bearing_deg));

    wind_n_mps = 0.0f;
    wind_e_mps = 0.0f;
    if (rt->wind_valid != false)
    {
        float wind_to_deg;
        float wind_to_rad;
        wind_to_deg = vario_nav_wrap_360(rt->wind_from_deg + 180.0f);
        wind_to_rad = vario_nav_deg_to_rad(wind_to_deg);
        wind_n_mps = cosf(wind_to_rad) * (rt->wind_speed_kmh / 3.6f);
        wind_e_mps = sinf(wind_to_rad) * (rt->wind_speed_kmh / 3.6f);
    }

    along_wind_mps = (wind_n_mps * track_n) + (wind_e_mps * track_e);

    if (rt->speed_to_fly_valid != false)
    {
        cruise_airspeed_mps = rt->speed_to_fly_kmh / 3.6f;
    }
    else
    {
        cruise_airspeed_mps = rt->estimated_airspeed_kmh / 3.6f;
        if (cruise_airspeed_mps < 8.0f)
        {
            cruise_airspeed_mps = 8.0f;
        }
    }

    cruise_sink_mps = vario_nav_polar_sink_mps(settings, cruise_airspeed_mps);
    ground_along_mps = cruise_airspeed_mps + along_wind_mps;
    if (ground_along_mps < 2.0f)
    {
        return true;
    }

    available_height_m = rt->filtered_altitude_m - target->altitude_m - (float)settings->final_glide_safety_margin_m;
    height_loss_m = cruise_sink_mps * (out_solution->distance_m / ground_along_mps);

    if (available_height_m > 1.0f)
    {
        out_solution->required_glide_ratio = out_solution->distance_m / available_height_m;
        if (out_solution->required_glide_ratio < 0.0f)
        {
            out_solution->required_glide_ratio = 0.0f;
        }
        if (out_solution->required_glide_ratio > 999.0f)
        {
            out_solution->required_glide_ratio = 999.0f;
        }
    }
    else
    {
        out_solution->required_glide_ratio = 999.0f;
    }

    out_solution->arrival_height_m = available_height_m - height_loss_m;
    out_solution->final_glide_valid = true;
    return true;
}

void Vario_Navigation_Init(void)
{
    memset(&s_nav, 0, sizeof(s_nav));
    s_nav.valid = true;
    s_nav.next_id = 1u;
    s_nav.next_wp_index = 1u;
    s_nav.next_field_index = 1u;
    s_nav.next_mark_index = 1u;
    s_nav.active_source = VARIO_NAV_SOURCE_NONE;
}

void Vario_Navigation_ResetFlightRam(void)
{
    s_nav.launch.valid = false;
    s_nav.last_landing.valid = false;
    s_nav.open_page = VARIO_NAV_PAGE_NONE;
    s_nav.cursor = 0u;

    if (s_nav.active_source == VARIO_NAV_SOURCE_LAUNCH)
    {
        s_nav.active_source = VARIO_NAV_SOURCE_NONE;
    }
}

void Vario_Navigation_OnTakeoff(const vario_runtime_t *rt)
{
    vario_nav_point_t point;

    if (vario_nav_capture_runtime_point(rt,
                                        &point,
                                        VARIO_NAV_TARGET_KIND_LAUNCH,
                                        true,
                                        "LAUNCH") == false)
    {
        return;
    }

    s_nav.launch = point;
    s_nav.active_source = VARIO_NAV_SOURCE_LAUNCH;
}

void Vario_Navigation_OnLanding(const vario_runtime_t *rt, uint32_t now_ms)
{
    vario_nav_point_t point;

    if (vario_nav_capture_runtime_point(rt,
                                        &point,
                                        VARIO_NAV_TARGET_KIND_LANDING,
                                        true,
                                        "LANDING") == false)
    {
        return;
    }

    s_nav.last_landing = point;
    UI_Confirm_Show("LANDING SAVED!",
                    "DO YOU WANT TO SAVE THIS FIELD?",
                    "F1 = NO",
                    "F2 = AS FIELD",
                    "F3 = SET HOME",
                    (uint16_t)VARIO_NAV_CONFIRM_LANDING_SAVE,
                    now_ms);
}

bool Vario_Navigation_GetActiveTarget(vario_nav_active_target_t *out_target)
{
    const vario_nav_point_t *point;

    if (out_target == NULL)
    {
        return false;
    }

    vario_nav_sync_active_indexes();
    point = vario_nav_get_point_for_source(s_nav.active_source);
    vario_nav_fill_runtime_target(out_target, point);
    return (out_target->valid != false) ? true : false;
}

vario_nav_target_source_t Vario_Navigation_GetActiveSource(void)
{
    return s_nav.active_source;
}

void Vario_Navigation_CycleTargetSource(void)
{
    static const vario_nav_target_source_t k_order[] = {
        VARIO_NAV_SOURCE_LAUNCH,
        VARIO_NAV_SOURCE_HOME,
        VARIO_NAV_SOURCE_LANDABLE,
        VARIO_NAV_SOURCE_USER_WP,
        VARIO_NAV_SOURCE_MARK,
        VARIO_NAV_SOURCE_NONE
    };
    uint8_t i;
    uint8_t start;

    start = 0u;
    for (i = 0u; i < (uint8_t)(sizeof(k_order) / sizeof(k_order[0])); ++i)
    {
        if (k_order[i] == s_nav.active_source)
        {
            start = i;
            break;
        }
    }

    for (i = 1u; i <= (uint8_t)(sizeof(k_order) / sizeof(k_order[0])); ++i)
    {
        uint8_t idx;
        idx = (uint8_t)((start + i) % (uint8_t)(sizeof(k_order) / sizeof(k_order[0])));
        if ((k_order[idx] == VARIO_NAV_SOURCE_NONE) || (vario_nav_source_has_valid_target(k_order[idx]) != false))
        {
            s_nav.active_source = k_order[idx];
            return;
        }
    }

    s_nav.active_source = VARIO_NAV_SOURCE_NONE;
}

void Vario_Navigation_ClearTarget(void)
{
    s_nav.active_source = VARIO_NAV_SOURCE_NONE;
}

void Vario_Navigation_FormatActiveSourceToast(char *out_text, size_t out_size)
{
    if ((out_text == NULL) || (out_size == 0u))
    {
        return;
    }

    snprintf(out_text, out_size, "Target: %s", vario_nav_source_name(s_nav.active_source));
}

void Vario_Navigation_OpenPage(vario_nav_page_t page)
{
    s_nav.open_page = page;
    s_nav.cursor = 0u;
    vario_nav_clamp_cursor_to_page();
}

void Vario_Navigation_ClosePage(void)
{
    s_nav.open_page = VARIO_NAV_PAGE_NONE;
    s_nav.cursor = 0u;
}

bool Vario_Navigation_IsPageOpen(void)
{
    return (s_nav.open_page != VARIO_NAV_PAGE_NONE) ? true : false;
}

vario_nav_page_t Vario_Navigation_GetOpenPage(void)
{
    return s_nav.open_page;
}

void Vario_Navigation_MoveCursor(int8_t direction)
{
    uint8_t count;
    int16_t next;

    count = vario_nav_page_item_count(s_nav.open_page);
    if (count == 0u)
    {
        s_nav.cursor = 0u;
        return;
    }

    next = (int16_t)s_nav.cursor + ((direction >= 0) ? 1 : -1);
    if (next < 0)
    {
        next = (int16_t)count - 1;
    }
    else if (next >= (int16_t)count)
    {
        next = 0;
    }

    s_nav.cursor = (uint8_t)next;
}

static void vario_nav_show_simple_toast(const char *text, uint32_t now_ms)
{
    UI_Toast_Show(text, NULL, 0u, 0u, now_ms, 1200u);
}

static void vario_nav_activate_point_source(vario_nav_target_source_t source,
                                            uint8_t backing_index,
                                            uint32_t now_ms)
{
    char toast[40];
    const vario_nav_point_t *point;

    if (source == VARIO_NAV_SOURCE_LANDABLE)
    {
        s_nav.active_landable_index = backing_index;
    }
    else if (source == VARIO_NAV_SOURCE_USER_WP)
    {
        s_nav.active_waypoint_index = backing_index;
    }
    else if (source == VARIO_NAV_SOURCE_MARK)
    {
        s_nav.active_mark_index = backing_index;
    }

    s_nav.active_source = source;
    point = vario_nav_get_point_for_source(source);
    if ((point != NULL) && (point->name[0] != '\0'))
    {
        snprintf(toast, sizeof(toast), "Target: %s", point->name);
    }
    else
    {
        snprintf(toast, sizeof(toast), "Target: %s", vario_nav_source_name(source));
    }
    vario_nav_show_simple_toast(toast, now_ms);
}

static void vario_nav_save_last_landing_as_field(uint32_t now_ms)
{
    vario_nav_point_t point;

    if (s_nav.last_landing.valid == false)
    {
        return;
    }

    point = s_nav.last_landing;
    point.kind = VARIO_NAV_TARGET_KIND_LANDABLE;
    point.has_elevation = true;
    point.id = s_nav.next_id++;
    vario_nav_make_name(point.name, sizeof(point.name), "FIELD", s_nav.next_field_index++);
    vario_nav_store_point(s_nav.landables, VARIO_NAV_MAX_LANDABLES, &point, &s_nav.active_landable_index);
    vario_nav_show_simple_toast("Saved as field", now_ms);
}

static void vario_nav_set_home_from_point(const vario_nav_point_t *point, uint32_t now_ms)
{
    if ((point == NULL) || (point->valid == false))
    {
        return;
    }

    s_nav.home = *point;
    s_nav.home.kind = VARIO_NAV_TARGET_KIND_HOME;
    s_nav.home.has_elevation = true;
    s_nav.home.id = s_nav.next_id++;
    vario_nav_copy_name(s_nav.home.name, sizeof(s_nav.home.name), "HOME");
    vario_nav_show_simple_toast("Home updated", now_ms);
}

static void vario_nav_save_current_here(uint8_t action_row, uint32_t now_ms)
{
    const vario_runtime_t *rt;
    vario_nav_point_t point;

    rt = Vario_State_GetRuntime();
    if (rt == NULL)
    {
        return;
    }

    if (action_row == 0u)
    {
        vario_nav_make_name(point.name, sizeof(point.name), "WP", s_nav.next_wp_index);
        if (vario_nav_capture_runtime_point(rt,
                                            &point,
                                            VARIO_NAV_TARGET_KIND_USER_WP,
                                            true,
                                            point.name) != false)
        {
            ++s_nav.next_wp_index;
            vario_nav_store_point(s_nav.waypoints,
                                  VARIO_NAV_MAX_USER_WAYPOINTS,
                                  &point,
                                  &s_nav.active_waypoint_index);
            vario_nav_activate_point_source(VARIO_NAV_SOURCE_USER_WP,
                                            s_nav.active_waypoint_index,
                                            now_ms);
        }
    }
    else if (action_row == 1u)
    {
        vario_nav_make_name(point.name, sizeof(point.name), "FIELD", s_nav.next_field_index);
        if (vario_nav_capture_runtime_point(rt,
                                            &point,
                                            VARIO_NAV_TARGET_KIND_LANDABLE,
                                            true,
                                            point.name) != false)
        {
            ++s_nav.next_field_index;
            vario_nav_store_point(s_nav.landables,
                                  VARIO_NAV_MAX_LANDABLES,
                                  &point,
                                  &s_nav.active_landable_index);
            vario_nav_activate_point_source(VARIO_NAV_SOURCE_LANDABLE,
                                            s_nav.active_landable_index,
                                            now_ms);
        }
    }
    else if (action_row == 2u)
    {
        if (vario_nav_capture_runtime_point(rt,
                                            &point,
                                            VARIO_NAV_TARGET_KIND_HOME,
                                            true,
                                            "HOME") != false)
        {
            vario_nav_set_home_from_point(&point, now_ms);
            s_nav.active_source = VARIO_NAV_SOURCE_HOME;
        }
    }
    else if (action_row == 3u)
    {
        vario_nav_make_name(point.name, sizeof(point.name), "MARK", s_nav.next_mark_index);
        if (vario_nav_capture_runtime_point(rt,
                                            &point,
                                            VARIO_NAV_TARGET_KIND_MARK,
                                            false,
                                            point.name) != false)
        {
            ++s_nav.next_mark_index;
            vario_nav_store_point(s_nav.marks,
                                  VARIO_NAV_MAX_MARKS,
                                  &point,
                                  &s_nav.active_mark_index);
            vario_nav_activate_point_source(VARIO_NAV_SOURCE_MARK,
                                            s_nav.active_mark_index,
                                            now_ms);
        }
    }
}

void Vario_Navigation_ActivateSelected(uint32_t now_ms)
{
    const vario_runtime_t *rt;
    vario_nav_point_t point;
    uint8_t backing_index;
    vario_nav_target_source_t source;

    rt = Vario_State_GetRuntime();
    backing_index = 0u;
    source = VARIO_NAV_SOURCE_NONE;
    memset(&point, 0, sizeof(point));

    switch (s_nav.open_page)
    {
        case VARIO_NAV_PAGE_HOME:
            if (s_nav.cursor == 0u)
            {
                if (s_nav.home.valid != false)
                {
                    s_nav.active_source = VARIO_NAV_SOURCE_HOME;
                    vario_nav_show_simple_toast("Target: HOME", now_ms);
                    Vario_Navigation_ClosePage();
                    Vario_State_ReturnToMain();
                }
            }
            else if (s_nav.cursor == 1u)
            {
                vario_nav_save_current_here(2u, now_ms);
            }
            else if (s_nav.cursor == 2u)
            {
                if (s_nav.launch.valid != false)
                {
                    vario_nav_set_home_from_point(&s_nav.launch, now_ms);
                }
            }
            else if (s_nav.cursor == 3u)
            {
                memset(&s_nav.home, 0, sizeof(s_nav.home));
                if (s_nav.active_source == VARIO_NAV_SOURCE_HOME)
                {
                    s_nav.active_source = VARIO_NAV_SOURCE_NONE;
                }
                vario_nav_show_simple_toast("Home cleared", now_ms);
            }
            break;

        case VARIO_NAV_PAGE_LAUNCH:
            if (s_nav.cursor == 0u)
            {
                if (s_nav.launch.valid != false)
                {
                    s_nav.active_source = VARIO_NAV_SOURCE_LAUNCH;
                    vario_nav_show_simple_toast("Target: LAUNCH", now_ms);
                    Vario_Navigation_ClosePage();
                    Vario_State_ReturnToMain();
                }
            }
            else if (s_nav.cursor == 1u)
            {
                if (s_nav.launch.valid != false)
                {
                    point = s_nav.launch;
                    point.kind = VARIO_NAV_TARGET_KIND_LANDABLE;
                    point.has_elevation = true;
                    point.id = s_nav.next_id++;
                    vario_nav_make_name(point.name, sizeof(point.name), "FIELD", s_nav.next_field_index++);
                    vario_nav_store_point(s_nav.landables,
                                          VARIO_NAV_MAX_LANDABLES,
                                          &point,
                                          &s_nav.active_landable_index);
                    vario_nav_show_simple_toast("Launch saved as field", now_ms);
                }
            }
            else if (s_nav.cursor == 2u)
            {
                if (s_nav.launch.valid != false)
                {
                    vario_nav_set_home_from_point(&s_nav.launch, now_ms);
                }
            }
            break;

        case VARIO_NAV_PAGE_NEARBY_LANDABLE:
        case VARIO_NAV_PAGE_USER_WAYPOINTS:
            if (vario_nav_get_page_row_point(s_nav.open_page,
                                             rt,
                                             s_nav.cursor,
                                             &point,
                                             &backing_index,
                                             &source) != false)
            {
                vario_nav_activate_point_source(source, backing_index, now_ms);
                Vario_Navigation_ClosePage();
                Vario_State_ReturnToMain();
            }
            break;

        case VARIO_NAV_PAGE_MARK_HERE:
            vario_nav_save_current_here(s_nav.cursor, now_ms);
            Vario_Navigation_ClosePage();
            Vario_State_ReturnToMain();
            break;

        case VARIO_NAV_PAGE_CLEAR_TARGET:
            Vario_Navigation_ClearTarget();
            vario_nav_show_simple_toast("Target cleared", now_ms);
            Vario_Navigation_ClosePage();
            Vario_State_ReturnToMain();
            break;

        case VARIO_NAV_PAGE_NONE:
        default:
            break;
    }
}

void Vario_Navigation_HandleMenuAction(uint16_t action_id, uint32_t now_ms)
{
    (void)now_ms;

    switch ((vario_nav_menu_action_t)action_id)
    {
        case VARIO_NAV_MENU_ACTION_HOME:
            Vario_Navigation_OpenPage(VARIO_NAV_PAGE_HOME);
            Vario_State_EnterSettings();
            break;

        case VARIO_NAV_MENU_ACTION_LAUNCH:
            Vario_Navigation_OpenPage(VARIO_NAV_PAGE_LAUNCH);
            Vario_State_EnterSettings();
            break;

        case VARIO_NAV_MENU_ACTION_NEARBY_LANDABLE:
            Vario_Navigation_OpenPage(VARIO_NAV_PAGE_NEARBY_LANDABLE);
            Vario_State_EnterSettings();
            break;

        case VARIO_NAV_MENU_ACTION_USER_WAYPOINTS:
            Vario_Navigation_OpenPage(VARIO_NAV_PAGE_USER_WAYPOINTS);
            Vario_State_EnterSettings();
            break;

        case VARIO_NAV_MENU_ACTION_MARK_HERE:
            Vario_Navigation_OpenPage(VARIO_NAV_PAGE_MARK_HERE);
            Vario_State_EnterSettings();
            break;

        case VARIO_NAV_MENU_ACTION_CLEAR_TARGET:
            Vario_Navigation_OpenPage(VARIO_NAV_PAGE_CLEAR_TARGET);
            Vario_State_EnterSettings();
            break;

        case VARIO_NAV_MENU_ACTION_NONE:
        default:
            break;
    }
}

bool Vario_Navigation_HasConfirmHandler(uint16_t context_id)
{
    return (context_id == (uint16_t)VARIO_NAV_CONFIRM_LANDING_SAVE) ? true : false;
}

void Vario_Navigation_HandleConfirmChoice(uint16_t context_id, uint8_t button_id, uint32_t now_ms)
{
    if (context_id != (uint16_t)VARIO_NAV_CONFIRM_LANDING_SAVE)
    {
        return;
    }

    if (button_id == 1u)
    {
        vario_nav_show_simple_toast("Landing kept in RAM", now_ms);
    }
    else if (button_id == 2u)
    {
        vario_nav_save_last_landing_as_field(now_ms);
    }
    else if (button_id == 3u)
    {
        if (s_nav.last_landing.valid != false)
        {
            vario_nav_set_home_from_point(&s_nav.last_landing, now_ms);
            s_nav.active_source = VARIO_NAV_SOURCE_HOME;
        }
    }
}

static void vario_nav_format_solution_subtitle(char *buf,
                                               size_t buf_len,
                                               const vario_nav_solution_t *sol,
                                               bool has_elevation)
{
    const char *alt_unit;
    long arrival_disp;

    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    if ((sol == NULL) || (sol->valid == false))
    {
        snprintf(buf, buf_len, "NO GPS");
        return;
    }

    if ((has_elevation != false) && (sol->final_glide_valid != false))
    {
        alt_unit = Vario_Settings_GetAltitudeUnitText();
        arrival_disp = (long)Vario_Settings_AltitudeMetersToDisplayRounded(sol->arrival_height_m);
        snprintf(buf, buf_len, "L/D %.1f %+ld%s", (double)sol->required_glide_ratio, arrival_disp, alt_unit);
    }
    else
    {
        snprintf(buf, buf_len, "NAV ONLY");
    }
}

static void vario_nav_draw_dir_arrow(u8g2_t *u8g2, int16_t cx, int16_t cy, float rel_bearing_deg)
{
    float rad;
    float nx;
    float ny;
    int16_t tip_x;
    int16_t tip_y;
    int16_t base_x;
    int16_t base_y;
    int16_t left_x;
    int16_t left_y;
    int16_t right_x;
    int16_t right_y;

    if (u8g2 == NULL)
    {
        return;
    }

    rad = vario_nav_deg_to_rad(rel_bearing_deg);
    nx = cosf(rad);
    ny = sinf(rad);

    tip_x = (int16_t)lroundf((float)cx + (ny * 5.0f));
    tip_y = (int16_t)lroundf((float)cy - (nx * 5.0f));
    base_x = (int16_t)lroundf((float)cx - (ny * 2.0f));
    base_y = (int16_t)lroundf((float)cy + (nx * 2.0f));
    left_x = (int16_t)lroundf((float)base_x + (nx * 2.0f) - (ny * 1.5f));
    left_y = (int16_t)lroundf((float)base_y + (ny * 2.0f) + (nx * 1.5f));
    right_x = (int16_t)lroundf((float)base_x - (nx * 2.0f) - (ny * 1.5f));
    right_y = (int16_t)lroundf((float)base_y - (ny * 2.0f) + (nx * 1.5f));

    u8g2_DrawLine(u8g2, base_x, base_y, tip_x, tip_y);
    u8g2_DrawLine(u8g2, tip_x, tip_y, left_x, left_y);
    u8g2_DrawLine(u8g2, tip_x, tip_y, right_x, right_y);
}

static void vario_nav_get_list_window(uint8_t count, uint8_t cursor, uint8_t *out_start, uint8_t *out_rows)
{
    uint8_t start;
    uint8_t rows;

    start = 0u;
    rows = count;
    if (rows > VARIO_NAV_LIST_VISIBLE_ROWS)
    {
        rows = VARIO_NAV_LIST_VISIBLE_ROWS;
        if (cursor >= rows)
        {
            start = (uint8_t)(cursor - rows + 1u);
        }
        if ((start + rows) > count)
        {
            start = (uint8_t)(count - rows);
        }
    }

    if (out_start != NULL)
    {
        *out_start = start;
    }
    if (out_rows != NULL)
    {
        *out_rows = rows;
    }
}

static void vario_nav_draw_list_row(u8g2_t *u8g2,
                                    const vario_viewport_t *v,
                                    int16_t y_baseline,
                                    bool selected,
                                    const vario_nav_point_t *point,
                                    const vario_nav_solution_t *sol)
{
    char left_text[40];
    char right_text[20];
    int16_t row_top;
    int16_t row_h;
    int16_t arrow_cx;
    int16_t arrow_cy;
    int16_t text_x;

    if ((u8g2 == NULL) || (v == NULL) || (point == NULL) || (sol == NULL))
    {
        return;
    }

    row_h = 14;
    row_top = (int16_t)(y_baseline - 11);

    if (selected != false)
    {
        u8g2_SetDrawColor(u8g2, 1);
        u8g2_DrawBox(u8g2,
                     (u8g2_uint_t)(v->x + 2),
                     (u8g2_uint_t)row_top,
                     (u8g2_uint_t)(v->w - 4),
                     (u8g2_uint_t)row_h);
        u8g2_SetDrawColor(u8g2, 0);
    }
    else
    {
        u8g2_SetDrawColor(u8g2, 1);
        u8g2_DrawFrame(u8g2,
                       (u8g2_uint_t)(v->x + 2),
                       (u8g2_uint_t)row_top,
                       (u8g2_uint_t)(v->w - 4),
                       (u8g2_uint_t)row_h);
    }

    arrow_cx = (int16_t)(v->x + 12);
    arrow_cy = (int16_t)(row_top + (row_h / 2));
    vario_nav_draw_dir_arrow(u8g2, arrow_cx, arrow_cy, sol->relative_bearing_deg);

    snprintf(left_text,
             sizeof(left_text),
             "%s %s",
             point->name,
             vario_nav_cardinal16(sol->bearing_deg));

    if (sol->valid != false)
    {
        snprintf(right_text,
                 sizeof(right_text),
                 "%.1f%s",
                 (double)Vario_Settings_NavDistanceMetersToDisplayFloat(sol->distance_m),
                 Vario_Settings_GetNavDistanceUnitText());
    }
    else
    {
        snprintf(right_text, sizeof(right_text), "---.-");
    }

    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    text_x = (int16_t)(arrow_cx + 9);
    u8g2_DrawStr(u8g2, (u8g2_uint_t)text_x, (u8g2_uint_t)y_baseline, left_text);
    Vario_Display_DrawTextRight(u8g2, (int16_t)(v->x + v->w - 8), y_baseline, right_text);
    u8g2_SetDrawColor(u8g2, 1);
}

static void vario_nav_get_page_title(char *title, size_t title_size)
{
    if ((title == NULL) || (title_size == 0u))
    {
        return;
    }

    switch (s_nav.open_page)
    {
        case VARIO_NAV_PAGE_HOME:
            snprintf(title, title_size, "TARGET HOME");
            break;
        case VARIO_NAV_PAGE_LAUNCH:
            snprintf(title, title_size, "TARGET LAUNCH");
            break;
        case VARIO_NAV_PAGE_NEARBY_LANDABLE:
            snprintf(title, title_size, "NEARBY LANDABLE");
            break;
        case VARIO_NAV_PAGE_USER_WAYPOINTS:
            snprintf(title, title_size, "USER WAYPOINTS");
            break;
        case VARIO_NAV_PAGE_MARK_HERE:
            snprintf(title, title_size, "MARK HERE");
            break;
        case VARIO_NAV_PAGE_CLEAR_TARGET:
            snprintf(title, title_size, "CLEAR TARGET");
            break;
        case VARIO_NAV_PAGE_NONE:
        default:
            snprintf(title, title_size, "NAV");
            break;
    }
}

void Vario_Navigation_RenderSettingPage(u8g2_t *u8g2)
{
    const vario_viewport_t *v;
    const vario_runtime_t *rt;
    char title[32];
    char subtitle[48];
    uint8_t count;
    uint8_t start;
    uint8_t rows;
    uint8_t row;
    int16_t y;

    if (u8g2 == NULL)
    {
        return;
    }

    v = Vario_Display_GetContentViewport();
    rt = Vario_State_GetRuntime();
    if ((v == NULL) || (rt == NULL))
    {
        return;
    }

    vario_nav_get_page_title(title, sizeof(title));
    subtitle[0] = '\0';

    if ((s_nav.open_page == VARIO_NAV_PAGE_NEARBY_LANDABLE) ||
        (s_nav.open_page == VARIO_NAV_PAGE_USER_WAYPOINTS))
    {
        vario_nav_point_t point;
        uint8_t backing_index;
        vario_nav_target_source_t source;
        vario_nav_active_target_t target;
        vario_nav_solution_t sol;

        memset(&point, 0, sizeof(point));
        memset(&target, 0, sizeof(target));
        memset(&sol, 0, sizeof(sol));
        backing_index = 0u;
        source = VARIO_NAV_SOURCE_NONE;
        if (vario_nav_get_page_row_point(s_nav.open_page,
                                         rt,
                                         s_nav.cursor,
                                         &point,
                                         &backing_index,
                                         &source) != false)
        {
            vario_nav_fill_runtime_target(&target, &point);
            Vario_Navigation_ComputeSolutionForTarget(rt, &target, &sol);
            vario_nav_format_solution_subtitle(subtitle, sizeof(subtitle), &sol, point.has_elevation);
        }
    }
    else if (s_nav.open_page == VARIO_NAV_PAGE_HOME)
    {
        vario_nav_active_target_t target;
        vario_nav_solution_t sol;
        memset(&target, 0, sizeof(target));
        memset(&sol, 0, sizeof(sol));
        vario_nav_fill_runtime_target(&target, &s_nav.home);
        Vario_Navigation_ComputeSolutionForTarget(rt, &target, &sol);
        vario_nav_format_solution_subtitle(subtitle, sizeof(subtitle), &sol, s_nav.home.has_elevation);
    }
    else if (s_nav.open_page == VARIO_NAV_PAGE_LAUNCH)
    {
        vario_nav_active_target_t target;
        vario_nav_solution_t sol;
        memset(&target, 0, sizeof(target));
        memset(&sol, 0, sizeof(sol));
        vario_nav_fill_runtime_target(&target, &s_nav.launch);
        Vario_Navigation_ComputeSolutionForTarget(rt, &target, &sol);
        vario_nav_format_solution_subtitle(subtitle, sizeof(subtitle), &sol, s_nav.launch.has_elevation);
    }
    else if (s_nav.open_page == VARIO_NAV_PAGE_CLEAR_TARGET)
    {
        snprintf(subtitle, sizeof(subtitle), "%s", vario_nav_source_name(s_nav.active_source));
    }

    Vario_Display_DrawPageTitle(u8g2, v, title, subtitle);

    count = vario_nav_page_item_count(s_nav.open_page);
    vario_nav_get_list_window(count, s_nav.cursor, &start, &rows);

    if (s_nav.open_page == VARIO_NAV_PAGE_HOME)
    {
        Vario_Display_DrawMenuRow(u8g2, v, (int16_t)(v->y + 20), (s_nav.cursor == 0u), "Activate", (s_nav.home.valid != false) ? "HOME" : "--");
        Vario_Display_DrawMenuRow(u8g2, v, (int16_t)(v->y + 36), (s_nav.cursor == 1u), "Set Home Here", "GPS");
        Vario_Display_DrawMenuRow(u8g2, v, (int16_t)(v->y + 52), (s_nav.cursor == 2u), "Home = Launch", (s_nav.launch.valid != false) ? "COPY" : "--");
        Vario_Display_DrawMenuRow(u8g2, v, (int16_t)(v->y + 68), (s_nav.cursor == 3u), "Clear Home", " ");
        return;
    }

    if (s_nav.open_page == VARIO_NAV_PAGE_LAUNCH)
    {
        Vario_Display_DrawMenuRow(u8g2, v, (int16_t)(v->y + 20), (s_nav.cursor == 0u), "Activate", (s_nav.launch.valid != false) ? "LAUNCH" : "--");
        Vario_Display_DrawMenuRow(u8g2, v, (int16_t)(v->y + 36), (s_nav.cursor == 1u), "Save as Field", "RAM");
        Vario_Display_DrawMenuRow(u8g2, v, (int16_t)(v->y + 52), (s_nav.cursor == 2u), "Set Home = Launch", "COPY");
        return;
    }

    if (s_nav.open_page == VARIO_NAV_PAGE_MARK_HERE)
    {
        Vario_Display_DrawMenuRow(u8g2, v, (int16_t)(v->y + 20), (s_nav.cursor == 0u), "Save as WP", "RAM");
        Vario_Display_DrawMenuRow(u8g2, v, (int16_t)(v->y + 36), (s_nav.cursor == 1u), "Save as Field", "RAM");
        Vario_Display_DrawMenuRow(u8g2, v, (int16_t)(v->y + 52), (s_nav.cursor == 2u), "Set Home Here", "GPS");
        Vario_Display_DrawMenuRow(u8g2, v, (int16_t)(v->y + 68), (s_nav.cursor == 3u), "Target Here", "MARK");
        return;
    }

    if (s_nav.open_page == VARIO_NAV_PAGE_CLEAR_TARGET)
    {
        Vario_Display_DrawMenuRow(u8g2, v, (int16_t)(v->y + 20), true, "Clear Active Target", "OK");
        return;
    }

    y = (int16_t)(v->y + VARIO_NAV_TITLE_ROW_Y0);
    for (row = 0u; row < rows; ++row)
    {
        vario_nav_point_t point;
        vario_nav_active_target_t target;
        vario_nav_solution_t sol;
        uint8_t backing_index;
        vario_nav_target_source_t source;

        memset(&point, 0, sizeof(point));
        memset(&target, 0, sizeof(target));
        memset(&sol, 0, sizeof(sol));
        backing_index = 0u;
        source = VARIO_NAV_SOURCE_NONE;

        if (vario_nav_get_page_row_point(s_nav.open_page,
                                         rt,
                                         (uint8_t)(start + row),
                                         &point,
                                         &backing_index,
                                         &source) != false)
        {
            vario_nav_fill_runtime_target(&target, &point);
            Vario_Navigation_ComputeSolutionForTarget(rt, &target, &sol);
            vario_nav_draw_list_row(u8g2,
                                    v,
                                    y,
                                    ((uint8_t)(start + row) == s_nav.cursor),
                                    &point,
                                    &sol);
        }
        y = (int16_t)(y + VARIO_NAV_TITLE_ROW_STEP);
    }

    if ((count == 0u) && (s_nav.open_page == VARIO_NAV_PAGE_NEARBY_LANDABLE))
    {
        Vario_Display_DrawTextCentered(u8g2, (int16_t)(v->x + (v->w / 2)), (int16_t)(v->y + 44), "NO LANDABLES IN RAM");
    }
    else if ((count == 0u) && (s_nav.open_page == VARIO_NAV_PAGE_USER_WAYPOINTS))
    {
        Vario_Display_DrawTextCentered(u8g2, (int16_t)(v->x + (v->w / 2)), (int16_t)(v->y + 44), "NO WAYPOINTS IN RAM");
    }
}
