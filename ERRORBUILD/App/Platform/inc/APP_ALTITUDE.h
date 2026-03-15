#ifndef APP_ALTITUDE_H
#define APP_ALTITUDE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  APP_ALTITUDE                                                               */
/*                                                                            */
/*  이 모듈은                                                                 */
/*  - MS5611 pressure를 바탕으로                                              */
/*    pressure altitude / manual QNH altitude / GPS equivalent-QNH altitude   */
/*    를 모두 계산하고,                                                       */
/*  - GPS를 절대 기준(anchor)으로 사용해 drift를 천천히 잡는                  */
/*    no-IMU / IMU-assisted 2개의 병렬 필터를 동시에 유지하며,                */
/*  - variometer / relative-home altitude / grade / vario audio까지           */
/*    한 곳에서 만든다.                                                       */
/*                                                                            */
/*  핵심 철학                                                                  */
/*  1) 기압 고도와 GPS 고도는 의미가 다르므로 절대 하나로 덮어쓰지 않는다.     */
/*  2) 수동 QNH 고도와 GPS anchor 절대고도는 병렬 채널로 모두 유지한다.        */
/*  3) IMU는 자세/가속 상황에 따라 수직축 추정이 나빠질 수 있으므로             */
/*     항상 no-IMU 경로를 함께 유지한다.                                      */
/*  4) 상위 APP/UI는 APP_STATE.altitude snapshot만 읽고 사용한다.              */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*  출력 고도 모드                                                             */
/*                                                                            */
/*  output_alt_mode 는                                                        */
/*  "APP 단에서 기본적으로 사용하기를 권장하는 주 채널" 을 뜻한다.            */
/*                                                                            */
/*  FUSED_ABS / REL_HOME 모드에서는                                           */
/*  use_imu_assist 플래그에 따라                                              */
/*    - false : no-IMU 필터 채널                                               */
/*    - true  : IMU-assisted 필터 채널                                         */
/*  중 하나가 alt_output_cm 로 선택된다.                                      */
/* -------------------------------------------------------------------------- */
typedef enum
{
    APP_ALT_OUTPUT_PRESSURE_STD = 0u,
    APP_ALT_OUTPUT_QNH_MANUAL   = 1u,
    APP_ALT_OUTPUT_QNH_GPS_EQUIV= 2u,
    APP_ALT_OUTPUT_GPS_HMSL     = 3u,
    APP_ALT_OUTPUT_FUSED_ABS    = 4u,
    APP_ALT_OUTPUT_REL_HOME     = 5u,
    APP_ALT_OUTPUT_COUNT        = 6u
} app_alt_output_mode_t;

/* -------------------------------------------------------------------------- */
/*  ALTITUDE debug page에서 노출할 튜닝 필드 목록                              */
/*                                                                            */
/*  버튼 F2/F3 : 이전/다음 필드 선택                                           */
/*  버튼 F4/F5 : 현재 필드 값 감소/증가                                        */
/*  버튼 F6    : tune mode 종료                                                */
/*                                                                            */
/*  이 enum은 UI와 service가 "같은 필드 순서" 를 공유하기 위해 공개한다.       */
/* -------------------------------------------------------------------------- */
typedef enum
{
    APP_ALT_TUNE_MANUAL_QNH = 0u,
    APP_ALT_TUNE_PRESSURE_TAU,
    APP_ALT_TUNE_GPS_ALT_TAU,
    APP_ALT_TUNE_QNH_EQUIV_TAU,
    APP_ALT_TUNE_VARIO_FAST_TAU,
    APP_ALT_TUNE_VARIO_SLOW_TAU,
    APP_ALT_TUNE_BARO_SPIKE_REJECT_CM,
    APP_ALT_TUNE_BARO_NOISE_CM,
    APP_ALT_TUNE_GPS_MAX_VACC_MM,
    APP_ALT_TUNE_NOIMU_PROCESS_ACCEL_CMS2,
    APP_ALT_TUNE_IMU_PROCESS_ACCEL_CMS2,
    APP_ALT_TUNE_IMU_ATTITUDE_TAU_MS,
    APP_ALT_TUNE_IMU_ACC_TRUST_MG,
    APP_ALT_TUNE_CLIMB_DEADBAND_CMS,
    APP_ALT_TUNE_SINK_DEADBAND_CMS,
    APP_ALT_TUNE_COUNT
} app_altitude_tune_field_t;

/* -------------------------------------------------------------------------- */
/*  APP_STATE.altitude.flags 비트                                               */
/* -------------------------------------------------------------------------- */
enum
{
    APP_ALT_FLAG_BARO_VALID       = 0x01u,
    APP_ALT_FLAG_GPS_VALID        = 0x02u,
    APP_ALT_FLAG_GPS_USED         = 0x04u,
    APP_ALT_FLAG_IMU_VALID        = 0x08u,
    APP_ALT_FLAG_IMU_CONFIDENT    = 0x10u,
    APP_ALT_FLAG_HOME_VALID       = 0x20u,
    APP_ALT_FLAG_VARIO_AUDIO_EN   = 0x40u,
    APP_ALT_FLAG_PRIMARY_USES_IMU = 0x80u
};

/* -------------------------------------------------------------------------- */
/*  컴파일 타임 기본값                                                         */
/*                                                                            */
/*  사용 방법                                                                  */
/*  - IDE/빌드 옵션에서 -D 로 덮어써도 되고                                    */
/*  - 이 헤더의 define 값을 직접 바꿔도 된다.                                  */
/*  - runtime에서는 APP_STATE.settings.altitude를 바꾸면 즉시 반영된다.        */
/* -------------------------------------------------------------------------- */
#ifndef APP_ALTITUDE_DEFAULT_MANUAL_QNH_HPA_X100
#define APP_ALTITUDE_DEFAULT_MANUAL_QNH_HPA_X100       101325L
#endif

#ifndef APP_ALTITUDE_DEFAULT_OUTPUT_ALT_MODE
#define APP_ALTITUDE_DEFAULT_OUTPUT_ALT_MODE           APP_ALT_OUTPUT_FUSED_ABS
#endif

#ifndef APP_ALTITUDE_DEFAULT_USE_IMU_ASSIST
#define APP_ALTITUDE_DEFAULT_USE_IMU_ASSIST            0u
#endif

#ifndef APP_ALTITUDE_DEFAULT_VARIO_AUDIO_ENABLE
#define APP_ALTITUDE_DEFAULT_VARIO_AUDIO_ENABLE        0u
#endif

#ifndef APP_ALTITUDE_DEFAULT_PRESSURE_LPF_TAU_MS
#define APP_ALTITUDE_DEFAULT_PRESSURE_LPF_TAU_MS       120u
#endif

#ifndef APP_ALTITUDE_DEFAULT_GPS_ALT_LPF_TAU_MS
#define APP_ALTITUDE_DEFAULT_GPS_ALT_LPF_TAU_MS        800u
#endif

#ifndef APP_ALTITUDE_DEFAULT_QNH_EQUIV_LPF_TAU_MS
#define APP_ALTITUDE_DEFAULT_QNH_EQUIV_LPF_TAU_MS      4000u
#endif

#ifndef APP_ALTITUDE_DEFAULT_DISPLAY_ALT_LPF_TAU_MS
#define APP_ALTITUDE_DEFAULT_DISPLAY_ALT_LPF_TAU_MS    450u
#endif

#ifndef APP_ALTITUDE_DEFAULT_VARIO_FAST_LPF_TAU_MS
#define APP_ALTITUDE_DEFAULT_VARIO_FAST_LPF_TAU_MS     180u
#endif

#ifndef APP_ALTITUDE_DEFAULT_VARIO_SLOW_LPF_TAU_MS
#define APP_ALTITUDE_DEFAULT_VARIO_SLOW_LPF_TAU_MS     950u
#endif

#ifndef APP_ALTITUDE_DEFAULT_GPS_MIN_NUMSV
#define APP_ALTITUDE_DEFAULT_GPS_MIN_NUMSV             8u
#endif

#ifndef APP_ALTITUDE_DEFAULT_GPS_MAX_VACC_MM
#define APP_ALTITUDE_DEFAULT_GPS_MAX_VACC_MM           5000u
#endif

#ifndef APP_ALTITUDE_DEFAULT_GPS_MAX_PDOP_X100
#define APP_ALTITUDE_DEFAULT_GPS_MAX_PDOP_X100         250u
#endif

#ifndef APP_ALTITUDE_DEFAULT_GRADE_MIN_SPEED_MMPS
#define APP_ALTITUDE_DEFAULT_GRADE_MIN_SPEED_MMPS      2000u
#endif

#ifndef APP_ALTITUDE_DEFAULT_BARO_SPIKE_REJECT_CM
#define APP_ALTITUDE_DEFAULT_BARO_SPIKE_REJECT_CM      300u
#endif

#ifndef APP_ALTITUDE_DEFAULT_BARO_NOISE_CM
#define APP_ALTITUDE_DEFAULT_BARO_NOISE_CM             40u
#endif

#ifndef APP_ALTITUDE_DEFAULT_GPS_MIN_NOISE_CM
#define APP_ALTITUDE_DEFAULT_GPS_MIN_NOISE_CM          120u
#endif

#ifndef APP_ALTITUDE_DEFAULT_NOIMU_PROCESS_ACCEL_CMS2
#define APP_ALTITUDE_DEFAULT_NOIMU_PROCESS_ACCEL_CMS2  150u
#endif

#ifndef APP_ALTITUDE_DEFAULT_IMU_PROCESS_ACCEL_CMS2
#define APP_ALTITUDE_DEFAULT_IMU_PROCESS_ACCEL_CMS2    220u
#endif

#ifndef APP_ALTITUDE_DEFAULT_BARO_BIAS_WALK_CMS
#define APP_ALTITUDE_DEFAULT_BARO_BIAS_WALK_CMS        3u
#endif

#ifndef APP_ALTITUDE_DEFAULT_ACCEL_BIAS_WALK_CMS2
#define APP_ALTITUDE_DEFAULT_ACCEL_BIAS_WALK_CMS2      8u
#endif

#ifndef APP_ALTITUDE_DEFAULT_IMU_ATTITUDE_TAU_MS
#define APP_ALTITUDE_DEFAULT_IMU_ATTITUDE_TAU_MS       450u
#endif

#ifndef APP_ALTITUDE_DEFAULT_IMU_ACC_TRUST_MG
#define APP_ALTITUDE_DEFAULT_IMU_ACC_TRUST_MG          140u
#endif

#ifndef APP_ALTITUDE_DEFAULT_CLIMB_DEADBAND_CMS
#define APP_ALTITUDE_DEFAULT_CLIMB_DEADBAND_CMS        20
#endif

#ifndef APP_ALTITUDE_DEFAULT_SINK_DEADBAND_CMS
#define APP_ALTITUDE_DEFAULT_SINK_DEADBAND_CMS         30
#endif

#ifndef APP_ALTITUDE_DEFAULT_VARIO_BEEP_BASE_HZ
#define APP_ALTITUDE_DEFAULT_VARIO_BEEP_BASE_HZ        850u
#endif

#ifndef APP_ALTITUDE_DEFAULT_VARIO_BEEP_SPAN_HZ
#define APP_ALTITUDE_DEFAULT_VARIO_BEEP_SPAN_HZ        850u
#endif

#ifndef APP_ALTITUDE_DEFAULT_VARIO_BEEP_MIN_ON_MS
#define APP_ALTITUDE_DEFAULT_VARIO_BEEP_MIN_ON_MS      45u
#endif

#ifndef APP_ALTITUDE_DEFAULT_VARIO_BEEP_MAX_ON_MS
#define APP_ALTITUDE_DEFAULT_VARIO_BEEP_MAX_ON_MS      180u
#endif

#ifndef APP_ALTITUDE_DEFAULT_VARIO_BEEP_MIN_PERIOD_MS
#define APP_ALTITUDE_DEFAULT_VARIO_BEEP_MIN_PERIOD_MS  85u
#endif

#ifndef APP_ALTITUDE_DEFAULT_VARIO_BEEP_MAX_PERIOD_MS
#define APP_ALTITUDE_DEFAULT_VARIO_BEEP_MAX_PERIOD_MS  520u
#endif

/* -------------------------------------------------------------------------- */
/*  사용자 설정                                                                 */
/*                                                                            */
/*  주의                                                                      */
/*  - 이 구조체는 "런타임에 바꿀 수 있는 정책" 이다.                           */
/*  - APP_ALTITUDE.c 내부 필터 state(공분산, bias state 등)는                  */
/*    여기 넣지 않고 static runtime에 둔다.                                    */
/* -------------------------------------------------------------------------- */
typedef struct
{
    int32_t  manual_qnh_hpa_x100;       /* 수동 QNH, 0.01 hPa 고정소수점            */

    uint8_t  output_alt_mode;           /* app_alt_output_mode_t raw                */
    uint8_t  use_imu_assist;            /* 0=no-IMU primary, 1=IMU primary          */
    uint8_t  vario_audio_enable;        /* 0=off, 1=on                              */
    uint8_t  reserved0;                 /* 정렬/향후 확장용                          */

    uint16_t pressure_lpf_tau_ms;       /* pressure LPF time constant               */
    uint16_t gps_alt_lpf_tau_ms;        /* GPS altitude LPF time constant           */
    uint16_t qnh_equiv_lpf_tau_ms;      /* equivalent QNH LPF time constant         */
    uint16_t display_alt_lpf_tau_ms;    /* 화면/주출력 altitude LPF time constant   */
    uint16_t vario_fast_lpf_tau_ms;     /* 빠른 vario LPF                           */
    uint16_t vario_slow_lpf_tau_ms;     /* 느린 vario LPF                           */

    uint16_t gps_min_numsv;             /* GPS 사용 최소 위성 수                    */
    uint16_t gps_max_vacc_mm;           /* GPS 사용 최대 vAcc(mm)                   */
    uint16_t gps_max_pdop_x100;         /* GPS 사용 최대 pDOP(0.01 스케일)          */
    uint16_t grade_min_speed_mmps;      /* grade 계산 최소 수평속도(mm/s)           */

    uint16_t baro_spike_reject_cm;      /* raw baro altitude step reject threshold  */
    uint16_t baro_noise_cm;             /* baro measurement sigma                   */
    uint16_t gps_min_noise_cm;          /* GPS sigma 하한                           */
    uint16_t noimu_process_accel_cms2;  /* no-IMU filter process accel sigma        */
    uint16_t imu_process_accel_cms2;    /* IMU filter process accel sigma           */
    uint16_t baro_bias_walk_cms;        /* baro bias random walk sigma              */
    uint16_t accel_bias_walk_cms2;      /* accel bias random walk sigma             */
    uint16_t imu_attitude_tau_ms;       /* roll/pitch complementary filter tau      */
    uint16_t imu_acc_trust_mg;          /* accel trust gate around 1g               */

    int16_t  climb_deadband_cms;        /* climb beep deadband                      */
    int16_t  sink_deadband_cms;         /* sink tone deadband                       */

    uint16_t vario_beep_base_hz;        /* climb beep base frequency                */
    uint16_t vario_beep_span_hz;        /* climb beep 추가 주파수 범위              */
    uint16_t vario_beep_min_on_ms;      /* 가장 강한 climb일 때 tone on 시간        */
    uint16_t vario_beep_max_on_ms;      /* 약한 climb일 때 tone on 시간             */
    uint16_t vario_beep_min_period_ms;  /* 가장 강한 climb일 때 beep period         */
    uint16_t vario_beep_max_period_ms;  /* 약한 climb일 때 beep period              */
} app_altitude_settings_t;

/* -------------------------------------------------------------------------- */
/*  공개 altitude 상태 저장소                                                   */
/*                                                                            */
/*  단위 규칙                                                                  */
/*  - altitude : cm                                                            */
/*  - vario    : cm/s                                                          */
/*  - grade    : 0.1 %                                                         */
/*  - pressure : 0.01 hPa                                                      */
/*  - angle    : 0.01 deg                                                      */
/* -------------------------------------------------------------------------- */
typedef struct
{
    bool     initialized;               /* APP_ALTITUDE_Init 완료 여부               */
    uint8_t  flags;                     /* APP_ALT_FLAG_* bitmask                    */
    uint8_t  output_alt_mode;           /* app_alt_output_mode_t raw                 */
    uint8_t  reserved0;                 /* 정렬용                                    */

    uint32_t last_update_ms;            /* 이 slice가 마지막으로 갱신된 시각         */
    uint32_t last_baro_update_ms;       /* 마지막 baro 반영 시각                     */
    uint32_t last_gps_update_ms;        /* 마지막 GPS 반영 시각                      */
    uint32_t last_mpu_update_ms;        /* 마지막 MPU 반영 시각                      */
    uint32_t home_capture_ms;           /* home 기준점 재설정 시각                   */

    uint32_t baro_sample_count;         /* altitude 모듈이 반영한 baro 샘플 수       */
    uint32_t gps_sample_count;          /* altitude 모듈이 반영한 GPS 샘플 수        */
    uint32_t imu_sample_count;          /* altitude 모듈이 반영한 IMU 샘플 수        */

    uint32_t rejected_baro_spike_count; /* spike gate에서 버린 baro 샘플 수          */
    uint32_t rejected_gps_count;        /* residual/quality gate에서 버린 GPS 수     */
    uint32_t imu_gated_count;           /* IMU confidence gate에 막힌 횟수           */
    uint32_t home_reset_count;          /* home 기준 재설정 횟수                     */

    int32_t  pressure_hpa_x100_raw;     /* 최신 raw pressure                         */
    int32_t  pressure_hpa_x100_filt;    /* pressure LPF 결과                         */
    int32_t  temp_cdeg_baro;            /* 최신 baro temperature                     */

    int32_t  qnh_manual_hpa_x100;       /* 현재 수동 QNH                              */
    int32_t  qnh_equiv_gps_hpa_x100;    /* GPS equivalent sea-level pressure         */

    int32_t  alt_pressure_std_cm;       /* 1013.25 hPa 기준 pressure altitude        */
    int32_t  alt_qnh_manual_cm;         /* manual QNH 기준 altitude                  */
    int32_t  alt_qnh_gps_equiv_cm;      /* GPS equivalent QNH 기준 altitude          */
    int32_t  alt_gps_hmsl_cm;           /* GPS HMSL altitude                         */

    int32_t  alt_fused_noimu_cm;        /* GPS+baro 3-state filter result            */
    int32_t  alt_fused_imu_cm;          /* GPS+baro+IMU 4-state filter result        */
    int32_t  alt_output_cm;             /* settings.output_alt_mode 선택 결과         */
    int32_t  alt_display_cm;            /* 화면/주출력용 LPF 결과                    */

    int32_t  alt_rel_home_noimu_cm;     /* no-IMU relative-home                      */
    int32_t  alt_rel_home_imu_cm;       /* IMU relative-home                         */
    int32_t  alt_rel_home_output_cm;    /* 현재 주출력 relative-home                 */

    int32_t  vario_fast_noimu_cms;      /* 빠른 no-IMU variometer                    */
    int32_t  vario_slow_noimu_cms;      /* 느린 no-IMU variometer                    */
    int32_t  vario_fast_imu_cms;        /* 빠른 IMU-assisted variometer              */
    int32_t  vario_slow_imu_cms;        /* 느린 IMU-assisted variometer              */
    int32_t  vario_output_cms;          /* 현재 주출력 variometer                    */

    int16_t  grade_noimu_x10;           /* no-IMU grade, 0.1%                        */
    int16_t  grade_imu_x10;             /* IMU-assisted grade, 0.1%                  */
    int16_t  grade_output_x10;          /* 현재 주출력 grade, 0.1%                   */
    int16_t  imu_roll_cdeg;             /* 수직축 보조용 roll estimate               */
    int16_t  imu_pitch_cdeg;            /* 수직축 보조용 pitch estimate              */

    uint8_t  imu_trust_pct;             /* 현재 IMU attitude trust 0..100            */
    uint8_t  gps_weight_pct;            /* 현재 GPS update weight 0..100             */
    uint16_t reserved1;                 /* 정렬용                                    */

    int32_t  baro_bias_noimu_cm;        /* no-IMU filter estimated baro bias         */
    int32_t  baro_bias_imu_cm;          /* IMU filter estimated baro bias            */
    int32_t  accel_bias_mmps2;          /* IMU filter estimated accel bias           */

    uint16_t vario_audio_last_freq_hz;  /* 마지막으로 예약한 vario tone 주파수       */
    uint16_t vario_audio_last_on_ms;    /* 마지막 tone on 길이                        */
    uint16_t vario_audio_last_period_ms;/* 마지막 tone period                         */
    uint8_t  vario_audio_active;        /* 현재 tone on 구간인가                      */
    uint8_t  reserved2;                 /* 정렬용                                    */
} app_altitude_state_t;

/* -------------------------------------------------------------------------- */
/*  공개 API                                                                   */
/* -------------------------------------------------------------------------- */

void APP_ALTITUDE_Init(void);
void APP_ALTITUDE_Task(uint32_t now_ms);
void APP_ALTITUDE_ResetHomeNow(uint32_t now_ms);

void APP_ALTITUDE_ApplyDefaultSettings(app_altitude_settings_t *dst);
void APP_ALTITUDE_ClampSettings(app_altitude_settings_t *settings);
void APP_ALTITUDE_SetSettings(const app_altitude_settings_t *src);
void APP_ALTITUDE_CopySettings(app_altitude_settings_t *dst);

void APP_ALTITUDE_AdjustSettingField(app_altitude_settings_t *settings,
                                     app_altitude_tune_field_t field,
                                     int32_t delta_steps);
void APP_ALTITUDE_FormatTuneFieldValue(const app_altitude_settings_t *settings,
                                       app_altitude_tune_field_t field,
                                       char *out_text,
                                       size_t out_size);
const char *APP_ALTITUDE_GetTuneFieldText(app_altitude_tune_field_t field);
const char *APP_ALTITUDE_GetOutputModeText(app_alt_output_mode_t mode);

#ifdef __cplusplus
}
#endif

#endif /* APP_ALTITUDE_H */
