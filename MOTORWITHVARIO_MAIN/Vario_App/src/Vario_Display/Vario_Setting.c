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

    (void)buttonbar;

    v        = Vario_Display_GetContentViewport();
    settings = Vario_Settings_Get();

    if ((u8g2 == NULL) || (v == NULL) || (settings == NULL))
    {
        return;
    }

    switch (settings->display_backlight_mode)
    {
        case VARIO_BACKLIGHT_MODE_AUTO_DAY_NIGHT:
            snprintf(brightness_text, sizeof(brightness_text), "AUTO D/N");
            break;

        case VARIO_BACKLIGHT_MODE_MANUAL:
            snprintf(brightness_text,
                     sizeof(brightness_text),
                     "MAN %u%%",
                     (unsigned)settings->display_brightness_percent);
            break;

        case VARIO_BACKLIGHT_MODE_AUTO_CONTINUOUS:
        case VARIO_BACKLIGHT_MODE_COUNT:
        default:
            snprintf(brightness_text, sizeof(brightness_text), "AUTO CONT");
            break;
    }

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

    Vario_Display_DrawPageTitle(u8g2, v, "SETUP", "");

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
                 (uint8_t)(v->y + v->h - 7),
                 qnh_text);
}
