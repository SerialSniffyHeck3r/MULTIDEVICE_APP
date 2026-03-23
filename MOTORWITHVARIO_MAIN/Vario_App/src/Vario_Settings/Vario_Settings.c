#include "Vario_Settings.h"

#include "../Vario_State/Vario_State.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>

static vario_settings_t s_vario_settings;

typedef enum
{
    VARIO_MENU_ITEM_QNH = 0u,
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
    VARIO_MENU_ITEM_CLIMB_TONE,
    VARIO_MENU_ITEM_SINK_TONE,
    VARIO_MENU_ITEM_LOG_ENABLE,
    VARIO_MENU_ITEM_LOG_INTERVAL,
    VARIO_MENU_ITEM_DAMPING,
    VARIO_MENU_ITEM_INT_AVG,
    VARIO_MENU_ITEM_FLIGHT_START,
    VARIO_MENU_ITEM_VARIO_SCALE,
    VARIO_MENU_ITEM_ALT2_CAPTURE,
    VARIO_MENU_ITEM_ALT3_RESET,
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
    VARIO_MENU_ITEM_GS_TOP,
};

static const vario_menu_item_t s_vario_category_audio_items[] = {
    VARIO_MENU_ITEM_AUDIO_ENABLE,
    VARIO_MENU_ITEM_AUDIO_VOLUME,
    VARIO_MENU_ITEM_BEEP_GATE,
    VARIO_MENU_ITEM_CLIMB_TONE,
    VARIO_MENU_ITEM_SINK_TONE,
};

static const vario_menu_item_t s_vario_category_log_items[] = {
    VARIO_MENU_ITEM_LOG_ENABLE,
    VARIO_MENU_ITEM_LOG_INTERVAL,
    VARIO_MENU_ITEM_PLOT_RANGE,
    VARIO_MENU_ITEM_PLOT_STEP,
};

static const vario_menu_item_t s_vario_category_flight_items[] = {
    VARIO_MENU_ITEM_QNH,
    VARIO_MENU_ITEM_ALT2_MODE,
    VARIO_MENU_ITEM_ALT2_UNIT,
    VARIO_MENU_ITEM_ALT_SOURCE,
    VARIO_MENU_ITEM_HEADING_SOURCE,
    VARIO_MENU_ITEM_DAMPING,
    VARIO_MENU_ITEM_INT_AVG,
    VARIO_MENU_ITEM_FLIGHT_START,
    VARIO_MENU_ITEM_VARIO_SCALE,
    VARIO_MENU_ITEM_ALT2_CAPTURE,
    VARIO_MENU_ITEM_ALT3_RESET,
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
    s_vario_settings.qnh_hpa_x100                  = 101325;
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
    s_vario_settings.altitude_source               = VARIO_ALT_SOURCE_QNH_MANUAL;
    s_vario_settings.heading_source                = VARIO_HEADING_SOURCE_AUTO;
    s_vario_settings.audio_enabled                 = 1u;
    s_vario_settings.audio_volume_percent          = 75u;
    s_vario_settings.beep_only_when_flying         = 1u;
    s_vario_settings.display_backlight_mode       = VARIO_BACKLIGHT_MODE_AUTO_CONTINUOUS;
    s_vario_settings.display_brightness_percent    = 70u;
    s_vario_settings.display_contrast_raw          = 160u;
    s_vario_settings.display_temp_compensation     = 1u;
    s_vario_settings.vario_damping_level           = 5u;
    s_vario_settings.digital_vario_average_seconds = 5u;
    s_vario_settings.climb_tone_threshold_cms      = 20;
    s_vario_settings.sink_tone_threshold_cms       = -150;
    s_vario_settings.flight_start_speed_kmh_x10    = 50u;
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
}

const vario_settings_t *Vario_Settings_Get(void)
{
    return &s_vario_settings;
}

void Vario_Settings_AdjustQuickSet(vario_quickset_item_t item, int8_t direction)
{
    const vario_runtime_t *rt;
    int32_t                qnh_step_x100;

    rt = Vario_State_GetRuntime();
    qnh_step_x100 = (s_vario_settings.pressure_unit == VARIO_PRESSURE_UNIT_INHG) ? 34 : 10;

    switch (item)
    {
        case VARIO_QUICKSET_ITEM_QNH:
            s_vario_settings.qnh_hpa_x100 =
                vario_settings_clamp_s32(s_vario_settings.qnh_hpa_x100 + ((int32_t)direction * qnh_step_x100),
                                         95000,
                                         105500);
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

        case VARIO_QUICKSET_ITEM_VARIO_AVG_SECONDS:
            s_vario_settings.digital_vario_average_seconds =
                vario_settings_clamp_u8((uint8_t)((int32_t)s_vario_settings.digital_vario_average_seconds +
                                                  (int32_t)direction),
                                        1u,
                                        30u);
            break;

        case VARIO_QUICKSET_ITEM_CLIMB_TONE_THRESHOLD:
            s_vario_settings.climb_tone_threshold_cms =
                (int16_t)vario_settings_clamp_s32((int32_t)s_vario_settings.climb_tone_threshold_cms +
                                                  ((int32_t)direction * 10),
                                                  0,
                                                  300);
            break;

        case VARIO_QUICKSET_ITEM_SINK_TONE_THRESHOLD:
            s_vario_settings.sink_tone_threshold_cms =
                (int16_t)vario_settings_clamp_s32((int32_t)s_vario_settings.sink_tone_threshold_cms +
                                                  ((int32_t)direction * 10),
                                                  -500,
                                                  0);
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

        case VARIO_QUICKSET_ITEM_ALT2_CAPTURE:
            if ((direction != 0) && (rt != NULL) && (rt->derived_valid != false))
            {
                s_vario_settings.alt2_reference_cm = (int32_t)lroundf(rt->alt1_absolute_m * 100.0f);
            }
            break;

        case VARIO_QUICKSET_ITEM_ALT3_RESET:
            if (direction != 0)
            {
                Vario_State_ResetAccumulatedGain();
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
            s_vario_settings.vario_range_mps_x10 =
                vario_settings_clamp_u8((uint8_t)((int32_t)s_vario_settings.vario_range_mps_x10 +
                                                  ((int32_t)direction * 10)),
                                        40u,
                                        80u);
            break;

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
    return Vario_Settings_PressureHpaToDisplayFloat(((float)s_vario_settings.qnh_hpa_x100) * 0.01f);
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

        case VARIO_MENU_ITEM_CLIMB_TONE:
            snprintf(out_label, label_len, "Climb Tone");
            vario_settings_format_vspeed_threshold(out_value, value_len, settings->climb_tone_threshold_cms);
            break;

        case VARIO_MENU_ITEM_SINK_TONE:
            snprintf(out_label, label_len, "Sink Tone");
            vario_settings_format_vspeed_threshold(out_value, value_len, settings->sink_tone_threshold_cms);
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
            snprintf(out_label, label_len, "Damping");
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

        case VARIO_MENU_ITEM_VARIO_SCALE:
            snprintf(out_label, label_len, "Vario Top");
            snprintf(out_value,
                     value_len,
                     "%u m/s",
                     (unsigned)(settings->vario_range_mps_x10 / 10u));
            break;

        case VARIO_MENU_ITEM_ALT2_CAPTURE:
            snprintf(out_label, label_len, "ALT2 Capture");
            vario_settings_format_alt2_ref(out_value, value_len, settings->alt2_reference_cm);
            break;

        case VARIO_MENU_ITEM_ALT3_RESET:
            snprintf(out_label, label_len, "ALT3 Reset");
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

        case VARIO_MENU_ITEM_CLIMB_TONE:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_CLIMB_TONE_THRESHOLD, direction);
            break;

        case VARIO_MENU_ITEM_SINK_TONE:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_SINK_TONE_THRESHOLD, direction);
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

        case VARIO_MENU_ITEM_VARIO_SCALE:
            Vario_Settings_AdjustValue(VARIO_VALUE_ITEM_VARIO_RANGE, direction);
            break;

        case VARIO_MENU_ITEM_ALT2_CAPTURE:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_ALT2_CAPTURE, direction);
            break;

        case VARIO_MENU_ITEM_ALT3_RESET:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_ALT3_RESET, direction);
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
