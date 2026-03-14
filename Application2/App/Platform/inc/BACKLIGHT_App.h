#ifndef BACKLIGHT_APP_H
#define BACKLIGHT_APP_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  BACKLIGHT_App                                                              */
/*                                                                            */
/*  목적                                                                      */
/*  - APP_STATE.brightness에 저장된 주변 조도 센서 상태를 읽는다.              */
/*  - 센서 raw/counts/정규화값에 App 단 보정값을 추가 적용한다.                */
/*  - "스마트폰 자동 밝기"와 비슷하게                                          */
/*      1) 작은 변화는 무시하고                                                */
/*      2) 의미 있는 변화가 일정 시간 유지될 때만                              */
/*         target brightness를 다시 잡고                                       */
/*      3) 실제 화면 밝기는 별도의 slew 속도로 부드럽게 따라가게 만든다.      */
/*                                                                            */
/*  계층 분리                                                                  */
/*  - 저수준 PWM, 감마, CCR 갱신: BACKLIGHT_DRIVER                              */
/*  - 정책, 히스테리시스, hold time, mapping curve, user bias: BACKLIGHT_App   */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*  사용자 환경설정용 밝기 bias 단계                                            */
/*                                                                            */
/*  추후 settings UI에서                                                        */
/*    -2 / -1 / 0 / +1 / +2                                                     */
/*  같은 단계형 옵션과 바로 연결하기 쉽게 enum으로 분리한다.                  */
/* -------------------------------------------------------------------------- */
typedef enum
{
    BACKLIGHT_USER_BIAS_LEVEL_MINUS_2 = -2,
    BACKLIGHT_USER_BIAS_LEVEL_MINUS_1 = -1,
    BACKLIGHT_USER_BIAS_LEVEL_0       =  0,
    BACKLIGHT_USER_BIAS_LEVEL_PLUS_1  =  1,
    BACKLIGHT_USER_BIAS_LEVEL_PLUS_2  =  2
} backlight_user_bias_level_t;

/* -------------------------------------------------------------------------- */
/*  센서 입력 -> 목표 화면 밝기 매핑 커브 포인트 수                            */
/*                                                                            */
/*  두 배열은 1:1로 대응한다.                                                   */
/*  예) sensor_permille[i] 가 들어오면                                          */
/*      target_linear_permille[i] 근처 밝기로 간다.                             */
/* -------------------------------------------------------------------------- */
#ifndef BACKLIGHT_APP_CURVE_POINT_COUNT
#define BACKLIGHT_APP_CURVE_POINT_COUNT  12u
#endif

typedef struct
{
    uint8_t  auto_enabled;                  /* 1이면 주변광 기반 자동 밝기 사용       */
    uint8_t  reserved0;                     /* 정렬/향후 확장                         */
    int8_t   user_bias_steps;               /* -2..+2 단계형 사용자 밝기 보정         */
    int8_t   reserved1;                     /* 정렬/향후 확장                         */

    int16_t  sensor_bias_counts;            /* calibrated count에 더하는 App 단 보정  */
    int16_t  sensor_bias_permille;          /* 정규화 후 추가로 더하는 보정           */

    uint16_t sensor_filter_alpha_permille;  /* 센서 IIR 필터 alpha                    */
    uint16_t ambient_change_threshold_up_permille;   /* 밝아질 때 판단 임계치     */
    uint16_t ambient_change_threshold_down_permille; /* 어두워질 때 판단 임계치   */
    uint16_t target_change_threshold_permille;       /* 화면 목표 밝기 최소 변화 */

    uint16_t brighten_hold_ms;              /* 밝아지는 방향 retarget hold time       */
    uint16_t darken_hold_ms;                /* 어두워지는 방향 retarget hold time     */

    uint16_t brighten_slew_permille_per_sec;/* 실제 화면이 밝아질 때 속도             */
    uint16_t darken_slew_permille_per_sec;  /* 실제 화면이 어두워질 때 속도           */

    uint16_t min_linear_permille;           /* auto/manual 공통 최소 밝기             */
    uint16_t max_linear_permille;           /* auto/manual 공통 최대 밝기             */
    uint16_t startup_linear_permille;       /* 부팅 직후 센서 전 초기 밝기            */
    uint16_t invalid_sensor_fallback_permille; /* 첫 유효 샘플 전 임시 밝기          */

    uint16_t sample_stale_timeout_ms;       /* 센서 샘플 freshness 기준                */
    uint16_t post_commit_ignore_ms;         /* retarget 직후 재판정 유예              */
} backlight_app_tuning_t;

typedef struct
{
    bool     initialized;                   /* App init 완료 여부                      */
    bool     sensor_seeded;                 /* 첫 유효 센서값으로 내부 상태를 seed함   */
    bool     sensor_valid;                  /* 이번 task 기준 유효/신선 센서 여부      */
    bool     pending_retarget;              /* 의미 있는 변화가 hold 대기 중인가       */

    uint8_t  auto_mode_active;              /* auto_enabled snapshot                    */
    int8_t   pending_direction;             /* -1: 어두워짐, +1: 밝아짐, 0: 없음       */
    int8_t   applied_user_bias_steps;       /* 이번 target 계산에 쓴 bias 단계         */

    uint32_t last_update_ms;                /* App task 마지막 실행 시각               */
    uint32_t last_sensor_sample_ms;         /* 마지막 유효 센서 샘플 시각              */
    uint32_t pending_since_ms;              /* pending retarget 시작 시각              */
    uint32_t last_target_commit_ms;         /* 마지막 target 확정 시각                 */

    uint16_t sensor_raw_counts;             /* APP_STATE에서 읽은 calibrated counts    */
    uint16_t sensor_corrected_permille;     /* App bias 후 센서 정규화값               */
    uint16_t sensor_filtered_permille;      /* IIR 필터 후 값                          */
    uint16_t committed_ambient_permille;    /* 마지막으로 brightness 판단에 채택한 값  */
    uint16_t pending_ambient_permille;      /* hold 대기 중 후보 ambient 값            */

    uint16_t candidate_linear_permille;     /* 현재 ambient로 계산한 후보 화면 밝기    */
    uint16_t pending_target_linear_permille;/* hold 대기 중 후보 화면 밝기             */
    uint16_t target_linear_permille;        /* 확정된 목표 화면 밝기                   */
    uint16_t applied_linear_permille;       /* 현재 실제 출력 중인 화면 밝기           */
    uint16_t manual_linear_permille;        /* auto off 시 수동 밝기                   */
} backlight_app_runtime_t;

/* -------------------------------------------------------------------------- */
/*  공개 전역 튜닝 저장소                                                       */
/*                                                                            */
/*  나중에 settings UI가 생기면 이 전역들을 직접 수정해도 된다.               */
/*  현재 단계에서는 compile-time default를 아래 전역 초기값에 실어 둔다.       */
/* -------------------------------------------------------------------------- */
extern volatile backlight_app_tuning_t  g_backlight_app_tuning;
extern volatile backlight_app_runtime_t g_backlight_app_runtime;

/* 사용자 bias -2..+2 단계별 실제 permille 오프셋 테이블 */
extern volatile int16_t  g_backlight_app_bias_step_offsets_permille[5];

/* 센서 입력(x축)과 목표 화면 밝기(y축) 매핑 커브 */
extern volatile uint16_t g_backlight_app_curve_sensor_permille[BACKLIGHT_APP_CURVE_POINT_COUNT];
extern volatile uint16_t g_backlight_app_curve_target_permille[BACKLIGHT_APP_CURVE_POINT_COUNT];

/* -------------------------------------------------------------------------- */
/*  공개 API                                                                   */
/* -------------------------------------------------------------------------- */

/* 드라이버 초기화 + startup brightness 적용 */
void Backlight_App_Init(void);

/* main loop에서 주기적으로 호출하는 정책 task */
void Backlight_App_Task(uint32_t now_ms);

/* auto brightness on/off */
void Backlight_App_SetAutoEnabled(bool enable);

/* auto off 상태에서 사용할 manual brightness 지정 */
void Backlight_App_SetManualBrightnessPermille(uint16_t linear_permille);

/* 사용자 bias 단계(-2..+2) 지정 */
void Backlight_App_SetUserBiasSteps(int8_t bias_steps);

#ifdef __cplusplus
}
#endif

#endif /* BACKLIGHT_APP_H */
