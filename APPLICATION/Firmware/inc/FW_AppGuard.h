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
/*  목적                                                                      */
/*  - main app 가 '정상적으로 while(1)에 진입했다'는 사실을 resident            */
/*    bootloader 에 알려주는 아주 얇은 훅이다.                                  */
/*  - IWDG 리셋이 발생했을 때, bootloader 가 '정상 부팅 확정 전에 죽은 것인지'를  */
/*    판정할 수 있게 BKPSRAM 플래그를 관리한다.                                */
/*                                                                            */
/*  호출 규칙                                                                 */
/*  - FW_AppGuard_OnAppBootStart()  : 부팅 초반 1회                           */
/*  - FW_AppGuard_ConfirmBootOk()   : main loop 진입 직전 1회                 */
/*  - FW_AppGuard_Kick()            : main loop 주기 호출                     */
/* -------------------------------------------------------------------------- */

void FW_AppGuard_OnAppBootStart(void);
void FW_AppGuard_ConfirmBootOk(void);
void FW_AppGuard_Kick(void);

#ifdef __cplusplus
}
#endif

#endif /* FW_APP_GUARD_H */
