#ifndef VARIO_PERSISTENCE_H
#define VARIO_PERSISTENCE_H

#include "Vario_Settings.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef VARIO_PERSISTENCE_MAX_SITES
#define VARIO_PERSISTENCE_MAX_SITES 30u
#endif

#ifndef VARIO_PERSISTENCE_MAX_FIELDS
#define VARIO_PERSISTENCE_MAX_FIELDS 16u
#endif

#ifndef VARIO_PERSISTENCE_MAX_WAYPOINTS
#define VARIO_PERSISTENCE_MAX_WAYPOINTS 16u
#endif

#ifndef VARIO_PERSISTENCE_NAME_MAX
#define VARIO_PERSISTENCE_NAME_MAX 16u
#endif

typedef struct
{
    bool     valid;
    bool     has_elevation;
    uint32_t id;
    uint8_t  kind;
    int32_t  lat_e7;
    int32_t  lon_e7;
    float    altitude_m;
    char     name[VARIO_PERSISTENCE_NAME_MAX];
} vario_persist_point_t;

typedef struct
{
    bool                  stored;
    char                  site_name[VARIO_PERSISTENCE_NAME_MAX];
    uint32_t              next_id;
    uint8_t               next_field_serial;
    uint8_t               next_waypoint_serial;
    vario_persist_point_t home;
    vario_persist_point_t landings[VARIO_PERSISTENCE_MAX_FIELDS];
    vario_persist_point_t waypoints[VARIO_PERSISTENCE_MAX_WAYPOINTS];
} vario_persist_site_t;

typedef struct
{
    bool     stored;
    char     site_name[VARIO_PERSISTENCE_NAME_MAX];
    bool     home_valid;
    uint8_t  landing_count;
    uint8_t  waypoint_count;
} vario_persist_site_summary_t;

bool Vario_Persistence_Init(void);
bool Vario_Persistence_IsReady(void);
const char *Vario_Persistence_GetLastErrorText(void);

bool Vario_Persistence_LoadSettings(vario_settings_t *inout_settings);
bool Vario_Persistence_SaveSettings(const vario_settings_t *settings);

uint8_t Vario_Persistence_GetSiteCount(void);
uint8_t Vario_Persistence_GetActiveSiteIndex(void);
bool Vario_Persistence_SetActiveSiteIndex(uint8_t index);
bool Vario_Persistence_GetSiteSummary(uint8_t index, vario_persist_site_summary_t *out_summary);
bool Vario_Persistence_LoadSite(uint8_t index, vario_persist_site_t *out_site);
bool Vario_Persistence_SaveSite(uint8_t index, const vario_persist_site_t *site);
bool Vario_Persistence_RenameSite(uint8_t index, const char *name);
bool Vario_Persistence_ClearSite(uint8_t index, bool keep_name);

#ifdef __cplusplus
}
#endif

#endif /* VARIO_PERSISTENCE_H */
