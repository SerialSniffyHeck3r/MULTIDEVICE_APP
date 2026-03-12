#include "DEBUG_UART.h"
#include "usart.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*  외부 UART handle                                                           */
/* -------------------------------------------------------------------------- */
extern UART_HandleTypeDef DEBUG_UART_HANDLE;

/* -------------------------------------------------------------------------- */
/*  드라이버 내부 runtime 상태                                                 */
/*                                                                            */
/*  DEBUG UART는 RX를 쓰지 않고 TX만 다루므로                                  */
/*  main -> ISR 단방향 SPSC TX ring 하나만 둔다.                               */
/*                                                                            */
/*  이렇게 하면 DBG_PRINTLN / DEBUG_UART_Printf 가 더 이상                     */
/*  HAL_UART_Transmit blocking timeout에 묶이지 않고,                           */
/*  "문자열을 queue에 넣고 리턴" 하는 저오버헤드 경로가 된다.                  */
/* -------------------------------------------------------------------------- */
typedef struct
{
    volatile uint16_t tx_head;                 /* main이 쓰는 TX ring head              */
    volatile uint16_t tx_tail;                 /* ISR가 소비하는 TX ring tail           */
    uint8_t  tx_ring[DEBUG_UART_TX_RING_SIZE]; /* main -> ISR 단방향 SPSC TX ring       */
    volatile uint16_t tx_inflight_length;      /* 현재 HAL_UART_Transmit_IT 길이        */
    volatile uint8_t  tx_running;              /* 현재 UART TX interrupt 동작 여부      */
} debug_uart_runtime_t;

static debug_uart_runtime_t s_debug_uart_rt;

/* -------------------------------------------------------------------------- */
/*  내부 helper: 마지막 텍스트를 사람이 읽기 좋게 저장                          */
/*                                                                            */
/*  raw byte 송신 API도 있으므로,                                               */
/*  제어문자나 비인쇄 문자가 들어와도 snapshot에는                              */
/*  화면에서 보기 쉬운 형태로 보관한다.                                        */
/* -------------------------------------------------------------------------- */
static void DEBUG_UART_RecordLastTextFromBytes(const uint8_t *data, uint16_t length)
{
    app_debug_uart_state_t *debug_uart;
    uint16_t index;
    uint16_t copy_length;

    debug_uart = (app_debug_uart_state_t *)&g_app_state.debug_uart;

    memset(debug_uart->last_text, 0, sizeof(debug_uart->last_text));

    if ((data == 0) || (length == 0u))
    {
        return;
    }

    copy_length = length;
    if (copy_length >= (uint16_t)sizeof(debug_uart->last_text))
    {
        copy_length = (uint16_t)sizeof(debug_uart->last_text) - 1u;
    }

    for (index = 0u; index < copy_length; index++)
    {
        uint8_t ch;

        ch = data[index];

        if ((ch >= 32u) && (ch <= 126u))
        {
            debug_uart->last_text[index] = (char)ch;
        }
        else if (ch == '\r')
        {
            debug_uart->last_text[index] = '\\';

            if ((index + 1u) < copy_length)
            {
                debug_uart->last_text[index + 1u] = 'r';
                index++;
            }
        }
        else if (ch == '\n')
        {
            debug_uart->last_text[index] = '\\';

            if ((index + 1u) < copy_length)
            {
                debug_uart->last_text[index + 1u] = 'n';
                index++;
            }
        }
        else
        {
            debug_uart->last_text[index] = '.';
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  TX ring helper                                                             */
/* -------------------------------------------------------------------------- */
static void DEBUG_UART_TxRingReset(void)
{
    s_debug_uart_rt.tx_head = 0u;
    s_debug_uart_rt.tx_tail = 0u;
    s_debug_uart_rt.tx_inflight_length = 0u;
    s_debug_uart_rt.tx_running = 0u;
}

static uint16_t DEBUG_UART_TxRingLevelFrom(uint16_t head, uint16_t tail)
{
    if (head >= tail)
    {
        return (uint16_t)(head - tail);
    }

    return (uint16_t)(DEBUG_UART_TX_RING_SIZE - tail + head);
}

static uint16_t DEBUG_UART_TxRingFreeFrom(uint16_t head, uint16_t tail)
{
    return (uint16_t)((DEBUG_UART_TX_RING_SIZE - 1u) -
                      DEBUG_UART_TxRingLevelFrom(head, tail));
}

static void DEBUG_UART_TxKick(void)
{
    app_debug_uart_state_t *debug_uart;
    HAL_StatusTypeDef status;
    uint16_t head_snapshot;
    uint16_t tail_snapshot;
    uint16_t chunk_length;
    uint8_t *chunk_ptr;

    debug_uart = (app_debug_uart_state_t *)&g_app_state.debug_uart;

    __disable_irq();
    head_snapshot = s_debug_uart_rt.tx_head;
    tail_snapshot = s_debug_uart_rt.tx_tail;

    if ((s_debug_uart_rt.tx_running != 0u) || (head_snapshot == tail_snapshot))
    {
        __enable_irq();
        return;
    }

    if (head_snapshot > tail_snapshot)
    {
        chunk_length = (uint16_t)(head_snapshot - tail_snapshot);
    }
    else
    {
        chunk_length = (uint16_t)(DEBUG_UART_TX_RING_SIZE - tail_snapshot);
    }

    s_debug_uart_rt.tx_running = 1u;
    s_debug_uart_rt.tx_inflight_length = chunk_length;
    chunk_ptr = &s_debug_uart_rt.tx_ring[tail_snapshot];
    __enable_irq();

    status = HAL_UART_Transmit_IT(&DEBUG_UART_HANDLE, chunk_ptr, chunk_length);
    debug_uart->last_hal_status = (uint8_t)status;

    if (status != HAL_OK)
    {
        __disable_irq();
        s_debug_uart_rt.tx_running = 0u;
        s_debug_uart_rt.tx_inflight_length = 0u;
        __enable_irq();

        debug_uart->tx_fail_count++;
    }
}

static HAL_StatusTypeDef DEBUG_UART_TxEnqueue(const uint8_t *data, uint16_t length)
{
    app_debug_uart_state_t *debug_uart;
    uint16_t head_snapshot;
    uint16_t tail_snapshot;
    uint16_t free_snapshot;
    uint16_t write_index;
    uint16_t index;

    debug_uart = (app_debug_uart_state_t *)&g_app_state.debug_uart;

    if ((data == 0) || (length == 0u))
    {
        debug_uart->last_hal_status = (uint8_t)HAL_ERROR;
        return HAL_ERROR;
    }

    if (length >= DEBUG_UART_TX_RING_SIZE)
    {
        debug_uart->tx_fail_count++;
        debug_uart->last_hal_status = (uint8_t)HAL_ERROR;
        return HAL_ERROR;
    }

    __disable_irq();
    head_snapshot = s_debug_uart_rt.tx_head;
    tail_snapshot = s_debug_uart_rt.tx_tail;
    free_snapshot = DEBUG_UART_TxRingFreeFrom(head_snapshot, tail_snapshot);
    __enable_irq();

    if (free_snapshot < length)
    {
        debug_uart->tx_fail_count++;
        debug_uart->last_hal_status = (uint8_t)HAL_BUSY;
        return HAL_BUSY;
    }

    write_index = head_snapshot;

    for (index = 0u; index < length; index++)
    {
        s_debug_uart_rt.tx_ring[write_index] = data[index];

        write_index = (uint16_t)(write_index + 1u);
        if (write_index >= DEBUG_UART_TX_RING_SIZE)
        {
            write_index = 0u;
        }
    }

    __disable_irq();
    s_debug_uart_rt.tx_head = write_index;
    __enable_irq();

    debug_uart->last_hal_status = (uint8_t)HAL_OK;
    DEBUG_UART_TxKick();
    return HAL_OK;
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: APP_STATE slice 초기화                                         */
/* -------------------------------------------------------------------------- */
static void DEBUG_UART_ResetAppStateSlice(app_debug_uart_state_t *debug_uart)
{
    if (debug_uart == 0)
    {
        return;
    }

    memset(debug_uart, 0, sizeof(*debug_uart));

    debug_uart->initialized     = false;
    debug_uart->last_hal_status = 0xFFu;
    debug_uart->last_tx_ms      = 0u;
    debug_uart->tx_count        = 0u;
    debug_uart->tx_bytes        = 0u;
    debug_uart->tx_fail_count   = 0u;
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: 선택적으로 UART를 115200 8N1로 다시 맞추기                     */
/*                                                                            */
/*  기본값은 CubeMX 설정을 존중한다.                                           */
/*  단, bring-up 단계에서 "무조건 115200 8N1로 맞추고 싶다" 면                  */
/*  DEBUG_UART_FORCE_REINIT_ON_INIT를 1로 켜면 된다.                            */
/* -------------------------------------------------------------------------- */
static void DEBUG_UART_ApplyDefaultConfigIfRequested(void)
{
#if (DEBUG_UART_FORCE_REINIT_ON_INIT != 0u)
    (void)HAL_UART_DeInit(&DEBUG_UART_HANDLE);

    DEBUG_UART_HANDLE.Init.BaudRate     = DEBUG_UART_BAUDRATE;
    DEBUG_UART_HANDLE.Init.WordLength   = UART_WORDLENGTH_8B;
    DEBUG_UART_HANDLE.Init.StopBits     = UART_STOPBITS_1;
    DEBUG_UART_HANDLE.Init.Parity       = UART_PARITY_NONE;
    DEBUG_UART_HANDLE.Init.Mode         = UART_MODE_TX_RX;
    DEBUG_UART_HANDLE.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    DEBUG_UART_HANDLE.Init.OverSampling = UART_OVERSAMPLING_16;

    (void)HAL_UART_Init(&DEBUG_UART_HANDLE);
#endif
}

/* -------------------------------------------------------------------------- */
/*  공개 API: init                                                              */
/* -------------------------------------------------------------------------- */
void DEBUG_UART_Init(void)
{
    app_debug_uart_state_t *debug_uart;

    debug_uart = (app_debug_uart_state_t *)&g_app_state.debug_uart;

    DEBUG_UART_ResetAppStateSlice(debug_uart);
    DEBUG_UART_TxRingReset();
    DEBUG_UART_ApplyDefaultConfigIfRequested();

    debug_uart->initialized     = true;
    debug_uart->last_hal_status = (uint8_t)HAL_OK;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: raw byte 송신                                                     */
/*                                                                            */
/*  주의                                                                      */
/*  - 이 함수는 이제 HAL_UART_Transmit blocking 경로를 직접 호출하지 않는다.   */
/*  - 대신 software TX ring에 데이터를 enqueue하고                             */
/*    HAL_UART_Transmit_IT()가 background drain 하도록 kick만 건다.            */
/*  - 따라서 main loop에서 긴 timeout만큼 붙잡히지 않는다.                     */
/* -------------------------------------------------------------------------- */
HAL_StatusTypeDef DEBUG_UART_Write(const uint8_t *data, uint16_t length)
{
    app_debug_uart_state_t *debug_uart;
    HAL_StatusTypeDef status;

    debug_uart = (app_debug_uart_state_t *)&g_app_state.debug_uart;

    if ((data == 0) || (length == 0u))
    {
        debug_uart->last_hal_status = (uint8_t)HAL_ERROR;
        return HAL_ERROR;
    }

    if (debug_uart->initialized == false)
    {
        debug_uart->last_hal_status = (uint8_t)HAL_ERROR;
        return HAL_ERROR;
    }

    status = DEBUG_UART_TxEnqueue(data, length);
    if (status == HAL_OK)
    {
        DEBUG_UART_RecordLastTextFromBytes(data, length);
        debug_uart->tx_count++;
    }

    return status;
}


/* -------------------------------------------------------------------------- */
/*  공개 API: 문자열 그대로 송신                                                */
/* -------------------------------------------------------------------------- */
HAL_StatusTypeDef DEBUG_UART_Print(const char *text)
{
    size_t text_length;

    if (text == 0)
    {
        return HAL_ERROR;
    }

    text_length = strlen(text);
    if (text_length == 0u)
    {
        return HAL_OK;
    }

    if (text_length > 65535u)
    {
        text_length = 65535u;
    }

    return DEBUG_UART_Write((const uint8_t *)text, (uint16_t)text_length);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 문자열 + CRLF 송신                                                */
/* -------------------------------------------------------------------------- */
HAL_StatusTypeDef DEBUG_UART_PrintLine(const char *text)
{
    char line_buffer[160];
    int written;

    if (text == 0)
    {
        return HAL_ERROR;
    }

    written = snprintf(line_buffer, sizeof(line_buffer), "%s\r\n", text);
    if ((written <= 0) || ((size_t)written >= sizeof(line_buffer)))
    {
        return HAL_ERROR;
    }

    return DEBUG_UART_Write((const uint8_t *)line_buffer, (uint16_t)written);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: printf 스타일 송신                                                */
/* -------------------------------------------------------------------------- */
HAL_StatusTypeDef DEBUG_UART_Printf(const char *fmt, ...)
{
    va_list args;
    char text_buffer[192];
    int written;

    if (fmt == 0)
    {
        return HAL_ERROR;
    }

    va_start(args, fmt);
    written = vsnprintf(text_buffer, sizeof(text_buffer), fmt, args);
    va_end(args);

    if (written <= 0)
    {
        return HAL_ERROR;
    }

    if ((size_t)written >= sizeof(text_buffer))
    {
        written = (int)(sizeof(text_buffer) - 1u);
        text_buffer[written] = '\0';
    }

    return DEBUG_UART_Write((const uint8_t *)text_buffer, (uint16_t)written);
}


/* -------------------------------------------------------------------------- */
/*  공개 API: UART TX complete callback                                         */
/*                                                                            */
/*  TX ring의 이번 contiguous chunk가 모두 전송되면                            */
/*  tail을 전진시키고, 남은 queue가 있으면 다음 chunk를 이어서 시작한다.        */
/* -------------------------------------------------------------------------- */
void DEBUG_UART_OnUartTxCplt(UART_HandleTypeDef *huart)
{
    app_debug_uart_state_t *debug_uart;
    uint16_t tail_snapshot;
    uint16_t inflight_snapshot;
    uint16_t next_tail;
    uint8_t need_kick;

    if (huart == 0)
    {
        return;
    }

    if (huart->Instance != DEBUG_UART_HANDLE.Instance)
    {
        return;
    }

    debug_uart = (app_debug_uart_state_t *)&g_app_state.debug_uart;

    __disable_irq();
    tail_snapshot     = s_debug_uart_rt.tx_tail;
    inflight_snapshot = s_debug_uart_rt.tx_inflight_length;

    next_tail = (uint16_t)(tail_snapshot + inflight_snapshot);
    if (next_tail >= DEBUG_UART_TX_RING_SIZE)
    {
        next_tail = (uint16_t)(next_tail - DEBUG_UART_TX_RING_SIZE);
    }

    s_debug_uart_rt.tx_tail = next_tail;
    s_debug_uart_rt.tx_inflight_length = 0u;
    s_debug_uart_rt.tx_running = 0u;
    need_kick = (s_debug_uart_rt.tx_head != s_debug_uart_rt.tx_tail) ? 1u : 0u;
    __enable_irq();

    debug_uart->last_hal_status = (uint8_t)HAL_OK;
    debug_uart->last_tx_ms = HAL_GetTick();
    debug_uart->tx_bytes += inflight_snapshot;

    if (need_kick != 0u)
    {
        DEBUG_UART_TxKick();
    }
}

/* -------------------------------------------------------------------------- */
/*  공개 API: UART error callback                                               */
/*                                                                            */
/*  현재 DEBUG UART는 RX를 쓰지 않으므로                                       */
/*  에러가 들어오면 상태만 기록하고, 다음 main loop task에서 다시 kick할 수    */
/*  있도록 tx_running 플래그만 내려 준다.                                      */
/* -------------------------------------------------------------------------- */
void DEBUG_UART_OnUartError(UART_HandleTypeDef *huart)
{
    app_debug_uart_state_t *debug_uart;

    if (huart == 0)
    {
        return;
    }

    if (huart->Instance != DEBUG_UART_HANDLE.Instance)
    {
        return;
    }

    debug_uart = (app_debug_uart_state_t *)&g_app_state.debug_uart;

    __disable_irq();
    s_debug_uart_rt.tx_running = 0u;
    s_debug_uart_rt.tx_inflight_length = 0u;
    __enable_irq();

    debug_uart->last_hal_status = (uint8_t)HAL_ERROR;
    debug_uart->tx_fail_count++;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: main loop task                                                    */
/*                                                                            */
/*  보통은 enqueue 시점과 TX complete ISR가 queue를 계속 흘려보내므로            */
/*  별도의 task가 꼭 필요하지는 않다.                                          */
/*                                                                            */
/*  하지만 UART busy/error 후 TX state가 풀린 채 멈춰 있을 수 있으므로,         */
/*  main loop에서 가볍게 한 번 더 kick해 주는 복구 경로를 둔다.                */
/* -------------------------------------------------------------------------- */
void DEBUG_UART_Task(uint32_t now_ms)
{
    (void)now_ms;
    DEBUG_UART_TxKick();
}

/* -------------------------------------------------------------------------- */
/*  공개 API: snapshot 복사 helper                                              */
/* -------------------------------------------------------------------------- */
void DEBUG_UART_CopySnapshot(app_debug_uart_state_t *out_snapshot)
{
    APP_STATE_CopyDebugUartSnapshot(out_snapshot);
}
