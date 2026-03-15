#ifndef BACKLIGHT_DRIVER_H
#define BACKLIGHT_DRIVER_H

#include <stdbool.h>
#include <stdint.h>

#include "main.h"
#include "tim.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  BACKLIGHT_DRIVER                                                          */
/*                                                                            */
/*  목적                                                                      */
/*  - PB1에 연결된 LCD backlight PWM 출력을 전담하는 저수준 드라이버다.        */
/*  - 상위 App 계층은 "사람 눈 기준의 선형 밝기"만 넘기고,                     */
/*    실제 PWM duty 계산 / 감마 보정 / CCR 기록은 여기서 수행한다.            */
/*                                                                            */
/*  이번 정리의 핵심                                                           */
/*  - CubeMX가 이미 TIM3_CH4 / PB1 alternate function을 생성하고 있으므로      */
/*    이 드라이버는 더 이상 CHxN, 다른 AF, 다른 timer로 런타임 재정의를       */
/*    시도하지 않는다.                                                        */
/*  - 즉, "Cube가 만든 정의"와 "런타임이 덮어쓴 정의"가 서로 싸우던 구조를     */
/*    끊고, PB1 = TIM3_CH4 normal PWM 이라는 단일 진실만 사용한다.            */
/*                                                                            */
/*  CubeMX 재생성 내성                                                         */
/*  - generated tim.c / gpio.c / main.h를 직접 수정하지 않는다.               */
/*  - 아래 매크로는 현재 IOC가 만든 결과와 일치해야 한다.                     */
/*  - IOC를 다시 생성하더라도 PB1이 TIM3_CH4 PWM으로 유지되는 한,             */
/*    이 파일은 덮어써지지 않고 그대로 살아남는다.                            */
/* -------------------------------------------------------------------------- */
#ifndef BACKLIGHT_DRIVER_TIM_HANDLE
#define BACKLIGHT_DRIVER_TIM_HANDLE            htim3
#endif

#ifndef BACKLIGHT_DRIVER_TIM_CHANNEL
#define BACKLIGHT_DRIVER_TIM_CHANNEL           TIM_CHANNEL_4
#endif

#ifndef BACKLIGHT_DRIVER_OUTPUT_ACTIVE_HIGH
#define BACKLIGHT_DRIVER_OUTPUT_ACTIVE_HIGH    1u
#endif

/* -------------------------------------------------------------------------- */
/*  밝기축 / 감마 설정                                                         */
/*                                                                            */
/*  내부 밝기축은 16-bit 전체 범위(Q16, 0..65535)를 사용한다.                  */
/*  - linear_q16      : 사람이 느끼는 선형 밝기                               */
/*  - electrical_q16  : 실제 PWM duty에 대응하는 전기적 밝기                  */
/*                                                                            */
/*  LUT_RESOLUTION                                                              */
/*  - 4096+1 포인트 LUT를 사용해서 감마 보정 해상도를 충분히 높인다.          */
/*  - 기존 1% 단위 LUT보다 훨씬 촘촘하므로,                                   */
/*    밝기축 계단감이 크게 줄어든다.                                          */
/* -------------------------------------------------------------------------- */
#ifndef BACKLIGHT_DRIVER_Q16_MAX
#define BACKLIGHT_DRIVER_Q16_MAX               65535u
#endif

#ifndef BACKLIGHT_DRIVER_GAMMA_LUT_RESOLUTION
#define BACKLIGHT_DRIVER_GAMMA_LUT_RESOLUTION  4096u
#endif

#ifndef BACKLIGHT_DRIVER_GAMMA_VALUE
#define BACKLIGHT_DRIVER_GAMMA_VALUE           2.20f
#endif

/* -------------------------------------------------------------------------- */
/*  아주 낮은 duty에서 패널이 완전히 꺼져 보이는 회로 특성을 피하기 위한       */
/*  최소 전기적 duty 하한.                                                     */
/*                                                                            */
/*  규칙                                                                      */
/*  - 0 요청은 완전 소등이므로 그대로 0                                       */
/*  - 0이 아닌 요청은 필요 시 최소 duty 이상으로 올린다.                      */
/* -------------------------------------------------------------------------- */
#ifndef BACKLIGHT_DRIVER_MIN_NONZERO_ELECTRICAL_Q16
#define BACKLIGHT_DRIVER_MIN_NONZERO_ELECTRICAL_Q16  192u
#endif

typedef struct
{
    bool     initialized;              /* 드라이버 초기화 완료 여부                 */
    bool     pwm_running;              /* HAL_TIM_PWM_Start 성공 여부               */

    uint16_t requested_linear_q16;     /* 상위 계층이 요구한 눈 기준 밝기           */
    uint16_t applied_electrical_q16;   /* 감마 보정 후 실제 PWM duty 축             */

    uint32_t timer_arr;                /* 현재 TIM ARR snapshot                     */
    uint32_t last_compare_counts;      /* 마지막 CCR 기록값                         */
    uint32_t last_hal_status;          /* 마지막 HAL 반환값 raw                     */
} backlight_driver_state_t;

/* -------------------------------------------------------------------------- */
/*  디버그/설정 UI용 공개 상태                                                  */
/* -------------------------------------------------------------------------- */
extern volatile backlight_driver_state_t g_backlight_driver_state;

/* -------------------------------------------------------------------------- */
/*  공개 API                                                                   */
/* -------------------------------------------------------------------------- */

/* Cube가 생성한 TIM3_CH4 PWM을 그대로 시작한다. */
void Backlight_Driver_Init(void);

/* PWM 출력을 정지시키고 CCR를 0으로 되돌린다. */
void Backlight_Driver_DeInitToGpioLow(void);

/* 0..65535 눈 기준 선형 밝기 -> 감마 보정 -> CCR 반영 */
void Backlight_Driver_SetLinearQ16(uint16_t linear_q16);

/* 0..65535 전기적 PWM duty를 직접 CCR에 반영 */
void Backlight_Driver_SetRawPwmQ16(uint16_t electrical_q16);

/* 호환용 wrapper: 0..1000 permille 선형 밝기 입력 */
void Backlight_Driver_SetLinearPermille(uint16_t linear_permille);

/* 호환용 wrapper: 0..1000 permille raw PWM duty 입력 */
void Backlight_Driver_SetRawPwmPermille(uint16_t pwm_permille);

/* 호환용 query: permille 선형 밝기를 permille raw PWM duty로 계산 */
uint16_t Backlight_Driver_MapLinearToGammaPermille(uint16_t linear_permille);

/* 현재 PB1/TIM3_CH4 PWM이 정상 동작 가능한 상태인지 반환 */
bool Backlight_Driver_IsReady(void);

#ifdef __cplusplus
}
#endif

#endif /* BACKLIGHT_DRIVER_H */
