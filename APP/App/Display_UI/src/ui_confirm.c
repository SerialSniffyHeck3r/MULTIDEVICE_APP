#include "ui_confirm.h"

#include <string.h>

#define UI_CONFIRM_BOX_W      208
#define UI_CONFIRM_BOX_H       82
#define UI_CONFIRM_TITLE_MAX   31
#define UI_CONFIRM_BODY_MAX    47
#define UI_CONFIRM_OPTION_MAX  23

typedef struct
{
    bool active;
    uint16_t context_id;
    char title[UI_CONFIRM_TITLE_MAX + 1];
    char body[UI_CONFIRM_BODY_MAX + 1];
    char option1[UI_CONFIRM_OPTION_MAX + 1];
    char option2[UI_CONFIRM_OPTION_MAX + 1];
    char option3[UI_CONFIRM_OPTION_MAX + 1];
} ui_confirm_state_t;

static ui_confirm_state_t s_confirm;

static void ui_confirm_copy_text(char *dst, uint32_t dst_size, const char *src)
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

static void ui_confirm_draw_centered(u8g2_t *u8g2,
                                     const uint8_t *font,
                                     int16_t center_x,
                                     int16_t y_baseline,
                                     const char *text)
{
    int16_t text_w;

    if ((u8g2 == 0) || (font == 0) || (text == 0))
    {
        return;
    }

    u8g2_SetFont(u8g2, font);
    text_w = (int16_t)u8g2_GetStrWidth(u8g2, text);
    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)(center_x - (text_w / 2)),
                 (u8g2_uint_t)y_baseline,
                 text);
}

void UI_Confirm_Init(void)
{
    memset(&s_confirm, 0, sizeof(s_confirm));
}

void UI_Confirm_Show(const char *title,
                     const char *body,
                     const char *option1,
                     const char *option2,
                     const char *option3,
                     uint16_t context_id,
                     uint32_t now_ms)
{
    (void)now_ms;

    memset(&s_confirm, 0, sizeof(s_confirm));
    ui_confirm_copy_text(s_confirm.title, sizeof(s_confirm.title), title);
    ui_confirm_copy_text(s_confirm.body, sizeof(s_confirm.body), body);
    ui_confirm_copy_text(s_confirm.option1, sizeof(s_confirm.option1), option1);
    ui_confirm_copy_text(s_confirm.option2, sizeof(s_confirm.option2), option2);
    ui_confirm_copy_text(s_confirm.option3, sizeof(s_confirm.option3), option3);
    s_confirm.context_id = context_id;
    s_confirm.active = true;
}

void UI_Confirm_Hide(void)
{
    s_confirm.active = false;
}

bool UI_Confirm_IsVisible(void)
{
    return s_confirm.active;
}

uint16_t UI_Confirm_GetContextId(void)
{
    return s_confirm.context_id;
}

void UI_Confirm_Draw(u8g2_t *u8g2)
{
    int16_t x0;
    int16_t y0;
    int16_t cx;

    if ((u8g2 == 0) || (s_confirm.active == false))
    {
        return;
    }

    x0 = (int16_t)((UI_LCD_W - UI_CONFIRM_BOX_W) / 2);
    y0 = (int16_t)((UI_LCD_H - UI_CONFIRM_BOX_H) / 2);
    cx = (int16_t)(x0 + (UI_CONFIRM_BOX_W / 2));

    u8g2_SetDrawColor(u8g2, 0);
    u8g2_DrawBox(u8g2, (u8g2_uint_t)x0, (u8g2_uint_t)y0, UI_CONFIRM_BOX_W, UI_CONFIRM_BOX_H);

    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawFrame(u8g2, (u8g2_uint_t)x0, (u8g2_uint_t)y0, UI_CONFIRM_BOX_W, UI_CONFIRM_BOX_H);
    u8g2_DrawHLine(u8g2,
                   (u8g2_uint_t)(x0 + 6),
                   (u8g2_uint_t)(y0 + 22),
                   (u8g2_uint_t)(UI_CONFIRM_BOX_W - 12));

    ui_confirm_draw_centered(u8g2, u8g2_font_6x12_mf, cx, (int16_t)(y0 + 15), s_confirm.title);
    ui_confirm_draw_centered(u8g2, u8g2_font_5x8_tr,  cx, (int16_t)(y0 + 33), s_confirm.body);
    ui_confirm_draw_centered(u8g2, u8g2_font_6x12_mf, cx, (int16_t)(y0 + 48), s_confirm.option1);
    ui_confirm_draw_centered(u8g2, u8g2_font_6x12_mf, cx, (int16_t)(y0 + 60), s_confirm.option2);
    ui_confirm_draw_centered(u8g2, u8g2_font_6x12_mf, cx, (int16_t)(y0 + 72), s_confirm.option3);
    ui_confirm_draw_centered(u8g2,
                             u8g2_font_5x7_tr,
                             cx,
                             (int16_t)(y0 + UI_CONFIRM_BOX_H - 4),
                             "LONG PRESS FN KEY TO CHOOSE!");
}
