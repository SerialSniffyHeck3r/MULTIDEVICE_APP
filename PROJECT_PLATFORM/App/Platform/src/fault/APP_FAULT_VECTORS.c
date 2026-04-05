#include "APP_FAULT.h"

/* -------------------------------------------------------------------------- */
/*  APP_FAULT_VECTORS                                                          */
/*                                                                            */
/*  이 파일은 startup vector가 기대하는 진짜 fault handler 심볼                  */
/*    - NMI_Handler                                                            */
/*    - HardFault_Handler                                                      */
/*    - MemManage_Handler                                                      */
/*    - BusFault_Handler                                                       */
/*    - UsageFault_Handler                                                     */
/*  을 non-generated 파일에서 다시 정의한다.                                   */
/*                                                                            */
/*  왜 별도 파일이 필요한가                                                    */
/*  - Cube가 생성한 stm32f4xx_it.c의 기본 handler는 일반 C 함수다.              */
/*  - 일반 C 함수 안으로 이미 들어온 뒤에는                                    */
/*    LR(EXC_RETURN)와 원래 예외 스택 프레임을 그대로 보존하기 어렵다.          */
/*  - 그래서 fault snapshot의 EXRET / PC / LR / xPSR가                         */
/*    실제 사고 지점과 어긋날 수 있다.                                         */
/*                                                                            */
/*  이 파일의 handler는 모두 naked asm wrapper라서                              */
/*    1) LR(EXC_RETURN)의 bit[2]로 MSP/PSP를 정확히 고르고                     */
/*    2) r0 = 예외 스택 프레임, r1 = EXC_RETURN                                 */
/*    3) APP_FAULT_xxxC()로 즉시 branch                                         */
/*  하는 정석 경로를 쓴다.                                                     */
/*                                                                            */
/*  중요                                                                      */
/*  - 이 파일만 추가해서는 안 된다.                                             */
/*  - stm32f4xx_it.c USER CODE BEGIN 0 구간에서                                */
/*    동일 이름의 Cube handler 심볼을 다른 이름으로 alias 하는                   */
/*    매크로 블록을 함께 넣어 중복 정의를 피해야 한다.                           */
/* -------------------------------------------------------------------------- */

__attribute__((naked)) void NMI_Handler(void)
{
    __asm volatile
    (
        "tst lr, #4          \n"
        "ite eq              \n"
        "mrseq r0, msp       \n"
        "mrsne r0, psp       \n"
        "mov r1, lr          \n"
        "b APP_FAULT_NmiC    \n"
    );
}

__attribute__((naked)) void HardFault_Handler(void)
{
    __asm volatile
    (
        "tst lr, #4               \n"
        "ite eq                   \n"
        "mrseq r0, msp            \n"
        "mrsne r0, psp            \n"
        "mov r1, lr               \n"
        "b APP_FAULT_HardFaultC   \n"
    );
}

__attribute__((naked)) void MemManage_Handler(void)
{
    __asm volatile
    (
        "tst lr, #4                \n"
        "ite eq                    \n"
        "mrseq r0, msp             \n"
        "mrsne r0, psp             \n"
        "mov r1, lr                \n"
        "b APP_FAULT_MemManageC    \n"
    );
}

__attribute__((naked)) void BusFault_Handler(void)
{
    __asm volatile
    (
        "tst lr, #4               \n"
        "ite eq                   \n"
        "mrseq r0, msp            \n"
        "mrsne r0, psp            \n"
        "mov r1, lr               \n"
        "b APP_FAULT_BusFaultC    \n"
    );
}

__attribute__((naked)) void UsageFault_Handler(void)
{
    __asm volatile
    (
        "tst lr, #4                 \n"
        "ite eq                     \n"
        "mrseq r0, msp              \n"
        "mrsne r0, psp              \n"
        "mov r1, lr                 \n"
        "b APP_FAULT_UsageFaultC    \n"
    );
}
