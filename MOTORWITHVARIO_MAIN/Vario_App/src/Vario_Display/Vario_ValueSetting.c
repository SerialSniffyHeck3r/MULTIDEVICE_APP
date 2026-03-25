#include "Vario_ValueSetting.h"

#include "Vario_Display_Common.h"
#include "Vario_Settings.h"
#include "Vario_State.h"

#include <stdio.h>

#define VARIO_VALUESETTING_VISIBLE_ROWS 6
#define VARIO_VALUESETTING_ROW_PITCH    15

static const char *vario_valuesetting_get_subtitle(vario_settings_category_t category)
{
    switch (category)
    {
        case VARIO_SETTINGS_CATEGORY_SYSTEM:
            return "UNITS / FORMAT";

        case VARIO_SETTINGS_CATEGORY_DISPLAY:
            return "PANEL / BAR";

        case VARIO_SETTINGS_CATEGORY_AUDIO:
            return "LIVE VARIO";

        case VARIO_SETTINGS_CATEGORY_LOG:
            return "PLOT / TRAIL";

        case VARIO_SETTINGS_CATEGORY_FLIGHT:
            return "ALT / VARIO";

        case VARIO_SETTINGS_CATEGORY_BLUETOOTH:
            return "LINK / TEST";

        case VARIO_SETTINGS_CATEGORY_COUNT:
        default:
            return "DETAIL";
    }
}

void Vario_ValueSetting_Render(u8g2_t *u8g2, const vario_buttonbar_t *buttonbar)
{
    const vario_viewport_t    *v;
    vario_settings_category_t  category;
    uint8_t                    cursor;
    uint8_t                    first;
    uint8_t                    visible;
    uint8_t                    item_count;
    uint8_t                    i;

    (void)buttonbar;

    v         = Vario_Display_GetContentViewport();
    category  = Vario_State_GetSettingsCategory();
    cursor    = Vario_State_GetValueSettingCursor();
    visible   = VARIO_VALUESETTING_VISIBLE_ROWS;
    item_count = Vario_Settings_GetCategoryItemCount(category);

    if ((u8g2 == NULL) || (v == NULL))
    {
        return;
    }

    if (cursor > 2u)
    {
        first = (uint8_t)(cursor - 2u);
    }
    else
    {
        first = 0u;
    }

    if ((first + visible) > item_count)
    {
        if (item_count > visible)
        {
            first = (uint8_t)(item_count - visible);
        }
        else
        {
            first = 0u;
        }
    }

    Vario_Display_DrawPageTitle(u8g2,
                                v,
                                Vario_Settings_GetCategoryText(category),
                                vario_valuesetting_get_subtitle(category));

    for (i = 0u; i < visible; ++i)
    {
        uint8_t item_index;
        char    label[24];
        char    value[24];
        int16_t row_y;

        item_index = (uint8_t)(first + i);
        if (item_index >= item_count)
        {
            break;
        }

        row_y = (int16_t)(v->y + 28 + ((int16_t)i * VARIO_VALUESETTING_ROW_PITCH));
        Vario_Settings_GetCategoryItemText(category,
                                           item_index,
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
}
