
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
#define BIKE_DYN_GRAVITY_MPS2          (9.80665f)
#define BIKE_DYN_GRAVITY_CMS2          (980.665f)
#define BIKE_DYN_GRAVITY_MMPS2         (9806.65f)
#define BIKE_DYN_ONE_G_MG              (1000.0f)
#define BIKE_DYN_MIN_VEC_NORM          (0.000001f)
#define BIKE_DYN_MIN_DT_S              (0.001f)
#define BIKE_DYN_MAX_DT_S              (0.100f)
#define BIKE_DYN_DEFAULT_ACCEL_LSB_G   (8192.0f)  /* MPU6050 ±4g current driver */
#define BIKE_DYN_DEFAULT_GYRO_LSB_DPS  (65.5f)    /* MPU6050 ±500dps current driver */
#define BIKE_DYN_DEFAULT_STALE_MS      (250u)

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
    bool     gravity_valid;               /* gravity observer가 현재 유효한가         */
    bool     zero_valid;                  /* reset 기준 bike basis가 유효한가         */
    bool     zero_requested;              /* 다음 유효 IMU 샘플에서 zero capture 수행 */
    bool     hard_rezero_requested;       /* hard rezero 요청 pending                 */
    bool     auto_zero_done;              /* auto_zero_on_boot를 이미 수행했는가      */
    bool     imu_sample_valid;            /* 마지막 IMU sample이 유효한가             */

    uint32_t init_ms;                     /* 서비스 init 시각                         */
    uint32_t last_task_ms;                /* 마지막 task 진입 시각                    */
    uint32_t last_imu_timestamp_ms;       /* 마지막으로 반영한 MPU timestamp_ms       */
    uint32_t last_mpu_sample_count;       /* 마지막으로 반영한 MPU sample_count       */
    uint32_t last_gnss_fix_update_ms;     /* 마지막으로 소비한 GNSS fix.last_update_ms */
    uint32_t last_zero_capture_ms;        /* 마지막 zero capture 시각                 */

    /* ---------------------------------------------------------------------- */
    /*  gravity observer 상태                                                  */
    /*                                                                        */
    /*  gravity_est_* 는 sensor frame에서 본 "world up" unit vector 이다.     */
    /*  즉, accelerometer raw가 정지 시 가리키는 +1g 방향을 천천히 추적한다.   */
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
    /*                                                                        */
    /*  이후 현재 gravity 벡터와 이 basis를 비교하면                            */
    /*  yaw 절대값이 없어도 bank / grade를 계산할 수 있다.                     */
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
    /*  마지막 변환된 IMU sample                                                */
    /*                                                                        */
    /*  raw->공학단위 변환 결과를 여기 캐시해 두면                              */
    /*  같은 sample_count를 UI draw 사이클에서 다시 읽더라도                    */
    /*  불필요한 재계산을 줄일 수 있다.                                         */
    /* ---------------------------------------------------------------------- */
    float last_ax_g;
    float last_ay_g;
    float last_az_g;

    float last_gx_dps;
    float last_gy_dps;
    float last_gz_dps;

    float last_accel_norm_mg;
    float last_jerk_mg_per_s;
    float last_attitude_trust_permille;

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
    /*                                                                        */
    /*  여기서 level-frame 이란                                                 */
    /*  - current heading(전진 방향)은 유지하되                                 */
    /*  - current roll / pitch 는 제거해서                                      */
    /*    "지면 수평면 기준 forward / left" 로 재투영한 프레임을 뜻한다.        */
    /*                                                                        */
    /*  이 프레임을 쓰면 기울어진 상태에서도                                     */
    /*  lateral G / accel-decel 을 frame각과 분리해서                           */
    /*  훨씬 솔직하게 표시할 수 있다.                                           */
    /* ---------------------------------------------------------------------- */
    float lon_imu_g;                      /* IMU만으로 본 level forward accel        */
    float lat_imu_g;                      /* IMU만으로 본 level left accel           */

    /* GNSS / OBD가 주는 저주파 기준값 */
    float lon_ref_g;
    float lat_ref_g;

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

/* -------------------------------------------------------------------------- */
/*  axis enum -> sensor unit vector                                             */
/*                                                                            */
/*  mount axis 설정은 "센서 보드의 어느 축이 차량 forward / left 를 향하는가"   */
/*  를 지정하는 용도다.                                                        */
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
/*                                                                            */
/*  zero capture 시 yaw trim을 적용할 때 사용한다.                             */
/*  회전축은 zero-up 벡터이며,                                                  */
/*  "현재 수직축을 기준으로 forward/left basis를 몇 도 비틀 것인가" 를          */
/*  degree 단위로 조정한다.                                                    */
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
/*                                                                            */
/*  현 프로젝트의 GY86 driver는 현재                                           */
/*  - accel ±4g   => 8192 LSB/g                                                */
/*  - gyro  ±500dps => 65.5 LSB/dps                                            */
/*  로 설정되어 있다.                                                           */
/*                                                                            */
/*  그러나 future MPU9250 / 다른 scale 실험을 대비해서                         */
/*  이 값은 APP_STATE.settings.bike 에서 런타임으로 읽는다.                    */
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
/*                                                                            */
/*  우선순위                                                                   */
/*  1) future OBD speed                                                        */
/*  2) GNSS speed                                                               */
/*  3) IMU-only fallback                                                        */
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
/*  state publish helper                                                       */
/*                                                                            */
/*  중요                                                                       */
/*  - g_app_state.bike 안에는 future OBD service가 써 넣을 입력 필드도 있다.    */
/*  - 따라서 이 함수는 bike slice 전체를 memset 하지 않고                      */
/*    "추정기가 소유한 출력 필드만" 하나씩 갱신한다.                            */
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
            (s_bike_runtime.gravity_valid ? (0.70f * s_bike_runtime.last_attitude_trust_permille) : 0.0f) +
            ((speed_source == (uint8_t)APP_BIKE_SPEED_SOURCE_GNSS) ? 180.0f : 0.0f) +
            ((speed_source == (uint8_t)APP_BIKE_SPEED_SOURCE_OBD)  ? 260.0f : 0.0f)
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
        BIKE_DYN_RoundFloatToS32(((float)BIKE_DYN_AbsS32(selected_speed_mmps)) * 0.036f), 0, 65535);
    bike->gnss_speed_acc_kmh_x10    = (uint16_t)BIKE_DYN_ClampS32(
        BIKE_DYN_RoundFloatToS32(((float)g_app_state.gps.fix.sAcc) * 0.036f), 0, 65535);
    bike->gnss_head_acc_deg_x10     = (uint16_t)BIKE_DYN_ClampS32(
        BIKE_DYN_RoundFloatToS32(((float)g_app_state.gps.fix.headAcc) * 0.0001f), 0, 65535);

    bike->mount_yaw_trim_deg_x10    = (settings != 0) ? settings->mount_yaw_trim_deg_x10 : 0;

    bike->gnss_fix_ok               = (uint8_t)(g_app_state.gps.fix.fixOk ? 1u : 0u);
    bike->gnss_numsv_used           = g_app_state.gps.fix.numSV_used;
    bike->gnss_pdop_x100            = g_app_state.gps.fix.pDOP;
}

/* -------------------------------------------------------------------------- */
/*  zero / bias 포함 내부 상태 전체 리셋                                         */
/* -------------------------------------------------------------------------- */
static void BIKE_DYN_ResetRuntime(uint32_t now_ms)
{
    memset(&s_bike_runtime, 0, sizeof(s_bike_runtime));

    s_bike_runtime.initialized = true;
    s_bike_runtime.init_ms = now_ms;
    s_bike_runtime.last_task_ms = now_ms;
}

/* -------------------------------------------------------------------------- */
/*  zero basis 구성                                                             */
/*                                                                            */
/*  핵심 아이디어                                                               */
/*  1) 현재 gravity estimate를 zero-up 축으로 저장한다.                         */
/*  2) settings의 mount_forward_axis / mount_left_axis 힌트를                   */
/*     현재 수평면에 투영해서 vehicle 전진 방향의 부호/축을 정한다.             */
/*  3) yaw trim으로 전진축을 수직축 주위로 미세 보정한다.                       */
/*                                                                            */
/*  이 작업을 reset 시점에만 수행하면                                            */
/*  이후 runtime에서는 absolute yaw 없이도                                      */
/*  bank / grade / level-frame acceleration 계산이 가능하다.                   */
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

    /* ---------------------------------------------------------------------- */
    /*  우선 forward hint를 현재 수평면으로 투영한다.                            */
    /*  장착이 약간 비뚤어졌어도 reset 순간의 up 축을 기준으로                   */
    /*  "순수 전진방향 수평 투영" 만 남기기 위한 단계다.                         */
    /* ---------------------------------------------------------------------- */
    projection = BIKE_DYN_Dot3(fwd_hint_x, fwd_hint_y, fwd_hint_z, up_x, up_y, up_z);
    fwd_x = fwd_hint_x - (projection * up_x);
    fwd_y = fwd_hint_y - (projection * up_y);
    fwd_z = fwd_hint_z - (projection * up_z);

    if (BIKE_DYN_Normalize3(&fwd_x, &fwd_y, &fwd_z) == false)
    {
        /* ------------------------------------------------------------------ */
        /*  forward hint가 거의 up 축과 평행이면 left hint에서 역산한다.       */
        /* ------------------------------------------------------------------ */
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

    /* ---------------------------------------------------------------------- */
    /*  left basis는 up x fwd 로 다시 만들고                                   */
    /*  left hint와 부호가 반대면 forward/left 둘 다 뒤집어                     */
    /*  "좌/우 뒤집힘" 을 막는다.                                               */
    /* ---------------------------------------------------------------------- */
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

    /* ---------------------------------------------------------------------- */
    /*  yaw trim 적용                                                          */
    /*                                                                        */
    /*  enclosure / PCB / 양면테이프 조립 오차 때문에                           */
    /*  sensor의 nominal forward가 frame의 실제 전진축과 완전히 일치하지        */
    /*  않을 수 있으므로 reset-up 축 주위로 미세 회전을 허용한다.               */
    /* ---------------------------------------------------------------------- */
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

    /* ---------------------------------------------------------------------- */
    /*  수치 오차를 줄이기 위해 up도 한 번 더 재직교화한다.                     */
    /* ---------------------------------------------------------------------- */
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

    s_bike_runtime.zero_valid          = true;
    s_bike_runtime.last_zero_capture_ms = s_bike_runtime.last_imu_timestamp_ms;
    s_bike_runtime.bank_raw_deg         = 0.0f;
    s_bike_runtime.grade_raw_deg        = 0.0f;
    s_bike_runtime.bank_display_deg     = 0.0f;
    s_bike_runtime.grade_display_deg    = 0.0f;

    /* ---------------------------------------------------------------------- */
    /*  reset 순간의 level accel을 bias 초기값으로 바로 잡아 둔다.             */
    /*  정지 상태 reset 직후 lat/lon이 0 근처에서 시작하도록 만드는 장치다.    */
    /* ---------------------------------------------------------------------- */
    s_bike_runtime.lon_bias_g = s_bike_runtime.lon_imu_g;
    s_bike_runtime.lat_bias_g = s_bike_runtime.lat_imu_g;
    s_bike_runtime.lon_fused_g = 0.0f;
    s_bike_runtime.lat_fused_g = 0.0f;

    return true;
}

/* -------------------------------------------------------------------------- */
/*  current gravity와 zero basis로부터                                         */
/*  현재 heading-preserving level frame을 만든다.                              */
/*                                                                            */
/*  결과                                                                       */
/*  - out_fwd_*  : 현재 heading을 유지하되 roll/pitch 제거된 전진축            */
/*  - out_left_* : 위 level frame의 좌측축                                     */
/*                                                                            */
/*  이 축 위로 dynamic acceleration을 투영하면                                  */
/*  기울기/경사 영향을 제거한 lateral G / accel-decel을 얻을 수 있다.          */
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

    /* ---------------------------------------------------------------------- */
    /*  reset 기준 forward 축을 "현재 수평면" 으로 재투영한다.                  */
    /*  이 한 단계 덕분에 현재 roll/pitch가 사라진 heading-only forward 가       */
    /*  만들어지고, lat/lon 가속도를 bike level frame에서 읽을 수 있다.        */
    /* ---------------------------------------------------------------------- */
    proj  = BIKE_DYN_Dot3(s_bike_runtime.zero_fwd_x_s, s_bike_runtime.zero_fwd_y_s, s_bike_runtime.zero_fwd_z_s,
                          up_x, up_y, up_z);

    fwd_x = s_bike_runtime.zero_fwd_x_s - (proj * up_x);
    fwd_y = s_bike_runtime.zero_fwd_y_s - (proj * up_y);
    fwd_z = s_bike_runtime.zero_fwd_z_s - (proj * up_z);

    if (BIKE_DYN_Normalize3(&fwd_x, &fwd_y, &fwd_z) == false)
    {
        /* ------------------------------------------------------------------ */
        /*  극단적인 자세에서 forward projection이 무너지면                     */
        /*  left hint 기반으로 한 번 더 시도한다.                               */
        /* ------------------------------------------------------------------ */
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
/* -------------------------------------------------------------------------- */
static void BIKE_DYN_ConvertImuRawToEngineering(const app_gy86_mpu_raw_t *mpu,
                                                const app_bike_settings_t *settings,
                                                float *ax_g,
                                                float *ay_g,
                                                float *az_g,
                                                float *gx_dps,
                                                float *gy_dps,
                                                float *gz_dps)
{
    float accel_lsb_per_g;
    float gyro_lsb_per_dps;

    if (mpu == 0)
    {
        return;
    }

    accel_lsb_per_g = BIKE_DYN_GetAccelLsbPerG(settings);
    gyro_lsb_per_dps = BIKE_DYN_GetGyroLsbPerDps(settings);

    if (ax_g != 0) { *ax_g = ((float)mpu->accel_x_raw) / accel_lsb_per_g; }
    if (ay_g != 0) { *ay_g = ((float)mpu->accel_y_raw) / accel_lsb_per_g; }
    if (az_g != 0) { *az_g = ((float)mpu->accel_z_raw) / accel_lsb_per_g; }

    if (gx_dps != 0) { *gx_dps = ((float)mpu->gyro_x_raw) / gyro_lsb_per_dps; }
    if (gy_dps != 0) { *gy_dps = ((float)mpu->gyro_y_raw) / gyro_lsb_per_dps; }
    if (gz_dps != 0) { *gz_dps = ((float)mpu->gyro_z_raw) / gyro_lsb_per_dps; }
}

/* -------------------------------------------------------------------------- */
/*  6-axis gravity observer                                                     */
/*                                                                            */
/*  전략                                                                       */
/*  1) gyro로 gravity vector를 고속 예측한다.                                  */
/*  2) accelerometer norm / jerk를 보고 trust를 계산한다.                      */
/*  3) trust가 좋은 구간에서만 accel 방향으로 천천히 correction 한다.           */
/*                                                                            */
/*  이 observer는 APP_ALTITUDE의 gravity estimator와 같은 철학을 공유하지만    */
/*  bike dynamics 용으로 jerk gate와 bike-specific 설정을 포함한다.            */
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

    float accel_norm_g;
    float accel_norm_mg;
    float a_hat_x;
    float a_hat_y;
    float a_hat_z;

    float dt_s;
    float jerk_mg_per_s;
    float trust_norm;
    float trust_jerk;
    float trust_total;
    float g_pred_x;
    float g_pred_y;
    float g_pred_z;
    float correction_alpha;

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

    BIKE_DYN_ConvertImuRawToEngineering(mpu, settings, &ax_g, &ay_g, &az_g, &gx_dps, &gy_dps, &gz_dps);

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

    a_hat_x = ax_g / accel_norm_g;
    a_hat_y = ay_g / accel_norm_g;
    a_hat_z = az_g / accel_norm_g;

    /* ---------------------------------------------------------------------- */
    /*  같은 raw sample을 두 번 적분하지 않도록 sample_count를 먼저 확인한다.   */
    /* ---------------------------------------------------------------------- */
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

    /* ---------------------------------------------------------------------- */
    /*  gravity observer 초기화                                                 */
    /* ---------------------------------------------------------------------- */
    if (s_bike_runtime.gravity_valid == false)
    {
        s_bike_runtime.gravity_est_x_s = a_hat_x;
        s_bike_runtime.gravity_est_y_s = a_hat_y;
        s_bike_runtime.gravity_est_z_s = a_hat_z;
        s_bike_runtime.gravity_valid   = true;
    }
    else
    {
        float gx_rad_s;
        float gy_rad_s;
        float gz_rad_s;
        float dgx;
        float dgy;
        float dgz;

        /* ------------------------------------------------------------------ */
        /*  gyro로 gravity 벡터를 한 step 예측한다.                            */
        /*  sensor frame에서 본 world-up 벡터이므로                            */
        /*  du/dt = -omega x u 관계를 사용한다.                                */
        /* ------------------------------------------------------------------ */
        gx_rad_s = gx_dps * ((float)M_PI / 180.0f);
        gy_rad_s = gy_dps * ((float)M_PI / 180.0f);
        gz_rad_s = gz_dps * ((float)M_PI / 180.0f);

        BIKE_DYN_Cross3(gx_rad_s, gy_rad_s, gz_rad_s,
                        s_bike_runtime.gravity_est_x_s,
                        s_bike_runtime.gravity_est_y_s,
                        s_bike_runtime.gravity_est_z_s,
                        &dgx, &dgy, &dgz);

        g_pred_x = s_bike_runtime.gravity_est_x_s - (dgx * dt_s);
        g_pred_y = s_bike_runtime.gravity_est_y_s - (dgy * dt_s);
        g_pred_z = s_bike_runtime.gravity_est_z_s - (dgz * dt_s);

        if (BIKE_DYN_Normalize3(&g_pred_x, &g_pred_y, &g_pred_z) == false)
        {
            g_pred_x = a_hat_x;
            g_pred_y = a_hat_y;
            g_pred_z = a_hat_z;
        }

        /* ------------------------------------------------------------------ */
        /*  trust 계산                                                          */
        /*                                                                    */
        /*  trust_norm                                                         */
        /*  - accel norm이 1g에 가까울수록 높다.                               */
        /*                                                                    */
        /*  trust_jerk                                                         */
        /*  - 요철/진동/충격으로 jerk가 크면 낮춘다.                            */
        /*                                                                    */
        /*  둘을 곱해 correction gain을 조절한다.                               */
        /* ------------------------------------------------------------------ */
        trust_norm = 1.0f - (fabsf(accel_norm_mg - BIKE_DYN_ONE_G_MG) /
                             (float)BIKE_DYN_ClampS32((int32_t)settings->imu_attitude_accel_gate_mg, 1, 5000));

        trust_jerk = 1.0f - (jerk_mg_per_s /
                             (float)BIKE_DYN_ClampS32((int32_t)settings->imu_jerk_gate_mg_per_s, 1, 50000));

        trust_norm = BIKE_DYN_Clamp01F(trust_norm);
        trust_jerk = BIKE_DYN_Clamp01F(trust_jerk);
        trust_total = trust_norm * trust_jerk;

        correction_alpha = BIKE_DYN_LpfAlphaFromTauMs(settings->imu_gravity_tau_ms, dt_s);
        correction_alpha *= trust_total;

        s_bike_runtime.gravity_est_x_s = g_pred_x + (correction_alpha * (a_hat_x - g_pred_x));
        s_bike_runtime.gravity_est_y_s = g_pred_y + (correction_alpha * (a_hat_y - g_pred_y));
        s_bike_runtime.gravity_est_z_s = g_pred_z + (correction_alpha * (a_hat_z - g_pred_z));

        if (BIKE_DYN_Normalize3(&s_bike_runtime.gravity_est_x_s,
                                &s_bike_runtime.gravity_est_y_s,
                                &s_bike_runtime.gravity_est_z_s) == false)
        {
            s_bike_runtime.gravity_est_x_s = a_hat_x;
            s_bike_runtime.gravity_est_y_s = a_hat_y;
            s_bike_runtime.gravity_est_z_s = a_hat_z;
        }

        s_bike_runtime.last_attitude_trust_permille = 1000.0f * trust_total;
    }

    s_bike_runtime.last_ax_g = ax_g;
    s_bike_runtime.last_ay_g = ay_g;
    s_bike_runtime.last_az_g = az_g;

    s_bike_runtime.last_gx_dps = gx_dps;
    s_bike_runtime.last_gy_dps = gy_dps;
    s_bike_runtime.last_gz_dps = gz_dps;

    s_bike_runtime.last_mpu_sample_count = mpu->sample_count;
    s_bike_runtime.last_imu_timestamp_ms = mpu->timestamp_ms;

    return true;
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

    if ((s_bike_runtime.last_task_ms != 0u) &&
        (s_bike_runtime.last_imu_timestamp_ms > s_bike_runtime.last_task_ms))
    {
        dt_s = ((float)(s_bike_runtime.last_imu_timestamp_ms - s_bike_runtime.last_task_ms)) * 0.001f;
    }
    else
    {
        dt_s = BIKE_DYN_MIN_DT_S;
    }
    dt_s = BIKE_DYN_ClampF(dt_s, BIKE_DYN_MIN_DT_S, BIKE_DYN_MAX_DT_S);

    /* ---------------------------------------------------------------------- */
    /*  zero basis에 대한 현재 up 벡터 성분                                     */
    /* ---------------------------------------------------------------------- */
    up_bx = BIKE_DYN_Dot3(s_bike_runtime.zero_fwd_x_s,  s_bike_runtime.zero_fwd_y_s,  s_bike_runtime.zero_fwd_z_s,
                          s_bike_runtime.gravity_est_x_s, s_bike_runtime.gravity_est_y_s, s_bike_runtime.gravity_est_z_s);

    up_by = BIKE_DYN_Dot3(s_bike_runtime.zero_left_x_s, s_bike_runtime.zero_left_y_s, s_bike_runtime.zero_left_z_s,
                          s_bike_runtime.gravity_est_x_s, s_bike_runtime.gravity_est_y_s, s_bike_runtime.gravity_est_z_s);

    up_bz = BIKE_DYN_Dot3(s_bike_runtime.zero_up_x_s,   s_bike_runtime.zero_up_y_s,   s_bike_runtime.zero_up_z_s,
                          s_bike_runtime.gravity_est_x_s, s_bike_runtime.gravity_est_y_s, s_bike_runtime.gravity_est_z_s);

    /* ---------------------------------------------------------------------- */
    /*  sign 규칙                                                               */
    /*  - bank  : +좌 lean, -우 lean                                           */
    /*  - grade : +nose up, -nose down                                         */
    /* ---------------------------------------------------------------------- */
    bank_rad  = atan2f(-up_by, BIKE_DYN_ClampF(up_bz, -1.0f, 1.0f));
    grade_rad = atan2f(-up_bx, BIKE_DYN_SafeSqrtF((up_by * up_by) + (up_bz * up_bz)));

    s_bike_runtime.bank_raw_deg  = bank_rad  * (180.0f / (float)M_PI);
    s_bike_runtime.grade_raw_deg = grade_rad * (180.0f / (float)M_PI);

    s_bike_runtime.bank_rate_dps  = -BIKE_DYN_Dot3(s_bike_runtime.last_gx_dps, s_bike_runtime.last_gy_dps, s_bike_runtime.last_gz_dps,
                                                   s_bike_runtime.zero_fwd_x_s, s_bike_runtime.zero_fwd_y_s, s_bike_runtime.zero_fwd_z_s);
    s_bike_runtime.grade_rate_dps = -BIKE_DYN_Dot3(s_bike_runtime.last_gx_dps, s_bike_runtime.last_gy_dps, s_bike_runtime.last_gz_dps,
                                                   s_bike_runtime.zero_left_x_s, s_bike_runtime.zero_left_y_s, s_bike_runtime.zero_left_z_s);

    s_bike_runtime.bank_display_deg = BIKE_DYN_LpfUpdate(s_bike_runtime.bank_display_deg,
                                                         s_bike_runtime.bank_raw_deg,
                                                         settings->lean_display_tau_ms,
                                                         dt_s);

    s_bike_runtime.grade_display_deg = BIKE_DYN_LpfUpdate(s_bike_runtime.grade_display_deg,
                                                          s_bike_runtime.grade_raw_deg,
                                                          settings->grade_display_tau_ms,
                                                          dt_s);

    /* ---------------------------------------------------------------------- */
    /*  dynamic acceleration = specific force - gravity                         */
    /*                                                                        */
    /*  accelerometer는 정지 시에도 +1g를 보기 때문에                            */
    /*  gravity observer가 추정한 up 벡터를 빼서 실제 선형가속만 남긴다.         */
    /* ---------------------------------------------------------------------- */
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
/*  - IMU는 빠르고 연속적이다.                                                 */
/*  - GNSS / OBD는 절대 속도/코스를 천천히 주는 anchor다.                      */
/*  - 따라서 ref는 "low-frequency truth" 로만 쓰고                             */
/*    최종 출력은 bias-corrected IMU 값을 계속 유지한다.                       */
/* -------------------------------------------------------------------------- */
static void BIKE_DYN_UpdateExternalReferences(const app_bike_settings_t *settings,
                                              uint8_t speed_source,
                                              bool gnss_speed_valid,
                                              bool gnss_heading_valid,
                                              bool obd_speed_valid)
{
    const gps_fix_basic_t *fix;
    const app_bike_state_t *bike;
    float dt_s;
    float current_speed_mps;
    float speed_derivative_g;
    float heading_deg;
    float heading_delta_deg;
    float course_rate_rad_s;
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

    /* ---------------------------------------------------------------------- */
    /*  longitudinal ref                                                        */
    /*                                                                        */
    /*  speed source가 OBD면 OBD speed derivative를 우선 사용하고,               */
    /*  아니면 GNSS gSpeed derivative를 사용한다.                               */
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

    /* ---------------------------------------------------------------------- */
    /*  lateral ref                                                             */
    /*                                                                        */
    /*  GNSS headMot는 시계방향 증가(eastward turn이 +)이므로                    */
    /*  좌회전을 + 로 쓰는 본 서비스 sign 규칙에 맞추기 위해                     */
    /*  lat_ref = -(v * heading_rate) / g 를 사용한다.                          */
    /*                                                                        */
    /*  speed는 source 우선순위를 따른다.                                       */
    /*  즉 future OBD speed가 있다면 GNSS heading + OBD speed 조합도 가능하다.  */
    /* ---------------------------------------------------------------------- */
    if ((gnss_heading_valid != false) &&
        (current_gnss_ms != 0u) &&
        (current_gnss_ms != s_bike_runtime.prev_gnss_heading_ms))
    {
        if (speed_source == (uint8_t)APP_BIKE_SPEED_SOURCE_OBD)
        {
            current_speed_mps = ((float)bike->obd_input_speed_mmps) * 0.001f;
        }
        else
        {
            current_speed_mps = ((float)fix->gSpeed) * 0.001f;
        }

        heading_deg = ((float)fix->headMot) * 0.00001f;

        if ((s_bike_runtime.prev_gnss_heading_valid != false) &&
            (current_gnss_ms > s_bike_runtime.prev_gnss_heading_ms))
        {
            dt_s = ((float)(current_gnss_ms - s_bike_runtime.prev_gnss_heading_ms)) * 0.001f;
            dt_s = BIKE_DYN_ClampF(dt_s, BIKE_DYN_MIN_DT_S, BIKE_DYN_MAX_DT_S);

            heading_delta_deg = BIKE_DYN_WrapDeg(heading_deg - s_bike_runtime.prev_gnss_heading_deg);
            course_rate_rad_s = (heading_delta_deg * ((float)M_PI / 180.0f)) / dt_s;

            s_bike_runtime.lat_ref_g = -(current_speed_mps * course_rate_rad_s) / BIKE_DYN_GRAVITY_MPS2;
        }

        s_bike_runtime.prev_gnss_heading_valid = true;
        s_bike_runtime.prev_gnss_heading_deg   = heading_deg;
        s_bike_runtime.prev_gnss_heading_ms    = current_gnss_ms;
    }

    if (current_gnss_ms != 0u)
    {
        s_bike_runtime.last_gnss_fix_update_ms = current_gnss_ms;
    }
}

/* -------------------------------------------------------------------------- */
/*  bias adaptation                                                             */
/*                                                                            */
/*  low-frequency ref가 valid한 구간에서만                                      */
/*  IMU accel과 external ref의 차이를 bias로 천천히 흡수한다.                   */
/* -------------------------------------------------------------------------- */
static void BIKE_DYN_UpdateBiasAndFusedOutputs(const app_bike_settings_t *settings,
                                               bool gnss_speed_valid,
                                               bool gnss_heading_valid,
                                               bool obd_speed_valid)
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

    if ((s_bike_runtime.last_task_ms != 0u) &&
        (s_bike_runtime.last_imu_timestamp_ms > s_bike_runtime.last_task_ms))
    {
        dt_s = ((float)(s_bike_runtime.last_imu_timestamp_ms - s_bike_runtime.last_task_ms)) * 0.001f;
    }
    else
    {
        dt_s = BIKE_DYN_MIN_DT_S;
    }
    dt_s = BIKE_DYN_ClampF(dt_s, BIKE_DYN_MIN_DT_S, BIKE_DYN_MAX_DT_S);

    outlier_gate_g = ((float)settings->gnss_outlier_gate_mg) / 1000.0f;

    allow_lon_ref = (obd_speed_valid || gnss_speed_valid) ? true : false;
    allow_lat_ref = gnss_heading_valid ? true : false;

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
/*  public API                                                                  */
/* -------------------------------------------------------------------------- */
void BIKE_DYNAMICS_Init(uint32_t now_ms)
{
    BIKE_DYN_ResetRuntime(now_ms);

    /* ---------------------------------------------------------------------- */
    /*  초기 state 공개값                                                       */
    /*  - zero_valid가 아직 없더라도                                             */
    /*    initialized 플래그는 먼저 올려 둔다.                                  */
    /* ---------------------------------------------------------------------- */
    g_app_state.bike.initialized = true;
    g_app_state.bike.last_update_ms = now_ms;
    g_app_state.bike.last_imu_update_ms = 0u;
    g_app_state.bike.last_zero_capture_ms = 0u;
    g_app_state.bike.last_gnss_aid_ms = 0u;

    if (g_app_state.settings.bike.auto_zero_on_boot != 0u)
    {
        s_bike_runtime.zero_requested = true;
    }
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

void ResetBankingAngleSensor(void)
{
    BIKE_DYNAMICS_RequestZeroCapture();
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
        g_app_state.bike.last_update_ms = now_ms;
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  hard rezero는 runtime 내부 상태를 완전히 청소한 뒤                      */
    /*  다음 유효 샘플에서 zero capture가 다시 일어나게 만든다.                 */
    /* ---------------------------------------------------------------------- */
    if (s_bike_runtime.hard_rezero_requested != false)
    {
        BIKE_DYN_ResetRuntime(now_ms);
        s_bike_runtime.zero_requested = true;
        s_bike_runtime.hard_rezero_requested = false;
    }

    imu_new_sample = BIKE_DYN_UpdateGravityObserver(mpu, settings, now_ms);

    /* ---------------------------------------------------------------------- */
    /*  speed validity / source selection                                       */
    /* ---------------------------------------------------------------------- */
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

    /* ---------------------------------------------------------------------- */
    /*  auto zero                                                               */
    /* ---------------------------------------------------------------------- */
    if ((settings->auto_zero_on_boot != 0u) &&
        (s_bike_runtime.auto_zero_done == false) &&
        (s_bike_runtime.gravity_valid != false) &&
        (imu_new_sample != false))
    {
        BIKE_DYN_UpdateOutputsFromCurrentImu(settings);
        if (BIKE_DYN_RebuildZeroBasis(settings) != false)
        {
            s_bike_runtime.auto_zero_done = true;
            s_bike_runtime.zero_requested = false;
        }
    }

    /* ---------------------------------------------------------------------- */
    /*  수동 reset 요청 처리                                                    */
    /* ---------------------------------------------------------------------- */
    if ((s_bike_runtime.zero_requested != false) &&
        (s_bike_runtime.gravity_valid != false) &&
        (imu_new_sample != false))
    {
        BIKE_DYN_UpdateOutputsFromCurrentImu(settings);

        if (BIKE_DYN_RebuildZeroBasis(settings) != false)
        {
            s_bike_runtime.zero_requested = false;
        }
    }

    /* ---------------------------------------------------------------------- */
    /*  zero basis가 있으면 현재 출력 갱신                                      */
    /* ---------------------------------------------------------------------- */
    if ((s_bike_runtime.gravity_valid != false) &&
        (s_bike_runtime.zero_valid != false) &&
        (imu_new_sample != false))
    {
        BIKE_DYN_UpdateOutputsFromCurrentImu(settings);
        BIKE_DYN_UpdateExternalReferences(settings,
                                          speed_source,
                                          gnss_speed_valid,
                                          gnss_heading_valid,
                                          obd_speed_valid);
        BIKE_DYN_UpdateBiasAndFusedOutputs(settings,
                                           gnss_speed_valid,
                                           gnss_heading_valid,
                                           obd_speed_valid);
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

    BIKE_DYN_PublishState(now_ms,
                          settings,
                          speed_source,
                          gnss_speed_valid,
                          gnss_heading_valid,
                          obd_speed_valid,
                          selected_speed_mmps);

    s_bike_runtime.last_task_ms = s_bike_runtime.last_imu_timestamp_ms;
}
