#ifndef DEBUG_UART_H
#define DEBUG_UART_H

#include "main.h"
#include "APP_STATE.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  DEBUG_UART                                                                 */
/*                                                                            */
/*  목적                                                                      */
/*  - USART1(PA9/PA10) 같은 유선 UART 포트를                                   */
/*    "가장 단순한 문자열 로그 출력구" 로 만든다.                               */
/*  - 사용자가 SWD 디버거 없이도,                                               */
/*    USB-UART TTL 어댑터 + 시리얼 터미널만으로                                 */
/*    현재 기기 상태를 관찰할 수 있게 한다.                                    */
/*  - 모든 공개 상태는 APP_STATE.debug_uart 에만 저장한다.                    */
/*                                                                            */
/*  가장 흔한 사용법                                                            */
/*      DEBUG_UART_PrintLine("boot ok");                                      */
/*      DEBUG_UART_Printf("gps fix=%u lat=%ld\r\n", fix, lat);              */
/*                                                                            */
/*  주의                                                                      */
/*  - 이 드라이버는 "런타임 로그 UART" 이다.                                   */
/*  - STM32CubeProgrammer의 UART 다운로드는                                   */
/*    이 함수들과는 다른 "시스템 부트로더" 경로다.                            */
/* -------------------------------------------------------------------------- */

#ifndef DEBUG_UART_HANDLE
#define DEBUG_UART_HANDLE                     huart1
#endif

#ifndef DEBUG_UART_TX_TIMEOUT_MS
#define DEBUG_UART_TX_TIMEOUT_MS             100u
#endif

#ifndef DEBUG_UART_FORCE_REINIT_ON_INIT
#define DEBUG_UART_FORCE_REINIT_ON_INIT      0u
#endif

#ifndef DEBUG_UART_BAUDRATE
#define DEBUG_UART_BAUDRATE                  115200u
#endif

/* -------------------------------------------------------------------------- */
/*  TX queue 크기                                                              */
/*                                                                            */
/*  DEBUG UART는 짧은 로그 문자열 위주이므로                                    */
/*  DMA보다 interrupt-driven TX ring이 구현 복잡도와 재생성 내성 측면에서      */
/*  더 유리하다.                                                               */
/* -------------------------------------------------------------------------- */
#ifndef DEBUG_UART_TX_RING_SIZE
#define DEBUG_UART_TX_RING_SIZE              1024u
#endif

/* -------------------------------------------------------------------------- */
/*  공개 API                                                                   */
/* -------------------------------------------------------------------------- */

/* APP_STATE.debug_uart slice 초기화.
 * 기본값은 CubeMX가 만든 huart 설정을 그대로 사용한다.
 * 필요하면 DEBUG_UART_FORCE_REINIT_ON_INIT를 1로 켜서 115200 8N1로 강제 가능. */
void DEBUG_UART_Init(void);

/* raw byte 송신 */
HAL_StatusTypeDef DEBUG_UART_Write(const uint8_t *data, uint16_t length);

/* 문자열 그대로 송신 */
HAL_StatusTypeDef DEBUG_UART_Print(const char *text);

/* 문자열 뒤에 CRLF를 붙여 한 줄 송신 */
HAL_StatusTypeDef DEBUG_UART_PrintLine(const char *text);

/* printf 스타일 helper */
HAL_StatusTypeDef DEBUG_UART_Printf(const char *fmt, ...);

/* HAL UART callback 진입점.
 * main.c의 HAL_UART_TxCpltCallback() / HAL_UART_ErrorCallback()에서
 * 그대로 전달해 주면 된다. */
void DEBUG_UART_OnUartTxCplt(UART_HandleTypeDef *huart);
void DEBUG_UART_OnUartError(UART_HandleTypeDef *huart);

/* recovery kick용 main loop task */
void DEBUG_UART_Task(uint32_t now_ms);

/* APP_STATE snapshot 복사 helper */
void DEBUG_UART_CopySnapshot(app_debug_uart_state_t *out_snapshot);

/* 자주 쓰기 위한 짧은 매크로 */
#define DBG_PRINTF(...)   ((void)DEBUG_UART_Printf(__VA_ARGS__))
#define DBG_PRINTLN(txt)  ((void)DEBUG_UART_PrintLine((txt)))

#ifdef __cplusplus
}
#endif

#endif /* DEBUG_UART_H */
