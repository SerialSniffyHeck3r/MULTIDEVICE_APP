#ifndef FW_BOOTCTL_H
#define FW_BOOTCTL_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

#include "FW_BootConfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  FW_BootCtl                                                                 */
/*                                                                            */
/*  RTC backup register는 APP_FAULT가 이미 사고기록용으로 점유하고 있으므로,    */
/*  F/W Update Mode Boot의 제어 상태는 BKPSRAM 4KB를 사용한다.                */
/*                                                                            */
/*  저장 내용                                                                  */
/*  - 마지막 reset cause snapshot                                             */
/*  - IWDG 미확인 부팅 누적 횟수                                               */
/*  - 현재 설치된 앱 버전                                                      */
/*  - 다음 부팅 요청 모드                                                      */
/*  - app jump 직전 pending / app confirm 상태                                */
/* -------------------------------------------------------------------------- */

#define FW_BOOTCTL_MAGIC            0x42544346u   /* 'FCTB' */
#define FW_BOOTCTL_VERSION          0x0001u

typedef enum
{
    FW_BOOT_REQUEST_NONE          = 0u,
    FW_BOOT_REQUEST_FW_FLASH_MODE = 1u
} fw_boot_request_t;

typedef enum
{
    FW_BOOT_REASON_NONE        = 0u,
    FW_BOOT_REASON_USER        = 1u,
    FW_BOOT_REASON_UPDATE_NEW  = 2u,
    FW_BOOT_REASON_UPDATE_OLD  = 3u,
    FW_BOOT_REASON_BOOT_FAIL   = 4u,
    FW_BOOT_REASON_INVALID_APP = 5u
} fw_boot_reason_t;

typedef struct __attribute__((packed))
{
    uint32_t magic;
    uint16_t version;
    uint16_t struct_size;

    uint32_t requested_mode;
    uint32_t requested_reason;

    uint32_t last_reset_flags;
    uint32_t iwdg_unconfirmed_reset_count;

    uint32_t app_boot_pending;
    uint32_t app_boot_confirmed;

    uint32_t installed_version_u32;
    char     installed_version_string[FW_VERSION_STRING_MAX];

    uint32_t last_seen_package_version_u32;
    char     last_seen_package_version_string[FW_VERSION_STRING_MAX];

    uint32_t reserved[8];
    uint32_t crc32;
} fw_boot_control_t;

void FW_BOOTCTL_InitStorage(void);
void FW_BOOTCTL_ResetToDefaults(fw_boot_control_t *ctl);
bool FW_BOOTCTL_Load(fw_boot_control_t *out_ctl);
HAL_StatusTypeDef FW_BOOTCTL_Store(const fw_boot_control_t *ctl);

uint32_t FW_BOOTCTL_ReadResetFlags(void);
void FW_BOOTCTL_ClearResetFlags(void);

#ifdef __cplusplus
}
#endif

#endif /* FW_BOOTCTL_H */
