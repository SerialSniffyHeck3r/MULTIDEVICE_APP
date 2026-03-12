#ifndef BUTTON_H
#define BUTTON_H

#include "main.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  button                                                                    */
/*                                                                            */
/*  목적                                                                      */
/*  - BUTTON1 ~ BUTTON6 입력을                                                */
/*    "가벼운 EXTI + main loop task" 구조로 처리한다.                         */
/*  - IRQ에서는 오직 "상태가 바뀌었을 가능성" 만 기록하고 빠르게 빠져나온다.   */
/*  - 실제 debounce 판정 / short press / long press 판정 / event queue 적재는 */
/*    Button_Task() 에서 수행한다.                                            */
/*                                                                            */
/*  사용 규칙                                                                  */
/*  1) CubeMX에서 BUTTON1 ~ BUTTON6을                                         */
/*     EXTI Rising/Falling + Pull-up 으로 설정한다.                           */
/*  2) 부팅 시 Button_Init()를 1회 호출한다.                                  */
/*  3) HAL_GPIO_EXTI_Callback()에서                                           */
/*     Button_OnExtiInterrupt(GPIO_Pin)를 호출한다.                           */
/*  4) main while(1) 안에서 Button_Task(HAL_GetTick())를 반복 호출한다.       */
/*  5) 앱 로직은 Button_PopEvent()로 event queue를 소비해서 처리한다.         */
/*                                                                            */
/*  기본 전기적 가정                                                            */
/*  - 현재 프로젝트의 GPIO 설정이 Pull-up 기반이므로                           */
/*    버튼 눌림(active level)은 LOW(GPIO_PIN_RESET)로 본다.                   */
/*  - 만약 실제 회로가 active-high 라면 BUTTON_ACTIVE_LEVEL만 바꾸면 된다.     */
/* -------------------------------------------------------------------------- */

#ifndef BUTTON_DEBOUNCE_MS
#define BUTTON_DEBOUNCE_MS 25u
#endif

#ifndef BUTTON_LONG_PRESS_MS
#define BUTTON_LONG_PRESS_MS 700u
#endif

#ifndef BUTTON_EVENT_QUEUE_SIZE
#define BUTTON_EVENT_QUEUE_SIZE 16u
#endif

#ifndef BUTTON_ACTIVE_LEVEL
#define BUTTON_ACTIVE_LEVEL GPIO_PIN_RESET
#endif

/* -------------------------------------------------------------------------- */
/*  버튼 ID                                                                    */
/*                                                                            */
/*  숫자 1~6이 그대로 화면 표시 요구사항과 매칭되도록                          */
/*  enum 값도 1~6으로 맞춘다.                                                 */
/* -------------------------------------------------------------------------- */
typedef enum
{
    BUTTON_ID_NONE = 0u,
    BUTTON_ID_1    = 1u,
    BUTTON_ID_2    = 2u,
    BUTTON_ID_3    = 3u,
    BUTTON_ID_4    = 4u,
    BUTTON_ID_5    = 5u,
    BUTTON_ID_6    = 6u
} button_id_t;

/* -------------------------------------------------------------------------- */
/*  버튼 이벤트 타입                                                           */
/*                                                                            */
/*  PRESS       : debounce가 끝난 "안정된 눌림 시작"                          */
/*  RELEASE     : debounce가 끝난 "안정된 떼기"                               */
/*  SHORT_PRESS : 길게 누르기 기준 시간 이전에 떼어진 짧은 눌림               */
/*  LONG_PRESS  : 길게 누르기 기준 시간을 넘긴 시점에 1회 발생                */
/* -------------------------------------------------------------------------- */
typedef enum
{
    BUTTON_EVENT_NONE        = 0u,
    BUTTON_EVENT_PRESS       = 1u,
    BUTTON_EVENT_RELEASE     = 2u,
    BUTTON_EVENT_SHORT_PRESS = 3u,
    BUTTON_EVENT_LONG_PRESS  = 4u
} button_event_type_t;

/* -------------------------------------------------------------------------- */
/*  버튼 이벤트 패킷                                                           */
/*                                                                            */
/*  id      : 어느 버튼에서 발생했는가                                         */
/*  type    : 어떤 종류의 이벤트인가                                           */
/*  tick_ms : 이벤트를 확정한 HAL tick 시각                                    */
/*  hold_ms : PRESS 외의 이벤트에서는 "눌린 총 시간" 을 담는다.               */
/* -------------------------------------------------------------------------- */
typedef struct
{
    button_id_t         id;
    button_event_type_t type;
    uint32_t            tick_ms;
    uint32_t            hold_ms;
} button_event_t;

/* -------------------------------------------------------------------------- */
/*  공개 API                                                                   */
/* -------------------------------------------------------------------------- */

/* 내부 런타임 상태와 event queue를 초기화하고,
 * 부팅 시점의 현재 버튼 안정 상태를 읽어 baseline으로 삼는다. */
void Button_Init(void);

/* main loop에서 반복 호출되는 주기 처리 함수.
 * - debounce 확정
 * - long press 시간 판정
 * - event queue 적재
 * 를 모두 여기서 수행한다. */
void Button_Task(uint32_t now_ms);

/* EXTI callback에서 호출하는 ISR 진입점.
 * 여기서는 버튼 하나의 "상태 변화 가능성" 과 debounce 만료 시각만 기록한다. */
void Button_OnExtiInterrupt(uint16_t gpio_pin);

/* event queue에서 가장 오래된 이벤트 1개를 꺼낸다.
 * - true  : out_event에 유효 이벤트가 채워짐
 * - false : 현재 꺼낼 이벤트가 없음 */
bool Button_PopEvent(button_event_t *out_event);

/* 현재 debounce까지 끝난 "안정 상태" 기준으로 눌림 여부를 읽는다. */
bool Button_IsPressed(button_id_t button_id);

/* 현재 눌린 버튼들을 bitmask로 읽는다.
 * - bit0 : BUTTON1
 * - bit1 : BUTTON2
 * - ...
 * - bit5 : BUTTON6 */
uint32_t Button_GetPressedMask(void);

/* 현재 눌리고 있는 버튼을 "135" 같은 숫자열로 만들어 준다.
 * 아무 버튼도 안 눌려 있지 않으면 "-" 를 반환 문자열로 만든다. */
void Button_BuildPressedDigits(char *out_text, size_t out_size);

/* 최근 이벤트를 사람이 보기 쉬운 짧은 문자열로 만들어 준다.
 * 예: "PRESS1", "SHORT2", "LONG6" */
void Button_BuildEventText(const button_event_t *event,
                           char *out_text,
                           size_t out_size);

/* event type만 텍스트로 보고 싶을 때 쓰는 helper */
const char *Button_GetEventTypeText(button_event_type_t type);

#ifdef __cplusplus
}
#endif

#endif /* BUTTON_H */
