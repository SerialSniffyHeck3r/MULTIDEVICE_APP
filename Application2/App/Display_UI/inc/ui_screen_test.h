#ifndef UI_SCREEN_TEST_MODULE_H
#define UI_SCREEN_TEST_MODULE_H

#include "ui_types.h"
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  Test screen renderer                                                       */
/*                                                                            */
/*  이 화면은 새 UI 엔진 위에서 동작하는 인터페이스 테스트용 화면이다.          */
/*  - 큰 숫자 20Hz 증가 카운터                                                 */
/*  - 느린 깜빡임 패널                                                         */
/*  - 이동하는 동적 요소                                                       */
/*  - cute / toast / popup 아이콘 예시                                         */
/* -------------------------------------------------------------------------- */
void UI_ScreenTest_Draw(u8g2_t *u8g2,
                        const ui_rect_t *viewport,
                        uint32_t live_counter_20hz,
                        bool updates_paused,
                        bool blink_phase_on,
                        uint8_t cute_icon_index,
                        ui_layout_mode_t layout_mode);

#ifdef __cplusplus
}
#endif

#endif /* UI_SCREEN_TEST_MODULE_H */
