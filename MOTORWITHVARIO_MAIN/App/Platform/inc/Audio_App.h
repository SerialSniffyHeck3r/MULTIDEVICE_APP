#ifndef AUDIO_APP_H
#define AUDIO_APP_H

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
/*  목적                                                                      */
/*  - 상위 앱(Vario_App / Motor_App 등)이 Audio_Driver나                      */
/*    APP_ALTITUDE debug transport를 직접 호출하지 않게 하기 위함이다.        */
/*  - 현재 variometer tone transport는 legacy APP_ALTITUDE debug path를       */
/*    재사용하고 있지만, upper layer 입장에서는 그 사실을 몰라도 되게        */
/*    이 façade가 경계를 잡아 준다.                                           */
/*                                                                            */
/*  주의                                                                      */
/*  - 이 API는 "제품 코드가 driver/debug API 이름을 직접 알지 않게 하자"     */
/*    는 1차 정리용이다.                                                      */
/*  - 추후 진짜 product-grade variometer mixer/policy가 생기면                */
/*    구현만 갈아끼우고 upper app 호출부는 그대로 유지할 수 있다.            */
/* -------------------------------------------------------------------------- */
void Audio_App_SetVolumePercent(uint8_t volume_percent);
void Audio_App_SetVariometerState(bool active, int32_t vario_cms, uint32_t now_ms);
void Audio_App_ReleaseVariometer(uint32_t now_ms);

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
