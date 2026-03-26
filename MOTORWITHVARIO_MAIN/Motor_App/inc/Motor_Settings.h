#ifndef MOTOR_SETTINGS_H
#define MOTOR_SETTINGS_H

#include "Motor_Model.h"

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void Motor_Settings_Init(void);
void Motor_Settings_Copy(motor_settings_t *dst);
const motor_settings_t *Motor_Settings_Get(void);
motor_settings_t *Motor_Settings_GetMutable(void);
void Motor_Settings_Commit(void);
void Motor_Settings_ResetToDefaults(void);

/* -------------------------------------------------------------------------- */
/*  Settings UI helper API                                                     */
/*                                                                            */
/*  Vario_App의 settings 구조를 Motor_App에서 재사용하기 위해,                 */
/*  category/row metadata와 label/value 문자열 생성 책임을 Settings 모듈로     */
/*  모은다.                                                                   */
/*                                                                            */
/*  효과                                                                       */
/*  - UI renderer는 "어떻게 그릴지"만 안다.                                   */
/*  - Button handler는 "어느 row를 움직이는지"만 안다.                        */
/*  - 실제 label/value 문구와 row count는 Settings가 단일 진실이 된다.         */
/* -------------------------------------------------------------------------- */
uint8_t Motor_Settings_GetRowCount(motor_screen_t screen);
const char *Motor_Settings_GetScreenTitle(motor_screen_t screen);
const char *Motor_Settings_GetScreenSubtitle(motor_screen_t screen);
void Motor_Settings_GetRowText(const motor_state_t *state,
                               motor_screen_t screen,
                               uint8_t row,
                               char *out_label,
                               size_t label_size,
                               char *out_value,
                               size_t value_size);

#ifdef __cplusplus
}
#endif

#endif /* MOTOR_SETTINGS_H */
