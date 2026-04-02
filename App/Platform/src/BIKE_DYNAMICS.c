#include "BIKE_DYNAMICS.h"

#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* -------------------------------------------------------------------------- */
/*  BIKE_DYNAMICS                                                              */
/*                                                                            */
/*  구현 철학                                                                  */
/*                                                                            */
/*  1) 이 파일은 APP_STATE에 올라와 있는 공개 입력만 읽는다.                   */
/*     - IMU raw : g_app_state.gy86.mpu                                        */
/*     - GPS fix : g_app_state.gps.fix                                         */
/*     - OBD input(선택적) : g_app_state.bike.obd_input_*                      */
/*     - GPS 품질 임계값 : 기존 public settings 저장소에 이미 있는             */
/*       altitude.gps_max_pdop_x100 / gps_min_sats 를 재사용한다.             */
/*                                                                            */
/*  2) 이 파일은 "중간 계층 서비스" 이다.                                      */
/*     - 저수준 드라이버를 직접 두드리지 않는다.                               */
/*     - 외부 공개 결과는 반드시 g_app_state.bike 로만 publish 한다.           */
/*                                                                            */
/*  3) 핵심 추정기는 magnetometer-free IMU-first 구조다.                       */
/*     - 쿼터니언 자세 추정은 Mahony 계열 complementary observer를 사용한다.   */
/*     - 단, accelerometer correction은 raw 가속도를 그대로 믿지 않는다.      */
/*     - 외부 speed aid(GNSS / optional OBD)가 있을 때는                       */
/*       "예상 longitudinal / lateral dynamic acceleration" 을 먼저 빼서       */
/*       gravity correction을 더 깨끗하게 만든다.                               */
/*                                                                            */
/*  4) GNSS는 어디까지나 "저주파 aid" 다.                                      */
/*     - 기본 동작은 IMU-only 이다.                                            */
/*     - GNSS는 fix/stale/sAcc/headAcc/DOP/sats를 모두 통과할 때만             */
/*       hysteresis를 거쳐 aid로 승격된다.                                     */
/*     - OBD는 설정에서 켜진 경우에만 최우선 speed aid로 사용한다.             */
/*                                                                            */
/*  5) 서비스 동작(zero capture / gyro calibration)은                          */
/*     한 샘플 통과가 아니라 dwell-window 기반으로 수행한다.                  */
/*     - GNSS가 없다고 곧바로 "정지" 로 간주하지 않는다.                       */
/*     - 외부 속도가 없을 때는 IMU 통계량만으로 정지 구간을 확인한다.           */
/*                                                                            */
/*  6) public ABI / APP_STATE 입출력 계약은 유지한다.                          */
/*     - 함수명, publish 위치, app_state.bike 필드 의미는 기존과 동일하게      */
/*       유지한다.                                                             */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*  물리 상수 / 기본 파라미터                                                  */
/* -------------------------------------------------------------------------- */
#define BIKE_DYN_GRAVITY_MPS2                (9.80665f)
#define BIKE_DYN_GRAVITY_CMS2                (980.665f)
#define BIKE_DYN_GRAVITY_MMPS2               (9806.65f)
#define BIKE_DYN_ONE_G_MG                    (1000.0f)

#define BIKE_DYN_MIN_VEC_NORM                (0.000001f)
#define BIKE_DYN_MIN_DT_S                    (0.001f)
#define BIKE_DYN_MAX_IMU_DT_S                (0.100f)
#define BIKE_DYN_MAX_REFERENCE_DT_S          (2.500f)

#define BIKE_DYN_DEFAULT_ACCEL_LSB_G         (8192.0f)  /* MPU6050 ±4g  */
#define BIKE_DYN_DEFAULT_GYRO_LSB_DPS        (65.5f)    /* MPU6050 ±500dps */
#define BIKE_DYN_DEFAULT_IMU_STALE_MS        (250u)
#define BIKE_DYN_DEFAULT_GNSS_STALE_MS       (1500u)
#define BIKE_DYN_DEFAULT_GNSS_MAX_PDOP_X100  (350u)
#define BIKE_DYN_DEFAULT_GNSS_MIN_SATS       (6u)

#define BIKE_DYN_DEG2RAD                     (0.01745329251994329577f)
#define BIKE_DYN_RAD2DEG                     (57.295779513082320876f)

/* -------------------------------------------------------------------------- */
/*  Zero capture / gyro calibration 정책                                       */
/*                                                                            */
/*  이유                                                                       */
/*  - 버튼을 누르는 순간 또는 부팅 직후 한 샘플만 보고 capture 하면             */
/*    손떨림 / 스탠드 흔들림 / 정차 직전 브레이크 잔진동을 같이 잡을 수 있다.   */
/*  - 따라서 "일정 시간 동안 연속으로 안정 조건을 만족" 해야만                 */
/*    capture / calibration 이 진행되도록 한다.                               */
/* -------------------------------------------------------------------------- */
#define BIKE_DYN_ZERO_CAPTURE_SETTLE_MS      (350u)
#define BIKE_DYN_ZERO_CAPTURE_TARGET_GOOD_MS (1500u)
#define BIKE_DYN_ZERO_CAPTURE_MIN_SAMPLES    (60u)
#define BIKE_DYN_ZERO_CAPTURE_TIMEOUT_MS     (12000u)
#define BIKE_DYN_ZERO_CAPTURE_RESTART_MS     (400u)
#define BIKE_DYN_ZERO_CAPTURE_MAX_GYRO_DPS   (3.5f)
#define BIKE_DYN_ZERO_CAPTURE_MAX_RAW_DPS    (8.0f)
#define BIKE_DYN_ZERO_CAPTURE_ACCEL_SCALE    (1.20f)

#define BIKE_DYN_GYRO_CAL_SETTLE_MS          (600u)
#define BIKE_DYN_GYRO_CAL_TARGET_GOOD_MS     (1800u)
#define BIKE_DYN_GYRO_CAL_TIMEOUT_MS         (7000u)
#define BIKE_DYN_GYRO_CAL_MIN_SAMPLES        (90u)
#define BIKE_DYN_GYRO_CAL_MAX_SPEED_MMPS     (1500)
#define BIKE_DYN_GYRO_CAL_MAX_GYRO_DPS       (6.0f)
#define BIKE_DYN_GYRO_CAL_MAX_RAW_STARTUP_DPS (15.0f)

/* -------------------------------------------------------------------------- */
/*  GNSS aid hysteresis                                                        */
/*                                                                            */
/*  raw quality 한 번 좋다고 바로 aid를 켜면                                  */
/*  수신 품질이 흔들릴 때 estimator mode가 들쭉날쭉 바뀐다.                    */
/* -------------------------------------------------------------------------- */
#define BIKE_DYN_GNSS_GOOD_ENTER_COUNT       (3u)
#define BIKE_DYN_GNSS_BAD_EXIT_COUNT         (2u)

/* -------------------------------------------------------------------------- */
/*  내부 안전 gate / clamp                                                     */
/* -------------------------------------------------------------------------- */
#define BIKE_DYN_MAX_REFERENCE_ABS_G         (1.60f)
#define BIKE_DYN_MAX_DYNAMIC_COMP_ABS_G      (1.40f)
#define BIKE_DYN_BIAS_RATE_LIMIT_G_PER_S     (0.080f)
#define BIKE_DYN_REFERENCE_LPF_TAU_MS        (160u)
#define BIKE_DYN_TRUST_SMOOTH_TAU_MS         (120u)

/* -------------------------------------------------------------------------- */
/*  coordinated manoeuvre lean fusion                                         */
/*                                                                            */
/*  논문식 핵심 아이디어                                                      */
/*  - steady / quasi-steady corner 에서는                                     */
/*      tan(bank) ≈ lateral_specific_force / g                                */
/*  - 즉, speed * yaw-rate 또는 GNSS course-rate 로 얻은 lateral reference 를  */
/*    lean pseudo-measurement 로 다시 사용할 수 있다.                         */
/*                                                                            */
/*  tuning 경향                                                               */
/*  - MIN_LAT_G 를 올리면 직진/저동특성에서 외부 aid 영향이 줄고 더 보수적     */
/*  - FULL_LAT_G 를 내리면 코너 진입부터 coordinated lean 비중이 빨라짐       */
/*  - MAX_LON_G 를 낮추면 제동/가속이 섞인 구간에서 coordinated lean 비중 감소 */
/* -------------------------------------------------------------------------- */
#define BIKE_DYN_COORD_MIN_LAT_G                (0.08f)
#define BIKE_DYN_COORD_FULL_LAT_G               (0.45f)
#define BIKE_DYN_COORD_MAX_LON_G                (0.35f)
#define BIKE_DYN_COORD_BLEND_MAX_FUSION         (0.60f)
#define BIKE_DYN_COORD_BLEND_MAX_SOURCE_ONLY    (1.00f)

/* -------------------------------------------------------------------------- */
/*  online mount self-calibration                                             */
/*                                                                            */
/*  구현 범위                                                                 */
/*  - 현재 구조에서는 zero capture 가 roll/pitch mounting 오차를 이미 흡수한다.*/
/*  - 따라서 온라인 self-cal의 실질적 핵심은 yaw mounting trim 을              */
/*    주행 중 천천히 추정하는 것이다.                                         */
/*                                                                            */
/*  tuning 경향                                                               */
/*  - MIN_REF_G 를 올리면 더 큰 동특성 구간에서만 학습 -> 안정적이지만 느림    */
/*  - MAX_ERR_DEG 를 내리면 이상치 억제는 좋아지나 수렴 속도 저하             */
/*  - RATE_DEG_PER_S 를 올리면 빠르게 맞지만 transient에 과민해질 수 있음      */
/*  - MAX_ABS_YAW_DEG 는 기계 장착 오차 허용 상한                             */
/* -------------------------------------------------------------------------- */
#define BIKE_DYN_MOUNT_SELF_CAL_MIN_REF_G       (0.20f)
#define BIKE_DYN_MOUNT_SELF_CAL_MAX_REF_G       (1.20f)
#define BIKE_DYN_MOUNT_SELF_CAL_MAX_ERR_DEG     (8.0f)
#define BIKE_DYN_MOUNT_SELF_CAL_MAX_ABS_YAW_DEG (12.0f)
#define BIKE_DYN_MOUNT_SELF_CAL_RATE_DEG_PER_S  (0.32f)
#define BIKE_DYN_MOUNT_SELF_CAL_MIN_QUALITY     (450.0f)
#define BIKE_DYN_MOUNT_SELF_CAL_MIN_SPEED_MMPS  (2500)

/* -------------------------------------------------------------------------- */
/*  내부 런타임                                                                 */
/*                                                                            */
/*  중요                                                                       */
/*  - 이 구조체는 BIKE_DYNAMICS 내부 private state 이다.                      */
/*  - 외부 공개값은 반드시 g_app_state.bike 로만 나간다.                       */
/*  - public header / public state layout 은 건드리지 않는다.                  */
/* -------------------------------------------------------------------------- */
typedef struct
{
    /* ---------------------------------------------------------------------- */
    /*  서비스 life-cycle / 입력 유효 플래그                                   */
    /* ---------------------------------------------------------------------- */
    bool     initialized;
    bool     gravity_valid;
    bool     q_valid;
    bool     zero_valid;
    bool     zero_requested;
    bool     hard_rezero_requested;
    bool     auto_zero_done;
    bool     imu_sample_valid;

    /* ---------------------------------------------------------------------- */
    /*  Gyro bias calibration runtime                                          */
    /* ---------------------------------------------------------------------- */
    bool     gyro_bias_valid;
    bool     gyro_bias_cal_requested;
    bool     gyro_bias_cal_active;
    bool     gyro_bias_cal_last_success;

    /* ---------------------------------------------------------------------- */
    /*  GNSS aid hysteresis runtime                                             */
    /* ---------------------------------------------------------------------- */
    bool     gnss_speed_armed;
    bool     gnss_heading_armed;
    uint8_t  gnss_speed_good_count;
    uint8_t  gnss_speed_bad_count;
    uint8_t  gnss_heading_good_count;
    uint8_t  gnss_heading_bad_count;

    /* ---------------------------------------------------------------------- */
    /*  주요 timestamp / bookkeeping                                            */
    /* ---------------------------------------------------------------------- */
    uint32_t init_ms;
    uint32_t last_task_ms;
    uint32_t last_imu_timestamp_ms;
    uint32_t last_mpu_sample_count;
    uint32_t last_gnss_fix_update_ms;
    uint32_t last_zero_capture_ms;

    /* ---------------------------------------------------------------------- */
    /*  zero capture dwell bookkeeping                                          */
    /* ---------------------------------------------------------------------- */
    uint32_t zero_capture_settle_until_ms;
    uint32_t zero_capture_start_ms;
    uint32_t zero_capture_good_ms;
    uint32_t zero_capture_sample_count;

    /* ---------------------------------------------------------------------- */
    /*  gyro bias calibration bookkeeping                                       */
    /* ---------------------------------------------------------------------- */
    uint32_t gyro_bias_cal_start_ms;
    uint32_t gyro_bias_cal_good_ms;
    uint32_t gyro_bias_cal_sample_count;
    uint32_t gyro_bias_cal_success_count;
    uint32_t last_gyro_bias_cal_ms;
    uint32_t gyro_bias_cal_settle_until_ms;

    /* ---------------------------------------------------------------------- */
    /*  attitude quaternion                                                     */
    /* ---------------------------------------------------------------------- */
    float q0;
    float q1;
    float q2;
    float q3;

    /* ---------------------------------------------------------------------- */
    /*  sensor frame 에서 본 world-up unit vector                               */
    /* ---------------------------------------------------------------------- */
    float gravity_est_x_s;
    float gravity_est_y_s;
    float gravity_est_z_s;

    /* ---------------------------------------------------------------------- */
    /*  zero 기준 bike basis in sensor frame                                    */
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
    /*  최근 IMU sample / 공학 단위 캐시                                        */
    /* ---------------------------------------------------------------------- */
    float last_ax_g;
    float last_ay_g;
    float last_az_g;

    float last_gx_raw_dps;
    float last_gy_raw_dps;
    float last_gz_raw_dps;

    float last_gx_dps;
    float last_gy_dps;
    float last_gz_dps;

    float last_dt_s;
    float last_accel_norm_mg;
    float last_accel_corr_norm_mg;
    float last_jerk_mg_per_s;
    float last_linear_mag_mg;
    float last_stationary_conf_permille;
    float last_attitude_trust_permille;
    float yaw_rate_up_dps;

    /* ---------------------------------------------------------------------- */
    /*  heading diagnostic                                                      */
    /*                                                                        */
    /*  본 코어는 magnetometer를 사용하지 않는다.                               */
    /*  public ABI 호환을 위해 mag_* 필드는 남겨 두되,                           */
    /*  항상 false / 0 으로 publish 한다.                                       */
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

    float gyro_bias_sum_x_dps;
    float gyro_bias_sum_y_dps;
    float gyro_bias_sum_z_dps;

    /* ---------------------------------------------------------------------- */
    /*  angle / display smoothing                                               */
    /* ---------------------------------------------------------------------- */
    float bank_raw_deg;
    float grade_raw_deg;
    float bank_imu_deg;      /* gravity-only instantaneous bank estimate      */
    float bank_coord_deg;    /* coordinated-manouevre pseudo measurement      */
    float bank_display_deg;
    float grade_display_deg;
    float bank_rate_dps;
    float grade_rate_dps;

    /* ---------------------------------------------------------------------- */
    /*  online yaw mount self-cal                                              */
    /*                                                                        */
    /*  mount_auto_yaw_deg                                                     */
    /*  - static user trim(settings->mount_yaw_trim_deg_x10) 과 별도로          */
    /*    주행 중 천천히 적응되는 runtime trim                                  */
    /*  - zero capture는 roll/pitch 오차를 잡고, 이 변수는 yaw 오차를 잡는다.  */
    /* ---------------------------------------------------------------------- */
    float mount_auto_yaw_deg;

    /* ---------------------------------------------------------------------- */
    /*  IMU level-frame acceleration                                            */
    /* ---------------------------------------------------------------------- */
    float lon_imu_g;
    float lat_imu_g;

    /* ---------------------------------------------------------------------- */
    /*  external references                                                     */
    /* ---------------------------------------------------------------------- */
    float lon_ref_g;
    float lat_ref_g;
    float lat_ref_fast_g;
    float lat_ref_slow_g;

    float lon_ref_quality_permille;
    float lat_ref_quality_permille;

    /* ---------------------------------------------------------------------- */
    /*  GNSS / OBD aid 로 적응되는 bias                                          */
    /* ---------------------------------------------------------------------- */
    float lon_bias_g;
    float lat_bias_g;

    /* ---------------------------------------------------------------------- */
    /*  최종 표시 후보                                                          */
    /* ---------------------------------------------------------------------- */
    float lon_fused_g;
    float lat_fused_g;

    /* ---------------------------------------------------------------------- */
    /*  GNSS / OBD derivative cache                                             */
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

static uint32_t BIKE_DYN_ClampU32(uint32_t value, uint32_t min_value, uint32_t max_value)
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
    float abs_value_g;
    float deadband_g;
    float clip_g;

    deadband_g = ((float)deadband_mg) * 0.001f;
    clip_g     = ((float)clip_mg) * 0.001f;

    abs_value_g = fabsf(value_g);

    if (abs_value_g <= deadband_g)
    {
        return 0.0f;
    }

    if (clip_g > 0.0f)
    {
        value_g = BIKE_DYN_ClampF(value_g, -clip_g, clip_g);
    }

    if (value_g > 0.0f)
    {
        return value_g - deadband_g;
    }
    return value_g + deadband_g;
}

static int16_t BIKE_DYN_RoundFloatToS16X10(float value)
{
    float clamped;

    clamped = BIKE_DYN_ClampF(value, -32768.0f, 32767.0f);
    if (clamped >= 0.0f)
    {
        return (int16_t)(clamped + 0.5f);
    }
    return (int16_t)(clamped - 0.5f);
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
    return (speed_mmps < 0) ? (-speed_mmps) : speed_mmps;
}

static float BIKE_DYN_LimitDeltaF(float current, float target, float max_step)
{
    float delta;

    if (max_step <= 0.0f)
    {
        return target;
    }

    delta = target - current;
    delta = BIKE_DYN_ClampF(delta, -max_step, max_step);
    return current + delta;
}

/* -------------------------------------------------------------------------- */
/*  축 enum -> unit vector                                                      */
/* -------------------------------------------------------------------------- */
static void BIKE_DYN_AxisEnumToUnitVec(uint8_t axis_enum,
                                       float *x, float *y, float *z)
{
    float lx = 0.0f;
    float ly = 0.0f;
    float lz = 0.0f;

    switch ((app_bike_axis_t)axis_enum)
    {
    case APP_BIKE_AXIS_POS_X: lx =  1.0f; break;
    case APP_BIKE_AXIS_NEG_X: lx = -1.0f; break;
    case APP_BIKE_AXIS_POS_Y: ly =  1.0f; break;
    case APP_BIKE_AXIS_NEG_Y: ly = -1.0f; break;
    case APP_BIKE_AXIS_POS_Z: lz =  1.0f; break;
    case APP_BIKE_AXIS_NEG_Z: lz = -1.0f; break;
    default:                  lx =  1.0f; break;
    }

    if (x != 0) { *x = lx; }
    if (y != 0) { *y = ly; }
    if (z != 0) { *z = lz; }
}

static bool BIKE_DYN_IsAxisEnumValid(uint8_t axis_enum)
{
    switch ((app_bike_axis_t)axis_enum)
    {
    case APP_BIKE_AXIS_POS_X:
    case APP_BIKE_AXIS_NEG_X:
    case APP_BIKE_AXIS_POS_Y:
    case APP_BIKE_AXIS_NEG_Y:
    case APP_BIKE_AXIS_POS_Z:
    case APP_BIKE_AXIS_NEG_Z:
        return true;
    default:
        return false;
    }
}

static uint8_t BIKE_DYN_AxisEnumBaseIndex(uint8_t axis_enum)
{
    switch ((app_bike_axis_t)axis_enum)
    {
    case APP_BIKE_AXIS_POS_X:
    case APP_BIKE_AXIS_NEG_X:
        return 0u;
    case APP_BIKE_AXIS_POS_Y:
    case APP_BIKE_AXIS_NEG_Y:
        return 1u;
    case APP_BIKE_AXIS_POS_Z:
    case APP_BIKE_AXIS_NEG_Z:
        return 2u;
    default:
        return 0u;
    }
}

static void BIKE_DYN_GetSanitizedMountAxes(const app_bike_settings_t *settings,
                                           uint8_t *out_forward_axis,
                                           uint8_t *out_left_axis)
{
    uint8_t forward_axis;
    uint8_t left_axis;

    forward_axis = APP_BIKE_AXIS_POS_X;
    left_axis    = APP_BIKE_AXIS_POS_Y;

    if (settings != 0)
    {
        if (BIKE_DYN_IsAxisEnumValid(settings->mount_forward_axis) != false)
        {
            forward_axis = settings->mount_forward_axis;
        }

        if (BIKE_DYN_IsAxisEnumValid(settings->mount_left_axis) != false)
        {
            left_axis = settings->mount_left_axis;
        }
    }

    if (BIKE_DYN_AxisEnumBaseIndex(forward_axis) == BIKE_DYN_AxisEnumBaseIndex(left_axis))
    {
        switch (BIKE_DYN_AxisEnumBaseIndex(forward_axis))
        {
        case 0u:
            left_axis = APP_BIKE_AXIS_POS_Y;
            break;
        case 1u:
            left_axis = APP_BIKE_AXIS_POS_X;
            break;
        case 2u:
        default:
            left_axis = APP_BIKE_AXIS_POS_X;
            break;
        }
    }

    if (out_forward_axis != 0)
    {
        *out_forward_axis = forward_axis;
    }

    if (out_left_axis != 0)
    {
        *out_left_axis = left_axis;
    }
}


/* -------------------------------------------------------------------------- */
/*  Rodrigues rotation                                                          */
/* -------------------------------------------------------------------------- */
static void BIKE_DYN_RotateVecAroundAxis(float vx, float vy, float vz,
                                         float ax, float ay, float az,
                                         float angle_rad,
                                         float *out_x, float *out_y, float *out_z)
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
    dot = BIKE_DYN_Dot3(vx, vy, vz, ax, ay, az);
    BIKE_DYN_Cross3(ax, ay, az, vx, vy, vz, &cross_x, &cross_y, &cross_z);

    *out_x = (vx * c) + (cross_x * s) + (ax * dot * (1.0f - c));
    *out_y = (vy * c) + (cross_y * s) + (ay * dot * (1.0f - c));
    *out_z = (vz * c) + (cross_z * s) + (az * dot * (1.0f - c));
}

/* -------------------------------------------------------------------------- */
/*  settings / quality helper                                                   */
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

static uint32_t BIKE_DYN_GetImuStaleTimeoutMs(const app_bike_settings_t *settings)
{
    if ((settings != 0) && (settings->imu_stale_timeout_ms != 0u))
    {
        return settings->imu_stale_timeout_ms;
    }
    return BIKE_DYN_DEFAULT_IMU_STALE_MS;
}

static uint16_t BIKE_DYN_GetGnssMaxPdopX100(void)
{
    if (g_app_state.settings.altitude.gps_max_pdop_x100 != 0u)
    {
        return g_app_state.settings.altitude.gps_max_pdop_x100;
    }
    return BIKE_DYN_DEFAULT_GNSS_MAX_PDOP_X100;
}

static uint8_t BIKE_DYN_GetGnssMinSats(void)
{
    if (g_app_state.settings.altitude.gps_min_sats != 0u)
    {
        return g_app_state.settings.altitude.gps_min_sats;
    }
    return BIKE_DYN_DEFAULT_GNSS_MIN_SATS;
}

static uint32_t BIKE_DYN_GetGnssStaleTimeoutMs(void)
{
    return BIKE_DYN_DEFAULT_GNSS_STALE_MS;
}

/* -------------------------------------------------------------------------- */
/*  GNSS quality 계산                                                           */
/* -------------------------------------------------------------------------- */
static bool BIKE_DYN_IsGnssCommonValidRaw(const gps_fix_basic_t *fix,
                                          uint32_t now_ms)
{
    uint32_t age_ms;

    if (fix == 0)
    {
        return false;
    }

    if ((fix->valid == false) || (fix->fixOk == false))
    {
        return false;
    }

    if (fix->last_update_ms == 0u)
    {
        return false;
    }

    age_ms = now_ms - fix->last_update_ms;
    if (age_ms > BIKE_DYN_GetGnssStaleTimeoutMs())
    {
        return false;
    }

    if (fix->numSV_used < BIKE_DYN_GetGnssMinSats())
    {
        return false;
    }

    if ((fix->pDOP == 0u) || (fix->pDOP > BIKE_DYN_GetGnssMaxPdopX100()))
    {
        return false;
    }

    return true;
}

static bool BIKE_DYN_IsGnssSpeedGoodRaw(const gps_fix_basic_t *fix,
                                        const app_bike_settings_t *settings,
                                        uint32_t now_ms)
{
    float speed_acc_kmh_x10;

    if ((fix == 0) || (settings == 0))
    {
        return false;
    }

    if (BIKE_DYN_IsGnssCommonValidRaw(fix, now_ms) == false)
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

static bool BIKE_DYN_IsGnssHeadingGoodRaw(const gps_fix_basic_t *fix,
                                          const app_bike_settings_t *settings,
                                          uint32_t now_ms)
{
    float speed_kmh_x10;
    float head_acc_deg_x10;

    if ((fix == 0) || (settings == 0))
    {
        return false;
    }

    if (BIKE_DYN_IsGnssSpeedGoodRaw(fix, settings, now_ms) == false)
    {
        return false;
    }

    speed_kmh_x10    = ((float)BIKE_DYN_SpeedAbsMmps(fix->gSpeed)) * 0.036f;
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

static float BIKE_DYN_CalcGnssSpeedQuality01(const gps_fix_basic_t *fix,
                                             const app_bike_settings_t *settings,
                                             uint32_t now_ms)
{
    float q_age;
    float q_pdop;
    float q_sats;
    float q_speed_acc;
    float age_ms;
    float max_pdop_x100;
    float min_sats;
    float speed_acc_kmh_x10;

    if ((fix == 0) || (settings == 0))
    {
        return 0.0f;
    }

    if ((fix->valid == false) || (fix->fixOk == false) || (fix->last_update_ms == 0u))
    {
        return 0.0f;
    }

    age_ms = (float)(now_ms - fix->last_update_ms);
    q_age  = 1.0f - (age_ms / (float)BIKE_DYN_GetGnssStaleTimeoutMs());

    max_pdop_x100 = (float)BIKE_DYN_GetGnssMaxPdopX100();
    if ((fix->pDOP == 0u) || (fix->pDOP > (uint16_t)max_pdop_x100))
    {
        q_pdop = 0.0f;
    }
    else
    {
        q_pdop = 1.0f - (((float)fix->pDOP) / max_pdop_x100);
        q_pdop = BIKE_DYN_Clamp01F((0.25f + (0.75f * q_pdop)));
    }

    min_sats = (float)BIKE_DYN_GetGnssMinSats();
    if ((float)fix->numSV_used < min_sats)
    {
        q_sats = 0.0f;
    }
    else
    {
        q_sats = ((float)fix->numSV_used - min_sats + 1.0f) / 5.0f;
        q_sats = BIKE_DYN_Clamp01F(q_sats);
    }

    speed_acc_kmh_x10 = ((float)fix->sAcc) * 0.036f;
    if (settings->gnss_max_speed_acc_kmh_x10 == 0u)
    {
        q_speed_acc = 0.0f;
    }
    else
    {
        q_speed_acc = 1.0f - (speed_acc_kmh_x10 / (float)settings->gnss_max_speed_acc_kmh_x10);
        q_speed_acc = BIKE_DYN_Clamp01F(q_speed_acc);
    }

    return BIKE_DYN_Clamp01F(q_age * q_pdop * q_sats * q_speed_acc);
}

static float BIKE_DYN_CalcGnssHeadingQuality01(const gps_fix_basic_t *fix,
                                               const app_bike_settings_t *settings,
                                               uint32_t now_ms)
{
    float base_q;
    float speed_q;
    float head_q;
    float speed_kmh_x10;
    float head_acc_deg_x10;

    if ((fix == 0) || (settings == 0))
    {
        return 0.0f;
    }

    base_q = BIKE_DYN_CalcGnssSpeedQuality01(fix, settings, now_ms);
    if (base_q <= 0.0f)
    {
        return 0.0f;
    }

    speed_kmh_x10    = ((float)BIKE_DYN_SpeedAbsMmps(fix->gSpeed)) * 0.036f;
    head_acc_deg_x10 = ((float)fix->headAcc) * 0.0001f;

    if (settings->gnss_min_speed_kmh_x10 == 0u)
    {
        speed_q = 1.0f;
    }
    else
    {
        speed_q = speed_kmh_x10 / (float)settings->gnss_min_speed_kmh_x10;
        speed_q = BIKE_DYN_Clamp01F(speed_q);
    }

    if (settings->gnss_max_head_acc_deg_x10 == 0u)
    {
        head_q = 0.0f;
    }
    else
    {
        head_q = 1.0f - (head_acc_deg_x10 / (float)settings->gnss_max_head_acc_deg_x10);
        head_q = BIKE_DYN_Clamp01F(head_q);
    }

    return BIKE_DYN_Clamp01F(base_q * speed_q * head_q);
}

/* -------------------------------------------------------------------------- */
/*  Optional OBD speed validity                                                 */
/* -------------------------------------------------------------------------- */
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

    if ((bike->obd_input_speed_valid == false) || (bike->obd_input_last_update_ms == 0u))
    {
        return false;
    }

    if ((now_ms - bike->obd_input_last_update_ms) > settings->obd_stale_timeout_ms)
    {
        return false;
    }

    return true;
}

/* -------------------------------------------------------------------------- */
/*  GNSS hysteresis update                                                      */
/* -------------------------------------------------------------------------- */
static void BIKE_DYN_UpdateGnssAidHysteresis(bool raw_speed_good,
                                             bool raw_heading_good)
{
    /* speed aid hysteresis */
    if (raw_speed_good != false)
    {
        if (s_bike_runtime.gnss_speed_good_count < 255u)
        {
            s_bike_runtime.gnss_speed_good_count++;
        }
        s_bike_runtime.gnss_speed_bad_count = 0u;

        if ((s_bike_runtime.gnss_speed_armed == false) &&
            (s_bike_runtime.gnss_speed_good_count >= BIKE_DYN_GNSS_GOOD_ENTER_COUNT))
        {
            s_bike_runtime.gnss_speed_armed = true;
        }
    }
    else
    {
        s_bike_runtime.gnss_speed_good_count = 0u;
        if (s_bike_runtime.gnss_speed_bad_count < 255u)
        {
            s_bike_runtime.gnss_speed_bad_count++;
        }

        if ((s_bike_runtime.gnss_speed_armed != false) &&
            (s_bike_runtime.gnss_speed_bad_count >= BIKE_DYN_GNSS_BAD_EXIT_COUNT))
        {
            s_bike_runtime.gnss_speed_armed = false;
        }
    }

    /* heading aid hysteresis */
    if ((raw_speed_good != false) && (raw_heading_good != false))
    {
        if (s_bike_runtime.gnss_heading_good_count < 255u)
        {
            s_bike_runtime.gnss_heading_good_count++;
        }
        s_bike_runtime.gnss_heading_bad_count = 0u;

        if ((s_bike_runtime.gnss_heading_armed == false) &&
            (s_bike_runtime.gnss_heading_good_count >= BIKE_DYN_GNSS_GOOD_ENTER_COUNT))
        {
            s_bike_runtime.gnss_heading_armed = true;
        }
    }
    else
    {
        s_bike_runtime.gnss_heading_good_count = 0u;
        if (s_bike_runtime.gnss_heading_bad_count < 255u)
        {
            s_bike_runtime.gnss_heading_bad_count++;
        }

        if ((s_bike_runtime.gnss_heading_armed != false) &&
            (s_bike_runtime.gnss_heading_bad_count >= BIKE_DYN_GNSS_BAD_EXIT_COUNT))
        {
            s_bike_runtime.gnss_heading_armed = false;
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  bank calculation backend / speed source selection                           */
/* -------------------------------------------------------------------------- */
static uint8_t BIKE_DYN_GetBankCalcMode(const app_bike_settings_t *settings)
{
    if (settings == 0)
    {
        return (uint8_t)APP_BIKE_BANK_CALC_MODE_FUSION;
    }

    switch ((app_bike_bank_calc_mode_t)settings->bank_calc_mode)
    {
    case APP_BIKE_BANK_CALC_MODE_FUSION:
    case APP_BIKE_BANK_CALC_MODE_OBD:
    case APP_BIKE_BANK_CALC_MODE_GNSS:
    case APP_BIKE_BANK_CALC_MODE_IMU_ONLY:
        return settings->bank_calc_mode;

    default:
        return (uint8_t)APP_BIKE_BANK_CALC_MODE_FUSION;
    }
}

static bool BIKE_DYN_ModeAllowsGnss(uint8_t bank_calc_mode)
{
    return ((bank_calc_mode == (uint8_t)APP_BIKE_BANK_CALC_MODE_FUSION) ||
            (bank_calc_mode == (uint8_t)APP_BIKE_BANK_CALC_MODE_GNSS)) ? true : false;
}

static bool BIKE_DYN_ModeAllowsObd(uint8_t bank_calc_mode)
{
    return ((bank_calc_mode == (uint8_t)APP_BIKE_BANK_CALC_MODE_FUSION) ||
            (bank_calc_mode == (uint8_t)APP_BIKE_BANK_CALC_MODE_OBD)) ? true : false;
}

static uint8_t BIKE_DYN_SelectSpeedSource(uint8_t bank_calc_mode,
                                          bool obd_speed_valid,
                                          bool gnss_speed_valid)
{
    switch ((app_bike_bank_calc_mode_t)bank_calc_mode)
    {
    case APP_BIKE_BANK_CALC_MODE_OBD:
        return (obd_speed_valid != false) ? (uint8_t)APP_BIKE_SPEED_SOURCE_OBD
                                          : (uint8_t)APP_BIKE_SPEED_SOURCE_IMU_FALLBACK;

    case APP_BIKE_BANK_CALC_MODE_GNSS:
        return (gnss_speed_valid != false) ? (uint8_t)APP_BIKE_SPEED_SOURCE_GNSS
                                           : (uint8_t)APP_BIKE_SPEED_SOURCE_IMU_FALLBACK;

    case APP_BIKE_BANK_CALC_MODE_IMU_ONLY:
        return (uint8_t)APP_BIKE_SPEED_SOURCE_IMU_FALLBACK;

    case APP_BIKE_BANK_CALC_MODE_FUSION:
    default:
        if (obd_speed_valid != false)
        {
            return (uint8_t)APP_BIKE_SPEED_SOURCE_OBD;
        }

        if (gnss_speed_valid != false)
        {
            return (uint8_t)APP_BIKE_SPEED_SOURCE_GNSS;
        }

        return (uint8_t)APP_BIKE_SPEED_SOURCE_IMU_FALLBACK;
    }
}

static int32_t BIKE_DYN_GetSelectedSpeedMmps(uint8_t speed_source,
                                             const gps_fix_basic_t *fix,
                                             const app_bike_state_t *bike)
{
    if ((speed_source == (uint8_t)APP_BIKE_SPEED_SOURCE_OBD) && (bike != 0))
    {
        return (int32_t)bike->obd_input_speed_mmps;
    }

    if ((speed_source == (uint8_t)APP_BIKE_SPEED_SOURCE_GNSS) && (fix != 0))
    {
        return fix->gSpeed;
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
/*  Quaternion helper                                                           */
/* -------------------------------------------------------------------------- */
static bool BIKE_DYN_QuatNormalize(float *q0, float *q1, float *q2, float *q3)
{
    float norm;

    if ((q0 == 0) || (q1 == 0) || (q2 == 0) || (q3 == 0))
    {
        return false;
    }

    norm = BIKE_DYN_SafeSqrtF((*q0 * *q0) + (*q1 * *q1) + (*q2 * *q2) + (*q3 * *q3));
    if (norm < BIKE_DYN_MIN_VEC_NORM)
    {
        return false;
    }

    *q0 /= norm;
    *q1 /= norm;
    *q2 /= norm;
    *q3 /= norm;
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
    /*  body frame 에서 본 world-up 방향.                                       */
    /*  정지 상태에서 normalized accelerometer 와 같은 방향이다.               */
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

static float BIKE_DYN_CalcMahonyBaseKpRadPerSec(uint16_t gravity_tau_ms)
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
                                     float stationary_conf_01,
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
    float base_kp;
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

    ex = (ay_n * vz) - (az_n * vy);
    ey = (az_n * vx) - (ax_n * vz);
    ez = (ax_n * vy) - (ay_n * vx);

    /* ---------------------------------------------------------------------- */
    /*  Adaptive Mahony gain                                                    */
    /*                                                                        */
    /*  - base gain은 기존 gravity tau 설정을 그대로 따른다.                    */
    /*  - 실제 correction gain은 현재 trust / stationary confidence 에 따라     */
    /*    적응적으로 낮춘다.                                                    */
    /*  - trust가 낮을 때 gain을 사실상 0에 가깝게 내려                       */
    /*    dynamic acceleration에 끌려가는 것을 막는다.                          */
    /* ---------------------------------------------------------------------- */
    base_kp = BIKE_DYN_CalcMahonyBaseKpRadPerSec(gravity_tau_ms);
    kp      = base_kp * BIKE_DYN_Clamp01F(trust_01);
    kp      = BIKE_DYN_ClampF(kp, 0.0f, 12.0f);

    if (stationary_conf_01 > 0.90f)
    {
        kp *= 1.10f;
    }

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
/*  basis builder                                                               */
/* -------------------------------------------------------------------------- */
static bool BIKE_DYN_BuildAxesFromUpAndHints(float up_x,
                                             float up_y,
                                             float up_z,
                                             float fwd_hint_x,
                                             float fwd_hint_y,
                                             float fwd_hint_z,
                                             float left_hint_x,
                                             float left_hint_y,
                                             float left_hint_z,
                                             int16_t yaw_trim_deg_x10,
                                             float *out_fwd_x,
                                             float *out_fwd_y,
                                             float *out_fwd_z,
                                             float *out_left_x,
                                             float *out_left_y,
                                             float *out_left_z,
                                             float *out_up_x,
                                             float *out_up_y,
                                             float *out_up_z)
{
    float proj;
    float fwd_x;
    float fwd_y;
    float fwd_z;
    float left_x;
    float left_y;
    float left_z;
    float ref_left_x;
    float ref_left_y;
    float ref_left_z;

    if ((out_fwd_x == 0) || (out_fwd_y == 0) || (out_fwd_z == 0) ||
        (out_left_x == 0) || (out_left_y == 0) || (out_left_z == 0) ||
        (out_up_x == 0) || (out_up_y == 0) || (out_up_z == 0))
    {
        return false;
    }

    if (BIKE_DYN_Normalize3(&up_x, &up_y, &up_z) == false)
    {
        return false;
    }

    proj  = BIKE_DYN_Dot3(fwd_hint_x, fwd_hint_y, fwd_hint_z, up_x, up_y, up_z);
    fwd_x = fwd_hint_x - (proj * up_x);
    fwd_y = fwd_hint_y - (proj * up_y);
    fwd_z = fwd_hint_z - (proj * up_z);

    if (BIKE_DYN_Normalize3(&fwd_x, &fwd_y, &fwd_z) == false)
    {
        proj    = BIKE_DYN_Dot3(left_hint_x, left_hint_y, left_hint_z, up_x, up_y, up_z);
        left_x  = left_hint_x - (proj * up_x);
        left_y  = left_hint_y - (proj * up_y);
        left_z  = left_hint_z - (proj * up_z);

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

    ref_left_x = left_hint_x;
    ref_left_y = left_hint_y;
    ref_left_z = left_hint_z;
    proj = BIKE_DYN_Dot3(ref_left_x, ref_left_y, ref_left_z, up_x, up_y, up_z);
    ref_left_x -= (proj * up_x);
    ref_left_y -= (proj * up_y);
    ref_left_z -= (proj * up_z);

    if (BIKE_DYN_Normalize3(&ref_left_x, &ref_left_y, &ref_left_z) != false)
    {
        if (BIKE_DYN_Dot3(left_x, left_y, left_z, ref_left_x, ref_left_y, ref_left_z) < 0.0f)
        {
            fwd_x  = -fwd_x;
            fwd_y  = -fwd_y;
            fwd_z  = -fwd_z;
            left_x = -left_x;
            left_y = -left_y;
            left_z = -left_z;
        }
    }

    if (yaw_trim_deg_x10 != 0)
    {
        float angle_rad;
        float rot_fwd_x;
        float rot_fwd_y;
        float rot_fwd_z;

        angle_rad = ((float)yaw_trim_deg_x10) * ((float)M_PI / 1800.0f);
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

    *out_fwd_x  = fwd_x;
    *out_fwd_y  = fwd_y;
    *out_fwd_z  = fwd_z;
    *out_left_x = left_x;
    *out_left_y = left_y;
    *out_left_z = left_z;
    *out_up_x   = up_x;
    *out_up_y   = up_y;
    *out_up_z   = up_z;

    return true;
}

static bool BIKE_DYN_BuildMountLevelAxes(const app_bike_settings_t *settings,
                                         float up_x,
                                         float up_y,
                                         float up_z,
                                         float *out_fwd_x,
                                         float *out_fwd_y,
                                         float *out_fwd_z,
                                         float *out_left_x,
                                         float *out_left_y,
                                         float *out_left_z)
{
    float fwd_hint_x;
    float fwd_hint_y;
    float fwd_hint_z;
    float left_hint_x;
    float left_hint_y;
    float left_hint_z;
    float discard_up_x;
    float discard_up_y;
    float discard_up_z;
    uint8_t forward_axis;
    uint8_t left_axis;

    if (settings == 0)
    {
        return false;
    }

    /* ---------------------------------------------------------------------- */
    /*  mount axis sanitize                                                    */
    /*                                                                        */
    /*  최신 레포에서는 사용자 설정을 그대로 신뢰하면                          */
    /*  forward/left 축이 같은 축으로 저장된 경우                               */
    /*  zero basis rebuild가 끝없이 실패할 수 있다.                             */
    /*                                                                        */
    /*  여기서는 private helper에서 최소한의 정규화를 적용한다.                */
    /*  - invalid enum -> 기본값으로 교정                                       */
    /*  - 같은 축 / 정반대 축 조합 -> 직교 기본축으로 교정                      */
    /*                                                                        */
    /*  public settings ABI는 유지하고, low-level 내부 계산에만 적용한다.       */
    /* ---------------------------------------------------------------------- */
    BIKE_DYN_GetSanitizedMountAxes(settings, &forward_axis, &left_axis);

    BIKE_DYN_AxisEnumToUnitVec(forward_axis, &fwd_hint_x, &fwd_hint_y, &fwd_hint_z);
    BIKE_DYN_AxisEnumToUnitVec(left_axis,    &left_hint_x, &left_hint_y, &left_hint_z);

    return BIKE_DYN_BuildAxesFromUpAndHints(up_x,
                                            up_y,
                                            up_z,
                                            fwd_hint_x,
                                            fwd_hint_y,
                                            fwd_hint_z,
                                            left_hint_x,
                                            left_hint_y,
                                            left_hint_z,
                                            settings->mount_yaw_trim_deg_x10,
                                            out_fwd_x,
                                            out_fwd_y,
                                            out_fwd_z,
                                            out_left_x,
                                            out_left_y,
                                            out_left_z,
                                            &discard_up_x,
                                            &discard_up_y,
                                            &discard_up_z);
}


/* forward declarations used by zero-service static gravity reseed ----------- */
static void BIKE_DYN_QuatSetFromAccelNoYaw(float ax_g, float ay_g, float az_g);
static void BIKE_DYN_UpdateGravityVectorFromQuat(void);
static bool BIKE_DYN_BuildStaticUpFromAccel(const app_bike_settings_t *settings,
                                            float *out_up_x,
                                            float *out_up_y,
                                            float *out_up_z)
{
    float accel_gate_mg;
    float up_x;
    float up_y;
    float up_z;

    if ((settings == 0) ||
        (out_up_x == 0) ||
        (out_up_y == 0) ||
        (out_up_z == 0) ||
        (s_bike_runtime.imu_sample_valid == false))
    {
        return false;
    }

    /* ---------------------------------------------------------------------- */
    /*  zero capture는 "정지 상태의 현재 자세를 새 기준으로 삼는" 서비스다.     */
    /*                                                                        */
    /*  따라서 이 구간에서는 observer trust보다                                */
    /*  현재 accelerometer의 1g 정지 측정을 더 직접적으로 신뢰하는 편이         */
    /*  훨씬 안전하다.                                                          */
    /*                                                                        */
    /*  accel norm이 1g 근처일 때만 현재 raw accel을 world-up 후보로 사용한다. */
    /* ---------------------------------------------------------------------- */
    accel_gate_mg = (float)BIKE_DYN_ClampS32((int32_t)settings->imu_attitude_accel_gate_mg,
                                             50,
                                             300);

    if (fabsf(s_bike_runtime.last_accel_norm_mg - BIKE_DYN_ONE_G_MG) >
        BIKE_DYN_ClampF(accel_gate_mg * BIKE_DYN_ZERO_CAPTURE_ACCEL_SCALE,
                        160.0f,
                        320.0f))
    {
        return false;
    }

    up_x = s_bike_runtime.last_ax_g;
    up_y = s_bike_runtime.last_ay_g;
    up_z = s_bike_runtime.last_az_g;

    if (BIKE_DYN_Normalize3(&up_x, &up_y, &up_z) == false)
    {
        return false;
    }

    *out_up_x = up_x;
    *out_up_y = up_y;
    *out_up_z = up_z;
    return true;
}

static bool BIKE_DYN_ForceGravityFromStaticAccel(const app_bike_settings_t *settings)
{
    float up_x;
    float up_y;
    float up_z;

    if (BIKE_DYN_BuildStaticUpFromAccel(settings, &up_x, &up_y, &up_z) == false)
    {
        return false;
    }

    /* ---------------------------------------------------------------------- */
    /*  circular startup lock breaker                                          */
    /*                                                                        */
    /*  현재 zero 서비스는 "현재 정지 자세를 기준으로 삼아라" 이므로,           */
    /*  서비스가 진행 중인 동안에는 yaw를 버린 accel-only roll/pitch seed를     */
    /*  다시 꽂아도 의미가 맞다.                                                */
    /*                                                                        */
    /*  이렇게 하면 observer trust가 낮아서 zero가 막히고,                     */
    /*  zero가 안 돼서 observer가 다시 안정되지 못하는                          */
    /*  순환 상태를 끊을 수 있다.                                               */
    /* ---------------------------------------------------------------------- */
    BIKE_DYN_QuatSetFromAccelNoYaw(s_bike_runtime.last_ax_g,
                                   s_bike_runtime.last_ay_g,
                                   s_bike_runtime.last_az_g);
    BIKE_DYN_UpdateGravityVectorFromQuat();

    if (s_bike_runtime.gravity_valid == false)
    {
        s_bike_runtime.gravity_est_x_s = up_x;
        s_bike_runtime.gravity_est_y_s = up_y;
        s_bike_runtime.gravity_est_z_s = up_z;
        s_bike_runtime.gravity_valid = true;
    }

    if (s_bike_runtime.last_attitude_trust_permille < 350.0f)
    {
        s_bike_runtime.last_attitude_trust_permille = 350.0f;
    }

    return true;
}



static bool BIKE_DYN_RebuildZeroBasis(const app_bike_settings_t *settings)
{
    float source_up_x;
    float source_up_y;
    float source_up_z;
    float fwd_x;
    float fwd_y;
    float fwd_z;
    float left_x;
    float left_y;
    float left_z;
    float up_x;
    float up_y;
    float up_z;
    bool  have_static_up;

    if (settings == 0)
    {
        return false;
    }

    have_static_up = BIKE_DYN_BuildStaticUpFromAccel(settings,
                                                     &source_up_x,
                                                     &source_up_y,
                                                     &source_up_z);

    if (have_static_up != false)
    {
        (void)BIKE_DYN_ForceGravityFromStaticAccel(settings);
    }
    else
    {
        if (s_bike_runtime.gravity_valid == false)
        {
            return false;
        }

        source_up_x = s_bike_runtime.gravity_est_x_s;
        source_up_y = s_bike_runtime.gravity_est_y_s;
        source_up_z = s_bike_runtime.gravity_est_z_s;
    }

    if (BIKE_DYN_BuildMountLevelAxes(settings,
                                     source_up_x,
                                     source_up_y,
                                     source_up_z,
                                     &fwd_x,
                                     &fwd_y,
                                     &fwd_z,
                                     &left_x,
                                     &left_y,
                                     &left_z) == false)
    {
        return false;
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

    s_bike_runtime.zero_valid                 = true;
    s_bike_runtime.last_zero_capture_ms       = (s_bike_runtime.last_imu_timestamp_ms != 0u)
                                              ? s_bike_runtime.last_imu_timestamp_ms
                                              : s_bike_runtime.last_task_ms;
    s_bike_runtime.zero_capture_start_ms      = 0u;
    s_bike_runtime.zero_capture_good_ms       = 0u;
    s_bike_runtime.zero_capture_sample_count  = 0u;

    /* ---------------------------------------------------------------------- */
    /*  새 기준을 잡는 순간 표시각은 0으로 만든다.                              */
    /*  bias 는 현재 IMU level acceleration 값을 기준으로 재기준화해서          */
    /*  정차 직후 "잔류 lat/lon" 이 남지 않도록 한다.                           */
    /* ---------------------------------------------------------------------- */
    s_bike_runtime.bank_raw_deg      = 0.0f;
    s_bike_runtime.grade_raw_deg     = 0.0f;
    s_bike_runtime.bank_imu_deg      = 0.0f;
    s_bike_runtime.bank_coord_deg    = 0.0f;
    s_bike_runtime.bank_display_deg  = 0.0f;
    s_bike_runtime.grade_display_deg = 0.0f;

    s_bike_runtime.lon_bias_g  = s_bike_runtime.lon_imu_g;
    s_bike_runtime.lat_bias_g  = s_bike_runtime.lat_imu_g;
    s_bike_runtime.lon_fused_g = 0.0f;
    s_bike_runtime.lat_fused_g = 0.0f;

    return true;
}


static int16_t BIKE_DYN_GetRuntimeAutoYawTrimDegX10(void)
{
    return BIKE_DYN_RoundFloatToS16X10(BIKE_DYN_ClampF(s_bike_runtime.mount_auto_yaw_deg * 10.0f,
                                                       -300.0f,
                                                        300.0f));
}

static bool BIKE_DYN_GetCorrectedZeroHints(float *out_fwd_x,
                                           float *out_fwd_y,
                                           float *out_fwd_z,
                                           float *out_left_x,
                                           float *out_left_y,
                                           float *out_left_z,
                                           float *out_up_x,
                                           float *out_up_y,
                                           float *out_up_z)
{
    float fwd_x;
    float fwd_y;
    float fwd_z;
    float left_x;
    float left_y;
    float left_z;
    float up_x;
    float up_y;
    float up_z;
    float rot_fwd_x;
    float rot_fwd_y;
    float rot_fwd_z;
    float angle_rad;

    if ((out_fwd_x == 0) || (out_fwd_y == 0) || (out_fwd_z == 0) ||
        (out_left_x == 0) || (out_left_y == 0) || (out_left_z == 0) ||
        (out_up_x == 0) || (out_up_y == 0) || (out_up_z == 0))
    {
        return false;
    }

    if (s_bike_runtime.zero_valid == false)
    {
        return false;
    }

    fwd_x = s_bike_runtime.zero_fwd_x_s;
    fwd_y = s_bike_runtime.zero_fwd_y_s;
    fwd_z = s_bike_runtime.zero_fwd_z_s;
    left_x = s_bike_runtime.zero_left_x_s;
    left_y = s_bike_runtime.zero_left_y_s;
    left_z = s_bike_runtime.zero_left_z_s;
    up_x = s_bike_runtime.zero_up_x_s;
    up_y = s_bike_runtime.zero_up_y_s;
    up_z = s_bike_runtime.zero_up_z_s;

    /* ------------------------------------------------------------------ */
    /*  online yaw mount self-cal 적용                                       */
    /*                                                                      */
    /*  zero capture는 static up + mount axis로 roll/pitch 기준을 잡는다.   */
    /*  남는 장착 오차는 주로 vertical axis 주위 yaw trim 이므로,            */
    /*  주행 중 학습된 runtime yaw trim 을 zero hints 에만 추가 회전한다.   */
    /* ------------------------------------------------------------------ */
    if (fabsf(s_bike_runtime.mount_auto_yaw_deg) > 0.01f)
    {
        angle_rad = s_bike_runtime.mount_auto_yaw_deg * BIKE_DYN_DEG2RAD;
        BIKE_DYN_RotateVecAroundAxis(fwd_x,
                                     fwd_y,
                                     fwd_z,
                                     up_x,
                                     up_y,
                                     up_z,
                                     angle_rad,
                                     &rot_fwd_x,
                                     &rot_fwd_y,
                                     &rot_fwd_z);

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

    *out_fwd_x = fwd_x;
    *out_fwd_y = fwd_y;
    *out_fwd_z = fwd_z;
    *out_left_x = left_x;
    *out_left_y = left_y;
    *out_left_z = left_z;
    *out_up_x = up_x;
    *out_up_y = up_y;
    *out_up_z = up_z;
    return true;
}

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
    float hint_fwd_x;
    float hint_fwd_y;
    float hint_fwd_z;
    float hint_left_x;
    float hint_left_y;
    float hint_left_z;
    float hint_up_x;
    float hint_up_y;
    float hint_up_z;
    float discard_up_x;
    float discard_up_y;
    float discard_up_z;

    if ((out_fwd_x == 0) || (out_fwd_y == 0) || (out_fwd_z == 0) ||
        (out_left_x == 0) || (out_left_y == 0) || (out_left_z == 0))
    {
        return false;
    }

    if ((s_bike_runtime.gravity_valid == false) || (s_bike_runtime.zero_valid == false))
    {
        return false;
    }

    if (BIKE_DYN_GetCorrectedZeroHints(&hint_fwd_x,
                                       &hint_fwd_y,
                                       &hint_fwd_z,
                                       &hint_left_x,
                                       &hint_left_y,
                                       &hint_left_z,
                                       &hint_up_x,
                                       &hint_up_y,
                                       &hint_up_z) == false)
    {
        return false;
    }

    (void)hint_up_x;
    (void)hint_up_y;
    (void)hint_up_z;

    up_x = s_bike_runtime.gravity_est_x_s;
    up_y = s_bike_runtime.gravity_est_y_s;
    up_z = s_bike_runtime.gravity_est_z_s;

    return BIKE_DYN_BuildAxesFromUpAndHints(up_x,
                                            up_y,
                                            up_z,
                                            hint_fwd_x,
                                            hint_fwd_y,
                                            hint_fwd_z,
                                            hint_left_x,
                                            hint_left_y,
                                            hint_left_z,
                                            0,
                                            out_fwd_x,
                                            out_fwd_y,
                                            out_fwd_z,
                                            out_left_x,
                                            out_left_y,
                                            out_left_z,
                                            &discard_up_x,
                                            &discard_up_y,
                                            &discard_up_z);
}

/* -------------------------------------------------------------------------- */
/*  raw IMU -> engineering unit                                                 */
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
/*  정지 confidence                                                             */
/* -------------------------------------------------------------------------- */
static float BIKE_DYN_CalcStationaryConfidence01(const app_bike_settings_t *settings,
                                                 int32_t selected_speed_mmps,
                                                 uint8_t speed_source,
                                                 float accel_norm_mg,
                                                 float jerk_mg_per_s,
                                                 float gyro_mag_dps,
                                                 float linear_mag_mg)
{
    float accel_gate_mg;
    float jerk_gate_mg_per_s;
    float linear_gate_mg;
    float accel_conf;
    float jerk_conf;
    float gyro_conf;
    float linear_conf;
    float speed_conf;
    float conf;

    if (settings == 0)
    {
        return 0.0f;
    }

    accel_gate_mg      = (float)BIKE_DYN_ClampS32((int32_t)settings->imu_attitude_accel_gate_mg, 40, 300);
    jerk_gate_mg_per_s = (float)BIKE_DYN_ClampS32((int32_t)settings->imu_jerk_gate_mg_per_s / 2, 250, 15000);
    linear_gate_mg     = BIKE_DYN_ClampF(accel_gate_mg * 1.25f, 80.0f, 220.0f);

    accel_conf  = 1.0f - (fabsf(accel_norm_mg - BIKE_DYN_ONE_G_MG) / accel_gate_mg);
    jerk_conf   = 1.0f - (jerk_mg_per_s / jerk_gate_mg_per_s);
    gyro_conf   = 1.0f - (gyro_mag_dps / BIKE_DYN_GYRO_CAL_MAX_GYRO_DPS);
    linear_conf = 1.0f - (linear_mag_mg / linear_gate_mg);

    accel_conf  = BIKE_DYN_Clamp01F(accel_conf);
    jerk_conf   = BIKE_DYN_Clamp01F(jerk_conf);
    gyro_conf   = BIKE_DYN_Clamp01F(gyro_conf);
    linear_conf = BIKE_DYN_Clamp01F(linear_conf);

    conf = accel_conf;
    if (jerk_conf < conf)   { conf = jerk_conf; }
    if (gyro_conf < conf)   { conf = gyro_conf; }
    if (linear_conf < conf) { conf = linear_conf; }

    if (speed_source != (uint8_t)APP_BIKE_SPEED_SOURCE_IMU_FALLBACK)
    {
        speed_conf = 1.0f - (((float)BIKE_DYN_SpeedAbsMmps(selected_speed_mmps)) /
                              (float)(BIKE_DYN_GYRO_CAL_MAX_SPEED_MMPS * 2));
        speed_conf = BIKE_DYN_Clamp01F(speed_conf);
        if (speed_conf < conf)
        {
            conf = speed_conf;
        }
    }

    return BIKE_DYN_Clamp01F(conf);
}

/* -------------------------------------------------------------------------- */
/*  attitude update 에 사용할 dynamic compensation                             */
/*                                                                            */
/*  목적                                                                       */
/*  - accelerometer 는 gravity + dynamic specific-force 를 함께 본다.          */
/*  - cornering / accel / braking 중에는 이 dynamic 성분 때문에                */
/*    roll/pitch observer가 gravity 방향을 오염되게 본다.                      */
/*  - 그래서 외부 speed aid가 있을 때는                                         */
/*      longitudinal ref + v*yaw_rate 기반 lateral ref                         */
/*    를 body/sensor frame으로 다시 투영해                                      */
/*    accelerometer correction 전에 빼 준다.                                   */
/*                                                                            */
/*  주의                                                                       */
/*  - 이 compensation 은 "observer correction" 용이다.                        */
/*  - 최종 lat/lon 출력은 여전히 raw specific-force 경로에서 계산한다.          */
/* -------------------------------------------------------------------------- */
static void BIKE_DYN_ComputeDynamicCompensation(const app_bike_settings_t *settings,
                                                uint8_t speed_source,
                                                bool gnss_heading_valid,
                                                int32_t selected_speed_mmps,
                                                float gx_dps,
                                                float gy_dps,
                                                float gz_dps,
                                                float *out_comp_x_g,
                                                float *out_comp_y_g,
                                                float *out_comp_z_g,
                                                float *out_yaw_rate_up_dps,
                                                float *out_lat_fast_g)
{
    float up_x;
    float up_y;
    float up_z;
    float fwd_x;
    float fwd_y;
    float fwd_z;
    float left_x;
    float left_y;
    float left_z;
    float yaw_rate_up_dps;
    float current_speed_mps;
    float lon_comp_g;
    float lat_fast_g;
    float lat_comp_g;
    float lon_gain;
    float lat_gain;

    if (out_comp_x_g != 0) { *out_comp_x_g = 0.0f; }
    if (out_comp_y_g != 0) { *out_comp_y_g = 0.0f; }
    if (out_comp_z_g != 0) { *out_comp_z_g = 0.0f; }
    if (out_yaw_rate_up_dps != 0) { *out_yaw_rate_up_dps = 0.0f; }
    if (out_lat_fast_g != 0) { *out_lat_fast_g = 0.0f; }

    if ((settings == 0) || (s_bike_runtime.gravity_valid == false))
    {
        return;
    }

    up_x = s_bike_runtime.gravity_est_x_s;
    up_y = s_bike_runtime.gravity_est_y_s;
    up_z = s_bike_runtime.gravity_est_z_s;

    if (s_bike_runtime.zero_valid != false)
    {
        if (BIKE_DYN_BuildCurrentLevelAxes(&fwd_x, &fwd_y, &fwd_z,
                                           &left_x, &left_y, &left_z) == false)
        {
            return;
        }
    }
    else
    {
        if (BIKE_DYN_BuildMountLevelAxes(settings,
                                         up_x,
                                         up_y,
                                         up_z,
                                         &fwd_x,
                                         &fwd_y,
                                         &fwd_z,
                                         &left_x,
                                         &left_y,
                                         &left_z) == false)
        {
            return;
        }
    }

    yaw_rate_up_dps = BIKE_DYN_Dot3(gx_dps, gy_dps, gz_dps, up_x, up_y, up_z);
    current_speed_mps = ((float)BIKE_DYN_SpeedAbsMmps(selected_speed_mmps)) * 0.001f;

    lon_comp_g = 0.0f;
    lat_fast_g = 0.0f;
    lat_comp_g = 0.0f;

    if ((speed_source != (uint8_t)APP_BIKE_SPEED_SOURCE_IMU_FALLBACK) &&
        (current_speed_mps > 0.5f))
    {
        lon_comp_g = s_bike_runtime.lon_ref_g;
        lat_fast_g = (current_speed_mps * (yaw_rate_up_dps * BIKE_DYN_DEG2RAD)) / BIKE_DYN_GRAVITY_MPS2;
        lat_comp_g = lat_fast_g;

        if (gnss_heading_valid != false)
        {
            /* -------------------------------------------------------------- */
            /*  slow GNSS course-rate 는 quantization / latency 가 있으므로   */
            /*  observer compensation 에는 소량만 섞는다.                    */
            /* -------------------------------------------------------------- */
            lat_comp_g = (0.85f * lat_fast_g) + (0.15f * s_bike_runtime.lat_ref_slow_g);
        }
    }

    /* ---------------------------------------------------------------------- */
    /*  aid source 별 gain                                                     */
    /*  - OBD : speed 자체는 일반적으로 가장 빠르고 안정적이므로 더 높게        */
    /*  - GNSS: speed aid 는 쓰되, observer correction gain은 약간 보수적으로   */
    /* ---------------------------------------------------------------------- */
    if (speed_source == (uint8_t)APP_BIKE_SPEED_SOURCE_OBD)
    {
        lon_gain = 1.00f;
        lat_gain = (gnss_heading_valid != false) ? 0.95f : 0.85f;
    }
    else if (speed_source == (uint8_t)APP_BIKE_SPEED_SOURCE_GNSS)
    {
        lon_gain = 0.80f;
        lat_gain = (gnss_heading_valid != false) ? 0.82f : 0.72f;
    }
    else
    {
        lon_gain = 0.0f;
        lat_gain = 0.0f;
    }

    lon_comp_g = BIKE_DYN_ClampF(lon_comp_g, -BIKE_DYN_MAX_DYNAMIC_COMP_ABS_G, BIKE_DYN_MAX_DYNAMIC_COMP_ABS_G) * lon_gain;
    lat_comp_g = BIKE_DYN_ClampF(lat_comp_g, -BIKE_DYN_MAX_DYNAMIC_COMP_ABS_G, BIKE_DYN_MAX_DYNAMIC_COMP_ABS_G) * lat_gain;

    if (out_comp_x_g != 0)
    {
        *out_comp_x_g = (lon_comp_g * fwd_x) + (lat_comp_g * left_x);
    }
    if (out_comp_y_g != 0)
    {
        *out_comp_y_g = (lon_comp_g * fwd_y) + (lat_comp_g * left_y);
    }
    if (out_comp_z_g != 0)
    {
        *out_comp_z_g = (lon_comp_g * fwd_z) + (lat_comp_g * left_z);
    }
    if (out_yaw_rate_up_dps != 0)
    {
        *out_yaw_rate_up_dps = yaw_rate_up_dps;
    }
    if (out_lat_fast_g != 0)
    {
        *out_lat_fast_g = lat_fast_g;
    }
}

/* -------------------------------------------------------------------------- */
/*  IMU-only / aid-assisted attitude observer                                  */
/* -------------------------------------------------------------------------- */
static bool BIKE_DYN_UpdateGravityObserver(const app_gy86_mpu_raw_t *mpu,
                                           const app_bike_settings_t *settings,
                                           uint8_t speed_source,
                                           int32_t selected_speed_mmps,
                                           bool gnss_heading_valid,
                                           uint32_t now_ms)
{
    bool  new_sample;
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
    float accel_corr_x_g;
    float accel_corr_y_g;
    float accel_corr_z_g;
    float accel_corr_norm_g;
    float accel_corr_norm_mg;
    float jerk_mg_per_s;
    float trust_norm;
    float trust_dyn;
    float trust_jerk;
    float trust_total;
    float trust_filtered;
    float dt_s;
    float dax_g;
    float day_g;
    float daz_g;
    float jerk_g_per_s;
    float linear_x_g;
    float linear_y_g;
    float linear_z_g;
    float linear_mag_mg;
    float gyro_mag_dps;
    float stationary_conf_01;
    float dyn_comp_x_g;
    float dyn_comp_y_g;
    float dyn_comp_z_g;
    float yaw_rate_up_dps_local;
    float lat_fast_g_local;
    uint32_t stale_timeout_ms;
    float norm_gate_mg;
    float dyn_gate_mg;
    float jerk_gate_mg_per_s;

    if ((mpu == 0) || (settings == 0))
    {
        s_bike_runtime.imu_sample_valid = false;
        s_bike_runtime.gravity_valid = false;
        s_bike_runtime.q_valid = false;
        s_bike_runtime.last_attitude_trust_permille = 0.0f;
        return false;
    }

    stale_timeout_ms = BIKE_DYN_GetImuStaleTimeoutMs(settings);

    new_sample = (mpu->sample_count != s_bike_runtime.last_mpu_sample_count) ? true : false;

    if ((mpu->sample_count == 0u) || (mpu->timestamp_ms == 0u))
    {
        s_bike_runtime.imu_sample_valid = false;
        s_bike_runtime.gravity_valid = false;
        s_bike_runtime.q_valid = false;
        s_bike_runtime.last_attitude_trust_permille = 0.0f;
        return false;
    }

    if ((now_ms - mpu->timestamp_ms) > stale_timeout_ms)
    {
        s_bike_runtime.imu_sample_valid = false;
        s_bike_runtime.gravity_valid = false;
        s_bike_runtime.q_valid = false;
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
        s_bike_runtime.q_valid = false;
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

    dt_s = BIKE_DYN_ClampF(dt_s, BIKE_DYN_MIN_DT_S, BIKE_DYN_MAX_IMU_DT_S);
    s_bike_runtime.last_dt_s = dt_s;

    if (s_bike_runtime.last_mpu_sample_count != 0u)
    {
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

    /* ---------------------------------------------------------------------- */
    /*  raw linear specific-force magnitude                                    */
    /*  - current gravity estimate가 있을 때만 계산 가능하다.                    */
    /*  - 정지 / service gate / trust 계산에 사용한다.                          */
    /* ---------------------------------------------------------------------- */
    if (s_bike_runtime.gravity_valid != false)
    {
        linear_x_g = ax_g - s_bike_runtime.gravity_est_x_s;
        linear_y_g = ay_g - s_bike_runtime.gravity_est_y_s;
        linear_z_g = az_g - s_bike_runtime.gravity_est_z_s;
        linear_mag_mg = BIKE_DYN_SafeSqrtF((linear_x_g * linear_x_g) +
                                           (linear_y_g * linear_y_g) +
                                           (linear_z_g * linear_z_g)) * 1000.0f;
    }
    else
    {
        linear_mag_mg = 0.0f;
    }

    gyro_mag_dps = BIKE_DYN_SafeSqrtF((gx_dps * gx_dps) + (gy_dps * gy_dps) + (gz_dps * gz_dps));
    stationary_conf_01 = BIKE_DYN_CalcStationaryConfidence01(settings,
                                                             selected_speed_mmps,
                                                             speed_source,
                                                             accel_norm_mg,
                                                             jerk_mg_per_s,
                                                             gyro_mag_dps,
                                                             linear_mag_mg);

    s_bike_runtime.last_linear_mag_mg = linear_mag_mg;
    s_bike_runtime.last_stationary_conf_permille = stationary_conf_01 * 1000.0f;

    /* ---------------------------------------------------------------------- */
    /*  dynamic compensation                                                   */
    /*                                                                        */
    /*  아직 gravity 가 유효하지 않은 초기 구간에서는 compensation 없이         */
    /*  accel 자체로 초기자세를 세운다.                                         */
    /* ---------------------------------------------------------------------- */
    dyn_comp_x_g = 0.0f;
    dyn_comp_y_g = 0.0f;
    dyn_comp_z_g = 0.0f;
    yaw_rate_up_dps_local = 0.0f;
    lat_fast_g_local = 0.0f;

    if (s_bike_runtime.gravity_valid != false)
    {
        BIKE_DYN_ComputeDynamicCompensation(settings,
                                            speed_source,
                                            gnss_heading_valid,
                                            selected_speed_mmps,
                                            gx_dps,
                                            gy_dps,
                                            gz_dps,
                                            &dyn_comp_x_g,
                                            &dyn_comp_y_g,
                                            &dyn_comp_z_g,
                                            &yaw_rate_up_dps_local,
                                            &lat_fast_g_local);
    }

    accel_corr_x_g = ax_g - dyn_comp_x_g;
    accel_corr_y_g = ay_g - dyn_comp_y_g;
    accel_corr_z_g = az_g - dyn_comp_z_g;

    accel_corr_norm_g  = BIKE_DYN_SafeSqrtF((accel_corr_x_g * accel_corr_x_g) +
                                            (accel_corr_y_g * accel_corr_y_g) +
                                            (accel_corr_z_g * accel_corr_z_g));
    accel_corr_norm_mg = accel_corr_norm_g * 1000.0f;
    s_bike_runtime.last_accel_corr_norm_mg = accel_corr_norm_mg;

    norm_gate_mg       = (float)BIKE_DYN_ClampS32((int32_t)settings->imu_attitude_accel_gate_mg, 50, 400);
    dyn_gate_mg        = BIKE_DYN_ClampF(norm_gate_mg * 2.0f, 160.0f, 700.0f);
    jerk_gate_mg_per_s = (float)BIKE_DYN_ClampS32((int32_t)settings->imu_jerk_gate_mg_per_s, 500, 20000);

    trust_norm = 1.0f - (fabsf(accel_corr_norm_mg - BIKE_DYN_ONE_G_MG) / norm_gate_mg);
    trust_dyn  = 1.0f - (linear_mag_mg / dyn_gate_mg);
    trust_jerk = 1.0f - (jerk_mg_per_s / jerk_gate_mg_per_s);

    trust_norm = BIKE_DYN_Clamp01F(trust_norm);
    trust_dyn  = BIKE_DYN_Clamp01F(trust_dyn);
    trust_jerk = BIKE_DYN_Clamp01F(trust_jerk);

    /* ---------------------------------------------------------------------- */
    /*  observer trust 조합                                                    */
    /*                                                                        */
    /*  - corrected norm 이 1g에 가까운가?                                     */
    /*  - residual dynamic magnitude 가 충분히 작은가?                          */
    /*  - jerk 가 과도하지 않은가?                                              */
    /*  - 정지 confidence 가 높을 때는 최소한의 correction 을 허용한다.         */
    /* ---------------------------------------------------------------------- */
    /* ------------------------------------------------------------------ */
    /*  trust_total 조합                                                   */
    /*                                                                    */
    /*  핵심 설계                                                         */
    /*  1) norm trust 는 가장 강한 기본 신호이다.                          */
    /*  2) jerk 는 급격한 충격 / 럼블 / 과도구간을 누르되,                  */
    /*     sustained pose change 자체를 완전히 막지 않도록 완만하게 반영한다.*/
    /*  3) linear_mag 기반 trust_dyn 은 기존 gravity와의 차이를 보기 때문에 */
    /*     정적인 자세 변경(예: 주차 후 차체를 기울임)까지 막아 버릴 수 있다.*/
    /*     따라서 외부 속도 aid가 있을 때에만 보조적으로 사용한다.          */
    /*  4) IMU-only 모드에서는 yaw-rate가 큰 구간에서만 correction을 더      */
    /*     보수적으로 줄여, 장시간 코너링 중 가속도에 끌려가는 것을 막는다. */
    /* ------------------------------------------------------------------ */
    trust_total = trust_norm;
    trust_total *= (0.25f + (0.75f * trust_jerk));

    if (speed_source != (uint8_t)APP_BIKE_SPEED_SOURCE_IMU_FALLBACK)
    {
        trust_total *= (0.35f + (0.65f * trust_dyn));
    }
    else
    {
        float turn_conf;

        turn_conf = 1.0f - (fabsf(yaw_rate_up_dps_local) / 45.0f);
        turn_conf = BIKE_DYN_Clamp01F(turn_conf);

        if (stationary_conf_01 < 0.85f)
        {
            trust_total *= (0.25f + (0.75f * turn_conf));
        }
    }

    if (stationary_conf_01 > 0.95f)
    {
        if (trust_total < 0.85f)
        {
            trust_total = 0.85f;
        }
    }
    else if (stationary_conf_01 > trust_total)
    {
        trust_total = (0.25f * stationary_conf_01) + (0.75f * trust_total);
    }

    trust_total = BIKE_DYN_Clamp01F(trust_total);
    trust_filtered = BIKE_DYN_LpfUpdate(s_bike_runtime.last_attitude_trust_permille * 0.001f,
                                        trust_total,
                                        BIKE_DYN_TRUST_SMOOTH_TAU_MS,
                                        dt_s);
    trust_filtered = BIKE_DYN_Clamp01F(trust_filtered);

    if ((s_bike_runtime.q_valid == false) || (s_bike_runtime.gravity_valid == false))
    {
        BIKE_DYN_QuatSetFromAccelNoYaw(accel_corr_x_g, accel_corr_y_g, accel_corr_z_g);
        BIKE_DYN_UpdateGravityVectorFromQuat();
    }
    else
    {
        BIKE_DYN_MahonyImuUpdate(accel_corr_x_g,
                                 accel_corr_y_g,
                                 accel_corr_z_g,
                                 gx_dps,
                                 gy_dps,
                                 gz_dps,
                                 trust_filtered,
                                 stationary_conf_01,
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
    s_bike_runtime.last_attitude_trust_permille = 1000.0f * trust_filtered;

    /* ---------------------------------------------------------------------- */
    /*  yaw-rate over current up                                                */
    /*  - compensation 전에 계산한 local value가 있으면 그대로 사용한다.         */
    /*  - 그렇지 않으면 업데이트된 gravity 기준으로 다시 계산한다.               */
    /* ---------------------------------------------------------------------- */
    if (s_bike_runtime.gravity_valid != false)
    {
        if (fabsf(yaw_rate_up_dps_local) <= 0.0f)
        {
            yaw_rate_up_dps_local = BIKE_DYN_Dot3(s_bike_runtime.last_gx_dps,
                                                  s_bike_runtime.last_gy_dps,
                                                  s_bike_runtime.last_gz_dps,
                                                  s_bike_runtime.gravity_est_x_s,
                                                  s_bike_runtime.gravity_est_y_s,
                                                  s_bike_runtime.gravity_est_z_s);
        }
    }

    s_bike_runtime.yaw_rate_up_dps = yaw_rate_up_dps_local;
    s_bike_runtime.lat_ref_fast_g  = lat_fast_g_local;

    return true;
}

/* -------------------------------------------------------------------------- */
/*  zero capture sample good?                                                   */
/* -------------------------------------------------------------------------- */
static bool BIKE_DYN_IsZeroCaptureSampleGood(const app_bike_settings_t *settings,
                                             int32_t selected_speed_mmps,
                                             uint8_t speed_source)
{
    float accel_gate_mg;
    float jerk_gate_mg_per_s;
    float gyro_mag_dps;
    float dummy_fwd_x;
    float dummy_fwd_y;
    float dummy_fwd_z;
    float dummy_left_x;
    float dummy_left_y;
    float dummy_left_z;
    float up_x;
    float up_y;
    float up_z;
    bool  have_external_speed;

    if ((settings == 0) || (s_bike_runtime.imu_sample_valid == false))
    {
        return false;
    }

    have_external_speed =
        (speed_source != (uint8_t)APP_BIKE_SPEED_SOURCE_IMU_FALLBACK) ? true : false;

    if ((have_external_speed != false) &&
        (BIKE_DYN_SpeedAbsMmps(selected_speed_mmps) > BIKE_DYN_GYRO_CAL_MAX_SPEED_MMPS))
    {
        return false;
    }

    accel_gate_mg = (float)BIKE_DYN_ClampS32((int32_t)settings->imu_attitude_accel_gate_mg,
                                             50,
                                             300);

    if (fabsf(s_bike_runtime.last_accel_norm_mg - BIKE_DYN_ONE_G_MG) >
        BIKE_DYN_ClampF(accel_gate_mg * BIKE_DYN_ZERO_CAPTURE_ACCEL_SCALE,
                        160.0f,
                        320.0f))
    {
        return false;
    }

    jerk_gate_mg_per_s = (float)BIKE_DYN_ClampS32((int32_t)settings->imu_jerk_gate_mg_per_s,
                                                  500,
                                                  20000);
    if (fabsf(s_bike_runtime.last_jerk_mg_per_s) > jerk_gate_mg_per_s)
    {
        return false;
    }

    if (s_bike_runtime.gyro_bias_valid != false)
    {
        gyro_mag_dps = BIKE_DYN_SafeSqrtF((s_bike_runtime.last_gx_dps * s_bike_runtime.last_gx_dps) +
                                          (s_bike_runtime.last_gy_dps * s_bike_runtime.last_gy_dps) +
                                          (s_bike_runtime.last_gz_dps * s_bike_runtime.last_gz_dps));
        if (gyro_mag_dps > BIKE_DYN_ZERO_CAPTURE_MAX_GYRO_DPS)
        {
            return false;
        }
    }
    else
    {
        gyro_mag_dps = BIKE_DYN_SafeSqrtF((s_bike_runtime.last_gx_raw_dps * s_bike_runtime.last_gx_raw_dps) +
                                          (s_bike_runtime.last_gy_raw_dps * s_bike_runtime.last_gy_raw_dps) +
                                          (s_bike_runtime.last_gz_raw_dps * s_bike_runtime.last_gz_raw_dps));
        if (gyro_mag_dps > BIKE_DYN_ZERO_CAPTURE_MAX_RAW_DPS)
        {
            return false;
        }
    }

    /* ---------------------------------------------------------------------- */
    /*  zero capture는 observer trust를 hard gate로 쓰지 않는다.               */
    /*                                                                        */
    /*  이유                                                                   */
    /*  - 지금 벤치 증상은 trust가 낮아서 zero가 막히고,                        */
    /*    zero가 막혀서 다시 trust가 올라오지 않는 순환 구조였다.              */
    /*  - 정지 상태 zero는 "현재 1g 방향"만 확실하면 충분하므로,                */
    /*    raw accel + gyro/speed/jerk gate로 직접 판정하는 편이 더 안전하다.   */
    /* ---------------------------------------------------------------------- */
    if (BIKE_DYN_BuildStaticUpFromAccel(settings, &up_x, &up_y, &up_z) == false)
    {
        return false;
    }

    if (BIKE_DYN_BuildMountLevelAxes(settings,
                                     up_x,
                                     up_y,
                                     up_z,
                                     &dummy_fwd_x,
                                     &dummy_fwd_y,
                                     &dummy_fwd_z,
                                     &dummy_left_x,
                                     &dummy_left_y,
                                     &dummy_left_z) == false)
    {
        return false;
    }

    return true;
}


static bool BIKE_DYN_IsGyroCalibrationSampleGood(const app_bike_settings_t *settings,
                                                 int32_t selected_speed_mmps,
                                                 uint8_t speed_source)
{
    float accel_gate_mg;
    float jerk_gate_mg_per_s;
    float stationary_conf_01;
    float gyro_mag_dps;
    bool  have_external_speed;

    if ((settings == 0) || (s_bike_runtime.imu_sample_valid == false))
    {
        return false;
    }

    have_external_speed = (speed_source != (uint8_t)APP_BIKE_SPEED_SOURCE_IMU_FALLBACK) ? true : false;
    if ((have_external_speed != false) &&
        (BIKE_DYN_SpeedAbsMmps(selected_speed_mmps) > BIKE_DYN_GYRO_CAL_MAX_SPEED_MMPS))
    {
        return false;
    }

    stationary_conf_01 = s_bike_runtime.last_stationary_conf_permille * 0.001f;

    /* ---------------------------------------------------------------------- */
    /*  핵심 수정                                                               */
    /*                                                                        */
    /*  기존 구현은 "gyro bias가 아직 없을 때" 조차                           */
    /*  stationary_conf(내부적으로 raw gyro 크기를 이미 포함)와                */
    /*  raw gyro magnitude <= 6 dps 를 동시에 요구했다.                        */
    /*                                                                        */
    /*  그 결과 first-time calibration에서 raw bias가 조금만 큰 유닛은         */
    /*  시작점부터 good sample이 하나도 쌓이지 못하고,                          */
    /*  active=true / progress=0%% 에 머무는 순환 구조가 생겼다.                */
    /*                                                                        */
    /*  수정 정책                                                               */
    /*  - bias가 이미 유효한 이후의 재보정은 기존처럼 엄격하게 본다.           */
    /*  - bias가 아직 없는 최초 보정은 accel/jitter/speed 위주 still gate를     */
    /*    사용하고, raw gyro 자체는 더 완화된 startup 임계값으로만 제한한다.    */
    /* ---------------------------------------------------------------------- */
    if ((s_bike_runtime.gyro_bias_valid != false) && (stationary_conf_01 < 0.96f))
    {
        return false;
    }

    accel_gate_mg = (float)BIKE_DYN_ClampS32((int32_t)settings->imu_attitude_accel_gate_mg, 50, 300);
    if (fabsf(s_bike_runtime.last_accel_norm_mg - BIKE_DYN_ONE_G_MG) > (accel_gate_mg * 1.15f))
    {
        return false;
    }

    jerk_gate_mg_per_s = (float)BIKE_DYN_ClampS32((int32_t)settings->imu_jerk_gate_mg_per_s, 500, 20000);
    if (fabsf(s_bike_runtime.last_jerk_mg_per_s) > jerk_gate_mg_per_s)
    {
        return false;
    }

    if (s_bike_runtime.gyro_bias_valid != false)
    {
        gyro_mag_dps = BIKE_DYN_SafeSqrtF((s_bike_runtime.last_gx_dps * s_bike_runtime.last_gx_dps) +
                                          (s_bike_runtime.last_gy_dps * s_bike_runtime.last_gy_dps) +
                                          (s_bike_runtime.last_gz_dps * s_bike_runtime.last_gz_dps));
        if (gyro_mag_dps > BIKE_DYN_GYRO_CAL_MAX_GYRO_DPS)
        {
            return false;
        }
    }
    else
    {
        gyro_mag_dps = BIKE_DYN_SafeSqrtF((s_bike_runtime.last_gx_raw_dps * s_bike_runtime.last_gx_raw_dps) +
                                          (s_bike_runtime.last_gy_raw_dps * s_bike_runtime.last_gy_raw_dps) +
                                          (s_bike_runtime.last_gz_raw_dps * s_bike_runtime.last_gz_raw_dps));
        if (gyro_mag_dps > BIKE_DYN_GYRO_CAL_MAX_RAW_STARTUP_DPS)
        {
            return false;
        }
    }

    return true;
}

/* -------------------------------------------------------------------------- */
/*  output from current IMU                                                     */
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
    float zero_hint_fwd_x;
    float zero_hint_fwd_y;
    float zero_hint_fwd_z;
    float zero_hint_left_x;
    float zero_hint_left_y;
    float zero_hint_left_z;
    float zero_hint_up_x;
    float zero_hint_up_y;
    float zero_hint_up_z;
    float dyn_x_s;
    float dyn_y_s;
    float dyn_z_s;
    float lon_raw_g;
    float lat_raw_g;
    bool  level_axes_valid;

    if ((settings == 0) ||
        (s_bike_runtime.gravity_valid == false))
    {
        return;
    }

    dt_s = BIKE_DYN_ClampF(s_bike_runtime.last_dt_s, BIKE_DYN_MIN_DT_S, BIKE_DYN_MAX_IMU_DT_S);
    level_axes_valid = false;

    if (s_bike_runtime.zero_valid != false)
    {
        /* ------------------------------------------------------------------ */
        /*  zero 기준이 유효한 뒤에는 bank/grade/rate 모두 canonical basis를    */
        /*  사용한다.                                                           */
        /*                                                                    */
        /*  단, canonical basis 자체에도 runtime yaw self-cal 을 반영해서      */
        /*  mounting yaw 오차 때문에 bank 와 grade 가 서로 섞이는 현상을      */
        /*  완화한다.                                                           */
        /* ------------------------------------------------------------------ */
        if (BIKE_DYN_GetCorrectedZeroHints(&zero_hint_fwd_x,
                                           &zero_hint_fwd_y,
                                           &zero_hint_fwd_z,
                                           &zero_hint_left_x,
                                           &zero_hint_left_y,
                                           &zero_hint_left_z,
                                           &zero_hint_up_x,
                                           &zero_hint_up_y,
                                           &zero_hint_up_z) == false)
        {
            return;
        }

        up_bx = BIKE_DYN_Dot3(zero_hint_fwd_x,  zero_hint_fwd_y,  zero_hint_fwd_z,
                              s_bike_runtime.gravity_est_x_s, s_bike_runtime.gravity_est_y_s, s_bike_runtime.gravity_est_z_s);
        up_by = BIKE_DYN_Dot3(zero_hint_left_x, zero_hint_left_y, zero_hint_left_z,
                              s_bike_runtime.gravity_est_x_s, s_bike_runtime.gravity_est_y_s, s_bike_runtime.gravity_est_z_s);
        up_bz = BIKE_DYN_Dot3(zero_hint_up_x,   zero_hint_up_y,   zero_hint_up_z,
                              s_bike_runtime.gravity_est_x_s, s_bike_runtime.gravity_est_y_s, s_bike_runtime.gravity_est_z_s);

        level_axes_valid = BIKE_DYN_BuildCurrentLevelAxes(&level_fwd_x,
                                                          &level_fwd_y,
                                                          &level_fwd_z,
                                                          &level_left_x,
                                                          &level_left_y,
                                                          &level_left_z);

        bank_rad  = atan2f(-up_by, BIKE_DYN_ClampF(up_bz, -1.0f, 1.0f));
        grade_rad = atan2f(-up_bx, BIKE_DYN_SafeSqrtF((up_by * up_by) + (up_bz * up_bz)));

        s_bike_runtime.bank_imu_deg  = bank_rad  * BIKE_DYN_RAD2DEG;
        s_bike_runtime.bank_coord_deg = s_bike_runtime.bank_imu_deg;
        s_bike_runtime.bank_raw_deg  = s_bike_runtime.bank_imu_deg;
        s_bike_runtime.grade_raw_deg = grade_rad * BIKE_DYN_RAD2DEG;

        if (level_axes_valid != false)
        {
            s_bike_runtime.bank_rate_dps  = -BIKE_DYN_Dot3(s_bike_runtime.last_gx_dps,
                                                           s_bike_runtime.last_gy_dps,
                                                           s_bike_runtime.last_gz_dps,
                                                           level_fwd_x,
                                                           level_fwd_y,
                                                           level_fwd_z);

            s_bike_runtime.grade_rate_dps = -BIKE_DYN_Dot3(s_bike_runtime.last_gx_dps,
                                                           s_bike_runtime.last_gy_dps,
                                                           s_bike_runtime.last_gz_dps,
                                                           level_left_x,
                                                           level_left_y,
                                                           level_left_z);
        }
        else
        {
            s_bike_runtime.bank_rate_dps  = 0.0f;
            s_bike_runtime.grade_rate_dps = 0.0f;
        }

        /* ------------------------------------------------------------------ */
        /*  bank_display_deg 는 coordinated fusion 단계에서 최종 목표값으로     */
        /*  업데이트한다.                                                       */
        /*                                                                    */
        /*  이유                                                               */
        /*  - gravity-only bank 와 coordinated lean pseudo-measurement 를      */
        /*    한 프레임 안에서 다시 합성해야 하므로                              */
        /*    display LPF 는 그 합성 뒤에 한 번만 적용하는 편이 더 명확하다.    */
        /* ------------------------------------------------------------------ */
        s_bike_runtime.grade_display_deg = BIKE_DYN_LpfUpdate(s_bike_runtime.grade_display_deg,
                                                              s_bike_runtime.grade_raw_deg,
                                                              settings->grade_display_tau_ms,
                                                              dt_s);
    }
    else
    {
        /* ------------------------------------------------------------------ */
        /*  zero_valid 전에는 rider-facing lean/grade/rate를 절대 열지 않는다. */
        /*                                                                    */
        /*  과거 provisional display path는 mount-axis만으로 만든 임시 basis를 */
        /*  바로 UI에 노출해서, 부팅 직후 ±170° branch-cut 값이 화면과 history */
        /*  경로로 새는 문제를 만들었다.                                        */
        /*                                                                    */
        /*  여기서는 display 출력을 모두 0으로 잠근다.                         */
        /*  다만 내부적으로는 mount-level axes를 계속 갱신해서,                 */
        /*  - lat/lon IMU 성분의 현재값                                         */
        /*  - zero capture 직후 bias 재기준화                                   */
        /*  에 필요한 최소 내부 상태는 유지한다.                                */
        /* ------------------------------------------------------------------ */
        level_axes_valid = BIKE_DYN_BuildMountLevelAxes(settings,
                                                        s_bike_runtime.gravity_est_x_s,
                                                        s_bike_runtime.gravity_est_y_s,
                                                        s_bike_runtime.gravity_est_z_s,
                                                        &level_fwd_x,
                                                        &level_fwd_y,
                                                        &level_fwd_z,
                                                        &level_left_x,
                                                        &level_left_y,
                                                        &level_left_z);

        s_bike_runtime.bank_raw_deg      = 0.0f;
        s_bike_runtime.grade_raw_deg     = 0.0f;
        s_bike_runtime.bank_imu_deg      = 0.0f;
        s_bike_runtime.bank_coord_deg    = 0.0f;
        s_bike_runtime.bank_rate_dps     = 0.0f;
        s_bike_runtime.grade_rate_dps    = 0.0f;
        s_bike_runtime.bank_display_deg  = 0.0f;
        s_bike_runtime.grade_display_deg = 0.0f;
    }

    dyn_x_s = s_bike_runtime.last_ax_g - s_bike_runtime.gravity_est_x_s;
    dyn_y_s = s_bike_runtime.last_ay_g - s_bike_runtime.gravity_est_y_s;
    dyn_z_s = s_bike_runtime.last_az_g - s_bike_runtime.gravity_est_z_s;

    if (level_axes_valid != false)
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
/*  external references                                                         */
/* -------------------------------------------------------------------------- */
static void BIKE_DYN_UpdateExternalReferences(const app_bike_settings_t *settings,
                                              uint8_t speed_source,
                                              bool gnss_speed_valid,
                                              bool gnss_heading_valid,
                                              bool obd_speed_valid,
                                              int32_t selected_speed_mmps,
                                              uint32_t now_ms)
{
    const gps_fix_basic_t  *fix;
    const app_bike_state_t *bike;
    float dt_s;
    float derivative_g;
    float heading_deg;
    float heading_delta_deg;
    float course_rate_rad_s;
    float current_speed_mps;
    uint32_t current_gnss_ms;
    uint32_t current_obd_ms;

    (void)now_ms;

    if (settings == 0)
    {
        return;
    }

    fix  = (const gps_fix_basic_t *)&g_app_state.gps.fix;
    bike = (const app_bike_state_t *)&g_app_state.bike;

    current_gnss_ms = fix->last_update_ms;
    current_obd_ms  = bike->obd_input_last_update_ms;

    /* ---------------------------------------------------------------------- */
    /*  longitudinal reference                                                  */
    /*  - 반드시 실제 producer timestamp 차이로 dt를 계산한다.                  */
    /*  - 기존처럼 0.1 s 로 강제 clamp 해서 과대가속이 튀지 않도록 한다.        */
    /* ---------------------------------------------------------------------- */
    if ((speed_source == (uint8_t)APP_BIKE_SPEED_SOURCE_OBD) &&
        (obd_speed_valid != false) &&
        (current_obd_ms != 0u) &&
        (current_obd_ms != s_bike_runtime.prev_obd_speed_ms))
    {
        if ((s_bike_runtime.prev_obd_speed_valid != false) &&
            (current_obd_ms > s_bike_runtime.prev_obd_speed_ms))
        {
            dt_s = ((float)(current_obd_ms - s_bike_runtime.prev_obd_speed_ms)) * 0.001f;
            dt_s = BIKE_DYN_ClampF(dt_s, 0.010f, BIKE_DYN_MAX_REFERENCE_DT_S);

            derivative_g = (((float)bike->obd_input_speed_mmps - (float)s_bike_runtime.prev_obd_speed_mmps) / dt_s) /
                           BIKE_DYN_GRAVITY_MMPS2;
            derivative_g = BIKE_DYN_ClampF(derivative_g,
                                           -BIKE_DYN_MAX_REFERENCE_ABS_G,
                                           BIKE_DYN_MAX_REFERENCE_ABS_G);

            s_bike_runtime.lon_ref_g = BIKE_DYN_LpfUpdate(s_bike_runtime.lon_ref_g,
                                                          derivative_g,
                                                          BIKE_DYN_REFERENCE_LPF_TAU_MS,
                                                          dt_s);
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
            dt_s = BIKE_DYN_ClampF(dt_s, 0.010f, BIKE_DYN_MAX_REFERENCE_DT_S);

            derivative_g = (((float)fix->gSpeed - (float)s_bike_runtime.prev_gnss_speed_mmps) / dt_s) /
                           BIKE_DYN_GRAVITY_MMPS2;
            derivative_g = BIKE_DYN_ClampF(derivative_g,
                                           -BIKE_DYN_MAX_REFERENCE_ABS_G,
                                           BIKE_DYN_MAX_REFERENCE_ABS_G);

            s_bike_runtime.lon_ref_g = BIKE_DYN_LpfUpdate(s_bike_runtime.lon_ref_g,
                                                          derivative_g,
                                                          BIKE_DYN_REFERENCE_LPF_TAU_MS,
                                                          dt_s);
        }

        s_bike_runtime.prev_gnss_speed_valid = true;
        s_bike_runtime.prev_gnss_speed_mmps  = fix->gSpeed;
        s_bike_runtime.prev_gnss_speed_ms    = current_gnss_ms;
    }

    /* ---------------------------------------------------------------------- */
    /*  slow lateral reference                                                  */
    /*  - GNSS heading derivative 기반                                          */
    /*  - lat_ref_fast_g 는 IMU update 후 별도로 계산한다.                      */
    /* ---------------------------------------------------------------------- */
    current_speed_mps = ((float)BIKE_DYN_SpeedAbsMmps(selected_speed_mmps)) * 0.001f;

    if ((gnss_heading_valid != false) &&
        (current_gnss_ms != 0u) &&
        (current_gnss_ms != s_bike_runtime.prev_gnss_heading_ms))
    {
        heading_deg = ((float)fix->headMot) * 0.00001f;

        if ((s_bike_runtime.prev_gnss_heading_valid != false) &&
            (current_gnss_ms > s_bike_runtime.prev_gnss_heading_ms) &&
            (current_speed_mps > 0.5f))
        {
            dt_s = ((float)(current_gnss_ms - s_bike_runtime.prev_gnss_heading_ms)) * 0.001f;
            dt_s = BIKE_DYN_ClampF(dt_s, 0.010f, BIKE_DYN_MAX_REFERENCE_DT_S);

            heading_delta_deg = BIKE_DYN_WrapDeg(heading_deg - s_bike_runtime.prev_gnss_heading_deg);
            course_rate_rad_s = (heading_delta_deg * BIKE_DYN_DEG2RAD) / dt_s;

            /* ------------------------------------------------------------------ */
            /*  UBX headMot 는 시계방향 증가이므로                                  */
            /*  좌회전 + sign 규칙에 맞추기 위해 부호를 뒤집는다.                  */
            /* ------------------------------------------------------------------ */
            s_bike_runtime.lat_ref_slow_g = BIKE_DYN_LpfUpdate(
                s_bike_runtime.lat_ref_slow_g,
                BIKE_DYN_ClampF(-(current_speed_mps * course_rate_rad_s) / BIKE_DYN_GRAVITY_MPS2,
                                -BIKE_DYN_MAX_REFERENCE_ABS_G,
                                 BIKE_DYN_MAX_REFERENCE_ABS_G),
                BIKE_DYN_REFERENCE_LPF_TAU_MS,
                dt_s);
        }

        s_bike_runtime.prev_gnss_heading_valid = true;
        s_bike_runtime.prev_gnss_heading_deg   = heading_deg;
        s_bike_runtime.prev_gnss_heading_ms    = current_gnss_ms;
    }

    s_bike_runtime.lon_ref_quality_permille =
        ((speed_source == (uint8_t)APP_BIKE_SPEED_SOURCE_OBD) && (obd_speed_valid != false)) ? 1000.0f :
        ((gnss_speed_valid != false) ? (1000.0f * BIKE_DYN_CalcGnssSpeedQuality01(fix, settings, now_ms)) : 0.0f);

    s_bike_runtime.lat_ref_quality_permille =
        (gnss_heading_valid != false) ? (1000.0f * BIKE_DYN_CalcGnssHeadingQuality01(fix, settings, now_ms)) :
        ((speed_source != (uint8_t)APP_BIKE_SPEED_SOURCE_IMU_FALLBACK) ? 600.0f : 0.0f);

    if (current_gnss_ms != 0u)
    {
        s_bike_runtime.last_gnss_fix_update_ms = current_gnss_ms;
    }
}

static void BIKE_DYN_UpdateMotionReferenceBlend(uint8_t speed_source,
                                                bool gnss_heading_valid,
                                                int32_t selected_speed_mmps)
{
    float current_speed_mps;
    float yaw_rate_up_rad_s;

    current_speed_mps = ((float)BIKE_DYN_SpeedAbsMmps(selected_speed_mmps)) * 0.001f;
    yaw_rate_up_rad_s = s_bike_runtime.yaw_rate_up_dps * BIKE_DYN_DEG2RAD;

    if ((speed_source != (uint8_t)APP_BIKE_SPEED_SOURCE_IMU_FALLBACK) &&
        (current_speed_mps > 0.5f))
    {
        s_bike_runtime.lat_ref_fast_g = BIKE_DYN_ClampF((current_speed_mps * yaw_rate_up_rad_s) / BIKE_DYN_GRAVITY_MPS2,
                                                        -BIKE_DYN_MAX_REFERENCE_ABS_G,
                                                         BIKE_DYN_MAX_REFERENCE_ABS_G);
    }
    else
    {
        s_bike_runtime.lat_ref_fast_g = 0.0f;
    }

    if (gnss_heading_valid != false)
    {
        /* ------------------------------------------------------------------ */
        /*  fast = gyro yaw-rate * speed                                       */
        /*  slow = GNSS heading derivative                                      */
        /*  - observer compensation 보다 fused reference 에서는 slow 비중을     */
        /*    조금 더 올려 drift 억제력을 확보한다.                             */
        /* ------------------------------------------------------------------ */
        s_bike_runtime.lat_ref_g = (0.70f * s_bike_runtime.lat_ref_fast_g) +
                                   (0.30f * s_bike_runtime.lat_ref_slow_g);
    }
    else
    {
        s_bike_runtime.lat_ref_g = s_bike_runtime.lat_ref_fast_g;
    }
}

/* -------------------------------------------------------------------------- */
/*  coordinated lean / online mount self-cal helpers                           */
/* -------------------------------------------------------------------------- */
static float BIKE_DYN_CalcCoordinatedBankBlend01(const app_bike_settings_t *settings,
                                                 uint8_t bank_calc_mode,
                                                 uint8_t speed_source,
                                                 int32_t selected_speed_mmps)
{
    float lat_abs_g;
    float lon_abs_g;
    float dyn01;
    float steady01;
    float quality01;
    float trust01;
    float speed01;
    float max_blend01;
    float min_trust_floor;

    if (settings == 0)
    {
        return 0.0f;
    }

    if ((bank_calc_mode == (uint8_t)APP_BIKE_BANK_CALC_MODE_IMU_ONLY) ||
        (speed_source == (uint8_t)APP_BIKE_SPEED_SOURCE_IMU_FALLBACK) ||
        (s_bike_runtime.zero_valid == false))
    {
        return 0.0f;
    }

    lat_abs_g = fabsf(s_bike_runtime.lat_ref_g);
    lon_abs_g = fabsf(s_bike_runtime.lon_ref_g);
    if (lat_abs_g < BIKE_DYN_COORD_MIN_LAT_G)
    {
        return 0.0f;
    }

    dyn01 = BIKE_DYN_Clamp01F((lat_abs_g - BIKE_DYN_COORD_MIN_LAT_G) /
                              BIKE_DYN_ClampF(BIKE_DYN_COORD_FULL_LAT_G - BIKE_DYN_COORD_MIN_LAT_G,
                                              0.05f,
                                              2.0f));
    steady01 = BIKE_DYN_Clamp01F((BIKE_DYN_COORD_MAX_LON_G - lon_abs_g) /
                                 BIKE_DYN_ClampF(BIKE_DYN_COORD_MAX_LON_G,
                                                 0.10f,
                                                 2.0f));
    quality01 = BIKE_DYN_Clamp01F(s_bike_runtime.lat_ref_quality_permille * 0.001f);

    min_trust_floor = (float)BIKE_DYN_ClampS32((int32_t)settings->imu_predict_min_trust_permille,
                                               150,
                                               700);
    trust01 = BIKE_DYN_Clamp01F((s_bike_runtime.last_attitude_trust_permille - min_trust_floor) /
                                BIKE_DYN_ClampF(1000.0f - min_trust_floor,
                                                100.0f,
                                                1000.0f));
    speed01 = BIKE_DYN_Clamp01F((((float)BIKE_DYN_SpeedAbsMmps(selected_speed_mmps)) - 2000.0f) / 10000.0f);

    max_blend01 = (bank_calc_mode == (uint8_t)APP_BIKE_BANK_CALC_MODE_FUSION) ?
                  BIKE_DYN_COORD_BLEND_MAX_FUSION :
                  BIKE_DYN_COORD_BLEND_MAX_SOURCE_ONLY;

    return max_blend01 * dyn01 * steady01 * quality01 * BIKE_DYN_ClampF(speed01 + 0.10f, 0.0f, 1.0f) *
           BIKE_DYN_ClampF(trust01 + 0.15f, 0.0f, 1.0f);
}

static void BIKE_DYN_UpdateOnlineMountSelfCalibration(const app_bike_settings_t *settings,
                                                      uint8_t bank_calc_mode,
                                                      uint8_t speed_source,
                                                      int32_t selected_speed_mmps)
{
    float ref_lon_g;
    float ref_lat_g;
    float ref_mag2_g2;
    float ref_mag_g;
    float residual_yaw_rad;
    float residual_yaw_deg;
    float innovation_deg;
    float dt_s;
    float max_step_deg;
    float quality01;
    float jerk_limit_mg_per_s;
    float trust_floor;

    if (settings == 0)
    {
        return;
    }

    if ((bank_calc_mode == (uint8_t)APP_BIKE_BANK_CALC_MODE_IMU_ONLY) ||
        (speed_source == (uint8_t)APP_BIKE_SPEED_SOURCE_IMU_FALLBACK) ||
        (s_bike_runtime.zero_valid == false) ||
        (s_bike_runtime.imu_sample_valid == false))
    {
        return;
    }

    if (BIKE_DYN_SpeedAbsMmps(selected_speed_mmps) < BIKE_DYN_MOUNT_SELF_CAL_MIN_SPEED_MMPS)
    {
        return;
    }

    trust_floor = (float)BIKE_DYN_ClampS32((int32_t)settings->imu_predict_min_trust_permille,
                                           150,
                                           700);
    if (s_bike_runtime.last_attitude_trust_permille < trust_floor)
    {
        return;
    }

    jerk_limit_mg_per_s = BIKE_DYN_ClampF(((float)settings->imu_jerk_gate_mg_per_s) * 0.70f,
                                          1000.0f,
                                          6000.0f);
    if (fabsf(s_bike_runtime.last_jerk_mg_per_s) > jerk_limit_mg_per_s)
    {
        return;
    }

    ref_lon_g = s_bike_runtime.lon_ref_g;
    ref_lat_g = s_bike_runtime.lat_ref_g;
    ref_mag2_g2 = (ref_lon_g * ref_lon_g) + (ref_lat_g * ref_lat_g);
    ref_mag_g = BIKE_DYN_SafeSqrtF(ref_mag2_g2);

    if ((ref_mag_g < BIKE_DYN_MOUNT_SELF_CAL_MIN_REF_G) ||
        (ref_mag_g > BIKE_DYN_MOUNT_SELF_CAL_MAX_REF_G))
    {
        return;
    }

    quality01 = BIKE_DYN_Clamp01F(BIKE_DYN_ClampF(
        (s_bike_runtime.lon_ref_quality_permille < s_bike_runtime.lat_ref_quality_permille)
            ? (s_bike_runtime.lon_ref_quality_permille * 0.001f)
            : (s_bike_runtime.lat_ref_quality_permille * 0.001f),
        0.0f,
        1.0f));

    if ((quality01 * 1000.0f) < BIKE_DYN_MOUNT_SELF_CAL_MIN_QUALITY)
    {
        return;
    }

    /* ------------------------------------------------------------------ */
    /*  small-angle yaw residual estimator                                   */
    /*                                                                      */
    /*  reference vector r = [lon_ref, lat_ref]                              */
    /*  measured  vector m = [lon_imu, lat_imu]                              */
    /*                                                                      */
    /*  m ≈ R(yaw_err) * r 일 때, 작은 각도 근사로                            */
    /*      yaw_err ≈ (r_x * m_y - r_y * m_x) / |r|^2                        */
    /*                                                                      */
    /*  여기서 얻은 residual yaw 를 반대 방향으로 천천히 적분해서             */
    /*  mounting yaw trim 을 추정한다.                                       */
    /* ------------------------------------------------------------------ */
    residual_yaw_rad = ((ref_lon_g * s_bike_runtime.lat_imu_g) -
                        (ref_lat_g * s_bike_runtime.lon_imu_g)) /
                       BIKE_DYN_ClampF(ref_mag2_g2, 0.020f, 4.000f);
    residual_yaw_deg = BIKE_DYN_ClampF(residual_yaw_rad * BIKE_DYN_RAD2DEG,
                                       -BIKE_DYN_MOUNT_SELF_CAL_MAX_ERR_DEG,
                                        BIKE_DYN_MOUNT_SELF_CAL_MAX_ERR_DEG);

    dt_s = BIKE_DYN_ClampF(s_bike_runtime.last_dt_s, BIKE_DYN_MIN_DT_S, BIKE_DYN_MAX_IMU_DT_S);
    max_step_deg = BIKE_DYN_MOUNT_SELF_CAL_RATE_DEG_PER_S * dt_s;

    innovation_deg = -residual_yaw_deg * quality01 * 0.08f * dt_s;
    innovation_deg = BIKE_DYN_ClampF(innovation_deg, -max_step_deg, max_step_deg);

    s_bike_runtime.mount_auto_yaw_deg = BIKE_DYN_ClampF(s_bike_runtime.mount_auto_yaw_deg + innovation_deg,
                                                        -BIKE_DYN_MOUNT_SELF_CAL_MAX_ABS_YAW_DEG,
                                                         BIKE_DYN_MOUNT_SELF_CAL_MAX_ABS_YAW_DEG);
}

static void BIKE_DYN_UpdateCoordinatedBankEstimate(const app_bike_settings_t *settings,
                                                   uint8_t bank_calc_mode,
                                                   uint8_t speed_source,
                                                   int32_t selected_speed_mmps)
{
    float dt_s;
    float blend01;
    float bank_target_deg;

    if (settings == 0)
    {
        return;
    }

    dt_s = BIKE_DYN_ClampF(s_bike_runtime.last_dt_s, BIKE_DYN_MIN_DT_S, BIKE_DYN_MAX_IMU_DT_S);
    s_bike_runtime.bank_coord_deg = atanf(BIKE_DYN_ClampF(s_bike_runtime.lat_ref_g,
                                                          -BIKE_DYN_MAX_DYNAMIC_COMP_ABS_G,
                                                           BIKE_DYN_MAX_DYNAMIC_COMP_ABS_G)) * BIKE_DYN_RAD2DEG;

    blend01 = BIKE_DYN_CalcCoordinatedBankBlend01(settings,
                                                  bank_calc_mode,
                                                  speed_source,
                                                  selected_speed_mmps);

    if ((bank_calc_mode == (uint8_t)APP_BIKE_BANK_CALC_MODE_IMU_ONLY) ||
        (speed_source == (uint8_t)APP_BIKE_SPEED_SOURCE_IMU_FALLBACK))
    {
        bank_target_deg = s_bike_runtime.bank_imu_deg;
        s_bike_runtime.bank_coord_deg = s_bike_runtime.bank_imu_deg;
    }
    else if (((bank_calc_mode == (uint8_t)APP_BIKE_BANK_CALC_MODE_OBD) ||
              (bank_calc_mode == (uint8_t)APP_BIKE_BANK_CALC_MODE_GNSS)) &&
             (fabsf(s_bike_runtime.lat_ref_g) >= BIKE_DYN_COORD_MIN_LAT_G) &&
             (s_bike_runtime.lat_ref_quality_permille >= 500.0f))
    {
        /* ------------------------------------------------------------------ */
        /*  dedicated backend mode                                              */
        /*                                                                      */
        /*  사용자가 OBD 또는 GNSS 전용 모드를 고르면                           */
        /*  coordinated pseudo bank 를 해당 backend의 대표값으로 간주한다.      */
        /*  즉, IMU bank 와 절충하지 않고 외부 reference가 충분할 때는          */
        /*  bank_coord_deg 를 canonical bank target으로 그대로 쓴다.           */
        /* ------------------------------------------------------------------ */
        bank_target_deg = s_bike_runtime.bank_coord_deg;
    }
    else
    {
        bank_target_deg = s_bike_runtime.bank_imu_deg +
                          (blend01 * (s_bike_runtime.bank_coord_deg - s_bike_runtime.bank_imu_deg));
    }

    s_bike_runtime.bank_raw_deg = bank_target_deg;
    s_bike_runtime.bank_display_deg = BIKE_DYN_LpfUpdate(s_bike_runtime.bank_display_deg,
                                                         s_bike_runtime.bank_raw_deg,
                                                         settings->lean_display_tau_ms,
                                                         dt_s);
}

/* -------------------------------------------------------------------------- */
/*  bias adaptation / fused output                                              */
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
    float max_bias_step_g;
    float bias_target_g;
    bool  allow_lon_ref;
    bool  allow_lat_ref;
    int32_t lat_bias_min_speed_mmps;
    uint8_t bank_calc_mode;

    if (settings == 0)
    {
        return;
    }

    bank_calc_mode = BIKE_DYN_GetBankCalcMode(settings);
    dt_s = BIKE_DYN_ClampF(s_bike_runtime.last_dt_s, BIKE_DYN_MIN_DT_S, BIKE_DYN_MAX_IMU_DT_S);
    outlier_gate_g = ((float)settings->gnss_outlier_gate_mg) * 0.001f;
    outlier_gate_g = BIKE_DYN_ClampF(outlier_gate_g, 0.120f, 0.800f);
    max_bias_step_g = BIKE_DYN_BIAS_RATE_LIMIT_G_PER_S * dt_s;

    allow_lon_ref = ((speed_source != (uint8_t)APP_BIKE_SPEED_SOURCE_IMU_FALLBACK) &&
                     (obd_speed_valid || gnss_speed_valid) &&
                     (s_bike_runtime.lon_ref_quality_permille >= 450.0f)) ? true : false;

    lat_bias_min_speed_mmps = BIKE_DYN_ClampS32((((int32_t)settings->gnss_min_speed_kmh_x10) * 1000) / 36,
                                                 1500,
                                                 20000);

    allow_lat_ref = ((speed_source != (uint8_t)APP_BIKE_SPEED_SOURCE_IMU_FALLBACK) &&
                     (BIKE_DYN_SpeedAbsMmps(selected_speed_mmps) >= lat_bias_min_speed_mmps) &&
                     (((gnss_heading_valid != false) &&
                       (s_bike_runtime.lat_ref_quality_permille >= 650.0f)) ||
                      ((bank_calc_mode == (uint8_t)APP_BIKE_BANK_CALC_MODE_OBD) &&
                       (speed_source == (uint8_t)APP_BIKE_SPEED_SOURCE_OBD) &&
                       (s_bike_runtime.lat_ref_quality_permille >= 600.0f) &&
                       (fabsf(s_bike_runtime.lon_ref_g) <= 0.20f)))) ? true : false;

    if ((s_bike_runtime.last_attitude_trust_permille >= (float)settings->imu_predict_min_trust_permille) &&
        (allow_lon_ref != false))
    {
        lon_err_g = s_bike_runtime.lon_imu_g - s_bike_runtime.lon_ref_g;
        if (fabsf(lon_err_g) <= outlier_gate_g)
        {
            bias_target_g = BIKE_DYN_LpfUpdate(s_bike_runtime.lon_bias_g,
                                               lon_err_g,
                                               settings->gnss_bias_tau_ms,
                                               dt_s);
            s_bike_runtime.lon_bias_g = BIKE_DYN_LimitDeltaF(s_bike_runtime.lon_bias_g,
                                                             bias_target_g,
                                                             max_bias_step_g);
        }
    }

    if ((s_bike_runtime.last_attitude_trust_permille >= (float)settings->imu_predict_min_trust_permille) &&
        (allow_lat_ref != false))
    {
        lat_err_g = s_bike_runtime.lat_imu_g - s_bike_runtime.lat_ref_g;
        if (fabsf(lat_err_g) <= outlier_gate_g)
        {
            bias_target_g = BIKE_DYN_LpfUpdate(s_bike_runtime.lat_bias_g,
                                               lat_err_g,
                                               settings->gnss_bias_tau_ms,
                                               dt_s);
            s_bike_runtime.lat_bias_g = BIKE_DYN_LimitDeltaF(s_bike_runtime.lat_bias_g,
                                                             bias_target_g,
                                                             max_bias_step_g);
        }
    }

    s_bike_runtime.lon_fused_g = BIKE_DYN_LpfUpdate(
        s_bike_runtime.lon_fused_g,
        BIKE_DYN_DeadbandAndClipG(s_bike_runtime.lon_imu_g - s_bike_runtime.lon_bias_g,
                                  settings->output_deadband_mg,
                                  settings->output_clip_mg),
        settings->accel_display_tau_ms,
        dt_s);

    s_bike_runtime.lat_fused_g = BIKE_DYN_LpfUpdate(
        s_bike_runtime.lat_fused_g,
        BIKE_DYN_DeadbandAndClipG(s_bike_runtime.lat_imu_g - s_bike_runtime.lat_bias_g,
                                  settings->output_deadband_mg,
                                  settings->output_clip_mg),
        settings->accel_display_tau_ms,
        dt_s);

    BIKE_DYN_UpdateOnlineMountSelfCalibration(settings,
                                              bank_calc_mode,
                                              speed_source,
                                              selected_speed_mmps);
    BIKE_DYN_UpdateCoordinatedBankEstimate(settings,
                                           bank_calc_mode,
                                           speed_source,
                                           selected_speed_mmps);
}

static void BIKE_DYN_UpdateProvisionalDisplayOutputs(const app_bike_settings_t *settings)
{
    (void)settings;

    /* ---------------------------------------------------------------------- */
    /*  pre-zero rider-facing output lock                                      */
    /*                                                                        */
    /*  현재 정책은 명확하다.                                                   */
    /*  - zero_valid 전에는 display lean / grade / lat-G / lon-G를            */
    /*    사용자 출력층에 절대 노출하지 않는다.                                */
    /*  - 내부 IMU projection은 BIKE_DYN_UpdateOutputsFromCurrentImu()에서      */
    /*    계속 유지하므로, zero capture 성공 직후 bias 재기준화에는 문제가     */
    /*    없다.                                                                */
    /*                                                                        */
    /*  즉, 이 함수의 역할은 "임시 표시값 생성" 이 아니라                     */
    /*  "임시 표시값을 0으로 봉인" 하는 것이다.                               */
    /* ---------------------------------------------------------------------- */
    s_bike_runtime.lon_fused_g = 0.0f;
    s_bike_runtime.lat_fused_g = 0.0f;
}

/* -------------------------------------------------------------------------- */
/*  gyro bias calibration state machine                                         */
/* -------------------------------------------------------------------------- */
static void BIKE_DYN_ProcessGyroBiasCalibration(const app_bike_settings_t *settings,
                                                int32_t selected_speed_mmps,
                                                uint8_t speed_source,
                                                bool imu_new_sample,
                                                uint32_t now_ms)
{
    uint32_t sample_dt_ms;

    (void)selected_speed_mmps;
    (void)speed_source;

    if (settings == 0)
    {
        return;
    }

    if ((s_bike_runtime.gyro_bias_cal_requested != false) &&
        (s_bike_runtime.gyro_bias_cal_active == false))
    {
        s_bike_runtime.gyro_bias_cal_requested       = false;
        s_bike_runtime.gyro_bias_cal_active          = true;
        s_bike_runtime.gyro_bias_cal_last_success    = false;
        s_bike_runtime.gyro_bias_cal_start_ms        = now_ms;
        s_bike_runtime.gyro_bias_cal_settle_until_ms = now_ms + BIKE_DYN_GYRO_CAL_SETTLE_MS;
        s_bike_runtime.gyro_bias_cal_good_ms         = 0u;
        s_bike_runtime.gyro_bias_cal_sample_count    = 0u;
        s_bike_runtime.gyro_bias_sum_x_dps           = 0.0f;
        s_bike_runtime.gyro_bias_sum_y_dps           = 0.0f;
        s_bike_runtime.gyro_bias_sum_z_dps           = 0.0f;
    }

    if (s_bike_runtime.gyro_bias_cal_active == false)
    {
        return;
    }

    if ((now_ms - s_bike_runtime.gyro_bias_cal_start_ms) > BIKE_DYN_GYRO_CAL_TIMEOUT_MS)
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

    if (now_ms < s_bike_runtime.gyro_bias_cal_settle_until_ms)
    {
        return;
    }

    if (BIKE_DYN_IsGyroCalibrationSampleGood(settings, selected_speed_mmps, speed_source) == false)
    {
        s_bike_runtime.gyro_bias_cal_good_ms      = 0u;
        s_bike_runtime.gyro_bias_cal_sample_count = 0u;
        s_bike_runtime.gyro_bias_sum_x_dps        = 0.0f;
        s_bike_runtime.gyro_bias_sum_y_dps        = 0.0f;
        s_bike_runtime.gyro_bias_sum_z_dps        = 0.0f;
        return;
    }

    sample_dt_ms = BIKE_DYN_ClampU32((uint32_t)BIKE_DYN_RoundFloatToS32(s_bike_runtime.last_dt_s * 1000.0f), 1u, 100u);

    s_bike_runtime.gyro_bias_cal_good_ms      += sample_dt_ms;
    s_bike_runtime.gyro_bias_cal_sample_count += 1u;
    s_bike_runtime.gyro_bias_sum_x_dps        += s_bike_runtime.last_gx_raw_dps;
    s_bike_runtime.gyro_bias_sum_y_dps        += s_bike_runtime.last_gy_raw_dps;
    s_bike_runtime.gyro_bias_sum_z_dps        += s_bike_runtime.last_gz_raw_dps;

    if ((s_bike_runtime.gyro_bias_cal_good_ms >= BIKE_DYN_GYRO_CAL_TARGET_GOOD_MS) &&
        (s_bike_runtime.gyro_bias_cal_sample_count >= BIKE_DYN_GYRO_CAL_MIN_SAMPLES))
    {
        s_bike_runtime.gyro_bias_x_dps = s_bike_runtime.gyro_bias_sum_x_dps /
                                         (float)s_bike_runtime.gyro_bias_cal_sample_count;
        s_bike_runtime.gyro_bias_y_dps = s_bike_runtime.gyro_bias_sum_y_dps /
                                         (float)s_bike_runtime.gyro_bias_cal_sample_count;
        s_bike_runtime.gyro_bias_z_dps = s_bike_runtime.gyro_bias_sum_z_dps /
                                         (float)s_bike_runtime.gyro_bias_cal_sample_count;

        s_bike_runtime.gyro_bias_valid             = true;
        s_bike_runtime.gyro_bias_cal_active        = false;
        s_bike_runtime.gyro_bias_cal_last_success  = true;
        s_bike_runtime.gyro_bias_cal_success_count += 1u;
        s_bike_runtime.last_gyro_bias_cal_ms       = now_ms;
    }
}

/* -------------------------------------------------------------------------- */
/*  zero capture state machine                                                  */
/* -------------------------------------------------------------------------- */
static void BIKE_DYN_ProcessZeroCapture(const app_bike_settings_t *settings,
                                        int32_t selected_speed_mmps,
                                        uint8_t speed_source,
                                        bool imu_new_sample,
                                        uint32_t now_ms)
{
    bool auto_zero_pending;
    bool manual_zero_pending;
    uint32_t sample_dt_ms;

    if (settings == 0)
    {
        return;
    }

    auto_zero_pending = ((settings->auto_zero_on_boot != 0u) &&
                         (s_bike_runtime.auto_zero_done == false)) ? true : false;
    manual_zero_pending = (s_bike_runtime.zero_requested != false) ? true : false;

    if ((auto_zero_pending == false) && (manual_zero_pending == false))
    {
        s_bike_runtime.zero_capture_start_ms = 0u;
        s_bike_runtime.zero_capture_good_ms = 0u;
        s_bike_runtime.zero_capture_sample_count = 0u;
        return;
    }

    if (s_bike_runtime.zero_capture_start_ms == 0u)
    {
        s_bike_runtime.zero_capture_start_ms = now_ms;
    }

    if ((s_bike_runtime.gyro_bias_cal_active != false) ||
        (s_bike_runtime.imu_sample_valid == false) ||
        (imu_new_sample == false))
    {
        return;
    }

    if (now_ms < s_bike_runtime.zero_capture_settle_until_ms)
    {
        if ((now_ms - s_bike_runtime.zero_capture_start_ms) > BIKE_DYN_ZERO_CAPTURE_TIMEOUT_MS)
        {
            s_bike_runtime.zero_capture_start_ms = now_ms;
            s_bike_runtime.zero_capture_settle_until_ms = now_ms + BIKE_DYN_ZERO_CAPTURE_RESTART_MS;
            s_bike_runtime.zero_capture_good_ms = 0u;
            s_bike_runtime.zero_capture_sample_count = 0u;
        }
        return;
    }

    sample_dt_ms = BIKE_DYN_ClampU32((uint32_t)BIKE_DYN_RoundFloatToS32(s_bike_runtime.last_dt_s * 1000.0f),
                                     1u,
                                     100u);

    if (BIKE_DYN_IsZeroCaptureSampleGood(settings, selected_speed_mmps, speed_source) == false)
    {
        /* ------------------------------------------------------------------ */
        /*  single bad sample hard-reset 제거                                   */
        /*                                                                      */
        /*  기존 구현은 조건을 한 번만 벗어나도 누적 시간을 전부 0으로 날려서    */
        /*  미세 진동 환경에서 사실상 완료가 불가능해질 수 있었다.               */
        /*                                                                      */
        /*  여기서는 progress를 서서히 감쇠시키고, 너무 오래 진전이 없을 때만    */
        /*  settle window를 다시 시작한다.                                      */
        /* ------------------------------------------------------------------ */
        if (s_bike_runtime.zero_capture_good_ms > sample_dt_ms)
        {
            s_bike_runtime.zero_capture_good_ms -= sample_dt_ms;
        }
        else
        {
            s_bike_runtime.zero_capture_good_ms = 0u;
        }

        if (s_bike_runtime.zero_capture_sample_count > 0u)
        {
            s_bike_runtime.zero_capture_sample_count -= 1u;
        }

        if ((now_ms - s_bike_runtime.zero_capture_start_ms) > BIKE_DYN_ZERO_CAPTURE_TIMEOUT_MS)
        {
            s_bike_runtime.zero_capture_start_ms = now_ms;
            s_bike_runtime.zero_capture_settle_until_ms = now_ms + BIKE_DYN_ZERO_CAPTURE_RESTART_MS;
            s_bike_runtime.zero_capture_good_ms = 0u;
            s_bike_runtime.zero_capture_sample_count = 0u;
        }
        return;
    }

    (void)BIKE_DYN_ForceGravityFromStaticAccel(settings);

    s_bike_runtime.zero_capture_good_ms += sample_dt_ms;
    s_bike_runtime.zero_capture_sample_count += 1u;

    if ((s_bike_runtime.zero_capture_good_ms >= BIKE_DYN_ZERO_CAPTURE_TARGET_GOOD_MS) &&
        (s_bike_runtime.zero_capture_sample_count >= BIKE_DYN_ZERO_CAPTURE_MIN_SAMPLES))
    {
        if (BIKE_DYN_RebuildZeroBasis(settings) != false)
        {
            s_bike_runtime.auto_zero_done = true;
            s_bike_runtime.zero_requested = false;
            s_bike_runtime.zero_capture_start_ms = 0u;
        }
        else
        {
            s_bike_runtime.zero_capture_start_ms = now_ms;
            s_bike_runtime.zero_capture_settle_until_ms = now_ms + BIKE_DYN_ZERO_CAPTURE_RESTART_MS;
            s_bike_runtime.zero_capture_good_ms = 0u;
            s_bike_runtime.zero_capture_sample_count = 0u;
        }
    }
}


/* -------------------------------------------------------------------------- */
/*  heading output                                                              */
/* -------------------------------------------------------------------------- */
static void BIKE_DYN_UpdateHeadingOutput(bool gnss_heading_valid)
{
    s_bike_runtime.mag_heading_valid = false;
    s_bike_runtime.mag_heading_deg   = 0.0f;

    if (gnss_heading_valid != false)
    {
        s_bike_runtime.heading_valid  = true;
        s_bike_runtime.heading_source = (uint8_t)APP_BIKE_HEADING_SOURCE_GNSS;
        s_bike_runtime.heading_deg    = BIKE_DYN_WrapDeg360(((float)g_app_state.gps.fix.headMot) * 0.00001f);
    }
    else
    {
        s_bike_runtime.heading_valid  = false;
        s_bike_runtime.heading_source = (uint8_t)APP_BIKE_HEADING_SOURCE_NONE;
        s_bike_runtime.heading_deg    = 0.0f;
    }
}

/* -------------------------------------------------------------------------- */
/*  publish                                                                     */
/* -------------------------------------------------------------------------- */
static void BIKE_DYN_PublishState(uint32_t now_ms,
                                  const app_bike_settings_t *settings,
                                  uint8_t speed_source,
                                  bool gnss_speed_valid,
                                  bool gnss_heading_valid,
                                  bool obd_speed_valid,
                                  int32_t selected_speed_mmps)
{
    app_bike_state_t *bike;
    uint16_t attitude_conf;
    uint16_t aid_conf;
    uint16_t zero_conf;
    uint16_t gyro_conf;
    uint16_t final_conf;
    uint8_t  bank_calc_mode;
    float    hint_fwd_x;
    float    hint_fwd_y;
    float    hint_fwd_z;
    float    hint_left_x;
    float    hint_left_y;
    float    hint_left_z;
    float    hint_up_x;
    float    hint_up_y;
    float    hint_up_z;

    bike = (app_bike_state_t *)&g_app_state.bike;
    bank_calc_mode = BIKE_DYN_GetBankCalcMode(settings);

    attitude_conf = (uint16_t)BIKE_DYN_ClampS32(BIKE_DYN_RoundFloatToS32(s_bike_runtime.last_attitude_trust_permille), 0, 1000);
    zero_conf     = (s_bike_runtime.zero_valid != false) ? 1000u : 0u;
    gyro_conf     = (s_bike_runtime.gyro_bias_valid != false) ? 900u : 650u;

    if (speed_source == (uint8_t)APP_BIKE_SPEED_SOURCE_OBD)
    {
        aid_conf = 900u;
    }
    else if (speed_source == (uint8_t)APP_BIKE_SPEED_SOURCE_GNSS)
    {
        aid_conf = (uint16_t)BIKE_DYN_ClampS32(BIKE_DYN_RoundFloatToS32(
                        BIKE_DYN_CalcGnssSpeedQuality01((const gps_fix_basic_t *)&g_app_state.gps.fix,
                                                        settings,
                                                        now_ms) * 1000.0f), 0, 1000);
    }
    else
    {
        aid_conf = 500u;
    }

    if (s_bike_runtime.gravity_valid == false)
    {
        final_conf = 0u;
    }
    else
    {
        final_conf = (uint16_t)BIKE_DYN_ClampS32(
            BIKE_DYN_RoundFloatToS32((0.50f * (float)attitude_conf) +
                                     (0.20f * (float)zero_conf) +
                                     (0.15f * (float)gyro_conf) +
                                     (0.15f * (float)aid_conf)),
            0,
            1000);

        if (s_bike_runtime.zero_valid == false)
        {
            final_conf = (uint16_t)BIKE_DYN_ClampS32((int32_t)final_conf, 0, 350);
        }
    }

    bike->initialized         = s_bike_runtime.initialized;
    bike->zero_valid          = s_bike_runtime.zero_valid;
    bike->imu_valid           = s_bike_runtime.gravity_valid;
    bike->gnss_aid_valid      = (uint8_t)((gnss_speed_valid || gnss_heading_valid) ? 1u : 0u);
    bike->gnss_heading_valid  = (uint8_t)(gnss_heading_valid ? 1u : 0u);
    bike->obd_speed_valid     = (uint8_t)(obd_speed_valid ? 1u : 0u);
    bike->speed_source        = speed_source;
    bike->estimator_mode      =
        (speed_source == (uint8_t)APP_BIKE_SPEED_SOURCE_IMU_FALLBACK) ? (uint8_t)APP_BIKE_ESTIMATOR_MODE_IMU_ONLY :
        (bank_calc_mode == (uint8_t)APP_BIKE_BANK_CALC_MODE_FUSION)   ? (uint8_t)APP_BIKE_ESTIMATOR_MODE_FUSION   :
        (bank_calc_mode == (uint8_t)APP_BIKE_BANK_CALC_MODE_OBD)      ? (uint8_t)APP_BIKE_ESTIMATOR_MODE_OBD_AIDED:
                                                                        (uint8_t)APP_BIKE_ESTIMATOR_MODE_GNSS_AIDED;
    bike->confidence_permille = final_conf;

    bike->last_update_ms       = now_ms;
    bike->last_imu_update_ms   = s_bike_runtime.last_imu_timestamp_ms;
    bike->last_zero_capture_ms = s_bike_runtime.last_zero_capture_ms;
    bike->last_gnss_aid_ms     = s_bike_runtime.last_gnss_fix_update_ms;

    /* ------------------------------------------------------------------ */
    /*  canonical telemetry와 rider-facing display 출력을 함께 publish 한다. */
    /*                                                                    */
    /*  이유                                                               */
    /*  - logger / peak detector / replay tool 은                         */
    /*    pre-display smoothing 값을 읽어야 응답이 둔화되지 않는다.       */
    /*  - live UI 는 display tau가 적용된 부드러운 값을 계속 사용한다.    */
    /* ------------------------------------------------------------------ */
    if (s_bike_runtime.zero_valid != false)
    {
        /* ------------------------------------------------------------------ */
        /*  canonical estimator output 는 zero_valid 이후부터만 연다.           */
        /*                                                                    */
        /*  이렇게 해야 display-only provisional 값이 peak/log 경로로            */
        /*  오염되지 않는다.                                                    */
        /* ------------------------------------------------------------------ */
        bike->bank_raw_deg_x10 = BIKE_DYN_RoundFloatToS16X10(s_bike_runtime.bank_raw_deg * 10.0f);
        bike->grade_raw_deg_x10 = BIKE_DYN_RoundFloatToS16X10(s_bike_runtime.grade_raw_deg * 10.0f);
        bike->lat_accel_est_mg = BIKE_DYN_RoundFloatToS32(BIKE_DYN_DeadbandAndClipG(s_bike_runtime.lat_imu_g - s_bike_runtime.lat_bias_g,
                                                                                     settings->output_deadband_mg,
                                                                                     settings->output_clip_mg) * 1000.0f);
        bike->lon_accel_est_mg = BIKE_DYN_RoundFloatToS32(BIKE_DYN_DeadbandAndClipG(s_bike_runtime.lon_imu_g - s_bike_runtime.lon_bias_g,
                                                                                     settings->output_deadband_mg,
                                                                                     settings->output_clip_mg) * 1000.0f);
    }
    else
    {
        bike->bank_raw_deg_x10 = 0;
        bike->grade_raw_deg_x10 = 0;
        bike->lat_accel_est_mg = 0;
        bike->lon_accel_est_mg = 0;
    }

    if (s_bike_runtime.zero_valid != false)
    {
        /* ------------------------------------------------------------------ */
        /*  rider-facing display output 역시 zero_valid 이후에만 연다.          */
        /*                                                                    */
        /*  이는 canonical est_* 경로뿐 아니라 live UI 경로도 동일 계약으로    */
        /*  묶어서, 부팅 직후 provisional 값이 bank/LatG 화면에 노출되는 일을  */
        /*  구조적으로 차단하기 위함이다.                                       */
        /* ------------------------------------------------------------------ */
        bike->banking_angle_deg_x10     = BIKE_DYN_RoundFloatToS16X10(s_bike_runtime.bank_display_deg * 10.0f);
        bike->banking_angle_display_deg = BIKE_DYN_RoundFloatToS16X10(s_bike_runtime.bank_display_deg);
        bike->grade_deg_x10             = BIKE_DYN_RoundFloatToS16X10(s_bike_runtime.grade_display_deg * 10.0f);
        bike->grade_display_deg         = BIKE_DYN_RoundFloatToS16X10(s_bike_runtime.grade_display_deg);

        bike->bank_rate_dps_x10  = BIKE_DYN_RoundFloatToS16X10(s_bike_runtime.bank_rate_dps * 10.0f);
        bike->grade_rate_dps_x10 = BIKE_DYN_RoundFloatToS16X10(s_bike_runtime.grade_rate_dps * 10.0f);

        bike->lat_accel_mg   = BIKE_DYN_RoundFloatToS32(s_bike_runtime.lat_fused_g * 1000.0f);
        bike->lon_accel_mg   = BIKE_DYN_RoundFloatToS32(s_bike_runtime.lon_fused_g * 1000.0f);
        bike->lat_accel_cms2 = BIKE_DYN_RoundFloatToS32(s_bike_runtime.lat_fused_g * BIKE_DYN_GRAVITY_CMS2);
        bike->lon_accel_cms2 = BIKE_DYN_RoundFloatToS32(s_bike_runtime.lon_fused_g * BIKE_DYN_GRAVITY_CMS2);
    }
    else
    {
        bike->banking_angle_deg_x10     = 0;
        bike->banking_angle_display_deg = 0;
        bike->grade_deg_x10             = 0;
        bike->grade_display_deg         = 0;
        bike->bank_rate_dps_x10         = 0;
        bike->grade_rate_dps_x10        = 0;
        bike->lat_accel_mg              = 0;
        bike->lon_accel_mg              = 0;
        bike->lat_accel_cms2            = 0;
        bike->lon_accel_cms2            = 0;
    }

    bike->lat_accel_imu_mg = BIKE_DYN_RoundFloatToS32(s_bike_runtime.lat_imu_g * 1000.0f);
    bike->lon_accel_imu_mg = BIKE_DYN_RoundFloatToS32(s_bike_runtime.lon_imu_g * 1000.0f);
    bike->lat_accel_ref_mg = BIKE_DYN_RoundFloatToS32(s_bike_runtime.lat_ref_g * 1000.0f);
    bike->lon_accel_ref_mg = BIKE_DYN_RoundFloatToS32(s_bike_runtime.lon_ref_g * 1000.0f);

    bike->lat_bias_mg = BIKE_DYN_RoundFloatToS32(s_bike_runtime.lat_bias_g * 1000.0f);
    bike->lon_bias_mg = BIKE_DYN_RoundFloatToS32(s_bike_runtime.lon_bias_g * 1000.0f);

    bike->imu_accel_norm_mg           = BIKE_DYN_RoundFloatToS32(s_bike_runtime.last_accel_norm_mg);
    bike->imu_jerk_mg_per_s           = BIKE_DYN_RoundFloatToS32(s_bike_runtime.last_jerk_mg_per_s);
    bike->imu_attitude_trust_permille = attitude_conf;

    if ((s_bike_runtime.zero_valid != false) &&
        (s_bike_runtime.gravity_valid != false) &&
        (BIKE_DYN_GetCorrectedZeroHints(&hint_fwd_x,
                                        &hint_fwd_y,
                                        &hint_fwd_z,
                                        &hint_left_x,
                                        &hint_left_y,
                                        &hint_left_z,
                                        &hint_up_x,
                                        &hint_up_y,
                                        &hint_up_z) != false))
    {
        bike->up_bx_milli = BIKE_DYN_RoundFloatToS32(
            BIKE_DYN_Dot3(hint_fwd_x,  hint_fwd_y,  hint_fwd_z,
                          s_bike_runtime.gravity_est_x_s, s_bike_runtime.gravity_est_y_s, s_bike_runtime.gravity_est_z_s) * 1000.0f);
        bike->up_by_milli = BIKE_DYN_RoundFloatToS32(
            BIKE_DYN_Dot3(hint_left_x, hint_left_y, hint_left_z,
                          s_bike_runtime.gravity_est_x_s, s_bike_runtime.gravity_est_y_s, s_bike_runtime.gravity_est_z_s) * 1000.0f);
        bike->up_bz_milli = BIKE_DYN_RoundFloatToS32(
            BIKE_DYN_Dot3(hint_up_x,   hint_up_y,   hint_up_z,
                          s_bike_runtime.gravity_est_x_s, s_bike_runtime.gravity_est_y_s, s_bike_runtime.gravity_est_z_s) * 1000.0f);
    }
    else
    {
        bike->up_bx_milli = 0;
        bike->up_by_milli = 0;
        bike->up_bz_milli = 0;
    }

    bike->speed_mmps             = selected_speed_mmps;
    bike->speed_kmh_x10          = (uint16_t)BIKE_DYN_ClampS32(
        BIKE_DYN_RoundFloatToS32(((float)BIKE_DYN_SpeedAbsMmps(selected_speed_mmps)) * 0.036f), 0, 65535);
    bike->gnss_speed_acc_kmh_x10 = (uint16_t)BIKE_DYN_ClampS32(
        BIKE_DYN_RoundFloatToS32(((float)g_app_state.gps.fix.sAcc) * 0.036f), 0, 65535);
    bike->gnss_head_acc_deg_x10  = (uint16_t)BIKE_DYN_ClampS32(
        BIKE_DYN_RoundFloatToS32(((float)g_app_state.gps.fix.headAcc) * 0.0001f), 0, 65535);
    bike->mount_yaw_trim_deg_x10 = (settings != 0)
                                 ? (int16_t)((int32_t)settings->mount_yaw_trim_deg_x10 + (int32_t)BIKE_DYN_GetRuntimeAutoYawTrimDegX10())
                                 : BIKE_DYN_GetRuntimeAutoYawTrimDegX10();

    bike->gnss_fix_ok     = (uint8_t)(g_app_state.gps.fix.fixOk ? 1u : 0u);
    bike->gnss_numsv_used = g_app_state.gps.fix.numSV_used;
    bike->gnss_pdop_x100  = g_app_state.gps.fix.pDOP;

    bike->heading_valid       = s_bike_runtime.heading_valid;
    bike->mag_heading_valid   = false;
    bike->heading_source      = s_bike_runtime.heading_source;
    bike->reserved_heading0   = 0u;
    bike->heading_deg_x10     = BIKE_DYN_RoundFloatToS16X10(s_bike_runtime.heading_deg * 10.0f);
    bike->mag_heading_deg_x10 = 0;

    bike->gyro_bias_cal_active          = s_bike_runtime.gyro_bias_cal_active;
    bike->gyro_bias_valid               = s_bike_runtime.gyro_bias_valid;
    bike->gyro_bias_cal_last_success    = s_bike_runtime.gyro_bias_cal_last_success;
    bike->reserved_gyro_bias0           = 0u;
    bike->gyro_bias_cal_progress_permille = (uint16_t)BIKE_DYN_ClampS32(
        BIKE_DYN_RoundFloatToS32(((float)s_bike_runtime.gyro_bias_cal_good_ms * 1000.0f) /
                                 (float)BIKE_DYN_ClampS32((int32_t)BIKE_DYN_GYRO_CAL_TARGET_GOOD_MS, 1, 60000)),
        0,
        1000);
    bike->last_gyro_bias_cal_ms = s_bike_runtime.last_gyro_bias_cal_ms;
    bike->gyro_bias_cal_count   = s_bike_runtime.gyro_bias_cal_success_count;
    bike->gyro_bias_x_dps_x100  = BIKE_DYN_RoundFloatToS16X10(s_bike_runtime.gyro_bias_x_dps * 100.0f);
    bike->gyro_bias_y_dps_x100  = BIKE_DYN_RoundFloatToS16X10(s_bike_runtime.gyro_bias_y_dps * 100.0f);
    bike->gyro_bias_z_dps_x100  = BIKE_DYN_RoundFloatToS16X10(s_bike_runtime.gyro_bias_z_dps * 100.0f);
    bike->yaw_rate_dps_x10      = BIKE_DYN_RoundFloatToS16X10(s_bike_runtime.yaw_rate_up_dps * 10.0f);
}

/* -------------------------------------------------------------------------- */
/*  runtime reset                                                               */
/* -------------------------------------------------------------------------- */
static void BIKE_DYN_ResetRuntime(uint32_t now_ms)
{
    bool     keep_gyro_bias_valid;
    float    keep_gyro_bias_x_dps;
    float    keep_gyro_bias_y_dps;
    float    keep_gyro_bias_z_dps;
    float    keep_mount_auto_yaw_deg;
    uint32_t keep_gyro_bias_cal_success_count;
    uint32_t keep_last_gyro_bias_cal_ms;
    bool     keep_gyro_bias_cal_last_success;

    keep_gyro_bias_valid              = s_bike_runtime.gyro_bias_valid;
    keep_gyro_bias_x_dps              = s_bike_runtime.gyro_bias_x_dps;
    keep_gyro_bias_y_dps              = s_bike_runtime.gyro_bias_y_dps;
    keep_gyro_bias_z_dps              = s_bike_runtime.gyro_bias_z_dps;
    keep_mount_auto_yaw_deg           = s_bike_runtime.mount_auto_yaw_deg;
    keep_gyro_bias_cal_success_count  = s_bike_runtime.gyro_bias_cal_success_count;
    keep_last_gyro_bias_cal_ms        = s_bike_runtime.last_gyro_bias_cal_ms;
    keep_gyro_bias_cal_last_success   = s_bike_runtime.gyro_bias_cal_last_success;

    memset(&s_bike_runtime, 0, sizeof(s_bike_runtime));

    s_bike_runtime.initialized                  = true;
    s_bike_runtime.init_ms                      = now_ms;
    s_bike_runtime.last_task_ms                 = now_ms;
    s_bike_runtime.zero_capture_settle_until_ms = now_ms + BIKE_DYN_ZERO_CAPTURE_SETTLE_MS;
    s_bike_runtime.zero_capture_start_ms        = now_ms;

    s_bike_runtime.gyro_bias_valid            = keep_gyro_bias_valid;
    s_bike_runtime.gyro_bias_x_dps            = keep_gyro_bias_x_dps;
    s_bike_runtime.gyro_bias_y_dps            = keep_gyro_bias_y_dps;
    s_bike_runtime.gyro_bias_z_dps            = keep_gyro_bias_z_dps;
    s_bike_runtime.mount_auto_yaw_deg          = keep_mount_auto_yaw_deg;
    s_bike_runtime.gyro_bias_cal_success_count = keep_gyro_bias_cal_success_count;
    s_bike_runtime.last_gyro_bias_cal_ms      = keep_last_gyro_bias_cal_ms;
    s_bike_runtime.gyro_bias_cal_last_success = keep_gyro_bias_cal_last_success;
}

/* -------------------------------------------------------------------------- */
/*  public API                                                                  */
/* -------------------------------------------------------------------------- */
void BIKE_DYNAMICS_Init(uint32_t now_ms)
{
    BIKE_DYN_ResetRuntime(now_ms);

    g_app_state.bike.initialized          = true;
    g_app_state.bike.last_update_ms       = now_ms;
    g_app_state.bike.last_imu_update_ms   = 0u;
    g_app_state.bike.last_zero_capture_ms = 0u;
    g_app_state.bike.last_gnss_aid_ms     = 0u;
    g_app_state.bike.bank_raw_deg_x10     = 0;
    g_app_state.bike.grade_raw_deg_x10    = 0;
    g_app_state.bike.lat_accel_est_mg     = 0;
    g_app_state.bike.lon_accel_est_mg     = 0;
}

void BIKE_DYNAMICS_RequestZeroCapture(void)
{
    s_bike_runtime.zero_requested               = true;
    s_bike_runtime.zero_capture_start_ms        = s_bike_runtime.last_task_ms;
    s_bike_runtime.zero_capture_good_ms         = 0u;
    s_bike_runtime.zero_capture_sample_count    = 0u;
    s_bike_runtime.zero_capture_settle_until_ms = s_bike_runtime.last_task_ms + BIKE_DYN_ZERO_CAPTURE_SETTLE_MS;
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
    bool raw_gnss_speed_good;
    bool raw_gnss_heading_good;
    bool gnss_speed_valid_raw;
    bool gnss_heading_valid_raw;
    bool gnss_speed_valid_est;
    bool gnss_heading_valid_est;
    bool obd_speed_valid_est;
    bool imu_new_sample;
    uint8_t bank_calc_mode;
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
    bank_calc_mode = BIKE_DYN_GetBankCalcMode(settings);

    if (settings->enabled == 0u)
    {
        g_app_state.bike.initialized               = true;
        g_app_state.bike.imu_valid                 = false;
        g_app_state.bike.zero_valid                = false;
        g_app_state.bike.gnss_aid_valid            = 0u;
        g_app_state.bike.gnss_heading_valid        = 0u;
        g_app_state.bike.obd_speed_valid           = 0u;
        g_app_state.bike.heading_valid             = false;
        g_app_state.bike.mag_heading_valid         = false;
        g_app_state.bike.heading_source            = (uint8_t)APP_BIKE_HEADING_SOURCE_NONE;
        g_app_state.bike.speed_source              = (uint8_t)APP_BIKE_SPEED_SOURCE_IMU_FALLBACK;
        g_app_state.bike.estimator_mode            = (uint8_t)APP_BIKE_ESTIMATOR_MODE_IMU_ONLY;
        g_app_state.bike.confidence_permille       = 0u;
        g_app_state.bike.heading_deg_x10           = 0;
        g_app_state.bike.mag_heading_deg_x10       = 0;
        g_app_state.bike.bank_raw_deg_x10          = 0;
        g_app_state.bike.grade_raw_deg_x10         = 0;
        g_app_state.bike.banking_angle_deg_x10     = 0;
        g_app_state.bike.banking_angle_display_deg = 0;
        g_app_state.bike.grade_deg_x10             = 0;
        g_app_state.bike.grade_display_deg         = 0;
        g_app_state.bike.bank_rate_dps_x10         = 0;
        g_app_state.bike.grade_rate_dps_x10        = 0;
        g_app_state.bike.lat_accel_est_mg          = 0;
        g_app_state.bike.lon_accel_est_mg          = 0;
        g_app_state.bike.lat_accel_mg              = 0;
        g_app_state.bike.lon_accel_mg              = 0;
        g_app_state.bike.lat_accel_cms2            = 0;
        g_app_state.bike.lon_accel_cms2            = 0;
        g_app_state.bike.lat_accel_imu_mg          = 0;
        g_app_state.bike.lon_accel_imu_mg          = 0;
        g_app_state.bike.lat_accel_ref_mg          = 0;
        g_app_state.bike.lon_accel_ref_mg          = 0;
        g_app_state.bike.lat_bias_mg               = 0;
        g_app_state.bike.lon_bias_mg               = 0;
        g_app_state.bike.last_imu_update_ms        = 0u;
        g_app_state.bike.last_update_ms            = now_ms;
        return;
    }

    if (s_bike_runtime.hard_rezero_requested != false)
    {
        BIKE_DYN_ResetRuntime(now_ms);
        s_bike_runtime.hard_rezero_requested = false;
    }

    raw_gnss_speed_good   = (settings->gnss_aid_enabled != 0u) ? BIKE_DYN_IsGnssSpeedGoodRaw(fix, settings, now_ms)   : false;
    raw_gnss_heading_good = (settings->gnss_aid_enabled != 0u) ? BIKE_DYN_IsGnssHeadingGoodRaw(fix, settings, now_ms) : false;
    BIKE_DYN_UpdateGnssAidHysteresis(raw_gnss_speed_good, raw_gnss_heading_good);

    gnss_speed_valid_raw   = (settings->gnss_aid_enabled != 0u) ? s_bike_runtime.gnss_speed_armed   : false;
    gnss_heading_valid_raw = (settings->gnss_aid_enabled != 0u) ? s_bike_runtime.gnss_heading_armed : false;

    gnss_speed_valid_est   = (BIKE_DYN_ModeAllowsGnss(bank_calc_mode) != false) ? gnss_speed_valid_raw   : false;
    gnss_heading_valid_est = (BIKE_DYN_ModeAllowsGnss(bank_calc_mode) != false) ? gnss_heading_valid_raw : false;
    obd_speed_valid_est    = (BIKE_DYN_ModeAllowsObd(bank_calc_mode)  != false) ? BIKE_DYN_IsObdSpeedValid(bike_state, settings, now_ms) : false;

    speed_source        = BIKE_DYN_SelectSpeedSource(bank_calc_mode,
                                                     obd_speed_valid_est,
                                                     gnss_speed_valid_est);
    selected_speed_mmps = BIKE_DYN_GetSelectedSpeedMmps(speed_source, fix, bike_state);

    BIKE_DYN_UpdateExternalReferences(settings,
                                      speed_source,
                                      gnss_speed_valid_est,
                                      gnss_heading_valid_est,
                                      obd_speed_valid_est,
                                      selected_speed_mmps,
                                      now_ms);

    imu_new_sample = BIKE_DYN_UpdateGravityObserver(mpu,
                                                    settings,
                                                    speed_source,
                                                    selected_speed_mmps,
                                                    gnss_heading_valid_est,
                                                    now_ms);

    if ((imu_new_sample != false) &&
        (s_bike_runtime.gravity_valid != false))
    {
        /* ------------------------------------------------------------------ */
        /*  current IMU -> bike-frame 내부 출력 계산                            */
        /*                                                                      */
        /*  gravity가 유효하면 현재 IMU 샘플을 bike-frame으로 투영해             */
        /*  내부 상태를 갱신한다.                                                */
        /*                                                                      */
        /*  중요한 변경점                                                        */
        /*  - zero_valid 전에도 내부 lon/lat IMU 성분 계산은 유지한다.          */
        /*  - 그러나 rider-facing lean / grade / G 출력은 이후 단계에서         */
        /*    zero_valid 전까지 0으로 잠긴다.                                   */
        /* ------------------------------------------------------------------ */
        BIKE_DYN_UpdateOutputsFromCurrentImu(settings);
    }

    BIKE_DYN_ProcessGyroBiasCalibration(settings,
                                        selected_speed_mmps,
                                        speed_source,
                                        imu_new_sample,
                                        now_ms);

    BIKE_DYN_ProcessZeroCapture(settings,
                                selected_speed_mmps,
                                speed_source,
                                imu_new_sample,
                                now_ms);

    if (s_bike_runtime.gravity_valid != false)
    {
        if (imu_new_sample != false)
        {
            if (s_bike_runtime.zero_valid != false)
            {
                BIKE_DYN_UpdateMotionReferenceBlend(speed_source,
                                                    gnss_heading_valid_est,
                                                    selected_speed_mmps);

                BIKE_DYN_UpdateBiasAndFusedOutputs(settings,
                                                   speed_source,
                                                   gnss_speed_valid_est,
                                                   gnss_heading_valid_est,
                                                   obd_speed_valid_est,
                                                   selected_speed_mmps);
            }
            else
            {
                BIKE_DYN_UpdateProvisionalDisplayOutputs(settings);
            }
        }
    }
    else
    {
        s_bike_runtime.bank_raw_deg      = 0.0f;
        s_bike_runtime.grade_raw_deg     = 0.0f;
        s_bike_runtime.bank_display_deg  = 0.0f;
        s_bike_runtime.grade_display_deg = 0.0f;
        s_bike_runtime.bank_rate_dps     = 0.0f;
        s_bike_runtime.grade_rate_dps    = 0.0f;
        s_bike_runtime.lon_imu_g         = 0.0f;
        s_bike_runtime.lat_imu_g         = 0.0f;
        s_bike_runtime.lon_fused_g       = 0.0f;
        s_bike_runtime.lat_fused_g       = 0.0f;
    }

    BIKE_DYN_UpdateHeadingOutput(gnss_heading_valid_raw);

    BIKE_DYN_PublishState(now_ms,
                          settings,
                          speed_source,
                          gnss_speed_valid_est,
                          gnss_heading_valid_est,
                          obd_speed_valid_est,
                          selected_speed_mmps);

    s_bike_runtime.last_task_ms = now_ms;
}
