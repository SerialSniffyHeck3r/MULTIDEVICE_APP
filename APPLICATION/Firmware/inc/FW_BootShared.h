#ifndef FW_BOOT_SHARED_H
#define FW_BOOT_SHARED_H

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  BKPSRAM 기반 부트 제어 블록                                                */
/*                                                                            */
/*  목적                                                                      */
/*  - resident bootloader 와 main app 이 '재부팅을 넘어' 공유해야 하는 최소     */
/*    상태를 한 군데에 모은다.                                                 */
/*  - APP_FAULT 의 RTC backup register 로그와는 역할을 분리한다.               */
/*                                                                            */
/*  설계 포인트                                                               */
/*  - magic / version / crc32 를 둬서 backup domain 흔들림에도 유효성 검사가   */
/*    가능하게 한다.                                                           */
/*  - app 쪽에서는 boot pending / confirmed / installed version 업데이트를      */
/*    담당한다.                                                                */
/*  - bootloader 쪽에서는 진입 사유 판단 / update 정책 / IWDG reset 누적을      */
/*    담당한다.                                                                */
/* -------------------------------------------------------------------------- */

#define FW_BOOTCTRL_MAGIC                    (0x46574254u) /* 'FWBT' */
#define FW_BOOTCTRL_VERSION                  (0x00010001u)
#define FW_BOOTCTRL_BKPSRAM_BASE             (0x40024000u)
#define FW_BOOTCTRL_INSTALLED_VERSION_INIT   (0u)
#define FW_BOOTCTRL_IWDG_FAIL_LIMIT          (3u)

typedef enum
{
    FW_BOOT_REASON_NONE = 0u,
    FW_BOOT_REASON_USER_REQUEST = 1u,
    FW_BOOT_REASON_AUTO_UPDATE = 2u,
    FW_BOOT_REASON_APP_INVALID = 3u,
    FW_BOOT_REASON_APP_FAULT = 4u,
    FW_BOOT_REASON_IWDG_FAIL = 5u
} fw_boot_reason_t;

typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint32_t crc32;

    uint32_t requested_mode;
    uint32_t enter_reason;

    uint32_t app_boot_pending;
    uint32_t app_boot_confirmed;
    uint32_t iwdg_unconfirmed_reset_count;

    uint32_t installed_version_u32;
    uint32_t candidate_version_u32;

    uint32_t last_reset_flags;
    uint32_t last_error;

    uint32_t reserved0;
    uint32_t reserved1;
    uint32_t reserved2;
    uint32_t reserved3;
} fw_bootctrl_t;

uint32_t FW_BootShared_CalcCrc32(const void *data, uint32_t length);
void FW_BootShared_EnableBkpsramAccess(void);
fw_bootctrl_t *FW_BootShared_GetCtrl(void);
void FW_BootShared_LoadOrInit(fw_bootctrl_t *out_ctrl);
void FW_BootShared_Store(const fw_bootctrl_t *ctrl);

#ifdef __cplusplus
}
#endif

#endif /* FW_BOOT_SHARED_H */
