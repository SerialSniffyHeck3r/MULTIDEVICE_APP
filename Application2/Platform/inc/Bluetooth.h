#ifndef BLUETOOTH_H
#define BLUETOOTH_H

#include "main.h"
#include "APP_STATE.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  Bluetooth                                                                  */
/*                                                                            */
/*  이 드라이버가 맡는 역할                                                    */
/*  - Classic Bluetooth SPP(UART 대체) 모듈을                                 */
/*    "무선 시리얼 케이블" 처럼 다룬다.                                        */
/*  - 지금 단계에서는 상위 프로토콜(XCSoar용 NMEA, OBD 프레임 등)을           */
/*    완전히 확정하지 않고,                                                     */
/*    먼저 "송신이 되는가 / 수신이 되는가 / 양방향 echo가 되는가" 를           */
/*    빠르게 검증하는 bring-up 계층을 제공한다.                               */
/*  - 모든 공개 상태는 APP_STATE.bluetooth 에만 저장한다.                     */
/*                                                                            */
/*  블루투스를 잘 모르는 사람을 위한 요약                                      */
/*  - 여기서 가정하는 BC417 계열 모듈은 보통 BLE가 아니라                     */
/*    Bluetooth Classic SPP(Serial Port Profile) 계열이다.                    */
/*  - MCU 입장에서는 "UART로 문자열을 보내면, 페어링된 상대에게              */
/*    무선으로 그 문자열이 전달되는 장치" 처럼 생각하면 된다.                 */
/*  - 즉, 앱 계층은 유선 UART를 쓰듯                                            */
/*      Bluetooth_SendLine("PING");                                            */
/*    같은 호출만 하면 된다.                                                  */
/*                                                                            */
/*  사용 규칙                                                                  */
/*  1) 부팅 시 Bluetooth_Init() 1회 호출                                       */
/*  2) main while(1) 안에서 Bluetooth_Task(HAL_GetTick()) 반복 호출           */
/*  3) HAL_UART_RxCpltCallback() / HAL_UART_ErrorCallback() 에서               */
/*     Bluetooth_OnUartRxCplt() / Bluetooth_OnUartError() 전달                 */
/*  4) 앱 로직은 Bluetooth_SendLine(), Bluetooth_SendPrintf(),                 */
/*     Bluetooth_SendNmeaSentenceBody() 같은 helper만 사용                     */
/*                                                                            */
/*  CubeMX / .ioc 재생성 내성 포인트                                           */
/*  - 생성 코드(usart.c)를 직접 수정하지 않는다.                               */
/*  - 이 드라이버가 init 시점에 원하는 baud/8N1 설정을 다시 맞춘다.            */
/*  - 따라서 .ioc 재생성으로 huart3 baud가 바뀌어도                            */
/*    driver init가 다시 기본 bring-up 값으로 정렬한다.                        */
/* -------------------------------------------------------------------------- */

#ifndef BLUETOOTH_UART_HANDLE
#define BLUETOOTH_UART_HANDLE                  huart3
#endif

/* -------------------------------------------------------------------------- */
/*  BC417 / HC-05류 데이터 모드 기본값                                          */
/*                                                                            */
/*  주의                                                                      */
/*  - 많은 BC417/HC-05 계열 보드의 "통신 모드(data mode)" 기본 baud는          */
/*    9600 8N1 이다.                                                           */
/*  - 보드별로 이미 다른 baud로 설정되어 있을 수도 있으므로,                    */
/*    실제 모듈이 응답이 없으면 이 값부터 먼저 의심하면 된다.                  */
/* -------------------------------------------------------------------------- */
#ifndef BLUETOOTH_DATA_BAUDRATE
#define BLUETOOTH_DATA_BAUDRATE               9600u
#endif

#ifndef BLUETOOTH_FORCE_UART_REINIT_ON_INIT
#define BLUETOOTH_FORCE_UART_REINIT_ON_INIT   1u
#endif

#ifndef BLUETOOTH_RX_RING_SIZE
#define BLUETOOTH_RX_RING_SIZE                256u
#endif

#ifndef BLUETOOTH_RX_PARSE_BUDGET
#define BLUETOOTH_RX_PARSE_BUDGET             128u
#endif

/* -------------------------------------------------------------------------- */
/*  TX queue 크기                                                              */
/*                                                                            */
/*  Bluetooth TX는 짧은 control text / NMEA demo / echo 응답이 대부분이므로,    */
/*  DMA까지 끌어오지 않고 interrupt-driven ring buffer 방식이 더 단순하고       */
/*  CubeMX 재생성에도 덜 취약하다.                                             */
/* -------------------------------------------------------------------------- */
#ifndef BLUETOOTH_TX_RING_SIZE
#define BLUETOOTH_TX_RING_SIZE                512u
#endif

#ifndef BLUETOOTH_TX_TIMEOUT_MS
#define BLUETOOTH_TX_TIMEOUT_MS               100u
#endif

#ifndef BLUETOOTH_AUTO_PING_PERIOD_MS
#define BLUETOOTH_AUTO_PING_PERIOD_MS         1000u
#endif

#ifndef BLUETOOTH_LINE_TEXT_MAX
#define BLUETOOTH_LINE_TEXT_MAX               APP_BLUETOOTH_LAST_TEXT_MAX
#endif

#ifndef BLUETOOTH_MIRROR_TO_DEBUG_UART
#define BLUETOOTH_MIRROR_TO_DEBUG_UART        1u
#endif

/* -------------------------------------------------------------------------- */
/*  공개 API                                                                   */
/* -------------------------------------------------------------------------- */

/* 드라이버 내부 ring/parser 상태와 APP_STATE.bluetooth slice를 초기화하고,
 * 지정 UART를 기본 bring-up baud로 다시 맞춘 뒤 RX interrupt를 시작한다. */
void Bluetooth_Init(void);

/* SysTick 기반 주기 처리 함수.
 * - ISR가 ring에 쌓아 둔 RX byte를 main context에서 line 단위로 해석
 * - echo / auto ping / 간단한 test command 응답 처리
 * 를 수행한다. */
void Bluetooth_Task(uint32_t now_ms);

/* HAL UART callback 진입점.
 * main.c의 HAL_UART_RxCpltCallback() / HAL_UART_TxCpltCallback() /
 * HAL_UART_ErrorCallback()에서 그대로 전달해 주면 된다. */
void Bluetooth_OnUartRxCplt(UART_HandleTypeDef *huart);
void Bluetooth_OnUartTxCplt(UART_HandleTypeDef *huart);
void Bluetooth_OnUartError(UART_HandleTypeDef *huart);

/* -------------------------------------------------------------------------- */
/*  가장 자주 쓰게 될 송신 helper                                               */
/* -------------------------------------------------------------------------- */

/* raw byte 송신.
 * 바이너리 프레임을 직접 보내고 싶을 때 사용한다. */
HAL_StatusTypeDef Bluetooth_SendBytes(const uint8_t *data, uint16_t length);

/* null-terminated 문자열을 그대로 송신한다.
 * 개행(CR/LF)을 자동으로 붙이지 않는다. */
HAL_StatusTypeDef Bluetooth_SendString(const char *text);

/* 문자열 뒤에 "\r\n" 을 붙여 한 줄(line) 단위로 송신한다.
 * 휴대폰/PC의 Bluetooth serial terminal 테스트에 가장 편한 기본 함수다. */
HAL_StatusTypeDef Bluetooth_SendLine(const char *text);

/* printf 스타일 송신 helper.
 * 디버그 텍스트를 한 줄로 만들기 쉽도록 내부에서 vsnprintf를 사용한다. */
HAL_StatusTypeDef Bluetooth_SendPrintf(const char *fmt, ...);

/* NMEA 계열 문장을 쉽게 만들기 위한 helper.
 * 입력:  body only  -> 예: "PTSTM,HELLO,1"
 * 출력: "$PTSTM,HELLO,1*CS\r\n"
 * 실제 checksum은 함수가 계산해서 붙인다. */
HAL_StatusTypeDef Bluetooth_SendNmeaSentenceBody(const char *body_text);

/* -------------------------------------------------------------------------- */
/*  상태 제어 helper                                                            */
/* -------------------------------------------------------------------------- */

/* 수신한 줄을 다시 돌려보내는 echo 기능 on/off */
void Bluetooth_SetEchoEnabled(bool enable);
void Bluetooth_ToggleEcho(void);

/* 1초 주기의 auto ping 기능 on/off */
void Bluetooth_SetAutoPingEnabled(bool enable);
void Bluetooth_ToggleAutoPing(void);

/* APP_STATE.bluetooth snapshot 복사 helper */
void Bluetooth_CopySnapshot(app_bluetooth_state_t *out_snapshot);

/* -------------------------------------------------------------------------- */
/*  Bring-up / 시험용 one-call wrapper                                          */
/*                                                                            */
/*  의도                                                                       */
/*  - 상위 레이어를 아직 안 만든 상태에서도                                     */
/*    "아무 함수 하나만 호출해서" 송신 시험을 할 수 있게 하기 위함이다.        */
/*  - 사용자는 일단 main/button/debug page 어디에서든                           */
/*    DoSomething 계열을 불러서 반응을 볼 수 있다.                              */
/* -------------------------------------------------------------------------- */

/* 가장 단순한 데모 호출.
 * 현재 구현은 Hello 계열 송신으로 연결되어 있다. */
void Bluetooth_DoSomething(void);

/* "PING" 한 줄 송신 */
void Bluetooth_DoSomethingPing(void);

/* "HELLO FROM STM32" 한 줄 송신 */
void Bluetooth_DoSomethingHello(void);

/* 현재 카운터/상태 요약 문자열 송신 */
void Bluetooth_DoSomethingInfo(void);

/* NMEA framing이 필요한 상위 레이어 실험을 위한 데모 문장 송신 */
void Bluetooth_DoSomethingDemoNmea(void);

#ifdef __cplusplus
}
#endif

#endif /* BLUETOOTH_H */
