#ifndef UI_DEBUG_LEGACY_H
#define UI_DEBUG_LEGACY_H

#include <stdint.h>
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  Legacy debug screen                                                        */
/*                                                                            */
/*  현재 main.c에 있던 GPS/SENSOR/BRIGHTNESS/SD/BT/AUDIO/FLASH/CLOCK           */
/*  디버그 페이지를 새 UI 엔진 위로 옮긴 화면이다.                              */
/* -------------------------------------------------------------------------- */
void UI_DebugLegacy_Init(void);
void UI_DebugLegacy_Task(uint32_t now_ms);
void UI_DebugLegacy_Draw(u8g2_t *u8g2, uint32_t now_ms);
void UI_DebugLegacy_OnBoardButtonIrq(uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* UI_DEBUG_LEGACY_H */
