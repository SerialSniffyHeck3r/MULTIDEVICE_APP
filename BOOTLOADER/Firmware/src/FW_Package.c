#include "FW_Package.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "FW_Crc32.h"

/* -------------------------------------------------------------------------- */
/*  내부 helper: 안전 문자열 복사                                              */
/* -------------------------------------------------------------------------- */
static void FW_Package_CopyTextSafe(char *dst, size_t dst_size, const char *src)
{
    size_t copy_len;

    if ((dst == 0) || (dst_size == 0u))
    {
        return;
    }

    dst[0] = '\0';

    if (src == 0)
    {
        return;
    }

    copy_len = strlen(src);
    if (copy_len >= dst_size)
    {
        copy_len = dst_size - 1u;
    }

    memcpy(dst, src, copy_len);
    dst[copy_len] = '\0';
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: header CRC 계산                                               */
/*                                                                            */
/*  header_crc32 필드를 0으로 보고 전체 header 크기만큼 CRC를 계산한다.        */
/* -------------------------------------------------------------------------- */
static uint32_t FW_Package_CalcHeaderCrc(const fw_package_header_t *header)
{
    fw_package_header_t temp;

    if (header == 0)
    {
        return 0u;
    }

    memcpy(&temp, header, sizeof(temp));
    temp.header_crc32 = 0u;

    return FW_CRC32_Calc(&temp, sizeof(temp));
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 버전 문자열 -> packed u32                                        */
/* -------------------------------------------------------------------------- */
uint32_t FW_Package_VersionStringToU32(const char *version_string)
{
    unsigned int a;
    unsigned int b;
    unsigned int c;
    unsigned int d;

    a = 0u;
    b = 0u;
    c = 0u;
    d = 0u;

    if (version_string == 0)
    {
        return 0u;
    }

    if (sscanf(version_string, "%u.%u.%u.%u", &a, &b, &c, &d) != 4)
    {
        return 0u;
    }

    if ((a > 255u) || (b > 255u) || (c > 255u) || (d > 255u))
    {
        return 0u;
    }

    return FW_VERSION_PACK_U32(a, b, c, d);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: packed u32 -> 버전 문자열                                        */
/* -------------------------------------------------------------------------- */
void FW_Package_VersionU32ToString(uint32_t version_u32, char *out_text, size_t out_size)
{
    unsigned int a;
    unsigned int b;
    unsigned int c;
    unsigned int d;

    if ((out_text == 0) || (out_size == 0u))
    {
        return;
    }

    a = (unsigned int)((version_u32 >> 24) & 0xFFu);
    b = (unsigned int)((version_u32 >> 16) & 0xFFu);
    c = (unsigned int)((version_u32 >> 8)  & 0xFFu);
    d = (unsigned int)((version_u32 >> 0)  & 0xFFu);

    (void)snprintf(out_text, out_size, "%02u.%02u.%02u.%02u", a, b, c, d);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 버전 비교                                                        */
/* -------------------------------------------------------------------------- */
fw_package_compare_t FW_Package_CompareVersions(uint32_t installed_version_u32,
                                                uint32_t package_version_u32)
{
    if ((installed_version_u32 == 0u) || (package_version_u32 == 0u))
    {
        return FW_PACKAGE_COMPARE_UNKNOWN;
    }

    if (package_version_u32 == installed_version_u32)
    {
        return FW_PACKAGE_COMPARE_SAME;
    }

    if (package_version_u32 > installed_version_u32)
    {
        return FW_PACKAGE_COMPARE_NEWER;
    }

    return FW_PACKAGE_COMPARE_OLDER;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: header 유효성 검사                                               */
/* -------------------------------------------------------------------------- */
bool FW_Package_IsHeaderValid(const fw_package_header_t *header)
{
    if (header == 0)
    {
        return false;
    }

    if (header->magic != FW_PACKAGE_MAGIC)
    {
        return false;
    }

    if (header->format_version != FW_PACKAGE_FORMAT_VERSION)
    {
        return false;
    }

    if (header->header_size != sizeof(fw_package_header_t))
    {
        return false;
    }

    if (strncmp(header->product_tag,
                FW_PACKAGE_PRODUCT_TAG,
                FW_PACKAGE_PRODUCT_TAG_MAX) != 0)
    {
        return false;
    }

    if (header->app_base_address != FW_APP_BASE_ADDRESS)
    {
        return false;
    }

    if ((header->payload_size_bytes == 0u) ||
        (header->payload_size_bytes > FW_APP_MAX_SIZE_BYTES))
    {
        return false;
    }

    if (FW_Package_VersionStringToU32(header->version_string) != header->version_u32)
    {
        return false;
    }

    if (FW_Package_CalcHeaderCrc(header) != header->header_crc32)
    {
        return false;
    }

    return true;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: raw header 파싱                                                  */
/* -------------------------------------------------------------------------- */
fw_package_result_t FW_Package_ParseHeader(const uint8_t *raw_header_bytes,
                                           size_t raw_length,
                                           fw_package_header_t *out_header)
{
    fw_package_header_t temp;

    if ((raw_header_bytes == 0) || (out_header == 0))
    {
        return FW_PACKAGE_RESULT_BAD_MAGIC;
    }

    if (raw_length < sizeof(fw_package_header_t))
    {
        return FW_PACKAGE_RESULT_SHORT_READ;
    }

    memcpy(&temp, raw_header_bytes, sizeof(temp));

    if (temp.magic != FW_PACKAGE_MAGIC)
    {
        return FW_PACKAGE_RESULT_BAD_MAGIC;
    }

    if (temp.header_size != sizeof(fw_package_header_t))
    {
        return FW_PACKAGE_RESULT_BAD_HEADER_SIZE;
    }

    if (FW_Package_CalcHeaderCrc(&temp) != temp.header_crc32)
    {
        return FW_PACKAGE_RESULT_BAD_HEADER_CRC;
    }

    if (strncmp(temp.product_tag,
                FW_PACKAGE_PRODUCT_TAG,
                FW_PACKAGE_PRODUCT_TAG_MAX) != 0)
    {
        return FW_PACKAGE_RESULT_BAD_PRODUCT_TAG;
    }

    if (temp.app_base_address != FW_APP_BASE_ADDRESS)
    {
        return FW_PACKAGE_RESULT_BAD_APP_BASE;
    }

    if ((temp.payload_size_bytes == 0u) ||
        (temp.payload_size_bytes > FW_APP_MAX_SIZE_BYTES))
    {
        return FW_PACKAGE_RESULT_BAD_PAYLOAD_SIZE;
    }

    if (FW_Package_VersionStringToU32(temp.version_string) != temp.version_u32)
    {
        return FW_PACKAGE_RESULT_BAD_VERSION_STRING;
    }

    memcpy(out_header, &temp, sizeof(temp));
    return FW_PACKAGE_RESULT_OK;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 파일에서 header 읽기                                             */
/* -------------------------------------------------------------------------- */
fw_package_result_t FW_Package_ReadHeaderFromFile(FIL *fp,
                                                  fw_package_header_t *out_header)
{
    UINT br;
    uint8_t raw_header[sizeof(fw_package_header_t)];
    FRESULT fr;

    if ((fp == 0) || (out_header == 0))
    {
        return FW_PACKAGE_RESULT_FILE_IO;
    }

    br = 0u;
    memset(raw_header, 0, sizeof(raw_header));

    fr = f_lseek(fp, 0u);
    if (fr != FR_OK)
    {
        return FW_PACKAGE_RESULT_FILE_IO;
    }

    fr = f_read(fp, raw_header, sizeof(raw_header), &br);
    if (fr != FR_OK)
    {
        return FW_PACKAGE_RESULT_FILE_IO;
    }

    if (br != sizeof(raw_header))
    {
        return FW_PACKAGE_RESULT_SHORT_READ;
    }

    return FW_Package_ParseHeader(raw_header, sizeof(raw_header), out_header);
}
