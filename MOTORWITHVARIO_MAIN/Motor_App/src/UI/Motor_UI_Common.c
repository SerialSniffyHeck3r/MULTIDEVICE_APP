
#include "Motor_UI_Internal.h"

#include "Motor_Units.h"

#include <stdio.h>
#include <string.h>

void Motor_UI_DrawStatusBar(u8g2_t *u8g2, const motor_state_t *state)
{
    char left[24];
    char center[24];
    char right[32];

    if ((u8g2 == 0) || (state == 0))
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  status bar 영역                                                         */
    /*  - y=0~10 정도의 얇은 상단 띠를 사용한다.                                */
    /*  - 좌측: REC / GPS / SD / OBD 상태                                      */
    /*  - 중앙: 현재 화면 짧은 이름                                             */
    /*  - 우측: 시간                                                             */
    /* ---------------------------------------------------------------------- */
    u8g2_DrawBox(u8g2, 0, 0, 240, 10);
    u8g2_SetDrawColor(u8g2, 0);
    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);

    (void)snprintf(left,
                   sizeof(left),
                   "%s %s %s",
                   ((motor_record_state_t)state->record.state == MOTOR_RECORD_STATE_RECORDING) ? "REC" : "---",
                   (state->nav.fix_ok != false) ? "GPS" : "NOFIX",
                   (state->snapshot.sd.mounted != false) ? "SD" : "NOSD");

    switch ((motor_screen_t)state->ui.screen)
    {
    case MOTOR_SCREEN_MAIN:              (void)snprintf(center, sizeof(center), "MAIN"); break;
    case MOTOR_SCREEN_DATA_FIELD_1:      (void)snprintf(center, sizeof(center), "FIELD1"); break;
    case MOTOR_SCREEN_DATA_FIELD_2:      (void)snprintf(center, sizeof(center), "FIELD2"); break;
    case MOTOR_SCREEN_CORNER:            (void)snprintf(center, sizeof(center), "CORNER"); break;
    case MOTOR_SCREEN_COMPASS:           (void)snprintf(center, sizeof(center), "COMPASS"); break;
    case MOTOR_SCREEN_BREADCRUMB:        (void)snprintf(center, sizeof(center), "TRACK"); break;
    case MOTOR_SCREEN_ALTITUDE:          (void)snprintf(center, sizeof(center), "ALT"); break;
    case MOTOR_SCREEN_HORIZON:           (void)snprintf(center, sizeof(center), "HORIZON"); break;
    case MOTOR_SCREEN_VEHICLE_SUMMARY:   (void)snprintf(center, sizeof(center), "VEHICLE"); break;
    case MOTOR_SCREEN_MENU:              (void)snprintf(center, sizeof(center), "MENU"); break;
    default:                             (void)snprintf(center, sizeof(center), "SETUP"); break;
    }

    if (state->snapshot.clock.local.year >= 2000u)
    {
        (void)snprintf(right,
                       sizeof(right),
                       "%02u:%02u:%02u",
                       (unsigned)state->snapshot.clock.local.hour,
                       (unsigned)state->snapshot.clock.local.min,
                       (unsigned)state->snapshot.clock.local.sec);
    }
    else
    {
        (void)snprintf(right, sizeof(right), "--:--:--");
    }

    u8g2_DrawStr(u8g2, 2, 8, left);
    u8g2_DrawStr(u8g2, 103, 8, center);
    u8g2_DrawStr(u8g2, 194, 8, right);
    u8g2_SetDrawColor(u8g2, 1);
}

void Motor_UI_DrawBottomHint(u8g2_t *u8g2, const char *left, const char *center, const char *right)
{
    if (u8g2 == 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  bottom hint bar                                                         */
    /*  - y=118~127 하단 10픽셀 높이                                            */
    /*  - 좌/중/우에 현재 버튼 역할의 축약 텍스트를 표시한다.                  */
    /* ---------------------------------------------------------------------- */
    u8g2_DrawFrame(u8g2, 0, 118, 240, 10);
    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);

    if (left != 0)   u8g2_DrawStr(u8g2, 2, 126, left);
    if (center != 0) u8g2_DrawStr(u8g2, 102, 126, center);
    if (right != 0)  u8g2_DrawStr(u8g2, 200, 126, right);
}

void Motor_UI_DrawToast(u8g2_t *u8g2, const motor_state_t *state)
{
    uint8_t w;

    if ((u8g2 == 0) || (state == 0))
    {
        return;
    }

    if ((state->ui.toast_until_ms == 0u) || (state->ui.toast_until_ms < state->now_ms))
    {
        return;
    }

    w = (uint8_t)(strlen(state->ui.toast_text) * 6u + 8u);
    if (w > 120u)
    {
        w = 120u;
    }

    /* ---------------------------------------------------------------------- */
    /*  toast 영역                                                              */
    /*  - 우하단에 작은 팝업 박스를 띄워 "REC START", "LOCK" 등을 짧게 표시    */
    /* ---------------------------------------------------------------------- */
    u8g2_DrawBox(u8g2, (uint8_t)(236u - w), 104, w, 12);
    u8g2_SetDrawColor(u8g2, 0);
    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(u8g2, (uint8_t)(240u - w + 4u), 113, state->ui.toast_text);
    u8g2_SetDrawColor(u8g2, 1);
}

void Motor_UI_DrawLabeledValueBox(u8g2_t *u8g2,
                                  uint8_t x,
                                  uint8_t y,
                                  uint8_t w,
                                  uint8_t h,
                                  const char *label,
                                  const char *value,
                                  uint8_t large_font)
{
    if (u8g2 == 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  공용 value box                                                          */
    /*  - 바깥 frame 1개                                                         */
    /*  - 상단 작은 label                                                       */
    /*  - 중앙의 큰 숫자 또는 텍스트                                             */
    /* ---------------------------------------------------------------------- */
    u8g2_DrawFrame(u8g2, x, y, w, h);
    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    if (label != 0)
    {
        u8g2_DrawStr(u8g2, x + 2u, y + 7u, label);
    }

    if (large_font != 0u)
    {
        u8g2_SetFont(u8g2, u8g2_font_9x15B_tr);
        if (value != 0)
        {
            u8g2_DrawStr(u8g2, x + 3u, y + h - 4u, value);
        }
    }
    else
    {
        u8g2_SetFont(u8g2, u8g2_font_6x10_tr);
        if (value != 0)
        {
            u8g2_DrawStr(u8g2, x + 2u, y + h - 3u, value);
        }
    }
}

void Motor_UI_DrawHorizontalBar(u8g2_t *u8g2,
                                uint8_t x,
                                uint8_t y,
                                uint8_t w,
                                uint8_t h,
                                int32_t value,
                                int32_t min_value,
                                int32_t max_value,
                                const char *caption)
{
    uint32_t fill_w;
    int32_t span;
    int32_t clamped;

    if (u8g2 == 0)
    {
        return;
    }

    span = max_value - min_value;
    if (span <= 0)
    {
        span = 1;
    }

    clamped = value;
    if (clamped < min_value) clamped = min_value;
    if (clamped > max_value) clamped = max_value;
    fill_w = (uint32_t)(((int64_t)(clamped - min_value) * (int64_t)(w - 2u)) / (int64_t)span);

    u8g2_DrawFrame(u8g2, x, y, w, h);
    u8g2_DrawBox(u8g2, x + 1u, y + 1u, (uint8_t)fill_w, (uint8_t)(h - 2u));
    if (caption != 0)
    {
        u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
        u8g2_DrawStr(u8g2, x + 2u, y - 2u, caption);
    }
}

void Motor_UI_DrawCenteredTextBlock(u8g2_t *u8g2, uint8_t y, const char *title, const char *line1, const char *line2)
{
    if (u8g2 == 0)
    {
        return;
    }

    u8g2_SetFont(u8g2, u8g2_font_9x15B_tr);
    if (title != 0) u8g2_DrawStr(u8g2, 50, y, title);
    u8g2_SetFont(u8g2, u8g2_font_6x10_tr);
    if (line1 != 0) u8g2_DrawStr(u8g2, 24, (uint8_t)(y + 18u), line1);
    if (line2 != 0) u8g2_DrawStr(u8g2, 24, (uint8_t)(y + 32u), line2);
}
