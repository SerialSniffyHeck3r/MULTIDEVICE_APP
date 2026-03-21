#include "Vario_Screen1.h"

#include "Vario_Display_Common.h"

/* -------------------------------------------------------------------------- */
/* SCREEN 1 wrapper                                                            */
/*                                                                            */
/* 실제 UI 로직은 Vario_Display_Common.c 의 공통 flight page renderer 가 모두   */
/* 담당한다.                                                                   */
/*                                                                            */
/* 이 파일은 상태머신 dispatch 호환성을 유지하기 위한 매우 얇은 어댑터다.       */
/* 나중에 Screen 1 만 별도 동작을 추가하고 싶으면, 이 wrapper 에서 mode 전처리   */
/* 또는 draw 전/후 hook 만 덧붙이면 된다.                                      */
/* -------------------------------------------------------------------------- */
void Vario_Screen1_Render(u8g2_t *u8g2, const vario_buttonbar_t *buttonbar)
{
    /* 현재 메인 비행 화면은 더 이상 button bar 를 직접 그리지 않는다. */
    (void)buttonbar;

    Vario_Display_RenderFlightPage(u8g2, VARIO_FLIGHT_PAGE_SCREEN_1);
}
