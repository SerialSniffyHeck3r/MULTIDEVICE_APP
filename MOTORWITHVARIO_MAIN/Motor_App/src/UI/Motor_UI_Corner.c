
#include "Motor_UI_Internal.h"

#include <stdio.h>
#include <stdlib.h>

void Motor_UI_DrawScreen_Corner(u8g2_t *u8g2, const motor_state_t *state)
{
    uint16_t i;
    uint16_t head;

    if ((u8g2 == 0) || (state == 0))
    {
        return;
    }

    Motor_UI_DrawStatusBar(u8g2, state);

    /* ---------------------------------------------------------------------- */
    /*  코너링 중심 화면                                                       */
    /*  - 좌측 상단: 현재 bank                                                  */
    /*  - 우측 상단: 현재 lateral G                                             */
    /*  - 중단: 좌/우 최대값                                                    */
    /*  - 하단: 최근 history strip                                              */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFont(u8g2, u8g2_font_9x15B_tr);
    u8g2_DrawStr(u8g2, 8, 32, "BANK");
    u8g2_DrawStr(u8g2, 130, 32, "LAT G");

    u8g2_SetFont(u8g2, u8g2_font_9x15_mn);
    {
        char buf[16];
        (void)snprintf(buf, sizeof(buf), "%+d.%01d", (int)(state->dyn.bank_deg_x10 / 10), (int)abs(state->dyn.bank_deg_x10 % 10));
        u8g2_DrawStr(u8g2, 8, 56, buf);
        (void)snprintf(buf, sizeof(buf), "%+ld.%01ld", (long)(state->dyn.lat_accel_mg / 1000), (long)abs((int)(state->dyn.lat_accel_mg % 1000) / 100));
        u8g2_DrawStr(u8g2, 130, 56, buf);
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
        u8g2_DrawStr(u8g2, 8, 68, buf);

        (void)snprintf(buf,
                       sizeof(buf),
                       "G L %+ld.%01ld  R %+ld.%01ld",
                       (long)(state->dyn.max_left_lat_mg / 1000),
                       (long)abs((int)(state->dyn.max_left_lat_mg % 1000) / 100),
                       (long)(state->dyn.max_right_lat_mg / 1000),
                       (long)abs((int)(state->dyn.max_right_lat_mg % 1000) / 100));
        u8g2_DrawStr(u8g2, 8, 78, buf);
    }

    /* ---------------------------------------------------------------------- */
    /*  history strip                                                           */
    /*  - 화면 하단 90~112 영역에 bank 라인                                     */
    /*  - 중앙 기준선 위/아래로 최근 코너링 값을 한눈에 본다.                   */
    /* ---------------------------------------------------------------------- */
    u8g2_DrawFrame(u8g2, 4, 86, 232, 28);
    u8g2_DrawHLine(u8g2, 6, 100, 228);
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
        x = (uint8_t)(6u + (i * 228u) / MOTOR_HISTORY_SAMPLE_COUNT);
        y0 = (int16_t)(100 - (bank_x10 / 20));
        y1 = (int16_t)(100 - (lat_x10 / 4));
        if (y0 < 88) y0 = 88;
        if (y0 > 112) y0 = 112;
        if (y1 < 88) y1 = 88;
        if (y1 > 112) y1 = 112;
        u8g2_DrawPixel(u8g2, x, (uint8_t)y0);
        u8g2_DrawPixel(u8g2, x, (uint8_t)y1);
    }

    Motor_UI_DrawBottomHint(u8g2, "3 MARK", "4 PEAK", "6 MENU");
}
