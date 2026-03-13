#ifndef UI_BOOT_MODULE_H
#define UI_BOOT_MODULE_H

#include "ui_types.h"
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    const uint8_t *bits;
    uint16_t width;
    uint16_t height;
} ui_boot_logo_t;

/* -------------------------------------------------------------------------- */
/*  Boot logo API                                                              */
/*                                                                            */
/*  - 기본 로고는 200x80 검정 XBM이다.                                          */
/*  - 나중에 Winbond flash에서 읽은 사용자 로고 XBM 포인터를                    */
/*    그대로 넘겨도 되도록 descriptor 방식으로 분리했다.                        */
/* -------------------------------------------------------------------------- */
void UI_Boot_SetLogo(const uint8_t *bits, uint16_t width, uint16_t height);
void UI_Boot_GetLogo(ui_boot_logo_t *out_logo);
void UI_Boot_Draw(u8g2_t *u8g2);

#ifdef __cplusplus
}
#endif

#endif /* UI_BOOT_MODULE_H */
