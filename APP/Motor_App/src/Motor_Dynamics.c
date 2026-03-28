#include "Motor_Dynamics.h"

#include "BIKE_DYNAMICS.h"
#include "Motor_State.h"

#include <stdio.h>
#include <string.h>

typedef enum
{
    MOTOR_DYN_CAL_FLOW_NONE = 0u,
    MOTOR_DYN_CAL_FLOW_BOOT_GYRO,
    MOTOR_DYN_CAL_FLOW_BOOT_ZERO,
    MOTOR_DYN_CAL_FLOW_MANUAL_GYRO,
    MOTOR_DYN_CAL_FLOW_MANUAL_ZERO
} motor_dyn_cal_flow_t;

typedef struct
{
    uint32_t last_shared_update_ms;
    uint32_t last_zero_capture_ms;
    uint32_t last_zero_request_count;
    uint32_t last_gyro_bias_cal_ms;
    uint32_t cal_flow_start_ms;
    uint32_t cal_popup_last_show_ms;
    uint32_t cal_expected_zero_capture_ms;
    uint32_t cal_expected_gyro_cal_ms;
    uint8_t  last_zero_valid;
    uint8_t  last_gyro_bias_cal_active;
    uint8_t  cal_flow;
    uint8_t  cal_request_sent;
} motor_dynamics_runtime_t;

static motor_dynamics_runtime_t s_runtime;

#define MOTOR_DYN_CAL_POPUP_HOLD_MS           1200u
#define MOTOR_DYN_CAL_POPUP_REFRESH_MS          400u
#define MOTOR_DYN_MANUAL_ZERO_POPUP_MAX_MS    12000u
#define MOTOR_DYN_BOOT_IMU_READY_STALE_MS       500u
#define MOTOR_DYN_BOOT_ZERO_HINT_ESCALATE_MS   8000u
#define MOTOR_DYN_ZERO_HINT_ACCEL_MG            220
#define MOTOR_DYN_ZERO_HINT_JERK_MG_PER_S      3500
#define MOTOR_DYN_ZERO_HINT_MIN_TRUST           250u
#define MOTOR_DYN_ZERO_HINT_STOP_KMH_X10         10u

static void motor_dyn_reset_history(motor_dynamics_state_t *dyn)
{
    if (dyn == 0)
    {
        return;
    }

    memset(dyn->bank_history_x10, 0, sizeof(dyn->bank_history_x10));
    memset(dyn->lat_history_x10, 0, sizeof(dyn->lat_history_x10));
    memset(dyn->alt_history_m, 0, sizeof(dyn->alt_history_m));
    memset(dyn->grade_history_x10, 0, sizeof(dyn->grade_history_x10));
    dyn->history_head = 0u;
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
    dyn->grade_history_x10[idx] = dyn->grade_deg_x10;
    dyn->history_head = (uint16_t)((idx + 1u) % MOTOR_HISTORY_SAMPLE_COUNT);
}

static void motor_dyn_copy_shared_truth(motor_dynamics_state_t *dyn, const app_bike_state_t *bike)
{
    if ((dyn == 0) || (bike == 0))
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  low-level canonical flags / routing                                    */
    /* ---------------------------------------------------------------------- */
    dyn->initialized = bike->initialized;
    dyn->zero_valid = bike->zero_valid;
    dyn->imu_valid = bike->imu_valid;
    dyn->gnss_aid_valid = bike->gnss_aid_valid;
    dyn->gnss_heading_valid = bike->gnss_heading_valid;
    dyn->obd_speed_valid = bike->obd_speed_valid;
    dyn->confidence_permille = bike->confidence_permille;
    dyn->speed_source = bike->speed_source;
    dyn->estimator_mode = bike->estimator_mode;
    dyn->heading_source = bike->heading_source;
    dyn->last_source_update_ms = bike->last_update_ms;
    dyn->last_zero_capture_ms = bike->last_zero_capture_ms;

    /* ---------------------------------------------------------------------- */
    /*  canonical estimator output                                             */
    /*                                                                        */
    /*  Motor logger / peak detector 는 이 est_* 층을 기준으로 동작한다.        */
    /*  Motor_App는 더 이상 raw IMU를 직접 읽어 estimator를 재구현하지 않고,   */
    /*  shared BIKE_DYNAMICS가 publish 한 결과를 그대로 신뢰한다.             */
    /* ---------------------------------------------------------------------- */
    dyn->est_bank_deg_x10 = bike->bank_raw_deg_x10;
    dyn->est_grade_deg_x10 = bike->grade_raw_deg_x10;
    dyn->est_bank_rate_dps_x10 = bike->bank_rate_dps_x10;
    dyn->est_grade_rate_dps_x10 = bike->grade_rate_dps_x10;
    dyn->est_heading_deg_x10 = bike->heading_deg_x10;
    dyn->est_yaw_rate_dps_x10 = bike->yaw_rate_dps_x10;
    dyn->est_lat_accel_mg = bike->lat_accel_est_mg;
    dyn->est_lon_accel_mg = bike->lon_accel_est_mg;
    dyn->est_lat_accel_imu_mg = bike->lat_accel_imu_mg;
    dyn->est_lon_accel_imu_mg = bike->lon_accel_imu_mg;
    dyn->lat_bias_mg = bike->lat_bias_mg;
    dyn->lon_bias_mg = bike->lon_bias_mg;

    /* ---------------------------------------------------------------------- */
    /*  display-facing mirror                                                  */
    /*                                                                        */
    /*  live UI는 rider-facing display layer를 그대로 사용한다.               */
    /*  즉, logger / peak detector 용 canonical est_* 와                      */
    /*  화면 표시용 bank_deg_x10 / lat_accel_mg 층이 명확히 분리된다.          */
    /* ---------------------------------------------------------------------- */
    dyn->bank_deg_x10 = bike->banking_angle_deg_x10;
    dyn->grade_deg_x10 = bike->grade_deg_x10;
    dyn->bank_rate_dps_x10 = dyn->est_bank_rate_dps_x10;
    dyn->grade_rate_dps_x10 = dyn->est_grade_rate_dps_x10;
    dyn->heading_deg_x10 = dyn->est_heading_deg_x10;
    dyn->yaw_rate_dps_x10 = dyn->est_yaw_rate_dps_x10;
    dyn->lat_accel_mg = bike->lat_accel_mg;
    dyn->lon_accel_mg = bike->lon_accel_mg;
    dyn->lat_accel_imu_mg = dyn->est_lat_accel_imu_mg;
    dyn->lon_accel_imu_mg = dyn->est_lon_accel_imu_mg;
}

static void motor_dyn_update_peaks_from_estimator(motor_dynamics_state_t *dyn)
{
    if (dyn == 0)
    {
        return;
    }

    if (dyn->est_bank_deg_x10 > dyn->max_left_bank_deg_x10)
    {
        dyn->max_left_bank_deg_x10 = dyn->est_bank_deg_x10;
    }
    if (dyn->est_bank_deg_x10 < dyn->max_right_bank_deg_x10)
    {
        dyn->max_right_bank_deg_x10 = dyn->est_bank_deg_x10;
    }
    if (dyn->est_lat_accel_mg > dyn->max_left_lat_mg)
    {
        dyn->max_left_lat_mg = dyn->est_lat_accel_mg;
    }
    if (dyn->est_lat_accel_mg < dyn->max_right_lat_mg)
    {
        dyn->max_right_lat_mg = dyn->est_lat_accel_mg;
    }
    if (dyn->est_lon_accel_mg > dyn->max_accel_mg)
    {
        dyn->max_accel_mg = dyn->est_lon_accel_mg;
    }
    if (dyn->est_lon_accel_mg < dyn->max_brake_mg)
    {
        dyn->max_brake_mg = dyn->est_lon_accel_mg;
    }
}

static void motor_dyn_start_cal_flow(motor_dyn_cal_flow_t flow,
                                     uint32_t now_ms,
                                     const app_bike_state_t *bike)
{
    s_runtime.cal_flow = (uint8_t)flow;
    s_runtime.cal_flow_start_ms = now_ms;
    s_runtime.cal_popup_last_show_ms = 0u;
    s_runtime.cal_expected_zero_capture_ms = (bike != 0) ? bike->last_zero_capture_ms : 0u;
    s_runtime.cal_expected_gyro_cal_ms = (bike != 0) ? bike->last_gyro_bias_cal_ms : 0u;

    /* ---------------------------------------------------------------------- */
    /*  boot flow 는 여기서 실제 low-level request를 보낸다.                   */
    /*  manual flow 는 이미 버튼/상위 요청이 내려간 뒤 이를 관찰한 것이므로     */
    /*  추가 request를 중복 발행하지 않는다.                                   */
    /* ---------------------------------------------------------------------- */
    switch (flow)
    {
    case MOTOR_DYN_CAL_FLOW_MANUAL_GYRO:
    case MOTOR_DYN_CAL_FLOW_MANUAL_ZERO:
        s_runtime.cal_request_sent = 1u;
        break;

    case MOTOR_DYN_CAL_FLOW_BOOT_GYRO:
    case MOTOR_DYN_CAL_FLOW_BOOT_ZERO:
    default:
        s_runtime.cal_request_sent = 0u;
        break;
    }
}

static void motor_dyn_finish_cal_flow(const char *toast_text)
{
    s_runtime.cal_flow = (uint8_t)MOTOR_DYN_CAL_FLOW_NONE;
    s_runtime.cal_request_sent = 0u;
    s_runtime.cal_flow_start_ms = 0u;
    s_runtime.cal_popup_last_show_ms = 0u;
    s_runtime.cal_expected_zero_capture_ms = 0u;
    s_runtime.cal_expected_gyro_cal_ms = 0u;

    Motor_State_HidePopup();
    if (toast_text != 0)
    {
        Motor_State_ShowToast(toast_text, 1400u);
    }
}

static void motor_dyn_show_cal_popup(uint32_t now_ms,
                                     const char *title,
                                     const char *line1,
                                     const char *line2)
{
    if ((s_runtime.cal_popup_last_show_ms == 0u) ||
        ((uint32_t)(now_ms - s_runtime.cal_popup_last_show_ms) >= MOTOR_DYN_CAL_POPUP_REFRESH_MS))
    {
        Motor_State_ShowPopup(title, line1, line2, MOTOR_DYN_CAL_POPUP_HOLD_MS);
        s_runtime.cal_popup_last_show_ms = now_ms;
    }
}

static bool motor_dyn_is_imu_ready(uint32_t now_ms, const app_bike_state_t *bike)
{
    if (bike == 0)
    {
        return false;
    }

    /* ---------------------------------------------------------------------- */
    /*  boot gyro calibration은 실제로 fresh IMU sample이 돌고 있을 때만         */
    /*  시작해야 한다.                                                         */
    /*                                                                        */
    /*  그렇지 않으면 low-level state machine이 active 상태로만 머문 채        */
    /*  progress 0% timeout -> 재시도 루프에 들어갈 수 있다.                   */
    /* ---------------------------------------------------------------------- */
    if (bike->imu_valid == false)
    {
        return false;
    }

    if (bike->last_imu_update_ms == 0u)
    {
        return false;
    }

    if ((uint32_t)(now_ms - bike->last_imu_update_ms) > MOTOR_DYN_BOOT_IMU_READY_STALE_MS)
    {
        return false;
    }

    return true;
}

static int32_t motor_dyn_abs_i32(int32_t value)
{
    return (value < 0) ? -value : value;
}

static void motor_dyn_set_line2(char *dst, size_t dst_size, const char *src)
{
    if ((dst == 0) || (dst_size == 0u))
    {
        return;
    }

    if (src == 0)
    {
        dst[0] = '\0';
        return;
    }

    (void)snprintf(dst, dst_size, "%s", src);
}

static const char *motor_dyn_format_zero_popup(uint32_t now_ms,
                                               const app_bike_state_t *bike,
                                               char *line2,
                                               size_t line2_size)
{
    if (motor_dyn_is_imu_ready(now_ms, bike) == false)
    {
        motor_dyn_set_line2(line2, line2_size, "CHECK IMU DATA");
        return "WAITING FOR IMU";
    }

    if ((uint32_t)motor_dyn_abs_i32((int32_t)bike->speed_kmh_x10) > MOTOR_DYN_ZERO_HINT_STOP_KMH_X10)
    {
        motor_dyn_set_line2(line2, line2_size, "STOP VEHICLE");
        return "LEVEL BIKE FOR ZERO";
    }

    if (motor_dyn_abs_i32((int32_t)bike->imu_accel_norm_mg - 1000) > MOTOR_DYN_ZERO_HINT_ACCEL_MG)
    {
        motor_dyn_set_line2(line2, line2_size, "CHECK MOUNT ANGLE");
        return "KEEP BIKE LEVEL";
    }

    if (motor_dyn_abs_i32(bike->imu_jerk_mg_per_s) > MOTOR_DYN_ZERO_HINT_JERK_MG_PER_S)
    {
        motor_dyn_set_line2(line2, line2_size, "WAIT FOR STABLE HOLD");
        return "HOLD MORE STILL";
    }

    if (bike->imu_attitude_trust_permille < MOTOR_DYN_ZERO_HINT_MIN_TRUST)
    {
        motor_dyn_set_line2(line2, line2_size, "KEEP BIKE STILL");
        return "WAIT ATTITUDE SETTLE";
    }

    if ((uint32_t)(now_ms - s_runtime.cal_flow_start_ms) >= MOTOR_DYN_BOOT_ZERO_HINT_ESCALATE_MS)
    {
        motor_dyn_set_line2(line2, line2_size, "VERIFY AXIS SETUP");
        return "CHECK AXIS / MOUNT";
    }

    motor_dyn_set_line2(line2, line2_size, "HOLD STILL 1.5s");
    return "LEVEL BIKE FOR ZERO";
}


static void motor_dyn_run_calibration_supervisor(uint32_t now_ms,
                                                 const app_bike_state_t *bike)
{
    char line2[32];

    if (bike == 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  manual request 감지                                                     */
    /*                                                                        */
    /*  zero capture 는 public counter가 있으므로 이를 edge-trigger로 사용한다. */
    /*  gyro cal 은 active rising edge를 수동 시작 신호로 본다.                */
    /*                                                                        */
    /*  주의                                                                   */
    /*  - boot flow가 자기 자신이 발행한 request 때문에 manual flow로          */
    /*    오인되지 않도록, 현재 flow가 NONE 일 때만 새 manual flow를 연다.     */
    /* ---------------------------------------------------------------------- */
    if (bike->zero_request_count != s_runtime.last_zero_request_count)
    {
        s_runtime.last_zero_request_count = bike->zero_request_count;

        if ((motor_dyn_cal_flow_t)s_runtime.cal_flow == MOTOR_DYN_CAL_FLOW_NONE)
        {
            motor_dyn_start_cal_flow(MOTOR_DYN_CAL_FLOW_MANUAL_ZERO, now_ms, bike);
        }
    }

    if ((bike->gyro_bias_cal_active != false) && (s_runtime.last_gyro_bias_cal_active == 0u))
    {
        if ((motor_dyn_cal_flow_t)s_runtime.cal_flow == MOTOR_DYN_CAL_FLOW_NONE)
        {
            motor_dyn_start_cal_flow(MOTOR_DYN_CAL_FLOW_MANUAL_GYRO, now_ms, bike);
        }
    }

    /* ---------------------------------------------------------------------- */
    /*  boot-time / zero-invalid supervisor                                    */
    /*                                                                        */
    /*  정책                                                                   */
    /*  1) gyro bias 가 한 번도 유효하지 않으면 그것을 먼저 잡는다.             */
    /*  2) gyro bias 가 준비되면 zero capture 를 수행한다.                     */
    /*  3) zero_valid 전에는 BIKE_DYNAMICS publish가 화면/기록을 잠그므로,      */
    /*     사용자는 반드시 popup을 보고 캘리 동작을 이해하게 된다.             */
    /* ---------------------------------------------------------------------- */
    if ((motor_dyn_cal_flow_t)s_runtime.cal_flow == MOTOR_DYN_CAL_FLOW_NONE)
    {
        if (bike->gyro_bias_valid == false)
        {
            motor_dyn_start_cal_flow(MOTOR_DYN_CAL_FLOW_BOOT_GYRO, now_ms, bike);
        }
        else if (bike->zero_valid == false)
        {
            motor_dyn_start_cal_flow(MOTOR_DYN_CAL_FLOW_BOOT_ZERO, now_ms, bike);
        }
    }

    switch ((motor_dyn_cal_flow_t)s_runtime.cal_flow)
    {
    case MOTOR_DYN_CAL_FLOW_BOOT_GYRO:
        if (motor_dyn_is_imu_ready(now_ms, bike) == false)
        {
            /* ------------------------------------------------------------------ */
            /*  IMU sample이 아직 준비되지 않은 동안에는 calibration request를      */
            /*  실제로 발행하지 않는다.                                             */
            /*                                                                    */
            /*  이렇게 해야 첫 부팅 직후 또는 IMU stream 공백 구간에서             */
            /*  low-level gyro cal이 active 0%% timeout 상태로 들어가는 것을        */
            /*  막을 수 있다.                                                      */
            /* ------------------------------------------------------------------ */
            s_runtime.cal_request_sent = 0u;
            motor_dyn_show_cal_popup(now_ms,
                                     "CALIB REQUIRED!",
                                     "WAITING FOR IMU",
                                     "CHECK IMU DATA");
            break;
        }

        if ((bike->gyro_bias_valid == false) &&
            (bike->gyro_bias_cal_active == false) &&
            (s_runtime.cal_request_sent == 0u))
        {
            Motor_Dynamics_RequestGyroBiasCalibration();
            s_runtime.cal_request_sent = 1u;
            s_runtime.cal_expected_gyro_cal_ms = bike->last_gyro_bias_cal_ms;
        }

        if ((bike->gyro_bias_valid != false) && (bike->gyro_bias_cal_active == false))
        {
            motor_dyn_start_cal_flow(MOTOR_DYN_CAL_FLOW_BOOT_ZERO, now_ms, bike);
            break;
        }

        if ((s_runtime.cal_request_sent != 0u) &&
            (bike->gyro_bias_cal_active == false) &&
            (bike->last_gyro_bias_cal_ms != s_runtime.cal_expected_gyro_cal_ms) &&
            (bike->gyro_bias_valid == false))
        {
            /* ------------------------------------------------------------------ */
            /*  이전 시도가 timeout/실패로 종료되었으므로 다시 request를 열어 둔다. */
            /*  단, fresh IMU가 유지되는 동안에만 다시 시작되므로                  */
            /*  progress 0%% 고착 루프는 여기서 끊긴다.                             */
            /* ------------------------------------------------------------------ */
            s_runtime.cal_request_sent = 0u;
            s_runtime.cal_expected_gyro_cal_ms = bike->last_gyro_bias_cal_ms;
        }

        if (bike->gyro_bias_cal_active != false)
        {
            (void)snprintf(line2,
                           sizeof(line2),
                           "GYRO %u%%",
                           (unsigned)(bike->gyro_bias_cal_progress_permille / 10u));
        }
        else
        {
            (void)snprintf(line2, sizeof(line2), "DO NOT TOUCH DEVICE");
        }

        motor_dyn_show_cal_popup(now_ms,
                                 "CALIB REQUIRED!",
                                 "KEEP BIKE STILL",
                                 line2);
        break;

    case MOTOR_DYN_CAL_FLOW_BOOT_ZERO:
    {
        const char *line1_ptr;

        if (bike->zero_valid != false)
        {
            motor_dyn_finish_cal_flow("CAL OK");
            break;
        }

        if (motor_dyn_is_imu_ready(now_ms, bike) == false)
        {
            s_runtime.cal_request_sent = 0u;
            motor_dyn_show_cal_popup(now_ms,
                                     "CALIB REQUIRED!",
                                     "WAITING FOR IMU",
                                     "CHECK IMU DATA");
            break;
        }

        if (s_runtime.cal_request_sent == 0u)
        {
            Motor_Dynamics_RequestZeroCapture();
            s_runtime.cal_request_sent = 1u;
            s_runtime.cal_expected_zero_capture_ms = bike->last_zero_capture_ms;
        }

        line1_ptr = motor_dyn_format_zero_popup(now_ms, bike, line2, sizeof(line2));
        motor_dyn_show_cal_popup(now_ms,
                                 "CALIB REQUIRED!",
                                 line1_ptr,
                                 line2);
        break;
    }

    case MOTOR_DYN_CAL_FLOW_MANUAL_GYRO:
        if ((bike->gyro_bias_valid != false) &&
            (bike->gyro_bias_cal_active == false) &&
            (bike->last_gyro_bias_cal_ms != 0u))
        {
            motor_dyn_finish_cal_flow("GYRO CAL OK");
            break;
        }

        if ((bike->gyro_bias_cal_active == false) &&
            (bike->last_gyro_bias_cal_ms != s_runtime.cal_expected_gyro_cal_ms) &&
            (bike->gyro_bias_valid == false))
        {
            motor_dyn_finish_cal_flow("GYRO CAL FAIL");
            break;
        }

        if (bike->gyro_bias_cal_active != false)
        {
            (void)snprintf(line2,
                           sizeof(line2),
                           "GYRO %u%%",
                           (unsigned)(bike->gyro_bias_cal_progress_permille / 10u));
        }
        else
        {
            (void)snprintf(line2, sizeof(line2), "HOLD STILL");
        }

        motor_dyn_show_cal_popup(now_ms,
                                 "GYRO CAL",
                                 "KEEP BIKE STILL",
                                 line2);
        break;

    case MOTOR_DYN_CAL_FLOW_MANUAL_ZERO:
    {
        const char *line1_ptr;

        if (bike->last_zero_capture_ms != s_runtime.cal_expected_zero_capture_ms)
        {
            motor_dyn_finish_cal_flow("ZERO OK");
            break;
        }

        /* ------------------------------------------------------------------ */
        /*  manual zero 는 low-level에서 timeout 없이 pending 될 수 있다.       */
        /*  따라서 popup을 무한정 붙잡아 두지 않고, 일정 시간이 지나면          */
        /*  "요청은 남겨 두되 UI popup만 정리" 하는 쪽으로 UX를 완화한다.      */
        /*  이후 사용자가 실제로 level/stable 상태를 만들면 zero capture는      */
        /*  여전히 low-level state machine 안에서 정상적으로 완료될 수 있다.    */
        /* ------------------------------------------------------------------ */
        if ((uint32_t)(now_ms - s_runtime.cal_flow_start_ms) >= MOTOR_DYN_MANUAL_ZERO_POPUP_MAX_MS)
        {
            motor_dyn_finish_cal_flow("ZERO PENDING");
            break;
        }

        line1_ptr = motor_dyn_format_zero_popup(now_ms, bike, line2, sizeof(line2));
        motor_dyn_show_cal_popup(now_ms,
                                 "ZERO CAPTURE",
                                 line1_ptr,
                                 line2);
        break;
    }

    case MOTOR_DYN_CAL_FLOW_NONE:
    default:
        break;
    }

    s_runtime.last_gyro_bias_cal_active = (bike->gyro_bias_cal_active != false) ? 1u : 0u;
    s_runtime.last_gyro_bias_cal_ms = bike->last_gyro_bias_cal_ms;
}

void Motor_Dynamics_Init(void)
{
    memset(&s_runtime, 0, sizeof(s_runtime));
}

void Motor_Dynamics_RequestZeroCapture(void)
{
    /* ---------------------------------------------------------------------- */
    /*  수동 zero capture는 shared low-level 서비스에 그대로 위임한다.         */
    /*  해당 서비스 내부에서 settle + good-sample dwell 상태기계가 동작한다.  */
    /* ---------------------------------------------------------------------- */
    BIKE_DYNAMICS_RequestZeroCapture();
}

void Motor_Dynamics_RequestHardRezero(void)
{
    BIKE_DYNAMICS_RequestHardRezero();
}

void Motor_Dynamics_RequestGyroBiasCalibration(void)
{
    BIKE_DYNAMICS_RequestGyroBiasCalibration();
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
    motor_dynamics_state_t *dyn;
    const app_bike_state_t *bike;

    (void)now_ms;

    state = Motor_State_GetMutable();
    if (state == 0)
    {
        return;
    }

    dyn = &state->dyn;
    bike = &state->snapshot.bike;

    /* ---------------------------------------------------------------------- */
    /*  shared low-level bike truth -> Motor high-level mirror                 */
    /* ---------------------------------------------------------------------- */
    motor_dyn_copy_shared_truth(dyn, bike);

    /* ---------------------------------------------------------------------- */
    /*  calibration popup / boot supervisor                                    */
    /*                                                                        */
    /*  Dynamics adapter가 low-level snapshot을 가장 먼저 보는 상위 경로이므로, */
    /*  calibration UX를 여기서 관리하면 화면 종류와 무관하게 동일하게 동작한다.*/
    /* ---------------------------------------------------------------------- */
    motor_dyn_run_calibration_supervisor(now_ms, bike);

    /* ---------------------------------------------------------------------- */
    /*  zero_valid가 내려간 순간 history를 즉시 비운다.                        */
    /*                                                                        */
    /*  이유                                                                   */
    /*  - hard rezero 직후 이전 주행의 bank/G trace가 화면에 남아 있으면        */
    /*    사용자는 "현재 값이 아직 살아 있다" 고 오해하기 쉽다.               */
    /*  - 따라서 zero 기준이 무효화되면 history는 즉시 빈 상태로 되돌린다.      */
    /* ---------------------------------------------------------------------- */
    if ((dyn->zero_valid == false) && (s_runtime.last_zero_valid != 0u))
    {
        motor_dyn_reset_history(dyn);
    }
    s_runtime.last_zero_valid = (dyn->zero_valid != false) ? 1u : 0u;

    /* ---------------------------------------------------------------------- */
    /*  실제 zero capture 완료 시점에만 session peak / history 를 초기화한다.  */
    /*                                                                        */
    /*  중요한 이유                                                            */
    /*  - 버튼 요청 시점이 아니라, low-level zero state machine이               */
    /*    "안정 조건 만족 후 실제 capture를 끝낸 시점"을 기준으로             */
    /*    reset이 일어나야 의미가 맞다.                                        */
    /* ---------------------------------------------------------------------- */
    if ((bike->last_zero_capture_ms != 0u) &&
        (bike->last_zero_capture_ms != s_runtime.last_zero_capture_ms))
    {
        s_runtime.last_zero_capture_ms = bike->last_zero_capture_ms;
        Motor_Dynamics_ResetSessionPeaks();
        motor_dyn_reset_history(dyn);
    }

    /* ---------------------------------------------------------------------- */
    /*  history / peak는 shared source timestamp가 바뀐 경우에만 1회 적재한다. */
    /*                                                                        */
    /*  이렇게 하면 menu/settings 화면에서도 Dynamics 자체는 절대 멈추지       */
    /*  않지만, 같은 샘플을 여러 번 중복 기록하는 일은 피할 수 있다.          */
    /* ---------------------------------------------------------------------- */
    /* ---------------------------------------------------------------------- */
    /*  dedupe 기준은 task tick이 아니라 실제 IMU sample timestamp 여야 한다.   */
    /*                                                                        */
    /*  BIKE_DYNAMICS는 매 task에서 last_update_ms를 갱신하므로,                */
    /*  그 값을 그대로 쓰면 같은 IMU 샘플을 여러 superloop에서 중복 소비할 수   */
    /*  있다. 따라서 IMU sample stamp가 있으면 그것을 우선 사용한다.           */
    /* ---------------------------------------------------------------------- */
    {
        uint32_t sample_stamp_ms = (bike->last_imu_update_ms != 0u) ? bike->last_imu_update_ms
                                                                    : bike->last_update_ms;
        if ((sample_stamp_ms == 0u) || (sample_stamp_ms == s_runtime.last_shared_update_ms))
        {
            return;
        }

        s_runtime.last_shared_update_ms = sample_stamp_ms;
    }

    if (dyn->imu_valid == false)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  pre-zero display/history 오염 차단                                     */
    /*                                                                        */
    /*  이전 버전은 provisional lean/G를 history에 적재해서 부팅 직후 garbage  */
    /*  값이 graph / corner trace에 남을 수 있었다.                            */
    /*  현재는 low-level publish 자체가 zero 전 출력을 0으로 잠그고,           */
    /*  high-level history 역시 zero_valid 이후부터만 열어                       */
    /*  UI/기록 경로가 모두 같은 계약을 따르도록 맞춘다.                       */
    /* ---------------------------------------------------------------------- */
    if (dyn->zero_valid == false)
    {
        return;
    }

    motor_dyn_update_history(dyn, state->nav.altitude_cm);
    motor_dyn_update_peaks_from_estimator(dyn);
}
