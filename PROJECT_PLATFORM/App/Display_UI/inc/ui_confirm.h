#ifndef UI_CONFIRM_MODULE_H
#define UI_CONFIRM_MODULE_H

#include "ui_types.h"
#include "u8g2.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

void UI_Confirm_Init(void);
void UI_Confirm_Show(const char *title,
                     const char *body,
                     const char *option1,
                     const char *option2,
                     const char *option3,
                     uint16_t context_id,
                     uint32_t now_ms);
void UI_Confirm_Hide(void);
bool UI_Confirm_IsVisible(void);
uint16_t UI_Confirm_GetContextId(void);
void UI_Confirm_Draw(u8g2_t *u8g2);

#ifdef __cplusplus
}
#endif

#endif /* UI_CONFIRM_MODULE_H */
