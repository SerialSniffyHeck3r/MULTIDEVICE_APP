#ifndef UI_TOAST_MODULE_H
#define UI_TOAST_MODULE_H

#include "ui_types.h"
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

void UI_Toast_Init(void);
void UI_Toast_Show(const char *text,
                   const uint8_t *icon,
                   uint8_t icon_w,
                   uint8_t icon_h,
                   uint32_t now_ms,
                   uint32_t timeout_ms);
void UI_Toast_Hide(void);
void UI_Toast_Task(uint32_t now_ms);
bool UI_Toast_IsVisible(void);
void UI_Toast_Draw(u8g2_t *u8g2, bool avoid_bottom_bar);

#ifdef __cplusplus
}
#endif

#endif /* UI_TOAST_MODULE_H */
