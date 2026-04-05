
#ifndef MOTOR_PANEL_H
#define MOTOR_PANEL_H

#include "Motor_Model.h"

#ifdef __cplusplus
extern "C" {
#endif

void Motor_Panel_Init(void);
void Motor_Panel_ApplyDisplaySettings(const motor_settings_t *settings);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_PANEL_H */
