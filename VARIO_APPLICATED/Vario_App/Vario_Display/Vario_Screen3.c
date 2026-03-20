#include "Vario_Screen3.h"

#include "Vario_Display_Common.h"

#include <stdio.h>

/* -------------------------------------------------------------------------- */
/*  Screen 3 stub page                                                         */
/*                                                                            */
/*  사용자의 요구사항                                                          */
/*  - 페이지 3은 이번 세션에서 실제 계기 그래픽을 넣지 않고 stub 으로 유지한다. */
/*                                                                            */
/*  유지 이유                                                                  */
/*  - 상태머신 구조는 그대로 보존한다.                                         */
/*  - SCREEN_3 mode 는 살아 있으므로 버튼 흐름/화면 전환 구조가 깨지지 않는다.  */
/*  - 추후 glide computer, thermal assistant, final-glide page 같은 고급      */
/*    페이지를 이 자리에 이식할 수 있다.                                      */
/*                                                                            */
/*  조정 포인트                                                                */
/*  - 타이틀 위치를 바꾸고 싶으면 Vario_Display_DrawPageTitle() 를 수정하거나   */
/*    아래의 y baseline 숫자를 조절한다.                                      */
/*  - 가운데 메시지의 폰트 크기를 바꾸고 싶으면 u8g2_SetFont() 의 폰트를        */
/*    변경한다.                                                               */
/* -------------------------------------------------------------------------- */
void Vario_Screen3_Render(u8g2_t *u8g2, const vario_buttonbar_t *buttonbar)
{
    const vario_viewport_t *v;
    char                    line1[24];
    char                    line2[32];

    (void)buttonbar;

    v = Vario_Display_GetFullViewport();

    Vario_Display_DrawPageTitle(u8g2, v, "PAGE 3", "STUB");

    /* ---------------------------------------------------------------------- */
    /* 중앙 stub 메시지                                                        */
    /* - 이 페이지가 고의적으로 비워져 있음을 명확히 보여 준다.                */
    /* - 본문은 full viewport 중심에 배치한다.                                 */
    /* ---------------------------------------------------------------------- */
    snprintf(line1, sizeof(line1), "RESERVED");
    snprintf(line2, sizeof(line2), "for future graphics");

    u8g2_SetFont(u8g2, u8g2_font_10x20_mf);
    Vario_Display_DrawTextCentered(u8g2,
                                   (int16_t)(v->x + (v->w / 2)),
                                   (int16_t)(v->y + (v->h / 2) - 4),
                                   line1);

    u8g2_SetFont(u8g2, u8g2_font_6x12_mf);
    Vario_Display_DrawTextCentered(u8g2,
                                   (int16_t)(v->x + (v->w / 2)),
                                   (int16_t)(v->y + (v->h / 2) + 14),
                                   line2);

    Vario_Display_DrawRawOverlay(u8g2, v);
}
