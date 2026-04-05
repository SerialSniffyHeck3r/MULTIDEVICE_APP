#include "Motor_Settings.h"

#include "APP_STATE.h"
#include "Motor_Panel.h"
#include "Motor_Units.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static motor_settings_t s_motor_settings;

static const char *const s_settings_root_items[] =
{
    "DISPLAY",
    "GPS",
    "UNITS",
    "RECORDING",
    "DYNAMICS",
    "MAINTENANCE",
    "OBD",
    "SYSTEM"
};

static void motor_settings_fill_default_services(motor_maintenance_settings_t *maintenance)
{
    if (maintenance == 0)
    {
        return;
    }

    memset(maintenance, 0, sizeof(*maintenance));
    maintenance->due_soon_distance_m = 30000u;
    maintenance->due_soon_days = 14u;

    maintenance->items[0].enabled = 1u;
    (void)strncpy(maintenance->items[0].label, "ENG OIL", sizeof(maintenance->items[0].label) - 1u);
    maintenance->items[0].interval_km_x1000 = 6000u;
    maintenance->items[0].interval_days = 180u;
    maintenance->items[0].interval_engine_seconds = 120u * 3600u;

    maintenance->items[1].enabled = 1u;
    (void)strncpy(maintenance->items[1].label, "CHAIN", sizeof(maintenance->items[1].label) - 1u);
    maintenance->items[1].interval_km_x1000 = 1000u;
    maintenance->items[1].interval_days = 30u;

    maintenance->items[2].enabled = 1u;
    (void)strncpy(maintenance->items[2].label, "BRAKE", sizeof(maintenance->items[2].label) - 1u);
    maintenance->items[2].interval_km_x1000 = 12000u;
    maintenance->items[2].interval_days = 180u;

    maintenance->items[3].enabled = 1u;
    (void)strncpy(maintenance->items[3].label, "FILTER", sizeof(maintenance->items[3].label) - 1u);
    maintenance->items[3].interval_km_x1000 = 12000u;
    maintenance->items[3].interval_days = 365u;

    maintenance->items[4].enabled = 1u;
    (void)strncpy(maintenance->items[4].label, "CUSTOM1", sizeof(maintenance->items[4].label) - 1u);
    maintenance->items[4].interval_km_x1000 = 5000u;

    maintenance->items[5].enabled = 1u;
    (void)strncpy(maintenance->items[5].label, "CUSTOM2", sizeof(maintenance->items[5].label) - 1u);
    maintenance->items[5].interval_km_x1000 = 5000u;
}

static void motor_settings_fill_default_data_fields(void)
{
    /* ---------------------------------------------------------------------- */
    /*  Data Field page 1                                                      */
    /* ---------------------------------------------------------------------- */
    s_motor_settings.data_fields[0][0]  = (uint8_t)MOTOR_FIELD_SPEED;
    s_motor_settings.data_fields[0][1]  = (uint8_t)MOTOR_FIELD_BANK;
    s_motor_settings.data_fields[0][2]  = (uint8_t)MOTOR_FIELD_G_LAT;
    s_motor_settings.data_fields[0][3]  = (uint8_t)MOTOR_FIELD_G_LON;
    s_motor_settings.data_fields[0][4]  = (uint8_t)MOTOR_FIELD_ALTITUDE;
    s_motor_settings.data_fields[0][5]  = (uint8_t)MOTOR_FIELD_GRADE_PERCENT;
    s_motor_settings.data_fields[0][6]  = (uint8_t)MOTOR_FIELD_HEADING;
    s_motor_settings.data_fields[0][7]  = (uint8_t)MOTOR_FIELD_GPS_SATS;
    s_motor_settings.data_fields[0][8]  = (uint8_t)MOTOR_FIELD_GPS_FIX;
    s_motor_settings.data_fields[0][9]  = (uint8_t)MOTOR_FIELD_TIME;
    s_motor_settings.data_fields[0][10] = (uint8_t)MOTOR_FIELD_RIDE_TIME;
    s_motor_settings.data_fields[0][11] = (uint8_t)MOTOR_FIELD_DISTANCE;
    s_motor_settings.data_fields[0][12] = (uint8_t)MOTOR_FIELD_RECORD_STATE;
    s_motor_settings.data_fields[0][13] = (uint8_t)MOTOR_FIELD_SD_STATE;
    s_motor_settings.data_fields[0][14] = (uint8_t)MOTOR_FIELD_BT_STATE;

    /* ---------------------------------------------------------------------- */
    /*  Data Field page 2                                                      */
    /* ---------------------------------------------------------------------- */
    s_motor_settings.data_fields[1][0]  = (uint8_t)MOTOR_FIELD_RPM;
    s_motor_settings.data_fields[1][1]  = (uint8_t)MOTOR_FIELD_COOLANT_TEMP;
    s_motor_settings.data_fields[1][2]  = (uint8_t)MOTOR_FIELD_GEAR;
    s_motor_settings.data_fields[1][3]  = (uint8_t)MOTOR_FIELD_BATTERY;
    s_motor_settings.data_fields[1][4]  = (uint8_t)MOTOR_FIELD_OBD_LINK;
    s_motor_settings.data_fields[1][5]  = (uint8_t)MOTOR_FIELD_NEXT_SERVICE;
    s_motor_settings.data_fields[1][6]  = (uint8_t)MOTOR_FIELD_MAINT_DUE_COUNT;
    s_motor_settings.data_fields[1][7]  = (uint8_t)MOTOR_FIELD_TRIP_A;
    s_motor_settings.data_fields[1][8]  = (uint8_t)MOTOR_FIELD_TRIP_B;
    s_motor_settings.data_fields[1][9]  = (uint8_t)MOTOR_FIELD_BANK_MAX_LEFT;
    s_motor_settings.data_fields[1][10] = (uint8_t)MOTOR_FIELD_BANK_MAX_RIGHT;
    s_motor_settings.data_fields[1][11] = (uint8_t)MOTOR_FIELD_G_LAT_MAX_LEFT;
    s_motor_settings.data_fields[1][12] = (uint8_t)MOTOR_FIELD_G_LAT_MAX_RIGHT;
    s_motor_settings.data_fields[1][13] = (uint8_t)MOTOR_FIELD_G_ACCEL_MAX;
    s_motor_settings.data_fields[1][14] = (uint8_t)MOTOR_FIELD_G_BRAKE_MAX;
}

static const char *motor_settings_axis_name(uint8_t axis_raw)
{
    switch ((app_bike_axis_t)axis_raw)
    {
    case APP_BIKE_AXIS_POS_X: return "+X";
    case APP_BIKE_AXIS_NEG_X: return "-X";
    case APP_BIKE_AXIS_POS_Y: return "+Y";
    case APP_BIKE_AXIS_NEG_Y: return "-Y";
    case APP_BIKE_AXIS_POS_Z: return "+Z";
    case APP_BIKE_AXIS_NEG_Z: return "-Z";
    default:                  return "AXIS";
    }
}

static const char *motor_settings_onoff_text(uint8_t enabled)
{
    return (enabled != 0u) ? "ON" : "OFF";
}

static const char *motor_settings_unit_preset_name(uint8_t preset_raw)
{
    switch ((motor_unit_preset_t)preset_raw)
    {
    case MOTOR_UNIT_PRESET_ENGLISH:  return "ENGLISH";
    case MOTOR_UNIT_PRESET_IMPERIAL: return "IMPERIAL";
    case MOTOR_UNIT_PRESET_CUSTOM:   return "CUSTOM";
    case MOTOR_UNIT_PRESET_METRIC:
    default:                         return "METRIC";
    }
}

static const char *motor_settings_display_mode_name(uint8_t mode_raw)
{
    switch ((motor_display_brightness_mode_t)mode_raw)
    {
    case MOTOR_DISPLAY_BRIGHTNESS_AUTO_DAY_NITE:  return "AUTO D/N";
    case MOTOR_DISPLAY_BRIGHTNESS_MANUAL_PERCENT: return "MANUAL %";
    case MOTOR_DISPLAY_BRIGHTNESS_AUTO_CONTINUOUS:
    default:                                      return "AUTO CONT";
    }
}

static const char *motor_settings_main_top_mode_name(uint8_t mode_raw)
{
    switch ((motor_main_top_mode_t)mode_raw)
    {
    case MOTOR_MAIN_TOP_MODE_RPM:   return "RPM";
    case MOTOR_MAIN_TOP_MODE_SPEED:
    default:                        return "SPEED";
    }
}

static const char *motor_settings_main_speed_scale_name(uint8_t mode_raw)
{
    switch ((motor_main_speed_scale_t)mode_raw)
    {
    case MOTOR_MAIN_SPEED_SCALE_100: return "100";
    case MOTOR_MAIN_SPEED_SCALE_300: return "300";
    case MOTOR_MAIN_SPEED_SCALE_200:
    default:                         return "200";
    }
}

static const char *motor_settings_main_rpm_scale_name(uint8_t mode_raw)
{
    switch ((motor_main_rpm_scale_t)mode_raw)
    {
    case MOTOR_MAIN_RPM_SCALE_6K:  return "6K";
    case MOTOR_MAIN_RPM_SCALE_8K:  return "8K";
    case MOTOR_MAIN_RPM_SCALE_10K: return "10K";
    case MOTOR_MAIN_RPM_SCALE_12K: return "12K";
    case MOTOR_MAIN_RPM_SCALE_16K: return "16K";
    case MOTOR_MAIN_RPM_SCALE_14K:
    default:                       return "14K";
    }
}

static const char *motor_settings_main_g_scale_name(uint8_t mode_raw)
{
    switch ((motor_main_g_scale_t)mode_raw)
    {
    case MOTOR_MAIN_G_SCALE_0P5: return "0.5G";
    case MOTOR_MAIN_G_SCALE_1P5: return "1.5G";
    case MOTOR_MAIN_G_SCALE_1P0:
    default:                     return "1.0G";
    }
}

static const char *motor_settings_gps_mode_name(uint8_t mode_raw)
{
    switch ((motor_gps_dynamic_model_t)mode_raw)
    {
    case MOTOR_GPS_DYNAMIC_MODEL_PORTABLE:  return "PORTABLE";
    case MOTOR_GPS_DYNAMIC_MODEL_SIMULATOR: return "SIMULATOR";
    case MOTOR_GPS_DYNAMIC_MODEL_AUTOMOTIVE:
    default:                                return "AUTO";
    }
}

static const char *motor_settings_screen_title(motor_screen_t screen)
{
    switch (screen)
    {
    case MOTOR_SCREEN_SETTINGS_DISPLAY:     return "DISPLAY";
    case MOTOR_SCREEN_SETTINGS_GPS:         return "GPS";
    case MOTOR_SCREEN_SETTINGS_UNITS:       return "UNITS";
    case MOTOR_SCREEN_SETTINGS_RECORDING:   return "RECORDING";
    case MOTOR_SCREEN_SETTINGS_DYNAMICS:    return "DYNAMICS";
    case MOTOR_SCREEN_SETTINGS_MAINTENANCE: return "MAINTENANCE";
    case MOTOR_SCREEN_SETTINGS_OBD:         return "OBD";
    case MOTOR_SCREEN_SETTINGS_SYSTEM:      return "SYSTEM";
    case MOTOR_SCREEN_SETTINGS_ROOT:
    default:                                return "SETUP";
    }
}

static const char *motor_settings_screen_subtitle(motor_screen_t screen)
{
    switch (screen)
    {
    case MOTOR_SCREEN_SETTINGS_DISPLAY:     return "PANEL / BAR";
    case MOTOR_SCREEN_SETTINGS_GPS:         return "RECEIVER / TRACK";
    case MOTOR_SCREEN_SETTINGS_UNITS:       return "UNITS / FORMAT";
    case MOTOR_SCREEN_SETTINGS_RECORDING:   return "LOG / SESSION";
    case MOTOR_SCREEN_SETTINGS_DYNAMICS:    return "LEAN / G-FORCE";
    case MOTOR_SCREEN_SETTINGS_MAINTENANCE: return "SERVICE / LIFE";
    case MOTOR_SCREEN_SETTINGS_OBD:         return "LINK / WARN";
    case MOTOR_SCREEN_SETTINGS_SYSTEM:      return "APP / DEBUG";
    case MOTOR_SCREEN_SETTINGS_ROOT:
    default:                                return "CATEGORY";
    }
}

void Motor_Settings_ResetToDefaults(void)
{
    memset(&s_motor_settings, 0, sizeof(s_motor_settings));

    /* ---------------------------------------------------------------------- */
    /*  unit preset                                                            */
    /* ---------------------------------------------------------------------- */
    Motor_Units_ApplyPreset(&s_motor_settings.units, MOTOR_UNIT_PRESET_METRIC);

    /* ---------------------------------------------------------------------- */
    /*  display                                                                */
    /* ---------------------------------------------------------------------- */
    s_motor_settings.display.brightness_mode = (uint8_t)MOTOR_DISPLAY_BRIGHTNESS_AUTO_CONTINUOUS;
    s_motor_settings.display.manual_brightness_percent = 45u;
    s_motor_settings.display.auto_continuous_bias_steps = 0;
    s_motor_settings.display.auto_day_night_night_threshold_percent = 32u;
    s_motor_settings.display.auto_day_night_super_night_threshold_percent = 12u;
    s_motor_settings.display.auto_day_night_night_brightness_percent = 42u;
    s_motor_settings.display.auto_day_night_super_night_brightness_percent = 18u;
    s_motor_settings.display.contrast_raw = 120u;
    s_motor_settings.display.temperature_compensation_raw = 2u;
    s_motor_settings.display.smart_update_enabled = 1u;
    s_motor_settings.display.frame_limit_enabled = 1u;
    s_motor_settings.display.page_wrap_enabled = 1u;
    s_motor_settings.display.lock_while_moving = 0u;
    s_motor_settings.display.main_top_mode = (uint8_t)MOTOR_MAIN_TOP_MODE_SPEED;
    s_motor_settings.display.main_speed_scale = (uint8_t)MOTOR_MAIN_SPEED_SCALE_200;
    s_motor_settings.display.main_rpm_scale = (uint8_t)MOTOR_MAIN_RPM_SCALE_14K;
    s_motor_settings.display.main_g_scale = (uint8_t)MOTOR_MAIN_G_SCALE_1P0;

    /* ---------------------------------------------------------------------- */
    /*  GPS                                                                    */
    /* ---------------------------------------------------------------------- */
    s_motor_settings.gps.receiver_profile = (uint8_t)APP_GPS_BOOT_PROFILE_MULTI_CONST_10HZ;
    s_motor_settings.gps.power_profile = (uint8_t)APP_GPS_POWER_PROFILE_HIGH_POWER;
    s_motor_settings.gps.dynamic_model = (uint8_t)MOTOR_GPS_DYNAMIC_MODEL_AUTOMOTIVE;
    s_motor_settings.gps.static_hold_enabled = 1u;
    s_motor_settings.gps.low_speed_course_filter_enabled = 1u;
    s_motor_settings.gps.low_speed_velocity_filter_enabled = 1u;
    s_motor_settings.gps.rtc_gps_sync_enabled = 1u;
    s_motor_settings.gps.rtc_gps_sync_interval_min = 10u;
    s_motor_settings.gps.course_valid_min_speed_kmh_x10 = 50u;
    s_motor_settings.gps.breadcrumb_min_distance_m = 10u;

    /* ---------------------------------------------------------------------- */
    /*  recording                                                              */
    /* ---------------------------------------------------------------------- */
    s_motor_settings.recording.auto_start_enabled = 1u;
    s_motor_settings.recording.auto_pause_enabled = 0u;
    s_motor_settings.recording.summary_popup_enabled = 1u;
    s_motor_settings.recording.raw_imu_log_enabled = 0u;
    s_motor_settings.recording.auto_start_speed_kmh_x10 = 50u;
    s_motor_settings.recording.auto_stop_idle_seconds = 120u;

    /* ---------------------------------------------------------------------- */
    /*  dynamics                                                               */
    /*                                                                          */
    /*  아래 값들은 shared BIKE_DYNAMICS의 field와 1:1 mirror 된다.            */
    /*  기존 Motor_App의 estimator 중복 구현을 제거하고,                       */
    /*  이제 low-level canonical service가 이 값을 직접 소비한다.              */
    /* ---------------------------------------------------------------------- */
    s_motor_settings.dynamics.enabled = 1u;
    s_motor_settings.dynamics.auto_zero_on_boot = 1u;
    s_motor_settings.dynamics.gnss_aid_enabled = 1u;
    s_motor_settings.dynamics.obd_aid_enabled = 1u;
    s_motor_settings.dynamics.mount_forward_axis = (uint8_t)APP_BIKE_AXIS_POS_X;
    s_motor_settings.dynamics.mount_left_axis = (uint8_t)APP_BIKE_AXIS_POS_Y;
    s_motor_settings.dynamics.mount_yaw_trim_deg_x10 = 0;

    s_motor_settings.dynamics.imu_accel_lsb_per_g = 8192u;
    s_motor_settings.dynamics.imu_gyro_lsb_per_dps_x10 = 655u;
    s_motor_settings.dynamics.imu_gravity_tau_ms = 450u;
    s_motor_settings.dynamics.imu_linear_tau_ms = 180u;
    s_motor_settings.dynamics.imu_attitude_accel_gate_mg = 150u;
    s_motor_settings.dynamics.imu_jerk_gate_mg_per_s = 3500u;
    s_motor_settings.dynamics.imu_predict_min_trust_permille = 250u;
    s_motor_settings.dynamics.imu_stale_timeout_ms = 250u;

    s_motor_settings.dynamics.output_deadband_mg = 25u;
    s_motor_settings.dynamics.output_clip_mg = 1400u;
    s_motor_settings.dynamics.lean_display_tau_ms = 180u;
    s_motor_settings.dynamics.grade_display_tau_ms = 180u;
    s_motor_settings.dynamics.accel_display_tau_ms = 180u;

    s_motor_settings.dynamics.gnss_min_speed_kmh_x10 = 80u;
    s_motor_settings.dynamics.gnss_max_speed_acc_kmh_x10 = 80u;
    s_motor_settings.dynamics.gnss_max_head_acc_deg_x10 = 120u;
    s_motor_settings.dynamics.gnss_bias_tau_ms = 1200u;
    s_motor_settings.dynamics.gnss_outlier_gate_mg = 300u;
    s_motor_settings.dynamics.obd_stale_timeout_ms = 500u;

    /* ---------------------------------------------------------------------- */
    /*  maintenance / vehicle / system                                         */
    /* ---------------------------------------------------------------------- */
    motor_settings_fill_default_services(&s_motor_settings.maintenance);

    s_motor_settings.obd.autoconnect_enabled = 0u;
    s_motor_settings.obd.preferred_speed_source = (uint8_t)APP_BIKE_SPEED_SOURCE_GNSS;
    s_motor_settings.obd.coolant_warn_temp_c_x10 = 1050u;
    s_motor_settings.obd.shift_light_rpm = 10000u;

    s_motor_settings.system.menu_wrap_enabled = 1u;
    s_motor_settings.system.show_debug_stubs = 1u;
    s_motor_settings.system.ride_summary_popup_enabled = 1u;

    motor_settings_fill_default_data_fields();
}

void Motor_Settings_Init(void)
{
    Motor_Settings_ResetToDefaults();
    Motor_Settings_Commit();
}

void Motor_Settings_Copy(motor_settings_t *dst)
{
    if (dst == 0)
    {
        return;
    }
    memcpy(dst, &s_motor_settings, sizeof(*dst));
}

const motor_settings_t *Motor_Settings_Get(void)
{
    return &s_motor_settings;
}

motor_settings_t *Motor_Settings_GetMutable(void)
{
    return &s_motor_settings;
}

void Motor_Settings_Commit(void)
{
    app_settings_t shared_settings;

    /* ---------------------------------------------------------------------- */
    /*  unit preset 이 하위 개별 단위 조절 때문에 깨졌다면 CUSTOM 으로 정리한다. */
    /* ---------------------------------------------------------------------- */
    Motor_Units_NormalizePreset(&s_motor_settings.units);

    APP_STATE_CopySettingsSnapshot(&shared_settings);

    /* ---------------------------------------------------------------------- */
    /*  shared APP_STATE 에 직접 반영 가능한 설정                               */
    /* ---------------------------------------------------------------------- */
    shared_settings.gps.boot_profile  = (app_gps_boot_profile_t)s_motor_settings.gps.receiver_profile;
    shared_settings.gps.power_profile = (app_gps_power_profile_t)s_motor_settings.gps.power_profile;

    shared_settings.clock.gps_auto_sync_enabled = s_motor_settings.gps.rtc_gps_sync_enabled;
    shared_settings.clock.gps_sync_interval_minutes = s_motor_settings.gps.rtc_gps_sync_interval_min;

    shared_settings.backlight.continuous_bias_steps = s_motor_settings.display.auto_continuous_bias_steps;
    shared_settings.backlight.night_threshold_percent = s_motor_settings.display.auto_day_night_night_threshold_percent;
    shared_settings.backlight.super_night_threshold_percent = s_motor_settings.display.auto_day_night_super_night_threshold_percent;
    shared_settings.backlight.night_brightness_percent = s_motor_settings.display.auto_day_night_night_brightness_percent;
    shared_settings.backlight.super_night_brightness_percent = s_motor_settings.display.auto_day_night_super_night_brightness_percent;

    shared_settings.uc1608.contrast = s_motor_settings.display.contrast_raw;
    shared_settings.uc1608.temperature_compensation = s_motor_settings.display.temperature_compensation_raw;

    /* ---------------------------------------------------------------------- */
    /*  canonical bike settings mirror                                          */
    /*                                                                          */
    /*  Motor_App high-level settings를 low-level BIKE_DYNAMICS가 바로 쓸 수   */
    /*  있도록 shared APP_STATE.settings.bike에 그대로 반영한다.              */
    /* ---------------------------------------------------------------------- */
    shared_settings.bike.enabled = s_motor_settings.dynamics.enabled;
    shared_settings.bike.auto_zero_on_boot = s_motor_settings.dynamics.auto_zero_on_boot;
    shared_settings.bike.gnss_aid_enabled = s_motor_settings.dynamics.gnss_aid_enabled;
    shared_settings.bike.obd_aid_enabled = s_motor_settings.dynamics.obd_aid_enabled;
    shared_settings.bike.mount_forward_axis = s_motor_settings.dynamics.mount_forward_axis;
    shared_settings.bike.mount_left_axis = s_motor_settings.dynamics.mount_left_axis;
    shared_settings.bike.mount_yaw_trim_deg_x10 = s_motor_settings.dynamics.mount_yaw_trim_deg_x10;

    shared_settings.bike.imu_accel_lsb_per_g = s_motor_settings.dynamics.imu_accel_lsb_per_g;
    shared_settings.bike.imu_gyro_lsb_per_dps_x10 = s_motor_settings.dynamics.imu_gyro_lsb_per_dps_x10;
    shared_settings.bike.imu_gravity_tau_ms = s_motor_settings.dynamics.imu_gravity_tau_ms;
    shared_settings.bike.imu_linear_tau_ms = s_motor_settings.dynamics.imu_linear_tau_ms;
    shared_settings.bike.imu_attitude_accel_gate_mg = s_motor_settings.dynamics.imu_attitude_accel_gate_mg;
    shared_settings.bike.imu_jerk_gate_mg_per_s = s_motor_settings.dynamics.imu_jerk_gate_mg_per_s;
    shared_settings.bike.imu_predict_min_trust_permille = s_motor_settings.dynamics.imu_predict_min_trust_permille;
    shared_settings.bike.imu_stale_timeout_ms = s_motor_settings.dynamics.imu_stale_timeout_ms;

    shared_settings.bike.output_deadband_mg = s_motor_settings.dynamics.output_deadband_mg;
    shared_settings.bike.output_clip_mg = s_motor_settings.dynamics.output_clip_mg;
    shared_settings.bike.lean_display_tau_ms = s_motor_settings.dynamics.lean_display_tau_ms;
    shared_settings.bike.grade_display_tau_ms = s_motor_settings.dynamics.grade_display_tau_ms;
    shared_settings.bike.accel_display_tau_ms = s_motor_settings.dynamics.accel_display_tau_ms;

    shared_settings.bike.gnss_min_speed_kmh_x10 = s_motor_settings.dynamics.gnss_min_speed_kmh_x10;
    shared_settings.bike.gnss_max_speed_acc_kmh_x10 = s_motor_settings.dynamics.gnss_max_speed_acc_kmh_x10;
    shared_settings.bike.gnss_max_head_acc_deg_x10 = s_motor_settings.dynamics.gnss_max_head_acc_deg_x10;
    shared_settings.bike.gnss_bias_tau_ms = s_motor_settings.dynamics.gnss_bias_tau_ms;
    shared_settings.bike.gnss_outlier_gate_mg = s_motor_settings.dynamics.gnss_outlier_gate_mg;
    shared_settings.bike.obd_stale_timeout_ms = s_motor_settings.dynamics.obd_stale_timeout_ms;

    APP_STATE_StoreSettingsSnapshot(&shared_settings);
    Motor_Panel_ApplyDisplaySettings(&s_motor_settings);
}

uint8_t Motor_Settings_GetRowCount(motor_screen_t screen)
{
    switch (screen)
    {
    case MOTOR_SCREEN_SETTINGS_ROOT:        return 8u;
    case MOTOR_SCREEN_SETTINGS_DISPLAY:     return 17u;
    case MOTOR_SCREEN_SETTINGS_GPS:         return 10u;
    case MOTOR_SCREEN_SETTINGS_UNITS:       return 7u;
    case MOTOR_SCREEN_SETTINGS_RECORDING:   return 6u;
    case MOTOR_SCREEN_SETTINGS_DYNAMICS:    return 27u;
    case MOTOR_SCREEN_SETTINGS_MAINTENANCE: return 8u;
    case MOTOR_SCREEN_SETTINGS_OBD:         return 5u;
    case MOTOR_SCREEN_SETTINGS_SYSTEM:      return 6u;
    default:                                return 0u;
    }
}

const char *Motor_Settings_GetScreenTitle(motor_screen_t screen)
{
    return motor_settings_screen_title(screen);
}

const char *Motor_Settings_GetScreenSubtitle(motor_screen_t screen)
{
    return motor_settings_screen_subtitle(screen);
}

void Motor_Settings_GetRowText(const motor_state_t *state,
                               motor_screen_t screen,
                               uint8_t row,
                               char *out_label,
                               size_t label_size,
                               char *out_value,
                               size_t value_size)
{
    if ((state == 0) || (out_label == 0) || (out_value == 0) || (label_size == 0u) || (value_size == 0u))
    {
        return;
    }

    out_label[0] = '\0';
    out_value[0] = '\0';

    switch (screen)
    {
    case MOTOR_SCREEN_SETTINGS_ROOT:
        if (row < (uint8_t)(sizeof(s_settings_root_items) / sizeof(s_settings_root_items[0])))
        {
            (void)snprintf(out_label, label_size, "%s", s_settings_root_items[row]);
            (void)snprintf(out_value, value_size, ">>");
        }
        break;

    case MOTOR_SCREEN_SETTINGS_DISPLAY:
        switch (row)
        {
        case 0u:  (void)snprintf(out_label, label_size, "BRIGHT MODE"); (void)snprintf(out_value, value_size, "%s", motor_settings_display_mode_name(state->settings.display.brightness_mode)); break;
        case 1u:  (void)snprintf(out_label, label_size, "MANUAL LEVEL"); (void)snprintf(out_value, value_size, "%u%%", (unsigned)state->settings.display.manual_brightness_percent); break;
        case 2u:  (void)snprintf(out_label, label_size, "AUTO BIAS"); (void)snprintf(out_value, value_size, "%d", (int)state->settings.display.auto_continuous_bias_steps); break;
        case 3u:  (void)snprintf(out_label, label_size, "NIGHT THR"); (void)snprintf(out_value, value_size, "%u%%", (unsigned)state->settings.display.auto_day_night_night_threshold_percent); break;
        case 4u:  (void)snprintf(out_label, label_size, "SUPER THR"); (void)snprintf(out_value, value_size, "%u%%", (unsigned)state->settings.display.auto_day_night_super_night_threshold_percent); break;
        case 5u:  (void)snprintf(out_label, label_size, "NIGHT BRT"); (void)snprintf(out_value, value_size, "%u%%", (unsigned)state->settings.display.auto_day_night_night_brightness_percent); break;
        case 6u:  (void)snprintf(out_label, label_size, "SUPER BRT"); (void)snprintf(out_value, value_size, "%u%%", (unsigned)state->settings.display.auto_day_night_super_night_brightness_percent); break;
        case 7u:  (void)snprintf(out_label, label_size, "CONTRAST"); (void)snprintf(out_value, value_size, "%u", (unsigned)state->settings.display.contrast_raw); break;
        case 8u:  (void)snprintf(out_label, label_size, "TEMP COMP"); (void)snprintf(out_value, value_size, "%u", (unsigned)state->settings.display.temperature_compensation_raw); break;
        case 9u:  (void)snprintf(out_label, label_size, "SMART UPDATE"); (void)snprintf(out_value, value_size, "%s", motor_settings_onoff_text(state->settings.display.smart_update_enabled)); break;
        case 10u: (void)snprintf(out_label, label_size, "FRAME LIMIT"); (void)snprintf(out_value, value_size, "%s", motor_settings_onoff_text(state->settings.display.frame_limit_enabled)); break;
        case 11u: (void)snprintf(out_label, label_size, "PAGE WRAP"); (void)snprintf(out_value, value_size, "%s", motor_settings_onoff_text(state->settings.display.page_wrap_enabled)); break;
        case 12u: (void)snprintf(out_label, label_size, "MOVING LOCK"); (void)snprintf(out_value, value_size, "%s", motor_settings_onoff_text(state->settings.display.lock_while_moving)); break;
        case 13u: (void)snprintf(out_label, label_size, "HOME TOP"); (void)snprintf(out_value, value_size, "%s", motor_settings_main_top_mode_name(state->settings.display.main_top_mode)); break;
        case 14u: (void)snprintf(out_label, label_size, "HOME SPD MAX"); (void)snprintf(out_value, value_size, "%s", motor_settings_main_speed_scale_name(state->settings.display.main_speed_scale)); break;
        case 15u: (void)snprintf(out_label, label_size, "HOME RPM MAX"); (void)snprintf(out_value, value_size, "%s", motor_settings_main_rpm_scale_name(state->settings.display.main_rpm_scale)); break;
        case 16u: (void)snprintf(out_label, label_size, "HOME G MAX"); (void)snprintf(out_value, value_size, "%s", motor_settings_main_g_scale_name(state->settings.display.main_g_scale)); break;
        default: break;
        }
        break;

    case MOTOR_SCREEN_SETTINGS_GPS:
        switch (row)
        {
        case 0u: (void)snprintf(out_label, label_size, "RECEIVER"); (void)snprintf(out_value, value_size, "%s", (state->settings.gps.receiver_profile == (uint8_t)APP_GPS_BOOT_PROFILE_GPS_ONLY_20HZ) ? "20HZ GPS" : "10HZ FULL"); break;
        case 1u: (void)snprintf(out_label, label_size, "POWER"); (void)snprintf(out_value, value_size, "%s", (state->settings.gps.power_profile == (uint8_t)APP_GPS_POWER_PROFILE_HIGH_POWER) ? "HIGH" : "SAVE"); break;
        case 2u: (void)snprintf(out_label, label_size, "GPS MODE"); (void)snprintf(out_value, value_size, "%s", motor_settings_gps_mode_name(state->settings.gps.dynamic_model)); break;
        case 3u: (void)snprintf(out_label, label_size, "STATIC HOLD"); (void)snprintf(out_value, value_size, "%s", motor_settings_onoff_text(state->settings.gps.static_hold_enabled)); break;
        case 4u: (void)snprintf(out_label, label_size, "LOWSPD COG"); (void)snprintf(out_value, value_size, "%s", motor_settings_onoff_text(state->settings.gps.low_speed_course_filter_enabled)); break;
        case 5u: (void)snprintf(out_label, label_size, "LOWSPD VEL"); (void)snprintf(out_value, value_size, "%s", motor_settings_onoff_text(state->settings.gps.low_speed_velocity_filter_enabled)); break;
        case 6u: (void)snprintf(out_label, label_size, "RTC SYNC"); (void)snprintf(out_value, value_size, "%s", motor_settings_onoff_text(state->settings.gps.rtc_gps_sync_enabled)); break;
        case 7u: (void)snprintf(out_label, label_size, "SYNC INT"); (void)snprintf(out_value, value_size, "%u min", (unsigned)state->settings.gps.rtc_gps_sync_interval_min); break;
        case 8u: (void)snprintf(out_label, label_size, "CRUMB DIST"); (void)snprintf(out_value, value_size, "%u m", (unsigned)state->settings.gps.breadcrumb_min_distance_m); break;
        case 9u: (void)snprintf(out_label, label_size, "HEAD MIN SPD"); (void)snprintf(out_value, value_size, "%u.%01u", (unsigned)(state->settings.gps.course_valid_min_speed_kmh_x10 / 10u), (unsigned)(state->settings.gps.course_valid_min_speed_kmh_x10 % 10u)); break;
        default: break;
        }
        break;

    case MOTOR_SCREEN_SETTINGS_UNITS:
        switch (row)
        {
        case 0u: (void)snprintf(out_label, label_size, "OVERALL"); (void)snprintf(out_value, value_size, "%s", motor_settings_unit_preset_name(state->settings.units.preset)); break;
        case 1u: (void)snprintf(out_label, label_size, "SPEED"); (void)snprintf(out_value, value_size, "%s", (state->settings.units.speed == (uint8_t)MOTOR_SPEED_UNIT_KMH) ? "KM/H" : "MPH"); break;
        case 2u: (void)snprintf(out_label, label_size, "DISTANCE"); (void)snprintf(out_value, value_size, "%s", (state->settings.units.distance == (uint8_t)MOTOR_DISTANCE_UNIT_KM) ? "KM" : "MI"); break;
        case 3u: (void)snprintf(out_label, label_size, "ALTITUDE"); (void)snprintf(out_value, value_size, "%s", (state->settings.units.altitude == (uint8_t)MOTOR_ALTITUDE_UNIT_M) ? "M" : "FT"); break;
        case 4u: (void)snprintf(out_label, label_size, "TEMP"); (void)snprintf(out_value, value_size, "%s", (state->settings.units.temperature == (uint8_t)MOTOR_TEMP_UNIT_C) ? "C" : "F"); break;
        case 5u: (void)snprintf(out_label, label_size, "PRESSURE"); (void)snprintf(out_value, value_size, "%s", (state->settings.units.pressure == (uint8_t)MOTOR_PRESSURE_UNIT_HPA) ? "HPA" : "PSI"); break;
        case 6u: (void)snprintf(out_label, label_size, "ECONOMY"); (void)snprintf(out_value, value_size, "%s", (state->settings.units.economy == (uint8_t)MOTOR_ECON_UNIT_L_PER_100KM) ? "L/100" : ((state->settings.units.economy == (uint8_t)MOTOR_ECON_UNIT_MPG_US) ? "MPG US" : "MPG UK")); break;
        default: break;
        }
        break;

    case MOTOR_SCREEN_SETTINGS_RECORDING:
        switch (row)
        {
        case 0u: (void)snprintf(out_label, label_size, "AUTO START"); (void)snprintf(out_value, value_size, "%s", motor_settings_onoff_text(state->settings.recording.auto_start_enabled)); break;
        case 1u: (void)snprintf(out_label, label_size, "START SPD"); (void)snprintf(out_value, value_size, "%u.%01u", (unsigned)(state->settings.recording.auto_start_speed_kmh_x10 / 10u), (unsigned)(state->settings.recording.auto_start_speed_kmh_x10 % 10u)); break;
        case 2u: (void)snprintf(out_label, label_size, "STOP IDLE"); (void)snprintf(out_value, value_size, "%u s", (unsigned)state->settings.recording.auto_stop_idle_seconds); break;
        case 3u: (void)snprintf(out_label, label_size, "AUTO PAUSE"); (void)snprintf(out_value, value_size, "%s", motor_settings_onoff_text(state->settings.recording.auto_pause_enabled)); break;
        case 4u: (void)snprintf(out_label, label_size, "SUMMARY POP"); (void)snprintf(out_value, value_size, "%s", motor_settings_onoff_text(state->settings.recording.summary_popup_enabled)); break;
        case 5u: (void)snprintf(out_label, label_size, "RAW IMU LOG"); (void)snprintf(out_value, value_size, "%s", motor_settings_onoff_text(state->settings.recording.raw_imu_log_enabled)); break;
        default: break;
        }
        break;

    case MOTOR_SCREEN_SETTINGS_DYNAMICS:
        switch (row)
        {
        case 0u:  (void)snprintf(out_label, label_size, "SERVICE"); (void)snprintf(out_value, value_size, "%s", motor_settings_onoff_text(state->settings.dynamics.enabled)); break;
        case 1u:  (void)snprintf(out_label, label_size, "AUTO ZERO"); (void)snprintf(out_value, value_size, "%s", motor_settings_onoff_text(state->settings.dynamics.auto_zero_on_boot)); break;
        case 2u:  (void)snprintf(out_label, label_size, "GNSS AID"); (void)snprintf(out_value, value_size, "%s", motor_settings_onoff_text(state->settings.dynamics.gnss_aid_enabled)); break;
        case 3u:  (void)snprintf(out_label, label_size, "OBD AID"); (void)snprintf(out_value, value_size, "%s", motor_settings_onoff_text(state->settings.dynamics.obd_aid_enabled)); break;
        case 4u:  (void)snprintf(out_label, label_size, "FWD AXIS"); (void)snprintf(out_value, value_size, "%s", motor_settings_axis_name(state->settings.dynamics.mount_forward_axis)); break;
        case 5u:  (void)snprintf(out_label, label_size, "LEFT AXIS"); (void)snprintf(out_value, value_size, "%s", motor_settings_axis_name(state->settings.dynamics.mount_left_axis)); break;
        case 6u:  (void)snprintf(out_label, label_size, "YAW TRIM"); (void)snprintf(out_value, value_size, "%+d.%01d", (int)(state->settings.dynamics.mount_yaw_trim_deg_x10 / 10), (int)abs((int)(state->settings.dynamics.mount_yaw_trim_deg_x10 % 10))); break;
        case 7u:  (void)snprintf(out_label, label_size, "GRAV TAU"); (void)snprintf(out_value, value_size, "%u ms", (unsigned)state->settings.dynamics.imu_gravity_tau_ms); break;
        case 8u:  (void)snprintf(out_label, label_size, "LIN TAU"); (void)snprintf(out_value, value_size, "%u ms", (unsigned)state->settings.dynamics.imu_linear_tau_ms); break;
        case 9u:  (void)snprintf(out_label, label_size, "ACC GATE"); (void)snprintf(out_value, value_size, "%u mg", (unsigned)state->settings.dynamics.imu_attitude_accel_gate_mg); break;
        case 10u: (void)snprintf(out_label, label_size, "JERK GATE"); (void)snprintf(out_value, value_size, "%u", (unsigned)state->settings.dynamics.imu_jerk_gate_mg_per_s); break;
        case 11u: (void)snprintf(out_label, label_size, "MIN TRUST"); (void)snprintf(out_value, value_size, "%u", (unsigned)state->settings.dynamics.imu_predict_min_trust_permille); break;
        case 12u: (void)snprintf(out_label, label_size, "IMU STALE"); (void)snprintf(out_value, value_size, "%u ms", (unsigned)state->settings.dynamics.imu_stale_timeout_ms); break;
        case 13u: (void)snprintf(out_label, label_size, "OUT DB"); (void)snprintf(out_value, value_size, "%u mg", (unsigned)state->settings.dynamics.output_deadband_mg); break;
        case 14u: (void)snprintf(out_label, label_size, "OUT CLIP"); (void)snprintf(out_value, value_size, "%u mg", (unsigned)state->settings.dynamics.output_clip_mg); break;
        case 15u: (void)snprintf(out_label, label_size, "LEAN TAU"); (void)snprintf(out_value, value_size, "%u ms", (unsigned)state->settings.dynamics.lean_display_tau_ms); break;
        case 16u: (void)snprintf(out_label, label_size, "GRADE TAU"); (void)snprintf(out_value, value_size, "%u ms", (unsigned)state->settings.dynamics.grade_display_tau_ms); break;
        case 17u: (void)snprintf(out_label, label_size, "ACC TAU"); (void)snprintf(out_value, value_size, "%u ms", (unsigned)state->settings.dynamics.accel_display_tau_ms); break;
        case 18u: (void)snprintf(out_label, label_size, "GNSS MIN SPD"); (void)snprintf(out_value, value_size, "%u.%01u", (unsigned)(state->settings.dynamics.gnss_min_speed_kmh_x10 / 10u), (unsigned)(state->settings.dynamics.gnss_min_speed_kmh_x10 % 10u)); break;
        case 19u: (void)snprintf(out_label, label_size, "SPD ACC MAX"); (void)snprintf(out_value, value_size, "%u.%01u", (unsigned)(state->settings.dynamics.gnss_max_speed_acc_kmh_x10 / 10u), (unsigned)(state->settings.dynamics.gnss_max_speed_acc_kmh_x10 % 10u)); break;
        case 20u: (void)snprintf(out_label, label_size, "HEAD ACC MAX"); (void)snprintf(out_value, value_size, "%u.%01u deg", (unsigned)(state->settings.dynamics.gnss_max_head_acc_deg_x10 / 10u), (unsigned)(state->settings.dynamics.gnss_max_head_acc_deg_x10 % 10u)); break;
        case 21u: (void)snprintf(out_label, label_size, "BIAS TAU"); (void)snprintf(out_value, value_size, "%u ms", (unsigned)state->settings.dynamics.gnss_bias_tau_ms); break;
        case 22u: (void)snprintf(out_label, label_size, "OUTLIER GATE"); (void)snprintf(out_value, value_size, "%u mg", (unsigned)state->settings.dynamics.gnss_outlier_gate_mg); break;
        case 23u: (void)snprintf(out_label, label_size, "OBD STALE"); (void)snprintf(out_value, value_size, "%u ms", (unsigned)state->settings.dynamics.obd_stale_timeout_ms); break;
        case 24u: (void)snprintf(out_label, label_size, "ZERO CAPTURE"); (void)snprintf(out_value, value_size, "EXEC"); break;
        case 25u: (void)snprintf(out_label, label_size, "HARD REZERO"); (void)snprintf(out_value, value_size, "EXEC"); break;
        case 26u:
            (void)snprintf(out_label, label_size, "GYRO CAL");
            if (state->snapshot.bike.gyro_bias_cal_active != false)
            {
                (void)snprintf(out_value, value_size, "%u%%", (unsigned)(state->snapshot.bike.gyro_bias_cal_progress_permille / 10u));
            }
            else
            {
                (void)snprintf(out_value, value_size, "EXEC");
            }
            break;
        default: break;
        }
        break;

    case MOTOR_SCREEN_SETTINGS_MAINTENANCE:
        switch (row)
        {
        case 0u: (void)snprintf(out_label, label_size, "DUE SOON M"); (void)snprintf(out_value, value_size, "%lu", (unsigned long)state->settings.maintenance.due_soon_distance_m); break;
        case 1u: (void)snprintf(out_label, label_size, "DUE SOON D"); (void)snprintf(out_value, value_size, "%u", (unsigned)state->settings.maintenance.due_soon_days); break;
        case 2u: (void)snprintf(out_label, label_size, "RESET %s", state->settings.maintenance.items[0].label); (void)snprintf(out_value, value_size, "EXEC"); break;
        case 3u: (void)snprintf(out_label, label_size, "RESET %s", state->settings.maintenance.items[1].label); (void)snprintf(out_value, value_size, "EXEC"); break;
        case 4u: (void)snprintf(out_label, label_size, "RESET %s", state->settings.maintenance.items[2].label); (void)snprintf(out_value, value_size, "EXEC"); break;
        case 5u: (void)snprintf(out_label, label_size, "RESET %s", state->settings.maintenance.items[3].label); (void)snprintf(out_value, value_size, "EXEC"); break;
        case 6u: (void)snprintf(out_label, label_size, "RESET %s", state->settings.maintenance.items[4].label); (void)snprintf(out_value, value_size, "EXEC"); break;
        case 7u: (void)snprintf(out_label, label_size, "RESET %s", state->settings.maintenance.items[5].label); (void)snprintf(out_value, value_size, "EXEC"); break;
        default: break;
        }
        break;

    case MOTOR_SCREEN_SETTINGS_OBD:
        switch (row)
        {
        case 0u: (void)snprintf(out_label, label_size, "AUTO CONNECT"); (void)snprintf(out_value, value_size, "%s", motor_settings_onoff_text(state->settings.obd.autoconnect_enabled)); break;
        case 1u: (void)snprintf(out_label, label_size, "SPEED SOURCE"); (void)snprintf(out_value, value_size, "%s", (state->settings.obd.preferred_speed_source == (uint8_t)APP_BIKE_SPEED_SOURCE_OBD) ? "OBD" : "GNSS"); break;
        case 2u: (void)snprintf(out_label, label_size, "COOL WARN"); (void)snprintf(out_value, value_size, "%u.%01uC", (unsigned)(state->settings.obd.coolant_warn_temp_c_x10 / 10u), (unsigned)(state->settings.obd.coolant_warn_temp_c_x10 % 10u)); break;
        case 3u: (void)snprintf(out_label, label_size, "SHIFT RPM"); (void)snprintf(out_value, value_size, "%u", (unsigned)state->settings.obd.shift_light_rpm); break;
        case 4u: (void)snprintf(out_label, label_size, "LINK / DISC"); (void)snprintf(out_value, value_size, "%s", (state->vehicle.connected != false) ? "DISC" : "LINK"); break;
        default: break;
        }
        break;

    case MOTOR_SCREEN_SETTINGS_SYSTEM:
        switch (row)
        {
        case 0u: (void)snprintf(out_label, label_size, "MENU WRAP"); (void)snprintf(out_value, value_size, "%s", motor_settings_onoff_text(state->settings.system.menu_wrap_enabled)); break;
        case 1u: (void)snprintf(out_label, label_size, "RIDE SUMMARY"); (void)snprintf(out_value, value_size, "%s", motor_settings_onoff_text(state->settings.system.ride_summary_popup_enabled)); break;
        case 2u: (void)snprintf(out_label, label_size, "DEBUG STUBS"); (void)snprintf(out_value, value_size, "%s", motor_settings_onoff_text(state->settings.system.show_debug_stubs)); break;
        case 3u: (void)snprintf(out_label, label_size, "FACTORY RESET"); (void)snprintf(out_value, value_size, "EXEC"); break;
        case 4u: (void)snprintf(out_label, label_size, "UI STYLE"); (void)snprintf(out_value, value_size, "VARIO"); break;
        case 5u: (void)snprintf(out_label, label_size, "DYN SRC"); (void)snprintf(out_value, value_size, "APP BIKE"); break;
        default: break;
        }
        break;

    default:
        break;
    }
}
