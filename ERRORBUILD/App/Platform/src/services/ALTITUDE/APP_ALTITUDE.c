#include "APP_ALTITUDE.h"

#include "APP_STATE.h"
#include "Audio_Driver.h"
#include "main.h"

#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

/* -------------------------------------------------------------------------- */
/*  내부 상수                                                                   */
/* -------------------------------------------------------------------------- */
#define APP_ALTITUDE_MAX_KALMAN_N           4u
#define APP_ALTITUDE_PRESSURE_STD_HPA       1013.25f
#define APP_ALTITUDE_ISA_EXPONENT           0.190284f
#define APP_ALTITUDE_ISA_EXPONENT_INV       (1.0f / APP_ALTITUDE_ISA_EXPONENT)
#define APP_ALTITUDE_ISA_SCALE_M            44330.77f
#define APP_ALTITUDE_GRAVITY_MPS2           9.80665f
#define APP_ALTITUDE_MPU_ACCEL_LSB_PER_G    8192.0f
#define APP_ALTITUDE_MPU_GYRO_LSB_PER_DPS   65.5f
#define APP_ALTITUDE_MAX_ACCEPTABLE_DT_S    0.250f
#define APP_ALTITUDE_MIN_ACCEPTABLE_DT_S    0.001f
#define APP_ALTITUDE_GPS_RESIDUAL_GATE_SIGMA 6.0f
#define APP_ALTITUDE_BARO_RESIDUAL_GATE_SIGMA 8.0f
#define APP_ALTITUDE_AUDIO_MAX_CLIMB_CMS    400.0f
#define APP_ALTITUDE_AUDIO_MAX_SINK_CMS     400.0f
#define APP_ALTITUDE_OUTPUT_SWITCH_SEED_TOLERANCE_CM 0.5f

/* -------------------------------------------------------------------------- */
/*  no-IMU 3-state filter                                                       */
/*                                                                            */
/*  상태벡터 x = [ h_cm, v_cms, b_baro_cm ]^T                                  */
/*                                                                            */
/*  측정식                                                                     */
/*  - baro : z = h + b                                                         */
/*  - GPS  : z = h                                                             */
/* -------------------------------------------------------------------------- */
typedef struct
{
    uint8_t  seeded;
    uint32_t time_ms;
    float    x[3];
    float    P[9];
} app_alt_kf_noimu_t;

/* -------------------------------------------------------------------------- */
/*  IMU-assisted 4-state filter                                                 */
/*                                                                            */
/*  상태벡터 x = [ h_cm, v_cms, b_baro_cm, b_acc_cms2 ]^T                      */
/*                                                                            */
/*  예측식                                                                     */
/*  - a_vert_cms2 는 IMU에서 구한 earth vertical linear acceleration            */
/*  - 실제 예측에는 (a_vert_cms2 - b_acc_cms2)를 사용한다.                      */
/*                                                                            */
/*  측정식                                                                     */
/*  - baro : z = h + b_baro                                                    */
/*  - GPS  : z = h                                                             */
/* -------------------------------------------------------------------------- */
typedef struct
{
    uint8_t  seeded;
    uint32_t time_ms;
    float    x[4];
    float    P[16];
} app_alt_kf_imu_t;

/* -------------------------------------------------------------------------- */
/*  모듈 내부 runtime                                                           */
/*                                                                            */
/*  APP_STATE.altitude 는 공개 스냅샷 저장소이고,                               */
/*  아래 runtime은 필터 공분산/히스토리/오디오 스케줄처럼                        */
/*  외부에 그대로 노출할 필요가 없는 내부 상태를 담는다.                        */
/* -------------------------------------------------------------------------- */
typedef struct
{
    uint8_t  pressure_seeded;
    uint8_t  gps_alt_seeded;
    uint8_t  qnh_equiv_seeded;
    uint8_t  display_seeded;
    uint8_t  baro_gate_seeded;
    uint8_t  home_valid;
    uint8_t  home_reset_pending;
    uint8_t  attitude_seeded;

    uint8_t  baro_hist_count;
    uint8_t  output_mode_prev;
    uint8_t  use_imu_prev;
    uint8_t  audio_tone_active;

    uint32_t last_baro_sample_count;
    uint32_t last_mpu_sample_count;
    uint32_t last_gps_update_ms;
    int32_t  last_manual_qnh_hpa_x100;

    float    pressure_lpf_hpa;
    float    gps_alt_lpf_cm;
    float    qnh_equiv_lpf_hpa;
    float    display_alt_lpf_cm;

    float    baro_std_hist_cm[3];
    float    last_accepted_baro_std_cm;

    float    last_vario_fast_noimu_cms;
    float    last_vario_slow_noimu_cms;
    float    last_vario_fast_imu_cms;
    float    last_vario_slow_imu_cms;

    float    home_noimu_cm;
    float    home_imu_cm;

    float    roll_rad;
    float    pitch_rad;
    float    last_imu_vertical_accel_cms2;

    uint32_t audio_tone_off_ms;
    uint32_t audio_next_beep_ms;

    app_alt_kf_noimu_t noimu_kf;
    app_alt_kf_imu_t   imu_kf;
} app_altitude_runtime_t;

static app_altitude_runtime_t s_alt_rt;

/* -------------------------------------------------------------------------- */
/*  작은 수학 helper                                                            */
/* -------------------------------------------------------------------------- */
static float APP_ALT_ClampFloat(float value, float min_value, float max_value)
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

static int32_t APP_ALT_ClampS32(int32_t value, int32_t min_value, int32_t max_value)
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

static uint32_t APP_ALT_ClampU32(uint32_t value, uint32_t min_value, uint32_t max_value)
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

static float APP_ALT_AbsFloat(float value)
{
    return (value < 0.0f) ? -value : value;
}

static float APP_ALT_DtSecondsFromMs(uint32_t newer_ms, uint32_t older_ms)
{
    float dt_s;

    if (newer_ms <= older_ms)
    {
        return 0.0f;
    }

    dt_s = (float)((int32_t)(newer_ms - older_ms)) * 0.001f;
    dt_s = APP_ALT_ClampFloat(dt_s,
                              APP_ALTITUDE_MIN_ACCEPTABLE_DT_S,
                              APP_ALTITUDE_MAX_ACCEPTABLE_DT_S);
    return dt_s;
}

static float APP_ALT_LpfFloat(float previous_value,
                              float input_value,
                              float dt_s,
                              uint32_t tau_ms)
{
    float tau_s;
    float alpha;

    if (tau_ms == 0u)
    {
        return input_value;
    }

    if (dt_s <= 0.0f)
    {
        return previous_value;
    }

    tau_s = (float)tau_ms * 0.001f;
    alpha = dt_s / (tau_s + dt_s);
    alpha = APP_ALT_ClampFloat(alpha, 0.0f, 1.0f);

    return previous_value + (input_value - previous_value) * alpha;
}

/* -------------------------------------------------------------------------- */
/*  pressure <-> altitude 변환                                                  */
/*                                                                            */
/*  이 구현은 ISA 기반 pressure altitude 식을 사용한다.                        */
/* -------------------------------------------------------------------------- */
static float APP_ALT_PressureToAltitudeCm(float pressure_hpa, float sea_level_hpa)
{
    float ratio;
    float altitude_m;

    if ((pressure_hpa <= 0.0f) || (sea_level_hpa <= 0.0f))
    {
        return 0.0f;
    }

    ratio = pressure_hpa / sea_level_hpa;
    ratio = APP_ALT_ClampFloat(ratio, 0.001f, 4.0f);

    altitude_m = APP_ALTITUDE_ISA_SCALE_M *
                 (1.0f - powf(ratio, APP_ALTITUDE_ISA_EXPONENT));

    return altitude_m * 100.0f;
}

static float APP_ALT_AltitudeToQnhHpa(float altitude_cm, float pressure_hpa)
{
    float altitude_m;
    float term;

    altitude_m = altitude_cm * 0.01f;
    term = 1.0f - (altitude_m / APP_ALTITUDE_ISA_SCALE_M);
    term = APP_ALT_ClampFloat(term, 0.02f, 2.00f);

    return pressure_hpa / powf(term, APP_ALTITUDE_ISA_EXPONENT_INV);
}

/* -------------------------------------------------------------------------- */
/*  3-sample median                                                            */
/*                                                                            */
/*  pressure raw에서 단발성 튐을 한 번 더 깎기 위해                            */
/*  3개 히스토리를 사용한다.                                                   */
/* -------------------------------------------------------------------------- */
static float APP_ALT_Median3(float a, float b, float c)
{
    if (a > b)
    {
        float t = a;
        a = b;
        b = t;
    }

    if (b > c)
    {
        float t = b;
        b = c;
        c = t;
    }

    if (a > b)
    {
        float t = a;
        a = b;
        b = t;
    }

    return b;
}

static float APP_ALT_PushAndGetBaroMedian(float new_value_cm)
{
    uint8_t slot;

    slot = (uint8_t)(s_alt_rt.last_baro_sample_count % 3u);
    s_alt_rt.baro_std_hist_cm[slot] = new_value_cm;

    if (s_alt_rt.baro_hist_count < 3u)
    {
        s_alt_rt.baro_hist_count++;
    }

    if (s_alt_rt.baro_hist_count < 3u)
    {
        return new_value_cm;
    }

    return APP_ALT_Median3(s_alt_rt.baro_std_hist_cm[0],
                           s_alt_rt.baro_std_hist_cm[1],
                           s_alt_rt.baro_std_hist_cm[2]);
}

/* -------------------------------------------------------------------------- */
/*  아주 작은 row-major matrix helper                                           */
/*                                                                            */
/*  이 모듈의 Kalman 차원은 최대 4이므로                                        */
/*  일반 목적 matrix library 대신 작은 for-loop helper만 사용한다.             */
/* -------------------------------------------------------------------------- */
static void APP_ALT_MatIdentity(float *matrix, uint32_t n)
{
    uint32_t row;
    uint32_t col;

    for (row = 0u; row < n; row++)
    {
        for (col = 0u; col < n; col++)
        {
            matrix[row * n + col] = (row == col) ? 1.0f : 0.0f;
        }
    }
}

static void APP_ALT_KalmanPredictLinear(float *x,
                                        float *P,
                                        uint32_t n,
                                        const float *F,
                                        const float *Q)
{
    float x_new[APP_ALTITUDE_MAX_KALMAN_N] = {0.0f, 0.0f, 0.0f, 0.0f};
    float fp[APP_ALTITUDE_MAX_KALMAN_N * APP_ALTITUDE_MAX_KALMAN_N] = {0};
    float p_new[APP_ALTITUDE_MAX_KALMAN_N * APP_ALTITUDE_MAX_KALMAN_N] = {0};
    uint32_t row;
    uint32_t col;
    uint32_t k;

    for (row = 0u; row < n; row++)
    {
        for (col = 0u; col < n; col++)
        {
            x_new[row] += F[row * n + col] * x[col];
        }
    }

    for (row = 0u; row < n; row++)
    {
        for (col = 0u; col < n; col++)
        {
            for (k = 0u; k < n; k++)
            {
                fp[row * n + col] += F[row * n + k] * P[k * n + col];
            }
        }
    }

    for (row = 0u; row < n; row++)
    {
        for (col = 0u; col < n; col++)
        {
            for (k = 0u; k < n; k++)
            {
                p_new[row * n + col] += fp[row * n + k] * F[col * n + k];
            }

            p_new[row * n + col] += Q[row * n + col];
        }
    }

    for (row = 0u; row < n; row++)
    {
        x[row] = x_new[row];
    }

    for (row = 0u; row < (n * n); row++)
    {
        P[row] = p_new[row];
    }
}

static float APP_ALT_KalmanPredictMeasurement(const float *x,
                                              uint32_t n,
                                              const float *H)
{
    float zhat;
    uint32_t i;

    zhat = 0.0f;
    for (i = 0u; i < n; i++)
    {
        zhat += H[i] * x[i];
    }

    return zhat;
}

static float APP_ALT_KalmanMeasurementVariance(const float *P,
                                               uint32_t n,
                                               const float *H,
                                               float R)
{
    float ph_t[APP_ALTITUDE_MAX_KALMAN_N] = {0.0f, 0.0f, 0.0f, 0.0f};
    float s;
    uint32_t row;
    uint32_t col;

    for (row = 0u; row < n; row++)
    {
        for (col = 0u; col < n; col++)
        {
            ph_t[row] += P[row * n + col] * H[col];
        }
    }

    s = R;
    for (row = 0u; row < n; row++)
    {
        s += H[row] * ph_t[row];
    }

    if (s < 1.0e-6f)
    {
        s = 1.0e-6f;
    }

    return s;
}

static void APP_ALT_KalmanUpdateScalar(float *x,
                                       float *P,
                                       uint32_t n,
                                       const float *H,
                                       float z,
                                       float R)
{
    float ph_t[APP_ALTITUDE_MAX_KALMAN_N] = {0.0f, 0.0f, 0.0f, 0.0f};
    float k_gain[APP_ALTITUDE_MAX_KALMAN_N] = {0.0f, 0.0f, 0.0f, 0.0f};
    float i_kh[APP_ALTITUDE_MAX_KALMAN_N * APP_ALTITUDE_MAX_KALMAN_N] = {0};
    float temp[APP_ALTITUDE_MAX_KALMAN_N * APP_ALTITUDE_MAX_KALMAN_N] = {0};
    float p_new[APP_ALTITUDE_MAX_KALMAN_N * APP_ALTITUDE_MAX_KALMAN_N] = {0};
    float zhat;
    float s;
    float innovation;
    uint32_t row;
    uint32_t col;
    uint32_t k;

    zhat = APP_ALT_KalmanPredictMeasurement(x, n, H);
    s = APP_ALT_KalmanMeasurementVariance(P, n, H, R);
    innovation = z - zhat;

    for (row = 0u; row < n; row++)
    {
        for (col = 0u; col < n; col++)
        {
            ph_t[row] += P[row * n + col] * H[col];
        }
    }

    for (row = 0u; row < n; row++)
    {
        k_gain[row] = ph_t[row] / s;
        x[row] += k_gain[row] * innovation;
    }

    APP_ALT_MatIdentity(i_kh, n);
    for (row = 0u; row < n; row++)
    {
        for (col = 0u; col < n; col++)
        {
            i_kh[row * n + col] -= k_gain[row] * H[col];
        }
    }

    for (row = 0u; row < n; row++)
    {
        for (col = 0u; col < n; col++)
        {
            for (k = 0u; k < n; k++)
            {
                temp[row * n + col] += i_kh[row * n + k] * P[k * n + col];
            }
        }
    }

    for (row = 0u; row < n; row++)
    {
        for (col = 0u; col < n; col++)
        {
            for (k = 0u; k < n; k++)
            {
                p_new[row * n + col] += temp[row * n + k] * i_kh[col * n + k];
            }

            p_new[row * n + col] += R * k_gain[row] * k_gain[col];
        }
    }

    /* ---------------------------------------------------------------------- */
    /*  수치 오차로 비대칭이 생기는 것을 줄이기 위해                            */
    /*  마지막에 대칭화 + 대각 하한을 걸어 준다.                               */
    /* ---------------------------------------------------------------------- */
    for (row = 0u; row < n; row++)
    {
        for (col = 0u; col < n; col++)
        {
            float sym;

            sym = 0.5f * (p_new[row * n + col] + p_new[col * n + row]);
            P[row * n + col] = sym;
        }

        if (P[row * n + row] < 1.0e-3f)
        {
            P[row * n + row] = 1.0e-3f;
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  필터 seed/predict/update wrapper                                            */
/* -------------------------------------------------------------------------- */
static void APP_ALT_SeedNoImuFilter(app_alt_kf_noimu_t *kf,
                                    float initial_alt_cm,
                                    float initial_bias_cm,
                                    uint32_t time_ms)
{
    memset(kf, 0, sizeof(*kf));

    kf->seeded = 1u;
    kf->time_ms = time_ms;
    kf->x[0] = initial_alt_cm;
    kf->x[1] = 0.0f;
    kf->x[2] = initial_bias_cm;

    kf->P[0] = 900.0f;
    kf->P[4] = 400.0f;
    kf->P[8] = 2500.0f;
}

static void APP_ALT_PredictNoImu(app_alt_kf_noimu_t *kf,
                                 uint32_t target_time_ms,
                                 const app_altitude_settings_t *cfg)
{
    float dt_s;
    float F[9] = {0};
    float Q[9] = {0};
    float q_acc2;
    float q_bias2;

    if ((kf == 0) || (cfg == 0) || (kf->seeded == 0u))
    {
        return;
    }

    if (target_time_ms <= kf->time_ms)
    {
        return;
    }

    dt_s = APP_ALT_DtSecondsFromMs(target_time_ms, kf->time_ms);
    if (dt_s <= 0.0f)
    {
        kf->time_ms = target_time_ms;
        return;
    }

    q_acc2 = (float)cfg->noimu_process_accel_cms2 * (float)cfg->noimu_process_accel_cms2;
    q_bias2 = (float)cfg->baro_bias_walk_cms * (float)cfg->baro_bias_walk_cms;

    F[0] = 1.0f; F[1] = dt_s; F[2] = 0.0f;
    F[3] = 0.0f; F[4] = 1.0f; F[5] = 0.0f;
    F[6] = 0.0f; F[7] = 0.0f; F[8] = 1.0f;

    Q[0] = 0.25f * dt_s * dt_s * dt_s * dt_s * q_acc2;
    Q[1] = 0.50f * dt_s * dt_s * dt_s * q_acc2;
    Q[3] = Q[1];
    Q[4] = dt_s * dt_s * q_acc2;
    Q[8] = dt_s * q_bias2;

    APP_ALT_KalmanPredictLinear(kf->x, kf->P, 3u, F, Q);
    kf->time_ms = target_time_ms;
}

static void APP_ALT_UpdateNoImuBaro(app_alt_kf_noimu_t *kf,
                                    float altitude_baro_cm,
                                    float sigma_cm)
{
    static const float H_BARO[3] = {1.0f, 0.0f, 1.0f};
    float R;
    float innovation;
    float S;

    if ((kf == 0) || (kf->seeded == 0u))
    {
        return;
    }

    R = sigma_cm * sigma_cm;
    innovation = altitude_baro_cm - APP_ALT_KalmanPredictMeasurement(kf->x, 3u, H_BARO);
    S = APP_ALT_KalmanMeasurementVariance(kf->P, 3u, H_BARO, R);

    if (APP_ALT_AbsFloat(innovation) > (APP_ALTITUDE_BARO_RESIDUAL_GATE_SIGMA * sqrtf(S)))
    {
        return;
    }

    APP_ALT_KalmanUpdateScalar(kf->x, kf->P, 3u, H_BARO, altitude_baro_cm, R);
}

static uint8_t APP_ALT_UpdateNoImuGps(app_alt_kf_noimu_t *kf,
                                      float altitude_gps_cm,
                                      float sigma_cm)
{
    static const float H_GPS[3] = {1.0f, 0.0f, 0.0f};
    float R;
    float innovation;
    float S;

    if ((kf == 0) || (kf->seeded == 0u))
    {
        return 0u;
    }

    R = sigma_cm * sigma_cm;
    innovation = altitude_gps_cm - APP_ALT_KalmanPredictMeasurement(kf->x, 3u, H_GPS);
    S = APP_ALT_KalmanMeasurementVariance(kf->P, 3u, H_GPS, R);

    if (APP_ALT_AbsFloat(innovation) > (APP_ALTITUDE_GPS_RESIDUAL_GATE_SIGMA * sqrtf(S)))
    {
        return 0u;
    }

    APP_ALT_KalmanUpdateScalar(kf->x, kf->P, 3u, H_GPS, altitude_gps_cm, R);
    return 1u;
}

static void APP_ALT_SeedImuFilter(app_alt_kf_imu_t *kf,
                                  float initial_alt_cm,
                                  float initial_bias_cm,
                                  uint32_t time_ms)
{
    memset(kf, 0, sizeof(*kf));

    kf->seeded = 1u;
    kf->time_ms = time_ms;
    kf->x[0] = initial_alt_cm;
    kf->x[1] = 0.0f;
    kf->x[2] = initial_bias_cm;
    kf->x[3] = 0.0f;

    kf->P[0]  = 900.0f;
    kf->P[5]  = 500.0f;
    kf->P[10] = 2500.0f;
    kf->P[15] = 400.0f;
}

static void APP_ALT_PredictImu(app_alt_kf_imu_t *kf,
                               uint32_t target_time_ms,
                               float vertical_accel_cms2,
                               const app_altitude_settings_t *cfg)
{
    float dt_s;
    float F[16] = {0};
    float Q[16] = {0};
    float q_acc2;
    float q_bias_baro2;
    float q_bias_acc2;
    float effective_accel_cms2;

    if ((kf == 0) || (cfg == 0) || (kf->seeded == 0u))
    {
        return;
    }

    if (target_time_ms <= kf->time_ms)
    {
        return;
    }

    dt_s = APP_ALT_DtSecondsFromMs(target_time_ms, kf->time_ms);
    if (dt_s <= 0.0f)
    {
        kf->time_ms = target_time_ms;
        return;
    }

    effective_accel_cms2 = vertical_accel_cms2 - kf->x[3];

    /* ---------------------------------------------------------------------- */
    /*  제어 입력(accel)을 상태 예측에 직접 반영한다.                            */
    /* ---------------------------------------------------------------------- */
    kf->x[0] += (kf->x[1] * dt_s) + (0.5f * effective_accel_cms2 * dt_s * dt_s);
    kf->x[1] += effective_accel_cms2 * dt_s;

    q_acc2 = (float)cfg->imu_process_accel_cms2 * (float)cfg->imu_process_accel_cms2;
    q_bias_baro2 = (float)cfg->baro_bias_walk_cms * (float)cfg->baro_bias_walk_cms;
    q_bias_acc2  = (float)cfg->accel_bias_walk_cms2 * (float)cfg->accel_bias_walk_cms2;

    F[0]  = 1.0f; F[1]  = dt_s; F[2]  = 0.0f; F[3]  = -0.5f * dt_s * dt_s;
    F[4]  = 0.0f; F[5]  = 1.0f; F[6]  = 0.0f; F[7]  = -dt_s;
    F[8]  = 0.0f; F[9]  = 0.0f; F[10] = 1.0f; F[11] = 0.0f;
    F[12] = 0.0f; F[13] = 0.0f; F[14] = 0.0f; F[15] = 1.0f;

    Q[0]  = 0.25f * dt_s * dt_s * dt_s * dt_s * q_acc2;
    Q[1]  = 0.50f * dt_s * dt_s * dt_s * q_acc2;
    Q[4]  = Q[1];
    Q[5]  = dt_s * dt_s * q_acc2;
    Q[10] = dt_s * q_bias_baro2;
    Q[15] = dt_s * q_bias_acc2;

    APP_ALT_KalmanPredictLinear(kf->x, kf->P, 4u, F, Q);
    kf->time_ms = target_time_ms;
}

static void APP_ALT_UpdateImuBaro(app_alt_kf_imu_t *kf,
                                  float altitude_baro_cm,
                                  float sigma_cm)
{
    static const float H_BARO[4] = {1.0f, 0.0f, 1.0f, 0.0f};
    float R;
    float innovation;
    float S;

    if ((kf == 0) || (kf->seeded == 0u))
    {
        return;
    }

    R = sigma_cm * sigma_cm;
    innovation = altitude_baro_cm - APP_ALT_KalmanPredictMeasurement(kf->x, 4u, H_BARO);
    S = APP_ALT_KalmanMeasurementVariance(kf->P, 4u, H_BARO, R);

    if (APP_ALT_AbsFloat(innovation) > (APP_ALTITUDE_BARO_RESIDUAL_GATE_SIGMA * sqrtf(S)))
    {
        return;
    }

    APP_ALT_KalmanUpdateScalar(kf->x, kf->P, 4u, H_BARO, altitude_baro_cm, R);
}

static uint8_t APP_ALT_UpdateImuGps(app_alt_kf_imu_t *kf,
                                    float altitude_gps_cm,
                                    float sigma_cm)
{
    static const float H_GPS[4] = {1.0f, 0.0f, 0.0f, 0.0f};
    float R;
    float innovation;
    float S;

    if ((kf == 0) || (kf->seeded == 0u))
    {
        return 0u;
    }

    R = sigma_cm * sigma_cm;
    innovation = altitude_gps_cm - APP_ALT_KalmanPredictMeasurement(kf->x, 4u, H_GPS);
    S = APP_ALT_KalmanMeasurementVariance(kf->P, 4u, H_GPS, R);

    if (APP_ALT_AbsFloat(innovation) > (APP_ALTITUDE_GPS_RESIDUAL_GATE_SIGMA * sqrtf(S)))
    {
        return 0u;
    }

    APP_ALT_KalmanUpdateScalar(kf->x, kf->P, 4u, H_GPS, altitude_gps_cm, R);
    return 1u;
}

/* -------------------------------------------------------------------------- */
/*  텍스트 helper                                                               */
/* -------------------------------------------------------------------------- */
const char *APP_ALTITUDE_GetOutputModeText(app_alt_output_mode_t mode)
{
    switch (mode)
    {
    case APP_ALT_OUTPUT_PRESSURE_STD:
        return "STD";

    case APP_ALT_OUTPUT_QNH_MANUAL:
        return "QNH";

    case APP_ALT_OUTPUT_QNH_GPS_EQUIV:
        return "QNHG";

    case APP_ALT_OUTPUT_GPS_HMSL:
        return "GPS";

    case APP_ALT_OUTPUT_FUSED_ABS:
        return "ABS";

    case APP_ALT_OUTPUT_REL_HOME:
        return "HOME";

    case APP_ALT_OUTPUT_COUNT:
    default:
        return "?";
    }
}

const char *APP_ALTITUDE_GetTuneFieldText(app_altitude_tune_field_t field)
{
    switch (field)
    {
    case APP_ALT_TUNE_MANUAL_QNH:
        return "QNH";

    case APP_ALT_TUNE_PRESSURE_TAU:
        return "P_TAU";

    case APP_ALT_TUNE_GPS_ALT_TAU:
        return "GPS_TAU";

    case APP_ALT_TUNE_QNH_EQUIV_TAU:
        return "EQ_TAU";

    case APP_ALT_TUNE_VARIO_FAST_TAU:
        return "VF_TAU";

    case APP_ALT_TUNE_VARIO_SLOW_TAU:
        return "VS_TAU";

    case APP_ALT_TUNE_BARO_SPIKE_REJECT_CM:
        return "SPIKE";

    case APP_ALT_TUNE_BARO_NOISE_CM:
        return "B_NOIS";

    case APP_ALT_TUNE_GPS_MAX_VACC_MM:
        return "G_VACC";

    case APP_ALT_TUNE_NOIMU_PROCESS_ACCEL_CMS2:
        return "Q_NI";

    case APP_ALT_TUNE_IMU_PROCESS_ACCEL_CMS2:
        return "Q_IMU";

    case APP_ALT_TUNE_IMU_ATTITUDE_TAU_MS:
        return "ATT_T";

    case APP_ALT_TUNE_IMU_ACC_TRUST_MG:
        return "ACC_TR";

    case APP_ALT_TUNE_CLIMB_DEADBAND_CMS:
        return "CL_DB";

    case APP_ALT_TUNE_SINK_DEADBAND_CMS:
        return "SN_DB";

    case APP_ALT_TUNE_COUNT:
    default:
        return "?";
    }
}

/* -------------------------------------------------------------------------- */
/*  settings default / clamp                                                    */
/* -------------------------------------------------------------------------- */
void APP_ALTITUDE_ApplyDefaultSettings(app_altitude_settings_t *dst)
{
    if (dst == 0)
    {
        return;
    }

    memset(dst, 0, sizeof(*dst));

    dst->manual_qnh_hpa_x100      = APP_ALTITUDE_DEFAULT_MANUAL_QNH_HPA_X100;
    dst->output_alt_mode          = (uint8_t)APP_ALTITUDE_DEFAULT_OUTPUT_ALT_MODE;
    dst->use_imu_assist           = APP_ALTITUDE_DEFAULT_USE_IMU_ASSIST;
    dst->vario_audio_enable       = APP_ALTITUDE_DEFAULT_VARIO_AUDIO_ENABLE;

    dst->pressure_lpf_tau_ms      = APP_ALTITUDE_DEFAULT_PRESSURE_LPF_TAU_MS;
    dst->gps_alt_lpf_tau_ms       = APP_ALTITUDE_DEFAULT_GPS_ALT_LPF_TAU_MS;
    dst->qnh_equiv_lpf_tau_ms     = APP_ALTITUDE_DEFAULT_QNH_EQUIV_LPF_TAU_MS;
    dst->display_alt_lpf_tau_ms   = APP_ALTITUDE_DEFAULT_DISPLAY_ALT_LPF_TAU_MS;
    dst->vario_fast_lpf_tau_ms    = APP_ALTITUDE_DEFAULT_VARIO_FAST_LPF_TAU_MS;
    dst->vario_slow_lpf_tau_ms    = APP_ALTITUDE_DEFAULT_VARIO_SLOW_LPF_TAU_MS;

    dst->gps_min_numsv            = APP_ALTITUDE_DEFAULT_GPS_MIN_NUMSV;
    dst->gps_max_vacc_mm          = APP_ALTITUDE_DEFAULT_GPS_MAX_VACC_MM;
    dst->gps_max_pdop_x100        = APP_ALTITUDE_DEFAULT_GPS_MAX_PDOP_X100;
    dst->grade_min_speed_mmps     = APP_ALTITUDE_DEFAULT_GRADE_MIN_SPEED_MMPS;

    dst->baro_spike_reject_cm     = APP_ALTITUDE_DEFAULT_BARO_SPIKE_REJECT_CM;
    dst->baro_noise_cm            = APP_ALTITUDE_DEFAULT_BARO_NOISE_CM;
    dst->gps_min_noise_cm         = APP_ALTITUDE_DEFAULT_GPS_MIN_NOISE_CM;
    dst->noimu_process_accel_cms2 = APP_ALTITUDE_DEFAULT_NOIMU_PROCESS_ACCEL_CMS2;
    dst->imu_process_accel_cms2   = APP_ALTITUDE_DEFAULT_IMU_PROCESS_ACCEL_CMS2;
    dst->baro_bias_walk_cms       = APP_ALTITUDE_DEFAULT_BARO_BIAS_WALK_CMS;
    dst->accel_bias_walk_cms2     = APP_ALTITUDE_DEFAULT_ACCEL_BIAS_WALK_CMS2;
    dst->imu_attitude_tau_ms      = APP_ALTITUDE_DEFAULT_IMU_ATTITUDE_TAU_MS;
    dst->imu_acc_trust_mg         = APP_ALTITUDE_DEFAULT_IMU_ACC_TRUST_MG;

    dst->climb_deadband_cms       = APP_ALTITUDE_DEFAULT_CLIMB_DEADBAND_CMS;
    dst->sink_deadband_cms        = APP_ALTITUDE_DEFAULT_SINK_DEADBAND_CMS;

    dst->vario_beep_base_hz       = APP_ALTITUDE_DEFAULT_VARIO_BEEP_BASE_HZ;
    dst->vario_beep_span_hz       = APP_ALTITUDE_DEFAULT_VARIO_BEEP_SPAN_HZ;
    dst->vario_beep_min_on_ms     = APP_ALTITUDE_DEFAULT_VARIO_BEEP_MIN_ON_MS;
    dst->vario_beep_max_on_ms     = APP_ALTITUDE_DEFAULT_VARIO_BEEP_MAX_ON_MS;
    dst->vario_beep_min_period_ms = APP_ALTITUDE_DEFAULT_VARIO_BEEP_MIN_PERIOD_MS;
    dst->vario_beep_max_period_ms = APP_ALTITUDE_DEFAULT_VARIO_BEEP_MAX_PERIOD_MS;

    APP_ALTITUDE_ClampSettings(dst);
}

void APP_ALTITUDE_ClampSettings(app_altitude_settings_t *settings)
{
    if (settings == 0)
    {
        return;
    }

    settings->manual_qnh_hpa_x100 = APP_ALT_ClampS32(settings->manual_qnh_hpa_x100,
                                                     85000L,
                                                     110000L);

    if (settings->output_alt_mode >= (uint8_t)APP_ALT_OUTPUT_COUNT)
    {
        settings->output_alt_mode = (uint8_t)APP_ALTITUDE_DEFAULT_OUTPUT_ALT_MODE;
    }

    settings->use_imu_assist = (settings->use_imu_assist != 0u) ? 1u : 0u;
    settings->vario_audio_enable = (settings->vario_audio_enable != 0u) ? 1u : 0u;

    settings->pressure_lpf_tau_ms      = (uint16_t)APP_ALT_ClampU32(settings->pressure_lpf_tau_ms,      10u,    3000u);
    settings->gps_alt_lpf_tau_ms       = (uint16_t)APP_ALT_ClampU32(settings->gps_alt_lpf_tau_ms,       50u,    5000u);
    settings->qnh_equiv_lpf_tau_ms     = (uint16_t)APP_ALT_ClampU32(settings->qnh_equiv_lpf_tau_ms,     100u,  15000u);
    settings->display_alt_lpf_tau_ms   = (uint16_t)APP_ALT_ClampU32(settings->display_alt_lpf_tau_ms,   0u,     5000u);
    settings->vario_fast_lpf_tau_ms    = (uint16_t)APP_ALT_ClampU32(settings->vario_fast_lpf_tau_ms,    10u,    2000u);
    settings->vario_slow_lpf_tau_ms    = (uint16_t)APP_ALT_ClampU32(settings->vario_slow_lpf_tau_ms,    50u,    5000u);

    settings->gps_min_numsv            = (uint16_t)APP_ALT_ClampU32(settings->gps_min_numsv,            4u,      32u);
    settings->gps_max_vacc_mm          = (uint16_t)APP_ALT_ClampU32(settings->gps_max_vacc_mm,          500u,  50000u);
    settings->gps_max_pdop_x100        = (uint16_t)APP_ALT_ClampU32(settings->gps_max_pdop_x100,        100u,   1000u);
    settings->grade_min_speed_mmps     = (uint16_t)APP_ALT_ClampU32(settings->grade_min_speed_mmps,     100u,  30000u);

    settings->baro_spike_reject_cm     = (uint16_t)APP_ALT_ClampU32(settings->baro_spike_reject_cm,     20u,    2000u);
    settings->baro_noise_cm            = (uint16_t)APP_ALT_ClampU32(settings->baro_noise_cm,            5u,      500u);
    settings->gps_min_noise_cm         = (uint16_t)APP_ALT_ClampU32(settings->gps_min_noise_cm,         20u,    3000u);
    settings->noimu_process_accel_cms2 = (uint16_t)APP_ALT_ClampU32(settings->noimu_process_accel_cms2, 10u,    2000u);
    settings->imu_process_accel_cms2   = (uint16_t)APP_ALT_ClampU32(settings->imu_process_accel_cms2,   10u,    3000u);
    settings->baro_bias_walk_cms       = (uint16_t)APP_ALT_ClampU32(settings->baro_bias_walk_cms,       1u,      100u);
    settings->accel_bias_walk_cms2     = (uint16_t)APP_ALT_ClampU32(settings->accel_bias_walk_cms2,     1u,      300u);
    settings->imu_attitude_tau_ms      = (uint16_t)APP_ALT_ClampU32(settings->imu_attitude_tau_ms,      10u,    5000u);
    settings->imu_acc_trust_mg         = (uint16_t)APP_ALT_ClampU32(settings->imu_acc_trust_mg,         20u,    1000u);

    settings->climb_deadband_cms       = (int16_t)APP_ALT_ClampS32(settings->climb_deadband_cms,        0,      1000);
    settings->sink_deadband_cms        = (int16_t)APP_ALT_ClampS32(settings->sink_deadband_cms,         0,      1000);

    settings->vario_beep_base_hz       = (uint16_t)APP_ALT_ClampU32(settings->vario_beep_base_hz,       100u,   3000u);
    settings->vario_beep_span_hz       = (uint16_t)APP_ALT_ClampU32(settings->vario_beep_span_hz,       0u,     3000u);
    settings->vario_beep_min_on_ms     = (uint16_t)APP_ALT_ClampU32(settings->vario_beep_min_on_ms,     10u,     500u);
    settings->vario_beep_max_on_ms     = (uint16_t)APP_ALT_ClampU32(settings->vario_beep_max_on_ms,     settings->vario_beep_min_on_ms, 1000u);
    settings->vario_beep_min_period_ms = (uint16_t)APP_ALT_ClampU32(settings->vario_beep_min_period_ms, settings->vario_beep_min_on_ms, 2000u);
    settings->vario_beep_max_period_ms = (uint16_t)APP_ALT_ClampU32(settings->vario_beep_max_period_ms, settings->vario_beep_min_period_ms, 4000u);
}

void APP_ALTITUDE_CopySettings(app_altitude_settings_t *dst)
{
    if (dst == 0)
    {
        return;
    }

    memcpy(dst, (const void *)&g_app_state.settings.altitude, sizeof(*dst));
}

void APP_ALTITUDE_SetSettings(const app_altitude_settings_t *src)
{
    app_altitude_settings_t clamped;

    if (src == 0)
    {
        return;
    }

    clamped = *src;
    APP_ALTITUDE_ClampSettings(&clamped);
    memcpy((void *)&g_app_state.settings.altitude, (const void *)&clamped, sizeof(clamped));
}

/* -------------------------------------------------------------------------- */
/*  tune field format/adjust helper                                             */
/* -------------------------------------------------------------------------- */
void APP_ALTITUDE_FormatTuneFieldValue(const app_altitude_settings_t *settings,
                                       app_altitude_tune_field_t field,
                                       char *out_text,
                                       size_t out_size)
{
    uint32_t absolute_value;

    if ((settings == 0) || (out_text == 0) || (out_size == 0u))
    {
        return;
    }

    switch (field)
    {
    case APP_ALT_TUNE_MANUAL_QNH:
        absolute_value = (uint32_t)((settings->manual_qnh_hpa_x100 >= 0) ?
                          settings->manual_qnh_hpa_x100 :
                          -settings->manual_qnh_hpa_x100);
        snprintf(out_text, out_size, "%s%lu.%02lu hPa",
                 (settings->manual_qnh_hpa_x100 < 0) ? "-" : "",
                 (unsigned long)(absolute_value / 100u),
                 (unsigned long)(absolute_value % 100u));
        break;

    case APP_ALT_TUNE_PRESSURE_TAU:
        snprintf(out_text, out_size, "%ums", (unsigned)settings->pressure_lpf_tau_ms);
        break;

    case APP_ALT_TUNE_GPS_ALT_TAU:
        snprintf(out_text, out_size, "%ums", (unsigned)settings->gps_alt_lpf_tau_ms);
        break;

    case APP_ALT_TUNE_QNH_EQUIV_TAU:
        snprintf(out_text, out_size, "%ums", (unsigned)settings->qnh_equiv_lpf_tau_ms);
        break;

    case APP_ALT_TUNE_VARIO_FAST_TAU:
        snprintf(out_text, out_size, "%ums", (unsigned)settings->vario_fast_lpf_tau_ms);
        break;

    case APP_ALT_TUNE_VARIO_SLOW_TAU:
        snprintf(out_text, out_size, "%ums", (unsigned)settings->vario_slow_lpf_tau_ms);
        break;

    case APP_ALT_TUNE_BARO_SPIKE_REJECT_CM:
        snprintf(out_text, out_size, "%ucm", (unsigned)settings->baro_spike_reject_cm);
        break;

    case APP_ALT_TUNE_BARO_NOISE_CM:
        snprintf(out_text, out_size, "%ucm", (unsigned)settings->baro_noise_cm);
        break;

    case APP_ALT_TUNE_GPS_MAX_VACC_MM:
        snprintf(out_text, out_size, "%u.%02um",
                 (unsigned)(settings->gps_max_vacc_mm / 1000u),
                 (unsigned)((settings->gps_max_vacc_mm % 1000u) / 10u));
        break;

    case APP_ALT_TUNE_NOIMU_PROCESS_ACCEL_CMS2:
        snprintf(out_text, out_size, "%ucm/s2", (unsigned)settings->noimu_process_accel_cms2);
        break;

    case APP_ALT_TUNE_IMU_PROCESS_ACCEL_CMS2:
        snprintf(out_text, out_size, "%ucm/s2", (unsigned)settings->imu_process_accel_cms2);
        break;

    case APP_ALT_TUNE_IMU_ATTITUDE_TAU_MS:
        snprintf(out_text, out_size, "%ums", (unsigned)settings->imu_attitude_tau_ms);
        break;

    case APP_ALT_TUNE_IMU_ACC_TRUST_MG:
        snprintf(out_text, out_size, "%umg", (unsigned)settings->imu_acc_trust_mg);
        break;

    case APP_ALT_TUNE_CLIMB_DEADBAND_CMS:
        snprintf(out_text, out_size, "%dcm/s", (int)settings->climb_deadband_cms);
        break;

    case APP_ALT_TUNE_SINK_DEADBAND_CMS:
        snprintf(out_text, out_size, "%dcm/s", (int)settings->sink_deadband_cms);
        break;

    case APP_ALT_TUNE_COUNT:
    default:
        snprintf(out_text, out_size, "-");
        break;
    }
}

void APP_ALTITUDE_AdjustSettingField(app_altitude_settings_t *settings,
                                     app_altitude_tune_field_t field,
                                     int32_t delta_steps)
{
    if ((settings == 0) || (delta_steps == 0))
    {
        return;
    }

    switch (field)
    {
    case APP_ALT_TUNE_MANUAL_QNH:
        settings->manual_qnh_hpa_x100 += (delta_steps * 10);
        break;

    case APP_ALT_TUNE_PRESSURE_TAU:
        settings->pressure_lpf_tau_ms = (uint16_t)((int32_t)settings->pressure_lpf_tau_ms + (delta_steps * 10));
        break;

    case APP_ALT_TUNE_GPS_ALT_TAU:
        settings->gps_alt_lpf_tau_ms = (uint16_t)((int32_t)settings->gps_alt_lpf_tau_ms + (delta_steps * 50));
        break;

    case APP_ALT_TUNE_QNH_EQUIV_TAU:
        settings->qnh_equiv_lpf_tau_ms = (uint16_t)((int32_t)settings->qnh_equiv_lpf_tau_ms + (delta_steps * 100));
        break;

    case APP_ALT_TUNE_VARIO_FAST_TAU:
        settings->vario_fast_lpf_tau_ms = (uint16_t)((int32_t)settings->vario_fast_lpf_tau_ms + (delta_steps * 10));
        break;

    case APP_ALT_TUNE_VARIO_SLOW_TAU:
        settings->vario_slow_lpf_tau_ms = (uint16_t)((int32_t)settings->vario_slow_lpf_tau_ms + (delta_steps * 50));
        break;

    case APP_ALT_TUNE_BARO_SPIKE_REJECT_CM:
        settings->baro_spike_reject_cm = (uint16_t)((int32_t)settings->baro_spike_reject_cm + (delta_steps * 10));
        break;

    case APP_ALT_TUNE_BARO_NOISE_CM:
        settings->baro_noise_cm = (uint16_t)((int32_t)settings->baro_noise_cm + (delta_steps * 5));
        break;

    case APP_ALT_TUNE_GPS_MAX_VACC_MM:
        settings->gps_max_vacc_mm = (uint16_t)((int32_t)settings->gps_max_vacc_mm + (delta_steps * 250));
        break;

    case APP_ALT_TUNE_NOIMU_PROCESS_ACCEL_CMS2:
        settings->noimu_process_accel_cms2 = (uint16_t)((int32_t)settings->noimu_process_accel_cms2 + (delta_steps * 10));
        break;

    case APP_ALT_TUNE_IMU_PROCESS_ACCEL_CMS2:
        settings->imu_process_accel_cms2 = (uint16_t)((int32_t)settings->imu_process_accel_cms2 + (delta_steps * 10));
        break;

    case APP_ALT_TUNE_IMU_ATTITUDE_TAU_MS:
        settings->imu_attitude_tau_ms = (uint16_t)((int32_t)settings->imu_attitude_tau_ms + (delta_steps * 10));
        break;

    case APP_ALT_TUNE_IMU_ACC_TRUST_MG:
        settings->imu_acc_trust_mg = (uint16_t)((int32_t)settings->imu_acc_trust_mg + (delta_steps * 10));
        break;

    case APP_ALT_TUNE_CLIMB_DEADBAND_CMS:
        settings->climb_deadband_cms += (int16_t)(delta_steps * 5);
        break;

    case APP_ALT_TUNE_SINK_DEADBAND_CMS:
        settings->sink_deadband_cms += (int16_t)(delta_steps * 5);
        break;

    case APP_ALT_TUNE_COUNT:
    default:
        break;
    }

    APP_ALTITUDE_ClampSettings(settings);
}

/* -------------------------------------------------------------------------- */
/*  GPS quality helper                                                          */
/* -------------------------------------------------------------------------- */
static uint8_t APP_ALT_IsGpsUsable(const gps_fix_basic_t *fix,
                                   const app_altitude_settings_t *cfg,
                                   uint32_t now_ms,
                                   float *weight_pct,
                                   float *sigma_cm)
{
    float weight_vacc;
    float weight_pdop;
    float sigma_candidate_cm;
    uint32_t age_ms;

    if ((fix == 0) || (cfg == 0) || (weight_pct == 0) || (sigma_cm == 0))
    {
        return 0u;
    }

    *weight_pct = 0.0f;
    *sigma_cm = 0.0f;

    if ((fix->valid == false) ||
        (fix->fixOk == false) ||
        (fix->fixType < 3u))
    {
        return 0u;
    }

    if (fix->numSV_used < cfg->gps_min_numsv)
    {
        return 0u;
    }

    if ((fix->vAcc == 0u) || (fix->vAcc > cfg->gps_max_vacc_mm))
    {
        return 0u;
    }

    if ((fix->pDOP == 0u) || (fix->pDOP > cfg->gps_max_pdop_x100))
    {
        return 0u;
    }

    age_ms = (fix->last_fix_ms == 0u) ? 0xFFFFFFFFu : (uint32_t)(now_ms - fix->last_fix_ms);
    if (age_ms > 1500u)
    {
        return 0u;
    }

    weight_vacc = 1.0f - ((float)fix->vAcc / (float)cfg->gps_max_vacc_mm);
    weight_pdop = 1.0f - ((float)fix->pDOP / (float)cfg->gps_max_pdop_x100);
    weight_vacc = APP_ALT_ClampFloat(weight_vacc, 0.05f, 1.0f);
    weight_pdop = APP_ALT_ClampFloat(weight_pdop, 0.05f, 1.0f);

    *weight_pct = APP_ALT_ClampFloat(100.0f * ((weight_vacc < weight_pdop) ? weight_vacc : weight_pdop),
                                     5.0f,
                                     100.0f);

    sigma_candidate_cm = (float)fix->vAcc * 0.1f;
    if (sigma_candidate_cm < (float)cfg->gps_min_noise_cm)
    {
        sigma_candidate_cm = (float)cfg->gps_min_noise_cm;
    }

    *sigma_cm = sigma_candidate_cm;
    return 1u;
}

/* -------------------------------------------------------------------------- */
/*  IMU attitude + vertical acceleration helper                                 */
/* -------------------------------------------------------------------------- */
static void APP_ALT_UpdateImuAttitudeAndVerticalAccel(const app_gy86_state_t *imu,
                                                      const app_altitude_settings_t *cfg,
                                                      app_altitude_state_t *alt)
{
    float ax_ms2;
    float ay_ms2;
    float az_ms2;
    float gx_rps;
    float gy_rps;
    float roll_acc_rad;
    float pitch_acc_rad;
    float dt_s;
    float alpha;
    float acc_norm_g;
    float acc_trust;
    float vertical_specific_force_ms2;
    float vertical_linear_accel_cms2;

    if ((imu == 0) || (cfg == 0) || (alt == 0))
    {
        return;
    }

    if ((imu->status_flags & APP_GY86_STATUS_MPU_VALID) == 0u)
    {
        alt->imu_trust_pct = 0u;
        alt->flags &= (uint8_t)~APP_ALT_FLAG_IMU_VALID;
        alt->flags &= (uint8_t)~APP_ALT_FLAG_IMU_CONFIDENT;
        return;
    }

    if (imu->mpu.sample_count == s_alt_rt.last_mpu_sample_count)
    {
        return;
    }

    alt->flags |= APP_ALT_FLAG_IMU_VALID;

    /* ---------------------------------------------------------------------- */
    /*  축 가정                                                                 */
    /*  - accel/gyro raw는 현재 보드 실장 기준으로                               */
    /*      X : 전후 축                                                         */
    /*      Y : 좌우 축                                                         */
    /*      Z : 위/아래 축                                                      */
    /*    으로 사용한다.                                                        */
    /*  - 만약 실제 실장 방향이 다르면, 아래 raw->SI 변환 직후 축 remap/sign     */
    /*    블록 하나만 바꾸면 전체 IMU assist 경로가 같이 보정된다.               */
    /* ---------------------------------------------------------------------- */
    ax_ms2 = ((float)imu->mpu.accel_x_raw / APP_ALTITUDE_MPU_ACCEL_LSB_PER_G) * APP_ALTITUDE_GRAVITY_MPS2;
    ay_ms2 = ((float)imu->mpu.accel_y_raw / APP_ALTITUDE_MPU_ACCEL_LSB_PER_G) * APP_ALTITUDE_GRAVITY_MPS2;
    az_ms2 = ((float)imu->mpu.accel_z_raw / APP_ALTITUDE_MPU_ACCEL_LSB_PER_G) * APP_ALTITUDE_GRAVITY_MPS2;

    gx_rps = ((float)imu->mpu.gyro_x_raw / APP_ALTITUDE_MPU_GYRO_LSB_PER_DPS) * ((float)M_PI / 180.0f);
    gy_rps = ((float)imu->mpu.gyro_y_raw / APP_ALTITUDE_MPU_GYRO_LSB_PER_DPS) * ((float)M_PI / 180.0f);

    acc_norm_g = sqrtf(ax_ms2 * ax_ms2 + ay_ms2 * ay_ms2 + az_ms2 * az_ms2) / APP_ALTITUDE_GRAVITY_MPS2;
    acc_trust = 1.0f - (APP_ALT_AbsFloat(acc_norm_g - 1.0f) / ((float)cfg->imu_acc_trust_mg * 0.001f));
    acc_trust = APP_ALT_ClampFloat(acc_trust, 0.0f, 1.0f);

    if (s_alt_rt.attitude_seeded == 0u)
    {
        s_alt_rt.roll_rad = atan2f(ay_ms2, az_ms2);
        s_alt_rt.pitch_rad = atan2f(-ax_ms2, sqrtf((ay_ms2 * ay_ms2) + (az_ms2 * az_ms2)));
        s_alt_rt.attitude_seeded = 1u;
    }
    else
    {
        dt_s = APP_ALT_DtSecondsFromMs(imu->mpu.timestamp_ms, alt->last_mpu_update_ms);
        if (dt_s > 0.0f)
        {
            s_alt_rt.roll_rad += gx_rps * dt_s;
            s_alt_rt.pitch_rad += gy_rps * dt_s;
        }

        roll_acc_rad = atan2f(ay_ms2, az_ms2);
        pitch_acc_rad = atan2f(-ax_ms2, sqrtf((ay_ms2 * ay_ms2) + (az_ms2 * az_ms2)));

        if (acc_trust > 0.0f)
        {
            alpha = ((float)cfg->imu_attitude_tau_ms * 0.001f) /
                    (((float)cfg->imu_attitude_tau_ms * 0.001f) + ((dt_s > 0.0f) ? dt_s : 0.01f));
            alpha = APP_ALT_ClampFloat(alpha, 0.0f, 1.0f);

            s_alt_rt.roll_rad  = (alpha * s_alt_rt.roll_rad)  + ((1.0f - alpha) * roll_acc_rad);
            s_alt_rt.pitch_rad = (alpha * s_alt_rt.pitch_rad) + ((1.0f - alpha) * pitch_acc_rad);
        }
    }

    vertical_specific_force_ms2 =
        (-sinf(s_alt_rt.pitch_rad) * ax_ms2) +
        ((sinf(s_alt_rt.roll_rad) * cosf(s_alt_rt.pitch_rad)) * ay_ms2) +
        ((cosf(s_alt_rt.roll_rad) * cosf(s_alt_rt.pitch_rad)) * az_ms2);

    vertical_linear_accel_cms2 = (vertical_specific_force_ms2 - APP_ALTITUDE_GRAVITY_MPS2) * 100.0f;
    s_alt_rt.last_imu_vertical_accel_cms2 = vertical_linear_accel_cms2;

    alt->last_mpu_update_ms = imu->mpu.timestamp_ms;
    alt->imu_sample_count++;
    alt->imu_roll_cdeg = (int16_t)(s_alt_rt.roll_rad * (18000.0f / (float)M_PI));
    alt->imu_pitch_cdeg = (int16_t)(s_alt_rt.pitch_rad * (18000.0f / (float)M_PI));
    alt->imu_trust_pct = (uint8_t)(acc_trust * 100.0f);

    if (acc_trust >= 0.20f)
    {
        alt->flags |= APP_ALT_FLAG_IMU_CONFIDENT;
    }
    else
    {
        alt->flags &= (uint8_t)~APP_ALT_FLAG_IMU_CONFIDENT;
        alt->imu_gated_count++;
    }

    s_alt_rt.last_mpu_sample_count = imu->mpu.sample_count;
}

/* -------------------------------------------------------------------------- */
/*  home / output / grade helper                                                */
/* -------------------------------------------------------------------------- */
static int16_t APP_ALT_ComputeGradeX10(int32_t vario_cms, uint32_t horiz_speed_mmps, uint32_t min_speed_mmps)
{
    int32_t grade_x10;

    if (horiz_speed_mmps < min_speed_mmps)
    {
        return 0;
    }

    grade_x10 = (int32_t)((10000LL * (int64_t)vario_cms) / (int64_t)horiz_speed_mmps);
    grade_x10 = APP_ALT_ClampS32(grade_x10, -999, 999);
    return (int16_t)grade_x10;
}

static uint32_t APP_ALT_GetHorizontalSpeedMmps(const gps_fix_basic_t *fix)
{
    uint32_t speed_mmps;

    if (fix == 0)
    {
        return 0u;
    }

    speed_mmps = (fix->gSpeed >= 0) ? (uint32_t)fix->gSpeed : (uint32_t)(-fix->gSpeed);
    if (speed_mmps == 0u)
    {
        speed_mmps = (fix->speed_llh_mps >= 0.0f) ?
                     (uint32_t)(fix->speed_llh_mps * 1000.0f) :
                     (uint32_t)((-fix->speed_llh_mps) * 1000.0f);
    }

    return speed_mmps;
}

static void APP_ALT_CaptureHomeNow(app_altitude_state_t *alt, uint32_t now_ms)
{
    if (alt == 0)
    {
        return;
    }

    s_alt_rt.home_noimu_cm = (float)alt->alt_fused_noimu_cm;
    s_alt_rt.home_imu_cm   = (float)alt->alt_fused_imu_cm;
    s_alt_rt.home_valid = 1u;
    s_alt_rt.home_reset_pending = 0u;

    alt->flags |= APP_ALT_FLAG_HOME_VALID;
    alt->home_capture_ms = now_ms;
    alt->home_reset_count++;
}

static void APP_ALT_SelectOutputChannel(const app_altitude_settings_t *cfg,
                                        app_altitude_state_t *alt)
{
    uint8_t use_imu;

    if ((cfg == 0) || (alt == 0))
    {
        return;
    }

    alt->output_alt_mode = cfg->output_alt_mode;

    use_imu = (cfg->use_imu_assist != 0u) ? 1u : 0u;

    switch ((app_alt_output_mode_t)cfg->output_alt_mode)
    {
    case APP_ALT_OUTPUT_PRESSURE_STD:
        alt->alt_output_cm = alt->alt_pressure_std_cm;
        alt->alt_rel_home_output_cm = 0;
        alt->vario_output_cms = alt->vario_slow_noimu_cms;
        alt->grade_output_x10 = alt->grade_noimu_x10;
        alt->flags &= (uint8_t)~APP_ALT_FLAG_PRIMARY_USES_IMU;
        break;

    case APP_ALT_OUTPUT_QNH_MANUAL:
        alt->alt_output_cm = alt->alt_qnh_manual_cm;
        alt->alt_rel_home_output_cm = 0;
        alt->vario_output_cms = alt->vario_slow_noimu_cms;
        alt->grade_output_x10 = alt->grade_noimu_x10;
        alt->flags &= (uint8_t)~APP_ALT_FLAG_PRIMARY_USES_IMU;
        break;

    case APP_ALT_OUTPUT_QNH_GPS_EQUIV:
        alt->alt_output_cm = alt->alt_qnh_gps_equiv_cm;
        alt->alt_rel_home_output_cm = 0;
        alt->vario_output_cms = alt->vario_slow_noimu_cms;
        alt->grade_output_x10 = alt->grade_noimu_x10;
        alt->flags &= (uint8_t)~APP_ALT_FLAG_PRIMARY_USES_IMU;
        break;

    case APP_ALT_OUTPUT_GPS_HMSL:
        alt->alt_output_cm = alt->alt_gps_hmsl_cm;
        alt->alt_rel_home_output_cm = 0;
        alt->vario_output_cms = alt->vario_slow_noimu_cms;
        alt->grade_output_x10 = alt->grade_noimu_x10;
        alt->flags &= (uint8_t)~APP_ALT_FLAG_PRIMARY_USES_IMU;
        break;

    case APP_ALT_OUTPUT_REL_HOME:
        if ((use_imu != 0u) && ((alt->flags & APP_ALT_FLAG_IMU_VALID) != 0u))
        {
            alt->alt_output_cm = alt->alt_rel_home_imu_cm;
            alt->alt_rel_home_output_cm = alt->alt_rel_home_imu_cm;
            alt->vario_output_cms = alt->vario_slow_imu_cms;
            alt->grade_output_x10 = alt->grade_imu_x10;
            alt->flags |= APP_ALT_FLAG_PRIMARY_USES_IMU;
        }
        else
        {
            alt->alt_output_cm = alt->alt_rel_home_noimu_cm;
            alt->alt_rel_home_output_cm = alt->alt_rel_home_noimu_cm;
            alt->vario_output_cms = alt->vario_slow_noimu_cms;
            alt->grade_output_x10 = alt->grade_noimu_x10;
            alt->flags &= (uint8_t)~APP_ALT_FLAG_PRIMARY_USES_IMU;
        }
        break;

    case APP_ALT_OUTPUT_FUSED_ABS:
    default:
        if ((use_imu != 0u) && ((alt->flags & APP_ALT_FLAG_IMU_VALID) != 0u))
        {
            alt->alt_output_cm = alt->alt_fused_imu_cm;
            alt->alt_rel_home_output_cm = alt->alt_rel_home_imu_cm;
            alt->vario_output_cms = alt->vario_slow_imu_cms;
            alt->grade_output_x10 = alt->grade_imu_x10;
            alt->flags |= APP_ALT_FLAG_PRIMARY_USES_IMU;
        }
        else
        {
            alt->alt_output_cm = alt->alt_fused_noimu_cm;
            alt->alt_rel_home_output_cm = alt->alt_rel_home_noimu_cm;
            alt->vario_output_cms = alt->vario_slow_noimu_cms;
            alt->grade_output_x10 = alt->grade_noimu_x10;
            alt->flags &= (uint8_t)~APP_ALT_FLAG_PRIMARY_USES_IMU;
        }
        break;
    }
}

/* -------------------------------------------------------------------------- */
/*  vario audio                                                                 */
/*                                                                            */
/*  목적                                                                      */
/*  - ALTITUDE debug page에서 계산된 vario 값이 실제 음으로 들리게 한다.         */
/*  - climb는 상승률이 커질수록 pitch/cadence가 빨라지는 비프음,                 */
/*    sink는 낮은 톤의 긴 경고음 스타일로 낸다.                                 */
/*                                                                            */
/*  주의                                                                      */
/*  - 이 모듈은 오디오 믹서가 아니라 테스트용 vario scheduler다.                */
/*  - vario_audio_enable이 꺼져 있으면 tone scheduling을 즉시 멈춘다.           */
/* -------------------------------------------------------------------------- */
static void APP_ALT_UpdateVarioAudio(const app_altitude_settings_t *cfg,
                                     app_altitude_state_t *alt,
                                     uint32_t now_ms)
{
    float vario_cms;
    float magnitude_norm;
    uint32_t on_ms;
    uint32_t period_ms;
    uint32_t freq_hz;

    if ((cfg == 0) || (alt == 0))
    {
        return;
    }

    if (cfg->vario_audio_enable == 0u)
    {
        if (s_alt_rt.audio_tone_active != 0u)
        {
            Audio_Driver_Stop();
        }

        s_alt_rt.audio_tone_active = 0u;
        alt->vario_audio_active = 0u;
        alt->flags &= (uint8_t)~APP_ALT_FLAG_VARIO_AUDIO_EN;
        return;
    }

    alt->flags |= APP_ALT_FLAG_VARIO_AUDIO_EN;

    if ((s_alt_rt.audio_tone_active != 0u) &&
        ((int32_t)(now_ms - s_alt_rt.audio_tone_off_ms) >= 0))
    {
        Audio_Driver_Stop();
        s_alt_rt.audio_tone_active = 0u;
        alt->vario_audio_active = 0u;
    }

    vario_cms = (float)alt->vario_output_cms;

    if ((vario_cms >= -(float)cfg->sink_deadband_cms) &&
        (vario_cms <=  (float)cfg->climb_deadband_cms))
    {
        return;
    }

    if ((int32_t)(now_ms - s_alt_rt.audio_next_beep_ms) < 0)
    {
        return;
    }

    if (vario_cms > (float)cfg->climb_deadband_cms)
    {
        magnitude_norm = (vario_cms - (float)cfg->climb_deadband_cms) /
                         APP_ALTITUDE_AUDIO_MAX_CLIMB_CMS;
        magnitude_norm = APP_ALT_ClampFloat(magnitude_norm, 0.0f, 1.0f);

        freq_hz = cfg->vario_beep_base_hz +
                  (uint32_t)((float)cfg->vario_beep_span_hz * magnitude_norm);

        on_ms = cfg->vario_beep_max_on_ms -
                (uint32_t)((float)(cfg->vario_beep_max_on_ms - cfg->vario_beep_min_on_ms) * magnitude_norm);

        period_ms = cfg->vario_beep_max_period_ms -
                    (uint32_t)((float)(cfg->vario_beep_max_period_ms - cfg->vario_beep_min_period_ms) * magnitude_norm);
    }
    else
    {
        magnitude_norm = ((-vario_cms) - (float)cfg->sink_deadband_cms) /
                         APP_ALTITUDE_AUDIO_MAX_SINK_CMS;
        magnitude_norm = APP_ALT_ClampFloat(magnitude_norm, 0.0f, 1.0f);

        freq_hz = APP_ALT_ClampU32((uint32_t)((float)cfg->vario_beep_base_hz * 0.45f) +
                                   (uint32_t)((float)cfg->vario_beep_span_hz * 0.20f * magnitude_norm),
                                   120u,
                                   1800u);

        on_ms = cfg->vario_beep_max_on_ms +
                (uint32_t)(80.0f * magnitude_norm);

        period_ms = on_ms + 60u;
    }

    if (Audio_Driver_PlaySineWave(freq_hz, on_ms) == HAL_OK)
    {
        s_alt_rt.audio_tone_active = 1u;
        s_alt_rt.audio_tone_off_ms = now_ms + on_ms;
        s_alt_rt.audio_next_beep_ms = now_ms + period_ms;

        alt->vario_audio_active = 1u;
        alt->vario_audio_last_freq_hz = (uint16_t)freq_hz;
        alt->vario_audio_last_on_ms = (uint16_t)on_ms;
        alt->vario_audio_last_period_ms = (uint16_t)period_ms;
    }
}

/* -------------------------------------------------------------------------- */
/*  공개 API: init                                                             */
/* -------------------------------------------------------------------------- */
void APP_ALTITUDE_Init(void)
{
    app_altitude_state_t *alt;

    alt = (app_altitude_state_t *)&g_app_state.altitude;

    memset(&s_alt_rt, 0, sizeof(s_alt_rt));
    memset((void *)alt, 0, sizeof(*alt));

    alt->initialized = true;
    alt->qnh_manual_hpa_x100 = g_app_state.settings.altitude.manual_qnh_hpa_x100;
    alt->output_alt_mode = g_app_state.settings.altitude.output_alt_mode;
    alt->pressure_hpa_x100_raw = 0;
    alt->pressure_hpa_x100_filt = 0;
    alt->qnh_equiv_gps_hpa_x100 = g_app_state.settings.altitude.manual_qnh_hpa_x100;

    s_alt_rt.output_mode_prev = g_app_state.settings.altitude.output_alt_mode;
    s_alt_rt.use_imu_prev = g_app_state.settings.altitude.use_imu_assist;
    s_alt_rt.last_manual_qnh_hpa_x100 = g_app_state.settings.altitude.manual_qnh_hpa_x100;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: home 기준 강제 재설정 요청                                         */
/* -------------------------------------------------------------------------- */
void APP_ALTITUDE_ResetHomeNow(uint32_t now_ms)
{
    app_altitude_state_t *alt;

    alt = (app_altitude_state_t *)&g_app_state.altitude;
    s_alt_rt.home_reset_pending = 1u;

    if (alt != 0)
    {
        alt->home_capture_ms = now_ms;
    }
}

/* -------------------------------------------------------------------------- */
/*  공개 API: task                                                             */
/* -------------------------------------------------------------------------- */
void APP_ALTITUDE_Task(uint32_t now_ms)
{
    app_altitude_state_t *alt;
    app_altitude_settings_t cfg;
    const app_gy86_state_t *imu;
    const gps_fix_basic_t *fix;
    float gps_weight_pct;
    float gps_sigma_cm;
    uint8_t gps_usable;
    uint32_t horiz_speed_mmps;

    alt = (app_altitude_state_t *)&g_app_state.altitude;
    imu = (const app_gy86_state_t *)&g_app_state.gy86;
    fix = (const gps_fix_basic_t *)&g_app_state.gps.fix;

    cfg = g_app_state.settings.altitude;
    APP_ALTITUDE_ClampSettings(&cfg);
    memcpy((void *)&g_app_state.settings.altitude, (const void *)&cfg, sizeof(cfg));

    alt->initialized = true;
    alt->qnh_manual_hpa_x100 = cfg.manual_qnh_hpa_x100;

    /* ---------------------------------------------------------------------- */
    /*  manual QNH가 바뀌면 raw spike gate 기준을 다시 seed한다.                */
    /* ---------------------------------------------------------------------- */
    if (cfg.manual_qnh_hpa_x100 != s_alt_rt.last_manual_qnh_hpa_x100)
    {
        s_alt_rt.baro_gate_seeded = 0u;
        s_alt_rt.last_manual_qnh_hpa_x100 = cfg.manual_qnh_hpa_x100;
    }

    /* ---------------------------------------------------------------------- */
    /*  IMU path는 "비교용 병렬 채널" 이므로                                     */
    /*  use_imu_assist가 꺼져 있어도 계속 계산한다.                              */
    /*  단, primary/output 선택만 no-IMU로 유지된다.                            */
    /* ---------------------------------------------------------------------- */
    APP_ALT_UpdateImuAttitudeAndVerticalAccel(imu, &cfg, alt);

    /* ---------------------------------------------------------------------- */
    /*  no-IMU / IMU filter time을 MPU timestamp까지 먼저 advance 한다.         */
    /*  이렇게 하면 IMU-assisted filter가 baro/gps update보다 앞서 예측된다.    */
    /* ---------------------------------------------------------------------- */
    if ((alt->last_mpu_update_ms != 0u) && (s_alt_rt.imu_kf.seeded != 0u))
    {
        APP_ALT_PredictImu(&s_alt_rt.imu_kf,
                           alt->last_mpu_update_ms,
                           s_alt_rt.last_imu_vertical_accel_cms2,
                           &cfg);
    }

    /* ---------------------------------------------------------------------- */
    /*  새 baro 샘플 처리                                                        */
    /* ---------------------------------------------------------------------- */
    if (((imu->status_flags & APP_GY86_STATUS_BARO_VALID) != 0u) &&
        (imu->baro.sample_count != s_alt_rt.last_baro_sample_count))
    {
        float raw_pressure_hpa;
        float raw_std_alt_cm;
        float median_std_alt_cm;
        float dt_baro_s;
        float raw_qnh_manual_cm;
        float pressure_filt_hpa;
        float initial_bias_cm;

        raw_pressure_hpa = (float)imu->baro.pressure_hpa_x100 * 0.01f;
        raw_std_alt_cm = APP_ALT_PressureToAltitudeCm(raw_pressure_hpa,
                                                      APP_ALTITUDE_PRESSURE_STD_HPA);
        median_std_alt_cm = APP_ALT_PushAndGetBaroMedian(raw_std_alt_cm);

        if (s_alt_rt.baro_gate_seeded == 0u)
        {
            s_alt_rt.last_accepted_baro_std_cm = median_std_alt_cm;
            s_alt_rt.baro_gate_seeded = 1u;
        }
        else if (APP_ALT_AbsFloat(median_std_alt_cm - s_alt_rt.last_accepted_baro_std_cm) >
                 (float)cfg.baro_spike_reject_cm)
        {
            alt->rejected_baro_spike_count++;
            s_alt_rt.last_baro_sample_count = imu->baro.sample_count;
            goto baro_done;
        }
        else
        {
            s_alt_rt.last_accepted_baro_std_cm = median_std_alt_cm;
        }

        alt->flags |= APP_ALT_FLAG_BARO_VALID;
        alt->last_baro_update_ms = imu->baro.timestamp_ms;
        alt->baro_sample_count++;
        alt->pressure_hpa_x100_raw = imu->baro.pressure_hpa_x100;
        alt->temp_cdeg_baro = imu->baro.temp_cdeg;

        dt_baro_s = APP_ALT_DtSecondsFromMs(imu->baro.timestamp_ms,
                                            alt->last_update_ms == 0u ? imu->baro.timestamp_ms : alt->last_update_ms);

        if (s_alt_rt.pressure_seeded == 0u)
        {
            s_alt_rt.pressure_lpf_hpa = raw_pressure_hpa;
            s_alt_rt.pressure_seeded = 1u;
        }
        else
        {
            s_alt_rt.pressure_lpf_hpa = APP_ALT_LpfFloat(s_alt_rt.pressure_lpf_hpa,
                                                         raw_pressure_hpa,
                                                         dt_baro_s,
                                                         cfg.pressure_lpf_tau_ms);
        }

        pressure_filt_hpa = s_alt_rt.pressure_lpf_hpa;
        alt->pressure_hpa_x100_filt = (int32_t)(pressure_filt_hpa * 100.0f);

        alt->alt_pressure_std_cm = (int32_t)APP_ALT_PressureToAltitudeCm(pressure_filt_hpa,
                                                                          APP_ALTITUDE_PRESSURE_STD_HPA);
        raw_qnh_manual_cm = APP_ALT_PressureToAltitudeCm(pressure_filt_hpa,
                                                         (float)cfg.manual_qnh_hpa_x100 * 0.01f);
        alt->alt_qnh_manual_cm = (int32_t)raw_qnh_manual_cm;

        if (s_alt_rt.qnh_equiv_seeded != 0u)
        {
            alt->alt_qnh_gps_equiv_cm = (int32_t)APP_ALT_PressureToAltitudeCm(pressure_filt_hpa,
                                                                               s_alt_rt.qnh_equiv_lpf_hpa);
            alt->qnh_equiv_gps_hpa_x100 = (int32_t)(s_alt_rt.qnh_equiv_lpf_hpa * 100.0f);
        }
        else
        {
            alt->alt_qnh_gps_equiv_cm = alt->alt_qnh_manual_cm;
            alt->qnh_equiv_gps_hpa_x100 = cfg.manual_qnh_hpa_x100;
        }

        if (s_alt_rt.noimu_kf.seeded == 0u)
        {
            initial_bias_cm = (float)alt->alt_gps_hmsl_cm - raw_qnh_manual_cm;
            APP_ALT_SeedNoImuFilter(&s_alt_rt.noimu_kf,
                                    raw_qnh_manual_cm,
                                    initial_bias_cm,
                                    imu->baro.timestamp_ms);
        }
        else
        {
            APP_ALT_PredictNoImu(&s_alt_rt.noimu_kf,
                                 imu->baro.timestamp_ms,
                                 &cfg);
        }
        APP_ALT_UpdateNoImuBaro(&s_alt_rt.noimu_kf,
                                raw_qnh_manual_cm,
                                (float)cfg.baro_noise_cm);

        if (s_alt_rt.imu_kf.seeded == 0u)
        {
            initial_bias_cm = (float)alt->alt_gps_hmsl_cm - raw_qnh_manual_cm;
            APP_ALT_SeedImuFilter(&s_alt_rt.imu_kf,
                                  raw_qnh_manual_cm,
                                  initial_bias_cm,
                                  imu->baro.timestamp_ms);
        }
        else
        {
            APP_ALT_PredictImu(&s_alt_rt.imu_kf,
                               imu->baro.timestamp_ms,
                               s_alt_rt.last_imu_vertical_accel_cms2,
                               &cfg);
        }
        APP_ALT_UpdateImuBaro(&s_alt_rt.imu_kf,
                              raw_qnh_manual_cm,
                              (float)cfg.baro_noise_cm);

        alt->alt_fused_noimu_cm = (int32_t)s_alt_rt.noimu_kf.x[0];
        alt->baro_bias_noimu_cm = (int32_t)s_alt_rt.noimu_kf.x[2];
        alt->alt_fused_imu_cm   = (int32_t)s_alt_rt.imu_kf.x[0];
        alt->baro_bias_imu_cm   = (int32_t)s_alt_rt.imu_kf.x[2];
        alt->accel_bias_mmps2   = (int32_t)(s_alt_rt.imu_kf.x[3] * 10.0f);

        alt->vario_fast_noimu_cms = (int32_t)APP_ALT_LpfFloat(s_alt_rt.last_vario_fast_noimu_cms,
                                                              s_alt_rt.noimu_kf.x[1],
                                                              dt_baro_s,
                                                              cfg.vario_fast_lpf_tau_ms);
        s_alt_rt.last_vario_fast_noimu_cms = (float)alt->vario_fast_noimu_cms;

        alt->vario_slow_noimu_cms = (int32_t)APP_ALT_LpfFloat(s_alt_rt.last_vario_slow_noimu_cms,
                                                              s_alt_rt.noimu_kf.x[1],
                                                              dt_baro_s,
                                                              cfg.vario_slow_lpf_tau_ms);
        s_alt_rt.last_vario_slow_noimu_cms = (float)alt->vario_slow_noimu_cms;

        alt->vario_fast_imu_cms = (int32_t)APP_ALT_LpfFloat(s_alt_rt.last_vario_fast_imu_cms,
                                                            s_alt_rt.imu_kf.x[1],
                                                            dt_baro_s,
                                                            cfg.vario_fast_lpf_tau_ms);
        s_alt_rt.last_vario_fast_imu_cms = (float)alt->vario_fast_imu_cms;

        alt->vario_slow_imu_cms = (int32_t)APP_ALT_LpfFloat(s_alt_rt.last_vario_slow_imu_cms,
                                                            s_alt_rt.imu_kf.x[1],
                                                            dt_baro_s,
                                                            cfg.vario_slow_lpf_tau_ms);
        s_alt_rt.last_vario_slow_imu_cms = (float)alt->vario_slow_imu_cms;

baro_done:
        s_alt_rt.last_baro_sample_count = imu->baro.sample_count;
    }

    /* ---------------------------------------------------------------------- */
    /*  새 GPS 샘플 처리                                                        */
    /* ---------------------------------------------------------------------- */
    gps_usable = APP_ALT_IsGpsUsable(fix,
                                     &cfg,
                                     now_ms,
                                     &gps_weight_pct,
                                     &gps_sigma_cm);

    if ((gps_usable != 0u) && (fix->last_update_ms != s_alt_rt.last_gps_update_ms))
    {
        float gps_alt_cm;
        uint8_t gps_used;

        alt->flags |= APP_ALT_FLAG_GPS_VALID;
        alt->gps_weight_pct = (uint8_t)gps_weight_pct;

        gps_alt_cm = (float)fix->hMSL * 0.1f;
        alt->alt_gps_hmsl_cm = (int32_t)gps_alt_cm;
        alt->last_gps_update_ms = fix->last_update_ms;
        alt->gps_sample_count++;

        if (s_alt_rt.gps_alt_seeded == 0u)
        {
            s_alt_rt.gps_alt_lpf_cm = gps_alt_cm;
            s_alt_rt.gps_alt_seeded = 1u;
        }
        else
        {
            s_alt_rt.gps_alt_lpf_cm = APP_ALT_LpfFloat(s_alt_rt.gps_alt_lpf_cm,
                                                       gps_alt_cm,
                                                       APP_ALT_DtSecondsFromMs(fix->last_update_ms,
                                                                               s_alt_rt.last_gps_update_ms == 0u ? fix->last_update_ms : s_alt_rt.last_gps_update_ms),
                                                       cfg.gps_alt_lpf_tau_ms);
        }

        if ((s_alt_rt.pressure_seeded != 0u) && (s_alt_rt.gps_alt_seeded != 0u))
        {
            float qnh_equiv_raw_hpa;

            qnh_equiv_raw_hpa = APP_ALT_AltitudeToQnhHpa(s_alt_rt.gps_alt_lpf_cm,
                                                         s_alt_rt.pressure_lpf_hpa);

            if (s_alt_rt.qnh_equiv_seeded == 0u)
            {
                s_alt_rt.qnh_equiv_lpf_hpa = qnh_equiv_raw_hpa;
                s_alt_rt.qnh_equiv_seeded = 1u;
            }
            else
            {
                s_alt_rt.qnh_equiv_lpf_hpa = APP_ALT_LpfFloat(s_alt_rt.qnh_equiv_lpf_hpa,
                                                              qnh_equiv_raw_hpa,
                                                              APP_ALT_DtSecondsFromMs(fix->last_update_ms,
                                                                                      s_alt_rt.last_gps_update_ms == 0u ? fix->last_update_ms : s_alt_rt.last_gps_update_ms),
                                                              cfg.qnh_equiv_lpf_tau_ms);
            }

            alt->qnh_equiv_gps_hpa_x100 = (int32_t)(s_alt_rt.qnh_equiv_lpf_hpa * 100.0f);
            if (s_alt_rt.pressure_seeded != 0u)
            {
                alt->alt_qnh_gps_equiv_cm = (int32_t)APP_ALT_PressureToAltitudeCm(s_alt_rt.pressure_lpf_hpa,
                                                                                   s_alt_rt.qnh_equiv_lpf_hpa);
            }
        }

        if (s_alt_rt.noimu_kf.seeded == 0u)
        {
            APP_ALT_SeedNoImuFilter(&s_alt_rt.noimu_kf,
                                    (float)alt->alt_qnh_manual_cm,
                                    gps_alt_cm - (float)alt->alt_qnh_manual_cm,
                                    fix->last_update_ms);
        }
        else
        {
            APP_ALT_PredictNoImu(&s_alt_rt.noimu_kf,
                                 fix->last_update_ms,
                                 &cfg);
        }
        gps_used = APP_ALT_UpdateNoImuGps(&s_alt_rt.noimu_kf,
                                          gps_alt_cm,
                                          gps_sigma_cm);
        if (gps_used == 0u)
        {
            alt->rejected_gps_count++;
        }

        if (s_alt_rt.imu_kf.seeded == 0u)
        {
            APP_ALT_SeedImuFilter(&s_alt_rt.imu_kf,
                                  (float)alt->alt_qnh_manual_cm,
                                  gps_alt_cm - (float)alt->alt_qnh_manual_cm,
                                  fix->last_update_ms);
        }
        else
        {
            APP_ALT_PredictImu(&s_alt_rt.imu_kf,
                               fix->last_update_ms,
                               s_alt_rt.last_imu_vertical_accel_cms2,
                               &cfg);
        }
        gps_used = APP_ALT_UpdateImuGps(&s_alt_rt.imu_kf,
                                        gps_alt_cm,
                                        gps_sigma_cm);
        if (gps_used == 0u)
        {
            alt->rejected_gps_count++;
        }
        else
        {
            alt->flags |= APP_ALT_FLAG_GPS_USED;
        }

        alt->alt_fused_noimu_cm = (int32_t)s_alt_rt.noimu_kf.x[0];
        alt->baro_bias_noimu_cm = (int32_t)s_alt_rt.noimu_kf.x[2];
        alt->alt_fused_imu_cm   = (int32_t)s_alt_rt.imu_kf.x[0];
        alt->baro_bias_imu_cm   = (int32_t)s_alt_rt.imu_kf.x[2];
        alt->accel_bias_mmps2   = (int32_t)(s_alt_rt.imu_kf.x[3] * 10.0f);

        s_alt_rt.last_gps_update_ms = fix->last_update_ms;
    }
    else
    {
        alt->gps_weight_pct = 0u;
    }

    /* ---------------------------------------------------------------------- */
    /*  home 기준점                                                              */
    /*                                                                        */
    /*  - 처음 유효한 fused altitude가 생기면 자동으로 1회 캡처                   */
    /*  - debug page B6 long으로 언제든 강제 재설정 가능                         */
    /* ---------------------------------------------------------------------- */
    if ((s_alt_rt.home_valid == 0u) &&
        ((alt->flags & APP_ALT_FLAG_BARO_VALID) != 0u))
    {
        APP_ALT_CaptureHomeNow(alt, now_ms);
    }
    else if (s_alt_rt.home_reset_pending != 0u)
    {
        APP_ALT_CaptureHomeNow(alt, now_ms);
    }
    /* ---------------------------------------------------------------------- */
    /*  flags는 매 task에서 "현재 상태" 기준으로 다시 구성한다.                  */
    /*                                                                        */
    /*  이유                                                                    */
    /*  - 센서 task와 altitude task의 호출 주기가 완전히 같지 않으므로             */
    /*    "이번 루프에 새 샘플이 왔는가" 와                                      */
    /*    "현재 값이 아직 유효한가" 를 분리해야 한다.                            */
    /* ---------------------------------------------------------------------- */
    alt->flags = 0u;

    if ((alt->last_baro_update_ms != 0u) &&
        ((uint32_t)(now_ms - alt->last_baro_update_ms) <= 1500u))
    {
        alt->flags |= APP_ALT_FLAG_BARO_VALID;
    }

    if ((alt->last_gps_update_ms != 0u) &&
        ((uint32_t)(now_ms - alt->last_gps_update_ms) <= 1500u) &&
        (gps_usable != 0u))
    {
        alt->flags |= APP_ALT_FLAG_GPS_VALID;
        alt->flags |= APP_ALT_FLAG_GPS_USED;
    }

    if ((alt->last_mpu_update_ms != 0u) &&
        ((uint32_t)(now_ms - alt->last_mpu_update_ms) <= 500u))
    {
        alt->flags |= APP_ALT_FLAG_IMU_VALID;

        if (alt->imu_trust_pct >= 20u)
        {
            alt->flags |= APP_ALT_FLAG_IMU_CONFIDENT;
        }
    }

    if (s_alt_rt.home_valid != 0u)
    {
        alt->flags |= APP_ALT_FLAG_HOME_VALID;
    }

    if (cfg.vario_audio_enable != 0u)
    {
        alt->flags |= APP_ALT_FLAG_VARIO_AUDIO_EN;
    }

    if ((alt->flags & APP_ALT_FLAG_HOME_VALID) != 0u)
    {
        alt->alt_rel_home_noimu_cm = alt->alt_fused_noimu_cm - (int32_t)s_alt_rt.home_noimu_cm;
        alt->alt_rel_home_imu_cm   = alt->alt_fused_imu_cm   - (int32_t)s_alt_rt.home_imu_cm;
    }
    else
    {
        alt->alt_rel_home_noimu_cm = 0;
        alt->alt_rel_home_imu_cm = 0;
    }

    horiz_speed_mmps = APP_ALT_GetHorizontalSpeedMmps(fix);
    alt->grade_noimu_x10 = APP_ALT_ComputeGradeX10(alt->vario_slow_noimu_cms,
                                                   horiz_speed_mmps,
                                                   cfg.grade_min_speed_mmps);
    alt->grade_imu_x10   = APP_ALT_ComputeGradeX10(alt->vario_slow_imu_cms,
                                                   horiz_speed_mmps,
                                                   cfg.grade_min_speed_mmps);

    /* ---------------------------------------------------------------------- */
    /*  현재 출력 채널 선택 + 화면용 LPF                                         */
    /* ---------------------------------------------------------------------- */
    APP_ALT_SelectOutputChannel(&cfg, alt);

    if ((cfg.output_alt_mode != s_alt_rt.output_mode_prev) ||
        (cfg.use_imu_assist != s_alt_rt.use_imu_prev) ||
        (s_alt_rt.display_seeded == 0u) ||
        (APP_ALT_AbsFloat(s_alt_rt.display_alt_lpf_cm - (float)alt->alt_output_cm) < APP_ALTITUDE_OUTPUT_SWITCH_SEED_TOLERANCE_CM))
    {
        s_alt_rt.display_alt_lpf_cm = (float)alt->alt_output_cm;
        s_alt_rt.display_seeded = 1u;
        s_alt_rt.output_mode_prev = cfg.output_alt_mode;
        s_alt_rt.use_imu_prev = cfg.use_imu_assist;
    }
    else
    {
        s_alt_rt.display_alt_lpf_cm = APP_ALT_LpfFloat(s_alt_rt.display_alt_lpf_cm,
                                                       (float)alt->alt_output_cm,
                                                       APP_ALT_DtSecondsFromMs(now_ms,
                                                                               alt->last_update_ms == 0u ? now_ms : alt->last_update_ms),
                                                       cfg.display_alt_lpf_tau_ms);
    }
    alt->alt_display_cm = (int32_t)s_alt_rt.display_alt_lpf_cm;

    APP_ALT_UpdateVarioAudio(&cfg, alt, now_ms);

    alt->last_update_ms = now_ms;
}
