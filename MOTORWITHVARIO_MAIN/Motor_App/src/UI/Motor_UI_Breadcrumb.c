
#include "Motor_UI_Internal.h"

#include <stdint.h>

void Motor_UI_DrawScreen_Breadcrumb(u8g2_t *u8g2, const motor_state_t *state)
{
    int32_t min_lat;
    int32_t max_lat;
    int32_t min_lon;
    int32_t max_lon;
    uint16_t i;

    if ((u8g2 == 0) || (state == 0))
    {
        return;
    }

    Motor_UI_DrawStatusBar(u8g2, state);

    /* ---------------------------------------------------------------------- */
    /*  breadcrumb plot                                                         */
    /*  - 10,14 ~ 230,112 영역을 2D track canvas로 사용                        */
    /*  - 최근 breadcrumb point ring을 bounding box에 맞춰 투영                 */
    /*  - 현재 위치는 채운 원으로, 홈 위치는 십자 표시로 찍는다.                */
    /* ---------------------------------------------------------------------- */
    u8g2_DrawFrame(u8g2, 10, 14, 220, 98);

    min_lat = 0x7FFFFFFF;
    max_lat = (int32_t)0x80000000;
    min_lon = 0x7FFFFFFF;
    max_lon = (int32_t)0x80000000;

    for (i = 0u; i < state->breadcrumb.count; i++)
    {
        const motor_breadcrumb_point_t *p;
        uint16_t idx;
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

    if ((max_lat > min_lat) && (max_lon > min_lon))
    {
        for (i = 0u; i < state->breadcrumb.count; i++)
        {
            const motor_breadcrumb_point_t *p;
            uint16_t idx;
            uint8_t x;
            uint8_t y;

            idx = (uint16_t)((state->breadcrumb.head + MOTOR_BREADCRUMB_POINT_COUNT - state->breadcrumb.count + i) % MOTOR_BREADCRUMB_POINT_COUNT);
            p = &state->breadcrumb.points[idx];
            if (p->valid == 0u)
            {
                continue;
            }

            x = (uint8_t)(12u + ((uint32_t)(p->lon_e7 - min_lon) * 216u) / (uint32_t)(max_lon - min_lon));
            y = (uint8_t)(110u - ((uint32_t)(p->lat_e7 - min_lat) * 94u) / (uint32_t)(max_lat - min_lat));
            u8g2_DrawPixel(u8g2, x, y);
        }

        if (state->breadcrumb.home_valid != 0u)
        {
            uint8_t hx;
            uint8_t hy;
            hx = (uint8_t)(12u + ((uint32_t)(state->breadcrumb.home_lon_e7 - min_lon) * 216u) / (uint32_t)(max_lon - min_lon));
            hy = (uint8_t)(110u - ((uint32_t)(state->breadcrumb.home_lat_e7 - min_lat) * 94u) / (uint32_t)(max_lat - min_lat));
            u8g2_DrawLine(u8g2, hx - 3u, hy, hx + 3u, hy);
            u8g2_DrawLine(u8g2, hx, hy - 3u, hx, hy + 3u);
        }

        {
            uint8_t cx;
            uint8_t cy;
            cx = (uint8_t)(12u + ((uint32_t)(state->nav.lon_e7 - min_lon) * 216u) / (uint32_t)(max_lon - min_lon));
            cy = (uint8_t)(110u - ((uint32_t)(state->nav.lat_e7 - min_lat) * 94u) / (uint32_t)(max_lat - min_lat));
            u8g2_DrawDisc(u8g2, cx, cy, 2, U8G2_DRAW_ALL);
        }
    }

    Motor_UI_DrawBottomHint(u8g2, "HOME+", "TRACK", "6 MENU");
}
