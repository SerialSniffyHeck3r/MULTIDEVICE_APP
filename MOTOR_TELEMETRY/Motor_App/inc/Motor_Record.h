
#ifndef MOTOR_RECORD_H
#define MOTOR_RECORD_H

#include "Motor_Model.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  Motor binary log format                                                    */
/*                                                                            */
/*  파일은 가변 길이 record stream 으로 저장한다.                              */
/*  각 record는 header(type, size, tick_ms) + payload 구조를 갖는다.          */
/* -------------------------------------------------------------------------- */
#define MOTOR_LOG_MAGIC   0x4D4C4F47u  /* 'MLOG' */
#define MOTOR_LOG_VERSION 0x00010000u

typedef enum
{
    MOTOR_LOG_REC_HDR = 1u,
    MOTOR_LOG_REC_NAV = 2u,
    MOTOR_LOG_REC_DYN = 3u,
    MOTOR_LOG_REC_EVT = 4u,
    MOTOR_LOG_REC_OBD = 5u,
    MOTOR_LOG_REC_SUM = 6u
} motor_log_record_type_t;

typedef struct
{
    uint8_t  type;
    uint8_t  reserved0;
    uint16_t payload_size;
    uint32_t tick_ms;
} motor_log_record_header_t;

typedef struct
{
    uint32_t magic;
    uint32_t version;
    uint32_t session_id;
    uint32_t start_tick_ms;
    uint8_t  receiver_profile;
    uint8_t  units_preset;
    int16_t  yaw_trim_deg_x10;
    uint8_t  forward_axis;
    uint8_t  left_axis;
    uint8_t  reserved0[2];
} motor_log_header_payload_t;

typedef struct
{
    int32_t lat_e7;
    int32_t lon_e7;
    int32_t altitude_cm;
    int32_t speed_mmps;
    int32_t heading_deg_x10;
    uint32_t hacc_mm;
    uint32_t vacc_mm;
    uint8_t  fix_type;
    uint8_t  sats_used;
    uint16_t reserved0;
} motor_log_nav_payload_t;

typedef struct
{
    int16_t bank_deg_x10;
    int16_t grade_deg_x10;
    int16_t bank_rate_dps_x10;
    int16_t grade_rate_dps_x10;
    int32_t lat_accel_mg;
    int32_t lon_accel_mg;
    uint16_t confidence_permille;
    uint8_t  speed_source;
    uint8_t  heading_source;
} motor_log_dyn_payload_t;

typedef struct
{
    uint16_t event_code;
    uint16_t event_value;
    uint32_t aux_u32;
} motor_log_evt_payload_t;

typedef struct
{
    uint16_t rpm;
    int16_t  coolant_temp_c_x10;
    int16_t  gear;
    uint16_t battery_mv;
    uint8_t  throttle_percent;
    uint8_t  dtc_count;
    uint16_t reserved0;
} motor_log_obd_payload_t;

typedef struct
{
    uint32_t ride_seconds;
    uint32_t moving_seconds;
    uint32_t distance_m;
    uint16_t max_speed_kmh_x10;
    int16_t  max_left_bank_deg_x10;
    int16_t  max_right_bank_deg_x10;
    int32_t  max_left_lat_mg;
    int32_t  max_right_lat_mg;
    int32_t  max_accel_mg;
    int32_t  max_brake_mg;
    uint16_t marker_count;
    uint16_t drop_count;
} motor_log_sum_payload_t;

void Motor_Record_Init(void);
void Motor_Record_Task(uint32_t now_ms);
void Motor_Record_RequestStart(void);
void Motor_Record_RequestStop(void);
void Motor_Record_RequestMarker(void);
void Motor_Record_OnSdWillUnmount(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_RECORD_H */
