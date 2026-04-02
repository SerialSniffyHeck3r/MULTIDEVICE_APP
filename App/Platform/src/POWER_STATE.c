#include "POWER_STATE.h"

#include "APP_STATE.h"
#include "button.h"
#include "ui_statusbar.h"
#include "ui_types.h"
#include "u8g2.h"
#include "u8g2_uc1608_stm32.h"

#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/* Soft Power Switch pin compatibility                                         */
/*                                                                            */
/* Application2 쪽 main.h에는 이미                                             */
/*   SOFT_PWR_OFF_Pin / SOFT_PWR_PUSH_Pin                                      */
/* 이 정의되어 있다.                                                            */
/*                                                                            */
/* 다만 로컬 .ioc를 아직 regenerate 하지 않았더라도,                            */
/* 사용자가 이번 작업에서 배선 기준을 "PE3=OFF, PE4=PUSH" 로 고정했다고       */
/* 명시했으므로 fallback 매크로를 함께 둔다.                                   */
/* -------------------------------------------------------------------------- */
#ifndef SOFT_PWR_OFF_Pin
#define SOFT_PWR_OFF_Pin        GPIO_PIN_3
#define SOFT_PWR_OFF_GPIO_Port  GPIOE
#endif

#ifndef SOFT_PWR_PUSH_Pin
#define SOFT_PWR_PUSH_Pin       GPIO_PIN_4
#define SOFT_PWR_PUSH_GPIO_Port GPIOE
#endif

/* -------------------------------------------------------------------------- */
/* 내부 정책 상수                                                              */
/* -------------------------------------------------------------------------- */
#define POWER_STATE_SOFTKEY_ACTIVE_LEVEL             GPIO_PIN_RESET
#define POWER_STATE_SOFTKEY_DEBOUNCE_MS              25u
#define POWER_STATE_SOFTKEY_LONG_PRESS_MS            BUTTON_LONG_PRESS_MS
#define POWER_STATE_POWER_ON_CONFIRM_TIMEOUT_MS      30000u
#define POWER_STATE_BOOT_GATE_REDRAW_PERIOD_MS       33u

/* -------------------------------------------------------------------------- */
/* XBM power icon                                                              */
/*                                                                            */
/* 크기                                                                        */
/* - 48 x 48 pixel                                                             */
/*                                                                            */
/* 그림 의미                                                                   */
/* - 원형 power 심볼의 외곽 C-arc                                               */
/* - 가운데 위쪽의 수직 bar                                                    */
/*                                                                            */
/* 사용 위치                                                                   */
/* - POWER ON 확인 화면                                                        */
/* - POWER OFF 확인 화면                                                       */
/*                                                                            */
/* 위치 정책                                                                   */
/* - 전체 화면(240x128) 기준 가로 중앙 근처에 배치한다.                        */
/* - 텍스트보다 위쪽에 두어 "큰 전원 아이콘 + 큰 제목" 구조를 만든다.          */
/* -------------------------------------------------------------------------- */
#define POWER_STATE_ICON_POWER_W 48u
#define POWER_STATE_ICON_POWER_H 48u

static const uint8_t s_power_icon_48x48[] =
{
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x80, 0x07, 0x00, 0x00, 0x00, 0x00, 0x80, 0x07, 0x00, 0x00,
    0x00, 0x00, 0x80, 0x07, 0x00, 0x00, 0x00, 0x00, 0x80, 0x07, 0x00, 0x00,
    0x00, 0x00, 0xF0, 0x0F, 0x00, 0x00, 0x00, 0x00, 0xFE, 0x7F, 0x00, 0x00,
    0x00, 0x80, 0xFF, 0xFF, 0x01, 0x00, 0x00, 0xC0, 0xFF, 0xFF, 0x03, 0x00,
    0x00, 0xF0, 0x8F, 0xF7, 0x0F, 0x00, 0x00, 0xF8, 0x83, 0xC7, 0x1F, 0x00,
    0x00, 0xFC, 0x80, 0x07, 0x3F, 0x00, 0x00, 0x7C, 0x80, 0x07, 0x3E, 0x00,
    0x00, 0x3E, 0x80, 0x07, 0x3C, 0x00, 0x00, 0x1F, 0x80, 0x07, 0x08, 0x00,
    0x00, 0x0F, 0x80, 0x07, 0x00, 0x00, 0x80, 0x0F, 0x80, 0x07, 0x00, 0x00,
    0x80, 0x07, 0x80, 0x07, 0x00, 0x00, 0x80, 0x07, 0x80, 0x07, 0x00, 0x00,
    0xC0, 0x03, 0x80, 0x07, 0x00, 0x00, 0xC0, 0x03, 0x80, 0x07, 0x00, 0x00,
    0xC0, 0x03, 0x80, 0x07, 0x00, 0x00, 0xC0, 0x03, 0x00, 0x00, 0x00, 0x00,
    0xC0, 0x03, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x03, 0x00, 0x00, 0x00, 0x00,
    0xC0, 0x03, 0x00, 0x00, 0x00, 0x00, 0xC0, 0x03, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x07, 0x00, 0x00, 0x00, 0x00, 0x80, 0x07, 0x00, 0x00, 0x00, 0x00,
    0x80, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0F, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x1F, 0x00, 0x00, 0x08, 0x00, 0x00, 0x3E, 0x00, 0x00, 0x3C, 0x00,
    0x00, 0x7C, 0x00, 0x00, 0x3E, 0x00, 0x00, 0xFC, 0x00, 0x00, 0x3F, 0x00,
    0x00, 0xF8, 0x03, 0xC0, 0x1F, 0x00, 0x00, 0xF0, 0x0F, 0xF0, 0x0F, 0x00,
    0x00, 0xC0, 0xFF, 0xFF, 0x03, 0x00, 0x00, 0x80, 0xFF, 0xFF, 0x01, 0x00,
    0x00, 0x00, 0xFE, 0x7F, 0x00, 0x00, 0x00, 0x00, 0xF0, 0x0F, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
};

/* -------------------------------------------------------------------------- */
/* Soft power key runtime                                                      */
/*                                                                            */
/* sampled_pressed                                                             */
/* - 가장 최근 polling에서 읽은 raw pressed 상태                               */
/*                                                                            */
/* stable_pressed                                                              */
/* - debounce가 끝난 뒤 확정된 논리 pressed 상태                               */
/*                                                                            */
/* debounce_active / debounce_due_ms                                           */
/* - raw 상태가 바뀐 뒤 몇 ms 뒤에 안정 상태를 확정할지 저장한다.              */
/*                                                                            */
/* long_reported                                                               */
/* - 이번 눌림 구간에서 long press를 이미 한 번 발생시켰는가                    */
/*                                                                            */
/* press_start_ms                                                              */
/* - 안정된 눌림 시작 시각                                                     */
/* -------------------------------------------------------------------------- */
typedef struct
{
    uint8_t  sampled_pressed;
    uint8_t  stable_pressed;
    uint8_t  debounce_active;
    uint8_t  long_reported;
    uint32_t debounce_due_ms;
    uint32_t press_start_ms;
} power_softkey_runtime_t;

/* -------------------------------------------------------------------------- */
/* 정적 상태 저장소                                                            */
/* -------------------------------------------------------------------------- */
static power_state_mode_t    s_power_mode = POWER_STATE_MODE_NONE;
static power_state_mode_t    s_resume_mode_after_off_confirm = POWER_STATE_MODE_NONE;
static power_softkey_runtime_t s_softkey_rt;
static uint32_t              s_power_on_confirm_deadline_ms = 0u;
static uint32_t              s_last_boot_gate_draw_ms = 0u;
static uint8_t               s_boot_gate_draw_started = 0u;

/* -------------------------------------------------------------------------- */
/* 내부 helper 선언                                                            */
/* -------------------------------------------------------------------------- */
static uint8_t POWER_STATE_TimeDue(uint32_t now_ms, uint32_t due_ms);
static void POWER_STATE_EnableGpioClock(GPIO_TypeDef *port);
static void POWER_STATE_ConfigureSoftPowerPins(void);
static uint8_t POWER_STATE_ReadSoftKeyPressed(void);
static void POWER_STATE_SoftKeySyncToCurrent(uint32_t now_ms);
static void POWER_STATE_ServiceSoftKeyRuntime(uint32_t now_ms);
static void POWER_STATE_HandleSoftKeyShortPress(uint32_t now_ms);
static void POWER_STATE_HandleSoftKeyLongPress(uint32_t now_ms);
static void POWER_STATE_RequestHardwareOff(void);
static void POWER_STATE_EnterQuickSettings(void);
static void POWER_STATE_EnterPowerOffConfirm(power_state_mode_t resume_mode);
static void POWER_STATE_ExitBlockingOverlay(void);
static void POWER_STATE_HandleBlockingButtonEvents(uint32_t now_ms);
static void POWER_STATE_HandleBlockingButtonEvent(const button_event_t *event,
                                                  uint32_t now_ms);
static void POWER_STATE_UpdateTimeouts(uint32_t now_ms);
static void POWER_STATE_BuildStatusModel(ui_statusbar_model_t *out_model);
static void POWER_STATE_DrawAndCommit(uint32_t now_ms);
static void POWER_STATE_DrawCurrentScreen(u8g2_t *u8g2, uint32_t now_ms);
static void POWER_STATE_DrawCenteredStr(u8g2_t *u8g2,
                                        int16_t y,
                                        const char *text);
static void POWER_STATE_DrawQuickSettings(u8g2_t *u8g2, uint32_t now_ms);
static void POWER_STATE_DrawQuickSettingsBottomMessage(u8g2_t *u8g2,
                                                       const char *text);
static void POWER_STATE_DrawConfirmOn(u8g2_t *u8g2, uint32_t now_ms);
static void POWER_STATE_DrawConfirmOff(u8g2_t *u8g2, uint32_t now_ms);

/* -------------------------------------------------------------------------- */
/* 내부 helper: wrap-safe due 판정                                             */
/* -------------------------------------------------------------------------- */
static uint8_t POWER_STATE_TimeDue(uint32_t now_ms, uint32_t due_ms)
{
    return ((int32_t)(now_ms - due_ms) >= 0) ? 1u : 0u;
}

/* -------------------------------------------------------------------------- */
/* 내부 helper: GPIO port clock enable                                         */
/*                                                                            */
/* CubeMX가 이미 GPIO clock를 켰더라도                                         */
/* Soft Power pin은 boot/app hand-off와 regenerate 내성을 위해                 */
/* 이 모듈에서 한 번 더 확실히 켠다.                                           */
/* -------------------------------------------------------------------------- */
static void POWER_STATE_EnableGpioClock(GPIO_TypeDef *port)
{
    if (port == GPIOA)
    {
        __HAL_RCC_GPIOA_CLK_ENABLE();
    }
    else if (port == GPIOB)
    {
        __HAL_RCC_GPIOB_CLK_ENABLE();
    }
    else if (port == GPIOC)
    {
        __HAL_RCC_GPIOC_CLK_ENABLE();
    }
    else if (port == GPIOD)
    {
        __HAL_RCC_GPIOD_CLK_ENABLE();
    }
    else if (port == GPIOE)
    {
        __HAL_RCC_GPIOE_CLK_ENABLE();
    }
#if defined(GPIOF)
    else if (port == GPIOF)
    {
        __HAL_RCC_GPIOF_CLK_ENABLE();
    }
#endif
#if defined(GPIOG)
    else if (port == GPIOG)
    {
        __HAL_RCC_GPIOG_CLK_ENABLE();
    }
#endif
#if defined(GPIOH)
    else if (port == GPIOH)
    {
        __HAL_RCC_GPIOH_CLK_ENABLE();
    }
#endif
}

/* -------------------------------------------------------------------------- */
/* 내부 helper: Soft Power GPIO runtime 보정                                   */
/*                                                                            */
/* OFF 핀                                                                      */
/* - output push-pull                                                          */
/* - 평상시 LOW                                                                */
/* - HIGH를 주면 SparkFun Soft Power Switch가 실제 전원 차단                   */
/*                                                                            */
/* PUSH 핀                                                                     */
/* - input + pull-up                                                           */
/* - SparkFun PUSH 출력은 오픈드레인 active-low 이므로                         */
/*   pull-up이 반드시 필요하다.                                                */
/* -------------------------------------------------------------------------- */
static void POWER_STATE_ConfigureSoftPowerPins(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    POWER_STATE_EnableGpioClock(SOFT_PWR_OFF_GPIO_Port);
    POWER_STATE_EnableGpioClock(SOFT_PWR_PUSH_GPIO_Port);

    memset(&GPIO_InitStruct, 0, sizeof(GPIO_InitStruct));

    /* ---------------------------------------------------------------------- */
    /* OFF 핀은 먼저 LOW를 써 둔 뒤 output mode로 잡는다.                      */
    /* 이렇게 해야 mode 전환 순간의 불필요한 HIGH glitch 가능성을 줄인다.      */
    /* ---------------------------------------------------------------------- */
    HAL_GPIO_WritePin(SOFT_PWR_OFF_GPIO_Port,
                      SOFT_PWR_OFF_Pin,
                      GPIO_PIN_RESET);

    GPIO_InitStruct.Pin = SOFT_PWR_OFF_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SOFT_PWR_OFF_GPIO_Port, &GPIO_InitStruct);

    /* ---------------------------------------------------------------------- */
    /* PUSH 핀은 내부 pull-up을 강제로 건다.                                   */
    /* CubeMX gpio.c가 NOPULL이어도 이 모듈이 런타임에서 덮어쓴다.             */
    /* ---------------------------------------------------------------------- */
    memset(&GPIO_InitStruct, 0, sizeof(GPIO_InitStruct));
    GPIO_InitStruct.Pin = SOFT_PWR_PUSH_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(SOFT_PWR_PUSH_GPIO_Port, &GPIO_InitStruct);
}

/* -------------------------------------------------------------------------- */
/* 내부 helper: Soft Power PUSH 논리 pressed 읽기                              */
/*                                                                            */
/* SparkFun PUSH는 active-low(open-drain) 기준으로 사용한다.                   */
/* 즉, GPIO가 RESET이면 "전원키 눌림" 으로 본다.                              */
/* -------------------------------------------------------------------------- */
static uint8_t POWER_STATE_ReadSoftKeyPressed(void)
{
    GPIO_PinState pin_state;

    pin_state = HAL_GPIO_ReadPin(SOFT_PWR_PUSH_GPIO_Port, SOFT_PWR_PUSH_Pin);

    return (pin_state == POWER_STATE_SOFTKEY_ACTIVE_LEVEL) ? 1u : 0u;
}

/* -------------------------------------------------------------------------- */
/* 내부 helper: Soft key runtime baseline 동기화                               */
/*                                                                            */
/* 사용 목적                                                                   */
/* - 부팅 직후 power button으로 켠 뒤 release되는 잔여 edge를                  */
/*   runtime short press로 잘못 해석하지 않게 막는다.                          */
/* - blocking overlay에서 빠져나올 때 현재 실제 GPIO 상태를                    */
/*   새 baseline으로 다시 맞춘다.                                              */
/* -------------------------------------------------------------------------- */
static void POWER_STATE_SoftKeySyncToCurrent(uint32_t now_ms)
{
    memset(&s_softkey_rt, 0, sizeof(s_softkey_rt));

    s_softkey_rt.sampled_pressed = POWER_STATE_ReadSoftKeyPressed();
    s_softkey_rt.stable_pressed  = s_softkey_rt.sampled_pressed;
    s_softkey_rt.press_start_ms  = now_ms;
}

/* -------------------------------------------------------------------------- */
/* 공개 API: 초기화                                                            */
/* -------------------------------------------------------------------------- */
void POWER_STATE_Init(void)
{
    POWER_STATE_ConfigureSoftPowerPins();

    s_power_mode = POWER_STATE_MODE_NONE;
    s_resume_mode_after_off_confirm = POWER_STATE_MODE_NONE;
    s_power_on_confirm_deadline_ms = 0u;
    s_last_boot_gate_draw_ms = 0u;
    s_boot_gate_draw_started = 0u;

    POWER_STATE_SoftKeySyncToCurrent(HAL_GetTick());
}

/* -------------------------------------------------------------------------- */
/* 공개 API: 부팅 직후 CONFIRM POWER ON 진입                                   */
/* -------------------------------------------------------------------------- */
void POWER_STATE_EnterPowerOnConfirm(uint32_t now_ms)
{
    s_power_mode = POWER_STATE_MODE_CONFIRM_ON;
    s_resume_mode_after_off_confirm = POWER_STATE_MODE_NONE;
    s_power_on_confirm_deadline_ms = now_ms + POWER_STATE_POWER_ON_CONFIRM_TIMEOUT_MS;
    s_last_boot_gate_draw_ms = 0u;
    s_boot_gate_draw_started = 0u;

    /* ---------------------------------------------------------------------- */
    /* 전원 ON 확인 화면에서는 soft power key 자체는 해석하지 않는다.          */
    /* 이유                                                                    */
    /* - 사용자가 막 power button으로 켠 직후이므로                            */
    /*   아직 release 잔상이 남아 있을 수 있다.                                */
    /* - 이 구간은 F1 / LONG F6만 받게 고정하는 편이 안정적이다.               */
    /* ---------------------------------------------------------------------- */
    POWER_STATE_SoftKeySyncToCurrent(now_ms);
}

/* -------------------------------------------------------------------------- */
/* 공개 API: UI blocking 여부                                                  */
/* -------------------------------------------------------------------------- */
bool POWER_STATE_IsUiBlocking(void)
{
    return (s_power_mode != POWER_STATE_MODE_NONE) ? true : false;
}

/* -------------------------------------------------------------------------- */
/* 공개 API: 현재 모드 getter                                                  */
/* -------------------------------------------------------------------------- */
power_state_mode_t POWER_STATE_GetMode(void)
{
    return s_power_mode;
}

/* -------------------------------------------------------------------------- */
/* 내부 helper: QUICK SETTINGS 진입                                             */
/* -------------------------------------------------------------------------- */
static void POWER_STATE_EnterQuickSettings(void)
{
    s_power_mode = POWER_STATE_MODE_QUICK_SETTINGS;
}

/* -------------------------------------------------------------------------- */
/* 내부 helper: POWER OFF 확인 화면 진입                                       */
/*                                                                            */
/* resume_mode                                                                 */
/* - F1 CANCEL 시 어떤 화면으로 돌아갈지 기록한다.                             */
/* - 평상시 메인 런타임 위에서 열렸다면 NONE으로 돌아간다.                     */
/* - QUICK SETTINGS 위에서 열렸다면 QUICK SETTINGS로 복귀한다.                */
/* -------------------------------------------------------------------------- */
static void POWER_STATE_EnterPowerOffConfirm(power_state_mode_t resume_mode)
{
    s_resume_mode_after_off_confirm = resume_mode;
    s_power_mode = POWER_STATE_MODE_CONFIRM_OFF;
}

/* -------------------------------------------------------------------------- */
/* 내부 helper: blocking overlay 종료                                          */
/*                                                                            */
/* 종료 후에는 soft key baseline을 현재 물리 상태로 다시 맞춘다.               */
/* 이렇게 해야 방금 overlay를 연 long-press의 release가                        */
/* 새 short press로 잘못 바뀌지 않는다.                                        */
/* -------------------------------------------------------------------------- */
static void POWER_STATE_ExitBlockingOverlay(void)
{
    s_power_mode = POWER_STATE_MODE_NONE;
    s_resume_mode_after_off_confirm = POWER_STATE_MODE_NONE;
    POWER_STATE_SoftKeySyncToCurrent(HAL_GetTick());
}

/* -------------------------------------------------------------------------- */
/* 내부 helper: 실제 하드웨어 전원 차단                                        */
/*                                                                            */
/* SparkFun Soft Power Switch Mk2 기준                                         */
/* - OFF 핀 HIGH -> 전원 차단 요청                                             */
/*                                                                            */
/* 동작 순서                                                                   */
/* 1) OFF 핀을 output/high로 보장                                              */
/* 2) HIGH 기록                                                                */
/* 3) 무한 루프                                                                 */
/*                                                                            */
/* 정상 배선이면 이 함수 안에서 곧바로 전원이 꺼져 return 하지 않는다.         */
/* -------------------------------------------------------------------------- */
static void POWER_STATE_RequestHardwareOff(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    s_power_mode = POWER_STATE_MODE_POWERING_OFF;

    POWER_STATE_EnableGpioClock(SOFT_PWR_OFF_GPIO_Port);

    memset(&GPIO_InitStruct, 0, sizeof(GPIO_InitStruct));
    GPIO_InitStruct.Pin = SOFT_PWR_OFF_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SOFT_PWR_OFF_GPIO_Port, &GPIO_InitStruct);

    HAL_GPIO_WritePin(SOFT_PWR_OFF_GPIO_Port,
                      SOFT_PWR_OFF_Pin,
                      GPIO_PIN_SET);

    for (;;)
    {
        __NOP();
    }
}

/* -------------------------------------------------------------------------- */
/* 내부 helper: Soft key short press action                                    */
/*                                                                            */
/* 요구사항                                                                    */
/* - 런타임 어떤 화면에서든 짧게 누르면 QUICK SETTINGS STUB 진입               */
/* -------------------------------------------------------------------------- */
static void POWER_STATE_HandleSoftKeyShortPress(uint32_t now_ms)
{
    (void)now_ms;

    if (s_power_mode == POWER_STATE_MODE_NONE)
    {
        POWER_STATE_EnterQuickSettings();
    }
}

/* -------------------------------------------------------------------------- */
/* 내부 helper: Soft key long press action                                     */
/*                                                                            */
/* 요구사항                                                                    */
/* - 런타임 어떤 화면에서든 길게 누르면 POWER OFF 확인 화면 진입               */
/* -------------------------------------------------------------------------- */
static void POWER_STATE_HandleSoftKeyLongPress(uint32_t now_ms)
{
    (void)now_ms;

    if (s_power_mode == POWER_STATE_MODE_NONE)
    {
        POWER_STATE_EnterPowerOffConfirm(POWER_STATE_MODE_NONE);
    }
    else if (s_power_mode == POWER_STATE_MODE_QUICK_SETTINGS)
    {
        POWER_STATE_EnterPowerOffConfirm(POWER_STATE_MODE_QUICK_SETTINGS);
    }
}

/* -------------------------------------------------------------------------- */
/* 내부 helper: Soft key polling + debounce + short/long 판정                  */
/*                                                                            */
/* 이 로직은                                                                    */
/* - 일반 런타임에서는 항상 돌아간다.                                          */
/* - 전원 ON 확인 화면에서는 의도적으로 호출하지 않는다.                       */
/* - POWER OFF 확인 화면에서는 추가 진입/취소 충돌을 막기 위해 동작시키지 않는다.*/
/* -------------------------------------------------------------------------- */
static void POWER_STATE_ServiceSoftKeyRuntime(uint32_t now_ms)
{
    uint8_t current_pressed;

    if ((s_power_mode != POWER_STATE_MODE_NONE) &&
        (s_power_mode != POWER_STATE_MODE_QUICK_SETTINGS))
    {
        return;
    }

    current_pressed = POWER_STATE_ReadSoftKeyPressed();

    /* ---------------------------------------------------------------------- */
    /* raw edge가 보이면 debounce 타이머를 다시 잡는다.                        */
    /* ---------------------------------------------------------------------- */
    if (current_pressed != s_softkey_rt.sampled_pressed)
    {
        s_softkey_rt.sampled_pressed = current_pressed;
        s_softkey_rt.debounce_active = 1u;
        s_softkey_rt.debounce_due_ms = now_ms + POWER_STATE_SOFTKEY_DEBOUNCE_MS;
    }

    /* ---------------------------------------------------------------------- */
    /* debounce 만료 후 stable 상태를 확정한다.                                */
    /* ---------------------------------------------------------------------- */
    if ((s_softkey_rt.debounce_active != 0u) &&
        (POWER_STATE_TimeDue(now_ms, s_softkey_rt.debounce_due_ms) != 0u))
    {
        s_softkey_rt.debounce_active = 0u;

        if (s_softkey_rt.stable_pressed != s_softkey_rt.sampled_pressed)
        {
            s_softkey_rt.stable_pressed = s_softkey_rt.sampled_pressed;

            if (s_softkey_rt.stable_pressed != 0u)
            {
                s_softkey_rt.press_start_ms = now_ms;
                s_softkey_rt.long_reported  = 0u;
            }
            else
            {
                uint32_t hold_ms;

                hold_ms = now_ms - s_softkey_rt.press_start_ms;

                if ((s_softkey_rt.long_reported == 0u) &&
                    (hold_ms < POWER_STATE_SOFTKEY_LONG_PRESS_MS))
                {
                    POWER_STATE_HandleSoftKeyShortPress(now_ms);
                }
            }
        }
    }

    /* ---------------------------------------------------------------------- */
    /* stable pressed 유지 중이면 long press를 딱 한 번만 발생시킨다.          */
    /* ---------------------------------------------------------------------- */
    if ((s_softkey_rt.stable_pressed != 0u) &&
        (s_softkey_rt.long_reported == 0u) &&
        (POWER_STATE_TimeDue(now_ms,
                             s_softkey_rt.press_start_ms +
                             POWER_STATE_SOFTKEY_LONG_PRESS_MS) != 0u))
    {
        s_softkey_rt.long_reported = 1u;
        POWER_STATE_HandleSoftKeyLongPress(now_ms);
    }
}

/* -------------------------------------------------------------------------- */
/* 내부 helper: blocking 상태에서 BUTTON queue 소비                            */
/*                                                                            */
/* 이 함수는                                                                    */
/* - QUICK SETTINGS STUB                                                      */
/* - POWER ON 확인                                                            */
/* - POWER OFF 확인                                                           */
/* 상태에서만 queue를 소모한다.                                                */
/*                                                                            */
/* 이렇게 해야 기존 UI 엔진이 같은 F1/F6 이벤트를 중복 처리하지 않는다.        */
/* -------------------------------------------------------------------------- */
static void POWER_STATE_HandleBlockingButtonEvents(uint32_t now_ms)
{
    button_event_t event;

    if (s_power_mode == POWER_STATE_MODE_NONE)
    {
        return;
    }

    while (Button_PopEvent(&event) != false)
    {
        POWER_STATE_HandleBlockingButtonEvent(&event, now_ms);
    }
}

/* -------------------------------------------------------------------------- */
/* 내부 helper: blocking 상태별 버튼 해석                                      */
/*                                                                            */
/* CONFIRM ON                                                                  */
/* - F1 short      -> 즉시 전원 OFF                                            */
/* - F6 long       -> 앱 런타임 계속 진행                                       */
/*                                                                            */
/* QUICK SETTINGS                                                              */
/* - F1 short      -> 이전 화면 복귀                                            */
/*                                                                            */
/* CONFIRM OFF                                                                 */
/* - F1 short      -> 직전 화면으로 복귀                                        */
/* - F6 long       -> 즉시 전원 OFF                                            */
/* -------------------------------------------------------------------------- */
static void POWER_STATE_HandleBlockingButtonEvent(const button_event_t *event,
                                                  uint32_t now_ms)
{
    if (event == 0)
    {
        return;
    }

    switch (s_power_mode)
    {
    case POWER_STATE_MODE_CONFIRM_ON:
        if ((event->id == BUTTON_ID_1) &&
            (event->type == BUTTON_EVENT_SHORT_PRESS))
        {
            POWER_STATE_RequestHardwareOff();
        }
        else if ((event->id == BUTTON_ID_6) &&
                 (event->type == BUTTON_EVENT_LONG_PRESS))
        {
            s_power_on_confirm_deadline_ms = 0u;
            s_power_mode = POWER_STATE_MODE_NONE;
            POWER_STATE_SoftKeySyncToCurrent(now_ms);
        }
        break;

    case POWER_STATE_MODE_QUICK_SETTINGS:
        if ((event->id == BUTTON_ID_1) &&
            (event->type == BUTTON_EVENT_SHORT_PRESS))
        {
            POWER_STATE_ExitBlockingOverlay();
        }
        break;

    case POWER_STATE_MODE_CONFIRM_OFF:
        if ((event->id == BUTTON_ID_1) &&
            (event->type == BUTTON_EVENT_SHORT_PRESS))
        {
            if (s_resume_mode_after_off_confirm == POWER_STATE_MODE_QUICK_SETTINGS)
            {
                s_power_mode = POWER_STATE_MODE_QUICK_SETTINGS;
                POWER_STATE_SoftKeySyncToCurrent(now_ms);
            }
            else
            {
                POWER_STATE_ExitBlockingOverlay();
            }
        }
        else if ((event->id == BUTTON_ID_6) &&
                 (event->type == BUTTON_EVENT_LONG_PRESS))
        {
            POWER_STATE_RequestHardwareOff();
        }
        break;

    case POWER_STATE_MODE_NONE:
    case POWER_STATE_MODE_POWERING_OFF:
    default:
        break;
    }
}

/* -------------------------------------------------------------------------- */
/* 내부 helper: timeout 처리                                                   */
/*                                                                            */
/* 현재는 CONFIRM POWER ON 화면만 countdown timeout을 가진다.                  */
/* timeout 시 정책                                                             */
/* - 30초 동안 사용자가 아무 확인도 하지 않으면                               */
/*   전원을 계속 먹고 기다리지 않고 OFF로 내린다.                              */
/* -------------------------------------------------------------------------- */
static void POWER_STATE_UpdateTimeouts(uint32_t now_ms)
{
    if (s_power_mode == POWER_STATE_MODE_CONFIRM_ON)
    {
        if ((s_power_on_confirm_deadline_ms != 0u) &&
            (POWER_STATE_TimeDue(now_ms, s_power_on_confirm_deadline_ms) != 0u))
        {
            POWER_STATE_RequestHardwareOff();
        }
    }
}

/* -------------------------------------------------------------------------- */
/* 내부 helper: status bar model 구성                                          */
/*                                                                            */
/* QUICK SETTINGS는 상단바가 있는 뷰포트 요구사항이 있으므로                   */
/* ui_engine.c가 하던 lightweight model 조립을 이 모듈이 로컬로 한 번 더 한다.*/
/* -------------------------------------------------------------------------- */
static void POWER_STATE_BuildStatusModel(ui_statusbar_model_t *out_model)
{
    if (out_model == 0)
    {
        return;
    }

    memset(out_model, 0, sizeof(*out_model));

    out_model->gps_fix = g_app_state.gps.fix;

    out_model->temp_status_flags = g_app_state.ds18b20.status_flags;
    out_model->temp_c_x100 = g_app_state.ds18b20.raw.temp_c_x100;
    out_model->temp_f_x100 = g_app_state.ds18b20.raw.temp_f_x100;
    out_model->temp_last_error = g_app_state.ds18b20.debug.last_error;

    out_model->sd_inserted = g_app_state.sd.detect_stable_present;
    out_model->sd_mounted = g_app_state.sd.mounted;
    out_model->sd_initialized = g_app_state.sd.initialized;

    out_model->time_valid   = g_app_state.clock.rtc_time_valid;
    out_model->time_year    = g_app_state.clock.local.year;
    out_model->time_month   = g_app_state.clock.local.month;
    out_model->time_day     = g_app_state.clock.local.day;
    out_model->time_hour    = g_app_state.clock.local.hour;
    out_model->time_minute  = g_app_state.clock.local.min;
    out_model->time_weekday = g_app_state.clock.local.weekday;

    /* ---------------------------------------------------------------------- */
    /* 이 오버레이는 APP_STATE에 별도 레코딩/BT 상태를 만들지 않는다.          */
    /* 기존 UI 엔진과 최대한 비슷한 기본값만 넣어 상단바가 깨지지 않게 한다.    */
    /* ---------------------------------------------------------------------- */
    out_model->record_state = UI_RECORD_STATE_STOP;
    out_model->imperial_units = 0u;
    out_model->bluetooth_stub_state = UI_BT_STUB_ON;
}

/* -------------------------------------------------------------------------- */
/* 내부 helper: 가운데 정렬 문자열 draw                                        */
/* -------------------------------------------------------------------------- */
static void POWER_STATE_DrawCenteredStr(u8g2_t *u8g2,
                                        int16_t y,
                                        const char *text)
{
    int16_t x;
    uint16_t width;

    if ((u8g2 == 0) || (text == 0))
    {
        return;
    }

    width = u8g2_GetStrWidth(u8g2, text);

    if (width >= UI_LCD_W)
    {
        x = 0;
    }
    else
    {
        x = (int16_t)((UI_LCD_W - width) / 2);
    }

    if (x < 0)
    {
        x = 0;
    }

    u8g2_DrawStr(u8g2, (u8g2_uint_t)x, (u8g2_uint_t)y, text);
}

/* -------------------------------------------------------------------------- */
/* 내부 helper: QUICK SETTINGS용 하단 메시지 바                                */
/*                                                                            */
/* 그리는 위치                                                                  */
/* - 화면 맨 아래 8px 높이의 black strip                                       */
/* - 중앙에 white text 하나만 배치                                             */
/*                                                                            */
/* 이번 stub 화면에서는 복잡한 6-segment key map 대신                          */
/* "F1 BACK" 한 줄만 명시해도 사용성이 충분하다.                               */
/* -------------------------------------------------------------------------- */
static void POWER_STATE_DrawQuickSettingsBottomMessage(u8g2_t *u8g2,
                                                       const char *text)
{
    uint8_t y0;
    uint8_t text_y;
    uint16_t width;
    int16_t x;

    if ((u8g2 == 0) || (text == 0))
    {
        return;
    }

    y0 = (uint8_t)(UI_LCD_H - UI_BOTTOMBAR_H);
    text_y = (uint8_t)(UI_LCD_H - 1u);

    /* ---------------------------------------------------------------------- */
    /* 바 전체를 검은 박스로 깔고                                              */
    /* 그 위에 흰 글자를 올리는 기존 bottom bar 계열의 시각 언어를 따른다.     */
    /* ---------------------------------------------------------------------- */
    u8g2_SetDrawColor(u8g2, 1u);
    u8g2_DrawBox(u8g2, 0u, y0, UI_LCD_W, UI_BOTTOMBAR_H);

    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    width = u8g2_GetStrWidth(u8g2, text);

    if (width >= UI_LCD_W)
    {
        x = 0;
    }
    else
    {
        x = (int16_t)((UI_LCD_W - width) / 2u);
    }

    if (x < 0)
    {
        x = 0;
    }

    u8g2_SetDrawColor(u8g2, 0u);
    u8g2_DrawStr(u8g2, (u8g2_uint_t)x, text_y, text);
    u8g2_SetDrawColor(u8g2, 1u);
}

/* -------------------------------------------------------------------------- */
/* 내부 helper: QUICK SETTINGS STUB draw                                       */
/*                                                                            */
/* 화면 구성                                                                   */
/* 1) 맨 위: 실제 status bar                                                   */
/* 2) 본문: status 아래 ~ bottom bar 위                                       */
/* 3) 맨 아래: black bottom message bar("F1 BACK")                           */
/*                                                                            */
/* 본문 내용                                                                   */
/* - 중앙에 "QUICK SETTINGS"                                                  */
/* - 그 아래에 "STUB!"                                                       */
/* -------------------------------------------------------------------------- */
static void POWER_STATE_DrawQuickSettings(u8g2_t *u8g2, uint32_t now_ms)
{
    ui_statusbar_model_t status_model;
    int16_t body_top_y;
    int16_t body_bottom_y;
    int16_t body_center_y;

    if (u8g2 == 0)
    {
        return;
    }

    POWER_STATE_BuildStatusModel(&status_model);
    UI_StatusBar_Draw(u8g2, &status_model, now_ms);

    body_top_y = (int16_t)UI_StatusBar_GetReservedHeight(u8g2);
    body_top_y = (int16_t)(body_top_y + UI_STATUSBAR_GAP_H);
    body_bottom_y = (int16_t)(UI_LCD_H - UI_BOTTOMBAR_GAP_H - UI_BOTTOMBAR_H);
    body_center_y = (int16_t)((body_top_y + body_bottom_y) / 2);

    /* ---------------------------------------------------------------------- */
    /* 본문 중앙 타이틀                                                         */
    /* - QUICK SETTINGS : body 중심보다 조금 위                                 */
    /* - STUB!          : body 중심보다 조금 아래                               */
    /* ---------------------------------------------------------------------- */
    u8g2_SetDrawColor(u8g2, 1u);
    u8g2_SetFont(u8g2, u8g2_font_6x13B_tr);
    POWER_STATE_DrawCenteredStr(u8g2, (int16_t)(body_center_y - 6), "QUICK SETTINGS");

    u8g2_SetFont(u8g2, u8g2_font_6x12_mf);
    POWER_STATE_DrawCenteredStr(u8g2, (int16_t)(body_center_y + 12), "STUB!");

    POWER_STATE_DrawQuickSettingsBottomMessage(u8g2, "F1 BACK");
}

/* -------------------------------------------------------------------------- */
/* 내부 helper: POWER ON 확인 화면 draw                                        */
/*                                                                            */
/* 전체 화면 레이아웃                                                          */
/* - 상하단바 없는 FULLSCREEN                                                  */
/* - 큰 power icon                                                             */
/* - 큰 제목 "CONFIRM POWER ON?"                                              */
/* - countdown line                                                            */
/* - 하단 설명 "F1 = OFF" / "LONG PRESS F6 = POWER ON"                     */
/* -------------------------------------------------------------------------- */
static void POWER_STATE_DrawConfirmOn(u8g2_t *u8g2, uint32_t now_ms)
{
    uint32_t remaining_ms;
    uint32_t remaining_sec;
    int16_t icon_x;
    char line[32];

    if (u8g2 == 0)
    {
        return;
    }

    if (POWER_STATE_TimeDue(now_ms, s_power_on_confirm_deadline_ms) != 0u)
    {
        remaining_ms = 0u;
    }
    else
    {
        remaining_ms = s_power_on_confirm_deadline_ms - now_ms;
    }

    remaining_sec = (remaining_ms + 999u) / 1000u;

    icon_x = (int16_t)((UI_LCD_W - POWER_STATE_ICON_POWER_W) / 2);

    u8g2_SetDrawColor(u8g2, 1u);
    u8g2_DrawXBM(u8g2,
                 (u8g2_uint_t)icon_x,
                 6u,
                 POWER_STATE_ICON_POWER_W,
                 POWER_STATE_ICON_POWER_H,
                 s_power_icon_48x48);

    u8g2_SetFont(u8g2, u8g2_font_6x13B_tr);
    POWER_STATE_DrawCenteredStr(u8g2, 68, "CONFIRM POWER ON?");

    u8g2_SetFont(u8g2, u8g2_font_6x12_mf);
    (void)snprintf(line, sizeof(line), "AUTO OFF IN %lus", (unsigned long)remaining_sec);
    POWER_STATE_DrawCenteredStr(u8g2, 86, line);

    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    POWER_STATE_DrawCenteredStr(u8g2, 108, "F1 = OFF");
    POWER_STATE_DrawCenteredStr(u8g2, 120, "LONG PRESS F6 = POWER ON");
}

/* -------------------------------------------------------------------------- */
/* 내부 helper: POWER OFF 확인 화면 draw                                       */
/*                                                                            */
/* 전체 화면 레이아웃                                                          */
/* - 상하단바 없는 FULLSCREEN                                                  */
/* - 큰 power icon                                                             */
/* - 큰 제목 "POWER OFF"                                                     */
/* - 그 아래 "CONFIRM?"                                                      */
/* - 하단 설명 "F1 = CANCEL" / "LONG PRESS F6 = OFF"                       */
/* -------------------------------------------------------------------------- */
static void POWER_STATE_DrawConfirmOff(u8g2_t *u8g2, uint32_t now_ms)
{
    int16_t icon_x;

    (void)now_ms;

    if (u8g2 == 0)
    {
        return;
    }

    icon_x = (int16_t)((UI_LCD_W - POWER_STATE_ICON_POWER_W) / 2);

    u8g2_SetDrawColor(u8g2, 1u);
    u8g2_DrawXBM(u8g2,
                 (u8g2_uint_t)icon_x,
                 6u,
                 POWER_STATE_ICON_POWER_W,
                 POWER_STATE_ICON_POWER_H,
                 s_power_icon_48x48);

    u8g2_SetFont(u8g2, u8g2_font_6x13B_tr);
    POWER_STATE_DrawCenteredStr(u8g2, 68, "POWER OFF");

    u8g2_SetFont(u8g2, u8g2_font_6x12_mf);
    POWER_STATE_DrawCenteredStr(u8g2, 86, "CONFIRM?");

    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    POWER_STATE_DrawCenteredStr(u8g2, 108, "F1 = CANCEL");
    POWER_STATE_DrawCenteredStr(u8g2, 120, "LONG PRESS F6 = OFF");
}

/* -------------------------------------------------------------------------- */
/* 내부 helper: 모드별 화면 선택 draw                                          */
/* -------------------------------------------------------------------------- */
static void POWER_STATE_DrawCurrentScreen(u8g2_t *u8g2, uint32_t now_ms)
{
    switch (s_power_mode)
    {
    case POWER_STATE_MODE_CONFIRM_ON:
        POWER_STATE_DrawConfirmOn(u8g2, now_ms);
        break;

    case POWER_STATE_MODE_QUICK_SETTINGS:
        POWER_STATE_DrawQuickSettings(u8g2, now_ms);
        break;

    case POWER_STATE_MODE_CONFIRM_OFF:
        POWER_STATE_DrawConfirmOff(u8g2, now_ms);
        break;

    case POWER_STATE_MODE_POWERING_OFF:
        u8g2_SetFont(u8g2, u8g2_font_6x13B_tr);
        POWER_STATE_DrawCenteredStr(u8g2, 64, "POWERING OFF...");
        break;

    case POWER_STATE_MODE_NONE:
    default:
        break;
    }
}

/* -------------------------------------------------------------------------- */
/* 내부 helper: 현재 전원 오버레이 1프레임 draw + commit                       */
/* -------------------------------------------------------------------------- */
static void POWER_STATE_DrawAndCommit(uint32_t now_ms)
{
    u8g2_t *u8g2;

    u8g2 = U8G2_UC1608_GetHandle();
    if (u8g2 == 0)
    {
        return;
    }

    u8g2_ClearBuffer(u8g2);
    POWER_STATE_DrawCurrentScreen(u8g2, now_ms);
    U8G2_UC1608_CommitBuffer();
}

/* -------------------------------------------------------------------------- */
/* 공개 API: boot gate task                                                    */
/*                                                                            */
/* 이 함수는 frame token이 없는 구간에서 쓴다.                                 */
/* 따라서 내부 redraw 속도를 시간 기반(33ms)으로만 제한한다.                   */
/* -------------------------------------------------------------------------- */
void POWER_STATE_TaskBootGate(uint32_t now_ms)
{
    POWER_STATE_HandleBlockingButtonEvents(now_ms);
    POWER_STATE_UpdateTimeouts(now_ms);

    if (s_power_mode == POWER_STATE_MODE_NONE)
    {
        return;
    }

    if ((s_boot_gate_draw_started != 0u) &&
        ((uint32_t)(now_ms - s_last_boot_gate_draw_ms) <
         POWER_STATE_BOOT_GATE_REDRAW_PERIOD_MS))
    {
        return;
    }

    s_boot_gate_draw_started = 1u;
    s_last_boot_gate_draw_ms = now_ms;

    POWER_STATE_DrawAndCommit(now_ms);
}

/* -------------------------------------------------------------------------- */
/* 공개 API: 일반 런타임 task                                                  */
/*                                                                            */
/* 순서                                                                        */
/* 1) Soft key short/long polling                                              */
/* 2) blocking overlay 상태면 F1/F6 queue 소비                                 */
/* 3) timeout 처리                                                             */
/* 4) blocking overlay 상태면 frame token을 얻은 뒤 draw                       */
/* -------------------------------------------------------------------------- */
void POWER_STATE_Task(uint32_t now_ms)
{
    POWER_STATE_ServiceSoftKeyRuntime(now_ms);
    POWER_STATE_HandleBlockingButtonEvents(now_ms);
    POWER_STATE_UpdateTimeouts(now_ms);

    if (s_power_mode == POWER_STATE_MODE_NONE)
    {
        return;
    }

    if (U8G2_UC1608_TryAcquireFrameToken() == 0u)
    {
        return;
    }

    POWER_STATE_DrawAndCommit(now_ms);
}
