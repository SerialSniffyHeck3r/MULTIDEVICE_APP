
#ifndef MOTOR_NAVIGATION_H
#define MOTOR_NAVIGATION_H

#include "Motor_Model.h"

#ifdef __cplusplus
extern "C" {
#endif

void Motor_Navigation_Init(void);
void Motor_Navigation_Task(uint32_t now_ms);
void Motor_Navigation_ResetTrail(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_NAVIGATION_H */
