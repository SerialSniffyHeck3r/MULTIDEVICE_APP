#include "Motor_UI_Internal.h"

#include "Motor_DataField.h"

#include <string.h>

#ifndef MOTOR_DATAFIELD_FONT_SMALL
#define MOTOR_DATAFIELD_FONT_SMALL   u8g2_font_5x7_tf
#endif

#ifndef MOTOR_DATAFIELD_FONT_MEDIUM
#define MOTOR_DATAFIELD_FONT_MEDIUM  u8g2_font_7x14B_tf
#endif

#define MOTOR_DATAFIELD_COL_COUNT    5
#define MOTOR_DATAFIELD_ROW_COUNT    3

static int16_t motor_datafield_get_font_height(u8g2_t *u8g2, const uint8_t *font)
{
    if ((u8g2 == 0) || (font == 0))
    {
        return 0;
    }

    u8g2_SetFont(u8g2, font);
    return (int16_t)(u8g2_GetAscent(u8g2) - u8g2_GetDescent(u8g2));
}

static uint16_t motor_datafield_measure_text(u8g2_t *u8g2, const uint8_t *font, const char *text)
{
    if ((u8g2 == 0) || (font == 0) || (text == 0))
    {
        return 0u;
    }

    u8g2_SetFont(u8g2, font);
    return u8g2_GetStrWidth(u8g2, text);
}

static int16_t motor_datafield_center_text_x(u8g2_t *u8g2,
                                             const uint8_t *font,
                                             int16_t x,
                                             int16_t w,
                                             const char *text)
{
    uint16_t text_w;

    if ((u8g2 == 0) || (font == 0) || (text == 0))
    {
        return x;
    }

    text_w = motor_datafield_measure_text(u8g2, font, text);
    if (text_w >= (uint16_t)w)
    {
        return x;
    }

    return (int16_t)(x + ((w - (int16_t)text_w) / 2));
}

static void motor_datafield_copy_fit(u8g2_t *u8g2,
                                     const uint8_t *font,
                                     const char *src,
                                     char *dst,
                                     size_t dst_size,
                                     int16_t max_width_px)
{
    size_t len;

    if ((u8g2 == 0) || (font == 0) || (dst == 0) || (dst_size == 0u))
    {
        return;
    }

    if (src == 0)
    {
        dst[0] = '\0';
        return;
    }

    (void)strncpy(dst, src, dst_size - 1u);
    dst[dst_size - 1u] = '\0';

    u8g2_SetFont(u8g2, font);
    while ((dst[0] != '\0') && (u8g2_GetStrWidth(u8g2, dst) > (uint16_t)max_width_px))
    {
        len = strlen(dst);
        if (len == 0u)
        {
            break;
        }
        dst[len - 1u] = '\0';
    }
}

static void motor_datafield_draw_text_top(u8g2_t *u8g2,
                                          const uint8_t *font,
                                          int16_t x,
                                          int16_t y,
                                          const char *text)
{
    if ((u8g2 == 0) || (font == 0) || (text == 0))
    {
        return;
    }

    u8g2_SetFontPosTop(u8g2);
    u8g2_SetFont(u8g2, font);
    u8g2_DrawStr(u8g2, x, y, text);
    u8g2_SetFontPosBaseline(u8g2);
}

static void motor_datafield_draw_cell(u8g2_t *u8g2,
                                      int16_t x,
                                      int16_t y,
                                      int16_t w,
                                      int16_t h,
                                      const char *label,
                                      const char *value,
                                      uint8_t selected,
                                      uint8_t editing)
{
    char label_buf[16];
    char value_buf[24];
    int16_t small_h;
    int16_t mid_h;
    int16_t label_band_y;
    int16_t label_text_y;
    int16_t value_y;
    int16_t indicator_h;

    if ((u8g2 == 0) || (w < 18) || (h < 18))
    {
        return;
    }

    small_h = motor_datafield_get_font_height(u8g2, MOTOR_DATAFIELD_FONT_SMALL);
    mid_h = motor_datafield_get_font_height(u8g2, MOTOR_DATAFIELD_FONT_MEDIUM);
    if (small_h < 1)
    {
        small_h = 1;
    }

    label_band_y = (int16_t)(y + 1);
    label_text_y = (int16_t)(label_band_y - 1);
    value_y = (int16_t)(label_band_y + small_h + 3);
    if ((value_y + mid_h) > (y + h - 2))
    {
        value_y = (int16_t)(y + h - mid_h - 2);
    }

    motor_datafield_copy_fit(u8g2,
                             MOTOR_DATAFIELD_FONT_SMALL,
                             (label != 0) ? label : "",
                             label_buf,
                             sizeof(label_buf),
                             (int16_t)(w - 4));
    motor_datafield_copy_fit(u8g2,
                             MOTOR_DATAFIELD_FONT_MEDIUM,
                             (value != 0) ? value : "",
                             value_buf,
                             sizeof(value_buf),
                             (int16_t)(w - 4));

    u8g2_DrawBox(u8g2, x + 1, label_band_y, (uint8_t)(w - 1), (uint8_t)small_h);
    u8g2_SetDrawColor(u8g2, 0);
    motor_datafield_draw_text_top(u8g2,
                                  MOTOR_DATAFIELD_FONT_SMALL,
                                  motor_datafield_center_text_x(u8g2,
                                                                MOTOR_DATAFIELD_FONT_SMALL,
                                                                x + 1,
                                                                (int16_t)(w - 2),
                                                                label_buf),
                                  label_text_y,
                                  label_buf);
    u8g2_SetDrawColor(u8g2, 1);
    motor_datafield_draw_text_top(u8g2,
                                  MOTOR_DATAFIELD_FONT_MEDIUM,
                                  motor_datafield_center_text_x(u8g2,
                                                                MOTOR_DATAFIELD_FONT_MEDIUM,
                                                                x + 1,
                                                                (int16_t)(w - 2),
                                                                value_buf),
                                  value_y,
                                  value_buf);

    if (selected != 0u)
    {
        indicator_h = (editing != 0u) ? 2 : 1;
        if ((h - 3) >= indicator_h)
        {
            u8g2_DrawBox(u8g2,
                         x + 1,
                         (int16_t)(y + h - indicator_h - 1),
                         (uint8_t)(w - 1),
                         (uint8_t)indicator_h);
        }
    }
}

void Motor_UI_DrawScreen_DataField(u8g2_t *u8g2,
                                   const ui_rect_t *viewport,
                                   const motor_state_t *state,
                                   uint8_t page_index)
{
    ui_rect_t top_rect;
    int16_t divider_y;
    int16_t grid_y;
    int16_t grid_h;
    int16_t col_w;
    int16_t row_h;
    uint8_t row;
    uint8_t col;

    if ((u8g2 == 0) || (viewport == 0) || (state == 0) || (page_index >= MOTOR_DATA_FIELD_PAGE_COUNT))
    {
        return;
    }

    Motor_UI_Main_GetTopGaugeLayout(viewport, &top_rect, &divider_y);
    grid_y = (int16_t)(divider_y + 1);
    grid_h = (int16_t)((viewport->y + viewport->h) - grid_y);

    if (grid_h < 30)
    {
        Motor_UI_DrawCenteredTextBlock(u8g2,
                                       viewport,
                                       (int16_t)((viewport->h / 2) - 14),
                                       "FIELD",
                                       "Viewport too small",
                                       "Need taller layout");
        return;
    }

    Motor_UI_Main_DrawTopGauge(u8g2, &top_rect, state);
    u8g2_DrawHLine(u8g2, viewport->x, divider_y, (uint8_t)viewport->w);

    col_w = (int16_t)(viewport->w / MOTOR_DATAFIELD_COL_COUNT);
    row_h = (int16_t)(grid_h / MOTOR_DATAFIELD_ROW_COUNT);

    for (row = 0u; row < MOTOR_DATAFIELD_ROW_COUNT; row++)
    {
        int16_t cell_y;
        int16_t next_row_y;
        int16_t cell_h;

        cell_y = (int16_t)(grid_y + (row * row_h));
        next_row_y = (row == (MOTOR_DATAFIELD_ROW_COUNT - 1u))
                   ? (int16_t)(grid_y + grid_h)
                   : (int16_t)(grid_y + ((row + 1u) * row_h));
        cell_h = (int16_t)(next_row_y - cell_y);

        for (col = 0u; col < MOTOR_DATAFIELD_COL_COUNT; col++)
        {
            motor_data_field_text_t text;
            motor_data_field_id_t field_id;
            uint8_t slot;
            int16_t cell_x;
            int16_t next_col_x;
            int16_t cell_w;

            slot = (uint8_t)(row * MOTOR_DATAFIELD_COL_COUNT + col);
            cell_x = (int16_t)(viewport->x + (col * col_w));
            next_col_x = (col == (MOTOR_DATAFIELD_COL_COUNT - 1u))
                       ? (int16_t)(viewport->x + viewport->w)
                       : (int16_t)(viewport->x + ((col + 1u) * col_w));
            cell_w = (int16_t)(next_col_x - cell_x);

            field_id = (motor_data_field_id_t)state->settings.data_fields[page_index][slot];
            Motor_DataField_Format(field_id, state, &text);

            motor_datafield_draw_cell(u8g2,
                                      cell_x,
                                      cell_y,
                                      cell_w,
                                      cell_h,
                                      text.label,
                                      text.value,
                                      (uint8_t)(state->ui.selected_slot == slot),
                                      state->ui.editing);
        }
    }

    u8g2_DrawVLine(u8g2, viewport->x, grid_y, (uint8_t)grid_h);
    u8g2_DrawVLine(u8g2, (int16_t)(viewport->x + viewport->w - 1), grid_y, (uint8_t)grid_h);
    u8g2_DrawHLine(u8g2, viewport->x, (int16_t)(grid_y + grid_h - 1), (uint8_t)viewport->w);

    for (col = 1u; col < MOTOR_DATAFIELD_COL_COUNT; col++)
    {
        int16_t line_x = (int16_t)(viewport->x + (col * col_w));
        if (line_x < (int16_t)(viewport->x + viewport->w - 1))
        {
            u8g2_DrawVLine(u8g2, line_x, grid_y, (uint8_t)grid_h);
        }
    }

    for (row = 1u; row < MOTOR_DATAFIELD_ROW_COUNT; row++)
    {
        int16_t line_y = (int16_t)(grid_y + (row * row_h));
        if (line_y < (int16_t)(grid_y + grid_h - 1))
        {
            u8g2_DrawHLine(u8g2, viewport->x, line_y, (uint8_t)viewport->w);
        }
    }
}
