
#include "Motor_Record.h"

#include "Motor_State.h"

#include "APP_SD.h"
#include "fatfs.h"

#include <stdio.h>
#include <string.h>

static FIL s_log_file;
static uint8_t s_log_file_open;
static uint32_t s_last_nav_write_ms;
static uint32_t s_last_dyn_write_ms;
static uint32_t s_last_obd_write_ms;
static uint32_t s_last_distance_integrate_ms;
static uint32_t s_session_counter;

static FRESULT motor_record_write_payload(uint8_t type, uint32_t tick_ms, const void *payload, uint16_t payload_size)
{
    FRESULT fr;
    UINT written;
    motor_log_record_header_t hdr;

    if (s_log_file_open == 0u)
    {
        return FR_NOT_READY;
    }

    hdr.type = type;
    hdr.reserved0 = 0u;
    hdr.payload_size = payload_size;
    hdr.tick_ms = tick_ms;

    fr = f_write(&s_log_file, &hdr, sizeof(hdr), &written);
    if ((fr != FR_OK) || (written != sizeof(hdr)))
    {
        return (fr != FR_OK) ? fr : FR_DISK_ERR;
    }

    if ((payload != 0) && (payload_size != 0u))
    {
        fr = f_write(&s_log_file, payload, payload_size, &written);
        if ((fr != FR_OK) || (written != payload_size))
        {
            return (fr != FR_OK) ? fr : FR_DISK_ERR;
        }
    }

    return FR_OK;
}

static void motor_record_close_with_summary(motor_state_t *state)
{
    motor_log_sum_payload_t sum;

    if ((state == 0) || (s_log_file_open == 0u))
    {
        return;
    }

    memset(&sum, 0, sizeof(sum));
    sum.ride_seconds = state->session.ride_seconds;
    sum.moving_seconds = state->session.moving_seconds;
    sum.distance_m = state->session.distance_m;
    sum.max_speed_kmh_x10 = state->session.max_speed_kmh_x10;
    sum.max_left_bank_deg_x10 = state->dyn.max_left_bank_deg_x10;
    sum.max_right_bank_deg_x10 = state->dyn.max_right_bank_deg_x10;
    sum.max_left_lat_mg = state->dyn.max_left_lat_mg;
    sum.max_right_lat_mg = state->dyn.max_right_lat_mg;
    sum.max_accel_mg = state->dyn.max_accel_mg;
    sum.max_brake_mg = state->dyn.max_brake_mg;
    sum.marker_count = state->session.marker_count;
    sum.drop_count = (uint16_t)state->record.drop_count;

    (void)motor_record_write_payload(MOTOR_LOG_REC_SUM, state->now_ms, &sum, sizeof(sum));
    (void)f_sync(&s_log_file);
    (void)f_close(&s_log_file);
    s_log_file_open = 0u;
    state->record.graceful_close_done = true;
    state->record.state = (uint8_t)MOTOR_RECORD_STATE_IDLE;
}

static void motor_record_start(motor_state_t *state)
{
    FRESULT fr;
    char path[64];
    motor_log_header_payload_t hdr;

    if ((state == 0) || (s_log_file_open != 0u) || (APP_SD_IsFsAccessAllowedNow() == false))
    {
        return;
    }

    (void)f_mkdir("0:/MOTORLOG");

    s_session_counter++;
    if (state->snapshot.clock.local.year >= 2000u)
    {
        (void)snprintf(path,
                       sizeof(path),
                       "0:/MOTORLOG/%04u%02u%02u_%02u%02u%02u_%04lu.MLG",
                       (unsigned)state->snapshot.clock.local.year,
                       (unsigned)state->snapshot.clock.local.month,
                       (unsigned)state->snapshot.clock.local.day,
                       (unsigned)state->snapshot.clock.local.hour,
                       (unsigned)state->snapshot.clock.local.min,
                       (unsigned)state->snapshot.clock.local.sec,
                       (unsigned long)s_session_counter);
    }
    else
    {
        (void)snprintf(path, sizeof(path), "0:/MOTORLOG/SESSION_%04lu.MLG", (unsigned long)s_session_counter);
    }

    fr = f_open(&s_log_file, path, FA_WRITE | FA_CREATE_ALWAYS);
    if (fr != FR_OK)
    {
        state->record.state = (uint8_t)MOTOR_RECORD_STATE_ERROR;
        state->record.drop_count++;
        Motor_State_ShowToast("REC ERR", 1200u);
        return;
    }

    s_log_file_open = 1u;
    memset(&hdr, 0, sizeof(hdr));
    hdr.magic = MOTOR_LOG_MAGIC;
    hdr.version = MOTOR_LOG_VERSION;
    hdr.session_id = ++state->session.session_id;
    hdr.start_tick_ms = state->now_ms;
    hdr.receiver_profile = state->settings.gps.receiver_profile;
    hdr.units_preset = state->settings.units.preset;
    hdr.yaw_trim_deg_x10 = state->settings.dynamics.mount_yaw_trim_deg_x10;
    hdr.forward_axis = state->settings.dynamics.mount_forward_axis;
    hdr.left_axis = state->settings.dynamics.mount_left_axis;

    (void)motor_record_write_payload(MOTOR_LOG_REC_HDR, state->now_ms, &hdr, sizeof(hdr));
    (void)strncpy(state->record.file_name, path, sizeof(state->record.file_name) - 1u);
    state->record.state = (uint8_t)MOTOR_RECORD_STATE_RECORDING;
    state->record.graceful_close_done = false;
    state->record.bytes_written = 0u;
    state->record.last_open_ms = state->now_ms;
    state->record.record_sequence++;
    state->session.active = true;
    state->session.start_ms = state->now_ms;
    state->session.stop_ms = 0u;
    state->session.ride_seconds = 0u;
    state->session.moving_seconds = 0u;
    state->session.stopped_seconds = 0u;
    state->session.distance_m = 0u;
    state->session.marker_count = 0u;
    state->session.max_speed_kmh_x10 = 0u;
    s_last_nav_write_ms = 0u;
    s_last_dyn_write_ms = 0u;
    s_last_obd_write_ms = 0u;
    s_last_distance_integrate_ms = state->now_ms;
    Motor_State_ShowToast("REC START", 1200u);
}

void Motor_Record_Init(void)
{
    memset(&s_log_file, 0, sizeof(s_log_file));
    s_log_file_open = 0u;
    s_last_nav_write_ms = 0u;
    s_last_dyn_write_ms = 0u;
    s_last_obd_write_ms = 0u;
    s_last_distance_integrate_ms = 0u;
    s_session_counter = 0u;
}

void Motor_Record_RequestStart(void)
{
    motor_state_t *state;

    state = Motor_State_GetMutable();
    if (state != 0)
    {
        state->record.start_requested = true;
    }
}

void Motor_Record_RequestStop(void)
{
    motor_state_t *state;

    state = Motor_State_GetMutable();
    if (state != 0)
    {
        state->record.stop_requested = true;
    }
}

void Motor_Record_RequestMarker(void)
{
    motor_state_t *state;

    state = Motor_State_GetMutable();
    if (state != 0)
    {
        state->record.marker_requested = true;
    }
}

void Motor_Record_Task(uint32_t now_ms)
{
    motor_state_t *state;

    (void)now_ms;
    state = Motor_State_GetMutable();
    if (state == 0)
    {
        return;
    }
    if ((state->settings.recording.auto_start_enabled != 0u) &&
        ((motor_record_state_t)state->record.state == MOTOR_RECORD_STATE_IDLE) &&
        (state->nav.speed_kmh_x10 >= state->settings.recording.auto_start_speed_kmh_x10))
    {
        state->record.start_requested = true;
    }

    if (((motor_record_state_t)state->record.state == MOTOR_RECORD_STATE_RECORDING) &&
        (state->settings.recording.auto_start_enabled != 0u) &&
        (state->nav.moving == false) &&
        (state->settings.recording.auto_stop_idle_seconds != 0u) &&
        (state->session.stopped_seconds >= state->settings.recording.auto_stop_idle_seconds))
    {
        state->record.stop_requested = true;
    }

    if ((state->record.start_requested != false) &&
        ((motor_record_state_t)state->record.state != MOTOR_RECORD_STATE_RECORDING))
    {
        state->record.start_requested = false;
        motor_record_start(state);
    }

    if ((state->record.stop_requested != false) && (s_log_file_open != 0u))
    {
        state->record.stop_requested = false;
        state->record.state = (uint8_t)MOTOR_RECORD_STATE_CLOSING;
        state->session.stop_ms = state->now_ms;
        state->session.active = false;
        motor_record_close_with_summary(state);
        Motor_State_ShowToast("REC STOP", 1200u);
        return;
    }

    if ((motor_record_state_t)state->record.state == MOTOR_RECORD_STATE_PAUSED)
    {
        s_last_distance_integrate_ms = state->now_ms;
        return;
    }

    if ((motor_record_state_t)state->record.state != MOTOR_RECORD_STATE_RECORDING)
    {
        return;
    }

    if (state->nav.speed_kmh_x10 > state->session.max_speed_kmh_x10)
    {
        state->session.max_speed_kmh_x10 = state->nav.speed_kmh_x10;
    }

    if ((uint32_t)(state->now_ms - s_last_nav_write_ms) >= 100u)
    {
        motor_log_nav_payload_t nav;
        memset(&nav, 0, sizeof(nav));
        nav.lat_e7 = state->nav.lat_e7;
        nav.lon_e7 = state->nav.lon_e7;
        nav.altitude_cm = state->nav.altitude_cm;
        nav.speed_mmps = state->nav.speed_mmps;
        nav.heading_deg_x10 = state->nav.heading_deg_x10;
        nav.hacc_mm = state->nav.hacc_mm;
        nav.vacc_mm = state->nav.vacc_mm;
        nav.fix_type = state->nav.fix_type;
        nav.sats_used = state->nav.sats_used;
        if (motor_record_write_payload(MOTOR_LOG_REC_NAV, state->now_ms, &nav, sizeof(nav)) != FR_OK)
        {
            state->record.drop_count++;
        }
        s_last_nav_write_ms = state->now_ms;
    }

    if ((uint32_t)(state->now_ms - s_last_dyn_write_ms) >= 50u)
    {
        motor_log_dyn_payload_t dyn;
        memset(&dyn, 0, sizeof(dyn));
        dyn.bank_deg_x10 = state->dyn.bank_deg_x10;
        dyn.grade_deg_x10 = state->dyn.grade_deg_x10;
        dyn.bank_rate_dps_x10 = state->dyn.bank_rate_dps_x10;
        dyn.grade_rate_dps_x10 = state->dyn.grade_rate_dps_x10;
        dyn.lat_accel_mg = state->dyn.lat_accel_mg;
        dyn.lon_accel_mg = state->dyn.lon_accel_mg;
        dyn.confidence_permille = state->dyn.confidence_permille;
        dyn.speed_source = state->dyn.speed_source;
        dyn.heading_source = state->dyn.heading_source;
        if (motor_record_write_payload(MOTOR_LOG_REC_DYN, state->now_ms, &dyn, sizeof(dyn)) != FR_OK)
        {
            state->record.drop_count++;
        }
        s_last_dyn_write_ms = state->now_ms;
    }

    if ((state->vehicle.connected != false) && ((uint32_t)(state->now_ms - s_last_obd_write_ms) >= 200u))
    {
        motor_log_obd_payload_t obd;
        memset(&obd, 0, sizeof(obd));
        obd.rpm = state->vehicle.rpm;
        obd.coolant_temp_c_x10 = state->vehicle.coolant_temp_c_x10;
        obd.gear = state->vehicle.gear;
        obd.battery_mv = state->vehicle.battery_mv;
        obd.throttle_percent = state->vehicle.throttle_percent;
        obd.dtc_count = state->vehicle.dtc_count;
        if (motor_record_write_payload(MOTOR_LOG_REC_OBD, state->now_ms, &obd, sizeof(obd)) != FR_OK)
        {
            state->record.drop_count++;
        }
        s_last_obd_write_ms = state->now_ms;
    }

    if (state->record.marker_requested != false)
    {
        motor_log_evt_payload_t evt;
        memset(&evt, 0, sizeof(evt));
        evt.event_code = 1u;
        evt.event_value = 0u;
        evt.aux_u32 = state->session.marker_count;
        if (motor_record_write_payload(MOTOR_LOG_REC_EVT, state->now_ms, &evt, sizeof(evt)) == FR_OK)
        {
            state->session.marker_count++;
            Motor_State_ShowToast("MARK", 900u);
        }
        else
        {
            state->record.drop_count++;
        }
        state->record.marker_requested = false;
    }

    /* ---------------------------------------------------------------------- */
    /*  session distance는 GNSS speed 적분 기반으로 누적한다.                  */
    /*  - dt는 실제 now_ms 차이로 계산한다.                                     */
    /*  - upper app layer이므로 raw position 적분 대신                          */
    /*    APP_STATE snapshot의 정규화된 speed_mmps만 사용한다.                  */
    /* ---------------------------------------------------------------------- */
    if (s_last_distance_integrate_ms == 0u)
    {
        s_last_distance_integrate_ms = state->now_ms;
    }
    else if (state->now_ms > s_last_distance_integrate_ms)
    {
        uint32_t dt_ms;

        dt_ms = state->now_ms - s_last_distance_integrate_ms;
        if (dt_ms > 1000u)
        {
            dt_ms = 1000u;
        }

        state->session.distance_m += (uint32_t)(((uint64_t)state->nav.speed_mmps * dt_ms) / 1000u / 1000u);
        s_last_distance_integrate_ms = state->now_ms;
    }
}
