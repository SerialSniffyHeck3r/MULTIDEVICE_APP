#ifndef VARIO_DISPLAY_H
#define VARIO_DISPLAY_H

#include "u8g2.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* VARIO display facade                                                        */
/*                                                                            */
/* 이 모듈은 더 이상 frame token / buffer clear / buffer commit 을 소유하지      */
/* 않는다.                                                                      */
/*                                                                            */
/* 이제 실제 프레임 생명주기는 UI 엔진이 소유하고,                              */
/* 이 모듈은 "현재 VARIO mode 에 맞는 본문 renderer 하나를 호출" 하는 역할만     */
/* 맡는다.                                                                      */
/* -------------------------------------------------------------------------- */
void Vario_Display_Init(void);

/* deprecated compatibility entry                                              */
/* - 예전 코드와 링크 호환을 위해 남겨 둔다.                                   */
/* - 직접 렌더링은 하지 않으며, now_ms 는 사용하지 않는다.                      */
void Vario_Display_Task(uint32_t now_ms);

/* -------------------------------------------------------------------------- */
/* 본문 renderer dispatch                                                      */
/*                                                                            */
/* 전제                                                                         */
/* - UI 엔진이 이미 buffer clear 를 수행했다.                                  */
/* - UI 엔진 bridge 가 현재 프레임의 viewport 를                                */
/*   Vario_Display_SetViewports() 로 먼저 공급했다.                            */
/* - status bar / bottom bar 는 절대 여기서 그리지 않는다.                     */
/* -------------------------------------------------------------------------- */
void Vario_Display_RenderCurrent(u8g2_t *u8g2);

#ifdef __cplusplus
}
#endif

#endif /* VARIO_DISPLAY_H */
