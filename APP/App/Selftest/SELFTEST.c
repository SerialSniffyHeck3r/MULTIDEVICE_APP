#include "SELFTEST.h"

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include <stdlib.h>

#include "main.h"
#include "rtc.h"
#include "fatfs.h"
#include "ff.h"

/* 앱에서 이미 공개하고 있는 상위 레벨 상태/드라이버 API만 사용한다. */
#include "APP_STATE.h"
#include "APP_SD.h"
#include "Ublox_GPS.h"
#include "GY86_IMU.h"
#include "Bluetooth.h"
#include "SPI_Flash.h"
#include "button.h"
#include "Audio_Driver.h"
#include "Audio_App.h"
#include "LED_App.h"
#include "BACKLIGHT_App.h"
#include "FW_AppGuard.h"
#include "u8g2_uc1608_stm32.h"

/* -------------------------------------------------------------------------- */
/* APP_STATE snapshot copy helper forward declarations                         */
/*                                                                            */
/* 일부 파일 배치에서는 APP_STATE.h 내부 선언이 include 순서에 민감할 수 있어  */
/* 여기서 동일 시그니처를 한 번 더 선언한다.                                  */
/*                                                                            */
/* 이 선언은 "새 low-level 함수 추가"가 아니라,                               */
/* 이미 기존 quick self-test가 사용하던 공개 snapshot API를 그대로 재사용하는 */
/* 목적이다.                                                                   */
/* -------------------------------------------------------------------------- */
void APP_STATE_CopyGpsSnapshot(app_gps_state_t *dst);
void APP_STATE_CopyGy86Snapshot(app_gy86_state_t *dst);

/* -------------------------------------------------------------------------- */
/* 로컬 상수                                                                   */
/*                                                                            */
/* Boot/APP 주소는 bootloader 쪽 FW_BootConfig.h 계약과 맞춘다.               */
/* - bootloader : 0x08000000 ~ 0x0801FFFF (128 KB)                            */
/* - main app   : 0x08020000 ~ flash end                                      */
/*                                                                            */
/* app 쪽 프로젝트에서는 bootloader 헤더를 직접 include 하지 않고,             */
/* 유지보수 self-test 전용 읽기 상수로만 중복 정의한다.                        */
/* -------------------------------------------------------------------------- */
#define SELFTEST_BOOT_BASE_ADDRESS        0x08000000u
#define SELFTEST_BOOT_RESERVED_SIZE       0x00020000u
#define SELFTEST_APP_BASE_ADDRESS         (SELFTEST_BOOT_BASE_ADDRESS + SELFTEST_BOOT_RESERVED_SIZE)
#define SELFTEST_FLASH_END_ADDRESS        0x08100000u

/* STM32F407VG 계열의 RAM 범위를 보수적으로 잡는다.
 * - SRAM1/2 : 0x20000000 ~ 0x2002FFFF (192 KB 범위 사용)
 * - CCM RAM : 0x10000000 ~ 0x1000FFFF (64 KB)
 */
#define SELFTEST_SRAM_BASE                0x20000000u
#define SELFTEST_SRAM_END                 0x20030000u
#define SELFTEST_CCMRAM_BASE              0x10000000u
#define SELFTEST_CCMRAM_END               0x10010000u

#define SELFTEST_LOGFILE_PATH             "0:/SELFTEST_LOG.TXT"
#define SELFTEST_SDTEST_FILE_PATH         "0:/SELFTEST_TMP.BIN"

#define SELFTEST_MENU_BOX_X               12
#define SELFTEST_MENU_BOX_Y               16
#define SELFTEST_MENU_BOX_W               216
#define SELFTEST_MENU_BOX_H               96
#define SELFTEST_MENU_LINE_H              8

#define SELFTEST_PROGRESS_REDRAW_MS       50u

#define SELFTEST_BOOTCRC_PUMP_CHUNK       1024u
#define SELFTEST_RAM_SCRATCH_SIZE         4096u

#define SELFTEST_GPS_TIMEOUT_MS           7000u
#define SELFTEST_GY86_TIMEOUT_MS          4000u
#define SELFTEST_SPI_TIMEOUT_MS           2500u
#define SELFTEST_RTC_TICK_WAIT_MS         1500u
#define SELFTEST_GUIDED_CONFIRM_TIMEOUT_MS 30000u

/* -------------------------------------------------------------------------- */
/* 메뉴 ID                                                                     */
/*                                                                            */
/* 숫자 표시창(bottom center)은 2-digit zero padded 형식으로 보여 주므로      */
/* 실제 ID도 00 ~ 10 구조로 맞춘다.                                           */
/* -------------------------------------------------------------------------- */
typedef enum
{
  SELFTEST_ITEM_AUTO_FULL = 0u,
  SELFTEST_ITEM_BOOT_CHAIN = 1u,
  SELFTEST_ITEM_MCU = 2u,
  SELFTEST_ITEM_RTC = 3u,
  SELFTEST_ITEM_SPI_FLASH = 4u,
  SELFTEST_ITEM_SD = 5u,
  SELFTEST_ITEM_GPS = 6u,
  SELFTEST_ITEM_GY86 = 7u,
  SELFTEST_ITEM_BLUETOOTH = 8u,
  SELFTEST_ITEM_GUIDED = 9u,
  SELFTEST_ITEM_RAM = 10u,
  SELFTEST_ITEM_COUNT
} selftest_main_item_t;

typedef enum
{
  SELFTEST_GUIDED_BUTTON_MATRIX = 0u,
  SELFTEST_GUIDED_LCD_PATTERN = 1u,
  SELFTEST_GUIDED_BACKLIGHT_LOW = 2u,
  SELFTEST_GUIDED_BACKLIGHT_MID = 3u,
  SELFTEST_GUIDED_BACKLIGHT_HIGH = 4u,
  SELFTEST_GUIDED_LED_PATTERN = 5u,
  SELFTEST_GUIDED_AUDIO_TONE = 6u,
  SELFTEST_GUIDED_ITEM_COUNT
} selftest_guided_item_t;

typedef enum
{
  SELFTEST_RESULT_NONE = 0u,
  SELFTEST_RESULT_PASS,
  SELFTEST_RESULT_FAIL,
  SELFTEST_RESULT_INFO
} selftest_result_code_t;

typedef struct
{
  uint8_t id;
  const char *label;
} selftest_menu_item_t;

typedef struct
{
  selftest_result_code_t code;
  char title[40];
  char line1[96];
  char line2[96];
  char line3[96];
  uint32_t duration_ms;
} selftest_exec_result_t;

/* -------------------------------------------------------------------------- */
/* 메뉴 라벨                                                                   */
/* -------------------------------------------------------------------------- */
static const selftest_menu_item_t g_selftest_main_menu[SELFTEST_ITEM_COUNT] = {
  {SELFTEST_ITEM_AUTO_FULL, "AUTO FULL TEST"},
  {SELFTEST_ITEM_BOOT_CHAIN, "BOOT CHAIN / F-W"},
  {SELFTEST_ITEM_MCU, "MCU TEST"},
  {SELFTEST_ITEM_RTC, "RTC TEST"},
  {SELFTEST_ITEM_SPI_FLASH, "SPI FLASH R/W"},
  {SELFTEST_ITEM_SD, "SD R/W TEST"},
  {SELFTEST_ITEM_GPS, "GPS / MON-VER"},
  {SELFTEST_ITEM_GY86, "GY86 / BARO / IMU"},
  {SELFTEST_ITEM_BLUETOOTH, "BLUETOOTH TX"},
  {SELFTEST_ITEM_GUIDED, "USER GUIDED TEST"},
  {SELFTEST_ITEM_RAM, "MEM / RAM TEST"},
};

static const selftest_menu_item_t g_selftest_guided_menu[SELFTEST_GUIDED_ITEM_COUNT] = {
  {SELFTEST_GUIDED_BUTTON_MATRIX, "BUTTON MATRIX"},
  {SELFTEST_GUIDED_LCD_PATTERN, "LCD PATTERN"},
  {SELFTEST_GUIDED_BACKLIGHT_LOW, "BACKLIGHT LOW"},
  {SELFTEST_GUIDED_BACKLIGHT_MID, "BACKLIGHT MID"},
  {SELFTEST_GUIDED_BACKLIGHT_HIGH, "BACKLIGHT HIGH"},
  {SELFTEST_GUIDED_LED_PATTERN, "LED PATTERN"},
  {SELFTEST_GUIDED_AUDIO_TONE, "AUDIO TONE"},
};

/* -------------------------------------------------------------------------- */
/* 유지보수 모드 전용 scratch RAM                                              */
/*                                                                            */
/* full destructive March test는 live app 문맥에서는 시스템 자체를 깨뜨릴 수   */
/* 있으므로, 여기서는 dedicated scratch buffer에 대해 non-destructive pattern  */
/* verify를 수행한다.                                                          */
/* -------------------------------------------------------------------------- */
static uint8_t g_selftest_ram_scratch[SELFTEST_RAM_SCRATCH_SIZE];

/* -------------------------------------------------------------------------- */
/* 공통 유틸                                                                   */
/* -------------------------------------------------------------------------- */
static void SELFTEST_ResultReset(selftest_exec_result_t *out_result,
                                 const char *title)
{
  if (out_result == NULL)
  {
    return;
  }

  memset(out_result, 0, sizeof(*out_result));
  out_result->code = SELFTEST_RESULT_NONE;

  if (title != NULL)
  {
    (void)snprintf(out_result->title, sizeof(out_result->title), "%s", title);
  }
}

static void SELFTEST_ResultSet(selftest_exec_result_t *out_result,
                               selftest_result_code_t code,
                               const char *line1,
                               const char *line2,
                               const char *line3,
                               uint32_t duration_ms)
{
  if (out_result == NULL)
  {
    return;
  }

  out_result->code = code;
  out_result->duration_ms = duration_ms;

  if (line1 != NULL)
  {
    (void)snprintf(out_result->line1, sizeof(out_result->line1), "%s", line1);
  }

  if (line2 != NULL)
  {
    (void)snprintf(out_result->line2, sizeof(out_result->line2), "%s", line2);
  }

  if (line3 != NULL)
  {
    (void)snprintf(out_result->line3, sizeof(out_result->line3), "%s", line3);
  }
}

static const char *SELFTEST_ResultCodeText(selftest_result_code_t code)
{
  switch (code)
  {
  case SELFTEST_RESULT_PASS:
    return "OK";
  case SELFTEST_RESULT_FAIL:
    return "FAIL";
  case SELFTEST_RESULT_INFO:
    return "INFO";
  case SELFTEST_RESULT_NONE:
  default:
    return "----";
  }
}

static const char *SELFTEST_MainItemLabel(uint8_t item_id)
{
  if (item_id >= (uint8_t)SELFTEST_ITEM_COUNT)
  {
    return "UNKNOWN";
  }

  return g_selftest_main_menu[item_id].label;
}

static const char *SELFTEST_GuidedItemLabel(uint8_t item_id)
{
  if (item_id >= (uint8_t)SELFTEST_GUIDED_ITEM_COUNT)
  {
    return "UNKNOWN";
  }

  return g_selftest_guided_menu[item_id].label;
}

static uint8_t SELFTEST_WrapSelection(uint8_t current, int delta, uint8_t item_count)
{
  int next = (int)current + delta;

  if (item_count == 0u)
  {
    return 0u;
  }

  while (next < 0)
  {
    next += (int)item_count;
  }

  while (next >= (int)item_count)
  {
    next -= (int)item_count;
  }

  return (uint8_t)next;
}

static bool SELFTEST_IsSramAddressPlausible(uint32_t addr)
{
  if ((addr >= SELFTEST_SRAM_BASE) && (addr < SELFTEST_SRAM_END))
  {
    return true;
  }

  if ((addr >= SELFTEST_CCMRAM_BASE) && (addr < SELFTEST_CCMRAM_END))
  {
    return true;
  }

  return false;
}

static bool SELFTEST_IsFlashAddressPlausible(uint32_t addr)
{
  return ((addr >= SELFTEST_BOOT_BASE_ADDRESS) &&
          (addr < SELFTEST_FLASH_END_ADDRESS));
}

static bool SELFTEST_IsVectorTablePlausible(uint32_t base_address,
                                            uint32_t *out_stack,
                                            uint32_t *out_reset_pc)
{
  const uint32_t stack_value = *(const volatile uint32_t *)(uintptr_t)(base_address + 0u);
  const uint32_t reset_handler = *(const volatile uint32_t *)(uintptr_t)(base_address + 4u);
  const uint32_t reset_handler_addr = (reset_handler & ~1u);

  if (out_stack != NULL)
  {
    *out_stack = stack_value;
  }

  if (out_reset_pc != NULL)
  {
    *out_reset_pc = reset_handler;
  }

  if (SELFTEST_IsSramAddressPlausible(stack_value) == false)
  {
    return false;
  }

  if ((reset_handler & 0x1u) == 0u)
  {
    return false;
  }

  if (SELFTEST_IsFlashAddressPlausible(reset_handler_addr) == false)
  {
    return false;
  }

  return true;
}

/* -------------------------------------------------------------------------- */
/* CRC32                                                                       */
/*                                                                            */
/* build-time golden CRC manifest가 현재 프로젝트에 없으므로,                  */
/* 여기서는 "계산 자체는 수행"하고, 결과값을 화면/로그에 노출한다.            */
/* pass/fail은 vector sanity + programmed span 존재 여부를 기준으로 한다.      */
/* -------------------------------------------------------------------------- */
static uint32_t SELFTEST_Crc32UpdateByte(uint32_t crc, uint8_t data)
{
  uint32_t i;

  crc ^= (uint32_t)data;

  for (i = 0u; i < 8u; ++i)
  {
    if ((crc & 1u) != 0u)
    {
      crc = (crc >> 1u) ^ 0xEDB88320u;
    }
    else
    {
      crc >>= 1u;
    }
  }

  return crc;
}

static void SELFTEST_ServiceRuntime(void);

static uint32_t SELFTEST_CalcFlashCrc32(uint32_t start_address,
                                        uint32_t length_bytes,
                                        uint8_t item_id,
                                        const char *title)
{
  uint32_t crc = 0xFFFFFFFFu;
  uint32_t offset = 0u;
  uint32_t last_redraw_ms = 0u;
  char line1[64];
  char line2[64];
  u8g2_t *u8g2;

  if (length_bytes == 0u)
  {
    return 0u;
  }

  u8g2 = U8G2_UC1608_GetHandle();

  while (offset < length_bytes)
  {
    uint32_t local_count = 0u;

    while ((offset < length_bytes) && (local_count < SELFTEST_BOOTCRC_PUMP_CHUNK))
    {
      const uint8_t byte_value = *(const volatile uint8_t *)(uintptr_t)(start_address + offset);
      crc = SELFTEST_Crc32UpdateByte(crc, byte_value);
      ++offset;
      ++local_count;
    }

    SELFTEST_ServiceRuntime();

    if ((HAL_GetTick() - last_redraw_ms) >= SELFTEST_PROGRESS_REDRAW_MS)
    {
      (void)snprintf(line1,
                     sizeof(line1),
                     "CRC %lu / %lu bytes",
                     (unsigned long)offset,
                     (unsigned long)length_bytes);
      (void)snprintf(line2,
                     sizeof(line2),
                     "region 0x%08lX",
                     (unsigned long)start_address);

      /* 간단 진행 화면 */
      u8g2_ClearBuffer(u8g2);
      u8g2_SetFont(u8g2, u8g2_font_6x13B_tf);
      u8g2_SetFontPosTop(u8g2);
      u8g2_DrawStr(u8g2, 18, 8, "TEST MODE");
      u8g2_DrawFrame(u8g2,
                     SELFTEST_MENU_BOX_X,
                     SELFTEST_MENU_BOX_Y,
                     SELFTEST_MENU_BOX_W,
                     SELFTEST_MENU_BOX_H);
      u8g2_SetFont(u8g2, u8g2_font_5x8_tf);
      u8g2_DrawStr(u8g2, 20, 28, title);
      u8g2_DrawStr(u8g2, 20, 44, line1);
      u8g2_DrawStr(u8g2, 20, 56, line2);

      {
        char index_text[8];
        (void)snprintf(index_text, sizeof(index_text), "%02u", (unsigned)item_id);
        u8g2_SetFont(u8g2, u8g2_font_6x13B_tf);
        u8g2_DrawStr(u8g2, 108, 111, index_text);
      }

      U8G2_UC1608_CommitBuffer();
      last_redraw_ms = HAL_GetTick();
    }
  }

  return ~crc;
}

static uint32_t SELFTEST_FindProgrammedFlashEnd(uint32_t start_address,
                                                uint32_t end_address_exclusive)
{
  uint32_t addr;

  if (end_address_exclusive <= start_address)
  {
    return start_address;
  }

  addr = end_address_exclusive;

  while (addr > start_address)
  {
    addr -= 4u;

    if (*(const volatile uint32_t *)(uintptr_t)addr != 0xFFFFFFFFu)
    {
      return (addr + 4u);
    }

    if (((end_address_exclusive - addr) & 0x3FFu) == 0u)
    {
      SELFTEST_ServiceRuntime();
    }
  }

  return start_address;
}

static bool SELFTEST_LogPrintf(const char *fmt, ...)
{
  FIL fil;
  FRESULT fr;
  UINT bytes_written = 0u;
  va_list ap;
  char line_buffer[192];
  int line_len;

  if ((fmt == NULL) || (APP_SD_IsFsAccessAllowedNow() == false))
  {
    return false;
  }

  va_start(ap, fmt);
  line_len = vsnprintf(line_buffer, sizeof(line_buffer), fmt, ap);
  va_end(ap);

  if (line_len <= 0)
  {
    return false;
  }

  fr = f_open(&fil, SELFTEST_LOGFILE_PATH, FA_OPEN_ALWAYS | FA_WRITE);
  if (fr != FR_OK)
  {
    return false;
  }

  fr = f_lseek(&fil, f_size(&fil));
  if (fr != FR_OK)
  {
    (void)f_close(&fil);
    return false;
  }

  fr = f_write(&fil,
               line_buffer,
               (UINT)strlen(line_buffer),
               &bytes_written);
  if ((fr == FR_OK) && (bytes_written == (UINT)strlen(line_buffer)))
  {
    (void)f_sync(&fil);
  }

  (void)f_close(&fil);
  return ((fr == FR_OK) && (bytes_written == (UINT)strlen(line_buffer)));
}

static void SELFTEST_LogSessionBanner(const char *reason_text)
{
  RTC_TimeTypeDef rtc_time;
  RTC_DateTypeDef rtc_date;

  memset(&rtc_time, 0, sizeof(rtc_time));
  memset(&rtc_date, 0, sizeof(rtc_date));

  (void)HAL_RTC_GetTime(&hrtc, &rtc_time, RTC_FORMAT_BIN);
  (void)HAL_RTC_GetDate(&hrtc, &rtc_date, RTC_FORMAT_BIN);

  (void)SELFTEST_LogPrintf(
      "\r\n================ SELFTEST SESSION ================\r\n");
  (void)SELFTEST_LogPrintf(
      "tick=%lu rtc=20%02u-%02u-%02u %02u:%02u:%02u reason=%s\r\n",
      (unsigned long)HAL_GetTick(),
      (unsigned)(rtc_date.Year),
      (unsigned)(rtc_date.Month),
      (unsigned)(rtc_date.Date),
      (unsigned)(rtc_time.Hours),
      (unsigned)(rtc_time.Minutes),
      (unsigned)(rtc_time.Seconds),
      (reason_text != NULL) ? reason_text : "-");
}

static void SELFTEST_LogResult(uint8_t item_id,
                               const selftest_exec_result_t *result)
{
  if (result == NULL)
  {
    return;
  }

  (void)SELFTEST_LogPrintf(
      "[%02u] %-18s result=%s dur=%lums\r\n",
      (unsigned)item_id,
      result->title,
      SELFTEST_ResultCodeText(result->code),
      (unsigned long)result->duration_ms);
  (void)SELFTEST_LogPrintf("      %s\r\n", result->line1);
  (void)SELFTEST_LogPrintf("      %s\r\n", result->line2);
  (void)SELFTEST_LogPrintf("      %s\r\n", result->line3);
}

static void SELFTEST_DrawDitherBackgroundExcludingBox(u8g2_t *u8g2,
                                                      int box_x,
                                                      int box_y,
                                                      int box_w,
                                                      int box_h)
{
  int x;
  int y;
  const int disp_w = (int)u8g2_GetDisplayWidth(u8g2);
  const int disp_h = (int)u8g2_GetDisplayHeight(u8g2);

  for (y = 0; y < disp_h; ++y)
  {
    for (x = ((y & 1) != 0) ? 1 : 0; x < disp_w; x += 2)
    {
      if ((x >= box_x) && (x < (box_x + box_w)) &&
          (y >= box_y) && (y < (box_y + box_h)))
      {
        continue;
      }

      if ((((x >> 1) + y) & 1) == 0)
      {
        u8g2_DrawPixel(u8g2, x, y);
      }
    }
  }
}

static void SELFTEST_DrawCenteredString(u8g2_t *u8g2, int y, const char *text)
{
  int x;
  int width;

  if ((u8g2 == NULL) || (text == NULL))
  {
    return;
  }

  width = u8g2_GetStrWidth(u8g2, text);
  x = ((int)u8g2_GetDisplayWidth(u8g2) - width) / 2;

  if (x < 0)
  {
    x = 0;
  }

  u8g2_DrawStr(u8g2, x, y, text);
}

static void SELFTEST_DrawBaseFrame(u8g2_t *u8g2)
{
  u8g2_ClearBuffer(u8g2);
  u8g2_SetDrawColor(u8g2, 1);
  SELFTEST_DrawDitherBackgroundExcludingBox(u8g2,
                                            SELFTEST_MENU_BOX_X,
                                            SELFTEST_MENU_BOX_Y,
                                            SELFTEST_MENU_BOX_W,
                                            SELFTEST_MENU_BOX_H);

  /* 메뉴 박스 내부는 깨끗한 흰 바탕으로 비운 뒤 프레임만 그린다. */
  u8g2_SetDrawColor(u8g2, 0);
  u8g2_DrawBox(u8g2,
               SELFTEST_MENU_BOX_X,
               SELFTEST_MENU_BOX_Y,
               SELFTEST_MENU_BOX_W,
               SELFTEST_MENU_BOX_H);
  u8g2_SetDrawColor(u8g2, 1);
  u8g2_DrawFrame(u8g2,
                 SELFTEST_MENU_BOX_X,
                 SELFTEST_MENU_BOX_Y,
                 SELFTEST_MENU_BOX_W,
                 SELFTEST_MENU_BOX_H);

  u8g2_SetFont(u8g2, u8g2_font_6x13B_tf);
  u8g2_SetFontPosTop(u8g2);
  SELFTEST_DrawCenteredString(u8g2, 2, "TEST MODE");
}

static void SELFTEST_DrawBottomIndex(u8g2_t *u8g2, uint8_t item_id)
{
  char index_text[8];

  (void)snprintf(index_text, sizeof(index_text), "%02u", (unsigned)item_id);
  u8g2_SetFont(u8g2, u8g2_font_6x13B_tf);
  u8g2_SetFontPosTop(u8g2);
  SELFTEST_DrawCenteredString(u8g2, 112, index_text);
}

static void SELFTEST_DrawMenuScreen(const char *title,
                                    const selftest_menu_item_t *items,
                                    uint8_t item_count,
                                    uint8_t selected_id)
{
  u8g2_t *u8g2 = U8G2_UC1608_GetHandle();
  uint8_t i;

  SELFTEST_DrawBaseFrame(u8g2);

  u8g2_SetFont(u8g2, u8g2_font_5x8_tf);
  u8g2_SetFontPosTop(u8g2);

  if (title != NULL)
  {
    u8g2_DrawStr(u8g2, SELFTEST_MENU_BOX_X + 6, SELFTEST_MENU_BOX_Y + 3, title);
  }

  for (i = 0u; i < item_count; ++i)
  {
    char line_buffer[40];
    const int row_y = SELFTEST_MENU_BOX_Y + 12 + ((int)i * SELFTEST_MENU_LINE_H);
    const bool selected = (items[i].id == selected_id);

    (void)snprintf(line_buffer,
                   sizeof(line_buffer),
                   "%u. %s",
                   (unsigned)items[i].id,
                   items[i].label);

    if (selected)
    {
      u8g2_SetDrawColor(u8g2, 1);
      u8g2_DrawBox(u8g2,
                   SELFTEST_MENU_BOX_X + 3,
                   row_y - 1,
                   SELFTEST_MENU_BOX_W - 6,
                   SELFTEST_MENU_LINE_H);
      u8g2_SetDrawColor(u8g2, 0);
    }
    else
    {
      u8g2_SetDrawColor(u8g2, 1);
    }

    u8g2_DrawStr(u8g2, SELFTEST_MENU_BOX_X + 6, row_y, line_buffer);

    if (selected)
    {
      u8g2_SetDrawColor(u8g2, 1);
    }
  }

  SELFTEST_DrawBottomIndex(u8g2, selected_id);
  U8G2_UC1608_CommitBuffer();
}

static void SELFTEST_DrawResultScreen(uint8_t item_id,
                                      const selftest_exec_result_t *result,
                                      bool allow_rerun)
{
  char status_line[48];
  u8g2_t *u8g2 = U8G2_UC1608_GetHandle();

  SELFTEST_DrawBaseFrame(u8g2);

  u8g2_SetFont(u8g2, u8g2_font_5x8_tf);
  u8g2_SetFontPosTop(u8g2);

  (void)snprintf(status_line,
                 sizeof(status_line),
                 "RESULT : %s",
                 SELFTEST_ResultCodeText(result->code));
  u8g2_DrawStr(u8g2, SELFTEST_MENU_BOX_X + 6, SELFTEST_MENU_BOX_Y + 6, result->title);
  u8g2_DrawStr(u8g2, SELFTEST_MENU_BOX_X + 6, SELFTEST_MENU_BOX_Y + 18, status_line);
  u8g2_DrawStr(u8g2, SELFTEST_MENU_BOX_X + 6, SELFTEST_MENU_BOX_Y + 34, result->line1);
  u8g2_DrawStr(u8g2, SELFTEST_MENU_BOX_X + 6, SELFTEST_MENU_BOX_Y + 46, result->line2);
  u8g2_DrawStr(u8g2, SELFTEST_MENU_BOX_X + 6, SELFTEST_MENU_BOX_Y + 58, result->line3);

  if (allow_rerun != false)
  {
    u8g2_DrawStr(u8g2,
                 SELFTEST_MENU_BOX_X + 6,
                 SELFTEST_MENU_BOX_Y + 74,
                 "F3=RERUN / F4=BACK");
  }
  else
  {
    u8g2_DrawStr(u8g2,
                 SELFTEST_MENU_BOX_X + 6,
                 SELFTEST_MENU_BOX_Y + 74,
                 "F4=BACK");
  }

  SELFTEST_DrawBottomIndex(u8g2, item_id);
  U8G2_UC1608_CommitBuffer();
}

static void SELFTEST_DrawWorkingScreen(uint8_t item_id,
                                       const char *title,
                                       const char *line1,
                                       const char *line2,
                                       const char *line3)
{
  u8g2_t *u8g2 = U8G2_UC1608_GetHandle();

  SELFTEST_DrawBaseFrame(u8g2);

  u8g2_SetFont(u8g2, u8g2_font_5x8_tf);
  u8g2_SetFontPosTop(u8g2);
  u8g2_DrawStr(u8g2, SELFTEST_MENU_BOX_X + 6, SELFTEST_MENU_BOX_Y + 6, title);
  u8g2_DrawStr(u8g2, SELFTEST_MENU_BOX_X + 6, SELFTEST_MENU_BOX_Y + 20, line1);
  u8g2_DrawStr(u8g2, SELFTEST_MENU_BOX_X + 6, SELFTEST_MENU_BOX_Y + 32, line2);
  u8g2_DrawStr(u8g2, SELFTEST_MENU_BOX_X + 6, SELFTEST_MENU_BOX_Y + 44, line3);
  u8g2_DrawStr(u8g2,
               SELFTEST_MENU_BOX_X + 6,
               SELFTEST_MENU_BOX_Y + 74,
               "RUNNING...");
  SELFTEST_DrawBottomIndex(u8g2, item_id);
  U8G2_UC1608_CommitBuffer();
}


static bool SELFTEST_ShouldRedraw(uint32_t *io_last_redraw_ms)
{
  const uint32_t now_ms = HAL_GetTick();

  if (io_last_redraw_ms == NULL)
  {
    return true;
  }

  if ((*io_last_redraw_ms == 0u) ||
      ((now_ms - *io_last_redraw_ms) >= SELFTEST_PROGRESS_REDRAW_MS))
  {
    *io_last_redraw_ms = now_ms;
    return true;
  }

  return false;
}

static void SELFTEST_PollNavigation(int *out_delta,
                                    bool *out_execute,
                                    bool *out_back)
{
  button_event_t event;

  if (out_delta != NULL)
  {
    *out_delta = 0;
  }

  if (out_execute != NULL)
  {
    *out_execute = false;
  }

  if (out_back != NULL)
  {
    *out_back = false;
  }

  while (Button_PopEvent(&event) != false)
  {
    if ((event.type != BUTTON_EVENT_SHORT_PRESS) &&
        (event.type != BUTTON_EVENT_LONG_PRESS))
    {
      continue;
    }

    switch (event.id)
    {
    case BUTTON_ID_1:
      if (out_delta != NULL)
      {
        *out_delta = +1;
      }
      break;

    case BUTTON_ID_2:
      if (out_delta != NULL)
      {
        *out_delta = -1;
      }
      break;

    case BUTTON_ID_3:
      if (out_execute != NULL)
      {
        *out_execute = true;
      }
      break;

    case BUTTON_ID_4:
      if (out_back != NULL)
      {
        *out_back = true;
      }
      break;

    default:
      break;
    }
  }
}

static void SELFTEST_ServiceRuntime(void)
{
  const uint32_t now_ms = HAL_GetTick();

  FW_AppGuard_Kick();
  Button_Task(now_ms);

  /* maintenance mode에서도 기존 bring-up 서비스들을 살아 있게 유지한다.
   * - SD hotplug/mount 상태 유지
   * - GPS packet parser keep-alive
   * - BT RX/TX keep-alive
   * - IMU polling 유지
   * - 오디오/LED/백라이트 테스트 동작 유지
   * - SPI flash request state machine 진행
   */
  APP_SD_Task(now_ms);
  Ublox_GPS_Task(now_ms);
  Bluetooth_Task(now_ms);
  GY86_IMU_Task(now_ms);
  Audio_Driver_Task(now_ms);
  Audio_App_Task(now_ms);
  LED_App_Task(now_ms);
  Backlight_App_Task(now_ms);
  SPI_Flash_Task(now_ms);
}

static void SELFTEST_PumpForMs(uint32_t hold_ms,
                               uint8_t item_id,
                               const char *title,
                               const char *line1,
                               const char *line2,
                               const char *line3)
{
  const uint32_t start_ms = HAL_GetTick();
  uint32_t last_redraw_ms = 0u;

  while ((HAL_GetTick() - start_ms) < hold_ms)
  {
    SELFTEST_ServiceRuntime();

    if ((HAL_GetTick() - last_redraw_ms) >= SELFTEST_PROGRESS_REDRAW_MS)
    {
      SELFTEST_DrawWorkingScreen(item_id, title, line1, line2, line3);
      last_redraw_ms = HAL_GetTick();
    }
  }
}

static bool SELFTEST_AskOperatorPassFail(uint8_t item_id,
                                         const char *title,
                                         const char *line1,
                                         const char *line2,
                                         const char *line3,
                                         uint32_t timeout_ms,
                                         bool *out_timed_out)
{
  const uint32_t start_ms = HAL_GetTick();
  uint32_t last_redraw_ms = 0u;
  button_event_t event;

  if (out_timed_out != NULL)
  {
    *out_timed_out = false;
  }

  for (;;)
  {
    SELFTEST_ServiceRuntime();

    if ((HAL_GetTick() - last_redraw_ms) >= SELFTEST_PROGRESS_REDRAW_MS)
    {
      SELFTEST_DrawWorkingScreen(item_id, title, line1, line2, line3);
      last_redraw_ms = HAL_GetTick();
    }

    while (Button_PopEvent(&event) != false)
    {
      if ((event.type != BUTTON_EVENT_SHORT_PRESS) &&
          (event.type != BUTTON_EVENT_LONG_PRESS))
      {
        continue;
      }

      if (event.id == BUTTON_ID_3)
      {
        return true;
      }

      if (event.id == BUTTON_ID_4)
      {
        return false;
      }
    }

    if ((timeout_ms != 0u) && ((HAL_GetTick() - start_ms) >= timeout_ms))
    {
      if (out_timed_out != NULL)
      {
        *out_timed_out = true;
      }

      return false;
    }
  }
}

/* -------------------------------------------------------------------------- */
/* 개별 테스트                                                                  */
/* -------------------------------------------------------------------------- */
static void SELFTEST_TestBootChain(uint8_t item_id,
                                   selftest_exec_result_t *out_result)
{
  uint32_t start_ms;
  uint32_t boot_stack = 0u;
  uint32_t boot_pc = 0u;
  uint32_t app_stack = 0u;
  uint32_t app_pc = 0u;
  uint32_t app_programmed_end;
  uint32_t boot_crc;
  uint32_t app_crc;
  bool boot_vector_ok;
  bool app_vector_ok;
  char line1[96];
  char line2[96];
  char line3[96];

  SELFTEST_ResultReset(out_result, "BOOT CHAIN / F-W");
  start_ms = HAL_GetTick();

  boot_vector_ok = SELFTEST_IsVectorTablePlausible(SELFTEST_BOOT_BASE_ADDRESS,
                                                   &boot_stack,
                                                   &boot_pc);
  app_vector_ok = SELFTEST_IsVectorTablePlausible(SELFTEST_APP_BASE_ADDRESS,
                                                  &app_stack,
                                                  &app_pc);

  app_programmed_end = SELFTEST_FindProgrammedFlashEnd(SELFTEST_APP_BASE_ADDRESS,
                                                       SELFTEST_FLASH_END_ADDRESS);

  boot_crc = SELFTEST_CalcFlashCrc32(SELFTEST_BOOT_BASE_ADDRESS,
                                     SELFTEST_BOOT_RESERVED_SIZE,
                                     item_id,
                                     "BOOT CRC");
  app_crc = 0u;

  if (app_programmed_end > SELFTEST_APP_BASE_ADDRESS)
  {
    app_crc = SELFTEST_CalcFlashCrc32(SELFTEST_APP_BASE_ADDRESS,
                                      app_programmed_end - SELFTEST_APP_BASE_ADDRESS,
                                      item_id,
                                      "APP CRC");
  }

  (void)snprintf(line1,
                 sizeof(line1),
                 "BOOT vec=%s APP vec=%s",
                 boot_vector_ok ? "OK" : "FAIL",
                 app_vector_ok ? "OK" : "FAIL");
  (void)snprintf(line2,
                 sizeof(line2),
                 "BOOT CRC=%08lX APP CRC=%08lX",
                 (unsigned long)boot_crc,
                 (unsigned long)app_crc);
  (void)snprintf(line3,
                 sizeof(line3),
                 "APP span=%lu bytes reset=0x%08lX",
                 (unsigned long)(app_programmed_end - SELFTEST_APP_BASE_ADDRESS),
                 (unsigned long)(app_pc & ~1u));

  if ((boot_vector_ok != false) &&
      (app_vector_ok != false) &&
      (app_programmed_end > (SELFTEST_APP_BASE_ADDRESS + 16u)))
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_PASS,
                       line1,
                       line2,
                       line3,
                       HAL_GetTick() - start_ms);
  }
  else
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_FAIL,
                       line1,
                       line2,
                       line3,
                       HAL_GetTick() - start_ms);
  }
}

static void SELFTEST_TestMcu(uint8_t item_id,
                             selftest_exec_result_t *out_result)
{
  uint32_t start_ms;
  uint32_t cpuid;
  uint32_t devid;
  uint32_t revid;
  uint32_t uid0;
  uint32_t uid1;
  uint32_t uid2;
  uint32_t sysclk;
  uint32_t hclk;
  uint32_t pclk1;
  uint32_t pclk2;
  uint32_t tick0;
  uint32_t tick1;
  bool hse_ready;
  bool lse_ready;
  bool pll_ready;
  char line1[96];
  char line2[96];
  char line3[96];

  (void)item_id;

  SELFTEST_ResultReset(out_result, "MCU TEST");
  start_ms = HAL_GetTick();

  cpuid = SCB->CPUID;
  devid = HAL_GetDEVID();
  revid = HAL_GetREVID();
  uid0 = HAL_GetUIDw0();
  uid1 = HAL_GetUIDw1();
  uid2 = HAL_GetUIDw2();

  sysclk = HAL_RCC_GetSysClockFreq();
  hclk = HAL_RCC_GetHCLKFreq();
  pclk1 = HAL_RCC_GetPCLK1Freq();
  pclk2 = HAL_RCC_GetPCLK2Freq();

  hse_ready = (__HAL_RCC_GET_FLAG(RCC_FLAG_HSERDY) != RESET);
  lse_ready = (__HAL_RCC_GET_FLAG(RCC_FLAG_LSERDY) != RESET);
  pll_ready = (__HAL_RCC_GET_FLAG(RCC_FLAG_PLLRDY) != RESET);

  tick0 = HAL_GetTick();
  SELFTEST_PumpForMs(20u, SELFTEST_ITEM_MCU, "MCU TEST", "tick sanity", "", "");
  tick1 = HAL_GetTick();

  (void)snprintf(line1,
                 sizeof(line1),
                 "CPUID=%08lX DEV=%03lX REV=%04lX",
                 (unsigned long)cpuid,
                 (unsigned long)devid,
                 (unsigned long)revid);
  (void)snprintf(line2,
                 sizeof(line2),
                 "SYS=%lu H=%lu P1=%lu P2=%lu",
                 (unsigned long)sysclk,
                 (unsigned long)hclk,
                 (unsigned long)pclk1,
                 (unsigned long)pclk2);
  (void)snprintf(line3,
                 sizeof(line3),
                 "HSE=%c LSE=%c PLL=%c UID=%08lX",
                 hse_ready ? 'Y' : 'N',
                 lse_ready ? 'Y' : 'N',
                 pll_ready ? 'Y' : 'N',
                 (unsigned long)uid0);

  if ((cpuid != 0u) &&
      (devid != 0u) &&
      (uid0 != 0u || uid1 != 0u || uid2 != 0u) &&
      (sysclk != 0u) &&
      (hclk != 0u) &&
      (pclk1 != 0u) &&
      (pclk2 != 0u) &&
      (tick1 > tick0) &&
      (hse_ready != false) &&
      (pll_ready != false))
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_PASS,
                       line1,
                       line2,
                       line3,
                       HAL_GetTick() - start_ms);
  }
  else
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_FAIL,
                       line1,
                       line2,
                       line3,
                       HAL_GetTick() - start_ms);
  }
}

static bool SELFTEST_RtcFieldsPlausible(const RTC_TimeTypeDef *time_bin,
                                        const RTC_DateTypeDef *date_bin)
{
  if ((time_bin == NULL) || (date_bin == NULL))
  {
    return false;
  }

  if ((time_bin->Hours > 23u) ||
      (time_bin->Minutes > 59u) ||
      (time_bin->Seconds > 59u))
  {
    return false;
  }

  if ((date_bin->Month < 1u) || (date_bin->Month > 12u) ||
      (date_bin->Date < 1u) || (date_bin->Date > 31u))
  {
    return false;
  }

  return true;
}

static void SELFTEST_TestRtc(uint8_t item_id,
                             selftest_exec_result_t *out_result)
{
  uint32_t start_ms;
  RTC_TimeTypeDef time0;
  RTC_TimeTypeDef time1;
  RTC_DateTypeDef date0;
  RTC_DateTypeDef date1;
  uint32_t wait_start_ms;
  bool range_ok;
  bool tick_ok;
  char line1[96];
  char line2[96];
  char line3[96];

  SELFTEST_ResultReset(out_result, "RTC TEST");
  start_ms = HAL_GetTick();

  memset(&time0, 0, sizeof(time0));
  memset(&time1, 0, sizeof(time1));
  memset(&date0, 0, sizeof(date0));
  memset(&date1, 0, sizeof(date1));

  (void)HAL_RTC_GetTime(&hrtc, &time0, RTC_FORMAT_BIN);
  (void)HAL_RTC_GetDate(&hrtc, &date0, RTC_FORMAT_BIN);

  wait_start_ms = HAL_GetTick();
  tick_ok = false;

  while ((HAL_GetTick() - wait_start_ms) < SELFTEST_RTC_TICK_WAIT_MS)
  {
    SELFTEST_ServiceRuntime();
    (void)HAL_RTC_GetTime(&hrtc, &time1, RTC_FORMAT_BIN);
    (void)HAL_RTC_GetDate(&hrtc, &date1, RTC_FORMAT_BIN);

    if ((time1.Seconds != time0.Seconds) ||
        (time1.Minutes != time0.Minutes) ||
        (time1.Hours != time0.Hours))
    {
      tick_ok = true;
      break;
    }
  }

  range_ok = SELFTEST_RtcFieldsPlausible(&time1, &date1);

  (void)snprintf(line1,
                 sizeof(line1),
                 "20%02u-%02u-%02u %02u:%02u:%02u",
                 (unsigned)date1.Year,
                 (unsigned)date1.Month,
                 (unsigned)date1.Date,
                 (unsigned)time1.Hours,
                 (unsigned)time1.Minutes,
                 (unsigned)time1.Seconds);
  (void)snprintf(line2,
                 sizeof(line2),
                 "range=%s tick=%s",
                 range_ok ? "OK" : "FAIL",
                 tick_ok ? "OK" : "FAIL");
  (void)snprintf(line3,
                 sizeof(line3),
                 "weekday=%u subseconds=%lu",
                 (unsigned)date1.WeekDay,
                 (unsigned long)time1.SubSeconds);

  if ((range_ok != false) && (tick_ok != false))
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_PASS,
                       line1,
                       line2,
                       line3,
                       HAL_GetTick() - start_ms);
  }
  else
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_FAIL,
                       line1,
                       line2,
                       line3,
                       HAL_GetTick() - start_ms);
  }
}

static bool SELFTEST_WaitForSpiCommandCompletion(uint32_t timeout_ms,
                                                 spi_flash_snapshot_t *out_snapshot)
{
  uint32_t start_ms = HAL_GetTick();
  spi_flash_snapshot_t first_snapshot;
  bool saw_activity = false;

  memset(&first_snapshot, 0, sizeof(first_snapshot));
  SPI_Flash_CopySnapshot(&first_snapshot);

  while ((HAL_GetTick() - start_ms) < timeout_ms)
  {
    SELFTEST_ServiceRuntime();
    SPI_Flash_CopySnapshot(out_snapshot);

    if ((out_snapshot->busy != false) ||
        (out_snapshot->command_count != first_snapshot.command_count) ||
        (out_snapshot->read_count != first_snapshot.read_count) ||
        (out_snapshot->write_count != first_snapshot.write_count) ||
        (out_snapshot->erase_count != first_snapshot.erase_count))
    {
      saw_activity = true;
    }

    if ((saw_activity != false) && (out_snapshot->busy == false))
    {
      return true;
    }
  }

  return false;
}

static void SELFTEST_TestSpiFlash(uint8_t item_id,
                                  selftest_exec_result_t *out_result)
{
  uint32_t start_ms;
  spi_flash_snapshot_t snapshot;
  bool read_done;
  bool write_done;
  const char *result_text;
  const char *state_text;
  char line1[96];
  char line2[96];
  char line3[96];

  SELFTEST_ResultReset(out_result, "SPI FLASH R/W");
  start_ms = HAL_GetTick();
  memset(&snapshot, 0, sizeof(snapshot));

  SPI_Flash_RequestReadTest();
  read_done = SELFTEST_WaitForSpiCommandCompletion(SELFTEST_SPI_TIMEOUT_MS, &snapshot);

  SPI_Flash_RequestWriteTest();
  write_done = SELFTEST_WaitForSpiCommandCompletion(SELFTEST_SPI_TIMEOUT_MS, &snapshot);

  result_text = SPI_Flash_GetResultText((spi_flash_result_t)snapshot.last_result);
  state_text = SPI_Flash_GetTestStateText((spi_flash_test_state_t)snapshot.test_state);

  (void)snprintf(line1,
                 sizeof(line1),
                 "part=%s compat=%s",
                 (snapshot.part_name[0] != '\0') ? snapshot.part_name : "-",
                 snapshot.w25q16bv_compatible ? "YES" : "NO");
  (void)snprintf(line2,
                 sizeof(line2),
                 "read=%s write=%s result=%s",
                 read_done ? "DONE" : "TIMEOUT",
                 write_done ? "DONE" : "TIMEOUT",
                 (result_text != NULL) ? result_text : "-");
  (void)snprintf(line3,
                 sizeof(line3),
                 "state=%s cmd=%lu err=%lu addr=0x%08lX",
                 (state_text != NULL) ? state_text : "-",
                 (unsigned long)snapshot.command_count,
                 (unsigned long)snapshot.error_count,
                 (unsigned long)snapshot.test_address);

  if ((read_done != false) &&
      (write_done != false) &&
      (snapshot.w25q16bv_compatible != false) &&
      (result_text != NULL) &&
      (strcmp(result_text, "OK") == 0))
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_PASS,
                       line1,
                       line2,
                       line3,
                       HAL_GetTick() - start_ms);
  }
  else
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_FAIL,
                       line1,
                       line2,
                       line3,
                       HAL_GetTick() - start_ms);
  }
}

static void SELFTEST_TestSd(uint8_t item_id,
                            selftest_exec_result_t *out_result)
{
  uint32_t start_ms;
  FIL fil;
  FRESULT fr;
  UINT bytes_done = 0u;
  UINT bytes_read = 0u;
  const uint8_t write_pattern[16] = {
    0x53u, 0x45u, 0x4Cu, 0x46u, 0x54u, 0x45u, 0x53u, 0x54u,
    0x2Du, 0x53u, 0x44u, 0x2Du, 0x4Fu, 0x4Bu, 0x0Du, 0x0Au
  };
  uint8_t read_back[sizeof(write_pattern)];
  bool gate_ok;
  char line1[96];
  char line2[96];
  char line3[96];

  SELFTEST_ResultReset(out_result, "SD R/W TEST");
  start_ms = HAL_GetTick();

  memset(&fil, 0, sizeof(fil));
  memset(read_back, 0, sizeof(read_back));

  gate_ok = APP_SD_IsFsAccessAllowedNow();

  if (gate_ok == false)
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_FAIL,
                       "APP_SD gate closed",
                       "Card absent, debounce pending, or mount not ready",
                       "",
                       HAL_GetTick() - start_ms);
    return;
  }

  fr = f_open(&fil, SELFTEST_SDTEST_FILE_PATH, FA_CREATE_ALWAYS | FA_WRITE);
  if (fr != FR_OK)
  {
    (void)snprintf(line1, sizeof(line1), "f_open write failed: %u", (unsigned)fr);
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_FAIL,
                       line1,
                       "path=0:/SELFTEST_TMP.BIN",
                       "",
                       HAL_GetTick() - start_ms);
    return;
  }

  fr = f_write(&fil, write_pattern, sizeof(write_pattern), &bytes_done);
  if ((fr == FR_OK) && (bytes_done == sizeof(write_pattern)))
  {
    fr = f_sync(&fil);
  }

  (void)f_close(&fil);

  if ((fr != FR_OK) || (bytes_done != sizeof(write_pattern)))
  {
    (void)snprintf(line1,
                   sizeof(line1),
                   "write failed fr=%u bw=%u",
                   (unsigned)fr,
                   (unsigned)bytes_done);
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_FAIL,
                       line1,
                       "non-destructive temp write could not complete",
                       "",
                       HAL_GetTick() - start_ms);
    (void)f_unlink(SELFTEST_SDTEST_FILE_PATH);
    return;
  }

  fr = f_open(&fil, SELFTEST_SDTEST_FILE_PATH, FA_READ);
  if (fr != FR_OK)
  {
    (void)snprintf(line1, sizeof(line1), "f_open read failed: %u", (unsigned)fr);
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_FAIL,
                       line1,
                       "",
                       "",
                       HAL_GetTick() - start_ms);
    (void)f_unlink(SELFTEST_SDTEST_FILE_PATH);
    return;
  }

  fr = f_read(&fil, read_back, sizeof(read_back), &bytes_read);
  (void)f_close(&fil);

  (void)f_unlink(SELFTEST_SDTEST_FILE_PATH);

  (void)snprintf(line1,
                 sizeof(line1),
                 "gate=%s write=%u read=%u",
                 gate_ok ? "OPEN" : "CLOSED",
                 (unsigned)bytes_done,
                 (unsigned)bytes_read);
  (void)snprintf(line2,
                 sizeof(line2),
                 "verify=%s fr=%u",
                 ((fr == FR_OK) &&
                  (bytes_read == sizeof(write_pattern)) &&
                  (memcmp(read_back, write_pattern, sizeof(write_pattern)) == 0)) ? "OK" : "FAIL",
                 (unsigned)fr);
  (void)snprintf(line3,
                 sizeof(line3),
                 "path=0:/SELFTEST_TMP.BIN");

  if ((fr == FR_OK) &&
      (bytes_read == sizeof(write_pattern)) &&
      (memcmp(read_back, write_pattern, sizeof(write_pattern)) == 0))
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_PASS,
                       line1,
                       line2,
                       line3,
                       HAL_GetTick() - start_ms);
  }
  else
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_FAIL,
                       line1,
                       line2,
                       line3,
                       HAL_GetTick() - start_ms);
  }
}

static void SELFTEST_TestGps(uint8_t item_id,
                             selftest_exec_result_t *out_result)
{
  uint32_t start_ms;
  uint32_t baseline_rx_bytes;
  uint32_t baseline_frames_ok;
  app_gps_state_t gps;
  bool rx_flow_ok = false;
  bool parser_ok = false;
  bool mon_ver_ok = false;
  char line1[96];
  char line2[96];
  char line3[96];

  SELFTEST_ResultReset(out_result, "GPS / MON-VER");
  start_ms = HAL_GetTick();
  memset(&gps, 0, sizeof(gps));

  APP_STATE_CopyGpsSnapshot(&gps);
  baseline_rx_bytes = gps.rx_bytes;
  baseline_frames_ok = gps.frames_ok;

  while ((HAL_GetTick() - start_ms) < SELFTEST_GPS_TIMEOUT_MS)
  {
    SELFTEST_ServiceRuntime();
    APP_STATE_CopyGpsSnapshot(&gps);

    rx_flow_ok = (gps.rx_bytes > baseline_rx_bytes);
    parser_ok = (gps.frames_ok > baseline_frames_ok) || (gps.frames_ok > 0u);
    mon_ver_ok = (gps.mon_ver.valid != false) &&
                 (gps.mon_ver.sw_version[0] != '\0');

    if ((rx_flow_ok != false) && (parser_ok != false) && (mon_ver_ok != false))
    {
      break;
    }
  }

  (void)snprintf(line1,
                 sizeof(line1),
                 "init=%c cfg=%c rxrun=%c rx=%lu ok=%lu bad=%lu",
                 gps.initialized ? 'Y' : 'N',
                 gps.configured ? 'Y' : 'N',
                 gps.uart_rx_running ? 'Y' : 'N',
                 (unsigned long)gps.rx_bytes,
                 (unsigned long)gps.frames_ok,
                 (unsigned long)gps.frames_bad_checksum);
  (void)snprintf(line2,
                 sizeof(line2),
                 "MON-VER %s / %s",
                 (gps.mon_ver.sw_version[0] != '\0') ? gps.mon_ver.sw_version : "PENDING",
                 (gps.mon_ver.hw_version[0] != '\0') ? gps.mon_ver.hw_version : "-");
  (void)snprintf(line3,
                 sizeof(line3),
                 "sat vis=%u used=%u ack=%lu/%lu",
                 (unsigned)gps.sat_count_visible,
                 (unsigned)gps.sat_count_used,
                 (unsigned long)gps.ack_ack_count,
                 (unsigned long)gps.ack_nak_count);

  if ((gps.initialized != false) &&
      (gps.configured != false) &&
      (gps.uart_rx_running != false) &&
      (rx_flow_ok != false) &&
      (parser_ok != false) &&
      (mon_ver_ok != false))
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_PASS,
                       line1,
                       line2,
                       line3,
                       HAL_GetTick() - start_ms);
  }
  else
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_FAIL,
                       line1,
                       line2,
                       line3,
                       HAL_GetTick() - start_ms);
  }
}

static void SELFTEST_TestGy86(uint8_t item_id,
                              selftest_exec_result_t *out_result)
{
  uint32_t start_ms;
  app_gy86_state_t imu_start;
  app_gy86_state_t imu_now;
  bool mpu_init_ok;
  bool mag_init_ok;
  bool baro_init_ok;
  bool mpu_flow_ok;
  bool mag_flow_ok;
  bool baro_flow_ok;
  uint32_t valid_slots = 0u;
  int32_t slot0_pa = 0;
  int32_t slot1_pa = 0;
  int32_t delta_pa = 0;
  uint32_t idx;
  char line1[96];
  char line2[96];
  char line3[96];

  SELFTEST_ResultReset(out_result, "GY86 / BARO / IMU");
  start_ms = HAL_GetTick();
  memset(&imu_start, 0, sizeof(imu_start));
  memset(&imu_now, 0, sizeof(imu_now));

  APP_STATE_CopyGy86Snapshot(&imu_start);

  while ((HAL_GetTick() - start_ms) < SELFTEST_GY86_TIMEOUT_MS)
  {
    SELFTEST_ServiceRuntime();
    APP_STATE_CopyGy86Snapshot(&imu_now);

    mpu_init_ok = ((imu_now.debug.init_ok_mask & APP_GY86_DEVICE_MPU) != 0u);
    mag_init_ok = ((imu_now.debug.init_ok_mask & APP_GY86_DEVICE_MAG) != 0u);
    baro_init_ok = ((imu_now.debug.init_ok_mask & APP_GY86_DEVICE_BARO) != 0u);

    mpu_flow_ok = (imu_now.mpu.sample_count > imu_start.mpu.sample_count);
    mag_flow_ok = (imu_now.mag.sample_count > imu_start.mag.sample_count);
    baro_flow_ok = (imu_now.baro.sample_count > imu_start.baro.sample_count);

    if ((mpu_init_ok != false) &&
        (mag_init_ok != false) &&
        (baro_init_ok != false) &&
        (mpu_flow_ok != false) &&
        (mag_flow_ok != false) &&
        (baro_flow_ok != false))
    {
      break;
    }
  }

  valid_slots = 0u;
  slot0_pa = 0;
  slot1_pa = 0;
  delta_pa = 0;

  for (idx = 0u; idx < APP_GY86_BARO_SENSOR_SLOTS; ++idx)
  {
    if ((imu_now.baro_sensor[idx].online != false) &&
        (imu_now.baro_sensor[idx].valid != false) &&
        (imu_now.baro_sensor[idx].sample_count > 0u))
    {
      if (valid_slots == 0u)
      {
        slot0_pa = (int32_t)imu_now.baro_sensor[idx].pressure_pa;
      }
      else if (valid_slots == 1u)
      {
        slot1_pa = (int32_t)imu_now.baro_sensor[idx].pressure_pa;
      }

      ++valid_slots;
    }
  }

  if (valid_slots >= 2u)
  {
    delta_pa = (int32_t)labs((long)(slot0_pa - slot1_pa));
  }

  mpu_init_ok = ((imu_now.debug.init_ok_mask & APP_GY86_DEVICE_MPU) != 0u);
  mag_init_ok = ((imu_now.debug.init_ok_mask & APP_GY86_DEVICE_MAG) != 0u);
  baro_init_ok = ((imu_now.debug.init_ok_mask & APP_GY86_DEVICE_BARO) != 0u);

  mpu_flow_ok = (imu_now.mpu.sample_count > imu_start.mpu.sample_count);
  mag_flow_ok = (imu_now.mag.sample_count > imu_start.mag.sample_count);
  baro_flow_ok = (imu_now.baro.sample_count > imu_start.baro.sample_count);

  (void)snprintf(line1,
                 sizeof(line1),
                 "mask=%02lX m=%lu g=%lu b=%lu",
                 (unsigned long)imu_now.debug.init_ok_mask,
                 (unsigned long)imu_now.mpu.sample_count,
                 (unsigned long)imu_now.mag.sample_count,
                 (unsigned long)imu_now.baro.sample_count);
  (void)snprintf(line2,
                 sizeof(line2),
                 "slots=%lu delta=%ldPa accel=(%d,%d,%d)",
                 (unsigned long)valid_slots,
                 (long)delta_pa,
                 (int)imu_now.mpu.accel_x_raw,
                 (int)imu_now.mpu.accel_y_raw,
                 (int)imu_now.mpu.accel_z_raw);
  (void)snprintf(line3,
                 sizeof(line3),
                 "mpu=%c mag=%c baro=%c flow=%c%c%c",
                 mpu_init_ok ? 'Y' : 'N',
                 mag_init_ok ? 'Y' : 'N',
                 baro_init_ok ? 'Y' : 'N',
                 mpu_flow_ok ? 'Y' : 'N',
                 mag_flow_ok ? 'Y' : 'N',
                 baro_flow_ok ? 'Y' : 'N');

  if ((mpu_init_ok != false) &&
      (mag_init_ok != false) &&
      (baro_init_ok != false) &&
      (mpu_flow_ok != false) &&
      (mag_flow_ok != false) &&
      (baro_flow_ok != false) &&
      (valid_slots >= 1u) &&
      ((valid_slots < 2u) || (delta_pa <= 250)))
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_PASS,
                       line1,
                       line2,
                       line3,
                       HAL_GetTick() - start_ms);
  }
  else
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_FAIL,
                       line1,
                       line2,
                       line3,
                       HAL_GetTick() - start_ms);
  }
}

static void SELFTEST_TestBluetooth(uint8_t item_id,
                                   selftest_exec_result_t *out_result)
{
  uint32_t start_ms;
  HAL_StatusTypeDef tx_status;
  char line1[96];
  char line2[96];
  char line3[96];

  (void)item_id;

  SELFTEST_ResultReset(out_result, "BLUETOOTH TX");
  start_ms = HAL_GetTick();

  tx_status = Bluetooth_SendPrintf("[SELFTEST] Bluetooth TX sanity tick=%lu\r\n",
                                   (unsigned long)HAL_GetTick());

  SELFTEST_PumpForMs(120u,
                     SELFTEST_ITEM_BLUETOOTH,
                     "BLUETOOTH TX",
                     "sending sanity frame...",
                     "",
                     "");

  (void)snprintf(line1,
                 sizeof(line1),
                 "Bluetooth_SendPrintf -> HAL %ld",
                 (long)tx_status);
  (void)snprintf(line2,
                 sizeof(line2),
                 "Local TX path only.");
  (void)snprintf(line3,
                 sizeof(line3),
                 "Peer-side RX needs remote observer/echo.");

  if (tx_status == HAL_OK)
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_PASS,
                       line1,
                       line2,
                       line3,
                       HAL_GetTick() - start_ms);
  }
  else
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_FAIL,
                       line1,
                       line2,
                       line3,
                       HAL_GetTick() - start_ms);
  }
}

static void SELFTEST_TestRam(uint8_t item_id,
                             selftest_exec_result_t *out_result)
{
  uint32_t start_ms;
  size_t i;
  static const uint8_t patterns[] = {0x00u, 0xFFu, 0xAAu, 0x55u};
  uint8_t pattern_index;
  bool pass = true;
  char line1[96];
  char line2[96];
  char line3[96];

  SELFTEST_ResultReset(out_result, "MEM / RAM TEST");
  start_ms = HAL_GetTick();

  for (pattern_index = 0u;
       pattern_index < (uint8_t)(sizeof(patterns) / sizeof(patterns[0]));
       ++pattern_index)
  {
    const uint8_t pat = patterns[pattern_index];

    for (i = 0u; i < sizeof(g_selftest_ram_scratch); ++i)
    {
      g_selftest_ram_scratch[i] = (uint8_t)(pat ^ (uint8_t)i);
    }

    for (i = 0u; i < sizeof(g_selftest_ram_scratch); ++i)
    {
      const uint8_t expected = (uint8_t)(pat ^ (uint8_t)i);

      if (g_selftest_ram_scratch[i] != expected)
      {
        pass = false;
        (void)snprintf(line1,
                       sizeof(line1),
                       "mismatch at %lu pattern=%02X got=%02X",
                       (unsigned long)i,
                       (unsigned)expected,
                       (unsigned)g_selftest_ram_scratch[i]);
        break;
      }

      if ((i & 0x7Fu) == 0u)
      {
        SELFTEST_ServiceRuntime();
      }
    }

    if (pass == false)
    {
      break;
    }
  }

  if (pass != false)
  {
    (void)snprintf(line1,
                   sizeof(line1),
                   "scratch=%lu bytes pattern verify OK",
                   (unsigned long)sizeof(g_selftest_ram_scratch));
  }

  (void)snprintf(line2,
                 sizeof(line2),
                 "non-destructive runtime RAM sanity");
  (void)snprintf(line3,
                 sizeof(line3),
                 "full destructive March test intentionally omitted");

  if (pass != false)
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_PASS,
                       line1,
                       line2,
                       line3,
                       HAL_GetTick() - start_ms);
  }
  else
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_FAIL,
                       line1,
                       line2,
                       line3,
                       HAL_GetTick() - start_ms);
  }
}

/* -------------------------------------------------------------------------- */
/* User Guided Test                                                           */
/* -------------------------------------------------------------------------- */
static bool SELFTEST_GuidedButtonMatrixCore(uint8_t item_id,
                                            selftest_exec_result_t *out_result)
{
  uint32_t start_ms;
  uint32_t seen_mask = 0u;
  uint32_t last_redraw_ms = 0u;
  char line1[96];
  char line2[96];
  char line3[96];
  button_event_t event;

  SELFTEST_ResultReset(out_result, "BUTTON MATRIX");
  start_ms = HAL_GetTick();

  while ((HAL_GetTick() - start_ms) < SELFTEST_GUIDED_CONFIRM_TIMEOUT_MS)
  {
    SELFTEST_ServiceRuntime();

    while (Button_PopEvent(&event) != false)
    {
      if ((event.type != BUTTON_EVENT_SHORT_PRESS) &&
          (event.type != BUTTON_EVENT_LONG_PRESS))
      {
        continue;
      }

      if ((event.id >= BUTTON_ID_1) && (event.id <= BUTTON_ID_6))
      {
        seen_mask |= (1u << ((uint32_t)event.id - 1u));
      }
    }

    (void)snprintf(line1,
                   sizeof(line1),
                   "Press all keys 1..6 once");
    (void)snprintf(line2,
                   sizeof(line2),
                   "seen mask = 0x%02lX",
                   (unsigned long)seen_mask);
    (void)snprintf(line3,
                   sizeof(line3),
                   "Need 0x3F. F4 = FAIL.");

    if ((HAL_GetTick() - last_redraw_ms) >= SELFTEST_PROGRESS_REDRAW_MS)
    {
      SELFTEST_DrawWorkingScreen(item_id, "BUTTON MATRIX", line1, line2, line3);
      last_redraw_ms = HAL_GetTick();
    }

    if (seen_mask == 0x3Fu)
    {
      SELFTEST_ResultSet(out_result,
                         SELFTEST_RESULT_PASS,
                         "All six buttons observed",
                         "mask = 0x3F",
                         "short/long press both accepted",
                         HAL_GetTick() - start_ms);
      return true;
    }
  }

  SELFTEST_ResultSet(out_result,
                     SELFTEST_RESULT_FAIL,
                     "Timeout waiting for all buttons",
                     "Not all 1..6 events were observed",
                     "Use manual retry if wiring is OK",
                     HAL_GetTick() - start_ms);
  return false;
}

static bool SELFTEST_GuidedLcdPatternCore(uint8_t item_id,
                                          selftest_exec_result_t *out_result)
{
  u8g2_t *u8g2 = U8G2_UC1608_GetHandle();
  bool timed_out = false;

  SELFTEST_ResultReset(out_result, "LCD PATTERN");

  /* 1) 전체 흰 화면 */
  u8g2_ClearBuffer(u8g2);
  U8G2_UC1608_CommitBuffer();
  SELFTEST_PumpForMs(700u, item_id, "LCD PATTERN", "full white", "", "");

  /* 2) 전체 검은 화면 */
  u8g2_ClearBuffer(u8g2);
  u8g2_DrawBox(u8g2, 0, 0, u8g2_GetDisplayWidth(u8g2), u8g2_GetDisplayHeight(u8g2));
  U8G2_UC1608_CommitBuffer();
  SELFTEST_PumpForMs(700u, item_id, "LCD PATTERN", "full black", "", "");

  /* 3) checker */
  u8g2_ClearBuffer(u8g2);
  {
    int x;
    int y;
    for (y = 0; y < (int)u8g2_GetDisplayHeight(u8g2); y += 4)
    {
      for (x = 0; x < (int)u8g2_GetDisplayWidth(u8g2); x += 4)
      {
        if ((((x >> 2) + (y >> 2)) & 1) == 0)
        {
          u8g2_DrawBox(u8g2, x, y, 4, 4);
        }
      }
    }
  }
  U8G2_UC1608_CommitBuffer();
  SELFTEST_PumpForMs(700u, item_id, "LCD PATTERN", "checkerboard", "", "");

  if (SELFTEST_AskOperatorPassFail(item_id,
                                   "LCD PATTERN",
                                   "Look for stuck lines/pixels",
                                   "F3 = PASS / F4 = FAIL",
                                   "",
                                   SELFTEST_GUIDED_CONFIRM_TIMEOUT_MS,
                                   &timed_out) != false)
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_PASS,
                       "Operator approved LCD pattern sequence",
                       "white / black / checker completed",
                       "",
                       HAL_GetTick());
    return true;
  }

  if (timed_out != false)
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_FAIL,
                       "Operator confirmation timeout",
                       "LCD pattern sequence finished",
                       "",
                       HAL_GetTick());
  }
  else
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_FAIL,
                       "Operator marked LCD as FAIL",
                       "visual defect reported during pattern sequence",
                       "",
                       HAL_GetTick());
  }

  return false;
}

static bool SELFTEST_GuidedBacklightLevelCore(uint8_t item_id,
                                              uint16_t permille,
                                              const char *title,
                                              selftest_exec_result_t *out_result)
{
  bool timed_out = false;
  char line1[96];

  SELFTEST_ResultReset(out_result, title);

  Backlight_App_SetAutoEnabled(false);
  Backlight_App_SetManualBrightnessPermille(permille);

  (void)snprintf(line1, sizeof(line1), "manual brightness = %u permille", (unsigned)permille);

  if (SELFTEST_AskOperatorPassFail(item_id,
                                   title,
                                   line1,
                                   "Confirm level looks correct",
                                   "F3 = PASS / F4 = FAIL",
                                   SELFTEST_GUIDED_CONFIRM_TIMEOUT_MS,
                                   &timed_out) != false)
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_PASS,
                       line1,
                       "Operator approved backlight level",
                       "",
                       HAL_GetTick());
    return true;
  }

  if (timed_out != false)
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_FAIL,
                       "Operator confirmation timeout",
                       line1,
                       "",
                       HAL_GetTick());
  }
  else
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_FAIL,
                       "Operator marked brightness as FAIL",
                       line1,
                       "",
                       HAL_GetTick());
  }

  return false;
}

static bool SELFTEST_GuidedLedPatternCore(uint8_t item_id,
                                          selftest_exec_result_t *out_result)
{
  bool timed_out = false;
  const char *pattern_name;

  SELFTEST_ResultReset(out_result, "LED PATTERN");

  LED_App_AdvanceTestPattern();
  pattern_name = LED_App_GetTestPatternName(LED_App_GetTestPattern());

  if (SELFTEST_AskOperatorPassFail(item_id,
                                   "LED PATTERN",
                                   (pattern_name != NULL) ? pattern_name : "pattern advanced",
                                   "Observe external LED animation",
                                   "F3 = PASS / F4 = FAIL",
                                   SELFTEST_GUIDED_CONFIRM_TIMEOUT_MS,
                                   &timed_out) != false)
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_PASS,
                       (pattern_name != NULL) ? pattern_name : "pattern advanced",
                       "Operator approved LED pattern",
                       "",
                       HAL_GetTick());
    return true;
  }

  if (timed_out != false)
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_FAIL,
                       "Operator confirmation timeout",
                       (pattern_name != NULL) ? pattern_name : "pattern advanced",
                       "",
                       HAL_GetTick());
  }
  else
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_FAIL,
                       "Operator marked LED pattern as FAIL",
                       (pattern_name != NULL) ? pattern_name : "pattern advanced",
                       "",
                       HAL_GetTick());
  }

  return false;
}

static bool SELFTEST_GuidedAudioToneCore(uint8_t item_id,
                                         selftest_exec_result_t *out_result)
{
  bool timed_out = false;

  SELFTEST_ResultReset(out_result, "AUDIO TONE");

  Audio_Driver_PlaySquareWave(880u);

  if (SELFTEST_AskOperatorPassFail(item_id,
                                   "AUDIO TONE",
                                   "880 Hz square-wave requested",
                                   "Listen for speaker/buzzer output",
                                   "F3 = PASS / F4 = FAIL",
                                   SELFTEST_GUIDED_CONFIRM_TIMEOUT_MS,
                                   &timed_out) != false)
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_PASS,
                       "880 Hz tone requested",
                       "Operator approved audio output",
                       "",
                       HAL_GetTick());
    return true;
  }

  if (timed_out != false)
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_FAIL,
                       "Operator confirmation timeout",
                       "880 Hz tone requested",
                       "",
                       HAL_GetTick());
  }
  else
  {
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_FAIL,
                       "Operator marked audio output as FAIL",
                       "880 Hz tone requested",
                       "",
                       HAL_GetTick());
  }

  return false;
}

static void SELFTEST_GuidedRestoreUiOutputs(void)
{
  Backlight_App_SetManualBrightnessPermille(1000u);
  Backlight_App_SetAutoEnabled(true);
}

static void SELFTEST_RunGuidedSubItem(uint8_t sub_item_id,
                                      selftest_exec_result_t *out_result)
{
  uint32_t start_ms = HAL_GetTick();
  bool pass = false;

  switch ((selftest_guided_item_t)sub_item_id)
  {
  case SELFTEST_GUIDED_BUTTON_MATRIX:
    pass = SELFTEST_GuidedButtonMatrixCore(SELFTEST_ITEM_GUIDED, out_result);
    break;

  case SELFTEST_GUIDED_LCD_PATTERN:
    pass = SELFTEST_GuidedLcdPatternCore(SELFTEST_ITEM_GUIDED, out_result);
    break;

  case SELFTEST_GUIDED_BACKLIGHT_LOW:
    pass = SELFTEST_GuidedBacklightLevelCore(SELFTEST_ITEM_GUIDED,
                                             120u,
                                             "BACKLIGHT LOW",
                                             out_result);
    break;

  case SELFTEST_GUIDED_BACKLIGHT_MID:
    pass = SELFTEST_GuidedBacklightLevelCore(SELFTEST_ITEM_GUIDED,
                                             500u,
                                             "BACKLIGHT MID",
                                             out_result);
    break;

  case SELFTEST_GUIDED_BACKLIGHT_HIGH:
    pass = SELFTEST_GuidedBacklightLevelCore(SELFTEST_ITEM_GUIDED,
                                             1000u,
                                             "BACKLIGHT HIGH",
                                             out_result);
    break;

  case SELFTEST_GUIDED_LED_PATTERN:
    pass = SELFTEST_GuidedLedPatternCore(SELFTEST_ITEM_GUIDED, out_result);
    break;

  case SELFTEST_GUIDED_AUDIO_TONE:
    pass = SELFTEST_GuidedAudioToneCore(SELFTEST_ITEM_GUIDED, out_result);
    break;

  default:
    SELFTEST_ResultReset(out_result, "USER GUIDED TEST");
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_FAIL,
                       "Unknown guided sub-item",
                       "",
                       "",
                       0u);
    break;
  }

  /* duration_ms는 일부 guided helper에서 HAL_GetTick() raw 값으로 넣지 않도록
   * 여기서 한 번 정리한다. */
  out_result->duration_ms = HAL_GetTick() - start_ms;
  (void)pass;

  SELFTEST_GuidedRestoreUiOutputs();
}

static void SELFTEST_RunGuidedComposite(selftest_exec_result_t *out_result)
{
  uint8_t sub_id;
  uint32_t fail_count = 0u;
  uint32_t start_ms = HAL_GetTick();
  char line1[96];
  char line2[96];
  char line3[96];
  selftest_exec_result_t local_result;

  SELFTEST_ResultReset(out_result, "USER GUIDED TEST");

  for (sub_id = 0u; sub_id < (uint8_t)SELFTEST_GUIDED_ITEM_COUNT; ++sub_id)
  {
    SELFTEST_RunGuidedSubItem(sub_id, &local_result);
    SELFTEST_LogResult(SELFTEST_ITEM_GUIDED, &local_result);

    if (local_result.code != SELFTEST_RESULT_PASS)
    {
      ++fail_count;
    }

    /* auto full test 도중에는 각 guided sub-step 결과를 잠깐 보여준다. */
    SELFTEST_DrawResultScreen(SELFTEST_ITEM_GUIDED, &local_result, false);
    SELFTEST_PumpForMs(900u,
                       SELFTEST_ITEM_GUIDED,
                       local_result.title,
                       local_result.line1,
                       local_result.line2,
                       local_result.line3);
  }

  (void)snprintf(line1,
                 sizeof(line1),
                 "guided sub-tests = %u",
                 (unsigned)SELFTEST_GUIDED_ITEM_COUNT);
  (void)snprintf(line2,
                 sizeof(line2),
                 "fail count = %lu",
                 (unsigned long)fail_count);
  (void)snprintf(line3,
                 sizeof(line3),
                 "LCD / buttons / backlight / LED / audio");

  SELFTEST_ResultSet(out_result,
                     (fail_count == 0u) ? SELFTEST_RESULT_PASS : SELFTEST_RESULT_FAIL,
                     line1,
                     line2,
                     line3,
                     0u);

  SELFTEST_GuidedRestoreUiOutputs();
}

/* -------------------------------------------------------------------------- */
/* 테스트 디스패치                                                             */
/* -------------------------------------------------------------------------- */
static void SELFTEST_RunSingleMainItem(uint8_t item_id,
                                       selftest_exec_result_t *out_result)
{
  switch ((selftest_main_item_t)item_id)
  {
  case SELFTEST_ITEM_BOOT_CHAIN:
    SELFTEST_TestBootChain(item_id, out_result);
    break;

  case SELFTEST_ITEM_MCU:
    SELFTEST_TestMcu(item_id, out_result);
    break;

  case SELFTEST_ITEM_RTC:
    SELFTEST_TestRtc(item_id, out_result);
    break;

  case SELFTEST_ITEM_SPI_FLASH:
    SELFTEST_TestSpiFlash(item_id, out_result);
    break;

  case SELFTEST_ITEM_SD:
    SELFTEST_TestSd(item_id, out_result);
    break;

  case SELFTEST_ITEM_GPS:
    SELFTEST_TestGps(item_id, out_result);
    break;

  case SELFTEST_ITEM_GY86:
    SELFTEST_TestGy86(item_id, out_result);
    break;

  case SELFTEST_ITEM_BLUETOOTH:
    SELFTEST_TestBluetooth(item_id, out_result);
    break;

  case SELFTEST_ITEM_GUIDED:
    SELFTEST_RunGuidedComposite(out_result);
    break;

  case SELFTEST_ITEM_RAM:
    SELFTEST_TestRam(item_id, out_result);
    break;

  case SELFTEST_ITEM_AUTO_FULL:
  default:
    SELFTEST_ResultReset(out_result, "AUTO FULL TEST");
    SELFTEST_ResultSet(out_result,
                       SELFTEST_RESULT_INFO,
                       "AUTO FULL TEST is dispatched by separate runner",
                       "",
                       "",
                       0u);
    break;
  }
}

static void SELFTEST_RunAutoFull(selftest_exec_result_t *out_result)
{
  uint8_t item_id;
  uint32_t pass_count = 0u;
  uint32_t fail_count = 0u;
  uint32_t info_count = 0u;
  uint32_t auto_start_ms = HAL_GetTick();
  selftest_exec_result_t local_result;
  char line1[96];
  char line2[96];
  char line3[96];

  SELFTEST_ResultReset(out_result, "AUTO FULL TEST");

  for (item_id = (uint8_t)SELFTEST_ITEM_BOOT_CHAIN;
       item_id < (uint8_t)SELFTEST_ITEM_COUNT;
       ++item_id)
  {
    const char *label = SELFTEST_MainItemLabel(item_id);

    SELFTEST_DrawWorkingScreen(SELFTEST_ITEM_AUTO_FULL,
                               "AUTO FULL TEST",
                               "running...",
                               label,
                               "");
    SELFTEST_RunSingleMainItem(item_id, &local_result);
    SELFTEST_LogResult(item_id, &local_result);

    switch (local_result.code)
    {
    case SELFTEST_RESULT_PASS:
      ++pass_count;
      break;

    case SELFTEST_RESULT_FAIL:
      ++fail_count;
      break;

    case SELFTEST_RESULT_INFO:
    default:
      ++info_count;
      break;
    }

    /* 각 테스트가 끝날 때 operator가 즉시 성공/실패를 볼 수 있게 잠깐 표시 */
    SELFTEST_DrawResultScreen(item_id, &local_result, false);
    SELFTEST_PumpForMs(900u,
                       item_id,
                       local_result.title,
                       local_result.line1,
                       local_result.line2,
                       local_result.line3);
  }

  (void)snprintf(line1,
                 sizeof(line1),
                 "pass=%lu fail=%lu info=%lu",
                 (unsigned long)pass_count,
                 (unsigned long)fail_count,
                 (unsigned long)info_count);
  (void)snprintf(line2,
                 sizeof(line2),
                 "items 1..10 completed");
  (void)snprintf(line3,
                 sizeof(line3),
                 "total=%lums",
                 (unsigned long)(HAL_GetTick() - auto_start_ms));

  SELFTEST_ResultSet(out_result,
                     (fail_count == 0u) ? SELFTEST_RESULT_PASS : SELFTEST_RESULT_FAIL,
                     line1,
                     line2,
                     line3,
                     HAL_GetTick() - auto_start_ms);
}

static void SELFTEST_ShowStickyInfo(uint8_t item_id,
                                    const char *title,
                                    const char *line1,
                                    const char *line2)
{
  bool back = false;
  uint32_t last_redraw_ms = 0u;

  while (back == false)
  {
    int delta = 0;
    bool execute = false;

    SELFTEST_ServiceRuntime();

    if (SELFTEST_ShouldRedraw(&last_redraw_ms) != false)
    {
      SELFTEST_DrawWorkingScreen(item_id, title, line1, line2, "F4 = BACK");
    }

    SELFTEST_PollNavigation(&delta, &execute, &back);
    (void)delta;
    (void)execute;
  }
}

static void SELFTEST_ShowResultAndWait(uint8_t item_id,
                                       const selftest_exec_result_t *result,
                                       bool allow_rerun,
                                       bool *out_rerun)
{
  bool done = false;
  uint32_t last_redraw_ms = 0u;

  if (out_rerun != NULL)
  {
    *out_rerun = false;
  }

  while (done == false)
  {
    int delta = 0;
    bool execute = false;
    bool back = false;

    SELFTEST_ServiceRuntime();

    if (SELFTEST_ShouldRedraw(&last_redraw_ms) != false)
    {
      SELFTEST_DrawResultScreen(item_id, result, allow_rerun);
    }

    SELFTEST_PollNavigation(&delta, &execute, &back);
    (void)delta;

    if ((allow_rerun != false) && (execute != false))
    {
      if (out_rerun != NULL)
      {
        *out_rerun = true;
      }
      done = true;
    }
    else if (back != false)
    {
      done = true;
    }
  }
}


static void SELFTEST_MainMenuLoop(void)
{
  uint8_t main_selection = 0u;
  bool boot_confirmed = false;

  /* 유지보수 모드에 실제로 들어왔으므로,
   * 이제 bootloader 입장에서는 "정상 부팅이 끝난 뒤 maintenance loop로 진입했다"
   * 라고 보는 편이 더 안전하다.
   */
  FW_AppGuard_ConfirmBootOk();
  boot_confirmed = true;
  (void)boot_confirmed;

  SELFTEST_LogSessionBanner("F2 hold maintenance mode");

  {
    uint32_t last_main_redraw_ms = 0u;

    for (;;)
    {
      int delta = 0;
      bool execute = false;
      bool back = false;
      selftest_exec_result_t result;
      bool rerun = false;

      SELFTEST_ServiceRuntime();

      if (SELFTEST_ShouldRedraw(&last_main_redraw_ms) != false)
      {
        SELFTEST_DrawMenuScreen("MAIN MENU",
                                g_selftest_main_menu,
                                (uint8_t)SELFTEST_ITEM_COUNT,
                                main_selection);
      }

      SELFTEST_PollNavigation(&delta, &execute, &back);

      if (delta != 0)
      {
        main_selection = SELFTEST_WrapSelection(main_selection,
                                                delta,
                                                (uint8_t)SELFTEST_ITEM_COUNT);
        continue;
      }

      if (back != false)
      {
        SELFTEST_ShowStickyInfo(main_selection,
                                "MAINTENANCE MODE",
                                "Exit is disabled by design.",
                                "Power cycle / hardware power-off only.");
        continue;
      }

      if (execute == false)
      {
        continue;
      }

      if (main_selection == (uint8_t)SELFTEST_ITEM_GUIDED)
      {
        /* Guided test는 서브메뉴를 한 단계 더 탄다. */
        uint8_t guided_selection = 0u;
        bool leave_guided_menu = false;
        uint32_t last_guided_redraw_ms = 0u;

        while (leave_guided_menu == false)
        {
          int guided_delta = 0;
          bool guided_execute = false;
          bool guided_back = false;
          bool guided_rerun = false;

          SELFTEST_ServiceRuntime();

          if (SELFTEST_ShouldRedraw(&last_guided_redraw_ms) != false)
          {
            SELFTEST_DrawMenuScreen("GUIDED TEST",
                                    g_selftest_guided_menu,
                                    (uint8_t)SELFTEST_GUIDED_ITEM_COUNT,
                                    guided_selection);
          }

          SELFTEST_PollNavigation(&guided_delta, &guided_execute, &guided_back);

          if (guided_delta != 0)
          {
            guided_selection = SELFTEST_WrapSelection(guided_selection,
                                                      guided_delta,
                                                      (uint8_t)SELFTEST_GUIDED_ITEM_COUNT);
            continue;
          }

          if (guided_back != false)
          {
            leave_guided_menu = true;
            continue;
          }

          if (guided_execute == false)
          {
            continue;
          }

          do
          {
            SELFTEST_RunGuidedSubItem(guided_selection, &result);
            SELFTEST_LogResult(SELFTEST_ITEM_GUIDED, &result);
            SELFTEST_ShowResultAndWait(SELFTEST_ITEM_GUIDED,
                                       &result,
                                       true,
                                       &guided_rerun);
          } while (guided_rerun != false);

          /* sub-item 실행 직후에는 다음 redraw를 즉시 허용한다. */
          last_guided_redraw_ms = 0u;
        }

        /* guided submenu를 빠져나오면 main menu도 즉시 redraw한다. */
        last_main_redraw_ms = 0u;
        continue;
      }

      if (main_selection == (uint8_t)SELFTEST_ITEM_AUTO_FULL)
      {
        do
        {
          SELFTEST_RunAutoFull(&result);
          SELFTEST_LogResult(SELFTEST_ITEM_AUTO_FULL, &result);
          SELFTEST_ShowResultAndWait(SELFTEST_ITEM_AUTO_FULL,
                                     &result,
                                     true,
                                     &rerun);
        } while (rerun != false);

        last_main_redraw_ms = 0u;
        continue;
      }

      do
      {
        SELFTEST_RunSingleMainItem(main_selection, &result);
        SELFTEST_LogResult(main_selection, &result);
        SELFTEST_ShowResultAndWait(main_selection, &result, true, &rerun);
      } while (rerun != false);

      last_main_redraw_ms = 0u;
    }
  }
}

/* -------------------------------------------------------------------------- */
/* 공개 API                                                                    */
/* -------------------------------------------------------------------------- */
void SELFTEST_RunMaintenanceModeLoop(void)
{
  /* ---------------------------------------------------------------------- */
  /* 이 함수는 의도적으로 return 하지 않는다.                               */
  /* - 부트 F2 hold로 maintenance mode에 들어오면 일반 앱 슈퍼루프로 복귀하지 */
  /*   않고, 유지보수 전용 루프를 독립적으로 유지한다.                       */
  /* - watchdog은 FW_AppGuard_Kick()로 계속 refresh 한다.                    */
  /* ---------------------------------------------------------------------- */
  SELFTEST_MainMenuLoop();

  /* 논리상 도달하지 않는 구간.
   * 혹시라도 빠져나오면 안전하게 무한 루프에서 watchdog reset을 기다린다. */
  for (;;)
  {
    SELFTEST_ServiceRuntime();
  }
}
