#ifndef UI_TYPES_H
#define UI_TYPES_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  Display-wide fixed geometry                                                */
/*                                                                            */
/*  이 UI 엔진은 현재 제품의 LCD 해상도(240x128)를 절대 픽셀 기준으로 사용한다. */
/*  상단바/하단바 규격도 이 값을 기준으로 고정한다.                             */
/* -------------------------------------------------------------------------- */
#define UI_LCD_W                 240
#define UI_LCD_H                 128

/* -------------------------------------------------------------------------- */
/*  Bar geometry                                                               */
/*                                                                            */
/*  업로드된 프로토타입 규격을 그대로 가져온다.                                */
/*  - status bar  : 7 px                                                       */
/*  - bottom bar  : 8 px                                                       */
/*  - 각 바와 본문 사이에는 1 px gap을 둔다.                                   */
/* -------------------------------------------------------------------------- */
#define UI_STATUSBAR_H           7
#define UI_STATUSBAR_GAP_H       1
#define UI_BOTTOMBAR_H           8
#define UI_BOTTOMBAR_GAP_H       1

/* -------------------------------------------------------------------------- */
/*  Overlay policy / timing                                                    */
/*                                                                            */
/*  overlay형 하단바는 본문을 덮어쓰며 잠깐 올라오는 형태다.                    */
/*  이 타임아웃은 이후 프로젝트 성격에 맞게 조절하기 쉽도록 define으로 둔다.   */
/* -------------------------------------------------------------------------- */
#ifndef UI_BOTTOMBAR_OVERLAY_TIMEOUT_MS
#define UI_BOTTOMBAR_OVERLAY_TIMEOUT_MS   2200u
#endif

#ifndef UI_TOAST_DEFAULT_TIMEOUT_MS
#define UI_TOAST_DEFAULT_TIMEOUT_MS       1500u
#endif

#ifndef UI_POPUP_DEFAULT_TIMEOUT_MS
#define UI_POPUP_DEFAULT_TIMEOUT_MS       2500u
#endif

/* -------------------------------------------------------------------------- */
/*  Common rectangle                                                           */
/*                                                                            */
/*  모든 화면/뷰포트는 이 구조체 한 개로 전달한다.                              */
/* -------------------------------------------------------------------------- */
typedef struct
{
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
} ui_rect_t;

/* -------------------------------------------------------------------------- */
/*  Layout modes                                                               */
/*                                                                            */
/*  요구사항에서 정리한 4가지 표시 모드를 그대로 enum으로 정의한다.             */
/* -------------------------------------------------------------------------- */
typedef enum
{
    UI_LAYOUT_MODE_TOP_EXTENDED_NO_BOTTOM = 0,      /* 상단바만 있고, 본문이 하단까지 확장 */
    UI_LAYOUT_MODE_TOP_EXTENDED_OVERLAY   = 1,      /* 상단바 + overlay형 하단바          */
    UI_LAYOUT_MODE_TOP_BOTTOM_FIXED       = 2,      /* 상단바 + 영구 하단바               */
    UI_LAYOUT_MODE_FULLSCREEN             = 3,      /* 상하단바 없는 전체 화면            */

    UI_LAYOUT_MODE_COUNT
} ui_layout_mode_t;

/* -------------------------------------------------------------------------- */
/*  Root screens                                                               */
/*                                                                            */
/*  지금 단계에서는 테스트 화면과 레거시 디버그 화면 2개만 엔진에 올린다.      */
/* -------------------------------------------------------------------------- */
typedef enum
{
    UI_SCREEN_TEST = 0,
    UI_SCREEN_DEBUG_LEGACY,

    UI_SCREEN_COUNT
} ui_screen_id_t;

/* -------------------------------------------------------------------------- */
/*  Recording icon state                                                       */
/*                                                                            */
/*  상단바 왼쪽 첫 아이콘의 의미를 외부에서 바꾸기 쉽게 별도 enum으로 둔다.     */
/* -------------------------------------------------------------------------- */
typedef enum
{
    UI_RECORD_STATE_STOP  = 0,
    UI_RECORD_STATE_REC   = 1,
    UI_RECORD_STATE_PAUSE = 2
} ui_record_state_t;

/* -------------------------------------------------------------------------- */
/*  Bluetooth stub state                                                       */
/*                                                                            */
/*  사용자가 요구한 "블루투스 상태 스텁" 표현용 enum이다.                       */
/*  아직 실제 연결 상태를 강하게 묶지 않고, 아이콘 정책만 정리한다.            */
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
