#ifndef FW_PACKAGE_H
#define FW_PACKAGE_H

#include "main.h"
#include "fatfs.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "FW_BootConfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  FW_Package                                                                 */
/*                                                                            */
/*  엔드유저가 SD root 에 넣는 SYSUPDATE.bin 의 헤더 정의와                   */
/*  버전 비교 helper를 제공한다.                                              */
/*                                                                            */
/*  파일 레이아웃                                                              */
/*  [fw_package_header_t][raw app binary payload...]                          */
/*                                                                            */
/*  raw app binary payload는                                                   */
/*  - APP linker script가 FW_APP_BASE_ADDRESS 기준으로 링크한                 */
/*    Main App 전용 .bin 파일이어야 한다.                                     */
/*  - 즉, bootloader를 포함한 통짜 이미지가 아니다.                            */
/* -------------------------------------------------------------------------- */

#define FW_PACKAGE_MAGIC             0x47504B46u  /* 'FKPG' little-endian view */
#define FW_PACKAGE_FORMAT_VERSION    0x0001u

typedef enum
{
    FW_PACKAGE_RESULT_OK = 0u,
    FW_PACKAGE_RESULT_FILE_IO,
    FW_PACKAGE_RESULT_SHORT_READ,
    FW_PACKAGE_RESULT_BAD_MAGIC,
    FW_PACKAGE_RESULT_BAD_HEADER_SIZE,
    FW_PACKAGE_RESULT_BAD_HEADER_CRC,
    FW_PACKAGE_RESULT_BAD_PRODUCT_TAG,
    FW_PACKAGE_RESULT_BAD_APP_BASE,
    FW_PACKAGE_RESULT_BAD_PAYLOAD_SIZE,
    FW_PACKAGE_RESULT_BAD_VERSION_STRING,
    FW_PACKAGE_RESULT_BAD_PAYLOAD_CRC
} fw_package_result_t;

typedef enum
{
    FW_PACKAGE_COMPARE_UNKNOWN = 0u,
    FW_PACKAGE_COMPARE_SAME    = 1u,
    FW_PACKAGE_COMPARE_NEWER   = 2u,
    FW_PACKAGE_COMPARE_OLDER   = 3u
} fw_package_compare_t;

typedef struct __attribute__((packed))
{
    uint32_t magic;                                         /* FW_PACKAGE_MAGIC          */
    uint16_t format_version;                                /* FW_PACKAGE_FORMAT_VERSION */
    uint16_t header_size;                                   /* sizeof(fw_package_header_t) */

    char     product_tag[FW_PACKAGE_PRODUCT_TAG_MAX];       /* 대상 제품 식별 문자열     */

    uint32_t app_base_address;                              /* 반드시 FW_APP_BASE_ADDRESS */
    uint32_t payload_size_bytes;                            /* raw app .bin 길이         */
    uint32_t payload_crc32;                                 /* payload CRC32             */

    uint32_t version_u32;                                   /* packed version            */
    char     version_string[FW_VERSION_STRING_MAX];         /* "AA.BB.CC.DD"            */

    uint32_t flags;                                         /* 확장용 reserve            */
    uint32_t reserved[8];                                   /* 확장용 reserve            */

    uint32_t header_crc32;                                  /* header CRC32              */
} fw_package_header_t;

uint32_t FW_Package_VersionStringToU32(const char *version_string);
void FW_Package_VersionU32ToString(uint32_t version_u32, char *out_text, size_t out_size);
fw_package_compare_t FW_Package_CompareVersions(uint32_t installed_version_u32,
                                                uint32_t package_version_u32);

fw_package_result_t FW_Package_ParseHeader(const uint8_t *raw_header_bytes,
                                           size_t raw_length,
                                           fw_package_header_t *out_header);

fw_package_result_t FW_Package_ReadHeaderFromFile(FIL *fp,
                                                  fw_package_header_t *out_header);

bool FW_Package_IsHeaderValid(const fw_package_header_t *header);

#ifdef __cplusplus
}
#endif

#endif /* FW_PACKAGE_H */
