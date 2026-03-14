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

/* 디스플레이 작동을 위한 헤더 */
#include "u8g2_uc1608_stm32.h"
#include "u8g2.h"

/* 디스플레이 작동을 위한 헤더 */
#include <math.h>
#include <stdio.h>


/* 데이터 창고 APP_STATE. 모든 함수는 앱스테이트를 보고 거기서 복사해 와야 한다. */
#include "APP_STATE.h"

/* 유블록스 NEO_M10 GPS 드라이버 */
#include "Ublox_GPS.h"

/* 치명 에러 발생시 디스플레이에 띄우는 핸들링을 위한 헤더 */
#include "APP_FAULT.h"

/* 새 센서 드라이버 */
#include "GY86_IMU.h"
#include "DS18B20_DRIVER.h"


#include "button.h"
#include "APP_SD.h"
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

#include "APP_FAULT_DIAG.h"

#include "FW_AppGuard.h"

#include "ui_engine.h"

#include "LED_App.h"

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


/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */


/* -------------------------------------------------------------------------- */
/*  delayed boot confirm 상태 저장 변수                                        */
/*                                                                            */
/*  s_app_boot_confirm_arm_ms                                                  */
/*  - "이 시각부터 안정화 시간을 재자" 는 기준 tick을 담는다.                 */
/*                                                                            */
/*  s_app_boot_confirm_done                                                    */
/*  - FW_AppGuard_ConfirmBootOk()를 딱 한 번만 호출하도록 막는 latch다.       */
/*                                                                            */
/*  둘 다 main.c의 USER CODE 영역에 두어 CubeMX 재생성으로 사라지지 않게 한다. */
/* -------------------------------------------------------------------------- */
static uint32_t s_app_boot_confirm_arm_ms = 0u;
static uint8_t  s_app_boot_confirm_done   = 0u;


/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
#if 0   /* legacy debug UI moved to Application2/App/Display_UI/Debug */


  static void GPS_FormatSpeedLine(char *line,

  {
      uint32_t speed_mmps_abs;
      uint32_t speed_kmh_x10;

      if ((line == 0) || (line_size == 0u) || (gps == 0))
      {
          return;
      }

      if (gps->fix.gSpeed < 0)
      {
          speed_mmps_abs = (uint32_t)(-gps->fix.gSpeed);
      }
      else
      {
          speed_mmps_abs = (uint32_t)gps->fix.gSpeed;
      }

      speed_kmh_x10 = (uint32_t)((((uint64_t)speed_mmps_abs) * 36u + 500u) / 1000u);

      snprintf(line, line_size, "SPD %ld / %lu.%luk",
               (long)gps->fix.gSpeed,
               (unsigned long)(speed_kmh_x10 / 10u),
               (unsigned long)(speed_kmh_x10 % 10u));
  }

  static const char *GPS_GetRequestedBootProfileText(app_gps_boot_profile_t profile)
  {
      switch (profile)
      {
      case APP_GPS_BOOT_PROFILE_GPS_ONLY_20HZ:
          return "GPS20";

      case APP_GPS_BOOT_PROFILE_GPS_ONLY_10HZ:
          return "GPS10";

      case APP_GPS_BOOT_PROFILE_MULTI_CONST_10HZ:
      default:
          return "MULT10";
      }
  }

  static const char *GPS_GetRequestedPowerProfileText(app_gps_power_profile_t profile)
  {
      switch (profile)
      {
      case APP_GPS_POWER_PROFILE_POWER_SAVE:
          return "SAVE";

      case APP_GPS_POWER_PROFILE_HIGH_POWER:
      default:
          return "HIGH";
      }
  }

  static const char *GPS_GetRuntimePowerProfileText(const app_gps_runtime_config_t *cfg)
  {
      if (cfg == 0)
      {
          return "----";
      }

      if ((cfg->query_complete == false) && (cfg->query_failed != false))
      {
          return "----";
      }

      if (cfg->query_complete == false)
      {
          return "QRY";
      }

      return (cfg->pm_operate_mode == 0u) ? "HIGH" : "SAVE";
  }

  static void GPS_GetRuntimeBootProfileText(const app_gps_runtime_config_t *cfg,
                                            char *out,
                                            size_t out_size)
  {
      uint8_t multi_enabled;

      if ((out == 0) || (out_size == 0u))
      {
          return;
      }

      out[0] = '\0';

      if (cfg == 0)
      {
          snprintf(out, out_size, "----");
          return;
      }

      if ((cfg->query_complete == false) && (cfg->query_failed != false))
      {
          snprintf(out, out_size, "----");
          return;
      }

      if (cfg->query_complete == false)
      {
          snprintf(out, out_size, "QRY%u/6", (unsigned)cfg->query_attempts);
          return;
      }

      multi_enabled =
          (uint8_t)((cfg->sbas_ena ? 1u : 0u) +
                    (cfg->gal_ena  ? 1u : 0u) +
                    (cfg->bds_ena  ? 1u : 0u) +
                    (cfg->qzss_ena ? 1u : 0u) +
                    (cfg->glo_ena  ? 1u : 0u));

      if ((cfg->meas_rate_ms == 50u) && (cfg->gps_ena != false) && (multi_enabled == 0u))
      {
          snprintf(out, out_size, "GPS20");
      }
      else if ((cfg->meas_rate_ms == 100u) && (cfg->gps_ena != false) && (multi_enabled == 0u))
      {
          snprintf(out, out_size, "GPS10");
      }
      else if ((cfg->meas_rate_ms == 100u) && (cfg->gps_ena != false) && (multi_enabled != 0u))
      {
          snprintf(out, out_size, "MULT10");
      }
      else
      {
          snprintf(out, out_size, "CUSTOM");
      }
  }

  static void GPS_DrawSkyPlot(u8g2_t *u8g2,
                              const app_gps_ui_snapshot_t *gps)
  {
      const int cx = 177;
      const int cy = 68;
      const int r1 = 48;
      const int r2 = 32;
      const int r3 = 16;
      uint8_t i;

      u8g2_DrawCircle(u8g2, cx, cy, r1, U8G2_DRAW_ALL);
      u8g2_DrawCircle(u8g2, cx, cy, r2, U8G2_DRAW_ALL);
      u8g2_DrawCircle(u8g2, cx, cy, r3, U8G2_DRAW_ALL);
      u8g2_DrawLine(u8g2, cx - r1, cy, cx + r1, cy);
      u8g2_DrawLine(u8g2, cx, cy - r1, cx, cy + r1);
      u8g2_DrawStr(u8g2, cx - 2, cy - r1 - 2, "N");
      u8g2_DrawStr(u8g2, cx + r1 + 3, cy + 3, "E");
      u8g2_DrawStr(u8g2, cx - 2, cy + r1 + 9, "S");
      u8g2_DrawStr(u8g2, cx - r1 - 9, cy + 3, "W");

      for (i = 0u; i < gps->nav_sat_count; i++)
      {
          const app_gps_sat_t *sat = &gps->sats[i];
          float az_rad;
          float range;
          int px;
          int py;
          char label[6];

          if (sat->visible == 0u)
          {
              continue;
          }

          if ((sat->elevation_deg < -90) || (sat->elevation_deg > 90))
          {
              continue;
          }

          az_rad = ((float)sat->azimuth_deg) * ((float)M_PI / 180.0f);
          range = ((float)(90 - sat->elevation_deg) / 90.0f) * (float)r1;
          px = (int)((float)cx + sinf(az_rad) * range);
          py = (int)((float)cy - cosf(az_rad) * range);

          if (sat->used_in_solution != 0u)
          {
              u8g2_DrawDisc(u8g2, (u8g2_uint_t)px, (u8g2_uint_t)py, 3, U8G2_DRAW_ALL);
          }
          else
          {
              u8g2_DrawCircle(u8g2, (u8g2_uint_t)px, (u8g2_uint_t)py, 3, U8G2_DRAW_ALL);
          }

          snprintf(label, sizeof(label), "%02u", (unsigned)sat->sv_id);
          u8g2_DrawStr(u8g2, (u8g2_uint_t)(px + 4), (u8g2_uint_t)(py + 3), label);
      }
  }

  static void GPS_DrawDebugPage(u8g2_t *u8g2,
                                const app_gps_ui_snapshot_t *gps,
                                const app_settings_t *settings,
                                uint32_t now_ms)
  {
      char line[40];
      char run_profile[16];
      char req_profile[24];
      uint32_t rx_age_ms;

      if ((u8g2 == 0) || (gps == 0) || (settings == 0))
      {
          return;
      }

      if (gps->rx_bytes == 0u)
      {
          rx_age_ms = 0u;
      }
      else
      {
          rx_age_ms = now_ms - gps->last_rx_ms;
      }

      u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
      u8g2_DrawStr(u8g2, 2, 8, "GPS DBG S1/BTN");

      snprintf(line, sizeof(line), "FIX %u ok:%u v:%u",
               (unsigned)gps->fix.fixType,
               gps->fix.fixOk ? 1u : 0u,
               gps->fix.valid ? 1u : 0u);
      u8g2_DrawStr(u8g2, 2, 17, line);

      snprintf(line, sizeof(line), "SV vis:%u use:%u",
               (unsigned)gps->fix.numSV_visible,
               (unsigned)gps->fix.numSV_used);
      u8g2_DrawStr(u8g2, 2, 26, line);

      snprintf(line, sizeof(line), "LAT %ld", (long)gps->fix.lat);
      u8g2_DrawStr(u8g2, 2, 35, line);

      snprintf(line, sizeof(line), "LON %ld", (long)gps->fix.lon);
      u8g2_DrawStr(u8g2, 2, 44, line);

      GPS_FormatSpeedLine(line, sizeof(line), gps);
      u8g2_DrawStr(u8g2, 2, 53, line);

      snprintf(line, sizeof(line), "ACC h%lu v%lu",
               (unsigned long)gps->fix.hAcc,
               (unsigned long)gps->fix.vAcc);
      u8g2_DrawStr(u8g2, 2, 62, line);

      snprintf(req_profile, sizeof(req_profile), "%s/%s",
               GPS_GetRequestedBootProfileText(settings->gps.boot_profile),
               GPS_GetRequestedPowerProfileText(settings->gps.power_profile));
      snprintf(line, sizeof(line), "REQ %s", req_profile);
      u8g2_DrawStr(u8g2, 2, 71, line);

      if (gps->runtime_cfg.query_complete != false)
      {
          GPS_GetRuntimeBootProfileText(&gps->runtime_cfg, run_profile, sizeof(run_profile));
          snprintf(line, sizeof(line), "RUN %s/%s",
                   run_profile,
                   GPS_GetRuntimePowerProfileText(&gps->runtime_cfg));
      }
      else if (gps->runtime_cfg.query_failed != false)
      {
          snprintf(line, sizeof(line), "RUN ----/----");
      }
      else
      {
          snprintf(line, sizeof(line), "RUN QRY%u/6",
                   (unsigned)gps->runtime_cfg.query_attempts);
      }
      u8g2_DrawStr(u8g2, 2, 80, line);

      if (gps->runtime_cfg.query_complete != false)
      {
          snprintf(line, sizeof(line), "GN G%u S%u E%u B%u Q%u R%u",
                   gps->runtime_cfg.gps_ena ? 1u : 0u,
                   gps->runtime_cfg.sbas_ena ? 1u : 0u,
                   gps->runtime_cfg.gal_ena ? 1u : 0u,
                   gps->runtime_cfg.bds_ena ? 1u : 0u,
                   gps->runtime_cfg.qzss_ena ? 1u : 0u,
                   gps->runtime_cfg.glo_ena ? 1u : 0u);
      }
      else if (gps->runtime_cfg.query_failed != false)
      {
          snprintf(line, sizeof(line), "GN ----------");
      }
      else
      {
          snprintf(line, sizeof(line), "GN querying...");
      }
      u8g2_DrawStr(u8g2, 2, 89, line);

      if (gps->runtime_cfg.query_complete != false)
      {
          snprintf(line, sizeof(line), "B%lu P%u S%u",
                   (unsigned long)gps->runtime_cfg.uart1_baudrate,
                   (unsigned)gps->runtime_cfg.msgout_nav_pvt_uart1,
                   (unsigned)gps->runtime_cfg.msgout_nav_sat_uart1);
      }
      else if (gps->runtime_cfg.query_failed != false)
      {
          snprintf(line, sizeof(line), "B---- P- S-");
      }
      else
      {
          snprintf(line, sizeof(line), "CFG %u/6",
                   (unsigned)gps->runtime_cfg.query_attempts);
      }
      u8g2_DrawStr(u8g2, 2, 98, line);

      snprintf(line, sizeof(line), "RX %lu F%lu B%lu OV%lu",
               (unsigned long)gps->rx_bytes,
               (unsigned long)gps->frames_ok,
               (unsigned long)gps->frames_bad_checksum,
               (unsigned long)gps->uart_ring_overflow_count);
      u8g2_DrawStr(u8g2, 2, 107, line);

      snprintf(line, sizeof(line), "AGE%lu R%u Q%u/%u",
               (unsigned long)rx_age_ms,
               gps->uart_rx_running ? 1u : 0u,
               (unsigned)gps->rx_ring_level,
               (unsigned)gps->rx_ring_high_watermark);
      u8g2_DrawStr(u8g2, 2, 116, line);

      snprintf(line, sizeof(line), "UE%lu O%lu F%lu N%lu P%lu",
               (unsigned long)gps->uart_error_count,
               (unsigned long)gps->uart_error_ore_count,
               (unsigned long)gps->uart_error_fe_count,
               (unsigned long)gps->uart_error_ne_count,
               (unsigned long)gps->uart_error_pe_count);
      u8g2_DrawStr(u8g2, 2, 125, line);

      u8g2_DrawFrame(u8g2, 118, 0, 122, 128);
      u8g2_DrawStr(u8g2, 140, 8, "SKY PLOT");
      GPS_DrawSkyPlot(u8g2, gps);
  }


  /* -------------------------------------------------------------------------- */
  /*  UI 페이지 상태                                                             */
  /* -------------------------------------------------------------------------- */

  typedef enum
  {
      APP_UI_PAGE_GPS         = 0u,
      APP_UI_PAGE_SENSOR      = 1u,
      APP_UI_PAGE_BRIGHTNESS  = 2u,
      APP_UI_PAGE_SD          = 3u,
      APP_UI_PAGE_BLUETOOTH   = 4u,
      APP_UI_PAGE_AUDIO       = 5u,
      APP_UI_PAGE_SPI_FLASH   = 6u,
      APP_UI_PAGE_CLOCK       = 7u,
      APP_UI_PAGE_COUNT       = 8u
  } app_ui_page_t;

  /* -------------------------------------------------------------------------- */
  /*  작은 문자열 포맷 유틸                                                      */
  /* -------------------------------------------------------------------------- */

  static void APP_FormatSignedCentiText(char *out, size_t out_size, int32_t value_x100)
  {
      uint32_t absolute_value;

      if ((out == 0) || (out_size == 0u))
      {
          return;
      }

      if (value_x100 < 0)
      {
          absolute_value = (uint32_t)(-value_x100);
          snprintf(out, out_size, "-%lu.%02lu",
                   (unsigned long)(absolute_value / 100u),
                   (unsigned long)(absolute_value % 100u));
      }
      else
      {
          absolute_value = (uint32_t)value_x100;
          snprintf(out, out_size, "%lu.%02lu",
                   (unsigned long)(absolute_value / 100u),
                   (unsigned long)(absolute_value % 100u));
      }
  }

  static void APP_DrawTextLine(u8g2_t *u8g2, uint8_t *y, const char *text)
  {
      if ((u8g2 == 0) || (y == 0) || (text == 0))
      {
          return;
      }

      u8g2_DrawStr(u8g2, 0, *y, text);
      *y = (uint8_t)(*y + 6u);
  }

  /* -------------------------------------------------------------------------- */
  /*  새 센서 디버그 페이지                                                      */
  /*                                                                            */
  /*  요구사항에 맞춰 복잡한 그래픽 없이                                         */
  /*  raw 값 + 핵심 디버그 값을 텍스트로 정갈하게 보여준다.                      */
  /* -------------------------------------------------------------------------- */
  static void APP_DrawSensorDebugPage(u8g2_t *u8g2,
                                      const app_sensor_debug_snapshot_t *sensor,
                                      uint32_t now_ms)
  {
      const app_gy86_state_t *imu;
      const app_ds18b20_state_t *ds;
      uint8_t y;
      uint32_t imu_age_ms;
      uint32_t ds_age_ms;
      char line[64];
      char imu_temp_text[16];
      char baro_temp_text[16];
      char baro_press_text[16];
      char ds_temp_text[16];

      if ((u8g2 == 0) || (sensor == 0))
      {
          return;
      }

      imu = &sensor->gy86;
      ds  = &sensor->ds18b20;

      imu_age_ms = (imu->last_update_ms == 0u) ? 0u : (now_ms - imu->last_update_ms);
      ds_age_ms  = (ds->raw.timestamp_ms == 0u) ? 0u : (now_ms - ds->raw.timestamp_ms);

      APP_FormatSignedCentiText(imu_temp_text, sizeof(imu_temp_text), imu->mpu.temp_cdeg);
      APP_FormatSignedCentiText(baro_temp_text, sizeof(baro_temp_text), imu->baro.temp_cdeg);
      APP_FormatSignedCentiText(baro_press_text, sizeof(baro_press_text), imu->baro.pressure_hpa_x100);

      if (ds->raw.temp_c_x100 == APP_DS18B20_TEMP_INVALID)
      {
          snprintf(ds_temp_text, sizeof(ds_temp_text), "---.--");
      }
      else
      {
          APP_FormatSignedCentiText(ds_temp_text, sizeof(ds_temp_text), ds->raw.temp_c_x100);
      }

      u8g2_SetFont(u8g2, u8g2_font_4x6_tr);
      y = 6u;

      APP_DrawTextLine(u8g2, &y, "SENSOR DBG S1/BTN");

      snprintf(line, sizeof(line), "GY86 init:%u det:%02X ok:%02X",
               imu->initialized ? 1u : 0u,
               (unsigned)imu->debug.detected_mask,
               (unsigned)imu->debug.init_ok_mask);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "A %6d %6d %6d",
               imu->mpu.accel_x_raw,
               imu->mpu.accel_y_raw,
               imu->mpu.accel_z_raw);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "G %6d %6d %6d",
               imu->mpu.gyro_x_raw,
               imu->mpu.gyro_y_raw,
               imu->mpu.gyro_z_raw);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "MT %sC raw:%6d",
               imu_temp_text,
               imu->mpu.temp_raw);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "M %6d %6d %6d",
               imu->mag.mag_x_raw,
               imu->mag.mag_y_raw,
               imu->mag.mag_z_raw);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "BAR %s hPa %sC",
               baro_press_text,
               baro_temp_text);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "D1:%08lu D2:%08lu",
               (unsigned long)imu->baro.d1_raw,
               (unsigned long)imu->baro.d2_raw);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "C1:%u C2:%u C3:%u",
               (unsigned)imu->baro.prom_c[1],
               (unsigned)imu->baro.prom_c[2],
               (unsigned)imu->baro.prom_c[3]);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "C4:%u C5:%u C6:%u",
               (unsigned)imu->baro.prom_c[4],
               (unsigned)imu->baro.prom_c[5],
               (unsigned)imu->baro.prom_c[6]);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "Pms m:%u g:%u b:%u st:%u",
               (unsigned)imu->debug.mpu_poll_period_ms,
               (unsigned)imu->debug.mag_poll_period_ms,
               (unsigned)imu->debug.baro_poll_period_ms,
               (unsigned)imu->debug.ms5611_state);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "E m:%lu g:%lu b:%lu",
               (unsigned long)imu->debug.mpu_error_count,
               (unsigned long)imu->debug.mag_error_count,
               (unsigned long)imu->debug.baro_error_count);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "DS init:%u st:%02X ph:%u er:%u",
               ds->initialized ? 1u : 0u,
               (unsigned)ds->status_flags,
               (unsigned)ds->debug.phase,
               (unsigned)ds->debug.last_error);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "ROM %02X %02X %02X %02X",
               (unsigned)ds->raw.rom_code[0],
               (unsigned)ds->raw.rom_code[1],
               (unsigned)ds->raw.rom_code[2],
               (unsigned)ds->raw.rom_code[3]);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "    %02X %02X %02X %02X",
               (unsigned)ds->raw.rom_code[4],
               (unsigned)ds->raw.rom_code[5],
               (unsigned)ds->raw.rom_code[6],
               (unsigned)ds->raw.rom_code[7]);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "TMP %sC raw:%6d r:%u",
               ds_temp_text,
               ds->raw.raw_temp_lsb,
               (unsigned)ds->raw.resolution_bits);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "CFG %02X TH:%d TL:%d",
               (unsigned)ds->raw.config_reg,
               (int)ds->raw.alarm_high_c,
               (int)ds->raw.alarm_low_c);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "SP %02X %02X %02X %02X %02X",
               (unsigned)ds->raw.scratchpad[0],
               (unsigned)ds->raw.scratchpad[1],
               (unsigned)ds->raw.scratchpad[2],
               (unsigned)ds->raw.scratchpad[3],
               (unsigned)ds->raw.scratchpad[4]);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "   %02X %02X %02X %02X",
               (unsigned)ds->raw.scratchpad[5],
               (unsigned)ds->raw.scratchpad[6],
               (unsigned)ds->raw.scratchpad[7],
               (unsigned)ds->raw.scratchpad[8]);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "CNT cv:%lu rd:%lu tf:%lu",
               (unsigned long)ds->debug.conversion_start_count,
               (unsigned long)ds->debug.read_complete_count,
               (unsigned long)ds->debug.transaction_fail_count);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "AGE i:%lu d:%lu crc:%lu us:%lu",
               (unsigned long)imu_age_ms,
               (unsigned long)ds_age_ms,
               (unsigned long)ds->debug.crc_fail_count,
               (unsigned long)ds->debug.last_transaction_us);
      APP_DrawTextLine(u8g2, &y, line);
  }

  #ifndef APP_BOARD_PAGE_BUTTON_DEBOUNCE_MS
  #define APP_BOARD_PAGE_BUTTON_DEBOUNCE_MS 150u
  #endif

  /* -------------------------------------------------------------------------- */
  /*  SD 페이지 표시용 작은 텍스트 helper                                         */
  /* -------------------------------------------------------------------------- */

  static uint32_t APP_BytesToMiB(uint64_t bytes)
  {
      return (uint32_t)(bytes / (1024ull * 1024ull));
  }

  static const char *APP_GetSdFsTypeText(uint8_t fs_type, bool fat_valid)
  {
      if (fat_valid == false)
      {
          return "----";
      }

      switch (fs_type)
      {
      case FS_FAT12:
          return "F12";

      case FS_FAT16:
          return "F16";

      case FS_FAT32:
          return "F32";

  #ifdef FS_EXFAT
      case FS_EXFAT:
          return "EXF";
  #endif

      default:
          return "RAW";
      }
  }

  static const char *APP_GetSdFresultText(uint32_t fres_raw)
  {
      if (fres_raw == 0xFFFFFFFFu)
      {
          return "--";
      }

      switch ((FRESULT)fres_raw)
      {
      case FR_OK:
          return "OK";

      case FR_DISK_ERR:
          return "DSK";

      case FR_INT_ERR:
          return "INT";

      case FR_NOT_READY:
          return "NRDY";

      case FR_NO_FILE:
          return "NOF";

      case FR_NO_PATH:
          return "NOP";

      case FR_INVALID_NAME:
          return "INV";

      case FR_DENIED:
          return "DEN";

      case FR_EXIST:
          return "EXT";

      case FR_INVALID_OBJECT:
          return "OBJ";

      case FR_WRITE_PROTECTED:
          return "WPR";

      case FR_INVALID_DRIVE:
          return "DRV";

      case FR_NOT_ENABLED:
          return "OFF";

      case FR_NO_FILESYSTEM:
          return "NOFS";

      case FR_MKFS_ABORTED:
          return "MKF";

      case FR_TIMEOUT:
          return "TMO";

      case FR_LOCKED:
          return "LCK";

      case FR_NOT_ENOUGH_CORE:
          return "MEM";

      case FR_TOO_MANY_OPEN_FILES:
          return "MANY";

      case FR_INVALID_PARAMETER:
          return "PAR";

      default:
          return "UKN";
      }
  }

  static const char *APP_GetSdTransferStateText(uint8_t transfer_state)
  {
      if (transfer_state == 0xFFu)
      {
          return "--";
      }

      if (transfer_state == SD_TRANSFER_OK)
      {
          return "OK";
      }

      if (transfer_state == SD_TRANSFER_BUSY)
      {
          return "BUSY";
      }

      return "RAW";
  }

  static const char *APP_GetSdCardTypeText(uint8_t card_type, bool initialized)
  {
      if (initialized == false)
      {
          return "----";
      }

  #if defined(CARD_SDSC)
      if (card_type == CARD_SDSC)
      {
          return "SDSC";
      }
  #endif

  #if defined(CARD_SDHC_SDXC)
      if (card_type == CARD_SDHC_SDXC)
      {
          return "SDHCX";
      }
  #endif

  #if defined(CARD_SECURED)
      if (card_type == CARD_SECURED)
      {
          return "SECU";
      }
  #endif

      return "RAW";
  }

  static char APP_GetSdSampleTypeChar(uint8_t sample_type)
  {
      switch (sample_type)
      {
      case 2u:
          return 'D';

      case 1u:
          return 'F';

      case 0u:
      default:
          return '-';
      }
  }

  /* -------------------------------------------------------------------------- */
  /*  SD 디버그 페이지                                                           */
  /*                                                                            */
  /*  화면 제약 때문에 카드/FAT의 "핵심 메타데이터 + 최근 디버그 결과" 를         */
  /*  20줄 안쪽의 고밀도 텍스트로 정리한다.                                      */
  /* -------------------------------------------------------------------------- */
  static void APP_DrawSdDebugPage(u8g2_t *u8g2,
                                  const app_sd_state_t *sd,
                                  uint32_t now_ms)
  {
      uint8_t y;
      uint32_t detect_age_ms;
      char line[64];

      if ((u8g2 == 0) || (sd == 0))
      {
          return;
      }

      detect_age_ms = (sd->last_detect_change_ms == 0u) ?
                      0u :
                      (now_ms - sd->last_detect_change_ms);

      u8g2_SetFont(u8g2, u8g2_font_4x6_tr);
      y = 6u;

      APP_DrawTextLine(u8g2, &y, "SD DBG S1/BTN");

      snprintf(line, sizeof(line), "DET r:%u s:%u db:%u irq:%lu",
               sd->detect_raw_present ? 1u : 0u,
               sd->detect_stable_present ? 1u : 0u,
               sd->detect_debounce_pending ? 1u : 0u,
               (unsigned long)sd->detect_irq_count);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "EV in:%lu out:%lu age:%lu",
               (unsigned long)sd->detect_insert_count,
               (unsigned long)sd->detect_remove_count,
               (unsigned long)detect_age_ms);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "MNT i:%u m:%u fv:%u f32:%u",
               sd->initialized ? 1u : 0u,
               sd->mounted ? 1u : 0u,
               sd->fat_valid ? 1u : 0u,
               sd->is_fat32 ? 1u : 0u);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "ATT ok:%lu ng:%lu um:%lu",
               (unsigned long)sd->mount_success_count,
               (unsigned long)sd->mount_fail_count,
               (unsigned long)sd->unmount_count);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "FR m:%s g:%s r:%s",
               APP_GetSdFresultText(sd->last_mount_fresult),
               APP_GetSdFresultText(sd->last_getfree_fresult),
               APP_GetSdFresultText(sd->last_root_scan_fresult));
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "HAL s:%u t:%s c:%08lX",
               (unsigned)sd->hal_state,
               APP_GetSdTransferStateText(sd->transfer_state),
               (unsigned long)sd->hal_context);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "ERR:%08lX RCA:%04lX",
               (unsigned long)sd->hal_error_code,
               (unsigned long)sd->rel_card_add);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "CARD %s v:%u c:%u",
               APP_GetSdCardTypeText(sd->card_type, sd->initialized),
               (unsigned)sd->card_version,
               (unsigned)sd->card_class);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "PHY %lu x %lu",
               (unsigned long)sd->block_nbr,
               (unsigned long)sd->block_size);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "LOG %lu x %lu",
               (unsigned long)sd->log_block_nbr,
               (unsigned long)sd->log_block_size);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "CAP %luMiB TOT %luMiB",
               (unsigned long)APP_BytesToMiB(sd->capacity_bytes),
               (unsigned long)APP_BytesToMiB(sd->total_bytes));
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "FS %s SPC:%lu FAT:%lu",
               APP_GetSdFsTypeText(sd->fs_type, sd->fat_valid),
               (unsigned long)sd->sectors_per_cluster,
               (unsigned long)sd->sectors_per_fat);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "FREE %luMiB CL:%lu",
               (unsigned long)APP_BytesToMiB(sd->free_bytes),
               (unsigned long)sd->free_clusters);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "SEC vb:%lu fb:%lu",
               (unsigned long)sd->volume_start_sector,
               (unsigned long)sd->fat_start_sector);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "RAW dirb:%lu db:%lu",
               (unsigned long)sd->root_dir_base,
               (unsigned long)sd->data_start_sector);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "ROOT e:%lu f:%lu d:%lu",
               (unsigned long)sd->root_entry_count,
               (unsigned long)sd->root_file_count,
               (unsigned long)sd->root_dir_count);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "N1 %c:%s",
               APP_GetSdSampleTypeChar(sd->root_entry_sample_type[0]),
               (sd->root_entry_sample_name[0][0] != '\0') ? sd->root_entry_sample_name[0] : "-");
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "N2 %c:%s",
               APP_GetSdSampleTypeChar(sd->root_entry_sample_type[1]),
               (sd->root_entry_sample_name[1][0] != '\0') ? sd->root_entry_sample_name[1] : "-");
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "N3 %c:%s",
               APP_GetSdSampleTypeChar(sd->root_entry_sample_type[2]),
               (sd->root_entry_sample_name[2][0] != '\0') ? sd->root_entry_sample_name[2] : "-");
      APP_DrawTextLine(u8g2, &y, line);
  }

  /* -------------------------------------------------------------------------- */
  /*  바이트 배열을 짧은 hex 문자열로 바꾸는 helper                               */
  /* -------------------------------------------------------------------------- */
  static void APP_HexBytesToText(const uint8_t *data,
                                 uint32_t length,
                                 char *out_text,
                                 size_t out_size)
  {
      uint32_t index;
      size_t offset;
      int written;

      if ((out_text == 0) || (out_size == 0u))
      {
          return;
      }

      out_text[0] = '\0';

      if ((data == 0) || (length == 0u))
      {
          return;
      }

      offset = 0u;

      for (index = 0u; index < length; index++)
      {
          if (offset >= (out_size - 1u))
          {
              break;
          }

          written = snprintf(&out_text[offset],
                             out_size - offset,
                             (index + 1u < length) ? "%02X " : "%02X",
                             data[index]);
          if (written <= 0)
          {
              break;
          }

          if ((size_t)written >= (out_size - offset))
          {
              offset = out_size - 1u;
              break;
          }

          offset += (size_t)written;
      }
  }

  /* -------------------------------------------------------------------------- */
  /*  CDS 밝기 센서 디버그 페이지                                                */
  /* -------------------------------------------------------------------------- */
  static void APP_DrawBrightnessDebugPage(u8g2_t *u8g2,
                                          const app_brightness_state_t *brightness,
                                          uint32_t now_ms)
  {
      uint8_t y;
      uint32_t age_ms;
      char line[64];

      if ((u8g2 == 0) || (brightness == 0))
      {
          return;
      }

      age_ms = (brightness->last_update_ms == 0u) ?
               0u :
               (now_ms - brightness->last_update_ms);

      u8g2_SetFont(u8g2, u8g2_font_4x6_tr);
      y = 6u;

      APP_DrawTextLine(u8g2, &y, "CDS DBG S1/NEXT");

      snprintf(line, sizeof(line), "INIT:%u VALID:%u AGE:%lu",
               brightness->initialized ? 1u : 0u,
               brightness->valid ? 1u : 0u,
               (unsigned long)age_ms);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "RAW L:%u A:%u",
               (unsigned)brightness->raw_last,
               (unsigned)brightness->raw_average);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "RAW MIN:%u MAX:%u",
               (unsigned)brightness->raw_min,
               (unsigned)brightness->raw_max);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "CAL:%u MV:%lu",
               (unsigned)brightness->calibrated_counts,
               (unsigned long)brightness->voltage_mv);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "NORM:%u PER:%u%%",
               (unsigned)brightness->normalized_permille,
               (unsigned)brightness->brightness_percent);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "PER:%lu AVG:%u TO:%u",
               (unsigned long)brightness->debug.period_ms,
               (unsigned)brightness->debug.average_count,
               (unsigned)brightness->debug.adc_timeout_ms);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "SMPL:%lu OFF:%ld",
               (unsigned long)brightness->debug.sampling_time,
               (long)brightness->debug.calibration_offset_counts);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "GAIN:%lu/%lu",
               (unsigned long)brightness->debug.calibration_gain_num,
               (unsigned long)brightness->debug.calibration_gain_den);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "0%%:%u 100%%:%u",
               (unsigned)brightness->debug.calibration_raw_0_percent,
               (unsigned)brightness->debug.calibration_raw_100_percent);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "CNT:%lu FAIL:%lu",
               (unsigned long)brightness->sample_count,
               (unsigned long)brightness->debug.read_fail_count);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "HAL:%u ERR:%lu",
               (unsigned)brightness->debug.last_hal_status,
               (unsigned long)brightness->debug.hal_error_count);
      APP_DrawTextLine(u8g2, &y, line);
  }



  /* -------------------------------------------------------------------------- */
  /*  Audio mode text helper                                                    */
  /* -------------------------------------------------------------------------- */
  static const char *APP_GetAudioModeText(uint8_t mode)
  {
      switch ((app_audio_mode_t)mode)
      {
      case APP_AUDIO_MODE_TONE:
          return "TONE";

      case APP_AUDIO_MODE_SEQUENCE_MIX:
          return "MIX";

      case APP_AUDIO_MODE_SEQUENCE_MONO:
          return "MONO";

      case APP_AUDIO_MODE_WAV_FILE:
          return "WAV";

      case APP_AUDIO_MODE_IDLE:
      default:
          return "IDLE";
      }
  }

  /* -------------------------------------------------------------------------- */
  /*  Audio debug page                                                          */
  /*                                                                            */
  /*  이 페이지는                                                               */
  /*    - transport(TIM6 + DAC DMA) 상태                                         */
  /*    - 마지막 render block level                                              */
  /*    - sequence BPM                                                           */
  /*    - WAV source 메타데이터                                                  */
  /*    - voice 4개 raw 상태                                                     */
  /*  를 최대한 그대로 보여준다.                                                 */
  /* -------------------------------------------------------------------------- */
  static void APP_DrawAudioDebugPage(u8g2_t *u8g2,
                                     const app_audio_state_t *audio,
                                     uint32_t now_ms)
  {
      uint8_t y;
      uint32_t age_ms;
      uint32_t voice_index;
      char line[96];

      if ((u8g2 == 0) || (audio == 0))
      {
          return;
      }

      age_ms = (audio->last_update_ms == 0u) ?
               0u :
               (now_ms - audio->last_update_ms);

      u8g2_SetFont(u8g2, u8g2_font_4x6_tr);
      y = 6u;

      APP_DrawTextLine(u8g2, &y, "AUDIO DBG S1/2/3/4/5/6");

      snprintf(line, sizeof(line), "INIT:%u TR:%u ACT:%u WAV:%u",
               audio->initialized ? 1u : 0u,
               audio->transport_running ? 1u : 0u,
               audio->content_active ? 1u : 0u,
               audio->wav_active ? 1u : 0u);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "SR:%luHz OUT:%ubit VOL:%u%%",
               (unsigned long)audio->sample_rate_hz,
               (unsigned)audio->output_resolution_bits,
               (unsigned)audio->volume_percent);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "DMA:%u/%u FIFO:%lu/%lu",
               (unsigned)audio->dma_buffer_sample_count,
               (unsigned)audio->dma_half_buffer_sample_count,
               (unsigned long)audio->sw_fifo_level_samples,
               (unsigned long)audio->sw_fifo_capacity_samples);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "WM:%lu/%lu PK:%lu",
               (unsigned long)audio->sw_fifo_low_watermark_samples,
               (unsigned long)audio->sw_fifo_high_watermark_samples,
               (unsigned long)audio->sw_fifo_peak_level_samples);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "HAL D:%u T:%u UND:%lu",
               (unsigned)audio->last_hal_status_dac,
               (unsigned)audio->last_hal_status_tim,
               (unsigned long)audio->dma_underrun_count);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "CB H:%lu F:%lu SRV:%lu",
               (unsigned long)audio->half_callback_count,
               (unsigned long)audio->full_callback_count,
               (unsigned long)audio->dma_service_half_count);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "BLK:%lu PROD:%lu STV:%lu",
               (unsigned long)audio->render_block_count,
               (unsigned long)audio->producer_refill_block_count,
               (unsigned long)audio->fifo_starvation_count);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "AGE:%lu MODE:%s V:%u",
               (unsigned long)age_ms,
               APP_GetAudioModeText(audio->mode),
               (unsigned)audio->active_voice_count);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "LVL:%u..%u CLP:%lu/%u",
               (unsigned)audio->last_block_min_u12,
               (unsigned)audio->last_block_max_u12,
               (unsigned long)audio->clip_block_count,
               (unsigned)audio->last_block_clipped);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "SIL:%lu BPM:%lu",
               (unsigned long)audio->silence_injected_sample_count,
               (unsigned long)audio->sequence_bpm);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "NAME:%s",
               (audio->current_name[0] != '\0') ? audio->current_name : "-");
      APP_DrawTextLine(u8g2, &y, line);

      if ((audio->wav_active != false) || (audio->wav_source_sample_rate_hz != 0u))
      {
          snprintf(line, sizeof(line), "WAV %luHz %uch %ubit %s",
                   (unsigned long)audio->wav_source_sample_rate_hz,
                   (unsigned)audio->wav_source_channels,
                   (unsigned)audio->wav_source_bits_per_sample,
                   (audio->wav_native_rate_active != 0u) ? "NAT" : "RSM");
      }
      else
      {
          snprintf(line, sizeof(line), "WAV:-");
      }
      APP_DrawTextLine(u8g2, &y, line);

      for (voice_index = 0u; voice_index < APP_AUDIO_MAX_VOICES; voice_index++)
      {
          const app_audio_voice_state_t *voice;

          voice = &audio->voices[voice_index];

          snprintf(line, sizeof(line), "V%lu A%u W%u E%u %lu.%02luHz",
                   (unsigned long)(voice_index + 1u),
                   voice->active ? 1u : 0u,
                   (unsigned)voice->waveform_id,
                   (unsigned)voice->env_phase,
                   (unsigned long)(voice->note_hz_x100 / 100u),
                   (unsigned long)(voice->note_hz_x100 % 100u));
          APP_DrawTextLine(u8g2, &y, line);
      }

      APP_DrawTextLine(u8g2, &y, "B2 SINE440  B3 SQR440");
      APP_DrawTextLine(u8g2, &y, "B4 SAW440   B5 BOOT4CH");
      APP_DrawTextLine(u8g2, &y, "B6 ROOT WAV");
  }













  /* -------------------------------------------------------------------------- */
  /*  Bluetooth bring-up / 통신 시험 페이지                                      */
  /*                                                                            */
  /*  이 페이지의 목적                                                            */
  /*  - BC417 계열 classic Bluetooth 모듈이                                      */
  /*    실제로 UART<->무선 링크처럼 동작하는지 현장에서 바로 확인                */
  /*  - 최근 RX/TX 줄, ring 수위, error count, echo/auto ping 상태를             */
  /*    작은 화면에서 빠르게 확인                                                */
  /*  - 버튼만으로도 여러 종류의 송신 시험을 수행                                */
  /*                                                                            */
  /*  권장 시험 순서                                                              */
  /*  1) 휴대폰/PC에서 블루투스 페어링                                           */
  /*  2) serial terminal 앱/프로그램으로 접속                                    */
  /*  3) 휴대폰/PC에서 "PING" / "INFO" / "ECHO OFF" 등을 보내 반응 확인       */
  /*  4) 기기 버튼 BUTTON2~BUTTON6으로 반대 방향 송신도 확인                     */
  /* -------------------------------------------------------------------------- */
  static void APP_DrawBluetoothDebugPage(u8g2_t *u8g2,
                                         const app_bluetooth_state_t *bluetooth,
                                         uint32_t now_ms)
  {
      uint8_t y;
      uint32_t rx_age_ms;
      uint32_t tx_age_ms;
      char line[96];

      if ((u8g2 == 0) || (bluetooth == 0))
      {
          return;
      }

      rx_age_ms = (bluetooth->last_rx_ms == 0u) ?
                  0u :
                  (now_ms - bluetooth->last_rx_ms);

      tx_age_ms = (bluetooth->last_tx_ms == 0u) ?
                  0u :
                  (now_ms - bluetooth->last_tx_ms);

      u8g2_SetFont(u8g2, u8g2_font_4x6_tr);
      y = 6u;

      APP_DrawTextLine(u8g2, &y, "BT DBG S1/2/3/4/5/6");
      APP_DrawTextLine(u8g2, &y, "MOD BC417 CLASSIC SPP");

      snprintf(line, sizeof(line), "INIT:%u RXRUN:%u E:%u A:%u",
               bluetooth->initialized ? 1u : 0u,
               bluetooth->uart_rx_running ? 1u : 0u,
               bluetooth->echo_enabled ? 1u : 0u,
               bluetooth->auto_ping_enabled ? 1u : 0u);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "AGE RX:%lu TX:%lu AP:%lu",
               (unsigned long)rx_age_ms,
               (unsigned long)tx_age_ms,
               (unsigned long)((bluetooth->last_auto_ping_ms == 0u) ? 0u : (now_ms - bluetooth->last_auto_ping_ms)));
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "CNT RX:%lu TX:%lu",
               (unsigned long)bluetooth->rx_bytes,
               (unsigned long)bluetooth->tx_bytes);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "LIN RX:%lu TX:%lu",
               (unsigned long)bluetooth->rx_line_count,
               (unsigned long)bluetooth->tx_line_count);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "RNG %u/%u",
               (unsigned)bluetooth->rx_ring_level,
               (unsigned)bluetooth->rx_ring_high_watermark);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "ERR OV:%lu RE:%lu TXF:%lu",
               (unsigned long)bluetooth->rx_overflow_count,
               (unsigned long)bluetooth->uart_rearm_fail_count,
               (unsigned long)bluetooth->uart_tx_fail_count);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "HAL RX:%u TX:%u ER:%u",
               (unsigned)bluetooth->last_hal_status_rx,
               (unsigned)bluetooth->last_hal_status_tx,
               (unsigned)bluetooth->last_hal_error);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "LAST RX:%s",
               (bluetooth->last_rx_line[0] != '\0') ? bluetooth->last_rx_line : "-");
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "PREV   :%s",
               (bluetooth->rx_preview[0] != '\0') ? bluetooth->rx_preview : "-");
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "LAST TX:%s",
               (bluetooth->last_tx_line[0] != '\0') ? bluetooth->last_tx_line : "-");
      APP_DrawTextLine(u8g2, &y, line);

      APP_DrawTextLine(u8g2, &y, "B2 PING  B3 HELLO");
      APP_DrawTextLine(u8g2, &y, "B4 INFO  B5 NMEA");
      APP_DrawTextLine(u8g2, &y, "B6 short AUTO");
      APP_DrawTextLine(u8g2, &y, "B6 long  ECHO");
      APP_DrawTextLine(u8g2, &y, "RX CMD: PING INFO");
      APP_DrawTextLine(u8g2, &y, "RX CMD: ECHO/AUTO ON OFF");
  }

  /* -------------------------------------------------------------------------- */
  /*  SPI Flash 전용 디버그 / 테스트 페이지                                       */
  /* -------------------------------------------------------------------------- */
  static void APP_DrawSpiFlashDebugPage(u8g2_t *u8g2,
                                        const spi_flash_snapshot_t *flash)
  {
      uint8_t y;
      char line[96];
      char hex_text[96];

      if ((u8g2 == 0) || (flash == 0))
      {
          return;
      }

      u8g2_SetFont(u8g2, u8g2_font_4x6_tr);
      y = 6u;

      APP_DrawTextLine(u8g2, &y, "FLASH DBG S1/2R/3W");

      snprintf(line, sizeof(line), "DOC:%s", flash->doc_part_name);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "NAME:%s", flash->part_name);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "ID:%02X %02X %02X MID:%04X",
               (unsigned)flash->manufacturer_id,
               (unsigned)flash->memory_type,
               (unsigned)flash->capacity_id,
               (unsigned)flash->manufacturer_device_id);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "PRS:%u BV:%u CAP:%luKiB",
               flash->present ? 1u : 0u,
               flash->w25q16bv_compatible ? 1u : 0u,
               (unsigned long)(flash->capacity_bytes / 1024u));
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "SR1:%02X SR2:%02X SR3:%02X",
               (unsigned)flash->status_reg1,
               (unsigned)flash->status_reg2,
               (unsigned)flash->status_reg3);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "BUSY:%u WEL:%u HAL:%u",
               flash->busy ? 1u : 0u,
               flash->write_enable_latch ? 1u : 0u,
               (unsigned)flash->last_hal_status);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "TST:%s RES:%s",
               SPI_Flash_GetTestStateText((spi_flash_test_state_t)flash->test_state),
               SPI_Flash_GetResultText((spi_flash_result_t)flash->last_result));
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "ADDR:%06lX LEN:%u CMD:%lu",
               (unsigned long)flash->test_address,
               (unsigned)flash->last_test_length,
               (unsigned long)flash->command_count);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "CNT R:%lu W:%lu E:%lu",
               (unsigned long)flash->read_count,
               (unsigned long)flash->write_count,
               (unsigned long)flash->erase_count);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "ERR:%lu LASTOP:%02X",
               (unsigned long)flash->error_count,
               (unsigned)flash->last_opcode);
      APP_DrawTextLine(u8g2, &y, line);

      APP_HexBytesToText(flash->unique_id, 8u, hex_text, sizeof(hex_text));
      snprintf(line, sizeof(line), "UID:%s", hex_text);
      APP_DrawTextLine(u8g2, &y, line);

      APP_HexBytesToText(flash->sfdp_header, 8u, hex_text, sizeof(hex_text));
      snprintf(line, sizeof(line), "SFDP:%s", hex_text);
      APP_DrawTextLine(u8g2, &y, line);

      APP_HexBytesToText(flash->last_read_data, 8u, hex_text, sizeof(hex_text));
      snprintf(line, sizeof(line), "RD:%s", hex_text);
      APP_DrawTextLine(u8g2, &y, line);

      APP_HexBytesToText(flash->last_write_data, 8u, hex_text, sizeof(hex_text));
      snprintf(line, sizeof(line), "WR:%s", hex_text);
      APP_DrawTextLine(u8g2, &y, line);
  }


  /* -------------------------------------------------------------------------- */
  /*  CLOCK TEST 페이지 helper                                                   */
  /*                                                                            */
  /*  역할                                                                      */
  /*  - RTC / timezone / GPS sync 상태를 한 화면에서 검증한다.                   */
  /*  - 수동 시간 설정 edit buffer를 main.c 안에 작게 두고, 실제 반영은           */
  /*    APP_CLOCK API로 넘긴다.                                                 */
  /*  - 하드웨어 접근은 절대 main.c가 직접 하지 않고,                           */
  /*    APP_STATE snapshot + APP_CLOCK service만 사용한다.                      */
  /* -------------------------------------------------------------------------- */

  typedef enum
  {
      APP_CLOCK_EDIT_FIELD_YEAR  = 0u,
      APP_CLOCK_EDIT_FIELD_MONTH = 1u,
      APP_CLOCK_EDIT_FIELD_DAY   = 2u,
      APP_CLOCK_EDIT_FIELD_HOUR  = 3u,
      APP_CLOCK_EDIT_FIELD_MIN   = 4u,
      APP_CLOCK_EDIT_FIELD_SEC   = 5u,
      APP_CLOCK_EDIT_FIELD_TZ    = 6u,
      APP_CLOCK_EDIT_FIELD_COUNT = 7u
  } app_clock_edit_field_t;

  typedef struct
  {
      bool                 active;              /* edit mode 진입 여부                 */
      uint8_t              selected_field;      /* 현재 편집 중인 필드                 */
      app_clock_calendar_t local;               /* 사용자가 만지는 local date/time     */
      int8_t               timezone_quarters;   /* 사용자가 만지는 timezone(15분 단위) */
  } app_clock_edit_state_t;

  static app_clock_state_t g_clock_snapshot;
  static app_clock_edit_state_t g_clock_edit;

  /* -------------------------------------------------------------------------- */
  /*  아래 helper 함수들이 g_ui_page를 참조하므로,                                */
  /*  main.c 뒤쪽의 실제 초기화 정의보다 앞에서 tentative declaration을 하나 둔다. */
  /* -------------------------------------------------------------------------- */
  static volatile app_ui_page_t g_ui_page;

  /* -------------------------------------------------------------------------- */
  /*  월별 날짜 수 helper                                                       */
  /*                                                                            */
  /*  수동 시간 편집에서 day wrap/clamp 계산에만 사용한다.                       */
  /* -------------------------------------------------------------------------- */
  static uint8_t APP_ClockDaysInMonth(uint16_t year, uint8_t month)
  {
      static const uint8_t s_days[12] =
      {
          31u, 28u, 31u, 30u, 31u, 30u, 31u, 31u, 30u, 31u, 30u, 31u
      };

      if ((month == 0u) || (month > 12u))
      {
          return 31u;
      }

      if ((month == 2u) &&
          ((((year % 400u) == 0u) || (((year % 4u) == 0u) && ((year % 100u) != 0u)))))
      {
          return 29u;
      }

      return s_days[month - 1u];
  }

  /* -------------------------------------------------------------------------- */
  /*  편집 버퍼의 day를 month/year 범위 안으로 조정                              */
  /* -------------------------------------------------------------------------- */
  static void APP_ClockEditClampDay(void)
  {
      uint8_t max_day;

      max_day = APP_ClockDaysInMonth(g_clock_edit.local.year, g_clock_edit.local.month);

      if (g_clock_edit.local.day == 0u)
      {
          g_clock_edit.local.day = 1u;
      }

      if (g_clock_edit.local.day > max_day)
      {
          g_clock_edit.local.day = max_day;
      }
  }

  /* -------------------------------------------------------------------------- */
  /*  편집 버퍼 weekday 재계산                                                   */
  /* -------------------------------------------------------------------------- */
  static void APP_ClockEditRecomputeWeekday(void)
  {
      APP_ClockEditClampDay();
      g_clock_edit.local.weekday = APP_CLOCK_ComputeWeekday(g_clock_edit.local.year,
                                                            g_clock_edit.local.month,
                                                            g_clock_edit.local.day);
  }

  /* -------------------------------------------------------------------------- */
  /*  weekday를 3글자 약어로 변환                                                */
  /* -------------------------------------------------------------------------- */
  static const char *APP_ClockWeekdayText(uint8_t weekday)
  {
      switch (weekday)
      {
      case 1u:
          return "MON";

      case 2u:
          return "TUE";

      case 3u:
          return "WED";

      case 4u:
          return "THU";

      case 5u:
          return "FRI";

      case 6u:
          return "SAT";

      case 7u:
          return "SUN";

      default:
          return "---";
      }
  }

  /* -------------------------------------------------------------------------- */
  /*  마지막 sync source를 짧은 문자열로 변환                                    */
  /* -------------------------------------------------------------------------- */
  static const char *APP_ClockSyncSourceText(uint8_t source)
  {
      switch ((app_clock_sync_source_t)source)
      {
      case APP_CLOCK_SYNC_SOURCE_BOOT_DEFAULT:
          return "BOOT";

      case APP_CLOCK_SYNC_SOURCE_MANUAL:
          return "MAN";

      case APP_CLOCK_SYNC_SOURCE_GPS_FULL:
          return "GPSF";

      case APP_CLOCK_SYNC_SOURCE_GPS_PERIODIC:
          return "GPST";

      case APP_CLOCK_SYNC_SOURCE_EXTERNAL_STUB:
          return "EXT";

      case APP_CLOCK_SYNC_SOURCE_NONE:
      default:
          return "NONE";
      }
  }

  /* -------------------------------------------------------------------------- */
  /*  편집 중인 필드 이름 반환                                                   */
  /* -------------------------------------------------------------------------- */
  static const char *APP_ClockEditFieldText(uint8_t field)
  {
      switch ((app_clock_edit_field_t)field)
      {
      case APP_CLOCK_EDIT_FIELD_YEAR:
          return "YEAR";

      case APP_CLOCK_EDIT_FIELD_MONTH:
          return "MONTH";

      case APP_CLOCK_EDIT_FIELD_DAY:
          return "DAY";

      case APP_CLOCK_EDIT_FIELD_HOUR:
          return "HOUR";

      case APP_CLOCK_EDIT_FIELD_MIN:
          return "MIN";

      case APP_CLOCK_EDIT_FIELD_SEC:
          return "SEC";

      case APP_CLOCK_EDIT_FIELD_TZ:
          return "TZ";

      case APP_CLOCK_EDIT_FIELD_COUNT:
      default:
          return "?";
      }
  }

  /* -------------------------------------------------------------------------- */
  /*  snapshot으로부터 edit buffer 초기화                                        */
  /*                                                                            */
  /*  RTC가 valid하면 현재 local time을 복사하고,                                */
  /*  그렇지 않으면 안전한 기본값(2026-01-01 00:00:00, KST)을 채운다.            */
  /* -------------------------------------------------------------------------- */
  static void APP_ClockEditLoadFromSnapshot(const app_clock_state_t *clock)
  {
      memset((void *)&g_clock_edit, 0, sizeof(g_clock_edit));

      g_clock_edit.active = true;
      g_clock_edit.selected_field = (uint8_t)APP_CLOCK_EDIT_FIELD_YEAR;

      if ((clock != 0) &&
          (clock->rtc_time_valid != false) &&
          (APP_CLOCK_ValidateCalendar(&clock->local) != false))
      {
          g_clock_edit.local = clock->local;
          g_clock_edit.timezone_quarters =
              APP_CLOCK_ClampTimezoneQuarters((int32_t)clock->timezone_quarters);
      }
      else
      {
          g_clock_edit.local.year = 2026u;
          g_clock_edit.local.month = 1u;
          g_clock_edit.local.day = 1u;
          g_clock_edit.local.hour = 0u;
          g_clock_edit.local.min = 0u;
          g_clock_edit.local.sec = 0u;
          g_clock_edit.timezone_quarters = APP_CLOCK_TIMEZONE_QUARTERS_DEFAULT;
      }

      APP_ClockEditRecomputeWeekday();
  }

  /* -------------------------------------------------------------------------- */
  /*  편집 버퍼에서 현재 선택 필드 값 변경                                       */
  /*                                                                            */
  /*  모든 증감은 wrap 방식으로 처리한다.                                        */
  /*  timezone은 15분 단위로 한 칸씩 움직인다.                                   */
  /* -------------------------------------------------------------------------- */
  static void APP_ClockEditAdjustField(int32_t delta)
  {
      if (delta == 0)
      {
          return;
      }

      switch ((app_clock_edit_field_t)g_clock_edit.selected_field)
      {
      case APP_CLOCK_EDIT_FIELD_YEAR:
          if (delta > 0)
          {
              g_clock_edit.local.year =
                  (g_clock_edit.local.year >= 2099u) ? 2000u : (uint16_t)(g_clock_edit.local.year + 1u);
          }
          else
          {
              g_clock_edit.local.year =
                  (g_clock_edit.local.year <= 2000u) ? 2099u : (uint16_t)(g_clock_edit.local.year - 1u);
          }
          break;

      case APP_CLOCK_EDIT_FIELD_MONTH:
          if (delta > 0)
          {
              g_clock_edit.local.month = (g_clock_edit.local.month >= 12u) ? 1u : (uint8_t)(g_clock_edit.local.month + 1u);
          }
          else
          {
              g_clock_edit.local.month = (g_clock_edit.local.month <= 1u) ? 12u : (uint8_t)(g_clock_edit.local.month - 1u);
          }
          break;

      case APP_CLOCK_EDIT_FIELD_DAY:
      {
          uint8_t max_day = APP_ClockDaysInMonth(g_clock_edit.local.year, g_clock_edit.local.month);

          if (delta > 0)
          {
              g_clock_edit.local.day = (g_clock_edit.local.day >= max_day) ? 1u : (uint8_t)(g_clock_edit.local.day + 1u);
          }
          else
          {
              g_clock_edit.local.day = (g_clock_edit.local.day <= 1u) ? max_day : (uint8_t)(g_clock_edit.local.day - 1u);
          }
          break;
      }

      case APP_CLOCK_EDIT_FIELD_HOUR:
          if (delta > 0)
          {
              g_clock_edit.local.hour = (g_clock_edit.local.hour >= 23u) ? 0u : (uint8_t)(g_clock_edit.local.hour + 1u);
          }
          else
          {
              g_clock_edit.local.hour = (g_clock_edit.local.hour == 0u) ? 23u : (uint8_t)(g_clock_edit.local.hour - 1u);
          }
          break;

      case APP_CLOCK_EDIT_FIELD_MIN:
          if (delta > 0)
          {
              g_clock_edit.local.min = (g_clock_edit.local.min >= 59u) ? 0u : (uint8_t)(g_clock_edit.local.min + 1u);
          }
          else
          {
              g_clock_edit.local.min = (g_clock_edit.local.min == 0u) ? 59u : (uint8_t)(g_clock_edit.local.min - 1u);
          }
          break;

      case APP_CLOCK_EDIT_FIELD_SEC:
          if (delta > 0)
          {
              g_clock_edit.local.sec = (g_clock_edit.local.sec >= 59u) ? 0u : (uint8_t)(g_clock_edit.local.sec + 1u);
          }
          else
          {
              g_clock_edit.local.sec = (g_clock_edit.local.sec == 0u) ? 59u : (uint8_t)(g_clock_edit.local.sec - 1u);
          }
          break;

      case APP_CLOCK_EDIT_FIELD_TZ:
      {
          int32_t timezone_quarters;

          timezone_quarters = (int32_t)g_clock_edit.timezone_quarters + ((delta > 0) ? 1 : -1);
          if (timezone_quarters > APP_CLOCK_TIMEZONE_QUARTERS_MAX)
          {
              timezone_quarters = APP_CLOCK_TIMEZONE_QUARTERS_MIN;
          }
          else if (timezone_quarters < APP_CLOCK_TIMEZONE_QUARTERS_MIN)
          {
              timezone_quarters = APP_CLOCK_TIMEZONE_QUARTERS_MAX;
          }

          g_clock_edit.timezone_quarters = (int8_t)timezone_quarters;
          break;
      }

      case APP_CLOCK_EDIT_FIELD_COUNT:
      default:
          break;
      }

      APP_ClockEditRecomputeWeekday();
  }

  /* -------------------------------------------------------------------------- */
  /*  CLOCK TEST 페이지 전용 버튼 처리                                           */
  /*                                                                            */
  /*  view mode                                                                  */
  /*    B2 short : GPS full sync 즉시 수행                                       */
  /*    B3 short : GPS time-only sync 즉시 수행                                  */
  /*    B4 short : manual edit mode 진입                                         */
  /*    B5 short : GPS auto sync 토글                                            */
  /*    B6 short : RTC valid 플래그를 강제로 false                               */
  /*                                                                            */
  /*  edit mode                                                                  */
  /*    B2 short : 이전 필드 선택                                                */
  /*    B3 short : 다음 필드 선택                                                */
  /*    B4 short : 현재 필드 -1                                                  */
  /*    B5 short : 현재 필드 +1                                                  */
  /*    B6 short : 수동 설정 저장                                                */
  /*    B6 long  : 편집 취소                                                     */
  /* -------------------------------------------------------------------------- */
  static bool APP_HandleClockPageButtonEvent(const button_event_t *event, uint32_t now_ms)
  {
      if (event == 0)
      {
          return false;
      }

      if (g_ui_page != APP_UI_PAGE_CLOCK)
      {
          return false;
      }

      if (g_clock_edit.active != false)
      {
          if (event->type == BUTTON_EVENT_SHORT_PRESS)
          {
              if (event->id == BUTTON_ID_2)
              {
                  if (g_clock_edit.selected_field == 0u)
                  {
                      g_clock_edit.selected_field = (uint8_t)(APP_CLOCK_EDIT_FIELD_COUNT - 1u);
                  }
                  else
                  {
                      g_clock_edit.selected_field--;
                  }
                  return true;
              }
              else if (event->id == BUTTON_ID_3)
              {
                  g_clock_edit.selected_field =
                      (uint8_t)((g_clock_edit.selected_field + 1u) % (uint8_t)APP_CLOCK_EDIT_FIELD_COUNT);
                  return true;
              }
              else if (event->id == BUTTON_ID_4)
              {
                  APP_ClockEditAdjustField(-1);
                  return true;
              }
              else if (event->id == BUTTON_ID_5)
              {
                  APP_ClockEditAdjustField(+1);
                  return true;
              }
              else if (event->id == BUTTON_ID_6)
              {
                  if (APP_CLOCK_SetManualLocalTime(&g_clock_edit.local,
                                                   g_clock_edit.timezone_quarters,
                                                   now_ms) != false)
                  {
                      g_clock_edit.active = false;
                      APP_CLOCK_ForceRefresh(now_ms);
                  }
                  return true;
              }
          }
          else if ((event->type == BUTTON_EVENT_LONG_PRESS) &&
                   (event->id == BUTTON_ID_6))
          {
              g_clock_edit.active = false;
              return true;
          }

          return false;
      }

      if (event->type != BUTTON_EVENT_SHORT_PRESS)
      {
          return false;
      }

      if (event->id == BUTTON_ID_2)
      {
          (void)APP_CLOCK_RequestGpsFullSyncNow(now_ms);
          APP_CLOCK_ForceRefresh(now_ms);
          return true;
      }
      else if (event->id == BUTTON_ID_3)
      {
          (void)APP_CLOCK_RequestGpsTimeOnlySyncNow(now_ms);
          APP_CLOCK_ForceRefresh(now_ms);
          return true;
      }
      else if (event->id == BUTTON_ID_4)
      {
          APP_STATE_CopyClockSnapshot(&g_clock_snapshot);
          APP_ClockEditLoadFromSnapshot(&g_clock_snapshot);
          return true;
      }
      else if (event->id == BUTTON_ID_5)
      {
          APP_STATE_CopyClockSnapshot(&g_clock_snapshot);
          APP_CLOCK_SetAutoGpsSyncEnabled((g_clock_snapshot.gps_auto_sync_enabled_runtime == false), now_ms);
          APP_CLOCK_ForceRefresh(now_ms);
          return true;
      }
      else if (event->id == BUTTON_ID_6)
      {
          APP_CLOCK_MarkTimeInvalid(now_ms);
          APP_CLOCK_ForceRefresh(now_ms);
          return true;
      }

      return false;
  }

  /* -------------------------------------------------------------------------- */
  /*  CLOCK TEST 페이지 draw                                                    */
  /*                                                                            */
  /*  목적                                                                      */
  /*  - local / UTC / raw RTC / valid 플래그 / GPS sync 카운터를 한 화면에서 본다. */
  /*  - edit mode에서는 사용자가 조정 중인 버퍼를 그대로 보여준다.               */
  /* -------------------------------------------------------------------------- */
  static void APP_DrawClockDebugPage(u8g2_t *u8g2,
                                     const app_clock_state_t *clock,
                                     uint32_t now_ms)
  {
      uint8_t y;
      char line[64];
      char tz_text[16];
      uint32_t gps_sync_age_ms;

      if ((u8g2 == 0) || (clock == 0))
      {
          return;
      }

      u8g2_SetFont(u8g2, u8g2_font_4x6_tr);
      y = 6u;

      APP_DrawTextLine(u8g2, &y, "CLOCK TEST S1/NEXT");

      if (g_clock_edit.active != false)
      {
          APP_CLOCK_FormatUtcOffsetText(g_clock_edit.timezone_quarters, tz_text, sizeof(tz_text));

          snprintf(line, sizeof(line), "EDT %04u-%02u-%02u %02u:%02u:%02u",
                   (unsigned)g_clock_edit.local.year,
                   (unsigned)g_clock_edit.local.month,
                   (unsigned)g_clock_edit.local.day,
                   (unsigned)g_clock_edit.local.hour,
                   (unsigned)g_clock_edit.local.min,
                   (unsigned)g_clock_edit.local.sec);
          APP_DrawTextLine(u8g2, &y, line);

          snprintf(line, sizeof(line), "SEL:%s TZ:%s WD:%s",
                   APP_ClockEditFieldText(g_clock_edit.selected_field),
                   tz_text,
                   APP_ClockWeekdayText(g_clock_edit.local.weekday));
          APP_DrawTextLine(u8g2, &y, line);
      }
      else
      {
          APP_CLOCK_FormatUtcOffsetText(clock->timezone_quarters, tz_text, sizeof(tz_text));

          if ((clock->rtc_time_valid != false) &&
              (APP_CLOCK_ValidateCalendar(&clock->local) != false))
          {
              snprintf(line, sizeof(line), "LCL %04u-%02u-%02u %02u:%02u:%02u",
                       (unsigned)clock->local.year,
                       (unsigned)clock->local.month,
                       (unsigned)clock->local.day,
                       (unsigned)clock->local.hour,
                       (unsigned)clock->local.min,
                       (unsigned)clock->local.sec);
          }
          else
          {
              snprintf(line, sizeof(line), "LCL ---- -- -- --:--:--");
          }
          APP_DrawTextLine(u8g2, &y, line);

          snprintf(line, sizeof(line), "WD:%s TZ:%s VAL:%u RD:%u",
                   APP_ClockWeekdayText(clock->local.weekday),
                   tz_text,
                   clock->rtc_time_valid ? 1u : 0u,
                   clock->rtc_read_valid ? 1u : 0u);
          APP_DrawTextLine(u8g2, &y, line);
      }

      if (APP_CLOCK_ValidateCalendar(&clock->utc) != false)
      {
          snprintf(line, sizeof(line), "UTC %04u-%02u-%02u %02u:%02u:%02u",
                   (unsigned)clock->utc.year,
                   (unsigned)clock->utc.month,
                   (unsigned)clock->utc.day,
                   (unsigned)clock->utc.hour,
                   (unsigned)clock->utc.min,
                   (unsigned)clock->utc.sec);
      }
      else
      {
          snprintf(line, sizeof(line), "UTC ---- -- -- --:--:--");
      }
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "SRC:%s AUTO:%u INT:%uM",
               APP_ClockSyncSourceText(clock->last_sync_source),
               clock->gps_auto_sync_enabled_runtime ? 1u : 0u,
               (unsigned)clock->gps_sync_interval_minutes);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "RAW D:%02u/%02u/%02u WD:%u",
               (unsigned)clock->rtc_date_raw.year_2digit,
               (unsigned)clock->rtc_date_raw.month,
               (unsigned)clock->rtc_date_raw.date,
               (unsigned)clock->rtc_date_raw.week_day);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "RAW T:%02u:%02u:%02u HAL:%lu",
               (unsigned)clock->rtc_time_raw.hours,
               (unsigned)clock->rtc_time_raw.minutes,
               (unsigned)clock->rtc_time_raw.seconds,
               (unsigned long)clock->last_hal_status);
      APP_DrawTextLine(u8g2, &y, line);

      gps_sync_age_ms = (clock->last_gps_sync_ms == 0u) ? 0u : (now_ms - clock->last_gps_sync_ms);
      snprintf(line, sizeof(line), "GPS C:%u RES:%u AGE:%lums",
               clock->gps_candidate_valid ? 1u : 0u,
               clock->gps_resolved_seen ? 1u : 0u,
               (unsigned long)gps_sync_age_ms);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "CNT F:%lu P:%lu M:%lu",
               (unsigned long)clock->gps_full_sync_count,
               (unsigned long)clock->gps_periodic_sync_count,
               (unsigned long)clock->manual_set_count);
      APP_DrawTextLine(u8g2, &y, line);

      snprintf(line, sizeof(line), "ERR RD:%lu WR:%lu E:%lu",
               (unsigned long)clock->rtc_read_count,
               (unsigned long)clock->rtc_write_count,
               (unsigned long)clock->rtc_error_count);
      APP_DrawTextLine(u8g2, &y, line);

      if (APP_CLOCK_ValidateCalendar(&clock->last_gps_utc) != false)
      {
          snprintf(line, sizeof(line), "GPS %04u-%02u-%02u %02u:%02u:%02u",
                   (unsigned)clock->last_gps_utc.year,
                   (unsigned)clock->last_gps_utc.month,
                   (unsigned)clock->last_gps_utc.day,
                   (unsigned)clock->last_gps_utc.hour,
                   (unsigned)clock->last_gps_utc.min,
                   (unsigned)clock->last_gps_utc.sec);
      }
      else
      {
          snprintf(line, sizeof(line), "GPS ---- -- -- --:--:--");
      }
      APP_DrawTextLine(u8g2, &y, line);

      if (g_clock_edit.active != false)
      {
          APP_DrawTextLine(u8g2, &y, "B2< FIELD  B3> FIELD");
          APP_DrawTextLine(u8g2, &y, "B4 DEC    B5 INC");
          APP_DrawTextLine(u8g2, &y, "B6 SAVE   B6L CANCEL");
      }
      else
      {
          APP_DrawTextLine(u8g2, &y, "B2 GPS FULL");
          APP_DrawTextLine(u8g2, &y, "B3 GPS TIME");
          APP_DrawTextLine(u8g2, &y, "B4 EDIT  B5 AUTO");
          APP_DrawTextLine(u8g2, &y, "B6 INVALID");
      }
  }

  /* -------------------------------------------------------------------------- */
  /*  UI가 쓰는 snapshot                                                          */
  /*                                                                            */
  /*  g_gps_snapshot 은 이제 app_gps_state_t 전체가 아니라                        */
  /*  app_gps_ui_snapshot_t 경량 구조체를 사용한다.                               */
  /*                                                                            */
  /*  나머지 페이지는 현재 "디버그 페이지" 성격이 강하므로                        */
  /*  필요한 raw 필드를 거의 그대로 본다.                                        */
  /*  다만 render 자체는 page별 최소 갱신 주기를 두어                              */
  /*  "frame token이 올 때마다 무조건 다시 그림" 오버헤드를 줄인다.               */
  /* -------------------------------------------------------------------------- */
  static app_gps_ui_snapshot_t       g_gps_snapshot;
  static app_sensor_debug_snapshot_t g_sensor_snapshot;
  static app_brightness_state_t      g_brightness_snapshot;
  static app_audio_state_t           g_audio_snapshot;
  static app_bluetooth_state_t       g_bluetooth_snapshot;
  static app_sd_state_t              g_sd_snapshot;
  static spi_flash_snapshot_t        g_spi_flash_snapshot;
  static app_settings_t              g_settings_snapshot;

  #ifndef APP_UI_GPS_MIN_REFRESH_MS
  #define APP_UI_GPS_MIN_REFRESH_MS          100u
  #endif

  #ifndef APP_UI_SENSOR_MIN_REFRESH_MS
  #define APP_UI_SENSOR_MIN_REFRESH_MS       100u
  #endif

  #ifndef APP_UI_BRIGHTNESS_MIN_REFRESH_MS
  #define APP_UI_BRIGHTNESS_MIN_REFRESH_MS   100u
  #endif

  #ifndef APP_UI_SD_MIN_REFRESH_MS
  #define APP_UI_SD_MIN_REFRESH_MS           250u
  #endif

  #ifndef APP_UI_BLUETOOTH_MIN_REFRESH_MS
  #define APP_UI_BLUETOOTH_MIN_REFRESH_MS    100u
  #endif

  #ifndef APP_UI_AUDIO_MIN_REFRESH_MS
  #define APP_UI_AUDIO_MIN_REFRESH_MS        100u
  #endif

  #ifndef APP_UI_SPI_FLASH_MIN_REFRESH_MS
  #define APP_UI_SPI_FLASH_MIN_REFRESH_MS    200u
  #endif

  #ifndef APP_UI_CLOCK_MIN_REFRESH_MS
  #define APP_UI_CLOCK_MIN_REFRESH_MS        200u
  #endif

  static volatile app_ui_page_t      g_ui_page = APP_UI_PAGE_GPS;
  static volatile uint32_t           g_ui_last_page_button_ms = 0u;
  static uint32_t                    g_last_ui_draw_ms = 0u;
  static uint32_t                    g_last_button_pressed_mask = 0u;
  static uint8_t                     g_ui_force_redraw = 1u;

  static void APP_RequestUiRedraw(void)
  {
      g_ui_force_redraw = 1u;
  }

  static uint32_t APP_GetUiMinRefreshMs(app_ui_page_t page)
  {
      switch (page)
      {
      case APP_UI_PAGE_SENSOR:
          return APP_UI_SENSOR_MIN_REFRESH_MS;

      case APP_UI_PAGE_BRIGHTNESS:
          return APP_UI_BRIGHTNESS_MIN_REFRESH_MS;

      case APP_UI_PAGE_SD:
          return APP_UI_SD_MIN_REFRESH_MS;

      case APP_UI_PAGE_BLUETOOTH:
          return APP_UI_BLUETOOTH_MIN_REFRESH_MS;

      case APP_UI_PAGE_AUDIO:
          return APP_UI_AUDIO_MIN_REFRESH_MS;

      case APP_UI_PAGE_SPI_FLASH:
          return APP_UI_SPI_FLASH_MIN_REFRESH_MS;

      case APP_UI_PAGE_CLOCK:
          return APP_UI_CLOCK_MIN_REFRESH_MS;

      case APP_UI_PAGE_GPS:
      default:
          return APP_UI_GPS_MIN_REFRESH_MS;
      }
  }

  /* -------------------------------------------------------------------------- */
  /*  UI 페이지 전환 helper                                                      */
  /* -------------------------------------------------------------------------- */

  static void APP_SelectNextUiPage(void)
  {
      uint32_t next_page_index;

      next_page_index = (uint32_t)g_ui_page + 1u;
      if (next_page_index >= (uint32_t)APP_UI_PAGE_COUNT)
      {
          next_page_index = (uint32_t)APP_UI_PAGE_GPS;
      }

      g_ui_page = (app_ui_page_t)next_page_index;
      g_clock_edit.active = false;

      /* ---------------------------------------------------------------------- */
      /*  페이지가 바뀌면 다음 frame token을 잡는 즉시 새 페이지를 그리게 한다.   */
      /* ---------------------------------------------------------------------- */
      APP_RequestUiRedraw();
  }


  static void APP_HandleBoardPageButtonIrq(uint32_t now_ms)
  {
      /* ---------------------------------------------------------------------- */
      /*  보드 위 단일 디버그 버튼은 IRQ에서 바로 페이지를 넘기되,                 */
      /*  bounce에 의해 여러 번 들어오는 것을 시간 필터로 차단한다.               */
      /*                                                                        */
      /*  회로의 active level 정보가 현재 main.c 밖으로 추상화되어 있지 않으므로   */
      /*  BUTTON1~6처럼 state machine을 따로 두기보다                             */
      /*  "한 번 accepted 되면 N ms 동안 재수락하지 않음" 구조를 쓴다.            */
      /* ---------------------------------------------------------------------- */
      if ((uint32_t)(now_ms - g_ui_last_page_button_ms) < APP_BOARD_PAGE_BUTTON_DEBOUNCE_MS)
      {
          return;
      }

      g_ui_last_page_button_ms = now_ms;
      APP_SelectNextUiPage();
  }


  /* -------------------------------------------------------------------------- */
  /*  버튼 이벤트 표시용 상태 + helper                                             */
  /*                                                                            */
  /*  요구사항                                                                  */
  /*  - 화면 오른쪽 위에는 현재 눌리고 있는 버튼 번호열을 표시한다.               */
  /*  - 그 바로 아래 줄에는 가장 최근에 확정된 버튼 이벤트를 표시한다.          */
  /*                                                                            */
  /*  실제 앱 로직은 APP_HandleButtonEvents() 안의 while(event pop) 구간에서     */
  /*  event 기반으로 추가하면 된다.                                             */
  /* -------------------------------------------------------------------------- */
  static button_event_t           g_last_button_event;

  static void APP_HandleButtonEvents(void)
  {
      button_event_t event;
      uint32_t now_ms;

      while (Button_PopEvent(&event) != false)
      {
          now_ms = HAL_GetTick();
          g_last_button_event = event;

          /* ------------------------------------------------------------------ */
          /*  최근 이벤트 HUD는 버튼 이벤트가 확정되는 즉시 바뀌어야 하므로        */
          /*  event를 하나 소비할 때마다 다음 UI redraw를 즉시 허용한다.          */
          /* ------------------------------------------------------------------ */
          APP_RequestUiRedraw();

          /* ------------------------------------------------------------------ */
          /*  BUTTON1 short press 는 UI 페이지를 다음 페이지로 넘긴다.           */
          /* ------------------------------------------------------------------ */
          if ((event.id == BUTTON_ID_1) &&
              (event.type == BUTTON_EVENT_SHORT_PRESS))
          {
              APP_SelectNextUiPage();
              continue;
          }

          /* ------------------------------------------------------------------ */
          /*  CLOCK TEST 페이지 전용 버튼 매핑은 별도 helper로 먼저 처리한다.    */
          /* ------------------------------------------------------------------ */
          if (APP_HandleClockPageButtonEvent(&event, now_ms) != false)
          {
              continue;
          }

          /* ------------------------------------------------------------------ */
          /*  Bluetooth 페이지 버튼 매핑                                         */
          /*                                                                    */
          /*  short press                                                        */
          /*    B2 : PING                                                        */
          /*    B3 : HELLO                                                       */
          /*    B4 : INFO                                                        */
          /*    B5 : NMEA demo                                                   */
          /*    B6 : auto ping toggle                                            */
          /*                                                                    */
          /*  long press                                                         */
          /*    B6 : echo toggle                                                 */
          /* ------------------------------------------------------------------ */
          if (g_ui_page == APP_UI_PAGE_BLUETOOTH)
          {
              if (event.type == BUTTON_EVENT_SHORT_PRESS)
              {
                  if (event.id == BUTTON_ID_2)
                  {
                      Bluetooth_DoSomethingPing();
                      continue;
                  }
                  else if (event.id == BUTTON_ID_3)
                  {
                      Bluetooth_DoSomethingHello();
                      continue;
                  }
                  else if (event.id == BUTTON_ID_4)
                  {
                      Bluetooth_DoSomethingInfo();
                      continue;
                  }
                  else if (event.id == BUTTON_ID_5)
                  {
                      Bluetooth_DoSomethingDemoNmea();
                      continue;
                  }
                  else if (event.id == BUTTON_ID_6)
                  {
                      Bluetooth_ToggleAutoPing();
                      continue;
                  }
              }
              else if ((event.type == BUTTON_EVENT_LONG_PRESS) &&
                       (event.id == BUTTON_ID_6))
              {
                  Bluetooth_ToggleEcho();
                  continue;
              }
          }

          /* ------------------------------------------------------------------ */
          /*  Audio 페이지 버튼 매핑                                             */
          /*                                                                    */
          /*  short press                                                        */
          /*    B2 : sine 440Hz test                                             */
          /*    B3 : square 440Hz test                                           */
          /*    B4 : saw 440Hz test                                              */
          /*    B5 : 4채널 boot sequence                                         */
          /*    B6 : SD root에서 아무 WAV 하나                                    */
          /* ------------------------------------------------------------------ */
          if ((g_ui_page == APP_UI_PAGE_AUDIO) &&
              (event.type == BUTTON_EVENT_SHORT_PRESS))
          {
              if (event.id == BUTTON_ID_2)
              {
                  Audio_App_DoSomething1();
                  continue;
              }
              else if (event.id == BUTTON_ID_3)
              {
                  Audio_App_DoSomething2();
                  continue;
              }
              else if (event.id == BUTTON_ID_4)
              {
                  Audio_App_DoSomething3();
                  continue;
              }
              else if (event.id == BUTTON_ID_5)
              {
                  Audio_App_DoSomething4();
                  continue;
              }
              else if (event.id == BUTTON_ID_6)
              {
                  Audio_App_DoSomething5();
                  continue;
              }
          }


          /* ------------------------------------------------------------------ */
          /*  SPI Flash 페이지에서는                                            */
          /*    - BUTTON2 short press : read test 요청                           */
          /*    - BUTTON3 short press : write test 요청                          */
          /*  으로 사용한다.                                                    */
          /* ------------------------------------------------------------------ */
          if ((g_ui_page == APP_UI_PAGE_SPI_FLASH) &&
              (event.type == BUTTON_EVENT_SHORT_PRESS))
          {
              if (event.id == BUTTON_ID_2)
              {
                  SPI_Flash_RequestReadTest();
              }
              else if (event.id == BUTTON_ID_3)
              {
                  SPI_Flash_RequestWriteTest();
              }
          }
      }
  }


  static void APP_DrawButtonOverlay(u8g2_t *u8g2)
  {
      char pressed_text[8];
      char event_text[24];
      uint16_t display_width;
      uint16_t text_width;
      uint16_t x;

      if (u8g2 == 0)
      {
          return;
      }

      /* ---------------------------------------------------------------------- */
      /*  1줄째: 현재 눌리고 있는 버튼 번호열                                     */
      /*  2줄째: 가장 최근에 확정된 이벤트                                        */
      /*                                                                        */
      /*  둘 다 오른쪽 정렬로 그려서                                             */
      /*  "오른쪽 위 상태 HUD" 처럼 보이게 만든다.                               */
      /* ---------------------------------------------------------------------- */
      Button_BuildPressedDigits(pressed_text, sizeof(pressed_text));
      Button_BuildEventText(&g_last_button_event, event_text, sizeof(event_text));

      u8g2_SetFont(u8g2, u8g2_font_4x6_tr);
      display_width = u8g2_GetDisplayWidth(u8g2);

      text_width = u8g2_GetStrWidth(u8g2, pressed_text);
      x = (text_width >= display_width) ? 0u : (uint16_t)(display_width - text_width - 1u);
      u8g2_DrawStr(u8g2, x, 6u, pressed_text);

      text_width = u8g2_GetStrWidth(u8g2, event_text);
      x = (text_width >= display_width) ? 0u : (uint16_t)(display_width - text_width - 1u);
      u8g2_DrawStr(u8g2, x, 12u, event_text);
  }

#endif  /* legacy debug UI moved to Application2/App/Display_UI/Debug */


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
      UI_Engine_OnFrameTickFromISR();
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
  /* USER CODE BEGIN 2 */

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




  APP_STATE_Init();

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
  /*  새 UI 엔진 초기화                                                        */
  /*                                                                          */
  /*  - 버튼 초기화가 끝난 뒤 현재 눌림 마스크를 안전하게 읽을 수 있다.         */
  /*  - 여기서는 엔진의 내부 상태/하단바/토스트/팝업/legacy debug state를      */
  /*    초기화만 하고, 실제 LCD draw는 아직 하지 않는다.                       */
  /* ------------------------------------------------------------------------ */
  UI_Engine_Init();


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
  UI_Engine_EarlyBootDraw();

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
   s_app_boot_confirm_arm_ms = HAL_GetTick();
   s_app_boot_confirm_done = 0u;

   /* -------------------------------------------------------------------- */
   /*  첫 화면은 다음 frame token을 잡는 즉시 바로 한 번 그리게 한다.         */
   /* -------------------------------------------------------------------- */
   UI_Engine_RequestRedraw();





  /* USER CODE END 2 */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */

    /* USER CODE BEGIN 3 */
	  uint32_t now_ms;

	        now_ms = HAL_GetTick();
	        FW_AppGuard_Kick();

	        /* ------------------------------------------------------------------ */
	        /*  SD hotplug / mount / unmount / FAT 메타데이터 갱신                 */
	        /*                                                                    */
	        /*  이번 안정화에서는 APP_SD_Task를 Audio_Driver_Task보다 앞에 둔다.   */
	        /*                                                                    */
	        /*  이유                                                               */
	        /*  - DET edge가 방금 들어온 경우                                       */
	        /*    APP_SD가 먼저 debounce / stable change / teardown을 정리해야     */
	        /*    오디오가 stale SD 세션을 다시 건드릴 확률이 더 낮아진다.          */
	        /* ------------------------------------------------------------------ */
	        APP_SD_Task(now_ms);

	        /* ------------------------------------------------------------------ */
	        /*  오디오는 이 시스템에서 deadline이 가장 빡빡한 축에 속하므로           */
	        /*  가능한 한 loop의 앞쪽에서 producer refill을 수행한다.               */
	        /*                                                                    */
	        /*  이제 실제 deadline은 DMA ISR이 맡고,                                 */
	        /*  여기서는 software FIFO를 high watermark 근처까지 다시 채우는         */
	        /*  producer 역할만 한다.                                               */
	        /* ------------------------------------------------------------------ */
	        Audio_Driver_Task(now_ms);
	        Audio_App_Task(now_ms);

	        /* GPS parser / state maintenance */
	        Ublox_GPS_Task(now_ms);

	        /* ------------------------------------------------------------------ */
	        /*  RTC / timezone / GPS sync policy service                           */
	        /* ------------------------------------------------------------------ */
	        APP_CLOCK_Task(now_ms);

	        /* ------------------------------------------------------------------ */
	        /*  Bluetooth classic SPP bring-up / line parser / echo / auto ping    */
	        /* ------------------------------------------------------------------ */
	        Bluetooth_Task(now_ms);

	        /* ------------------------------------------------------------------ */
	        /*  DEBUG UART 로그 큐도 main loop에서 가볍게 한 번 더 kick 해서         */
	        /*  HAL busy/error 후 멈춘 TX를 복구한다.                                */
	        /* ------------------------------------------------------------------ */
	        DEBUG_UART_Task(now_ms);

	        /* 새 센서 드라이버들 */
	        GY86_IMU_Task(now_ms);
	        DS18B20_DRIVER_Task(now_ms);
	        Brightness_Sensor_Task(now_ms);

	        /* SPI flash는 상태 머신 기반이라 loop마다 한 번씩만 진전시킨다. */
	        SPI_Flash_Task(now_ms);

	        /* ------------------------------------------------------------------ */
	        /*  버튼 debounce / long-press 판정                                     */
	        /*                                                                    */
	        /*  실제 event 소비와 UI 화면 전환, 하단바 press invert,                 */
	        /*  popup/toast, legacy debug dispatch는 이제 UI 엔진이 맡는다.          */
	        /* ------------------------------------------------------------------ */
	        Button_Task(now_ms);

	        /* ------------------------------------------------------------------ */
	        /*  새 UI 엔진 task                                                     */
	        /* ------------------------------------------------------------------ */
	        UI_Engine_Task(now_ms);

	        /* ------------------------------------------------------------------ */
	        /*  LED App task                                                       */
	        /*                                                                    */
	        /*  main loop는 이 함수를 매번 호출한다.                               */
	        /*                                                                    */
	        /*  실제 frame rate 제한(17ms 간격, 60fps 이하 cap),                   */
	        /*  perceptual brightness remap, target/current frame smoothing,       */
	        /*  PWM compare 갱신은 내부 LED_Driver가 맡는다.                       */
	        /*                                                                    */
	        /*  상위 app / UI 계층은 LED_Off(), LED_BreathIdle(),                  */
	        /*  LED_App_SetTestPattern() 같은 mode setter만 호출하면 된다.         */
	        /* ------------------------------------------------------------------ */
	        LED_App_Task(now_ms);

	        /* ------------------------------------------------------------------ */


	                /*  delayed boot confirm                                                */
	                /*                                                                    */
	                /*  이 블록은 "실제 서비스 loop가 일정 시간 동안 살아남았다" 는 사실을 */
	                /*  본 뒤에만 boot confirmed를 세운다.                                 */
	                /*                                                                    */
	                /*  판정 조건                                                          */
	                /*  - 아직 confirm을 한 번도 하지 않았고                               */
	                /*  - USER CODE BEGIN 2에서 arm한 시각으로부터                          */
	                /*    APP_BOOT_CONFIRM_DELAY_MS 이상 지났다면                           */
	                /*    여기서 FW_AppGuard_ConfirmBootOk()를 1회 호출한다.               */
	                /*                                                                    */
	                /*  배치 위치를 loop 하단에 둔 이유                                     */
	                /*  - SD / Audio / GPS / Clock / Bluetooth / Sensor / UI task 등        */
	                /*    핵심 bring-up 이후에도 실제 task 구동이 문제 없이 이어졌는지를    */
	                /*    가능한 한 많이 본 뒤에 확정하기 위해서다.                         */
	                /* ------------------------------------------------------------------ */
	                if ((s_app_boot_confirm_done == 0u) &&
	                    ((uint32_t)(now_ms - s_app_boot_confirm_arm_ms) >= APP_BOOT_CONFIRM_DELAY_MS))
	                {
	                    FW_AppGuard_ConfirmBootOk();
	                    s_app_boot_confirm_done = 1u;
	                }

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
  UI_Engine_OnBoardDebugButtonIrq(now_ms);
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
