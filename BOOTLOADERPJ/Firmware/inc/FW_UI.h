#ifndef FW_UI_H
#define FW_UI_H

#include "main.h"
#include <stdbool.h>
#include <stdint.h>

#include "FW_BootCtl.h"
#include "FW_Flash.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  FW_UI                                                                      */
/*                                                                            */
/*  F/W Update Mode Boot 전용 텍스트 UI 모델                                   */
/* -------------------------------------------------------------------------- */

typedef struct
{
    fw_boot_reason_t reason;
    char             version_string[FW_VERSION_STRING_MAX];
    bool             package_ready;
    uint8_t          progress_percent;
    fw_flash_stage_t flash_stage;
} fw_ui_view_t;

void FW_UI_Init(void);
void FW_UI_Draw(const fw_ui_view_t *view);

#ifdef __cplusplus
}
#endif

#endif /* FW_UI_H */
