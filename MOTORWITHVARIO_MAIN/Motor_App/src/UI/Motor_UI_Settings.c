#include "Motor_UI_Internal.h"
#include "Motor_Settings.h"

#include <string.h>

#ifndef MOTOR_SETTINGS_VISIBLE_ROWS
#define MOTOR_SETTINGS_VISIBLE_ROWS 6u
#endif

#define MOTOR_SETTINGS_ROW_PITCH     15
#define MOTOR_SETTINGS_FIRST_ROW_Y   29

static void motor_ui_settings_draw_text_right(u8g2_t *u8g2,
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

static void motor_ui_settings_draw_page_title(u8g2_t *u8g2,
                                              const ui_rect_t *viewport,
                                              const char *title,
                                              const char *subtitle)
{
    int16_t title_baseline;
    int16_t rule_y;

    if ((u8g2 == 0) || (viewport == 0) || (title == 0))
    {
        return;
    }

    title_baseline = (int16_t)(viewport->y + 12);
    rule_y = (int16_t)(viewport->y + 15);

    u8g2_SetDrawColor(u8g2, 1);
    u8g2_SetFont(u8g2, u8g2_font_helvB10_tf);
    u8g2_DrawStr(u8g2, (uint8_t)(viewport->x + 3), (uint8_t)title_baseline, title);

    if ((subtitle != 0) && (subtitle[0] != '\0'))
    {
        u8g2_SetFont(u8g2, u8g2_font_6x10_mf);
        motor_ui_settings_draw_text_right(u8g2,
                                          (int16_t)(viewport->x + viewport->w - 3),
                                          (int16_t)(viewport->y + 11),
                                          subtitle);
    }

    u8g2_DrawHLine(u8g2,
                   (uint8_t)(viewport->x + 1),
                   (uint8_t)rule_y,
                   (uint8_t)(viewport->w - 2));
}

static void motor_ui_settings_draw_menu_row(u8g2_t *u8g2,
                                            const ui_rect_t *viewport,
                                            int16_t y_baseline,
                                            uint8_t selected,
                                            const char *label,
                                            const char *value)
{
    int16_t row_top_y;
    int16_t row_height;

    if ((u8g2 == 0) || (viewport == 0) || (label == 0) || (value == 0))
    {
        return;
    }

    row_height = 15;
    row_top_y = (int16_t)(y_baseline - 11);

    u8g2_SetDrawColor(u8g2, 1);
    if (selected != 0u)
    {
        u8g2_DrawFrame(u8g2,
                       (uint8_t)(viewport->x + 2),
                       (uint8_t)row_top_y,
                       (uint8_t)(viewport->w - 4),
                       (uint8_t)row_height);
        u8g2_DrawBox(u8g2,
                     (uint8_t)(viewport->x + 4),
                     (uint8_t)(row_top_y + 3),
                     3u,
                     (uint8_t)(row_height - 6));
    }

    u8g2_SetFont(u8g2, u8g2_font_6x12_mf);
    u8g2_DrawStr(u8g2,
                 (uint8_t)(viewport->x + ((selected != 0u) ? 12 : 8)),
                 (uint8_t)y_baseline,
                 label);
    motor_ui_settings_draw_text_right(u8g2,
                                      (int16_t)(viewport->x + viewport->w - 8),
                                      y_baseline,
                                      value);
}

static uint8_t motor_ui_settings_compute_first_row(const motor_state_t *state,
                                                   uint8_t row_count)
{
    uint8_t first_row;

    if ((state == 0) || (row_count == 0u))
    {
        return 0u;
    }

    /* ---------------------------------------------------------------------- */
    /*  버튼 계층이 유지하는 scroll state를 우선 존중하되,                       */
    /*  selection과 row_count가 바뀌어도 화면이 어긋나지 않게 여기서 한 번 더   */
    /*  clamp 한다.                                                             */
    /* ---------------------------------------------------------------------- */
    first_row = state->ui.first_visible_row;

    if (state->ui.selected_index < first_row)
    {
        first_row = state->ui.selected_index;
    }

    if (state->ui.selected_index >= (uint8_t)(first_row + MOTOR_SETTINGS_VISIBLE_ROWS))
    {
        first_row = (uint8_t)(state->ui.selected_index - (MOTOR_SETTINGS_VISIBLE_ROWS - 1u));
    }

    if (first_row >= row_count)
    {
        first_row = (row_count > MOTOR_SETTINGS_VISIBLE_ROWS)
                  ? (uint8_t)(row_count - MOTOR_SETTINGS_VISIBLE_ROWS)
                  : 0u;
    }

    return first_row;
}

void Motor_UI_DrawScreen_Settings(u8g2_t *u8g2, const ui_rect_t *viewport, const motor_state_t *state)
{
    motor_screen_t screen;
    uint8_t row_count;
    uint8_t first_row;
    uint8_t visible_index;
    char label[28];
    char value[28];
    const char *title;
    const char *subtitle;

    if ((u8g2 == 0) || (viewport == 0) || (state == 0))
    {
        return;
    }

    screen = (motor_screen_t)state->ui.screen;
    row_count = Motor_Settings_GetRowCount(screen);
    title = Motor_Settings_GetScreenTitle(screen);
    subtitle = Motor_Settings_GetScreenSubtitle(screen);
    first_row = motor_ui_settings_compute_first_row(state, row_count);

    motor_ui_settings_draw_page_title(u8g2, viewport, title, subtitle);

    for (visible_index = 0u;
         (visible_index < MOTOR_SETTINGS_VISIBLE_ROWS) && ((first_row + visible_index) < row_count);
         visible_index++)
    {
        uint8_t row;
        int16_t y_baseline;

        row = (uint8_t)(first_row + visible_index);
        y_baseline = (int16_t)(viewport->y + MOTOR_SETTINGS_FIRST_ROW_Y + (visible_index * MOTOR_SETTINGS_ROW_PITCH));

        memset(label, 0, sizeof(label));
        memset(value, 0, sizeof(value));

        Motor_Settings_GetRowText(state,
                                  screen,
                                  row,
                                  label,
                                  sizeof(label),
                                  value,
                                  sizeof(value));

        motor_ui_settings_draw_menu_row(u8g2,
                                        viewport,
                                        y_baseline,
                                        (row == state->ui.selected_index) ? 1u : 0u,
                                        label,
                                        value);
    }
}
