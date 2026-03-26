#include "Motor_Dynamics.h"

#include "BIKE_DYNAMICS.h"
#include "Motor_State.h"

#include <string.h>

typedef struct
{
    uint32_t last_shared_update_ms;
    uint32_t last_zero_capture_ms;
} motor_dynamics_runtime_t;

static motor_dynamics_runtime_t s_runtime;

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
    /*  실제 zero capture 완료 시점에만 session peak 를 초기화한다.            */
    /*                                                                        */
    /*  중요한 이유                                                            */
    /*  - 버튼 요청 시점이 아니라, low-level zero state machine이               */
    /*    "안정 조건 만족 후 실제 capture를 끝낸 시점"을 기준으로             */
    /*    peak reset이 일어나야 의미가 맞다.                                  */
    /* ---------------------------------------------------------------------- */
    if ((bike->last_zero_capture_ms != 0u) &&
        (bike->last_zero_capture_ms != s_runtime.last_zero_capture_ms))
    {
        s_runtime.last_zero_capture_ms = bike->last_zero_capture_ms;
        Motor_Dynamics_ResetSessionPeaks();
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

    /* ---------------------------------------------------------------------- */
    /*  history는 display layer의 일부이므로 zero_valid 전에도 살아 있어야 한다.*/
    /*                                                                        */
    /*  즉, 사용자는 부팅 직후에도 lean / G 변화가 화면에서 보인다.            */
    /*  다만 peak 는 확정 zero 기준이 서기 전까지 절대 누적하지 않는다.         */
    /* ---------------------------------------------------------------------- */
    if (dyn->imu_valid == false)
    {
        return;
    }

    motor_dyn_update_history(dyn, state->nav.altitude_cm);

    if (dyn->zero_valid == false)
    {
        return;
    }

    motor_dyn_update_peaks_from_estimator(dyn);
}
