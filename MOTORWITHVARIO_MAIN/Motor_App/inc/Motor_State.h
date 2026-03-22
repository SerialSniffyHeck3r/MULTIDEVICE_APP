
#ifndef MOTOR_STATE_H
#define MOTOR_STATE_H

#include "Motor_Model.h"

#ifdef __cplusplus
extern "C" {
#endif

void Motor_State_Init(void);
void Motor_State_Task(uint32_t now_ms);
const motor_state_t *Motor_State_Get(void);
motor_state_t *Motor_State_GetMutable(void);
void Motor_State_RequestRedraw(void);
void Motor_State_ShowToast(const char *text, uint32_t hold_ms);
void Motor_State_SetScreen(motor_screen_t screen);
void Motor_State_StorePreviousDriveScreen(motor_screen_t screen);
void Motor_State_RequestMarker(void);
void Motor_State_RequestRecordToggle(void);
void Motor_State_RequestRecordStop(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_STATE_H */
