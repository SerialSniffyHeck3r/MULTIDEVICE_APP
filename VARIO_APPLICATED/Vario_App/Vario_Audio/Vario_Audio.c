#include "Vario_Audio.h"

#include "APP_ALTITUDE.h"
#include "Vario_Settings.h"
#include "Vario_State.h"

#include <stdbool.h>

void Vario_Audio_Init(void)
{
    /* ---------------------------------------------------------------------- */
    /*  현재 단계의 바리오 오디오 정책                                          */
    /*                                                                        */
    /*  - 기존 APP_ALTITUDE 쪽에 이미 연결되어 있는                              */
    /*    debug vario tone 경로를 재사용한다.                                  */
    /*  - 바리오 메인 화면(1/2/3) + audio enabled 조건일 때만                   */
    /*    UI active 를 true 로 유지한다.                                       */
    /*  - 설정/QuickSet/ValueSetting 화면에서는                                 */
    /*    반드시 false 로 내려 tone ownership 을 해제한다.                     */
    /*                                                                        */
    /*  이렇게 하면 앱 레이어가 DAC/TIM 같은 저수준 오디오 transport 를         */
    /*  직접 건드리지 않으면서도, 기존 오디오 엔진을 그대로 활용할 수 있다.      */
    /* ---------------------------------------------------------------------- */
    APP_ALTITUDE_DebugSetUiActive(false, 0u);
}

void Vario_Audio_Task(uint32_t now_ms)
{
    const vario_settings_t *settings;
    vario_mode_t            mode;
    bool                    audio_active;

    settings = Vario_Settings_Get();
    mode     = Vario_State_GetMode();

    audio_active = false;

    if ((settings->audio_enabled != 0u) &&
        ((mode == VARIO_MODE_SCREEN_1) ||
         (mode == VARIO_MODE_SCREEN_2) ||
         (mode == VARIO_MODE_SCREEN_3)))
    {
        audio_active = true;
    }

    APP_ALTITUDE_DebugSetUiActive(audio_active, now_ms);
}
