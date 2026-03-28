#ifndef APP_SELFTEST_SELFTEST_H
#define APP_SELFTEST_SELFTEST_H

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* SELFTEST                                                                    */
/*                                                                            */
/* 목적                                                                        */
/* - 기존 Platform/SELFTEST(부트용 quick self-test)와 별개로,                  */
/*   유지보수/정비용 강한 테스트 모드를 별도 루프로 제공한다.                  */
/* - 이 모드는 UI 엔진을 통하지 않고, UC1608 + U8G2 직접 draw로              */
/*   독립 화면을 구성한다.                                                     */
/* - 진입 후에는 일반 앱 슈퍼루프로 복귀하지 않으며,                           */
/*   전원 차단 또는 리셋 전까지 maintenance loop를 유지한다.                   */
/*                                                                            */
/* 사용 위치                                                                    */
/* - main.c 에서 부팅 시 F2 hold를 latch 한 뒤,                               */
/*   필요한 주변장치 init이 끝난 시점에 SELFTEST_RunMaintenanceModeLoop()를    */
/*   호출한다.                                                                 */
/* -------------------------------------------------------------------------- */

void SELFTEST_RunMaintenanceModeLoop(void);

#ifdef __cplusplus
}
#endif

#endif /* APP_SELFTEST_SELFTEST_H */
