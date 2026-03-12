#ifndef APP_SD_H
#define APP_SD_H

#include "main.h"
#include "APP_STATE.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  APP_SD                                                                     */
/*                                                                            */
/*  목적                                                                      */
/*  - SD_DETECT 핀의 raw/stable 상태를 runtime 에서 직접 관리한다.              */
/*  - 카드 삽입/제거 hotplug 를 debounce 후 안정적으로 판정한다.               */
/*  - 카드가 들어오면 HAL SD + FatFs mount 를 시도하고,                        */
/*    카드/FAT 메타데이터를 APP_STATE.sd에 정리해서 공개한다.                  */
/*  - 카드가 빠지면 unmount + HAL deinit 을 수행해서                           */
/*    다음 재삽입 시 깨끗한 상태로 다시 브링업한다.                            */
/*                                                                            */
/*  CubeMX 재생성 내성 포인트                                                  */
/*  - detect 핀 모드(EXTI both-edge + pull-up)는 APP_SD_Init()가               */
/*    runtime 에서 다시 덮어쓴다.                                              */
/*  - EXTI2_IRQHandler 는 stm32f4xx_it.c 의 USER CODE 블록 안에 넣어 둔다.    */
/*  - main.c 는 APP_SD_Init/Task/OnDetectExti 만 호출하면 된다.               */
/* -------------------------------------------------------------------------- */

void APP_SD_Init(void);
void APP_SD_Task(uint32_t now_ms);
void APP_SD_OnDetectExti(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_SD_H */
