/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    stm32f4xx_it.c
  * @brief   Interrupt Service Routines.
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "stm32f4xx_it.h"
/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "APP_FAULT.h"
#include "Ublox_GPS.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN TD */

/* USER CODE END TD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN PV */

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


/* -------------------------------------------------------------------------- */
/*  Cube가 생성하는 기본 fault handler 심볼을 다른 이름으로 바꾼다.             */
/*                                                                            */
/*  목적                                                                      */
/*  - 이 파일 아래쪽에 생성된 일반 C handler 정의와                            */
/*    APP_FAULT_VECTORS.c의 진짜 naked handler 정의가                          */
/*    이름 충돌 없이 동시에 존재하게 만든다.                                   */
/*                                                                            */
/*  결과                                                                      */
/*  - startup vector가 찾는 진짜 이름                                          */
/*      NMI_Handler / HardFault_Handler / MemManage_Handler /                  */
/*      BusFault_Handler / UsageFault_Handler                                  */
/*    은 APP_FAULT_VECTORS.c가 담당한다.                                       */
/*  - Cube가 생성한 기존 함수들은                                              */
/*    아래 alias 이름으로 바뀌어 링크만 조용히 통과한다.                       */
/*                                                                            */
/*  이 블록은 USER CODE 0 안에 있으므로                                        */
/*  Cube 재생성 후에도 다시 붙여 넣기 쉽다.                                    */
/* -------------------------------------------------------------------------- */
#define NMI_Handler         CubeMX_NMI_Handler_Unused
#define HardFault_Handler   CubeMX_HardFault_Handler_Unused
#define MemManage_Handler   CubeMX_MemManage_Handler_Unused
#define BusFault_Handler    CubeMX_BusFault_Handler_Unused
#define UsageFault_Handler  CubeMX_UsageFault_Handler_Unused

/* --------------------------------------------------------------------------
 *  Cortex-M fault handler는 "정확한 예외 스택 프레임"을 받아야 하므로
 *  naked wrapper로 구현한다.
 *
 *  여기서 stack 선택 규칙은 Cortex-M 표준 패턴 그대로다.
 *    - LR(EXC_RETURN) bit[2] == 0 : MSP 사용
 *    - LR(EXC_RETURN) bit[2] == 1 : PSP 사용
 *
 *  wrapper는 r0에 stack frame 시작 주소를, r1에 EXC_RETURN 값을 담아서
 *  APP_FAULT_xxxC()로 점프한다.
 *
 *  중요한 점
 *  - fault가 난 뒤 while(1)로 멈춰 버리면, 다음 부팅 때 원인을 보기 어렵다.
 *  - 따라서 wrapper는 APP_FAULT 쪽 C 엔트리로 즉시 넘겨서
 *      1) fault register + stacked frame 기록
 *      2) system reset
 *    순서로 마무리하게 한다.
 * ------------------------------------------------------------------------*/

__attribute__((naked)) static void APP_FAULT_DispatchHardFault(void)
{
  __asm volatile
  (
    "tst lr, #4            \n"
    "ite eq                \n"
    "mrseq r0, msp         \n"
    "mrsne r0, psp         \n"
    "mov r1, lr            \n"
    "b APP_FAULT_HardFaultC\n"
  );
}

__attribute__((naked)) static void APP_FAULT_DispatchMemManage(void)
{
  __asm volatile
  (
    "tst lr, #4             \n"
    "ite eq                 \n"
    "mrseq r0, msp          \n"
    "mrsne r0, psp          \n"
    "mov r1, lr             \n"
    "b APP_FAULT_MemManageC \n"
  );
}

__attribute__((naked)) static void APP_FAULT_DispatchBusFault(void)
{
  __asm volatile
  (
    "tst lr, #4           \n"
    "ite eq               \n"
    "mrseq r0, msp        \n"
    "mrsne r0, psp        \n"
    "mov r1, lr           \n"
    "b APP_FAULT_BusFaultC\n"
  );
}

__attribute__((naked)) static void APP_FAULT_DispatchUsageFault(void)
{
  __asm volatile
  (
    "tst lr, #4             \n"
    "ite eq                 \n"
    "mrseq r0, msp          \n"
    "mrsne r0, psp          \n"
    "mov r1, lr             \n"
    "b APP_FAULT_UsageFaultC\n"
  );
}



#if defined(__GNUC__)
#define APP_FAULT_DECLARE_WRAPPER(handler_name, c_entry)                 \
__attribute__((naked)) void handler_name(void)                           \
{                                                                        \
  __asm volatile(                                                        \
    "tst lr, #4      \n"                                                \
    "ite eq          \n"                                                \
    "mrseq r0, msp   \n"                                                \
    "mrsne r0, psp   \n"                                                \
    "mov r1, lr      \n"                                                \
    "b " #c_entry "  \n");                                              \
}

__attribute__((naked)) static void APP_FAULT_DispatchNmi(void)
{
  __asm volatile
  (
    "tst lr, #4      \n"
    "ite eq          \n"
    "mrseq r0, msp   \n"
    "mrsne r0, psp   \n"
    "mov r1, lr      \n"
    "b APP_FAULT_NmiC\n"
  );
}

#else
#define APP_FAULT_DECLARE_WRAPPER(handler_name, c_entry)                 \
void handler_name(void)                                                  \
{                                                                        \
  c_entry(0u, 0u);                                                       \
}
#endif


/* USER CODE END 0 */

/* External variables --------------------------------------------------------*/
extern DMA_HandleTypeDef hdma_dac1;
extern TIM_HandleTypeDef htim7;
extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart2;
extern UART_HandleTypeDef huart3;
/* USER CODE BEGIN EV */

/* USER CODE END EV */

/******************************************************************************/
/*           Cortex-M4 Processor Interruption and Exception Handlers          */
/******************************************************************************/
/**
  * @brief This function handles Non maskable interrupt.
  */
void NMI_Handler(void)
{
  /* USER CODE BEGIN NonMaskableInt_IRQn 0 */

  /* USER CODE END NonMaskableInt_IRQn 0 */
  /* USER CODE BEGIN NonMaskableInt_IRQn 1 */

	   APP_FAULT_DispatchNmi();
  /* USER CODE END NonMaskableInt_IRQn 1 */
}

/**
  * @brief This function handles Hard fault interrupt.
  */
void HardFault_Handler(void)
{
  /* USER CODE BEGIN HardFault_IRQn 0 */

  /* USER CODE END HardFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_HardFault_IRQn 0 */


	  APP_FAULT_DispatchHardFault();

    /* USER CODE END W1_HardFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Memory management fault.
  */
void MemManage_Handler(void)
{
  /* USER CODE BEGIN MemoryManagement_IRQn 0 */

  /* USER CODE END MemoryManagement_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_MemoryManagement_IRQn 0 */
	  APP_FAULT_DispatchMemManage();
    /* USER CODE END W1_MemoryManagement_IRQn 0 */
  }
}

/**
  * @brief This function handles Pre-fetch fault, memory access fault.
  */
void BusFault_Handler(void)
{
  /* USER CODE BEGIN BusFault_IRQn 0 */

  /* USER CODE END BusFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_BusFault_IRQn 0 */
	  APP_FAULT_DispatchBusFault();
    /* USER CODE END W1_BusFault_IRQn 0 */
  }
}

/**
  * @brief This function handles Undefined instruction or illegal state.
  */
void UsageFault_Handler(void)
{
  /* USER CODE BEGIN UsageFault_IRQn 0 */

  /* USER CODE END UsageFault_IRQn 0 */
  while (1)
  {
    /* USER CODE BEGIN W1_UsageFault_IRQn 0 */
	  APP_FAULT_DispatchUsageFault();
    /* USER CODE END W1_UsageFault_IRQn 0 */
  }
}

/**
  * @brief This function handles System service call via SWI instruction.
  */
void SVC_Handler(void)
{
  /* USER CODE BEGIN SVCall_IRQn 0 */

  /* USER CODE END SVCall_IRQn 0 */
  /* USER CODE BEGIN SVCall_IRQn 1 */

  /* USER CODE END SVCall_IRQn 1 */
}

/**
  * @brief This function handles Debug monitor.
  */
void DebugMon_Handler(void)
{
  /* USER CODE BEGIN DebugMonitor_IRQn 0 */

  /* USER CODE END DebugMonitor_IRQn 0 */
  /* USER CODE BEGIN DebugMonitor_IRQn 1 */

  /* USER CODE END DebugMonitor_IRQn 1 */
}

/**
  * @brief This function handles Pendable request for system service.
  */
void PendSV_Handler(void)
{
  /* USER CODE BEGIN PendSV_IRQn 0 */

  /* USER CODE END PendSV_IRQn 0 */
  /* USER CODE BEGIN PendSV_IRQn 1 */

  /* USER CODE END PendSV_IRQn 1 */
}

/**
  * @brief This function handles System tick timer.
  */
void SysTick_Handler(void)
{
  /* USER CODE BEGIN SysTick_IRQn 0 */

  /* USER CODE END SysTick_IRQn 0 */
  HAL_IncTick();
  /* USER CODE BEGIN SysTick_IRQn 1 */

  /* USER CODE END SysTick_IRQn 1 */
}

/******************************************************************************/
/* STM32F4xx Peripheral Interrupt Handlers                                    */
/* Add here the Interrupt Handlers for the used peripherals.                  */
/* For the available peripheral interrupt handler names,                      */
/* please refer to the startup file (startup_stm32f4xx.s).                    */
/******************************************************************************/

/**
  * @brief This function handles EXTI line0 interrupt.
  */
void EXTI0_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI0_IRQn 0 */

  /* USER CODE END EXTI0_IRQn 0 */
  HAL_GPIO_EXTI_IRQHandler(BOARD_DEBUG_BUTTON_Pin);
  /* USER CODE BEGIN EXTI0_IRQn 1 */

  /* USER CODE END EXTI0_IRQn 1 */
}

/**
  * @brief This function handles DMA1 stream5 global interrupt.
  */
void DMA1_Stream5_IRQHandler(void)
{
  /* USER CODE BEGIN DMA1_Stream5_IRQn 0 */

  /* USER CODE END DMA1_Stream5_IRQn 0 */
  HAL_DMA_IRQHandler(&hdma_dac1);
  /* USER CODE BEGIN DMA1_Stream5_IRQn 1 */

  /* USER CODE END DMA1_Stream5_IRQn 1 */
}

/**
  * @brief This function handles USART1 global interrupt.
  */
void USART1_IRQHandler(void)
{
  /* USER CODE BEGIN USART1_IRQn 0 */

  /* USER CODE END USART1_IRQn 0 */
  HAL_UART_IRQHandler(&huart1);
  /* USER CODE BEGIN USART1_IRQn 1 */

  /* USER CODE END USART1_IRQn 1 */
}

/**
  * @brief This function handles USART2 global interrupt.
  */
void USART2_IRQHandler(void)
{
  /* USER CODE BEGIN USART2_IRQn 0 */

  /* ------------------------------------------------------------------------ */
  /*  GPS UART는 HAL의 1-byte RxCplt 재arm 경로를 쓰지 않는다.                */
  /*                                                                        */
  /*  direct RXNE/ERR IRQ 경로로 즉시 처리하고 return 한다.                  */
  /*                                                                        */
  /*  이 방식은 CubeMX가 HAL_UART_IRQHandler(&huart2)를 다시 생성하더라도     */
  /*  USER CODE 블록이 먼저 실행되므로 재생성 내성이 좋다.                    */
  /* ------------------------------------------------------------------------ */
  Ublox_GPS_OnUartIrq(&huart2);
  return;

  /* USER CODE END USART2_IRQn 0 */
  HAL_UART_IRQHandler(&huart2);
  /* USER CODE BEGIN USART2_IRQn 1 */
  /* USER CODE END USART2_IRQn 1 */
}

/**
  * @brief This function handles USART3 global interrupt.
  */
void USART3_IRQHandler(void)
{
  /* USER CODE BEGIN USART3_IRQn 0 */

  /* USER CODE END USART3_IRQn 0 */
  HAL_UART_IRQHandler(&huart3);
  /* USER CODE BEGIN USART3_IRQn 1 */

  /* USER CODE END USART3_IRQn 1 */
}

/**
  * @brief This function handles EXTI line[15:10] interrupts.
  */
void EXTI15_10_IRQHandler(void)
{
  /* USER CODE BEGIN EXTI15_10_IRQn 0 */

  /* USER CODE END EXTI15_10_IRQn 0 */
  HAL_GPIO_EXTI_IRQHandler(BUTTON1_Pin);
  HAL_GPIO_EXTI_IRQHandler(BUTTON2_Pin);
  HAL_GPIO_EXTI_IRQHandler(BUTTON3_Pin);
  HAL_GPIO_EXTI_IRQHandler(BUTTON4_Pin);
  HAL_GPIO_EXTI_IRQHandler(BUTTON5_Pin);
  HAL_GPIO_EXTI_IRQHandler(BUTTON6_Pin);
  /* USER CODE BEGIN EXTI15_10_IRQn 1 */

  /* USER CODE END EXTI15_10_IRQn 1 */
}

/**
  * @brief This function handles TIM7 global interrupt.
  */
void TIM7_IRQHandler(void)
{
  /* USER CODE BEGIN TIM7_IRQn 0 */

  /* USER CODE END TIM7_IRQn 0 */
  HAL_TIM_IRQHandler(&htim7);
  /* USER CODE BEGIN TIM7_IRQn 1 */

  /* USER CODE END TIM7_IRQn 1 */
}

/* USER CODE BEGIN 1 */

/**
  * @brief This function handles EXTI line2 interrupt.
  *
  * SD_DETECT 핀(PA2)을 EXTI로 runtime 재구성해 두었기 때문에,
  * CubeMX가 현재 .ioc 에 EXTI2를 생성하지 않았더라도
  * 이 USER CODE 핸들러만 유지되면 hotplug IRQ는 계속 살아 있다.
  */
void EXTI2_IRQHandler(void)
{
  HAL_GPIO_EXTI_IRQHandler(SD_DETECT_Pin);
}

/* USER CODE END 1 */
