
#ifndef MOTOR_TASK_H
#define MOTOR_TASK_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  Motor_App 상위 슈퍼루프 진입점                                             */
/*                                                                            */
/*  main.c 는 CubeMX 재생성 안전을 위해                                        */
/*    - Motor_App_Init()          1회 호출                                     */
/*    - Motor_App_EarlyBootDraw() LCD init 직후 1회 호출                       */
/*    - Motor_App_Task()          while(1)에서 반복 호출                       */
/*    - Motor_App_OnBoardDebugButtonIrq() 보드 디버그 버튼 IRQ 시 호출         */
/*  만 수행한다.                                                               */
/* -------------------------------------------------------------------------- */
void Motor_App_Init(void);
void Motor_App_EarlyBootDraw(void);
void Motor_App_Task(void);
void Motor_App_OnBoardDebugButtonIrq(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_TASK_H */
