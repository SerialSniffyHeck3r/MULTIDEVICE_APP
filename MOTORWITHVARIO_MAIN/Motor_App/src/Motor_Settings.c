
#include "Motor_Settings.h"

#include "APP_STATE.h"
#include "Motor_Panel.h"
#include "Motor_Units.h"

#include <string.h>

static motor_settings_t s_motor_settings;

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

void Motor_Settings_ResetToDefaults(void)
{
    memset(&s_motor_settings, 0, sizeof(s_motor_settings));

    Motor_Units_ApplyPreset(&s_motor_settings.units, MOTOR_UNIT_PRESET_METRIC);

    s_motor_settings.display.brightness_mode = (uint8_t)MOTOR_DISPLAY_BRIGHTNESS_AUTO_CONTINUOUS;
    s_motor_settings.display.manual_brightness_percent = 45u;
    s_motor_settings.display.auto_continuous_bias_steps = 0;
    s_motor_settings.display.auto_day_night_night_threshold_percent = 32u;
    s_motor_settings.display.auto_day_night_super_night_threshold_percent = 12u;
    s_motor_settings.display.auto_day_night_night_brightness_percent = 42u;
    s_motor_settings.display.auto_day_night_super_night_brightness_percent = 18u;
    s_motor_settings.display.screen_flip_enabled = 0u;
    s_motor_settings.display.contrast_raw = 120u;
    s_motor_settings.display.temperature_compensation_raw = 2u;
    s_motor_settings.display.page_wrap_enabled = 1u;
    s_motor_settings.display.lock_while_moving = 0u;

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

    s_motor_settings.recording.auto_start_enabled = 1u;
    s_motor_settings.recording.auto_pause_enabled = 0u;
    s_motor_settings.recording.summary_popup_enabled = 1u;
    s_motor_settings.recording.raw_imu_log_enabled = 0u;
    s_motor_settings.recording.auto_start_speed_kmh_x10 = 50u;
    s_motor_settings.recording.auto_stop_idle_seconds = 120u;

    s_motor_settings.dynamics.auto_zero_on_boot = 1u;
    s_motor_settings.dynamics.gnss_aid_enabled = 1u;
    s_motor_settings.dynamics.obd_aid_enabled = 1u;
    s_motor_settings.dynamics.mount_forward_axis = (uint8_t)APP_BIKE_AXIS_POS_X;
    s_motor_settings.dynamics.mount_left_axis = (uint8_t)APP_BIKE_AXIS_POS_Y;
    s_motor_settings.dynamics.mount_yaw_trim_deg_x10 = 0;
    s_motor_settings.dynamics.lean_display_tau_ms = 180u;
    s_motor_settings.dynamics.accel_display_tau_ms = 180u;

    motor_settings_fill_default_services(&s_motor_settings.maintenance);

    s_motor_settings.obd.autoconnect_enabled = 0u;
    s_motor_settings.obd.preferred_speed_source = (uint8_t)APP_BIKE_SPEED_SOURCE_GNSS;
    s_motor_settings.obd.coolant_warn_temp_c_x10 = 1050u;
    s_motor_settings.obd.shift_light_rpm = 10000u;

    s_motor_settings.system.menu_wrap_enabled = 1u;

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
    /*  shared APP_STATE 에 직접 반영 가능한 설정만 즉시 저장한다.             */
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

    shared_settings.uc1608.flip_mode = (s_motor_settings.display.screen_flip_enabled != 0u) ? 1u : 0u;
    shared_settings.uc1608.contrast = s_motor_settings.display.contrast_raw;
    shared_settings.uc1608.temperature_compensation = s_motor_settings.display.temperature_compensation_raw;

    /* ---------------------------------------------------------------------- */
    /*  bike settings 는 Motor_App 상위 계층에서 다시 소유하지만,               */
    /*  mount axis / trim / 일부 tuning 값은 APP_STATE.settings.bike 에         */
    /*  미러링해 두면 다른 debug 툴과 일관성을 유지하기 좋다.                   */
    /* ---------------------------------------------------------------------- */
    shared_settings.bike.auto_zero_on_boot = s_motor_settings.dynamics.auto_zero_on_boot;
    shared_settings.bike.gnss_aid_enabled  = s_motor_settings.dynamics.gnss_aid_enabled;
    shared_settings.bike.obd_aid_enabled   = s_motor_settings.dynamics.obd_aid_enabled;
    shared_settings.bike.mount_forward_axis = s_motor_settings.dynamics.mount_forward_axis;
    shared_settings.bike.mount_left_axis    = s_motor_settings.dynamics.mount_left_axis;
    shared_settings.bike.mount_yaw_trim_deg_x10 = s_motor_settings.dynamics.mount_yaw_trim_deg_x10;
    shared_settings.bike.lean_display_tau_ms = s_motor_settings.dynamics.lean_display_tau_ms;
    shared_settings.bike.accel_display_tau_ms = s_motor_settings.dynamics.accel_display_tau_ms;

    APP_STATE_StoreSettingsSnapshot(&shared_settings);
    Motor_Panel_ApplyDisplaySettings(&s_motor_settings);
}
