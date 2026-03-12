#include "Audio_Driver.h"

#include <ctype.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

#ifndef AUDIO_DAC_MIDPOINT_U12
#define AUDIO_DAC_MIDPOINT_U12 2048u
#endif

#ifndef AUDIO_GATE_PERCENT_DEFAULT
#define AUDIO_GATE_PERCENT_DEFAULT 85u
#endif

#ifndef AUDIO_WAV_FULL_PATH_MAX
#define AUDIO_WAV_FULL_PATH_MAX 96u
#endif

#ifndef AUDIO_SOFT_CLIP_KNEE_S16
#define AUDIO_SOFT_CLIP_KNEE_S16 24576
#endif

#ifndef AUDIO_DAC_OUTPUT_BITS
#define AUDIO_DAC_OUTPUT_BITS 12u
#endif

#ifndef AUDIO_SW_FIFO_SAMPLES
#define AUDIO_SW_FIFO_SAMPLES 8192u
#endif

#ifndef AUDIO_SW_FIFO_LOW_WATERMARK_SAMPLES
#define AUDIO_SW_FIFO_LOW_WATERMARK_SAMPLES 2048u
#endif

#ifndef AUDIO_SW_FIFO_HIGH_WATERMARK_SAMPLES
#define AUDIO_SW_FIFO_HIGH_WATERMARK_SAMPLES 6144u
#endif

#ifndef AUDIO_PRODUCER_CHUNK_SAMPLES
#define AUDIO_PRODUCER_CHUNK_SAMPLES 512u
#endif

#ifndef AUDIO_PRODUCER_MAX_BLOCKS_PER_TASK
#define AUDIO_PRODUCER_MAX_BLOCKS_PER_TASK 8u
#endif

#define AUDIO_SW_FIFO_MASK (AUDIO_SW_FIFO_SAMPLES - 1u)

/* -------------------------------------------------------------------------- */
/*  외부 CubeMX handle                                                         */
/*                                                                            */
/*  사용자는 이미 DAC1 + TIM6 + DMA를 IOC에 추가할 예정이므로                    */
/*  이 드라이버는 "해당 handle이 이미 존재한다" 를 전제로 작성한다.             */
/* -------------------------------------------------------------------------- */
extern DAC_HandleTypeDef AUDIO_DAC_HANDLE;
extern TIM_HandleTypeDef AUDIO_TIMER_HANDLE;

/* -------------------------------------------------------------------------- */
/*  내부 runtime 타입                                                          */
/* -------------------------------------------------------------------------- */
typedef struct
{
    uint8_t  active;
    uint8_t  track_index;
    uint8_t  timbre_id;
    uint8_t  env_phase;

    const audio_timbre_preset_t *timbre;

    uint32_t note_hz_x100;
    uint32_t phase_q32;
    uint32_t phase_inc_q32;

    uint32_t note_samples_total;
    uint32_t note_samples_elapsed;
    uint32_t gate_samples;

    uint32_t attack_samples;
    uint32_t decay_samples;
    uint32_t release_samples;

    uint16_t sustain_level_q15;
    uint16_t env_level_q15;
    uint16_t velocity_q15;
    uint16_t attack_inc_q15;
    uint16_t decay_dec_q15;
    uint16_t release_dec_q15;
} audio_voice_runtime_t;

typedef struct
{
    uint8_t  active;
    uint8_t  mono_mode;
    uint8_t  melody_track_index;
    uint8_t  reserved0;

    uint16_t bpm;
    uint16_t current_note_index[AUDIO_PRESET_MAX_TRACKS];

    uint32_t samples_until_next_event[AUDIO_PRESET_MAX_TRACKS];

    const audio_note_preset_t *note_preset;
    const audio_timbre_stack_preset_t *stack_preset;
    audio_timbre_preset_id_t mono_timbre_id;
} audio_sequence_runtime_t;

typedef struct
{
    uint8_t  active;
    uint8_t  native_rate_mode;
    uint8_t  sample_pair_valid;
    uint8_t  end_of_stream_pending;

    uint16_t channels;
    uint16_t bits_per_sample;
    uint16_t bytes_per_sample;
    uint16_t block_align;

    uint32_t source_sample_rate_hz;
    uint32_t output_sample_rate_hz;
    uint32_t data_start_offset;
    uint32_t data_bytes_remaining;

    uint64_t resample_phase_q32;
    uint64_t resample_step_q32;

    int16_t  current_sample_s16;
    int16_t  next_sample_s16;

    uint8_t  read_buffer[AUDIO_WAV_STREAM_READ_CHUNK];
    uint32_t read_index;
    uint32_t read_valid;

    FIL      file;

    char     display_name[APP_AUDIO_NAME_MAX];
    char     full_path[AUDIO_WAV_FULL_PATH_MAX];
} audio_wav_runtime_t;

typedef struct
{
    uint8_t  initialized;
    uint8_t  volume_table_ready;

    uint32_t active_output_sample_rate_hz;
    uint16_t current_volume_q15;
    uint16_t current_output_gain_q15;
    uint16_t volume_table_q15[101u];

    uint16_t dma_buffer[AUDIO_DMA_BUFFER_SAMPLES];

    /* ---------------------------------------------------------------------- */
    /*  software PCM FIFO                                                      */
    /*                                                                        */
    /*  main context가 미리 만들어 둔 DAC 샘플(U12)을 쌓아 두고,               */
    /*  DMA half/full callback은 여기서 반쪽 버퍼 분량만 빠르게 꺼내 간다.     */
    /*                                                                        */
    /*  single-producer(main) / single-consumer(ISR) 구조이므로                */
    /*  read/write index를 분리해서 lock-free로 운용한다.                      */
    /* ---------------------------------------------------------------------- */
    uint16_t pcm_fifo[AUDIO_SW_FIFO_SAMPLES];
    volatile uint32_t pcm_fifo_read_index;
    volatile uint32_t pcm_fifo_write_index;
    uint32_t pcm_fifo_peak_level_samples;

    /* main context render scratch block */
    uint16_t producer_block[AUDIO_PRODUCER_CHUNK_SAMPLES];

    audio_voice_runtime_t    voices[APP_AUDIO_MAX_VOICES];
    audio_sequence_runtime_t sequence;
    audio_wav_runtime_t      wav;
} audio_driver_runtime_t;

static audio_driver_runtime_t s_audio_rt;

/* -------------------------------------------------------------------------- */
/*  forward declaration                                                         */
/*                                                                            */
/*  FIFO service helper가 아래의 silence fill helper를 먼저 쓰므로              */
/*  정적 prototype를 여기에서 한 번 선언해 둔다.                               */
/* -------------------------------------------------------------------------- */
static void Audio_Driver_FillBufferWithSilence(uint16_t *dst, uint32_t sample_count);

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 문자열 복사                                                     */
/* -------------------------------------------------------------------------- */
static void Audio_Driver_CopyTextSafe(char *dst, size_t dst_size, const char *src)
{
    size_t copy_len;

    if ((dst == 0) || (dst_size == 0u))
    {
        return;
    }

    dst[0] = '\0';

    if (src == 0)
    {
        return;
    }

    copy_len = strlen(src);
    if (copy_len >= dst_size)
    {
        copy_len = dst_size - 1u;
    }

    memcpy(dst, src, copy_len);
    dst[copy_len] = '\0';
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 출력 sample rate clamp                                          */
/*                                                                            */
/*  너무 낮거나 너무 높은 sample rate를 그대로 쓰면                            */
/*  품질/CPU 모두 불리해질 수 있으므로                                         */
/*  driver 내부에서 허용 범위로 정리한다.                                      */
/* -------------------------------------------------------------------------- */
static uint32_t Audio_Driver_ClampOutputSampleRate(uint32_t requested_hz)
{
    if (requested_hz < AUDIO_OUTPUT_SAMPLE_RATE_MIN_HZ)
    {
        return AUDIO_OUTPUT_SAMPLE_RATE_MIN_HZ;
    }

    if (requested_hz > AUDIO_OUTPUT_SAMPLE_RATE_MAX_HZ)
    {
        return AUDIO_OUTPUT_SAMPLE_RATE_MAX_HZ;
    }

    return requested_hz;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 현재 활성 출력 sample rate                                      */
/* -------------------------------------------------------------------------- */
static uint32_t Audio_Driver_GetActiveOutputSampleRate(void)
{
    if (s_audio_rt.active_output_sample_rate_hz == 0u)
    {
        return Audio_Driver_ClampOutputSampleRate(AUDIO_SAMPLE_RATE_HZ);
    }

    return s_audio_rt.active_output_sample_rate_hz;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: software FIFO reset                                             */
/*                                                                            */
/*  sample rate 변경, content stop, transport restart 시에는                   */
/*  이전 rate/content에서 만들어 둔 샘플을 절대 재사용하면 안 된다.            */
/*  그래서 read/write index를 함께 0으로 되돌려 FIFO를 비운다.                */
/* -------------------------------------------------------------------------- */
static void Audio_Driver_FifoReset(void)
{
    s_audio_rt.pcm_fifo_read_index = 0u;
    s_audio_rt.pcm_fifo_write_index = 0u;
    s_audio_rt.pcm_fifo_peak_level_samples = 0u;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: FIFO 현재 수위                                                  */
/*                                                                            */
/*  unsigned wrap-around subtraction을 사용하므로,                             */
/*  index가 32-bit를 한 바퀴 돌아도 FIFO depth가 2^31보다 훨씬 작기만 하면    */
/*  차분은 계속 올바르게 유지된다.                                             */
/* -------------------------------------------------------------------------- */
static uint32_t Audio_Driver_FifoLevelSamples(void)
{
    return (uint32_t)(s_audio_rt.pcm_fifo_write_index - s_audio_rt.pcm_fifo_read_index);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: FIFO 남은 여유 공간                                              */
/* -------------------------------------------------------------------------- */
static uint32_t Audio_Driver_FifoFreeSamples(void)
{
    uint32_t level;

    level = Audio_Driver_FifoLevelSamples();
    if (level >= AUDIO_SW_FIFO_SAMPLES)
    {
        return 0u;
    }

    return (AUDIO_SW_FIFO_SAMPLES - level);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: FIFO telemetry publish                                          */
/*                                                                            */
/*  APP_STATE.audio는 저수준 raw 창고이므로,                                   */
/*  현재 FIFO 수위와 watermark도 거기에 그대로 적재해 둔다.                    */
/* -------------------------------------------------------------------------- */
static void Audio_Driver_PublishFifoTelemetry(void)
{
    app_audio_state_t *audio;
    uint32_t level;

    audio = (app_audio_state_t *)&g_app_state.audio;
    level = Audio_Driver_FifoLevelSamples();

    if (level > s_audio_rt.pcm_fifo_peak_level_samples)
    {
        s_audio_rt.pcm_fifo_peak_level_samples = level;
    }

    audio->sw_fifo_capacity_samples       = AUDIO_SW_FIFO_SAMPLES;
    audio->sw_fifo_level_samples          = level;
    audio->sw_fifo_peak_level_samples     = s_audio_rt.pcm_fifo_peak_level_samples;
    audio->sw_fifo_low_watermark_samples  = AUDIO_SW_FIFO_LOW_WATERMARK_SAMPLES;
    audio->sw_fifo_high_watermark_samples = AUDIO_SW_FIFO_HIGH_WATERMARK_SAMPLES;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 현재 main context가 더 만들 수 있는 실제 content가 있는가        */
/*                                                                            */
/*  FIFO tail만 남아 있는 상태와,                                               */
/*  앞으로도 계속 sample을 더 만들어 낼 수 있는 상태를 구분하려고 쓴다.        */
/* -------------------------------------------------------------------------- */
static uint8_t Audio_Driver_HasRenderableContent(void)
{
    uint32_t voice_index;

    if (s_audio_rt.sequence.active != 0u)
    {
        return 1u;
    }

    if (s_audio_rt.wav.active != 0u)
    {
        return 1u;
    }

    for (voice_index = 0u; voice_index < APP_AUDIO_MAX_VOICES; voice_index++)
    {
        if (s_audio_rt.voices[voice_index].active != 0u)
        {
            return 1u;
        }
    }

    return 0u;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: pipeline busy/idle 상태 갱신                                    */
/*                                                                            */
/*  중요한 점                                                                  */
/*  - source(sequence/voice/WAV)가 이미 끝났더라도 FIFO tail이 남아 있으면      */
/*    실제 스피커로는 아직 소리가 나가고 있을 수 있다.                         */
/*  - 그래서 busy 판단은 "source active OR FIFO non-empty" 로 잡는다.         */
/* -------------------------------------------------------------------------- */
static void Audio_Driver_UpdatePipelineFlags(uint32_t now_ms)
{
    app_audio_state_t *audio;

    audio = (app_audio_state_t *)&g_app_state.audio;
    Audio_Driver_PublishFifoTelemetry();

    if ((Audio_Driver_HasRenderableContent() != 0u) ||
        (Audio_Driver_FifoLevelSamples() != 0u))
    {
        audio->content_active = true;
        return;
    }

    if (audio->content_active != false)
    {
        audio->playback_stop_ms = now_ms;
    }

    audio->content_active = false;
    audio->mode = APP_AUDIO_MODE_IDLE;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: FIFO push                                                       */
/*                                                                            */
/*  producer(main context)가 완성된 DAC sample block을 FIFO에 적재한다.        */
/*  write index는 실제 메모리 write가 끝난 뒤에만 publish 한다.                */
/* -------------------------------------------------------------------------- */
static uint32_t Audio_Driver_FifoPushSamples(const uint16_t *src,
                                             uint32_t sample_count)
{
    uint32_t free_samples;
    uint32_t write_index;
    uint32_t first_copy_samples;
    uint32_t second_copy_samples;

    if ((src == 0) || (sample_count == 0u))
    {
        return 0u;
    }

    free_samples = Audio_Driver_FifoFreeSamples();
    if (sample_count > free_samples)
    {
        sample_count = free_samples;
    }

    if (sample_count == 0u)
    {
        return 0u;
    }

    write_index = s_audio_rt.pcm_fifo_write_index;
    first_copy_samples = AUDIO_SW_FIFO_SAMPLES - (write_index & AUDIO_SW_FIFO_MASK);
    if (first_copy_samples > sample_count)
    {
        first_copy_samples = sample_count;
    }

    memcpy(&s_audio_rt.pcm_fifo[write_index & AUDIO_SW_FIFO_MASK],
           src,
           first_copy_samples * sizeof(uint16_t));

    second_copy_samples = sample_count - first_copy_samples;
    if (second_copy_samples > 0u)
    {
        memcpy(&s_audio_rt.pcm_fifo[0],
               &src[first_copy_samples],
               second_copy_samples * sizeof(uint16_t));
    }

    __DMB();
    s_audio_rt.pcm_fifo_write_index = write_index + sample_count;
    Audio_Driver_PublishFifoTelemetry();

    return sample_count;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: FIFO pop                                                        */
/*                                                                            */
/*  consumer(ISR)가 FIFO에서 DMA half-buffer로 보낼 sample을 꺼낸다.           */
/*  read index는 memcpy가 모두 끝난 뒤에만 갱신한다.                           */
/* -------------------------------------------------------------------------- */
static uint32_t Audio_Driver_FifoPopSamples(uint16_t *dst,
                                            uint32_t sample_count)
{
    uint32_t available_samples;
    uint32_t read_index;
    uint32_t first_copy_samples;
    uint32_t second_copy_samples;

    if ((dst == 0) || (sample_count == 0u))
    {
        return 0u;
    }

    available_samples = Audio_Driver_FifoLevelSamples();
    if (sample_count > available_samples)
    {
        sample_count = available_samples;
    }

    if (sample_count == 0u)
    {
        return 0u;
    }

    read_index = s_audio_rt.pcm_fifo_read_index;
    first_copy_samples = AUDIO_SW_FIFO_SAMPLES - (read_index & AUDIO_SW_FIFO_MASK);
    if (first_copy_samples > sample_count)
    {
        first_copy_samples = sample_count;
    }

    memcpy(dst,
           &s_audio_rt.pcm_fifo[read_index & AUDIO_SW_FIFO_MASK],
           first_copy_samples * sizeof(uint16_t));

    second_copy_samples = sample_count - first_copy_samples;
    if (second_copy_samples > 0u)
    {
        memcpy(&dst[first_copy_samples],
               &s_audio_rt.pcm_fifo[0],
               second_copy_samples * sizeof(uint16_t));
    }

    __DMB();
    s_audio_rt.pcm_fifo_read_index = read_index + sample_count;
    Audio_Driver_PublishFifoTelemetry();

    return sample_count;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: DMA half-buffer service                                         */
/*                                                                            */
/*  이 함수는 ISR에서 호출되어도 안전해야 하므로                               */
/*  절대 FATFS read나 파형 합성을 하지 않는다.                                 */
/*  오직 software FIFO -> DMA buffer 복사와 부족분 silence padding만 한다.     */
/* -------------------------------------------------------------------------- */
static void Audio_Driver_ServiceDmaHalfFromFifo(uint32_t dst_offset_samples)
{
    uint32_t requested_samples;
    uint32_t copied_samples;
    uint8_t renderable_before;
    app_audio_state_t *audio;

    if (dst_offset_samples >= AUDIO_DMA_BUFFER_SAMPLES)
    {
        return;
    }

    requested_samples = AUDIO_DMA_BUFFER_SAMPLES / 2u;
    audio = (app_audio_state_t *)&g_app_state.audio;
    renderable_before = Audio_Driver_HasRenderableContent();

    copied_samples = Audio_Driver_FifoPopSamples(&s_audio_rt.dma_buffer[dst_offset_samples],
                                                 requested_samples);

    if (copied_samples < requested_samples)
    {
        Audio_Driver_FillBufferWithSilence(&s_audio_rt.dma_buffer[dst_offset_samples + copied_samples],
                                           requested_samples - copied_samples);

        /* ------------------------------------------------------------------ */
        /*  source가 아직 살아 있는데도 silence를 섞어 넣었다면                 */
        /*  producer(main context)가 deadline을 따라오지 못했다는 뜻이다.       */
        /*  이 경우만 starvation으로 집계한다.                                 */
        /*                                                                    */
        /*  반대로 source가 이미 끝나고 tail만 비워지는 자연 종료라면            */
        /*  starvation으로 보지 않는다.                                        */
        /* ------------------------------------------------------------------ */
        if (renderable_before != 0u)
        {
            audio->fifo_starvation_count++;
            audio->silence_injected_sample_count += (requested_samples - copied_samples);
        }
    }

    audio->dma_service_half_count++;
    Audio_Driver_PublishFifoTelemetry();
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: APP_STATE.audio slice 초기화                                    */
/* -------------------------------------------------------------------------- */
static void Audio_Driver_ResetAppStateSlice(app_audio_state_t *audio)
{
    uint32_t voice_index;

    if (audio == 0)
    {
        return;
    }

    memset(audio, 0, sizeof(*audio));

    audio->initialized                    = false;
    audio->transport_running              = false;
    audio->content_active                 = false;
    audio->wav_active                     = false;

    audio->mode                           = APP_AUDIO_MODE_IDLE;
    audio->active_voice_count             = 0u;
    audio->last_hal_status_dac            = 0xFFu;
    audio->last_hal_status_tim            = 0xFFu;

    audio->output_resolution_bits         = AUDIO_DAC_OUTPUT_BITS;
    audio->volume_percent                 = AUDIO_DEFAULT_VOLUME_PERCENT;
    audio->last_block_clipped             = 0u;
    audio->wav_native_rate_active         = 0u;

    audio->sample_rate_hz                 = Audio_Driver_ClampOutputSampleRate(AUDIO_SAMPLE_RATE_HZ);
    audio->dma_buffer_sample_count        = AUDIO_DMA_BUFFER_SAMPLES;
    audio->dma_half_buffer_sample_count   = AUDIO_DMA_BUFFER_SAMPLES / 2u;
    audio->last_block_min_u12             = AUDIO_DAC_MIDPOINT_U12;
    audio->last_block_max_u12             = AUDIO_DAC_MIDPOINT_U12;

    /* ---------------------------------------------------------------------- */
    /*  software FIFO telemetry 기본값                                          */
    /*                                                                        */
    /*  아직 transport가 돌기 전이라도 UI/디버그 페이지가                      */
    /*  전체 pipeline 구조를 알 수 있도록 용량과 watermark를 먼저 적어 둔다.   */
    /* ---------------------------------------------------------------------- */
    audio->sw_fifo_capacity_samples       = AUDIO_SW_FIFO_SAMPLES;
    audio->sw_fifo_level_samples          = 0u;
    audio->sw_fifo_peak_level_samples     = 0u;
    audio->sw_fifo_low_watermark_samples  = AUDIO_SW_FIFO_LOW_WATERMARK_SAMPLES;
    audio->sw_fifo_high_watermark_samples = AUDIO_SW_FIFO_HIGH_WATERMARK_SAMPLES;

    audio->last_update_ms                 = 0u;
    audio->playback_start_ms              = 0u;
    audio->playback_stop_ms               = 0u;
    audio->half_callback_count            = 0u;
    audio->full_callback_count            = 0u;
    audio->dma_underrun_count             = 0u;
    audio->render_block_count             = 0u;
    audio->clip_block_count               = 0u;
    audio->transport_reconfig_count       = 0u;
    audio->producer_refill_block_count    = 0u;
    audio->dma_service_half_count         = 0u;
    audio->fifo_starvation_count          = 0u;
    audio->silence_injected_sample_count  = 0u;

    audio->sequence_bpm                   = 0u;
    audio->wav_source_sample_rate_hz      = 0u;
    audio->wav_source_data_bytes_remaining = 0u;
    audio->wav_source_channels            = 0u;
    audio->wav_source_bits_per_sample     = 0u;

    for (voice_index = 0u; voice_index < APP_AUDIO_MAX_VOICES; voice_index++)
    {
        audio->voices[voice_index].active               = false;
        audio->voices[voice_index].waveform_id          = APP_AUDIO_WAVEFORM_NONE;
        audio->voices[voice_index].timbre_id            = 0u;
        audio->voices[voice_index].track_index          = 0u;
        audio->voices[voice_index].env_phase            = APP_AUDIO_ENV_OFF;
        audio->voices[voice_index].note_hz_x100         = 0u;
        audio->voices[voice_index].phase_q32            = 0u;
        audio->voices[voice_index].phase_inc_q32        = 0u;
        audio->voices[voice_index].note_samples_total   = 0u;
        audio->voices[voice_index].note_samples_elapsed = 0u;
        audio->voices[voice_index].gate_samples         = 0u;
        audio->voices[voice_index].env_level_q15        = 0u;
        audio->voices[voice_index].velocity_q15         = 0u;
    }
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: perceptual volume LUT 생성                                      */
/*                                                                            */
/*  render path에서 float/pow를 돌리면 낭비이므로                              */
/*  driver init 시점에 0~100 volume에 대응하는 Q15 LUT를 1회 만든다.           */
/*                                                                            */
/*  0%는 hard mute, 1~100%는                                                   */
/*  AUDIO_VOLUME_CURVE_MIN_DB .. 0dB 선형 dB 스케일을 사용한다.                */
/* -------------------------------------------------------------------------- */
static void Audio_Driver_BuildVolumeTable(void)
{
    uint32_t percent;

    if (s_audio_rt.volume_table_ready != 0u)
    {
        return;
    }

    s_audio_rt.volume_table_q15[0] = 0u;

    for (percent = 1u; percent <= 100u; percent++)
    {
        double db_value;
        double linear_gain;
        uint32_t gain_q15;

        db_value = AUDIO_VOLUME_CURVE_MIN_DB +
                   (((double)percent / 100.0) * (0.0 - AUDIO_VOLUME_CURVE_MIN_DB));
        linear_gain = pow(10.0, db_value / 20.0);
        gain_q15 = (uint32_t)(linear_gain * 32767.0 + 0.5);

        if (gain_q15 > 32767u)
        {
            gain_q15 = 32767u;
        }

        s_audio_rt.volume_table_q15[percent] = (uint16_t)gain_q15;
    }

    s_audio_rt.volume_table_ready = 1u;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: volume percent 적용                                             */
/* -------------------------------------------------------------------------- */
static void Audio_Driver_SetVolumePercentInternal(uint8_t volume_percent)
{
    uint32_t effective_gain_q15;
    app_audio_state_t *audio;

    Audio_Driver_BuildVolumeTable();

    if (volume_percent > 100u)
    {
        volume_percent = 100u;
    }

    audio = (app_audio_state_t *)&g_app_state.audio;

    s_audio_rt.current_volume_q15 = s_audio_rt.volume_table_q15[volume_percent];
    effective_gain_q15 = ((uint32_t)s_audio_rt.current_volume_q15 * (uint32_t)AUDIO_ANALOG_SAFE_HEADROOM_Q15) >> 15;
    if (effective_gain_q15 > 32767u)
    {
        effective_gain_q15 = 32767u;
    }

    s_audio_rt.current_output_gain_q15 = (uint16_t)effective_gain_q15;
    audio->volume_percent = volume_percent;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: volume set/get                                                   */
/* -------------------------------------------------------------------------- */
void Audio_Driver_SetVolumePercent(uint8_t volume_percent)
{
    Audio_Driver_SetVolumePercentInternal(volume_percent);
}

uint8_t Audio_Driver_GetVolumePercent(void)
{
    return ((const app_audio_state_t *)&g_app_state.audio)->volume_percent;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: voice snapshot 1개 반영                                          */
/* -------------------------------------------------------------------------- */
static void Audio_Driver_PublishVoiceSnapshot(uint32_t voice_index)
{
    app_audio_state_t *audio;
    const audio_voice_runtime_t *voice;

    if (voice_index >= APP_AUDIO_MAX_VOICES)
    {
        return;
    }

    audio = (app_audio_state_t *)&g_app_state.audio;
    voice = &s_audio_rt.voices[voice_index];

    audio->voices[voice_index].active               = (voice->active != 0u) ? true : false;
    audio->voices[voice_index].waveform_id          = (voice->timbre != 0) ? (uint8_t)voice->timbre->ui_waveform : APP_AUDIO_WAVEFORM_NONE;
    audio->voices[voice_index].timbre_id            = voice->timbre_id;
    audio->voices[voice_index].track_index          = voice->track_index;
    audio->voices[voice_index].env_phase            = voice->env_phase;
    audio->voices[voice_index].note_hz_x100         = voice->note_hz_x100;
    audio->voices[voice_index].phase_q32            = voice->phase_q32;
    audio->voices[voice_index].phase_inc_q32        = voice->phase_inc_q32;
    audio->voices[voice_index].note_samples_total   = voice->note_samples_total;
    audio->voices[voice_index].note_samples_elapsed = voice->note_samples_elapsed;
    audio->voices[voice_index].gate_samples         = voice->gate_samples;
    audio->voices[voice_index].env_level_q15        = voice->env_level_q15;
    audio->voices[voice_index].velocity_q15         = voice->velocity_q15;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: voice snapshot 전체 반영                                         */
/* -------------------------------------------------------------------------- */
static void Audio_Driver_PublishAllVoiceSnapshots(void)
{
    uint32_t voice_index;

    for (voice_index = 0u; voice_index < APP_AUDIO_MAX_VOICES; voice_index++)
    {
        Audio_Driver_PublishVoiceSnapshot(voice_index);
    }
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: active voice count 재계산                                       */
/* -------------------------------------------------------------------------- */
static void Audio_Driver_UpdateActiveVoiceCount(void)
{
    app_audio_state_t *audio;
    uint32_t voice_index;
    uint8_t active_count;

    audio = (app_audio_state_t *)&g_app_state.audio;
    active_count = 0u;

    for (voice_index = 0u; voice_index < APP_AUDIO_MAX_VOICES; voice_index++)
    {
        if (s_audio_rt.voices[voice_index].active != 0u)
        {
            active_count++;
        }
    }

    audio->active_voice_count = active_count;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: timer clock 계산                                                */
/* -------------------------------------------------------------------------- */
static uint32_t Audio_Driver_GetTim6ClockHz(void)
{
    uint32_t pclk1_hz;
    uint32_t hclk_hz;

    pclk1_hz = HAL_RCC_GetPCLK1Freq();
    hclk_hz  = HAL_RCC_GetHCLKFreq();

    if (pclk1_hz < hclk_hz)
    {
        return (pclk1_hz * 2u);
    }

    return pclk1_hz;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: sample rate -> TIM6 runtime 설정                               */
/* -------------------------------------------------------------------------- */
static HAL_StatusTypeDef Audio_Driver_ConfigureTimerRuntime(uint32_t sample_rate_hz)
{
    HAL_StatusTypeDef status;
    TIM_MasterConfigTypeDef master_config;
    uint32_t tim_clk_hz;
    uint32_t period_plus_one;

    tim_clk_hz = Audio_Driver_GetTim6ClockHz();
    if ((tim_clk_hz == 0u) || (sample_rate_hz == 0u))
    {
        return HAL_ERROR;
    }

    period_plus_one = (tim_clk_hz + (sample_rate_hz / 2u)) / sample_rate_hz;
    if (period_plus_one == 0u)
    {
        return HAL_ERROR;
    }

    (void)HAL_TIM_Base_Stop(&AUDIO_TIMER_HANDLE);

    AUDIO_TIMER_HANDLE.Init.Prescaler         = 0u;
    AUDIO_TIMER_HANDLE.Init.CounterMode       = TIM_COUNTERMODE_UP;
    AUDIO_TIMER_HANDLE.Init.Period            = period_plus_one - 1u;
    AUDIO_TIMER_HANDLE.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;

#if defined(TIM_CLOCKDIVISION_DIV1)
    AUDIO_TIMER_HANDLE.Init.ClockDivision     = TIM_CLOCKDIVISION_DIV1;
#endif

    status = HAL_TIM_Base_Init(&AUDIO_TIMER_HANDLE);
    ((app_audio_state_t *)&g_app_state.audio)->last_hal_status_tim = (uint8_t)status;
    if (status != HAL_OK)
    {
        return status;
    }

    memset(&master_config, 0, sizeof(master_config));
    master_config.MasterOutputTrigger = TIM_TRGO_UPDATE;
    master_config.MasterSlaveMode     = TIM_MASTERSLAVEMODE_DISABLE;

    status = HAL_TIMEx_MasterConfigSynchronization(&AUDIO_TIMER_HANDLE, &master_config);
    ((app_audio_state_t *)&g_app_state.audio)->last_hal_status_tim = (uint8_t)status;
    if (status == HAL_OK)
    {
        s_audio_rt.active_output_sample_rate_hz = sample_rate_hz;
        ((app_audio_state_t *)&g_app_state.audio)->sample_rate_hz = sample_rate_hz;
    }

    return status;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: DAC1 CH1 runtime 설정                                           */
/* -------------------------------------------------------------------------- */
static HAL_StatusTypeDef Audio_Driver_ConfigureDacRuntime(void)
{
    DAC_ChannelConfTypeDef channel_config;
    HAL_StatusTypeDef status;

    memset(&channel_config, 0, sizeof(channel_config));
    channel_config.DAC_Trigger = DAC_TRIGGER_T6_TRGO;

#if (AUDIO_DAC_USE_OUTPUT_BUFFER != 0u)
    channel_config.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
#else
    channel_config.DAC_OutputBuffer = DAC_OUTPUTBUFFER_DISABLE;
#endif

    status = HAL_DAC_ConfigChannel(&AUDIO_DAC_HANDLE,
                                   &channel_config,
                                   AUDIO_DAC_CHANNEL);
    ((app_audio_state_t *)&g_app_state.audio)->last_hal_status_dac = (uint8_t)status;
    return status;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 전체 DMA buffer를 silence로 채움                                */
/* -------------------------------------------------------------------------- */
static void Audio_Driver_FillBufferWithSilence(uint16_t *dst, uint32_t sample_count)
{
    uint32_t index;

    if (dst == 0)
    {
        return;
    }

    for (index = 0u; index < sample_count; index++)
    {
        dst[index] = AUDIO_DAC_MIDPOINT_U12;
    }
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: transport 시작                                                  */
/*                                                                            */
/*  transport는 항상 circular DMA + TIM6 trigger 기반으로 유지하고,            */
/*  실제 content가 없을 때는 silence만 내보낸다.                               */
/* -------------------------------------------------------------------------- */
static HAL_StatusTypeDef Audio_Driver_StartTransport(void)
{
    HAL_StatusTypeDef status;
    app_audio_state_t *audio;

    audio = (app_audio_state_t *)&g_app_state.audio;

    /* ---------------------------------------------------------------------- */
    /*  transport 재시작 전에는 FIFO와 DMA 버퍼를 모두 silence로 비운다.       */
    /*                                                                        */
    /*  이유                                                                   */
    /*  - sample rate가 바뀐 뒤 old-rate sample을 재생하면 pitch/time이 깨진다. */
    /*  - content 전환 직후 old content tail이 섞이면 click/pop 원인이 된다.   */
    /* ---------------------------------------------------------------------- */
    Audio_Driver_FifoReset();
    Audio_Driver_FillBufferWithSilence(s_audio_rt.dma_buffer, AUDIO_DMA_BUFFER_SAMPLES);
    Audio_Driver_PublishFifoTelemetry();

    (void)HAL_DAC_Stop_DMA(&AUDIO_DAC_HANDLE, AUDIO_DAC_CHANNEL);
    (void)HAL_TIM_Base_Stop(&AUDIO_TIMER_HANDLE);

    status = HAL_DAC_Start_DMA(&AUDIO_DAC_HANDLE,
                               AUDIO_DAC_CHANNEL,
                               (uint32_t *)s_audio_rt.dma_buffer,
                               AUDIO_DMA_BUFFER_SAMPLES,
                               DAC_ALIGN_12B_R);
    audio->last_hal_status_dac = (uint8_t)status;
    if (status != HAL_OK)
    {
        audio->transport_running = false;
        return status;
    }

    status = HAL_TIM_Base_Start(&AUDIO_TIMER_HANDLE);
    audio->last_hal_status_tim = (uint8_t)status;
    if (status != HAL_OK)
    {
        (void)HAL_DAC_Stop_DMA(&AUDIO_DAC_HANDLE, AUDIO_DAC_CHANNEL);
        audio->transport_running = false;
        return status;
    }

    audio->transport_running = true;
    return HAL_OK;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 출력 sample rate 적용                                           */
/*                                                                            */
/*  content start 시점에만 transport를 재시작하므로 steady-state CPU 비용은     */
/*  증가하지 않는다.                                                           */
/* -------------------------------------------------------------------------- */
static HAL_StatusTypeDef Audio_Driver_ApplyOutputSampleRate(uint32_t requested_hz)
{
    HAL_StatusTypeDef status;
    uint32_t clamped_hz;
    uint32_t previous_hz;
    app_audio_state_t *audio;

    audio = (app_audio_state_t *)&g_app_state.audio;
    clamped_hz = Audio_Driver_ClampOutputSampleRate(requested_hz);
    previous_hz = Audio_Driver_GetActiveOutputSampleRate();

    if (audio->transport_running != false)
    {
        if (previous_hz == clamped_hz)
        {
            audio->sample_rate_hz = clamped_hz;
            return HAL_OK;
        }
    }

    status = Audio_Driver_ConfigureTimerRuntime(clamped_hz);
    if (status != HAL_OK)
    {
        return status;
    }

    status = Audio_Driver_StartTransport();
    if (status != HAL_OK)
    {
        return status;
    }

    if (previous_hz != clamped_hz)
    {
        audio->transport_reconfig_count++;
    }

    audio->sample_rate_hz = clamped_hz;
    return HAL_OK;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: permille -> Q15                                                 */
/* -------------------------------------------------------------------------- */
static uint16_t Audio_Driver_PermilleToQ15(uint16_t permille)
{
    uint32_t scaled;

    if (permille >= 1000u)
    {
        return 32767u;
    }

    scaled = ((uint32_t)permille * 32767u + 500u) / 1000u;
    return (uint16_t)scaled;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: active source 수에 따른 headroom scale                          */
/*                                                                            */
/*  4채널 sequence + WAV를 그냥 더하면                                         */
/*  순간적으로 full-scale를 크게 넘기기 쉽다.                                 */
/*                                                                            */
/*  그래서 현재 active source 수에 따라                                        */
/*  1/N 계열의 간단한 정규화를 먼저 적용하고,                                  */
/*  그 위에 user volume을 곱한다.                                              */
/* -------------------------------------------------------------------------- */
static uint16_t Audio_Driver_GetMixScaleQ15(uint8_t active_source_count)
{
    switch (active_source_count)
    {
    case 0u:
    case 1u:
        return 32767u;

    case 2u:
        return 16384u;

    case 3u:
        return 10923u;

    case 4u:
        return 8192u;

    case 5u:
    default:
        return 6554u;
    }
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: soft clip                                                       */
/*                                                                            */
/*  단순 hard clip은 귀에 매우 거칠게 들리므로                                  */
/*  knee 이상 구간만 완만하게 눌러 주고,                                        */
/*  그래도 넘치는 값만 마지막에 saturate 한다.                                 */
/* -------------------------------------------------------------------------- */
static int16_t Audio_Driver_SoftClipS16(int32_t sample_s32, uint8_t *clip_happened)
{
    int32_t sign;
    int32_t abs_value;
    int32_t compressed;

    if (sample_s32 > AUDIO_SOFT_CLIP_KNEE_S16)
    {
        if (clip_happened != 0)
        {
            *clip_happened = 1u;
        }
    }
    else if (sample_s32 < -AUDIO_SOFT_CLIP_KNEE_S16)
    {
        if (clip_happened != 0)
        {
            *clip_happened = 1u;
        }
    }

    sign = (sample_s32 < 0) ? -1 : 1;
    abs_value = (sample_s32 < 0) ? -sample_s32 : sample_s32;

    if (abs_value <= AUDIO_SOFT_CLIP_KNEE_S16)
    {
        return (int16_t)sample_s32;
    }

    compressed = AUDIO_SOFT_CLIP_KNEE_S16 + ((abs_value - AUDIO_SOFT_CLIP_KNEE_S16) >> 2);
    if (compressed > 32767)
    {
        compressed = 32767;
    }

    return (int16_t)((sign > 0) ? compressed : -compressed);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: duration tick -> sample 수                                      */
/* -------------------------------------------------------------------------- */
static uint32_t Audio_Driver_TicksToSamples(uint32_t duration_ticks, uint32_t bpm)
{
    uint64_t numerator;
    uint64_t denominator;
    uint32_t samples;
    uint32_t sample_rate_hz;

    if ((duration_ticks == 0u) || (bpm == 0u))
    {
        return 0u;
    }

    sample_rate_hz = Audio_Driver_GetActiveOutputSampleRate();
    numerator   = (uint64_t)duration_ticks * (uint64_t)sample_rate_hz * 60ull;
    denominator = (uint64_t)bpm * (uint64_t)AUDIO_PPQN;

    samples = (uint32_t)((numerator + (denominator / 2ull)) / denominator);
    if (samples == 0u)
    {
        samples = 1u;
    }

    return samples;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 주파수 -> Q32 phase increment                                   */
/* -------------------------------------------------------------------------- */
static uint32_t Audio_Driver_FrequencyToPhaseIncQ32(uint32_t note_hz_x100)
{
    uint64_t numerator;
    uint64_t denominator;
    uint32_t phase_inc;
    uint32_t sample_rate_hz;

    if (note_hz_x100 == 0u)
    {
        return 0u;
    }

    sample_rate_hz = Audio_Driver_GetActiveOutputSampleRate();
    numerator   = (uint64_t)note_hz_x100 << 32;
    denominator = (uint64_t)sample_rate_hz * 100ull;

    phase_inc = (uint32_t)((numerator + (denominator / 2ull)) / denominator);
    if (phase_inc == 0u)
    {
        phase_inc = 1u;
    }

    return phase_inc;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: content name 갱신                                               */
/* -------------------------------------------------------------------------- */
static void Audio_Driver_SetCurrentName(const char *name)
{
    Audio_Driver_CopyTextSafe(((app_audio_state_t *)&g_app_state.audio)->current_name,
                              sizeof(((app_audio_state_t *)&g_app_state.audio)->current_name),
                              name);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 특정 voice 즉시 clear                                            */
/* -------------------------------------------------------------------------- */
static void Audio_Driver_ClearVoice(uint32_t voice_index)
{
    if (voice_index >= APP_AUDIO_MAX_VOICES)
    {
        return;
    }

    memset(&s_audio_rt.voices[voice_index], 0, sizeof(s_audio_rt.voices[voice_index]));
    s_audio_rt.voices[voice_index].env_phase = APP_AUDIO_ENV_OFF;
    Audio_Driver_PublishVoiceSnapshot(voice_index);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 전체 voice clear                                                */
/* -------------------------------------------------------------------------- */
static void Audio_Driver_ClearAllVoices(void)
{
    uint32_t voice_index;

    for (voice_index = 0u; voice_index < APP_AUDIO_MAX_VOICES; voice_index++)
    {
        Audio_Driver_ClearVoice(voice_index);
    }

    Audio_Driver_UpdateActiveVoiceCount();
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: WAV close                                                       */
/* -------------------------------------------------------------------------- */
static void Audio_Driver_WavClose(void)
{
    if (s_audio_rt.wav.active != 0u)
    {
        (void)f_close(&s_audio_rt.wav.file);
    }

    memset(&s_audio_rt.wav, 0, sizeof(s_audio_rt.wav));

    ((app_audio_state_t *)&g_app_state.audio)->wav_active = false;
    ((app_audio_state_t *)&g_app_state.audio)->wav_native_rate_active = false;
    ((app_audio_state_t *)&g_app_state.audio)->wav_source_sample_rate_hz = 0u;
    ((app_audio_state_t *)&g_app_state.audio)->wav_source_data_bytes_remaining = 0u;
    ((app_audio_state_t *)&g_app_state.audio)->wav_source_channels = 0u;
    ((app_audio_state_t *)&g_app_state.audio)->wav_source_bits_per_sample = 0u;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: sequence clear                                                  */
/* -------------------------------------------------------------------------- */
static void Audio_Driver_ClearSequence(void)
{
    memset(&s_audio_rt.sequence, 0, sizeof(s_audio_rt.sequence));
    ((app_audio_state_t *)&g_app_state.audio)->sequence_bpm = 0u;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 현재 content 전체 stop                                           */
/*                                                                            */
/*  transport(TIM6+DAC DMA)은 유지한 채                                        */
/*  tone / sequence / WAV 만 비운다.                                           */
/* -------------------------------------------------------------------------- */
static void Audio_Driver_StopContentInternal(void)
{
    app_audio_state_t *audio;

    audio = (app_audio_state_t *)&g_app_state.audio;

    Audio_Driver_ClearSequence();
    Audio_Driver_WavClose();
    Audio_Driver_ClearAllVoices();

    /* ---------------------------------------------------------------------- */
    /*  content stop은 "앞으로 더 만들 오디오" 뿐 아니라                       */
    /*  이미 만들어 둔 FIFO tail도 함께 버리는 의미다.                          */
    /*                                                                        */
    /*  즉, stop 직후에는 곧바로 silence만 나가야 하므로                         */
    /*  software FIFO도 여기서 함께 비운다.                                    */
    /* ---------------------------------------------------------------------- */
    Audio_Driver_FifoReset();
    Audio_Driver_PublishFifoTelemetry();

    audio->content_active         = false;
    audio->wav_active             = false;
    audio->wav_native_rate_active = false;
    audio->mode                   = APP_AUDIO_MODE_IDLE;
    audio->last_block_clipped     = 0u;
    audio->playback_stop_ms       = HAL_GetTick();
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: voice release 진입                                              */
/* -------------------------------------------------------------------------- */
static void Audio_Driver_EnterVoiceRelease(audio_voice_runtime_t *voice)
{
    uint32_t step;

    if ((voice == 0) || (voice->active == 0u))
    {
        return;
    }

    if (voice->env_phase == APP_AUDIO_ENV_RELEASE)
    {
        return;
    }

    if (voice->release_samples == 0u)
    {
        voice->active = 0u;
        voice->env_phase = APP_AUDIO_ENV_OFF;
        voice->env_level_q15 = 0u;
        return;
    }

    step = (voice->env_level_q15 + voice->release_samples - 1u) / voice->release_samples;
    if (step == 0u)
    {
        step = 1u;
    }

    voice->release_dec_q15 = (uint16_t)step;
    voice->env_phase = APP_AUDIO_ENV_RELEASE;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: voice start                                                     */
/* -------------------------------------------------------------------------- */
static void Audio_Driver_StartVoice(uint32_t voice_index,
                                    uint8_t track_index,
                                    audio_timbre_preset_id_t timbre_id,
                                    uint32_t note_hz_x100,
                                    uint32_t total_samples,
                                    uint32_t gate_samples,
                                    uint16_t velocity_permille)
{
    audio_voice_runtime_t *voice;
    const audio_timbre_preset_t *timbre;
    uint32_t attack_step;
    uint32_t decay_step;
    uint32_t sample_rate_hz;

    if (voice_index >= APP_AUDIO_MAX_VOICES)
    {
        return;
    }

    timbre = Audio_Presets_GetTimbre(timbre_id);
    if (timbre == 0)
    {
        return;
    }

    sample_rate_hz = Audio_Driver_GetActiveOutputSampleRate();

    voice = &s_audio_rt.voices[voice_index];
    memset(voice, 0, sizeof(*voice));

    voice->active               = 1u;
    voice->track_index          = track_index;
    voice->timbre_id            = (uint8_t)timbre_id;
    voice->timbre               = timbre;
    voice->note_hz_x100         = note_hz_x100;
    voice->phase_q32            = 0u;
    voice->phase_inc_q32        = Audio_Driver_FrequencyToPhaseIncQ32(note_hz_x100);
    voice->note_samples_total   = total_samples;
    voice->note_samples_elapsed = 0u;
    voice->gate_samples         = (gate_samples > total_samples) ? total_samples : gate_samples;

    voice->attack_samples       = (uint32_t)timbre->attack_ms * sample_rate_hz / 1000u;
    voice->decay_samples        = (uint32_t)timbre->decay_ms  * sample_rate_hz / 1000u;
    voice->release_samples      = (uint32_t)timbre->release_ms * sample_rate_hz / 1000u;
    voice->sustain_level_q15    = timbre->sustain_level_q15;
    voice->velocity_q15         = Audio_Driver_PermilleToQ15(velocity_permille);
    voice->env_level_q15        = 0u;

    if (voice->attack_samples == 0u)
    {
        voice->env_phase = APP_AUDIO_ENV_DECAY;
        voice->env_level_q15 = 32767u;
    }
    else
    {
        voice->env_phase = APP_AUDIO_ENV_ATTACK;
    }

    attack_step = (voice->attack_samples != 0u) ?
                  ((32767u + voice->attack_samples - 1u) / voice->attack_samples) :
                  32767u;
    if (attack_step == 0u)
    {
        attack_step = 1u;
    }
    voice->attack_inc_q15 = (uint16_t)attack_step;

    decay_step = (voice->decay_samples != 0u) ?
                 (((32767u - voice->sustain_level_q15) + voice->decay_samples - 1u) / voice->decay_samples) :
                 32767u;
    if (decay_step == 0u)
    {
        decay_step = 1u;
    }
    voice->decay_dec_q15 = (uint16_t)decay_step;
    voice->release_dec_q15 = 1u;

    Audio_Driver_PublishVoiceSnapshot(voice_index);
    Audio_Driver_UpdateActiveVoiceCount();
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: voice sample 1개 생성                                            */
/* -------------------------------------------------------------------------- */
static int16_t Audio_Driver_RenderVoiceOneSample(audio_voice_runtime_t *voice)
{
    int32_t raw_sample;
    int32_t shaped_sample;

    if ((voice == 0) || (voice->active == 0u) || (voice->timbre == 0))
    {
        return 0;
    }

    if ((voice->note_samples_elapsed >= voice->gate_samples) &&
        (voice->env_phase != APP_AUDIO_ENV_RELEASE) &&
        (voice->env_phase != APP_AUDIO_ENV_OFF))
    {
        Audio_Driver_EnterVoiceRelease(voice);
    }

    raw_sample = (int32_t)voice->timbre->render_fn(voice->phase_q32,
                                                    voice->phase_inc_q32);

    switch (voice->env_phase)
    {
    case APP_AUDIO_ENV_ATTACK:
        if ((uint32_t)voice->env_level_q15 + (uint32_t)voice->attack_inc_q15 >= 32767u)
        {
            voice->env_level_q15 = 32767u;
            voice->env_phase = APP_AUDIO_ENV_DECAY;
        }
        else
        {
            voice->env_level_q15 = (uint16_t)(voice->env_level_q15 + voice->attack_inc_q15);
        }
        break;

    case APP_AUDIO_ENV_DECAY:
        if (voice->env_level_q15 <= voice->sustain_level_q15)
        {
            voice->env_level_q15 = voice->sustain_level_q15;
            voice->env_phase = APP_AUDIO_ENV_SUSTAIN;
        }
        else if (voice->env_level_q15 <= voice->decay_dec_q15)
        {
            voice->env_level_q15 = voice->sustain_level_q15;
            voice->env_phase = APP_AUDIO_ENV_SUSTAIN;
        }
        else
        {
            uint16_t next_level;

            next_level = (uint16_t)(voice->env_level_q15 - voice->decay_dec_q15);
            if (next_level <= voice->sustain_level_q15)
            {
                next_level = voice->sustain_level_q15;
                voice->env_phase = APP_AUDIO_ENV_SUSTAIN;
            }

            voice->env_level_q15 = next_level;
        }
        break;

    case APP_AUDIO_ENV_SUSTAIN:
        voice->env_level_q15 = voice->sustain_level_q15;
        break;

    case APP_AUDIO_ENV_RELEASE:
        if (voice->env_level_q15 <= voice->release_dec_q15)
        {
            voice->env_level_q15 = 0u;
            voice->env_phase = APP_AUDIO_ENV_OFF;
            voice->active = 0u;
        }
        else
        {
            voice->env_level_q15 = (uint16_t)(voice->env_level_q15 - voice->release_dec_q15);
        }
        break;

    case APP_AUDIO_ENV_OFF:
    default:
        voice->env_level_q15 = 0u;
        voice->active = 0u;
        break;
    }

    shaped_sample = (int32_t)(((int64_t)raw_sample * (int64_t)voice->env_level_q15) >> 15);
    shaped_sample = (int32_t)(((int64_t)shaped_sample * (int64_t)voice->velocity_q15) >> 15);
    shaped_sample = (shaped_sample * (int32_t)voice->timbre->gain_permille) / 1000;

    voice->phase_q32 += voice->phase_inc_q32;
    voice->note_samples_elapsed++;

    if ((voice->note_samples_elapsed >= voice->note_samples_total) &&
        (voice->env_phase != APP_AUDIO_ENV_OFF))
    {
        Audio_Driver_EnterVoiceRelease(voice);

        if (voice->release_samples == 0u)
        {
            voice->active = 0u;
            voice->env_phase = APP_AUDIO_ENV_OFF;
            voice->env_level_q15 = 0u;
        }
    }

    return (int16_t)shaped_sample;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: sequence에서 이 트랙을 현재 사용해야 하는가                      */
/* -------------------------------------------------------------------------- */
static uint8_t Audio_Driver_IsSequenceTrackActive(uint32_t track_index)
{
    if (track_index >= AUDIO_PRESET_MAX_TRACKS)
    {
        return 0u;
    }

    if (s_audio_rt.sequence.active == 0u)
    {
        return 0u;
    }

    if (s_audio_rt.sequence.mono_mode != 0u)
    {
        return (track_index == s_audio_rt.sequence.melody_track_index) ? 1u : 0u;
    }

    return 1u;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: sequence 다음 event 시작                                        */
/* -------------------------------------------------------------------------- */
static void Audio_Driver_SequenceStartNextEvent(uint32_t track_index)
{
    const audio_note_event_t *track;
    const audio_note_event_t *event;
    uint16_t track_length;
    uint16_t event_index;
    uint32_t duration_samples;
    uint32_t gate_samples;
    audio_timbre_preset_id_t timbre_id;

    if (Audio_Driver_IsSequenceTrackActive(track_index) == 0u)
    {
        return;
    }

    if ((s_audio_rt.sequence.note_preset == 0) ||
        (track_index >= s_audio_rt.sequence.note_preset->track_count))
    {
        return;
    }

    track = s_audio_rt.sequence.note_preset->tracks[track_index];
    track_length = s_audio_rt.sequence.note_preset->track_lengths[track_index];
    event_index = s_audio_rt.sequence.current_note_index[track_index];

    if ((track == 0) || (track_length == 0u) || (event_index >= track_length))
    {
        return;
    }

    event = &track[event_index];
    s_audio_rt.sequence.current_note_index[track_index]++;

    duration_samples = Audio_Driver_TicksToSamples(event->duration_ticks,
                                                   s_audio_rt.sequence.bpm);
    if (duration_samples == 0u)
    {
        duration_samples = 1u;
    }

    if (event->gate_ticks != 0u)
    {
        gate_samples = Audio_Driver_TicksToSamples(event->gate_ticks,
                                                   s_audio_rt.sequence.bpm);
    }
    else
    {
        gate_samples = (duration_samples * AUDIO_GATE_PERCENT_DEFAULT) / 100u;
    }

    if (gate_samples == 0u)
    {
        gate_samples = 1u;
    }

    if (gate_samples > duration_samples)
    {
        gate_samples = duration_samples;
    }

    s_audio_rt.sequence.samples_until_next_event[track_index] = duration_samples;

    if ((event->note_hz_x100 == 0u) || (event->velocity_permille == 0u))
    {
        Audio_Driver_ClearVoice(track_index);
        return;
    }

    if (s_audio_rt.sequence.mono_mode != 0u)
    {
        timbre_id = s_audio_rt.sequence.mono_timbre_id;
    }
    else
    {
        timbre_id = s_audio_rt.sequence.stack_preset->channel_timbres[track_index];
    }

    Audio_Driver_StartVoice(track_index,
                            (uint8_t)track_index,
                            timbre_id,
                            event->note_hz_x100,
                            duration_samples,
                            gate_samples,
                            event->velocity_permille);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: sequence sample tick 1회 진행                                   */
/* -------------------------------------------------------------------------- */
static void Audio_Driver_SequenceAdvanceOneSample(void)
{
    uint32_t track_index;

    if (s_audio_rt.sequence.active == 0u)
    {
        return;
    }

    for (track_index = 0u; track_index < AUDIO_PRESET_MAX_TRACKS; track_index++)
    {
        if (Audio_Driver_IsSequenceTrackActive(track_index) == 0u)
        {
            continue;
        }

        if (s_audio_rt.sequence.samples_until_next_event[track_index] == 0u)
        {
            Audio_Driver_SequenceStartNextEvent(track_index);
        }

        if (s_audio_rt.sequence.samples_until_next_event[track_index] != 0u)
        {
            s_audio_rt.sequence.samples_until_next_event[track_index]--;
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: sequence가 완전히 끝났는가                                      */
/* -------------------------------------------------------------------------- */
static uint8_t Audio_Driver_SequenceIsFinished(void)
{
    uint32_t track_index;

    if (s_audio_rt.sequence.active == 0u)
    {
        return 1u;
    }

    if (s_audio_rt.sequence.note_preset == 0)
    {
        return 1u;
    }

    for (track_index = 0u; track_index < AUDIO_PRESET_MAX_TRACKS; track_index++)
    {
        if (Audio_Driver_IsSequenceTrackActive(track_index) == 0u)
        {
            continue;
        }

        if (track_index >= s_audio_rt.sequence.note_preset->track_count)
        {
            continue;
        }

        if (s_audio_rt.sequence.current_note_index[track_index] <
            s_audio_rt.sequence.note_preset->track_lengths[track_index])
        {
            return 0u;
        }

        if (s_audio_rt.sequence.samples_until_next_event[track_index] != 0u)
        {
            return 0u;
        }

        if (s_audio_rt.voices[track_index].active != 0u)
        {
            return 0u;
        }
    }

    return 1u;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: WAV path extension 검사                                         */
/* -------------------------------------------------------------------------- */
static uint8_t Audio_Driver_IsWaveFileName(const char *name)
{
    size_t len;
    char c0;
    char c1;
    char c2;
    char c3;

    if (name == 0)
    {
        return 0u;
    }

    len = strlen(name);
    if (len < 4u)
    {
        return 0u;
    }

    c0 = (char)toupper((unsigned char)name[len - 4u]);
    c1 = (char)toupper((unsigned char)name[len - 3u]);
    c2 = (char)toupper((unsigned char)name[len - 2u]);
    c3 = (char)toupper((unsigned char)name[len - 1u]);

    return ((c0 == '.') && (c1 == 'W') && (c2 == 'A') && (c3 == 'V')) ? 1u : 0u;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: little-endian decode                                            */
/* -------------------------------------------------------------------------- */
static uint16_t Audio_Driver_U16FromLe(const uint8_t *p)
{
    return (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
}

static uint32_t Audio_Driver_U32FromLe(const uint8_t *p)
{
    return ((uint32_t)p[0]) |
           ((uint32_t)p[1] << 8) |
           ((uint32_t)p[2] << 16) |
           ((uint32_t)p[3] << 24);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: WAV buffered read helper                                        */
/* -------------------------------------------------------------------------- */
static uint8_t Audio_Driver_WavEnsureBufferedBytes(audio_wav_runtime_t *wav,
                                                   uint32_t required_bytes)
{
    UINT bytes_read;
    uint32_t buffered_bytes;
    uint32_t unread_file_bytes;
    uint32_t bytes_to_read;
    FRESULT fr;

    if (wav == 0)
    {
        return 0u;
    }

    buffered_bytes = wav->read_valid - wav->read_index;
    if (buffered_bytes >= required_bytes)
    {
        return 1u;
    }

    if (buffered_bytes > 0u)
    {
        memmove(wav->read_buffer,
                &wav->read_buffer[wav->read_index],
                buffered_bytes);
    }

    wav->read_index = 0u;
    wav->read_valid = buffered_bytes;

    if (wav->data_bytes_remaining > buffered_bytes)
    {
        unread_file_bytes = wav->data_bytes_remaining - buffered_bytes;
    }
    else
    {
        unread_file_bytes = 0u;
    }

    if (unread_file_bytes == 0u)
    {
        return (wav->read_valid >= required_bytes) ? 1u : 0u;
    }

    bytes_to_read = (uint32_t)sizeof(wav->read_buffer) - wav->read_valid;
    if (bytes_to_read > unread_file_bytes)
    {
        bytes_to_read = unread_file_bytes;
    }

    bytes_read = 0u;
    fr = f_read(&wav->file,
                &wav->read_buffer[wav->read_valid],
                bytes_to_read,
                &bytes_read);
    if ((fr != FR_OK) || (bytes_read == 0u))
    {
        return 0u;
    }

    wav->read_valid += bytes_read;
    return ((wav->read_valid - wav->read_index) >= required_bytes) ? 1u : 0u;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: WAV source sample 1개 읽기                                      */
/*                                                                            */
/*  지원 범위                                                                  */
/*  - 8bit unsigned PCM                                                        */
/*  - 16/24/32bit signed PCM                                                   */
/*  - mono/stereo                                                              */
/* -------------------------------------------------------------------------- */
static uint8_t Audio_Driver_WavReadNextSourceSample(audio_wav_runtime_t *wav,
                                                    int16_t *out_sample_s16)
{
    uint32_t frame_bytes;
    int64_t sample_accum;
    uint32_t channel_index;

    if ((wav == 0) || (out_sample_s16 == 0))
    {
        return 0u;
    }

    if ((wav->channels == 0u) || (wav->bits_per_sample == 0u) ||
        (wav->bytes_per_sample == 0u) || (wav->block_align == 0u))
    {
        return 0u;
    }

    frame_bytes = wav->block_align;
    if (wav->data_bytes_remaining < frame_bytes)
    {
        return 0u;
    }

    if (Audio_Driver_WavEnsureBufferedBytes(wav, frame_bytes) == 0u)
    {
        return 0u;
    }

    sample_accum = 0;

    for (channel_index = 0u; channel_index < wav->channels; channel_index++)
    {
        uint32_t offset;
        int32_t sample_value_s32;

        offset = wav->read_index + (channel_index * wav->bytes_per_sample);
        sample_value_s32 = 0;

        switch (wav->bits_per_sample)
        {
        case 8u:
            sample_value_s32 = (((int32_t)wav->read_buffer[offset]) - 128) << 8;
            break;

        case 16u:
            sample_value_s32 = (int16_t)Audio_Driver_U16FromLe(&wav->read_buffer[offset]);
            break;

        case 24u:
        {
            int32_t sample_24;

            sample_24 = ((int32_t)wav->read_buffer[offset + 0u]) |
                        ((int32_t)wav->read_buffer[offset + 1u] << 8) |
                        ((int32_t)wav->read_buffer[offset + 2u] << 16);

            if ((sample_24 & 0x00800000L) != 0)
            {
                sample_24 |= (int32_t)0xFF000000L;
            }

            sample_value_s32 = (sample_24 >> 8);
            break;
        }

        case 32u:
            sample_value_s32 = ((int32_t)Audio_Driver_U32FromLe(&wav->read_buffer[offset])) >> 16;
            break;

        default:
            return 0u;
        }

        sample_accum += sample_value_s32;
    }

    sample_accum /= (int64_t)wav->channels;

    if (sample_accum > 32767)
    {
        sample_accum = 32767;
    }
    else if (sample_accum < -32768)
    {
        sample_accum = -32768;
    }

    wav->read_index += frame_bytes;
    wav->data_bytes_remaining -= frame_bytes;
    ((app_audio_state_t *)&g_app_state.audio)->wav_source_data_bytes_remaining = wav->data_bytes_remaining;

    *out_sample_s16 = (int16_t)sample_accum;
    return 1u;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: WAV sample pair prime                                           */
/* -------------------------------------------------------------------------- */
static uint8_t Audio_Driver_WavPrimeSamplePair(audio_wav_runtime_t *wav)
{
    if (wav == 0)
    {
        return 0u;
    }

    if (Audio_Driver_WavReadNextSourceSample(wav, &wav->current_sample_s16) == 0u)
    {
        return 0u;
    }

    if (Audio_Driver_WavReadNextSourceSample(wav, &wav->next_sample_s16) == 0u)
    {
        wav->next_sample_s16 = wav->current_sample_s16;
        wav->end_of_stream_pending = 1u;
    }
    else
    {
        wav->end_of_stream_pending = 0u;
    }

    wav->sample_pair_valid = 1u;
    return 1u;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: WAV render sample 1개                                           */
/* -------------------------------------------------------------------------- */
static int16_t Audio_Driver_RenderWavOneSample(void)
{
    int16_t sample_s16;
    audio_wav_runtime_t *wav;

    wav = &s_audio_rt.wav;

    if (wav->active == 0u)
    {
        return 0;
    }

    if (wav->sample_pair_valid == 0u)
    {
        if (Audio_Driver_WavPrimeSamplePair(wav) == 0u)
        {
            Audio_Driver_WavClose();
            return 0;
        }
    }

    if (wav->native_rate_mode != 0u)
    {
        sample_s16 = wav->current_sample_s16;

        if (wav->end_of_stream_pending != 0u)
        {
            Audio_Driver_WavClose();
            return sample_s16;
        }

        wav->current_sample_s16 = wav->next_sample_s16;

        if (Audio_Driver_WavReadNextSourceSample(wav, &wav->next_sample_s16) == 0u)
        {
            wav->next_sample_s16 = wav->current_sample_s16;
            wav->end_of_stream_pending = 1u;
        }

        return sample_s16;
    }

    /* ---------------------------------------------------------------------- */
    /*  fallback resampler                                                      */
    /*                                                                        */
    /*  source sample rate를 DAC에 그대로 못 맞출 때만                          */
    /*  current/next 두 sample 사이를 linear interpolation 한다.               */
    /* ---------------------------------------------------------------------- */
    {
        int32_t delta;
        uint32_t frac_q32;
        int64_t interp_value;

        frac_q32 = (uint32_t)wav->resample_phase_q32;
        delta = (int32_t)wav->next_sample_s16 - (int32_t)wav->current_sample_s16;
        interp_value = (int64_t)wav->current_sample_s16 + (((int64_t)delta * (int64_t)frac_q32) >> 32);

        if (interp_value > 32767)
        {
            interp_value = 32767;
        }
        else if (interp_value < -32768)
        {
            interp_value = -32768;
        }

        sample_s16 = (int16_t)interp_value;
    }

    wav->resample_phase_q32 += wav->resample_step_q32;

    while (wav->resample_phase_q32 >= (1ull << 32))
    {
        wav->resample_phase_q32 -= (1ull << 32);
        wav->current_sample_s16 = wav->next_sample_s16;

        if (wav->end_of_stream_pending != 0u)
        {
            Audio_Driver_WavClose();
            break;
        }

        if (Audio_Driver_WavReadNextSourceSample(wav, &wav->next_sample_s16) == 0u)
        {
            wav->next_sample_s16 = wav->current_sample_s16;
            wav->end_of_stream_pending = 1u;
        }
    }

    return sample_s16;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: basename 추출                                                   */
/* -------------------------------------------------------------------------- */
static const char *Audio_Driver_GetBaseName(const char *path)
{
    const char *last_slash;
    const char *last_colon;

    if (path == 0)
    {
        return 0;
    }

    last_slash = strrchr(path, '/');
    last_colon = strrchr(path, ':');

    if ((last_slash != 0) && (last_colon != 0))
    {
        return (last_slash > last_colon) ? (last_slash + 1) : (last_colon + 1);
    }

    if (last_slash != 0)
    {
        return (last_slash + 1);
    }

    if (last_colon != 0)
    {
        return (last_colon + 1);
    }

    return path;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: WAV open + header parse                                         */
/* -------------------------------------------------------------------------- */
static HAL_StatusTypeDef Audio_Driver_OpenWaveFileInternal(const char *full_path)
{
    FRESULT fr;
    UINT bytes_read;
    uint8_t riff_header[12];
    uint8_t chunk_header[8];
    uint8_t fmt_payload[40];
    uint32_t next_offset;
    uint32_t chunk_size;
    uint32_t fmt_read_bytes;
    uint32_t data_chunk_offset;
    uint32_t data_chunk_size;
    uint16_t audio_format;
    uint16_t bytes_per_sample;
    uint16_t block_align;
    uint32_t output_sample_rate_hz;

    if (full_path == 0)
    {
        return HAL_ERROR;
    }

    Audio_Driver_StopContentInternal();
    memset(&s_audio_rt.wav, 0, sizeof(s_audio_rt.wav));

    fr = f_open(&s_audio_rt.wav.file, full_path, FA_READ);
    if (fr != FR_OK)
    {
        return HAL_ERROR;
    }

    bytes_read = 0u;
    fr = f_read(&s_audio_rt.wav.file, riff_header, sizeof(riff_header), &bytes_read);
    if ((fr != FR_OK) || (bytes_read != sizeof(riff_header)))
    {
        (void)f_close(&s_audio_rt.wav.file);
        return HAL_ERROR;
    }

    if ((memcmp(&riff_header[0], "RIFF", 4u) != 0) ||
        (memcmp(&riff_header[8], "WAVE", 4u) != 0))
    {
        (void)f_close(&s_audio_rt.wav.file);
        return HAL_ERROR;
    }

    audio_format = 0u;
    data_chunk_offset = 0u;
    data_chunk_size = 0u;

    for (;;)
    {
        bytes_read = 0u;
        fr = f_read(&s_audio_rt.wav.file, chunk_header, sizeof(chunk_header), &bytes_read);
        if ((fr != FR_OK) || (bytes_read != sizeof(chunk_header)))
        {
            break;
        }

        chunk_size = Audio_Driver_U32FromLe(&chunk_header[4]);
        next_offset = f_tell(&s_audio_rt.wav.file) + chunk_size + (chunk_size & 1u);

        if (memcmp(&chunk_header[0], "fmt ", 4u) == 0)
        {
            if (chunk_size < 16u)
            {
                break;
            }

            memset(fmt_payload, 0, sizeof(fmt_payload));
            fmt_read_bytes = (chunk_size > sizeof(fmt_payload)) ? (uint32_t)sizeof(fmt_payload) : chunk_size;

            bytes_read = 0u;
            fr = f_read(&s_audio_rt.wav.file, fmt_payload, fmt_read_bytes, &bytes_read);
            if ((fr != FR_OK) || (bytes_read != fmt_read_bytes))
            {
                break;
            }

            audio_format = Audio_Driver_U16FromLe(&fmt_payload[0]);
            s_audio_rt.wav.channels              = Audio_Driver_U16FromLe(&fmt_payload[2]);
            s_audio_rt.wav.source_sample_rate_hz = Audio_Driver_U32FromLe(&fmt_payload[4]);
            block_align                          = Audio_Driver_U16FromLe(&fmt_payload[12]);
            s_audio_rt.wav.bits_per_sample       = Audio_Driver_U16FromLe(&fmt_payload[14]);

            if ((audio_format == 0xFFFEu) && (fmt_read_bytes >= 40u))
            {
                /* ---------------------------------------------------------- */
                /*  WAVE_FORMAT_EXTENSIBLE 중 PCM subtype 만 허용              */
                /* ---------------------------------------------------------- */
                if ((fmt_payload[24] == 0x01u) && (fmt_payload[25] == 0x00u))
                {
                    audio_format = 1u;
                }
            }

            s_audio_rt.wav.block_align = block_align;
        }
        else if (memcmp(&chunk_header[0], "data", 4u) == 0)
        {
            data_chunk_offset = f_tell(&s_audio_rt.wav.file);
            data_chunk_size   = chunk_size;
        }

        if ((audio_format != 0u) && (data_chunk_offset != 0u))
        {
            break;
        }

        if (f_lseek(&s_audio_rt.wav.file, next_offset) != FR_OK)
        {
            break;
        }
    }

    if ((audio_format == 0u) || (data_chunk_offset == 0u))
    {
        (void)f_close(&s_audio_rt.wav.file);
        return HAL_ERROR;
    }

    if ((audio_format != 1u) ||
        ((s_audio_rt.wav.channels != 1u) && (s_audio_rt.wav.channels != 2u)) ||
        ((s_audio_rt.wav.bits_per_sample != 8u) &&
         (s_audio_rt.wav.bits_per_sample != 16u) &&
         (s_audio_rt.wav.bits_per_sample != 24u) &&
         (s_audio_rt.wav.bits_per_sample != 32u)) ||
        ((s_audio_rt.wav.bits_per_sample % 8u) != 0u) ||
        (s_audio_rt.wav.source_sample_rate_hz == 0u))
    {
        (void)f_close(&s_audio_rt.wav.file);
        return HAL_ERROR;
    }

    bytes_per_sample = (uint16_t)(s_audio_rt.wav.bits_per_sample / 8u);
    block_align = (uint16_t)(s_audio_rt.wav.channels * bytes_per_sample);

    if (s_audio_rt.wav.block_align == 0u)
    {
        s_audio_rt.wav.block_align = block_align;
    }

    if (s_audio_rt.wav.block_align < block_align)
    {
        (void)f_close(&s_audio_rt.wav.file);
        return HAL_ERROR;
    }

    s_audio_rt.wav.bytes_per_sample = bytes_per_sample;
    s_audio_rt.wav.data_start_offset = data_chunk_offset;
    s_audio_rt.wav.data_bytes_remaining = data_chunk_size;

    output_sample_rate_hz = Audio_Driver_ClampOutputSampleRate(s_audio_rt.wav.source_sample_rate_hz);
    if (Audio_Driver_ApplyOutputSampleRate(output_sample_rate_hz) != HAL_OK)
    {
        (void)f_close(&s_audio_rt.wav.file);
        return HAL_ERROR;
    }

    if (f_lseek(&s_audio_rt.wav.file, s_audio_rt.wav.data_start_offset) != FR_OK)
    {
        (void)f_close(&s_audio_rt.wav.file);
        return HAL_ERROR;
    }

    s_audio_rt.wav.output_sample_rate_hz = output_sample_rate_hz;
    s_audio_rt.wav.native_rate_mode = (s_audio_rt.wav.source_sample_rate_hz == output_sample_rate_hz) ? 1u : 0u;
    s_audio_rt.wav.resample_phase_q32 = 0u;
    s_audio_rt.wav.resample_step_q32  = (((uint64_t)s_audio_rt.wav.source_sample_rate_hz) << 32) /
                                        (uint64_t)output_sample_rate_hz;
    s_audio_rt.wav.read_index         = 0u;
    s_audio_rt.wav.read_valid         = 0u;
    s_audio_rt.wav.sample_pair_valid  = 0u;
    s_audio_rt.wav.end_of_stream_pending = 0u;
    s_audio_rt.wav.active             = 1u;

    Audio_Driver_CopyTextSafe(s_audio_rt.wav.full_path,
                              sizeof(s_audio_rt.wav.full_path),
                              full_path);
    Audio_Driver_CopyTextSafe(s_audio_rt.wav.display_name,
                              sizeof(s_audio_rt.wav.display_name),
                              Audio_Driver_GetBaseName(full_path));

    ((app_audio_state_t *)&g_app_state.audio)->wav_active = true;
    ((app_audio_state_t *)&g_app_state.audio)->wav_native_rate_active = (s_audio_rt.wav.native_rate_mode != 0u) ? 1u : 0u;
    ((app_audio_state_t *)&g_app_state.audio)->mode = APP_AUDIO_MODE_WAV_FILE;
    ((app_audio_state_t *)&g_app_state.audio)->content_active = true;
    ((app_audio_state_t *)&g_app_state.audio)->playback_start_ms = HAL_GetTick();
    ((app_audio_state_t *)&g_app_state.audio)->wav_source_sample_rate_hz = s_audio_rt.wav.source_sample_rate_hz;
    ((app_audio_state_t *)&g_app_state.audio)->wav_source_data_bytes_remaining = s_audio_rt.wav.data_bytes_remaining;
    ((app_audio_state_t *)&g_app_state.audio)->wav_source_channels = s_audio_rt.wav.channels;
    ((app_audio_state_t *)&g_app_state.audio)->wav_source_bits_per_sample = s_audio_rt.wav.bits_per_sample;

    Audio_Driver_CopyTextSafe(((app_audio_state_t *)&g_app_state.audio)->last_wav_path,
                              sizeof(((app_audio_state_t *)&g_app_state.audio)->last_wav_path),
                              full_path);
    Audio_Driver_SetCurrentName(s_audio_rt.wav.display_name);

    return HAL_OK;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: block render                                                    */
/* -------------------------------------------------------------------------- */
static void Audio_Driver_RenderBlock(uint16_t *dst,
                                     uint32_t sample_count,
                                     uint32_t now_ms)
{
    uint32_t sample_index;
    uint16_t block_min_u12;
    uint16_t block_max_u12;
    uint8_t block_clipped;
    app_audio_state_t *audio;

    if (dst == 0)
    {
        return;
    }

    audio = (app_audio_state_t *)&g_app_state.audio;
    block_min_u12 = 4095u;
    block_max_u12 = 0u;
    block_clipped = 0u;

    for (sample_index = 0u; sample_index < sample_count; sample_index++)
    {
        uint32_t voice_index;
        uint8_t active_source_count;
        uint16_t mix_scale_q15;
        int32_t mixed_accum_s16;
        int32_t dac_value;
        int16_t mixed_sample_s16;

        if (s_audio_rt.sequence.active != 0u)
        {
            Audio_Driver_SequenceAdvanceOneSample();
        }

        mixed_accum_s16 = 0;
        active_source_count = 0u;

        for (voice_index = 0u; voice_index < APP_AUDIO_MAX_VOICES; voice_index++)
        {
            if (s_audio_rt.voices[voice_index].active != 0u)
            {
                active_source_count++;
            }

            mixed_accum_s16 += (int32_t)Audio_Driver_RenderVoiceOneSample(&s_audio_rt.voices[voice_index]);
        }

        if (s_audio_rt.wav.active != 0u)
        {
            active_source_count++;
        }

        mixed_accum_s16 += (int32_t)Audio_Driver_RenderWavOneSample();

        mix_scale_q15 = Audio_Driver_GetMixScaleQ15(active_source_count);
        mixed_accum_s16 = (int32_t)(((int64_t)mixed_accum_s16 * (int64_t)mix_scale_q15) >> 15);
        mixed_accum_s16 = (int32_t)(((int64_t)mixed_accum_s16 * (int64_t)s_audio_rt.current_output_gain_q15) >> 15);

        mixed_sample_s16 = Audio_Driver_SoftClipS16(mixed_accum_s16, &block_clipped);
        dac_value = (int32_t)AUDIO_DAC_MIDPOINT_U12 + ((int32_t)mixed_sample_s16 >> 4);

        if (dac_value < 0)
        {
            dac_value = 0;
            block_clipped = 1u;
        }
        else if (dac_value > 4095)
        {
            dac_value = 4095;
            block_clipped = 1u;
        }

        dst[sample_index] = (uint16_t)dac_value;

        if ((uint16_t)dac_value < block_min_u12)
        {
            block_min_u12 = (uint16_t)dac_value;
        }
        if ((uint16_t)dac_value > block_max_u12)
        {
            block_max_u12 = (uint16_t)dac_value;
        }
    }

    if ((s_audio_rt.sequence.active != 0u) && (Audio_Driver_SequenceIsFinished() != 0u))
    {
        Audio_Driver_ClearSequence();
    }

    Audio_Driver_PublishAllVoiceSnapshots();
    Audio_Driver_UpdateActiveVoiceCount();

    audio->last_block_min_u12 = block_min_u12;
    audio->last_block_max_u12 = block_max_u12;
    audio->last_block_clipped = block_clipped;
    audio->last_update_ms = now_ms;
    audio->render_block_count++;

    if (block_clipped != 0u)
    {
        audio->clip_block_count++;
    }

    /* ---------------------------------------------------------------------- */
    /*  여기서는 content_active를 idle로 내리지 않는다.                         */
    /*                                                                        */
    /*  이유                                                                   */
    /*  - 이번 block이 source의 마지막 block이어도                             */
    /*    caller가 이 block을 FIFO에 push한 뒤에는 아직 tail이 남아 있다.       */
    /*  - 따라서 busy/idle 판단은                                              */
    /*    Audio_Driver_UpdatePipelineFlags()가                                 */
    /*    "source + FIFO" 전체를 보고 결정해야 한다.                           */
    /* ---------------------------------------------------------------------- */
    audio->content_active = true;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: whole DMA buffer 즉시 prime                                      */
/* -------------------------------------------------------------------------- */
static void Audio_Driver_ProducerRefillToTarget(uint32_t now_ms,
                                               uint32_t target_level_samples,
                                               uint32_t max_blocks)
{
    uint32_t refill_count;

    if (target_level_samples > AUDIO_SW_FIFO_SAMPLES)
    {
        target_level_samples = AUDIO_SW_FIFO_SAMPLES;
    }

    refill_count = 0u;

    while (refill_count < max_blocks)
    {
        uint32_t fifo_level_samples;
        uint32_t fifo_free_samples;
        uint32_t need_samples;
        uint32_t render_samples;
        uint32_t written_samples;

        if (Audio_Driver_HasRenderableContent() == 0u)
        {
            break;
        }

        fifo_level_samples = Audio_Driver_FifoLevelSamples();
        if (fifo_level_samples >= target_level_samples)
        {
            break;
        }

        fifo_free_samples = Audio_Driver_FifoFreeSamples();
        if (fifo_free_samples == 0u)
        {
            break;
        }

        need_samples = target_level_samples - fifo_level_samples;
        render_samples = AUDIO_PRODUCER_CHUNK_SAMPLES;

        if (render_samples > need_samples)
        {
            render_samples = need_samples;
        }
        if (render_samples > fifo_free_samples)
        {
            render_samples = fifo_free_samples;
        }
        if (render_samples == 0u)
        {
            break;
        }

        Audio_Driver_RenderBlock(s_audio_rt.producer_block,
                                 render_samples,
                                 now_ms);

        written_samples = Audio_Driver_FifoPushSamples(s_audio_rt.producer_block,
                                                       render_samples);
        ((app_audio_state_t *)&g_app_state.audio)->producer_refill_block_count++;

        if (written_samples != render_samples)
        {
            break;
        }

        refill_count++;

        /* ------------------------------------------------------------------ */
        /*  이번 block을 push한 뒤 source가 종료되었다면                         */
        /*  이후에는 silence를 FIFO에 미리 적재하지 않는다.                      */
        /*  부족한 순간은 ISR이 직접 silence padding으로 처리한다.              */
        /* ------------------------------------------------------------------ */
        if (Audio_Driver_HasRenderableContent() == 0u)
        {
            break;
        }
    }

    Audio_Driver_PublishFifoTelemetry();
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: whole DMA buffer 즉시 prime                                      */
/*                                                                            */
/*  content start 직후에는 producer가 FIFO를 먼저 충분히 채우고,               */
/*  그 다음 DMA buffer 양 half를 FIFO에서 직접 꺼내 와서                      */
/*  첫 audible block부터 새 content가 바로 들리게 만든다.                     */
/* -------------------------------------------------------------------------- */
static void Audio_Driver_PrimeWholeBuffer(uint32_t now_ms)
{
    uint32_t target_level_samples;
    uint32_t max_blocks;

    target_level_samples = AUDIO_SW_FIFO_HIGH_WATERMARK_SAMPLES;
    if (target_level_samples < AUDIO_DMA_BUFFER_SAMPLES)
    {
        target_level_samples = AUDIO_DMA_BUFFER_SAMPLES;
    }

    max_blocks = (AUDIO_SW_FIFO_SAMPLES / AUDIO_PRODUCER_CHUNK_SAMPLES) + 2u;
    Audio_Driver_ProducerRefillToTarget(now_ms,
                                        target_level_samples,
                                        max_blocks);

    __disable_irq();
    Audio_Driver_ServiceDmaHalfFromFifo(0u);
    Audio_Driver_ServiceDmaHalfFromFifo(AUDIO_DMA_BUFFER_SAMPLES / 2u);
    __enable_irq();

    Audio_Driver_UpdatePipelineFlags(now_ms);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: tone 이름 문자열 생성                                           */
/* -------------------------------------------------------------------------- */
static void Audio_Driver_BuildToneName(char *out_text,
                                       size_t out_size,
                                       const char *wave_name,
                                       uint32_t freq_hz)
{
    if ((out_text == 0) || (out_size == 0u))
    {
        return;
    }

    snprintf(out_text, out_size, "%s_%luhz",
             (wave_name != 0) ? wave_name : "TONE",
             (unsigned long)freq_hz);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: simple tone 공통 시작                                           */
/* -------------------------------------------------------------------------- */
static HAL_StatusTypeDef Audio_Driver_PlaySimpleTone(audio_timbre_preset_id_t timbre_id,
                                                     const char *name_prefix,
                                                     uint32_t freq_hz,
                                                     uint32_t time_ms)
{
    uint32_t total_samples;
    uint32_t gate_samples;
    char name_buffer[APP_AUDIO_NAME_MAX];

    if ((freq_hz == 0u) || (time_ms == 0u))
    {
        return HAL_ERROR;
    }

    if (Audio_Driver_ApplyOutputSampleRate(AUDIO_SAMPLE_RATE_HZ) != HAL_OK)
    {
        return HAL_ERROR;
    }

    Audio_Driver_StopContentInternal();

    total_samples = ((uint64_t)time_ms * Audio_Driver_GetActiveOutputSampleRate() + 999u) / 1000u;
    if (total_samples == 0u)
    {
        total_samples = 1u;
    }

    gate_samples = (total_samples * AUDIO_GATE_PERCENT_DEFAULT) / 100u;
    if (gate_samples == 0u)
    {
        gate_samples = 1u;
    }

    Audio_Driver_StartVoice(0u,
                            0u,
                            timbre_id,
                            (freq_hz * 100u),
                            total_samples,
                            gate_samples,
                            1000u);

    ((app_audio_state_t *)&g_app_state.audio)->mode = APP_AUDIO_MODE_TONE;
    ((app_audio_state_t *)&g_app_state.audio)->content_active = true;
    ((app_audio_state_t *)&g_app_state.audio)->wav_active = false;
    ((app_audio_state_t *)&g_app_state.audio)->wav_native_rate_active = false;
    ((app_audio_state_t *)&g_app_state.audio)->playback_start_ms = HAL_GetTick();

    Audio_Driver_BuildToneName(name_buffer, sizeof(name_buffer), name_prefix, freq_hz);
    Audio_Driver_SetCurrentName(name_buffer);

    Audio_Driver_PrimeWholeBuffer(HAL_GetTick());
    return HAL_OK;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: init                                                             */
/* -------------------------------------------------------------------------- */
void Audio_Driver_Init(void)
{
    app_audio_state_t *audio;
    uint32_t default_sample_rate_hz;

    audio = (app_audio_state_t *)&g_app_state.audio;
    default_sample_rate_hz = Audio_Driver_ClampOutputSampleRate(AUDIO_SAMPLE_RATE_HZ);

    memset(&s_audio_rt, 0, sizeof(s_audio_rt));
    Audio_Driver_ResetAppStateSlice(audio);
    Audio_Driver_BuildVolumeTable();
    Audio_Driver_SetVolumePercentInternal(AUDIO_DEFAULT_VOLUME_PERCENT);
    Audio_Driver_FifoReset();
    Audio_Driver_PublishFifoTelemetry();

    if (Audio_Driver_ConfigureTimerRuntime(default_sample_rate_hz) != HAL_OK)
    {
        return;
    }

    if (Audio_Driver_ConfigureDacRuntime() != HAL_OK)
    {
        return;
    }

    if (Audio_Driver_StartTransport() != HAL_OK)
    {
        return;
    }

    audio->initialized = true;
    audio->transport_running = true;
    audio->sample_rate_hz = default_sample_rate_hz;
    audio->output_resolution_bits = AUDIO_DAC_OUTPUT_BITS;

    /* ---------------------------------------------------------------------- */
    /*  첫 버퍼는 silence로 확실히 채워서                                      */
    /*  부팅 직후 임의 전압이 나오지 않게 한다.                                 */
    /* ---------------------------------------------------------------------- */
    Audio_Driver_FillBufferWithSilence(s_audio_rt.dma_buffer, AUDIO_DMA_BUFFER_SAMPLES);
    Audio_Driver_PublishFifoTelemetry();
    Audio_Driver_PublishAllVoiceSnapshots();
}

/* -------------------------------------------------------------------------- */
/*  공개 API: simple waveform playback                                         */
/* -------------------------------------------------------------------------- */
HAL_StatusTypeDef Audio_Driver_PlaySineWave(uint32_t freq_hz,
                                            uint32_t time_ms)
{
    return Audio_Driver_PlaySimpleTone(AUDIO_TIMBRE_PURE_SINE,
                                       "SINE",
                                       freq_hz,
                                       time_ms);
}

HAL_StatusTypeDef Audio_Driver_PlaySquareWaveMs(uint32_t freq_hz,
                                                uint32_t time_ms)
{
    return Audio_Driver_PlaySimpleTone(AUDIO_TIMBRE_PURE_SQUARE,
                                       "SQUARE",
                                       freq_hz,
                                       time_ms);
}

HAL_StatusTypeDef Audio_Driver_PlaySawToothWaveMs(uint32_t freq_hz,
                                                  uint32_t time_ms)
{
    return Audio_Driver_PlaySimpleTone(AUDIO_TIMBRE_PURE_SAW,
                                       "SAW",
                                       freq_hz,
                                       time_ms);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 4채널 sequence preset playback                                   */
/* -------------------------------------------------------------------------- */
HAL_StatusTypeDef Audio_Driver_PlaySequencePreset(audio_timbre_stack_preset_id_t timbre_stack_id,
                                                  audio_note_preset_id_t note_preset_id)
{
    const audio_timbre_stack_preset_t *stack_preset;
    const audio_note_preset_t *note_preset;
    app_audio_state_t *audio;

    stack_preset = Audio_Presets_GetTimbreStack(timbre_stack_id);
    note_preset  = Audio_Presets_GetNotePreset(note_preset_id);
    audio = (app_audio_state_t *)&g_app_state.audio;

    if ((stack_preset == 0) || (note_preset == 0))
    {
        return HAL_ERROR;
    }

    if (Audio_Driver_ApplyOutputSampleRate(AUDIO_SAMPLE_RATE_HZ) != HAL_OK)
    {
        return HAL_ERROR;
    }

    Audio_Driver_StopContentInternal();

    s_audio_rt.sequence.active             = 1u;
    s_audio_rt.sequence.mono_mode          = 0u;
    s_audio_rt.sequence.melody_track_index = note_preset->melody_track_index;
    s_audio_rt.sequence.bpm                = note_preset->bpm;
    s_audio_rt.sequence.note_preset        = note_preset;
    s_audio_rt.sequence.stack_preset       = stack_preset;
    s_audio_rt.sequence.mono_timbre_id     = AUDIO_TIMBRE_CH3_PIANO;

    audio->mode              = APP_AUDIO_MODE_SEQUENCE_MIX;
    audio->content_active    = true;
    audio->wav_active        = false;
    audio->wav_native_rate_active = false;
    audio->sequence_bpm      = note_preset->bpm;
    audio->playback_start_ms = HAL_GetTick();

    Audio_Driver_SetCurrentName(note_preset->name);
    Audio_Driver_PrimeWholeBuffer(HAL_GetTick());

    return HAL_OK;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: melody track 단일 재생                                           */
/* -------------------------------------------------------------------------- */
HAL_StatusTypeDef Audio_Driver_PlaySingleTrackPreset(audio_timbre_preset_id_t timbre_id,
                                                     audio_note_preset_id_t note_preset_id)
{
    const audio_note_preset_t *note_preset;
    app_audio_state_t *audio;

    note_preset = Audio_Presets_GetNotePreset(note_preset_id);
    audio = (app_audio_state_t *)&g_app_state.audio;

    if ((note_preset == 0) || (Audio_Presets_GetTimbre(timbre_id) == 0))
    {
        return HAL_ERROR;
    }

    if (Audio_Driver_ApplyOutputSampleRate(AUDIO_SAMPLE_RATE_HZ) != HAL_OK)
    {
        return HAL_ERROR;
    }

    Audio_Driver_StopContentInternal();

    s_audio_rt.sequence.active             = 1u;
    s_audio_rt.sequence.mono_mode          = 1u;
    s_audio_rt.sequence.melody_track_index = note_preset->melody_track_index;
    s_audio_rt.sequence.bpm                = note_preset->bpm;
    s_audio_rt.sequence.note_preset        = note_preset;
    s_audio_rt.sequence.stack_preset       = 0;
    s_audio_rt.sequence.mono_timbre_id     = timbre_id;

    audio->mode              = APP_AUDIO_MODE_SEQUENCE_MONO;
    audio->content_active    = true;
    audio->wav_active        = false;
    audio->wav_native_rate_active = false;
    audio->sequence_bpm      = note_preset->bpm;
    audio->playback_start_ms = HAL_GetTick();

    Audio_Driver_SetCurrentName(note_preset->name);
    Audio_Driver_PrimeWholeBuffer(HAL_GetTick());

    return HAL_OK;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 지정 경로 WAV playback                                           */
/* -------------------------------------------------------------------------- */
HAL_StatusTypeDef Audio_Driver_PlayWaveFilePath(const char *path)
{
    char full_path[AUDIO_WAV_FULL_PATH_MAX];

    if (((const app_audio_state_t *)&g_app_state.audio)->initialized == false)
    {
        return HAL_ERROR;
    }

    if ((path == 0) || (path[0] == '\0'))
    {
        return HAL_ERROR;
    }

    if ((strchr(path, ':') != 0) || (path[0] == '/'))
    {
        Audio_Driver_CopyTextSafe(full_path, sizeof(full_path), path);
    }
    else
    {
        snprintf(full_path, sizeof(full_path), "%s/%s", SDPath, path);
    }

    if (Audio_Driver_OpenWaveFileInternal(full_path) != HAL_OK)
    {
        return HAL_ERROR;
    }

    Audio_Driver_PrimeWholeBuffer(HAL_GetTick());
    return HAL_OK;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: root에서 아무 WAV 하나 재생                                      */
/* -------------------------------------------------------------------------- */
HAL_StatusTypeDef Audio_Driver_PlayAnyWaveFromSd(void)
{
    DIR dir;
    FILINFO info;
    FRESULT fr;
    char full_path[AUDIO_WAV_FULL_PATH_MAX];

    if (((const app_audio_state_t *)&g_app_state.audio)->initialized == false)
    {
        return HAL_ERROR;
    }

    if (((const app_sd_state_t *)&g_app_state.sd)->mounted == false)
    {
        return HAL_ERROR;
    }

    fr = f_opendir(&dir, SDPath);
    if (fr != FR_OK)
    {
        return HAL_ERROR;
    }

    for (;;)
    {
        fr = f_readdir(&dir, &info);
        if (fr != FR_OK)
        {
            (void)f_closedir(&dir);
            return HAL_ERROR;
        }

        if (info.fname[0] == '\0')
        {
            (void)f_closedir(&dir);
            return HAL_ERROR;
        }

#ifdef AM_DIR
        if ((info.fattrib & AM_DIR) != 0u)
        {
            continue;
        }
#endif

        if (Audio_Driver_IsWaveFileName(info.fname) == 0u)
        {
            continue;
        }

        snprintf(full_path, sizeof(full_path), "%s/%s", SDPath, info.fname);
        (void)f_closedir(&dir);
        return Audio_Driver_PlayWaveFilePath(full_path);
    }
}

/* -------------------------------------------------------------------------- */
/*  공개 API: stop                                                             */
/* -------------------------------------------------------------------------- */
void Audio_Driver_Stop(void)
{
    Audio_Driver_StopContentInternal();
    Audio_Driver_FillBufferWithSilence(s_audio_rt.dma_buffer, AUDIO_DMA_BUFFER_SAMPLES);
    ((app_audio_state_t *)&g_app_state.audio)->last_block_clipped = 0u;
    Audio_Driver_PublishFifoTelemetry();
}

/* -------------------------------------------------------------------------- */
/*  공개 API: busy 여부                                                        */
/* -------------------------------------------------------------------------- */
bool Audio_Driver_IsBusy(void)
{
    if (Audio_Driver_HasRenderableContent() != 0u)
    {
        return true;
    }

    if (Audio_Driver_FifoLevelSamples() != 0u)
    {
        return true;
    }

    return (((const app_audio_state_t *)&g_app_state.audio)->content_active != false) ? true : false;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: snapshot copy helper                                             */
/* -------------------------------------------------------------------------- */
void Audio_Driver_CopySnapshot(app_audio_state_t *out_snapshot)
{
    APP_STATE_CopyAudioSnapshot(out_snapshot);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: main loop task                                                   */
/* -------------------------------------------------------------------------- */
void Audio_Driver_Task(uint32_t now_ms)
{
    uint32_t fifo_level_samples;

    if (((const app_audio_state_t *)&g_app_state.audio)->initialized == false)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  producer(main context)는 가능한 한 FIFO를 high watermark 근처까지       */
    /*  미리 채워 두려고 한다.                                                  */
    /*                                                                        */
    /*  이렇게 하면                                                             */
    /*  - WAV f_read 지터                                                       */
    /*  - UI blocking SPI 전송                                                  */
    /*  - 다른 센서 task로 인한 loop 지연                                       */
    /*  을 ISR가 직접 맞지 않고 FIFO가 먼저 흡수해 준다.                        */
    /* ---------------------------------------------------------------------- */
    fifo_level_samples = Audio_Driver_FifoLevelSamples();
    if ((Audio_Driver_HasRenderableContent() != 0u) &&
        (fifo_level_samples < AUDIO_SW_FIFO_HIGH_WATERMARK_SAMPLES))
    {
        Audio_Driver_ProducerRefillToTarget(now_ms,
                                            AUDIO_SW_FIFO_HIGH_WATERMARK_SAMPLES,
                                            AUDIO_PRODUCER_MAX_BLOCKS_PER_TASK);
    }

    Audio_Driver_UpdatePipelineFlags(now_ms);
}

/* -------------------------------------------------------------------------- */
/*  HAL callback: DMA half complete                                            */
/* -------------------------------------------------------------------------- */
void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef *hdac_handle)
{
    if (hdac_handle == 0)
    {
        return;
    }

    if (hdac_handle->Instance != AUDIO_DAC_HANDLE.Instance)
    {
        return;
    }

    ((app_audio_state_t *)&g_app_state.audio)->half_callback_count++;
    Audio_Driver_ServiceDmaHalfFromFifo(0u);
}

/* -------------------------------------------------------------------------- */
/*  HAL callback: DMA full complete                                            */
/* -------------------------------------------------------------------------- */
void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *hdac_handle)
{
    if (hdac_handle == 0)
    {
        return;
    }

    if (hdac_handle->Instance != AUDIO_DAC_HANDLE.Instance)
    {
        return;
    }

    ((app_audio_state_t *)&g_app_state.audio)->full_callback_count++;
    Audio_Driver_ServiceDmaHalfFromFifo(AUDIO_DMA_BUFFER_SAMPLES / 2u);
}

/* -------------------------------------------------------------------------- */
/*  HAL callback: DAC DMA underrun                                             */
/* -------------------------------------------------------------------------- */
void HAL_DAC_DMAUnderrunCallbackCh1(DAC_HandleTypeDef *hdac_handle)
{
    if (hdac_handle == 0)
    {
        return;
    }

    if (hdac_handle->Instance != AUDIO_DAC_HANDLE.Instance)
    {
        return;
    }

    ((app_audio_state_t *)&g_app_state.audio)->dma_underrun_count++;
}
