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
#define APP_ALTITUDE_BARO_VARIO_NOISE_MAX_SCALE 5.0f
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
    float pressure_filt_hpa;           /* 1차 LPF pressure                        */
    float pressure_residual_hpa;       /* prefilt - filt residual                 */
    float pressure_hist_hpa[3];        /* median-3 원본 history                   */
    uint8_t pressure_hist_count;       /* history 유효 개수                       */
    uint8_t pressure_hist_index;       /* history ring index                      */

    float baro_residual_lp_cm;         /* adaptive R envelope용 residual LPF      */
    float adaptive_baro_noise_cm;      /* 현재 실제 사용된 adaptive baro noise    */
    float qnh_equiv_filt_hpa;          /* GPS anchor 기반 equivalent QNH LPF      */
    float display_alt_filt_cm;         /* 최종 표시 altitude LPF                  */

    float baro_alt_prev_cm;            /* baro altitude derivative 이전 샘플      */
    bool  baro_alt_prev_valid;         /* derivative 이전 샘플 유효 여부          */
    float baro_vario_raw_cms;          /* baro altitude raw derivative            */
    float baro_vario_filt_cms;         /* velocity measurement용 LPF derivative   */

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
    float imu_attitude_trust_permille; /* accel norm 기반 attitude trust 0..1000  */
    float imu_predict_weight_permille; /* KF4 predict에 실제 적용된 weight        */

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

static bool APP_ALTITUDE_NormalizeVec3(float *x, float *y, float *z)
{
    float norm;

    if ((x == 0) || (y == 0) || (z == 0))
    {
        return false;
    }

    norm = APP_ALTITUDE_VectorNorm3F(*x, *y, *z);
    if (norm < 0.000001f)
    {
        return false;
    }

    *x /= norm;
    *y /= norm;
    *z /= norm;
    return true;
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

static float APP_ALTITUDE_UpdateBaroVarioMeasurement(const app_altitude_settings_t *settings,
                                                     float baro_alt_cm,
                                                     float baro_dt_s)
{
    float raw_cms;

    if (settings == 0)
    {
        return 0.0f;
    }

    if ((s_altitude_runtime.baro_alt_prev_valid == false) || (baro_dt_s <= 0.0f))
    {
        s_altitude_runtime.baro_alt_prev_cm = baro_alt_cm;
        s_altitude_runtime.baro_alt_prev_valid = true;
        s_altitude_runtime.baro_vario_raw_cms = 0.0f;
        s_altitude_runtime.baro_vario_filt_cms = 0.0f;
        return 0.0f;
    }

    raw_cms = (baro_alt_cm - s_altitude_runtime.baro_alt_prev_cm) / baro_dt_s;
    raw_cms = APP_ALTITUDE_ClampF(raw_cms,
                                  -APP_ALTITUDE_BARO_VARIO_CLIP_CMS,
                                  APP_ALTITUDE_BARO_VARIO_CLIP_CMS);

    s_altitude_runtime.baro_alt_prev_cm = baro_alt_cm;
    s_altitude_runtime.baro_vario_raw_cms = raw_cms;
    s_altitude_runtime.baro_vario_filt_cms = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.baro_vario_filt_cms,
                                                                    raw_cms,
                                                                    settings->baro_vario_lpf_tau_ms,
                                                                    baro_dt_s);
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

    adaptive_noise_cms = base_noise_cms +
                         (spread_cms * APP_ALTITUDE_BARO_VARIO_NOISE_SPREAD_GAIN) +
                         (residual_cms * APP_ALTITUDE_BARO_VARIO_NOISE_RESIDUAL_GAIN);

    max_noise_cms = base_noise_cms * APP_ALTITUDE_BARO_VARIO_NOISE_MAX_SCALE;
    adaptive_noise_cms = APP_ALTITUDE_ClampF(adaptive_noise_cms,
                                             base_noise_cms,
                                             APP_ALTITUDE_ClampF(max_noise_cms, base_noise_cms, 500.0f));

    return adaptive_noise_cms;
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
    /*  IMU가 의도적으로 꺼졌거나 stale sample만 남아 있는 경우                 */
    /*  gravity / trust / predict weight를 모두 0으로 내려                     */
    /*  stale acceleration이 KF4 predict에 반복 주입되는 일을 막는다.          */
    /* ---------------------------------------------------------------------- */
    s_altitude_runtime.gravity_est_valid = false;
    s_altitude_runtime.imu_vertical_lp_cms2 = 0.0f;
    s_altitude_runtime.imu_accel_norm_mg = 0.0f;
    s_altitude_runtime.imu_attitude_trust_permille = 0.0f;
    s_altitude_runtime.imu_predict_weight_permille = 0.0f;
}

static float APP_ALTITUDE_GetDebugAudioSourceVarioCms(const app_altitude_settings_t *settings)
{
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
/*  자세 센서/쿼터니언이 없는 현재 하드웨어에서는                              */
/*  accelerometer의 저주파 성분을 gravity vector로 보고,                       */
/*  현재 accel에서 gravity LPF를 뺀 뒤                                         */
/*  그 차이를 gravity 방향으로 projection 해서                                 */
/*  수직 specific-force를 근사한다.                                            */
/*                                                                            */
/*  장점                                                                       */
/*  - 기기 장착 방향이 꼭 Z-up이 아니어도                                      */
/*    정적 gravity 방향을 자동으로 따라간다.                                   */
/*                                                                            */
/*  주의                                                                       */
/*  - 급한 자세 변화 / 큰 횡가속이 많으면                                      */
/*    순수 AHRS보다 부정확할 수 있다.                                           */
/*  - 그래서 이 결과는 no-IMU 결과와 병렬로 함께 보관한다.                     */
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
    float ax_g;
    float ay_g;
    float az_g;
    float accel_norm_g;
    float accel_norm_mg;
    float a_hat_x;
    float a_hat_y;
    float a_hat_z;
    float g_pred_x;
    float g_pred_y;
    float g_pred_z;
    float gx_rad_s;
    float gy_rad_s;
    float gz_rad_s;
    float dg_x;
    float dg_y;
    float dg_z;
    float accel_gate_mg;
    float accel_error_mg;
    float attitude_trust;
    float correction_alpha;
    float sign_f;
    float predict_weight;
    float vertical_total_g;
    float vertical_dyn_g;
    float deadband_g;
    float clip_g;

    if (out_vector_valid != 0)
    {
        *out_vector_valid = false;
    }

    if ((settings == 0) || (mpu == 0) || (settings->imu_accel_lsb_per_g == 0u))
    {
        return 0.0f;
    }

    /* ------------------------------------------------------------------ */
    /*  settings에서 IMU input을 꺼 둔 경우                                   */
    /*  stale sample을 다시 쓰지 않도록 즉시 predict 입력을 0으로 만든다.     */
    /* ------------------------------------------------------------------ */
    if (APP_ALTITUDE_IsImuInputEnabled(settings) == false)
    {
        APP_ALTITUDE_ResetImuRuntimeForUnavailableInput();
        return 0.0f;
    }

    /* ------------------------------------------------------------------ */
    /*  bus 차단 / freeze / 통신 정지 등으로                                  */
    /*  MPU timestamp가 오래 멈춘 경우에도                                     */
    /*  마지막 accel 값을 매 task마다 재사용하지 않도록 stale guard를 둔다.   */
    /* ------------------------------------------------------------------ */
    if ((mpu->sample_count == 0u) ||
        (mpu->timestamp_ms == 0u) ||
        ((uint32_t)(now_ms - mpu->timestamp_ms) > APP_ALTITUDE_IMU_STALE_TIMEOUT_MS))
    {
        APP_ALTITUDE_ResetImuRuntimeForUnavailableInput();
        return 0.0f;
    }

    /* ------------------------------------------------------------------ */
    /*  같은 raw sample을 두 번 이상 적분하지 않도록 sample_count를 본다.   */
    /*  task loop가 sensor rate보다 빠른 경우에도                             */
    /*  실제 MPU sample cadence 기준으로만 IMU estimator가 전진한다.         */
    /* ------------------------------------------------------------------ */
    if ((mpu->sample_count != 0u) && (mpu->sample_count == s_altitude_runtime.last_mpu_sample_count))
    {
        if (out_vector_valid != 0)
        {
            *out_vector_valid = s_altitude_runtime.gravity_est_valid;
        }

        return s_altitude_runtime.imu_vertical_lp_cms2;
    }

    imu_dt_s = task_dt_s;
    if ((mpu->sample_count != 0u) &&
        (mpu->timestamp_ms != 0u) &&
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
    ax_g = ((float)mpu->accel_x_raw) / accel_lsb_per_g;
    ay_g = ((float)mpu->accel_y_raw) / accel_lsb_per_g;
    az_g = ((float)mpu->accel_z_raw) / accel_lsb_per_g;

    accel_norm_g = APP_ALTITUDE_VectorNorm3F(ax_g, ay_g, az_g);
    accel_norm_mg = accel_norm_g * 1000.0f;
    s_altitude_runtime.imu_accel_norm_mg = accel_norm_mg;

    if (accel_norm_g < 0.25f)
    {
        s_altitude_runtime.imu_attitude_trust_permille = 0.0f;
        s_altitude_runtime.imu_predict_weight_permille = 0.0f;
        s_altitude_runtime.gravity_est_valid = false;
        return 0.0f;
    }

    a_hat_x = ax_g / accel_norm_g;
    a_hat_y = ay_g / accel_norm_g;
    a_hat_z = az_g / accel_norm_g;

    if (s_altitude_runtime.gravity_est_valid == false)
    {
        /* -------------------------------------------------------------- */
        /*  첫 유효 샘플에서는 가속도 방향을 그대로 gravity estimate로 쓴다.*/
        /*  이후부터는 gyro predict + accel correction 구조로 넘어간다.    */
        /* -------------------------------------------------------------- */
        s_altitude_runtime.gravity_est_x = a_hat_x;
        s_altitude_runtime.gravity_est_y = a_hat_y;
        s_altitude_runtime.gravity_est_z = a_hat_z;
        s_altitude_runtime.gravity_est_valid = true;
    }
    else
    {
        g_pred_x = s_altitude_runtime.gravity_est_x;
        g_pred_y = s_altitude_runtime.gravity_est_y;
        g_pred_z = s_altitude_runtime.gravity_est_z;

        /* -------------------------------------------------------------- */
        /*  gyro 적분 prediction                                          */
        /*  g_body 는 body frame에서 본 중력 방향 unit vector이므로        */
        /*  dg/dt = -omega x g 형태로 전진한다.                             */
        /* -------------------------------------------------------------- */
        if (settings->imu_gyro_lsb_per_dps > 0u)
        {
            gyro_lsb_per_dps = (float)settings->imu_gyro_lsb_per_dps;

            gx_rad_s = (((float)mpu->gyro_x_raw) / gyro_lsb_per_dps) * ((float)M_PI / 180.0f);
            gy_rad_s = (((float)mpu->gyro_y_raw) / gyro_lsb_per_dps) * ((float)M_PI / 180.0f);
            gz_rad_s = (((float)mpu->gyro_z_raw) / gyro_lsb_per_dps) * ((float)M_PI / 180.0f);

            dg_x = -((gy_rad_s * g_pred_z) - (gz_rad_s * g_pred_y));
            dg_y = -((gz_rad_s * g_pred_x) - (gx_rad_s * g_pred_z));
            dg_z = -((gx_rad_s * g_pred_y) - (gy_rad_s * g_pred_x));

            g_pred_x += dg_x * imu_dt_s;
            g_pred_y += dg_y * imu_dt_s;
            g_pred_z += dg_z * imu_dt_s;

            if (APP_ALTITUDE_NormalizeVec3(&g_pred_x, &g_pred_y, &g_pred_z) == false)
            {
                g_pred_x = a_hat_x;
                g_pred_y = a_hat_y;
                g_pred_z = a_hat_z;
            }
        }

        /* -------------------------------------------------------------- */
        /*  accel correction trust                                         */
        /*  accel norm이 1g와 가까울수록 attitude correction을 허용한다.     */
        /*  큰 선형가속 / 충격 구간에서는 trust를 자동으로 줄여              */
        /*  accel이 gravity estimate를 오염시키지 못하게 한다.               */
        /* -------------------------------------------------------------- */
        accel_gate_mg = (float)settings->imu_attitude_accel_gate_mg;
        if (accel_gate_mg <= 1.0f)
        {
            accel_gate_mg = 1.0f;
        }

        accel_error_mg = fabsf(accel_norm_mg - APP_ALTITUDE_IMU_ACCEL_NORM_REF_MG);
        attitude_trust = 1.0f - (accel_error_mg / accel_gate_mg);
        attitude_trust = APP_ALTITUDE_Clamp01F(attitude_trust);

        correction_alpha = APP_ALTITUDE_LpfAlphaFromTauMs(settings->imu_gravity_tau_ms, imu_dt_s);
        correction_alpha *= attitude_trust;

        s_altitude_runtime.gravity_est_x = g_pred_x + (correction_alpha * (a_hat_x - g_pred_x));
        s_altitude_runtime.gravity_est_y = g_pred_y + (correction_alpha * (a_hat_y - g_pred_y));
        s_altitude_runtime.gravity_est_z = g_pred_z + (correction_alpha * (a_hat_z - g_pred_z));

        if (APP_ALTITUDE_NormalizeVec3(&s_altitude_runtime.gravity_est_x,
                                       &s_altitude_runtime.gravity_est_y,
                                       &s_altitude_runtime.gravity_est_z) == false)
        {
            s_altitude_runtime.gravity_est_x = a_hat_x;
            s_altitude_runtime.gravity_est_y = a_hat_y;
            s_altitude_runtime.gravity_est_z = a_hat_z;
        }
    }

    /* ------------------------------------------------------------------ */
    /*  predict weight                                                     */
    /*  - attitude trust가 일정 threshold 아래로 내려가면                   */
    /*    KF4 predict에 IMU 가속도를 거의 주지 않는다.                      */
    /*  - "IMU는 반응은 빠르지만, 믿을 수 있을 때만 센다" 라는 정책이다.     */
    /* ------------------------------------------------------------------ */
    if (s_altitude_runtime.gravity_est_valid != false)
    {
        s_altitude_runtime.imu_attitude_trust_permille =
            APP_ALTITUDE_ClampF((fabsf(accel_norm_mg - APP_ALTITUDE_IMU_ACCEL_NORM_REF_MG) <= (float)settings->imu_attitude_accel_gate_mg) ?
                                (1000.0f * (1.0f - (fabsf(accel_norm_mg - APP_ALTITUDE_IMU_ACCEL_NORM_REF_MG) /
                                                   APP_ALTITUDE_ClampF((float)settings->imu_attitude_accel_gate_mg, 1.0f, 2000.0f)))) :
                                0.0f,
                                0.0f,
                                1000.0f);
    }
    else
    {
        s_altitude_runtime.imu_attitude_trust_permille = 0.0f;
    }

    if (s_altitude_runtime.imu_attitude_trust_permille <= (float)settings->imu_predict_min_trust_permille)
    {
        predict_weight = 0.0f;
    }
    else
    {
        predict_weight = (s_altitude_runtime.imu_attitude_trust_permille -
                          (float)settings->imu_predict_min_trust_permille) /
                         APP_ALTITUDE_ClampF((1000.0f - (float)settings->imu_predict_min_trust_permille), 1.0f, 1000.0f);
        predict_weight = APP_ALTITUDE_Clamp01F(predict_weight);
    }

    s_altitude_runtime.imu_predict_weight_permille = predict_weight * 1000.0f;

    /* ------------------------------------------------------------------ */
    /*  gravity 방향 축으로 총 specific-force를 projection 하고             */
    /*  1g 정적 성분을 제거해서 vertical dynamic component만 남긴다.         */
    /* ------------------------------------------------------------------ */
    vertical_total_g = (ax_g * s_altitude_runtime.gravity_est_x) +
                       (ay_g * s_altitude_runtime.gravity_est_y) +
                       (az_g * s_altitude_runtime.gravity_est_z);

    sign_f = (settings->imu_vertical_sign >= 0) ? 1.0f : -1.0f;
    vertical_dyn_g = (vertical_total_g - 1.0f) * sign_f;

    deadband_g = ((float)settings->imu_vertical_deadband_mg) * 0.001f;
    clip_g     = ((float)settings->imu_vertical_clip_mg) * 0.001f;

    if (fabsf(vertical_dyn_g) < deadband_g)
    {
        vertical_dyn_g = 0.0f;
    }

    vertical_dyn_g = APP_ALTITUDE_ClampF(vertical_dyn_g, -clip_g, clip_g);

    /* ------------------------------------------------------------------ */
    /*  최종적으로 trust-based predict weight를 곱해                         */
    /*  "가끔 혼자 뚜우우욱!" 하던 stationary burst를 줄인다.               */
    /* ------------------------------------------------------------------ */
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

    if (s_altitude_runtime.display_alt_filt_cm == 0.0f)
    {
        s_altitude_runtime.display_alt_filt_cm = initial_alt_cm;
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
    bool imu_display_enabled;
    bool core_rest_active;
    uint16_t gps_quality_permille;
    float dt_s;
    float baro_dt_s;
    float pressure_raw_hpa;
    float pressure_prefilt_hpa;
    float qnh_manual_hpa;
    float qnh_equiv_hpa;
    float alt_std_cm;
    float alt_qnh_cm;
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
    float display_target_cm;
    float display_source_vario_cms;
    uint32_t display_tau_ms;
    bool display_rest_active;
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
    imu_display_enabled = false;
    core_rest_active = false;
    gps_quality_permille = 0u;

    pressure_raw_hpa = 0.0f;
    pressure_prefilt_hpa = s_altitude_runtime.pressure_prefilt_hpa;
    qnh_equiv_hpa = s_altitude_runtime.qnh_equiv_filt_hpa;
    alt_std_cm = 0.0f;
    alt_qnh_cm = 0.0f;
    alt_gps_cm = 0.0f;
    gps_noise_cm = 0.0f;
    baro_noise_cm = (float)settings->baro_measurement_noise_cm;
    baro_altitude_residual_cm = 0.0f;
    baro_velocity_meas_cms = s_altitude_runtime.baro_vario_filt_cms;
    imu_vertical_cms2 = 0.0f;
    imu_predict_weight = 0.0f;
    imu_predict_cms2 = 0.0f;
    imu_predict_noise_cms2 = (float)settings->imu_measurement_noise_cms2;
    display_target_cm = 0.0f;
    display_source_vario_cms = 0.0f;
    display_tau_ms = settings->display_lpf_tau_ms;
    display_rest_active = false;
    baro_dt_s = dt_s;

    qnh_manual_hpa = ((float)settings->manual_qnh_hpa_x100) * 0.01f;
    qnh_manual_hpa = APP_ALTITUDE_ClampF(qnh_manual_hpa, 800.0f, 1100.0f);

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
        pressure_prefilt_hpa = APP_ALTITUDE_UpdatePressurePrefilter(pressure_raw_hpa);

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

        alt_std_cm = APP_ALTITUDE_PressureToAltitudeMeters(s_altitude_runtime.pressure_filt_hpa,
                                                           APP_ALTITUDE_STD_QNH_HPA) * 100.0f;
        alt_qnh_cm = APP_ALTITUDE_PressureToAltitudeMeters(s_altitude_runtime.pressure_filt_hpa,
                                                           qnh_manual_hpa) * 100.0f;

        baro_noise_cm = APP_ALTITUDE_ComputeAdaptiveBaroNoiseCm(settings,
                                                                s_altitude_runtime.pressure_filt_hpa,
                                                                s_altitude_runtime.pressure_residual_hpa,
                                                                baro_dt_s);

        baro_velocity_meas_cms = APP_ALTITUDE_UpdateBaroVarioMeasurement(settings,
                                                                         alt_qnh_cm,
                                                                         baro_dt_s);
        baro_velocity_noise_cms = APP_ALTITUDE_ComputeAdaptiveBaroVarioNoiseCms(settings);
    }
    else if (alt_prev->baro_valid != false)
    {
        pressure_raw_hpa = ((float)alt_prev->pressure_raw_hpa_x100) * 0.01f;
        pressure_prefilt_hpa = ((float)alt_prev->pressure_prefilt_hpa_x100) * 0.01f;
        alt_std_cm = (float)alt_prev->alt_pressure_std_cm;
        alt_qnh_cm = (float)alt_prev->alt_qnh_manual_cm;
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
    core_rest_active = APP_ALTITUDE_IsCoreRestActive(settings,
                                                     s_altitude_runtime.baro_vario_filt_cms,
                                                     imu_vertical_cms2,
                                                     imu_vector_valid,
                                                     (uint16_t)APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.imu_attitude_trust_permille));

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
        s_altitude_runtime.vario_fast_noimu_cms = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.vario_fast_noimu_cms,
                                                                         s_altitude_runtime.kf_noimu.x[1],
                                                                         settings->vario_fast_tau_ms,
                                                                         dt_s);
        s_altitude_runtime.vario_slow_noimu_cms = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.vario_slow_noimu_cms,
                                                                         s_altitude_runtime.vario_fast_noimu_cms,
                                                                         settings->vario_slow_tau_ms,
                                                                         dt_s);
    }

    if (s_altitude_runtime.kf_imu.valid != false)
    {
        s_altitude_runtime.vario_fast_imu_cms = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.vario_fast_imu_cms,
                                                                       s_altitude_runtime.kf_imu.x[1],
                                                                       settings->vario_fast_tau_ms,
                                                                       dt_s);
        s_altitude_runtime.vario_slow_imu_cms = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.vario_slow_imu_cms,
                                                                       s_altitude_runtime.vario_fast_imu_cms,
                                                                       settings->vario_slow_tau_ms,
                                                                       dt_s);
    }

    imu_display_enabled = ((settings->imu_aid_enabled != 0u) &&
                           (imu_input_enabled != false) &&
                           (s_altitude_runtime.kf_imu.valid != false));

    display_target_cm = imu_display_enabled ?
                        s_altitude_runtime.kf_imu.x[0] :
                        (s_altitude_runtime.kf_noimu.valid ? s_altitude_runtime.kf_noimu.x[0] : alt_qnh_cm);

    display_source_vario_cms = imu_display_enabled ?
                               s_altitude_runtime.vario_slow_imu_cms :
                               s_altitude_runtime.vario_slow_noimu_cms;

    display_rest_active = APP_ALTITUDE_IsDisplayRestActive(settings,
                                                           display_source_vario_cms,
                                                           imu_vertical_cms2,
                                                           imu_vector_valid);

    if (display_rest_active != false)
    {
        display_tau_ms = settings->rest_display_tau_ms;

        if (fabsf(display_target_cm - s_altitude_runtime.display_alt_filt_cm) <
            (float)settings->rest_display_hold_cm)
        {
            display_target_cm = s_altitude_runtime.display_alt_filt_cm;
        }
    }

    s_altitude_runtime.display_alt_filt_cm = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.display_alt_filt_cm,
                                                                    display_target_cm,
                                                                    display_tau_ms,
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
    g_app_state.altitude.alt_display_cm             = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.display_alt_filt_cm);

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
}

void APP_ALTITUDE_DebugSetUiActive(bool active, uint32_t now_ms)
{
    (void)now_ms;
    s_altitude_runtime.ui_active = active;

    if (active == false)
    {
        g_app_state.altitude.debug_audio_active = 0u;
        APP_ALTITUDE_StopOwnedAudioIfNeeded();
    }
}

void APP_ALTITUDE_DebugRequestHomeCapture(void)
{
    s_altitude_runtime.home_capture_request = true;
}

void APP_ALTITUDE_DebugRequestBiasRezero(void)
{
    s_altitude_runtime.bias_rezero_request = true;
}
