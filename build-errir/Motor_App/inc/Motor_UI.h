
#ifndef MOTOR_UI_H
#define MOTOR_UI_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void Motor_UI_Init(void);
void Motor_UI_EarlyBootDraw(void);
void Motor_UI_Task(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_UI_H */
