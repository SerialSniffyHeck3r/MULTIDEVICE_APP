#ifndef VARIO_NAVIGATION_H
#define VARIO_NAVIGATION_H

#include "u8g2.h"
#include "Vario_State.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef VARIO_NAV_NAME_MAX
#define VARIO_NAV_NAME_MAX 16u
#endif

#ifndef VARIO_NAV_RUNTIME_NAME_MAX
#define VARIO_NAV_RUNTIME_NAME_MAX 12u
#endif

typedef enum
{
    VARIO_NAV_TARGET_KIND_NONE = 0u,
    VARIO_NAV_TARGET_KIND_HOME,
    VARIO_NAV_TARGET_KIND_LAUNCH,
    VARIO_NAV_TARGET_KIND_LANDABLE,
    VARIO_NAV_TARGET_KIND_USER_WP,
    VARIO_NAV_TARGET_KIND_MARK,
    VARIO_NAV_TARGET_KIND_LANDING
} vario_nav_target_kind_t;

typedef enum
{
    VARIO_NAV_SOURCE_NONE = 0u,
    VARIO_NAV_SOURCE_HOME,
    VARIO_NAV_SOURCE_LAUNCH,
    VARIO_NAV_SOURCE_LANDABLE,
    VARIO_NAV_SOURCE_USER_WP,
    VARIO_NAV_SOURCE_MARK
} vario_nav_target_source_t;

typedef enum
{
    VARIO_NAV_PAGE_NONE = 0u,
    VARIO_NAV_PAGE_HOME,
    VARIO_NAV_PAGE_LAUNCH,
    VARIO_NAV_PAGE_NEARBY_LANDABLE,
    VARIO_NAV_PAGE_USER_WAYPOINTS,
    VARIO_NAV_PAGE_MARK_HERE,
    VARIO_NAV_PAGE_CLEAR_TARGET,
    VARIO_NAV_PAGE_SITE_SETS,
    VARIO_NAV_PAGE_SITE_DETAIL,
    VARIO_NAV_PAGE_SITE_NAME
} vario_nav_page_t;

typedef enum
{
    VARIO_NAV_MENU_ACTION_NONE = 0u,
    VARIO_NAV_MENU_ACTION_HOME,
    VARIO_NAV_MENU_ACTION_LAUNCH,
    VARIO_NAV_MENU_ACTION_NEARBY_LANDABLE,
    VARIO_NAV_MENU_ACTION_USER_WAYPOINTS,
    VARIO_NAV_MENU_ACTION_MARK_HERE,
    VARIO_NAV_MENU_ACTION_CLEAR_TARGET,
    VARIO_NAV_MENU_ACTION_SITE_SETS
} vario_nav_menu_action_t;

typedef enum
{
    VARIO_NAV_CONFIRM_NONE = 0u,
    VARIO_NAV_CONFIRM_LANDING_SAVE = 1u,
    VARIO_NAV_CONFIRM_CLEAR_SITE = 2u
} vario_nav_confirm_context_t;

typedef struct
{
    bool valid;
    bool has_elevation;
    uint32_t id;
    vario_nav_target_kind_t kind;
    int32_t lat_e7;
    int32_t lon_e7;
    float altitude_m;
    char name[VARIO_NAV_NAME_MAX];
} vario_nav_point_t;

typedef struct
{
    bool valid;
    bool has_elevation;
    uint32_t id;
    uint8_t kind;
    int32_t lat_e7;
    int32_t lon_e7;
    float altitude_m;
    char name[VARIO_NAV_RUNTIME_NAME_MAX];
} vario_nav_active_target_t;

typedef struct
{
    bool valid;
    bool heading_valid;
    float distance_m;
    float bearing_deg;
    float relative_bearing_deg;
    bool final_glide_valid;
    float required_glide_ratio;
    float arrival_height_m;
} vario_nav_solution_t;

typedef enum
{
    VARIO_NAV_TRAIL_MARKER_START = 0u,
    VARIO_NAV_TRAIL_MARKER_LANDABLE,
    VARIO_NAV_TRAIL_MARKER_WAYPOINT
} vario_nav_trail_marker_kind_t;

typedef struct
{
    bool valid;
    uint8_t kind;
    int32_t lat_e7;
    int32_t lon_e7;
    char name[VARIO_NAV_RUNTIME_NAME_MAX];
} vario_nav_trail_marker_t;

void Vario_Navigation_Init(void);
void Vario_Navigation_ResetFlightRam(void);

void Vario_Navigation_OnTakeoff(const vario_runtime_t *rt);
void Vario_Navigation_OnLanding(const vario_runtime_t *rt, uint32_t now_ms);

bool Vario_Navigation_GetActiveTarget(vario_nav_active_target_t *out_target);
vario_nav_target_source_t Vario_Navigation_GetActiveSource(void);
void Vario_Navigation_CycleTargetSource(void);
void Vario_Navigation_ClearTarget(void);
void Vario_Navigation_FormatActiveSourceToast(char *out_text, size_t out_size);
const char *Vario_Navigation_GetShortSourceLabel(uint8_t target_kind);
uint8_t Vario_Navigation_GetTrailMarkerCount(void);
bool Vario_Navigation_GetTrailMarker(uint8_t marker_index,
                                     vario_nav_trail_marker_t *out_marker);

bool Vario_Navigation_ComputeSolutionForTarget(const vario_runtime_t *rt,
                                               const vario_nav_active_target_t *target,
                                               vario_nav_solution_t *out_solution);

void Vario_Navigation_OpenPage(vario_nav_page_t page);
void Vario_Navigation_ClosePage(void);
bool Vario_Navigation_IsPageOpen(void);
vario_nav_page_t Vario_Navigation_GetOpenPage(void);
void Vario_Navigation_MoveCursor(int8_t direction);
void Vario_Navigation_ActivateSelected(uint32_t now_ms);
void Vario_Navigation_RenderSettingPage(u8g2_t *u8g2);
void Vario_Navigation_HandleMenuAction(uint16_t action_id, uint32_t now_ms);

bool Vario_Navigation_HasConfirmHandler(uint16_t context_id);
void Vario_Navigation_HandleConfirmChoice(uint16_t context_id, uint8_t button_id, uint32_t now_ms);

bool Vario_Navigation_IsNameEditActive(void);
void Vario_Navigation_NameEdit_AdjustChar(int8_t direction);
void Vario_Navigation_NameEdit_MoveCursor(int8_t direction);
void Vario_Navigation_NameEdit_Save(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* VARIO_NAVIGATION_H */
