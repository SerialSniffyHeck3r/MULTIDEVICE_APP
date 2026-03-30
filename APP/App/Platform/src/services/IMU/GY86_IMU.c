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
#if (USE_DOUBLE_BAROSENSOR == 2u)
extern I2C_HandleTypeDef GY86_IMU_I2C2_HANDLE;
#endif

/* -------------------------------------------------------------------------- */
/*  GY-86 내부 칩 주소                                                         */
/* -------------------------------------------------------------------------- */

/* HAL I2C는 7-bit address를 left shift 한 값을 기대한다. */
#define GY86_MPU6050_ADDR            (0x68u << 1)
#define GY86_HMC5883L_ADDR           (0x1Eu << 1)
/* MS5611 주소는 header의 compile-time switch를 그대로 사용한다. */

/* -------------------------------------------------------------------------- */
/*  Magnetometer compile-time switch                                           */
/*                                                                            */
/*  현재 정책                                                                   */
/*  - HMC5883L raw polling은 다시 활성화한다.                                  */
/*  - 이유                                                                       */
/*    1) SELFTEST 의 MAG FLOW 통과                                              */
/*    2) 보조 heading 진단용 raw 확보                                            */
/*  - 단, BIKE_DYNAMICS의 lean / grade / lateral G / accel-decel               */
/*    메인 추정식에는 magnetometer를 직접 피드백하지 않는다.                   */
/*                                                                            */
/*  값이 1이면                                                                 */
/*  - HMC5883L probe / init / poll 수행                                         */
/*  - APP_STATE debug에 mag backend / poll period 노출                          */
/* -------------------------------------------------------------------------- */
#ifndef GY86_IMU_ENABLE_MAGNETOMETER
#define GY86_IMU_ENABLE_MAGNETOMETER 1u
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
/*  dual MS5611 fusion tuning                                                 */
/*                                                                            */
/*  목적                                                                       */
/*  - 두 pressure sensor를 단순 평균이 아니라                                 */
/*    freshness / offset / disagreement 상태를 보며 합친다.                   */
/*  - sensor 하나가 한두 샘플 늦거나, static offset이 있거나,                  */
/*    순간적으로 튀어도 fused pressure가 크게 흔들리지 않게 한다.             */
/* -------------------------------------------------------------------------- */
#ifndef GY86_MS5611_FUSION_FRESH_TIMEOUT_MS
#define GY86_MS5611_FUSION_FRESH_TIMEOUT_MS 80u
#endif

#ifndef GY86_MS5611_FUSION_DISAGREE_ENTER_PA
#define GY86_MS5611_FUSION_DISAGREE_ENTER_PA 60
#endif

#ifndef GY86_MS5611_FUSION_DISAGREE_EXIT_PA
#define GY86_MS5611_FUSION_DISAGREE_EXIT_PA 35
#endif

#ifndef GY86_MS5611_FUSION_BIAS_TRACK_ALPHA
#define GY86_MS5611_FUSION_BIAS_TRACK_ALPHA 0.025f
#endif

#ifndef GY86_MS5611_FUSION_BIAS_TRACK_GATE_PA
#define GY86_MS5611_FUSION_BIAS_TRACK_GATE_PA 160.0f
#endif

/* -------------------------------------------------------------------------- */
/*  MS5611 published-sample plausibility gate                                 */
/*                                                                            */
/*  목적                                                                       */
/*  - HAL/I2C가 HAL_OK를 돌려도 ADC 바이트가 한 번 깨지면                     */
/*    compensated pressure 하나가 그대로 APP_STATE로 publish 된다.            */
/*  - Alt1은 latest code에서 pure baro/display path를 그대로 쓰므로,          */
/*    sample 하나의 발광도 메인 altitude spike로 직결될 수 있다.              */
/*                                                                            */
/*  정책                                                                       */
/*  - driver 내부 per-device runtime / fusion 계산은 그대로 둔다.             */
/*  - "상위로 publish 되는 fused sample" 직전에만                             */
/*    raw/range/step plausibility를 한 번 더 본다.                            */
/*  - reject 시 last-good published sample을 그대로 유지하여                   */
/*    APP_ALTITUDE_Task()가 새 sample_count를 보지 않게 만든다.               */
/*                                                                            */
/*  이 gate는 정상 비행 envelope보다 한참 넓다.                               */
/*  따라서 실제 climb/sink 응답은 거의 건드리지 않고,                         */
/*  순간적인 I2C/ADC glitch만 자르는 용도다.                                  */
/* -------------------------------------------------------------------------- */
#ifndef GY86_MS5611_PUBLISH_MIN_PRESSURE_PA
#define GY86_MS5611_PUBLISH_MIN_PRESSURE_PA 20000
#endif

#ifndef GY86_MS5611_PUBLISH_MAX_PRESSURE_PA
#define GY86_MS5611_PUBLISH_MAX_PRESSURE_PA 120000
#endif

#ifndef GY86_MS5611_PUBLISH_MIN_TEMP_CDEG
#define GY86_MS5611_PUBLISH_MIN_TEMP_CDEG (-4000)
#endif

#ifndef GY86_MS5611_PUBLISH_MAX_TEMP_CDEG
#define GY86_MS5611_PUBLISH_MAX_TEMP_CDEG 8500
#endif

#ifndef GY86_MS5611_PUBLISH_STEP_FLOOR_PA
#define GY86_MS5611_PUBLISH_STEP_FLOOR_PA 35u
#endif

#ifndef GY86_MS5611_PUBLISH_STEP_RATE_PA_PER_S
#define GY86_MS5611_PUBLISH_STEP_RATE_PA_PER_S 500u
#endif

#ifndef GY86_MS5611_PUBLISH_TEMP_STEP_FLOOR_CDEG
#define GY86_MS5611_PUBLISH_TEMP_STEP_FLOOR_CDEG 200u
#endif

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
/*  I2C bus descriptor                                                         */
/*                                                                            */
/*  이번 버전의 핵심은 "MS5611 source를 하나 더 추가하되, 상위 파이프라인은      */
/*  그대로 두는 것" 이다.                                                     */
/*                                                                            */
/*  그래서 MS5611 device state에는 "주소(addr)" 뿐 아니라                     */
/*  "어느 HAL I2C handle / 어느 핀쌍" 을 쓰는지까지 같이 묶어 둔다.             */
/*                                                                            */
/*  mode 0 : bus1 + addr_primary                                               */
/*  mode 1 : bus1 + addr_primary / bus1 + addr_secondary                       */
/*  mode 2 : bus1 + addr_primary / bus2 + addr_i2c2                            */
/*                                                                            */
/*  fuse logic은 세 모드 모두 동일하고, 단지 각 sensor가 접근하는 physical bus만   */
/*  달라진다.                                                                  */
/* -------------------------------------------------------------------------- */
typedef struct
{
    I2C_HandleTypeDef *handle;
    GPIO_TypeDef      *scl_port;
    uint16_t           scl_pin;
    GPIO_TypeDef      *sda_port;
    uint16_t           sda_pin;
    uint8_t            bus_id;
} gy86_i2c_bus_desc_t;

static const gy86_i2c_bus_desc_t s_gy86_i2c_bus1 =
{
    &GY86_IMU_I2C_HANDLE,
    GY86_IMU_I2C1_SCL_GPIO_PORT,
    GY86_IMU_I2C1_SCL_PIN,
    GY86_IMU_I2C1_SDA_GPIO_PORT,
    GY86_IMU_I2C1_SDA_PIN,
    1u
};

#if (USE_DOUBLE_BAROSENSOR == 2u)
static const gy86_i2c_bus_desc_t s_gy86_i2c_bus2 =
{
    &GY86_IMU_I2C2_HANDLE,
    GY86_IMU_I2C2_SCL_GPIO_PORT,
    GY86_IMU_I2C2_SCL_PIN,
    GY86_IMU_I2C2_SDA_GPIO_PORT,
    GY86_IMU_I2C2_SDA_PIN,
    2u
};
#endif

/* -------------------------------------------------------------------------- */
/*  드라이버 내부 런타임 상태                                                   */
/* -------------------------------------------------------------------------- */

typedef struct
{
    /* ---------------------------------------------------------------------- */
    /*  dual MS5611 per-device state                                           */
    /*                                                                        */
    /*  bus          : 어떤 I2C peripheral / 어떤 핀쌍을 쓰는가                */
    /*  addr         : 그 bus에서 접근할 실제 7-bit<<1 주소                    */
    /*  online       : 해당 센서 하나가 현재 살아 있는가                       */
    /*  error_streak : runtime 연속 실패 streak                                */
    /*  phase        : 0:D1 start, 1:D1 read+D2 start, 2:D2 read               */
    /* ---------------------------------------------------------------------- */
    const gy86_i2c_bus_desc_t *bus;
    uint8_t  addr;
    uint8_t  online;
    uint8_t  error_streak;
    uint8_t  phase;

    uint32_t deadline_ms;
    uint32_t d1_raw;
    uint32_t d2_raw;

    uint16_t prom_c[7];

    uint32_t timestamp_ms;
    uint32_t sample_count;

    int32_t  temp_cdeg;
    int32_t  pressure_hpa_x100;
    int32_t  pressure_pa;
    uint8_t  valid;
} gy86_ms5611_device_t;

typedef struct
{
    uint8_t  mpu_online;                /* MPU backend init 성공 여부             */
    uint8_t  mag_online;                /* MAG backend init 성공 여부             */
    uint8_t  baro_online;               /* 적어도 1개의 MS5611이 online 인가      */

    /* ---------------------------------------------------------------------- */
    /*  runtime read 연속 실패 streak                                          */
    /*                                                                        */
    /*  MPU/MAG는 기존처럼 backend 단위로 관리한다.                            */
    /*  MS5611은 per-device streak를 gy86_ms5611_device_t 내부에 둔다.         */
    /* ---------------------------------------------------------------------- */
    uint8_t  mpu_error_streak;
    uint8_t  mag_error_streak;
    uint8_t  reserved0;

    uint32_t next_probe_ms;             /* 미탐지/미초기화 칩 재시도 시각         */
    uint32_t next_mpu_ms;               /* 다음 MPU polling 시각                  */
    uint32_t next_mag_ms;               /* 다음 MAG polling 시각                  */

    uint16_t mpu_period_ms;             /* 실제 선택된 accel/gyro polling 주기    */
    uint16_t reserved1;

    /* ---------------------------------------------------------------------- */
    /*  bus recovery debug                                                     */
    /* ---------------------------------------------------------------------- */
    uint32_t i2c_recovery_count;
    uint32_t last_i2c_recovery_ms;

    /* ---------------------------------------------------------------------- */
    /*  MS5611 aggregate / dual-sensor runtime                                 */
    /*                                                                        */
    /*  fused_baro_sample_count     : fused publish 누적 수                    */
    /*  ms5611_pressure_bias_pa[]   : sensor0 기준 상대 pressure offset 추정    */
    /*  ms5611_aligned_pressure_pa[]: bias 제거 후 fusion에 사용한 pressure      */
    /*  ms5611_residual_pa[]        : aligned - primary residual               */
    /*  ms5611_weight_permille[]    : 이번 publish에 사용된 weight             */
    /*  ms5611_selected_mask        : fused sample에 실제 반영된 sensor bit     */
    /*  ms5611_rejected_mask        : stale/disagree로 제외된 sensor bit        */
    /*  ms5611_disagree_latched_mask: disagreement hysteresis latch 상태       */
    /*  ms5611_primary_index        : 현재 raw/prom 기준으로 채택된 primary     */
    /* ---------------------------------------------------------------------- */
    gy86_ms5611_device_t ms5611_dev[GY86_IMU_MS5611_DEVICE_COUNT];
    uint32_t fused_baro_sample_count;
    float    ms5611_pressure_bias_pa[GY86_IMU_MS5611_DEVICE_COUNT];
    int32_t  ms5611_aligned_pressure_pa[GY86_IMU_MS5611_DEVICE_COUNT];
    int32_t  ms5611_residual_pa[GY86_IMU_MS5611_DEVICE_COUNT];
    uint16_t ms5611_weight_permille[GY86_IMU_MS5611_DEVICE_COUNT];
    uint8_t  ms5611_selected_mask;
    uint8_t  ms5611_rejected_mask;
    uint8_t  ms5611_disagree_latched_mask;
    uint8_t  ms5611_primary_index;
    uint8_t  ms5611_fused_count;
    uint8_t  ms5611_fusion_flags;
    uint8_t  reserved2;
    int32_t  ms5611_raw_delta_pa;
    int32_t  ms5611_aligned_delta_pa;
} gy86_runtime_t;

static gy86_runtime_t s_gy86_rt;

/* -------------------------------------------------------------------------- */
/*  forward declarations used by backend code                                 */
/* -------------------------------------------------------------------------- */
static void GY86_UpdateInitializedFlag(app_gy86_state_t *imu);
static uint8_t GY86_Ms5611_DeviceIsFresh(uint32_t now_ms,
                                         const gy86_ms5611_device_t *dev);
static void GY86_Ms5611_ClearFusionDiagnostics(void);
static void GY86_Ms5611_UpdateAppStateDiagnostics(app_gy86_state_t *imu,
                                                  uint32_t now_ms);
void GY86_RecordRuntimeErrorAndMaybeOffline(uint8_t device_mask,
                                            uint8_t *online_flag,
                                            uint8_t *error_streak,
                                            uint32_t now_ms,
                                            app_gy86_state_t *imu);


static void GY86_Ms5611_DeviceRuntimeInit(gy86_ms5611_device_t *dev,
                                          const gy86_i2c_bus_desc_t *bus,
                                          uint8_t addr)
{
    if (dev == 0)
    {
        return;
    }

    memset(dev, 0, sizeof(*dev));
    dev->bus = bus;
    dev->addr = addr;
}

static void GY86_Ms5611_RuntimeInit(void)
{
    GY86_Ms5611_ClearFusionDiagnostics();

    /* ------------------------------------------------------------------ */
    /*  device[0]은 항상 "기존 onboard GY-86 MS5611" 역할을 맡긴다.      */
    /* ------------------------------------------------------------------ */
    GY86_Ms5611_DeviceRuntimeInit(&s_gy86_rt.ms5611_dev[0],
                                  &s_gy86_i2c_bus1,
                                  GY86_MS5611_ADDR_PRIMARY);

#if (USE_DOUBLE_BAROSENSOR == 1u)
    /* ------------------------------------------------------------------ */
    /*  mode 1                                                             */
    /*  - I2C1 bus 안에서 0x77 + 0x76 두 주소를 사용한다.                  */
    /* ------------------------------------------------------------------ */
    GY86_Ms5611_DeviceRuntimeInit(&s_gy86_rt.ms5611_dev[1],
                                  &s_gy86_i2c_bus1,
                                  GY86_MS5611_ADDR_SECONDARY);
#elif (USE_DOUBLE_BAROSENSOR == 2u)
    /* ------------------------------------------------------------------ */
    /*  mode 2                                                             */
    /*  - I2C2에 외부 MS5611 하나를 더 단다.                               */
    /*  - bus가 다르므로 주소는 onboard와 같아도 된다.                     */
    /* ------------------------------------------------------------------ */
    GY86_Ms5611_DeviceRuntimeInit(&s_gy86_rt.ms5611_dev[1],
                                  &s_gy86_i2c_bus2,
                                  GY86_MS5611_ADDR_I2C2);
#endif
}

static int32_t GY86_RoundFloatToS32(float value)
{
    if (value >= 0.0f)
    {
        return (int32_t)(value + 0.5f);
    }

    return (int32_t)(value - 0.5f);
}

static uint32_t GY86_Abs32(int32_t value)
{
    if (value < 0)
    {
        return (uint32_t)(-value);
    }

    return (uint32_t)value;
}

static uint8_t GY86_Ms5611_DeviceIsFresh(uint32_t now_ms,
                                         const gy86_ms5611_device_t *dev)
{
    uint32_t age_ms;

    if (dev == 0)
    {
        return 0u;
    }

    if ((dev->online == 0u) || (dev->valid == 0u) || (dev->timestamp_ms == 0u))
    {
        return 0u;
    }

    age_ms = now_ms - dev->timestamp_ms;

    return (age_ms <= GY86_MS5611_FUSION_FRESH_TIMEOUT_MS) ? 1u : 0u;
}

static void GY86_Ms5611_ClearFusionDiagnostics(void)
{
    uint32_t idx;

    s_gy86_rt.ms5611_selected_mask = 0u;
    s_gy86_rt.ms5611_rejected_mask = 0u;
    s_gy86_rt.ms5611_primary_index = 0xFFu;
    s_gy86_rt.ms5611_fused_count = 0u;
    s_gy86_rt.ms5611_fusion_flags = 0u;
    s_gy86_rt.ms5611_raw_delta_pa = 0;
    s_gy86_rt.ms5611_aligned_delta_pa = 0;

    for (idx = 0u; idx < GY86_IMU_MS5611_DEVICE_COUNT; idx++)
    {
        s_gy86_rt.ms5611_aligned_pressure_pa[idx] = 0;
        s_gy86_rt.ms5611_residual_pa[idx] = 0;
        s_gy86_rt.ms5611_weight_permille[idx] = 0u;
    }
}

static void GY86_Ms5611_UpdateAppStateDiagnostics(app_gy86_state_t *imu,
                                                  uint32_t now_ms)
{
    uint32_t idx;

    if (imu == 0)
    {
        return;
    }

    imu->debug.baro_device_slots = APP_GY86_BARO_SENSOR_SLOTS;
    imu->debug.baro_fused_sensor_count = s_gy86_rt.ms5611_fused_count;
    imu->debug.baro_primary_sensor_index = s_gy86_rt.ms5611_primary_index;
    imu->debug.baro_fusion_flags = s_gy86_rt.ms5611_fusion_flags;
    imu->debug.baro_sensor_delta_pa_raw = s_gy86_rt.ms5611_raw_delta_pa;
    imu->debug.baro_sensor_delta_pa_aligned = s_gy86_rt.ms5611_aligned_delta_pa;

    for (idx = 0u; idx < APP_GY86_BARO_SENSOR_SLOTS; idx++)
    {
        app_gy86_baro_sensor_state_t *dst;

        dst = &imu->baro_sensor[idx];
        memset(dst, 0, sizeof(*dst));

        if (idx >= GY86_IMU_MS5611_DEVICE_COUNT)
        {
            continue;
        }

        if ((s_gy86_rt.ms5611_dev[idx].bus == 0) ||
            (s_gy86_rt.ms5611_dev[idx].addr == 0u))
        {
            continue;
        }

        dst->configured = 1u;
        dst->online = s_gy86_rt.ms5611_dev[idx].online;
        dst->valid = s_gy86_rt.ms5611_dev[idx].valid;
        dst->fresh = GY86_Ms5611_DeviceIsFresh(now_ms, &s_gy86_rt.ms5611_dev[idx]);
        dst->selected = ((s_gy86_rt.ms5611_selected_mask & (1u << idx)) != 0u) ? 1u : 0u;
        dst->rejected = ((s_gy86_rt.ms5611_rejected_mask & (1u << idx)) != 0u) ? 1u : 0u;
        dst->bus_id = (s_gy86_rt.ms5611_dev[idx].bus != 0) ? s_gy86_rt.ms5611_dev[idx].bus->bus_id : 0u;
        dst->addr_7bit = (uint8_t)(s_gy86_rt.ms5611_dev[idx].addr >> 1);
        dst->error_streak = s_gy86_rt.ms5611_dev[idx].error_streak;
        dst->weight_permille = s_gy86_rt.ms5611_weight_permille[idx];
        dst->timestamp_ms = s_gy86_rt.ms5611_dev[idx].timestamp_ms;
        dst->sample_count = s_gy86_rt.ms5611_dev[idx].sample_count;
        dst->temp_cdeg = s_gy86_rt.ms5611_dev[idx].temp_cdeg;
        dst->pressure_hpa_x100 = s_gy86_rt.ms5611_dev[idx].pressure_hpa_x100;
        dst->pressure_pa = s_gy86_rt.ms5611_dev[idx].pressure_pa;
        dst->aligned_pressure_pa = s_gy86_rt.ms5611_aligned_pressure_pa[idx];
        dst->bias_pa = GY86_RoundFloatToS32(s_gy86_rt.ms5611_pressure_bias_pa[idx]);
        dst->residual_pa = s_gy86_rt.ms5611_residual_pa[idx];
    }
}

static uint8_t GY86_Ms5611_AnyOnline(void)
{
    uint32_t idx;

    for (idx = 0u; idx < GY86_IMU_MS5611_DEVICE_COUNT; idx++)
    {
        if (s_gy86_rt.ms5611_dev[idx].online != 0u)
        {
            return 1u;
        }
    }

    return 0u;
}

static uint8_t GY86_Ms5611_NeedsProbe(void)
{
    uint32_t idx;

    for (idx = 0u; idx < GY86_IMU_MS5611_DEVICE_COUNT; idx++)
    {
        if (s_gy86_rt.ms5611_dev[idx].online == 0u)
        {
            return 1u;
        }
    }

    return 0u;
}

static void GY86_Ms5611_CopyPromToAppState(app_gy86_state_t *imu)
{
    uint32_t idx;
    uint32_t src_index;

    if (imu == 0)
    {
        return;
    }

    memset(imu->baro.prom_c, 0, sizeof(imu->baro.prom_c));

    src_index = 0u;

    if ((s_gy86_rt.ms5611_primary_index < GY86_IMU_MS5611_DEVICE_COUNT) &&
        (s_gy86_rt.ms5611_dev[s_gy86_rt.ms5611_primary_index].online != 0u))
    {
        src_index = s_gy86_rt.ms5611_primary_index;
    }
#if USE_DOUBLE_BAROSENSOR
    else if ((s_gy86_rt.ms5611_dev[0].online == 0u) &&
             (s_gy86_rt.ms5611_dev[1].online != 0u))
    {
        src_index = 1u;
    }
#endif

    for (idx = 0u; idx < 7u; idx++)
    {
        imu->baro.prom_c[idx] = s_gy86_rt.ms5611_dev[src_index].prom_c[idx];
    }
}


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

static void GY86_I2C_EnableGpioClock(GPIO_TypeDef *port)
{
    if (port == GPIOA)
    {
        __HAL_RCC_GPIOA_CLK_ENABLE();
    }
    else if (port == GPIOB)
    {
        __HAL_RCC_GPIOB_CLK_ENABLE();
    }
#ifdef GPIOC
    else if (port == GPIOC)
    {
        __HAL_RCC_GPIOC_CLK_ENABLE();
    }
#endif
#ifdef GPIOD
    else if (port == GPIOD)
    {
        __HAL_RCC_GPIOD_CLK_ENABLE();
    }
#endif
#ifdef GPIOE
    else if (port == GPIOE)
    {
        __HAL_RCC_GPIOE_CLK_ENABLE();
    }
#endif
#ifdef GPIOF
    else if (port == GPIOF)
    {
        __HAL_RCC_GPIOF_CLK_ENABLE();
    }
#endif
#ifdef GPIOG
    else if (port == GPIOG)
    {
        __HAL_RCC_GPIOG_CLK_ENABLE();
    }
#endif
#ifdef GPIOH
    else if (port == GPIOH)
    {
        __HAL_RCC_GPIOH_CLK_ENABLE();
    }
#endif
}

static void GY86_I2C_EnablePeripheralClock(const gy86_i2c_bus_desc_t *bus)
{
    if ((bus == 0) || (bus->handle == 0) || (bus->handle->Instance == 0))
    {
        return;
    }

    if (bus->handle->Instance == I2C1)
    {
        __HAL_RCC_I2C1_CLK_ENABLE();
    }
#ifdef I2C2
    else if (bus->handle->Instance == I2C2)
    {
        __HAL_RCC_I2C2_CLK_ENABLE();
    }
#endif
#ifdef I2C3
    else if (bus->handle->Instance == I2C3)
    {
        __HAL_RCC_I2C3_CLK_ENABLE();
    }
#endif
}

static void GY86_I2C_ForceBusPinsToGpioOdPullup(const gy86_i2c_bus_desc_t *bus)
{
    GPIO_InitTypeDef gpio_init;

    if ((bus == 0) ||
        (bus->scl_port == 0) ||
        (bus->sda_port == 0) ||
        (bus->scl_pin == 0u) ||
        (bus->sda_pin == 0u))
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  bus recovery 동안은 AF 대신 GPIO open-drain 으로 잠깐 되돌린다.        */
    /*  mode 2에서는 I2C2(PB10/PB11)도 이 경로를 같은 로직으로 처리한다.      */
    /* ---------------------------------------------------------------------- */
    GY86_I2C_EnableGpioClock(bus->scl_port);
    if (bus->sda_port != bus->scl_port)
    {
        GY86_I2C_EnableGpioClock(bus->sda_port);
    }

    memset(&gpio_init, 0, sizeof(gpio_init));
    gpio_init.Mode  = GPIO_MODE_OUTPUT_OD;
    gpio_init.Pull  = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;

    if (bus->scl_port == bus->sda_port)
    {
        gpio_init.Pin = bus->scl_pin | bus->sda_pin;
        HAL_GPIO_Init(bus->scl_port, &gpio_init);
    }
    else
    {
        gpio_init.Pin = bus->scl_pin;
        HAL_GPIO_Init(bus->scl_port, &gpio_init);
        gpio_init.Pin = bus->sda_pin;
        HAL_GPIO_Init(bus->sda_port, &gpio_init);
    }

    /* bus idle-high 상태 */
    HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_SET);
    HAL_GPIO_WritePin(bus->sda_port, bus->sda_pin, GPIO_PIN_SET);
    GY86_I2C_ShortDelay();
}

static void GY86_I2C_BusUnwedge(const gy86_i2c_bus_desc_t *bus)
{
    uint8_t pulse_index;

    if (bus == 0)
    {
        return;
    }

    GY86_I2C_ForceBusPinsToGpioOdPullup(bus);

    /* ---------------------------------------------------------------------- */
    /*  slave가 SDA를 low로 붙잡고 있으면 SCL을 몇 번 흔들어서                  */
    /*  내부 bit state machine이 남은 비트를 밀어내게 유도한다.                */
    /* ---------------------------------------------------------------------- */
    for (pulse_index = 0u; pulse_index < 9u; pulse_index++)
    {
        if (HAL_GPIO_ReadPin(bus->sda_port, bus->sda_pin) == GPIO_PIN_SET)
        {
            break;
        }

        HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_RESET);
        GY86_I2C_ShortDelay();

        HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_SET);
        GY86_I2C_ShortDelay();
    }

    /* ---------------------------------------------------------------------- */
    /*  STOP 모양을 한 번 만들어서 bus를 idle 상태로 복귀시킨다.               */
    /* ---------------------------------------------------------------------- */
    HAL_GPIO_WritePin(bus->sda_port, bus->sda_pin, GPIO_PIN_RESET);
    GY86_I2C_ShortDelay();

    HAL_GPIO_WritePin(bus->scl_port, bus->scl_pin, GPIO_PIN_SET);
    GY86_I2C_ShortDelay();

    HAL_GPIO_WritePin(bus->sda_port, bus->sda_pin, GPIO_PIN_SET);
    GY86_I2C_ShortDelay();
}

static void GY86_I2C_PeripheralSoftReset(const gy86_i2c_bus_desc_t *bus)
{
    if ((bus == 0) || (bus->handle == 0) || (bus->handle->Instance == 0))
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  STM32F4 I2C errata workaround 쪽에서 자주 쓰는 SWRST 경로              */
    /* ---------------------------------------------------------------------- */
    GY86_I2C_EnablePeripheralClock(bus);

    SET_BIT(bus->handle->Instance->CR1, I2C_CR1_SWRST);
    GY86_I2C_ShortDelay();
    CLEAR_BIT(bus->handle->Instance->CR1, I2C_CR1_SWRST);
    GY86_I2C_ShortDelay();
}

static void GY86_I2C_ApplySafeClockIfNeededOne(const gy86_i2c_bus_desc_t *bus)
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
    if ((bus == 0) || (bus->handle == 0))
    {
        return;
    }

    if ((bus->handle->Init.ClockSpeed >= 88000u) &&
        (bus->handle->Init.ClockSpeed <= 100000u))
    {
        (void)HAL_I2C_DeInit(bus->handle);
        bus->handle->Init.ClockSpeed = GY86_IMU_SAFE_STANDARD_MODE_HZ;
        (void)HAL_I2C_Init(bus->handle);
    }
#else
    (void)bus;
#endif
}

static void GY86_I2C_ApplySafeClockIfNeeded(void)
{
    /* ---------------------------------------------------------------------- */
    /*  MPU/HMC/MS5611 onboard가 붙는 primary bus는 항상 검사한다.             */
    /*  mode 2에서는 외부 baro가 붙는 secondary bus도 같은 규칙을 적용한다.   */
    /* ---------------------------------------------------------------------- */
    GY86_I2C_ApplySafeClockIfNeededOne(&s_gy86_i2c_bus1);

#if (USE_DOUBLE_BAROSENSOR == 2u)
    GY86_I2C_ApplySafeClockIfNeededOne(&s_gy86_i2c_bus2);
#endif
}

static void GY86_I2C_RecoverBus(const gy86_i2c_bus_desc_t *bus)
{
    if ((bus == 0) || (bus->handle == 0))
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  recovery debug 카운터                                                  */
    /* ---------------------------------------------------------------------- */
    s_gy86_rt.i2c_recovery_count++;
    s_gy86_rt.last_i2c_recovery_ms = HAL_GetTick();

    /* ---------------------------------------------------------------------- */
    /*  1) peripheral SWRST                                                    */
    /* ---------------------------------------------------------------------- */
    GY86_I2C_PeripheralSoftReset(bus);

    /* ---------------------------------------------------------------------- */
    /*  2) HAL handle / peripheral deinit                                      */
    /* ---------------------------------------------------------------------- */
    (void)HAL_I2C_DeInit(bus->handle);

    /* ---------------------------------------------------------------------- */
    /*  3) GPIO mode에서 bus-unwedge                                           */
    /* ---------------------------------------------------------------------- */
    GY86_I2C_BusUnwedge(bus);

    /* ---------------------------------------------------------------------- */
    /*  4) HAL init로 peripheral 복구                                          */
    /* ---------------------------------------------------------------------- */
    (void)HAL_I2C_Init(bus->handle);
}

static HAL_StatusTypeDef GY86_I2C_WriteU8_Bus(const gy86_i2c_bus_desc_t *bus,
                                              uint8_t dev_addr,
                                              uint8_t reg_addr,
                                              uint8_t value)
{
    HAL_StatusTypeDef st;

    if ((bus == 0) || (bus->handle == 0))
    {
        return HAL_ERROR;
    }

    st = HAL_I2C_Mem_Write(bus->handle,
                           dev_addr,
                           reg_addr,
                           I2C_MEMADD_SIZE_8BIT,
                           &value,
                           1u,
                           GY86_IMU_I2C_TIMEOUT_MS);

    if (st != HAL_OK)
    {
        GY86_I2C_RecoverBus(bus);

        st = HAL_I2C_Mem_Write(bus->handle,
                               dev_addr,
                               reg_addr,
                               I2C_MEMADD_SIZE_8BIT,
                               &value,
                               1u,
                               GY86_IMU_I2C_TIMEOUT_MS);
    }

    return st;
}

static HAL_StatusTypeDef GY86_I2C_Read_Bus(const gy86_i2c_bus_desc_t *bus,
                                           uint8_t dev_addr,
                                           uint8_t reg_addr,
                                           uint8_t *buffer,
                                           uint16_t length)
{
    HAL_StatusTypeDef st;

    if ((bus == 0) || (bus->handle == 0))
    {
        return HAL_ERROR;
    }

    st = HAL_I2C_Mem_Read(bus->handle,
                          dev_addr,
                          reg_addr,
                          I2C_MEMADD_SIZE_8BIT,
                          buffer,
                          length,
                          GY86_IMU_I2C_TIMEOUT_MS);

    if (st != HAL_OK)
    {
        GY86_I2C_RecoverBus(bus);

        st = HAL_I2C_Mem_Read(bus->handle,
                              dev_addr,
                              reg_addr,
                              I2C_MEMADD_SIZE_8BIT,
                              buffer,
                              length,
                              GY86_IMU_I2C_TIMEOUT_MS);
    }

    return st;
}

static HAL_StatusTypeDef GY86_I2C_CommandOnly_Bus(const gy86_i2c_bus_desc_t *bus,
                                                  uint8_t dev_addr,
                                                  uint8_t command)
{
    HAL_StatusTypeDef st;

    if ((bus == 0) || (bus->handle == 0))
    {
        return HAL_ERROR;
    }

    st = HAL_I2C_Master_Transmit(bus->handle,
                                 dev_addr,
                                 &command,
                                 1u,
                                 GY86_IMU_I2C_TIMEOUT_MS);

    if (st != HAL_OK)
    {
        GY86_I2C_RecoverBus(bus);

        st = HAL_I2C_Master_Transmit(bus->handle,
                                     dev_addr,
                                     &command,
                                     1u,
                                     GY86_IMU_I2C_TIMEOUT_MS);
    }

    return st;
}

static HAL_StatusTypeDef GY86_I2C_ReadDirect_Bus(const gy86_i2c_bus_desc_t *bus,
                                                 uint8_t dev_addr,
                                                 uint8_t *buffer,
                                                 uint16_t length)
{
    HAL_StatusTypeDef st;

    if ((bus == 0) || (bus->handle == 0))
    {
        return HAL_ERROR;
    }

    st = HAL_I2C_Master_Receive(bus->handle,
                                dev_addr,
                                buffer,
                                length,
                                GY86_IMU_I2C_TIMEOUT_MS);

    if (st != HAL_OK)
    {
        GY86_I2C_RecoverBus(bus);

        st = HAL_I2C_Master_Receive(bus->handle,
                                    dev_addr,
                                    buffer,
                                    length,
                                    GY86_IMU_I2C_TIMEOUT_MS);
    }

    return st;
}

/* ---------------------------------------------------------------------- */
/*  기존 MPU/HMC 경로는 여전히 primary bus(I2C1)를 쓴다.                    */
/*  상위 코드는 바뀌지 않도록 thin wrapper를 남겨 둔다.                     */
/* ---------------------------------------------------------------------- */
static HAL_StatusTypeDef GY86_I2C_WriteU8(uint8_t dev_addr, uint8_t reg_addr, uint8_t value)
{
    return GY86_I2C_WriteU8_Bus(&s_gy86_i2c_bus1, dev_addr, reg_addr, value);
}

static HAL_StatusTypeDef GY86_I2C_Read(uint8_t dev_addr,
                                       uint8_t reg_addr,
                                       uint8_t *buffer,
                                       uint16_t length)
{
    return GY86_I2C_Read_Bus(&s_gy86_i2c_bus1, dev_addr, reg_addr, buffer, length);
}

static HAL_StatusTypeDef GY86_I2C_CommandOnly(uint8_t dev_addr, uint8_t command)
{
    return GY86_I2C_CommandOnly_Bus(&s_gy86_i2c_bus1, dev_addr, command);
}

static HAL_StatusTypeDef GY86_I2C_ReadDirect(uint8_t dev_addr,
                                             uint8_t *buffer,
                                             uint16_t length)
{
    return GY86_I2C_ReadDirect_Bus(&s_gy86_i2c_bus1, dev_addr, buffer, length);
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

    i2c_hz = s_gy86_i2c_bus1.handle->Init.ClockSpeed;

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
/*                                                                            */
/*  설계 포인트                                                               */
/*  - USE_DOUBLE_BAROSENSOR=0 : 기존과 동일하게 단일 MS5611만 사용            */
/*  - USE_DOUBLE_BAROSENSOR=1 : 같은 I2C1 bus 에 0x77 + 0x76 두 개를 둔다.   */
/*  - USE_DOUBLE_BAROSENSOR=2 : I2C1 + I2C2 에 각각 하나씩 둔다.              */
/*  - 세 모드 모두 driver 내부에서 compensated pressure/temperature 를 평균   */
/*    fuse 해서 APP_STATE에는 기존과 동일한 단일 baro slice만 publish 한다.   */
/*                                                                            */
/*  즉 상위 APP_ALTITUDE / vario_app 은                                       */
/*  "barometer가 1개인지 2개인지" 를 몰라도 된다.                             */
/* -------------------------------------------------------------------------- */

static HAL_StatusTypeDef GY86_Ms5611_ReadAdcFromAddress(const gy86_i2c_bus_desc_t *bus,
                                                        uint8_t addr,
                                                        uint32_t *raw_adc)
{
    HAL_StatusTypeDef st;
    uint8_t read_command;
    uint8_t buffer[3];

    if ((bus == 0) || (raw_adc == 0))
    {
        return HAL_ERROR;
    }

    read_command = MS5611_CMD_ADC_READ;

    st = GY86_I2C_CommandOnly_Bus(bus, addr, read_command);
    if (st != HAL_OK)
    {
        return st;
    }

    st = GY86_I2C_ReadDirect_Bus(bus, addr, buffer, sizeof(buffer));
    if (st != HAL_OK)
    {
        return st;
    }

    *raw_adc = ((uint32_t)buffer[0] << 16) |
               ((uint32_t)buffer[1] << 8)  |
               ((uint32_t)buffer[2]);

    return HAL_OK;
}

static HAL_StatusTypeDef GY86_Ms5611_ReadPromFromAddress(const gy86_i2c_bus_desc_t *bus,
                                                         uint8_t addr,
                                                         uint16_t prom_c[7])
{
    HAL_StatusTypeDef st;
    uint8_t command;
    uint8_t buffer[2];
    uint8_t prom_index;

    if ((bus == 0) || (prom_c == 0))
    {
        return HAL_ERROR;
    }

    memset(prom_c, 0, sizeof(uint16_t) * 7u);

    for (prom_index = 1u; prom_index <= 6u; prom_index++)
    {
        command = (uint8_t)(MS5611_CMD_PROM_READ_BASE + (prom_index * 2u));

        st = GY86_I2C_CommandOnly_Bus(bus, addr, command);
        if (st != HAL_OK)
        {
            return st;
        }

        st = GY86_I2C_ReadDirect_Bus(bus, addr, buffer, sizeof(buffer));
        if (st != HAL_OK)
        {
            return st;
        }

        prom_c[prom_index] = (uint16_t)((buffer[0] << 8) | buffer[1]);
    }

    if ((prom_c[1] == 0u) &&
        (prom_c[2] == 0u) &&
        (prom_c[3] == 0u) &&
        (prom_c[4] == 0u) &&
        (prom_c[5] == 0u) &&
        (prom_c[6] == 0u))
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

static HAL_StatusTypeDef GY86_Ms5611_InitOne(app_gy86_state_t *imu,
                                             gy86_ms5611_device_t *dev)
{
    HAL_StatusTypeDef st;
    uint8_t command;

    if ((imu == 0) || (dev == 0))
    {
        return HAL_ERROR;
    }

    if ((dev->bus == 0) || (dev->addr == 0u))
    {
        return HAL_ERROR;
    }

    command = MS5611_CMD_RESET;
    st = GY86_I2C_CommandOnly_Bus(dev->bus, dev->addr, command);
    imu->debug.last_hal_status_baro = (uint8_t)st;
    if (st != HAL_OK)
    {
        return st;
    }

    HAL_Delay(3u);

    st = GY86_Ms5611_ReadPromFromAddress(dev->bus, dev->addr, dev->prom_c);
    imu->debug.last_hal_status_baro = (uint8_t)st;
    if (st != HAL_OK)
    {
        return st;
    }

    dev->online = 1u;
    dev->error_streak = 0u;
    dev->phase = 0u;
    dev->deadline_ms = 0u;
    dev->d1_raw = 0u;
    dev->d2_raw = 0u;
    dev->timestamp_ms = 0u;
    dev->sample_count = 0u;
    dev->temp_cdeg = 0;
    dev->pressure_hpa_x100 = 0;
    dev->pressure_pa = 0;
    dev->valid = 0u;

    {
        uint32_t device_index;

        device_index = (uint32_t)(dev - &s_gy86_rt.ms5611_dev[0]);
        if (device_index < GY86_IMU_MS5611_DEVICE_COUNT)
        {
            s_gy86_rt.ms5611_pressure_bias_pa[device_index] = 0.0f;
            s_gy86_rt.ms5611_aligned_pressure_pa[device_index] = 0;
            s_gy86_rt.ms5611_residual_pa[device_index] = 0;
            s_gy86_rt.ms5611_weight_permille[device_index] = 0u;
            s_gy86_rt.ms5611_disagree_latched_mask &= (uint8_t)~(1u << device_index);
        }
    }

    return HAL_OK;
}

static void GY86_Ms5611_RefreshBaroOnlineFlag(void)
{
    s_gy86_rt.baro_online = GY86_Ms5611_AnyOnline();
}

static HAL_StatusTypeDef GY86_Ms5611_Init(app_gy86_state_t *imu)
{
    HAL_StatusTypeDef st;
    HAL_StatusTypeDef final_status;
    uint32_t idx;
    uint8_t any_online;

    if (imu == 0)
    {
        return HAL_ERROR;
    }

    final_status = HAL_ERROR;
    any_online = 0u;

    for (idx = 0u; idx < GY86_IMU_MS5611_DEVICE_COUNT; idx++)
    {
        gy86_ms5611_device_t *dev;

        dev = &s_gy86_rt.ms5611_dev[idx];

        if (dev->online != 0u)
        {
            any_online = 1u;
            final_status = HAL_OK;
            continue;
        }

        st = GY86_Ms5611_InitOne(imu, dev);
        if (st == HAL_OK)
        {
            any_online = 1u;
            final_status = HAL_OK;
        }
        else
        {
            dev->online = 0u;
            dev->valid = 0u;
        }
    }

    GY86_Ms5611_RefreshBaroOnlineFlag();

    if (any_online == 0u)
    {
        return HAL_ERROR;
    }

    imu->debug.detected_mask      |= APP_GY86_DEVICE_BARO;
    imu->debug.init_ok_mask       |= APP_GY86_DEVICE_BARO;
    imu->debug.ms5611_state        = 0u;
    imu->debug.baro_backend_id     = APP_IMU_BACKEND_MS5611;
    imu->debug.last_hal_status_baro = (uint8_t)HAL_OK;
    GY86_Ms5611_CopyPromToAppState(imu);
    GY86_Ms5611_UpdateAppStateDiagnostics(imu, HAL_GetTick());

    return (final_status == HAL_OK) ? HAL_OK : HAL_ERROR;
}

static void GY86_Ms5611_StoreCompensatedSample(gy86_ms5611_device_t *dev,
                                               uint32_t now_ms)
{
    int32_t dT;
    int32_t temp_cdeg;
    int64_t off;
    int64_t sens;
    int64_t t2;
    int64_t off2;
    int64_t sens2;
    int32_t temp_low;
    int32_t temp_very_low;

    if (dev == 0)
    {
        return;
    }

    dT = (int32_t)dev->d2_raw - (int32_t)((uint32_t)dev->prom_c[5] * 256u);

    temp_cdeg = 2000 +
                (int32_t)(((int64_t)dT * (int64_t)dev->prom_c[6]) / 8388608LL);

    off = ((int64_t)dev->prom_c[2] * 65536LL) +
          (((int64_t)dev->prom_c[4] * (int64_t)dT) / 128LL);

    sens = ((int64_t)dev->prom_c[1] * 32768LL) +
           (((int64_t)dev->prom_c[3] * (int64_t)dT) / 256LL);

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

    dev->temp_cdeg = temp_cdeg;
    dev->pressure_hpa_x100 = (int32_t)(((((int64_t)dev->d1_raw * sens) /
                                          2097152LL) - off) / 32768LL);
    /* pressure_hpa_x100 은 hPa*100 이므로 수치적으로 이미 Pa 와 같다. */
    dev->pressure_pa = dev->pressure_hpa_x100;
    dev->timestamp_ms = now_ms;
    dev->sample_count++;
    dev->valid = 1u;
}

static uint8_t GY86_Ms5611_PublishedSampleAccepted(const app_gy86_state_t *imu,
                                                   const gy86_ms5611_device_t *primary_dev,
                                                   uint32_t candidate_timestamp_ms,
                                                   int32_t candidate_pressure_pa,
                                                   int32_t candidate_temp_cdeg)
{
    uint32_t dt_ms;
    uint32_t pressure_limit_pa;

    if ((imu == 0) || (primary_dev == 0))
    {
        return 0u;
    }

    /* ------------------------------------------------------------------ */
    /*  raw ADC가 all-zero / all-one 같은 버스 쓰레기면 바로 버린다.       */
    /* ------------------------------------------------------------------ */
    if ((primary_dev->d1_raw == 0u) ||
        (primary_dev->d2_raw == 0u) ||
        (primary_dev->d1_raw >= 0xFFFFFFu) ||
        (primary_dev->d2_raw >= 0xFFFFFFu))
    {
        return 0u;
    }

    /* ------------------------------------------------------------------ */
    /*  보정된 pressure / temperature 절대 범위 검사                        */
    /* ------------------------------------------------------------------ */
    if ((candidate_pressure_pa < GY86_MS5611_PUBLISH_MIN_PRESSURE_PA) ||
        (candidate_pressure_pa > GY86_MS5611_PUBLISH_MAX_PRESSURE_PA) ||
        (candidate_temp_cdeg < GY86_MS5611_PUBLISH_MIN_TEMP_CDEG) ||
        (candidate_temp_cdeg > GY86_MS5611_PUBLISH_MAX_TEMP_CDEG))
    {
        return 0u;
    }

    /* ------------------------------------------------------------------ */
    /*  첫 publish는 비교 기준이 없으므로 절대 범위만 통과하면 허용          */
    /* ------------------------------------------------------------------ */
    if ((imu->baro.sample_count == 0u) ||
        (imu->baro.timestamp_ms == 0u) ||
        (imu->baro.pressure_pa <= 0))
    {
        return 1u;
    }

    dt_ms = candidate_timestamp_ms - imu->baro.timestamp_ms;
    if ((candidate_timestamp_ms == 0u) || (candidate_timestamp_ms <= imu->baro.timestamp_ms))
    {
        dt_ms = GY86_IMU_BARO_PERIOD_MS;
    }
    else if (dt_ms > GY86_MS5611_FUSION_FRESH_TIMEOUT_MS)
    {
        dt_ms = GY86_MS5611_FUSION_FRESH_TIMEOUT_MS;
    }

    /* ------------------------------------------------------------------ */
    /*  pressure step gate                                                 */
    /*                                                                    */
    /*  35Pa floor + 500Pa/s slope                                         */
    /*  - 20ms sample에서는 약 45Pa (~3.8m 상당)                            */
    /*  - 80ms stale/fallback edge에서도 약 75Pa (~6.3m 상당)               */
    /*                                                                    */
    /*  실제 비행 envelope보다 넓게 잡아, 고도 spike만 자르는 용도다.       */
    /* ------------------------------------------------------------------ */
    pressure_limit_pa = GY86_MS5611_PUBLISH_STEP_FLOOR_PA +
                        (uint32_t)(((uint64_t)GY86_MS5611_PUBLISH_STEP_RATE_PA_PER_S *
                                    (uint64_t)dt_ms + 999u) / 1000u);

    if (GY86_Abs32(candidate_pressure_pa - imu->baro.pressure_pa) > pressure_limit_pa)
    {
        return 0u;
    }

    /* ------------------------------------------------------------------ */
    /*  D2 깨짐으로 compensation이 발광하는 경우를 위해                     */
    /*  temperature jump도 한 번 막는다.                                   */
    /* ------------------------------------------------------------------ */
    if (GY86_Abs32(candidate_temp_cdeg - imu->baro.temp_cdeg) >
        (uint32_t)GY86_MS5611_PUBLISH_TEMP_STEP_FLOOR_CDEG)
    {
        return 0u;
    }

    return 1u;
}

static void GY86_Ms5611_PublishFusedSample(app_gy86_state_t *imu,
                                           uint32_t now_ms,
                                           uint8_t *updated)
{
    uint32_t idx;
    uint32_t fresh_age_ms[GY86_IMU_MS5611_DEVICE_COUNT];
    uint32_t freshness_score[GY86_IMU_MS5611_DEVICE_COUNT];
    uint8_t  fresh_mask;
    uint8_t  accepted_mask;
    uint8_t  primary_index;
    gy86_ms5611_device_t *primary_dev;
    int32_t fused_pressure_pa;
    int32_t fused_temp_cdeg;
    uint32_t fused_weight_sum;
    uint32_t latest_timestamp_ms;
    int64_t pressure_acc;
    int64_t temp_acc;

    if (imu == 0)
    {
        return;
    }

    GY86_Ms5611_ClearFusionDiagnostics();
    fresh_mask = 0u;
    accepted_mask = 0u;
    primary_index = 0xFFu;
    primary_dev = 0;

    for (idx = 0u; idx < GY86_IMU_MS5611_DEVICE_COUNT; idx++)
    {
        gy86_ms5611_device_t *dev;

        dev = &s_gy86_rt.ms5611_dev[idx];
        freshness_score[idx] = 0u;
        fresh_age_ms[idx] = 0u;

        if (GY86_Ms5611_DeviceIsFresh(now_ms, dev) == 0u)
        {
            continue;
        }

        fresh_age_ms[idx] = now_ms - dev->timestamp_ms;
        fresh_mask |= (uint8_t)(1u << idx);
        freshness_score[idx] = (GY86_MS5611_FUSION_FRESH_TIMEOUT_MS - fresh_age_ms[idx]) + 1u;
    }

    if ((fresh_mask & 0x01u) != 0u)
    {
        primary_index = 0u;
    }
    else
    {
        uint32_t best_age_ms;

        best_age_ms = 0xFFFFFFFFu;

        for (idx = 0u; idx < GY86_IMU_MS5611_DEVICE_COUNT; idx++)
        {
            if ((fresh_mask & (1u << idx)) == 0u)
            {
                continue;
            }

            if (fresh_age_ms[idx] < best_age_ms)
            {
                best_age_ms = fresh_age_ms[idx];
                primary_index = (uint8_t)idx;
            }
        }
    }

    if (primary_index >= GY86_IMU_MS5611_DEVICE_COUNT)
    {
        GY86_Ms5611_UpdateAppStateDiagnostics(imu, now_ms);
        return;
    }

    primary_dev = &s_gy86_rt.ms5611_dev[primary_index];
    s_gy86_rt.ms5611_primary_index = primary_index;

    for (idx = 0u; idx < GY86_IMU_MS5611_DEVICE_COUNT; idx++)
    {
        gy86_ms5611_device_t *dev;
        int32_t aligned_pressure_pa;
        int32_t residual_pa;

        dev = &s_gy86_rt.ms5611_dev[idx];

        if ((fresh_mask & (1u << idx)) == 0u)
        {
            if ((dev->online != 0u) || (dev->valid != 0u))
            {
                s_gy86_rt.ms5611_rejected_mask |= (uint8_t)(1u << idx);
                s_gy86_rt.ms5611_fusion_flags |= APP_GY86_BARO_FUSION_FLAG_STALE_FALLBACK;
            }
            continue;
        }

        aligned_pressure_pa = dev->pressure_pa;
        residual_pa = 0;

        if ((idx != 0u) && ((fresh_mask & 0x01u) != 0u))
        {
            float residual_to_bias_pa;
            uint8_t disagree_latched;

            residual_to_bias_pa = (float)(dev->pressure_pa -
                                          s_gy86_rt.ms5611_dev[0].pressure_pa) -
                                  s_gy86_rt.ms5611_pressure_bias_pa[idx];

            if ((residual_to_bias_pa > -GY86_MS5611_FUSION_BIAS_TRACK_GATE_PA) &&
                (residual_to_bias_pa <  GY86_MS5611_FUSION_BIAS_TRACK_GATE_PA))
            {
                s_gy86_rt.ms5611_pressure_bias_pa[idx] +=
                    GY86_MS5611_FUSION_BIAS_TRACK_ALPHA * residual_to_bias_pa;

                if ((s_gy86_rt.ms5611_pressure_bias_pa[idx] > 1.0f) ||
                    (s_gy86_rt.ms5611_pressure_bias_pa[idx] < -1.0f))
                {
                    s_gy86_rt.ms5611_fusion_flags |= APP_GY86_BARO_FUSION_FLAG_OFFSET_TRACK_ACTIVE;
                }
            }

            aligned_pressure_pa = dev->pressure_pa -
                                  GY86_RoundFloatToS32(s_gy86_rt.ms5611_pressure_bias_pa[idx]);
            residual_pa = aligned_pressure_pa - s_gy86_rt.ms5611_dev[0].pressure_pa;

            disagree_latched = ((s_gy86_rt.ms5611_disagree_latched_mask & (1u << idx)) != 0u) ? 1u : 0u;

            if (disagree_latched != 0u)
            {
                if (GY86_Abs32(residual_pa) <= (uint32_t)GY86_MS5611_FUSION_DISAGREE_EXIT_PA)
                {
                    s_gy86_rt.ms5611_disagree_latched_mask &= (uint8_t)~(1u << idx);
                }
                else
                {
                    s_gy86_rt.ms5611_rejected_mask |= (uint8_t)(1u << idx);
                    s_gy86_rt.ms5611_fusion_flags |= APP_GY86_BARO_FUSION_FLAG_DISAGREE_REJECT;
                    s_gy86_rt.ms5611_aligned_pressure_pa[idx] = aligned_pressure_pa;
                    s_gy86_rt.ms5611_residual_pa[idx] = residual_pa;
                    continue;
                }
            }
            else if (GY86_Abs32(residual_pa) > (uint32_t)GY86_MS5611_FUSION_DISAGREE_ENTER_PA)
            {
                s_gy86_rt.ms5611_disagree_latched_mask |= (uint8_t)(1u << idx);
                s_gy86_rt.ms5611_rejected_mask |= (uint8_t)(1u << idx);
                s_gy86_rt.ms5611_fusion_flags |= APP_GY86_BARO_FUSION_FLAG_DISAGREE_REJECT;
                s_gy86_rt.ms5611_aligned_pressure_pa[idx] = aligned_pressure_pa;
                s_gy86_rt.ms5611_residual_pa[idx] = residual_pa;
                continue;
            }
        }

        if (idx == primary_index)
        {
            aligned_pressure_pa = dev->pressure_pa;
            residual_pa = 0;
        }

        accepted_mask |= (uint8_t)(1u << idx);
        s_gy86_rt.ms5611_selected_mask |= (uint8_t)(1u << idx);
        s_gy86_rt.ms5611_aligned_pressure_pa[idx] = aligned_pressure_pa;
        s_gy86_rt.ms5611_residual_pa[idx] = residual_pa;
    }

#if (GY86_IMU_MS5611_DEVICE_COUNT >= 2u)
    if ((s_gy86_rt.ms5611_dev[0].valid != 0u) && (s_gy86_rt.ms5611_dev[1].valid != 0u))
    {
        s_gy86_rt.ms5611_raw_delta_pa =
            s_gy86_rt.ms5611_dev[1].pressure_pa - s_gy86_rt.ms5611_dev[0].pressure_pa;
        s_gy86_rt.ms5611_aligned_delta_pa =
            s_gy86_rt.ms5611_aligned_pressure_pa[1] - s_gy86_rt.ms5611_aligned_pressure_pa[0];
    }
#endif

    if ((accepted_mask & (1u << primary_index)) == 0u)
    {
        accepted_mask |= (uint8_t)(1u << primary_index);
        s_gy86_rt.ms5611_selected_mask |= (uint8_t)(1u << primary_index);
        s_gy86_rt.ms5611_aligned_pressure_pa[primary_index] = primary_dev->pressure_pa;
        s_gy86_rt.ms5611_residual_pa[primary_index] = 0;
    }

    pressure_acc = 0LL;
    temp_acc = 0LL;
    fused_weight_sum = 0u;
    latest_timestamp_ms = primary_dev->timestamp_ms;

    for (idx = 0u; idx < GY86_IMU_MS5611_DEVICE_COUNT; idx++)
    {
        gy86_ms5611_device_t *dev;
        uint32_t weight;

        if ((accepted_mask & (1u << idx)) == 0u)
        {
            continue;
        }

        dev = &s_gy86_rt.ms5611_dev[idx];
        weight = freshness_score[idx];

        if ((idx != primary_index) && (primary_index == 0u))
        {
            uint32_t residual_abs_pa;
            uint32_t penalty_permille;

            residual_abs_pa = GY86_Abs32(s_gy86_rt.ms5611_residual_pa[idx]);
            penalty_permille = 1000u;

            if ((uint32_t)GY86_MS5611_FUSION_DISAGREE_ENTER_PA != 0u)
            {
                uint32_t scaled_penalty;

                scaled_penalty = (residual_abs_pa * 700u) / (uint32_t)GY86_MS5611_FUSION_DISAGREE_ENTER_PA;
                if (scaled_penalty > 700u)
                {
                    scaled_penalty = 700u;
                }

                penalty_permille -= scaled_penalty;
            }

            weight = (weight * penalty_permille) / 1000u;
        }

        if (weight == 0u)
        {
            weight = 1u;
        }

        s_gy86_rt.ms5611_weight_permille[idx] = (uint16_t)weight;
        pressure_acc += (int64_t)s_gy86_rt.ms5611_aligned_pressure_pa[idx] * (int64_t)weight;
        temp_acc += (int64_t)dev->temp_cdeg * (int64_t)weight;
        fused_weight_sum += weight;
        if ((int32_t)(dev->timestamp_ms - latest_timestamp_ms) > 0)
        {
            latest_timestamp_ms = dev->timestamp_ms;
        }
        s_gy86_rt.ms5611_fused_count++;
    }

    if (fused_weight_sum == 0u)
    {
        GY86_Ms5611_UpdateAppStateDiagnostics(imu, now_ms);
        return;
    }

    if (s_gy86_rt.ms5611_fused_count <= 1u)
    {
        s_gy86_rt.ms5611_fusion_flags |= APP_GY86_BARO_FUSION_FLAG_SINGLE_SENSOR;
    }

    for (idx = 0u; idx < GY86_IMU_MS5611_DEVICE_COUNT; idx++)
    {
        if ((accepted_mask & (1u << idx)) != 0u)
        {
            s_gy86_rt.ms5611_weight_permille[idx] =
                (uint16_t)(((uint32_t)s_gy86_rt.ms5611_weight_permille[idx] * 1000u) / fused_weight_sum);
        }
    }

    fused_pressure_pa = (int32_t)(pressure_acc / (int64_t)fused_weight_sum);
    fused_temp_cdeg = (int32_t)(temp_acc / (int64_t)fused_weight_sum);

    if (GY86_Ms5611_PublishedSampleAccepted(imu,
                                            primary_dev,
                                            latest_timestamp_ms,
                                            fused_pressure_pa,
                                            fused_temp_cdeg) == 0u)
    {
        GY86_Ms5611_UpdateAppStateDiagnostics(imu, now_ms);
        return;
    }

    imu->baro.timestamp_ms      = latest_timestamp_ms;
    imu->baro.sample_count      = ++s_gy86_rt.fused_baro_sample_count;
    imu->baro.d1_raw            = primary_dev->d1_raw;
    imu->baro.d2_raw            = primary_dev->d2_raw;
    imu->baro.temp_cdeg         = fused_temp_cdeg;
    imu->baro.pressure_hpa_x100 = fused_pressure_pa;
    imu->baro.pressure_pa       = fused_pressure_pa;

    GY86_Ms5611_CopyPromToAppState(imu);
    GY86_Ms5611_UpdateAppStateDiagnostics(imu, now_ms);

    imu->debug.baro_last_ok_ms   = now_ms;
    imu->debug.ms5611_state      = 0u;
    imu->status_flags           |= APP_GY86_STATUS_BARO_VALID;
    imu->last_update_ms          = now_ms;

    if (updated != 0)
    {
        *updated = 1u;
    }
}

static HAL_StatusTypeDef GY86_Ms5611_PollOne(uint32_t now_ms,
                                             app_gy86_state_t *imu,
                                             gy86_ms5611_device_t *dev,
                                             uint8_t *device_updated)
{
    HAL_StatusTypeDef st;
    uint8_t command;

    if (device_updated != 0)
    {
        *device_updated = 0u;
    }

    if ((imu == 0) || (dev == 0) || (dev->bus == 0) || (dev->online == 0u))
    {
        return HAL_ERROR;
    }

    switch (dev->phase)
    {
        case 0u:
            command = MS5611_CMD_CONV_D1_OSR4096;
            st = GY86_I2C_CommandOnly_Bus(dev->bus, dev->addr, command);
            imu->debug.last_hal_status_baro = (uint8_t)st;
            if (st != HAL_OK)
            {
                imu->debug.baro_error_count++;
                imu->status_flags &= (uint8_t)~APP_GY86_STATUS_BARO_VALID;
                GY86_RecordRuntimeErrorAndMaybeOffline(APP_GY86_DEVICE_BARO,
                                                       &dev->online,
                                                       &dev->error_streak,
                                                       now_ms,
                                                       imu);
                GY86_Ms5611_RefreshBaroOnlineFlag();
                return st;
            }
            dev->deadline_ms = now_ms + GY86_IMU_MS5611_CONV_MS;
            dev->phase = 1u;
            break;

        case 1u:
            if (GY86_TimeDue(now_ms, dev->deadline_ms) == 0u)
            {
                break;
            }

            st = GY86_Ms5611_ReadAdcFromAddress(dev->bus, dev->addr, &dev->d1_raw);
            imu->debug.last_hal_status_baro = (uint8_t)st;
            if (st != HAL_OK)
            {
                dev->phase = 0u;
                imu->debug.baro_error_count++;
                imu->status_flags &= (uint8_t)~APP_GY86_STATUS_BARO_VALID;
                GY86_RecordRuntimeErrorAndMaybeOffline(APP_GY86_DEVICE_BARO,
                                                       &dev->online,
                                                       &dev->error_streak,
                                                       now_ms,
                                                       imu);
                GY86_Ms5611_RefreshBaroOnlineFlag();
                return st;
            }

            command = MS5611_CMD_CONV_D2_OSR4096;
            st = GY86_I2C_CommandOnly_Bus(dev->bus, dev->addr, command);
            imu->debug.last_hal_status_baro = (uint8_t)st;
            if (st != HAL_OK)
            {
                dev->phase = 0u;
                imu->debug.baro_error_count++;
                imu->status_flags &= (uint8_t)~APP_GY86_STATUS_BARO_VALID;
                GY86_RecordRuntimeErrorAndMaybeOffline(APP_GY86_DEVICE_BARO,
                                                       &dev->online,
                                                       &dev->error_streak,
                                                       now_ms,
                                                       imu);
                GY86_Ms5611_RefreshBaroOnlineFlag();
                return st;
            }

            dev->deadline_ms = now_ms + GY86_IMU_MS5611_CONV_MS;
            dev->phase = 2u;
            break;

        case 2u:
            if (GY86_TimeDue(now_ms, dev->deadline_ms) == 0u)
            {
                break;
            }

            st = GY86_Ms5611_ReadAdcFromAddress(dev->bus, dev->addr, &dev->d2_raw);
            imu->debug.last_hal_status_baro = (uint8_t)st;
            if (st != HAL_OK)
            {
                dev->phase = 0u;
                imu->debug.baro_error_count++;
                imu->status_flags &= (uint8_t)~APP_GY86_STATUS_BARO_VALID;
                GY86_RecordRuntimeErrorAndMaybeOffline(APP_GY86_DEVICE_BARO,
                                                       &dev->online,
                                                       &dev->error_streak,
                                                       now_ms,
                                                       imu);
                GY86_Ms5611_RefreshBaroOnlineFlag();
                return st;
            }

            GY86_Ms5611_StoreCompensatedSample(dev, now_ms);
            dev->phase = 0u;
            dev->error_streak = 0u;
            if (device_updated != 0)
            {
                *device_updated = 1u;
            }
            break;

        default:
            dev->phase = 0u;
            break;
    }

    return HAL_OK;
}

static HAL_StatusTypeDef GY86_Ms5611_Poll(uint32_t now_ms,
                                          app_gy86_state_t *imu,
                                          uint8_t *updated)
{
    HAL_StatusTypeDef st;
    uint8_t any_device_updated;
    uint8_t dev_updated;
    uint32_t idx;

    if (updated != 0)
    {
        *updated = 0u;
    }

    if (imu == 0)
    {
        return HAL_ERROR;
    }

    any_device_updated = 0u;

    for (idx = 0u; idx < GY86_IMU_MS5611_DEVICE_COUNT; idx++)
    {
        gy86_ms5611_device_t *dev;

        dev = &s_gy86_rt.ms5611_dev[idx];
        if (dev->online == 0u)
        {
            continue;
        }

        dev_updated = 0u;
        st = GY86_Ms5611_PollOne(now_ms, imu, dev, &dev_updated);
        if (st != HAL_OK)
        {
            continue;
        }

        if (dev_updated != 0u)
        {
            any_device_updated = 1u;
        }
    }

    GY86_Ms5611_RefreshBaroOnlineFlag();

    if (any_device_updated != 0u)
    {
        GY86_Ms5611_PublishFusedSample(imu, now_ms, updated);
    }
    else
    {
        GY86_Ms5611_UpdateAppStateDiagnostics(imu, now_ms);
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
    imu->debug.baro_device_slots = APP_GY86_BARO_SENSOR_SLOTS;
    imu->debug.baro_primary_sensor_index = 0xFFu;

    GY86_Ms5611_UpdateAppStateDiagnostics(imu, HAL_GetTick());
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

    /* ---------------------------------------------------------------------- */
    /*  dual-baro 모드에서는 global flag를 cached copy로 오래 들고 있지 않고    */
    /*  매번 per-device online 상태에서 다시 모은다.                           */
    /* ---------------------------------------------------------------------- */
    s_gy86_rt.baro_online = GY86_Ms5611_AnyOnline();

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
    if (GY86_Ms5611_NeedsProbe() != 0u)
    {
        st = s_baro_backend.init(imu);

        if (st == HAL_OK)
        {
            s_gy86_rt.baro_online = GY86_Ms5611_AnyOnline();
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
    GY86_Ms5611_RuntimeInit();

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
         (GY86_Ms5611_NeedsProbe() != 0u)) &&
        (GY86_TimeDue(now_ms, s_gy86_rt.next_probe_ms) != 0u))
#else
    if (((s_gy86_rt.mpu_online == 0u) ||
         (GY86_Ms5611_NeedsProbe() != 0u)) &&
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
    /*                                                                        */
    /*  HMC5883L raw는                                                          */
    /*  - SELFTEST 의 MAG FLOW 확인                                             */
    /*  - 보조 heading 진단용                                                   */
    /*  에 사용한다.                                                             */
    /*                                                                        */
    /*  주의                                                                   */
    /*  - BIKE_DYNAMICS의 Mahony 6축 자세 추정 / lean 계산에는                  */
    /*    직접 피드백하지 않는다.                                               */
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
    if (GY86_Ms5611_AnyOnline() != 0u)
    {
        updated = 0u;
        (void)s_baro_backend.poll(now_ms, imu, &updated);
    }
}
