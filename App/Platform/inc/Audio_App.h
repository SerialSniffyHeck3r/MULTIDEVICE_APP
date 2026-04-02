#ifndef AUDIO_APP_H
#define AUDIO_APP_H

#include "APP_STATE.h"
#include "main.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  Audio_App                                                                  */
/*                                                                            */
/*  역할                                                                      */
/*  - Audio_Driver의 저수준 API를                                              */
/*    "기기 의미 단위의 함수 이름" 으로 감싼다.                               */
/*  - 앞으로 상위 계층이 늘어나더라도                                           */
/*    현재 앱 코드는 이 파일의 direct-call wrapper를 바로 호출하면 된다.       */
/*                                                                            */
/*  예시                                                                      */
/*    Audio_App_DoBootSound();                                                 */
/*    Audio_App_DoErrorSound();                                                */
/*    Audio_App_DoPlayAnyWaveFromSd();                                         */
/* -------------------------------------------------------------------------- */

void Audio_App_Init(void);
void Audio_App_Task(uint32_t now_ms);

/* -------------------------------------------------------------------------- */
/*  Upper-app convenience wrappers                                            */
/*                                                                            */
/*  핵심 경계                                                                 */
/*  - 상위 앱은 Audio_Driver를 직접 두드리지 않는다.                          */
/*  - 상위 앱은 "무슨 의미의 소리인가" 만 Audio_App에 전달한다.              */
/*  - Audio_App는 그 의미를 저수준 driver API로 옮기는 façade다.             */
/*                                                                            */
/*  variometer 전용 경계                                                      */
/*  - 제품용 바리오 오디오는 더 이상 APP_ALTITUDE debug audio 경로를          */
/*    경유하지 않는다.                                                        */
/*  - Vario_Audio 상위 엔진은 아래 4개 함수만 사용해                          */
/*      waveform / freq / level / release                                     */
/*    를 제어한다.                                                            */
/*  - 이렇게 해야                                                             */
/*      APP_STATE low-level snapshot                                          */
/*        -> Vario_State derived runtime                                      */
/*        -> Vario_Audio product policy                                       */
/*        -> Audio_App façade                                                 */
/*        -> Audio_Driver transport                                           */
/*    의 계층이 선명하게 유지된다.                                            */
/* -------------------------------------------------------------------------- */
void Audio_App_SetVolumePercent(uint8_t volume_percent);
HAL_StatusTypeDef Audio_App_VariometerStart(app_audio_waveform_t waveform,
                                            uint32_t initial_freq_hz,
                                            uint16_t initial_level_permille);
HAL_StatusTypeDef Audio_App_VariometerSetTarget(uint32_t target_freq_hz,
                                                uint16_t target_level_permille,
                                                uint32_t glide_time_ms);
HAL_StatusTypeDef Audio_App_VariometerStop(uint32_t release_time_ms);
bool              Audio_App_IsVariometerActive(void);
void              Audio_App_ReleaseVariometer(uint32_t now_ms);

/* -------------------------------------------------------------------------- */
/*  디버그 오디오 페이지 button 2~6용                                           */
/* -------------------------------------------------------------------------- */
void Audio_App_DoSomething1(void);
void Audio_App_DoSomething2(void);
void Audio_App_DoSomething3(void);
void Audio_App_DoSomething4(void);
void Audio_App_DoSomething5(void);

/* -------------------------------------------------------------------------- */
/*  직접 호출 가능한 사전 정의 효과음                                           */
/*                                                                            */
/*  mixed 버전: 4채널 합성                                                      */
/*  mono  버전: melody track만 단일 채널로 비교 청취                            */
/* -------------------------------------------------------------------------- */
void Audio_App_DoBootSound(void);
void Audio_App_DoBootSoundMono(void);

void Audio_App_DoPowerOffSound(void);
void Audio_App_DoPowerOffSoundMono(void);

void Audio_App_DoWarningSound(void);
void Audio_App_DoWarningSoundMono(void);

void Audio_App_DoErrorSound(void);
void Audio_App_DoErrorSoundMono(void);

void Audio_App_DoFatalErrorSound(void);
void Audio_App_DoFatalErrorSoundMono(void);

void Audio_App_DoVariometerPlaceholderSound(void);
void Audio_App_DoVariometerPlaceholderSoundMono(void);

void Audio_App_DoButtonShortPressSound(void);
void Audio_App_DoButtonShortPressSoundMono(void);

void Audio_App_DoButtonLongPressSound(void);
void Audio_App_DoButtonLongPressSoundMono(void);

void Audio_App_DoInformationSound(void);
void Audio_App_DoInformationSoundMono(void);

void Audio_App_DoHourlyChimeSound(void);
void Audio_App_DoHourlyChimeSoundMono(void);

/* -------------------------------------------------------------------------- */
/*  WAV file 테스트                                                             */
/* -------------------------------------------------------------------------- */
void Audio_App_DoPlayAnyWaveFromSd(void);

/* 현재 재생 중인 content를 정지 */
void Audio_App_Stop(void);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_APP_H */
