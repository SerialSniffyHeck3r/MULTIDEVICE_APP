#ifndef VARIO_BUTTON_H
#define VARIO_BUTTON_H

#include "Vario_State.h"

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    const char *f1;
    const char *f2;
    const char *f3;
    const char *f4;
    const char *f5;
    const char *f6;
} vario_buttonbar_t;

void Vario_Button_Init(void);
void Vario_Button_Task(uint32_t now_ms);
void Vario_Button_GetButtonBar(vario_mode_t mode, vario_buttonbar_t *out_bar);

#ifdef __cplusplus
}
#endif

#endif /* VARIO_BUTTON_H */
