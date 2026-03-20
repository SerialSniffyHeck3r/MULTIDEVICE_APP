#ifndef VARIO_SCREEN1_H
#define VARIO_SCREEN1_H

#include "u8g2.h"
#include "Vario_Button.h"

#ifdef __cplusplus
extern "C" {
#endif

void Vario_Screen1_Render(u8g2_t *u8g2, const vario_buttonbar_t *buttonbar);

#ifdef __cplusplus
}
#endif

#endif /* VARIO_SCREEN1_H */
