#ifndef APP_CLOCK_H
#define APP_CLOCK_H

#include "APP_STATE.h"

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  APP_CLOCK                                                                 */
/*                                                                            */
/*  목적                                                                      */
/*  - STM32F4 하드웨어 RTC를 APP 계층에서 안전하게 브링업한다.                 */
/*  - APP_STATE.clock 에 raw / 저수준 / 파생(local time) 정보를 한 번에 모은다. */
/*  - RTC 하드웨어는 UTC 기준으로만 유지하고, timezone은 15분 단위로 별도 관리  */
/*    한다.                                                                   */
/*  - GPS가 fully resolved 되는 순간에는 full date/time sync를 수행하고,       */
/*    이후에는 10분 주기로 GPS의 시간 부분만 다시 반영할 수 있게 만든다.       */
/*                                                                            */
/*  backup domain 사용 규칙                                                    */
/*  - RTC date/time register : 실제 시계                                      */
/*  - RTC backup register   : clock 전용 metadata                             */
/*  - BKPSRAM               : RTC와 무관한 persistent 용도(APP_FAULT 등)       */
/* -------------------------------------------------------------------------- */

void APP_CLOCK_Init(uint32_t now_ms);
void APP_CLOCK_Task(uint32_t now_ms);
void APP_CLOCK_ForceRefresh(uint32_t now_ms);

bool APP_CLOCK_SetManualLocalTime(const app_clock_calendar_t *local_time,
                                  int8_t timezone_quarters,
                                  uint32_t now_ms);

bool APP_CLOCK_RequestExternalRegistrationStub(const app_clock_calendar_t *local_time,
                                               int8_t timezone_quarters,
                                               uint32_t now_ms);

bool APP_CLOCK_RequestGpsFullSyncNow(uint32_t now_ms);
bool APP_CLOCK_RequestGpsTimeOnlySyncNow(uint32_t now_ms);

bool APP_CLOCK_SetTimezoneOnly(int8_t timezone_quarters, uint32_t now_ms);
void APP_CLOCK_SetAutoGpsSyncEnabled(bool enabled, uint32_t now_ms);
void APP_CLOCK_MarkTimeInvalid(uint32_t now_ms);

bool APP_CLOCK_ValidateCalendar(const app_clock_calendar_t *calendar);
uint8_t APP_CLOCK_ComputeWeekday(uint16_t year, uint8_t month, uint8_t day);
int8_t APP_CLOCK_ClampTimezoneQuarters(int32_t timezone_quarters);
void APP_CLOCK_FormatUtcOffsetText(int8_t timezone_quarters,
                                   char *out_text,
                                   size_t out_size);

#ifdef __cplusplus
}
#endif

#endif /* APP_CLOCK_H */
