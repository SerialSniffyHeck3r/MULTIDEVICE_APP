#ifndef UI_MENU_MODULE_H
#define UI_MENU_MODULE_H

#include "ui_types.h"
#include "u8g2.h"

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifndef UI_MENU_MAX_ITEMS
#define UI_MENU_MAX_ITEMS 8u
#endif

typedef struct
{
    const char *text;
    uint16_t action_id;
} ui_menu_item_t;

void UI_Menu_Init(void);
void UI_Menu_Show(const char *title,
                  const ui_menu_item_t *items,
                  uint8_t item_count,
                  uint8_t selected_index);
void UI_Menu_Hide(void);
bool UI_Menu_IsVisible(void);
uint8_t UI_Menu_GetSelectedIndex(void);
uint16_t UI_Menu_GetSelectedAction(void);
void UI_Menu_MoveSelection(int8_t direction);
void UI_Menu_Draw(u8g2_t *u8g2);

#ifdef __cplusplus
}
#endif

#endif /* UI_MENU_MODULE_H */
