
#include "Motor_UI_Internal.h"

#include <stdio.h>

void Motor_UI_DrawScreen_Vehicle(u8g2_t *u8g2, const motor_state_t *state)
{
    char ride_text[20];
    char move_text[20];
    char due_text[20];

    if ((u8g2 == 0) || (state == 0))
    {
        return;
    }

    Motor_UI_DrawStatusBar(u8g2, state);

    /* ---------------------------------------------------------------------- */
    /*  차량 요약 / trip computer                                               */
    /*  - 상단: coolant bar, battery, OBD link                                  */
    /*  - 하단: maintenance due, ride time, moving time                         */
    /* ---------------------------------------------------------------------- */
    Motor_UI_DrawLabeledValueBox(u8g2, 4, 14, 74, 24, "OBD", (state->vehicle.connected != false) ? "LINK" : "OFF", 0u);
    Motor_UI_DrawLabeledValueBox(u8g2, 82, 14, 74, 24, "RPM",
                                 (state->vehicle.rpm_valid != false) ? "LIVE" : "--", 0u);
    Motor_UI_DrawLabeledValueBox(u8g2, 160, 14, 76, 24, "BATT",
                                 (state->vehicle.battery_valid != false) ? "OK" : "--", 0u);

    Motor_UI_DrawHorizontalBar(u8g2,
                               10,
                               52,
                               220,
                               12,
                               state->vehicle.coolant_valid ? state->vehicle.coolant_temp_c_x10 : 0,
                               500,
                               1200,
                               "COOLANT TEMP");

    (void)snprintf(ride_text, sizeof(ride_text), "%lu s", (unsigned long)state->session.ride_seconds);
    (void)snprintf(move_text, sizeof(move_text), "%lu s", (unsigned long)state->session.moving_seconds);
    (void)snprintf(due_text, sizeof(due_text), "%u ITEMS", (unsigned)state->maintenance.due_count);

    Motor_UI_DrawLabeledValueBox(u8g2, 4, 74, 74, 26, "RIDE", ride_text, 0u);
    Motor_UI_DrawLabeledValueBox(u8g2, 82, 74, 74, 26, "MOVING", move_text, 0u);
    Motor_UI_DrawLabeledValueBox(u8g2, 160, 74, 76, 26, "SERVICE", due_text, 0u);

    Motor_UI_DrawBottomHint(u8g2, "OBD", "SERVICE", "6 MENU");
}
