#ifndef FW_FLASH_H
#define FW_FLASH_H

#include "main.h"
#include "fatfs.h"
#include <stdbool.h>
#include <stdint.h>

#include "FW_Package.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  FW_Flash                                                                   */
/*                                                                            */
/*  역할                                                                      */
/*  - 내장 Flash app 영역 전체 erase                                           */
/*  - SYSUPDATE.bin payload stream -> app 영역 program                         */
/*  - read-back verify + final CRC verify                                      */
/*  - app vector sanity check                                                  */
/*  - boot -> app jump                                                         */
/*  - 구버전 설치 시 W25Q16 전체 4KB sector erase                              */
/* -------------------------------------------------------------------------- */

typedef enum
{
    FW_FLASH_STAGE_IDLE          = 0u,
    FW_FLASH_STAGE_W25Q_ERASE    = 1u,
    FW_FLASH_STAGE_APP_ERASE     = 2u,
    FW_FLASH_STAGE_APP_PROGRAM   = 3u,
    FW_FLASH_STAGE_APP_VERIFY    = 4u
} fw_flash_stage_t;

typedef void (*fw_flash_progress_cb_t)(fw_flash_stage_t stage,
                                       uint32_t completed_units,
                                       uint32_t total_units,
                                       void *user_context);

bool FW_FLASH_IsAppVectorValid(void);
void FW_FLASH_JumpToApp(void);

HAL_StatusTypeDef FW_FLASH_EraseW25Q16All(fw_flash_progress_cb_t progress_cb,
                                          void *user_context);

HAL_StatusTypeDef FW_FLASH_InstallFromOpenFile(FIL *fp,
                                               const fw_package_header_t *header,
                                               fw_flash_progress_cb_t progress_cb,
                                               void *user_context);

#ifdef __cplusplus
}
#endif

#endif /* FW_FLASH_H */
