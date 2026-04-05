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
/*  ???꾨줈?앺듃?먯꽌 APP_STATE??"湲곌린 ?꾩껜???먮즺李쎄퀬" ??븷???쒕떎.             */
/*                                                                            */
/*  - GPS ?쒕씪?대쾭(Ublox_GPS.c)??UART濡??ㅼ뼱??UBX 硫붿떆吏瑜??댁꽍?댁꽌          */
/*    g_app_state.gps ?덉뿉 怨꾩냽 諛???ｋ뒗??                                  */
/*  - UI(main.c???붾쾭洹??붾㈃/?ㅼ뭅?댄뵆濡???APP_STATE???ㅻ깄?룸쭔 ?쎈뒗??       */
/*  - ?ъ슜???ㅼ젙(?섍꼍?ㅼ젙)??APP_STATE ?덉뿉 ?붾떎.                             */
/*                                                                            */
/*  利? "諛쏅뒗 ?? 怨?"蹂댁뿬二쇰뒗 ?? ??遺꾨━?댁꽌                                 */
/*  肄붾뱶媛 ??瑗ъ씠怨??좎?蹂댁닔媛 ?ъ썙吏?꾨줉 留뚮뱺 援ъ“??                         */
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
/*  ?ъ슜???섍꼍?ㅼ젙                                                             */
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
    int8_t  timezone_quarters;          /* UTC offset / 15 min ?⑥쐞. ?쒓뎅 湲곕낯媛?+36 */
    uint8_t gps_auto_sync_enabled;      /* GPS ?먮룞 ?숆린???ъ슜 ?щ?                 */
    uint8_t gps_sync_interval_minutes;  /* GPS time-only 二쇨린. ?꾩옱 湲곕낯 10遺?       */
    uint8_t reserved0;                  /* ?뺣젹/?ν썑 ?뺤옣??                          */
} app_clock_settings_t;

typedef enum
{
    APP_BACKLIGHT_AUTO_MODE_CONTINUOUS = 0u,
    APP_BACKLIGHT_AUTO_MODE_DIMMER     = 1u
} app_backlight_auto_mode_t;

typedef struct
{
    /* ---------------------------------------------------------------------- */
    /*  ?먮룞 諛앷린 ?숈옉 紐⑤뱶                                                    */
    /*                                                                        */
    /*  CONTINUOUS                                                             */
    /*  - 二쇰?愿?媛??꾩껜瑜??곗냽 怨≪꽑?쇰줈 ?댁꽍?댁꽌 ?붾㈃ 諛앷린瑜??곗냽?곸쑝濡?異붿쥌  */
    /*                                                                        */
    /*  DIMMER                                                                 */
    /*  - DAY / NIGHT / SUPER NIGHT 3媛?議댁쑝濡쒕쭔 target??怨좊Ⅸ??             */
    /*  - ?? ?ㅼ젣 target?쇰줈 ?섏뼱媛??異쒕젰 ?꾪솚 ?먯껜??遺?쒕읇寃??댁뼱吏꾨떎.    */
    /* ---------------------------------------------------------------------- */
    uint8_t auto_mode;                      /* app_backlight_auto_mode_t raw         */
    int8_t  continuous_bias_steps;         /* AUTO-CONT ?꾩슜 -2..+2 bias ?④퀎        */
    uint8_t transition_smoothness;         /* 1..5, ?믪쓣?섎줉 ??泥쒖쿇???곕씪媛?       */
    uint8_t reserved0;                     /* ?뺣젹/?ν썑 ?뺤옣??                      */

    /* ---------------------------------------------------------------------- */
    /*  AUTO-DIMMER 議?湲곗?/諛앷린                                               */
    /*                                                                        */
    /*  sensor 湲곗?? Brightness_Sensor??normalized percent(0..100) 湲곗??대떎. */
    /*  - night_threshold_percent        : DAY ??NIGHT 寃쎄퀎                    */
    /*  - super_night_threshold_percent  : NIGHT ??SUPER NIGHT 寃쎄퀎            */
    /*  - *_brightness_percent           : ?대떦 議댁쓽 紐⑺몴 ?붾㈃ 諛앷린            */
    /*                                                                        */
    /*  DAY 諛앷린???붽뎄?ы빆?濡?100% 怨좎젙?대?濡?蹂꾨룄 ??ν븯吏 ?딅뒗??          */
    /* ---------------------------------------------------------------------- */
    uint8_t night_threshold_percent;       /* 0..100                               */
    uint8_t super_night_threshold_percent; /* 0..100                               */
    uint8_t night_brightness_percent;      /* 0..100                               */
    uint8_t super_night_brightness_percent;/* 0..100                               */
} app_backlight_settings_t;

typedef struct
{
    /* ---------------------------------------------------------------------- */
    /*  UC1608 ?⑤꼸 ?ㅼ젙                                                       */
    /*                                                                        */
    /*  ??援ъ“泥대뒗 "?ㅼ젣 ?⑤꼸 ?덉??ㅽ꽣??諛붾줈 諛섏쁺 媛?ν븳 媛????섎룄?곸쑝濡?    */
    /*  raw??媛源앷쾶 蹂닿??쒕떎.                                                 */
    /*                                                                        */
    /*  contrast                 -> CMD 0x81 + arg                             */
    /*  temperature_compensation -> CMD 0x24..0x27 (240x128?먯꽌??mux128 怨좎젙) */
    /*  bias_ratio               -> CMD 0xE8..0xEB                             */
    /*  ram_access_mode          -> CMD 0x88..0x8B                             */
    /*  start_line_raw           -> CMD 0x40..0x7F ???섏쐞 raw 媛?0..63)       */
    /*  fixed_line_raw           -> CMD 0x90..0x9F ???섏쐞 raw 媛?0..15)       */
    /*  power_control_raw        -> CMD 0x28..0x2F ???섏쐞 raw 媛?0..7)        */
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
    /*  ?섎룞 QNH ?ㅼ젙                                                          */
    /*                                                                        */
    /*  ??났???섎?瑜?蹂댁〈?섍린 ?꾪빐 "?ъ슜?먭? 吏곸젒 ?낅젰??QNH" ??            */
    /*  GPS 湲곕컲 ?먮룞 蹂댁젙媛믨낵 遺꾨━??蹂꾨룄 蹂?섎줈 ?좎??쒕떎.                    */
    /*  ?⑥쐞??0.01 hPa 怨좎젙?뚯닔?먯씠??                                        */
    /* ---------------------------------------------------------------------- */
    int32_t manual_qnh_hpa_x100;

    /* ---------------------------------------------------------------------- */
    /*  ?뺤븬 ?ы듃 / ?ㅼ튂 ?ㅼ감 蹂댁젙                                             */
    /*                                                                        */
    /*  pressure_correction_hpa_x100                                           */
    /*  - ?쇱꽌 raw static pressure??additive correction?쇰줈 癒쇱? ?곸슜?쒕떎.    */
    /*  - QNH瑜?諛붽씀????ぉ???꾨땲?? ?ㅼ튂 ?ㅼ감 / static bias瑜?蹂댁젙?쒕떎.      */
    /*  - ??媛믪쓣 嫄곗튇 pressure媛                                              */
    /*    manual-QNH altitude / FL / Smart Fuse / display path ?꾩껜??        */
    /*    怨듯넻 ?낅젰???쒕떎.                                                    */
    /* ---------------------------------------------------------------------- */
    int32_t pressure_correction_hpa_x100;

    /* ---------------------------------------------------------------------- */
    /*  GPS / IMU / HOME ?뺤콉 ?좉?                                             */
    /*                                                                        */
    /*  gps_auto_equiv_qnh_enabled                                             */
    /*  - GPS hMSL怨??꾩옱 pressure瑜??댁슜??                                   */
    /*    "GPS? ?쇱튂?섎뒗 ?깃? sea-level pressure" 瑜?怨꾩궛?좎? ?щ?          */
    /*                                                                        */
    /*  gps_bias_correction_enabled                                            */
    /*  - GPS瑜??κ린 ?쒕━?꾪듃 ?녿뒗 absolute anchor濡??ъ슜??                   */
    /*    baro bias瑜?泥쒖쿇???≪쓣吏 ?щ?                                       */
    /*                                                                        */
    /*  imu_aid_enabled                                                        */
    /*  - IMU-aided 4-state filter瑜?display/audio 湲곕낯 ?꾨낫濡??섏? ?щ?       */
    /*  - no-IMU / IMU 寃곌낵????긽 蹂묐젹 怨꾩궛?댁꽌 ????APP_STATE???④릿??     */
    /*                                                                        */
    /*  auto_home_capture_enabled                                              */
    /*  - 泥?valid fused altitude媛 ?≫엳硫?home altitude瑜??먮룞 罹≪쿂?쒕떎.     */
    /* ---------------------------------------------------------------------- */
    uint8_t gps_auto_equiv_qnh_enabled;
    uint8_t gps_bias_correction_enabled;
    uint8_t imu_aid_enabled;
    uint8_t auto_home_capture_enabled;

    /* ---------------------------------------------------------------------- */
       /*  IMU ?섏쭅異?遺??+ GY-86 poll gate                                      */
       /*                                                                        */
       /*  imu_vertical_sign                                                     */
       /*  - gravity projection???ъ슜?대룄 ?ㅼ젣 ?μ갑 諛⑺뼢???ㅼ쭛? ?덉쑝硫?        */
       /*    ?섏쭅 specific-force 遺?멸? 諛섎?濡??섏삱 ???덉쑝誘濡?                  */
       /*    +1 / -1 ???고??꾩뿉??諛붽? ???덇쾶 ?좎??쒕떎.                         */
       /*                                                                        */
       /*  imu_poll_enabled                                                      */
       /*  - 1?대㈃ MPU6050 polling??湲곗〈 二쇨린?濡??섑뻾?쒕떎.                     */
       /*  - 0?대㈃ GY86_IMU_Task()媛 MPU I2C read瑜??꾩쟾??嫄대꼫?대떎.              */
       /*                                                                        */
       /*  mag_poll_enabled                                                      */
       /*  - 1?대㈃ HMC5883L polling??湲곗〈 二쇨린?濡??섑뻾?쒕떎.                    */
       /*  - 0?대㈃ magnetometer I2C read瑜?嫄대꼫?곗뼱 bus 遺?섎? 以꾩씤??            */
       /*                                                                        */
       /*  ms5611_only                                                           */
       /*  - 1?대㈃ "barometer only" 吏꾨떒 紐⑤뱶??                                */
       /*  - MPU/HMC probe? polling??紐⑤몢 留됯퀬,                                */
       /*    ALTITUDE ?쒕퉬?ㅻ룄 IMU aid / IMU audio source瑜??먮룞 鍮꾪솢?깊솕?쒕떎.   */
       /*  - imu_poll_enabled / mag_poll_enabled 媛믪? 蹂댁〈?섏?留?                */
       /*    ???뚮옒洹멸? 1???숈븞?먮뒗 臾댁떆?쒕떎.                                  */
       /* ---------------------------------------------------------------------- */
       int8_t  imu_vertical_sign;
       uint8_t imu_poll_enabled;
       uint8_t mag_poll_enabled;
       uint8_t ms5611_only;

    /* ---------------------------------------------------------------------- */
    /*  Pressure / display LPF ?쒖젙??                                         */
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
    /*  ?뺤? ?곹깭(rest) ?꾩슜 display ?덉젙??                                   */
    /*                                                                        */
    /*  core filter / fast vario??洹몃?濡??먭퀬                                 */
    /*  "理쒖쥌 ?붾㈃媛? 留???泥쒖쿇???곕씪媛寃?留뚮뱶??怨꾩링?대떎.                  */
    /*                                                                        */
    /*  rest_display_enabled      : ?쒖떆 ?꾩슜 stabilizer on/off               */
    /*  rest_detect_vario_cms     : ?뺤? ?먯젙??slow vario ?꾧퀎媛?            */
    /*  rest_detect_accel_mg      : ?뺤? ?먯젙??IMU specific-force ?꾧퀎媛?    */
    /*  rest_display_tau_ms       : ?뺤? ??display LPF ?쒖젙??               */
    /*  rest_display_hold_cm      : ??踰붿쐞 ?대궡??誘몄꽭 蹂?붾뒗 ?붾㈃媛??좎?    */
    /*  zupt_enabled              : ?뺤? ?곹깭????velocity=0 pseudo update   */
    /* ---------------------------------------------------------------------- */
    uint8_t  rest_display_enabled;
    uint8_t  zupt_enabled;
    uint16_t reserved_rest0;
    uint16_t rest_detect_vario_cms;
    uint16_t rest_detect_accel_mg;
    uint16_t rest_display_tau_ms;
    uint16_t rest_display_hold_cm;

    /* ---------------------------------------------------------------------- */
    /*  Baro / GPS measurement noise 諛?gate                                  */
    /*                                                                        */
    /*  baro_measurement_noise_cm      : ?됱삩 援ш컙?먯꽌 湲곕낯 baro altitude R    */
    /*  baro_adaptive_noise_max_cm     : residual???????щ┫ 理쒕? R          */
    /*  gps_measurement_noise_floor_cm : GPS altitude 理쒖냼 noise floor         */
    /*  gps_max_vacc_mm                : GPS vertical accuracy ?덉슜 ?곹븳       */
    /*  gps_max_pdop_x100              : GPS pDOP ?덉슜 ?곹븳                    */
    /*  gps_min_sats                   : 理쒖냼 ?꾩꽦 ??                         */
    /*  gps_bias_tau_ms                : GPS濡?baro bias瑜??곕씪?〓뒗 ?띾룄       */
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
    /*  pressure -> altitude derivative 濡?留뚮뱺 baro vario瑜?                 */
    /*  velocity measurement濡?KF??吏곸젒 ?ｋ뒗??                              */
    /*                                                                        */
    /*  baro_vario_lpf_tau_ms             : derivative LPF ?쒖젙??            */
    /*  baro_vario_measurement_noise_cms  : velocity update R                 */
    /* ---------------------------------------------------------------------- */
    uint16_t baro_vario_lpf_tau_ms;
    uint16_t baro_vario_measurement_noise_cms;

    /* ---------------------------------------------------------------------- */
    /*  IMU vertical specific-force 異붿젙 ?쒕떇                                  */
    /*                                                                        */
    /*  imu_gravity_tau_ms             : gyro-aided gravity estimator          */
    /*                                   accel correction ?쒖젙??             */
    /*  imu_accel_tau_ms               : vertical specific-force LPF           */
    /*  imu_accel_lsb_per_g            : MPU raw scale                         */
    /*  imu_vertical_deadband_mg       : ?꾩＜ ?묒? ?섏쭅 ?깅텇 臾댁떆             */
    /*  imu_vertical_clip_mg           : 怨쇰룄???낅젰 clip                     */
    /*  imu_measurement_noise_cms2     : KF4 predict input noise 湲곗?媛?      */
    /*  imu_gyro_lsb_per_dps           : gyro raw scale                        */
    /*  imu_attitude_accel_gate_mg     : accel norm??1g?먯꽌 ?쇰쭏??踰쀬뼱?섎㈃   */
    /*                                   attitude trust瑜?源롮쓣吏 寃곗젙         */
    /*  imu_predict_min_trust_permille : ??媛??댄븯 trust?먯꽌??              */
    /*                                   IMU predict weight瑜?0?쇰줈 ?붾떎.     */
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
    /*  ?⑥쐞??debug/IDE?먯꽌 議곗젅?섍린 ?ъ슫 "珥덈떦 洹쒕え" 濡??붾떎.               */
    /*  ?ㅼ젣 援ы쁽?먯꽌??dt瑜?怨깊빐 process covariance??諛섏쁺?쒕떎.               */
    /* ---------------------------------------------------------------------- */
    uint16_t kf_q_height_cm_per_s;
    uint16_t kf_q_velocity_cms_per_s;
    uint16_t kf_q_baro_bias_cm_per_s;
    uint16_t kf_q_accel_bias_cms2_per_s;

    /* ---------------------------------------------------------------------- */
    /*  ALTITUDE debug page ?꾩슜 test vario audio                              */
    /*                                                                        */
    /*  debug_audio_enabled : test tone 異쒕젰 ?덉슜                              */
    /*  debug_audio_source  : 0=no-IMU fast vario, 1=IMU fast vario            */
    /*  audio_deadband_cms  : ??媛??댄븯??vario??臾댁쓬                        */
    /*  audio_min/max_freq  : FM vario tone 二쇳뙆??踰붿쐞                        */
    /*  audio_repeat_ms     : climb beep cadence 湲곗?媛?                       */
    /*  audio_beep_ms       : climb beep tone 湲몄씠 湲곗?媛?                     */
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
/*  BIKE DYNAMICS ?ъ슜???ㅼ젙                                                   */
/*                                                                            */
/*  湲곕낯 泥좏븰                                                                   */
/*  - lean / grade / lat-G / accel-decel ?                                    */
/*    紐⑤몢 "?꾨젅??湲곗? + reset 湲곗? + IMU core" 濡?怨꾩궛?쒕떎.                  */
/*  - GNSS / future OBD speed??bias anchor ??븷留??쒕떎.                       */
/*  - mount axis / yaw trim ???ㅼ젙?쇰줈 ?몄텧?댁꽌                               */
/*    enclosure / PCB ?μ갑 ?ㅼ감瑜??꾩옣?먯꽌 諛붾줈 ?≪쓣 ???덇쾶 ?쒕떎.             */
/* -------------------------------------------------------------------------- */
typedef enum
{
    APP_BIKE_AXIS_POS_X = 0u,   /* sensor +X 媛 李⑤웾 forward/left/up 諛⑺뼢????*/
    APP_BIKE_AXIS_NEG_X = 1u,   /* sensor -X 媛 ?대떦 諛⑺뼢????                 */
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
    /* ------------------------------------------------------------------ */
    /*  BANK ANGLE CALC backend selection                                  */
    /*                                                                    */
    /*  FUSION   : IMU + coordinated manoeuvre + GNSS/OBD aid 瑜?紐⑤몢 ?ъ슜 */
    /*  OBD      : OBD speed + IMU yaw-rate 湲곕컲 coordinated lean ?곗꽑      */
    /*  GNSS     : GNSS speed/course + IMU 湲곕컲 lean aid ?곗꽑               */
    /*  IMU_ONLY : ?몃? aid瑜?lean / LatG 怨꾩궛???꾪? ?ъ슜?섏? ?딆쓬         */
    /* ------------------------------------------------------------------ */
    APP_BIKE_BANK_CALC_MODE_FUSION   = 0u,
    APP_BIKE_BANK_CALC_MODE_OBD      = 1u,
    APP_BIKE_BANK_CALC_MODE_GNSS     = 2u,
    APP_BIKE_BANK_CALC_MODE_IMU_ONLY = 3u
} app_bike_bank_calc_mode_t;

typedef enum
{
    APP_BIKE_ESTIMATOR_MODE_IMU_ONLY   = 0u,
    APP_BIKE_ESTIMATOR_MODE_GNSS_AIDED = 1u,
    APP_BIKE_ESTIMATOR_MODE_OBD_AIDED  = 2u,
    APP_BIKE_ESTIMATOR_MODE_FUSION     = 3u
} app_bike_estimator_mode_t;

/* -------------------------------------------------------------------------- */
/*  heading 怨듦컻 source                                                       */
/*                                                                            */
/*  ??媛믪? lean / grade 怨꾩궛 寃쎈줈???遺꾨━??"蹂댁“ heading 異쒕젰" ?꾩슜?대떎.     */
/*  - GNSS : 異⑸텇???띾룄? headAcc 議곌굔??留뚯”?섎뒗 寃쎌슦                         */
/*  - MAG  : GNSS heading???녾퀬, tilt-compensated magnetic heading留?        */
/*           ?ъ슜?????덉쓣 ??                                                 */
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
    /*  ?쒕퉬??on/off 諛?zero ?뺤콉                                              */
    /* ---------------------------------------------------------------------- */
    uint8_t enabled;                     /* 1?대㈃ BIKE_DYNAMICS ?쒕퉬???ъ슜         */
    uint8_t auto_zero_on_boot;           /* 1?대㈃ 泥??좏슚 IMU ?섑뵆?먯꽌 ?먮룞 reset   */
    uint8_t gnss_aid_enabled;            /* 1?대㈃ GNSS speed/course aid ?ъ슜        */
    uint8_t obd_aid_enabled;             /* 1?대㈃ future OBD speed aid ?ъ슜         */
    uint8_t bank_calc_mode;              /* app_bike_bank_calc_mode_t raw           */

    /* ---------------------------------------------------------------------- */
    /*  ?μ갑 異?/ yaw trim                                                      */
    /*                                                                        */
    /*  mount_forward_axis                                                     */
    /*  - ?쇱꽌 蹂대뱶???대뼡 異뺤씠 李⑤웾 forward 瑜?蹂대뒗媛                          */
    /*                                                                        */
    /*  mount_left_axis                                                        */
    /*  - ?쇱꽌 蹂대뱶???대뼡 異뺤씠 李⑤웾 left 瑜?蹂대뒗媛                             */
    /*                                                                        */
    /*  mount_yaw_trim_deg_x10                                                 */
    /*  - enclosure / PCB 湲곌퀎 ?ㅼ감瑜?蹂댁젙?섎뒗 ?섏쭅異?二쇱쐞 trim                */
    /*  - ?⑥쐞: 0.1 deg                                                        */
    /* ---------------------------------------------------------------------- */
    uint8_t mount_forward_axis;          /* app_bike_axis_t raw                    */
    uint8_t mount_left_axis;             /* app_bike_axis_t raw                    */
    int16_t mount_yaw_trim_deg_x10;      /* 0.1 deg                                */

    /* ---------------------------------------------------------------------- */
    /*  IMU ?ㅼ???/ observer gate                                              */
    /*                                                                        */
    /*  imu_accel_lsb_per_g                                                    */
    /*  - ?꾩옱 MPU6050 짹4g 湲곕낯媛믪? 8192                                       */
    /*                                                                        */
    /*  imu_gyro_lsb_per_dps_x10                                               */
    /*  - 0.1 LSB/dps ?⑥쐞                                                     */
    /*  - ?꾩옱 MPU6050 짹500dps 湲곕낯媛믪? 655 (=65.5 LSB/dps)                    */
    /* ---------------------------------------------------------------------- */
    uint16_t imu_accel_lsb_per_g;
    uint16_t imu_gyro_lsb_per_dps_x10;

    uint16_t imu_gravity_tau_ms;             /* gravity correction LPF tau           */
    uint16_t imu_linear_tau_ms;              /* level accel LPF tau                  */
    uint16_t imu_attitude_accel_gate_mg;     /* norm trust gate                      */
    uint16_t imu_jerk_gate_mg_per_s;         /* rough-road jerk trust gate           */
    uint16_t imu_predict_min_trust_permille; /* bias update瑜??덉슜??理쒖냼 trust      */
    uint16_t imu_stale_timeout_ms;           /* raw sample stale timeout             */

    /* ---------------------------------------------------------------------- */
    /*  異쒕젰 ?꾩쿂由?                                                            */
    /* ---------------------------------------------------------------------- */
    uint16_t output_deadband_mg;         /* lat/lon 異쒕젰 deadband                 */
    uint16_t output_clip_mg;             /* lat/lon 異쒕젰 clip                     */
    uint16_t lean_display_tau_ms;        /* lean ?쒖떆 LPF                         */
    uint16_t grade_display_tau_ms;       /* grade ?쒖떆 LPF                        */
    uint16_t accel_display_tau_ms;       /* lat/lon ?쒖떆 LPF                      */

    /* ---------------------------------------------------------------------- */
    /*  GNSS / OBD aid gate                                                     */
    /* ---------------------------------------------------------------------- */
    uint16_t gnss_min_speed_kmh_x10;         /* heading aid 理쒖냼 ?띾룄                */
    uint16_t gnss_max_speed_acc_kmh_x10;     /* GNSS speed accuracy gate             */
    uint16_t gnss_max_head_acc_deg_x10;      /* GNSS heading accuracy gate           */
    uint16_t gnss_bias_tau_ms;               /* external ref -> bias ?곸쓳 tau        */
    uint16_t gnss_outlier_gate_mg;           /* ref outlier reject gate              */

    uint16_t obd_stale_timeout_ms;           /* future OBD speed stale timeout       */
    uint16_t reserved0;                      /* ?ν썑 ?뺤옣??                         */
} app_bike_settings_t;


typedef struct
{
    app_gps_settings_t       gps;
    app_clock_settings_t     clock;
    app_backlight_settings_t backlight;
    app_uc1608_settings_t    uc1608;
    app_altitude_settings_t  altitude;
    app_bike_settings_t      bike;       /* 紐⑦꽣?ъ씠???꾩슜 dynamics ?ㅼ젙 ??μ냼     */
} app_settings_t;


/* -------------------------------------------------------------------------- */
/*  RTC / CLOCK 怨듦컻 ?곹깭                                                      */
/*                                                                            */
/*  泥좏븰                                                                       */
/*  - ?섎뱶?⑥뼱 RTC??UTC 湲곗??쇰줈留??좎??쒕떎.                                  */
/*  - timezone ?곸슜/?붿씪 怨꾩궛/濡쒖뺄 ?쒓컙 ?꾧컻??APP 怨꾩링?먯꽌 ?섑뻾?쒕떎.          */
/*  - UI??????μ냼??valid ?뚮옒洹몃? 蹂닿퀬 ?쒓컙??洹몃┫吏 "--:--"瑜?洹몃┫吏      */
/*    寃곗젙?????덈떎.                                                          */
/*                                                                            */
/*  backup domain ?ъ슜 洹쒖튃                                                    */
/*  - RTC date/time register : ?ㅼ젣 ?쒓퀎 ?먯껜                                  */
/*  - RTC backup register   : clock ?꾩슜 magic / config / validity metadata    */
/*  - BKPSRAM               : RTC? 臾닿????ㅻⅨ persistent ?⑸룄(APP_FAULT ??  */
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
    uint8_t  hours;                      /* HAL_RTC_GetTime 寃곌낵 raw hour             */
    uint8_t  minutes;                    /* HAL_RTC_GetTime 寃곌낵 raw minute           */
    uint8_t  seconds;                    /* HAL_RTC_GetTime 寃곌낵 raw second           */
    uint8_t  time_format;                /* HAL raw time format                       */
    uint32_t sub_seconds;                /* HAL raw subsecond                         */
    uint32_t second_fraction;            /* HAL raw second fraction                   */
    uint32_t daylight_saving;            /* HAL raw daylight saving field             */
    uint32_t store_operation;            /* HAL raw store operation field             */
} app_clock_rtc_time_raw_t;

typedef struct
{
    uint8_t week_day;                    /* HAL_RTC_GetDate 寃곌낵 raw weekday          */
    uint8_t month;                       /* HAL_RTC_GetDate 寃곌낵 raw month            */
    uint8_t date;                        /* HAL_RTC_GetDate 寃곌낵 raw day-of-month     */
    uint8_t year_2digit;                 /* HAL_RTC_GetDate 寃곌낵 raw year(00..99)     */
} app_clock_rtc_date_raw_t;

typedef struct
{
    bool     initialized;                /* APP_CLOCK_Init ?꾨즺 ?щ?                  */
    bool     backup_config_valid;        /* RTC backup register config magic ?좏슚     */
    bool     rtc_time_valid;             /* ?꾩옱 RTC 媛믪씠 ?좊ː 媛?ν븳媛               */
    bool     rtc_read_valid;             /* 留덉?留?HAL read 寃곌낵媛 ?좏슚?덈뒗媛         */
    bool     gps_candidate_valid;        /* ?꾩옱 GPS time/date媛 sync ?꾨낫?멸?        */
    bool     gps_auto_sync_enabled_runtime; /* runtime??諛섏쁺??auto-sync ?곹깭        */
    bool     gps_last_sync_success;      /* 留덉?留?GPS sync ?깃났 ?щ?                 */
    bool     gps_last_sync_was_full;     /* 留덉?留?GPS sync媛 full date/time??붽?    */
    bool     gps_resolved_seen;          /* ?대쾲 遺?낆뿉??GPS fully resolved 愿痢??щ? */
    bool     timezone_config_valid;      /* timezone 媛?踰붿쐞媛 ?좏슚?쒓?               */

    int8_t   timezone_quarters;          /* UTC offset / 15 min ?⑥쐞                  */
    uint8_t  gps_sync_interval_minutes;  /* periodic GPS sync 二쇨린                    */
    uint8_t  last_sync_source;           /* app_clock_sync_source_t raw               */
    uint8_t  reserved0;                  /* ?뺣젹/?ν썑 ?뺤옣??                          */

    uint32_t last_hw_read_ms;            /* 留덉?留?RTC ?섎뱶?⑥뼱 read ?쒓컖             */
    uint32_t last_hw_set_ms;             /* 留덉?留?RTC ?섎뱶?⑥뼱 write ?쒓컖            */
    uint32_t last_validity_change_ms;    /* valid ?뚮옒洹?理쒖쥌 蹂寃??쒓컖               */
    uint32_t last_gps_sync_ms;           /* 留덉?留?GPS sync ?꾨즺 ?쒓컖                 */
    uint32_t next_gps_sync_due_ms;       /* ?ㅼ쓬 GPS periodic sync ?덉젙 ?쒓컖          */

    uint32_t gps_full_sync_count;        /* GPS full sync ?꾩쟻 ?잛닔                   */
    uint32_t gps_periodic_sync_count;    /* GPS time-only sync ?꾩쟻 ?잛닔              */
    uint32_t manual_set_count;           /* ?섎룞 ?ㅼ젙 ?꾩쟻 ?잛닔                       */
    uint32_t invalid_read_count;         /* invalid RTC read 愿痢??잛닔                */
    uint32_t rtc_read_count;             /* RTC read ?깃났 ?꾩쟻 ?잛닔                   */
    uint32_t rtc_write_count;            /* RTC write ?깃났 ?꾩쟻 ?잛닔                  */
    uint32_t rtc_error_count;            /* HAL RTC read/write ?ㅽ뙣 ?꾩쟻 ?잛닔         */
    uint32_t backup_write_count;         /* RTC backup config write ?꾩쟻 ?잛닔         */
    uint32_t backup_read_count;          /* RTC backup config read ?꾩쟻 ?잛닔          */
    uint32_t last_hal_status;            /* 留덉?留?HAL_RTC_* 諛섑솚媛?raw               */

    app_clock_rtc_time_raw_t rtc_time_raw; /* 留덉?留?RTC raw time snapshot            */
    app_clock_rtc_date_raw_t rtc_date_raw; /* 留덉?留?RTC raw date snapshot            */

    app_clock_calendar_t utc;            /* ?섎뱶?⑥뼱 RTC?먯꽌 ?쎌? UTC ?쒓컙            */
    app_clock_calendar_t local;          /* timezone ?곸슜 ??local ?쒓컙               */
    app_clock_calendar_t last_gps_utc;   /* 留덉?留됱쑝濡?梨꾪깮??GPS UTC ?쒓컙            */
} app_clock_state_t;

/* -------------------------------------------------------------------------- */
/*  UBX raw payload structures                                                 */
/*                                                                            */
/*  "raw" ??GPS 移⑹씠 蹂대궦 payload瑜?媛?ν븳 ??洹몃?濡??대뒗 援ъ“泥대떎.          */
/*  ?붾쾭源낇븷 ??媛??誘우쓣 ???덈뒗 ?먮즺??raw ?대떎.                              */
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
/*  raw UBX payload瑜?洹몃?濡??ㅺ퀬 ?덉쑝硫댁꽌?? UI媛 諛붾줈 ?쎄린 ?쎄쾶               */
/*  ?꾧컻???꾨뱶瑜??곕줈 ?붾떎.                                                    */
/* -------------------------------------------------------------------------- */

typedef struct
{
    uint8_t gnss_id;
    uint8_t sv_id;
    uint8_t cno_dbhz;
    int8_t  elevation_deg;
    int16_t azimuth_deg;
    int16_t pseudorange_res_dm;   /* UBX??0.1 m ?⑥쐞 */

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

    float    speed_llh_mps;       /* lat/lon 蹂?붾웾?쇰줈 怨꾩궛???뚯깮 ?띾룄 */
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
/*  ?대쾲 ?뺤옣?먯꽌??APP_STATE媛 GPS肉??꾨땲??                                 */
/*  IMU(GY-86)? DS18B20??"?좎씪??怨듦컻 ??μ냼" ??븷??留〓뒗??                 */
/*                                                                            */
/*  以묒슂??洹쒖튃                                                                */
/*  - ?쇱꽌 ?쒕씪?대쾭??static ?대? ?곹깭???쒕씪?대쾭 ?뚯씪 ?덉뿉留??④꺼 ?붾떎.        */
/*  - ?ㅻⅨ ?뚯씪(UI / logger / state machine)? APP_STATE留?蹂몃떎.               */
/*  - 洹몃옒??raw 痢≪젙媛? ?붾쾭洹?移댁슫?? ?쇱꽌 ID, 留덉?留??ㅻ쪟 媛숈? ?뺣낫??       */
/*    媛?ν븳 ??APP_STATE ?덉뿉 援ъ“泥대줈 ?뺣━?댁꽌 ?ｋ뒗??                       */
/* -------------------------------------------------------------------------- */

typedef enum
{
    APP_IMU_BACKEND_NONE     = 0u,
    APP_IMU_BACKEND_MPU6050  = 1u,
    APP_IMU_BACKEND_HMC5883L = 2u,
    APP_IMU_BACKEND_MS5611   = 3u
} app_imu_backend_id_t;

/* GY-86 ?대? 媛?移⑹쓽 鍮꾪듃 留덉뒪??
 * status_flags / detected_mask / init_ok_mask ??怨듯넻?쇰줈 ?ъ슜?쒕떎. */
enum
{
    APP_GY86_DEVICE_MPU  = 0x01u,
    APP_GY86_DEVICE_MAG  = 0x02u,
    APP_GY86_DEVICE_BARO = 0x04u
};

/* 理쒖떊 raw ?섑뵆???좏슚 鍮꾪듃.
 * "?μ튂媛 議댁옱?섎뒗媛?" ? "理쒓렐 ?섑뵆???좏슚?쒓??" ??援щ텇?쒕떎. */
enum
{
    APP_GY86_STATUS_MPU_VALID  = 0x01u,
    APP_GY86_STATUS_MAG_VALID  = 0x02u,
    APP_GY86_STATUS_BARO_VALID = 0x04u
};

/* MPU/媛?띾룄/?먯씠濡?raw 媛???μ냼 */
typedef struct
{
    uint32_t timestamp_ms;     /* ??raw ?섑뵆???쎌? SysTick ?쒓컖               */
    uint32_t sample_count;     /* ?꾩쟻 ?섑뵆 ??                                 */

    int16_t  accel_x_raw;      /* MPU 媛?띾룄 X raw                             */
    int16_t  accel_y_raw;      /* MPU 媛?띾룄 Y raw                             */
    int16_t  accel_z_raw;      /* MPU 媛?띾룄 Z raw                             */

    int16_t  gyro_x_raw;       /* MPU ?먯씠濡?X raw                             */
    int16_t  gyro_y_raw;       /* MPU ?먯씠濡?Y raw                             */
    int16_t  gyro_z_raw;       /* MPU ?먯씠濡?Z raw                             */

    int16_t  temp_raw;         /* MPU ?대? ?⑤룄 raw                            */
    int16_t  temp_cdeg;        /* MPU ?대? ?⑤룄, 0.01 degC 怨좎젙?뚯닔??         */
} app_gy86_mpu_raw_t;

/* ?먮젰怨?raw 媛???μ냼 */
typedef struct
{
    uint32_t timestamp_ms;     /* ??raw ?섑뵆???쎌? SysTick ?쒓컖               */
    uint32_t sample_count;     /* ?꾩쟻 ?섑뵆 ??                                 */

    int16_t  mag_x_raw;        /* ?먮젰怨?X raw                                  */
    int16_t  mag_y_raw;        /* ?먮젰怨?Y raw                                  */
    int16_t  mag_z_raw;        /* ?먮젰怨?Z raw                                  */
} app_gy86_mag_raw_t;

/* 湲곗븬怨?raw + 蹂댁젙怨꾩닔 ??μ냼 */
typedef struct
{
    uint32_t timestamp_ms;         /* ?꾩쟾??D1/D2 ???명듃瑜?怨꾩궛???쒓컖         */
    uint32_t sample_count;         /* ?꾩쟻 ?좏슚 ?섑뵆 ??                          */

    uint32_t d1_raw;               /* MS5611 pressure ADC raw                     */
    uint32_t d2_raw;               /* MS5611 temperature ADC raw                  */

    uint16_t prom_c[7];            /* C1..C6 ?ъ슜, [0]? 鍮꾩썙 ??                 */

    int32_t  temp_cdeg;            /* 蹂댁젙 ???⑤룄, 0.01 degC                     */
    int32_t  pressure_hpa_x100;    /* 蹂댁젙 ??湲곗븬, 0.01 hPa                      */
    int32_t  pressure_pa;          /* 蹂댁젙 ??湲곗븬, Pa                            */
} app_gy86_baro_raw_t;


#ifndef APP_GY86_BARO_SENSOR_SLOTS
#define APP_GY86_BARO_SENSOR_SLOTS 2u
#endif

/* -------------------------------------------------------------------------- */
/*  dual barometer app-state 怨듦컻 ?щ’                                         */
/*                                                                            */
/*  紐⑹쟻                                                                       */
/*  - 湲곗〈 APP_STATE.gy86.baro ??"?곸쐞 怨꾩링??洹몃?濡??곕뒗 fused 寃곌낵" ?섎굹留?  */
/*    ?좎??쒕떎.                                                                */
/*  - ?꾨옒 援ъ“泥대뒗 洹?fused 寃곌낵媛 ?대뼸寃?留뚮뱾?댁죱?붿? ?뺤씤?섍린 ?꾪븳          */
/*    "?쇱꽌蹂?吏꾨떒 ?щ’" ?대떎.                                                 */
/*                                                                            */
/*  ?ъ슜 洹쒖튃                                                                  */
/*  - ??UI/self-test ????援ъ“泥대? read-only 濡쒕쭔 ?ъ슜?쒕떎.                  */
/*  - low-level driver(GY86_IMU.c)留???媛믪쓣 梨꾩슫??                           */
/*  - sensor 1媛?鍮뚮뱶?먯꽌??slot ?섎뒗 2媛쒕? ?좎??섍퀬, 誘몄궗??slot? 0?쇰줈 ?붾떎. */
/*                                                                            */
/*  pressure_pa / pressure_hpa_x100                                            */
/*  - ???꾨줈?앺듃??MS5611 寃쎈줈?먯꽌??                                          */
/*    hPa*100 怨?Pa 媛 ?섏튂?곸쑝濡??숈씪???ㅼ??쇱씠誘濡?                          */
/*    ?????④퍡 ?좎??대룄 媛믪? 媛숈? ?レ옄 踰붿쐞瑜?媛吏꾨떎.                        */
/*                                                                            */
/*  aligned_pressure_pa                                                        */
/*  - secondary sensor??static offset(bias)???쒓굅??                          */
/*    fusion ?먮떒???ㅼ젣濡??ъ슜??pressure 媛?                                 */
/*                                                                            */
/*  bias_pa                                                                    */
/*  - primary 湲곗??쇰줈 異붿젙???곷? ?뺣젰 ?ㅽ봽??                                */
/*                                                                            */
/*  residual_pa                                                                */
/*  - aligned_pressure_pa ? primary pressure ?ъ씠???붿감                      */
/*  - outlier reject / disagreement gate媛 蹂대뒗 ?듭떖 媛?                       */
/* -------------------------------------------------------------------------- */
typedef struct
{
    uint8_t  configured;           /* ??slot???ㅼ젣 ?섎뱶?⑥뼱 諛곗튂??議댁옱?섎뒗媛   */
    uint8_t  online;               /* ?꾩옱 probe/init/poll 湲곗? ?댁븘 ?덈뒗媛       */
    uint8_t  valid;                /* 理쒓렐 sample???꾩쟾??D1/D2 ?명듃?멸?         */
    uint8_t  fresh;                /* fusion freshness timeout ?덉뿉 ?ㅼ뼱?ㅻ뒗媛    */

    uint8_t  selected;             /* ?대쾲 fused sample 怨꾩궛???ㅼ젣 ?ъ슜?섏뿀?붽?  */
    uint8_t  rejected;             /* disagreement/stale gate濡??쒖쇅?섏뿀?붽?      */
    uint8_t  bus_id;               /* 1=I2C1, 2=I2C2                              */
    uint8_t  addr_7bit;            /* ?щ엺???쎄린 ?ъ슫 7-bit I2C 二쇱냼             */

    uint8_t  error_streak;         /* runtime ?곗냽 ?ㅻ쪟 streak                    */
    uint8_t  reserved0;            /* ?뺣젹/?ν썑 ?뺤옣??                           */
    uint16_t weight_permille;      /* ?대쾲 fusion?먯꽌 ?ъ슜??媛以묒튂 0..1000       */

    uint32_t timestamp_ms;         /* 留덉?留??꾩쟾 ?섑뵆 ?쒓컖                       */
    uint32_t sample_count;         /* ???쇱꽌 ?⑤룆 ?꾩쟻 sample count              */

    int32_t  temp_cdeg;            /* ???쇱꽌 ?⑤룆 ?⑤룄, 0.01 degC               */
    int32_t  pressure_hpa_x100;    /* ???쇱꽌 ?⑤룆 pressure, 0.01 hPa            */
    int32_t  pressure_pa;          /* ???쇱꽌 ?⑤룆 pressure, Pa                  */

    int32_t  aligned_pressure_pa;  /* bias ?쒓굅 ??fusion???ъ엯??pressure       */
    int32_t  bias_pa;              /* primary ?鍮?異붿젙??static offset           */
    int32_t  residual_pa;          /* aligned - primary residual                  */
} app_gy86_baro_sensor_state_t;

/* -------------------------------------------------------------------------- */
/*  dual barometer fusion summary flag                                         */
/*                                                                            */
/*  SINGLE_SENSOR        : ?ㅼ젣 publish媛 1媛??쇱꽌留뚯쑝濡??대쨪議뚮뒗媛             */
/*  STALE_FALLBACK       : ?ㅻⅨ ?쇱꽌??stale ?댁꽌 ?쒖쇅?섏뿀?붽?                 */
/*  DISAGREE_REJECT      : ?ㅻⅨ ?쇱꽌??disagreement gate濡??쒖쇅?섏뿀?붽?        */
/*  OFFSET_TRACK_ACTIVE  : bias 異붿젙媛믪씠 ?섎? ?덇쾶 ?쒖꽦?붾릺?덈뒗媛              */
/* -------------------------------------------------------------------------- */
enum
{
    APP_GY86_BARO_FUSION_FLAG_SINGLE_SENSOR       = 0x01u,
    APP_GY86_BARO_FUSION_FLAG_STALE_FALLBACK      = 0x02u,
    APP_GY86_BARO_FUSION_FLAG_DISAGREE_REJECT     = 0x04u,
    APP_GY86_BARO_FUSION_FLAG_OFFSET_TRACK_ACTIVE = 0x08u
};

/* GY-86 ?꾩껜 紐⑤뱢 ?붾쾭洹??뺣낫 */
typedef struct
{
    uint8_t  accelgyro_backend_id; /* ?꾩옱 accel/gyro backend ID                  */
    uint8_t  mag_backend_id;       /* ?꾩옱 magnetometer backend ID                */
    uint8_t  baro_backend_id;      /* ?꾩옱 barometer backend ID                   */

    uint8_t  detected_mask;        /* I2C ?묐떟/ID 湲곗??쇰줈 媛먯???移?鍮꾪듃留덉뒪??  */
    uint8_t  init_ok_mask;         /* init源뚯? ?뺤긽 ?꾨즺??移?鍮꾪듃留덉뒪??         */

    uint8_t  last_hal_status_mpu;  /* 留덉?留?MPU HAL status                       */
    uint8_t  last_hal_status_mag;  /* 留덉?留?MAG HAL status                       */
    uint8_t  last_hal_status_baro; /* 留덉?留?BARO HAL status                      */

    uint8_t  mpu_whoami;           /* MPU WHO_AM_I raw                            */
    uint8_t  mag_id_a;             /* HMC5883L ID A raw                           */
    uint8_t  mag_id_b;             /* HMC5883L ID B raw                           */
    uint8_t  mag_id_c;             /* HMC5883L ID C raw                           */

    uint8_t  ms5611_state;         /* D1/D2 蹂??state machine ?대? ?곹깭          */

    /* ---------------------------------------------------------------------- */
    /*  dual barometer fusion summary                                          */
    /*                                                                        */
    /*  baro_device_slots        : compile-time ?쇰줈 ?몄텧?섎뒗 slot ??         */
    /*  baro_fused_sensor_count  : ?대쾲 fused sample???ㅼ젣 ?ъ슜???쇱꽌 ??    */
    /*  baro_primary_sensor_index: ?꾩옱 primary/fallback 湲곗? sensor index     */
    /*  baro_fusion_flags        : APP_GY86_BARO_FUSION_FLAG_* bitmask         */
    /*                                                                        */
    /*  baro_sensor_delta_pa_raw                                                */
    /*  - sensor1 - sensor0 ??raw pressure 李⑥씠                               */
    /*                                                                        */
    /*  baro_sensor_delta_pa_aligned                                            */
    /*  - bias 蹂댁젙 ??sensor1 - sensor0 李⑥씠                                   */
    /*  - disagreement gate????媛믪쓣 蹂몃떎                                     */
    /* ---------------------------------------------------------------------- */
    uint8_t  baro_device_slots;
    uint8_t  baro_fused_sensor_count;
    uint8_t  baro_primary_sensor_index;
    uint8_t  baro_fusion_flags;
    int32_t  baro_sensor_delta_pa_raw;
    int32_t  baro_sensor_delta_pa_aligned;

    uint32_t init_attempt_count;   /* ?꾩껜 init/re-probe ?쒕룄 ?잛닔                */
    uint32_t last_init_attempt_ms; /* 留덉?留?init ?쒕룄 ?쒓컖                       */

    uint16_t mpu_poll_period_ms;   /* ?꾩옱 accel/gyro polling 二쇨린                */
    uint16_t mag_poll_period_ms;   /* ?꾩옱 magnetometer polling 二쇨린              */
    uint16_t baro_poll_period_ms;  /* ?꾩옱 pressure ?꾩쟾 ?섑뵆 二쇨린                */

    uint32_t mpu_last_ok_ms;       /* 留덉?留?MPU read ?깃났 ?쒓컖                   */
    uint32_t mag_last_ok_ms;       /* 留덉?留?MAG read ?깃났 ?쒓컖                   */
    uint32_t baro_last_ok_ms;      /* 留덉?留?BARO full sample ?깃났 ?쒓컖           */

    uint32_t mpu_error_count;      /* MPU ?꾩쟻 read/init ?ㅻ쪟 ?잛닔                */
    uint32_t mag_error_count;      /* MAG ?꾩쟻 read/init ?ㅻ쪟 ?잛닔                */
    uint32_t baro_error_count;     /* BARO ?꾩쟻 read/init ?ㅻ쪟 ?잛닔               */
} app_gy86_debug_t;

/* GY-86 ?꾩껜 怨듦컻 ?곹깭 */
typedef struct
{
    bool              initialized; /* ?곸뼱???섎굹 ?댁긽???섏쐞 移⑹씠 init ?깃났      */
    uint8_t           status_flags;/* 理쒖떊 raw ?좏슚 鍮꾪듃                          */
    uint32_t          last_update_ms; /* 留덉?留됱쑝濡??대뼡 ?섏쐞 移⑹씠???낅뜲?댄듃???쒓컖 */

    app_gy86_mpu_raw_t           mpu;        /* accel/gyro/raw ??μ냼                    */
    app_gy86_mag_raw_t           mag;        /* magnetometer raw ??μ냼                  */
    app_gy86_baro_raw_t          baro;       /* fused pressure raw ??μ냼                */
    app_gy86_baro_sensor_state_t baro_sensor[APP_GY86_BARO_SENSOR_SLOTS];
                                             /* sensor蹂?dual-baro 吏꾨떒 ?щ’             */
    app_gy86_debug_t             debug;      /* GY-86 ?붾쾭洹???μ냼                      */
} app_gy86_state_t;

/* -------------------------------------------------------------------------- */
/*  DS18B20 raw state                                                          */
/* -------------------------------------------------------------------------- */

/* DS18B20 ?곹깭 鍮꾪듃 */
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

/* DS18B20 raw + scratchpad ??μ냼 */
typedef struct
{
    uint32_t timestamp_ms;         /* 留덉?留??뺤긽 ?섑뵆 ?쒓컖                        */
    uint32_t sample_count;         /* ?꾩쟻 ?뺤긽 ?섑뵆 ??                           */

    int16_t  raw_temp_lsb;         /* scratchpad temperature raw                   */
    int16_t  temp_c_x100;          /* 0.01 degC                                    */
    int16_t  temp_f_x100;          /* 0.01 degF                                    */

    int8_t   alarm_high_c;         /* TH register                                  */
    int8_t   alarm_low_c;          /* TL register                                  */

    uint8_t  config_reg;           /* config register raw                          */
    uint8_t  resolution_bits;      /* 9/10/11/12 bit                               */

    uint8_t  rom_code[8];          /* READ ROM 寃곌낵                                */
    uint8_t  scratchpad[9];        /* 留덉?留?scratchpad raw                        */

    uint8_t  crc_expected;         /* scratchpad[8]                                */
    uint8_t  crc_computed;         /* host 怨꾩궛 CRC                                */
} app_ds18b20_raw_t;

/* DS18B20 ?붾쾭洹??곹깭 */
typedef struct
{
    uint8_t  phase;                /* app_ds18b20_phase_t                          */
    uint8_t  last_error;           /* app_ds18b20_error_t                          */

    uint32_t init_attempt_count;   /* init/re-probe ?쒕룄 ?잛닔                      */
    uint32_t conversion_start_count; /* Convert T ?쒖옉 ?잛닔                        */
    uint32_t read_complete_count;  /* scratchpad read ?깃났 ?잛닔                    */

    uint32_t bus_reset_count;      /* 1-Wire reset pulse ?쒕룄 ?잛닔                 */
    uint32_t presence_fail_count;  /* presence pulse ?ㅽ뙣 ?잛닔                     */
    uint32_t crc_fail_count;       /* scratchpad CRC ?ㅽ뙣 ?잛닔                     */
    uint32_t transaction_fail_count; /* read/write ?몃옖??뀡 ?ㅽ뙣 ?잛닔             */

    uint32_t last_init_attempt_ms; /* 留덉?留?init ?쒕룄 ?쒓컖                        */
    uint32_t last_conversion_start_ms; /* 留덉?留?Convert T ?쒖옉 ?쒓컖               */
    uint32_t last_read_complete_ms;/* 留덉?留??뺤긽 read ?꾨즺 ?쒓컖                   */

    uint32_t next_action_ms;       /* ?ㅼ쓬 state machine ?≪뀡 ?덉젙 ?쒓컖            */
    uint16_t conversion_time_ms;   /* ?꾩옱 ?댁긽??湲곗? 蹂???쒓컙                   */

    uint32_t last_transaction_us;  /* 留덉?留?blocking 1-Wire ?몃옖??뀡 ?뚯슂 ?쒓컙    */
} app_ds18b20_debug_t;

/* DS18B20 ?꾩껜 怨듦컻 ?곹깭 */
typedef struct
{
    bool                 initialized; /* presence 諛?湲곕낯 珥덇린???깃났 ?щ?         */
    uint8_t              status_flags;/* present/valid/busy/crc ??鍮꾪듃            */
    uint32_t             last_update_ms; /* 留덉?留??곹깭 媛깆떊 ?쒓컖                   */

    app_ds18b20_raw_t    raw;         /* raw ??μ냼                                */
    app_ds18b20_debug_t  debug;       /* ?붾쾭洹???μ냼                              */
} app_ds18b20_state_t;

/* -------------------------------------------------------------------------- */
/*  ?쇱꽌 ?붾쾭洹?UI ?꾩슜 ?ㅻ깄??                                                 */
/*                                                                            */
/*  GPS UI???대? 寃쎈웾 snapshot???곕줈 ?먭퀬 ?덉쑝誘濡?                           */
/*  ?쇱꽌 ?섏씠吏??媛숈? 泥좏븰?쇰줈                                                */
/*  "?쇱꽌 ???⑹뼱由щ쭔 ??踰덉뿉" 蹂듭궗?섎뒗 ?꾩슜 援ъ“泥대? ?붾떎.                    */
/* -------------------------------------------------------------------------- */
typedef struct
{
    app_gy86_state_t     gy86;
    app_ds18b20_state_t  ds18b20;
} app_sensor_debug_snapshot_t;


/* -------------------------------------------------------------------------- */
/*  SD / FATFS 怨듦컻 ?곹깭                                                       */
/*                                                                            */
/*  ?대쾲 SD 釉뚮쭅?낆뿉?쒕뒗                                                        */
/*  - 移대뱶 detect raw/stable ?곹깭                                               */
/*  - mount / unmount / retry 移댁슫??                                           */
/*  - HAL SD handle ?붾쾭洹?媛?                                                  */
/*  - HAL_SD_GetCardInfo 濡??살? 移대뱶 ?뺣낫                                      */
/*  - FatFs volume 援ъ“泥댁뿉???쎌쓣 ???덈뒗 FAT 硫뷀??곗씠??                     */
/*  - root ?붾젆?곕━ 媛쒖닔/?섑뵆 ?대쫫                                              */
/*  瑜???援ъ“泥댁뿉 紐⑥븘 ?붾떎.                                                   */
/*                                                                            */
/*  二쇱쓽                                                                      */
/*  - ??援ъ“泥대뒗 'UI / logger媛 ?쎄린 ?ъ슫 怨듦컻 ??μ냼'??                      */
/*  - detect debounce??ISR ?몃? 援ы쁽 ?곹깭??APP_SD.c ?대? static runtime ??    */
/*    蹂꾨룄濡??먭퀬, ?ш린?먮뒗 ?щ엺??蹂닿퀬 ?띠? 寃곌낵 媛믩쭔 怨듦컻?쒕떎.               */
/* -------------------------------------------------------------------------- */
typedef struct
{
    /* ------------------------------ */
    /*  detect / mount 湲곕낯 ?곹깭       */
    /* ------------------------------ */
    bool     detect_raw_present;        /* 留덉?留?raw DET ? ?섑뵆 寃곌낵               */
    bool     detect_stable_present;     /* debounce ???뺤젙??移대뱶 ?쎌엯 ?곹깭         */
    bool     detect_debounce_pending;   /* ?꾩옱 detect debounce ?湲?以묒씤媛          */

    bool     initialized;               /* HAL_SD_Init 源뚯? ?깃났?덈뒗媛               */
    bool     mounted;                   /* f_mount ?깃났 ?곹깭                         */
    bool     fat_valid;                 /* f_getfree 湲곕컲 FAT 硫뷀??곗씠???좏슚 ?щ?   */
    bool     is_fat32;                  /* ?꾩옱 mount ???뚯씪?쒖뒪?쒖씠 FAT32 ?멸?     */

    /* ------------------------------ */
    /*  ?묒? raw enum/?곹깭 媛?         */
    /* ------------------------------ */
    uint8_t  fs_type;                   /* FatFs fs_type raw                         */
    uint8_t  card_type;                 /* HAL card type raw                         */
    uint8_t  card_version;              /* HAL card version raw                      */
    uint8_t  card_class;                /* HAL card class raw                        */
    uint8_t  hal_state;                 /* hsd.State snapshot                        */
    uint8_t  transfer_state;            /* BSP_SD_GetCardState() 寃곌낵                */
    uint8_t  last_bsp_init_status;      /* 留덉?留?BSP_SD_Init() 諛섑솚媛?              */
    uint8_t  root_entry_sample_count;   /* root_entry_sample_name[] ?ㅼ젣 ?ъ슜 媛쒖닔   */
    uint8_t  root_entry_sample_type[3]; /* 0:none 1:file 2:dir                       */

    /* ------------------------------ */
    /*  ?쒓컙異?/ due ?쒓컖              */
    /* ------------------------------ */
    uint32_t debounce_due_ms;           /* debounce 留뚮즺 ?덉젙 ?쒓컖                   */
    uint32_t last_detect_irq_ms;        /* 留덉?留?detect EXTI 吏꾩엯 ?쒓컖              */
    uint32_t last_detect_change_ms;     /* 留덉?留?stable insert/remove ?뺤젙 ?쒓컖     */
    uint32_t last_mount_attempt_ms;     /* 留덉?留?mount ?쒕룄 ?쒓컖                    */
    uint32_t last_mount_ms;             /* 留덉?留?mount ?깃났 ?쒓컖                    */
    uint32_t last_unmount_ms;           /* 留덉?留?unmount ?섑뻾 ?쒓컖                  */
    uint32_t mount_retry_due_ms;        /* ?ㅼ쓬 mount retry ?덉젙 ?쒓컖                */

    /* ------------------------------ */
    /*  hotplug / mount 移댁슫??        */
    /* ------------------------------ */
    uint32_t detect_irq_count;          /* detect pin EXTI ?꾩쟻 ?잛닔                 */
    uint32_t detect_insert_count;       /* stable insert ?뺤젙 ?잛닔                   */
    uint32_t detect_remove_count;       /* stable remove ?뺤젙 ?잛닔                   */

    uint32_t mount_attempt_count;       /* mount ?쒕룄 ?잛닔                           */
    uint32_t mount_success_count;       /* mount ?깃났 ?잛닔                           */
    uint32_t mount_fail_count;          /* mount ?ㅽ뙣 ?잛닔                           */
    uint32_t unmount_count;             /* unmount ?섑뻾 ?잛닔                         */

    /* ------------------------------ */
    /*  理쒓렐 寃곌낵 肄붾뱶                  */
    /* ------------------------------ */
    uint32_t last_mount_fresult;        /* 留덉?留?f_mount 寃곌낵 raw                   */
    uint32_t last_getfree_fresult;      /* 留덉?留?f_getfree 寃곌낵 raw                 */
    uint32_t last_root_scan_fresult;    /* 留덉?留?root scan 寃곌낵 raw                 */

    /* ------------------------------ */
    /*  storage failure trace         */
    /* ------------------------------ */
    uint8_t  last_fs_stage;             /* 留덉?留?FatFs stage raw enum                */
    uint8_t  last_io_op;                /* 0:none 1:read 2:write                      */
    uint8_t  last_io_bsp_status;        /* BSP_SD_* 諛섑솚媛? raw                       */
    uint8_t  last_io_hal_status;        /* HAL_SD_* 諛섑솚媛? raw / internal sentinel   */
    uint8_t  last_io_buffer_flags;      /* bit0:word-aligned bit1:ccm bit2:direct     */
    uint8_t  last_io_transfer_state;    /* BSP_SD_GetCardState() snapshot             */
    uint8_t  reserved_sd_diag0;
    uint16_t reserved_sd_diag1;

    uint32_t last_fs_fresult;           /* 留덉?留?FatFs error code raw                 */
    uint32_t last_io_block_addr;        /* 留덉?留?read/write block addr                */
    uint32_t last_io_block_count;       /* 留덉?留?read/write block count               */
    uint32_t last_io_buffer_addr;       /* 留덉?留?user/source buffer address           */
    uint32_t last_io_tick_ms;           /* 留덉?留?I/O failure snapshot tick            */
    uint32_t last_io_hal_error_code;    /* failure snapshot hsd.ErrorCode             */
    uint32_t last_io_hal_context;       /* failure snapshot hsd.Context               */
    uint32_t io_read_fail_count;        /* low-level read failure count               */
    uint32_t io_write_fail_count;       /* low-level write failure count              */

    /* ------------------------------ */
    /*  HAL / 移대뱶 ?뺣낫                 */
    /* ------------------------------ */
    uint32_t hal_error_code;            /* hsd.ErrorCode snapshot                    */
    uint32_t hal_context;               /* hsd.Context snapshot                      */
    uint32_t rel_card_add;              /* RCA                                       */
    uint32_t block_nbr;                 /* physical block count                      */
    uint32_t block_size;                /* physical block size                       */
    uint32_t log_block_nbr;             /* logical block count                       */
    uint32_t log_block_size;            /* logical block size                        */

    /* ------------------------------ */
    /*  FAT 硫뷀??곗씠??                 */
    /* ------------------------------ */
    uint32_t sectors_per_cluster;       /* cluster ??sector ??                     */
    uint32_t total_clusters;            /* ?곗씠???곸뿭 珥?cluster ??                */
    uint32_t free_clusters;             /* ?꾩옱 free cluster ??                     */
    uint32_t sectors_per_fat;           /* FAT ?섎굹??sector ??                     */
    uint32_t volume_start_sector;       /* volume ?쒖옉 sector                        */
    uint32_t fat_start_sector;          /* FAT ?쒖옉 sector                           */
    uint32_t root_dir_base;             /* root dir base(raw: FAT32硫?start cluster) */
    uint32_t data_start_sector;         /* data area ?쒖옉 sector                     */

    /* ------------------------------ */
    /*  root directory ?붿빟            */
    /* ------------------------------ */
    uint32_t root_entry_count;          /* root ?꾩껜 ?뷀듃由???                      */
    uint32_t root_file_count;           /* root ?꾩껜 ?뚯씪 ??                        */
    uint32_t root_dir_count;            /* root ?꾩껜 ?붾젆?곕━ ??                    */

    /* ------------------------------ */
    /*  byte ?⑥쐞 ?⑸웾 ?뺣낫             */
    /* ------------------------------ */
    uint64_t capacity_bytes;            /* 移대뱶 ?쇰━ ?⑸웾(byte)                      */
    uint64_t total_bytes;               /* FAT volume 珥??ъ슜 媛??byte              */
    uint64_t free_bytes;                /* FAT volume ?꾩옱 free byte                 */

    /* ------------------------------ */
    /*  root ?뷀듃由??섑뵆 ?대쫫           */
    /*                                                                            */
    /*  LFN??爰쇱졇 ?덉쑝誘濡??꾩옱 ?꾨줈?앺듃?먯꽌??                                  */
    /*  short 8.3 ?대쫫留???ν븳??                                               */
    /* ------------------------------ */
    char     root_entry_sample_name[3][14];
} app_sd_state_t;

/* -------------------------------------------------------------------------- */
/*  CDS 諛앷린 ?쇱꽌 / ADC 怨듦컻 ?곹깭                                             */
/*                                                                            */
/*  二쇱쓽                                                                      */
/*  - CDS(LDR)??愿묐웾怨????씠 ?좏삎 愿怨꾧? ?꾨땲誘濡? ?ш린?쒕뒗 lux瑜?吏곸젒       */
/*    留뚮뱾吏 ?딄퀬 raw ADC / 蹂댁젙 counts / 0~100% ?뺢퇋??媛믪쓣 怨듦컻?쒕떎.        */
/*  - STM32F407 ADC???ㅻⅨ ?쇰? STM32 怨꾩뿴泥섎읆 HAL self calibration API媛     */
/*    ?놁쑝誘濡? offset / gain / 0% / 100% 湲곗??먯쓣 ?뚰봽?몄썾?대줈 ?곸슜?쒕떎.     */
/* -------------------------------------------------------------------------- */
typedef struct
{
    uint8_t  last_hal_status;           /* 留덉?留?HAL status raw                    */
    uint32_t read_fail_count;           /* ADC read ?ㅽ뙣 ?꾩쟻 ?잛닔                  */
    uint32_t hal_error_count;           /* HAL ?먮윭 ?꾩쟻 ?잛닔                       */
    uint32_t last_read_start_ms;        /* 留덉?留??쎄린 ?쒖옉 ?쒓컖                    */
    uint32_t last_read_complete_ms;     /* 留덉?留??쎄린 ?꾨즺 ?쒓컖                    */

    uint16_t average_count;             /* ??踰덉쓽 寃곌낵瑜?留뚮뱾 ???됯퇏?섎뒗 ?섑뵆 ??  */
    uint16_t adc_timeout_ms;            /* PollForConversion timeout                */
    uint32_t period_ms;                 /* 二쇨린 痢≪젙 媛꾧꺽                           */
    uint32_t sampling_time;             /* ADC sampling time raw enum 媛?           */

    int32_t  calibration_offset_counts; /* raw ??癒쇱? ?뷀븯??offset                */
    uint32_t calibration_gain_num;      /* gain numerator                           */
    uint32_t calibration_gain_den;      /* gain denominator                         */
    uint16_t calibration_raw_0_percent; /* 0% 湲곗? raw count                        */
    uint16_t calibration_raw_100_percent; /* 100% 湲곗? raw count                    */
} app_brightness_debug_t;

typedef struct
{
    bool     initialized;               /* ?쒕씪?대쾭 init ?몄텧 ?щ?                   */
    bool     valid;                     /* ?곸뼱????踰??뺤긽 ?섑뵆???ㅼ뼱?붾뒗媛       */
    uint32_t last_update_ms;            /* 留덉?留??뺤긽 媛깆떊 ?쒓컖                    */
    uint32_t sample_count;              /* ?꾩쟻 ?뺤긽 ?섑뵆 ?잛닔                      */

    uint16_t raw_last;                  /* 留덉?留??⑥씪 raw ?섑뵆                     */
    uint16_t raw_average;               /* ?대쾲 burst ?됯퇏 raw                      */
    uint16_t raw_min;                   /* ?대쾲 burst 理쒖냼 raw                      */
    uint16_t raw_max;                   /* ?대쾲 burst 理쒕? raw                      */

    uint16_t calibrated_counts;         /* offset/gain ?곸슜 ??count                */
    uint16_t normalized_permille;       /* 0~1000 ?뺢퇋??寃곌낵                       */
    uint8_t  brightness_percent;        /* 0~100 ?뺢퇋??寃곌낵                        */
    uint32_t voltage_mv;                /* calibrated count瑜?mV濡??섏궛??媛?       */

    app_brightness_debug_t debug;       /* 諛앷린 ?쇱꽌 ?붾쾭洹???μ냼                  */
} app_brightness_state_t;


/* -------------------------------------------------------------------------- */
/*  Audio / DAC / DMA 怨듦컻 ?곹깭                                                */
/*                                                                            */
/*  ??援ъ“泥대뒗                                                               */
/*    - DAC transport媛 ?꾩옱 ?댁븘 ?덈뒗媛                                       */
/*    - ?ㅼ젣濡?tone / sequence / WAV content媛 ?ъ깮 以묒씤媛                    */
/*    - sample rate / DMA buffer ?ш린 / half/full callback count              */
/*    - voice蹂?phase / freq / envelope raw ?곹깭                              */
/*  瑜?APP_STATE ?덉뿉 洹몃?濡??곸옱?섍린 ?꾪븳 "??섏? 李쎄퀬" ??                  */
/*                                                                            */
/*  利? UI ?섏씠吏???곸쐞 ??濡쒖쭅?                                             */
/*  driver ?대? static ?곹깭瑜?吏곸젒 蹂댁? ?딄퀬                                   */
/*  諛섎뱶??APP_STATE.audio snapshot留?蹂대룄濡??좎??쒕떎.                         */
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
    bool     active;                    /* ??voice slot???꾩옱 ?ъ깮 以묒씤媛         */
    uint8_t  waveform_id;               /* app_audio_waveform_t raw                 */
    uint8_t  timbre_id;                 /* Audio_Presets 怨꾩뿴 timbre ID raw         */
    uint8_t  track_index;               /* 4梨꾨꼸 sequence?먯꽌 紐?踰덉㎏ track?멸?     */
    uint8_t  env_phase;                 /* app_audio_env_phase_t raw                */

    uint32_t note_hz_x100;              /* ?꾩옱 voice??紐⑺몴 二쇳뙆?? Hz x100        */
    uint32_t phase_q32;                 /* ?꾩옱 oscillator phase raw                */
    uint32_t phase_inc_q32;             /* sample 1媛쒕떦 phase 利앷???               */

    uint32_t note_samples_total;        /* ?꾩옱 note ?꾩껜 湲몄씠(sample)              */
    uint32_t note_samples_elapsed;      /* ?꾩옱 note媛 紐?sample 吏꾪뻾?먮뒗媛         */
    uint32_t gate_samples;              /* release濡??ㅼ뼱媛湲???gate 湲몄씠          */

    uint16_t env_level_q15;             /* ?꾩옱 envelope level                      */
    uint16_t velocity_q15;              /* ?꾩옱 note velocity                       */
} app_audio_voice_state_t;

typedef struct
{
    bool     initialized;               /* Audio_Driver_Init ?깃났 ?щ?              */
    bool     transport_running;         /* TIM6 + DAC DMA transport ?숈옉 ?щ?       */
    bool     content_active;            /* source ?먮뒗 FIFO tail ?ы븿 busy ?щ?     */
    bool     wav_active;                /* WAV file streaming 寃쎈줈 active ?щ?      */

    uint8_t  mode;                      /* app_audio_mode_t raw                     */
    uint8_t  active_voice_count;        /* ?꾩옱 active voice ??                    */
    uint8_t  last_hal_status_dac;       /* 留덉?留?DAC HAL status                    */
    uint8_t  last_hal_status_tim;       /* 留덉?留?TIM HAL status                    */

    uint8_t  output_resolution_bits;    /* ?ㅼ젣 DAC ?꾨궇濡쒓렇 異쒕젰 遺꾪빐??            */
    uint8_t  volume_percent;            /* 泥닿컧???좏삎??紐⑺몴濡???0~100 蹂쇰ⅷ        */
    uint8_t  last_block_clipped;        /* 吏곸쟾 render block?먯꽌 soft clip ?щ?     */
    uint8_t  wav_native_rate_active;    /* WAV媛 DAC native rate濡??ъ깮 以묒씤媛      */

    uint32_t sample_rate_hz;            /* ?꾩옱 DAC sample rate                     */
    uint16_t dma_buffer_sample_count;   /* circular DMA ?꾩껜 sample ??             */
    uint16_t dma_half_buffer_sample_count; /* half buffer sample ??                 */
    uint16_t last_block_min_u12;        /* 吏곸쟾 render block??DAC 理쒖냼媛?          */
    uint16_t last_block_max_u12;        /* 吏곸쟾 render block??DAC 理쒕?媛?          */

    /* ---------------------------------------------------------------------- */
    /*  software FIFO telemetry                                                */
    /*                                                                        */
    /*  ??媛믩뱾? "main producer" ? "DMA ISR consumer" ?ъ씠???꾩땐吏?瑜?    */
    /*  ?쇰쭏?????좎??섍퀬 ?덈뒗吏 蹂댁뿬 二쇰뒗 raw 吏꾨떒媛믪씠??                    */
    /* ---------------------------------------------------------------------- */
    uint32_t sw_fifo_capacity_samples;       /* FIFO ?꾩껜 ?⑸웾(sample)            */
    uint32_t sw_fifo_level_samples;          /* ?꾩옱 FIFO ?섏쐞(sample)            */
    uint32_t sw_fifo_peak_level_samples;     /* 遺???댄썑 愿痢〓맂 理쒕? FIFO ?섏쐞   */
    uint32_t sw_fifo_low_watermark_samples;  /* producer refill low watermark     */
    uint32_t sw_fifo_high_watermark_samples; /* producer refill high watermark    */

    uint32_t last_update_ms;            /* 留덉?留?block render ?꾨즺 ?쒓컖            */
    uint32_t playback_start_ms;         /* ?꾩옱/吏곸쟾 ?ъ깮 ?쒖옉 ?쒓컖                 */
    uint32_t playback_stop_ms;          /* 留덉?留??ъ깮 ?뺤? ?쒓컖                    */

    uint32_t half_callback_count;       /* DMA half complete callback ?꾩쟻 ??      */
    uint32_t full_callback_count;       /* DMA full complete callback ?꾩쟻 ??      */
    uint32_t dma_underrun_count;        /* DAC DMA underrun ?꾩쟻 ??                */
    uint32_t render_block_count;        /* main context?먯꽌 block render???잛닔     */
    uint32_t clip_block_count;          /* clip/soft clip??諛쒖깮??block ??        */
    uint32_t transport_reconfig_count;  /* sample rate 蹂寃쎌쑝濡?transport瑜??ъ떆?묓븳 ?잛닔 */

    uint32_t producer_refill_block_count;   /* main producer媛 FIFO???곸옱??block ??*/
    uint32_t dma_service_half_count;        /* ISR媛 DMA half-buffer瑜??쒕퉬?ㅽ븳 ?잛닔 */
    uint32_t fifo_starvation_count;         /* source???댁븘 ?덈뒗??FIFO媛 鍮??잛닔   */
    uint32_t silence_injected_sample_count; /* starvation 蹂댁젙 silence sample ??    */

    uint32_t sequence_bpm;              /* ?꾩옱 sequence BPM                        */
    uint32_t wav_source_sample_rate_hz; /* WAV ?먮낯 sample rate                     */
    uint32_t wav_source_data_bytes_remaining; /* ?꾩쭅 ?⑥? WAV data bytes           */
    uint16_t wav_source_channels;       /* WAV ?먮낯 channel ??                     */
    uint16_t wav_source_bits_per_sample; /* WAV ?먮낯 bit depth                      */

    char     current_name[APP_AUDIO_NAME_MAX];  /* ?꾩옱/吏곸쟾 content ?대쫫            */
    char     last_wav_path[APP_AUDIO_NAME_MAX]; /* 留덉?留?WAV 寃쎈줈 異뺤빟 ??μ냼      */

    app_audio_voice_state_t voices[APP_AUDIO_MAX_VOICES];
} app_audio_state_t;



/* -------------------------------------------------------------------------- */
/*  Bluetooth / 臾댁꽑 ?쒕━??怨듦컻 ?곹깭                                          */
/*                                                                            */
/*  ?꾩옱 ?꾨줈?앺듃??Bluetooth 紐⑤뱢?                                           */
/*  "Classic Bluetooth SPP濡?UART瑜?臾댁꽑?뷀븯??transparent serial adapter"     */
/*  泥섎읆 ?ㅻ，??                                                               */
/*                                                                            */
/*  利? ???낆옣?먯꽌??                                                         */
/*    - TX: 臾몄옄??諛붿씠?몃? UART泥섎읆 蹂대깂                                      */
/*    - RX: ?곷? ?⑤쭚??蹂대궦 臾몄옄??諛붿씠?몃? line ?⑥쐞濡?愿李?                 */
/*  ?섎㈃ ?쒕떎.                                                                 */
/*                                                                            */
/*  ?ш린?먮뒗 ?щ엺???붾쾭洹??붾㈃?먯꽌 蹂닿퀬 ?띠뼱 ??媛믩뱾??紐⑥???               */
/*  ?? 理쒓렐 RX/TX 以? 諛붿씠??移댁슫?? echo/auto ping ?곹깭, RX ring ?섏쐞 ??    */
/* -------------------------------------------------------------------------- */
#ifndef APP_BLUETOOTH_LAST_TEXT_MAX
#define APP_BLUETOOTH_LAST_TEXT_MAX 96u
#endif

typedef struct
{
    bool     initialized;               /* Bluetooth driver init ?몄텧 ?щ?          */
    bool     uart_rx_running;           /* HAL_UART_Receive_IT ?섏떊 寃쎈줈 ?숈옉 ?щ?  */
    bool     echo_enabled;              /* ?섏떊 以꾩쓣 ?ㅼ떆 ?뚮젮蹂대궡??echo 湲곕뒫      */
    bool     auto_ping_enabled;         /* 1珥?二쇨린 auto ping 湲곕뒫                  */

    uint32_t last_update_ms;            /* RX ?먮뒗 TX濡?留덉?留??곹깭 媛깆떊 ?쒓컖       */
    uint32_t last_rx_ms;                /* 留덉?留?raw byte ?섏떊 ?쒓컖                */
    uint32_t last_tx_ms;                /* 留덉?留?raw byte ?≪떊 ?꾨즺 ?쒓컖           */
    uint32_t last_auto_ping_ms;         /* 留덉?留?auto ping ?≪떊 ?쒓컖               */

    uint32_t rx_bytes;                  /* ?꾩쟻 ?섏떊 諛붿씠????                     */
    uint32_t tx_bytes;                  /* ?꾩쟻 ?≪떊 諛붿씠????                     */
    uint32_t rx_line_count;             /* CR/LF 湲곗? ?뺤젙??RX 以???              */
    uint32_t tx_line_count;             /* ?≪떊 helper濡?蹂대궦 ?쇰━??以???         */
    uint32_t rx_overflow_count;         /* ?뚰봽?몄썾??line/ring overflow ?잛닔       */
    uint32_t uart_error_count;          /* HAL UART error callback ?꾩쟻 ?잛닔        */
    uint32_t uart_rearm_fail_count;     /* Receive_IT ?촡rm ?ㅽ뙣 ?잛닔               */
    uint32_t uart_tx_fail_count;        /* HAL_UART_Transmit ?ㅽ뙣 ?잛닔              */

    uint16_t rx_ring_level;             /* ?꾩옱 RX ring???볦씤 諛붿씠????           */
    uint16_t rx_ring_high_watermark;    /* 遺???댄썑 RX ring 理쒕? ?곸옱移?           */

    uint8_t  last_hal_status_rx;        /* 留덉?留?RX 愿??HAL status                */
    uint8_t  last_hal_status_tx;        /* 留덉?留?TX 愿??HAL status                */
    uint8_t  last_hal_error;            /* 留덉?留?HAL UART error raw                */
    uint8_t  reserved0;                 /* ?뺣젹???덉빟                               */

    char     last_rx_line[APP_BLUETOOTH_LAST_TEXT_MAX];
    char     last_tx_line[APP_BLUETOOTH_LAST_TEXT_MAX];
    char     rx_preview[APP_BLUETOOTH_LAST_TEXT_MAX];
} app_bluetooth_state_t;

/* -------------------------------------------------------------------------- */
/*  DEBUG UART 怨듦컻 ?곹깭                                                       */
/*                                                                            */
/*  ??援ъ“泥대뒗 PA9/PA10(USART1) 媛숈? ?좎꽑 UART 濡쒓렇 ?ы듃瑜?                   */
/*  ?쇰쭏???쇰뒗吏, 留덉?留됱쑝濡??대뼡 臾몄옄?댁쓣 ?대낫?덈뒗吏 ?깆쓣                     */
/*  APP_STATE??湲곕줉?섍린 ?꾪븳 ??μ냼??                                        */
/* -------------------------------------------------------------------------- */


/* -------------------------------------------------------------------------- */
/*  ALTITUDE / VARIO 怨듦컻 ?곹깭                                                 */
/*                                                                            */
/*  ????μ냼??                                                             */
/*  - pressure altitude (STD 1013.25)                                         */
/*  - manual QNH altitude                                                     */
/*  - GPS hMSL                                                                */
/*  - GPS anchor瑜??ъ슜??fused absolute altitude(no-IMU / IMU)               */
/*  - relative/home altitude                                                  */
/*  - fast/slow variometer                                                    */
/*  - grade(寃쎌궗??                                                           */
/*  瑜?蹂묐젹濡?蹂닿??쒕떎.                                                       */
/*                                                                            */
/*  以묒슂                                                                      */
/*  - manual QNH 怨좊룄? GPS 湲곕컲 fused absolute altitude??                   */
/*    ?쒕줈 ?섎?媛 ?ㅻⅤ誘濡??????곕줈 ?④릿??                                 */
/*  - IMU-aided 寃곌낵? no-IMU 寃곌낵???숈떆???좎??댁꽌                          */
/*    ?쒕떇/寃利?濡쒓렇 鍮꾧탳媛 媛?ν븯寃?留뚮뱺??                                   */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*  ALTITUDE low-level unit bank helper types                                  */
/*                                                                            */
/*  canonical / derived ownership                                             */
/*  - *_cm / *_cms / *_hpa_x100 ?꾨뱶媛 source-of-truth ?대떎.                  */
/*  - ?꾨옒 bank??洹?canonical metric 媛믪쓣 嫄대뱶由ъ? ?딄퀬,                      */
/*    ?숈씪???쒓컙??臾쇰━?됱쓣 ?щ윭 ?⑥쐞怨꾨줈 蹂묐젹 蹂닿??섎뒗 "?뚯깮 ?쒗쁽痢? ?대떎.   */
/*                                                                            */
/*  ?ㅺ퀎 ?섎룄                                                                  */
/*  - ?곸쐞 UI / App layer??meter 媛믪쓣 ?ㅼ떆 feet濡??섏궛?섏? ?딄퀬              */
/*    ??bank?먯꽌 ?꾩슂???⑥쐞 ?щ’留??좏깮?쒕떎.                                */
/*  - ?뱁엳 feet???대? 1m濡??묒옄?붾맂 媛믪쓣 ?ㅼ떆 諛붽씔 寃껋씠 ?꾨땲??               */
/*    canonical centimeter source?먯꽌 吏곸젒 怨꾩궛?쒕떎.                          */
/*    ?곕씪??feet ?댁긽?꾧? meter ?쒖떆 ?댁긽?꾩뿉 ?뚮젮 ?대젮媛吏 ?딅뒗??          */
/* -------------------------------------------------------------------------- */
typedef struct
{
    /* ---------------------------------------------------------------------- */
    /*  ?좏삎 altitude quantity ?쒗쁽??                                         */
    /*                                                                        */
    /*  meters_rounded                                                         */
    /*  - centimeter canonical source瑜?媛??媛源뚯슫 1 m 濡?諛섏삱由쇳븳 媛?       */
    /*                                                                        */
    /*  feet_rounded                                                           */
    /*  - ?숈씪 source瑜?feet濡?吏곸젒 蹂?섑븳 ??媛??媛源뚯슫 1 ft 濡?諛섏삱由?      */
    /*  - meters_rounded 瑜??ㅼ떆 3.28084諛???媛믪씠 ?꾨땲??                    */
    /* ---------------------------------------------------------------------- */
    int32_t meters_rounded;
    int32_t feet_rounded;
} app_altitude_linear_units_t;

typedef struct
{
    /* ---------------------------------------------------------------------- */
    /*  vertical-speed ?쒗쁽??                                                 */
    /*                                                                        */
    /*  mps_x10_rounded                                                        */
    /*  - 0.1 m/s 怨좎젙?뚯닔???쒗쁽                                              */
    /*                                                                        */
    /*  fpm_rounded                                                            */
    /*  - feet per minute ?뺤닔 諛섏삱由??쒗쁽                                     */
    /* ---------------------------------------------------------------------- */
    int32_t mps_x10_rounded;
    int32_t fpm_rounded;
} app_altitude_vspeed_units_t;

typedef struct
{
    /* ---------------------------------------------------------------------- */
    /*  pressure ?쒗쁽??                                                       */
    /*                                                                        */
    /*  hpa_x100                                                               */
    /*  - canonical 洹몃?濡??좎??섎뒗 0.01 hPa 怨좎젙?뚯닔??                       */
    /*                                                                        */
    /*  inhg_x1000                                                             */
    /*  - inch of mercury, 0.001 inHg 怨좎젙?뚯닔??                              */
    /* ---------------------------------------------------------------------- */
    int32_t hpa_x100;
    int32_t inhg_x1000;
} app_altitude_pressure_units_t;

typedef struct
{
    /* ---------------------------------------------------------------------- */
    /*  pressure / qnh quantity bank                                           */
    /*                                                                        */
    /*  ?대쫫? ?꾨옒 canonical ?꾨뱶紐낃낵 1:1 ??묐릺?꾨줉 ?좎??쒕떎.                */
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
    /*  怨듦컻 ?곹깭 ?뚮옒洹?/ ?덉쭏 ?뺣낫                                           */
    /* ---------------------------------------------------------------------- */
    bool     initialized;                   /* ?쒕퉬??init ?꾨즺 ?щ?                 */
    bool     baro_valid;                    /* baro 寃쎈줈媛 ?꾩옱 ?댁븘 ?덈뒗媛          */
    bool     gps_valid;                     /* GPS height gate ?듦낵 ?щ?             */
    bool     home_valid;                    /* home altitude媛 罹≪쿂?섏뿀?붽?          */
    bool     imu_vector_valid;              /* gravity estimator媛 ?좏슚?쒓?          */
    uint8_t  debug_audio_active;            /* ALTITUDE debug audio ?쒖꽦 ?щ?        */
    uint16_t gps_quality_permille;          /* GPS quality 0..1000                   */

    /* ---------------------------------------------------------------------- */
    /*  理쒓렐 update timestamp                                                  */
    /* ---------------------------------------------------------------------- */
    uint32_t last_update_ms;                /* 留덉?留?task ?ㅽ뻾 ?쒓컖                 */
    uint32_t last_baro_update_ms;           /* 留덉?留??좉퇋 baro sample 諛섏쁺 ?쒓컖     */
    uint32_t last_gps_update_ms;            /* 留덉?留??좉퇋 GPS sample 諛섏쁺 ?쒓컖      */

    /* ---------------------------------------------------------------------- */
    /*  Pressure / QNH intermediate 怨듦컻媛?                                    */
    /* ---------------------------------------------------------------------- */
    int32_t  pressure_raw_hpa_x100;         /* raw pressure, 0.01 hPa                */
    int32_t  pressure_prefilt_hpa_x100;     /* median-3 prefilter pressure           */
    int32_t  pressure_filt_hpa_x100;        /* LPF pressure                          */
    int32_t  pressure_residual_hpa_x100;    /* prefilt - lpf residual                */
    int32_t  qnh_manual_hpa_x100;           /* manual QNH                            */
    int32_t  qnh_equiv_gps_hpa_x100;        /* GPS equivalent QNH                    */

    /* ---------------------------------------------------------------------- */
    /*  蹂묐젹 怨좊룄媛?                                                           */
    /* ---------------------------------------------------------------------- */
    int32_t  alt_pressure_std_cm;           /* STD 1013.25 pressure altitude         */
    int32_t  alt_qnh_manual_cm;             /* manual QNH altitude                   */
    int32_t  alt_gps_hmsl_cm;               /* GPS hMSL                              */
    int32_t  alt_fused_noimu_cm;            /* 3-state KF fused altitude             */
    int32_t  alt_fused_imu_cm;              /* 4-state IMU-aided fused altitude      */
    int32_t  alt_display_cm;                /* 理쒖쥌 UI/log 二??쒖떆??altitude        */

    /* ---------------------------------------------------------------------- */
    /*  home / relative altitude                                               */
    /* ---------------------------------------------------------------------- */
    int32_t  alt_rel_home_noimu_cm;         /* no-IMU ?곷?怨좊룄                       */
    int32_t  alt_rel_home_imu_cm;           /* IMU ?곷?怨좊룄                          */
    int32_t  home_alt_noimu_cm;             /* no-IMU home absolute altitude         */
    int32_t  home_alt_imu_cm;               /* IMU home absolute altitude            */

    /* ---------------------------------------------------------------------- */
    /*  filter bias / noise / display mode                                     */
    /* ---------------------------------------------------------------------- */
    int32_t  baro_bias_noimu_cm;            /* no-IMU filter??baro bias             */
    int32_t  baro_bias_imu_cm;              /* IMU filter??baro bias                */
    uint16_t baro_noise_used_cm;            /* ?대쾲 step?먯꽌 ?ъ슜??altitude R       */
    uint8_t  display_rest_active;           /* rest display stabilizer ?쒖꽦          */
    uint8_t  zupt_active;                   /* zero-velocity pseudo update ?쒖꽦      */
    uint8_t  debug_audio_source;            /* ?꾩옱 audio source 0/1                 */
    uint8_t  reserved_audio0;               /* alignment / future use                */
    int32_t  debug_audio_vario_cms;         /* ?ㅼ젣 tone source vario                */

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
    int32_t  imu_vertical_accel_mg;         /* gravity ?쒓굅 ???섏쭅 specific-force   */
    int32_t  imu_vertical_accel_cms2;       /* ??媛믪쓣 cm/s^2 濡?蹂?섑븳 媛?          */
    int32_t  imu_gravity_norm_mg;           /* gravity vector norm diagnostic        */
    int32_t  imu_accel_norm_mg;             /* raw accel norm diagnostic             */
    uint16_t imu_attitude_trust_permille;   /* attitude trust 0..1000                */
    uint16_t imu_predict_weight_permille;   /* KF4 predict weight 0..1000            */

    /* ---------------------------------------------------------------------- */
    /*  理쒓렐 ?ъ슜??GPS ?덉쭏 ?섏튂                                              */
    /* ---------------------------------------------------------------------- */
    uint32_t gps_vacc_mm;                   /* GPS vertical accuracy                 */
    uint16_t gps_pdop_x100;                 /* GPS PDOP x100                         */
    uint8_t  gps_numsv_used;                /* GPS numSV_used                        */
    uint8_t  gps_fix_type;                  /* GPS fixType                           */

    /* ---------------------------------------------------------------------- */
    /*  ??섏? ?⑥쐞 bank                                                       */
    /*                                                                        */
    /*  以묒슂??洹쒖튃                                                            */
    /*  - ??canonical metric ?꾨뱶媛 癒쇱? 媛깆떊?섍퀬                             */
    /*  - ?꾨옒 units bank??洹?canonical 媛믪쓣 湲곕컲?쇰줈 媛숈? task ?덉뿉??       */
    /*    利됱떆 ?뚯깮 怨꾩궛?쒕떎.                                                  */
    /*                                                                        */
    /*  ?곕씪???곸쐞 怨꾩링?                                                     */
    /*  - "meter 媛믪쓣 諛쏆븘???ㅼ떆 feet濡??섏궛" ?섏? 留먭퀬                      */
    /*  - ?꾩슂???щ’留??좏깮???쎌뼱???쒕떎.                                    */
    /* ---------------------------------------------------------------------- */
    app_altitude_unit_bank_t units;
} app_altitude_state_t;


/* -------------------------------------------------------------------------- */
/*  BIKE DYNAMICS 怨듦컻 ?곹깭                                                     */
/*                                                                            */
/*  sign 洹쒖튃                                                                   */
/*  - banking angle : + = left lean,  - = right lean                           */
/*  - grade         : + = nose up,    - = nose down                            */
/*  - lat accel     : + = left,       - = right                                */
/*  - lon accel     : + = accel,      - = braking                              */
/*                                                                            */
/*  input / output 遺꾨━                                                         */
/*  - ?꾨옒 援ъ“泥댁쓽 ?遺遺꾩? BIKE_DYNAMICS.c媛 媛깆떊?섎뒗 異쒕젰 ?꾨뱶??            */
/*  - ?? obd_input_* ?꾨뱶??異뷀썑 OBD service媛 app_state瑜??듯빐 ???ｋ뒗        */
/*    ?낅젰 ?꾨뱶?? BIKE_DYNAMICS????媛믪쓣 ?쎄린留??섍퀬, 由ъ뀑 ?몄뿉??            */
/*    ??뼱?곗? ?딅뒗??                                                          */
/* -------------------------------------------------------------------------- */
typedef struct
{
    /* ---------------------------------------------------------------------- */
    /*  怨듦컻 ?뚮옒洹?/ mode                                                     */
    /* ---------------------------------------------------------------------- */
    bool     initialized;               /* ?쒕퉬??init ?꾨즺 ?щ?                  */
    bool     zero_valid;                /* reset 湲곗? basis媛 ?좏슚?쒓?            */
    bool     imu_valid;                 /* gravity observer媛 ?좏슚?쒓?            */
    bool     gnss_aid_valid;            /* GNSS speed ?먮뒗 heading aid ?좏슚?쒓?   */
    bool     gnss_heading_valid;        /* GNSS heading aid媛 ?꾩옱 ?좏슚?쒓?       */
    bool     obd_speed_valid;           /* future OBD speed媛 ?꾩옱 ?좏슚?쒓?       */
    uint8_t  speed_source;              /* app_bike_speed_source_t raw            */
    uint8_t  estimator_mode;            /* app_bike_estimator_mode_t raw          */
    uint16_t confidence_permille;       /* 0..1000                                */

    /* ---------------------------------------------------------------------- */
    /*  理쒓렐 update timestamp / ?ъ슜??紐낅졊 ?꾩쟻 ?잛닔                           */
    /* ---------------------------------------------------------------------- */
    uint32_t last_update_ms;
    uint32_t last_imu_update_ms;
    uint32_t last_zero_capture_ms;
    uint32_t last_gnss_aid_ms;
    uint32_t zero_request_count;        /* ResetBankingAngleSensor ?몄텧 ?꾩쟻       */
    uint32_t hard_rezero_count;         /* hard rezero ?붿껌 ?꾩쟻                  */

    /* ---------------------------------------------------------------------- */
    /*  理쒖쥌 ?쒖떆 ?꾨낫                                                          */
    /* ---------------------------------------------------------------------- */
    int16_t  banking_angle_deg_x10;     /* 0.1 deg                                */
    int16_t  banking_angle_display_deg; /* 1 deg ?쒖떆媛?                          */
    int16_t  grade_deg_x10;             /* 0.1 deg                                */
    int16_t  grade_display_deg;         /* 1 deg ?쒖떆媛?                          */
    int16_t  bank_rate_dps_x10;         /* 0.1 dps                                */
    int16_t  grade_rate_dps_x10;        /* 0.1 dps                                */

    /* ---------------------------------------------------------------------- */
    /*  canonical estimator output                                             */
    /*                                                                        */
    /*  bank_raw_deg_x10 / grade_raw_deg_x10                                  */
    /*  - display smoothing(lean/grade tau) ?곸슜 ?꾩쓽 ?먯꽭 異붿젙媛?            */
    /*  - logger / peak detector / offline analyzer ????媛믪쓣 湲곗??쇰줈        */
    /*    ?쎌뼱???쒕떎.                                                         */
    /*                                                                        */
    /*  lat_accel_est_mg / lon_accel_est_mg                                   */
    /*  - bias ?쒓굅 + deadband/clip ?곸슜 ?? display tau ?곸슜 ??fused accel   */
    /*  - 利? rider-facing UI 媛?lat_accel_mg/lon_accel_mg)蹂대떎               */
    /*    ?묐떟??鍮좊Ⅸ canonical telemetry 痢듭씠??                              */
    /* ---------------------------------------------------------------------- */
    int16_t  bank_raw_deg_x10;          /* 0.1 deg, pre-display smoothing         */
    int16_t  grade_raw_deg_x10;         /* 0.1 deg, pre-display smoothing         */
    int32_t  lat_accel_est_mg;          /* mg, pre-display smoothing              */
    int32_t  lon_accel_est_mg;          /* mg, pre-display smoothing              */

    int32_t  lat_accel_mg;              /* 理쒖쥌 rider-facing lateral accel, mg    */
    int32_t  lon_accel_mg;              /* 理쒖쥌 rider-facing accel/decel, mg      */
    int32_t  lat_accel_cms2;            /* cm/s^2                                 */
    int32_t  lon_accel_cms2;            /* cm/s^2                                 */

    /* ---------------------------------------------------------------------- */
    /*  鍮꾧탳/?쒕떇??intermediate                                                */
    /* ---------------------------------------------------------------------- */
    int32_t  lat_accel_imu_mg;          /* IMU-only level lateral                 */
    int32_t  lon_accel_imu_mg;          /* IMU-only level longitudinal            */
    int32_t  lat_accel_ref_mg;          /* GNSS heading 湲곕컲 lateral reference    */
    int32_t  lon_accel_ref_mg;          /* GNSS/OBD speed derivative reference    */
    int32_t  lat_bias_mg;               /* external ref濡??곸쓳??lateral bias     */
    int32_t  lon_bias_mg;               /* external ref濡??곸쓳??longitudinal bias */

    /* ---------------------------------------------------------------------- */
    /*  IMU / attitude diagnostic                                               */
    /* ---------------------------------------------------------------------- */
    int32_t  imu_accel_norm_mg;         /* raw accel norm                         */
    int32_t  imu_jerk_mg_per_s;         /* rough-road / 異⑷꺽 吏꾨떒??jerk          */
    uint16_t imu_attitude_trust_permille; /* gravity observer trust 0..1000      */
    int32_t  up_bx_milli;               /* current up dot bike_fwd, x1000         */
    int32_t  up_by_milli;               /* current up dot bike_left, x1000        */
    int32_t  up_bz_milli;               /* current up dot bike_up, x1000          */

    /* ---------------------------------------------------------------------- */
    /*  speed / GNSS quality                                                    */
    /* ---------------------------------------------------------------------- */
    int32_t  speed_mmps;                /* ?꾩옱 ?좏깮??speed source 媛?           */
    uint16_t speed_kmh_x10;             /* 0.1 km/h                               */
    uint16_t gnss_speed_acc_kmh_x10;    /* GNSS speed accuracy diagnostic         */
    uint16_t gnss_head_acc_deg_x10;     /* GNSS heading accuracy diagnostic       */
    int16_t  mount_yaw_trim_deg_x10;    /* ?꾩옱 ?곸슜 以?yaw trim                  */

    uint8_t  gnss_fix_ok;
        uint8_t  gnss_numsv_used;
        uint16_t gnss_pdop_x100;


        /* ---------------------------------------------------------------------- */
        /*  heading diagnostic                                                     */
        /*                                                                        */
        /*  heading_valid                                                          */
        /*  - ?꾩옱 怨듦컻 以묒씤 heading 媛믪씠 ?좏슚?쒓?                                  */
        /*                                                                        */
        /*  mag_heading_valid                                                      */
        /*  - tilt-compensated magnetic heading 媛믪씠 ?좏슚?쒓?                       */
        /*                                                                        */
        /*  heading_source                                                         */
        /*  - app_bike_heading_source_t raw                                         */
        /*                                                                        */
        /*  heading_deg_x10                                                        */
        /*  - ?꾩옱 怨듦컻 以묒씤 heading 媛? 0.1 deg                                    */
        /*                                                                        */
        /*  mag_heading_deg_x10                                                    */
        /*  - tilt-compensated magnetic heading raw, 0.1 deg                        */
        /*                                                                        */
        /*  二쇱쓽                                                                   */
        /*  - ??heading 異쒕젰? lean / grade / lateral G 怨꾩궛?앹뿉 ?쇰뱶諛깊븯吏 ?딅뒗??*/
        /*  - 利? 6異?Mahony ?먯꽭 異붿젙? 洹몃?濡??좎??섍퀬, heading? 蹂댁“ 吏꾨떒?⑹씠??*/
        /* ---------------------------------------------------------------------- */
        bool     heading_valid;
        bool     mag_heading_valid;
        uint8_t  heading_source;
        uint8_t  reserved_heading0;
        int16_t  heading_deg_x10;
        int16_t  mag_heading_deg_x10;



        /* ---------------------------------------------------------------------- */
        /*  Gyro bias calibration 怨듦컻 ?곹깭                                         */
        /*                                                                        */
        /*  gyro_bias_cal_active                                                   */
        /*  - ?꾩옱 gyro bias calibration??吏꾪뻾 以묒씤媛                             */
        /*                                                                        */
        /*  gyro_bias_valid                                                        */
        /*  - bias 媛믪씠 ??踰덉씠?쇰룄 ?깃났?곸쑝濡?痢≪젙?섏뿀?붽?                         */
        /*                                                                        */
        /*  gyro_bias_cal_last_success                                             */
        /*  - 媛??理쒓렐 calibration ?붿껌???깃났?덈뒗媛                              */
        /*                                                                        */
        /*  gyro_bias_cal_progress_permille                                        */
        /*  - calibration good-sample ?꾩쟻 吏꾪뻾瑜? 0..1000                         */
        /*                                                                        */
        /*  last_gyro_bias_cal_ms / gyro_bias_cal_count                            */
        /*  - 留덉?留?醫낅즺 ?쒓컖 / ?깃났 ?꾩쟻 ?잛닔                                    */
        /*                                                                        */
        /*  gyro_bias_*_dps_x100                                                   */
        /*  - ?ъ슜?먯뿉寃?蹂댁뿬 二쇨린 ?꾪븳 bias 媛? 0.01 dps 怨좎젙?뚯닔??              */
        /*                                                                        */
        /*  yaw_rate_dps_x10                                                       */
        /*  - ?꾩옱 world-up 異?湲곗? yaw rate, 0.1 dps                              */
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
        /*  異뷀썑 OBD service媛 ?꾨옒 ???꾨뱶留?媛깆떊?섎㈃                              */
        /*  BIKE_DYNAMICS媛 ?먮룞?쇰줈 OBD speed source瑜??ъ슜?????덈떎.             */
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
    app_settings_t         settings;    /* 湲곌린 ?꾩껜 ?ㅼ젙 ?ㅻ깄??                    */
    app_gps_state_t        gps;         /* GPS ?쒕씪?대쾭???먮낯 ?곹깭 ??μ냼           */
    app_gy86_state_t       gy86;        /* GY-86 / IMU ?꾩껜 ?곹깭 ??μ냼              */
    app_ds18b20_state_t    ds18b20;     /* DS18B20 ?꾩껜 ?곹깭 ??μ냼                  */
    app_brightness_state_t brightness;  /* CDS 諛앷린 ?쇱꽌 ?곹깭 ??μ냼                 */
    app_audio_state_t      audio;       /* DAC / DMA / ?ㅻ뵒???붿쭊 ?곹깭 ??μ냼       */
    app_bluetooth_state_t  bluetooth;   /* Bluetooth bring-up / 臾댁꽑 ?쒕━????μ냼   */
    app_sd_state_t         sd;          /* SD / FATFS / hotplug 怨듦컻 ?곹깭 ??μ냼     */
    app_clock_state_t      clock;       /* RTC / timezone / GPS sync ?곹깭 ??μ냼     */
    app_altitude_state_t   altitude;    /* 怨좊룄 / 諛붾━??/ 寃쎌궗??怨듦컻 ?곹깭 ??μ냼   */
    app_bike_state_t       bike;        /* 紐⑦꽣?ъ씠??frame dynamics 怨듦컻 ?곹깭 ??μ냼*/

    /* ?ㅻⅨ ?쇱꽌/?쒕툕?쒖뒪?쒕룄 怨꾩냽 ?ш린??異붽??섎㈃ ?쒕떎. */
    /* ?? battery, storage, ui, logger ... */
} app_state_t;


/* -------------------------------------------------------------------------- */
/*  UI ?꾩슜 寃쎈웾 GPS ?ㅻ깄??                                                   */
/*                                                                            */
/*  ???곕줈 ?먮뒗媛?                                                             */
/*                                                                            */
/*  湲곗〈 app_gps_state_t ??"?쒕씪?대쾭 ?꾩껜 ?곹깭 ??μ냼"?쇱꽌                    */
/*  raw UBX payload / last frame / cfg payload / nav_sat_sv[] / sats[] ??    */
/*  UI媛 ?꾩슂 ?녿뒗 ???곗씠?곌퉴吏 ?꾨? ?ы븿?쒕떎.                                */
/*                                                                            */
/*  硫붿씤 猷⑦봽?먯꽌 留??꾨젅?꾨쭏??? ??援ъ“泥대? ?듭㎏濡?memcpy ?섎㈃               */
/*  IRQ off ?쒓컙??湲몄뼱吏怨? 洹??ъ씠 UART RX ?ㅻ쾭???꾪뿕??而ㅼ쭊??             */
/*                                                                            */
/*  洹몃옒??UI媛 ?ㅼ젣濡?洹몃━?????꾩슂???꾨뱶留?紐⑥?                           */
/*  app_gps_ui_snapshot_t 瑜??곕줈 留뚮뱺??                                      */
/* -------------------------------------------------------------------------- */
typedef struct
{
    /* ------------------------------ */
    /*  ?꾩튂/?띾룄/?뺥솗??湲곕낯 ?뺣낫     */
    /* ------------------------------ */
    gps_fix_basic_t fix;                    /* ?붾쾭洹??붾㈃??FIX/LAT/LON/SPD/ACC ?깆뿉 ?ъ슜 */

    /* ------------------------------ */
    /*  ?고????ㅼ젙 吏덉쓽 寃곌낵          */
    /* ------------------------------ */
    app_gps_runtime_config_t runtime_cfg;  /* RUN/GN/B...P...S... 以??쒖떆??*/

    /* ------------------------------ */
    /*  UART / parser 愿痢≪튂           */
    /* ------------------------------ */
    bool     uart_rx_running;              /* direct IRQ RX 寃쎈줈媛 ?꾩옱 ?숈옉 以묒씤吏 ?щ? */

    uint32_t rx_bytes;                     /* 珥??섏떊 諛붿씠????*/
    uint32_t frames_ok;                    /* checksum OK濡??듦낵??UBX frame ??*/
    uint32_t frames_bad_checksum;          /* checksum mismatch濡?踰꾨젮吏?frame ??*/
    uint32_t uart_ring_overflow_count;     /* ?뚰봽?몄썾??RX ring overflow ?꾩쟻 ??*/

    uint32_t uart_error_count;             /* UART ?섎뱶?⑥뼱 ?먮윭 珥앺빀 */
    uint32_t uart_error_ore_count;         /* Overrun Error ?꾩쟻 ??*/
    uint32_t uart_error_fe_count;          /* Framing Error ?꾩쟻 ??*/
    uint32_t uart_error_ne_count;          /* Noise Error ?꾩쟻 ??*/
    uint32_t uart_error_pe_count;          /* Parity Error ?꾩쟻 ??*/

    uint16_t rx_ring_level;                /* 吏湲?ring ?덉뿉 ?볦뿬 ?덈뒗 諛붿씠????*/
    uint16_t rx_ring_high_watermark;       /* 遺???댄썑 ring 理쒕? ?곸옱移?*/

    uint32_t last_rx_ms;                   /* 留덉?留?raw byte ?섏떊 tick */

    /* ------------------------------ */
    /*  ?ㅼ뭅???뚮’???곗씠??          */
    /* ------------------------------ */
    uint8_t       nav_sat_count;           /* sats[] 以??좏슚 ??ぉ ??*/
    app_gps_sat_t sats[APP_GPS_MAX_SATS];  /* ?ㅼ뭅???뚮’??諛붾줈 ?곕뒗 ?꾩꽦 諛곗뿴 */
} app_gps_ui_snapshot_t;


/* -------------------------------------------------------------------------- */
/*  Global state                                                               */
/* -------------------------------------------------------------------------- */

extern volatile app_state_t g_app_state;

void APP_STATE_Init(void);
void APP_STATE_ResetGps(void);
void APP_STATE_CopySnapshot(app_state_t *dst);

/* ?덇굅???꾩껜 GPS snapshot API.
 * ?ㅻⅨ 肄붾뱶媛 ?꾩쭅 ?곌퀬 ?덉쓣 ???덉쑝誘濡??④꺼?붾떎. */
void APP_STATE_CopyGpsSnapshot(app_gps_state_t *dst);

/* ??UI ?꾩슜 寃쎈웾 snapshot API.
 * 硫붿씤 GPS ?붾㈃/?붾쾭洹??붾㈃? ???⑥닔留??ъ슜?섎룄濡??좎??쒕떎. */
void APP_STATE_CopyGpsUiSnapshot(app_gps_ui_snapshot_t *dst);

/* ???쇱꽌 ?꾩슜 snapshot API.
 * ?쇱꽌 ?붾쾭洹??섏씠吏?????⑥닔 ?섎굹留??몄텧?섎㈃ ?쒕떎. */
void APP_STATE_CopyGy86Snapshot(app_gy86_state_t *dst);
void APP_STATE_CopyDs18b20Snapshot(app_ds18b20_state_t *dst);
void APP_STATE_CopySensorDebugSnapshot(app_sensor_debug_snapshot_t *dst);
void APP_STATE_CopyBrightnessSnapshot(app_brightness_state_t *dst);
void APP_STATE_CopyAudioSnapshot(app_audio_state_t *dst);
void APP_STATE_CopyBluetoothSnapshot(app_bluetooth_state_t *dst);
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
