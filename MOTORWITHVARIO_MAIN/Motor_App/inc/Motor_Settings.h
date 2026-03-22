
#ifndef MOTOR_SETTINGS_H
#define MOTOR_SETTINGS_H

#include "Motor_Model.h"

#ifdef __cplusplus
extern "C" {
#endif

void Motor_Settings_Init(void);
void Motor_Settings_Copy(motor_settings_t *dst);
const motor_settings_t *Motor_Settings_Get(void);
motor_settings_t *Motor_Settings_GetMutable(void);
void Motor_Settings_Commit(void);
void Motor_Settings_ResetToDefaults(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_SETTINGS_H */
