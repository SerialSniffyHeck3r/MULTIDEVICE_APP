#ifndef FW_BOOTMAIN_H
#define FW_BOOTMAIN_H

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  FW_BootMain                                                                */
/*                                                                            */
/*  역할                                                                      */
/*  - Core/Src/main.c 에서 HAL / clock / MX_* init가 끝난 뒤 호출된다.        */
/*  - resident bootloader 런타임을 실제로 시작한다.                           */
/*  - 이 함수는 정상 경로에서는 Main App으로 jump 하거나,                      */
/*    F/W Update Mode로 진입한 뒤 return 하지 않는다.                         */
/* -------------------------------------------------------------------------- */

void FW_BOOT_Run(void);

#ifdef __cplusplus
}
#endif

#endif /* FW_BOOTMAIN_H */
