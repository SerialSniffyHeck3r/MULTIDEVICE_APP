#ifndef FW_BOOT_SHARED_H
#define FW_BOOT_SHARED_H

#include "main.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  FW_BootShared                                                              */
/*                                                                            */
/*  아주 중요한 규칙                                                           */
/*  - 이 파일의 magic / version / struct layout / CRC 방식은                  */
/*    BOOTLOADERPJ/Firmware/inc/FW_BootCtl.h 와 byte 단위로 같아야 한다.      */
/*  - 둘 중 한쪽만 바꾸면 bootloader 와 app 가 서로의 BKPSRAM 데이터를       */
/*    읽지 못하게 된다.                                                        */
/*                                                                            */
/*  이번 교체본의 목적                                                          */
/*  - APPLICATION 쪽의 옛 struct 형식과 bootloader 쪽 형식이 달라서           */
/*    pending / confirmed / installed version 공유가 깨지던 문제를 없앤다.    */
/*  - app 쪽 public 함수 이름은 FW_BootShared_* 그대로 유지해서               */
/*    기존 include / call site를 건드리지 않게 한다.                          */
/* -------------------------------------------------------------------------- */

#ifndef FW_VERSION_STRING_MAX
#define FW_VERSION_STRING_MAX 16u
#endif

#ifndef FW_BOOTCTRL_MAGIC
#define FW_BOOTCTRL_MAGIC 0x42544346u /* bootloader FW_BOOTCTL_MAGIC 와 동일 */
#endif

#ifndef FW_BOOTCTRL_VERSION
#define FW_BOOTCTRL_VERSION 0x0001u   /* bootloader FW_BOOTCTL_VERSION 와 동일 */
#endif

#ifndef FW_BOOTCTRL_BKPSRAM_BASE
#define FW_BOOTCTRL_BKPSRAM_BASE BKPSRAM_BASE
#endif

#ifndef FW_BOOTCTRL_INSTALLED_VERSION_INIT
#define FW_BOOTCTRL_INSTALLED_VERSION_INIT 0u
#endif

#ifndef FW_BOOTCTRL_IWDG_FAIL_LIMIT
#define FW_BOOTCTRL_IWDG_FAIL_LIMIT 3u
#endif

typedef enum
{
    FW_BOOT_REQUEST_NONE = 0u,
    FW_BOOT_REQUEST_FW_FLASH_MODE = 1u
} fw_boot_request_t;

typedef enum
{
    FW_BOOT_REASON_NONE = 0u,
    FW_BOOT_REASON_USER = 1u,
    FW_BOOT_REASON_UPDATE_NEW = 2u,
    FW_BOOT_REASON_UPDATE_OLD = 3u,
    FW_BOOT_REASON_BOOT_FAIL = 4u,
    FW_BOOT_REASON_INVALID_APP = 5u
} fw_boot_reason_t;

/* -------------------------------------------------------------------------- */
/*  예전 APPLICATION 쪽 enum 이름과의 호환 alias                              */
/* -------------------------------------------------------------------------- */
#define FW_BOOT_REASON_USER_REQUEST FW_BOOT_REASON_USER
#define FW_BOOT_REASON_AUTO_UPDATE  FW_BOOT_REASON_UPDATE_NEW
#define FW_BOOT_REASON_APP_INVALID  FW_BOOT_REASON_INVALID_APP
#define FW_BOOT_REASON_APP_FAULT    FW_BOOT_REASON_BOOT_FAIL
#define FW_BOOT_REASON_IWDG_FAIL    FW_BOOT_REASON_BOOT_FAIL

/* -------------------------------------------------------------------------- */
/*  bootloader 와 완전히 같은 packed control block                            */
/* -------------------------------------------------------------------------- */
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

typedef fw_boot_control_t fw_bootctrl_t;

uint32_t FW_BootShared_CalcCrc32(const void *data, uint32_t length);
void FW_BootShared_EnableBkpsramAccess(void);
fw_bootctrl_t *FW_BootShared_GetCtrl(void);
void FW_BootShared_LoadOrInit(fw_bootctrl_t *out_ctrl);
void FW_BootShared_Store(const fw_bootctrl_t *ctrl);

#ifdef __cplusplus
}
#endif

#endif /* FW_BOOT_SHARED_H */
