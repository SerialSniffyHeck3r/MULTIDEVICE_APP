#ifndef APP_PLATFORM_LSM6DSOX_H
#define APP_PLATFORM_LSM6DSOX_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  LSM6DSOX thin wrapper                                                      */
/*                                                                            */
/*  목적                                                                       */
/*  - GY86_IMU.c 가 LSM6DSOX를 "MPU6050 대체 accel/gyro backend" 로           */
/*    쉽게 붙일 수 있게 하는 얇은 래퍼다.                                     */
/*  - ST 공식 driver/datasheet 가 사용하는 register 배치를 그대로 따르되,     */
/*    프로젝트가 실제로 필요한 기능만 최소한으로 구현한다.                    */
/*  - APP_STATE 호환을 위해 raw accel/gyro/temperature 값을 읽어 오는 데 집중 */
/*    하고, 복잡한 MLC/FSM/FIFO 기능은 여기서 다루지 않는다.                  */
/* -------------------------------------------------------------------------- */

typedef int32_t (*lsm6dsox_read_f)(void *ctx,
                                   uint8_t dev_addr,
                                   uint8_t reg_addr,
                                   uint8_t *data,
                                   uint16_t len);

typedef int32_t (*lsm6dsox_write_f)(void *ctx,
                                    uint8_t dev_addr,
                                    uint8_t reg_addr,
                                    const uint8_t *data,
                                    uint16_t len);

typedef void (*lsm6dsox_delay_ms_f)(void *ctx, uint32_t delay_ms);

/* -------------------------------------------------------------------------- */
/*  Public register constants                                                  */
/* -------------------------------------------------------------------------- */
#define LSM6DSOX_REG_WHO_AM_I              0x0Fu
#define LSM6DSOX_REG_CTRL1_XL              0x10u
#define LSM6DSOX_REG_CTRL2_G               0x11u
#define LSM6DSOX_REG_CTRL3_C               0x12u
#define LSM6DSOX_REG_CTRL9_XL              0x18u
#define LSM6DSOX_REG_STATUS                0x1Eu
#define LSM6DSOX_REG_OUT_TEMP_L            0x20u

#define LSM6DSOX_WHO_AM_I_VALUE            0x6Cu

/* ODR field values (datasheet / ST driver mapping) */
#define LSM6DSOX_ODR_OFF                   0x00u
#define LSM6DSOX_ODR_12HZ5                 0x01u
#define LSM6DSOX_ODR_26HZ                  0x02u
#define LSM6DSOX_ODR_52HZ                  0x03u
#define LSM6DSOX_ODR_104HZ                 0x04u
#define LSM6DSOX_ODR_208HZ                 0x05u
#define LSM6DSOX_ODR_417HZ                 0x06u
#define LSM6DSOX_ODR_833HZ                 0x07u
#define LSM6DSOX_ODR_1667HZ                0x08u
#define LSM6DSOX_ODR_3333HZ                0x09u
#define LSM6DSOX_ODR_6667HZ                0x0Au

/* accel full-scale field values */
#define LSM6DSOX_ACCEL_FS_2G               0x00u
#define LSM6DSOX_ACCEL_FS_16G              0x01u
#define LSM6DSOX_ACCEL_FS_4G               0x02u
#define LSM6DSOX_ACCEL_FS_8G               0x03u

/* gyro full-scale field values */
#define LSM6DSOX_GYRO_FS_250DPS            0x00u
#define LSM6DSOX_GYRO_FS_125DPS            0x01u
#define LSM6DSOX_GYRO_FS_500DPS            0x02u
#define LSM6DSOX_GYRO_FS_1000DPS           0x04u
#define LSM6DSOX_GYRO_FS_2000DPS           0x06u

/* status register bits */
#define LSM6DSOX_STATUS_XLDA               0x01u
#define LSM6DSOX_STATUS_GDA                0x02u
#define LSM6DSOX_STATUS_TDA                0x04u

/* -------------------------------------------------------------------------- */
/*  Configuration / runtime structs                                            */
/* -------------------------------------------------------------------------- */
typedef struct
{
    uint8_t i2c_addr;               /* HAL 8-bit address format (already <<1) */
    uint8_t accel_odr;
    uint8_t accel_fs;
    uint8_t gyro_odr;
    uint8_t gyro_fs;
    uint8_t enable_block_data_update;
    uint8_t enable_auto_increment;
    uint8_t disable_i3c;
    uint8_t enable_accel_lpf2;
} lsm6dsox_config_t;

typedef struct
{
    lsm6dsox_read_f      read;
    lsm6dsox_write_f     write;
    lsm6dsox_delay_ms_f  delay_ms;
    void                *ctx;
} lsm6dsox_io_t;

typedef struct
{
    uint8_t  who_am_i;
    uint8_t  ctrl1_xl_shadow;
    uint8_t  ctrl2_g_shadow;
    uint8_t  ctrl3_c_shadow;
    uint8_t  ctrl9_xl_shadow;
    uint8_t  status_reg;

    int16_t  raw_temp;
    int16_t  raw_gyro_x;
    int16_t  raw_gyro_y;
    int16_t  raw_gyro_z;
    int16_t  raw_accel_x;
    int16_t  raw_accel_y;
    int16_t  raw_accel_z;
    int32_t  temp_cdeg;
} lsm6dsox_sample_t;

typedef struct
{
    lsm6dsox_io_t      io;
    lsm6dsox_config_t  cfg;

    uint8_t            who_am_i;
    uint8_t            ctrl1_xl_shadow;
    uint8_t            ctrl2_g_shadow;
    uint8_t            ctrl3_c_shadow;
    uint8_t            ctrl9_xl_shadow;
    uint8_t            last_status_reg;
} lsm6dsox_device_t;

int32_t LSM6DSOX_Init(lsm6dsox_device_t *dev);

/* -------------------------------------------------------------------------- */
/*  LSM6DSOX_ReadSample                                                        */
/*                                                                            */
/*  new_sample                                                                  */
/*  - 1 : accel/gyro ready bit가 떠서 새로운 샘플을 읽음                       */
/*  - 0 : 아직 ODR 새 샘플이 없음                                              */
/* -------------------------------------------------------------------------- */
int32_t LSM6DSOX_ReadSample(lsm6dsox_device_t *dev,
                            lsm6dsox_sample_t *sample,
                            uint8_t *new_sample);

#ifdef __cplusplus
}
#endif

#endif /* APP_PLATFORM_LSM6DSOX_H */
