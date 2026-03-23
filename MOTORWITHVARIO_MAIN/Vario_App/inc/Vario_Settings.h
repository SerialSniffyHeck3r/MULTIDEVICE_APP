#ifndef VARIO_SETTINGS_H
#define VARIO_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  바리오 앱 사용자 설정 저장소                                               */
/*                                                                            */
/*  중요한 설계 원칙                                                          */
/*  1) 이 파일은 VARIO_APP 상위 계층이 직접 들고 있는 "사용자 의도" 저장소다. */
/*  2) 센서 raw register 와는 무관하며, APP_STATE snapshot 을 화면/필터/오디오 */
/*     에 어떻게 가공해서 쓸지를 결정하는 스위치만 보관한다.                  */
/*  3) 현재는 RAM 전용 구조체지만, 추후 Winbond flash/NVRAM 으로 그대로       */
/*     옮겨 갈 수 있도록 plain struct 형태를 유지한다.                        */
/* -------------------------------------------------------------------------- */

typedef enum
{
    VARIO_ALT_UNIT_METER = 0u,
    VARIO_ALT_UNIT_FEET  = 1u
} vario_alt_unit_t;

typedef enum
{
    VARIO_VSPEED_UNIT_MPS = 0u,
    VARIO_VSPEED_UNIT_FPM = 1u
} vario_vspeed_unit_t;

typedef enum
{
    VARIO_SPEED_UNIT_KMH = 0u,
    VARIO_SPEED_UNIT_MPH,
    VARIO_SPEED_UNIT_KNOT,
    VARIO_SPEED_UNIT_COUNT
} vario_speed_unit_t;

/* -------------------------------------------------------------------------- */
/*  ALT source                                                                */
/*                                                                            */
/*  DISPLAY      : APP_ALTITUDE 가 최종 UI용으로 공개한 alt_display_cm         */
/*  QNH_MANUAL   : APP_STATE pressure_filt + VARIO 수동 QNH 로 재계산          */
/*  FUSED_NOIMU  : IMU 미사용 fused altitude                                  */
/*  FUSED_IMU    : IMU 보조 fused altitude                                    */
/*  GPS_HMSL     : GPS hMSL                                                    */
/* -------------------------------------------------------------------------- */
typedef enum
{
    VARIO_ALT_SOURCE_DISPLAY = 0u,
    VARIO_ALT_SOURCE_QNH_MANUAL,
    VARIO_ALT_SOURCE_FUSED_NOIMU,
    VARIO_ALT_SOURCE_FUSED_IMU,
    VARIO_ALT_SOURCE_GPS_HMSL,
    VARIO_ALT_SOURCE_COUNT
} vario_alt_source_t;

/* -------------------------------------------------------------------------- */
/*  Heading source                                                            */
/*                                                                            */
/*  AUTO : APP_BIKE heading -> GPS headVeh -> GPS headMot 순으로 선택          */
/*  BIKE : APP_BIKE heading 우선, 없으면 마지막 유효값 유지                    */
/*  GPS  : GPS course/headVeh 계열만 사용                                      */
/* -------------------------------------------------------------------------- */
typedef enum
{
    VARIO_HEADING_SOURCE_AUTO = 0u,
    VARIO_HEADING_SOURCE_BIKE,
    VARIO_HEADING_SOURCE_GPS,
    VARIO_HEADING_SOURCE_COUNT
} vario_heading_source_t;

/* -------------------------------------------------------------------------- */
/*  QUICKSET page                                                             */
/*                                                                            */
/*  기존 상태머신 구조를 바꾸지 않기 위해 "QUICKSET" 모드는 유지한다.         */
/*  다만 화면 의미를 Flytec/상용 variometer 성격에 맞춰                       */
/*  Flight/Audio/Instruments 설정 페이지로 재정의한다.                        */
/* -------------------------------------------------------------------------- */
typedef enum
{
    VARIO_QUICKSET_ITEM_QNH = 0u,
    VARIO_QUICKSET_ITEM_ALT_UNIT,
    VARIO_QUICKSET_ITEM_VSPEED_UNIT,
    VARIO_QUICKSET_ITEM_SPEED_UNIT,
    VARIO_QUICKSET_ITEM_ALT_SOURCE,
    VARIO_QUICKSET_ITEM_HEADING_SOURCE,
    VARIO_QUICKSET_ITEM_VARIO_DAMPING,
    VARIO_QUICKSET_ITEM_VARIO_AVG_SECONDS,
    VARIO_QUICKSET_ITEM_CLIMB_TONE_THRESHOLD,
    VARIO_QUICKSET_ITEM_SINK_TONE_THRESHOLD,
    VARIO_QUICKSET_ITEM_FLIGHT_START_SPEED,
    VARIO_QUICKSET_ITEM_AUDIO_ENABLE,
    VARIO_QUICKSET_ITEM_AUDIO_VOLUME,
    VARIO_QUICKSET_ITEM_ALT2_CAPTURE,
    VARIO_QUICKSET_ITEM_ALT3_RESET,
    VARIO_QUICKSET_ITEM_FLIGHT_RESET,
    VARIO_QUICKSET_ITEM_COUNT
} vario_quickset_item_t;

/* -------------------------------------------------------------------------- */
/*  VALUESETTING page                                                         */
/*                                                                            */
/*  기존 VALUESETTING 모드는 그대로 두되,                                      */
/*  사용자가 요구한 그래픽 전용 설정 페이지로 재해석한다.                      */
/* -------------------------------------------------------------------------- */
typedef enum
{
    VARIO_VALUE_ITEM_COMPASS_SPAN = 0u,
    VARIO_VALUE_ITEM_COMPASS_BOX_HEIGHT,
    VARIO_VALUE_ITEM_VARIO_RANGE,
    VARIO_VALUE_ITEM_GS_RANGE,
    VARIO_VALUE_ITEM_TRAIL_RANGE,
    VARIO_VALUE_ITEM_TRAIL_SPACING,
    VARIO_VALUE_ITEM_TRAIL_DOT_SIZE,
    VARIO_VALUE_ITEM_ARROW_SIZE,
    VARIO_VALUE_ITEM_SHOW_TIME,
    VARIO_VALUE_ITEM_SHOW_FLIGHT_TIME,
    VARIO_VALUE_ITEM_SHOW_MAX_VARIO,
    VARIO_VALUE_ITEM_SHOW_GS_BAR,
    VARIO_VALUE_ITEM_COUNT
} vario_value_item_t;

typedef struct
{
    /* ---------------------------------------------------------------------- */
    /*  수동 QNH                                                               */
    /*  - 단위 : 0.01 hPa                                                      */
    /*  - 예   : 1013.25 hPa -> 101325                                         */
    /* ---------------------------------------------------------------------- */
    int32_t qnh_hpa_x100;

    /* ---------------------------------------------------------------------- */
    /*  ALT2 reference altitude                                                */
    /*  - Flytec 계열의 ALT2 개념을 따라 "사용자가 잡아 둔 기준 고도" 를 저장   */
    /*  - 페이지 1의 ALT2 는 현재 absolute altitude - alt2_reference_cm 이다.    */
    /* ---------------------------------------------------------------------- */
    int32_t alt2_reference_cm;

    /* ---------------------------------------------------------------------- */
    /*  단위 설정                                                              */
    /*                                                                          */
    /*  altitude_unit                                                           */
    /*  - ALT1 / ALT3 기본 단위                                                */
    /*                                                                          */
    /*  alt2_unit                                                               */
    /*  - ALT2 전용 단위                                                        */
    /*  - 사용자가 요청한 "ALT2는 ft, ALT1/ALT3는 m" 조합을 지원하기 위해      */
    /*    별도 필드로 분리한다.                                                 */
    /* ---------------------------------------------------------------------- */
    vario_alt_unit_t     altitude_unit;
    vario_alt_unit_t     alt2_unit;
    vario_vspeed_unit_t  vspeed_unit;
    vario_speed_unit_t   speed_unit;

    /* ---------------------------------------------------------------------- */
    /*  데이터 소스 선택                                                       */
    /* ---------------------------------------------------------------------- */
    vario_alt_source_t     altitude_source;
    vario_heading_source_t heading_source;

    /* ---------------------------------------------------------------------- */
    /*  오디오 기본 설정                                                       */
    /* ---------------------------------------------------------------------- */
    uint8_t audio_enabled;
    uint8_t audio_volume_percent;

    /* ---------------------------------------------------------------------- */
    /*  디지털 표시/오디오용 필터 튜닝값                                       */
    /*                                                                            */
    /*  vario_damping_level                                                    */
    /*  - 1~10 범위                                                            */
    /*  - 값이 클수록 화면의 숫자는 더 차분해지지만 반응은 약간 느려진다.       */
    /*                                                                            */
    /*  digital_vario_average_seconds                                          */
    /*  - Flytec/6030/6015 계열 integrated vario 시간 개념을 반영한 값         */
    /*  - 1~30초 범위                                                          */
    /*                                                                            */
    /*  climb/sink tone threshold                                              */
    /*  - tone 활성 deadband.                                                  */
    /*  - 단위 : cm/s                                                          */
    /* ---------------------------------------------------------------------- */
    uint8_t  vario_damping_level;
    uint8_t  digital_vario_average_seconds;
    int16_t  climb_tone_threshold_cms;
    int16_t  sink_tone_threshold_cms;
    uint16_t flight_start_speed_kmh_x10;

    /* ---------------------------------------------------------------------- */
    /*  그래픽 설정                                                            */
    /* ---------------------------------------------------------------------- */
    uint16_t compass_span_deg;
    uint8_t  compass_box_height_px;
    uint8_t  vario_range_mps_x10;
    uint16_t gs_range_kmh;
    uint16_t trail_range_m;
    uint16_t trail_spacing_m;
    uint8_t  trail_dot_size_px;
    uint8_t  arrow_size_px;
    uint8_t  show_current_time;
    uint8_t  show_flight_time;
    uint8_t  show_max_vario;
    uint8_t  show_gs_bar;
} vario_settings_t;

void Vario_Settings_Init(void);
const vario_settings_t *Vario_Settings_Get(void);

void Vario_Settings_AdjustQuickSet(vario_quickset_item_t item, int8_t direction);
void Vario_Settings_AdjustValue(vario_value_item_t item, int8_t direction);

int32_t Vario_Settings_GetQnhDisplayWhole(void);
int32_t Vario_Settings_GetQnhDisplayFrac2(void);
int32_t Vario_Settings_GetQnhDisplayFrac1(void);

int32_t Vario_Settings_AltitudeMetersToDisplayRounded(float altitude_m);
int32_t Vario_Settings_AltitudeMetersToDisplayRoundedWithUnit(float altitude_m, vario_alt_unit_t unit);
int32_t Vario_Settings_AltitudeCentimetersToDisplayRounded(int32_t altitude_cm);
int32_t Vario_Settings_VSpeedToDisplayRounded(float vspd_mps);
int32_t Vario_Settings_SpeedToDisplayRounded(float speed_kmh);

float Vario_Settings_SpeedKmhToDisplayFloat(float speed_kmh);
float Vario_Settings_VSpeedMpsToDisplayFloat(float vspd_mps);
float Vario_Settings_NavDistanceMetersToDisplayFloat(float distance_m);

const char *Vario_Settings_GetAltitudeUnitText(void);
const char *Vario_Settings_GetAltitudeUnitTextForUnit(vario_alt_unit_t unit);
const char *Vario_Settings_GetVSpeedUnitText(void);
const char *Vario_Settings_GetSpeedUnitText(void);
const char *Vario_Settings_GetNavDistanceUnitText(void);
const char *Vario_Settings_GetAudioOnOffText(void);
const char *Vario_Settings_GetAltitudeSourceText(void);
const char *Vario_Settings_GetHeadingSourceText(void);

#ifdef __cplusplus
}
#endif

#endif /* VARIO_SETTINGS_H */
