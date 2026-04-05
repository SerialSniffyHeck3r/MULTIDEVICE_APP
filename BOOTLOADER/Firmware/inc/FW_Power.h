#ifndef FW_POWER_H
#define FW_POWER_H

#include "main.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* FW_POWER                                                                    */
/*                                                                            */
/* 역할                                                                       */
/* - BOOTLOADER 프로젝트에서 SparkFun Soft Power Switch Mk2의 OFF / PUSH 핀을  */
/*   안전하게 정렬한다.                                                        */
/* - CubeMX가 아직 예전 POWER_HOLD(PE2) 설정을 유지하고 있더라도,              */
/*   런타임에서 실제 Soft Power 배선(PE3=OFF, PE4=PUSH)을 다시 강제로 맞춘다. */
/* - bootloader -> app jump 직전에도 OFF 핀을 LOW로 다시 보장해서,             */
/*   점프 경계에서 전원 차단 glitch가 생기지 않게 막는다.                     */
/*                                                                            */
/* 설계 원칙                                                                   */
/* - IOC 재생성으로 날아가기 쉬운 gpio.c 를 직접 손대지 않는다.                */
/* - BOOTLOADER/Firmware/src 경로의 호출점 2곳만 수정한다.                     */
/*   1) FW_BOOT_Run() 초반                                                     */
/*   2) FW_FLASH_JumpToApp() 직전                                              */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* 공개 API: bootloader runtime 초기화                                         */
/*                                                                            */
/* 호출 시점                                                                   */
/* - FW_BOOT_Run() 안에서 FW_INPUT_Init() 보다 먼저 또는 바로 근처             */
/*                                                                            */
/* 수행 내용                                                                   */
/* - OFF  : output push-pull / LOW 고정                                        */
/* - PUSH : input pull-up                                                      */
/* -------------------------------------------------------------------------- */
void FW_POWER_Init(void);

/* -------------------------------------------------------------------------- */
/* 공개 API: app jump 직전 OFF 핀 재보정                                       */
/*                                                                            */
/* 호출 시점                                                                   */
/* - FW_FLASH_JumpToApp() 안에서 HAL_DeInit() 직후                            */
/*                                                                            */
/* 목적                                                                       */
/* - HAL_DeInit() 및 각종 점프 준비 과정에서 GPIO 상태가 애매해질 수 있으므로   */
/*   app reset handler로 넘어가기 직전에 OFF 핀을 LOW로 다시 확실히 잡는다.    */
/* - 이 함수는 return 이후에도 OFF 라인이 계속 LOW 유지되도록 구성한다.        */
/* -------------------------------------------------------------------------- */
void FW_POWER_PrepareForAppJump(void);

#ifdef __cplusplus
}
#endif

#endif /* FW_POWER_H */
