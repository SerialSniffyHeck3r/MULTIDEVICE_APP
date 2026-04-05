#include "DEBUG_UART.h"

#include <stdarg.h>

/* -------------------------------------------------------------------------- */
/*  DEBUG_UART no-op backend                                                   */
/*                                                                            */
/*  이 모듈은 의도적으로 어떤 런타임 버퍼도 보유하지 않는다.                     */
/*  즉, 과거의 TX ring / snapshot / 마지막 전송 시각 등은 모두 제거되었고,        */
/*  이 파일은 기존 호출부를 깨지 않게 유지하기 위한 호환성 셔터 역할만 한다.      */
/*                                                                            */
/*  DMA 비호환 CCMRAM 최적화와 별개로, DEBUG UART는 "RAM 자체를 아예 쓰지 않는"   */
/*  방향이 가장 단순하고 안전하므로 모든 API를 성공 반환 또는 no-op으로 통일했다. */
/* -------------------------------------------------------------------------- */

void DEBUG_UART_Init(void)
{
}

HAL_StatusTypeDef DEBUG_UART_Write(const uint8_t *data, uint16_t length)
{
    (void)data;
    (void)length;
    return HAL_OK;
}

HAL_StatusTypeDef DEBUG_UART_Print(const char *text)
{
    (void)text;
    return HAL_OK;
}

HAL_StatusTypeDef DEBUG_UART_PrintLine(const char *text)
{
    (void)text;
    return HAL_OK;
}

HAL_StatusTypeDef DEBUG_UART_Printf(const char *fmt, ...)
{
    va_list ap;

    va_start(ap, fmt);
    va_end(ap);
    return HAL_OK;
}

void DEBUG_UART_OnUartTxCplt(UART_HandleTypeDef *huart)
{
    (void)huart;
}

void DEBUG_UART_OnUartError(UART_HandleTypeDef *huart)
{
    (void)huart;
}

void DEBUG_UART_Task(uint32_t now_ms)
{
    (void)now_ms;
}
