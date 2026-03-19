#include "ui_screen_gps.h"

#include "ui_bottombar.h"
#include "ui_popup.h"
#include "ui_toast.h"
#include "ui_common_icons.h"
#include "APP_STATE.h"
#include "Ublox_GPS.h"

#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*  External UART handle                                                      */
/*                                                                            */
/*  Ublox_GPS.h 안의 UBLOX_GPS_UART_HANDLE 매크로는 기본적으로 huart2를 뜻한다. */
/*  이 화면 파일은 GPS 드라이버 내부를 건드리지 않고도                        */
/*  UBX 설정 packet만 직접 송신해야 하므로                                    */
/*  같은 UART handle 심볼을 extern으로 참조한다.                              */
/* -------------------------------------------------------------------------- */
extern UART_HandleTypeDef UBLOX_GPS_UART_HANDLE;

/* -------------------------------------------------------------------------- */
/*  Font policy                                                               */
/*                                                                            */
/*  240x128 고정 좌표계에서                                                    */
/*  - 작은 설명 글씨 : 5x8                                                    */
/*  - 정확도 숫자     : 7x13                                                  */
/*  - 좌표 숫자       : 6x12                                                  */
/*  를 사용해 정보량과 겹침 방지를 동시에 잡는다.                              */
/* -------------------------------------------------------------------------- */
#define UI_SCREEN_GPS_FONT_SMALL     u8g2_font_5x8_tr
#define UI_SCREEN_GPS_FONT_MEDIUM    u8g2_font_6x12_mf
#define UI_SCREEN_GPS_FONT_LARGE     u8g2_font_7x13_mf

/* -------------------------------------------------------------------------- */
/*  Fixed layout constants                                                    */
/*                                                                            */
/*  중요한 전제                                                               */
/*  - 이 프로젝트의 현재 활성 LCD 좌표계는 240x128 고정이다.                  */
/*  - TOP+BTM fixed mode의 main viewport는 240x112가 된다.                    */
/*  - 사용자가 요청한 "좌 120 + 우 128" 은 합이 248이라                       */
/*    실제 viewport 240을 초과한다.                                           */
/*                                                                            */
/*  그래서 이 화면은                                                          */
/*  - 우측 plot pane를 128px로 고정                                            */
/*  - 좌측 info pane는 남는 112px 전부 사용                                    */
/*  하도록 설계한다.                                                          */
/*                                                                            */
/*  이렇게 하면                                                              */
/*  - 우측 스카이플롯 정사각형 84x84 확보                                      */
/*  - 하단 CN0 bar graph 28px 확보                                             */
/*  - 좌측 텍스트와 숫자도 겹치지 않음                                         */
/*  을 동시에 만족시킬 수 있다.                                               */
/* -------------------------------------------------------------------------- */
#define UI_SCREEN_GPS_PLOT_PANE_W          128
#define UI_SCREEN_GPS_INFO_MIN_W            96
#define UI_SCREEN_GPS_SKY_SECTION_NUM       3
#define UI_SCREEN_GPS_SKY_SECTION_DEN       4
#define UI_SCREEN_GPS_SKY_BAR_GAP            2
#define UI_SCREEN_GPS_BAR_LABEL_H            8
#define UI_SCREEN_GPS_BAR_MAX_CNO           60u
#define UI_SCREEN_GPS_UBX_TX_TIMEOUT_MS     50u

/* -------------------------------------------------------------------------- */
/*  UBX message constants                                                     */
/*                                                                            */
/*  GPS driver(Ublox_GPS.c)의 private define을 여기로 복사하지 않고            */
/*  GPS 화면이 실제로 쓰는 최소 subset만 독립적으로 다시 선언한다.            */
/*                                                                            */
/*  목적                                                                      */
/*  - 드라이버 파일은 "수신/파싱 전용" 역할 유지                             */
/*  - 화면 파일은 버튼 액션에 필요한 즉시 설정 packet만 송신                   */
/* -------------------------------------------------------------------------- */
#define UI_GPS_UBX_SYNC_1                       0xB5u
#define UI_GPS_UBX_SYNC_2                       0x62u
#define UI_GPS_UBX_CLASS_CFG                    0x06u
#define UI_GPS_UBX_ID_CFG_VALSET                0x8Au
#define UI_GPS_UBX_ID_CFG_RST                   0x04u
#define UI_GPS_UBX_VALSET_LAYER_RAM             0x01u

#define UI_GPS_CFG_KEY_MSGOUT_UBX_NAV_PVT_UART1 0x20910007UL
#define UI_GPS_CFG_KEY_MSGOUT_UBX_NAV_SAT_UART1 0x20910016UL
#define UI_GPS_CFG_KEY_RATE_MEAS                0x30210001UL
#define UI_GPS_CFG_KEY_PM_OPERATEMODE           0x20d00001UL
#define UI_GPS_CFG_KEY_SIGNAL_GPS_ENA           0x1031001fUL
#define UI_GPS_CFG_KEY_SIGNAL_GPS_L1CA_ENA      0x10310001UL
#define UI_GPS_CFG_KEY_SIGNAL_SBAS_ENA          0x10310020UL
#define UI_GPS_CFG_KEY_SIGNAL_SBAS_L1CA_ENA     0x10310005UL
#define UI_GPS_CFG_KEY_SIGNAL_GAL_ENA           0x10310021UL
#define UI_GPS_CFG_KEY_SIGNAL_GAL_E1_ENA        0x10310007UL
#define UI_GPS_CFG_KEY_SIGNAL_BDS_ENA           0x10310022UL
#define UI_GPS_CFG_KEY_SIGNAL_BDS_B1I_ENA       0x1031000dUL
#define UI_GPS_CFG_KEY_SIGNAL_BDS_B1C_ENA       0x1031000fUL
#define UI_GPS_CFG_KEY_SIGNAL_QZSS_ENA          0x10310024UL
#define UI_GPS_CFG_KEY_SIGNAL_QZSS_L1CA_ENA     0x10310012UL
#define UI_GPS_CFG_KEY_SIGNAL_QZSS_L1S_ENA      0x10310014UL
#define UI_GPS_CFG_KEY_SIGNAL_GLO_ENA           0x10310025UL
#define UI_GPS_CFG_KEY_SIGNAL_GLO_L1_ENA        0x10310018UL

#define UI_GPS_PM_OPERATEMODE_FULL              0u
#define UI_GPS_PM_OPERATEMODE_PSMCT             2u

/* -------------------------------------------------------------------------- */
/*  Fix display hysteresis                                                    */
/*                                                                            */
/*  GPS 상세 화면의 FIX 아이콘과 "NO FIX / 2D FIX / 3D FIX" 텍스트는           */
/*  사용자가 상태를 읽기 위한 표시물이다.                                      */
/*                                                                            */
/*  그런데 NAV-PVT가 10 Hz로 들어오는 동안                                   */
/*  - fixOk 비트가 재획득 경계에서 한두 샘플 흔들리거나                        */
/*  - fixType은 유지되는데 fixOk만 순간적으로 0으로 내려가는 경우가 있으면     */
/*  화면이 "3D FIX <-> NO FIX" 를 매우 빠르게 번갈아 그리게 된다.             */
/*                                                                            */
/*  그래서 이 화면에서는                                                       */
/*  - 좋아지는 변화는 즉시 반영                                                */
/*  - 나빠지는 변화는 짧은 hold 이후에만 반영                                  */
/*  - fixOk 한 비트만 보지 않고 fixType / 정확도 / 위성수 같은                */
/*    "항법 해가 실제로 존재한다" 는 보조 증거도 함께 본다                     */
/*  는 표시 전용 hysteresis를 둔다.                                           */
/*                                                                            */
/*  주의                                                                      */
/*  - 이는 화면 표현 안정화용이다.                                            */
/*  - GPS 드라이버의 raw fix 값이나 APP_STATE의 저장 구조는 바꾸지 않는다.     */
/* -------------------------------------------------------------------------- */
#define UI_SCREEN_GPS_FIX_HOLD_3D_MS          1200u
#define UI_SCREEN_GPS_FIX_HOLD_2D_MS           700u

typedef enum
{
    UI_SCREEN_GPS_FIX_STATE_NOFIX = 0u,
    UI_SCREEN_GPS_FIX_STATE_2D    = 2u,
    UI_SCREEN_GPS_FIX_STATE_3D    = 3u
} ui_screen_gps_fix_state_t;

typedef struct
{
    bool     initialized;
    uint8_t  display_fix_state;
    uint32_t last_seen_any_fix_ms;
    uint32_t last_seen_3d_fix_ms;
} ui_screen_gps_fix_display_state_t;

static ui_screen_gps_fix_display_state_t s_ui_screen_gps_fix_display = {0};


/* -------------------------------------------------------------------------- */
/*  Fixed-point trig helper table                                              */
/*                                                                            */
/*  sky plot marker 배치는 sin/cos가 필요하지만,                                */
/*  libm(-lm) 링크 의존성을 만들지 않기 위해                                  */
/*  5도 간격 quarter-wave sine table(0..90도)를 Q10(1024=1.0)로 저장한다.     */
/*  나머지 각도는 사분면 대칭으로 복원하고,                                    */
/*  5도 사이 값은 선형 보간으로 얻는다.                                        */
/*                                                                            */
/*  정밀도                                                                     */
/*  - 이 화면의 위성 마커 반지름은 2px 수준이고                               */
/*  - sky plot 전체 반지름도 35px 안팎이므로                                   */
/*    5도 간격 선형 보간 정밀도로 충분하다.                                    */
/* -------------------------------------------------------------------------- */
static const uint16_t s_ui_screen_gps_sin_q10_0_to_90_by_5deg[] =
{
    0u, 89u, 178u, 265u, 350u, 433u, 512u, 587u, 658u, 724u,
    784u, 839u, 887u, 928u, 962u, 989u, 1008u, 1020u, 1024u
};


/* -------------------------------------------------------------------------- */
/*  Tiny key/value builder for UBX-CFG-VALSET                                 */
/*                                                                            */
/*  U1, U2 key만 쓰면 충분하므로 builder도 아주 작게 유지한다.                  */
/* -------------------------------------------------------------------------- */
typedef struct
{
    uint8_t  buf[128];
    uint16_t len;
} ui_screen_gps_cfg_builder_t;

/* -------------------------------------------------------------------------- */
/*  Local helpers                                                             */
/* -------------------------------------------------------------------------- */
static void ui_screen_gps_configure_bottom_bar(void);
static void ui_screen_gps_u16_to_le(uint8_t *dst, uint16_t value);
static void ui_screen_gps_u32_to_le(uint8_t *dst, uint32_t value);
static void ui_screen_gps_send_ubx(uint8_t cls,
                                   uint8_t id,
                                   const uint8_t *payload,
                                   uint16_t payload_len);
static void ui_screen_gps_cfg_builder_reset(ui_screen_gps_cfg_builder_t *builder);
static void ui_screen_gps_cfg_builder_push_u1(ui_screen_gps_cfg_builder_t *builder,
                                              uint32_t key,
                                              uint8_t value);
static void ui_screen_gps_cfg_builder_push_u2(ui_screen_gps_cfg_builder_t *builder,
                                              uint32_t key,
                                              uint16_t value);
static void ui_screen_gps_send_valset_ram(const ui_screen_gps_cfg_builder_t *builder);
static void ui_screen_gps_send_requested_profile(app_gps_boot_profile_t boot_profile,
                                                 app_gps_power_profile_t power_profile);
static void ui_screen_gps_send_cold_start(void);
static app_gps_boot_profile_t ui_screen_gps_get_toggle_target_mode(app_gps_boot_profile_t current);
static const char *ui_screen_gps_get_mode_text_short(app_gps_boot_profile_t mode);
static const char *ui_screen_gps_get_mode_text_long(app_gps_boot_profile_t mode);
static void ui_screen_gps_reset_fix_display_state(void);
static ui_screen_gps_fix_state_t ui_screen_gps_candidate_fix_state_from_fix(const gps_fix_basic_t *fix,
                                                                            uint32_t now_ms);
static ui_screen_gps_fix_state_t ui_screen_gps_get_display_fix_state(const gps_fix_basic_t *fix,
                                                                     uint32_t now_ms);
static const uint8_t *ui_screen_gps_get_fix_icon(uint8_t display_fix_state,
                                                 uint8_t *out_w,
                                                 uint8_t *out_h);
static const char *ui_screen_gps_get_fix_text(uint8_t display_fix_state);
static void ui_screen_gps_format_distance(uint32_t mm,
                                          uint8_t imperial_units,
                                          char *out_value,
                                          size_t out_value_size,
                                          char *out_unit,
                                          size_t out_unit_size,
                                          bool valid);
static void ui_screen_gps_format_coord(int32_t coord_e7,
                                       bool is_lat,
                                       char *out_hemi,
                                       size_t out_hemi_size,
                                       char *out_value,
                                       size_t out_value_size,
                                       bool valid);
static void ui_screen_gps_sort_visible_sat_indices(const app_gps_ui_snapshot_t *snapshot,
                                                   uint8_t *out_indices,
                                                   uint8_t *out_count);
static void ui_screen_gps_draw_accuracy_row(u8g2_t *u8g2,
                                            const ui_rect_t *pane,
                                            int16_t row_top_y,
                                            const uint8_t *icon,
                                            const char *label,
                                            uint32_t mm_value,
                                            uint8_t imperial_units,
                                            bool valid);
static void ui_screen_gps_draw_info_pane(u8g2_t *u8g2,
                                         const ui_rect_t *pane,
                                         const app_gps_ui_snapshot_t *snapshot,
                                         app_gps_boot_profile_t current_mode,
                                         uint8_t imperial_units);
static void ui_screen_gps_draw_sky_plot(u8g2_t *u8g2,
                                        const ui_rect_t *plot_rect,
                                        const app_gps_ui_snapshot_t *snapshot);
static uint16_t ui_screen_gps_sin_quadrant_q10(uint8_t deg_0_to_90);
static int16_t ui_screen_gps_sin_deg_q10(int16_t deg);
static void ui_screen_gps_draw_cn0_bars(u8g2_t *u8g2,
                                        const ui_rect_t *plot_rect,
                                        const app_gps_ui_snapshot_t *snapshot);
static void ui_screen_gps_compute_panes(const ui_rect_t *viewport,
                                        ui_rect_t *out_info_pane,
                                        ui_rect_t *out_plot_pane);

/* -------------------------------------------------------------------------- */
/*  GPS bottom bar                                                            */
/*                                                                            */
/*  요구사항을 그대로 반영한다.                                                */
/*  - F1 : RESET 라벨, 실제 동작은 "이전 메뉴로 복귀"                        */
/*  - F2 : GNSS                                                               */
/*  - F3 : COLD                                                               */
/*  - F4/F5/F6 : 빈 라벨                                                      */
/* -------------------------------------------------------------------------- */
static void ui_screen_gps_configure_bottom_bar(void)
{
    UI_BottomBar_SetMode(UI_BOTTOMBAR_MODE_BUTTONS);

    UI_BottomBar_SetButton(UI_FKEY_1, "RESET", UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButton(UI_FKEY_2, "GNSS",  UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButton(UI_FKEY_3, "COLD",  UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButton(UI_FKEY_4, "",      UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButton(UI_FKEY_5, "",      UI_BOTTOMBAR_FLAG_DIVIDER);
    UI_BottomBar_SetButton(UI_FKEY_6, "",      0u);
}

/* -------------------------------------------------------------------------- */
/*  Public init                                                               */
/* -------------------------------------------------------------------------- */
void UI_ScreenGps_Init(void)
{
    ui_screen_gps_reset_fix_display_state();
    ui_screen_gps_configure_bottom_bar();
}

/* -------------------------------------------------------------------------- */
/*  Public on-enter                                                           */
/* -------------------------------------------------------------------------- */
void UI_ScreenGps_OnEnter(void)
{
    ui_screen_gps_reset_fix_display_state();
    ui_screen_gps_configure_bottom_bar();
}

/* -------------------------------------------------------------------------- */
/*  Public button handler                                                     */
/*                                                                            */
/*  중요                                                                      */
/*  - F2/F3에서 packet 송신과 사용자 알림을 이 파일 안에서 모두 끝낸다.        */
/*  - UI 엔진은 오직 화면 전환 action만 받아 처리한다.                         */
/* -------------------------------------------------------------------------- */
ui_screen_gps_action_t UI_ScreenGps_HandleButtonEvent(const button_event_t *event,
                                                      uint32_t now_ms)
{
    if (event == 0)
    {
        return UI_SCREEN_GPS_ACTION_NONE;
    }

    if (event->type != BUTTON_EVENT_SHORT_PRESS)
    {
        return UI_SCREEN_GPS_ACTION_NONE;
    }

    switch (event->id)
    {
        case BUTTON_ID_1:
            return UI_SCREEN_GPS_ACTION_BACK_TO_PREVIOUS;

        case BUTTON_ID_2:
        {
            app_gps_boot_profile_t current_mode;
            app_gps_boot_profile_t next_mode;
            app_gps_power_profile_t power_mode;

            current_mode = g_app_state.settings.gps.boot_profile;
            next_mode = ui_screen_gps_get_toggle_target_mode(current_mode);
            power_mode = g_app_state.settings.gps.power_profile;

            g_app_state.settings.gps.boot_profile = next_mode;
            ui_screen_gps_send_requested_profile(next_mode, power_mode);

            UI_Toast_Show(ui_screen_gps_get_mode_text_long(next_mode),
                          icon_ui_info_8x8,
                          ICON8_W,
                          ICON8_H,
                          now_ms,
                          1200u);
            break;
        }

        case BUTTON_ID_3:
            ui_screen_gps_send_cold_start();
            UI_Popup_Show("GPS COLD START",
                          "UBX-CFG-RST SENT",
                          "WAIT FOR REACQUIRE",
                          icon_ui_warn_8x8,
                          ICON8_W,
                          ICON8_H,
                          now_ms,
                          1800u);
            break;

        case BUTTON_ID_4:
        case BUTTON_ID_5:
        case BUTTON_ID_6:
        case BUTTON_ID_NONE:
        default:
            break;
    }

    return UI_SCREEN_GPS_ACTION_NONE;
}

/* -------------------------------------------------------------------------- */
/*  Compose getters                                                           */
/* -------------------------------------------------------------------------- */
ui_layout_mode_t UI_ScreenGps_GetLayoutMode(void)
{
    return UI_LAYOUT_MODE_TOP_BOTTOM_FIXED;
}

bool UI_ScreenGps_IsStatusBarVisible(void)
{
    return true;
}

bool UI_ScreenGps_IsBottomBarVisible(void)
{
    return true;
}

/* -------------------------------------------------------------------------- */
/*  Little-endian helpers                                                     */
/* -------------------------------------------------------------------------- */
static void ui_screen_gps_u16_to_le(uint8_t *dst, uint16_t value)
{
    if (dst == 0)
    {
        return;
    }

    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8) & 0xFFu);
}

static void ui_screen_gps_u32_to_le(uint8_t *dst, uint32_t value)
{
    if (dst == 0)
    {
        return;
    }

    dst[0] = (uint8_t)(value & 0xFFu);
    dst[1] = (uint8_t)((value >> 8) & 0xFFu);
    dst[2] = (uint8_t)((value >> 16) & 0xFFu);
    dst[3] = (uint8_t)((value >> 24) & 0xFFu);
}

/* -------------------------------------------------------------------------- */
/*  Send one UBX frame                                                        */
/*                                                                            */
/*  frame layout                                                              */
/*  - sync 2 byte                                                             */
/*  - class / id                                                              */
/*  - payload length little-endian                                            */
/*  - payload                                                                  */
/*  - Fletcher checksum 2 byte                                                */
/* -------------------------------------------------------------------------- */
static void ui_screen_gps_send_ubx(uint8_t cls,
                                   uint8_t id,
                                   const uint8_t *payload,
                                   uint16_t payload_len)
{
    uint8_t frame[8u + 128u];
    uint8_t ck_a;
    uint8_t ck_b;
    uint16_t i;
    uint16_t frame_len;

    if (payload_len > 128u)
    {
        return;
    }

    frame[0] = UI_GPS_UBX_SYNC_1;
    frame[1] = UI_GPS_UBX_SYNC_2;
    frame[2] = cls;
    frame[3] = id;
    frame[4] = (uint8_t)(payload_len & 0xFFu);
    frame[5] = (uint8_t)((payload_len >> 8) & 0xFFu);

    ck_a = 0u;
    ck_b = 0u;

    for (i = 2u; i < 6u; i++)
    {
        ck_a = (uint8_t)(ck_a + frame[i]);
        ck_b = (uint8_t)(ck_b + ck_a);
    }

    for (i = 0u; i < payload_len; i++)
    {
        uint8_t byte = (payload != 0) ? payload[i] : 0u;

        frame[6u + i] = byte;
        ck_a = (uint8_t)(ck_a + byte);
        ck_b = (uint8_t)(ck_b + ck_a);
    }

    frame[6u + payload_len] = ck_a;
    frame[7u + payload_len] = ck_b;
    frame_len = (uint16_t)(8u + payload_len);

    (void)HAL_UART_Transmit(&UBLOX_GPS_UART_HANDLE,
                            frame,
                            frame_len,
                            UI_SCREEN_GPS_UBX_TX_TIMEOUT_MS);
}

/* -------------------------------------------------------------------------- */
/*  Tiny CFG-VALSET builder                                                   */
/* -------------------------------------------------------------------------- */
static void ui_screen_gps_cfg_builder_reset(ui_screen_gps_cfg_builder_t *builder)
{
    if (builder == 0)
    {
        return;
    }

    memset(builder, 0, sizeof(*builder));
}

static void ui_screen_gps_cfg_builder_push_u1(ui_screen_gps_cfg_builder_t *builder,
                                              uint32_t key,
                                              uint8_t value)
{
    if ((builder == 0) || ((uint16_t)(builder->len + 5u) > (uint16_t)sizeof(builder->buf)))
    {
        return;
    }

    ui_screen_gps_u32_to_le(&builder->buf[builder->len], key);
    builder->len += 4u;
    builder->buf[builder->len++] = value;
}

static void ui_screen_gps_cfg_builder_push_u2(ui_screen_gps_cfg_builder_t *builder,
                                              uint32_t key,
                                              uint16_t value)
{
    if ((builder == 0) || ((uint16_t)(builder->len + 6u) > (uint16_t)sizeof(builder->buf)))
    {
        return;
    }

    ui_screen_gps_u32_to_le(&builder->buf[builder->len], key);
    builder->len += 4u;
    ui_screen_gps_u16_to_le(&builder->buf[builder->len], value);
    builder->len += 2u;
}

/* -------------------------------------------------------------------------- */
/*  Send CFG-VALSET to RAM layer only                                         */
/*                                                                            */
/*  사용 이유                                                                  */
/*  - 즉시 모드 반영                                                          */
/*  - 재부팅 시에는 APP_STATE.settings.gps에 저장된 사용자가                    */
/*    원하는 모드를 드라이버가 다시 적용                                      */
/* -------------------------------------------------------------------------- */
static void ui_screen_gps_send_valset_ram(const ui_screen_gps_cfg_builder_t *builder)
{
    uint8_t payload[4u + sizeof(builder->buf)];

    if ((builder == 0) || (builder->len == 0u))
    {
        return;
    }

    payload[0] = 0u;
    payload[1] = UI_GPS_UBX_VALSET_LAYER_RAM;
    payload[2] = 0u;
    payload[3] = 0u;

    memcpy(&payload[4], builder->buf, builder->len);
    ui_screen_gps_send_ubx(UI_GPS_UBX_CLASS_CFG,
                           UI_GPS_UBX_ID_CFG_VALSET,
                           payload,
                           (uint16_t)(4u + builder->len));
}

/* -------------------------------------------------------------------------- */
/*  Toggle target mode helper                                                 */
/*                                                                            */
/*  요구사항은 두 모드 토글이다.                                               */
/*  - GPS ONLY 20Hz                                                            */
/*  - FULL CONSTELLATION 10Hz                                                  */
/*                                                                            */
/*  따라서 현재 값이                                                          */
/*  - GPS ONLY 20Hz 면 -> FULL CONST 10Hz                                      */
/*  - 그 외(GPS ONLY 10Hz 포함)는 -> GPS ONLY 20Hz                             */
/*  로 정리한다.                                                              */
/* -------------------------------------------------------------------------- */
static app_gps_boot_profile_t ui_screen_gps_get_toggle_target_mode(app_gps_boot_profile_t current)
{
    if (current == APP_GPS_BOOT_PROFILE_GPS_ONLY_20HZ)
    {
        return APP_GPS_BOOT_PROFILE_MULTI_CONST_10HZ;
    }

    return APP_GPS_BOOT_PROFILE_GPS_ONLY_20HZ;
}

/* -------------------------------------------------------------------------- */
/*  Send requested runtime profile                                            */
/*                                                                            */
/*  이 helper는 F2 버튼 한 번에 필요한 최소 key/value만 보낸다.                */
/*  - NAV-PVT / NAV-SAT output 유지                                            */
/*  - meas rate 변경                                                           */
/*  - power profile 유지                                                       */
/*  - constellation enable set 변경                                            */
/* -------------------------------------------------------------------------- */
static void ui_screen_gps_send_requested_profile(app_gps_boot_profile_t boot_profile,
                                                 app_gps_power_profile_t power_profile)
{
    ui_screen_gps_cfg_builder_t b;
    uint16_t meas_rate_ms;
    uint8_t pm_operate_mode;
    uint8_t gps_only;

    meas_rate_ms = 100u;
    gps_only = 1u;

    switch (boot_profile)
    {
        case APP_GPS_BOOT_PROFILE_GPS_ONLY_20HZ:
            meas_rate_ms = 50u;
            gps_only = 1u;
            break;

        case APP_GPS_BOOT_PROFILE_GPS_ONLY_10HZ:
            meas_rate_ms = 100u;
            gps_only = 1u;
            break;

        case APP_GPS_BOOT_PROFILE_MULTI_CONST_10HZ:
        default:
            meas_rate_ms = 100u;
            gps_only = 0u;
            break;
    }

    pm_operate_mode = (power_profile == APP_GPS_POWER_PROFILE_POWER_SAVE) ?
                      UI_GPS_PM_OPERATEMODE_PSMCT :
                      UI_GPS_PM_OPERATEMODE_FULL;

    ui_screen_gps_cfg_builder_reset(&b);

    /* ---------------------------------------------------------------------- */
    /*  주기 출력 메시지는 항상 유지한다.                                       */
    /*  UI 화면이 NAV-PVT / NAV-SAT을 계속 받도록 보존하는 부분이다.           */
    /* ---------------------------------------------------------------------- */
    ui_screen_gps_cfg_builder_push_u1(&b, UI_GPS_CFG_KEY_MSGOUT_UBX_NAV_PVT_UART1, 1u);
    ui_screen_gps_cfg_builder_push_u1(&b, UI_GPS_CFG_KEY_MSGOUT_UBX_NAV_SAT_UART1, 1u);

    /* navigation measurement rate */
    ui_screen_gps_cfg_builder_push_u2(&b, UI_GPS_CFG_KEY_RATE_MEAS, meas_rate_ms);

    /* power mode는 현재 settings 값을 그대로 유지 */
    ui_screen_gps_cfg_builder_push_u1(&b, UI_GPS_CFG_KEY_PM_OPERATEMODE, pm_operate_mode);

    /* GPS core는 두 모드 모두 활성 유지 */
    ui_screen_gps_cfg_builder_push_u1(&b, UI_GPS_CFG_KEY_SIGNAL_GPS_ENA, 1u);
    ui_screen_gps_cfg_builder_push_u1(&b, UI_GPS_CFG_KEY_SIGNAL_GPS_L1CA_ENA, 1u);

    /* ---------------------------------------------------------------------- */
    /*  GPS only 모드에서는 나머지 constellation을 전부 끈다.                  */
    /*  Full constellation 모드에서는 SBAS / GAL / BDS / QZSS / GLO를 켠다.   */
    /* ---------------------------------------------------------------------- */
    ui_screen_gps_cfg_builder_push_u1(&b, UI_GPS_CFG_KEY_SIGNAL_SBAS_ENA,      (uint8_t)(gps_only ? 0u : 1u));
    ui_screen_gps_cfg_builder_push_u1(&b, UI_GPS_CFG_KEY_SIGNAL_SBAS_L1CA_ENA, (uint8_t)(gps_only ? 0u : 1u));

    ui_screen_gps_cfg_builder_push_u1(&b, UI_GPS_CFG_KEY_SIGNAL_GAL_ENA,       (uint8_t)(gps_only ? 0u : 1u));
    ui_screen_gps_cfg_builder_push_u1(&b, UI_GPS_CFG_KEY_SIGNAL_GAL_E1_ENA,    (uint8_t)(gps_only ? 0u : 1u));

    ui_screen_gps_cfg_builder_push_u1(&b, UI_GPS_CFG_KEY_SIGNAL_BDS_ENA,       (uint8_t)(gps_only ? 0u : 1u));

    /* ---------------------------------------------------------------------- */
    /*  BeiDou signal 선택                                                     */
    /*                                                                        */
    /*  u-blox M10 계열은 B1I와 B1C를 동시에 사용할 수 없다.                  */
    /*  특히 GLONASS를 함께 쓰는 일반 multi-constellation 조합에서는          */
    /*  B1C 쪽으로 맞추고 B1I는 끄는 편이 안전하다.                            */
    /*                                                                        */
    /*  기존 코드처럼 둘 다 1로 보내면                                         */
    /*  - UBX-CFG-VALSET가 NAK 되거나                                          */
    /*  - 일부 항목만 적용되고 일부는 무시되면서                               */
    /*    실제 사용 constellation 구성이 의도와 달라질 수 있다.               */
    /*                                                                        */
    /*  따라서 GPS ONLY에서는 둘 다 끄고,                                      */
    /*  FULL CONST에서는 B1C만 켜고 B1I는 명시적으로 끈다.                     */
    /* ---------------------------------------------------------------------- */
    ui_screen_gps_cfg_builder_push_u1(&b, UI_GPS_CFG_KEY_SIGNAL_BDS_B1I_ENA,   0u);
    ui_screen_gps_cfg_builder_push_u1(&b, UI_GPS_CFG_KEY_SIGNAL_BDS_B1C_ENA,   (uint8_t)(gps_only ? 0u : 1u));

    ui_screen_gps_cfg_builder_push_u1(&b, UI_GPS_CFG_KEY_SIGNAL_QZSS_ENA,      (uint8_t)(gps_only ? 0u : 1u));
    ui_screen_gps_cfg_builder_push_u1(&b, UI_GPS_CFG_KEY_SIGNAL_QZSS_L1CA_ENA, (uint8_t)(gps_only ? 0u : 1u));
    ui_screen_gps_cfg_builder_push_u1(&b, UI_GPS_CFG_KEY_SIGNAL_QZSS_L1S_ENA,  (uint8_t)(gps_only ? 0u : 1u));

    ui_screen_gps_cfg_builder_push_u1(&b, UI_GPS_CFG_KEY_SIGNAL_GLO_ENA,       (uint8_t)(gps_only ? 0u : 1u));
    ui_screen_gps_cfg_builder_push_u1(&b, UI_GPS_CFG_KEY_SIGNAL_GLO_L1_ENA,    (uint8_t)(gps_only ? 0u : 1u));

    ui_screen_gps_send_valset_ram(&b);
}

/* -------------------------------------------------------------------------- */
/*  Send cold-start reset                                                     */
/*                                                                            */
/*  payload                                                                    */
/*  - navBbrMask = 0xFFFF : cold start용 BBR clear set                         */
/*  - resetMode  = 0x02   : controlled software reset (GNSS only)             */
/*                                                                            */
/*  이 조합을 쓰는 이유                                                        */
/*  - cold start 의미는 살리되                                                 */
/*  - 전체 MCU/호스트 인터페이스까지 크게 뒤흔드는 full watchdog reset보다     */
/*    GNSS processing만 제어하는 쪽이 UI 관점에서 다루기 쉽기 때문이다.        */
/* -------------------------------------------------------------------------- */
static void ui_screen_gps_send_cold_start(void)
{
    uint8_t payload[4];

    payload[0] = 0xFFu;
    payload[1] = 0xFFu;
    payload[2] = 0x02u;
    payload[3] = 0x00u;

    ui_screen_gps_send_ubx(UI_GPS_UBX_CLASS_CFG,
                           UI_GPS_UBX_ID_CFG_RST,
                           payload,
                           4u);
}

/* -------------------------------------------------------------------------- */
/*  Mode text helpers                                                         */
/* -------------------------------------------------------------------------- */
static const char *ui_screen_gps_get_mode_text_short(app_gps_boot_profile_t mode)
{
    switch (mode)
    {
        case APP_GPS_BOOT_PROFILE_GPS_ONLY_20HZ:
            return "20HZ GPS";

        case APP_GPS_BOOT_PROFILE_GPS_ONLY_10HZ:
            return "10HZ GPS";

        case APP_GPS_BOOT_PROFILE_MULTI_CONST_10HZ:
        default:
            return "10HZ FULL";
    }
}

static const char *ui_screen_gps_get_mode_text_long(app_gps_boot_profile_t mode)
{
    switch (mode)
    {
        case APP_GPS_BOOT_PROFILE_GPS_ONLY_20HZ:
            return "GPS ONLY 20HZ";

        case APP_GPS_BOOT_PROFILE_GPS_ONLY_10HZ:
            return "GPS ONLY 10HZ";

        case APP_GPS_BOOT_PROFILE_MULTI_CONST_10HZ:
        default:
            return "FULL CONST 10HZ";
    }
}

/* -------------------------------------------------------------------------- */
/*  Fix display state reset                                                   */
/*                                                                            */
/*  화면 진입 시 이전 화면에서 남아 있던 latched FIX 표시가                    */
/*  새 세션에 섞여 보이지 않도록 상태를 초기화한다.                           */
/* -------------------------------------------------------------------------- */
static void ui_screen_gps_reset_fix_display_state(void)
{
    memset(&s_ui_screen_gps_fix_display, 0, sizeof(s_ui_screen_gps_fix_display));
}

/* -------------------------------------------------------------------------- */
/*  Raw fix -> candidate display state                                        */
/*                                                                            */
/*  여기서는 raw fix를 그대로 믿지 않고                                       */
/*  - 메시지가 최근에 갱신되었는지                                             */
/*  - fixType이 2D/3D인지                                                      */
/*  - fixOk가 순간적으로 0이어도 정확도/위성수 같은 보조 증거가 있는지         */
/*  를 함께 본다.                                                              */
/* -------------------------------------------------------------------------- */
static ui_screen_gps_fix_state_t ui_screen_gps_candidate_fix_state_from_fix(const gps_fix_basic_t *fix,
                                                                            uint32_t now_ms)
{
    bool fresh;
    bool has_quality_evidence;

    if (fix == 0)
    {
        return UI_SCREEN_GPS_FIX_STATE_NOFIX;
    }

    /* ---------------------------------------------------------------------- */
    /*  stale sample 방어                                                      */
    /*                                                                        */
    /*  last_update_ms가 오래된 샘플은                                         */
    /*  현재 화면에 "아직 fix가 살아 있다" 고 표시하면 안 되므로               */
    /*  candidate를 즉시 NO FIX로 본다.                                        */
    /* ---------------------------------------------------------------------- */
    fresh = ((fix->last_update_ms != 0u) &&
             ((now_ms - fix->last_update_ms) <= UI_SCREEN_GPS_FIX_HOLD_3D_MS));

    if (fresh == false)
    {
        return UI_SCREEN_GPS_FIX_STATE_NOFIX;
    }

    has_quality_evidence =
        (fix->pDOP != 0u) ||
        (fix->hAcc != 0u) ||
        (fix->vAcc != 0u) ||
        (fix->numSV_used != 0u) ||
        (fix->numSV_visible != 0u);

    if ((fix->fixType >= 3u) && ((fix->fixOk != false) || (has_quality_evidence != false)))
    {
        return UI_SCREEN_GPS_FIX_STATE_3D;
    }

    if ((fix->fixType == 2u) && ((fix->fixOk != false) || (has_quality_evidence != false)))
    {
        return UI_SCREEN_GPS_FIX_STATE_2D;
    }

    return UI_SCREEN_GPS_FIX_STATE_NOFIX;
}

/* -------------------------------------------------------------------------- */
/*  Display hysteresis update                                                 */
/*                                                                            */
/*  raw fix를 받아 GPS 화면용 latched FIX 상태를 갱신한다.                     */
/*                                                                            */
/*  규칙                                                                      */
/*  - NO FIX -> 2D/3D : 즉시 승급                                             */
/*  - 2D -> 3D       : 즉시 승급                                               */
/*  - 3D -> 2D/NOFIX : 3D hold 시간 이후에만 강등                              */
/*  - 2D -> NOFIX    : 2D hold 시간 이후에만 강등                              */
/* -------------------------------------------------------------------------- */
static ui_screen_gps_fix_state_t ui_screen_gps_get_display_fix_state(const gps_fix_basic_t *fix,
                                                                     uint32_t now_ms)
{
    ui_screen_gps_fix_state_t candidate_fix_state;

    candidate_fix_state = ui_screen_gps_candidate_fix_state_from_fix(fix, now_ms);

    if (s_ui_screen_gps_fix_display.initialized == false)
    {
        s_ui_screen_gps_fix_display.initialized = true;
        s_ui_screen_gps_fix_display.display_fix_state = (uint8_t)candidate_fix_state;
        s_ui_screen_gps_fix_display.last_seen_any_fix_ms = now_ms;
        s_ui_screen_gps_fix_display.last_seen_3d_fix_ms = now_ms;
    }

    if (candidate_fix_state >= UI_SCREEN_GPS_FIX_STATE_2D)
    {
        s_ui_screen_gps_fix_display.last_seen_any_fix_ms = now_ms;
    }

    if (candidate_fix_state >= UI_SCREEN_GPS_FIX_STATE_3D)
    {
        s_ui_screen_gps_fix_display.last_seen_3d_fix_ms = now_ms;
    }

    if (candidate_fix_state >= (ui_screen_gps_fix_state_t)s_ui_screen_gps_fix_display.display_fix_state)
    {
        s_ui_screen_gps_fix_display.display_fix_state = (uint8_t)candidate_fix_state;
    }
    else
    {
        switch ((ui_screen_gps_fix_state_t)s_ui_screen_gps_fix_display.display_fix_state)
        {
            case UI_SCREEN_GPS_FIX_STATE_3D:
                if ((now_ms - s_ui_screen_gps_fix_display.last_seen_3d_fix_ms) < UI_SCREEN_GPS_FIX_HOLD_3D_MS)
                {
                    s_ui_screen_gps_fix_display.display_fix_state = (uint8_t)UI_SCREEN_GPS_FIX_STATE_3D;
                }
                else if ((candidate_fix_state >= UI_SCREEN_GPS_FIX_STATE_2D) ||
                         ((now_ms - s_ui_screen_gps_fix_display.last_seen_any_fix_ms) < UI_SCREEN_GPS_FIX_HOLD_2D_MS))
                {
                    s_ui_screen_gps_fix_display.display_fix_state = (uint8_t)UI_SCREEN_GPS_FIX_STATE_2D;
                }
                else
                {
                    s_ui_screen_gps_fix_display.display_fix_state = (uint8_t)UI_SCREEN_GPS_FIX_STATE_NOFIX;
                }
                break;

            case UI_SCREEN_GPS_FIX_STATE_2D:
                if ((now_ms - s_ui_screen_gps_fix_display.last_seen_any_fix_ms) < UI_SCREEN_GPS_FIX_HOLD_2D_MS)
                {
                    s_ui_screen_gps_fix_display.display_fix_state = (uint8_t)UI_SCREEN_GPS_FIX_STATE_2D;
                }
                else
                {
                    s_ui_screen_gps_fix_display.display_fix_state = (uint8_t)candidate_fix_state;
                }
                break;

            case UI_SCREEN_GPS_FIX_STATE_NOFIX:
            default:
                s_ui_screen_gps_fix_display.display_fix_state = (uint8_t)candidate_fix_state;
                break;
        }
    }

    return (ui_screen_gps_fix_state_t)s_ui_screen_gps_fix_display.display_fix_state;
}

/* -------------------------------------------------------------------------- */
/*  Fix icon / text helpers                                                   */
/* -------------------------------------------------------------------------- */
static const uint8_t *ui_screen_gps_get_fix_icon(uint8_t display_fix_state,
                                                 uint8_t *out_w,
                                                 uint8_t *out_h)
{
    if (out_w != 0)
    {
        *out_w = ICON11_W;
    }

    if (out_h != 0)
    {
        *out_h = ICON11_H;
    }

    if (display_fix_state >= UI_SCREEN_GPS_FIX_STATE_3D)
    {
        return icon_gps_3d_bits;
    }

    if (display_fix_state == UI_SCREEN_GPS_FIX_STATE_2D)
    {
        return icon_gps_2d_bits;
    }

    return icon_gps_nofix_bits;
}

static const char *ui_screen_gps_get_fix_text(uint8_t display_fix_state)
{
    if (display_fix_state >= UI_SCREEN_GPS_FIX_STATE_3D)
    {
        return "3D FIX";
    }

    if (display_fix_state == UI_SCREEN_GPS_FIX_STATE_2D)
    {
        return "2D FIX";
    }

    return "NO FIX";
}

/* -------------------------------------------------------------------------- */
/*  Format distance value                                                     */
/*                                                                            */
/*  printf float 의존성을 피하기 위해                                         */
/*  정수 연산만으로 "한 자리 소수"까지 표현한다.                              */
/* -------------------------------------------------------------------------- */
static void ui_screen_gps_format_distance(uint32_t mm,
                                          uint8_t imperial_units,
                                          char *out_value,
                                          size_t out_value_size,
                                          char *out_unit,
                                          size_t out_unit_size,
                                          bool valid)
{
    if ((out_value == 0) || (out_unit == 0) || (out_value_size == 0u) || (out_unit_size == 0u))
    {
        return;
    }

    if (valid == false)
    {
        snprintf(out_value, out_value_size, "--.-");
        snprintf(out_unit, out_unit_size, "%s", (imperial_units != 0u) ? "ft" : "m");
        return;
    }

    if (imperial_units != 0u)
    {
        uint32_t tenths_ft;

        /* 1 ft = 304.8 mm, tenths_ft = mm * 100 / 3048 */
        tenths_ft = (uint32_t)(((uint64_t)mm * 100u + 1524u) / 3048u);

        if (tenths_ft >= 1000u)
        {
            snprintf(out_value, out_value_size, "%lu", (unsigned long)((tenths_ft + 5u) / 10u));
        }
        else
        {
            snprintf(out_value, out_value_size,
                     "%lu.%01lu",
                     (unsigned long)(tenths_ft / 10u),
                     (unsigned long)(tenths_ft % 10u));
        }

        snprintf(out_unit, out_unit_size, "ft");
    }
    else
    {
        uint32_t tenths_m;

        /* 0.1 m 단위 = 100 mm */
        tenths_m = (mm + 50u) / 100u;

        if (tenths_m >= 1000u)
        {
            snprintf(out_value, out_value_size, "%lu", (unsigned long)((tenths_m + 5u) / 10u));
        }
        else
        {
            snprintf(out_value, out_value_size,
                     "%lu.%01lu",
                     (unsigned long)(tenths_m / 10u),
                     (unsigned long)(tenths_m % 10u));
        }

        snprintf(out_unit, out_unit_size, "m");
    }
}

/* -------------------------------------------------------------------------- */
/*  Format latitude / longitude                                               */
/*                                                                            */
/*  입력 단위                                                                  */
/*  - coord_e7 : degree * 1e-7                                                 */
/*                                                                            */
/*  출력 형식                                                                  */
/*  - hemisphere : N/S 또는 E/W                                                */
/*  - value      : dd.ddddd 또는 ddd.ddddd                                     */
/*                                                                            */
/*  숫자 폭이 112px info pane에 무리 없이 들어가도록                           */
/*  소수 다섯 자리까지만 표시한다.                                             */
/* -------------------------------------------------------------------------- */
static void ui_screen_gps_format_coord(int32_t coord_e7,
                                       bool is_lat,
                                       char *out_hemi,
                                       size_t out_hemi_size,
                                       char *out_value,
                                       size_t out_value_size,
                                       bool valid)
{
    uint32_t abs_value;
    uint32_t whole_deg;
    uint32_t frac5;

    if ((out_hemi == 0) || (out_value == 0) || (out_hemi_size == 0u) || (out_value_size == 0u))
    {
        return;
    }

    if (valid == false)
    {
        snprintf(out_hemi, out_hemi_size, "%c", is_lat ? 'N' : 'E');
        snprintf(out_value, out_value_size, "--.-----");
        return;
    }

    abs_value = (coord_e7 < 0) ? (uint32_t)(-coord_e7) : (uint32_t)coord_e7;
    whole_deg = abs_value / 10000000u;
    frac5 = (abs_value % 10000000u) / 100u;

    snprintf(out_hemi,
             out_hemi_size,
             "%c",
             is_lat ? ((coord_e7 < 0) ? 'S' : 'N') : ((coord_e7 < 0) ? 'W' : 'E'));

    snprintf(out_value,
             out_value_size,
             "%lu.%05lu",
             (unsigned long)whole_deg,
             (unsigned long)frac5);
}

/* -------------------------------------------------------------------------- */
/*  Sort visible satellites by SV ID                                          */
/*                                                                            */
/*  bar graph의 X축 의미를 "채널 순서" 가 아니라                              */
/*  사용자가 이해하는 위성 번호(sv_id) 기준으로 맞추기 위해                    */
/*  아주 작은 insertion sort를 쓴다.                                           */
/*                                                                            */
/*  APP_GPS_MAX_SATS 가 32로 작으므로                                           */
/*  O(n^2) 정렬이어도 화면 프레임 비용에 충분히 안전하다.                      */
/* -------------------------------------------------------------------------- */
static void ui_screen_gps_sort_visible_sat_indices(const app_gps_ui_snapshot_t *snapshot,
                                                   uint8_t *out_indices,
                                                   uint8_t *out_count)
{
    uint8_t i;
    uint8_t count;

    if ((snapshot == 0) || (out_indices == 0) || (out_count == 0))
    {
        return;
    }

    count = 0u;

    for (i = 0u; i < snapshot->nav_sat_count; i++)
    {
        if (snapshot->sats[i].visible == 0u)
        {
            continue;
        }

        out_indices[count++] = i;
    }

    for (i = 1u; i < count; i++)
    {
        uint8_t key_index;
        uint8_t j;

        key_index = out_indices[i];
        j = i;

        while ((j > 0u) &&
               (snapshot->sats[out_indices[j - 1u]].sv_id > snapshot->sats[key_index].sv_id))
        {
            out_indices[j] = out_indices[j - 1u];
            j--;
        }

        out_indices[j] = key_index;
    }

    *out_count = count;
}

/* -------------------------------------------------------------------------- */
/*  Draw one accuracy row                                                     */
/*                                                                            */
/*  한 줄 구성                                                                  */
/*  - 좌측 13x13 아이콘                                                        */
/*  - 우측 위  : 작은 label                                                    */
/*  - 우측 아래: 큰 숫자 + 작은 단위                                           */
/* -------------------------------------------------------------------------- */
static void ui_screen_gps_draw_accuracy_row(u8g2_t *u8g2,
                                            const ui_rect_t *pane,
                                            int16_t row_top_y,
                                            const uint8_t *icon,
                                            const char *label,
                                            uint32_t mm_value,
                                            uint8_t imperial_units,
                                            bool valid)
{
    char value_text[16];
    char unit_text[4];
    int16_t icon_x;
    int16_t icon_y;
    int16_t text_x;
    int16_t label_y;
    int16_t value_baseline_y;
    int16_t unit_x;

    if ((u8g2 == 0) || (pane == 0) || (icon == 0) || (label == 0))
    {
        return;
    }

    ui_screen_gps_format_distance(mm_value,
                                  imperial_units,
                                  value_text,
                                  sizeof(value_text),
                                  unit_text,
                                  sizeof(unit_text),
                                  valid);

    icon_x = pane->x;
    icon_y = (int16_t)(row_top_y + 2);
    text_x = (int16_t)(pane->x + ICON13_W + 4);
    label_y = (int16_t)(row_top_y + 7);
    value_baseline_y = (int16_t)(row_top_y + 18);

    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawXBM(u8g2,
                 (u8g2_uint_t)icon_x,
                 (u8g2_uint_t)icon_y,
                 ICON13_W,
                 ICON13_H,
                 icon);

    u8g2_SetFont(u8g2, UI_SCREEN_GPS_FONT_SMALL);
    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)text_x,
                 (u8g2_uint_t)label_y,
                 label);

    u8g2_SetFont(u8g2, UI_SCREEN_GPS_FONT_LARGE);
    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)text_x,
                 (u8g2_uint_t)value_baseline_y,
                 value_text);

    unit_x = (int16_t)(text_x + u8g2_GetStrWidth(u8g2, value_text) + 2);

    u8g2_SetFont(u8g2, UI_SCREEN_GPS_FONT_SMALL);
    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)unit_x,
                 (u8g2_uint_t)value_baseline_y,
                 unit_text);
}

/* -------------------------------------------------------------------------- */
/*  Compute left info pane and right plot pane                                */
/*                                                                            */
/*  pane 배치                                                                  */
/*  - left  : 문자/숫자 정보                                                    */
/*  - right : sky plot + CN0 bars                                              */
/*                                                                            */
/*  plot pane는 사용자가 요구한 128px을 우선 보장한다.                         */
/*  단, viewport가 더 좁은 미래 레이아웃을 대비해                              */
/*  최소 info width를 침범하면 자동으로 절반 분할로 fallback 한다.             */
/* -------------------------------------------------------------------------- */
static void ui_screen_gps_compute_panes(const ui_rect_t *viewport,
                                        ui_rect_t *out_info_pane,
                                        ui_rect_t *out_plot_pane)
{
    ui_rect_t info;
    ui_rect_t plot;
    int16_t plot_w;

    if ((viewport == 0) || (out_info_pane == 0) || (out_plot_pane == 0))
    {
        return;
    }

    plot_w = (viewport->w >= UI_SCREEN_GPS_PLOT_PANE_W) ? UI_SCREEN_GPS_PLOT_PANE_W : (int16_t)(viewport->w / 2);

    if ((viewport->w - plot_w) < UI_SCREEN_GPS_INFO_MIN_W)
    {
        plot_w = (int16_t)(viewport->w / 2);
    }

    info.x = viewport->x;
    info.y = viewport->y;
    info.w = (int16_t)(viewport->w - plot_w);
    info.h = viewport->h;

    plot.x = (int16_t)(info.x + info.w);
    plot.y = viewport->y;
    plot.w = plot_w;
    plot.h = viewport->h;

    *out_info_pane = info;
    *out_plot_pane = plot;
}

/* -------------------------------------------------------------------------- */
/*  Draw left info pane                                                       */
/*                                                                            */
/*  좌측 pane 내용                                                             */
/*  1) 상단 제목 + 현재 GPS mode                                               */
/*  2) POS ACC / ALT ACC                                                       */
/*  3) FIX 상태 + 사용/가시 위성 수                                             */
/*  4) LAT / LON                                                               */
/*                                                                            */
/*  숫자 겹침 방지를 위해 모든 baseline을 고정 좌표로 설계했다.                */
/* -------------------------------------------------------------------------- */
static void ui_screen_gps_draw_info_pane(u8g2_t *u8g2,
                                         const ui_rect_t *pane,
                                         const app_gps_ui_snapshot_t *snapshot,
                                         app_gps_boot_profile_t current_mode,
                                         uint8_t imperial_units)
{
    char sv_text[16];
    char hemi_text[2];
    char coord_text[20];
    uint8_t fix_icon_w;
    uint8_t fix_icon_h;
    uint8_t display_fix_state;
    const uint8_t *fix_icon;
    int16_t mode_x;

    if ((u8g2 == 0) || (pane == 0) || (snapshot == 0))
    {
        return;
    }

    u8g2_SetDrawColor(u8g2, 1);

    /* ---------------------------------------------------------------------- */
    /*  1) Header line                                                         */
    /*                                                                        */
    /*  좌측 상단에는 제목 "GPS STATUS" 를 두고                               */
    /*  같은 줄 우측에는 현재 요청 mode의 짧은 문자열을 우측 정렬한다.         */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFont(u8g2, UI_SCREEN_GPS_FONT_SMALL);
    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)pane->x,
                 (u8g2_uint_t)(pane->y + 7),
                 "GPS STATUS");

    mode_x = (int16_t)(pane->x + pane->w - u8g2_GetStrWidth(u8g2, ui_screen_gps_get_mode_text_short(current_mode)));
    if (mode_x < pane->x)
    {
        mode_x = pane->x;
    }

    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)mode_x,
                 (u8g2_uint_t)(pane->y + 7),
                 ui_screen_gps_get_mode_text_short(current_mode));

    u8g2_DrawHLine(u8g2,
                   (u8g2_uint_t)pane->x,
                   (u8g2_uint_t)(pane->y + 9),
                   (u8g2_uint_t)pane->w);

    /* ---------------------------------------------------------------------- */
    /*  2) Accuracy rows                                                       */
    /*                                                                        */
    /*  - 첫 줄  : 위치 수평 정확도(hAcc)                                      */
    /*  - 둘째 줄: 높이 수직 정확도(vAcc)                                      */
    /*                                                                        */
    /*  사용자가 준 13x13 아이콘을 정확히 이 두 줄의 맨 왼쪽에 배치한다.       */
    /* ---------------------------------------------------------------------- */
    ui_screen_gps_draw_accuracy_row(u8g2,
                                    pane,
                                    (int16_t)(pane->y + 12),
                                    icon_gps_position_accuracy_13x13,
                                    "POS ACC",
                                    snapshot->fix.hAcc,
                                    imperial_units,
                                    snapshot->fix.valid);

    ui_screen_gps_draw_accuracy_row(u8g2,
                                    pane,
                                    (int16_t)(pane->y + 32),
                                    icon_gps_altitude_accuracy_13x13,
                                    "ALT ACC",
                                    snapshot->fix.vAcc,
                                    imperial_units,
                                    snapshot->fix.valid);

    /* ---------------------------------------------------------------------- */
    /*  3) Fix 상태 + 위성 수                                                  */
    /*                                                                        */
    /*  11x7 GPS fix 아이콘을 좌측에 두고                                      */
    /*  - 오른쪽 첫 줄 : NO FIX / 2D FIX / 3D FIX                              */
    /*  - 오른쪽 둘째 줄: SV used/visible                                      */
    /*  로 간결하게 표시한다.                                                  */
    /* ---------------------------------------------------------------------- */
    /* ---------------------------------------------------------------------- */
    /*  FIX 아이콘/텍스트는 raw fixOk를 그대로 쓰지 않고                        */
    /*  화면 전용 hysteresis를 통과한 display_fix_state를 사용한다.             */
    /*                                                                        */
    /*  이 줄이 실제로                                                         */
    /*  - 좌측 정보 pane의 가운데 영역                                         */
    /*  - y = pane->y + 53 부근에 그려지는 11x7 GPS 상태 아이콘                */
    /*  - 그 오른쪽의 "NO FIX / 2D FIX / 3D FIX" 텍스트                        */
    /*  의 기준 상태를 결정한다.                                               */
    /* ---------------------------------------------------------------------- */
    display_fix_state = (uint8_t)ui_screen_gps_get_display_fix_state(&snapshot->fix, HAL_GetTick());

    fix_icon = ui_screen_gps_get_fix_icon(display_fix_state,
                                          &fix_icon_w,
                                          &fix_icon_h);

    u8g2_DrawXBM(u8g2,
                 (u8g2_uint_t)pane->x,
                 (u8g2_uint_t)(pane->y + 53),
                 fix_icon_w,
                 fix_icon_h,
                 fix_icon);

    u8g2_SetFont(u8g2, UI_SCREEN_GPS_FONT_SMALL);
    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)(pane->x + 16),
                 (u8g2_uint_t)(pane->y + 59),
                 ui_screen_gps_get_fix_text(display_fix_state));

    snprintf(sv_text,
             sizeof(sv_text),
             "SV %02u/%02u",
             (unsigned int)snapshot->fix.numSV_used,
             (unsigned int)snapshot->fix.numSV_visible);

    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)(pane->x + 16),
                 (u8g2_uint_t)(pane->y + 67),
                 sv_text);

    /* mode full string은 fix 영역 오른쪽 아래 보조 정보로 한번 더 노출 */
    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)(pane->x + 62),
                 (u8g2_uint_t)(pane->y + 67),
                 ui_screen_gps_get_mode_text_short(current_mode));

    /* ---------------------------------------------------------------------- */
    /*  4) LAT block                                                           */
    /*                                                                        */
    /*  요구사항 반영                                                          */
    /*  - 좌측 정렬 작은 라벨 LAT                                              */
    /*  - hemisphere는 작게                                                    */
    /*  - 숫자는 크게                                                          */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFont(u8g2, UI_SCREEN_GPS_FONT_SMALL);
    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)pane->x,
                 (u8g2_uint_t)(pane->y + 77),
                 "LAT");

    ui_screen_gps_format_coord(snapshot->fix.lat,
                               true,
                               hemi_text,
                               sizeof(hemi_text),
                               coord_text,
                               sizeof(coord_text),
                               snapshot->fix.valid);

    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)(pane->x + 20),
                 (u8g2_uint_t)(pane->y + 82),
                 hemi_text);

    u8g2_SetFont(u8g2, UI_SCREEN_GPS_FONT_MEDIUM);
    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)(pane->x + 28),
                 (u8g2_uint_t)(pane->y + 86),
                 coord_text);

    /* ---------------------------------------------------------------------- */
    /*  5) LON block                                                           */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFont(u8g2, UI_SCREEN_GPS_FONT_SMALL);
    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)pane->x,
                 (u8g2_uint_t)(pane->y + 100),
                 "LON");

    ui_screen_gps_format_coord(snapshot->fix.lon,
                               false,
                               hemi_text,
                               sizeof(hemi_text),
                               coord_text,
                               sizeof(coord_text),
                               snapshot->fix.valid);

    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)(pane->x + 20),
                 (u8g2_uint_t)(pane->y + 105),
                 hemi_text);

    u8g2_SetFont(u8g2, UI_SCREEN_GPS_FONT_MEDIUM);
    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)(pane->x + 28),
                 (u8g2_uint_t)(pane->y + 109),
                 coord_text);
}

/* -------------------------------------------------------------------------- */
/*  Fixed-point sine for 0..90 degree                                          */
/*                                                                            */
/*  table은 5도 간격이므로                                                     */
/*  중간 degree는 선형 보간으로 채운다.                                       */
/* -------------------------------------------------------------------------- */
static uint16_t ui_screen_gps_sin_quadrant_q10(uint8_t deg_0_to_90)
{
    uint8_t index;
    uint8_t remainder_deg;
    uint16_t v0;
    uint16_t v1;

    if (deg_0_to_90 >= 90u)
    {
        return 1024u;
    }

    index = (uint8_t)(deg_0_to_90 / 5u);
    remainder_deg = (uint8_t)(deg_0_to_90 % 5u);

    v0 = s_ui_screen_gps_sin_q10_0_to_90_by_5deg[index];
    v1 = s_ui_screen_gps_sin_q10_0_to_90_by_5deg[index + 1u];

    return (uint16_t)(v0 + (uint16_t)((((uint32_t)(v1 - v0)) * remainder_deg + 2u) / 5u));
}

/* -------------------------------------------------------------------------- */
/*  Fixed-point sine for arbitrary degree                                      */
/*                                                                            */
/*  반환값                                                                     */
/*  - Q10 format                                                               */
/*  - +1024 == +1.0                                                            */
/*  - -1024 == -1.0                                                            */
/* -------------------------------------------------------------------------- */
static int16_t ui_screen_gps_sin_deg_q10(int16_t deg)
{
    int16_t norm_deg;

    norm_deg = deg;

    while (norm_deg < 0)
    {
        norm_deg = (int16_t)(norm_deg + 360);
    }

    while (norm_deg >= 360)
    {
        norm_deg = (int16_t)(norm_deg - 360);
    }

    if (norm_deg <= 90)
    {
        return (int16_t)ui_screen_gps_sin_quadrant_q10((uint8_t)norm_deg);
    }

    if (norm_deg < 180)
    {
        return (int16_t)ui_screen_gps_sin_quadrant_q10((uint8_t)(180 - norm_deg));
    }

    if (norm_deg <= 270)
    {
        return (int16_t)(-((int16_t)ui_screen_gps_sin_quadrant_q10((uint8_t)(norm_deg - 180))));
    }

    return (int16_t)(-((int16_t)ui_screen_gps_sin_quadrant_q10((uint8_t)(360 - norm_deg))));
}

/* -------------------------------------------------------------------------- */
/*  Draw sky plot                                                             */
/*                                                                            */
/*  우측 pane의 상단 3/4 영역 한가운데에 정사각형을 만든 뒤                     */
/*  그 안에 원형 sky plot을 넣는다.                                            */
/*                                                                            */
/*  표현 규칙                                                                  */
/*  - 외곽 원 : horizon                                                        */
/*  - 안쪽 원 : elevation 45도                                                 */
/*  - 십자선  : N/E/S/W 방향 기준                                               */
/*  - 마커 fill  : used_in_solution == 1                                       */
/*  - 마커 hollow: used_in_solution == 0                                       */
/* -------------------------------------------------------------------------- */
static void ui_screen_gps_draw_sky_plot(u8g2_t *u8g2,
                                        const ui_rect_t *plot_rect,
                                        const app_gps_ui_snapshot_t *snapshot)
{
    int16_t square_side;
    int16_t square_x;
    int16_t square_y;
    int16_t cx;
    int16_t cy;
    int16_t radius;
    uint8_t sat_i;

    if ((u8g2 == 0) || (plot_rect == 0) || (snapshot == 0))
    {
        return;
    }

    square_side = plot_rect->h;
    if (square_side > plot_rect->w)
    {
        square_side = plot_rect->w;
    }

    square_x = (int16_t)(plot_rect->x + ((plot_rect->w - square_side) / 2));
    square_y = plot_rect->y;
    cx = (int16_t)(square_x + (square_side / 2));
    cy = (int16_t)(square_y + (square_side / 2));
    radius = (int16_t)((square_side / 2) - 7);

    if (radius < 8)
    {
        radius = 8;
    }

    /* ---------------------------------------------------------------------- */
    /*  Sky plot skeleton                                                      */
    /*                                                                        */
    /*  상단 84x84 근처의 정사각형 중앙에 horizon 원을 그리고,                  */
    /*  그 안에 45도 고도 보조원과 십자선을 넣는다.                            */
    /* ---------------------------------------------------------------------- */
    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawCircle(u8g2,
                    (u8g2_uint_t)cx,
                    (u8g2_uint_t)cy,
                    (u8g2_uint_t)radius,
                    U8G2_DRAW_ALL);

    u8g2_DrawCircle(u8g2,
                    (u8g2_uint_t)cx,
                    (u8g2_uint_t)cy,
                    (u8g2_uint_t)(radius / 2),
                    U8G2_DRAW_ALL);

    u8g2_DrawVLine(u8g2,
                   (u8g2_uint_t)cx,
                   (u8g2_uint_t)(cy - radius),
                   (u8g2_uint_t)(radius * 2));

    u8g2_DrawHLine(u8g2,
                   (u8g2_uint_t)(cx - radius),
                   (u8g2_uint_t)cy,
                   (u8g2_uint_t)(radius * 2));

    /*  북쪽 표시 글자 N는 원 위쪽 바깥에 작게 찍는다. */
    u8g2_SetFont(u8g2, UI_SCREEN_GPS_FONT_SMALL);
    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)(cx - 2),
                 (u8g2_uint_t)(square_y + 6),
                 "N");

    /* ---------------------------------------------------------------------- */
    /*  Satellite markers                                                      */
    /*                                                                        */
    /*  azimuth 0 deg = 북쪽 위                                                */
    /*  azimuth 90 deg = 오른쪽                                                */
    /*  elevation 90 deg = 중심                                                */
    /*  elevation 0 deg  = 외곽 원                                              */
    /* ---------------------------------------------------------------------- */
    for (sat_i = 0u; sat_i < snapshot->nav_sat_count; sat_i++)
    {
        const app_gps_sat_t *sat;
        int16_t elev_deg;
        int16_t az_deg;
        int16_t radial_dist;
        int16_t sin_q10;
        int16_t cos_q10;
        int16_t px;
        int16_t py;

        sat = &snapshot->sats[sat_i];

        if (sat->visible == 0u)
        {
            continue;
        }

        elev_deg = sat->elevation_deg;
        if (elev_deg < 0)
        {
            elev_deg = 0;
        }
        if (elev_deg > 90)
        {
            elev_deg = 90;
        }

        az_deg = sat->azimuth_deg;
        while (az_deg < 0)
        {
            az_deg = (int16_t)(az_deg + 360);
        }
        while (az_deg >= 360)
        {
            az_deg = (int16_t)(az_deg - 360);
        }

        radial_dist = (int16_t)(((90 - elev_deg) * radius) / 90);
        sin_q10 = ui_screen_gps_sin_deg_q10(az_deg);
        cos_q10 = ui_screen_gps_sin_deg_q10((int16_t)(90 - az_deg));

        px = (int16_t)(cx + (int16_t)(((int32_t)sin_q10 * (int32_t)radial_dist) / 1024));
        py = (int16_t)(cy - (int16_t)(((int32_t)cos_q10 * (int32_t)radial_dist) / 1024));

        if (sat->used_in_solution != 0u)
        {
            u8g2_DrawDisc(u8g2,
                          (u8g2_uint_t)px,
                          (u8g2_uint_t)py,
                          2u,
                          U8G2_DRAW_ALL);
        }
        else
        {
            u8g2_DrawCircle(u8g2,
                            (u8g2_uint_t)px,
                            (u8g2_uint_t)py,
                            2u,
                            U8G2_DRAW_ALL);
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  Draw CN0 bars                                                             */
/*                                                                            */
/*  우측 pane의 하단 영역에 Garmin류 느낌의 위성 번호별 수신 강도 bar를 그린다. */
/*                                                                            */
/*  표현 규칙                                                                  */
/*  - X축: sv_id                                                               */
/*  - Y축: cno_dbhz                                                            */
/*  - fill  : used_in_solution == 1                                            */
/*  - hollow: used_in_solution == 0                                            */
/* -------------------------------------------------------------------------- */
static void ui_screen_gps_draw_cn0_bars(u8g2_t *u8g2,
                                        const ui_rect_t *plot_rect,
                                        const app_gps_ui_snapshot_t *snapshot)
{
    uint8_t indices[APP_GPS_MAX_SATS];
    uint8_t sat_count;
    int16_t baseline_y;
    int16_t bar_top_limit_y;
    int16_t graph_h;
    int16_t slot_w;
    uint8_t i;

    if ((u8g2 == 0) || (plot_rect == 0) || (snapshot == 0))
    {
        return;
    }

    ui_screen_gps_sort_visible_sat_indices(snapshot, indices, &sat_count);

    baseline_y = (int16_t)(plot_rect->y + plot_rect->h - UI_SCREEN_GPS_BAR_LABEL_H - 1);
    bar_top_limit_y = plot_rect->y;
    graph_h = (int16_t)(baseline_y - bar_top_limit_y);

    if (graph_h < 4)
    {
        graph_h = 4;
    }

    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawHLine(u8g2,
                   (u8g2_uint_t)plot_rect->x,
                   (u8g2_uint_t)baseline_y,
                   (u8g2_uint_t)plot_rect->w);

    if (sat_count == 0u)
    {
        u8g2_SetFont(u8g2, UI_SCREEN_GPS_FONT_SMALL);
        u8g2_DrawStr(u8g2,
                     (u8g2_uint_t)(plot_rect->x + 2),
                     (u8g2_uint_t)(plot_rect->y + 7),
                     "NO SAT");
        return;
    }

    slot_w = (int16_t)(plot_rect->w / sat_count);
    if (slot_w < 3)
    {
        slot_w = 3;
    }

    for (i = 0u; i < sat_count; i++)
    {
        const app_gps_sat_t *sat;
        int16_t x0;
        int16_t bar_w;
        int16_t bar_h;
        int16_t bar_y;
        char id_text[4];

        sat = &snapshot->sats[indices[i]];

        x0 = (int16_t)(plot_rect->x + (int16_t)i * slot_w + 1);
        bar_w = (int16_t)(slot_w - 2);
        if (bar_w < 1)
        {
            bar_w = 1;
        }

        bar_h = (int16_t)(((uint32_t)sat->cno_dbhz * (uint32_t)graph_h) / UI_SCREEN_GPS_BAR_MAX_CNO);
        if (bar_h > graph_h)
        {
            bar_h = graph_h;
        }

        bar_y = (int16_t)(baseline_y - bar_h);

        if (sat->used_in_solution != 0u)
        {
            u8g2_DrawBox(u8g2,
                         (u8g2_uint_t)x0,
                         (u8g2_uint_t)bar_y,
                         (u8g2_uint_t)bar_w,
                         (u8g2_uint_t)bar_h);
        }
        else
        {
            u8g2_DrawFrame(u8g2,
                           (u8g2_uint_t)x0,
                           (u8g2_uint_t)bar_y,
                           (u8g2_uint_t)bar_w,
                           (u8g2_uint_t)bar_h);
        }

        /* ------------------------------------------------------------------ */
        /*  X축 위성 번호 라벨                                                 */
        /*                                                                    */
        /*  slot width가 충분할 때는 전체 sv_id를,                            */
        /*  너무 좁을 때는 겹침 방지를 위해 듬성듬성만 찍는다.                  */
        /* ------------------------------------------------------------------ */
        if (slot_w >= 10)
        {
            snprintf(id_text, sizeof(id_text), "%u", (unsigned int)sat->sv_id);
            u8g2_SetFont(u8g2, UI_SCREEN_GPS_FONT_SMALL);
            u8g2_DrawStr(u8g2,
                         (u8g2_uint_t)(x0 + ((bar_w - u8g2_GetStrWidth(u8g2, id_text)) / 2)),
                         (u8g2_uint_t)(plot_rect->y + plot_rect->h - 1),
                         id_text);
        }
        else if ((slot_w >= 6) && ((i & 1u) == 0u))
        {
            snprintf(id_text, sizeof(id_text), "%u", (unsigned int)(sat->sv_id % 10u));
            u8g2_SetFont(u8g2, UI_SCREEN_GPS_FONT_SMALL);
            u8g2_DrawStr(u8g2,
                         (u8g2_uint_t)x0,
                         (u8g2_uint_t)(plot_rect->y + plot_rect->h - 1),
                         id_text);
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  Public draw                                                               */
/*                                                                            */
/*  viewport를 좌/우 두 pane로 분리하고                                        */
/*  - 좌측 pane : 텍스트 정보                                                  */
/*  - 우측 pane : sky plot + CN0 bar                                           */
/*  를 각각 전용 helper로 그린다.                                             */
/* -------------------------------------------------------------------------- */
void UI_ScreenGps_Draw(u8g2_t *u8g2,
                       const ui_rect_t *viewport,
                       uint8_t imperial_units)
{
    app_gps_ui_snapshot_t snapshot;
    app_gps_boot_profile_t current_mode;
    ui_rect_t info_pane;
    ui_rect_t plot_pane;
    ui_rect_t sky_rect;
    ui_rect_t bar_rect;
    int16_t sky_h;

    if ((u8g2 == 0) || (viewport == 0))
    {
        return;
    }

    if ((viewport->w <= 0) || (viewport->h <= 0))
    {
        return;
    }

    memset(&snapshot, 0, sizeof(snapshot));
    APP_STATE_CopyGpsUiSnapshot(&snapshot);
    current_mode = g_app_state.settings.gps.boot_profile;

    ui_screen_gps_compute_panes(viewport, &info_pane, &plot_pane);

    /* ---------------------------------------------------------------------- */
    /*  가운데 분할선                                                         */
    /*                                                                        */
    /*  좌측 정보와 우측 그래프가 육안으로 분리되도록                          */
    /*  pane 경계에 1px 세로선을 넣는다.                                       */
    /* ---------------------------------------------------------------------- */
    u8g2_SetDrawColor(u8g2, 1);
    if (plot_pane.x > viewport->x)
    {
        u8g2_DrawVLine(u8g2,
                       (u8g2_uint_t)(plot_pane.x - 1),
                       (u8g2_uint_t)viewport->y,
                       (u8g2_uint_t)viewport->h);
    }

    ui_screen_gps_draw_info_pane(u8g2,
                                 &info_pane,
                                 &snapshot,
                                 current_mode,
                                 imperial_units);

    /* ---------------------------------------------------------------------- */
    /*  우측 pane 내부를 다시 상/하 분할                                       */
    /*                                                                        */
    /*  - 상단 3/4 : 정사각형 sky plot                                          */
    /*  - 하단 1/4 : CN0 bar graph                                             */
    /* ---------------------------------------------------------------------- */
    sky_h = (int16_t)((plot_pane.h * UI_SCREEN_GPS_SKY_SECTION_NUM) / UI_SCREEN_GPS_SKY_SECTION_DEN);

    sky_rect.x = plot_pane.x;
    sky_rect.y = plot_pane.y;
    sky_rect.w = plot_pane.w;
    sky_rect.h = sky_h;

    bar_rect.x = plot_pane.x;
    bar_rect.y = (int16_t)(plot_pane.y + sky_h + UI_SCREEN_GPS_SKY_BAR_GAP);
    bar_rect.w = plot_pane.w;
    bar_rect.h = (int16_t)(plot_pane.h - sky_h - UI_SCREEN_GPS_SKY_BAR_GAP);

    ui_screen_gps_draw_sky_plot(u8g2, &sky_rect, &snapshot);

    if ((bar_rect.h > 0) && (bar_rect.w > 0))
    {
        ui_screen_gps_draw_cn0_bars(u8g2, &bar_rect, &snapshot);
    }
}
