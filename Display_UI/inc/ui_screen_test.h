#ifndef UI_SCREEN_TEST_MODULE_H
#define UI_SCREEN_TEST_MODULE_H

#include "ui_types.h"
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

void UI_ScreenTest_Draw(u8g2_t *u8g2,
                        const ui_rect_t *viewport,
                        uint32_t frame_tick_20hz,
                        bool blink_paused,
                        uint8_t cute_icon_index,
                        ui_layout_mode_t layout_mode);

#ifdef __cplusplus
}
#endif

#endif /* UI_SCREEN_TEST_MODULE_H */
