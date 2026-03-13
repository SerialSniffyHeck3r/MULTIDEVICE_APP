#include "Bluetooth.h"
#include "DEBUG_UART.h"
#include "usart.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*  외부 UART handle                                                           */
/* -------------------------------------------------------------------------- */
extern UART_HandleTypeDef BLUETOOTH_UART_HANDLE;

/* -------------------------------------------------------------------------- */
/*  드라이버 내부 runtime 상태                                                  */
/*                                                                            */
/*  원칙                                                                       */
/*  - ISR는 최대한 짧게 끝낸다.                                                */
/*  - ISR에서는 "바이트를 ring에 적재 + 다음 1바이트 수신 재arm" 만 한다.      */
/*  - 줄(line) 파싱, echo 응답, auto ping, 명령 해석은 모두 main context의      */
/*    Bluetooth_Task() 가 수행한다.                                            */
/* -------------------------------------------------------------------------- */

typedef struct
{
    volatile uint16_t rx_head;                     /* ISR가 쓰는 RX ring head                 */
    volatile uint16_t rx_tail;                     /* main이 소비하는 RX ring tail            */
    uint8_t  rx_ring[BLUETOOTH_RX_RING_SIZE];      /* ISR -> main 단방향 SPSC RX ring         */

    /* ---------------------------------------------------------------------- */
    /*  TX ring은 main -> ISR 단방향 SPSC 구조를 사용한다.                     */
    /*                                                                        */
    /*  main context는 head만 전진시키고 enqueue만 수행한다.                   */
    /*  UART TX complete ISR는 tail만 전진시키고 다음 chunk만 kick한다.        */
    /*                                                                        */
    /*  이렇게 하면 Bluetooth_SendLine/Printf가 더 이상 blocking transmit에     */
    /*  묶이지 않고, 짧은 메모리 복사 + kick 시도만 하고 곧바로 리턴한다.      */
    /* ---------------------------------------------------------------------- */
    volatile uint16_t tx_head;                     /* main이 쓰는 TX ring head               */
    volatile uint16_t tx_tail;                     /* ISR가 소비하는 TX ring tail            */
    uint8_t  tx_ring[BLUETOOTH_TX_RING_SIZE];      /* main -> ISR 단방향 SPSC TX ring        */
    volatile uint16_t tx_inflight_length;          /* 현재 HAL_UART_Transmit_IT 길이         */
    volatile uint8_t  tx_running;                  /* 현재 UART TX interrupt가 살아 있는가    */

    uint8_t  rx_byte_it;                           /* HAL_UART_Receive_IT 1-byte buffer       */

    char     line_build[BLUETOOTH_LINE_TEXT_MAX];  /* 현재 조립 중인 한 줄                     */
    uint16_t line_build_length;                    /* 현재 줄의 유효 길이                      */

    uint32_t next_auto_ping_ms;                    /* 다음 auto ping due 시각                  */
    uint32_t auto_ping_sequence;                   /* auto ping 번호                           */
} bluetooth_runtime_t;

static bluetooth_runtime_t s_bluetooth_rt;

/* -------------------------------------------------------------------------- */
/*  시간 helper                                                                */
/* -------------------------------------------------------------------------- */
static uint8_t Bluetooth_TimeDue(uint32_t now_ms, uint32_t due_ms)
{
    return ((int32_t)(now_ms - due_ms) >= 0) ? 1u : 0u;
}

/* -------------------------------------------------------------------------- */
/*  RX ring helper                                                             */
/* -------------------------------------------------------------------------- */
static void Bluetooth_RxRingReset(void)
{
    s_bluetooth_rt.rx_head = 0u;
    s_bluetooth_rt.rx_tail = 0u;
}

static uint16_t Bluetooth_RxRingLevelFrom(uint16_t head, uint16_t tail)
{
    if (head >= tail)
    {
        return (uint16_t)(head - tail);
    }

    return (uint16_t)(BLUETOOTH_RX_RING_SIZE - tail + head);
}

static uint16_t Bluetooth_RxRingLevel(void)
{
    return Bluetooth_RxRingLevelFrom(s_bluetooth_rt.rx_head, s_bluetooth_rt.rx_tail);
}

static uint16_t Bluetooth_RxRingPushIsr(uint8_t byte)
{
    uint16_t head;
    uint16_t tail;
    uint16_t next;

    head = s_bluetooth_rt.rx_head;
    tail = s_bluetooth_rt.rx_tail;
    next = (uint16_t)(head + 1u);

    if (next >= BLUETOOTH_RX_RING_SIZE)
    {
        next = 0u;
    }

    if (next == tail)
    {
        return 0xFFFFu;
    }

    s_bluetooth_rt.rx_ring[head] = byte;
    s_bluetooth_rt.rx_head = next;

    return Bluetooth_RxRingLevelFrom(next, tail);
}

static uint8_t Bluetooth_RxRingPop(uint8_t *out_byte)
{
    uint16_t tail;

    if (out_byte == 0)
    {
        return 0u;
    }

    tail = s_bluetooth_rt.rx_tail;
    if (tail == s_bluetooth_rt.rx_head)
    {
        return 0u;
    }

    *out_byte = s_bluetooth_rt.rx_ring[tail];

    tail = (uint16_t)(tail + 1u);
    if (tail >= BLUETOOTH_RX_RING_SIZE)
    {
        tail = 0u;
    }

    s_bluetooth_rt.rx_tail = tail;
    return 1u;
}


/* -------------------------------------------------------------------------- */
/*  TX ring helper                                                             */
/*                                                                            */
/*  설계 포인트                                                                */
/*  - producer(main)는 queue에 복사만 하고 즉시 리턴한다.                      */
/*  - consumer(ISR)는 HAL_UART_Transmit_IT 완료 시점마다 tail만 전진시킨다.    */
/*  - head는 "데이터가 완전히 복사된 뒤" 마지막에 갱신해서, ISR가 부분 복사된  */
/*    바이트를 잘못 읽지 않게 한다.                                            */
/* -------------------------------------------------------------------------- */
static void Bluetooth_TxRingReset(void)
{
    s_bluetooth_rt.tx_head = 0u;
    s_bluetooth_rt.tx_tail = 0u;
    s_bluetooth_rt.tx_inflight_length = 0u;
    s_bluetooth_rt.tx_running = 0u;
}

static uint16_t Bluetooth_TxRingLevelFrom(uint16_t head, uint16_t tail)
{
    if (head >= tail)
    {
        return (uint16_t)(head - tail);
    }

    return (uint16_t)(BLUETOOTH_TX_RING_SIZE - tail + head);
}

static uint16_t Bluetooth_TxRingFreeFrom(uint16_t head, uint16_t tail)
{
    return (uint16_t)((BLUETOOTH_TX_RING_SIZE - 1u) -
                      Bluetooth_TxRingLevelFrom(head, tail));
}

static void Bluetooth_TxKick(void)
{
    app_bluetooth_state_t *bluetooth;
    HAL_StatusTypeDef status;
    uint16_t head_snapshot;
    uint16_t tail_snapshot;
    uint16_t chunk_length;
    uint8_t *chunk_ptr;

    bluetooth = (app_bluetooth_state_t *)&g_app_state.bluetooth;

    __disable_irq();
    head_snapshot = s_bluetooth_rt.tx_head;
    tail_snapshot = s_bluetooth_rt.tx_tail;

    if ((s_bluetooth_rt.tx_running != 0u) || (head_snapshot == tail_snapshot))
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
        chunk_length = (uint16_t)(BLUETOOTH_TX_RING_SIZE - tail_snapshot);
    }

    s_bluetooth_rt.tx_running = 1u;
    s_bluetooth_rt.tx_inflight_length = chunk_length;
    chunk_ptr = &s_bluetooth_rt.tx_ring[tail_snapshot];
    __enable_irq();

    status = HAL_UART_Transmit_IT(&BLUETOOTH_UART_HANDLE, chunk_ptr, chunk_length);
    bluetooth->last_hal_status_tx = (uint8_t)status;

    if (status != HAL_OK)
    {
        __disable_irq();
        s_bluetooth_rt.tx_running = 0u;
        s_bluetooth_rt.tx_inflight_length = 0u;
        __enable_irq();

        bluetooth->uart_tx_fail_count++;
    }
}

static HAL_StatusTypeDef Bluetooth_TxEnqueue(const uint8_t *data, uint16_t length)
{
    app_bluetooth_state_t *bluetooth;
    uint16_t head_snapshot;
    uint16_t tail_snapshot;
    uint16_t free_snapshot;
    uint16_t write_index;
    uint16_t index;

    bluetooth = (app_bluetooth_state_t *)&g_app_state.bluetooth;

    if ((data == 0) || (length == 0u))
    {
        bluetooth->last_hal_status_tx = (uint8_t)HAL_ERROR;
        return HAL_ERROR;
    }

    if (length >= BLUETOOTH_TX_RING_SIZE)
    {
        bluetooth->uart_tx_fail_count++;
        bluetooth->last_hal_status_tx = (uint8_t)HAL_ERROR;
        return HAL_ERROR;
    }

    __disable_irq();
    head_snapshot = s_bluetooth_rt.tx_head;
    tail_snapshot = s_bluetooth_rt.tx_tail;
    free_snapshot = Bluetooth_TxRingFreeFrom(head_snapshot, tail_snapshot);
    __enable_irq();

    if (free_snapshot < length)
    {
        bluetooth->uart_tx_fail_count++;
        bluetooth->last_hal_status_tx = (uint8_t)HAL_BUSY;
        return HAL_BUSY;
    }

    write_index = head_snapshot;

    for (index = 0u; index < length; index++)
    {
        s_bluetooth_rt.tx_ring[write_index] = data[index];

        write_index = (uint16_t)(write_index + 1u);
        if (write_index >= BLUETOOTH_TX_RING_SIZE)
        {
            write_index = 0u;
        }
    }

    __disable_irq();
    s_bluetooth_rt.tx_head = write_index;
    __enable_irq();

    bluetooth->last_hal_status_tx = (uint8_t)HAL_OK;
    Bluetooth_TxKick();
    return HAL_OK;
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: APP_STATE.bluetooth slice 초기화                               */
/* -------------------------------------------------------------------------- */
static void Bluetooth_ResetAppStateSlice(app_bluetooth_state_t *bluetooth)
{
    if (bluetooth == 0)
    {
        return;
    }

    memset(bluetooth, 0, sizeof(*bluetooth));

    bluetooth->initialized            = false;
    bluetooth->uart_rx_running        = false;
    bluetooth->echo_enabled           = true;
    bluetooth->auto_ping_enabled      = false;

    bluetooth->last_update_ms         = 0u;
    bluetooth->last_rx_ms             = 0u;
    bluetooth->last_tx_ms             = 0u;
    bluetooth->last_auto_ping_ms      = 0u;

    bluetooth->rx_bytes               = 0u;
    bluetooth->tx_bytes               = 0u;
    bluetooth->rx_line_count          = 0u;
    bluetooth->tx_line_count          = 0u;
    bluetooth->rx_overflow_count      = 0u;
    bluetooth->uart_error_count       = 0u;
    bluetooth->uart_rearm_fail_count  = 0u;
    bluetooth->uart_tx_fail_count     = 0u;

    bluetooth->rx_ring_level          = 0u;
    bluetooth->rx_ring_high_watermark = 0u;

    bluetooth->last_hal_status_rx     = 0xFFu;
    bluetooth->last_hal_status_tx     = 0xFFu;
    bluetooth->last_hal_error         = 0u;
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: UART를 BC417 bring-up 기본값으로 다시 맞추기                   */
/*                                                                            */
/*  왜 필요한가?                                                               */
/*  - 사용자는 .ioc를 적극적으로 재생성한다.                                   */
/*  - 그러면 huart3 baud가 다시 다른 값으로 생성될 수 있다.                    */
/*  - BC417 bring-up 초기에는 "일단 9600 8N1" 로 맞춰 두는 편이                */
/*    실제 모듈과 처음 대화하기 쉽다.                                          */
/* -------------------------------------------------------------------------- */
static void Bluetooth_ApplyDefaultUartConfigIfRequested(void)
{
#if (BLUETOOTH_FORCE_UART_REINIT_ON_INIT != 0u)
    (void)HAL_UART_DeInit(&BLUETOOTH_UART_HANDLE);

    BLUETOOTH_UART_HANDLE.Init.BaudRate     = BLUETOOTH_DATA_BAUDRATE;
    BLUETOOTH_UART_HANDLE.Init.WordLength   = UART_WORDLENGTH_8B;
    BLUETOOTH_UART_HANDLE.Init.StopBits     = UART_STOPBITS_1;
    BLUETOOTH_UART_HANDLE.Init.Parity       = UART_PARITY_NONE;
    BLUETOOTH_UART_HANDLE.Init.Mode         = UART_MODE_TX_RX;
    BLUETOOTH_UART_HANDLE.Init.HwFlowCtl    = UART_HWCONTROL_NONE;
    BLUETOOTH_UART_HANDLE.Init.OverSampling = UART_OVERSAMPLING_16;

    (void)HAL_UART_Init(&BLUETOOTH_UART_HANDLE);
#endif
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: RX interrupt 시작 / 재시작                                     */
/* -------------------------------------------------------------------------- */
static void Bluetooth_StartRxIt(void)
{
    app_bluetooth_state_t *bluetooth;
    HAL_StatusTypeDef status;

    bluetooth = (app_bluetooth_state_t *)&g_app_state.bluetooth;

    status = HAL_UART_Receive_IT(&BLUETOOTH_UART_HANDLE,
                                 &s_bluetooth_rt.rx_byte_it,
                                 1u);

    bluetooth->last_hal_status_rx = (uint8_t)status;

    if (status == HAL_OK)
    {
        bluetooth->uart_rx_running = true;
    }
    else
    {
        bluetooth->uart_rx_running = false;
        bluetooth->uart_rearm_fail_count++;
    }
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: 대소문자 무시 문자열 비교                                      */
/*                                                                            */
/*  ECHO ON / ping / Info 같은 test command를                                  */
/*  사용자가 대문자/소문자 섞어서 보내도 쉽게 처리하려는 목적이다.              */
/* -------------------------------------------------------------------------- */
static uint8_t Bluetooth_TextEqualsIgnoreCase(const char *a, const char *b)
{
    if ((a == 0) || (b == 0))
    {
        return 0u;
    }

    while ((*a != '\0') && (*b != '\0'))
    {
        if (toupper((unsigned char)*a) != toupper((unsigned char)*b))
        {
            return 0u;
        }

        a++;
        b++;
    }

    return ((*a == '\0') && (*b == '\0')) ? 1u : 0u;
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: 마지막 TX 줄 snapshot 저장                                     */
/* -------------------------------------------------------------------------- */
static void Bluetooth_RecordQueuedTxLine(const char *text,
                                          uint16_t length,
                                          uint32_t now_ms)
{
    app_bluetooth_state_t *bluetooth;
    uint16_t copy_length;

    bluetooth = (app_bluetooth_state_t *)&g_app_state.bluetooth;

    memset(bluetooth->last_tx_line, 0, sizeof(bluetooth->last_tx_line));

    if ((text != 0) && (length > 0u))
    {
        copy_length = length;
        if (copy_length >= (uint16_t)sizeof(bluetooth->last_tx_line))
        {
            copy_length = (uint16_t)sizeof(bluetooth->last_tx_line) - 1u;
        }

        memcpy(bluetooth->last_tx_line, text, copy_length);
    }

    /* ---------------------------------------------------------------------- */
    /*  TX line count는 "UART로 실제 비트가 모두 나갔는가" 가 아니라             */
    /*  "상위 로직이 논리적 송신 요청을 하나 enqueue했는가" 를 기준으로 센다.    */
    /*                                                                        */
    /*  실제 byte drain 완료 시각/누적 byte는 TX complete callback이            */
    /*  별도로 갱신한다.                                                       */
    /* ---------------------------------------------------------------------- */
    bluetooth->last_update_ms = now_ms;
    bluetooth->tx_line_count++;
}


/* -------------------------------------------------------------------------- */
/*  내부 helper: 현재 조립 중인 RX preview를 APP_STATE에 반영                    */
/*                                                                            */
/*  아직 CR/LF가 오지 않았더라도,                                               */
/*  사용자가 디버그 페이지에서                                                  */
/*  "지금 어느 정도까지 들어왔는가" 를 볼 수 있게 하기 위한 preview다.          */
/* -------------------------------------------------------------------------- */
static void Bluetooth_UpdatePreviewText(app_bluetooth_state_t *bluetooth)
{
    if (bluetooth == 0)
    {
        return;
    }

    memset(bluetooth->rx_preview, 0, sizeof(bluetooth->rx_preview));

    if (s_bluetooth_rt.line_build_length == 0u)
    {
        return;
    }

    memcpy(bluetooth->rx_preview,
           s_bluetooth_rt.line_build,
           s_bluetooth_rt.line_build_length);
    bluetooth->rx_preview[s_bluetooth_rt.line_build_length] = '\0';
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: 한 줄 commit                                                   */
/*                                                                            */
/*  규칙                                                                       */
/*  - CR 또는 LF를 받으면 "한 줄 수신 완료" 로 본다.                           */
/*  - 마지막 완성 줄은 APP_STATE.last_rx_line에 남긴다.                         */
/*  - echo 명령/간단한 test command 처리는                                     */
/*    line commit 직후 main context에서 수행한다.                               */
/* -------------------------------------------------------------------------- */
static void Bluetooth_CommitRxLine(app_bluetooth_state_t *bluetooth,
                                   char *out_line,
                                   size_t out_line_size,
                                   uint32_t now_ms)
{
    uint16_t copy_length;

    if ((bluetooth == 0) || (out_line == 0) || (out_line_size == 0u))
    {
        return;
    }

    out_line[0] = '\0';

    if (s_bluetooth_rt.line_build_length == 0u)
    {
        Bluetooth_UpdatePreviewText(bluetooth);
        return;
    }

    copy_length = s_bluetooth_rt.line_build_length;
    if (copy_length >= (uint16_t)sizeof(bluetooth->last_rx_line))
    {
        copy_length = (uint16_t)sizeof(bluetooth->last_rx_line) - 1u;
    }

    memset(bluetooth->last_rx_line, 0, sizeof(bluetooth->last_rx_line));
    memcpy(bluetooth->last_rx_line, s_bluetooth_rt.line_build, copy_length);
    bluetooth->last_rx_line[copy_length] = '\0';

    if (copy_length >= (uint16_t)out_line_size)
    {
        copy_length = (uint16_t)out_line_size - 1u;
    }

    memcpy(out_line, s_bluetooth_rt.line_build, copy_length);
    out_line[copy_length] = '\0';

    bluetooth->rx_line_count++;
    bluetooth->last_update_ms = now_ms;

    memset(s_bluetooth_rt.line_build, 0, sizeof(s_bluetooth_rt.line_build));
    s_bluetooth_rt.line_build_length = 0u;
    Bluetooth_UpdatePreviewText(bluetooth);
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: 현재 상태 요약 한 줄 송신                                      */
/* -------------------------------------------------------------------------- */
static void Bluetooth_SendInfoLine(void)
{
    app_bluetooth_state_t *bluetooth;

    bluetooth = (app_bluetooth_state_t *)&g_app_state.bluetooth;

    (void)Bluetooth_SendPrintf("INFO RX=%lu TX=%lu RXL=%lu TXL=%lu E=%u A=%u\r\n",
                               (unsigned long)bluetooth->rx_bytes,
                               (unsigned long)bluetooth->tx_bytes,
                               (unsigned long)bluetooth->rx_line_count,
                               (unsigned long)bluetooth->tx_line_count,
                               bluetooth->echo_enabled ? 1u : 0u,
                               bluetooth->auto_ping_enabled ? 1u : 0u);
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: 수신 줄 해석                                                   */
/*                                                                            */
/*  테스트가 쉬워지도록 아주 단순한 명령 몇 개를 넣어 둔다.                     */
/*                                                                            */
/*  사용 예                                                                    */
/*    PING      -> PONG                                                        */
/*    HELLO     -> HELLO FROM STM32                                            */
/*    INFO      -> 현재 카운터/설정 한 줄                                       */
/*    ECHO ON   -> echo 기능 켬                                                */
/*    ECHO OFF  -> echo 기능 끔                                                */
/*    AUTO ON   -> auto ping 켬                                                */
/*    AUTO OFF  -> auto ping 끔                                                */
/*    NMEA      -> 데모 vendor sentence 송신                                   */
/*                                                                            */
/*  그 외의 문자열은 echo가 켜져 있으면 ECHO:<text> 형태로 되돌려 준다.         */
/* -------------------------------------------------------------------------- */
static void Bluetooth_HandleReceivedLine(const char *line_text)
{
    app_bluetooth_state_t *bluetooth;

    if (line_text == 0)
    {
        return;
    }

    bluetooth = (app_bluetooth_state_t *)&g_app_state.bluetooth;

    if (Bluetooth_TextEqualsIgnoreCase(line_text, "PING") != 0u)
    {
        (void)Bluetooth_SendLine("PONG");
        return;
    }

    if (Bluetooth_TextEqualsIgnoreCase(line_text, "HELLO") != 0u)
    {
        (void)Bluetooth_SendLine("HELLO FROM STM32");
        return;
    }

    if (Bluetooth_TextEqualsIgnoreCase(line_text, "INFO") != 0u)
    {
        Bluetooth_SendInfoLine();
        return;
    }

    if (Bluetooth_TextEqualsIgnoreCase(line_text, "ECHO ON") != 0u)
    {
        Bluetooth_SetEchoEnabled(true);
        (void)Bluetooth_SendLine("OK ECHO ON");
        return;
    }

    if (Bluetooth_TextEqualsIgnoreCase(line_text, "ECHO OFF") != 0u)
    {
        Bluetooth_SetEchoEnabled(false);
        (void)Bluetooth_SendLine("OK ECHO OFF");
        return;
    }

    if (Bluetooth_TextEqualsIgnoreCase(line_text, "AUTO ON") != 0u)
    {
        Bluetooth_SetAutoPingEnabled(true);
        (void)Bluetooth_SendLine("OK AUTO ON");
        return;
    }

    if (Bluetooth_TextEqualsIgnoreCase(line_text, "AUTO OFF") != 0u)
    {
        Bluetooth_SetAutoPingEnabled(false);
        (void)Bluetooth_SendLine("OK AUTO OFF");
        return;
    }

    if (Bluetooth_TextEqualsIgnoreCase(line_text, "NMEA") != 0u)
    {
        Bluetooth_DoSomethingDemoNmea();
        return;
    }

    if (bluetooth->echo_enabled != false)
    {
        (void)Bluetooth_SendPrintf("ECHO:%s\r\n", line_text);
    }
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: NMEA checksum                                                  */
/* -------------------------------------------------------------------------- */
static uint8_t Bluetooth_CalcNmeaChecksum(const char *body_text)
{
    uint8_t checksum;

    checksum = 0u;

    if (body_text == 0)
    {
        return checksum;
    }

    while (*body_text != '\0')
    {
        checksum ^= (uint8_t)(*body_text);
        body_text++;
    }

    return checksum;
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: raw 바이트 1개를 line parser에 먹이기                           */
/* -------------------------------------------------------------------------- */
static void Bluetooth_ProcessReceivedByte(uint8_t byte, uint32_t now_ms)
{
    app_bluetooth_state_t *bluetooth;
    char committed_line[BLUETOOTH_LINE_TEXT_MAX];

    bluetooth = (app_bluetooth_state_t *)&g_app_state.bluetooth;

    /* ---------------------------------------------------------------------- */
    /*  줄 끝(CR/LF)이 들어오면 지금까지 모은 텍스트를 한 줄로 commit 한다.     */
    /* ---------------------------------------------------------------------- */
    if ((byte == '\r') || (byte == '\n'))
    {
        Bluetooth_CommitRxLine(bluetooth,
                               committed_line,
                               sizeof(committed_line),
                               now_ms);

        if (committed_line[0] != '\0')
        {
#if (BLUETOOTH_MIRROR_TO_DEBUG_UART != 0u)
            (void)DEBUG_UART_Printf("[BT RX] %s\r\n", committed_line);
#endif
            Bluetooth_HandleReceivedLine(committed_line);
        }
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  일반 인쇄 가능 ASCII를 우선 가정한다.                                   */
    /*  비인쇄 문자는 '.' 로 치환해서 preview/last line이 깨지지 않게 한다.      */
    /* ---------------------------------------------------------------------- */
    if (s_bluetooth_rt.line_build_length < (BLUETOOTH_LINE_TEXT_MAX - 1u))
    {
        if ((byte >= 32u) && (byte <= 126u))
        {
            s_bluetooth_rt.line_build[s_bluetooth_rt.line_build_length++] = (char)byte;
        }
        else
        {
            s_bluetooth_rt.line_build[s_bluetooth_rt.line_build_length++] = '.';
        }

        s_bluetooth_rt.line_build[s_bluetooth_rt.line_build_length] = '\0';
    }
    else
    {
        /* ------------------------------------------------------------------ */
        /*  line buffer가 꽉 찼다.                                              */
        /*  문자열 전체를 잃어버리기보다                                         */
        /*  현재 줄을 강제로 commit하고 overflow count를 올린다.                */
        /* ------------------------------------------------------------------ */
        bluetooth->rx_overflow_count++;

        Bluetooth_CommitRxLine(bluetooth,
                               committed_line,
                               sizeof(committed_line),
                               now_ms);

        if (committed_line[0] != '\0')
        {
#if (BLUETOOTH_MIRROR_TO_DEBUG_UART != 0u)
            (void)DEBUG_UART_Printf("[BT RX] %s\r\n", committed_line);
#endif
            Bluetooth_HandleReceivedLine(committed_line);
        }
    }

    Bluetooth_UpdatePreviewText(bluetooth);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: init                                                              */
/* -------------------------------------------------------------------------- */
void Bluetooth_Init(void)
{
    app_bluetooth_state_t *bluetooth;

    bluetooth = (app_bluetooth_state_t *)&g_app_state.bluetooth;

    memset(&s_bluetooth_rt, 0, sizeof(s_bluetooth_rt));
    Bluetooth_ResetAppStateSlice(bluetooth);
    Bluetooth_RxRingReset();
    Bluetooth_TxRingReset();
    Bluetooth_ApplyDefaultUartConfigIfRequested();

    bluetooth->initialized = true;
    bluetooth->last_hal_status_rx = (uint8_t)HAL_OK;
    bluetooth->last_hal_status_tx = (uint8_t)HAL_OK;

    s_bluetooth_rt.next_auto_ping_ms = HAL_GetTick() + BLUETOOTH_AUTO_PING_PERIOD_MS;
    Bluetooth_StartRxIt();

#if (BLUETOOTH_MIRROR_TO_DEBUG_UART != 0u)
    (void)DEBUG_UART_Printf("[BT] init baud=%lu\r\n",
                            (unsigned long)BLUETOOTH_UART_HANDLE.Init.BaudRate);
#endif
}

/* -------------------------------------------------------------------------- */
/*  공개 API: raw byte 송신                                                     */
/*                                                                            */
/*  현재 구현은                                                               */
/*  - HAL_UART_Transmit() blocking 방식 대신                                  */
/*  - software TX ring + HAL_UART_Transmit_IT() interrupt drain 방식으로       */
/*    동작한다.                                                               */
/*                                                                            */
/*  즉, 호출자는                                                              */
/*    1) 데이터를 queue에 복사하고                                            */
/*    2) 첫 chunk kick만 시도한 뒤                                            */
/*  곧바로 리턴하므로, main loop가 Bluetooth TX 때문에 길게 붙잡히지 않는다.  */
/* -------------------------------------------------------------------------- */
HAL_StatusTypeDef Bluetooth_SendBytes(const uint8_t *data, uint16_t length)
{
    app_bluetooth_state_t *bluetooth;
    HAL_StatusTypeDef status;
    uint32_t now_ms;

    bluetooth = (app_bluetooth_state_t *)&g_app_state.bluetooth;

    if ((data == 0) || (length == 0u))
    {
        bluetooth->last_hal_status_tx = (uint8_t)HAL_ERROR;
        return HAL_ERROR;
    }

    if (bluetooth->initialized == false)
    {
        bluetooth->last_hal_status_tx = (uint8_t)HAL_ERROR;
        return HAL_ERROR;
    }

    status = Bluetooth_TxEnqueue(data, length);
    if (status == HAL_OK)
    {
        now_ms = HAL_GetTick();
        Bluetooth_RecordQueuedTxLine((const char *)data, length, now_ms);
    }

    return status;
}


/* -------------------------------------------------------------------------- */
/*  공개 API: 문자열 그대로 송신                                                */
/* -------------------------------------------------------------------------- */
HAL_StatusTypeDef Bluetooth_SendString(const char *text)
{
    size_t length;

    if (text == 0)
    {
        return HAL_ERROR;
    }

    length = strlen(text);
    if (length == 0u)
    {
        return HAL_OK;
    }

    if (length > 65535u)
    {
        length = 65535u;
    }

#if (BLUETOOTH_MIRROR_TO_DEBUG_UART != 0u)
    (void)DEBUG_UART_Printf("[BT TX] %s\r\n", text);
#endif

    return Bluetooth_SendBytes((const uint8_t *)text, (uint16_t)length);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 한 줄 송신                                                        */
/* -------------------------------------------------------------------------- */
HAL_StatusTypeDef Bluetooth_SendLine(const char *text)
{
    char line_buffer[192];
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

#if (BLUETOOTH_MIRROR_TO_DEBUG_UART != 0u)
    (void)DEBUG_UART_Printf("[BT TX] %s\r\n", text);
#endif

    return Bluetooth_SendBytes((const uint8_t *)line_buffer, (uint16_t)written);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: printf 스타일 송신                                                */
/* -------------------------------------------------------------------------- */
HAL_StatusTypeDef Bluetooth_SendPrintf(const char *fmt, ...)
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

#if (BLUETOOTH_MIRROR_TO_DEBUG_UART != 0u)
    (void)DEBUG_UART_Printf("[BT TX] %s\r\n", text_buffer);
#endif

    return Bluetooth_SendBytes((const uint8_t *)text_buffer, (uint16_t)written);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: NMEA body -> 완전한 한 줄 송신                                    */
/* -------------------------------------------------------------------------- */
HAL_StatusTypeDef Bluetooth_SendNmeaSentenceBody(const char *body_text)
{
    char sentence[192];
    uint8_t checksum;
    int written;

    if (body_text == 0)
    {
        return HAL_ERROR;
    }

    checksum = Bluetooth_CalcNmeaChecksum(body_text);

    written = snprintf(sentence,
                       sizeof(sentence),
                       "$%s*%02X\r\n",
                       body_text,
                       (unsigned)checksum);
    if ((written <= 0) || ((size_t)written >= sizeof(sentence)))
    {
        return HAL_ERROR;
    }

#if (BLUETOOTH_MIRROR_TO_DEBUG_UART != 0u)
    (void)DEBUG_UART_Printf("[BT TX] $%s*%02X\r\n",
                            body_text,
                            (unsigned)checksum);
#endif

    return Bluetooth_SendBytes((const uint8_t *)sentence, (uint16_t)written);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: echo / auto ping 제어                                             */
/* -------------------------------------------------------------------------- */
void Bluetooth_SetEchoEnabled(bool enable)
{
    ((app_bluetooth_state_t *)&g_app_state.bluetooth)->echo_enabled = enable;
}

void Bluetooth_ToggleEcho(void)
{
    app_bluetooth_state_t *bluetooth;

    bluetooth = (app_bluetooth_state_t *)&g_app_state.bluetooth;
    bluetooth->echo_enabled = (bluetooth->echo_enabled == false) ? true : false;
}

void Bluetooth_SetAutoPingEnabled(bool enable)
{
    app_bluetooth_state_t *bluetooth;

    bluetooth = (app_bluetooth_state_t *)&g_app_state.bluetooth;
    bluetooth->auto_ping_enabled = enable;

    if (enable != false)
    {
        s_bluetooth_rt.next_auto_ping_ms = HAL_GetTick() + BLUETOOTH_AUTO_PING_PERIOD_MS;
    }
}

void Bluetooth_ToggleAutoPing(void)
{
    app_bluetooth_state_t *bluetooth;

    bluetooth = (app_bluetooth_state_t *)&g_app_state.bluetooth;
    Bluetooth_SetAutoPingEnabled((bluetooth->auto_ping_enabled == false) ? true : false);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: snapshot 복사 helper                                              */
/* -------------------------------------------------------------------------- */
void Bluetooth_CopySnapshot(app_bluetooth_state_t *out_snapshot)
{
    APP_STATE_CopyBluetoothSnapshot(out_snapshot);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: DoSomething 계열                                                  */
/* -------------------------------------------------------------------------- */
void Bluetooth_DoSomething(void)
{
    Bluetooth_DoSomethingHello();
}

void Bluetooth_DoSomethingPing(void)
{
    (void)Bluetooth_SendLine("PING");
}

void Bluetooth_DoSomethingHello(void)
{
    (void)Bluetooth_SendLine("HELLO FROM STM32");
}

void Bluetooth_DoSomethingInfo(void)
{
    Bluetooth_SendInfoLine();
}

void Bluetooth_DoSomethingDemoNmea(void)
{
    (void)Bluetooth_SendNmeaSentenceBody("PTSTM,HELLO,1");
}

/* -------------------------------------------------------------------------- */
/*  공개 API: UART RX complete callback                                         */
/*                                                                            */
/*  이 경로에서는 긴 처리 금지.                                                 */
/*  - raw byte 카운터 증가                                                     */
/*  - ring 적재                                                                 */
/*  - 다음 1바이트 수신 재arm                                                  */
/*  세 가지만 수행한다.                                                        */
/* -------------------------------------------------------------------------- */
void Bluetooth_OnUartRxCplt(UART_HandleTypeDef *huart)
{
    app_bluetooth_state_t *bluetooth;
    uint16_t ring_level;

    if (huart == 0)
    {
        return;
    }

    if (huart->Instance != BLUETOOTH_UART_HANDLE.Instance)
    {
        return;
    }

    bluetooth = (app_bluetooth_state_t *)&g_app_state.bluetooth;

    bluetooth->rx_bytes++;
    bluetooth->last_rx_ms     = HAL_GetTick();
    bluetooth->last_update_ms = bluetooth->last_rx_ms;
    bluetooth->last_hal_status_rx = (uint8_t)HAL_OK;

    ring_level = Bluetooth_RxRingPushIsr(s_bluetooth_rt.rx_byte_it);
    if (ring_level == 0xFFFFu)
    {
        bluetooth->rx_overflow_count++;
        bluetooth->rx_ring_level = Bluetooth_RxRingLevel();
    }
    else
    {
        bluetooth->rx_ring_level = ring_level;
        if (ring_level > bluetooth->rx_ring_high_watermark)
        {
            bluetooth->rx_ring_high_watermark = ring_level;
        }
    }

    Bluetooth_StartRxIt();
}

/* -------------------------------------------------------------------------- */
/*  공개 API: UART TX complete callback                                         */
/*                                                                            */
/*  HAL_UART_Transmit_IT() 가 한 번 끝날 때마다                                 */
/*    1) 이번 chunk 길이만큼 tail을 전진시키고                                  */
/*    2) 누적 byte / 마지막 완료 시각을 갱신한 뒤                               */
/*    3) 남은 queue가 있으면 다음 contiguous chunk를 즉시 kick 한다.            */
/* -------------------------------------------------------------------------- */
void Bluetooth_OnUartTxCplt(UART_HandleTypeDef *huart)
{
    app_bluetooth_state_t *bluetooth;
    uint16_t tail_snapshot;
    uint16_t inflight_snapshot;
    uint16_t next_tail;
    uint8_t need_kick;

    if (huart == 0)
    {
        return;
    }

    if (huart->Instance != BLUETOOTH_UART_HANDLE.Instance)
    {
        return;
    }

    bluetooth = (app_bluetooth_state_t *)&g_app_state.bluetooth;

    __disable_irq();
    tail_snapshot     = s_bluetooth_rt.tx_tail;
    inflight_snapshot = s_bluetooth_rt.tx_inflight_length;

    next_tail = (uint16_t)(tail_snapshot + inflight_snapshot);
    if (next_tail >= BLUETOOTH_TX_RING_SIZE)
    {
        next_tail = (uint16_t)(next_tail - BLUETOOTH_TX_RING_SIZE);
    }

    s_bluetooth_rt.tx_tail = next_tail;
    s_bluetooth_rt.tx_inflight_length = 0u;
    s_bluetooth_rt.tx_running = 0u;
    need_kick = (s_bluetooth_rt.tx_head != s_bluetooth_rt.tx_tail) ? 1u : 0u;
    __enable_irq();

    bluetooth->last_hal_status_tx = (uint8_t)HAL_OK;
    bluetooth->last_tx_ms = HAL_GetTick();
    bluetooth->last_update_ms = bluetooth->last_tx_ms;
    bluetooth->tx_bytes += inflight_snapshot;

    if (need_kick != 0u)
    {
        Bluetooth_TxKick();
    }
}

/* -------------------------------------------------------------------------- */
/*  공개 API: UART error callback                                               */
/* -------------------------------------------------------------------------- */
void Bluetooth_OnUartError(UART_HandleTypeDef *huart)
{
    app_bluetooth_state_t *bluetooth;

    if (huart == 0)
    {
        return;
    }

    if (huart->Instance != BLUETOOTH_UART_HANDLE.Instance)
    {
        return;
    }

    bluetooth = (app_bluetooth_state_t *)&g_app_state.bluetooth;

    bluetooth->uart_error_count++;
    bluetooth->last_hal_error = (uint8_t)(huart->ErrorCode & 0xFFu);
    bluetooth->uart_rx_running = false;

    /* ---------------------------------------------------------------------- */
    /*  HAL 상태 머신이 error 후 busy 상태로 남을 수 있으므로                   */
    /*  수신 경로를 한 번 정리한 뒤 다시 Receive_IT를 시작한다.                 */
    /* ---------------------------------------------------------------------- */
    (void)HAL_UART_AbortReceive(&BLUETOOTH_UART_HANDLE);
    Bluetooth_StartRxIt();
}

/* -------------------------------------------------------------------------- */
/*  공개 API: main loop task                                                    */
/* -------------------------------------------------------------------------- */
void Bluetooth_Task(uint32_t now_ms)
{
    app_bluetooth_state_t *bluetooth;
    uint32_t processed;
    uint8_t byte;

    bluetooth = (app_bluetooth_state_t *)&g_app_state.bluetooth;

    if (bluetooth->initialized == false)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  1) ISR가 적재한 RX byte를 line parser에 먹인다.                        */
    /* ---------------------------------------------------------------------- */
    processed = 0u;
    while (processed < BLUETOOTH_RX_PARSE_BUDGET)
    {
        if (Bluetooth_RxRingPop(&byte) == 0u)
        {
            break;
        }

        Bluetooth_ProcessReceivedByte(byte, now_ms);
        processed++;
    }

    bluetooth->rx_ring_level = Bluetooth_RxRingLevel();

    /* ---------------------------------------------------------------------- */
    /*  2) auto ping 기능                                                      */
    /*                                                                        */
    /*  bring-up 때 "상대 단말이 수신하고 있는가" 를 보기 위해                  */
    /*  1초 주기의 반복 송신을 쉽게 켜고 끌 수 있게 했다.                      */
    /* ---------------------------------------------------------------------- */
    if ((bluetooth->auto_ping_enabled != false) &&
        (Bluetooth_TimeDue(now_ms, s_bluetooth_rt.next_auto_ping_ms) != 0u))
    {
        s_bluetooth_rt.auto_ping_sequence++;

        (void)Bluetooth_SendPrintf("AUTO PING %lu\r\n",
                                   (unsigned long)s_bluetooth_rt.auto_ping_sequence);

        bluetooth->last_auto_ping_ms = now_ms;
        s_bluetooth_rt.next_auto_ping_ms = now_ms + BLUETOOTH_AUTO_PING_PERIOD_MS;
    }

    /* ---------------------------------------------------------------------- */
    /*  3) 혹시 RX가 죽어 있으면 main loop가 다시 살려 준다.                    */
    /* ---------------------------------------------------------------------- */
    if (bluetooth->uart_rx_running == false)
    {
        Bluetooth_StartRxIt();
    }

    /* ---------------------------------------------------------------------- */
    /*  4) queue에 남아 있는데도 TX가 idle이면 main loop가 한 번 더 kick 한다. */
    /*                                                                        */
    /*  보통은 enqueue 시점이나 TX complete ISR가 다음 chunk를 바로 시작한다.   */
    /*  하지만 HAL busy/error 후 tx_running 플래그가 내려간 채 멈춰 있으면      */
    /*  이 경로가 다음 loop에서 다시 살려 준다.                               */
    /* ---------------------------------------------------------------------- */
    Bluetooth_TxKick();
}
