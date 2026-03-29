#include "Vario_QuickSet.h"

#include "Vario_Display_Common.h"
#include "Vario_Settings.h"
#include "Vario_State.h"

#include <stdio.h>

#define VARIO_QUICKSET_VISIBLE_ROWS 6
#define VARIO_QUICKSET_ROW_PITCH    15

void Vario_QuickSet_Render(u8g2_t *u8g2, const vario_buttonbar_t *buttonbar)
{
    const vario_viewport_t *v;
    uint8_t                 cursor;
    uint8_t                 visible;
    uint8_t                 i;

    (void)buttonbar;

    v       = Vario_Display_GetContentViewport();
    cursor  = Vario_State_GetQuickSetCursor();
    visible = VARIO_QUICKSET_VISIBLE_ROWS;

    if ((u8g2 == NULL) || (v == NULL))
    {
        return;
    }

    Vario_Display_DrawPageTitle(u8g2, v, "SETUP", "CATEGORIES");

    for (i = 0u; i < visible; ++i)
    {
        int16_t row_y;

        if (i >= (uint8_t)VARIO_SETTINGS_CATEGORY_COUNT)
        {
            break;
        }

        row_y = (int16_t)(v->y + 28 + ((int16_t)i * VARIO_QUICKSET_ROW_PITCH));
        Vario_Display_DrawMenuRow(u8g2,
                                  v,
                                  row_y,
                                  (cursor == i),
                                  Vario_Settings_GetCategoryText((vario_settings_category_t)i),
                                  "");
    }
}
