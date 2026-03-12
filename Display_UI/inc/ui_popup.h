#ifndef UI_POPUP_MODULE_H
#define UI_POPUP_MODULE_H

#include "ui_types.h"
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

void UI_Popup_Init(void);
void UI_Popup_Show(const char *title,
                   const char *line1,
                   const char *line2,
                   const uint8_t *icon,
                   uint8_t icon_w,
                   uint8_t icon_h,
                   uint32_t now_ms,
                   uint32_t timeout_ms);
void UI_Popup_Hide(void);
void UI_Popup_Task(uint32_t now_ms);
bool UI_Popup_IsVisible(void);
void UI_Popup_Draw(u8g2_t *u8g2);

#ifdef __cplusplus
}
#endif

#endif /* UI_POPUP_MODULE_H */
