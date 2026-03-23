#include "Motor_UI_Internal.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifndef MOTOR_SETTINGS_VISIBLE_ROWS
#define MOTOR_SETTINGS_VISIBLE_ROWS 7u
#endif

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

static const char *motor_ui_axis_name(uint8_t axis_raw)
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

static const char *motor_ui_overall_unit_name(uint8_t preset_raw)
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

static void motor_ui_settings_draw_row(u8g2_t *u8g2,
                                       const ui_rect_t *viewport,
                                       uint8_t visible_row,
                                       uint8_t selected,
                                       const char *label,
                                       const char *value)
{
    int16_t x;
    int16_t y;
    int16_t w;

    if ((u8g2 == 0) || (viewport == 0) || (label == 0) || (value == 0))
    {
        return;
    }

    x = (int16_t)(viewport->x + 4);
    y = (int16_t)(viewport->y + 16 + (visible_row * 13));
    w = (int16_t)(viewport->w - 8);

    if (selected != 0u)
    {
        u8g2_DrawBox(u8g2, x, y - 8, w, 11);
        u8g2_SetDrawColor(u8g2, 0);
    }

    u8g2_SetFont(u8g2, u8g2_font_6x10_tr);
    u8g2_DrawStr(u8g2, x + 2, y, label);
    u8g2_DrawStr(u8g2, x + 134, y, value);

    if (selected != 0u)
    {
        u8g2_SetDrawColor(u8g2, 1);
    }
}

static void motor_ui_settings_draw_footer(u8g2_t *u8g2,
                                          const ui_rect_t *viewport,
                                          const char *text)
{
    if ((u8g2 == 0) || (viewport == 0) || (text == 0))
    {
        return;
    }

    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    u8g2_DrawStr(u8g2, viewport->x + 4, viewport->y + viewport->h - 4, text);
}

static void motor_ui_settings_draw_display_aux(u8g2_t *u8g2,
                                               const ui_rect_t *viewport,
                                               const motor_state_t *state)
{
    if ((u8g2 == 0) || (viewport == 0) || (state == 0))
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  Display 페이지 하단 보조 그래픽                                        */
    /*  - contrast와 temperature compensation을 bar로 한 번 더 보여 준다.      */
    /*  - 실제 레지스터는 상위 Panel apply가 wrapper API를 통해 반영한다.      */
    /* ---------------------------------------------------------------------- */
    Motor_UI_DrawHorizontalBar(u8g2,
                               viewport->x + 8,
                               viewport->y + viewport->h - 22,
                               102,
                               8,
                               state->settings.display.contrast_raw,
                               0,
                               255,
                               "CONTRAST");

    Motor_UI_DrawHorizontalBar(u8g2,
                               viewport->x + 126,
                               viewport->y + viewport->h - 22,
                               102,
                               8,
                               state->settings.display.temperature_compensation_raw,
                               0,
                               3,
                               "TEMP COMP");
}

static void motor_ui_settings_get_row_text(const motor_state_t *state,
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
        case 0u:
            (void)snprintf(out_label, label_size, "BRIGHT MODE");
            switch ((motor_display_brightness_mode_t)state->settings.display.brightness_mode)
            {
            case MOTOR_DISPLAY_BRIGHTNESS_AUTO_DAY_NITE:   (void)snprintf(out_value, value_size, "AUTO D/N"); break;
            case MOTOR_DISPLAY_BRIGHTNESS_MANUAL_PERCENT:  (void)snprintf(out_value, value_size, "MANUAL %%"); break;
            case MOTOR_DISPLAY_BRIGHTNESS_AUTO_CONTINUOUS:
            default:                                       (void)snprintf(out_value, value_size, "AUTO CONT"); break;
            }
            break;
        case 1u: (void)snprintf(out_label, label_size, "MANUAL LEVEL"); (void)snprintf(out_value, value_size, "%u%%", (unsigned)state->settings.display.manual_brightness_percent); break;
        case 2u: (void)snprintf(out_label, label_size, "AUTO BIAS"); (void)snprintf(out_value, value_size, "%d", (int)state->settings.display.auto_continuous_bias_steps); break;
        case 3u: (void)snprintf(out_label, label_size, "NIGHT THR"); (void)snprintf(out_value, value_size, "%u%%", (unsigned)state->settings.display.auto_day_night_night_threshold_percent); break;
        case 4u: (void)snprintf(out_label, label_size, "SUPER THR"); (void)snprintf(out_value, value_size, "%u%%", (unsigned)state->settings.display.auto_day_night_super_night_threshold_percent); break;
        case 5u: (void)snprintf(out_label, label_size, "NIGHT BRT"); (void)snprintf(out_value, value_size, "%u%%", (unsigned)state->settings.display.auto_day_night_night_brightness_percent); break;
        case 6u: (void)snprintf(out_label, label_size, "SUPER BRT"); (void)snprintf(out_value, value_size, "%u%%", (unsigned)state->settings.display.auto_day_night_super_night_brightness_percent); break;
        case 7u: (void)snprintf(out_label, label_size, "CONTRAST"); (void)snprintf(out_value, value_size, "%u", (unsigned)state->settings.display.contrast_raw); break;
        case 8u: (void)snprintf(out_label, label_size, "TEMP COMP"); (void)snprintf(out_value, value_size, "%u", (unsigned)state->settings.display.temperature_compensation_raw); break;
        case 9u: (void)snprintf(out_label, label_size, "SMART UPDATE"); (void)snprintf(out_value, value_size, "%s", (state->settings.display.smart_update_enabled != 0u) ? "ON" : "OFF"); break;
        case 10u:(void)snprintf(out_label, label_size, "FRAME LIMIT"); (void)snprintf(out_value, value_size, "%s", (state->settings.display.frame_limit_enabled != 0u) ? "ON" : "OFF"); break;
        case 11u:(void)snprintf(out_label, label_size, "PAGE WRAP"); (void)snprintf(out_value, value_size, "%s", (state->settings.display.page_wrap_enabled != 0u) ? "ON" : "OFF"); break;
        case 12u:(void)snprintf(out_label, label_size, "MOVING LOCK"); (void)snprintf(out_value, value_size, "%s", (state->settings.display.lock_while_moving != 0u) ? "ON" : "OFF"); break;
        default: break;
        }
        break;

    case MOTOR_SCREEN_SETTINGS_GPS:
        switch (row)
        {
        case 0u:
            (void)snprintf(out_label, label_size, "RECEIVER");
            (void)snprintf(out_value,
                           value_size,
                           "%s",
                           (state->settings.gps.receiver_profile == (uint8_t)APP_GPS_BOOT_PROFILE_GPS_ONLY_20HZ) ? "20HZ GPS" : "10HZ FULL");
            break;
        case 1u: (void)snprintf(out_label, label_size, "POWER"); (void)snprintf(out_value, value_size, "%s", (state->settings.gps.power_profile == (uint8_t)APP_GPS_POWER_PROFILE_HIGH_POWER) ? "HIGH" : "SAVE"); break;
        case 2u: (void)snprintf(out_label, label_size, "DYNAMIC MODEL"); (void)snprintf(out_value, value_size, "%s", (state->settings.gps.dynamic_model == (uint8_t)MOTOR_GPS_DYNAMIC_MODEL_AUTOMOTIVE) ? "AUTO" : "PORTABLE"); break;
        case 3u: (void)snprintf(out_label, label_size, "STATIC HOLD"); (void)snprintf(out_value, value_size, "%s", (state->settings.gps.static_hold_enabled != 0u) ? "ON" : "OFF"); break;
        case 4u: (void)snprintf(out_label, label_size, "LOWSPD COG"); (void)snprintf(out_value, value_size, "%s", (state->settings.gps.low_speed_course_filter_enabled != 0u) ? "ON" : "OFF"); break;
        case 5u: (void)snprintf(out_label, label_size, "LOWSPD VEL"); (void)snprintf(out_value, value_size, "%s", (state->settings.gps.low_speed_velocity_filter_enabled != 0u) ? "ON" : "OFF"); break;
        case 6u: (void)snprintf(out_label, label_size, "RTC SYNC"); (void)snprintf(out_value, value_size, "%s", (state->settings.gps.rtc_gps_sync_enabled != 0u) ? "ON" : "OFF"); break;
        case 7u: (void)snprintf(out_label, label_size, "SYNC INT"); (void)snprintf(out_value, value_size, "%u min", (unsigned)state->settings.gps.rtc_gps_sync_interval_min); break;
        case 8u: (void)snprintf(out_label, label_size, "CRUMB DIST"); (void)snprintf(out_value, value_size, "%u m", (unsigned)state->settings.gps.breadcrumb_min_distance_m); break;
        case 9u: (void)snprintf(out_label, label_size, "COURSE MIN"); (void)snprintf(out_value, value_size, "%u.%01u", (unsigned)(state->settings.gps.course_valid_min_speed_kmh_x10 / 10u), (unsigned)(state->settings.gps.course_valid_min_speed_kmh_x10 % 10u)); break;
        default: break;
        }
        break;

    case MOTOR_SCREEN_SETTINGS_UNITS:
        switch (row)
        {
        case 0u: (void)snprintf(out_label, label_size, "OVERALL"); (void)snprintf(out_value, value_size, "%s", motor_ui_overall_unit_name(state->settings.units.preset)); break;
        case 1u: (void)snprintf(out_label, label_size, "SPEED"); (void)snprintf(out_value, value_size, "%s", (state->settings.units.speed == (uint8_t)MOTOR_SPEED_UNIT_MPH) ? "MPH" : "KM/H"); break;
        case 2u: (void)snprintf(out_label, label_size, "DISTANCE"); (void)snprintf(out_value, value_size, "%s", (state->settings.units.distance == (uint8_t)MOTOR_DISTANCE_UNIT_MI) ? "MI" : "KM"); break;
        case 3u: (void)snprintf(out_label, label_size, "ALTITUDE"); (void)snprintf(out_value, value_size, "%s", (state->settings.units.altitude == (uint8_t)MOTOR_ALTITUDE_UNIT_FT) ? "FT" : "M"); break;
        case 4u: (void)snprintf(out_label, label_size, "TEMP"); (void)snprintf(out_value, value_size, "%s", (state->settings.units.temperature == (uint8_t)MOTOR_TEMP_UNIT_F) ? "F" : "C"); break;
        case 5u: (void)snprintf(out_label, label_size, "PRESSURE"); (void)snprintf(out_value, value_size, "%s", (state->settings.units.pressure == (uint8_t)MOTOR_PRESSURE_UNIT_PSI) ? "PSI" : "HPA"); break;
        case 6u:
            (void)snprintf(out_label, label_size, "ECONOMY");
            switch ((motor_economy_unit_t)state->settings.units.economy)
            {
            case MOTOR_ECON_UNIT_MPG_US: (void)snprintf(out_value, value_size, "MPG US"); break;
            case MOTOR_ECON_UNIT_MPG_UK: (void)snprintf(out_value, value_size, "MPG UK"); break;
            case MOTOR_ECON_UNIT_L_PER_100KM:
            default:                     (void)snprintf(out_value, value_size, "L/100KM"); break;
            }
            break;
        default: break;
        }
        break;

    case MOTOR_SCREEN_SETTINGS_RECORDING:
        switch (row)
        {
        case 0u: (void)snprintf(out_label, label_size, "AUTO START"); (void)snprintf(out_value, value_size, "%s", (state->settings.recording.auto_start_enabled != 0u) ? "ON" : "OFF"); break;
        case 1u: (void)snprintf(out_label, label_size, "START SPD"); (void)snprintf(out_value, value_size, "%u.%01u", (unsigned)(state->settings.recording.auto_start_speed_kmh_x10 / 10u), (unsigned)(state->settings.recording.auto_start_speed_kmh_x10 % 10u)); break;
        case 2u: (void)snprintf(out_label, label_size, "STOP IDLE"); (void)snprintf(out_value, value_size, "%u s", (unsigned)state->settings.recording.auto_stop_idle_seconds); break;
        case 3u: (void)snprintf(out_label, label_size, "AUTO PAUSE"); (void)snprintf(out_value, value_size, "%s", (state->settings.recording.auto_pause_enabled != 0u) ? "ON" : "OFF"); break;
        case 4u: (void)snprintf(out_label, label_size, "SUMMARY POP"); (void)snprintf(out_value, value_size, "%s", (state->settings.recording.summary_popup_enabled != 0u) ? "ON" : "OFF"); break;
        case 5u: (void)snprintf(out_label, label_size, "RAW IMU LOG"); (void)snprintf(out_value, value_size, "%s", (state->settings.recording.raw_imu_log_enabled != 0u) ? "ON" : "OFF"); break;
        default: break;
        }
        break;

    case MOTOR_SCREEN_SETTINGS_DYNAMICS:
        switch (row)
        {
        case 0u: (void)snprintf(out_label, label_size, "AUTO ZERO"); (void)snprintf(out_value, value_size, "%s", (state->settings.dynamics.auto_zero_on_boot != 0u) ? "ON" : "OFF"); break;
        case 1u: (void)snprintf(out_label, label_size, "GNSS AID"); (void)snprintf(out_value, value_size, "%s", (state->settings.dynamics.gnss_aid_enabled != 0u) ? "ON" : "OFF"); break;
        case 2u: (void)snprintf(out_label, label_size, "OBD AID"); (void)snprintf(out_value, value_size, "%s", (state->settings.dynamics.obd_aid_enabled != 0u) ? "ON" : "OFF"); break;
        case 3u: (void)snprintf(out_label, label_size, "FWD AXIS"); (void)snprintf(out_value, value_size, "%s", motor_ui_axis_name(state->settings.dynamics.mount_forward_axis)); break;
        case 4u: (void)snprintf(out_label, label_size, "LEFT AXIS"); (void)snprintf(out_value, value_size, "%s", motor_ui_axis_name(state->settings.dynamics.mount_left_axis)); break;
        case 5u: (void)snprintf(out_label, label_size, "YAW TRIM"); (void)snprintf(out_value, value_size, "%+d.%01d", (int)(state->settings.dynamics.mount_yaw_trim_deg_x10 / 10), (int)abs((int)(state->settings.dynamics.mount_yaw_trim_deg_x10 % 10))); break;
        case 6u: (void)snprintf(out_label, label_size, "LEAN TAU"); (void)snprintf(out_value, value_size, "%u ms", (unsigned)state->settings.dynamics.lean_display_tau_ms); break;
        case 7u: (void)snprintf(out_label, label_size, "ACC TAU"); (void)snprintf(out_value, value_size, "%u ms", (unsigned)state->settings.dynamics.accel_display_tau_ms); break;
        case 8u: (void)snprintf(out_label, label_size, "ZERO CAPTURE"); (void)snprintf(out_value, value_size, "EXEC"); break;
        default: break;
        }
        break;

    case MOTOR_SCREEN_SETTINGS_MAINTENANCE:
        switch (row)
        {
        case 0u: (void)snprintf(out_label, label_size, "DUE SOON M"); (void)snprintf(out_value, value_size, "%lu", (unsigned long)state->settings.maintenance.due_soon_distance_m); break;
        case 1u: (void)snprintf(out_label, label_size, "DUE SOON D"); (void)snprintf(out_value, value_size, "%u", (unsigned)state->settings.maintenance.due_soon_days); break;
        case 2u: (void)snprintf(out_label, label_size, "RESET OIL"); (void)snprintf(out_value, value_size, "EXEC"); break;
        case 3u: (void)snprintf(out_label, label_size, "RESET CHAIN"); (void)snprintf(out_value, value_size, "EXEC"); break;
        case 4u: (void)snprintf(out_label, label_size, "RESET BRAKE"); (void)snprintf(out_value, value_size, "EXEC"); break;
        case 5u: (void)snprintf(out_label, label_size, "RESET FILTER"); (void)snprintf(out_value, value_size, "EXEC"); break;
        case 6u: (void)snprintf(out_label, label_size, "RESET CUST1"); (void)snprintf(out_value, value_size, "EXEC"); break;
        case 7u: (void)snprintf(out_label, label_size, "RESET CUST2"); (void)snprintf(out_value, value_size, "EXEC"); break;
        default: break;
        }
        break;

    case MOTOR_SCREEN_SETTINGS_OBD:
        switch (row)
        {
        case 0u: (void)snprintf(out_label, label_size, "AUTO CONNECT"); (void)snprintf(out_value, value_size, "%s", (state->settings.obd.autoconnect_enabled != 0u) ? "ON" : "OFF"); break;
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
        case 0u: (void)snprintf(out_label, label_size, "MENU WRAP"); (void)snprintf(out_value, value_size, "%s", (state->settings.system.menu_wrap_enabled != 0u) ? "ON" : "OFF"); break;
        case 1u: (void)snprintf(out_label, label_size, "RIDE SUMMARY"); (void)snprintf(out_value, value_size, "%s", (state->settings.system.ride_summary_popup_enabled != 0u) ? "ON" : "OFF"); break;
        case 2u: (void)snprintf(out_label, label_size, "DEBUG STUBS"); (void)snprintf(out_value, value_size, "%s", (state->settings.system.show_debug_stubs != 0u) ? "ON" : "OFF"); break;
        case 3u: (void)snprintf(out_label, label_size, "FACTORY RESET"); (void)snprintf(out_value, value_size, "EXEC"); break;
        case 4u: (void)snprintf(out_label, label_size, "FW"); (void)snprintf(out_value, value_size, "SKELETON"); break;
        case 5u: (void)snprintf(out_label, label_size, "LAYER"); (void)snprintf(out_value, value_size, "APP ONLY"); break;
        default: break;
        }
        break;

    default:
        break;
    }
}

void Motor_UI_DrawScreen_Settings(u8g2_t *u8g2, const ui_rect_t *viewport, const motor_state_t *state)
{
    motor_screen_t screen;
    uint8_t row_count;
    uint8_t first_row;
    uint8_t i;
    char label[24];
    char value[24];
    const char *title;

    if ((u8g2 == 0) || (viewport == 0) || (state == 0))
    {
        return;
    }

    screen = (motor_screen_t)state->ui.screen;
    title = "SETTINGS";

    switch (screen)
    {
    case MOTOR_SCREEN_SETTINGS_DISPLAY:     title = "DISPLAY"; break;
    case MOTOR_SCREEN_SETTINGS_GPS:         title = "GPS"; break;
    case MOTOR_SCREEN_SETTINGS_UNITS:       title = "UNITS"; break;
    case MOTOR_SCREEN_SETTINGS_RECORDING:   title = "RECORDING"; break;
    case MOTOR_SCREEN_SETTINGS_DYNAMICS:    title = "DYNAMICS"; break;
    case MOTOR_SCREEN_SETTINGS_MAINTENANCE: title = "MAINTENANCE"; break;
    case MOTOR_SCREEN_SETTINGS_OBD:         title = "OBD"; break;
    case MOTOR_SCREEN_SETTINGS_SYSTEM:      title = "SYSTEM"; break;
    case MOTOR_SCREEN_SETTINGS_ROOT:
    default:                                title = "SETTINGS"; break;
    }

    Motor_UI_DrawViewportTitle(u8g2, viewport, title);

    switch (screen)
    {
    case MOTOR_SCREEN_SETTINGS_ROOT:        row_count = 8u; break;
    case MOTOR_SCREEN_SETTINGS_DISPLAY:     row_count = 13u; break;
    case MOTOR_SCREEN_SETTINGS_GPS:         row_count = 10u; break;
    case MOTOR_SCREEN_SETTINGS_UNITS:       row_count = 7u; break;
    case MOTOR_SCREEN_SETTINGS_RECORDING:   row_count = 6u; break;
    case MOTOR_SCREEN_SETTINGS_DYNAMICS:    row_count = 9u; break;
    case MOTOR_SCREEN_SETTINGS_MAINTENANCE: row_count = 8u; break;
    case MOTOR_SCREEN_SETTINGS_OBD:         row_count = 5u; break;
    case MOTOR_SCREEN_SETTINGS_SYSTEM:      row_count = 6u; break;
    default:                                row_count = 0u; break;
    }

    first_row = state->ui.first_visible_row;
    if (first_row > state->ui.selected_index)
    {
        first_row = state->ui.selected_index;
    }

    for (i = 0u; (i < MOTOR_SETTINGS_VISIBLE_ROWS) && ((first_row + i) < row_count); i++)
    {
        motor_ui_settings_get_row_text(state,
                                       screen,
                                       (uint8_t)(first_row + i),
                                       label,
                                       sizeof(label),
                                       value,
                                       sizeof(value));
        motor_ui_settings_draw_row(u8g2,
                                   viewport,
                                   i,
                                   ((first_row + i) == state->ui.selected_index) ? 1u : 0u,
                                   label,
                                   value);
    }

    /* ---------------------------------------------------------------------- */
    /*  페이지별 보조 표시                                                     */
    /* ---------------------------------------------------------------------- */
    if (screen == MOTOR_SCREEN_SETTINGS_DISPLAY)
    {
        motor_ui_settings_draw_display_aux(u8g2, viewport, state);
        motor_ui_settings_draw_footer(u8g2, viewport, "B3/B4 adjust, B5 execute, B6 back");
    }
    else if (screen == MOTOR_SCREEN_SETTINGS_ROOT)
    {
        motor_ui_settings_draw_footer(u8g2, viewport, "B5 enter submenu, B6 return to drive");
    }
    else if (screen == MOTOR_SCREEN_SETTINGS_DYNAMICS)
    {
        motor_ui_settings_draw_footer(u8g2, viewport, "Zero capture needs bike stopped");
    }
    else if (screen == MOTOR_SCREEN_SETTINGS_GPS)
    {
        motor_ui_settings_draw_footer(u8g2, viewport, "Profile + filtering + sync options");
    }
    else
    {
        motor_ui_settings_draw_footer(u8g2, viewport, "B1/B2 row, B3/B4 change, B6 back");
    }
}
