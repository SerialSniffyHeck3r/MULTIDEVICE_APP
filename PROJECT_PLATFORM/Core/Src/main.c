/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
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
#include "adc.h"
#include "dac.h"
#include "dma.h"
#include "fatfs.h"
#include "i2c.h"
#include "iwdg.h"
#include "rtc.h"
#include "sdio.h"
#include "spi.h"
#include "tim.h"
#include "usart.h"
#include "usb_otg.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */

#include "APP_PRODUCT_INIT.h"

/* 디스플레이 작동을 위한 헤더 */
#include "u8g2_uc1608_stm32.h"
#include "u8g2.h"

/* 디스플레이 작동을 위한 헤더 */
#include <math.h>
#include <stdio.h>


/* 데이터 창고 APP_STATE. 모든 함수는 앱스테이트를 보고 거기서 복사해 와야 한다. */
#include "APP_STATE.h"
#include "APP_ALTITUDE.h"

/* 유블록스 NEO_M10 GPS 드라이버 */
#include "Ublox_GPS.h"

/* 치명 에러 발생시 디스플레이에 띄우는 핸들링을 위한 헤더 */
#include "APP_FAULT.h"

/* 새 센서 드라이버 */
#include "GY86_IMU.h"
#include "DS18B20_DRIVER.h"

/* BUTTONS DRIVER */
#include "button.h"

/* SD CARD WRITE HIGH LEVEL LAYER */
#include "APP_SD.h"

/* BRIGHTNESS SENSOR DETECTION */
#include "Brightness_Sensor.h"

/* Bluetooth bring-up / 무선 시리얼 드라이버 */
#include "Bluetooth.h"

/* 유선 UART 로그 출력 helper */
#include "DEBUG_UART.h"

#include "SPI_Flash.h"

/* DAC + DMA 오디오 엔진 */
#include "Audio_Driver.h"
#include "Audio_Presets.h"
#include "Audio_App.h"

/* FAULT DETECTOR */
#include "APP_FAULT_DIAG.h"

/* FIRMWARE APP GUARD */
#include "FW_AppGuard.h"

/* LED DRIVER */
#include "LED_App.h"

/* BACKLIGHT DRIVER */
#include "BACKLIGHT_App.h"

/* PWR MGNT */
#include "POWER_STATE.h"

/* POST */
#include "BOOT_SELFTEST_SCREEN.h"
/* MOTORBIKE DYNAMICS CALCULATION */
#include "BIKE_DYNAMICS.h"


#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif


/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */


/* -------------------------------------------------------------------------- */
/*  앱 부팅 확정 지연 시간                                                     */
/*                                                                            */
/*  목적                                                                       */
/*  - FW_AppGuard_ConfirmBootOk()를 너무 이르게 호출하면,                      */
/*    실제 서비스 task들이 몇 번 돌아보기도 전에 "정상 부팅 완료" 로           */
/*    판정되어 버릴 수 있다.                                                   */
/*  - 따라서 최소한 짧은 안정화 시간 동안 main loop가 실제로 돈 뒤에만          */
/*    boot confirmed를 세운다.                                                 */
/*                                                                            */
/*  현재 값 2000ms 의미                                                        */
/*  - 초기 boot splash / 각종 task 시작 직후의 즉발성 fault나 hang을            */
/*    unconfirmed boot 범주에 남겨 둔다.                                       */
/*  - 반대로 너무 길게 잡아 정상 부팅에서도 boot fail streak가 누적되는         */
/*    상황은 피하기 위해, 서비스 초기화 관점에서 짧지만 의미 있는 시간으로      */
/*    2초를 사용한다.                                                          */
/* -------------------------------------------------------------------------- */
#define APP_BOOT_CONFIRM_DELAY_MS    2000u

/* -------------------------------------------------------------------------- */
/*  상위 앱 선택                                                               */
/*                                                                            */
/*  지금 단계에서는 단순히 main.c의 정적 변수 하나로                         */
/*  Motor_App / Vario_App 중 어느 슈퍼루프를 돌릴지 선택한다.                 */
/*                                                                            */
/*  - 1 : Motor_App                                                           */
/*  - 0 : Vario_App                                                           */
/* -------------------------------------------------------------------------- */
/* -------------------------------------------------------------------------- */
/*  Product divergence routing                                                */
/*                                                                            */
/*  Cube-generated main.c should know as little as possible about             */
/*  MOTOR_APP vs VARIO_APP.                                                   */
/*                                                                            */
/*  Product-dependent dispatch therefore lives in APP_PRODUCT_INIT.c, which   */
/*  is called only through stable hook functions inside USER CODE sections.   */
/* -------------------------------------------------------------------------- */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */





/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


  /* -------------------------------------------------------------------------- */
  /*  런타임 IRQ priority 보정                                                    */
  /*                                                                            */
  /*  CubeMX / .ioc 재생성 후에도 최종 priority를 코드에서 다시 덮어써서         */
  /*  GPS RX가 항상 UI timer보다 먼저 처리되게 만든다.                           */
  /* -------------------------------------------------------------------------- */
  static void APP_ApplyRuntimeInterruptPriorities(void)
  {
      /* ---------------------------------------------------------------------- */
      /*  오디오 DAC DMA는 이 시스템에서 가장 deadline 민감도가 높다.             */
      /*  따라서 DMA1_Stream5(DAC1 CH1)는 최상위 priority로 둔다.                */
      /* ---------------------------------------------------------------------- */
      HAL_NVIC_SetPriority(DMA1_Stream5_IRQn, 0, 0);
      HAL_NVIC_EnableIRQ(DMA1_Stream5_IRQn);

      /* ---------------------------------------------------------------------- */
      /*  GPS RX는 direct IRQ parser를 사용하므로,                               */
      /*  오디오 다음 우선순위로 둔다.                                            */
      /* ---------------------------------------------------------------------- */
      HAL_NVIC_SetPriority(USART2_IRQn, 1, 0);
      HAL_NVIC_EnableIRQ(USART2_IRQn);

      /* ---------------------------------------------------------------------- */
      /*  Bluetooth UART는 GPS 다음 단계.                                        */
      /*  TX도 이제 interrupt-driven queue를 쓰므로 IRQ priority가 의미 있다.     */
      /* ---------------------------------------------------------------------- */
      HAL_NVIC_SetPriority(USART3_IRQn, 2, 0);
      HAL_NVIC_EnableIRQ(USART3_IRQn);

      /* ---------------------------------------------------------------------- */
      /*  20Hz UI frame timer는 deadline이 가장 느슨한 축이므로 더 낮게 둔다.     */
      /* ---------------------------------------------------------------------- */
      HAL_NVIC_SetPriority(TIM7_IRQn, 4, 0);
      HAL_NVIC_EnableIRQ(TIM7_IRQn);

      /* ---------------------------------------------------------------------- */
      /*  버튼/SD detect EXTI는 사람 입력/삽입 이벤트이므로                       */
      /*  UI timer보다 더 낮은 우선순위로 둔다.                                   */
      /* ---------------------------------------------------------------------- */
      HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
      HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

      HAL_NVIC_SetPriority(EXTI2_IRQn, 5, 0);
      HAL_NVIC_EnableIRQ(EXTI2_IRQn);

      HAL_NVIC_SetPriority(EXTI0_IRQn, 5, 0);
      HAL_NVIC_EnableIRQ(EXTI0_IRQn);

      /* ---------------------------------------------------------------------- */
      /*  유선 DEBUG UART는 가장 낮아도 된다.                                     */
      /*  debug log는 늦어져도 되지만, 오디오/DMA deadline은 늦어지면 안 된다.    */
      /* ---------------------------------------------------------------------- */
      HAL_NVIC_SetPriority(USART1_IRQn, 6, 0);
      HAL_NVIC_EnableIRQ(USART1_IRQn);
  }



  static void APP_BlinkFatalLoop(uint32_t code)
  {
      uint32_t i;
      volatile uint32_t d;

      for (;;)
      {
          for (i = 0u; i < code; i++)
          {
              HAL_GPIO_WritePin(BOARD_LED_GPIO_Port, BOARD_LED_Pin, GPIO_PIN_SET);
              for (d = 0u; d < 250000u; d++) { __NOP(); }
              HAL_GPIO_WritePin(BOARD_LED_GPIO_Port, BOARD_LED_Pin, GPIO_PIN_RESET);
              for (d = 0u; d < 250000u; d++) { __NOP(); }
          }

          for (d = 0u; d < 900000u; d++) { __NOP(); }
      }
  }

  void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
  {
      /* ---------------------------------------------------------------------- */
      /*  현재 프로젝트는                                                        */
      /*    - USART2 : u-blox GPS                                                */
      /*    - USART3 : Bluetooth classic SPP                                     */
      /*  경로를 각각 다른 드라이버에 맡긴다.                                    */
      /*                                                                        */
      /*  각 드라이버가 huart->Instance를 스스로 검사하므로                       */
      /*  여기서는 둘 다 호출해도 안전하다.                                      */
      /* ---------------------------------------------------------------------- */
      Ublox_GPS_OnUartRxCplt(huart);
      Bluetooth_OnUartRxCplt(huart);
  }

  void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
  {
      /* ---------------------------------------------------------------------- */
      /*  TX complete callback은                                                 */
      /*    - USART3 Bluetooth TX queue drain                                    */
      /*    - USART1 DEBUG UART TX queue drain                                   */
      /*  두 경로에 전달한다.                                                    */
      /*                                                                        */
      /*  각 드라이버가 자기 UART instance인지 검사하므로                         */
      /*  여기서 둘 다 호출해도 안전하다.                                        */
      /* ---------------------------------------------------------------------- */
      Bluetooth_OnUartTxCplt(huart);
      DEBUG_UART_OnUartTxCplt(huart);
  }

  void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
  {
      Ublox_GPS_OnUartError(huart);
      Bluetooth_OnUartError(huart);
      DEBUG_UART_OnUartError(huart);
  }

  void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
  {
    if (htim->Instance == TIM7)
    {
      U8G2_UC1608_FrameTickFromISR();
      APP_PRODUCT_OnFrameTickFromISR();
    }
  }

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

  /* USER CODE END 1 */

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */

  /* USER CODE END SysInit */

  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_DMA_Init();
  MX_SDIO_SD_Init();
  MX_SPI1_Init();
  MX_SPI2_Init();
  MX_USB_OTG_FS_PCD_Init();
  MX_I2C1_Init();
  MX_USART1_UART_Init();
  MX_USART2_UART_Init();
  MX_USART3_UART_Init();
  MX_FATFS_Init();
  MX_TIM1_Init();
  MX_TIM3_Init();
  MX_TIM4_Init();
  MX_TIM9_Init();
  MX_ADC1_Init();
  MX_RTC_Init();
  MX_TIM7_Init();
  MX_DAC_Init();
  MX_TIM6_Init();
  MX_IWDG_Init();
  MX_TIM8_Init();
  MX_I2C2_Init();
  /* USER CODE BEGIN 2 */

  /* ------------------------------------------------------------------------ */
  /*  Product post-init hand-off after Cube MX_* bring-up                     */
  /*                                                                          */
  /*  Why this exists                                                         */
  /*  - GPIO / peripheral topology is still owned by the shared APP IOC.      */
  /*  - Some runtime ownership decisions, however, are product policy and     */
  /*    must not be hard-coded inside Cube-generated files.                   */
  /*                                                                          */
  /*  Therefore                                                               */
  /*  - Cube config stays common and regeneratable                            */
  /*  - PRODUCT hook may refine runtime ownership without editing gpio.c,     */
  /*    tim.c, usart.c, or other generated files directly                     */
  /*                                                                          */
  /*  Future examples                                                         */
  /*  - MOTOR: IGN sense / AUX rail FET / STOP-STANDBY prep                   */
  /*  - VARIO: battery-gated peripherals / transport lock / soft power flow   */
  /* ------------------------------------------------------------------------ */
  APP_PRODUCT_PostCubePeripheralInit();

  /* ------------------------------------------------------------------------ */
  /*  LED 서브시스템 초기화                                                    */
  /*                                                                          */
  /*  전제                                                                    */
  /*  - MX_TIM1_Init() ~ MX_TIM9_Init() 이 이미 끝난 뒤여야 한다.              */
  /*  - LED_Driver가 여기서 각 PWM 채널을 실제로 start한다.                    */
  /*                                                                          */
  /*  초기 모드                                                                */
  /*  - 전원 인가 직후 한 번은 Welcome sweep를 보여 주고                      */
  /*  - sweep 완료 후에는 LED_App 내부에서 자동으로 Idle breath로 전환된다.    */
  /* ------------------------------------------------------------------------ */
  LED_App_Init();
  /* ------------------------------------------------------------------------ */
  /*  LCD 백라이트 서브시스템 초기화                                            */
  /*                                                                          */
  /*  실제 출력 경로                                                            */
  /*  - LCD 패널 백라이트 PWM 실출력은 PB1 / TIM3_CH4 이다.                    */
  /*  - 이 핀의 alternate function 설정은 Cube가 생성한 MX_TIM3_Init() +       */
  /*    HAL_TIM_MspPostInit()가 이미 끝낸 상태여야 한다.                        */
  /*  - Backlight_App_Init()는 pin mux를 다시 바꾸지 않고,                     */
  /*    BACKLIGHT_DRIVER가 Cube가 만든 PB1 / TIM3_CH4 PWM 채널을 start하고     */
  /*    백라이트 정책 runtime만 초기화한다.                                     */
  /*                                                                          */
  /*  주의                                                                      */
  /*  - Application2/Core/Src/gpio.c 에서 일반 GPIO output으로 잡는 것은       */
  /*    PE2의 BACKLIGHT_Pin 이며, LCD 패널 PWM 출력인 PB1/LCD_BACKLIGHT_Pin과   */
  /*    동일한 핀이 아니다.                                                     */
  /*  - 따라서 PB1 소유권은 TIM3_CH4 + BACKLIGHT_DRIVER 한 경로로만 본다.      */
  /* ------------------------------------------------------------------------ */
  Backlight_App_Init();



  APP_STATE_Init();
  APP_ALTITUDE_Init(HAL_GetTick());
  BIKE_DYNAMICS_Init(HAL_GetTick());

  /* ------------------------------------------------------------------------ */
  /*  RTC / timezone / GPS sync service bring-up                               */
  /*                                                                          */
  /*  MX_RTC_Init() 는 Cube 생성 코드가 이미 수행했고,                          */
  /*  여기서는 backup register metadata load + baseline validity 정리 +       */
  /*  APP_STATE.clock runtime snapshot 초기화를 맡는다.                        */
  /* ------------------------------------------------------------------------ */
  APP_CLOCK_Init(HAL_GetTick());


  FW_AppGuard_OnAppBootStart();


  /* ------------------------------------------------------------------------ */
  /*  유선 DEBUG UART bring-up                                                 */
  /*                                                                          */
  /*  이제부터는 DEBUG_UART_PrintLine / Printf 한 줄로                          */
  /*  USB-UART TTL 어댑터 쪽으로 문자열 로그를 보낼 수 있다.                    */
  /* ------------------------------------------------------------------------ */
  DEBUG_UART_Init();
  DBG_PRINTLN("[BOOT] DEBUG UART ready");

  /* ------------------------------------------------------------------------ */
  /*  버튼 초기화
  /* ------------------------------------------------------------------------ */

  Button_Init();

  /* ------------------------------------------------------------------------ */
  /* 부팅 시 F2 hold maintenance self-test mode / F3 hold legacy UI profile   */
  /*                                                                          */
  /* 규칙                                                                     */
  /* - F2 hold : maintenance self-test mode 진입 latch                        */
  /* - F3 hold : 기존 legacy UI boot profile latch                            */
  /* - 둘 다 눌려도 maintenance mode가 우선이며,                              */
  /*   이때는 UI 엔진의 legacy root를 강제로 타지 않게 한다.                  */
  /* ------------------------------------------------------------------------ */
  const bool boot_f2_held_for_selftest = (Button_IsPressed(BUTTON_ID_2) != false);

  /* ------------------------------------------------------------------------ */
  /*  Product boot-mode arbitration                                           */
  /*                                                                          */
  /*  main.c only publishes the boot-time facts it knows here.                */
  /*  The actual interpretation of those facts is delegated to                */
  /*  APP_PRODUCT_INIT.c so F2/F3 policy, legacy profile routing, and future  */
  /*  product-specific boot overlays stay outside the Cube-generated shell.   */
  /* ------------------------------------------------------------------------ */
  APP_PRODUCT_ConfigureBootMode(boot_f2_held_for_selftest);
  /* ------------------------------------------------------------------------ */
  /* Soft Power 상태머신 초기화                                               */
  /*                                                                          */
  /* 이 초기화는 CubeMX gpio.c 설정을 그대로 믿지 않고,                       */
  /* Soft Power Switch Mk2의 실제 전기적 요구사항에 맞게                       */
  /* OFF/PUSH 핀을 런타임에서 다시 정렬한다.                                   */
  /*                                                                          */
  /* - OFF  : output push-pull / LOW 유지                                      */
  /* - PUSH : input pull-up / active-low 해석                                  */
  /* ------------------------------------------------------------------------ */
  POWER_STATE_Init();

  /* ------------------------------------------------------------------------ */
  /*  Product power-policy post-init                                          */
  /*                                                                          */
  /*  POWER_STATE_Init() restores the common overlay state machine.           */
  /*  Any product-specific runtime ownership that depends on that overlay     */
  /*  being alive must happen after this point, not inside generated code.    */
  /* ------------------------------------------------------------------------ */
  APP_PRODUCT_PostPowerStateInit();

  /* ------------------------------------------------------------------------ */
  /*  새 UI 엔진 초기화                                                        */
  /*                                                                          */
  /*  - 버튼 초기화가 끝난 뒤 현재 눌림 마스크를 안전하게 읽을 수 있다.         */
  /*  - 여기서는 엔진의 내부 상태/하단바/토스트/팝업/legacy debug state를      */
  /*    초기화만 하고, 실제 LCD draw는 아직 하지 않는다.                       */
  /* ------------------------------------------------------------------------ */
  /* ------------------------------------------------------------------------ */
  /*  상위 앱에 따라 UI 상단 계층 초기화를 분기한다.                           */
  /*                                                                          */
  /*  - Motor_App 는 자체 UI 렌더러를 사용하므로 shared UI_Engine을 올리지     */
  /*    않는다.                                                                */
  /*  - Vario_App 는 기존 shared UI_Engine 위에서 계속 동작한다.               */
  /* ------------------------------------------------------------------------ */
  APP_PRODUCT_InitUiSubsystem();

  /* ------------------------------------------------------------------------ */
  /*  CubeMX가 MSP init에서 priority를 다시 생성하더라도                       */
  /*  여기서 최종 priority를 한 번 더 강제로 맞춘다.                           */
  /* ------------------------------------------------------------------------ */
  APP_ApplyRuntimeInterruptPriorities();

  /* ===== Display Init ===== */

  /* ----------------------------------------------------------------------
   *  Display는 가능한 한 먼저 올린다.
   *
   *  이유:
   *  - 직전 부팅의 fault 로그를 이번 부팅 초기에 곧바로 보여주기 위해서다.
   *  - 만약 그 전에 다른 주변장치 init가 다시 fault를 내면, fault viewer가
   *    화면에 뜰 기회조차 없이 재리셋될 수 있다.
   * --------------------------------------------------------------------*/
  U8G2_UC1608_Init();



  /* ----------------------------------------------------------------------
   *  직전 부팅 fault 확인 + 10초 표시
   *
   *  fault 로그가 없으면 함수는 즉시 리턴한다.
   *  fault 로그가 있으면:
   *    1) 제목(HARDFAULT / BUSFAULT / USAGEFAULT ...)
   *    2) fault status register dump
   *    3) 친절한 영어 요약 설명
   *    4) 하단 progress bar
   *  를 보여준 뒤 로그를 지운다.
   * --------------------------------------------------------------------*/
  APP_FAULT_BootCheckAndShow(U8G2_UC1608_GetHandle(), 10000u);

  /* -------------------------------------------------------------------- */
  /*  bring-up용 fault 진단 모드를 여기서 한 번 켠다.                     */
  /*                                                                      */
  /*  이 블록은                                                            */
  /*    1) MemManage / BusFault / UsageFault enable                       */
  /*    2) 옵션에 따라 write buffer disable                               */
  /*  를 수행해서                                                          */
  /*  다음번 fault가 다시 나더라도                                         */
  /*  현재보다 훨씬 의미 있는 예외 정보(EXRET / PC / LR)를 남기게 한다.   */
  /* -------------------------------------------------------------------- */
  APP_FAULT_DIAG_EnableBringupMode();

  FW_AppGuard_Kick();

  /* ---------------------------------------------------------------------- */
  /*  새 UI 엔진의 부트 로고를 지금 시점에 한 번 그린다.                      */
  /*                                                                        */
  /*  이후 나머지 센서/통신/스토리지 init가 진행되는 동안                     */
  /*  마지막으로 그려진 이 부트 로고가 화면에 남아 있게 된다.                 */
  /* ---------------------------------------------------------------------- */
  APP_PRODUCT_DrawEarlyBoot();

  /* ---------------------------------------------------------------------- */
  /* Soft Power 부팅 확인 게이트 진입                                        */
  /*                                                                        */
  /* 요구사항                                                                */
  /* - bootloader 에서 app 로 점프한 직후                                    */
  /*   곧바로 전체 화면 확인 UI를 띄운다.                                     */
  /* - 30초 안에 사용자가 LONG PRESS F6 으로 진행을 승인하지 않으면           */
  /*   자동으로 전원을 끈다.                                                  */
  /* - F1 short 는 즉시 전원 OFF 이다.                                       */
  /*                                                                        */
  /* 이 게이트는 아직 일반 런타임 init 를 본격적으로 시작하기 전에            */
  /* 실행해서, 사용자가 전원을 켜자마자 곧바로 의도를 확정할 수 있게 한다.    */
  /* ---------------------------------------------------------------------- */
  APP_PRODUCT_EnterPowerOnConfirmIfRequired(HAL_GetTick());

  /* ---------------------------------------------------------------------- */
  /* boot gate loop                                                          */
  /*                                                                        */
  /* 이 루프에서는                                                            */
  /* - watchdog refresh                                                      */
  /* - F1~F6 버튼 debounce / long press 판정                                 */
  /* - 전원 확인 화면 draw                                                   */
  /* 만 수행한다.                                                            */
  /*                                                                        */
  /* 아직 GPS / Bluetooth / 센서 / SD runtime init 를 시작하지 않으므로       */
  /* 사용자가 POWER ON 을 취소했을 때 깔끔하게 바로 꺼질 수 있다.            */
  /* ---------------------------------------------------------------------- */
  while (POWER_STATE_IsUiBlocking() != false)
  {
      uint32_t power_gate_now_ms;

      power_gate_now_ms = HAL_GetTick();

      /* ------------------------------------------------------------------ */
      /* IWDG 가 이미 살아 있는 빌드에서도                                   */
      /* 사용자가 확인 화면에서 30초를 다 쓰는 동안 리셋되지 않게 한다.       */
      /* ------------------------------------------------------------------ */
      FW_AppGuard_Kick();

      /* ------------------------------------------------------------------ */
      /* F1/F6 이벤트는 기존 button.c 의 event queue 를 그대로 사용한다.     */
      /* ------------------------------------------------------------------ */
      Button_Task(power_gate_now_ms);

      /* ------------------------------------------------------------------ */
      /* boot gate 전용 power UI task                                        */
      /* - frame token 없이 직접 redraw                                       */
      /* - F1 short / LONG F6 / timeout 처리                                  */
      /* ------------------------------------------------------------------ */
      APP_PRODUCT_ServiceBootGate(power_gate_now_ms);
  }











  /* Display가 준비된 뒤, 나머지 런타임 init를 진행한다. */
  Init_Ublox_M10(); // NEO-M10 GPS Initialization

  /* --------------------------------------------------------------------
   *  Bluetooth classic SPP bring-up
   *
   *  - 기본값은 huart3 + 9600 8N1
   *  - RX interrupt를 시작해서 수신 바이트를 ring에 적재한다.
   *  - 이후 main loop의 Bluetooth_Task()가 line 단위로 해석한다.
   * ------------------------------------------------------------------*/
  Bluetooth_Init();
  DBG_PRINTLN("[BOOT] Bluetooth ready");

  /* --------------------------------------------------------------------
   *  센서 드라이버도 이제 APP_STATE 기반 단일 시간축(main의 now_ms)에 묶는다.
   *  - GY-86 : I2C backend 블록 구조
   *  - DS18B20 : DWT 기반 1-Wire bit-bang
   * ------------------------------------------------------------------*/
  GY86_IMU_Init();
  DS18B20_DRIVER_Init();
  Brightness_Sensor_Init();
  SPI_Flash_Init();

  /* --------------------------------------------------------------------
     *  DAC + DMA 오디오 엔진 bring-up
     *
     *  전제:
     *  - MX_DAC_Init() / MX_TIM6_Init() 는 IOC 생성 코드가 이미 호출한 상태
     *  - 여기서는 runtime에서 sample rate / trigger / content 상태를 정렬한다.
     * ------------------------------------------------------------------*/
    Audio_Driver_Init();

    /* ------------------------------------------------------------------
     *  시작 볼륨은 APP_STATE raw 필드에 직접 대입하지 않고,
     *  반드시 driver API를 통해 적용한다.
     *
     *  이유:
     *  - APP_STATE.audio.volume_percent는 "표시용 raw 창고" 이고
     *  - 실제 출력 gain(Q15 LUT + analog safe headroom)은
     *    Audio_Driver_SetVolumePercent() 안에서 함께 갱신된다.
     * ------------------------------------------------------------------*/
    Audio_Driver_SetVolumePercent(3u);

    Audio_App_Init();


  /* --------------------------------------------------------------------
   *  SD / FATFS hotplug bring-up
   *
   *  MX_FATFS_Init() 는 이미 Cube 생성 코드에서 끝났으므로,
   *  여기서는 detect pin runtime 재설정 + 초기 detect baseline +
   *  필요 시 mount retry state만 올린다.
   * ------------------------------------------------------------------*/
  APP_SD_Init();

  /* -------------------------------------------------------------------- */
  /*  Product post-bringup hand-off                                        */
  /*                                                                      */
  /*  Shared low-level services are now alive. Any product policy that    */
  /*  depends on those services existing, but still must happen before     */
  /*  the product app state machine starts, belongs behind this hook.      */
  /* -------------------------------------------------------------------- */
  APP_PRODUCT_PostCommonBringup();

  /* -------------------------------------------------------------------- */
  /* F2 hold maintenance self-test mode                                    */
  /*                                                                        */
  /* - GPS / Bluetooth / GY86 / Audio / SPI Flash / SD init이 끝난 뒤      */
  /*   진입하므로, 새 self-test 루프는 기존 공개 API를 그대로 재사용한다.   */
  /* - 이 함수는 정상 경로에서는 return 하지 않는다.                        */
  /* -------------------------------------------------------------------- */
  if (boot_f2_held_for_selftest != false)
  {
    DBG_PRINTLN("[BOOT] F2 held -> maintenance self-test mode");
    SELFTEST_RunMaintenanceModeLoop();
  }


  /* -------------------------------------------------------------------- */
  /* blocking 부트 self-test screen                                       */
  /*                                                                      */
  /* 이 화면은 아래 역할을 동시에 맡는다.                                 */
  /* 1) boot logo를 위쪽에 다시 배치한 self-test 전용 UI draw             */
  /* 2) GPS / IMU / SENSORS / HARDWARE 4개 카테고리 상태 표시             */
  /* 3) 전원 ON 확인 화면에서 남아 있는 F6 release 등 잔상 버튼 이벤트를  */
  /*    계속 소비해서 일반 런타임 화면으로 새어 들어가지 않게 차단        */
  /* 4) self-test가 끝날 때까지 boot 동안 필요한 background task를        */
  /*    임시 main loop처럼 계속 서비스                                     */
  /*                                                                      */
  /* 주의                                                                  */
  /* - 이 함수는 내부에서 watchdog kick을 계속 수행한다.                  */
  /* - TIM7 frame token이 아직 시작되지 않았으므로                         */
  /*   direct U8G2 commit 방식으로 화면을 갱신한다.                       */
  /* -------------------------------------------------------------------- */
  BOOT_SELFTEST_SCREEN_RunBlocking();

  /* -------------------------------------------------------------------- */
  /*  20fps 화면 갱신용 timer는 TIM7을 사용한다.                            */
  /*  오디오 transport는 TIM6이 맡으므로, 역할을 주석에서도 분리해 둔다.     */
  /* -------------------------------------------------------------------- */
  HAL_TIM_Base_Start_IT(&htim7);

  /* 스마트 업데이트 + FPS 리밋 유지 */
  U8G2_UC1608_EnableSmartUpdate(1);
  U8G2_UC1608_EnableFrameLimit(1);



  //////////////////////////////    BOOT SUCCESS    //////////////////////////////////////////////////
  /* -------------------------------------------------------------------- */
   /*  여기서는 더 이상 즉시 boot confirmed를 세우지 않는다.                 */
   /*                                                                        */
   /*  이유                                                                   */
   /*  - 이 시점은 주변장치 init 호출은 끝났지만,                              */
   /*    실제 서비스 task들이 main loop에서 안정적으로 반복 실행되었다고        */
   /*    보장할 수는 없는 구간이다.                                            */
   /*  - 따라서 지금은 "확정" 대신 "확정 대기 시작 시각" 만 기록한다.         */
   /*                                                                        */
   /*  이후 while(1) 안에서                                                    */
   /*    1) 주요 task들이 실제로 몇 번 돌고                                     */
   /*    2) APP_BOOT_CONFIRM_DELAY_MS 만큼 시간이 흐른 뒤                      */
   /*  FW_AppGuard_ConfirmBootOk()를 딱 한 번 호출한다.                       */
   /* -------------------------------------------------------------------- */

   APP_PRODUCT_RunAppInit();




  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */

    APP_PRODUCT_RunAppTask();

  }


  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Configure the main internal regulator output voltage
  */
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_LSI|RCC_OSCILLATORTYPE_HSE
                              |RCC_OSCILLATORTYPE_LSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.LSEState = RCC_LSE_ON;
  RCC_OscInitStruct.LSIState = RCC_LSI_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 4;
  RCC_OscInitStruct.PLL.PLLN = 168;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 7;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* -------------------------------------------------------------------------- */
/*  보드 디버그 버튼으로 페이지 전환                                           */
/*                                                                            */
/*  stm32f4xx_it.c 는 이미 EXTI0에서                                           */
/*  HAL_GPIO_EXTI_IRQHandler(BOARD_DEBUG_BUTTON_Pin)를 호출하고 있으므로        */
/*  여기 HAL_GPIO_EXTI_Callback만 구현하면 된다.                               */
/* -------------------------------------------------------------------------- */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  uint32_t now_ms;

  switch (GPIO_Pin)
  {
    case BUTTON1_Pin:
    case BUTTON2_Pin:
    case BUTTON3_Pin:
    case BUTTON4_Pin:
    case BUTTON5_Pin:
    case BUTTON6_Pin:
      /* ------------------------------------------------------------------ */
      /*  BUTTON1~BUTTON6은 공용 버튼 핸들러로 보낸다.                        */
      /*  실제 debounce / short / long 판정은 Button_Task()가 맡는다.        */
      /* ------------------------------------------------------------------ */
      Button_OnExtiInterrupt(GPIO_Pin);
      return;

    case SD_DETECT_Pin:
      /* ------------------------------------------------------------------ */
      /*  SD detect 는 EXTI2에서 들어온다.                                    */
      /*                                                                        */
      /*  여기서는 APP_SD 쪽 debounce state만 갱신하고,                        */
      /*  실제 mount/unmount 판단은 main loop의 APP_SD_Task()가 수행한다.      */
      /* ------------------------------------------------------------------ */
      APP_SD_OnDetectExti();
      return;

    case BOARD_DEBUG_BUTTON_Pin:
      /* ------------------------------------------------------------------ */
      /*  보드 디버그 버튼은 여전히 페이지 넘김용으로 유지한다.                 */
      /*  단, bounce에 의해 여러 번 넘어가지 않게 소프트웨어 debounce를       */
      /*  별도 helper로 묶어 처리한다.                                         */
      /* ------------------------------------------------------------------ */
      break;

    default:
      return;
  }

  now_ms = HAL_GetTick();
  APP_PRODUCT_OnBoardDebugButtonIrq(now_ms);
}

/* USER CODE END 4 */

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* ----------------------------------------------------------------------
   *  기존 코드는 여기서 LED blink loop로 영구 정지했다.
   *
   *  이제는:
   *    1) software fatal record를 남기고
   *    2) 시스템 리셋을 걸어서
   *    3) 다음 부팅 때 U8G2 fault viewer가 내용을 보여주게 만든다.
   *
   *  __builtin_return_address(0)는 "누가 Error_Handler를 불렀는가"를 찾기 위한
   *  best-effort PC 힌트다. 최적화 상태에 따라 완벽하지 않을 수는 있지만,
   *  개발 중에는 꽤 유용하다.
   * --------------------------------------------------------------------*/
  __disable_irq();

#if defined(__GNUC__)
  APP_FAULT_RecordSoftware(APP_FAULT_TYPE_ERROR_HANDLER,
                           (uint32_t)__builtin_return_address(0));
#else
  APP_FAULT_RecordSoftware(APP_FAULT_TYPE_ERROR_HANDLER, 0u);
#endif

  __DSB();
  NVIC_SystemReset();

  /* 이 아래는 정상이라면 도달하지 않는다.
   * 혹시라도 reset이 걸리지 않았다면 기존 blink loop를 fallback으로 유지한다. */
  APP_BlinkFatalLoop(9u);
  while (1)
  {
  }
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
