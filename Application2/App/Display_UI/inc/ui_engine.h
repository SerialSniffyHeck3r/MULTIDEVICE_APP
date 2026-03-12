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
