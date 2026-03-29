#include "Motor_UI_Internal.h"

#include "Motor_Units.h"

#include <stdio.h>

void Motor_UI_DrawScreen_Vehicle(u8g2_t *u8g2, const ui_rect_t *viewport, const motor_state_t *state)
{
    char buf[24];
    char ride_text[20];
    char move_text[20];
    char service_text[20];
    int32_t coolant_value;

    if ((u8g2 == 0) || (viewport == 0) || (state == 0))
    {
        return;
    }

    Motor_UI_DrawViewportTitle(u8g2, viewport, "VEHICLE / TRIP");

    /* ---------------------------------------------------------------------- */
    /*  1행 상자 3개                                                           */
    /*  - OBD link / RPM / Battery                                             */
    /* ---------------------------------------------------------------------- */
    Motor_UI_DrawLabeledValueBox(u8g2,
                                 viewport->x + 4,
                                 viewport->y + 16,
                                 74,
                                 22,
                                 "OBD",
                                 (state->vehicle.connected != false) ? "LINK" : "OFF",
                                 0u);

    if (state->vehicle.rpm_valid != false)
    {
        (void)snprintf(buf, sizeof(buf), "%u", (unsigned)state->vehicle.rpm);
    }
    else
    {
        (void)snprintf(buf, sizeof(buf), "--");
    }
    Motor_UI_DrawLabeledValueBox(u8g2, viewport->x + 82, viewport->y + 16, 74, 22, "RPM", buf, 0u);

    if (state->vehicle.battery_valid != false)
    {
        (void)snprintf(buf, sizeof(buf), "%u.%01uV",
                       (unsigned)(state->vehicle.battery_mv / 1000u),
                       (unsigned)((state->vehicle.battery_mv % 1000u) / 100u));
    }
    else
    {
        (void)snprintf(buf, sizeof(buf), "--");
    }
    Motor_UI_DrawLabeledValueBox(u8g2, viewport->x + 160, viewport->y + 16, 76, 22, "BATT", buf, 0u);

    /* ---------------------------------------------------------------------- */
    /*  냉각수 온도 바                                                         */
    /*  - 가운데 넓은 바를 사용해 현재 온도와 경고 임계를 glanceable 하게 표시 */
    /* ---------------------------------------------------------------------- */
    coolant_value = (state->vehicle.coolant_valid != false) ? state->vehicle.coolant_temp_c_x10 : 0;
    Motor_UI_DrawHorizontalBar(u8g2,
                               viewport->x + 10,
                               viewport->y + 52,
                               viewport->w - 20,
                               12,
                               coolant_value,
                               400,
                               1200,
                               "COOLANT");

    if (state->vehicle.coolant_valid != false)
    {
        Motor_Units_FormatTemperature(buf, sizeof(buf), state->vehicle.coolant_temp_c_x10, &state->settings.units);
        u8g2_SetFont(u8g2, u8g2_font_6x10_tr);
        u8g2_DrawStr(u8g2, viewport->x + 176, viewport->y + 49, buf);
    }

    /* ---------------------------------------------------------------------- */
    /*  하단 trip / service summary                                            */
    /* ---------------------------------------------------------------------- */
    (void)snprintf(ride_text, sizeof(ride_text), "%02lu:%02lu",
                   (unsigned long)(state->session.ride_seconds / 3600u),
                   (unsigned long)((state->session.ride_seconds % 3600u) / 60u));
    (void)snprintf(move_text, sizeof(move_text), "%02lu:%02lu",
                   (unsigned long)(state->session.moving_seconds / 3600u),
                   (unsigned long)((state->session.moving_seconds % 3600u) / 60u));
    (void)snprintf(service_text, sizeof(service_text), "%u DUE", (unsigned)state->maintenance.due_count);

    Motor_UI_DrawLabeledValueBox(u8g2, viewport->x + 4,   viewport->y + 74, 74, 26, "RIDE",    ride_text,    0u);
    Motor_UI_DrawLabeledValueBox(u8g2, viewport->x + 82,  viewport->y + 74, 74, 26, "MOVING",  move_text,    0u);
    Motor_UI_DrawLabeledValueBox(u8g2, viewport->x + 160, viewport->y + 74, 76, 26, "SERVICE", service_text, 0u);

    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    if (state->maintenance.next_due_index < MOTOR_SERVICE_ITEM_COUNT)
    {
        (void)snprintf(buf,
                       sizeof(buf),
                       "NEXT %s",
                       state->settings.maintenance.items[state->maintenance.next_due_index].label);
    }
    else
    {
        (void)snprintf(buf, sizeof(buf), "NEXT SERVICE OK");
    }
    u8g2_DrawStr(u8g2, viewport->x + 6, viewport->y + viewport->h - 4, buf);
}
