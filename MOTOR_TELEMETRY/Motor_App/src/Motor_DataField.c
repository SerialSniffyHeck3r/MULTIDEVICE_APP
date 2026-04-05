
#include "Motor_DataField.h"

#include "Motor_Units.h"

#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*  catalog 문자열 테이블                                                      */
/*                                                                            */
/*  label은 최대한 짧게 유지해서 240x128의 좁은 셀 안에서도 읽히게 한다.       */
/* -------------------------------------------------------------------------- */
static const char *const s_field_names[MOTOR_FIELD_COUNT] =
{
    "NONE",
    "SPEED",
    "SRC",
    "BANK",
    "BMAX L",
    "BMAX R",
    "LAT G",
    "LAT L",
    "LAT R",
    "ACCEL",
    "ACC MAX",
    "BRK MAX",
    "ALT",
    "REL ALT",
    "GRADE",
    "HEAD",
    "FIX",
    "SATS",
    "HACC",
    "VACC",
    "LAT",
    "LON",
    "TIME",
    "RIDE",
    "MOVE",
    "DIST",
    "TRIP A",
    "TRIP B",
    "COOLANT",
    "RPM",
    "GEAR",
    "BATT",
    "OBD",
    "MAINT",
    "NEXT SVC",
    "REC",
    "SD",
    "BT",
    "ENG H",
    "AIR TMP"
};

static const motor_data_field_id_t s_catalog[] =
{
    MOTOR_FIELD_SPEED,
    MOTOR_FIELD_SPEED_SOURCE,
    MOTOR_FIELD_BANK,
    MOTOR_FIELD_BANK_MAX_LEFT,
    MOTOR_FIELD_BANK_MAX_RIGHT,
    MOTOR_FIELD_G_LAT,
    MOTOR_FIELD_G_LAT_MAX_LEFT,
    MOTOR_FIELD_G_LAT_MAX_RIGHT,
    MOTOR_FIELD_G_LON,
    MOTOR_FIELD_G_ACCEL_MAX,
    MOTOR_FIELD_G_BRAKE_MAX,
    MOTOR_FIELD_ALTITUDE,
    MOTOR_FIELD_REL_ALTITUDE,
    MOTOR_FIELD_GRADE_PERCENT,
    MOTOR_FIELD_HEADING,
    MOTOR_FIELD_GPS_FIX,
    MOTOR_FIELD_GPS_SATS,
    MOTOR_FIELD_GPS_HACC,
    MOTOR_FIELD_GPS_VACC,
    MOTOR_FIELD_LATITUDE,
    MOTOR_FIELD_LONGITUDE,
    MOTOR_FIELD_TIME,
    MOTOR_FIELD_RIDE_TIME,
    MOTOR_FIELD_MOVING_TIME,
    MOTOR_FIELD_DISTANCE,
    MOTOR_FIELD_TRIP_A,
    MOTOR_FIELD_TRIP_B,
    MOTOR_FIELD_COOLANT_TEMP,
    MOTOR_FIELD_RPM,
    MOTOR_FIELD_GEAR,
    MOTOR_FIELD_BATTERY,
    MOTOR_FIELD_OBD_LINK,
    MOTOR_FIELD_MAINT_DUE_COUNT,
    MOTOR_FIELD_NEXT_SERVICE,
    MOTOR_FIELD_RECORD_STATE,
    MOTOR_FIELD_SD_STATE,
    MOTOR_FIELD_BT_STATE,
    MOTOR_FIELD_ENGINE_HOURS,
    MOTOR_FIELD_OUTSIDE_TEMP
};

static void motor_field_format_seconds(char *out_text, size_t out_size, uint32_t seconds)
{
    uint32_t h;
    uint32_t m;
    uint32_t s;

    if ((out_text == 0) || (out_size == 0u))
    {
        return;
    }

    h = seconds / 3600u;
    m = (seconds % 3600u) / 60u;
    s = seconds % 60u;
    (void)snprintf(out_text, out_size, "%02lu:%02lu:%02lu", (unsigned long)h, (unsigned long)m, (unsigned long)s);
}

static void motor_field_format_signed_x10(char *out_text, size_t out_size, int32_t value_x10)
{
    int32_t abs_value;
    char sign;

    if ((out_text == 0) || (out_size == 0u))
    {
        return;
    }

    sign = (value_x10 < 0) ? '-' : '+';
    abs_value = (value_x10 < 0) ? -value_x10 : value_x10;
    (void)snprintf(out_text, out_size, "%c%ld.%01ld", sign, (long)(abs_value / 10), (long)(abs_value % 10));
}

static void motor_field_format_coord(char *out_text, size_t out_size, int32_t coord_e7)
{
    int32_t abs_value;
    int32_t deg;
    int32_t frac;
    char sign;

    if ((out_text == 0) || (out_size == 0u))
    {
        return;
    }

    sign = (coord_e7 < 0) ? '-' : '+';
    abs_value = (coord_e7 < 0) ? -coord_e7 : coord_e7;
    deg = abs_value / 10000000;
    frac = abs_value % 10000000;
    (void)snprintf(out_text, out_size, "%c%ld.%07ld", sign, (long)deg, (long)frac);
}

const char *Motor_DataField_GetFieldName(motor_data_field_id_t field_id)
{
    if ((uint32_t)field_id >= (uint32_t)MOTOR_FIELD_COUNT)
    {
        return "FIELD";
    }
    return s_field_names[(uint32_t)field_id];
}

uint8_t Motor_DataField_GetCatalogCount(void)
{
    return (uint8_t)(sizeof(s_catalog) / sizeof(s_catalog[0]));
}

motor_data_field_id_t Motor_DataField_GetByCatalogIndex(uint8_t index)
{
    if ((uint32_t)index >= (uint32_t)(sizeof(s_catalog) / sizeof(s_catalog[0])))
    {
        return MOTOR_FIELD_NONE;
    }
    return s_catalog[index];
}

void Motor_DataField_Format(motor_data_field_id_t field_id,
                            const motor_state_t *state,
                            motor_data_field_text_t *out_text)
{
    const motor_unit_settings_t *units;

    if ((state == 0) || (out_text == 0))
    {
        return;
    }

    units = &state->settings.units;
    memset(out_text, 0, sizeof(*out_text));
    (void)snprintf(out_text->label, sizeof(out_text->label), "%s", Motor_DataField_GetFieldName(field_id));

    switch (field_id)
    {
    case MOTOR_FIELD_SPEED:
        Motor_Units_FormatSpeed(out_text->value, sizeof(out_text->value), state->nav.speed_kmh_x10, units);
        break;

    case MOTOR_FIELD_SPEED_SOURCE:
        switch ((app_bike_speed_source_t)state->dyn.speed_source)
        {
        case APP_BIKE_SPEED_SOURCE_GNSS:
            (void)snprintf(out_text->value, sizeof(out_text->value), "GNSS");
            break;
        case APP_BIKE_SPEED_SOURCE_OBD:
            (void)snprintf(out_text->value, sizeof(out_text->value), "OBD");
            break;
        case APP_BIKE_SPEED_SOURCE_IMU_FALLBACK:
            (void)snprintf(out_text->value, sizeof(out_text->value), "IMU");
            break;
        default:
            (void)snprintf(out_text->value, sizeof(out_text->value), "NONE");
            break;
        }
        break;

    case MOTOR_FIELD_BANK:
        motor_field_format_signed_x10(out_text->value, sizeof(out_text->value), state->dyn.bank_deg_x10);
        break;

    case MOTOR_FIELD_BANK_MAX_LEFT:
        motor_field_format_signed_x10(out_text->value, sizeof(out_text->value), state->dyn.max_left_bank_deg_x10);
        break;

    case MOTOR_FIELD_BANK_MAX_RIGHT:
        motor_field_format_signed_x10(out_text->value, sizeof(out_text->value), state->dyn.max_right_bank_deg_x10);
        break;

    case MOTOR_FIELD_G_LAT:
        motor_field_format_signed_x10(out_text->value, sizeof(out_text->value), state->dyn.lat_accel_mg / 100);
        break;

    case MOTOR_FIELD_G_LAT_MAX_LEFT:
        motor_field_format_signed_x10(out_text->value, sizeof(out_text->value), state->dyn.max_left_lat_mg / 100);
        break;

    case MOTOR_FIELD_G_LAT_MAX_RIGHT:
        motor_field_format_signed_x10(out_text->value, sizeof(out_text->value), state->dyn.max_right_lat_mg / 100);
        break;

    case MOTOR_FIELD_G_LON:
        motor_field_format_signed_x10(out_text->value, sizeof(out_text->value), state->dyn.lon_accel_mg / 100);
        break;

    case MOTOR_FIELD_G_ACCEL_MAX:
        motor_field_format_signed_x10(out_text->value, sizeof(out_text->value), state->dyn.max_accel_mg / 100);
        break;

    case MOTOR_FIELD_G_BRAKE_MAX:
        motor_field_format_signed_x10(out_text->value, sizeof(out_text->value), state->dyn.max_brake_mg / 100);
        break;

    case MOTOR_FIELD_ALTITUDE:
        /* ------------------------------------------------------------------ */
        /*  canonical altitude owner는 APP_ALTITUDE -> APP_STATE.altitude 이다. */
        /*  Motor data field는 그 low-level unit bank에서 선택만 수행한다.     */
        /* ------------------------------------------------------------------ */
        Motor_Units_FormatAltitudeFromUnitBank(out_text->value,
                                               sizeof(out_text->value),
                                               &state->snapshot.altitude.units.alt_display,
                                               units);
        break;

    case MOTOR_FIELD_REL_ALTITUDE:
        Motor_Units_FormatAltitudeFromUnitBank(out_text->value,
                                               sizeof(out_text->value),
                                               &state->snapshot.altitude.units.alt_rel_home_noimu,
                                               units);
        break;

    case MOTOR_FIELD_GRADE_PERCENT:
        motor_field_format_signed_x10(out_text->value, sizeof(out_text->value), state->snapshot.altitude.grade_noimu_x10);
        break;

    case MOTOR_FIELD_HEADING:
        (void)snprintf(out_text->value, sizeof(out_text->value), "%ld", (long)(state->dyn.heading_deg_x10 / 10));
        break;

    case MOTOR_FIELD_GPS_FIX:
        (void)snprintf(out_text->value, sizeof(out_text->value), "%uD", (unsigned)state->nav.fix_type);
        break;

    case MOTOR_FIELD_GPS_SATS:
        (void)snprintf(out_text->value, sizeof(out_text->value), "%u", (unsigned)state->nav.sats_used);
        break;

    case MOTOR_FIELD_GPS_HACC:
        (void)snprintf(out_text->value, sizeof(out_text->value), "%lum", (unsigned long)(state->nav.hacc_mm / 1000u));
        break;

    case MOTOR_FIELD_GPS_VACC:
        (void)snprintf(out_text->value, sizeof(out_text->value), "%lum", (unsigned long)(state->nav.vacc_mm / 1000u));
        break;

    case MOTOR_FIELD_LATITUDE:
        motor_field_format_coord(out_text->value, sizeof(out_text->value), state->nav.lat_e7);
        break;

    case MOTOR_FIELD_LONGITUDE:
        motor_field_format_coord(out_text->value, sizeof(out_text->value), state->nav.lon_e7);
        break;

    case MOTOR_FIELD_TIME:
        if (state->snapshot.clock.local.year >= 2000u)
        {
            (void)snprintf(out_text->value,
                           sizeof(out_text->value),
                           "%02u:%02u:%02u",
                           (unsigned)state->snapshot.clock.local.hour,
                           (unsigned)state->snapshot.clock.local.min,
                           (unsigned)state->snapshot.clock.local.sec);
        }
        else
        {
            (void)snprintf(out_text->value, sizeof(out_text->value), "--:--:--");
        }
        break;

    case MOTOR_FIELD_RIDE_TIME:
        motor_field_format_seconds(out_text->value, sizeof(out_text->value), state->session.ride_seconds);
        break;

    case MOTOR_FIELD_MOVING_TIME:
        motor_field_format_seconds(out_text->value, sizeof(out_text->value), state->session.moving_seconds);
        break;

    case MOTOR_FIELD_DISTANCE:
        Motor_Units_FormatDistance(out_text->value, sizeof(out_text->value), (int32_t)state->session.distance_m, units);
        break;

    case MOTOR_FIELD_TRIP_A:
        Motor_Units_FormatDistance(out_text->value, sizeof(out_text->value), (int32_t)state->session.trip_a_m, units);
        break;

    case MOTOR_FIELD_TRIP_B:
        Motor_Units_FormatDistance(out_text->value, sizeof(out_text->value), (int32_t)state->session.trip_b_m, units);
        break;

    case MOTOR_FIELD_COOLANT_TEMP:
        if (state->vehicle.coolant_valid)
        {
            Motor_Units_FormatTemperature(out_text->value, sizeof(out_text->value), state->vehicle.coolant_temp_c_x10, units);
        }
        else
        {
            (void)snprintf(out_text->value, sizeof(out_text->value), "--");
        }
        break;

    case MOTOR_FIELD_RPM:
        if (state->vehicle.rpm_valid)
        {
            (void)snprintf(out_text->value, sizeof(out_text->value), "%u", (unsigned)state->vehicle.rpm);
        }
        else
        {
            (void)snprintf(out_text->value, sizeof(out_text->value), "--");
        }
        break;

    case MOTOR_FIELD_GEAR:
        if (state->vehicle.gear_valid)
        {
            (void)snprintf(out_text->value, sizeof(out_text->value), "%d", (int)state->vehicle.gear);
        }
        else
        {
            (void)snprintf(out_text->value, sizeof(out_text->value), "--");
        }
        break;

    case MOTOR_FIELD_BATTERY:
        if (state->vehicle.battery_valid)
        {
            (void)snprintf(out_text->value, sizeof(out_text->value), "%u.%01uV",
                           (unsigned)(state->vehicle.battery_mv / 1000u),
                           (unsigned)((state->vehicle.battery_mv % 1000u) / 100u));
        }
        else
        {
            (void)snprintf(out_text->value, sizeof(out_text->value), "--");
        }
        break;

    case MOTOR_FIELD_OBD_LINK:
        switch ((motor_obd_link_state_t)state->vehicle.link_state)
        {
        case MOTOR_OBD_LINK_CONNECTED:
            (void)snprintf(out_text->value, sizeof(out_text->value), "LINK");
            break;
        case MOTOR_OBD_LINK_CONNECTING:
            (void)snprintf(out_text->value, sizeof(out_text->value), "CONN");
            break;
        case MOTOR_OBD_LINK_SEARCHING:
            (void)snprintf(out_text->value, sizeof(out_text->value), "SCAN");
            break;
        case MOTOR_OBD_LINK_ERROR:
            (void)snprintf(out_text->value, sizeof(out_text->value), "ERR");
            break;
        case MOTOR_OBD_LINK_DISCONNECTED:
        default:
            (void)snprintf(out_text->value, sizeof(out_text->value), "OFF");
            break;
        }
        break;

    case MOTOR_FIELD_MAINT_DUE_COUNT:
        (void)snprintf(out_text->value, sizeof(out_text->value), "%u", (unsigned)state->maintenance.due_count);
        break;

    case MOTOR_FIELD_NEXT_SERVICE:
        if (state->maintenance.next_due_index < MOTOR_SERVICE_ITEM_COUNT)
        {
            (void)snprintf(out_text->value,
                           sizeof(out_text->value),
                           "%s",
                           state->settings.maintenance.items[state->maintenance.next_due_index].label);
        }
        else
        {
            (void)snprintf(out_text->value, sizeof(out_text->value), "NONE");
        }
        break;

    case MOTOR_FIELD_RECORD_STATE:
        switch ((motor_record_state_t)state->record.state)
        {
        case MOTOR_RECORD_STATE_RECORDING:
            (void)snprintf(out_text->value, sizeof(out_text->value), "REC");
            break;
        case MOTOR_RECORD_STATE_PAUSED:
            (void)snprintf(out_text->value, sizeof(out_text->value), "PAUSE");
            break;
        case MOTOR_RECORD_STATE_CLOSING:
            (void)snprintf(out_text->value, sizeof(out_text->value), "SAVE");
            break;
        case MOTOR_RECORD_STATE_ERROR:
            (void)snprintf(out_text->value, sizeof(out_text->value), "ERR");
            break;
        case MOTOR_RECORD_STATE_ARMED:
            (void)snprintf(out_text->value, sizeof(out_text->value), "ARM");
            break;
        case MOTOR_RECORD_STATE_IDLE:
        default:
            (void)snprintf(out_text->value, sizeof(out_text->value), "IDLE");
            break;
        }
        break;

    case MOTOR_FIELD_SD_STATE:
        (void)snprintf(out_text->value,
                       sizeof(out_text->value),
                       "%s",
                       (state->snapshot.sd.mounted != false) ? "MNT" : "NO SD");
        break;

    case MOTOR_FIELD_BT_STATE:
        (void)snprintf(out_text->value,
                       sizeof(out_text->value),
                       "%s",
                       (state->snapshot.bluetooth.initialized != false) ? "ON" : "OFF");
        break;

    case MOTOR_FIELD_ENGINE_HOURS:
        motor_field_format_seconds(out_text->value, sizeof(out_text->value), state->maintenance.engine_on_seconds_total);
        break;

    case MOTOR_FIELD_OUTSIDE_TEMP:
        if ((state->snapshot.ds18b20.status_flags & APP_DS18B20_STATUS_VALID) != 0u)
        {
            Motor_Units_FormatTemperature(out_text->value,
                                          sizeof(out_text->value),
                                          state->snapshot.ds18b20.raw.temp_c_x100 / 10,
                                          units);
        }
        else
        {
            (void)snprintf(out_text->value, sizeof(out_text->value), "--");
        }
        break;

    case MOTOR_FIELD_NONE:
    default:
        (void)snprintf(out_text->value, sizeof(out_text->value), "-");
        break;
    }
}
