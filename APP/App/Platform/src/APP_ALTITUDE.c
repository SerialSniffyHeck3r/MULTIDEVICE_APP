#include "APP_ALTITUDE.h"

#include "Audio_Driver.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* -------------------------------------------------------------------------- */
/*  APP_ALTITUDE tuning quick memo                                             */
/*                                                                            */
/*  이 파일은 altitude / vario / grade / debug audio를 한 번에 처리한다.      */
/*                                                                            */
/*  현장에서 가장 먼저 만질 파라미터                                           */
/*  1) 정지 화면 떨림이 거슬리면                                               */
/*     - REST_EN, REST_V, REST_A, REST_TAU, REST_HLD 를 조절한다.             */
/*     - core vario 반응성은 유지하면서 display만 더 차분하게 만든다.         */
/*                                                                            */
/*  2) IMU 모드에서 가끔 혼자 "뚜우우욱" 하고 발광하면                        */
/*     - ZUPT_EN 을 켠다.                                                      */
/*     - I_TMIN 을 올린다.                                                     */
/*     - ATT_AG 를 낮춘다.                                                     */
/*     - A_TAU 를 조금 늘린다.                                                 */
/*     - 필요하면 IMU_AID를 끄고 no-IMU 경로를 주 표시로 쓴다.                */
/*                                                                            */
/*  3) baro 숫자는 안정적인데 vario가 둔하면                                   */
/*     - VF_TAU 를 줄인다.                                                     */
/*     - BV_TAU 를 줄인다.                                                     */
/*     - BV_R 를 조금 줄여 baro velocity measurement를 더 믿게 만든다.        */
/*                                                                            */
/*  4) 주행 중 airflow로 고도가 흔들리면                                       */
/*     - BARO_R / BARO_RX 를 올린다.                                           */
/*     - 가능하면 하우징 vent / static port 구조도 같이 본다.                 */
/*                                                                            */
/*  5) GPS absolute anchor가 너무 느리거나 빠르면                              */
/*     - GBIAS_T, GPS_R, GPS_VACC, GPS_PDOP, GPS_SATS 를 조절한다.            */
/*                                                                            */
/*  오디오 정책                                                                 */
/*  - climb : beep cadence + beep 내부 FM 둘 다 fast vario에 반응한다.        */
/*  - sink  : 긴 saw tone에 약한 warble을 얹는다.                              */
/*  - AUD_SRC로 no-IMU / IMU vario를 현장에서 비교 청취할 수 있다.            */
/* -------------------------------------------------------------------------- */

#ifndef APP_ALTITUDE_STD_QNH_HPA
#define APP_ALTITUDE_STD_QNH_HPA 1013.25f
#endif

#ifndef APP_ALTITUDE_ISA_EXPONENT
#define APP_ALTITUDE_ISA_EXPONENT 0.190263f
#endif

#ifndef APP_ALTITUDE_ISA_FACTOR_M
#define APP_ALTITUDE_ISA_FACTOR_M 44330.76923f
#endif

#ifndef APP_ALTITUDE_GRAVITY_MPS2
#define APP_ALTITUDE_GRAVITY_MPS2 9.80665f
#endif

#ifndef APP_ALTITUDE_MAX_DT_S
#define APP_ALTITUDE_MAX_DT_S 0.200f
#endif

#ifndef APP_ALTITUDE_MIN_DT_S
#define APP_ALTITUDE_MIN_DT_S 0.001f
#endif

#ifndef APP_ALTITUDE_DEFAULT_CLIMB_FULL_SCALE_CMS
#define APP_ALTITUDE_DEFAULT_CLIMB_FULL_SCALE_CMS 500.0f
#endif

#ifndef APP_ALTITUDE_MIN_SPEED_FOR_GRADE_MPS
#define APP_ALTITUDE_MIN_SPEED_FOR_GRADE_MPS 1.0f
#endif

#ifndef APP_ALTITUDE_UI_ONLY_AUDIO_OWNER_TIMEOUT_MS
#define APP_ALTITUDE_UI_ONLY_AUDIO_OWNER_TIMEOUT_MS 1000u
#endif

#ifndef APP_ALTITUDE_PRESSURE_CM_PER_HPA_AT_SEA_LEVEL
#define APP_ALTITUDE_PRESSURE_CM_PER_HPA_AT_SEA_LEVEL 843.0f
#endif

#ifndef APP_ALTITUDE_BARO_ADAPTIVE_NOISE_TAU_MS
#define APP_ALTITUDE_BARO_ADAPTIVE_NOISE_TAU_MS 350u
#endif

#ifndef APP_ALTITUDE_CLIMB_AUDIO_MIN_PERIOD_MS
#define APP_ALTITUDE_CLIMB_AUDIO_MIN_PERIOD_MS 60u
#endif

#ifndef APP_ALTITUDE_CLIMB_AUDIO_MAX_PERIOD_MS
#define APP_ALTITUDE_CLIMB_AUDIO_MAX_PERIOD_MS 420u
#endif

#ifndef APP_ALTITUDE_SINK_AUDIO_MIN_GAP_MS
#define APP_ALTITUDE_SINK_AUDIO_MIN_GAP_MS 0u
#endif

#ifndef APP_ALTITUDE_SINK_AUDIO_MAX_GAP_MS
#define APP_ALTITUDE_SINK_AUDIO_MAX_GAP_MS 45u
#endif

#ifndef APP_ALTITUDE_SINK_AUDIO_MIN_TONE_MS
#define APP_ALTITUDE_SINK_AUDIO_MIN_TONE_MS 180u
#endif

#ifndef APP_ALTITUDE_SINK_AUDIO_MAX_TONE_MS
#define APP_ALTITUDE_SINK_AUDIO_MAX_TONE_MS 700u
#endif

#ifndef APP_ALTITUDE_BARO_ALTITUDE_GATE_SIGMA
#define APP_ALTITUDE_BARO_ALTITUDE_GATE_SIGMA 6.0f
#endif

#ifndef APP_ALTITUDE_BARO_ALTITUDE_GATE_FLOOR_CM
#define APP_ALTITUDE_BARO_ALTITUDE_GATE_FLOOR_CM 250.0f
#endif

#ifndef APP_ALTITUDE_BARO_VELOCITY_GATE_SIGMA
#define APP_ALTITUDE_BARO_VELOCITY_GATE_SIGMA 6.0f
#endif

#ifndef APP_ALTITUDE_BARO_VELOCITY_GATE_FLOOR_CMS
#define APP_ALTITUDE_BARO_VELOCITY_GATE_FLOOR_CMS 120.0f
#endif

#ifndef APP_ALTITUDE_GPS_ALTITUDE_GATE_SIGMA
#define APP_ALTITUDE_GPS_ALTITUDE_GATE_SIGMA 5.0f
#endif

#ifndef APP_ALTITUDE_GPS_ALTITUDE_GATE_FLOOR_CM
#define APP_ALTITUDE_GPS_ALTITUDE_GATE_FLOOR_CM 600.0f
#endif

#ifndef APP_ALTITUDE_ZUPT_VELOCITY_NOISE_CMS
#define APP_ALTITUDE_ZUPT_VELOCITY_NOISE_CMS 6.0f
#endif

#ifndef APP_ALTITUDE_BARO_VARIO_CLIP_CMS
#define APP_ALTITUDE_BARO_VARIO_CLIP_CMS 4000.0f
#endif

#ifndef APP_ALTITUDE_IMU_ACCEL_NORM_REF_MG
#define APP_ALTITUDE_IMU_ACCEL_NORM_REF_MG 1000.0f
#endif

#ifndef APP_ALTITUDE_IMU_STALE_TIMEOUT_MS
#define APP_ALTITUDE_IMU_STALE_TIMEOUT_MS 80u
#endif

#ifndef APP_ALTITUDE_IMU_ACCEL_HP_TAU_MS
#define APP_ALTITUDE_IMU_ACCEL_HP_TAU_MS 90u
#endif

#ifndef APP_ALTITUDE_IMU_VIBRATION_RMS_TAU_MS
#define APP_ALTITUDE_IMU_VIBRATION_RMS_TAU_MS 240u
#endif

#ifndef APP_ALTITUDE_IMU_VIBRATION_TRUST_FULL_MG
#define APP_ALTITUDE_IMU_VIBRATION_TRUST_FULL_MG 180.0f
#endif

#ifndef APP_ALTITUDE_IMU_GYRO_TRUST_FULL_DPS
#define APP_ALTITUDE_IMU_GYRO_TRUST_FULL_DPS 180.0f
#endif

#ifndef APP_ALTITUDE_IMU_TEMP_TRUST_FULL_C
#define APP_ALTITUDE_IMU_TEMP_TRUST_FULL_C 45.0f
#endif

#ifndef APP_ALTITUDE_IMU_MAHONY_BASE_GAIN
#define APP_ALTITUDE_IMU_MAHONY_BASE_GAIN 0.40f
#endif

#ifndef APP_ALTITUDE_IMU_MAHONY_MAX_GAIN
#define APP_ALTITUDE_IMU_MAHONY_MAX_GAIN 4.00f
#endif

#ifndef APP_ALTITUDE_IMU_GYRO_BIAS_TAU_MS
#define APP_ALTITUDE_IMU_GYRO_BIAS_TAU_MS 8000u
#endif

#ifndef APP_ALTITUDE_IMU_ACCEL_BIAS_TAU_MS
#define APP_ALTITUDE_IMU_ACCEL_BIAS_TAU_MS 10000u
#endif

#ifndef APP_ALTITUDE_IMU_ACCEL_TEMP_TAU_MS
#define APP_ALTITUDE_IMU_ACCEL_TEMP_TAU_MS 140000u
#endif

#ifndef APP_ALTITUDE_IMU_BLEND_DISAGREE_TAU_MS
#define APP_ALTITUDE_IMU_BLEND_DISAGREE_TAU_MS 380u
#endif

#ifndef APP_ALTITUDE_IMU_BLEND_FAST_DIFF_CMS
#define APP_ALTITUDE_IMU_BLEND_FAST_DIFF_CMS 420.0f
#endif

#ifndef APP_ALTITUDE_IMU_BLEND_SLOW_DIFF_CMS
#define APP_ALTITUDE_IMU_BLEND_SLOW_DIFF_CMS 260.0f
#endif

#ifndef APP_ALTITUDE_IMU_REST_ACCEL_ERROR_MG
#define APP_ALTITUDE_IMU_REST_ACCEL_ERROR_MG 20.0f
#endif

#ifndef APP_ALTITUDE_AUDIO_SEGMENT_MARGIN_MS
#define APP_ALTITUDE_AUDIO_SEGMENT_MARGIN_MS 4u
#endif

#ifndef APP_ALTITUDE_AUDIO_SEGMENT_FALLBACK_MIN_MS
#define APP_ALTITUDE_AUDIO_SEGMENT_FALLBACK_MIN_MS 24u
#endif

#ifndef APP_ALTITUDE_AUDIO_SEGMENT_CLIMB_MAX_MS
#define APP_ALTITUDE_AUDIO_SEGMENT_CLIMB_MAX_MS 52u
#endif

#ifndef APP_ALTITUDE_AUDIO_SEGMENT_SINK_MAX_MS
#define APP_ALTITUDE_AUDIO_SEGMENT_SINK_MAX_MS 88u
#endif

#ifndef APP_ALTITUDE_AUDIO_SEGMENT_WARBLE_DIVISOR
#define APP_ALTITUDE_AUDIO_SEGMENT_WARBLE_DIVISOR 4.0f
#endif

#ifndef APP_ALTITUDE_AUDIO_GATE_EXIT_SCALE
#define APP_ALTITUDE_AUDIO_GATE_EXIT_SCALE 0.75f
#endif

#ifndef APP_ALTITUDE_AUDIO_CLIMB_WARBLE_RATE_MIN_HZ
#define APP_ALTITUDE_AUDIO_CLIMB_WARBLE_RATE_MIN_HZ 5.0f
#endif

#ifndef APP_ALTITUDE_AUDIO_CLIMB_WARBLE_RATE_MAX_HZ
#define APP_ALTITUDE_AUDIO_CLIMB_WARBLE_RATE_MAX_HZ 12.0f
#endif

#ifndef APP_ALTITUDE_AUDIO_SINK_WARBLE_RATE_MIN_HZ
#define APP_ALTITUDE_AUDIO_SINK_WARBLE_RATE_MIN_HZ 2.5f
#endif

#ifndef APP_ALTITUDE_AUDIO_SINK_WARBLE_RATE_MAX_HZ
#define APP_ALTITUDE_AUDIO_SINK_WARBLE_RATE_MAX_HZ 5.0f
#endif


#ifndef APP_ALTITUDE_AUDIO_CONTROL_TAU_MS
#define APP_ALTITUDE_AUDIO_CONTROL_TAU_MS 260u
#endif

#ifndef APP_ALTITUDE_AUDIO_CONTROL_FREQ_GLIDE_MS
#define APP_ALTITUDE_AUDIO_CONTROL_FREQ_GLIDE_MS 90u
#endif

#ifndef APP_ALTITUDE_AUDIO_CONTROL_LEVEL_GLIDE_MS
#define APP_ALTITUDE_AUDIO_CONTROL_LEVEL_GLIDE_MS 36u
#endif

#ifndef APP_ALTITUDE_AUDIO_CONTROL_STOP_RELEASE_MS
#define APP_ALTITUDE_AUDIO_CONTROL_STOP_RELEASE_MS 140u
#endif

#ifndef APP_ALTITUDE_AUDIO_MODE_ENTER_HOLD_MS
#define APP_ALTITUDE_AUDIO_MODE_ENTER_HOLD_MS 80u
#endif

#ifndef APP_ALTITUDE_AUDIO_MODE_EXIT_HOLD_MS
#define APP_ALTITUDE_AUDIO_MODE_EXIT_HOLD_MS 150u
#endif

#ifndef APP_ALTITUDE_AUDIO_LEVEL_MIN_PERMILLE
#define APP_ALTITUDE_AUDIO_LEVEL_MIN_PERMILLE 620u
#endif

#ifndef APP_ALTITUDE_AUDIO_LEVEL_MAX_PERMILLE
#define APP_ALTITUDE_AUDIO_LEVEL_MAX_PERMILLE 950u
#endif

#ifndef APP_ALTITUDE_AUDIO_CLIMB_ENTER_SCALE
#define APP_ALTITUDE_AUDIO_CLIMB_ENTER_SCALE 1.30f
#endif

#ifndef APP_ALTITUDE_AUDIO_CLIMB_EXIT_SCALE
#define APP_ALTITUDE_AUDIO_CLIMB_EXIT_SCALE 0.55f
#endif

#ifndef APP_ALTITUDE_AUDIO_SINK_ENTER_SCALE
#define APP_ALTITUDE_AUDIO_SINK_ENTER_SCALE 3.40f
#endif

#ifndef APP_ALTITUDE_AUDIO_SINK_EXIT_SCALE
#define APP_ALTITUDE_AUDIO_SINK_EXIT_SCALE 2.00f
#endif

#ifndef APP_ALTITUDE_AUDIO_CLIMB_ENTER_FLOOR_CMS
#define APP_ALTITUDE_AUDIO_CLIMB_ENTER_FLOOR_CMS 45.0f
#endif

#ifndef APP_ALTITUDE_AUDIO_CLIMB_EXIT_FLOOR_CMS
#define APP_ALTITUDE_AUDIO_CLIMB_EXIT_FLOOR_CMS 18.0f
#endif

#ifndef APP_ALTITUDE_AUDIO_SINK_ENTER_FLOOR_CMS
#define APP_ALTITUDE_AUDIO_SINK_ENTER_FLOOR_CMS 120.0f
#endif

#ifndef APP_ALTITUDE_AUDIO_SINK_EXIT_FLOOR_CMS
#define APP_ALTITUDE_AUDIO_SINK_EXIT_FLOOR_CMS 70.0f
#endif

#ifndef APP_ALTITUDE_BARO_VARIO_NOISE_SPREAD_GAIN
#define APP_ALTITUDE_BARO_VARIO_NOISE_SPREAD_GAIN 0.45f
#endif

#ifndef APP_ALTITUDE_BARO_VARIO_NOISE_RESIDUAL_GAIN
#define APP_ALTITUDE_BARO_VARIO_NOISE_RESIDUAL_GAIN 0.12f
#endif

#ifndef APP_ALTITUDE_BARO_VARIO_NOISE_MAX_SCALE
#define APP_ALTITUDE_BARO_VARIO_NOISE_MAX_SCALE 4.0f
#endif

/* -------------------------------------------------------------------------- */
/*  regression-based baro velocity measurement                                */
/*                                                                            */
/*  direct diff 대신 짧은 시간창 선형회귀 slope를 velocity measurement로 쓴다. */
/*                                                                            */
/*  중요                                                                     */
/*  - altitude 표시는 "정직하지만 차분한" 쪽                                 */
/*  - vario 반응은 "빠르지만 false tone 는 적게" 쪽                          */
/*  로 동시에 가져가려면, pressure source 를 한 갈래로만 쓰는 구조보다        */
/*  altitude용 / vario용 pressure 가지를 분리하는 편이 훨씬 낫다.             */
/*                                                                            */
/*  따라서 이 파일은                                                          */
/*  1) altitude용 slow pressure LPF                                           */
/*  2) vario용 fast pressure LPF                                              */
/*  를 따로 유지하고, regression slope는 vario 전용 가지에서 계산한다.        */
/*                                                                            */
/*  창 길이도 예전 aggressive patch(7 samples, 0.07s)보다는 조금 길게 잡아    */
/*  commercial-grade 쪽의 정확도 / repeatability 를 우선한다.                 */
/* -------------------------------------------------------------------------- */
#ifndef APP_ALTITUDE_BARO_VARIO_FIT_WINDOW
#define APP_ALTITUDE_BARO_VARIO_FIT_WINDOW 9u
#endif

#ifndef APP_ALTITUDE_BARO_VARIO_MIN_SAMPLES
#define APP_ALTITUDE_BARO_VARIO_MIN_SAMPLES 5u
#endif

#ifndef APP_ALTITUDE_BARO_VARIO_MIN_SPAN_S
#define APP_ALTITUDE_BARO_VARIO_MIN_SPAN_S 0.10f
#endif

#ifndef APP_ALTITUDE_BARO_VARIO_NOISE_FIT_RMSE_GAIN
#define APP_ALTITUDE_BARO_VARIO_NOISE_FIT_RMSE_GAIN 1.15f
#endif

#ifndef APP_ALTITUDE_BARO_VELOCITY_REST_NOISE_SCALE
#define APP_ALTITUDE_BARO_VELOCITY_REST_NOISE_SCALE 2.2f
#endif

#ifndef APP_ALTITUDE_BARO_VELOCITY_NEAR_ZERO_NOISE_SCALE
#define APP_ALTITUDE_BARO_VELOCITY_NEAR_ZERO_NOISE_SCALE 1.25f
#endif

#ifndef APP_ALTITUDE_VARIO_PRESSURE_TAU_MS
#define APP_ALTITUDE_VARIO_PRESSURE_TAU_MS 35u
#endif

#ifndef APP_ALTITUDE_AUDIO_OVERRIDE_TIMEOUT_MS
#define APP_ALTITUDE_AUDIO_OVERRIDE_TIMEOUT_MS 350u
#endif

#ifndef APP_ALTITUDE_DISPLAY_PRODUCT_REST_TAU_MIN_MS
#define APP_ALTITUDE_DISPLAY_PRODUCT_REST_TAU_MIN_MS 2600u
#endif

#ifndef APP_ALTITUDE_DISPLAY_PRODUCT_SLOW_TAU_MIN_MS
#define APP_ALTITUDE_DISPLAY_PRODUCT_SLOW_TAU_MIN_MS 1100u
#endif

#ifndef APP_ALTITUDE_DISPLAY_PRODUCT_FAST_TAU_MS
#define APP_ALTITUDE_DISPLAY_PRODUCT_FAST_TAU_MS 360u
#endif

#ifndef APP_ALTITUDE_DISPLAY_PRODUCT_IDLE_VARIO_CMS
#define APP_ALTITUDE_DISPLAY_PRODUCT_IDLE_VARIO_CMS 25.0f
#endif

#ifndef APP_ALTITUDE_DISPLAY_PRODUCT_FAST_VARIO_CMS
#define APP_ALTITUDE_DISPLAY_PRODUCT_FAST_VARIO_CMS 150.0f
#endif

#ifndef APP_ALTITUDE_DISPLAY_PRODUCT_REST_HOLD_MIN_CM
#define APP_ALTITUDE_DISPLAY_PRODUCT_REST_HOLD_MIN_CM 25.0f
#endif

#ifndef APP_ALTITUDE_DISPLAY_PRODUCT_REST_STEP_CM
#define APP_ALTITUDE_DISPLAY_PRODUCT_REST_STEP_CM 50.0f
#endif

#ifndef APP_ALTITUDE_DISPLAY_PRODUCT_IDLE_STEP_CM
#define APP_ALTITUDE_DISPLAY_PRODUCT_IDLE_STEP_CM 25.0f
#endif

#ifndef APP_ALTITUDE_DISPLAY_PRODUCT_MOVE_STEP_CM
#define APP_ALTITUDE_DISPLAY_PRODUCT_MOVE_STEP_CM 10.0f
#endif

#ifndef APP_ALTITUDE_DISPLAY_PRODUCT_REST_HYST_CM
#define APP_ALTITUDE_DISPLAY_PRODUCT_REST_HYST_CM 10.0f
#endif

#ifndef APP_ALTITUDE_DISPLAY_PRODUCT_MOVE_HYST_CM
#define APP_ALTITUDE_DISPLAY_PRODUCT_MOVE_HYST_CM 6.0f
#endif

#ifndef APP_ALTITUDE_DISPLAY_PRODUCT_REACQUIRE_CM
#define APP_ALTITUDE_DISPLAY_PRODUCT_REACQUIRE_CM 300.0f
#endif

/* -------------------------------------------------------------------------- */
/*  내부 런타임 저장소                                                         */
/* -------------------------------------------------------------------------- */
typedef struct
{
    /* ---------------------------------------------------------------------- */
    /*  서비스 전체 생명주기 / 외부 요청 상태                                  */
    /* ---------------------------------------------------------------------- */
    bool initialized;                  /* init 완료 여부                          */
    bool ui_active;                    /* ALTITUDE debug page 활성 여부           */
    bool audio_owned;                  /* 현재 tone을 이 서비스가 점유 중인가     */
    bool home_capture_request;         /* UI가 home recapture를 요청했는가        */
    bool bias_rezero_request;          /* UI가 bias rezero를 요청했는가           */

    /* ---------------------------------------------------------------------- */
    /*  main task / sensor update timing                                       */
    /*                                                                        */
    /*  last_task_ms               : APP_ALTITUDE_Task() 마지막 실행 시각      */
    /*  last_audio_ms              : 마지막 tone segment 발행 시각             */
    /*  last_audio_owner_ms        : debug audio ownership 유지 시각           */
    /*  last_baro_sample_count     : 마지막으로 반영한 baro sample count       */
    /*  last_baro_timestamp_ms     : 마지막 baro timestamp                     */
    /*  last_gps_fix_update_ms     : 마지막 GPS fix update ms                  */
    /*  last_mpu_sample_count      : 마지막으로 반영한 MPU raw sample count    */
    /*  last_mpu_timestamp_ms      : 마지막 MPU timestamp                      */
    /* ---------------------------------------------------------------------- */
    uint32_t last_task_ms;
    uint32_t last_audio_ms;
    uint32_t last_audio_owner_ms;
    uint32_t audio_cycle_start_ms;
    uint32_t audio_mode_candidate_since_ms;
    uint32_t last_baro_sample_count;
    uint32_t last_baro_timestamp_ms;
    uint32_t last_gps_fix_update_ms;
    uint32_t last_mpu_sample_count;
    uint32_t last_mpu_timestamp_ms;

    int8_t   audio_mode;               /* -1: sink, 0: silent, +1: climb          */
    int8_t   audio_mode_candidate;     /* hysteresis + hold 전 후보 mode          */

    /* ---------------------------------------------------------------------- */
    /*  Pressure / altitude intermediate states                                */
    /* ---------------------------------------------------------------------- */
    float pressure_prefilt_hpa;        /* median-3 선별 후 pressure               */
    float pressure_filt_hpa;           /* altitude / display용 slow LPF pressure  */
    float pressure_vario_filt_hpa;     /* vario 전용 fast LPF pressure            */
    float pressure_residual_hpa;       /* prefilt - slow filt residual            */
    float pressure_hist_hpa[3];        /* median-3 원본 history                   */
    uint8_t pressure_hist_count;       /* history 유효 개수                       */
    uint8_t pressure_hist_index;       /* history ring index                      */

    float baro_residual_lp_cm;         /* adaptive R envelope용 residual LPF      */
    float adaptive_baro_noise_cm;      /* 현재 실제 사용된 adaptive baro noise    */
    float qnh_equiv_filt_hpa;          /* GPS anchor 기반 equivalent QNH LPF      */

    /* ---------------------------------------------------------------------- */
    /*  최종 altitude 표시 계층                                                */
    /*                                                                        */
    /*  display_alt_filt_cm    : stage-1 fused altitude LPF                    */
    /*  display_alt_follow_cm  : stage-2 제품화용 presentation LPF             */
    /*  display_alt_present_cm : UI 숫자로 실제 내보낼 최종 altitude            */
    /*  display_output_valid   : 위 3개 표시 상태의 초기화 여부                 */
    /*                                                                        */
    /*  철학                                                                    */
    /*  - core filter / fused altitude는 가능한 honest 하게 유지한다.          */
    /*  - 하지만 alt_display_cm 은 파일럿이 보는 숫자이므로                    */
    /*    상용기처럼 한 단계 더 damp / hold / hysteresis 를 준다.              */
    /* ---------------------------------------------------------------------- */
    float display_alt_filt_cm;         /* stage-1 fused altitude LPF              */
    float display_alt_follow_cm;       /* stage-2 presentation LPF                */
    float display_alt_present_cm;      /* 최종 UI altitude                        */
    bool  display_output_valid;        /* 표시 상태 초기화 완료 여부              */

    /* ---------------------------------------------------------------------- */
    /*  Alt1 baro-only display spike gate                                     */
    /*                                                                        */
    /*  Alt1은 법적 primary altitude 이므로 fused/GPS로 바꾸지 않는다.        */
    /*  대신 baro/QNH path에 순간적인 비현실 step 이 들어오면                 */
    /*  마지막으로 accept 된 barometric target 에 일시적으로 고정한다.        */
    /* ---------------------------------------------------------------------- */
    float display_baro_gate_ref_cm;    /* 마지막 accept 된 Alt1 baro target       */
    bool  display_baro_gate_valid;     /* gate reference 초기화 여부              */
    int32_t display_gate_qnh_hpa_x100; /* gate reference가 묶인 QNH basis         */
    int32_t display_gate_pressure_correction_hpa_x100; /* static corr basis   */

    /* ---------------------------------------------------------------------- */
    /*  baro velocity measurement line                                         */
    /*                                                                        */
    /*  예전에는 샘플간 단순 차분(raw diff / dt)을 썼다.                        */
    /*  이제는 최근 altitude history에 대해                                     */
    /*  짧은 창 linear regression slope를 구해                                 */
    /*  velocity measurement로 쓴다.                                           */
    /* ---------------------------------------------------------------------- */
    float baro_vario_raw_cms;          /* regression slope raw                    */
    float baro_vario_filt_cms;         /* velocity measurement용 LPF slope        */
    float baro_vario_fit_rmse_cm;      /* regression fit RMSE                     */
    float baro_vario_fit_span_s;       /* regression window actual span           */
    float baro_alt_hist_cm[APP_ALTITUDE_BARO_VARIO_FIT_WINDOW];
    uint32_t baro_alt_hist_ms[APP_ALTITUDE_BARO_VARIO_FIT_WINDOW];
    uint8_t baro_alt_hist_count;
    uint8_t baro_alt_hist_head;

    /* ---------------------------------------------------------------------- */
    /*  IMU / gravity estimation intermediates                                 */
    /*                                                                        */
    /*  gravity_est_* 는 body frame 기준 "중력 방향 unit vector" 이다.         */
    /*  기존처럼 단순 accel LPF가 아니라,                                      */
    /*  gyro 적분으로 빠른 자세 변화를 따라가고,                               */
    /*  accel norm이 1g에 가까울 때만 천천히 교정하는                          */
    /*  6-axis complementary gravity estimator 역할을 한다.                    */
    /* ---------------------------------------------------------------------- */
    bool  gravity_est_valid;           /* gravity estimator 초기화 여부           */
    float gravity_est_x;
    float gravity_est_y;
    float gravity_est_z;

    float imu_vertical_lp_cms2;        /* trust-gated vertical specific-force LPF */
    float imu_accel_norm_mg;           /* 현재 accel norm, mg                     */
    float imu_attitude_trust_permille; /* accel / vibration / gyro / temp trust   */
    float imu_predict_weight_permille; /* KF4 predict에 실제 적용된 weight        */
    float imu_blend_weight_permille;   /* 최종 fast-vario blend 진단용 weight     */
    float imu_vibration_rms_mg;        /* 고주파 진동 RMS, mg                     */
    float imu_gyro_rms_dps;            /* 고주파 각속도 envelope, dps             */
    float imu_vario_disagree_lp_cms;   /* IMU vs baro/no-IMU vario mismatch LPF   */
    float imu_temp_c;                  /* 최근 MPU 온도, degC                     */
    float imu_temp_ref_c;              /* temp compensation 기준 온도, degC       */
    bool  imu_temp_ref_valid;          /* temp reference 유효 여부                */
    float imu_quat_w;                  /* body->nav quaternion w                  */
    float imu_quat_x;                  /* body->nav quaternion x                  */
    float imu_quat_y;                  /* body->nav quaternion y                  */
    float imu_quat_z;                  /* body->nav quaternion z                  */
    float imu_gyro_bias_x_rps;         /* rest-learned gyro bias x                */
    float imu_gyro_bias_y_rps;         /* rest-learned gyro bias y                */
    float imu_gyro_bias_z_rps;         /* rest-learned gyro bias z                */
    float imu_accel_bias_x_g;          /* rest-learned accel bias x               */
    float imu_accel_bias_y_g;          /* rest-learned accel bias y               */
    float imu_accel_bias_z_g;          /* rest-learned accel bias z               */
    float imu_accel_temp_gain_x_g_per_c; /* accel bias temp slope x               */
    float imu_accel_temp_gain_y_g_per_c; /* accel bias temp slope y               */
    float imu_accel_temp_gain_z_g_per_c; /* accel bias temp slope z               */
    float imu_accel_lp_x_g;            /* vibration metric용 accel LP x           */
    float imu_accel_lp_y_g;            /* vibration metric용 accel LP y           */
    float imu_accel_lp_z_g;            /* vibration metric용 accel LP z           */

    /* ---------------------------------------------------------------------- */
    /*  Home / output smoothing states                                         */
    /* ---------------------------------------------------------------------- */
    float home_noimu_cm;               /* no-IMU home absolute altitude           */
    float home_imu_cm;                 /* IMU home absolute altitude              */

    /* ---------------------------------------------------------------------- */
    /*  audio 전용 smoothing branch                                            */
    /*                                                                        */
    /*  core filter의 fast vario를 그대로 speaker에 꽂지 않고                  */
    /*  귀에 들려줄 전용 branch를 한 번 더 둔다.                               */
    /*                                                                        */
    /*  audio_vario_smooth_cms는                                               */
    /*  - mode hysteresis 판단                                                 */
    /*  - 연속 oscillator target freq 산출                                     */
    /*  - beep cadence gate target 산출                                        */
    /*  에 공통으로 쓰이며, 화면/로그용 fast vario와는 의도적으로 분리된다.     */
    /* ---------------------------------------------------------------------- */
    float audio_vario_smooth_cms;
    bool  audio_vario_override_valid;  /* 상위 10Hz audio path override 사용중    */
    float audio_vario_override_cms;    /* override vario, cm/s                    */
    uint32_t audio_vario_override_ms;  /* override freshness timestamp            */

    float vario_fast_noimu_cms;        /* no-IMU fast display vario               */
    float vario_slow_noimu_cms;        /* no-IMU slow display vario               */
    float vario_fast_imu_cms;          /* IMU fast display vario                  */
    float vario_slow_imu_cms;          /* IMU slow display vario                  */

    bool  zupt_active;                 /* 이번 task에서 ZUPT pseudo-update 적용   */

    struct
    {
        bool  valid;
        float x[3];
        float P[3][3];
    } kf_noimu;

    struct
    {
        bool  valid;
        float x[4];
        float P[4][4];
    } kf_imu;
} app_altitude_runtime_t;

static app_altitude_runtime_t s_altitude_runtime;

/* -------------------------------------------------------------------------- */
/*  작은 수학 helper                                                           */
/* -------------------------------------------------------------------------- */
static float APP_ALTITUDE_ClampF(float value, float min_value, float max_value)
{
    if (value < min_value)
    {
        return min_value;
    }
    if (value > max_value)
    {
        return max_value;
    }
    return value;
}

static uint32_t APP_ALTITUDE_ClampU32(uint32_t value, uint32_t min_value, uint32_t max_value)
{
    if (value < min_value)
    {
        return min_value;
    }
    if (value > max_value)
    {
        return max_value;
    }
    return value;
}

static float APP_ALTITUDE_SquareF(float value)
{
    return value * value;
}

static int32_t APP_ALTITUDE_RoundFloatToS32(float value)
{
    if (value >= 0.0f)
    {
        return (int32_t)(value + 0.5f);
    }
    return (int32_t)(value - 0.5f);
}


/* -------------------------------------------------------------------------- */
/*  low-level unit bank conversion helpers                                     */
/*                                                                            */
/*  중요한 원칙                                                                */
/*  - canonical metric source(*_cm / *_cms / *_hpa_x100)는 절대 수정하지 않는다.*/
/*  - feet / fpm / inHg 는 항상 그 canonical source에서 직접 계산한다.        */
/*  - 이미 1 m 또는 0.1 m/s 로 양자화된 표시값을 다시 바꾸지 않는다.           */
/*                                                                            */
/*  이 규칙을 지켜야                                                           */
/*  - meter 표시와 feet 표시가 거짓으로 1:1 대응해 보이지 않고                */
/*  - 상위 계층이 단위 변환 책임을 되풀이하지 않으며                           */
/*  - 같은 샘플에 대한 metric / imperial bank가 항상 동기화된다.              */
/* -------------------------------------------------------------------------- */
static int32_t APP_ALTITUDE_ConvertCmToRoundedMeters(int32_t altitude_cm)
{
    return APP_ALTITUDE_RoundFloatToS32(((float)altitude_cm) * 0.01f);
}

static int32_t APP_ALTITUDE_ConvertCmToRoundedFeet(int32_t altitude_cm)
{
    return APP_ALTITUDE_RoundFloatToS32(((float)altitude_cm) * 0.032808399f);
}

static int32_t APP_ALTITUDE_ConvertCmsToRoundedMpsX10(int32_t velocity_cms)
{
    return APP_ALTITUDE_RoundFloatToS32(((float)velocity_cms) * 0.1f);
}

static int32_t APP_ALTITUDE_ConvertCmsToRoundedFpm(int32_t velocity_cms)
{
    return APP_ALTITUDE_RoundFloatToS32(((float)velocity_cms) * 1.96850394f);
}

static int32_t APP_ALTITUDE_ConvertHpaX100ToRoundedInHgX1000(int32_t pressure_hpa_x100)
{
    return APP_ALTITUDE_RoundFloatToS32(((float)pressure_hpa_x100) * 0.29529983f);
}

static void APP_ALTITUDE_FillLinearUnitPair(app_altitude_linear_units_t *dst,
                                            int32_t canonical_altitude_cm)
{
    if (dst == 0)
    {
        return;
    }

    dst->meters_rounded = APP_ALTITUDE_ConvertCmToRoundedMeters(canonical_altitude_cm);
    dst->feet_rounded   = APP_ALTITUDE_ConvertCmToRoundedFeet(canonical_altitude_cm);
}

static void APP_ALTITUDE_FillVSpeedUnitPair(app_altitude_vspeed_units_t *dst,
                                            int32_t canonical_velocity_cms)
{
    if (dst == 0)
    {
        return;
    }

    dst->mps_x10_rounded = APP_ALTITUDE_ConvertCmsToRoundedMpsX10(canonical_velocity_cms);
    dst->fpm_rounded     = APP_ALTITUDE_ConvertCmsToRoundedFpm(canonical_velocity_cms);
}

static void APP_ALTITUDE_FillPressureUnitPair(app_altitude_pressure_units_t *dst,
                                              int32_t canonical_pressure_hpa_x100)
{
    if (dst == 0)
    {
        return;
    }

    dst->hpa_x100    = canonical_pressure_hpa_x100;
    dst->inhg_x1000  = APP_ALTITUDE_ConvertHpaX100ToRoundedInHgX1000(canonical_pressure_hpa_x100);
}

static void APP_ALTITUDE_PopulateLowLevelUnitBank(app_altitude_state_t *altitude_state)
{
    if (altitude_state == 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  pressure / qnh bank                                                    */
    /*                                                                        */
    /*  source-of-truth는 canonical hPa_x100 이다.                            */
    /*  inHg 는 여기서만 direct derivation 한다.                              */
    /* ---------------------------------------------------------------------- */
    APP_ALTITUDE_FillPressureUnitPair(&altitude_state->units.pressure_raw,
                                      altitude_state->pressure_raw_hpa_x100);
    APP_ALTITUDE_FillPressureUnitPair(&altitude_state->units.pressure_prefilt,
                                      altitude_state->pressure_prefilt_hpa_x100);
    APP_ALTITUDE_FillPressureUnitPair(&altitude_state->units.pressure_filt,
                                      altitude_state->pressure_filt_hpa_x100);
    APP_ALTITUDE_FillPressureUnitPair(&altitude_state->units.pressure_residual,
                                      altitude_state->pressure_residual_hpa_x100);
    APP_ALTITUDE_FillPressureUnitPair(&altitude_state->units.qnh_manual,
                                      altitude_state->qnh_manual_hpa_x100);
    APP_ALTITUDE_FillPressureUnitPair(&altitude_state->units.qnh_equiv_gps,
                                      altitude_state->qnh_equiv_gps_hpa_x100);

    /* ---------------------------------------------------------------------- */
    /*  altitude bank                                                          */
    /*                                                                        */
    /*  feet는 항상 cm canonical source에서 direct conversion 한다.            */
    /* ---------------------------------------------------------------------- */
    APP_ALTITUDE_FillLinearUnitPair(&altitude_state->units.alt_pressure_std,
                                    altitude_state->alt_pressure_std_cm);
    APP_ALTITUDE_FillLinearUnitPair(&altitude_state->units.alt_qnh_manual,
                                    altitude_state->alt_qnh_manual_cm);
    APP_ALTITUDE_FillLinearUnitPair(&altitude_state->units.alt_gps_hmsl,
                                    altitude_state->alt_gps_hmsl_cm);
    APP_ALTITUDE_FillLinearUnitPair(&altitude_state->units.alt_fused_noimu,
                                    altitude_state->alt_fused_noimu_cm);
    APP_ALTITUDE_FillLinearUnitPair(&altitude_state->units.alt_fused_imu,
                                    altitude_state->alt_fused_imu_cm);
    APP_ALTITUDE_FillLinearUnitPair(&altitude_state->units.alt_display,
                                    altitude_state->alt_display_cm);
    APP_ALTITUDE_FillLinearUnitPair(&altitude_state->units.alt_rel_home_noimu,
                                    altitude_state->alt_rel_home_noimu_cm);
    APP_ALTITUDE_FillLinearUnitPair(&altitude_state->units.alt_rel_home_imu,
                                    altitude_state->alt_rel_home_imu_cm);
    APP_ALTITUDE_FillLinearUnitPair(&altitude_state->units.home_alt_noimu,
                                    altitude_state->home_alt_noimu_cm);
    APP_ALTITUDE_FillLinearUnitPair(&altitude_state->units.home_alt_imu,
                                    altitude_state->home_alt_imu_cm);
    APP_ALTITUDE_FillLinearUnitPair(&altitude_state->units.baro_bias_noimu,
                                    altitude_state->baro_bias_noimu_cm);
    APP_ALTITUDE_FillLinearUnitPair(&altitude_state->units.baro_bias_imu,
                                    altitude_state->baro_bias_imu_cm);

    /* ---------------------------------------------------------------------- */
    /*  vertical-speed bank                                                    */
    /* ---------------------------------------------------------------------- */
    APP_ALTITUDE_FillVSpeedUnitPair(&altitude_state->units.debug_audio_vario,
                                    altitude_state->debug_audio_vario_cms);
    APP_ALTITUDE_FillVSpeedUnitPair(&altitude_state->units.vario_fast_noimu,
                                    altitude_state->vario_fast_noimu_cms);
    APP_ALTITUDE_FillVSpeedUnitPair(&altitude_state->units.vario_slow_noimu,
                                    altitude_state->vario_slow_noimu_cms);
    APP_ALTITUDE_FillVSpeedUnitPair(&altitude_state->units.vario_fast_imu,
                                    altitude_state->vario_fast_imu_cms);
    APP_ALTITUDE_FillVSpeedUnitPair(&altitude_state->units.vario_slow_imu,
                                    altitude_state->vario_slow_imu_cms);
    APP_ALTITUDE_FillVSpeedUnitPair(&altitude_state->units.baro_vario_raw,
                                    altitude_state->baro_vario_raw_cms);
    APP_ALTITUDE_FillVSpeedUnitPair(&altitude_state->units.baro_vario_filt,
                                    altitude_state->baro_vario_filt_cms);
}

static float APP_ALTITUDE_LpfAlphaFromTauMs(uint32_t tau_ms, float dt_s)
{
    float tau_s;

    if (tau_ms == 0u)
    {
        return 1.0f;
    }

    tau_s = ((float)tau_ms) * 0.001f;
    if (tau_s <= 0.0f)
    {
        return 1.0f;
    }

    return APP_ALTITUDE_ClampF(dt_s / (tau_s + dt_s), 0.0f, 1.0f);
}

static float APP_ALTITUDE_LpfUpdate(float current, float target, uint32_t tau_ms, float dt_s)
{
    float alpha;

    alpha = APP_ALTITUDE_LpfAlphaFromTauMs(tau_ms, dt_s);
    return current + alpha * (target - current);
}

static float APP_ALTITUDE_Clamp01F(float value)
{
    return APP_ALTITUDE_ClampF(value, 0.0f, 1.0f);
}

static float APP_ALTITUDE_LerpF(float start_value, float end_value, float t)
{
    t = APP_ALTITUDE_Clamp01F(t);
    return start_value + ((end_value - start_value) * t);
}

static float APP_ALTITUDE_VectorNorm3F(float x, float y, float z)
{
    return sqrtf((x * x) + (y * y) + (z * z));
}

static bool APP_ALTITUDE_QuaternionNormalize(float *qw,
                                            float *qx,
                                            float *qy,
                                            float *qz)
{
    float norm;

    if ((qw == 0) || (qx == 0) || (qy == 0) || (qz == 0))
    {
        return false;
    }

    norm = sqrtf(((*qw) * (*qw)) + ((*qx) * (*qx)) + ((*qy) * (*qy)) + ((*qz) * (*qz)));
    if (norm < 0.000001f)
    {
        return false;
    }

    *qw /= norm;
    *qx /= norm;
    *qy /= norm;
    *qz /= norm;
    return true;
}

static void APP_ALTITUDE_QuaternionRotateBodyToNav(float qw,
                                                   float qx,
                                                   float qy,
                                                   float qz,
                                                   float bx,
                                                   float by,
                                                   float bz,
                                                   float *nx,
                                                   float *ny,
                                                   float *nz)
{
    if ((nx == 0) || (ny == 0) || (nz == 0))
    {
        return;
    }

    *nx = ((1.0f - (2.0f * ((qy * qy) + (qz * qz)))) * bx) +
          ((2.0f * ((qx * qy) - (qw * qz))) * by) +
          ((2.0f * ((qx * qz) + (qw * qy))) * bz);

    *ny = ((2.0f * ((qx * qy) + (qw * qz))) * bx) +
          ((1.0f - (2.0f * ((qx * qx) + (qz * qz)))) * by) +
          ((2.0f * ((qy * qz) - (qw * qx))) * bz);

    *nz = ((2.0f * ((qx * qz) - (qw * qy))) * bx) +
          ((2.0f * ((qy * qz) + (qw * qx))) * by) +
          ((1.0f - (2.0f * ((qx * qx) + (qy * qy)))) * bz);
}

static void APP_ALTITUDE_QuaternionRotateNavToBody(float qw,
                                                   float qx,
                                                   float qy,
                                                   float qz,
                                                   float nx,
                                                   float ny,
                                                   float nz,
                                                   float *bx,
                                                   float *by,
                                                   float *bz)
{
    APP_ALTITUDE_QuaternionRotateBodyToNav(qw,
                                           -qx,
                                           -qy,
                                           -qz,
                                           nx,
                                           ny,
                                           nz,
                                           bx,
                                           by,
                                           bz);
}

static bool APP_ALTITUDE_QuaternionInitFromAccel(float ax_unit,
                                                 float ay_unit,
                                                 float az_unit,
                                                 float *qw,
                                                 float *qx,
                                                 float *qy,
                                                 float *qz)
{
    float cross_x;
    float cross_y;
    float cross_z;
    float dot;
    float scale;

    if ((qw == 0) || (qx == 0) || (qy == 0) || (qz == 0))
    {
        return false;
    }

    dot = az_unit;
    if (dot <= -0.9999f)
    {
        *qw = 0.0f;
        *qx = 1.0f;
        *qy = 0.0f;
        *qz = 0.0f;
        return true;
    }

    cross_x = ay_unit;
    cross_y = -ax_unit;
    cross_z = 0.0f;

    scale = sqrtf((1.0f + dot) * 2.0f);
    if (scale < 0.000001f)
    {
        return false;
    }

    *qw = 0.5f * scale;
    *qx = cross_x / scale;
    *qy = cross_y / scale;
    *qz = cross_z / scale;
    return APP_ALTITUDE_QuaternionNormalize(qw, qx, qy, qz);
}

static bool APP_ALTITUDE_QuaternionIntegrateBodyRates(float *qw,
                                                      float *qx,
                                                      float *qy,
                                                      float *qz,
                                                      float wx_rps,
                                                      float wy_rps,
                                                      float wz_rps,
                                                      float dt_s)
{
    float dq_w;
    float dq_x;
    float dq_y;
    float dq_z;

    if ((qw == 0) || (qx == 0) || (qy == 0) || (qz == 0))
    {
        return false;
    }

    dq_w = -0.5f * (((*qx) * wx_rps) + ((*qy) * wy_rps) + ((*qz) * wz_rps));
    dq_x =  0.5f * (((*qw) * wx_rps) + ((*qy) * wz_rps) - ((*qz) * wy_rps));
    dq_y =  0.5f * (((*qw) * wy_rps) - ((*qx) * wz_rps) + ((*qz) * wx_rps));
    dq_z =  0.5f * (((*qw) * wz_rps) + ((*qx) * wy_rps) - ((*qy) * wx_rps));

    *qw += dq_w * dt_s;
    *qx += dq_x * dt_s;
    *qy += dq_y * dt_s;
    *qz += dq_z * dt_s;

    return APP_ALTITUDE_QuaternionNormalize(qw, qx, qy, qz);
}

static float APP_ALTITUDE_Median3F(float a, float b, float c)
{
    float tmp;

    if (a > b)
    {
        tmp = a;
        a = b;
        b = tmp;
    }

    if (b > c)
    {
        tmp = b;
        b = c;
        c = tmp;
    }

    if (a > b)
    {
        tmp = a;
        a = b;
        b = tmp;
    }

    return b;
}

static bool APP_ALTITUDE_IsResidualAccepted(float residual,
                                            float sigma,
                                            float gate_sigma,
                                            float floor_value)
{
    float limit;

    limit = fabsf(sigma) * gate_sigma;
    if (limit < floor_value)
    {
        limit = floor_value;
    }

    return (fabsf(residual) <= limit);
}

static float APP_ALTITUDE_ComputeSampleDtS(uint32_t now_ms,
                                           uint32_t last_ms,
                                           float fallback_dt_s)
{
    float dt_s;

    if ((last_ms == 0u) || (now_ms <= last_ms))
    {
        return fallback_dt_s;
    }

    dt_s = ((float)(now_ms - last_ms)) * 0.001f;
    return APP_ALTITUDE_ClampF(dt_s, APP_ALTITUDE_MIN_DT_S, APP_ALTITUDE_MAX_DT_S);
}

static float APP_ALTITUDE_UpdatePressurePrefilter(float pressure_raw_hpa)
{
    uint8_t index;
    float filtered_hpa;

    index = s_altitude_runtime.pressure_hist_index;
    if (index >= 3u)
    {
        index = 0u;
        s_altitude_runtime.pressure_hist_index = 0u;
    }

    s_altitude_runtime.pressure_hist_hpa[index] = pressure_raw_hpa;
    s_altitude_runtime.pressure_hist_index = (uint8_t)((index + 1u) % 3u);

    if (s_altitude_runtime.pressure_hist_count < 3u)
    {
        s_altitude_runtime.pressure_hist_count++;
    }

    if (s_altitude_runtime.pressure_hist_count < 3u)
    {
        filtered_hpa = pressure_raw_hpa;
    }
    else
    {
        filtered_hpa = APP_ALTITUDE_Median3F(s_altitude_runtime.pressure_hist_hpa[0],
                                             s_altitude_runtime.pressure_hist_hpa[1],
                                             s_altitude_runtime.pressure_hist_hpa[2]);
    }

    s_altitude_runtime.pressure_prefilt_hpa = filtered_hpa;
    return filtered_hpa;
}

static void APP_ALTITUDE_BaroVarioHistoryPush(float baro_alt_cm,
                                              uint32_t baro_timestamp_ms)
{
    uint8_t write_index;

    if (baro_timestamp_ms == 0u)
    {
        return;
    }

    write_index = s_altitude_runtime.baro_alt_hist_head;
    s_altitude_runtime.baro_alt_hist_cm[write_index] = baro_alt_cm;
    s_altitude_runtime.baro_alt_hist_ms[write_index] = baro_timestamp_ms;

    s_altitude_runtime.baro_alt_hist_head =
        (uint8_t)((write_index + 1u) % APP_ALTITUDE_BARO_VARIO_FIT_WINDOW);

    if (s_altitude_runtime.baro_alt_hist_count < APP_ALTITUDE_BARO_VARIO_FIT_WINDOW)
    {
        s_altitude_runtime.baro_alt_hist_count++;
    }
}

static bool APP_ALTITUDE_ComputeBaroRegressionSlope(float *out_slope_cms,
                                                    float *out_rmse_cm,
                                                    float *out_span_s)
{
    float sample_t_s[APP_ALTITUDE_BARO_VARIO_FIT_WINDOW];
    float sample_y_cm[APP_ALTITUDE_BARO_VARIO_FIT_WINDOW];
    float mean_t_s;
    float mean_y_cm;
    float numer;
    float denom;
    float span_s;
    float rmse_acc;
    uint8_t count;
    uint8_t start_index;
    uint8_t i;

    if ((out_slope_cms == 0) || (out_rmse_cm == 0) || (out_span_s == 0))
    {
        return false;
    }

    count = s_altitude_runtime.baro_alt_hist_count;
    if (count < APP_ALTITUDE_BARO_VARIO_MIN_SAMPLES)
    {
        return false;
    }

    start_index = (uint8_t)((s_altitude_runtime.baro_alt_hist_head +
                             APP_ALTITUDE_BARO_VARIO_FIT_WINDOW -
                             count) % APP_ALTITUDE_BARO_VARIO_FIT_WINDOW);

    for (i = 0u; i < count; i++)
    {
        uint8_t idx;
        uint32_t dt_ms;

        idx = (uint8_t)((start_index + i) % APP_ALTITUDE_BARO_VARIO_FIT_WINDOW);
        dt_ms = s_altitude_runtime.baro_alt_hist_ms[idx] -
                s_altitude_runtime.baro_alt_hist_ms[start_index];

        sample_t_s[i] = ((float)dt_ms) * 0.001f;
        sample_y_cm[i] = s_altitude_runtime.baro_alt_hist_cm[idx];
    }

    span_s = sample_t_s[count - 1u] - sample_t_s[0u];
    if (span_s < APP_ALTITUDE_BARO_VARIO_MIN_SPAN_S)
    {
        return false;
    }

    mean_t_s = 0.0f;
    mean_y_cm = 0.0f;
    for (i = 0u; i < count; i++)
    {
        mean_t_s += sample_t_s[i];
        mean_y_cm += sample_y_cm[i];
    }
    mean_t_s /= (float)count;
    mean_y_cm /= (float)count;

    numer = 0.0f;
    denom = 0.0f;
    for (i = 0u; i < count; i++)
    {
        float dt_centered_s;
        float dy_centered_cm;

        dt_centered_s = sample_t_s[i] - mean_t_s;
        dy_centered_cm = sample_y_cm[i] - mean_y_cm;
        numer += dt_centered_s * dy_centered_cm;
        denom += dt_centered_s * dt_centered_s;
    }

    if (denom <= 1.0e-6f)
    {
        return false;
    }

    *out_slope_cms = numer / denom;

    rmse_acc = 0.0f;
    for (i = 0u; i < count; i++)
    {
        float predicted_cm;
        float error_cm;

        predicted_cm = mean_y_cm + ((*out_slope_cms) * (sample_t_s[i] - mean_t_s));
        error_cm = sample_y_cm[i] - predicted_cm;
        rmse_acc += error_cm * error_cm;
    }

    *out_rmse_cm = sqrtf(rmse_acc / (float)count);
    *out_span_s = span_s;
    return true;
}

static float APP_ALTITUDE_UpdateBaroVarioMeasurement(const app_altitude_settings_t *settings,
                                                     float baro_alt_cm,
                                                     float baro_dt_s,
                                                     uint32_t baro_timestamp_ms)
{
    float raw_cms;
    float fit_rmse_cm;
    float fit_span_s;
    bool fit_valid;

    if (settings == 0)
    {
        return 0.0f;
    }

    APP_ALTITUDE_BaroVarioHistoryPush(baro_alt_cm, baro_timestamp_ms);

    fit_valid = APP_ALTITUDE_ComputeBaroRegressionSlope(&raw_cms,
                                                        &fit_rmse_cm,
                                                        &fit_span_s);
    if (fit_valid == false)
    {
        s_altitude_runtime.baro_vario_raw_cms = 0.0f;
        s_altitude_runtime.baro_vario_fit_rmse_cm = 0.0f;
        s_altitude_runtime.baro_vario_fit_span_s = 0.0f;
        return s_altitude_runtime.baro_vario_filt_cms;
    }

    raw_cms = APP_ALTITUDE_ClampF(raw_cms,
                                  -APP_ALTITUDE_BARO_VARIO_CLIP_CMS,
                                  APP_ALTITUDE_BARO_VARIO_CLIP_CMS);

    s_altitude_runtime.baro_vario_raw_cms = raw_cms;
    s_altitude_runtime.baro_vario_fit_rmse_cm = fit_rmse_cm;
    s_altitude_runtime.baro_vario_fit_span_s = fit_span_s;

    if (s_altitude_runtime.baro_alt_hist_count <= APP_ALTITUDE_BARO_VARIO_MIN_SAMPLES)
    {
        s_altitude_runtime.baro_vario_filt_cms = raw_cms;
    }
    else
    {
        s_altitude_runtime.baro_vario_filt_cms = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.baro_vario_filt_cms,
                                                                        raw_cms,
                                                                        settings->baro_vario_lpf_tau_ms,
                                                                        baro_dt_s);
    }

    return s_altitude_runtime.baro_vario_filt_cms;
}

/* -------------------------------------------------------------------------- */
/*  Pressure ↔ altitude 변환                                                   */
/* -------------------------------------------------------------------------- */
static float APP_ALTITUDE_PressureToAltitudeMeters(float pressure_hpa, float qnh_hpa)
{
    float ratio;

    if ((pressure_hpa <= 0.0f) || (qnh_hpa <= 0.0f))
    {
        return 0.0f;
    }

    ratio = pressure_hpa / qnh_hpa;
    ratio = APP_ALTITUDE_ClampF(ratio, 0.0001f, 4.0f);

    return APP_ALTITUDE_ISA_FACTOR_M * (1.0f - powf(ratio, APP_ALTITUDE_ISA_EXPONENT));
}

static float APP_ALTITUDE_AltitudeMetersToEquivalentQnh(float pressure_hpa, float altitude_m)
{
    float inner;

    if (pressure_hpa <= 0.0f)
    {
        return APP_ALTITUDE_STD_QNH_HPA;
    }

    altitude_m = APP_ALTITUDE_ClampF(altitude_m, -1000.0f, 15000.0f);
    inner = 1.0f - (altitude_m / APP_ALTITUDE_ISA_FACTOR_M);
    inner = APP_ALTITUDE_ClampF(inner, 0.05f, 2.0f);

    return pressure_hpa / powf(inner, (1.0f / APP_ALTITUDE_ISA_EXPONENT));
}

/* -------------------------------------------------------------------------- */
/*  GPS quality / noise helper                                                 */
/* -------------------------------------------------------------------------- */
static bool APP_ALTITUDE_IsGpsAltitudeUsable(const app_altitude_settings_t *settings,
                                             const gps_fix_basic_t *fix,
                                             uint16_t *out_quality_permille)
{
    uint16_t quality;
    uint32_t v_part;
    uint32_t p_part;
    uint8_t deficit;

    if (out_quality_permille != 0)
    {
        *out_quality_permille = 0u;
    }

    if ((settings == 0) || (fix == 0))
    {
        return false;
    }

    if ((fix->valid == false) || (fix->fixOk == false) || (fix->fixType < 3u))
    {
        return false;
    }

    if (fix->numSV_used < settings->gps_min_sats)
    {
        return false;
    }

    if ((settings->gps_max_vacc_mm > 0u) && (fix->vAcc > settings->gps_max_vacc_mm))
    {
        return false;
    }

    if ((settings->gps_max_pdop_x100 > 0u) && (fix->pDOP != 0u) && (fix->pDOP > settings->gps_max_pdop_x100))
    {
        return false;
    }

    quality = 1000u;

    if (settings->gps_max_vacc_mm > 0u)
    {
        v_part = (fix->vAcc * 700u) / settings->gps_max_vacc_mm;
        if (v_part > 700u)
        {
            v_part = 700u;
        }
        quality = (uint16_t)(quality - (uint16_t)v_part);
    }

    if (settings->gps_max_pdop_x100 > 0u)
    {
        p_part = ((uint32_t)fix->pDOP * 200u) / settings->gps_max_pdop_x100;
        if (p_part > 200u)
        {
            p_part = 200u;
        }
        quality = (uint16_t)(quality - (uint16_t)p_part);
    }

    if (fix->numSV_used < (uint8_t)(settings->gps_min_sats + 4u))
    {
        deficit = (uint8_t)((settings->gps_min_sats + 4u) - fix->numSV_used);
        if (quality > (uint16_t)(deficit * 20u))
        {
            quality = (uint16_t)(quality - (uint16_t)(deficit * 20u));
        }
        else
        {
            quality = 0u;
        }
    }

    if (out_quality_permille != 0)
    {
        *out_quality_permille = quality;
    }

    return true;
}

static float APP_ALTITUDE_ComputeGpsMeasurementNoiseCm(const app_altitude_settings_t *settings,
                                                       const gps_fix_basic_t *fix)
{
    float noise_cm;
    float pdop_scale;

    noise_cm = (float)settings->gps_measurement_noise_floor_cm;

    if (fix->vAcc != 0u)
    {
        float vacc_cm;
        vacc_cm = ((float)fix->vAcc) * 0.1f;
        if (vacc_cm > noise_cm)
        {
            noise_cm = vacc_cm;
        }
    }

    pdop_scale = 1.0f;
    if (fix->pDOP != 0u)
    {
        pdop_scale = 1.0f + (((float)fix->pDOP) * 0.0025f);
    }

    noise_cm *= pdop_scale;
    return APP_ALTITUDE_ClampF(noise_cm, 30.0f, 5000.0f);
}

static float APP_ALTITUDE_GetHorizontalSpeedMps(const gps_fix_basic_t *fix)
{
    float speed_mps;

    if (fix == 0)
    {
        return 0.0f;
    }

    speed_mps = fix->speed_llh_mps;
    if (fabsf(speed_mps) < 0.05f)
    {
        speed_mps = ((float)fix->gSpeed) * 0.001f;
    }

    return fabsf(speed_mps);
}

static int32_t APP_ALTITUDE_ComputeGradeX10(float vario_cms, const gps_fix_basic_t *fix)
{
    float horizontal_speed_mps;
    float grade_percent;

    horizontal_speed_mps = APP_ALTITUDE_GetHorizontalSpeedMps(fix);
    if (horizontal_speed_mps < APP_ALTITUDE_MIN_SPEED_FOR_GRADE_MPS)
    {
        return 0;
    }

    grade_percent = (((vario_cms) * 0.01f) / horizontal_speed_mps) * 100.0f;
    grade_percent = APP_ALTITUDE_ClampF(grade_percent, -99.9f, 99.9f);
    return APP_ALTITUDE_RoundFloatToS32(grade_percent * 10.0f);
}

/* -------------------------------------------------------------------------- */
/*  Adaptive baro noise / display rest helper                                 */
/* -------------------------------------------------------------------------- */
static float APP_ALTITUDE_ComputeAdaptiveBaroNoiseCm(const app_altitude_settings_t *settings,
                                                     float pressure_filt_hpa,
                                                     float pressure_residual_hpa,
                                                     float dt_s)
{
    float base_noise_cm;
    float max_noise_cm;
    float cm_per_hpa;
    float residual_cm;
    float adaptive_noise_cm;

    if (settings == 0)
    {
        return 0.0f;
    }

    base_noise_cm = (float)settings->baro_measurement_noise_cm;
    max_noise_cm  = (float)settings->baro_adaptive_noise_max_cm;

    /* ------------------------------------------------------------------ */
    /*  사용자가 adaptive max를 nominal 이하로 두면                         */
    /*  runtime/IDE에서 곧바로 adaptive path를 끈 것과 같은 효과가 난다.   */
    /* ------------------------------------------------------------------ */
    if (max_noise_cm <= base_noise_cm)
    {
        s_altitude_runtime.baro_residual_lp_cm = 0.0f;
        s_altitude_runtime.adaptive_baro_noise_cm = base_noise_cm;
        return base_noise_cm;
    }

    /* ------------------------------------------------------------------ */
    /*  pressure residual(hPa)를 altitude residual(cm) 규모로 환산한다.     */
    /*  sea-level 근사 843cm/hPa를 현재 pressure 비율로 약하게 보정한다.     */
    /* ------------------------------------------------------------------ */
    cm_per_hpa = APP_ALTITUDE_PRESSURE_CM_PER_HPA_AT_SEA_LEVEL *
                 (APP_ALTITUDE_STD_QNH_HPA / APP_ALTITUDE_ClampF(pressure_filt_hpa, 850.0f, 1100.0f));

    residual_cm = fabsf(pressure_residual_hpa) * cm_per_hpa;

    /* ------------------------------------------------------------------ */
    /*  residual magnitude를 짧게 LPF해서                                  */
    /*  현재 주변 난류/정압 흔들림의 envelope처럼 사용한다.                */
    /* ------------------------------------------------------------------ */
    s_altitude_runtime.baro_residual_lp_cm =
        APP_ALTITUDE_LpfUpdate(s_altitude_runtime.baro_residual_lp_cm,
                               residual_cm,
                               APP_ALTITUDE_BARO_ADAPTIVE_NOISE_TAU_MS,
                               dt_s);

    /* ------------------------------------------------------------------ */
    /*  nominal R 위에 residual envelope 기반 여유치를 더한다.             */
    /*  calm할 때는 nominal을 유지하고,                                    */
    /*  에어플로우/미세 turbulence가 커질 때만 trust를 자동으로 낮춘다.    */
    /* ------------------------------------------------------------------ */
    adaptive_noise_cm = base_noise_cm + (s_altitude_runtime.baro_residual_lp_cm * 0.90f);
    adaptive_noise_cm = APP_ALTITUDE_ClampF(adaptive_noise_cm, base_noise_cm, max_noise_cm);

    s_altitude_runtime.adaptive_baro_noise_cm = adaptive_noise_cm;
    return adaptive_noise_cm;
}

/* -------------------------------------------------------------------------- */
/*  baro velocity observation용 adaptive noise                                 */
/*                                                                            */
/*  핵심 아이디어                                                              */
/*  - altitude update용 adaptive R는 이미 pressure residual을 보고 있다.       */
/*  - 그런데 실제로 오디오와 fast vario를 가장 시끄럽게 만드는 경로는          */
/*    altitude의 시간미분으로 만든 velocity observation이다.                   */
/*                                                                            */
/*  따라서 velocity observation에도                                            */
/*  - raw derivative와 LPF derivative의 벌어짐                                */
/*  - pressure residual envelope가 보여 주는 현재 공력/노이즈 상태             */
/*  를 반영해 R를 자동으로 키워 준다.                                          */
/*                                                                            */
/*  결과                                                                      */
/*  - 정지 bench에서 ±0.1hPa 수준 흔들림이 있을 때                            */
/*    derivative 관측을 덜 믿게 되어 오디오 sign flip / 고속 잔떨림이 줄어든다. */
/* -------------------------------------------------------------------------- */
static float APP_ALTITUDE_ComputeAdaptiveBaroVarioNoiseCms(const app_altitude_settings_t *settings)
{
    float base_noise_cms;
    float max_noise_cms;
    float spread_cms;
    float residual_cms;
    float fit_rmse_cm;
    float adaptive_noise_cms;

    if (settings == 0)
    {
        return 50.0f;
    }

    base_noise_cms = (float)settings->baro_vario_measurement_noise_cms;
    if (base_noise_cms < 1.0f)
    {
        base_noise_cms = 1.0f;
    }

    spread_cms = fabsf(s_altitude_runtime.baro_vario_raw_cms -
                       s_altitude_runtime.baro_vario_filt_cms);
    residual_cms = fabsf(s_altitude_runtime.baro_residual_lp_cm);
    fit_rmse_cm = fabsf(s_altitude_runtime.baro_vario_fit_rmse_cm);

    adaptive_noise_cms = base_noise_cms +
                         (spread_cms * APP_ALTITUDE_BARO_VARIO_NOISE_SPREAD_GAIN) +
                         (residual_cms * APP_ALTITUDE_BARO_VARIO_NOISE_RESIDUAL_GAIN) +
                         (fit_rmse_cm * APP_ALTITUDE_BARO_VARIO_NOISE_FIT_RMSE_GAIN);

    if (s_altitude_runtime.baro_vario_fit_span_s < APP_ALTITUDE_BARO_VARIO_MIN_SPAN_S)
    {
        adaptive_noise_cms *= 1.25f;
    }

    max_noise_cms = base_noise_cms * APP_ALTITUDE_BARO_VARIO_NOISE_MAX_SCALE;
    adaptive_noise_cms = APP_ALTITUDE_ClampF(adaptive_noise_cms,
                                             base_noise_cms,
                                             APP_ALTITUDE_ClampF(max_noise_cms, base_noise_cms, 500.0f));

    return adaptive_noise_cms;
}

static float APP_ALTITUDE_AdjustBaroVelocityMeasurementNearRest(const app_altitude_settings_t *settings,
                                                                float baro_velocity_meas_cms,
                                                                bool rest_hint)
{
    float rest_threshold_cms;

    if (settings == 0)
    {
        return baro_velocity_meas_cms;
    }

    if (rest_hint == false)
    {
        return baro_velocity_meas_cms;
    }

    rest_threshold_cms = (float)settings->rest_detect_vario_cms;
    if (fabsf(baro_velocity_meas_cms) <= (rest_threshold_cms * 1.50f))
    {
        return 0.0f;
    }

    return baro_velocity_meas_cms * 0.85f;
}

static float APP_ALTITUDE_AdjustBaroVelocityNoiseNearRest(const app_altitude_settings_t *settings,
                                                          float baro_velocity_noise_cms,
                                                          float baro_velocity_meas_cms,
                                                          bool rest_hint)
{
    float rest_threshold_cms;

    if (settings == 0)
    {
        return baro_velocity_noise_cms;
    }

    rest_threshold_cms = (float)settings->rest_detect_vario_cms;

    if (rest_hint != false)
    {
        if (fabsf(baro_velocity_meas_cms) <= (rest_threshold_cms * 1.50f))
        {
            baro_velocity_noise_cms *= APP_ALTITUDE_BARO_VELOCITY_REST_NOISE_SCALE;
        }
        else
        {
            baro_velocity_noise_cms *= 1.80f;
        }
    }
    else if (fabsf(baro_velocity_meas_cms) <= (rest_threshold_cms * 2.00f))
    {
        baro_velocity_noise_cms *= APP_ALTITUDE_BARO_VELOCITY_NEAR_ZERO_NOISE_SCALE;
    }

    return APP_ALTITUDE_ClampF(baro_velocity_noise_cms, 1.0f, 500.0f);
}

static bool APP_ALTITUDE_IsDisplayRestActive(const app_altitude_settings_t *settings,
                                             float display_source_vario_cms,
                                             float imu_vertical_cms2,
                                             bool imu_vector_valid)
{
    float accel_abs_mg;

    if ((settings == 0) || (settings->rest_display_enabled == 0u))
    {
        return false;
    }

    if (fabsf(display_source_vario_cms) > (float)settings->rest_detect_vario_cms)
    {
        return false;
    }

    /* ------------------------------------------------------------------ */
    /*  IMU gravity vector가 유효한 상황에서는                              */
    /*  수직 specific-force까지 같이 보아,                                  */
    /*  숫자만 억지로 멈추고 실제 미세 진동/충격을 놓치는 일을 줄인다.      */
    /* ------------------------------------------------------------------ */
    if (imu_vector_valid != false)
    {
        accel_abs_mg = fabsf(imu_vertical_cms2) / (APP_ALTITUDE_GRAVITY_MPS2 * 100.0f) * 1000.0f;
        if (accel_abs_mg > (float)settings->rest_detect_accel_mg)
        {
            return false;
        }
    }

    return true;
}

static uint32_t APP_ALTITUDE_ComputeProductDisplayTauMs(const app_altitude_settings_t *settings,
                                                     float display_source_vario_cms,
                                                     bool display_rest_active)
{
    uint32_t configured_display_tau_ms;
    uint32_t configured_rest_tau_ms;
    float    abs_vario_cms;
    float    slow_tau_ms;
    float    fast_tau_ms;
    float    interp_t;
    float    tau_ms;

    configured_display_tau_ms = (settings != 0) ? settings->display_lpf_tau_ms : 450u;
    configured_rest_tau_ms    = (settings != 0) ? settings->rest_display_tau_ms : 1800u;

    slow_tau_ms = (float)APP_ALTITUDE_ClampU32(configured_display_tau_ms * 2u,
                                               APP_ALTITUDE_DISPLAY_PRODUCT_SLOW_TAU_MIN_MS,
                                               4000u);

    if (display_rest_active != false)
    {
        return APP_ALTITUDE_ClampU32(configured_rest_tau_ms + 800u,
                                     APP_ALTITUDE_DISPLAY_PRODUCT_REST_TAU_MIN_MS,
                                     6000u);
    }

    fast_tau_ms = (float)APP_ALTITUDE_DISPLAY_PRODUCT_FAST_TAU_MS;
    abs_vario_cms = fabsf(display_source_vario_cms);

    if (abs_vario_cms <= APP_ALTITUDE_DISPLAY_PRODUCT_IDLE_VARIO_CMS)
    {
        return (uint32_t)(slow_tau_ms + 0.5f);
    }

    interp_t = (abs_vario_cms - APP_ALTITUDE_DISPLAY_PRODUCT_IDLE_VARIO_CMS) /
               APP_ALTITUDE_ClampF(APP_ALTITUDE_DISPLAY_PRODUCT_FAST_VARIO_CMS - APP_ALTITUDE_DISPLAY_PRODUCT_IDLE_VARIO_CMS,
                                   1.0f,
                                   100000.0f);

    interp_t = APP_ALTITUDE_Clamp01F(interp_t);
    tau_ms   = APP_ALTITUDE_LerpF(slow_tau_ms, fast_tau_ms, interp_t);

    return APP_ALTITUDE_ClampU32((uint32_t)(tau_ms + 0.5f),
                                 APP_ALTITUDE_DISPLAY_PRODUCT_FAST_TAU_MS,
                                 (uint32_t)(slow_tau_ms + 0.5f));
}

static float APP_ALTITUDE_ComputeProductDisplayStepCm(float display_source_vario_cms,
                                                      bool display_rest_active)
{
    float abs_vario_cms;

    if (display_rest_active != false)
    {
        return APP_ALTITUDE_DISPLAY_PRODUCT_REST_STEP_CM;
    }

    abs_vario_cms = fabsf(display_source_vario_cms);

    if (abs_vario_cms <= APP_ALTITUDE_DISPLAY_PRODUCT_IDLE_VARIO_CMS)
    {
        return APP_ALTITUDE_DISPLAY_PRODUCT_IDLE_STEP_CM;
    }

    return APP_ALTITUDE_DISPLAY_PRODUCT_MOVE_STEP_CM;
}

static float APP_ALTITUDE_ComputeProductDisplayHoldCm(const app_altitude_settings_t *settings,
                                                      bool display_rest_active)
{
    float hold_cm;

    if (display_rest_active == false)
    {
        return 0.0f;
    }

    hold_cm = (settings != 0) ? ((float)settings->rest_display_hold_cm * 2.5f) : 0.0f;

    return APP_ALTITUDE_ClampF(hold_cm,
                               APP_ALTITUDE_DISPLAY_PRODUCT_REST_HOLD_MIN_CM,
                               300.0f);
}

static float APP_ALTITUDE_UpdateProductDisplayAltitudeCm(const app_altitude_settings_t *settings,
                                                         float display_target_cm,
                                                         float display_source_vario_cms,
                                                         bool display_rest_active,
                                                         float dt_s)
{
    uint32_t presentation_tau_ms;
    float    hold_cm;
    float    step_cm;
    float    hyst_cm;
    float    upper_cm;
    float    lower_cm;

    if (s_altitude_runtime.display_output_valid == false)
    {
        s_altitude_runtime.display_alt_follow_cm  = display_target_cm;
        s_altitude_runtime.display_alt_present_cm = display_target_cm;
        s_altitude_runtime.display_output_valid   = true;
        return display_target_cm;
    }

    presentation_tau_ms = APP_ALTITUDE_ComputeProductDisplayTauMs(settings,
                                                                  display_source_vario_cms,
                                                                  display_rest_active);

    s_altitude_runtime.display_alt_follow_cm = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.display_alt_follow_cm,
                                                                      display_target_cm,
                                                                      presentation_tau_ms,
                                                                      dt_s);

    hold_cm = APP_ALTITUDE_ComputeProductDisplayHoldCm(settings, display_rest_active);

    if ((display_rest_active != false) &&
        (fabsf(s_altitude_runtime.display_alt_follow_cm - s_altitude_runtime.display_alt_present_cm) <= hold_cm))
    {
        return s_altitude_runtime.display_alt_present_cm;
    }

    step_cm = APP_ALTITUDE_ComputeProductDisplayStepCm(display_source_vario_cms,
                                                       display_rest_active);

    hyst_cm = (display_rest_active != false) ? APP_ALTITUDE_DISPLAY_PRODUCT_REST_HYST_CM :
                                               APP_ALTITUDE_DISPLAY_PRODUCT_MOVE_HYST_CM;

    upper_cm = s_altitude_runtime.display_alt_present_cm + (step_cm * 0.5f) + hyst_cm;
    lower_cm = s_altitude_runtime.display_alt_present_cm - (step_cm * 0.5f) - hyst_cm;

    while (s_altitude_runtime.display_alt_follow_cm >= upper_cm)
    {
        s_altitude_runtime.display_alt_present_cm += step_cm;
        upper_cm += step_cm;
        lower_cm += step_cm;
    }

    while (s_altitude_runtime.display_alt_follow_cm <= lower_cm)
    {
        s_altitude_runtime.display_alt_present_cm -= step_cm;
        upper_cm -= step_cm;
        lower_cm -= step_cm;
    }

    if (fabsf(s_altitude_runtime.display_alt_follow_cm - s_altitude_runtime.display_alt_present_cm) >
        APP_ALTITUDE_DISPLAY_PRODUCT_REACQUIRE_CM)
    {
        s_altitude_runtime.display_alt_present_cm = s_altitude_runtime.display_alt_follow_cm;
    }

    return s_altitude_runtime.display_alt_present_cm;
}

static bool APP_ALTITUDE_IsCoreRestActive(const app_altitude_settings_t *settings,
                                          float baro_vario_filt_cms,
                                          float imu_vertical_cms2,
                                          bool imu_vector_valid,
                                          uint16_t imu_trust_permille)
{
    float accel_abs_mg;

    if ((settings == 0) || (settings->zupt_enabled == 0u))
    {
        return false;
    }

    /* ------------------------------------------------------------------ */
    /*  core rest / ZUPT는 display rest보다 조금 더 보수적으로 본다.       */
    /*  기준이 되는 속도는 core filter의 source가 아니라                   */
    /*  "baro에서 직접 만든 velocity observation" 이다.                     */
    /*  이렇게 해야 display LPF나 IMU display source 상태와                */
    /*  독립적으로 stationarity 판단이 가능하다.                           */
    /* ------------------------------------------------------------------ */
    if (fabsf(baro_vario_filt_cms) > (float)settings->rest_detect_vario_cms)
    {
        return false;
    }

    if (imu_vector_valid != false)
    {
        accel_abs_mg = fabsf(imu_vertical_cms2) / (APP_ALTITUDE_GRAVITY_MPS2 * 100.0f) * 1000.0f;

        if (imu_trust_permille < settings->imu_predict_min_trust_permille)
        {
            return false;
        }

        if (accel_abs_mg > (float)settings->rest_detect_accel_mg)
        {
            return false;
        }
    }

    return true;
}

static bool APP_ALTITUDE_IsImuInputEnabled(const app_altitude_settings_t *settings)
{
    if (settings == 0)
    {
        return false;
    }

    if (settings->ms5611_only != 0u)
    {
        return false;
    }

    if (settings->imu_poll_enabled == 0u)
    {
        return false;
    }

    return true;
}

static void APP_ALTITUDE_ResetImuRuntimeForUnavailableInput(void)
{
    /* ---------------------------------------------------------------------- */
    /*  IMU 입력이 꺼졌거나 stale sample만 남은 경우                            */
    /*                                                                        */
    /*  중요한 정책                                                            */
    /*  1) 자세/수직가속 freshness와 직접 관련된 상태만 즉시 무효화한다.        */
    /*  2) bias / temp slope 같은 장기 학습값은 그대로 남겨                    */
    /*     warm-up 이후 다시 살아났을 때 바로 도움을 주게 한다.               */
    /*  3) vario blend 가속 통로는 0으로 내려                                  */
    /*     stale accel 이 KF4 예측에 반복 주입되는 일을 막는다.               */
    /* ---------------------------------------------------------------------- */
    s_altitude_runtime.gravity_est_valid = false;
    s_altitude_runtime.gravity_est_x = 0.0f;
    s_altitude_runtime.gravity_est_y = 0.0f;
    s_altitude_runtime.gravity_est_z = 1.0f;

    s_altitude_runtime.imu_quat_w = 1.0f;
    s_altitude_runtime.imu_quat_x = 0.0f;
    s_altitude_runtime.imu_quat_y = 0.0f;
    s_altitude_runtime.imu_quat_z = 0.0f;

    s_altitude_runtime.imu_vertical_lp_cms2 = 0.0f;
    s_altitude_runtime.imu_accel_norm_mg = 0.0f;
    s_altitude_runtime.imu_attitude_trust_permille = 0.0f;
    s_altitude_runtime.imu_predict_weight_permille = 0.0f;
    s_altitude_runtime.imu_blend_weight_permille = 0.0f;
    s_altitude_runtime.imu_vario_disagree_lp_cms = 0.0f;
}

static float APP_ALTITUDE_GetDebugAudioSourceVarioCms(const app_altitude_settings_t *settings)
{
    if (s_altitude_runtime.audio_vario_override_valid != false)
    {
        if ((uint32_t)(HAL_GetTick() - s_altitude_runtime.audio_vario_override_ms) <=
            APP_ALTITUDE_AUDIO_OVERRIDE_TIMEOUT_MS)
        {
            return s_altitude_runtime.audio_vario_override_cms;
        }
    }

    if ((settings != 0) &&
        (settings->debug_audio_source != 0u) &&
        (APP_ALTITUDE_IsImuInputEnabled(settings) != false))
    {
        return s_altitude_runtime.vario_fast_imu_cms;
    }

    return s_altitude_runtime.vario_fast_noimu_cms;
}

/* -------------------------------------------------------------------------- */
/*  Kalman helper: scalar measurement update                                   */
/* -------------------------------------------------------------------------- */
static void APP_ALTITUDE_Kf3_UpdateScalar(float x[3], float P[3][3], const float H[3], float z, float R)
{
    float PHt[3];
    float K[3];
    float y;
    float S;
    float HP[3];
    float newP[3][3];
    uint32_t i;
    uint32_t j;

    for (i = 0u; i < 3u; i++)
    {
        PHt[i] = 0.0f;
        for (j = 0u; j < 3u; j++)
        {
            PHt[i] += P[i][j] * H[j];
        }
    }

    S = R;
    for (i = 0u; i < 3u; i++)
    {
        S += H[i] * PHt[i];
    }
    if (S < 1.0f)
    {
        S = 1.0f;
    }

    y = z;
    for (i = 0u; i < 3u; i++)
    {
        y -= H[i] * x[i];
        K[i] = PHt[i] / S;
    }

    for (i = 0u; i < 3u; i++)
    {
        x[i] += K[i] * y;
    }

    for (j = 0u; j < 3u; j++)
    {
        HP[j] = 0.0f;
        for (i = 0u; i < 3u; i++)
        {
            HP[j] += H[i] * P[i][j];
        }
    }

    for (i = 0u; i < 3u; i++)
    {
        for (j = 0u; j < 3u; j++)
        {
            newP[i][j] = P[i][j] - K[i] * HP[j];
        }
    }

    memcpy(P, newP, sizeof(newP));
}

static void APP_ALTITUDE_Kf4_UpdateScalar(float x[4], float P[4][4], const float H[4], float z, float R)
{
    float PHt[4];
    float K[4];
    float y;
    float S;
    float HP[4];
    float newP[4][4];
    uint32_t i;
    uint32_t j;

    for (i = 0u; i < 4u; i++)
    {
        PHt[i] = 0.0f;
        for (j = 0u; j < 4u; j++)
        {
            PHt[i] += P[i][j] * H[j];
        }
    }

    S = R;
    for (i = 0u; i < 4u; i++)
    {
        S += H[i] * PHt[i];
    }
    if (S < 1.0f)
    {
        S = 1.0f;
    }

    y = z;
    for (i = 0u; i < 4u; i++)
    {
        y -= H[i] * x[i];
        K[i] = PHt[i] / S;
    }

    for (i = 0u; i < 4u; i++)
    {
        x[i] += K[i] * y;
    }

    for (j = 0u; j < 4u; j++)
    {
        HP[j] = 0.0f;
        for (i = 0u; i < 4u; i++)
        {
            HP[j] += H[i] * P[i][j];
        }
    }

    for (i = 0u; i < 4u; i++)
    {
        for (j = 0u; j < 4u; j++)
        {
            newP[i][j] = P[i][j] - K[i] * HP[j];
        }
    }

    memcpy(P, newP, sizeof(newP));
}

/* -------------------------------------------------------------------------- */
/*  Kalman init / predict                                                      */
/* -------------------------------------------------------------------------- */
static void APP_ALTITUDE_Kf3_Init(float x[3], float P[3][3], float altitude_cm, float baro_bias_cm)
{
    memset(P, 0, sizeof(float) * 9u);
    x[0] = altitude_cm;
    x[1] = 0.0f;
    x[2] = baro_bias_cm;
    P[0][0] = 40000.0f;
    P[1][1] = 40000.0f;
    P[2][2] = 40000.0f;
}

static void APP_ALTITUDE_Kf4_Init(float x[4], float P[4][4], float altitude_cm, float baro_bias_cm)
{
    memset(P, 0, sizeof(float) * 16u);
    x[0] = altitude_cm;
    x[1] = 0.0f;
    x[2] = baro_bias_cm;
    x[3] = 0.0f;
    P[0][0] = 40000.0f;
    P[1][1] = 40000.0f;
    P[2][2] = 40000.0f;
    P[3][3] = 10000.0f;
}

static void APP_ALTITUDE_Kf3_Predict(float x[3], float P[3][3], float dt_s, const app_altitude_settings_t *settings)
{
    float F[3][3];
    float A[3][3];
    float newP[3][3];
    float qh;
    float qv;
    float qb;
    uint32_t i;
    uint32_t j;
    uint32_t k;

    x[0] += x[1] * dt_s;

    F[0][0] = 1.0f; F[0][1] = dt_s; F[0][2] = 0.0f;
    F[1][0] = 0.0f; F[1][1] = 1.0f; F[1][2] = 0.0f;
    F[2][0] = 0.0f; F[2][1] = 0.0f; F[2][2] = 1.0f;

    for (i = 0u; i < 3u; i++)
    {
        for (j = 0u; j < 3u; j++)
        {
            A[i][j] = 0.0f;
            for (k = 0u; k < 3u; k++)
            {
                A[i][j] += F[i][k] * P[k][j];
            }
        }
    }

    for (i = 0u; i < 3u; i++)
    {
        for (j = 0u; j < 3u; j++)
        {
            newP[i][j] = 0.0f;
            for (k = 0u; k < 3u; k++)
            {
                newP[i][j] += A[i][k] * F[j][k];
            }
        }
    }

    qh = ((float)settings->kf_q_height_cm_per_s) * dt_s;
    qv = ((float)settings->kf_q_velocity_cms_per_s) * dt_s;
    qb = ((float)settings->kf_q_baro_bias_cm_per_s) * dt_s;

    newP[0][0] += APP_ALTITUDE_SquareF(qh);
    newP[1][1] += APP_ALTITUDE_SquareF(qv);
    newP[2][2] += APP_ALTITUDE_SquareF(qb);

    memcpy(P, newP, sizeof(newP));
}

static void APP_ALTITUDE_Kf4_Predict(float x[4],
                                     float P[4][4],
                                     float dt_s,
                                     float accel_cms2,
                                     float accel_noise_cms2,
                                     const app_altitude_settings_t *settings)
{
    float dt2;
    float corrected_accel;
    float accel_noise_var;
    float F[4][4];
    float A[4][4];
    float newP[4][4];
    float qh;
    float qv;
    float qb;
    float qab;
    uint32_t i;
    uint32_t j;
    uint32_t k;

    dt2 = dt_s * dt_s;
    corrected_accel = accel_cms2 - x[3];

    /* ------------------------------------------------------------------ */
    /*  4-state IMU aid filter                                             */
    /*  x = [height_cm, velocity_cms, baro_bias_cm, accel_bias_cms2]       */
    /*                                                                     */
    /*  predict는 vertical specific-force를 사용해 h/v를 전파한다.         */
    /*  단, accel_noise_cms2를 별도로 받아                                  */
    /*  IMU trust가 낮을 때는 공분산을 더 크게 부풀린다.                   */
    /*  즉, "IMU를 쓰되 맹신하지 않는다" 가 핵심이다.                       */
    /* ------------------------------------------------------------------ */
    x[0] += x[1] * dt_s + (0.5f * corrected_accel * dt2);
    x[1] += corrected_accel * dt_s;

    F[0][0] = 1.0f; F[0][1] = dt_s; F[0][2] = 0.0f; F[0][3] = -0.5f * dt2;
    F[1][0] = 0.0f; F[1][1] = 1.0f; F[1][2] = 0.0f; F[1][3] = -dt_s;
    F[2][0] = 0.0f; F[2][1] = 0.0f; F[2][2] = 1.0f; F[2][3] = 0.0f;
    F[3][0] = 0.0f; F[3][1] = 0.0f; F[3][2] = 0.0f; F[3][3] = 1.0f;

    for (i = 0u; i < 4u; i++)
    {
        for (j = 0u; j < 4u; j++)
        {
            A[i][j] = 0.0f;
            for (k = 0u; k < 4u; k++)
            {
                A[i][j] += F[i][k] * P[k][j];
            }
        }
    }

    for (i = 0u; i < 4u; i++)
    {
        for (j = 0u; j < 4u; j++)
        {
            newP[i][j] = 0.0f;
            for (k = 0u; k < 4u; k++)
            {
                newP[i][j] += A[i][k] * F[j][k];
            }
        }
    }

    qh  = ((float)settings->kf_q_height_cm_per_s) * dt_s;
    qv  = ((float)settings->kf_q_velocity_cms_per_s) * dt_s;
    qb  = ((float)settings->kf_q_baro_bias_cm_per_s) * dt_s;
    qab = ((float)settings->kf_q_accel_bias_cms2_per_s) * dt_s;

    newP[0][0] += APP_ALTITUDE_SquareF(qh);
    newP[1][1] += APP_ALTITUDE_SquareF(qv);
    newP[2][2] += APP_ALTITUDE_SquareF(qb);
    newP[3][3] += APP_ALTITUDE_SquareF(qab);

    /* ------------------------------------------------------------------ */
    /*  입력 가속도의 불확실성은 h/v 공분산에 직접 전파된다.                 */
    /*  dt가 짧더라도 누적되면 큰 차이를 만들므로                            */
    /*  IMU trust가 낮은 구간일수록 accel_noise_cms2를 키워서               */
    /*  filter가 baro/GPS 쪽으로 자연스럽게 기대게 만든다.                  */
    /* ------------------------------------------------------------------ */
    accel_noise_var = APP_ALTITUDE_SquareF(accel_noise_cms2);

    newP[0][0] += 0.25f * dt2 * dt2 * accel_noise_var;
    newP[0][1] += 0.5f * dt2 * dt_s * accel_noise_var;
    newP[1][0] += 0.5f * dt2 * dt_s * accel_noise_var;
    newP[1][1] += dt_s * dt_s * accel_noise_var;

    memcpy(P, newP, sizeof(newP));
}

/* -------------------------------------------------------------------------- */
/*  IMU 수직 가속 추정                                                         */
/*                                                                            */
/*  새 엔진의 핵심                                                             */
/*  - quaternion 기반 6-axis attitude observer                                */
/*  - rest 구간에서 gyro / accel bias를 천천히 학습                           */
/*  - bias의 온도 기울기도 아주 느리게 적응                                   */
/*  - vibration / gyro activity / accel-norm / temperature를 함께 보아        */
/*    IMU trust 와 predict weight를 연속값으로 산출                           */
/*                                                                            */
/*  설계 철학                                                                 */
/*  1) INS는 fast response를 주는 보조 엔진이다.                               */
/*  2) baro/GPS는 long-term truth anchor 다.                                   */
/*  3) 따라서 여기서는 "좋은 IMU면 과감하게 빠르게",                          */
/*     "나쁜 IMU면 조용히 물러나기" 를 구현한다.                               */
/* -------------------------------------------------------------------------- */
static float APP_ALTITUDE_UpdateImuVerticalAccelCms2(const app_altitude_settings_t *settings,
                                                     const app_gy86_mpu_raw_t *mpu,
                                                     uint32_t now_ms,
                                                     float task_dt_s,
                                                     bool *out_vector_valid)
{
    float imu_dt_s;
    float accel_lsb_per_g;
    float gyro_lsb_per_dps;
    float ax_raw_g;
    float ay_raw_g;
    float az_raw_g;
    float ax_g;
    float ay_g;
    float az_g;
    float gx_rad_s;
    float gy_rad_s;
    float gz_rad_s;
    float accel_norm_g;
    float accel_norm_mg;
    float accel_error_mg;
    float a_hat_x;
    float a_hat_y;
    float a_hat_z;
    float gravity_body_x;
    float gravity_body_y;
    float gravity_body_z;
    float vib_res_x;
    float vib_res_y;
    float vib_res_z;
    float vib_mg;
    float gyro_norm_dps;
    float trust_acc;
    float trust_vib_raw;
    float trust_gyro_raw;
    float trust_temp_raw;
    float trust_vib;
    float trust_gyro;
    float trust_temp;
    float attitude_trust;
    float mahony_gain;
    float err_x;
    float err_y;
    float err_z;
    float corrected_gx_rad_s;
    float corrected_gy_rad_s;
    float corrected_gz_rad_s;
    float nav_ax_g;
    float nav_ay_g;
    float nav_az_g;
    float vertical_dyn_g;
    float deadband_g;
    float clip_g;
    float sign_f;
    float predict_weight;
    float min_trust;
    float temp_c;
    float delta_temp_c;
    float rest_vertical_mg;
    bool  rest_candidate;

    if (out_vector_valid != 0)
    {
        *out_vector_valid = false;
    }

    if ((settings == 0) || (mpu == 0) || (settings->imu_accel_lsb_per_g == 0u))
    {
        return 0.0f;
    }

    if (APP_ALTITUDE_IsImuInputEnabled(settings) == false)
    {
        APP_ALTITUDE_ResetImuRuntimeForUnavailableInput();
        return 0.0f;
    }

    if ((mpu->sample_count == 0u) ||
        (mpu->timestamp_ms == 0u) ||
        ((uint32_t)(now_ms - mpu->timestamp_ms) > APP_ALTITUDE_IMU_STALE_TIMEOUT_MS))
    {
        APP_ALTITUDE_ResetImuRuntimeForUnavailableInput();
        return 0.0f;
    }

    if ((mpu->sample_count != 0u) && (mpu->sample_count == s_altitude_runtime.last_mpu_sample_count))
    {
        if (out_vector_valid != 0)
        {
            *out_vector_valid = s_altitude_runtime.gravity_est_valid;
        }

        return s_altitude_runtime.imu_vertical_lp_cms2;
    }

    imu_dt_s = task_dt_s;
    if ((mpu->timestamp_ms != 0u) &&
        (s_altitude_runtime.last_mpu_timestamp_ms != 0u) &&
        (mpu->timestamp_ms > s_altitude_runtime.last_mpu_timestamp_ms))
    {
        imu_dt_s = APP_ALTITUDE_ComputeSampleDtS(mpu->timestamp_ms,
                                                 s_altitude_runtime.last_mpu_timestamp_ms,
                                                 task_dt_s);
    }

    s_altitude_runtime.last_mpu_sample_count = mpu->sample_count;
    s_altitude_runtime.last_mpu_timestamp_ms = mpu->timestamp_ms;

    accel_lsb_per_g = (float)settings->imu_accel_lsb_per_g;
    gyro_lsb_per_dps = (settings->imu_gyro_lsb_per_dps > 0u) ?
                       (float)settings->imu_gyro_lsb_per_dps :
                       131.0f;

    temp_c = ((float)mpu->temp_cdeg) * 0.01f;
    s_altitude_runtime.imu_temp_c = temp_c;
    if (s_altitude_runtime.imu_temp_ref_valid == false)
    {
        s_altitude_runtime.imu_temp_ref_c = temp_c;
        s_altitude_runtime.imu_temp_ref_valid = true;
    }
    delta_temp_c = temp_c - s_altitude_runtime.imu_temp_ref_c;

    ax_raw_g = ((float)mpu->accel_x_raw) / accel_lsb_per_g;
    ay_raw_g = ((float)mpu->accel_y_raw) / accel_lsb_per_g;
    az_raw_g = ((float)mpu->accel_z_raw) / accel_lsb_per_g;

    ax_g = ax_raw_g - (s_altitude_runtime.imu_accel_bias_x_g +
                       (s_altitude_runtime.imu_accel_temp_gain_x_g_per_c * delta_temp_c));
    ay_g = ay_raw_g - (s_altitude_runtime.imu_accel_bias_y_g +
                       (s_altitude_runtime.imu_accel_temp_gain_y_g_per_c * delta_temp_c));
    az_g = az_raw_g - (s_altitude_runtime.imu_accel_bias_z_g +
                       (s_altitude_runtime.imu_accel_temp_gain_z_g_per_c * delta_temp_c));

    gx_rad_s = ((((float)mpu->gyro_x_raw) / gyro_lsb_per_dps) * ((float)M_PI / 180.0f));
    gy_rad_s = ((((float)mpu->gyro_y_raw) / gyro_lsb_per_dps) * ((float)M_PI / 180.0f));
    gz_rad_s = ((((float)mpu->gyro_z_raw) / gyro_lsb_per_dps) * ((float)M_PI / 180.0f));

    s_altitude_runtime.imu_accel_lp_x_g = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.imu_accel_lp_x_g,
                                                                 ax_g,
                                                                 APP_ALTITUDE_IMU_ACCEL_HP_TAU_MS,
                                                                 imu_dt_s);
    s_altitude_runtime.imu_accel_lp_y_g = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.imu_accel_lp_y_g,
                                                                 ay_g,
                                                                 APP_ALTITUDE_IMU_ACCEL_HP_TAU_MS,
                                                                 imu_dt_s);
    s_altitude_runtime.imu_accel_lp_z_g = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.imu_accel_lp_z_g,
                                                                 az_g,
                                                                 APP_ALTITUDE_IMU_ACCEL_HP_TAU_MS,
                                                                 imu_dt_s);

    vib_res_x = ax_g - s_altitude_runtime.imu_accel_lp_x_g;
    vib_res_y = ay_g - s_altitude_runtime.imu_accel_lp_y_g;
    vib_res_z = az_g - s_altitude_runtime.imu_accel_lp_z_g;
    vib_mg = APP_ALTITUDE_VectorNorm3F(vib_res_x, vib_res_y, vib_res_z) * 1000.0f;

    s_altitude_runtime.imu_vibration_rms_mg = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.imu_vibration_rms_mg,
                                                                     vib_mg,
                                                                     APP_ALTITUDE_IMU_VIBRATION_RMS_TAU_MS,
                                                                     imu_dt_s);

    gyro_norm_dps = APP_ALTITUDE_VectorNorm3F(gx_rad_s - s_altitude_runtime.imu_gyro_bias_x_rps,
                                              gy_rad_s - s_altitude_runtime.imu_gyro_bias_y_rps,
                                              gz_rad_s - s_altitude_runtime.imu_gyro_bias_z_rps) *
                    (180.0f / (float)M_PI);

    s_altitude_runtime.imu_gyro_rms_dps = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.imu_gyro_rms_dps,
                                                                 gyro_norm_dps,
                                                                 APP_ALTITUDE_IMU_VIBRATION_RMS_TAU_MS,
                                                                 imu_dt_s);

    accel_norm_g = APP_ALTITUDE_VectorNorm3F(ax_g, ay_g, az_g);
    accel_norm_mg = accel_norm_g * 1000.0f;
    s_altitude_runtime.imu_accel_norm_mg = accel_norm_mg;

    if (accel_norm_g < 0.25f)
    {
        s_altitude_runtime.gravity_est_valid = false;
        s_altitude_runtime.imu_attitude_trust_permille = 0.0f;
        s_altitude_runtime.imu_predict_weight_permille = 0.0f;
        s_altitude_runtime.imu_blend_weight_permille = 0.0f;
        return 0.0f;
    }

    a_hat_x = ax_g / accel_norm_g;
    a_hat_y = ay_g / accel_norm_g;
    a_hat_z = az_g / accel_norm_g;

    if (s_altitude_runtime.gravity_est_valid == false)
    {
        if (APP_ALTITUDE_QuaternionInitFromAccel(a_hat_x,
                                                 a_hat_y,
                                                 a_hat_z,
                                                 &s_altitude_runtime.imu_quat_w,
                                                 &s_altitude_runtime.imu_quat_x,
                                                 &s_altitude_runtime.imu_quat_y,
                                                 &s_altitude_runtime.imu_quat_z) == false)
        {
            APP_ALTITUDE_ResetImuRuntimeForUnavailableInput();
            return 0.0f;
        }

        s_altitude_runtime.gravity_est_valid = true;
    }

    APP_ALTITUDE_QuaternionRotateNavToBody(s_altitude_runtime.imu_quat_w,
                                           s_altitude_runtime.imu_quat_x,
                                           s_altitude_runtime.imu_quat_y,
                                           s_altitude_runtime.imu_quat_z,
                                           0.0f,
                                           0.0f,
                                           1.0f,
                                           &gravity_body_x,
                                           &gravity_body_y,
                                           &gravity_body_z);

    accel_error_mg = fabsf(accel_norm_mg - APP_ALTITUDE_IMU_ACCEL_NORM_REF_MG);
    trust_acc = 1.0f - (accel_error_mg /
                        APP_ALTITUDE_ClampF((float)settings->imu_attitude_accel_gate_mg, 15.0f, 2000.0f));
    trust_acc = APP_ALTITUDE_Clamp01F(trust_acc);

    trust_vib_raw = 1.0f - (s_altitude_runtime.imu_vibration_rms_mg /
                            APP_ALTITUDE_IMU_VIBRATION_TRUST_FULL_MG);
    trust_gyro_raw = 1.0f - (s_altitude_runtime.imu_gyro_rms_dps /
                             APP_ALTITUDE_IMU_GYRO_TRUST_FULL_DPS);
    trust_temp_raw = 1.0f - (fabsf(delta_temp_c) /
                             APP_ALTITUDE_IMU_TEMP_TRUST_FULL_C);

    trust_vib = APP_ALTITUDE_LerpF(0.40f, 1.0f, APP_ALTITUDE_Clamp01F(trust_vib_raw));
    trust_gyro = APP_ALTITUDE_LerpF(0.50f, 1.0f, APP_ALTITUDE_Clamp01F(trust_gyro_raw));
    trust_temp = APP_ALTITUDE_LerpF(0.60f, 1.0f, APP_ALTITUDE_Clamp01F(trust_temp_raw));

    attitude_trust = trust_acc * trust_vib * trust_gyro * trust_temp;
    attitude_trust = APP_ALTITUDE_Clamp01F(attitude_trust);
    s_altitude_runtime.imu_attitude_trust_permille = attitude_trust * 1000.0f;

    err_x = (a_hat_y * gravity_body_z) - (a_hat_z * gravity_body_y);
    err_y = (a_hat_z * gravity_body_x) - (a_hat_x * gravity_body_z);
    err_z = (a_hat_x * gravity_body_y) - (a_hat_y * gravity_body_x);

    mahony_gain = APP_ALTITUDE_IMU_MAHONY_BASE_GAIN +
                  ((APP_ALTITUDE_IMU_MAHONY_MAX_GAIN - APP_ALTITUDE_IMU_MAHONY_BASE_GAIN) * attitude_trust);

    corrected_gx_rad_s = (gx_rad_s - s_altitude_runtime.imu_gyro_bias_x_rps) + (mahony_gain * err_x);
    corrected_gy_rad_s = (gy_rad_s - s_altitude_runtime.imu_gyro_bias_y_rps) + (mahony_gain * err_y);
    corrected_gz_rad_s = (gz_rad_s - s_altitude_runtime.imu_gyro_bias_z_rps) + (mahony_gain * err_z);

    if (APP_ALTITUDE_QuaternionIntegrateBodyRates(&s_altitude_runtime.imu_quat_w,
                                                  &s_altitude_runtime.imu_quat_x,
                                                  &s_altitude_runtime.imu_quat_y,
                                                  &s_altitude_runtime.imu_quat_z,
                                                  corrected_gx_rad_s,
                                                  corrected_gy_rad_s,
                                                  corrected_gz_rad_s,
                                                  imu_dt_s) == false)
    {
        if (APP_ALTITUDE_QuaternionInitFromAccel(a_hat_x,
                                                 a_hat_y,
                                                 a_hat_z,
                                                 &s_altitude_runtime.imu_quat_w,
                                                 &s_altitude_runtime.imu_quat_x,
                                                 &s_altitude_runtime.imu_quat_y,
                                                 &s_altitude_runtime.imu_quat_z) == false)
        {
            APP_ALTITUDE_ResetImuRuntimeForUnavailableInput();
            return 0.0f;
        }
    }

    APP_ALTITUDE_QuaternionRotateNavToBody(s_altitude_runtime.imu_quat_w,
                                           s_altitude_runtime.imu_quat_x,
                                           s_altitude_runtime.imu_quat_y,
                                           s_altitude_runtime.imu_quat_z,
                                           0.0f,
                                           0.0f,
                                           1.0f,
                                           &gravity_body_x,
                                           &gravity_body_y,
                                           &gravity_body_z);

    s_altitude_runtime.gravity_est_x = gravity_body_x;
    s_altitude_runtime.gravity_est_y = gravity_body_y;
    s_altitude_runtime.gravity_est_z = gravity_body_z;
    s_altitude_runtime.gravity_est_valid = true;

    APP_ALTITUDE_QuaternionRotateBodyToNav(s_altitude_runtime.imu_quat_w,
                                           s_altitude_runtime.imu_quat_x,
                                           s_altitude_runtime.imu_quat_y,
                                           s_altitude_runtime.imu_quat_z,
                                           ax_g,
                                           ay_g,
                                           az_g,
                                           &nav_ax_g,
                                           &nav_ay_g,
                                           &nav_az_g);

    sign_f = (settings->imu_vertical_sign >= 0) ? 1.0f : -1.0f;
    vertical_dyn_g = (nav_az_g - 1.0f) * sign_f;

    deadband_g = ((float)settings->imu_vertical_deadband_mg) * 0.001f;
    clip_g = ((float)settings->imu_vertical_clip_mg) * 0.001f;

    if (fabsf(vertical_dyn_g) < deadband_g)
    {
        vertical_dyn_g = 0.0f;
    }

    vertical_dyn_g = APP_ALTITUDE_ClampF(vertical_dyn_g, -clip_g, clip_g);

    rest_vertical_mg = fabsf(vertical_dyn_g) * 1000.0f;
    rest_candidate = ((s_altitude_runtime.imu_gyro_rms_dps < 2.5f) &&
                      (accel_error_mg < APP_ALTITUDE_IMU_REST_ACCEL_ERROR_MG) &&
                      (rest_vertical_mg < APP_ALTITUDE_ClampF((float)settings->rest_detect_accel_mg,
                                                 12.0f,
                                                 150.0f)));

    if (rest_candidate != false)
    {
        s_altitude_runtime.imu_gyro_bias_x_rps = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.imu_gyro_bias_x_rps,
                                                                        gx_rad_s,
                                                                        APP_ALTITUDE_IMU_GYRO_BIAS_TAU_MS,
                                                                        imu_dt_s);
        s_altitude_runtime.imu_gyro_bias_y_rps = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.imu_gyro_bias_y_rps,
                                                                        gy_rad_s,
                                                                        APP_ALTITUDE_IMU_GYRO_BIAS_TAU_MS,
                                                                        imu_dt_s);
        s_altitude_runtime.imu_gyro_bias_z_rps = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.imu_gyro_bias_z_rps,
                                                                        gz_rad_s,
                                                                        APP_ALTITUDE_IMU_GYRO_BIAS_TAU_MS,
                                                                        imu_dt_s);

        s_altitude_runtime.imu_accel_bias_x_g = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.imu_accel_bias_x_g,
                                                                       ax_raw_g - gravity_body_x,
                                                                       APP_ALTITUDE_IMU_ACCEL_BIAS_TAU_MS,
                                                                       imu_dt_s);
        s_altitude_runtime.imu_accel_bias_y_g = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.imu_accel_bias_y_g,
                                                                       ay_raw_g - gravity_body_y,
                                                                       APP_ALTITUDE_IMU_ACCEL_BIAS_TAU_MS,
                                                                       imu_dt_s);
        s_altitude_runtime.imu_accel_bias_z_g = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.imu_accel_bias_z_g,
                                                                       az_raw_g - gravity_body_z,
                                                                       APP_ALTITUDE_IMU_ACCEL_BIAS_TAU_MS,
                                                                       imu_dt_s);

        if (fabsf(delta_temp_c) > 0.5f)
        {
            s_altitude_runtime.imu_accel_temp_gain_x_g_per_c =
                APP_ALTITUDE_LpfUpdate(s_altitude_runtime.imu_accel_temp_gain_x_g_per_c,
                                       ((ax_raw_g - gravity_body_x) - s_altitude_runtime.imu_accel_bias_x_g) /
                                       delta_temp_c,
                                       APP_ALTITUDE_IMU_ACCEL_TEMP_TAU_MS,
                                       imu_dt_s);
            s_altitude_runtime.imu_accel_temp_gain_y_g_per_c =
                APP_ALTITUDE_LpfUpdate(s_altitude_runtime.imu_accel_temp_gain_y_g_per_c,
                                       ((ay_raw_g - gravity_body_y) - s_altitude_runtime.imu_accel_bias_y_g) /
                                       delta_temp_c,
                                       APP_ALTITUDE_IMU_ACCEL_TEMP_TAU_MS,
                                       imu_dt_s);
            s_altitude_runtime.imu_accel_temp_gain_z_g_per_c =
                APP_ALTITUDE_LpfUpdate(s_altitude_runtime.imu_accel_temp_gain_z_g_per_c,
                                       ((az_raw_g - gravity_body_z) - s_altitude_runtime.imu_accel_bias_z_g) /
                                       delta_temp_c,
                                       APP_ALTITUDE_IMU_ACCEL_TEMP_TAU_MS,
                                       imu_dt_s);
        }
    }

    min_trust = ((float)settings->imu_predict_min_trust_permille) * 0.001f;
    if (attitude_trust <= min_trust)
    {
        predict_weight = 0.0f;
    }
    else
    {
        predict_weight = (attitude_trust - min_trust) /
                         APP_ALTITUDE_ClampF((1.0f - min_trust), 0.001f, 1.0f);
        predict_weight = APP_ALTITUDE_Clamp01F(predict_weight);
    }

    predict_weight *= APP_ALTITUDE_Clamp01F(1.0f -
                                            (s_altitude_runtime.imu_vibration_rms_mg /
                                             (APP_ALTITUDE_IMU_VIBRATION_TRUST_FULL_MG * 1.15f)));
    predict_weight *= APP_ALTITUDE_LerpF(0.35f,
                                         1.0f,
                                         APP_ALTITUDE_Clamp01F(1.0f -
                                                               (s_altitude_runtime.imu_gyro_rms_dps /
                                                                (APP_ALTITUDE_IMU_GYRO_TRUST_FULL_DPS * 1.10f))));
    predict_weight = APP_ALTITUDE_Clamp01F(predict_weight);

    s_altitude_runtime.imu_predict_weight_permille = predict_weight * 1000.0f;

    vertical_dyn_g *= predict_weight;
    s_altitude_runtime.imu_vertical_lp_cms2 = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.imu_vertical_lp_cms2,
                                                                     vertical_dyn_g * APP_ALTITUDE_GRAVITY_MPS2 * 100.0f,
                                                                     settings->imu_accel_tau_ms,
                                                                     imu_dt_s);

    if (out_vector_valid != 0)
    {
        *out_vector_valid = s_altitude_runtime.gravity_est_valid;
    }

    return s_altitude_runtime.imu_vertical_lp_cms2;
}

/* -------------------------------------------------------------------------- */
/*  Filter 초기화                                                              */
/* -------------------------------------------------------------------------- */
static void APP_ALTITUDE_EnsureFiltersInitialized(float baro_alt_qnh_cm,
                                                  float gps_alt_cm,
                                                  bool gps_valid)
{
    float initial_alt_cm;
    float initial_bias_cm;

    initial_alt_cm = gps_valid ? gps_alt_cm : baro_alt_qnh_cm;
    initial_bias_cm = baro_alt_qnh_cm - initial_alt_cm;

    if (s_altitude_runtime.kf_noimu.valid == false)
    {
        APP_ALTITUDE_Kf3_Init(s_altitude_runtime.kf_noimu.x,
                              s_altitude_runtime.kf_noimu.P,
                              initial_alt_cm,
                              initial_bias_cm);
        s_altitude_runtime.kf_noimu.valid = true;
    }

    if (s_altitude_runtime.kf_imu.valid == false)
    {
        APP_ALTITUDE_Kf4_Init(s_altitude_runtime.kf_imu.x,
                              s_altitude_runtime.kf_imu.P,
                              initial_alt_cm,
                              initial_bias_cm);
        s_altitude_runtime.kf_imu.valid = true;
    }

    if (s_altitude_runtime.display_output_valid == false)
    {
        /* ------------------------------------------------------------------ */
        /*  표시용 altitude는 상용 vario 철학대로                               */
        /*  "현재 manual QNH가 정의하는 barometric altitude" 를 canonical 로   */
        /*  삼는다.                                                            */
        /*                                                                    */
        /*  즉 fused absolute filter의 GPS anchor 여부와 무관하게              */
        /*  부팅 직후 숫자 고도는 baro/QNH 기준으로 시작한다.                  */
        /* ------------------------------------------------------------------ */
        s_altitude_runtime.display_alt_filt_cm    = baro_alt_qnh_cm;
        s_altitude_runtime.display_alt_follow_cm  = baro_alt_qnh_cm;
        s_altitude_runtime.display_alt_present_cm = baro_alt_qnh_cm;
        s_altitude_runtime.display_output_valid   = true;
    }
}

/* -------------------------------------------------------------------------- */
/*  pending debug action 적용                                                  */
/* -------------------------------------------------------------------------- */
static void APP_ALTITUDE_ApplyPendingActions(float baro_alt_qnh_cm)
{
    if (s_altitude_runtime.bias_rezero_request != false)
    {
        if (s_altitude_runtime.kf_noimu.valid != false)
        {
            s_altitude_runtime.kf_noimu.x[2] = baro_alt_qnh_cm - s_altitude_runtime.kf_noimu.x[0];
        }
        if (s_altitude_runtime.kf_imu.valid != false)
        {
            s_altitude_runtime.kf_imu.x[2] = baro_alt_qnh_cm - s_altitude_runtime.kf_imu.x[0];
        }
        s_altitude_runtime.bias_rezero_request = false;
    }

    if (s_altitude_runtime.home_capture_request != false)
    {
        s_altitude_runtime.home_noimu_cm = s_altitude_runtime.kf_noimu.valid ? s_altitude_runtime.kf_noimu.x[0] : baro_alt_qnh_cm;
        s_altitude_runtime.home_imu_cm   = s_altitude_runtime.kf_imu.valid ? s_altitude_runtime.kf_imu.x[0] : baro_alt_qnh_cm;
        g_app_state.altitude.home_valid = true;
        s_altitude_runtime.home_capture_request = false;
    }
}

/* -------------------------------------------------------------------------- */
/*  Debug vario audio                                                          */
/* -------------------------------------------------------------------------- */
static void APP_ALTITUDE_StopOwnedAudioIfNeeded(void)
{
    if (s_altitude_runtime.audio_owned != false)
    {
        Audio_Driver_Stop();
        s_altitude_runtime.audio_owned = false;
    }
}

static uint32_t APP_ALTITUDE_ComputeClimbBaseFreqHz(const app_altitude_settings_t *settings,
                                                     float vario_cms)
{
    float scale;
    float base_freq_hz;
    float deadband_cms;
    float full_scale_cms;

    deadband_cms = (float)settings->audio_deadband_cms;
    full_scale_cms = APP_ALTITUDE_DEFAULT_CLIMB_FULL_SCALE_CMS;
    scale = (fabsf(vario_cms) - deadband_cms) /
            APP_ALTITUDE_ClampF(full_scale_cms - deadband_cms, 1.0f, 10000.0f);
    scale = APP_ALTITUDE_Clamp01F(scale);

    base_freq_hz = (float)settings->audio_min_freq_hz +
                   ((((settings->audio_max_freq_hz > settings->audio_min_freq_hz) ?
                      ((float)settings->audio_max_freq_hz - (float)settings->audio_min_freq_hz) : 0.0f)) * powf(scale, 0.78f));

    return APP_ALTITUDE_ClampU32((uint32_t)base_freq_hz, 50u, 12000u);
}

static uint32_t APP_ALTITUDE_ComputeSinkBaseFreqHz(const app_altitude_settings_t *settings,
                                                   float vario_cms)
{
    float scale;
    float deadband_cms;
    float full_scale_cms;
    uint32_t sink_freq_min_hz;
    uint32_t sink_freq_max_hz;
    float base_freq_hz;

    deadband_cms = (float)settings->audio_deadband_cms;
    full_scale_cms = APP_ALTITUDE_DEFAULT_CLIMB_FULL_SCALE_CMS;
    scale = (fabsf(vario_cms) - deadband_cms) /
            APP_ALTITUDE_ClampF(full_scale_cms - deadband_cms, 1.0f, 10000.0f);
    scale = APP_ALTITUDE_Clamp01F(scale);

    sink_freq_min_hz = (uint32_t)APP_ALTITUDE_ClampU32((uint32_t)((float)settings->audio_min_freq_hz * 0.28f), 90u, 2000u);
    sink_freq_max_hz = (uint32_t)APP_ALTITUDE_ClampU32((uint32_t)((float)settings->audio_min_freq_hz * 0.62f),
                                                       sink_freq_min_hz + 20u,
                                                       4000u);

    base_freq_hz = (float)sink_freq_max_hz -
                   (((float)(sink_freq_max_hz - sink_freq_min_hz)) * scale);

    return APP_ALTITUDE_ClampU32((uint32_t)base_freq_hz, 50u, 12000u);
}

static float APP_ALTITUDE_ComputeAudioWarbleRateHz(float scale,
                                                bool sink_mode)
{
    if (sink_mode == false)
    {
        return APP_ALTITUDE_LerpF(APP_ALTITUDE_AUDIO_CLIMB_WARBLE_RATE_MIN_HZ,
                                  APP_ALTITUDE_AUDIO_CLIMB_WARBLE_RATE_MAX_HZ,
                                  scale);
    }

    return APP_ALTITUDE_LerpF(APP_ALTITUDE_AUDIO_SINK_WARBLE_RATE_MIN_HZ,
                              APP_ALTITUDE_AUDIO_SINK_WARBLE_RATE_MAX_HZ,
                              scale);
}

static uint32_t APP_ALTITUDE_ComputeAudioMinSegmentMs(void)
{
    uint32_t dma_half_buffer_ms;
    uint32_t min_segment_ms;

    /* ------------------------------------------------------------------ */
    /* simple tone API는 호출할 때마다 내부 재생 상태를 다시 prime한다.     */
    /* 따라서 segment 길이가 DMA half-buffer 시간보다 너무 짧으면          */
    /* 실제 tone보다 restart overhead 비중이 커져 귀에 끊김이 들릴 수 있다. */
    /*                                                                    */
    /* 현재 드라이버 설정값으로 half-buffer 시간을 계산하고,               */
    /* 여기에 margin을 더해 "이보다 짧게는 자르지 않는" 최소 길이를 만든다. */
    /* ------------------------------------------------------------------ */
    dma_half_buffer_ms =
        (uint32_t)((((((uint64_t)AUDIO_DMA_BUFFER_SAMPLES) / 2u) * 1000u) +
                     ((uint64_t)AUDIO_SAMPLE_RATE_HZ - 1u)) /
                    (uint64_t)AUDIO_SAMPLE_RATE_HZ);

    min_segment_ms = dma_half_buffer_ms + APP_ALTITUDE_AUDIO_SEGMENT_MARGIN_MS;

    if (min_segment_ms < APP_ALTITUDE_AUDIO_SEGMENT_FALLBACK_MIN_MS)
    {
        min_segment_ms = APP_ALTITUDE_AUDIO_SEGMENT_FALLBACK_MIN_MS;
    }

    return min_segment_ms;
}

static uint32_t APP_ALTITUDE_ComputeAudioSegmentTargetMs(float scale,
                                                         uint32_t tone_window_ms,
                                                         bool sink_mode)
{
    float mod_rate_hz;
    uint32_t min_segment_ms;
    uint32_t max_segment_ms;
    uint32_t target_segment_ms;

    /* ------------------------------------------------------------------ */
    /* segment 최소 길이는 DMA / restart overhead를 고려해 계산한다.       */
    /* ------------------------------------------------------------------ */
    min_segment_ms = APP_ALTITUDE_ComputeAudioMinSegmentMs();

    /* ------------------------------------------------------------------ */
    /* sink 모드는 더 긴 segment를 허용하고, climb 모드는 조금 더 촘촘하게 */
    /* 잘라도 되므로 최대 길이를 분리한다.                                */
    /* ------------------------------------------------------------------ */
    max_segment_ms = (sink_mode != false)
                     ? APP_ALTITUDE_AUDIO_SEGMENT_SINK_MAX_MS
                     : APP_ALTITUDE_AUDIO_SEGMENT_CLIMB_MAX_MS;

    /* ------------------------------------------------------------------ */
    /* tone window 자체가 최소 segment보다 짧으면 더 나눌 수 없으므로      */
    /* window 길이를 그대로 반환한다.                                     */
    /* ------------------------------------------------------------------ */
    if (tone_window_ms <= min_segment_ms)
    {
        return tone_window_ms;
    }

    /* ------------------------------------------------------------------ */
    /* warble rate가 빠를수록 더 짧은 segment가 필요하다.                  */
    /* 다만 너무 짧아지면 끊김이 생기므로 최소/최대 길이 안으로 clamp한다. */
    /* ------------------------------------------------------------------ */
    mod_rate_hz = APP_ALTITUDE_ComputeAudioWarbleRateHz(scale, sink_mode);

    if (mod_rate_hz < 0.5f)
    {
        mod_rate_hz = 0.5f;
    }

    /* ------------------------------------------------------------------ */
    /* warble의 반주기 정도를 한 segment 목표 길이로 사용한다.             */
    /* 이렇게 하면 비프 한 번 안에서도 fast vario가 Hz에 계속 반영된다.    */
    /* ------------------------------------------------------------------ */
    target_segment_ms = (uint32_t)(1000.0f / (mod_rate_hz * 2.0f));

    if (target_segment_ms < min_segment_ms)
    {
        target_segment_ms = min_segment_ms;
    }

    if (target_segment_ms > max_segment_ms)
    {
        target_segment_ms = max_segment_ms;
    }

    if (target_segment_ms > tone_window_ms)
    {
        target_segment_ms = tone_window_ms;
    }

    if (target_segment_ms == 0u)
    {
        target_segment_ms = tone_window_ms;
    }

    return target_segment_ms;
}

static uint32_t APP_ALTITUDE_ApplyAudioWarbleHz(uint32_t base_freq_hz,
                                                float vario_cms,
                                                float scale,
                                                uint32_t now_ms,
                                                bool sink_mode)
{
    float now_s;
    float mod_rate_hz;
    float mod_depth_hz;
    float phase;
    float warped_hz;

    now_s = ((float)now_ms) * 0.001f;
    mod_rate_hz = APP_ALTITUDE_ComputeAudioWarbleRateHz(scale, sink_mode);

    if (sink_mode == false)
    {
        mod_depth_hz = 22.0f + (0.12f * fabsf(vario_cms));
    }
    else
    {
        mod_depth_hz = 14.0f + (0.07f * fabsf(vario_cms));
    }

    phase = 2.0f * (float)M_PI * mod_rate_hz * now_s;
    warped_hz = ((float)base_freq_hz) + (mod_depth_hz * sinf(phase));

    return APP_ALTITUDE_ClampU32((uint32_t)APP_ALTITUDE_ClampF(warped_hz, 50.0f, 12000.0f), 50u, 12000u);
}

static uint32_t APP_ALTITUDE_ComputeClimbCadencePeriodMs(const app_altitude_settings_t *settings,
                                                         float vario_cms)
{
    float deadband_cms;
    float full_scale_cms;
    float scale;
    uint32_t slow_ms;
    uint32_t fast_ms;
    uint32_t period_ms;

    deadband_cms = (float)settings->audio_deadband_cms;
    full_scale_cms = APP_ALTITUDE_DEFAULT_CLIMB_FULL_SCALE_CMS;

    scale = (fabsf(vario_cms) - deadband_cms) /
            APP_ALTITUDE_ClampF(full_scale_cms - deadband_cms, 1.0f, 10000.0f);
    scale = APP_ALTITUDE_Clamp01F(scale);

    slow_ms = APP_ALTITUDE_ClampU32((uint32_t)settings->audio_repeat_ms + 140u,
                                    APP_ALTITUDE_CLIMB_AUDIO_MIN_PERIOD_MS,
                                    APP_ALTITUDE_CLIMB_AUDIO_MAX_PERIOD_MS);
    fast_ms = APP_ALTITUDE_ClampU32((uint32_t)(settings->audio_repeat_ms / 2u),
                                    APP_ALTITUDE_CLIMB_AUDIO_MIN_PERIOD_MS,
                                    slow_ms);

    period_ms = (uint32_t)((float)slow_ms - (((float)(slow_ms - fast_ms)) * scale));
    return APP_ALTITUDE_ClampU32(period_ms,
                                 APP_ALTITUDE_CLIMB_AUDIO_MIN_PERIOD_MS,
                                 APP_ALTITUDE_CLIMB_AUDIO_MAX_PERIOD_MS);
}

static uint32_t APP_ALTITUDE_ComputeClimbToneWindowMs(const app_altitude_settings_t *settings,
                                                      float vario_cms,
                                                      uint32_t period_ms)
{
    float deadband_cms;
    float full_scale_cms;
    float scale;
    uint32_t tone_ms;

    deadband_cms = (float)settings->audio_deadband_cms;
    full_scale_cms = APP_ALTITUDE_DEFAULT_CLIMB_FULL_SCALE_CMS;

    scale = (fabsf(vario_cms) - deadband_cms) /
            APP_ALTITUDE_ClampF(full_scale_cms - deadband_cms, 1.0f, 10000.0f);
    scale = APP_ALTITUDE_Clamp01F(scale);

    tone_ms = (uint32_t)((float)settings->audio_beep_ms * (1.20f - (0.25f * scale)) + 12.0f);
    return APP_ALTITUDE_ClampU32(tone_ms,
                                 APP_ALTITUDE_ComputeAudioMinSegmentMs(),
                                 (period_ms > 8u) ? (period_ms - 8u) : period_ms);
}

static uint32_t APP_ALTITUDE_ComputeSinkToneWindowMs(const app_altitude_settings_t *settings,
                                                     float vario_cms)
{
    float deadband_cms;
    float full_scale_cms;
    float scale;
    uint32_t tone_ms;

    deadband_cms = (float)settings->audio_deadband_cms;
    full_scale_cms = APP_ALTITUDE_DEFAULT_CLIMB_FULL_SCALE_CMS;

    scale = (fabsf(vario_cms) - deadband_cms) /
            APP_ALTITUDE_ClampF(full_scale_cms - deadband_cms, 1.0f, 10000.0f);
    scale = APP_ALTITUDE_Clamp01F(scale);

    tone_ms = (uint32_t)((float)APP_ALTITUDE_SINK_AUDIO_MAX_TONE_MS -
                         (((float)(APP_ALTITUDE_SINK_AUDIO_MAX_TONE_MS - APP_ALTITUDE_SINK_AUDIO_MIN_TONE_MS)) * scale));

    return APP_ALTITUDE_ClampU32(tone_ms,
                                 APP_ALTITUDE_SINK_AUDIO_MIN_TONE_MS,
                                 APP_ALTITUDE_SINK_AUDIO_MAX_TONE_MS);
}

static uint32_t APP_ALTITUDE_ComputeSinkGapMs(const app_altitude_settings_t *settings,
                                              float vario_cms)
{
    float deadband_cms;
    float full_scale_cms;
    float scale;
    uint32_t gap_ms;

    deadband_cms = (float)settings->audio_deadband_cms;
    full_scale_cms = APP_ALTITUDE_DEFAULT_CLIMB_FULL_SCALE_CMS;

    scale = (fabsf(vario_cms) - deadband_cms) /
            APP_ALTITUDE_ClampF(full_scale_cms - deadband_cms, 1.0f, 10000.0f);
    scale = APP_ALTITUDE_Clamp01F(scale);

    gap_ms = (uint32_t)((float)APP_ALTITUDE_SINK_AUDIO_MAX_GAP_MS -
                        (((float)(APP_ALTITUDE_SINK_AUDIO_MAX_GAP_MS - APP_ALTITUDE_SINK_AUDIO_MIN_GAP_MS)) * scale));

    return APP_ALTITUDE_ClampU32(gap_ms,
                                 APP_ALTITUDE_SINK_AUDIO_MIN_GAP_MS,
                                 APP_ALTITUDE_ClampU32((uint32_t)(settings->audio_repeat_ms / 4u),
                                                       APP_ALTITUDE_SINK_AUDIO_MAX_GAP_MS,
                                                       180u));
}

static void APP_ALTITUDE_ResetAudioCycleIfNeeded(uint32_t now_ms, int8_t mode)
{
    if (s_altitude_runtime.audio_mode != mode)
    {
        s_altitude_runtime.audio_mode = mode;
        s_altitude_runtime.audio_cycle_start_ms = now_ms;
    }
}

/* -------------------------------------------------------------------------- */
/*  audio hysteresis threshold 계산 helper                                     */
/* -------------------------------------------------------------------------- */
static float APP_ALTITUDE_ComputeAudioClimbEnterCms(const app_altitude_settings_t *settings)
{
    float deadband_cms;

    deadband_cms = (settings != 0) ? (float)settings->audio_deadband_cms : 35.0f;
    return APP_ALTITUDE_ClampF(APP_ALTITUDE_ClampF(deadband_cms * APP_ALTITUDE_AUDIO_CLIMB_ENTER_SCALE,
                                                   APP_ALTITUDE_AUDIO_CLIMB_ENTER_FLOOR_CMS,
                                                   500.0f),
                               1.0f,
                               500.0f);
}

static float APP_ALTITUDE_ComputeAudioClimbExitCms(const app_altitude_settings_t *settings)
{
    float deadband_cms;

    deadband_cms = (settings != 0) ? (float)settings->audio_deadband_cms : 35.0f;
    return APP_ALTITUDE_ClampF(APP_ALTITUDE_ClampF(deadband_cms * APP_ALTITUDE_AUDIO_CLIMB_EXIT_SCALE,
                                                   APP_ALTITUDE_AUDIO_CLIMB_EXIT_FLOOR_CMS,
                                                   500.0f),
                               1.0f,
                               500.0f);
}

static float APP_ALTITUDE_ComputeAudioSinkEnterCms(const app_altitude_settings_t *settings)
{
    float deadband_cms;

    deadband_cms = (settings != 0) ? (float)settings->audio_deadband_cms : 35.0f;
    return APP_ALTITUDE_ClampF(APP_ALTITUDE_ClampF(deadband_cms * APP_ALTITUDE_AUDIO_SINK_ENTER_SCALE,
                                                   APP_ALTITUDE_AUDIO_SINK_ENTER_FLOOR_CMS,
                                                   1200.0f),
                               1.0f,
                               1200.0f);
}

static float APP_ALTITUDE_ComputeAudioSinkExitCms(const app_altitude_settings_t *settings)
{
    float deadband_cms;

    deadband_cms = (settings != 0) ? (float)settings->audio_deadband_cms : 35.0f;
    return APP_ALTITUDE_ClampF(APP_ALTITUDE_ClampF(deadband_cms * APP_ALTITUDE_AUDIO_SINK_EXIT_SCALE,
                                                   APP_ALTITUDE_AUDIO_SINK_EXIT_FLOOR_CMS,
                                                   1200.0f),
                               1.0f,
                               1200.0f);
}

/* -------------------------------------------------------------------------- */
/*  현재 smooth vario 기준으로 audio mode를 무엇으로 보고 싶은지 계산          */
/*                                                                            */
/*  mode 의미                                                                  */
/*  - +1 : climb square waveform                                              */
/*  -  0 : silent / neutral                                                   */
/*  - -1 : sink saw waveform                                                  */
/* -------------------------------------------------------------------------- */
static int8_t APP_ALTITUDE_ComputeRequestedAudioMode(const app_altitude_settings_t *settings,
                                                     float audio_vario_cms)
{
    float climb_enter_cms;
    float climb_exit_cms;
    float sink_enter_cms;
    float sink_exit_cms;
    int8_t current_mode;

    climb_enter_cms = APP_ALTITUDE_ComputeAudioClimbEnterCms(settings);
    climb_exit_cms  = APP_ALTITUDE_ComputeAudioClimbExitCms(settings);
    sink_enter_cms  = APP_ALTITUDE_ComputeAudioSinkEnterCms(settings);
    sink_exit_cms   = APP_ALTITUDE_ComputeAudioSinkExitCms(settings);
    current_mode    = s_altitude_runtime.audio_mode;

    if (current_mode > 0)
    {
        if (audio_vario_cms >= climb_exit_cms)
        {
            return 1;
        }

        if (audio_vario_cms <= -sink_enter_cms)
        {
            return -1;
        }

        return 0;
    }

    if (current_mode < 0)
    {
        if (audio_vario_cms <= -sink_exit_cms)
        {
            return -1;
        }

        if (audio_vario_cms >= climb_enter_cms)
        {
            return 1;
        }

        return 0;
    }

    if (audio_vario_cms >= climb_enter_cms)
    {
        return 1;
    }

    if (audio_vario_cms <= -sink_enter_cms)
    {
        return -1;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
/*  requested mode를 hold time까지 만족했을 때만 committed mode로 승격         */
/*                                                                            */
/*  이유                                                                       */
/*  - fast vario가 threshold를 잠깐 스치는 순간                                 */
/*    climb/sink waveform이 즉시 뒤집히면 귀에 매우 거칠게 들린다.            */
/*  - 그래서 requested mode가 일정 시간 유지됐을 때만                         */
/*    실제 audio mode를 바꾼다.                                                */
/* -------------------------------------------------------------------------- */
static int8_t APP_ALTITUDE_UpdateCommittedAudioMode(uint32_t now_ms,
                                                    int8_t requested_mode)
{
    uint32_t hold_ms;

    if (requested_mode != s_altitude_runtime.audio_mode_candidate)
    {
        s_altitude_runtime.audio_mode_candidate = requested_mode;
        s_altitude_runtime.audio_mode_candidate_since_ms = now_ms;
    }

    if (requested_mode == s_altitude_runtime.audio_mode)
    {
        return s_altitude_runtime.audio_mode;
    }

    hold_ms = (requested_mode == 0) ?
              APP_ALTITUDE_AUDIO_MODE_EXIT_HOLD_MS :
              APP_ALTITUDE_AUDIO_MODE_ENTER_HOLD_MS;

    if ((uint32_t)(now_ms - s_altitude_runtime.audio_mode_candidate_since_ms) < hold_ms)
    {
        return s_altitude_runtime.audio_mode;
    }

    return requested_mode;
}

/* -------------------------------------------------------------------------- */
/*  현재 speed scale로 audio level target을 계산                               */
/* -------------------------------------------------------------------------- */
static uint16_t APP_ALTITUDE_ComputeAudioLevelPermille(float scale, bool tone_on)
{
    float level_f;

    if (tone_on == false)
    {
        return 0u;
    }

    scale = APP_ALTITUDE_Clamp01F(scale);
    level_f = (float)APP_ALTITUDE_AUDIO_LEVEL_MIN_PERMILLE +
              (((float)(APP_ALTITUDE_AUDIO_LEVEL_MAX_PERMILLE - APP_ALTITUDE_AUDIO_LEVEL_MIN_PERMILLE)) * scale);

    return (uint16_t)APP_ALTITUDE_ClampU32((uint32_t)(level_f + 0.5f),
                                           APP_ALTITUDE_AUDIO_LEVEL_MIN_PERMILLE,
                                           APP_ALTITUDE_AUDIO_LEVEL_MAX_PERMILLE);
}

static void APP_ALTITUDE_HandleDebugAudio(uint32_t now_ms, const app_altitude_settings_t *settings)
{
    float vario_raw_cms;
    float audio_dt_s;
    float audio_vario_cms;
    float speed_abs_cms;
    float deadband_cms;
    float full_scale_cms;
    float scale;
    float glide_ms_f;
    uint32_t cycle_period_ms;
    uint32_t tone_window_ms;
    uint32_t cycle_elapsed_ms;
    uint32_t glide_ms;
    uint32_t freq_hz;
    uint16_t level_permille;
    int8_t previous_mode;
    int8_t requested_mode;
    int8_t committed_mode;
    bool tone_on;
    bool sink_mode;
    app_audio_waveform_t waveform;

    if ((settings == 0) || (s_altitude_runtime.ui_active == false) || (settings->debug_audio_enabled == 0u))
    {
        g_app_state.altitude.debug_audio_active = 0u;
        g_app_state.altitude.debug_audio_vario_cms = 0;
        s_altitude_runtime.audio_mode = 0;
        s_altitude_runtime.audio_mode_candidate = 0;
        APP_ALTITUDE_StopOwnedAudioIfNeeded();
        return;
    }

    g_app_state.altitude.debug_audio_active = 1u;

    /* ------------------------------------------------------------------ */
    /*  neutral 상태에서 이미 우리 continuous vario voice가 완전히 내려갔다면 */
    /*  ownership 플래그도 함께 해제해,                                     */
    /*  이후 다른 오디오를 잘못 "내 소유" 로 오해하지 않게 한다.            */
    /* ------------------------------------------------------------------ */
    if ((Audio_Driver_IsVarioActive() == false) && (s_altitude_runtime.audio_mode == 0))
    {
        s_altitude_runtime.audio_owned = false;
    }

    /* ------------------------------------------------------------------ */
    /*  현재 선택된 fast vario source를 가져오고                            */
    /*  audio 전용 branch에서 한 번 더 LPF 처리한다.                        */
    /*  이 분기는 화면 표시용 fast vario와 의도적으로 분리되어 있다.        */
    /* ------------------------------------------------------------------ */
    vario_raw_cms = APP_ALTITUDE_GetDebugAudioSourceVarioCms(settings);
    g_app_state.altitude.debug_audio_vario_cms = APP_ALTITUDE_RoundFloatToS32(vario_raw_cms);

    audio_dt_s = ((float)(now_ms - s_altitude_runtime.last_audio_ms)) * 0.001f;
    audio_dt_s = APP_ALTITUDE_ClampF(audio_dt_s, APP_ALTITUDE_MIN_DT_S, APP_ALTITUDE_MAX_DT_S);
    s_altitude_runtime.last_audio_ms = now_ms;

    if (s_altitude_runtime.audio_vario_smooth_cms == 0.0f)
    {
        s_altitude_runtime.audio_vario_smooth_cms = vario_raw_cms;
    }
    else
    {
        s_altitude_runtime.audio_vario_smooth_cms = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.audio_vario_smooth_cms,
                                                                           vario_raw_cms,
                                                                           APP_ALTITUDE_AUDIO_CONTROL_TAU_MS,
                                                                           audio_dt_s);
    }

    audio_vario_cms = s_altitude_runtime.audio_vario_smooth_cms;
    speed_abs_cms   = fabsf(audio_vario_cms);
    deadband_cms    = (float)settings->audio_deadband_cms;
    full_scale_cms  = APP_ALTITUDE_DEFAULT_CLIMB_FULL_SCALE_CMS;

    scale = (speed_abs_cms - deadband_cms) /
            APP_ALTITUDE_ClampF(full_scale_cms - deadband_cms, 1.0f, 10000.0f);
    scale = APP_ALTITUDE_Clamp01F(scale);

    previous_mode  = s_altitude_runtime.audio_mode;
    requested_mode = APP_ALTITUDE_ComputeRequestedAudioMode(settings, audio_vario_cms);
    committed_mode = APP_ALTITUDE_UpdateCommittedAudioMode(now_ms, requested_mode);

    /* ------------------------------------------------------------------ */
    /*  neutral zone에 들어가면 current oscillator를 서서히 감쇠시킨다.     */
    /*  이렇게 하면 UI는 켜져 있어도 near-zero chatter가 바로 소리로        */
    /*  튀어나오지 않는다.                                                   */
    /* ------------------------------------------------------------------ */
    if (committed_mode == 0)
    {
        s_altitude_runtime.audio_mode = 0;

        if (Audio_Driver_IsVarioActive() != false)
        {
            (void)Audio_Driver_VarioStop(APP_ALTITUDE_AUDIO_CONTROL_STOP_RELEASE_MS);
            s_altitude_runtime.last_audio_owner_ms = now_ms;
            s_altitude_runtime.audio_owned = true;
        }
        else
        {
            s_altitude_runtime.audio_owned = false;
        }

        return;
    }

    /* ------------------------------------------------------------------ */
    /*  다른 오디오가 이미 재생 중인데 우리가 아직 owner가 아니면            */
    /*  debug vario가 강제로 steal하지 않는다.                               */
    /* ------------------------------------------------------------------ */
    if ((Audio_Driver_IsBusy() != false) && (Audio_Driver_IsVarioActive() == false))
    {
        return;
    }

    APP_ALTITUDE_ResetAudioCycleIfNeeded(now_ms, committed_mode);

    sink_mode = (committed_mode < 0) ? true : false;
    waveform = sink_mode ? APP_AUDIO_WAVEFORM_SAW : APP_AUDIO_WAVEFORM_SQUARE;

    if (committed_mode > 0)
    {
        cycle_period_ms = APP_ALTITUDE_ComputeClimbCadencePeriodMs(settings, audio_vario_cms);
        tone_window_ms  = APP_ALTITUDE_ComputeClimbToneWindowMs(settings, audio_vario_cms, cycle_period_ms);
    }
    else
    {
        tone_window_ms  = APP_ALTITUDE_ComputeSinkToneWindowMs(settings, audio_vario_cms);
        cycle_period_ms = tone_window_ms + APP_ALTITUDE_ComputeSinkGapMs(settings, audio_vario_cms);
    }

    if (cycle_period_ms == 0u)
    {
        cycle_period_ms = 1u;
    }

    while ((uint32_t)(now_ms - s_altitude_runtime.audio_cycle_start_ms) >= cycle_period_ms)
    {
        s_altitude_runtime.audio_cycle_start_ms += cycle_period_ms;
    }

    cycle_elapsed_ms = (uint32_t)(now_ms - s_altitude_runtime.audio_cycle_start_ms);
    tone_on = (cycle_elapsed_ms < tone_window_ms) ? true : false;

    if (committed_mode > 0)
    {
        freq_hz = APP_ALTITUDE_ApplyAudioWarbleHz(APP_ALTITUDE_ComputeClimbBaseFreqHz(settings, audio_vario_cms),
                                                  audio_vario_cms,
                                                  scale,
                                                  now_ms,
                                                  false);
    }
    else
    {
        freq_hz = APP_ALTITUDE_ApplyAudioWarbleHz(APP_ALTITUDE_ComputeSinkBaseFreqHz(settings, audio_vario_cms),
                                                  audio_vario_cms,
                                                  scale,
                                                  now_ms,
                                                  true);
    }

    glide_ms_f = (float)APP_ALTITUDE_ComputeAudioSegmentTargetMs(scale,
                                                                 tone_window_ms,
                                                                 sink_mode);
    glide_ms_f = APP_ALTITUDE_ClampF(glide_ms_f,
                                     (float)APP_ALTITUDE_AUDIO_CONTROL_LEVEL_GLIDE_MS,
                                     (float)APP_ALTITUDE_AUDIO_CONTROL_FREQ_GLIDE_MS);
    glide_ms = (uint32_t)(glide_ms_f + 0.5f);
    if (glide_ms < APP_ALTITUDE_AUDIO_CONTROL_LEVEL_GLIDE_MS)
    {
        glide_ms = APP_ALTITUDE_AUDIO_CONTROL_LEVEL_GLIDE_MS;
    }

    level_permille = APP_ALTITUDE_ComputeAudioLevelPermille(scale, tone_on);

    /* ------------------------------------------------------------------ */
    /*  waveform이 바뀌거나 아직 continuous vario가 살아 있지 않으면         */
    /*  여기서 한 번만 Start 하고, 그 뒤에는 SetTarget만 반복한다.          */
    /* ------------------------------------------------------------------ */
    if ((Audio_Driver_IsVarioActive() == false) || (previous_mode != committed_mode))
    {
        if (Audio_Driver_VarioStart(waveform, freq_hz, level_permille) != HAL_OK)
        {
            return;
        }
    }

    if (Audio_Driver_VarioSetTarget(freq_hz,
                                    level_permille,
                                    glide_ms) == HAL_OK)
    {
        s_altitude_runtime.last_audio_owner_ms = now_ms;
        s_altitude_runtime.audio_owned = true;
    }
}

/* -------------------------------------------------------------------------- */
/*  Public API                                                                 */
/* -------------------------------------------------------------------------- */
void APP_ALTITUDE_Init(uint32_t now_ms)
{
    memset((void *)&s_altitude_runtime, 0, sizeof(s_altitude_runtime));
    s_altitude_runtime.initialized = true;
    s_altitude_runtime.last_task_ms = now_ms;
    s_altitude_runtime.qnh_equiv_filt_hpa = ((float)g_app_state.settings.altitude.manual_qnh_hpa_x100) * 0.01f;
    s_altitude_runtime.display_baro_gate_ref_cm = 0.0f;
    s_altitude_runtime.display_baro_gate_valid = false;
    s_altitude_runtime.display_gate_qnh_hpa_x100 = g_app_state.settings.altitude.manual_qnh_hpa_x100;
    s_altitude_runtime.display_gate_pressure_correction_hpa_x100 =
        g_app_state.settings.altitude.pressure_correction_hpa_x100;
    s_altitude_runtime.audio_mode = 0;
    s_altitude_runtime.audio_mode_candidate = 0;
    s_altitude_runtime.audio_mode_candidate_since_ms = now_ms;
    s_altitude_runtime.last_audio_ms = now_ms;

    g_app_state.altitude.initialized = true;
    g_app_state.altitude.qnh_manual_hpa_x100 = g_app_state.settings.altitude.manual_qnh_hpa_x100;
    g_app_state.altitude.qnh_equiv_gps_hpa_x100 = g_app_state.settings.altitude.manual_qnh_hpa_x100;
}

void APP_ALTITUDE_Task(uint32_t now_ms)
{
    app_altitude_settings_t settings_local;
    app_altitude_state_t altitude_prev_local;
    app_gy86_baro_raw_t baro_local;
    gps_fix_basic_t gps_fix_local;
    app_gy86_mpu_raw_t mpu_local;
    const app_altitude_settings_t *settings;
    const app_altitude_state_t *alt_prev;
    const app_gy86_baro_raw_t *baro;
    const gps_fix_basic_t *gps_fix;
    const app_gy86_mpu_raw_t *mpu;
    bool new_baro_sample;
    bool new_gps_sample;
    bool gps_valid;
    bool imu_vector_valid;
    bool imu_input_enabled;
    bool core_rest_active;
    bool baro_rest_hint;
    uint16_t gps_quality_permille;
    float dt_s;
    float baro_dt_s;
    float pressure_raw_hpa;
    float pressure_corrected_hpa;
    float pressure_prefilt_hpa;
    float qnh_manual_hpa;
    float qnh_equiv_hpa;
    float alt_std_cm;
    float alt_qnh_cm;
    float alt_qnh_vario_cm;
    float alt_gps_cm;
    float gps_noise_cm;
    float baro_noise_cm;
    float baro_altitude_residual_cm;
    float baro_velocity_meas_cms;
    float baro_velocity_noise_cms;
    float imu_vertical_cms2;
    float imu_predict_weight;
    float imu_predict_cms2;
    float imu_predict_noise_cms2;
    float imu_anchor_velocity_cms;
    float imu_velocity_disagreement_cms;
    float imu_blend_fast;
    float imu_blend_slow;
    float imu_fast_target_cms;
    float imu_slow_target_cms;
    float display_target_cm;
    float display_source_vario_cms;
    uint32_t display_tau_ms;
    bool display_rest_active;
    bool display_baro_available;
    bool display_baro_target_accepted;
    bool display_baro_basis_changed;
    float H_baro3[3];
    float H_gps3[3];
    float H_vel3[3];
    float H_zero_vel3[3];
    float H_baro4[4];
    float H_gps4[4];
    float H_vel4[4];
    float H_zero_vel4[4];

    /* ------------------------------------------------------------------ */
    /*  volatile APP_STATE를 함수 시작 시점에 로컬 snapshot으로 복사한다.  */
    /*  이렇게 하면                                                           */
    /*  - 한 번의 task 동안 입력이 일관되게 유지되고                         */
    /*  - volatile qualifier discard 경고 없이                               */
    /*    계산 helper에 안전하게 넘길 수 있다.                               */
    /* ------------------------------------------------------------------ */
    settings_local     = g_app_state.settings.altitude;
    altitude_prev_local = g_app_state.altitude;
    baro_local         = g_app_state.gy86.baro;
    gps_fix_local      = g_app_state.gps.fix;
    mpu_local          = g_app_state.gy86.mpu;

    settings = &settings_local;
    alt_prev = &altitude_prev_local;
    baro     = &baro_local;
    gps_fix  = &gps_fix_local;
    mpu      = &mpu_local;

    if (s_altitude_runtime.initialized == false)
    {
        APP_ALTITUDE_Init(now_ms);
    }

    dt_s = ((float)(now_ms - s_altitude_runtime.last_task_ms)) * 0.001f;
    dt_s = APP_ALTITUDE_ClampF(dt_s, APP_ALTITUDE_MIN_DT_S, APP_ALTITUDE_MAX_DT_S);
    s_altitude_runtime.last_task_ms = now_ms;

    new_baro_sample = false;
    new_gps_sample  = false;
    gps_valid       = false;
    imu_vector_valid = false;
    imu_input_enabled = APP_ALTITUDE_IsImuInputEnabled(settings);
    core_rest_active = false;
    baro_rest_hint = false;
    gps_quality_permille = 0u;

    pressure_raw_hpa = 0.0f;
    pressure_corrected_hpa = 0.0f;
    pressure_prefilt_hpa = s_altitude_runtime.pressure_prefilt_hpa;
    qnh_equiv_hpa = s_altitude_runtime.qnh_equiv_filt_hpa;
    alt_std_cm = 0.0f;
    alt_qnh_cm = 0.0f;
    alt_qnh_vario_cm = 0.0f;
    alt_gps_cm = 0.0f;
    gps_noise_cm = 0.0f;
    baro_noise_cm = (float)settings->baro_measurement_noise_cm;
    baro_altitude_residual_cm = 0.0f;
    baro_velocity_meas_cms = s_altitude_runtime.baro_vario_filt_cms;
    baro_velocity_noise_cms = (float)settings->baro_vario_measurement_noise_cms;
    imu_vertical_cms2 = 0.0f;
    imu_predict_weight = 0.0f;
    imu_predict_cms2 = 0.0f;
    imu_predict_noise_cms2 = (float)settings->imu_measurement_noise_cms2;
    imu_anchor_velocity_cms = 0.0f;
    imu_velocity_disagreement_cms = 0.0f;
    imu_blend_fast = 0.0f;
    imu_blend_slow = 0.0f;
    imu_fast_target_cms = 0.0f;
    imu_slow_target_cms = 0.0f;
    display_target_cm = 0.0f;
    display_source_vario_cms = 0.0f;
    display_tau_ms = settings->display_lpf_tau_ms;
    display_rest_active = false;
    display_baro_available = false;
    display_baro_target_accepted = true;
    display_baro_basis_changed = false;
    baro_dt_s = dt_s;

    qnh_manual_hpa = ((float)settings->manual_qnh_hpa_x100) * 0.01f;
    qnh_manual_hpa = APP_ALTITUDE_ClampF(qnh_manual_hpa, 800.0f, 1100.0f);
    pressure_corrected_hpa = pressure_raw_hpa + (((float)settings->pressure_correction_hpa_x100) * 0.01f);

    /* ------------------------------------------------------------------ */
    /*  BARO path                                                           */
    /*  - sample_count / timestamp 기준으로만 새 샘플을 반영한다.            */
    /*  - raw pressure는 그대로 보존하되,                                    */
    /*    filter 입력은 median-3 -> LPF 순서로 정리한다.                    */
    /*  - derivative는 실제 baro sample dt로 계산한다.                      */
    /* ------------------------------------------------------------------ */
    if ((baro->sample_count != 0u) &&
        (baro->pressure_hpa_x100 > 0) &&
        (baro->sample_count != s_altitude_runtime.last_baro_sample_count))
    {
        new_baro_sample = true;
        s_altitude_runtime.last_baro_sample_count = baro->sample_count;

        if ((baro->timestamp_ms != 0u) &&
            (s_altitude_runtime.last_baro_timestamp_ms != 0u) &&
            (baro->timestamp_ms > s_altitude_runtime.last_baro_timestamp_ms))
        {
            baro_dt_s = APP_ALTITUDE_ComputeSampleDtS(baro->timestamp_ms,
                                                      s_altitude_runtime.last_baro_timestamp_ms,
                                                      dt_s);
        }
        s_altitude_runtime.last_baro_timestamp_ms = baro->timestamp_ms;

        pressure_raw_hpa = ((float)baro->pressure_hpa_x100) * 0.01f;

        /* ------------------------------------------------------------------ */
        /*  installation / static bias correction                              */
        /*                                                                    */
        /*  correction은 QNH를 바꾸는 항목이 아니다.                           */
        /*  센서 raw static pressure에 additive bias를 먼저 적용한 뒤          */
        /*  그 corrected pressure를                                            */
        /*  - baro median/prefilter                                            */
        /*  - QNH altitude                                                     */
        /*  - FL / standard altitude                                           */
        /*  - Smart Fuse / display path                                        */
        /*  전부의 공통 입력으로 사용한다.                                     */
        /*                                                                    */
        /*  반대로 pressure_raw_hpa_x100 publish 값은 실제 raw 센서값을        */
        /*  그대로 남겨, 현장 로그에서 센서 원본과 correction 적용 결과를      */
        /*  분리해서 볼 수 있게 유지한다.                                      */
        /* ------------------------------------------------------------------ */
        pressure_corrected_hpa = pressure_raw_hpa + (((float)settings->pressure_correction_hpa_x100) * 0.01f);
        pressure_prefilt_hpa = APP_ALTITUDE_UpdatePressurePrefilter(pressure_corrected_hpa);

        /* ------------------------------------------------------------------ */
        /*  pressure source 분리                                               */
        /*                                                                    */
        /*  slow branch  : altitude / fused altitude / display 안정화용         */
        /*  fast branch  : vario regression slope 계산 전용                     */
        /*                                                                    */
        /*  왜 분리하나?                                                        */
        /*  기존 구조에서는 settings->pressure_lpf_tau_ms 하나를 줄이면         */
        /*  "vario는 빨라지지만 altitude도 같이 흔들리는" trade-off 가 생겼다. */
        /*                                                                    */
        /*  commercial-grade 목표는                                            */
        /*  - 숫자 altitude 는 차분하게                                         */
        /*  - fast vario / audio 는 즉각적이되 false tone 는 과하지 않게       */
        /*  이므로, pressure source 를 두 갈래로 나눠야 한다.                  */
        /* ------------------------------------------------------------------ */
        if ((s_altitude_runtime.pressure_filt_hpa <= 0.0f) || (alt_prev->baro_valid == false))
        {
            s_altitude_runtime.pressure_residual_hpa = 0.0f;
            s_altitude_runtime.pressure_filt_hpa = pressure_prefilt_hpa;
        }
        else
        {
            s_altitude_runtime.pressure_residual_hpa = pressure_prefilt_hpa - s_altitude_runtime.pressure_filt_hpa;
            s_altitude_runtime.pressure_filt_hpa = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.pressure_filt_hpa,
                                                                          pressure_prefilt_hpa,
                                                                          settings->pressure_lpf_tau_ms,
                                                                          baro_dt_s);
        }

        if ((s_altitude_runtime.pressure_vario_filt_hpa <= 0.0f) || (alt_prev->baro_valid == false))
        {
            s_altitude_runtime.pressure_vario_filt_hpa = pressure_prefilt_hpa;
        }
        else
        {
            s_altitude_runtime.pressure_vario_filt_hpa = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.pressure_vario_filt_hpa,
                                                                                pressure_prefilt_hpa,
                                                                                APP_ALTITUDE_VARIO_PRESSURE_TAU_MS,
                                                                                baro_dt_s);
        }

        alt_std_cm = APP_ALTITUDE_PressureToAltitudeMeters(s_altitude_runtime.pressure_filt_hpa,
                                                           APP_ALTITUDE_STD_QNH_HPA) * 100.0f;
        alt_qnh_cm = APP_ALTITUDE_PressureToAltitudeMeters(s_altitude_runtime.pressure_filt_hpa,
                                                           qnh_manual_hpa) * 100.0f;
        alt_qnh_vario_cm = APP_ALTITUDE_PressureToAltitudeMeters(s_altitude_runtime.pressure_vario_filt_hpa,
                                                                 qnh_manual_hpa) * 100.0f;

        baro_noise_cm = APP_ALTITUDE_ComputeAdaptiveBaroNoiseCm(settings,
                                                                s_altitude_runtime.pressure_filt_hpa,
                                                                s_altitude_runtime.pressure_residual_hpa,
                                                                baro_dt_s);

        baro_velocity_meas_cms = APP_ALTITUDE_UpdateBaroVarioMeasurement(settings,
                                                                         alt_qnh_vario_cm,
                                                                         baro_dt_s,
                                                                         baro->timestamp_ms);
        baro_velocity_noise_cms = APP_ALTITUDE_ComputeAdaptiveBaroVarioNoiseCms(settings);
    }
    else if (alt_prev->baro_valid != false)
    {
        pressure_raw_hpa = ((float)alt_prev->pressure_raw_hpa_x100) * 0.01f;
        pressure_corrected_hpa = pressure_raw_hpa + (((float)settings->pressure_correction_hpa_x100) * 0.01f);
        pressure_prefilt_hpa = ((float)alt_prev->pressure_prefilt_hpa_x100) * 0.01f;
        alt_std_cm = (float)alt_prev->alt_pressure_std_cm;
        alt_qnh_cm = (float)alt_prev->alt_qnh_manual_cm;
        alt_qnh_vario_cm = alt_qnh_cm;
        baro_noise_cm = (float)alt_prev->baro_noise_used_cm;
        if (baro_noise_cm <= 0.0f)
        {
            baro_noise_cm = (float)settings->baro_measurement_noise_cm;
        }

        baro_velocity_meas_cms = (float)alt_prev->baro_vario_filt_cms;
        baro_velocity_noise_cms = (float)settings->baro_vario_measurement_noise_cms;
    }

    /* ------------------------------------------------------------------ */
    /*  GPS path                                                            */
    /* ------------------------------------------------------------------ */
    gps_valid = APP_ALTITUDE_IsGpsAltitudeUsable(settings, gps_fix, &gps_quality_permille);
    if (gps_valid != false)
    {
        alt_gps_cm = ((float)gps_fix->hMSL) * 0.1f;

        if (gps_fix->last_update_ms != s_altitude_runtime.last_gps_fix_update_ms)
        {
            s_altitude_runtime.last_gps_fix_update_ms = gps_fix->last_update_ms;
            new_gps_sample = true;
        }
    }

    /* ------------------------------------------------------------------ */
    /*  equivalent QNH는 "GPS와 pressure가 현재 일치하도록 만드는 기준압"   */
    /*  참고값으로만 유지하고, manual QNH를 덮어쓰지 않는다.                 */
    /* ------------------------------------------------------------------ */
    if ((settings->gps_auto_equiv_qnh_enabled != 0u) &&
        ((new_gps_sample != false) || (alt_prev->gps_valid != false)) &&
        ((new_baro_sample != false) || (alt_prev->baro_valid != false)) &&
        (gps_valid != false) &&
        (s_altitude_runtime.pressure_filt_hpa > 0.0f))
    {
        qnh_equiv_hpa = APP_ALTITUDE_AltitudeMetersToEquivalentQnh(s_altitude_runtime.pressure_filt_hpa,
                                                                   alt_gps_cm * 0.01f);

        if (s_altitude_runtime.qnh_equiv_filt_hpa <= 0.0f)
        {
            s_altitude_runtime.qnh_equiv_filt_hpa = qnh_equiv_hpa;
        }
        else
        {
            s_altitude_runtime.qnh_equiv_filt_hpa = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.qnh_equiv_filt_hpa,
                                                                           qnh_equiv_hpa,
                                                                           settings->gps_bias_tau_ms,
                                                                           dt_s);
        }
    }
    else if (s_altitude_runtime.qnh_equiv_filt_hpa <= 0.0f)
    {
        s_altitude_runtime.qnh_equiv_filt_hpa = qnh_manual_hpa;
    }

    /* ------------------------------------------------------------------ */
    /*  filter init는 새 baro altitude가 생겼을 때만 수행한다.               */
    /* ------------------------------------------------------------------ */
    if (new_baro_sample != false)
    {
        APP_ALTITUDE_EnsureFiltersInitialized(alt_qnh_cm, alt_gps_cm, gps_valid);
        g_app_state.altitude.baro_valid = true;
        g_app_state.altitude.last_baro_update_ms = now_ms;
    }

    /* ------------------------------------------------------------------ */
    /*  IMU path                                                            */
    /*  - 6-axis complementary gravity estimator 사용                       */
    /*  - trust가 낮으면 KF4 입력 가속도를 자동으로 줄인다.                 */
    /* ------------------------------------------------------------------ */
    imu_vertical_cms2 = APP_ALTITUDE_UpdateImuVerticalAccelCms2(settings,
                                                               mpu,
                                                               now_ms,
                                                               dt_s,
                                                               &imu_vector_valid);

    imu_predict_weight = APP_ALTITUDE_ClampF(s_altitude_runtime.imu_predict_weight_permille * 0.001f, 0.0f, 1.0f);

    /* ------------------------------------------------------------------ */
    /*  APP_ALTITUDE_UpdateImuVerticalAccelCms2() 내부에서                  */
    /*  vertical dynamic acceleration에는 이미 predict_weight가 한 번       */
    /*  곱해져 있다.                                                        */
    /*                                                                    */
    /*  여기서 다시 한 번 곱하면 near-zero 구간에서                         */
    /*  trust가 출렁일 때 가속도 입력이 과도하게 왜곡되고,                   */
    /*  stationary burst tuning 감각도 나빠진다.                            */
    /*                                                                    */
    /*  따라서 KF4 predict에는                                              */
    /*  "이미 trust-weighted 된 imu_vertical_cms2" 를 그대로 넣고,          */
    /*  대신 measurement/process noise 쪽만 trust에 따라 키운다.             */
    /* ------------------------------------------------------------------ */
    imu_predict_cms2 = imu_vertical_cms2;

    imu_predict_noise_cms2 = (float)settings->imu_measurement_noise_cms2;
    imu_predict_noise_cms2 *= (1.0f + ((1.0f - imu_predict_weight) * 2.5f));
    imu_predict_noise_cms2 = APP_ALTITUDE_ClampF(imu_predict_noise_cms2, 5.0f, 10000.0f);

    /* ------------------------------------------------------------------ */
    /*  rest-aware baro velocity tuning                                    */
    /*  - regression slope 자체는 이미 raw diff보다 훨씬 낫지만               */
    /*    0 근처 / stationary 구간에서는 더 보수적으로 다룬다.               */
    /*  - ZUPT와 동일한 stationarity rule을 먼저 한 번 보고                  */
    /*    near-zero velocity observation을 더 덜 믿는다.                     */
    /* ------------------------------------------------------------------ */
    baro_rest_hint = APP_ALTITUDE_IsCoreRestActive(settings,
                                                    s_altitude_runtime.baro_vario_filt_cms,
                                                    imu_vertical_cms2,
                                                    imu_vector_valid,
                                                    (uint16_t)APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.imu_attitude_trust_permille));

    baro_velocity_meas_cms = APP_ALTITUDE_AdjustBaroVelocityMeasurementNearRest(settings,
                                                                                 baro_velocity_meas_cms,
                                                                                 baro_rest_hint);
    baro_velocity_noise_cms = APP_ALTITUDE_AdjustBaroVelocityNoiseNearRest(settings,
                                                                           baro_velocity_noise_cms,
                                                                           baro_velocity_meas_cms,
                                                                           baro_rest_hint);

    if (s_altitude_runtime.kf_noimu.valid != false)
    {
        APP_ALTITUDE_Kf3_Predict(s_altitude_runtime.kf_noimu.x,
                                 s_altitude_runtime.kf_noimu.P,
                                 dt_s,
                                 settings);
    }

    if (s_altitude_runtime.kf_imu.valid != false)
    {
        APP_ALTITUDE_Kf4_Predict(s_altitude_runtime.kf_imu.x,
                                 s_altitude_runtime.kf_imu.P,
                                 dt_s,
                                 imu_predict_cms2,
                                 imu_predict_noise_cms2,
                                 settings);
    }

    H_baro3[0] = 1.0f; H_baro3[1] = 0.0f; H_baro3[2] = 1.0f;
    H_gps3[0]  = 1.0f; H_gps3[1]  = 0.0f; H_gps3[2]  = 0.0f;
    H_vel3[0]  = 0.0f; H_vel3[1]  = 1.0f; H_vel3[2]  = 0.0f;
    H_zero_vel3[0] = 0.0f; H_zero_vel3[1] = 1.0f; H_zero_vel3[2] = 0.0f;

    H_baro4[0] = 1.0f; H_baro4[1] = 0.0f; H_baro4[2] = 1.0f; H_baro4[3] = 0.0f;
    H_gps4[0]  = 1.0f; H_gps4[1]  = 0.0f; H_gps4[2]  = 0.0f; H_gps4[3]  = 0.0f;
    H_vel4[0]  = 0.0f; H_vel4[1]  = 1.0f; H_vel4[2]  = 0.0f; H_vel4[3]  = 0.0f;
    H_zero_vel4[0] = 0.0f; H_zero_vel4[1] = 1.0f; H_zero_vel4[2] = 0.0f; H_zero_vel4[3] = 0.0f;

    /* ------------------------------------------------------------------ */
    /*  baro altitude update                                               */
    /*  - adaptive R 사용                                                   */
    /*  - residual gate로 말도 안 되는 spike는 걸러낸다.                    */
    /* ------------------------------------------------------------------ */
    if ((new_baro_sample != false) && (s_altitude_runtime.kf_noimu.valid != false))
    {
        baro_altitude_residual_cm = alt_qnh_cm - (s_altitude_runtime.kf_noimu.x[0] + s_altitude_runtime.kf_noimu.x[2]);

        if (APP_ALTITUDE_IsResidualAccepted(baro_altitude_residual_cm,
                                            baro_noise_cm,
                                            APP_ALTITUDE_BARO_ALTITUDE_GATE_SIGMA,
                                            APP_ALTITUDE_BARO_ALTITUDE_GATE_FLOOR_CM) != false)
        {
            APP_ALTITUDE_Kf3_UpdateScalar(s_altitude_runtime.kf_noimu.x,
                                          s_altitude_runtime.kf_noimu.P,
                                          H_baro3,
                                          alt_qnh_cm,
                                          APP_ALTITUDE_SquareF(baro_noise_cm));
        }
    }

    if ((new_baro_sample != false) && (s_altitude_runtime.kf_imu.valid != false))
    {
        baro_altitude_residual_cm = alt_qnh_cm - (s_altitude_runtime.kf_imu.x[0] + s_altitude_runtime.kf_imu.x[2]);

        if (APP_ALTITUDE_IsResidualAccepted(baro_altitude_residual_cm,
                                            baro_noise_cm,
                                            APP_ALTITUDE_BARO_ALTITUDE_GATE_SIGMA,
                                            APP_ALTITUDE_BARO_ALTITUDE_GATE_FLOOR_CM) != false)
        {
            APP_ALTITUDE_Kf4_UpdateScalar(s_altitude_runtime.kf_imu.x,
                                          s_altitude_runtime.kf_imu.P,
                                          H_baro4,
                                          alt_qnh_cm,
                                          APP_ALTITUDE_SquareF(baro_noise_cm));
        }
    }

    /* ------------------------------------------------------------------ */
    /*  baro velocity observation update                                   */
    /*  - altitude difference로 만든 derivative를 별도 velocity 측정처럼     */
    /*    사용한다.                                                         */
    /*  - no-IMU filter의 반응성을 끌어올리고,                              */
    /*    IMU filter에도 velocity anchor를 하나 더 준다.                    */
    /* ------------------------------------------------------------------ */
    if ((new_baro_sample != false) && (baro_velocity_noise_cms > 0.0f))
    {
        if (s_altitude_runtime.kf_noimu.valid != false)
        {
            if (APP_ALTITUDE_IsResidualAccepted(baro_velocity_meas_cms - s_altitude_runtime.kf_noimu.x[1],
                                                baro_velocity_noise_cms,
                                                APP_ALTITUDE_BARO_VELOCITY_GATE_SIGMA,
                                                APP_ALTITUDE_BARO_VELOCITY_GATE_FLOOR_CMS) != false)
            {
                APP_ALTITUDE_Kf3_UpdateScalar(s_altitude_runtime.kf_noimu.x,
                                              s_altitude_runtime.kf_noimu.P,
                                              H_vel3,
                                              baro_velocity_meas_cms,
                                              APP_ALTITUDE_SquareF(baro_velocity_noise_cms));
            }
        }

        if (s_altitude_runtime.kf_imu.valid != false)
        {
            if (APP_ALTITUDE_IsResidualAccepted(baro_velocity_meas_cms - s_altitude_runtime.kf_imu.x[1],
                                                baro_velocity_noise_cms,
                                                APP_ALTITUDE_BARO_VELOCITY_GATE_SIGMA,
                                                APP_ALTITUDE_BARO_VELOCITY_GATE_FLOOR_CMS) != false)
            {
                APP_ALTITUDE_Kf4_UpdateScalar(s_altitude_runtime.kf_imu.x,
                                              s_altitude_runtime.kf_imu.P,
                                              H_vel4,
                                              baro_velocity_meas_cms,
                                              APP_ALTITUDE_SquareF(baro_velocity_noise_cms));
            }
        }
    }

    /* ------------------------------------------------------------------ */
    /*  GPS absolute altitude update                                       */
    /*  - GPS 품질 gate + innovation gate                                  */
    /*  - 상용 EKF 철학처럼 GPS는 slow absolute anchor 역할                 */
    /* ------------------------------------------------------------------ */
    if ((new_gps_sample != false) && (gps_valid != false) && (settings->gps_bias_correction_enabled != 0u))
    {
        gps_noise_cm = APP_ALTITUDE_ComputeGpsMeasurementNoiseCm(settings, gps_fix);

        if (s_altitude_runtime.kf_noimu.valid != false)
        {
            if (APP_ALTITUDE_IsResidualAccepted(alt_gps_cm - s_altitude_runtime.kf_noimu.x[0],
                                                gps_noise_cm,
                                                APP_ALTITUDE_GPS_ALTITUDE_GATE_SIGMA,
                                                APP_ALTITUDE_GPS_ALTITUDE_GATE_FLOOR_CM) != false)
            {
                APP_ALTITUDE_Kf3_UpdateScalar(s_altitude_runtime.kf_noimu.x,
                                              s_altitude_runtime.kf_noimu.P,
                                              H_gps3,
                                              alt_gps_cm,
                                              APP_ALTITUDE_SquareF(gps_noise_cm));
            }
        }

        if (s_altitude_runtime.kf_imu.valid != false)
        {
            if (APP_ALTITUDE_IsResidualAccepted(alt_gps_cm - s_altitude_runtime.kf_imu.x[0],
                                                gps_noise_cm,
                                                APP_ALTITUDE_GPS_ALTITUDE_GATE_SIGMA,
                                                APP_ALTITUDE_GPS_ALTITUDE_GATE_FLOOR_CM) != false)
            {
                APP_ALTITUDE_Kf4_UpdateScalar(s_altitude_runtime.kf_imu.x,
                                              s_altitude_runtime.kf_imu.P,
                                              H_gps4,
                                              alt_gps_cm,
                                              APP_ALTITUDE_SquareF(gps_noise_cm));
            }
        }

        g_app_state.altitude.last_gps_update_ms = now_ms;
    }

    /* ------------------------------------------------------------------ */
    /*  ZUPT (Zero Velocity Update)                                        */
    /*  - stationary로 판단되면 v=0 pseudo-measurement를 넣는다.            */
    /*  - IMU burst 억제와 실내 정지 안정화에 특히 효과가 크다.             */
    /* ------------------------------------------------------------------ */
    core_rest_active = baro_rest_hint;

    s_altitude_runtime.zupt_active = core_rest_active;

    if (core_rest_active != false)
    {
        if (s_altitude_runtime.kf_noimu.valid != false)
        {
            APP_ALTITUDE_Kf3_UpdateScalar(s_altitude_runtime.kf_noimu.x,
                                          s_altitude_runtime.kf_noimu.P,
                                          H_zero_vel3,
                                          0.0f,
                                          APP_ALTITUDE_SquareF(APP_ALTITUDE_ZUPT_VELOCITY_NOISE_CMS));
        }

        if (s_altitude_runtime.kf_imu.valid != false)
        {
            APP_ALTITUDE_Kf4_UpdateScalar(s_altitude_runtime.kf_imu.x,
                                          s_altitude_runtime.kf_imu.P,
                                          H_zero_vel4,
                                          0.0f,
                                          APP_ALTITUDE_SquareF(APP_ALTITUDE_ZUPT_VELOCITY_NOISE_CMS));
        }
    }

    if ((settings->auto_home_capture_enabled != 0u) &&
        (alt_prev->home_valid == false) &&
        (s_altitude_runtime.kf_noimu.valid != false))
    {
        s_altitude_runtime.home_capture_request = true;
    }

    APP_ALTITUDE_ApplyPendingActions((alt_prev->baro_valid != false) ? alt_qnh_cm : 0.0f);

    if (s_altitude_runtime.kf_noimu.valid != false)
    {
        /* ------------------------------------------------------------------ */
        /*  전통적인 baro/GPS backed vario                                     */
        /*                                                                    */
        /*  이 경로는 INS와 무관하게 항상 유지된다.                             */
        /*  즉, 상용 variometer가 갖는                                          */
        /*  "조금 느려도 진실에 가까운 전통 baro vario" 역할을 맡는다.         */
        /* ------------------------------------------------------------------ */
        s_altitude_runtime.vario_fast_noimu_cms = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.vario_fast_noimu_cms,
                                                                         s_altitude_runtime.kf_noimu.x[1],
                                                                         settings->vario_fast_tau_ms,
                                                                         dt_s);
        s_altitude_runtime.vario_slow_noimu_cms = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.vario_slow_noimu_cms,
                                                                         s_altitude_runtime.vario_fast_noimu_cms,
                                                                         settings->vario_slow_tau_ms,
                                                                         dt_s);
    }

    /* ---------------------------------------------------------------------- */
    /*  sensor-fusion vario blend                                              */
    /*                                                                          */
    /*  중요한 재배선                                                          */
    /*  - IMU on/off 는 altitude source 와 분리된다.                           */
    /*  - vario_fast/slow_imu 는 이제 "raw IMU velocity" 가 아니라             */
    /*    baro/no-IMU anchor 와 quaternion INS 를 연속 confidence 로 섞은      */
    /*    sensor-fusion vario 출력이 된다.                                     */
    /*                                                                          */
    /*  따라서 상위 VARIO 앱은 altitude source 와 무관하게                     */
    /*  이 fused vario 경로만 읽으면 된다.                                     */
    /* ---------------------------------------------------------------------- */
    imu_anchor_velocity_cms = s_altitude_runtime.kf_noimu.valid ?
                              s_altitude_runtime.kf_noimu.x[1] :
                              baro_velocity_meas_cms;
    imu_fast_target_cms = imu_anchor_velocity_cms;
    imu_slow_target_cms = imu_anchor_velocity_cms;
    s_altitude_runtime.imu_blend_weight_permille = 0.0f;

    if ((settings->imu_aid_enabled != 0u) &&
        (imu_input_enabled != false) &&
        (imu_vector_valid != false) &&
        (s_altitude_runtime.kf_imu.valid != false))
    {
        imu_velocity_disagreement_cms = fabsf(s_altitude_runtime.kf_imu.x[1] - imu_anchor_velocity_cms);
        s_altitude_runtime.imu_vario_disagree_lp_cms = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.imu_vario_disagree_lp_cms,
                                                                              imu_velocity_disagreement_cms,
                                                                              APP_ALTITUDE_IMU_BLEND_DISAGREE_TAU_MS,
                                                                              dt_s);

        imu_blend_fast = APP_ALTITUDE_Clamp01F(1.0f -
                                               (s_altitude_runtime.imu_vario_disagree_lp_cms /
                                                APP_ALTITUDE_IMU_BLEND_FAST_DIFF_CMS));
        imu_blend_slow = APP_ALTITUDE_Clamp01F(1.0f -
                                               (s_altitude_runtime.imu_vario_disagree_lp_cms /
                                                APP_ALTITUDE_IMU_BLEND_SLOW_DIFF_CMS));

        imu_blend_fast *= imu_predict_weight;
        imu_blend_slow *= imu_predict_weight;

        imu_blend_fast = sqrtf(APP_ALTITUDE_Clamp01F(imu_blend_fast));
        imu_blend_slow = APP_ALTITUDE_Clamp01F(imu_blend_slow);

        imu_fast_target_cms = imu_anchor_velocity_cms +
                              (imu_blend_fast * (s_altitude_runtime.kf_imu.x[1] - imu_anchor_velocity_cms));
        imu_slow_target_cms = imu_anchor_velocity_cms +
                              (imu_blend_slow * (s_altitude_runtime.kf_imu.x[1] - imu_anchor_velocity_cms));

        s_altitude_runtime.imu_blend_weight_permille = imu_blend_fast * 1000.0f;
    }
    else
    {
        s_altitude_runtime.imu_vario_disagree_lp_cms = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.imu_vario_disagree_lp_cms,
                                                                              0.0f,
                                                                              APP_ALTITUDE_IMU_BLEND_DISAGREE_TAU_MS,
                                                                              dt_s);
    }

    s_altitude_runtime.vario_fast_imu_cms = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.vario_fast_imu_cms,
                                                                   imu_fast_target_cms,
                                                                   settings->vario_fast_tau_ms,
                                                                   dt_s);
    s_altitude_runtime.vario_slow_imu_cms = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.vario_slow_imu_cms,
                                                                   imu_slow_target_cms,
                                                                   settings->vario_slow_tau_ms,
                                                                   dt_s);

    /* ---------------------------------------------------------------------- */
    /*  display altitude source 정리                                           */
    /*                                                                          */
    /*  display/QNH 숫자는 INS가 아니라 manual QNH 기반 barometric altitude 를 */
    /*  사용한다.                                                              */
    /*                                                                          */
    /*  다만 Alt1이 법적 primary altitude 인 만큼,                              */
    /*  fused/GPS로 우회하지는 않되 baro/QNH path의 순간적인 비현실 step 은     */
    /*  마지막으로 accept 된 barometric target 에서 한 번 막아 준다.           */
    /*                                                                          */
    /*  중요한 점                                                              */
    /*  - raw/fused internal state는 그대로 유지한다.                          */
    /*  - Alt1용 display path만 spike 를 무시한다.                             */
    /* ---------------------------------------------------------------------- */
    display_baro_available = (new_baro_sample != false) || (alt_prev->baro_valid != false);
    display_baro_basis_changed =
        (s_altitude_runtime.display_gate_qnh_hpa_x100 != settings->manual_qnh_hpa_x100) ||
        (s_altitude_runtime.display_gate_pressure_correction_hpa_x100 !=
         settings->pressure_correction_hpa_x100);

    if (display_baro_basis_changed != false)
    {
        s_altitude_runtime.display_baro_gate_valid = false;
    }

    if (display_baro_available != false)
    {
        if (s_altitude_runtime.display_baro_gate_valid == false)
        {
            s_altitude_runtime.display_baro_gate_ref_cm = alt_qnh_cm;
            s_altitude_runtime.display_baro_gate_valid = true;
            display_baro_target_accepted = true;
        }
        else
        {
            display_baro_target_accepted = APP_ALTITUDE_IsResidualAccepted(alt_qnh_cm -
                                                                           s_altitude_runtime.display_baro_gate_ref_cm,
                                                                           baro_noise_cm,
                                                                           APP_ALTITUDE_BARO_ALTITUDE_GATE_SIGMA,
                                                                           APP_ALTITUDE_BARO_ALTITUDE_GATE_FLOOR_CM);

            if (display_baro_target_accepted != false)
            {
                s_altitude_runtime.display_baro_gate_ref_cm = alt_qnh_cm;
            }
        }

        display_target_cm = (display_baro_target_accepted != false) ?
                            alt_qnh_cm :
                            s_altitude_runtime.display_baro_gate_ref_cm;
    }
    else
    {
        display_target_cm = s_altitude_runtime.kf_noimu.valid ? s_altitude_runtime.kf_noimu.x[0] :
                                                                alt_qnh_cm;
    }

    display_source_vario_cms = s_altitude_runtime.vario_slow_noimu_cms;

    display_rest_active = APP_ALTITUDE_IsDisplayRestActive(settings,
                                                           display_source_vario_cms,
                                                           (imu_predict_weight > 0.35f) ? imu_vertical_cms2 : 0.0f,
                                                           (imu_predict_weight > 0.35f) ? imu_vector_valid : false);

    if (display_rest_active != false)
    {
        display_tau_ms = settings->rest_display_tau_ms;

        if (fabsf(display_target_cm - s_altitude_runtime.display_alt_filt_cm) <
            (float)settings->rest_display_hold_cm)
        {
            display_target_cm = s_altitude_runtime.display_alt_filt_cm;
        }
    }

    /* ------------------------------------------------------------------ */
    /*  stage-1 display LPF                                                 */
    /*                                                                      */
    /*  fused altitude를 바로 UI에 던지지 않고                               */
    /*  먼저 기존 display_lpf / rest_display_lpf 한 번을 통과시킨다.        */
    /* ------------------------------------------------------------------ */
    s_altitude_runtime.display_alt_filt_cm = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.display_alt_filt_cm,
                                                                    display_target_cm,
                                                                    display_tau_ms,
                                                                    dt_s);

    /* ------------------------------------------------------------------ */
    /*  stage-2 product display presentation                               */
    /*                                                                      */
    /*  상용기 느낌의 핵심은 여기다.                                        */
    /*                                                                      */
    /*  - 정지/미세 vario 구간에서는 숫자를 더 끈끈하게 붙잡고               */
    /*  - 이동이 분명해지면 다시 빠르게 따라가며                            */
    /*  - 최종 숫자는 작은 hysteresis step을 통해                           */
    /*    덜 "솔직하게" 보이도록 만든다.                                   */
    /*                                                                      */
    /*  중요                                                                */
    /*  - alt_fused_noimu_cm / alt_fused_imu_cm 는 계속 honest 하다.        */
    /*  - UI가 실제로 읽어야 할 값은 alt_display_cm 이다.                    */
    /* ------------------------------------------------------------------ */
    s_altitude_runtime.display_alt_present_cm = APP_ALTITUDE_UpdateProductDisplayAltitudeCm(settings,
                                                                                             s_altitude_runtime.display_alt_filt_cm,
                                                                                             display_source_vario_cms,
                                                                                             display_rest_active,
                                                                                             dt_s);

    if (alt_prev->home_valid != false)
    {
        g_app_state.altitude.alt_rel_home_noimu_cm =
            APP_ALTITUDE_RoundFloatToS32((s_altitude_runtime.kf_noimu.valid ? s_altitude_runtime.kf_noimu.x[0] : alt_qnh_cm) -
                                         s_altitude_runtime.home_noimu_cm);

        g_app_state.altitude.alt_rel_home_imu_cm =
            APP_ALTITUDE_RoundFloatToS32((s_altitude_runtime.kf_imu.valid ? s_altitude_runtime.kf_imu.x[0] : alt_qnh_cm) -
                                         s_altitude_runtime.home_imu_cm);
    }
    else
    {
        g_app_state.altitude.alt_rel_home_noimu_cm = 0;
        g_app_state.altitude.alt_rel_home_imu_cm = 0;
    }

    s_altitude_runtime.display_gate_qnh_hpa_x100 = settings->manual_qnh_hpa_x100;
    s_altitude_runtime.display_gate_pressure_correction_hpa_x100 =
        settings->pressure_correction_hpa_x100;

    g_app_state.altitude.initialized                = true;
    g_app_state.altitude.baro_valid                 = (new_baro_sample != false) || (alt_prev->baro_valid != false);
    g_app_state.altitude.gps_valid                  = gps_valid;
    g_app_state.altitude.home_valid                 = (g_app_state.altitude.home_valid != false) || (alt_prev->home_valid != false);
    g_app_state.altitude.imu_vector_valid           = imu_vector_valid;
    g_app_state.altitude.gps_quality_permille       = gps_quality_permille;
    g_app_state.altitude.last_update_ms             = now_ms;

    g_app_state.altitude.pressure_raw_hpa_x100      = APP_ALTITUDE_RoundFloatToS32(pressure_raw_hpa * 100.0f);
    g_app_state.altitude.pressure_prefilt_hpa_x100  = APP_ALTITUDE_RoundFloatToS32(pressure_prefilt_hpa * 100.0f);
    g_app_state.altitude.pressure_filt_hpa_x100     = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.pressure_filt_hpa * 100.0f);
    g_app_state.altitude.pressure_residual_hpa_x100 = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.pressure_residual_hpa * 100.0f);

    g_app_state.altitude.qnh_manual_hpa_x100        = settings->manual_qnh_hpa_x100;
    g_app_state.altitude.qnh_equiv_gps_hpa_x100     = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.qnh_equiv_filt_hpa * 100.0f);

    g_app_state.altitude.alt_pressure_std_cm        = APP_ALTITUDE_RoundFloatToS32(alt_std_cm);
    g_app_state.altitude.alt_qnh_manual_cm          = APP_ALTITUDE_RoundFloatToS32(alt_qnh_cm);
    g_app_state.altitude.alt_gps_hmsl_cm            = APP_ALTITUDE_RoundFloatToS32(alt_gps_cm);
    g_app_state.altitude.alt_fused_noimu_cm         = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.kf_noimu.valid ? s_altitude_runtime.kf_noimu.x[0] : alt_qnh_cm);
    g_app_state.altitude.alt_fused_imu_cm           = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.kf_imu.valid ? s_altitude_runtime.kf_imu.x[0] : alt_qnh_cm);
    /* ------------------------------------------------------------------ */
    /*  alt_display_cm                                                       */
    /*                                                                      */
    /*  UI 숫자 표시용 altitude다.                                            */
    /*  메인 비행 화면 / 디버그 altitude 화면의 숫자 영역은                   */
    /*  이 값을 사용해야 상용기처럼 조금 더 차분한 표시를 얻는다.             */
    /* ------------------------------------------------------------------ */
    g_app_state.altitude.alt_display_cm             = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.display_alt_present_cm);

    g_app_state.altitude.home_alt_noimu_cm          = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.home_noimu_cm);
    g_app_state.altitude.home_alt_imu_cm            = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.home_imu_cm);

    g_app_state.altitude.baro_bias_noimu_cm         = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.kf_noimu.valid ? s_altitude_runtime.kf_noimu.x[2] : 0.0f);
    g_app_state.altitude.baro_bias_imu_cm           = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.kf_imu.valid ? s_altitude_runtime.kf_imu.x[2] : 0.0f);
    g_app_state.altitude.baro_noise_used_cm         = (uint16_t)APP_ALTITUDE_RoundFloatToS32(baro_noise_cm);
    g_app_state.altitude.display_rest_active        = display_rest_active ? 1u : 0u;
    g_app_state.altitude.zupt_active                = s_altitude_runtime.zupt_active ? 1u : 0u;
    g_app_state.altitude.debug_audio_source         = ((settings->debug_audio_source != 0u) &&
                                                       (imu_input_enabled != false)) ? 1u : 0u;

    g_app_state.altitude.vario_fast_noimu_cms       = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.vario_fast_noimu_cms);
    g_app_state.altitude.vario_slow_noimu_cms       = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.vario_slow_noimu_cms);
    g_app_state.altitude.vario_fast_imu_cms         = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.vario_fast_imu_cms);
    g_app_state.altitude.vario_slow_imu_cms         = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.vario_slow_imu_cms);
    g_app_state.altitude.baro_vario_raw_cms         = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.baro_vario_raw_cms);
    g_app_state.altitude.baro_vario_filt_cms        = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.baro_vario_filt_cms);

    g_app_state.altitude.grade_noimu_x10            = APP_ALTITUDE_ComputeGradeX10(s_altitude_runtime.vario_slow_noimu_cms, gps_fix);
    g_app_state.altitude.grade_imu_x10              = APP_ALTITUDE_ComputeGradeX10(s_altitude_runtime.vario_slow_imu_cms, gps_fix);

    g_app_state.altitude.imu_vertical_accel_mg      = APP_ALTITUDE_RoundFloatToS32((imu_vertical_cms2 / (APP_ALTITUDE_GRAVITY_MPS2 * 100.0f)) * 1000.0f);
    g_app_state.altitude.imu_vertical_accel_cms2    = APP_ALTITUDE_RoundFloatToS32(imu_vertical_cms2);
    g_app_state.altitude.imu_gravity_norm_mg        = s_altitude_runtime.gravity_est_valid ? 1000 : 0;
    g_app_state.altitude.imu_accel_norm_mg          = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.imu_accel_norm_mg);
    g_app_state.altitude.imu_attitude_trust_permille = (uint16_t)APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.imu_attitude_trust_permille);
    g_app_state.altitude.imu_predict_weight_permille = (uint16_t)APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.imu_predict_weight_permille);

    g_app_state.altitude.gps_vacc_mm                = gps_fix->vAcc;
    g_app_state.altitude.gps_pdop_x100              = gps_fix->pDOP;
    g_app_state.altitude.gps_numsv_used             = gps_fix->numSV_used;
    g_app_state.altitude.gps_fix_type               = gps_fix->fixType;

    APP_ALTITUDE_HandleDebugAudio(now_ms, settings);

    /* ------------------------------------------------------------------ */
    /*  low-level multi-unit bank refresh                                  */
    /*                                                                    */
    /*  canonical metric slice와 debug-audio source까지 모두 확정한 뒤     */
    /*  같은 task 안에서 parallel metric / imperial bank를 갱신한다.       */
    /*  상위 계층은 이 bank를 선택만 하고 다시 환산하지 않는다.            */
    /* ------------------------------------------------------------------ */
    APP_ALTITUDE_PopulateLowLevelUnitBank((app_altitude_state_t *)&g_app_state.altitude);
}

void APP_ALTITUDE_DebugSetUiActive(bool active, uint32_t now_ms)
{
    (void)now_ms;
    s_altitude_runtime.ui_active = active;

    if (active == false)
    {
        s_altitude_runtime.audio_vario_override_valid = false;
        g_app_state.altitude.debug_audio_active = 0u;
        APP_ALTITUDE_StopOwnedAudioIfNeeded();
    }
}

void APP_ALTITUDE_DebugSetAudioVarioOverride(bool valid,
                                             int32_t vario_cms,
                                             uint32_t now_ms)
{
    s_altitude_runtime.audio_vario_override_valid = valid;
    s_altitude_runtime.audio_vario_override_cms = (float)vario_cms;
    s_altitude_runtime.audio_vario_override_ms = now_ms;
}

void APP_ALTITUDE_DebugRequestHomeCapture(void)
{
    s_altitude_runtime.home_capture_request = true;
}

void APP_ALTITUDE_DebugRequestBiasRezero(void)
{
    s_altitude_runtime.bias_rezero_request = true;
}
