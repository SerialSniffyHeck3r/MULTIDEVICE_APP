#ifndef APP_FAULT_H
#define APP_FAULT_H

/* -------------------------------------------------------------------------- */
/*  APP_FAULT                                                                 */
/*                                                                            */
/*  목적                                                                      */
/*  - Cortex-M fault 레지스터와 예외 스택 프레임을 '직전 부팅의 사고 기록'처럼     */
/*    남긴다.                                                                  */
/*  - 다음 부팅 때 이 기록을 읽어서 U8G2 화면에 약 10초 동안 친절하게 보여준다.   */
/*  - 표시가 끝나면 저장된 기록을 지워서, 같은 로그가 매 부팅마다 반복되지 않게   */
/*    만든다.                                                                  */
/*                                                                            */
/*  설계 포인트                                                               */
/*  - APP_STATE는 부팅 시 memset으로 초기화되므로, '다음 부팅까지 살아 있어야 하는' */
/*    fault 로그를 APP_STATE에 두지 않는다.                                    */
/*  - 대신 RTC backup register를 사용해서 리셋 이후에도 유지되게 만든다.        */
/*  - fault handler에서는 가능한 한 짧고 단순하게:                             */
/*      1) 레지스터 스냅샷 기록                                                */
/*      2) NVIC_SystemReset()                                                  */
/*    순서로 끝낸다.                                                           */
/* -------------------------------------------------------------------------- */

#include "main.h"
#include "u8g2.h"
#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  fault 종류 코드                                                            */
/* -------------------------------------------------------------------------- */

typedef enum
{
    APP_FAULT_TYPE_NONE          = 0u,
    APP_FAULT_TYPE_HARDFAULT     = 1u,
    APP_FAULT_TYPE_MEMMANAGE     = 2u,
    APP_FAULT_TYPE_BUSFAULT      = 3u,
    APP_FAULT_TYPE_USAGEFAULT    = 4u,
    APP_FAULT_TYPE_NMI           = 5u,
    APP_FAULT_TYPE_ERROR_HANDLER = 9u
} app_fault_type_t;

/* -------------------------------------------------------------------------- */
/*  persistent log 구조체                                                      */
/*                                                                            */
/*  이 구조체는 RTC backup register 20개 x 32bit = 총 80바이트에 정확히 맞춘다.  */
/*                                                                            */
/*  header     : magic + version + fault type                                  */
/*  cfsr~shcsr : Cortex-M system fault status 계열 레지스터                    */
/*  exc_return : LR에 들어 있던 EXC_RETURN 값                                 */
/*  msp/psp    : 예외 시점의 두 스택 포인터                                    */
/*  r0~xpsr    : 예외 진입 시 자동 스택된 기본 exception frame                 */
/*  control    : CONTROL 레지스터                                              */
/* -------------------------------------------------------------------------- */

typedef struct
{
    uint32_t header;      /* [31:16] magic, [15:8] version, [7:0] type */

    uint32_t cfsr;        /* Configurable Fault Status Register         */
    uint32_t hfsr;        /* HardFault Status Register                  */
    uint32_t dfsr;        /* Debug Fault Status Register                */
    uint32_t afsr;        /* Auxiliary Fault Status Register            */
    uint32_t mmfar;       /* MemManage Fault Address Register           */
    uint32_t bfar;        /* BusFault Address Register                  */
    uint32_t shcsr;       /* System Handler Control and State Register  */

    uint32_t exc_return;  /* LR의 EXC_RETURN 값                         */
    uint32_t msp;         /* Main Stack Pointer snapshot                */
    uint32_t psp;         /* Process Stack Pointer snapshot             */

    uint32_t r0;          /* stacked R0                                 */
    uint32_t r1;          /* stacked R1                                 */
    uint32_t r2;          /* stacked R2                                 */
    uint32_t r3;          /* stacked R3                                 */
    uint32_t r12;         /* stacked R12                                */
    uint32_t lr;          /* stacked LR                                 */
    uint32_t pc;          /* stacked PC                                 */
    uint32_t xpsr;        /* stacked xPSR                               */

    uint32_t control;     /* CONTROL register snapshot                  */
} app_fault_log_t;

/* -------------------------------------------------------------------------- */
/*  읽기/지우기/부팅 시 표시                                                   */
/* -------------------------------------------------------------------------- */

bool APP_FAULT_ReadPersistentLog(app_fault_log_t *out_log);
void APP_FAULT_ClearPersistentLog(void);
void APP_FAULT_BootCheckAndShow(u8g2_t *u8g2, uint32_t show_ms);

/* -------------------------------------------------------------------------- */
/*  소프트웨어 경로용 기록 API                                                 */
/* -------------------------------------------------------------------------- */

void APP_FAULT_RecordSoftware(app_fault_type_t type, uint32_t pc_hint);

/* -------------------------------------------------------------------------- */
/*  fault handler의 C 엔트리                                                   */
/*                                                                            */
/*  stm32f4xx_it.c의 naked wrapper에서                                         */
/*    - r0 = 예외 프레임 시작 주소(MSP 또는 PSP)                                */
/*    - r1 = LR(EXC_RETURN)                                                     */
/*  를 넘겨주면, 아래 함수가 실제 스냅샷 기록 + 시스템 리셋을 수행한다.          */
/* -------------------------------------------------------------------------- */

void APP_FAULT_NmiC(uint32_t *stack_frame, uint32_t exc_return);
void APP_FAULT_HardFaultC(uint32_t *stack_frame, uint32_t exc_return);
void APP_FAULT_MemManageC(uint32_t *stack_frame, uint32_t exc_return);
void APP_FAULT_BusFaultC(uint32_t *stack_frame, uint32_t exc_return);
void APP_FAULT_UsageFaultC(uint32_t *stack_frame, uint32_t exc_return);

#ifdef __cplusplus
}
#endif

#endif /* APP_FAULT_H */
