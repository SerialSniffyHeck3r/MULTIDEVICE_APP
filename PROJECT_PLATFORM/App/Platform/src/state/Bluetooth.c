#include "Bluetooth.h"
#include "APP_MEMORY_SECTIONS.h"
#include "DEBUG_UART.h"
#include "usart.h"

#include <ctype.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*  ?몃? UART handle                                                           */
/* -------------------------------------------------------------------------- */
extern UART_HandleTypeDef BLUETOOTH_UART_HANDLE;

/* -------------------------------------------------------------------------- */
/*  ?쒕씪?대쾭 ?대? runtime ?곹깭                                                  */
/*                                                                            */
/*  ?먯튃                                                                       */
/*  - ISR??理쒕???吏㏐쾶 ?앸궦??                                                */
/*  - ISR?먯꽌??"諛붿씠?몃? ring???곸옱 + ?ㅼ쓬 1諛붿씠???섏떊 ?촡rm" 留??쒕떎.      */
/*  - 以?line) ?뚯떛, echo ?묐떟, auto ping, 紐낅졊 ?댁꽍? 紐⑤몢 main context??     */
/*    Bluetooth_Task() 媛 ?섑뻾?쒕떎.                                            */
/* -------------------------------------------------------------------------- */

typedef struct
{
    volatile uint16_t rx_head;                     /* ISR媛 ?곕뒗 RX ring head                 */
    volatile uint16_t rx_tail;                     /* main???뚮퉬?섎뒗 RX ring tail            */
    uint8_t  rx_ring[BLUETOOTH_RX_RING_SIZE];      /* ISR -> main ?⑤갑??SPSC RX ring         */

    /* ---------------------------------------------------------------------- */
    /*  TX ring? main -> ISR ?⑤갑??SPSC 援ъ“瑜??ъ슜?쒕떎.                     */
    /*                                                                        */
    /*  main context??head留??꾩쭊?쒗궎怨?enqueue留??섑뻾?쒕떎.                   */
    /*  UART TX complete ISR??tail留??꾩쭊?쒗궎怨??ㅼ쓬 chunk留?kick?쒕떎.        */
    /*                                                                        */
    /*  ?대젃寃??섎㈃ Bluetooth_SendLine/Printf媛 ???댁긽 blocking transmit??    */
    /*  臾띠씠吏 ?딄퀬, 吏㏃? 硫붾え由?蹂듭궗 + kick ?쒕룄留??섍퀬 怨㏓컮濡?由ы꽩?쒕떎.      */
    /* ---------------------------------------------------------------------- */
    volatile uint16_t tx_head;                     /* main???곕뒗 TX ring head               */
    volatile uint16_t tx_tail;                     /* ISR媛 ?뚮퉬?섎뒗 TX ring tail            */
    uint8_t  tx_ring[BLUETOOTH_TX_RING_SIZE];      /* main -> ISR ?⑤갑??SPSC TX ring        */
    volatile uint16_t tx_inflight_length;          /* ?꾩옱 HAL_UART_Transmit_IT 湲몄씠         */
    volatile uint8_t  tx_running;                  /* ?꾩옱 UART TX interrupt媛 ?댁븘 ?덈뒗媛    */

    uint8_t  rx_byte_it;                           /* HAL_UART_Receive_IT 1-byte buffer       */

    char     line_build[BLUETOOTH_LINE_TEXT_MAX];  /* ?꾩옱 議곕┰ 以묒씤 ??以?                    */
    uint16_t line_build_length;                    /* ?꾩옱 以꾩쓽 ?좏슚 湲몄씠                      */

    uint32_t next_auto_ping_ms;                    /* ?ㅼ쓬 auto ping due ?쒓컖                  */
    uint32_t auto_ping_sequence;                   /* auto ping 踰덊샇                           */
} bluetooth_runtime_t;

static bluetooth_runtime_t s_bluetooth_rt APP_CCMRAM_BSS;

/* -------------------------------------------------------------------------- */
/*  ?쒓컙 helper                                                                */
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
/*  ?ㅺ퀎 ?ъ씤??                                                               */
/*  - producer(main)??queue??蹂듭궗留??섍퀬 利됱떆 由ы꽩?쒕떎.                      */
/*  - consumer(ISR)??HAL_UART_Transmit_IT ?꾨즺 ?쒖젏留덈떎 tail留??꾩쭊?쒗궓??    */
/*  - head??"?곗씠?곌? ?꾩쟾??蹂듭궗???? 留덉?留됱뿉 媛깆떊?댁꽌, ISR媛 遺遺?蹂듭궗?? */
/*    諛붿씠?몃? ?섎せ ?쎌? ?딄쾶 ?쒕떎.                                            */
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
/*  ?대? helper: APP_STATE.bluetooth slice 珥덇린??                              */
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
/*  ?대? helper: UART瑜?BC417 bring-up 湲곕낯媛믪쑝濡??ㅼ떆 留욎텛湲?                  */
/*                                                                            */
/*  ???꾩슂?쒓??                                                               */
/*  - ?ъ슜?먮뒗 .ioc瑜??곴레?곸쑝濡??ъ깮?깊븳??                                   */
/*  - 洹몃윭硫?huart3 baud媛 ?ㅼ떆 ?ㅻⅨ 媛믪쑝濡??앹꽦?????덈떎.                    */
/*  - BC417 bring-up 珥덇린?먮뒗 "?쇰떒 9600 8N1" 濡?留욎떠 ?먮뒗 ?몄씠                */
/*    ?ㅼ젣 紐⑤뱢怨?泥섏쓬 ??뷀븯湲??쎈떎.                                          */
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
/*  ?대? helper: RX interrupt ?쒖옉 / ?ъ떆??                                    */
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
/*  ?대? helper: ??뚮Ц??臾댁떆 臾몄옄??鍮꾧탳                                      */
/*                                                                            */
/*  ECHO ON / ping / Info 媛숈? test command瑜?                                 */
/*  ?ъ슜?먭? ?臾몄옄/?뚮Ц???욎뼱??蹂대궡???쎄쾶 泥섎━?섎젮??紐⑹쟻?대떎.              */
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
/*  ?대? helper: 留덉?留?TX 以?snapshot ???                                    */
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
    /*  TX line count??"UART濡??ㅼ젣 鍮꾪듃媛 紐⑤몢 ?섍컮?붽?" 媛 ?꾨땲??            */
    /*  "?곸쐞 濡쒖쭅???쇰━???≪떊 ?붿껌???섎굹 enqueue?덈뒗媛" 瑜?湲곗??쇰줈 ?쇰떎.    */
    /*                                                                        */
    /*  ?ㅼ젣 byte drain ?꾨즺 ?쒓컖/?꾩쟻 byte??TX complete callback??           */
    /*  蹂꾨룄濡?媛깆떊?쒕떎.                                                       */
    /* ---------------------------------------------------------------------- */
    bluetooth->last_update_ms = now_ms;
    bluetooth->tx_line_count++;
}


/* -------------------------------------------------------------------------- */
/*  ?대? helper: ?꾩옱 議곕┰ 以묒씤 RX preview瑜?APP_STATE??諛섏쁺                    */
/*                                                                            */
/*  ?꾩쭅 CR/LF媛 ?ㅼ? ?딆븯?붾씪??                                               */
/*  ?ъ슜?먭? ?붾쾭洹??섏씠吏?먯꽌                                                  */
/*  "吏湲??대뒓 ?뺣룄源뚯? ?ㅼ뼱?붾뒗媛" 瑜?蹂????덇쾶 ?섍린 ?꾪븳 preview??          */
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
/*  ?대? helper: ??以?commit                                                   */
/*                                                                            */
/*  洹쒖튃                                                                       */
/*  - CR ?먮뒗 LF瑜?諛쏆쑝硫?"??以??섏떊 ?꾨즺" 濡?蹂몃떎.                           */
/*  - 留덉?留??꾩꽦 以꾩? APP_STATE.last_rx_line???④릿??                         */
/*  - echo 紐낅졊/媛꾨떒??test command 泥섎━??                                    */
/*    line commit 吏곹썑 main context?먯꽌 ?섑뻾?쒕떎.                               */
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
/*  ?대? helper: ?꾩옱 ?곹깭 ?붿빟 ??以??≪떊                                      */
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
/*  ?대? helper: ?섏떊 以??댁꽍                                                   */
/*                                                                            */
/*  ?뚯뒪?멸? ?ъ썙吏?꾨줉 ?꾩＜ ?⑥닚??紐낅졊 紐?媛쒕? ?ｌ뼱 ?붾떎.                     */
/*                                                                            */
/*  ?ъ슜 ??                                                                   */
/*    PING      -> PONG                                                        */
/*    HELLO     -> HELLO FROM STM32                                            */
/*    INFO      -> ?꾩옱 移댁슫???ㅼ젙 ??以?                                      */
/*    ECHO ON   -> echo 湲곕뒫 耳?                                               */
/*    ECHO OFF  -> echo 湲곕뒫 ??                                               */
/*    AUTO ON   -> auto ping 耳?                                               */
/*    AUTO OFF  -> auto ping ??                                               */
/*    NMEA      -> ?곕え vendor sentence ?≪떊                                   */
/*                                                                            */
/*  洹??몄쓽 臾몄옄?댁? echo媛 耳쒖졇 ?덉쑝硫?ECHO:<text> ?뺥깭濡??섎룎??以??         */
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
/*  ?대? helper: NMEA checksum                                                  */
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
/*  ?대? helper: raw 諛붿씠??1媛쒕? line parser??癒뱀씠湲?                          */
/* -------------------------------------------------------------------------- */
static void Bluetooth_ProcessReceivedByte(uint8_t byte, uint32_t now_ms)
{
    app_bluetooth_state_t *bluetooth;
    char committed_line[BLUETOOTH_LINE_TEXT_MAX];

    bluetooth = (app_bluetooth_state_t *)&g_app_state.bluetooth;

    /* ---------------------------------------------------------------------- */
    /*  以???CR/LF)???ㅼ뼱?ㅻ㈃ 吏湲덇퉴吏 紐⑥? ?띿뒪?몃? ??以꾨줈 commit ?쒕떎.     */
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
    /*  ?쇰컲 ?몄뇙 媛??ASCII瑜??곗꽑 媛?뺥븳??                                   */
    /*  鍮꾩씤??臾몄옄??'.' 濡?移섑솚?댁꽌 preview/last line??源⑥?吏 ?딄쾶 ?쒕떎.      */
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
        /*  line buffer媛 苑?李쇰떎.                                              */
        /*  臾몄옄???꾩껜瑜??껋뼱踰꾨━湲곕낫??                                        */
        /*  ?꾩옱 以꾩쓣 媛뺤젣濡?commit?섍퀬 overflow count瑜??щ┛??                */
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
/*  怨듦컻 API: init                                                              */
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
/*  怨듦컻 API: raw byte ?≪떊                                                     */
/*                                                                            */
/*  ?꾩옱 援ы쁽?                                                               */
/*  - HAL_UART_Transmit() blocking 諛⑹떇 ???                                 */
/*  - software TX ring + HAL_UART_Transmit_IT() interrupt drain 諛⑹떇?쇰줈       */
/*    ?숈옉?쒕떎.                                                               */
/*                                                                            */
/*  利? ?몄텧?먮뒗                                                              */
/*    1) ?곗씠?곕? queue??蹂듭궗?섍퀬                                            */
/*    2) 泥?chunk kick留??쒕룄????                                           */
/*  怨㏓컮濡?由ы꽩?섎?濡? main loop媛 Bluetooth TX ?뚮Ц??湲멸쾶 遺숈옟?덉? ?딅뒗??  */
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
/*  怨듦컻 API: 臾몄옄??洹몃?濡??≪떊                                                */
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
/*  怨듦컻 API: ??以??≪떊                                                        */
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
/*  怨듦컻 API: printf ?ㅽ????≪떊                                                */
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
/*  怨듦컻 API: NMEA body -> ?꾩쟾????以??≪떊                                    */
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
/*  怨듦컻 API: echo / auto ping ?쒖뼱                                             */
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
/*  怨듦컻 API: snapshot 蹂듭궗 helper                                              */
/* -------------------------------------------------------------------------- */
void Bluetooth_CopySnapshot(app_bluetooth_state_t *out_snapshot)
{
    APP_STATE_CopyBluetoothSnapshot(out_snapshot);
}

/* -------------------------------------------------------------------------- */
/*  怨듦컻 API: DoSomething 怨꾩뿴                                                  */
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
/*  怨듦컻 API: UART RX complete callback                                         */
/*                                                                            */
/*  ??寃쎈줈?먯꽌??湲?泥섎━ 湲덉?.                                                 */
/*  - raw byte 移댁슫??利앷?                                                     */
/*  - ring ?곸옱                                                                 */
/*  - ?ㅼ쓬 1諛붿씠???섏떊 ?촡rm                                                  */
/*  ??媛吏留??섑뻾?쒕떎.                                                        */
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
/*  怨듦컻 API: UART TX complete callback                                         */
/*                                                                            */
/*  HAL_UART_Transmit_IT() 媛 ??踰??앸궇 ?뚮쭏??                                */
/*    1) ?대쾲 chunk 湲몄씠留뚰겮 tail???꾩쭊?쒗궎怨?                                 */
/*    2) ?꾩쟻 byte / 留덉?留??꾨즺 ?쒓컖??媛깆떊????                              */
/*    3) ?⑥? queue媛 ?덉쑝硫??ㅼ쓬 contiguous chunk瑜?利됱떆 kick ?쒕떎.            */
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
/*  怨듦컻 API: UART error callback                                               */
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
    /*  HAL ?곹깭 癒몄떊??error ??busy ?곹깭濡??⑥쓣 ???덉쑝誘濡?                  */
    /*  ?섏떊 寃쎈줈瑜???踰??뺣━?????ㅼ떆 Receive_IT瑜??쒖옉?쒕떎.                 */
    /* ---------------------------------------------------------------------- */
    (void)HAL_UART_AbortReceive(&BLUETOOTH_UART_HANDLE);
    Bluetooth_StartRxIt();
}

/* -------------------------------------------------------------------------- */
/*  怨듦컻 API: main loop task                                                    */
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
    /*  1) ISR媛 ?곸옱??RX byte瑜?line parser??癒뱀씤??                        */
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
    /*  2) auto ping 湲곕뒫                                                      */
    /*                                                                        */
    /*  bring-up ??"?곷? ?⑤쭚???섏떊?섍퀬 ?덈뒗媛" 瑜?蹂닿린 ?꾪빐                  */
    /*  1珥?二쇨린??諛섎났 ?≪떊???쎄쾶 耳쒓퀬 ?????덇쾶 ?덈떎.                      */
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
    /*  3) ?뱀떆 RX媛 二쎌뼱 ?덉쑝硫?main loop媛 ?ㅼ떆 ?대젮 以??                    */
    /* ---------------------------------------------------------------------- */
    if (bluetooth->uart_rx_running == false)
    {
        Bluetooth_StartRxIt();
    }

    /* ---------------------------------------------------------------------- */
    /*  4) queue???⑥븘 ?덈뒗?곕룄 TX媛 idle?대㈃ main loop媛 ??踰???kick ?쒕떎. */
    /*                                                                        */
    /*  蹂댄넻? enqueue ?쒖젏?대굹 TX complete ISR媛 ?ㅼ쓬 chunk瑜?諛붾줈 ?쒖옉?쒕떎.   */
    /*  ?섏?留?HAL busy/error ??tx_running ?뚮옒洹멸? ?대젮媛?梨?硫덉떠 ?덉쑝硫?     */
    /*  ??寃쎈줈媛 ?ㅼ쓬 loop?먯꽌 ?ㅼ떆 ?대젮 以??                               */
    /* ---------------------------------------------------------------------- */
    Bluetooth_TxKick();
}
