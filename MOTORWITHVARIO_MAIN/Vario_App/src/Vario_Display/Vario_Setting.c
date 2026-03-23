#include "Vario_Setting.h"

#include "Vario_Display_Common.h"
#include "Vario_Settings.h"
#include "Vario_State.h"

#include <stdio.h>

void Vario_Setting_Render(u8g2_t *u8g2, const vario_buttonbar_t *buttonbar)
{
    const vario_viewport_t *v;
    const vario_settings_t *settings;
    char                    brightness_text[24];
    char                    volume_text[24];
    char                    damping_text[24];
    char                    qnh_text[32];
    char                    line2[48];
    int32_t                 alt2_zero_disp;

    (void)buttonbar;

    v        = Vario_Display_GetContentViewport();
    settings = Vario_Settings_Get();

    if ((u8g2 == NULL) || (v == NULL) || (settings == NULL))
    {
        return;
    }

    snprintf(brightness_text,
             sizeof(brightness_text),
             "%u%%",
             (unsigned)settings->display_brightness_percent);

    snprintf(volume_text,
             sizeof(volume_text),
             "%s %u%%",
             Vario_Settings_GetAudioOnOffText(),
             (unsigned)settings->audio_volume_percent);

    snprintf(damping_text,
             sizeof(damping_text),
             "%u/10",
             (unsigned)settings->vario_damping_level);

    Vario_Settings_FormatQnhText(qnh_text, sizeof(qnh_text));

    alt2_zero_disp = Vario_Settings_AltitudeMetersToDisplayRoundedWithUnit(((float)settings->alt2_reference_cm) * 0.01f,
                                                                           settings->alt2_unit);

    if (settings->alt2_mode == VARIO_ALT2_MODE_RELATIVE)
    {
        snprintf(line2,
                 sizeof(line2),
                 "ALT2 ZERO %ld %s  |  BEEP %s",
                 (long)alt2_zero_disp,
                 Vario_Settings_GetAltitudeUnitTextForUnit(settings->alt2_unit),
                 Vario_Settings_GetBeepModeText());
    }
    else
    {
        snprintf(line2,
                 sizeof(line2),
                 "ALT2 %s  |  TIME %s  |  BEEP %s",
                 Vario_Settings_GetAlt2ModeText(),
                 Vario_Settings_GetTimeFormatText(),
                 Vario_Settings_GetBeepModeText());
    }

    Vario_Display_DrawPageTitle(u8g2, v, "SETUP", "LIVE CONTROL");

    Vario_Display_DrawBarRow(u8g2,
                             v,
                             (int16_t)(v->y + 20),
                             (Vario_State_GetSettingsCursor() == VARIO_SETTING_MENU_BRIGHTNESS),
                             "Brightness",
                             brightness_text,
                             settings->display_brightness_percent);

    Vario_Display_DrawBarRow(u8g2,
                             v,
                             (int16_t)(v->y + 40),
                             (Vario_State_GetSettingsCursor() == VARIO_SETTING_MENU_VOLUME),
                             "Volume",
                             volume_text,
                             settings->audio_volume_percent);

    Vario_Display_DrawBarRow(u8g2,
                             v,
                             (int16_t)(v->y + 60),
                             (Vario_State_GetSettingsCursor() == VARIO_SETTING_MENU_DAMPING),
                             "Damping",
                             damping_text,
                             (uint8_t)(settings->vario_damping_level * 10u));

    Vario_Display_DrawMenuRow(u8g2,
                              v,
                              (int16_t)(v->y + 90),
                              (Vario_State_GetSettingsCursor() == VARIO_SETTING_MENU_ALT2),
                              "ALT2 Mode",
                              Vario_Settings_GetAlt2ModeText());

    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    u8g2_DrawStr(u8g2,
                 (uint8_t)(v->x + 8),
                 (uint8_t)(v->y + v->h - 15),
                 qnh_text);
    u8g2_DrawStr(u8g2,
                 (uint8_t)(v->x + 8),
                 (uint8_t)(v->y + v->h - 7),
                 line2);
}
