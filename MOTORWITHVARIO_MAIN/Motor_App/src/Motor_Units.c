
#include "Motor_Units.h"

#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*  preset 적용                                                                */
/*                                                                            */
/*  English  : mph / mile / ft / °F / psi / mpg US                            */
/*  Imperial : mph / mile / ft / °F / psi / mpg UK                            */
/*  Metric   : km/h / km / m / °C / hPa / L/100km                             */
/* -------------------------------------------------------------------------- */
void Motor_Units_ApplyPreset(motor_unit_settings_t *units, motor_unit_preset_t preset)
{
    if (units == 0)
    {
        return;
    }

    memset(units, 0, sizeof(*units));
    units->preset = (uint8_t)preset;

    switch (preset)
    {
    case MOTOR_UNIT_PRESET_ENGLISH:
        units->speed       = (uint8_t)MOTOR_SPEED_UNIT_MPH;
        units->distance    = (uint8_t)MOTOR_DISTANCE_UNIT_MI;
        units->altitude    = (uint8_t)MOTOR_ALTITUDE_UNIT_FT;
        units->temperature = (uint8_t)MOTOR_TEMP_UNIT_F;
        units->pressure    = (uint8_t)MOTOR_PRESSURE_UNIT_PSI;
        units->economy     = (uint8_t)MOTOR_ECON_UNIT_MPG_US;
        break;

    case MOTOR_UNIT_PRESET_IMPERIAL:
        units->speed       = (uint8_t)MOTOR_SPEED_UNIT_MPH;
        units->distance    = (uint8_t)MOTOR_DISTANCE_UNIT_MI;
        units->altitude    = (uint8_t)MOTOR_ALTITUDE_UNIT_FT;
        units->temperature = (uint8_t)MOTOR_TEMP_UNIT_F;
        units->pressure    = (uint8_t)MOTOR_PRESSURE_UNIT_PSI;
        units->economy     = (uint8_t)MOTOR_ECON_UNIT_MPG_UK;
        break;

    case MOTOR_UNIT_PRESET_METRIC:
    case MOTOR_UNIT_PRESET_CUSTOM:
    default:
        units->speed       = (uint8_t)MOTOR_SPEED_UNIT_KMH;
        units->distance    = (uint8_t)MOTOR_DISTANCE_UNIT_KM;
        units->altitude    = (uint8_t)MOTOR_ALTITUDE_UNIT_M;
        units->temperature = (uint8_t)MOTOR_TEMP_UNIT_C;
        units->pressure    = (uint8_t)MOTOR_PRESSURE_UNIT_HPA;
        units->economy     = (uint8_t)MOTOR_ECON_UNIT_L_PER_100KM;
        break;
    }
}

void Motor_Units_NormalizePreset(motor_unit_settings_t *units)
{
    motor_unit_settings_t preset_metric;
    motor_unit_settings_t preset_english;
    motor_unit_settings_t preset_imperial;

    if (units == 0)
    {
        return;
    }

    Motor_Units_ApplyPreset(&preset_metric, MOTOR_UNIT_PRESET_METRIC);
    Motor_Units_ApplyPreset(&preset_english, MOTOR_UNIT_PRESET_ENGLISH);
    Motor_Units_ApplyPreset(&preset_imperial, MOTOR_UNIT_PRESET_IMPERIAL);

    if (memcmp(&preset_metric.speed, &units->speed, sizeof(motor_unit_settings_t) - 1u) == 0)
    {
        units->preset = (uint8_t)MOTOR_UNIT_PRESET_METRIC;
        return;
    }
    if (memcmp(&preset_english.speed, &units->speed, sizeof(motor_unit_settings_t) - 1u) == 0)
    {
        units->preset = (uint8_t)MOTOR_UNIT_PRESET_ENGLISH;
        return;
    }
    if (memcmp(&preset_imperial.speed, &units->speed, sizeof(motor_unit_settings_t) - 1u) == 0)
    {
        units->preset = (uint8_t)MOTOR_UNIT_PRESET_IMPERIAL;
        return;
    }

    units->preset = (uint8_t)MOTOR_UNIT_PRESET_CUSTOM;
}

int32_t Motor_Units_ConvertSpeedX10(int32_t speed_kmh_x10, const motor_unit_settings_t *units)
{
    if ((units != 0) && (units->speed == (uint8_t)MOTOR_SPEED_UNIT_MPH))
    {
        return (speed_kmh_x10 * 6214) / 10000;
    }
    return speed_kmh_x10;
}

int32_t Motor_Units_ConvertDistanceM(int32_t distance_m, const motor_unit_settings_t *units)
{
    if ((units != 0) && (units->distance == (uint8_t)MOTOR_DISTANCE_UNIT_MI))
    {
        return (distance_m * 6214) / 10000;
    }
    return distance_m / 1000;
}

int32_t Motor_Units_ConvertAltitudeCm(int32_t altitude_cm, const motor_unit_settings_t *units)
{
    if ((units != 0) && (units->altitude == (uint8_t)MOTOR_ALTITUDE_UNIT_FT))
    {
        return (altitude_cm * 3281) / 10000;
    }
    return altitude_cm / 100;
}

int32_t Motor_Units_SelectAltitudeFromUnitBank(const app_altitude_linear_units_t *unit_bank,
                                               const motor_unit_settings_t *units)
{
    if (unit_bank == 0)
    {
        return 0;
    }

    /* ---------------------------------------------------------------------- */
    /*  altitude low-level bank select                                         */
    /*                                                                        */
    /*  source-of-truth 변환은 APP_ALTITUDE 서비스가 이미 끝낸 상태이므로       */
    /*  여기서는 preset/custom 선택에 맞는 슬롯만 고른다.                      */
    /*  즉, Motor upper layer는 meter->feet 재환산을 반복하지 않는다.          */
    /* ---------------------------------------------------------------------- */
    if ((units != 0) && (units->altitude == (uint8_t)MOTOR_ALTITUDE_UNIT_FT))
    {
        return unit_bank->feet_rounded;
    }

    return unit_bank->meters_rounded;
}

int32_t Motor_Units_ConvertTempCx10(int32_t temp_c_x10, const motor_unit_settings_t *units)
{
    if ((units != 0) && (units->temperature == (uint8_t)MOTOR_TEMP_UNIT_F))
    {
        return ((temp_c_x10 * 9) / 5) + 320;
    }
    return temp_c_x10;
}

void Motor_Units_FormatSpeed(char *out_text, size_t out_size, int32_t speed_kmh_x10, const motor_unit_settings_t *units)
{
    int32_t converted;

    if ((out_text == 0) || (out_size == 0u))
    {
        return;
    }

    converted = Motor_Units_ConvertSpeedX10(speed_kmh_x10, units);
    (void)snprintf(out_text, out_size, "%ld.%01ld",
                   (long)(converted / 10),
                   (long)((converted >= 0) ? (converted % 10) : (-(converted % 10))));
}

void Motor_Units_FormatDistance(char *out_text, size_t out_size, int32_t distance_m, const motor_unit_settings_t *units)
{
    int32_t converted;

    if ((out_text == 0) || (out_size == 0u))
    {
        return;
    }

    if ((units != 0) && (units->distance == (uint8_t)MOTOR_DISTANCE_UNIT_MI))
    {
        converted = (distance_m * 6214) / 10000;
    }
    else
    {
        converted = distance_m / 1000;
    }

    (void)snprintf(out_text, out_size, "%ld", (long)converted);
}

void Motor_Units_FormatAltitude(char *out_text, size_t out_size, int32_t altitude_cm, const motor_unit_settings_t *units)
{
    int32_t converted;

    if ((out_text == 0) || (out_size == 0u))
    {
        return;
    }

    converted = Motor_Units_ConvertAltitudeCm(altitude_cm, units);
    (void)snprintf(out_text, out_size, "%ld", (long)converted);
}

void Motor_Units_FormatAltitudeFromUnitBank(char *out_text,
                                            size_t out_size,
                                            const app_altitude_linear_units_t *unit_bank,
                                            const motor_unit_settings_t *units)
{
    int32_t converted;

    if ((out_text == 0) || (out_size == 0u))
    {
        return;
    }

    converted = Motor_Units_SelectAltitudeFromUnitBank(unit_bank, units);
    (void)snprintf(out_text, out_size, "%ld", (long)converted);
}

void Motor_Units_FormatTemperature(char *out_text, size_t out_size, int32_t temp_c_x10, const motor_unit_settings_t *units)
{
    int32_t converted;

    if ((out_text == 0) || (out_size == 0u))
    {
        return;
    }

    converted = Motor_Units_ConvertTempCx10(temp_c_x10, units);
    (void)snprintf(out_text, out_size, "%ld.%01ld",
                   (long)(converted / 10),
                   (long)((converted >= 0) ? (converted % 10) : (-(converted % 10))));
}

const char *Motor_Units_GetSpeedSuffix(const motor_unit_settings_t *units)
{
    if ((units != 0) && (units->speed == (uint8_t)MOTOR_SPEED_UNIT_MPH))
    {
        return "mph";
    }
    return "km/h";
}

const char *Motor_Units_GetDistanceSuffix(const motor_unit_settings_t *units)
{
    if ((units != 0) && (units->distance == (uint8_t)MOTOR_DISTANCE_UNIT_MI))
    {
        return "mi";
    }
    return "km";
}

const char *Motor_Units_GetAltitudeSuffix(const motor_unit_settings_t *units)
{
    if ((units != 0) && (units->altitude == (uint8_t)MOTOR_ALTITUDE_UNIT_FT))
    {
        return "ft";
    }
    return "m";
}

const char *Motor_Units_GetTempSuffix(const motor_unit_settings_t *units)
{
    if ((units != 0) && (units->temperature == (uint8_t)MOTOR_TEMP_UNIT_F))
    {
        return "F";
    }
    return "C";
}
