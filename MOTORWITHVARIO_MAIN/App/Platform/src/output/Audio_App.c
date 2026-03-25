#include "Audio_App.h"

#include "Audio_Driver.h"
#include "Audio_Presets.h"

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 4채널 합성 preset 재생                                            */
/*                                                                            */
/*  현재 사용자는                                                              */
/*    - ch1 STRING                                                             */
/*    - ch2 BRASS                                                              */
/*    - ch3 PIANO                                                              */
/*    - ch4 PERCUSSION                                                         */
/*  구성의 standard stack을 기본값으로 쓰길 원한다.                            */
/*                                                                            */
/*  그래서 앱 레벨 direct-call wrapper는                                       */
/*  우선 이 helper 하나를 통해 standard 4채널 합성 버전을 호출한다.            */
/* -------------------------------------------------------------------------- */
static void Audio_App_PlayMixedPreset(audio_note_preset_id_t note_preset_id)
{
    (void)Audio_Driver_PlaySequencePreset(AUDIO_TIMBRE_STACK_STANDARD_4CH,
                                          note_preset_id);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: mono preview 재생                                               */
/*                                                                            */
/*  비교 청취용 mono 버전은                                                    */
/*  현재 preset의 melody track을                                               */
/*  ch3 PIANO timbre 하나로만 재생한다.                                        */
/*                                                                            */
/*  이렇게 하면 같은 melody를                                                  */
/*    - 4채널 합성 버전                                                        */
/*    - 1채널 melody preview                                                   */
/*  두 방식으로 바로 들어 보고                                                 */
/*  더 나은 쪽만 남기거나 수정하기 쉽다.                                       */
/* -------------------------------------------------------------------------- */
static void Audio_App_PlayMonoPreset(audio_note_preset_id_t note_preset_id)
{
    (void)Audio_Driver_PlaySingleTrackPreset(AUDIO_TIMBRE_CH3_PIANO,
                                             note_preset_id);
}

static uint8_t Audio_App_ClampPercent(uint8_t volume_percent)
{
    if (volume_percent > 100u)
    {
        return 100u;
    }

    return volume_percent;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: init                                                             */
/*                                                                            */
/*  현재 단계에서는 앱 레벨에 별도 persistent runtime이 거의 없다.             */
/*  그래도 향후                                                                */
/*    - sound policy                                                            */
/*    - state별 mute / priority                                                 */
/*    - user volume / profile                                                   */
/*  같은 로직이 붙을 수 있으므로 init/task 함수 형태를 미리 만들어 둔다.       */
/* -------------------------------------------------------------------------- */
void Audio_App_Init(void)
{
    /* ---------------------------------------------------------------------- */
    /*  지금은 별도 앱 런타임 상태가 없으므로                                   */
    /*  여기서는 일부러 아무 것도 하지 않는다.                                 */
    /* ---------------------------------------------------------------------- */
}

/* -------------------------------------------------------------------------- */
/*  공개 API: task                                                             */
/* -------------------------------------------------------------------------- */
void Audio_App_Task(uint32_t now_ms)
{
    /* ---------------------------------------------------------------------- */
    /*  추후 이 자리에                                                          */
    /*    - 바리오 climb/sink 정책                                              */
    /*    - 전원 상태별 sound policy                                             */
    /*    - hourly chime 스케줄링                                                */
    /*    - user mute / quiet hour                                               */
    /*  같은 앱 정책 로직이 들어올 수 있다.                                     */
    /* ---------------------------------------------------------------------- */
    (void)now_ms;
}

/* -------------------------------------------------------------------------- */
/*  Upper-app convenience wrappers                                            */
/* -------------------------------------------------------------------------- */
void Audio_App_SetVolumePercent(uint8_t volume_percent)
{
    /* ---------------------------------------------------------------------- */
    /*  canonical volume sink는 Audio_Driver다.                               */
    /*  upper layer는 clamp 후 전달만 수행한다.                               */
    /* ---------------------------------------------------------------------- */
    Audio_Driver_SetVolumePercent(Audio_App_ClampPercent(volume_percent));
}

HAL_StatusTypeDef Audio_App_VariometerStart(app_audio_waveform_t waveform,
                                            uint32_t initial_freq_hz,
                                            uint16_t initial_level_permille)
{
    /* ---------------------------------------------------------------------- */
    /*  제품용 바리오는 여기서 곧바로 Audio_Driver의 연속-톤 전용 API로 간다. */
    /*  더 이상 APP_ALTITUDE debug audio 경로를 경유하지 않는다.             */
    /* ---------------------------------------------------------------------- */
    return Audio_Driver_VarioStart(waveform,
                                   initial_freq_hz,
                                   initial_level_permille);
}

HAL_StatusTypeDef Audio_App_VariometerSetTarget(uint32_t target_freq_hz,
                                                uint16_t target_level_permille,
                                                uint32_t glide_time_ms)
{
    return Audio_Driver_VarioSetTarget(target_freq_hz,
                                       target_level_permille,
                                       glide_time_ms);
}

HAL_StatusTypeDef Audio_App_VariometerStop(uint32_t release_time_ms)
{
    return Audio_Driver_VarioStop(release_time_ms);
}

bool Audio_App_IsVariometerActive(void)
{
    return Audio_Driver_IsVarioActive();
}

void Audio_App_ReleaseVariometer(uint32_t now_ms)
{
    /* ---------------------------------------------------------------------- */
    /*  now_ms 파라미터는 기존 상위 호출부와의 함수 시그니처 호환용으로만      */
    /*  유지한다.                                                              */
    /*                                                                        */
    /*  제품용 바리오 ownership 해제는 드라이버 전용 release path로 곧바로     */
    /*  연결한다.                                                              */
    /* ---------------------------------------------------------------------- */
    (void)now_ms;
    (void)Audio_Driver_VarioStop(48u);
}

/* -------------------------------------------------------------------------- */
/*  디버그 오디오 페이지 button 2~6용 test wrapper                             */
/*                                                                            */
/*  B2 : sine                                                                  */
/*  B3 : square                                                                */
/*  B4 : saw                                                                   */
/*  B5 : 4채널 boot sound                                                      */
/*  B6 : SD card에서 아무 WAV 하나                                              */
/* -------------------------------------------------------------------------- */
void Audio_App_DoSomething1(void)
{
    (void)Audio_Driver_PlaySineWave(440u, 300u);
}

void Audio_App_DoSomething2(void)
{
    (void)Audio_Driver_PlaySquareWave(440u);
}

void Audio_App_DoSomething3(void)
{
    (void)Audio_Driver_PlaySawToothWave(440u);
}

void Audio_App_DoSomething4(void)
{
    Audio_App_DoBootSound();
}

void Audio_App_DoSomething5(void)
{
    /* ---------------------------------------------------------------------- */
    /*  WAV 파일이 없거나 mount가 안 된 경우에도                                */
    /*  버튼을 눌렀는데 아무 반응도 없는 것처럼 보이면 디버깅이 불편하므로        */
    /*  실패 시에는 error sound를 대신 낸다.                                   */
    /* ---------------------------------------------------------------------- */
    if (Audio_Driver_PlayAnyWaveFromSd() != HAL_OK)
    {
        Audio_App_DoErrorSound();
    }
}

/* -------------------------------------------------------------------------- */
/*  mixed direct-call wrapper들                                                */
/* -------------------------------------------------------------------------- */
void Audio_App_DoBootSound(void)
{
    Audio_App_PlayMixedPreset(AUDIO_NOTE_PRESET_BOOT);
}

void Audio_App_DoPowerOffSound(void)
{
    Audio_App_PlayMixedPreset(AUDIO_NOTE_PRESET_POWER_OFF);
}

void Audio_App_DoWarningSound(void)
{
    Audio_App_PlayMixedPreset(AUDIO_NOTE_PRESET_WARNING);
}

void Audio_App_DoErrorSound(void)
{
    Audio_App_PlayMixedPreset(AUDIO_NOTE_PRESET_ERROR);
}

void Audio_App_DoFatalErrorSound(void)
{
    Audio_App_PlayMixedPreset(AUDIO_NOTE_PRESET_FATAL);
}

void Audio_App_DoVariometerPlaceholderSound(void)
{
    /* ---------------------------------------------------------------------- */
    /*  이 함수는 아직 실제 climb/sink rate 기반 바리오 로직이 아니다.          */
    /*  지금은 placeholder preset만 재생한다.                                  */
    /*  추후에는 Audio_App_Task() 또는 더 상위 정책 레이어에서                  */
    /*  상승/하강율에 따라 cadence / pitch / dead-band를 동적으로 생성하도록    */
    /*  바꾸는 것이 맞다.                                                       */
    /* ---------------------------------------------------------------------- */
    Audio_App_PlayMixedPreset(AUDIO_NOTE_PRESET_VARIO_PLACEHOLDER);
}

void Audio_App_DoButtonShortPressSound(void)
{
    Audio_App_PlayMixedPreset(AUDIO_NOTE_PRESET_BUTTON_SHORT);
}

void Audio_App_DoButtonLongPressSound(void)
{
    Audio_App_PlayMixedPreset(AUDIO_NOTE_PRESET_BUTTON_LONG);
}

void Audio_App_DoInformationSound(void)
{
    Audio_App_PlayMixedPreset(AUDIO_NOTE_PRESET_INFORMATION);
}

void Audio_App_DoHourlyChimeSound(void)
{
    Audio_App_PlayMixedPreset(AUDIO_NOTE_PRESET_HOURLY_CHIME);
}

/* -------------------------------------------------------------------------- */
/*  mono preview wrapper들                                                     */
/* -------------------------------------------------------------------------- */
void Audio_App_DoBootSoundMono(void)
{
    Audio_App_PlayMonoPreset(AUDIO_NOTE_PRESET_BOOT);
}

void Audio_App_DoPowerOffSoundMono(void)
{
    Audio_App_PlayMonoPreset(AUDIO_NOTE_PRESET_POWER_OFF);
}

void Audio_App_DoWarningSoundMono(void)
{
    Audio_App_PlayMonoPreset(AUDIO_NOTE_PRESET_WARNING);
}

void Audio_App_DoErrorSoundMono(void)
{
    Audio_App_PlayMonoPreset(AUDIO_NOTE_PRESET_ERROR);
}

void Audio_App_DoFatalErrorSoundMono(void)
{
    Audio_App_PlayMonoPreset(AUDIO_NOTE_PRESET_FATAL);
}

void Audio_App_DoVariometerPlaceholderSoundMono(void)
{
    Audio_App_PlayMonoPreset(AUDIO_NOTE_PRESET_VARIO_PLACEHOLDER);
}

void Audio_App_DoButtonShortPressSoundMono(void)
{
    Audio_App_PlayMonoPreset(AUDIO_NOTE_PRESET_BUTTON_SHORT);
}

void Audio_App_DoButtonLongPressSoundMono(void)
{
    Audio_App_PlayMonoPreset(AUDIO_NOTE_PRESET_BUTTON_LONG);
}

void Audio_App_DoInformationSoundMono(void)
{
    Audio_App_PlayMonoPreset(AUDIO_NOTE_PRESET_INFORMATION);
}

void Audio_App_DoHourlyChimeSoundMono(void)
{
    Audio_App_PlayMonoPreset(AUDIO_NOTE_PRESET_HOURLY_CHIME);
}

/* -------------------------------------------------------------------------- */
/*  WAV file test                                                              */
/* -------------------------------------------------------------------------- */
void Audio_App_DoPlayAnyWaveFromSd(void)
{
    (void)Audio_Driver_PlayAnyWaveFromSd();
}

/* -------------------------------------------------------------------------- */
/*  정지 wrapper                                                               */
/* -------------------------------------------------------------------------- */
void Audio_App_Stop(void)
{
    Audio_Driver_Stop();
}
