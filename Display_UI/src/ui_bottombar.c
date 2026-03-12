#include "ui_bottombar.h"

#include <string.h>

/* -------------------------------------------------------------------------- */
/*  Fixed geometry                                                             */
/* -------------------------------------------------------------------------- */
#define BOTTOMBAR_SEG_COUNT   6
#define BOTTOMBAR_SEG_W       (UI_LCD_W / BOTTOMBAR_SEG_COUNT)
#define BOTTOMBAR_FONT        u8g2_font_5x8_tr

static ui_bottombar_mode_t s_mode = UI_BOTTOMBAR_MODE_BUTTONS;
static ui_bottom_button_t  s_buttons[UI_FKEY_COUNT];
static ui_bottom_message_t s_msg;

static void ui_bottombar_draw_buttons(u8g2_t *u8g2, uint32_t pressed_mask);
static void ui_bottombar_draw_message(u8g2_t *u8g2);

/* -------------------------------------------------------------------------- */
/*  Init                                                                       */
/* -------------------------------------------------------------------------- */
void UI_BottomBar_Init(void)
{
    memset(s_buttons, 0, sizeof(s_buttons));
    memset(&s_msg, 0, sizeof(s_msg));
    s_mode = UI_BOTTOMBAR_MODE_BUTTONS;

    UI_BottomBar_SetButton(UI_FKEY_1, "F1", UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButton(UI_FKEY_2, "F2", UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButton(UI_FKEY_3, "F3", UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButton(UI_FKEY_4, "F4", UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButton(UI_FKEY_5, "F5", UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButton(UI_FKEY_6, "F6", 0u);
}

void UI_BottomBar_SetMode(ui_bottombar_mode_t mode)
{
    s_mode = mode;
}

void UI_BottomBar_SetButton(ui_fkey_t key, const char *text, uint8_t flags)
{
    if ((uint32_t)key >= (uint32_t)UI_FKEY_COUNT)
    {
        return;
    }

    s_buttons[key].text = text;
    s_buttons[key].icon = 0;
    s_buttons[key].icon_w = 0u;
    s_buttons[key].icon_h = 0u;
    s_buttons[key].flags = (uint8_t)(flags & (uint8_t)(UI_BOTTOMBAR_FLAG_DIVIDER | UI_BOTTOMBAR_FLAG_INVERT));
}

void UI_BottomBar_SetButtonIcon4(ui_fkey_t key,
                                 const uint8_t *icon,
                                 uint8_t icon_w,
                                 uint8_t flags)
{
    if ((uint32_t)key >= (uint32_t)UI_FKEY_COUNT)
    {
        return;
    }

    s_buttons[key].text = 0;
    s_buttons[key].icon = icon;
    s_buttons[key].icon_w = icon_w;
    s_buttons[key].icon_h = 4u;
    s_buttons[key].flags = (uint8_t)(flags |
                                     UI_BOTTOMBAR_FLAG_ICON_4PX);
}

void UI_BottomBar_SetMessage(const ui_bottom_message_t *msg)
{
    if (msg == 0)
    {
        memset(&s_msg, 0, sizeof(s_msg));
        return;
    }

    s_msg = *msg;
}

void UI_BottomBar_Draw(u8g2_t *u8g2, uint32_t pressed_mask)
{
    if (u8g2 == 0)
    {
        return;
    }

    u8g2_SetFont(u8g2, BOTTOMBAR_FONT);
    u8g2_SetFontDirection(u8g2, 0);

    if (s_mode == UI_BOTTOMBAR_MODE_MESSAGE)
    {
        ui_bottombar_draw_message(u8g2);
    }
    else
    {
        ui_bottombar_draw_buttons(u8g2, pressed_mask);
    }
}

/* -------------------------------------------------------------------------- */
/*  Buttons draw                                                               */
/*                                                                            */
/*  배경은 전체 black bar로 깔고,                                               */
/*  - 기본 상태는 white text/icon on black                                     */
/*  - 눌린 버튼 또는 forced invert는 white background with black content       */
/*  로 표현한다.                                                               */
/* -------------------------------------------------------------------------- */
static void ui_bottombar_draw_buttons(u8g2_t *u8g2, uint32_t pressed_mask)
{
    uint8_t y0 = (uint8_t)(UI_LCD_H - UI_BOTTOMBAR_H);
    uint8_t text_y = (uint8_t)(UI_LCD_H - 1);

    int i;

    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawBox(u8g2, 0u, y0, UI_LCD_W, UI_BOTTOMBAR_H);

    for (i = 0; i < UI_FKEY_COUNT; ++i)
    {
        const ui_bottom_button_t *btn = &s_buttons[i];
        uint8_t seg_x = (uint8_t)(i * BOTTOMBAR_SEG_W);
        bool is_pressed = ((pressed_mask & (1u << i)) != 0u);
        bool inverted = (((btn->flags & UI_BOTTOMBAR_FLAG_INVERT) != 0u) || (is_pressed != false));
        uint8_t content_color = 0u;
        uint8_t divider_color = 0u;

        if (inverted != false)
        {
            u8g2_SetDrawColor(u8g2, 0);
            u8g2_DrawBox(u8g2, seg_x, y0, BOTTOMBAR_SEG_W, UI_BOTTOMBAR_H);
            content_color = 1u;
            divider_color = 1u;
        }

        if ((btn->flags & UI_BOTTOMBAR_FLAG_DIVIDER) != 0u)
        {
            uint8_t x_line = (uint8_t)(seg_x + BOTTOMBAR_SEG_W - 1u);
            u8g2_SetDrawColor(u8g2, divider_color);
            u8g2_DrawVLine(u8g2, x_line, y0, UI_BOTTOMBAR_H);
        }

        if (((btn->flags & UI_BOTTOMBAR_FLAG_ICON_4PX) != 0u) &&
            (btn->icon != 0) &&
            (btn->icon_w > 0u))
        {
            int16_t icon_x = (int16_t)seg_x + (int16_t)((BOTTOMBAR_SEG_W - btn->icon_w) / 2);
            int16_t icon_y = (int16_t)y0 + (int16_t)((UI_BOTTOMBAR_H - btn->icon_h) / 2);

            if (icon_x < (int16_t)seg_x + 1)
            {
                icon_x = (int16_t)seg_x + 1;
            }

            u8g2_SetDrawColor(u8g2, content_color);
            u8g2_DrawXBM(u8g2,
                         (u8g2_uint_t)icon_x,
                         (u8g2_uint_t)icon_y,
                         btn->icon_w,
                         btn->icon_h,
                         btn->icon);
        }
        else if ((btn->text != 0) && (btn->text[0] != '\0'))
        {
            uint8_t str_w = (uint8_t)u8g2_GetStrWidth(u8g2, btn->text);
            int16_t text_x = (int16_t)seg_x + (int16_t)((BOTTOMBAR_SEG_W - str_w) / 2);

            if (text_x < (int16_t)seg_x + 1)
            {
                text_x = (int16_t)seg_x + 1;
            }

            u8g2_SetDrawColor(u8g2, content_color);
            u8g2_DrawStr(u8g2, (u8g2_uint_t)text_x, text_y, btn->text);
        }
    }

    u8g2_SetDrawColor(u8g2, 1);
}

/* -------------------------------------------------------------------------- */
/*  Message mode draw                                                          */
/*                                                                            */
/*  프로토타입과 같은 철학으로, 왼쪽에 아이콘을 둘 수 있고                      */
/*  텍스트는 black bar 안에서 white로 출력한다.                                */
/* -------------------------------------------------------------------------- */
static void ui_bottombar_draw_message(u8g2_t *u8g2)
{
    uint8_t y0 = (uint8_t)(UI_LCD_H - UI_BOTTOMBAR_H);
    uint8_t text_y = (uint8_t)(UI_LCD_H - 1);
    uint8_t icon_space = 0u;
    uint8_t icon_x = 0u;
    uint8_t icon_y = 0u;

    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawBox(u8g2, 0u, y0, UI_LCD_W, UI_BOTTOMBAR_H);

    if ((s_msg.text == 0) && (s_msg.icon == 0))
    {
        return;
    }

    if ((s_msg.icon != 0) && (s_msg.icon_w > 0u) && (s_msg.icon_h > 0u))
    {
        icon_space = (uint8_t)(s_msg.icon_w + 2u);
        icon_x = 1u;

        if (s_msg.icon_h >= UI_BOTTOMBAR_H)
        {
            icon_y = y0;
        }
        else
        {
            icon_y = (uint8_t)(y0 + (uint8_t)((UI_BOTTOMBAR_H - s_msg.icon_h) / 2u));
        }

        u8g2_SetDrawColor(u8g2, 0);
        u8g2_DrawXBM(u8g2, icon_x, icon_y, s_msg.icon_w, s_msg.icon_h, s_msg.icon);
    }

    if ((s_msg.text != 0) && (s_msg.text[0] != '\0'))
    {
        uint8_t str_w = (uint8_t)u8g2_GetStrWidth(u8g2, s_msg.text);
        int16_t text_x;

        if (icon_space != 0u)
        {
            text_x = (int16_t)(icon_x + icon_space);
        }
        else
        {
            text_x = (int16_t)((UI_LCD_W - str_w) / 2);
        }

        if (text_x < 1)
        {
            text_x = 1;
        }

        u8g2_SetDrawColor(u8g2, 0);
        u8g2_DrawStr(u8g2, (u8g2_uint_t)text_x, text_y, s_msg.text);
    }

    u8g2_SetDrawColor(u8g2, 1);
}
