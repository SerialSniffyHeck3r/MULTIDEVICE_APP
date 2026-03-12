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

/* 현재 부트 로고를 외부에서 바꿀 수 있게 descriptor를 분리했다.
 * 나중에 Winbond flash에서 읽은 XBM 포인터를 여기에 넣으면 된다. */
void UI_Boot_SetLogo(const uint8_t *bits, uint16_t width, uint16_t height);
void UI_Boot_GetLogo(ui_boot_logo_t *out_logo);
void UI_Boot_Draw(u8g2_t *u8g2);

#ifdef __cplusplus
}
#endif

#endif /* UI_BOOT_MODULE_H */
