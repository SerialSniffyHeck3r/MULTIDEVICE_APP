#ifndef VARIO_SCREEN2_H
#define VARIO_SCREEN2_H

#include "u8g2.h"
#include "Vario_Button.h"

#ifdef __cplusplus
extern "C" {
#endif

void Vario_Screen2_Render(u8g2_t *u8g2, const vario_buttonbar_t *buttonbar);

#ifdef __cplusplus
}
#endif

#endif /* VARIO_SCREEN2_H */
