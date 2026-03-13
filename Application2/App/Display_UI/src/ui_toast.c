#include "ui_toast.h"

#include <string.h>

#define UI_TOAST_TEXT_MAX             63
#define UI_TOAST_BOX_H                13
#define UI_TOAST_TEXT_BASELINE_Y_OFF  10
#define UI_TOAST_ICON_Y_VISUAL_OFF    1

typedef struct
{
    bool active;
    uint32_t expire_ms;
    char text[UI_TOAST_TEXT_MAX + 1];
    const uint8_t *icon;
    uint8_t icon_w;
    uint8_t icon_h;
} ui_toast_state_t;

static ui_toast_state_t s_toast;

static void ui_toast_copy_text(char *dst, uint32_t dst_size, const char *src)
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

void UI_Toast_Init(void)
{
    memset(&s_toast, 0, sizeof(s_toast));
}

void UI_Toast_Show(const char *text,
                   const uint8_t *icon,
                   uint8_t icon_w,
                   uint8_t icon_h,
                   uint32_t now_ms,
                   uint32_t timeout_ms)
{
    ui_toast_copy_text(s_toast.text, sizeof(s_toast.text), text);
    s_toast.icon = icon;
    s_toast.icon_w = icon_w;
    s_toast.icon_h = icon_h;
    s_toast.active = true;
    s_toast.expire_ms = now_ms + ((timeout_ms == 0u) ? UI_TOAST_DEFAULT_TIMEOUT_MS : timeout_ms);
}

void UI_Toast_Hide(void)
{
    s_toast.active = false;
}

void UI_Toast_Task(uint32_t now_ms)
{
    if ((s_toast.active != false) &&
        ((int32_t)(now_ms - s_toast.expire_ms) >= 0))
    {
        s_toast.active = false;
    }
}

bool UI_Toast_IsVisible(void)
{
    return s_toast.active;
}

void UI_Toast_Draw(u8g2_t *u8g2, bool avoid_bottom_bar)
{
    uint8_t text_w;
    uint8_t icon_space = 0u;
    uint8_t box_w;
    int16_t x0;
    int16_t y0;
    int16_t text_x;

    if ((u8g2 == 0) || (s_toast.active == false))
    {
        return;
    }

    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);

    text_w = (uint8_t)u8g2_GetStrWidth(u8g2, s_toast.text);
    if ((s_toast.icon != 0) && (s_toast.icon_w > 0u) && (s_toast.icon_h > 0u))
    {
        icon_space = (uint8_t)(s_toast.icon_w + 4u);
    }

    box_w = (uint8_t)(text_w + icon_space + 10u);
    if (box_w < 40u)
    {
        box_w = 40u;
    }
    if (box_w > (UI_LCD_W - 8u))
    {
        box_w = (uint8_t)(UI_LCD_W - 8u);
    }

    x0 = (int16_t)((UI_LCD_W - box_w) / 2);
    y0 = (int16_t)(UI_LCD_H - UI_TOAST_BOX_H - 2);

    if (avoid_bottom_bar != false)
    {
        y0 -= UI_BOTTOMBAR_H;
    }

    /* ---------------------------------------------------------------------- */
    /*  토스트는 항상 불투명한 흰색 body 위에 검은 외곽선을 둔다.               */
    /* ---------------------------------------------------------------------- */
    u8g2_SetDrawColor(u8g2, 0);
    u8g2_DrawBox(u8g2, (u8g2_uint_t)x0, (u8g2_uint_t)y0, box_w, UI_TOAST_BOX_H);

    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawFrame(u8g2, (u8g2_uint_t)x0, (u8g2_uint_t)y0, box_w, UI_TOAST_BOX_H);

    text_x = (int16_t)(x0 + 5);

    if (icon_space != 0u)
    {
        int16_t icon_y;

        icon_y = (int16_t)(y0 + ((UI_TOAST_BOX_H - s_toast.icon_h) / 2));
        icon_y += UI_TOAST_ICON_Y_VISUAL_OFF;

        u8g2_DrawXBM(u8g2,
                     (u8g2_uint_t)(x0 + 4),
                     (u8g2_uint_t)icon_y,
                     s_toast.icon_w,
                     s_toast.icon_h,
                     s_toast.icon);
        text_x += icon_space;
    }

    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)text_x,
                 (u8g2_uint_t)(y0 + UI_TOAST_TEXT_BASELINE_Y_OFF),
                 s_toast.text);
}
