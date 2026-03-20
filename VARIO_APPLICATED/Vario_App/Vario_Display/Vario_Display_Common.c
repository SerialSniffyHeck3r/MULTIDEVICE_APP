#include "Vario_Display_Common.h"

#include "Vario_Dev.h"
#include "Vario_State.h"

#include <stdio.h>
#include <stddef.h>

/* -------------------------------------------------------------------------- */
/* 기본 viewport 규격                                                          */
/*                                                                            */
/* 이 값들은 "아직 UI 엔진 bridge 가 실제 viewport 를 주입하기 전" 의 fallback   */
/* 이다.                                                                        */
/*                                                                            */
/* 정상 런타임에서는 매 프레임                                                   */
/*   UI_ScreenVario_Draw() -> Vario_Display_SetViewports()                      */
/* 호출이 먼저 일어나므로, renderer 는 실제 root compose viewport 를 보게 된다.  */
/* -------------------------------------------------------------------------- */
#define VARIO_LCD_W       240
#define VARIO_LCD_H       128
#define VARIO_STATUSBAR_H 7
#define VARIO_BOTTOMBAR_H 8
#define VARIO_CONTENT_X   0
#define VARIO_CONTENT_Y   (VARIO_STATUSBAR_H + 1)
#define VARIO_CONTENT_W   VARIO_LCD_W
#define VARIO_CONTENT_H   (VARIO_LCD_H - VARIO_STATUSBAR_H - VARIO_BOTTOMBAR_H - 2)

static vario_viewport_t s_vario_full_viewport =
{
    0,
    0,
    VARIO_LCD_W,
    VARIO_LCD_H
};

static vario_viewport_t s_vario_content_viewport =
{
    VARIO_CONTENT_X,
    VARIO_CONTENT_Y,
    VARIO_CONTENT_W,
    VARIO_CONTENT_H
};

/* -------------------------------------------------------------------------- */
/* 내부 helper: viewport sanity clamp                                          */
/*                                                                            */
/* 이유                                                                          */
/* - bridge 가 잘못된 값을 주더라도 renderer 가 음수 width/height 를 잡지 않게   */
/*   방어한다.                                                                  */
/* - 기존 고정 240x128 좌표계를 벗어나는 값을 잘라낸다.                         */
/* -------------------------------------------------------------------------- */
static void vario_display_common_sanitize_viewport(vario_viewport_t *v)
{
    if (v == NULL)
    {
        return;
    }

    if (v->x < 0)
    {
        v->x = 0;
    }

    if (v->y < 0)
    {
        v->y = 0;
    }

    if (v->x > VARIO_LCD_W)
    {
        v->x = VARIO_LCD_W;
    }

    if (v->y > VARIO_LCD_H)
    {
        v->y = VARIO_LCD_H;
    }

    if (v->w < 0)
    {
        v->w = 0;
    }

    if (v->h < 0)
    {
        v->h = 0;
    }

    if ((v->x + v->w) > VARIO_LCD_W)
    {
        v->w = (int16_t)(VARIO_LCD_W - v->x);
    }

    if ((v->y + v->h) > VARIO_LCD_H)
    {
        v->h = (int16_t)(VARIO_LCD_H - v->y);
    }
}

void Vario_Display_SetViewports(const vario_viewport_t *full_viewport,
                                const vario_viewport_t *content_viewport)
{
    if (full_viewport != NULL)
    {
        s_vario_full_viewport = *full_viewport;
    }
    else
    {
        s_vario_full_viewport.x = 0;
        s_vario_full_viewport.y = 0;
        s_vario_full_viewport.w = VARIO_LCD_W;
        s_vario_full_viewport.h = VARIO_LCD_H;
    }

    if (content_viewport != NULL)
    {
        s_vario_content_viewport = *content_viewport;
    }
    else
    {
        s_vario_content_viewport.x = VARIO_CONTENT_X;
        s_vario_content_viewport.y = VARIO_CONTENT_Y;
        s_vario_content_viewport.w = VARIO_CONTENT_W;
        s_vario_content_viewport.h = VARIO_CONTENT_H;
    }

    vario_display_common_sanitize_viewport(&s_vario_full_viewport);
    vario_display_common_sanitize_viewport(&s_vario_content_viewport);
}

const vario_viewport_t *Vario_Display_GetFullViewport(void)
{
    return &s_vario_full_viewport;
}

const vario_viewport_t *Vario_Display_GetContentViewport(void)
{
    return &s_vario_content_viewport;
}

void Vario_Display_DrawTextRight(u8g2_t *u8g2,
                                 int16_t right_x,
                                 int16_t y_baseline,
                                 const char *text)
{
    int16_t width;
    int16_t draw_x;

    if ((u8g2 == NULL) || (text == NULL))
    {
        return;
    }

    width = (int16_t)u8g2_GetStrWidth(u8g2, text);
    draw_x = (int16_t)(right_x - width);

    if (draw_x < 0)
    {
        draw_x = 0;
    }

    u8g2_DrawStr(u8g2, (uint8_t)draw_x, (uint8_t)y_baseline, text);
}

void Vario_Display_DrawTextCentered(u8g2_t *u8g2,
                                    int16_t center_x,
                                    int16_t y_baseline,
                                    const char *text)
{
    int16_t width;
    int16_t draw_x;

    if ((u8g2 == NULL) || (text == NULL))
    {
        return;
    }

    width = (int16_t)u8g2_GetStrWidth(u8g2, text);
    draw_x = (int16_t)(center_x - (width / 2));

    if (draw_x < 0)
    {
        draw_x = 0;
    }

    u8g2_DrawStr(u8g2, (uint8_t)draw_x, (uint8_t)y_baseline, text);
}

void Vario_Display_DrawPageTitle(u8g2_t *u8g2,
                                 const vario_viewport_t *v,
                                 const char *title,
                                 const char *subtitle)
{
    int16_t title_baseline;
    int16_t rule_y;

    if ((u8g2 == NULL) || (v == NULL) || (title == NULL))
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /* title 위치                                                               */
    /* - viewport 좌측 상단에서 2px 안쪽으로 제목을 둔다.                        */
    /* - baseline 은 viewport top + 10 으로 잡아 6x12 폰트가 윗변에 닿지 않게    */
    /*   한다.                                                                  */
    /* ---------------------------------------------------------------------- */
    title_baseline = (int16_t)(v->y + 10);

    /* ---------------------------------------------------------------------- */
    /* rule line 위치                                                            */
    /* - 제목 바로 아래 2px 정도 여유를 둔 horizontal separator line             */
    /* - viewport 전체 폭을 가로지른다.                                         */
    /* ---------------------------------------------------------------------- */
    rule_y = (int16_t)(v->y + 12);

    u8g2_SetFont(u8g2, u8g2_font_6x12_mf);
    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawStr(u8g2, (uint8_t)(v->x + 2), (uint8_t)title_baseline, title);

    if ((subtitle != NULL) && (subtitle[0] != '\0'))
    {
        /* ------------------------------------------------------------------ */
        /* subtitle 위치                                                        */
        /* - 같은 제목 줄의 맨 오른쪽 끝                                       */
        /* - 작은 5x8 font 를 써서 title 과 위계를 구분한다.                    */
        /* ------------------------------------------------------------------ */
        u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
        Vario_Display_DrawTextRight(u8g2,
                                    (int16_t)(v->x + v->w - 2),
                                    (int16_t)(v->y + 9),
                                    subtitle);
    }

    u8g2_DrawHLine(u8g2, (uint8_t)v->x, (uint8_t)rule_y, (uint8_t)v->w);
}

void Vario_Display_DrawKeyValueRow(u8g2_t *u8g2,
                                   const vario_viewport_t *v,
                                   int16_t y_baseline,
                                   const char *label,
                                   const char *value)
{
    if ((u8g2 == NULL) || (v == NULL) || (label == NULL) || (value == NULL))
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /* 좌측 label                                                               */
    /* - viewport 왼쪽에서 4px 안쪽                                             */
    /* ---------------------------------------------------------------------- */
    u8g2_SetDrawColor(u8g2, 1);
    u8g2_SetFont(u8g2, u8g2_font_6x10_mf);
    u8g2_DrawStr(u8g2, (uint8_t)(v->x + 4), (uint8_t)y_baseline, label);

    /* ---------------------------------------------------------------------- */
    /* 우측 value                                                               */
    /* - viewport 오른쪽에서 4px 안쪽으로 right-align                           */
    /* ---------------------------------------------------------------------- */
    Vario_Display_DrawTextRight(u8g2,
                                (int16_t)(v->x + v->w - 4),
                                y_baseline,
                                value);
}

void Vario_Display_DrawMenuRow(u8g2_t *u8g2,
                               const vario_viewport_t *v,
                               int16_t y_baseline,
                               bool selected,
                               const char *label,
                               const char *value)
{
    int16_t row_top_y;
    int16_t row_height;

    if ((u8g2 == NULL) || (v == NULL) || (label == NULL) || (value == NULL))
    {
        return;
    }

    row_height = 12;
    row_top_y = (int16_t)(y_baseline - 9);

    if (selected)
    {
        /* ------------------------------------------------------------------ */
        /* 선택 행 배경                                                         */
        /* - viewport 좌우에 1px margin 을 남기고 흰 박스로 채운다.             */
        /* - 이후 draw color 를 0으로 바꿔 글자가 반전되게 만든다.               */
        /* ------------------------------------------------------------------ */
        u8g2_SetDrawColor(u8g2, 1);
        u8g2_DrawBox(u8g2,
                     (uint8_t)(v->x + 1),
                     (uint8_t)row_top_y,
                     (uint8_t)(v->w - 2),
                     (uint8_t)row_height);
        u8g2_SetDrawColor(u8g2, 0);
    }
    else
    {
        u8g2_SetDrawColor(u8g2, 1);
    }

    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);

    /* ---------------------------------------------------------------------- */
    /* selection marker                                                         */
    /* - 행 맨 왼쪽 3px 정도 안쪽                                               */
    /* - selected 일 때 '>' 를 찍어 방향성을 한 번 더 준다.                     */
    /* ---------------------------------------------------------------------- */
    u8g2_DrawStr(u8g2,
                 (uint8_t)(v->x + 3),
                 (uint8_t)y_baseline,
                 selected ? ">" : " ");

    /* label text */
    u8g2_DrawStr(u8g2,
                 (uint8_t)(v->x + 12),
                 (uint8_t)y_baseline,
                 label);

    /* value text */
    Vario_Display_DrawTextRight(u8g2,
                                (int16_t)(v->x + v->w - 5),
                                y_baseline,
                                value);

    /* 다음 draw 를 위해 color 복원 */
    u8g2_SetDrawColor(u8g2, 1);
}

void Vario_Display_DrawRawOverlay(u8g2_t *u8g2, const vario_viewport_t *v)
{
    const vario_dev_settings_t *dev;
    const vario_runtime_t *rt;
    char text[48];

    if ((u8g2 == NULL) || (v == NULL))
    {
        return;
    }

    dev = Vario_Dev_Get();
    if ((dev == NULL) || (dev->raw_overlay_enabled == 0u))
    {
        return;
    }

    rt = Vario_State_GetRuntime();

    /* ---------------------------------------------------------------------- */
    /* raw overlay 내용                                                          */
    /* - P  : pressure                                                         */
    /* - T  : temperature                                                      */
    /* - BC : baro sample counter                                              */
    /*                                                                          */
    /* 위치                                                                       */
    /* - 현재 viewport 의 맨 아래줄 직전                                        */
    /* - 좌측 2px margin                                                        */
    /* ---------------------------------------------------------------------- */
    snprintf(text,
             sizeof(text),
             "P%ld T%ld BC%lu",
             (long)rt->pressure_hpa_x100,
             (long)rt->temperature_c_x100,
             (unsigned long)rt->gy86.baro.sample_count);

    u8g2_SetFont(u8g2, u8g2_font_4x6_tf);
    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawStr(u8g2,
                 (uint8_t)(v->x + 2),
                 (uint8_t)(v->y + v->h - 2),
                 text);
}
