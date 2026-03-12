#ifndef FW_APPGUARD_H
#define FW_APPGUARD_H

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  FW_AppGuard                                                                */
/*                                                                            */
/*  Main App 쪽에서 해야 하는 일                                               */
/*  - 부팅 시작을 BKPSRAM에 표시                                               */
/*  - main loop 진입 직전에 "정상 부팅 확인" 을 BKPSRAM에 표시                */
/*  - main loop에서 IWDG refresh                                               */
/*                                                                            */
/*  주의                                                                      */
/*  - hiwdg / MX_IWDG_Init() 는 CubeMX .ioc 를 통해 추가되어 있어야 한다.     */
/* -------------------------------------------------------------------------- */

void FW_AppGuard_OnAppBootStart(void);
void FW_AppGuard_ConfirmBootOk(void);
void FW_AppGuard_Kick(void);

#ifdef __cplusplus
}
#endif

#endif /* FW_APPGUARD_H */
