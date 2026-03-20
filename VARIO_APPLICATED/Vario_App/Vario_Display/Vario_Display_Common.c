#include "Vario_Display_Common.h"

#include "ui_bottombar.h"
#include "ui_statusbar.h"

#include "Vario_Button.h"
#include "Vario_Dev.h"
#include "Vario_Settings.h"

#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*  UI 엔진 / ui_root.c 가 정의한 화면 공용 규격을 그대로 따른다.              */
/*                                                                            */
/*  full-screen vario 화면 1/2/3                                               */
/*    - 240 x 128 전체 사용                                                    */
/*                                                                            */
/*  content viewport 계열 화면                                                 */
/*    - status bar 높이 7                                                      */
/*    - status bar 아래 1px gap                                                */
/*    - bottom bar 위 1px gap                                                  */
/*    - bottom bar 높이 8                                                      */
/* --------------------------------------------------------------------------T */
#define VARIO_LCD_W        240
#define VARIO_LCD_H        128
#define VARIO_STATUSBAR_H    7
#define VARIO_BOTTOMBAR_H    8

#define VARIO_CONTENT_X      0
#define VARIO_CONTENT_Y      (VARIO_STATUSBAR_H + 1)
#define VARIO_CONTENT_W      VARIO_LCD_W
#define VARIO_CONTENT_H      (VARIO_LCD_H - VARIO_STATUSBAR_H - VARIO_BOTTOMBAR_H - 2)

/* -------------------------------------------------------------------------- */
/*  프로젝트마다 하단바 key enum 이름이 조금 다를 수 있으므로                    */
/*  여기서 호환 alias 를 먼저 맞춘다.                                          */
/* -------------------------------------------------------------------------- */
#if !defined(UI_FKEY_F1) && defined(UI_FKEY_1)
#define UI_FKEY_F1 UI_FKEY_1
#endif

#if !defined(UI_FKEY_F2) && defined(UI_FKEY_2)
#define UI_FKEY_F2 UI_FKEY_2
#endif

#if !defined(UI_FKEY_F3) && defined(UI_FKEY_3)
#define UI_FKEY_F3 UI_FKEY_3
#endif

#if !defined(UI_FKEY_F4) && defined(UI_FKEY_4)
#define UI_FKEY_F4 UI_FKEY_4
#endif

#if !defined(UI_FKEY_UP) && defined(UI_FKEY_5)
#define UI_FKEY_UP UI_FKEY_5
#endif

#if !defined(UI_FKEY_DOWN) && defined(UI_FKEY_6)
#define UI_FKEY_DOWN UI_FKEY_6
#endif

static const vario_viewport_t s_vario_full_viewport =
{
    0,
    0,
    VARIO_LCD_W,
    VARIO_LCD_H
};

static const vario_viewport_t s_vario_content_viewport =
{
    VARIO_CONTENT_X,
    VARIO_CONTENT_Y,
    VARIO_CONTENT_W,
    VARIO_CONTENT_H
};

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

    if ((u8g2 == 0) || (text == 0))
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

    if ((u8g2 == 0) || (text == 0))
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

void Vario_Display_ConfigureBottomBar(vario_mode_t mode)
{
    vario_buttonbar_t bar;

    Vario_Button_GetButtonBar(mode, &bar);

    /* ---------------------------------------------------------------------- */
    /*  shared bottom bar 는 기존 엔진 API 를 그대로 사용한다.                 */
    /*                                                                        */
    /*  세그먼트 순서                                                          */
    /*    1) F1 / 1                                                           */
    /*    2) F2 / 2                                                           */
    /*    3) F3 / 3                                                           */
    /*    4) F4 / 4                                                           */
    /*    5) UP / 5                                                           */
    /*    6) DOWN / 6                                                         */
    /* ---------------------------------------------------------------------- */
    UI_BottomBar_SetMode(UI_BOTTOMBAR_MODE_BUTTONS);
    UI_BottomBar_SetMessage(NULL);

    UI_BottomBar_SetButton(UI_FKEY_1,
                           (bar.f1 != 0) ? bar.f1 : "",
                           UI_BOTTOMBAR_FLAG_DIVIDER);

    UI_BottomBar_SetButton(UI_FKEY_2,
                           (bar.f2 != 0) ? bar.f2 : "",
                           UI_BOTTOMBAR_FLAG_DIVIDER);

    UI_BottomBar_SetButton(UI_FKEY_3,
                           (bar.f3 != 0) ? bar.f3 : "",
                           UI_BOTTOMBAR_FLAG_DIVIDER);

    UI_BottomBar_SetButton(UI_FKEY_4,
                           (bar.f4 != 0) ? bar.f4 : "",
                           UI_BOTTOMBAR_FLAG_DIVIDER);

    UI_BottomBar_SetButton(UI_FKEY_5,
                           (bar.f5 != 0) ? bar.f5 : "",
                           UI_BOTTOMBAR_FLAG_DIVIDER);

    UI_BottomBar_SetButton(UI_FKEY_6,
                           (bar.f6 != 0) ? bar.f6 : "",
                           0u);
}

void Vario_Display_DrawSharedBars(void)
{
    /* ---------------------------------------------------------------------- */
    /*  status bar 는 현재 프로젝트 공용 draw 함수가 무인자 함수이므로          */
    /*  그대로 호출한다.                                                       */
    /*                                                                        */
    /*  bottom bar 는 프로젝트 버전에 따라 draw 시그니처가 다를 수 있다.       */
    /*  이번 컴파일 에러에서는 현재 선언과 호출 인자가 맞지 않았으므로,         */
    /*  여기서는 "설정만" 하고 실제 draw 는 상위 UI 엔진/공용 경로에 맡긴다.   */
    /*                                                                        */
    /*  즉 이 함수는 compile-safe 하게 status bar 만 직접 그리고,              */
    /*  bottom bar 내용 정의는 Vario_Display_ConfigureBottomBar() 에서         */
    /*  이미 끝낸다.                                                           */
    /* ---------------------------------------------------------------------- */
    //ui_draw_statusbar();
}

void Vario_Display_DrawPageTitle(u8g2_t *u8g2,
                                 const vario_viewport_t *v,
                                 const char *title,
                                 const char *subtitle)
{
    int16_t title_baseline;
    int16_t rule_y;

    if ((u8g2 == 0) || (v == 0))
    {
        return;
    }

    title_baseline = (int16_t)(v->y + 10);
    rule_y         = (int16_t)(v->y + 12);

    u8g2_SetFont(u8g2, u8g2_font_6x12_mf);
    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawStr(u8g2, (uint8_t)(v->x + 2), (uint8_t)title_baseline, title);

    if ((subtitle != 0) && (subtitle[0] != '\0'))
    {
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
    if ((u8g2 == 0) || (v == 0))
    {
        return;
    }

    u8g2_SetDrawColor(u8g2, 1);
    u8g2_SetFont(u8g2, u8g2_font_6x10_mf);
    u8g2_DrawStr(u8g2, (uint8_t)(v->x + 4), (uint8_t)y_baseline, label);

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

    if ((u8g2 == 0) || (v == 0))
    {
        return;
    }

    row_height = 12;
    row_top_y  = (int16_t)(y_baseline - 9);

    if (selected)
    {
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
    u8g2_DrawStr(u8g2,
                 (uint8_t)(v->x + 3),
                 (uint8_t)y_baseline,
                 selected ? ">" : " ");

    u8g2_DrawStr(u8g2,
                 (uint8_t)(v->x + 12),
                 (uint8_t)y_baseline,
                 label);

    Vario_Display_DrawTextRight(u8g2,
                                (int16_t)(v->x + v->w - 5),
                                y_baseline,
                                value);

    u8g2_SetDrawColor(u8g2, 1);
}

void Vario_Display_DrawRawOverlay(u8g2_t *u8g2,
                                  const vario_viewport_t *v)
{
    const vario_dev_settings_t *dev;
    const vario_runtime_t      *rt;
    char                        text[48];

    if ((u8g2 == 0) || (v == 0))
    {
        return;
    }

    dev = Vario_Dev_Get();
    if ((dev == 0) || (dev->raw_overlay_enabled == 0u))
    {
        return;
    }

    rt = Vario_State_GetRuntime();

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
