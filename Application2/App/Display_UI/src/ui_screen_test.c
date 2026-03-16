#include "ui_screen_test.h"

#include "ui_bottombar.h"
#include "ui_statusbar.h"
#include "ui_toast.h"
#include "ui_popup.h"
#include "ui_common_icons.h"
#include "LED_App.h"
#include "APP_STATE.h"
#include "BACKLIGHT_App.h"
#include "BACKLIGHT_DRIVER.h"
#include "u8g2_uc1608_stm32.h"

#include <stdio.h>

/* -------------------------------------------------------------------------- */
/*  Screen-local runtime state                                                */
/*                                                                            */
/*  이 파일은 TEST 홈 화면의 "기능 로직" 을 전담한다.                          */
/*  즉, 아래 상태들은 UI 엔진(ui_engine.c)이 아니라                             */
/*  TEST 화면 정의 파일인 여기에서만 관리한다.                                 */
/*                                                                            */
/*  보관하는 항목                                                              */
/*  - 현재 main viewport 모드(layout mode)                                     */
/*  - RUN / HOLD 상태                                                          */
/*  - cute icon index                                                          */
/*  - 20Hz live counter                                                        */
/*  - overlay형 bottom bar의 표시 만료 시각                                    */
/* -------------------------------------------------------------------------- */
static ui_layout_mode_t s_test_layout_mode = UI_LAYOUT_MODE_TOP_BOTTOM_FIXED;
static uint8_t          s_test_updates_paused = 0u;
static uint8_t          s_test_cute_icon_index = 0u;
static bool             s_test_blink_phase_on = true;
static uint32_t         s_test_live_counter_20hz = 0u;
static uint32_t         s_test_last_processed_tick_20hz = 0u;
static uint32_t         s_test_bottom_overlay_until_ms = 0u;

/* -------------------------------------------------------------------------- */
/*  Local helpers for TEST home logic                                         */
/* -------------------------------------------------------------------------- */
static void ui_screen_test_request_overlay(uint32_t now_ms);
static void ui_screen_test_configure_bottom_bar(void);
static const uint8_t *ui_screen_test_get_cute_icon(uint8_t index);

/* -------------------------------------------------------------------------- */
/*  TEST setting sub-mode state                                               */
/* -------------------------------------------------------------------------- */
typedef enum
{
  UI_TEST_SETTINGS_CATEGORY_BACKLIGHT = 0u,
  UI_TEST_SETTINGS_CATEGORY_UC1608    = 1u
} ui_test_settings_category_t;

typedef enum
{
  UI_TEST_SETTINGS_ITEM_BL_MODE = 0u,
  UI_TEST_SETTINGS_ITEM_BL_CONT_BIAS,
  UI_TEST_SETTINGS_ITEM_BL_SMOOTHNESS,
  UI_TEST_SETTINGS_ITEM_BL_NIGHT_THRESHOLD,
  UI_TEST_SETTINGS_ITEM_BL_SUPER_THRESHOLD,
  UI_TEST_SETTINGS_ITEM_BL_NIGHT_LEVEL,
  UI_TEST_SETTINGS_ITEM_BL_SUPER_LEVEL,
  UI_TEST_SETTINGS_ITEM_DISP_CONTRAST,
  UI_TEST_SETTINGS_ITEM_DISP_TEMP_COMP,
  UI_TEST_SETTINGS_ITEM_DISP_BIAS,
  UI_TEST_SETTINGS_ITEM_DISP_RAM_ACCESS,
  UI_TEST_SETTINGS_ITEM_DISP_START_LINE,
  UI_TEST_SETTINGS_ITEM_DISP_FIXED_LINE,
  UI_TEST_SETTINGS_ITEM_DISP_POWER_CTRL,
  UI_TEST_SETTINGS_ITEM_DISP_FLIP_MODE,
  UI_TEST_SETTINGS_ITEM_COUNT
} ui_test_settings_item_t;

typedef struct
{
  uint8_t     category;
  const char *title;
  const char *subtitle;
} ui_test_settings_item_meta_t;

static uint8_t       s_test_settings_active = 0u;
static uint8_t       s_test_settings_item_index = 0u;
static app_settings_t s_test_settings_original;
static app_settings_t s_test_settings_draft;

static const ui_test_settings_item_meta_t s_test_settings_meta[UI_TEST_SETTINGS_ITEM_COUNT] =
{
  { UI_TEST_SETTINGS_CATEGORY_BACKLIGHT, "AUTO MODE",       "AUTO-CONT / AUTO-DIMMER policy selector" },
  { UI_TEST_SETTINGS_CATEGORY_BACKLIGHT, "CONT BIAS",       "Continuous mode brightness offset -2..+2" },
  { UI_TEST_SETTINGS_CATEGORY_BACKLIGHT, "SMOOTHNESS",      "Higher value follows ambient more slowly" },
  { UI_TEST_SETTINGS_CATEGORY_BACKLIGHT, "NIGHT THRESHOLD", "DAY -> NIGHT boundary in ambient percent" },
  { UI_TEST_SETTINGS_CATEGORY_BACKLIGHT, "SUPER THRESHOLD", "NIGHT -> SUPER boundary in ambient percent" },
  { UI_TEST_SETTINGS_CATEGORY_BACKLIGHT, "NIGHT LEVEL",     "AUTO-DIMMER night output brightness" },
  { UI_TEST_SETTINGS_CATEGORY_BACKLIGHT, "SUPER LEVEL",     "AUTO-DIMMER super night brightness" },
  { UI_TEST_SETTINGS_CATEGORY_UC1608,    "CONTRAST",        "UC1608 command 0x81 raw contrast value" },
  { UI_TEST_SETTINGS_CATEGORY_UC1608,    "TEMP COMP",       "UC1608 temperature compensation bits" },
  { UI_TEST_SETTINGS_CATEGORY_UC1608,    "BIAS RATIO",      "UC1608 LCD bias ratio bits" },
  { UI_TEST_SETTINGS_CATEGORY_UC1608,    "RAM ACCESS",      "UC1608 RAM access mode bits" },
  { UI_TEST_SETTINGS_CATEGORY_UC1608,    "START LINE",      "UC1608 display start line raw value" },
  { UI_TEST_SETTINGS_CATEGORY_UC1608,    "FIXED LINE",      "UC1608 fixed line raw value" },
  { UI_TEST_SETTINGS_CATEGORY_UC1608,    "POWER CTRL",      "UC1608 power control raw bits" },
  { UI_TEST_SETTINGS_CATEGORY_UC1608,    "FLIP MODE",       "U8G2 / UC1608 panel flip selection" }
};

static void ui_screen_test_configure_settings_bottom_bar(void);
static ui_screen_test_action_t ui_screen_test_handle_settings_button_event(const button_event_t *event,
                                                                           uint32_t now_ms);
static void ui_screen_test_settings_enter(uint32_t now_ms);
static void ui_screen_test_settings_cancel(uint32_t now_ms);
static void ui_screen_test_settings_save(uint32_t now_ms);
static void ui_screen_test_settings_apply_preview(const app_settings_t *settings);
static void ui_screen_test_settings_normalize(app_settings_t *settings);
static void ui_screen_test_settings_adjust_current_item(int8_t direction,
                                                        bool fast_step,
                                                        uint32_t now_ms);
static void ui_screen_test_settings_get_value_text(uint8_t item_index,
                                                   const app_settings_t *settings,
                                                   char *out_text,
                                                   size_t out_size);
static uint8_t ui_screen_test_settings_get_value_bar_percent(uint8_t item_index,
                                                             const app_settings_t *settings);
static const char *ui_screen_test_settings_category_text(uint8_t category);
static const char *ui_screen_test_backlight_mode_text(uint8_t mode_raw);
static const char *ui_screen_test_dimmer_zone_text(uint8_t zone_raw);
static void ui_screen_test_draw_settings_screen(u8g2_t *u8g2,
                                                const ui_rect_t *viewport);

/* -------------------------------------------------------------------------- */
/*  Overlay request                                                           */
/*                                                                            */
/*  overlay mode에서는 버튼을 누르거나 event가 들어왔을 때만                    */
/*  bottom bar가 잠깐 올라오도록 한다.                                         */
/*  이 함수는 그 만료 시각을 갱신하는 역할만 한다.                              */
/* -------------------------------------------------------------------------- */
static void ui_screen_test_request_overlay(uint32_t now_ms)
{
  s_test_bottom_overlay_until_ms = now_ms + UI_BOTTOMBAR_OVERLAY_TIMEOUT_MS;
}

/* -------------------------------------------------------------------------- */
/*  TEST home bottom bar                                                      */
/*                                                                            */
/*  이 하단바는 TEST 홈 화면 전용 기능 힌트다.                                  */
/*  - F1 : DBG                                                                 */
/*  - F2 : PAUSE / RUN                                                         */
/*  - F3 : MODE                                                                */
/*  - F4 : TOAST                                                               */
/*  - F5 : POPUP                                                               */
/*  - F6 : 오른쪽 화살표 아이콘                                                */
/* -------------------------------------------------------------------------- */
static void ui_screen_test_configure_bottom_bar(void)
{
  UI_BottomBar_SetMode(UI_BOTTOMBAR_MODE_BUTTONS);

  UI_BottomBar_SetButton(UI_FKEY_1, "DBG", UI_BOTTOMBAR_FLAG_DIVIDER);
  UI_BottomBar_SetButton(UI_FKEY_2,
                         (s_test_updates_paused != 0u) ? "RUN" : "PAUSE",
                         UI_BOTTOMBAR_FLAG_DIVIDER);
  UI_BottomBar_SetButton(UI_FKEY_3, "MODE", UI_BOTTOMBAR_FLAG_DIVIDER);
  UI_BottomBar_SetButton(UI_FKEY_4, "TOAST", UI_BOTTOMBAR_FLAG_DIVIDER);
  UI_BottomBar_SetButton(UI_FKEY_5, "POPUP", UI_BOTTOMBAR_FLAG_DIVIDER);
  UI_BottomBar_SetButtonIcon4(UI_FKEY_6,
                              icon_arrow_right_7x4,
                              ICON7X4_W,
                              0u);
}

/* -------------------------------------------------------------------------- */
/*  TEST setting bottom bar                                                   */
/*                                                                            */
/*  요구사항 매핑                                                              */
/*  - F1 : back / cancel                                                      */
/*  - F2 : previous item                                                      */
/*  - F3 : next item                                                          */
/*  - F4 : value decrement                                                    */
/*  - F5 : value increment                                                    */
/*  - F6 : save / confirm                                                     */
/* -------------------------------------------------------------------------- */
static void ui_screen_test_configure_settings_bottom_bar(void)
{
  UI_BottomBar_SetMode(UI_BOTTOMBAR_MODE_BUTTONS);

  UI_BottomBar_SetButton(UI_FKEY_1, "BACK", UI_BOTTOMBAR_FLAG_DIVIDER);
  UI_BottomBar_SetButton(UI_FKEY_2, "<", UI_BOTTOMBAR_FLAG_DIVIDER);
  UI_BottomBar_SetButton(UI_FKEY_3, ">", UI_BOTTOMBAR_FLAG_DIVIDER);
  UI_BottomBar_SetButton(UI_FKEY_4, "-", UI_BOTTOMBAR_FLAG_DIVIDER);
  UI_BottomBar_SetButton(UI_FKEY_5, "+", UI_BOTTOMBAR_FLAG_DIVIDER);
  UI_BottomBar_SetButton(UI_FKEY_6, "SAVE", 0u);
}

/* -------------------------------------------------------------------------- */
/*  Public init                                                               */
/* -------------------------------------------------------------------------- */
void UI_ScreenTest_Init(uint32_t ui_tick_20hz)
{
  s_test_layout_mode = UI_LAYOUT_MODE_TOP_BOTTOM_FIXED;
  s_test_updates_paused = 0u;
  s_test_cute_icon_index = 0u;
  s_test_blink_phase_on = true;
  s_test_live_counter_20hz = 0u;
  s_test_last_processed_tick_20hz = ui_tick_20hz;
  s_test_bottom_overlay_until_ms = 0u;
  s_test_settings_active = 0u;
  s_test_settings_item_index = 0u;
  memset(&s_test_settings_original, 0, sizeof(s_test_settings_original));
  memset(&s_test_settings_draft, 0, sizeof(s_test_settings_draft));

  ui_screen_test_configure_bottom_bar();
}

/* -------------------------------------------------------------------------- */
/*  Public on-enter                                                           */
/*                                                                            */
/*  TEST 홈으로 돌아올 때마다 현재 상태에 맞는 하단바만 다시 세팅한다.          */
/*  숫자 카운터, layout mode, cute icon index는 유지한다.                      */
/* -------------------------------------------------------------------------- */
void UI_ScreenTest_OnEnter(void)
{
  s_test_settings_active = 0u;
  s_test_settings_item_index = 0u;
  ui_screen_test_configure_bottom_bar();
}

/* -------------------------------------------------------------------------- */
/*  Public 20Hz task                                                          */
/*                                                                            */
/*  20Hz raw tick는 큰 숫자 증가용으로만 사용한다.                              */
/*  눈으로 보이는 깜빡임 위상은 SlowToggle2Hz의 현재 값만 반영한다.             */
/* -------------------------------------------------------------------------- */
void UI_ScreenTest_Task(uint32_t ui_tick_20hz)
{
  uint32_t delta;

  delta = (ui_tick_20hz - s_test_last_processed_tick_20hz);
  if (delta == 0u)
  {
    return;
  }

  s_test_last_processed_tick_20hz = ui_tick_20hz;

  if (s_test_updates_paused == 0u)
  {
    s_test_live_counter_20hz += delta;
    s_test_blink_phase_on = (SlowToggle2Hz != false);
  }
}

/* -------------------------------------------------------------------------- */
/*  Public button handler                                                     */
/*                                                                            */
/*  화면 전환이 필요한 경우에만 action을 반환한다.                              */
/*  toast / popup / layout / bottom bar text 변경은 이 함수 안에서 끝낸다.     */
/* -------------------------------------------------------------------------- */
ui_screen_test_action_t UI_ScreenTest_HandleButtonEvent(const button_event_t *event,
                                                        uint32_t now_ms)
{
  if (event == 0)
  {
    return UI_SCREEN_TEST_ACTION_NONE;
  }

  /* ---------------------------------------------------------------------- */
  /* settings sub-mode가 active이면 여기서 버튼 의미를 완전히 바꿔 잡는다.    */
  /*                                                                        */
  /* 홈 화면과 설정 화면이 같은 root screen을 공유하더라도                    */
  /* F-key 의미가 서로 섞이지 않도록 가장 먼저 분기한다.                      */
  /* ---------------------------------------------------------------------- */
  if (s_test_settings_active != 0u)
  {
    return ui_screen_test_handle_settings_button_event(event, now_ms);
  }

  /* ---------------------------------------------------------------------- */
  /* overlay mode에서 들어오는 모든 버튼 event는 하단 overlay 표시를 연장한다. */
  /* ---------------------------------------------------------------------- */
  if (s_test_layout_mode == UI_LAYOUT_MODE_TOP_EXTENDED_OVERLAY)
  {
    switch (event->type)
    {
      case BUTTON_EVENT_PRESS:
      case BUTTON_EVENT_RELEASE:
      case BUTTON_EVENT_SHORT_PRESS:
      case BUTTON_EVENT_LONG_PRESS:
        ui_screen_test_request_overlay(now_ms);
        break;

      case BUTTON_EVENT_NONE:
      default:
        break;
    }
  }

  /* ---------------------------------------------------------------------- */
   /* TEST 홈 F1 long press -> 엔진 오일 교체 주기 설정 화면 진입               */
   /*                                                                        */
   /* F1 short press의 기존 debug 진입 기능은 그대로 유지해야 하므로,          */
   /* long press만 별도 action으로 분리해 UI 엔진으로 올린다.                  */
   /* ---------------------------------------------------------------------- */
   if ((event->id == BUTTON_ID_1) &&
       (event->type == BUTTON_EVENT_LONG_PRESS))
   {
     return UI_SCREEN_TEST_ACTION_ENTER_ENGINE_OIL;
   }

   /* ---------------------------------------------------------------------- */
   /* TEST 홈 F2 long press -> LED test pattern advance                       */
   /* ---------------------------------------------------------------------- */
   if ((event->id == BUTTON_ID_2) &&
       (event->type == BUTTON_EVENT_LONG_PRESS))
   {
     const char *led_mode_text;

     led_mode_text = LED_App_AdvanceTestPattern();

     UI_Toast_Show(led_mode_text,
                   icon_ui_info_8x8,
                   ICON8_W,
                   ICON8_H,
                   now_ms,
                   1100u);

     return UI_SCREEN_TEST_ACTION_NONE;
   }

   /* ---------------------------------------------------------------------- */
   /* TEST 홈 F3 long press -> SCREEN TEST 설정창 진입                        */
   /*                                                                        */
   /* 요구사항                                                                */
   /* - 진입 즉시 TOP+BOT 고정처럼 동작                                       */
   /* - viewport 안에 설정창을 꽉 맞춰 렌더                                   */
   /* ---------------------------------------------------------------------- */
   if ((event->id == BUTTON_ID_3) &&
       (event->type == BUTTON_EVENT_LONG_PRESS))
   {
     ui_screen_test_settings_enter(now_ms);
     return UI_SCREEN_TEST_ACTION_NONE;
   }

   /* ---------------------------------------------------------------------- */
   /* TEST 홈 F6 long press -> GPS 화면 진입                                  */
   /*                                                                        */
   /* F6 short press의 cute icon cycle은 그대로 유지하고,                    */
   /* long press만 새 GPS 화면 진입 action으로 분리한다.                     */
   /* ---------------------------------------------------------------------- */
   if ((event->id == BUTTON_ID_6) &&
       (event->type == BUTTON_EVENT_LONG_PRESS))
   {
     return UI_SCREEN_TEST_ACTION_ENTER_GPS;
   }

   if (event->type != BUTTON_EVENT_SHORT_PRESS)
   {
     return UI_SCREEN_TEST_ACTION_NONE;
   }


  switch (event->id)
  {
    case BUTTON_ID_1:
      return UI_SCREEN_TEST_ACTION_ENTER_DEBUG_LEGACY;

    case BUTTON_ID_2:
      s_test_updates_paused = (s_test_updates_paused == 0u) ? 1u : 0u;
      if (s_test_updates_paused != 0u)
      {
        s_test_blink_phase_on = (SlowToggle2Hz != false);
      }

      ui_screen_test_configure_bottom_bar();

      UI_Toast_Show((s_test_updates_paused != 0u) ? "TEST HOLD" : "TEST RUN",
                    icon_ui_bell_8x8,
                    ICON8_W,
                    ICON8_H,
                    now_ms,
                    900u);
      break;

    case BUTTON_ID_3:
      s_test_layout_mode = (ui_layout_mode_t)(((uint32_t)s_test_layout_mode + 1u) %
                                              (uint32_t)UI_LAYOUT_MODE_COUNT);
      if (s_test_layout_mode == UI_LAYOUT_MODE_TOP_EXTENDED_OVERLAY)
      {
        ui_screen_test_request_overlay(now_ms);
      }

      UI_Toast_Show("LAYOUT CHANGED",
                    icon_ui_folder_8x8,
                    ICON8_W,
                    ICON8_H,
                    now_ms,
                    900u);
      break;

    case BUTTON_ID_4:
      UI_Toast_Show("TOAST WITH ICON",
                    icon_ui_info_8x8,
                    ICON8_W,
                    ICON8_H,
                    now_ms,
                    UI_TOAST_DEFAULT_TIMEOUT_MS);
      break;

    case BUTTON_ID_5:
      UI_Popup_Show("POPUP",
                    "OPAQUE BODY ENABLED",
                    "RIGHT TEXT ALIGN FIXED",
                    icon_ui_warn_8x8,
                    ICON8_W,
                    ICON8_H,
                    now_ms,
                    UI_POPUP_DEFAULT_TIMEOUT_MS);
      break;

    case BUTTON_ID_6:
      s_test_cute_icon_index = (uint8_t)((s_test_cute_icon_index + 1u) & 0x03u);
      UI_Toast_Show("CUTE ICON NEXT",
                    ui_screen_test_get_cute_icon(s_test_cute_icon_index),
                    ICON8_W,
                    ICON8_H,
                    now_ms,
                    800u);
      break;

    case BUTTON_ID_NONE:
    default:
      break;
  }

  return UI_SCREEN_TEST_ACTION_NONE;
}

/* -------------------------------------------------------------------------- */
/*  Local helper: settings mode text helpers                                  */
/* -------------------------------------------------------------------------- */
static const char *ui_screen_test_backlight_mode_text(uint8_t mode_raw)
{
  return (mode_raw == (uint8_t)APP_BACKLIGHT_AUTO_MODE_DIMMER) ?
         "AUTO-DIMMER" : "AUTO-CONT";
}

static const char *ui_screen_test_dimmer_zone_text(uint8_t zone_raw)
{
  switch (zone_raw)
  {
    case BACKLIGHT_APP_DIMMER_ZONE_SUPER_NIGHT:
      return "SUPER";

    case BACKLIGHT_APP_DIMMER_ZONE_NIGHT:
      return "NIGHT";

    case BACKLIGHT_APP_DIMMER_ZONE_DAY:
    default:
      return "DAY";
  }
}

static const char *ui_screen_test_settings_category_text(uint8_t category)
{
  return (category == UI_TEST_SETTINGS_CATEGORY_UC1608) ? "UC1608" : "BACKLIGHT";
}

/* -------------------------------------------------------------------------- */
/*  Local helper: settings normalize                                           */
/* -------------------------------------------------------------------------- */
static void ui_screen_test_settings_normalize(app_settings_t *settings)
{
  if (settings == 0)
  {
    return;
  }

  if (settings->backlight.auto_mode > (uint8_t)APP_BACKLIGHT_AUTO_MODE_DIMMER)
  {
    settings->backlight.auto_mode = (uint8_t)APP_BACKLIGHT_AUTO_MODE_CONTINUOUS;
  }

  if (settings->backlight.continuous_bias_steps < -2)
  {
    settings->backlight.continuous_bias_steps = -2;
  }
  else if (settings->backlight.continuous_bias_steps > 2)
  {
    settings->backlight.continuous_bias_steps = 2;
  }

  if (settings->backlight.transition_smoothness < 1u)
  {
    settings->backlight.transition_smoothness = 1u;
  }
  else if (settings->backlight.transition_smoothness > 5u)
  {
    settings->backlight.transition_smoothness = 5u;
  }

  if (settings->backlight.night_threshold_percent > 100u)
  {
    settings->backlight.night_threshold_percent = 100u;
  }

  if (settings->backlight.super_night_threshold_percent > 100u)
  {
    settings->backlight.super_night_threshold_percent = 100u;
  }

  if (settings->backlight.super_night_threshold_percent >
      settings->backlight.night_threshold_percent)
  {
    settings->backlight.super_night_threshold_percent =
      settings->backlight.night_threshold_percent;
  }

  if (settings->backlight.night_brightness_percent > 100u)
  {
    settings->backlight.night_brightness_percent = 100u;
  }

  if (settings->backlight.super_night_brightness_percent > 100u)
  {
    settings->backlight.super_night_brightness_percent = 100u;
  }

  if (settings->uc1608.temperature_compensation > 3u)
  {
    settings->uc1608.temperature_compensation = 3u;
  }

  if (settings->uc1608.bias_ratio > 3u)
  {
    settings->uc1608.bias_ratio = 3u;
  }

  if (settings->uc1608.ram_access_mode > 3u)
  {
    settings->uc1608.ram_access_mode = 3u;
  }

  if (settings->uc1608.start_line_raw > 63u)
  {
    settings->uc1608.start_line_raw = 63u;
  }

  if (settings->uc1608.fixed_line_raw > 15u)
  {
    settings->uc1608.fixed_line_raw = 15u;
  }

  if (settings->uc1608.power_control_raw > 7u)
  {
    settings->uc1608.power_control_raw = 7u;
  }

  if (settings->uc1608.flip_mode > 1u)
  {
    settings->uc1608.flip_mode = 1u;
  }
}

/* -------------------------------------------------------------------------- */
/*  Local helper: settings live preview apply                                  */
/* -------------------------------------------------------------------------- */
static void ui_screen_test_settings_apply_preview(const app_settings_t *settings)
{
  if (settings == 0)
  {
    return;
  }

  APP_STATE_StoreSettingsSnapshot(settings);
  U8G2_UC1608_ApplyPanelSettings(&settings->uc1608);
}

/* -------------------------------------------------------------------------- */
/*  Local helper: settings enter / cancel / save                               */
/* -------------------------------------------------------------------------- */
static void ui_screen_test_settings_enter(uint32_t now_ms)
{
  APP_STATE_CopySettingsSnapshot(&s_test_settings_original);
  s_test_settings_draft = s_test_settings_original;
  ui_screen_test_settings_normalize(&s_test_settings_draft);

  s_test_settings_item_index = 0u;
  s_test_settings_active = 1u;
  ui_screen_test_configure_settings_bottom_bar();

  UI_Toast_Show("SCREEN SETTINGS",
                icon_ui_folder_8x8,
                ICON8_W,
                ICON8_H,
                now_ms,
                900u);
}

static void ui_screen_test_settings_cancel(uint32_t now_ms)
{
  ui_screen_test_settings_apply_preview(&s_test_settings_original);
  s_test_settings_draft = s_test_settings_original;
  s_test_settings_active = 0u;
  ui_screen_test_configure_bottom_bar();

  UI_Toast_Show("SETTINGS CANCELED",
                icon_ui_warn_8x8,
                ICON8_W,
                ICON8_H,
                now_ms,
                900u);
}

static void ui_screen_test_settings_save(uint32_t now_ms)
{
  ui_screen_test_settings_normalize(&s_test_settings_draft);
  ui_screen_test_settings_apply_preview(&s_test_settings_draft);
  s_test_settings_original = s_test_settings_draft;
  s_test_settings_active = 0u;
  ui_screen_test_configure_bottom_bar();

  UI_Toast_Show("SETTINGS SAVED",
                icon_ui_ok_8x8,
                ICON8_W,
                ICON8_H,
                now_ms,
                900u);
}

/* -------------------------------------------------------------------------- */
/*  Local helper: item value formatter                                          */
/* -------------------------------------------------------------------------- */
static void ui_screen_test_settings_get_value_text(uint8_t item_index,
                                                   const app_settings_t *settings,
                                                   char *out_text,
                                                   size_t out_size)
{
  if ((out_text == 0) || (out_size == 0u) || (settings == 0))
  {
    return;
  }

  switch (item_index)
  {
    case UI_TEST_SETTINGS_ITEM_BL_MODE:
      snprintf(out_text, out_size, "%s",
               ui_screen_test_backlight_mode_text(settings->backlight.auto_mode));
      break;

    case UI_TEST_SETTINGS_ITEM_BL_CONT_BIAS:
      snprintf(out_text, out_size, "%d",
               (int)settings->backlight.continuous_bias_steps);
      break;

    case UI_TEST_SETTINGS_ITEM_BL_SMOOTHNESS:
      snprintf(out_text, out_size, "%u",
               (unsigned)settings->backlight.transition_smoothness);
      break;

    case UI_TEST_SETTINGS_ITEM_BL_NIGHT_THRESHOLD:
      snprintf(out_text, out_size, "%u%%",
               (unsigned)settings->backlight.night_threshold_percent);
      break;

    case UI_TEST_SETTINGS_ITEM_BL_SUPER_THRESHOLD:
      snprintf(out_text, out_size, "%u%%",
               (unsigned)settings->backlight.super_night_threshold_percent);
      break;

    case UI_TEST_SETTINGS_ITEM_BL_NIGHT_LEVEL:
      snprintf(out_text, out_size, "%u%%",
               (unsigned)settings->backlight.night_brightness_percent);
      break;

    case UI_TEST_SETTINGS_ITEM_BL_SUPER_LEVEL:
      snprintf(out_text, out_size, "%u%%",
               (unsigned)settings->backlight.super_night_brightness_percent);
      break;

    case UI_TEST_SETTINGS_ITEM_DISP_CONTRAST:
      snprintf(out_text, out_size, "%u", (unsigned)settings->uc1608.contrast);
      break;

    case UI_TEST_SETTINGS_ITEM_DISP_TEMP_COMP:
      snprintf(out_text, out_size, "TC%u",
               (unsigned)settings->uc1608.temperature_compensation);
      break;

    case UI_TEST_SETTINGS_ITEM_DISP_BIAS:
      switch (settings->uc1608.bias_ratio)
      {
        case 0u: snprintf(out_text, out_size, "10.7"); break;
        case 1u: snprintf(out_text, out_size, "10.3"); break;
        case 2u: snprintf(out_text, out_size, "12.0"); break;
        default: snprintf(out_text, out_size, "12.7"); break;
      }
      break;

    case UI_TEST_SETTINGS_ITEM_DISP_RAM_ACCESS:
      snprintf(out_text, out_size, "RAM%u",
               (unsigned)settings->uc1608.ram_access_mode);
      break;

    case UI_TEST_SETTINGS_ITEM_DISP_START_LINE:
      snprintf(out_text, out_size, "%u",
               (unsigned)settings->uc1608.start_line_raw);
      break;

    case UI_TEST_SETTINGS_ITEM_DISP_FIXED_LINE:
      snprintf(out_text, out_size, "%u",
               (unsigned)settings->uc1608.fixed_line_raw);
      break;

    case UI_TEST_SETTINGS_ITEM_DISP_POWER_CTRL:
      snprintf(out_text, out_size, "%u",
               (unsigned)settings->uc1608.power_control_raw);
      break;

    case UI_TEST_SETTINGS_ITEM_DISP_FLIP_MODE:
      snprintf(out_text, out_size, "%s",
               (settings->uc1608.flip_mode != 0u) ? "FLIPPED" : "NORMAL");
      break;

    default:
      snprintf(out_text, out_size, "-");
      break;
  }
}

/* -------------------------------------------------------------------------- */
/*  Local helper: item value -> slider percent                                 */
/* -------------------------------------------------------------------------- */
static uint8_t ui_screen_test_settings_get_value_bar_percent(uint8_t item_index,
                                                             const app_settings_t *settings)
{
  if (settings == 0)
  {
    return 0u;
  }

  switch (item_index)
  {
    case UI_TEST_SETTINGS_ITEM_BL_MODE:
      return (settings->backlight.auto_mode == (uint8_t)APP_BACKLIGHT_AUTO_MODE_DIMMER) ? 100u : 0u;

    case UI_TEST_SETTINGS_ITEM_BL_CONT_BIAS:
      return (uint8_t)(((int32_t)settings->backlight.continuous_bias_steps + 2) * 25);

    case UI_TEST_SETTINGS_ITEM_BL_SMOOTHNESS:
      return (uint8_t)((settings->backlight.transition_smoothness - 1u) * 25u);

    case UI_TEST_SETTINGS_ITEM_BL_NIGHT_THRESHOLD:
      return settings->backlight.night_threshold_percent;

    case UI_TEST_SETTINGS_ITEM_BL_SUPER_THRESHOLD:
      return settings->backlight.super_night_threshold_percent;

    case UI_TEST_SETTINGS_ITEM_BL_NIGHT_LEVEL:
      return settings->backlight.night_brightness_percent;

    case UI_TEST_SETTINGS_ITEM_BL_SUPER_LEVEL:
      return settings->backlight.super_night_brightness_percent;

    case UI_TEST_SETTINGS_ITEM_DISP_CONTRAST:
      return (uint8_t)((settings->uc1608.contrast * 100u) / 255u);

    case UI_TEST_SETTINGS_ITEM_DISP_TEMP_COMP:
      return (uint8_t)((settings->uc1608.temperature_compensation * 100u) / 3u);

    case UI_TEST_SETTINGS_ITEM_DISP_BIAS:
      return (uint8_t)((settings->uc1608.bias_ratio * 100u) / 3u);

    case UI_TEST_SETTINGS_ITEM_DISP_RAM_ACCESS:
      return (uint8_t)((settings->uc1608.ram_access_mode * 100u) / 3u);

    case UI_TEST_SETTINGS_ITEM_DISP_START_LINE:
      return (uint8_t)((settings->uc1608.start_line_raw * 100u) / 63u);

    case UI_TEST_SETTINGS_ITEM_DISP_FIXED_LINE:
      return (uint8_t)((settings->uc1608.fixed_line_raw * 100u) / 15u);

    case UI_TEST_SETTINGS_ITEM_DISP_POWER_CTRL:
      return (uint8_t)((settings->uc1608.power_control_raw * 100u) / 7u);

    case UI_TEST_SETTINGS_ITEM_DISP_FLIP_MODE:
      return (settings->uc1608.flip_mode != 0u) ? 100u : 0u;

    default:
      return 0u;
  }
}

/* -------------------------------------------------------------------------- */
/*  Local helper: current item value adjust                                    */
/* -------------------------------------------------------------------------- */
static void ui_screen_test_settings_adjust_current_item(int8_t direction,
                                                        bool fast_step,
                                                        uint32_t now_ms)
{
  int32_t step_small;
  int32_t step_large;
  int32_t delta;
  int32_t value_temp;

  step_small = (direction >= 0) ? 1 : -1;
  step_large = (direction >= 0) ? 5 : -5;
  delta = fast_step ? step_large : step_small;

  switch (s_test_settings_item_index)
  {
    case UI_TEST_SETTINGS_ITEM_BL_MODE:
      s_test_settings_draft.backlight.auto_mode =
        (s_test_settings_draft.backlight.auto_mode == (uint8_t)APP_BACKLIGHT_AUTO_MODE_DIMMER) ?
        (uint8_t)APP_BACKLIGHT_AUTO_MODE_CONTINUOUS :
        (uint8_t)APP_BACKLIGHT_AUTO_MODE_DIMMER;
      break;

    case UI_TEST_SETTINGS_ITEM_BL_CONT_BIAS:
      value_temp = (int32_t)s_test_settings_draft.backlight.continuous_bias_steps + step_small;
      if (value_temp < -2) value_temp = -2;
      if (value_temp > 2)  value_temp = 2;
      s_test_settings_draft.backlight.continuous_bias_steps = (int8_t)value_temp;
      break;

    case UI_TEST_SETTINGS_ITEM_BL_SMOOTHNESS:
      value_temp = (int32_t)s_test_settings_draft.backlight.transition_smoothness + step_small;
      if (value_temp < 1) value_temp = 1;
      if (value_temp > 5) value_temp = 5;
      s_test_settings_draft.backlight.transition_smoothness = (uint8_t)value_temp;
      break;

    case UI_TEST_SETTINGS_ITEM_BL_NIGHT_THRESHOLD:
      value_temp = (int32_t)s_test_settings_draft.backlight.night_threshold_percent + delta;
      if (value_temp < 0) value_temp = 0;
      if (value_temp > 100) value_temp = 100;
      s_test_settings_draft.backlight.night_threshold_percent = (uint8_t)value_temp;
      break;

    case UI_TEST_SETTINGS_ITEM_BL_SUPER_THRESHOLD:
      value_temp = (int32_t)s_test_settings_draft.backlight.super_night_threshold_percent + delta;
      if (value_temp < 0) value_temp = 0;
      if (value_temp > 100) value_temp = 100;
      s_test_settings_draft.backlight.super_night_threshold_percent = (uint8_t)value_temp;
      break;

    case UI_TEST_SETTINGS_ITEM_BL_NIGHT_LEVEL:
      value_temp = (int32_t)s_test_settings_draft.backlight.night_brightness_percent + delta;
      if (value_temp < 0) value_temp = 0;
      if (value_temp > 100) value_temp = 100;
      s_test_settings_draft.backlight.night_brightness_percent = (uint8_t)value_temp;
      break;

    case UI_TEST_SETTINGS_ITEM_BL_SUPER_LEVEL:
      value_temp = (int32_t)s_test_settings_draft.backlight.super_night_brightness_percent + delta;
      if (value_temp < 0) value_temp = 0;
      if (value_temp > 100) value_temp = 100;
      s_test_settings_draft.backlight.super_night_brightness_percent = (uint8_t)value_temp;
      break;

    case UI_TEST_SETTINGS_ITEM_DISP_CONTRAST:
      value_temp = (int32_t)s_test_settings_draft.uc1608.contrast +
                   (fast_step ? ((direction >= 0) ? 16 : -16) : step_small);
      if (value_temp < 0) value_temp = 0;
      if (value_temp > 255) value_temp = 255;
      s_test_settings_draft.uc1608.contrast = (uint8_t)value_temp;
      break;

    case UI_TEST_SETTINGS_ITEM_DISP_TEMP_COMP:
      value_temp = (int32_t)s_test_settings_draft.uc1608.temperature_compensation + step_small;
      if (value_temp < 0) value_temp = 0;
      if (value_temp > 3) value_temp = 3;
      s_test_settings_draft.uc1608.temperature_compensation = (uint8_t)value_temp;
      break;

    case UI_TEST_SETTINGS_ITEM_DISP_BIAS:
      value_temp = (int32_t)s_test_settings_draft.uc1608.bias_ratio + step_small;
      if (value_temp < 0) value_temp = 0;
      if (value_temp > 3) value_temp = 3;
      s_test_settings_draft.uc1608.bias_ratio = (uint8_t)value_temp;
      break;

    case UI_TEST_SETTINGS_ITEM_DISP_RAM_ACCESS:
      value_temp = (int32_t)s_test_settings_draft.uc1608.ram_access_mode + step_small;
      if (value_temp < 0) value_temp = 0;
      if (value_temp > 3) value_temp = 3;
      s_test_settings_draft.uc1608.ram_access_mode = (uint8_t)value_temp;
      break;

    case UI_TEST_SETTINGS_ITEM_DISP_START_LINE:
      value_temp = (int32_t)s_test_settings_draft.uc1608.start_line_raw +
                   (fast_step ? ((direction >= 0) ? 8 : -8) : step_small);
      if (value_temp < 0) value_temp = 0;
      if (value_temp > 63) value_temp = 63;
      s_test_settings_draft.uc1608.start_line_raw = (uint8_t)value_temp;
      break;

    case UI_TEST_SETTINGS_ITEM_DISP_FIXED_LINE:
      value_temp = (int32_t)s_test_settings_draft.uc1608.fixed_line_raw + step_small;
      if (value_temp < 0) value_temp = 0;
      if (value_temp > 15) value_temp = 15;
      s_test_settings_draft.uc1608.fixed_line_raw = (uint8_t)value_temp;
      break;

    case UI_TEST_SETTINGS_ITEM_DISP_POWER_CTRL:
      value_temp = (int32_t)s_test_settings_draft.uc1608.power_control_raw + step_small;
      if (value_temp < 0) value_temp = 0;
      if (value_temp > 7) value_temp = 7;
      s_test_settings_draft.uc1608.power_control_raw = (uint8_t)value_temp;
      break;

    case UI_TEST_SETTINGS_ITEM_DISP_FLIP_MODE:
      s_test_settings_draft.uc1608.flip_mode =
        (s_test_settings_draft.uc1608.flip_mode == 0u) ? 1u : 0u;
      break;

    default:
      break;
  }

  ui_screen_test_settings_normalize(&s_test_settings_draft);
  ui_screen_test_settings_apply_preview(&s_test_settings_draft);

  UI_Toast_Show(s_test_settings_meta[s_test_settings_item_index].title,
                icon_ui_info_8x8,
                ICON8_W,
                ICON8_H,
                now_ms,
                500u);
}

/* -------------------------------------------------------------------------- */
/*  Local helper: settings mode button handler                                 */
/* -------------------------------------------------------------------------- */
static ui_screen_test_action_t ui_screen_test_handle_settings_button_event(const button_event_t *event,
                                                                           uint32_t now_ms)
{
  if (event == 0)
  {
    return UI_SCREEN_TEST_ACTION_NONE;
  }

  if ((event->type != BUTTON_EVENT_SHORT_PRESS) &&
      (event->type != BUTTON_EVENT_LONG_PRESS))
  {
    return UI_SCREEN_TEST_ACTION_NONE;
  }

  switch (event->id)
  {
    case BUTTON_ID_1:
      ui_screen_test_settings_cancel(now_ms);
      break;

    case BUTTON_ID_2:
      if (s_test_settings_item_index == 0u)
      {
        s_test_settings_item_index = (UI_TEST_SETTINGS_ITEM_COUNT - 1u);
      }
      else
      {
        s_test_settings_item_index--;
      }
      break;

    case BUTTON_ID_3:
      s_test_settings_item_index =
        (uint8_t)((s_test_settings_item_index + 1u) % UI_TEST_SETTINGS_ITEM_COUNT);
      break;

    case BUTTON_ID_4:
      ui_screen_test_settings_adjust_current_item(-1,
                                                  (event->type == BUTTON_EVENT_LONG_PRESS),
                                                  now_ms);
      break;

    case BUTTON_ID_5:
      ui_screen_test_settings_adjust_current_item(1,
                                                  (event->type == BUTTON_EVENT_LONG_PRESS),
                                                  now_ms);
      break;

    case BUTTON_ID_6:
      ui_screen_test_settings_save(now_ms);
      break;

    case BUTTON_ID_NONE:
    default:
      break;
  }

  return UI_SCREEN_TEST_ACTION_NONE;
}

/* -------------------------------------------------------------------------- */
/*  Compose getters                                                           */
/* -------------------------------------------------------------------------- */
ui_layout_mode_t UI_ScreenTest_GetLayoutMode(void)
{
  if (s_test_settings_active != 0u)
  {
    return UI_LAYOUT_MODE_TOP_BOTTOM_FIXED;
  }

  return s_test_layout_mode;
}

bool UI_ScreenTest_IsStatusBarVisible(void)
{
  if (s_test_settings_active != 0u)
  {
    return true;
  }

  return (s_test_layout_mode != UI_LAYOUT_MODE_FULLSCREEN);
}

bool UI_ScreenTest_IsBottomBarVisible(uint32_t now_ms, uint32_t pressed_mask)
{
  if (s_test_settings_active != 0u)
  {
    return true;
  }

  switch (s_test_layout_mode)
  {
    case UI_LAYOUT_MODE_TOP_BOTTOM_FIXED:
      return true;

    case UI_LAYOUT_MODE_TOP_EXTENDED_OVERLAY:
      return ((pressed_mask != 0u) ||
              ((int32_t)(s_test_bottom_overlay_until_ms - now_ms) > 0));

    case UI_LAYOUT_MODE_TOP_EXTENDED_NO_BOTTOM:
    case UI_LAYOUT_MODE_FULLSCREEN:
    default:
      return false;
  }
}

/* -------------------------------------------------------------------------- */
/*  Draw-state getters                                                        */
/* -------------------------------------------------------------------------- */
uint32_t UI_ScreenTest_GetLiveCounter20Hz(void)
{
  return s_test_live_counter_20hz;
}

bool UI_ScreenTest_GetUpdatesPaused(void)
{
  return (s_test_updates_paused != 0u);
}

bool UI_ScreenTest_GetBlinkPhase(void)
{
  return s_test_blink_phase_on;
}

uint8_t UI_ScreenTest_GetCuteIconIndex(void)
{
  return s_test_cute_icon_index;
}

/* -------------------------------------------------------------------------- */
/* Local helper: cute icon selector                                           */
/*                                                                            */
/* 이 함수는 테스트 화면 우측/하단에 찍어 볼 8x8 아이콘을 순환 선택한다.     */
/* - index 0 : 고양이 얼굴                                                    */
/* - index 1 : 하트                                                           */
/* - index 2 : 별                                                             */
/* - index 3 : 바이크                                                         */
/* -------------------------------------------------------------------------- */
static const uint8_t *ui_screen_test_get_cute_icon(uint8_t index)
{
  switch (index & 0x03u)
  {
    case 0u:
      return icon_cute_cat_8x8;

    case 1u:
      return icon_cute_heart_8x8;

    case 2u:
      return icon_cute_star_8x8;

    case 3u:
    default:
      return icon_cute_bike_8x8;
  }
}

/* -------------------------------------------------------------------------- */
/* Local helper: layout mode text                                             */
/*                                                                            */
/* 이 함수는 현재 엔진 레이아웃 모드를 짧은 영문 텍스트로 바꿔서             */
/* 본문 테스트 화면 상단 패널 안에 표시한다.                                 */
/* -------------------------------------------------------------------------- */
static const char *ui_screen_test_layout_text(ui_layout_mode_t mode)
{
  switch (mode)
  {
    case UI_LAYOUT_MODE_TOP_EXTENDED_NO_BOTTOM:
      return "TOP+EXT";

    case UI_LAYOUT_MODE_TOP_EXTENDED_OVERLAY:
      return "TOP+OVR";

    case UI_LAYOUT_MODE_TOP_BOTTOM_FIXED:
      return "TOP+BOT";

    case UI_LAYOUT_MODE_FULLSCREEN:
      return "FULLSCR";

    default:
      return "UNKNOWN";
  }
}

/* -------------------------------------------------------------------------- */
/* Local helper: viewport outer boundary                                      */
/*                                                                            */
/* 이 사각형은 "상단바/하단바를 제외한 실제 본문 뷰포트"의 가장 바깥 테두리를 */
/* 1픽셀 선으로 둘러싼다.                                                     */
/*                                                                            */
/* 목적                                                                      */
/* - 본문 표시 영역이 status bar font 영역과 겹치지 않는지 즉시 눈으로 확인  */
/* - bottom bar gap을 침범하지 않는지 확인                                    */
/* - UI 엔진이 계산한 viewport 좌표를 바로 시각화                             */
/* -------------------------------------------------------------------------- */
static void ui_screen_test_draw_viewport_boundary(u8g2_t *u8g2,
                                                  const ui_rect_t *viewport)
{
  if ((u8g2 == 0) || (viewport == 0))
  {
    return;
  }

  if ((viewport->w <= 1) || (viewport->h <= 1))
  {
    return;
  }

  u8g2_SetDrawColor(u8g2, 1);
  u8g2_DrawFrame(u8g2,
                 (u8g2_uint_t)viewport->x,
                 (u8g2_uint_t)viewport->y,
                 (u8g2_uint_t)viewport->w,
                 (u8g2_uint_t)viewport->h);
}

/* -------------------------------------------------------------------------- */
/* Local helper: title panel                                                  */
/*                                                                            */
/* 이 패널은 본문 뷰포트 안쪽 상단 1단에 그리는 안내 패널이다.               */
/* - blink_phase_on = true  : 흰 배경 + 프레임                               */
/* - blink_phase_on = false : 검은 박스 + 흰 글씨                            */
/*                                                                            */
/* 위치                                                                      */
/* - viewport 안쪽에서 2px 들여쓴 시작점부터                                 */
/* - 본문 상단에 폭 전체를 거의 다 쓰는 가로 패널                             */
/* -------------------------------------------------------------------------- */
static void ui_screen_test_draw_title_panel(u8g2_t *u8g2,
                                            const ui_rect_t *rect,
                                            bool blink_phase_on,
                                            bool updates_paused,
                                            ui_layout_mode_t layout_mode,
                                            uint8_t scene_index)
{
  char line[32];
  int16_t panel_x;
  int16_t panel_y;
  int16_t panel_w;
  int16_t panel_h;

  if ((u8g2 == 0) || (rect == 0))
  {
    return;
  }

  panel_x = rect->x;
  panel_y = rect->y;
  panel_w = rect->w;
  panel_h = 24;

  if (panel_w < 24)
  {
    return;
  }

  if (panel_h > rect->h)
  {
    panel_h = rect->h;
  }

  if (blink_phase_on != false)
  {
    /* -------------------------------------------------------------------- */
    /* 밝은 상태: 테두리만 그리고 검은 글씨를 올린다.                        */
    /* -------------------------------------------------------------------- */
    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawFrame(u8g2,
                   (u8g2_uint_t)panel_x,
                   (u8g2_uint_t)panel_y,
                   (u8g2_uint_t)panel_w,
                   (u8g2_uint_t)panel_h);

    u8g2_SetFont(u8g2, u8g2_font_7x13_mf);
    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)(panel_x + 6),
                 (u8g2_uint_t)(panel_y + 11),
                 "TEST UI ENGINE");

    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    snprintf(line,
             sizeof(line),
             "AUTO DEMO  %s  %s  SCN %u/4",
             updates_paused ? "HOLD" : "RUN",
             ui_screen_test_layout_text(layout_mode),
             (unsigned)(scene_index + 1u));
    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)(panel_x + 6),
                 (u8g2_uint_t)(panel_y + 21),
                 line);
  }
  else
  {
    /* -------------------------------------------------------------------- */
    /* 어두운 상태: 박스로 채우고 반전 글씨를 올린다.                        */
    /* -------------------------------------------------------------------- */
    u8g2_SetDrawColor(u8g2, 1);
    u8g2_DrawBox(u8g2,
                 (u8g2_uint_t)panel_x,
                 (u8g2_uint_t)panel_y,
                 (u8g2_uint_t)panel_w,
                 (u8g2_uint_t)panel_h);

    u8g2_SetDrawColor(u8g2, 0);
    u8g2_SetFont(u8g2, u8g2_font_7x13_mf);
    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)(panel_x + 6),
                 (u8g2_uint_t)(panel_y + 11),
                 "TEST UI ENGINE");

    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    snprintf(line,
             sizeof(line),
             "AUTO DEMO  %s  %s  SCN %u/4",
             updates_paused ? "HOLD" : "RUN",
             ui_screen_test_layout_text(layout_mode),
             (unsigned)(scene_index + 1u));
    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)(panel_x + 6),
                 (u8g2_uint_t)(panel_y + 21),
                 line);

    u8g2_SetDrawColor(u8g2, 1);
  }
}

/* -------------------------------------------------------------------------- */
/* Local helper: scene 0 - font showcase                                      */
/*                                                                            */
/* 이 장면은 U8G2에서 현재 바로 쓰는 텍스트 폰트 3종을 뷰포트 안에서         */
/* 자동으로 돌려 보이는 장면이다.                                            */
/*                                                                            */
/* 위치                                                                      */
/* - body_rect 안쪽 좌상단부터 순서대로 아래로 텍스트를 적재                 */
/* - 우측에는 cute icon 하나를 붙여서 XBM과 폰트가 동시에 보이게 함          */
/* -------------------------------------------------------------------------- */
static void ui_screen_test_draw_scene_fonts(u8g2_t *u8g2,
                                            const ui_rect_t *body_rect,
                                            uint32_t live_counter_20hz,
                                            const uint8_t *cute_icon)
{
  char line[32];
  uint32_t counter4;
  int16_t x;
  int16_t y;

  if ((u8g2 == 0) || (body_rect == 0))
  {
    return;
  }

  x = (int16_t)(body_rect->x + 4);
  y = (int16_t)(body_rect->y + 8);
  counter4 = live_counter_20hz % 10000u;

  u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
  u8g2_DrawStr(u8g2, (u8g2_uint_t)x, (u8g2_uint_t)y, "SCN0 FONT / TEXT");
  y = (int16_t)(y + 12);

  u8g2_SetFont(u8g2, u8g2_font_6x12_mf);
  snprintf(line, sizeof(line), "CNT %04lu", (unsigned long)counter4);
  u8g2_DrawStr(u8g2, (u8g2_uint_t)x, (u8g2_uint_t)y, line);
  y = (int16_t)(y + 15);

  u8g2_SetFont(u8g2, u8g2_font_7x13_mf);
  u8g2_DrawStr(u8g2, (u8g2_uint_t)x, (u8g2_uint_t)y, "U8G2 DEMO");
  y = (int16_t)(y + 16);

  u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
  u8g2_DrawStr(u8g2, (u8g2_uint_t)x, (u8g2_uint_t)y, "5x8 small status text");
  y = (int16_t)(y + 10);

  u8g2_SetFont(u8g2, u8g2_font_6x12_mf);
  u8g2_DrawStr(u8g2, (u8g2_uint_t)x, (u8g2_uint_t)y, "6x12 mid text");
  y = (int16_t)(y + 14);

  u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
  u8g2_DrawStr(u8g2,
               (u8g2_uint_t)(body_rect->x + body_rect->w - 42),
               (u8g2_uint_t)(body_rect->y + 18),
               "ICON");

  if (cute_icon != 0)
  {
    u8g2_DrawXBM(u8g2,
                 (u8g2_uint_t)(body_rect->x + body_rect->w - 22),
                 (u8g2_uint_t)(body_rect->y + 10),
                 ICON8_W,
                 ICON8_H,
                 cute_icon);
  }
}

/* -------------------------------------------------------------------------- */
/* Local helper: scene 1 - geometry showcase                                  */
/*                                                                            */
/* 이 장면은 프레임, 박스, 원, 라운드 프레임, 대각선 등을 한 번에 보여 준다. */
/*                                                                            */
/* 위치                                                                      */
/* - body_rect 중앙에 큰 프레임 1개                                           */
/* - 좌측에는 원/채운 원                                                      */
/* - 우측에는 라운드 프레임/박스                                              */
/* - 중앙에는 X자 대각선                                                      */
/* -------------------------------------------------------------------------- */
static void ui_screen_test_draw_scene_geometry(u8g2_t *u8g2,
                                               const ui_rect_t *body_rect,
                                               uint32_t live_counter_20hz)
{
  int16_t x0;
  int16_t y0;
  int16_t w0;
  int16_t h0;
  int16_t cx_left;
  int16_t cy_mid;
  int16_t cx_right;
  int16_t radius;
  int16_t mover;

  if ((u8g2 == 0) || (body_rect == 0))
  {
    return;
  }

  x0 = (int16_t)(body_rect->x + 4);
  y0 = (int16_t)(body_rect->y + 6);
  w0 = (int16_t)(body_rect->w - 8);
  h0 = (int16_t)(body_rect->h - 12);

  if ((w0 <= 20) || (h0 <= 20))
  {
    return;
  }

  cy_mid = (int16_t)(y0 + (h0 / 2));
  cx_left = (int16_t)(x0 + 22);
  cx_right = (int16_t)(x0 + w0 - 22);
  radius = 10;

  if (radius > (h0 / 3))
  {
    radius = (int16_t)(h0 / 3);
  }

  mover = (int16_t)(x0 + 8 + (int16_t)(live_counter_20hz % (uint32_t)(w0 - 16)));

  u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
  u8g2_DrawStr(u8g2, (u8g2_uint_t)x0, (u8g2_uint_t)(y0 - 1), "SCN1 GEOMETRY");

  /* 바깥 큰 작업 영역 프레임 */
  u8g2_DrawFrame(u8g2, (u8g2_uint_t)x0, (u8g2_uint_t)y0, (u8g2_uint_t)w0, (u8g2_uint_t)h0);

  /* 중앙 X자: 라인 렌더 품질 확인용 */
  u8g2_DrawLine(u8g2,
                (u8g2_uint_t)(x0 + 1),
                (u8g2_uint_t)(y0 + 1),
                (u8g2_uint_t)(x0 + w0 - 2),
                (u8g2_uint_t)(y0 + h0 - 2));
  u8g2_DrawLine(u8g2,
                (u8g2_uint_t)(x0 + 1),
                (u8g2_uint_t)(y0 + h0 - 2),
                (u8g2_uint_t)(x0 + w0 - 2),
                (u8g2_uint_t)(y0 + 1));

  /* 좌측 원: 외곽 원 + 채운 원으로 각각 outline/fill 확인 */
  u8g2_DrawCircle(u8g2, (u8g2_uint_t)cx_left, (u8g2_uint_t)cy_mid, (u8g2_uint_t)radius, U8G2_DRAW_ALL);
  u8g2_DrawDisc(u8g2,
                (u8g2_uint_t)cx_left,
                (u8g2_uint_t)(cy_mid + radius + 8),
                (u8g2_uint_t)((radius > 3) ? (radius - 3) : radius),
                U8G2_DRAW_ALL);

  /* 우측 라운드 박스: 모서리 라운드 렌더 확인 */
  u8g2_DrawRFrame(u8g2,
                  (u8g2_uint_t)(cx_right - 14),
                  (u8g2_uint_t)(cy_mid - 14),
                  28u,
                  14u,
                  3u);
  u8g2_DrawRBox(u8g2,
                (u8g2_uint_t)(cx_right - 14),
                (u8g2_uint_t)(cy_mid + 6),
                28u,
                10u,
                3u);

  /* 하단 이동 마커: 매 프레임 좌우 위치가 바뀌는 선형 이동 테스트 */
  u8g2_DrawFrame(u8g2,
                 (u8g2_uint_t)(x0 + 4),
                 (u8g2_uint_t)(y0 + h0 - 10),
                 (u8g2_uint_t)(w0 - 8),
                 7u);
  u8g2_DrawBox(u8g2,
               (u8g2_uint_t)mover,
               (u8g2_uint_t)(y0 + h0 - 9),
               6u,
               5u);
}

/* -------------------------------------------------------------------------- */
/* Local helper: scene 2 - icon showcase                                      */
/*                                                                            */
/* 이 장면은 상태/알림용 8x8 아이콘과 7x4 화살표를 본문 영역 안에서만        */
/* 정렬해서 그린다.                                                           */
/*                                                                            */
/* 위치                                                                      */
/* - 상단 1줄 : 8x8 아이콘 갤러리                                             */
/* - 중단 1줄 : 7x4 화살표 갤러리                                             */
/* - 하단     : cute icon이 트랙 위를 자동으로 이동                           */
/* -------------------------------------------------------------------------- */
static void ui_screen_test_draw_scene_icons(u8g2_t *u8g2,
                                            const ui_rect_t *body_rect,
                                            uint32_t live_counter_20hz,
                                            const uint8_t *cute_icon)
{
  int16_t x;
  int16_t y_icons;
  int16_t y_arrows;
  int16_t track_x;
  int16_t track_y;
  int16_t track_w;
  int16_t mover_x;
  uint32_t mover_mod;

  if ((u8g2 == 0) || (body_rect == 0))
  {
    return;
  }

  x = (int16_t)(body_rect->x + 6);
  y_icons = (int16_t)(body_rect->y + 10);
  y_arrows = (int16_t)(body_rect->y + 30);
  track_x = (int16_t)(body_rect->x + 8);
  track_y = (int16_t)(body_rect->y + body_rect->h - 14);
  track_w = (int16_t)(body_rect->w - 16);

  if (track_w < 20)
  {
    track_w = 20;
  }

  mover_mod = (track_w > 10) ? (uint32_t)(track_w - 10) : 1u;
  mover_x = (int16_t)(track_x + (int16_t)(live_counter_20hz % mover_mod));

  u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
  u8g2_DrawStr(u8g2, (u8g2_uint_t)x, (u8g2_uint_t)(body_rect->y + 6), "SCN2 ICON / XBM");

  /* 8x8 상태/알림 아이콘 줄 */
  u8g2_DrawXBM(u8g2, (u8g2_uint_t)(x + 0),  (u8g2_uint_t)y_icons, ICON8_W, ICON8_H, icon_ui_info_8x8);
  u8g2_DrawXBM(u8g2, (u8g2_uint_t)(x + 16), (u8g2_uint_t)y_icons, ICON8_W, ICON8_H, icon_ui_warn_8x8);
  u8g2_DrawXBM(u8g2, (u8g2_uint_t)(x + 32), (u8g2_uint_t)y_icons, ICON8_W, ICON8_H, icon_ui_ok_8x8);
  u8g2_DrawXBM(u8g2, (u8g2_uint_t)(x + 48), (u8g2_uint_t)y_icons, ICON8_W, ICON8_H, icon_ui_bell_8x8);
  u8g2_DrawXBM(u8g2, (u8g2_uint_t)(x + 64), (u8g2_uint_t)y_icons, ICON8_W, ICON8_H, icon_ui_folder_8x8);

  /* 7x4 방향 화살표 줄 */
  u8g2_DrawXBM(u8g2, (u8g2_uint_t)(x + 0),  (u8g2_uint_t)y_arrows, ICON7X4_W, ICON7X4_H, icon_arrow_left_7x4);
  u8g2_DrawXBM(u8g2, (u8g2_uint_t)(x + 16), (u8g2_uint_t)y_arrows, ICON7X4_W, ICON7X4_H, icon_arrow_right_7x4);
  u8g2_DrawXBM(u8g2, (u8g2_uint_t)(x + 32), (u8g2_uint_t)y_arrows, ICON7X4_W, ICON7X4_H, icon_arrow_up_7x4);
  u8g2_DrawXBM(u8g2, (u8g2_uint_t)(x + 48), (u8g2_uint_t)y_arrows, ICON7X4_W, ICON7X4_H, icon_arrow_down_7x4);

  /* 하단 이동 트랙: XBM sprite 이동 확인 */
  u8g2_DrawFrame(u8g2,
                 (u8g2_uint_t)track_x,
                 (u8g2_uint_t)track_y,
                 (u8g2_uint_t)track_w,
                 9u);

  if (cute_icon != 0)
  {
    u8g2_DrawXBM(u8g2,
                 (u8g2_uint_t)mover_x,
                 (u8g2_uint_t)(track_y - 10),
                 ICON8_W,
                 ICON8_H,
                 cute_icon);
  }

  u8g2_DrawBox(u8g2,
               (u8g2_uint_t)mover_x,
               (u8g2_uint_t)(track_y + 2),
               8u,
               4u);
}

/* -------------------------------------------------------------------------- */
/* Local helper: scene 3 - auto stress showcase                               */
/*                                                                            */
/* 이 장면은 실제 측정값을 찍는 profiler는 아니고,                            */
/* 본문 영역 안에서 라인/박스/막대가 동시에 계속 바뀌게 해서                  */
/* 그래픽 파이프라인이 찢어지거나 병목으로 눈에 띄게 끊기는지 확인하는        */
/* 자동 스트레스 장면이다.                                                    */
/*                                                                            */
/* 위치                                                                      */
/* - 좌측 : 4개 세로 막대                                                     */
/* - 우측 : 여러 가로 스캔 라인                                               */
/* - 하단 : 진행률 바                                                         */
/* -------------------------------------------------------------------------- */
static void ui_screen_test_draw_scene_stress(u8g2_t *u8g2,
                                             const ui_rect_t *body_rect,
                                             uint32_t live_counter_20hz,
                                             bool updates_paused)
{
  char line[32];
  int16_t graph_x;
  int16_t graph_y;
  int16_t bar_w;
  int16_t bar_h;
  int16_t progress_x;
  int16_t progress_y;
  int16_t progress_w;
  int16_t scan_span;
  int16_t i;

  if ((u8g2 == 0) || (body_rect == 0))
  {
    return;
  }

  graph_x = (int16_t)(body_rect->x + 8);
  graph_y = (int16_t)(body_rect->y + body_rect->h - 34);
  bar_w = 10;
  bar_h = 24;
  progress_x = (int16_t)(body_rect->x + 8);
  progress_y = (int16_t)(body_rect->y + body_rect->h - 8);
  progress_w = (int16_t)(body_rect->w - 16);
  scan_span = (int16_t)(body_rect->w - 90);

  if (scan_span < 12)
  {
    scan_span = 12;
  }

  u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
  u8g2_DrawStr(u8g2,
               (u8g2_uint_t)(body_rect->x + 4),
               (u8g2_uint_t)(body_rect->y + 6),
               "SCN3 AUTO STRESS");

  snprintf(line,
           sizeof(line),
           "OPS LINE/BOX/BAR  %s",
           updates_paused ? "HOLD" : "RUN");
  u8g2_DrawStr(u8g2,
               (u8g2_uint_t)(body_rect->x + 4),
               (u8g2_uint_t)(body_rect->y + 15),
               line);

  /* 좌측 막대 4개: 서로 다른 위상으로 자동 변화 */
  for (i = 0; i < 4; i++)
  {
    int16_t bx;
    int16_t fill_h;
    uint32_t phase;

    bx = (int16_t)(graph_x + (i * 16));
    phase = (live_counter_20hz + (uint32_t)(i * 11)) % (uint32_t)bar_h;
    fill_h = (int16_t)phase;

    u8g2_DrawFrame(u8g2, (u8g2_uint_t)bx, (u8g2_uint_t)graph_y, (u8g2_uint_t)bar_w, (u8g2_uint_t)bar_h);
    u8g2_DrawBox(u8g2,
                 (u8g2_uint_t)(bx + 2),
                 (u8g2_uint_t)(graph_y + bar_h - fill_h),
                 (u8g2_uint_t)(bar_w - 4),
                 (u8g2_uint_t)fill_h);
  }

  /* 우측 스캔 라인: 가로선 길이가 계속 변하도록 해서 라인 드로잉 확인 */
  for (i = 0; i < 8; i++)
  {
    int16_t y;
    int16_t len;
    uint32_t phase;

    y = (int16_t)(body_rect->y + 24 + (i * 8));
    phase = (live_counter_20hz + (uint32_t)(i * 7)) % (uint32_t)scan_span;
    len = (int16_t)(12 + phase);

    if (len > scan_span)
    {
      len = scan_span;
    }

    u8g2_DrawLine(u8g2,
                  (u8g2_uint_t)(body_rect->x + 86),
                  (u8g2_uint_t)y,
                  (u8g2_uint_t)(body_rect->x + 86 + len),
                  (u8g2_uint_t)y);
  }

  /* 하단 진행률 바: 본문 영역 안에서만 반복 */
  if (progress_w > 12)
  {
    uint32_t span;
    int16_t fill_w;

    span = (uint32_t)(progress_w - 2);
    fill_w = (int16_t)(1 + (int16_t)(live_counter_20hz % span));

    u8g2_DrawFrame(u8g2,
                   (u8g2_uint_t)progress_x,
                   (u8g2_uint_t)progress_y,
                   (u8g2_uint_t)progress_w,
                   6u);

    u8g2_DrawBox(u8g2,
                 (u8g2_uint_t)(progress_x + 1),
                 (u8g2_uint_t)(progress_y + 1),
                 (u8g2_uint_t)fill_w,
                 4u);
  }
}

/* -------------------------------------------------------------------------- */
/* Local helper: footer info line                                             */
/*                                                                            */
/* 이 줄은 본문 영역 맨 아래쪽 안쪽에 현재 viewport 크기와 tick 정보를       */
/* 작게 표시한다.                                                             */
/* -------------------------------------------------------------------------- */
static void ui_screen_test_draw_footer_info(u8g2_t *u8g2,
                                            const ui_rect_t *rect,
                                            uint32_t live_counter_20hz,
                                            bool updates_paused)
{
  char line[40];
  int16_t y;

  if ((u8g2 == 0) || (rect == 0))
  {
    return;
  }

  y = (int16_t)(rect->y + rect->h - 2);

  u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
  snprintf(line,
           sizeof(line),
           "VIEW %dx%d  T%05lu  %s",
           (int)rect->w,
           (int)rect->h,
           (unsigned long)(live_counter_20hz % 100000u),
           updates_paused ? "PAUSED" : "RUNNING");
  u8g2_DrawStr(u8g2,
               (u8g2_uint_t)(rect->x + 4),
               (u8g2_uint_t)y,
               line);
}

/* -------------------------------------------------------------------------- */
/*  Local helper: settings screen draw                                        */
/*                                                                            */
/*  그림 설명                                                                  */
/*  - viewport 바깥 1px 테두리: TEST screen 본문 영역 외곽                     */
/*  - 그 안쪽 상단: SCREEN TEST SETTINGS title bar                            */
/*  - title bar 아래 2개 탭: BACKLIGHT / UC1608                               */
/*  - 중앙 큰 카드: 현재 선택된 setting item 상세                             */
/*  - 우측 skinny panel: live runtime diagnostics                             */
/*  - 카드 하단 slider: 현재 값의 상대 위치                                   */
/*                                                                            */
/*  이 화면은 status bar와 bottom bar가 이미 외부에서 따로 그려지고 있다는     */
/*  전제 아래, viewport 내부만 정확히 채운다.                                 */
/* -------------------------------------------------------------------------- */
static void ui_screen_test_draw_settings_screen(u8g2_t *u8g2,
                                                const ui_rect_t *viewport)
{
  ui_rect_t inner_rect;
  uint8_t current_category;
  char value_text[24];
  char line[40];
  int16_t card_x;
  int16_t card_y;
  int16_t card_w;
  int16_t card_h;
  int16_t diag_x;
  int16_t diag_y;
  int16_t diag_w;
  int16_t diag_h;
  uint8_t bar_percent;
  uint8_t fill_w;

  if ((u8g2 == 0) || (viewport == 0))
  {
    return;
  }

  ui_screen_test_draw_viewport_boundary(u8g2, viewport);

  inner_rect = *viewport;
  inner_rect.x = (int16_t)(inner_rect.x + 2);
  inner_rect.y = (int16_t)(inner_rect.y + 2);
  inner_rect.w = (int16_t)(inner_rect.w - 4);
  inner_rect.h = (int16_t)(inner_rect.h - 4);

  current_category = s_test_settings_meta[s_test_settings_item_index].category;
  ui_screen_test_settings_get_value_text(s_test_settings_item_index,
                                         &s_test_settings_draft,
                                         value_text,
                                         sizeof(value_text));
  bar_percent = ui_screen_test_settings_get_value_bar_percent(s_test_settings_item_index,
                                                              &s_test_settings_draft);

  /* title bar */
  u8g2_SetDrawColor(u8g2, 1);
  u8g2_DrawFrame(u8g2,
                 (u8g2_uint_t)inner_rect.x,
                 (u8g2_uint_t)inner_rect.y,
                 (u8g2_uint_t)inner_rect.w,
                 20u);
  u8g2_SetFont(u8g2, u8g2_font_7x13_mf);
  u8g2_DrawStr(u8g2,
               (u8g2_uint_t)(inner_rect.x + 6),
               (u8g2_uint_t)(inner_rect.y + 13),
               "SCREEN TEST SETTINGS");

  /* category tabs */
  {
    int16_t tab_y = (int16_t)(inner_rect.y + 24);
    int16_t tab_w = 72;

    if (current_category == UI_TEST_SETTINGS_CATEGORY_BACKLIGHT)
    {
      u8g2_DrawBox(u8g2, (u8g2_uint_t)(inner_rect.x + 4), (u8g2_uint_t)tab_y, (u8g2_uint_t)tab_w, 12u);
      u8g2_SetDrawColor(u8g2, 0);
      u8g2_DrawStr(u8g2, (u8g2_uint_t)(inner_rect.x + 10), (u8g2_uint_t)(tab_y + 9), "BACKLIGHT");
      u8g2_SetDrawColor(u8g2, 1);
      u8g2_DrawFrame(u8g2, (u8g2_uint_t)(inner_rect.x + 84), (u8g2_uint_t)tab_y, (u8g2_uint_t)tab_w, 12u);
      u8g2_DrawStr(u8g2, (u8g2_uint_t)(inner_rect.x + 104), (u8g2_uint_t)(tab_y + 9), "UC1608");
    }
    else
    {
      u8g2_DrawFrame(u8g2, (u8g2_uint_t)(inner_rect.x + 4), (u8g2_uint_t)tab_y, (u8g2_uint_t)tab_w, 12u);
      u8g2_DrawStr(u8g2, (u8g2_uint_t)(inner_rect.x + 10), (u8g2_uint_t)(tab_y + 9), "BACKLIGHT");
      u8g2_DrawBox(u8g2, (u8g2_uint_t)(inner_rect.x + 84), (u8g2_uint_t)tab_y, (u8g2_uint_t)tab_w, 12u);
      u8g2_SetDrawColor(u8g2, 0);
      u8g2_DrawStr(u8g2, (u8g2_uint_t)(inner_rect.x + 104), (u8g2_uint_t)(tab_y + 9), "UC1608");
      u8g2_SetDrawColor(u8g2, 1);
    }
  }

  /* main card + diagnostic panel */
  card_x = (int16_t)(inner_rect.x + 4);
  card_y = (int16_t)(inner_rect.y + 40);
  card_w = (int16_t)(inner_rect.w - 86);
  card_h = (int16_t)(inner_rect.h - 44);
  diag_x = (int16_t)(inner_rect.x + inner_rect.w - 78);
  diag_y = card_y;
  diag_w = 74;
  diag_h = card_h;

  if (card_w < 80)
  {
    card_w = (int16_t)(inner_rect.w - 8);
    diag_w = 0;
  }

  u8g2_DrawFrame(u8g2, (u8g2_uint_t)card_x, (u8g2_uint_t)card_y, (u8g2_uint_t)card_w, (u8g2_uint_t)card_h);
  if (diag_w > 0)
  {
    u8g2_DrawFrame(u8g2, (u8g2_uint_t)diag_x, (u8g2_uint_t)diag_y, (u8g2_uint_t)diag_w, (u8g2_uint_t)diag_h);
  }

  u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
  snprintf(line, sizeof(line), "%s  %u/%u",
           ui_screen_test_settings_category_text(current_category),
           (unsigned)(s_test_settings_item_index + 1u),
           (unsigned)UI_TEST_SETTINGS_ITEM_COUNT);
  u8g2_DrawStr(u8g2, (u8g2_uint_t)(card_x + 4), (u8g2_uint_t)(card_y + 8), line);

  u8g2_SetFont(u8g2, u8g2_font_7x13_mf);
  u8g2_DrawStr(u8g2,
               (u8g2_uint_t)(card_x + 4),
               (u8g2_uint_t)(card_y + 22),
               s_test_settings_meta[s_test_settings_item_index].title);

  u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
  u8g2_DrawStr(u8g2,
               (u8g2_uint_t)(card_x + 4),
               (u8g2_uint_t)(card_y + 31),
               s_test_settings_meta[s_test_settings_item_index].subtitle);

  u8g2_SetFont(u8g2, u8g2_font_7x13_mf);
  u8g2_DrawFrame(u8g2,
                 (u8g2_uint_t)(card_x + 6),
                 (u8g2_uint_t)(card_y + 38),
                 (u8g2_uint_t)(card_w - 12),
                 22u);
  u8g2_DrawStr(u8g2,
               (u8g2_uint_t)(card_x + 12),
               (u8g2_uint_t)(card_y + 53),
               value_text);

  /* slider */
  u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
  u8g2_DrawStr(u8g2,
               (u8g2_uint_t)(card_x + 4),
               (u8g2_uint_t)(card_y + 70),
               "VALUE POSITION");
  u8g2_DrawFrame(u8g2,
                 (u8g2_uint_t)(card_x + 6),
                 (u8g2_uint_t)(card_y + 74),
                 (u8g2_uint_t)(card_w - 12),
                 8u);
  fill_w = (uint8_t)((((uint32_t)(card_w - 14)) * bar_percent) / 100u);
  if (fill_w > 0u)
  {
    u8g2_DrawBox(u8g2,
                 (u8g2_uint_t)(card_x + 7),
                 (u8g2_uint_t)(card_y + 75),
                 (u8g2_uint_t)fill_w,
                 6u);
  }

  snprintf(line, sizeof(line), "F2/F3 ITEM  F4/F5 VALUE");
  u8g2_DrawStr(u8g2,
               (u8g2_uint_t)(card_x + 4),
               (u8g2_uint_t)(card_y + card_h - 16),
               line);
  snprintf(line, sizeof(line), "F1 CANCEL  F6 SAVE");
  u8g2_DrawStr(u8g2,
               (u8g2_uint_t)(card_x + 4),
               (u8g2_uint_t)(card_y + card_h - 6),
               line);

  if (diag_w > 0)
  {
    int16_t y = (int16_t)(diag_y + 8);

    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    u8g2_DrawStr(u8g2, (u8g2_uint_t)(diag_x + 4), (u8g2_uint_t)y, "LIVE PREVIEW");
    y = (int16_t)(y + 10);

    if (current_category == UI_TEST_SETTINGS_CATEGORY_BACKLIGHT)
    {
      snprintf(line, sizeof(line), "MODE %s",
               (g_backlight_app_runtime.active_mode == BACKLIGHT_APP_ACTIVE_MODE_AUTO_DIMMER) ? "DIM" :
               ((g_backlight_app_runtime.active_mode == BACKLIGHT_APP_ACTIVE_MODE_MANUAL_OVERRIDE) ? "MAN" : "CONT"));
      u8g2_DrawStr(u8g2, (u8g2_uint_t)(diag_x + 4), (u8g2_uint_t)y, line);
      y = (int16_t)(y + 9);

      snprintf(line, sizeof(line), "ZONE %s",
               ui_screen_test_dimmer_zone_text(g_backlight_app_runtime.active_zone));
      u8g2_DrawStr(u8g2, (u8g2_uint_t)(diag_x + 4), (u8g2_uint_t)y, line);
      y = (int16_t)(y + 9);

      snprintf(line, sizeof(line), "SNS %u%%", (unsigned)g_backlight_app_runtime.sensor_percent);
      u8g2_DrawStr(u8g2, (u8g2_uint_t)(diag_x + 4), (u8g2_uint_t)y, line);
      y = (int16_t)(y + 9);

      snprintf(line, sizeof(line), "FIL %u%%",
               (unsigned)((g_backlight_app_runtime.sensor_filtered_permille + 5u) / 10u));
      u8g2_DrawStr(u8g2, (u8g2_uint_t)(diag_x + 4), (u8g2_uint_t)y, line);
      y = (int16_t)(y + 9);

      snprintf(line, sizeof(line), "TGT %u%%", (unsigned)g_backlight_app_runtime.target_linear_percent);
      u8g2_DrawStr(u8g2, (u8g2_uint_t)(diag_x + 4), (u8g2_uint_t)y, line);
      y = (int16_t)(y + 9);

      snprintf(line, sizeof(line), "OUT %u%%", (unsigned)g_backlight_app_runtime.applied_linear_percent);
      u8g2_DrawStr(u8g2, (u8g2_uint_t)(diag_x + 4), (u8g2_uint_t)y, line);
      y = (int16_t)(y + 9);

      snprintf(line, sizeof(line), "CCR %lu", (unsigned long)g_backlight_driver_state.last_compare_counts);
      u8g2_DrawStr(u8g2, (u8g2_uint_t)(diag_x + 4), (u8g2_uint_t)y, line);
      y = (int16_t)(y + 9);

      u8g2_DrawStr(u8g2, (u8g2_uint_t)(diag_x + 4), (u8g2_uint_t)y, "PB1 TIM3C4");
    }
    else
    {
      snprintf(line, sizeof(line), "CTR %u", (unsigned)g_u8g2_uc1608_runtime.applied.contrast);
      u8g2_DrawStr(u8g2, (u8g2_uint_t)(diag_x + 4), (u8g2_uint_t)y, line);
      y = (int16_t)(y + 9);

      snprintf(line, sizeof(line), "TC %u", (unsigned)g_u8g2_uc1608_runtime.applied.temperature_compensation);
      u8g2_DrawStr(u8g2, (u8g2_uint_t)(diag_x + 4), (u8g2_uint_t)y, line);
      y = (int16_t)(y + 9);

      snprintf(line, sizeof(line), "BIA %u", (unsigned)g_u8g2_uc1608_runtime.applied.bias_ratio);
      u8g2_DrawStr(u8g2, (u8g2_uint_t)(diag_x + 4), (u8g2_uint_t)y, line);
      y = (int16_t)(y + 9);

      snprintf(line, sizeof(line), "RAM %u", (unsigned)g_u8g2_uc1608_runtime.applied.ram_access_mode);
      u8g2_DrawStr(u8g2, (u8g2_uint_t)(diag_x + 4), (u8g2_uint_t)y, line);
      y = (int16_t)(y + 9);

      snprintf(line, sizeof(line), "SL %u", (unsigned)g_u8g2_uc1608_runtime.applied.start_line_raw);
      u8g2_DrawStr(u8g2, (u8g2_uint_t)(diag_x + 4), (u8g2_uint_t)y, line);
      y = (int16_t)(y + 9);

      snprintf(line, sizeof(line), "FL %u", (unsigned)g_u8g2_uc1608_runtime.applied.fixed_line_raw);
      u8g2_DrawStr(u8g2, (u8g2_uint_t)(diag_x + 4), (u8g2_uint_t)y, line);
      y = (int16_t)(y + 9);

      snprintf(line, sizeof(line), "PWR %u", (unsigned)g_u8g2_uc1608_runtime.applied.power_control_raw);
      u8g2_DrawStr(u8g2, (u8g2_uint_t)(diag_x + 4), (u8g2_uint_t)y, line);
      y = (int16_t)(y + 9);

      snprintf(line, sizeof(line), "FLIP %u", (unsigned)g_u8g2_uc1608_runtime.applied.flip_mode);
      u8g2_DrawStr(u8g2, (u8g2_uint_t)(diag_x + 4), (u8g2_uint_t)y, line);
    }
  }
}

/* -------------------------------------------------------------------------- */
/* Public draw: UI test screen                                                */
/*                                                                            */
/* 설계 원칙                                                                  */
/* - 이 함수는 "상단바/하단바를 제외한 viewport 안" 에만 그린다.              */
/* - status bar / bottom bar / toast / popup 은 전혀 건드리지 않는다.        */
/* - 모든 도형은 viewport 바깥으로 나가지 않도록 좌표를 viewport 기준으로만  */
/*   계산한다.                                                                */
/* - 자동 scene rotation 으로 버튼 없이도 데모 쇼 / 시각적 스트레스 테스트가  */
/*   계속 진행된다.                                                           */
/* -------------------------------------------------------------------------- */
void UI_ScreenTest_Draw(u8g2_t *u8g2,
                        const ui_rect_t *viewport,
                        uint32_t live_counter_20hz,
                        bool updates_paused,
                        bool blink_phase_on,
                        uint8_t cute_icon_index,
                        ui_layout_mode_t layout_mode)
{
  const uint8_t *cute_icon;
  ui_rect_t inner_rect;
  ui_rect_t body_rect;
  uint8_t scene_index;

  if ((u8g2 == 0) || (viewport == 0))
  {
    return;
  }

  if ((viewport->w <= 2) || (viewport->h <= 2))
  {
    return;
  }

  if (s_test_settings_active != 0u)
  {
    ui_screen_test_draw_settings_screen(u8g2, viewport);
    return;
  }

  /* ------------------------------------------------------------------------ */
  /* 가장 바깥 1px 경계선                                                     */
  /*                                                                          */
  /* 이 선이 바로 "상단바/하단바를 제외한 본문 합성 영역의 외곽선" 이다.      */
  /* ------------------------------------------------------------------------ */
  ui_screen_test_draw_viewport_boundary(u8g2, viewport);

  /* ------------------------------------------------------------------------ */
  /* 경계선 안쪽에서 실제 데모를 그릴 내부 사각형                             */
  /* ------------------------------------------------------------------------ */
  inner_rect = *viewport;
  inner_rect.x = (int16_t)(inner_rect.x + 2);
  inner_rect.y = (int16_t)(inner_rect.y + 2);
  inner_rect.w = (int16_t)(inner_rect.w - 4);
  inner_rect.h = (int16_t)(inner_rect.h - 4);

  if ((inner_rect.w <= 10) || (inner_rect.h <= 20))
  {
    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    u8g2_DrawStr(u8g2,
                 (u8g2_uint_t)(viewport->x + 2),
                 (u8g2_uint_t)(viewport->y + 8),
                 "SMALL VIEWPORT");
    return;
  }

  cute_icon = ui_screen_test_get_cute_icon(cute_icon_index);
  scene_index = (uint8_t)((live_counter_20hz / 60u) & 0x03u);

  /* ------------------------------------------------------------------------ */
  /* 상단 타이틀 패널                                                         */
  /* ------------------------------------------------------------------------ */
  ui_screen_test_draw_title_panel(u8g2,
                                  &inner_rect,
                                  blink_phase_on,
                                  updates_paused,
                                  layout_mode,
                                  scene_index);

  /* ------------------------------------------------------------------------ */
  /* 타이틀 패널 아래 실제 장면 본문 영역                                     */
  /* ------------------------------------------------------------------------ */
  body_rect = inner_rect;
  body_rect.y = (int16_t)(body_rect.y + 28);
  body_rect.h = (int16_t)(body_rect.h - 40);

  if (body_rect.h < 24)
  {
    body_rect.h = (int16_t)(inner_rect.h - 30);
  }

  if (body_rect.h < 12)
  {
    body_rect.h = 12;
  }

  /* ------------------------------------------------------------------------ */
  /* scene rotation                                                           */
  /* ------------------------------------------------------------------------ */
  switch (scene_index)
  {
    case 0u:
      ui_screen_test_draw_scene_fonts(u8g2, &body_rect, live_counter_20hz, cute_icon);
      break;

    case 1u:
      ui_screen_test_draw_scene_geometry(u8g2, &body_rect, live_counter_20hz);
      break;

    case 2u:
      ui_screen_test_draw_scene_icons(u8g2, &body_rect, live_counter_20hz, cute_icon);
      break;

    case 3u:
    default:
      ui_screen_test_draw_scene_stress(u8g2, &body_rect, live_counter_20hz, updates_paused);
      break;
  }

  /* ------------------------------------------------------------------------ */
  /* 맨 아래 정보 라인                                                        */
  /* ------------------------------------------------------------------------ */
  ui_screen_test_draw_footer_info(u8g2, &inner_rect, live_counter_20hz, updates_paused);
}
