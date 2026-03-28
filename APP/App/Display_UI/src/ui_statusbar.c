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
/*  GPS display hysteresis state                                               */
/*                                                                            */
/*  상단바의 GPS 아이콘/수신칸/위성수는 "원본 raw 샘플" 을 그대로 1:1로           */
/*  따라가지 않는다.                                                           */
/*                                                                            */
/*  이유                                                                       */
/*  - NAV-PVT/NAV-SAT가 10 Hz로 들어오면 fixOk, fixType, pDOP, numSV_used 가    */
/*    재획득/가중치 재평가/메시지 경계에서 한두 샘플 흔들릴 수 있다.              */
/*  - 그런데 상태바는 사용자에게 "지금 실제 체감 상태가 어떤가" 를               */
/*    한눈에 보여주는 곳이라, 1샘플 튐을 그대로 깜빡임으로 번역하면               */
/*    오히려 정보 가치가 떨어진다.                                             */
/*                                                                            */
/*  따라서 여기서는                                                            */
/*  - 좋아지는 변화는 즉시 반영                                                 */
/*  - 나빠지는 변화는 짧은 유지 시간 이후에만 반영                             */
/*  - 수신 bar는 pDOP뿐 아니라 hAcc/vAcc도 함께 참조                           */
/*  - fix 아이콘은 time/date valid 같은 "항법 외 부가 조건" 에 덜 민감하게       */
/*    geometry 상태를 latched 하여 표현                                        */
/*  하는 표시 전용 hysteresis를 둔다.                                          */
/* -------------------------------------------------------------------------- */
#define UI_STATUSBAR_GPS_HOLD_3D_MS         1200u
#define UI_STATUSBAR_GPS_HOLD_2D_MS          700u
#define UI_STATUSBAR_GPS_RX_FALL_HOLD_MS     900u
#define UI_STATUSBAR_GPS_SV_HOLD_MS         1200u

typedef enum
{
    UI_STATUSBAR_GPS_FIX_STATE_NOFIX = 0u,
    UI_STATUSBAR_GPS_FIX_STATE_2D    = 2u,
    UI_STATUSBAR_GPS_FIX_STATE_3D    = 3u
} ui_statusbar_gps_fix_state_t;

typedef struct
{
    bool     initialized;
    uint8_t  display_fix_state;
    uint8_t  display_num_sv_visible;
    uint8_t  display_num_sv_used;
    int      display_rx_level;
    uint32_t last_seen_any_fix_ms;
    uint32_t last_seen_3d_fix_ms;
    uint32_t last_rx_reinforce_ms;
    uint32_t last_visible_sv_ms;
    uint32_t last_used_sv_ms;
} ui_statusbar_gps_display_state_t;

static ui_statusbar_gps_display_state_t s_gps_display = {0};

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
/*  GPS display helpers                                                        */
/* -------------------------------------------------------------------------- */
static ui_statusbar_gps_fix_state_t ui_statusbar_gps_candidate_fix_state_from_fix(const gps_fix_basic_t *fix,
                                                                                   uint32_t now_ms)
{
    bool fresh;
    bool has_quality_evidence;

    if (fix == 0)
    {
        return UI_STATUSBAR_GPS_FIX_STATE_NOFIX;
    }

    /* ---------------------------------------------------------------------- */
    /*  stale data 방어                                                         */
    /*                                                                        */
    /*  fixType만 남아 있고 실제 새 메시지가 끊긴 상태에서는                    */
    /*  "과거 3D" 가 상단바에 얼어붙으면 안 되므로,                            */
    /*  last_update_ms가 오래된 경우는 즉시 candidate를 NO FIX로 본다.          */
    /* ---------------------------------------------------------------------- */
    fresh = ((fix->last_update_ms != 0u) &&
             ((now_ms - fix->last_update_ms) <= UI_STATUSBAR_GPS_HOLD_3D_MS));

    if (fresh == false)
    {
        return UI_STATUSBAR_GPS_FIX_STATE_NOFIX;
    }

    has_quality_evidence =
        (fix->pDOP != 0u) ||
        (fix->hAcc != 0u) ||
        (fix->vAcc != 0u) ||
        (fix->numSV_used != 0u) ||
        (fix->numSV_visible != 0u);

    if ((fix->fixType >= 3u) && ((fix->fixOk != false) || (has_quality_evidence != false)))
    {
        return UI_STATUSBAR_GPS_FIX_STATE_3D;
    }

    if ((fix->fixType == 2u) && ((fix->fixOk != false) || (has_quality_evidence != false)))
    {
        return UI_STATUSBAR_GPS_FIX_STATE_2D;
    }

    return UI_STATUSBAR_GPS_FIX_STATE_NOFIX;
}

/* -------------------------------------------------------------------------- */
/*  Accuracy -> RX level                                                       */
/*                                                                            */
/*  상태바의 안테나 수신칸은 "전파 세기" 전용 계측이 아니라                    */
/*  사용자 체감 품질용 추정치다.                                               */
/*                                                                            */
/*  그래서 여기서는                                                            */
/*  - pDOP 기반 geometry 추정                                                  */
/*  - hAcc/vAcc 기반 실제 위치/고도 정확도 추정                                */
/*  두 축을 함께 보고 더 설득력 있는 쪽을 택한다.                              */
/* -------------------------------------------------------------------------- */
static int ui_statusbar_gps_rx_level_from_accuracy_mm(uint32_t acc_mm)
{
    if (acc_mm == 0u)
    {
        return 0;
    }

    if (acc_mm <= 500u)  { return 7; }
    if (acc_mm <= 800u)  { return 6; }
    if (acc_mm <= 1200u) { return 5; }
    if (acc_mm <= 2000u) { return 4; }
    if (acc_mm <= 3000u) { return 3; }
    if (acc_mm <= 6000u) { return 2; }
    return 1;
}

static int ui_statusbar_gps_rx_level_from_pdop(uint16_t pdop_x100, uint8_t fix_type)
{
    if ((fix_type < 2u) || (pdop_x100 == 0u))
    {
        return 0;
    }

    if (pdop_x100 <= 60u)  { return 7; }
    if (pdop_x100 <= 90u)  { return 6; }
    if (pdop_x100 <= 130u) { return 5; }
    if (pdop_x100 <= 220u) { return 4; }
    if (pdop_x100 <= 320u) { return 3; }
    if (pdop_x100 <= 650u) { return 2; }
    return 1;
}

static int ui_statusbar_gps_rx_level_from_fix(const gps_fix_basic_t *fix)
{
    uint32_t worst_acc_mm;
    int acc_level;
    int dop_level;

    if (fix == 0)
    {
        return 1;
    }

    if (fix->fixType < 2u)
    {
        return 1;
    }

    worst_acc_mm = fix->hAcc;
    if ((fix->vAcc != 0u) && ((worst_acc_mm == 0u) || (fix->vAcc > worst_acc_mm)))
    {
        worst_acc_mm = fix->vAcc;
    }

    acc_level = ui_statusbar_gps_rx_level_from_accuracy_mm(worst_acc_mm);
    dop_level = ui_statusbar_gps_rx_level_from_pdop(fix->pDOP, fix->fixType);

    if (acc_level > dop_level)
    {
        return acc_level;
    }

    if (dop_level > 0)
    {
        return dop_level;
    }

    return 1;
}

static int ui_statusbar_gps_quality_from_rx_level(int rx_level)
{
    if (rx_level >= 6)
    {
        return 4;
    }

    if (rx_level >= 5)
    {
        return 3;
    }

    if (rx_level >= 3)
    {
        return 2;
    }

    return 1;
}

/* -------------------------------------------------------------------------- */
/*  GPS hysteresis update                                                      */
/*                                                                            */
/*  입력 raw fix를 받아 상태바 전용 latched 표시 상태를 갱신한다.               */
/*  반환값은 draw 단계가 그대로 쓰는 "이미 안정화된 상태" 다.                  */
/* -------------------------------------------------------------------------- */
static void ui_statusbar_update_gps_display(const gps_fix_basic_t *fix,
                                            uint32_t now_ms,
                                            uint8_t *out_fix_state,
                                            uint8_t *out_num_sv_visible,
                                            uint8_t *out_num_sv_used,
                                            int *out_quality,
                                            int *out_rx_level)
{
    ui_statusbar_gps_fix_state_t candidate_fix_state;
    int candidate_rx_level;

    if ((fix == 0) ||
        (out_fix_state == 0) ||
        (out_num_sv_visible == 0) ||
        (out_num_sv_used == 0) ||
        (out_quality == 0) ||
        (out_rx_level == 0))
    {
        return;
    }

    candidate_fix_state = ui_statusbar_gps_candidate_fix_state_from_fix(fix, now_ms);
    candidate_rx_level = ui_statusbar_gps_rx_level_from_fix(fix);
    if (candidate_rx_level < 1)
    {
        candidate_rx_level = 1;
    }

    if (s_gps_display.initialized == false)
    {
        s_gps_display.initialized = true;
        s_gps_display.display_fix_state = (uint8_t)candidate_fix_state;
        s_gps_display.display_num_sv_visible = fix->numSV_visible;
        s_gps_display.display_num_sv_used = fix->numSV_used;
        s_gps_display.display_rx_level = candidate_rx_level;
        s_gps_display.last_seen_any_fix_ms = now_ms;
        s_gps_display.last_seen_3d_fix_ms = now_ms;
        s_gps_display.last_rx_reinforce_ms = now_ms;
        s_gps_display.last_visible_sv_ms = now_ms;
        s_gps_display.last_used_sv_ms = now_ms;
    }

    if (candidate_fix_state >= UI_STATUSBAR_GPS_FIX_STATE_2D)
    {
        s_gps_display.last_seen_any_fix_ms = now_ms;
    }

    if (candidate_fix_state >= UI_STATUSBAR_GPS_FIX_STATE_3D)
    {
        s_gps_display.last_seen_3d_fix_ms = now_ms;
    }

    if (candidate_fix_state >= (ui_statusbar_gps_fix_state_t)s_gps_display.display_fix_state)
    {
        s_gps_display.display_fix_state = (uint8_t)candidate_fix_state;
    }
    else
    {
        switch ((ui_statusbar_gps_fix_state_t)s_gps_display.display_fix_state)
        {
        case UI_STATUSBAR_GPS_FIX_STATE_3D:
            if ((now_ms - s_gps_display.last_seen_3d_fix_ms) < UI_STATUSBAR_GPS_HOLD_3D_MS)
            {
                s_gps_display.display_fix_state = (uint8_t)UI_STATUSBAR_GPS_FIX_STATE_3D;
            }
            else if ((candidate_fix_state >= UI_STATUSBAR_GPS_FIX_STATE_2D) ||
                     ((now_ms - s_gps_display.last_seen_any_fix_ms) < UI_STATUSBAR_GPS_HOLD_2D_MS))
            {
                s_gps_display.display_fix_state = (uint8_t)UI_STATUSBAR_GPS_FIX_STATE_2D;
            }
            else
            {
                s_gps_display.display_fix_state = (uint8_t)UI_STATUSBAR_GPS_FIX_STATE_NOFIX;
            }
            break;

        case UI_STATUSBAR_GPS_FIX_STATE_2D:
            if ((now_ms - s_gps_display.last_seen_any_fix_ms) < UI_STATUSBAR_GPS_HOLD_2D_MS)
            {
                s_gps_display.display_fix_state = (uint8_t)UI_STATUSBAR_GPS_FIX_STATE_2D;
            }
            else
            {
                s_gps_display.display_fix_state = (uint8_t)candidate_fix_state;
            }
            break;

        case UI_STATUSBAR_GPS_FIX_STATE_NOFIX:
        default:
            s_gps_display.display_fix_state = (uint8_t)candidate_fix_state;
            break;
        }
    }

    if (candidate_fix_state >= UI_STATUSBAR_GPS_FIX_STATE_2D)
    {
        if (candidate_rx_level >= s_gps_display.display_rx_level)
        {
            s_gps_display.display_rx_level = candidate_rx_level;
            s_gps_display.last_rx_reinforce_ms = now_ms;
        }
        else if ((now_ms - s_gps_display.last_rx_reinforce_ms) >= UI_STATUSBAR_GPS_RX_FALL_HOLD_MS)
        {
            s_gps_display.display_rx_level = candidate_rx_level;
            s_gps_display.last_rx_reinforce_ms = now_ms;
        }
    }
    else if ((now_ms - s_gps_display.last_seen_any_fix_ms) >= UI_STATUSBAR_GPS_RX_FALL_HOLD_MS)
    {
        s_gps_display.display_rx_level = 1;
        s_gps_display.last_rx_reinforce_ms = now_ms;
    }

    if (fix->numSV_visible != 0u)
    {
        s_gps_display.display_num_sv_visible = fix->numSV_visible;
        s_gps_display.last_visible_sv_ms = now_ms;
    }
    else if ((now_ms - s_gps_display.last_visible_sv_ms) >= UI_STATUSBAR_GPS_SV_HOLD_MS)
    {
        s_gps_display.display_num_sv_visible = 0u;
    }

    if (fix->numSV_used != 0u)
    {
        s_gps_display.display_num_sv_used = fix->numSV_used;
        s_gps_display.last_used_sv_ms = now_ms;
    }
    else if ((now_ms - s_gps_display.last_used_sv_ms) >= UI_STATUSBAR_GPS_SV_HOLD_MS)
    {
        s_gps_display.display_num_sv_used = 0u;
    }

    *out_fix_state = s_gps_display.display_fix_state;
    *out_num_sv_visible = s_gps_display.display_num_sv_visible;
    *out_num_sv_used = s_gps_display.display_num_sv_used;
    *out_rx_level = s_gps_display.display_rx_level;
    *out_quality = ui_statusbar_gps_quality_from_rx_level(s_gps_display.display_rx_level);
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
    ui_statusbar_update_gps_display(&model->gps_fix,
                                    now_ms,
                                    &gps_fix_type,
                                    &num_sv_visible,
                                    &num_sv_used,
                                    &quality,
                                    &rx_level);

    /* ---------------------------------------------------------------------- */
    /*  상태바의 fix 아이콘 판단 기준                                            */
    /*                                                                        */
    /*  - raw valid는 이 프로젝트에서 valid_date/time까지 포함하는 더 엄격한    */
    /*    조건이고, 상단바의 "잡힘/안 잡힘" 체감과는 결이 다르다.               */
    /*  - raw fixOk 역시 1샘플 튐에 매우 민감하므로, 상단바는 위 helper가        */
    /*    만든 latched fix state를 그대로 사용한다.                             */
    /*  - 기존 지역 변수 gps_fix_ok 는 컴파일 경고 없이 유지되도록               */
    /*    아래의 표시 판단에서도 계속 사용한다.                                  */
    /* ---------------------------------------------------------------------- */
    gps_fix_ok = (gps_fix_type != UI_STATUSBAR_GPS_FIX_STATE_NOFIX);

    x = UI_STATUSBAR_X_GPS_GROUP;

    u8g2_DrawXBM(u8g2,
                 (u8g2_uint_t)x,
                 UI_STATUSBAR_Y_ICON,
                 ICON7_W,
                 ICON7_H,
                 icon_gps_main_bits);
    x += ICON7_W + 2;

    if (gps_fix_ok == false)
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
    else if (gps_fix_type >= UI_STATUSBAR_GPS_FIX_STATE_3D)
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
    else if (gps_fix_type == UI_STATUSBAR_GPS_FIX_STATE_2D)
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

    if ((gps_fix_ok == false) &&
        (num_sv_visible == 0u) &&
        (num_sv_used == 0u) &&
        (model->gps_fix.fixType == 0u) &&
        (model->gps_fix.pDOP == 0u) &&
        (model->gps_fix.hAcc == 0u) &&
        (model->gps_fix.vAcc == 0u))
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
