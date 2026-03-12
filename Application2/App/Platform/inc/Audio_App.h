#ifndef AUDIO_APP_H
#define AUDIO_APP_H

#include "main.h"

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
