#include "APP_FlashStore.h"

#include <string.h>

#ifndef APP_FLASHSTORE_HEADER_MAGIC
#define APP_FLASHSTORE_HEADER_MAGIC 0x46535452u /* 'FSTR' */
#endif

#ifndef APP_FLASHSTORE_HEADER_VERSION
#define APP_FLASHSTORE_HEADER_VERSION 1u
#endif

#ifndef APP_FLASHSTORE_COMMIT_MAGIC
#define APP_FLASHSTORE_COMMIT_MAGIC 0x434D4954u /* 'CMIT' */
#endif

#ifndef APP_FLASHSTORE_SUPER_REGION_ID
#define APP_FLASHSTORE_SUPER_REGION_ID 0x53555052u /* 'SUPR' */
#endif

#ifndef APP_FLASHSTORE_SUPER_SCHEMA_ID
#define APP_FLASHSTORE_SUPER_SCHEMA_ID 0x00010001u
#endif

#ifndef APP_FLASHSTORE_SUPER_A_ADDRESS
#define APP_FLASHSTORE_SUPER_A_ADDRESS (0u * APP_FLASHSTORE_SECTOR_SIZE)
#endif

#ifndef APP_FLASHSTORE_SUPER_B_ADDRESS
#define APP_FLASHSTORE_SUPER_B_ADDRESS (1u * APP_FLASHSTORE_SECTOR_SIZE)
#endif

#ifndef APP_FLASHSTORE_READBACK_CHUNK
#define APP_FLASHSTORE_READBACK_CHUNK 64u
#endif

#if defined(__GNUC__)
#define APP_FLASHSTORE_PACKED __attribute__((packed))
#else
#define APP_FLASHSTORE_PACKED
#endif

typedef struct APP_FLASHSTORE_PACKED
{
    uint32_t magic;
    uint16_t header_version;
    uint16_t header_size;
    uint32_t owner_app_id;
    uint32_t region_id;
    uint32_t schema_id;
    uint32_t sequence;
    uint32_t payload_size;
    uint32_t payload_crc32;
    uint32_t reserved0;
    uint32_t header_crc32;
    uint32_t commit_magic;
} app_flashstore_record_header_t;

typedef struct APP_FLASHSTORE_PACKED
{
    uint32_t owner_app_id;
    uint32_t layout_version;
    uint32_t managed_span_bytes;
    uint32_t reserved0;
    uint32_t reserved1;
    uint32_t reserved2;
} app_flashstore_super_payload_t;

typedef struct
{
    bool                    initialized;
    bool                    present;
    bool                    ready;
    app_flashstore_layout_t layout;
    uint32_t                capacity_bytes;
    app_flashstore_result_t last_result;
} app_flashstore_runtime_t;

static app_flashstore_runtime_t s_flashstore;

static uint32_t app_flashstore_crc32(const void *data, uint32_t length)
{
    const uint8_t *p;
    uint32_t crc;
    uint32_t i;
    uint8_t bit;

    p = (const uint8_t *)data;
    crc = 0xFFFFFFFFu;
    for (i = 0u; i < length; ++i)
    {
        crc ^= (uint32_t)p[i];
        for (bit = 0u; bit < 8u; ++bit)
        {
            if ((crc & 1u) != 0u)
            {
                crc = (crc >> 1u) ^ 0xEDB88320u;
            }
            else
            {
                crc >>= 1u;
            }
        }
    }
    return ~crc;
}

static void app_flashstore_update_presence_snapshot(void)
{
    spi_flash_snapshot_t snap;

    memset(&snap, 0, sizeof(snap));
    SPI_Flash_CopySnapshot(&snap);
    if (snap.initialized == false)
    {
        SPI_Flash_Init();
        memset(&snap, 0, sizeof(snap));
        SPI_Flash_CopySnapshot(&snap);
    }

    s_flashstore.initialized = true;
    s_flashstore.present = (snap.present != false) && (snap.capacity_bytes >= (2u * APP_FLASHSTORE_SECTOR_SIZE));
    s_flashstore.capacity_bytes = snap.capacity_bytes;
    if (s_flashstore.present == false)
    {
        s_flashstore.ready = false;
        s_flashstore.last_result = APP_FLASHSTORE_RESULT_NOT_PRESENT;
    }
}

static bool app_flashstore_region_addresses_valid(const app_flashstore_region_t *region)
{
    if ((region == NULL) || (s_flashstore.present == false))
    {
        return false;
    }

    if ((region->copy0_address % APP_FLASHSTORE_SECTOR_SIZE) != 0u)
    {
        return false;
    }
    if ((region->copy1_address % APP_FLASHSTORE_SECTOR_SIZE) != 0u)
    {
        return false;
    }
    if (region->payload_capacity_bytes == 0u)
    {
        return false;
    }
    if ((sizeof(app_flashstore_record_header_t) + region->payload_capacity_bytes) > APP_FLASHSTORE_SECTOR_SIZE)
    {
        return false;
    }
    if ((region->copy0_address + APP_FLASHSTORE_SECTOR_SIZE) > s_flashstore.capacity_bytes)
    {
        return false;
    }
    if ((region->copy1_address + APP_FLASHSTORE_SECTOR_SIZE) > s_flashstore.capacity_bytes)
    {
        return false;
    }

    return true;
}

static bool app_flashstore_header_is_valid_basic(const app_flashstore_record_header_t *hdr,
                                                 const app_flashstore_region_t *region)
{
    app_flashstore_record_header_t temp;
    uint32_t crc;

    if ((hdr == NULL) || (region == NULL))
    {
        return false;
    }

    if (hdr->magic != APP_FLASHSTORE_HEADER_MAGIC)
    {
        return false;
    }
    if (hdr->header_version != APP_FLASHSTORE_HEADER_VERSION)
    {
        return false;
    }
    if (hdr->header_size != sizeof(app_flashstore_record_header_t))
    {
        return false;
    }
    if (hdr->owner_app_id != s_flashstore.layout.owner_app_id)
    {
        return false;
    }
    if (hdr->region_id != region->region_id)
    {
        return false;
    }
    if (hdr->schema_id != region->schema_id)
    {
        return false;
    }
    if (hdr->payload_size > region->payload_capacity_bytes)
    {
        return false;
    }
    if (hdr->commit_magic != APP_FLASHSTORE_COMMIT_MAGIC)
    {
        return false;
    }

    temp = *hdr;
    temp.header_crc32 = 0u;
    crc = app_flashstore_crc32(&temp, sizeof(temp));
    if (crc != hdr->header_crc32)
    {
        return false;
    }

    return true;
}

static bool app_flashstore_verify_payload_crc(const app_flashstore_region_t *region,
                                              uint32_t copy_address,
                                              const app_flashstore_record_header_t *hdr)
{
    uint8_t buffer[APP_FLASHSTORE_READBACK_CHUNK];
    uint32_t remaining;
    uint32_t address;
    uint32_t crc;
    uint32_t chunk;
    uint32_t i;
    uint8_t bit;

    if ((region == NULL) || (hdr == NULL))
    {
        return false;
    }

    remaining = hdr->payload_size;
    address = copy_address + sizeof(app_flashstore_record_header_t);
    crc = 0xFFFFFFFFu;

    while (remaining > 0u)
    {
        chunk = (remaining > sizeof(buffer)) ? sizeof(buffer) : remaining;
        if (SPI_Flash_ReadBuffer(address, buffer, chunk) != HAL_OK)
        {
            return false;
        }

        for (i = 0u; i < chunk; ++i)
        {
            crc ^= (uint32_t)buffer[i];
            for (bit = 0u; bit < 8u; ++bit)
            {
                if ((crc & 1u) != 0u)
                {
                    crc = (crc >> 1u) ^ 0xEDB88320u;
                }
                else
                {
                    crc >>= 1u;
                }
            }
        }

        address += chunk;
        remaining -= chunk;
    }

    crc = ~crc;
    return (crc == hdr->payload_crc32) ? true : false;
}

static bool app_flashstore_read_valid_copy(const app_flashstore_region_t *region,
                                           uint32_t copy_address,
                                           app_flashstore_record_header_t *out_hdr)
{
    app_flashstore_record_header_t hdr;

    if ((region == NULL) || (out_hdr == NULL))
    {
        return false;
    }

    if (SPI_Flash_ReadBuffer(copy_address, (uint8_t *)&hdr, sizeof(hdr)) != HAL_OK)
    {
        return false;
    }
    if (app_flashstore_header_is_valid_basic(&hdr, region) == false)
    {
        return false;
    }
    if (app_flashstore_verify_payload_crc(region, copy_address, &hdr) == false)
    {
        return false;
    }

    *out_hdr = hdr;
    return true;
}

static app_flashstore_result_t app_flashstore_write_and_verify(uint32_t address,
                                                               const void *data,
                                                               uint32_t length)
{
    uint8_t verify[APP_FLASHSTORE_READBACK_CHUNK];
    const uint8_t *src;
    uint32_t remaining;
    uint32_t chunk;

    if ((data == NULL) && (length != 0u))
    {
        return APP_FLASHSTORE_RESULT_BAD_PARAM;
    }

    if (length == 0u)
    {
        return APP_FLASHSTORE_RESULT_OK;
    }

    if (SPI_Flash_WriteBuffer(address, (const uint8_t *)data, length) != HAL_OK)
    {
        return APP_FLASHSTORE_RESULT_HAL_ERROR;
    }

    src = (const uint8_t *)data;
    remaining = length;
    while (remaining > 0u)
    {
        chunk = (remaining > sizeof(verify)) ? sizeof(verify) : remaining;
        if (SPI_Flash_ReadBuffer(address, verify, chunk) != HAL_OK)
        {
            return APP_FLASHSTORE_RESULT_HAL_ERROR;
        }
        if (memcmp(src, verify, chunk) != 0)
        {
            return APP_FLASHSTORE_RESULT_VERIFY_FAIL;
        }
        address += chunk;
        src += chunk;
        remaining -= chunk;
    }

    return APP_FLASHSTORE_RESULT_OK;
}

static app_flashstore_result_t app_flashstore_load_internal(const app_flashstore_region_t *region,
                                                            void *dst_payload,
                                                            uint32_t dst_capacity_bytes,
                                                            uint32_t *out_payload_size,
                                                            uint32_t *out_sequence,
                                                            app_flashstore_record_header_t *out_selected_hdr,
                                                            uint32_t *out_selected_address)
{
    app_flashstore_record_header_t hdr_a;
    app_flashstore_record_header_t hdr_b;
    bool valid_a;
    bool valid_b;
    const app_flashstore_record_header_t *selected_hdr;
    uint32_t selected_address;

    if ((region == NULL) || (dst_payload == NULL))
    {
        return APP_FLASHSTORE_RESULT_BAD_PARAM;
    }
    if (app_flashstore_region_addresses_valid(region) == false)
    {
        return APP_FLASHSTORE_RESULT_RANGE;
    }

    valid_a = app_flashstore_read_valid_copy(region, region->copy0_address, &hdr_a);
    valid_b = app_flashstore_read_valid_copy(region, region->copy1_address, &hdr_b);

    if ((valid_a == false) && (valid_b == false))
    {
        return APP_FLASHSTORE_RESULT_NO_VALID_COPY;
    }

    if ((valid_a != false) && ((valid_b == false) || (hdr_a.sequence >= hdr_b.sequence)))
    {
        selected_hdr = &hdr_a;
        selected_address = region->copy0_address;
    }
    else
    {
        selected_hdr = &hdr_b;
        selected_address = region->copy1_address;
    }

    if (selected_hdr->payload_size > dst_capacity_bytes)
    {
        return APP_FLASHSTORE_RESULT_RANGE;
    }
    if (SPI_Flash_ReadBuffer(selected_address + sizeof(app_flashstore_record_header_t),
                             (uint8_t *)dst_payload,
                             selected_hdr->payload_size) != HAL_OK)
    {
        return APP_FLASHSTORE_RESULT_HAL_ERROR;
    }

    if (out_payload_size != NULL)
    {
        *out_payload_size = selected_hdr->payload_size;
    }
    if (out_sequence != NULL)
    {
        *out_sequence = selected_hdr->sequence;
    }
    if (out_selected_hdr != NULL)
    {
        *out_selected_hdr = *selected_hdr;
    }
    if (out_selected_address != NULL)
    {
        *out_selected_address = selected_address;
    }

    return APP_FLASHSTORE_RESULT_OK;
}

static app_flashstore_result_t app_flashstore_save_internal(const app_flashstore_region_t *region,
                                                            const void *src_payload,
                                                            uint32_t payload_size,
                                                            uint32_t *inout_sequence)
{
    app_flashstore_record_header_t hdr_a;
    app_flashstore_record_header_t hdr_b;
    bool valid_a;
    bool valid_b;
    uint32_t target_address;
    uint32_t next_sequence;
    app_flashstore_record_header_t header;
    app_flashstore_result_t result;

    if ((region == NULL) || ((src_payload == NULL) && (payload_size != 0u)))
    {
        return APP_FLASHSTORE_RESULT_BAD_PARAM;
    }
    if (app_flashstore_region_addresses_valid(region) == false)
    {
        return APP_FLASHSTORE_RESULT_RANGE;
    }
    if (payload_size > region->payload_capacity_bytes)
    {
        return APP_FLASHSTORE_RESULT_RANGE;
    }

    valid_a = app_flashstore_read_valid_copy(region, region->copy0_address, &hdr_a);
    valid_b = app_flashstore_read_valid_copy(region, region->copy1_address, &hdr_b);

    if ((valid_a != false) && ((valid_b == false) || (hdr_a.sequence >= hdr_b.sequence)))
    {
        target_address = region->copy1_address;
        next_sequence = hdr_a.sequence + 1u;
    }
    else if (valid_b != false)
    {
        target_address = region->copy0_address;
        next_sequence = hdr_b.sequence + 1u;
    }
    else
    {
        target_address = region->copy0_address;
        next_sequence = 1u;
    }

    if ((inout_sequence != NULL) && (*inout_sequence != 0u) && (*inout_sequence >= next_sequence))
    {
        next_sequence = (*inout_sequence + 1u);
    }
    if (next_sequence == 0u)
    {
        next_sequence = 1u;
    }

    if (SPI_Flash_EraseSector4K(target_address) != HAL_OK)
    {
        return APP_FLASHSTORE_RESULT_HAL_ERROR;
    }

    result = app_flashstore_write_and_verify(target_address + sizeof(app_flashstore_record_header_t),
                                             src_payload,
                                             payload_size);
    if (result != APP_FLASHSTORE_RESULT_OK)
    {
        return result;
    }

    memset(&header, 0, sizeof(header));
    header.magic = APP_FLASHSTORE_HEADER_MAGIC;
    header.header_version = APP_FLASHSTORE_HEADER_VERSION;
    header.header_size = sizeof(header);
    header.owner_app_id = s_flashstore.layout.owner_app_id;
    header.region_id = region->region_id;
    header.schema_id = region->schema_id;
    header.sequence = next_sequence;
    header.payload_size = payload_size;
    header.payload_crc32 = app_flashstore_crc32(src_payload, payload_size);
    header.commit_magic = APP_FLASHSTORE_COMMIT_MAGIC;
    header.header_crc32 = 0u;
    header.header_crc32 = app_flashstore_crc32(&header, sizeof(header));

    result = app_flashstore_write_and_verify(target_address, &header, sizeof(header));
    if (result != APP_FLASHSTORE_RESULT_OK)
    {
        return result;
    }

    if (inout_sequence != NULL)
    {
        *inout_sequence = next_sequence;
    }

    return APP_FLASHSTORE_RESULT_OK;
}

void APP_FlashStore_Init(void)
{
    memset(&s_flashstore, 0, sizeof(s_flashstore));
    app_flashstore_update_presence_snapshot();
}

app_flashstore_result_t APP_FlashStore_EraseSpan(uint32_t start_address,
                                                 uint32_t length_bytes)
{
    uint32_t address;
    uint32_t end_address;

    if (s_flashstore.initialized == false)
    {
        APP_FlashStore_Init();
    }
    if (s_flashstore.present == false)
    {
        s_flashstore.last_result = APP_FLASHSTORE_RESULT_NOT_PRESENT;
        return s_flashstore.last_result;
    }
    if ((start_address % APP_FLASHSTORE_SECTOR_SIZE) != 0u)
    {
        s_flashstore.last_result = APP_FLASHSTORE_RESULT_RANGE;
        return s_flashstore.last_result;
    }
    if ((length_bytes % APP_FLASHSTORE_SECTOR_SIZE) != 0u)
    {
        s_flashstore.last_result = APP_FLASHSTORE_RESULT_RANGE;
        return s_flashstore.last_result;
    }
    if ((start_address + length_bytes) > s_flashstore.capacity_bytes)
    {
        s_flashstore.last_result = APP_FLASHSTORE_RESULT_RANGE;
        return s_flashstore.last_result;
    }

    end_address = start_address + length_bytes;
    for (address = start_address; address < end_address; address += APP_FLASHSTORE_SECTOR_SIZE)
    {
        if (SPI_Flash_EraseSector4K(address) != HAL_OK)
        {
            s_flashstore.last_result = APP_FLASHSTORE_RESULT_HAL_ERROR;
            return s_flashstore.last_result;
        }
    }

    s_flashstore.last_result = APP_FLASHSTORE_RESULT_OK;
    return s_flashstore.last_result;
}

app_flashstore_result_t APP_FlashStore_AttachLayout(const app_flashstore_layout_t *layout)
{
    app_flashstore_region_t super_region;
    app_flashstore_super_payload_t super_payload;
    uint32_t payload_size;
    uint32_t sequence;
    app_flashstore_result_t result;
    bool need_format;

    if (layout == NULL)
    {
        return APP_FLASHSTORE_RESULT_BAD_PARAM;
    }

    if (s_flashstore.initialized == false)
    {
        APP_FlashStore_Init();
    }
    if (s_flashstore.present == false)
    {
        s_flashstore.last_result = APP_FLASHSTORE_RESULT_NOT_PRESENT;
        return s_flashstore.last_result;
    }

    if ((layout->managed_span_bytes < (2u * APP_FLASHSTORE_SECTOR_SIZE)) ||
        ((layout->managed_span_bytes % APP_FLASHSTORE_SECTOR_SIZE) != 0u) ||
        (layout->managed_span_bytes > s_flashstore.capacity_bytes))
    {
        s_flashstore.last_result = APP_FLASHSTORE_RESULT_RANGE;
        return s_flashstore.last_result;
    }

    memset(&super_region, 0, sizeof(super_region));
    memset(&super_payload, 0, sizeof(super_payload));
    super_region.region_id = APP_FLASHSTORE_SUPER_REGION_ID;
    super_region.schema_id = APP_FLASHSTORE_SUPER_SCHEMA_ID;
    super_region.copy0_address = APP_FLASHSTORE_SUPER_A_ADDRESS;
    super_region.copy1_address = APP_FLASHSTORE_SUPER_B_ADDRESS;
    super_region.payload_capacity_bytes = sizeof(super_payload);

    s_flashstore.layout = *layout;
    s_flashstore.ready = false;

    payload_size = 0u;
    sequence = 0u;
    result = app_flashstore_load_internal(&super_region,
                                          &super_payload,
                                          sizeof(super_payload),
                                          &payload_size,
                                          &sequence,
                                          NULL,
                                          NULL);

    need_format = false;
    if (result != APP_FLASHSTORE_RESULT_OK)
    {
        need_format = true;
    }
    else if ((payload_size != sizeof(super_payload)) ||
             (super_payload.owner_app_id != layout->owner_app_id) ||
             (super_payload.layout_version != layout->layout_version) ||
             (super_payload.managed_span_bytes != layout->managed_span_bytes))
    {
        need_format = true;
    }

    if (need_format != false)
    {
        result = APP_FlashStore_EraseSpan(0u, layout->managed_span_bytes);
        if (result != APP_FLASHSTORE_RESULT_OK)
        {
            s_flashstore.ready = false;
            s_flashstore.last_result = APP_FLASHSTORE_RESULT_FORMAT_FAIL;
            return s_flashstore.last_result;
        }

        memset(&super_payload, 0, sizeof(super_payload));
        super_payload.owner_app_id = layout->owner_app_id;
        super_payload.layout_version = layout->layout_version;
        super_payload.managed_span_bytes = layout->managed_span_bytes;
        sequence = 1u;
        result = app_flashstore_save_internal(&super_region,
                                              &super_payload,
                                              sizeof(super_payload),
                                              &sequence);
        if (result != APP_FLASHSTORE_RESULT_OK)
        {
            s_flashstore.ready = false;
            s_flashstore.last_result = result;
            return s_flashstore.last_result;
        }
    }

    s_flashstore.ready = true;
    s_flashstore.last_result = APP_FLASHSTORE_RESULT_OK;
    return s_flashstore.last_result;
}

bool APP_FlashStore_IsReady(void)
{
    return s_flashstore.ready;
}

bool APP_FlashStore_IsPresent(void)
{
    return s_flashstore.present;
}

void APP_FlashStore_CopySnapshot(app_flashstore_snapshot_t *out_snapshot)
{
    if (out_snapshot == NULL)
    {
        return;
    }

    memset(out_snapshot, 0, sizeof(*out_snapshot));
    out_snapshot->initialized = s_flashstore.initialized;
    out_snapshot->present = s_flashstore.present;
    out_snapshot->ready = s_flashstore.ready;
    out_snapshot->owner_app_id = s_flashstore.layout.owner_app_id;
    out_snapshot->layout_version = s_flashstore.layout.layout_version;
    out_snapshot->managed_span_bytes = s_flashstore.layout.managed_span_bytes;
    out_snapshot->capacity_bytes = s_flashstore.capacity_bytes;
    out_snapshot->last_result = s_flashstore.last_result;
}

app_flashstore_result_t APP_FlashStore_GetLastResult(void)
{
    return s_flashstore.last_result;
}

const char *APP_FlashStore_GetResultText(app_flashstore_result_t result)
{
    switch (result)
    {
        case APP_FLASHSTORE_RESULT_OK:
            return "OK";
        case APP_FLASHSTORE_RESULT_NOT_READY:
            return "NOT READY";
        case APP_FLASHSTORE_RESULT_NOT_PRESENT:
            return "NO FLASH";
        case APP_FLASHSTORE_RESULT_BAD_PARAM:
            return "BAD PARAM";
        case APP_FLASHSTORE_RESULT_RANGE:
            return "RANGE";
        case APP_FLASHSTORE_RESULT_OWNER_MISMATCH:
            return "OWNER MISMATCH";
        case APP_FLASHSTORE_RESULT_NO_VALID_COPY:
            return "NO VALID COPY";
        case APP_FLASHSTORE_RESULT_CRC_FAIL:
            return "CRC FAIL";
        case APP_FLASHSTORE_RESULT_SCHEMA_MISMATCH:
            return "SCHEMA";
        case APP_FLASHSTORE_RESULT_HAL_ERROR:
            return "HAL ERR";
        case APP_FLASHSTORE_RESULT_VERIFY_FAIL:
            return "VERIFY";
        case APP_FLASHSTORE_RESULT_FORMAT_FAIL:
            return "FORMAT";
        default:
            return "UNKNOWN";
    }
}

app_flashstore_result_t APP_FlashStore_Load(const app_flashstore_region_t *region,
                                            void *dst_payload,
                                            uint32_t dst_capacity_bytes,
                                            uint32_t *out_payload_size,
                                            uint32_t *out_sequence)
{
    app_flashstore_result_t result;

    if (s_flashstore.ready == false)
    {
        s_flashstore.last_result = APP_FLASHSTORE_RESULT_NOT_READY;
        return s_flashstore.last_result;
    }

    result = app_flashstore_load_internal(region,
                                          dst_payload,
                                          dst_capacity_bytes,
                                          out_payload_size,
                                          out_sequence,
                                          NULL,
                                          NULL);
    s_flashstore.last_result = result;
    return result;
}

app_flashstore_result_t APP_FlashStore_Save(const app_flashstore_region_t *region,
                                            const void *src_payload,
                                            uint32_t payload_size,
                                            uint32_t *inout_sequence)
{
    app_flashstore_result_t result;

    if (s_flashstore.ready == false)
    {
        s_flashstore.last_result = APP_FLASHSTORE_RESULT_NOT_READY;
        return s_flashstore.last_result;
    }

    result = app_flashstore_save_internal(region,
                                          src_payload,
                                          payload_size,
                                          inout_sequence);
    s_flashstore.last_result = result;
    return result;
}

app_flashstore_result_t APP_FlashStore_Clear(const app_flashstore_region_t *region)
{
    if (s_flashstore.ready == false)
    {
        s_flashstore.last_result = APP_FLASHSTORE_RESULT_NOT_READY;
        return s_flashstore.last_result;
    }
    if (app_flashstore_region_addresses_valid(region) == false)
    {
        s_flashstore.last_result = APP_FLASHSTORE_RESULT_RANGE;
        return s_flashstore.last_result;
    }
    if (SPI_Flash_EraseSector4K(region->copy0_address) != HAL_OK)
    {
        s_flashstore.last_result = APP_FLASHSTORE_RESULT_HAL_ERROR;
        return s_flashstore.last_result;
    }
    if (SPI_Flash_EraseSector4K(region->copy1_address) != HAL_OK)
    {
        s_flashstore.last_result = APP_FLASHSTORE_RESULT_HAL_ERROR;
        return s_flashstore.last_result;
    }

    s_flashstore.last_result = APP_FLASHSTORE_RESULT_OK;
    return s_flashstore.last_result;
}
