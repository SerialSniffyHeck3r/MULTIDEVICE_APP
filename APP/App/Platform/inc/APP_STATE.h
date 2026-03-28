#ifndef APP_STATE_H
#define APP_STATE_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  APP_STATE                                                                  */
/*                                                                            */
/*  이 프로젝트에서 APP_STATE는 "기기 전체의 자료창고" 역할을 한다.             */
/*                                                                            */
/*  - GPS 드라이버(Ublox_GPS.c)는 UART로 들어온 UBX 메시지를 해석해서          */
/*    g_app_state.gps 안에 계속 밀어 넣는다.                                  */
/*  - UI(main.c의 디버그 화면/스카이플롯)는 APP_STATE의 스냅샷만 읽는다.       */
/*  - 사용자 설정(환경설정)도 APP_STATE 안에 둔다.                             */
/*                                                                            */
/*  즉, "받는 놈" 과 "보여주는 놈" 을 분리해서                                 */
/*  코드가 덜 꼬이고 유지보수가 쉬워지도록 만든 구조다.                         */
/* -------------------------------------------------------------------------- */

#ifndef APP_GPS_MAX_SATS
#define APP_GPS_MAX_SATS            32u
#endif

#ifndef APP_GPS_LAST_RAW_MAX
#define APP_GPS_LAST_RAW_MAX        512u
#endif

#ifndef APP_GPS_MON_VER_EXT_MAX
#define APP_GPS_MON_VER_EXT_MAX     10u
#endif

#if defined(__GNUC__)
#define APP_PACKED __attribute__((packed))
#else
#define APP_PACKED
#endif



/* -------------------------------------------------------------------------- */
/*  사용자 환경설정                                                             */
/* -------------------------------------------------------------------------- */

typedef enum
{
    APP_GPS_BOOT_PROFILE_GPS_ONLY_20HZ = 0,
    APP_GPS_BOOT_PROFILE_GPS_ONLY_10HZ = 1,
    APP_GPS_BOOT_PROFILE_MULTI_CONST_10HZ = 2
} app_gps_boot_profile_t;

typedef enum
{
    APP_GPS_POWER_PROFILE_HIGH_POWER = 0,
    APP_GPS_POWER_PROFILE_POWER_SAVE = 1
} app_gps_power_profile_t;

typedef struct
{
    app_gps_boot_profile_t  boot_profile;
    app_gps_power_profile_t power_profile;
} app_gps_settings_t;

#ifndef APP_CLOCK_TIMEZONE_QUARTERS_MIN
#define APP_CLOCK_TIMEZONE_QUARTERS_MIN   (-48)
#endif

#ifndef APP_CLOCK_TIMEZONE_QUARTERS_MAX
#define APP_CLOCK_TIMEZONE_QUARTERS_MAX   (56)
#endif

#ifndef APP_CLOCK_TIMEZONE_QUARTERS_DEFAULT
#define APP_CLOCK_TIMEZONE_QUARTERS_DEFAULT  (36)
#endif

#ifndef APP_CLOCK_GPS_SYNC_INTERVAL_MIN_DEFAULT
#define APP_CLOCK_GPS_SYNC_INTERVAL_MIN_DEFAULT  (10u)
#endif

typedef struct
{
    int8_t  timezone_quarters;          /* UTC offset / 15 min 단위. 한국 기본값 +36 */
    uint8_t gps_auto_sync_enabled;      /* GPS 자동 동기화 사용 여부                 */
    uint8_t gps_sync_interval_minutes;  /* GPS time-only 주기. 현재 기본 10분        */
    uint8_t reserved0;                  /* 정렬/향후 확장용                           */
} app_clock_settings_t;

typedef enum
{
    APP_BACKLIGHT_AUTO_MODE_CONTINUOUS = 0u,
    APP_BACKLIGHT_AUTO_MODE_DIMMER     = 1u
} app_backlight_auto_mode_t;

typedef struct
{
    /* ---------------------------------------------------------------------- */
    /*  자동 밝기 동작 모드                                                    */
    /*                                                                        */
    /*  CONTINUOUS                                                             */
    /*  - 주변광 값 전체를 연속 곡선으로 해석해서 화면 밝기를 연속적으로 추종  */
    /*                                                                        */
    /*  DIMMER                                                                 */
    /*  - DAY / NIGHT / SUPER NIGHT 3개 존으로만 target을 고른다.             */
    /*  - 단, 실제 target으로 넘어가는 출력 전환 자체는 부드럽게 이어진다.    */
    /* ---------------------------------------------------------------------- */
    uint8_t auto_mode;                      /* app_backlight_auto_mode_t raw         */
    int8_t  continuous_bias_steps;         /* AUTO-CONT 전용 -2..+2 bias 단계        */
    uint8_t transition_smoothness;         /* 1..5, 높을수록 더 천천히 따라감        */
    uint8_t reserved0;                     /* 정렬/향후 확장용                       */

    /* ---------------------------------------------------------------------- */
    /*  AUTO-DIMMER 존 기준/밝기                                               */
    /*                                                                        */
    /*  sensor 기준은 Brightness_Sensor의 normalized percent(0..100) 기준이다. */
    /*  - night_threshold_percent        : DAY ↔ NIGHT 경계                    */
    /*  - super_night_threshold_percent  : NIGHT ↔ SUPER NIGHT 경계            */
    /*  - *_brightness_percent           : 해당 존의 목표 화면 밝기            */
    /*                                                                        */
    /*  DAY 밝기는 요구사항대로 100% 고정이므로 별도 저장하지 않는다.          */
    /* ---------------------------------------------------------------------- */
    uint8_t night_threshold_percent;       /* 0..100                               */
    uint8_t super_night_threshold_percent; /* 0..100                               */
    uint8_t night_brightness_percent;      /* 0..100                               */
    uint8_t super_night_brightness_percent;/* 0..100                               */
} app_backlight_settings_t;

typedef struct
{
    /* ---------------------------------------------------------------------- */
    /*  UC1608 패널 설정                                                       */
    /*                                                                        */
    /*  이 구조체는 "실제 패널 레지스터에 바로 반영 가능한 값"을 의도적으로     */
    /*  raw에 가깝게 보관한다.                                                 */
    /*                                                                        */
    /*  contrast                 -> CMD 0x81 + arg                             */
    /*  temperature_compensation -> CMD 0x24..0x27 (240x128에서는 mux128 고정) */
    /*  bias_ratio               -> CMD 0xE8..0xEB                             */
    /*  ram_access_mode          -> CMD 0x88..0x8B                             */
    /*  start_line_raw           -> CMD 0x40..0x7F 의 하위 raw 값(0..63)       */
    /*  fixed_line_raw           -> CMD 0x90..0x9F 의 하위 raw 값(0..15)       */
    /*  power_control_raw        -> CMD 0x28..0x2F 의 하위 raw 값(0..7)        */
    /*  flip_mode                -> U8G2 flip mode 0/1                         */
    /* ---------------------------------------------------------------------- */
    uint8_t contrast;                     /* 0..255                               */
    uint8_t temperature_compensation;     /* 0..3                                 */
    uint8_t bias_ratio;                   /* 0..3                                 */
    uint8_t ram_access_mode;              /* 0..3                                 */

    uint8_t start_line_raw;               /* 0..63                                */
    uint8_t fixed_line_raw;               /* 0..15                                */
    uint8_t power_control_raw;            /* 0..7                                 */
    uint8_t flip_mode;                    /* 0..1                                 */
} app_uc1608_settings_t;

typedef struct
{
    /* ---------------------------------------------------------------------- */
    /*  수동 QNH 설정                                                          */
    /*                                                                        */
    /*  항공식 의미를 보존하기 위해 "사용자가 직접 입력한 QNH" 는             */
    /*  GPS 기반 자동 보정값과 분리해 별도 변수로 유지한다.                    */
    /*  단위는 0.01 hPa 고정소수점이다.                                        */
    /* ---------------------------------------------------------------------- */
    int32_t manual_qnh_hpa_x100;

    /* ---------------------------------------------------------------------- */
    /*  GPS / IMU / HOME 정책 토글                                             */
    /*                                                                        */
    /*  gps_auto_equiv_qnh_enabled                                             */
    /*  - GPS hMSL과 현재 pressure를 이용해                                    */
    /*    "GPS와 일치하는 등가 sea-level pressure" 를 계산할지 여부          */
    /*                                                                        */
    /*  gps_bias_correction_enabled                                            */
    /*  - GPS를 장기 드리프트 없는 absolute anchor로 사용해                    */
    /*    baro bias를 천천히 잡을지 여부                                       */
    /*                                                                        */
    /*  imu_aid_enabled                                                        */
    /*  - IMU-aided 4-state filter를 display/audio 기본 후보로 둘지 여부       */
    /*  - no-IMU / IMU 결과는 항상 병렬 계산해서 둘 다 APP_STATE에 남긴다.     */
    /*                                                                        */
    /*  auto_home_capture_enabled                                              */
    /*  - 첫 valid fused altitude가 잡히면 home altitude를 자동 캡처한다.     */
    /* ---------------------------------------------------------------------- */
    uint8_t gps_auto_equiv_qnh_enabled;
    uint8_t gps_bias_correction_enabled;
    uint8_t imu_aid_enabled;
    uint8_t auto_home_capture_enabled;

    /* ---------------------------------------------------------------------- */
       /*  IMU 수직축 부호 + GY-86 poll gate                                      */
       /*                                                                        */
       /*  imu_vertical_sign                                                     */
       /*  - gravity projection을 사용해도 실제 장착 방향이 뒤집혀 있으면         */
       /*    수직 specific-force 부호가 반대로 나올 수 있으므로                   */
       /*    +1 / -1 을 런타임에서 바꿀 수 있게 유지한다.                         */
       /*                                                                        */
       /*  imu_poll_enabled                                                      */
       /*  - 1이면 MPU6050 polling을 기존 주기대로 수행한다.                     */
       /*  - 0이면 GY86_IMU_Task()가 MPU I2C read를 완전히 건너뛴다.              */
       /*                                                                        */
       /*  mag_poll_enabled                                                      */
       /*  - 1이면 HMC5883L polling을 기존 주기대로 수행한다.                    */
       /*  - 0이면 magnetometer I2C read를 건너뛰어 bus 부하를 줄인다.            */
       /*                                                                        */
       /*  ms5611_only                                                           */
       /*  - 1이면 "barometer only" 진단 모드다.                                */
       /*  - MPU/HMC probe와 polling을 모두 막고,                                */
       /*    ALTITUDE 서비스도 IMU aid / IMU audio source를 자동 비활성화한다.   */
       /*  - imu_poll_enabled / mag_poll_enabled 값은 보존되지만                 */
       /*    이 플래그가 1인 동안에는 무시된다.                                  */
       /* ---------------------------------------------------------------------- */
       int8_t  imu_vertical_sign;
       uint8_t imu_poll_enabled;
       uint8_t mag_poll_enabled;
       uint8_t ms5611_only;

    /* ---------------------------------------------------------------------- */
    /*  Pressure / display LPF 시정수                                          */
    /*                                                                        */
    /*  pressure_lpf_tau_ms : raw pressure -> pressure_filt LPF                */
    /*  vario_fast_tau_ms   : KF velocity -> fast vario LPF                    */
    /*  vario_slow_tau_ms   : KF velocity -> slow vario LPF                    */
    /*  display_lpf_tau_ms  : fused altitude -> display altitude LPF           */
    /* ---------------------------------------------------------------------- */
    uint16_t pressure_lpf_tau_ms;
    uint16_t vario_fast_tau_ms;
    uint16_t vario_slow_tau_ms;
    uint16_t display_lpf_tau_ms;

    /* ---------------------------------------------------------------------- */
    /*  정지 상태(rest) 전용 display 안정화                                    */
    /*                                                                        */
    /*  core filter / fast vario는 그대로 두고                                 */
    /*  "최종 화면값" 만 더 천천히 따라가게 만드는 계층이다.                  */
    /*                                                                        */
    /*  rest_display_enabled      : 표시 전용 stabilizer on/off               */
    /*  rest_detect_vario_cms     : 정지 판정용 slow vario 임계값             */
    /*  rest_detect_accel_mg      : 정지 판정용 IMU specific-force 임계값     */
    /*  rest_display_tau_ms       : 정지 시 display LPF 시정수                */
    /*  rest_display_hold_cm      : 이 범위 이내의 미세 변화는 화면값 유지    */
    /*  zupt_enabled              : 정지 상태일 때 velocity=0 pseudo update   */
    /* ---------------------------------------------------------------------- */
    uint8_t  rest_display_enabled;
    uint8_t  zupt_enabled;
    uint16_t reserved_rest0;
    uint16_t rest_detect_vario_cms;
    uint16_t rest_detect_accel_mg;
    uint16_t rest_display_tau_ms;
    uint16_t rest_display_hold_cm;

    /* ---------------------------------------------------------------------- */
    /*  Baro / GPS measurement noise 및 gate                                  */
    /*                                                                        */
    /*  baro_measurement_noise_cm      : 평온 구간에서 기본 baro altitude R    */
    /*  baro_adaptive_noise_max_cm     : residual이 클 때 올릴 최대 R          */
    /*  gps_measurement_noise_floor_cm : GPS altitude 최소 noise floor         */
    /*  gps_max_vacc_mm                : GPS vertical accuracy 허용 상한       */
    /*  gps_max_pdop_x100              : GPS pDOP 허용 상한                    */
    /*  gps_min_sats                   : 최소 위성 수                          */
    /*  gps_bias_tau_ms                : GPS로 baro bias를 따라잡는 속도       */
    /* ---------------------------------------------------------------------- */
    uint16_t baro_measurement_noise_cm;
    uint16_t baro_adaptive_noise_max_cm;
    uint16_t gps_measurement_noise_floor_cm;
    uint16_t gps_max_vacc_mm;
    uint16_t gps_max_pdop_x100;
    uint8_t  gps_min_sats;
    uint8_t  reserved2;
    uint16_t gps_bias_tau_ms;

    /* ---------------------------------------------------------------------- */
    /*  Baro velocity observation                                              */
    /*                                                                        */
    /*  pressure -> altitude derivative 로 만든 baro vario를                  */
    /*  velocity measurement로 KF에 직접 넣는다.                              */
    /*                                                                        */
    /*  baro_vario_lpf_tau_ms             : derivative LPF 시정수             */
    /*  baro_vario_measurement_noise_cms  : velocity update R                 */
    /* ---------------------------------------------------------------------- */
    uint16_t baro_vario_lpf_tau_ms;
    uint16_t baro_vario_measurement_noise_cms;

    /* ---------------------------------------------------------------------- */
    /*  IMU vertical specific-force 추정 튜닝                                  */
    /*                                                                        */
    /*  imu_gravity_tau_ms             : gyro-aided gravity estimator          */
    /*                                   accel correction 시정수              */
    /*  imu_accel_tau_ms               : vertical specific-force LPF           */
    /*  imu_accel_lsb_per_g            : MPU raw scale                         */
    /*  imu_vertical_deadband_mg       : 아주 작은 수직 성분 무시             */
    /*  imu_vertical_clip_mg           : 과도한 입력 clip                     */
    /*  imu_measurement_noise_cms2     : KF4 predict input noise 기준값       */
    /*  imu_gyro_lsb_per_dps           : gyro raw scale                        */
    /*  imu_attitude_accel_gate_mg     : accel norm이 1g에서 얼마나 벗어나면   */
    /*                                   attitude trust를 깎을지 결정         */
    /*  imu_predict_min_trust_permille : 이 값 이하 trust에서는               */
    /*                                   IMU predict weight를 0으로 둔다.     */
    /* ---------------------------------------------------------------------- */
    uint16_t imu_gravity_tau_ms;
    uint16_t imu_accel_tau_ms;
    uint16_t imu_accel_lsb_per_g;
    uint16_t imu_vertical_deadband_mg;
    uint16_t imu_vertical_clip_mg;
    uint16_t imu_measurement_noise_cms2;
    uint16_t imu_gyro_lsb_per_dps;
    uint16_t imu_attitude_accel_gate_mg;
    uint16_t imu_predict_min_trust_permille;

    /* ---------------------------------------------------------------------- */
    /*  Kalman process noise                                                   */
    /*                                                                        */
    /*  단위는 debug/IDE에서 조절하기 쉬운 "초당 규모" 로 둔다.               */
    /*  실제 구현에서는 dt를 곱해 process covariance에 반영한다.               */
    /* ---------------------------------------------------------------------- */
    uint16_t kf_q_height_cm_per_s;
    uint16_t kf_q_velocity_cms_per_s;
    uint16_t kf_q_baro_bias_cm_per_s;
    uint16_t kf_q_accel_bias_cms2_per_s;

    /* ---------------------------------------------------------------------- */
    /*  ALTITUDE debug page 전용 test vario audio                              */
    /*                                                                        */
    /*  debug_audio_enabled : test tone 출력 허용                              */
    /*  debug_audio_source  : 0=no-IMU fast vario, 1=IMU fast vario            */
    /*  audio_deadband_cms  : 이 값 이하의 vario는 무음                        */
    /*  audio_min/max_freq  : FM vario tone 주파수 범위                        */
    /*  audio_repeat_ms     : climb beep cadence 기준값                        */
    /*  audio_beep_ms       : climb beep tone 길이 기준값                      */
    /* ---------------------------------------------------------------------- */
    uint8_t  debug_audio_enabled;
    uint8_t  debug_audio_source;
    uint16_t audio_deadband_cms;
    uint16_t audio_min_freq_hz;
    uint16_t audio_max_freq_hz;
    uint16_t audio_repeat_ms;
    uint16_t audio_beep_ms;
} app_altitude_settings_t;

/* -------------------------------------------------------------------------- */
/*  BIKE DYNAMICS 사용자 설정                                                   */
/*                                                                            */
/*  기본 철학                                                                   */
/*  - lean / grade / lat-G / accel-decel 은                                    */
/*    모두 "프레임 기준 + reset 기준 + IMU core" 로 계산한다.                  */
/*  - GNSS / future OBD speed는 bias anchor 역할만 한다.                       */
/*  - mount axis / yaw trim 을 설정으로 노출해서                               */
/*    enclosure / PCB 장착 오차를 현장에서 바로 잡을 수 있게 한다.             */
/* -------------------------------------------------------------------------- */
typedef enum
{
    APP_BIKE_AXIS_POS_X = 0u,   /* sensor +X 가 차량 forward/left/up 방향일 때 */
    APP_BIKE_AXIS_NEG_X = 1u,   /* sensor -X 가 해당 방향일 때                  */
    APP_BIKE_AXIS_POS_Y = 2u,
    APP_BIKE_AXIS_NEG_Y = 3u,
    APP_BIKE_AXIS_POS_Z = 4u,
    APP_BIKE_AXIS_NEG_Z = 5u
} app_bike_axis_t;

typedef enum
{
    APP_BIKE_SPEED_SOURCE_NONE         = 0u,
    APP_BIKE_SPEED_SOURCE_IMU_FALLBACK = 1u,
    APP_BIKE_SPEED_SOURCE_GNSS         = 2u,
    APP_BIKE_SPEED_SOURCE_OBD          = 3u
} app_bike_speed_source_t;

typedef enum
{
    APP_BIKE_ESTIMATOR_MODE_IMU_ONLY   = 0u,
    APP_BIKE_ESTIMATOR_MODE_GNSS_AIDED = 1u,
    APP_BIKE_ESTIMATOR_MODE_OBD_AIDED  = 2u
} app_bike_estimator_mode_t;

/* -------------------------------------------------------------------------- */
/*  heading 공개 source                                                       */
/*                                                                            */
/*  이 값은 lean / grade 계산 경로와는 분리된 "보조 heading 출력" 전용이다.     */
/*  - GNSS : 충분한 속도와 headAcc 조건을 만족하는 경우                         */
/*  - MAG  : GNSS heading이 없고, tilt-compensated magnetic heading만         */
/*           사용할 수 있을 때                                                  */
/* -------------------------------------------------------------------------- */
typedef enum
{
    APP_BIKE_HEADING_SOURCE_NONE = 0u,
    APP_BIKE_HEADING_SOURCE_GNSS = 1u,
    APP_BIKE_HEADING_SOURCE_MAG  = 2u
} app_bike_heading_source_t;



typedef struct
{
    /* ---------------------------------------------------------------------- */
    /*  서비스 on/off 및 zero 정책                                              */
    /* ---------------------------------------------------------------------- */
    uint8_t enabled;                     /* 1이면 BIKE_DYNAMICS 서비스 사용         */
    uint8_t auto_zero_on_boot;           /* 1이면 첫 유효 IMU 샘플에서 자동 reset   */
    uint8_t gnss_aid_enabled;            /* 1이면 GNSS speed/course aid 사용        */
    uint8_t obd_aid_enabled;             /* 1이면 future OBD speed aid 사용         */

    /* ---------------------------------------------------------------------- */
    /*  장착 축 / yaw trim                                                      */
    /*                                                                        */
    /*  mount_forward_axis                                                     */
    /*  - 센서 보드의 어떤 축이 차량 forward 를 보는가                          */
    /*                                                                        */
    /*  mount_left_axis                                                        */
    /*  - 센서 보드의 어떤 축이 차량 left 를 보는가                             */
    /*                                                                        */
    /*  mount_yaw_trim_deg_x10                                                 */
    /*  - enclosure / PCB 기계 오차를 보정하는 수직축 주위 trim                */
    /*  - 단위: 0.1 deg                                                        */
    /* ---------------------------------------------------------------------- */
    uint8_t mount_forward_axis;          /* app_bike_axis_t raw                    */
    uint8_t mount_left_axis;             /* app_bike_axis_t raw                    */
    int16_t mount_yaw_trim_deg_x10;      /* 0.1 deg                                */

    /* ---------------------------------------------------------------------- */
    /*  IMU 스케일 / observer gate                                              */
    /*                                                                        */
    /*  imu_accel_lsb_per_g                                                    */
    /*  - 현재 MPU6050 ±4g 기본값은 8192                                       */
    /*                                                                        */
    /*  imu_gyro_lsb_per_dps_x10                                               */
    /*  - 0.1 LSB/dps 단위                                                     */
    /*  - 현재 MPU6050 ±500dps 기본값은 655 (=65.5 LSB/dps)                    */
    /* ---------------------------------------------------------------------- */
    uint16_t imu_accel_lsb_per_g;
    uint16_t imu_gyro_lsb_per_dps_x10;

    uint16_t imu_gravity_tau_ms;             /* gravity correction LPF tau           */
    uint16_t imu_linear_tau_ms;              /* level accel LPF tau                  */
    uint16_t imu_attitude_accel_gate_mg;     /* norm trust gate                      */
    uint16_t imu_jerk_gate_mg_per_s;         /* rough-road jerk trust gate           */
    uint16_t imu_predict_min_trust_permille; /* bias update를 허용할 최소 trust      */
    uint16_t imu_stale_timeout_ms;           /* raw sample stale timeout             */

    /* ---------------------------------------------------------------------- */
    /*  출력 후처리                                                             */
    /* ---------------------------------------------------------------------- */
    uint16_t output_deadband_mg;         /* lat/lon 출력 deadband                 */
    uint16_t output_clip_mg;             /* lat/lon 출력 clip                     */
    uint16_t lean_display_tau_ms;        /* lean 표시 LPF                         */
    uint16_t grade_display_tau_ms;       /* grade 표시 LPF                        */
    uint16_t accel_display_tau_ms;       /* lat/lon 표시 LPF                      */

    /* ---------------------------------------------------------------------- */
    /*  GNSS / OBD aid gate                                                     */
    /* ---------------------------------------------------------------------- */
    uint16_t gnss_min_speed_kmh_x10;         /* heading aid 최소 속도                */
    uint16_t gnss_max_speed_acc_kmh_x10;     /* GNSS speed accuracy gate             */
    uint16_t gnss_max_head_acc_deg_x10;      /* GNSS heading accuracy gate           */
    uint16_t gnss_bias_tau_ms;               /* external ref -> bias 적응 tau        */
    uint16_t gnss_outlier_gate_mg;           /* ref outlier reject gate              */

    uint16_t obd_stale_timeout_ms;           /* future OBD speed stale timeout       */
    uint16_t reserved0;                      /* 향후 확장용                          */
} app_bike_settings_t;


typedef struct
{
    app_gps_settings_t       gps;
    app_clock_settings_t     clock;
    app_backlight_settings_t backlight;
    app_uc1608_settings_t    uc1608;
    app_altitude_settings_t  altitude;
    app_bike_settings_t      bike;       /* 모터사이클 전용 dynamics 설정 저장소     */
} app_settings_t;


/* -------------------------------------------------------------------------- */
/*  RTC / CLOCK 공개 상태                                                      */
/*                                                                            */
/*  철학                                                                       */
/*  - 하드웨어 RTC는 UTC 기준으로만 유지한다.                                  */
/*  - timezone 적용/요일 계산/로컬 시간 전개는 APP 계층에서 수행한다.          */
/*  - UI는 이 저장소의 valid 플래그를 보고 시간을 그릴지 "--:--"를 그릴지      */
/*    결정할 수 있다.                                                          */
/*                                                                            */
/*  backup domain 사용 규칙                                                    */
/*  - RTC date/time register : 실제 시계 자체                                  */
/*  - RTC backup register   : clock 전용 magic / config / validity metadata    */
/*  - BKPSRAM               : RTC와 무관한 다른 persistent 용도(APP_FAULT 등)  */
/* -------------------------------------------------------------------------- */

typedef enum
{
    APP_CLOCK_SYNC_SOURCE_NONE           = 0u,
    APP_CLOCK_SYNC_SOURCE_BOOT_DEFAULT   = 1u,
    APP_CLOCK_SYNC_SOURCE_MANUAL         = 2u,
    APP_CLOCK_SYNC_SOURCE_GPS_FULL       = 3u,
    APP_CLOCK_SYNC_SOURCE_GPS_PERIODIC   = 4u,
    APP_CLOCK_SYNC_SOURCE_EXTERNAL_STUB  = 5u
} app_clock_sync_source_t;

typedef struct
{
    uint16_t year;                       /* 4-digit year                              */
    uint8_t  month;                      /* 1..12                                     */
    uint8_t  day;                        /* 1..31                                     */
    uint8_t  hour;                       /* 0..23                                     */
    uint8_t  min;                        /* 0..59                                     */
    uint8_t  sec;                        /* 0..59                                     */
    uint8_t  weekday;                    /* 1..7, Monday=1                            */
} app_clock_calendar_t;

typedef struct
{
    uint8_t  hours;                      /* HAL_RTC_GetTime 결과 raw hour             */
    uint8_t  minutes;                    /* HAL_RTC_GetTime 결과 raw minute           */
    uint8_t  seconds;                    /* HAL_RTC_GetTime 결과 raw second           */
    uint8_t  time_format;                /* HAL raw time format                       */
    uint32_t sub_seconds;                /* HAL raw subsecond                         */
    uint32_t second_fraction;            /* HAL raw second fraction                   */
    uint32_t daylight_saving;            /* HAL raw daylight saving field             */
    uint32_t store_operation;            /* HAL raw store operation field             */
} app_clock_rtc_time_raw_t;

typedef struct
{
    uint8_t week_day;                    /* HAL_RTC_GetDate 결과 raw weekday          */
    uint8_t month;                       /* HAL_RTC_GetDate 결과 raw month            */
    uint8_t date;                        /* HAL_RTC_GetDate 결과 raw day-of-month     */
    uint8_t year_2digit;                 /* HAL_RTC_GetDate 결과 raw year(00..99)     */
} app_clock_rtc_date_raw_t;

typedef struct
{
    bool     initialized;                /* APP_CLOCK_Init 완료 여부                  */
    bool     backup_config_valid;        /* RTC backup register config magic 유효     */
    bool     rtc_time_valid;             /* 현재 RTC 값이 신뢰 가능한가               */
    bool     rtc_read_valid;             /* 마지막 HAL read 결과가 유효했는가         */
    bool     gps_candidate_valid;        /* 현재 GPS time/date가 sync 후보인가        */
    bool     gps_auto_sync_enabled_runtime; /* runtime에 반영된 auto-sync 상태        */
    bool     gps_last_sync_success;      /* 마지막 GPS sync 성공 여부                 */
    bool     gps_last_sync_was_full;     /* 마지막 GPS sync가 full date/time였는가    */
    bool     gps_resolved_seen;          /* 이번 부팅에서 GPS fully resolved 관측 여부 */
    bool     timezone_config_valid;      /* timezone 값 범위가 유효한가               */

    int8_t   timezone_quarters;          /* UTC offset / 15 min 단위                  */
    uint8_t  gps_sync_interval_minutes;  /* periodic GPS sync 주기                    */
    uint8_t  last_sync_source;           /* app_clock_sync_source_t raw               */
    uint8_t  reserved0;                  /* 정렬/향후 확장용                           */

    uint32_t last_hw_read_ms;            /* 마지막 RTC 하드웨어 read 시각             */
    uint32_t last_hw_set_ms;             /* 마지막 RTC 하드웨어 write 시각            */
    uint32_t last_validity_change_ms;    /* valid 플래그 최종 변경 시각               */
    uint32_t last_gps_sync_ms;           /* 마지막 GPS sync 완료 시각                 */
    uint32_t next_gps_sync_due_ms;       /* 다음 GPS periodic sync 예정 시각          */

    uint32_t gps_full_sync_count;        /* GPS full sync 누적 횟수                   */
    uint32_t gps_periodic_sync_count;    /* GPS time-only sync 누적 횟수              */
    uint32_t manual_set_count;           /* 수동 설정 누적 횟수                       */
    uint32_t invalid_read_count;         /* invalid RTC read 관측 횟수                */
    uint32_t rtc_read_count;             /* RTC read 성공 누적 횟수                   */
    uint32_t rtc_write_count;            /* RTC write 성공 누적 횟수                  */
    uint32_t rtc_error_count;            /* HAL RTC read/write 실패 누적 횟수         */
    uint32_t backup_write_count;         /* RTC backup config write 누적 횟수         */
    uint32_t backup_read_count;          /* RTC backup config read 누적 횟수          */
    uint32_t last_hal_status;            /* 마지막 HAL_RTC_* 반환값 raw               */

    app_clock_rtc_time_raw_t rtc_time_raw; /* 마지막 RTC raw time snapshot            */
    app_clock_rtc_date_raw_t rtc_date_raw; /* 마지막 RTC raw date snapshot            */

    app_clock_calendar_t utc;            /* 하드웨어 RTC에서 읽은 UTC 시간            */
    app_clock_calendar_t local;          /* timezone 적용 후 local 시간               */
    app_clock_calendar_t last_gps_utc;   /* 마지막으로 채택한 GPS UTC 시간            */
} app_clock_state_t;

/* -------------------------------------------------------------------------- */
/*  UBX raw payload structures                                                 */
/*                                                                            */
/*  "raw" 는 GPS 칩이 보낸 payload를 가능한 한 그대로 담는 구조체다.          */
/*  디버깅할 때 가장 믿을 수 있는 자료는 raw 이다.                              */
/* -------------------------------------------------------------------------- */

typedef struct APP_PACKED
{
    uint32_t iTOW;
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  min;
    uint8_t  sec;
    uint8_t  valid;
    uint32_t tAcc;
    int32_t  nano;
    uint8_t  fixType;
    uint8_t  flags;
    uint8_t  flags2;
    uint8_t  numSV;
    int32_t  lon;
    int32_t  lat;
    int32_t  height;
    int32_t  hMSL;
    uint32_t hAcc;
    uint32_t vAcc;
    int32_t  velN;
    int32_t  velE;
    int32_t  velD;
    int32_t  gSpeed;
    int32_t  headMot;
    uint32_t sAcc;
    uint32_t headAcc;
    uint16_t pDOP;
    uint16_t flags3;
    uint8_t  reserved0[4];
    int32_t  headVeh;
    int16_t  magDec;
    uint16_t magAcc;
} ubx_nav_pvt_t;

typedef struct APP_PACKED
{
    uint32_t iTOW;
    uint8_t  version;
    uint8_t  numSvs;
    uint8_t  reserved0[2];
} ubx_nav_sat_header_t;

typedef struct APP_PACKED
{
    uint8_t  gnssId;
    uint8_t  svId;
    uint8_t  cno;
    int8_t   elev;
    int16_t  azim;
    int16_t  prRes;
    uint32_t flags;
} ubx_nav_sat_sv_t;

typedef struct APP_PACKED
{
    uint8_t clsID;
    uint8_t msgID;
} ubx_ack_payload_t;

/* -------------------------------------------------------------------------- */
/*  UI / App friendly GPS state                                                */
/*                                                                            */
/*  raw UBX payload를 그대로 들고 있으면서도, UI가 바로 읽기 쉽게               */
/*  전개한 필드를 따로 둔다.                                                    */
/* -------------------------------------------------------------------------- */

typedef struct
{
    uint8_t gnss_id;
    uint8_t sv_id;
    uint8_t cno_dbhz;
    int8_t  elevation_deg;
    int16_t azimuth_deg;
    int16_t pseudorange_res_dm;   /* UBX는 0.1 m 단위 */

    uint8_t quality_ind;
    uint8_t used_in_solution;
    uint8_t health;
    uint8_t diff_corr;
    uint8_t smoothed;
    uint8_t orbit_source;
    uint8_t eph_avail;
    uint8_t alm_avail;
    uint8_t ano_avail;
    uint8_t aop_avail;
    uint8_t sbas_corr_used;
    uint8_t rtcm_corr_used;
    uint8_t slas_corr_used;
    uint8_t spartn_corr_used;
    uint8_t pr_corr_used;
    uint8_t cr_corr_used;
    uint8_t do_corr_used;
    uint8_t clas_corr_used;
    uint8_t visible;

    uint32_t flags_raw;
} app_gps_sat_t;

typedef struct
{
    uint32_t iTOW_ms;

    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  min;
    uint8_t  sec;

    uint8_t  raw_valid;
    uint8_t  raw_flags;
    uint8_t  raw_flags2;
    uint16_t raw_flags3;

    uint32_t tAcc_ns;
    int32_t  nano_ns;

    uint8_t  valid_date;
    uint8_t  valid_time;
    uint8_t  fully_resolved;
    uint8_t  valid_mag;

    uint8_t  fixType;
    bool     fixOk;
    bool     valid;

    uint8_t  diff_soln;
    uint8_t  psm_state;
    uint8_t  head_veh_valid;
    uint8_t  carr_soln;
    uint8_t  confirmed_avail;
    uint8_t  confirmed_date;
    uint8_t  confirmed_time;
    uint8_t  invalid_llh;
    uint8_t  last_correction_age;
    uint8_t  auth_time;

    uint8_t  numSV_nav_pvt;
    uint8_t  numSV_visible;
    uint8_t  numSV_used;

    int32_t  lon;                 /* deg * 1e-7 */
    int32_t  lat;                 /* deg * 1e-7 */
    int32_t  height;              /* mm */
    int32_t  hMSL;                /* mm */

    int32_t  velN;                /* mm/s */
    int32_t  velE;                /* mm/s */
    int32_t  velD;                /* mm/s */
    int32_t  gSpeed;              /* mm/s */
    int32_t  headMot;             /* deg * 1e-5 */
    int32_t  headVeh;             /* deg * 1e-5 */

    uint32_t hAcc;                /* mm */
    uint32_t vAcc;                /* mm */
    uint32_t sAcc;                /* mm/s */
    uint32_t headAcc;             /* deg * 1e-5 */
    uint16_t pDOP;                /* 0.01 scaled */

    int16_t  magDec;              /* 1e-2 deg */
    uint16_t magAcc;              /* 1e-2 deg */

    float    speed_llh_mps;       /* lat/lon 변화량으로 계산한 파생 속도 */
    float    speed_llh_kmh;

    uint32_t last_update_ms;
    uint32_t last_fix_ms;
} gps_fix_basic_t;

typedef struct
{
    bool     valid;
    uint8_t  msg_class;
    uint8_t  msg_id;
    uint16_t payload_len;
    uint32_t tick_ms;
    uint8_t  payload[APP_GPS_LAST_RAW_MAX];
} app_gps_last_frame_t;

typedef struct
{
    bool     valid;
    uint8_t  cls_id;
    uint8_t  msg_id;
    uint32_t tick_ms;
} app_gps_ack_t;

typedef struct
{
    bool     valid;
    uint32_t received_ms;
    char     sw_version[31];
    char     hw_version[11];
    char     extensions[APP_GPS_MON_VER_EXT_MAX][31];
    uint8_t  extension_count;
} app_gps_mon_ver_t;

typedef struct
{
    bool     query_started;
    bool     query_complete;
    bool     query_failed;

    uint8_t  query_attempts;
    uint8_t  response_layer;

    uint32_t last_query_tx_ms;
    uint32_t last_query_rx_ms;

    bool     uart1_enabled;
    uint32_t uart1_baudrate;
    bool     uart1_in_ubx;
    bool     uart1_in_nmea;
    bool     uart1_out_ubx;
    bool     uart1_out_nmea;

    uint16_t meas_rate_ms;
    uint8_t  pm_operate_mode;

    bool     gps_ena;
    bool     gps_l1ca_ena;

    bool     sbas_ena;
    bool     sbas_l1ca_ena;

    bool     gal_ena;
    bool     gal_e1_ena;

    bool     bds_ena;
    bool     bds_b1i_ena;
    bool     bds_b1c_ena;

    bool     qzss_ena;
    bool     qzss_l1ca_ena;
    bool     qzss_l1s_ena;

    bool     glo_ena;
    bool     glo_l1_ena;

    uint8_t  msgout_nav_pvt_uart1;
    uint8_t  msgout_nav_sat_uart1;
} app_gps_runtime_config_t;

typedef struct
{
    bool initialized;
    bool configured;
    bool uart_rx_running;

    uint32_t uart_irq_bytes;
    uint32_t uart_rearm_fail_count;
    uint32_t uart_ring_overflow_count;
    uint32_t uart_error_count;
    uint32_t uart_error_ore_count;
    uint32_t uart_error_fe_count;
    uint32_t uart_error_ne_count;
    uint32_t uart_error_pe_count;
    uint16_t rx_ring_level;
    uint16_t rx_ring_high_watermark;

    uint32_t rx_bytes;
    uint32_t frames_ok;
    uint32_t frames_bad_checksum;
    uint32_t frames_dropped_oversize;
    uint32_t parser_resync_count;
    uint32_t unknown_msg_count;
    uint32_t ack_ack_count;
    uint32_t ack_nak_count;

    uint32_t last_rx_ms;
    uint32_t last_message_ms;

    app_gps_last_frame_t last_frame;
    app_gps_last_frame_t last_unknown_frame;

    bool          nav_pvt_valid;
    bool          nav_sat_valid;
    bool          cfg_valget_valid;
    bool          ack_ack_valid;
    bool          ack_nak_valid;
    bool          mon_ver_valid;

    ubx_nav_pvt_t       nav_pvt;
    ubx_nav_sat_header_t nav_sat_header;
    ubx_nav_sat_sv_t     nav_sat_sv[APP_GPS_MAX_SATS];
    uint8_t              nav_sat_count;

    ubx_ack_payload_t last_ack_ack_raw;
    ubx_ack_payload_t last_ack_nak_raw;

    uint8_t  cfg_valget_payload[APP_GPS_LAST_RAW_MAX];
    uint16_t cfg_valget_payload_len;

    app_gps_ack_t            ack_ack;
    app_gps_ack_t            ack_nak;
    app_gps_mon_ver_t        mon_ver;
    app_gps_runtime_config_t runtime_cfg;

    gps_fix_basic_t fix;
    uint8_t         sat_count_visible;
    uint8_t         sat_count_used;
    app_gps_sat_t   sats[APP_GPS_MAX_SATS];
} app_gps_state_t;


/* -------------------------------------------------------------------------- */
/*  GY-86 / IMU raw state                                                      */
/*                                                                            */
/*  이번 확장에서는 APP_STATE가 GPS뿐 아니라                                  */
/*  IMU(GY-86)와 DS18B20의 "유일한 공개 저장소" 역할도 맡는다.                 */
/*                                                                            */
/*  중요한 규칙                                                                */
/*  - 센서 드라이버의 static 내부 상태는 드라이버 파일 안에만 숨겨 둔다.        */
/*  - 다른 파일(UI / logger / state machine)은 APP_STATE만 본다.               */
/*  - 그래서 raw 측정값, 디버그 카운터, 센서 ID, 마지막 오류 같은 정보는        */
/*    가능한 한 APP_STATE 안에 구조체로 정리해서 넣는다.                       */
/* -------------------------------------------------------------------------- */

typedef enum
{
    APP_IMU_BACKEND_NONE     = 0u,
    APP_IMU_BACKEND_MPU6050  = 1u,
    APP_IMU_BACKEND_HMC5883L = 2u,
    APP_IMU_BACKEND_MS5611   = 3u
} app_imu_backend_id_t;

/* GY-86 내부 각 칩의 비트 마스크.
 * status_flags / detected_mask / init_ok_mask 에 공통으로 사용한다. */
enum
{
    APP_GY86_DEVICE_MPU  = 0x01u,
    APP_GY86_DEVICE_MAG  = 0x02u,
    APP_GY86_DEVICE_BARO = 0x04u
};

/* 최신 raw 샘플의 유효 비트.
 * "장치가 존재하는가?" 와 "최근 샘플이 유효한가?" 는 구분한다. */
enum
{
    APP_GY86_STATUS_MPU_VALID  = 0x01u,
    APP_GY86_STATUS_MAG_VALID  = 0x02u,
    APP_GY86_STATUS_BARO_VALID = 0x04u
};

/* MPU/가속도/자이로 raw 값 저장소 */
typedef struct
{
    uint32_t timestamp_ms;     /* 이 raw 샘플을 읽은 SysTick 시각               */
    uint32_t sample_count;     /* 누적 샘플 수                                  */

    int16_t  accel_x_raw;      /* MPU 가속도 X raw                             */
    int16_t  accel_y_raw;      /* MPU 가속도 Y raw                             */
    int16_t  accel_z_raw;      /* MPU 가속도 Z raw                             */

    int16_t  gyro_x_raw;       /* MPU 자이로 X raw                             */
    int16_t  gyro_y_raw;       /* MPU 자이로 Y raw                             */
    int16_t  gyro_z_raw;       /* MPU 자이로 Z raw                             */

    int16_t  temp_raw;         /* MPU 내부 온도 raw                            */
    int16_t  temp_cdeg;        /* MPU 내부 온도, 0.01 degC 고정소수점          */
} app_gy86_mpu_raw_t;

/* 자력계 raw 값 저장소 */
typedef struct
{
    uint32_t timestamp_ms;     /* 이 raw 샘플을 읽은 SysTick 시각               */
    uint32_t sample_count;     /* 누적 샘플 수                                  */

    int16_t  mag_x_raw;        /* 자력계 X raw                                  */
    int16_t  mag_y_raw;        /* 자력계 Y raw                                  */
    int16_t  mag_z_raw;        /* 자력계 Z raw                                  */
} app_gy86_mag_raw_t;

/* 기압계 raw + 보정계수 저장소 */
typedef struct
{
    uint32_t timestamp_ms;         /* 완전한 D1/D2 한 세트를 계산한 시각         */
    uint32_t sample_count;         /* 누적 유효 샘플 수                           */

    uint32_t d1_raw;               /* MS5611 pressure ADC raw                     */
    uint32_t d2_raw;               /* MS5611 temperature ADC raw                  */

    uint16_t prom_c[7];            /* C1..C6 사용, [0]은 비워 둠                  */

    int32_t  temp_cdeg;            /* 보정 후 온도, 0.01 degC                     */
    int32_t  pressure_hpa_x100;    /* 보정 후 기압, 0.01 hPa                      */
    int32_t  pressure_pa;          /* 보정 후 기압, Pa                            */
} app_gy86_baro_raw_t;


#ifndef APP_GY86_BARO_SENSOR_SLOTS
#define APP_GY86_BARO_SENSOR_SLOTS 2u
#endif

/* -------------------------------------------------------------------------- */
/*  dual barometer app-state 공개 슬롯                                         */
/*                                                                            */
/*  목적                                                                       */
/*  - 기존 APP_STATE.gy86.baro 는 "상위 계층이 그대로 쓰는 fused 결과" 하나만   */
/*    유지한다.                                                                */
/*  - 아래 구조체는 그 fused 결과가 어떻게 만들어졌는지 확인하기 위한          */
/*    "센서별 진단 슬롯" 이다.                                                 */
/*                                                                            */
/*  사용 규칙                                                                  */
/*  - 앱/UI/self-test 는 이 구조체를 read-only 로만 사용한다.                  */
/*  - low-level driver(GY86_IMU.c)만 이 값을 채운다.                           */
/*  - sensor 1개 빌드에서도 slot 수는 2개를 유지하고, 미사용 slot은 0으로 둔다. */
/*                                                                            */
/*  pressure_pa / pressure_hpa_x100                                            */
/*  - 이 프로젝트의 MS5611 경로에서는                                           */
/*    hPa*100 과 Pa 가 수치적으로 동일한 스케일이므로                           */
/*    둘 다 함께 유지해도 값은 같은 숫자 범위를 가진다.                        */
/*                                                                            */
/*  aligned_pressure_pa                                                        */
/*  - secondary sensor의 static offset(bias)을 제거해                           */
/*    fusion 판단에 실제로 사용한 pressure 값                                  */
/*                                                                            */
/*  bias_pa                                                                    */
/*  - primary 기준으로 추정된 상대 압력 오프셋                                 */
/*                                                                            */
/*  residual_pa                                                                */
/*  - aligned_pressure_pa 와 primary pressure 사이의 잔차                      */
/*  - outlier reject / disagreement gate가 보는 핵심 값                        */
/* -------------------------------------------------------------------------- */
typedef struct
{
    uint8_t  configured;           /* 이 slot이 실제 하드웨어 배치상 존재하는가   */
    uint8_t  online;               /* 현재 probe/init/poll 기준 살아 있는가       */
    uint8_t  valid;                /* 최근 sample이 완전한 D1/D2 세트인가         */
    uint8_t  fresh;                /* fusion freshness timeout 안에 들어오는가    */

    uint8_t  selected;             /* 이번 fused sample 계산에 실제 사용되었는가  */
    uint8_t  rejected;             /* disagreement/stale gate로 제외되었는가      */
    uint8_t  bus_id;               /* 1=I2C1, 2=I2C2                              */
    uint8_t  addr_7bit;            /* 사람이 읽기 쉬운 7-bit I2C 주소             */

    uint8_t  error_streak;         /* runtime 연속 오류 streak                    */
    uint8_t  reserved0;            /* 정렬/향후 확장용                            */
    uint16_t weight_permille;      /* 이번 fusion에서 사용된 가중치 0..1000       */

    uint32_t timestamp_ms;         /* 마지막 완전 샘플 시각                       */
    uint32_t sample_count;         /* 이 센서 단독 누적 sample count              */

    int32_t  temp_cdeg;            /* 이 센서 단독 온도, 0.01 degC               */
    int32_t  pressure_hpa_x100;    /* 이 센서 단독 pressure, 0.01 hPa            */
    int32_t  pressure_pa;          /* 이 센서 단독 pressure, Pa                  */

    int32_t  aligned_pressure_pa;  /* bias 제거 후 fusion에 투입한 pressure       */
    int32_t  bias_pa;              /* primary 대비 추정한 static offset           */
    int32_t  residual_pa;          /* aligned - primary residual                  */
} app_gy86_baro_sensor_state_t;

/* -------------------------------------------------------------------------- */
/*  dual barometer fusion summary flag                                         */
/*                                                                            */
/*  SINGLE_SENSOR        : 실제 publish가 1개 센서만으로 이뤄졌는가             */
/*  STALE_FALLBACK       : 다른 센서는 stale 해서 제외되었는가                 */
/*  DISAGREE_REJECT      : 다른 센서는 disagreement gate로 제외되었는가        */
/*  OFFSET_TRACK_ACTIVE  : bias 추정값이 의미 있게 활성화되었는가              */
/* -------------------------------------------------------------------------- */
enum
{
    APP_GY86_BARO_FUSION_FLAG_SINGLE_SENSOR       = 0x01u,
    APP_GY86_BARO_FUSION_FLAG_STALE_FALLBACK      = 0x02u,
    APP_GY86_BARO_FUSION_FLAG_DISAGREE_REJECT     = 0x04u,
    APP_GY86_BARO_FUSION_FLAG_OFFSET_TRACK_ACTIVE = 0x08u
};

/* GY-86 전체 모듈 디버그 정보 */
typedef struct
{
    uint8_t  accelgyro_backend_id; /* 현재 accel/gyro backend ID                  */
    uint8_t  mag_backend_id;       /* 현재 magnetometer backend ID                */
    uint8_t  baro_backend_id;      /* 현재 barometer backend ID                   */

    uint8_t  detected_mask;        /* I2C 응답/ID 기준으로 감지된 칩 비트마스크   */
    uint8_t  init_ok_mask;         /* init까지 정상 완료된 칩 비트마스크          */

    uint8_t  last_hal_status_mpu;  /* 마지막 MPU HAL status                       */
    uint8_t  last_hal_status_mag;  /* 마지막 MAG HAL status                       */
    uint8_t  last_hal_status_baro; /* 마지막 BARO HAL status                      */

    uint8_t  mpu_whoami;           /* MPU WHO_AM_I raw                            */
    uint8_t  mag_id_a;             /* HMC5883L ID A raw                           */
    uint8_t  mag_id_b;             /* HMC5883L ID B raw                           */
    uint8_t  mag_id_c;             /* HMC5883L ID C raw                           */

    uint8_t  ms5611_state;         /* D1/D2 변환 state machine 내부 상태          */

    /* ---------------------------------------------------------------------- */
    /*  dual barometer fusion summary                                          */
    /*                                                                        */
    /*  baro_device_slots        : compile-time 으로 노출하는 slot 수          */
    /*  baro_fused_sensor_count  : 이번 fused sample에 실제 사용된 센서 수     */
    /*  baro_primary_sensor_index: 현재 primary/fallback 기준 sensor index     */
    /*  baro_fusion_flags        : APP_GY86_BARO_FUSION_FLAG_* bitmask         */
    /*                                                                        */
    /*  baro_sensor_delta_pa_raw                                                */
    /*  - sensor1 - sensor0 의 raw pressure 차이                               */
    /*                                                                        */
    /*  baro_sensor_delta_pa_aligned                                            */
    /*  - bias 보정 후 sensor1 - sensor0 차이                                   */
    /*  - disagreement gate는 이 값을 본다                                     */
    /* ---------------------------------------------------------------------- */
    uint8_t  baro_device_slots;
    uint8_t  baro_fused_sensor_count;
    uint8_t  baro_primary_sensor_index;
    uint8_t  baro_fusion_flags;
    int32_t  baro_sensor_delta_pa_raw;
    int32_t  baro_sensor_delta_pa_aligned;

    uint32_t init_attempt_count;   /* 전체 init/re-probe 시도 횟수                */
    uint32_t last_init_attempt_ms; /* 마지막 init 시도 시각                       */

    uint16_t mpu_poll_period_ms;   /* 현재 accel/gyro polling 주기                */
    uint16_t mag_poll_period_ms;   /* 현재 magnetometer polling 주기              */
    uint16_t baro_poll_period_ms;  /* 현재 pressure 완전 샘플 주기                */

    uint32_t mpu_last_ok_ms;       /* 마지막 MPU read 성공 시각                   */
    uint32_t mag_last_ok_ms;       /* 마지막 MAG read 성공 시각                   */
    uint32_t baro_last_ok_ms;      /* 마지막 BARO full sample 성공 시각           */

    uint32_t mpu_error_count;      /* MPU 누적 read/init 오류 횟수                */
    uint32_t mag_error_count;      /* MAG 누적 read/init 오류 횟수                */
    uint32_t baro_error_count;     /* BARO 누적 read/init 오류 횟수               */
} app_gy86_debug_t;

/* GY-86 전체 공개 상태 */
typedef struct
{
    bool              initialized; /* 적어도 하나 이상의 하위 칩이 init 성공      */
    uint8_t           status_flags;/* 최신 raw 유효 비트                          */
    uint32_t          last_update_ms; /* 마지막으로 어떤 하위 칩이든 업데이트된 시각 */

    app_gy86_mpu_raw_t           mpu;        /* accel/gyro/raw 저장소                    */
    app_gy86_mag_raw_t           mag;        /* magnetometer raw 저장소                  */
    app_gy86_baro_raw_t          baro;       /* fused pressure raw 저장소                */
    app_gy86_baro_sensor_state_t baro_sensor[APP_GY86_BARO_SENSOR_SLOTS];
                                             /* sensor별 dual-baro 진단 슬롯             */
    app_gy86_debug_t             debug;      /* GY-86 디버그 저장소                      */
} app_gy86_state_t;

/* -------------------------------------------------------------------------- */
/*  DS18B20 raw state                                                          */
/* -------------------------------------------------------------------------- */

/* DS18B20 상태 비트 */
enum
{
    APP_DS18B20_STATUS_PRESENT   = 0x01u,
    APP_DS18B20_STATUS_VALID     = 0x02u,
    APP_DS18B20_STATUS_BUSY      = 0x04u,
    APP_DS18B20_STATUS_ROM_VALID = 0x08u,
    APP_DS18B20_STATUS_CRC_OK    = 0x10u
};

typedef enum
{
    APP_DS18B20_PHASE_UNINIT          = 0u,
    APP_DS18B20_PHASE_IDLE            = 1u,
    APP_DS18B20_PHASE_WAIT_CONVERSION = 2u
} app_ds18b20_phase_t;

typedef enum
{
    APP_DS18B20_ERR_NONE            = 0u,
    APP_DS18B20_ERR_NO_PRESENCE     = 1u,
    APP_DS18B20_ERR_BUS_STUCK_LOW   = 2u,
    APP_DS18B20_ERR_ROM_CRC         = 3u,
    APP_DS18B20_ERR_CONFIG_WRITE    = 4u,
    APP_DS18B20_ERR_SCRATCHPAD_CRC  = 5u,
    APP_DS18B20_ERR_READ_TRANSACTION = 6u
} app_ds18b20_error_t;

#ifndef APP_DS18B20_TEMP_INVALID
#define APP_DS18B20_TEMP_INVALID   ((int16_t)-32768)
#endif

/* DS18B20 raw + scratchpad 저장소 */
typedef struct
{
    uint32_t timestamp_ms;         /* 마지막 정상 샘플 시각                        */
    uint32_t sample_count;         /* 누적 정상 샘플 수                            */

    int16_t  raw_temp_lsb;         /* scratchpad temperature raw                   */
    int16_t  temp_c_x100;          /* 0.01 degC                                    */
    int16_t  temp_f_x100;          /* 0.01 degF                                    */

    int8_t   alarm_high_c;         /* TH register                                  */
    int8_t   alarm_low_c;          /* TL register                                  */

    uint8_t  config_reg;           /* config register raw                          */
    uint8_t  resolution_bits;      /* 9/10/11/12 bit                               */

    uint8_t  rom_code[8];          /* READ ROM 결과                                */
    uint8_t  scratchpad[9];        /* 마지막 scratchpad raw                        */

    uint8_t  crc_expected;         /* scratchpad[8]                                */
    uint8_t  crc_computed;         /* host 계산 CRC                                */
} app_ds18b20_raw_t;

/* DS18B20 디버그 상태 */
typedef struct
{
    uint8_t  phase;                /* app_ds18b20_phase_t                          */
    uint8_t  last_error;           /* app_ds18b20_error_t                          */

    uint32_t init_attempt_count;   /* init/re-probe 시도 횟수                      */
    uint32_t conversion_start_count; /* Convert T 시작 횟수                        */
    uint32_t read_complete_count;  /* scratchpad read 성공 횟수                    */

    uint32_t bus_reset_count;      /* 1-Wire reset pulse 시도 횟수                 */
    uint32_t presence_fail_count;  /* presence pulse 실패 횟수                     */
    uint32_t crc_fail_count;       /* scratchpad CRC 실패 횟수                     */
    uint32_t transaction_fail_count; /* read/write 트랜잭션 실패 횟수             */

    uint32_t last_init_attempt_ms; /* 마지막 init 시도 시각                        */
    uint32_t last_conversion_start_ms; /* 마지막 Convert T 시작 시각               */
    uint32_t last_read_complete_ms;/* 마지막 정상 read 완료 시각                   */

    uint32_t next_action_ms;       /* 다음 state machine 액션 예정 시각            */
    uint16_t conversion_time_ms;   /* 현재 해상도 기준 변환 시간                   */

    uint32_t last_transaction_us;  /* 마지막 blocking 1-Wire 트랜잭션 소요 시간    */
} app_ds18b20_debug_t;

/* DS18B20 전체 공개 상태 */
typedef struct
{
    bool                 initialized; /* presence 및 기본 초기화 성공 여부         */
    uint8_t              status_flags;/* present/valid/busy/crc 등 비트            */
    uint32_t             last_update_ms; /* 마지막 상태 갱신 시각                   */

    app_ds18b20_raw_t    raw;         /* raw 저장소                                */
    app_ds18b20_debug_t  debug;       /* 디버그 저장소                              */
} app_ds18b20_state_t;

/* -------------------------------------------------------------------------- */
/*  센서 디버그 UI 전용 스냅샷                                                  */
/*                                                                            */
/*  GPS UI는 이미 경량 snapshot을 따로 두고 있으므로,                           */
/*  센서 페이지도 같은 철학으로                                                */
/*  "센서 두 덩어리만 한 번에" 복사하는 전용 구조체를 둔다.                    */
/* -------------------------------------------------------------------------- */
typedef struct
{
    app_gy86_state_t     gy86;
    app_ds18b20_state_t  ds18b20;
} app_sensor_debug_snapshot_t;


/* -------------------------------------------------------------------------- */
/*  SD / FATFS 공개 상태                                                       */
/*                                                                            */
/*  이번 SD 브링업에서는                                                        */
/*  - 카드 detect raw/stable 상태                                               */
/*  - mount / unmount / retry 카운터                                            */
/*  - HAL SD handle 디버그 값                                                   */
/*  - HAL_SD_GetCardInfo 로 얻은 카드 정보                                      */
/*  - FatFs volume 구조체에서 읽을 수 있는 FAT 메타데이터                      */
/*  - root 디렉터리 개수/샘플 이름                                              */
/*  를 한 구조체에 모아 둔다.                                                   */
/*                                                                            */
/*  주의                                                                      */
/*  - 이 구조체는 'UI / logger가 읽기 쉬운 공개 저장소'다.                      */
/*  - detect debounce의 ISR 세부 구현 상태는 APP_SD.c 내부 static runtime 에     */
/*    별도로 두고, 여기에는 사람이 보고 싶은 결과 값만 공개한다.               */
/* -------------------------------------------------------------------------- */
typedef struct
{
    /* ------------------------------ */
    /*  detect / mount 기본 상태       */
    /* ------------------------------ */
    bool     detect_raw_present;        /* 마지막 raw DET 핀 샘플 결과               */
    bool     detect_stable_present;     /* debounce 후 확정된 카드 삽입 상태         */
    bool     detect_debounce_pending;   /* 현재 detect debounce 대기 중인가          */

    bool     initialized;               /* HAL_SD_Init 까지 성공했는가               */
    bool     mounted;                   /* f_mount 성공 상태                         */
    bool     fat_valid;                 /* f_getfree 기반 FAT 메타데이터 유효 여부   */
    bool     is_fat32;                  /* 현재 mount 된 파일시스템이 FAT32 인가     */

    /* ------------------------------ */
    /*  작은 raw enum/상태 값          */
    /* ------------------------------ */
    uint8_t  fs_type;                   /* FatFs fs_type raw                         */
    uint8_t  card_type;                 /* HAL card type raw                         */
    uint8_t  card_version;              /* HAL card version raw                      */
    uint8_t  card_class;                /* HAL card class raw                        */
    uint8_t  hal_state;                 /* hsd.State snapshot                        */
    uint8_t  transfer_state;            /* BSP_SD_GetCardState() 결과                */
    uint8_t  last_bsp_init_status;      /* 마지막 BSP_SD_Init() 반환값               */
    uint8_t  root_entry_sample_count;   /* root_entry_sample_name[] 실제 사용 개수   */
    uint8_t  root_entry_sample_type[3]; /* 0:none 1:file 2:dir                       */

    /* ------------------------------ */
    /*  시간축 / due 시각              */
    /* ------------------------------ */
    uint32_t debounce_due_ms;           /* debounce 만료 예정 시각                   */
    uint32_t last_detect_irq_ms;        /* 마지막 detect EXTI 진입 시각              */
    uint32_t last_detect_change_ms;     /* 마지막 stable insert/remove 확정 시각     */
    uint32_t last_mount_attempt_ms;     /* 마지막 mount 시도 시각                    */
    uint32_t last_mount_ms;             /* 마지막 mount 성공 시각                    */
    uint32_t last_unmount_ms;           /* 마지막 unmount 수행 시각                  */
    uint32_t mount_retry_due_ms;        /* 다음 mount retry 예정 시각                */

    /* ------------------------------ */
    /*  hotplug / mount 카운터         */
    /* ------------------------------ */
    uint32_t detect_irq_count;          /* detect pin EXTI 누적 횟수                 */
    uint32_t detect_insert_count;       /* stable insert 확정 횟수                   */
    uint32_t detect_remove_count;       /* stable remove 확정 횟수                   */

    uint32_t mount_attempt_count;       /* mount 시도 횟수                           */
    uint32_t mount_success_count;       /* mount 성공 횟수                           */
    uint32_t mount_fail_count;          /* mount 실패 횟수                           */
    uint32_t unmount_count;             /* unmount 수행 횟수                         */

    /* ------------------------------ */
    /*  최근 결과 코드                  */
    /* ------------------------------ */
    uint32_t last_mount_fresult;        /* 마지막 f_mount 결과 raw                   */
    uint32_t last_getfree_fresult;      /* 마지막 f_getfree 결과 raw                 */
    uint32_t last_root_scan_fresult;    /* 마지막 root scan 결과 raw                 */

    /* ------------------------------ */
    /*  HAL / 카드 정보                 */
    /* ------------------------------ */
    uint32_t hal_error_code;            /* hsd.ErrorCode snapshot                    */
    uint32_t hal_context;               /* hsd.Context snapshot                      */
    uint32_t rel_card_add;              /* RCA                                       */
    uint32_t block_nbr;                 /* physical block count                      */
    uint32_t block_size;                /* physical block size                       */
    uint32_t log_block_nbr;             /* logical block count                       */
    uint32_t log_block_size;            /* logical block size                        */

    /* ------------------------------ */
    /*  FAT 메타데이터                  */
    /* ------------------------------ */
    uint32_t sectors_per_cluster;       /* cluster 당 sector 수                      */
    uint32_t total_clusters;            /* 데이터 영역 총 cluster 수                 */
    uint32_t free_clusters;             /* 현재 free cluster 수                      */
    uint32_t sectors_per_fat;           /* FAT 하나의 sector 수                      */
    uint32_t volume_start_sector;       /* volume 시작 sector                        */
    uint32_t fat_start_sector;          /* FAT 시작 sector                           */
    uint32_t root_dir_base;             /* root dir base(raw: FAT32면 start cluster) */
    uint32_t data_start_sector;         /* data area 시작 sector                     */

    /* ------------------------------ */
    /*  root directory 요약            */
    /* ------------------------------ */
    uint32_t root_entry_count;          /* root 전체 엔트리 수                       */
    uint32_t root_file_count;           /* root 전체 파일 수                         */
    uint32_t root_dir_count;            /* root 전체 디렉터리 수                     */

    /* ------------------------------ */
    /*  byte 단위 용량 정보             */
    /* ------------------------------ */
    uint64_t capacity_bytes;            /* 카드 논리 용량(byte)                      */
    uint64_t total_bytes;               /* FAT volume 총 사용 가능 byte              */
    uint64_t free_bytes;                /* FAT volume 현재 free byte                 */

    /* ------------------------------ */
    /*  root 엔트리 샘플 이름           */
    /*                                                                            */
    /*  LFN이 꺼져 있으므로 현재 프로젝트에서는                                   */
    /*  short 8.3 이름만 저장한다.                                               */
    /* ------------------------------ */
    char     root_entry_sample_name[3][14];
} app_sd_state_t;

/* -------------------------------------------------------------------------- */
/*  CDS 밝기 센서 / ADC 공개 상태                                             */
/*                                                                            */
/*  주의                                                                      */
/*  - CDS(LDR)는 광량과 저항이 선형 관계가 아니므로, 여기서는 lux를 직접       */
/*    만들지 않고 raw ADC / 보정 counts / 0~100% 정규화 값을 공개한다.        */
/*  - STM32F407 ADC는 다른 일부 STM32 계열처럼 HAL self calibration API가     */
/*    없으므로, offset / gain / 0% / 100% 기준점을 소프트웨어로 적용한다.     */
/* -------------------------------------------------------------------------- */
typedef struct
{
    uint8_t  last_hal_status;           /* 마지막 HAL status raw                    */
    uint32_t read_fail_count;           /* ADC read 실패 누적 횟수                  */
    uint32_t hal_error_count;           /* HAL 에러 누적 횟수                       */
    uint32_t last_read_start_ms;        /* 마지막 읽기 시작 시각                    */
    uint32_t last_read_complete_ms;     /* 마지막 읽기 완료 시각                    */

    uint16_t average_count;             /* 한 번의 결과를 만들 때 평균하는 샘플 수   */
    uint16_t adc_timeout_ms;            /* PollForConversion timeout                */
    uint32_t period_ms;                 /* 주기 측정 간격                           */
    uint32_t sampling_time;             /* ADC sampling time raw enum 값            */

    int32_t  calibration_offset_counts; /* raw 에 먼저 더하는 offset                */
    uint32_t calibration_gain_num;      /* gain numerator                           */
    uint32_t calibration_gain_den;      /* gain denominator                         */
    uint16_t calibration_raw_0_percent; /* 0% 기준 raw count                        */
    uint16_t calibration_raw_100_percent; /* 100% 기준 raw count                    */
} app_brightness_debug_t;

typedef struct
{
    bool     initialized;               /* 드라이버 init 호출 여부                   */
    bool     valid;                     /* 적어도 한 번 정상 샘플이 들어왔는가       */
    uint32_t last_update_ms;            /* 마지막 정상 갱신 시각                    */
    uint32_t sample_count;              /* 누적 정상 샘플 횟수                      */

    uint16_t raw_last;                  /* 마지막 단일 raw 샘플                     */
    uint16_t raw_average;               /* 이번 burst 평균 raw                      */
    uint16_t raw_min;                   /* 이번 burst 최소 raw                      */
    uint16_t raw_max;                   /* 이번 burst 최대 raw                      */

    uint16_t calibrated_counts;         /* offset/gain 적용 후 count                */
    uint16_t normalized_permille;       /* 0~1000 정규화 결과                       */
    uint8_t  brightness_percent;        /* 0~100 정규화 결과                        */
    uint32_t voltage_mv;                /* calibrated count를 mV로 환산한 값        */

    app_brightness_debug_t debug;       /* 밝기 센서 디버그 저장소                  */
} app_brightness_state_t;


/* -------------------------------------------------------------------------- */
/*  Audio / DAC / DMA 공개 상태                                                */
/*                                                                            */
/*  이 구조체는                                                               */
/*    - DAC transport가 현재 살아 있는가                                       */
/*    - 실제로 tone / sequence / WAV content가 재생 중인가                    */
/*    - sample rate / DMA buffer 크기 / half/full callback count              */
/*    - voice별 phase / freq / envelope raw 상태                              */
/*  를 APP_STATE 안에 그대로 적재하기 위한 "저수준 창고" 다.                  */
/*                                                                            */
/*  즉, UI 페이지나 상위 앱 로직은                                             */
/*  driver 내부 static 상태를 직접 보지 않고                                   */
/*  반드시 APP_STATE.audio snapshot만 보도록 유지한다.                         */
/* -------------------------------------------------------------------------- */
#ifndef APP_AUDIO_MAX_VOICES
#define APP_AUDIO_MAX_VOICES 4u
#endif

#ifndef APP_AUDIO_NAME_MAX
#define APP_AUDIO_NAME_MAX 32u
#endif

typedef enum
{
    APP_AUDIO_MODE_IDLE          = 0u,
    APP_AUDIO_MODE_TONE          = 1u,
    APP_AUDIO_MODE_SEQUENCE_MIX  = 2u,
    APP_AUDIO_MODE_SEQUENCE_MONO = 3u,
    APP_AUDIO_MODE_WAV_FILE      = 4u
} app_audio_mode_t;

typedef enum
{
    APP_AUDIO_WAVEFORM_NONE       = 0u,
    APP_AUDIO_WAVEFORM_SINE       = 1u,
    APP_AUDIO_WAVEFORM_SQUARE     = 2u,
    APP_AUDIO_WAVEFORM_SAW        = 3u,

    APP_AUDIO_WAVEFORM_STRING     = 10u,
    APP_AUDIO_WAVEFORM_BRASS      = 11u,
    APP_AUDIO_WAVEFORM_PIANO      = 12u,
    APP_AUDIO_WAVEFORM_PERCUSSION = 13u
} app_audio_waveform_t;

typedef enum
{
    APP_AUDIO_ENV_OFF     = 0u,
    APP_AUDIO_ENV_ATTACK  = 1u,
    APP_AUDIO_ENV_DECAY   = 2u,
    APP_AUDIO_ENV_SUSTAIN = 3u,
    APP_AUDIO_ENV_RELEASE = 4u
} app_audio_env_phase_t;

typedef struct
{
    bool     active;                    /* 이 voice slot이 현재 재생 중인가         */
    uint8_t  waveform_id;               /* app_audio_waveform_t raw                 */
    uint8_t  timbre_id;                 /* Audio_Presets 계열 timbre ID raw         */
    uint8_t  track_index;               /* 4채널 sequence에서 몇 번째 track인가     */
    uint8_t  env_phase;                 /* app_audio_env_phase_t raw                */

    uint32_t note_hz_x100;              /* 현재 voice의 목표 주파수, Hz x100        */
    uint32_t phase_q32;                 /* 현재 oscillator phase raw                */
    uint32_t phase_inc_q32;             /* sample 1개당 phase 증가량                */

    uint32_t note_samples_total;        /* 현재 note 전체 길이(sample)              */
    uint32_t note_samples_elapsed;      /* 현재 note가 몇 sample 진행됐는가         */
    uint32_t gate_samples;              /* release로 들어가기 전 gate 길이          */

    uint16_t env_level_q15;             /* 현재 envelope level                      */
    uint16_t velocity_q15;              /* 현재 note velocity                       */
} app_audio_voice_state_t;

typedef struct
{
    bool     initialized;               /* Audio_Driver_Init 성공 여부              */
    bool     transport_running;         /* TIM6 + DAC DMA transport 동작 여부       */
    bool     content_active;            /* source 또는 FIFO tail 포함 busy 여부     */
    bool     wav_active;                /* WAV file streaming 경로 active 여부      */

    uint8_t  mode;                      /* app_audio_mode_t raw                     */
    uint8_t  active_voice_count;        /* 현재 active voice 수                     */
    uint8_t  last_hal_status_dac;       /* 마지막 DAC HAL status                    */
    uint8_t  last_hal_status_tim;       /* 마지막 TIM HAL status                    */

    uint8_t  output_resolution_bits;    /* 실제 DAC 아날로그 출력 분해능             */
    uint8_t  volume_percent;            /* 체감상 선형을 목표로 한 0~100 볼륨        */
    uint8_t  last_block_clipped;        /* 직전 render block에서 soft clip 여부     */
    uint8_t  wav_native_rate_active;    /* WAV가 DAC native rate로 재생 중인가      */

    uint32_t sample_rate_hz;            /* 현재 DAC sample rate                     */
    uint16_t dma_buffer_sample_count;   /* circular DMA 전체 sample 수              */
    uint16_t dma_half_buffer_sample_count; /* half buffer sample 수                  */
    uint16_t last_block_min_u12;        /* 직전 render block의 DAC 최소값           */
    uint16_t last_block_max_u12;        /* 직전 render block의 DAC 최대값           */

    /* ---------------------------------------------------------------------- */
    /*  software FIFO telemetry                                                */
    /*                                                                        */
    /*  이 값들은 "main producer" 와 "DMA ISR consumer" 사이의 완충지대를     */
    /*  얼마나 잘 유지하고 있는지 보여 주는 raw 진단값이다.                    */
    /* ---------------------------------------------------------------------- */
    uint32_t sw_fifo_capacity_samples;       /* FIFO 전체 용량(sample)            */
    uint32_t sw_fifo_level_samples;          /* 현재 FIFO 수위(sample)            */
    uint32_t sw_fifo_peak_level_samples;     /* 부팅 이후 관측된 최대 FIFO 수위   */
    uint32_t sw_fifo_low_watermark_samples;  /* producer refill low watermark     */
    uint32_t sw_fifo_high_watermark_samples; /* producer refill high watermark    */

    uint32_t last_update_ms;            /* 마지막 block render 완료 시각            */
    uint32_t playback_start_ms;         /* 현재/직전 재생 시작 시각                 */
    uint32_t playback_stop_ms;          /* 마지막 재생 정지 시각                    */

    uint32_t half_callback_count;       /* DMA half complete callback 누적 수       */
    uint32_t full_callback_count;       /* DMA full complete callback 누적 수       */
    uint32_t dma_underrun_count;        /* DAC DMA underrun 누적 수                 */
    uint32_t render_block_count;        /* main context에서 block render한 횟수     */
    uint32_t clip_block_count;          /* clip/soft clip이 발생한 block 수         */
    uint32_t transport_reconfig_count;  /* sample rate 변경으로 transport를 재시작한 횟수 */

    uint32_t producer_refill_block_count;   /* main producer가 FIFO에 적재한 block 수 */
    uint32_t dma_service_half_count;        /* ISR가 DMA half-buffer를 서비스한 횟수 */
    uint32_t fifo_starvation_count;         /* source는 살아 있는데 FIFO가 빈 횟수   */
    uint32_t silence_injected_sample_count; /* starvation 보정 silence sample 수     */

    uint32_t sequence_bpm;              /* 현재 sequence BPM                        */
    uint32_t wav_source_sample_rate_hz; /* WAV 원본 sample rate                     */
    uint32_t wav_source_data_bytes_remaining; /* 아직 남은 WAV data bytes           */
    uint16_t wav_source_channels;       /* WAV 원본 channel 수                      */
    uint16_t wav_source_bits_per_sample; /* WAV 원본 bit depth                      */

    char     current_name[APP_AUDIO_NAME_MAX];  /* 현재/직전 content 이름            */
    char     last_wav_path[APP_AUDIO_NAME_MAX]; /* 마지막 WAV 경로 축약 저장소      */

    app_audio_voice_state_t voices[APP_AUDIO_MAX_VOICES];
} app_audio_state_t;



/* -------------------------------------------------------------------------- */
/*  Bluetooth / 무선 시리얼 공개 상태                                          */
/*                                                                            */
/*  현재 프로젝트의 Bluetooth 모듈은                                           */
/*  "Classic Bluetooth SPP로 UART를 무선화하는 transparent serial adapter"     */
/*  처럼 다룬다.                                                               */
/*                                                                            */
/*  즉, 앱 입장에서는                                                          */
/*    - TX: 문자열/바이트를 UART처럼 보냄                                      */
/*    - RX: 상대 단말이 보낸 문자열/바이트를 line 단위로 관찰                  */
/*  하면 된다.                                                                 */
/*                                                                            */
/*  여기에는 사람이 디버그 화면에서 보고 싶어 할 값들을 모은다.               */
/*  예: 최근 RX/TX 줄, 바이트 카운터, echo/auto ping 상태, RX ring 수위 등     */
/* -------------------------------------------------------------------------- */
#ifndef APP_BLUETOOTH_LAST_TEXT_MAX
#define APP_BLUETOOTH_LAST_TEXT_MAX 96u
#endif

typedef struct
{
    bool     initialized;               /* Bluetooth driver init 호출 여부          */
    bool     uart_rx_running;           /* HAL_UART_Receive_IT 수신 경로 동작 여부  */
    bool     echo_enabled;              /* 수신 줄을 다시 돌려보내는 echo 기능      */
    bool     auto_ping_enabled;         /* 1초 주기 auto ping 기능                  */

    uint32_t last_update_ms;            /* RX 또는 TX로 마지막 상태 갱신 시각       */
    uint32_t last_rx_ms;                /* 마지막 raw byte 수신 시각                */
    uint32_t last_tx_ms;                /* 마지막 raw byte 송신 완료 시각           */
    uint32_t last_auto_ping_ms;         /* 마지막 auto ping 송신 시각               */

    uint32_t rx_bytes;                  /* 누적 수신 바이트 수                      */
    uint32_t tx_bytes;                  /* 누적 송신 바이트 수                      */
    uint32_t rx_line_count;             /* CR/LF 기준 확정된 RX 줄 수               */
    uint32_t tx_line_count;             /* 송신 helper로 보낸 논리적 줄 수          */
    uint32_t rx_overflow_count;         /* 소프트웨어 line/ring overflow 횟수       */
    uint32_t uart_error_count;          /* HAL UART error callback 누적 횟수        */
    uint32_t uart_rearm_fail_count;     /* Receive_IT 재arm 실패 횟수               */
    uint32_t uart_tx_fail_count;        /* HAL_UART_Transmit 실패 횟수              */

    uint16_t rx_ring_level;             /* 현재 RX ring에 쌓인 바이트 수            */
    uint16_t rx_ring_high_watermark;    /* 부팅 이후 RX ring 최대 적재치            */

    uint8_t  last_hal_status_rx;        /* 마지막 RX 관련 HAL status                */
    uint8_t  last_hal_status_tx;        /* 마지막 TX 관련 HAL status                */
    uint8_t  last_hal_error;            /* 마지막 HAL UART error raw                */
    uint8_t  reserved0;                 /* 정렬용 예약                               */

    char     last_rx_line[APP_BLUETOOTH_LAST_TEXT_MAX];
    char     last_tx_line[APP_BLUETOOTH_LAST_TEXT_MAX];
    char     rx_preview[APP_BLUETOOTH_LAST_TEXT_MAX];
} app_bluetooth_state_t;

/* -------------------------------------------------------------------------- */
/*  DEBUG UART 공개 상태                                                       */
/*                                                                            */
/*  이 구조체는 PA9/PA10(USART1) 같은 유선 UART 로그 포트를                    */
/*  얼마나 썼는지, 마지막으로 어떤 문자열을 내보냈는지 등을                     */
/*  APP_STATE에 기록하기 위한 저장소다.                                        */
/* -------------------------------------------------------------------------- */
#ifndef APP_DEBUG_UART_LAST_TEXT_MAX
#define APP_DEBUG_UART_LAST_TEXT_MAX 96u
#endif

typedef struct
{
    bool     initialized;               /* DEBUG_UART_Init 호출 여부                */
    uint8_t  last_hal_status;           /* 마지막 TX HAL status                     */
    uint16_t reserved0;                 /* 정렬용 예약                               */

    uint32_t last_tx_ms;                /* 마지막 송신 완료 시각                    */
    uint32_t tx_count;                  /* 송신 호출 성공 횟수                      */
    uint32_t tx_bytes;                  /* 누적 송신 바이트 수                      */
    uint32_t tx_fail_count;             /* HAL_UART_Transmit 실패 횟수              */

    char     last_text[APP_DEBUG_UART_LAST_TEXT_MAX];
} app_debug_uart_state_t;




/* -------------------------------------------------------------------------- */
/*  ALTITUDE / VARIO 공개 상태                                                 */
/*                                                                            */
/*  이 저장소는                                                              */
/*  - pressure altitude (STD 1013.25)                                         */
/*  - manual QNH altitude                                                     */
/*  - GPS hMSL                                                                */
/*  - GPS anchor를 사용한 fused absolute altitude(no-IMU / IMU)               */
/*  - relative/home altitude                                                  */
/*  - fast/slow variometer                                                    */
/*  - grade(경사도)                                                           */
/*  를 병렬로 보관한다.                                                       */
/*                                                                            */
/*  중요                                                                      */
/*  - manual QNH 고도와 GPS 기반 fused absolute altitude는                    */
/*    서로 의미가 다르므로 둘 다 따로 남긴다.                                 */
/*  - IMU-aided 결과와 no-IMU 결과도 동시에 유지해서                          */
/*    튜닝/검증/로그 비교가 가능하게 만든다.                                   */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*  ALTITUDE low-level unit bank helper types                                  */
/*                                                                            */
/*  canonical / derived ownership                                             */
/*  - *_cm / *_cms / *_hpa_x100 필드가 source-of-truth 이다.                  */
/*  - 아래 bank는 그 canonical metric 값을 건드리지 않고,                      */
/*    동일한 순간의 물리량을 여러 단위계로 병렬 보관하는 "파생 표현층" 이다.   */
/*                                                                            */
/*  설계 의도                                                                  */
/*  - 상위 UI / App layer는 meter 값을 다시 feet로 환산하지 않고              */
/*    이 bank에서 필요한 단위 슬롯만 선택한다.                                */
/*  - 특히 feet는 이미 1m로 양자화된 값을 다시 바꾼 것이 아니라                */
/*    canonical centimeter source에서 직접 계산된다.                          */
/*    따라서 feet 해상도가 meter 표시 해상도에 끌려 내려가지 않는다.          */
/* -------------------------------------------------------------------------- */
typedef struct
{
    /* ---------------------------------------------------------------------- */
    /*  선형 altitude quantity 표현쌍                                          */
    /*                                                                        */
    /*  meters_rounded                                                         */
    /*  - centimeter canonical source를 가장 가까운 1 m 로 반올림한 값        */
    /*                                                                        */
    /*  feet_rounded                                                           */
    /*  - 동일 source를 feet로 직접 변환한 뒤 가장 가까운 1 ft 로 반올림       */
    /*  - meters_rounded 를 다시 3.28084배 한 값이 아니다.                    */
    /* ---------------------------------------------------------------------- */
    int32_t meters_rounded;
    int32_t feet_rounded;
} app_altitude_linear_units_t;

typedef struct
{
    /* ---------------------------------------------------------------------- */
    /*  vertical-speed 표현쌍                                                  */
    /*                                                                        */
    /*  mps_x10_rounded                                                        */
    /*  - 0.1 m/s 고정소수점 표현                                              */
    /*                                                                        */
    /*  fpm_rounded                                                            */
    /*  - feet per minute 정수 반올림 표현                                     */
    /* ---------------------------------------------------------------------- */
    int32_t mps_x10_rounded;
    int32_t fpm_rounded;
} app_altitude_vspeed_units_t;

typedef struct
{
    /* ---------------------------------------------------------------------- */
    /*  pressure 표현쌍                                                        */
    /*                                                                        */
    /*  hpa_x100                                                               */
    /*  - canonical 그대로 유지되는 0.01 hPa 고정소수점                        */
    /*                                                                        */
    /*  inhg_x1000                                                             */
    /*  - inch of mercury, 0.001 inHg 고정소수점                               */
    /* ---------------------------------------------------------------------- */
    int32_t hpa_x100;
    int32_t inhg_x1000;
} app_altitude_pressure_units_t;

typedef struct
{
    /* ---------------------------------------------------------------------- */
    /*  pressure / qnh quantity bank                                           */
    /*                                                                        */
    /*  이름은 아래 canonical 필드명과 1:1 대응되도록 유지한다.                */
    /* ---------------------------------------------------------------------- */
    app_altitude_pressure_units_t pressure_raw;
    app_altitude_pressure_units_t pressure_prefilt;
    app_altitude_pressure_units_t pressure_filt;
    app_altitude_pressure_units_t pressure_residual;
    app_altitude_pressure_units_t qnh_manual;
    app_altitude_pressure_units_t qnh_equiv_gps;

    /* ---------------------------------------------------------------------- */
    /*  altitude quantity bank                                                 */
    /* ---------------------------------------------------------------------- */
    app_altitude_linear_units_t   alt_pressure_std;
    app_altitude_linear_units_t   alt_qnh_manual;
    app_altitude_linear_units_t   alt_gps_hmsl;
    app_altitude_linear_units_t   alt_fused_noimu;
    app_altitude_linear_units_t   alt_fused_imu;
    app_altitude_linear_units_t   alt_display;
    app_altitude_linear_units_t   alt_rel_home_noimu;
    app_altitude_linear_units_t   alt_rel_home_imu;
    app_altitude_linear_units_t   home_alt_noimu;
    app_altitude_linear_units_t   home_alt_imu;
    app_altitude_linear_units_t   baro_bias_noimu;
    app_altitude_linear_units_t   baro_bias_imu;

    /* ---------------------------------------------------------------------- */
    /*  vertical-speed quantity bank                                           */
    /* ---------------------------------------------------------------------- */
    app_altitude_vspeed_units_t   debug_audio_vario;
    app_altitude_vspeed_units_t   vario_fast_noimu;
    app_altitude_vspeed_units_t   vario_slow_noimu;
    app_altitude_vspeed_units_t   vario_fast_imu;
    app_altitude_vspeed_units_t   vario_slow_imu;
    app_altitude_vspeed_units_t   baro_vario_raw;
    app_altitude_vspeed_units_t   baro_vario_filt;
} app_altitude_unit_bank_t;

typedef struct
{
    /* ---------------------------------------------------------------------- */
    /*  공개 상태 플래그 / 품질 정보                                           */
    /* ---------------------------------------------------------------------- */
    bool     initialized;                   /* 서비스 init 완료 여부                 */
    bool     baro_valid;                    /* baro 경로가 현재 살아 있는가          */
    bool     gps_valid;                     /* GPS height gate 통과 여부             */
    bool     home_valid;                    /* home altitude가 캡처되었는가          */
    bool     imu_vector_valid;              /* gravity estimator가 유효한가          */
    uint8_t  debug_audio_active;            /* ALTITUDE debug audio 활성 여부        */
    uint16_t gps_quality_permille;          /* GPS quality 0..1000                   */

    /* ---------------------------------------------------------------------- */
    /*  최근 update timestamp                                                  */
    /* ---------------------------------------------------------------------- */
    uint32_t last_update_ms;                /* 마지막 task 실행 시각                 */
    uint32_t last_baro_update_ms;           /* 마지막 신규 baro sample 반영 시각     */
    uint32_t last_gps_update_ms;            /* 마지막 신규 GPS sample 반영 시각      */

    /* ---------------------------------------------------------------------- */
    /*  Pressure / QNH intermediate 공개값                                     */
    /* ---------------------------------------------------------------------- */
    int32_t  pressure_raw_hpa_x100;         /* raw pressure, 0.01 hPa                */
    int32_t  pressure_prefilt_hpa_x100;     /* median-3 prefilter pressure           */
    int32_t  pressure_filt_hpa_x100;        /* LPF pressure                          */
    int32_t  pressure_residual_hpa_x100;    /* prefilt - lpf residual                */
    int32_t  qnh_manual_hpa_x100;           /* manual QNH                            */
    int32_t  qnh_equiv_gps_hpa_x100;        /* GPS equivalent QNH                    */

    /* ---------------------------------------------------------------------- */
    /*  병렬 고도값                                                            */
    /* ---------------------------------------------------------------------- */
    int32_t  alt_pressure_std_cm;           /* STD 1013.25 pressure altitude         */
    int32_t  alt_qnh_manual_cm;             /* manual QNH altitude                   */
    int32_t  alt_gps_hmsl_cm;               /* GPS hMSL                              */
    int32_t  alt_fused_noimu_cm;            /* 3-state KF fused altitude             */
    int32_t  alt_fused_imu_cm;              /* 4-state IMU-aided fused altitude      */
    int32_t  alt_display_cm;                /* 최종 UI/log 주 표시용 altitude        */

    /* ---------------------------------------------------------------------- */
    /*  home / relative altitude                                               */
    /* ---------------------------------------------------------------------- */
    int32_t  alt_rel_home_noimu_cm;         /* no-IMU 상대고도                       */
    int32_t  alt_rel_home_imu_cm;           /* IMU 상대고도                          */
    int32_t  home_alt_noimu_cm;             /* no-IMU home absolute altitude         */
    int32_t  home_alt_imu_cm;               /* IMU home absolute altitude            */

    /* ---------------------------------------------------------------------- */
    /*  filter bias / noise / display mode                                     */
    /* ---------------------------------------------------------------------- */
    int32_t  baro_bias_noimu_cm;            /* no-IMU filter의 baro bias             */
    int32_t  baro_bias_imu_cm;              /* IMU filter의 baro bias                */
    uint16_t baro_noise_used_cm;            /* 이번 step에서 사용된 altitude R       */
    uint8_t  display_rest_active;           /* rest display stabilizer 활성          */
    uint8_t  zupt_active;                   /* zero-velocity pseudo update 활성      */
    uint8_t  debug_audio_source;            /* 현재 audio source 0/1                 */
    uint8_t  reserved_audio0;               /* alignment / future use                */
    int32_t  debug_audio_vario_cms;         /* 실제 tone source vario                */

    /* ---------------------------------------------------------------------- */
    /*  Vario / baro-velocity / grade                                          */
    /* ---------------------------------------------------------------------- */
    int32_t  vario_fast_noimu_cms;          /* no-IMU fast vario                     */
    int32_t  vario_slow_noimu_cms;          /* no-IMU slow vario                     */
    int32_t  vario_fast_imu_cms;            /* IMU fast vario                        */
    int32_t  vario_slow_imu_cms;            /* IMU slow vario                        */
    int32_t  baro_vario_raw_cms;            /* altitude derivative raw               */
    int32_t  baro_vario_filt_cms;           /* altitude derivative LPF               */
    int32_t  grade_noimu_x10;               /* no-IMU grade %, x0.1                  */
    int32_t  grade_imu_x10;                 /* IMU grade %, x0.1                     */

    /* ---------------------------------------------------------------------- */
    /*  IMU vertical estimate diagnostics                                      */
    /* ---------------------------------------------------------------------- */
    int32_t  imu_vertical_accel_mg;         /* gravity 제거 후 수직 specific-force   */
    int32_t  imu_vertical_accel_cms2;       /* 위 값을 cm/s^2 로 변환한 값           */
    int32_t  imu_gravity_norm_mg;           /* gravity vector norm diagnostic        */
    int32_t  imu_accel_norm_mg;             /* raw accel norm diagnostic             */
    uint16_t imu_attitude_trust_permille;   /* attitude trust 0..1000                */
    uint16_t imu_predict_weight_permille;   /* KF4 predict weight 0..1000            */

    /* ---------------------------------------------------------------------- */
    /*  최근 사용된 GPS 품질 수치                                              */
    /* ---------------------------------------------------------------------- */
    uint32_t gps_vacc_mm;                   /* GPS vertical accuracy                 */
    uint16_t gps_pdop_x100;                 /* GPS PDOP x100                         */
    uint8_t  gps_numsv_used;                /* GPS numSV_used                        */
    uint8_t  gps_fix_type;                  /* GPS fixType                           */

    /* ---------------------------------------------------------------------- */
    /*  저수준 단위 bank                                                       */
    /*                                                                        */
    /*  중요한 규칙                                                            */
    /*  - 위 canonical metric 필드가 먼저 갱신되고                             */
    /*  - 아래 units bank는 그 canonical 값을 기반으로 같은 task 안에서        */
    /*    즉시 파생 계산된다.                                                  */
    /*                                                                        */
    /*  따라서 상위 계층은                                                     */
    /*  - "meter 값을 받아서 다시 feet로 환산" 하지 말고                      */
    /*  - 필요한 슬롯만 선택해 읽어야 한다.                                    */
    /* ---------------------------------------------------------------------- */
    app_altitude_unit_bank_t units;
} app_altitude_state_t;


/* -------------------------------------------------------------------------- */
/*  BIKE DYNAMICS 공개 상태                                                     */
/*                                                                            */
/*  sign 규칙                                                                   */
/*  - banking angle : + = left lean,  - = right lean                           */
/*  - grade         : + = nose up,    - = nose down                            */
/*  - lat accel     : + = left,       - = right                                */
/*  - lon accel     : + = accel,      - = braking                              */
/*                                                                            */
/*  input / output 분리                                                         */
/*  - 아래 구조체의 대부분은 BIKE_DYNAMICS.c가 갱신하는 출력 필드다.            */
/*  - 단, obd_input_* 필드는 추후 OBD service가 app_state를 통해 써 넣는        */
/*    입력 필드다. BIKE_DYNAMICS는 이 값을 읽기만 하고, 리셋 외에는             */
/*    덮어쓰지 않는다.                                                          */
/* -------------------------------------------------------------------------- */
typedef struct
{
    /* ---------------------------------------------------------------------- */
    /*  공개 플래그 / mode                                                     */
    /* ---------------------------------------------------------------------- */
    bool     initialized;               /* 서비스 init 완료 여부                  */
    bool     zero_valid;                /* reset 기준 basis가 유효한가            */
    bool     imu_valid;                 /* gravity observer가 유효한가            */
    bool     gnss_aid_valid;            /* GNSS speed 또는 heading aid 유효한가   */
    bool     gnss_heading_valid;        /* GNSS heading aid가 현재 유효한가       */
    bool     obd_speed_valid;           /* future OBD speed가 현재 유효한가       */
    uint8_t  speed_source;              /* app_bike_speed_source_t raw            */
    uint8_t  estimator_mode;            /* app_bike_estimator_mode_t raw          */
    uint16_t confidence_permille;       /* 0..1000                                */

    /* ---------------------------------------------------------------------- */
    /*  최근 update timestamp / 사용자 명령 누적 횟수                           */
    /* ---------------------------------------------------------------------- */
    uint32_t last_update_ms;
    uint32_t last_imu_update_ms;
    uint32_t last_zero_capture_ms;
    uint32_t last_gnss_aid_ms;
    uint32_t zero_request_count;        /* ResetBankingAngleSensor 호출 누적       */
    uint32_t hard_rezero_count;         /* hard rezero 요청 누적                  */

    /* ---------------------------------------------------------------------- */
    /*  최종 표시 후보                                                          */
    /* ---------------------------------------------------------------------- */
    int16_t  banking_angle_deg_x10;     /* 0.1 deg                                */
    int16_t  banking_angle_display_deg; /* 1 deg 표시값                           */
    int16_t  grade_deg_x10;             /* 0.1 deg                                */
    int16_t  grade_display_deg;         /* 1 deg 표시값                           */
    int16_t  bank_rate_dps_x10;         /* 0.1 dps                                */
    int16_t  grade_rate_dps_x10;        /* 0.1 dps                                */

    /* ---------------------------------------------------------------------- */
    /*  canonical estimator output                                             */
    /*                                                                        */
    /*  bank_raw_deg_x10 / grade_raw_deg_x10                                  */
    /*  - display smoothing(lean/grade tau) 적용 전의 자세 추정값             */
    /*  - logger / peak detector / offline analyzer 는 이 값을 기준으로        */
    /*    읽어야 한다.                                                         */
    /*                                                                        */
    /*  lat_accel_est_mg / lon_accel_est_mg                                   */
    /*  - bias 제거 + deadband/clip 적용 후, display tau 적용 전 fused accel   */
    /*  - 즉, rider-facing UI 값(lat_accel_mg/lon_accel_mg)보다               */
    /*    응답이 빠른 canonical telemetry 층이다.                              */
    /* ---------------------------------------------------------------------- */
    int16_t  bank_raw_deg_x10;          /* 0.1 deg, pre-display smoothing         */
    int16_t  grade_raw_deg_x10;         /* 0.1 deg, pre-display smoothing         */
    int32_t  lat_accel_est_mg;          /* mg, pre-display smoothing              */
    int32_t  lon_accel_est_mg;          /* mg, pre-display smoothing              */

    int32_t  lat_accel_mg;              /* 최종 rider-facing lateral accel, mg    */
    int32_t  lon_accel_mg;              /* 최종 rider-facing accel/decel, mg      */
    int32_t  lat_accel_cms2;            /* cm/s^2                                 */
    int32_t  lon_accel_cms2;            /* cm/s^2                                 */

    /* ---------------------------------------------------------------------- */
    /*  비교/튜닝용 intermediate                                                */
    /* ---------------------------------------------------------------------- */
    int32_t  lat_accel_imu_mg;          /* IMU-only level lateral                 */
    int32_t  lon_accel_imu_mg;          /* IMU-only level longitudinal            */
    int32_t  lat_accel_ref_mg;          /* GNSS heading 기반 lateral reference    */
    int32_t  lon_accel_ref_mg;          /* GNSS/OBD speed derivative reference    */
    int32_t  lat_bias_mg;               /* external ref로 적응된 lateral bias     */
    int32_t  lon_bias_mg;               /* external ref로 적응된 longitudinal bias */

    /* ---------------------------------------------------------------------- */
    /*  IMU / attitude diagnostic                                               */
    /* ---------------------------------------------------------------------- */
    int32_t  imu_accel_norm_mg;         /* raw accel norm                         */
    int32_t  imu_jerk_mg_per_s;         /* rough-road / 충격 진단용 jerk          */
    uint16_t imu_attitude_trust_permille; /* gravity observer trust 0..1000      */
    int32_t  up_bx_milli;               /* current up dot bike_fwd, x1000         */
    int32_t  up_by_milli;               /* current up dot bike_left, x1000        */
    int32_t  up_bz_milli;               /* current up dot bike_up, x1000          */

    /* ---------------------------------------------------------------------- */
    /*  speed / GNSS quality                                                    */
    /* ---------------------------------------------------------------------- */
    int32_t  speed_mmps;                /* 현재 선택된 speed source 값            */
    uint16_t speed_kmh_x10;             /* 0.1 km/h                               */
    uint16_t gnss_speed_acc_kmh_x10;    /* GNSS speed accuracy diagnostic         */
    uint16_t gnss_head_acc_deg_x10;     /* GNSS heading accuracy diagnostic       */
    int16_t  mount_yaw_trim_deg_x10;    /* 현재 적용 중 yaw trim                  */

    uint8_t  gnss_fix_ok;
        uint8_t  gnss_numsv_used;
        uint16_t gnss_pdop_x100;


        /* ---------------------------------------------------------------------- */
        /*  heading diagnostic                                                     */
        /*                                                                        */
        /*  heading_valid                                                          */
        /*  - 현재 공개 중인 heading 값이 유효한가                                  */
        /*                                                                        */
        /*  mag_heading_valid                                                      */
        /*  - tilt-compensated magnetic heading 값이 유효한가                       */
        /*                                                                        */
        /*  heading_source                                                         */
        /*  - app_bike_heading_source_t raw                                         */
        /*                                                                        */
        /*  heading_deg_x10                                                        */
        /*  - 현재 공개 중인 heading 값, 0.1 deg                                    */
        /*                                                                        */
        /*  mag_heading_deg_x10                                                    */
        /*  - tilt-compensated magnetic heading raw, 0.1 deg                        */
        /*                                                                        */
        /*  주의                                                                   */
        /*  - 이 heading 출력은 lean / grade / lateral G 계산식에 피드백하지 않는다.*/
        /*  - 즉, 6축 Mahony 자세 추정은 그대로 유지하고, heading은 보조 진단용이다.*/
        /* ---------------------------------------------------------------------- */
        bool     heading_valid;
        bool     mag_heading_valid;
        uint8_t  heading_source;
        uint8_t  reserved_heading0;
        int16_t  heading_deg_x10;
        int16_t  mag_heading_deg_x10;



        /* ---------------------------------------------------------------------- */
        /*  Gyro bias calibration 공개 상태                                         */
        /*                                                                        */
        /*  gyro_bias_cal_active                                                   */
        /*  - 현재 gyro bias calibration이 진행 중인가                             */
        /*                                                                        */
        /*  gyro_bias_valid                                                        */
        /*  - bias 값이 한 번이라도 성공적으로 측정되었는가                         */
        /*                                                                        */
        /*  gyro_bias_cal_last_success                                             */
        /*  - 가장 최근 calibration 요청이 성공했는가                              */
        /*                                                                        */
        /*  gyro_bias_cal_progress_permille                                        */
        /*  - calibration good-sample 누적 진행률, 0..1000                         */
        /*                                                                        */
        /*  last_gyro_bias_cal_ms / gyro_bias_cal_count                            */
        /*  - 마지막 종료 시각 / 성공 누적 횟수                                    */
        /*                                                                        */
        /*  gyro_bias_*_dps_x100                                                   */
        /*  - 사용자에게 보여 주기 위한 bias 값, 0.01 dps 고정소수점               */
        /*                                                                        */
        /*  yaw_rate_dps_x10                                                       */
        /*  - 현재 world-up 축 기준 yaw rate, 0.1 dps                              */
        /* ---------------------------------------------------------------------- */
        bool     gyro_bias_cal_active;
        bool     gyro_bias_valid;
        bool     gyro_bias_cal_last_success;
        uint8_t  reserved_gyro_bias0;
        uint16_t gyro_bias_cal_progress_permille;
        uint32_t last_gyro_bias_cal_ms;
        uint32_t gyro_bias_cal_count;
        int16_t  gyro_bias_x_dps_x100;
        int16_t  gyro_bias_y_dps_x100;
        int16_t  gyro_bias_z_dps_x100;
        int16_t  yaw_rate_dps_x10;

        /* ---------------------------------------------------------------------- */
        /*  future OBD service input                                                */
        /*                                                                        */
        /*  추후 OBD service가 아래 세 필드만 갱신하면                              */
        /*  BIKE_DYNAMICS가 자동으로 OBD speed source를 사용할 수 있다.             */
        /* ---------------------------------------------------------------------- */
        bool     obd_input_speed_valid;
        uint32_t obd_input_speed_mmps;
        uint32_t obd_input_last_update_ms;
} app_bike_state_t;






/* -------------------------------------------------------------------------- */
/*  app_state_t                                                                 */
/* -------------------------------------------------------------------------- */

typedef struct
{
    app_settings_t         settings;    /* 기기 전체 설정 스냅샷                     */
    app_gps_state_t        gps;         /* GPS 드라이버의 원본 상태 저장소           */
    app_gy86_state_t       gy86;        /* GY-86 / IMU 전체 상태 저장소              */
    app_ds18b20_state_t    ds18b20;     /* DS18B20 전체 상태 저장소                  */
    app_brightness_state_t brightness;  /* CDS 밝기 센서 상태 저장소                 */
    app_audio_state_t      audio;       /* DAC / DMA / 오디오 엔진 상태 저장소       */
    app_bluetooth_state_t  bluetooth;   /* Bluetooth bring-up / 무선 시리얼 저장소   */
    app_debug_uart_state_t debug_uart;  /* 유선 DEBUG UART 상태 저장소               */
    app_sd_state_t         sd;          /* SD / FATFS / hotplug 공개 상태 저장소     */
    app_clock_state_t      clock;       /* RTC / timezone / GPS sync 상태 저장소     */
    app_altitude_state_t   altitude;    /* 고도 / 바리오 / 경사도 공개 상태 저장소   */
    app_bike_state_t       bike;        /* 모터사이클 frame dynamics 공개 상태 저장소*/

    /* 다른 센서/서브시스템도 계속 여기에 추가하면 된다. */
    /* 예: battery, storage, ui, logger ... */
} app_state_t;


/* -------------------------------------------------------------------------- */
/*  UI 전용 경량 GPS 스냅샷                                                    */
/*                                                                            */
/*  왜 따로 두는가?                                                             */
/*                                                                            */
/*  기존 app_gps_state_t 는 "드라이버 전체 상태 저장소"라서                    */
/*  raw UBX payload / last frame / cfg payload / nav_sat_sv[] / sats[] 등     */
/*  UI가 필요 없는 큰 데이터까지 전부 포함한다.                                */
/*                                                                            */
/*  메인 루프에서 매 프레임마다 저 큰 구조체를 통째로 memcpy 하면               */
/*  IRQ off 시간이 길어지고, 그 사이 UART RX 오버런 위험이 커진다.             */
/*                                                                            */
/*  그래서 UI가 실제로 그리는 데 필요한 필드만 모은                           */
/*  app_gps_ui_snapshot_t 를 따로 만든다.                                      */
/* -------------------------------------------------------------------------- */
typedef struct
{
    /* ------------------------------ */
    /*  위치/속도/정확도 기본 정보     */
    /* ------------------------------ */
    gps_fix_basic_t fix;                    /* 디버그 화면의 FIX/LAT/LON/SPD/ACC 등에 사용 */

    /* ------------------------------ */
    /*  런타임 설정 질의 결과          */
    /* ------------------------------ */
    app_gps_runtime_config_t runtime_cfg;  /* RUN/GN/B...P...S... 줄 표시용 */

    /* ------------------------------ */
    /*  UART / parser 관측치           */
    /* ------------------------------ */
    bool     uart_rx_running;              /* direct IRQ RX 경로가 현재 동작 중인지 여부 */

    uint32_t rx_bytes;                     /* 총 수신 바이트 수 */
    uint32_t frames_ok;                    /* checksum OK로 통과한 UBX frame 수 */
    uint32_t frames_bad_checksum;          /* checksum mismatch로 버려진 frame 수 */
    uint32_t uart_ring_overflow_count;     /* 소프트웨어 RX ring overflow 누적 수 */

    uint32_t uart_error_count;             /* UART 하드웨어 에러 총합 */
    uint32_t uart_error_ore_count;         /* Overrun Error 누적 수 */
    uint32_t uart_error_fe_count;          /* Framing Error 누적 수 */
    uint32_t uart_error_ne_count;          /* Noise Error 누적 수 */
    uint32_t uart_error_pe_count;          /* Parity Error 누적 수 */

    uint16_t rx_ring_level;                /* 지금 ring 안에 쌓여 있는 바이트 수 */
    uint16_t rx_ring_high_watermark;       /* 부팅 이후 ring 최대 적재치 */

    uint32_t last_rx_ms;                   /* 마지막 raw byte 수신 tick */

    /* ------------------------------ */
    /*  스카이 플롯용 데이터           */
    /* ------------------------------ */
    uint8_t       nav_sat_count;           /* sats[] 중 유효 항목 수 */
    app_gps_sat_t sats[APP_GPS_MAX_SATS];  /* 스카이 플롯에 바로 쓰는 위성 배열 */
} app_gps_ui_snapshot_t;


/* -------------------------------------------------------------------------- */
/*  Global state                                                               */
/* -------------------------------------------------------------------------- */

extern volatile app_state_t g_app_state;

void APP_STATE_Init(void);
void APP_STATE_ResetGps(void);
void APP_STATE_CopySnapshot(app_state_t *dst);

/* 레거시 전체 GPS snapshot API.
 * 다른 코드가 아직 쓰고 있을 수 있으므로 남겨둔다. */
void APP_STATE_CopyGpsSnapshot(app_gps_state_t *dst);

/* 새 UI 전용 경량 snapshot API.
 * 메인 GPS 화면/디버그 화면은 이 함수만 사용하도록 유지한다. */
void APP_STATE_CopyGpsUiSnapshot(app_gps_ui_snapshot_t *dst);

/* 새 센서 전용 snapshot API.
 * 센서 디버그 페이지는 이 함수 하나만 호출하면 된다. */
void APP_STATE_CopyGy86Snapshot(app_gy86_state_t *dst);
void APP_STATE_CopyDs18b20Snapshot(app_ds18b20_state_t *dst);
void APP_STATE_CopySensorDebugSnapshot(app_sensor_debug_snapshot_t *dst);
void APP_STATE_CopyBrightnessSnapshot(app_brightness_state_t *dst);
void APP_STATE_CopyAudioSnapshot(app_audio_state_t *dst);
void APP_STATE_CopyBluetoothSnapshot(app_bluetooth_state_t *dst);
void APP_STATE_CopyDebugUartSnapshot(app_debug_uart_state_t *dst);
void APP_STATE_CopySdSnapshot(app_sd_state_t *dst);
void APP_STATE_CopyClockSnapshot(app_clock_state_t *dst);
void APP_STATE_CopyAltitudeSnapshot(app_altitude_state_t *dst);
void APP_STATE_CopyBikeSnapshot(app_bike_state_t *dst);

void APP_STATE_CopySettingsSnapshot(app_settings_t *dst);
void APP_STATE_StoreSettingsSnapshot(const app_settings_t *src);


#ifdef __cplusplus
}
#endif

#endif /* APP_STATE_H */
