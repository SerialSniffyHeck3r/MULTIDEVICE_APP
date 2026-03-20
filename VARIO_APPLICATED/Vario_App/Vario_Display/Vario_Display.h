#ifndef VARIO_DISPLAY_H
#define VARIO_DISPLAY_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void Vario_Display_Init(void);
void Vario_Display_Task(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* VARIO_DISPLAY_H */
