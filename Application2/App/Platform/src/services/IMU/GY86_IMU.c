#include "GY86_IMU.h"

#include <string.h>

/* -------------------------------------------------------------------------- */
/*  STM32에서 사용시 I2C CLOCK을 88KHz 이하로 낮추거나 FAST 모드 사용
 * 지금은 FAST 모드 사용 중                                                   */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*  외부 I2C 핸들                                                              */
/* -------------------------------------------------------------------------- */

extern I2C_HandleTypeDef GY86_IMU_I2C_HANDLE;

/* -------------------------------------------------------------------------- */
/*  GY-86 내부 칩 주소                                                         */
/* -------------------------------------------------------------------------- */

/* HAL I2C는 7-bit address를 left shift 한 값을 기대한다. */
#define GY86_MPU6050_ADDR            (0x68u << 1)
#define GY86_HMC5883L_ADDR           (0x1Eu << 1)
#define GY86_MS5611_ADDR             (0x77u << 1)

/* -------------------------------------------------------------------------- */
/*  Magnetometer compile-time kill switch                                      */
/*                                                                            */
/*  이 제품에서는 magnetometer를 아예 사용하지 않으므로                         */
/*  low-level driver 레벨에서 probe / init / poll 자체를 막는다.               */
/*                                                                            */
/*  값이 0이면                                                                */
/*  - HMC5883L probe 안 함                                                     */
/*  - HMC5883L init 안 함                                                      */
/*  - HMC5883L poll 안 함                                                      */
/*  - APP_STATE debug에도 mag backend / poll period를 0으로 표시               */
/* -------------------------------------------------------------------------- */
#ifndef GY86_IMU_ENABLE_MAGNETOMETER
#define GY86_IMU_ENABLE_MAGNETOMETER 0u
#endif

/* -------------------------------------------------------------------------- */
/*  MPU6050 register map                                                       */
/* -------------------------------------------------------------------------- */

#define MPU6050_RA_SMPLRT_DIV        0x19u
#define MPU6050_RA_CONFIG            0x1Au
#define MPU6050_RA_GYRO_CONFIG       0x1Bu
#define MPU6050_RA_ACCEL_CONFIG      0x1Cu
#define MPU6050_RA_INT_PIN_CFG       0x37u
#define MPU6050_RA_INT_ENABLE        0x38u
#define MPU6050_RA_USER_CTRL         0x6Au
#define MPU6050_RA_PWR_MGMT_1        0x6Bu
#define MPU6050_RA_WHO_AM_I          0x75u
#define MPU6050_RA_ACCEL_XOUT_H      0x3Bu

/* -------------------------------------------------------------------------- */
/*  HMC5883L register map                                                      */
/* -------------------------------------------------------------------------- */

#define HMC5883L_RA_CONFIG_A         0x00u
#define HMC5883L_RA_CONFIG_B         0x01u
#define HMC5883L_RA_MODE             0x02u
#define HMC5883L_RA_DATA_X_H         0x03u
#define HMC5883L_RA_ID_A             0x0Au

/* -------------------------------------------------------------------------- */
/*  MS5611 command set                                                         */
/* -------------------------------------------------------------------------- */

#define MS5611_CMD_RESET             0x1Eu
#define MS5611_CMD_ADC_READ          0x00u
#define MS5611_CMD_CONV_D1_OSR4096   0x48u
#define MS5611_CMD_CONV_D2_OSR4096   0x58u
#define MS5611_CMD_PROM_READ_BASE    0xA0u

/* -------------------------------------------------------------------------- */
/*  backend 공통 타입                                                          */
/*                                                                            */
/*  포인트                                                                     */
/*  - accel/gyro, magnetometer, barometer를                                   */
/*    동일한 "init/poll function pointer" 형태로 감싼다.                      */
/*  - 나중에 칩이 바뀌면 이 backend 블록만 갈아끼면 되고,                       */
/*    main.c / APP_STATE / UI 페이지 구조는 그대로 유지할 수 있다.            */
/* -------------------------------------------------------------------------- */

typedef HAL_StatusTypeDef (*gy86_backend_init_fn_t)(app_gy86_state_t *imu);
typedef HAL_StatusTypeDef (*gy86_backend_poll_fn_t)(uint32_t now_ms,
                                                    app_gy86_state_t *imu,
                                                    uint8_t *updated);

typedef struct
{
    uint8_t               backend_id;   /* APP_IMU_BACKEND_* 값                  */
    gy86_backend_init_fn_t init;        /* probe + register init                 */
    gy86_backend_poll_fn_t poll;        /* raw read or state-machine poll        */
} gy86_backend_ops_t;

/* -------------------------------------------------------------------------- */
/*  드라이버 내부 런타임 상태                                                   */
/* -------------------------------------------------------------------------- */

typedef struct
{
    uint8_t  mpu_online;                /* MPU backend init 성공 여부             */
    uint8_t  mag_online;                /* MAG backend init 성공 여부             */
    uint8_t  baro_online;               /* BARO backend init 성공 여부            */

    /* ---------------------------------------------------------------------- */
    /*  runtime read 연속 실패 streak                                          */
    /*                                                                        */
    /*  기존 코드는 "init 한 번 성공하면 forever online" 이었다.               */
    /*  그건 실제 배선 노이즈/순간 탈락 상황에서 복구성이 떨어진다.            */
    /* ---------------------------------------------------------------------- */
    uint8_t  mpu_error_streak;
    uint8_t  mag_error_streak;
    uint8_t  baro_error_streak;

    uint32_t next_probe_ms;             /* 미탐지/미초기화 칩 재시도 시각         */
    uint32_t next_mpu_ms;               /* 다음 MPU polling 시각                  */
    uint32_t next_mag_ms;               /* 다음 MAG polling 시각                  */

    uint16_t mpu_period_ms;             /* 실제 선택된 accel/gyro polling 주기    */

    /* ---------------------------------------------------------------------- */
    /*  bus recovery debug                                                     */
    /* ---------------------------------------------------------------------- */
    uint32_t i2c_recovery_count;
    uint32_t last_i2c_recovery_ms;

    /* ------------------------------ */
    /*  MS5611 전용 state machine      */
    /* ------------------------------ */
    uint8_t  ms5611_phase;              /* 0:D1 start, 1:D1 read, 2:D2 read       */
    uint32_t ms5611_deadline_ms;        /* 다음 phase 진행 가능 시각              */
    uint32_t ms5611_d1_raw;             /* 직전 pressure ADC raw                  */
    uint32_t ms5611_d2_raw;             /* 직전 temperature ADC raw               */
} gy86_runtime_t;

static gy86_runtime_t s_gy86_rt;

/* -------------------------------------------------------------------------- */
/*  시간 관련 유틸                                                             */
/* -------------------------------------------------------------------------- */

/* SysTick 32-bit wrap-around 를 안전하게 처리하는 due 판정 */
static uint8_t GY86_TimeDue(uint32_t now_ms, uint32_t due_ms)
{
    return ((int32_t)(now_ms - due_ms) >= 0) ? 1u : 0u;
}

/* 주기 작업이 너무 밀린 경우 "오래된 슬롯" 을 무한히 따라잡지 않고
 * 현재 시각 기준으로 다음 슬롯 하나만 잡게 만드는 helper */
static uint32_t GY86_NextPeriodicDue(uint32_t previous_due_ms,
                                     uint16_t period_ms,
                                     uint32_t now_ms)
{
    uint32_t next_due_ms;

    if (period_ms == 0u)
    {
        return now_ms;
    }

    next_due_ms = previous_due_ms + (uint32_t)period_ms;

    if ((int32_t)(now_ms - next_due_ms) >= 0)
    {
        next_due_ms = now_ms + (uint32_t)period_ms;
    }

    return next_due_ms;
}

/* -------------------------------------------------------------------------- */
/*  I2C helper                                                                 */
/*                                                                            */
/*  이번 수정의 핵심                                                           */
/*  - HAL I2C timeout을 짧게 줄인다.                                           */
/*  - 실패하면 "그냥 에러 카운트만 증가"로 끝내지 않고                         */
/*    1) peripheral SWRST                                                      */
/*    2) GPIO open-drain 모드로 bus-unwedge                                   */
/*    3) HAL_I2C_DeInit / HAL_I2C_Init                                         */
/*    4) 같은 트랜잭션 1회 재시도                                              */
/*    순서로 복구를 시도한다.                                                  */
/*                                                                            */
/*  이유                                                                       */
/*  - 브레드보드 + 점퍼선 + 노이즈 환경에서는                                   */
/*    slave가 SDA를 붙잡은 채 남거나 controller BUSY state가 꼬일 수 있다.     */
/*  - main loop sensor task에서는                                              */
/*    "긴 timeout으로 버티기" 보다 "짧게 실패 + 명시적 recovery"가 낫다.       */
/* -------------------------------------------------------------------------- */

static void GY86_I2C_ShortDelay(void)
{
    volatile uint32_t n;

    /* ---------------------------------------------------------------------- */
    /*  recovery path 전용 아주 짧은 busy-wait                                 */
    /*  정확한 us delay가 목적이 아니라, GPIO edge 사이에 숨만 쉬게 하는 용도  */
    /* ---------------------------------------------------------------------- */
    for (n = 0u; n < 160u; n++)
    {
        __NOP();
    }
}

static void GY86_I2C_ForceBusPinsToGpioOdPullup(void)
{
    GPIO_InitTypeDef gpio_init;

    /* ---------------------------------------------------------------------- */
    /*  현재 프로젝트의 GY I2C1 핀은 PB6/PB7 이다.                              */
    /*  bus recovery 동안은 AF 대신 GPIO open-drain 으로 잠깐 되돌린다.        */
    /* ---------------------------------------------------------------------- */
    __HAL_RCC_GPIOB_CLK_ENABLE();

    memset(&gpio_init, 0, sizeof(gpio_init));
    gpio_init.Pin   = I2C_SCL_GY_Pin | I2C_SDA_GY_Pin;
    gpio_init.Mode  = GPIO_MODE_OUTPUT_OD;
    gpio_init.Pull  = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(GPIOB, &gpio_init);

    /* bus idle-high 상태 */
    HAL_GPIO_WritePin(I2C_SCL_GY_GPIO_Port, I2C_SCL_GY_Pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(I2C_SDA_GY_GPIO_Port, I2C_SDA_GY_Pin, GPIO_PIN_SET);
    GY86_I2C_ShortDelay();
}

static void GY86_I2C_BusUnwedge(void)
{
    uint8_t pulse_index;

    GY86_I2C_ForceBusPinsToGpioOdPullup();

    /* ---------------------------------------------------------------------- */
    /*  slave가 SDA를 low로 붙잡고 있으면 SCL을 몇 번 흔들어서                  */
    /*  내부 bit state machine이 남은 비트를 밀어내게 유도한다.                */
    /* ---------------------------------------------------------------------- */
    for (pulse_index = 0u; pulse_index < 9u; pulse_index++)
    {
        if (HAL_GPIO_ReadPin(I2C_SDA_GY_GPIO_Port, I2C_SDA_GY_Pin) == GPIO_PIN_SET)
        {
            break;
        }

        HAL_GPIO_WritePin(I2C_SCL_GY_GPIO_Port, I2C_SCL_GY_Pin, GPIO_PIN_RESET);
        GY86_I2C_ShortDelay();

        HAL_GPIO_WritePin(I2C_SCL_GY_GPIO_Port, I2C_SCL_GY_Pin, GPIO_PIN_SET);
        GY86_I2C_ShortDelay();
    }

    /* ---------------------------------------------------------------------- */
    /*  STOP 모양을 한 번 만들어서 bus를 idle 상태로 복귀시킨다.               */
    /* ---------------------------------------------------------------------- */
    HAL_GPIO_WritePin(I2C_SDA_GY_GPIO_Port, I2C_SDA_GY_Pin, GPIO_PIN_RESET);
    GY86_I2C_ShortDelay();

    HAL_GPIO_WritePin(I2C_SCL_GY_GPIO_Port, I2C_SCL_GY_Pin, GPIO_PIN_SET);
    GY86_I2C_ShortDelay();

    HAL_GPIO_WritePin(I2C_SDA_GY_GPIO_Port, I2C_SDA_GY_Pin, GPIO_PIN_SET);
    GY86_I2C_ShortDelay();
}

static void GY86_I2C_PeripheralSoftReset(void)
{
    /* ---------------------------------------------------------------------- */
    /*  STM32F4 I2C errata workaround 쪽에서 자주 쓰는 SWRST 경로              */
    /* ---------------------------------------------------------------------- */
    __HAL_RCC_I2C1_CLK_ENABLE();

    SET_BIT(GY86_IMU_I2C_HANDLE.Instance->CR1, I2C_CR1_SWRST);
    GY86_I2C_ShortDelay();
    CLEAR_BIT(GY86_IMU_I2C_HANDLE.Instance->CR1, I2C_CR1_SWRST);
    GY86_I2C_ShortDelay();
}

static void GY86_I2C_ApplySafeClockIfNeeded(void)
{
#if (GY86_IMU_FORCE_SAFE_STANDARD_MODE_ON_100K != 0u)
    /* ---------------------------------------------------------------------- */
    /*  STM32F405/407 errata는                                                 */
    /*  "controller standard mode 88~100kHz" 구간에서 repeated-start          */
    /*  timing 문제가 있을 수 있다고 적고 있다.                               */
    /*                                                                        */
    /*  CubeMX가 100000으로 다시 생성해도                                     */
    /*  driver init 시점에 80000으로 한 번 더 덮어써서 민감 구간을 피한다.     */
    /* ---------------------------------------------------------------------- */
    if ((GY86_IMU_I2C_HANDLE.Init.ClockSpeed >= 88000u) &&
        (GY86_IMU_I2C_HANDLE.Init.ClockSpeed <= 100000u))
    {
        (void)HAL_I2C_DeInit(&GY86_IMU_I2C_HANDLE);
        GY86_IMU_I2C_HANDLE.Init.ClockSpeed = GY86_IMU_SAFE_STANDARD_MODE_HZ;
        (void)HAL_I2C_Init(&GY86_IMU_I2C_HANDLE);
    }
#endif
}

static void GY86_I2C_RecoverBus(void)
{
    /* ---------------------------------------------------------------------- */
    /*  recovery debug 카운터                                                  */
    /* ---------------------------------------------------------------------- */
    s_gy86_rt.i2c_recovery_count++;
    s_gy86_rt.last_i2c_recovery_ms = HAL_GetTick();

    /* ---------------------------------------------------------------------- */
    /*  1) peripheral SWRST                                                    */
    /* ---------------------------------------------------------------------- */
    GY86_I2C_PeripheralSoftReset();

    /* ---------------------------------------------------------------------- */
    /*  2) HAL handle / peripheral deinit                                      */
    /* ---------------------------------------------------------------------- */
    (void)HAL_I2C_DeInit(&GY86_IMU_I2C_HANDLE);

    /* ---------------------------------------------------------------------- */
    /*  3) GPIO mode에서 bus-unwedge                                           */
    /* ---------------------------------------------------------------------- */
    GY86_I2C_BusUnwedge();

    /* ---------------------------------------------------------------------- */
    /*  4) HAL init로 peripheral 복구                                          */
    /* ---------------------------------------------------------------------- */
    (void)HAL_I2C_Init(&GY86_IMU_I2C_HANDLE);
}

static HAL_StatusTypeDef GY86_I2C_WriteU8(uint8_t dev_addr, uint8_t reg_addr, uint8_t value)
{
    HAL_StatusTypeDef st;

    st = HAL_I2C_Mem_Write(&GY86_IMU_I2C_HANDLE,
                           dev_addr,
                           reg_addr,
                           I2C_MEMADD_SIZE_8BIT,
                           &value,
                           1u,
                           GY86_IMU_I2C_TIMEOUT_MS);

    if (st != HAL_OK)
    {
        GY86_I2C_RecoverBus();

        st = HAL_I2C_Mem_Write(&GY86_IMU_I2C_HANDLE,
                               dev_addr,
                               reg_addr,
                               I2C_MEMADD_SIZE_8BIT,
                               &value,
                               1u,
                               GY86_IMU_I2C_TIMEOUT_MS);
    }

    return st;
}

static HAL_StatusTypeDef GY86_I2C_Read(uint8_t dev_addr,
                                       uint8_t reg_addr,
                                       uint8_t *buffer,
                                       uint16_t length)
{
    HAL_StatusTypeDef st;

    st = HAL_I2C_Mem_Read(&GY86_IMU_I2C_HANDLE,
                          dev_addr,
                          reg_addr,
                          I2C_MEMADD_SIZE_8BIT,
                          buffer,
                          length,
                          GY86_IMU_I2C_TIMEOUT_MS);

    if (st != HAL_OK)
    {
        GY86_I2C_RecoverBus();

        st = HAL_I2C_Mem_Read(&GY86_IMU_I2C_HANDLE,
                              dev_addr,
                              reg_addr,
                              I2C_MEMADD_SIZE_8BIT,
                              buffer,
                              length,
                              GY86_IMU_I2C_TIMEOUT_MS);
    }

    return st;
}

static HAL_StatusTypeDef GY86_I2C_CommandOnly(uint8_t dev_addr, uint8_t command)
{
    HAL_StatusTypeDef st;

    st = HAL_I2C_Master_Transmit(&GY86_IMU_I2C_HANDLE,
                                 dev_addr,
                                 &command,
                                 1u,
                                 GY86_IMU_I2C_TIMEOUT_MS);

    if (st != HAL_OK)
    {
        GY86_I2C_RecoverBus();

        st = HAL_I2C_Master_Transmit(&GY86_IMU_I2C_HANDLE,
                                     dev_addr,
                                     &command,
                                     1u,
                                     GY86_IMU_I2C_TIMEOUT_MS);
    }

    return st;
}

static HAL_StatusTypeDef GY86_I2C_ReadDirect(uint8_t dev_addr,
                                             uint8_t *buffer,
                                             uint16_t length)
{
    HAL_StatusTypeDef st;

    st = HAL_I2C_Master_Receive(&GY86_IMU_I2C_HANDLE,
                                dev_addr,
                                buffer,
                                length,
                                GY86_IMU_I2C_TIMEOUT_MS);

    if (st != HAL_OK)
    {
        GY86_I2C_RecoverBus();

        st = HAL_I2C_Master_Receive(&GY86_IMU_I2C_HANDLE,
                                    dev_addr,
                                    buffer,
                                    length,
                                    GY86_IMU_I2C_TIMEOUT_MS);
    }

    return st;
}

/* -------------------------------------------------------------------------- */
/*  accel/gyro polling 주기 선택                                                */
/*                                                                            */
/*  사용자의 요구는 "100Hz인지 모르겠지만 가능한 한 빠르게" 였다.              */
/*  그렇다고 CubeMX가 현재 100kHz로 만든 I2C를 무시하고 1kHz polling을         */
/*  박아 버리면 버스가 빡빡해질 수 있다.                                        */
/*                                                                            */
/*  그래서 현재 I2C1 clock 설정값을 보고                                       */
/*    - 400kHz 이상 : 1ms 목표                                                 */
/*    - 200kHz 이상 : 2ms 목표                                                 */
/*    - 그 이하     : 4ms 목표                                                 */
/*  로 자동 선택한다.                                                          */
/*                                                                            */
/*  즉 "지금 보드의 현실적인 최대치" 를 기본값으로 잡는다.                    */
/* -------------------------------------------------------------------------- */

static uint16_t GY86_SelectMpuPeriodMs(void)
{
    uint32_t i2c_hz;

    i2c_hz = GY86_IMU_I2C_HANDLE.Init.ClockSpeed;

    /* ---------------------------------------------------------------------- */
    /*  현재 MPU 설정은 DLPF=3 이라 accel/gyro 대역폭이 약 42~44Hz다.          */
    /*                                                                        */
    /*  즉 100kHz bus에서 4ms(250Hz) polling은                                 */
    /*  정보 이득보다 bus 스트레스를 더 크게 만들 가능성이 있다.               */
    /*                                                                        */
    /*  그래서 기본값을                                                        */
    /*    - 400kHz 이상 : 2ms                                                  */
    /*    - 200kHz 이상 : 4ms                                                  */
    /*    - 그 이하     : 8ms                                                  */
    /*  로 조정한다.                                                           */
    /* ---------------------------------------------------------------------- */
    if (i2c_hz >= 400000u)
    {
        return GY86_IMU_MPU_PERIOD_MS_AT_400K;
    }

    if (i2c_hz >= 200000u)
    {
        return GY86_IMU_MPU_PERIOD_MS_AT_200K;
    }

    return GY86_IMU_MPU_PERIOD_MS_AT_100K;
}

/* -------------------------------------------------------------------------- */
/*  MPU6050 backend                                                            */
/* -------------------------------------------------------------------------- */

static HAL_StatusTypeDef GY86_Mpu6050_Init(app_gy86_state_t *imu)
{
    HAL_StatusTypeDef st;
    uint8_t who_am_i;

    if (imu == 0)
    {
        return HAL_ERROR;
    }

    /* ------------------------------ */
    /*  WHO_AM_I로 먼저 존재 확인      */
    /* ------------------------------ */
    st = GY86_I2C_Read(GY86_MPU6050_ADDR, MPU6050_RA_WHO_AM_I, &who_am_i, 1u);
    imu->debug.last_hal_status_mpu = (uint8_t)st;

    if (st != HAL_OK)
    {
        return st;
    }

    imu->debug.mpu_whoami = who_am_i;
    imu->debug.detected_mask |= APP_GY86_DEVICE_MPU;

    /* MPU6050/MPU6000 계열은 보통 0x68 또는 0x69 */
    if ((who_am_i != 0x68u) && (who_am_i != 0x69u))
    {
        return HAL_ERROR;
    }

    /* ------------------------------ */
    /*  sleep 해제 + PLL clock 선택    */
    /* ------------------------------ */
    st = GY86_I2C_WriteU8(GY86_MPU6050_ADDR, MPU6050_RA_PWR_MGMT_1, 0x01u);
    imu->debug.last_hal_status_mpu = (uint8_t)st;
    if (st != HAL_OK)
    {
        return st;
    }

    HAL_Delay(10u);

    /* ---------------------------------------------------------------------- */
    /*  SMPLRT_DIV = 0                                                         */
    /*  - DLPF on 상태에서는 내부 1kHz sample domain                           */
    /*  - 실제 MCU polling 주기는 위에서 선택한 mpu_period_ms가 결정           */
    /* ---------------------------------------------------------------------- */
    st = GY86_I2C_WriteU8(GY86_MPU6050_ADDR, MPU6050_RA_SMPLRT_DIV, 0x00u);
    if (st != HAL_OK) { imu->debug.last_hal_status_mpu = (uint8_t)st; return st; }

    /* DLPF = 3 -> gyro ~42Hz, accel ~44Hz 대역폭 */
    st = GY86_I2C_WriteU8(GY86_MPU6050_ADDR, MPU6050_RA_CONFIG, 0x03u);
    if (st != HAL_OK) { imu->debug.last_hal_status_mpu = (uint8_t)st; return st; }

    /* Gyro full scale = +/-500dps */
    st = GY86_I2C_WriteU8(GY86_MPU6050_ADDR, MPU6050_RA_GYRO_CONFIG, 0x08u);
    if (st != HAL_OK) { imu->debug.last_hal_status_mpu = (uint8_t)st; return st; }

    /* Accel full scale = +/-4g */
    st = GY86_I2C_WriteU8(GY86_MPU6050_ADDR, MPU6050_RA_ACCEL_CONFIG, 0x08u);
    if (st != HAL_OK) { imu->debug.last_hal_status_mpu = (uint8_t)st; return st; }

    /* AUX I2C bypass 허용 */
    st = GY86_I2C_WriteU8(GY86_MPU6050_ADDR, MPU6050_RA_USER_CTRL, 0x00u);
    if (st != HAL_OK) { imu->debug.last_hal_status_mpu = (uint8_t)st; return st; }

    st = GY86_I2C_WriteU8(GY86_MPU6050_ADDR, MPU6050_RA_INT_PIN_CFG, 0x02u);
    if (st != HAL_OK) { imu->debug.last_hal_status_mpu = (uint8_t)st; return st; }

    st = GY86_I2C_WriteU8(GY86_MPU6050_ADDR, MPU6050_RA_INT_ENABLE, 0x00u);
    if (st != HAL_OK) { imu->debug.last_hal_status_mpu = (uint8_t)st; return st; }

    imu->debug.accelgyro_backend_id = APP_IMU_BACKEND_MPU6050;
    imu->debug.init_ok_mask |= APP_GY86_DEVICE_MPU;
    imu->debug.last_hal_status_mpu = (uint8_t)HAL_OK;

    return HAL_OK;
}

static HAL_StatusTypeDef GY86_Mpu6050_Poll(uint32_t now_ms,
                                           app_gy86_state_t *imu,
                                           uint8_t *updated)
{
    HAL_StatusTypeDef st;
    uint8_t buffer[14];
    int16_t raw_accel_x;
    int16_t raw_accel_y;
    int16_t raw_accel_z;
    int16_t raw_temp;
    int16_t raw_gyro_x;
    int16_t raw_gyro_y;
    int16_t raw_gyro_z;

    if (updated != 0)
    {
        *updated = 0u;
    }

    if (imu == 0)
    {
        return HAL_ERROR;
    }

    st = GY86_I2C_Read(GY86_MPU6050_ADDR, MPU6050_RA_ACCEL_XOUT_H, buffer, sizeof(buffer));
    imu->debug.last_hal_status_mpu = (uint8_t)st;

    if (st != HAL_OK)
    {
        /* ------------------------------------------------------------------ */
        /*  단순 error count 증가만 하지 말고                                  */
        /*  runtime 연속 실패 streak를 기록해서                                */
        /*  일정 횟수 이상이면 offline -> re-probe 로 보낸다.                  */
        /* ------------------------------------------------------------------ */
        imu->debug.mpu_error_count++;
        imu->status_flags &= (uint8_t)~APP_GY86_STATUS_MPU_VALID;

        GY86_RecordRuntimeErrorAndMaybeOffline(APP_GY86_DEVICE_MPU,
                                               &s_gy86_rt.mpu_online,
                                               &s_gy86_rt.mpu_error_streak,
                                               now_ms,
                                               imu);
        return st;
    }
    raw_accel_x = (int16_t)((buffer[0]  << 8) | buffer[1]);
    raw_accel_y = (int16_t)((buffer[2]  << 8) | buffer[3]);
    raw_accel_z = (int16_t)((buffer[4]  << 8) | buffer[5]);
    raw_temp    = (int16_t)((buffer[6]  << 8) | buffer[7]);
    raw_gyro_x  = (int16_t)((buffer[8]  << 8) | buffer[9]);
    raw_gyro_y  = (int16_t)((buffer[10] << 8) | buffer[11]);
    raw_gyro_z  = (int16_t)((buffer[12] << 8) | buffer[13]);

    /* datasheet: Temp[degC] = 36.53 + raw / 340 */
    imu->mpu.timestamp_ms  = now_ms;
    imu->mpu.sample_count++;
    imu->mpu.accel_x_raw   = raw_accel_x;
    imu->mpu.accel_y_raw   = raw_accel_y;
    imu->mpu.accel_z_raw   = raw_accel_z;
    imu->mpu.gyro_x_raw    = raw_gyro_x;
    imu->mpu.gyro_y_raw    = raw_gyro_y;
    imu->mpu.gyro_z_raw    = raw_gyro_z;
    imu->mpu.temp_raw      = raw_temp;
    imu->mpu.temp_cdeg     = (int16_t)(3653 + (((int32_t)raw_temp * 100) / 340));

    imu->debug.mpu_last_ok_ms = now_ms;
    imu->status_flags        |= APP_GY86_STATUS_MPU_VALID;
    imu->last_update_ms       = now_ms;

    if (updated != 0)
    {
        *updated = 1u;
    }

    return HAL_OK;
}

/* -------------------------------------------------------------------------- */
/*  HMC5883L backend                                                           */
/* -------------------------------------------------------------------------- */

static HAL_StatusTypeDef GY86_Hmc5883l_Init(app_gy86_state_t *imu)
{
    HAL_StatusTypeDef st;
    uint8_t id_bytes[3];

    if (imu == 0)
    {
        return HAL_ERROR;
    }

    /* HMC5883L ID = 'H' '4' '3' */
    st = GY86_I2C_Read(GY86_HMC5883L_ADDR, HMC5883L_RA_ID_A, id_bytes, sizeof(id_bytes));
    imu->debug.last_hal_status_mag = (uint8_t)st;

    if (st != HAL_OK)
    {
        return st;
    }

    imu->debug.mag_id_a = id_bytes[0];
    imu->debug.mag_id_b = id_bytes[1];
    imu->debug.mag_id_c = id_bytes[2];
    imu->debug.detected_mask |= APP_GY86_DEVICE_MAG;

    if ((id_bytes[0] != 0x48u) || (id_bytes[1] != 0x34u) || (id_bytes[2] != 0x33u))
    {
        return HAL_ERROR;
    }

    /* 8-sample average, 75Hz, normal measurement */
    st = GY86_I2C_WriteU8(GY86_HMC5883L_ADDR, HMC5883L_RA_CONFIG_A, 0x78u);
    if (st != HAL_OK) { imu->debug.last_hal_status_mag = (uint8_t)st; return st; }

    /* gain = 1.3Ga */
    st = GY86_I2C_WriteU8(GY86_HMC5883L_ADDR, HMC5883L_RA_CONFIG_B, 0x20u);
    if (st != HAL_OK) { imu->debug.last_hal_status_mag = (uint8_t)st; return st; }

    /* continuous conversion mode */
    st = GY86_I2C_WriteU8(GY86_HMC5883L_ADDR, HMC5883L_RA_MODE, 0x00u);
    if (st != HAL_OK) { imu->debug.last_hal_status_mag = (uint8_t)st; return st; }

    imu->debug.mag_backend_id = APP_IMU_BACKEND_HMC5883L;
    imu->debug.init_ok_mask |= APP_GY86_DEVICE_MAG;
    imu->debug.last_hal_status_mag = (uint8_t)HAL_OK;

    return HAL_OK;
}

static HAL_StatusTypeDef GY86_Hmc5883l_Poll(uint32_t now_ms,
                                            app_gy86_state_t *imu,
                                            uint8_t *updated)
{
    HAL_StatusTypeDef st;
    uint8_t buffer[6];

    if (updated != 0)
    {
        *updated = 0u;
    }

    if (imu == 0)
    {
        return HAL_ERROR;
    }

    st = GY86_I2C_Read(GY86_HMC5883L_ADDR, HMC5883L_RA_DATA_X_H, buffer, sizeof(buffer));
    imu->debug.last_hal_status_mag = (uint8_t)st;

    if (st != HAL_OK)
    {
        imu->debug.mag_error_count++;
        imu->status_flags &= (uint8_t)~APP_GY86_STATUS_MAG_VALID;

        GY86_RecordRuntimeErrorAndMaybeOffline(APP_GY86_DEVICE_MAG,
                                               &s_gy86_rt.mag_online,
                                               &s_gy86_rt.mag_error_streak,
                                               now_ms,
                                               imu);
        return st;
    }

    /* 이번 sample은 정상 수신이므로 연속 실패 streak를 지운다. */
    s_gy86_rt.mag_error_streak = 0u;

    /* HMC5883L 출력 순서는 X, Z, Y */
    imu->mag.timestamp_ms = now_ms;
    imu->mag.sample_count++;
    imu->mag.mag_x_raw = (int16_t)((buffer[0] << 8) | buffer[1]);
    imu->mag.mag_z_raw = (int16_t)((buffer[2] << 8) | buffer[3]);
    imu->mag.mag_y_raw = (int16_t)((buffer[4] << 8) | buffer[5]);

    imu->debug.mag_last_ok_ms = now_ms;
    imu->status_flags        |= APP_GY86_STATUS_MAG_VALID;
    imu->last_update_ms       = now_ms;



    if (updated != 0)
    {
        *updated = 1u;
    }

    return HAL_OK;
}

/* -------------------------------------------------------------------------- */
/*  MS5611 backend                                                             */
/* -------------------------------------------------------------------------- */

static HAL_StatusTypeDef GY86_Ms5611_ReadAdc(uint32_t *raw_adc)
{
    HAL_StatusTypeDef st;
    uint8_t read_command;
    uint8_t buffer[3];

    if (raw_adc == 0)
    {
        return HAL_ERROR;
    }

    read_command = MS5611_CMD_ADC_READ;

    st = GY86_I2C_CommandOnly(GY86_MS5611_ADDR, read_command);
    if (st != HAL_OK)
    {
        return st;
    }

    st = GY86_I2C_ReadDirect(GY86_MS5611_ADDR, buffer, sizeof(buffer));
    if (st != HAL_OK)
    {
        return st;
    }

    *raw_adc = ((uint32_t)buffer[0] << 16) |
               ((uint32_t)buffer[1] << 8)  |
               ((uint32_t)buffer[2]);

    return HAL_OK;
}

static HAL_StatusTypeDef GY86_Ms5611_Init(app_gy86_state_t *imu)
{
    HAL_StatusTypeDef st;
    uint8_t command;
    uint8_t buffer[2];
    uint8_t prom_index;

    if (imu == 0)
    {
        return HAL_ERROR;
    }

    /* command respond 자체를 간단한 probe로 사용 */
    command = MS5611_CMD_RESET;
    st = GY86_I2C_CommandOnly(GY86_MS5611_ADDR, command);
    imu->debug.last_hal_status_baro = (uint8_t)st;

    if (st != HAL_OK)
    {
        return st;
    }

    imu->debug.detected_mask |= APP_GY86_DEVICE_BARO;

    HAL_Delay(3u);

    /* PROM C1..C6 읽기 */
    memset(imu->baro.prom_c, 0, sizeof(imu->baro.prom_c));

    for (prom_index = 1u; prom_index <= 6u; prom_index++)
    {
        command = (uint8_t)(MS5611_CMD_PROM_READ_BASE + (prom_index * 2u));

        st = GY86_I2C_CommandOnly(GY86_MS5611_ADDR, command);
        if (st != HAL_OK)
        {
            imu->debug.last_hal_status_baro = (uint8_t)st;
            return st;
        }

        st = GY86_I2C_ReadDirect(GY86_MS5611_ADDR, buffer, sizeof(buffer));
        if (st != HAL_OK)
        {
            imu->debug.last_hal_status_baro = (uint8_t)st;
            return st;
        }

        imu->baro.prom_c[prom_index] = (uint16_t)((buffer[0] << 8) | buffer[1]);
    }

    /* coeff가 전부 0인 경우는 정상 보드로 보기 어렵다. */
    if ((imu->baro.prom_c[1] == 0u) &&
        (imu->baro.prom_c[2] == 0u) &&
        (imu->baro.prom_c[3] == 0u) &&
        (imu->baro.prom_c[4] == 0u) &&
        (imu->baro.prom_c[5] == 0u) &&
        (imu->baro.prom_c[6] == 0u))
    {
        return HAL_ERROR;
    }

    s_gy86_rt.ms5611_phase       = 0u;
    s_gy86_rt.ms5611_deadline_ms = 0u;
    s_gy86_rt.ms5611_d1_raw      = 0u;
    s_gy86_rt.ms5611_d2_raw      = 0u;

    imu->debug.ms5611_state      = 0u;
    imu->debug.baro_backend_id   = APP_IMU_BACKEND_MS5611;
    imu->debug.init_ok_mask     |= APP_GY86_DEVICE_BARO;
    imu->debug.last_hal_status_baro = (uint8_t)HAL_OK;

    return HAL_OK;
}

static HAL_StatusTypeDef GY86_Ms5611_Poll(uint32_t now_ms,
                                          app_gy86_state_t *imu,
                                          uint8_t *updated)
{
    HAL_StatusTypeDef st;
    uint8_t command;

    if (updated != 0)
    {
        *updated = 0u;
    }

    if (imu == 0)
    {
        return HAL_ERROR;
    }

    switch (s_gy86_rt.ms5611_phase)
    {
    case 0u:
        /* -------------------------- */
        /*  pressure conversion 시작   */
        /* -------------------------- */
        command = MS5611_CMD_CONV_D1_OSR4096;
        st = GY86_I2C_CommandOnly(GY86_MS5611_ADDR, command);
        imu->debug.last_hal_status_baro = (uint8_t)st;

        if (st != HAL_OK)
        {
            imu->debug.baro_error_count++;
            imu->status_flags &= (uint8_t)~APP_GY86_STATUS_BARO_VALID;

            GY86_RecordRuntimeErrorAndMaybeOffline(APP_GY86_DEVICE_BARO,
                                                   &s_gy86_rt.baro_online,
                                                   &s_gy86_rt.baro_error_streak,
                                                   now_ms,
                                                   imu);
            return st;
        }

        s_gy86_rt.ms5611_deadline_ms = now_ms + GY86_IMU_MS5611_CONV_MS;
        s_gy86_rt.ms5611_phase = 1u;
        imu->debug.ms5611_state = 1u;
        break;

    case 1u:
        if (GY86_TimeDue(now_ms, s_gy86_rt.ms5611_deadline_ms) == 0u)
        {
            break;
        }

        st = GY86_Ms5611_ReadAdc(&s_gy86_rt.ms5611_d1_raw);
        imu->debug.last_hal_status_baro = (uint8_t)st;

        if (st != HAL_OK)
        {
            s_gy86_rt.ms5611_phase = 0u;
            imu->debug.ms5611_state = 0u;
            imu->debug.baro_error_count++;
            imu->status_flags &= (uint8_t)~APP_GY86_STATUS_BARO_VALID;

            GY86_RecordRuntimeErrorAndMaybeOffline(APP_GY86_DEVICE_BARO,
                                                   &s_gy86_rt.baro_online,
                                                   &s_gy86_rt.baro_error_streak,
                                                   now_ms,
                                                   imu);
            return st;
        }

        command = MS5611_CMD_CONV_D2_OSR4096;
        st = GY86_I2C_CommandOnly(GY86_MS5611_ADDR, command);
        imu->debug.last_hal_status_baro = (uint8_t)st;

        if (st != HAL_OK)
        {
            s_gy86_rt.ms5611_phase = 0u;
            imu->debug.ms5611_state = 0u;
            imu->debug.baro_error_count++;
            imu->status_flags &= (uint8_t)~APP_GY86_STATUS_BARO_VALID;

            GY86_RecordRuntimeErrorAndMaybeOffline(APP_GY86_DEVICE_BARO,
                                                   &s_gy86_rt.baro_online,
                                                   &s_gy86_rt.baro_error_streak,
                                                   now_ms,
                                                   imu);
            return st;
        }

        s_gy86_rt.ms5611_deadline_ms = now_ms + GY86_IMU_MS5611_CONV_MS;
        s_gy86_rt.ms5611_phase = 2u;
        imu->debug.ms5611_state = 2u;
        break;

    case 2u:
        if (GY86_TimeDue(now_ms, s_gy86_rt.ms5611_deadline_ms) == 0u)
        {
            break;
        }

        st = GY86_Ms5611_ReadAdc(&s_gy86_rt.ms5611_d2_raw);
        imu->debug.last_hal_status_baro = (uint8_t)st;

        if (st != HAL_OK)
        {
            s_gy86_rt.ms5611_phase = 0u;
            imu->debug.ms5611_state = 0u;
            imu->debug.baro_error_count++;
            imu->status_flags &= (uint8_t)~APP_GY86_STATUS_BARO_VALID;

            GY86_RecordRuntimeErrorAndMaybeOffline(APP_GY86_DEVICE_BARO,
                                                   &s_gy86_rt.baro_online,
                                                   &s_gy86_rt.baro_error_streak,
                                                   now_ms,
                                                   imu);
            return st;
        }

        /* ------------------------------------------------------------------ */
        /*  datasheet 정식 공식 사용                                            */
        /*  - 1차 보정                                                         */
        /*  - 저온 구간 2차 보정                                               */
        /* ------------------------------------------------------------------ */
        {
            int32_t dT;
            int32_t temp_cdeg;
            int64_t off;
            int64_t sens;
            int64_t t2;
            int64_t off2;
            int64_t sens2;
            int32_t pressure_hpa_x100;
            int32_t pressure_pa;
            int32_t temp_low;
            int32_t temp_very_low;

            dT = (int32_t)s_gy86_rt.ms5611_d2_raw -
                 (int32_t)((uint32_t)imu->baro.prom_c[5] * 256u);

            temp_cdeg = 2000 +
                        (int32_t)(((int64_t)dT * (int64_t)imu->baro.prom_c[6]) / 8388608LL);

            off = ((int64_t)imu->baro.prom_c[2] * 65536LL) +
                  (((int64_t)imu->baro.prom_c[4] * (int64_t)dT) / 128LL);

            sens = ((int64_t)imu->baro.prom_c[1] * 32768LL) +
                   (((int64_t)imu->baro.prom_c[3] * (int64_t)dT) / 256LL);

            t2    = 0LL;
            off2  = 0LL;
            sens2 = 0LL;

            if (temp_cdeg < 2000)
            {
                temp_low = temp_cdeg - 2000;
                t2    = (((int64_t)dT * (int64_t)dT) >> 31);
                off2  = (5LL * (int64_t)temp_low * (int64_t)temp_low) >> 1;
                sens2 = (5LL * (int64_t)temp_low * (int64_t)temp_low) >> 2;

                if (temp_cdeg < -1500)
                {
                    temp_very_low = temp_cdeg + 1500;
                    off2  += 7LL  * (int64_t)temp_very_low * (int64_t)temp_very_low;
                    sens2 += (11LL * (int64_t)temp_very_low * (int64_t)temp_very_low) >> 1;
                }
            }

            temp_cdeg -= (int32_t)t2;
            off       -= off2;
            sens      -= sens2;

            pressure_hpa_x100 = (int32_t)(((((int64_t)s_gy86_rt.ms5611_d1_raw * sens) /
                                            2097152LL) - off) / 32768LL);

            pressure_pa = pressure_hpa_x100 * 100;

            imu->baro.timestamp_ms      = now_ms;
            imu->baro.sample_count++;
            imu->baro.d1_raw            = s_gy86_rt.ms5611_d1_raw;
            imu->baro.d2_raw            = s_gy86_rt.ms5611_d2_raw;
            imu->baro.temp_cdeg         = temp_cdeg;
            imu->baro.pressure_hpa_x100 = pressure_hpa_x100;
            imu->baro.pressure_pa       = pressure_pa;
        }

        s_gy86_rt.ms5611_phase = 0u;
        imu->debug.ms5611_state = 0u;

        imu->debug.baro_last_ok_ms = now_ms;
        imu->status_flags         |= APP_GY86_STATUS_BARO_VALID;
        imu->last_update_ms        = now_ms;

        /* full sample 한 세트가 정상 완료됐으므로 streak reset */
        s_gy86_rt.baro_error_streak = 0u;

        if (updated != 0)
        {
            *updated = 1u;
        }
        break;

    default:
        s_gy86_rt.ms5611_phase = 0u;
        imu->debug.ms5611_state = 0u;
        break;
    }

    return HAL_OK;
}

/* -------------------------------------------------------------------------- */
/*  backend 선택 테이블                                                        */
/* -------------------------------------------------------------------------- */

static const gy86_backend_ops_t s_accelgyro_backend =
{
    APP_IMU_BACKEND_MPU6050,
    GY86_Mpu6050_Init,
    GY86_Mpu6050_Poll
};

static const gy86_backend_ops_t s_mag_backend =
{
    APP_IMU_BACKEND_HMC5883L,
    GY86_Hmc5883l_Init,
    GY86_Hmc5883l_Poll
};

static const gy86_backend_ops_t s_baro_backend =
{
    APP_IMU_BACKEND_MS5611,
    GY86_Ms5611_Init,
    GY86_Ms5611_Poll
};

/* -------------------------------------------------------------------------- */
/*  내부 helper: APP_STATE.gy86 slice 초기화                                    */
/* -------------------------------------------------------------------------- */

static void GY86_ResetAppStateSlice(app_gy86_state_t *imu)
{
    if (imu == 0)
    {
        return;
    }

    memset(imu, 0, sizeof(*imu));

    imu->initialized = false;
    imu->status_flags = 0u;
    imu->last_update_ms = 0u;

    imu->debug.accelgyro_backend_id = APP_IMU_BACKEND_NONE;
    imu->debug.mag_backend_id       = APP_IMU_BACKEND_NONE;
    imu->debug.baro_backend_id      = APP_IMU_BACKEND_NONE;

    imu->debug.mpu_poll_period_ms  = s_gy86_rt.mpu_period_ms;

#if GY86_IMU_ENABLE_MAGNETOMETER
    imu->debug.mag_poll_period_ms  = GY86_IMU_MAG_PERIOD_MS;
#else
    imu->debug.mag_poll_period_ms  = 0u;
#endif

    imu->debug.baro_poll_period_ms = GY86_IMU_BARO_PERIOD_MS;
}

/* -------------------------------------------------------------------------- */
/*  runtime online 상태 반영 helper                                            */
/*                                                                            */
/*  기존 코드는 init_ok_mask만 보고 "initialized=true" 를 유지했는데,          */
/*  그건 runtime 중 실제 통신이 다 죽어도 여전히 initialized=true 로 남는다.   */
/* -------------------------------------------------------------------------- */

static void GY86_UpdateInitializedFlag(app_gy86_state_t *imu)
{
    if (imu == 0)
    {
        return;
    }

    imu->initialized =
        ((s_gy86_rt.mpu_online  != 0u) ||
         (s_gy86_rt.mag_online  != 0u) ||
         (s_gy86_rt.baro_online != 0u)) ? true : false;
}

static void GY86_MarkBackendOffline(uint8_t device_mask,
                                    uint8_t *online_flag,
                                    uint8_t *error_streak,
                                    uint32_t now_ms,
                                    app_gy86_state_t *imu)
{
    if ((online_flag == 0) || (error_streak == 0) || (imu == 0))
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  1) 현재 backend를 offline 으로 내려서                                  */
    /*     주기 poll 대신 re-probe 루트로 보낸다.                              */
    /* ---------------------------------------------------------------------- */
    *online_flag  = 0u;
    *error_streak = 0u;

    /* ---------------------------------------------------------------------- */
    /*  2) 해당 raw valid bit를 내린다.                                        */
    /* ---------------------------------------------------------------------- */
    if (device_mask == APP_GY86_DEVICE_MPU)
    {
        imu->status_flags &= (uint8_t)~APP_GY86_STATUS_MPU_VALID;
    }
    else if (device_mask == APP_GY86_DEVICE_MAG)
    {
        imu->status_flags &= (uint8_t)~APP_GY86_STATUS_MAG_VALID;
    }
    else if (device_mask == APP_GY86_DEVICE_BARO)
    {
        imu->status_flags &= (uint8_t)~APP_GY86_STATUS_BARO_VALID;
    }

    /* ---------------------------------------------------------------------- */
    /*  3) 1초까지 기다리지 않고, 짧은 시간 뒤 바로 re-probe 하게 당겨 온다.    */
    /* ---------------------------------------------------------------------- */
    s_gy86_rt.next_probe_ms = now_ms + GY86_IMU_REPROBE_AFTER_RUNTIME_FAIL_MS;

    GY86_UpdateInitializedFlag(imu);
}

void GY86_RecordRuntimeErrorAndMaybeOffline(uint8_t device_mask,
                                                   uint8_t *online_flag,
                                                   uint8_t *error_streak,
                                                   uint32_t now_ms,
                                                   app_gy86_state_t *imu)
{
    if ((error_streak == 0) || (imu == 0))
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  연속 실패 streak 증가                                                  */
    /* ---------------------------------------------------------------------- */
    if (*error_streak < 255u)
    {
        (*error_streak)++;
    }

    /* ---------------------------------------------------------------------- */
    /*  연속 실패가 threshold를 넘으면                                         */
    /*  "그냥 계속 read만 두드리는" 대신 offline -> re-probe 로 전환한다.      */
    /* ---------------------------------------------------------------------- */
    if (*error_streak >= GY86_IMU_I2C_MAX_CONSECUTIVE_ERRORS)
    {
        GY86_MarkBackendOffline(device_mask,
                                online_flag,
                                error_streak,
                                now_ms,
                                imu);
    }
    else
    {
        GY86_UpdateInitializedFlag(imu);
    }
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: 미초기화 칩 probe                                              */
/* -------------------------------------------------------------------------- */

static void GY86_ProbeMissingBackends(uint32_t now_ms, app_gy86_state_t *imu)
{
    HAL_StatusTypeDef st;

    if (imu == 0)
    {
        return;
    }

    imu->debug.init_attempt_count++;
    imu->debug.last_init_attempt_ms = now_ms;

    /* ------------------------------ */
    /*  MPU probe/init                 */
    /* ------------------------------ */
    if (s_gy86_rt.mpu_online == 0u)
    {
        st = s_accelgyro_backend.init(imu);

        if (st == HAL_OK)
        {
            s_gy86_rt.mpu_online = 1u;
            s_gy86_rt.next_mpu_ms = now_ms;

            s_gy86_rt.mpu_error_streak = 0u;
        }
        else
        {
            imu->debug.mpu_error_count++;
        }
    }

        /* ------------------------------ */
        /*  MAG probe/init                 */
        /* ------------------------------ */
    #if GY86_IMU_ENABLE_MAGNETOMETER
        if (s_gy86_rt.mag_online == 0u)
        {
            st = s_mag_backend.init(imu);

            if (st == HAL_OK)
            {
                s_gy86_rt.mag_online = 1u;
                s_gy86_rt.next_mag_ms = now_ms;

                s_gy86_rt.mag_error_streak = 0u;
            }
            else
            {
                imu->debug.mag_error_count++;
            }
        }
    #else
        s_gy86_rt.mag_online = 0u;
        s_gy86_rt.next_mag_ms = 0u;
    #endif

    /* ------------------------------ */
    /*  BARO probe/init                */
    /* ------------------------------ */
    if (s_gy86_rt.baro_online == 0u)
    {
        st = s_baro_backend.init(imu);

        if (st == HAL_OK)
        {
            s_gy86_rt.baro_online = 1u;

            s_gy86_rt.baro_error_streak = 0u;
        }
        else
        {
            imu->debug.baro_error_count++;
        }
    }

    /* ---------------------------------------------------------------------- */
    /*  "과거에 한 번이라도 init 성공했는가" 가 아니라                         */
    /*  "지금 실제로 살아 있는 backend가 하나라도 있는가" 로 initialized를 본다 */
    /* ---------------------------------------------------------------------- */
    GY86_UpdateInitializedFlag(imu);
    s_gy86_rt.next_probe_ms = now_ms + GY86_IMU_RETRY_MS;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: init                                                              */
/* -------------------------------------------------------------------------- */

void GY86_IMU_Init(void)
{
    app_gy86_state_t *imu;

    imu = (app_gy86_state_t *)&g_app_state.gy86;

    /* ---------------------------------------------------------------------- */
    /*  드라이버 런타임 상태 초기화                                             */
    /* ---------------------------------------------------------------------- */
    memset(&s_gy86_rt, 0, sizeof(s_gy86_rt));

    /* ---------------------------------------------------------------------- */
    /*  CubeMX가 I2C1을 다시 100kHz로 생성해도                                 */
    /*  driver init에서 한 번 더 안전한 표준모드 속도로 덮어쓴다.              */
    /* ---------------------------------------------------------------------- */
    GY86_I2C_ApplySafeClockIfNeeded();

    /* ---------------------------------------------------------------------- */
    /*  현재 실제 I2C 속도 기준으로                                             */
    /*  MPU polling 주기를 선택한다.                                           */
    /* ---------------------------------------------------------------------- */
    s_gy86_rt.mpu_period_ms = GY86_SelectMpuPeriodMs();
    s_gy86_rt.next_probe_ms = HAL_GetTick();

    /* ---------------------------------------------------------------------- */
    /*  공개 APP_STATE slice 초기화                                             */
    /* ---------------------------------------------------------------------- */
    GY86_ResetAppStateSlice(imu);

    /* ---------------------------------------------------------------------- */
    /*  보드 전원/센서 안정 시간 약간 확보                                      */
    /* ---------------------------------------------------------------------- */
    HAL_Delay(50u);

    /* ---------------------------------------------------------------------- */
    /*  가능한 경우 즉시 1회 probe                                              */
    /* ---------------------------------------------------------------------- */
    GY86_ProbeMissingBackends(HAL_GetTick(), imu);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: monotonic task                                                    */
/* -------------------------------------------------------------------------- */

void GY86_IMU_Task(uint32_t now_ms)
{
    app_gy86_state_t *imu;
    uint8_t updated;

    imu = (app_gy86_state_t *)&g_app_state.gy86;

    /* ---------------------------------------------------------------------- */
    /*  미탐지/미초기화 칩이 있으면 1초마다 다시 probe                          */
    /* ---------------------------------------------------------------------- */
#if GY86_IMU_ENABLE_MAGNETOMETER
    if (((s_gy86_rt.mpu_online == 0u) ||
         (s_gy86_rt.mag_online == 0u) ||
         (s_gy86_rt.baro_online == 0u)) &&
        (GY86_TimeDue(now_ms, s_gy86_rt.next_probe_ms) != 0u))
#else
    if (((s_gy86_rt.mpu_online == 0u) ||
         (s_gy86_rt.baro_online == 0u)) &&
        (GY86_TimeDue(now_ms, s_gy86_rt.next_probe_ms) != 0u))
#endif
    {
        GY86_ProbeMissingBackends(now_ms, imu);
    }

    /* ---------------------------------------------------------------------- */
    /*  accel/gyro polling                                                     */
    /* ---------------------------------------------------------------------- */
    if ((s_gy86_rt.mpu_online != 0u) &&
        (GY86_TimeDue(now_ms, s_gy86_rt.next_mpu_ms) != 0u))
    {
        updated = 0u;
        (void)s_accelgyro_backend.poll(now_ms, imu, &updated);
        s_gy86_rt.next_mpu_ms = GY86_NextPeriodicDue(s_gy86_rt.next_mpu_ms,
                                                     s_gy86_rt.mpu_period_ms,
                                                     now_ms);
    }

    /* ---------------------------------------------------------------------- */
    /*  magnetometer polling                                                   */
    /* ---------------------------------------------------------------------- */
    /* ---------------------------------------------------------------------- */
    /*  magnetometer polling                                                   */
    /*                                                                        */
    /*  이번 제품에서는 magnetometer를 완전히 비활성화했으므로                 */
    /*  이 블록은 compile-time switch가 1일 때만 살아 있다.                    */
    /* ---------------------------------------------------------------------- */
#if GY86_IMU_ENABLE_MAGNETOMETER
    if ((s_gy86_rt.mag_online != 0u) &&
        (GY86_TimeDue(now_ms, s_gy86_rt.next_mag_ms) != 0u))
    {
        updated = 0u;
        (void)s_mag_backend.poll(now_ms, imu, &updated);
        s_gy86_rt.next_mag_ms = GY86_NextPeriodicDue(s_gy86_rt.next_mag_ms,
                                                     GY86_IMU_MAG_PERIOD_MS,
                                                     now_ms);
    }
#endif

    /* ---------------------------------------------------------------------- */
    /*  barometer state machine                                                */
    /*                                                                        */
    /*  MS5611은 "1회 read = 1회 full sample" 구조가 아니라                     */
    /*  D1 변환 -> 대기 -> D1 read -> D2 변환 -> 대기 -> D2 read                */
    /*  흐름이 필요하므로 main loop마다 now_ms 하나로 계속 돌려 준다.           */
    /* ---------------------------------------------------------------------- */
    if (s_gy86_rt.baro_online != 0u)
    {
        updated = 0u;
        (void)s_baro_backend.poll(now_ms, imu, &updated);
    }
}
