#include "Vario_QuickSet.h"

#include "Vario_Display_Common.h"
#include "Vario_Settings.h"
#include "Vario_State.h"

#include <stdio.h>

void Vario_QuickSet_Render(u8g2_t *u8g2, const vario_buttonbar_t *buttonbar)
{
    const vario_viewport_t *v;
    const vario_settings_t *settings;
    char                    qnh_text[24];
    char                    alt_unit_text[16];
    char                    vs_text[16];
    char                    audio_text[16];
    char                    vol_text[16];

    (void)buttonbar;

    v        = Vario_Display_GetContentViewport();
    settings = Vario_Settings_Get();

    snprintf(qnh_text,
             sizeof(qnh_text),
             "%ld.%ld hPa",
             (long)Vario_Settings_GetQnhDisplayWhole(),
             (long)Vario_Settings_GetQnhDisplayFrac1());
    snprintf(alt_unit_text, sizeof(alt_unit_text), "%s", Vario_Settings_GetAltitudeUnitText());
    snprintf(vs_text, sizeof(vs_text), "%s", Vario_Settings_GetVSpeedUnitText());
    snprintf(audio_text, sizeof(audio_text), "%s", Vario_Settings_GetAudioOnOffText());
    snprintf(vol_text, sizeof(vol_text), "%u%%", (unsigned)settings->audio_volume_percent);

    Vario_Display_DrawPageTitle(u8g2, v, "VARIO QUICKSET", "PRE-FLIGHT");

    Vario_Display_DrawMenuRow(u8g2,
                              v,
                              (int16_t)(v->y + 22),
                              (Vario_State_GetQuickSetCursor() == VARIO_QUICKSET_ITEM_QNH),
                              "QNH",
                              qnh_text);

    Vario_Display_DrawMenuRow(u8g2,
                              v,
                              (int16_t)(v->y + 36),
                              (Vario_State_GetQuickSetCursor() == VARIO_QUICKSET_ITEM_ALT_UNIT),
                              "AltUnit",
                              alt_unit_text);

    Vario_Display_DrawMenuRow(u8g2,
                              v,
                              (int16_t)(v->y + 50),
                              (Vario_State_GetQuickSetCursor() == VARIO_QUICKSET_ITEM_VSPEED_UNIT),
                              "VSpeed",
                              vs_text);

    Vario_Display_DrawMenuRow(u8g2,
                              v,
                              (int16_t)(v->y + 64),
                              (Vario_State_GetQuickSetCursor() == VARIO_QUICKSET_ITEM_AUDIO_ENABLE),
                              "Audio",
                              audio_text);

    Vario_Display_DrawMenuRow(u8g2,
                              v,
                              (int16_t)(v->y + 78),
                              (Vario_State_GetQuickSetCursor() == VARIO_QUICKSET_ITEM_AUDIO_VOLUME),
                              "Volume",
                              vol_text);

    Vario_Display_DrawRawOverlay(u8g2, v);
}
