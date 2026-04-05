#ifndef FW_SD_H
#define FW_SD_H

#include "main.h"
#include "fatfs.h"
#include <stdbool.h>
#include <stdint.h>

#include "FW_Package.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  FW_SD                                                                      */
/*                                                                            */
/*  부트 모드에서는 APP_SD처럼 상시 hotplug 상태 머신을 둘 필요가 없다.         */
/*  대신 "scan 시점마다 정리 -> mount fresh -> package header 확인" 경로로    */
/*  단순하게 운용한다.                                                         */
/* -------------------------------------------------------------------------- */

typedef struct
{
    bool                card_present;
    bool                mount_ok;
    bool                file_found;
    bool                header_valid;
    uint8_t             last_bsp_init_status;
    FRESULT             last_mount_result;
    FRESULT             last_open_result;
    fw_package_result_t last_package_result;
    fw_package_header_t header;
} fw_sd_scan_result_t;

void FW_SD_InitRuntime(void);
void FW_SD_Unmount(void);
bool FW_SD_IsCardPresent(void);
void FW_SD_ScanUpdatePackage(fw_sd_scan_result_t *out_result);
fw_package_result_t FW_SD_OpenUpdatePackage(FIL *out_fp,
                                            fw_package_header_t *out_header);

#ifdef __cplusplus
}
#endif

#endif /* FW_SD_H */
