#ifndef VARIO_TASK_H
#define VARIO_TASK_H

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  바리오 앱 슈퍼루프 진입점                                                  */
/*                                                                            */
/*  main.c 는 CubeMX 재생성 안전을 위해                                        */
/*    - Vario_App_Init()  1회 호출                                             */
/*    - Vario_App_Task()  무한 루프에서 반복 호출                              */
/*  두 API만 사용한다.                                                          */
/* -------------------------------------------------------------------------- */
void Vario_App_Init(void);
void Vario_App_Task(void);

#ifdef __cplusplus
}
#endif

#endif /* VARIO_TASK_H */
