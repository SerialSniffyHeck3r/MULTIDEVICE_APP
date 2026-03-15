#ifndef BACKLIGHT_APP_H
#define BACKLIGHT_APP_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  BACKLIGHT_App                                                             */
/*                                                                            */
/*  역할                                                                      */
/*  - APP_STATE.brightness의 주변광 상태를 읽는다.                             */
/*  - APP_STATE.settings.backlight의 정책을 읽는다.                           */
/*  - AUTO-CONTINUOUS / AUTO-DIMMER 규칙에 따라 target 밝기를 계산한다.       */
/*  - 최종 출력은 BACKLIGHT_DRIVER에 Q16 축으로 전달한다.                     */
/*                                                                            */
/*  이번 정리의 포인트                                                         */
/*  - 기존 스마트폰류 hold-time / delayed darken 로직을 제거했다.             */
/*  - 주변광 입력도 연속적으로 필터링하고, 출력도 연속적으로 따라가게 해서    */
/*    time-axis 계단감을 줄인다.                                              */
/*  - 내부 밝기축은 16-bit 전체 범위를 사용하고,                              */
/*    UI에 보이는 값만 0..100% 또는 1..5처럼 단순화한다.                      */
/* -------------------------------------------------------------------------- */

typedef enum
{
    BACKLIGHT_APP_ACTIVE_MODE_AUTO_CONTINUOUS = 0u,
    BACKLIGHT_APP_ACTIVE_MODE_AUTO_DIMMER     = 1u,
    BACKLIGHT_APP_ACTIVE_MODE_MANUAL_OVERRIDE = 2u
} backlight_app_active_mode_t;

typedef enum
{
    BACKLIGHT_APP_DIMMER_ZONE_DAY = 0u,
    BACKLIGHT_APP_DIMMER_ZONE_NIGHT,
    BACKLIGHT_APP_DIMMER_ZONE_SUPER_NIGHT
} backlight_app_dimmer_zone_t;

typedef struct
{
    bool     initialized;                 /* App init 완료 여부                     */
    bool     sensor_valid;                /* 신선한 주변광 샘플이 있는가            */
    bool     manual_override_enabled;     /* 강제 수동 밝기 override 활성화 여부    */

    uint8_t  active_mode;                 /* backlight_app_active_mode_t raw       */
    uint8_t  active_zone;                 /* backlight_app_dimmer_zone_t raw       */
    int8_t   active_bias_steps;           /* 현재 연산에 적용된 bias 단계           */
    uint8_t  active_smoothness;           /* 현재 연산에 적용된 smoothness 1..5     */

    uint32_t last_update_ms;              /* 마지막 task 시각                       */
    uint32_t last_sensor_update_ms;       /* 마지막 유효 센서 시각                  */

    uint16_t sensor_raw_permille;         /* APP_STATE normalized_permille snapshot */
    uint16_t sensor_filtered_permille;    /* 연속 필터 후 주변광 값                 */
    uint8_t  sensor_percent;              /* APP_STATE brightness_percent snapshot  */

    uint16_t target_linear_q16;           /* 계산된 목표 화면 밝기(Q16)             */
    uint16_t applied_linear_q16;          /* 실제 출력 중인 밝기(Q16)               */
    uint16_t manual_override_q16;         /* 강제 수동 override 값(Q16)             */

    uint8_t  target_linear_percent;       /* UI 표시용 목표 밝기(0..100)            */
    uint8_t  applied_linear_percent;      /* UI 표시용 실제 밝기(0..100)            */
} backlight_app_runtime_t;

extern volatile backlight_app_runtime_t g_backlight_app_runtime;

/* -------------------------------------------------------------------------- */
/*  공개 API                                                                   */
/* -------------------------------------------------------------------------- */

/* 부팅 시 1회. 드라이버를 먼저 살리고 fallback 밝기로 시작한다. */
void Backlight_App_Init(void);

/* main loop에서 반복 호출. APP_STATE settings + brightness를 반영한다. */
void Backlight_App_Task(uint32_t now_ms);

/* 호환용 emergency/manual API. enable=false 이면 manual override로 전환. */
void Backlight_App_SetAutoEnabled(bool enable);

/* 호환용 manual override 밝기 지정. 호출 즉시 manual override를 켠다. */
void Backlight_App_SetManualBrightnessPermille(uint16_t linear_permille);

/* 호환용 bias override. APP_STATE 설정 대신 임시로 -2..+2 bias를 강제한다. */
void Backlight_App_SetUserBiasSteps(int8_t bias_steps);

#ifdef __cplusplus
}
#endif

#endif /* BACKLIGHT_APP_H */
