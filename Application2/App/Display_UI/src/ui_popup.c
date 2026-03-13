#include "ui_popup.h"

#include <string.h>

#define UI_POPUP_W             200
#define UI_POPUP_BOX_H         48
#define UI_POPUP_TITLE_H       9
#define UI_POPUP_TITLE_MAX     31
#define UI_POPUP_LINE_MAX      47

typedef struct
{
    bool active;
    uint32_t expire_ms;
    char title[UI_POPUP_TITLE_MAX + 1];
    char line1[UI_POPUP_LINE_MAX + 1];
    char line2[UI_POPUP_LINE_MAX + 1];
    const uint8_t *icon;
    uint8_t icon_w;
    uint8_t icon_h;
} ui_popup_state_t;

static ui_popup_state_t s_popup;

static void ui_popup_copy_text(char *dst, uint32_t dst_size, const char *src)
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

void UI_Popup_Init(void)
{
    memset(&s_popup, 0, sizeof(s_popup));
}

void UI_Popup_Show(const char *title,
                   const char *line1,
                   const char *line2,
                   const uint8_t *icon,
                   uint8_t icon_w,
                   uint8_t icon_h,
                   uint32_t now_ms,
                   uint32_t timeout_ms)
{
    ui_popup_copy_text(s_popup.title, sizeof(s_popup.title), title);
    ui_popup_copy_text(s_popup.line1, sizeof(s_popup.line1), line1);
    ui_popup_copy_text(s_popup.line2, sizeof(s_popup.line2), line2);

    s_popup.icon = icon;
    s_popup.icon_w = icon_w;
    s_popup.icon_h = icon_h;
    s_popup.active = true;
    s_popup.expire_ms = now_ms + ((timeout_ms == 0u) ? UI_POPUP_DEFAULT_TIMEOUT_MS : timeout_ms);
}

void UI_Popup_Hide(void)
{
    s_popup.active = false;
}

void UI_Popup_Task(uint32_t now_ms)
{
    if ((s_popup.active != false) &&
        ((int32_t)(now_ms - s_popup.expire_ms) >= 0))
    {
        s_popup.active = false;
    }
}

bool UI_Popup_IsVisible(void)
{
    return s_popup.active;
}

void UI_Popup_Draw(u8g2_t *u8g2)
{
    int16_t x0;
    int16_t y0;
    int16_t icon_x;
    int16_t icon_y;
    int16_t text_x;
    int16_t line1_y;
    int16_t line2_y;
    int16_t icon_area_w;

    if ((u8g2 == 0) || (s_popup.active == false))
    {
        return;
    }

    x0 = (int16_t)((UI_LCD_W - UI_POPUP_W) / 2);
    y0 = (int16_t)((UI_LCD_H - UI_POPUP_BOX_H) / 2);

    /* ---------------------------------------------------------------------- */
    /*  팝업은 body 전체를 먼저 white로 지워서 반드시 불투명하게 만든다.         */
    /*  그 위에 black frame / title band / icon frame / text를 올린다.         */
    /* ---------------------------------------------------------------------- */
    u8g2_SetDrawColor(u8g2, 0);
    u8g2_DrawBox(u8g2, (u8g2_uint_t)x0, (u8g2_uint_t)y0, UI_POPUP_W, UI_POPUP_BOX_H);

    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawFrame(u8g2, (u8g2_uint_t)x0, (u8g2_uint_t)y0, UI_POPUP_W, UI_POPUP_BOX_H);
    u8g2_DrawBox(u8g2, (u8g2_uint_t)x0, (u8g2_uint_t)y0, UI_POPUP_W, UI_POPUP_TITLE_H);

    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    u8g2_SetDrawColor(u8g2, 0);
    u8g2_DrawStr(u8g2, (u8g2_uint_t)(x0 + 4), (u8g2_uint_t)(y0 + 7), s_popup.title);

    u8g2_SetDrawColor(u8g2, 1);

    icon_area_w = 0;
    if ((s_popup.icon != 0) && (s_popup.icon_w > 0u) && (s_popup.icon_h > 0u))
    {
        icon_area_w = 36;
        u8g2_DrawFrame(u8g2,
                       (u8g2_uint_t)(x0 + 4),
                       (u8g2_uint_t)(y0 + UI_POPUP_TITLE_H + 2),
                       30u,
                       30u);

        icon_x = (int16_t)(x0 + 4 + ((30 - s_popup.icon_w) / 2));
        icon_y = (int16_t)(y0 + UI_POPUP_TITLE_H + 2 + ((30 - s_popup.icon_h) / 2));

        u8g2_DrawXBM(u8g2,
                     (u8g2_uint_t)icon_x,
                     (u8g2_uint_t)icon_y,
                     s_popup.icon_w,
                     s_popup.icon_h,
                     s_popup.icon);
    }

    /* ---------------------------------------------------------------------- */
    /*  오른쪽 텍스트는 line1 / line2 모두 완전히 같은 시작 x를 사용한다.        */
    /*  이전 패키지에서 발생했던 1행만 앞으로 튀는 버그를 여기서 차단한다.       */
    /* ---------------------------------------------------------------------- */
    text_x = (int16_t)(x0 + 8 + icon_area_w);
    line1_y = (int16_t)(y0 + 22);
    line2_y = (int16_t)(y0 + 34);

    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawStr(u8g2, (u8g2_uint_t)text_x, (u8g2_uint_t)line1_y, s_popup.line1);
    u8g2_DrawStr(u8g2, (u8g2_uint_t)text_x, (u8g2_uint_t)line2_y, s_popup.line2);
}
