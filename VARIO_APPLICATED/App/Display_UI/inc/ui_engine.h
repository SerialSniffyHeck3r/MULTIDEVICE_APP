#ifndef UI_ENGINE_MODULE_H
#define UI_ENGINE_MODULE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* UI engine boot profile                                                      */
/*                                                                            */
/* 이 값은 "부팅 직후 어떤 root screen family 를 기본값으로 쓸 것인가" 를 결정한다.*/
/*                                                                            */
/* - UI_ENGINE_BOOT_MODE_VARIO  : 정상 부팅. VARIO root screen 으로 시작한다.    */
/* - UI_ENGINE_BOOT_MODE_LEGACY : F3 held boot. TEST / DEBUG / GPS legacy 묶음. */
/*                                                                            */
/* 주의                                                                         */
/* - 이 값은 main.c 에서 Button_Init() 직후 한번 latch 해서                      */
/*   UI_Engine_Init() 전에 넣는 용도로 설계했다.                                */
/* - 런타임 중 profile 을 바꾸는 기능은 의도하지 않는다.                         */
/* -------------------------------------------------------------------------- */
typedef enum
{
    UI_ENGINE_BOOT_MODE_VARIO  = 0u,
    UI_ENGINE_BOOT_MODE_LEGACY = 1u
} ui_engine_boot_mode_t;

/* -------------------------------------------------------------------------- */
/* Boot profile API                                                            */
/* -------------------------------------------------------------------------- */
void UI_Engine_SetBootMode(ui_engine_boot_mode_t mode);
ui_engine_boot_mode_t UI_Engine_GetBootMode(void);
uint8_t UI_Engine_IsLegacyBootMode(void);

/* -------------------------------------------------------------------------- */
/* Public root API                                                             */
/* -------------------------------------------------------------------------- */
void UI_Engine_Init(void);
void UI_Engine_Task(uint32_t now_ms);
void UI_Engine_EarlyBootDraw(void);
void UI_Engine_RequestRedraw(void);
void UI_Engine_OnFrameTickFromISR(void);
void UI_Engine_OnBoardDebugButtonIrq(uint32_t now_ms);

/* -------------------------------------------------------------------------- */
/* Explicit screen entry helper                                                */
/*                                                                            */
/* GPS 화면은 legacy profile 전용 화면으로 유지한다.                            */
/* 정상 VARIO boot profile 에서 이 함수를 호출해도 아무 동작도 하지 않는다.       */
/* -------------------------------------------------------------------------- */
void UI_Engine_EnterGpsScreen(void);

/* -------------------------------------------------------------------------- */
/* Small state setters                                                         */
/* -------------------------------------------------------------------------- */
void UI_Engine_SetRecordState(uint8_t record_state);
void UI_Engine_SetImperialUnits(uint8_t enabled);
void UI_Engine_SetBluetoothStubState(uint8_t state);

#ifdef __cplusplus
}
#endif

#endif /* UI_ENGINE_MODULE_H */
