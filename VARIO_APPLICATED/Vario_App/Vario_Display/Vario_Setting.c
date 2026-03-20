#include "Vario_Setting.h"

#include "Vario_Display_Common.h"
#include "Vario_Dev.h"
#include "Vario_Settings.h"
#include "Vario_State.h"

#include <stdio.h>

void Vario_Setting_Render(u8g2_t *u8g2, const vario_buttonbar_t *buttonbar)
{
    const vario_viewport_t     *v;
    const vario_settings_t     *settings;
    const vario_dev_settings_t *dev;
    char                        audio_text[24];
    char                        debug_text[24];

    (void)buttonbar;

    v        = Vario_Display_GetContentViewport();
    settings = Vario_Settings_Get();
    dev      = Vario_Dev_Get();

    snprintf(audio_text,
             sizeof(audio_text),
             "%s %u%%",
             Vario_Settings_GetAudioOnOffText(),
             (unsigned)settings->audio_volume_percent);

    snprintf(debug_text,
             sizeof(debug_text),
             "%s",
             (dev->raw_overlay_enabled != 0u) ? "RAW ON" : "RAW OFF");

    /* ---------------------------------------------------------------------- */
    /*  이 화면은 status bar + bottom bar 를 실제로 사용하는                    */
    /*  content viewport 계열 화면이다.                                        */
    /* ---------------------------------------------------------------------- */
    Vario_Display_DrawPageTitle(u8g2, v, "VARIO SETTINGS", "MENU ROOT");

    /* QuickSet 진입 행 */
    Vario_Display_DrawMenuRow(u8g2,
                              v,
                              (int16_t)(v->y + 24),
                              (Vario_State_GetSettingsCursor() == VARIO_SETTING_MENU_QUICKSET),
                              "QuickSet",
                              "open");

    /* ValueSetting 진입 행 */
    Vario_Display_DrawMenuRow(u8g2,
                              v,
                              (int16_t)(v->y + 38),
                              (Vario_State_GetSettingsCursor() == VARIO_SETTING_MENU_VALUESETTING),
                              "ValueSet",
                              "open");

    /* Audio 토글/상태 행 */
    Vario_Display_DrawMenuRow(u8g2,
                              v,
                              (int16_t)(v->y + 52),
                              (Vario_State_GetSettingsCursor() == VARIO_SETTING_MENU_AUDIO),
                              "Audio",
                              audio_text);

    /* Debug overlay 토글 행 */
    Vario_Display_DrawMenuRow(u8g2,
                              v,
                              (int16_t)(v->y + 66),
                              (Vario_State_GetSettingsCursor() == VARIO_SETTING_MENU_DEBUG),
                              "Debug",
                              debug_text);

    Vario_Display_DrawRawOverlay(u8g2, v);
}
