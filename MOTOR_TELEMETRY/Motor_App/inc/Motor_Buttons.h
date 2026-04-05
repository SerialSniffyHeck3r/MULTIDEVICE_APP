
#ifndef MOTOR_BUTTONS_H
#define MOTOR_BUTTONS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void Motor_Buttons_Init(void);
void Motor_Buttons_Task(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_BUTTONS_H */
