#include "Vario_Audio.h"

#include "APP_ALTITUDE.h"
#include "Audio_Driver.h"
#include "Vario_Settings.h"
#include "Vario_State.h"

#include <stdbool.h>

/* -------------------------------------------------------------------------- */
/* 마지막으로 실제 드라이버에 적용한 볼륨 캐시                                  */
/*                                                                            */
/* 이유                                                                          */
/* - 현재 Audio_Driver_SetVolumePercent() 는 꽤 가벼운 함수지만,                 */
/*   설정 값이 바뀌지 않았는데 매 loop 마다 다시 넣을 필요는 없다.             */
/* - 반대로 "초기 부팅 시 3%로만 고정되고 VARIO 설정 75%가 실제 드라이버에       */
/*   반영되지 않는" 문제는 반드시 막아야 한다.                                  */
/* -------------------------------------------------------------------------- */
static uint8_t s_vario_audio_last_applied_volume = 0xFFu;

/* -------------------------------------------------------------------------- */
/* 내부 helper: 0~100 범위 clamp                                               */
/* -------------------------------------------------------------------------- */
static uint8_t vario_audio_clamp_percent(uint8_t percent)
{
    if (percent > 100u)
    {
        return 100u;
    }

    return percent;
}

/* -------------------------------------------------------------------------- */
/* 내부 helper: 지금 mode 가 variometer tone ownership 을 가져야 하는가?        */
/*                                                                            */
/* 정책                                                                          */
/* - SCREEN_1/2/3 : 비행 중 메인 화면. tone ownership = true                   */
/* - SETTING 계열   : 설정 편집 화면. tone ownership = false                    */
/* -------------------------------------------------------------------------- */
static bool vario_audio_mode_owns_tone(vario_mode_t mode)
{
    switch (mode)
    {
    case VARIO_MODE_SCREEN_1:
    case VARIO_MODE_SCREEN_2:
    case VARIO_MODE_SCREEN_3:
        return true;

    case VARIO_MODE_SETTING:
    case VARIO_MODE_QUICKSET:
    case VARIO_MODE_VALUESETTING:
    case VARIO_MODE_COUNT:
    default:
        return false;
    }
}

void Vario_Audio_Init(void)
{
    const vario_settings_t *settings;
    uint8_t volume_percent;

    settings = Vario_Settings_Get();
    volume_percent = vario_audio_clamp_percent(settings->audio_volume_percent);

    /* ---------------------------------------------------------------------- */
    /* 부팅 직후 실제 오디오 드라이버 볼륨을 VARIO 설정값으로 즉시 동기화한다.   */
    /*                                                                          */
    /* 이 줄이 중요한 이유                                                       */
    /* - main.c 는 Audio_Driver_SetVolumePercent(3u) 로 매우 낮은 시작 볼륨을    */
    /*   걸어 둔다.                                                              */
    /* - 기존 VARIO 코드는 settings.audio_volume_percent(기본 75%) 를            */
    /*   실제 드라이버에 다시 써 주지 않아서, 사용자는 "소리가 안 난다" 고        */
    /*   느끼기 쉬웠다.                                                          */
    /* - 따라서 VARIO init 시점에 한 번, 이후 설정 변경 시마다 다시              */
    /*   Audio_Driver_SetVolumePercent() 를 호출해야 한다.                       */
    /* ---------------------------------------------------------------------- */
    Audio_Driver_SetVolumePercent(volume_percent);
    s_vario_audio_last_applied_volume = volume_percent;

    /* ---------------------------------------------------------------------- */
    /* tone ownership 은 처음에는 끄고 시작한다.                                */
    /* 실제 main screen 진입 후 Task() 에서 mode / enabled 조건을 보고           */
    /* true 로 올린다.                                                           */
    /* ---------------------------------------------------------------------- */
    APP_ALTITUDE_DebugSetUiActive(false, 0u);
}

void Vario_Audio_Task(uint32_t now_ms)
{
    const vario_settings_t *settings;
    vario_mode_t mode;
    uint8_t volume_percent;
    bool audio_active;

    settings = Vario_Settings_Get();
    mode = Vario_State_GetMode();

    /* ---------------------------------------------------------------------- */
    /* 실제 오디오 드라이버 볼륨 동기화                                          */
    /* ---------------------------------------------------------------------- */
    volume_percent = vario_audio_clamp_percent(settings->audio_volume_percent);
    if (volume_percent != s_vario_audio_last_applied_volume)
    {
        Audio_Driver_SetVolumePercent(volume_percent);
        s_vario_audio_last_applied_volume = volume_percent;
    }

    /* ---------------------------------------------------------------------- */
    /* variometer tone ownership 결정                                            */
    /*                                                                          */
    /* 조건                                                                        */
    /* - 사용자 설정에서 audio enabled 이어야 한다.                              */
    /* - 현재 mode 가 메인 비행 화면(1/2/3) 이어야 한다.                         */
    /*                                                                          */
    /* 이 ownership bit 는 APP_ALTITUDE 쪽 기존 debug-audio 경로를 재사용한다.   */
    /* 즉, VARIO 앱은 DAC/TIM transport 를 직접 건드리지 않고,                   */
    /* 아래 드라이버 API 만 이용한다.                                            */
    /* ---------------------------------------------------------------------- */
    audio_active = ((settings->audio_enabled != 0u) &&
                    (vario_audio_mode_owns_tone(mode) != false));

    APP_ALTITUDE_DebugSetUiActive(audio_active, now_ms);
}
