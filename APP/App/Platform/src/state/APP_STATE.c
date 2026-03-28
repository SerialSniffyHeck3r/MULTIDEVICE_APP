#include "main.h"
#include "APP_STATE.h"

#include <string.h>

/* -------------------------------------------------------------------------- */
/*  전역 APP_STATE 저장소                                                      */
/* -------------------------------------------------------------------------- */

volatile app_state_t g_app_state;

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 기본 사용자 설정 적용                                            */
/* -------------------------------------------------------------------------- */

static void APP_STATE_ApplyDefaultSettingsUnlocked(void)
{
    /* ---------------------------------------------------------------------- */
    /*  현재 프로젝트의 기본 GPS 설정은                                        */
    /*    - MULTI CONSTELLATION 10Hz                                           */
    /*    - HIGH POWER                                                         */
    /*  로 유지한다.                                                           */
    /* ---------------------------------------------------------------------- */
    g_app_state.settings.gps.boot_profile  = APP_GPS_BOOT_PROFILE_MULTI_CONST_10HZ;
    g_app_state.settings.gps.power_profile = APP_GPS_POWER_PROFILE_HIGH_POWER;

    /* ---------------------------------------------------------------------- */
    /*  시계 기본 정책은                                                       */
    /*    - timezone : KST(UTC+09:00)                                          */
    /*    - GPS auto sync : enabled                                             */
    /*    - GPS periodic time-only sync : 10 min                                */
    /*  으로 시작한다. 추후 UI/API에서 바꾸더라도 backup register에 따로        */
    /*  저장되므로 cold boot 기본값은 여기만 유지하면 된다.                    */
    /* ---------------------------------------------------------------------- */
    g_app_state.settings.clock.timezone_quarters = APP_CLOCK_TIMEZONE_QUARTERS_DEFAULT;
    g_app_state.settings.clock.gps_auto_sync_enabled = 1u;
    g_app_state.settings.clock.gps_sync_interval_minutes = APP_CLOCK_GPS_SYNC_INTERVAL_MIN_DEFAULT;
    g_app_state.settings.clock.reserved0 = 0u;

    /* ---------------------------------------------------------------------- */
        /*  백라이트 기본 정책                                                     */
        /*                                                                        */
        /*  기본값 의도                                                            */
        /*  - 부팅 직후에는 주변광을 연속 추종하는 AUTO-CONT 모드로 시작한다.      */
        /*  - bias는 0, smoothness는 3(중간값)으로 둔다.                          */
        /*  - AUTO-DIMMER의 존 값도 함께 기본값을 넣어 두어,                       */
        /*    나중에 모드만 바꿔도 즉시 동작하게 만든다.                           */
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
        /*  UC1608 기본 패널 값                                                    */
        /*                                                                        */
        /*  이 값들은 현재 코드/참조 시퀀스의 안전한 기본값을 APP_STATE에도         */
        /*  그대로 저장해 두는 용도다.                                            */
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
    /*  Altitude / vario 기본 정책                                             */
    /*                                                                        */
    /*  설계 철학                                                              */
    /*  - manual QNH와 GPS-equivalent QNH를 분리한다.                          */
    /*  - GPS absolute anchor와 IMU aid는 각각 별도 토글로 둔다.               */
    /*  - no-IMU / IMU 병렬 추정은 모두 항상 계산하되,                          */
    /*    주 표시용 선택만 imu_aid_enabled로 결정한다.                         */
    /* ---------------------------------------------------------------------- */
        /* ------------------------------------------------------------------ */
        /*  ALTITUDE 서비스 기본 설정                                           */
        /*                                                                    */
        /*  기본 철학                                                         */
        /*  - manual QNH / GPS equivalent QNH / fused altitude를 분리한다.    */
        /*  - baro는 빠르고 촘촘한 상대 채널, GPS는 느린 절대 anchor다.         */
        /*  - IMU-aided 결과는 항상 계산하되, 기본 표시 후보는 안전하게        */
        /*    no-IMU 쪽을 먼저 쓰도록 시작한다.                                */
        /*  - stationary burst 억제를 위해 rest display와 ZUPT를 켠다.        */
        /* ------------------------------------------------------------------ */
        g_app_state.settings.altitude.manual_qnh_hpa_x100            = 101325;
        g_app_state.settings.altitude.pressure_correction_hpa_x100   = 0;
               g_app_state.settings.altitude.gps_auto_equiv_qnh_enabled     = 1u;
               g_app_state.settings.altitude.gps_bias_correction_enabled    = 1u;
               g_app_state.settings.altitude.imu_aid_enabled                = 0u;
               g_app_state.settings.altitude.auto_home_capture_enabled      = 1u;

               /*  IMU sign + sensor poll debug gate 기본값                           */
               /*                                                                    */
               /*  imu_vertical_sign = +1                                             */
               /*  - 현재 기준 장착 방향에서 vertical specific-force 부호 기본값      */
               /*                                                                    */
               /*  imu_poll_enabled = 1                                               */
               /*  - MPU6050 polling은 평상시 기본 ON                                 */
               /*                                                                    */
               /*  mag_poll_enabled = 1                                               */
               /*  - HMC5883L raw polling을 기본 ON으로 둔다.                         */
               /*  - lean / grade 계산에는 직접 피드백하지 않고,                        */
               /*    self-test + 보조 heading 진단용 raw 확보에만 사용한다.            */
               /*                                                                    */
               /*  ms5611_only = 0                                                    */
               /*  - 평상시에는 강제 barometer-only 진단 모드를 끈 상태로 시작한다.   */
               /* ------------------------------------------------------------------ */
               g_app_state.settings.altitude.imu_vertical_sign              = 1;
               g_app_state.settings.altitude.imu_poll_enabled               = 1u;
               g_app_state.settings.altitude.mag_poll_enabled               = 1u;
               g_app_state.settings.altitude.ms5611_only                    = 0u;



               /* ------------------------------------------------------------------ */
               /*  pressure / vario / display 반응 속도                               */
               /*                                                                    */
               /*  pressure_lpf_tau_ms = 110ms                                        */
               /*  - raw pressure 잔떨림을 줄이되, baro 고유 응답성은 유지한다.       */
               /*                                                                    */
               /*  vario_fast_tau_ms = 160ms                                          */
               /*  - 오디오/즉응용 fast vario                                         */
               /*                                                                    */
               /*  vario_slow_tau_ms = 900ms                                          */
               /*  - 숫자 표시/경사도용 slow vario                                    */
               /*                                                                    */
               /*  display_lpf_tau_ms = 650ms                                         */
               /*  - stage-1 display altitude 기본 LPF                                */
               /*  - 이번 패치에서는 최종 UI presentation 계층이 한 번 더 있으므로     */
               /*    기존 450ms보다 약간만 더 눌러서 제품 느낌을 만든다.              */
               /* ------------------------------------------------------------------ */
               g_app_state.settings.altitude.pressure_lpf_tau_ms            = 110u;
               g_app_state.settings.altitude.vario_fast_tau_ms              = 160u;
               g_app_state.settings.altitude.vario_slow_tau_ms              = 900u;
               g_app_state.settings.altitude.display_lpf_tau_ms             = 650u;

               /* ------------------------------------------------------------------ */
               /*  정지 상태 display 안정화 + ZUPT 기본값                             */
               /*                                                                    */
               /*  rest_detect_vario_cms  = 0.12m/s                                   */
               /*  rest_detect_accel_mg   = 15mg                                       */
               /*  rest_display_tau_ms    = 2.6s                                       */
               /*  rest_display_hold_cm   = ±20cm                                      */
               /*  zupt_enabled           = 1                                          */
               /*                                                                    */
               /*  주의                                                               */
               /*  - rest_detect_* 는 ZUPT 판정과도 공유되므로 그대로 둔다.           */
               /*  - 이번 변경은 "정지로 판정된 뒤 숫자를 얼마나 오래 붙잡을지"만     */
               /*    더 제품화하는 조정이다.                                          */
               /* ------------------------------------------------------------------ */
               g_app_state.settings.altitude.rest_display_enabled           = 1u;
               g_app_state.settings.altitude.zupt_enabled                   = 1u;
               g_app_state.settings.altitude.reserved_rest0                 = 0u;
               g_app_state.settings.altitude.rest_detect_vario_cms          = 12u;
               g_app_state.settings.altitude.rest_detect_accel_mg           = 15u;
               g_app_state.settings.altitude.rest_display_tau_ms            = 2600u;
               g_app_state.settings.altitude.rest_display_hold_cm           = 20u;

               /* ------------------------------------------------------------------ */
               /*  baro / GPS gate 및 measurement noise                               */
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
               /*  baro_vario_lpf_tau_ms            = 80ms                            */
               /*  baro_vario_measurement_noise_cms = 0.65m/s                         */
               /*                                                                    */
               /*  regression slope로 만든 velocity 관측은                            */
               /*  정지 bench에서도 가장 시끄러운 경로이므로                           */
               /*  nominal R를 기존보다 보수적으로 높여                               */
               /*  small pressure jitter를 덜 믿게 만든다.                             */
               /*                                                                    */
               /*  이 값은 regression slope를 velocity measurement로 사용할 때        */
               /*  얼마나 매끈하게 / 얼마나 신뢰할지 정한다.                           */
               /* ------------------------------------------------------------------ */
               g_app_state.settings.altitude.baro_vario_lpf_tau_ms            = 80u;
               g_app_state.settings.altitude.baro_vario_measurement_noise_cms = 65u;

                /* ------------------------------------------------------------------ */
                /*  IMU vertical estimate 기본값                                       */
                /*                                                                    */
                /*  imu_gravity_tau_ms            = 700ms                              */
                /*  imu_accel_tau_ms              = 180ms                              */
                /*  imu_accel_lsb_per_g           = 16384 (MPU6050 ±2g)                */
                /*  imu_vertical_deadband_mg      = 12mg                               */
                /*  imu_vertical_clip_mg          = 450mg                              */
                /*  imu_measurement_noise_cms2    = 160cm/s²                           */
                /*  imu_gyro_lsb_per_dps          = 131 (MPU6050 ±250dps)              */
                /*  imu_attitude_accel_gate_mg    = 80mg                               */
                /*  imu_predict_min_trust_permille= 600                                */
                /*                                                                    */
                /*  stationary burst가 보이면                                           */
                /*  1) I_TMIN을 올리고                                                  */
                /*  2) ATT_AG를 낮추고                                                  */
                /*  3) A_TAU를 조금 늘려라.                                             */
                /* ------------------------------------------------------------------ */
                g_app_state.settings.altitude.imu_gravity_tau_ms             = 700u;
                g_app_state.settings.altitude.imu_accel_tau_ms               = 180u;
                g_app_state.settings.altitude.imu_accel_lsb_per_g            = 16384u;
                g_app_state.settings.altitude.imu_vertical_deadband_mg       = 12u;
                g_app_state.settings.altitude.imu_vertical_clip_mg           = 450u;
                g_app_state.settings.altitude.imu_measurement_noise_cms2     = 160u;
                g_app_state.settings.altitude.imu_gyro_lsb_per_dps           = 131u;
                g_app_state.settings.altitude.imu_attitude_accel_gate_mg     = 80u;
                g_app_state.settings.altitude.imu_predict_min_trust_permille = 600u;

        /* ------------------------------------------------------------------ */
        /*  Kalman process noise 기본값                                        */
        /*                                                                    */
        /*  Q_h : 고도 상태가 스스로 퍼지는 정도                               */
        /*  Q_v : 속도 상태가 스스로 퍼지는 정도                               */
        /*  Q_b : baro bias가 천천히 움직이는 정도                              */
        /*  Q_a : accel bias가 천천히 움직이는 정도                             */
        /* ------------------------------------------------------------------ */
        g_app_state.settings.altitude.kf_q_height_cm_per_s           = 5u;
        g_app_state.settings.altitude.kf_q_velocity_cms_per_s        = 60u;
        g_app_state.settings.altitude.kf_q_baro_bias_cm_per_s        = 2u;
        g_app_state.settings.altitude.kf_q_accel_bias_cms2_per_s     = 20u;

        /* ------------------------------------------------------------------ */
        /*  ALTITUDE debug page 전용 vario audio 기본값                        */
        /*                                                                    */
        /*  audio_repeat_ms / audio_beep_ms 는                                 */
        /*  climb cadence의 기준값일 뿐이고,                                    */
        /*  실제 음정은 fast vario 수치가 실시간으로 FM modulation 한다.         */
        /*                                                                    */
        /*  이번 기본값은                                                       */
        /*  - 지나치게 촘촘한 beep 반복을 약간 늦추고                           */
        /*  - single beep 길이를 조금 늘려                                      */
        /*  continuous oscillator 기반 tone이                                   */
        /*  더 자연스럽게 들리도록 맞춘 값이다.                                 */
        /* ------------------------------------------------------------------ */
        g_app_state.settings.altitude.debug_audio_enabled            = 1u;
        g_app_state.settings.altitude.debug_audio_source             = 0u;
        g_app_state.settings.altitude.audio_deadband_cms             = 35u;
        g_app_state.settings.altitude.audio_min_freq_hz              = 700u;
        g_app_state.settings.altitude.audio_max_freq_hz              = 2200u;
        g_app_state.settings.altitude.audio_repeat_ms                = 170u;
        g_app_state.settings.altitude.audio_beep_ms                  = 65u;

        /* ---------------------------------------------------------------------- */
        /*  BIKE DYNAMICS 기본 설정                                                 */
        /*                                                                        */
        /*  기본 장착 가정                                                         */
        /*  - sensor +X : 차량 forward                                             */
        /*  - sensor +Y : 차량 left                                                */
        /*  - sensor +Z : 차량 up                                                  */
        /*                                                                        */
        /*  현재 GY86_IMU driver 기준 scale                                         */
        /*  - accel ±4g     -> 8192 LSB/g                                           */
        /*  - gyro  ±500dps -> 65.5 LSB/dps -> x10 scale로 655                      */
        /*                                                                        */
        /*  필터 성격                                                               */
        /*  - gravity_tau 700ms   : lean/grade 기준축은 너무 민감하지 않게          */
        /*  - linear_tau  120ms   : lat/lon 가속도는 진동을 약간 누르되             */
        /*                            투어링 컴퓨터 응답성은 유지                   */
        /*  - GNSS bias tau 4.0s  : GNSS는 저주파 anchor로만 천천히 먹인다.        */
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
/*  내부 유틸: GPS 저장소 초기화                                                */
/* -------------------------------------------------------------------------- */

static void APP_STATE_ResetGpsUnlocked(void)
{
    memset((void *)&g_app_state.gps, 0, sizeof(g_app_state.gps));
    g_app_state.gps.initialized = true;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: CLOCK 저장소 초기화                                              */
/* -------------------------------------------------------------------------- */
static void APP_STATE_ResetClockUnlocked(void)
{
    memset((void *)&g_app_state.clock, 0, sizeof(g_app_state.clock));

    /* ---------------------------------------------------------------------- */
    /*  APP_STATE.clock 는 "RTC에서 실제로 읽은 raw/runtime" 저장소다.          */
    /*  초기값 단계에서는 settings의 clock 기본 정책을 그대로 반영해 둔다.     */
    /*  이후 APP_CLOCK_Init()가 backup register를 읽어 최종 runtime 값을 채운다. */
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
/*  내부 유틸: GY-86 저장소 초기화                                              */
/* -------------------------------------------------------------------------- */

static void APP_STATE_ResetGy86Unlocked(void)
{
    memset((void *)&g_app_state.gy86, 0, sizeof(g_app_state.gy86));

    /* ---------------------------------------------------------------------- */
    /*  backend ID / 디폴트 주기 같은 값은                                    */
    /*  드라이버 init에서도 다시 채우지만,                                     */
    /*  자료창고 초기 상태도 명시적으로 보이게 여기서 0/기본값을 넣어 둔다.    */
    /* ---------------------------------------------------------------------- */
    g_app_state.gy86.initialized = false;
    g_app_state.gy86.status_flags = 0u;
    g_app_state.gy86.last_update_ms = 0u;

    g_app_state.gy86.debug.accelgyro_backend_id = APP_IMU_BACKEND_NONE;
    g_app_state.gy86.debug.mag_backend_id       = APP_IMU_BACKEND_NONE;
    g_app_state.gy86.debug.baro_backend_id      = APP_IMU_BACKEND_NONE;

    /* ---------------------------------------------------------------------- */
    /*  dual-baro 디버그 슬롯 기본값                                           */
    /*                                                                        */
    /*  실제 configured/online/valid 값은                                      */
    /*  low-level driver init이 끝난 뒤 GY86_IMU.c 가 채운다.                 */
    /*  여기서는 slot 개수와 invalid primary index sentinel 만 미리 넣는다.   */
    /* ---------------------------------------------------------------------- */
    g_app_state.gy86.debug.baro_device_slots        = APP_GY86_BARO_SENSOR_SLOTS;
    g_app_state.gy86.debug.baro_primary_sensor_index = 0xFFu;

    g_app_state.gy86.debug.mpu_poll_period_ms  = 0u;
    g_app_state.gy86.debug.mag_poll_period_ms  = 0u;
    g_app_state.gy86.debug.baro_poll_period_ms = 0u;

}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: DS18B20 저장소 초기화                                            */
/* -------------------------------------------------------------------------- */

static void APP_STATE_ResetDs18b20Unlocked(void)
{
    memset((void *)&g_app_state.ds18b20, 0, sizeof(g_app_state.ds18b20));

    /* ---------------------------------------------------------------------- */
    /*  온도는 "0" 과 "아직 읽지 못함" 을 구분하기 위해                         */
    /*  invalid sentinel 값을 명시적으로 넣는다.                               */
    /* ---------------------------------------------------------------------- */
    g_app_state.ds18b20.initialized = false;
    g_app_state.ds18b20.status_flags = 0u;
    g_app_state.ds18b20.last_update_ms = 0u;

    g_app_state.ds18b20.raw.temp_c_x100 = APP_DS18B20_TEMP_INVALID;
    g_app_state.ds18b20.raw.temp_f_x100 = APP_DS18B20_TEMP_INVALID;

    g_app_state.ds18b20.debug.phase = APP_DS18B20_PHASE_UNINIT;
    g_app_state.ds18b20.debug.last_error = APP_DS18B20_ERR_NONE;

    /* 12-bit 기본 목표값 */
    g_app_state.ds18b20.debug.conversion_time_ms = 750u;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 밝기 센서 저장소 초기화                                          */
/* -------------------------------------------------------------------------- */

static void APP_STATE_ResetBrightnessUnlocked(void)
{
    memset((void *)&g_app_state.brightness, 0, sizeof(g_app_state.brightness));

    g_app_state.brightness.initialized = false;
    g_app_state.brightness.valid       = false;
    g_app_state.brightness.last_update_ms = 0u;
    g_app_state.brightness.sample_count   = 0u;

    /* ---------------------------------------------------------------------- */
    /*  ADC 드라이버가 아직 안 올라오기 전 상태를 구분하기 위해                 */
    /*  last_hal_status는 0xFF sentinel로 둔다.                                */
    /* ---------------------------------------------------------------------- */
    g_app_state.brightness.debug.last_hal_status = 0xFFu;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: Audio 저장소 초기화                                             */
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
    /*  STM32F4 DAC는 실제 아날로그 출력 분해능이 12bit 이다.                  */
    /*  source WAV가 16/24/32bit 이어도 최종 DAC 단계에서는 12bit로 나간다.    */
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
/*  내부 유틸: Bluetooth 저장소 초기화                                          */
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
    /*  아직 RX/TX가 일어나지 않았음을 구분하기 위해                            */
    /*  HAL status raw는 0xFF sentinel로 둔다.                                 */
    /* ---------------------------------------------------------------------- */
    g_app_state.bluetooth.last_hal_status_rx     = 0xFFu;
    g_app_state.bluetooth.last_hal_status_tx     = 0xFFu;
    g_app_state.bluetooth.last_hal_error         = 0u;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: DEBUG UART 저장소 초기화                                         */
/* -------------------------------------------------------------------------- */

static void APP_STATE_ResetDebugUartUnlocked(void)
{
    memset((void *)&g_app_state.debug_uart, 0, sizeof(g_app_state.debug_uart));

    g_app_state.debug_uart.initialized     = false;
    g_app_state.debug_uart.last_hal_status = 0xFFu;
    g_app_state.debug_uart.last_tx_ms      = 0u;
    g_app_state.debug_uart.tx_count        = 0u;
    g_app_state.debug_uart.tx_bytes        = 0u;
    g_app_state.debug_uart.tx_fail_count   = 0u;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: SD / FATFS 저장소 초기화                                         */
/* -------------------------------------------------------------------------- */

static void APP_STATE_ResetSdUnlocked(void)
{
    memset((void *)&g_app_state.sd, 0, sizeof(g_app_state.sd));

    /* ---------------------------------------------------------------------- */
    /*  SD 쪽은 "아직 시도하지 않음" 과 "0" 을 구분하기 위해                    */
    /*  몇몇 raw 상태 값에 sentinel 을 넣어 둔다.                              */
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
/*  내부 유틸: ALTITUDE 저장소 초기화                                          */
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
    /*  future OBD 입력 필드는 reset 시에만 0으로 둔다.                         */
    /*  추후 OBD service가 이 필드들을 다시 채우면 BIKE_DYNAMICS가 읽는다.      */
    /* ---------------------------------------------------------------------- */
    g_app_state.bike.obd_input_speed_valid       = false;
    g_app_state.bike.obd_input_speed_mmps        = 0u;
    g_app_state.bike.obd_input_last_update_ms    = 0u;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: GPS slice만 초기화                                                */
/* -------------------------------------------------------------------------- */

void APP_STATE_ResetGps(void)
{
    __disable_irq();
    APP_STATE_ResetGpsUnlocked();
    __enable_irq();
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 전체 APP_STATE 초기화                                             */
/* -------------------------------------------------------------------------- */

void APP_STATE_Init(void)
{
    __disable_irq();

    /* ---------------------------------------------------------------------- */
    /*  자료창고 전체를 0으로 리셋한 뒤                                         */
    /*  "0이 아닌 의미값" 이 필요한 항목들만 별도 helper로 복구한다.           */
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
    APP_STATE_ResetDebugUartUnlocked();
    APP_STATE_ResetSdUnlocked();
    APP_STATE_ResetAltitudeUnlocked();
    APP_STATE_ResetBikeUnlocked();

    __enable_irq();
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 전체 스냅샷 복사                                                  */
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
/*  공개 API: GPS 전체 스냅샷 복사                                              */
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
/*  공개 API: GY-86 전체 스냅샷 복사                                            */
/* -------------------------------------------------------------------------- */

void APP_STATE_CopyGy86Snapshot(app_gy86_state_t *dst)
{
    if (dst == 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  GY-86 slice는 main context에서만 갱신된다.                               */
    /*  따라서 snapshot 복사를 위해 IRQ를 막을 필요가 없다.                     */
    /* ---------------------------------------------------------------------- */
    memcpy(dst, (const void *)&g_app_state.gy86, sizeof(*dst));
}

/* -------------------------------------------------------------------------- */
/*  공개 API: DS18B20 전체 스냅샷 복사                                          */
/* -------------------------------------------------------------------------- */

void APP_STATE_CopyDs18b20Snapshot(app_ds18b20_state_t *dst)
{
    if (dst == 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  DS18B20 slice도 bit-bang timing은 내부에서 처리하지만,                   */
    /*  공개 저장소 갱신은 main context에서만 수행한다.                         */
    /* ---------------------------------------------------------------------- */
    memcpy(dst, (const void *)&g_app_state.ds18b20, sizeof(*dst));
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 밝기 센서 전체 스냅샷 복사                                        */
/* -------------------------------------------------------------------------- */

void APP_STATE_CopyBrightnessSnapshot(app_brightness_state_t *dst)
{
    if (dst == 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  밝기 센서 slice는 main loop state machine만 갱신한다.                   */
    /*  따라서 snapshot 복사는 plain memcpy로 충분하다.                        */
    /* ---------------------------------------------------------------------- */
    memcpy(dst, (const void *)&g_app_state.brightness, sizeof(*dst));
}

/* -------------------------------------------------------------------------- */
/*  공개 API: Audio 전체 스냅샷 복사                                            */
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
/*  공개 API: Bluetooth 전체 스냅샷 복사                                        */
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
/*  공개 API: DEBUG UART 전체 스냅샷 복사                                       */
/* -------------------------------------------------------------------------- */

void APP_STATE_CopyDebugUartSnapshot(app_debug_uart_state_t *dst)
{
    if (dst == 0)
    {
        return;
    }

    __disable_irq();
    memcpy(dst, (const void *)&g_app_state.debug_uart, sizeof(*dst));
    __enable_irq();
}

/* -------------------------------------------------------------------------- */
/*  공개 API: Settings 스냅샷 복사                                              */
/* -------------------------------------------------------------------------- */

void APP_STATE_CopySettingsSnapshot(app_settings_t *dst)
{
    if (dst == 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  settings는 ISR에서 갱신하지 않는 정적 정책 저장소다.                    */
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
/*  공개 API: CLOCK 전체 스냅샷 복사                                            */
/* -------------------------------------------------------------------------- */

void APP_STATE_CopyClockSnapshot(app_clock_state_t *dst)
{
    if (dst == 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  CLOCK slice는 main loop의 APP_CLOCK_Task()에서만 갱신된다.              */
    /*  따라서 snapshot 복사는 plain memcpy로 충분하다.                        */
    /* ---------------------------------------------------------------------- */
    memcpy((void *)dst, (const void *)&g_app_state.clock, sizeof(*dst));
}

/* -------------------------------------------------------------------------- */
/*  공개 API: ALTITUDE 전체 스냅샷 복사                                         */
/* -------------------------------------------------------------------------- */
void APP_STATE_CopyAltitudeSnapshot(app_altitude_state_t *dst)
{
    if (dst == 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  altitude slice는 APP_ALTITUDE_Task() main context에서만 갱신된다.       */
    /*  따라서 plain memcpy로 충분하다.                                        */
    /* ---------------------------------------------------------------------- */
    memcpy((void *)dst, (const void *)&g_app_state.altitude, sizeof(*dst));
}

/* -------------------------------------------------------------------------- */
/*  공개 API: BIKE DYNAMICS 전체 스냅샷 복사                                    */
/* -------------------------------------------------------------------------- */
void APP_STATE_CopyBikeSnapshot(app_bike_state_t *dst)
{
    if (dst == 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  bike slice는 main loop의 BIKE_DYNAMICS_Task()에서만 갱신된다.           */
    /*  future OBD service도 main context에서 쓴다는 전제를 두고 plain memcpy    */
    /*  로 유지한다. 만약 추후 ISR writer가 생기면 그때만 임계구역으로 바꾼다.    */
    /* ---------------------------------------------------------------------- */
    memcpy((void *)dst, (const void *)&g_app_state.bike, sizeof(*dst));
}

/* -------------------------------------------------------------------------- */
/*  공개 API: GPS UI 전용 경량 스냅샷 복사                                      */
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
    /*  긴 raw payload 전체가 아니라                                            */
    /*  화면 렌더에 필요한 필드만 짧은 임계 구역에서 복사한다.                  */
    /* ---------------------------------------------------------------------- */
    __disable_irq();

    src = &g_app_state.gps;

    /* 위치/속도/정확도 */
    dst->fix         = src->fix;
    dst->runtime_cfg = src->runtime_cfg;

    /* UART / parser 관측치 */
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

    /* 스카이 플롯용 위성 목록 */
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
/*  공개 API: 센서 디버그 페이지 전용 스냅샷 복사                               */
/* -------------------------------------------------------------------------- */

void APP_STATE_CopySensorDebugSnapshot(app_sensor_debug_snapshot_t *dst)
{
    if (dst == 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  센서 페이지는 GPS 전체 state가 필요 없으므로                            */
    /*  GY-86 / DS18B20 두 덩어리만 묶어서 복사한다.                            */
    /*                                                                        */
    /*  두 slice 모두 main context에서만 갱신되므로 IRQ-off 없이 복사한다.      */
    /* ---------------------------------------------------------------------- */
    memcpy((void *)&dst->gy86,    (const void *)&g_app_state.gy86,    sizeof(dst->gy86));
    memcpy((void *)&dst->ds18b20, (const void *)&g_app_state.ds18b20, sizeof(dst->ds18b20));
}


/* -------------------------------------------------------------------------- */
/*  공개 API: SD 전체 스냅샷 복사                                               */
/* -------------------------------------------------------------------------- */

void APP_STATE_CopySdSnapshot(app_sd_state_t *dst)
{
    if (dst == 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  SD detect IRQ는 이제 runtime mailbox만 만지고,                          */
    /*  공개 저장소(APP_STATE.sd)는 main loop의 APP_SD_Task()만 갱신한다.       */
    /*  따라서 snapshot 복사는 plain memcpy로 충분하다.                        */
    /* ---------------------------------------------------------------------- */
    memcpy((void *)dst, (const void *)&g_app_state.sd, sizeof(*dst));
}
