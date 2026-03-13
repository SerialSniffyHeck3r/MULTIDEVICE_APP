#include "ui_screen_engine_oil.h"

#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*  Local font policy                                                          */
/*                                                                            */
/*  제목은 5x8 텍스트로 상단 좌측 정렬하고,                                    */
/*  값은 기존 프로젝트에서 이미 사용 중인 10x20 숫자 폰트로 크게 보여 준다.   */
/* -------------------------------------------------------------------------- */
#define UI_ENGINE_OIL_TITLE_FONT   u8g2_font_5x8_tr
#define UI_ENGINE_OIL_VALUE_FONT   u8g2_font_10x20_mf

/* -------------------------------------------------------------------------- */
/*  Fixed edit field geometry                                                  */
/*                                                                            */
/*  5자리 숫자를 cell 기반으로 배치해                                          */
/*  자릿수 선택 박스를 안정적으로 그릴 수 있게 한다.                           */
/* -------------------------------------------------------------------------- */
#define UI_ENGINE_OIL_DIGIT_COUNT  5u
#define UI_ENGINE_OIL_DIGIT_GAP    2

void UI_ScreenEngineOil_Draw(u8g2_t *u8g2,
                             const ui_rect_t *viewport,
                             uint32_t interval_value,
                             uint8_t selected_digit_index)
{
    char value_text[UI_ENGINE_OIL_DIGIT_COUNT + 1u];
    int16_t title_x;
    int16_t title_y;
    int16_t divider_y;
    int16_t digit_ascent;
    int16_t digit_descent;
    int16_t digit_box_h;
    int16_t digit_cell_w;
    int16_t total_digit_w;
    int16_t start_x;
    int16_t baseline_y;
    uint8_t digit_index;

    if ((u8g2 == 0) || (viewport == 0))
    {
        return;
    }

    if ((viewport->w <= 0) || (viewport->h <= 0))
    {
        return;
    }

    if (interval_value > 99999u)
    {
        interval_value = 99999u;
    }

    if (selected_digit_index >= UI_ENGINE_OIL_DIGIT_COUNT)
    {
        selected_digit_index = (UI_ENGINE_OIL_DIGIT_COUNT - 1u);
    }

    snprintf(value_text, sizeof(value_text), "%05lu", (unsigned long)interval_value);

    /* ---------------------------------------------------------------------- */
    /*  1) Title                                                               */
    /*                                                                            */
    /*  문자열 "ENGINE OIL CHANGE INTERVAL" 은                                 */
    /*  status bar 아래, 즉 전달받은 viewport의 좌상단 기준으로 찍는다.         */
    /*  이때 title 자체는 왼쪽 정렬을 유지한다.                                */
    /* ---------------------------------------------------------------------- */
    u8g2_SetDrawColor(u8g2, 1);
    u8g2_SetFont(u8g2, UI_ENGINE_OIL_TITLE_FONT);

    title_x = viewport->x;
    title_y = (int16_t)(viewport->y + 8);
    divider_y = (int16_t)(viewport->y + 11);

    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)title_x,
                 (u8g2_uint_t)title_y,
                 "ENGINE OIL CHANGE INTERVAL");

    /* ---------------------------------------------------------------------- */
    /*  Title underline                                                        */
    /*                                                                            */
    /*  제목 아래 1px 수평선을 넣어                                             */
    /*  "상단 제목 영역" 과 "중앙 숫자 편집 영역" 이 분리되도록 한다.           */
    /* ---------------------------------------------------------------------- */
    if (divider_y < (viewport->y + viewport->h))
    {
        u8g2_DrawHLine(u8g2,
                       (u8g2_uint_t)viewport->x,
                       (u8g2_uint_t)divider_y,
                       (u8g2_uint_t)viewport->w);
    }

    /* ---------------------------------------------------------------------- */
    /*  2) Large 5-digit value                                                 */
    /*                                                                            */
    /*  10x20 숫자 폰트를 사용해 화면 중앙에 5자리 숫자를 배치한다.             */
    /*  각 숫자는 cell 단위로 나누어 정렬하고,                                  */
    /*  선택된 자릿수만 박스로 둘러싸서 editing target을 명확히 한다.          */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFont(u8g2, UI_ENGINE_OIL_VALUE_FONT);

    digit_ascent = (int16_t)u8g2_GetAscent(u8g2);
    digit_descent = (int16_t)u8g2_GetDescent(u8g2);
    if (digit_descent < 0)
    {
        digit_descent = (int16_t)(-digit_descent);
    }

    digit_box_h = (int16_t)(digit_ascent + digit_descent);
    digit_cell_w = (int16_t)u8g2_GetStrWidth(u8g2, "0");
    total_digit_w = (int16_t)((UI_ENGINE_OIL_DIGIT_COUNT * digit_cell_w) +
                              ((UI_ENGINE_OIL_DIGIT_COUNT - 1u) * UI_ENGINE_OIL_DIGIT_GAP));

    start_x = (int16_t)(viewport->x + ((viewport->w - total_digit_w) / 2));
    if (start_x < viewport->x)
    {
        start_x = viewport->x;
    }

    baseline_y = (int16_t)(viewport->y + (viewport->h / 2) + (digit_ascent / 2));
    if (baseline_y < (divider_y + 16))
    {
        baseline_y = (int16_t)(divider_y + 16);
    }

    for (digit_index = 0u; digit_index < UI_ENGINE_OIL_DIGIT_COUNT; digit_index++)
    {
        char digit_text[2];
        int16_t cell_x;
        int16_t char_w;
        int16_t char_x;
        int16_t box_x;
        int16_t box_y;
        int16_t box_w;
        int16_t box_h;

        digit_text[0] = value_text[digit_index];
        digit_text[1] = '\0';

        cell_x = (int16_t)(start_x + (int16_t)digit_index * (digit_cell_w + UI_ENGINE_OIL_DIGIT_GAP));
        char_w = (int16_t)u8g2_GetStrWidth(u8g2, digit_text);
        char_x = (int16_t)(cell_x + ((digit_cell_w - char_w) / 2));

        if (digit_index == selected_digit_index)
        {
            /* -------------------------------------------------------------- */
            /*  Selected digit box                                             */
            /*                                                                */
            /*  현재 편집 중인 자릿수를 둘러싸는 박스다.                       */
            /*  숫자 glyph 위/아래 여백을 조금 포함해서                       */
            /*  실제 편집 커서처럼 보이게 만든다.                             */
            /* -------------------------------------------------------------- */
            box_x = (int16_t)(cell_x - 2);
            box_y = (int16_t)(baseline_y - digit_ascent - 2);
            box_w = (int16_t)(digit_cell_w + 4);
            box_h = (int16_t)(digit_box_h + 4);

            if (box_x < viewport->x)
            {
                box_x = viewport->x;
            }

            u8g2_DrawFrame(u8g2,
                           (u8g2_uint_t)box_x,
                           (u8g2_uint_t)box_y,
                           (u8g2_uint_t)box_w,
                           (u8g2_uint_t)box_h);
        }

        /* ------------------------------------------------------------------ */
        /*  Individual digit glyph                                            */
        /*                                                                    */
        /*  각 숫자는 자신의 cell 중앙에 찍는다.                               */
        /* ------------------------------------------------------------------ */
        u8g2_DrawStr(u8g2,
                     (u8g2_uint_t)char_x,
                     (u8g2_uint_t)baseline_y,
                     digit_text);
    }

    /* ---------------------------------------------------------------------- */
    /*  3) Small helper text                                                   */
    /*                                                                            */
    /*  viewport 하단 왼쪽에 현재 선택 자릿수 조작 방법을 짧게 표기한다.        */
    /*  bottom bar에 F-key 힌트가 이미 있으므로, 화면 안쪽 텍스트는            */
    /*  설명 과밀을 피하기 위해 아주 짧게 유지한다.                            */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFont(u8g2, UI_ENGINE_OIL_TITLE_FONT);
    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)viewport->x,
                 (u8g2_uint_t)(viewport->y + viewport->h - 1),
                 "F2/F3 DIGIT  F4/F5 VALUE");
}
