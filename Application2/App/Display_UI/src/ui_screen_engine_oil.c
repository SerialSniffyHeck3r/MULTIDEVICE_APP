#include "ui_screen_engine_oil.h"

#include "ui_bottombar.h"
#include "ui_toast.h"
#include "ui_common_icons.h"

#include <stdio.h>

/* -------------------------------------------------------------------------- */
/*  Screen-local runtime state                                                */
/*                                                                            */
/*  이 파일은 엔진 오일 교체 주기 화면의 기능 로직을 전담한다.                  */
/*  즉, 저장값 / 편집값 / 선택 자릿수 / bottom bar 내용 결정은                  */
/*  UI 엔진이 아니라 이 화면 파일 안에서만 관리한다.                           */
/* -------------------------------------------------------------------------- */
#define UI_ENGINE_OIL_DIGIT_COUNT    5u
#define UI_ENGINE_OIL_INTERVAL_MAX   99999u

static uint32_t s_engine_oil_interval_saved = 5000u;
static uint32_t s_engine_oil_interval_edit = 5000u;
static uint8_t  s_engine_oil_digit_index = 0u;

/* -------------------------------------------------------------------------- */
/*  Local helpers for engine-oil screen                                       */
/* -------------------------------------------------------------------------- */
static void ui_screen_engine_oil_configure_bottom_bar(void);
static uint32_t ui_screen_engine_oil_get_place_value(uint8_t digit_index);
static void ui_screen_engine_oil_adjust_digit(int8_t delta);

/* -------------------------------------------------------------------------- */
/*  Engine-oil bottom bar                                                     */
/*                                                                            */
/*  이 화면의 bottom bar는 6개의 물리 버튼과 1:1로 대응한다.                    */
/*  - F1 : BACK 텍스트                                                         */
/*  - F2 : 왼쪽 화살표                                                         */
/*  - F3 : 오른쪽 화살표                                                       */
/*  - F4 : 위쪽 화살표                                                         */
/*  - F5 : 아래쪽 화살표                                                       */
/*  - F6 : DONE 텍스트                                                         */
/* -------------------------------------------------------------------------- */
static void ui_screen_engine_oil_configure_bottom_bar(void)
{
    UI_BottomBar_SetMode(UI_BOTTOMBAR_MODE_BUTTONS);

    UI_BottomBar_SetButton(UI_FKEY_1, "BACK", UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButtonIcon4(UI_FKEY_2,
                                icon_arrow_left_7x4,
                                ICON7X4_W,
                                UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButtonIcon4(UI_FKEY_3,
                                icon_arrow_right_7x4,
                                ICON7X4_W,
                                UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButtonIcon4(UI_FKEY_4,
                                icon_arrow_up_7x4,
                                ICON7X4_W,
                                UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButtonIcon4(UI_FKEY_5,
                                icon_arrow_down_7x4,
                                ICON7X4_W,
                                UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButton(UI_FKEY_6, "DONE", 0u);
}

/* -------------------------------------------------------------------------- */
/*  Place-value helper                                                        */
/*                                                                            */
/*  선택 자릿수 index는 왼쪽에서 오른쪽 순서다.                                */
/*  - 0 -> 10000 자리                                                          */
/*  - 1 -> 1000 자리                                                           */
/*  - 2 -> 100 자리                                                            */
/*  - 3 -> 10 자리                                                             */
/*  - 4 -> 1 자리                                                              */
/* -------------------------------------------------------------------------- */
static uint32_t ui_screen_engine_oil_get_place_value(uint8_t digit_index)
{
    static const uint32_t place_table[UI_ENGINE_OIL_DIGIT_COUNT] =
    {
        10000u, 1000u, 100u, 10u, 1u
    };

    if (digit_index >= UI_ENGINE_OIL_DIGIT_COUNT)
    {
        return 1u;
    }

    return place_table[digit_index];
}

/* -------------------------------------------------------------------------- */
/*  Current digit +/- 1                                                       */
/*                                                                            */
/*  현재 선택된 자릿수 하나만 0~9 범위에서 순환시킨다.                          */
/*  다른 자릿수는 절대 건드리지 않는다.                                         */
/* -------------------------------------------------------------------------- */
static void ui_screen_engine_oil_adjust_digit(int8_t delta)
{
    uint32_t place_value;
    uint32_t current_digit;
    uint32_t new_digit;

    place_value = ui_screen_engine_oil_get_place_value(s_engine_oil_digit_index);
    current_digit = (s_engine_oil_interval_edit / place_value) % 10u;

    if (delta >= 0)
    {
        new_digit = (current_digit + 1u) % 10u;
    }
    else
    {
        new_digit = (current_digit == 0u) ? 9u : (current_digit - 1u);
    }

    s_engine_oil_interval_edit -= (current_digit * place_value);
    s_engine_oil_interval_edit += (new_digit * place_value);

    if (s_engine_oil_interval_edit > UI_ENGINE_OIL_INTERVAL_MAX)
    {
        s_engine_oil_interval_edit = UI_ENGINE_OIL_INTERVAL_MAX;
    }
}

/* -------------------------------------------------------------------------- */
/*  Public init                                                               */
/* -------------------------------------------------------------------------- */
void UI_ScreenEngineOil_Init(void)
{
    s_engine_oil_interval_saved = 5000u;
    s_engine_oil_interval_edit = s_engine_oil_interval_saved;
    s_engine_oil_digit_index = 0u;
}

/* -------------------------------------------------------------------------- */
/*  Public on-enter                                                           */
/*                                                                            */
/*  진입 시 현재 편집값을 저장값으로부터 다시 읽어 와서                         */
/*  "항상 저장 완료값에서 편집 시작" 하도록 만든다.                            */
/* -------------------------------------------------------------------------- */
void UI_ScreenEngineOil_OnEnter(void)
{
    s_engine_oil_interval_edit = s_engine_oil_interval_saved;
    s_engine_oil_digit_index = 0u;

    ui_screen_engine_oil_configure_bottom_bar();
}

/* -------------------------------------------------------------------------- */
/*  Public on-resume                                                          */
/*                                                                            */
/*  GPS 화면으로 잠깐 나갔다가 다시 돌아올 때                                  */
/*  현재 편집값(edit)과 선택 자릿수를 유지한 채                                */
/*  이 화면 전용 하단바만 복구한다.                                            */
/* -------------------------------------------------------------------------- */
void UI_ScreenEngineOil_OnResume(void)
{
    ui_screen_engine_oil_configure_bottom_bar();
}

/* -------------------------------------------------------------------------- */
/*  Public button handler                                                     */
/* -------------------------------------------------------------------------- */
ui_screen_engine_oil_action_t UI_ScreenEngineOil_HandleButtonEvent(const button_event_t *event,
                                                                   uint32_t now_ms)
{
    if (event == 0)
    {
        return UI_SCREEN_ENGINE_OIL_ACTION_NONE;
    }

    if (event->type != BUTTON_EVENT_SHORT_PRESS)
    {
        return UI_SCREEN_ENGINE_OIL_ACTION_NONE;
    }

    switch (event->id)
    {
        case BUTTON_ID_1:
            return UI_SCREEN_ENGINE_OIL_ACTION_BACK_TO_TEST;

        case BUTTON_ID_2:
            if (s_engine_oil_digit_index > 0u)
            {
                s_engine_oil_digit_index--;
            }
            break;

        case BUTTON_ID_3:
            if ((uint32_t)s_engine_oil_digit_index + 1u < UI_ENGINE_OIL_DIGIT_COUNT)
            {
                s_engine_oil_digit_index++;
            }
            break;

        case BUTTON_ID_4:
            ui_screen_engine_oil_adjust_digit(+1);
            break;

        case BUTTON_ID_5:
            ui_screen_engine_oil_adjust_digit(-1);
            break;

        case BUTTON_ID_6:
            s_engine_oil_interval_saved = s_engine_oil_interval_edit;
            UI_Toast_Show("OIL INT SAVED",
                          icon_ui_ok_8x8,
                          ICON8_W,
                          ICON8_H,
                          now_ms,
                          900u);
            return UI_SCREEN_ENGINE_OIL_ACTION_SAVE_AND_BACK_TO_TEST;

        case BUTTON_ID_NONE:
        default:
            break;
    }

    return UI_SCREEN_ENGINE_OIL_ACTION_NONE;
}

/* -------------------------------------------------------------------------- */
/*  Compose getters                                                           */
/* -------------------------------------------------------------------------- */
ui_layout_mode_t UI_ScreenEngineOil_GetLayoutMode(void)
{
    return UI_LAYOUT_MODE_TOP_BOTTOM_FIXED;
}

bool UI_ScreenEngineOil_IsStatusBarVisible(void)
{
    return true;
}

bool UI_ScreenEngineOil_IsBottomBarVisible(void)
{
    return true;
}

/* -------------------------------------------------------------------------- */
/*  Draw-state getters                                                        */
/* -------------------------------------------------------------------------- */
uint32_t UI_ScreenEngineOil_GetIntervalValue(void)
{
    return s_engine_oil_interval_edit;
}

uint8_t UI_ScreenEngineOil_GetSelectedDigitIndex(void)
{
    return s_engine_oil_digit_index;
}

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
