#include "Motor_State.h"
#include "APP_MEMORY_SECTIONS.h"

#include "Motor_Settings.h"
#include "Motor_Navigation.h"
#include "ui_popup.h"
#include "ui_toast.h"

#include <string.h>

static motor_state_t s_motor_state APP_CCMRAM_BSS;

typedef struct
{
    uint8_t  init;
    uint32_t last_ms;
    uint32_t segment_ms;
    uint32_t segment_duration_ms;
    uint32_t seed;
    int32_t  lat_e7;
    int32_t  lon_e7;
    int32_t  altitude_cm;
    int32_t  heading_deg_x10;
    int32_t  target_heading_deg_x10;
    uint16_t speed_kmh_x10;
    uint16_t target_speed_kmh_x10;
} motor_state_gps_sim_t;

static motor_state_gps_sim_t s_motor_state_gps_sim APP_CCMRAM_BSS;
static uint8_t s_motor_state_last_gps_mode;

static uint8_t motor_state_is_drive_screen(motor_screen_t screen)
{
    return (screen < MOTOR_SCREEN_MENU) ? 1u : 0u;
}

static uint16_t motor_state_mmps_to_kmh_x10(int32_t mmps)
{
    if (mmps <= 0)
    {
        return 0u;
    }

    return (uint16_t)((mmps * 36) / 100);
}

static int32_t motor_state_wrap_heading_x10(int32_t heading_deg_x10)
{
    while (heading_deg_x10 < 0)
    {
        heading_deg_x10 += 3600;
    }
    while (heading_deg_x10 >= 3600)
    {
        heading_deg_x10 -= 3600;
    }
    return heading_deg_x10;
}

static uint32_t motor_state_sim_next_u32(void)
{
    s_motor_state_gps_sim.seed = (s_motor_state_gps_sim.seed * 1664525u) + 1013904223u;
    return s_motor_state_gps_sim.seed;
}

static void motor_state_sim_choose_next_segment(void)
{
    static const int16_t s_heading_delta_x10[] = { -450, -225, -90, 90, 225, 450 };
    uint32_t rnd;
    int32_t heading_delta_x10;

    rnd = motor_state_sim_next_u32();
    heading_delta_x10 = s_heading_delta_x10[(rnd >> 16) % (sizeof(s_heading_delta_x10) / sizeof(s_heading_delta_x10[0]))];
    s_motor_state_gps_sim.target_heading_deg_x10 =
        motor_state_wrap_heading_x10(s_motor_state_gps_sim.target_heading_deg_x10 + heading_delta_x10);

    rnd = motor_state_sim_next_u32();
    s_motor_state_gps_sim.target_speed_kmh_x10 = (uint16_t)(180u + ((rnd >> 8) % 481u));

    rnd = motor_state_sim_next_u32();
    s_motor_state_gps_sim.segment_duration_ms = 4000u + ((rnd >> 12) % 7000u);
    s_motor_state_gps_sim.segment_ms = 0u;
}

static void motor_state_sim_reset(uint32_t now_ms, int32_t base_altitude_cm)
{
    static const int32_t s_start_lat_e7[] = { 375664990, 375700100, 375514200, 375802400, 375436500 };
    static const int32_t s_start_lon_e7[] = { 1269780000, 1269910000, 1269883000, 1270418000, 1270079000 };
    uint32_t start_idx;

    memset(&s_motor_state_gps_sim, 0, sizeof(s_motor_state_gps_sim));
    s_motor_state_gps_sim.init = 1u;
    s_motor_state_gps_sim.last_ms = now_ms;
    s_motor_state_gps_sim.seed = 0x13579BDFu ^ now_ms;
    start_idx = motor_state_sim_next_u32() % (sizeof(s_start_lat_e7) / sizeof(s_start_lat_e7[0]));
    s_motor_state_gps_sim.lat_e7 = s_start_lat_e7[start_idx];
    s_motor_state_gps_sim.lon_e7 = s_start_lon_e7[start_idx];
    s_motor_state_gps_sim.altitude_cm = (base_altitude_cm != 0) ? base_altitude_cm : 3800;
    s_motor_state_gps_sim.heading_deg_x10 = 900;
    s_motor_state_gps_sim.target_heading_deg_x10 = 900;
    s_motor_state_gps_sim.speed_kmh_x10 = 280u;
    s_motor_state_gps_sim.target_speed_kmh_x10 = 280u;
    motor_state_sim_choose_next_segment();
}

static void motor_state_apply_gps_simulator_fix_to_app_state(uint32_t now_ms)
{
    static const int16_t s_east_permille[16] =
    {
         0,  383,  707,  924, 1000,  924,  707,  383,
         0, -383, -707, -924,-1000, -924, -707, -383
    };
    static const int16_t s_north_permille[16] =
    {
        1000,  924,  707,  383,    0, -383, -707, -924,
       -1000, -924, -707, -383,    0,  383,  707,  924
    };
    uint32_t dt_ms;
    int32_t heading_error_x10;
    int32_t heading_step_x10;
    int32_t speed_error_x10;
    int32_t speed_step_x10;
    int32_t direction_idx;
    int64_t travel_mm;
    int64_t east_mm;
    int64_t north_mm;

    app_gps_state_t *gps;
    int32_t base_altitude_cm;

    gps = (app_gps_state_t *)&g_app_state.gps;
    base_altitude_cm = g_app_state.altitude.alt_display_cm;

    if (s_motor_state_gps_sim.init == 0u)
    {
        motor_state_sim_reset(now_ms, base_altitude_cm);
    }

    if (now_ms <= s_motor_state_gps_sim.last_ms)
    {
        dt_ms = 0u;
    }
    else
    {
        dt_ms = now_ms - s_motor_state_gps_sim.last_ms;
    }

    if (dt_ms > 1000u)
    {
        dt_ms = 1000u;
    }
    s_motor_state_gps_sim.last_ms = now_ms;
    s_motor_state_gps_sim.segment_ms += dt_ms;

    if (s_motor_state_gps_sim.segment_ms >= s_motor_state_gps_sim.segment_duration_ms)
    {
        motor_state_sim_choose_next_segment();
    }

    heading_error_x10 = s_motor_state_gps_sim.target_heading_deg_x10 - s_motor_state_gps_sim.heading_deg_x10;
    if (heading_error_x10 > 1800)
    {
        heading_error_x10 -= 3600;
    }
    else if (heading_error_x10 < -1800)
    {
        heading_error_x10 += 3600;
    }
    heading_step_x10 = (int32_t)((dt_ms * 180u) / 1000u);
    if (heading_step_x10 < 1)
    {
        heading_step_x10 = 1;
    }
    if (heading_error_x10 > heading_step_x10)
    {
        heading_error_x10 = heading_step_x10;
    }
    else if (heading_error_x10 < -heading_step_x10)
    {
        heading_error_x10 = -heading_step_x10;
    }
    s_motor_state_gps_sim.heading_deg_x10 =
        motor_state_wrap_heading_x10(s_motor_state_gps_sim.heading_deg_x10 + heading_error_x10);

    speed_error_x10 = (int32_t)s_motor_state_gps_sim.target_speed_kmh_x10 - (int32_t)s_motor_state_gps_sim.speed_kmh_x10;
    speed_step_x10 = (int32_t)((dt_ms * 35u) / 1000u);
    if (speed_step_x10 < 1)
    {
        speed_step_x10 = 1;
    }
    if (speed_error_x10 > speed_step_x10)
    {
        speed_error_x10 = speed_step_x10;
    }
    else if (speed_error_x10 < -speed_step_x10)
    {
        speed_error_x10 = -speed_step_x10;
    }
    s_motor_state_gps_sim.speed_kmh_x10 = (uint16_t)((int32_t)s_motor_state_gps_sim.speed_kmh_x10 + speed_error_x10);

    direction_idx = (int32_t)((s_motor_state_gps_sim.heading_deg_x10 + 112) / 225) & 0x0F;
    travel_mm = ((int64_t)s_motor_state_gps_sim.speed_kmh_x10 * 1000000ll * (int64_t)dt_ms) / 36000ll;
    east_mm = (travel_mm * (int64_t)s_east_permille[direction_idx]) / 1000ll;
    north_mm = (travel_mm * (int64_t)s_north_permille[direction_idx]) / 1000ll;

    s_motor_state_gps_sim.lat_e7 += (int32_t)((north_mm * 898ll) / 10000ll);
    s_motor_state_gps_sim.lon_e7 += (int32_t)((east_mm * 1133ll) / 10000ll);
    s_motor_state_gps_sim.altitude_cm = 3800 +
                                        (int32_t)(((int32_t)(motor_state_sim_next_u32() >> 24) % 7) - 3);

    gps->fix.valid = true;
    gps->fix.fixOk = true;
    gps->fix.fixType = 3u;
    gps->fix.head_veh_valid = 1u;
    gps->fix.numSV_nav_pvt = 15u;
    gps->fix.numSV_visible = 18u;
    gps->fix.numSV_used = 15u;
    gps->fix.lat = s_motor_state_gps_sim.lat_e7;
    gps->fix.lon = s_motor_state_gps_sim.lon_e7;
    gps->fix.height = s_motor_state_gps_sim.altitude_cm * 10;
    gps->fix.hMSL = gps->fix.height;
    gps->fix.gSpeed = (int32_t)(((int64_t)s_motor_state_gps_sim.speed_kmh_x10 * 1000000ll) / 36000ll);
    gps->fix.headVeh = s_motor_state_gps_sim.heading_deg_x10 * 1000;
    gps->fix.headMot = s_motor_state_gps_sim.heading_deg_x10 * 1000;
    gps->fix.hAcc = 900u;
    gps->fix.vAcc = 1400u;
    gps->fix.sAcc = 180u;
    gps->fix.headAcc = 25000u;
    gps->fix.pDOP = 85u;
    gps->fix.last_update_ms = now_ms;
    gps->fix.last_fix_ms = now_ms;
    gps->sat_count_visible = gps->fix.numSV_visible;
    gps->sat_count_used = gps->fix.numSV_used;
}

void Motor_State_ApplyGpsSimulator(uint32_t now_ms)
{
    const motor_settings_t *settings;

    settings = Motor_Settings_Get();
    if ((settings != 0) &&
        (settings->gps.dynamic_model == (uint8_t)MOTOR_GPS_DYNAMIC_MODEL_SIMULATOR))
    {
        if (s_motor_state_last_gps_mode != (uint8_t)MOTOR_GPS_DYNAMIC_MODEL_SIMULATOR)
        {
            motor_state_sim_reset(now_ms, g_app_state.altitude.alt_display_cm);
            Motor_Navigation_ResetTrail();
        }
        motor_state_apply_gps_simulator_fix_to_app_state(now_ms);
        s_motor_state_last_gps_mode = (uint8_t)MOTOR_GPS_DYNAMIC_MODEL_SIMULATOR;
    }
    else
    {
        if (s_motor_state_last_gps_mode == (uint8_t)MOTOR_GPS_DYNAMIC_MODEL_SIMULATOR)
        {
            Motor_Navigation_ResetTrail();
        }
        s_motor_state_gps_sim.init = 0u;
        s_motor_state_last_gps_mode = (settings != 0) ? settings->gps.dynamic_model : (uint8_t)MOTOR_GPS_DYNAMIC_MODEL_AUTOMOTIVE;
    }
}

static void motor_state_refresh_settings_snapshot(void)
{
    const motor_settings_t *settings;

    /* ---------------------------------------------------------------------- */
    /*  Motor_State.settings ??canonical owner??Motor_Settings ??μ냼??     */
    /*  ?곕씪??runtime copy瑜?媛깆떊???뚮룄 ??긽 洹???μ냼?먯꽌 ?ㅼ떆 蹂듭궗?쒕떎.   */
    /* ---------------------------------------------------------------------- */
    settings = Motor_Settings_Get();
    if (settings != 0)
    {
        memcpy(&s_motor_state.settings, settings, sizeof(s_motor_state.settings));
    }
}

void Motor_State_Init(void)
{
    memset(&s_motor_state, 0, sizeof(s_motor_state));
    memset(&s_motor_state_gps_sim, 0, sizeof(s_motor_state_gps_sim));
    s_motor_state_last_gps_mode = (uint8_t)MOTOR_GPS_DYNAMIC_MODEL_AUTOMOTIVE;

    /* ---------------------------------------------------------------------- */
    /*  湲곕낯 吏꾩엯 ?붾㈃? ?쇱씠??怨꾧린??硫붿씤 ?붾㈃?대떎.                           */
    /*  ?곸쐞 Motor_App??泥??꾨젅?꾨?????screen id瑜?湲곗??쇰줈                  */
    /*  big switch-case ?곹깭癒몄떊??援щ룞?쒕떎.                                   */
    /* ---------------------------------------------------------------------- */
    s_motor_state.ui.screen = (uint8_t)MOTOR_SCREEN_MAIN;
    s_motor_state.ui.previous_drive_screen = (uint8_t)MOTOR_SCREEN_MAIN;
    s_motor_state.ui.selected_index = 0u;
    s_motor_state.ui.selected_slot = 0u;
    s_motor_state.ui.first_visible_row = 0u;
    s_motor_state.ui.editing = 0u;
    s_motor_state.ui.screen_locked = 0u;
    s_motor_state.ui.redraw_requested = 1u;
    s_motor_state.ui.overlay_visible = 0u;
    s_motor_state.ui.overlay_key_armed = 0u;
    s_motor_state.ui.bottom_page = 0u;
    s_motor_state.ui.overlay_until_ms = 0u;
    s_motor_state.ui.toast_until_ms = 0u;
}

void Motor_State_Task(uint32_t now_ms)
{
    const gps_fix_basic_t *fix;
    const app_altitude_state_t *alt;

    s_motor_state.last_task_ms = s_motor_state.now_ms;
    s_motor_state.now_ms = now_ms;

    /* ---------------------------------------------------------------------- */
    /*  Motor_App媛 ?대쾲 frame?먯꽌 蹂???섏? truth??APP_STATE snapshot ??踰?*/
    /*  肉먯씠?? ?댄썑 紐⑤뱺 ?곸쐞 ??怨꾩궛? ??濡쒖뺄 snapshot留?李몄“?쒕떎.          */
    /* ---------------------------------------------------------------------- */
    APP_STATE_CopySnapshot(&s_motor_state.snapshot);
    motor_state_refresh_settings_snapshot();

    fix = &s_motor_state.snapshot.gps.fix;
    alt = &s_motor_state.snapshot.altitude;

    /* ---------------------------------------------------------------------- */
    /*  navigation ?고????뺢퇋??                                              */
    /*  - GPS raw fix?먯꽌 UI/logger媛 諛붾줈 ?곌린 醫뗭? 理쒖냼 ?뚯깮媛믩쭔 留뚮뱺??     */
    /* ---------------------------------------------------------------------- */
    s_motor_state.nav.valid = fix->valid;
    s_motor_state.nav.fix_ok = fix->fixOk;
    s_motor_state.nav.heading_valid = (fix->head_veh_valid != 0u) ? true : false;
    s_motor_state.nav.fix_type = fix->fixType;
    s_motor_state.nav.sats_used = fix->numSV_used;
    s_motor_state.nav.lat_e7 = fix->lat;
    s_motor_state.nav.lon_e7 = fix->lon;
    s_motor_state.nav.speed_mmps = fix->gSpeed;
    s_motor_state.nav.speed_kmh_x10 = motor_state_mmps_to_kmh_x10(fix->gSpeed);
    s_motor_state.nav.heading_deg_x10 = (int32_t)((fix->head_veh_valid != 0u) ? (fix->headVeh / 1000) : (fix->headMot / 1000));
    s_motor_state.nav.altitude_cm = alt->alt_display_cm;
    s_motor_state.nav.rel_altitude_cm = alt->alt_rel_home_noimu_cm;
    s_motor_state.nav.hacc_mm = fix->hAcc;
    s_motor_state.nav.vacc_mm = fix->vAcc;
    s_motor_state.nav.last_fix_ms = fix->last_fix_ms;
    s_motor_state.nav.moving = (s_motor_state.nav.speed_kmh_x10 >= 30u) ? true : false;

    /* ---------------------------------------------------------------------- */
    /*  drive screen 湲곗뼲                                                      */
    /*  - 硫붾돱/?ㅼ젙 吏꾩엯 ??吏곸쟾 ride page濡?蹂듦??섍린 ?꾪빐 ?좎??쒕떎.            */
    /* ---------------------------------------------------------------------- */
    if ((s_motor_state.ui.screen < (uint8_t)MOTOR_SCREEN_COUNT) &&
        (motor_state_is_drive_screen((motor_screen_t)s_motor_state.ui.screen) != 0u))
    {
        s_motor_state.ui.previous_drive_screen = s_motor_state.ui.screen;
    }

    /* ---------------------------------------------------------------------- */
    /*  drive bottom bar overlay timeout                                       */
    /*  - overlay媛 耳쒖쭊 drive frame? 10珥?inactivity媛 吏?섎㈃ ?먮룞?쇰줈 ?④릿??*/
    /* ---------------------------------------------------------------------- */
    if ((s_motor_state.ui.overlay_visible != 0u) && (s_motor_state.ui.overlay_until_ms <= now_ms))
    {
        s_motor_state.ui.overlay_visible = 0u;
        s_motor_state.ui.overlay_key_armed = 0u;
        s_motor_state.ui.redraw_requested = 1u;
    }
}

const motor_state_t *Motor_State_Get(void)
{
    return &s_motor_state;
}

motor_state_t *Motor_State_GetMutable(void)
{
    return &s_motor_state;
}

void Motor_State_RefreshSettingsSnapshot(void)
{
    motor_state_refresh_settings_snapshot();
}

void Motor_State_RequestRedraw(void)
{
    s_motor_state.ui.redraw_requested = 1u;
}

void Motor_State_ShowToast(const char *text, uint32_t hold_ms)
{
    if (text == 0)
    {
        return;
    }

    (void)strncpy(s_motor_state.ui.toast_text, text, sizeof(s_motor_state.ui.toast_text) - 1u);
    s_motor_state.ui.toast_text[sizeof(s_motor_state.ui.toast_text) - 1u] = '\0';
    s_motor_state.ui.toast_until_ms = s_motor_state.now_ms + hold_ms;
    s_motor_state.ui.redraw_requested = 1u;

    /* ---------------------------------------------------------------------- */
    /*  ??섏? UI toast 紐⑤뱢??洹몃?濡??ъ슜?쒕떎.                                 */
    /*  Motor_App??popup/toast raster瑜?吏곸젒 援ы쁽?섏? ?딄퀬 怨듭슜 紐⑤뱢???대떎.  */
    /* ---------------------------------------------------------------------- */
    UI_Toast_Show(text, 0, 0u, 0u, s_motor_state.now_ms, hold_ms);
}

void Motor_State_ShowPopup(const char *title,
                           const char *line1,
                           const char *line2,
                           uint32_t hold_ms)
{
    /* ---------------------------------------------------------------------- */
    /*  popup ??떆 Motor_App媛 ??섏? draw ?몃??ы빆??吏곸젒 ?뚯? ?딅룄濡?       */
    /*  怨듭슜 Display_UI 紐⑤뱢???꾩엫?쒕떎.                                       */
    /*                                                                        */
    /*  title/line* 媛 NULL ?댁뼱???섏쐞 紐⑤뱢???덉쟾?섍쾶 鍮?臾몄옄?대줈           */
    /*  ?뺢퇋?뷀븯誘濡? ?ш린?쒕뒗 ?⑥닚 facade ??븷留??섑뻾?쒕떎.                    */
    /* ---------------------------------------------------------------------- */
    UI_Popup_Show(title,
                  line1,
                  line2,
                  0,
                  0u,
                  0u,
                  s_motor_state.now_ms,
                  hold_ms);
    s_motor_state.ui.redraw_requested = 1u;
}

void Motor_State_HidePopup(void)
{
    UI_Popup_Hide();
    s_motor_state.ui.redraw_requested = 1u;
}

void Motor_State_SetScreen(motor_screen_t screen)
{
    if (screen >= MOTOR_SCREEN_COUNT)
    {
        return;
    }

    s_motor_state.ui.screen = (uint8_t)screen;
    s_motor_state.ui.selected_index = 0u;
    s_motor_state.ui.selected_slot = 0u;
    s_motor_state.ui.first_visible_row = 0u;
    s_motor_state.ui.editing = 0u;
    s_motor_state.ui.redraw_requested = 1u;

    /* ---------------------------------------------------------------------- */
    /*  drive screen?쇰줈 ?뚯븘媛硫?overlay??湲곕낯 ?④? ?곹깭濡??쒖옉?쒕떎.          */
    /*  menu/settings/stub 怨꾩뿴? fixed bottom bar瑜??곸떆 ?쒖떆?섎?濡?           */
    /*  overlay latch瑜??뺣━???붾떎.                                           */
    /* ---------------------------------------------------------------------- */
    s_motor_state.ui.overlay_visible = 0u;
    s_motor_state.ui.overlay_key_armed = 0u;
    s_motor_state.ui.overlay_until_ms = 0u;

    if (motor_state_is_drive_screen(screen) != 0u)
    {
        s_motor_state.ui.previous_drive_screen = (uint8_t)screen;
    }
}

void Motor_State_StorePreviousDriveScreen(motor_screen_t screen)
{
    if (motor_state_is_drive_screen(screen) != 0u)
    {
        s_motor_state.ui.previous_drive_screen = (uint8_t)screen;
    }
}

void Motor_State_RequestMarker(void)
{
    s_motor_state.record.marker_requested = true;
}

void Motor_State_RequestRecordToggle(void)
{
    if ((motor_record_state_t)s_motor_state.record.state == MOTOR_RECORD_STATE_RECORDING)
    {
        s_motor_state.record.state = (uint8_t)MOTOR_RECORD_STATE_PAUSED;
        Motor_State_ShowToast("REC PAUSE", 1200u);
    }
    else if ((motor_record_state_t)s_motor_state.record.state == MOTOR_RECORD_STATE_PAUSED)
    {
        s_motor_state.record.state = (uint8_t)MOTOR_RECORD_STATE_RECORDING;
        Motor_State_ShowToast("REC RESUME", 1200u);
    }
    else
    {
        s_motor_state.record.start_requested = true;
    }
}

void Motor_State_RequestRecordStop(void)
{
    s_motor_state.record.stop_requested = true;
}
