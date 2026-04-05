#include "Vario_Persistence.h"
#include "APP_MEMORY_SECTIONS.h"

#include "APP_FlashStore.h"

#include <stdio.h>
#include <string.h>

#if defined(__GNUC__)
#define VARIO_PERSIST_PACKED __attribute__((packed))
#else
#define VARIO_PERSIST_PACKED
#endif

#ifndef VARIO_PERSIST_LAYOUT_VERSION
#define VARIO_PERSIST_LAYOUT_VERSION 0x00010002u
#endif

#ifndef VARIO_PERSIST_SETTINGS_SCHEMA_ID
#define VARIO_PERSIST_SETTINGS_SCHEMA_ID 0x00010001u
#endif

#ifndef VARIO_PERSIST_META_SCHEMA_ID
#define VARIO_PERSIST_META_SCHEMA_ID 0x00010001u
#endif

#ifndef VARIO_PERSIST_SITE_SCHEMA_ID
#define VARIO_PERSIST_SITE_SCHEMA_ID 0x00010002u
#endif

#ifndef VARIO_PERSIST_SECTOR_SUPER_A
#define VARIO_PERSIST_SECTOR_SUPER_A 0u
#endif
#ifndef VARIO_PERSIST_SECTOR_SUPER_B
#define VARIO_PERSIST_SECTOR_SUPER_B 1u
#endif
#ifndef VARIO_PERSIST_SECTOR_SETTINGS_A
#define VARIO_PERSIST_SECTOR_SETTINGS_A 2u
#endif
#ifndef VARIO_PERSIST_SECTOR_SETTINGS_B
#define VARIO_PERSIST_SECTOR_SETTINGS_B 3u
#endif
#ifndef VARIO_PERSIST_SECTOR_META_A
#define VARIO_PERSIST_SECTOR_META_A 4u
#endif
#ifndef VARIO_PERSIST_SECTOR_META_B
#define VARIO_PERSIST_SECTOR_META_B 5u
#endif
#ifndef VARIO_PERSIST_SECTOR_SITE_BASE
#define VARIO_PERSIST_SECTOR_SITE_BASE 6u
#endif
#define VARIO_PERSIST_MANAGED_SECTOR_COUNT (VARIO_PERSIST_SECTOR_SITE_BASE + (VARIO_PERSISTENCE_MAX_SITES * 2u))
#define VARIO_PERSIST_MANAGED_SPAN_BYTES   (VARIO_PERSIST_MANAGED_SECTOR_COUNT * APP_FLASHSTORE_SECTOR_SIZE)

typedef struct VARIO_PERSIST_PACKED
{
    uint8_t  valid;
    uint8_t  has_elevation;
    uint8_t  kind;
    uint8_t  reserved0;
    uint32_t id;
    int32_t  lat_e7;
    int32_t  lon_e7;
    int32_t  altitude_cm;
    char     name[VARIO_PERSISTENCE_NAME_MAX];
    uint32_t reserved1;
} vario_persist_point_disk_t;

typedef struct VARIO_PERSIST_PACKED
{
    uint32_t format_version;
    uint8_t  active_site_index;
    uint8_t  reserved0[27];
} vario_persist_meta_disk_t;

typedef struct VARIO_PERSIST_PACKED
{
    uint32_t                  format_version;
    char                      site_name[VARIO_PERSISTENCE_NAME_MAX];
    uint32_t                  next_id;
    uint8_t                   next_field_serial;
    uint8_t                   next_waypoint_serial;
    uint8_t                   stored;
    uint8_t                   reserved0[9];
    vario_persist_point_disk_t home;
    vario_persist_point_disk_t landings[VARIO_PERSISTENCE_MAX_FIELDS];
    vario_persist_point_disk_t waypoints[VARIO_PERSISTENCE_MAX_WAYPOINTS];
    uint8_t                   reserved1[256];
} vario_persist_site_disk_t;

typedef struct
{
    bool                         initialized;
    bool                         ready;
    uint32_t                     settings_sequence;
    uint32_t                     meta_sequence;
    uint32_t                     site_sequences[VARIO_PERSISTENCE_MAX_SITES];
    uint8_t                      active_site_index;
    vario_persist_site_summary_t summaries[VARIO_PERSISTENCE_MAX_SITES];
    app_flashstore_result_t      last_error;
} vario_persistence_runtime_t;

static vario_persistence_runtime_t s_persist APP_CCMRAM_BSS;

static uint32_t vario_persist_sector_address(uint32_t sector_index)
{
    return sector_index * APP_FLASHSTORE_SECTOR_SIZE;
}

static void vario_persist_build_default_site_name(uint8_t index, char *out_name, size_t out_size)
{
    if ((out_name == NULL) || (out_size == 0u))
    {
        return;
    }
    snprintf(out_name, out_size, "SITE %02u", (unsigned)(index + 1u));
}

static void vario_persist_copy_name(char *dst, size_t dst_size, const char *src, const char *fallback)
{
    const char *source;
    size_t i;

    if ((dst == NULL) || (dst_size == 0u))
    {
        return;
    }

    source = src;
    if ((source == NULL) || (source[0] == '\0'))
    {
        source = fallback;
    }
    if (source == NULL)
    {
        dst[0] = '\0';
        return;
    }

    for (i = 0u; (i + 1u) < dst_size; ++i)
    {
        char c;
        c = source[i];
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

static void vario_persist_fill_default_site(uint8_t index, vario_persist_site_t *out_site)
{
    if (out_site == NULL)
    {
        return;
    }

    memset(out_site, 0, sizeof(*out_site));
    vario_persist_build_default_site_name(index, out_site->site_name, sizeof(out_site->site_name));
    out_site->next_id = 1u;
    out_site->next_field_serial = 1u;
    out_site->next_waypoint_serial = 1u;
}

static uint8_t vario_persist_count_valid_points(const vario_persist_point_t *points, uint8_t count)
{
    uint8_t i;
    uint8_t n;

    n = 0u;
    for (i = 0u; i < count; ++i)
    {
        if (points[i].valid != false)
        {
            ++n;
        }
    }
    return n;
}

static void vario_persist_update_summary_from_site(uint8_t index,
                                                   const vario_persist_site_t *site,
                                                   vario_persist_site_summary_t *out_summary)
{
    char default_name[VARIO_PERSISTENCE_NAME_MAX];

    if ((site == NULL) || (out_summary == NULL))
    {
        return;
    }

    vario_persist_build_default_site_name(index, default_name, sizeof(default_name));
    memset(out_summary, 0, sizeof(*out_summary));
    out_summary->stored = site->stored;
    vario_persist_copy_name(out_summary->site_name, sizeof(out_summary->site_name), site->site_name, default_name);
    out_summary->home_valid = site->home.valid;
    out_summary->landing_count = vario_persist_count_valid_points(site->landings, VARIO_PERSISTENCE_MAX_FIELDS);
    out_summary->waypoint_count = vario_persist_count_valid_points(site->waypoints, VARIO_PERSISTENCE_MAX_WAYPOINTS);
}

static void vario_persist_point_to_disk(const vario_persist_point_t *src,
                                        vario_persist_point_disk_t *dst)
{
    if ((src == NULL) || (dst == NULL))
    {
        return;
    }

    memset(dst, 0, sizeof(*dst));
    dst->valid = (src->valid != false) ? 1u : 0u;
    dst->has_elevation = (src->has_elevation != false) ? 1u : 0u;
    dst->kind = src->kind;
    dst->id = src->id;
    dst->lat_e7 = src->lat_e7;
    dst->lon_e7 = src->lon_e7;
    dst->altitude_cm = (int32_t)((src->altitude_m >= 0.0f) ? (src->altitude_m * 100.0f + 0.5f) : (src->altitude_m * 100.0f - 0.5f));
    vario_persist_copy_name(dst->name, sizeof(dst->name), src->name, NULL);
}

static void vario_persist_point_from_disk(const vario_persist_point_disk_t *src,
                                          vario_persist_point_t *dst)
{
    if ((src == NULL) || (dst == NULL))
    {
        return;
    }

    memset(dst, 0, sizeof(*dst));
    dst->valid = (src->valid != 0u) ? true : false;
    dst->has_elevation = (src->has_elevation != 0u) ? true : false;
    dst->kind = src->kind;
    dst->id = src->id;
    dst->lat_e7 = src->lat_e7;
    dst->lon_e7 = src->lon_e7;
    dst->altitude_m = ((float)src->altitude_cm) * 0.01f;
    vario_persist_copy_name(dst->name, sizeof(dst->name), src->name, NULL);
}

static app_flashstore_region_t vario_persist_make_settings_region(void)
{
    app_flashstore_region_t region;
    memset(&region, 0, sizeof(region));
    region.region_id = 0x00001001u;
    region.schema_id = VARIO_PERSIST_SETTINGS_SCHEMA_ID;
    region.copy0_address = vario_persist_sector_address(VARIO_PERSIST_SECTOR_SETTINGS_A);
    region.copy1_address = vario_persist_sector_address(VARIO_PERSIST_SECTOR_SETTINGS_B);
    region.payload_capacity_bytes = sizeof(vario_settings_t);
    return region;
}

static app_flashstore_region_t vario_persist_make_meta_region(void)
{
    app_flashstore_region_t region;
    memset(&region, 0, sizeof(region));
    region.region_id = 0x00001002u;
    region.schema_id = VARIO_PERSIST_META_SCHEMA_ID;
    region.copy0_address = vario_persist_sector_address(VARIO_PERSIST_SECTOR_META_A);
    region.copy1_address = vario_persist_sector_address(VARIO_PERSIST_SECTOR_META_B);
    region.payload_capacity_bytes = sizeof(vario_persist_meta_disk_t);
    return region;
}

static app_flashstore_region_t vario_persist_make_site_region(uint8_t index)
{
    app_flashstore_region_t region;
    uint32_t sector_a;
    uint32_t sector_b;

    memset(&region, 0, sizeof(region));
    sector_a = VARIO_PERSIST_SECTOR_SITE_BASE + ((uint32_t)index * 2u);
    sector_b = sector_a + 1u;
    region.region_id = 0x00002000u + (uint32_t)index;
    region.schema_id = VARIO_PERSIST_SITE_SCHEMA_ID;
    region.copy0_address = vario_persist_sector_address(sector_a);
    region.copy1_address = vario_persist_sector_address(sector_b);
    region.payload_capacity_bytes = sizeof(vario_persist_site_disk_t);
    return region;
}

static bool vario_persist_save_meta(void)
{
    app_flashstore_region_t region;
    vario_persist_meta_disk_t meta;

    if (s_persist.ready == false)
    {
        return false;
    }

    memset(&meta, 0, sizeof(meta));
    meta.format_version = 1u;
    meta.active_site_index = s_persist.active_site_index;
    region = vario_persist_make_meta_region();
    if (APP_FlashStore_Save(&region,
                            &meta,
                            sizeof(meta),
                            &s_persist.meta_sequence) != APP_FLASHSTORE_RESULT_OK)
    {
        s_persist.last_error = APP_FlashStore_GetLastResult();
        return false;
    }

    s_persist.last_error = APP_FLASHSTORE_RESULT_OK;
    return true;
}

static bool vario_persist_load_meta(void)
{
    app_flashstore_region_t region;
    vario_persist_meta_disk_t meta;
    uint32_t size;
    uint32_t seq;

    memset(&meta, 0, sizeof(meta));
    region = vario_persist_make_meta_region();
    if (APP_FlashStore_Load(&region, &meta, sizeof(meta), &size, &seq) != APP_FLASHSTORE_RESULT_OK)
    {
        s_persist.active_site_index = 0u;
        s_persist.meta_sequence = 0u;
        s_persist.last_error = APP_FlashStore_GetLastResult();
        return false;
    }

    if ((size != sizeof(meta)) || (meta.format_version != 1u) || (meta.active_site_index >= VARIO_PERSISTENCE_MAX_SITES))
    {
        s_persist.active_site_index = 0u;
        s_persist.meta_sequence = 0u;
        s_persist.last_error = APP_FLASHSTORE_RESULT_SCHEMA_MISMATCH;
        return false;
    }

    s_persist.active_site_index = meta.active_site_index;
    s_persist.meta_sequence = seq;
    s_persist.last_error = APP_FLASHSTORE_RESULT_OK;
    return true;
}

static bool vario_persist_load_site_internal(uint8_t index,
                                             vario_persist_site_t *out_site,
                                             uint32_t *out_sequence)
{
    app_flashstore_region_t region;
    vario_persist_site_disk_t disk;
    uint32_t size;
    uint32_t seq;
    uint8_t i;

    if ((index >= VARIO_PERSISTENCE_MAX_SITES) || (out_site == NULL))
    {
        return false;
    }

    vario_persist_fill_default_site(index, out_site);
    region = vario_persist_make_site_region(index);
    memset(&disk, 0, sizeof(disk));
    if (APP_FlashStore_Load(&region, &disk, sizeof(disk), &size, &seq) != APP_FLASHSTORE_RESULT_OK)
    {
        if (out_sequence != NULL)
        {
            *out_sequence = 0u;
        }
        s_persist.last_error = APP_FlashStore_GetLastResult();
        return false;
    }

    if ((size != sizeof(disk)) || (disk.format_version != 2u))
    {
        if (out_sequence != NULL)
        {
            *out_sequence = 0u;
        }
        s_persist.last_error = APP_FLASHSTORE_RESULT_SCHEMA_MISMATCH;
        return false;
    }

    out_site->stored = (disk.stored != 0u) ? true : false;
    vario_persist_copy_name(out_site->site_name,
                            sizeof(out_site->site_name),
                            disk.site_name,
                            out_site->site_name);
    out_site->next_id = (disk.next_id != 0u) ? disk.next_id : 1u;
    out_site->next_field_serial = (disk.next_field_serial != 0u) ? disk.next_field_serial : 1u;
    out_site->next_waypoint_serial = (disk.next_waypoint_serial != 0u) ? disk.next_waypoint_serial : 1u;
    vario_persist_point_from_disk(&disk.home, &out_site->home);
    for (i = 0u; i < VARIO_PERSISTENCE_MAX_FIELDS; ++i)
    {
        vario_persist_point_from_disk(&disk.landings[i], &out_site->landings[i]);
    }
    for (i = 0u; i < VARIO_PERSISTENCE_MAX_WAYPOINTS; ++i)
    {
        vario_persist_point_from_disk(&disk.waypoints[i], &out_site->waypoints[i]);
    }

    if (out_sequence != NULL)
    {
        *out_sequence = seq;
    }
    s_persist.last_error = APP_FLASHSTORE_RESULT_OK;
    return true;
}

static bool vario_persist_save_site_internal(uint8_t index,
                                             const vario_persist_site_t *site,
                                             uint32_t *inout_sequence)
{
    app_flashstore_region_t region;
    vario_persist_site_disk_t disk;
    char default_name[VARIO_PERSISTENCE_NAME_MAX];
    uint8_t i;

    if ((site == NULL) || (index >= VARIO_PERSISTENCE_MAX_SITES) || (s_persist.ready == false))
    {
        return false;
    }

    memset(&disk, 0, sizeof(disk));
    disk.format_version = 2u;
    vario_persist_build_default_site_name(index, default_name, sizeof(default_name));
    vario_persist_copy_name(disk.site_name, sizeof(disk.site_name), site->site_name, default_name);
    disk.next_id = (site->next_id != 0u) ? site->next_id : 1u;
    disk.next_field_serial = (site->next_field_serial != 0u) ? site->next_field_serial : 1u;
    disk.next_waypoint_serial = (site->next_waypoint_serial != 0u) ? site->next_waypoint_serial : 1u;
    disk.stored = (site->stored != false) ? 1u : 0u;
    vario_persist_point_to_disk(&site->home, &disk.home);
    for (i = 0u; i < VARIO_PERSISTENCE_MAX_FIELDS; ++i)
    {
        vario_persist_point_to_disk(&site->landings[i], &disk.landings[i]);
    }
    for (i = 0u; i < VARIO_PERSISTENCE_MAX_WAYPOINTS; ++i)
    {
        vario_persist_point_to_disk(&site->waypoints[i], &disk.waypoints[i]);
    }

    region = vario_persist_make_site_region(index);
    if (APP_FlashStore_Save(&region, &disk, sizeof(disk), inout_sequence) != APP_FLASHSTORE_RESULT_OK)
    {
        s_persist.last_error = APP_FlashStore_GetLastResult();
        return false;
    }

    s_persist.last_error = APP_FLASHSTORE_RESULT_OK;
    return true;
}

bool Vario_Persistence_Init(void)
{
    app_flashstore_layout_t layout;
    uint8_t i;

    if (s_persist.initialized != false)
    {
        return s_persist.ready;
    }

    memset(&s_persist, 0, sizeof(s_persist));
    APP_FlashStore_Init();

    memset(&layout, 0, sizeof(layout));
    layout.owner_app_id = APP_FLASHSTORE_APP_ID_VARIO;
    layout.layout_version = VARIO_PERSIST_LAYOUT_VERSION;
    layout.managed_span_bytes = VARIO_PERSIST_MANAGED_SPAN_BYTES;

    if (APP_FlashStore_AttachLayout(&layout) != APP_FLASHSTORE_RESULT_OK)
    {
        for (i = 0u; i < VARIO_PERSISTENCE_MAX_SITES; ++i)
        {
            vario_persist_site_t site;
            vario_persist_fill_default_site(i, &site);
            vario_persist_update_summary_from_site(i, &site, &s_persist.summaries[i]);
        }
        s_persist.initialized = true;
        s_persist.ready = false;
        s_persist.active_site_index = 0u;
        s_persist.last_error = APP_FlashStore_GetLastResult();
        return false;
    }

    (void)vario_persist_load_meta();
    for (i = 0u; i < VARIO_PERSISTENCE_MAX_SITES; ++i)
    {
        vario_persist_site_t site;
        uint32_t seq;
        if (vario_persist_load_site_internal(i, &site, &seq) != false)
        {
            s_persist.site_sequences[i] = seq;
        }
        else
        {
            vario_persist_fill_default_site(i, &site);
            s_persist.site_sequences[i] = 0u;
        }
        vario_persist_update_summary_from_site(i, &site, &s_persist.summaries[i]);
    }

    if (s_persist.active_site_index >= VARIO_PERSISTENCE_MAX_SITES)
    {
        s_persist.active_site_index = 0u;
        (void)vario_persist_save_meta();
    }

    s_persist.initialized = true;
    s_persist.ready = true;
    s_persist.last_error = APP_FLASHSTORE_RESULT_OK;
    return true;
}

bool Vario_Persistence_IsReady(void)
{
    if (s_persist.initialized == false)
    {
        (void)Vario_Persistence_Init();
    }
    return s_persist.ready;
}

const char *Vario_Persistence_GetLastErrorText(void)
{
    return APP_FlashStore_GetResultText(s_persist.last_error);
}

bool Vario_Persistence_LoadSettings(vario_settings_t *inout_settings)
{
    app_flashstore_region_t region;
    uint32_t size;
    uint32_t seq;

    if (inout_settings == NULL)
    {
        return false;
    }
    if (Vario_Persistence_IsReady() == false)
    {
        return false;
    }

    region = vario_persist_make_settings_region();
    if (APP_FlashStore_Load(&region,
                            inout_settings,
                            sizeof(*inout_settings),
                            &size,
                            &seq) != APP_FLASHSTORE_RESULT_OK)
    {
        s_persist.last_error = APP_FlashStore_GetLastResult();
        return false;
    }
    if (size != sizeof(*inout_settings))
    {
        s_persist.last_error = APP_FLASHSTORE_RESULT_SCHEMA_MISMATCH;
        return false;
    }

    s_persist.settings_sequence = seq;
    s_persist.last_error = APP_FLASHSTORE_RESULT_OK;
    return true;
}

bool Vario_Persistence_SaveSettings(const vario_settings_t *settings)
{
    app_flashstore_region_t region;

    if ((settings == NULL) || (Vario_Persistence_IsReady() == false))
    {
        return false;
    }

    region = vario_persist_make_settings_region();
    if (APP_FlashStore_Save(&region,
                            settings,
                            sizeof(*settings),
                            &s_persist.settings_sequence) != APP_FLASHSTORE_RESULT_OK)
    {
        s_persist.last_error = APP_FlashStore_GetLastResult();
        return false;
    }

    s_persist.last_error = APP_FLASHSTORE_RESULT_OK;
    return true;
}

uint8_t Vario_Persistence_GetSiteCount(void)
{
    return VARIO_PERSISTENCE_MAX_SITES;
}

uint8_t Vario_Persistence_GetActiveSiteIndex(void)
{
    if (s_persist.initialized == false)
    {
        (void)Vario_Persistence_Init();
    }
    return s_persist.active_site_index;
}

bool Vario_Persistence_SetActiveSiteIndex(uint8_t index)
{
    if ((index >= VARIO_PERSISTENCE_MAX_SITES) || (Vario_Persistence_IsReady() == false))
    {
        return false;
    }

    s_persist.active_site_index = index;
    return vario_persist_save_meta();
}

bool Vario_Persistence_GetSiteSummary(uint8_t index, vario_persist_site_summary_t *out_summary)
{
    if ((index >= VARIO_PERSISTENCE_MAX_SITES) || (out_summary == NULL))
    {
        return false;
    }
    if (s_persist.initialized == false)
    {
        (void)Vario_Persistence_Init();
    }

    *out_summary = s_persist.summaries[index];
    return true;
}

bool Vario_Persistence_LoadSite(uint8_t index, vario_persist_site_t *out_site)
{
    uint32_t seq;

    if ((index >= VARIO_PERSISTENCE_MAX_SITES) || (out_site == NULL))
    {
        return false;
    }
    if (s_persist.initialized == false)
    {
        (void)Vario_Persistence_Init();
    }

    if (s_persist.ready == false)
    {
        vario_persist_fill_default_site(index, out_site);
        return false;
    }

    if (vario_persist_load_site_internal(index, out_site, &seq) != false)
    {
        s_persist.site_sequences[index] = seq;
        vario_persist_update_summary_from_site(index, out_site, &s_persist.summaries[index]);
        return true;
    }

    vario_persist_fill_default_site(index, out_site);
    vario_persist_update_summary_from_site(index, out_site, &s_persist.summaries[index]);
    return false;
}

bool Vario_Persistence_SaveSite(uint8_t index, const vario_persist_site_t *site)
{
    vario_persist_site_t normalized;

    if ((index >= VARIO_PERSISTENCE_MAX_SITES) || (site == NULL) || (Vario_Persistence_IsReady() == false))
    {
        return false;
    }

    normalized = *site;
    normalized.stored = true;
    if (vario_persist_save_site_internal(index, &normalized, &s_persist.site_sequences[index]) == false)
    {
        return false;
    }

    vario_persist_update_summary_from_site(index, &normalized, &s_persist.summaries[index]);
    return true;
}

bool Vario_Persistence_RenameSite(uint8_t index, const char *name)
{
    vario_persist_site_t site;

    if (index >= VARIO_PERSISTENCE_MAX_SITES)
    {
        return false;
    }

    (void)Vario_Persistence_LoadSite(index, &site);
    vario_persist_copy_name(site.site_name, sizeof(site.site_name), name, site.site_name);
    return Vario_Persistence_SaveSite(index, &site);
}

bool Vario_Persistence_ClearSite(uint8_t index, bool keep_name)
{
    vario_persist_site_t site;
    char name_copy[VARIO_PERSISTENCE_NAME_MAX];

    if ((index >= VARIO_PERSISTENCE_MAX_SITES) || (Vario_Persistence_IsReady() == false))
    {
        return false;
    }

    (void)Vario_Persistence_LoadSite(index, &site);
    vario_persist_copy_name(name_copy, sizeof(name_copy), site.site_name, NULL);
    vario_persist_fill_default_site(index, &site);
    site.stored = false;
    if (keep_name != false)
    {
        vario_persist_copy_name(site.site_name, sizeof(site.site_name), name_copy, site.site_name);
    }

    if (vario_persist_save_site_internal(index, &site, &s_persist.site_sequences[index]) == false)
    {
        return false;
    }

    vario_persist_update_summary_from_site(index, &site, &s_persist.summaries[index]);
    return true;
}
