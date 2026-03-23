#include "Vario_Settings.h"

#include "../Vario_State/Vario_State.h"

#include <math.h>
#include <stddef.h>
#include <stdio.h>

static vario_settings_t s_vario_settings;

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
    s_vario_settings.display_brightness_percent    = 70u;
    s_vario_settings.vario_damping_level           = 5u;
    s_vario_settings.digital_vario_average_seconds = 5u;
    s_vario_settings.climb_tone_threshold_cms      = 20;
    s_vario_settings.sink_tone_threshold_cms       = -150;
    s_vario_settings.flight_start_speed_kmh_x10    = 50u;
    s_vario_settings.compass_span_deg              = 120u;
    s_vario_settings.compass_box_height_px         = 16u;
    s_vario_settings.vario_range_mps_x10           = 100u;
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
                                                  ((int32_t)direction * 5)),
                                        30u,
                                        100u);
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
