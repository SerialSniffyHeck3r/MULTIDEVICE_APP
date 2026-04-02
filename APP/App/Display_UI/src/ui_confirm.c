#include "ui_confirm.h"
#include "ui_popup.h"
#include "ui_toast.h"
#include "ui_menu.h"

#include <string.h>

#define UI_CONFIRM_BOX_W              208
#define UI_CONFIRM_BOX_H               90
#define UI_CONFIRM_TITLE_MAX           31
#define UI_CONFIRM_BODY_MAX            47
#define UI_CONFIRM_OPTION_MAX          23
#define UI_CONFIRM_SAFE_MARGIN_X        8
#define UI_CONFIRM_SAFE_TOP_Y          (UI_STATUSBAR_H + UI_STATUSBAR_GAP_H + 2)
#define UI_CONFIRM_SAFE_BOTTOM_RESERVED (UI_BOTTOMBAR_H + UI_BOTTOMBAR_GAP_H + 3)

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

static void ui_confirm_compute_box(int16_t *out_x0,
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

    box_w = UI_CONFIRM_BOX_W;
    if (box_w > (int16_t)(UI_LCD_W - (2 * UI_CONFIRM_SAFE_MARGIN_X)))
    {
        box_w = (int16_t)(UI_LCD_W - (2 * UI_CONFIRM_SAFE_MARGIN_X));
    }

    box_h = UI_CONFIRM_BOX_H;
    safe_top = UI_CONFIRM_SAFE_TOP_Y;
    safe_bottom = (int16_t)(UI_LCD_H - UI_CONFIRM_SAFE_BOTTOM_RESERVED);
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
    UI_Popup_Hide();
    UI_Toast_Hide();
    UI_Menu_Hide();
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
    int16_t box_w;
    int16_t box_h;
    int16_t x0;
    int16_t y0;
    int16_t cx;

    if ((u8g2 == 0) || (s_confirm.active == false))
    {
        return;
    }

    ui_confirm_compute_box(&x0, &y0, &box_w, &box_h);
    cx = (int16_t)(x0 + (box_w / 2));

    u8g2_SetDrawColor(u8g2, 0);
    u8g2_DrawBox(u8g2, (u8g2_uint_t)x0, (u8g2_uint_t)y0, (u8g2_uint_t)box_w, (u8g2_uint_t)box_h);

    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawFrame(u8g2, (u8g2_uint_t)x0, (u8g2_uint_t)y0, (u8g2_uint_t)box_w, (u8g2_uint_t)box_h);
    u8g2_DrawHLine(u8g2,
                   (u8g2_uint_t)(x0 + 6),
                   (u8g2_uint_t)(y0 + 22),
                   (u8g2_uint_t)(box_w - 12));

    /* ------------------------------------------------------------------ */
    /* CONFIRM 엔진 공통 레이아웃                                           */
    /*                                                                    */
    /* CLEAR SITE 같은 3-option confirm 에서는                             */
    /* - option3 줄                                                        */
    /* - LONG PRESS 안내 줄                                                */
    /* 이 서로 겹치지 않도록 엔진 레벨에서 spacing 을 보장해야 한다.        */
    /*                                                                    */
    /* 구현 방침                                                            */
    /* - box 높이를 90 px 로 올려 하단 안내 band 를 따로 확보한다.          */
    /* - option3 baseline 은 기존 기억을 크게 깨지 않게 y0 + 72 를 유지한다.*/
    /* - 안내 문구 baseline 은 y0 + box_h - 5 로 내려, option3 와의        */
    /*   수직 간격을 충분히 둔다.                                           */
    /*                                                                    */
    /* 튜닝 메모                                                            */
    /* - 82 px 에서는 6x12 / 5x7 조합이 하단에서 맞부딪힐 수 있다.          */
    /* - 90 px 는 128 px 화면의 safe area 안에서 중앙 정렬을 유지하면서     */
    /*   세 줄 선택지와 안내문을 동시에 안정적으로 담는다.                  */
    /* ------------------------------------------------------------------ */
    ui_confirm_draw_centered(u8g2, u8g2_font_6x12_mf, cx, (int16_t)(y0 + 15), s_confirm.title);
    ui_confirm_draw_centered(u8g2, u8g2_font_5x8_tr,  cx, (int16_t)(y0 + 33), s_confirm.body);
    ui_confirm_draw_centered(u8g2, u8g2_font_6x12_mf, cx, (int16_t)(y0 + 48), s_confirm.option1);
    ui_confirm_draw_centered(u8g2, u8g2_font_6x12_mf, cx, (int16_t)(y0 + 60), s_confirm.option2);
    ui_confirm_draw_centered(u8g2, u8g2_font_6x12_mf, cx, (int16_t)(y0 + 72), s_confirm.option3);
    ui_confirm_draw_centered(u8g2,
                             u8g2_font_5x7_tr,
                             cx,
                             (int16_t)(y0 + box_h - 5),
                             "LONG PRESS FN KEY TO CHOOSE!");
}
