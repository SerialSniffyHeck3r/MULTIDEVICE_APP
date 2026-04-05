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



/* magnetometer polling enable */
#ifndef GY86_IMU_ENABLE_MAGNETOMETER
#define GY86_IMU_ENABLE_MAGNETOMETER 1u
#endif

/* -------------------------------------------------------------------------- */
/*  alternate sensor-front-end compile-time switch                             */
/*                                                                            */
/*  CHIP_IS_NOT_GY86 = 0                                                      */
/*  - 현재 리포의 기존 경로를 그대로 사용한다.                                 */
/*  - 즉 accel/gyro=MPU6050, magnetometer=HMC5883L, baro=MS5611 이다.         */
/*                                                                            */
/*  CHIP_IS_NOT_GY86 = 1                                                      */
/*  - accel/gyro backend 를 LSM6DSOX 로 교체한다.                              */
/*  - magnetometer 는 기존 HMC5883L 경로를 그대로 유지한다.                   */
/*  - baro backend 를 BMP581 로 교체한다.                                      */
/*  - APP_STATE.gy86.* 의 공개 구조는 기존과 동일하게 유지하고,                */
/*    GY86_IMU.c 내부에서 신규 센서 값을 기존 slice 형식으로 재포장한다.       */
/* -------------------------------------------------------------------------- */
#ifndef CHIP_IS_NOT_GY86
#define CHIP_IS_NOT_GY86 0u
#endif

/* -------------------------------------------------------------------------- */
/*  per-sensor backend selection                                               */
/*                                                                            */
/*  실사용은 아래 3개 매크로로 고른다.                                         */
/*  - GY86_ACCELGYRO_BACKEND : MPU6050 / LSM6DSOX                             */
/*  - GY86_BARO_BACKEND      : MS5611 / BMP581                                */
/*  - GY86_BARO_BUS_ID       : I2C1 / I2C2                                    */
/*                                                                            */
/*  CHIP_IS_NOT_GY86 는 예전 "통째 교체" 스위치라서,                           */
/*  새 설정 방식에서는 0으로 두는 것을 권장한다.                               */
/* -------------------------------------------------------------------------- */
#define GY86_ACCELGYRO_BACKEND_MPU6050  0u
#define GY86_ACCELGYRO_BACKEND_LSM6DSOX 1u

#ifndef GY86_ACCELGYRO_BACKEND
#define GY86_ACCELGYRO_BACKEND GY86_ACCELGYRO_BACKEND_MPU6050
#endif

/* -------------------------------------------------------------------------- */
/*  barometer backend selector                                                 */
/*                                                                            */
/*  기본값은 "BMP581 를 I2C1 에 꽂아 바로 테스트" 구성이다.                    */
/* -------------------------------------------------------------------------- */
#define GY86_BARO_BACKEND_MS5611 0u
#define GY86_BARO_BACKEND_BMP581 1u

#ifndef GY86_BARO_BACKEND
#define GY86_BARO_BACKEND GY86_BARO_BACKEND_BMP581
#endif

#define GY86_BARO_BUS_I2C1 1u
#define GY86_BARO_BUS_I2C2 2u

#ifndef GY86_BARO_BUS_ID
#define GY86_BARO_BUS_ID GY86_BARO_BUS_I2C1
#endif

/* -------------------------------------------------------------------------- */
/*  alternate layout selector (CHIP_IS_NOT_GY86 = 1 일 때만 사용)             */
/*                                                                            */
/*  GY86_ALT_LAYOUT_ALL_ON_I2C1                                               */
/*  - LSM6DSOX + HMC5883L + BMP581 를 모두 I2C1 에 단다.                      */
/*                                                                            */
/*  GY86_ALT_LAYOUT_SPLIT_BARO_TO_I2C2                                        */
/*  - LSM6DSOX + HMC5883L 은 I2C1                                              */
/*  - BMP581 은 I2C2                                                           */
/*                                                                            */
/*  bus 자체를 더 세밀하게 바꾸고 싶으면 아래 BUS_ID define 을 각각 override   */
/*  하면 된다.                                                                 */
/* -------------------------------------------------------------------------- */
#define GY86_ALT_LAYOUT_ALL_ON_I2C1        0u
#define GY86_ALT_LAYOUT_SPLIT_BARO_TO_I2C2 1u

#ifndef GY86_ALT_LAYOUT
#define GY86_ALT_LAYOUT GY86_ALT_LAYOUT_ALL_ON_I2C1
#endif

/* -------------------------------------------------------------------------- */
/*  alternate sensor bus assignment                                            */
/* -------------------------------------------------------------------------- */
#ifndef GY86_ALT_LSM6DSOX_BUS_ID
#define GY86_ALT_LSM6DSOX_BUS_ID 1u
#endif

#ifndef GY86_ALT_HMC5883L_BUS_ID
#define GY86_ALT_HMC5883L_BUS_ID 1u
#endif

#ifndef GY86_ALT_BMP581_BUS_ID
#define GY86_ALT_BMP581_BUS_ID GY86_BARO_BUS_ID
#endif

/* -------------------------------------------------------------------------- */
/*  alternate sensor I2C addresses                                             */
/*                                                                            */
/*  HAL I2C는 7-bit address 를 left-shift 한 8-bit 형식을 기대한다.            */
/* -------------------------------------------------------------------------- */
#ifndef GY86_ALT_LSM6DSOX_ADDR
#define GY86_ALT_LSM6DSOX_ADDR (0x6Au << 1)
#endif

#ifndef GY86_ALT_HMC5883L_ADDR
#define GY86_ALT_HMC5883L_ADDR (0x1Eu << 1)
#endif

/* HMC5883L register defaults */
#ifndef GY86_ALT_HMC5883L_CONFIG_A
#define GY86_ALT_HMC5883L_CONFIG_A 0x78u /* 8-sample avg, 75Hz, normal */
#endif

#ifndef GY86_ALT_HMC5883L_CONFIG_B
#define GY86_ALT_HMC5883L_CONFIG_B 0x20u /* gain=1.3Ga */
#endif

#ifndef GY86_ALT_HMC5883L_MODE
#define GY86_ALT_HMC5883L_MODE 0x00u     /* continuous */
#endif

#ifndef GY86_ALT_BMP581_ADDR
#define GY86_ALT_BMP581_ADDR (0x47u << 1)
#endif

/* -------------------------------------------------------------------------- */
/*  alternate LSM6DSOX register-field defaults                                 */
/*                                                                            */
/*  accel_fs = 2 -> ±4g                                                        */
/*  gyro_fs  = 2 -> ±500dps                                                    */
/*                                                                            */
/*  이 조합은 기존 MPU6050 설정(±4g / ±500dps)과 가장 잘 맞는다.               */
/*  따라서 GY86_IMU.c 에서 raw 값을 기존 MPU6050 raw 스케일로 재매핑하기가      */
/*  쉽고, 상위 코드의 compatibility 가 좋다.                                   */
/* -------------------------------------------------------------------------- */
#ifndef GY86_ALT_LSM6DSOX_ACCEL_ODR
#define GY86_ALT_LSM6DSOX_ACCEL_ODR 4u   /* 104Hz */
#endif

#ifndef GY86_ALT_LSM6DSOX_GYRO_ODR
#define GY86_ALT_LSM6DSOX_GYRO_ODR 4u    /* 104Hz */
#endif

#ifndef GY86_ALT_LSM6DSOX_ACCEL_FS
#define GY86_ALT_LSM6DSOX_ACCEL_FS 2u    /* ±4g */
#endif

#ifndef GY86_ALT_LSM6DSOX_GYRO_FS
#define GY86_ALT_LSM6DSOX_GYRO_FS 2u     /* ±500dps */
#endif

#ifndef GY86_ALT_LSM6DSOX_ENABLE_LPF2
#define GY86_ALT_LSM6DSOX_ENABLE_LPF2 1u
#endif

#ifndef GY86_ALT_LSM6DSOX_ENABLE_BDU
#define GY86_ALT_LSM6DSOX_ENABLE_BDU 1u
#endif

#ifndef GY86_ALT_LSM6DSOX_ENABLE_AUTO_INC
#define GY86_ALT_LSM6DSOX_ENABLE_AUTO_INC 1u
#endif

#ifndef GY86_ALT_LSM6DSOX_DISABLE_I3C
#define GY86_ALT_LSM6DSOX_DISABLE_I3C 1u
#endif

/* -------------------------------------------------------------------------- */
/*  alternate BMP581 defaults                                                  */
/*                                                                            */
/*  권장 시작점                                                                */
/*  - temp OSR  = 1x                                                           */
/*  - press OSR = 8x                                                           */
/*  - ODR       = 약 100Hz                                                     */
/*  - IIR       = bypass                                                       */
/*                                                                            */
/*  이 조합은 "벤치에서 즉응성 있게 바리오를 보되, host 측 필터는 기존         */
/*  프로젝트가 담당한다" 는 목적에 맞춘 기본값이다.                           */
/* -------------------------------------------------------------------------- */
#ifndef GY86_ALT_BMP581_TEMP_OSR
#define GY86_ALT_BMP581_TEMP_OSR 0u       /* 1x */
#endif

#ifndef GY86_ALT_BMP581_PRESS_OSR
#define GY86_ALT_BMP581_PRESS_OSR 3u      /* 8x */
#endif

#ifndef GY86_ALT_BMP581_ODR
#define GY86_ALT_BMP581_ODR 0x0Au         /* about 100.2Hz */
#endif

#ifndef GY86_ALT_BMP581_IIR_TEMP
#define GY86_ALT_BMP581_IIR_TEMP 0u       /* bypass */
#endif

#ifndef GY86_ALT_BMP581_IIR_PRESS
#define GY86_ALT_BMP581_IIR_PRESS 0u      /* bypass */
#endif

#ifndef GY86_ALT_BMP581_POLL_MS
#define GY86_ALT_BMP581_POLL_MS 10u
#endif

#ifndef GY86_ALT_BMP581_ACCEPT_CHIP_ID_0x51
#define GY86_ALT_BMP581_ACCEPT_CHIP_ID_0x51 0u
#endif

/* publish plausibility window */
#ifndef GY86_ALT_BMP581_PUBLISH_MIN_PRESSURE_PA
#define GY86_ALT_BMP581_PUBLISH_MIN_PRESSURE_PA 30000
#endif

#ifndef GY86_ALT_BMP581_PUBLISH_MAX_PRESSURE_PA
#define GY86_ALT_BMP581_PUBLISH_MAX_PRESSURE_PA 125000
#endif

#ifndef GY86_ALT_BMP581_PUBLISH_MIN_TEMP_CDEG
#define GY86_ALT_BMP581_PUBLISH_MIN_TEMP_CDEG (-4000)
#endif

#ifndef GY86_ALT_BMP581_PUBLISH_MAX_TEMP_CDEG
#define GY86_ALT_BMP581_PUBLISH_MAX_TEMP_CDEG 8500
#endif

#ifndef GY86_ALT_BMP581_PUBLISH_STEP_FLOOR_PA
#define GY86_ALT_BMP581_PUBLISH_STEP_FLOOR_PA 60
#endif

#ifndef GY86_ALT_BMP581_PUBLISH_STEP_RATE_PA_PER_S
#define GY86_ALT_BMP581_PUBLISH_STEP_RATE_PA_PER_S 500u
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

#if (GY86_BARO_BACKEND == GY86_BARO_BACKEND_BMP581)
#define GY86_IMU_ACTIVE_BARO_PERIOD_MS GY86_ALT_BMP581_POLL_MS
#else
#define GY86_IMU_ACTIVE_BARO_PERIOD_MS GY86_IMU_BARO_PERIOD_MS
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
