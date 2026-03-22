
#ifndef MOTOR_VEHICLE_H
#define MOTOR_VEHICLE_H

#include "Motor_Model.h"

#ifdef __cplusplus
extern "C" {
#endif

void Motor_Vehicle_Init(void);
void Motor_Vehicle_Task(uint32_t now_ms);
void Motor_Vehicle_RequestConnect(void);
void Motor_Vehicle_RequestDisconnect(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_VEHICLE_H */
