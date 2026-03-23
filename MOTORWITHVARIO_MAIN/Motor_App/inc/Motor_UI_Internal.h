#ifndef MOTOR_UI_INTERNAL_H
#define MOTOR_UI_INTERNAL_H

#include "Motor_Model.h"
#include "ui_types.h"
#include "ui_statusbar.h"
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

const char *Motor_UI_GetScreenShortTitle(motor_screen_t screen);
void Motor_UI_BuildStatusBarModel(const motor_state_t *state, ui_statusbar_model_t *out_model);
void Motor_UI_ConfigureBottomBar(const motor_state_t *state);
void Motor_UI_ComputeViewport(u8g2_t *u8g2,
                              const motor_state_t *state,
                              ui_layout_mode_t layout_mode,
                              ui_rect_t *out_viewport);

void Motor_UI_DrawScreen_Main(u8g2_t *u8g2, const ui_rect_t *viewport, const motor_state_t *state);
void Motor_UI_DrawScreen_DataField(u8g2_t *u8g2, const ui_rect_t *viewport, const motor_state_t *state, uint8_t page_index);
void Motor_UI_DrawScreen_Corner(u8g2_t *u8g2, const ui_rect_t *viewport, const motor_state_t *state);
void Motor_UI_DrawScreen_Compass(u8g2_t *u8g2, const ui_rect_t *viewport, const motor_state_t *state);
void Motor_UI_DrawScreen_Breadcrumb(u8g2_t *u8g2, const ui_rect_t *viewport, const motor_state_t *state);
void Motor_UI_DrawScreen_Altitude(u8g2_t *u8g2, const ui_rect_t *viewport, const motor_state_t *state);
void Motor_UI_DrawScreen_Horizon(u8g2_t *u8g2, const ui_rect_t *viewport, const motor_state_t *state);
void Motor_UI_DrawScreen_Vehicle(u8g2_t *u8g2, const ui_rect_t *viewport, const motor_state_t *state);
void Motor_UI_DrawScreen_Menu(u8g2_t *u8g2, const ui_rect_t *viewport, const motor_state_t *state);
void Motor_UI_DrawScreen_Settings(u8g2_t *u8g2, const ui_rect_t *viewport, const motor_state_t *state);
void Motor_UI_DrawScreen_Stub(u8g2_t *u8g2,
                              const ui_rect_t *viewport,
                              const motor_state_t *state,
                              const char *title,
                              const char *line1,
                              const char *line2);

void Motor_UI_DrawLabeledValueBox(u8g2_t *u8g2,
                                  int16_t x,
                                  int16_t y,
                                  int16_t w,
                                  int16_t h,
                                  const char *label,
                                  const char *value,
                                  uint8_t large_font);
void Motor_UI_DrawHorizontalBar(u8g2_t *u8g2,
                                int16_t x,
                                int16_t y,
                                int16_t w,
                                int16_t h,
                                int32_t value,
                                int32_t min_value,
                                int32_t max_value,
                                const char *caption);
void Motor_UI_DrawCenteredTextBlock(u8g2_t *u8g2,
                                    const ui_rect_t *viewport,
                                    int16_t y,
                                    const char *title,
                                    const char *line1,
                                    const char *line2);
void Motor_UI_DrawViewportTitle(u8g2_t *u8g2, const ui_rect_t *viewport, const char *title);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_UI_INTERNAL_H */
