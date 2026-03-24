#include "Vario_Audio.h"

#include "Audio_App.h"
#include "Vario_Settings.h"
#include "Vario_State.h"

#include <stdbool.h>
#include <stddef.h>

/* -------------------------------------------------------------------------- */
/* 마지막으로 실제 드라이버에 적용한 볼륨 캐시                                  */
/* -------------------------------------------------------------------------- */
static uint8_t s_vario_audio_last_applied_volume = 0xFFu;

static uint8_t vario_audio_clamp_percent(uint8_t percent)
{
    if (percent > 100u)
    {
        return 100u;
    }

    return percent;
}

static int32_t vario_audio_round_mps_to_cms(float value_mps)
{
    value_mps *= 100.0f;

    if (value_mps >= 0.0f)
    {
        return (int32_t)(value_mps + 0.5f);
    }

    return (int32_t)(value_mps - 0.5f);
}

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

/* -------------------------------------------------------------------------- */
/*  app-layer tone gate                                                       */
/*                                                                            */
/*  APP_ALTITUDE debug audio 경로를 그대로 transport 로 재사용하되,            */
/*  실제 tone on/off 판정과 tone source는                                     */
/*  상위 VARIO 앱의 10Hz fast presentation vario를 사용한다.                  */
/*                                                                            */
/*  결과                                                                      */
/*  - climb threshold 이상일 때만 상승음 허용                                 */
/*  - sink threshold 이하일 때만 하강음 허용                                  */
/*  - neutral deadband 에서는 아예 audio path 를 끈다                          */
/*  - 바/사운드가 같은 10Hz 고급 필터 출력을 공유하므로                        */
/*    화면 bar와 비프의 체감 반응이 어긋나지 않는다.                           */
/* -------------------------------------------------------------------------- */
static bool vario_audio_is_outside_deadband(const vario_runtime_t *rt,
                                            const vario_settings_t *settings)
{
    float climb_threshold_mps;
    float sink_threshold_mps;
    float vario_mps;

    if ((rt == NULL) || (settings == NULL))
    {
        return false;
    }

    climb_threshold_mps = ((float)settings->climb_tone_threshold_cms) * 0.01f;
    sink_threshold_mps  = ((float)settings->sink_tone_threshold_cms) * 0.01f;
    vario_mps = rt->fast_vario_bar_mps;

    if (vario_mps >= climb_threshold_mps)
    {
        return true;
    }

    if (vario_mps <= sink_threshold_mps)
    {
        return true;
    }

    return false;
}

void Vario_Audio_Init(void)
{
    const vario_settings_t *settings;
    uint8_t                 volume_percent;

    settings = Vario_Settings_Get();
    volume_percent = vario_audio_clamp_percent(settings->audio_volume_percent);

    Audio_App_SetVolumePercent(volume_percent);
    s_vario_audio_last_applied_volume = volume_percent;

    Audio_App_ReleaseVariometer(0u);
}

void Vario_Audio_Task(uint32_t now_ms)
{
    const vario_settings_t *settings;
    const vario_runtime_t  *rt;
    vario_mode_t            mode;
    uint8_t                 volume_percent;
    bool                    audio_active;
    int32_t                 audio_vario_cms;

    settings = Vario_Settings_Get();
    rt = Vario_State_GetRuntime();
    mode = Vario_State_GetMode();

    volume_percent = vario_audio_clamp_percent(settings->audio_volume_percent);
    if (volume_percent != s_vario_audio_last_applied_volume)
    {
        Audio_App_SetVolumePercent(volume_percent);
        s_vario_audio_last_applied_volume = volume_percent;
    }

    audio_active = ((settings->audio_enabled != 0u) &&
                    ((settings->beep_only_when_flying == 0u) ||
                     ((rt != NULL) && (rt->flight_active != false))) &&
                    (vario_audio_mode_owns_tone(mode) != false) &&
                    (vario_audio_is_outside_deadband(rt, settings) != false));

    audio_vario_cms = 0;
    if (rt != NULL)
    {
        audio_vario_cms = vario_audio_round_mps_to_cms(rt->fast_vario_bar_mps);
    }

    Audio_App_SetVariometerState(audio_active,
                                 audio_vario_cms,
                                 now_ms);
}
