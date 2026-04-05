#ifndef APP_FLASHSTORE_H
#define APP_FLASHSTORE_H

#include "SPI_Flash.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  APP_FlashStore                                                            */
/*                                                                            */
/*  목적                                                                      */
/*  - Winbond SPI NOR Flash 위에 "전원 중간 차단에도 이전 정상 사본을 남기는"   */
/*    상위 저장 계층을 제공한다.                                              */
/*  - low-level SPI_Flash 드라이버는 opcode / read / write / erase만 알고,   */
/*    본 계층은 owner marker, A/B mirror, sequence, CRC32, commit 판정까지   */
/*    책임진다.                                                               */
/*  - 앱 상위 레이어는 raw sector address를 직접 두드리지 않고,               */
/*    APP_FlashStore region 문법만 사용하도록 설계한다.                       */
/*                                                                            */
/*  저장 모델                                                                 */
/*  - logical blob 1개 = 4KB sector 2개(A/B mirror)                           */
/*  - 저장 시 inactive copy를 먼저 erase/program/verify 후                    */
/*    마지막에 commit 가능한 header를 기록한다.                               */
/*  - load 시 A/B 둘 다 검사한 뒤                                             */
/*    1) header magic/version/schema/app/size                                 */
/*    2) header CRC                                                           */
/*    3) payload CRC                                                          */
/*    4) commit magic                                                         */
/*    을 모두 만족하는 사본만 유효로 본다.                                    */
/*  - 둘 다 유효하면 sequence가 큰 쪽을 채택한다.                             */
/* -------------------------------------------------------------------------- */

#ifndef APP_FLASHSTORE_APP_ID_VARIO
#define APP_FLASHSTORE_APP_ID_VARIO 0x56415249u /* 'VARI' */
#endif

#ifndef APP_FLASHSTORE_APP_ID_MOTOR
#define APP_FLASHSTORE_APP_ID_MOTOR 0x4D4F544Fu /* 'MOTO' */
#endif

#ifndef APP_FLASHSTORE_SECTOR_SIZE
#define APP_FLASHSTORE_SECTOR_SIZE SPI_FLASH_SECTOR_SIZE_4K
#endif

typedef enum
{
    APP_FLASHSTORE_RESULT_OK = 0u,
    APP_FLASHSTORE_RESULT_NOT_READY,
    APP_FLASHSTORE_RESULT_NOT_PRESENT,
    APP_FLASHSTORE_RESULT_BAD_PARAM,
    APP_FLASHSTORE_RESULT_RANGE,
    APP_FLASHSTORE_RESULT_OWNER_MISMATCH,
    APP_FLASHSTORE_RESULT_NO_VALID_COPY,
    APP_FLASHSTORE_RESULT_CRC_FAIL,
    APP_FLASHSTORE_RESULT_SCHEMA_MISMATCH,
    APP_FLASHSTORE_RESULT_HAL_ERROR,
    APP_FLASHSTORE_RESULT_VERIFY_FAIL,
    APP_FLASHSTORE_RESULT_FORMAT_FAIL
} app_flashstore_result_t;

typedef struct
{
    /* ---------------------------------------------------------------------- */
    /*  owner_app_id                                                           */
    /*  - flash 맨 앞 owner marker에 기록되는 값                               */
    /*  - VARIO 빌드에서는 VARIO, MOTOR 빌드에서는 MOTOR를 넣는다.            */
    /* ---------------------------------------------------------------------- */
    uint32_t owner_app_id;

    /* ---------------------------------------------------------------------- */
    /*  layout_version                                                         */
    /*  - 논리 레이아웃이 깨지는 구조 변경 시 증가시킨다.                     */
    /* ---------------------------------------------------------------------- */
    uint32_t layout_version;

    /* ---------------------------------------------------------------------- */
    /*  managed_span_bytes                                                     */
    /*  - address 0부터 이 길이만큼을 현재 앱이 관리한다고 선언한다.          */
    /*  - owner mismatch 또는 superblock mismatch 시 이 span 전체를 format     */
    /*    해서 이전 앱 흔적을 정리한다.                                       */
    /* ---------------------------------------------------------------------- */
    uint32_t managed_span_bytes;
} app_flashstore_layout_t;

typedef struct
{
    uint32_t region_id;
    uint32_t schema_id;
    uint32_t copy0_address;
    uint32_t copy1_address;
    uint32_t payload_capacity_bytes;
} app_flashstore_region_t;

typedef struct
{
    bool                     initialized;
    bool                     present;
    bool                     ready;
    uint32_t                 owner_app_id;
    uint32_t                 layout_version;
    uint32_t                 managed_span_bytes;
    uint32_t                 capacity_bytes;
    app_flashstore_result_t  last_result;
} app_flashstore_snapshot_t;

void APP_FlashStore_Init(void);
app_flashstore_result_t APP_FlashStore_AttachLayout(const app_flashstore_layout_t *layout);
bool APP_FlashStore_IsReady(void);
bool APP_FlashStore_IsPresent(void);
void APP_FlashStore_CopySnapshot(app_flashstore_snapshot_t *out_snapshot);
app_flashstore_result_t APP_FlashStore_GetLastResult(void);
const char *APP_FlashStore_GetResultText(app_flashstore_result_t result);

app_flashstore_result_t APP_FlashStore_Load(const app_flashstore_region_t *region,
                                            void *dst_payload,
                                            uint32_t dst_capacity_bytes,
                                            uint32_t *out_payload_size,
                                            uint32_t *out_sequence);

app_flashstore_result_t APP_FlashStore_Save(const app_flashstore_region_t *region,
                                            const void *src_payload,
                                            uint32_t payload_size,
                                            uint32_t *inout_sequence);

app_flashstore_result_t APP_FlashStore_Clear(const app_flashstore_region_t *region);
app_flashstore_result_t APP_FlashStore_EraseSpan(uint32_t start_address,
                                                 uint32_t length_bytes);

#ifdef __cplusplus
}
#endif

#endif /* APP_FLASHSTORE_H */
