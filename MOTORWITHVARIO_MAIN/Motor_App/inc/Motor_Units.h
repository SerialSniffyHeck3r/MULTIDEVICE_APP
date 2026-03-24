
#ifndef MOTOR_UNITS_H
#define MOTOR_UNITS_H

#include "Motor_Model.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void Motor_Units_ApplyPreset(motor_unit_settings_t *units, motor_unit_preset_t preset);
void Motor_Units_NormalizePreset(motor_unit_settings_t *units);
int32_t Motor_Units_ConvertSpeedX10(int32_t speed_kmh_x10, const motor_unit_settings_t *units);
int32_t Motor_Units_ConvertDistanceM(int32_t distance_m, const motor_unit_settings_t *units);
int32_t Motor_Units_ConvertAltitudeCm(int32_t altitude_cm, const motor_unit_settings_t *units);
int32_t Motor_Units_SelectAltitudeFromUnitBank(const app_altitude_linear_units_t *unit_bank, const motor_unit_settings_t *units);
int32_t Motor_Units_ConvertTempCx10(int32_t temp_c_x10, const motor_unit_settings_t *units);
void Motor_Units_FormatSpeed(char *out_text, size_t out_size, int32_t speed_kmh_x10, const motor_unit_settings_t *units);
void Motor_Units_FormatDistance(char *out_text, size_t out_size, int32_t distance_m, const motor_unit_settings_t *units);
void Motor_Units_FormatAltitude(char *out_text, size_t out_size, int32_t altitude_cm, const motor_unit_settings_t *units);
void Motor_Units_FormatAltitudeFromUnitBank(char *out_text, size_t out_size, const app_altitude_linear_units_t *unit_bank, const motor_unit_settings_t *units);
void Motor_Units_FormatTemperature(char *out_text, size_t out_size, int32_t temp_c_x10, const motor_unit_settings_t *units);
const char *Motor_Units_GetSpeedSuffix(const motor_unit_settings_t *units);
const char *Motor_Units_GetDistanceSuffix(const motor_unit_settings_t *units);
const char *Motor_Units_GetAltitudeSuffix(const motor_unit_settings_t *units);
const char *Motor_Units_GetTempSuffix(const motor_unit_settings_t *units);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_UNITS_H */
