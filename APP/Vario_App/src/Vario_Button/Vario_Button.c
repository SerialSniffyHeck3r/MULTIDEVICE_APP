#include "Vario_Button.h"

#include "button.h"
#include "Vario_Display_Common.h"
#include "Vario_Navigation.h"
#include "Vario_Settings.h"
#include "ui_confirm.h"
#include "ui_menu.h"
#include "ui_toast.h"

#include <stdio.h>
#include <string.h>

#ifndef VARIO_BUTTON_TRAIL_SCALE_COUNT
#define VARIO_BUTTON_TRAIL_SCALE_COUNT 5u
#endif

static const uint16_t s_trail_scale_presets_m[VARIO_BUTTON_TRAIL_SCALE_COUNT] = {
    100u, 250u, 500u, 750u, 1000u
};

static const ui_menu_item_t s_nav_menu_items[] = {
    { "HOME",             (uint16_t)VARIO_NAV_MENU_ACTION_HOME },
    { "LAUNCH",           (uint16_t)VARIO_NAV_MENU_ACTION_LAUNCH },
    { "NEARBY LANDABLE",  (uint16_t)VARIO_NAV_MENU_ACTION_NEARBY_LANDABLE },
    { "USER WAYPOINTS",   (uint16_t)VARIO_NAV_MENU_ACTION_USER_WAYPOINTS },
    { "MARK HERE",        (uint16_t)VARIO_NAV_MENU_ACTION_MARK_HERE },
    { "CLEAR TARGET",     (uint16_t)VARIO_NAV_MENU_ACTION_CLEAR_TARGET },
    { "SITE SETS",        (uint16_t)VARIO_NAV_MENU_ACTION_SITE_SETS }
};

static void vario_button_show_qnh_toast(uint32_t now_ms)
{
    char qnh_text[32];
    char toast_text[40];

    Vario_Settings_FormatQnhText(qnh_text, sizeof(qnh_text));
    snprintf(toast_text, sizeof(toast_text), "QNH %s", qnh_text);
    UI_Toast_Show(toast_text, NULL, 0u, 0u, now_ms, 1200u);
}

static void vario_button_show_attitude_hint_toast(uint32_t now_ms)
{
    UI_Toast_Show("HOLD TO RESET ATTITUDE", NULL, 0u, 0u, now_ms, 1200u);
}

static void vario_button_show_trail_mode_toast(uint32_t now_ms)
{
    if (Vario_Display_IsTrailHeadingUpMode() != false)
    {
        UI_Toast_Show("TRAIL UP", NULL, 0u, 0u, now_ms, 1200u);
    }
    else
    {
        UI_Toast_Show("NORTH UP", NULL, 0u, 0u, now_ms, 1200u);
    }
}

static void vario_button_show_target_source_toast(uint32_t now_ms)
{
    char toast_text[40];

    memset(toast_text, 0, sizeof(toast_text));
    Vario_Navigation_FormatActiveSourceToast(toast_text, sizeof(toast_text));
    UI_Toast_Show(toast_text, NULL, 0u, 0u, now_ms, 1200u);
}

static uint8_t vario_button_find_trail_scale_index(uint16_t range_m)
{
    uint8_t i;
    uint8_t best_index;
    uint16_t best_error;

    best_index = 0u;
    best_error = (uint16_t)((range_m > s_trail_scale_presets_m[0]) ?
                            (range_m - s_trail_scale_presets_m[0]) :
                            (s_trail_scale_presets_m[0] - range_m));

    for (i = 1u; i < VARIO_BUTTON_TRAIL_SCALE_COUNT; ++i)
    {
        uint16_t error;
        error = (uint16_t)((range_m > s_trail_scale_presets_m[i]) ?
                           (range_m - s_trail_scale_presets_m[i]) :
                           (s_trail_scale_presets_m[i] - range_m));
        if (error < best_error)
        {
            best_error = error;
            best_index = i;
        }
    }

    return best_index;
}

static void vario_button_apply_trail_scale_preset(uint8_t preset_index)
{
    const vario_settings_t *settings;
    int16_t target_m;
    int16_t current_m;
    int16_t steps;

    if (preset_index >= VARIO_BUTTON_TRAIL_SCALE_COUNT)
    {
        return;
    }

    settings = Vario_Settings_Get();
    if (settings == NULL)
    {
        return;
    }

    target_m = (int16_t)s_trail_scale_presets_m[preset_index];
    current_m = (int16_t)settings->trail_range_m;
    steps = (int16_t)((target_m - current_m) / 50);

    if (steps != 0)
    {
        Vario_Settings_AdjustValue(VARIO_VALUE_ITEM_TRAIL_RANGE, (int8_t)steps);
    }
}

static void vario_button_show_trail_scale_toast(uint8_t preset_index, uint32_t now_ms)
{
    char toast_text[40];

    if (preset_index >= VARIO_BUTTON_TRAIL_SCALE_COUNT)
    {
        preset_index = 0u;
    }

    snprintf(toast_text,
             sizeof(toast_text),
             "TRAIL SCALE %u/5 (%um)",
             (unsigned)(preset_index + 1u),
             (unsigned)s_trail_scale_presets_m[preset_index]);
    UI_Toast_Show(toast_text, NULL, 0u, 0u, now_ms, 1200u);
}

static void vario_button_cycle_trail_scale(uint32_t now_ms)
{
    const vario_settings_t *settings;
    uint8_t current_index;
    uint8_t next_index;

    settings = Vario_Settings_Get();
    if (settings == NULL)
    {
        return;
    }

    current_index = vario_button_find_trail_scale_index(settings->trail_range_m);
    next_index = (uint8_t)((current_index + 1u) % VARIO_BUTTON_TRAIL_SCALE_COUNT);
    vario_button_apply_trail_scale_preset(next_index);
    vario_button_show_trail_scale_toast(next_index, now_ms);
}

static void vario_button_open_nav_menu(void)
{
    UI_Menu_Show("NAV MENU",
                 s_nav_menu_items,
                 (uint8_t)(sizeof(s_nav_menu_items) / sizeof(s_nav_menu_items[0])),
                 0u);
}

static void vario_button_handle_confirm_overlay(const button_event_t *event, uint32_t now_ms)
{
    if (event == NULL)
    {
        return;
    }

    if (event->type != BUTTON_EVENT_LONG_PRESS)
    {
        return;
    }

    switch (event->id)
    {
        case BUTTON_ID_1:
        case BUTTON_ID_2:
        case BUTTON_ID_3:
            if (Vario_Navigation_HasConfirmHandler(UI_Confirm_GetContextId()) != false)
            {
                Vario_Navigation_HandleConfirmChoice(UI_Confirm_GetContextId(), event->id, now_ms);
            }
            UI_Confirm_Hide();
            Vario_State_RequestRedraw();
            break;

        default:
            break;
    }
}

static void vario_button_handle_menu_overlay(const button_event_t *event, uint32_t now_ms)
{
    uint16_t action_id;

    if ((event == NULL) || (event->type != BUTTON_EVENT_SHORT_PRESS))
    {
        return;
    }

    switch (event->id)
    {
        case BUTTON_ID_1:
            UI_Menu_Hide();
            Vario_State_RequestRedraw();
            break;

        case BUTTON_ID_2:
            UI_Menu_MoveSelection(-1);
            Vario_State_RequestRedraw();
            break;

        case BUTTON_ID_3:
            UI_Menu_MoveSelection(+1);
            Vario_State_RequestRedraw();
            break;

        case BUTTON_ID_6:
            action_id = UI_Menu_GetSelectedAction();
            UI_Menu_Hide();
            Vario_Navigation_HandleMenuAction(action_id, now_ms);
            Vario_State_RequestRedraw();
            break;

        default:
            break;
    }
}

static void vario_button_handle_nav_page(const button_event_t *event, uint32_t now_ms)
{
    if ((event == NULL) || (event->type != BUTTON_EVENT_SHORT_PRESS))
    {
        return;
    }

    if (Vario_Navigation_IsNameEditActive() != false)
    {
        switch (event->id)
        {
            case BUTTON_ID_1:
                Vario_Navigation_ClosePage();
                if (Vario_Navigation_IsPageOpen() == false)
                {
                    Vario_State_ReturnToMain();
                }
                Vario_State_RequestRedraw();
                break;

            case BUTTON_ID_2:
                Vario_Navigation_NameEdit_AdjustChar(-1);
                Vario_State_RequestRedraw();
                break;

            case BUTTON_ID_3:
                Vario_Navigation_NameEdit_AdjustChar(+1);
                Vario_State_RequestRedraw();
                break;

            case BUTTON_ID_4:
                Vario_Navigation_NameEdit_MoveCursor(-1);
                Vario_State_RequestRedraw();
                break;

            case BUTTON_ID_5:
                Vario_Navigation_NameEdit_MoveCursor(+1);
                Vario_State_RequestRedraw();
                break;

            case BUTTON_ID_6:
                Vario_Navigation_NameEdit_Save(now_ms);
                Vario_State_RequestRedraw();
                break;

            default:
                break;
        }
        return;
    }

    switch (event->id)
    {
        case BUTTON_ID_1:
            Vario_Navigation_ClosePage();
            if (Vario_Navigation_IsPageOpen() == false)
            {
                Vario_State_ReturnToMain();
            }
            Vario_State_RequestRedraw();
            break;

        case BUTTON_ID_2:
            Vario_Navigation_MoveCursor(-1);
            Vario_State_RequestRedraw();
            break;

        case BUTTON_ID_3:
            Vario_Navigation_MoveCursor(+1);
            Vario_State_RequestRedraw();
            break;

        case BUTTON_ID_6:
            Vario_Navigation_ActivateSelected(now_ms);
            Vario_State_RequestRedraw();
            break;

        default:
            break;
    }
}

static void vario_button_handle_main_screen(const button_event_t *event, uint32_t now_ms)
{
    bool trainer_enabled;
    vario_mode_t mode;

    if (event == NULL)
    {
        return;
    }

    trainer_enabled = (Vario_Settings_Get()->trainer_enabled != 0u) ? true : false;
    mode = Vario_State_GetMode();

    if (trainer_enabled != false)
    {
        if ((event->id == BUTTON_ID_6) && (event->type == BUTTON_EVENT_LONG_PRESS))
        {
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_TRAINER, -1);
            Vario_State_RequestRedraw();
            return;
        }

        if (event->type != BUTTON_EVENT_SHORT_PRESS)
        {
            return;
        }

        switch (event->id)
        {
            case BUTTON_ID_1:
                Vario_State_TrainerAdjustSpeed(+1);
                Vario_State_RequestRedraw();
                break;

            case BUTTON_ID_2:
                Vario_State_TrainerAdjustSpeed(-1);
                Vario_State_RequestRedraw();
                break;

            case BUTTON_ID_3:
                Vario_State_TrainerAdjustAltitude(+1);
                Vario_State_RequestRedraw();
                break;

            case BUTTON_ID_4:
                Vario_State_TrainerAdjustAltitude(-1);
                Vario_State_RequestRedraw();
                break;

            case BUTTON_ID_5:
                Vario_State_TrainerAdjustHeading(+1);
                Vario_State_RequestRedraw();
                break;

            case BUTTON_ID_6:
                Vario_State_TrainerAdjustHeading(-1);
                Vario_State_RequestRedraw();
                break;

            default:
                break;
        }

        return;
    }

    if ((event->id == BUTTON_ID_1) && (event->type == BUTTON_EVENT_SHORT_PRESS))
    {
        Vario_State_SelectNextMainScreen();
        return;
    }

    if (event->id == BUTTON_ID_2)
    {
        if (event->type == BUTTON_EVENT_SHORT_PRESS)
        {
            if (mode == VARIO_MODE_SCREEN_3)
            {
                vario_button_show_attitude_hint_toast(now_ms);
                return;
            }

            if (mode == VARIO_MODE_SCREEN_2)
            {
                vario_button_cycle_trail_scale(now_ms);
                Vario_State_RequestRedraw();
                return;
            }

            return;
        }

        if (event->type == BUTTON_EVENT_LONG_PRESS)
        {
            if (mode == VARIO_MODE_SCREEN_3)
            {
                Vario_State_ResetAttitudeIndicator();
                Vario_State_RequestRedraw();
                return;
            }

            if (mode == VARIO_MODE_SCREEN_2)
            {
                Vario_Display_ToggleTrailHeadingUpMode();
                vario_button_show_trail_mode_toast(now_ms);
                Vario_State_RequestRedraw();
                return;
            }
        }

        return;
    }

    if (event->id == BUTTON_ID_3)
    {
        if (event->type == BUTTON_EVENT_SHORT_PRESS)
        {
            Vario_Navigation_CycleTargetSource();
            vario_button_show_target_source_toast(now_ms);
            Vario_State_RequestRedraw();
            return;
        }

        if (event->type == BUTTON_EVENT_LONG_PRESS)
        {
            vario_button_open_nav_menu();
            Vario_State_RequestRedraw();
            return;
        }

        return;
    }

    if (event->type != BUTTON_EVENT_SHORT_PRESS)
    {
        return;
    }

    switch (event->id)
    {
        case BUTTON_ID_4:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_QNH, -1);
            vario_button_show_qnh_toast(now_ms);
            Vario_State_RequestRedraw();
            break;

        case BUTTON_ID_5:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_QNH, +1);
            vario_button_show_qnh_toast(now_ms);
            Vario_State_RequestRedraw();
            break;

        case BUTTON_ID_6:
            Vario_State_EnterSettings();
            break;

        default:
            break;
    }
}

static void vario_button_handle_setting(const button_event_t *event)
{
    uint8_t cursor;

    if ((event == NULL) || (event->type != BUTTON_EVENT_SHORT_PRESS))
    {
        return;
    }

    cursor = Vario_State_GetSettingsCursor();

    switch (event->id)
    {
        case BUTTON_ID_1:
            Vario_State_ReturnToMain();
            break;

        case BUTTON_ID_2:
            Vario_State_MoveSettingsCursor(-1);
            break;

        case BUTTON_ID_3:
            Vario_State_MoveSettingsCursor(+1);
            break;

        case BUTTON_ID_4:
        case BUTTON_ID_5:
        {
            int8_t direction;
            direction = (event->id == BUTTON_ID_4) ? -1 : +1;

            switch ((vario_setting_menu_item_t)cursor)
            {
                case VARIO_SETTING_MENU_BRIGHTNESS:
                    Vario_Settings_AdjustValue(VARIO_VALUE_ITEM_BRIGHTNESS, direction);
                    break;

                case VARIO_SETTING_MENU_VOLUME:
                    Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_AUDIO_VOLUME, direction);
                    break;

                case VARIO_SETTING_MENU_RESPONSE:
                    Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_VARIO_DAMPING, direction);
                    break;

                case VARIO_SETTING_MENU_TRAINER:
                    Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_TRAINER, direction);
                    break;

                case VARIO_SETTING_MENU_CLIMB_START:
                    Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_CLIMB_TONE_THRESHOLD, direction);
                    break;

                case VARIO_SETTING_MENU_COUNT:
                default:
                    break;
            }

            Vario_State_RequestRedraw();
            break;
        }

        case BUTTON_ID_6:
        {
            vario_settings_category_t category;

            switch ((vario_setting_menu_item_t)cursor)
            {
                case VARIO_SETTING_MENU_BRIGHTNESS:
                    category = VARIO_SETTINGS_CATEGORY_DISPLAY;
                    break;

                case VARIO_SETTING_MENU_VOLUME:
                    category = VARIO_SETTINGS_CATEGORY_AUDIO;
                    break;

                case VARIO_SETTING_MENU_RESPONSE:
                    category = VARIO_SETTINGS_CATEGORY_AUDIO;
                    break;

                case VARIO_SETTING_MENU_TRAINER:
                    category = VARIO_SETTINGS_CATEGORY_FLIGHT;
                    break;

                case VARIO_SETTING_MENU_CLIMB_START:
                    category = VARIO_SETTINGS_CATEGORY_AUDIO;
                    break;

                case VARIO_SETTING_MENU_COUNT:
                default:
                    category = VARIO_SETTINGS_CATEGORY_SYSTEM;
                    break;
            }

            Vario_State_SetSettingsCategory(category);
            Vario_State_EnterQuickSet();
            break;
        }

        default:
            break;
    }
}

static void vario_button_handle_quickset(const button_event_t *event)
{
    if ((event == NULL) || (event->type != BUTTON_EVENT_SHORT_PRESS))
    {
        return;
    }

    switch (event->id)
    {
        case BUTTON_ID_1:
            Vario_State_EnterSettings();
            break;

        case BUTTON_ID_2:
            Vario_State_MoveQuickSetCursor(-1);
            break;

        case BUTTON_ID_3:
            Vario_State_MoveQuickSetCursor(+1);
            break;

        case BUTTON_ID_4:
        case BUTTON_ID_5:
            break;

        case BUTTON_ID_6:
            Vario_State_SetSettingsCategory((vario_settings_category_t)Vario_State_GetQuickSetCursor());
            Vario_State_EnterValueSetting();
            break;

        default:
            break;
    }
}

static void vario_button_handle_valuesetting(const button_event_t *event)
{
    uint8_t index;
    vario_settings_category_t category;

    if ((event == NULL) || (event->type != BUTTON_EVENT_SHORT_PRESS))
    {
        return;
    }

    category = Vario_State_GetSettingsCategory();
    index = Vario_State_GetValueSettingCursor();

    switch (event->id)
    {
        case BUTTON_ID_1:
            Vario_State_EnterQuickSet();
            break;

        case BUTTON_ID_2:
            Vario_State_MoveValueSettingCursor(-1);
            break;

        case BUTTON_ID_3:
            Vario_State_MoveValueSettingCursor(+1);
            break;

        case BUTTON_ID_4:
            Vario_Settings_AdjustCategoryItem(category, index, -1);
            Vario_State_RequestRedraw();
            break;

        case BUTTON_ID_5:
            Vario_Settings_AdjustCategoryItem(category, index, +1);
            Vario_State_RequestRedraw();
            break;

        case BUTTON_ID_6:
            Vario_State_EnterQuickSet();
            break;

        default:
            break;
    }
}

void Vario_Button_Init(void)
{
    /* 현재 구조에서는 별도 런타임 버퍼가 없다. */
}

void Vario_Button_Task(uint32_t now_ms)
{
    button_event_t event;

    while (Button_PopEvent(&event) != false)
    {
        if (UI_Confirm_IsVisible() != false)
        {
            vario_button_handle_confirm_overlay(&event, now_ms);
            continue;
        }

        if (UI_Menu_IsVisible() != false)
        {
            vario_button_handle_menu_overlay(&event, now_ms);
            continue;
        }

        if ((Vario_State_GetMode() == VARIO_MODE_SETTING) &&
            (Vario_Navigation_IsPageOpen() != false))
        {
            vario_button_handle_nav_page(&event, now_ms);
            continue;
        }

        switch (Vario_State_GetMode())
        {
            case VARIO_MODE_SCREEN_1:
            case VARIO_MODE_SCREEN_2:
            case VARIO_MODE_SCREEN_3:
                vario_button_handle_main_screen(&event, now_ms);
                break;

            case VARIO_MODE_SETTING:
                vario_button_handle_setting(&event);
                break;

            case VARIO_MODE_QUICKSET:
                vario_button_handle_quickset(&event);
                break;

            case VARIO_MODE_VALUESETTING:
                vario_button_handle_valuesetting(&event);
                break;

            case VARIO_MODE_COUNT:
            default:
                break;
        }
    }
}

void Vario_Button_GetButtonBar(vario_mode_t mode, vario_buttonbar_t *out_bar)
{
    if (out_bar == 0)
    {
        return;
    }

    memset(out_bar, 0, sizeof(*out_bar));

    if (UI_Confirm_IsVisible() != false)
    {
        switch ((vario_nav_confirm_context_t)UI_Confirm_GetContextId())
        {
            case VARIO_NAV_CONFIRM_LANDING_SAVE:
                out_bar->f1 = "NO";
                out_bar->f2 = "FIELD";
                out_bar->f3 = "HOME";
                out_bar->f4 = "-";
                out_bar->f5 = "-";
                out_bar->f6 = "HOLD";
                break;

            case VARIO_NAV_CONFIRM_CLEAR_SITE:
                out_bar->f1 = "BACK";
                out_bar->f2 = "CLEAR";
                out_bar->f3 = "CANCEL";
                out_bar->f4 = "-";
                out_bar->f5 = "-";
                out_bar->f6 = "HOLD";
                break;

            case VARIO_NAV_CONFIRM_NONE:
            default:
                out_bar->f1 = "OPT1";
                out_bar->f2 = "OPT2";
                out_bar->f3 = "OPT3";
                out_bar->f4 = "-";
                out_bar->f5 = "-";
                out_bar->f6 = "HOLD";
                break;
        }
        return;
    }

    if (UI_Menu_IsVisible() != false)
    {
        out_bar->f1 = "BACK";
        out_bar->f2 = "UP";
        out_bar->f3 = "DN";
        out_bar->f4 = "-";
        out_bar->f5 = "-";
        out_bar->f6 = "ENTER";
        return;
    }

    if ((mode == VARIO_MODE_SETTING) && (Vario_Navigation_IsPageOpen() != false))
    {
        if (Vario_Navigation_IsNameEditActive() != false)
        {
            out_bar->f1 = "BACK";
            out_bar->f2 = "CH-";
            out_bar->f3 = "CH+";
            out_bar->f4 = "LEFT";
            out_bar->f5 = "RIGHT";
            out_bar->f6 = "SAVE";
        }
        else
        {
            out_bar->f1 = "BACK";
            out_bar->f2 = "UP";
            out_bar->f3 = "DN";
            out_bar->f4 = "-";
            out_bar->f5 = "-";
            out_bar->f6 = "ENTER";
        }
        return;
    }

    switch (mode)
    {
        case VARIO_MODE_SCREEN_1:
        case VARIO_MODE_SCREEN_2:
        case VARIO_MODE_SCREEN_3:
            if (Vario_Settings_Get()->trainer_enabled != 0u)
            {
                out_bar->f1 = "SPD+";
                out_bar->f2 = "SPD-";
                out_bar->f3 = "ALT+";
                out_bar->f4 = "ALT-";
                out_bar->f5 = "HDG+";
                out_bar->f6 = "HDG-";
            }
            else
            {
                out_bar->f1 = "VIEW";
                out_bar->f2 = "ACT";
                out_bar->f3 = "NAV";
                out_bar->f4 = "QNH-";
                out_bar->f5 = "QNH+";
                out_bar->f6 = "SET";
            }
            break;

        case VARIO_MODE_SETTING:
            out_bar->f1 = "BACK";
            out_bar->f2 = "UP";
            out_bar->f3 = "DN";
            out_bar->f4 = "-";
            out_bar->f5 = "+";
            out_bar->f6 = "OPEN";
            break;

        case VARIO_MODE_QUICKSET:
            out_bar->f1 = "BACK";
            out_bar->f2 = "UP";
            out_bar->f3 = "DN";
            out_bar->f4 = "-";
            out_bar->f5 = "-";
            out_bar->f6 = "OPEN";
            break;

        case VARIO_MODE_VALUESETTING:
            out_bar->f1 = "BACK";
            out_bar->f2 = "UP";
            out_bar->f3 = "DN";
            out_bar->f4 = "-";
            out_bar->f5 = "+";
            out_bar->f6 = "CATS";
            break;

        case VARIO_MODE_COUNT:
        default:
            out_bar->f1 = "-";
            out_bar->f2 = "-";
            out_bar->f3 = "-";
            out_bar->f4 = "-";
            out_bar->f5 = "-";
            out_bar->f6 = "-";
            break;
    }
}
