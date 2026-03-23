#include "Vario_Screen2.h"

#include "Vario_Display_Common.h"

/* -------------------------------------------------------------------------- */
/* SCREEN 2 wrapper                                                            */
/*                                                                            */
/* Screen 2 는 Screen 1 의 상단/하단/사이드 바 shell 을 공유하되,                 */
/* 중앙 원형 나침반 대신 breadcrumb GPS trail 배경을 사용하는 페이지다.          */
/*                                                                            */
/* trail 배경은 공통 renderer 내부에서 좌/우 14px bar 를 제외한 영역에만         */
/* 렌더되도록 되어 있다.                                                       */
/* -------------------------------------------------------------------------- */
void Vario_Screen2_Render(u8g2_t *u8g2, const vario_buttonbar_t *buttonbar)
{
    (void)buttonbar;

    Vario_Display_RenderFlightPage(u8g2, VARIO_FLIGHT_PAGE_SCREEN_2_TRAIL);
}
