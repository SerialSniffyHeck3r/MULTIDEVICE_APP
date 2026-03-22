
#include "Motor_Buttons.h"

#include "Motor_DataField.h"
#include "Motor_Dynamics.h"
#include "Motor_Maintenance.h"
#include "Motor_Record.h"
#include "Motor_Units.h"
#include "Motor_Settings.h"
#include "Motor_State.h"
#include "Motor_Vehicle.h"
#include "button.h"

#include <string.h>

static const motor_screen_t s_drive_screens[] =
{
    MOTOR_SCREEN_MAIN,
    MOTOR_SCREEN_DATA_FIELD_1,
    MOTOR_SCREEN_DATA_FIELD_2,
    MOTOR_SCREEN_CORNER,
    MOTOR_SCREEN_COMPASS,
    MOTOR_SCREEN_BREADCRUMB,
    MOTOR_SCREEN_ALTITUDE,
    MOTOR_SCREEN_HORIZON,
    MOTOR_SCREEN_VEHICLE_SUMMARY
};

static uint8_t motor_buttons_get_drive_screen_index(motor_screen_t screen)
{
    uint8_t i;
    for (i = 0u; i < (uint8_t)(sizeof(s_drive_screens) / sizeof(s_drive_screens[0])); i++)
    {
        if (s_drive_screens[i] == screen)
        {
            return i;
        }
    }
    return 0u;
}

static void motor_buttons_cycle_drive_screen(int8_t delta)
{
    motor_state_t *state;
    uint8_t index;
    uint8_t count;

    state = Motor_State_GetMutable();
    if (state == 0)
    {
        return;
    }

    count = (uint8_t)(sizeof(s_drive_screens) / sizeof(s_drive_screens[0]));
    index = motor_buttons_get_drive_screen_index((motor_screen_t)state->ui.screen);

    if (delta > 0)
    {
        index = (uint8_t)((index + 1u) % count);
    }
    else
    {
        index = (uint8_t)((index == 0u) ? (count - 1u) : (index - 1u));
    }

    Motor_State_SetScreen(s_drive_screens[index]);
}

static void motor_buttons_handle_drive(const button_event_t *ev)
{
    motor_state_t *state;

    state = Motor_State_GetMutable();
    if ((state == 0) || (ev == 0))
    {
        return;
    }

    if ((state->ui.screen_locked != 0u) &&
        !((ev->id == BUTTON_ID_6) && (ev->type == BUTTON_EVENT_LONG_PRESS)))
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  Data Field 화면의 slot editing                                          */
    /*  - 이 블록은 drive 공통 버튼 해석보다 먼저 배치한다.                     */
    /*  - 이유: edit 중에는 B1/B2/B3/B4/B5 의미가 ride 공통 동작과 달라진다.   */
    /* ---------------------------------------------------------------------- */
    if (((motor_screen_t)state->ui.screen == MOTOR_SCREEN_DATA_FIELD_1) ||
        ((motor_screen_t)state->ui.screen == MOTOR_SCREEN_DATA_FIELD_2))
    {
        motor_settings_t *settings;
        uint8_t page_idx;
        uint8_t slot;
        uint8_t catalog_count;
        uint8_t catalog_idx;
        uint8_t i;

        settings = Motor_Settings_GetMutable();
        if (settings != 0)
        {
            page_idx = ((motor_screen_t)state->ui.screen == MOTOR_SCREEN_DATA_FIELD_1) ? 0u : 1u;
            slot = state->ui.selected_slot;
            if (slot >= MOTOR_DATA_FIELD_SLOT_COUNT)
            {
                state->ui.selected_slot = 0u;
                slot = 0u;
            }

            if ((ev->id == BUTTON_ID_5) && (ev->type == BUTTON_EVENT_SHORT_PRESS))
            {
                state->ui.editing = (state->ui.editing == 0u) ? 1u : 0u;
                Motor_State_ShowToast((state->ui.editing != 0u) ? "DF EDIT" : "DF DONE", 900u);
                return;
            }

            if (state->ui.editing != 0u)
            {
                if ((ev->id == BUTTON_ID_1) && (ev->type == BUTTON_EVENT_SHORT_PRESS))
                {
                    state->ui.selected_slot = (uint8_t)((slot == 0u) ? (MOTOR_DATA_FIELD_SLOT_COUNT - 1u) : (slot - 1u));
                    return;
                }
                if ((ev->id == BUTTON_ID_2) && (ev->type == BUTTON_EVENT_SHORT_PRESS))
                {
                    state->ui.selected_slot = (uint8_t)((slot + 1u) % MOTOR_DATA_FIELD_SLOT_COUNT);
                    return;
                }

                catalog_count = Motor_DataField_GetCatalogCount();
                catalog_idx = 0u;
                for (i = 0u; i < catalog_count; i++)
                {
                    if (Motor_DataField_GetByCatalogIndex(i) == (motor_data_field_id_t)settings->data_fields[page_idx][slot])
                    {
                        catalog_idx = i;
                        break;
                    }
                }

                if ((ev->id == BUTTON_ID_3) && (ev->type == BUTTON_EVENT_SHORT_PRESS))
                {
                    catalog_idx = (uint8_t)((catalog_idx == 0u) ? (catalog_count - 1u) : (catalog_idx - 1u));
                    settings->data_fields[page_idx][slot] = (uint8_t)Motor_DataField_GetByCatalogIndex(catalog_idx);
                    Motor_Settings_Commit();
                    return;
                }
                if ((ev->id == BUTTON_ID_4) && (ev->type == BUTTON_EVENT_SHORT_PRESS))
                {
                    catalog_idx = (uint8_t)((catalog_idx + 1u) % catalog_count);
                    settings->data_fields[page_idx][slot] = (uint8_t)Motor_DataField_GetByCatalogIndex(catalog_idx);
                    Motor_Settings_Commit();
                    return;
                }
            }
        }
    }

    /* ---------------------------------------------------------------------- */
    /*  공통 drive screen 조작                                                 */
    /*  B1/B2 : 이전/다음 화면                                                 */
    /*  B3    : marker                                                          */
    /*  B4    : peak reset / long zero capture                                  */
    /*  B5    : record toggle / long stop                                       */
    /*  B6    : menu, long lock                                                 */
    /* ---------------------------------------------------------------------- */
    if ((ev->id == BUTTON_ID_1) && (ev->type == BUTTON_EVENT_SHORT_PRESS))
    {
        motor_buttons_cycle_drive_screen(-1);
        return;
    }

    if ((ev->id == BUTTON_ID_2) && (ev->type == BUTTON_EVENT_SHORT_PRESS))
    {
        motor_buttons_cycle_drive_screen(+1);
        return;
    }

    if ((ev->id == BUTTON_ID_3) && (ev->type == BUTTON_EVENT_SHORT_PRESS))
    {
        Motor_Record_RequestMarker();
        return;
    }

    if ((ev->id == BUTTON_ID_4) && (ev->type == BUTTON_EVENT_LONG_PRESS))
    {
        Motor_Dynamics_RequestZeroCapture();
        Motor_State_ShowToast("ZERO CAPTURE", 1200u);
        return;
    }

    if ((ev->id == BUTTON_ID_4) && (ev->type == BUTTON_EVENT_SHORT_PRESS))
    {
        Motor_Dynamics_ResetSessionPeaks();
        Motor_State_ShowToast("PEAK RESET", 1000u);
        return;
    }

    if ((ev->id == BUTTON_ID_5) && (ev->type == BUTTON_EVENT_SHORT_PRESS))
    {
        Motor_State_RequestRecordToggle();
        return;
    }

    if ((ev->id == BUTTON_ID_5) && (ev->type == BUTTON_EVENT_LONG_PRESS))
    {
        Motor_Record_RequestStop();
        return;
    }

    if ((ev->id == BUTTON_ID_6) && (ev->type == BUTTON_EVENT_SHORT_PRESS))
    {
        Motor_State_SetScreen(MOTOR_SCREEN_MENU);
        return;
    }

    if ((ev->id == BUTTON_ID_6) && (ev->type == BUTTON_EVENT_LONG_PRESS))
    {
        state->ui.screen_locked = (state->ui.screen_locked == 0u) ? 1u : 0u;
        Motor_State_ShowToast((state->ui.screen_locked != 0u) ? "LOCK" : "UNLOCK", 900u);
        return;
    }
}

static void motor_buttons_handle_menu(const button_event_t *ev)
{
    motor_state_t *state;
    motor_menu_item_t selected;

    state = Motor_State_GetMutable();
    if ((state == 0) || (ev == 0))
    {
        return;
    }

    if ((ev->id == BUTTON_ID_1) && (ev->type == BUTTON_EVENT_SHORT_PRESS))
    {
        state->ui.selected_index = (uint8_t)((state->ui.selected_index == 0u) ? (MOTOR_MENU_ITEM_COUNT - 1u) : (state->ui.selected_index - 1u));
        return;
    }
    if ((ev->id == BUTTON_ID_2) && (ev->type == BUTTON_EVENT_SHORT_PRESS))
    {
        state->ui.selected_index = (uint8_t)((state->ui.selected_index + 1u) % MOTOR_MENU_ITEM_COUNT);
        return;
    }
    if ((ev->id == BUTTON_ID_6) && (ev->type == BUTTON_EVENT_SHORT_PRESS))
    {
        Motor_State_SetScreen((motor_screen_t)state->ui.previous_drive_screen);
        return;
    }

    if (!((ev->id == BUTTON_ID_5) && (ev->type == BUTTON_EVENT_SHORT_PRESS)))
    {
        return;
    }

    selected = (motor_menu_item_t)state->ui.selected_index;
    switch (selected)
    {
    case MOTOR_MENU_ITEM_ACCEL_TEST:
        Motor_State_SetScreen(MOTOR_SCREEN_ACCEL_TEST_STUB);
        break;
    case MOTOR_MENU_ITEM_LAP_TIMER:
        Motor_State_SetScreen(MOTOR_SCREEN_LAP_TIMER_STUB);
        break;
    case MOTOR_MENU_ITEM_RIDE_LOGS:
        Motor_State_SetScreen(MOTOR_SCREEN_LOG_VIEW_STUB);
        break;
    case MOTOR_MENU_ITEM_OBD_CONNECT:
        if (state->vehicle.connected != false)
        {
            Motor_Vehicle_RequestDisconnect();
        }
        else
        {
            Motor_Vehicle_RequestConnect();
        }
        Motor_State_SetScreen(MOTOR_SCREEN_OBD_CONNECT_STUB);
        break;
    case MOTOR_MENU_ITEM_OBD_DTC:
        Motor_State_SetScreen(MOTOR_SCREEN_OBD_DTC_STUB);
        break;
    case MOTOR_MENU_ITEM_SETTINGS:
    default:
        Motor_State_SetScreen(MOTOR_SCREEN_SETTINGS_ROOT);
        break;
    }
}

static uint8_t motor_buttons_get_settings_row_count(motor_screen_t screen)
{
    switch (screen)
    {
    case MOTOR_SCREEN_SETTINGS_ROOT:        return 8u;
    case MOTOR_SCREEN_SETTINGS_DISPLAY:     return 6u;
    case MOTOR_SCREEN_SETTINGS_GPS:         return 8u;
    case MOTOR_SCREEN_SETTINGS_UNITS:       return 6u;
    case MOTOR_SCREEN_SETTINGS_RECORDING:   return 6u;
    case MOTOR_SCREEN_SETTINGS_DYNAMICS:    return 8u;
    case MOTOR_SCREEN_SETTINGS_MAINTENANCE: return 6u;
    case MOTOR_SCREEN_SETTINGS_OBD:         return 4u;
    case MOTOR_SCREEN_SETTINGS_SYSTEM:      return 4u;
    default:                                return 0u;
    }
}

static void motor_buttons_adjust_setting_row(int8_t delta)
{
    motor_state_t *state;
    motor_settings_t *settings;

    state = Motor_State_GetMutable();
    settings = Motor_Settings_GetMutable();
    if ((state == 0) || (settings == 0))
    {
        return;
    }

    switch ((motor_screen_t)state->ui.screen)
    {
    case MOTOR_SCREEN_SETTINGS_DISPLAY:
        switch (state->ui.selected_index)
        {
        case 0u:
            settings->display.brightness_mode = (uint8_t)((settings->display.brightness_mode + MOTOR_DISPLAY_BRIGHTNESS_COUNT + delta) % MOTOR_DISPLAY_BRIGHTNESS_COUNT);
            break;
        case 1u:
            if (delta > 0)
            {
                if (settings->display.manual_brightness_percent < 100u) settings->display.manual_brightness_percent++;
            }
            else if (settings->display.manual_brightness_percent > 1u) settings->display.manual_brightness_percent--;
            break;
        case 2u:
            settings->display.auto_continuous_bias_steps += delta;
            if (settings->display.auto_continuous_bias_steps < -2) settings->display.auto_continuous_bias_steps = -2;
            if (settings->display.auto_continuous_bias_steps > 2) settings->display.auto_continuous_bias_steps = 2;
            break;
        case 3u:
            settings->display.screen_flip_enabled = (settings->display.screen_flip_enabled == 0u) ? 1u : 0u;
            break;
        case 4u:
            settings->display.contrast_raw = (uint8_t)((int32_t)settings->display.contrast_raw + delta * 2);
            break;
        case 5u:
            settings->display.temperature_compensation_raw = (uint8_t)((settings->display.temperature_compensation_raw + 4 + delta) % 4);
            break;
        default:
            break;
        }
        break;

    case MOTOR_SCREEN_SETTINGS_GPS:
        switch (state->ui.selected_index)
        {
        case 0u:
            settings->gps.receiver_profile = (settings->gps.receiver_profile == (uint8_t)APP_GPS_BOOT_PROFILE_GPS_ONLY_20HZ)
                                           ? (uint8_t)APP_GPS_BOOT_PROFILE_MULTI_CONST_10HZ
                                           : (uint8_t)APP_GPS_BOOT_PROFILE_GPS_ONLY_20HZ;
            break;
        case 1u:
            settings->gps.power_profile = (settings->gps.power_profile == (uint8_t)APP_GPS_POWER_PROFILE_HIGH_POWER)
                                        ? (uint8_t)APP_GPS_POWER_PROFILE_POWER_SAVE
                                        : (uint8_t)APP_GPS_POWER_PROFILE_HIGH_POWER;
            break;
        case 2u:
            settings->gps.dynamic_model = (settings->gps.dynamic_model == (uint8_t)MOTOR_GPS_DYNAMIC_MODEL_AUTOMOTIVE)
                                        ? (uint8_t)MOTOR_GPS_DYNAMIC_MODEL_PORTABLE
                                        : (uint8_t)MOTOR_GPS_DYNAMIC_MODEL_AUTOMOTIVE;
            break;
        case 3u:
            settings->gps.static_hold_enabled = (settings->gps.static_hold_enabled == 0u) ? 1u : 0u;
            break;
        case 4u:
            settings->gps.low_speed_course_filter_enabled = (settings->gps.low_speed_course_filter_enabled == 0u) ? 1u : 0u;
            break;
        case 5u:
            settings->gps.low_speed_velocity_filter_enabled = (settings->gps.low_speed_velocity_filter_enabled == 0u) ? 1u : 0u;
            break;
        case 6u:
            settings->gps.rtc_gps_sync_enabled = (settings->gps.rtc_gps_sync_enabled == 0u) ? 1u : 0u;
            break;
        case 7u:
            settings->gps.rtc_gps_sync_interval_min = (uint8_t)((int32_t)settings->gps.rtc_gps_sync_interval_min + delta);
            if (settings->gps.rtc_gps_sync_interval_min < 1u) settings->gps.rtc_gps_sync_interval_min = 1u;
            if (settings->gps.rtc_gps_sync_interval_min > 60u) settings->gps.rtc_gps_sync_interval_min = 60u;
            break;
        default:
            break;
        }
        break;

    case MOTOR_SCREEN_SETTINGS_UNITS:
        switch (state->ui.selected_index)
        {
        case 0u:
        {
            uint8_t preset;
            preset = (uint8_t)((settings->units.preset + MOTOR_UNIT_PRESET_COUNT + delta) % MOTOR_UNIT_PRESET_COUNT);
            if (preset == (uint8_t)MOTOR_UNIT_PRESET_CUSTOM)
            {
                preset = (delta > 0) ? 0u : (uint8_t)MOTOR_UNIT_PRESET_IMPERIAL;
            }
            Motor_Units_ApplyPreset(&settings->units, (motor_unit_preset_t)preset);
            break;
        }
        case 1u:
            settings->units.speed = (uint8_t)((settings->units.speed + MOTOR_SPEED_UNIT_COUNT + delta) % MOTOR_SPEED_UNIT_COUNT);
            break;
        case 2u:
            settings->units.distance = (uint8_t)((settings->units.distance + MOTOR_DISTANCE_UNIT_COUNT + delta) % MOTOR_DISTANCE_UNIT_COUNT);
            break;
        case 3u:
            settings->units.altitude = (uint8_t)((settings->units.altitude + MOTOR_ALTITUDE_UNIT_COUNT + delta) % MOTOR_ALTITUDE_UNIT_COUNT);
            break;
        case 4u:
            settings->units.temperature = (uint8_t)((settings->units.temperature + MOTOR_TEMP_UNIT_COUNT + delta) % MOTOR_TEMP_UNIT_COUNT);
            break;
        case 5u:
            settings->units.pressure = (uint8_t)((settings->units.pressure + MOTOR_PRESSURE_UNIT_COUNT + delta) % MOTOR_PRESSURE_UNIT_COUNT);
            break;
        default:
            break;
        }
        break;

    case MOTOR_SCREEN_SETTINGS_RECORDING:
        switch (state->ui.selected_index)
        {
        case 0u:
            settings->recording.auto_start_enabled = (settings->recording.auto_start_enabled == 0u) ? 1u : 0u;
            break;
        case 1u:
            settings->recording.auto_start_speed_kmh_x10 = (uint16_t)((int32_t)settings->recording.auto_start_speed_kmh_x10 + delta * 5);
            break;
        case 2u:
            settings->recording.auto_stop_idle_seconds = (uint16_t)((int32_t)settings->recording.auto_stop_idle_seconds + delta * 5);
            break;
        case 3u:
            settings->recording.auto_pause_enabled = (settings->recording.auto_pause_enabled == 0u) ? 1u : 0u;
            break;
        case 4u:
            settings->recording.raw_imu_log_enabled = (settings->recording.raw_imu_log_enabled == 0u) ? 1u : 0u;
            break;
        case 5u:
            settings->recording.summary_popup_enabled = (settings->recording.summary_popup_enabled == 0u) ? 1u : 0u;
            break;
        default:
            break;
        }
        break;

    case MOTOR_SCREEN_SETTINGS_DYNAMICS:
        switch (state->ui.selected_index)
        {
        case 0u:
            settings->dynamics.auto_zero_on_boot = (settings->dynamics.auto_zero_on_boot == 0u) ? 1u : 0u;
            break;
        case 1u:
            settings->dynamics.gnss_aid_enabled = (settings->dynamics.gnss_aid_enabled == 0u) ? 1u : 0u;
            break;
        case 2u:
            settings->dynamics.obd_aid_enabled = (settings->dynamics.obd_aid_enabled == 0u) ? 1u : 0u;
            break;
        case 3u:
            settings->dynamics.mount_forward_axis = (uint8_t)((settings->dynamics.mount_forward_axis + 6u + delta) % 6u);
            break;
        case 4u:
            settings->dynamics.mount_left_axis = (uint8_t)((settings->dynamics.mount_left_axis + 6u + delta) % 6u);
            break;
        case 5u:
            settings->dynamics.mount_yaw_trim_deg_x10 = (int16_t)((int32_t)settings->dynamics.mount_yaw_trim_deg_x10 + delta);
            break;
        case 6u:
            settings->dynamics.lean_display_tau_ms = (uint16_t)((int32_t)settings->dynamics.lean_display_tau_ms + delta * 10);
            break;
        case 7u:
            settings->dynamics.accel_display_tau_ms = (uint16_t)((int32_t)settings->dynamics.accel_display_tau_ms + delta * 10);
            break;
        default:
            break;
        }
        break;

    case MOTOR_SCREEN_SETTINGS_MAINTENANCE:
        switch (state->ui.selected_index)
        {
        case 0u:
            settings->maintenance.due_soon_distance_m = (uint32_t)((int32_t)settings->maintenance.due_soon_distance_m + delta * 1000);
            break;
        case 1u:
            settings->maintenance.due_soon_days = (uint16_t)((int32_t)settings->maintenance.due_soon_days + delta);
            break;
        case 2u:
            Motor_Maintenance_ResetService(0u);
            break;
        case 3u:
            Motor_Maintenance_ResetService(1u);
            break;
        case 4u:
            Motor_Maintenance_ResetService(2u);
            break;
        case 5u:
            Motor_Maintenance_ResetService(3u);
            break;
        default:
            break;
        }
        break;

    case MOTOR_SCREEN_SETTINGS_OBD:
        switch (state->ui.selected_index)
        {
        case 0u:
            settings->obd.autoconnect_enabled = (settings->obd.autoconnect_enabled == 0u) ? 1u : 0u;
            break;
        case 1u:
            settings->obd.preferred_speed_source = (settings->obd.preferred_speed_source == (uint8_t)APP_BIKE_SPEED_SOURCE_GNSS)
                                                 ? (uint8_t)APP_BIKE_SPEED_SOURCE_OBD
                                                 : (uint8_t)APP_BIKE_SPEED_SOURCE_GNSS;
            break;
        case 2u:
            settings->obd.coolant_warn_temp_c_x10 = (uint16_t)((int32_t)settings->obd.coolant_warn_temp_c_x10 + delta * 5);
            break;
        case 3u:
            settings->obd.shift_light_rpm = (uint16_t)((int32_t)settings->obd.shift_light_rpm + delta * 250);
            break;
        default:
            break;
        }
        break;

    default:
        break;
    }

    Motor_Settings_Commit();
}

static void motor_buttons_handle_settings(const button_event_t *ev)
{
    motor_state_t *state;

    state = Motor_State_GetMutable();
    if ((state == 0) || (ev == 0))
    {
        return;
    }

    if ((motor_screen_t)state->ui.screen == MOTOR_SCREEN_SETTINGS_ROOT)
    {
        if ((ev->id == BUTTON_ID_1) && (ev->type == BUTTON_EVENT_SHORT_PRESS))
        {
            state->ui.selected_index = (uint8_t)((state->ui.selected_index == 0u) ? 7u : (state->ui.selected_index - 1u));
            return;
        }
        if ((ev->id == BUTTON_ID_2) && (ev->type == BUTTON_EVENT_SHORT_PRESS))
        {
            state->ui.selected_index = (uint8_t)((state->ui.selected_index + 1u) % 8u);
            return;
        }
        if ((ev->id == BUTTON_ID_6) && (ev->type == BUTTON_EVENT_SHORT_PRESS))
        {
            Motor_State_SetScreen(MOTOR_SCREEN_MENU);
            return;
        }
        if ((ev->id == BUTTON_ID_5) && (ev->type == BUTTON_EVENT_SHORT_PRESS))
        {
            Motor_State_SetScreen((motor_screen_t)(MOTOR_SCREEN_SETTINGS_DISPLAY + state->ui.selected_index));
            return;
        }
        return;
    }

    if ((ev->id == BUTTON_ID_6) && (ev->type == BUTTON_EVENT_SHORT_PRESS))
    {
        Motor_State_SetScreen(MOTOR_SCREEN_SETTINGS_ROOT);
        return;
    }
    if ((ev->id == BUTTON_ID_1) && (ev->type == BUTTON_EVENT_SHORT_PRESS))
    {
        if (state->ui.selected_index > 0u)
        {
            state->ui.selected_index--;
        }
        return;
    }
    if ((ev->id == BUTTON_ID_2) && (ev->type == BUTTON_EVENT_SHORT_PRESS))
    {
        uint8_t row_count;
        row_count = motor_buttons_get_settings_row_count((motor_screen_t)state->ui.screen);
        if ((row_count != 0u) && (state->ui.selected_index + 1u < row_count))
        {
            state->ui.selected_index++;
        }
        return;
    }
    if ((ev->id == BUTTON_ID_3) && (ev->type == BUTTON_EVENT_SHORT_PRESS))
    {
        motor_buttons_adjust_setting_row(-1);
        return;
    }
    if ((ev->id == BUTTON_ID_4) && (ev->type == BUTTON_EVENT_SHORT_PRESS))
    {
        motor_buttons_adjust_setting_row(+1);
        return;
    }
}

void Motor_Buttons_Init(void)
{
}

void Motor_Buttons_Task(uint32_t now_ms)
{
    button_event_t ev;
    motor_state_t *state;

    (void)now_ms;
    state = Motor_State_GetMutable();
    if (state == 0)
    {
        return;
    }

    while (Button_PopEvent(&ev) != false)
    {
        if ((motor_screen_t)state->ui.screen < MOTOR_SCREEN_MENU)
        {
            motor_buttons_handle_drive(&ev);
        }
        else if ((motor_screen_t)state->ui.screen == MOTOR_SCREEN_MENU)
        {
            motor_buttons_handle_menu(&ev);
        }
        else if (((motor_screen_t)state->ui.screen >= MOTOR_SCREEN_SETTINGS_ROOT) &&
                 ((motor_screen_t)state->ui.screen <= MOTOR_SCREEN_SETTINGS_SYSTEM))
        {
            motor_buttons_handle_settings(&ev);
        }
        else
        {
            if ((ev.id == BUTTON_ID_6) && (ev.type == BUTTON_EVENT_SHORT_PRESS))
            {
                Motor_State_SetScreen(MOTOR_SCREEN_MENU);
            }
        }
    }
}
