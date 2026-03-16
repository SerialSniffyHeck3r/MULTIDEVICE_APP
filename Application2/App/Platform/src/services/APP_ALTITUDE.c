#include "APP_ALTITUDE.h"

#include "Audio_Driver.h"

#include <math.h>
#include <string.h>

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

/* -------------------------------------------------------------------------- */
/*  내부 런타임 저장소                                                         */
/* -------------------------------------------------------------------------- */
typedef struct
{
    bool initialized;
    bool ui_active;
    bool audio_owned;
    bool home_capture_request;
    bool bias_rezero_request;

    uint32_t last_task_ms;
    uint32_t last_audio_ms;
    uint32_t last_audio_owner_ms;
    uint32_t last_baro_sample_count;
    uint32_t last_gps_fix_update_ms;

    float pressure_filt_hpa;
    float pressure_residual_hpa;
    float baro_residual_lp_cm;
    float adaptive_baro_noise_cm;
    float qnh_equiv_filt_hpa;
    float display_alt_filt_cm;

    float gravity_lp_x_g;
    float gravity_lp_y_g;
    float gravity_lp_z_g;
    float imu_vertical_lp_cms2;

    float home_noimu_cm;
    float home_imu_cm;

    float vario_fast_noimu_cms;
    float vario_slow_noimu_cms;
    float vario_fast_imu_cms;
    float vario_slow_imu_cms;

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

static float APP_ALTITUDE_GetDebugAudioSourceVarioCms(const app_altitude_settings_t *settings)
{
    if ((settings != 0) && (settings->debug_audio_source != 0u))
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

static void APP_ALTITUDE_Kf4_Predict(float x[4], float P[4][4], float dt_s, float accel_cms2, const app_altitude_settings_t *settings)
{
    float dt2;
    float corrected_accel;
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

    x[0] += x[1] * dt_s + 0.5f * corrected_accel * dt2;
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
                                                      float dt_s,
                                                      bool *out_vector_valid)
{
    float lsb_per_g;
    float ax_g;
    float ay_g;
    float az_g;
    float gravity_alpha;
    float grav_norm_g;
    float gx_hat;
    float gy_hat;
    float gz_hat;
    float dyn_x_g;
    float dyn_y_g;
    float dyn_z_g;
    float vertical_dyn_g;
    float deadband_g;
    float clip_g;
    float sign_f;

    if (out_vector_valid != 0)
    {
        *out_vector_valid = false;
    }

    if ((settings == 0) || (mpu == 0) || (settings->imu_accel_lsb_per_g == 0u))
    {
        return 0.0f;
    }

    lsb_per_g = (float)settings->imu_accel_lsb_per_g;
    ax_g = ((float)mpu->accel_x_raw) / lsb_per_g;
    ay_g = ((float)mpu->accel_y_raw) / lsb_per_g;
    az_g = ((float)mpu->accel_z_raw) / lsb_per_g;

    gravity_alpha = APP_ALTITUDE_LpfAlphaFromTauMs(settings->imu_gravity_tau_ms, dt_s);

    if ((s_altitude_runtime.gravity_lp_x_g == 0.0f) &&
        (s_altitude_runtime.gravity_lp_y_g == 0.0f) &&
        (s_altitude_runtime.gravity_lp_z_g == 0.0f))
    {
        s_altitude_runtime.gravity_lp_x_g = ax_g;
        s_altitude_runtime.gravity_lp_y_g = ay_g;
        s_altitude_runtime.gravity_lp_z_g = az_g;
    }
    else
    {
        s_altitude_runtime.gravity_lp_x_g += gravity_alpha * (ax_g - s_altitude_runtime.gravity_lp_x_g);
        s_altitude_runtime.gravity_lp_y_g += gravity_alpha * (ay_g - s_altitude_runtime.gravity_lp_y_g);
        s_altitude_runtime.gravity_lp_z_g += gravity_alpha * (az_g - s_altitude_runtime.gravity_lp_z_g);
    }

    grav_norm_g = sqrtf(APP_ALTITUDE_SquareF(s_altitude_runtime.gravity_lp_x_g) +
                        APP_ALTITUDE_SquareF(s_altitude_runtime.gravity_lp_y_g) +
                        APP_ALTITUDE_SquareF(s_altitude_runtime.gravity_lp_z_g));

    if (grav_norm_g < 0.25f)
    {
        return 0.0f;
    }

    gx_hat = s_altitude_runtime.gravity_lp_x_g / grav_norm_g;
    gy_hat = s_altitude_runtime.gravity_lp_y_g / grav_norm_g;
    gz_hat = s_altitude_runtime.gravity_lp_z_g / grav_norm_g;

    dyn_x_g = ax_g - s_altitude_runtime.gravity_lp_x_g;
    dyn_y_g = ay_g - s_altitude_runtime.gravity_lp_y_g;
    dyn_z_g = az_g - s_altitude_runtime.gravity_lp_z_g;

    vertical_dyn_g = (dyn_x_g * gx_hat) + (dyn_y_g * gy_hat) + (dyn_z_g * gz_hat);

    sign_f = (settings->imu_vertical_sign >= 0) ? 1.0f : -1.0f;
    vertical_dyn_g *= sign_f;

    deadband_g = ((float)settings->imu_vertical_deadband_mg) * 0.001f;
    clip_g     = ((float)settings->imu_vertical_clip_mg) * 0.001f;

    if (fabsf(vertical_dyn_g) < deadband_g)
    {
        vertical_dyn_g = 0.0f;
    }
    vertical_dyn_g = APP_ALTITUDE_ClampF(vertical_dyn_g, -clip_g, clip_g);

    s_altitude_runtime.imu_vertical_lp_cms2 = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.imu_vertical_lp_cms2,
                                                                     vertical_dyn_g * APP_ALTITUDE_GRAVITY_MPS2 * 100.0f,
                                                                     settings->imu_accel_tau_ms,
                                                                     dt_s);

    if (out_vector_valid != 0)
    {
        *out_vector_valid = true;
    }

    g_app_state.altitude.imu_gravity_norm_mg = APP_ALTITUDE_RoundFloatToS32(grav_norm_g * 1000.0f);
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

static void APP_ALTITUDE_HandleDebugAudio(uint32_t now_ms, const app_altitude_settings_t *settings)
{
    float vario_cms;
    float speed_abs_cms;
    float scale;
    uint32_t period_ms;
    uint32_t tone_ms;
    uint32_t gap_ms;
    uint32_t freq_hz;
    uint32_t sink_freq_min_hz;
    uint32_t sink_freq_max_hz;
    uint32_t climb_freq_min_hz;
    uint32_t climb_freq_max_hz;
    HAL_StatusTypeDef status;

    if ((settings == 0) || (s_altitude_runtime.ui_active == false) || (settings->debug_audio_enabled == 0u))
    {
        g_app_state.altitude.debug_audio_active = 0u;
        g_app_state.altitude.debug_audio_vario_cms = 0;
        APP_ALTITUDE_StopOwnedAudioIfNeeded();
        return;
    }

    g_app_state.altitude.debug_audio_active = 1u;

    /* ------------------------------------------------------------------ */
    /*  사용자가 현재 비교 청취하고 싶은 vario source를 고른다.             */
    /*  - 0 : no-IMU fast vario                                           */
    /*  - 1 : IMU-aided fast vario                                        */
    /* ------------------------------------------------------------------ */
    vario_cms = APP_ALTITUDE_GetDebugAudioSourceVarioCms(settings);
    g_app_state.altitude.debug_audio_vario_cms = APP_ALTITUDE_RoundFloatToS32(vario_cms);

    speed_abs_cms = fabsf(vario_cms);

    if (speed_abs_cms < (float)settings->audio_deadband_cms)
    {
        /* -------------------------------------------------------------- */
        /*  deadband 안에서는 silence로 돌아간다.                         */
        /*  다만 다른 페이지/오디오 경로와 소유권 충돌을 줄이기 위해       */
        /*  owner 플래그는 짧은 timeout 후 자연 해제한다.                 */
        /* -------------------------------------------------------------- */
        if ((s_altitude_runtime.audio_owned != false) &&
            ((uint32_t)(now_ms - s_altitude_runtime.last_audio_owner_ms) > APP_ALTITUDE_UI_ONLY_AUDIO_OWNER_TIMEOUT_MS))
        {
            s_altitude_runtime.audio_owned = false;
        }
        return;
    }

    scale = (speed_abs_cms - (float)settings->audio_deadband_cms) /
            (APP_ALTITUDE_DEFAULT_CLIMB_FULL_SCALE_CMS - (float)settings->audio_deadband_cms);
    scale = APP_ALTITUDE_ClampF(scale, 0.0f, 1.0f);

    if (Audio_Driver_IsBusy() != false)
    {
        return;
    }

    /* ------------------------------------------------------------------ */
    /*  상승음                                                             */
    /*  - climb가 커질수록 pitch가 올라간다.                              */
    /*  - climb가 커질수록 beep cadence도 빨라진다.                       */
    /*  - 즉, Flytec류 바리오처럼 "높이 + 주기" 둘 다 살아 있는 방향.    */
    /* ------------------------------------------------------------------ */
    if (vario_cms >= 0.0f)
    {
        uint32_t climb_period_slow_ms;
        uint32_t climb_period_fast_ms;

        climb_freq_min_hz = settings->audio_min_freq_hz;
        climb_freq_max_hz = (settings->audio_max_freq_hz > climb_freq_min_hz) ?
                            settings->audio_max_freq_hz : climb_freq_min_hz;

        /* ------------------------------------------------------------------ */
        /*  audio_repeat_ms는 climb beep의 기본 cadence knob로 사용한다.      */
        /*  사용자가 이 값을 키우면 약한 상승 영역의 기본 간격이 느려지고,     */
        /*  줄이면 전체 climb cadence가 더 민첩하게 들린다.                   */
        /* ------------------------------------------------------------------ */
        climb_period_slow_ms = APP_ALTITUDE_ClampU32((uint32_t)settings->audio_repeat_ms + 140u,
                                                     APP_ALTITUDE_CLIMB_AUDIO_MIN_PERIOD_MS,
                                                     APP_ALTITUDE_CLIMB_AUDIO_MAX_PERIOD_MS);
        climb_period_fast_ms = APP_ALTITUDE_ClampU32((uint32_t)(settings->audio_repeat_ms / 2u),
                                                     APP_ALTITUDE_CLIMB_AUDIO_MIN_PERIOD_MS,
                                                     climb_period_slow_ms);

        period_ms = (uint32_t)((float)climb_period_slow_ms -
                               ((float)(climb_period_slow_ms - climb_period_fast_ms) * scale));
        period_ms = APP_ALTITUDE_ClampU32(period_ms,
                                          APP_ALTITUDE_CLIMB_AUDIO_MIN_PERIOD_MS,
                                          APP_ALTITUDE_CLIMB_AUDIO_MAX_PERIOD_MS);

        tone_ms = (uint32_t)((float)settings->audio_beep_ms * (1.20f - (0.25f * scale)) + 12.0f);
        tone_ms = APP_ALTITUDE_ClampU32(tone_ms, 18u, (period_ms > 8u) ? (period_ms - 8u) : period_ms);

        if ((uint32_t)(now_ms - s_altitude_runtime.last_audio_ms) < period_ms)
        {
            return;
        }

        /* ------------------------------------------------------------------ */
        /*  주파수는 약간 비선형으로 올려서                                    */
        /*  약한 상승에서도 "살아 있는" 느낌이 나고,                           */
        /*  강한 상승에서는 빠르게 고조되도록 한다.                            */
        /* ------------------------------------------------------------------ */
        freq_hz = (uint32_t)((float)climb_freq_min_hz +
                  ((float)(climb_freq_max_hz - climb_freq_min_hz) * powf(scale, 0.78f)));

        status = Audio_Driver_PlaySquareWaveMs(freq_hz, tone_ms);
    }
    else
    {
        /* ------------------------------------------------------------------ */
        /*  하강음                                                             */
        /*  - 삑삑거리는 sink beep가 아니라,                                   */
        /*    길게 이어지는 연속 tone + 아주 짧은 gap 형태로 만든다.           */
        /*  - 실제 체감은 "뚜우우~" 에 가깝게 들린다.                           */
        /* ------------------------------------------------------------------ */
        sink_freq_min_hz = (uint32_t)APP_ALTITUDE_ClampU32((uint32_t)((float)settings->audio_min_freq_hz * 0.28f), 90u, 2000u);
        sink_freq_max_hz = (uint32_t)APP_ALTITUDE_ClampU32((uint32_t)((float)settings->audio_min_freq_hz * 0.62f), sink_freq_min_hz + 20u, 4000u);

        /* ------------------------------------------------------------------ */
        /*  sink가 커질수록 더 낮고 더 촘촘한 연속 tone 쪽으로 몰아준다.       */
        /* ------------------------------------------------------------------ */
        tone_ms = (uint32_t)((float)APP_ALTITUDE_SINK_AUDIO_MAX_TONE_MS -
                             ((float)(APP_ALTITUDE_SINK_AUDIO_MAX_TONE_MS - APP_ALTITUDE_SINK_AUDIO_MIN_TONE_MS) * scale));
        tone_ms = APP_ALTITUDE_ClampU32(tone_ms,
                                        APP_ALTITUDE_SINK_AUDIO_MIN_TONE_MS,
                                        APP_ALTITUDE_SINK_AUDIO_MAX_TONE_MS);

        gap_ms = (uint32_t)((float)APP_ALTITUDE_SINK_AUDIO_MAX_GAP_MS -
                            ((float)(APP_ALTITUDE_SINK_AUDIO_MAX_GAP_MS - APP_ALTITUDE_SINK_AUDIO_MIN_GAP_MS) * scale));
        gap_ms = APP_ALTITUDE_ClampU32(gap_ms,
                                       APP_ALTITUDE_SINK_AUDIO_MIN_GAP_MS,
                                       APP_ALTITUDE_ClampU32((uint32_t)(settings->audio_repeat_ms / 4u),
                                                             APP_ALTITUDE_SINK_AUDIO_MAX_GAP_MS,
                                                             180u));

        period_ms = tone_ms + gap_ms;

        if ((uint32_t)(now_ms - s_altitude_runtime.last_audio_ms) < period_ms)
        {
            return;
        }

        freq_hz = (uint32_t)((float)sink_freq_max_hz -
                  ((float)(sink_freq_max_hz - sink_freq_min_hz) * scale));

        status = Audio_Driver_PlaySawToothWaveMs(freq_hz, tone_ms);
    }

    if (status == HAL_OK)
    {
        s_altitude_runtime.last_audio_ms = now_ms;
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

    g_app_state.altitude.initialized = true;
    g_app_state.altitude.qnh_manual_hpa_x100 = g_app_state.settings.altitude.manual_qnh_hpa_x100;
    g_app_state.altitude.qnh_equiv_gps_hpa_x100 = g_app_state.settings.altitude.manual_qnh_hpa_x100;
}

void APP_ALTITUDE_Task(uint32_t now_ms)
{
    app_altitude_settings_t settings_local;
    app_gy86_baro_raw_t baro_local;
    gps_fix_basic_t gps_fix_local;
    app_gy86_mpu_raw_t mpu_local;
    app_altitude_settings_t *settings;
    const app_gy86_baro_raw_t *baro;
    const gps_fix_basic_t *gps_fix;
    const app_gy86_mpu_raw_t *mpu;
    bool new_baro_sample;
    bool new_gps_sample;
    bool gps_valid;
    bool imu_vector_valid;
    uint16_t gps_quality_permille;
    float dt_s;
    float pressure_raw_hpa;
    float qnh_manual_hpa;
    float qnh_equiv_hpa;
    float alt_std_cm;
    float alt_qnh_cm;
    float alt_gps_cm;
    float gps_noise_cm;
    float baro_noise_cm;
    float imu_vertical_cms2;
    float display_target_cm;
    float display_source_vario_cms;
    uint32_t display_tau_ms;
    bool display_rest_active;
    float H_baro3[3];
    float H_gps3[3];
    float H_baro4[4];
    float H_gps4[4];

    /* ------------------------------------------------------------------ */
    /*  volatile APP_STATE를 함수 시작 시점에 로컬 snapshot으로 복사한다.  */
    /*  이렇게 하면                                                           */
    /*  - 한 번의 task 동안 입력이 일관되게 유지되고                         */
    /*  - volatile qualifier discard 경고 없이                               */
    /*    계산 helper에 안전하게 넘길 수 있다.                               */
    /* ------------------------------------------------------------------ */
    settings_local = g_app_state.settings.altitude;
    baro_local     = g_app_state.gy86.baro;
    gps_fix_local  = g_app_state.gps.fix;
    mpu_local      = g_app_state.gy86.mpu;

    settings = &settings_local;
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
    gps_quality_permille = 0u;
    imu_vector_valid = false;
    pressure_raw_hpa = 0.0f;
    alt_std_cm = 0.0f;
    alt_qnh_cm = 0.0f;
    alt_gps_cm = 0.0f;
    baro_noise_cm = (float)settings->baro_measurement_noise_cm;
    imu_vertical_cms2 = 0.0f;
    display_source_vario_cms = 0.0f;
    display_tau_ms = settings->display_lpf_tau_ms;
    display_rest_active = false;

    qnh_manual_hpa = ((float)settings->manual_qnh_hpa_x100) * 0.01f;
    qnh_manual_hpa = APP_ALTITUDE_ClampF(qnh_manual_hpa, 800.0f, 1100.0f);

    if ((baro->sample_count != 0u) && (baro->pressure_hpa_x100 > 0) && (baro->sample_count != s_altitude_runtime.last_baro_sample_count))
    {
        new_baro_sample = true;
        s_altitude_runtime.last_baro_sample_count = baro->sample_count;
        pressure_raw_hpa = ((float)baro->pressure_hpa_x100) * 0.01f;

        /* ------------------------------------------------------------------ */
        /*  residual은 "새 raw pressure - 직전 LPF pressure" 로 계산한다.       */
        /*  이 값은 adaptive baro trust 추정에 사용된다.                       */
        /* ------------------------------------------------------------------ */
        if ((s_altitude_runtime.pressure_filt_hpa <= 0.0f) || (g_app_state.altitude.baro_valid == false))
        {
            s_altitude_runtime.pressure_residual_hpa = 0.0f;
            s_altitude_runtime.pressure_filt_hpa = pressure_raw_hpa;
        }
        else
        {
            s_altitude_runtime.pressure_residual_hpa = pressure_raw_hpa - s_altitude_runtime.pressure_filt_hpa;
            s_altitude_runtime.pressure_filt_hpa = APP_ALTITUDE_LpfUpdate(s_altitude_runtime.pressure_filt_hpa,
                                                                          pressure_raw_hpa,
                                                                          settings->pressure_lpf_tau_ms,
                                                                          dt_s);
        }

        alt_std_cm = APP_ALTITUDE_PressureToAltitudeMeters(s_altitude_runtime.pressure_filt_hpa,
                                                           APP_ALTITUDE_STD_QNH_HPA) * 100.0f;
        alt_qnh_cm = APP_ALTITUDE_PressureToAltitudeMeters(s_altitude_runtime.pressure_filt_hpa,
                                                           qnh_manual_hpa) * 100.0f;

        /* ------------------------------------------------------------------ */
        /*  adaptive baro noise는 core altitude filter에만 적용한다.           */
        /*  raw/filt pressure와 QNH altitude는 그대로 유지하므로               */
        /*  logger/debug에서 원 신호도 계속 볼 수 있다.                        */
        /* ------------------------------------------------------------------ */
        baro_noise_cm = APP_ALTITUDE_ComputeAdaptiveBaroNoiseCm(settings,
                                                                s_altitude_runtime.pressure_filt_hpa,
                                                                s_altitude_runtime.pressure_residual_hpa,
                                                                dt_s);
    }
    else if (g_app_state.altitude.baro_valid != false)
    {
        pressure_raw_hpa = ((float)g_app_state.altitude.pressure_raw_hpa_x100) * 0.01f;
        alt_std_cm = ((float)g_app_state.altitude.alt_pressure_std_cm);
        alt_qnh_cm = ((float)g_app_state.altitude.alt_qnh_manual_cm);
        baro_noise_cm = (s_altitude_runtime.adaptive_baro_noise_cm > 0.0f) ?
                        s_altitude_runtime.adaptive_baro_noise_cm :
                        (float)settings->baro_measurement_noise_cm;
    }

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

    if ((settings->gps_auto_equiv_qnh_enabled != 0u) &&
        ((new_gps_sample != false) || (g_app_state.altitude.gps_valid != false)) &&
        ((new_baro_sample != false) || (g_app_state.altitude.baro_valid != false)) &&
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

    if (new_baro_sample != false)
    {
        APP_ALTITUDE_EnsureFiltersInitialized(alt_qnh_cm, alt_gps_cm, gps_valid);
        g_app_state.altitude.baro_valid = true;
        g_app_state.altitude.last_baro_update_ms = now_ms;
    }

    if (s_altitude_runtime.kf_noimu.valid != false)
    {
        APP_ALTITUDE_Kf3_Predict(s_altitude_runtime.kf_noimu.x,
                                 s_altitude_runtime.kf_noimu.P,
                                 dt_s,
                                 settings);
    }

    imu_vertical_cms2 = APP_ALTITUDE_UpdateImuVerticalAccelCms2(settings, mpu, dt_s, &imu_vector_valid);
    if (s_altitude_runtime.kf_imu.valid != false)
    {
        APP_ALTITUDE_Kf4_Predict(s_altitude_runtime.kf_imu.x,
                                 s_altitude_runtime.kf_imu.P,
                                 dt_s,
                                 imu_vertical_cms2,
                                 settings);
    }

    H_baro3[0] = 1.0f; H_baro3[1] = 0.0f; H_baro3[2] = 1.0f;
    H_gps3[0]  = 1.0f; H_gps3[1]  = 0.0f; H_gps3[2]  = 0.0f;
    H_baro4[0] = 1.0f; H_baro4[1] = 0.0f; H_baro4[2] = 1.0f; H_baro4[3] = 0.0f;
    H_gps4[0]  = 1.0f; H_gps4[1]  = 0.0f; H_gps4[2]  = 0.0f; H_gps4[3]  = 0.0f;

    if ((new_baro_sample != false) && (s_altitude_runtime.kf_noimu.valid != false))
    {
        APP_ALTITUDE_Kf3_UpdateScalar(s_altitude_runtime.kf_noimu.x,
                                      s_altitude_runtime.kf_noimu.P,
                                      H_baro3,
                                      alt_qnh_cm,
                                      APP_ALTITUDE_SquareF(baro_noise_cm));
    }
    if ((new_baro_sample != false) && (s_altitude_runtime.kf_imu.valid != false))
    {
        APP_ALTITUDE_Kf4_UpdateScalar(s_altitude_runtime.kf_imu.x,
                                      s_altitude_runtime.kf_imu.P,
                                      H_baro4,
                                      alt_qnh_cm,
                                      APP_ALTITUDE_SquareF(baro_noise_cm));
    }

    if ((new_gps_sample != false) && (gps_valid != false) && (settings->gps_bias_correction_enabled != 0u))
    {
        gps_noise_cm = APP_ALTITUDE_ComputeGpsMeasurementNoiseCm(settings, gps_fix);

        if (s_altitude_runtime.kf_noimu.valid != false)
        {
            APP_ALTITUDE_Kf3_UpdateScalar(s_altitude_runtime.kf_noimu.x,
                                          s_altitude_runtime.kf_noimu.P,
                                          H_gps3,
                                          alt_gps_cm,
                                          APP_ALTITUDE_SquareF(gps_noise_cm));
        }
        if (s_altitude_runtime.kf_imu.valid != false)
        {
            APP_ALTITUDE_Kf4_UpdateScalar(s_altitude_runtime.kf_imu.x,
                                          s_altitude_runtime.kf_imu.P,
                                          H_gps4,
                                          alt_gps_cm,
                                          APP_ALTITUDE_SquareF(gps_noise_cm));
        }

        g_app_state.altitude.last_gps_update_ms = now_ms;
    }

    if ((settings->auto_home_capture_enabled != 0u) &&
        (g_app_state.altitude.home_valid == false) &&
        (s_altitude_runtime.kf_noimu.valid != false))
    {
        s_altitude_runtime.home_capture_request = true;
    }

    APP_ALTITUDE_ApplyPendingActions(alt_qnh_cm);

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

    display_target_cm = (settings->imu_aid_enabled != 0u && (s_altitude_runtime.kf_imu.valid != false)) ?
                        s_altitude_runtime.kf_imu.x[0] :
                        (s_altitude_runtime.kf_noimu.valid ? s_altitude_runtime.kf_noimu.x[0] : alt_qnh_cm);

    display_source_vario_cms = (settings->imu_aid_enabled != 0u && (s_altitude_runtime.kf_imu.valid != false)) ?
                               s_altitude_runtime.vario_slow_imu_cms :
                               s_altitude_runtime.vario_slow_noimu_cms;

    display_rest_active = APP_ALTITUDE_IsDisplayRestActive(settings,
                                                           display_source_vario_cms,
                                                           imu_vertical_cms2,
                                                           imu_vector_valid);

    if (display_rest_active != false)
    {
        display_tau_ms = settings->rest_display_tau_ms;

        /* -------------------------------------------------------------- */
        /*  정지 상태에서 target과 display가 아주 조금만 다르면            */
        /*  숫자를 그대로 붙잡아 최종 표시 떨림을 줄인다.                 */
        /*  core state / vario에는 전혀 개입하지 않는다.                  */
        /* -------------------------------------------------------------- */
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

    if (g_app_state.altitude.home_valid != false)
    {
        g_app_state.altitude.alt_rel_home_noimu_cm = APP_ALTITUDE_RoundFloatToS32((s_altitude_runtime.kf_noimu.valid ? s_altitude_runtime.kf_noimu.x[0] : alt_qnh_cm) - s_altitude_runtime.home_noimu_cm);
        g_app_state.altitude.alt_rel_home_imu_cm   = APP_ALTITUDE_RoundFloatToS32((s_altitude_runtime.kf_imu.valid ? s_altitude_runtime.kf_imu.x[0] : alt_qnh_cm) - s_altitude_runtime.home_imu_cm);
    }
    else
    {
        g_app_state.altitude.alt_rel_home_noimu_cm = 0;
        g_app_state.altitude.alt_rel_home_imu_cm   = 0;
    }

    g_app_state.altitude.initialized             = true;
    g_app_state.altitude.baro_valid              = (new_baro_sample != false) || (g_app_state.altitude.baro_valid != false);
    g_app_state.altitude.gps_valid               = gps_valid;
    g_app_state.altitude.imu_vector_valid        = imu_vector_valid;
    g_app_state.altitude.gps_quality_permille    = gps_quality_permille;
    g_app_state.altitude.last_update_ms          = now_ms;

    g_app_state.altitude.pressure_raw_hpa_x100   = APP_ALTITUDE_RoundFloatToS32(pressure_raw_hpa * 100.0f);
    g_app_state.altitude.pressure_filt_hpa_x100  = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.pressure_filt_hpa * 100.0f);
    g_app_state.altitude.pressure_residual_hpa_x100 = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.pressure_residual_hpa * 100.0f);
    g_app_state.altitude.qnh_manual_hpa_x100     = settings->manual_qnh_hpa_x100;
    g_app_state.altitude.qnh_equiv_gps_hpa_x100  = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.qnh_equiv_filt_hpa * 100.0f);

    g_app_state.altitude.alt_pressure_std_cm     = APP_ALTITUDE_RoundFloatToS32(alt_std_cm);
    g_app_state.altitude.alt_qnh_manual_cm       = APP_ALTITUDE_RoundFloatToS32(alt_qnh_cm);
    g_app_state.altitude.alt_gps_hmsl_cm         = APP_ALTITUDE_RoundFloatToS32(alt_gps_cm);
    g_app_state.altitude.alt_fused_noimu_cm      = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.kf_noimu.valid ? s_altitude_runtime.kf_noimu.x[0] : alt_qnh_cm);
    g_app_state.altitude.alt_fused_imu_cm        = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.kf_imu.valid ? s_altitude_runtime.kf_imu.x[0] : alt_qnh_cm);
    g_app_state.altitude.alt_display_cm          = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.display_alt_filt_cm);

    g_app_state.altitude.home_alt_noimu_cm       = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.home_noimu_cm);
    g_app_state.altitude.home_alt_imu_cm         = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.home_imu_cm);

    g_app_state.altitude.baro_bias_noimu_cm      = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.kf_noimu.valid ? s_altitude_runtime.kf_noimu.x[2] : 0.0f);
    g_app_state.altitude.baro_bias_imu_cm        = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.kf_imu.valid ? s_altitude_runtime.kf_imu.x[2] : 0.0f);
    g_app_state.altitude.baro_noise_used_cm      = (uint16_t)APP_ALTITUDE_RoundFloatToS32(baro_noise_cm);
    g_app_state.altitude.display_rest_active     = display_rest_active ? 1u : 0u;
    g_app_state.altitude.debug_audio_source      = settings->debug_audio_source;

    g_app_state.altitude.vario_fast_noimu_cms    = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.vario_fast_noimu_cms);
    g_app_state.altitude.vario_slow_noimu_cms    = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.vario_slow_noimu_cms);
    g_app_state.altitude.vario_fast_imu_cms      = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.vario_fast_imu_cms);
    g_app_state.altitude.vario_slow_imu_cms      = APP_ALTITUDE_RoundFloatToS32(s_altitude_runtime.vario_slow_imu_cms);

    g_app_state.altitude.grade_noimu_x10         = APP_ALTITUDE_ComputeGradeX10(s_altitude_runtime.vario_slow_noimu_cms, gps_fix);
    g_app_state.altitude.grade_imu_x10           = APP_ALTITUDE_ComputeGradeX10(s_altitude_runtime.vario_slow_imu_cms, gps_fix);

    g_app_state.altitude.imu_vertical_accel_mg   = APP_ALTITUDE_RoundFloatToS32((imu_vertical_cms2 / (APP_ALTITUDE_GRAVITY_MPS2 * 100.0f)) * 1000.0f);
    g_app_state.altitude.imu_vertical_accel_cms2 = APP_ALTITUDE_RoundFloatToS32(imu_vertical_cms2);

    g_app_state.altitude.gps_vacc_mm             = gps_fix->vAcc;
    g_app_state.altitude.gps_pdop_x100           = gps_fix->pDOP;
    g_app_state.altitude.gps_numsv_used          = gps_fix->numSV_used;
    g_app_state.altitude.gps_fix_type            = gps_fix->fixType;

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
