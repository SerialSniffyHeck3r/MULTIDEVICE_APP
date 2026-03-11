#ifndef DS18B20_DRIVER_H
#define DS18B20_DRIVER_H

#include "main.h"
#include "APP_STATE.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  DS18B20_DRIVER                                                             */
/*                                                                            */
/*  기존 ow.c + ds18b20.c 경로를 완전히 우회하는 단순 드라이버다.              */
/*                                                                            */
/*  핵심 특징                                                                  */
/*  - TIM callback register 기능을 전혀 쓰지 않는다.                           */
/*  - .ioc 재생성 때 USE_HAL_TIM_REGISTER_CALLBACKS 가 0으로 되돌아가도         */
/*    아무 영향이 없다.                                                        */
/*  - microsecond timing은 Cortex-M DWT cycle counter를 이용해 처리한다.       */
/*  - 공개 데이터는 APP_STATE.ds18b20 안에만 저장한다.                         */
/*                                                                            */
/*  사용 규칙                                                                  */
/*  - 부팅 시 DS18B20_DRIVER_Init() 한 번 호출                                 */
/*  - main while(1) 안에서 DS18B20_DRIVER_Task(now_ms) 반복 호출               */
/*  - UI / logger 는 APP_STATE_CopyDs18b20Snapshot() 또는                      */
/*    APP_STATE_CopySensorDebugSnapshot() 만 사용                              */
/* -------------------------------------------------------------------------- */

#ifndef DS18B20_DRIVER_GPIO_Port
#define DS18B20_DRIVER_GPIO_Port  DS18B20_OW_GPIO_Port
#endif

#ifndef DS18B20_DRIVER_Pin
#define DS18B20_DRIVER_Pin        DS18B20_OW_Pin
#endif

/* 측정 주기.
 * DS18B20은 온도 변화가 느리므로 1Hz가 기본값이다. */
#ifndef DS18B20_DRIVER_PERIOD_MS
#define DS18B20_DRIVER_PERIOD_MS  1000u
#endif

/* presence 실패 / 초기화 실패 시 재시도 주기 */
#ifndef DS18B20_DRIVER_RETRY_MS
#define DS18B20_DRIVER_RETRY_MS   1000u
#endif

/* 기본 해상도: 12-bit */
#ifndef DS18B20_DRIVER_RESOLUTION_BITS
#define DS18B20_DRIVER_RESOLUTION_BITS 12u
#endif

/* -------------------------------------------------------------------------- */
/*  브레드보드 bring-up 안전장치                                               */
/*                                                                            */
/*  주의                                                                      */
/*  - DS18B20 1-Wire의 정석은 "외부 4.7k pull-up" 이다.                       */
/*  - 아래 내부 pull-up은 "아예 floating line이 되는 최악"을 피하기 위한       */
/*    약한 fallback 이다.                                                     */
/*  - 즉, 이 매크로를 켠다고 외부 pull-up 없이 양산 OK 라는 뜻은 아니다. 회로 설계시 반드시 외부 풒업을 달 것.   */
/* -------------------------------------------------------------------------- */

/* 내부 pull-up fallback 사용 여부 */
#ifndef DS18B20_DRIVER_USE_INTERNAL_PULLUP_FALLBACK
#define DS18B20_DRIVER_USE_INTERNAL_PULLUP_FALLBACK 1u
#endif

/* bus release 직후 "정말 high로 올라왔는가" 확인하는 대기 시간.
 * 기존 5us는 브레드보드 + 긴 점퍼 + 약한 pull-up 에서 너무 빡빡했다.
 * 12us로 늘려도 DS18B20 presence 시작 구간(15~60us) 전에 충분히 검사 가능하다. */
#ifndef DS18B20_DRIVER_BUS_HIGH_CHECK_US
#define DS18B20_DRIVER_BUS_HIGH_CHECK_US 12u
#endif

/* 단일 센서라고 가정할 때, READ ROM으로 받아야 하는 family code.
 * DS18B20은 일반적으로 0x28 이다. */
#ifndef DS18B20_DRIVER_EXPECT_FAMILY_CODE
#define DS18B20_DRIVER_EXPECT_FAMILY_CODE 0x28u
#endif

/* -------------------------------------------------------------------------- */
/*  공개 API                                                                   */
/* -------------------------------------------------------------------------- */

void DS18B20_DRIVER_Init(void);
void DS18B20_DRIVER_Task(uint32_t now_ms);

/* -------------------------------------------------------------------------- */
/*  레거시 호환 매크로                                                         */
/*                                                                            */
/*  기존 코드가 DS18B20_AppInit / AppUpdate 이름을 기대해도                    */
/*  새 드라이버로 쉽게 연결할 수 있게 최소한의 alias만 남긴다.                */
/* -------------------------------------------------------------------------- */
#define DS18B20_AppInit()     DS18B20_DRIVER_Init()
#define DS18B20_AppUpdate()   DS18B20_DRIVER_Task(HAL_GetTick())

#ifdef __cplusplus
}
#endif

#endif /* DS18B20_DRIVER_H */
