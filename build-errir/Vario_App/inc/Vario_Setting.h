#ifndef VARIO_SETTING_H
#define VARIO_SETTING_H

#include "u8g2.h"
#include "Vario_Button.h"

#ifdef __cplusplus
extern "C" {
#endif

void Vario_Setting_Render(u8g2_t *u8g2, const vario_buttonbar_t *buttonbar);

#ifdef __cplusplus
}
#endif

#endif /* VARIO_SETTING_H */
