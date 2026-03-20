#ifndef VARIO_STATE_H
#define VARIO_STATE_H

#include "APP_STATE.h"

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  주 화면/설정 상태머신 mode                                                 */
/*                                                                            */
/*  사용자의 요구대로 상태머신 뼈대는 유지한다.                                */
/*  mode 값 추가/삭제 없이, 기존 6개 모드의 의미만 더 풍부하게 채운다.         */
/* -------------------------------------------------------------------------- */
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

/* -------------------------------------------------------------------------- */
/*  SETTING root menu item                                                    */
/*                                                                            */
/*  enum 값 개수도 유지한다.                                                   */
/*  의미는 아래처럼 재해석한다.                                                */
/*  - QUICKSET     : Flight/Audio/Instruments                                 */
/*  - VALUESETTING : Graphics                                                 */
/*  - AUDIO        : root 에서 audio on/off quick toggle                      */
/*  - DEBUG        : raw overlay toggle                                       */
/* -------------------------------------------------------------------------- */
typedef enum
{
    VARIO_SETTING_MENU_QUICKSET = 0u,
    VARIO_SETTING_MENU_VALUESETTING,
    VARIO_SETTING_MENU_AUDIO,
    VARIO_SETTING_MENU_DEBUG,
    VARIO_SETTING_MENU_COUNT
} vario_setting_menu_item_t;

#ifndef VARIO_TRAIL_MAX_POINTS
#define VARIO_TRAIL_MAX_POINTS 96u
#endif

typedef struct
{
    uint32_t last_task_ms;

    /* ---------------------------------------------------------------------- */
    /*  APP_STATE snapshot 복사본                                              */
    /*                                                                            */
    /*  애플리케이션 레이어의 경계 규칙을 지키기 위해                           */
    /*  저수준 드라이버를 직접 읽지 않고, APP_STATE snapshot 만 복사해서 쓴다.   */
    /* ---------------------------------------------------------------------- */
    app_gy86_state_t     gy86;
    app_gps_state_t      gps;
    app_ds18b20_state_t  ds18b20;
    app_altitude_state_t altitude;
    app_bike_state_t     bike;
    app_clock_state_t    clock;

    /* ---------------------------------------------------------------------- */
    /*  validity latch                                                          */
    /* ---------------------------------------------------------------------- */
    bool     baro_valid;
    bool     gps_valid;
    bool     temp_valid;
    bool     altitude_valid;
    bool     heading_valid;
    bool     clock_valid;
    bool     derived_valid;
    bool     flight_active;
    bool     trail_valid;

    /* ---------------------------------------------------------------------- */
    /*  원시/중간 표시값                                                        */
    /* ---------------------------------------------------------------------- */
    int32_t  pressure_hpa_x100;
    int32_t  temperature_c_x100;

    float    pressure_hpa;
    float    temperature_c;
    float    gps_altitude_m;

    /* ---------------------------------------------------------------------- */
    /*  페이지/렌더러가 직접 참조하는 공개 표시값                               */
    /*                                                                            */
    /*  baro_altitude_m / baro_vario_mps                                        */
    /*  - 화면에 실제로 뿌리는 5Hz publish 값                                   */
    /*  - 고도는 1m, vario 는 0.1m/s resolution 으로 양자화한다.               */
    /*                                                                            */
    /*  alt1_absolute_m                                                         */
    /*  - Flytec ALT1 개념: sea-level / absolute altitude                       */
    /*                                                                            */
    /*  alt2_relative_m                                                         */
    /*  - Flytec ALT2 개념: 사용자가 캡처한 기준고도 대비 상대고도               */
    /*                                                                            */
    /*  alt3_accum_gain_m                                                       */
    /*  - Flytec ALT3 개념: 누적 상승고도                                       */
    /* ---------------------------------------------------------------------- */
    float    baro_altitude_m;
    float    baro_vario_mps;
    float    ground_speed_kmh;
    float    heading_deg;
    float    max_top_vario_mps;
    float    alt1_absolute_m;
    float    alt2_relative_m;
    float    alt3_accum_gain_m;

    /* ---------------------------------------------------------------------- */
    /*  내부 필터 상태                                                          */
    /* ---------------------------------------------------------------------- */
    float    raw_selected_altitude_m;
    float    raw_selected_vario_mps;
    float    filtered_altitude_m;
    float    filtered_vario_mps;
    float    filtered_ground_speed_kmh;
    float    observer_velocity_mps;
    float    last_measured_altitude_m;
    float    last_accum_altitude_m;
    float    last_heading_deg;

    /* ---------------------------------------------------------------------- */
    /*  timestamp / publish / flight timer                                      */
    /* ---------------------------------------------------------------------- */
    uint8_t  local_hour;
    uint8_t  local_min;
    uint8_t  local_sec;
    uint8_t  heading_source;

    uint32_t last_baro_sample_count;
    uint32_t last_gps_host_time_ms;
    uint32_t last_temp_sample_count;
    uint32_t last_altitude_update_ms;
    uint32_t last_publish_ms;
    uint32_t flight_takeoff_candidate_ms;
    uint32_t flight_landing_candidate_ms;
    uint32_t flight_start_ms;
    uint32_t flight_time_s;

    /* ---------------------------------------------------------------------- */
    /*  breadcrumb trail ring buffer                                           */
    /*                                                                            */
    /*  page 2 는 north-up trail 이므로,                                        */
    /*  각 점의 절대 lat/lon 을 저장해 두고 화면 그릴 때 현재 위치 기준         */
    /*  ENU 근사로 dx/dy 로 바꿔 사용한다.                                      */
    /* ---------------------------------------------------------------------- */
    uint8_t  trail_head;
    uint8_t  trail_count;
    int32_t  trail_lat_e7[VARIO_TRAIL_MAX_POINTS];
    int32_t  trail_lon_e7[VARIO_TRAIL_MAX_POINTS];
    uint32_t trail_stamp_ms[VARIO_TRAIL_MAX_POINTS];
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

/* -------------------------------------------------------------------------- */
/*  누적치 / 비행 상태 reset API                                              */
/*                                                                            */
/*  설정 페이지의 one-shot action 항목이 호출한다.                              */
/* -------------------------------------------------------------------------- */
void Vario_State_ResetAccumulatedGain(void);
void Vario_State_ResetFlightMetrics(void);

void Vario_State_RequestRedraw(void);
void Vario_State_ClearRedrawRequest(void);
bool Vario_State_IsRedrawRequested(void);

#ifdef __cplusplus
}
#endif

#endif /* VARIO_STATE_H */
