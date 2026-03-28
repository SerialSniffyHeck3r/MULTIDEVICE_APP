
#ifndef MOTOR_MAINTENANCE_H
#define MOTOR_MAINTENANCE_H

#include "Motor_Model.h"

#ifdef __cplusplus
extern "C" {
#endif

void Motor_Maintenance_Init(void);
void Motor_Maintenance_Task(uint32_t now_ms);
void Motor_Maintenance_ResetService(uint8_t service_index);
const char *Motor_Maintenance_GetServiceLabel(uint8_t service_index);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_MAINTENANCE_H */
