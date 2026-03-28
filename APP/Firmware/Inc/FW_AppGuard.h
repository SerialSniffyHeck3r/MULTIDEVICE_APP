#ifndef FW_APP_GUARD_H
#define FW_APP_GUARD_H

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  FW_AppGuard                                                                */
/*                                                                            */
/*  목적                                                                       */
/*  - main app 가 정상적으로 부팅을 시작했다는 사실을 bootloader 와 공유한다. */
/*  - while(1) 진입 직전에 "boot confirmed" 를 세워,                        */
/*    이후 IWDG reset 과 "초기 부팅 실패" 를 구분할 수 있게 한다.            */
/*  - main loop에서는 hiwdg refresh helper 역할도 함께 맡는다.                */
/* -------------------------------------------------------------------------- */

void FW_AppGuard_OnAppBootStart(void);
void FW_AppGuard_ConfirmBootOk(void);
void FW_AppGuard_Kick(void);

#ifdef __cplusplus
}
#endif

#endif /* FW_APP_GUARD_H */
