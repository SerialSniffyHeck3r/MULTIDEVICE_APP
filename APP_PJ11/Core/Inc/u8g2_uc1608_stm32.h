#ifndef U8G2_UC1608_STM32_H
#define U8G2_UC1608_STM32_H

#include "main.h"
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

void U8G2_UC1608_Init(void);
void U8G2_UC1608_DrawTestScreen(void);
void U8G2_UC1608_EnableSmartUpdate(uint8_t enable);
void U8G2_UC1608_EnableFrameLimit(uint8_t enable);
void U8G2_UC1608_FrameTickFromISR(void);

/* 새 프레임 토큰 획득 API
 * - 1: 이번 loop에서 실제로 snapshot/copy/draw/commit 해도 됨
 * - 0: 아직 이번 20Hz 슬롯이 안 왔으니 아무 것도 그리지 말 것 */
uint8_t U8G2_UC1608_TryAcquireFrameToken(void);

void U8G2_UC1608_CommitBuffer(void);
u8g2_t *U8G2_UC1608_GetHandle(void);

#ifdef __cplusplus
}
#endif

#endif /* U8G2_UC1608_STM32_H */
