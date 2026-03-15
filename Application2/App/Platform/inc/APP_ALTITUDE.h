#ifndef APP_ALTITUDE_H
#define APP_ALTITUDE_H

#include "main.h"
#include "APP_STATE.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  APP_ALTITUDE                                                               */
/*                                                                            */
/*  역할                                                                       */
/*  - MS5611 pressure + manual QNH로부터 pressure altitude / QNH altitude를    */
/*    계산한다.                                                               */
/*  - GPS hMSL을 absolute anchor로 사용해서                                   */
/*    no-IMU / IMU-aided absolute altitude를 병렬로 추정한다.                 */
/*  - fast/slow vario, relative/home altitude, grade를                        */
/*    APP_STATE.altitude 에 항상 유지한다.                                    */
/*  - legacy debug ALTITUDE 페이지가 켜져 있을 때는                           */
/*    vario 기반 디버그 비프음을 낸다.                                        */
/*                                                                            */
/*  사용 규칙                                                                  */
/*  - 부팅 시 APP_ALTITUDE_Init(now_ms) 한 번 호출                            */
/*  - main while(1) 안에서 APP_ALTITUDE_Task(now_ms) 반복 호출                */
/*  - UI / logger / 다른 계층은 APP_STATE_CopyAltitudeSnapshot()만 사용       */
/* -------------------------------------------------------------------------- */
void APP_ALTITUDE_Init(uint32_t now_ms);
void APP_ALTITUDE_Task(uint32_t now_ms);

/* -------------------------------------------------------------------------- */
/*  Legacy debug ALTITUDE 페이지 연동용 API                                    */
/*                                                                            */
/*  debug page가 현재 열려 있는지 알려주면                                     */
/*  APP_ALTITUDE는 vario beep를 on/off 하고                                    */
/*  자기 소유 tone만 안전하게 정지한다.                                        */
/* -------------------------------------------------------------------------- */
void APP_ALTITUDE_DebugSetUiActive(bool active, uint32_t now_ms);

/* -------------------------------------------------------------------------- */
/*  Home altitude 강제 재캡처                                                  */
/*                                                                            */
/*  ALTITUDE debug page에서                                                    */
/*  현재 fused altitude를 새로운 home 기준으로 잡고 싶을 때 호출한다.         */
/* -------------------------------------------------------------------------- */
void APP_ALTITUDE_DebugRequestHomeCapture(void);

/* -------------------------------------------------------------------------- */
/*  Baro bias 재정렬 요청                                                      */
/*                                                                            */
/*  현재 pressure/QNH altitude와 filter absolute altitude가                    */
/*  크게 어긋난 상태에서                                                       */
/*  fused bias state만 다시 맞추고 싶을 때 호출한다.                           */
/* -------------------------------------------------------------------------- */
void APP_ALTITUDE_DebugRequestBiasRezero(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_ALTITUDE_H */
