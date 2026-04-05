#include "FW_Power.h"

#include <string.h>

/* -------------------------------------------------------------------------- */
/* Soft Power Switch pin compatibility                                         */
/*                                                                            */
/* 정상적인 경우                                                               */
/* - bootloader .ioc regenerate 후 main.h 에                                  */
/*   SOFT_PWR_OFF_Pin / SOFT_PWR_PUSH_Pin 이 생겨 있어야 한다.                 */
/*                                                                            */
/* 하지만 현재 업로드된 레포 상태에서는                                        */
/* - bootloader main.h 가 아직 예전 POWER_HOLD(PE2) 를 유지하고 있다.          */
/*                                                                            */
/* 따라서 이 파일은 fallback 매크로를 함께 둬서                               */
/* - 아직 bootloader ioc 를 regenerate 하지 않았더라도                         */
/* - 실제 배선 기준(PE3=OFF, PE4=PUSH) 으로 런타임 보정을 수행한다.            */
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
/* 내부 helper 선언                                                            */
/* -------------------------------------------------------------------------- */
static void FW_POWER_EnableGpioClock(GPIO_TypeDef *port);
static void FW_POWER_ConfigureOffLow(void);
static void FW_POWER_ConfigurePushInputPullup(void);

/* -------------------------------------------------------------------------- */
/* 내부 helper: GPIO clock enable                                              */
/*                                                                            */
/* bootloader jump 직전 / 직후 상황에서는                                      */
/* 어떤 클럭이 이미 켜져 있는지 가정하지 않는 편이 안전하므로,                  */
/* 매번 필요한 포트 클럭을 다시 켠다.                                          */
/* -------------------------------------------------------------------------- */
static void FW_POWER_EnableGpioClock(GPIO_TypeDef *port)
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
/* 내부 helper: OFF 핀을 LOW 출력으로 강제                                     */
/*                                                                            */
/* SparkFun Soft Power Switch Mk2 규칙                                         */
/* - OFF HIGH  : 전원 차단                                                     */
/* - OFF LOW   : 정상 유지                                                     */
/*                                                                            */
/* 따라서 bootloader 와 app 어느 쪽에서도                                      */
/* jump 중에는 이 핀을 HIGH로 만들지 않는 것이 핵심이다.                      */
/* -------------------------------------------------------------------------- */
static void FW_POWER_ConfigureOffLow(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    FW_POWER_EnableGpioClock(SOFT_PWR_OFF_GPIO_Port);

    /* ---------------------------------------------------------------------- */
    /* mode 변경 전에 먼저 LOW를 써 둔다.                                     */
    /* 이렇게 해야 output으로 전환되는 순간의 불필요한 glitch 가능성을         */
    /* 줄일 수 있다.                                                           */
    /* ---------------------------------------------------------------------- */
    HAL_GPIO_WritePin(SOFT_PWR_OFF_GPIO_Port,
                      SOFT_PWR_OFF_Pin,
                      GPIO_PIN_RESET);

    memset(&GPIO_InitStruct, 0, sizeof(GPIO_InitStruct));
    GPIO_InitStruct.Pin = SOFT_PWR_OFF_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SOFT_PWR_OFF_GPIO_Port, &GPIO_InitStruct);

    /* ---------------------------------------------------------------------- */
    /* init 직후에도 LOW를 한 번 더 명시적으로 남겨 둔다.                     */
    /* ---------------------------------------------------------------------- */
    HAL_GPIO_WritePin(SOFT_PWR_OFF_GPIO_Port,
                      SOFT_PWR_OFF_Pin,
                      GPIO_PIN_RESET);
}

/* -------------------------------------------------------------------------- */
/* 내부 helper: PUSH 핀을 input pull-up 으로 강제                              */
/*                                                                            */
/* SparkFun PUSH는 open-drain active-low 이므로                                */
/* MCU 쪽에서는 pull-up이 반드시 필요하다.                                     */
/*                                                                            */
/* 현재 bootloader는 이 PUSH 핀을 직접 사용하지 않지만,                        */
/* 전기적으로 떠 있는 입력을 남겨 두는 것보다                                  */
/* boot/runtime 모두 같은 정책으로 정렬하는 편이 안전하다.                    */
/* -------------------------------------------------------------------------- */
static void FW_POWER_ConfigurePushInputPullup(void)
{
    GPIO_InitTypeDef GPIO_InitStruct;

    FW_POWER_EnableGpioClock(SOFT_PWR_PUSH_GPIO_Port);

    memset(&GPIO_InitStruct, 0, sizeof(GPIO_InitStruct));
    GPIO_InitStruct.Pin = SOFT_PWR_PUSH_Pin;
    GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
    GPIO_InitStruct.Pull = GPIO_PULLUP;
    HAL_GPIO_Init(SOFT_PWR_PUSH_GPIO_Port, &GPIO_InitStruct);
}

/* -------------------------------------------------------------------------- */
/* 공개 API: bootloader runtime 초기화                                         */
/* -------------------------------------------------------------------------- */
void FW_POWER_Init(void)
{
    FW_POWER_ConfigureOffLow();
    FW_POWER_ConfigurePushInputPullup();
}

/* -------------------------------------------------------------------------- */
/* 공개 API: app jump 직전 OFF 핀 재보정                                       */
/*                                                                            */
/* 이 함수는 app로 넘어가기 직전 마지막 안전핀 역할만 수행한다.                */
/* PUSH 핀은 jump 경계 안정성에 직접 영향이 없으므로                           */
/* 여기서는 OFF 핀만 다시 LOW로 확정한다.                                      */
/* -------------------------------------------------------------------------- */
void FW_POWER_PrepareForAppJump(void)
{
    FW_POWER_ConfigureOffLow();
}
