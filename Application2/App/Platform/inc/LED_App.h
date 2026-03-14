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
/*  - LED가 "무엇을 표현해야 하는지"를 고르는 상위 계층이다.                 */
/*  - 실제 frame 생성, smoothing, perceptual remap, PWM compare 계산은        */
/*    LED_Driver가 맡는다.                                                    */
/*  - main loop는 LED_App_Task(now_ms) 하나만 호출하면 된다.                  */
/*                                                                            */
/*  이번 개편 핵심                                                             */
/*  - SPEEDOMETER 모드는 제거했다.                                            */
/*  - 부팅 직후 기본 상태는 반드시 OFF다.                                     */
/*  - 테스트 패턴은 app 계층에서 어떤 패턴을 고를지만 결정하고,               */
/*    실제 패턴 그림은 driver가 렌더링한다.                                   */
/*  - F2 long press에서 이 app의 test pattern advance API를 호출하면          */
/*    토스트 메시지와 함께 즉시 LED 모드가 바뀌게 설계되어 있다.              */
/* ========================================================================== */

typedef enum
{
    LED_APP_MODE_OFF = 0u,
    LED_APP_MODE_SOLID_ALL,
    LED_APP_MODE_BREATH_IDLE,
    LED_APP_MODE_WELCOME_SWEEP,
    LED_APP_MODE_SCANNER,
    LED_APP_MODE_ALERT_BLINK,
    LED_APP_MODE_SINGLE_DOT,
    LED_APP_MODE_CUSTOM_FRAME,
    LED_APP_MODE_TEST_PATTERN
} LED_AppMode_t;

/* -------------------------------------------------------------------------- */
/*  테스트 패턴 enum                                                           */
/*                                                                            */
/*  최소 10개 이상 요구사항을 만족하도록 OFF 포함 11개 + 2개 추가,             */
/*  총 13개 상태를 순환한다.                                                  */
/*                                                                            */
/*  boot 직후에는 OFF를 유지하고, F2 long press 시 다음 패턴으로 advance한다. */
/* -------------------------------------------------------------------------- */
typedef enum
{
    LED_APP_TEST_PATTERN_OFF = 0u,
    LED_APP_TEST_PATTERN_ALL_ON,
    LED_APP_TEST_PATTERN_WALK_LEFT_TO_RIGHT,
    LED_APP_TEST_PATTERN_WALK_RIGHT_TO_LEFT,
    LED_APP_TEST_PATTERN_SCANNER,
    LED_APP_TEST_PATTERN_CENTER_OUT,
    LED_APP_TEST_PATTERN_EDGE_IN,
    LED_APP_TEST_PATTERN_ODD_EVEN,
    LED_APP_TEST_PATTERN_HALF_SWAP,
    LED_APP_TEST_PATTERN_BREATH_ALL,
    LED_APP_TEST_PATTERN_BREATH_CENTER,
    LED_APP_TEST_PATTERN_BAR_FILL,
    LED_APP_TEST_PATTERN_STROBE_SLOW,
    LED_APP_TEST_PATTERN_COUNT
} LED_AppTestPattern_t;

void LED_App_Init(void);
void LED_App_Task(uint32_t now_ms);
LED_AppMode_t LED_App_GetMode(void);

/* -------------------------------------------------------------------------- */
/*  전체 글로벌 밝기 스케일 설정                                               */
/*                                                                            */
/*  Permille API는 하위 호환용, Q16 API는 신규 full-scale 밝기 축이다.        */
/* -------------------------------------------------------------------------- */
void LED_App_SetGlobalBrightnessScale(uint16_t scale_permille);
void LED_App_SetGlobalBrightnessScaleQ16(uint16_t scale_q16);

/* -------------------------------------------------------------------------- */
/*  부드러운 전환 기본 시간(ms)                                                */
/*                                                                            */
/*  일반 모드(OFF, SOLID, CUSTOM 등)에서 사용할 기본 전환 시간이다.           */
/*  테스트 패턴과 특수 애니메이션은 내부 helper가 별도 값을 쓸 수 있다.       */
/* -------------------------------------------------------------------------- */
void LED_App_SetTransitionTimeMs(uint16_t transition_ms);

/* -------------------------------------------------------------------------- */
/*  일반 모드 setter                                                           */
/* -------------------------------------------------------------------------- */
void LED_Off(void);
void LED_AllOn(uint16_t brightness_permille);
void LED_AllOnQ16(uint16_t brightness_q16);
void LED_BreathIdle(void);
void LED_WelcomeSweep(void);
void LED_Scanner(void);
void LED_AlertBlink(void);
void LED_SingleDot(uint8_t logical_led_index, uint16_t brightness_permille);
void LED_SingleDotQ16(uint8_t logical_led_index, uint16_t brightness_q16);
void LED_CustomFrame(const uint16_t frame_permille[LED_DRIVER_CHANNEL_COUNT]);
void LED_CustomFrameQ16(const uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT]);

/* -------------------------------------------------------------------------- */
/*  테스트 패턴 제어 API                                                        */
/*                                                                            */
/*  SetTestPattern()                                                           */
/*  - 특정 패턴으로 즉시 전환                                                  */
/*                                                                            */
/*  AdvanceTestPattern()                                                       */
/*  - 현재 패턴에서 다음 패턴으로 이동                                         */
/*  - 내부적으로 app mode까지 함께 바꾸고,                                    */
/*    사용자에게 보여 줄 toast text 포인터를 반환한다.                        */
/*                                                                            */
/*  GetModeName()                                                              */
/*  - 현재 활성 LED mode / test pattern 이름을 읽을 수 있다.                  */
/* -------------------------------------------------------------------------- */
void LED_App_SetTestPattern(LED_AppTestPattern_t pattern);
LED_AppTestPattern_t LED_App_GetTestPattern(void);
const char *LED_App_GetTestPatternName(LED_AppTestPattern_t pattern);
const char *LED_App_AdvanceTestPattern(void);
const char *LED_App_GetModeName(void);

#ifdef __cplusplus
}
#endif

#endif /* LED_APP_H */
