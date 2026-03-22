
#include "Motor_UI_Internal.h"

#include <stdio.h>
#include <stdlib.h>

static void motor_ui_draw_settings_row(u8g2_t *u8g2,
                                       uint8_t row_index,
                                       uint8_t selected_index,
                                       const char *label,
                                       const char *value)
{
    uint8_t y;

    y = (uint8_t)(26u + row_index * 12u);
    if (row_index == selected_index)
    {
        u8g2_DrawBox(u8g2, 4, y - 8u, 232, 10);
        u8g2_SetDrawColor(u8g2, 0);
        u8g2_DrawStr(u8g2, 8, y, label);
        u8g2_DrawStr(u8g2, 150, y, value);
        u8g2_SetDrawColor(u8g2, 1);
    }
    else
    {
        u8g2_DrawStr(u8g2, 8, y, label);
        u8g2_DrawStr(u8g2, 150, y, value);
    }
}

void Motor_UI_DrawScreen_Settings(u8g2_t *u8g2, const motor_state_t *state)
{
    char value[32];

    if ((u8g2 == 0) || (state == 0))
    {
        return;
    }

    Motor_UI_DrawStatusBar(u8g2, state);
    u8g2_SetFont(u8g2, u8g2_font_6x10_tr);

    switch ((motor_screen_t)state->ui.screen)
    {
    case MOTOR_SCREEN_SETTINGS_ROOT:
        u8g2_DrawStr(u8g2, 6, 18, "SETTINGS");
        motor_ui_draw_settings_row(u8g2, 0u, state->ui.selected_index, "DISPLAY", "ENTER");
        motor_ui_draw_settings_row(u8g2, 1u, state->ui.selected_index, "GPS", "ENTER");
        motor_ui_draw_settings_row(u8g2, 2u, state->ui.selected_index, "UNITS", "ENTER");
        motor_ui_draw_settings_row(u8g2, 3u, state->ui.selected_index, "RECORDING", "ENTER");
        motor_ui_draw_settings_row(u8g2, 4u, state->ui.selected_index, "DYNAMICS", "ENTER");
        motor_ui_draw_settings_row(u8g2, 5u, state->ui.selected_index, "MAINTENANCE", "ENTER");
        motor_ui_draw_settings_row(u8g2, 6u, state->ui.selected_index, "OBD", "ENTER");
        motor_ui_draw_settings_row(u8g2, 7u, state->ui.selected_index, "SYSTEM", "ENTER");
        break;

    case MOTOR_SCREEN_SETTINGS_DISPLAY:
        u8g2_DrawStr(u8g2, 6, 18, "DISPLAY");
        switch ((motor_display_brightness_mode_t)state->settings.display.brightness_mode)
        {
        case MOTOR_DISPLAY_BRIGHTNESS_AUTO_DAY_NITE: (void)snprintf(value, sizeof(value), "AUTO D/N"); break;
        case MOTOR_DISPLAY_BRIGHTNESS_MANUAL_PERCENT: (void)snprintf(value, sizeof(value), "MANUAL"); break;
        case MOTOR_DISPLAY_BRIGHTNESS_AUTO_CONTINUOUS:
        default: (void)snprintf(value, sizeof(value), "AUTO CONT"); break;
        }
        motor_ui_draw_settings_row(u8g2, 0u, state->ui.selected_index, "BRIGHT MODE", value);
        (void)snprintf(value, sizeof(value), "%u%%", (unsigned)state->settings.display.manual_brightness_percent);
        motor_ui_draw_settings_row(u8g2, 1u, state->ui.selected_index, "MANUAL %%", value);
        (void)snprintf(value, sizeof(value), "%d", (int)state->settings.display.auto_continuous_bias_steps);
        motor_ui_draw_settings_row(u8g2, 2u, state->ui.selected_index, "AUTO BIAS", value);
        (void)snprintf(value, sizeof(value), "%s", (state->settings.display.screen_flip_enabled != 0u) ? "ON" : "OFF");
        motor_ui_draw_settings_row(u8g2, 3u, state->ui.selected_index, "FLIP SCREEN", value);
        (void)snprintf(value, sizeof(value), "%u", (unsigned)state->settings.display.contrast_raw);
        motor_ui_draw_settings_row(u8g2, 4u, state->ui.selected_index, "CONTRAST", value);
        (void)snprintf(value, sizeof(value), "%u", (unsigned)state->settings.display.temperature_compensation_raw);
        motor_ui_draw_settings_row(u8g2, 5u, state->ui.selected_index, "TEMP COMP", value);
        Motor_UI_DrawHorizontalBar(u8g2, 12, 102, 96, 8, state->settings.display.contrast_raw, 0, 255, "CTR");
        Motor_UI_DrawHorizontalBar(u8g2, 126, 102, 96, 8, state->settings.display.temperature_compensation_raw, 0, 3, "TC");
        break;

    case MOTOR_SCREEN_SETTINGS_GPS:
        u8g2_DrawStr(u8g2, 6, 18, "GPS");
        (void)snprintf(value, sizeof(value), "%s",
                       (state->settings.gps.receiver_profile == (uint8_t)APP_GPS_BOOT_PROFILE_GPS_ONLY_20HZ)
                       ? "20Hz GPS" : "10Hz FULL");
        motor_ui_draw_settings_row(u8g2, 0u, state->ui.selected_index, "PROFILE", value);
        (void)snprintf(value, sizeof(value), "%s",
                       (state->settings.gps.power_profile == (uint8_t)APP_GPS_POWER_PROFILE_HIGH_POWER)
                       ? "HIGH" : "SAVE");
        motor_ui_draw_settings_row(u8g2, 1u, state->ui.selected_index, "POWER", value);
        (void)snprintf(value, sizeof(value), "%s",
                       (state->settings.gps.dynamic_model == (uint8_t)MOTOR_GPS_DYNAMIC_MODEL_AUTOMOTIVE)
                       ? "AUTO" : "PORTABLE");
        motor_ui_draw_settings_row(u8g2, 2u, state->ui.selected_index, "DYN MODEL", value);
        motor_ui_draw_settings_row(u8g2, 3u, state->ui.selected_index, "STATIC HOLD", (state->settings.gps.static_hold_enabled != 0u) ? "ON" : "OFF");
        motor_ui_draw_settings_row(u8g2, 4u, state->ui.selected_index, "LOWSPD COG", (state->settings.gps.low_speed_course_filter_enabled != 0u) ? "ON" : "OFF");
        motor_ui_draw_settings_row(u8g2, 5u, state->ui.selected_index, "LOWSPD VEL", (state->settings.gps.low_speed_velocity_filter_enabled != 0u) ? "ON" : "OFF");
        motor_ui_draw_settings_row(u8g2, 6u, state->ui.selected_index, "RTC SYNC", (state->settings.gps.rtc_gps_sync_enabled != 0u) ? "ON" : "OFF");
        (void)snprintf(value, sizeof(value), "%u min", (unsigned)state->settings.gps.rtc_gps_sync_interval_min);
        motor_ui_draw_settings_row(u8g2, 7u, state->ui.selected_index, "SYNC INT", value);
        break;

    case MOTOR_SCREEN_SETTINGS_UNITS:
        u8g2_DrawStr(u8g2, 6, 18, "UNITS");
        switch ((motor_unit_preset_t)state->settings.units.preset)
        {
        case MOTOR_UNIT_PRESET_ENGLISH:  (void)snprintf(value, sizeof(value), "ENGLISH"); break;
        case MOTOR_UNIT_PRESET_IMPERIAL: (void)snprintf(value, sizeof(value), "IMPERIAL"); break;
        case MOTOR_UNIT_PRESET_CUSTOM:   (void)snprintf(value, sizeof(value), "CUSTOM"); break;
        case MOTOR_UNIT_PRESET_METRIC:
        default:                         (void)snprintf(value, sizeof(value), "METRIC"); break;
        }
        motor_ui_draw_settings_row(u8g2, 0u, state->ui.selected_index, "OVERALL", value);
        motor_ui_draw_settings_row(u8g2, 1u, state->ui.selected_index, "SPEED", (state->settings.units.speed == (uint8_t)MOTOR_SPEED_UNIT_MPH) ? "MPH" : "KM/H");
        motor_ui_draw_settings_row(u8g2, 2u, state->ui.selected_index, "DISTANCE", (state->settings.units.distance == (uint8_t)MOTOR_DISTANCE_UNIT_MI) ? "MI" : "KM");
        motor_ui_draw_settings_row(u8g2, 3u, state->ui.selected_index, "ALTITUDE", (state->settings.units.altitude == (uint8_t)MOTOR_ALTITUDE_UNIT_FT) ? "FT" : "M");
        motor_ui_draw_settings_row(u8g2, 4u, state->ui.selected_index, "TEMP", (state->settings.units.temperature == (uint8_t)MOTOR_TEMP_UNIT_F) ? "F" : "C");
        motor_ui_draw_settings_row(u8g2, 5u, state->ui.selected_index, "PRESSURE", (state->settings.units.pressure == (uint8_t)MOTOR_PRESSURE_UNIT_PSI) ? "PSI" : "HPA");
        break;

    case MOTOR_SCREEN_SETTINGS_RECORDING:
        u8g2_DrawStr(u8g2, 6, 18, "RECORDING");
        motor_ui_draw_settings_row(u8g2, 0u, state->ui.selected_index, "AUTO START", (state->settings.recording.auto_start_enabled != 0u) ? "ON" : "OFF");
        (void)snprintf(value, sizeof(value), "%u.%01u", (unsigned)(state->settings.recording.auto_start_speed_kmh_x10 / 10u), (unsigned)(state->settings.recording.auto_start_speed_kmh_x10 % 10u));
        motor_ui_draw_settings_row(u8g2, 1u, state->ui.selected_index, "START SPD", value);
        (void)snprintf(value, sizeof(value), "%us", (unsigned)state->settings.recording.auto_stop_idle_seconds);
        motor_ui_draw_settings_row(u8g2, 2u, state->ui.selected_index, "STOP IDLE", value);
        motor_ui_draw_settings_row(u8g2, 3u, state->ui.selected_index, "AUTO PAUSE", (state->settings.recording.auto_pause_enabled != 0u) ? "ON" : "OFF");
        motor_ui_draw_settings_row(u8g2, 4u, state->ui.selected_index, "RAW IMU", (state->settings.recording.raw_imu_log_enabled != 0u) ? "ON" : "OFF");
        motor_ui_draw_settings_row(u8g2, 5u, state->ui.selected_index, "SUMMARY", (state->settings.recording.summary_popup_enabled != 0u) ? "ON" : "OFF");
        break;

    case MOTOR_SCREEN_SETTINGS_DYNAMICS:
        u8g2_DrawStr(u8g2, 6, 18, "DYNAMICS");
        motor_ui_draw_settings_row(u8g2, 0u, state->ui.selected_index, "AUTO ZERO", (state->settings.dynamics.auto_zero_on_boot != 0u) ? "ON" : "OFF");
        motor_ui_draw_settings_row(u8g2, 1u, state->ui.selected_index, "GNSS AID", (state->settings.dynamics.gnss_aid_enabled != 0u) ? "ON" : "OFF");
        motor_ui_draw_settings_row(u8g2, 2u, state->ui.selected_index, "OBD AID", (state->settings.dynamics.obd_aid_enabled != 0u) ? "ON" : "OFF");
        (void)snprintf(value, sizeof(value), "%u", (unsigned)state->settings.dynamics.mount_forward_axis);
        motor_ui_draw_settings_row(u8g2, 3u, state->ui.selected_index, "FWD AXIS", value);
        (void)snprintf(value, sizeof(value), "%u", (unsigned)state->settings.dynamics.mount_left_axis);
        motor_ui_draw_settings_row(u8g2, 4u, state->ui.selected_index, "LEFT AXIS", value);
        (void)snprintf(value, sizeof(value), "%d.%01d", (int)(state->settings.dynamics.mount_yaw_trim_deg_x10 / 10), (int)abs(state->settings.dynamics.mount_yaw_trim_deg_x10 % 10));
        motor_ui_draw_settings_row(u8g2, 5u, state->ui.selected_index, "YAW TRIM", value);
        (void)snprintf(value, sizeof(value), "%ums", (unsigned)state->settings.dynamics.lean_display_tau_ms);
        motor_ui_draw_settings_row(u8g2, 6u, state->ui.selected_index, "LEAN LPF", value);
        (void)snprintf(value, sizeof(value), "%ums", (unsigned)state->settings.dynamics.accel_display_tau_ms);
        motor_ui_draw_settings_row(u8g2, 7u, state->ui.selected_index, "ACC LPF", value);
        break;

    case MOTOR_SCREEN_SETTINGS_MAINTENANCE:
        u8g2_DrawStr(u8g2, 6, 18, "MAINTENANCE");
        (void)snprintf(value, sizeof(value), "%lum", (unsigned long)state->settings.maintenance.due_soon_distance_m);
        motor_ui_draw_settings_row(u8g2, 0u, state->ui.selected_index, "DUE SOON M", value);
        (void)snprintf(value, sizeof(value), "%u d", (unsigned)state->settings.maintenance.due_soon_days);
        motor_ui_draw_settings_row(u8g2, 1u, state->ui.selected_index, "DUE SOON D", value);
        motor_ui_draw_settings_row(u8g2, 2u, state->ui.selected_index, "RESET OIL", "EXEC");
        motor_ui_draw_settings_row(u8g2, 3u, state->ui.selected_index, "RESET CHAIN", "EXEC");
        motor_ui_draw_settings_row(u8g2, 4u, state->ui.selected_index, "RESET BRAKE", "EXEC");
        motor_ui_draw_settings_row(u8g2, 5u, state->ui.selected_index, "RESET FILTER", "EXEC");
        break;

    case MOTOR_SCREEN_SETTINGS_OBD:
        u8g2_DrawStr(u8g2, 6, 18, "OBD");
        motor_ui_draw_settings_row(u8g2, 0u, state->ui.selected_index, "AUTO CONNECT", (state->settings.obd.autoconnect_enabled != 0u) ? "ON" : "OFF");
        motor_ui_draw_settings_row(u8g2, 1u, state->ui.selected_index, "SPD SOURCE", (state->settings.obd.preferred_speed_source == (uint8_t)APP_BIKE_SPEED_SOURCE_OBD) ? "OBD" : "GNSS");
        (void)snprintf(value, sizeof(value), "%u.%01uC", (unsigned)(state->settings.obd.coolant_warn_temp_c_x10 / 10u), (unsigned)(state->settings.obd.coolant_warn_temp_c_x10 % 10u));
        motor_ui_draw_settings_row(u8g2, 2u, state->ui.selected_index, "COOL WARN", value);
        (void)snprintf(value, sizeof(value), "%u", (unsigned)state->settings.obd.shift_light_rpm);
        motor_ui_draw_settings_row(u8g2, 3u, state->ui.selected_index, "SHIFT RPM", value);
        break;

    case MOTOR_SCREEN_SETTINGS_SYSTEM:
    default:
        u8g2_DrawStr(u8g2, 6, 18, "SYSTEM");
        motor_ui_draw_settings_row(u8g2, 0u, state->ui.selected_index, "MENU WRAP", (state->settings.system.menu_wrap_enabled != 0u) ? "ON" : "OFF");
        motor_ui_draw_settings_row(u8g2, 1u, state->ui.selected_index, "FW", "SKELETON");
        motor_ui_draw_settings_row(u8g2, 2u, state->ui.selected_index, "BUILD", "MOTOR_APP");
        motor_ui_draw_settings_row(u8g2, 3u, state->ui.selected_index, "NOTE", "Cube-safe layer");
        break;
    }

    Motor_UI_DrawBottomHint(u8g2, "1/2 ROW", "3/4 +/-", "6 BACK");
}
