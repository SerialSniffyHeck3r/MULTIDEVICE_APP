#ifndef GY86_IMU_H
#define GY86_IMU_H

#include "main.h"
#include "APP_STATE.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  GY86_IMU                                                                   */
/*                                                                            */
/*  설계 목표                                                                  */
/*  - GY-86 보드의 각 하위 칩(MPU6050 / HMC5883L / MS5611)을                    */
/*    "교체 가능한 backend 블록" 으로 분리한다.                                */
/*  - 외부 모듈은 driver 내부 static 상태를 절대 보지 않고 APP_STATE만 본다.    */
/*  - polling 시간축은 main loop가 넘겨주는 SysTick(now_ms) 하나만 쓴다.       */
/*                                                                            */
/*  사용 규칙                                                                  */
/*  - 부팅 시 한 번 GY86_IMU_Init() 호출                                       */
/*  - main while(1) 안에서 GY86_IMU_Task(now_ms) 반복 호출                     */
/*  - UI / logger / 다른 상태 머신은 APP_STATE_CopyGy86Snapshot()만 사용       */
/* -------------------------------------------------------------------------- */

#ifndef GY86_IMU_I2C_HANDLE
#define GY86_IMU_I2C_HANDLE hi2c1
#endif

/* -------------------------------------------------------------------------- */
/*  secondary bus handle / pin mapping                                          */
/*                                                                            */
/*  USE_DOUBLE_BAROSENSOR = 2 모드에서만 실제로 사용한다.                      */
/*  현재 MOTORWITHVARIO_MAIN 의 CubeMX 출력은                                  */
/*  - I2C1 : PB6/PB7                                                            */
/*  - I2C2 : PB10/PB11                                                          */
/*  이므로, 기본값도 그 배치를 그대로 반영한다.                                 */
/*                                                                            */
/*  나중에 CubeMX에서 I2C2 핀을 바꿔도 ioc 재생성을 건드릴 필요 없이            */
/*  이 헤더의 override만 바꾸면 되게 만든다.                                   */
/* -------------------------------------------------------------------------- */
#ifndef GY86_IMU_I2C2_HANDLE
#define GY86_IMU_I2C2_HANDLE hi2c2
#endif

#ifndef GY86_IMU_I2C1_SCL_GPIO_PORT
#define GY86_IMU_I2C1_SCL_GPIO_PORT I2C_SCL_GY_GPIO_Port
#endif

#ifndef GY86_IMU_I2C1_SCL_PIN
#define GY86_IMU_I2C1_SCL_PIN I2C_SCL_GY_Pin
#endif

#ifndef GY86_IMU_I2C1_SDA_GPIO_PORT
#define GY86_IMU_I2C1_SDA_GPIO_PORT I2C_SDA_GY_GPIO_Port
#endif

#ifndef GY86_IMU_I2C1_SDA_PIN
#define GY86_IMU_I2C1_SDA_PIN I2C_SDA_GY_Pin
#endif

#ifndef GY86_IMU_I2C2_SCL_GPIO_PORT
#define GY86_IMU_I2C2_SCL_GPIO_PORT GPIOB
#endif

#ifndef GY86_IMU_I2C2_SCL_PIN
#define GY86_IMU_I2C2_SCL_PIN GPIO_PIN_10
#endif

#ifndef GY86_IMU_I2C2_SDA_GPIO_PORT
#define GY86_IMU_I2C2_SDA_GPIO_PORT GPIOB
#endif

#ifndef GY86_IMU_I2C2_SDA_PIN
#define GY86_IMU_I2C2_SDA_PIN GPIO_PIN_11
#endif

/* -------------------------------------------------------------------------- */
/*  dual MS5611 compile-time switch                                            */
/*                                                                            */
/*  USE_DOUBLE_BAROSENSOR = 0                                                 */
/*  - 기존 GY-86 onboard MS5611 한 개(기본 0x77)만 사용한다.                  */
/*                                                                            */
/*  USE_DOUBLE_BAROSENSOR = 1                                                 */
/*  - 같은 I2C1 bus 에 MS5611 두 개(0x77 + 0x76)를 둔다.                      */
/*  - driver 내부에서 pressure/temperature 를 평균 fuse 해서                   */
/*    APP_STATE에는 "하나의 가상 baro" 처럼 publish 한다.                     */
/*                                                                            */
/*  USE_DOUBLE_BAROSENSOR = 2                                                 */
/*  - I2C1 과 I2C2 에 각각 MS5611 하나씩 둔다.                                 */
/*  - 두 센서는 기본적으로 같은 주소를 써도 된다.                              */
/*  - 즉 "기존 onboard MS5611 +히 동일하고, 단지 source  I2C2 외부 MS5611" 구성을 겨냥한다.           */
/*  - fuse 방식은 mode 1과 완전bus만 하나 더 생긴다.   */
/* -------------------------------------------------------------------------- */
#ifndef USE_DOUBLE_BAROSENSOR
#define USE_DOUBLE_BAROSENSOR 0u
#endif

#ifndef GY86_MS5611_ADDR_PRIMARY
#define GY86_MS5611_ADDR_PRIMARY (0x77u << 1)
#endif

#ifndef GY86_MS5611_ADDR_SECONDARY
#define GY86_MS5611_ADDR_SECONDARY (0x76u << 1)
#endif

#ifndef GY86_MS5611_ADDR_I2C2
#define GY86_MS5611_ADDR_I2C2 GY86_MS5611_ADDR_PRIMARY
#endif

#if USE_DOUBLE_BAROSENSOR
#define GY86_IMU_MS5611_DEVICE_COUNT 2u
#else
#define GY86_IMU_MS5611_DEVICE_COUNT 1u
#endif

/* init 실패나 미탐지 시 재시도 주기 */
#ifndef GY86_IMU_RETRY_MS
#define GY86_IMU_RETRY_MS 1000u
#endif

/* 현재 HMC5883L 설정값 기준 75Hz 근처 */
#ifndef GY86_IMU_MAG_PERIOD_MS
#define GY86_IMU_MAG_PERIOD_MS 14u
#endif

/* MS5611 OSR4096 기준 pressure+temperature 한 세트가 약 20ms */
#ifndef GY86_IMU_BARO_PERIOD_MS
#define GY86_IMU_BARO_PERIOD_MS 20u
#endif

/* MS5611 single conversion wait */
#ifndef GY86_IMU_MS5611_CONV_MS
#define GY86_IMU_MS5611_CONV_MS 10u
#endif

/* -------------------------------------------------------------------------- */
/*  I2C robustness knobs                                                       */
/* -------------------------------------------------------------------------- */

/* 정상적인 register read/write는 수 ms 안에 끝나야 한다.
 * 30~60ms timeout은 bus가 꼬였을 때 main loop를 오래 붙잡는다. */
#ifndef GY86_IMU_I2C_TIMEOUT_MS
#define GY86_IMU_I2C_TIMEOUT_MS 5u
#endif

/* runtime 중 연속 오류가 이 값 이상이면
 * "그 backend는 잠시 offline" 으로 보고 재-probe 루트로 보낸다. */
#ifndef GY86_IMU_I2C_MAX_CONSECUTIVE_ERRORS
#define GY86_IMU_I2C_MAX_CONSECUTIVE_ERRORS 3u
#endif

/* runtime fail 후 다음 probe를 1초까지 끌지 않고 짧게 당겨온다. */
#ifndef GY86_IMU_REPROBE_AFTER_RUNTIME_FAIL_MS
#define GY86_IMU_REPROBE_AFTER_RUNTIME_FAIL_MS 100u
#endif

/* -------------------------------------------------------------------------- */
/*  STM32F405/407 errata 회피용                                                */
/*                                                                            */
/*  현재 CubeMX 값이 100kHz인 경우, driver init 시점에                         */
/*  80kHz로 한 번 더 덮어써서 88~100kHz 민감 구간을 피한다.                   */
/* -------------------------------------------------------------------------- */
#ifndef GY86_IMU_FORCE_SAFE_STANDARD_MODE_ON_100K
#define GY86_IMU_FORCE_SAFE_STANDARD_MODE_ON_100K 1u
#endif

#ifndef GY86_IMU_SAFE_STANDARD_MODE_HZ
#define GY86_IMU_SAFE_STANDARD_MODE_HZ 80000u
#endif

/* -------------------------------------------------------------------------- */
/*  MPU polling 기본값                                                         */
/*                                                                            */
/*  현재 MPU 설정이 DLPF=3(약 42~44Hz 대역폭)이므로,                           */
/*  100kHz bus에서 4ms(250Hz) polling은 얻는 정보보다 bus 부담이 더 크다.     */
/* -------------------------------------------------------------------------- */
#ifndef GY86_IMU_MPU_PERIOD_MS_AT_400K
#define GY86_IMU_MPU_PERIOD_MS_AT_400K 2u
#endif

#ifndef GY86_IMU_MPU_PERIOD_MS_AT_200K
#define GY86_IMU_MPU_PERIOD_MS_AT_200K 4u
#endif

#ifndef GY86_IMU_MPU_PERIOD_MS_AT_100K
#define GY86_IMU_MPU_PERIOD_MS_AT_100K 8u
#endif

/* -------------------------------------------------------------------------- */
/*  공개 API                                                                   */
/* -------------------------------------------------------------------------- */

/* 드라이버 내부 상태와 APP_STATE.gy86 slice를 초기 상태로 만든다. */
void GY86_IMU_Init(void);

/* SysTick 기반 monotonic scheduler.
 * now_ms는 반드시 HAL_GetTick() 계열 SysTick 값을 넣는다. */
void GY86_IMU_Task(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* GY86_IMU_H */
