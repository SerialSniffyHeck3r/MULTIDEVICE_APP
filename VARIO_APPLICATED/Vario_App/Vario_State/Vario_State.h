#ifndef VARIO_STATE_H
#define VARIO_STATE_H

#include "APP_STATE.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef enum
{
    VARIO_MODE_SCREEN_1 = 0u,
    VARIO_MODE_SCREEN_2,
    VARIO_MODE_SCREEN_3,
    VARIO_MODE_SETTING,
    VARIO_MODE_QUICKSET,
    VARIO_MODE_VALUESETTING,
    VARIO_MODE_COUNT
} vario_mode_t;

typedef enum
{
    VARIO_SETTING_MENU_QUICKSET = 0u,
    VARIO_SETTING_MENU_VALUESETTING,
    VARIO_SETTING_MENU_AUDIO,
    VARIO_SETTING_MENU_DEBUG,
    VARIO_SETTING_MENU_COUNT
} vario_setting_menu_item_t;

typedef struct
{
    uint32_t last_task_ms;

    /* ---------------------------------------------------------------------- */
    /*  APP_STATE 에서 복사해 온 sensor snapshot                               */
    /* ---------------------------------------------------------------------- */
    app_gy86_state_t    gy86;
    app_gps_state_t     gps;
    app_ds18b20_state_t ds18b20;

    /* ---------------------------------------------------------------------- */
    /*  validity latch                                                          */
    /* ---------------------------------------------------------------------- */
    bool     baro_valid;
    bool     gps_valid;
    bool     temp_valid;
    bool     derived_valid;

    /* ---------------------------------------------------------------------- */
    /*  원시 값을 그대로 복사해 둔 표시용 창고                                  */
    /* ---------------------------------------------------------------------- */
    int32_t  pressure_hpa_x100;
    int32_t  temperature_c_x100;

    /* ---------------------------------------------------------------------- */
    /*  렌더러가 바로 쓰는 파생 값                                              */
    /* ---------------------------------------------------------------------- */
    float    pressure_hpa;
    float    temperature_c;
    float    baro_altitude_m;
    float    baro_vario_mps;
    float    gps_altitude_m;
    float    ground_speed_kmh;

    /* ---------------------------------------------------------------------- */
    /*  변화 감지용 이력                                                        */
    /* ---------------------------------------------------------------------- */
    uint32_t last_baro_sample_count;
    uint32_t last_gps_host_time_ms;
    uint32_t last_temp_sample_count;
    uint32_t last_baro_timestamp_ms;

    /* ---------------------------------------------------------------------- */
    /*  필터 내부 상태                                                          */
    /* ---------------------------------------------------------------------- */
    float    last_raw_altitude_m;
    float    filtered_altitude_m;
    float    filtered_vario_mps;
    float    filtered_ground_speed_kmh;
} vario_runtime_t;

void Vario_State_Init(void);
void Vario_State_Task(uint32_t now_ms);

vario_mode_t Vario_State_GetMode(void);
void Vario_State_SetMode(vario_mode_t mode);
void Vario_State_ReturnToMain(void);
void Vario_State_EnterSettings(void);
void Vario_State_EnterQuickSet(void);
void Vario_State_EnterValueSetting(void);
void Vario_State_SelectPreviousMainScreen(void);
void Vario_State_SelectNextMainScreen(void);

uint8_t Vario_State_GetSettingsCursor(void);
uint8_t Vario_State_GetQuickSetCursor(void);
uint8_t Vario_State_GetValueSettingCursor(void);
void Vario_State_MoveSettingsCursor(int8_t direction);
void Vario_State_MoveQuickSetCursor(int8_t direction);
void Vario_State_MoveValueSettingCursor(int8_t direction);

const vario_runtime_t *Vario_State_GetRuntime(void);

void Vario_State_RequestRedraw(void);
void Vario_State_ClearRedrawRequest(void);
bool Vario_State_IsRedrawRequested(void);

#ifdef __cplusplus
}
#endif

#endif /* VARIO_STATE_H */
