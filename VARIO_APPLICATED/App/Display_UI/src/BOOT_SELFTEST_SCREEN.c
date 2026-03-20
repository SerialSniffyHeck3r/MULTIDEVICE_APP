#include "BOOT_SELFTEST_SCREEN.h"

#include "APP_ALTITUDE.h"
#include "APP_CLOCK.h"
#include "APP_SD.h"
#include "Audio_App.h"
#include "Audio_Driver.h"
#include "BACKLIGHT_App.h"
#include "Bluetooth.h"
#include "Brightness_Sensor.h"
#include "DEBUG_UART.h"
#include "DS18B20_DRIVER.h"
#include "FW_AppGuard.h"
#include "GY86_IMU.h"
#include "SELFTEST.h"
#include "SPI_Flash.h"
#include "Ublox_GPS.h"
#include "button.h"
#include "u8g2.h"
#include "u8g2_uc1608_stm32.h"
#include "ui_boot.h"
#include "ui_types.h"

#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/* 내부 정책 상수                                                              */
/*                                                                            */
/* draw period                                                                  */
/* - TIM7 frame token이 아직 시작되지 않은 구간이므로                           */
/*   direct redraw 주기를 시간 기반으로만 제한한다.                            */
/*                                                                            */
/* hold time                                                                    */
/* - PASS 시 final 화면을 너무 오래 붙잡지 않고 짧게 보여 준 뒤 넘어간다.     */
/* - FAIL 시에는 사용자가 실패 사실을 볼 시간을 조금 더 준다.                  */
/*                                                                            */
/* button release hold                                                          */
/* - 마지막 frame 이후에도 잠깐 버튼 release 안정 시간을 확보해                */
/*   일반 런타임으로 잔상 입력이 넘어가지 않게 한다.                           */
/* -------------------------------------------------------------------------- */
#define BOOT_SELFTEST_SCREEN_DRAW_PERIOD_MS          33u
#define BOOT_SELFTEST_SCREEN_PASS_HOLD_MS           800u
#define BOOT_SELFTEST_SCREEN_FAIL_HOLD_MS          3000u
#define BOOT_SELFTEST_SCREEN_RELEASE_HOLD_MS        150u

/* -------------------------------------------------------------------------- */
/* 내부 helper 선언                                                            */
/* -------------------------------------------------------------------------- */
static uint8_t BOOT_SELFTEST_SCREEN_TimeDue(uint32_t now_ms, uint32_t due_ms);
static void BOOT_SELFTEST_SCREEN_ServiceBackgroundTasks(uint32_t now_ms);
static void BOOT_SELFTEST_SCREEN_DrainButtonEvents(void);
static void BOOT_SELFTEST_SCREEN_DrawFrame(const selftest_report_t *report,
                                           uint32_t now_ms);
static void BOOT_SELFTEST_SCREEN_DrawCenteredStr(u8g2_t *u8g2,
                                                 int16_t y,
                                                 const char *text);
static void BOOT_SELFTEST_SCREEN_DrawRightAlignedStr(u8g2_t *u8g2,
                                                     int16_t x_right,
                                                     int16_t y,
                                                     const char *text);
static void BOOT_SELFTEST_SCREEN_BuildItemStateText(const selftest_item_report_t *item,
                                                    char *out_text,
                                                    size_t out_size);
static void BOOT_SELFTEST_SCREEN_DrawItemLine(u8g2_t *u8g2,
                                              int16_t y,
                                              const char *label,
                                              const selftest_item_report_t *item);
static void BOOT_SELFTEST_SCREEN_WaitForButtonReleaseAndDrain(void);

/* -------------------------------------------------------------------------- */
/* wrap-safe due 판정                                                           */
/* -------------------------------------------------------------------------- */
static uint8_t BOOT_SELFTEST_SCREEN_TimeDue(uint32_t now_ms, uint32_t due_ms)
{
    return ((int32_t)(now_ms - due_ms) >= 0) ? 1u : 0u;
}

/* -------------------------------------------------------------------------- */
/* 부트 self-test 중에도 돌려야 하는 background task 묶음                      */
/*                                                                            */
/* 이유                                                                        */
/* - GPS / IMU / DS18 / Brightness / SD / SPI Flash는                           */
/*   대부분 main loop task가 돌아야 상태가 진전된다.                           */
/* - self-test screen이 blocking 구조이므로,                                   */
/*   이 함수가 임시 boot main loop 역할을 맡는다.                              */
/* -------------------------------------------------------------------------- */
static void BOOT_SELFTEST_SCREEN_ServiceBackgroundTasks(uint32_t now_ms)
{
    /* ---------------------------------------------------------------------- */
    /* SD hotplug / mount / unmount / detect debounce                          */
    /* ---------------------------------------------------------------------- */
    APP_SD_Task(now_ms);

    /* ---------------------------------------------------------------------- */
    /* 오디오 transport / app 레벨 task                                        */
    /*                                                                        */
    /* 현재 self-test 화면 자체가 소리를 꼭 내지는 않더라도,                   */
    /* 이미 Audio_Driver_Init이 끝난 뒤 이 화면에 들어오므로                    */
    /* runtime service를 잠깐 끊어 두지 않기 위해 계속 돌린다.                 */
    /* ---------------------------------------------------------------------- */
    Audio_Driver_Task(now_ms);
    Audio_App_Task(now_ms);

    /* ---------------------------------------------------------------------- */
    /* GPS parser / runtime config maintenance                                 */
    /* ---------------------------------------------------------------------- */
    Ublox_GPS_Task(now_ms);

    /* ---------------------------------------------------------------------- */
    /* RTC / timezone / GPS sync policy                                        */
    /* ---------------------------------------------------------------------- */
    APP_CLOCK_Task(now_ms);

    /* ---------------------------------------------------------------------- */
    /* Bluetooth classic bring-up / RX line parser                             */
    /* ---------------------------------------------------------------------- */
    Bluetooth_Task(now_ms);

    /* ---------------------------------------------------------------------- */
    /* DEBUG UART recovery / queue drain                                       */
    /* ---------------------------------------------------------------------- */
    DEBUG_UART_Task(now_ms);

    /* ---------------------------------------------------------------------- */
    /* 센서 task들                                                             */
    /* ---------------------------------------------------------------------- */
    GY86_IMU_Task(now_ms);
    APP_ALTITUDE_Task(now_ms);
    DS18B20_DRIVER_Task(now_ms);
    Brightness_Sensor_Task(now_ms);

    /* ---------------------------------------------------------------------- */
    /* UX 유지용 background task                                               */
    /* ---------------------------------------------------------------------- */
    Backlight_App_Task(now_ms);
    SPI_Flash_Task(now_ms);
}

/* -------------------------------------------------------------------------- */
/* 버튼 event queue 비우기                                                      */
/*                                                                            */
/* 이 함수는                                                                    */
/* - 전원 ON 확인 화면에서 넘어온 F6 release                                  */
/* - 사용자가 self-test 중 눌러 본 F1~F6                                      */
/* - 기타 boot 중 찌꺼기 버튼 이벤트                                            */
/* 를 전부 소비해서 일반 런타임으로 새어 들어가지 않게 만든다.                */
/* -------------------------------------------------------------------------- */
static void BOOT_SELFTEST_SCREEN_DrainButtonEvents(void)
{
    button_event_t event;

    while (Button_PopEvent(&event) != false)
    {
        /* ------------------------------------------------------------------ */
        /* 의도적으로 아무 처리도 하지 않는다.                                 */
        /* 이 화면은 입력을 받지 않는 boot-only blocker 이다.                  */
        /* ------------------------------------------------------------------ */
        (void)event;
    }
}

/* -------------------------------------------------------------------------- */
/* 가운데 정렬 문자열 draw helper                                              */
/* -------------------------------------------------------------------------- */
static void BOOT_SELFTEST_SCREEN_DrawCenteredStr(u8g2_t *u8g2,
                                                 int16_t y,
                                                 const char *text)
{
    int16_t x;
    uint16_t width;

    if ((u8g2 == 0) || (text == 0))
    {
        return;
    }

    width = u8g2_GetStrWidth(u8g2, text);

    if (width >= UI_LCD_W)
    {
        x = 0;
    }
    else
    {
        x = (int16_t)((UI_LCD_W - width) / 2u);
    }

    if (x < 0)
    {
        x = 0;
    }

    u8g2_DrawStr(u8g2, (u8g2_uint_t)x, (u8g2_uint_t)y, text);
}

/* -------------------------------------------------------------------------- */
/* 오른쪽 기준 정렬 문자열 draw helper                                          */
/* -------------------------------------------------------------------------- */
static void BOOT_SELFTEST_SCREEN_DrawRightAlignedStr(u8g2_t *u8g2,
                                                     int16_t x_right,
                                                     int16_t y,
                                                     const char *text)
{
    int16_t x;
    uint16_t width;

    if ((u8g2 == 0) || (text == 0))
    {
        return;
    }

    width = u8g2_GetStrWidth(u8g2, text);
    x = (int16_t)(x_right - (int16_t)width);

    if (x < 0)
    {
        x = 0;
    }

    u8g2_DrawStr(u8g2, (u8g2_uint_t)x, (u8g2_uint_t)y, text);
}

/* -------------------------------------------------------------------------- */
/* item 상태 문자열 생성                                                        */
/*                                                                            */
/* RUNNING일 때는 progress가 있으면                                            */
/*   TEST 03/05                                                                */
/* 형태로, PASS/FAIL이면                                                        */
/*   OK / FAIL                                                                 */
/* 만 출력한다.                                                                */
/* -------------------------------------------------------------------------- */
static void BOOT_SELFTEST_SCREEN_BuildItemStateText(const selftest_item_report_t *item,
                                                    char *out_text,
                                                    size_t out_size)
{
    if ((out_text == 0) || (out_size == 0u))
    {
        return;
    }

    out_text[0] = '\0';

    if (item == 0)
    {
        (void)snprintf(out_text, out_size, "-");
        return;
    }

    switch (item->state)
    {
    case SELFTEST_ITEM_STATE_RUNNING:
        if (item->progress_target != 0u)
        {
            (void)snprintf(out_text,
                           out_size,
                           "TEST %02lu/%02lu",
                           (unsigned long)item->progress_value,
                           (unsigned long)item->progress_target);
        }
        else
        {
            (void)snprintf(out_text, out_size, "TESTING");
        }
        break;

    case SELFTEST_ITEM_STATE_PASS:
        if (item->short_text[0] != '\0')
        {
            (void)snprintf(out_text, out_size, "%s", item->short_text);
        }
        else
        {
            (void)snprintf(out_text, out_size, "OK");
        }
        break;

    case SELFTEST_ITEM_STATE_FAIL:
        if (item->short_text[0] != '\0')
        {
            (void)snprintf(out_text, out_size, "%s", item->short_text);
        }
        else
        {
            (void)snprintf(out_text, out_size, "FAIL");
        }
        break;

    case SELFTEST_ITEM_STATE_IDLE:
    default:
        (void)snprintf(out_text, out_size, "IDLE");
        break;
    }
}

/* -------------------------------------------------------------------------- */
/* 카테고리 1줄 draw                                                            */
/*                                                                            */
/* 좌측                                                                        */
/* - GPS / IMU / SENSORS / HARDWARE 라벨                                       */
/*                                                                            */
/* 우측                                                                        */
/* - TEST 03/05 / OK / FAIL 상태 문자열                                        */
/*                                                                            */
/* 가운데 dotted leader                                                        */
/* - 사용자가 말한 "gps.... ok" 느낌을 유지하기 위해                           */
/*   라벨과 상태 사이를 점 문자열로 메운다.                                     */
/* -------------------------------------------------------------------------- */
static void BOOT_SELFTEST_SCREEN_DrawItemLine(u8g2_t *u8g2,
                                              int16_t y,
                                              const char *label,
                                              const selftest_item_report_t *item)
{
    char state_text[24];
    uint16_t label_width;
    uint16_t state_width;
    int16_t dots_x0;
    int16_t dots_x1;
    int16_t dot_x;

    if ((u8g2 == 0) || (label == 0) || (item == 0))
    {
        return;
    }

    BOOT_SELFTEST_SCREEN_BuildItemStateText(item, state_text, sizeof(state_text));

    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    label_width = u8g2_GetStrWidth(u8g2, label);
    state_width = u8g2_GetStrWidth(u8g2, state_text);

    /* ---------------------------------------------------------------------- */
    /* 좌측 라벨                                                               */
    /* ---------------------------------------------------------------------- */
    u8g2_DrawStr(u8g2, 14u, (u8g2_uint_t)y, label);

    /* ---------------------------------------------------------------------- */
    /* dotted leader                                                            */
    /*                                                                        */
    /* x=14에서 라벨을 찍고,                                                     */
    /* 우측 상태 문자열은 화면 우측 여백 10px 안쪽에 맞춘다.                    */
    /* 그 사이를 '.' 하나씩 찍어 채운다.                                        */
    /* ---------------------------------------------------------------------- */
    dots_x0 = (int16_t)(14 + label_width + 6);
    dots_x1 = (int16_t)(UI_LCD_W - 10 - state_width - 6);

    for (dot_x = dots_x0; dot_x < dots_x1; dot_x += 4)
    {
        u8g2_DrawPixel(u8g2, (u8g2_uint_t)dot_x, (u8g2_uint_t)(y - 3));
    }

    /* ---------------------------------------------------------------------- */
    /* 우측 상태 문자열                                                         */
    /* ---------------------------------------------------------------------- */
    BOOT_SELFTEST_SCREEN_DrawRightAlignedStr(u8g2,
                                             (int16_t)(UI_LCD_W - 10),
                                             y,
                                             state_text);
}

/* -------------------------------------------------------------------------- */
/* self-test 화면 1프레임 draw                                                  */
/*                                                                            */
/* 화면 구성                                                                   */
/* 1) 상단: boot logo                                                          */
/*    - 기존 early boot draw보다 위쪽으로 당겨서                              */
/*      아래 4개 상태줄이 들어갈 공간을 확보한다.                              */
/* 2) 중간 separator line                                                      */
/* 3) 하단 4줄: GPS / IMU / SENSORS / HARDWARE 상태                            */
/* 4) 맨 아래 footer                                                           */
/* -------------------------------------------------------------------------- */
static void BOOT_SELFTEST_SCREEN_DrawFrame(const selftest_report_t *report,
                                           uint32_t now_ms)
{
    u8g2_t *u8g2;
    ui_boot_logo_t logo;
    int16_t logo_x;
    int16_t logo_y;
    char footer[40];

    (void)now_ms;

    u8g2 = U8G2_UC1608_GetHandle();
    if ((u8g2 == 0) || (report == 0))
    {
        return;
    }

    memset(&logo, 0, sizeof(logo));
    UI_Boot_GetLogo(&logo);

    u8g2_ClearBuffer(u8g2);
    u8g2_SetDrawColor(u8g2, 1u);

    /* ---------------------------------------------------------------------- */
    /* boot logo                                                               */
    /*                                                                        */
    /* 기본 logo가 200x80 기준이므로                                            */
    /* y=2 근처에 붙여서 아래 48px 영역에 상태 4줄을 확보한다.                 */
    /* ---------------------------------------------------------------------- */
    if ((logo.bits != 0) && (logo.width != 0u) && (logo.height != 0u))
    {
        logo_x = (int16_t)((UI_LCD_W - logo.width) / 2u);
        logo_y = 2;

        if (logo_x < 0)
        {
            logo_x = 0;
        }

        u8g2_DrawXBM(u8g2,
                     (u8g2_uint_t)logo_x,
                     (u8g2_uint_t)logo_y,
                     logo.width,
                     logo.height,
                     logo.bits);
    }

    /* ---------------------------------------------------------------------- */
    /* separator line                                                          */
    /*                                                                        */
    /* 상태줄 시작점 위에 얇은 가로선을 한 줄 깔아                             */
    /* logo 영역과 self-test 표 영역을 시각적으로 분리한다.                    */
    /* ---------------------------------------------------------------------- */
    u8g2_DrawHLine(u8g2, 12u, 82u, (u8g2_uint_t)(UI_LCD_W - 24u));

    /* ---------------------------------------------------------------------- */
    /* 4개 self-test 결과 라인                                                  */
    /* ---------------------------------------------------------------------- */
    BOOT_SELFTEST_SCREEN_DrawItemLine(u8g2, 92,  "GPS",      &report->gps);
    BOOT_SELFTEST_SCREEN_DrawItemLine(u8g2, 101, "IMU",      &report->imu);
    BOOT_SELFTEST_SCREEN_DrawItemLine(u8g2, 110, "SENSORS",  &report->sensors);
    BOOT_SELFTEST_SCREEN_DrawItemLine(u8g2, 119, "HARDWARE", &report->hardware);

    /* ---------------------------------------------------------------------- */
    /* footer                                                                   */
    /*                                                                        */
    /* 진행 중                                                                  */
    /* - SELF TEST IN PROGRESS                                                  */
    /*                                                                        */
    /* 완료 + PASS                                                              */
    /* - SELF TEST OK                                                           */
    /*                                                                        */
    /* 완료 + FAIL                                                              */
    /* - FAILURES STORED FOR RUNTIME                                            */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    if (report->finished == false)
    {
        (void)snprintf(footer, sizeof(footer), "SELF TEST IN PROGRESS");
    }
    else if (report->any_failed != false)
    {
        (void)snprintf(footer, sizeof(footer), "FAILURES STORED FOR RUNTIME");
    }
    else
    {
        (void)snprintf(footer, sizeof(footer), "SELF TEST OK");
    }

    BOOT_SELFTEST_SCREEN_DrawCenteredStr(u8g2, 127, footer);
    U8G2_UC1608_CommitBuffer();
}

/* -------------------------------------------------------------------------- */
/* self-test 종료 후 버튼 release 안정 대기                                    */
/*                                                                            */
/* 목적                                                                        */
/* - 사용자가 아직 F6에서 손을 떼는 중일 때                                   */
/*   release 이벤트가 일반 런타임 첫 화면으로 넘어가지 않게 한다.             */
/* - 이 구간에서도 watchdog과 background task는 계속 서비스한다.              */
/* -------------------------------------------------------------------------- */
static void BOOT_SELFTEST_SCREEN_WaitForButtonReleaseAndDrain(void)
{
    uint32_t stable_start_ms;
    uint32_t last_draw_ms;
    selftest_report_t report;

    stable_start_ms = 0u;
    last_draw_ms = 0u;
    SELFTEST_CopyReport(&report);

    for (;;)
    {
        uint32_t now_ms;

        now_ms = HAL_GetTick();
        FW_AppGuard_Kick();

        Button_Task(now_ms);
        BOOT_SELFTEST_SCREEN_DrainButtonEvents();
        BOOT_SELFTEST_SCREEN_ServiceBackgroundTasks(now_ms);

        if (Button_GetPressedMask() == 0u)
        {
            if (stable_start_ms == 0u)
            {
                stable_start_ms = now_ms;
            }
            else if ((uint32_t)(now_ms - stable_start_ms) >= BOOT_SELFTEST_SCREEN_RELEASE_HOLD_MS)
            {
                break;
            }
        }
        else
        {
            stable_start_ms = 0u;
        }

        if ((last_draw_ms == 0u) ||
            ((uint32_t)(now_ms - last_draw_ms) >= BOOT_SELFTEST_SCREEN_DRAW_PERIOD_MS))
        {
            last_draw_ms = now_ms;
            BOOT_SELFTEST_SCREEN_DrawFrame(&report, now_ms);
        }
    }
}

/* -------------------------------------------------------------------------- */
/* 공개 API: blocking boot self-test screen 실행                               */
/* -------------------------------------------------------------------------- */
void BOOT_SELFTEST_SCREEN_RunBlocking(void)
{
    uint32_t finish_hold_deadline_ms;
    uint32_t last_draw_ms;
    uint8_t hold_started;
    selftest_report_t report;

    finish_hold_deadline_ms = 0u;
    last_draw_ms = 0u;
    hold_started = 0u;

    /* ---------------------------------------------------------------------- */
    /* self-test 세션 시작                                                      */
    /* ---------------------------------------------------------------------- */
    SELFTEST_Begin(HAL_GetTick());

    /* ---------------------------------------------------------------------- */
    /* 진입 직후 event queue를 한 번 비워서                                    */
    /* confirm-on LONG F6에서 남은 찌꺼기 이벤트를 바로 소거한다.               */
    /* ---------------------------------------------------------------------- */
    BOOT_SELFTEST_SCREEN_DrainButtonEvents();

    for (;;)
    {
        uint32_t now_ms;

        now_ms = HAL_GetTick();
        FW_AppGuard_Kick();

        /* ------------------------------------------------------------------ */
        /* 버튼은 계속 디바운스하되, event는 즉시 폐기한다.                     */
        /* ------------------------------------------------------------------ */
        Button_Task(now_ms);
        BOOT_SELFTEST_SCREEN_DrainButtonEvents();

        /* ------------------------------------------------------------------ */
        /* 부트 동안 필요한 runtime task 진전                                   */
        /* ------------------------------------------------------------------ */
        BOOT_SELFTEST_SCREEN_ServiceBackgroundTasks(now_ms);

        /* ------------------------------------------------------------------ */
        /* 저수준 self-test 상태머신 진전                                       */
        /* ------------------------------------------------------------------ */
        SELFTEST_Task(now_ms);
        SELFTEST_CopyReport(&report);

        /* ------------------------------------------------------------------ */
        /* draw rate 제한                                                       */
        /* ------------------------------------------------------------------ */
        if ((last_draw_ms == 0u) ||
            ((uint32_t)(now_ms - last_draw_ms) >= BOOT_SELFTEST_SCREEN_DRAW_PERIOD_MS))
        {
            last_draw_ms = now_ms;
            BOOT_SELFTEST_SCREEN_DrawFrame(&report, now_ms);
        }

        /* ------------------------------------------------------------------ */
        /* 4개 self-test가 모두 끝나면 final 상태를 잠깐 더 보여 준다.         */
        /* ------------------------------------------------------------------ */
        if (report.finished != false)
        {
            if (hold_started == 0u)
            {
                hold_started = 1u;
                if (report.any_failed != false)
                {
                    finish_hold_deadline_ms = now_ms + BOOT_SELFTEST_SCREEN_FAIL_HOLD_MS;
                }
                else
                {
                    finish_hold_deadline_ms = now_ms + BOOT_SELFTEST_SCREEN_PASS_HOLD_MS;
                }
            }

            if (BOOT_SELFTEST_SCREEN_TimeDue(now_ms, finish_hold_deadline_ms) != 0u)
            {
                break;
            }
        }
    }

    /* ---------------------------------------------------------------------- */
    /* 마지막으로 release 안정 시간을 확보해                                    */
    /* 일반 런타임 첫 화면으로 잔상 버튼 입력이 넘어가지 않게 한다.             */
    /* ---------------------------------------------------------------------- */
    BOOT_SELFTEST_SCREEN_WaitForButtonReleaseAndDrain();
}
