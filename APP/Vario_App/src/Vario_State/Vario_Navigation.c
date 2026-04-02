#include "Vario_Navigation.h"

#include "Vario_Display_Common.h"
#include "Vario_Persistence.h"
#include "Vario_Settings.h"
#include "ui_confirm.h"
#include "ui_toast.h"
#include "Vario_State.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef VARIO_NAV_MAX_LANDABLES
#define VARIO_NAV_MAX_LANDABLES VARIO_PERSISTENCE_MAX_FIELDS
#endif

#ifndef VARIO_NAV_MAX_USER_WAYPOINTS
#define VARIO_NAV_MAX_USER_WAYPOINTS VARIO_PERSISTENCE_MAX_WAYPOINTS
#endif

#ifndef VARIO_NAV_MAX_MARKS
#define VARIO_NAV_MAX_MARKS 4u
#endif

#ifndef VARIO_NAV_LIST_VISIBLE_ROWS
#define VARIO_NAV_LIST_VISIBLE_ROWS 5u
#endif

#ifndef VARIO_NAV_FIRST_ROW_BASELINE_Y
#define VARIO_NAV_FIRST_ROW_BASELINE_Y 28
#endif

#ifndef VARIO_NAV_ROW_STEP_Y
#define VARIO_NAV_ROW_STEP_Y 16
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

#ifndef VARIO_NAV_NAME_EDITOR_MAX_CHARS
#define VARIO_NAV_NAME_EDITOR_MAX_CHARS 10u
#endif

typedef struct
{
    bool valid;
    vario_nav_page_t open_page;
    uint8_t cursor;

    uint8_t active_site_index;
    uint8_t detail_site_index;

    bool name_edit_active;
    uint8_t name_edit_pos;
    char name_edit_buffer[VARIO_NAV_NAME_MAX];

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

    vario_persist_site_summary_t site_summaries[VARIO_PERSISTENCE_MAX_SITES];
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
    size_t i;

    if ((dst == NULL) || (dst_size == 0u))
    {
        return;
    }

    if (src == NULL)
    {
        dst[0] = '\0';
        return;
    }

    for (i = 0u; (i + 1u) < dst_size; ++i)
    {
        char c;
        c = src[i];
        if (c == '\0')
        {
            break;
        }
        if ((c < 32) || (c > 126))
        {
            c = ' ';
        }
        dst[i] = c;
    }
    dst[i] = '\0';

    while (i > 0u)
    {
        if (dst[i - 1u] != ' ')
        {
            break;
        }
        dst[i - 1u] = '\0';
        --i;
    }
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

static void vario_nav_copy_from_persist(vario_nav_point_t *dst,
                                        const vario_persist_point_t *src)
{
    if ((dst == NULL) || (src == NULL))
    {
        return;
    }

    memset(dst, 0, sizeof(*dst));
    dst->valid = src->valid;
    dst->has_elevation = src->has_elevation;
    dst->id = src->id;
    dst->kind = (vario_nav_target_kind_t)src->kind;
    dst->lat_e7 = src->lat_e7;
    dst->lon_e7 = src->lon_e7;
    dst->altitude_m = src->altitude_m;
    vario_nav_copy_name(dst->name, sizeof(dst->name), src->name);
}

static void vario_nav_copy_to_persist(vario_persist_point_t *dst,
                                      const vario_nav_point_t *src)
{
    if ((dst == NULL) || (src == NULL))
    {
        return;
    }

    memset(dst, 0, sizeof(*dst));
    dst->valid = src->valid;
    dst->has_elevation = src->has_elevation;
    dst->id = src->id;
    dst->kind = (uint8_t)src->kind;
    dst->lat_e7 = src->lat_e7;
    dst->lon_e7 = src->lon_e7;
    dst->altitude_m = src->altitude_m;
    vario_nav_copy_name(dst->name, sizeof(dst->name), src->name);
}

static void vario_nav_refresh_site_summaries(void)
{
    uint8_t i;

    for (i = 0u; i < Vario_Persistence_GetSiteCount(); ++i)
    {
        if (Vario_Persistence_GetSiteSummary(i, &s_nav.site_summaries[i]) == false)
        {
            memset(&s_nav.site_summaries[i], 0, sizeof(s_nav.site_summaries[i]));
            snprintf(s_nav.site_summaries[i].site_name,
                     sizeof(s_nav.site_summaries[i].site_name),
                     "SITE %02u",
                     (unsigned)(i + 1u));
        }
    }
}

static void vario_nav_export_active_site(vario_persist_site_t *out_site)
{
    uint8_t i;

    if (out_site == NULL)
    {
        return;
    }

    memset(out_site, 0, sizeof(*out_site));
    out_site->stored = true;
    vario_nav_copy_name(out_site->site_name,
                        sizeof(out_site->site_name),
                        s_nav.site_summaries[s_nav.active_site_index].site_name);
    out_site->next_id = (s_nav.next_id != 0u) ? s_nav.next_id : 1u;
    out_site->next_field_serial = (s_nav.next_field_index != 0u) ? s_nav.next_field_index : 1u;
    out_site->next_waypoint_serial = (s_nav.next_wp_index != 0u) ? s_nav.next_wp_index : 1u;
    vario_nav_copy_to_persist(&out_site->home, &s_nav.home);
    for (i = 0u; i < VARIO_NAV_MAX_LANDABLES; ++i)
    {
        vario_nav_copy_to_persist(&out_site->landings[i], &s_nav.landables[i]);
    }
    for (i = 0u; i < VARIO_NAV_MAX_USER_WAYPOINTS; ++i)
    {
        vario_nav_copy_to_persist(&out_site->waypoints[i], &s_nav.waypoints[i]);
    }
}

static void vario_nav_apply_loaded_site(const vario_persist_site_t *site)
{
    uint8_t i;

    memset(&s_nav.home, 0, sizeof(s_nav.home));
    memset(s_nav.landables, 0, sizeof(s_nav.landables));
    memset(s_nav.waypoints, 0, sizeof(s_nav.waypoints));

    if (site == NULL)
    {
        s_nav.next_id = 1u;
        s_nav.next_field_index = 1u;
        s_nav.next_wp_index = 1u;
        return;
    }

    vario_nav_copy_from_persist(&s_nav.home, &site->home);
    for (i = 0u; i < VARIO_NAV_MAX_LANDABLES; ++i)
    {
        vario_nav_copy_from_persist(&s_nav.landables[i], &site->landings[i]);
    }
    for (i = 0u; i < VARIO_NAV_MAX_USER_WAYPOINTS; ++i)
    {
        vario_nav_copy_from_persist(&s_nav.waypoints[i], &site->waypoints[i]);
    }

    s_nav.next_id = (site->next_id != 0u) ? site->next_id : 1u;
    s_nav.next_field_index = (site->next_field_serial != 0u) ? site->next_field_serial : 1u;
    s_nav.next_wp_index = (site->next_waypoint_serial != 0u) ? site->next_waypoint_serial : 1u;
}

static bool vario_nav_save_active_site(void)
{
    vario_persist_site_t site;

    if (Vario_Persistence_IsReady() == false)
    {
        return false;
    }

    vario_nav_export_active_site(&site);
    if (Vario_Persistence_SaveSite(s_nav.active_site_index, &site) == false)
    {
        return false;
    }
    vario_nav_refresh_site_summaries();
    return true;
}

static bool vario_nav_load_site_to_nav(uint8_t site_index)
{
    vario_persist_site_t site;

    s_nav.active_site_index = site_index;
    if (Vario_Persistence_LoadSite(site_index, &site) != false)
    {
        vario_nav_apply_loaded_site(&site);
    }
    else
    {
        vario_nav_apply_loaded_site(&site);
    }

    return true;
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

    if ((s_nav.active_source == VARIO_NAV_SOURCE_HOME) && (s_nav.home.valid == false))
    {
        s_nav.active_source = VARIO_NAV_SOURCE_NONE;
    }
    if ((s_nav.active_source == VARIO_NAV_SOURCE_LANDABLE) &&
        ((s_nav.active_landable_index >= VARIO_NAV_MAX_LANDABLES) ||
         (s_nav.landables[s_nav.active_landable_index].valid == false)))
    {
        s_nav.active_source = VARIO_NAV_SOURCE_NONE;
    }
    if ((s_nav.active_source == VARIO_NAV_SOURCE_USER_WP) &&
        ((s_nav.active_waypoint_index >= VARIO_NAV_MAX_USER_WAYPOINTS) ||
         (s_nav.waypoints[s_nav.active_waypoint_index].valid == false)))
    {
        s_nav.active_source = VARIO_NAV_SOURCE_NONE;
    }
    if ((s_nav.active_source == VARIO_NAV_SOURCE_MARK) &&
        ((s_nav.active_mark_index >= VARIO_NAV_MAX_MARKS) ||
         (s_nav.marks[s_nav.active_mark_index].valid == false)))
    {
        s_nav.active_source = VARIO_NAV_SOURCE_NONE;
    }
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

/* -------------------------------------------------------------------------- */
/* Trail overlay marker export                                                */
/*                                                                            */
/* breadcrumb trail renderer 는 navigation 내부 저장 구조를 직접 참조하지       */
/* 않는다. display layer 는 이 export API 만 사용해 START / LAND / WPT         */
/* marker snapshot 을 얻고, 그 결과를 화면 좌표로 투영해서 그린다.             */
/*                                                                            */
/* 이렇게 분리하면                                                             */
/* - display 가 persistence layout 을 몰라도 되고                             */
/* - 추후 site 저장 구조가 바뀌어도 display contract 는 유지할 수 있다.        */
/* -------------------------------------------------------------------------- */
static bool vario_nav_fill_trail_marker_from_point(const vario_nav_point_t *point,
                                                   uint8_t marker_kind,
                                                   vario_nav_trail_marker_t *out_marker)
{
    if (out_marker == NULL)
    {
        return false;
    }

    memset(out_marker, 0, sizeof(*out_marker));
    if ((point == NULL) || (point->valid == false))
    {
        return false;
    }

    out_marker->valid = true;
    out_marker->kind = marker_kind;
    out_marker->lat_e7 = point->lat_e7;
    out_marker->lon_e7 = point->lon_e7;

    if (point->name[0] != '\0')
    {
        snprintf(out_marker->name, sizeof(out_marker->name), "%s", point->name);
    }
    else
    {
        switch ((vario_nav_trail_marker_kind_t)marker_kind)
        {
            case VARIO_NAV_TRAIL_MARKER_START:
                snprintf(out_marker->name, sizeof(out_marker->name), "START");
                break;
            case VARIO_NAV_TRAIL_MARKER_LANDABLE:
                snprintf(out_marker->name, sizeof(out_marker->name), "FIELD");
                break;
            case VARIO_NAV_TRAIL_MARKER_WAYPOINT:
            default:
                snprintf(out_marker->name, sizeof(out_marker->name), "POINT");
                break;
        }
    }

    return true;
}

uint8_t Vario_Navigation_GetTrailMarkerCount(void)
{
    uint8_t count;
    uint8_t i;

    count = 0u;

    /* ------------------------------------------------------------------ */
    /* START 는 launch point 를 export 한다.                                */
    /* trail 의 가장 오래된 breadcrumb 대신 takeoff capture 지점을 쓰면     */
    /* 사용자가 기대하는 "출발점" 과 더 잘 맞는다.                         */
    /* ------------------------------------------------------------------ */
    if (s_nav.launch.valid != false)
    {
        ++count;
    }

    /* ------------------------------------------------------------------ */
    /* 활성 SITE 의 LANDABLE / WAYPOINT 를 모두 overlay marker 로 내보낸다.*/
    /* active target 하나만 보여 주지 않고, 주변에 저장된 field/point 를    */
    /* 함께 보여 줘야 breadcrumb trail page 의 상황 파악이 쉬워진다.        */
    /* ------------------------------------------------------------------ */
    for (i = 0u; i < VARIO_NAV_MAX_LANDABLES; ++i)
    {
        if (s_nav.landables[i].valid != false)
        {
            ++count;
        }
    }

    for (i = 0u; i < VARIO_NAV_MAX_USER_WAYPOINTS; ++i)
    {
        if (s_nav.waypoints[i].valid != false)
        {
            ++count;
        }
    }

    return count;
}

bool Vario_Navigation_GetTrailMarker(uint8_t marker_index,
                                     vario_nav_trail_marker_t *out_marker)
{
    uint8_t logical_index;
    uint8_t i;

    if (out_marker == NULL)
    {
        return false;
    }

    memset(out_marker, 0, sizeof(*out_marker));
    logical_index = 0u;

    if (s_nav.launch.valid != false)
    {
        if (marker_index == logical_index)
        {
            return vario_nav_fill_trail_marker_from_point(&s_nav.launch,
                                                          (uint8_t)VARIO_NAV_TRAIL_MARKER_START,
                                                          out_marker);
        }
        ++logical_index;
    }

    for (i = 0u; i < VARIO_NAV_MAX_LANDABLES; ++i)
    {
        if (s_nav.landables[i].valid == false)
        {
            continue;
        }

        if (marker_index == logical_index)
        {
            return vario_nav_fill_trail_marker_from_point(&s_nav.landables[i],
                                                          (uint8_t)VARIO_NAV_TRAIL_MARKER_LANDABLE,
                                                          out_marker);
        }
        ++logical_index;
    }

    for (i = 0u; i < VARIO_NAV_MAX_USER_WAYPOINTS; ++i)
    {
        if (s_nav.waypoints[i].valid == false)
        {
            continue;
        }

        if (marker_index == logical_index)
        {
            return vario_nav_fill_trail_marker_from_point(&s_nav.waypoints[i],
                                                          (uint8_t)VARIO_NAV_TRAIL_MARKER_WAYPOINT,
                                                          out_marker);
        }
        ++logical_index;
    }

    return false;
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
            return "FIELD";
        case VARIO_NAV_SOURCE_USER_WP:
            return "POINT";
        case VARIO_NAV_SOURCE_MARK:
            return "MARK";
        case VARIO_NAV_SOURCE_NONE:
        default:
            return "OFF";
    }
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

static void vario_nav_show_simple_toast(const char *text, uint32_t now_ms)
{
    UI_Toast_Show(text, NULL, 0u, 0u, now_ms, 1200u);
}

static void vario_nav_activate_point_source(vario_nav_target_source_t source,
                                            uint8_t backing_index,
                                            uint32_t now_ms)
{
    char toast[48];
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

static void vario_nav_set_home_from_point(const vario_nav_point_t *point, uint32_t now_ms, bool persist_active_site)
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
    if (persist_active_site != false)
    {
        (void)vario_nav_save_active_site();
    }
    vario_nav_show_simple_toast("Home updated", now_ms);
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
    (void)vario_nav_save_active_site();
    vario_nav_show_simple_toast("Saved as field", now_ms);
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
        vario_nav_make_name(point.name, sizeof(point.name), "PT", s_nav.next_wp_index);
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
            (void)vario_nav_save_active_site();
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
            (void)vario_nav_save_active_site();
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
            vario_nav_set_home_from_point(&point, now_ms, true);
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

static void vario_nav_use_selected_site(uint8_t site_index, uint32_t now_ms)
{
    s_nav.active_site_index = site_index;
    (void)Vario_Persistence_SetActiveSiteIndex(site_index);
    (void)vario_nav_load_site_to_nav(site_index);
    vario_nav_sync_active_indexes();
    vario_nav_refresh_site_summaries();

    {
        char toast[48];
        snprintf(toast,
                 sizeof(toast),
                 "Site: %s",
                 s_nav.site_summaries[site_index].site_name);
        vario_nav_show_simple_toast(toast, now_ms);
    }
}

static void vario_nav_prepare_name_editor(uint8_t site_index)
{
    const char *src;
    uint8_t i;

    src = s_nav.site_summaries[site_index].site_name;
    memset(s_nav.name_edit_buffer, ' ', VARIO_NAV_NAME_EDITOR_MAX_CHARS);
    s_nav.name_edit_buffer[VARIO_NAV_NAME_EDITOR_MAX_CHARS] = '\0';
    for (i = 0u; (i < VARIO_NAV_NAME_EDITOR_MAX_CHARS) && (src[i] != '\0'); ++i)
    {
        s_nav.name_edit_buffer[i] = src[i];
    }
    s_nav.name_edit_active = true;
    s_nav.name_edit_pos = 0u;
    s_nav.open_page = VARIO_NAV_PAGE_SITE_NAME;
}

static const char *vario_nav_name_charset(void)
{
    return " ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789-_";
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
        case VARIO_NAV_PAGE_SITE_DETAIL:
            return 4u;
        case VARIO_NAV_PAGE_SITE_SETS:
            return Vario_Persistence_GetSiteCount();
        case VARIO_NAV_PAGE_NEARBY_LANDABLE:
            return vario_nav_count_valid_points(s_nav.landables, VARIO_NAV_MAX_LANDABLES);
        case VARIO_NAV_PAGE_USER_WAYPOINTS:
            return vario_nav_count_valid_points(s_nav.waypoints, VARIO_NAV_MAX_USER_WAYPOINTS);
        case VARIO_NAV_PAGE_SITE_NAME:
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
    uint8_t i;
    uint8_t seen;
    uint8_t order[VARIO_NAV_MAX_LANDABLES];
    uint8_t count;

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

    if (page == VARIO_NAV_PAGE_NEARBY_LANDABLE)
    {
        vario_nav_sort_landable_rows(rt, order, &count);
        if (row >= count)
        {
            return false;
        }
        *out_point = s_nav.landables[order[row]];
        if (out_backing_index != NULL)
        {
            *out_backing_index = order[row];
        }
        if (out_source != NULL)
        {
            *out_source = VARIO_NAV_SOURCE_LANDABLE;
        }
        return true;
    }

    if (page == VARIO_NAV_PAGE_USER_WAYPOINTS)
    {
        seen = 0u;
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
    }

    return false;
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
    s_nav.active_site_index = 0u;
    s_nav.detail_site_index = 0u;

    (void)Vario_Persistence_Init();
    s_nav.active_site_index = Vario_Persistence_GetActiveSiteIndex();
    vario_nav_refresh_site_summaries();
    (void)vario_nav_load_site_to_nav(s_nav.active_site_index);
    vario_nav_sync_active_indexes();
}

void Vario_Navigation_ResetFlightRam(void)
{
    memset(&s_nav.launch, 0, sizeof(s_nav.launch));
    memset(&s_nav.last_landing, 0, sizeof(s_nav.last_landing));
    memset(s_nav.marks, 0, sizeof(s_nav.marks));
    s_nav.open_page = VARIO_NAV_PAGE_NONE;
    s_nav.cursor = 0u;
    s_nav.name_edit_active = false;
    s_nav.name_edit_pos = 0u;

    if ((s_nav.active_source == VARIO_NAV_SOURCE_LAUNCH) ||
        (s_nav.active_source == VARIO_NAV_SOURCE_MARK))
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
    const vario_nav_point_t *point;

    if ((out_text == NULL) || (out_size == 0u))
    {
        return;
    }

    point = vario_nav_get_point_for_source(s_nav.active_source);
    if ((point != NULL) && (point->name[0] != '\0'))
    {
        snprintf(out_text, out_size, "Target: %s", point->name);
    }
    else
    {
        snprintf(out_text, out_size, "Target: %s", vario_nav_source_name(s_nav.active_source));
    }
}

void Vario_Navigation_OpenPage(vario_nav_page_t page)
{
    s_nav.open_page = page;
    s_nav.cursor = 0u;
    s_nav.name_edit_active = false;

    if (page == VARIO_NAV_PAGE_SITE_SETS)
    {
        vario_nav_refresh_site_summaries();
        s_nav.cursor = s_nav.active_site_index;
    }
    else if (page == VARIO_NAV_PAGE_SITE_DETAIL)
    {
        s_nav.cursor = 0u;
    }

    vario_nav_clamp_cursor_to_page();
}

void Vario_Navigation_ClosePage(void)
{
    if (s_nav.open_page == VARIO_NAV_PAGE_SITE_NAME)
    {
        s_nav.name_edit_active = false;
        s_nav.open_page = VARIO_NAV_PAGE_SITE_DETAIL;
        s_nav.cursor = 1u;
        return;
    }

    if (s_nav.open_page == VARIO_NAV_PAGE_SITE_DETAIL)
    {
        s_nav.open_page = VARIO_NAV_PAGE_SITE_SETS;
        s_nav.cursor = s_nav.detail_site_index;
        return;
    }

    s_nav.open_page = VARIO_NAV_PAGE_NONE;
    s_nav.cursor = 0u;
    s_nav.name_edit_active = false;
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

    if (s_nav.name_edit_active != false)
    {
        return;
    }

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
                    vario_nav_set_home_from_point(&s_nav.launch, now_ms, true);
                }
            }
            else if (s_nav.cursor == 3u)
            {
                memset(&s_nav.home, 0, sizeof(s_nav.home));
                if (s_nav.active_source == VARIO_NAV_SOURCE_HOME)
                {
                    s_nav.active_source = VARIO_NAV_SOURCE_NONE;
                }
                (void)vario_nav_save_active_site();
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
                    (void)vario_nav_save_active_site();
                    vario_nav_show_simple_toast("Launch saved as field", now_ms);
                }
            }
            else if (s_nav.cursor == 2u)
            {
                if (s_nav.launch.valid != false)
                {
                    vario_nav_set_home_from_point(&s_nav.launch, now_ms, true);
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

        case VARIO_NAV_PAGE_SITE_SETS:
            s_nav.detail_site_index = s_nav.cursor;
            s_nav.open_page = VARIO_NAV_PAGE_SITE_DETAIL;
            s_nav.cursor = 0u;
            break;

        case VARIO_NAV_PAGE_SITE_DETAIL:
            if (s_nav.cursor == 0u)
            {
                vario_nav_use_selected_site(s_nav.detail_site_index, now_ms);
                Vario_Navigation_ClosePage();
                Vario_Navigation_ClosePage();
                Vario_State_ReturnToMain();
            }
            else if (s_nav.cursor == 1u)
            {
                vario_nav_prepare_name_editor(s_nav.detail_site_index);
            }
            else if (s_nav.cursor == 2u)
            {
                vario_persist_site_t site;
                if ((rt != NULL) && (vario_nav_runtime_has_valid_gps(rt) != false))
                {
                    vario_nav_point_t home_point;
                    if (Vario_Persistence_LoadSite(s_nav.detail_site_index, &site) == false)
                    {
                        memset(&site, 0, sizeof(site));
                    }
                    if (vario_nav_capture_runtime_point(rt, &home_point, VARIO_NAV_TARGET_KIND_HOME, true, "HOME") != false)
                    {
                        vario_nav_copy_to_persist(&site.home, &home_point);
                        site.home.kind = VARIO_NAV_TARGET_KIND_HOME;
                        site.home.has_elevation = true;
                        if (s_nav.detail_site_index == s_nav.active_site_index)
                        {
                            vario_nav_set_home_from_point(&home_point, now_ms, true);
                        }
                        else if (Vario_Persistence_SaveSite(s_nav.detail_site_index, &site) != false)
                        {
                            vario_nav_refresh_site_summaries();
                            vario_nav_show_simple_toast("Home updated", now_ms);
                        }
                    }
                }
            }
            else if (s_nav.cursor == 3u)
            {
                UI_Confirm_Show("CLEAR SITE?",
                                "DELETE HOME, FIELDS AND POINTS",
                                "F1 = BACK",
                                "F2 = CLEAR",
                                "F3 = CANCEL",
                                (uint16_t)VARIO_NAV_CONFIRM_CLEAR_SITE,
                                now_ms);
            }
            break;

        case VARIO_NAV_PAGE_SITE_NAME:
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

        case VARIO_NAV_MENU_ACTION_SITE_SETS:
            Vario_Navigation_OpenPage(VARIO_NAV_PAGE_SITE_SETS);
            Vario_State_EnterSettings();
            break;

        case VARIO_NAV_MENU_ACTION_NONE:
        default:
            break;
    }
}

bool Vario_Navigation_HasConfirmHandler(uint16_t context_id)
{
    return ((context_id == (uint16_t)VARIO_NAV_CONFIRM_LANDING_SAVE) ||
            (context_id == (uint16_t)VARIO_NAV_CONFIRM_CLEAR_SITE)) ? true : false;
}

void Vario_Navigation_HandleConfirmChoice(uint16_t context_id, uint8_t button_id, uint32_t now_ms)
{
    if (context_id == (uint16_t)VARIO_NAV_CONFIRM_LANDING_SAVE)
    {
        if (button_id == 1u)
        {
            vario_nav_show_simple_toast("Landing noted", now_ms);
        }
        else if (button_id == 2u)
        {
            vario_nav_save_last_landing_as_field(now_ms);
        }
        else if (button_id == 3u)
        {
            if (s_nav.last_landing.valid != false)
            {
                vario_nav_set_home_from_point(&s_nav.last_landing, now_ms, true);
                s_nav.active_source = VARIO_NAV_SOURCE_HOME;
            }
        }
        return;
    }

    if (context_id == (uint16_t)VARIO_NAV_CONFIRM_CLEAR_SITE)
    {
        if (button_id == 2u)
        {
            (void)Vario_Persistence_ClearSite(s_nav.detail_site_index, true);
            vario_nav_refresh_site_summaries();
            if (s_nav.detail_site_index == s_nav.active_site_index)
            {
                (void)vario_nav_load_site_to_nav(s_nav.active_site_index);
                vario_nav_sync_active_indexes();
            }
            vario_nav_show_simple_toast("Site cleared", now_ms);
        }
        return;
    }
}

bool Vario_Navigation_IsNameEditActive(void)
{
    return s_nav.name_edit_active;
}

void Vario_Navigation_NameEdit_AdjustChar(int8_t direction)
{
    const char *charset;
    const char *found;
    char current;
    size_t len;
    int32_t index;

    if ((s_nav.name_edit_active == false) || (s_nav.name_edit_pos >= VARIO_NAV_NAME_EDITOR_MAX_CHARS))
    {
        return;
    }

    charset = vario_nav_name_charset();
    len = strlen(charset);
    current = s_nav.name_edit_buffer[s_nav.name_edit_pos];
    found = strchr(charset, current);
    index = (found != NULL) ? (int32_t)(found - charset) : 0;
    index += (direction >= 0) ? 1 : -1;
    if (index < 0)
    {
        index = (int32_t)len - 1;
    }
    else if (index >= (int32_t)len)
    {
        index = 0;
    }
    s_nav.name_edit_buffer[s_nav.name_edit_pos] = charset[index];
}

void Vario_Navigation_NameEdit_MoveCursor(int8_t direction)
{
    int16_t next;

    if (s_nav.name_edit_active == false)
    {
        return;
    }

    next = (int16_t)s_nav.name_edit_pos + ((direction >= 0) ? 1 : -1);
    if (next < 0)
    {
        next = VARIO_NAV_NAME_EDITOR_MAX_CHARS - 1;
    }
    else if (next >= (int16_t)VARIO_NAV_NAME_EDITOR_MAX_CHARS)
    {
        next = 0;
    }
    s_nav.name_edit_pos = (uint8_t)next;
}

void Vario_Navigation_NameEdit_Save(uint32_t now_ms)
{
    char trimmed[VARIO_NAV_NAME_MAX];
    size_t len;

    if (s_nav.name_edit_active == false)
    {
        return;
    }

    memset(trimmed, 0, sizeof(trimmed));
    vario_nav_copy_name(trimmed, sizeof(trimmed), s_nav.name_edit_buffer);
    len = strlen(trimmed);
    while ((len > 0u) && (trimmed[len - 1u] == ' '))
    {
        trimmed[len - 1u] = '\0';
        --len;
    }
    if (trimmed[0] == '\0')
    {
        snprintf(trimmed, sizeof(trimmed), "SITE %02u", (unsigned)(s_nav.detail_site_index + 1u));
    }

    if (Vario_Persistence_RenameSite(s_nav.detail_site_index, trimmed) != false)
    {
        vario_nav_refresh_site_summaries();
        vario_nav_show_simple_toast("Site renamed", now_ms);
    }
    else
    {
        vario_nav_show_simple_toast("Name not saved", now_ms);
    }

    s_nav.name_edit_active = false;
    s_nav.open_page = VARIO_NAV_PAGE_SITE_DETAIL;
    s_nav.cursor = 1u;
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
    char name_buf[VARIO_NAV_NAME_MAX];
    const char *cardinal;
    int16_t row_top;
    int16_t row_h;
    int16_t arrow_cx;
    int16_t arrow_cy;
    int16_t text_x;
    int16_t max_left_w;
    uint8_t trim_len;

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
    max_left_w = (int16_t)((v->x + v->w - 8) - text_x - 6 - u8g2_GetStrWidth(u8g2, right_text));
    if (max_left_w < 24)
    {
        max_left_w = 24;
    }

    cardinal = vario_nav_cardinal16(sol->bearing_deg);
    strncpy(name_buf, point->name, sizeof(name_buf) - 1u);
    name_buf[sizeof(name_buf) - 1u] = '\0';
    snprintf(left_text, sizeof(left_text), "%s %s", name_buf, cardinal);

    if (u8g2_GetStrWidth(u8g2, left_text) > (uint16_t)max_left_w)
    {
        trim_len = (uint8_t)strlen(name_buf);
        while ((trim_len > 3u) && (u8g2_GetStrWidth(u8g2, left_text) > (uint16_t)max_left_w))
        {
            --trim_len;
            name_buf[trim_len] = '\0';
            snprintf(left_text, sizeof(left_text), "%s~ %s", name_buf, cardinal);
        }
    }

    u8g2_DrawStr(u8g2, (u8g2_uint_t)text_x, (u8g2_uint_t)y_baseline, left_text);
    Vario_Display_DrawTextRight(u8g2, (int16_t)(v->x + v->w - 8), y_baseline, right_text);
    u8g2_SetDrawColor(u8g2, 1);
}

static int16_t vario_nav_get_row_baseline(const vario_viewport_t *v, uint8_t row_index)
{
    if (v == NULL)
    {
        return (int16_t)(VARIO_NAV_FIRST_ROW_BASELINE_Y + ((int16_t)row_index * VARIO_NAV_ROW_STEP_Y));
    }

    return (int16_t)(v->y + VARIO_NAV_FIRST_ROW_BASELINE_Y + ((int16_t)row_index * VARIO_NAV_ROW_STEP_Y));
}

static int16_t vario_nav_get_empty_state_baseline(const vario_viewport_t *v)
{
    if (v == NULL)
    {
        return 64;
    }

    return (int16_t)(v->y + (v->h / 2));
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
            snprintf(title, title_size, "HOME");
            break;
        case VARIO_NAV_PAGE_LAUNCH:
            snprintf(title, title_size, "LAUNCH");
            break;
        case VARIO_NAV_PAGE_NEARBY_LANDABLE:
            snprintf(title, title_size, "NEARBY LANDABLE");
            break;
        case VARIO_NAV_PAGE_USER_WAYPOINTS:
            snprintf(title, title_size, "WAYPOINTS");
            break;
        case VARIO_NAV_PAGE_MARK_HERE:
            snprintf(title, title_size, "MARK HERE");
            break;
        case VARIO_NAV_PAGE_CLEAR_TARGET:
            snprintf(title, title_size, "CLEAR TARGET");
            break;
        case VARIO_NAV_PAGE_SITE_SETS:
            snprintf(title, title_size, "SITE SETS");
            break;
        case VARIO_NAV_PAGE_SITE_DETAIL:
            snprintf(title, title_size, "%s", s_nav.site_summaries[s_nav.detail_site_index].site_name);
            break;
        case VARIO_NAV_PAGE_SITE_NAME:
            snprintf(title, title_size, "SITE NAME");
            break;
        case VARIO_NAV_PAGE_NONE:
        default:
            snprintf(title, title_size, "NAV");
            break;
    }
}

static void vario_nav_format_site_counts(uint8_t site_index, char *out_text, size_t out_size)
{
    const vario_persist_site_summary_t *sum;

    if ((out_text == NULL) || (out_size == 0u))
    {
        return;
    }

    sum = &s_nav.site_summaries[site_index];
    snprintf(out_text,
             out_size,
             "H%u L%u P%u",
             (sum->home_valid != false) ? 1u : 0u,
             (unsigned)sum->landing_count,
             (unsigned)sum->waypoint_count);
}

static void vario_nav_draw_site_name_editor(u8g2_t *u8g2, const vario_viewport_t *v)
{
    int16_t box_x;
    int16_t box_y;
    int16_t box_w;
    int16_t box_h;
    int16_t cx;
    uint8_t i;
    char subtitle[24];
    char one[2];

    snprintf(subtitle, sizeof(subtitle), "SITE %02u", (unsigned)(s_nav.detail_site_index + 1u));
    Vario_Display_DrawPageTitle(u8g2, v, "SITE NAME", subtitle);

    box_w = (int16_t)(v->w - 12);
    box_h = 34;
    box_x = (int16_t)(v->x + 6);
    box_y = (int16_t)(v->y + 28);
    cx = (int16_t)(box_x + (box_w / 2));

    u8g2_DrawFrame(u8g2, (u8g2_uint_t)box_x, (u8g2_uint_t)box_y, (u8g2_uint_t)box_w, (u8g2_uint_t)box_h);
    Vario_Display_DrawTextCentered(u8g2, cx, (int16_t)(box_y + 10), "F2/F3 CHAR   F4/F5 MOVE");

    one[1] = '\0';
    for (i = 0u; i < VARIO_NAV_NAME_EDITOR_MAX_CHARS; ++i)
    {
        int16_t cell_x;
        cell_x = (int16_t)(box_x + 8 + ((int16_t)i * 19));
        if (i == s_nav.name_edit_pos)
        {
            u8g2_DrawBox(u8g2, (u8g2_uint_t)cell_x, (u8g2_uint_t)(box_y + 14), (u8g2_uint_t)16, (u8g2_uint_t)12);
            u8g2_SetDrawColor(u8g2, 0);
        }
        else
        {
            u8g2_DrawFrame(u8g2, (u8g2_uint_t)cell_x, (u8g2_uint_t)(box_y + 14), (u8g2_uint_t)16, (u8g2_uint_t)12);
        }
        one[0] = s_nav.name_edit_buffer[i];
        Vario_Display_DrawTextCentered(u8g2, (int16_t)(cell_x + 8), (int16_t)(box_y + 23), one);
        u8g2_SetDrawColor(u8g2, 1);
    }

    Vario_Display_DrawTextCentered(u8g2, cx, (int16_t)(box_y + 48), "PRESS F6 TO SAVE");
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
    else if (s_nav.open_page == VARIO_NAV_PAGE_SITE_SETS)
    {
        snprintf(subtitle,
                 sizeof(subtitle),
                 "ACTIVE %02u %s",
                 (unsigned)(s_nav.active_site_index + 1u),
                 s_nav.site_summaries[s_nav.active_site_index].site_name);
    }
    else if (s_nav.open_page == VARIO_NAV_PAGE_SITE_DETAIL)
    {
        vario_nav_format_site_counts(s_nav.detail_site_index, subtitle, sizeof(subtitle));
    }
    else if (s_nav.open_page == VARIO_NAV_PAGE_SITE_NAME)
    {
        snprintf(subtitle, sizeof(subtitle), "EDIT NAME");
    }

    if (s_nav.open_page == VARIO_NAV_PAGE_SITE_NAME)
    {
        if (UI_Confirm_IsVisible() != false)
        {
            Vario_Display_DrawPageTitle(u8g2, v, title, subtitle);
            return;
        }

        vario_nav_draw_site_name_editor(u8g2, v);
        return;
    }

    Vario_Display_DrawPageTitle(u8g2, v, title, subtitle);

    /* ------------------------------------------------------------------ */
    /* NAV 계열 페이지 위에 confirm overlay 가 뜰 때,                       */
    /* 아래쪽 리스트까지 계속 그리면 글자가 중앙 confirm box 주변에        */
    /* 겹쳐 보여 시각적으로 지저분해진다.                                   */
    /*                                                                    */
    /* confirm 이 active 인 동안에는 NAV page 의 title/subtitle 만 남기고, */
    /* 본문 row 는 그리지 않는다.                                          */
    /*                                                                    */
    /* 이렇게 하면 confirm box 는 그대로 UI_ENGINE overlay 로 그려지고,    */
    /* NAV page 는 상단 문맥만 유지한 채 본문 겹침이 제거된다.              */
    /* ------------------------------------------------------------------ */
    if (UI_Confirm_IsVisible() != false)
    {
        return;
    }

    count = vario_nav_page_item_count(s_nav.open_page);
    vario_nav_get_list_window(count, s_nav.cursor, &start, &rows);

    if (s_nav.open_page == VARIO_NAV_PAGE_HOME)
    {
        Vario_Display_DrawMenuRow(u8g2, v, vario_nav_get_row_baseline(v, 0u), (s_nav.cursor == 0u), "Activate", (s_nav.home.valid != false) ? "HOME" : "--");
        Vario_Display_DrawMenuRow(u8g2, v, vario_nav_get_row_baseline(v, 1u), (s_nav.cursor == 1u), "Set Home Here", "GPS");
        Vario_Display_DrawMenuRow(u8g2, v, vario_nav_get_row_baseline(v, 2u), (s_nav.cursor == 2u), "Use Launch", (s_nav.launch.valid != false) ? "SET" : "--");
        Vario_Display_DrawMenuRow(u8g2, v, vario_nav_get_row_baseline(v, 3u), (s_nav.cursor == 3u), "Clear Home", "");
        return;
    }

    if (s_nav.open_page == VARIO_NAV_PAGE_LAUNCH)
    {
        Vario_Display_DrawMenuRow(u8g2, v, vario_nav_get_row_baseline(v, 0u), (s_nav.cursor == 0u), "Activate", (s_nav.launch.valid != false) ? "LAUNCH" : "--");
        Vario_Display_DrawMenuRow(u8g2, v, vario_nav_get_row_baseline(v, 1u), (s_nav.cursor == 1u), "Save as Field", "SAVE");
        Vario_Display_DrawMenuRow(u8g2, v, vario_nav_get_row_baseline(v, 2u), (s_nav.cursor == 2u), "Set as Home", "HOME");
        return;
    }

    if (s_nav.open_page == VARIO_NAV_PAGE_MARK_HERE)
    {
        Vario_Display_DrawMenuRow(u8g2, v, vario_nav_get_row_baseline(v, 0u), (s_nav.cursor == 0u), "Save as Point", "SAVE");
        Vario_Display_DrawMenuRow(u8g2, v, vario_nav_get_row_baseline(v, 1u), (s_nav.cursor == 1u), "Save as Field", "SAVE");
        Vario_Display_DrawMenuRow(u8g2, v, vario_nav_get_row_baseline(v, 2u), (s_nav.cursor == 2u), "Set Home Here", "HOME");
        Vario_Display_DrawMenuRow(u8g2, v, vario_nav_get_row_baseline(v, 3u), (s_nav.cursor == 3u), "Use as Target", "MARK");
        return;
    }

    if (s_nav.open_page == VARIO_NAV_PAGE_CLEAR_TARGET)
    {
        Vario_Display_DrawMenuRow(u8g2, v, vario_nav_get_row_baseline(v, 0u), true, "Clear Active Target", "CLEAR");
        return;
    }

    if (s_nav.open_page == VARIO_NAV_PAGE_SITE_DETAIL)
    {
        Vario_Display_DrawMenuRow(u8g2, v, vario_nav_get_row_baseline(v, 0u), (s_nav.cursor == 0u), "Use This Site", "OPEN");
        Vario_Display_DrawMenuRow(u8g2, v, vario_nav_get_row_baseline(v, 1u), (s_nav.cursor == 1u), "Rename Site", "EDIT");
        Vario_Display_DrawMenuRow(u8g2, v, vario_nav_get_row_baseline(v, 2u), (s_nav.cursor == 2u), "Set Home Here", "GPS");
        Vario_Display_DrawMenuRow(u8g2, v, vario_nav_get_row_baseline(v, 3u), (s_nav.cursor == 3u), "Clear Site", "CLEAR");
        return;
    }

    if (s_nav.open_page == VARIO_NAV_PAGE_SITE_SETS)
    {
        for (row = 0u; row < rows; ++row)
        {
            uint8_t site_index;
            char counts[20];
            site_index = (uint8_t)(start + row);
            vario_nav_format_site_counts(site_index, counts, sizeof(counts));
            Vario_Display_DrawMenuRow(u8g2,
                                      v,
                                      vario_nav_get_row_baseline(v, row),
                                      (site_index == s_nav.cursor),
                                      s_nav.site_summaries[site_index].site_name,
                                      counts);
        }
        return;
    }

    y = vario_nav_get_row_baseline(v, 0u);
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
        y = (int16_t)(y + VARIO_NAV_ROW_STEP_Y);
    }

    if ((count == 0u) && (s_nav.open_page == VARIO_NAV_PAGE_NEARBY_LANDABLE))
    {
        Vario_Display_DrawTextCentered(u8g2, (int16_t)(v->x + (v->w / 2)), vario_nav_get_empty_state_baseline(v), "NO LANDABLES SAVED");
    }
    else if ((count == 0u) && (s_nav.open_page == VARIO_NAV_PAGE_USER_WAYPOINTS))
    {
        Vario_Display_DrawTextCentered(u8g2, (int16_t)(v->x + (v->w / 2)), vario_nav_get_empty_state_baseline(v), "NO WAYPOINTS SAVED");
    }
}
