#ifndef LED_DRIVER_H
#define LED_DRIVER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "main.h"
#include <stdint.h>

/* ========================================================================== */
/*  LED_Driver                                                                */
/*                                                                            */
/*  역할                                                                      */
/*  - CubeMX가 생성한 PWM 채널을 실제로 start 한다.                           */
/*  - 논리 LED 번호(0~10)를 실제 TIMx/CHy 물리 채널에 매핑한다.               */
/*  - 상위 App 계층이 전달한 선형 밝기(permille, 0~1000)를 감마 보정한 뒤     */
/*    실제 PWM compare 값으로 변환하여 CCR 레지스터에 기록한다.               */
/*                                                                            */
/*  설계 철학                                                                  */
/*  - 이 파일은 "하드웨어 바로 위" 계층이다.                                  */
/*  - LED가 무엇을 표현할지(속도계, 스캐너, 브리드 등)는 절대 여기서 결정하지  */
/*    않는다. 그 역할은 LED_App 계층이 맡는다.                               */
/*                                                                            */
/*  CubeMX 재생성 안전성                                                       */
/*  - 이 파일은 신규 파일이다.                                                */
/*  - generated tim.c / gpio.c / main.c 내부를 덮어쓰지 않는다.               */
/*  - 특히 TIM9의 PSC/ARR 같은 timer base는 절대 다시 설정하지 않는다.        */
/*    LED11은 TIM9_CH2를 사용하지만, TIM9_CH1은 BEEP_SOUND와 공유이므로      */
/*    compare만 갱신해야 한다.                                                */
/* ========================================================================== */

#define LED_DRIVER_CHANNEL_COUNT   (11u)

/* -------------------------------------------------------------------------- */
/*  논리 LED 인덱스                                                            */
/*                                                                            */
/*  이 enum의 의미는 "UI에서 왼쪽에서 오른쪽으로 본 논리 순서"다.            */
/*                                                                            */
/*      논리 0  = LED1  = 가장 왼쪽                                           */
/*      논리 5  = LED6  = 가운데                                               */
/*      논리 10 = LED11 = 가장 오른쪽                                          */
/*                                                                            */
/*  실제 PCB 배치가 다르더라도, 상위 App 코드는 이 논리 번호를 기준으로만      */
/*  작성하고, 물리 배선 차이는 LED_Driver.c 안의 매핑 테이블만 수정한다.      */
/* -------------------------------------------------------------------------- */
typedef enum
{
    LED_DRIVER_INDEX_0  = 0u,
    LED_DRIVER_INDEX_1  = 1u,
    LED_DRIVER_INDEX_2  = 2u,
    LED_DRIVER_INDEX_3  = 3u,
    LED_DRIVER_INDEX_4  = 4u,
    LED_DRIVER_INDEX_5  = 5u,
    LED_DRIVER_INDEX_6  = 6u,
    LED_DRIVER_INDEX_7  = 7u,
    LED_DRIVER_INDEX_8  = 8u,
    LED_DRIVER_INDEX_9  = 9u,
    LED_DRIVER_INDEX_10 = 10u
} LED_DriverIndex_t;

/* -------------------------------------------------------------------------- */
/*  LED_Driver_Init                                                            */
/*                                                                            */
/*  호출 시점                                                                  */
/*  - 반드시 MX_TIM1_Init ~ MX_TIM9_Init 이후                                 */
/*  - 즉, CubeMX가 타이머 핸들들을 모두 초기화한 직후                         */
/*                                                                            */
/*  동작                                                                        */
/*  - 감마 LUT 생성                                                            */
/*  - LED용 PWM 채널 start                                                     */
/*  - 모든 LED를 꺼진 상태로 정렬                                              */
/* -------------------------------------------------------------------------- */
void LED_Driver_Init(void);

/* -------------------------------------------------------------------------- */
/*  전체 LED 즉시 소등                                                         */
/* -------------------------------------------------------------------------- */
void LED_Driver_AllOff(void);

/* -------------------------------------------------------------------------- */
/*  단일 LED 선형 밝기 기록                                                   */
/*                                                                            */
/*  입력                                                                        */
/*  - logical_index       : 0 ~ 10                                             */
/*  - brightness_permille : 0 ~ 1000                                           */
/*                                                                            */
/*  의미                                                                        */
/*  - 0    = 완전 소등                                                         */
/*  - 1000 = 그 채널에서 가능한 최대 밝기                                     */
/* -------------------------------------------------------------------------- */
void LED_Driver_WriteLinearOnePermille(uint8_t logical_index,
                                       uint16_t brightness_permille);

/* -------------------------------------------------------------------------- */
/*  11채널 프레임 전체 기록                                                    */
/*                                                                            */
/*  입력                                                                        */
/*  - frame_permille[11] : 각 LED의 선형 밝기 값                               */
/* -------------------------------------------------------------------------- */
void LED_Driver_WriteLinearFramePermille(
    const uint16_t frame_permille[LED_DRIVER_CHANNEL_COUNT]);

/* -------------------------------------------------------------------------- */
/*  디버그용 조회 API                                                           */
/* -------------------------------------------------------------------------- */
uint16_t LED_Driver_GetLastLinearPermille(uint8_t logical_index);
uint32_t LED_Driver_GetLastPwmCompare(uint8_t logical_index);

#ifdef __cplusplus
}
#endif

#endif /* LED_DRIVER_H */
