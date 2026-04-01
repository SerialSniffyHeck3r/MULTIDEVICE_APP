#ifndef APP_PLATFORM_BMP581_H
#define APP_PLATFORM_BMP581_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  BMP581 thin wrapper                                                        */
/*                                                                            */
/*  목적                                                                       */
/*  - GY86_IMU.c 가 Bosch BMP581을 "드롭인 pressure backend" 로 다루기 쉽게     */
/*    만드는 아주 얇은 래퍼다.                                                */
/*  - 상위 코드는 HAL 을 직접 보지 않고,                                      */
/*    이 드라이버에 read/write/delay callback 만 넘긴다.                      */
/*  - Bosch 공식 BMP5 SensorAPI / datasheet 를 따라                           */
/*    pressure = raw_pressure / 64 [Pa]                                       */
/*    temperature = raw_temperature / 65536 [degC]                            */
/*    규칙으로 값을 꺼낸다.                                                   */
/*                                                                            */
/*  설계 방침                                                                  */
/*  - 복잡한 FIFO/interrupt routing 까지 여기서 다 하지 않는다.               */
/*  - 현재 프로젝트 목적은 "상대고도/바리오용 pressure stream" 이므로            */
/*    I2C normal polling + continuous mode + optional DRDY bit 확인만 한다.   */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*  Callback signature                                                         */
/*                                                                            */
/*  반환 규칙                                                                  */
/*  - 0  : 성공                                                                 */
/*  - <0 : 통신/구현 오류                                                      */
/* -------------------------------------------------------------------------- */
typedef int32_t (*bmp581_read_f)(void *ctx,
                                 uint8_t dev_addr,
                                 uint8_t reg_addr,
                                 uint8_t *data,
                                 uint16_t len);

typedef int32_t (*bmp581_write_f)(void *ctx,
                                  uint8_t dev_addr,
                                  uint8_t reg_addr,
                                  const uint8_t *data,
                                  uint16_t len);

typedef void (*bmp581_delay_ms_f)(void *ctx, uint32_t delay_ms);

/* -------------------------------------------------------------------------- */
/*  Public constants                                                           */
/* -------------------------------------------------------------------------- */
#define BMP581_CHIP_ID_PRIMARY              0x50u
#define BMP581_CHIP_ID_SECONDARY            0x51u

#define BMP581_REG_CHIP_ID                  0x01u
#define BMP581_REG_REV_ID                   0x02u
#define BMP581_REG_CHIP_STATUS              0x11u
#define BMP581_REG_TEMP_DATA_XLSB           0x1Du
#define BMP581_REG_TEMP_DATA_LSB            0x1Eu
#define BMP581_REG_TEMP_DATA_MSB            0x1Fu
#define BMP581_REG_PRESS_DATA_XLSB          0x20u
#define BMP581_REG_PRESS_DATA_LSB           0x21u
#define BMP581_REG_PRESS_DATA_MSB           0x22u
#define BMP581_REG_INT_STATUS               0x27u
#define BMP581_REG_STATUS                   0x28u
#define BMP581_REG_DSP_CONFIG               0x30u
#define BMP581_REG_DSP_IIR                  0x31u
#define BMP581_REG_OSR_CONFIG               0x36u
#define BMP581_REG_ODR_CONFIG               0x37u
#define BMP581_REG_CMD                      0x7Eu

#define BMP581_CMD_SOFT_RESET               0xB6u

/* INT_STATUS bit0: DRDY asserted */
#define BMP581_INT_STATUS_DRDY              0x01u

/* ODR_CONFIG bitfield */
#define BMP581_ODR_SHIFT                    2u
#define BMP581_POWERMODE_MASK               0x03u
#define BMP581_DEEP_DISABLE_MASK            0x80u
#define BMP581_POWERMODE_STANDBY            0x00u
#define BMP581_POWERMODE_NORMAL             0x01u
#define BMP581_POWERMODE_FORCED             0x02u
#define BMP581_POWERMODE_CONTINUOUS         0x03u

/* OSR_CONFIG bitfield */
#define BMP581_TEMP_OS_SHIFT                0u
#define BMP581_PRESS_OS_SHIFT               3u
#define BMP581_PRESS_ENABLE_MASK            0x40u

/* DSP_IIR bitfield */
#define BMP581_TEMP_IIR_SHIFT               0u
#define BMP581_PRESS_IIR_SHIFT              3u

/* Oversampling field values (datasheet / Bosch SensorAPI mapping) */
#define BMP581_OVERSAMPLING_1X              0x00u
#define BMP581_OVERSAMPLING_2X              0x01u
#define BMP581_OVERSAMPLING_4X              0x02u
#define BMP581_OVERSAMPLING_8X              0x03u
#define BMP581_OVERSAMPLING_16X             0x04u
#define BMP581_OVERSAMPLING_32X             0x05u
#define BMP581_OVERSAMPLING_64X             0x06u
#define BMP581_OVERSAMPLING_128X            0x07u

/* IIR field values */
#define BMP581_IIR_BYPASS                   0x00u
#define BMP581_IIR_COEFF_1                  0x01u
#define BMP581_IIR_COEFF_3                  0x02u
#define BMP581_IIR_COEFF_7                  0x03u
#define BMP581_IIR_COEFF_15                 0x04u
#define BMP581_IIR_COEFF_31                 0x05u
#define BMP581_IIR_COEFF_63                 0x06u
#define BMP581_IIR_COEFF_127                0x07u

/* Selected useful ODR field values */
#define BMP581_ODR_240_HZ                   0x00u
#define BMP581_ODR_160_HZ                   0x04u
#define BMP581_ODR_100_2_HZ                 0x0Au
#define BMP581_ODR_80_HZ                    0x0Cu
#define BMP581_ODR_50_HZ                    0x0Fu
#define BMP581_ODR_40_HZ                    0x11u
#define BMP581_ODR_30_HZ                    0x13u
#define BMP581_ODR_25_HZ                    0x14u
#define BMP581_ODR_20_HZ                    0x15u
#define BMP581_ODR_15_HZ                    0x16u
#define BMP581_ODR_10_HZ                    0x17u
#define BMP581_ODR_05_HZ                    0x18u
#define BMP581_ODR_01_HZ                    0x1Cu

/* -------------------------------------------------------------------------- */
/*  Configuration / runtime structs                                            */
/* -------------------------------------------------------------------------- */
typedef struct
{
    /* 8-bit HAL address format: already <<1 된 값을 넣는다. */
    uint8_t            i2c_addr;

    uint8_t            osr_temp;
    uint8_t            osr_press;
    uint8_t            odr;
    uint8_t            iir_temp;
    uint8_t            iir_press;

    /* 1 이면 BMP585 등 chip_id=0x51 도 허용한다.
     * BMP581 보드가 실제로는 sibling chip 으로 바뀌는 상황을 대비한다. */
    uint8_t            accept_chip_id_0x51;
} bmp581_config_t;

typedef struct
{
    bmp581_read_f      read;
    bmp581_write_f     write;
    bmp581_delay_ms_f  delay_ms;
    void              *ctx;
} bmp581_io_t;

typedef struct
{
    uint8_t  chip_id;
    uint8_t  rev_id;
    uint8_t  last_int_status;
    uint8_t  last_status;
    uint8_t  last_chip_status;

    uint8_t  osr_config_shadow;
    uint8_t  odr_config_shadow;
    uint8_t  dsp_config_shadow;
    uint8_t  dsp_iir_shadow;

    uint32_t raw_pressure_u24;
    int32_t  raw_temperature_s24;

    int32_t  pressure_pa;
    int32_t  pressure_hpa_x100;
    int32_t  temp_cdeg;
} bmp581_sample_t;

typedef struct
{
    bmp581_io_t      io;
    bmp581_config_t  cfg;

    uint8_t          chip_id;
    uint8_t          rev_id;
    uint8_t          osr_config_shadow;
    uint8_t          odr_config_shadow;
    uint8_t          dsp_config_shadow;
    uint8_t          dsp_iir_shadow;
    uint8_t          last_int_status;
    uint8_t          last_status;
    uint8_t          last_chip_status;
} bmp581_device_t;

/* -------------------------------------------------------------------------- */
/*  Public API                                                                 */
/* -------------------------------------------------------------------------- */
int32_t BMP581_Init(bmp581_device_t *dev);

/* -------------------------------------------------------------------------- */
/*  BMP581_ReadSample                                                           */
/*                                                                            */
/*  반환                                                                       */
/*  - 0  : 통신은 성공                                                         */
/*  - <0 : 통신/센서 오류                                                      */
/*                                                                            */
/*  new_sample                                                                  */
/*  - 1 : 이번 호출에서 실제로 새로운 DRDY sample을 읽어 옴                   */
/*  - 0 : 센서는 정상이나 아직 새 sample이 없음                               */
/* -------------------------------------------------------------------------- */
int32_t BMP581_ReadSample(bmp581_device_t *dev,
                          bmp581_sample_t *sample,
                          uint8_t *new_sample);

#ifdef __cplusplus
}
#endif

#endif /* APP_PLATFORM_BMP581_H */
