#include "u8g2_uc1608_stm32.h"
#include "u8g2.h"

// main.c 에서 생성된 SPI2 핸들 사용
extern SPI_HandleTypeDef hspi2;

#ifndef UC1608_SPI_TX_TIMEOUT_MS
#define UC1608_SPI_TX_TIMEOUT_MS 20u
#endif

/*
 * DevEBox TFT 헤더의 D/C 핀은 PC5에 연결되어 있으므로,
 * 여기서 직접 정의해준다. (CubeMX에서 아직 사용 안 했기 때문)
 */
#define LCD_DC_GPIO_Port   GPIOC
#define LCD_DC_Pin         GPIO_PIN_5

// 내부 u8g2 객체 (외부에 직접 노출하지 않음)
static u8g2_t g_u8g2;

/* -------------------------------------------------------------
 *  UC1608 + U8G2용 화면 갱신 제어 상태
 *
 *  이번 수정의 핵심:
 *  - "전송만 20Hz 제한"이 아니라
 *    "렌더링 시작 자체를 20Hz 토큰으로 제한"한다.
 *  - 그래서 기존 1비트 flag 대신
 *    ISR가 쌓아 두는 '프레임 토큰 카운터'를 사용한다.
 * -----------------------------------------------------------*/

/// 1이면 "변경된 타일만 전송" 기능 사용, 0이면 항상 전체 화면 전송
static uint8_t s_uc1608_smart_update_enable = 1u;

/// 1이면 타이머 기반 프레임 제한 사용, 0이면 매 loop마다 렌더 허용
static uint8_t s_uc1608_frame_limit_enable = 1u;

/// ISR가 적립해 두는 프레임 토큰 수의 상한
/// 너무 많이 쌓아 봐야 오래된 화면을 뒤늦게 몰아서 그릴 이유가 없으므로 2로 제한
#define UC1608_FRAME_TOKEN_MAX 2u

/// 타이머 ISR가 적립해 둔 "렌더링 허가 토큰" 수
/// main loop는 이 토큰이 있을 때만 snapshot/copy/draw/commit 한다.
static volatile uint8_t s_uc1608_frame_tokens = 1u;

/// UC1608(240x128, 풀 버퍼) 기준 최대 버퍼 크기
/// tile_width(30) * 8바이트(한 타일) * tile_height(16 라인) = 3840바이트
#define UC1608_MAX_BUFFER_SIZE   (30u * 8u * 16u)

/// 이전 프레임 스냅샷 버퍼
/// 현재 u8g2 내부 버퍼와 비교해서, 어떤 타일이 바뀌었는지 계산하는 용도
static uint8_t s_uc1608_prev_frame[UC1608_MAX_BUFFER_SIZE];

/// 실제로 사용 중인 u8g2 버퍼 크기 (u8g2_GetBufferSize 결과 캐싱)
static uint16_t s_uc1608_buffer_size = 0u;



/* -------------------------------------------------------------
 *  GPIO & Delay 콜백 (U8x8 레벨)
 * -----------------------------------------------------------*/
static uint8_t u8x8_gpio_and_delay_stm32_uc1608(u8x8_t *u8x8,
                                                uint8_t msg,
                                                uint8_t arg_int,
                                                void *arg_ptr)
{
    (void)u8x8;
    (void)arg_ptr;

    switch (msg)
    {
    case U8X8_MSG_GPIO_AND_DELAY_INIT:
    {
        // D/C 핀을 Output으로 초기화 (PC5) – CS/BL은 이미 MX_GPIO_Init에서 설정됨
        GPIO_InitTypeDef GPIO_InitStruct = {0};

        __HAL_RCC_GPIOC_CLK_ENABLE();

        GPIO_InitStruct.Pin   = LCD_DC_Pin;
        GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
        GPIO_InitStruct.Pull  = GPIO_NOPULL;
        GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
        HAL_GPIO_Init(LCD_DC_GPIO_Port, &GPIO_InitStruct);

        // CS를 비선택 상태로, 백라이트는 일단 OFF로 시작
        HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
        HAL_GPIO_WritePin(LCD_BACKLIGHT_GPIO_Port, LCD_BACKLIGHT_Pin, GPIO_PIN_RESET);
        break;
    }

    case U8X8_MSG_DELAY_MILLI:
        HAL_Delay(arg_int);
        break;

    case U8X8_MSG_DELAY_10MICRO:
        // 대충 10us * arg_int 정도의 busy-wait (168MHz 기준 매우 러프)
        for (uint8_t i = 0; i < arg_int; i++)
        {
            for (volatile uint32_t n = 0; n < 30; n++)
            {
                __NOP();
            }
        }
        break;

    case U8X8_MSG_GPIO_CS:
        // UC1608 CS (Active High) – 실제 레벨 값은 display_info에서 넘겨줌
        HAL_GPIO_WritePin(LCD_CS_GPIO_Port,
                          LCD_CS_Pin,
                          arg_int ? GPIO_PIN_SET : GPIO_PIN_RESET);
        break;

    case U8X8_MSG_GPIO_DC:
        // D/C (Command/Data)
        HAL_GPIO_WritePin(LCD_DC_GPIO_Port,
                          LCD_DC_Pin,
                          arg_int ? GPIO_PIN_SET : GPIO_PIN_RESET);
        break;

    case U8X8_MSG_GPIO_RESET:
        // SLG240128D SPI 헤더에는 별도 RST가 없으므로 무시
        break;

    // 소프트웨어 SPI에 사용하는 나머지 GPIO 메시지는 여기선 필요 없음
    default:
        // 처리하지 않는 메시지는 0 리턴
        return 0;
    }

    return 1;
}

/* -------------------------------------------------------------
 *  SPI 바이트 콜백 (U8x8 레벨, HW SPI2 사용)
 * -----------------------------------------------------------*/
static uint8_t u8x8_byte_stm32_hw_spi_uc1608(u8x8_t *u8x8,
                                             uint8_t msg,
                                             uint8_t arg_int,
                                             void *arg_ptr)
{
    switch (msg)
    {
    case U8X8_MSG_BYTE_SEND:
        // arg_ptr: 전송할 데이터 포인터, arg_int: 길이
        if (HAL_SPI_Transmit(&hspi2,
                             (uint8_t *)arg_ptr,
                             arg_int,
							 UC1608_SPI_TX_TIMEOUT_MS) != HAL_OK)
        {
            return 0;
        }
        break;

    case U8X8_MSG_BYTE_INIT:
        // SPI2는 이미 MX_SPI2_Init()에서 초기화됨 :contentReference[oaicite:4]{index=4}
        break;

    case U8X8_MSG_BYTE_SET_DC:
        // D/C는 GPIO 콜백에 위임
        u8x8_gpio_SetDC(u8x8, arg_int);
        break;

    case U8X8_MSG_BYTE_START_TRANSFER:
        // CS 활성화 (chip_enable_level은 드라이버 쪽에서 설정)
        u8x8->gpio_and_delay_cb(u8x8,
                                U8X8_MSG_GPIO_CS,
                                u8x8->display_info->chip_enable_level,
                                NULL);
        break;

    case U8X8_MSG_BYTE_END_TRANSFER:
        // CS 비활성화
        u8x8->gpio_and_delay_cb(u8x8,
                                U8X8_MSG_GPIO_CS,
                                u8x8->display_info->chip_disable_level,
                                NULL);
        break;

    default:
        return 0;
    }

    return 1;
}

/* -------------------------------------------------------------
 *  내부 유틸: 테스트 화면 한 번 그리기
 * -----------------------------------------------------------*/
void U8G2_UC1608_DrawTestScreen(void)
{
    u8g2_ClearBuffer(&g_u8g2);

    // 적당한 작은 폰트 하나 선택 (필요에 따라 변경 가능)
    u8g2_SetFont(&g_u8g2, u8g2_font_ncenB08_tr);

    u8g2_DrawStr(&g_u8g2, 10, 20, "U8G2 OK");
    u8g2_DrawFrame(&g_u8g2, 0, 0, 240, 128);

    u8g2_SendBuffer(&g_u8g2);
}

/* -------------------------------------------------------------
 *  외부 공개 함수들
 * -----------------------------------------------------------*/

/* -------------------------------------------------------------
 *  내부 유틸: 스마트 업데이트용 상태 초기화
 *  - u8g2_Setup_xxx() 호출 이후에만 사용해야 함
 * -----------------------------------------------------------*/

/* -------------------------------------------------------------
 *  설정용 API: 스마트 업데이트 / FPS 리밋 On/Off
 * -----------------------------------------------------------*/

/// 스마트 업데이트 기능 On/Off (1: On, 0: Off)
void U8G2_UC1608_EnableSmartUpdate(uint8_t enable)
{
    s_uc1608_smart_update_enable = (enable != 0u) ? 1u : 0u;
}

/// 타이머 기반 FPS 리밋 기능 On/Off
void U8G2_UC1608_EnableFrameLimit(uint8_t enable)
{
    /* ---------------------------------------------------------------------- */
    /*  이 함수는 main context에서만 호출된다고 가정하지만,                     */
    /*  frame token을 ISR도 만지므로 짧게 IRQ를 막고 같이 갱신한다.             */
    /* ---------------------------------------------------------------------- */
    __disable_irq();

    s_uc1608_frame_limit_enable = (enable != 0u) ? 1u : 0u;

    if (s_uc1608_frame_limit_enable == 0u)
    {
        /* 제한을 끄면 다음 loop부터 바로 그릴 수 있게 1토큰 보장 */
        s_uc1608_frame_tokens = 1u;
    }
    else if (s_uc1608_frame_tokens == 0u)
    {
        /* 제한을 켤 때도 첫 프레임이 너무 늦어지지 않게 1토큰 보장 */
        s_uc1608_frame_tokens = 1u;
    }

    __enable_irq();
}




void U8G2_UC1608_FrameTickFromISR(void)
{
    /* ---------------------------------------------------------------------- */
    /*  타이머 ISR는 "렌더 해도 된다"는 토큰만 적립한다.                         */
    /*                                                                        */
    /*  토큰 수를 2개까지만 쌓아 두는 이유:                                     */
    /*  - UI가 잠깐 바빴더라도 바로 다음 슬롯 정도까지만 따라잡으면 충분하다.   */
    /*  - 오래된 프레임 허가를 무한정 적재하는 건 이득이 없다.                  */
    /* ---------------------------------------------------------------------- */
    if (s_uc1608_frame_limit_enable == 0u)
    {
        return;
    }

    if (s_uc1608_frame_tokens < UC1608_FRAME_TOKEN_MAX)
    {
        s_uc1608_frame_tokens++;
    }
}



uint8_t U8G2_UC1608_TryAcquireFrameToken(void)
{
    uint8_t acquired = 0u;

    /* 제한이 꺼져 있으면 언제나 렌더 허용 */
    if (s_uc1608_frame_limit_enable == 0u)
    {
        return 1u;
    }

    /* ---------------------------------------------------------------------- */
    /*  check + decrement 는 묶어서 원자적으로 처리한다.                        */
    /* ---------------------------------------------------------------------- */
    __disable_irq();

    if (s_uc1608_frame_tokens != 0u)
    {
        s_uc1608_frame_tokens--;
        acquired = 1u;
    }

    __enable_irq();

    return acquired;
}



/* -------------------------------------------------------------
 *  메인 루프에서 호출하는 "스마트 Commit" 함수
 *
 *  기존에는:
 *      u8g2_SendBuffer(&g_u8g2);
 *  라고 직접 호출했다면
 *
 *  이제는:
 *      U8G2_UC1608_CommitBuffer();
 *
 *  로 교체하면,
 *    - FPS 리밋이 걸려 있으면 타이머 틱이 올 때까진 전송 건너뜀
 *    - 스마트 업데이트가 켜져 있으면, 변경된 타일 영역만 전송
 * -----------------------------------------------------------*/
void U8G2_UC1608_CommitBuffer(void)
{
    const uint8_t *curr;
    uint8_t tile_width;
    uint8_t tile_height;
    uint16_t tile_count;

    /* ---------------------------------------------------------------------- */
    /*  이제 frame limit 판단은 main loop의                                   */
    /*  U8G2_UC1608_TryAcquireFrameToken() 에서 끝낸다.                        */
    /*                                                                        */
    /*  이 함수는 "이미 이번 프레임을 그려도 된다"고 결정된 뒤에만 호출된다.    */
    /* ---------------------------------------------------------------------- */

    /* 1. 스마트 업데이트가 꺼져 있으면 기존 동작과 동일하게 전체 화면 전송 */
    if (s_uc1608_smart_update_enable == 0u)
    {
        u8g2_SendBuffer(&g_u8g2);
        return;
    }

    /* 2. 스마트 업데이트 경로
     *    - u8g2 내부 버퍼와 이전 프레임 버퍼를 타일 단위로 비교해서
     *      변경된 타일들의 bounding box만 디스플레이에 전송한다. */

    curr = u8g2_GetBufferPtr(&g_u8g2);
    tile_width  = u8g2_GetBufferTileWidth(&g_u8g2);   // 240px / 8px = 30 tiles
    tile_height = u8g2_GetBufferTileHeight(&g_u8g2);  // 128px / 8px = 16 tiles
    tile_count  = (uint16_t)tile_width * (uint16_t)tile_height;

    /* 변경된 타일들의 bounding box (타일 좌표 기준) */
    uint8_t tx_min = tile_width;   // 아직 아무 것도 안 바뀌었다는 의미로 max 값으로 초기화
    uint8_t ty_min = tile_height;
    uint8_t tx_max = 0u;
    uint8_t ty_max = 0u;

    /* u8g2의 풀 버퍼 레이아웃:
     *  - 한 타일(8x8픽셀)이 8바이트를 차지
     *  - 타일들은 (0,0)부터 가로로 tile_width개, 그 다음 줄로 내려가는 순서로 나열 */
    uint16_t buf_index = 0u;

    for (uint16_t tile_index = 0u; tile_index < tile_count; tile_index++)
    {
        uint8_t tile_changed = 0u;

        /* 한 타일의 8바이트를 순회하면서 변경 여부 확인 */
        for (uint8_t b = 0u; b < 8u; b++, buf_index++)
        {
            uint8_t new_byte = curr[buf_index];

            if (new_byte != s_uc1608_prev_frame[buf_index])
            {
                tile_changed = 1u;
                s_uc1608_prev_frame[buf_index] = new_byte;
            }
        }

        if (tile_changed != 0u)
        {
            uint8_t ty = (uint8_t)(tile_index / tile_width);
            uint8_t tx = (uint8_t)(tile_index % tile_width);

            if (tx < tx_min) tx_min = tx;
            if (tx > tx_max) tx_max = tx;
            if (ty < ty_min) ty_min = ty;
            if (ty > ty_max) ty_max = ty;
        }
    }

    /* 변경된 타일이 하나도 없으면, SPI 전송 자체를 생략 */
    if (tx_min >= tile_width || ty_min >= tile_height)
    {
        return;
    }

    uint8_t tw = (uint8_t)(tx_max - tx_min + 1u);
    uint8_t th = (uint8_t)(ty_max - ty_min + 1u);

    /* 변경된 타일 영역만 UC1608으로 전송
     *  - 물리적으로는 해당 영역의 GDDRAM만 갱신되므로
     *    작은 UI 변화에 대해 버스 부하와 소요 시간이 크게 줄어든다. */
    u8g2_UpdateDisplayArea(&g_u8g2,
                           tx_min,  /* 시작 타일 X (0..29) */
                           ty_min,  /* 시작 타일 Y (0..15) */
                           tw,      /* 타일 폭 */
                           th);     /* 타일 높이 */
}


static void uc1608_init_smart_update_state(void)
{
    /* u8g2 내부 버퍼 크기 가져오기
     * 240x128 풀 버퍼 기준으로 3840 바이트가 나와야 정상 */
    s_uc1608_buffer_size = u8g2_GetBufferSize(&g_u8g2);

    if (s_uc1608_buffer_size > UC1608_MAX_BUFFER_SIZE)
    {
        /* 방어 코드 - 만약 매크로 계산 실수로 사이즈가 커지면 잘라낸다 */
        s_uc1608_buffer_size = UC1608_MAX_BUFFER_SIZE;
    }

    /* 첫 프레임 때는 "모든 바이트가 바뀐 것처럼" 보이도록
     * 이전 프레임 버퍼를 일부러 이상한 값으로 채워둔다.
     * 이렇게 하면 첫 Commit에서 화면 전체가 한 번 업데이트된다. */
    for (uint16_t i = 0; i < s_uc1608_buffer_size; i++)
    {
        s_uc1608_prev_frame[i] = 0xAAu;
    }

    /* 첫 프레임은 타이머가 아직 안 돌더라도 바로 나갈 수 있게 1로 세팅 */
    s_uc1608_frame_tokens = 1u;
}

void U8G2_UC1608_Init(void)
{
    // u8g2 구조체 설정: UC1608 240x128, 풀 버퍼 모드
    u8g2_Setup_uc1608_240x128_f(
        &g_u8g2,
        U8G2_R2,                             // 회전 90도 + 1단계 반전
        u8x8_byte_stm32_hw_spi_uc1608,       // SPI 콜백
        u8x8_gpio_and_delay_stm32_uc1608     // GPIO & delay 콜백
    );

    // 디스플레이 초기화 & 전원 ON
    u8g2_InitDisplay(&g_u8g2);
    u8g2_SetPowerSave(&g_u8g2, 0);

    // 백라이트 켜기 (옵션 – 필요 없으면 지워도 됨)
    HAL_GPIO_WritePin(LCD_BACKLIGHT_GPIO_Port,
                      LCD_BACKLIGHT_Pin,
                      GPIO_PIN_SET);

    // ★★★ contrast 줄여보기 (40~120 사이에서 감으로 트라이)
    u8g2_SetContrast(&g_u8g2, 120);    // 예시: 80 정도에서 시작

    // ★★★ 화면이 좌우/상하로 이상하게 보이면 flip 켜보기
    u8g2_SetFlipMode(&g_u8g2, 1);

    /* ---------------------------------------------------------
     *  스마트 업데이트용 이전 프레임 버퍼 초기화
     * -------------------------------------------------------*/
    uc1608_init_smart_update_state();
}

u8g2_t *U8G2_UC1608_GetHandle(void)
{
    return &g_u8g2;
}
