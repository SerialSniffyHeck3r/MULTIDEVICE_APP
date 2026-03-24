#ifndef VARIO_STATE_H
#define VARIO_STATE_H

#include "APP_STATE.h"
#include "Vario_Settings.h"

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
/*  root settings 화면은 즉시 조절 가능한 대시보드 역할을 맡는다.              */
/*  - BRIGHTNESS : live backlight bar                                          */
/*  - VOLUME     : audio volume bar                                            */
/*  - DAMPING    : vario response bar                                          */
/*  - ALT2       : ALT2 의미 전환                                              */
/* -------------------------------------------------------------------------- */
typedef enum
{
    VARIO_SETTING_MENU_BRIGHTNESS = 0u,
    VARIO_SETTING_MENU_VOLUME,
    VARIO_SETTING_MENU_DAMPING,
    VARIO_SETTING_MENU_ALT2,
    VARIO_SETTING_MENU_COUNT
} vario_setting_menu_item_t;

#ifndef VARIO_TRAIL_MAX_POINTS
#define VARIO_TRAIL_MAX_POINTS 96u
#endif

#ifndef VARIO_HISTORY_MAX_SAMPLES
#define VARIO_HISTORY_MAX_SAMPLES 160u
#endif

/* -------------------------------------------------------------------------- */
/*  renderer read contract                                                    */
/*                                                                            */
/*  Screen1/Screen2/Screen3 renderer 는 아래 규칙을 반드시 따른다.             */
/*                                                                            */
/*  1) APP_STATE 를 직접 읽지 않는다.                                         */
/*  2) const vario_runtime_t *rt = Vario_State_GetRuntime(); 만 사용한다.      */
/*  3) 필요한 표시값이 없으면 APP_STATE 필드를 화면 코드에서 끌어오지 말고,      */
/*     이 구조체에 field 를 추가하고 Vario_State.c 에서 계산해서 넣는다.        */
/*  4) renderer 는 draw-only/read-only 계층이다.                               */
/*                                                                            */
/*  이 규칙을 지켜야                                                           */
/*  - CubeMX/IOC 재생성 영향이 하위 계층 경계에서 끊기고                      */
/*  - APP_STATE 구조 변경이 화면 코드 전체로 번지지 않으며                      */
/*  - 필터링/단위변환/유효성판정을 한 곳(Vario_State.c)에 집중시킬 수 있다.     */
/* -------------------------------------------------------------------------- */
typedef struct
{
    uint32_t last_task_ms;

    /* ---------------------------------------------------------------------- */
    /*  APP_STATE snapshot 복사본                                              */
    /*                                                                          */
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
    /*  validity latch                                                         */
    /* ---------------------------------------------------------------------- */
    bool     baro_valid;
    bool     gps_valid;
    bool     temp_valid;
    bool     altitude_valid;
    bool     heading_valid;
    bool     clock_valid;
    bool     gps_time_valid;
    bool     derived_valid;
    bool     flight_active;
    bool     trail_valid;
    bool     glide_ratio_valid;

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
    /*                                                                          */
    /*  baro_altitude_m / baro_vario_mps                                        */
    /*  - 화면에 실제로 뿌리는 5Hz publish 값                                   */
    /*  - 고도는 1m, vario 는 0.1m/s resolution 으로 양자화한다.               */
    /*                                                                          */
    /*  ground_speed_kmh                                                        */
    /*  - 화면에 실제로 뿌리는 5Hz publish speed                                */
    /*  - 좌/우 큰 숫자 블록은 이 값을 사용한다.                                */
    /*                                                                          */
    /*  gs_bar_speed_kmh                                                        */
    /*  - 우측 14px GS bar 전용 고속 경로                                       */
    /*  - GPS raw gSpeed 를 km/h 로 바꾼 값이며, 숫자 표시보다 더 빠르게 갱신된다.*/
    /*                                                                          */
    /*  fast_vario_bar_mps                                                      */
    /*  - 좌측 14px VARIO bar 전용 고속 경로                                     */
    /*  - APP_STATE fast vario path를 사용하고, 숫자 표시용 바리오와는 별도 필터  */
    /*    를 거친다.                                                            */
    /*                                                                          */
    /*  average_vario_mps                                                       */
    /*  - Flytec 스타일 integrating / average vario 용 최근 평균 상승률         */
    /*                                                                          */
    /*  glide_ratio                                                             */
    /*  - 최근 평균 sink 와 최근 평균 GS 로 계산한 활공비                        */
    /*                                                                          */
    /*  alt1_absolute_m                                                         */
    /*  - Flytec ALT1 개념: absolute altitude                                   */
    /*  - 중요: meter 숫자를 다시 feet로 바꾸기 위한 "표시 전용 양자화값" 이   */
    /*    아니라, canonical centimeter source를 그대로 meter float로 옮긴      */
    /*    compatibility field다.                                               */
    /*                                                                          */
    /*  alt2_relative_m                                                         */
    /*  - ALT2 relative mode 의 backing field                                  */
    /*  - alt2_reference_cm 과 selected altitude cm 의 차이를                  */
    /*    먼저 centimeter 해상도로 계산한 뒤 meter float로 싣는다.             */
    /*    따라서 feet 표시가 1m 계단에 묶이지 않는다.                          */
    /*                                                                          */
    /*  pressure_altitude_std_m                                                 */
    /*  - 1013.25 hPa 기준 pressure altitude                                   */
    /*  - APP_ALTITUDE가 publish한 alt_pressure_std_cm 을                      */
    /*    compatibility 용 meter float로 옮긴 값이다.                          */
    /*  - ALT2 를 FL mode 로 쓸 때 renderer 가 이 값을 사용한다.               */
    /*                                                                          */
    /*  alt3_accum_gain_m                                                       */
    /*  - Flytec ALT3 개념: 누적 상승고도                                       */
    /* ---------------------------------------------------------------------- */
    float    baro_altitude_m;
    float    baro_vario_mps;
    float    ground_speed_kmh;
    float    gs_bar_speed_kmh;
    float    fast_vario_bar_mps;
    float    heading_deg;
    float    max_top_vario_mps;
    float    max_speed_kmh;
    float    average_vario_mps;
    float    glide_ratio;
    float    alt1_absolute_m;
    float    alt2_relative_m;
    float    pressure_altitude_std_m;
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
    float    last_published_ground_speed_kmh;
    int8_t   speed_trend;

    /* ---------------------------------------------------------------------- */
    /*  timestamp / publish / flight timer                                      */
    /* ---------------------------------------------------------------------- */
    uint8_t  local_hour;
    uint8_t  local_min;
    uint8_t  local_sec;
    uint8_t  gps_hour;
    uint8_t  gps_min;
    uint8_t  gps_sec;
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
    /*  5Hz publish history                                                     */
    /*                                                                          */
    /*  history_altitude_m                                                      */
    /*  - altitude sparkline / trend 용 이력                                    */
    /*                                                                          */
    /*  history_vario_mps                                                       */
    /*  - integrated / average vario 계산용 최근 publish history               */
    /*                                                                          */
    /*  history_speed_kmh                                                       */
    /*  - glide ratio 계산용 최근 publish speed history                        */
    /* ---------------------------------------------------------------------- */
    uint16_t history_head;
    uint16_t history_count;
    float    history_altitude_m[VARIO_HISTORY_MAX_SAMPLES];
    float    history_vario_mps[VARIO_HISTORY_MAX_SAMPLES];
    float    history_speed_kmh[VARIO_HISTORY_MAX_SAMPLES];

    /* ---------------------------------------------------------------------- */
    /*  breadcrumb trail ring buffer                                            */
    /*                                                                          */
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
vario_settings_category_t Vario_State_GetSettingsCategory(void);
void Vario_State_SetSettingsCategory(vario_settings_category_t category);
void Vario_State_MoveSettingsCursor(int8_t direction);
void Vario_State_MoveQuickSetCursor(int8_t direction);
void Vario_State_MoveValueSettingCursor(int8_t direction);

const vario_runtime_t *Vario_State_GetRuntime(void);

/* 현재 altitude_source 기준 absolute altitude를 cm source-of-truth로 반환 */
bool Vario_State_GetSelectedAltitudeCm(int32_t *out_altitude_cm);

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
