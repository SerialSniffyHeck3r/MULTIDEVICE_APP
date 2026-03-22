
#ifndef MOTOR_DYNAMICS_H
#define MOTOR_DYNAMICS_H

#include "Motor_Model.h"

#ifdef __cplusplus
extern "C" {
#endif

void Motor_Dynamics_Init(void);
void Motor_Dynamics_Task(uint32_t now_ms);
void Motor_Dynamics_RequestZeroCapture(void);
void Motor_Dynamics_ResetSessionPeaks(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_DYNAMICS_H */
