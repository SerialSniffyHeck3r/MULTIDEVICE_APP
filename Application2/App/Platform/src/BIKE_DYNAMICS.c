#include "BIKE_DYNAMICS.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* -------------------------------------------------------------------------- */
/*  물리 상수 / 고정 파라미터                                                  */
/* -------------------------------------------------------------------------- */
#define BIKE_DYN_GRAVITY_MPS2                (9.80665f)
#define BIKE_DYN_GRAVITY_CMS2                (980.665f)
#define BIKE_DYN_GRAVITY_MMPS2               (9806.65f)
#define BIKE_DYN_ONE_G_MG                    (1000.0f)
#define BIKE_DYN_MIN_VEC_NORM                (0.000001f)
#define BIKE_DYN_MIN_DT_S                    (0.001f)
#define BIKE_DYN_MAX_DT_S                    (0.100f)
#define BIKE_DYN_DEFAULT_ACCEL_LSB_G         (8192.0f)  /* MPU6050 ±4g */
#define BIKE_DYN_DEFAULT_GYRO_LSB_DPS        (65.5f)    /* MPU6050 ±500dps */
#define BIKE_DYN_DEFAULT_STALE_MS            (250u)
#define BIKE_DYN_DEG2RAD                     (0.01745329251994329577f)
#define BIKE_DYN_RAD2DEG                     (57.295779513082320876f)

/* -------------------------------------------------------------------------- */
/*  Gyro bias calibration policy                                              */
/*                                                                            */
/*  이 값들은 "사용자가 TEST PAGE에서 long press 한 뒤"                        */
/*  바이크를 가만히 세워 두는 캘리브레이션 동작에 대한 기본 요구 시간을 정의한다.*/
/*                                                                            */
/*  TARGET_GOOD_MS                                                             */
/*  - 실제로 정지 조건을 만족한 샘플을 누적해야 하는 최소 시간                 */
/*                                                                            */
/*  TIMEOUT_MS                                                                 */
/*  - 사용자가 바이크를 계속 흔들거나, 샘플 품질이 나쁘면                      */
/*    영원히 기다리지 않고 실패로 종료하는 상한 시간                           */
/* -------------------------------------------------------------------------- */
#define BIKE_DYN_GYRO_CAL_SETTLE_MS          (600u)
#define BIKE_DYN_GYRO_CAL_TARGET_GOOD_MS     (1800u)
#define BIKE_DYN_GYRO_CAL_TIMEOUT_MS         (7000u)
#define BIKE_DYN_GYRO_CAL_MIN_SAMPLES        (90u)
#define BIKE_DYN_GYRO_CAL_MAX_SPEED_MMPS     (1500)
#define BIKE_DYN_GYRO_CAL_MAX_GYRO_DPS       (6.0f)

/* -------------------------------------------------------------------------- */
/*  내부 런타임                                                                 */
/*                                                                            */
/*  중요                                                                       */
/*  - 이 구조체는 "filter의 사적인 내부 상태" 이다.                             */
/*  - 외부 공개값은 반드시 g_app_state.bike 로만 나간다.                       */
/*  - future OBD service가 써 넣을 입력 필드는 app_state 쪽에 남겨 두고,        */
/*    이 런타임 구조체에는 그 입력을 해석한 중간 상태만 둔다.                  */
/* -------------------------------------------------------------------------- */
typedef struct
{
    bool     initialized;                 /* Init 함수가 한 번이라도 호출되었는가     */
    bool     gravity_valid;               /* gravity vector가 현재 유효한가           */
    bool     q_valid;                     /* Mahony quaternion이 유효한가             */
    bool     zero_valid;                  /* reset 기준 bike basis가 유효한가         */
    bool     zero_requested;              /* 다음 안전한 IMU 샘플에서 zero capture    */
    bool     hard_rezero_requested;       /* hard rezero 요청 pending                 */
    bool     auto_zero_done;              /* auto_zero_on_boot를 이미 수행했는가      */
    bool     imu_sample_valid;            /* 마지막 IMU sample이 유효한가             */

    /* ---------------------------------------------------------------------- */
    /*  gyro bias calibration runtime                                          */
    /* ---------------------------------------------------------------------- */
    bool     gyro_bias_valid;             /* gyro bias가 유효한 평균값으로 채워졌는가 */
    bool     gyro_bias_cal_requested;     /* UI/외부에서 calibration 요청이 들어왔나   */
    bool     gyro_bias_cal_active;        /* 현재 calibration 수행 중인가             */
    bool     gyro_bias_cal_last_success;  /* 마지막 calibration의 결과                */

    uint32_t init_ms;                     /* 서비스 init 시각                         */
    uint32_t last_task_ms;                /* 마지막 task 진입 시각                    */
    uint32_t last_imu_timestamp_ms;       /* 마지막으로 반영한 MPU timestamp_ms       */
    uint32_t last_mpu_sample_count;       /* 마지막으로 반영한 MPU sample_count       */
    uint32_t last_gnss_fix_update_ms;     /* 마지막으로 소비한 GNSS fix.last_update_ms */
    uint32_t last_zero_capture_ms;        /* 마지막 zero capture 시각                 */

    /* ---------------------------------------------------------------------- */
    /*  gyro bias calibration bookkeeping                                      */
    /* ---------------------------------------------------------------------- */
    uint32_t gyro_bias_cal_start_ms;      /* calibration 시작 시각                    */
    uint32_t gyro_bias_cal_good_ms;       /* 정지 조건을 만족한 누적 시간             */
    uint32_t gyro_bias_cal_sample_count;  /* 정지 조건을 만족한 누적 샘플 수          */
    uint32_t gyro_bias_cal_success_count; /* 성공적으로 끝난 calibration 누적 횟수    */
    uint32_t last_gyro_bias_cal_ms;       /* 마지막 calibration 종료 시각             */
    uint32_t gyro_bias_cal_settle_until_ms; /* 버튼/손떨림이 가라앉을 때까지 대기 시각 */

    /* ---------------------------------------------------------------------- */
    /*  attitude quaternion                                                    */
    /*                                                                        */
    /*  q는 "sensor/body -> world(level)" 회전을 나타내는 quaternion이다.       */
    /*  yaw absolute는 magnetometer가 없으므로 장기 drift가 가능하지만,         */
    /*  roll/pitch 및 gravity 방향 추정에는 충분하다.                           */
    /* ---------------------------------------------------------------------- */
    float q0;
    float q1;
    float q2;
    float q3;

    /* ---------------------------------------------------------------------- */
    /*  gravity_est_* 는 sensor frame에서 본 "world up" unit vector 이다.      */
    /*  즉, accelerometer raw가 정지 시 가리키는 +1g 방향과 같은 개념이다.      */
    /*  quaternion으로부터 매 샘플 다시 계산해서 저장한다.                     */
    /* ---------------------------------------------------------------------- */
    float gravity_est_x_s;
    float gravity_est_y_s;
    float gravity_est_z_s;

    /* ---------------------------------------------------------------------- */
    /*  reset 기준 bike basis in sensor frame                                  */
    /*                                                                        */
    /*  reset 시점의 프레임을 기준으로                                          */
    /*  - fwd : 차량 전진축                                                    */
    /*  - left: 차량 좌측축                                                    */
    /*  - up  : 차량 상방축                                                    */
    /*  을 sensor frame 안에 고정 벡터로 저장한다.                              */
    /* ---------------------------------------------------------------------- */
    float zero_fwd_x_s;
    float zero_fwd_y_s;
    float zero_fwd_z_s;

    float zero_left_x_s;
    float zero_left_y_s;
    float zero_left_z_s;

    float zero_up_x_s;
    float zero_up_y_s;
    float zero_up_z_s;

    /* ---------------------------------------------------------------------- */
    /*  최근 변환된 IMU sample                                                  */
    /* ---------------------------------------------------------------------- */
    float last_ax_g;
    float last_ay_g;
    float last_az_g;

    /* raw gyro engineering unit before bias subtraction */
    float last_gx_raw_dps;
    float last_gy_raw_dps;
    float last_gz_raw_dps;

    /* bias-corrected gyro engineering unit */
    float last_gx_dps;
    float last_gy_dps;
    float last_gz_dps;

    float last_dt_s;
    float last_accel_norm_mg;
    float last_jerk_mg_per_s;
    float last_attitude_trust_permille;
    float yaw_rate_up_dps;

    /* ---------------------------------------------------------------------- */
    /*  auxiliary heading diagnostic                                            */
    /*                                                                        */
    /*  주의                                                                   */
    /*  - 이 값들은 lean / grade / lateral G 계산에 피드백하지 않는다.         */
    /*  - GNSS heading이 있으면 그 값을 우선 공개하고,                           */
    /*    없을 때만 tilt-compensated magnetic heading을 보조로 공개한다.        */
    /* ---------------------------------------------------------------------- */
    bool     mag_heading_valid;
    float    mag_heading_deg;
    bool     heading_valid;
    uint8_t  heading_source;
    float    heading_deg;

    /* ---------------------------------------------------------------------- */
    /*  user-calibrated gyro bias                                               */
    /* ---------------------------------------------------------------------- */
    float gyro_bias_x_dps;
    float gyro_bias_y_dps;
    float gyro_bias_z_dps;

    /* calibration accumulators */
    float gyro_bias_sum_x_dps;
    float gyro_bias_sum_y_dps;
    float gyro_bias_sum_z_dps;

    /* ---------------------------------------------------------------------- */
    /*  zero / current attitude / display smoothing                             */
    /* ---------------------------------------------------------------------- */
    float bank_raw_deg;
    float grade_raw_deg;
    float bank_display_deg;
    float grade_display_deg;

    float bank_rate_dps;
    float grade_rate_dps;

    /* ---------------------------------------------------------------------- */
    /*  IMU 기반 level-frame acceleration                                       */
    /* ---------------------------------------------------------------------- */
    float lon_imu_g;
    float lat_imu_g;

    /* 외부 reference */
    float lon_ref_g;
    float lat_ref_g;
    float lat_ref_fast_g;
    float lat_ref_slow_g;

    /* GNSS / OBD로 천천히 적응되는 bias */
    float lon_bias_g;
    float lat_bias_g;

    /* 최종 표시 후보 */
    float lon_fused_g;
    float lat_fused_g;

    /* ---------------------------------------------------------------------- */
    /*  GNSS / OBD derivative용 이전 값 캐시                                     */
    /* ---------------------------------------------------------------------- */
    bool     prev_gnss_speed_valid;
    int32_t  prev_gnss_speed_mmps;
    uint32_t prev_gnss_speed_ms;

    bool     prev_gnss_heading_valid;
    float    prev_gnss_heading_deg;
    uint32_t prev_gnss_heading_ms;

    bool     prev_obd_speed_valid;
    uint32_t prev_obd_speed_mmps;
    uint32_t prev_obd_speed_ms;
} bike_runtime_t;

static bike_runtime_t s_bike_runtime;

/* -------------------------------------------------------------------------- */
/*  내부 math helper                                                           */
/* -------------------------------------------------------------------------- */
static float BIKE_DYN_ClampF(float value, float min_value, float max_value)
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

static float BIKE_DYN_Clamp01F(float value)
{
    return BIKE_DYN_ClampF(value, 0.0f, 1.0f);
}

static int32_t BIKE_DYN_ClampS32(int32_t value, int32_t min_value, int32_t max_value)
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

static int32_t BIKE_DYN_AbsS32(int32_t value)
{
    return (value < 0) ? (-value) : value;
}

static float BIKE_DYN_SafeSqrtF(float value)
{
    if (value <= 0.0f)
    {
        return 0.0f;
    }
    return sqrtf(value);
}

static float BIKE_DYN_Dot3(float ax, float ay, float az,
                           float bx, float by, float bz)
{
    return (ax * bx) + (ay * by) + (az * bz);
}

static void BIKE_DYN_Cross3(float ax, float ay, float az,
                            float bx, float by, float bz,
                            float *out_x, float *out_y, float *out_z)
{
    if ((out_x == 0) || (out_y == 0) || (out_z == 0))
    {
        return;
    }

    *out_x = (ay * bz) - (az * by);
    *out_y = (az * bx) - (ax * bz);
    *out_z = (ax * by) - (ay * bx);
}

static bool BIKE_DYN_Normalize3(float *x, float *y, float *z)
{
    float norm;

    if ((x == 0) || (y == 0) || (z == 0))
    {
        return false;
    }

    norm = BIKE_DYN_SafeSqrtF((*x * *x) + (*y * *y) + (*z * *z));
    if (norm < BIKE_DYN_MIN_VEC_NORM)
    {
        return false;
    }

    *x /= norm;
    *y /= norm;
    *z /= norm;
    return true;
}

static float BIKE_DYN_LpfAlphaFromTauMs(uint32_t tau_ms, float dt_s)
{
    float tau_s;

    if (dt_s <= 0.0f)
    {
        return 1.0f;
    }

    if (tau_ms == 0u)
    {
        return 1.0f;
    }

    tau_s = ((float)tau_ms) * 0.001f;
    return BIKE_DYN_Clamp01F(dt_s / (tau_s + dt_s));
}

static float BIKE_DYN_LpfUpdate(float current, float target, uint32_t tau_ms, float dt_s)
{
    float alpha;

    alpha = BIKE_DYN_LpfAlphaFromTauMs(tau_ms, dt_s);
    return current + (alpha * (target - current));
}

static float BIKE_DYN_WrapDeg(float deg)
{
    while (deg > 180.0f)
    {
        deg -= 360.0f;
    }
    while (deg < -180.0f)
    {
        deg += 360.0f;
    }
    return deg;
}

/* -------------------------------------------------------------------------- */
/*  각도를 0..360 deg 범위로 정규화한다.                                       */
/* -------------------------------------------------------------------------- */
static float BIKE_DYN_WrapDeg360(float deg)
{
    while (deg >= 360.0f)
    {
        deg -= 360.0f;
    }
    while (deg < 0.0f)
    {
        deg += 360.0f;
    }
    return deg;
}

static float BIKE_DYN_DeadbandAndClipG(float value_g,
                                       uint16_t deadband_mg,
                                       uint16_t clip_mg)
{
    float abs_value_mg;
    float sign;
    float clip_g;
    float deadband_g;

    sign = (value_g < 0.0f) ? -1.0f : 1.0f;
    abs_value_mg = fabsf(value_g) * 1000.0f;

    deadband_g = ((float)deadband_mg) / 1000.0f;
    clip_g     = ((float)clip_mg) / 1000.0f;

    if (abs_value_mg <= (float)deadband_mg)
    {
        return 0.0f;
    }

    value_g = sign * (fabsf(value_g) - deadband_g);
    if (clip_mg != 0u)
    {
        value_g = BIKE_DYN_ClampF(value_g, -clip_g, clip_g);
    }

    return value_g;
}

static int16_t BIKE_DYN_RoundFloatToS16X10(float value)
{
    long temp;

    if (value >= 0.0f)
    {
        temp = (long)(value + 0.5f);
    }
    else
    {
        temp = (long)(value - 0.5f);
    }

    temp = BIKE_DYN_ClampS32((int32_t)temp, -32768, 32767);
    return (int16_t)temp;
}

static int32_t BIKE_DYN_RoundFloatToS32(float value)
{
    if (value >= 0.0f)
    {
        return (int32_t)(value + 0.5f);
    }
    return (int32_t)(value - 0.5f);
}

static int32_t BIKE_DYN_SpeedAbsMmps(int32_t speed_mmps)
{
    return (speed_mmps >= 0) ? speed_mmps : -speed_mmps;
}

/* -------------------------------------------------------------------------- */
/*  axis enum -> sensor unit vector                                            */
/* -------------------------------------------------------------------------- */
static void BIKE_DYN_AxisEnumToUnitVec(uint8_t axis_enum,
                                       float *out_x,
                                       float *out_y,
                                       float *out_z)
{
    float x = 0.0f;
    float y = 0.0f;
    float z = 0.0f;

    switch ((app_bike_axis_t)axis_enum)
    {
    case APP_BIKE_AXIS_POS_X:
        x = 1.0f;
        break;

    case APP_BIKE_AXIS_NEG_X:
        x = -1.0f;
        break;

    case APP_BIKE_AXIS_POS_Y:
        y = 1.0f;
        break;

    case APP_BIKE_AXIS_NEG_Y:
        y = -1.0f;
        break;

    case APP_BIKE_AXIS_POS_Z:
        z = 1.0f;
        break;

    case APP_BIKE_AXIS_NEG_Z:
        z = -1.0f;
        break;

    default:
        x = 1.0f;
        break;
    }

    if (out_x != 0) { *out_x = x; }
    if (out_y != 0) { *out_y = y; }
    if (out_z != 0) { *out_z = z; }
}

/* -------------------------------------------------------------------------- */
/*  Rodrigues rotation                                                         */
/* -------------------------------------------------------------------------- */
static void BIKE_DYN_RotateVecAroundAxis(float vx, float vy, float vz,
                                         float ax, float ay, float az,
                                         float angle_rad,
                                         float *out_x,
                                         float *out_y,
                                         float *out_z)
{
    float c;
    float s;
    float dot;
    float cross_x;
    float cross_y;
    float cross_z;

    if ((out_x == 0) || (out_y == 0) || (out_z == 0))
    {
        return;
    }

    c = cosf(angle_rad);
    s = sinf(angle_rad);

    dot = BIKE_DYN_Dot3(ax, ay, az, vx, vy, vz);
    BIKE_DYN_Cross3(ax, ay, az, vx, vy, vz, &cross_x, &cross_y, &cross_z);

    *out_x = (vx * c) + (cross_x * s) + (ax * dot * (1.0f - c));
    *out_y = (vy * c) + (cross_y * s) + (ay * dot * (1.0f - c));
    *out_z = (vz * c) + (cross_z * s) + (az * dot * (1.0f - c));
}

/* -------------------------------------------------------------------------- */
/*  IMU scale 읽기                                                              */
/* -------------------------------------------------------------------------- */
static float BIKE_DYN_GetAccelLsbPerG(const app_bike_settings_t *settings)
{
    if ((settings != 0) && (settings->imu_accel_lsb_per_g != 0u))
    {
        return (float)settings->imu_accel_lsb_per_g;
    }
    return BIKE_DYN_DEFAULT_ACCEL_LSB_G;
}

static float BIKE_DYN_GetGyroLsbPerDps(const app_bike_settings_t *settings)
{
    if ((settings != 0) && (settings->imu_gyro_lsb_per_dps_x10 != 0u))
    {
        return ((float)settings->imu_gyro_lsb_per_dps_x10) * 0.1f;
    }
    return BIKE_DYN_DEFAULT_GYRO_LSB_DPS;
}

/* -------------------------------------------------------------------------- */
/*  GNSS / OBD validity helper                                                 */
/* -------------------------------------------------------------------------- */
static bool BIKE_DYN_IsGnssSpeedValid(const gps_fix_basic_t *fix,
                                      const app_bike_settings_t *settings)
{
    float speed_acc_kmh_x10;

    if ((fix == 0) || (settings == 0))
    {
        return false;
    }

    if ((fix->valid == false) || (fix->fixOk == false))
    {
        return false;
    }

    speed_acc_kmh_x10 = ((float)fix->sAcc) * 0.036f;
    if (speed_acc_kmh_x10 > (float)settings->gnss_max_speed_acc_kmh_x10)
    {
        return false;
    }

    return true;
}

static bool BIKE_DYN_IsGnssHeadingValid(const gps_fix_basic_t *fix,
                                        const app_bike_settings_t *settings)
{
    float speed_kmh_x10;
    float head_acc_deg_x10;

    if ((fix == 0) || (settings == 0))
    {
        return false;
    }

    if (BIKE_DYN_IsGnssSpeedValid(fix, settings) == false)
    {
        return false;
    }

    speed_kmh_x10    = ((float)BIKE_DYN_AbsS32(fix->gSpeed)) * 0.036f;
    head_acc_deg_x10 = ((float)fix->headAcc) * 0.0001f;

    if (speed_kmh_x10 < (float)settings->gnss_min_speed_kmh_x10)
    {
        return false;
    }

    if (head_acc_deg_x10 > (float)settings->gnss_max_head_acc_deg_x10)
    {
        return false;
    }

    return true;
}

static bool BIKE_DYN_IsObdSpeedValid(const app_bike_state_t *bike,
                                     const app_bike_settings_t *settings,
                                     uint32_t now_ms)
{
    if ((bike == 0) || (settings == 0))
    {
        return false;
    }

    if (settings->obd_aid_enabled == 0u)
    {
        return false;
    }

    if (bike->obd_input_speed_valid == false)
    {
        return false;
    }

    if (bike->obd_input_last_update_ms == 0u)
    {
        return false;
    }

    if ((uint32_t)(now_ms - bike->obd_input_last_update_ms) > settings->obd_stale_timeout_ms)
    {
        return false;
    }

    return true;
}

/* -------------------------------------------------------------------------- */
/*  speed source selection                                                     */
/* -------------------------------------------------------------------------- */
static uint8_t BIKE_DYN_SelectSpeedSource(const app_bike_state_t *bike,
                                          const gps_fix_basic_t *fix,
                                          const app_bike_settings_t *settings,
                                          uint32_t now_ms)
{
    if (BIKE_DYN_IsObdSpeedValid(bike, settings, now_ms) != false)
    {
        return (uint8_t)APP_BIKE_SPEED_SOURCE_OBD;
    }

    if ((settings != 0) &&
        (settings->gnss_aid_enabled != 0u) &&
        (BIKE_DYN_IsGnssSpeedValid(fix, settings) != false))
    {
        return (uint8_t)APP_BIKE_SPEED_SOURCE_GNSS;
    }

    return (uint8_t)APP_BIKE_SPEED_SOURCE_IMU_FALLBACK;
}

/* -------------------------------------------------------------------------- */
/*  Quaternion helper                                                          */
/* -------------------------------------------------------------------------- */
static bool BIKE_DYN_QuatNormalize(float *q0, float *q1, float *q2, float *q3)
{
    float n;

    if ((q0 == 0) || (q1 == 0) || (q2 == 0) || (q3 == 0))
    {
        return false;
    }

    n = BIKE_DYN_SafeSqrtF((*q0 * *q0) + (*q1 * *q1) + (*q2 * *q2) + (*q3 * *q3));
    if (n < BIKE_DYN_MIN_VEC_NORM)
    {
        return false;
    }

    *q0 /= n;
    *q1 /= n;
    *q2 /= n;
    *q3 /= n;
    return true;
}

static void BIKE_DYN_QuatGetGravityBody(float q0, float q1, float q2, float q3,
                                        float *gx, float *gy, float *gz)
{
    if ((gx == 0) || (gy == 0) || (gz == 0))
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  Mahony / Madgwick 관례와 동일한 "body frame에서 본 world-up" 방향.      */
    /*  정지 상태에서 accelerometer normalized vector와 같은 방향으로 나온다.   */
    /* ---------------------------------------------------------------------- */
    *gx = 2.0f * ((q1 * q3) - (q0 * q2));
    *gy = 2.0f * ((q0 * q1) + (q2 * q3));
    *gz = (q0 * q0) - (q1 * q1) - (q2 * q2) + (q3 * q3);
}

static void BIKE_DYN_QuatSetFromAccelNoYaw(float ax_g, float ay_g, float az_g)
{
    float accel_norm_g;
    float ax_n;
    float ay_n;
    float az_n;
    float roll_rad;
    float pitch_rad;
    float cr;
    float sr;
    float cp;
    float sp;

    accel_norm_g = BIKE_DYN_SafeSqrtF((ax_g * ax_g) + (ay_g * ay_g) + (az_g * az_g));
    if (accel_norm_g < BIKE_DYN_MIN_VEC_NORM)
    {
        s_bike_runtime.q0 = 1.0f;
        s_bike_runtime.q1 = 0.0f;
        s_bike_runtime.q2 = 0.0f;
        s_bike_runtime.q3 = 0.0f;
        s_bike_runtime.q_valid = true;
        return;
    }

    ax_n = ax_g / accel_norm_g;
    ay_n = ay_g / accel_norm_g;
    az_n = az_g / accel_norm_g;

    roll_rad  = atan2f(ay_n, az_n);
    pitch_rad = atan2f(-ax_n, BIKE_DYN_SafeSqrtF((ay_n * ay_n) + (az_n * az_n)));

    cr = cosf(0.5f * roll_rad);
    sr = sinf(0.5f * roll_rad);
    cp = cosf(0.5f * pitch_rad);
    sp = sinf(0.5f * pitch_rad);

    /* yaw = 0으로 둔 body->world quaternion */
    s_bike_runtime.q0 = cr * cp;
    s_bike_runtime.q1 = sr * cp;
    s_bike_runtime.q2 = cr * sp;
    s_bike_runtime.q3 = -sr * sp;

    if (BIKE_DYN_QuatNormalize(&s_bike_runtime.q0,
                               &s_bike_runtime.q1,
                               &s_bike_runtime.q2,
                               &s_bike_runtime.q3) == false)
    {
        s_bike_runtime.q0 = 1.0f;
        s_bike_runtime.q1 = 0.0f;
        s_bike_runtime.q2 = 0.0f;
        s_bike_runtime.q3 = 0.0f;
    }

    s_bike_runtime.q_valid = true;
}

static float BIKE_DYN_CalcMahonyKpRadPerSec(uint16_t gravity_tau_ms)
{
    float tau_s;

    if (gravity_tau_ms == 0u)
    {
        return 4.0f;
    }

    tau_s = ((float)gravity_tau_ms) * 0.001f;
    if (tau_s < 0.010f)
    {
        tau_s = 0.010f;
    }

    /* ---------------------------------------------------------------------- */
    /*  Kp는 attitude error(rad)를 angular correction(rad/s)로 바꾸는 gain이다. */
    /*  tau가 작을수록 더 세게 accel 방향으로 끌어당긴다.                        */
    /* ---------------------------------------------------------------------- */
    return 1.0f / tau_s;
}

static void BIKE_DYN_UpdateGravityVectorFromQuat(void)
{
    BIKE_DYN_QuatGetGravityBody(s_bike_runtime.q0,
                                s_bike_runtime.q1,
                                s_bike_runtime.q2,
                                s_bike_runtime.q3,
                                &s_bike_runtime.gravity_est_x_s,
                                &s_bike_runtime.gravity_est_y_s,
                                &s_bike_runtime.gravity_est_z_s);

    if (BIKE_DYN_Normalize3(&s_bike_runtime.gravity_est_x_s,
                            &s_bike_runtime.gravity_est_y_s,
                            &s_bike_runtime.gravity_est_z_s) == false)
    {
        s_bike_runtime.gravity_est_x_s = 0.0f;
        s_bike_runtime.gravity_est_y_s = 0.0f;
        s_bike_runtime.gravity_est_z_s = 1.0f;
    }

    s_bike_runtime.gravity_valid = true;
}

static void BIKE_DYN_MahonyImuUpdate(float ax_g,
                                     float ay_g,
                                     float az_g,
                                     float gx_dps,
                                     float gy_dps,
                                     float gz_dps,
                                     float trust_01,
                                     uint16_t gravity_tau_ms,
                                     float dt_s)
{
    float accel_norm_g;
    float ax_n;
    float ay_n;
    float az_n;
    float vx;
    float vy;
    float vz;
    float ex;
    float ey;
    float ez;
    float kp;
    float gx_rad_s;
    float gy_rad_s;
    float gz_rad_s;
    float q0;
    float q1;
    float q2;
    float q3;
    float qdot0;
    float qdot1;
    float qdot2;
    float qdot3;

    accel_norm_g = BIKE_DYN_SafeSqrtF((ax_g * ax_g) + (ay_g * ay_g) + (az_g * az_g));
    if (accel_norm_g < BIKE_DYN_MIN_VEC_NORM)
    {
        return;
    }

    ax_n = ax_g / accel_norm_g;
    ay_n = ay_g / accel_norm_g;
    az_n = az_g / accel_norm_g;

    if (s_bike_runtime.q_valid == false)
    {
        BIKE_DYN_QuatSetFromAccelNoYaw(ax_n, ay_n, az_n);
        BIKE_DYN_UpdateGravityVectorFromQuat();
        return;
    }

    q0 = s_bike_runtime.q0;
    q1 = s_bike_runtime.q1;
    q2 = s_bike_runtime.q2;
    q3 = s_bike_runtime.q3;

    BIKE_DYN_QuatGetGravityBody(q0, q1, q2, q3, &vx, &vy, &vz);

    /* ---------------------------------------------------------------------- */
    /*  Mahony accel correction error                                          */
    /*                                                                        */
    /*  measured gravity(ax_n, ay_n, az_n) 와                                  */
    /*  quaternion이 예측한 gravity(vx, vy, vz) 사이의 외적을 사용한다.         */
    /*  신뢰도 trust_01이 낮으면 correction이 거의 0이 된다.                    */
    /* ---------------------------------------------------------------------- */
    ex = (ay_n * vz) - (az_n * vy);
    ey = (az_n * vx) - (ax_n * vz);
    ez = (ax_n * vy) - (ay_n * vx);

    kp = BIKE_DYN_CalcMahonyKpRadPerSec(gravity_tau_ms) * BIKE_DYN_Clamp01F(trust_01);

    gx_rad_s = gx_dps * BIKE_DYN_DEG2RAD;
    gy_rad_s = gy_dps * BIKE_DYN_DEG2RAD;
    gz_rad_s = gz_dps * BIKE_DYN_DEG2RAD;

    gx_rad_s += kp * ex;
    gy_rad_s += kp * ey;
    gz_rad_s += kp * ez;

    qdot0 = 0.5f * ((-q1 * gx_rad_s) - (q2 * gy_rad_s) - (q3 * gz_rad_s));
    qdot1 = 0.5f * (( q0 * gx_rad_s) + (q2 * gz_rad_s) - (q3 * gy_rad_s));
    qdot2 = 0.5f * (( q0 * gy_rad_s) - (q1 * gz_rad_s) + (q3 * gx_rad_s));
    qdot3 = 0.5f * (( q0 * gz_rad_s) + (q1 * gy_rad_s) - (q2 * gx_rad_s));

    s_bike_runtime.q0 = q0 + (qdot0 * dt_s);
    s_bike_runtime.q1 = q1 + (qdot1 * dt_s);
    s_bike_runtime.q2 = q2 + (qdot2 * dt_s);
    s_bike_runtime.q3 = q3 + (qdot3 * dt_s);

    if (BIKE_DYN_QuatNormalize(&s_bike_runtime.q0,
                               &s_bike_runtime.q1,
                               &s_bike_runtime.q2,
                               &s_bike_runtime.q3) == false)
    {
        BIKE_DYN_QuatSetFromAccelNoYaw(ax_n, ay_n, az_n);
    }

    BIKE_DYN_UpdateGravityVectorFromQuat();
}

/* -------------------------------------------------------------------------- */
/*  정지 / 보정 조건 helper                                                    */
/* -------------------------------------------------------------------------- */
static bool BIKE_DYN_IsVehicleNearlyStoppedForService(int32_t selected_speed_mmps,
                                                      uint8_t speed_source)
{
    if (speed_source == (uint8_t)APP_BIKE_SPEED_SOURCE_IMU_FALLBACK)
    {
        return true;
    }

    return (BIKE_DYN_SpeedAbsMmps(selected_speed_mmps) <= BIKE_DYN_GYRO_CAL_MAX_SPEED_MMPS) ? true : false;
}

static bool BIKE_DYN_IsZeroCaptureSafe(const app_bike_settings_t *settings,
                                       int32_t selected_speed_mmps,
                                       uint8_t speed_source)
{
    float accel_err_mg;
    float jerk_limit;
    float gyro_mag_dps;

    if ((settings == 0) || (s_bike_runtime.gravity_valid == false) || (s_bike_runtime.imu_sample_valid == false))
    {
        return false;
    }

    if (BIKE_DYN_IsVehicleNearlyStoppedForService(selected_speed_mmps, speed_source) == false)
    {
        return false;
    }

    accel_err_mg = fabsf(s_bike_runtime.last_accel_norm_mg - BIKE_DYN_ONE_G_MG);
    if (accel_err_mg > (float)BIKE_DYN_ClampS32((int32_t)settings->imu_attitude_accel_gate_mg, 20, 500))
    {
        return false;
    }

    jerk_limit = (float)BIKE_DYN_ClampS32((int32_t)settings->imu_jerk_gate_mg_per_s / 2, 200, 20000);
    if (fabsf(s_bike_runtime.last_jerk_mg_per_s) > jerk_limit)
    {
        return false;
    }

    gyro_mag_dps = BIKE_DYN_SafeSqrtF((s_bike_runtime.last_gx_dps * s_bike_runtime.last_gx_dps) +
                                      (s_bike_runtime.last_gy_dps * s_bike_runtime.last_gy_dps) +
                                      (s_bike_runtime.last_gz_dps * s_bike_runtime.last_gz_dps));
    if (gyro_mag_dps > 8.0f)
    {
        return false;
    }

    return true;
}

static bool BIKE_DYN_IsGyroCalibrationSampleGood(const app_bike_settings_t *settings,
                                                 int32_t selected_speed_mmps,
                                                 uint8_t speed_source)
{
    float accel_err_mg;
    float jerk_limit;
    float gyro_mag_dps;

    if ((settings == 0) || (s_bike_runtime.imu_sample_valid == false))
    {
        return false;
    }

    if (BIKE_DYN_IsVehicleNearlyStoppedForService(selected_speed_mmps, speed_source) == false)
    {
        return false;
    }

    accel_err_mg = fabsf(s_bike_runtime.last_accel_norm_mg - BIKE_DYN_ONE_G_MG);
    if (accel_err_mg > (float)BIKE_DYN_ClampS32((int32_t)settings->imu_attitude_accel_gate_mg, 40, 250))
    {
        return false;
    }

    jerk_limit = (float)BIKE_DYN_ClampS32((int32_t)settings->imu_jerk_gate_mg_per_s / 2, 250, 15000);
    if (fabsf(s_bike_runtime.last_jerk_mg_per_s) > jerk_limit)
    {
        return false;
    }

    gyro_mag_dps = BIKE_DYN_SafeSqrtF((s_bike_runtime.last_gx_raw_dps * s_bike_runtime.last_gx_raw_dps) +
                                      (s_bike_runtime.last_gy_raw_dps * s_bike_runtime.last_gy_raw_dps) +
                                      (s_bike_runtime.last_gz_raw_dps * s_bike_runtime.last_gz_raw_dps));
    if (gyro_mag_dps > BIKE_DYN_GYRO_CAL_MAX_GYRO_DPS)
    {
        return false;
    }

    return true;
}

/* -------------------------------------------------------------------------- */
/*  state publish helper                                                       */
/* -------------------------------------------------------------------------- */
static void BIKE_DYN_PublishState(uint32_t now_ms,
                                  const app_bike_settings_t *settings,
                                  uint8_t speed_source,
                                  bool gnss_speed_valid,
                                  bool gnss_heading_valid,
                                  bool obd_speed_valid,
                                  int32_t selected_speed_mmps)
{
    volatile app_bike_state_t *bike;

    bike = &g_app_state.bike;

    bike->initialized               = s_bike_runtime.initialized;
    bike->zero_valid                = s_bike_runtime.zero_valid;
    bike->imu_valid                 = s_bike_runtime.gravity_valid;
    bike->gnss_aid_valid            = (uint8_t)((gnss_speed_valid || gnss_heading_valid) ? 1u : 0u);
    bike->gnss_heading_valid        = (uint8_t)(gnss_heading_valid ? 1u : 0u);
    bike->obd_speed_valid           = (uint8_t)(obd_speed_valid ? 1u : 0u);
    bike->speed_source              = speed_source;
    bike->estimator_mode            =
        (speed_source == (uint8_t)APP_BIKE_SPEED_SOURCE_OBD)  ? (uint8_t)APP_BIKE_ESTIMATOR_MODE_OBD_AIDED  :
        (speed_source == (uint8_t)APP_BIKE_SPEED_SOURCE_GNSS) ? (uint8_t)APP_BIKE_ESTIMATOR_MODE_GNSS_AIDED :
                                                                 (uint8_t)APP_BIKE_ESTIMATOR_MODE_IMU_ONLY;
    bike->confidence_permille       = (uint16_t)BIKE_DYN_ClampS32(
        (int32_t)(
            (s_bike_runtime.gravity_valid ? (0.65f * s_bike_runtime.last_attitude_trust_permille) : 0.0f) +
            ((speed_source == (uint8_t)APP_BIKE_SPEED_SOURCE_GNSS) ? 180.0f : 0.0f) +
            ((speed_source == (uint8_t)APP_BIKE_SPEED_SOURCE_OBD)  ? 260.0f : 0.0f) +
            (s_bike_runtime.gyro_bias_valid ? 80.0f : 0.0f)
        ),
        0,
        1000);

    bike->last_update_ms            = now_ms;
    bike->last_imu_update_ms        = s_bike_runtime.last_imu_timestamp_ms;
    bike->last_zero_capture_ms      = s_bike_runtime.last_zero_capture_ms;
    bike->last_gnss_aid_ms          = s_bike_runtime.last_gnss_fix_update_ms;

    bike->banking_angle_deg_x10     = BIKE_DYN_RoundFloatToS16X10(s_bike_runtime.bank_display_deg * 10.0f);
    bike->banking_angle_display_deg = BIKE_DYN_RoundFloatToS16X10(s_bike_runtime.bank_display_deg);
    bike->grade_deg_x10             = BIKE_DYN_RoundFloatToS16X10(s_bike_runtime.grade_display_deg * 10.0f);
    bike->grade_display_deg         = BIKE_DYN_RoundFloatToS16X10(s_bike_runtime.grade_display_deg);

    bike->bank_rate_dps_x10         = BIKE_DYN_RoundFloatToS16X10(s_bike_runtime.bank_rate_dps * 10.0f);
    bike->grade_rate_dps_x10        = BIKE_DYN_RoundFloatToS16X10(s_bike_runtime.grade_rate_dps * 10.0f);

    bike->lat_accel_mg              = BIKE_DYN_RoundFloatToS32(s_bike_runtime.lat_fused_g * 1000.0f);
    bike->lon_accel_mg              = BIKE_DYN_RoundFloatToS32(s_bike_runtime.lon_fused_g * 1000.0f);
    bike->lat_accel_cms2            = BIKE_DYN_RoundFloatToS32(s_bike_runtime.lat_fused_g * BIKE_DYN_GRAVITY_CMS2);
    bike->lon_accel_cms2            = BIKE_DYN_RoundFloatToS32(s_bike_runtime.lon_fused_g * BIKE_DYN_GRAVITY_CMS2);

    bike->lat_accel_imu_mg          = BIKE_DYN_RoundFloatToS32(s_bike_runtime.lat_imu_g * 1000.0f);
    bike->lon_accel_imu_mg          = BIKE_DYN_RoundFloatToS32(s_bike_runtime.lon_imu_g * 1000.0f);
    bike->lat_accel_ref_mg          = BIKE_DYN_RoundFloatToS32(s_bike_runtime.lat_ref_g * 1000.0f);
    bike->lon_accel_ref_mg          = BIKE_DYN_RoundFloatToS32(s_bike_runtime.lon_ref_g * 1000.0f);

    bike->lat_bias_mg               = BIKE_DYN_RoundFloatToS32(s_bike_runtime.lat_bias_g * 1000.0f);
    bike->lon_bias_mg               = BIKE_DYN_RoundFloatToS32(s_bike_runtime.lon_bias_g * 1000.0f);

    bike->imu_accel_norm_mg         = BIKE_DYN_RoundFloatToS32(s_bike_runtime.last_accel_norm_mg);
    bike->imu_jerk_mg_per_s         = BIKE_DYN_RoundFloatToS32(s_bike_runtime.last_jerk_mg_per_s);
    bike->imu_attitude_trust_permille = (uint16_t)BIKE_DYN_ClampS32(
        BIKE_DYN_RoundFloatToS32(s_bike_runtime.last_attitude_trust_permille), 0, 1000);

    bike->up_bx_milli               = BIKE_DYN_RoundFloatToS32(
        BIKE_DYN_Dot3(s_bike_runtime.zero_fwd_x_s,  s_bike_runtime.zero_fwd_y_s,  s_bike_runtime.zero_fwd_z_s,
                      s_bike_runtime.gravity_est_x_s, s_bike_runtime.gravity_est_y_s, s_bike_runtime.gravity_est_z_s) * 1000.0f);
    bike->up_by_milli               = BIKE_DYN_RoundFloatToS32(
        BIKE_DYN_Dot3(s_bike_runtime.zero_left_x_s, s_bike_runtime.zero_left_y_s, s_bike_runtime.zero_left_z_s,
                      s_bike_runtime.gravity_est_x_s, s_bike_runtime.gravity_est_y_s, s_bike_runtime.gravity_est_z_s) * 1000.0f);
    bike->up_bz_milli               = BIKE_DYN_RoundFloatToS32(
        BIKE_DYN_Dot3(s_bike_runtime.zero_up_x_s,   s_bike_runtime.zero_up_y_s,   s_bike_runtime.zero_up_z_s,
                      s_bike_runtime.gravity_est_x_s, s_bike_runtime.gravity_est_y_s, s_bike_runtime.gravity_est_z_s) * 1000.0f);

    bike->speed_mmps                = selected_speed_mmps;
    bike->speed_kmh_x10             = (uint16_t)BIKE_DYN_ClampS32(
        BIKE_DYN_RoundFloatToS32(((float)BIKE_DYN_SpeedAbsMmps(selected_speed_mmps)) * 0.036f), 0, 65535);
    bike->gnss_speed_acc_kmh_x10    = (uint16_t)BIKE_DYN_ClampS32(
        BIKE_DYN_RoundFloatToS32(((float)g_app_state.gps.fix.sAcc) * 0.036f), 0, 65535);
    bike->gnss_head_acc_deg_x10     = (uint16_t)BIKE_DYN_ClampS32(
        BIKE_DYN_RoundFloatToS32(((float)g_app_state.gps.fix.headAcc) * 0.0001f), 0, 65535);

    bike->mount_yaw_trim_deg_x10    = (settings != 0) ? settings->mount_yaw_trim_deg_x10 : 0;

    bike->gnss_fix_ok               = (uint8_t)(g_app_state.gps.fix.fixOk ? 1u : 0u);
    bike->gnss_numsv_used           = g_app_state.gps.fix.numSV_used;
    bike->gnss_pdop_x100            = g_app_state.gps.fix.pDOP;

    bike->heading_valid             = s_bike_runtime.heading_valid;
    bike->mag_heading_valid         = s_bike_runtime.mag_heading_valid;
    bike->heading_source            = s_bike_runtime.heading_source;
    bike->reserved_heading0         = 0u;
    bike->heading_deg_x10           = BIKE_DYN_RoundFloatToS16X10(s_bike_runtime.heading_deg * 10.0f);
    bike->mag_heading_deg_x10       = BIKE_DYN_RoundFloatToS16X10(s_bike_runtime.mag_heading_deg * 10.0f);

    bike->gyro_bias_cal_active          = s_bike_runtime.gyro_bias_cal_active;
    bike->gyro_bias_valid               = s_bike_runtime.gyro_bias_valid;
    bike->gyro_bias_cal_last_success    = s_bike_runtime.gyro_bias_cal_last_success;
    bike->gyro_bias_cal_progress_permille =
        (uint16_t)BIKE_DYN_ClampS32(
            BIKE_DYN_RoundFloatToS32(
                ((float)s_bike_runtime.gyro_bias_cal_good_ms * 1000.0f) /
                (float)BIKE_DYN_ClampS32((int32_t)BIKE_DYN_GYRO_CAL_TARGET_GOOD_MS, 1, 60000)),
            0,
            1000);
    bike->last_gyro_bias_cal_ms         = s_bike_runtime.last_gyro_bias_cal_ms;
    bike->gyro_bias_cal_count           = s_bike_runtime.gyro_bias_cal_success_count;
    bike->gyro_bias_x_dps_x100          = BIKE_DYN_RoundFloatToS16X10(s_bike_runtime.gyro_bias_x_dps * 100.0f);
    bike->gyro_bias_y_dps_x100          = BIKE_DYN_RoundFloatToS16X10(s_bike_runtime.gyro_bias_y_dps * 100.0f);
    bike->gyro_bias_z_dps_x100          = BIKE_DYN_RoundFloatToS16X10(s_bike_runtime.gyro_bias_z_dps * 100.0f);
    bike->yaw_rate_dps_x10              = BIKE_DYN_RoundFloatToS16X10(s_bike_runtime.yaw_rate_up_dps * 10.0f);
}

/* -------------------------------------------------------------------------- */
/*  zero / bias 포함 내부 상태 전체 리셋                                        */
/*                                                                            */
/*  중요                                                                       */
/*  - hard rezero는 runtime 자세 상태만 비우고                                 */
/*    사용자가 이미 측정한 gyro bias는 유지해야 한다.                          */
/* -------------------------------------------------------------------------- */
static void BIKE_DYN_ResetRuntime(uint32_t now_ms)
{
    bool  keep_gyro_bias_valid;
    float keep_gyro_bias_x_dps;
    float keep_gyro_bias_y_dps;
    float keep_gyro_bias_z_dps;
    uint32_t keep_gyro_bias_cal_success_count;
    uint32_t keep_last_gyro_bias_cal_ms;
    bool keep_gyro_bias_cal_last_success;

    keep_gyro_bias_valid             = s_bike_runtime.gyro_bias_valid;
    keep_gyro_bias_x_dps             = s_bike_runtime.gyro_bias_x_dps;
    keep_gyro_bias_y_dps             = s_bike_runtime.gyro_bias_y_dps;
    keep_gyro_bias_z_dps             = s_bike_runtime.gyro_bias_z_dps;
    keep_gyro_bias_cal_success_count = s_bike_runtime.gyro_bias_cal_success_count;
    keep_last_gyro_bias_cal_ms       = s_bike_runtime.last_gyro_bias_cal_ms;
    keep_gyro_bias_cal_last_success  = s_bike_runtime.gyro_bias_cal_last_success;

    memset(&s_bike_runtime, 0, sizeof(s_bike_runtime));

    s_bike_runtime.initialized = true;
    s_bike_runtime.init_ms = now_ms;
    s_bike_runtime.last_task_ms = now_ms;

    s_bike_runtime.gyro_bias_valid            = keep_gyro_bias_valid;
    s_bike_runtime.gyro_bias_x_dps            = keep_gyro_bias_x_dps;
    s_bike_runtime.gyro_bias_y_dps            = keep_gyro_bias_y_dps;
    s_bike_runtime.gyro_bias_z_dps            = keep_gyro_bias_z_dps;
    s_bike_runtime.gyro_bias_cal_success_count = keep_gyro_bias_cal_success_count;
    s_bike_runtime.last_gyro_bias_cal_ms      = keep_last_gyro_bias_cal_ms;
    s_bike_runtime.gyro_bias_cal_last_success = keep_gyro_bias_cal_last_success;
}

/* -------------------------------------------------------------------------- */
/*  zero basis 구성                                                             */
/* -------------------------------------------------------------------------- */
static bool BIKE_DYN_RebuildZeroBasis(const app_bike_settings_t *settings)
{
    float up_x;
    float up_y;
    float up_z;

    float fwd_hint_x;
    float fwd_hint_y;
    float fwd_hint_z;

    float left_hint_x;
    float left_hint_y;
    float left_hint_z;

    float fwd_x;
    float fwd_y;
    float fwd_z;

    float left_x;
    float left_y;
    float left_z;

    float projection;

    if ((settings == 0) || (s_bike_runtime.gravity_valid == false))
    {
        return false;
    }

    up_x = s_bike_runtime.gravity_est_x_s;
    up_y = s_bike_runtime.gravity_est_y_s;
    up_z = s_bike_runtime.gravity_est_z_s;

    BIKE_DYN_AxisEnumToUnitVec(settings->mount_forward_axis, &fwd_hint_x, &fwd_hint_y, &fwd_hint_z);
    BIKE_DYN_AxisEnumToUnitVec(settings->mount_left_axis,    &left_hint_x, &left_hint_y, &left_hint_z);

    projection = BIKE_DYN_Dot3(fwd_hint_x, fwd_hint_y, fwd_hint_z, up_x, up_y, up_z);
    fwd_x = fwd_hint_x - (projection * up_x);
    fwd_y = fwd_hint_y - (projection * up_y);
    fwd_z = fwd_hint_z - (projection * up_z);

    if (BIKE_DYN_Normalize3(&fwd_x, &fwd_y, &fwd_z) == false)
    {
        projection = BIKE_DYN_Dot3(left_hint_x, left_hint_y, left_hint_z, up_x, up_y, up_z);
        left_x = left_hint_x - (projection * up_x);
        left_y = left_hint_y - (projection * up_y);
        left_z = left_hint_z - (projection * up_z);

        if (BIKE_DYN_Normalize3(&left_x, &left_y, &left_z) == false)
        {
            return false;
        }

        BIKE_DYN_Cross3(left_x, left_y, left_z, up_x, up_y, up_z, &fwd_x, &fwd_y, &fwd_z);
        if (BIKE_DYN_Normalize3(&fwd_x, &fwd_y, &fwd_z) == false)
        {
            return false;
        }
    }

    BIKE_DYN_Cross3(up_x, up_y, up_z, fwd_x, fwd_y, fwd_z, &left_x, &left_y, &left_z);
    if (BIKE_DYN_Normalize3(&left_x, &left_y, &left_z) == false)
    {
        return false;
    }

    projection = BIKE_DYN_Dot3(left_hint_x, left_hint_y, left_hint_z, up_x, up_y, up_z);
    left_hint_x -= (projection * up_x);
    left_hint_y -= (projection * up_y);
    left_hint_z -= (projection * up_z);

    if (BIKE_DYN_Normalize3(&left_hint_x, &left_hint_y, &left_hint_z) != false)
    {
        if (BIKE_DYN_Dot3(left_x, left_y, left_z, left_hint_x, left_hint_y, left_hint_z) < 0.0f)
        {
            fwd_x  = -fwd_x;
            fwd_y  = -fwd_y;
            fwd_z  = -fwd_z;
            left_x = -left_x;
            left_y = -left_y;
            left_z = -left_z;
        }
    }

    if (settings->mount_yaw_trim_deg_x10 != 0)
    {
        float angle_rad;
        float rot_fwd_x;
        float rot_fwd_y;
        float rot_fwd_z;

        angle_rad = ((float)settings->mount_yaw_trim_deg_x10) * (float)M_PI / 1800.0f;
        BIKE_DYN_RotateVecAroundAxis(fwd_x, fwd_y, fwd_z,
                                     up_x, up_y, up_z,
                                     angle_rad,
                                     &rot_fwd_x, &rot_fwd_y, &rot_fwd_z);

        fwd_x = rot_fwd_x;
        fwd_y = rot_fwd_y;
        fwd_z = rot_fwd_z;

        if (BIKE_DYN_Normalize3(&fwd_x, &fwd_y, &fwd_z) == false)
        {
            return false;
        }

        BIKE_DYN_Cross3(up_x, up_y, up_z, fwd_x, fwd_y, fwd_z, &left_x, &left_y, &left_z);
        if (BIKE_DYN_Normalize3(&left_x, &left_y, &left_z) == false)
        {
            return false;
        }
    }

    BIKE_DYN_Cross3(fwd_x, fwd_y, fwd_z, left_x, left_y, left_z, &up_x, &up_y, &up_z);
    if (BIKE_DYN_Normalize3(&up_x, &up_y, &up_z) == false)
    {
        return false;
    }

    s_bike_runtime.zero_fwd_x_s  = fwd_x;
    s_bike_runtime.zero_fwd_y_s  = fwd_y;
    s_bike_runtime.zero_fwd_z_s  = fwd_z;

    s_bike_runtime.zero_left_x_s = left_x;
    s_bike_runtime.zero_left_y_s = left_y;
    s_bike_runtime.zero_left_z_s = left_z;

    s_bike_runtime.zero_up_x_s   = up_x;
    s_bike_runtime.zero_up_y_s   = up_y;
    s_bike_runtime.zero_up_z_s   = up_z;

    s_bike_runtime.zero_valid           = true;
    s_bike_runtime.last_zero_capture_ms = s_bike_runtime.last_imu_timestamp_ms;
    s_bike_runtime.bank_raw_deg         = 0.0f;
    s_bike_runtime.grade_raw_deg        = 0.0f;
    s_bike_runtime.bank_display_deg     = 0.0f;
    s_bike_runtime.grade_display_deg    = 0.0f;

    s_bike_runtime.lon_bias_g = s_bike_runtime.lon_imu_g;
    s_bike_runtime.lat_bias_g = s_bike_runtime.lat_imu_g;
    s_bike_runtime.lon_fused_g = 0.0f;
    s_bike_runtime.lat_fused_g = 0.0f;

    return true;
}

/* -------------------------------------------------------------------------- */
/*  current gravity와 zero basis로부터                                         */
/*  현재 heading-preserving level frame을 만든다.                              */
/* -------------------------------------------------------------------------- */
static bool BIKE_DYN_BuildCurrentLevelAxes(float *out_fwd_x,
                                           float *out_fwd_y,
                                           float *out_fwd_z,
                                           float *out_left_x,
                                           float *out_left_y,
                                           float *out_left_z)
{
    float up_x;
    float up_y;
    float up_z;
    float proj;
    float fwd_x;
    float fwd_y;
    float fwd_z;
    float left_x;
    float left_y;
    float left_z;

    if ((out_fwd_x == 0) || (out_fwd_y == 0) || (out_fwd_z == 0) ||
        (out_left_x == 0) || (out_left_y == 0) || (out_left_z == 0))
    {
        return false;
    }

    if ((s_bike_runtime.gravity_valid == false) || (s_bike_runtime.zero_valid == false))
    {
        return false;
    }

    up_x = s_bike_runtime.gravity_est_x_s;
    up_y = s_bike_runtime.gravity_est_y_s;
    up_z = s_bike_runtime.gravity_est_z_s;

    proj  = BIKE_DYN_Dot3(s_bike_runtime.zero_fwd_x_s, s_bike_runtime.zero_fwd_y_s, s_bike_runtime.zero_fwd_z_s,
                          up_x, up_y, up_z);

    fwd_x = s_bike_runtime.zero_fwd_x_s - (proj * up_x);
    fwd_y = s_bike_runtime.zero_fwd_y_s - (proj * up_y);
    fwd_z = s_bike_runtime.zero_fwd_z_s - (proj * up_z);

    if (BIKE_DYN_Normalize3(&fwd_x, &fwd_y, &fwd_z) == false)
    {
        proj = BIKE_DYN_Dot3(s_bike_runtime.zero_left_x_s, s_bike_runtime.zero_left_y_s, s_bike_runtime.zero_left_z_s,
                             up_x, up_y, up_z);

        left_x = s_bike_runtime.zero_left_x_s - (proj * up_x);
        left_y = s_bike_runtime.zero_left_y_s - (proj * up_y);
        left_z = s_bike_runtime.zero_left_z_s - (proj * up_z);

        if (BIKE_DYN_Normalize3(&left_x, &left_y, &left_z) == false)
        {
            return false;
        }

        BIKE_DYN_Cross3(left_x, left_y, left_z, up_x, up_y, up_z, &fwd_x, &fwd_y, &fwd_z);
        if (BIKE_DYN_Normalize3(&fwd_x, &fwd_y, &fwd_z) == false)
        {
            return false;
        }
    }

    BIKE_DYN_Cross3(up_x, up_y, up_z, fwd_x, fwd_y, fwd_z, &left_x, &left_y, &left_z);
    if (BIKE_DYN_Normalize3(&left_x, &left_y, &left_z) == false)
    {
        return false;
    }

    *out_fwd_x  = fwd_x;
    *out_fwd_y  = fwd_y;
    *out_fwd_z  = fwd_z;
    *out_left_x = left_x;
    *out_left_y = left_y;
    *out_left_z = left_z;

    return true;
}

/* -------------------------------------------------------------------------- */
/*  raw sample -> 공학 단위 변환                                                */
/*                                                                            */
/*  포인트                                                                     */
/*  - raw gyro -> dps 변환은 "bias 적용 전 raw 값" 과                         */
/*    "사용자 bias를 뺀 corrected 값" 둘 다 필요하다.                          */
/*  - gyro bias calibration은 raw gyro 평균을 bias로 저장하므로                */
/*    raw 값을 따로 받아 둔다.                                                 */
/* -------------------------------------------------------------------------- */
static void BIKE_DYN_ConvertImuRawToEngineering(const app_gy86_mpu_raw_t *mpu,
                                                const app_bike_settings_t *settings,
                                                float *ax_g,
                                                float *ay_g,
                                                float *az_g,
                                                float *gx_dps,
                                                float *gy_dps,
                                                float *gz_dps,
                                                float *gx_raw_dps,
                                                float *gy_raw_dps,
                                                float *gz_raw_dps)
{
    float accel_lsb_per_g;
    float gyro_lsb_per_dps;
    float gx_raw_local;
    float gy_raw_local;
    float gz_raw_local;

    if (mpu == 0)
    {
        return;
    }

    accel_lsb_per_g = BIKE_DYN_GetAccelLsbPerG(settings);
    gyro_lsb_per_dps = BIKE_DYN_GetGyroLsbPerDps(settings);

    if (ax_g != 0) { *ax_g = ((float)mpu->accel_x_raw) / accel_lsb_per_g; }
    if (ay_g != 0) { *ay_g = ((float)mpu->accel_y_raw) / accel_lsb_per_g; }
    if (az_g != 0) { *az_g = ((float)mpu->accel_z_raw) / accel_lsb_per_g; }

    gx_raw_local = ((float)mpu->gyro_x_raw) / gyro_lsb_per_dps;
    gy_raw_local = ((float)mpu->gyro_y_raw) / gyro_lsb_per_dps;
    gz_raw_local = ((float)mpu->gyro_z_raw) / gyro_lsb_per_dps;

    if (gx_raw_dps != 0) { *gx_raw_dps = gx_raw_local; }
    if (gy_raw_dps != 0) { *gy_raw_dps = gy_raw_local; }
    if (gz_raw_dps != 0) { *gz_raw_dps = gz_raw_local; }

    if (gx_dps != 0) { *gx_dps = gx_raw_local - s_bike_runtime.gyro_bias_x_dps; }
    if (gy_dps != 0) { *gy_dps = gy_raw_local - s_bike_runtime.gyro_bias_y_dps; }
    if (gz_dps != 0) { *gz_dps = gz_raw_local - s_bike_runtime.gyro_bias_z_dps; }
}

/* -------------------------------------------------------------------------- */
/*  IMU-only Mahony AHRS core                                                  */
/*                                                                            */
/*  전략                                                                       */
/*  1) gyro로 quaternion을 고속 적분한다.                                      */
/*  2) accelerometer norm / jerk를 보고 trust를 계산한다.                      */
/*  3) trust가 좋은 구간에서만 accel 방향으로 천천히 correction 한다.           */
/*  4) magnetometer는 사용하지 않으므로 yaw absolute는 drift 가능하지만         */
/*     roll/pitch 및 gravity 방향은 안정적으로 유지된다.                       */
/* -------------------------------------------------------------------------- */
static bool BIKE_DYN_UpdateGravityObserver(const app_gy86_mpu_raw_t *mpu,
                                           const app_bike_settings_t *settings,
                                           uint32_t now_ms)
{
    bool new_sample;
    float ax_g;
    float ay_g;
    float az_g;
    float gx_dps;
    float gy_dps;
    float gz_dps;
    float gx_raw_dps;
    float gy_raw_dps;
    float gz_raw_dps;

    float accel_norm_g;
    float accel_norm_mg;
    float dt_s;
    float jerk_mg_per_s;
    float trust_norm;
    float trust_jerk;
    float trust_total;
    uint32_t stale_timeout_ms;

    if ((mpu == 0) || (settings == 0))
    {
        s_bike_runtime.imu_sample_valid = false;
        s_bike_runtime.gravity_valid = false;
        return false;
    }

    stale_timeout_ms = (settings->imu_stale_timeout_ms != 0u) ?
                        settings->imu_stale_timeout_ms :
                        BIKE_DYN_DEFAULT_STALE_MS;

    new_sample = (mpu->sample_count != s_bike_runtime.last_mpu_sample_count) ? true : false;

    if ((mpu->sample_count == 0u) || (mpu->timestamp_ms == 0u))
    {
        s_bike_runtime.imu_sample_valid = false;
        s_bike_runtime.gravity_valid = false;
        s_bike_runtime.last_attitude_trust_permille = 0.0f;
        return false;
    }

    if ((uint32_t)(now_ms - mpu->timestamp_ms) > stale_timeout_ms)
    {
        s_bike_runtime.imu_sample_valid = false;
        s_bike_runtime.gravity_valid = false;
        s_bike_runtime.last_attitude_trust_permille = 0.0f;
        return false;
    }

    BIKE_DYN_ConvertImuRawToEngineering(mpu,
                                        settings,
                                        &ax_g, &ay_g, &az_g,
                                        &gx_dps, &gy_dps, &gz_dps,
                                        &gx_raw_dps, &gy_raw_dps, &gz_raw_dps);

    accel_norm_g  = BIKE_DYN_SafeSqrtF((ax_g * ax_g) + (ay_g * ay_g) + (az_g * az_g));
    accel_norm_mg = accel_norm_g * 1000.0f;

    s_bike_runtime.last_accel_norm_mg = accel_norm_mg;
    s_bike_runtime.imu_sample_valid   = true;

    if (accel_norm_g < BIKE_DYN_MIN_VEC_NORM)
    {
        s_bike_runtime.gravity_valid = false;
        s_bike_runtime.last_attitude_trust_permille = 0.0f;
        return false;
    }

    if (new_sample == false)
    {
        return false;
    }

    if ((s_bike_runtime.last_imu_timestamp_ms != 0u) &&
        (mpu->timestamp_ms > s_bike_runtime.last_imu_timestamp_ms))
    {
        dt_s = ((float)(mpu->timestamp_ms - s_bike_runtime.last_imu_timestamp_ms)) * 0.001f;
    }
    else
    {
        dt_s = BIKE_DYN_MIN_DT_S;
    }

    dt_s = BIKE_DYN_ClampF(dt_s, BIKE_DYN_MIN_DT_S, BIKE_DYN_MAX_DT_S);
    s_bike_runtime.last_dt_s = dt_s;

    if (s_bike_runtime.last_mpu_sample_count != 0u)
    {
        float dax_g;
        float day_g;
        float daz_g;
        float jerk_g_per_s;

        dax_g = ax_g - s_bike_runtime.last_ax_g;
        day_g = ay_g - s_bike_runtime.last_ay_g;
        daz_g = az_g - s_bike_runtime.last_az_g;

        jerk_g_per_s = BIKE_DYN_SafeSqrtF((dax_g * dax_g) + (day_g * day_g) + (daz_g * daz_g)) / dt_s;
        jerk_mg_per_s = jerk_g_per_s * 1000.0f;
    }
    else
    {
        jerk_mg_per_s = 0.0f;
    }

    s_bike_runtime.last_jerk_mg_per_s = jerk_mg_per_s;

    trust_norm = 1.0f - (fabsf(accel_norm_mg - BIKE_DYN_ONE_G_MG) /
                         (float)BIKE_DYN_ClampS32((int32_t)settings->imu_attitude_accel_gate_mg, 1, 5000));

    trust_jerk = 1.0f - (jerk_mg_per_s /
                         (float)BIKE_DYN_ClampS32((int32_t)settings->imu_jerk_gate_mg_per_s, 1, 50000));

    trust_norm = BIKE_DYN_Clamp01F(trust_norm);
    trust_jerk = BIKE_DYN_Clamp01F(trust_jerk);
    trust_total = trust_norm * trust_jerk;

    if ((s_bike_runtime.q_valid == false) || (s_bike_runtime.gravity_valid == false))
    {
        BIKE_DYN_QuatSetFromAccelNoYaw(ax_g, ay_g, az_g);
        BIKE_DYN_UpdateGravityVectorFromQuat();
    }
    else
    {
        BIKE_DYN_MahonyImuUpdate(ax_g,
                                 ay_g,
                                 az_g,
                                 gx_dps,
                                 gy_dps,
                                 gz_dps,
                                 trust_total,
                                 settings->imu_gravity_tau_ms,
                                 dt_s);
    }

    s_bike_runtime.last_ax_g = ax_g;
    s_bike_runtime.last_ay_g = ay_g;
    s_bike_runtime.last_az_g = az_g;

    s_bike_runtime.last_gx_raw_dps = gx_raw_dps;
    s_bike_runtime.last_gy_raw_dps = gy_raw_dps;
    s_bike_runtime.last_gz_raw_dps = gz_raw_dps;

    s_bike_runtime.last_gx_dps = gx_dps;
    s_bike_runtime.last_gy_dps = gy_dps;
    s_bike_runtime.last_gz_dps = gz_dps;

    s_bike_runtime.last_mpu_sample_count = mpu->sample_count;
    s_bike_runtime.last_imu_timestamp_ms = mpu->timestamp_ms;
    s_bike_runtime.last_attitude_trust_permille = 1000.0f * trust_total;

    /* ---------------------------------------------------------------------- */
    /*  yaw_rate_up_dps                                                         */
    /*                                                                        */
    /*  현재 world-up 방향(=gravity_est)에 대한 body angular rate 투영값이다.  */
    /*  +값은 좌회전(CCW) 방향으로 해석한다.                                    */
    /* ---------------------------------------------------------------------- */
    s_bike_runtime.yaw_rate_up_dps =
        BIKE_DYN_Dot3(s_bike_runtime.last_gx_dps,
                      s_bike_runtime.last_gy_dps,
                      s_bike_runtime.last_gz_dps,
                      s_bike_runtime.gravity_est_x_s,
                      s_bike_runtime.gravity_est_y_s,
                      s_bike_runtime.gravity_est_z_s);

    return true;
}

/* -------------------------------------------------------------------------- */
/*  auxiliary heading estimate                                                 */
/*                                                                            */
/*  설계 의도                                                                   */
/*  - GNSS heading이 충분히 신뢰 가능하면 그 값을 heading 출력으로 사용한다.   */
/*  - GNSS heading이 없을 때만, raw magnetometer를 tilt compensation 하여      */
/*    보조 magnetic heading으로 공개한다.                                      */
/*  - 이 값은 "진단/표시용" 이며, Mahony 6축 자세 추정이나 lean 계산에는       */
/*    절대 피드백하지 않는다.                                                  */
/* -------------------------------------------------------------------------- */
static void BIKE_DYN_UpdateAuxHeadingEstimate(bool gnss_heading_valid)
{
    const app_gy86_state_t *imu;
    const gps_fix_basic_t  *fix;
    float level_fwd_x;
    float level_fwd_y;
    float level_fwd_z;
    float level_left_x;
    float level_left_y;
    float level_left_z;
    float up_x;
    float up_y;
    float up_z;
    float mx;
    float my;
    float mz;
    float m_up;
    float mh_x;
    float mh_y;
    float mh_z;
    float mag_heading_deg;

    imu = (const app_gy86_state_t *)&g_app_state.gy86;
    fix = (const gps_fix_basic_t *)&g_app_state.gps.fix;

    s_bike_runtime.mag_heading_valid = false;
    s_bike_runtime.mag_heading_deg   = 0.0f;
    s_bike_runtime.heading_valid     = false;
    s_bike_runtime.heading_source    = (uint8_t)APP_BIKE_HEADING_SOURCE_NONE;
    s_bike_runtime.heading_deg       = 0.0f;

    /* ---------------------------------------------------------------------- */
    /*  1) GNSS heading이 있으면 가장 먼저 공개한다.                            */
    /* ---------------------------------------------------------------------- */
    if (gnss_heading_valid != false)
    {
        s_bike_runtime.heading_valid  = true;
        s_bike_runtime.heading_source = (uint8_t)APP_BIKE_HEADING_SOURCE_GNSS;
        s_bike_runtime.heading_deg    = BIKE_DYN_WrapDeg360(((float)fix->headMot) * 0.00001f);
    }

    /* ---------------------------------------------------------------------- */
    /*  2) tilt-compensated magnetic heading 계산                              */
    /*                                                                        */
    /*  필요 조건                                                               */
    /*  - MAG raw valid                                                        */
    /*  - 현재 gravity 추정 valid                                               */
    /*  - zero basis valid                                                     */
    /*  - 현재 level forward/left 축을 만들 수 있을 것                         */
    /* ---------------------------------------------------------------------- */
    if (((imu->status_flags & APP_GY86_STATUS_MAG_VALID) != 0u) &&
        (s_bike_runtime.gravity_valid != false) &&
        (s_bike_runtime.zero_valid != false) &&
        (BIKE_DYN_BuildCurrentLevelAxes(&level_fwd_x,
                                        &level_fwd_y,
                                        &level_fwd_z,
                                        &level_left_x,
                                        &level_left_y,
                                        &level_left_z) != false))
    {
        up_x = s_bike_runtime.gravity_est_x_s;
        up_y = s_bike_runtime.gravity_est_y_s;
        up_z = s_bike_runtime.gravity_est_z_s;

        mx = (float)imu->mag.mag_x_raw;
        my = (float)imu->mag.mag_y_raw;
        mz = (float)imu->mag.mag_z_raw;

        /* ------------------------------------------------------------------ */
        /*  자계 벡터에서 vertical 성분을 제거해서 horizontal projection만 남긴다.*/
        /*  이것이 tilt compensation의 핵심이다.                               */
        /* ------------------------------------------------------------------ */
        m_up = BIKE_DYN_Dot3(mx, my, mz, up_x, up_y, up_z);
        mh_x = mx - (m_up * up_x);
        mh_y = my - (m_up * up_y);
        mh_z = mz - (m_up * up_z);

        if (BIKE_DYN_Normalize3(&mh_x, &mh_y, &mh_z) != false)
        {
            mag_heading_deg = atan2f(BIKE_DYN_Dot3(mh_x, mh_y, mh_z,
                                                   level_left_x, level_left_y, level_left_z),
                                     BIKE_DYN_Dot3(mh_x, mh_y, mh_z,
                                                   level_fwd_x, level_fwd_y, level_fwd_z)) * BIKE_DYN_RAD2DEG;

            s_bike_runtime.mag_heading_valid = true;
            s_bike_runtime.mag_heading_deg   = BIKE_DYN_WrapDeg360(mag_heading_deg);

            if (s_bike_runtime.heading_valid == false)
            {
                s_bike_runtime.heading_valid  = true;
                s_bike_runtime.heading_source = (uint8_t)APP_BIKE_HEADING_SOURCE_MAG;
                s_bike_runtime.heading_deg    = s_bike_runtime.mag_heading_deg;
            }
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  current gravity / zero basis / accel sample 로부터                         */
/*  bank, grade, level-frame accel을 계산한다.                                 */
/* -------------------------------------------------------------------------- */
static void BIKE_DYN_UpdateOutputsFromCurrentImu(const app_bike_settings_t *settings)
{
    float dt_s;
    float up_bx;
    float up_by;
    float up_bz;
    float bank_rad;
    float grade_rad;

    float level_fwd_x;
    float level_fwd_y;
    float level_fwd_z;
    float level_left_x;
    float level_left_y;
    float level_left_z;

    float dyn_x_s;
    float dyn_y_s;
    float dyn_z_s;
    float lon_raw_g;
    float lat_raw_g;

    if ((settings == 0) ||
        (s_bike_runtime.gravity_valid == false) ||
        (s_bike_runtime.zero_valid == false))
    {
        return;
    }

    dt_s = BIKE_DYN_ClampF(s_bike_runtime.last_dt_s, BIKE_DYN_MIN_DT_S, BIKE_DYN_MAX_DT_S);

    up_bx = BIKE_DYN_Dot3(s_bike_runtime.zero_fwd_x_s,  s_bike_runtime.zero_fwd_y_s,  s_bike_runtime.zero_fwd_z_s,
                          s_bike_runtime.gravity_est_x_s, s_bike_runtime.gravity_est_y_s, s_bike_runtime.gravity_est_z_s);

    up_by = BIKE_DYN_Dot3(s_bike_runtime.zero_left_x_s, s_bike_runtime.zero_left_y_s, s_bike_runtime.zero_left_z_s,
                          s_bike_runtime.gravity_est_x_s, s_bike_runtime.gravity_est_y_s, s_bike_runtime.gravity_est_z_s);

    up_bz = BIKE_DYN_Dot3(s_bike_runtime.zero_up_x_s,   s_bike_runtime.zero_up_y_s,   s_bike_runtime.zero_up_z_s,
                          s_bike_runtime.gravity_est_x_s, s_bike_runtime.gravity_est_y_s, s_bike_runtime.gravity_est_z_s);

    bank_rad  = atan2f(-up_by, BIKE_DYN_ClampF(up_bz, -1.0f, 1.0f));
    grade_rad = atan2f(-up_bx, BIKE_DYN_SafeSqrtF((up_by * up_by) + (up_bz * up_bz)));

    s_bike_runtime.bank_raw_deg  = bank_rad  * BIKE_DYN_RAD2DEG;
    s_bike_runtime.grade_raw_deg = grade_rad * BIKE_DYN_RAD2DEG;

    s_bike_runtime.bank_rate_dps  = -BIKE_DYN_Dot3(s_bike_runtime.last_gx_dps,
                                                   s_bike_runtime.last_gy_dps,
                                                   s_bike_runtime.last_gz_dps,
                                                   s_bike_runtime.zero_fwd_x_s,
                                                   s_bike_runtime.zero_fwd_y_s,
                                                   s_bike_runtime.zero_fwd_z_s);

    s_bike_runtime.grade_rate_dps = -BIKE_DYN_Dot3(s_bike_runtime.last_gx_dps,
                                                   s_bike_runtime.last_gy_dps,
                                                   s_bike_runtime.last_gz_dps,
                                                   s_bike_runtime.zero_left_x_s,
                                                   s_bike_runtime.zero_left_y_s,
                                                   s_bike_runtime.zero_left_z_s);

    s_bike_runtime.bank_display_deg = BIKE_DYN_LpfUpdate(s_bike_runtime.bank_display_deg,
                                                         s_bike_runtime.bank_raw_deg,
                                                         settings->lean_display_tau_ms,
                                                         dt_s);

    s_bike_runtime.grade_display_deg = BIKE_DYN_LpfUpdate(s_bike_runtime.grade_display_deg,
                                                          s_bike_runtime.grade_raw_deg,
                                                          settings->grade_display_tau_ms,
                                                          dt_s);

    dyn_x_s = s_bike_runtime.last_ax_g - s_bike_runtime.gravity_est_x_s;
    dyn_y_s = s_bike_runtime.last_ay_g - s_bike_runtime.gravity_est_y_s;
    dyn_z_s = s_bike_runtime.last_az_g - s_bike_runtime.gravity_est_z_s;

    if (BIKE_DYN_BuildCurrentLevelAxes(&level_fwd_x,
                                       &level_fwd_y,
                                       &level_fwd_z,
                                       &level_left_x,
                                       &level_left_y,
                                       &level_left_z) != false)
    {
        lon_raw_g = BIKE_DYN_Dot3(dyn_x_s, dyn_y_s, dyn_z_s,
                                  level_fwd_x, level_fwd_y, level_fwd_z);

        lat_raw_g = BIKE_DYN_Dot3(dyn_x_s, dyn_y_s, dyn_z_s,
                                  level_left_x, level_left_y, level_left_z);
    }
    else
    {
        lon_raw_g = 0.0f;
        lat_raw_g = 0.0f;
    }

    s_bike_runtime.lon_imu_g = BIKE_DYN_LpfUpdate(s_bike_runtime.lon_imu_g,
                                                  lon_raw_g,
                                                  settings->imu_linear_tau_ms,
                                                  dt_s);
    s_bike_runtime.lat_imu_g = BIKE_DYN_LpfUpdate(s_bike_runtime.lat_imu_g,
                                                  lat_raw_g,
                                                  settings->imu_linear_tau_ms,
                                                  dt_s);
}

/* -------------------------------------------------------------------------- */
/*  GNSS / OBD reference update                                                */
/*                                                                            */
/*  철학                                                                       */
/*  - longitudinal ref는 선택된 speed source의 시간차분에서 얻는다.            */
/*  - lateral ref는                                                           */
/*      1) fast model : v * yaw_rate_up / g                                   */
/*      2) slow model : GNSS heading derivative                               */
/*    두 개를 섞어서 만든다.                                                  */
/*  - yaw absolute가 없어도 yaw rate 자체는 gyro로 바로 얻을 수 있으므로       */
/*    fast model은 GPS heading이 없어도 동작한다.                              */
/* -------------------------------------------------------------------------- */
static void BIKE_DYN_UpdateExternalReferences(const app_bike_settings_t *settings,
                                              uint8_t speed_source,
                                              bool gnss_speed_valid,
                                              bool gnss_heading_valid,
                                              bool obd_speed_valid,
                                              int32_t selected_speed_mmps)
{
    const gps_fix_basic_t *fix;
    const app_bike_state_t *bike;
    float dt_s;
    float current_speed_mps;
    float speed_derivative_g;
    float heading_deg;
    float heading_delta_deg;
    float course_rate_rad_s;
    float yaw_rate_up_rad_s;
    uint32_t current_obd_ms;
    uint32_t current_gnss_ms;

    if (settings == 0)
    {
        return;
    }

    fix  = (const gps_fix_basic_t *)&g_app_state.gps.fix;
    bike = (const app_bike_state_t *)&g_app_state.bike;

    current_gnss_ms = fix->last_update_ms;
    current_obd_ms  = bike->obd_input_last_update_ms;

    if ((speed_source == (uint8_t)APP_BIKE_SPEED_SOURCE_OBD) &&
        (obd_speed_valid != false) &&
        (current_obd_ms != 0u) &&
        (current_obd_ms != s_bike_runtime.prev_obd_speed_ms))
    {
        if ((s_bike_runtime.prev_obd_speed_valid != false) &&
            (current_obd_ms > s_bike_runtime.prev_obd_speed_ms))
        {
            dt_s = ((float)(current_obd_ms - s_bike_runtime.prev_obd_speed_ms)) * 0.001f;
            dt_s = BIKE_DYN_ClampF(dt_s, BIKE_DYN_MIN_DT_S, BIKE_DYN_MAX_DT_S);

            speed_derivative_g =
                (((float)bike->obd_input_speed_mmps - (float)s_bike_runtime.prev_obd_speed_mmps) / dt_s) /
                BIKE_DYN_GRAVITY_MMPS2;

            s_bike_runtime.lon_ref_g = speed_derivative_g;
        }

        s_bike_runtime.prev_obd_speed_valid = true;
        s_bike_runtime.prev_obd_speed_mmps  = bike->obd_input_speed_mmps;
        s_bike_runtime.prev_obd_speed_ms    = current_obd_ms;
    }
    else if ((speed_source == (uint8_t)APP_BIKE_SPEED_SOURCE_GNSS) &&
             (gnss_speed_valid != false) &&
             (current_gnss_ms != 0u) &&
             (current_gnss_ms != s_bike_runtime.prev_gnss_speed_ms))
    {
        if ((s_bike_runtime.prev_gnss_speed_valid != false) &&
            (current_gnss_ms > s_bike_runtime.prev_gnss_speed_ms))
        {
            dt_s = ((float)(current_gnss_ms - s_bike_runtime.prev_gnss_speed_ms)) * 0.001f;
            dt_s = BIKE_DYN_ClampF(dt_s, BIKE_DYN_MIN_DT_S, BIKE_DYN_MAX_DT_S);

            speed_derivative_g =
                (((float)fix->gSpeed - (float)s_bike_runtime.prev_gnss_speed_mmps) / dt_s) /
                BIKE_DYN_GRAVITY_MMPS2;

            s_bike_runtime.lon_ref_g = speed_derivative_g;
        }

        s_bike_runtime.prev_gnss_speed_valid = true;
        s_bike_runtime.prev_gnss_speed_mmps  = fix->gSpeed;
        s_bike_runtime.prev_gnss_speed_ms    = current_gnss_ms;
    }

    current_speed_mps = ((float)BIKE_DYN_SpeedAbsMmps(selected_speed_mmps)) * 0.001f;
    yaw_rate_up_rad_s = s_bike_runtime.yaw_rate_up_dps * BIKE_DYN_DEG2RAD;

    if ((speed_source != (uint8_t)APP_BIKE_SPEED_SOURCE_IMU_FALLBACK) &&
        (current_speed_mps > 0.5f))
    {
        s_bike_runtime.lat_ref_fast_g = (current_speed_mps * yaw_rate_up_rad_s) / BIKE_DYN_GRAVITY_MPS2;
    }
    else
    {
        s_bike_runtime.lat_ref_fast_g = 0.0f;
    }

    if ((gnss_heading_valid != false) &&
        (current_gnss_ms != 0u) &&
        (current_gnss_ms != s_bike_runtime.prev_gnss_heading_ms))
    {
        heading_deg = ((float)fix->headMot) * 0.00001f;

        if ((s_bike_runtime.prev_gnss_heading_valid != false) &&
            (current_gnss_ms > s_bike_runtime.prev_gnss_heading_ms))
        {
            dt_s = ((float)(current_gnss_ms - s_bike_runtime.prev_gnss_heading_ms)) * 0.001f;
            dt_s = BIKE_DYN_ClampF(dt_s, BIKE_DYN_MIN_DT_S, BIKE_DYN_MAX_DT_S);

            heading_delta_deg = BIKE_DYN_WrapDeg(heading_deg - s_bike_runtime.prev_gnss_heading_deg);
            course_rate_rad_s = (heading_delta_deg * BIKE_DYN_DEG2RAD) / dt_s;

            /* GNSS headMot는 시계방향 증가이므로 좌회전 + sign에 맞추기 위해 음수 */
            s_bike_runtime.lat_ref_slow_g = -(current_speed_mps * course_rate_rad_s) / BIKE_DYN_GRAVITY_MPS2;
        }

        s_bike_runtime.prev_gnss_heading_valid = true;
        s_bike_runtime.prev_gnss_heading_deg   = heading_deg;
        s_bike_runtime.prev_gnss_heading_ms    = current_gnss_ms;
    }

    if (gnss_heading_valid != false)
    {
        /* ------------------------------------------------------------------ */
        /*  fast = gyro yaw-rate * speed                                       */
        /*  slow = GNSS heading derivative                                      */
        /*  GNSS가 있을 때는 slow를 20% 정도만 섞어서 drift를 잡고               */
        /*  고주파 응답은 fast path가 유지한다.                                  */
        /* ------------------------------------------------------------------ */
        s_bike_runtime.lat_ref_g = (0.80f * s_bike_runtime.lat_ref_fast_g) +
                                   (0.20f * s_bike_runtime.lat_ref_slow_g);
    }
    else
    {
        s_bike_runtime.lat_ref_g = s_bike_runtime.lat_ref_fast_g;
    }

    if (current_gnss_ms != 0u)
    {
        s_bike_runtime.last_gnss_fix_update_ms = current_gnss_ms;
    }
}

/* -------------------------------------------------------------------------- */
/*  bias adaptation                                                            */
/* -------------------------------------------------------------------------- */
static void BIKE_DYN_UpdateBiasAndFusedOutputs(const app_bike_settings_t *settings,
                                               uint8_t speed_source,
                                               bool gnss_speed_valid,
                                               bool gnss_heading_valid,
                                               bool obd_speed_valid,
                                               int32_t selected_speed_mmps)
{
    float dt_s;
    float lon_err_g;
    float lat_err_g;
    float outlier_gate_g;
    bool  allow_lon_ref;
    bool  allow_lat_ref;

    if (settings == 0)
    {
        return;
    }

    dt_s = BIKE_DYN_ClampF(s_bike_runtime.last_dt_s, BIKE_DYN_MIN_DT_S, BIKE_DYN_MAX_DT_S);
    outlier_gate_g = ((float)settings->gnss_outlier_gate_mg) / 1000.0f;

    allow_lon_ref = (obd_speed_valid || gnss_speed_valid) ? true : false;
    allow_lat_ref = ((speed_source != (uint8_t)APP_BIKE_SPEED_SOURCE_IMU_FALLBACK) &&
                     (BIKE_DYN_SpeedAbsMmps(selected_speed_mmps) >= 1500) &&
                     (gnss_heading_valid || s_bike_runtime.gyro_bias_valid)) ? true : false;

    if ((s_bike_runtime.last_attitude_trust_permille >= (float)settings->imu_predict_min_trust_permille) &&
        (allow_lon_ref != false))
    {
        lon_err_g = s_bike_runtime.lon_imu_g - s_bike_runtime.lon_ref_g;
        if (fabsf(lon_err_g) <= outlier_gate_g)
        {
            s_bike_runtime.lon_bias_g = BIKE_DYN_LpfUpdate(s_bike_runtime.lon_bias_g,
                                                           lon_err_g,
                                                           settings->gnss_bias_tau_ms,
                                                           dt_s);
        }
    }

    if ((s_bike_runtime.last_attitude_trust_permille >= (float)settings->imu_predict_min_trust_permille) &&
        (allow_lat_ref != false))
    {
        lat_err_g = s_bike_runtime.lat_imu_g - s_bike_runtime.lat_ref_g;
        if (fabsf(lat_err_g) <= outlier_gate_g)
        {
            s_bike_runtime.lat_bias_g = BIKE_DYN_LpfUpdate(s_bike_runtime.lat_bias_g,
                                                           lat_err_g,
                                                           settings->gnss_bias_tau_ms,
                                                           dt_s);
        }
    }

    s_bike_runtime.lon_fused_g =
        BIKE_DYN_LpfUpdate(s_bike_runtime.lon_fused_g,
                           BIKE_DYN_DeadbandAndClipG(s_bike_runtime.lon_imu_g - s_bike_runtime.lon_bias_g,
                                                    settings->output_deadband_mg,
                                                    settings->output_clip_mg),
                           settings->accel_display_tau_ms,
                           dt_s);

    s_bike_runtime.lat_fused_g =
        BIKE_DYN_LpfUpdate(s_bike_runtime.lat_fused_g,
                           BIKE_DYN_DeadbandAndClipG(s_bike_runtime.lat_imu_g - s_bike_runtime.lat_bias_g,
                                                    settings->output_deadband_mg,
                                                    settings->output_clip_mg),
                           settings->accel_display_tau_ms,
                           dt_s);
}

/* -------------------------------------------------------------------------- */
/*  gyro bias calibration state machine                                        */
/*                                                                            */
/*  요구사항                                                                   */
/*  - API 하나(GyroBiasCorrection)로 요청 가능해야 한다.                       */
/*  - 실제 평균 계산은 BIKE_DYNAMICS_Task 안에서 안전하게 수행한다.            */
/*  - 사용자가 바이크를 흔들면 "좋은 샘플 누적 시간" 을 다시 0으로 돌려서       */
/*    contaminated average를 막는다.                                           */
/* -------------------------------------------------------------------------- */
static void BIKE_DYN_ProcessGyroBiasCalibration(const app_bike_settings_t *settings,
                                                int32_t selected_speed_mmps,
                                                uint8_t speed_source,
                                                bool imu_new_sample,
                                                uint32_t now_ms)
{
    if (settings == 0)
    {
        return;
    }

    if ((s_bike_runtime.gyro_bias_cal_requested != false) &&
        (s_bike_runtime.gyro_bias_cal_active == false))
    {
        s_bike_runtime.gyro_bias_cal_requested      = false;
        s_bike_runtime.gyro_bias_cal_active         = true;
        s_bike_runtime.gyro_bias_cal_last_success   = false;
        s_bike_runtime.gyro_bias_cal_start_ms       = now_ms;
        s_bike_runtime.gyro_bias_cal_settle_until_ms = now_ms + BIKE_DYN_GYRO_CAL_SETTLE_MS;
        s_bike_runtime.gyro_bias_cal_good_ms        = 0u;
        s_bike_runtime.gyro_bias_cal_sample_count   = 0u;
        s_bike_runtime.gyro_bias_sum_x_dps          = 0.0f;
        s_bike_runtime.gyro_bias_sum_y_dps          = 0.0f;
        s_bike_runtime.gyro_bias_sum_z_dps          = 0.0f;
    }

    if (s_bike_runtime.gyro_bias_cal_active == false)
    {
        return;
    }

    if ((uint32_t)(now_ms - s_bike_runtime.gyro_bias_cal_start_ms) > BIKE_DYN_GYRO_CAL_TIMEOUT_MS)
    {
        s_bike_runtime.gyro_bias_cal_active       = false;
        s_bike_runtime.gyro_bias_cal_last_success = false;
        s_bike_runtime.last_gyro_bias_cal_ms      = now_ms;
        return;
    }

    if (imu_new_sample == false)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  버튼 long press 직후에는 손떨림 / 케이스 접촉 진동이 섞일 수 있다.      */
    /*  그래서 짧은 settle 구간 동안은 sample 품질 평가 자체를 시작하지 않는다. */
    /* ---------------------------------------------------------------------- */
    if ((int32_t)(s_bike_runtime.gyro_bias_cal_settle_until_ms - now_ms) > 0)
    {
        return;
    }

    if (BIKE_DYN_IsGyroCalibrationSampleGood(settings, selected_speed_mmps, speed_source) != false)
    {
        uint32_t dt_ms;

        dt_ms = (uint32_t)BIKE_DYN_ClampS32((int32_t)BIKE_DYN_RoundFloatToS32(s_bike_runtime.last_dt_s * 1000.0f), 1, 100);
        s_bike_runtime.gyro_bias_cal_good_ms += dt_ms;
        s_bike_runtime.gyro_bias_cal_sample_count++;

        s_bike_runtime.gyro_bias_sum_x_dps += s_bike_runtime.last_gx_raw_dps;
        s_bike_runtime.gyro_bias_sum_y_dps += s_bike_runtime.last_gy_raw_dps;
        s_bike_runtime.gyro_bias_sum_z_dps += s_bike_runtime.last_gz_raw_dps;
    }
    else
    {
        /* ------------------------------------------------------------------ */
        /*  예전 구현은 bad sample이 한 번만 들어와도 누적 평균을 0으로 리셋했다.*/
        /*  실제 대시보드 마운트에서는 버튼을 누른 뒤 아주 작은 떨림이 흔해서   */
        /*  이 정책이 CALIB FAIL을 과도하게 유발했다.                           */
        /*                                                                      */
        /*  새 정책                                                               */
        /*  - bad sample은 "무시" 하고, 이미 모은 stationary 평균은 유지한다.    */
        /*  - 즉, contaminated sample을 더하지 않기만 하면 된다.                 */
        /*  - timeout 안에 good sample을 충분히 모으면 성공한다.                 */
        /* ------------------------------------------------------------------ */
    }

    if ((s_bike_runtime.gyro_bias_cal_good_ms >= BIKE_DYN_GYRO_CAL_TARGET_GOOD_MS) &&
        (s_bike_runtime.gyro_bias_cal_sample_count >= BIKE_DYN_GYRO_CAL_MIN_SAMPLES))
    {
        float inv_n;

        inv_n = 1.0f / (float)s_bike_runtime.gyro_bias_cal_sample_count;

        s_bike_runtime.gyro_bias_x_dps = s_bike_runtime.gyro_bias_sum_x_dps * inv_n;
        s_bike_runtime.gyro_bias_y_dps = s_bike_runtime.gyro_bias_sum_y_dps * inv_n;
        s_bike_runtime.gyro_bias_z_dps = s_bike_runtime.gyro_bias_sum_z_dps * inv_n;

        s_bike_runtime.gyro_bias_valid             = true;
        s_bike_runtime.gyro_bias_cal_active        = false;
        s_bike_runtime.gyro_bias_cal_last_success  = true;
        s_bike_runtime.gyro_bias_cal_success_count++;
        s_bike_runtime.last_gyro_bias_cal_ms       = now_ms;

        /* ------------------------------------------------------------------ */
        /*  bias가 바뀌었으므로 quaternion을 다음 샘플에서 accel로 다시 세우게 한다. */
        /*  zero basis는 sensor frame 기준으로 저장되어 있으므로 유지 가능하다.   */
        /* ------------------------------------------------------------------ */
        s_bike_runtime.q_valid       = false;
        s_bike_runtime.gravity_valid = false;
    }
}

/* -------------------------------------------------------------------------- */
/*  public API                                                                  */
/* -------------------------------------------------------------------------- */
void BIKE_DYNAMICS_Init(uint32_t now_ms)
{
    BIKE_DYN_ResetRuntime(now_ms);

    g_app_state.bike.initialized = true;
    g_app_state.bike.last_update_ms = now_ms;
    g_app_state.bike.last_imu_update_ms = 0u;
    g_app_state.bike.last_zero_capture_ms = 0u;
    g_app_state.bike.last_gnss_aid_ms = 0u;
}

void BIKE_DYNAMICS_RequestZeroCapture(void)
{
    s_bike_runtime.zero_requested = true;
    g_app_state.bike.zero_request_count++;
}

void BIKE_DYNAMICS_RequestHardRezero(void)
{
    s_bike_runtime.hard_rezero_requested = true;
    g_app_state.bike.hard_rezero_count++;
}

void BIKE_DYNAMICS_RequestGyroBiasCalibration(void)
{
    s_bike_runtime.gyro_bias_cal_requested = true;
}

void ResetBankingAngleSensor(void)
{
    BIKE_DYNAMICS_RequestZeroCapture();
}

void GyroBiasCorrection(void)
{
    BIKE_DYNAMICS_RequestGyroBiasCalibration();
}

void BIKE_DYNAMICS_Task(uint32_t now_ms)
{
    const app_bike_settings_t *settings;
    const app_gy86_mpu_raw_t  *mpu;
    const gps_fix_basic_t     *fix;
    const app_bike_state_t    *bike_state;

    bool gnss_speed_valid;
    bool gnss_heading_valid;
    bool obd_speed_valid;
    bool imu_new_sample;
    uint8_t speed_source;
    int32_t selected_speed_mmps;

    if (s_bike_runtime.initialized == false)
    {
        BIKE_DYNAMICS_Init(now_ms);
    }

    settings   = (const app_bike_settings_t *)&g_app_state.settings.bike;
    mpu        = (const app_gy86_mpu_raw_t *)&g_app_state.gy86.mpu;
    fix        = (const gps_fix_basic_t *)&g_app_state.gps.fix;
    bike_state = (const app_bike_state_t *)&g_app_state.bike;

    if (settings->enabled == 0u)
    {
        g_app_state.bike.initialized = true;
        g_app_state.bike.imu_valid = false;
        g_app_state.bike.zero_valid = false;
        g_app_state.bike.heading_valid = false;
        g_app_state.bike.mag_heading_valid = false;
        g_app_state.bike.heading_source = (uint8_t)APP_BIKE_HEADING_SOURCE_NONE;
        g_app_state.bike.heading_deg_x10 = 0;
        g_app_state.bike.mag_heading_deg_x10 = 0;
        g_app_state.bike.last_update_ms = now_ms;
        return;
    }

    if (s_bike_runtime.hard_rezero_requested != false)
    {
        BIKE_DYN_ResetRuntime(now_ms);
        s_bike_runtime.hard_rezero_requested = false;
    }

    imu_new_sample = BIKE_DYN_UpdateGravityObserver(mpu, settings, now_ms);

    gnss_speed_valid   = (settings->gnss_aid_enabled != 0u) ?
                         BIKE_DYN_IsGnssSpeedValid(fix, settings) : false;
    gnss_heading_valid = (settings->gnss_aid_enabled != 0u) ?
                         BIKE_DYN_IsGnssHeadingValid(fix, settings) : false;
    obd_speed_valid    = BIKE_DYN_IsObdSpeedValid(bike_state, settings, now_ms);

    speed_source = BIKE_DYN_SelectSpeedSource(bike_state, fix, settings, now_ms);

    if (speed_source == (uint8_t)APP_BIKE_SPEED_SOURCE_OBD)
    {
        selected_speed_mmps = (int32_t)bike_state->obd_input_speed_mmps;
    }
    else if (speed_source == (uint8_t)APP_BIKE_SPEED_SOURCE_GNSS)
    {
        selected_speed_mmps = fix->gSpeed;
    }
    else
    {
        selected_speed_mmps = 0;
    }

    BIKE_DYN_ProcessGyroBiasCalibration(settings,
                                        selected_speed_mmps,
                                        speed_source,
                                        imu_new_sample,
                                        now_ms);

    if ((settings->auto_zero_on_boot != 0u) &&
        (s_bike_runtime.auto_zero_done == false) &&
        (s_bike_runtime.gyro_bias_cal_active == false) &&
        (s_bike_runtime.gravity_valid != false) &&
        (imu_new_sample != false) &&
        (BIKE_DYN_IsZeroCaptureSafe(settings, selected_speed_mmps, speed_source) != false))
    {
        BIKE_DYN_UpdateOutputsFromCurrentImu(settings);
        if (BIKE_DYN_RebuildZeroBasis(settings) != false)
        {
            s_bike_runtime.auto_zero_done = true;
            s_bike_runtime.zero_requested = false;
        }
    }

    if ((s_bike_runtime.zero_requested != false) &&
        (s_bike_runtime.gyro_bias_cal_active == false) &&
        (s_bike_runtime.gravity_valid != false) &&
        (imu_new_sample != false) &&
        (BIKE_DYN_IsZeroCaptureSafe(settings, selected_speed_mmps, speed_source) != false))
    {
        BIKE_DYN_UpdateOutputsFromCurrentImu(settings);

        if (BIKE_DYN_RebuildZeroBasis(settings) != false)
        {
            s_bike_runtime.zero_requested = false;
        }
    }

    if ((s_bike_runtime.gravity_valid != false) &&
        (s_bike_runtime.zero_valid != false))
    {
        BIKE_DYN_UpdateExternalReferences(settings,
                                          speed_source,
                                          gnss_speed_valid,
                                          gnss_heading_valid,
                                          obd_speed_valid,
                                          selected_speed_mmps);

        if (imu_new_sample != false)
        {
            BIKE_DYN_UpdateOutputsFromCurrentImu(settings);
            BIKE_DYN_UpdateBiasAndFusedOutputs(settings,
                                               speed_source,
                                               gnss_speed_valid,
                                               gnss_heading_valid,
                                               obd_speed_valid,
                                               selected_speed_mmps);
        }
    }
    else
    {
        s_bike_runtime.bank_raw_deg      = 0.0f;
        s_bike_runtime.grade_raw_deg     = 0.0f;
        s_bike_runtime.bank_display_deg  = 0.0f;
        s_bike_runtime.grade_display_deg = 0.0f;
        s_bike_runtime.lon_imu_g         = 0.0f;
        s_bike_runtime.lat_imu_g         = 0.0f;
        s_bike_runtime.lon_fused_g       = 0.0f;
        s_bike_runtime.lat_fused_g       = 0.0f;
    }

    BIKE_DYN_UpdateAuxHeadingEstimate(gnss_heading_valid);

    BIKE_DYN_PublishState(now_ms,
                          settings,
                          speed_source,
                          gnss_speed_valid,
                          gnss_heading_valid,
                          obd_speed_valid,
                          selected_speed_mmps);

    s_bike_runtime.last_task_ms = s_bike_runtime.last_imu_timestamp_ms;
}
