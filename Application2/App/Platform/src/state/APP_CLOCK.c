#include "APP_CLOCK.h"

#include "rtc.h"

#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*  RTC backup register layout                                                */
/*                                                                            */
/*  이번 교체본에서는 RTC backup register를 "clock 전용 metadata"에만 쓴다.   */
/*  fault log 같은 RTC와 무관한 persistent 데이터는 BKPSRAM으로 이동한다.      */
/* -------------------------------------------------------------------------- */
#define APP_CLOCK_BKP_MAGIC_REG              RTC_BKP_DR0
#define APP_CLOCK_BKP_CONFIG_REG             RTC_BKP_DR1

#define APP_CLOCK_BKP_MAGIC_VALUE            0x434C4B31u /* 'CLK1' */
#define APP_CLOCK_BKP_CONFIG_VERSION         0x01u

#define APP_CLOCK_BKP_CFG_AUTO_GPS_SYNC_BIT  (1u << 0)
#define APP_CLOCK_BKP_CFG_RTC_VALID_BIT      (1u << 1)
#define APP_CLOCK_BKP_CFG_TZ_SHIFT           8u
#define APP_CLOCK_BKP_CFG_TZ_MASK            (0xFFu << APP_CLOCK_BKP_CFG_TZ_SHIFT)
#define APP_CLOCK_BKP_CFG_SYNC_MIN_SHIFT     16u
#define APP_CLOCK_BKP_CFG_SYNC_MIN_MASK      (0xFFu << APP_CLOCK_BKP_CFG_SYNC_MIN_SHIFT)
#define APP_CLOCK_BKP_CFG_VER_SHIFT          24u
#define APP_CLOCK_BKP_CFG_VER_MASK           (0xFFu << APP_CLOCK_BKP_CFG_VER_SHIFT)

/* -------------------------------------------------------------------------- */
/*  runtime tunables                                                          */
/* -------------------------------------------------------------------------- */
#ifndef APP_CLOCK_RTC_REFRESH_MS
#define APP_CLOCK_RTC_REFRESH_MS             200u
#endif

#ifndef APP_CLOCK_INVALID_BASELINE_YEAR
#define APP_CLOCK_INVALID_BASELINE_YEAR      2000u
#endif

#ifndef APP_CLOCK_INVALID_BASELINE_MONTH
#define APP_CLOCK_INVALID_BASELINE_MONTH     1u
#endif

#ifndef APP_CLOCK_INVALID_BASELINE_DAY
#define APP_CLOCK_INVALID_BASELINE_DAY       1u
#endif

#ifndef APP_CLOCK_INVALID_BASELINE_HOUR
#define APP_CLOCK_INVALID_BASELINE_HOUR      0u
#endif

#ifndef APP_CLOCK_INVALID_BASELINE_MIN
#define APP_CLOCK_INVALID_BASELINE_MIN       0u
#endif

#ifndef APP_CLOCK_INVALID_BASELINE_SEC
#define APP_CLOCK_INVALID_BASELINE_SEC       0u
#endif

/* -------------------------------------------------------------------------- */
/*  internal runtime                                                           */
/* -------------------------------------------------------------------------- */
static uint32_t s_app_clock_last_refresh_ms = 0u;
static bool     s_app_clock_last_gps_resolved = false;
static bool     s_app_clock_force_refresh = true;

/* -------------------------------------------------------------------------- */
/*  calendar helper: leap year                                                */
/* -------------------------------------------------------------------------- */
static bool app_clock_is_leap_year(uint16_t year)
{
    if ((year % 400u) == 0u)
    {
        return true;
    }

    if ((year % 100u) == 0u)
    {
        return false;
    }

    return ((year % 4u) == 0u) ? true : false;
}

/* -------------------------------------------------------------------------- */
/*  calendar helper: days in month                                            */
/* -------------------------------------------------------------------------- */
static uint8_t app_clock_days_in_month(uint16_t year, uint8_t month)
{
    static const uint8_t s_days[12] =
    {
        31u, 28u, 31u, 30u, 31u, 30u, 31u, 31u, 30u, 31u, 30u, 31u
    };

    if ((month == 0u) || (month > 12u))
    {
        return 31u;
    }

    if ((month == 2u) && (app_clock_is_leap_year(year) != false))
    {
        return 29u;
    }

    return s_days[month - 1u];
}

/* -------------------------------------------------------------------------- */
/*  public helper: calendar validation                                        */
/* -------------------------------------------------------------------------- */
bool APP_CLOCK_ValidateCalendar(const app_clock_calendar_t *calendar)
{
    if (calendar == 0)
    {
        return false;
    }

    if ((calendar->year < 2000u) || (calendar->year > 2099u))
    {
        return false;
    }

    if ((calendar->month == 0u) || (calendar->month > 12u))
    {
        return false;
    }

    if ((calendar->day == 0u) ||
        (calendar->day > app_clock_days_in_month(calendar->year, calendar->month)))
    {
        return false;
    }

    if (calendar->hour > 23u)
    {
        return false;
    }

    if (calendar->min > 59u)
    {
        return false;
    }

    if (calendar->sec > 59u)
    {
        return false;
    }

    return true;
}

/* -------------------------------------------------------------------------- */
/*  public helper: timezone clamp                                             */
/* -------------------------------------------------------------------------- */
int8_t APP_CLOCK_ClampTimezoneQuarters(int32_t timezone_quarters)
{
    if (timezone_quarters < (int32_t)APP_CLOCK_TIMEZONE_QUARTERS_MIN)
    {
        return (int8_t)APP_CLOCK_TIMEZONE_QUARTERS_MIN;
    }

    if (timezone_quarters > (int32_t)APP_CLOCK_TIMEZONE_QUARTERS_MAX)
    {
        return (int8_t)APP_CLOCK_TIMEZONE_QUARTERS_MAX;
    }

    return (int8_t)timezone_quarters;
}

/* -------------------------------------------------------------------------- */
/*  weekday helper                                                             */
/*                                                                            */
/*  반환값은 STM32 RTC weekday 표현과 맞춰                                     */
/*    1 = Monday                                                               */
/*    2 = Tuesday                                                              */
/*    ...                                                                      */
/*    7 = Sunday                                                               */
/*  로 맞춘다.                                                                 */
/* -------------------------------------------------------------------------- */
uint8_t APP_CLOCK_ComputeWeekday(uint16_t year, uint8_t month, uint8_t day)
{
    static const uint8_t s_sakamoto_offsets[12] =
    {
        0u, 3u, 2u, 5u, 0u, 3u, 5u, 1u, 4u, 6u, 2u, 4u
    };
    uint32_t y;
    uint32_t weekday_sun0;

    y = (uint32_t)year;
    if (month < 3u)
    {
        y -= 1u;
    }

    weekday_sun0 = (y + (y / 4u) - (y / 100u) + (y / 400u) +
                    s_sakamoto_offsets[month - 1u] + (uint32_t)day) % 7u;

    if (weekday_sun0 == 0u)
    {
        return 7u;
    }

    return (uint8_t)weekday_sun0;
}

/* -------------------------------------------------------------------------- */
/*  civil <-> unix seconds helper                                             */
/*                                                                            */
/*  timezone 변환과 day rollover를 단순화하기 위해                             */
/*  내부 계산은 unix second 기반으로 처리한다.                                 */
/* -------------------------------------------------------------------------- */
static int64_t app_clock_days_from_civil(int32_t year, uint32_t month, uint32_t day)
{
    int32_t era;
    uint32_t yoe;
    uint32_t doy;
    uint32_t doe;

    year -= (month <= 2u) ? 1 : 0;
    era = (year >= 0) ? (year / 400) : ((year - 399) / 400);
    yoe = (uint32_t)(year - (era * 400));
    doy = (153u * (month + ((month > 2u) ? (uint32_t)-3 : 9u)) + 2u) / 5u + day - 1u;
    doe = yoe * 365u + yoe / 4u - yoe / 100u + doy;

    return (int64_t)(era * 146097) + (int64_t)doe - 719468ll;
}

static void app_clock_civil_from_days(int64_t days, app_clock_calendar_t *out_calendar)
{
    int64_t z;
    int64_t era;
    uint32_t doe;
    uint32_t yoe;
    uint32_t doy;
    uint32_t mp;
    uint32_t day;
    uint32_t month;
    int32_t year;

    if (out_calendar == 0)
    {
        return;
    }

    z = days + 719468ll;
    era = (z >= 0) ? (z / 146097ll) : ((z - 146096ll) / 146097ll);
    doe = (uint32_t)(z - (era * 146097ll));
    yoe = (doe - doe / 1460u + doe / 36524u - doe / 146096u) / 365u;
    year = (int32_t)yoe + (int32_t)(era * 400ll);
    doy = doe - (365u * yoe + yoe / 4u - yoe / 100u);
    mp = (5u * doy + 2u) / 153u;
    day = doy - (153u * mp + 2u) / 5u + 1u;
    month = mp + ((mp < 10u) ? 3u : (uint32_t)-9);
    year += (month <= 2u) ? 1 : 0;

    out_calendar->year = (uint16_t)year;
    out_calendar->month = (uint8_t)month;
    out_calendar->day = (uint8_t)day;
}

static int64_t app_clock_calendar_to_unix_seconds(const app_clock_calendar_t *calendar)
{
    int64_t days;
    int64_t seconds;

    days = app_clock_days_from_civil((int32_t)calendar->year,
                                     (uint32_t)calendar->month,
                                     (uint32_t)calendar->day);

    seconds = days * 86400ll;
    seconds += ((int64_t)calendar->hour * 3600ll);
    seconds += ((int64_t)calendar->min * 60ll);
    seconds += (int64_t)calendar->sec;

    return seconds;
}

static void app_clock_unix_seconds_to_calendar(int64_t unix_seconds,
                                               app_clock_calendar_t *out_calendar)
{
    int64_t days;
    int64_t seconds_of_day;

    if (out_calendar == 0)
    {
        return;
    }

    days = unix_seconds / 86400ll;
    seconds_of_day = unix_seconds % 86400ll;

    if (seconds_of_day < 0)
    {
        seconds_of_day += 86400ll;
        days -= 1ll;
    }

    app_clock_civil_from_days(days, out_calendar);

    out_calendar->hour = (uint8_t)(seconds_of_day / 3600ll);
    seconds_of_day %= 3600ll;
    out_calendar->min = (uint8_t)(seconds_of_day / 60ll);
    out_calendar->sec = (uint8_t)(seconds_of_day % 60ll);
    out_calendar->weekday = APP_CLOCK_ComputeWeekday(out_calendar->year,
                                                     out_calendar->month,
                                                     out_calendar->day);
}

/* -------------------------------------------------------------------------- */
/*  timezone text helper                                                       */
/* -------------------------------------------------------------------------- */
void APP_CLOCK_FormatUtcOffsetText(int8_t timezone_quarters,
                                   char *out_text,
                                   size_t out_size)
{
    uint32_t total_minutes;
    char sign;

    if ((out_text == 0) || (out_size == 0u))
    {
        return;
    }

    if (timezone_quarters < 0)
    {
        sign = '-';
        total_minutes = (uint32_t)(-(int32_t)timezone_quarters) * 15u;
    }
    else
    {
        sign = '+';
        total_minutes = (uint32_t)timezone_quarters * 15u;
    }

    snprintf(out_text, out_size, "%c%02lu:%02lu",
             sign,
             (unsigned long)(total_minutes / 60u),
             (unsigned long)(total_minutes % 60u));
}

/* -------------------------------------------------------------------------- */
/*  internal helper: backup access open                                       */
/* -------------------------------------------------------------------------- */
static void app_clock_enable_backup_access(void)
{
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();
}

/* -------------------------------------------------------------------------- */
/*  internal helper: setting mirror <-> runtime                              */
/* -------------------------------------------------------------------------- */
static void app_clock_apply_settings_to_runtime_unlocked(void)
{
    g_app_state.clock.timezone_quarters = g_app_state.settings.clock.timezone_quarters;
    g_app_state.clock.gps_sync_interval_minutes =
        g_app_state.settings.clock.gps_sync_interval_minutes;
    g_app_state.clock.gps_auto_sync_enabled_runtime =
        (g_app_state.settings.clock.gps_auto_sync_enabled != 0u) ? true : false;
    g_app_state.clock.timezone_config_valid = true;
}

/* -------------------------------------------------------------------------- */
/*  internal helper: backup config pack/unpack                                */
/* -------------------------------------------------------------------------- */
static uint32_t app_clock_pack_backup_config_word(void)
{
    uint32_t config_word;

    config_word = ((uint32_t)APP_CLOCK_BKP_CONFIG_VERSION << APP_CLOCK_BKP_CFG_VER_SHIFT);
    config_word |= ((uint32_t)g_app_state.settings.clock.gps_sync_interval_minutes <<
                    APP_CLOCK_BKP_CFG_SYNC_MIN_SHIFT);
    config_word |= ((uint32_t)(uint8_t)g_app_state.settings.clock.timezone_quarters <<
                    APP_CLOCK_BKP_CFG_TZ_SHIFT);

    if (g_app_state.settings.clock.gps_auto_sync_enabled != 0u)
    {
        config_word |= APP_CLOCK_BKP_CFG_AUTO_GPS_SYNC_BIT;
    }

    if (g_app_state.clock.rtc_time_valid != false)
    {
        config_word |= APP_CLOCK_BKP_CFG_RTC_VALID_BIT;
    }

    return config_word;
}

static bool app_clock_unpack_backup_config_word(uint32_t config_word,
                                                app_clock_settings_t *out_settings,
                                                bool *out_time_valid)
{
    int8_t timezone_quarters;
    uint8_t sync_minutes;
    uint8_t version;

    if ((out_settings == 0) || (out_time_valid == 0))
    {
        return false;
    }

    version = (uint8_t)((config_word & APP_CLOCK_BKP_CFG_VER_MASK) >> APP_CLOCK_BKP_CFG_VER_SHIFT);
    if (version != APP_CLOCK_BKP_CONFIG_VERSION)
    {
        return false;
    }

    timezone_quarters = (int8_t)((config_word & APP_CLOCK_BKP_CFG_TZ_MASK) >> APP_CLOCK_BKP_CFG_TZ_SHIFT);
    if ((timezone_quarters < APP_CLOCK_TIMEZONE_QUARTERS_MIN) ||
        (timezone_quarters > APP_CLOCK_TIMEZONE_QUARTERS_MAX))
    {
        return false;
    }

    sync_minutes = (uint8_t)((config_word & APP_CLOCK_BKP_CFG_SYNC_MIN_MASK) >> APP_CLOCK_BKP_CFG_SYNC_MIN_SHIFT);
    if (sync_minutes == 0u)
    {
        sync_minutes = APP_CLOCK_GPS_SYNC_INTERVAL_MIN_DEFAULT;
    }

    out_settings->timezone_quarters = timezone_quarters;
    out_settings->gps_auto_sync_enabled =
        ((config_word & APP_CLOCK_BKP_CFG_AUTO_GPS_SYNC_BIT) != 0u) ? 1u : 0u;
    out_settings->gps_sync_interval_minutes = sync_minutes;
    out_settings->reserved0 = 0u;

    *out_time_valid = ((config_word & APP_CLOCK_BKP_CFG_RTC_VALID_BIT) != 0u) ? true : false;
    return true;
}

/* -------------------------------------------------------------------------- */
/*  internal helper: backup config store                                      */
/* -------------------------------------------------------------------------- */
static void app_clock_store_backup_config_unlocked(void)
{
    uint32_t config_word;

    app_clock_enable_backup_access();

    config_word = app_clock_pack_backup_config_word();

    HAL_RTCEx_BKUPWrite(&hrtc, APP_CLOCK_BKP_MAGIC_REG, APP_CLOCK_BKP_MAGIC_VALUE);
    HAL_RTCEx_BKUPWrite(&hrtc, APP_CLOCK_BKP_CONFIG_REG, config_word);

    g_app_state.clock.backup_write_count++;
    g_app_state.clock.backup_config_valid = true;
}

/* -------------------------------------------------------------------------- */
/*  internal helper: defaults on empty backup domain                          */
/* -------------------------------------------------------------------------- */
static void app_clock_apply_default_config_unlocked(void)
{
    g_app_state.settings.clock.timezone_quarters = APP_CLOCK_TIMEZONE_QUARTERS_DEFAULT;
    g_app_state.settings.clock.gps_auto_sync_enabled = 1u;
    g_app_state.settings.clock.gps_sync_interval_minutes =
        APP_CLOCK_GPS_SYNC_INTERVAL_MIN_DEFAULT;
    g_app_state.settings.clock.reserved0 = 0u;

    app_clock_apply_settings_to_runtime_unlocked();
    g_app_state.clock.rtc_time_valid = false;
    g_app_state.clock.last_sync_source = (uint8_t)APP_CLOCK_SYNC_SOURCE_BOOT_DEFAULT;
}

/* -------------------------------------------------------------------------- */
/*  internal helper: load backup config or create default                     */
/* -------------------------------------------------------------------------- */
static bool app_clock_load_backup_config_unlocked(void)
{
    uint32_t magic_word;
    uint32_t config_word;
    bool rtc_time_valid;
    bool config_ok;
    app_clock_settings_t loaded_settings;

    memset((void *)&loaded_settings, 0, sizeof(loaded_settings));
    app_clock_enable_backup_access();

    magic_word = HAL_RTCEx_BKUPRead(&hrtc, APP_CLOCK_BKP_MAGIC_REG);
    config_word = HAL_RTCEx_BKUPRead(&hrtc, APP_CLOCK_BKP_CONFIG_REG);
    g_app_state.clock.backup_read_count++;

    if (magic_word != APP_CLOCK_BKP_MAGIC_VALUE)
    {
        app_clock_apply_default_config_unlocked();
        app_clock_store_backup_config_unlocked();
        return true;
    }

    config_ok = app_clock_unpack_backup_config_word(config_word,
                                                    &loaded_settings,
                                                    &rtc_time_valid);
    if (config_ok == false)
    {
        app_clock_apply_default_config_unlocked();
        app_clock_store_backup_config_unlocked();
        return true;
    }

    g_app_state.settings.clock = loaded_settings;
    app_clock_apply_settings_to_runtime_unlocked();
    g_app_state.clock.rtc_time_valid = rtc_time_valid;
    g_app_state.clock.backup_config_valid = true;
    return false;
}

/* -------------------------------------------------------------------------- */
/*  internal helper: convert UTC -> local using configured timezone           */
/* -------------------------------------------------------------------------- */
static void app_clock_update_local_from_utc_unlocked(void)
{
    int64_t utc_seconds;
    int64_t local_seconds;
    int32_t timezone_minutes;
    app_clock_calendar_t utc_copy;
    app_clock_calendar_t local_copy;

    utc_copy = g_app_state.clock.utc;
    memset((void *)&local_copy, 0, sizeof(local_copy));

    if (APP_CLOCK_ValidateCalendar(&utc_copy) == false)
    {
        memset((void *)&g_app_state.clock.local, 0, sizeof(g_app_state.clock.local));
        return;
    }

    timezone_minutes = (int32_t)g_app_state.clock.timezone_quarters * 15;
    utc_seconds = app_clock_calendar_to_unix_seconds(&utc_copy);
    local_seconds = utc_seconds + ((int64_t)timezone_minutes * 60ll);

    app_clock_unix_seconds_to_calendar(local_seconds, &local_copy);
    g_app_state.clock.local = local_copy;
}

/* -------------------------------------------------------------------------- */
/*  internal helper: mark HAL error                                           */
/* -------------------------------------------------------------------------- */
static void app_clock_record_hal_error_unlocked(HAL_StatusTypeDef hal_status)
{
    g_app_state.clock.last_hal_status = (uint32_t)hal_status;
    g_app_state.clock.rtc_error_count++;
}

/* -------------------------------------------------------------------------- */
/*  internal helper: read RTC registers                                       */
/*                                                                            */
/*  HAL 규칙상 time을 먼저 읽고, 바로 이어서 date를 읽어 shadow register 잠금을 */
/*  해제해야 한다.                                                             */
/* -------------------------------------------------------------------------- */
static bool app_clock_read_rtc_into_state_unlocked(uint32_t now_ms)
{
    RTC_TimeTypeDef rtc_time;
    RTC_DateTypeDef rtc_date;
    HAL_StatusTypeDef hal_status;
    app_clock_calendar_t utc;

    memset((void *)&rtc_time, 0, sizeof(rtc_time));
    memset((void *)&rtc_date, 0, sizeof(rtc_date));
    memset((void *)&utc, 0, sizeof(utc));

    hal_status = HAL_RTC_GetTime(&hrtc, &rtc_time, RTC_FORMAT_BIN);
    if (hal_status != HAL_OK)
    {
        app_clock_record_hal_error_unlocked(hal_status);
        g_app_state.clock.rtc_read_valid = false;
        return false;
    }

    hal_status = HAL_RTC_GetDate(&hrtc, &rtc_date, RTC_FORMAT_BIN);
    if (hal_status != HAL_OK)
    {
        app_clock_record_hal_error_unlocked(hal_status);
        g_app_state.clock.rtc_read_valid = false;
        return false;
    }

    utc.year = (uint16_t)(2000u + (uint16_t)rtc_date.Year);
    utc.month = rtc_date.Month;
    utc.day = rtc_date.Date;
    utc.hour = rtc_time.Hours;
    utc.min = rtc_time.Minutes;
    utc.sec = rtc_time.Seconds;
    utc.weekday = rtc_date.WeekDay;

    g_app_state.clock.rtc_time_raw.hours = rtc_time.Hours;
    g_app_state.clock.rtc_time_raw.minutes = rtc_time.Minutes;
    g_app_state.clock.rtc_time_raw.seconds = rtc_time.Seconds;
    g_app_state.clock.rtc_time_raw.time_format = rtc_time.TimeFormat;
    g_app_state.clock.rtc_time_raw.sub_seconds = rtc_time.SubSeconds;
    g_app_state.clock.rtc_time_raw.second_fraction = rtc_time.SecondFraction;
    g_app_state.clock.rtc_time_raw.daylight_saving = rtc_time.DayLightSaving;
    g_app_state.clock.rtc_time_raw.store_operation = rtc_time.StoreOperation;

    g_app_state.clock.rtc_date_raw.week_day = rtc_date.WeekDay;
    g_app_state.clock.rtc_date_raw.month = rtc_date.Month;
    g_app_state.clock.rtc_date_raw.date = rtc_date.Date;
    g_app_state.clock.rtc_date_raw.year_2digit = rtc_date.Year;

    if (APP_CLOCK_ValidateCalendar(&utc) == false)
    {
        memset((void *)&g_app_state.clock.utc, 0, sizeof(g_app_state.clock.utc));
        memset((void *)&g_app_state.clock.local, 0, sizeof(g_app_state.clock.local));
        g_app_state.clock.invalid_read_count++;
        g_app_state.clock.rtc_read_valid = false;
        return false;
    }

    g_app_state.clock.utc = utc;
    g_app_state.clock.rtc_read_valid = true;
    g_app_state.clock.last_hw_read_ms = now_ms;
    g_app_state.clock.rtc_read_count++;
    g_app_state.clock.last_hal_status = (uint32_t)HAL_OK;

    app_clock_update_local_from_utc_unlocked();
    return true;
}

/* -------------------------------------------------------------------------- */
/*  internal helper: write UTC calendar to RTC                                */
/* -------------------------------------------------------------------------- */
static bool app_clock_write_utc_to_rtc_unlocked(const app_clock_calendar_t *utc,
                                                uint32_t now_ms)
{
    RTC_TimeTypeDef rtc_time;
    RTC_DateTypeDef rtc_date;
    HAL_StatusTypeDef hal_status;

    if (APP_CLOCK_ValidateCalendar(utc) == false)
    {
        return false;
    }

    memset((void *)&rtc_time, 0, sizeof(rtc_time));
    memset((void *)&rtc_date, 0, sizeof(rtc_date));

    rtc_time.Hours = utc->hour;
    rtc_time.Minutes = utc->min;
    rtc_time.Seconds = utc->sec;
    rtc_time.TimeFormat = RTC_HOURFORMAT12_AM;
    rtc_time.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    rtc_time.StoreOperation = RTC_STOREOPERATION_RESET;

    rtc_date.WeekDay = APP_CLOCK_ComputeWeekday(utc->year, utc->month, utc->day);
    rtc_date.Month = utc->month;
    rtc_date.Date = utc->day;
    rtc_date.Year = (uint8_t)(utc->year - 2000u);

    hal_status = HAL_RTC_SetTime(&hrtc, &rtc_time, RTC_FORMAT_BIN);
    if (hal_status != HAL_OK)
    {
        app_clock_record_hal_error_unlocked(hal_status);
        return false;
    }

    hal_status = HAL_RTC_SetDate(&hrtc, &rtc_date, RTC_FORMAT_BIN);
    if (hal_status != HAL_OK)
    {
        app_clock_record_hal_error_unlocked(hal_status);
        return false;
    }

    g_app_state.clock.last_hal_status = (uint32_t)HAL_OK;
    g_app_state.clock.last_hw_set_ms = now_ms;
    g_app_state.clock.rtc_write_count++;
    return true;
}

/* -------------------------------------------------------------------------- */
/*  internal helper: baseline write on empty backup domain                    */
/*                                                                            */
/*  cold boot에서 backup battery가 없으면 RTC register 내용이 의미 없는 값일 수  */
/*  있다. 이 경우 "알려진 baseline UTC" 를 써 두고 valid 플래그는 false로 남긴다. */
/* -------------------------------------------------------------------------- */
static void app_clock_write_invalid_baseline_unlocked(uint32_t now_ms)
{
    app_clock_calendar_t baseline;

    baseline.year = APP_CLOCK_INVALID_BASELINE_YEAR;
    baseline.month = APP_CLOCK_INVALID_BASELINE_MONTH;
    baseline.day = APP_CLOCK_INVALID_BASELINE_DAY;
    baseline.hour = APP_CLOCK_INVALID_BASELINE_HOUR;
    baseline.min = APP_CLOCK_INVALID_BASELINE_MIN;
    baseline.sec = APP_CLOCK_INVALID_BASELINE_SEC;
    baseline.weekday = APP_CLOCK_ComputeWeekday(baseline.year,
                                                baseline.month,
                                                baseline.day);

    (void)app_clock_write_utc_to_rtc_unlocked(&baseline, now_ms);
}

/* -------------------------------------------------------------------------- */
/*  internal helper: set validity + persist metadata                          */
/* -------------------------------------------------------------------------- */
static void app_clock_set_rtc_validity_unlocked(bool valid, uint32_t now_ms)
{
    g_app_state.clock.rtc_time_valid = valid;
    g_app_state.clock.last_validity_change_ms = now_ms;
    app_clock_store_backup_config_unlocked();
}

/* -------------------------------------------------------------------------- */
/*  internal helper: copy GPS candidate                                       */
/* -------------------------------------------------------------------------- */
static bool app_clock_copy_gps_candidate_utc(app_clock_calendar_t *out_utc)
{
    gps_fix_basic_t gps_fix;

    if (out_utc == 0)
    {
        return false;
    }

    memset((void *)&gps_fix, 0, sizeof(gps_fix));
    memset((void *)out_utc, 0, sizeof(*out_utc));

    __disable_irq();
    gps_fix = g_app_state.gps.fix;
    __enable_irq();

    if ((gps_fix.valid_date == 0u) ||
        (gps_fix.valid_time == 0u) ||
        (gps_fix.fully_resolved == 0u))
    {
        return false;
    }

    out_utc->year = gps_fix.year;
    out_utc->month = gps_fix.month;
    out_utc->day = gps_fix.day;
    out_utc->hour = gps_fix.hour;
    out_utc->min = gps_fix.min;
    out_utc->sec = gps_fix.sec;
    out_utc->weekday = APP_CLOCK_ComputeWeekday(out_utc->year,
                                                out_utc->month,
                                                out_utc->day);

    if (APP_CLOCK_ValidateCalendar(out_utc) == false)
    {
        return false;
    }

    return true;
}

/* -------------------------------------------------------------------------- */
/*  internal helper: local -> UTC                                             */
/* -------------------------------------------------------------------------- */
static bool app_clock_convert_local_to_utc(const app_clock_calendar_t *local_time,
                                           int8_t timezone_quarters,
                                           app_clock_calendar_t *out_utc)
{
    int64_t local_seconds;
    int64_t utc_seconds;
    int32_t timezone_minutes;

    if ((local_time == 0) || (out_utc == 0))
    {
        return false;
    }

    if (APP_CLOCK_ValidateCalendar(local_time) == false)
    {
        return false;
    }

    timezone_quarters = APP_CLOCK_ClampTimezoneQuarters((int32_t)timezone_quarters);
    timezone_minutes = (int32_t)timezone_quarters * 15;

    local_seconds = app_clock_calendar_to_unix_seconds(local_time);
    utc_seconds = local_seconds - ((int64_t)timezone_minutes * 60ll);

    app_clock_unix_seconds_to_calendar(utc_seconds, out_utc);
    return APP_CLOCK_ValidateCalendar(out_utc);
}

/* -------------------------------------------------------------------------- */
/*  internal helper: refresh runtime snapshot                                 */
/* -------------------------------------------------------------------------- */
static void app_clock_refresh_runtime_snapshot_unlocked(uint32_t now_ms)
{
    (void)app_clock_read_rtc_into_state_unlocked(now_ms);
    s_app_clock_last_refresh_ms = now_ms;
    s_app_clock_force_refresh = false;
}

/* -------------------------------------------------------------------------- */
/*  internal helper: common local set path                                    */
/* -------------------------------------------------------------------------- */
static bool app_clock_set_from_local_common(const app_clock_calendar_t *local_time,
                                            int8_t timezone_quarters,
                                            app_clock_sync_source_t source,
                                            uint32_t now_ms)
{
    app_clock_calendar_t utc;

    if ((source != APP_CLOCK_SYNC_SOURCE_MANUAL) &&
        (source != APP_CLOCK_SYNC_SOURCE_EXTERNAL_STUB))
    {
        return false;
    }

    if (app_clock_convert_local_to_utc(local_time, timezone_quarters, &utc) == false)
    {
        return false;
    }

    g_app_state.settings.clock.timezone_quarters = APP_CLOCK_ClampTimezoneQuarters((int32_t)timezone_quarters);
    g_app_state.settings.clock.reserved0 = 0u;
    app_clock_apply_settings_to_runtime_unlocked();

    if (app_clock_write_utc_to_rtc_unlocked(&utc, now_ms) == false)
    {
        return false;
    }

    g_app_state.clock.last_sync_source = (uint8_t)source;
    g_app_state.clock.gps_last_sync_success = false;
    g_app_state.clock.gps_last_sync_was_full = false;
    g_app_state.clock.manual_set_count++;
    app_clock_set_rtc_validity_unlocked(true, now_ms);
    app_clock_refresh_runtime_snapshot_unlocked(now_ms);
    return true;
}

/* -------------------------------------------------------------------------- */
/*  internal helper: GPS sync                                                 */
/* -------------------------------------------------------------------------- */
static bool app_clock_sync_from_gps_unlocked(bool full_sync, uint32_t now_ms)
{
    app_clock_calendar_t gps_utc;
    app_clock_calendar_t rtc_utc_to_write;

    if (app_clock_copy_gps_candidate_utc(&gps_utc) == false)
    {
        g_app_state.clock.gps_candidate_valid = false;
        g_app_state.clock.gps_last_sync_success = false;
        return false;
    }

    g_app_state.clock.gps_candidate_valid = true;
    g_app_state.clock.last_gps_utc = gps_utc;

    /* ---------------------------------------------------------------------- */
    /*  periodic sync는 요구사항대로 "시간 부분만" 반영한다.                  */
    /*  단, RTC 값이 아직 valid하지 않으면 date가 신뢰 불가능하므로             */
    /*  full sync로 자동 승격한다.                                            */
    /* ---------------------------------------------------------------------- */
    if ((full_sync == false) &&
        ((g_app_state.clock.rtc_time_valid == false) ||
         (g_app_state.clock.rtc_read_valid == false)))
    {
        full_sync = true;
    }

    if (full_sync != false)
    {
        rtc_utc_to_write = gps_utc;
    }
    else
    {
        rtc_utc_to_write = g_app_state.clock.utc;
        rtc_utc_to_write.hour = gps_utc.hour;
        rtc_utc_to_write.min = gps_utc.min;
        rtc_utc_to_write.sec = gps_utc.sec;
        rtc_utc_to_write.weekday = APP_CLOCK_ComputeWeekday(rtc_utc_to_write.year,
                                                            rtc_utc_to_write.month,
                                                            rtc_utc_to_write.day);
    }

    if (app_clock_write_utc_to_rtc_unlocked(&rtc_utc_to_write, now_ms) == false)
    {
        g_app_state.clock.gps_last_sync_success = false;
        return false;
    }

    g_app_state.clock.gps_last_sync_success = true;
    g_app_state.clock.gps_last_sync_was_full = full_sync;
    g_app_state.clock.gps_resolved_seen = true;
    g_app_state.clock.last_gps_sync_ms = now_ms;
    g_app_state.clock.next_gps_sync_due_ms =
        now_ms + ((uint32_t)g_app_state.settings.clock.gps_sync_interval_minutes * 60000u);

    if (full_sync != false)
    {
        g_app_state.clock.last_sync_source = (uint8_t)APP_CLOCK_SYNC_SOURCE_GPS_FULL;
        g_app_state.clock.gps_full_sync_count++;
    }
    else
    {
        g_app_state.clock.last_sync_source = (uint8_t)APP_CLOCK_SYNC_SOURCE_GPS_PERIODIC;
        g_app_state.clock.gps_periodic_sync_count++;
    }

    app_clock_set_rtc_validity_unlocked(true, now_ms);
    app_clock_refresh_runtime_snapshot_unlocked(now_ms);
    return true;
}

/* -------------------------------------------------------------------------- */
/*  public API: init                                                          */
/* -------------------------------------------------------------------------- */
void APP_CLOCK_Init(uint32_t now_ms)
{
    bool config_defaulted;

    s_app_clock_last_refresh_ms = 0u;
    s_app_clock_last_gps_resolved = false;
    s_app_clock_force_refresh = true;

    g_app_state.clock.initialized = true;
    g_app_state.clock.last_hal_status = (uint32_t)HAL_OK;

    config_defaulted = app_clock_load_backup_config_unlocked();
    if (config_defaulted != false)
    {
        app_clock_write_invalid_baseline_unlocked(now_ms);
    }

    app_clock_refresh_runtime_snapshot_unlocked(now_ms);

    /* ---------------------------------------------------------------------- */
    /*  backup config에 valid=false가 저장되어 있었다면                         */
    /*  RTC는 읽히더라도 UI가 신뢰 시간으로 쓰지 않도록 그대로 유지한다.        */
    /* ---------------------------------------------------------------------- */
    if (g_app_state.clock.rtc_time_valid == false)
    {
        g_app_state.clock.last_validity_change_ms = now_ms;
    }
}

/* -------------------------------------------------------------------------- */
/*  public API: force refresh                                                 */
/* -------------------------------------------------------------------------- */
void APP_CLOCK_ForceRefresh(uint32_t now_ms)
{
    s_app_clock_force_refresh = true;
    app_clock_refresh_runtime_snapshot_unlocked(now_ms);
}

/* -------------------------------------------------------------------------- */
/*  public API: periodic task                                                 */
/* -------------------------------------------------------------------------- */
void APP_CLOCK_Task(uint32_t now_ms)
{
    app_clock_calendar_t gps_candidate_utc;
    bool gps_resolved_now;
    bool periodic_due;

    if (g_app_state.clock.initialized == false)
    {
        return;
    }

    if ((s_app_clock_force_refresh != false) ||
        ((uint32_t)(now_ms - s_app_clock_last_refresh_ms) >= APP_CLOCK_RTC_REFRESH_MS))
    {
        app_clock_refresh_runtime_snapshot_unlocked(now_ms);
    }

    gps_resolved_now = app_clock_copy_gps_candidate_utc(&gps_candidate_utc);
    g_app_state.clock.gps_candidate_valid = gps_resolved_now;
    if (gps_resolved_now != false)
    {
        g_app_state.clock.last_gps_utc = gps_candidate_utc;
    }

    if ((gps_resolved_now != false) &&
        ((s_app_clock_last_gps_resolved == false) || (g_app_state.clock.rtc_time_valid == false)))
    {
        (void)app_clock_sync_from_gps_unlocked(true, now_ms);
    }
    else
    {
        periodic_due = false;

        if ((gps_resolved_now != false) &&
            (g_app_state.settings.clock.gps_auto_sync_enabled != 0u))
        {
            if ((uint32_t)g_app_state.settings.clock.gps_sync_interval_minutes == 0u)
            {
                periodic_due = false;
            }
            else if ((g_app_state.clock.last_gps_sync_ms == 0u) ||
                     ((uint32_t)(now_ms - g_app_state.clock.last_gps_sync_ms) >=
                      ((uint32_t)g_app_state.settings.clock.gps_sync_interval_minutes * 60000u)))
            {
                periodic_due = true;
            }
        }

        if (periodic_due != false)
        {
            (void)app_clock_sync_from_gps_unlocked(false, now_ms);
        }
    }

    s_app_clock_last_gps_resolved = gps_resolved_now;
}

/* -------------------------------------------------------------------------- */
/*  public API: manual local time set                                         */
/* -------------------------------------------------------------------------- */
bool APP_CLOCK_SetManualLocalTime(const app_clock_calendar_t *local_time,
                                  int8_t timezone_quarters,
                                  uint32_t now_ms)
{
    return app_clock_set_from_local_common(local_time,
                                           timezone_quarters,
                                           APP_CLOCK_SYNC_SOURCE_MANUAL,
                                           now_ms);
}

/* -------------------------------------------------------------------------- */
/*  public API: future external registration stub                             */
/*                                                                            */
/*  아직 "GPS 없이 외부에서 시간 등록" 기능은 UI/통신 경로가 확정되지 않았으므로 */
/*  우선은 같은 수동 설정 경로를 재사용하는 thin wrapper만 제공한다.            */
/* -------------------------------------------------------------------------- */
bool APP_CLOCK_RequestExternalRegistrationStub(const app_clock_calendar_t *local_time,
                                               int8_t timezone_quarters,
                                               uint32_t now_ms)
{
    return app_clock_set_from_local_common(local_time,
                                           timezone_quarters,
                                           APP_CLOCK_SYNC_SOURCE_EXTERNAL_STUB,
                                           now_ms);
}

/* -------------------------------------------------------------------------- */
/*  public API: immediate GPS full sync                                       */
/* -------------------------------------------------------------------------- */
bool APP_CLOCK_RequestGpsFullSyncNow(uint32_t now_ms)
{
    return app_clock_sync_from_gps_unlocked(true, now_ms);
}

/* -------------------------------------------------------------------------- */
/*  public API: immediate GPS time-only sync                                  */
/* -------------------------------------------------------------------------- */
bool APP_CLOCK_RequestGpsTimeOnlySyncNow(uint32_t now_ms)
{
    return app_clock_sync_from_gps_unlocked(false, now_ms);
}

/* -------------------------------------------------------------------------- */
/*  public API: timezone only change                                          */
/*                                                                            */
/*  하드웨어 RTC는 UTC 기준으로만 유지하므로                                    */
/*  timezone 변경은 RTC register를 건드리지 않고 metadata + local 파생값만      */
/*  업데이트한다.                                                              */
/* -------------------------------------------------------------------------- */
bool APP_CLOCK_SetTimezoneOnly(int8_t timezone_quarters, uint32_t now_ms)
{
    (void)now_ms;

    timezone_quarters = APP_CLOCK_ClampTimezoneQuarters((int32_t)timezone_quarters);

    g_app_state.settings.clock.timezone_quarters = timezone_quarters;
    app_clock_apply_settings_to_runtime_unlocked();
    app_clock_store_backup_config_unlocked();
    app_clock_update_local_from_utc_unlocked();
    return true;
}

/* -------------------------------------------------------------------------- */
/*  public API: auto GPS sync toggle                                          */
/* -------------------------------------------------------------------------- */
void APP_CLOCK_SetAutoGpsSyncEnabled(bool enabled, uint32_t now_ms)
{
    g_app_state.settings.clock.gps_auto_sync_enabled = (enabled != false) ? 1u : 0u;
    app_clock_apply_settings_to_runtime_unlocked();

    if (enabled != false)
    {
        g_app_state.clock.next_gps_sync_due_ms =
            now_ms + ((uint32_t)g_app_state.settings.clock.gps_sync_interval_minutes * 60000u);
    }
    else
    {
        g_app_state.clock.next_gps_sync_due_ms = 0u;
    }

    app_clock_store_backup_config_unlocked();
}

/* -------------------------------------------------------------------------- */
/*  public API: mark invalid                                                  */
/*                                                                            */
/*  RTC hardware counter는 계속 흘러가게 두고,                                  */
/*  "UI에서 신뢰 시간으로 표시할 수 있는가" 플래그만 false로 만든다.            */
/* -------------------------------------------------------------------------- */
void APP_CLOCK_MarkTimeInvalid(uint32_t now_ms)
{
    g_app_state.clock.last_sync_source = (uint8_t)APP_CLOCK_SYNC_SOURCE_NONE;
    g_app_state.clock.gps_last_sync_success = false;
    g_app_state.clock.gps_last_sync_was_full = false;
    app_clock_set_rtc_validity_unlocked(false, now_ms);
    app_clock_refresh_runtime_snapshot_unlocked(now_ms);
}
