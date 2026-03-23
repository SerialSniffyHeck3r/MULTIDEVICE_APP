
#include "Motor_Dynamics.h"

#include "Motor_State.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

typedef struct
{
    bool  initialized;
    bool  gravity_valid;
    bool  zero_requested;
    uint32_t last_imu_sample_count;
    uint32_t last_imu_timestamp_ms;
    float grav_x_s;
    float grav_y_s;
    float grav_z_s;
    float prev_ax_g;
    float prev_ay_g;
    float prev_az_g;
    float zero_fwd_x_s;
    float zero_fwd_y_s;
    float zero_fwd_z_s;
    float zero_left_x_s;
    float zero_left_y_s;
    float zero_left_z_s;
    float zero_up_x_s;
    float zero_up_y_s;
    float zero_up_z_s;
    float prev_speed_mps;
    uint32_t prev_speed_ms;
    float bank_display_deg;
    float grade_display_deg;
    float lat_display_g;
    float lon_display_g;
} motor_dynamics_runtime_t;

static motor_dynamics_runtime_t s_runtime;

static float motor_dyn_clampf(float v, float min_v, float max_v)
{
    if (v < min_v)
    {
        return min_v;
    }
    if (v > max_v)
    {
        return max_v;
    }
    return v;
}

static float motor_dyn_safe_sqrt(float v)
{
    if (v <= 0.0f)
    {
        return 0.0f;
    }
    return sqrtf(v);
}

static void motor_dyn_normalize3(float *x, float *y, float *z)
{
    float norm;

    if ((x == 0) || (y == 0) || (z == 0))
    {
        return;
    }

    norm = motor_dyn_safe_sqrt((*x * *x) + (*y * *y) + (*z * *z));
    if (norm < 0.000001f)
    {
        return;
    }

    *x /= norm;
    *y /= norm;
    *z /= norm;
}

static float motor_dyn_dot3(float ax, float ay, float az, float bx, float by, float bz)
{
    return (ax * bx) + (ay * by) + (az * bz);
}

static void motor_dyn_cross3(float ax, float ay, float az,
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

static void motor_dyn_axis_to_vector(uint8_t axis, float *x, float *y, float *z)
{
    if ((x == 0) || (y == 0) || (z == 0))
    {
        return;
    }

    *x = 0.0f;
    *y = 0.0f;
    *z = 0.0f;

    switch ((app_bike_axis_t)axis)
    {
    case APP_BIKE_AXIS_NEG_X:
        *x = -1.0f;
        break;
    case APP_BIKE_AXIS_POS_Y:
        *y = 1.0f;
        break;
    case APP_BIKE_AXIS_NEG_Y:
        *y = -1.0f;
        break;
    case APP_BIKE_AXIS_POS_Z:
        *z = 1.0f;
        break;
    case APP_BIKE_AXIS_NEG_Z:
        *z = -1.0f;
        break;
    case APP_BIKE_AXIS_POS_X:
    default:
        *x = 1.0f;
        break;
    }
}

static void motor_dyn_capture_zero_basis(const app_bike_settings_t *bike_settings)
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
    float trim_rad;
    float c;
    float s;
    float trim_fwd_x;
    float trim_fwd_y;
    float trim_fwd_z;
    float projected_len;

    if ((bike_settings == 0) || (s_runtime.gravity_valid == false))
    {
        return;
    }

    motor_dyn_axis_to_vector(bike_settings->mount_forward_axis, &fwd_x, &fwd_y, &fwd_z);
    motor_dyn_axis_to_vector(bike_settings->mount_left_axis, &left_x, &left_y, &left_z);
    motor_dyn_cross3(fwd_x, fwd_y, fwd_z, left_x, left_y, left_z, &up_x, &up_y, &up_z);
    motor_dyn_normalize3(&up_x, &up_y, &up_z);

    trim_rad = ((float)bike_settings->mount_yaw_trim_deg_x10 / 10.0f) * (float)(M_PI / 180.0f);
    c = cosf(trim_rad);
    s = sinf(trim_rad);

    trim_fwd_x = (fwd_x * c) + (left_x * s);
    trim_fwd_y = (fwd_y * c) + (left_y * s);
    trim_fwd_z = (fwd_z * c) + (left_z * s);

    /* ---------------------------------------------------------------------- */
    /*  zero_up 은 "지금 현재의 중력 방향" 이다.                              */
    /*  여기서부터는 mounted axis 기반 forward를 이 up 평면에 재투영해서       */
    /*  실제 bike zero basis를 만든다.                                         */
    /* ---------------------------------------------------------------------- */
    s_runtime.zero_up_x_s = s_runtime.grav_x_s;
    s_runtime.zero_up_y_s = s_runtime.grav_y_s;
    s_runtime.zero_up_z_s = s_runtime.grav_z_s;
    motor_dyn_normalize3(&s_runtime.zero_up_x_s, &s_runtime.zero_up_y_s, &s_runtime.zero_up_z_s);

    projected_len = motor_dyn_dot3(trim_fwd_x, trim_fwd_y, trim_fwd_z,
                                   s_runtime.zero_up_x_s, s_runtime.zero_up_y_s, s_runtime.zero_up_z_s);

    s_runtime.zero_fwd_x_s = trim_fwd_x - (s_runtime.zero_up_x_s * projected_len);
    s_runtime.zero_fwd_y_s = trim_fwd_y - (s_runtime.zero_up_y_s * projected_len);
    s_runtime.zero_fwd_z_s = trim_fwd_z - (s_runtime.zero_up_z_s * projected_len);
    motor_dyn_normalize3(&s_runtime.zero_fwd_x_s, &s_runtime.zero_fwd_y_s, &s_runtime.zero_fwd_z_s);

    motor_dyn_cross3(s_runtime.zero_up_x_s, s_runtime.zero_up_y_s, s_runtime.zero_up_z_s,
                     s_runtime.zero_fwd_x_s, s_runtime.zero_fwd_y_s, s_runtime.zero_fwd_z_s,
                     &s_runtime.zero_left_x_s, &s_runtime.zero_left_y_s, &s_runtime.zero_left_z_s);
    motor_dyn_normalize3(&s_runtime.zero_left_x_s, &s_runtime.zero_left_y_s, &s_runtime.zero_left_z_s);
}

static void motor_dyn_update_history(motor_dynamics_state_t *dyn, int32_t altitude_cm)
{
    uint16_t idx;

    if (dyn == 0)
    {
        return;
    }

    idx = dyn->history_head % MOTOR_HISTORY_SAMPLE_COUNT;
    dyn->bank_history_x10[idx] = dyn->bank_deg_x10;
    dyn->lat_history_x10[idx] = (int16_t)(dyn->lat_accel_mg / 100);
    dyn->alt_history_m[idx] = (int16_t)(altitude_cm / 100);
    dyn->grade_history_x10[idx] = (int16_t)(dyn->grade_deg_x10);
    dyn->history_head = (uint16_t)((idx + 1u) % MOTOR_HISTORY_SAMPLE_COUNT);
}

void Motor_Dynamics_Init(void)
{
    memset(&s_runtime, 0, sizeof(s_runtime));
}

void Motor_Dynamics_RequestZeroCapture(void)
{
    s_runtime.zero_requested = true;
}

void Motor_Dynamics_ResetSessionPeaks(void)
{
    motor_state_t *state;

    state = Motor_State_GetMutable();
    if (state == 0)
    {
        return;
    }

    state->dyn.max_left_bank_deg_x10 = 0;
    state->dyn.max_right_bank_deg_x10 = 0;
    state->dyn.max_left_lat_mg = 0;
    state->dyn.max_right_lat_mg = 0;
    state->dyn.max_accel_mg = 0;
    state->dyn.max_brake_mg = 0;
}

void Motor_Dynamics_Task(uint32_t now_ms)
{
    motor_state_t *state;
    const app_bike_settings_t *bike_settings;
    const app_gy86_mpu_raw_t *mpu;
    float dt_s;
    float accel_lsb_per_g;
    float gyro_lsb_per_dps;
    float ax_g;
    float ay_g;
    float az_g;
    float gx_dps;
    float gy_dps;
    float gz_dps;
    float accel_norm_g;
    float alpha_grav;
    float trust_accel;
    float trust_jerk;
    float jerk_mg_per_s;
    float dyn_x_g;
    float dyn_y_g;
    float dyn_z_g;
    float up_bx;
    float up_by;
    float up_bz;
    float bank_deg;
    float grade_deg;
    float bank_rate_dps;
    float grade_rate_dps;
    float lon_imu_g;
    float lat_imu_g;
    float yaw_rate_rad_s;
    float lat_ref_g;
    float lon_ref_g;
    float speed_mps;
    float ref_blend;
    float output_alpha;
    float lean_alpha;

    state = Motor_State_GetMutable();
    if (state == 0)
    {
        return;
    }

    bike_settings = &state->snapshot.settings.bike;
    mpu = &state->snapshot.gy86.mpu;

    if ((state->snapshot.gy86.status_flags & APP_GY86_STATUS_MPU_VALID) == 0u)
    {
        state->dyn.imu_valid = false;
        return;
    }

    if ((s_runtime.last_imu_sample_count == mpu->sample_count) &&
        (s_runtime.last_imu_timestamp_ms == mpu->timestamp_ms))
    {
        return;
    }

    if (s_runtime.last_imu_timestamp_ms == 0u)
    {
        dt_s = 0.02f;
    }
    else
    {
        dt_s = (float)((int32_t)mpu->timestamp_ms - (int32_t)s_runtime.last_imu_timestamp_ms) / 1000.0f;
    }
    dt_s = motor_dyn_clampf(dt_s, 0.005f, 0.100f);

    s_runtime.last_imu_timestamp_ms = mpu->timestamp_ms;
    s_runtime.last_imu_sample_count = mpu->sample_count;

    accel_lsb_per_g = (bike_settings->imu_accel_lsb_per_g != 0u) ? (float)bike_settings->imu_accel_lsb_per_g : 8192.0f;
    gyro_lsb_per_dps = (bike_settings->imu_gyro_lsb_per_dps_x10 != 0u) ? ((float)bike_settings->imu_gyro_lsb_per_dps_x10 / 10.0f) : 65.5f;

    ax_g = (float)mpu->accel_x_raw / accel_lsb_per_g;
    ay_g = (float)mpu->accel_y_raw / accel_lsb_per_g;
    az_g = (float)mpu->accel_z_raw / accel_lsb_per_g;

    gx_dps = (float)mpu->gyro_x_raw / gyro_lsb_per_dps;
    gy_dps = (float)mpu->gyro_y_raw / gyro_lsb_per_dps;
    gz_dps = (float)mpu->gyro_z_raw / gyro_lsb_per_dps;

    accel_norm_g = motor_dyn_safe_sqrt((ax_g * ax_g) + (ay_g * ay_g) + (az_g * az_g));
    jerk_mg_per_s = motor_dyn_safe_sqrt(((ax_g - s_runtime.prev_ax_g) * (ax_g - s_runtime.prev_ax_g)) +
                                        ((ay_g - s_runtime.prev_ay_g) * (ay_g - s_runtime.prev_ay_g)) +
                                        ((az_g - s_runtime.prev_az_g) * (az_g - s_runtime.prev_az_g))) * (1000.0f / dt_s);

    s_runtime.prev_ax_g = ax_g;
    s_runtime.prev_ay_g = ay_g;
    s_runtime.prev_az_g = az_g;

    if (accel_norm_g > 0.20f)
    {
        ax_g /= accel_norm_g;
        ay_g /= accel_norm_g;
        az_g /= accel_norm_g;
    }

    trust_accel = 1.0f - motor_dyn_clampf(fabsf(accel_norm_g - 1.0f) / 0.35f, 0.0f, 1.0f);
    trust_jerk = 1.0f - motor_dyn_clampf(jerk_mg_per_s / (float)((bike_settings->imu_jerk_gate_mg_per_s != 0u) ? bike_settings->imu_jerk_gate_mg_per_s : 3500u), 0.0f, 1.0f);
    state->dyn.confidence_permille = (uint16_t)(1000.0f * motor_dyn_clampf(0.6f * trust_accel + 0.4f * trust_jerk, 0.0f, 1.0f));

    alpha_grav = dt_s / motor_dyn_clampf((float)bike_settings->imu_gravity_tau_ms / 1000.0f, 0.05f, 10.0f);
    alpha_grav = motor_dyn_clampf(alpha_grav, 0.001f, 0.40f);

    if (s_runtime.gravity_valid == false)
    {
        s_runtime.grav_x_s = ax_g;
        s_runtime.grav_y_s = ay_g;
        s_runtime.grav_z_s = az_g;
        s_runtime.gravity_valid = true;
    }
    else
    {
        s_runtime.grav_x_s += (ax_g - s_runtime.grav_x_s) * alpha_grav;
        s_runtime.grav_y_s += (ay_g - s_runtime.grav_y_s) * alpha_grav;
        s_runtime.grav_z_s += (az_g - s_runtime.grav_z_s) * alpha_grav;
        motor_dyn_normalize3(&s_runtime.grav_x_s, &s_runtime.grav_y_s, &s_runtime.grav_z_s);
    }

    if ((bike_settings->auto_zero_on_boot != 0u) && (state->dyn.zero_valid == false))
    {
        s_runtime.zero_requested = true;
    }

    if (s_runtime.zero_requested != false)
    {
        motor_dyn_capture_zero_basis(bike_settings);
        state->dyn.zero_valid = true;
        s_runtime.zero_requested = false;
        Motor_Dynamics_ResetSessionPeaks();
    }

    if ((s_runtime.gravity_valid == false) || (state->dyn.zero_valid == false))
    {
        return;
    }

    up_bx = motor_dyn_dot3(s_runtime.zero_fwd_x_s,  s_runtime.zero_fwd_y_s,  s_runtime.zero_fwd_z_s,
                           s_runtime.grav_x_s, s_runtime.grav_y_s, s_runtime.grav_z_s);
    up_by = motor_dyn_dot3(s_runtime.zero_left_x_s, s_runtime.zero_left_y_s, s_runtime.zero_left_z_s,
                           s_runtime.grav_x_s, s_runtime.grav_y_s, s_runtime.grav_z_s);
    up_bz = motor_dyn_dot3(s_runtime.zero_up_x_s,   s_runtime.zero_up_y_s,   s_runtime.zero_up_z_s,
                           s_runtime.grav_x_s, s_runtime.grav_y_s, s_runtime.grav_z_s);

    bank_deg = atan2f(-up_by, motor_dyn_clampf(up_bz, -1.0f, 1.0f)) * (180.0f / (float)M_PI);
    grade_deg = atan2f(-up_bx, motor_dyn_safe_sqrt((up_by * up_by) + (up_bz * up_bz))) * (180.0f / (float)M_PI);

    bank_rate_dps = -motor_dyn_dot3(gx_dps, gy_dps, gz_dps,
                                    s_runtime.zero_fwd_x_s, s_runtime.zero_fwd_y_s, s_runtime.zero_fwd_z_s);
    grade_rate_dps = -motor_dyn_dot3(gx_dps, gy_dps, gz_dps,
                                     s_runtime.zero_left_x_s, s_runtime.zero_left_y_s, s_runtime.zero_left_z_s);

    dyn_x_g = ((float)mpu->accel_x_raw / accel_lsb_per_g) - s_runtime.grav_x_s;
    dyn_y_g = ((float)mpu->accel_y_raw / accel_lsb_per_g) - s_runtime.grav_y_s;
    dyn_z_g = ((float)mpu->accel_z_raw / accel_lsb_per_g) - s_runtime.grav_z_s;

    lon_imu_g = motor_dyn_dot3(dyn_x_g, dyn_y_g, dyn_z_g,
                               s_runtime.zero_fwd_x_s, s_runtime.zero_fwd_y_s, s_runtime.zero_fwd_z_s);
    lat_imu_g = motor_dyn_dot3(dyn_x_g, dyn_y_g, dyn_z_g,
                               s_runtime.zero_left_x_s, s_runtime.zero_left_y_s, s_runtime.zero_left_z_s);

    /* ---------------------------------------------------------------------- */
    /*  GNSS heading / speed를 저주파 anchor 로만 사용한다.                     */
    /*  - yaw_rate(gyro) 와 speed 를 곱해 코너링 lateral reference 를 만든다.   */
    /*  - speed derivative 로 accel/brake reference 를 만든다.                 */
    /* ---------------------------------------------------------------------- */
    state->dyn.yaw_rate_dps_x10 = (int16_t)(motor_dyn_dot3(gx_dps, gy_dps, gz_dps,
                                                           s_runtime.zero_up_x_s, s_runtime.zero_up_y_s, s_runtime.zero_up_z_s) * 10.0f);

    speed_mps = (float)state->nav.speed_mmps / 1000.0f;
    yaw_rate_rad_s = ((float)state->dyn.yaw_rate_dps_x10 / 10.0f) * (float)(M_PI / 180.0f);
    lat_ref_g = (speed_mps * yaw_rate_rad_s) / 9.80665f;

    if (s_runtime.prev_speed_ms == 0u)
    {
        lon_ref_g = 0.0f;
    }
    else
    {
        lon_ref_g = ((speed_mps - s_runtime.prev_speed_mps) / dt_s) / 9.80665f;
    }

    s_runtime.prev_speed_mps = speed_mps;
    s_runtime.prev_speed_ms = now_ms;

    state->dyn.gnss_heading_valid = state->nav.heading_valid;
    if (state->nav.heading_valid)
    {
        state->dyn.heading_source = (uint8_t)APP_BIKE_HEADING_SOURCE_GNSS;
        state->dyn.heading_deg_x10 = (int16_t)state->nav.heading_deg_x10;
    }
    else
    {
        state->dyn.heading_source = (uint8_t)APP_BIKE_HEADING_SOURCE_NONE;
    }

    ref_blend = 0.0f;
    if ((bike_settings->gnss_aid_enabled != 0u) && (state->nav.speed_kmh_x10 >= bike_settings->gnss_min_speed_kmh_x10))
    {
        ref_blend = 0.04f;
    }

    state->dyn.lat_bias_mg = (int32_t)((float)state->dyn.lat_bias_mg + (((lat_imu_g - lat_ref_g) * 1000.0f) - (float)state->dyn.lat_bias_mg) * ref_blend);
    state->dyn.lon_bias_mg = (int32_t)((float)state->dyn.lon_bias_mg + (((lon_imu_g - lon_ref_g) * 1000.0f) - (float)state->dyn.lon_bias_mg) * ref_blend);

    lat_imu_g -= ((float)state->dyn.lat_bias_mg / 1000.0f);
    lon_imu_g -= ((float)state->dyn.lon_bias_mg / 1000.0f);

    lean_alpha = dt_s / motor_dyn_clampf((float)bike_settings->lean_display_tau_ms / 1000.0f, 0.05f, 5.0f);
    output_alpha = dt_s / motor_dyn_clampf((float)bike_settings->accel_display_tau_ms / 1000.0f, 0.05f, 5.0f);
    lean_alpha = motor_dyn_clampf(lean_alpha, 0.01f, 0.35f);
    output_alpha = motor_dyn_clampf(output_alpha, 0.01f, 0.35f);

    s_runtime.bank_display_deg += (bank_deg - s_runtime.bank_display_deg) * lean_alpha;
    s_runtime.grade_display_deg += (grade_deg - s_runtime.grade_display_deg) * lean_alpha;
    s_runtime.lat_display_g += (lat_imu_g - s_runtime.lat_display_g) * output_alpha;
    s_runtime.lon_display_g += (lon_imu_g - s_runtime.lon_display_g) * output_alpha;

    state->dyn.initialized = true;
    state->dyn.imu_valid = true;
    state->dyn.bank_deg_x10 = (int16_t)(s_runtime.bank_display_deg * 10.0f);
    state->dyn.grade_deg_x10 = (int16_t)(s_runtime.grade_display_deg * 10.0f);
    state->dyn.bank_rate_dps_x10 = (int16_t)(bank_rate_dps * 10.0f);
    state->dyn.grade_rate_dps_x10 = (int16_t)(grade_rate_dps * 10.0f);
    state->dyn.lat_accel_imu_mg = (int32_t)(lat_imu_g * 1000.0f);
    state->dyn.lon_accel_imu_mg = (int32_t)(lon_imu_g * 1000.0f);
    state->dyn.lat_accel_mg = (int32_t)(s_runtime.lat_display_g * 1000.0f);
    state->dyn.lon_accel_mg = (int32_t)(s_runtime.lon_display_g * 1000.0f);
    state->dyn.speed_source = (uint8_t)((state->vehicle.connected != false) ? APP_BIKE_SPEED_SOURCE_OBD : APP_BIKE_SPEED_SOURCE_GNSS);

    if (state->dyn.bank_deg_x10 > state->dyn.max_left_bank_deg_x10)
    {
        state->dyn.max_left_bank_deg_x10 = state->dyn.bank_deg_x10;
    }
    if (state->dyn.bank_deg_x10 < state->dyn.max_right_bank_deg_x10)
    {
        state->dyn.max_right_bank_deg_x10 = state->dyn.bank_deg_x10;
    }
    if (state->dyn.lat_accel_mg > state->dyn.max_left_lat_mg)
    {
        state->dyn.max_left_lat_mg = state->dyn.lat_accel_mg;
    }
    if (state->dyn.lat_accel_mg < state->dyn.max_right_lat_mg)
    {
        state->dyn.max_right_lat_mg = state->dyn.lat_accel_mg;
    }
    if (state->dyn.lon_accel_mg > state->dyn.max_accel_mg)
    {
        state->dyn.max_accel_mg = state->dyn.lon_accel_mg;
    }
    if (state->dyn.lon_accel_mg < state->dyn.max_brake_mg)
    {
        state->dyn.max_brake_mg = state->dyn.lon_accel_mg;
    }

    motor_dyn_update_history(&state->dyn, state->nav.altitude_cm);
}
