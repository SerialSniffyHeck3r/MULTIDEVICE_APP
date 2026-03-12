#include "ui_statusbar.h"
#include "ui_common_icons.h"

#include <stdio.h>

/* -------------------------------------------------------------------------- */
/*  Cold temperature warning policy                                            */
/* -------------------------------------------------------------------------- */
#define TEMP_COLD_WARN_ON_C            4
#define TEMP_COLD_WARN_OFF_C           6
#define TEMP_COLD_WARN_ON_X100         (TEMP_COLD_WARN_ON_C * 100)
#define TEMP_COLD_WARN_OFF_X100        (TEMP_COLD_WARN_OFF_C * 100)
#define TEMP_COLD_WARN_BLINK_MS        (10u * 1000u)

/* -------------------------------------------------------------------------- */
/*  Absolute X positions                                                       */
/*                                                                            */
/*  사용자가 요청한 대로 "상대 위치 보정" 로직을 없애고,                        */
/*  status bar의 기준 X를 절대 좌표로 고정한다.                                */
/* -------------------------------------------------------------------------- */
#define UI_STATUSBAR_X_RECORD_ICON     0
#define UI_STATUSBAR_X_BT_ICON         12
#define UI_STATUSBAR_X_BT_AUX_ICON     21
#define UI_STATUSBAR_X_SD_ICON         45
#define UI_STATUSBAR_X_GPS_GROUP       80
#define UI_STATUSBAR_X_TEMP            138
#define UI_STATUSBAR_X_TIME            168

#define UI_STATUSBAR_Y_ICON            0
#define UI_STATUSBAR_Y_TEXT            7

typedef enum
{
    TEMP_COLD_STATE_NORMAL = 0,
    TEMP_COLD_STATE_BLINK,
    TEMP_COLD_STATE_INVERTED
} temp_cold_state_t;

static temp_cold_state_t s_temp_cold_state = TEMP_COLD_STATE_NORMAL;
static uint32_t          s_temp_cold_start_ms = 0u;
static int16_t           s_temp_box_x = 0;
static int16_t           s_temp_box_w = 0;

/* -------------------------------------------------------------------------- */
/*  HDOP quality helper                                                        */
/* -------------------------------------------------------------------------- */
static int ui_statusbar_gps_quality_from_hdop(uint16_t pdop_x100, uint8_t gps_fix_type)
{
    float hdop;

    if ((gps_fix_type == 0u) || (pdop_x100 == 0u))
    {
        return 1;
    }

    hdop = ((float)pdop_x100) * 0.01f;

    if (hdop < 0.8f)
    {
        return 4;
    }
    else if (hdop < 1.5f)
    {
        return 3;
    }
    else if (hdop < 3.0f)
    {
        return 2;
    }

    return 1;
}

/* -------------------------------------------------------------------------- */
/*  Temperature cold warning state machine                                     */
/*                                                                            */
/*  업로드된 프로토타입의 동작을 그대로 유지한다.                               */
/*  - 4도 이하로 내려가면 10초 동안 2Hz blink                                   */
/*  - 그 뒤에는 항상 inverted 유지                                              */
/*  - 6도 이상 올라가면 경고 해제                                               */
/* -------------------------------------------------------------------------- */
static void ui_statusbar_update_temp_cold_warning(const ui_statusbar_model_t *model,
                                                  uint32_t now_ms)
{
    bool temp_ok;
    int16_t temp_c_x100;
    bool below_on;
    bool above_off;

    temp_ok = (((model->temp_status_flags & APP_DS18B20_STATUS_VALID) != 0u) &&
               (model->temp_c_x100 != APP_DS18B20_TEMP_INVALID));

    if (temp_ok == false)
    {
        s_temp_cold_state = TEMP_COLD_STATE_NORMAL;
        s_temp_cold_start_ms = 0u;
        return;
    }

    temp_c_x100 = model->temp_c_x100;
    below_on = (temp_c_x100 <= TEMP_COLD_WARN_ON_X100);
    above_off = (temp_c_x100 >= TEMP_COLD_WARN_OFF_X100);

    switch (s_temp_cold_state)
    {
    case TEMP_COLD_STATE_NORMAL:
        if (below_on != false)
        {
            s_temp_cold_state = TEMP_COLD_STATE_BLINK;
            s_temp_cold_start_ms = now_ms;
        }
        break;

    case TEMP_COLD_STATE_BLINK:
        if (above_off != false)
        {
            s_temp_cold_state = TEMP_COLD_STATE_NORMAL;
            s_temp_cold_start_ms = 0u;
        }
        else if ((now_ms - s_temp_cold_start_ms) >= TEMP_COLD_WARN_BLINK_MS)
        {
            s_temp_cold_state = TEMP_COLD_STATE_INVERTED;
        }
        break;

    case TEMP_COLD_STATE_INVERTED:
    default:
        if (above_off != false)
        {
            s_temp_cold_state = TEMP_COLD_STATE_NORMAL;
            s_temp_cold_start_ms = 0u;
        }
        break;
    }
}

/* -------------------------------------------------------------------------- */
/*  Temperature XOR overlay                                                    */
/* -------------------------------------------------------------------------- */
static void ui_statusbar_draw_temp_cold_overlay(u8g2_t *u8g2)
{
    bool do_xor = false;

    if ((u8g2 == 0) || (s_temp_box_w <= 0))
    {
        return;
    }

    switch (s_temp_cold_state)
    {
    case TEMP_COLD_STATE_BLINK:
        if (SlowToggle2Hz != false)
        {
            do_xor = true;
        }
        break;

    case TEMP_COLD_STATE_INVERTED:
        do_xor = true;
        break;

    case TEMP_COLD_STATE_NORMAL:
    default:
        break;
    }

    if (do_xor == false)
    {
        return;
    }

    u8g2_SetDrawColor(u8g2, 2);
    u8g2_DrawBox(u8g2, (u8g2_uint_t)s_temp_box_x, 0u, (u8g2_uint_t)s_temp_box_w, UI_STATUSBAR_H);
    u8g2_SetDrawColor(u8g2, 1);
}

/* -------------------------------------------------------------------------- */
/*  Clock weekday text                                                         */
/*                                                                            */
/*  현재 프로젝트의 APP_CLOCK weekday는 1..7, Monday=1 규칙이므로               */
/*  그 형식에 맞게 약어를 변환한다.                                             */
/* -------------------------------------------------------------------------- */
static const char *ui_statusbar_weekday_text(uint8_t weekday)
{
    switch (weekday)
    {
    case 1u: return "MON";
    case 2u: return "TUE";
    case 3u: return "WED";
    case 4u: return "THU";
    case 5u: return "FRI";
    case 6u: return "SAT";
    case 7u: return "SUN";
    default: return "---";
    }
}

/* -------------------------------------------------------------------------- */
/*  SD state to icon selection                                                 */
/*                                                                            */
/*  프로토타입의 0/1/2 상태를 현재 APP_STATE의 최소 조건으로 복원한다.          */
/*  - inserted && mounted      -> normal                                         */
/*  - !inserted                -> not present                                    */
/*  - inserted but not mounted -> error                                          */
/* -------------------------------------------------------------------------- */
static const uint8_t *ui_statusbar_get_sd_icon(const ui_statusbar_model_t *model,
                                               bool *visible)
{
    if ((model == 0) || (visible == 0))
    {
        return 0;
    }

    if (model->sd_inserted == false)
    {
        *visible = (SlowToggle2Hz != false);
        return icon_mmc_not_present_bits;
    }

    if ((model->sd_initialized != false) && (model->sd_mounted != false))
    {
        *visible = true;
        return icon_mmc_present_bits;
    }

    *visible = (FastToggle5Hz != false);
    return icon_mmc_error_bits;
}

/* -------------------------------------------------------------------------- */
/*  Main draw                                                                  */
/* -------------------------------------------------------------------------- */
void UI_StatusBar_Draw(u8g2_t *u8g2,
                       const ui_statusbar_model_t *model,
                       uint32_t now_ms)
{
    uint8_t gps_fix_type;
    bool gps_fix_ok;
    uint8_t num_sv_visible;
    uint8_t num_sv_used;
    int quality;
    int rx_level;
    const uint8_t *rx_bmp;
    char sat_buf[4];
    char temp_str[4];
    char time_str[20];
    int temp_whole;
    int temp_w;
    int deg_x;
    bool sd_visible;
    const uint8_t *sd_bmp;
    int x;

    if ((u8g2 == 0) || (model == 0))
    {
        return;
    }

    u8g2_SetDrawColor(u8g2, 1);
    u8g2_SetFont(u8g2, u8g2_font_6x12_mf);

    /* ---------------------------------------------------------------------- */
    /*  1) Recording icon only                                                 */
    /*                                                                        */
    /*  REC text는 제거하고 아이콘만 유지한다.                                  */
    /* ---------------------------------------------------------------------- */
    switch (model->record_state)
    {
    case UI_RECORD_STATE_REC:
        u8g2_DrawXBM(u8g2,
                     UI_STATUSBAR_X_RECORD_ICON,
                     UI_STATUSBAR_Y_ICON,
                     ICON7_W,
                     ICON7_H,
                     icon_rec_bits);
        break;

    case UI_RECORD_STATE_PAUSE:
        if (SlowToggle2Hz != false)
        {
            u8g2_DrawXBM(u8g2,
                         UI_STATUSBAR_X_RECORD_ICON,
                         UI_STATUSBAR_Y_ICON,
                         ICON7_W,
                         ICON7_H,
                         icon_pause_bits);
        }
        else
        {
            u8g2_DrawXBM(u8g2,
                         UI_STATUSBAR_X_RECORD_ICON,
                         UI_STATUSBAR_Y_ICON,
                         ICON7_W,
                         ICON7_H,
                         blank_7x7);
        }
        break;

    case UI_RECORD_STATE_STOP:
    default:
        u8g2_DrawXBM(u8g2,
                     UI_STATUSBAR_X_RECORD_ICON,
                     UI_STATUSBAR_Y_ICON,
                     ICON7_W,
                     ICON7_H,
                     icon_stop_bits);
        break;
    }

    /* ---------------------------------------------------------------------- */
    /*  2) Bluetooth stub icons                                                */
    /* ---------------------------------------------------------------------- */
    if (model->bluetooth_stub_state != UI_BT_STUB_OFF)
    {
        bool draw_bt = true;

        if (model->bluetooth_stub_state == UI_BT_STUB_BLINK)
        {
            draw_bt = (SlowToggle2Hz != false);
        }

        if (draw_bt != false)
        {
            u8g2_DrawXBM(u8g2,
                         UI_STATUSBAR_X_BT_ICON,
                         UI_STATUSBAR_Y_ICON,
                         ICON7_W,
                         ICON7_H,
                         icon_bluetooth_bits);

            if (model->bluetooth_aux_visible != 0u)
            {
                u8g2_DrawXBM(u8g2,
                             UI_STATUSBAR_X_BT_AUX_ICON,
                             UI_STATUSBAR_Y_ICON,
                             ICON7_W,
                             ICON7_H,
                             icon_bt_aux_bits);
            }
        }
    }

    /* ---------------------------------------------------------------------- */
    /*  3) SD icon, absolute X = original 40 + 5px                             */
    /* ---------------------------------------------------------------------- */
    sd_bmp = ui_statusbar_get_sd_icon(model, &sd_visible);
    if ((sd_visible != false) && (sd_bmp != 0))
    {
        u8g2_DrawXBM(u8g2,
                     UI_STATUSBAR_X_SD_ICON,
                     UI_STATUSBAR_Y_ICON,
                     ICON7_W,
                     ICON7_H,
                     sd_bmp);
    }

    /* ---------------------------------------------------------------------- */
    /*  4) GPS block                                                            */
    /* ---------------------------------------------------------------------- */
    gps_fix_type = model->gps_fix.fixType;
    gps_fix_ok = ((model->gps_fix.fixOk != false) && (model->gps_fix.valid != false));
    num_sv_visible = model->gps_fix.numSV_visible;
    num_sv_used = model->gps_fix.numSV_used;
    quality = ui_statusbar_gps_quality_from_hdop(model->gps_fix.pDOP, gps_fix_type);

    x = UI_STATUSBAR_X_GPS_GROUP;

    /* 4-1) GPS main icon */
    u8g2_DrawXBM(u8g2, (u8g2_uint_t)x, UI_STATUSBAR_Y_ICON, ICON7_W, ICON7_H, icon_gps_main_bits);
    x += ICON7_W + 2;

    /* 4-2) FIX icon */
    if ((gps_fix_ok == false) || (gps_fix_type == 0u))
    {
        if (SlowToggle2Hz != false)
        {
            u8g2_DrawXBM(u8g2, (u8g2_uint_t)x, UI_STATUSBAR_Y_ICON, ICON11_W, ICON11_H, icon_gps_nofix_bits);
        }
        else
        {
            u8g2_DrawXBM(u8g2, (u8g2_uint_t)x, UI_STATUSBAR_Y_ICON, ICON11_W, ICON11_H, blank_11x7);
        }
    }
    else if (gps_fix_type >= 3u)
    {
        if (quality >= 3)
        {
            u8g2_DrawXBM(u8g2, (u8g2_uint_t)x, UI_STATUSBAR_Y_ICON, ICON11_W, ICON11_H, icon_gps_3d_bits);
        }
        else if (SlowToggle2Hz != false)
        {
            u8g2_DrawXBM(u8g2, (u8g2_uint_t)x, UI_STATUSBAR_Y_ICON, ICON11_W, ICON11_H, icon_gps_3d_bits);
        }
        else
        {
            u8g2_DrawXBM(u8g2, (u8g2_uint_t)x, UI_STATUSBAR_Y_ICON, ICON11_W, ICON11_H, blank_11x7);
        }
    }
    else if (gps_fix_type == 2u)
    {
        if (SlowToggle2Hz != false)
        {
            u8g2_DrawXBM(u8g2, (u8g2_uint_t)x, UI_STATUSBAR_Y_ICON, ICON11_W, ICON11_H, icon_gps_2d_bits);
        }
        else
        {
            u8g2_DrawXBM(u8g2, (u8g2_uint_t)x, UI_STATUSBAR_Y_ICON, ICON11_W, ICON11_H, blank_11x7);
        }
    }
    else
    {
        if (SlowToggle2Hz != false)
        {
            u8g2_DrawXBM(u8g2, (u8g2_uint_t)x, UI_STATUSBAR_Y_ICON, ICON11_W, ICON11_H, icon_gps_nofix_bits);
        }
        else
        {
            u8g2_DrawXBM(u8g2, (u8g2_uint_t)x, UI_STATUSBAR_Y_ICON, ICON11_W, ICON11_H, blank_11x7);
        }
    }
    x += ICON11_W + 2;

    /* 4-3) satellite count */
    if ((model->gps_fix.valid == false) &&
        (gps_fix_type == 0u) &&
        (num_sv_visible == 0u) &&
        (num_sv_used == 0u) &&
        (model->gps_fix.pDOP == 0u))
    {
        snprintf(sat_buf, sizeof(sat_buf), "--");
    }
    else if (num_sv_used <= 2u)
    {
        if (FastToggle5Hz != false)
        {
            snprintf(sat_buf, sizeof(sat_buf), "%2u", (unsigned)((num_sv_visible > 99u) ? 99u : num_sv_visible));
        }
        else
        {
            snprintf(sat_buf, sizeof(sat_buf), "  ");
        }
    }
    else
    {
        snprintf(sat_buf, sizeof(sat_buf), "%2u", (unsigned)((num_sv_used > 99u) ? 99u : num_sv_used));
    }

    u8g2_DrawStr(u8g2, (u8g2_uint_t)x, UI_STATUSBAR_Y_TEXT, sat_buf);
    x += (int)u8g2_GetStrWidth(u8g2, sat_buf);

    /* 4-4) antenna + RX level */
    x += 2;
    u8g2_DrawXBM(u8g2, (u8g2_uint_t)x, UI_STATUSBAR_Y_ICON, ICON5_W, ICON5_H, icon_antenna_shape);
    x += ICON5_W + 1;

    if ((gps_fix_ok != false) && (gps_fix_type >= 2u) && (model->gps_fix.pDOP != 0u))
    {
        float hdop = ((float)model->gps_fix.pDOP) * 0.01f;

        if (hdop <= 0.5f)
        {
            rx_level = 7;
        }
        else if (hdop <= 0.8f)
        {
            rx_level = 6;
        }
        else if (hdop <= 1.2f)
        {
            rx_level = 5;
        }
        else if (hdop <= 2.0f)
        {
            rx_level = 4;
        }
        else if (hdop <= 3.0f)
        {
            rx_level = 3;
        }
        else if (hdop <= 6.0f)
        {
            rx_level = 2;
        }
        else
        {
            rx_level = 1;
        }
    }
    else
    {
        rx_level = 1;
    }

    switch (rx_level)
    {
    case 7:  rx_bmp = icon_gps_rx_7_bits; break;
    case 6:  rx_bmp = icon_gps_rx_6_bits; break;
    case 5:  rx_bmp = icon_gps_rx_5_bits; break;
    case 4:  rx_bmp = icon_gps_rx_4_bits; break;
    case 3:  rx_bmp = icon_gps_rx_3_bits; break;
    case 2:  rx_bmp = icon_gps_rx_2_bits; break;
    case 1:
    default: rx_bmp = icon_gps_rx_1_bits; break;
    }

    u8g2_DrawXBM(u8g2, (u8g2_uint_t)x, UI_STATUSBAR_Y_ICON, ICON7_W, ICON7_H, rx_bmp);

    /* ---------------------------------------------------------------------- */
    /*  5) Temperature string, absolute X                                      */
    /* ---------------------------------------------------------------------- */
    if (((model->temp_status_flags & APP_DS18B20_STATUS_VALID) == 0u) ||
        (model->temp_c_x100 == APP_DS18B20_TEMP_INVALID))
    {
        int err = (int)model->temp_last_error;
        if (err < 0)
        {
            err = -err;
        }
        if (err > 9)
        {
            err = 9;
        }

        temp_str[0] = 'E';
        temp_str[1] = ' ';
        temp_str[2] = (char)('0' + (err % 10));
        temp_str[3] = '\0';
    }
    else
    {
        int t100 = (model->imperial_units != 0u) ? (int)model->temp_f_x100
                                                 : (int)model->temp_c_x100;
        char c0 = ' ';
        char c1 = ' ';
        char c2 = ' ';
        int v;

        if (t100 >= 0)
        {
            temp_whole = (t100 + 50) / 100;
        }
        else
        {
            temp_whole = (t100 - 50) / 100;
        }

        if (temp_whole > 99)
        {
            temp_whole = 99;
        }
        if (temp_whole < -99)
        {
            temp_whole = -99;
        }

        if (temp_whole < 0)
        {
            v = -temp_whole;
            c0 = '-';
            if (v >= 10)
            {
                c1 = (char)('0' + (v / 10));
                c2 = (char)('0' + (v % 10));
            }
            else
            {
                c2 = (char)('0' + v);
            }
        }
        else
        {
            v = temp_whole;
            if (v >= 10)
            {
                c1 = (char)('0' + (v / 10));
                c2 = (char)('0' + (v % 10));
            }
            else
            {
                c2 = (char)('0' + v);
            }
        }

        temp_str[0] = c0;
        temp_str[1] = c1;
        temp_str[2] = c2;
        temp_str[3] = '\0';
    }

    ui_statusbar_update_temp_cold_warning(model, now_ms);

    u8g2_DrawStr(u8g2, UI_STATUSBAR_X_TEMP, UI_STATUSBAR_Y_TEXT, temp_str);
    temp_w = (int)u8g2_GetStrWidth(u8g2, temp_str);

    deg_x = UI_STATUSBAR_X_TEMP + temp_w;
    u8g2_DrawXBM(u8g2, (u8g2_uint_t)deg_x, UI_STATUSBAR_Y_ICON, ICON7_W, ICON7_H, icon_degrees);

    s_temp_box_x = UI_STATUSBAR_X_TEMP;
    s_temp_box_w = (deg_x + ICON7_W) - UI_STATUSBAR_X_TEMP;
    if (s_temp_box_w < 0)
    {
        s_temp_box_w = 0;
    }

    /* ---------------------------------------------------------------------- */
    /*  6) Time string, absolute X                                             */
    /* ---------------------------------------------------------------------- */
    if ((model->clock_valid != false) &&
        (model->clock_year >= 1980u) && (model->clock_year <= 2099u) &&
        (model->clock_month >= 1u) && (model->clock_month <= 12u) &&
        (model->clock_day >= 1u) && (model->clock_day <= 31u))
    {
        snprintf(time_str,
                 sizeof(time_str),
                 "%s %02u %02u:%02u",
                 ui_statusbar_weekday_text(model->clock_weekday),
                 (unsigned)model->clock_day,
                 (unsigned)model->clock_hour,
                 (unsigned)model->clock_minute);
    }
    else
    {
        snprintf(time_str, sizeof(time_str), "--- -- --:--");
    }

    u8g2_DrawStr(u8g2, UI_STATUSBAR_X_TIME, UI_STATUSBAR_Y_TEXT, time_str);

    /* ---------------------------------------------------------------------- */
    /*  7) Cold warning overlay                                                */
    /* ---------------------------------------------------------------------- */
    ui_statusbar_draw_temp_cold_overlay(u8g2);
}
