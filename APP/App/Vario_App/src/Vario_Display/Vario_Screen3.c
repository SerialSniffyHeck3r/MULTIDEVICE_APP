#include "Vario_Screen3.h"

#include "Vario_Display_Common.h"

/* -------------------------------------------------------------------------- */
/* SCREEN 3 wrapper                                                            */
/*                                                                            */
/* 사용자의 요구대로 Screen 3 는 아직 기능 stub 상태로 남겨 둔다.               */
/* 단, 전체 shell 은 이미 Screen 1/2 와 동일한 새 레이아웃을 공유한다.          */
/*                                                                            */
/* 즉, 좌/우 사이드 바, 상단 시계/고도, 하단 숫자 블록 구조는 유지되고,          */
/* 중앙부만 추후 기능을 덧씌우기 좋은 placeholder 로 남는다.                    */
/* -------------------------------------------------------------------------- */
void Vario_Screen3_Render(u8g2_t *u8g2, const vario_buttonbar_t *buttonbar)
{
    (void)buttonbar;

    Vario_Display_RenderFlightPage(u8g2, VARIO_FLIGHT_PAGE_SCREEN_3_STUB);
}
