#include "Motor_State.h"

#include "Motor_Settings.h"
#include "ui_popup.h"
#include "ui_toast.h"

#include <string.h>

static motor_state_t s_motor_state;

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

static void motor_state_refresh_settings_snapshot(void)
{
    const motor_settings_t *settings;

    /* ---------------------------------------------------------------------- */
    /*  Motor_State.settings 의 canonical owner는 Motor_Settings 저장소다.     */
    /*  따라서 runtime copy를 갱신할 때도 항상 그 저장소에서 다시 복사한다.   */
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

    /* ---------------------------------------------------------------------- */
    /*  기본 진입 화면은 라이딩 계기판 메인 화면이다.                           */
    /*  상위 Motor_App는 첫 프레임부터 이 screen id를 기준으로                  */
    /*  big switch-case 상태머신을 구동한다.                                   */
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
    /*  Motor_App가 이번 frame에서 볼 저수준 truth는 APP_STATE snapshot 한 벌 */
    /*  뿐이다. 이후 모든 상위 앱 계산은 이 로컬 snapshot만 참조한다.          */
    /* ---------------------------------------------------------------------- */
    APP_STATE_CopySnapshot(&s_motor_state.snapshot);
    motor_state_refresh_settings_snapshot();

    fix = &s_motor_state.snapshot.gps.fix;
    alt = &s_motor_state.snapshot.altitude;

    /* ---------------------------------------------------------------------- */
    /*  navigation 런타임 정규화                                               */
    /*  - GPS raw fix에서 UI/logger가 바로 쓰기 좋은 최소 파생값만 만든다.     */
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
    /*  drive screen 기억                                                      */
    /*  - 메뉴/설정 진입 전 직전 ride page로 복귀하기 위해 유지한다.            */
    /* ---------------------------------------------------------------------- */
    if ((s_motor_state.ui.screen < (uint8_t)MOTOR_SCREEN_COUNT) &&
        (motor_state_is_drive_screen((motor_screen_t)s_motor_state.ui.screen) != 0u))
    {
        s_motor_state.ui.previous_drive_screen = s_motor_state.ui.screen;
    }

    /* ---------------------------------------------------------------------- */
    /*  drive bottom bar overlay timeout                                       */
    /*  - overlay가 켜진 drive frame은 10초 inactivity가 지나면 자동으로 숨긴다.*/
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
    /*  저수준 UI toast 모듈을 그대로 사용한다.                                 */
    /*  Motor_App는 popup/toast raster를 직접 구현하지 않고 공용 모듈을 쓴다.  */
    /* ---------------------------------------------------------------------- */
    UI_Toast_Show(text, 0, 0u, 0u, s_motor_state.now_ms, hold_ms);
}

void Motor_State_ShowPopup(const char *title,
                           const char *line1,
                           const char *line2,
                           uint32_t hold_ms)
{
    /* ---------------------------------------------------------------------- */
    /*  popup 역시 Motor_App가 저수준 draw 세부사항을 직접 알지 않도록        */
    /*  공용 Display_UI 모듈에 위임한다.                                       */
    /*                                                                        */
    /*  title/line* 가 NULL 이어도 하위 모듈이 안전하게 빈 문자열로           */
    /*  정규화하므로, 여기서는 단순 facade 역할만 수행한다.                    */
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
    /*  drive screen으로 돌아가면 overlay는 기본 숨김 상태로 시작한다.          */
    /*  menu/settings/stub 계열은 fixed bottom bar를 상시 표시하므로            */
    /*  overlay latch를 정리해 둔다.                                           */
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
