#ifndef POWER_STATE_H
#define POWER_STATE_H

#include "main.h"

#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* POWER_STATE                                                                 */
/*                                                                            */
/* 역할                                                                       */
/* - SparkFun Soft Power Switch Mk2의 PUSH / OFF GPIO를 앱 런타임에 묶는다.   */
/* - 전원키 짧은 누름 -> QUICK SETTINGS STUB 화면 진입                       */
/* - 전원키 긴 누름 -> POWER OFF 확인 화면 진입                              */
/* - 앱 첫 진입 직후 -> CONFIRM POWER ON? 30초 카운트다운 화면 표시          */
/*                                                                            */
/* 설계 원칙                                                                   */
/* - APP_STATE에 전원 UI 상태를 넣지 않는다.                                  */
/* - CubeMX가 생성하는 gpio.c 설정을 믿기만 하지 않고,                        */
/*   이 모듈이 런타임에서 Soft Power 핀을 한 번 더 안전하게 정렬한다.         */
/* - 기존 UI 엔진(ui_engine.c)을 크게 흔들지 않고,                            */
/*   "전원 관련 오버레이"만 별도 상태머신으로 덧씌운다.                      */
/* - 이 모듈이 UI를 점유 중일 때는 Button event queue를 직접 소비해서         */
/*   기존 UI 화면으로 이벤트가 새어 들어가지 않게 막는다.                    */
/* -------------------------------------------------------------------------- */

typedef enum
{
    POWER_STATE_MODE_NONE = 0u,
    POWER_STATE_MODE_CONFIRM_ON,
    POWER_STATE_MODE_QUICK_SETTINGS,
    POWER_STATE_MODE_CONFIRM_OFF,
    POWER_STATE_MODE_POWERING_OFF
} power_state_mode_t;

/* -------------------------------------------------------------------------- */
/* 초기화                                                                      */
/*                                                                            */
/* 호출 시점                                                                   */
/* - Application2/Core/Src/main.c 의 USER CODE BEGIN 2 영역                   */
/* - Button_Init() 이후                                                        */
/*                                                                            */
/* 이 함수는                                                                   */
/* 1) Soft Power GPIO 방향/풀업/초기 출력 레벨을 런타임에서 다시 맞추고       */
/* 2) 내부 전원키 debounce/hold 상태를 현재 실제 GPIO 상태와 동기화한다.      */
/* -------------------------------------------------------------------------- */
void POWER_STATE_Init(void);

/* -------------------------------------------------------------------------- */
/* 부팅 직후 전원 ON 확인 화면 진입                                            */
/*                                                                            */
/* 호출 시점                                                                   */
/* - LCD 초기화가 끝난 뒤                                                     */
/* - 무거운 센서/통신 bring-up 전에                                            */
/*                                                                            */
/* 이후 main.c는 POWER_STATE_IsUiBlocking()가 false가 될 때까지               */
/* Button_Task() + POWER_STATE_TaskBootGate()만 반복 호출하면 된다.           */
/* -------------------------------------------------------------------------- */
void POWER_STATE_EnterPowerOnConfirm(uint32_t now_ms);

/* -------------------------------------------------------------------------- */
/* 부팅 직후 전원 ON 확인 화면 전용 task                                       */
/*                                                                            */
/* 이 함수는 TIM7 frame token을 요구하지 않는다.                               */
/* 즉, UI timer를 아직 start하지 않은 부트 bring-up 구간에서도                  */
/* 직접 U8G2 buffer를 갱신해 화면을 그릴 수 있다.                              */
/* -------------------------------------------------------------------------- */
void POWER_STATE_TaskBootGate(uint32_t now_ms);

/* -------------------------------------------------------------------------- */
/* 일반 런타임 task                                                            */
/*                                                                            */
/* 호출 시점                                                                   */
/* - main while(1) 안                                                         */
/* - Button_Task(now_ms) 이후                                                 */
/* - UI_Engine_Task(now_ms) 이전                                               */
/*                                                                            */
/* 이 함수는                                                                   */
/* 1) Soft Power PUSH 입력 polling + debounce + short/long 판정               */
/* 2) 전원 오버레이 상태에서 F1/F6 이벤트 소비                                 */
/* 3) 전원 오버레이가 활성일 때 해당 화면 draw                                 */
/* 를 수행한다.                                                                */
/* -------------------------------------------------------------------------- */
void POWER_STATE_Task(uint32_t now_ms);

/* -------------------------------------------------------------------------- */
/* 현재 전원 UI가 기존 UI 엔진을 가로막고 있는가?                              */
/*                                                                            */
/* true 이면                                                                   */
/* - UI_Engine_Task()를 이번 loop에서 호출하지 않는다.                         */
/* - POWER_STATE가 event queue와 LCD를 점유한다.                               */
/* -------------------------------------------------------------------------- */
bool POWER_STATE_IsUiBlocking(void);

/* -------------------------------------------------------------------------- */
/* 디버그/로그용 현재 모드 getter                                               */
/* -------------------------------------------------------------------------- */
power_state_mode_t POWER_STATE_GetMode(void);

#ifdef __cplusplus
}
#endif

#endif /* POWER_STATE_H */
