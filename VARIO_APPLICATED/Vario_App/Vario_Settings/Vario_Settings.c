#include "Vario_Settings.h"

#include <math.h>

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

static void vario_settings_toggle_alt_unit(void)
{
    if (s_vario_settings.altitude_unit == VARIO_ALT_UNIT_METER)
    {
        s_vario_settings.altitude_unit = VARIO_ALT_UNIT_FEET;
    }
    else
    {
        s_vario_settings.altitude_unit = VARIO_ALT_UNIT_METER;
    }
}

static void vario_settings_toggle_vspeed_unit(void)
{
    if (s_vario_settings.vspeed_unit == VARIO_VSPEED_UNIT_MPS)
    {
        s_vario_settings.vspeed_unit = VARIO_VSPEED_UNIT_FPM;
    }
    else
    {
        s_vario_settings.vspeed_unit = VARIO_VSPEED_UNIT_MPS;
    }
}

static void vario_settings_toggle_audio_enable(void)
{
    if (s_vario_settings.audio_enabled == 0u)
    {
        s_vario_settings.audio_enabled = 1u;
    }
    else
    {
        s_vario_settings.audio_enabled = 0u;
    }
}

void Vario_Settings_Init(void)
{
    /* ---------------------------------------------------------------------- */
    /*  기본 설정                                                              */
    /*  - QNH          : 1013.2 hPa                                            */
    /*  - ALT1/2/3     : 0m / 500m / 1000m                                     */
    /*  - Unit         : m / m/s                                               */
    /*  - Audio        : ON / 75%                                              */
    /* ---------------------------------------------------------------------- */
    s_vario_settings.qnh_hpa_x100         = 101320;
    s_vario_settings.alt1_cm              =      0;
    s_vario_settings.alt2_cm              =  50000;
    s_vario_settings.alt3_cm              = 100000;
    s_vario_settings.altitude_unit        = VARIO_ALT_UNIT_METER;
    s_vario_settings.vspeed_unit          = VARIO_VSPEED_UNIT_MPS;
    s_vario_settings.audio_enabled        = 1u;
    s_vario_settings.audio_volume_percent = 75u;
}

const vario_settings_t *Vario_Settings_Get(void)
{
    return &s_vario_settings;
}

void Vario_Settings_AdjustQuickSet(vario_quickset_item_t item, int8_t direction)
{
    switch (item)
    {
        case VARIO_QUICKSET_ITEM_QNH:
            s_vario_settings.qnh_hpa_x100 =
                vario_settings_clamp_s32(s_vario_settings.qnh_hpa_x100 + ((int32_t)direction * 10),
                                         95000,
                                         105000);
            break;

        case VARIO_QUICKSET_ITEM_ALT_UNIT:
            if (direction != 0)
            {
                vario_settings_toggle_alt_unit();
            }
            break;

        case VARIO_QUICKSET_ITEM_VSPEED_UNIT:
            if (direction != 0)
            {
                vario_settings_toggle_vspeed_unit();
            }
            break;

        case VARIO_QUICKSET_ITEM_AUDIO_ENABLE:
            if (direction != 0)
            {
                vario_settings_toggle_audio_enable();
            }
            break;

        case VARIO_QUICKSET_ITEM_AUDIO_VOLUME:
            s_vario_settings.audio_volume_percent =
                (uint8_t)vario_settings_clamp_s32((int32_t)s_vario_settings.audio_volume_percent +
                                                  ((int32_t)direction * 5),
                                                  0,
                                                  100);
            break;

        case VARIO_QUICKSET_ITEM_COUNT:
        default:
            break;
    }
}

void Vario_Settings_AdjustValue(vario_value_item_t item, int8_t direction)
{
    int32_t altitude_step_cm;

    /* ---------------------------------------------------------------------- */
    /*  metric 모드에서는 1m step, feet 모드에서는 대략 10ft step               */
    /* ---------------------------------------------------------------------- */
    altitude_step_cm = (s_vario_settings.altitude_unit == VARIO_ALT_UNIT_METER) ? 100 : 305;

    switch (item)
    {
        case VARIO_VALUE_ITEM_QNH:
            s_vario_settings.qnh_hpa_x100 =
                vario_settings_clamp_s32(s_vario_settings.qnh_hpa_x100 + ((int32_t)direction * 50),
                                         95000,
                                         105000);
            break;

        case VARIO_VALUE_ITEM_ALT1:
            s_vario_settings.alt1_cm =
                vario_settings_clamp_s32(s_vario_settings.alt1_cm + ((int32_t)direction * altitude_step_cm),
                                         -50000,
                                         1200000);
            break;

        case VARIO_VALUE_ITEM_ALT2:
            s_vario_settings.alt2_cm =
                vario_settings_clamp_s32(s_vario_settings.alt2_cm + ((int32_t)direction * altitude_step_cm),
                                         -50000,
                                         1200000);
            break;

        case VARIO_VALUE_ITEM_ALT3:
            s_vario_settings.alt3_cm =
                vario_settings_clamp_s32(s_vario_settings.alt3_cm + ((int32_t)direction * altitude_step_cm),
                                         -50000,
                                         1200000);
            break;

        case VARIO_VALUE_ITEM_ALT_UNIT:
            if (direction != 0)
            {
                vario_settings_toggle_alt_unit();
            }
            break;

        case VARIO_VALUE_ITEM_VSPEED_UNIT:
            if (direction != 0)
            {
                vario_settings_toggle_vspeed_unit();
            }
            break;

        case VARIO_VALUE_ITEM_COUNT:
        default:
            break;
    }
}

int32_t Vario_Settings_GetQnhDisplayWhole(void)
{
    return s_vario_settings.qnh_hpa_x100 / 100;
}

int32_t Vario_Settings_GetQnhDisplayFrac1(void)
{
    int32_t frac;

    frac = s_vario_settings.qnh_hpa_x100 % 100;
    if (frac < 0)
    {
        frac = -frac;
    }

    return frac / 10;
}

int32_t Vario_Settings_AltitudeMetersToDisplayRounded(float altitude_m)
{
    float converted;

    if (s_vario_settings.altitude_unit == VARIO_ALT_UNIT_FEET)
    {
        converted = altitude_m * 3.2808399f;
    }
    else
    {
        converted = altitude_m;
    }

    return (int32_t)lroundf(converted);
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

const char *Vario_Settings_GetAltitudeUnitText(void)
{
    if (s_vario_settings.altitude_unit == VARIO_ALT_UNIT_FEET)
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

const char *Vario_Settings_GetAudioOnOffText(void)
{
    if (s_vario_settings.audio_enabled != 0u)
    {
        return "ON";
    }

    return "OFF";
}
