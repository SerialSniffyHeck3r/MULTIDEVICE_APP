#ifndef VARIO_AUDIO_H
#define VARIO_AUDIO_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

void Vario_Audio_Init(void);
void Vario_Audio_Task(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* VARIO_AUDIO_H */
