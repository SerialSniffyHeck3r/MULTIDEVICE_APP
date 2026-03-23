#include "Vario_ValueSetting.h"

#include "Vario_Display_Common.h"
#include "Vario_Settings.h"
#include "Vario_State.h"

#include <stdio.h>

#define VARIO_VALUESETTING_VISIBLE_ROWS 6
#define VARIO_VALUESETTING_ROW_PITCH    15

static void vario_valuesetting_get_item_text(vario_value_item_t item,
                                             const vario_settings_t *settings,
                                             char *out_label,
                                             size_t label_len,
                                             char *out_value,
                                             size_t value_len)
{
    switch (item)
    {
        case VARIO_VALUE_ITEM_BRIGHTNESS:
            snprintf(out_label, label_len, "Brightness");
            snprintf(out_value, value_len, "%u%%", (unsigned)settings->display_brightness_percent);
            break;

        case VARIO_VALUE_ITEM_COMPASS_SPAN:
            snprintf(out_label, label_len, "Compass Span");
            snprintf(out_value, value_len, "%u deg", (unsigned)settings->compass_span_deg);
            break;

        case VARIO_VALUE_ITEM_COMPASS_BOX_HEIGHT:
            snprintf(out_label, label_len, "Compass BoxH");
            snprintf(out_value, value_len, "%u px", (unsigned)settings->compass_box_height_px);
            break;

        case VARIO_VALUE_ITEM_VARIO_RANGE:
            snprintf(out_label, label_len, "Vario Range");
            snprintf(out_value,
                     value_len,
                     "%u.%u m/s",
                     (unsigned)(settings->vario_range_mps_x10 / 10u),
                     (unsigned)(settings->vario_range_mps_x10 % 10u));
            break;

        case VARIO_VALUE_ITEM_GS_RANGE:
            snprintf(out_label, label_len, "GS Range");
            snprintf(out_value,
                     value_len,
                     "%ld %s",
                     (long)Vario_Settings_SpeedToDisplayRounded((float)settings->gs_range_kmh),
                     Vario_Settings_GetSpeedUnitText());
            break;

        case VARIO_VALUE_ITEM_TRAIL_RANGE:
            snprintf(out_label, label_len, "Trail Range");
            snprintf(out_value, value_len, "%u m", (unsigned)settings->trail_range_m);
            break;

        case VARIO_VALUE_ITEM_TRAIL_SPACING:
            snprintf(out_label, label_len, "Trail Step");
            snprintf(out_value, value_len, "%u m", (unsigned)settings->trail_spacing_m);
            break;

        case VARIO_VALUE_ITEM_TRAIL_DOT_SIZE:
            snprintf(out_label, label_len, "Trail Dot");
            snprintf(out_value, value_len, "%u px", (unsigned)settings->trail_dot_size_px);
            break;

        case VARIO_VALUE_ITEM_ARROW_SIZE:
            snprintf(out_label, label_len, "Arrow Size");
            snprintf(out_value, value_len, "%u px", (unsigned)settings->arrow_size_px);
            break;

        case VARIO_VALUE_ITEM_SHOW_TIME:
            snprintf(out_label, label_len, "Show Time");
            snprintf(out_value, value_len, "%s", (settings->show_current_time != 0u) ? "ON" : "OFF");
            break;

        case VARIO_VALUE_ITEM_SHOW_FLIGHT_TIME:
            snprintf(out_label, label_len, "Show FltT");
            snprintf(out_value, value_len, "%s", (settings->show_flight_time != 0u) ? "ON" : "OFF");
            break;

        case VARIO_VALUE_ITEM_SHOW_MAX_VARIO:
            snprintf(out_label, label_len, "Show MaxV");
            snprintf(out_value, value_len, "%s", (settings->show_max_vario != 0u) ? "ON" : "OFF");
            break;

        case VARIO_VALUE_ITEM_SHOW_GS_BAR:
            snprintf(out_label, label_len, "Show GS Bar");
            snprintf(out_value, value_len, "%s", (settings->show_gs_bar != 0u) ? "ON" : "OFF");
            break;

        case VARIO_VALUE_ITEM_COUNT:
        default:
            snprintf(out_label, label_len, "-");
            snprintf(out_value, value_len, "-");
            break;
    }
}

void Vario_ValueSetting_Render(u8g2_t *u8g2, const vario_buttonbar_t *buttonbar)
{
    const vario_viewport_t *v;
    const vario_settings_t *settings;
    uint8_t                 cursor;
    uint8_t                 first;
    uint8_t                 visible;
    uint8_t                 i;

    (void)buttonbar;

    v        = Vario_Display_GetContentViewport();
    settings = Vario_Settings_Get();
    cursor   = Vario_State_GetValueSettingCursor();
    visible  = VARIO_VALUESETTING_VISIBLE_ROWS;

    if (cursor > 2u)
    {
        first = (uint8_t)(cursor - 2u);
    }
    else
    {
        first = 0u;
    }

    if ((first + visible) > (uint8_t)VARIO_VALUE_ITEM_COUNT)
    {
        if ((uint8_t)VARIO_VALUE_ITEM_COUNT > visible)
        {
            first = (uint8_t)((uint8_t)VARIO_VALUE_ITEM_COUNT - visible);
        }
        else
        {
            first = 0u;
        }
    }

    Vario_Display_DrawPageTitle(u8g2, v, "DISPLAY", "HUD / GRAPHICS");

    for (i = 0u; i < visible; ++i)
    {
        uint8_t item_index;
        char    label[24];
        char    value[24];
        int16_t row_y;

        item_index = (uint8_t)(first + i);
        if (item_index >= (uint8_t)VARIO_VALUE_ITEM_COUNT)
        {
            break;
        }

        row_y = (int16_t)(v->y + 28 + ((int16_t)i * VARIO_VALUESETTING_ROW_PITCH));
        vario_valuesetting_get_item_text((vario_value_item_t)item_index,
                                         settings,
                                         label,
                                         sizeof(label),
                                         value,
                                         sizeof(value));

        Vario_Display_DrawMenuRow(u8g2,
                                  v,
                                  row_y,
                                  (cursor == item_index),
                                  label,
                                  value);
    }

    {
        char pos_text[20];
        snprintf(pos_text,
                 sizeof(pos_text),
                 "%u/%u",
                 (unsigned)(cursor + 1u),
                 (unsigned)VARIO_VALUE_ITEM_COUNT);
        u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
        Vario_Display_DrawTextRight(u8g2,
                                    (int16_t)(v->x + v->w - 6),
                                    (int16_t)(v->y + v->h - 6),
                                    pos_text);
        u8g2_DrawStr(u8g2,
                     (uint8_t)(v->x + 8),
                     (uint8_t)(v->y + v->h - 6),
                     "F6: Instrument page");
    }
}
