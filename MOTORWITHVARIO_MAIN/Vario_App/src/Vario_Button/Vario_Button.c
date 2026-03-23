#include "Vario_Button.h"

#include "button.h"
#include "Vario_Settings.h"

#include <string.h>

static void vario_button_handle_main_screen(const button_event_t *event)
{
    if (event->type != BUTTON_EVENT_SHORT_PRESS)
    {
        return;
    }

    switch (event->id)
    {
        case BUTTON_ID_1:
            Vario_State_SetMode(VARIO_MODE_SCREEN_1);
            break;

        case BUTTON_ID_2:
            Vario_State_SelectPreviousMainScreen();
            break;

        case BUTTON_ID_3:
            Vario_State_SelectNextMainScreen();
            break;

        case BUTTON_ID_4:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_QNH, -1);
            Vario_State_RequestRedraw();
            break;

        case BUTTON_ID_5:
            Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_QNH, +1);
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

    if (event->type != BUTTON_EVENT_SHORT_PRESS)
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

                case VARIO_SETTING_MENU_DAMPING:
                    Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_VARIO_DAMPING, direction);
                    break;

                case VARIO_SETTING_MENU_ALT2:
                    Vario_Settings_AdjustQuickSet(VARIO_QUICKSET_ITEM_ALT2_MODE, direction);
                    break;

                case VARIO_SETTING_MENU_COUNT:
                default:
                    break;
            }

            Vario_State_RequestRedraw();
            break;
        }

        case BUTTON_ID_6:
            switch ((vario_setting_menu_item_t)cursor)
            {
                case VARIO_SETTING_MENU_BRIGHTNESS:
                    Vario_State_EnterValueSetting();
                    break;

                case VARIO_SETTING_MENU_VOLUME:
                case VARIO_SETTING_MENU_DAMPING:
                case VARIO_SETTING_MENU_ALT2:
                    Vario_State_EnterQuickSet();
                    break;

                case VARIO_SETTING_MENU_COUNT:
                default:
                    break;
            }
            break;

        default:
            break;
    }
}

static void vario_button_handle_quickset(const button_event_t *event)
{
    vario_quickset_item_t item;

    if (event->type != BUTTON_EVENT_SHORT_PRESS)
    {
        return;
    }

    item = (vario_quickset_item_t)Vario_State_GetQuickSetCursor();

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
            Vario_Settings_AdjustQuickSet(item, -1);
            Vario_State_RequestRedraw();
            break;

        case BUTTON_ID_5:
            Vario_Settings_AdjustQuickSet(item, +1);
            Vario_State_RequestRedraw();
            break;

        case BUTTON_ID_6:
            Vario_State_EnterValueSetting();
            break;

        default:
            break;
    }
}

static void vario_button_handle_valuesetting(const button_event_t *event)
{
    vario_value_item_t item;

    if (event->type != BUTTON_EVENT_SHORT_PRESS)
    {
        return;
    }

    item = (vario_value_item_t)Vario_State_GetValueSettingCursor();

    switch (event->id)
    {
        case BUTTON_ID_1:
            Vario_State_EnterSettings();
            break;

        case BUTTON_ID_2:
            Vario_State_MoveValueSettingCursor(-1);
            break;

        case BUTTON_ID_3:
            Vario_State_MoveValueSettingCursor(+1);
            break;

        case BUTTON_ID_4:
            Vario_Settings_AdjustValue(item, -1);
            Vario_State_RequestRedraw();
            break;

        case BUTTON_ID_5:
            Vario_Settings_AdjustValue(item, +1);
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

    (void)now_ms;

    while (Button_PopEvent(&event) != false)
    {
        switch (Vario_State_GetMode())
        {
            case VARIO_MODE_SCREEN_1:
            case VARIO_MODE_SCREEN_2:
            case VARIO_MODE_SCREEN_3:
                vario_button_handle_main_screen(&event);
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

    switch (mode)
    {
        case VARIO_MODE_SCREEN_1:
        case VARIO_MODE_SCREEN_2:
        case VARIO_MODE_SCREEN_3:
            out_bar->f1 = "SCR1";
            out_bar->f2 = "PREV";
            out_bar->f3 = "NEXT";
            out_bar->f4 = "QNH-";
            out_bar->f5 = "QNH+";
            out_bar->f6 = "SET";
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
            out_bar->f5 = "+";
            out_bar->f6 = "DISP";
            break;

        case VARIO_MODE_VALUESETTING:
            out_bar->f1 = "BACK";
            out_bar->f2 = "UP";
            out_bar->f3 = "DN";
            out_bar->f4 = "-";
            out_bar->f5 = "+";
            out_bar->f6 = "INST";
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
