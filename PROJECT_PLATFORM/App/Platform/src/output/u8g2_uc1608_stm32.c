#include "u8g2_uc1608_stm32.h"

#include <string.h>

#include "u8g2.h"
#include "u8x8.h"

/* main.c / CubeMX에서 생성된 SPI2 핸들 사용 */
extern SPI_HandleTypeDef hspi2;

#ifndef UC1608_SPI_TX_TIMEOUT_MS
#define UC1608_SPI_TX_TIMEOUT_MS 20u
#endif

/* -------------------------------------------------------------------------- */
/*  DevEBox / 패널 헤더 연결 정보                                               */
/*                                                                            */
/*  D/C는 PC5에 직접 연결되어 있으므로 이 wrapper에서 제어한다.                */
/*  CS는 Cube가 이미 LCD_CS_* 매크로로 제공한다.                              */
/*                                                                            */
/*  중요                                                                      */
/*  - 이번 정리에서는 LCD_BACKLIGHT 핀을 절대 건드리지 않는다.                */
/*  - backlight는 PB1 / TIM3_CH4 / BACKLIGHT_DRIVER의 전담 책임이다.          */
/* -------------------------------------------------------------------------- */
#define LCD_DC_GPIO_Port   GPIOC
#define LCD_DC_Pin         GPIO_PIN_5

/* -------------------------------------------------------------------------- */
/*  내부 U8G2 객체 / 스마트 업데이트 상태                                     */
/* -------------------------------------------------------------------------- */
static u8g2_t g_u8g2;

#define UC1608_FRAME_TOKEN_MAX  2u

static volatile uint8_t s_uc1608_frame_tokens = 1u;
static uint16_t s_uc1608_buffer_size = 0u;

volatile u8g2_uc1608_runtime_t g_u8g2_uc1608_runtime;

/* -------------------------------------------------------------------------- */
/*  내부 유틸: runtime settings 정규화                                          */
/* -------------------------------------------------------------------------- */
static void U8G2_UC1608_NormalizeSettings(app_uc1608_settings_t *settings)
{
    if (settings == 0)
    {
        return;
    }

    if (settings->temperature_compensation > 3u)
    {
        settings->temperature_compensation = 3u;
    }

    if (settings->bias_ratio > 3u)
    {
        settings->bias_ratio = 3u;
    }

    if (settings->ram_access_mode > 3u)
    {
        settings->ram_access_mode = 3u;
    }

    if (settings->start_line_raw > 63u)
    {
        settings->start_line_raw = 63u;
    }

    if (settings->fixed_line_raw > 15u)
    {
        settings->fixed_line_raw = 15u;
    }

    if (settings->power_control_raw > 7u)
    {
        settings->power_control_raw = 7u;
    }

    if (settings->flip_mode > 1u)
    {
        settings->flip_mode = 1u;
    }
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: UC1608 command 전송                                              */
/*                                                                            */
/*  U8G2 full-buffer draw API와 별개로,                                       */
/*  설정 레지스터 write를 여기서 직접 보낸다.                                 */
/* -------------------------------------------------------------------------- */
static void U8G2_UC1608_SendCmdOnly(uint8_t cmd)
{
    u8x8_cad_StartTransfer(&g_u8g2.u8x8);
    u8x8_cad_SendCmd(&g_u8g2.u8x8, cmd);
    u8x8_cad_EndTransfer(&g_u8g2.u8x8);
}

static void U8G2_UC1608_SendCmdArg(uint8_t cmd, uint8_t arg)
{
    u8x8_cad_StartTransfer(&g_u8g2.u8x8);
    u8x8_cad_SendCmd(&g_u8g2.u8x8, cmd);
    u8x8_cad_SendArg(&g_u8g2.u8x8, arg);
    u8x8_cad_EndTransfer(&g_u8g2.u8x8);
}

/* -------------------------------------------------------------------------- */
/*  GPIO & Delay callback                                                     */
/* -------------------------------------------------------------------------- */
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
            GPIO_InitTypeDef GPIO_InitStruct;

            memset(&GPIO_InitStruct, 0, sizeof(GPIO_InitStruct));
            __HAL_RCC_GPIOC_CLK_ENABLE();

            GPIO_InitStruct.Pin   = LCD_DC_Pin;
            GPIO_InitStruct.Mode  = GPIO_MODE_OUTPUT_PP;
            GPIO_InitStruct.Pull  = GPIO_NOPULL;
            GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
            HAL_GPIO_Init(LCD_DC_GPIO_Port, &GPIO_InitStruct);

            /* -------------------------------------------------------------- */
            /*  CS만 비선택 상태로 둔다.                                      */
            /*  backlight는 절대 여기서 건드리지 않는다.                     */
            /* -------------------------------------------------------------- */
            HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
            break;
        }

        case U8X8_MSG_DELAY_MILLI:
            HAL_Delay(arg_int);
            break;

        case U8X8_MSG_DELAY_10MICRO:
            for (uint8_t i = 0u; i < arg_int; ++i)
            {
                for (volatile uint32_t n = 0u; n < 30u; ++n)
                {
                    __NOP();
                }
            }
            break;

        case U8X8_MSG_GPIO_CS:
            HAL_GPIO_WritePin(LCD_CS_GPIO_Port,
                              LCD_CS_Pin,
                              arg_int ? GPIO_PIN_SET : GPIO_PIN_RESET);
            break;

        case U8X8_MSG_GPIO_DC:
            HAL_GPIO_WritePin(LCD_DC_GPIO_Port,
                              LCD_DC_Pin,
                              arg_int ? GPIO_PIN_SET : GPIO_PIN_RESET);
            break;

        case U8X8_MSG_GPIO_RESET:
            /* 별도 reset 핀이 없으므로 무시 */
            break;

        default:
            return 0;
    }

    return 1;
}

/* -------------------------------------------------------------------------- */
/*  HW SPI callback                                                           */
/* -------------------------------------------------------------------------- */
static uint8_t u8x8_byte_stm32_hw_spi_uc1608(u8x8_t *u8x8,
                                             uint8_t msg,
                                             uint8_t arg_int,
                                             void *arg_ptr)
{
    switch (msg)
    {
        case U8X8_MSG_BYTE_SEND:
            if (HAL_SPI_Transmit(&hspi2,
                                 (uint8_t *)arg_ptr,
                                 arg_int,
                                 UC1608_SPI_TX_TIMEOUT_MS) != HAL_OK)
            {
                return 0;
            }
            break;

        case U8X8_MSG_BYTE_INIT:
            break;

        case U8X8_MSG_BYTE_SET_DC:
            u8x8_gpio_SetDC(u8x8, arg_int);
            break;

        case U8X8_MSG_BYTE_START_TRANSFER:
            u8x8->gpio_and_delay_cb(u8x8,
                                    U8X8_MSG_GPIO_CS,
                                    u8x8->display_info->chip_enable_level,
                                    NULL);
            break;

        case U8X8_MSG_BYTE_END_TRANSFER:
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

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 스마트 업데이트 캐시 초기화                                      */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/*  내부 helper: U8G2 단일 버퍼 메타데이터 초기화                                */
/*                                                                            */
/*  예전 구현은 U8G2 full framebuffer와 wrapper shadow framebuffer를 둘 다       */
/*  들고 있으면서 commit 시 두 버퍼를 비교했다.                                 */
/*                                                                            */
/*  이번 리팩터링에서는 shadow framebuffer를 제거했으므로                        */
/*  wrapper가 따로 초기화할 건 실제 U8G2 buffer size와 frame token 뿐이다.       */
/*  즉 렌더링 결과의 단일 진실원은 이제 g_u8g2 내부 버퍼 하나만 남는다.          */
/* -------------------------------------------------------------------------- */
static void uc1608_init_smart_update_state(void)
{
    s_uc1608_buffer_size = u8g2_GetBufferSize(&g_u8g2);

    s_uc1608_frame_tokens = 1u;
    g_u8g2_uc1608_runtime.buffer_size = s_uc1608_buffer_size;
    g_u8g2_uc1608_runtime.frame_tokens = s_uc1608_frame_tokens;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 스마트 업데이트 cache invalidation                               */
/*                                                                            */
/*  panel register만 바뀌고 framebuffer byte는 그대로인 상황에서도             */
/*  다음 commit에서 전체 화면이 다시 전송되도록 만든다.                       */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/*  공개 API: shadow-cache invalidation 호환 계층                              */
/*                                                                            */
/*  예전에는 이전 프레임 그림자 버퍼를 강제로 깨서 다음 commit이 전체 화면을      */
/*  다시 보내도록 만들었다.                                                     */
/*                                                                            */
/*  지금은 shadow buffer 자체가 사라졌기 때문에 실제 invalidation 대상은 없다.   */
/*  다만 기존 호출부를 깨지 않게 하려고 API는 유지하며, frame limiter가 켜져      */
/*  있을 때 다음 frame을 즉시 내보낼 수 있도록 token만 복구한다.                 */
/* -------------------------------------------------------------------------- */
void U8G2_UC1608_InvalidateSmartUpdateCache(void)
{
    __disable_irq();
    s_uc1608_frame_tokens = 1u;
    __enable_irq();

    g_u8g2_uc1608_runtime.frame_tokens = s_uc1608_frame_tokens;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 테스트 화면                                                      */
/* -------------------------------------------------------------------------- */
void U8G2_UC1608_DrawTestScreen(void)
{
    u8g2_ClearBuffer(&g_u8g2);
    u8g2_SetFont(&g_u8g2, u8g2_font_ncenB08_tr);
    u8g2_DrawStr(&g_u8g2, 10u, 20u, "U8G2 OK");
    u8g2_DrawFrame(&g_u8g2, 0u, 0u, 240u, 128u);
    u8g2_SendBuffer(&g_u8g2);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: smart update on/off                                              */
/* -------------------------------------------------------------------------- */
void U8G2_UC1608_EnableSmartUpdate(uint8_t enable)
{
    g_u8g2_uc1608_runtime.smart_update_enable = (enable != 0u) ? 1u : 0u;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: frame limit on/off                                               */
/* -------------------------------------------------------------------------- */
void U8G2_UC1608_EnableFrameLimit(uint8_t enable)
{
    __disable_irq();

    g_u8g2_uc1608_runtime.frame_limit_enable = (enable != 0u) ? 1u : 0u;

    if (g_u8g2_uc1608_runtime.frame_limit_enable == 0u)
    {
        s_uc1608_frame_tokens = 1u;
    }
    else if (s_uc1608_frame_tokens == 0u)
    {
        s_uc1608_frame_tokens = 1u;
    }

    __enable_irq();

    g_u8g2_uc1608_runtime.frame_tokens = s_uc1608_frame_tokens;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: ISR frame token 적립                                             */
/* -------------------------------------------------------------------------- */
void U8G2_UC1608_FrameTickFromISR(void)
{
    if (g_u8g2_uc1608_runtime.frame_limit_enable == 0u)
    {
        return;
    }

    if (s_uc1608_frame_tokens < UC1608_FRAME_TOKEN_MAX)
    {
        s_uc1608_frame_tokens++;
    }

    g_u8g2_uc1608_runtime.frame_tokens = s_uc1608_frame_tokens;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: frame token 획득                                                 */
/* -------------------------------------------------------------------------- */
uint8_t U8G2_UC1608_TryAcquireFrameToken(void)
{
    uint8_t acquired = 0u;

    if (g_u8g2_uc1608_runtime.frame_limit_enable == 0u)
    {
        return 1u;
    }

    __disable_irq();

    if (s_uc1608_frame_tokens != 0u)
    {
        s_uc1608_frame_tokens--;
        acquired = 1u;
    }

    __enable_irq();

    g_u8g2_uc1608_runtime.frame_tokens = s_uc1608_frame_tokens;
    return acquired;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: smart commit                                                     */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/*  공개 API: 단일 U8G2 framebuffer commit                                     */
/*                                                                            */
/*  중요한 변경점                                                               */
/*  - commit 경로는 이제 항상 U8G2 내부 framebuffer 하나만 사용한다.            */
/*  - wrapper shadow buffer가 없어졌으므로 RAM을 화면 1장만큼 절약한다.          */
/*  - smart_update_enable 설정은 호출부/설정 구조 호환성을 위해 남겨두되,         */
/*    실제 전송 경로는 full-buffer send 하나로 통일한다.                        */
/*                                                                            */
/*  이 방식이 더 안전한 이유                                                     */
/*  - panel 설정 변경 후 cache mismatch를 신경쓸 필요가 없다.                   */
/*  - 디버깅 시 현재 그려진 버퍼를 딱 한 군데만 보면 된다.                       */
/*  - 표시 경로가 단순해져 잠재적인 부분 갱신 버그를 줄인다.                    */
/* -------------------------------------------------------------------------- */
void U8G2_UC1608_CommitBuffer(void)
{
    u8g2_SendBuffer(&g_u8g2);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 개별 패널 설정                                                    */
/* -------------------------------------------------------------------------- */
void U8G2_UC1608_SetContrastRaw(uint8_t contrast_raw)
{
    U8G2_UC1608_SendCmdArg(0x081u, contrast_raw);
    g_u8g2_uc1608_runtime.applied.contrast = contrast_raw;
}

void U8G2_UC1608_SetTemperatureCompensation(uint8_t tc_raw_0_3)
{
    tc_raw_0_3 &= 0x03u;

    /* ---------------------------------------------------------------------- */
    /*  240x128 패널에서는 multiplex 128bit를 유지해야 하므로                  */
    /*  bit2=1인 0x24 base 위에 temp comp bits[1:0]를 얹는다.                 */
    /*  참조: 업로드된 UC1608 드라이버의 240x128 init 시퀀스.                  */
    /* ---------------------------------------------------------------------- */
    U8G2_UC1608_SendCmdOnly((uint8_t)(0x024u | tc_raw_0_3));
    g_u8g2_uc1608_runtime.applied.temperature_compensation = tc_raw_0_3;
}

void U8G2_UC1608_SetBiasRatio(uint8_t bias_raw_0_3)
{
    bias_raw_0_3 &= 0x03u;
    U8G2_UC1608_SendCmdOnly((uint8_t)(0x0E8u | bias_raw_0_3));
    g_u8g2_uc1608_runtime.applied.bias_ratio = bias_raw_0_3;
}

void U8G2_UC1608_SetRamAccessMode(uint8_t ram_access_raw_0_3)
{
    ram_access_raw_0_3 &= 0x03u;
    U8G2_UC1608_SendCmdOnly((uint8_t)(0x088u | ram_access_raw_0_3));
    g_u8g2_uc1608_runtime.applied.ram_access_mode = ram_access_raw_0_3;
}

void U8G2_UC1608_SetDisplayStartLineRaw(uint8_t start_line_raw_0_63)
{
    start_line_raw_0_63 &= 0x03Fu;
    U8G2_UC1608_SendCmdOnly((uint8_t)(0x040u | start_line_raw_0_63));
    g_u8g2_uc1608_runtime.applied.start_line_raw = start_line_raw_0_63;
    U8G2_UC1608_InvalidateSmartUpdateCache();
}

void U8G2_UC1608_SetFixedLineRaw(uint8_t fixed_line_raw_0_15)
{
    fixed_line_raw_0_15 &= 0x0Fu;
    U8G2_UC1608_SendCmdOnly((uint8_t)(0x090u | fixed_line_raw_0_15));
    g_u8g2_uc1608_runtime.applied.fixed_line_raw = fixed_line_raw_0_15;
    U8G2_UC1608_InvalidateSmartUpdateCache();
}

void U8G2_UC1608_SetPowerControlRaw(uint8_t power_raw_0_7)
{
    power_raw_0_7 &= 0x07u;
    U8G2_UC1608_SendCmdOnly((uint8_t)(0x028u | power_raw_0_7));
    g_u8g2_uc1608_runtime.applied.power_control_raw = power_raw_0_7;
}

void U8G2_UC1608_SetFlipModeRaw(uint8_t flip_mode_raw_0_1)
{
    flip_mode_raw_0_1 &= 0x01u;
    u8g2_SetFlipMode(&g_u8g2, flip_mode_raw_0_1);
    g_u8g2_uc1608_runtime.applied.flip_mode = flip_mode_raw_0_1;
    U8G2_UC1608_InvalidateSmartUpdateCache();
}

/* -------------------------------------------------------------------------- */
/*  공개 API: panel settings 일괄 적용                                          */
/* -------------------------------------------------------------------------- */
void U8G2_UC1608_ApplyPanelSettings(const app_uc1608_settings_t *settings)
{
    app_uc1608_settings_t normalized;

    if (settings == 0)
    {
        return;
    }

    normalized = *settings;
    U8G2_UC1608_NormalizeSettings(&normalized);

    U8G2_UC1608_SetTemperatureCompensation(normalized.temperature_compensation);
    U8G2_UC1608_SetBiasRatio(normalized.bias_ratio);
    U8G2_UC1608_SetPowerControlRaw(normalized.power_control_raw);
    U8G2_UC1608_SetDisplayStartLineRaw(normalized.start_line_raw);
    U8G2_UC1608_SetFixedLineRaw(normalized.fixed_line_raw);
    U8G2_UC1608_SetRamAccessMode(normalized.ram_access_mode);
    U8G2_UC1608_SetContrastRaw(normalized.contrast);
    U8G2_UC1608_SetFlipModeRaw(normalized.flip_mode);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: APP_STATE에서 읽어 패널 적용                                     */
/* -------------------------------------------------------------------------- */
void U8G2_UC1608_LoadAndApplySettingsFromAppState(void)
{
    app_settings_t settings_snapshot;

    APP_STATE_CopySettingsSnapshot(&settings_snapshot);
    U8G2_UC1608_ApplyPanelSettings(&settings_snapshot.uc1608);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 마지막 반영값 복사                                                */
/* -------------------------------------------------------------------------- */
void U8G2_UC1608_GetAppliedPanelSettings(app_uc1608_settings_t *dst)
{
    if (dst == 0)
    {
        return;
    }

    *dst = g_u8g2_uc1608_runtime.applied;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: init                                                             */
/* -------------------------------------------------------------------------- */
void U8G2_UC1608_Init(void)
{
    memset((void *)&g_u8g2_uc1608_runtime, 0, sizeof(g_u8g2_uc1608_runtime));

    u8g2_Setup_uc1608_240x128_f(&g_u8g2,
                                U8G2_R2,
                                u8x8_byte_stm32_hw_spi_uc1608,
                                u8x8_gpio_and_delay_stm32_uc1608);

    u8g2_InitDisplay(&g_u8g2);
    u8g2_SetPowerSave(&g_u8g2, 0u);

    g_u8g2_uc1608_runtime.initialized = 1u;
    g_u8g2_uc1608_runtime.smart_update_enable = 1u;
    g_u8g2_uc1608_runtime.frame_limit_enable = 1u;

    uc1608_init_smart_update_state();
    U8G2_UC1608_LoadAndApplySettingsFromAppState();
}

/* -------------------------------------------------------------------------- */
/*  공개 API: handle access                                                    */
/* -------------------------------------------------------------------------- */
u8g2_t *U8G2_UC1608_GetHandle(void)
{
    return &g_u8g2;
}
