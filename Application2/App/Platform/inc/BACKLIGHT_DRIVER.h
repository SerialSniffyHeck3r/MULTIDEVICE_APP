#ifndef BACKLIGHT_DRIVER_H
#define BACKLIGHT_DRIVER_H

#include <stdint.h>
#include <stdbool.h>

#include "main.h"
#include "tim.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  BACKLIGHT_DRIVER                                                           */
/*                                                                            */
/*  목적                                                                      */
/*  - LCD 전체 화면의 면광원(backlight)을 PWM으로 제어하는 저수준 드라이버다.  */
/*  - 상위 App 계층은 "사람 눈 기준의 선형 밝기(linear perceived brightness)"  */
/*    만 넘기고, 실제 PWM duty 산출 / 감마 보정 / compare 레지스터 갱신은      */
/*    이 드라이버가 맡는다.                                                   */
/*                                                                            */
/*  CubeMX 재생성 내성 설계                                                    */
/*  - gpio.c / tim.c / main.h의 자동 생성 코드를 직접 수정하지 않는다.        */
/*  - 대신 런타임에 LCD_BACKLIGHT 핀을 PWM alternate function으로 다시 묶고,    */
/*    TIM 채널도 HAL API로 다시 구성한다.                                     */
/*  - 따라서 IOC를 다시 저장해도, 아래 파일 자체는 Cube가 덮어쓰지 않는다.     */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*  하드웨어 선택 매크로                                                       */
/*                                                                            */
/*  현재 프로젝트 기준 권장값                                                  */
/*  - 핀        : PB1 (이미 LCD_BACKLIGHT_Pin 으로 존재)                       */
/*  - 타이머    : TIM8                                                         */
/*  - 채널      : CH3                                                          */
/*  - 출력 타입 : CH3N (PB1은 TIM8_CH3N 대체기능을 사용)                      */
/*                                                                            */
/*  나중에 다른 보드로 옮길 때는                                               */
/*  - 핀 포트 / 핀 번호 / AF / 타이머 핸들 / 채널 / N출력 사용 여부            */
/*  만 바꾸면 된다.                                                             */
/* -------------------------------------------------------------------------- */
#ifndef BACKLIGHT_DRIVER_TIM_HANDLE
#define BACKLIGHT_DRIVER_TIM_HANDLE                 htim3
#endif

#ifndef BACKLIGHT_DRIVER_TIM_CHANNEL
#define BACKLIGHT_DRIVER_TIM_CHANNEL                TIM_CHANNEL_4
#endif

#ifndef BACKLIGHT_DRIVER_USE_COMPLEMENTARY_OUTPUT
#define BACKLIGHT_DRIVER_USE_COMPLEMENTARY_OUTPUT   1u
#endif

#ifndef BACKLIGHT_DRIVER_GPIO_PORT
#define BACKLIGHT_DRIVER_GPIO_PORT                  LCD_BACKLIGHT_GPIO_Port
#endif

#ifndef BACKLIGHT_DRIVER_GPIO_PIN
#define BACKLIGHT_DRIVER_GPIO_PIN                   LCD_BACKLIGHT_Pin
#endif

#ifndef BACKLIGHT_DRIVER_GPIO_AF
#define BACKLIGHT_DRIVER_GPIO_AF                    GPIO_AF2_TIM3
#endif

/* -------------------------------------------------------------------------- */
/*  출력 극성 설정                                                             */
/*                                                                            */
/*  1: PWM duty가 커질수록 실제 백라이트가 더 밝아지는 일반 active-high 회로   */
/*  0: active-low 구동 회로. duty 의미는 그대로 유지하되 극성만 뒤집는다.      */
/* -------------------------------------------------------------------------- */
#ifndef BACKLIGHT_DRIVER_OUTPUT_ACTIVE_HIGH
#define BACKLIGHT_DRIVER_OUTPUT_ACTIVE_HIGH          1u
#endif

/* -------------------------------------------------------------------------- */
/*  감마 및 duty 제한                                                          */
/*                                                                            */
/*  BACKLIGHT_DRIVER_GAMMA_LUT은 사람 눈으로 느끼는 밝기가 가능한 한 선형에     */
/*  가깝게 느껴지도록 PWM duty를 비선형으로 재매핑하기 위한 테이블이다.        */
/*                                                                            */
/*  MIN_DUTY_PERMILLE                                                          */
/*  - 0이 아닌 매우 작은 밝기 명령이 들어왔을 때                               */
/*    LCD 백라이트 LED가 아예 안 켜져 보이거나 깜빡이는 구간을 피하기 위한      */
/*    하한선이다.                                                              */
/*                                                                            */
/*  MAX_DUTY_PERMILLE                                                          */
/*  - 패널/전원/열 특성상 100%보다 낮게 상한을 두고 싶을 때 사용한다.          */
/* -------------------------------------------------------------------------- */
#ifndef BACKLIGHT_DRIVER_MIN_DUTY_PERMILLE
#define BACKLIGHT_DRIVER_MIN_DUTY_PERMILLE          6u
#endif

#ifndef BACKLIGHT_DRIVER_MAX_DUTY_PERMILLE
#define BACKLIGHT_DRIVER_MAX_DUTY_PERMILLE          1000u
#endif

typedef struct
{
    bool     initialized;               /* 드라이버 init 성공 여부                  */
    bool     pwm_running;               /* 실제 PWM start 성공 여부                 */

    uint16_t requested_linear_permille; /* 상위 App가 요청한 인간 눈 기준 밝기      */
    uint16_t applied_pwm_permille;      /* 감마 보정 후 실제 PWM duty               */

    uint32_t last_compare_counts;       /* 마지막 CCR에 기록한 compare 값           */
    uint32_t timer_arr;                 /* 현재 사용 중인 ARR snapshot              */
    uint32_t last_hal_status;           /* 마지막 HAL status raw                    */
} backlight_driver_state_t;

/* -------------------------------------------------------------------------- */
/*  디버그/튜닝 확인용 공개 상태                                                */
/*                                                                            */
/*  UI나 디버그 페이지에서                                                     */
/*  - 지금 드라이버가 살아 있는지                                              */
/*  - App가 요청한 밝기와 실제 PWM duty가 얼마인지                             */
/*  를 즉시 볼 수 있게 volatile 전역으로 공개한다.                             */
/* -------------------------------------------------------------------------- */
extern volatile backlight_driver_state_t g_backlight_driver_state;

/* -------------------------------------------------------------------------- */
/*  공개 API                                                                   */
/* -------------------------------------------------------------------------- */

/* PWM 핀 재구성 + TIM 채널 설정 + PWM 시작 */
void Backlight_Driver_Init(void);

/* PWM 출력을 안전하게 멈추고, 핀을 회로 기준 OFF 상태 GPIO로 되돌린다. */
void Backlight_Driver_DeInitToGpioLow(void);

/* 사람 눈 기준 선형 밝기(0~1000 permille)를 받아 감마 보정 후 적용한다. */
void Backlight_Driver_SetLinearPermille(uint16_t linear_permille);

/* 디버그/공장 시험용.
 * 감마 보정 없이 raw PWM duty(0~1000 permille)를 바로 적용한다. */
void Backlight_Driver_SetRawPwmPermille(uint16_t pwm_permille);

/* 감마 LUT를 거친 duty 값을 계산만 해서 반환한다. */
uint16_t Backlight_Driver_MapLinearToGammaPermille(uint16_t linear_permille);

/* 현재 드라이버가 init + start까지 정상 상태인지 반환한다. */
bool Backlight_Driver_IsReady(void);

#ifdef __cplusplus
}
#endif

#endif /* BACKLIGHT_DRIVER_H */
