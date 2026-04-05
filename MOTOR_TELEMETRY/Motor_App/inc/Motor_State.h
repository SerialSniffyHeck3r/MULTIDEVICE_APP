#ifndef MOTOR_STATE_H
#define MOTOR_STATE_H

#include "Motor_Model.h"

#ifdef __cplusplus
extern "C" {
#endif

void Motor_State_Init(void);
void Motor_State_Task(uint32_t now_ms);
void Motor_State_ApplyGpsSimulator(uint32_t now_ms);
const motor_state_t *Motor_State_Get(void);

/* -------------------------------------------------------------------------- */
/*  runtime-owner 전용 mutable view                                           */
/*                                                                            */
/*  주의                                                                      */
/*  - 이 포인터는 breadcrumb/session/maintenance/runtime flag 같은             */
/*    "프레임성 런타임" 갱신에만 사용한다.                                    */
/*  - state.settings 는 canonical owner가 Motor_Settings 저장소이므로         */
/*    이 포인터를 통해 직접 수정하지 않는다.                                  */
/*  - persistent 설정 변경은                                                  */
/*      Motor_Settings_GetMutable() -> 수정 -> Motor_Settings_Commit()        */
/*    경로를 사용하고, 필요하면                                               */
/*      Motor_State_RefreshSettingsSnapshot()                                  */
/*    으로 local runtime copy를 다시 맞춘다.                                  */
/* -------------------------------------------------------------------------- */
motor_state_t *Motor_State_GetMutable(void);

/* Motor_Settings canonical store -> Motor_State local snapshot 재동기화 */
void Motor_State_RefreshSettingsSnapshot(void);

void Motor_State_RequestRedraw(void);
void Motor_State_ShowToast(const char *text, uint32_t hold_ms);

/* -------------------------------------------------------------------------- */
/*  공용 popup facade                                                         */
/*                                                                            */
/*  목적                                                                       */
/*  - Motor_App 상위 모듈이 UI raster 세부사항을 직접 만지지 않게 한다.        */
/*  - popup의 실제 구현은 Display_UI 공용 모듈이 담당하고,                    */
/*    Motor_App는 이 facade만 호출한다.                                       */
/*  - 향후 popup 정책을 바꾸더라도 호출 지점을 Motor_State 경계로            */
/*    묶어 두면 수정 반경을 최소화할 수 있다.                                 */
/* -------------------------------------------------------------------------- */
void Motor_State_ShowPopup(const char *title,
                           const char *line1,
                           const char *line2,
                           uint32_t hold_ms);
void Motor_State_HidePopup(void);

void Motor_State_SetScreen(motor_screen_t screen);
void Motor_State_StorePreviousDriveScreen(motor_screen_t screen);
void Motor_State_RequestMarker(void);
void Motor_State_RequestRecordToggle(void);
void Motor_State_RequestRecordStop(void);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_STATE_H */
