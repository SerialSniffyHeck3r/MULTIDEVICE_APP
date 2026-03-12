#ifndef U8G2_UC1608_STM32_H
#define U8G2_UC1608_STM32_H

#include "main.h"
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

void U8G2_UC1608_DrawTestScreen(void);
void U8G2_UC1608_EnableSmartUpdate(uint8_t enable);
void U8G2_UC1608_EnableFrameLimit(uint8_t enable);
void U8G2_UC1608_FrameTickFromISR(void);
void U8G2_UC1608_CommitBuffer(void);
void U8G2_UC1608_Init(void);
u8g2_t *U8G2_UC1608_GetHandle(void);

#ifdef __cplusplus
}
#endif

#endif /* U8G2_UC1608_STM32_H */
