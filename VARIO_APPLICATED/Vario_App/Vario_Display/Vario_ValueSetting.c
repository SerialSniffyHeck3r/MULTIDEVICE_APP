#include "Vario_ValueSetting.h"

#include "Vario_Display_Common.h"
#include "Vario_Settings.h"
#include "Vario_State.h"

#include <stdio.h>

static void vario_valuesetting_format_alt_cm(char *buf, size_t buf_len, int32_t altitude_cm)
{
    float altitude_m;
    int32_t value;

    altitude_m = ((float)altitude_cm) * 0.01f;
    value      = Vario_Settings_AltitudeMetersToDisplayRounded(altitude_m);

    snprintf(buf, buf_len, "%ld %s", (long)value, Vario_Settings_GetAltitudeUnitText());
}

void Vario_ValueSetting_Render(u8g2_t *u8g2, const vario_buttonbar_t *buttonbar)
{
    const vario_viewport_t *v;
    const vario_settings_t *settings;
    char                    qnh_text[24];
    char                    alt1_text[24];
    char                    alt2_text[24];
    char                    alt3_text[24];
    char                    unit_text[16];
    char                    vs_text[16];

    (void)buttonbar;

    v        = Vario_Display_GetContentViewport();
    settings = Vario_Settings_Get();

    snprintf(qnh_text,
             sizeof(qnh_text),
             "%ld.%ld hPa",
             (long)Vario_Settings_GetQnhDisplayWhole(),
             (long)Vario_Settings_GetQnhDisplayFrac1());
    vario_valuesetting_format_alt_cm(alt1_text, sizeof(alt1_text), settings->alt1_cm);
    vario_valuesetting_format_alt_cm(alt2_text, sizeof(alt2_text), settings->alt2_cm);
    vario_valuesetting_format_alt_cm(alt3_text, sizeof(alt3_text), settings->alt3_cm);
    snprintf(unit_text, sizeof(unit_text), "%s", Vario_Settings_GetAltitudeUnitText());
    snprintf(vs_text, sizeof(vs_text), "%s", Vario_Settings_GetVSpeedUnitText());

    Vario_Display_DrawPageTitle(u8g2, v, "VARIO VALUE SET", "PRECISION");

    Vario_Display_DrawMenuRow(u8g2,
                              v,
                              (int16_t)(v->y + 20),
                              (Vario_State_GetValueSettingCursor() == VARIO_VALUE_ITEM_QNH),
                              "QNH",
                              qnh_text);

    Vario_Display_DrawMenuRow(u8g2,
                              v,
                              (int16_t)(v->y + 33),
                              (Vario_State_GetValueSettingCursor() == VARIO_VALUE_ITEM_ALT1),
                              "ALT1",
                              alt1_text);

    Vario_Display_DrawMenuRow(u8g2,
                              v,
                              (int16_t)(v->y + 46),
                              (Vario_State_GetValueSettingCursor() == VARIO_VALUE_ITEM_ALT2),
                              "ALT2",
                              alt2_text);

    Vario_Display_DrawMenuRow(u8g2,
                              v,
                              (int16_t)(v->y + 59),
                              (Vario_State_GetValueSettingCursor() == VARIO_VALUE_ITEM_ALT3),
                              "ALT3",
                              alt3_text);

    Vario_Display_DrawMenuRow(u8g2,
                              v,
                              (int16_t)(v->y + 72),
                              (Vario_State_GetValueSettingCursor() == VARIO_VALUE_ITEM_ALT_UNIT),
                              "ALT UNIT",
                              unit_text);

    Vario_Display_DrawMenuRow(u8g2,
                              v,
                              (int16_t)(v->y + 85),
                              (Vario_State_GetValueSettingCursor() == VARIO_VALUE_ITEM_VSPEED_UNIT),
                              "VS UNIT",
                              vs_text);

    Vario_Display_DrawRawOverlay(u8g2, v);
}
