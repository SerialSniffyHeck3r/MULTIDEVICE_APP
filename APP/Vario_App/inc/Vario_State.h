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
/*  - BRIGHTNESS   : live backlight bar                                        */
/*  - VOLUME       : audio volume bar                                          */
/*  - RESPONSE     : display publish response knob                              */
/*  - CLIMB START  : 상승음 시작 임계값                                        */
/* -------------------------------------------------------------------------- */
typedef enum
{
    VARIO_SETTING_MENU_BRIGHTNESS = 0u,
    VARIO_SETTING_MENU_VOLUME,
    VARIO_SETTING_MENU_RESPONSE,
    VARIO_SETTING_MENU_TRAINER,
    VARIO_SETTING_MENU_CLIMB_START,
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
    bool     glide_ratio_valid;          /* compatibility mirror = glide_ratio_slow_valid */
    bool     glide_ratio_instant_valid;
    bool     glide_ratio_slow_valid;
    bool     glide_ratio_average_valid;
    bool     wind_valid;
    bool     target_valid;
    bool     speed_to_fly_valid;
    bool     final_glide_valid;
    bool     estimated_te_valid;

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
    /*  - APP_ALTITUDE fast vario path를 app-layer second filter 없이 그대로    */
    /*    전달한다.                                                             */
    /*                                                                          */
    /*  average_vario_mps / average_speed_kmh                                   */
    /*  - integrating/average window에 대한 최근 평균 상승률 / 평균 GS          */
    /*                                                                          */
    /*  glide_ratio_*                                                           */
    /*  - instantaneous : fast bar / fast GS 기반                               */
    /*  - slow          : 큰 숫자 표시와 같은 publish 값 기반                   */
    /*  - average       : integrating window 기반                               */
    /*  - glide_ratio   : 기존 코드 호환용 mirror, slow 값과 같은 의미          */
    /*                                                                          */
    /*  estimated_airspeed_kmh                                                  */
    /*  - GPS ground vector와 circling wind estimate로 복원한 "추정 TAS"        */
    /*  - pitot가 없으므로 최종 truth는 아니며 glide computer 보조치다.         */
    /*                                                                          */
    /*  wind_speed_kmh / wind_from_deg                                          */
    /*  - circling drift 기반 wind estimate                                     */
    /*  - bearing은 "바람이 불어오는 방향" 표기                               */
    /*                                                                          */
    /*  manual_mccready_mps / speed_to_fly_kmh                                  */
    /*  - 사용자가 넣은 수동 MC 와 그에 따른 block STF                         */
    /*                                                                          */
    /*  speed_command_delta_kmh                                                 */
    /*  - + 값이면 현재 추정 airspeed보다 더 빨리 가야 함                       */
    /*                                                                          */
    /*  target_distance_m / target_bearing_deg / target_altitude_m              */
    /*  - 현재 구현에서는 flight start/home 기준 목표                           */
    /*                                                                          */
    /*  required_glide_ratio / arrival_height_m                                 */
    /*  - final glide 판단 보조치                                               */
    /*                                                                          */
    /*  estimated_te_vario_mps                                                  */
    /*  - pitot 없는 조건에서 속도변화항을 더한 "추정 TE"                     */
    /*                                                                          */
    /*  alt1_absolute_m                                                         */
    /*  - 법적 / 대회 규정용 Alt1 absolute altitude                            */
    /*  - manual QNH 기반의 barometric display altitude를                      */
    /*    compatibility 용 meter float로 옮긴 값이다.                          */
    /*  - fused / GPS path는 Alt1에 섞지 않는다.                               */
    /*                                                                          */
    /*  alt2_relative_m                                                         */
    /*  - ALT2 relative mode 의 backing field                                  */
    /*  - 사용자가 캡처한 ALT1 기준점과 현재 Alt1의 차이를                     */
    /*    centimeter 해상도로 먼저 계산한 뒤 meter float로 싣는다.             */
    /*    따라서 feet 표시가 1m 계단에 묶이지 않는다.                          */
    /*                                                                          */
    /*  pressure_altitude_std_m                                                 */
    /*  - 1013.25 hPa 기준 pressure altitude                                   */
    /*  - APP_ALTITUDE가 publish한 alt_pressure_std_cm 을                      */
    /*    compatibility 용 meter float로 옮긴 값이다.                          */
    /*  - ALT2 를 FL mode 로 쓸 때 renderer 가 이 값을 사용한다.               */
    /*                                                                          */
    /*  alt3_accum_gain_m                                                       */
    /*  - takeoff 또는 수동 reset 이후의 ALT3 relative altitude                */
    /*  - 필드명은 legacy 호환 때문에 유지하지만, 의미는 누적 gain이 아니라    */
    /*    resettable signed delta altitude다.                                  */
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
    float    average_speed_kmh;
    float    glide_ratio;
    float    glide_ratio_instant;
    float    glide_ratio_slow;
    float    glide_ratio_average;
    float    estimated_airspeed_kmh;
    float    wind_speed_kmh;
    float    wind_from_deg;
    float    manual_mccready_mps;
    float    speed_to_fly_kmh;
    float    speed_command_delta_kmh;
    float    target_distance_m;
    float    target_bearing_deg;
    float    target_altitude_m;
    float    required_glide_ratio;
    float    arrival_height_m;
    float    estimated_te_vario_mps;
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
    float    last_accum_altitude_m;   /* ALT3 reference altitude */
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

/* -------------------------------------------------------------------------- */
/*  TRAINER synthetic scenario control                                        */
/*                                                                            */
/*  main screen 에서 TRAINER=ON 일 때 F1/F2/F5/F6이 이 API를 호출한다.          */
/*  실제 센서 raw / APP_STATE 저장소를 직접 건드리지 않고,                      */
/*  Vario_State 내부 synthetic table 값만 조작한다.                           */
/* -------------------------------------------------------------------------- */
void Vario_State_TrainerAdjustSpeed(int8_t direction);
void Vario_State_TrainerAdjustHeading(int8_t direction);

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
void Vario_State_ResetAttitudeIndicator(void);

void Vario_State_RequestRedraw(void);
void Vario_State_ClearRedrawRequest(void);
bool Vario_State_IsRedrawRequested(void);

#ifdef __cplusplus
}
#endif

#endif /* VARIO_STATE_H */
