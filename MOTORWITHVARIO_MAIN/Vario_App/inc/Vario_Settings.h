#ifndef VARIO_SETTINGS_H
#define VARIO_SETTINGS_H

#include <stdbool.h>
#include <stddef.h>
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
/*  3) 하위 레이어와 직접 결합하지 않도록, 실제 반영은 상위 task 가 API 로     */
/*     플랫폼 서비스에 전달한다.                                              */
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

typedef enum
{
    VARIO_PRESSURE_UNIT_HPA = 0u,
    VARIO_PRESSURE_UNIT_INHG,
    VARIO_PRESSURE_UNIT_COUNT
} vario_pressure_unit_t;

typedef enum
{
    VARIO_TEMPERATURE_UNIT_C = 0u,
    VARIO_TEMPERATURE_UNIT_F,
    VARIO_TEMPERATURE_UNIT_COUNT
} vario_temperature_unit_t;

typedef enum
{
    VARIO_TIME_FORMAT_24H = 0u,
    VARIO_TIME_FORMAT_12H,
    VARIO_TIME_FORMAT_COUNT
} vario_time_format_t;

typedef enum
{
    VARIO_COORD_FORMAT_DDMM_MMM = 0u,
    VARIO_COORD_FORMAT_DECIMAL,
    VARIO_COORD_FORMAT_DMS,
    VARIO_COORD_FORMAT_COUNT
} vario_coord_format_t;

/* -------------------------------------------------------------------------- */
/*  ALT2 mode                                                                 */
/*                                                                            */
/*  RELATIVE     : Flytec ALT2 기본 개념. 사용자가 잡은 기준고도 대비 상대고도  */
/*  ABSOLUTE     : ALT1 과 같은 absolute altitude 를 ALT2 자리에도 표시        */
/*  GPS          : GPS hMSL altitude                                           */
/*  FLIGHT_LEVEL : 1013.25 hPa 기준 pressure altitude 를 FL 로 표시            */
/* -------------------------------------------------------------------------- */
typedef enum
{
    VARIO_ALT2_MODE_RELATIVE = 0u,
    VARIO_ALT2_MODE_ABSOLUTE,
    VARIO_ALT2_MODE_GPS,
    VARIO_ALT2_MODE_FLIGHT_LEVEL,
    VARIO_ALT2_MODE_COUNT
} vario_alt2_mode_t;

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
/*  Flight/Audio/Instruments 쪽에서 상용 variometer 들이 자주 제공하는         */
/*  항목을 현재 펌웨어 범위 안에서 정리했다.                                   */
/* -------------------------------------------------------------------------- */
typedef enum
{
    VARIO_QUICKSET_ITEM_QNH = 0u,
    VARIO_QUICKSET_ITEM_ALT_UNIT,
    VARIO_QUICKSET_ITEM_ALT2_MODE,
    VARIO_QUICKSET_ITEM_ALT2_UNIT,
    VARIO_QUICKSET_ITEM_VSPEED_UNIT,
    VARIO_QUICKSET_ITEM_SPEED_UNIT,
    VARIO_QUICKSET_ITEM_PRESSURE_UNIT,
    VARIO_QUICKSET_ITEM_TEMPERATURE_UNIT,
    VARIO_QUICKSET_ITEM_TIME_FORMAT,
    VARIO_QUICKSET_ITEM_COORD_FORMAT,
    VARIO_QUICKSET_ITEM_ALT_SOURCE,
    VARIO_QUICKSET_ITEM_HEADING_SOURCE,
    VARIO_QUICKSET_ITEM_VARIO_DAMPING,
    VARIO_QUICKSET_ITEM_VARIO_AVG_SECONDS,
    VARIO_QUICKSET_ITEM_CLIMB_TONE_THRESHOLD,
    VARIO_QUICKSET_ITEM_SINK_TONE_THRESHOLD,
    VARIO_QUICKSET_ITEM_FLIGHT_START_SPEED,
    VARIO_QUICKSET_ITEM_BEEP_ONLY_WHEN_FLYING,
    VARIO_QUICKSET_ITEM_AUDIO_ENABLE,
    VARIO_QUICKSET_ITEM_AUDIO_VOLUME,
    VARIO_QUICKSET_ITEM_ALT2_CAPTURE,
    VARIO_QUICKSET_ITEM_ALT3_RESET,
    VARIO_QUICKSET_ITEM_FLIGHT_RESET,
    VARIO_QUICKSET_ITEM_COUNT
} vario_quickset_item_t;

/* -------------------------------------------------------------------------- */
/*  VALUESETTING page                                                         */
/* -------------------------------------------------------------------------- */
typedef enum
{
    VARIO_VALUE_ITEM_BRIGHTNESS = 0u,
    VARIO_VALUE_ITEM_COMPASS_SPAN,
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
    /* ---------------------------------------------------------------------- */
    int32_t alt2_reference_cm;

    /* ---------------------------------------------------------------------- */
    /*  단위 / 표시 형식                                                       */
    /* ---------------------------------------------------------------------- */
    vario_alt_unit_t         altitude_unit;
    vario_alt_unit_t         alt2_unit;
    vario_vspeed_unit_t      vspeed_unit;
    vario_speed_unit_t       speed_unit;
    vario_pressure_unit_t    pressure_unit;
    vario_temperature_unit_t temperature_unit;
    vario_time_format_t      time_format;
    vario_coord_format_t     coord_format;
    vario_alt2_mode_t        alt2_mode;

    /* ---------------------------------------------------------------------- */
    /*  데이터 소스 선택                                                       */
    /* ---------------------------------------------------------------------- */
    vario_alt_source_t     altitude_source;
    vario_heading_source_t heading_source;

    /* ---------------------------------------------------------------------- */
    /*  오디오 / 사용자 경험                                                   */
    /* ---------------------------------------------------------------------- */
    uint8_t audio_enabled;
    uint8_t audio_volume_percent;
    uint8_t beep_only_when_flying;

    /* ---------------------------------------------------------------------- */
    /*  디스플레이                                                             */
    /* ---------------------------------------------------------------------- */
    uint8_t display_brightness_percent;

    /* ---------------------------------------------------------------------- */
    /*  디지털 표시/오디오용 필터 튜닝값                                       */
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
float   Vario_Settings_GetQnhDisplayFloat(void);
void    Vario_Settings_FormatQnhText(char *buf, size_t buf_len);
float   Vario_Settings_PressureHpaToDisplayFloat(float pressure_hpa);
float   Vario_Settings_TemperatureCToDisplayFloat(float temperature_c);

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
const char *Vario_Settings_GetAlt2ModeText(void);
const char *Vario_Settings_GetAlt2ModeTextForMode(vario_alt2_mode_t mode);
const char *Vario_Settings_GetPressureUnitText(void);
const char *Vario_Settings_GetTemperatureUnitText(void);
const char *Vario_Settings_GetTimeFormatText(void);
const char *Vario_Settings_GetCoordFormatText(void);
const char *Vario_Settings_GetBeepModeText(void);

#ifdef __cplusplus
}
#endif

#endif /* VARIO_SETTINGS_H */
