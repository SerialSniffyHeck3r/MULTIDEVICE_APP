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
/*  - CubeMX / IOC가 생성한 PWM 타이머 채널을 실제 LED 출력으로 사용한다.      */
/*  - 논리 LED 번호(왼쪽에서 오른쪽으로 보이는 0~10)를 실제 TIMx/CHy에 매핑한다. */
/*  - 상위 App 계층이 지정한 "사람 눈 기준 선형 밝기" 값을                   */
/*    실제 PWM duty compare 값으로 변환한다.                                  */
/*  - LED 패턴의 실제 렌더링(target frame 생성), 부드러운 전환, 최종 PWM 기록을 */
/*    이 파일이 모두 책임진다.                                                */
/*                                                                            */
/*  설계 원칙                                                                  */
/*  - 이 파일은 CubeMX가 만든 tim.c, gpio.c, main.c를 덮어쓰지 않는다.         */
/*  - 이 파일은 PWM channel start와 CCR(compare) 갱신만 수행한다.             */
/*  - PSC / ARR / pin mux 같은 timer base 설정은 IOC가 만든 값을 그대로 쓴다. */
/*  - TIM9의 다른 채널은 LED가 아닌 다른 출력이 예약될 수 있으므로,           */
/*    LED driver는 논리 LED11에 해당하는 TIM9_CH2만 사용한다.                */
/*                                                                            */
/*  밝기 축 해상도                                                             */
/*  - 상위 계층이 다루는 기본 밝기 축은 16-bit 전체 범위(0~65535)다.           */
/*  - 이 값은 "전기적 duty"가 아니라 "사람 눈에 보이는 목표 밝기"로 해석한다. */
/*  - driver 내부에서 inverse perceptual curve를 적용해 실제 duty로 바꾼다.  */
/* ========================================================================== */

#define LED_DRIVER_CHANNEL_COUNT      (11u)
#define LED_DRIVER_Q16_MAX            (65535u)
#define LED_DRIVER_PERMILLE_MAX       (1000u)

/* -------------------------------------------------------------------------- */
/*  논리 LED 인덱스                                                            */
/*                                                                            */
/*  이 인덱스는 UI 기준 왼쪽 -> 오른쪽 순서다.                                */
/*                                                                            */
/*      logical 0  = LED1  = 가장 왼쪽                                        */
/*      logical 5  = LED6  = 가운데                                            */
/*      logical 10 = LED11 = 가장 오른쪽                                       */
/*                                                                            */
/*  실제 PCB에서 어느 핀 / 어느 TIM 채널에 연결되어 있는지는                   */
/*  LED_Driver.c 내부 매핑 테이블이 책임진다.                                 */
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
/*  LED_DriverPattern_t                                                       */
/*                                                                            */
/*  app 계층은 "지금 어떤 그림을 LED에 그릴 것인지"를 이 enum으로 지정한다.  */
/*  driver는 이 값을 보고 실제 target frame을 계산한다.                      */
/*                                                                            */
/*  일반 모드                                                                  */
/*  - OFF                : 전체 소등                                           */
/*  - SOLID_ALL          : 11개 전체 동일 밝기                                 */
/*  - BREATH_CENTER      : 가운데 LED 중심의 호흡 패턴                        */
/*  - WELCOME_SWEEP      : 좌 -> 우 스윕 후 자동 복귀 없이 스윕만 수행         */
/*  - SCANNER            : 좌우 왕복 도트                                      */
/*  - ALERT_BLINK        : 전체 동시 점멸                                      */
/*  - SINGLE_DOT         : 지정한 한 점만 점등                                 */
/*  - CUSTOM_FRAME       : 상위 계층이 넘긴 11채널 프레임 그대로 사용          */
/*                                                                            */
/*  테스트 모드                                                                */
/*  - TEST_* 계열은 bring-up / 생산 검사 / 수리 후 검증을 위한 패턴이다.      */
/*  - 각 패턴은 LED 순서, 대칭, 호흡 품질, 누락 채널, ghosting 여부를          */
/*    눈으로 확인하기 쉽게 설계한다.                                          */
/* -------------------------------------------------------------------------- */
typedef enum
{
    LED_DRIVER_PATTERN_OFF = 0u,
    LED_DRIVER_PATTERN_SOLID_ALL,
    LED_DRIVER_PATTERN_BREATH_CENTER,
    LED_DRIVER_PATTERN_WELCOME_SWEEP,
    LED_DRIVER_PATTERN_SCANNER,
    LED_DRIVER_PATTERN_ALERT_BLINK,
    LED_DRIVER_PATTERN_SINGLE_DOT,
    LED_DRIVER_PATTERN_CUSTOM_FRAME,
    LED_DRIVER_PATTERN_TEST_ALL_ON,
    LED_DRIVER_PATTERN_TEST_WALK_LEFT_TO_RIGHT,
    LED_DRIVER_PATTERN_TEST_WALK_RIGHT_TO_LEFT,
    LED_DRIVER_PATTERN_TEST_SCANNER,
    LED_DRIVER_PATTERN_TEST_CENTER_OUT,
    LED_DRIVER_PATTERN_TEST_EDGE_IN,
    LED_DRIVER_PATTERN_TEST_ODD_EVEN,
    LED_DRIVER_PATTERN_TEST_HALF_SWAP,
    LED_DRIVER_PATTERN_TEST_BREATH_ALL,
    LED_DRIVER_PATTERN_TEST_BREATH_CENTER,
    LED_DRIVER_PATTERN_TEST_BAR_FILL,
    LED_DRIVER_PATTERN_TEST_STROBE_SLOW
} LED_DriverPattern_t;

/* -------------------------------------------------------------------------- */
/*  LED_DriverCommand_t                                                       */
/*                                                                            */
/*  상위 app 계층이 매 frame 전달하는 렌더링 명령이다.                         */
/*                                                                            */
/*  pattern                                                                    */
/*  - 이번 frame에서 driver가 그릴 패턴 종류                                  */
/*                                                                            */
/*  mode_started_ms                                                            */
/*  - 현재 app mode가 시작된 HAL tick(ms)                                      */
/*  - time based animation(스윕, 호흡, 왕복)의 기준 시각                       */
/*                                                                            */
/*  transition_ms                                                              */
/*  - target frame -> current frame 으로 부드럽게 따라가는 시간               */
/*  - 0 이면 즉시 target과 동일하게 출력                                      */
/*                                                                            */
/*  global_scale_q16                                                           */
/*  - 전체 프레임의 마지막 master brightness                                   */
/*  - 각 채널에 동일하게 곱해진다.                                            */
/*                                                                            */
/*  primary_level_q16                                                          */
/*  - SOLID_ALL, SINGLE_DOT 등에서 사용하는 기본 밝기                         */
/*                                                                            */
/*  logical_index                                                              */
/*  - SINGLE_DOT에서 찍을 논리 LED 위치                                        */
/*                                                                            */
/*  custom_frame_q16                                                           */
/*  - CUSTOM_FRAME에서 사용할 11채널 프레임 포인터                            */
/* -------------------------------------------------------------------------- */
typedef struct
{
    LED_DriverPattern_t pattern;
    uint32_t            mode_started_ms;
    uint16_t            transition_ms;
    uint16_t            global_scale_q16;
    uint16_t            primary_level_q16;
    uint8_t             logical_index;
    const uint16_t     *custom_frame_q16;
} LED_DriverCommand_t;

/* -------------------------------------------------------------------------- */
/*  초기화                                                                     */
/*                                                                            */
/*  호출 시점                                                                  */
/*  - 반드시 MX_TIM1_Init() ~ MX_TIM9_Init() 이후                              */
/*  - 즉, CubeMX generated timer handle이 모두 살아난 뒤 1회 호출             */
/* -------------------------------------------------------------------------- */
void LED_Driver_Init(void);

/* -------------------------------------------------------------------------- */
/*  렌더링 엔진                                                                 */
/*                                                                            */
/*  - app 계층은 main loop에서 LED_App_Task()를 부르고,                        */
/*    LED_App_Task()가 내부적으로 이 함수를 호출한다.                          */
/*  - driver는 내부적으로 frame rate를 제한하고,                              */
/*    target/current frame을 관리한 뒤 PWM compare를 갱신한다.                */
/* -------------------------------------------------------------------------- */
void LED_Driver_RenderCommand(uint32_t now_ms,
                              const LED_DriverCommand_t *command);

/* -------------------------------------------------------------------------- */
/*  즉시 직접 출력 API                                                          */
/*                                                                            */
/*  - bring-up / emergency all off / 디버그에 사용한다.                        */
/*  - 이 함수들은 perceptual remap을 포함해 실제 PWM compare를 갱신한다.      */
/* -------------------------------------------------------------------------- */
void LED_Driver_AllOff(void);
void LED_Driver_WriteLinearOneQ16(uint8_t logical_index, uint16_t brightness_q16);
void LED_Driver_WriteLinearFrameQ16(
    const uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT]);

/* -------------------------------------------------------------------------- */
/*  하위 호환용 permille API                                                    */
/*                                                                            */
/*  - 기존 코드가 0~1000 밝기 축을 쓰고 있더라도 곧바로 컴파일되게 남겨 둔다. */
/*  - 내부적으로는 16-bit full scale로 변환해 처리한다.                        */
/* -------------------------------------------------------------------------- */
void LED_Driver_WriteLinearOnePermille(uint8_t logical_index,
                                       uint16_t brightness_permille);
void LED_Driver_WriteLinearFramePermille(
    const uint16_t frame_permille[LED_DRIVER_CHANNEL_COUNT]);

/* -------------------------------------------------------------------------- */
/*  디버그 조회 API                                                             */
/* -------------------------------------------------------------------------- */
uint16_t LED_Driver_GetLastLinearQ16(uint8_t logical_index);
uint16_t LED_Driver_GetLastLinearPermille(uint8_t logical_index);
uint32_t LED_Driver_GetLastPwmCompare(uint8_t logical_index);

#ifdef __cplusplus
}
#endif

#endif /* LED_DRIVER_H */
