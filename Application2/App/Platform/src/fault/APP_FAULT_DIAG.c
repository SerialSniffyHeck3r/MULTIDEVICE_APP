#include "APP_FAULT_DIAG.h"

/* -------------------------------------------------------------------------- */
/*  공개 API: bring-up용 fault 진단 모드 활성화                                */
/*                                                                            */
/*  이 함수는 부팅 직후 한 번만 호출하면 된다.                                 */
/*                                                                            */
/*  1) SHCSR의 configurable fault enable 비트를 켠다.                          */
/*     - BUSFAULTENA                                                          */
/*     - MEMFAULTENA                                                          */
/*     - USGFAULTENA                                                          */
/*                                                                            */
/*     이렇게 해야 BusFault/MemManage/UsageFault가                             */
/*     HardFault(FORCED)로 바로 승격되지 않고                                  */
/*     각자 전용 handler로 먼저 잡힌다.                                        */
/*                                                                            */
/*  2) 옵션으로 ACTLR.DISDEFWBUF를 켠다.                                       */
/*     - Cortex-M4의 default write buffer를 꺼서                               */
/*       일부 asynchronous bus fault를                                         */
/*       더 "정확한 시점"으로 드러내게 도와준다.                               */
/*                                                                            */
/*  주의                                                                      */
/*  - 이 설정은 근본 원인 해결이 아니라 진단력 향상용이다.                     */
/*  - SD hotplug / 외부 버스 teardown 경쟁 같은 실제 버그는                    */
/*    별도의 구조 개선으로 같이 잡아야 한다.                                   */
/* -------------------------------------------------------------------------- */
void APP_FAULT_DIAG_EnableBringupMode(void)
{
    /* ---------------------------------------------------------------------- */
    /*  sticky fault status가 남아 있었다면                                     */
    /*  bring-up 초기에 한 번 정리한다.                                         */
    /*                                                                        */
    /*  ARM fault status register는 write-1-to-clear 성격이 있으므로           */
    /*  현재 값 그대로 다시 써서 기존 찌꺼기만 걷어낸다.                         */
    /* ---------------------------------------------------------------------- */
    SCB->CFSR = SCB->CFSR;
    SCB->HFSR = SCB->HFSR;
    SCB->DFSR = SCB->DFSR;

    /* ---------------------------------------------------------------------- */
    /*  configurable fault handler enable                                      */
    /*                                                                        */
    /*  SHCSR가 0이면 현재처럼 configurable fault가 HardFault로                 */
    /*  바로 escalation 될 수 있다.                                             */
    /* ---------------------------------------------------------------------- */
    SCB->SHCSR |= (SCB_SHCSR_MEMFAULTENA_Msk |
                   SCB_SHCSR_BUSFAULTENA_Msk |
                   SCB_SHCSR_USGFAULTENA_Msk);

#if (APP_FAULT_DIAG_FORCE_PRECISE_BUSFAULT != 0u)
  #if defined(SCnSCB_ACTLR_DISDEFWBUF_Msk)
    /* ---------------------------------------------------------------------- */
    /*  write buffer를 끄면                                                    */
    /*  일부 IMPRECISERR가 더 일찍/정확히 드러날 수 있다.                      */
    /*                                                                        */
    /*  단, 성능/타이밍 특성은 달라질 수 있으므로                               */
    /*  release 고정 전에 반드시 다시 평가해야 한다.                            */
    /* ---------------------------------------------------------------------- */
    SCnSCB->ACTLR |= SCnSCB_ACTLR_DISDEFWBUF_Msk;
    __DSB();
    __ISB();
  #endif
#endif
}
