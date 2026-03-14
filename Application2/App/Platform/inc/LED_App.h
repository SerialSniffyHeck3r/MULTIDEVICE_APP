#ifndef LED_APP_H
#define LED_APP_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "LED_Driver.h"

/* ========================================================================== */
/*  LED_App                                                                   */
/*                                                                            */
/*  역할                                                                      */
/*  - LED가 무엇을 표현할지 정의하는 상위 계층                                 */
/*  - 패턴이 계산한 target frame을 current frame으로 부드럽게 이행             */
/*  - 최종 frame을 LED_Driver에 전달                                           */
/*                                                                            */
/*  사용 방법                                                                  */
/*  1) main init에서 LED_App_Init() 1회 호출                                  */
/*  2) main while에서 LED_App_Task(HAL_GetTick()) 매 loop 호출                */
/*  3) 상위 app에서 LED_Speedometer(), LED_Scanner() 같은 mode setter 호출    */
/*                                                                            */
/*  특징                                                                       */
/*  - 완전 논블로킹                                                            */
/*  - delay 없음                                                               */
/*  - APP_STATE 의존성 없음                                                    */
/*  - CubeMX 재생성 안전: 신규 파일                                           */
/* ========================================================================== */

typedef enum
{
    LED_APP_MODE_OFF = 0u,
    LED_APP_MODE_SOLID_ALL,
    LED_APP_MODE_BREATH_IDLE,
    LED_APP_MODE_WELCOME_SWEEP,
    LED_APP_MODE_SPEEDOMETER,
    LED_APP_MODE_SCANNER,
    LED_APP_MODE_ALERT_BLINK,
    LED_APP_MODE_SINGLE_DOT,
    LED_APP_MODE_CUSTOM_FRAME
} LED_AppMode_t;

void LED_App_Init(void);
void LED_App_Task(uint32_t now_ms);
LED_AppMode_t LED_App_GetMode(void);

/* 전체 글로벌 밝기 스케일 (0~1000) */
void LED_App_SetGlobalBrightnessScale(uint16_t scale_permille);

/* 부드러운 전환 기본 시간 설정(ms) */
void LED_App_SetTransitionTimeMs(uint16_t transition_ms);

/* 모드 함수들: 모두 논블로킹 mode setter */
void LED_Off(void);
void LED_AllOn(uint16_t brightness_permille);
void LED_BreathIdle(void);
void LED_WelcomeSweep(void);
void LED_Speedometer(uint16_t gauge_permille);
void LED_Scanner(void);
void LED_AlertBlink(void);
void LED_SingleDot(uint8_t logical_led_index, uint16_t brightness_permille);
void LED_CustomFrame(const uint16_t frame_permille[LED_DRIVER_CHANNEL_COUNT]);

#ifdef __cplusplus
}
#endif

#endif /* LED_APP_H */
