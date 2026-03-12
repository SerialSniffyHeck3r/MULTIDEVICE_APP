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

/* -------------------------------------------------------------------------- */
/*  Test screen                                                                 */
/*                                                                            */
/*  요구사항                                                                   */
/*  - 상/하단바가 정상 동작하는 상태에서 본문만 20Hz로 blink                     */
/*  - 이후 F3으로 레이아웃 모드를 바꿔도, 본문 뷰포트 안에서만 blink             */
/*    효과가 유지되도록 구성                                                    */
/* -------------------------------------------------------------------------- */
void UI_ScreenTest_Draw(u8g2_t *u8g2,
                        const ui_rect_t *viewport,
                        uint32_t frame_tick_20hz,
                        bool blink_paused,
                        uint8_t cute_icon_index,
                        ui_layout_mode_t layout_mode)
{
    bool phase_on;
    const uint8_t *cute_icon;
    int16_t center_x;
    int16_t center_y;
    char line[32];

    if ((u8g2 == 0) || (viewport == 0))
    {
        return;
    }

    if ((viewport->w <= 0) || (viewport->h <= 0))
    {
        return;
    }

    phase_on = (blink_paused != false) ? true : ((frame_tick_20hz & 1u) == 0u);
    cute_icon = ui_screen_test_get_cute_icon(cute_icon_index);

    center_x = (int16_t)(viewport->x + (viewport->w / 2));
    center_y = (int16_t)(viewport->y + (viewport->h / 2));

    if (phase_on != false)
    {
        u8g2_SetDrawColor(u8g2, 1);
        u8g2_DrawFrame(u8g2,
                       (u8g2_uint_t)viewport->x,
                       (u8g2_uint_t)viewport->y,
                       (u8g2_uint_t)viewport->w,
                       (u8g2_uint_t)viewport->h);

        u8g2_SetFont(u8g2, u8g2_font_7x13_mf);
        u8g2_DrawStr(u8g2,
                     (u8g2_uint_t)(viewport->x + 12),
                     (u8g2_uint_t)(viewport->y + 18),
                     "TEST UI");

        u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
        snprintf(line, sizeof(line), "20Hz BLINK  %s", blink_paused ? "PAUSE" : "RUN");
        u8g2_DrawStr(u8g2,
                     (u8g2_uint_t)(viewport->x + 12),
                     (u8g2_uint_t)(viewport->y + 32),
                     line);

        snprintf(line, sizeof(line), "MODE %s", ui_screen_test_layout_text(layout_mode));
        u8g2_DrawStr(u8g2,
                     (u8g2_uint_t)(viewport->x + 12),
                     (u8g2_uint_t)(viewport->y + 44),
                     line);

        u8g2_DrawStr(u8g2,
                     (u8g2_uint_t)(viewport->x + 12),
                     (u8g2_uint_t)(viewport->y + 56),
                     "F1 DBG F2 PAUSE");

        u8g2_DrawStr(u8g2,
                     (u8g2_uint_t)(viewport->x + 12),
                     (u8g2_uint_t)(viewport->y + 68),
                     "F3 MODE F4/F5 MSG");

        u8g2_DrawXBM(u8g2,
                     (u8g2_uint_t)(center_x - 4),
                     (u8g2_uint_t)(center_y - 4),
                     ICON8_W,
                     ICON8_H,
                     cute_icon);

        /* 본문 전체가 살아있음을 더 명확히 보여주기 위해
         * 가운데에 작은 십자선도 함께 그린다. */
        u8g2_DrawHLine(u8g2,
                       (u8g2_uint_t)(center_x - 12),
                       (u8g2_uint_t)center_y,
                       25u);
        u8g2_DrawVLine(u8g2,
                       (u8g2_uint_t)center_x,
                       (u8g2_uint_t)(center_y - 12),
                       25u);
    }
    else
    {
        /* off phase에서는 본문 뷰포트만 검정으로 꽉 채워서
         * 상/하단바는 그대로 두고 메인 UI 영역만 강하게 깜빡이게 만든다. */
        u8g2_SetDrawColor(u8g2, 1);
        u8g2_DrawBox(u8g2,
                     (u8g2_uint_t)viewport->x,
                     (u8g2_uint_t)viewport->y,
                     (u8g2_uint_t)viewport->w,
                     (u8g2_uint_t)viewport->h);

        u8g2_SetDrawColor(u8g2, 0);
        u8g2_SetFont(u8g2, u8g2_font_7x13_mf);
        u8g2_DrawStr(u8g2,
                     (u8g2_uint_t)(viewport->x + 12),
                     (u8g2_uint_t)(viewport->y + 18),
                     "TEST UI");

        u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
        u8g2_DrawStr(u8g2,
                     (u8g2_uint_t)(viewport->x + 12),
                     (u8g2_uint_t)(viewport->y + 32),
                     "MAIN AREA OFF");

        u8g2_DrawXBM(u8g2,
                     (u8g2_uint_t)(center_x - 4),
                     (u8g2_uint_t)(center_y - 4),
                     ICON8_W,
                     ICON8_H,
                     cute_icon);

        u8g2_SetDrawColor(u8g2, 1);
    }
}
