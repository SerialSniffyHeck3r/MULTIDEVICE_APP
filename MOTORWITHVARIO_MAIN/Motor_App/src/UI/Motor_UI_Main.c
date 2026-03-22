
#include "Motor_UI_Internal.h"

#include "Motor_Units.h"

#include <stdio.h>
#include <stdlib.h>

void Motor_UI_DrawScreen_Main(u8g2_t *u8g2, const motor_state_t *state)
{
    char speed_text[16];
    char bank_text[16];
    char latg_text[16];
    char long_text[16];
    char alt_text[16];
    char grade_text[16];

    if ((u8g2 == 0) || (state == 0))
    {
        return;
    }

    Motor_UI_DrawStatusBar(u8g2, state);

    /* ---------------------------------------------------------------------- */
    /*  메인 화면 레이아웃                                                      */
    /*  - 상단 상태바 아래에 2x2 대형 값 박스                                   */
    /*  - 하단에 altitude / grade / trip mini boxes                            */
    /*  - "모든 값이 다 나오는 메인 화면" 요구에 맞춰 핵심 값들을 한 장에 압축   */
    /* ---------------------------------------------------------------------- */
    Motor_Units_FormatSpeed(speed_text, sizeof(speed_text), state->nav.speed_kmh_x10, &state->settings.units);
    (void)snprintf(bank_text, sizeof(bank_text), "%+d.%01d", (int)(state->dyn.bank_deg_x10 / 10), (int)abs(state->dyn.bank_deg_x10 % 10));
    (void)snprintf(latg_text, sizeof(latg_text), "%+ld.%01ld", (long)(state->dyn.lat_accel_mg / 1000), (long)abs((int)(state->dyn.lat_accel_mg % 1000) / 100));
    (void)snprintf(long_text, sizeof(long_text), "%+ld.%01ld", (long)(state->dyn.lon_accel_mg / 1000), (long)abs((int)(state->dyn.lon_accel_mg % 1000) / 100));
    Motor_Units_FormatAltitude(alt_text, sizeof(alt_text), state->nav.altitude_cm, &state->settings.units);
    (void)snprintf(grade_text, sizeof(grade_text), "%+ld.%01ld", (long)(state->snapshot.altitude.grade_noimu_x10 / 10), (long)abs((int)(state->snapshot.altitude.grade_noimu_x10 % 10)));

    Motor_UI_DrawLabeledValueBox(u8g2, 4, 14, 114, 44, "SPEED", speed_text, 1u);
    Motor_UI_DrawLabeledValueBox(u8g2, 122, 14, 114, 44, "BANK", bank_text, 1u);
    Motor_UI_DrawLabeledValueBox(u8g2, 4, 62, 114, 28, "LAT G", latg_text, 0u);
    Motor_UI_DrawLabeledValueBox(u8g2, 122, 62, 114, 28, "ACC/BRK", long_text, 0u);

    Motor_UI_DrawLabeledValueBox(u8g2, 4, 92, 74, 22, "ALT", alt_text, 0u);
    Motor_UI_DrawLabeledValueBox(u8g2, 82, 92, 74, 22, "GRADE", grade_text, 0u);
    Motor_UI_DrawLabeledValueBox(u8g2, 160, 92, 76, 22,
                                 (state->vehicle.connected != false) ? "RPM" : "TRIP",
                                 (state->vehicle.connected != false) ? "OBD" : "LIVE",
                                 0u);

    Motor_UI_DrawBottomHint(u8g2, "1<", "5 REC", "6 MENU");
}
