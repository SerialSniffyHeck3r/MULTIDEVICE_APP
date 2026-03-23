#include "Motor_UI_Internal.h"

#include <stdio.h>
#include <stdlib.h>

void Motor_UI_DrawScreen_Corner(u8g2_t *u8g2, const ui_rect_t *viewport, const motor_state_t *state)
{
    uint16_t i;
    uint16_t head;
    int16_t base_y;

    if ((u8g2 == 0) || (viewport == 0) || (state == 0))
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  코너링 중심 화면                                                       */
    /*  - 상단: 현재 bank / lateral G                                           */
    /*  - 중단: 좌우 peak summary                                               */
    /*  - 하단: 최근 history strip                                              */
    /* ---------------------------------------------------------------------- */
    Motor_UI_DrawViewportTitle(u8g2, viewport, "CORNERING DYNAMICS");
    base_y = viewport->y + 18;

    u8g2_SetFont(u8g2, u8g2_font_9x15B_tr);
    u8g2_DrawStr(u8g2, viewport->x + 8, base_y + 14, "BANK");
    u8g2_DrawStr(u8g2, viewport->x + 130, base_y + 14, "LAT G");

    u8g2_SetFont(u8g2, u8g2_font_9x15B_tr);
    {
        char buf[16];
        (void)snprintf(buf, sizeof(buf), "%+d.%01d", (int)(state->dyn.bank_deg_x10 / 10), (int)abs(state->dyn.bank_deg_x10 % 10));
        u8g2_DrawStr(u8g2, viewport->x + 8, base_y + 38, buf);
        (void)snprintf(buf, sizeof(buf), "%+ld.%01ld", (long)(state->dyn.lat_accel_mg / 1000), (long)abs((int)(state->dyn.lat_accel_mg % 1000) / 100));
        u8g2_DrawStr(u8g2, viewport->x + 130, base_y + 38, buf);
    }

    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    {
        char buf[48];
        (void)snprintf(buf,
                       sizeof(buf),
                       "MAX L %+d.%01d  R %+d.%01d",
                       (int)(state->dyn.max_left_bank_deg_x10 / 10),
                       (int)abs(state->dyn.max_left_bank_deg_x10 % 10),
                       (int)(state->dyn.max_right_bank_deg_x10 / 10),
                       (int)abs(state->dyn.max_right_bank_deg_x10 % 10));
        u8g2_DrawStr(u8g2, viewport->x + 8, base_y + 50, buf);

        (void)snprintf(buf,
                       sizeof(buf),
                       "G L %+ld.%01ld  R %+ld.%01ld",
                       (long)(state->dyn.max_left_lat_mg / 1000),
                       (long)abs((int)(state->dyn.max_left_lat_mg % 1000) / 100),
                       (long)(state->dyn.max_right_lat_mg / 1000),
                       (long)abs((int)(state->dyn.max_right_lat_mg % 1000) / 100));
        u8g2_DrawStr(u8g2, viewport->x + 8, base_y + 60, buf);
    }

    /* history strip frame */
    u8g2_DrawFrame(u8g2, viewport->x + 4, viewport->y + viewport->h - 30, viewport->w - 8, 26);
    u8g2_DrawHLine(u8g2, viewport->x + 6, viewport->y + viewport->h - 17, viewport->w - 12);
    head = state->dyn.history_head;
    for (i = 0u; i < MOTOR_HISTORY_SAMPLE_COUNT; i++)
    {
        uint16_t src_idx;
        int16_t bank_x10;
        int16_t lat_x10;
        int16_t y0;
        int16_t y1;
        uint8_t x;

        src_idx = (uint16_t)((head + i) % MOTOR_HISTORY_SAMPLE_COUNT);
        bank_x10 = state->dyn.bank_history_x10[src_idx];
        lat_x10 = state->dyn.lat_history_x10[src_idx];
        x = (uint8_t)(viewport->x + 6 + (i * (uint16_t)(viewport->w - 12)) / MOTOR_HISTORY_SAMPLE_COUNT);
        y0 = (int16_t)(viewport->y + viewport->h - 17 - (bank_x10 / 20));
        y1 = (int16_t)(viewport->y + viewport->h - 17 - (lat_x10 / 4));
        if (y0 < viewport->y + viewport->h - 28) y0 = viewport->y + viewport->h - 28;
        if (y0 > viewport->y + viewport->h - 4)  y0 = viewport->y + viewport->h - 4;
        if (y1 < viewport->y + viewport->h - 28) y1 = viewport->y + viewport->h - 28;
        if (y1 > viewport->y + viewport->h - 4)  y1 = viewport->y + viewport->h - 4;
        u8g2_DrawPixel(u8g2, x, (uint8_t)y0);
        u8g2_DrawPixel(u8g2, x, (uint8_t)y1);
    }
}
