#ifndef UI_ENGINE_MODULE_H
#define UI_ENGINE_MODULE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  Public root API                                                            */
/* -------------------------------------------------------------------------- */
void UI_Engine_Init(void);
void UI_Engine_Task(uint32_t now_ms);
void UI_Engine_EarlyBootDraw(void);
void UI_Engine_RequestRedraw(void);
void UI_Engine_OnFrameTickFromISR(void);
void UI_Engine_OnBoardDebugButtonIrq(uint32_t now_ms);

/* -------------------------------------------------------------------------- */
/*  Explicit screen entry helpers                                              */
/*                                                                            */
/*  GPS 화면은 "이전 메뉴로 돌아가기" 요구가 있으므로                           */
/*  엔진이 현재 root screen을 기억한 뒤 진입시키는 public helper를 둔다.       */
/*  앞으로 다른 메뉴에서도 이 함수 하나만 호출하면                            */
/*  GPS -> F1 -> 원래 화면 복귀 흐름을 재사용할 수 있다.                       */
/* -------------------------------------------------------------------------- */
void UI_Engine_EnterGpsScreen(void);

/* -------------------------------------------------------------------------- */
/*  Small state setters                                                        */
/*                                                                            */
/*  현재 단계에서는 APP_STATE를 건드리지 않고도 상단바 스텁을 외부에서           */
/*  바꿀 수 있게 최소 setter를 제공한다.                                       */
/* -------------------------------------------------------------------------- */
void UI_Engine_SetRecordState(uint8_t record_state);
void UI_Engine_SetImperialUnits(uint8_t enabled);
void UI_Engine_SetBluetoothStubState(uint8_t state);

#ifdef __cplusplus
}
#endif

#endif /* UI_ENGINE_MODULE_H */
