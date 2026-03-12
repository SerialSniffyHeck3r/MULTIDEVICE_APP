#ifndef FW_BOOT_CONFIG_H
#define FW_BOOT_CONFIG_H

#include "main.h"
#include "FW_BuildVersion.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  FW_BootConfig                                                              */
/*                                                                            */
/*  이 파일은 F/W Update Mode Boot와 Main App가 공통으로 보는                 */
/*  "불변 배치 정보" 와 "정책 상수" 를 모아 둔 설정 헤더다.                  */
/*                                                                            */
/*  이번 설계의 큰 방향                                                        */
/*  - 부트 영역은 0x08000000 부터 128KB를 고정 점유한다.                      */
/*  - 메인 앱은 0x08020000 부터 시작한다.                                     */
/*  - 엔드유저 업데이트 패키지는 SYSUPDATE.bin 하나만 사용한다.                */
/*  - W25Q16 외부 플래시는 staging 용도로 쓰지 않고,                           */
/*    "구버전 설치 시 전체 삭제해야 하는 유저 데이터 공간" 으로 본다.         */
/* -------------------------------------------------------------------------- */

#ifndef FW_BOOT_FLASH_BASE
#define FW_BOOT_FLASH_BASE                 0x08000000u
#endif

#ifndef FW_BOOT_RESERVED_SIZE
#define FW_BOOT_RESERVED_SIZE              0x00020000u   /* 128KB: Sector 0~4 */
#endif

#ifndef FW_FLASH_END_ADDRESS
#define FW_FLASH_END_ADDRESS               0x08100000u   /* STM32F407VG 1MB */
#endif

#define FW_APP_BASE_ADDRESS                (FW_BOOT_FLASH_BASE + FW_BOOT_RESERVED_SIZE)
#define FW_APP_MAX_SIZE_BYTES              (FW_FLASH_END_ADDRESS - FW_APP_BASE_ADDRESS)

#ifndef FW_UPDATE_FILENAME
#define FW_UPDATE_FILENAME                 "SYSUPDAT.bin"
#endif

#ifndef FW_PACKAGE_PRODUCT_TAG
#define FW_PACKAGE_PRODUCT_TAG             "MOTORDASH_F407"
#endif

#ifndef FW_VERSION_STRING_MAX
#define FW_VERSION_STRING_MAX              16u
#endif

#ifndef FW_PACKAGE_PRODUCT_TAG_MAX
#define FW_PACKAGE_PRODUCT_TAG_MAX         16u
#endif

#ifndef FW_BOOT_AUTO_RESCAN_MS
#define FW_BOOT_AUTO_RESCAN_MS             1000u
#endif

#ifndef FW_BOOT_AUTOFAIL_THRESHOLD
#define FW_BOOT_AUTOFAIL_THRESHOLD         3u
#endif

#ifndef FW_INPUT_DEBOUNCE_MS
#define FW_INPUT_DEBOUNCE_MS               25u
#endif

#ifndef FW_INPUT_LONG_PRESS_MS
#define FW_INPUT_LONG_PRESS_MS             700u
#endif

#ifndef FW_INPUT_ACTIVE_LEVEL
#define FW_INPUT_ACTIVE_LEVEL              GPIO_PIN_RESET
#endif

#ifndef FW_W25Q16_CAPACITY_BYTES
#define FW_W25Q16_CAPACITY_BYTES           0x00200000u   /* 2 MBytes */
#endif

#ifndef FW_PROGRESS_BAR_X
#define FW_PROGRESS_BAR_X                  12u
#endif

#ifndef FW_PROGRESS_BAR_Y
#define FW_PROGRESS_BAR_Y                  112u
#endif

#ifndef FW_PROGRESS_BAR_W
#define FW_PROGRESS_BAR_W                  216u
#endif

#ifndef FW_PROGRESS_BAR_H
#define FW_PROGRESS_BAR_H                  12u
#endif

/* -------------------------------------------------------------------------- */
/*  버전 packed 표현 helper                                                    */
/*                                                                            */
/*  0xAABBCCDD 형태로 비교 가능하게 만든다.                                   */
/*  문자열 "01.02.03.04" 는 0x01020304 로 packing 된다.                      */
/* -------------------------------------------------------------------------- */
#define FW_VERSION_PACK_U32(a, b, c, d) \
    ((((uint32_t)(a) & 0xFFu) << 24) | \
     (((uint32_t)(b) & 0xFFu) << 16) | \
     (((uint32_t)(c) & 0xFFu) << 8)  | \
     (((uint32_t)(d) & 0xFFu) << 0))

#ifdef __cplusplus
}
#endif

#endif /* FW_BOOT_CONFIG_H */
