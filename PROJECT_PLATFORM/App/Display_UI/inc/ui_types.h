#ifndef UI_TYPES_H
#define UI_TYPES_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* Display-wide fixed geometry                                                 */
/*                                                                            */
/* 이 프로젝트의 UI 엔진이 현재 사용하는 논리 좌표계는 240 x 128 이다.            */
/* 실제 LCD glass 의 물리 해상도나 controller native memory map 과 별개로,       */
/* UI 엔진 / status bar / bottom bar / Vario screen bridge 는 이 좌표계를        */
/* 기준으로 화면을 배치한다.                                                   */
/* -------------------------------------------------------------------------- */
#define UI_LCD_W 240
#define UI_LCD_H 128

/* -------------------------------------------------------------------------- */
/* Bar geometry                                                                */
/*                                                                            */
/* status bar / bottom bar / gap 규격은 기존 엔진 규약을 그대로 유지한다.        */
/*                                                                            */
/* - status bar nominal height : 7 px                                          */
/* - status bar 아래 gap       : 0 px                                          */
/* - bottom bar 위 gap         : 1 px                                          */
/* - bottom bar nominal height : 8 px                                          */
/*                                                                            */
/* 주의                                                                         */
/* - status bar의 "실제 본문 침범 금지 높이" 는                                */
/*   UI_StatusBar_GetReservedHeight() 가 폰트 metric 기준으로 계산한다.         */
/* - 따라서 status bar nominal 7 px와 실제 viewport top Y는 완전히 같지 않을 수  */
/*   있다.                                                                      */
/* -------------------------------------------------------------------------- */
#define UI_STATUSBAR_H     7
#define UI_STATUSBAR_GAP_H 0
#define UI_BOTTOMBAR_H     8
#define UI_BOTTOMBAR_GAP_H 1

/* -------------------------------------------------------------------------- */
/* Overlay policy / timing                                                     */
/* -------------------------------------------------------------------------- */
#ifndef UI_BOTTOMBAR_OVERLAY_TIMEOUT_MS
#define UI_BOTTOMBAR_OVERLAY_TIMEOUT_MS 2200u
#endif

#ifndef UI_TOAST_DEFAULT_TIMEOUT_MS
#define UI_TOAST_DEFAULT_TIMEOUT_MS 1500u
#endif

#ifndef UI_POPUP_DEFAULT_TIMEOUT_MS
#define UI_POPUP_DEFAULT_TIMEOUT_MS 2500u
#endif

/* -------------------------------------------------------------------------- */
/* Common rectangle                                                            */
/*                                                                            */
/* 모든 root screen / sub-screen renderer 는 이 구조체 한 개만 받아서            */
/* "내가 그릴 수 있는 영역" 을 해석한다.                                       */
/* -------------------------------------------------------------------------- */
typedef struct
{
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
} ui_rect_t;

/* -------------------------------------------------------------------------- */
/* Layout modes                                                                */
/*                                                                            */
/* UI 엔진은 root screen 이 아래 4가지 중 어떤 layout policy 를 쓰는지만 알면    */
/* status bar / bottom bar / main viewport 을 자동으로 합성할 수 있다.          */
/* -------------------------------------------------------------------------- */
typedef enum
{
    /* status bar 는 있고, 본문은 LCD 맨 아래까지 확장된다.                    */
    UI_LAYOUT_MODE_TOP_EXTENDED_NO_BOTTOM = 0,

    /* status bar 는 있고, bottom bar 는 overlay 로 잠깐 올라온다.             */
    UI_LAYOUT_MODE_TOP_EXTENDED_OVERLAY = 1,

    /* status bar 와 bottom bar 가 둘 다 고정 표시된다.                        */
    UI_LAYOUT_MODE_TOP_BOTTOM_FIXED = 2,

    /* status bar / bottom bar 없이 LCD 240x128 전체를 본문이 사용한다.        */
    UI_LAYOUT_MODE_FULLSCREEN = 3,

    UI_LAYOUT_MODE_COUNT
} ui_layout_mode_t;

/* -------------------------------------------------------------------------- */
/* Root screens                                                                */
/*                                                                            */
/* 기존 TEST / DEBUG / ENGINE OIL / GPS root 들은 F3 부트 분기용 legacy profile  */
/* 에서만 살아 있고, 정상 부팅 profile 에서는 UI_SCREEN_VARIO 만 메인 root 로     */
/* 사용한다.                                                                    */
/*                                                                            */
/* enum 값 호환성을 위해 기존 TEST=0 은 그대로 유지하고, VARIO 는 뒤에 추가한다. */
/* -------------------------------------------------------------------------- */
typedef enum
{
    UI_SCREEN_TEST = 0,
    UI_SCREEN_DEBUG_LEGACY,
    UI_SCREEN_ENGINE_OIL_INTERVAL,
    UI_SCREEN_GPS,
    UI_SCREEN_VARIO,
    UI_SCREEN_COUNT
} ui_screen_id_t;

/* -------------------------------------------------------------------------- */
/* Recording icon state                                                        */
/* -------------------------------------------------------------------------- */
typedef enum
{
    UI_RECORD_STATE_STOP = 0,
    UI_RECORD_STATE_REC  = 1,
    UI_RECORD_STATE_PAUSE = 2
} ui_record_state_t;

/* -------------------------------------------------------------------------- */
/* Bluetooth stub state                                                        */
/* -------------------------------------------------------------------------- */
typedef enum
{
    UI_BT_STUB_OFF   = 0,
    UI_BT_STUB_ON    = 1,
    UI_BT_STUB_BLINK = 2
} ui_bluetooth_stub_state_t;

#ifdef __cplusplus
}
#endif

#endif /* UI_TYPES_H */
