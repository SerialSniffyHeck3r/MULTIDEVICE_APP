#ifndef VARIO_SCREEN3_H
#define VARIO_SCREEN3_H

#include "u8g2.h"
#include "Vario_Button.h"

#ifdef __cplusplus
extern "C" {
#endif

void Vario_Screen3_Render(u8g2_t *u8g2, const vario_buttonbar_t *buttonbar);

#ifdef __cplusplus
}
#endif

#endif /* VARIO_SCREEN3_H */
