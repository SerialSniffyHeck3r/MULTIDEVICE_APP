#ifndef UI_BOTTOMBAR_MODULE_H
#define UI_BOTTOMBAR_MODULE_H

#include "ui_types.h"
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  Bottom bar modes                                                           */
/* -------------------------------------------------------------------------- */
typedef enum
{
    UI_BOTTOMBAR_MODE_BUTTONS = 0,
    UI_BOTTOMBAR_MODE_MESSAGE = 1
} ui_bottombar_mode_t;

/* -------------------------------------------------------------------------- */
/*  Segment index                                                              */
/*                                                                            */
/*  물리 버튼 1~6과 화면 하단 6칸이 그대로 대응되도록 고정한다.                  */
/* -------------------------------------------------------------------------- */
typedef enum
{
    UI_FKEY_1 = 0,
    UI_FKEY_2,
    UI_FKEY_3,
    UI_FKEY_4,
    UI_FKEY_5,
    UI_FKEY_6,

    UI_FKEY_COUNT
} ui_fkey_t;

/* -------------------------------------------------------------------------- */
/*  Button draw flags                                                          */
/* -------------------------------------------------------------------------- */
#define UI_BOTTOMBAR_FLAG_DIVIDER   0x01u
#define UI_BOTTOMBAR_FLAG_INVERT    0x02u
#define UI_BOTTOMBAR_FLAG_ICON_4PX  0x04u

typedef struct
{
    const char *text;
    const uint8_t *icon;
    uint8_t icon_w;
    uint8_t icon_h;
    uint8_t flags;
} ui_bottom_button_t;

typedef struct
{
    const char *text;
    const uint8_t *icon;
    uint8_t icon_w;
    uint8_t icon_h;
} ui_bottom_message_t;

void UI_BottomBar_Init(void);
void UI_BottomBar_SetMode(ui_bottombar_mode_t mode);
void UI_BottomBar_SetButton(ui_fkey_t key, const char *text, uint8_t flags);
void UI_BottomBar_SetButtonIcon4(ui_fkey_t key,
                                 const uint8_t *icon,
                                 uint8_t icon_w,
                                 uint8_t flags);
void UI_BottomBar_SetMessage(const ui_bottom_message_t *msg);

/* -------------------------------------------------------------------------- */
/*  Draw                                                                       */
/*                                                                            */
/*  pressed_mask는 Button_GetPressedMask()의 결과를 그대로 넣는다.             */
/*  - bit0 -> button1                                                          */
/*  - bit5 -> button6                                                          */
/* -------------------------------------------------------------------------- */
void UI_BottomBar_Draw(u8g2_t *u8g2, uint32_t pressed_mask);

#ifdef __cplusplus
}
#endif

#endif /* UI_BOTTOMBAR_MODULE_H */
