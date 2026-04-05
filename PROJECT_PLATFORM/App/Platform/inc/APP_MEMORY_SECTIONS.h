#ifndef APP_MEMORY_SECTIONS_H
#define APP_MEMORY_SECTIONS_H

/* -------------------------------------------------------------------------- */
/*  APP_MEMORY_SECTIONS                                                        */
/*                                                                            */
/*  목적                                                                      */
/*  - STM32F407VGT6 의 64KB CCMRAM 을 "CPU 전용 고속 저장소"로 일관되게 사용하기  */
/*    위한 공통 매크로 모음이다.                                               */
/*  - MOTOR / VARIO 프로젝트가 같은 공통 Platform 을 공유하므로,                */
/*    섹션 이름과 선언 규칙도 이 파일 하나로 통일한다.                         */
/*                                                                            */
/*  중요한 하드웨어 제약                                                       */
/*  - CCMRAM(0x1000_0000)은 CPU는 빠르게 접근할 수 있지만, DMA가 직접 읽거나     */
/*    쓸 수 없다.                                                              */
/*  - 따라서 DMA source / destination buffer, DMA descriptor,                */
/*    peripheral bus master가 직접 건드리는 메모리는 절대로 이 섹션으로         */
/*    보내면 안 된다.                                                          */
/*                                                                            */
/*  사용 원칙                                                                  */
/*  - APP_CCMRAM_BSS : 0으로 초기화되는 큰 정적/전역 저장소에 사용한다.         */
/*    예) LUT, 소프트웨어 ring buffer, UI 동적 상태, logger queue 등            */
/*  - APP_CCMRAM_DATA : non-zero 초기값이 필요한 정적/전역 저장소에 사용한다.    */
/*    현재 프로젝트에서는 거의 사용하지 않지만, 링커/스타트업 경로를 미리         */
/*    준비해 두어 이후에도 같은 규칙으로 확장할 수 있게 한다.                  */
/*                                                                            */
/*  문법 예시                                                                  */
/*      static uint8_t  s_rx_ring[4096] APP_CCMRAM_BSS;                        */
/*      static uint16_t s_lut[4097] APP_CCMRAM_BSS;                            */
/* -------------------------------------------------------------------------- */

#if defined(__GNUC__)
#define APP_CCMRAM_DATA __attribute__((section(".ccmram_data")))
#define APP_CCMRAM_BSS  __attribute__((section(".ccmram_bss")))
#else
#define APP_CCMRAM_DATA
#define APP_CCMRAM_BSS
#endif

#endif /* APP_MEMORY_SECTIONS_H */
