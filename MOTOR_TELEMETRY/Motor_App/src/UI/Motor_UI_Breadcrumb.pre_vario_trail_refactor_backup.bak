#include "Motor_UI_Internal.h"

#include <limits.h>
#include <stdio.h>

void Motor_UI_DrawScreen_Breadcrumb(u8g2_t *u8g2, const ui_rect_t *viewport, const motor_state_t *state)
{
    int32_t min_lat;
    int32_t max_lat;
    int32_t min_lon;
    int32_t max_lon;
    int16_t plot_x;
    int16_t plot_y;
    int16_t plot_w;
    int16_t plot_h;
    uint16_t i;
    char buf[32];

    if ((u8g2 == 0) || (viewport == 0) || (state == 0))
    {
        return;
    }

    Motor_UI_DrawViewportTitle(u8g2, viewport, "GPS BREADCRUMB");

    /* ---------------------------------------------------------------------- */
    /*  중앙 트랙 플롯 영역                                                    */
    /*  - viewport 내부의 대부분을 사각 프레임으로 확보한다.                    */
    /*  - breadcrumb ring buffer를 min/max normalization으로 그린다.            */
    /* ---------------------------------------------------------------------- */
    plot_x = (int16_t)(viewport->x + 6);
    plot_y = (int16_t)(viewport->y + 16);
    plot_w = (int16_t)(viewport->w - 12);
    plot_h = (int16_t)(viewport->h - 28);
    u8g2_DrawFrame(u8g2, plot_x, plot_y, plot_w, plot_h);

    min_lat = INT_MAX;
    max_lat = INT_MIN;
    min_lon = INT_MAX;
    max_lon = INT_MIN;

    for (i = 0u; i < state->breadcrumb.count; i++)
    {
        uint16_t idx;
        const motor_breadcrumb_point_t *p;

        idx = (uint16_t)((state->breadcrumb.head + MOTOR_BREADCRUMB_POINT_COUNT - state->breadcrumb.count + i) % MOTOR_BREADCRUMB_POINT_COUNT);
        p = &state->breadcrumb.points[idx];
        if (p->valid == 0u)
        {
            continue;
        }

        if (p->lat_e7 < min_lat) min_lat = p->lat_e7;
        if (p->lat_e7 > max_lat) max_lat = p->lat_e7;
        if (p->lon_e7 < min_lon) min_lon = p->lon_e7;
        if (p->lon_e7 > max_lon) max_lon = p->lon_e7;
    }

    if ((state->breadcrumb.count == 0u) || (min_lat == INT_MAX) || (max_lat <= min_lat) || (max_lon <= min_lon))
    {
        Motor_UI_DrawCenteredTextBlock(u8g2,
                                       viewport,
                                       (int16_t)((viewport->h / 2) - 10),
                                       "TRACK EMPTY",
                                       "Ride and move to collect",
                                       "breadcrumb points");
    }
    else
    {
        for (i = 0u; i < state->breadcrumb.count; i++)
        {
            uint16_t idx;
            const motor_breadcrumb_point_t *p;
            int16_t x;
            int16_t y;

            idx = (uint16_t)((state->breadcrumb.head + MOTOR_BREADCRUMB_POINT_COUNT - state->breadcrumb.count + i) % MOTOR_BREADCRUMB_POINT_COUNT);
            p = &state->breadcrumb.points[idx];
            if (p->valid == 0u)
            {
                continue;
            }

            x = (int16_t)(plot_x + 2 + ((int64_t)(p->lon_e7 - min_lon) * (plot_w - 4)) / (max_lon - min_lon));
            y = (int16_t)(plot_y + plot_h - 3 - ((int64_t)(p->lat_e7 - min_lat) * (plot_h - 4)) / (max_lat - min_lat));
            u8g2_DrawPixel(u8g2, x, y);
        }

        /* home marker: 십자표시 */
        if (state->breadcrumb.home_valid != 0u)
        {
            int16_t hx;
            int16_t hy;
            hx = (int16_t)(plot_x + 2 + ((int64_t)(state->breadcrumb.home_lon_e7 - min_lon) * (plot_w - 4)) / (max_lon - min_lon));
            hy = (int16_t)(plot_y + plot_h - 3 - ((int64_t)(state->breadcrumb.home_lat_e7 - min_lat) * (plot_h - 4)) / (max_lat - min_lat));
            u8g2_DrawLine(u8g2, hx - 3, hy, hx + 3, hy);
            u8g2_DrawLine(u8g2, hx, hy - 3, hx, hy + 3);
        }

        /* current marker: filled disc */
        if (state->nav.valid != false)
        {
            int16_t cx;
            int16_t cy;
            cx = (int16_t)(plot_x + 2 + ((int64_t)(state->nav.lon_e7 - min_lon) * (plot_w - 4)) / (max_lon - min_lon));
            cy = (int16_t)(plot_y + plot_h - 3 - ((int64_t)(state->nav.lat_e7 - min_lat) * (plot_h - 4)) / (max_lat - min_lat));
            u8g2_DrawDisc(u8g2, cx, cy, 2, U8G2_DRAW_ALL);
        }

        u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
        (void)snprintf(buf, sizeof(buf), "PTS %u", (unsigned)state->breadcrumb.count);
        u8g2_DrawStr(u8g2, plot_x + 4, plot_y + 8, buf);
        u8g2_DrawStr(u8g2, plot_x + plot_w - 54, plot_y + 8, "HOME + CUR");
    }
}
