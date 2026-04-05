#ifndef BOOT_SELFTEST_SCREEN_H
#define BOOT_SELFTEST_SCREEN_H

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* BOOT_SELFTEST_SCREEN                                                        */
/*                                                                            */
/* 역할                                                                       */
/* - 부팅 직후 blocking 방식으로 self-test 화면을 보여 준다.                  */
/* - 화면 위에는 boot logo를 기존 early boot draw보다 위쪽에 다시 배치한다.  */
/* - 화면 아래에는 GPS / IMU / SENSORS / HARDWARE 4줄의 상태를               */
/*   TESTING / OK / FAIL 형태로 그린다.                                       */
/* - 실행 중에는 BUTTON1~BUTTON6 event queue를 계속 비워서                    */
/*   전원 ON 확인 화면에서 남아 있는 F6 release 등 잔상 입력이                */
/*   일반 런타임 UI로 넘어가지 않게 막는다.                                   */
/*                                                                            */
/* 사용 시점                                                                    */
/* - Application2/Core/Src/main.c                                             */
/* - GPS / Bluetooth / GY86 / DS18 / Brightness / SPI Flash / SD init 뒤      */
/* - TIM7 frame token 시작 전                                                  */
/* -------------------------------------------------------------------------- */
void BOOT_SELFTEST_SCREEN_RunBlocking(void);

#ifdef __cplusplus
}
#endif

#endif /* BOOT_SELFTEST_SCREEN_H */
