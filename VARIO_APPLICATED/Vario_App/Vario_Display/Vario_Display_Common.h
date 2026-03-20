#ifndef VARIO_DISPLAY_COMMON_H
#define VARIO_DISPLAY_COMMON_H

#include "u8g2.h"
#include "Vario_State.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
} vario_viewport_t;

const vario_viewport_t *Vario_Display_GetFullViewport(void);
const vario_viewport_t *Vario_Display_GetContentViewport(void);

void Vario_Display_ConfigureBottomBar(vario_mode_t mode);
void Vario_Display_DrawSharedBars(void);
void Vario_Display_DrawPageTitle(u8g2_t *u8g2,
                                 const vario_viewport_t *v,
                                 const char *title,
                                 const char *subtitle);
void Vario_Display_DrawMenuRow(u8g2_t *u8g2,
                               const vario_viewport_t *v,
                               int16_t y_baseline,
                               bool selected,
                               const char *label,
                               const char *value);
void Vario_Display_DrawKeyValueRow(u8g2_t *u8g2,
                                   const vario_viewport_t *v,
                                   int16_t y_baseline,
                                   const char *label,
                                   const char *value);
void Vario_Display_DrawTextRight(u8g2_t *u8g2,
                                 int16_t right_x,
                                 int16_t y_baseline,
                                 const char *text);
void Vario_Display_DrawTextCentered(u8g2_t *u8g2,
                                    int16_t center_x,
                                    int16_t y_baseline,
                                    const char *text);
void Vario_Display_DrawRawOverlay(u8g2_t *u8g2,
                                  const vario_viewport_t *v);

#ifdef __cplusplus
}
#endif

#endif /* VARIO_DISPLAY_COMMON_H */
