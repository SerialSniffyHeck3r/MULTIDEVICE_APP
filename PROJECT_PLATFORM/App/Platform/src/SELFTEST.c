#include "SELFTEST.h"

#include "APP_SD.h"
#include "APP_STATE.h"
#include "BMP581.h"
#include "GY86_IMU.h"
#include "SPI_Flash.h"
#include "rtc.h"

#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/* 외부 HAL handle                                                             */
/*                                                                            */
/* GY86_IMU.h 안의 GY86_IMU_I2C_HANDLE 매크로는 기본값으로 hi2c1을 가리킨다. */
/* 여기서는 그 매크로를 그대로 extern 심볼 선언에 사용해,                    */
/* .ioc에서 I2C handle 이름이 달라져도 header 매크로만 따라가게 한다.        */
/* -------------------------------------------------------------------------- */
extern I2C_HandleTypeDef GY86_IMU_I2C_HANDLE;
extern RTC_HandleTypeDef hrtc;

/* -------------------------------------------------------------------------- */
/* 내부 정책 상수                                                              */
/*                                                                            */
/* GPS                                                                           */
/* - 100ms마다 rx_bytes 증가 여부를 본다.                                       */
/* - 증가 관측 5회를 만족하면 PASS 한다.                                        */
/*                                                                            */
/* IMU                                                                           */
/* - MPU ID / MAG ID 직접 확인                                                  */
/* - MPU / MAG / BARO sample flow 확인                                          */
/* - accel sanity / baro sanity 확인                                            */
/*                                                                            */
/* SENSORS                                                                       */
/* - DS18B20 정상 샘플 1회                                                      */
/* - Brightness 정상 샘플 1회                                                   */
/*                                                                            */
/* HARDWARE                                                                      */
/* - SPI Flash JEDEC ID read                                                    */
/* - RTC read                                                                    */
/* - SD가 꽂혀 있으면 mount 완료 확인, 없으면 N/A로 통과                        */
/* -------------------------------------------------------------------------- */
#define SELFTEST_GPS_TIMEOUT_MS                 7000u
#define SELFTEST_GPS_SAMPLE_PERIOD_MS            100u
#define SELFTEST_GPS_REQUIRED_HITS                 5u

#define SELFTEST_IMU_TIMEOUT_MS                 4000u
#define SELFTEST_IMU_SAMPLE_PERIOD_MS             80u
#define SELFTEST_IMU_REQUIRED_SCORE                7u

#define SELFTEST_SENSORS_TIMEOUT_MS             3500u
#define SELFTEST_SENSORS_SAMPLE_PERIOD_MS         50u
#define SELFTEST_SENSORS_REQUIRED_SCORE            2u

#define SELFTEST_HARDWARE_TIMEOUT_MS            2500u
#define SELFTEST_HARDWARE_SAMPLE_PERIOD_MS       100u
#define SELFTEST_HARDWARE_REQUIRED_SCORE           3u
#define SELFTEST_HARDWARE_SD_DECIDE_DELAY_MS     250u

#define SELFTEST_I2C_TIMEOUT_MS                  10u

/* -------------------------------------------------------------------------- */
/* GY-86 direct register probe constants                                       */
/*                                                                            */
/* 주의                                                                        */
/* - 이 값들은 GY86_IMU.c 내부 구현과 동일한 register map을 그대로 반영한다. */
/* - private static 심볼을 가져다 쓰지 않고, self-test 모듈 안에               */
/*   동일 상수만 로컬로 재선언한다.                                             */
/* -------------------------------------------------------------------------- */
#define SELFTEST_GY86_MPU6050_ADDR          (0x68u << 1)
#define SELFTEST_GY86_HMC5883L_ADDR         (0x1Eu << 1)
#define SELFTEST_GY86_BMP58X_ADDR           GY86_ALT_BMP581_ADDR
#define SELFTEST_MPU6050_RA_WHO_AM_I        0x75u
#define SELFTEST_HMC5883L_RA_ID_A           0x0Au

/* -------------------------------------------------------------------------- */
/* 내부 런타임: GPS                                                             */
/* -------------------------------------------------------------------------- */
typedef struct
{
    uint32_t next_sample_ms;
    uint32_t last_sample_rx_bytes;
    uint32_t hit_count;
} selftest_gps_runtime_t;

/* -------------------------------------------------------------------------- */
/* 내부 런타임: IMU                                                             */
/* -------------------------------------------------------------------------- */
typedef struct
{
    uint32_t next_sample_ms;
    uint32_t baseline_mpu_samples;
    uint32_t baseline_mag_samples;
    uint32_t baseline_baro_samples;
    uint32_t baseline_baro_sensor_samples[APP_GY86_BARO_SENSOR_SLOTS];
    uint8_t mpu_id_ok;
    uint8_t mag_id_ok;
    uint8_t baro_id_ok;
    uint8_t mpu_flow_ok;
    uint8_t mag_flow_ok;
    uint8_t baro_flow_ok;
    uint8_t accel_sanity_ok;
    uint8_t baro_sanity_ok;
    uint8_t baro_sensor_expected_mask;
    uint8_t baro_sensor_flow_mask;
    uint8_t baro_sensor_sanity_mask;
    uint8_t baro_chip_id;
    uint8_t baro_rev_id;
} selftest_imu_runtime_t;

/* -------------------------------------------------------------------------- */
/* 내부 런타임: SENSORS                                                         */
/* -------------------------------------------------------------------------- */
typedef struct
{
    uint32_t next_sample_ms;
    uint32_t baseline_ds18_samples;
    uint32_t baseline_brightness_samples;
    uint8_t ds18_ok;
    uint8_t brightness_ok;
} selftest_sensors_runtime_t;

/* -------------------------------------------------------------------------- */
/* 내부 런타임: HARDWARE                                                        */
/* -------------------------------------------------------------------------- */
typedef struct
{
    uint32_t next_sample_ms;
    uint8_t flash_ok;
    uint8_t rtc_ok;
    uint8_t sd_ok;
    uint8_t sd_required;
    uint8_t sd_presence_decided;
} selftest_hardware_runtime_t;

/* -------------------------------------------------------------------------- */
/* 전체 static 저장소                                                           */
/* -------------------------------------------------------------------------- */
typedef struct
{
    selftest_report_t report;
    selftest_gps_runtime_t gps;
    selftest_imu_runtime_t imu;
    selftest_sensors_runtime_t sensors;
    selftest_hardware_runtime_t hardware;
} selftest_runtime_t;

static selftest_runtime_t s_selftest;

/* -------------------------------------------------------------------------- */
/* 내부 helper 선언                                                            */
/* -------------------------------------------------------------------------- */
static uint8_t SELFTEST_TimeDue(uint32_t now_ms, uint32_t due_ms);
static void SELFTEST_PrepareItem(selftest_item_report_t *item,
                                 uint32_t now_ms,
                                 uint32_t timeout_ms,
                                 uint32_t progress_target,
                                 const char *initial_text);
static void SELFTEST_FinishPass(selftest_item_report_t *item,
                                uint32_t now_ms,
                                const char *text);
static void SELFTEST_FinishFail(selftest_item_report_t *item,
                                uint32_t now_ms,
                                uint32_t fail_bit,
                                const char *text);
static void SELFTEST_UpdateFinishedFlag(uint32_t now_ms);
static uint32_t SELFTEST_Abs32(int32_t value);
static uint8_t SELFTEST_BaroConfiguredMask(const app_gy86_state_t *imu_snapshot);
static void SELFTEST_BuildBaroFailText(char *out,
                                       size_t out_size,
                                       uint8_t expected_mask,
                                       uint8_t observed_mask,
                                       const char *fallback_text);
static HAL_StatusTypeDef SELFTEST_I2C_ReadU8(uint16_t dev_addr,
                                             uint8_t reg_addr,
                                             uint8_t *out_value);
static HAL_StatusTypeDef SELFTEST_I2C_ReadBuffer(uint16_t dev_addr,
                                                 uint8_t reg_addr,
                                                 uint8_t *buffer,
                                                 uint16_t length);
static uint8_t SELFTEST_RtcCalendarLooksValid(const RTC_TimeTypeDef *time,
                                              const RTC_DateTypeDef *date);
static uint32_t SELFTEST_ImuScore(void);
static uint32_t SELFTEST_SensorsScore(void);
static uint32_t SELFTEST_HardwareScore(void);
static void SELFTEST_TaskGps(uint32_t now_ms);
static void SELFTEST_TaskImu(uint32_t now_ms);
static void SELFTEST_TaskSensors(uint32_t now_ms);
static void SELFTEST_TaskHardware(uint32_t now_ms);

/* -------------------------------------------------------------------------- */
/* wrap-safe due 판정                                                           */
/* -------------------------------------------------------------------------- */
static uint8_t SELFTEST_TimeDue(uint32_t now_ms, uint32_t due_ms)
{
    return ((int32_t)(now_ms - due_ms) >= 0) ? 1u : 0u;
}

/* -------------------------------------------------------------------------- */
/* 절댓값 helper                                                                */
/* -------------------------------------------------------------------------- */
static uint32_t SELFTEST_Abs32(int32_t value)
{
    if (value < 0)
    {
        return (uint32_t)(-value);
    }

    return (uint32_t)value;
}

static uint8_t SELFTEST_BaroConfiguredMask(const app_gy86_state_t *imu_snapshot)
{
    uint32_t idx;
    uint8_t mask;

    if (imu_snapshot == 0)
    {
        return 0u;
    }

    mask = 0u;

    for (idx = 0u; idx < APP_GY86_BARO_SENSOR_SLOTS; idx++)
    {
        if (imu_snapshot->baro_sensor[idx].configured != 0u)
        {
            mask |= (uint8_t)(1u << idx);
        }
    }

    /* ---------------------------------------------------------------------- */
    /*  초기 부트 순간에는 configured 슬롯 정보가 아직 비어 있을 수 있다.      */
    /*  그런 경우에는 compile-time device count를 fallback으로 사용한다.       */
    /* ---------------------------------------------------------------------- */
    if (mask == 0u)
    {
        for (idx = 0u; idx < GY86_IMU_MS5611_DEVICE_COUNT; idx++)
        {
            mask |= (uint8_t)(1u << idx);
        }
    }

    return mask;
}

static void SELFTEST_BuildBaroFailText(char *out,
                                       size_t out_size,
                                       uint8_t expected_mask,
                                       uint8_t observed_mask,
                                       const char *fallback_text)
{
    uint8_t missing_mask;

    if ((out == 0) || (out_size == 0u))
    {
        return;
    }

    missing_mask = expected_mask & (uint8_t)~observed_mask;

    if (missing_mask == 0u)
    {
        if (fallback_text != 0)
        {
            (void)snprintf(out, out_size, "%s", fallback_text);
        }
        else
        {
            (void)snprintf(out, out_size, "BARO FAIL");
        }
        return;
    }

    if (missing_mask == 0x01u)
    {
        (void)snprintf(out, out_size, "BARO1 FAIL");
    }
    else if (missing_mask == 0x02u)
    {
        (void)snprintf(out, out_size, "BARO2 FAIL");
    }
    else
    {
        (void)snprintf(out, out_size, "BARO1+2 FAIL");
    }
}

/* -------------------------------------------------------------------------- */
/* item report 준비                                                             */
/* -------------------------------------------------------------------------- */
static void SELFTEST_PrepareItem(selftest_item_report_t *item,
                                 uint32_t now_ms,
                                 uint32_t timeout_ms,
                                 uint32_t progress_target,
                                 const char *initial_text)
{
    if (item == 0)
    {
        return;
    }

    memset(item, 0, sizeof(*item));
    item->state = SELFTEST_ITEM_STATE_RUNNING;
    item->started_ms = now_ms;
    item->deadline_ms = now_ms + timeout_ms;
    item->progress_target = progress_target;

    if (initial_text != 0)
    {
        (void)snprintf(item->short_text, sizeof(item->short_text), "%s", initial_text);
    }
}

/* -------------------------------------------------------------------------- */
/* item PASS 확정                                                               */
/* -------------------------------------------------------------------------- */
static void SELFTEST_FinishPass(selftest_item_report_t *item,
                                uint32_t now_ms,
                                const char *text)
{
    if (item == 0)
    {
        return;
    }

    item->state = SELFTEST_ITEM_STATE_PASS;
    item->finished_ms = now_ms;
    item->progress_value = item->progress_target;

    if (text != 0)
    {
        (void)snprintf(item->short_text, sizeof(item->short_text), "%s", text);
    }
}

/* -------------------------------------------------------------------------- */
/* item FAIL 확정                                                               */
/* -------------------------------------------------------------------------- */
static void SELFTEST_FinishFail(selftest_item_report_t *item,
                                uint32_t now_ms,
                                uint32_t fail_bit,
                                const char *text)
{
    if (item == 0)
    {
        return;
    }

    item->state = SELFTEST_ITEM_STATE_FAIL;
    item->finished_ms = now_ms;
    s_selftest.report.any_failed = true;
    s_selftest.report.fail_mask |= fail_bit;

    if (text != 0)
    {
        (void)snprintf(item->short_text, sizeof(item->short_text), "%s", text);
    }
}

/* -------------------------------------------------------------------------- */
/* 4개 항목 종료 여부 집계                                                      */
/* -------------------------------------------------------------------------- */
static void SELFTEST_UpdateFinishedFlag(uint32_t now_ms)
{
    if (s_selftest.report.finished != false)
    {
        return;
    }

    if ((s_selftest.report.gps.state != SELFTEST_ITEM_STATE_RUNNING) &&
        (s_selftest.report.imu.state != SELFTEST_ITEM_STATE_RUNNING) &&
        (s_selftest.report.sensors.state != SELFTEST_ITEM_STATE_RUNNING) &&
        (s_selftest.report.hardware.state != SELFTEST_ITEM_STATE_RUNNING))
    {
        s_selftest.report.finished = true;
        s_selftest.report.finish_ms = now_ms;
    }
}

/* -------------------------------------------------------------------------- */
/* direct I2C 1-byte read helper                                               */
/* -------------------------------------------------------------------------- */
static HAL_StatusTypeDef SELFTEST_I2C_ReadU8(uint16_t dev_addr,
                                             uint8_t reg_addr,
                                             uint8_t *out_value)
{
    if (out_value == 0)
    {
        return HAL_ERROR;
    }

    return HAL_I2C_Mem_Read(&GY86_IMU_I2C_HANDLE,
                            dev_addr,
                            reg_addr,
                            I2C_MEMADD_SIZE_8BIT,
                            out_value,
                            1u,
                            SELFTEST_I2C_TIMEOUT_MS);
}

/* -------------------------------------------------------------------------- */
/* direct I2C multi-byte read helper                                           */
/* -------------------------------------------------------------------------- */
static HAL_StatusTypeDef SELFTEST_I2C_ReadBuffer(uint16_t dev_addr,
                                                 uint8_t reg_addr,
                                                 uint8_t *buffer,
                                                 uint16_t length)
{
    if ((buffer == 0) || (length == 0u))
    {
        return HAL_ERROR;
    }

    return HAL_I2C_Mem_Read(&GY86_IMU_I2C_HANDLE,
                            dev_addr,
                            reg_addr,
                            I2C_MEMADD_SIZE_8BIT,
                            buffer,
                            length,
                            SELFTEST_I2C_TIMEOUT_MS);
}

/* -------------------------------------------------------------------------- */
/* RTC 값의 기본 범위 sanity check                                              */
/*                                                                            */
/* 여기서는 "실제 날짜가 정확한가"가 아니라                                   */
/* "RTC read가 완전히 깨진 값은 아닌가"만 본다.                               */
/* -------------------------------------------------------------------------- */
static uint8_t SELFTEST_RtcCalendarLooksValid(const RTC_TimeTypeDef *time,
                                              const RTC_DateTypeDef *date)
{
    if ((time == 0) || (date == 0))
    {
        return 0u;
    }

    if (time->Hours > 23u)
    {
        return 0u;
    }

    if (time->Minutes > 59u)
    {
        return 0u;
    }

    if (time->Seconds > 59u)
    {
        return 0u;
    }

    if ((date->Month < 1u) || (date->Month > 12u))
    {
        return 0u;
    }

    if ((date->Date < 1u) || (date->Date > 31u))
    {
        return 0u;
    }

    return 1u;
}

/* -------------------------------------------------------------------------- */
/* IMU subcheck score 합산                                                     */
/* -------------------------------------------------------------------------- */
static uint32_t SELFTEST_ImuScore(void)
{
    uint32_t score;

    score = 0u;
    score += (s_selftest.imu.mpu_id_ok != 0u) ? 1u : 0u;
    score += (s_selftest.imu.mag_id_ok != 0u) ? 1u : 0u;
    score += (s_selftest.imu.mpu_flow_ok != 0u) ? 1u : 0u;
    score += (s_selftest.imu.mag_flow_ok != 0u) ? 1u : 0u;
    score += (s_selftest.imu.baro_flow_ok != 0u) ? 1u : 0u;
    score += (s_selftest.imu.accel_sanity_ok != 0u) ? 1u : 0u;
    score += (s_selftest.imu.baro_sanity_ok != 0u) ? 1u : 0u;

    return score;
}

static uint8_t SELFTEST_BaroChipIdAccepted(uint8_t chip_id)
{
    if (chip_id == BMP581_CHIP_ID_PRIMARY)
    {
        return 1u;
    }

    if (chip_id == BMP581_CHIP_ID_SECONDARY)
    {
        return 1u;
    }

    return 0u;
}

/* -------------------------------------------------------------------------- */
/* sensor subcheck score 합산                                                  */
/* -------------------------------------------------------------------------- */
static uint32_t SELFTEST_SensorsScore(void)
{
    uint32_t score;

    score = 0u;
    score += (s_selftest.sensors.ds18_ok != 0u) ? 1u : 0u;
    score += (s_selftest.sensors.brightness_ok != 0u) ? 1u : 0u;

    return score;
}

/* -------------------------------------------------------------------------- */
/* hardware subcheck score 합산                                                */
/* -------------------------------------------------------------------------- */
static uint32_t SELFTEST_HardwareScore(void)
{
    uint32_t score;

    score = 0u;
    score += (s_selftest.hardware.flash_ok != 0u) ? 1u : 0u;
    score += (s_selftest.hardware.rtc_ok != 0u) ? 1u : 0u;
    score += (s_selftest.hardware.sd_ok != 0u) ? 1u : 0u;

    return score;
}

/* -------------------------------------------------------------------------- */
/* 공개 API: reset                                                              */
/* -------------------------------------------------------------------------- */
void SELFTEST_Reset(void)
{
    memset(&s_selftest, 0, sizeof(s_selftest));

    (void)snprintf(s_selftest.report.gps.short_text,
                   sizeof(s_selftest.report.gps.short_text),
                   "IDLE");
    (void)snprintf(s_selftest.report.imu.short_text,
                   sizeof(s_selftest.report.imu.short_text),
                   "IDLE");
    (void)snprintf(s_selftest.report.sensors.short_text,
                   sizeof(s_selftest.report.sensors.short_text),
                   "IDLE");
    (void)snprintf(s_selftest.report.hardware.short_text,
                   sizeof(s_selftest.report.hardware.short_text),
                   "IDLE");
}

/* -------------------------------------------------------------------------- */
/* 공개 API: begin                                                              */
/* -------------------------------------------------------------------------- */
void SELFTEST_Begin(uint32_t now_ms)
{
    app_gps_state_t gps_snapshot;
    app_gy86_state_t imu_snapshot;
    app_ds18b20_state_t ds18_snapshot;
    app_brightness_state_t brightness_snapshot;

    SELFTEST_Reset();

    s_selftest.report.started = true;
    s_selftest.report.start_ms = now_ms;

    SELFTEST_PrepareItem(&s_selftest.report.gps,
                         now_ms,
                         SELFTEST_GPS_TIMEOUT_MS,
                         SELFTEST_GPS_REQUIRED_HITS,
                         "TESTING");
    SELFTEST_PrepareItem(&s_selftest.report.imu,
                         now_ms,
                         SELFTEST_IMU_TIMEOUT_MS,
                         SELFTEST_IMU_REQUIRED_SCORE,
                         "TESTING");
    SELFTEST_PrepareItem(&s_selftest.report.sensors,
                         now_ms,
                         SELFTEST_SENSORS_TIMEOUT_MS,
                         SELFTEST_SENSORS_REQUIRED_SCORE,
                         "TESTING");
    SELFTEST_PrepareItem(&s_selftest.report.hardware,
                         now_ms,
                         SELFTEST_HARDWARE_TIMEOUT_MS,
                         SELFTEST_HARDWARE_REQUIRED_SCORE,
                         "TESTING");

    /* ---------------------------------------------------------------------- */
    /* 시작 시점 baseline snapshot 확보                                        */
    /*                                                                        */
    /* 이후 증가량 비교가 필요한 항목은 이 baseline을 기준으로 본다.          */
    /* ---------------------------------------------------------------------- */
    APP_STATE_CopyGpsSnapshot(&gps_snapshot);
    APP_STATE_CopyGy86Snapshot(&imu_snapshot);
    APP_STATE_CopyDs18b20Snapshot(&ds18_snapshot);
    APP_STATE_CopyBrightnessSnapshot(&brightness_snapshot);

    s_selftest.gps.next_sample_ms = now_ms;
    s_selftest.gps.last_sample_rx_bytes = gps_snapshot.rx_bytes;
    s_selftest.gps.hit_count = 0u;

    s_selftest.imu.next_sample_ms = now_ms;
    s_selftest.imu.baseline_mpu_samples = imu_snapshot.mpu.sample_count;
    s_selftest.imu.baseline_mag_samples = imu_snapshot.mag.sample_count;
    s_selftest.imu.baseline_baro_samples = imu_snapshot.baro.sample_count;
    s_selftest.imu.baro_sensor_expected_mask = SELFTEST_BaroConfiguredMask(&imu_snapshot);

    {
        uint32_t idx;

        for (idx = 0u; idx < APP_GY86_BARO_SENSOR_SLOTS; idx++)
        {
            s_selftest.imu.baseline_baro_sensor_samples[idx] =
                imu_snapshot.baro_sensor[idx].sample_count;
        }
    }

    s_selftest.sensors.next_sample_ms = now_ms;
    s_selftest.sensors.baseline_ds18_samples = ds18_snapshot.raw.sample_count;
    s_selftest.sensors.baseline_brightness_samples = brightness_snapshot.sample_count;

    s_selftest.hardware.next_sample_ms = now_ms;
}

/* -------------------------------------------------------------------------- */
/* GPS 검사                                                                    */
/*                                                                            */
/* PASS 조건                                                                    */
/* - configured = true                                                         */
/* - uart_rx_running = true                                                    */
/* - 100ms 샘플 윈도우 기준 rx_bytes 증가 관측 5회                            */
/* -------------------------------------------------------------------------- */
static void SELFTEST_TaskGps(uint32_t now_ms)
{
    app_gps_state_t gps_snapshot;
    selftest_item_report_t *item;

    item = &s_selftest.report.gps;
    if (item->state != SELFTEST_ITEM_STATE_RUNNING)
    {
        return;
    }

    if (SELFTEST_TimeDue(now_ms, s_selftest.gps.next_sample_ms) == 0u)
    {
        if (SELFTEST_TimeDue(now_ms, item->deadline_ms) != 0u)
        {
            SELFTEST_FinishFail(item, now_ms, SELFTEST_FAIL_GPS, "ERROR G-3");
        }
        return;
    }

    s_selftest.gps.next_sample_ms = now_ms + SELFTEST_GPS_SAMPLE_PERIOD_MS;

    APP_STATE_CopyGpsSnapshot(&gps_snapshot);

    if ((gps_snapshot.configured != false) &&
        (gps_snapshot.uart_rx_running != false) &&
        (gps_snapshot.rx_bytes > s_selftest.gps.last_sample_rx_bytes))
    {
        if (s_selftest.gps.hit_count < SELFTEST_GPS_REQUIRED_HITS)
        {
            s_selftest.gps.hit_count++;
        }
    }

    s_selftest.gps.last_sample_rx_bytes = gps_snapshot.rx_bytes;
    item->progress_value = s_selftest.gps.hit_count;
    (void)snprintf(item->short_text,
                   sizeof(item->short_text),
                   "TEST %lu/%lu",
                   (unsigned long)item->progress_value,
                   (unsigned long)item->progress_target);

    if (s_selftest.gps.hit_count >= SELFTEST_GPS_REQUIRED_HITS)
    {
        SELFTEST_FinishPass(item, now_ms, "GPS OK!");
        return;
    }

    if (SELFTEST_TimeDue(now_ms, item->deadline_ms) != 0u)
    {
        SELFTEST_FinishFail(item, now_ms, SELFTEST_FAIL_GPS, "ERROR G-3L"); // FAIL RX TIMEOUT
    }
}

/* -------------------------------------------------------------------------- */
/* IMU 검사                                                                    */
/*                                                                            */
/* PASS 조건                                                                    */
/* 1) MPU WHO_AM_I read OK                                                     */
/* 2) HMC5883L ID A/B/C read OK                                                */
/* 3) MPU sample_count 증가 + VALID flag                                        */
/* 4) MAG sample_count 증가 + VALID flag                                        */
/* 5) BARO sample_count 증가 + VALID flag                                       */
/* 6) accel raw 합이 0이 아니고 비정상적으로 크지 않음                          */
/* 7) pressure/temp 값이 현실적인 범위 안                                      */
/* -------------------------------------------------------------------------- */
static void SELFTEST_TaskImu(uint32_t now_ms)
{
    app_gy86_state_t imu_snapshot;
    selftest_item_report_t *item;
    uint8_t who_am_i;
    uint8_t mag_ids[3];
    uint8_t baro_chip_id;
    uint8_t baro_rev_id;
    HAL_StatusTypeDef st;
    uint32_t accel_sum_abs;

    item = &s_selftest.report.imu;
    if (item->state != SELFTEST_ITEM_STATE_RUNNING)
    {
        return;
    }

    if (SELFTEST_TimeDue(now_ms, s_selftest.imu.next_sample_ms) == 0u)
    {
        if (SELFTEST_TimeDue(now_ms, item->deadline_ms) != 0u)
        {
            if (s_selftest.imu.mpu_id_ok == 0u)
            {
                SELFTEST_FinishFail(item, now_ms, SELFTEST_FAIL_IMU, "ERROR 1-1");
            }
            else if (s_selftest.imu.mag_id_ok == 0u)
            {
                SELFTEST_FinishFail(item, now_ms, SELFTEST_FAIL_IMU, "ERROR 2-1");
            }
            else if (s_selftest.imu.mpu_flow_ok == 0u)
            {
                SELFTEST_FinishFail(item, now_ms, SELFTEST_FAIL_IMU, "ERROR 2-2");
            }
            else if (s_selftest.imu.mag_flow_ok == 0u)
            {
                SELFTEST_FinishFail(item, now_ms, SELFTEST_FAIL_IMU, "ERROR 3-2");
            }
#if (GY86_BARO_BACKEND == GY86_BARO_BACKEND_BMP581)
            else if (s_selftest.imu.baro_id_ok == 0u)
            {
                char fail_text[sizeof(item->short_text)];
                (void)snprintf(fail_text,
                               sizeof(fail_text),
                               "BARO ID %02X",
                               (unsigned)s_selftest.imu.baro_chip_id);
                SELFTEST_FinishFail(item, now_ms, SELFTEST_FAIL_IMU, fail_text);
            }
#endif
            else if (s_selftest.imu.baro_flow_ok == 0u)
            {
                char fail_text[sizeof(item->short_text)];
                SELFTEST_BuildBaroFailText(fail_text,
                                           sizeof(fail_text),
                                           s_selftest.imu.baro_sensor_expected_mask,
                                           s_selftest.imu.baro_sensor_flow_mask,
                                           "BARO FAIL");
                SELFTEST_FinishFail(item, now_ms, SELFTEST_FAIL_IMU, fail_text);
            }
            else if (s_selftest.imu.accel_sanity_ok == 0u)
            {
                SELFTEST_FinishFail(item, now_ms, SELFTEST_FAIL_IMU, "ERROR 5-3");
            }
            else
            {
                char fail_text[sizeof(item->short_text)];
                SELFTEST_BuildBaroFailText(fail_text,
                                           sizeof(fail_text),
                                           s_selftest.imu.baro_sensor_expected_mask,
                                           s_selftest.imu.baro_sensor_sanity_mask,
                                           "BARO FAIL");
                SELFTEST_FinishFail(item, now_ms, SELFTEST_FAIL_IMU, fail_text);
            }
        }
        return;
    }

    s_selftest.imu.next_sample_ms = now_ms + SELFTEST_IMU_SAMPLE_PERIOD_MS;

    /* ---------------------------------------------------------------------- */
    /* direct ID probe                                                         */
    /*                                                                        */
    /* 드라이버 APP_STATE만 보는 것보다 한 단계 더 엄격하게,                   */
    /* 실제 I2C register read 성공 여부도 확인한다.                            */
    /* ---------------------------------------------------------------------- */
    st = SELFTEST_I2C_ReadU8(SELFTEST_GY86_MPU6050_ADDR,
                             SELFTEST_MPU6050_RA_WHO_AM_I,
                             &who_am_i);
    if ((st == HAL_OK) && ((who_am_i == 0x68u) || (who_am_i == 0x69u)))
    {
        s_selftest.imu.mpu_id_ok = 1u;
    }

    st = SELFTEST_I2C_ReadBuffer(SELFTEST_GY86_HMC5883L_ADDR,
                                 SELFTEST_HMC5883L_RA_ID_A,
                                 mag_ids,
                                 3u);
    if ((st == HAL_OK) &&
        (mag_ids[0] == 0x48u) &&
        (mag_ids[1] == 0x34u) &&
        (mag_ids[2] == 0x33u))
    {
        s_selftest.imu.mag_id_ok = 1u;
    }

#if (GY86_BARO_BACKEND == GY86_BARO_BACKEND_BMP581)
    st = SELFTEST_I2C_ReadU8(SELFTEST_GY86_BMP58X_ADDR,
                             BMP581_REG_CHIP_ID,
                             &baro_chip_id);
    if (st == HAL_OK)
    {
        s_selftest.imu.baro_chip_id = baro_chip_id;

        st = SELFTEST_I2C_ReadU8(SELFTEST_GY86_BMP58X_ADDR,
                                 BMP581_REG_REV_ID,
                                 &baro_rev_id);
        if (st == HAL_OK)
        {
            s_selftest.imu.baro_rev_id = baro_rev_id;
        }

        if (SELFTEST_BaroChipIdAccepted(baro_chip_id) != 0u)
        {
            s_selftest.imu.baro_id_ok = 1u;
        }
    }
#endif

    APP_STATE_CopyGy86Snapshot(&imu_snapshot);

    if (((imu_snapshot.debug.init_ok_mask & APP_GY86_DEVICE_MPU) != 0u) &&
        ((imu_snapshot.status_flags & APP_GY86_STATUS_MPU_VALID) != 0u) &&
        (imu_snapshot.mpu.sample_count > s_selftest.imu.baseline_mpu_samples))
    {
        s_selftest.imu.mpu_flow_ok = 1u;
    }

    if (((imu_snapshot.debug.init_ok_mask & APP_GY86_DEVICE_MAG) != 0u) &&
        ((imu_snapshot.status_flags & APP_GY86_STATUS_MAG_VALID) != 0u) &&
        (imu_snapshot.mag.sample_count > s_selftest.imu.baseline_mag_samples) &&
        ((imu_snapshot.mag.mag_x_raw != 0) ||
         (imu_snapshot.mag.mag_y_raw != 0) ||
         (imu_snapshot.mag.mag_z_raw != 0)))
    {
        s_selftest.imu.mag_flow_ok = 1u;
    }

    if (((imu_snapshot.debug.init_ok_mask & APP_GY86_DEVICE_BARO) != 0u) &&
        ((imu_snapshot.status_flags & APP_GY86_STATUS_BARO_VALID) != 0u) &&
        (imu_snapshot.baro.sample_count > s_selftest.imu.baseline_baro_samples))
    {
        uint32_t idx;

        for (idx = 0u; idx < APP_GY86_BARO_SENSOR_SLOTS; idx++)
        {
            const app_gy86_baro_sensor_state_t *baro_sensor;

            if ((s_selftest.imu.baro_sensor_expected_mask & (1u << idx)) == 0u)
            {
                continue;
            }

            baro_sensor = &imu_snapshot.baro_sensor[idx];
            if ((baro_sensor->online != 0u) &&
                (baro_sensor->valid != 0u) &&
                (baro_sensor->fresh != 0u) &&
                (baro_sensor->sample_count > s_selftest.imu.baseline_baro_sensor_samples[idx]))
            {
                s_selftest.imu.baro_sensor_flow_mask |= (uint8_t)(1u << idx);
            }
        }
    }

    if ((s_selftest.imu.baro_sensor_expected_mask != 0u) &&
        ((s_selftest.imu.baro_sensor_flow_mask & s_selftest.imu.baro_sensor_expected_mask) ==
         s_selftest.imu.baro_sensor_expected_mask))
    {
        s_selftest.imu.baro_flow_ok = 1u;
    }

    accel_sum_abs = 0u;
    accel_sum_abs += SELFTEST_Abs32((int32_t)imu_snapshot.mpu.accel_x_raw);
    accel_sum_abs += SELFTEST_Abs32((int32_t)imu_snapshot.mpu.accel_y_raw);
    accel_sum_abs += SELFTEST_Abs32((int32_t)imu_snapshot.mpu.accel_z_raw);

    if ((accel_sum_abs > 1500u) && (accel_sum_abs < 60000u))
    {
        s_selftest.imu.accel_sanity_ok = 1u;
    }

    /* ---------------------------------------------------------------------- */
    /* BARO sanity check                                                       */
    /*                                                                        */
    /* 기존 구현은 pressure_pa 와 temp_cdeg 가 부팅 초반부터 즉시             */
    /* 현실 범위에 들어와야만 PASS 시켰다.                                     */
    /*                                                                        */
    /* 그러나 실제 장비에서는 BARO flow/sample_count 와 VALID 비트는 먼저     */
    /* 올라오고, pressure_pa 최종값은 몇 샘플 뒤에 안정화될 수 있다.          */
    /* 그래서 여기서는 다음 조건을 만족하면 BARO sanity 를 PASS 시킨다.       */
    /*                                                                        */
    /* 1) BARO init 성공 비트가 이미 올라와 있을 것                            */
    /* 2) BARO VALID 비트가 이미 올라와 있을 것                                */
    /* 3) baseline 대비 새 BARO 샘플이 2회 이상 들어왔을 것                    */
    /* 4) pressure_pa 또는 pressure_hpa_x100 중 하나가 현실 범위일 것          */
    /*                                                                        */
    /* 주의                                                                    */
    /* - pressure_hpa_x100 는 0.01 hPa 단위이며, 숫자 크기상 sea-level 부근     */
    /*   값이 pressure_pa 와 동일한 101325 근처가 된다.                        */
    /* - 온도는 부트 초반 변동 때문에 hard fail 조건에서 제외하고,             */
    /*   pressure 값 안정성과 샘플 진행성을 더 중요하게 본다.                  */
    /* ---------------------------------------------------------------------- */
    {
        uint32_t idx;

        for (idx = 0u; idx < APP_GY86_BARO_SENSOR_SLOTS; idx++)
        {
            const app_gy86_baro_sensor_state_t *baro_sensor;
            uint32_t baro_new_samples;
            uint8_t  baro_pressure_ok;

            if ((s_selftest.imu.baro_sensor_expected_mask & (1u << idx)) == 0u)
            {
                continue;
            }

            baro_sensor = &imu_snapshot.baro_sensor[idx];
            baro_new_samples = 0u;

            if (baro_sensor->sample_count > s_selftest.imu.baseline_baro_sensor_samples[idx])
            {
                baro_new_samples =
                    baro_sensor->sample_count - s_selftest.imu.baseline_baro_sensor_samples[idx];
            }

            baro_pressure_ok = 0u;

            if ((baro_sensor->pressure_pa > 30000) &&
                (baro_sensor->pressure_pa < 120000))
            {
                baro_pressure_ok = 1u;
            }

            if ((baro_sensor->pressure_hpa_x100 > 30000) &&
                (baro_sensor->pressure_hpa_x100 < 120000))
            {
                baro_pressure_ok = 1u;
            }

            if ((baro_sensor->online != 0u) &&
                (baro_sensor->valid != 0u) &&
                (baro_sensor->fresh != 0u) &&
                (baro_new_samples >= 2u) &&
                (baro_pressure_ok != 0u))
            {
                s_selftest.imu.baro_sensor_sanity_mask |= (uint8_t)(1u << idx);
            }
        }

        if ((s_selftest.imu.baro_sensor_expected_mask != 0u) &&
            ((s_selftest.imu.baro_sensor_sanity_mask & s_selftest.imu.baro_sensor_expected_mask) ==
             s_selftest.imu.baro_sensor_expected_mask))
        {
            s_selftest.imu.baro_sanity_ok = 1u;
        }
    }

    item->progress_value = SELFTEST_ImuScore();
    (void)snprintf(item->short_text,
                   sizeof(item->short_text),
                   "TEST %lu/%lu",
                   (unsigned long)item->progress_value,
                   (unsigned long)item->progress_target);

    if (item->progress_value >= item->progress_target)
    {
        SELFTEST_FinishPass(item, now_ms, "SENSORS OK!"); // OK MPU/MAG/BARO
        return;
    }

    if (SELFTEST_TimeDue(now_ms, item->deadline_ms) != 0u)
    {
        if (s_selftest.imu.mpu_id_ok == 0u)
        {
            SELFTEST_FinishFail(item, now_ms, SELFTEST_FAIL_IMU, "ERROR 1-1L"); //FAIL MPU ID
        }
        else if (s_selftest.imu.mag_id_ok == 0u)
        {
            SELFTEST_FinishFail(item, now_ms, SELFTEST_FAIL_IMU, "ERROR 2-1L"); //FAIL MAG ID
        }
        else if (s_selftest.imu.mpu_flow_ok == 0u)
        {
            SELFTEST_FinishFail(item, now_ms, SELFTEST_FAIL_IMU, "ERROR 2-2L"); //FAIL MPU FLOW
        }
        else if (s_selftest.imu.mag_flow_ok == 0u)
        {
            SELFTEST_FinishFail(item, now_ms, SELFTEST_FAIL_IMU, "ERROR 3-2L"); //FAIL MAG FLOW
        }
#if (GY86_BARO_BACKEND == GY86_BARO_BACKEND_BMP581)
        else if (s_selftest.imu.baro_id_ok == 0u)
        {
            char fail_text[sizeof(item->short_text)];
            (void)snprintf(fail_text,
                           sizeof(fail_text),
                           "BARO ID %02X",
                           (unsigned)s_selftest.imu.baro_chip_id);
            SELFTEST_FinishFail(item, now_ms, SELFTEST_FAIL_IMU, fail_text);
        }
#endif
        else if (s_selftest.imu.baro_flow_ok == 0u)
        {
            char fail_text[sizeof(item->short_text)];
            SELFTEST_BuildBaroFailText(fail_text,
                                       sizeof(fail_text),
                                       s_selftest.imu.baro_sensor_expected_mask,
                                       s_selftest.imu.baro_sensor_flow_mask,
                                       "BARO FAIL");
            SELFTEST_FinishFail(item, now_ms, SELFTEST_FAIL_IMU, fail_text);
        }
        else if (s_selftest.imu.accel_sanity_ok == 0u)
        {
            SELFTEST_FinishFail(item, now_ms, SELFTEST_FAIL_IMU, "ERROR 5-3L"); //FAIL ACCEL SANITY
        }
        else
        {
            char fail_text[sizeof(item->short_text)];
            SELFTEST_BuildBaroFailText(fail_text,
                                       sizeof(fail_text),
                                       s_selftest.imu.baro_sensor_expected_mask,
                                       s_selftest.imu.baro_sensor_sanity_mask,
                                       "BARO FAIL");
            SELFTEST_FinishFail(item, now_ms, SELFTEST_FAIL_IMU, fail_text);
        }
    }
}

/* -------------------------------------------------------------------------- */
/* SENSORS 검사                                                                */
/*                                                                            */
/* PASS 조건                                                                    */
/* - DS18B20: PRESENT + VALID + CRC_OK + 샘플 증가                             */
/* - BRIGHTNESS: valid + 샘플 증가                                              */
/* -------------------------------------------------------------------------- */
static void SELFTEST_TaskSensors(uint32_t now_ms)
{
    app_ds18b20_state_t ds18_snapshot;
    app_brightness_state_t brightness_snapshot;
    selftest_item_report_t *item;

    item = &s_selftest.report.sensors;
    if (item->state != SELFTEST_ITEM_STATE_RUNNING)
    {
        return;
    }

    if (SELFTEST_TimeDue(now_ms, s_selftest.sensors.next_sample_ms) == 0u)
    {
        if (SELFTEST_TimeDue(now_ms, item->deadline_ms) != 0u)
        {
            if (s_selftest.sensors.ds18_ok == 0u)
            {
                SELFTEST_FinishFail(item, now_ms, SELFTEST_FAIL_SENSORS, "ERROR T-1"); // ds18 fail
            }
            else
            {
                SELFTEST_FinishFail(item, now_ms, SELFTEST_FAIL_SENSORS, "ERROR B-1"); // brt fail
            }
        }
        return;
    }

    s_selftest.sensors.next_sample_ms = now_ms + SELFTEST_SENSORS_SAMPLE_PERIOD_MS;

    APP_STATE_CopyDs18b20Snapshot(&ds18_snapshot);
    APP_STATE_CopyBrightnessSnapshot(&brightness_snapshot);

    if ((ds18_snapshot.initialized != false) &&
        ((ds18_snapshot.status_flags & APP_DS18B20_STATUS_PRESENT) != 0u) &&
        ((ds18_snapshot.status_flags & APP_DS18B20_STATUS_VALID) != 0u) &&
        ((ds18_snapshot.status_flags & APP_DS18B20_STATUS_CRC_OK) != 0u) &&
        (ds18_snapshot.raw.temp_c_x100 != APP_DS18B20_TEMP_INVALID) &&
        (ds18_snapshot.raw.sample_count > s_selftest.sensors.baseline_ds18_samples))
    {
        s_selftest.sensors.ds18_ok = 1u;
    }

    if ((brightness_snapshot.initialized != false) &&
        (brightness_snapshot.valid != false) &&
        (brightness_snapshot.sample_count > s_selftest.sensors.baseline_brightness_samples) &&
        (brightness_snapshot.raw_min <= brightness_snapshot.raw_average) &&
        (brightness_snapshot.raw_average <= brightness_snapshot.raw_max) &&
        (brightness_snapshot.calibrated_counts <= 4095u))
    {
        s_selftest.sensors.brightness_ok = 1u;
    }

    item->progress_value = SELFTEST_SensorsScore();
    (void)snprintf(item->short_text,
                   sizeof(item->short_text),
                   "TEST %lu/%lu",
                   (unsigned long)item->progress_value,
                   (unsigned long)item->progress_target);

    if (item->progress_value >= item->progress_target)
    {
        SELFTEST_FinishPass(item, now_ms, "SENSORS OK!");
        return;
    }

    if (SELFTEST_TimeDue(now_ms, item->deadline_ms) != 0u)
    {
        if (s_selftest.sensors.ds18_ok == 0u)
        {
            SELFTEST_FinishFail(item, now_ms, SELFTEST_FAIL_SENSORS, "ERROR T-2");
        }
        else
        {
            SELFTEST_FinishFail(item, now_ms, SELFTEST_FAIL_SENSORS, "ERROR B-2");
        }
    }
}

/* -------------------------------------------------------------------------- */
/* HARDWARE 검사                                                               */
/*                                                                            */
/* PASS 조건                                                                    */
/* - SPI Flash JEDEC ID read OK                                                */
/* - RTC read OK                                                               */
/* - SD detect가 present이면 mount 완료, 없으면 N/A로 통과                     */
/* -------------------------------------------------------------------------- */
static void SELFTEST_TaskHardware(uint32_t now_ms)
{
    app_sd_state_t sd_snapshot;
    selftest_item_report_t *item;
    RTC_TimeTypeDef rtc_time;
    RTC_DateTypeDef rtc_date;
    HAL_StatusTypeDef st;
    uint8_t manufacturer_id;
    uint8_t memory_type;
    uint8_t capacity_id;

    item = &s_selftest.report.hardware;
    if (item->state != SELFTEST_ITEM_STATE_RUNNING)
    {
        return;
    }

    if (SELFTEST_TimeDue(now_ms, s_selftest.hardware.next_sample_ms) == 0u)
    {
        if (SELFTEST_TimeDue(now_ms, item->deadline_ms) != 0u)
        {
            if (s_selftest.hardware.flash_ok == 0u)
            {
                SELFTEST_FinishFail(item, now_ms, SELFTEST_FAIL_HARDWARE, "ERROR F-1"); // flash err
            }
            else if (s_selftest.hardware.rtc_ok == 0u)
            {
                SELFTEST_FinishFail(item, now_ms, SELFTEST_FAIL_HARDWARE, "ERROR R-1"); // rtc err
            }
            else
            {
                SELFTEST_FinishFail(item, now_ms, SELFTEST_FAIL_HARDWARE, "ERROR S-1"); // d err
            }
        }
        return;
    }

    s_selftest.hardware.next_sample_ms = now_ms + SELFTEST_HARDWARE_SAMPLE_PERIOD_MS;

    /* ---------------------------------------------------------------------- */
    /* SPI Flash JEDEC ID probe                                               */
    /* ---------------------------------------------------------------------- */
    if (s_selftest.hardware.flash_ok == 0u)
    {
        st = SPI_Flash_ReadJedecId(&manufacturer_id, &memory_type, &capacity_id);
        if ((st == HAL_OK) &&
            (manufacturer_id != 0x00u) &&
            (manufacturer_id != 0xFFu) &&
            (memory_type != 0x00u) &&
            (memory_type != 0xFFu) &&
            (capacity_id != 0x00u) &&
            (capacity_id != 0xFFu))
        {
            s_selftest.hardware.flash_ok = 1u;
        }
    }

    /* ---------------------------------------------------------------------- */
    /* RTC read probe                                                          */
    /* ---------------------------------------------------------------------- */
    if (s_selftest.hardware.rtc_ok == 0u)
    {
        st = HAL_RTC_GetTime(&hrtc, &rtc_time, RTC_FORMAT_BIN);
        if (st == HAL_OK)
        {
            st = HAL_RTC_GetDate(&hrtc, &rtc_date, RTC_FORMAT_BIN);
        }

        if ((st == HAL_OK) &&
            (SELFTEST_RtcCalendarLooksValid(&rtc_time, &rtc_date) != 0u))
        {
            s_selftest.hardware.rtc_ok = 1u;
        }
    }

    /* ---------------------------------------------------------------------- */
    /* SD presence / mount 판정                                               */
    /*                                                                        */
    /* 정책                                                                    */
    /* - 카드가 실제로 꽂혀 있으면 mount 성공까지 요구한다.                    */
    /* - 카드가 없으면 이 항목은 N/A 취급으로 통과시킨다.                     */
    /* ---------------------------------------------------------------------- */
    APP_STATE_CopySdSnapshot(&sd_snapshot);

    if (s_selftest.hardware.sd_presence_decided == 0u)
    {
        if ((sd_snapshot.detect_raw_present != false) ||
            (sd_snapshot.detect_stable_present != false))
        {
            s_selftest.hardware.sd_required = 1u;
            s_selftest.hardware.sd_presence_decided = 1u;
        }
        else if ((uint32_t)(now_ms - item->started_ms) >= SELFTEST_HARDWARE_SD_DECIDE_DELAY_MS)
        {
            s_selftest.hardware.sd_required = 0u;
            s_selftest.hardware.sd_presence_decided = 1u;
            s_selftest.hardware.sd_ok = 1u;
        }
    }

    if ((s_selftest.hardware.sd_required != 0u) &&
        (APP_SD_IsFsAccessAllowedNow() != false))
    {
        s_selftest.hardware.sd_ok = 1u;
    }

    item->progress_value = SELFTEST_HardwareScore();
    (void)snprintf(item->short_text,
                   sizeof(item->short_text),
                   "TEST %lu/%lu",
                   (unsigned long)item->progress_value,
                   (unsigned long)item->progress_target);

    if (item->progress_value >= item->progress_target)
    {
        if (s_selftest.hardware.sd_required != 0u)
        {
            SELFTEST_FinishPass(item, now_ms, "H/W OK");
        }
        else
        {
            SELFTEST_FinishPass(item, now_ms, "RTC/FLASH OK");
        }
        return;
    }

    if (SELFTEST_TimeDue(now_ms, item->deadline_ms) != 0u)
    {
        if (s_selftest.hardware.flash_ok == 0u)
        {
            SELFTEST_FinishFail(item, now_ms, SELFTEST_FAIL_HARDWARE, "ERROR F-2");
        }
        else if (s_selftest.hardware.rtc_ok == 0u)
        {
            SELFTEST_FinishFail(item, now_ms, SELFTEST_FAIL_HARDWARE, "ERROR R-2");
        }
        else
        {
            SELFTEST_FinishFail(item, now_ms, SELFTEST_FAIL_HARDWARE, "ERROR S-2");
        }
    }
}

/* -------------------------------------------------------------------------- */
/* 공개 API: task                                                               */
/* -------------------------------------------------------------------------- */
void SELFTEST_Task(uint32_t now_ms)
{
    if ((s_selftest.report.started == false) ||
        (s_selftest.report.finished != false))
    {
        return;
    }

    SELFTEST_TaskGps(now_ms);
    SELFTEST_TaskImu(now_ms);
    SELFTEST_TaskSensors(now_ms);
    SELFTEST_TaskHardware(now_ms);
    SELFTEST_UpdateFinishedFlag(now_ms);
}

/* -------------------------------------------------------------------------- */
/* 공개 API: running 여부                                                       */
/* -------------------------------------------------------------------------- */
bool SELFTEST_IsRunning(void)
{
    if ((s_selftest.report.started != false) &&
        (s_selftest.report.finished == false))
    {
        return true;
    }

    return false;
}

/* -------------------------------------------------------------------------- */
/* 공개 API: finished 여부                                                      */
/* -------------------------------------------------------------------------- */
bool SELFTEST_IsFinished(void)
{
    return (s_selftest.report.finished != false) ? true : false;
}

/* -------------------------------------------------------------------------- */
/* 공개 API: any_failed 여부                                                    */
/* -------------------------------------------------------------------------- */
bool SELFTEST_AnyFailed(void)
{
    return (s_selftest.report.any_failed != false) ? true : false;
}

/* -------------------------------------------------------------------------- */
/* 공개 API: fail mask getter                                                   */
/* -------------------------------------------------------------------------- */
uint32_t SELFTEST_GetFailMask(void)
{
    return s_selftest.report.fail_mask;
}

/* -------------------------------------------------------------------------- */
/* 공개 API: report copy                                                        */
/* -------------------------------------------------------------------------- */
void SELFTEST_CopyReport(selftest_report_t *out_report)
{
    if (out_report == 0)
    {
        return;
    }

    memcpy(out_report, &s_selftest.report, sizeof(*out_report));
}

/* -------------------------------------------------------------------------- */
/* 공개 API: state text helper                                                  */
/* -------------------------------------------------------------------------- */
const char *SELFTEST_GetItemStateText(selftest_item_state_t state)
{
    switch (state)
    {
    case SELFTEST_ITEM_STATE_RUNNING:
        return "TESTING";

    case SELFTEST_ITEM_STATE_PASS:
        return "OK";

    case SELFTEST_ITEM_STATE_FAIL:
        return "FAIL";

    case SELFTEST_ITEM_STATE_IDLE:
    default:
        return "IDLE";
    }
}
