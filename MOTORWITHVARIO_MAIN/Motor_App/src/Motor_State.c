
#include "Motor_State.h"

#include "Motor_Settings.h"

#include <string.h>

static motor_state_t s_motor_state;

static uint16_t motor_state_mmps_to_kmh_x10(int32_t mmps)
{
    if (mmps <= 0)
    {
        return 0u;
    }
    return (uint16_t)((mmps * 36) / 100);
}

void Motor_State_Init(void)
{
    memset(&s_motor_state, 0, sizeof(s_motor_state));
    s_motor_state.ui.screen = (uint8_t)MOTOR_SCREEN_MAIN;
    s_motor_state.ui.previous_drive_screen = (uint8_t)MOTOR_SCREEN_MAIN;
    s_motor_state.ui.redraw_requested = 1u;
}

void Motor_State_Task(uint32_t now_ms)
{
    const motor_settings_t *settings;
    const gps_fix_basic_t *fix;
    const app_altitude_state_t *alt;

    s_motor_state.last_task_ms = s_motor_state.now_ms;
    s_motor_state.now_ms = now_ms;

    /* ---------------------------------------------------------------------- */
    /*  상위 앱 레이어의 가장 중요한 규칙                                       */
    /*  - 이번 프레임에서 사용할 저수준 truth 는 APP_STATE snapshot 한 벌이다.   */
    /*  - 이후 Motor_App 안의 하위 모듈은 이 snapshot만 본다.                  */
    /* ---------------------------------------------------------------------- */
    APP_STATE_CopySnapshot(&s_motor_state.snapshot);

    settings = Motor_Settings_Get();
    if (settings != 0)
    {
        memcpy(&s_motor_state.settings, settings, sizeof(s_motor_state.settings));
    }

    fix = &s_motor_state.snapshot.gps.fix;
    alt = &s_motor_state.snapshot.altitude;

    s_motor_state.nav.valid = fix->valid;
    s_motor_state.nav.fix_ok = fix->fixOk;
    s_motor_state.nav.heading_valid = (fix->head_veh_valid != 0u) ? true : false;
    s_motor_state.nav.fix_type = fix->fixType;
    s_motor_state.nav.sats_used = fix->numSV_used;
    s_motor_state.nav.lat_e7 = fix->lat;
    s_motor_state.nav.lon_e7 = fix->lon;
    s_motor_state.nav.speed_mmps = fix->gSpeed;
    s_motor_state.nav.speed_kmh_x10 = motor_state_mmps_to_kmh_x10(fix->gSpeed);
    s_motor_state.nav.heading_deg_x10 = fix->head_veh_valid ? (int32_t)(fix->headVeh / 1000) : (int32_t)(fix->headMot / 1000);
    s_motor_state.nav.altitude_cm = alt->alt_display_cm;
    s_motor_state.nav.rel_altitude_cm = alt->alt_rel_home_noimu_cm;
    s_motor_state.nav.hacc_mm = fix->hAcc;
    s_motor_state.nav.vacc_mm = fix->vAcc;
    s_motor_state.nav.last_fix_ms = fix->last_fix_ms;
    s_motor_state.nav.moving = (s_motor_state.nav.speed_kmh_x10 >= 30u) ? true : false;

    if ((s_motor_state.ui.screen < (uint8_t)MOTOR_SCREEN_MENU) &&
        (s_motor_state.ui.screen < (uint8_t)MOTOR_SCREEN_COUNT))
    {
        s_motor_state.ui.previous_drive_screen = s_motor_state.ui.screen;
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
}

void Motor_State_SetScreen(motor_screen_t screen)
{
    if (screen >= MOTOR_SCREEN_COUNT)
    {
        return;
    }

    s_motor_state.ui.screen = (uint8_t)screen;
    s_motor_state.ui.selected_index = 0u;
    s_motor_state.ui.editing = 0u;
    s_motor_state.ui.redraw_requested = 1u;

    if (screen < MOTOR_SCREEN_MENU)
    {
        s_motor_state.ui.previous_drive_screen = (uint8_t)screen;
    }
}

void Motor_State_StorePreviousDriveScreen(motor_screen_t screen)
{
    if (screen < MOTOR_SCREEN_MENU)
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
