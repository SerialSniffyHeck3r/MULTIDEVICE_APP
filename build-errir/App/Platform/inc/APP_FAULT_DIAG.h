#ifndef APP_FAULT_DIAG_H
#define APP_FAULT_DIAG_H

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  APP_FAULT_DIAG                                                             */
/*                                                                            */
/*  목적                                                                      */
/*  - bring-up 단계에서 fault 추적력을 높이기 위한 진단 설정을 켠다.            */
/*  - MemManage / BusFault / UsageFault handler enable 비트를 켠다.            */
/*  - 옵션으로 Cortex-M write buffer를 꺼서                                    */
/*    일부 IMPRECISERR를 PRECISERR 쪽으로 더 잘 드러나게 만든다.               */
/*                                                                            */
/*  중요한 점                                                                 */
/*  - DISDEFWBUF는 디버그용이다.                                               */
/*  - 성능이 약간 떨어질 수 있으므로 release 안정화가 끝나면                   */
/*    0으로 되돌리는 것을 권장한다.                                            */
/* -------------------------------------------------------------------------- */

#ifndef APP_FAULT_DIAG_FORCE_PRECISE_BUSFAULT
#define APP_FAULT_DIAG_FORCE_PRECISE_BUSFAULT 1u
#endif

void APP_FAULT_DIAG_EnableBringupMode(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_FAULT_DIAG_H */
