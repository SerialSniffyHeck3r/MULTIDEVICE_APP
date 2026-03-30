#include "Vario_Settings.h"

#include "../../inc/Vario_State.h"

#include "APP_STATE.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>

static vario_settings_t s_vario_settings;

/* -------------------------------------------------------------------------- */
/*  Manual QNH compatibility / ownership policy                               */
/*                                                                            */
/*  canonical owner                                                            */
/*    APP_STATE.settings.altitude.manual_qnh_hpa_x100                         */
/*                                                                            */
/*  local mirror                                                               */
/*    s_vario_settings.qnh_hpa_x100                                           */
/*                                                                            */
/*  이유                                                                       */
/*  - 기존 상위 VARIO UI 코드가 settings pointer를 통해 QNH를 읽는 구조를      */
/*    한 번에 깨지 않고                                                       */
/*  - 실제 low-level altitude 서비스와의 source-of-truth는 APP_STATE로         */
/*    통일하기 위해서다.                                                      */
/* -------------------------------------------------------------------------- */
#define VARIO_SETTINGS_QNH_DEFAULT_HPA_X100       101325
#define VARIO_SETTINGS_QNH_RUNTIME_MIN_HPA_X100    95000
#define VARIO_SETTINGS_QNH_RUNTIME_MAX_HPA_X100   105500
#define VARIO_SETTINGS_QNH_SNAPSHOT_MIN_HPA_X100   80000
#define VARIO_SETTINGS_QNH_SNAPSHOT_MAX_HPA_X100  110000

#define VARIO_SETTINGS_PRESSURE_CORR_MIN_HPA_X100 (-2000)
#define VARIO_SETTINGS_PRESSURE_CORR_MAX_HPA_X100 (2000)

typedef enum
{
    VARIO_MENU_ITEM_QNH = 0u,
    VARIO_MENU_ITEM_PRESSURE_CORRECTION,
    VARIO_MENU_ITEM_ALT_UNIT,
    VARIO_MENU_ITEM_ALT2_MODE,
    VARIO_MENU_ITEM_ALT2_UNIT,
    VARIO_MENU_ITEM_VSPEED_UNIT,
    VARIO_MENU_ITEM_SPEED_UNIT,
    VARIO_MENU_ITEM_PRESSURE_UNIT,
    VARIO_MENU_ITEM_TEMPERATURE_UNIT,
    VARIO_MENU_ITEM_TIME_FORMAT,
    VARIO_MENU_ITEM_COORD_FORMAT,
    VARIO_MENU_ITEM_ALT_SOURCE,
    VARIO_MENU_ITEM_IMU_ASSIST,
    VARIO_MENU_ITEM_HEADING_SOURCE,
    VARIO_MENU_ITEM_BACKLIGHT_MODE,
    VARIO_MENU_ITEM_BRIGHTNESS,
    VARIO_MENU_ITEM_CONTRAST,
    VARIO_MENU_ITEM_TEMP_COMP,
    VARIO_MENU_ITEM_GS_TOP,
    VARIO_MENU_ITEM_PLOT_RANGE,
    VARIO_MENU_ITEM_PLOT_STEP,
    VARIO_MENU_ITEM_PLOT_DOT,
    VARIO_MENU_ITEM_PLOT_ARROW,
    VARIO_MENU_ITEM_AUDIO_ENABLE,
    VARIO_MENU_ITEM_AUDIO_VOLUME,
    VARIO_MENU_ITEM_BEEP_GATE,
    VARIO_MENU_ITEM_AUDIO_PROFILE,
    VARIO_MENU_ITEM_AUDIO_RESPONSE,
    VARIO_MENU_ITEM_AUDIO_UP_BASE,
    VARIO_MENU_ITEM_AUDIO_MOD_DEPTH,
    VARIO_MENU_ITEM_AUDIO_PITCH_CURVE,
    VARIO_MENU_ITEM_CLIMB_TONE,
    VARIO_MENU_ITEM_CLIMB_TONE_OFF,
    VARIO_MENU_ITEM_PRETHERMAL_MODE,
    VARIO_MENU_ITEM_PRETHERMAL,
    VARIO_MENU_ITEM_PRETHERMAL_OFF,
    VARIO_MENU_ITEM_SINK_TONE,
    VARIO_MENU_ITEM_SINK_TONE_OFF,
    VARIO_MENU_ITEM_SINK_CONT,
    VARIO_MENU_ITEM_SINK_CONT_OFF,
    VARIO_MENU_ITEM_LOG_ENABLE,
    VARIO_MENU_ITEM_LOG_INTERVAL,
    VARIO_MENU_ITEM_DAMPING,
    VARIO_MENU_ITEM_INT_AVG,
    VARIO_MENU_ITEM_FLIGHT_START,
    VARIO_MENU_ITEM_TRAINER,
    VARIO_MENU_ITEM_MANUAL_MC,
    VARIO_MENU_ITEM_FINAL_GLIDE_MARGIN,
    VARIO_MENU_ITEM_POLAR_V1,
    VARIO_MENU_ITEM_POLAR_S1,
    VARIO_MENU_ITEM_POLAR_V2,
    VARIO_MENU_ITEM_POLAR_S2,
    VARIO_MENU_ITEM_POLAR_V3,
    VARIO_MENU_ITEM_POLAR_S3,
    VARIO_MENU_ITEM_VARIO_SCALE,
    VARIO_MENU_ITEM_ALT2_CAPTURE,
    VARIO_MENU_ITEM_ALT3_RESET,
    VARIO_MENU_ITEM_ATTITUDE_RESET,
    VARIO_MENU_ITEM_FLIGHT_RESET,
    VARIO_MENU_ITEM_BT_ECHO,
    VARIO_MENU_ITEM_BT_AUTOPING,
    VARIO_MENU_ITEM_COUNT
} vario_menu_item_t;

static const vario_menu_item_t s_vario_category_system_items[] = {
    VARIO_MENU_ITEM_ALT_UNIT,
    VARIO_MENU_ITEM_VSPEED_UNIT,
    VARIO_MENU_ITEM_SPEED_UNIT,
    VARIO_MENU_ITEM_PRESSURE_UNIT,
    VARIO_MENU_ITEM_TEMPERATURE_UNIT,
    VARIO_MENU_ITEM_TIME_FORMAT,
    VARIO_MENU_ITEM_COORD_FORMAT,
};

static const vario_menu_item_t s_vario_category_display_items[] = {
    VARIO_MENU_ITEM_BACKLIGHT_MODE,
    VARIO_MENU_ITEM_BRIGHTNESS,
    VARIO_MENU_ITEM_CONTRAST,
    VARIO_MENU_ITEM_TEMP_COMP,
    VARIO_MENU_ITEM_VARIO_SCALE,
    VARIO_MENU_ITEM_GS_TOP,
};

static const vario_menu_item_t s_vario_category_audio_items[] = {
    VARIO_MENU_ITEM_AUDIO_ENABLE,
    VARIO_MENU_ITEM_AUDIO_VOLUME,
    VARIO_MENU_ITEM_BEEP_GATE,
    VARIO_MENU_ITEM_AUDIO_PROFILE,
    VARIO_MENU_ITEM_AUDIO_RESPONSE,
    VARIO_MENU_ITEM_AUDIO_UP_BASE,
    VARIO_MENU_ITEM_AUDIO_MOD_DEPTH,
    VARIO_MENU_ITEM_AUDIO_PITCH_CURVE,
    VARIO_MENU_ITEM_CLIMB_TONE,
    VARIO_MENU_ITEM_CLIMB_TONE_OFF,
    VARIO_MENU_ITEM_PRETHERMAL_MODE,
    VARIO_MENU_ITEM_PRETHERMAL,
    VARIO_MENU_ITEM_PRETHERMAL_OFF,
    VARIO_MENU_ITEM_SINK_TONE,
    VARIO_MENU_ITEM_SINK_TONE_OFF,
    VARIO_MENU_ITEM_SINK_CONT,
    VARIO_MENU_ITEM_SINK_CONT_OFF,
};

static const vario_menu_item_t s_vario_category_log_items[] = {
    VARIO_MENU_ITEM_LOG_ENABLE,
    VARIO_MENU_ITEM_LOG_INTERVAL,
    VARIO_MENU_ITEM_PLOT_RANGE,
    VARIO_MENU_ITEM_PLOT_STEP,
};

static const vario_menu_item_t s_vario_category_flight_items[] = {
    VARIO_MENU_ITEM_QNH,
    VARIO_MENU_ITEM_PRESSURE_CORRECTION,
    VARIO_MENU_ITEM_ALT2_MODE,
    VARIO_MENU_ITEM_ALT2_UNIT,
    VARIO_MENU_ITEM_IMU_ASSIST,
    VARIO_MENU_ITEM_HEADING_SOURCE,
    VARIO_MENU_ITEM_INT_AVG,
    VARIO_MENU_ITEM_FLIGHT_START,
    VARIO_MENU_ITEM_TRAINER,
    VARIO_MENU_ITEM_MANUAL_MC,
    VARIO_MENU_ITEM_FINAL_GLIDE_MARGIN,
    VARIO_MENU_ITEM_POLAR_V1,
    VARIO_MENU_ITEM_POLAR_S1,
    VARIO_MENU_ITEM_POLAR_V2,
    VARIO_MENU_ITEM_POLAR_S2,
    VARIO_MENU_ITEM_POLAR_V3,
    VARIO_MENU_ITEM_POLAR_S3,
    VARIO_MENU_ITEM_VARIO_SCALE,
    VARIO_MENU_ITEM_ALT2_CAPTURE,
    VARIO_MENU_ITEM_ALT3_RESET,
    VARIO_MENU_ITEM_ATTITUDE_RESET,
    VARIO_MENU_ITEM_FLIGHT_RESET,
};

static const vario_menu_item_t s_vario_category_bluetooth_items[] = {
    VARIO_MENU_ITEM_BT_ECHO,
    VARIO_MENU_ITEM_BT_AUTOPING,
};

static int32_t vario_settings_clamp_s32(int32_t value, int32_t min_v, int32_t max_v)
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

static int32_t vario_settings_clamp_manual_qnh_x100(int32_t qnh_hpa_x100)
{
    return vario_settings_clamp_s32(qnh_hpa_x100,
                                    VARIO_SETTINGS_QNH_RUNTIME_MIN_HPA_X100,
                                    VARIO_SETTINGS_QNH_RUNTIME_MAX_HPA_X100);
}

static int32_t vario_settings_read_manual_qnh_from_app_state(void)
{
    app_settings_t shared_settings;
    int32_t        qnh_hpa_x100;

    APP_STATE_CopySettingsSnapshot(&shared_settings);
    qnh_hpa_x100 = shared_settings.altitude.manual_qnh_hpa_x100;

    if ((qnh_hpa_x100 < VARIO_SETTINGS_QNH_SNAPSHOT_MIN_HPA_X100) ||
        (qnh_hpa_x100 > VARIO_SETTINGS_QNH_SNAPSHOT_MAX_HPA_X100))
    {
        /* ------------------------------------------------------------------ */
        /*  cold boot / partially initialized state 방어                       */
        /*                                                                    */
        /*  APP_STATE가 아직 기본값을 싣지 못했거나, 저장된 값이 비정상 범위면  */
        /*  local mirror의 기존 값 또는 안전 기본값으로 복귀한다.             */
        /* ------------------------------------------------------------------ */
        if ((s_vario_settings.qnh_hpa_x100 >= VARIO_SETTINGS_QNH_RUNTIME_MIN_HPA_X100) &&
            (s_vario_settings.qnh_hpa_x100 <= VARIO_SETTINGS_QNH_RUNTIME_MAX_HPA_X100))
        {
            return s_vario_settings.qnh_hpa_x100;
        }

        return VARIO_SETTINGS_QNH_DEFAULT_HPA_X100;
    }

    return vario_settings_clamp_manual_qnh_x100(qnh_hpa_x100);
}

static void vario_settings_store_manual_qnh_to_app_state(int32_t qnh_hpa_x100)
{
    app_settings_t shared_settings;
    int32_t        clamped_qnh_hpa_x100;

    clamped_qnh_hpa_x100 = vario_settings_clamp_manual_qnh_x100(qnh_hpa_x100);

    APP_STATE_CopySettingsSnapshot(&shared_settings);
    if (shared_settings.altitude.manual_qnh_hpa_x100 != clamped_qnh_hpa_x100)
    {
        shared_settings.altitude.manual_qnh_hpa_x100 = clamped_qnh_hpa_x100;
        APP_STATE_StoreSettingsSnapshot(&shared_settings);
    }

    /* ---------------------------------------------------------------------- */
    /*  legacy 화면 코드 호환용 local mirror 동기화                            */
    /* ---------------------------------------------------------------------- */
    s_vario_settings.qnh_hpa_x100 = clamped_qnh_hpa_x100;
}

static int32_t vario_settings_clamp_pressure_correction_x100(int32_t correction_hpa_x100)
{
    return vario_settings_clamp_s32(correction_hpa_x100,
                                    VARIO_SETTINGS_PRESSURE_CORR_MIN_HPA_X100,
                                    VARIO_SETTINGS_PRESSURE_CORR_MAX_HPA_X100);
}

static int32_t vario_settings_read_pressure_correction_from_app_state(void)
{
    app_settings_t shared_settings;

    APP_STATE_CopySettingsSnapshot(&shared_settings);
    return vario_settings_clamp_pressure_correction_x100((int32_t)shared_settings.altitude.pressure_correction_hpa_x100);
}

static void vario_settings_store_pressure_correction_to_app_state(int32_t correction_hpa_x100)
{
    app_settings_t shared_settings;
    int32_t        clamped_correction_hpa_x100;

    clamped_correction_hpa_x100 = vario_settings_clamp_pressure_correction_x100(correction_hpa_x100);

    APP_STATE_CopySettingsSnapshot(&shared_settings);
    if ((int32_t)shared_settings.altitude.pressure_correction_hpa_x100 != clamped_correction_hpa_x100)
    {
        shared_settings.altitude.pressure_correction_hpa_x100 = (int16_t)clamped_correction_hpa_x100;
        APP_STATE_StoreSettingsSnapshot(&shared_settings);
    }

    /* ------------------------------------------------------------------ */
    /* legacy Vario_Task 호환용 local mirror 동기화                        */
    /* ------------------------------------------------------------------ */
    s_vario_settings.pressure_correction_hpa_x100 = clamped_correction_hpa_x100;
}

static bool vario_settings_read_imu_assist_from_app_state(void)
{
    app_settings_t shared_settings;

    APP_STATE_CopySettingsSnapshot(&shared_settings);
    return (shared_settings.altitude.imu_aid_enabled != 0u) ? true : false;
}

static void vario_settings_store_imu_assist_to_app_state(bool enabled)
{
    app_settings_t shared_settings;
    uint8_t        enabled_u8;

    enabled_u8 = (enabled != false) ? 1u : 0u;

    APP_STATE_CopySettingsSnapshot(&shared_settings);
    if (shared_settings.altitude.imu_aid_enabled != enabled_u8)
    {
        shared_settings.altitude.imu_aid_enabled = enabled_u8;
        APP_STATE_StoreSettingsSnapshot(&shared_settings);
    }

    /* ------------------------------------------------------------------ */
    /* legacy Vario_Task 호환용 local mirror 동기화                        */
    /* ------------------------------------------------------------------ */
    s_vario_settings.imu_assist_mode = (enabled != false) ?
                                           VARIO_IMU_ASSIST_AUTO :
                                           VARIO_IMU_ASSIST_OFF;
}

static float vario_settings_get_pressure_correction_display_float(void)
{
    return Vario_Settings_PressureHpaToDisplayFloat(((float)vario_settings_read_pressure_correction_from_app_state()) * 0.01f);
}

static void vario_settings_format_pressure_correction_text(char *buf, size_t buf_len)
{
    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    snprintf(buf,
             buf_len,
             "%.2f %s",
             (double)vario_settings_get_pressure_correction_display_float(),
             Vario_Settings_GetPressureUnitText());
}

static const char *vario_settings_get_imu_assist_text(void)
{
    return (vario_settings_read_imu_assist_from_app_state() != false) ? "AUTO" : "OFF";
}

static uint16_t vario_settings_clamp_u16(uint16_t value, uint16_t min_v, uint16_t max_v)
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

static uint8_t vario_settings_clamp_u8(uint8_t value, uint8_t min_v, uint8_t max_v)
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

static void vario_settings_sanitize_sink_audio_band(void)
{
    /* ---------------------------------------------------------------------- */
    /*  sink audio band 계약                                                   */
    /*                                                                        */
    /*  sink_tone_threshold_cms        : sink audio가 처음 살아나는 지점       */
    /*  sink_continuous_threshold_cms  : 이 값 아래(더 음수)부터               */
    /*                                   연속 sink saw tone으로 전환           */
    /*                                                                        */
    /*  따라서 연속 sink 전환점은 항상                                         */
    /*      sink_continuous_threshold_cms <= sink_tone_threshold_cms          */
    /*  를 만족해야 한다.                                                      */
    /*                                                                        */
    /*  두 값이 같으면 sink chirp band를 사실상 끈 것으로 해석한다.           */
    /* ---------------------------------------------------------------------- */
    if (s_vario_settings.sink_continuous_threshold_cms > s_vario_settings.sink_tone_threshold_cms)
    {
        s_vario_settings.sink_continuous_threshold_cms = s_vario_settings.sink_tone_threshold_cms;
    }
}


static void vario_settings_sanitize_climb_audio_band(void)
{
    if (s_vario_settings.climb_tone_off_threshold_cms > s_vario_settings.climb_tone_threshold_cms)
    {
        s_vario_settings.climb_tone_off_threshold_cms = s_vario_settings.climb_tone_threshold_cms;
    }

    if (s_vario_settings.prethermal_threshold_cms > s_vario_settings.climb_tone_threshold_cms)
    {
        s_vario_settings.prethermal_threshold_cms = s_vario_settings.climb_tone_threshold_cms;
    }

    if (s_vario_settings.prethermal_off_threshold_cms > s_vario_settings.prethermal_threshold_cms)
    {
        s_vario_settings.prethermal_off_threshold_cms = s_vario_settings.prethermal_threshold_cms;
    }
}

static void vario_settings_sanitize_audio_thresholds(void)
{
    vario_settings_sanitize_climb_audio_band();
    vario_settings_sanitize_sink_audio_band();

    if (s_vario_settings.sink_tone_off_threshold_cms < s_vario_settings.sink_tone_threshold_cms)
    {
        s_vario_settings.sink_tone_off_threshold_cms = s_vario_settings.sink_tone_threshold_cms;
    }

    if (s_vario_settings.sink_continuous_off_threshold_cms < s_vario_settings.sink_continuous_threshold_cms)
    {
        s_vario_settings.sink_continuous_off_threshold_cms = s_vario_settings.sink_continuous_threshold_cms;
    }
}

static void vario_settings_sanitize_glide_computer(void)
{
    /* ---------------------------------------------------------------------- */
    /*  3점 polar + 수동 MC 계약                                               */
    /*                                                                        */
    /*  - speed1 < speed2 < speed3                                            */
    /*  - sink1 <= sink2 <= sink3                                             */
    /*  - 각 속도 포인트는 최소 5.0 km/h 이상 간격을 둔다.                    */
    /*  - sink 값은 +cm/s magnitude 로 저장한다.                              */
    /* ---------------------------------------------------------------------- */
    s_vario_settings.manual_mccready_cms =
        (int16_t)vario_settings_clamp_s32((int32_t)s_vario_settings.manual_mccready_cms,
                                          0,
                                          500);
    s_vario_settings.final_glide_safety_margin_m =
        vario_settings_clamp_u16(s_vario_settings.final_glide_safety_margin_m,
                                 0u,
                                 500u);

    s_vario_settings.polar_speed1_kmh_x10 =
        vario_settings_clamp_u16(s_vario_settings.polar_speed1_kmh_x10, 300u, 1200u);
    s_vario_settings.polar_speed2_kmh_x10 =
        vario_settings_clamp_u16(s_vario_settings.polar_speed2_kmh_x10, 350u, 1600u);
    s_vario_settings.polar_speed3_kmh_x10 =
        vario_settings_clamp_u16(s_vario_settings.polar_speed3_kmh_x10, 400u, 2200u);

    if (s_vario_settings.polar_speed2_kmh_x10 <= (uint16_t)(s_vario_settings.polar_speed1_kmh_x10 + 50u))
    {
        s_vario_settings.polar_speed2_kmh_x10 = (uint16_t)(s_vario_settings.polar_speed1_kmh_x10 + 50u);
    }
    if (s_vario_settings.polar_speed3_kmh_x10 <= (uint16_t)(s_vario_settings.polar_speed2_kmh_x10 + 50u))
    {
        s_vario_settings.polar_speed3_kmh_x10 = (uint16_t)(s_vario_settings.polar_speed2_kmh_x10 + 50u);
    }
    s_vario_settings.polar_speed3_kmh_x10 =
        vario_settings_clamp_u16(s_vario_settings.polar_speed3_kmh_x10, 400u, 2200u);

    s_vario_settings.polar_sink1_cms =
        (int16_t)vario_settings_clamp_s32((int32_t)s_vario_settings.polar_sink1_cms,
                                          30,
                                          300);
    s_vario_settings.polar_sink2_cms =
        (int16_t)vario_settings_clamp_s32((int32_t)s_vario_settings.polar_sink2_cms,
                                          40,
                                          500);
    s_vario_settings.polar_sink3_cms =
        (int16_t)vario_settings_clamp_s32((int32_t)s_vario_settings.polar_sink3_cms,
                                          60,
                                          800);

    if (s_vario_settings.polar_sink2_cms < s_vario_settings.polar_sink1_cms)
    {
        s_vario_settings.polar_sink2_cms = s_vario_settings.polar_sink1_cms;
    }
    if (s_vario_settings.polar_sink3_cms < s_vario_settings.polar_sink2_cms)
    {
        s_vario_settings.polar_sink3_cms = s_vario_settings.polar_sink2_cms;
    }
}

static uint8_t vario_settings_snap_vario_range_x10(uint8_t value_x10)
{
    /* ---------------------------------------------------------------------- */
    /*  좌측 VARIO side bar scale 은 현재 4.0 / 5.0 두 단계만 제공한다.       */
    /*                                                                        */
    /*  기존 firmware가 8.0(=80) 같은 옛 값을 들고 들어오더라도               */
    /*  새 UI 계약에 맞춰 안전하게 5.0 쪽으로 접어 주기 위한 helper 다.       */
    /* ---------------------------------------------------------------------- */
    return (value_x10 >= 50u) ? 50u : 40u;
}

static void vario_settings_toggle_u8(uint8_t *value)
{
    if (value == NULL)
    {
        return;
    }

    *value = (*value == 0u) ? 1u : 0u;
}

static void vario_settings_cycle_alt_unit_field(vario_alt_unit_t *value, int8_t direction)
{
    if ((value == NULL) || (direction == 0))
    {
        return;
    }

    *value = (*value == VARIO_ALT_UNIT_METER) ? VARIO_ALT_UNIT_FEET : VARIO_ALT_UNIT_METER;
}

static void vario_settings_cycle_vspeed_unit(int8_t direction)
{
    if (direction == 0)
    {
        return;
    }

    s_vario_settings.vspeed_unit =
        (s_vario_settings.vspeed_unit == VARIO_VSPEED_UNIT_MPS) ?
            VARIO_VSPEED_UNIT_FPM : VARIO_VSPEED_UNIT_MPS;
}

static void vario_settings_cycle_speed_unit(int8_t direction)
{
    int32_t next;

    if (direction == 0)
    {
        return;
    }

    next = (int32_t)s_vario_settings.speed_unit + ((direction > 0) ? 1 : -1);

    if (next < 0)
    {
        next = (int32_t)VARIO_SPEED_UNIT_COUNT - 1;
    }
    else if (next >= (int32_t)VARIO_SPEED_UNIT_COUNT)
    {
        next = 0;
    }

    s_vario_settings.speed_unit = (vario_speed_unit_t)next;
}

static void vario_settings_cycle_pressure_unit(int8_t direction)
{
    int32_t next;

    if (direction == 0)
    {
        return;
    }

    next = (int32_t)s_vario_settings.pressure_unit + ((direction > 0) ? 1 : -1);
    if (next < 0)
    {
        next = (int32_t)VARIO_PRESSURE_UNIT_COUNT - 1;
    }
    else if (next >= (int32_t)VARIO_PRESSURE_UNIT_COUNT)
    {
        next = 0;
    }

    s_vario_settings.pressure_unit = (vario_pressure_unit_t)next;
}

static void vario_settings_cycle_temperature_unit(int8_t direction)
{
    int32_t next;

    if (direction == 0)
    {
        return;
    }

    next = (int32_t)s_vario_settings.temperature_unit + ((direction > 0) ? 1 : -1);
    if (next < 0)
    {
        next = (int32_t)VARIO_TEMPERATURE_UNIT_COUNT - 1;
    }
    else if (next >= (int32_t)VARIO_TEMPERATURE_UNIT_COUNT)
    {
        next = 0;
    }

    s_vario_settings.temperature_unit = (vario_temperature_unit_t)next;
}

static void vario_settings_cycle_time_format(int8_t direction)
{
    int32_t next;

    if (direction == 0)
    {
        return;
    }

    next = (int32_t)s_vario_settings.time_format + ((direction > 0) ? 1 : -1);
    if (next < 0)
    {
        next = (int32_t)VARIO_TIME_FORMAT_COUNT - 1;
    }
    else if (next >= (int32_t)VARIO_TIME_FORMAT_COUNT)
    {
        next = 0;
    }

    s_vario_settings.time_format = (vario_time_format_t)next;
}

static void vario_settings_cycle_coord_format(int8_t direction)
{
    int32_t next;

    if (direction == 0)
    {
        return;
    }

    next = (int32_t)s_vario_settings.coord_format + ((direction > 0) ? 1 : -1);
    if (next < 0)
    {
        next = (int32_t)VARIO_COORD_FORMAT_COUNT - 1;
    }
    else if (next >= (int32_t)VARIO_COORD_FORMAT_COUNT)
    {
        next = 0;
    }

    s_vario_settings.coord_format = (vario_coord_format_t)next;
}

static void vario_settings_cycle_audio_profile(int8_t direction)
{
    int32_t next;

    if (direction == 0)
    {
        return;
    }

    next = (int32_t)s_vario_settings.audio_profile + ((direction > 0) ? 1 : -1);
    if (next < 0)
    {
        next = (int32_t)VARIO_AUDIO_PROFILE_COUNT - 1;
    }
    else if (next >= (int32_t)VARIO_AUDIO_PROFILE_COUNT)
    {
        next = 0;
    }

    s_vario_settings.audio_profile = (vario_audio_profile_t)next;
}

static void vario_settings_cycle_prethermal_mode(int8_t direction)
{
    int32_t next;

    if (direction == 0)
    {
        return;
    }

    next = (int32_t)s_vario_settings.prethermal_mode + ((direction > 0) ? 1 : -1);
    if (next < 0)
    {
        next = (int32_t)VARIO_PRETHERMAL_MODE_COUNT - 1;
    }
    else if (next >= (int32_t)VARIO_PRETHERMAL_MODE_COUNT)
    {
        next = 0;
    }

    s_vario_settings.prethermal_mode = (vario_prethermal_mode_t)next;
}

static void vario_settings_cycle_backlight_mode(int8_t direction)
{
    int32_t next;

    if (direction == 0)
    {
        return;
    }

    next = (int32_t)s_vario_settings.display_backlight_mode + ((direction > 0) ? 1 : -1);
    if (next < 0)
    {
        next = (int32_t)VARIO_BACKLIGHT_MODE_COUNT - 1;
    }
    else if (next >= (int32_t)VARIO_BACKLIGHT_MODE_COUNT)
    {
        next = 0;
    }

    s_vario_settings.display_backlight_mode = (vario_backlight_mode_t)next;
}

static void vario_settings_cycle_alt2_mode(int8_t direction)
{
    int32_t next;

    if (direction == 0)
    {
        return;
    }

    next = (int32_t)s_vario_settings.alt2_mode + ((direction > 0) ? 1 : -1);
    if (next < 0)
    {
        next = (int32_t)VARIO_ALT2_MODE_COUNT - 1;
    }
    else if (next >= (int32_t)VARIO_ALT2_MODE_COUNT)
    {
        next = 0;
    }

    s_vario_settings.alt2_mode = (vario_alt2_mode_t)next;
}

static void vario_settings_cycle_alt_source(int8_t direction)
{
    int32_t next;

    if (direction == 0)
    {
        return;
    }

    next = (int32_t)s_vario_settings.altitude_source + ((direction > 0) ? 1 : -1);

    if (next < 0)
    {
        next = (int32_t)VARIO_ALT_SOURCE_COUNT - 1;
    }
    else if (next >= (int32_t)VARIO_ALT_SOURCE_COUNT)
    {
        next = 0;
    }

    s_vario_settings.altitude_source = (vario_alt_source_t)next;
}

static void vario_settings_cycle_heading_source(int8_t direction)
{
    int32_t next;

    if (direction == 0)
    {
        return;
    }

    next = (int32_t)s_vario_settings.heading_source + ((direction > 0) ? 1 : -1);

    if (next < 0)
    {
        next = (int32_t)VARIO_HEADING_SOURCE_COUNT - 1;
    }
    else if (next >= (int32_t)VARIO_HEADING_SOURCE_COUNT)
    {
        next = 0;
    }

    s_vario_settings.heading_source = (vario_heading_source_t)next;
}

static void vario_settings_cycle_compass_span(int8_t direction)
{
    static const uint16_t k_spans[] = { 60u, 90u, 120u, 180u };
    uint8_t index;

    index = 0u;

    while ((index < (uint8_t)(sizeof(k_spans) / sizeof(k_spans[0]))) &&
           (k_spans[index] != s_vario_settings.compass_span_deg))
    {
        ++index;
    }

    if (index >= (uint8_t)(sizeof(k_spans) / sizeof(k_spans[0])))
    {
        index = 2u;
    }

    if (direction > 0)
    {
        index = (uint8_t)((index + 1u) % (uint8_t)(sizeof(k_spans) / sizeof(k_spans[0])));
    }
    else if (direction < 0)
    {
        index = (index == 0u) ?
                    (uint8_t)((sizeof(k_spans) / sizeof(k_spans[0])) - 1u) :
                    (uint8_t)(index - 1u);
    }
    else
    {
        return;
    }

    s_vario_settings.compass_span_deg = k_spans[index];
}


static const vario_menu_item_t *vario_settings_get_category_items(vario_settings_category_t category,
                                                                  uint8_t *out_count)
{
    const vario_menu_item_t *items;
    uint8_t                  count;

    items = NULL;
    count = 0u;

    switch (category)
    {
        case VARIO_SETTINGS_CATEGORY_SYSTEM:
            items = s_vario_category_system_items;
            count = (uint8_t)(sizeof(s_vario_category_system_items) / sizeof(s_vario_category_system_items[0]));
            break;

        case VARIO_SETTINGS_CATEGORY_DISPLAY:
            items = s_vario_category_display_items;
            count = (uint8_t)(sizeof(s_vario_category_display_items) / sizeof(s_vario_category_display_items[0]));
            break;

        case VARIO_SETTINGS_CATEGORY_AUDIO:
            items = s_vario_category_audio_items;
            count = (uint8_t)(sizeof(s_vario_category_audio_items) / sizeof(s_vario_category_audio_items[0]));
            break;

        case VARIO_SETTINGS_CATEGORY_LOG:
            items = s_vario_category_log_items;
            count = (uint8_t)(sizeof(s_vario_category_log_items) / sizeof(s_vario_category_log_items[0]));
            break;

        case VARIO_SETTINGS_CATEGORY_FLIGHT:
            items = s_vario_category_flight_items;
            count = (uint8_t)(sizeof(s_vario_category_flight_items) / sizeof(s_vario_category_flight_items[0]));
            break;

        case VARIO_SETTINGS_CATEGORY_BLUETOOTH:
            items = s_vario_category_bluetooth_items;
            count = (uint8_t)(sizeof(s_vario_category_bluetooth_items) / sizeof(s_vario_category_bluetooth_items[0]));
            break;

        case VARIO_SETTINGS_CATEGORY_COUNT:
        default:
            break;
    }

    if (out_count != NULL)
    {
        *out_count = count;
    }

    return items;
}

static bool vario_settings_get_category_item_by_index(vario_settings_category_t category,
                                                      uint8_t index,
                                                      vario_menu_item_t *out_item)
{
    const vario_menu_item_t *items;
    uint8_t                  count;

    items = vario_settings_get_category_items(category, &count);
    if ((items == NULL) || (out_item == NULL) || (index >= count))
    {
        return false;
    }

    *out_item = items[index];
    return true;
}

static const char *vario_settings_get_on_off_text(uint8_t value)
{
    return (value != 0u) ? "ON" : "OFF";
}

static void vario_settings_format_vspeed_threshold(char *buf, size_t buf_len, int16_t threshold_cms)
{
    float value_mps;
    float disp;

    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    value_mps = ((float)threshold_cms) * 0.01f;
    disp = Vario_Settings_VSpeedMpsToDisplayFloat(value_mps);

    if (s_vario_settings.vspeed_unit == VARIO_VSPEED_UNIT_FPM)
    {
        snprintf(buf, buf_len, "%+ld %s", (long)lroundf(disp), Vario_Settings_GetVSpeedUnitText());
    }
    else
    {
        snprintf(buf, buf_len, "%+.1f %s", (double)disp, Vario_Settings_GetVSpeedUnitText());
    }
}

static void vario_settings_format_speed_threshold(char *buf, size_t buf_len, uint16_t speed_kmh_x10)
{
    float speed_disp;

    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    speed_disp = Vario_Settings_SpeedKmhToDisplayFloat(((float)speed_kmh_x10) * 0.1f);
    snprintf(buf, buf_len, "%.1f %s", (double)speed_disp, Vario_Settings_GetSpeedUnitText());
}

static void vario_settings_format_positive_sink(char *buf, size_t buf_len, int16_t sink_cms)
{
    float sink_mps;
    float disp;

    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    sink_mps = ((float)sink_cms) * 0.01f;
    disp = Vario_Settings_VSpeedMpsToDisplayFloat(sink_mps);

    if (s_vario_settings.vspeed_unit == VARIO_VSPEED_UNIT_FPM)
    {
        snprintf(buf, buf_len, "%ld %s", (long)lroundf(disp), Vario_Settings_GetVSpeedUnitText());
    }
    else
    {
        snprintf(buf, buf_len, "%.2f %s", (double)disp, Vario_Settings_GetVSpeedUnitText());
    }
}

static void vario_settings_format_alt2_ref(char *buf, size_t buf_len, int32_t altitude_cm)
{
    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    snprintf(buf,
             buf_len,
             "%ld %s",
             (long)Vario_Settings_AltitudeMetersToDisplayRoundedWithUnit(((float)altitude_cm) * 0.01f,
                                                                         s_vario_settings.alt2_unit),
             Vario_Settings_GetAltitudeUnitTextForUnit(s_vario_settings.alt2_unit));
}

void Vario_Settings_Init(void)
{
    /* ---------------------------------------------------------------------- */
    /*  QNH는 local struct가 아니라 APP_STATE.settings가 canonical owner다.   */
    /*  init 시에는 current snapshot 값을 읽어 local compatibility mirror만   */
    /*  맞춰 둔다.                                                             */
    /* ---------------------------------------------------------------------- */
    s_vario_settings.qnh_hpa_x100                  = vario_settings_read_manual_qnh_from_app_state();
    s_vario_settings.pressure_correction_hpa_x100  = vario_settings_read_pressure_correction_from_app_state();
    s_vario_settings.alt2_reference_cm             = 0;
    s_vario_settings.altitude_unit                 = VARIO_ALT_UNIT_METER;
    s_vario_settings.alt2_unit                     = VARIO_ALT_UNIT_FEET;
    s_vario_settings.vspeed_unit                   = VARIO_VSPEED_UNIT_MPS;
    s_vario_settings.speed_unit                    = VARIO_SPEED_UNIT_KMH;
    s_vario_settings.pressure_unit                 = VARIO_PRESSURE_UNIT_HPA;
    s_vario_settings.temperature_unit              = VARIO_TEMPERATURE_UNIT_C;
    s_vario_settings.time_format                   = VARIO_TIME_FORMAT_24H;
    s_vario_settings.coord_format                  = VARIO_COORD_FORMAT_DDMM_MMM;
    s_vario_settings.alt2_mode                     = VARIO_ALT2_MODE_RELATIVE;
    s_vario_settings.altitude_source               = VARIO_ALT_SOURCE_DISPLAY;
    s_vario_settings.heading_source                = VARIO_HEADING_SOURCE_AUTO;
    s_vario_settings.imu_assist_mode               = (vario_settings_read_imu_assist_from_app_state() != false) ?
                                                         VARIO_IMU_ASSIST_AUTO :
                                                         VARIO_IMU_ASSIST_OFF;
    s_vario_settings.audio_enabled                 = 1u;
    s_vario_settings.audio_volume_percent          = 75u;
    s_vario_settings.beep_only_when_flying         = 1u;
    s_vario_settings.audio_profile                 = VARIO_AUDIO_PROFILE_SOFT_SPEAKER;
    s_vario_settings.prethermal_mode               = VARIO_PRETHERMAL_MODE_BUZZER;
    s_vario_settings.audio_response_level          = 8u;
    s_vario_settings.audio_up_base_hz              = 700u;
    s_vario_settings.audio_modulation_depth_percent = 100u;
    s_vario_settings.audio_pitch_curve_percent     = 100u;
    s_vario_settings.display_backlight_mode       = VARIO_BACKLIGHT_MODE_AUTO_CONTINUOUS;
    s_vario_settings.display_brightness_percent    = 70u;
    s_vario_settings.display_contrast_raw          = 160u;
    s_vario_settings.display_temp_compensation     = 1u;
    /* ------------------------------------------------------------------ */
    /*  default response / tone thresholds                                  */
    /*                                                                      */
    /*  - response 8/10 : fast-trigger backbone을 살리는 쪽으로 한 단계 업   */
    /*  - damping  8/10 : fast tail은 짧고, backbone은 아직 제품답게 유지    */
    /*  - climb +0.12 m/s : 20~30cm급 작은 들림에서 첫 삑을 더 빨리 유도      */
    /*  - climb off +0.05 m/s : tone hysteresis는 남겨 chatter를 억제        */
    /*  - prethermal +0.03 m/s : 미세한 살아남음을 조금 더 빨리 알려 줌      */
    /*  - sink start -1.00 m/s : bench 잡음까지 sink tone이 울리지는 않게     */
    /*  - sink continuous -2.50 m/s : 그 위 대역은 Digifly식 짧은 sink chirp */
    /* ------------------------------------------------------------------ */
    s_vario_settings.vario_damping_level           = 8u;
    s_vario_settings.digital_vario_average_seconds = 5u;
    s_vario_settings.climb_tone_threshold_cms      = 12;
    s_vario_settings.climb_tone_off_threshold_cms  = 5;
    s_vario_settings.prethermal_threshold_cms      = 3;
    s_vario_settings.prethermal_off_threshold_cms  = 0;
    s_vario_settings.sink_tone_threshold_cms       = -100;
    s_vario_settings.sink_tone_off_threshold_cms   = -80;
    s_vario_settings.sink_continuous_threshold_cms = -250;
    s_vario_settings.sink_continuous_off_threshold_cms = -180;
    s_vario_settings.flight_start_speed_kmh_x10    = 50u;
    s_vario_settings.manual_mccready_cms           = 150;
    s_vario_settings.final_glide_safety_margin_m   = 120u;
    s_vario_settings.polar_speed1_kmh_x10          = 700u;
    s_vario_settings.polar_sink1_cms               = 65;
    s_vario_settings.polar_speed2_kmh_x10          = 950u;
    s_vario_settings.polar_sink2_cms               = 78;
    s_vario_settings.polar_speed3_kmh_x10          = 1300u;
    s_vario_settings.polar_sink3_cms               = 120;
    s_vario_settings.log_enabled                   = 1u;
    s_vario_settings.log_interval_seconds          = 1u;
    s_vario_settings.bluetooth_echo_enabled        = 0u;
    s_vario_settings.bluetooth_auto_ping_enabled   = 0u;
    s_vario_settings.compass_span_deg              = 120u;
    s_vario_settings.compass_box_height_px         = 16u;
    s_vario_settings.vario_range_mps_x10           = 40u;
    s_vario_settings.gs_range_kmh                  = 80u;
    s_vario_settings.trail_range_m                 = 250u;
    s_vario_settings.trail_spacing_m               = 15u;
    s_vario_settings.trail_dot_size_px             = 2u;
    s_vario_settings.arrow_size_px                 = 9u;
    s_vario_settings.show_current_time             = 1u;
    s_vario_settings.show_flight_time              = 1u;
    s_vario_settings.show_max_vario                = 1u;
    s_vario_settings.show_gs_bar                   = 1u;
    s_vario_settings.trainer_enabled               = 0u;

    vario_settings_sanitize_audio_thresholds();
    vario_settings_sanitize_glide_computer();
}

const vario_settings_t *Vario_Settings_Get(void)
{
    /* ---------------------------------------------------------------------- */
    /*  legacy 코드가 settings pointer의 mirror 멤버를 그대로 읽더라도        */
    /*  stale 값이 남지 않도록 getter 진입 시점에 동기화한다.                 */
    /* ---------------------------------------------------------------------- */
    s_vario_settings.qnh_hpa_x100 = vario_settings_read_manual_qnh_from_app_state();
    s_vario_settings.pressure_correction_hpa_x100 = vario_settings_read_pressure_correction_from_app_state();
    s_vario_settings.imu_assist_mode = (vario_settings_read_imu_assist_from_app_state() != false) ?
                                           VARIO_IMU_ASSIST_AUTO :
                                           VARIO_IMU_ASSIST_OFF;
    return &s_vario_settings;
}

int32_t Vario_Settings_GetManualQnhHpaX100(void)
{
    s_vario_settings.qnh_hpa_x100 = vario_settings_read_manual_qnh_from_app_state();
    return s_vario_settings.qnh_hpa_x100;
}

void Vario_Settings_SetManualQnhHpaX100(int32_t qnh_hpa_x100)
{
    vario_settings_store_manual_qnh_to_app_state(qnh_hpa_x100);
}

int32_t Vario_Settings_GetPressureCorrectionHpaX100(void)
{
    s_vario_settings.pressure_correction_hpa_x100 = vario_settings_read_pressure_correction_from_app_state();
    return s_vario_settings.pressure_correction_hpa_x100;
}

void Vario_Settings_SetPressureCorrectionHpaX100(int32_t correction_hpa_x100)
{
    vario_settings_store_pressure_correction_to_app_state(correction_hpa_x100);
}

void Vario_Settings_AdjustQuickSet(vario_quickset_item_t item, int8_t direction)
{
    const vario_runtime_t *rt;
    int32_t                qnh_step_x100;
    int32_t                pressure_correction_step_x100;

    rt = Vario_State_GetRuntime();
    qnh_step_x100 = (s_vario_settings.pressure_unit == VARIO_PRESSURE_UNIT_INHG) ? 34 : 10;
    pressure_correction_step_x100 = (s_vario_settings.pressure_unit == VARIO_PRESSURE_UNIT_INHG) ? 34 : 1;

    switch (item)
    {
        case VARIO_QUICKSET_ITEM_QNH:
            Vario_Settings_SetManualQnhHpaX100(Vario_Settings_GetManualQnhHpaX100() +
                                               ((int32_t)direction * qnh_step_x100));
            break;

        case VARIO_QUICKSET_ITEM_PRESSURE_CORRECTION:
            Vario_Settings_SetPressureCorrectionHpaX100(Vario_Settings_GetPressureCorrectionHpaX100() +
                                                        ((int32_t)direction * pressure_correction_step_x100));
            break;

        case VARIO_QUICKSET_ITEM_ALT_UNIT:
            vario_settings_cycle_alt_unit_field(&s_vario_settings.altitude_unit, direction);
            break;

        case VARIO_QUICKSET_ITEM_ALT2_MODE:
            vario_settings_cycle_alt2_mode(direction);
            break;

        case VARIO_QUICKSET_ITEM_ALT2_UNIT:
            vario_settings_cycle_alt_unit_field(&s_vario_settings.alt2_unit, direction);
            break;

        case VARIO_QUICKSET_ITEM_VSPEED_UNIT:
            vario_settings_cycle_vspeed_unit(direction);
            break;

        case VARIO_QUICKSET_ITEM_SPEED_UNIT:
            vario_settings_cycle_speed_unit(direction);
            break;

        case VARIO_QUICKSET_ITEM_PRESSURE_UNIT:
            vario_settings_cycle_pressure_unit(direction);
            break;

        case VARIO_QUICKSET_ITEM_TEMPERATURE_UNIT:
            vario_settings_cycle_temperature_unit(direction);
            break;

        case VARIO_QUICKSET_ITEM_TIME_FORMAT:
            vario_settings_cycle_time_format(direction);
            break;

        case VARIO_QUICKSET_ITEM_COORD_FORMAT:
            vario_settings_cycle_coord_format(direction);
            break;

        case VARIO_QUICKSET_ITEM_ALT_SOURCE:
            vario_settings_cycle_alt_source(direction);
            break;

        case VARIO_QUICKSET_ITEM_IMU_ASSIST:
            if (direction != 0)
            {
                vario_settings_store_imu_assist_to_app_state(vario_settings_read_imu_assist_from_app_state() == false);
            }
            break;

        case VARIO_QUICKSET_ITEM_HEADING_SOURCE:
            vario_settings_cycle_heading_source(direction);
            break;

        case VARIO_QUICKSET_ITEM_VARIO_DAMPING:
            s_vario_settings.vario_damping_level =
                vario_settings_clamp_u8((uint8_t)((int32_t)s_vario_settings.vario_damping_level +
                                                  (int32_t)direction),
                                        1u,
                                        10u);
            break;

        case VARIO_QUICKSET_ITEM_AUDIO_RESPONSE:
            s_vario_settings.audio_response_level =
                vario_settings_clamp_u8((uint8_t)((int32_t)s_vario_settings.audio_response_level +
                                                  (int32_t)direction),
                                        1u,
                                        10u);
            break;

        case VARIO_QUICKSET_ITEM_VARIO_AVG_SECONDS:
            s_vario_settings.digital_vario_average_seconds =
                vario_settings_clamp_u8((uint8_t)((int32_t)s_vario_settings.digital_vario_average_seconds +
                                                  (int32_t)direction),
                                        1u,
                                        30u);
            break;

        case VARIO_QUICKSET_ITEM_AUDIO_PROFILE:
            vario_settings_cycle_audio_profile(direction);
            break;

        case VARIO_QUICKSET_ITEM_AUDIO_UP_BASE_HZ:
            s_vario_settings.audio_up_base_hz =
                vario_settings_clamp_u16((uint16_t)((int32_t)s_vario_settings.audio_up_base_hz +
                                                    ((int32_t)direction * 20)),
                                         450u,
                                         1400u);
            break;

        case VARIO_QUICKSET_ITEM_AUDIO_MOD_DEPTH:
            s_vario_settings.audio_modulation_depth_percent =
                vario_settings_clamp_u8((uint8_t)((int32_t)s_vario_settings.audio_modulation_depth_percent +
                                                  ((int32_t)direction * 5)),
                                        50u,
                                        150u);
            break;

        case VARIO_QUICKSET_ITEM_AUDIO_PITCH_CURVE:
            s_vario_settings.audio_pitch_curve_percent =
                vario_settings_clamp_u8((uint8_t)((int32_t)s_vario_settings.audio_pitch_curve_percent +
                                                  ((int32_t)direction * 5)),
                                        50u,
                                        150u);
            break;

        case VARIO_QUICKSET_ITEM_CLIMB_TONE_THRESHOLD:
            s_vario_settings.climb_tone_threshold_cms =
                (int16_t)vario_settings_clamp_s32((int32_t)s_vario_settings.climb_tone_threshold_cms +
                                                  ((int32_t)direction * 5),
                                                  0,
                                                  300);
            vario_settings_sanitize_audio_thresholds();
            break;

        case VARIO_QUICKSET_ITEM_CLIMB_TONE_OFF_THRESHOLD:
            s_vario_settings.climb_tone_off_threshold_cms =
                (int16_t)vario_settings_clamp_s32((int32_t)s_vario_settings.climb_tone_off_threshold_cms +
                                                  ((int32_t)direction * 5),
                                                  0,
                                                  300);
            vario_settings_sanitize_audio_thresholds();
            break;

        case VARIO_QUICKSET_ITEM_PRETHERMAL_MODE:
            vario_settings_cycle_prethermal_mode(direction);
            break;

        case VARIO_QUICKSET_ITEM_PRETHERMAL_THRESHOLD:
            s_vario_settings.prethermal_threshold_cms =
                (int16_t)vario_settings_clamp_s32((int32_t)s_vario_settings.prethermal_threshold_cms +
                                                  ((int32_t)direction * 5),
                                                  -100,
                                                  300);
            vario_settings_sanitize_audio_thresholds();
            break;

        case VARIO_QUICKSET_ITEM_PRETHERMAL_OFF_THRESHOLD:
            s_vario_settings.prethermal_off_threshold_cms =
                (int16_t)vario_settings_clamp_s32((int32_t)s_vario_settings.prethermal_off_threshold_cms +
                                                  ((int32_t)direction * 5),
                                                  -100,
                                                  300);
            vario_settings_sanitize_audio_thresholds();
            break;

        case VARIO_QUICKSET_ITEM_SINK_TONE_THRESHOLD:
            s_vario_settings.sink_tone_threshold_cms =
                (int16_t)vario_settings_clamp_s32((int32_t)s_vario_settings.sink_tone_threshold_cms +
                                                  ((int32_t)direction * 10),
                                                  -500,
                                                  0);
            vario_settings_sanitize_audio_thresholds();
            break;

        case VARIO_QUICKSET_ITEM_SINK_TONE_OFF_THRESHOLD:
            s_vario_settings.sink_tone_off_threshold_cms =
                (int16_t)vario_settings_clamp_s32((int32_t)s_vario_settings.sink_tone_off_threshold_cms +
                                                  ((int32_t)direction * 10),
                                                  -500,
                                                  0);
            vario_settings_sanitize_audio_thresholds();
            break;

        case VARIO_QUICKSET_ITEM_SINK_CONT_THRESHOLD:
            s_vario_settings.sink_continuous_threshold_cms =
                (int16_t)vario_settings_clamp_s32((int32_t)s_vario_settings.sink_continuous_threshold_cms +
                                                  ((int32_t)direction * 10),
                                                  -800,
                                                  0);
            vario_settings_sanitize_audio_thresholds();
            break;

        case VARIO_QUICKSET_ITEM_SINK_CONT_OFF_THRESHOLD:
            s_vario_settings.sink_continuous_off_threshold_cms =
                (int16_t)vario_settings_clamp_s32((int32_t)s_vario_settings.sink_continuous_off_threshold_cms +
                                                  ((int32_t)direction * 10),
                                                  -800,
                                                  0);
            vario_settings_sanitize_audio_thresholds();
            break;

        case VARIO_QUICKSET_ITEM_FLIGHT_START_SPEED:
            s_vario_settings.flight_start_speed_kmh_x10 =
                vario_settings_clamp_u16((uint16_t)((int32_t)s_vario_settings.flight_start_speed_kmh_x10 +
                                                    ((int32_t)direction * 5)),
                                         20u,
                                         250u);
            break;

        case VARIO_QUICKSET_ITEM_BEEP_ONLY_WHEN_FLYING:
            if (direction != 0)
            {
                vario_settings_toggle_u8(&s_vario_settings.beep_only_when_flying);
            }
            break;

        case VARIO_QUICKSET_ITEM_AUDIO_ENABLE:
            if (direction != 0)
            {
                vario_settings_toggle_u8(&s_vario_settings.audio_enabled);
            }
            break;

        case VARIO_QUICKSET_ITEM_AUDIO_VOLUME:
            s_vario_settings.audio_volume_percent =
                vario_settings_clamp_u8((uint8_t)((int32_t)s_vario_settings.audio_volume_percent +
                                                  ((int32_t)direction * 5)),
                                        0u,
                                        100u);
            break;

        case VARIO_QUICKSET_ITEM_TRAINER:
            /* -------------------------------------------------------------- */
            /*  TRAINER toggle 는 단순 on/off 스위치다.                       */
            /*  direction 부호만 보고 토글해서 root setup, valuesetting,       */
            /*  추후 direct caller가 모두 같은 동작을 보게 한다.              */
            /* -------------------------------------------------------------- */
            if (direction != 0)
            {
                s_vario_settings.trainer_enabled =
                    (s_vario_settings.trainer_enabled == 0u) ? 1u : 0u;
            }
            break;

        case VARIO_QUICKSET_ITEM_ALT2_CAPTURE:
        {
            int32_t selected_altitude_cm;

            if ((direction != 0) &&
                (Vario_State_GetSelectedAltitudeCm(&selected_altitude_cm) != false))
            {
                /* ---------------------------------------------------------- */
                /*  ALT2 capture는 표시용 1m 양자화값이 아니라                */
                /*  altitude_source 기준 canonical centimeter source를         */
                /*  그대로 캡처한다.                                          */
                /*                                                            */
                /*  이렇게 해야                                              */
                /*  - meter / feet 표시가 각자 자기 해상도를 유지하고         */
                /*  - 상대고도 기준점도 upper display rounding 오차를         */
                /*    물고 들어오지 않는다.                                   */
                /* ---------------------------------------------------------- */
                s_vario_settings.alt2_reference_cm = selected_altitude_cm;
            }
            else if ((direction != 0) && (rt != NULL) && (rt->derived_valid != false))
            {
                /* ---------------------------------------------------------- */
                /*  방어 fallback                                             */
                /*                                                            */
                /*  runtime source resolve이 실패한 아주 초기 부팅 구간에서만 */
                /*  기존 경로를 남겨 둔다.                                    */
                /* ---------------------------------------------------------- */
                s_vario_settings.alt2_reference_cm = (int32_t)lroundf(rt->alt1_absolute_m * 100.0f);
            }
            break;
        }

        case VARIO_QUICKSET_ITEM_ALT3_RESET:
            if (direction != 0)
            {
                Vario_State_ResetAccumulatedGain();
            }
            break;

        case VARIO_QUICKSET_ITEM_ATTITUDE_RESET:
            if (direction != 0)
            {
                Vario_State_ResetAttitudeIndicator();
            }
            break;

        case VARIO_QUICKSET_ITEM_FLIGHT_RESET:
            if (direction != 0)
            {
                Vario_State_ResetFlightMetrics();
            }
            break;

        case VARIO_QUICKSET_ITEM_COUNT:
        default:
            break;
    }
}

void Vario_Settings_AdjustValue(vario_value_item_t item, int8_t direction)
{
    switch (item)
    {
        case VARIO_VALUE_ITEM_BRIGHTNESS:
            s_vario_settings.display_backlight_mode = VARIO_BACKLIGHT_MODE_MANUAL;
            s_vario_settings.display_brightness_percent =
                vario_settings_clamp_u8((uint8_t)((int32_t)s_vario_settings.display_brightness_percent +
                                                  ((int32_t)direction * 5)),
                                        5u,
                                        100u);
            break;

        case VARIO_VALUE_ITEM_COMPASS_SPAN:
            vario_settings_cycle_compass_span(direction);
            break;

        case VARIO_VALUE_ITEM_COMPASS_BOX_HEIGHT:
            s_vario_settings.compass_box_height_px =
                vario_settings_clamp_u8((uint8_t)((int32_t)s_vario_settings.compass_box_height_px +
                                                  (int32_t)direction),
                                        14u,
                                        22u);
            break;

        case VARIO_VALUE_ITEM_VARIO_RANGE:
        {
            uint8_t next_range_x10;

            /* -------------------------------------------------------------- */
            /*  VARIO scale 은 4.0 / 5.0 두 단계만 제공한다.                 */
            /*                                                                */
            /*  - + 방향 입력 : 5.0 쪽으로                                    */
            /*  - - 방향 입력 : 4.0 쪽으로                                    */
            /*  - 그 외의 저장값이 들어와도 먼저 4.0/5.0으로 snap 한 뒤       */
            /*    다음 연산을 수행한다.                                       */
            /* -------------------------------------------------------------- */
            next_range_x10 = vario_settings_snap_vario_range_x10(s_vario_settings.vario_range_mps_x10);
            next_range_x10 = vario_settings_clamp_u8((uint8_t)((int32_t)next_range_x10 +
                                                               ((int32_t)direction * 10)),
                                                     40u,
                                                     50u);
            s_vario_settings.vario_range_mps_x10 = vario_settings_snap_vario_range_x10(next_range_x10);
            break;
        }

        case VARIO_VALUE_ITEM_GS_RANGE:
            s_vario_settings.gs_range_kmh =
                vario_settings_clamp_u16((uint16_t)((int32_t)s_vario_settings.gs_range_kmh +
                                                    ((int32_t)direction * 10)),
                                         30u,
                                         150u);
            break;

        case VARIO_VALUE_ITEM_TRAIL_RANGE:
            s_vario_settings.trail_range_m =
                vario_settings_clamp_u16((uint16_t)((int32_t)s_vario_settings.trail_range_m +
                                                    ((int32_t)direction * 50)),
                                         100u,
                                         1000u);
            break;

        case VARIO_VALUE_ITEM_TRAIL_SPACING:
            s_vario_settings.trail_spacing_m =
                vario_settings_clamp_u16((uint16_t)((int32_t)s_vario_settings.trail_spacing_m +
                                                    ((int32_t)direction * 5)),
                                         5u,
                                         50u);
            break;

        case VARIO_VALUE_ITEM_TRAIL_DOT_SIZE:
            s_vario_settings.trail_dot_size_px =
                vario_settings_clamp_u8((uint8_t)((int32_t)s_vario_settings.trail_dot_size_px +
                                                  (int32_t)direction),
                                        1u,
                                        3u);
            break;

        case VARIO_VALUE_ITEM_ARROW_SIZE:
            s_vario_settings.arrow_size_px =
                vario_settings_clamp_u8((uint8_t)((int32_t)s_vario_settings.arrow_size_px +
                                                  (int32_t)direction),
                                        6u,
                                        16u);
            break;

        case VARIO_VALUE_ITEM_SHOW_TIME:
            if (direction != 0)
            {
                vario_settings_toggle_u8(&s_vario_settings.show_current_time);
            }
            break;

        case VARIO_VALUE_ITEM_SHOW_FLIGHT_TIME:
            if (direction != 0)
            {
                vario_settings_toggle_u8(&s_vario_settings.show_flight_time);
            }
            break;

        case VARIO_VALUE_ITEM_SHOW_MAX_VARIO:
            if (direction != 0)
            {
                vario_settings_toggle_u8(&s_vario_settings.show_max_vario);
            }
            break;

        case VARIO_VALUE_ITEM_SHOW_GS_BAR:
            if (direction != 0)
            {
                vario_settings_toggle_u8(&s_vario_settings.show_gs_bar);
            }
            break;

        case VARIO_VALUE_ITEM_COUNT:
        default:
            break;
    }
}

float Vario_Settings_PressureHpaToDisplayFloat(float pressure_hpa)
{
    if (s_vario_settings.pressure_unit == VARIO_PRESSURE_UNIT_INHG)
    {
        return pressure_hpa * 0.0295299831f;
    }

    return pressure_hpa;
}

float Vario_Settings_TemperatureCToDisplayFloat(float temperature_c)
{
    if (s_vario_settings.temperature_unit == VARIO_TEMPERATURE_UNIT_F)
    {
        return (temperature_c * 1.8f) + 32.0f;
    }

    return temperature_c;
}

float Vario_Settings_GetQnhDisplayFloat(void)
{
    return Vario_Settings_PressureHpaToDisplayFloat(((float)Vario_Settings_GetManualQnhHpaX100()) * 0.01f);
}

void Vario_Settings_FormatQnhText(char *buf, size_t buf_len)
{
    if ((buf == NULL) || (buf_len == 0u))
    {
        return;
    }

    snprintf(buf,
             buf_len,
             "%.2f %s",
             (double)Vario_Settings_GetQnhDisplayFloat(),
             Vario_Settings_GetPressureUnitText());
}

int32_t Vario_Settings_GetQnhDisplayWhole(void)
{
    float display;

    display = Vario_Settings_GetQnhDisplayFloat();
    return (int32_t)display;
}

int32_t Vario_Settings_GetQnhDisplayFrac2(void)
{
    float   display;
    int32_t whole;
    int32_t frac;

    display = Vario_Settings_GetQnhDisplayFloat();
    whole = (int32_t)display;
    frac = (int32_t)lroundf((display - (float)whole) * 100.0f);

    if (frac < 0)
    {
        frac = -frac;
    }

    if (frac >= 100)
    {
        frac = 99;
    }

    return frac;
}

int32_t Vario_Settings_GetQnhDisplayFrac1(void)
{
    return Vario_Settings_GetQnhDisplayFrac2() / 10;
}

int32_t Vario_Settings_AltitudeMetersToDisplayRounded(float altitude_m)
{
    return Vario_Settings_AltitudeMetersToDisplayRoundedWithUnit(altitude_m,
                                                                 s_vario_settings.altitude_unit);
}

int32_t Vario_Settings_AltitudeMetersToDisplayRoundedWithUnit(float altitude_m, vario_alt_unit_t unit)
{
    float converted;

    if (unit == VARIO_ALT_UNIT_FEET)
    {
        converted = altitude_m * 3.2808399f;
    }
    else
    {
        converted = altitude_m;
    }

    return (int32_t)lroundf(converted);
}

int32_t Vario_Settings_AltitudeCentimetersToDisplayRounded(int32_t altitude_cm)
{
    return Vario_Settings_AltitudeMetersToDisplayRounded(((float)altitude_cm) * 0.01f);
}

int32_t Vario_Settings_VSpeedToDisplayRounded(float vspd_mps)
{
    float converted;

    if (s_vario_settings.vspeed_unit == VARIO_VSPEED_UNIT_FPM)
    {
        converted = vspd_mps * 196.8504f;
    }
    else
    {
        converted = vspd_mps * 10.0f;
    }

    return (int32_t)lroundf(converted);
}

int32_t Vario_Settings_SpeedToDisplayRounded(float speed_kmh)
{
    return (int32_t)lroundf(Vario_Settings_SpeedKmhToDisplayFloat(speed_kmh));
}

float Vario_Settings_SpeedKmhToDisplayFloat(float speed_kmh)
{
    switch (s_vario_settings.speed_unit)
    {
        case VARIO_SPEED_UNIT_MPH:
            return speed_kmh * 0.6213712f;

        case VARIO_SPEED_UNIT_KNOT:
            return speed_kmh * 0.5399568f;

        case VARIO_SPEED_UNIT_KMH:
        default:
            return speed_kmh;
    }
}

float Vario_Settings_VSpeedMpsToDisplayFloat(float vspd_mps)
{
    if (s_vario_settings.vspeed_unit == VARIO_VSPEED_UNIT_FPM)
    {
        return vspd_mps * 196.8504f;
    }

    return vspd_mps;
}

float Vario_Settings_NavDistanceMetersToDisplayFloat(float distance_m)
{
    if (s_vario_settings.speed_unit == VARIO_SPEED_UNIT_MPH)
    {
        return distance_m * 0.0006213712f;
    }

    return distance_m * 0.001f;
}

const char *Vario_Settings_GetAltitudeUnitText(void)
{
    return Vario_Settings_GetAltitudeUnitTextForUnit(s_vario_settings.altitude_unit);
}

const char *Vario_Settings_GetAltitudeUnitTextForUnit(vario_alt_unit_t unit)
{
    if (unit == VARIO_ALT_UNIT_FEET)
    {
        return "ft";
    }

    return "m";
}

const char *Vario_Settings_GetVSpeedUnitText(void)
{
    if (s_vario_settings.vspeed_unit == VARIO_VSPEED_UNIT_FPM)
    {
        return "fpm";
    }

    return "m/s";
}

const char *Vario_Settings_GetSpeedUnitText(void)
{
    switch (s_vario_settings.speed_unit)
    {
        case VARIO_SPEED_UNIT_MPH:
            return "mph";

        case VARIO_SPEED_UNIT_KNOT:
            return "kt";

        case VARIO_SPEED_UNIT_KMH:
        default:
            return "km/h";
    }
}

const char *Vario_Settings_GetNavDistanceUnitText(void)
{
    if (s_vario_settings.speed_unit == VARIO_SPEED_UNIT_MPH)
    {
        return "mi";
    }

    return "km";
}

const char *Vario_Settings_GetAudioOnOffText(void)
{
    return (s_vario_settings.audio_enabled != 0u) ? "ON" : "OFF";
}

const char *Vario_Settings_GetAudioProfileText(void)
{
    return Vario_Settings_GetAudioProfileTextForProfile(s_vario_settings.audio_profile);
}

const char *Vario_Settings_GetAudioProfileTextForProfile(vario_audio_profile_t profile)
{
    switch (profile)
    {
        case VARIO_AUDIO_PROFILE_FLYTEC_CLASSIC:
            return "FLYTEC";

        case VARIO_AUDIO_PROFILE_BLUEFLY_SMOOTH:
            return "BLUEFLY";

        case VARIO_AUDIO_PROFILE_DIGIFLY_DG:
            return "DIGIFLY";

        case VARIO_AUDIO_PROFILE_SOFT_SPEAKER:
        case VARIO_AUDIO_PROFILE_COUNT:
        default:
            return "SOFT";
    }
}

const char *Vario_Settings_GetPrethermalModeText(void)
{
    return Vario_Settings_GetPrethermalModeTextForMode(s_vario_settings.prethermal_mode);
}

const char *Vario_Settings_GetPrethermalModeTextForMode(vario_prethermal_mode_t mode)
{
    switch (mode)
    {
        case VARIO_PRETHERMAL_MODE_BUZZER:
            return "BUZZER";

        case VARIO_PRETHERMAL_MODE_SOFT_PULSE:
            return "PULSE";

        case VARIO_PRETHERMAL_MODE_OFF:
        case VARIO_PRETHERMAL_MODE_COUNT:
        default:
            return "OFF";
    }
}

const char *Vario_Settings_GetAltitudeSourceText(void)
{
    switch (s_vario_settings.altitude_source)
    {
        case VARIO_ALT_SOURCE_DISPLAY:
            return "DISPLAY";

        case VARIO_ALT_SOURCE_QNH_MANUAL:
            return "QNH";

        case VARIO_ALT_SOURCE_FUSED_NOIMU:
            return "NO-IMU";

        case VARIO_ALT_SOURCE_FUSED_IMU:
            return "IMU";

        case VARIO_ALT_SOURCE_GPS_HMSL:
            return "GPS";

        case VARIO_ALT_SOURCE_COUNT:
        default:
            return "DISPLAY";
    }
}

const char *Vario_Settings_GetHeadingSourceText(void)
{
    switch (s_vario_settings.heading_source)
    {
        case VARIO_HEADING_SOURCE_BIKE:
            return "BIKE";

        case VARIO_HEADING_SOURCE_GPS:
            return "GPS";

        case VARIO_HEADING_SOURCE_AUTO:
        case VARIO_HEADING_SOURCE_COUNT:
        default:
            return "AUTO";
    }
}

const char *Vario_Settings_GetAlt2ModeText(void)
{
    return Vario_Settings_GetAlt2ModeTextForMode(s_vario_settings.alt2_mode);
}

const char *Vario_Settings_GetAlt2ModeTextForMode(vario_alt2_mode_t mode)
{
    switch (mode)
    {
        case VARIO_ALT2_MODE_ABSOLUTE:
            return "ABS";

        case VARIO_ALT2_MODE_SMART_FUSE:
            return "SMART FUSE";

        case VARIO_ALT2_MODE_GPS:
            return "GPS";

        case VARIO_ALT2_MODE_FLIGHT_LEVEL:
            return "FL";

        case VARIO_ALT2_MODE_RELATIVE:
        case VARIO_ALT2_MODE_COUNT:
        default:
            return "REL";
    }
}

const char *Vario_Settings_GetPressureUnitText(void)
{
    if (s_vario_settings.pressure_unit == VARIO_PRESSURE_UNIT_INHG)
    {
        return "inHg";
    }

    return "hPa";
}

const char *Vario_Settings_GetTemperatureUnitText(void)
{
    if (s_vario_settings.temperature_unit == VARIO_TEMPERATURE_UNIT_F)
    {
        return "F";
    }

    return "C";
}

const char *Vario_Settings_GetTimeFormatText(void)
{
    if (s_vario_settings.time_format == VARIO_TIME_FORMAT_12H)
    {
        return "12H";
    }

    return "24H";
}

const char *Vario_Settings_GetCoordFormatText(void)
{
    switch (s_vario_settings.coord_format)
    {
        case VARIO_COORD_FORMAT_DECIMAL:
            return "DEC";

        case VARIO_COORD_FORMAT_DMS:
            return "DMS";

        case VARIO_COORD_FORMAT_DDMM_MMM:
        case VARIO_COORD_FORMAT_COUNT:
        default:
            return "DDMM";
    }
}

const char *Vario_Settings_GetBeepModeText(void)
{
    return (s_vario_settings.beep_only_when_flying != 0u) ? "FLY" : "ALWAYS";
}

const char *Vario_Settings_GetBacklightModeText(void)
{
    return Vario_Settings_GetBacklightModeTextForMode(s_vario_settings.display_backlight_mode);
}

const char *Vario_Settings_GetBacklightModeTextForMode(vario_backlight_mode_t mode)
{
    switch (mode)
    {
        case VARIO_BACKLIGHT_MODE_AUTO_DAY_NIGHT:
            return "AUTO D/N";

        case VARIO_BACKLIGHT_MODE_MANUAL:
            return "MANUAL";

        case VARIO_BACKLIGHT_MODE_AUTO_CONTINUOUS:
        case VARIO_BACKLIGHT_MODE_COUNT:
        default:
            return "AUTO CONT";
    }
}

const char *Vario_Settings_GetCategoryText(vario_settings_category_t category)
{
    switch (category)
    {
        case VARIO_SETTINGS_CATEGORY_SYSTEM:
            return "System";

        case VARIO_SETTINGS_CATEGORY_DISPLAY:
            return "Display";

        case VARIO_SETTINGS_CATEGORY_AUDIO:
            return "Audio";

        case VARIO_SETTINGS_CATEGORY_LOG:
            return "Log";

        case VARIO_SETTINGS_CATEGORY_FLIGHT:
            return "Flight";

        case VARIO_SETTINGS_CATEGORY_BLUETOOTH:
            return "Bluetooth";

        case VARIO_SETTINGS_CATEGORY_COUNT:
        default:
            return "System";
    }
}

uint8_t Vario_Settings_GetCategoryItemCount(vario_settings_category_t category)
{
    uint8_t count;

    (void)vario_settings_get_category_items(category, &count);
    return count;
}

void Vario_Settings_GetCategoryItemText(vario_settings_category_t category,
                                        uint8_t index,
                                        char *out_label,
                                        size_t label_len,
                                        char *out_value,
                                        size_t value_len)
{
    vario_menu_item_t          item;
    const vario_settings_t    *settings;

    settings = &s_vario_settings;

    if ((out_label == NULL) || (label_len == 0u) || (out_value == NULL) || (value_len == 0u))
    {
        return;
    }

    if (vario_settings_get_category_item_by_index(category, index, &item) == false)
    {
        snprintf(out_label, label_len, "-");
        snprintf(out_value, value_len, "-");
        return;
    }

    switch (item)
    {
        case VARIO_MENU_ITEM_QNH:
            snprintf(out_label, label_len, "QNH");
            Vario_Settings_FormatQnhText(out_value, value_len);
            break;

        case VARIO_MENU_ITEM_PRESSURE_CORRECTION:
            snprintf(out_label, label_len, "Correction");
            vario_settings_format_pressure_correction_text(out_value, value_len);
            break;

        case VARIO_MENU_ITEM_ALT_UNIT:
            snprintf(out_label, label_len, "Alt Unit");
            snprintf(out_value, value_len, "%s", Vario_Settings_GetAltitudeUnitText());
            break;

        case VARIO_MENU_ITEM_ALT2_MODE:
            snprintf(out_label, label_len, "ALT2 Mode");
            snprintf(out_value, value_len, "%s", Vario_Settings_GetAlt2ModeText());
            break;

        case VARIO_MENU_ITEM_ALT2_UNIT:
            snprintf(out_label, label_len, "ALT2 Unit");
            snprintf(out_value, value_len, "%s", Vario_Settings_GetAltitudeUnitTextForUnit(settings->alt2_unit));
            break;

        case VARIO_MENU_ITEM_VSPEED_UNIT:
            snprintf(out_label, label_len, "Vario Unit");
            snprintf(out_value, value_len, "%s", Vario_Settings_GetVSpeedUnitText());
            break;

        case VARIO_MENU_ITEM_SPEED_UNIT:
            snprintf(out_label, label_len, "Speed Unit");
            snprintf(out_value, value_len, "%s", Vario_Settings_GetSpeedUnitText());
            break;

        case VARIO_MENU_ITEM_PRESSURE_UNIT:
            snprintf(out_label, label_len, "Pressure");
            snprintf(out_value, value_len, "%s", Vario_Settings_GetPressureUnitText());
            break;

        case VARIO_MENU_ITEM_TEMPERATURE_UNIT:
            snprintf(out_label, label_len, "Temp Unit");
            snprintf(out_value, value_len, "%s", Vario_Settings_GetTemperatureUnitText());
            break;

        case VARIO_MENU_ITEM_TIME_FORMAT:
            snprintf(out_label, label_len, "Time Format");
            snprintf(out_value, value_len, "%s", Vario_Settings_GetTimeFormatText());
            break;

        case VARIO_MENU_ITEM_COORD_FORMAT:
            snprintf(out_label, label_len, "Coord Format");
            snprintf(out_value, value_len, "%s", Vario_Settings_GetCoordFormatText());
            break;

        case VARIO_MENU_ITEM_ALT_SOURCE:
            snprintf(out_label, label_len, "Alt Source");
            snprintf(out_value, value_len, "%s", Vario_Settings_GetAltitudeSourceText());
            break;

        case VARIO_MENU_ITEM_IMU_ASSIST:
            snprintf(out_label, label_len, "IMU Assist");
            snprintf(out_value, value_len, "%s", vario_settings_get_imu_assist_text());
            break;

        case VARIO_MENU_ITEM_HEADING_SOURCE:
            snprintf(out_label, label_len, "Heading Src");
            snprintf(out_value, value_len, "%s", Vario_Settings_GetHeadingSourceText());
            break;

        case VARIO_MENU_ITEM_BACKLIGHT_MODE:
            snprintf(out_label, label_len, "BL Mode");
            snprintf(out_value, value_len, "%s", Vario_Settings_GetBacklightModeText());
            break;

        case VARIO_MENU_ITEM_BRIGHTNESS:
            snprintf(out_label, label_len, "Brightness");
            snprintf(out_value, value_len, "%u%%", (unsigned)settings->display_brightness_percent);
            break;

        case VARIO_MENU_ITEM_CONTRAST:
            snprintf(out_label, label_len, "Contrast");
            snprintf(out_value, value_len, "%u", (unsigned)settings->display_contrast_raw);
            break;

        case VARIO_MENU_ITEM_TEMP_COMP:
            snprintf(out_label, label_len, "Temp Comp");
            snprintf(out_value, value_len, "%u", (unsigned)settings->display_temp_compensation);
            break;

        case VARIO_MENU_ITEM_GS_TOP:
            snprintf(out_label, label_len, "GS Top");
            snprintf(out_value,
                     value_len,
                     "%ld %s",
                     (long)Vario_Settings_SpeedToDisplayRounded((float)settings->gs_range_kmh),
                     Vario_Settings_GetSpeedUnitText());
            break;

        case VARIO_MENU_ITEM_PLOT_RANGE:
            snprintf(out_label, label_len, "Plot Range");
            snprintf(out_value, value_len, "%u m", (unsigned)settings->trail_range_m);
            break;

        case VARIO_MENU_ITEM_PLOT_STEP:
            snprintf(out_label, label_len, "Plot Step");
            snprintf(out_value, value_len, "%u m", (unsigned)settings->trail_spacing_m);
            break;

        case VARIO_MENU_ITEM_PLOT_DOT:
            snprintf(out_label, label_len, "Plot Dot");
            snprintf(out_value, value_len, "%u px", (unsigned)settings->trail_dot_size_px);
            break;

        case VARIO_MENU_ITEM_PLOT_ARROW:
            snprintf(out_label, label_len, "Arrow Size");
            snprintf(out_value, value_len, "%u px", (unsigned)settings->arrow_size_px);
            break;

        case VARIO_MENU_ITEM_AUDIO_ENABLE:
            snprintf(out_label, label_len, "Audio");
            snprintf(out_value, value_len, "%s", Vario_Settings_GetAudioOnOffText());
            break;

        case VARIO_MENU_ITEM_AUDIO_VOLUME:
            snprintf(out_label, label_len, "Volume");
            snprintf(out_value, value_len, "%u%%", (unsigned)settings->audio_volume_percent);
            break;

        case VARIO_MENU_ITEM_BEEP_GATE:
            snprintf(out_label, label_len, "Beep Mode");
            snprintf(out_value, value_len, "%s", Vario_Settings_GetBeepModeText());
            break;

        case VARIO_MENU_ITEM_AUDIO_PROFILE:
            snprintf(out_label, label_len, "Sound Prof");
            snprintf(out_value, value_len, "%s", Vario_Settings_GetAudioProfileText());
            break;

        case VARIO_MENU_ITEM_AUDIO_RESPONSE:
            snprintf(out_label, label_len, "Audio Resp");
            snprintf(out_value, value_len, "%u/10", (unsigned)settings->audio_response_level);
            break;

        case VARIO_MENU_ITEM_AUDIO_UP_BASE:
            snprintf(out_label, label_len, "UPHZ");
            snprintf(out_value, value_len, "%u Hz", (unsigned)settings->audio_up_base_hz);
            break;

        case VARIO_MENU_ITEM_AUDIO_MOD_DEPTH:
            snprintf(out_label, label_len, "MODH");
            snprintf(out_value, value_len, "%u%%", (unsigned)settings->audio_modulation_depth_percent);
            break;

        case VARIO_MENU_ITEM_AUDIO_PITCH_CURVE:
            snprintf(out_label, label_len, "PITC");
            snprintf(out_value, value_len, "%u%%", (unsigned)settings->audio_pitch_curve_percent);
            break;

        case VARIO_MENU_ITEM_CLIMB_TONE:
            snprintf(out_label, label_len, "Climb On");
            vario_settings_format_vspeed_threshold(out_value, value_len, settings->climb_tone_threshold_cms);
            break;

        case VARIO_MENU_ITEM_CLIMB_TONE_OFF:
            snprintf(out_label, label_len, "Climb Off");
            vario_settings_format_vspeed_threshold(out_value, value_len, settings->climb_tone_off_threshold_cms);
            break;

        case VARIO_MENU_ITEM_PRETHERMAL_MODE:
            snprintf(out_label, label_len, "PreTherm");
            snprintf(out_value, value_len, "%s", Vario_Settings_GetPrethermalModeText());
            break;

        case VARIO_MENU_ITEM_PRETHERMAL:
            snprintf(out_label, label_len, "PT On");
            vario_settings_format_vspeed_threshold(out_value, value_len, settings->prethermal_threshold_cms);
            break;

        case VARIO_MENU_ITEM_PRETHERMAL_OFF:
            snprintf(out_label, label_len, "PT Off");
            vario_settings_format_vspeed_threshold(out_value, value_len, settings->prethermal_off_threshold_cms);
            break;

        case VARIO_MENU_ITEM_SINK_TONE:
            snprintf(out_label, label_len, "Sink On");
            vario_settings_format_vspeed_threshold(out_value, value_len, settings->sink_tone_threshold_cms);
            break;

        case VARIO_MENU_ITEM_SINK_TONE_OFF:
            snprintf(out_label, label_len, "Sink Off");
            vario_settings_format_vspeed_threshold(out_value, value_len, settings->sink_tone_off_threshold_cms);
            break;

        case VARIO_MENU_ITEM_SINK_CONT:
            snprintf(out_label, label_len, "Sink Cont");
            vario_settings_format_vspeed_threshold(out_value,
                                                   value_len,
                                                   settings->sink_continuous_threshold_cms);
            break;

        case VARIO_MENU_ITEM_SINK_CONT_OFF:
            snprintf(out_label, label_len, "Cont Off");
            vario_settings_format_vspeed_threshold(out_value,
                                                   value_len,
                                                   settings->sink_continuous_off_threshold_cms);
            break;

        case VARIO_MENU_ITEM_LOG_ENABLE:
            snprintf(out_label, label_len, "Track Log");
            snprintf(out_value, value_len, "%s", vario_settings_get_on_off_text(settings->log_enabled));
            break;

        case VARIO_MENU_ITEM_LOG_INTERVAL:
            snprintf(out_label, label_len, "Log Intv");
            snprintf(out_value, value_len, "%u s", (unsigned)settings->log_interval_seconds);
            break;

        case VARIO_MENU_ITEM_DAMPING:
            snprintf(out_label, label_len, "Disp Resp");
            snprintf(out_value, value_len, "%u/10", (unsigned)settings->vario_damping_level);
            break;

        case VARIO_MENU_ITEM_INT_AVG:
            snprintf(out_label, label_len, "Int Avg");
            snprintf(out_value, value_len, "%u s", (unsigned)settings->digital_vario_average_seconds);
            break;

        case VARIO_MENU_ITEM_FLIGHT_START:
            snprintf(out_label, label_len, "Flight Start");
            vario_settings_format_speed_threshold(out_value, value_len, settings->flight_start_speed_kmh_x10);
            break;

        case VARIO_MENU_ITEM_TRAINER:
            snprintf(out_label, label_len, "Trainer");
            snprintf(out_value, value_len, "%s", vario_settings_get_on_off_text(settings->trainer_enabled));
            break;

        case VARIO_MENU_ITEM_MANUAL_MC:
            snprintf(out_label, label_len, "Manual MC");
            vario_settings_format_positive_sink(out_value, value_len, settings->manual_mccready_cms);
            break;

        case VARIO_MENU_ITEM_FINAL_GLIDE_MARGIN:
            snprintf(out_label, label_len, "FG Margin");
            snprintf(out_value,
                     value_len,
                     "%ld %s",
                     (long)Vario_Settings_AltitudeMetersToDisplayRounded((float)settings->final_glide_safety_margin_m),
                     Vario_Settings_GetAltitudeUnitText());
            break;

        case VARIO_MENU_ITEM_POLAR_V1:
            snprintf(out_label, label_len, "Polar V1");
            vario_settings_format_speed_threshold(out_value, value_len, settings->polar_speed1_kmh_x10);
            break;

        case VARIO_MENU_ITEM_POLAR_S1:
            snprintf(out_label, label_len, "Polar S1");
            vario_settings_format_positive_sink(out_value, value_len, settings->polar_sink1_cms);
            break;

        case VARIO_MENU_ITEM_POLAR_V2:
            snprintf(out_label, label_len, "Polar V2");
            vario_settings_format_speed_threshold(out_value, value_len, settings->polar_speed2_kmh_x10);
            break;

        case VARIO_MENU_ITEM_POLAR_S2:
            snprintf(out_label, label_len, "Polar S2");
            vario_settings_format_positive_sink(out_value, value_len, settings->polar_sink2_cms);
            break;

        case VARIO_MENU_ITEM_POLAR_V3:
            snprintf(out_label, label_len, "Polar V3");
            vario_settings_format_speed_threshold(out_value, value_len, settings->polar_speed3_kmh_x10);
            break;

        case VARIO_MENU_ITEM_POLAR_S3:
            snprintf(out_label, label_len, "Polar S3");
            vario_settings_format_positive_sink(out_value, value_len, settings->polar_sink3_cms);
            break;

        case VARIO_MENU_ITEM_VARIO_SCALE:
        {
            uint8_t vario_scale_x10;

            /* -------------------------------------------------------------- */
            /*  표시 문자열도 draw layer와 같은 4.0 / 5.0 계약을 사용한다.    */
            /*  즉, 내부 필드가 옛 값(예: 80)으로 남아 있어도                 */
            /*  메뉴에는 현재 지원 범위만 명확히 보이게 한다.                */
            /* -------------------------------------------------------------- */
            vario_scale_x10 = vario_settings_snap_vario_range_x10(settings->vario_range_mps_x10);
            snprintf(out_label, label_len, "Vario Top");
            snprintf(out_value,
                     value_len,
                     "%u.%u m/s",
                     (unsigned)(vario_scale_x10 / 10u),
                     (unsigned)(vario_scale_x10 % 10u));
            break;
        }

        case VARIO_MENU_ITEM_ALT2_CAPTURE:
            snprintf(out_label, label_len, "ALT2 Capture");
            vario_settings_format_alt2_ref(out_value, value_len, settings->alt2_reference_cm);
            break;

        case VARIO_MENU_ITEM_ALT3_RESET:
            snprintf(out_label, label_len, "ALT3 Reset");
            snprintf(out_value, value_len, "press +/-");
            break;

        case VARIO_MENU_ITEM_ATTITUDE_RESET:
            snprintf(out_label, label_len, "Att Reset");
            snprintf(out_value, value_len, "press +/-");
            break;

        case VARIO_MENU_ITEM_FLIGHT_RESET:
            snprintf(out_label, label_len, "Flight Reset");
            snprintf(out_value, value_len, "press +/-");
            break;

        case VARIO_MENU_ITEM_BT_ECHO:
            snprintf(out_label, label_len, "Echo");
            snprintf(out_value, value_len, "%s", vario_settings_get_on_off_text(settings->bluetooth_echo_enabled));
            break;

        case VARIO_MENU_ITEM_BT_AUTOPING:
            snprintf(out_label, label_len, "Auto Ping");
            snprintf(out_value, value_len, "%s", vario_settings_get_on_off_text(settings->bluetooth_auto_ping_enabled));
            break;

        case VARIO_MENU_ITEM_COUNT:
        default:
            snprintf(out_label, label_len, "-");
            snprintf(out_value, value_len, "-");
            break;
    }
}

void Vario_Settings_AdjustCategoryItem(vario_settings_category_t category,
                                       uint8_t index,
                                       int8_t direction)
{
    vario_menu_item_t item;

    if (vario_settings_get_category_item_by_index(category, index, &item) == false)
    {
        return;
    }

    switch (item)
    {
        case VARIO_MENU_ITEM_QNH:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_QNH, direction);
            break;

        case VARIO_MENU_ITEM_PRESSURE_CORRECTION:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_PRESSURE_CORRECTION, direction);
            break;

        case VARIO_MENU_ITEM_ALT_UNIT:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_ALT_UNIT, direction);
            break;

        case VARIO_MENU_ITEM_ALT2_MODE:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_ALT2_MODE, direction);
            break;

        case VARIO_MENU_ITEM_ALT2_UNIT:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_ALT2_UNIT, direction);
            break;

        case VARIO_MENU_ITEM_VSPEED_UNIT:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_VSPEED_UNIT, direction);
            break;

        case VARIO_MENU_ITEM_SPEED_UNIT:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_SPEED_UNIT, direction);
            break;

        case VARIO_MENU_ITEM_PRESSURE_UNIT:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_PRESSURE_UNIT, direction);
            break;

        case VARIO_MENU_ITEM_TEMPERATURE_UNIT:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_TEMPERATURE_UNIT, direction);
            break;

        case VARIO_MENU_ITEM_TIME_FORMAT:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_TIME_FORMAT, direction);
            break;

        case VARIO_MENU_ITEM_COORD_FORMAT:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_COORD_FORMAT, direction);
            break;

        case VARIO_MENU_ITEM_ALT_SOURCE:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_ALT_SOURCE, direction);
            break;

        case VARIO_MENU_ITEM_IMU_ASSIST:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_IMU_ASSIST, direction);
            break;

        case VARIO_MENU_ITEM_HEADING_SOURCE:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_HEADING_SOURCE, direction);
            break;

        case VARIO_MENU_ITEM_BACKLIGHT_MODE:
            vario_settings_cycle_backlight_mode(direction);
            break;

        case VARIO_MENU_ITEM_BRIGHTNESS:
            Vario_Settings_AdjustValue(VARIO_VALUE_ITEM_BRIGHTNESS, direction);
            break;

        case VARIO_MENU_ITEM_CONTRAST:
            s_vario_settings.display_contrast_raw =
                vario_settings_clamp_u8((uint8_t)((int32_t)s_vario_settings.display_contrast_raw +
                                                  ((int32_t)direction * 8)),
                                        0u,
                                        255u);
            break;

        case VARIO_MENU_ITEM_TEMP_COMP:
            s_vario_settings.display_temp_compensation =
                vario_settings_clamp_u8((uint8_t)((int32_t)s_vario_settings.display_temp_compensation +
                                                  (int32_t)direction),
                                        0u,
                                        3u);
            break;

        case VARIO_MENU_ITEM_GS_TOP:
            Vario_Settings_AdjustValue(VARIO_VALUE_ITEM_GS_RANGE, direction);
            break;

        case VARIO_MENU_ITEM_PLOT_RANGE:
            Vario_Settings_AdjustValue(VARIO_VALUE_ITEM_TRAIL_RANGE, direction);
            break;

        case VARIO_MENU_ITEM_PLOT_STEP:
            Vario_Settings_AdjustValue(VARIO_VALUE_ITEM_TRAIL_SPACING, direction);
            break;

        case VARIO_MENU_ITEM_PLOT_DOT:
            Vario_Settings_AdjustValue(VARIO_VALUE_ITEM_TRAIL_DOT_SIZE, direction);
            break;

        case VARIO_MENU_ITEM_PLOT_ARROW:
            Vario_Settings_AdjustValue(VARIO_VALUE_ITEM_ARROW_SIZE, direction);
            break;

        case VARIO_MENU_ITEM_AUDIO_ENABLE:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_AUDIO_ENABLE, direction);
            break;

        case VARIO_MENU_ITEM_AUDIO_VOLUME:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_AUDIO_VOLUME, direction);
            break;

        case VARIO_MENU_ITEM_BEEP_GATE:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_BEEP_ONLY_WHEN_FLYING, direction);
            break;

        case VARIO_MENU_ITEM_AUDIO_PROFILE:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_AUDIO_PROFILE, direction);
            break;

        case VARIO_MENU_ITEM_AUDIO_RESPONSE:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_AUDIO_RESPONSE, direction);
            break;

        case VARIO_MENU_ITEM_AUDIO_UP_BASE:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_AUDIO_UP_BASE_HZ, direction);
            break;

        case VARIO_MENU_ITEM_AUDIO_MOD_DEPTH:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_AUDIO_MOD_DEPTH, direction);
            break;

        case VARIO_MENU_ITEM_AUDIO_PITCH_CURVE:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_AUDIO_PITCH_CURVE, direction);
            break;

        case VARIO_MENU_ITEM_CLIMB_TONE:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_CLIMB_TONE_THRESHOLD, direction);
            break;

        case VARIO_MENU_ITEM_CLIMB_TONE_OFF:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_CLIMB_TONE_OFF_THRESHOLD, direction);
            break;

        case VARIO_MENU_ITEM_PRETHERMAL_MODE:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_PRETHERMAL_MODE, direction);
            break;

        case VARIO_MENU_ITEM_PRETHERMAL:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_PRETHERMAL_THRESHOLD, direction);
            break;

        case VARIO_MENU_ITEM_PRETHERMAL_OFF:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_PRETHERMAL_OFF_THRESHOLD, direction);
            break;

        case VARIO_MENU_ITEM_SINK_TONE:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_SINK_TONE_THRESHOLD, direction);
            break;

        case VARIO_MENU_ITEM_SINK_TONE_OFF:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_SINK_TONE_OFF_THRESHOLD, direction);
            break;

        case VARIO_MENU_ITEM_SINK_CONT:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_SINK_CONT_THRESHOLD, direction);
            break;

        case VARIO_MENU_ITEM_SINK_CONT_OFF:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_SINK_CONT_OFF_THRESHOLD, direction);
            break;

        case VARIO_MENU_ITEM_LOG_ENABLE:
            if (direction != 0)
            {
                vario_settings_toggle_u8(&s_vario_settings.log_enabled);
            }
            break;

        case VARIO_MENU_ITEM_LOG_INTERVAL:
            s_vario_settings.log_interval_seconds =
                vario_settings_clamp_u8((uint8_t)((int32_t)s_vario_settings.log_interval_seconds +
                                                  (int32_t)direction),
                                        1u,
                                        10u);
            break;

        case VARIO_MENU_ITEM_DAMPING:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_VARIO_DAMPING, direction);
            break;

        case VARIO_MENU_ITEM_INT_AVG:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_VARIO_AVG_SECONDS, direction);
            break;

        case VARIO_MENU_ITEM_FLIGHT_START:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_FLIGHT_START_SPEED, direction);
            break;

        case VARIO_MENU_ITEM_TRAINER:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_TRAINER, direction);
            break;

        case VARIO_MENU_ITEM_MANUAL_MC:
            s_vario_settings.manual_mccready_cms =
                (int16_t)(s_vario_settings.manual_mccready_cms + ((int16_t)direction * 10));
            vario_settings_sanitize_glide_computer();
            break;

        case VARIO_MENU_ITEM_FINAL_GLIDE_MARGIN:
        {
            int32_t next_margin_m;

            next_margin_m = (int32_t)s_vario_settings.final_glide_safety_margin_m + ((int32_t)direction * 10);
            if (next_margin_m < 0)
            {
                next_margin_m = 0;
            }
            s_vario_settings.final_glide_safety_margin_m = (uint16_t)next_margin_m;
            vario_settings_sanitize_glide_computer();
            break;
        }

        case VARIO_MENU_ITEM_POLAR_V1:
        {
            int32_t next_speed_x10;

            next_speed_x10 = (int32_t)s_vario_settings.polar_speed1_kmh_x10 + ((int32_t)direction * 20);
            if (next_speed_x10 < 0)
            {
                next_speed_x10 = 0;
            }
            s_vario_settings.polar_speed1_kmh_x10 = (uint16_t)next_speed_x10;
            vario_settings_sanitize_glide_computer();
            break;
        }

        case VARIO_MENU_ITEM_POLAR_S1:
            s_vario_settings.polar_sink1_cms =
                (int16_t)(s_vario_settings.polar_sink1_cms + ((int16_t)direction * 5));
            vario_settings_sanitize_glide_computer();
            break;

        case VARIO_MENU_ITEM_POLAR_V2:
        {
            int32_t next_speed_x10;

            next_speed_x10 = (int32_t)s_vario_settings.polar_speed2_kmh_x10 + ((int32_t)direction * 20);
            if (next_speed_x10 < 0)
            {
                next_speed_x10 = 0;
            }
            s_vario_settings.polar_speed2_kmh_x10 = (uint16_t)next_speed_x10;
            vario_settings_sanitize_glide_computer();
            break;
        }

        case VARIO_MENU_ITEM_POLAR_S2:
            s_vario_settings.polar_sink2_cms =
                (int16_t)(s_vario_settings.polar_sink2_cms + ((int16_t)direction * 5));
            vario_settings_sanitize_glide_computer();
            break;

        case VARIO_MENU_ITEM_POLAR_V3:
        {
            int32_t next_speed_x10;

            next_speed_x10 = (int32_t)s_vario_settings.polar_speed3_kmh_x10 + ((int32_t)direction * 20);
            if (next_speed_x10 < 0)
            {
                next_speed_x10 = 0;
            }
            s_vario_settings.polar_speed3_kmh_x10 = (uint16_t)next_speed_x10;
            vario_settings_sanitize_glide_computer();
            break;
        }

        case VARIO_MENU_ITEM_POLAR_S3:
            s_vario_settings.polar_sink3_cms =
                (int16_t)(s_vario_settings.polar_sink3_cms + ((int16_t)direction * 5));
            vario_settings_sanitize_glide_computer();
            break;

        case VARIO_MENU_ITEM_VARIO_SCALE:
            Vario_Settings_AdjustValue(VARIO_VALUE_ITEM_VARIO_RANGE, direction);
            break;

        case VARIO_MENU_ITEM_ALT2_CAPTURE:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_ALT2_CAPTURE, direction);
            break;

        case VARIO_MENU_ITEM_ALT3_RESET:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_ALT3_RESET, direction);
            break;

        case VARIO_MENU_ITEM_ATTITUDE_RESET:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_ATTITUDE_RESET, direction);
            break;

        case VARIO_MENU_ITEM_FLIGHT_RESET:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_FLIGHT_RESET, direction);
            break;

        case VARIO_MENU_ITEM_BT_ECHO:
            if (direction != 0)
            {
                vario_settings_toggle_u8(&s_vario_settings.bluetooth_echo_enabled);
            }
            break;

        case VARIO_MENU_ITEM_BT_AUTOPING:
            if (direction != 0)
            {
                vario_settings_toggle_u8(&s_vario_settings.bluetooth_auto_ping_enabled);
            }
            break;

        case VARIO_MENU_ITEM_COUNT:
        default:
            break;
    }
}
