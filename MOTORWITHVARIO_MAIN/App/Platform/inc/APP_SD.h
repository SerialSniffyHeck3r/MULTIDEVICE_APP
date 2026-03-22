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

/* -------------------------------------------------------------------------- */
/*  공개 API: "지금 이 순간" FatFs 접근이 안전한가                              */
/*                                                                            */
/*  이 함수는 APP_STATE.sd의 공개 스냅샷만 보지 않고,                           */
/*  APP_SD 내부 runtime mailbox + raw DET 핀 상태까지 함께 확인한다.           */
/*                                                                            */
/*  따라서 Audio_Driver처럼 "APP_SD_Task()보다 먼저 도는 task"도              */
/*  SD remove edge 직후를 더 빨리 감지해서                                     */
/*  새 f_open/f_read/f_opendir 호출을 보수적으로 막을 수 있다.                 */
/*                                                                            */
/*  반환 규칙                                                                  */
/*  - true  : raw detect = present, debounce 진행 중 아님,                     */
/*            stable present = true, initialized = true, mounted = true        */
/*  - false : 위 조건 중 하나라도 깨진 상태                                    */
/*                                                                            */
/*  즉, 이 함수는 "조금이라도 애매하면 막는다" 쪽으로 설계된                   */
/*  bring-up 안전 게이트다.                                                    */
/* -------------------------------------------------------------------------- */
bool APP_SD_IsFsAccessAllowedNow(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_SD_H */
