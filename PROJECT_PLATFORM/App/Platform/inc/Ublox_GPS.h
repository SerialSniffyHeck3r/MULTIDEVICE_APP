#ifndef UBLOX_GPS_H
#define UBLOX_GPS_H

#include "main.h"
#include "APP_STATE.h"

#ifdef __cplusplus
extern "C" {
#endif

#ifndef UBLOX_GPS_UART_HANDLE
#define UBLOX_GPS_UART_HANDLE huart2
#endif

#ifndef UBLOX_GPS_MAX_PAYLOAD
#define UBLOX_GPS_MAX_PAYLOAD 512u
#endif

#ifndef UBLOX_GPS_HOST_BAUD
#define UBLOX_GPS_HOST_BAUD   115200u
#endif

#ifndef UBLOX_GPS_QUERY_RETRY_MS
#define UBLOX_GPS_QUERY_RETRY_MS 1000u
#endif

#ifndef UBLOX_GPS_QUERY_MAX_RETRY
#define UBLOX_GPS_QUERY_MAX_RETRY 5u
#endif

#ifndef UBLOX_GPS_QUERY_START_DELAY_MS
#define UBLOX_GPS_QUERY_START_DELAY_MS 3000u
#endif

/* -------------------------------------------------------------------------- */
/*  RX ring / parse budget                                                     */
/*                                                                            */
/*  NAV-PVT + NAV-SAT 를 둘 다 유지할 것이므로                                  */
/*  기존 2048 / 512 보다 여유를 조금 더 준다.                                   */
/*                                                                            */
/*  - RX_RING_SIZE: ISR가 먼저 받아 적어 두는 대기 공간                        */
/*  - RX_PARSE_BUDGET: 메인 루프 1회에서 최대 몇 바이트까지 파싱할지            */
/* -------------------------------------------------------------------------- */
#ifndef UBLOX_GPS_RX_RING_SIZE
#define UBLOX_GPS_RX_RING_SIZE 4096u
#endif

#ifndef UBLOX_GPS_RX_PARSE_BUDGET
#define UBLOX_GPS_RX_PARSE_BUDGET 2048u
#endif

/* 메인에서 이 함수 하나만 호출하면
 * - 내부 상태 초기화
 * - UART 보드레이트 정렬
 * - UBX only 설정
 * - 사용자 설정(APP_STATE.settings.gps)에 맞는 GNSS / rate / power 설정
 * - MON-VER poll
 * - CFG-VALGET poll
 * - direct RX interrupt 시작
 * 까지 전부 처리된다. */
void Init_Ublox_M10(void);

/* 레거시 호환용 API */
void Ublox_GPS_Init(void);
void Ublox_GPS_ConfigDefault(void);
void Ublox_GPS_StartRxIT(void);

/* 새 direct IRQ RX 시작 API */
void Ublox_GPS_StartRxIrqDriven(void);

/* 바이트 단위 파서 / IRQ 엔트리 / 주기적 관리 */
void Ublox_GPS_OnByte(uint8_t byte);

/* direct USART2 IRQ 엔트리 */
void Ublox_GPS_OnUartIrq(UART_HandleTypeDef *huart);

/* 레거시 HAL callback 호환 API */
void Ublox_GPS_OnUartRxCplt(UART_HandleTypeDef *huart);
void Ublox_GPS_OnUartError(UART_HandleTypeDef *huart);

void Ublox_GPS_Task(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* UBLOX_GPS_H */
