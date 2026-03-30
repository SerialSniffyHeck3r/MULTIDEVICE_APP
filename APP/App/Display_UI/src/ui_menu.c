#include "ui_menu.h"
#include "ui_popup.h"
#include "ui_toast.h"
#include "ui_confirm.h"

#include <string.h>

#define UI_MENU_BOX_W                180
#define UI_MENU_TITLE_H               10
#define UI_MENU_ROW_H                 14
#define UI_MENU_TITLE_MAX             31
#define UI_MENU_TEXT_MAX              31
#define UI_MENU_SAFE_MARGIN_X          8
#define UI_MENU_SAFE_TOP_Y            (UI_STATUSBAR_H + UI_STATUSBAR_GAP_H + 2)
#define UI_MENU_SAFE_BOTTOM_RESERVED  (UI_BOTTOMBAR_H + UI_BOTTOMBAR_GAP_H + 3)

typedef struct
{
    bool active;
    uint8_t count;
    uint8_t selected;
    char title[UI_MENU_TITLE_MAX + 1];
    char text[UI_MENU_MAX_ITEMS][UI_MENU_TEXT_MAX + 1];
    uint16_t action[UI_MENU_MAX_ITEMS];
} ui_menu_state_t;

static ui_menu_state_t s_menu;

static void ui_menu_compute_box(int16_t *out_x0,
                                int16_t *out_y0,
                                int16_t *out_w,
                                int16_t *out_h)
{
    int16_t box_w;
    int16_t box_h;
    int16_t safe_top;
    int16_t safe_bottom;
    int16_t safe_h;
    int16_t x0;
    int16_t y0;

    box_w = UI_MENU_BOX_W;
    if (box_w > (int16_t)(UI_LCD_W - (2 * UI_MENU_SAFE_MARGIN_X)))
    {
        box_w = (int16_t)(UI_LCD_W - (2 * UI_MENU_SAFE_MARGIN_X));
    }

    box_h = (int16_t)(UI_MENU_TITLE_H + 4 + ((int16_t)s_menu.count * UI_MENU_ROW_H));
    if (box_h < 28)
    {
        box_h = 28;
    }

    safe_top = UI_MENU_SAFE_TOP_Y;
    safe_bottom = (int16_t)(UI_LCD_H - UI_MENU_SAFE_BOTTOM_RESERVED);
    if (safe_bottom < safe_top)
    {
        safe_bottom = safe_top;
    }

    safe_h = (int16_t)(safe_bottom - safe_top);
    x0 = (int16_t)((UI_LCD_W - box_w) / 2);
    y0 = (int16_t)(safe_top + ((safe_h - box_h) / 2));
    if (y0 < safe_top)
    {
        y0 = safe_top;
    }

    if (out_x0 != NULL)
    {
        *out_x0 = x0;
    }
    if (out_y0 != NULL)
    {
        *out_y0 = y0;
    }
    if (out_w != NULL)
    {
        *out_w = box_w;
    }
    if (out_h != NULL)
    {
        *out_h = box_h;
    }
}

static void ui_menu_copy_text(char *dst, uint32_t dst_size, const char *src)
{
    if ((dst == 0) || (dst_size == 0u))
    {
        return;
    }

    if (src == 0)
    {
        dst[0] = '\0';
        return;
    }

    strncpy(dst, src, dst_size - 1u);
    dst[dst_size - 1u] = '\0';
}

void UI_Menu_Init(void)
{
    memset(&s_menu, 0, sizeof(s_menu));
}

void UI_Menu_Show(const char *title,
                  const ui_menu_item_t *items,
                  uint8_t item_count,
                  uint8_t selected_index)
{
    uint8_t i;

    memset(&s_menu, 0, sizeof(s_menu));
    UI_Popup_Hide();
    UI_Toast_Hide();
    UI_Confirm_Hide();
    ui_menu_copy_text(s_menu.title, sizeof(s_menu.title), title);

    if (item_count > UI_MENU_MAX_ITEMS)
    {
        item_count = UI_MENU_MAX_ITEMS;
    }

    s_menu.count = item_count;
    for (i = 0u; i < item_count; ++i)
    {
        ui_menu_copy_text(s_menu.text[i], sizeof(s_menu.text[i]), items[i].text);
        s_menu.action[i] = items[i].action_id;
    }

    if (item_count == 0u)
    {
        s_menu.selected = 0u;
    }
    else if (selected_index >= item_count)
    {
        s_menu.selected = 0u;
    }
    else
    {
        s_menu.selected = selected_index;
    }

    s_menu.active = true;
}

void UI_Menu_Hide(void)
{
    s_menu.active = false;
}

bool UI_Menu_IsVisible(void)
{
    return s_menu.active;
}

uint8_t UI_Menu_GetSelectedIndex(void)
{
    return s_menu.selected;
}

uint16_t UI_Menu_GetSelectedAction(void)
{
    if ((s_menu.active == false) || (s_menu.selected >= s_menu.count))
    {
        return 0u;
    }

    return s_menu.action[s_menu.selected];
}

void UI_Menu_MoveSelection(int8_t direction)
{
    int16_t next;

    if ((s_menu.active == false) || (s_menu.count == 0u))
    {
        return;
    }

    next = (int16_t)s_menu.selected + ((direction >= 0) ? 1 : -1);
    if (next < 0)
    {
        next = (int16_t)s_menu.count - 1;
    }
    else if (next >= (int16_t)s_menu.count)
    {
        next = 0;
    }

    s_menu.selected = (uint8_t)next;
}

void UI_Menu_Draw(u8g2_t *u8g2)
{
    int16_t box_w;
    int16_t box_h;
    int16_t x0;
    int16_t y0;
    uint8_t i;

    if ((u8g2 == 0) || (s_menu.active == false))
    {
        return;
    }

    ui_menu_compute_box(&x0, &y0, &box_w, &box_h);

    u8g2_SetDrawColor(u8g2, 0);
    u8g2_DrawBox(u8g2, (u8g2_uint_t)x0, (u8g2_uint_t)y0, (u8g2_uint_t)box_w, (u8g2_uint_t)box_h);

    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawFrame(u8g2, (u8g2_uint_t)x0, (u8g2_uint_t)y0, (u8g2_uint_t)box_w, (u8g2_uint_t)box_h);
    u8g2_DrawBox(u8g2, (u8g2_uint_t)x0, (u8g2_uint_t)y0, (u8g2_uint_t)box_w, UI_MENU_TITLE_H);

    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    u8g2_SetDrawColor(u8g2, 0);
    u8g2_DrawStr(u8g2, (u8g2_uint_t)(x0 + 4), (u8g2_uint_t)(y0 + 8), s_menu.title);

    for (i = 0u; i < s_menu.count; ++i)
    {
        int16_t row_y;
        row_y = (int16_t)(y0 + UI_MENU_TITLE_H + 2 + ((int16_t)i * UI_MENU_ROW_H));

        if (i == s_menu.selected)
        {
            u8g2_SetDrawColor(u8g2, 1);
            u8g2_DrawBox(u8g2,
                         (u8g2_uint_t)(x0 + 4),
                         (u8g2_uint_t)row_y,
                         (u8g2_uint_t)(box_w - 8),
                         (u8g2_uint_t)(UI_MENU_ROW_H - 2));
            u8g2_SetDrawColor(u8g2, 0);
        }
        else
        {
            u8g2_SetDrawColor(u8g2, 1);
            u8g2_DrawFrame(u8g2,
                           (u8g2_uint_t)(x0 + 4),
                           (u8g2_uint_t)row_y,
                           (u8g2_uint_t)(box_w - 8),
                           (u8g2_uint_t)(UI_MENU_ROW_H - 2));
        }

        u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
        u8g2_DrawStr(u8g2,
                     (u8g2_uint_t)(x0 + 10),
                     (u8g2_uint_t)(row_y + 10),
                     s_menu.text[i]);
        u8g2_SetDrawColor(u8g2, 1);
    }
}
