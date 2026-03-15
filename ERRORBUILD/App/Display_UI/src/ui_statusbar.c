#include "ui_statusbar.h"
#include "ui_common_icons.h"

#include <stdio.h>

/* -------------------------------------------------------------------------- */
/*  Font / geometry                                                            */
/*                                                                            */
/*  상단바의 세로 위치는 절대 고정이다.                                        */
/*  - 모든 아이콘의 Y는 기존과 완전히 동일하게 0                              */
/*  - 모든 텍스트의 baseline Y도 기존과 완전히 동일하게 7                     */
/*                                                                            */
/*  이번 수정에서 바뀌는 것은 "세 개의 왼쪽 아이콘의 X" 뿐이다.                */
/*  그리고 그마저도 record / bluetooth / SD 세 요소만 재배치한다.             */
/*  나머지 GPS, 온도, 시간 그룹의 X/Y는 기존 좌표를 1픽셀도 건드리지 않는다.  */
/*                                                                            */
/*  배치 계산                                                                  */
/*  - record 아이콘 : x = 0, width = 7  -> 점유 범위 0..6                     */
/*  - SD 아이콘     : 기존 45에서 정확히 5px 왼쪽으로 이동 -> x = 40          */
/*  - Bluetooth     : record 끝(7)과 SD 시작(40) 사이 중앙 배치                */
/*                   gap = 40 - 7 = 33                                         */
/*                   BT width = 7, 남는 여백 = 26, 좌우 균등 여백 = 13         */
/*                   따라서 BT x = 7 + 13 = 20                                */
/* -------------------------------------------------------------------------- */
#define UI_STATUSBAR_FONT             u8g2_font_6x12_mf
#define UI_STATUSBAR_X_RECORD_ICON    0
#define UI_STATUSBAR_X_BT_ICON        20
#define UI_STATUSBAR_X_SD_ICON        40
#define UI_STATUSBAR_X_GPS_GROUP      80
#define UI_STATUSBAR_X_TEMP           138
#define UI_STATUSBAR_X_TIME           168
#define UI_STATUSBAR_Y_ICON           0
#define UI_STATUSBAR_Y_TEXT           7

/* -------------------------------------------------------------------------- */
/*  Cold temperature warning policy                                            */
/* -------------------------------------------------------------------------- */
#define TEMP_COLD_WARN_ON_C            4
#define TEMP_COLD_WARN_OFF_C           6
#define TEMP_COLD_WARN_ON_X100         (TEMP_COLD_WARN_ON_C * 100)
#define TEMP_COLD_WARN_OFF_X100        (TEMP_COLD_WARN_OFF_C * 100)
#define TEMP_COLD_WARN_BLINK_MS        (10u * 1000u)

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
static uint8_t           s_reserved_height_cache = 0u;

/* -------------------------------------------------------------------------- */
/*  Height helper                                                              */
/*                                                                            */
/*  상단바 명목 높이는 7px 그대로 유지한다.                                    */
/*  다만 6x12 폰트 descender 때문에 실제 점유 영역은 더 아래까지 내려오므로,     */
/*  본문 뷰포트가 그 영역과 싸우지 않게 font metric 기준으로 한 번 계산한다.    */
/* -------------------------------------------------------------------------- */
uint8_t UI_StatusBar_GetReservedHeight(u8g2_t *u8g2)
{
    int16_t reserved;
    int16_t descent;
    int16_t icon_bottom;
    int16_t text_bottom;

    if (s_reserved_height_cache != 0u)
    {
        return s_reserved_height_cache;
    }

    reserved = UI_STATUSBAR_H;

    icon_bottom = (int16_t)(UI_STATUSBAR_Y_ICON + ICON11_H);
    if (icon_bottom > reserved)
    {
        reserved = icon_bottom;
    }

    if (u8g2 != 0)
    {
        u8g2_SetFont(u8g2, UI_STATUSBAR_FONT);
        descent = (int16_t)u8g2_GetDescent(u8g2);
        if (descent < 0)
        {
            descent = (int16_t)(-descent);
        }

        text_bottom = (int16_t)(UI_STATUSBAR_Y_TEXT + descent);
        if (text_bottom > reserved)
        {
            reserved = text_bottom;
        }
    }

    if (reserved < UI_STATUSBAR_H)
    {
        reserved = UI_STATUSBAR_H;
    }

    s_reserved_height_cache = (uint8_t)reserved;
    return s_reserved_height_cache;
}

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
    uint8_t box_h;

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

    box_h = UI_StatusBar_GetReservedHeight(u8g2);

    u8g2_SetDrawColor(u8g2, 2);
    u8g2_DrawBox(u8g2,
                 (u8g2_uint_t)s_temp_box_x,
                 0u,
                 (u8g2_uint_t)s_temp_box_w,
                 (u8g2_uint_t)box_h);
    u8g2_SetDrawColor(u8g2, 1);
}

/* -------------------------------------------------------------------------- */
/*  Weekday text                                                               */
/*                                                                            */
/*  업로드된 기존 statusbar.c 로직을 그대로 따라간다.                          */
/*  - 0 = SUN                                                                  */
/*  - 6 = SAT                                                                  */
/* -------------------------------------------------------------------------- */
static const char *ui_statusbar_weekday_text(uint8_t weekday)
{
    static const char *dow_table[7] =
    {
        "SUN", "MON", "TUE", "WED", "THU", "FRI", "SAT"
    };

    if (weekday <= 6u)
    {
        return dow_table[weekday];
    }

    return "---";
}

/* -------------------------------------------------------------------------- */
/*  SD state to icon selection                                                 */
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
    u8g2_SetFont(u8g2, UI_STATUSBAR_FONT);

    /* ---------------------------------------------------------------------- */
    /*  1) REC / STOP / PAUSE 아이콘                                            */
    /*                                                                            */
    /*  여기서는 7x7 상태 아이콘만 그린다.                                       */
    /*  문자열 "REC" 는 의도적으로 그리지 않는다.                               */
    /*  따라서 아이콘 오른쪽에 rec 텍스트가 새로 찍히는 일은 이 파일 기준으로   */
    /*  발생하지 않는다.                                                        */
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
    /*  2) Bluetooth icon only                                                 */
    /*                                                                            */
    /*  이전 패키지의 정체불명 보조 아이콘은 제거했다.                           */
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
        }
    }

    /* ---------------------------------------------------------------------- */
    /*  3) SD icon                                                              */
    /*                                                                            */
    /*  SD 아이콘은 사용자의 요구대로 절대 좌표 x = 40 에 고정한다.             */
    /*  이 값은 기존 x = 45 에서 정확히 5px 왼쪽으로 옮긴 결과다.               */
    /*  Y는 status bar 아이콘 공통 규칙에 따라 기존과 동일하게 0을 유지한다.    */
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

    u8g2_DrawXBM(u8g2,
                 (u8g2_uint_t)x,
                 UI_STATUSBAR_Y_ICON,
                 ICON7_W,
                 ICON7_H,
                 icon_gps_main_bits);
    x += ICON7_W + 2;

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
    /*  5) Temperature string                                                  */
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
    /*  6) Time string                                                         */
    /*                                                                            */
    /*  시계 소스는 기존 상태바 구현과 동일하게 g_app_state.time 계열을 사용한다. */
    /* ---------------------------------------------------------------------- */
    if ((model->time_valid != false) &&
        (model->time_year >= 1980u) && (model->time_year <= 2099u) &&
        (model->time_month >= 1u) && (model->time_month <= 12u) &&
        (model->time_day >= 1u) && (model->time_day <= 31u))
    {
        snprintf(time_str,
                 sizeof(time_str),
                 "%s %02u %02u:%02u",
                 ui_statusbar_weekday_text(model->time_weekday),
                 (unsigned)model->time_day,
                 (unsigned)model->time_hour,
                 (unsigned)model->time_minute);
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
