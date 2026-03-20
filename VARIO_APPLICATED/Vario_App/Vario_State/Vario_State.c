#include "Vario_State.h"

#include "../Vario_Settings/Vario_Settings.h"

#include <math.h>
#include <string.h>

#ifndef VARIO_STATE_PRESSURE_TO_ALT_FACTOR_M
#define VARIO_STATE_PRESSURE_TO_ALT_FACTOR_M 44330.76923f
#endif

#ifndef VARIO_STATE_PRESSURE_TO_ALT_EXPONENT
#define VARIO_STATE_PRESSURE_TO_ALT_EXPONENT 0.190263f
#endif

#ifndef VARIO_STATE_MIN_DT_S
#define VARIO_STATE_MIN_DT_S 0.020f
#endif

#ifndef VARIO_STATE_MAX_DT_S
#define VARIO_STATE_MAX_DT_S 0.250f
#endif

#ifndef VARIO_STATE_ALTITUDE_TAU_S
#define VARIO_STATE_ALTITUDE_TAU_S 0.75f
#endif

#ifndef VARIO_STATE_VARIO_TAU_S
#define VARIO_STATE_VARIO_TAU_S 0.45f
#endif

#ifndef VARIO_STATE_GS_TAU_S
#define VARIO_STATE_GS_TAU_S 0.50f
#endif

#ifndef VARIO_STATE_MAX_ALT_JUMP_M
#define VARIO_STATE_MAX_ALT_JUMP_M 25.0f
#endif

#ifndef VARIO_STATE_MAX_VARIO_MPS
#define VARIO_STATE_MAX_VARIO_MPS 10.0f
#endif

#ifndef VARIO_STATE_VARIO_DEADBAND_MPS
#define VARIO_STATE_VARIO_DEADBAND_MPS 0.03f
#endif

static struct
{
    vario_mode_t    current_mode;
    vario_mode_t    previous_main_mode;
    uint8_t         settings_cursor;
    uint8_t         quickset_cursor;
    uint8_t         valuesetting_cursor;
    uint8_t         redraw_request;
    vario_runtime_t runtime;
} s_vario_state;

static float vario_state_lpf_alpha(float dt_s, float tau_s)
{
    if (tau_s <= 0.0f)
    {
        return 1.0f;
    }

    return dt_s / (tau_s + dt_s);
}

static float vario_state_pressure_to_altitude_m(float pressure_hpa, float qnh_hpa)
{
    if ((pressure_hpa <= 0.0f) || (qnh_hpa <= 0.0f))
    {
        return 0.0f;
    }

    return VARIO_STATE_PRESSURE_TO_ALT_FACTOR_M *
           (1.0f - powf((pressure_hpa / qnh_hpa), VARIO_STATE_PRESSURE_TO_ALT_EXPONENT));
}

static float vario_state_absf(float value)
{
    return (value < 0.0f) ? -value : value;
}

static float vario_state_clampf(float value, float min_v, float max_v)
{
    if (value < min_v)
    {
        return min_v;
    }

    if (value > max_v)
    {
        return max_v;
    }

    return value;
}

static void vario_state_capture_snapshots(void)
{
    APP_STATE_CopyGy86Snapshot(&s_vario_state.runtime.gy86);
    APP_STATE_CopyGpsSnapshot(&s_vario_state.runtime.gps);
    APP_STATE_CopyDs18b20Snapshot(&s_vario_state.runtime.ds18b20);
}

static void vario_state_update_derived_from_baro(void)
{
    const vario_settings_t *settings;
    const app_gy86_state_t *gy86;
    float                   pressure_hpa;
    float                   qnh_hpa;
    float                   raw_altitude_m;
    float                   dt_s;
    float                   alpha_alt;
    float                   alpha_vario;
    float                   previous_filtered_altitude_m;
    float                   raw_vario_mps;
    uint32_t                ts_ms;

    settings = Vario_Settings_Get();
    gy86     = &s_vario_state.runtime.gy86;

    if ((gy86->status_flags & APP_GY86_STATUS_BARO_VALID) == 0u)
    {
        s_vario_state.runtime.baro_valid = false;
        return;
    }

    if (gy86->baro.sample_count == s_vario_state.runtime.last_baro_sample_count)
    {
        return;
    }

    pressure_hpa   = ((float)gy86->baro.pressure_hpa_x100) * 0.01f;
    qnh_hpa        = ((float)settings->qnh_hpa_x100) * 0.01f;
    raw_altitude_m = vario_state_pressure_to_altitude_m(pressure_hpa, qnh_hpa);

    s_vario_state.runtime.pressure_hpa_x100 = gy86->baro.pressure_hpa_x100;
    s_vario_state.runtime.temperature_c_x100 = gy86->baro.temp_cdeg;
    s_vario_state.runtime.pressure_hpa = pressure_hpa;
    s_vario_state.runtime.temperature_c = ((float)gy86->baro.temp_cdeg) * 0.01f;
    s_vario_state.runtime.baro_valid = true;

    ts_ms = gy86->baro.timestamp_ms;

    if (s_vario_state.runtime.derived_valid == false)
    {
        s_vario_state.runtime.last_baro_timestamp_ms = ts_ms;
        s_vario_state.runtime.last_raw_altitude_m    = raw_altitude_m;
        s_vario_state.runtime.filtered_altitude_m    = raw_altitude_m;
        s_vario_state.runtime.filtered_vario_mps     = 0.0f;
        s_vario_state.runtime.baro_altitude_m        = raw_altitude_m;
        s_vario_state.runtime.baro_vario_mps         = 0.0f;
        s_vario_state.runtime.derived_valid          = true;
    }
    else
    {
        dt_s = ((float)(ts_ms - s_vario_state.runtime.last_baro_timestamp_ms)) * 0.001f;
        dt_s = vario_state_clampf(dt_s, VARIO_STATE_MIN_DT_S, VARIO_STATE_MAX_DT_S);

        if (vario_state_absf(raw_altitude_m - s_vario_state.runtime.last_raw_altitude_m) >
            VARIO_STATE_MAX_ALT_JUMP_M)
        {
            raw_altitude_m = s_vario_state.runtime.last_raw_altitude_m;
        }

        alpha_alt = vario_state_lpf_alpha(dt_s, VARIO_STATE_ALTITUDE_TAU_S);
        previous_filtered_altitude_m = s_vario_state.runtime.filtered_altitude_m;
        s_vario_state.runtime.filtered_altitude_m +=
            alpha_alt * (raw_altitude_m - s_vario_state.runtime.filtered_altitude_m);

        raw_vario_mps = (s_vario_state.runtime.filtered_altitude_m - previous_filtered_altitude_m) / dt_s;
        raw_vario_mps = vario_state_clampf(raw_vario_mps,
                                           -VARIO_STATE_MAX_VARIO_MPS,
                                           +VARIO_STATE_MAX_VARIO_MPS);

        alpha_vario = vario_state_lpf_alpha(dt_s, VARIO_STATE_VARIO_TAU_S);
        s_vario_state.runtime.filtered_vario_mps +=
            alpha_vario * (raw_vario_mps - s_vario_state.runtime.filtered_vario_mps);

        if (vario_state_absf(s_vario_state.runtime.filtered_vario_mps) < VARIO_STATE_VARIO_DEADBAND_MPS)
        {
            s_vario_state.runtime.filtered_vario_mps = 0.0f;
        }

        s_vario_state.runtime.last_baro_timestamp_ms = ts_ms;
        s_vario_state.runtime.last_raw_altitude_m    = raw_altitude_m;
        s_vario_state.runtime.baro_altitude_m        = s_vario_state.runtime.filtered_altitude_m;
        s_vario_state.runtime.baro_vario_mps         = s_vario_state.runtime.filtered_vario_mps;
    }

    s_vario_state.runtime.last_baro_sample_count = gy86->baro.sample_count;
    s_vario_state.redraw_request = 1u;
}

static void vario_state_update_derived_from_gps(void)
{
    const app_gps_state_t *gps;
    uint32_t               gps_ts_ms;
    float                  raw_ground_speed_kmh;
    float                  alpha_gs;

    gps = &s_vario_state.runtime.gps;

    if ((gps->fix.valid == false) ||
        (gps->fix.fixOk == false) ||
        (gps->fix.fixType == 0u))
    {
        s_vario_state.runtime.gps_valid = false;
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  현재 프로젝트의 gps_fix_basic_t 에는 host_time_ms 가 없고,              */
    /*  대신 APP_STATE publish 시각인 last_update_ms 와                        */
    /*  GPS epoch 기준 iTOW_ms 가 공개된다.                                    */
    /*                                                                        */
    /*  여기서는 "새 snapshot 이 들어왔는가" 와 "필터 dt" 를 위해              */
    /*  last_update_ms 를 사용한다.                                            */
    /* ---------------------------------------------------------------------- */
    gps_ts_ms = gps->fix.last_update_ms;

    if ((gps_ts_ms != 0u) &&
        (gps_ts_ms == s_vario_state.runtime.last_gps_host_time_ms))
    {
        return;
    }

    raw_ground_speed_kmh = ((float)gps->fix.gSpeed) * 0.0036f;
    raw_ground_speed_kmh = vario_state_clampf(raw_ground_speed_kmh, 0.0f, 180.0f);

    s_vario_state.runtime.gps_valid      = true;
    s_vario_state.runtime.gps_altitude_m = ((float)gps->fix.hMSL) * 0.001f;

    if ((s_vario_state.runtime.last_gps_host_time_ms == 0u) || (gps_ts_ms == 0u))
    {
        s_vario_state.runtime.filtered_ground_speed_kmh = raw_ground_speed_kmh;
    }
    else
    {
        float dt_s;

        dt_s = ((float)(gps_ts_ms - s_vario_state.runtime.last_gps_host_time_ms)) * 0.001f;
        dt_s = vario_state_clampf(dt_s, 0.020f, 0.300f);

        alpha_gs = vario_state_lpf_alpha(dt_s, VARIO_STATE_GS_TAU_S);
        s_vario_state.runtime.filtered_ground_speed_kmh +=
            alpha_gs * (raw_ground_speed_kmh - s_vario_state.runtime.filtered_ground_speed_kmh);
    }

    s_vario_state.runtime.ground_speed_kmh = s_vario_state.runtime.filtered_ground_speed_kmh;
    s_vario_state.runtime.last_gps_host_time_ms = gps_ts_ms;
    s_vario_state.redraw_request = 1u;
}

static void vario_state_update_temperature(void)
{
    const app_ds18b20_state_t *ds;
    bool                       valid;

    ds = &s_vario_state.runtime.ds18b20;

    valid = false;

    if (((ds->status_flags & APP_DS18B20_STATUS_VALID) != 0u) &&
        (ds->raw.temp_c_x100 != APP_DS18B20_TEMP_INVALID))
    {
        valid = true;
    }

    s_vario_state.runtime.temp_valid = valid;

    if (valid == false)
    {
        return;
    }

    if (ds->raw.sample_count == s_vario_state.runtime.last_temp_sample_count)
    {
        return;
    }

    s_vario_state.runtime.temperature_c_x100 = ds->raw.temp_c_x100;
    s_vario_state.runtime.temperature_c      = ((float)ds->raw.temp_c_x100) * 0.01f;
    s_vario_state.runtime.last_temp_sample_count = ds->raw.sample_count;
    s_vario_state.redraw_request = 1u;
}

static uint8_t vario_state_wrap_cursor(uint8_t cursor, uint8_t count, int8_t direction)
{
    int32_t next;

    next = (int32_t)cursor + (int32_t)direction;

    if (next < 0)
    {
        next = (int32_t)count - 1;
    }
    else if (next >= (int32_t)count)
    {
        next = 0;
    }

    return (uint8_t)next;
}

void Vario_State_Init(void)
{
    memset(&s_vario_state, 0, sizeof(s_vario_state));

    s_vario_state.current_mode       = VARIO_MODE_SCREEN_1;
    s_vario_state.previous_main_mode = VARIO_MODE_SCREEN_1;
    s_vario_state.redraw_request     = 1u;
}

void Vario_State_Task(uint32_t now_ms)
{
    s_vario_state.runtime.last_task_ms = now_ms;

    vario_state_capture_snapshots();
    vario_state_update_derived_from_baro();
    vario_state_update_derived_from_gps();
    vario_state_update_temperature();
}

vario_mode_t Vario_State_GetMode(void)
{
    return s_vario_state.current_mode;
}

void Vario_State_SetMode(vario_mode_t mode)
{
    if (mode >= VARIO_MODE_COUNT)
    {
        return;
    }

    s_vario_state.current_mode = mode;

    if ((mode == VARIO_MODE_SCREEN_1) ||
        (mode == VARIO_MODE_SCREEN_2) ||
        (mode == VARIO_MODE_SCREEN_3))
    {
        s_vario_state.previous_main_mode = mode;
    }

    s_vario_state.redraw_request = 1u;
}

void Vario_State_ReturnToMain(void)
{
    Vario_State_SetMode(s_vario_state.previous_main_mode);
}

void Vario_State_EnterSettings(void)
{
    Vario_State_SetMode(VARIO_MODE_SETTING);
}

void Vario_State_EnterQuickSet(void)
{
    Vario_State_SetMode(VARIO_MODE_QUICKSET);
}

void Vario_State_EnterValueSetting(void)
{
    Vario_State_SetMode(VARIO_MODE_VALUESETTING);
}

void Vario_State_SelectPreviousMainScreen(void)
{
    switch (s_vario_state.previous_main_mode)
    {
        case VARIO_MODE_SCREEN_1:
            Vario_State_SetMode(VARIO_MODE_SCREEN_3);
            break;

        case VARIO_MODE_SCREEN_2:
            Vario_State_SetMode(VARIO_MODE_SCREEN_1);
            break;

        case VARIO_MODE_SCREEN_3:
        default:
            Vario_State_SetMode(VARIO_MODE_SCREEN_2);
            break;
    }
}

void Vario_State_SelectNextMainScreen(void)
{
    switch (s_vario_state.previous_main_mode)
    {
        case VARIO_MODE_SCREEN_1:
            Vario_State_SetMode(VARIO_MODE_SCREEN_2);
            break;

        case VARIO_MODE_SCREEN_2:
            Vario_State_SetMode(VARIO_MODE_SCREEN_3);
            break;

        case VARIO_MODE_SCREEN_3:
        default:
            Vario_State_SetMode(VARIO_MODE_SCREEN_1);
            break;
    }
}

uint8_t Vario_State_GetSettingsCursor(void)
{
    return s_vario_state.settings_cursor;
}

uint8_t Vario_State_GetQuickSetCursor(void)
{
    return s_vario_state.quickset_cursor;
}

uint8_t Vario_State_GetValueSettingCursor(void)
{
    return s_vario_state.valuesetting_cursor;
}

void Vario_State_MoveSettingsCursor(int8_t direction)
{
    s_vario_state.settings_cursor =
        vario_state_wrap_cursor(s_vario_state.settings_cursor,
                                (uint8_t)VARIO_SETTING_MENU_COUNT,
                                direction);
    s_vario_state.redraw_request = 1u;
}

void Vario_State_MoveQuickSetCursor(int8_t direction)
{
    s_vario_state.quickset_cursor =
        vario_state_wrap_cursor(s_vario_state.quickset_cursor,
                                (uint8_t)VARIO_QUICKSET_ITEM_COUNT,
                                direction);
    s_vario_state.redraw_request = 1u;
}

void Vario_State_MoveValueSettingCursor(int8_t direction)
{
    s_vario_state.valuesetting_cursor =
        vario_state_wrap_cursor(s_vario_state.valuesetting_cursor,
                                (uint8_t)VARIO_VALUE_ITEM_COUNT,
                                direction);
    s_vario_state.redraw_request = 1u;
}

const vario_runtime_t *Vario_State_GetRuntime(void)
{
    return &s_vario_state.runtime;
}

void Vario_State_RequestRedraw(void)
{
    s_vario_state.redraw_request = 1u;
}

void Vario_State_ClearRedrawRequest(void)
{
    s_vario_state.redraw_request = 0u;
}

bool Vario_State_IsRedrawRequested(void)
{
    return (s_vario_state.redraw_request != 0u) ? true : false;
}
