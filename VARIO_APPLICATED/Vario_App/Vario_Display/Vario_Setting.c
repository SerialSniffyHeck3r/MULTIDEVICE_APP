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
    /*  Root menu                                                              */
    /*                                                                            */
    /*  상태머신 mode 는 그대로 두고,                                           */
    /*  사용자가 요구한 의미에 맞게 item label 만 새로 정리했다.                */
    /* ---------------------------------------------------------------------- */
    Vario_Display_DrawPageTitle(u8g2, v, "VARIO SETTINGS", "ROOT");

    Vario_Display_DrawMenuRow(u8g2,
                              v,
                              (int16_t)(v->y + 24),
                              (Vario_State_GetSettingsCursor() == VARIO_SETTING_MENU_QUICKSET),
                              "Flight/Audio",
                              "open");

    Vario_Display_DrawMenuRow(u8g2,
                              v,
                              (int16_t)(v->y + 38),
                              (Vario_State_GetSettingsCursor() == VARIO_SETTING_MENU_VALUESETTING),
                              "Graphics",
                              "open");

    Vario_Display_DrawMenuRow(u8g2,
                              v,
                              (int16_t)(v->y + 52),
                              (Vario_State_GetSettingsCursor() == VARIO_SETTING_MENU_AUDIO),
                              "Audio Quick",
                              audio_text);

    Vario_Display_DrawMenuRow(u8g2,
                              v,
                              (int16_t)(v->y + 66),
                              (Vario_State_GetSettingsCursor() == VARIO_SETTING_MENU_DEBUG),
                              "Debug",
                              debug_text);

    /* ---------------------------------------------------------------------- */
    /*  하단 설명                                                              */
    /*  - 기존 버튼 구조를 그대로 유지하므로,                                   */
    /*    이 화면은 "어느 세부 페이지로 들어갈지" 를 선택하는 허브 역할이다.    */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFont(u8g2, u8g2_font_4x6_tf);
    u8g2_DrawStr(u8g2,
                 (uint8_t)(v->x + 4),
                 (uint8_t)(v->y + v->h - 10),
                 "Flight=QNH/unit/source/audio  Graphics=layout/range/trail");

    Vario_Display_DrawRawOverlay(u8g2, v);
}
