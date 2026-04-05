#include "main.h"
#include "APP_STATE.h"
#include "APP_MEMORY_SECTIONS.h"

#include <string.h>

/* -------------------------------------------------------------------------- */
/*  ?꾩뿭 APP_STATE ??μ냼                                                      */
/* -------------------------------------------------------------------------- */

volatile app_state_t g_app_state APP_CCMRAM_BSS;

/* -------------------------------------------------------------------------- */
/*  ?대? ?좏떥: 湲곕낯 ?ъ슜???ㅼ젙 ?곸슜                                            */
/* -------------------------------------------------------------------------- */

static void APP_STATE_ApplyDefaultSettingsUnlocked(void)
{
    /* ---------------------------------------------------------------------- */
    /*  ?꾩옱 ?꾨줈?앺듃??湲곕낯 GPS ?ㅼ젙?                                        */
    /*    - MULTI CONSTELLATION 10Hz                                           */
    /*    - HIGH POWER                                                         */
    /*  濡??좎??쒕떎.                                                           */
    /* ---------------------------------------------------------------------- */
    g_app_state.settings.gps.boot_profile  = APP_GPS_BOOT_PROFILE_MULTI_CONST_10HZ;
    g_app_state.settings.gps.power_profile = APP_GPS_POWER_PROFILE_HIGH_POWER;

    /* ---------------------------------------------------------------------- */
    /*  ?쒓퀎 湲곕낯 ?뺤콉?                                                       */
    /*    - timezone : KST(UTC+09:00)                                          */
    /*    - GPS auto sync : enabled                                             */
    /*    - GPS periodic time-only sync : 10 min                                */
    /*  ?쇰줈 ?쒖옉?쒕떎. 異뷀썑 UI/API?먯꽌 諛붽씀?붾씪??backup register???곕줈        */
    /*  ??λ릺誘濡?cold boot 湲곕낯媛믪? ?ш린留??좎??섎㈃ ?쒕떎.                    */
    /* ---------------------------------------------------------------------- */
    g_app_state.settings.clock.timezone_quarters = APP_CLOCK_TIMEZONE_QUARTERS_DEFAULT;
    g_app_state.settings.clock.gps_auto_sync_enabled = 1u;
    g_app_state.settings.clock.gps_sync_interval_minutes = APP_CLOCK_GPS_SYNC_INTERVAL_MIN_DEFAULT;
    g_app_state.settings.clock.reserved0 = 0u;

    /* ---------------------------------------------------------------------- */
        /*  諛깅씪?댄듃 湲곕낯 ?뺤콉                                                     */
        /*                                                                        */
        /*  湲곕낯媛??섎룄                                                            */
        /*  - 遺??吏곹썑?먮뒗 二쇰?愿묒쓣 ?곗냽 異붿쥌?섎뒗 AUTO-CONT 紐⑤뱶濡??쒖옉?쒕떎.      */
        /*  - bias??0, smoothness??3(以묎컙媛??쇰줈 ?붾떎.                          */
        /*  - AUTO-DIMMER??議?媛믩룄 ?④퍡 湲곕낯媛믪쓣 ?ｌ뼱 ?먯뼱,                       */
        /*    ?섏쨷??紐⑤뱶留?諛붽퓭??利됱떆 ?숈옉?섍쾶 留뚮뱺??                           */
        /* ---------------------------------------------------------------------- */
        g_app_state.settings.backlight.auto_mode                      =
            (uint8_t)APP_BACKLIGHT_AUTO_MODE_CONTINUOUS;
        g_app_state.settings.backlight.continuous_bias_steps         = 0;
        g_app_state.settings.backlight.transition_smoothness         = 3u;
        g_app_state.settings.backlight.reserved0                     = 0u;
        g_app_state.settings.backlight.night_threshold_percent       = 32u;
        g_app_state.settings.backlight.super_night_threshold_percent = 12u;
        g_app_state.settings.backlight.night_brightness_percent      = 42u;
        g_app_state.settings.backlight.super_night_brightness_percent = 18u;

        /* ---------------------------------------------------------------------- */
        /*  UC1608 湲곕낯 ?⑤꼸 媛?                                                   */
        /*                                                                        */
        /*  ??媛믩뱾? ?꾩옱 肄붾뱶/李몄“ ?쒗?ㅼ쓽 ?덉쟾??湲곕낯媛믪쓣 APP_STATE?먮룄         */
        /*  洹몃?濡???ν빐 ?먮뒗 ?⑸룄??                                            */
        /* ---------------------------------------------------------------------- */
        g_app_state.settings.uc1608.contrast                 = 120u;
        g_app_state.settings.uc1608.temperature_compensation = 2u;
        g_app_state.settings.uc1608.bias_ratio               = 2u;
        g_app_state.settings.uc1608.ram_access_mode          = 1u;
        g_app_state.settings.uc1608.start_line_raw           = 0u;
        g_app_state.settings.uc1608.fixed_line_raw           = 0u;
        g_app_state.settings.uc1608.power_control_raw        = 7u;
        g_app_state.settings.uc1608.flip_mode                = 1u;

    /* ---------------------------------------------------------------------- */
    /*  Altitude / vario 湲곕낯 ?뺤콉                                             */
    /*                                                                        */
    /*  ?ㅺ퀎 泥좏븰                                                              */
    /*  - manual QNH? GPS-equivalent QNH瑜?遺꾨━?쒕떎.                          */
    /*  - GPS absolute anchor? IMU aid??媛곴컖 蹂꾨룄 ?좉?濡??붾떎.               */
    /*  - no-IMU / IMU 蹂묐젹 異붿젙? 紐⑤몢 ??긽 怨꾩궛?섎릺,                          */
    /*    二??쒖떆???좏깮留?imu_aid_enabled濡?寃곗젙?쒕떎.                         */
    /* ---------------------------------------------------------------------- */
        /* ------------------------------------------------------------------ */
        /*  ALTITUDE ?쒕퉬??湲곕낯 ?ㅼ젙                                           */
        /*                                                                    */
        /*  湲곕낯 泥좏븰                                                         */
        /*  - manual QNH / GPS equivalent QNH / fused altitude瑜?遺꾨━?쒕떎.    */
        /*  - baro??鍮좊Ⅴ怨?珥섏킌???곷? 梨꾨꼸, GPS???먮┛ ?덈? anchor??         */
        /*  - IMU-aided 寃곌낵????긽 怨꾩궛?섎릺, 湲곕낯 ?쒖떆 ?꾨낫???덉쟾?섍쾶        */
        /*    no-IMU 履쎌쓣 癒쇱? ?곕룄濡??쒖옉?쒕떎.                                */
        /*  - stationary burst ?듭젣瑜??꾪빐 rest display? ZUPT瑜?耳좊떎.        */
        /* ------------------------------------------------------------------ */
        g_app_state.settings.altitude.manual_qnh_hpa_x100            = 101325;
        g_app_state.settings.altitude.pressure_correction_hpa_x100   = 0;
               g_app_state.settings.altitude.gps_auto_equiv_qnh_enabled     = 1u;
               g_app_state.settings.altitude.gps_bias_correction_enabled    = 1u;
               g_app_state.settings.altitude.imu_aid_enabled                = 1u;
               g_app_state.settings.altitude.auto_home_capture_enabled      = 1u;

               /*  IMU sign + sensor poll debug gate 湲곕낯媛?                          */
               /*                                                                    */
               /*  imu_vertical_sign = +1                                             */
               /*  - ?꾩옱 湲곗? ?μ갑 諛⑺뼢?먯꽌 vertical specific-force 遺??湲곕낯媛?     */
               /*                                                                    */
               /*  imu_poll_enabled = 1                                               */
               /*  - MPU6050 polling? ?됱긽??湲곕낯 ON                                 */
               /*                                                                    */
               /*  mag_poll_enabled = 1                                               */
               /*  - HMC5883L raw polling??湲곕낯 ON?쇰줈 ?붾떎.                         */
               /*  - lean / grade 怨꾩궛?먮뒗 吏곸젒 ?쇰뱶諛깊븯吏 ?딄퀬,                        */
               /*    self-test + 蹂댁“ heading 吏꾨떒??raw ?뺣낫?먮쭔 ?ъ슜?쒕떎.            */
               /*                                                                    */
               /*  ms5611_only = 0                                                    */
               /*  - ?됱긽?쒖뿉??媛뺤젣 barometer-only 吏꾨떒 紐⑤뱶瑜????곹깭濡??쒖옉?쒕떎.   */
               /* ------------------------------------------------------------------ */
               g_app_state.settings.altitude.imu_vertical_sign              = 1;
               g_app_state.settings.altitude.imu_poll_enabled               = 1u;
               g_app_state.settings.altitude.mag_poll_enabled               = 1u;
               g_app_state.settings.altitude.ms5611_only                    = 0u;



               /* ------------------------------------------------------------------ */
               /*  pressure / vario / display 諛섏쓳 ?띾룄                               */
               /*                                                                    */
               /*  pressure_lpf_tau_ms = 100ms                                        */
               /*  - ?먮┛ altitude backbone 履?pressure LPF                           */
               /*  - fast audio path???댁젣 蹂꾨룄 pressure tau(18ms)瑜??곕?濡?        */
               /*    ?ш린 媛믪쓣 ?덈Т 洹밸떒?곸쑝濡?以꾩씪 ?꾩슂???녿떎.                     */
               /*  - 媛믪쓣 ??以꾩씠硫??レ옄 altitude / display媛 ???덈??댁?怨?          */
               /*    ???섎━硫??レ옄??李⑤텇?댁?吏留?QNH altitude step 諛섏쓳???뷀빐吏꾨떎. */
               /*                                                                    */
               /*  vario_fast_tau_ms = 60ms                                           */
               /*  - ?댁젣 "fast vario??release / decay tau" ?섎?媛 媛뺥븯??          */
               /*  - attack? APP_ALTITUDE ?대???FAST_VARIO_ATTACK_TAU_MS媛          */
               /*    ?대떦?섎?濡? ??媛믪? onset????텛湲곕낫??chatter / tail??         */
               /*    ?쇰쭏???뚮윭 以꾩?????媛源앸떎.                                   */
               /*  - 媛믪쓣 以꾩씠硫????ㅼ뿀?????대졇??????利됯컖?곸씠怨?                 */
               /*    ?섎━硫???怨좉툒?ㅻ읇寃?留ㅻ걟?섏?留?瑗щ━媛 湲몄뼱吏꾨떎.                */
               /*                                                                    */
               /*  vario_slow_tau_ms = 850ms                                          */
               /*  - ?レ옄 ?쒖떆/?됯퇏?곸씤 truth vario ???먮┛ 寃쎈줈                      */
               /*  - 媛믪씠 ?묒븘吏덉닔濡??レ옄 vario?????덈??댁쭊??                      */
               /*                                                                    */
               /*  display_lpf_tau_ms = 650ms                                         */
               /*  - display altitude stage-1 LPF                                     */
               /*  - fast trigger? 遺꾨━?섎?濡??쒖떆媛먮쭔 ?곕줈 ?좎??쒕떎.               */
               /* ------------------------------------------------------------------ */
               g_app_state.settings.altitude.pressure_lpf_tau_ms            = 100u;
               g_app_state.settings.altitude.vario_fast_tau_ms              = 60u;
               g_app_state.settings.altitude.vario_slow_tau_ms              = 850u;
               g_app_state.settings.altitude.display_lpf_tau_ms             = 650u;

               /* ------------------------------------------------------------------ */
               /*  ?뺤? ?곹깭 display ?덉젙??+ ZUPT 湲곕낯媛?                            */
               /*                                                                    */
               /*  rest_detect_vario_cms  = 0.10m/s                                   */
               /*  - display rest ?먯젙??湲곗?                                         */
               /*  - ?대쾲 ?섏닠 ?댄썑 ZUPT entry/exit/hysteresis ??                     */
               /*    APP_ALTITUDE ?대???蹂꾨룄 ?곸닔濡?遺꾨━?섏뿀??                      */
               /*  - 利? ??媛믪쓣 留뚯졇??fast audio onset???덉쟾留뚰겮 ?ш쾶 二쎌? ?딅뒗?? */
               /*                                                                    */
               /*  rest_detect_accel_mg   = 15mg                                       */
               /*  rest_display_tau_ms    = 2.6s                                       */
               /*  rest_display_hold_cm   = 짹20cm                                      */
               /*  zupt_enabled           = 1                                          */
               /*                                                                    */
               /*  二쇱쓽                                                               */
               /*  - display rest???レ옄 ?덉젙????븷                                  */
               /*  - core ZUPT??dwell+hysteresis瑜??듦낵???ㅼ뿉留?嫄몃┛??             */
               /* ------------------------------------------------------------------ */
               g_app_state.settings.altitude.rest_display_enabled           = 1u;
               g_app_state.settings.altitude.zupt_enabled                   = 1u;
               g_app_state.settings.altitude.reserved_rest0                 = 0u;
               g_app_state.settings.altitude.rest_detect_vario_cms          = 10u;
               g_app_state.settings.altitude.rest_detect_accel_mg           = 15u;
               g_app_state.settings.altitude.rest_display_tau_ms            = 2600u;
               g_app_state.settings.altitude.rest_display_hold_cm           = 20u;

               /* ------------------------------------------------------------------ */
               /*  baro / GPS gate 諛?measurement noise                               */
               /* ------------------------------------------------------------------ */
               g_app_state.settings.altitude.baro_measurement_noise_cm      = 30u;
               g_app_state.settings.altitude.baro_adaptive_noise_max_cm     = 250u;
               g_app_state.settings.altitude.gps_measurement_noise_floor_cm = 150u;
               g_app_state.settings.altitude.gps_max_vacc_mm                = 4000u;
               g_app_state.settings.altitude.gps_max_pdop_x100              = 350u;
               g_app_state.settings.altitude.gps_min_sats                   = 6u;
               g_app_state.settings.altitude.reserved2                      = 0u;
               g_app_state.settings.altitude.gps_bias_tau_ms                = 45000u;

               /* ------------------------------------------------------------------ */
               /*  baro velocity observation                                          */
               /*                                                                    */
               /*  baro_vario_lpf_tau_ms            = 55ms                            */
               /*  - regression slope ?꾨떒 truth LPF                                  */
               /*  - raw slope??fast trigger branch媛 ?곕줈 媛?멸?誘濡?                */
               /*    ?ш린 媛믪? backbone???쇰쭏??留ㅻ걟?섍쾶 ?섏? 議곗젅?쒕떎.              */
               /*  - 以꾩씠硫???誘쇨컧, ?섎━硫????덉젙.                                  */
               /*                                                                    */
               /*  baro_vario_measurement_noise_cms = 0.42m/s                         */
               /*  - KF媛 baro velocity observation???쇰쭏??誘우쓣吏 ?뺥븯??nominal R  */
               /*  - adaptive noise / rest-aware scaling / residual gate媛            */
               /*    ?ъ쟾???⑥븘 ?덉쑝誘濡? 湲곗〈 0.65m/s蹂대떎 怨쇨컧?섍쾶 ??텣??          */
               /*  - 媛믪쓣 ??以꾩씠硫?onset? 鍮⑤씪吏吏留?false climb???????덈떎.      */
               /* ------------------------------------------------------------------ */
               g_app_state.settings.altitude.baro_vario_lpf_tau_ms            = 55u;
               g_app_state.settings.altitude.baro_vario_measurement_noise_cms = 42u;

                /* ------------------------------------------------------------------ */
                /*  IMU vertical estimate 湲곕낯媛?                                      */
                /*                                                                    */
                /*  imu_gravity_tau_ms            = 700ms                              */
                /*  imu_accel_tau_ms              = 180ms                              */
                /*  imu_accel_lsb_per_g           = 8192  (MPU6050 짹4g)                */
                /*  imu_vertical_deadband_mg      = 12mg                               */
                /*  imu_vertical_clip_mg          = 450mg                              */
                /*  imu_measurement_noise_cms2    = 160cm/s짼                           */
                /*  imu_gyro_lsb_per_dps          = 66    (MPU6050 짹500dps 洹쇱궗)       */
                /*  imu_attitude_accel_gate_mg    = 80mg                               */
                /*  imu_predict_min_trust_permille= 600                                */
                /*                                                                    */
                /*  stationary burst媛 蹂댁씠硫?                                          */
                /*  1) I_TMIN???щ━怨?                                                 */
                /*  2) ATT_AG瑜???텛怨?                                                 */
                /*  3) A_TAU瑜?議곌툑 ?섎젮??                                             */
                /* ------------------------------------------------------------------ */
                g_app_state.settings.altitude.imu_gravity_tau_ms             = 700u;
                g_app_state.settings.altitude.imu_accel_tau_ms               = 180u;
                g_app_state.settings.altitude.imu_accel_lsb_per_g            = 8192u;
                g_app_state.settings.altitude.imu_vertical_deadband_mg       = 12u;
                g_app_state.settings.altitude.imu_vertical_clip_mg           = 450u;
                g_app_state.settings.altitude.imu_measurement_noise_cms2     = 160u;
                g_app_state.settings.altitude.imu_gyro_lsb_per_dps           = 66u;
                g_app_state.settings.altitude.imu_attitude_accel_gate_mg     = 80u;
                g_app_state.settings.altitude.imu_predict_min_trust_permille = 600u;

        /* ------------------------------------------------------------------ */
        /*  Kalman process noise 湲곕낯媛?                                       */
        /*                                                                    */
        /*  Q_h : 怨좊룄 ?곹깭媛 ?ㅼ뒪濡??쇱????뺣룄                               */
        /*  Q_v : ?띾룄 ?곹깭媛 ?ㅼ뒪濡??쇱????뺣룄                               */
        /*  Q_b : baro bias媛 泥쒖쿇???吏곸씠???뺣룄                              */
        /*  Q_a : accel bias媛 泥쒖쿇???吏곸씠???뺣룄                             */
        /* ------------------------------------------------------------------ */
        g_app_state.settings.altitude.kf_q_height_cm_per_s           = 5u;
        g_app_state.settings.altitude.kf_q_velocity_cms_per_s        = 60u;
        g_app_state.settings.altitude.kf_q_baro_bias_cm_per_s        = 2u;
        g_app_state.settings.altitude.kf_q_accel_bias_cms2_per_s     = 20u;

        /* ------------------------------------------------------------------ */
        /*  ALTITUDE debug page ?꾩슜 vario audio 湲곕낯媛?                       */
        /*                                                                    */
        /*  audio_repeat_ms / audio_beep_ms ??                                */
        /*  climb cadence??湲곗?媛믪씪 肉먯씠怨?                                    */
        /*  ?ㅼ젣 ?뚯젙? fast vario ?섏튂媛 ?ㅼ떆媛꾩쑝濡?FM modulation ?쒕떎.         */
        /*                                                                    */
        /*  ?대쾲 湲곕낯媛믪?                                                       */
        /*  - 吏?섏튂寃?珥섏킌??beep 諛섎났???쎄컙 ??텛怨?                          */
        /*  - single beep 湲몄씠瑜?議곌툑 ?섎젮                                      */
        /*  continuous oscillator 湲곕컲 tone??                                  */
        /*  ???먯뿰?ㅻ읇寃??ㅻ━?꾨줉 留욎텣 媛믪씠??                                 */
        /* ------------------------------------------------------------------ */
        g_app_state.settings.altitude.debug_audio_enabled            = 1u;
        g_app_state.settings.altitude.debug_audio_source             = 0u;
        g_app_state.settings.altitude.audio_deadband_cms             = 35u;
        g_app_state.settings.altitude.audio_min_freq_hz              = 700u;
        g_app_state.settings.altitude.audio_max_freq_hz              = 2200u;
        g_app_state.settings.altitude.audio_repeat_ms                = 170u;
        g_app_state.settings.altitude.audio_beep_ms                  = 65u;

        /* ---------------------------------------------------------------------- */
        /*  BIKE DYNAMICS 湲곕낯 ?ㅼ젙                                                 */
        /*                                                                        */
        /*  湲곕낯 ?μ갑 媛??                                                        */
        /*  - sensor +X : 李⑤웾 forward                                             */
        /*  - sensor +Y : 李⑤웾 left                                                */
        /*  - sensor +Z : 李⑤웾 up                                                  */
        /*                                                                        */
        /*  ?꾩옱 GY86_IMU driver 湲곗? scale                                         */
        /*  - accel 짹4g     -> 8192 LSB/g                                           */
        /*  - gyro  짹500dps -> 65.5 LSB/dps -> x10 scale濡?655                      */
        /*                                                                        */
        /*  ?꾪꽣 ?깃꺽                                                               */
        /*  - gravity_tau 700ms   : lean/grade 湲곗?異뺤? ?덈Т 誘쇨컧?섏? ?딄쾶          */
        /*  - linear_tau  120ms   : lat/lon 媛?띾룄??吏꾨룞???쎄컙 ?꾨Ⅴ??            */
        /*                            ?ъ뼱留?而댄벂???묐떟?깆? ?좎?                   */
        /*  - GNSS bias tau 4.0s  : GNSS???二쇳뙆 anchor濡쒕쭔 泥쒖쿇??癒뱀씤??        */
        /* ---------------------------------------------------------------------- */
        g_app_state.settings.bike.enabled                        = 0u;
        g_app_state.settings.bike.auto_zero_on_boot              = 0u;
        g_app_state.settings.bike.gnss_aid_enabled               = 1u;
        g_app_state.settings.bike.obd_aid_enabled                = 0u;

        g_app_state.settings.bike.mount_forward_axis             = (uint8_t)APP_BIKE_AXIS_POS_X;
        g_app_state.settings.bike.mount_left_axis                = (uint8_t)APP_BIKE_AXIS_POS_Y;
        g_app_state.settings.bike.mount_yaw_trim_deg_x10         = 0;

        g_app_state.settings.bike.imu_accel_lsb_per_g            = 8192u;
        g_app_state.settings.bike.imu_gyro_lsb_per_dps_x10       = 655u;

        g_app_state.settings.bike.imu_gravity_tau_ms             = 700u;
        g_app_state.settings.bike.imu_linear_tau_ms              = 120u;
        g_app_state.settings.bike.imu_attitude_accel_gate_mg     = 120u;
        g_app_state.settings.bike.imu_jerk_gate_mg_per_s         = 3500u;
        g_app_state.settings.bike.imu_predict_min_trust_permille = 500u;
        g_app_state.settings.bike.imu_stale_timeout_ms           = 250u;

        g_app_state.settings.bike.output_deadband_mg             = 12u;
        g_app_state.settings.bike.output_clip_mg                 = 1800u;
        g_app_state.settings.bike.lean_display_tau_ms            = 180u;
        g_app_state.settings.bike.grade_display_tau_ms           = 250u;
        g_app_state.settings.bike.accel_display_tau_ms           = 180u;

        g_app_state.settings.bike.gnss_min_speed_kmh_x10         = 80u;   /* 8.0 km/h */
        g_app_state.settings.bike.gnss_max_speed_acc_kmh_x10     = 15u;   /* 1.5 km/h */
        g_app_state.settings.bike.gnss_max_head_acc_deg_x10      = 80u;   /* 8.0 deg  */
        g_app_state.settings.bike.gnss_bias_tau_ms               = 4000u;
        g_app_state.settings.bike.gnss_outlier_gate_mg           = 450u;

        g_app_state.settings.bike.obd_stale_timeout_ms           = 500u;
        g_app_state.settings.bike.reserved0                      = 0u;


}

/* -------------------------------------------------------------------------- */
/*  ?대? ?좏떥: GPS ??μ냼 珥덇린??                                               */
/* -------------------------------------------------------------------------- */

static void APP_STATE_ResetGpsUnlocked(void)
{
    memset((void *)&g_app_state.gps, 0, sizeof(g_app_state.gps));
    g_app_state.gps.initialized = true;
}

/* -------------------------------------------------------------------------- */
/*  ?대? ?좏떥: CLOCK ??μ냼 珥덇린??                                             */
/* -------------------------------------------------------------------------- */
static void APP_STATE_ResetClockUnlocked(void)
{
    memset((void *)&g_app_state.clock, 0, sizeof(g_app_state.clock));

    /* ---------------------------------------------------------------------- */
    /*  APP_STATE.clock ??"RTC?먯꽌 ?ㅼ젣濡??쎌? raw/runtime" ??μ냼??          */
    /*  珥덇린媛??④퀎?먯꽌??settings??clock 湲곕낯 ?뺤콉??洹몃?濡?諛섏쁺???붾떎.     */
    /*  ?댄썑 APP_CLOCK_Init()媛 backup register瑜??쎌뼱 理쒖쥌 runtime 媛믪쓣 梨꾩슫?? */
    /* ---------------------------------------------------------------------- */
    g_app_state.clock.initialized = false;
    g_app_state.clock.backup_config_valid = false;
    g_app_state.clock.rtc_time_valid = false;
    g_app_state.clock.rtc_read_valid = false;
    g_app_state.clock.gps_candidate_valid = false;
    g_app_state.clock.gps_auto_sync_enabled_runtime =
        (g_app_state.settings.clock.gps_auto_sync_enabled != 0u) ? true : false;
    g_app_state.clock.gps_last_sync_success = false;
    g_app_state.clock.gps_last_sync_was_full = false;
    g_app_state.clock.gps_resolved_seen = false;
    g_app_state.clock.timezone_config_valid = true;

    g_app_state.clock.timezone_quarters = g_app_state.settings.clock.timezone_quarters;
    g_app_state.clock.gps_sync_interval_minutes =
        g_app_state.settings.clock.gps_sync_interval_minutes;
    g_app_state.clock.last_sync_source = (uint8_t)APP_CLOCK_SYNC_SOURCE_NONE;
    g_app_state.clock.reserved0 = 0u;
}

/* -------------------------------------------------------------------------- */
/*  ?대? ?좏떥: GY-86 ??μ냼 珥덇린??                                             */
/* -------------------------------------------------------------------------- */

static void APP_STATE_ResetGy86Unlocked(void)
{
    memset((void *)&g_app_state.gy86, 0, sizeof(g_app_state.gy86));

    /* ---------------------------------------------------------------------- */
    /*  backend ID / ?뷀뤃??二쇨린 媛숈? 媛믪?                                    */
    /*  ?쒕씪?대쾭 init?먯꽌???ㅼ떆 梨꾩슦吏留?                                     */
    /*  ?먮즺李쎄퀬 珥덇린 ?곹깭??紐낆떆?곸쑝濡?蹂댁씠寃??ш린??0/湲곕낯媛믪쓣 ?ｌ뼱 ?붾떎.    */
    /* ---------------------------------------------------------------------- */
    g_app_state.gy86.initialized = false;
    g_app_state.gy86.status_flags = 0u;
    g_app_state.gy86.last_update_ms = 0u;

    g_app_state.gy86.debug.accelgyro_backend_id = APP_IMU_BACKEND_NONE;
    g_app_state.gy86.debug.mag_backend_id       = APP_IMU_BACKEND_NONE;
    g_app_state.gy86.debug.baro_backend_id      = APP_IMU_BACKEND_NONE;

    /* ---------------------------------------------------------------------- */
    /*  dual-baro ?붾쾭洹??щ’ 湲곕낯媛?                                          */
    /*                                                                        */
    /*  ?ㅼ젣 configured/online/valid 媛믪?                                      */
    /*  low-level driver init???앸궃 ??GY86_IMU.c 媛 梨꾩슫??                 */
    /*  ?ш린?쒕뒗 slot 媛쒖닔? invalid primary index sentinel 留?誘몃━ ?ｋ뒗??   */
    /* ---------------------------------------------------------------------- */
    g_app_state.gy86.debug.baro_device_slots        = APP_GY86_BARO_SENSOR_SLOTS;
    g_app_state.gy86.debug.baro_primary_sensor_index = 0xFFu;

    g_app_state.gy86.debug.mpu_poll_period_ms  = 0u;
    g_app_state.gy86.debug.mag_poll_period_ms  = 0u;
    g_app_state.gy86.debug.baro_poll_period_ms = 0u;

}

/* -------------------------------------------------------------------------- */
/*  ?대? ?좏떥: DS18B20 ??μ냼 珥덇린??                                           */
/* -------------------------------------------------------------------------- */

static void APP_STATE_ResetDs18b20Unlocked(void)
{
    memset((void *)&g_app_state.ds18b20, 0, sizeof(g_app_state.ds18b20));

    /* ---------------------------------------------------------------------- */
    /*  ?⑤룄??"0" 怨?"?꾩쭅 ?쎌? 紐삵븿" ??援щ텇?섍린 ?꾪빐                         */
    /*  invalid sentinel 媛믪쓣 紐낆떆?곸쑝濡??ｋ뒗??                               */
    /* ---------------------------------------------------------------------- */
    g_app_state.ds18b20.initialized = false;
    g_app_state.ds18b20.status_flags = 0u;
    g_app_state.ds18b20.last_update_ms = 0u;

    g_app_state.ds18b20.raw.temp_c_x100 = APP_DS18B20_TEMP_INVALID;
    g_app_state.ds18b20.raw.temp_f_x100 = APP_DS18B20_TEMP_INVALID;

    g_app_state.ds18b20.debug.phase = APP_DS18B20_PHASE_UNINIT;
    g_app_state.ds18b20.debug.last_error = APP_DS18B20_ERR_NONE;

    /* 12-bit 湲곕낯 紐⑺몴媛?*/
    g_app_state.ds18b20.debug.conversion_time_ms = 750u;
}

/* -------------------------------------------------------------------------- */
/*  ?대? ?좏떥: 諛앷린 ?쇱꽌 ??μ냼 珥덇린??                                         */
/* -------------------------------------------------------------------------- */

static void APP_STATE_ResetBrightnessUnlocked(void)
{
    memset((void *)&g_app_state.brightness, 0, sizeof(g_app_state.brightness));

    g_app_state.brightness.initialized = false;
    g_app_state.brightness.valid       = false;
    g_app_state.brightness.last_update_ms = 0u;
    g_app_state.brightness.sample_count   = 0u;

    /* ---------------------------------------------------------------------- */
    /*  ADC ?쒕씪?대쾭媛 ?꾩쭅 ???щ씪?ㅺ린 ???곹깭瑜?援щ텇?섍린 ?꾪빐                 */
    /*  last_hal_status??0xFF sentinel濡??붾떎.                                */
    /* ---------------------------------------------------------------------- */
    g_app_state.brightness.debug.last_hal_status = 0xFFu;
}

/* -------------------------------------------------------------------------- */
/*  ?대? ?좏떥: Audio ??μ냼 珥덇린??                                            */
/* -------------------------------------------------------------------------- */
static void APP_STATE_ResetAudioUnlocked(void)
{
    uint32_t voice_index;

    memset((void *)&g_app_state.audio, 0, sizeof(g_app_state.audio));

    g_app_state.audio.initialized                  = false;
    g_app_state.audio.transport_running            = false;
    g_app_state.audio.content_active               = false;
    g_app_state.audio.wav_active                   = false;

    g_app_state.audio.mode                         = APP_AUDIO_MODE_IDLE;
    g_app_state.audio.active_voice_count           = 0u;
    g_app_state.audio.last_hal_status_dac          = 0xFFu;
    g_app_state.audio.last_hal_status_tim          = 0xFFu;

    /* ---------------------------------------------------------------------- */
    /*  STM32F4 DAC???ㅼ젣 ?꾨궇濡쒓렇 異쒕젰 遺꾪빐?μ씠 12bit ?대떎.                  */
    /*  source WAV媛 16/24/32bit ?댁뼱??理쒖쥌 DAC ?④퀎?먯꽌??12bit濡??섍컙??    */
    /* ---------------------------------------------------------------------- */
    g_app_state.audio.output_resolution_bits       = 12u;
    g_app_state.audio.volume_percent               = 0u;
    g_app_state.audio.last_block_clipped           = 0u;
    g_app_state.audio.wav_native_rate_active       = 0u;

    g_app_state.audio.sample_rate_hz               = 0u;
    g_app_state.audio.dma_buffer_sample_count      = 0u;
    g_app_state.audio.dma_half_buffer_sample_count = 0u;
    g_app_state.audio.last_block_min_u12           = 2048u;
    g_app_state.audio.last_block_max_u12           = 2048u;

    g_app_state.audio.sw_fifo_capacity_samples        = 0u;
    g_app_state.audio.sw_fifo_level_samples           = 0u;
    g_app_state.audio.sw_fifo_peak_level_samples      = 0u;
    g_app_state.audio.sw_fifo_low_watermark_samples   = 0u;
    g_app_state.audio.sw_fifo_high_watermark_samples  = 0u;

    g_app_state.audio.last_update_ms               = 0u;
    g_app_state.audio.playback_start_ms            = 0u;
    g_app_state.audio.playback_stop_ms             = 0u;
    g_app_state.audio.half_callback_count          = 0u;
    g_app_state.audio.full_callback_count          = 0u;
    g_app_state.audio.dma_underrun_count           = 0u;
    g_app_state.audio.render_block_count           = 0u;
    g_app_state.audio.clip_block_count             = 0u;
    g_app_state.audio.transport_reconfig_count     = 0u;
    g_app_state.audio.producer_refill_block_count  = 0u;
    g_app_state.audio.dma_service_half_count       = 0u;
    g_app_state.audio.fifo_starvation_count        = 0u;
    g_app_state.audio.silence_injected_sample_count = 0u;

    g_app_state.audio.sequence_bpm                 = 0u;
    g_app_state.audio.wav_source_sample_rate_hz    = 0u;
    g_app_state.audio.wav_source_data_bytes_remaining = 0u;
    g_app_state.audio.wav_source_channels          = 0u;
    g_app_state.audio.wav_source_bits_per_sample   = 0u;

    for (voice_index = 0u; voice_index < APP_AUDIO_MAX_VOICES; voice_index++)
    {
        g_app_state.audio.voices[voice_index].active               = false;
        g_app_state.audio.voices[voice_index].waveform_id          = APP_AUDIO_WAVEFORM_NONE;
        g_app_state.audio.voices[voice_index].timbre_id            = 0u;
        g_app_state.audio.voices[voice_index].track_index          = 0u;
        g_app_state.audio.voices[voice_index].env_phase            = APP_AUDIO_ENV_OFF;
        g_app_state.audio.voices[voice_index].note_hz_x100         = 0u;
        g_app_state.audio.voices[voice_index].phase_q32            = 0u;
        g_app_state.audio.voices[voice_index].phase_inc_q32        = 0u;
        g_app_state.audio.voices[voice_index].note_samples_total   = 0u;
        g_app_state.audio.voices[voice_index].note_samples_elapsed = 0u;
        g_app_state.audio.voices[voice_index].gate_samples         = 0u;
        g_app_state.audio.voices[voice_index].env_level_q15        = 0u;
        g_app_state.audio.voices[voice_index].velocity_q15         = 0u;
    }
}



/* -------------------------------------------------------------------------- */
/*  ?대? ?좏떥: Bluetooth ??μ냼 珥덇린??                                         */
/* -------------------------------------------------------------------------- */

static void APP_STATE_ResetBluetoothUnlocked(void)
{
    memset((void *)&g_app_state.bluetooth, 0, sizeof(g_app_state.bluetooth));

    g_app_state.bluetooth.initialized            = false;
    g_app_state.bluetooth.uart_rx_running        = false;
    g_app_state.bluetooth.echo_enabled           = true;
    g_app_state.bluetooth.auto_ping_enabled      = false;

    g_app_state.bluetooth.last_update_ms         = 0u;
    g_app_state.bluetooth.last_rx_ms             = 0u;
    g_app_state.bluetooth.last_tx_ms             = 0u;
    g_app_state.bluetooth.last_auto_ping_ms      = 0u;

    g_app_state.bluetooth.rx_bytes               = 0u;
    g_app_state.bluetooth.tx_bytes               = 0u;
    g_app_state.bluetooth.rx_line_count          = 0u;
    g_app_state.bluetooth.tx_line_count          = 0u;
    g_app_state.bluetooth.rx_overflow_count      = 0u;
    g_app_state.bluetooth.uart_error_count       = 0u;
    g_app_state.bluetooth.uart_rearm_fail_count  = 0u;
    g_app_state.bluetooth.uart_tx_fail_count     = 0u;

    g_app_state.bluetooth.rx_ring_level          = 0u;
    g_app_state.bluetooth.rx_ring_high_watermark = 0u;

    /* ---------------------------------------------------------------------- */
    /*  ?꾩쭅 RX/TX媛 ?쇱뼱?섏? ?딆븯?뚯쓣 援щ텇?섍린 ?꾪빐                            */
    /*  HAL status raw??0xFF sentinel濡??붾떎.                                 */
    /* ---------------------------------------------------------------------- */
    g_app_state.bluetooth.last_hal_status_rx     = 0xFFu;
    g_app_state.bluetooth.last_hal_status_tx     = 0xFFu;
    g_app_state.bluetooth.last_hal_error         = 0u;
}

/* -------------------------------------------------------------------------- */
/*  ?대? ?좏떥: DEBUG UART ??μ냼 珥덇린??                                        */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*  ?대? ?좏떥: SD / FATFS ??μ냼 珥덇린??                                        */
/* -------------------------------------------------------------------------- */

static void APP_STATE_ResetSdUnlocked(void)
{
    memset((void *)&g_app_state.sd, 0, sizeof(g_app_state.sd));

    /* ---------------------------------------------------------------------- */
    /*  SD 履쎌? "?꾩쭅 ?쒕룄?섏? ?딆쓬" 怨?"0" ??援щ텇?섍린 ?꾪빐                    */
    /*  紐뉖챺 raw ?곹깭 媛믪뿉 sentinel ???ｌ뼱 ?붾떎.                              */
    /* ---------------------------------------------------------------------- */
    g_app_state.sd.detect_raw_present      = false;
    g_app_state.sd.detect_stable_present   = false;
    g_app_state.sd.detect_debounce_pending = false;

    g_app_state.sd.initialized             = false;
    g_app_state.sd.mounted                 = false;
    g_app_state.sd.fat_valid               = false;
    g_app_state.sd.is_fat32                = false;

    g_app_state.sd.fs_type                 = 0u;
    g_app_state.sd.card_type               = 0u;
    g_app_state.sd.card_version            = 0u;
    g_app_state.sd.card_class              = 0u;
    g_app_state.sd.hal_state               = 0xFFu;
    g_app_state.sd.transfer_state          = 0xFFu;
    g_app_state.sd.last_bsp_init_status    = 0xFFu;

    g_app_state.sd.last_mount_fresult      = 0xFFFFFFFFu;
    g_app_state.sd.last_getfree_fresult    = 0xFFFFFFFFu;
    g_app_state.sd.last_root_scan_fresult  = 0xFFFFFFFFu;
}

/* -------------------------------------------------------------------------- */
/*  ?대? ?좏떥: ALTITUDE ??μ냼 珥덇린??                                         */
/* -------------------------------------------------------------------------- */
static void APP_STATE_ResetAltitudeUnlocked(void)
{
    memset((void *)&g_app_state.altitude, 0, sizeof(g_app_state.altitude));

    g_app_state.altitude.initialized             = false;
    g_app_state.altitude.baro_valid              = false;
    g_app_state.altitude.gps_valid               = false;
    g_app_state.altitude.home_valid              = false;
    g_app_state.altitude.imu_vector_valid        = false;
    g_app_state.altitude.debug_audio_active      = 0u;
    g_app_state.altitude.gps_quality_permille    = 0u;

    g_app_state.altitude.last_update_ms          = 0u;
    g_app_state.altitude.last_baro_update_ms     = 0u;
    g_app_state.altitude.last_gps_update_ms      = 0u;

    g_app_state.altitude.pressure_raw_hpa_x100   = 0;
    g_app_state.altitude.pressure_filt_hpa_x100  = 0;
    g_app_state.altitude.qnh_manual_hpa_x100     = g_app_state.settings.altitude.manual_qnh_hpa_x100;
    g_app_state.altitude.qnh_equiv_gps_hpa_x100  = g_app_state.settings.altitude.manual_qnh_hpa_x100;

    g_app_state.altitude.alt_pressure_std_cm     = 0;
    g_app_state.altitude.alt_qnh_manual_cm       = 0;
    g_app_state.altitude.alt_gps_hmsl_cm         = 0;
    g_app_state.altitude.alt_fused_noimu_cm      = 0;
    g_app_state.altitude.alt_fused_imu_cm        = 0;
    g_app_state.altitude.alt_display_cm          = 0;

    g_app_state.altitude.alt_rel_home_noimu_cm   = 0;
    g_app_state.altitude.alt_rel_home_imu_cm     = 0;
    g_app_state.altitude.home_alt_noimu_cm       = 0;
    g_app_state.altitude.home_alt_imu_cm         = 0;

    g_app_state.altitude.baro_bias_noimu_cm      = 0;
    g_app_state.altitude.baro_bias_imu_cm        = 0;

    g_app_state.altitude.vario_fast_noimu_cms    = 0;
    g_app_state.altitude.vario_slow_noimu_cms    = 0;
    g_app_state.altitude.vario_fast_imu_cms      = 0;
    g_app_state.altitude.vario_slow_imu_cms      = 0;

    g_app_state.altitude.grade_noimu_x10         = 0;
    g_app_state.altitude.grade_imu_x10           = 0;

    g_app_state.altitude.imu_vertical_accel_mg   = 0;
    g_app_state.altitude.imu_vertical_accel_cms2 = 0;
    g_app_state.altitude.imu_gravity_norm_mg     = 0;

    g_app_state.altitude.gps_vacc_mm             = 0u;
    g_app_state.altitude.gps_pdop_x100           = 0u;
    g_app_state.altitude.gps_numsv_used          = 0u;
    g_app_state.altitude.gps_fix_type            = 0u;
}

static void APP_STATE_ResetBikeUnlocked(void)
{
    memset((void *)&g_app_state.bike, 0, sizeof(g_app_state.bike));

    g_app_state.bike.initialized                 = false;
    g_app_state.bike.zero_valid                  = false;
    g_app_state.bike.imu_valid                   = false;
    g_app_state.bike.gnss_aid_valid              = false;
    g_app_state.bike.gnss_heading_valid          = false;
    g_app_state.bike.obd_speed_valid             = false;
    g_app_state.bike.speed_source                = (uint8_t)APP_BIKE_SPEED_SOURCE_NONE;
    g_app_state.bike.estimator_mode              = (uint8_t)APP_BIKE_ESTIMATOR_MODE_IMU_ONLY;
    g_app_state.bike.confidence_permille         = 0u;

    g_app_state.bike.last_update_ms              = 0u;
    g_app_state.bike.last_imu_update_ms          = 0u;
    g_app_state.bike.last_zero_capture_ms        = 0u;
    g_app_state.bike.last_gnss_aid_ms            = 0u;
    g_app_state.bike.zero_request_count          = 0u;
    g_app_state.bike.hard_rezero_count           = 0u;

    g_app_state.bike.banking_angle_deg_x10       = 0;
    g_app_state.bike.banking_angle_display_deg   = 0;
    g_app_state.bike.grade_deg_x10               = 0;
    g_app_state.bike.grade_display_deg           = 0;
    g_app_state.bike.bank_rate_dps_x10           = 0;
    g_app_state.bike.grade_rate_dps_x10          = 0;

    g_app_state.bike.lat_accel_mg                = 0;
    g_app_state.bike.lon_accel_mg                = 0;
    g_app_state.bike.lat_accel_cms2              = 0;
    g_app_state.bike.lon_accel_cms2              = 0;

    g_app_state.bike.lat_accel_imu_mg            = 0;
    g_app_state.bike.lon_accel_imu_mg            = 0;
    g_app_state.bike.lat_accel_ref_mg            = 0;
    g_app_state.bike.lon_accel_ref_mg            = 0;
    g_app_state.bike.lat_bias_mg                 = 0;
    g_app_state.bike.lon_bias_mg                 = 0;

    g_app_state.bike.imu_accel_norm_mg           = 0;
    g_app_state.bike.imu_jerk_mg_per_s           = 0;
    g_app_state.bike.imu_attitude_trust_permille = 0u;
    g_app_state.bike.up_bx_milli                 = 0;
    g_app_state.bike.up_by_milli                 = 0;
    g_app_state.bike.up_bz_milli                 = 0;

    g_app_state.bike.speed_mmps                  = 0;
    g_app_state.bike.speed_kmh_x10               = 0u;
    g_app_state.bike.gnss_speed_acc_kmh_x10      = 0u;
    g_app_state.bike.gnss_head_acc_deg_x10       = 0u;
    g_app_state.bike.mount_yaw_trim_deg_x10      = g_app_state.settings.bike.mount_yaw_trim_deg_x10;

    g_app_state.bike.gnss_fix_ok                 = 0u;
    g_app_state.bike.gnss_numsv_used             = 0u;
    g_app_state.bike.gnss_pdop_x100              = 0u;

    g_app_state.bike.heading_valid               = false;
    g_app_state.bike.mag_heading_valid           = false;
    g_app_state.bike.heading_source              = (uint8_t)APP_BIKE_HEADING_SOURCE_NONE;
    g_app_state.bike.reserved_heading0           = 0u;
    g_app_state.bike.heading_deg_x10             = 0;
    g_app_state.bike.mag_heading_deg_x10         = 0;



    g_app_state.bike.gyro_bias_cal_active        = false;
    g_app_state.bike.gyro_bias_valid             = false;
    g_app_state.bike.gyro_bias_cal_last_success  = false;
    g_app_state.bike.reserved_gyro_bias0         = 0u;
    g_app_state.bike.gyro_bias_cal_progress_permille = 0u;
    g_app_state.bike.last_gyro_bias_cal_ms       = 0u;
    g_app_state.bike.gyro_bias_cal_count         = 0u;
    g_app_state.bike.gyro_bias_x_dps_x100        = 0;
    g_app_state.bike.gyro_bias_y_dps_x100        = 0;
    g_app_state.bike.gyro_bias_z_dps_x100        = 0;
    g_app_state.bike.yaw_rate_dps_x10            = 0;

    /* ---------------------------------------------------------------------- */
    /*  future OBD ?낅젰 ?꾨뱶??reset ?쒖뿉留?0?쇰줈 ?붾떎.                         */
    /*  異뷀썑 OBD service媛 ???꾨뱶?ㅼ쓣 ?ㅼ떆 梨꾩슦硫?BIKE_DYNAMICS媛 ?쎈뒗??      */
    /* ---------------------------------------------------------------------- */
    g_app_state.bike.obd_input_speed_valid       = false;
    g_app_state.bike.obd_input_speed_mmps        = 0u;
    g_app_state.bike.obd_input_last_update_ms    = 0u;
}

/* -------------------------------------------------------------------------- */
/*  怨듦컻 API: GPS slice留?珥덇린??                                               */
/* -------------------------------------------------------------------------- */

void APP_STATE_ResetGps(void)
{
    __disable_irq();
    APP_STATE_ResetGpsUnlocked();
    __enable_irq();
}

/* -------------------------------------------------------------------------- */
/*  怨듦컻 API: ?꾩껜 APP_STATE 珥덇린??                                            */
/* -------------------------------------------------------------------------- */

void APP_STATE_Init(void)
{
    __disable_irq();

    /* ---------------------------------------------------------------------- */
    /*  ?먮즺李쎄퀬 ?꾩껜瑜?0?쇰줈 由ъ뀑????                                        */
    /*  "0???꾨땶 ?섎?媛? ???꾩슂????ぉ?ㅻ쭔 蹂꾨룄 helper濡?蹂듦뎄?쒕떎.           */
    /* ---------------------------------------------------------------------- */
    memset((void *)&g_app_state, 0, sizeof(g_app_state));

    APP_STATE_ApplyDefaultSettingsUnlocked();
    APP_STATE_ResetGpsUnlocked();
    APP_STATE_ResetClockUnlocked();
    APP_STATE_ResetGy86Unlocked();
    APP_STATE_ResetDs18b20Unlocked();
    APP_STATE_ResetBrightnessUnlocked();
    APP_STATE_ResetAudioUnlocked();
    APP_STATE_ResetBluetoothUnlocked();
    APP_STATE_ResetSdUnlocked();
    APP_STATE_ResetAltitudeUnlocked();
    APP_STATE_ResetBikeUnlocked();

    __enable_irq();
}

/* -------------------------------------------------------------------------- */
/*  怨듦컻 API: ?꾩껜 ?ㅻ깄??蹂듭궗                                                  */
/* -------------------------------------------------------------------------- */

void APP_STATE_CopySnapshot(app_state_t *dst)
{
    if (dst == 0)
    {
        return;
    }

    __disable_irq();
    memcpy(dst, (const void *)&g_app_state, sizeof(*dst));
    __enable_irq();
}

/* -------------------------------------------------------------------------- */
/*  怨듦컻 API: GPS ?꾩껜 ?ㅻ깄??蹂듭궗                                              */
/* -------------------------------------------------------------------------- */

void APP_STATE_CopyGpsSnapshot(app_gps_state_t *dst)
{
    if (dst == 0)
    {
        return;
    }

    __disable_irq();
    memcpy(dst, (const void *)&g_app_state.gps, sizeof(*dst));
    __enable_irq();
}

/* -------------------------------------------------------------------------- */
/*  怨듦컻 API: GY-86 ?꾩껜 ?ㅻ깄??蹂듭궗                                            */
/* -------------------------------------------------------------------------- */

void APP_STATE_CopyGy86Snapshot(app_gy86_state_t *dst)
{
    if (dst == 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  GY-86 slice??main context?먯꽌留?媛깆떊?쒕떎.                               */
    /*  ?곕씪??snapshot 蹂듭궗瑜??꾪빐 IRQ瑜?留됱쓣 ?꾩슂媛 ?녿떎.                     */
    /* ---------------------------------------------------------------------- */
    memcpy(dst, (const void *)&g_app_state.gy86, sizeof(*dst));
}

/* -------------------------------------------------------------------------- */
/*  怨듦컻 API: DS18B20 ?꾩껜 ?ㅻ깄??蹂듭궗                                          */
/* -------------------------------------------------------------------------- */

void APP_STATE_CopyDs18b20Snapshot(app_ds18b20_state_t *dst)
{
    if (dst == 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  DS18B20 slice??bit-bang timing? ?대??먯꽌 泥섎━?섏?留?                   */
    /*  怨듦컻 ??μ냼 媛깆떊? main context?먯꽌留??섑뻾?쒕떎.                         */
    /* ---------------------------------------------------------------------- */
    memcpy(dst, (const void *)&g_app_state.ds18b20, sizeof(*dst));
}

/* -------------------------------------------------------------------------- */
/*  怨듦컻 API: 諛앷린 ?쇱꽌 ?꾩껜 ?ㅻ깄??蹂듭궗                                        */
/* -------------------------------------------------------------------------- */

void APP_STATE_CopyBrightnessSnapshot(app_brightness_state_t *dst)
{
    if (dst == 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  諛앷린 ?쇱꽌 slice??main loop state machine留?媛깆떊?쒕떎.                   */
    /*  ?곕씪??snapshot 蹂듭궗??plain memcpy濡?異⑸텇?섎떎.                        */
    /* ---------------------------------------------------------------------- */
    memcpy(dst, (const void *)&g_app_state.brightness, sizeof(*dst));
}

/* -------------------------------------------------------------------------- */
/*  怨듦컻 API: Audio ?꾩껜 ?ㅻ깄??蹂듭궗                                            */
/* -------------------------------------------------------------------------- */
void APP_STATE_CopyAudioSnapshot(app_audio_state_t *dst)
{
    if (dst == 0)
    {
        return;
    }

    __disable_irq();
    memcpy(dst, (const void *)&g_app_state.audio, sizeof(*dst));
    __enable_irq();
}


/* -------------------------------------------------------------------------- */
/*  怨듦컻 API: Bluetooth ?꾩껜 ?ㅻ깄??蹂듭궗                                        */
/* -------------------------------------------------------------------------- */

void APP_STATE_CopyBluetoothSnapshot(app_bluetooth_state_t *dst)
{
    if (dst == 0)
    {
        return;
    }

    __disable_irq();
    memcpy(dst, (const void *)&g_app_state.bluetooth, sizeof(*dst));
    __enable_irq();
}

/* -------------------------------------------------------------------------- */
/*  怨듦컻 API: DEBUG UART ?꾩껜 ?ㅻ깄??蹂듭궗                                       */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*  怨듦컻 API: Settings ?ㅻ깄??蹂듭궗                                              */
/* -------------------------------------------------------------------------- */

void APP_STATE_CopySettingsSnapshot(app_settings_t *dst)
{
    if (dst == 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  settings??ISR?먯꽌 媛깆떊?섏? ?딅뒗 ?뺤쟻 ?뺤콉 ??μ냼??                    */
    /* ---------------------------------------------------------------------- */
    memcpy(dst, (const void *)&g_app_state.settings, sizeof(*dst));
}

void APP_STATE_StoreSettingsSnapshot(const app_settings_t *src)
{
    if (src == 0)
    {
        return;
    }

    __disable_irq();
    memcpy((void *)&g_app_state.settings, (const void *)src, sizeof(*src));
    __enable_irq();
}

/* -------------------------------------------------------------------------- */
/*  怨듦컻 API: CLOCK ?꾩껜 ?ㅻ깄??蹂듭궗                                            */
/* -------------------------------------------------------------------------- */

void APP_STATE_CopyClockSnapshot(app_clock_state_t *dst)
{
    if (dst == 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  CLOCK slice??main loop??APP_CLOCK_Task()?먯꽌留?媛깆떊?쒕떎.              */
    /*  ?곕씪??snapshot 蹂듭궗??plain memcpy濡?異⑸텇?섎떎.                        */
    /* ---------------------------------------------------------------------- */
    memcpy((void *)dst, (const void *)&g_app_state.clock, sizeof(*dst));
}

/* -------------------------------------------------------------------------- */
/*  怨듦컻 API: ALTITUDE ?꾩껜 ?ㅻ깄??蹂듭궗                                         */
/* -------------------------------------------------------------------------- */
void APP_STATE_CopyAltitudeSnapshot(app_altitude_state_t *dst)
{
    if (dst == 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  altitude slice??APP_ALTITUDE_Task() main context?먯꽌留?媛깆떊?쒕떎.       */
    /*  ?곕씪??plain memcpy濡?異⑸텇?섎떎.                                        */
    /* ---------------------------------------------------------------------- */
    memcpy((void *)dst, (const void *)&g_app_state.altitude, sizeof(*dst));
}

/* -------------------------------------------------------------------------- */
/*  怨듦컻 API: BIKE DYNAMICS ?꾩껜 ?ㅻ깄??蹂듭궗                                    */
/* -------------------------------------------------------------------------- */
void APP_STATE_CopyBikeSnapshot(app_bike_state_t *dst)
{
    if (dst == 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  bike slice??main loop??BIKE_DYNAMICS_Task()?먯꽌留?媛깆떊?쒕떎.           */
    /*  future OBD service??main context?먯꽌 ?대떎???꾩젣瑜??먭퀬 plain memcpy    */
    /*  濡??좎??쒕떎. 留뚯빟 異뷀썑 ISR writer媛 ?앷린硫?洹몃븣留??꾧퀎援ъ뿭?쇰줈 諛붽씔??    */
    /* ---------------------------------------------------------------------- */
    memcpy((void *)dst, (const void *)&g_app_state.bike, sizeof(*dst));
}

/* -------------------------------------------------------------------------- */
/*  怨듦컻 API: GPS UI ?꾩슜 寃쎈웾 ?ㅻ깄??蹂듭궗                                      */
/* -------------------------------------------------------------------------- */

void APP_STATE_CopyGpsUiSnapshot(app_gps_ui_snapshot_t *dst)
{
    const volatile app_gps_state_t *src;
    uint8_t copied_sat_count;
    uint8_t i;

    if (dst == 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  湲?raw payload ?꾩껜媛 ?꾨땲??                                           */
    /*  ?붾㈃ ?뚮뜑???꾩슂???꾨뱶留?吏㏃? ?꾧퀎 援ъ뿭?먯꽌 蹂듭궗?쒕떎.                  */
    /* ---------------------------------------------------------------------- */
    __disable_irq();

    src = &g_app_state.gps;

    /* ?꾩튂/?띾룄/?뺥솗??*/
    dst->fix         = src->fix;
    dst->runtime_cfg = src->runtime_cfg;

    /* UART / parser 愿痢≪튂 */
    dst->uart_rx_running          = src->uart_rx_running;

    dst->rx_bytes                 = src->rx_bytes;
    dst->frames_ok                = src->frames_ok;
    dst->frames_bad_checksum      = src->frames_bad_checksum;
    dst->uart_ring_overflow_count = src->uart_ring_overflow_count;

    dst->uart_error_count         = src->uart_error_count;
    dst->uart_error_ore_count     = src->uart_error_ore_count;
    dst->uart_error_fe_count      = src->uart_error_fe_count;
    dst->uart_error_ne_count      = src->uart_error_ne_count;
    dst->uart_error_pe_count      = src->uart_error_pe_count;

    dst->rx_ring_level            = src->rx_ring_level;
    dst->rx_ring_high_watermark   = src->rx_ring_high_watermark;
    dst->last_rx_ms               = src->last_rx_ms;

    /* ?ㅼ뭅???뚮’???꾩꽦 紐⑸줉 */
    copied_sat_count = src->nav_sat_count;
    if (copied_sat_count > APP_GPS_MAX_SATS)
    {
        copied_sat_count = APP_GPS_MAX_SATS;
    }

    dst->nav_sat_count = copied_sat_count;

    for (i = 0u; i < copied_sat_count; i++)
    {
        dst->sats[i] = src->sats[i];
    }

    __enable_irq();
}

/* -------------------------------------------------------------------------- */
/*  怨듦컻 API: ?쇱꽌 ?붾쾭洹??섏씠吏 ?꾩슜 ?ㅻ깄??蹂듭궗                               */
/* -------------------------------------------------------------------------- */

void APP_STATE_CopySensorDebugSnapshot(app_sensor_debug_snapshot_t *dst)
{
    if (dst == 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  ?쇱꽌 ?섏씠吏??GPS ?꾩껜 state媛 ?꾩슂 ?놁쑝誘濡?                           */
    /*  GY-86 / DS18B20 ???⑹뼱由щ쭔 臾띠뼱??蹂듭궗?쒕떎.                            */
    /*                                                                        */
    /*  ??slice 紐⑤몢 main context?먯꽌留?媛깆떊?섎?濡?IRQ-off ?놁씠 蹂듭궗?쒕떎.      */
    /* ---------------------------------------------------------------------- */
    memcpy((void *)&dst->gy86,    (const void *)&g_app_state.gy86,    sizeof(dst->gy86));
    memcpy((void *)&dst->ds18b20, (const void *)&g_app_state.ds18b20, sizeof(dst->ds18b20));
}


/* -------------------------------------------------------------------------- */
/*  怨듦컻 API: SD ?꾩껜 ?ㅻ깄??蹂듭궗                                               */
/* -------------------------------------------------------------------------- */

void APP_STATE_CopySdSnapshot(app_sd_state_t *dst)
{
    if (dst == 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  SD detect IRQ???댁젣 runtime mailbox留?留뚯?怨?                          */
    /*  怨듦컻 ??μ냼(APP_STATE.sd)??main loop??APP_SD_Task()留?媛깆떊?쒕떎.       */
    /*  ?곕씪??snapshot 蹂듭궗??plain memcpy濡?異⑸텇?섎떎.                        */
    /* ---------------------------------------------------------------------- */
    memcpy((void *)dst, (const void *)&g_app_state.sd, sizeof(*dst));
}
