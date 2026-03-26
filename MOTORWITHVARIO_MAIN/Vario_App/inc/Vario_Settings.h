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

typedef enum
{
    VARIO_SETTINGS_CATEGORY_SYSTEM = 0u,
    VARIO_SETTINGS_CATEGORY_DISPLAY,
    VARIO_SETTINGS_CATEGORY_AUDIO,
    VARIO_SETTINGS_CATEGORY_LOG,
    VARIO_SETTINGS_CATEGORY_FLIGHT,
    VARIO_SETTINGS_CATEGORY_BLUETOOTH,
    VARIO_SETTINGS_CATEGORY_COUNT
} vario_settings_category_t;

/* -------------------------------------------------------------------------- */
/*  Backlight mode                                                            */
/* -------------------------------------------------------------------------- */
typedef enum
{
    VARIO_BACKLIGHT_MODE_AUTO_CONTINUOUS = 0u,
    VARIO_BACKLIGHT_MODE_AUTO_DAY_NIGHT,
    VARIO_BACKLIGHT_MODE_MANUAL,
    VARIO_BACKLIGHT_MODE_COUNT
} vario_backlight_mode_t;


/* -------------------------------------------------------------------------- */
/*  Acoustic profile                                                          */
/*                                                                            */
/*  상용기마다 threshold는 비슷해도 "귀맛" 이 다르다.                          */
/*  본 enum은 그 차이를 코드에 숨겨 두지 않고                                 */
/*  사용자가 profile 단위로 고를 수 있게 하기 위한 정책 스위치다.             */
/* -------------------------------------------------------------------------- */
typedef enum
{
    VARIO_AUDIO_PROFILE_SOFT_SPEAKER = 0u,
    VARIO_AUDIO_PROFILE_FLYTEC_CLASSIC,
    VARIO_AUDIO_PROFILE_BLUEFLY_SMOOTH,
    VARIO_AUDIO_PROFILE_DIGIFLY_DG,
    VARIO_AUDIO_PROFILE_COUNT
} vario_audio_profile_t;

/* -------------------------------------------------------------------------- */
/*  Pre-thermal tone mode                                                     */
/* -------------------------------------------------------------------------- */
typedef enum
{
    VARIO_PRETHERMAL_MODE_OFF = 0u,
    VARIO_PRETHERMAL_MODE_BUZZER,
    VARIO_PRETHERMAL_MODE_SOFT_PULSE,
    VARIO_PRETHERMAL_MODE_COUNT
} vario_prethermal_mode_t;

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
/*  QNH_MANUAL   : APP_ALTITUDE가 APP_STATE에 publish한 manual-QNH 결과 사용    */
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
    VARIO_QUICKSET_ITEM_AUDIO_RESPONSE,
    VARIO_QUICKSET_ITEM_VARIO_AVG_SECONDS,
    VARIO_QUICKSET_ITEM_AUDIO_PROFILE,
    VARIO_QUICKSET_ITEM_AUDIO_UP_BASE_HZ,
    VARIO_QUICKSET_ITEM_AUDIO_MOD_DEPTH,
    VARIO_QUICKSET_ITEM_AUDIO_PITCH_CURVE,
    VARIO_QUICKSET_ITEM_CLIMB_TONE_THRESHOLD,
    VARIO_QUICKSET_ITEM_CLIMB_TONE_OFF_THRESHOLD,
    VARIO_QUICKSET_ITEM_PRETHERMAL_MODE,
    VARIO_QUICKSET_ITEM_PRETHERMAL_THRESHOLD,
    VARIO_QUICKSET_ITEM_PRETHERMAL_OFF_THRESHOLD,
    VARIO_QUICKSET_ITEM_SINK_TONE_THRESHOLD,
    VARIO_QUICKSET_ITEM_SINK_TONE_OFF_THRESHOLD,
    VARIO_QUICKSET_ITEM_SINK_CONT_THRESHOLD,
    VARIO_QUICKSET_ITEM_SINK_CONT_OFF_THRESHOLD,
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
    /*                                                                        */
    /*  중요: canonical owner는                                                */
    /*        APP_STATE.settings.altitude.manual_qnh_hpa_x100 이다.           */
    /*                                                                        */
    /*  이 필드는 기존 화면/설정 코드가 그대로 컴파일되도록 남겨 둔            */
    /*  compatibility mirror다.                                                */
    /*  새 코드는 이 멤버를 직접 믿지 말고                                    */
    /*  Vario_Settings_GetManualQnhHpaX100() /                                */
    /*  Vario_Settings_SetManualQnhHpaX100() 를 사용한다.                     */
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
    uint8_t                 audio_enabled;
    uint8_t                 audio_volume_percent;
    uint8_t                 beep_only_when_flying;
    vario_audio_profile_t   audio_profile;
    vario_prethermal_mode_t prethermal_mode;
    uint8_t                 audio_response_level;

    /* ---------------------------------------------------------------------- */
    /*  acoustic fine tuning                                                  */
    /* ---------------------------------------------------------------------- */
    uint16_t audio_up_base_hz;
    uint8_t  audio_modulation_depth_percent;
    uint8_t  audio_pitch_curve_percent;

    /* ---------------------------------------------------------------------- */
    /*  디스플레이                                                             */
    /* ---------------------------------------------------------------------- */
    vario_backlight_mode_t display_backlight_mode;
    uint8_t                display_brightness_percent;
    uint8_t                display_contrast_raw;
    uint8_t                display_temp_compensation;

    /* ---------------------------------------------------------------------- */
    /*  디지털 표시 damping / audio response / 바리오 임계값                  */
    /*                                                                        */
    /*  vario_damping_level                                                   */
    /*  - 상위 제품 의미에서는 "숫자/표시 damping" 이다.                     */
    /*  - 현재 배선에서는                                                     */
    /*      1) 5Hz publish 숫자 hysteresis 폭                                 */
    /*      2) display-friendly slow path 반응성                              */
    /*      3) APP_ALTITUDE 저수준 tau / noise mirror                         */
    /*    를 함께 움직인다.                                                   */
    /*  - 중요한 점: 제품용 오디오 attack/release, cadence follow 는          */
    /*    audio_response_level 이 담당한다.                                   */
    /*    즉, damping 과 audio response 의 의미를 분리한다.                  */
    /*                                                                        */
    /*  climb_tone_threshold_cms                                              */
    /*  - 상승음이 처음 살아나는 임계값                                        */
    /*                                                                        */
    /*  sink_tone_threshold_cms                                               */
    /*  - 하강음이 처음 살아나는 임계값                                        */
    /*                                                                        */
    /*  sink_continuous_threshold_cms                                         */
    /*  - 이 값보다 더 큰 sink(더 음수)에서는                                  */
    /*    Digifly 스타일의 짧은 sink chirp band를 지나                        */
    /*    연속 sink saw tone으로 넘어간다.                                    */
    /*  - sink_tone_threshold_cms와 같게 두면                                  */
    /*    sink chirp band를 사실상 끌 수 있다.                                */
    /* ---------------------------------------------------------------------- */
    uint8_t  vario_damping_level;
    uint8_t  digital_vario_average_seconds;
    int16_t  climb_tone_threshold_cms;
    int16_t  climb_tone_off_threshold_cms;
    int16_t  prethermal_threshold_cms;
    int16_t  prethermal_off_threshold_cms;
    int16_t  sink_tone_threshold_cms;
    int16_t  sink_tone_off_threshold_cms;
    int16_t  sink_continuous_threshold_cms;
    int16_t  sink_continuous_off_threshold_cms;
    uint16_t flight_start_speed_kmh_x10;

    /* ---------------------------------------------------------------------- */
    /*  glide computer / polar / final glide                                  */
    /*                                                                        */
    /*  manual_mccready_cms                                                   */
    /*  - 수동 McCready 값, cm/s                                              */
    /*  - pitot 없이도 block speed-to-fly / final glide의 climb expectation   */
    /*    으로 사용할 수 있다.                                                */
    /*                                                                        */
    /*  final_glide_safety_margin_m                                           */
    /*  - arrival height 계산 시 항상 남겨 둘 안전 여유 고도                  */
    /*                                                                        */
    /*  polar_speed*_kmh_x10 / polar_sink*_cms                                */
    /*  - 3점 polar 입력값                                                    */
    /*  - speed 는 km/h * 10, sink 는 +cm/s (양의 sink magnitude)             */
    /*  - Vario_GlideComputer 가 이 세 점으로 quadratic polar 를 복원한다.    */
    /* ---------------------------------------------------------------------- */
    int16_t  manual_mccready_cms;
    uint16_t final_glide_safety_margin_m;
    uint16_t polar_speed1_kmh_x10;
    int16_t  polar_sink1_cms;
    uint16_t polar_speed2_kmh_x10;
    int16_t  polar_sink2_cms;
    uint16_t polar_speed3_kmh_x10;
    int16_t  polar_sink3_cms;

    /* ---------------------------------------------------------------------- */
    /*  로깅 / Bluetooth                                                      */
    /* ---------------------------------------------------------------------- */
    uint8_t  log_enabled;
    uint8_t  log_interval_seconds;
    uint8_t  bluetooth_echo_enabled;
    uint8_t  bluetooth_auto_ping_enabled;

    /* ---------------------------------------------------------------------- */
    /*  그래픽 설정                                                            */
    /* ---------------------------------------------------------------------- */
    uint16_t compass_span_deg;
    uint8_t  compass_box_height_px;

    /* ---------------------------------------------------------------------- */
    /*  좌측 VARIO side bar full-scale                                        */
    /*                                                                        */
    /*  단위는 0.1 m/s 이다.                                                  */
    /*  현재 UI 계약은 두 값만 사용한다.                                      */
    /*  - 40 -> 화면 맨 위 +4.0 / 맨 아래 -4.0                               */
    /*  - 50 -> 화면 맨 위 +5.0 / 맨 아래 -5.0                               */
    /*                                                                        */
    /*  draw layer는 이 값을 기준으로                                        */
    /*  - tick 위치                                                            */
    /*  - instant/average vario fill 길이                                     */
    /*  - over-range 지워짐 패턴                                              */
    /*  을 모두 같은 스케일로 계산한다.                                      */
    /* ---------------------------------------------------------------------- */
    uint8_t  vario_range_mps_x10;

    /* ---------------------------------------------------------------------- */
    /*  우측 GS side bar top scale                                            */
    /*  단위는 km/h 이다.                                                     */
    /* ---------------------------------------------------------------------- */
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

/* -------------------------------------------------------------------------- */
/*  Manual QNH canonical access                                               */
/*                                                                            */
/*  upper VARIO layer는 더 이상 local mirror를 source-of-truth로 쓰지 않고    */
/*  APP_STATE.settings.altitude.manual_qnh_hpa_x100 을 통해 접근한다.         */
/*  이 API는 기존 구조를 크게 깨지 않으면서 single-source-of-truth를           */
/*  회복하기 위한 얇은 호환 계층이다.                                         */
/* -------------------------------------------------------------------------- */
int32_t Vario_Settings_GetManualQnhHpaX100(void);
void    Vario_Settings_SetManualQnhHpaX100(int32_t qnh_hpa_x100);

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
const char *Vario_Settings_GetAudioProfileText(void);
const char *Vario_Settings_GetAudioProfileTextForProfile(vario_audio_profile_t profile);
const char *Vario_Settings_GetPrethermalModeText(void);
const char *Vario_Settings_GetPrethermalModeTextForMode(vario_prethermal_mode_t mode);
const char *Vario_Settings_GetAltitudeSourceText(void);
const char *Vario_Settings_GetHeadingSourceText(void);
const char *Vario_Settings_GetAlt2ModeText(void);
const char *Vario_Settings_GetAlt2ModeTextForMode(vario_alt2_mode_t mode);
const char *Vario_Settings_GetPressureUnitText(void);
const char *Vario_Settings_GetTemperatureUnitText(void);
const char *Vario_Settings_GetTimeFormatText(void);
const char *Vario_Settings_GetCoordFormatText(void);
const char *Vario_Settings_GetBeepModeText(void);
const char *Vario_Settings_GetBacklightModeText(void);
const char *Vario_Settings_GetBacklightModeTextForMode(vario_backlight_mode_t mode);

const char *Vario_Settings_GetCategoryText(vario_settings_category_t category);
uint8_t     Vario_Settings_GetCategoryItemCount(vario_settings_category_t category);
void        Vario_Settings_GetCategoryItemText(vario_settings_category_t category,
                                               uint8_t index,
                                               char *out_label,
                                               size_t label_len,
                                               char *out_value,
                                               size_t value_len);
void        Vario_Settings_AdjustCategoryItem(vario_settings_category_t category,
                                              uint8_t index,
                                              int8_t direction);

#ifdef __cplusplus
}
#endif

#endif /* VARIO_SETTINGS_H */
