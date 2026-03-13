#include "ui_screen_test.h"
#include "ui_common_icons.h"

#include <stdio.h>

static const uint8_t *ui_screen_test_get_cute_icon(uint8_t index)
{
    switch (index & 0x03u)
    {
    case 0u: return icon_cute_cat_8x8;
    case 1u: return icon_cute_heart_8x8;
    case 2u: return icon_cute_star_8x8;
    case 3u:
    default: return icon_cute_bike_8x8;
    }
}

static const char *ui_screen_test_layout_text(ui_layout_mode_t mode)
{
    switch (mode)
    {
    case UI_LAYOUT_MODE_TOP_EXTENDED_NO_BOTTOM: return "TOP+EXT";
    case UI_LAYOUT_MODE_TOP_EXTENDED_OVERLAY:   return "TOP+OVR";
    case UI_LAYOUT_MODE_TOP_BOTTOM_FIXED:       return "TOP+BOT";
    case UI_LAYOUT_MODE_FULLSCREEN:             return "FULLSCR";
    default:                                    return "UNKNOWN";
    }
}

void UI_ScreenTest_Draw(u8g2_t *u8g2,
                        const ui_rect_t *viewport,
                        uint32_t live_counter_20hz,
                        bool updates_paused,
                        bool blink_phase_on,
                        uint8_t cute_icon_index,
                        ui_layout_mode_t layout_mode)
{
    const uint8_t *cute_icon;
    char line[32];
    char counter_str[8];
    int16_t panel_x;
    int16_t panel_y;
    int16_t panel_w;
    int16_t panel_h;
    int16_t counter_x;
    int16_t counter_base_y;
    int16_t progress_x;
    int16_t progress_y;
    int16_t progress_w;
    int16_t mover_x;
    int16_t icon_row_y;
    uint32_t counter4;

    if ((u8g2 == 0) || (viewport == 0))
    {
        return;
    }

    if ((viewport->w <= 0) || (viewport->h <= 0))
    {
        return;
    }

    cute_icon = ui_screen_test_get_cute_icon(cute_icon_index);
    counter4 = live_counter_20hz % 10000u;

    panel_x = (int16_t)(viewport->x + 4);
    panel_y = (int16_t)(viewport->y + 4);
    panel_w = (int16_t)(viewport->w - 8);
    panel_h = 28;
    if (panel_h > (viewport->h - 8))
    {
        panel_h = (int16_t)(viewport->h - 8);
    }
    if (panel_h < 12)
    {
        panel_h = 12;
    }

    progress_x = (int16_t)(viewport->x + 8);
    progress_w = (int16_t)(viewport->w - 16);
    progress_y = (int16_t)(viewport->y + viewport->h - 10);
    if (progress_w < 12)
    {
        progress_w = 12;
    }

    mover_x = (int16_t)(progress_x + 1);
    if (progress_w > 10)
    {
        mover_x = (int16_t)(progress_x + 1 + (int16_t)(live_counter_20hz % (uint32_t)(progress_w - 8)));
    }

    counter_x = (int16_t)(viewport->x + 10);
    counter_base_y = (int16_t)(viewport->y + 70);
    if (counter_base_y > (viewport->y + viewport->h - 28))
    {
        counter_base_y = (int16_t)(viewport->y + viewport->h - 28);
    }

    icon_row_y = (int16_t)(viewport->y + viewport->h - 24);
    if (icon_row_y < (panel_y + panel_h + 8))
    {
        icon_row_y = (int16_t)(panel_y + panel_h + 8);
    }

    /* ---------------------------------------------------------------------- */
    /*  Blink panel                                                             */
    /*                                                                            */
    /*  이전 패키지처럼 프레임마다 on/off 하지 않고,                              */
    /*  SlowToggle2Hz 기반으로 큰 패널만 천천히 깜빡이게 바꿨다.                 */
    /* ---------------------------------------------------------------------- */
    if (blink_phase_on != false)
    {
        u8g2_SetDrawColor(u8g2, 1);
        u8g2_DrawFrame(u8g2,
                       (u8g2_uint_t)panel_x,
                       (u8g2_uint_t)panel_y,
                       (u8g2_uint_t)panel_w,
                       (u8g2_uint_t)panel_h);
        u8g2_SetFont(u8g2, u8g2_font_7x13_mf);
        u8g2_DrawStr(u8g2,
                     (u8g2_uint_t)(panel_x + 8),
                     (u8g2_uint_t)(panel_y + 14),
                     "TEST UI ENGINE");

        u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
        snprintf(line, sizeof(line), "BLINK ON  %s", updates_paused ? "HOLD" : "RUN");
        u8g2_DrawStr(u8g2,
                     (u8g2_uint_t)(panel_x + 8),
                     (u8g2_uint_t)(panel_y + panel_h - 4),
                     line);
    }
    else
    {
        u8g2_SetDrawColor(u8g2, 1);
        u8g2_DrawBox(u8g2,
                     (u8g2_uint_t)panel_x,
                     (u8g2_uint_t)panel_y,
                     (u8g2_uint_t)panel_w,
                     (u8g2_uint_t)panel_h);
        u8g2_SetDrawColor(u8g2, 0);
        u8g2_SetFont(u8g2, u8g2_font_7x13_mf);
        u8g2_DrawStr(u8g2,
                     (u8g2_uint_t)(panel_x + 8),
                     (u8g2_uint_t)(panel_y + 14),
                     "TEST UI ENGINE");

        u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
        snprintf(line, sizeof(line), "BLINK OFF %s", updates_paused ? "HOLD" : "RUN");
        u8g2_DrawStr(u8g2,
                     (u8g2_uint_t)(panel_x + 8),
                     (u8g2_uint_t)(panel_y + panel_h - 4),
                     line);
        u8g2_SetDrawColor(u8g2, 1);
    }

    /* ---------------------------------------------------------------------- */
    /*  Layout / controls info                                                  */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    snprintf(line, sizeof(line), "MODE %s", ui_screen_test_layout_text(layout_mode));
    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)(viewport->x + 8),
                 (u8g2_uint_t)(panel_y + panel_h + 9),
                 line);

    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)(viewport->x + 8),
                 (u8g2_uint_t)(panel_y + panel_h + 18),
                 "F1 DBG F2 HOLD F3 MODE");

    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)(viewport->x + 8),
                 (u8g2_uint_t)(panel_y + panel_h + 27),
                 "F4 TOAST F5 POPUP F6 ICON");

    /* ---------------------------------------------------------------------- */
    /*  20Hz counter                                                            */
    /* ---------------------------------------------------------------------- */
    snprintf(counter_str, sizeof(counter_str), "%04lu", (unsigned long)counter4);
    u8g2_SetFont(u8g2, u8g2_font_10x20_mf);
    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)counter_x,
                 (u8g2_uint_t)counter_base_y,
                 counter_str);

    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)counter_x,
                 (u8g2_uint_t)(counter_base_y + 9),
                 "20Hz ++ COUNTER");

    /* selected cute icon */
    u8g2_DrawXBM(u8g2,
                 (u8g2_uint_t)(viewport->x + viewport->w - 20),
                 (u8g2_uint_t)(counter_base_y - 16),
                 ICON8_W,
                 ICON8_H,
                 cute_icon);
    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)(viewport->x + viewport->w - 34),
                 (u8g2_uint_t)(counter_base_y + 9),
                 "CUTE");

    /* ---------------------------------------------------------------------- */
    /*  Moving bar / dynamic motion                                             */
    /* ---------------------------------------------------------------------- */
    u8g2_DrawFrame(u8g2,
                   (u8g2_uint_t)progress_x,
                   (u8g2_uint_t)progress_y,
                   (u8g2_uint_t)progress_w,
                   7u);
    u8g2_DrawBox(u8g2,
                 (u8g2_uint_t)mover_x,
                 (u8g2_uint_t)(progress_y + 1),
                 6u,
                 5u);

    /* ---------------------------------------------------------------------- */
    /*  Meaningful icon samples                                                 */
    /* ---------------------------------------------------------------------- */
    u8g2_DrawXBM(u8g2, (u8g2_uint_t)(viewport->x + 10), (u8g2_uint_t)icon_row_y, ICON8_W, ICON8_H, icon_ui_info_8x8);
    u8g2_DrawXBM(u8g2, (u8g2_uint_t)(viewport->x + 24), (u8g2_uint_t)icon_row_y, ICON8_W, ICON8_H, icon_ui_warn_8x8);
    u8g2_DrawXBM(u8g2, (u8g2_uint_t)(viewport->x + 38), (u8g2_uint_t)icon_row_y, ICON8_W, ICON8_H, icon_ui_ok_8x8);
    u8g2_DrawXBM(u8g2, (u8g2_uint_t)(viewport->x + 52), (u8g2_uint_t)icon_row_y, ICON8_W, ICON8_H, icon_ui_bell_8x8);
    u8g2_DrawXBM(u8g2, (u8g2_uint_t)(viewport->x + 66), (u8g2_uint_t)icon_row_y, ICON8_W, ICON8_H, icon_ui_folder_8x8);

    /* ---------------------------------------------------------------------- */
    /*  4px arrow samples                                                      */
    /* ---------------------------------------------------------------------- */
    u8g2_DrawXBM(u8g2, (u8g2_uint_t)(viewport->x + 84),  (u8g2_uint_t)(icon_row_y + 2), ICON7X4_W, ICON7X4_H, icon_arrow_left_7x4);
    u8g2_DrawXBM(u8g2, (u8g2_uint_t)(viewport->x + 96),  (u8g2_uint_t)(icon_row_y + 2), ICON7X4_W, ICON7X4_H, icon_arrow_right_7x4);
    u8g2_DrawXBM(u8g2, (u8g2_uint_t)(viewport->x + 108), (u8g2_uint_t)(icon_row_y + 2), ICON7X4_W, ICON7X4_H, icon_arrow_up_7x4);
    u8g2_DrawXBM(u8g2, (u8g2_uint_t)(viewport->x + 120), (u8g2_uint_t)(icon_row_y + 2), ICON7X4_W, ICON7X4_H, icon_arrow_down_7x4);

    if (updates_paused != false)
    {
        u8g2_DrawFrame(u8g2,
                       (u8g2_uint_t)(viewport->x + viewport->w - 54),
                       (u8g2_uint_t)(icon_row_y - 1),
                       46u,
                       10u);
        u8g2_DrawStr(u8g2,
                     (u8g2_uint_t)(viewport->x + viewport->w - 49),
                     (u8g2_uint_t)(icon_row_y + 7),
                     "PAUSED");
    }
}
