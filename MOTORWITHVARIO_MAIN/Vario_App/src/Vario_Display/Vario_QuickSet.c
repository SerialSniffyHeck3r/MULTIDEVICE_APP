#include "Vario_QuickSet.h"

#include "Vario_Display_Common.h"
#include "Vario_Settings.h"
#include "Vario_State.h"

#include <math.h>
#include <stdio.h>

#define VARIO_QUICKSET_VISIBLE_ROWS 6
#define VARIO_QUICKSET_ROW_PITCH    15

static void vario_quickset_format_vspeed_threshold(char *buf, size_t buf_len, int16_t threshold_cms)
{
    float value_mps;
    float disp;

    value_mps = ((float)threshold_cms) * 0.01f;
    disp = Vario_Settings_VSpeedMpsToDisplayFloat(value_mps);

    if (Vario_Settings_Get()->vspeed_unit == VARIO_VSPEED_UNIT_FPM)
    {
        snprintf(buf, buf_len, "%+ld %s", (long)lroundf(disp), Vario_Settings_GetVSpeedUnitText());
    }
    else
    {
        snprintf(buf, buf_len, "%+.1f %s", (double)disp, Vario_Settings_GetVSpeedUnitText());
    }
}

static void vario_quickset_format_speed_threshold(char *buf, size_t buf_len, uint16_t speed_kmh_x10)
{
    float speed_disp;

    speed_disp = Vario_Settings_SpeedKmhToDisplayFloat(((float)speed_kmh_x10) * 0.1f);
    snprintf(buf, buf_len, "%.1f %s", (double)speed_disp, Vario_Settings_GetSpeedUnitText());
}

static void vario_quickset_format_alt2_ref(char *buf, size_t buf_len, int32_t altitude_cm)
{
    snprintf(buf,
             buf_len,
             "%ld %s",
             (long)Vario_Settings_AltitudeMetersToDisplayRoundedWithUnit(((float)altitude_cm) * 0.01f,
                                                                         Vario_Settings_Get()->alt2_unit),
             Vario_Settings_GetAltitudeUnitTextForUnit(Vario_Settings_Get()->alt2_unit));
}

static void vario_quickset_get_item_text(vario_quickset_item_t item,
                                         const vario_settings_t *settings,
                                         char *out_label,
                                         size_t label_len,
                                         char *out_value,
                                         size_t value_len)
{
    switch (item)
    {
        case VARIO_QUICKSET_ITEM_QNH:
            snprintf(out_label, label_len, "QNH");
            Vario_Settings_FormatQnhText(out_value, value_len);
            break;

        case VARIO_QUICKSET_ITEM_ALT_UNIT:
            snprintf(out_label, label_len, "Alt Unit");
            snprintf(out_value, value_len, "%s", Vario_Settings_GetAltitudeUnitText());
            break;

        case VARIO_QUICKSET_ITEM_ALT2_MODE:
            snprintf(out_label, label_len, "ALT2 Mode");
            snprintf(out_value, value_len, "%s", Vario_Settings_GetAlt2ModeText());
            break;

        case VARIO_QUICKSET_ITEM_ALT2_UNIT:
            snprintf(out_label, label_len, "ALT2 Unit");
            snprintf(out_value,
                     value_len,
                     "%s",
                     Vario_Settings_GetAltitudeUnitTextForUnit(settings->alt2_unit));
            break;

        case VARIO_QUICKSET_ITEM_VSPEED_UNIT:
            snprintf(out_label, label_len, "Vario Unit");
            snprintf(out_value, value_len, "%s", Vario_Settings_GetVSpeedUnitText());
            break;

        case VARIO_QUICKSET_ITEM_SPEED_UNIT:
            snprintf(out_label, label_len, "Speed Unit");
            snprintf(out_value, value_len, "%s", Vario_Settings_GetSpeedUnitText());
            break;

        case VARIO_QUICKSET_ITEM_PRESSURE_UNIT:
            snprintf(out_label, label_len, "Pressure");
            snprintf(out_value, value_len, "%s", Vario_Settings_GetPressureUnitText());
            break;

        case VARIO_QUICKSET_ITEM_TEMPERATURE_UNIT:
            snprintf(out_label, label_len, "Temp Unit");
            snprintf(out_value, value_len, "%s", Vario_Settings_GetTemperatureUnitText());
            break;

        case VARIO_QUICKSET_ITEM_TIME_FORMAT:
            snprintf(out_label, label_len, "Time Format");
            snprintf(out_value, value_len, "%s", Vario_Settings_GetTimeFormatText());
            break;

        case VARIO_QUICKSET_ITEM_COORD_FORMAT:
            snprintf(out_label, label_len, "Coord Format");
            snprintf(out_value, value_len, "%s", Vario_Settings_GetCoordFormatText());
            break;

        case VARIO_QUICKSET_ITEM_ALT_SOURCE:
            snprintf(out_label, label_len, "Alt Source");
            snprintf(out_value, value_len, "%s", Vario_Settings_GetAltitudeSourceText());
            break;

        case VARIO_QUICKSET_ITEM_HEADING_SOURCE:
            snprintf(out_label, label_len, "Heading Src");
            snprintf(out_value, value_len, "%s", Vario_Settings_GetHeadingSourceText());
            break;

        case VARIO_QUICKSET_ITEM_VARIO_DAMPING:
            snprintf(out_label, label_len, "Damping");
            snprintf(out_value, value_len, "%u/10", (unsigned)settings->vario_damping_level);
            break;

        case VARIO_QUICKSET_ITEM_VARIO_AVG_SECONDS:
            snprintf(out_label, label_len, "Int Avg");
            snprintf(out_value, value_len, "%u s", (unsigned)settings->digital_vario_average_seconds);
            break;

        case VARIO_QUICKSET_ITEM_CLIMB_TONE_THRESHOLD:
            snprintf(out_label, label_len, "Climb Tone");
            vario_quickset_format_vspeed_threshold(out_value,
                                                  value_len,
                                                  settings->climb_tone_threshold_cms);
            break;

        case VARIO_QUICKSET_ITEM_SINK_TONE_THRESHOLD:
            snprintf(out_label, label_len, "Sink Tone");
            vario_quickset_format_vspeed_threshold(out_value,
                                                  value_len,
                                                  settings->sink_tone_threshold_cms);
            break;

        case VARIO_QUICKSET_ITEM_FLIGHT_START_SPEED:
            snprintf(out_label, label_len, "Flight Start");
            vario_quickset_format_speed_threshold(out_value,
                                                  value_len,
                                                  settings->flight_start_speed_kmh_x10);
            break;

        case VARIO_QUICKSET_ITEM_BEEP_ONLY_WHEN_FLYING:
            snprintf(out_label, label_len, "Beep Gate");
            snprintf(out_value, value_len, "%s", Vario_Settings_GetBeepModeText());
            break;

        case VARIO_QUICKSET_ITEM_AUDIO_ENABLE:
            snprintf(out_label, label_len, "Audio");
            snprintf(out_value, value_len, "%s", Vario_Settings_GetAudioOnOffText());
            break;

        case VARIO_QUICKSET_ITEM_AUDIO_VOLUME:
            snprintf(out_label, label_len, "Volume");
            snprintf(out_value, value_len, "%u%%", (unsigned)settings->audio_volume_percent);
            break;

        case VARIO_QUICKSET_ITEM_ALT2_CAPTURE:
            snprintf(out_label, label_len, "ALT2 Capture");
            vario_quickset_format_alt2_ref(out_value, value_len, settings->alt2_reference_cm);
            break;

        case VARIO_QUICKSET_ITEM_ALT3_RESET:
            snprintf(out_label, label_len, "ALT3 Reset");
            snprintf(out_value, value_len, "press +/-");
            break;

        case VARIO_QUICKSET_ITEM_FLIGHT_RESET:
            snprintf(out_label, label_len, "Flight Reset");
            snprintf(out_value, value_len, "press +/-");
            break;

        case VARIO_QUICKSET_ITEM_COUNT:
        default:
            snprintf(out_label, label_len, "-");
            snprintf(out_value, value_len, "-");
            break;
    }
}

void Vario_QuickSet_Render(u8g2_t *u8g2, const vario_buttonbar_t *buttonbar)
{
    const vario_viewport_t *v;
    const vario_settings_t *settings;
    uint8_t                 cursor;
    uint8_t                 first;
    uint8_t                 visible;
    uint8_t                 i;

    (void)buttonbar;

    v        = Vario_Display_GetContentViewport();
    settings = Vario_Settings_Get();
    cursor   = Vario_State_GetQuickSetCursor();
    visible  = VARIO_QUICKSET_VISIBLE_ROWS;

    if (cursor > 2u)
    {
        first = (uint8_t)(cursor - 2u);
    }
    else
    {
        first = 0u;
    }

    if ((first + visible) > (uint8_t)VARIO_QUICKSET_ITEM_COUNT)
    {
        if ((uint8_t)VARIO_QUICKSET_ITEM_COUNT > visible)
        {
            first = (uint8_t)((uint8_t)VARIO_QUICKSET_ITEM_COUNT - visible);
        }
        else
        {
            first = 0u;
        }
    }

    Vario_Display_DrawPageTitle(u8g2, v, "INSTRUMENT", "UNITS / AUDIO");

    for (i = 0u; i < visible; ++i)
    {
        uint8_t item_index;
        char    label[24];
        char    value[24];
        int16_t row_y;

        item_index = (uint8_t)(first + i);
        if (item_index >= (uint8_t)VARIO_QUICKSET_ITEM_COUNT)
        {
            break;
        }

        row_y = (int16_t)(v->y + 28 + ((int16_t)i * VARIO_QUICKSET_ROW_PITCH));
        vario_quickset_get_item_text((vario_quickset_item_t)item_index,
                                     settings,
                                     label,
                                     sizeof(label),
                                     value,
                                     sizeof(value));

        Vario_Display_DrawMenuRow(u8g2,
                                  v,
                                  row_y,
                                  (cursor == item_index),
                                  label,
                                  value);
    }

    {
        char pos_text[20];
        snprintf(pos_text,
                 sizeof(pos_text),
                 "%u/%u",
                 (unsigned)(cursor + 1u),
                 (unsigned)VARIO_QUICKSET_ITEM_COUNT);
        u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
        Vario_Display_DrawTextRight(u8g2,
                                    (int16_t)(v->x + v->w - 6),
                                    (int16_t)(v->y + v->h - 6),
                                    pos_text);
        u8g2_DrawStr(u8g2,
                     (uint8_t)(v->x + 8),
                     (uint8_t)(v->y + v->h - 6),
                     "F6: Display page");
    }
}
