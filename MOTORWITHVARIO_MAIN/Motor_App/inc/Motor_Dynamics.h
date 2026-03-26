#ifndef MOTOR_DYNAMICS_H
#define MOTOR_DYNAMICS_H

#include "Motor_Model.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  Motor dynamics facade                                                      */
/*                                                                            */
/*  중요한 계층 규칙                                                           */
/*  - 실제 lean / grade / G-force 추정의 canonical owner는                    */
/*    shared low-level BIKE_DYNAMICS service 이다.                            */
/*  - Motor_App의 Dynamics 모듈은 estimator를 다시 구현하지 않고,             */
/*    shared snapshot을 high-level model로 정리하고 session peak/history를     */
/*    유지하는 adapter 역할만 맡는다.                                         */
/* -------------------------------------------------------------------------- */
void Motor_Dynamics_Init(void);
void Motor_Dynamics_Task(uint32_t now_ms);

/* shared BIKE_DYNAMICS 명령 wrapper ---------------------------------------- */
void Motor_Dynamics_RequestZeroCapture(void);
void Motor_Dynamics_RequestHardRezero(void);
void Motor_Dynamics_RequestGyroBiasCalibration(void);

/* session-local runtime helper --------------------------------------------- */
void Motor_Dynamics_ResetSessionPeaks(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_DYNAMICS_H */
