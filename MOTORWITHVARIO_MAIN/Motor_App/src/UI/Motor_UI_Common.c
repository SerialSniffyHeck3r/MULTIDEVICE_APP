#include "Motor_UI_Internal.h"

#include "Motor_Units.h"
#include "button.h"
#include "ui_bottombar.h"

#include <stdio.h>
#include <string.h>

const char *Motor_UI_GetScreenShortTitle(motor_screen_t screen)
{
    switch (screen)
    {
    case MOTOR_SCREEN_MAIN:            return "MAIN";
    case MOTOR_SCREEN_DATA_FIELD_1:    return "FIELD1";
    case MOTOR_SCREEN_DATA_FIELD_2:    return "FIELD2";
    case MOTOR_SCREEN_CORNER:          return "CORNER";
    case MOTOR_SCREEN_COMPASS:         return "COMPASS";
    case MOTOR_SCREEN_BREADCRUMB:      return "TRACK";
    case MOTOR_SCREEN_ALTITUDE:        return "ALT";
    case MOTOR_SCREEN_HORIZON:         return "HORIZON";
    case MOTOR_SCREEN_VEHICLE_SUMMARY: return "VEHICLE";
    case MOTOR_SCREEN_MENU:            return "MENU";
    case MOTOR_SCREEN_SETTINGS_ROOT:   return "SET";
    case MOTOR_SCREEN_SETTINGS_DISPLAY:return "DISPLAY";
    case MOTOR_SCREEN_SETTINGS_GPS:    return "GPS";
    case MOTOR_SCREEN_SETTINGS_UNITS:  return "UNITS";
    case MOTOR_SCREEN_SETTINGS_RECORDING:return "REC";
    case MOTOR_SCREEN_SETTINGS_DYNAMICS:return "DYN";
    case MOTOR_SCREEN_SETTINGS_MAINTENANCE:return "MAINT";
    case MOTOR_SCREEN_SETTINGS_OBD:    return "OBD";
    case MOTOR_SCREEN_SETTINGS_SYSTEM: return "SYS";
    default:                           return "MOTOR";
    }
}

void Motor_UI_BuildStatusBarModel(const motor_state_t *state, ui_statusbar_model_t *out_model)
{
    if ((state == 0) || (out_model == 0))
    {
        return;
    }

    memset(out_model, 0, sizeof(*out_model));

    out_model->gps_fix = state->snapshot.gps.fix;
    out_model->temp_status_flags = state->snapshot.ds18b20.status_flags;
    out_model->temp_c_x100 = state->snapshot.ds18b20.raw.temp_c_x100;
    out_model->temp_f_x100 = state->snapshot.ds18b20.raw.temp_f_x100;
    out_model->temp_last_error = state->snapshot.ds18b20.debug.last_error;
    out_model->sd_inserted = state->snapshot.sd.detect_stable_present;
    out_model->sd_mounted = state->snapshot.sd.mounted;
    out_model->sd_initialized = state->snapshot.sd.initialized;
    out_model->time_valid = state->snapshot.clock.rtc_time_valid;
    out_model->time_year = state->snapshot.clock.local.year;
    out_model->time_month = state->snapshot.clock.local.month;
    out_model->time_day = state->snapshot.clock.local.day;
    out_model->time_hour = state->snapshot.clock.local.hour;
    out_model->time_minute = state->snapshot.clock.local.min;
    out_model->time_weekday = state->snapshot.clock.local.weekday;

    switch ((motor_record_state_t)state->record.state)
    {
    case MOTOR_RECORD_STATE_RECORDING:
        out_model->record_state = UI_RECORD_STATE_REC;
        break;
    case MOTOR_RECORD_STATE_PAUSED:
        out_model->record_state = UI_RECORD_STATE_PAUSE;
        break;
    case MOTOR_RECORD_STATE_IDLE:
    case MOTOR_RECORD_STATE_ARMED:
    case MOTOR_RECORD_STATE_CLOSING:
    case MOTOR_RECORD_STATE_ERROR:
    default:
        out_model->record_state = UI_RECORD_STATE_STOP;
        break;
    }

    out_model->imperial_units = (state->settings.units.speed == (uint8_t)MOTOR_SPEED_UNIT_MPH) ? 1u : 0u;
    out_model->bluetooth_stub_state = (state->vehicle.connected != false) ? UI_BT_STUB_ON : UI_BT_STUB_OFF;
}

void Motor_UI_ConfigureBottomBar(const motor_state_t *state)
{
    if (state == 0)
    {
        return;
    }

    UI_BottomBar_SetMode(UI_BOTTOMBAR_MODE_BUTTONS);

    switch ((motor_screen_t)state->ui.screen)
    {
    case MOTOR_SCREEN_MAIN:
    case MOTOR_SCREEN_CORNER:
    case MOTOR_SCREEN_COMPASS:
    case MOTOR_SCREEN_BREADCRUMB:
    case MOTOR_SCREEN_ALTITUDE:
    case MOTOR_SCREEN_HORIZON:
    case MOTOR_SCREEN_VEHICLE_SUMMARY:
        UI_BottomBar_SetButton(UI_FKEY_1, "PREV", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_2, "NEXT", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_3, "MARK", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_4, "PEAK", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_5, "REC", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_6, "MENU", 0u);
        break;

    case MOTOR_SCREEN_DATA_FIELD_1:
    case MOTOR_SCREEN_DATA_FIELD_2:
        if (state->ui.editing != 0u)
        {
            UI_BottomBar_SetButton(UI_FKEY_1, "SLOT-", UI_BOTTOMBAR_FLAG_DIVIDER);
            UI_BottomBar_SetButton(UI_FKEY_2, "SLOT+", UI_BOTTOMBAR_FLAG_DIVIDER);
            UI_BottomBar_SetButton(UI_FKEY_3, "FIELD-", UI_BOTTOMBAR_FLAG_DIVIDER);
            UI_BottomBar_SetButton(UI_FKEY_4, "FIELD+", UI_BOTTOMBAR_FLAG_DIVIDER);
            UI_BottomBar_SetButton(UI_FKEY_5, "DONE", UI_BOTTOMBAR_FLAG_DIVIDER);
            UI_BottomBar_SetButton(UI_FKEY_6, "MENU", 0u);
        }
        else
        {
            UI_BottomBar_SetButton(UI_FKEY_1, "PREV", UI_BOTTOMBAR_FLAG_DIVIDER);
            UI_BottomBar_SetButton(UI_FKEY_2, "NEXT", UI_BOTTOMBAR_FLAG_DIVIDER);
            UI_BottomBar_SetButton(UI_FKEY_3, "SLOT-", UI_BOTTOMBAR_FLAG_DIVIDER);
            UI_BottomBar_SetButton(UI_FKEY_4, "SLOT+", UI_BOTTOMBAR_FLAG_DIVIDER);
            UI_BottomBar_SetButton(UI_FKEY_5, "EDIT", UI_BOTTOMBAR_FLAG_DIVIDER);
            UI_BottomBar_SetButton(UI_FKEY_6, "MENU", 0u);
        }
        break;

    case MOTOR_SCREEN_MENU:
        UI_BottomBar_SetButton(UI_FKEY_1, "UP", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_2, "DN", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_3, "", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_4, "", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_5, "OK", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_6, "BACK", 0u);
        break;

    case MOTOR_SCREEN_SETTINGS_ROOT:
        UI_BottomBar_SetButton(UI_FKEY_1, "UP", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_2, "DN", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_3, "", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_4, "", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_5, "ENTER", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_6, "BACK", 0u);
        break;

    case MOTOR_SCREEN_SETTINGS_DISPLAY:
    case MOTOR_SCREEN_SETTINGS_GPS:
    case MOTOR_SCREEN_SETTINGS_UNITS:
    case MOTOR_SCREEN_SETTINGS_RECORDING:
    case MOTOR_SCREEN_SETTINGS_DYNAMICS:
    case MOTOR_SCREEN_SETTINGS_MAINTENANCE:
    case MOTOR_SCREEN_SETTINGS_OBD:
    case MOTOR_SCREEN_SETTINGS_SYSTEM:
        UI_BottomBar_SetButton(UI_FKEY_1, "UP", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_2, "DN", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_3, "-", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_4, "+", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_5, "EXEC", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_6, "BACK", 0u);
        break;

    case MOTOR_SCREEN_OBD_CONNECT_STUB:
        UI_BottomBar_SetButton(UI_FKEY_1, "", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_2, "", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_3, "", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_4, "", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_5, (state->vehicle.connected != false) ? "DISC" : "LINK", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_6, "BACK", 0u);
        break;

    default:
        UI_BottomBar_SetButton(UI_FKEY_1, "", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_2, "", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_3, "", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_4, "", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_5, "OK", UI_BOTTOMBAR_FLAG_DIVIDER);
        UI_BottomBar_SetButton(UI_FKEY_6, "BACK", 0u);
        break;
    }
}

void Motor_UI_ComputeViewport(u8g2_t *u8g2,
                              const motor_state_t *state,
                              ui_layout_mode_t layout_mode,
                              ui_rect_t *out_viewport)
{
    int16_t top_y;
    int16_t bottom_y;
    uint8_t reserved_h;

    (void)state;

    if ((u8g2 == 0) || (out_viewport == 0))
    {
        return;
    }

    reserved_h = UI_StatusBar_GetReservedHeight(u8g2);
    top_y = (int16_t)(reserved_h + UI_STATUSBAR_GAP_H);
    bottom_y = (int16_t)(UI_LCD_H - 1);

    if (layout_mode == UI_LAYOUT_MODE_TOP_BOTTOM_FIXED)
    {
        bottom_y = (int16_t)(UI_LCD_H - UI_BOTTOMBAR_H - UI_BOTTOMBAR_GAP_H - 1);
    }

    out_viewport->x = 0;
    out_viewport->y = top_y;
    out_viewport->w = UI_LCD_W;
    out_viewport->h = (int16_t)((bottom_y - top_y) + 1);
}

void Motor_UI_DrawLabeledValueBox(u8g2_t *u8g2,
                                  int16_t x,
                                  int16_t y,
                                  int16_t w,
                                  int16_t h,
                                  const char *label,
                                  const char *value,
                                  uint8_t large_font)
{
    if (u8g2 == 0)
    {
        return;
    }

    u8g2_DrawFrame(u8g2, x, y, w, h);
    u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
    if (label != 0)
    {
        u8g2_DrawStr(u8g2, x + 2, y + 7, label);
    }

    if (large_font != 0u)
    {
        u8g2_SetFont(u8g2, u8g2_font_9x15B_tr);
        if (value != 0)
        {
            u8g2_DrawStr(u8g2, x + 3, y + h - 4, value);
        }
    }
    else
    {
        u8g2_SetFont(u8g2, u8g2_font_6x10_tr);
        if (value != 0)
        {
            u8g2_DrawStr(u8g2, x + 2, y + h - 3, value);
        }
    }
}

void Motor_UI_DrawHorizontalBar(u8g2_t *u8g2,
                                int16_t x,
                                int16_t y,
                                int16_t w,
                                int16_t h,
                                int32_t value,
                                int32_t min_value,
                                int32_t max_value,
                                const char *caption)
{
    uint32_t fill_w;
    int32_t span;
    int32_t clamped;

    if (u8g2 == 0)
    {
        return;
    }

    span = max_value - min_value;
    if (span <= 0)
    {
        span = 1;
    }

    clamped = value;
    if (clamped < min_value) clamped = min_value;
    if (clamped > max_value) clamped = max_value;
    fill_w = (uint32_t)(((int64_t)(clamped - min_value) * (int64_t)(w - 2)) / (int64_t)span);

    u8g2_DrawFrame(u8g2, x, y, w, h);
    u8g2_DrawBox(u8g2, x + 1, y + 1, (uint8_t)fill_w, (uint8_t)(h - 2));
    if (caption != 0)
    {
        u8g2_SetFont(u8g2, u8g2_font_5x7_tr);
        u8g2_DrawStr(u8g2, x + 2, y - 2, caption);
    }
}

void Motor_UI_DrawCenteredTextBlock(u8g2_t *u8g2,
                                    const ui_rect_t *viewport,
                                    int16_t y,
                                    const char *title,
                                    const char *line1,
                                    const char *line2)
{
    int16_t base_y;

    if ((u8g2 == 0) || (viewport == 0))
    {
        return;
    }

    base_y = viewport->y + y;

    u8g2_SetFont(u8g2, u8g2_font_9x15B_tr);
    if (title != 0) u8g2_DrawStr(u8g2, viewport->x + 44, base_y, title);
    u8g2_SetFont(u8g2, u8g2_font_6x10_tr);
    if (line1 != 0) u8g2_DrawStr(u8g2, viewport->x + 18, base_y + 18, line1);
    if (line2 != 0) u8g2_DrawStr(u8g2, viewport->x + 18, base_y + 32, line2);
}

void Motor_UI_DrawViewportTitle(u8g2_t *u8g2, const ui_rect_t *viewport, const char *title)
{
    if ((u8g2 == 0) || (viewport == 0) || (title == 0))
    {
        return;
    }

    u8g2_SetFont(u8g2, u8g2_font_6x10_tr);
    u8g2_DrawStr(u8g2, viewport->x + 4, viewport->y + 10, title);
    u8g2_DrawHLine(u8g2, viewport->x + 4, viewport->y + 12, viewport->w - 8);
}
