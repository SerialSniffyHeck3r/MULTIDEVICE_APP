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
    APP_STATE_ResetGy86Unlocked();
    APP_STATE_ResetDs18b20Unlocked();
    APP_STATE_ResetBrightnessUnlocked();
    APP_STATE_ResetAudioUnlocked();
    APP_STATE_ResetBluetoothUnlocked();
    APP_STATE_ResetDebugUartUnlocked();
    APP_STATE_ResetSdUnlocked();

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
