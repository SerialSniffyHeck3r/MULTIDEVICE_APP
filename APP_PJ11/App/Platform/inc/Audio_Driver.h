#ifndef AUDIO_DRIVER_H
#define AUDIO_DRIVER_H

#include "main.h"
#include "APP_STATE.h"
#include "Audio_Presets.h"
#include "dac.h"
#include "fatfs.h"
#include "tim.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  Audio_Driver                                                               */
/*                                                                            */
/*  이 파일이 맡는 역할                                                        */
/*  - DAC1 CH1 + TIM6 TRGO + DMA circular transport를 사용해서                 */
/*    non-blocking 오디오 출력을 만든다.                                       */
/*  - 단순파(sine/square/saw), 4채널 시퀀스, single-track 비교 재생,            */
/*    SD card WAV 테스트 재생을 하나의 엔진 위에서 처리한다.                   */
/*                                                                            */
/*  이번 보강의 핵심                                                           */
/*  - source WAV sample rate가 표준 범위 안이면 DAC도 가능한 한               */
/*    그 sample rate에 맞춰 직접 재생한다.                                     */
/*  - source rate가 너무 높거나 제한 범위를 벗어나면                           */
/*    저오버헤드 linear interpolation resampler로 fallback 한다.               */
/*  - volume은 0~100 UI 값으로 받되, 사람 귀 기준 체감 선형에 가깝게            */
/*    들리도록 perceptual LUT를 driver init 시점에 1회 만든다.                 */
/*  - mixing은 active source count 기반 headroom + soft clip safety path를    */
/*    넣어, 단순 saturation보다 훨씬 덜 거칠게 만든다.                         */
/*  - ISR 안에서는 FATFS read나 파형 합성을 절대 하지 않고,                    */
/*    software PCM FIFO에서 DMA half-buffer로 memcpy만 수행한다.               */
/*                                                                            */
/*  중요한 설계 포인트                                                         */
/*  - ISR/DMA callback은 "이미 만들어 둔 샘플" 을 DMA 반쪽 버퍼로 옮기는      */
/*    역할만 맡는다.                                                           */
/*  - 실제 파형 합성, 시퀀스 스케줄링, WAV 파일 읽기/보간은                    */
/*    Audio_Driver_Task()가 main context에서 수행한다.                         */
/*  - steady-state render path에는 float 연산을 넣지 않는다.                   */
/*    pow()는 볼륨 LUT를 만들 때 init 시점에만 1회 사용한다.                   */
/* -------------------------------------------------------------------------- */

#ifndef AUDIO_DAC_HANDLE
#define AUDIO_DAC_HANDLE hdac
#endif

#ifndef AUDIO_DAC_CHANNEL
#define AUDIO_DAC_CHANNEL DAC_CHANNEL_1
#endif

#ifndef AUDIO_TIMER_HANDLE
#define AUDIO_TIMER_HANDLE htim6
#endif

/* -------------------------------------------------------------------------- */
/*  synth / sequence 기본 출력 sample rate                                     */
/*                                                                            */
/*  tone/sequence는 이 sample rate를 기본으로 사용한다.                        */
/*  WAV는 가능하면 원본 sample rate를 그대로 따라간다.                         */
/*                                                                            */
/*  24kHz는 CPU/메모리/대역폭 균형이 가장 무난하다.                             */
/*  필요 시 32000u 로 바꿔도 구조는 그대로 유지된다.                           */
/* -------------------------------------------------------------------------- */
#ifndef AUDIO_SAMPLE_RATE_HZ
#define AUDIO_SAMPLE_RATE_HZ 24000u
#endif

/* -------------------------------------------------------------------------- */
/*  WAV native-rate direct playback 허용 범위                                  */
/*                                                                            */
/*  44.1k / 48k는 충분히 실용적이지만,                                          */
/*  96k 이상을 그대로 받으면 callback/render 빈도가 불필요하게 올라간다.       */
/*  그래서 기본 상한은 48k로 두고, 그 이상은 fallback resampler를 탄다.        */
/* -------------------------------------------------------------------------- */
#ifndef AUDIO_OUTPUT_SAMPLE_RATE_MIN_HZ
#define AUDIO_OUTPUT_SAMPLE_RATE_MIN_HZ 8000u
#endif

#ifndef AUDIO_OUTPUT_SAMPLE_RATE_MAX_HZ
#define AUDIO_OUTPUT_SAMPLE_RATE_MAX_HZ 48000u
#endif

/* -------------------------------------------------------------------------- */
/*  DMA buffer 크기                                                            */
/*                                                                            */
/*  transport 쪽 half-buffer는 IRQ 주기와 지연을 동시에 좌우한다.               */
/*  1024 sample total buffer는                                                 */
/*    - 24kHz 에서 half 512 sample  ~= 21.3ms                                 */
/*    - 44.1kHz 에서 half 512 sample ~= 11.6ms                                */
/*  정도가 되어, ISR memcpy 부담은 작고 지연도 과하지 않다.                    */
/* -------------------------------------------------------------------------- */
#ifndef AUDIO_DMA_BUFFER_SAMPLES
#define AUDIO_DMA_BUFFER_SAMPLES 1024u
#endif

/* -------------------------------------------------------------------------- */
/*  software PCM FIFO 크기                                                     */
/*                                                                            */
/*  이 FIFO가 main loop 지터, FATFS read 지터,                                 */
/*  그리고 UI blocking 전송의 영향을 흡수한다.                                */
/*                                                                            */
/*  8192 sample은                                                             */
/*    - 24kHz 에서 약 341ms                                                    */
/*    - 32kHz 에서 약 256ms                                                    */
/*    - 44.1kHz 에서 약 186ms                                                  */
/*  분량이라, RTOS 없이도 오디오를 꽤 독립적으로 유지하기 좋다.                */
/* -------------------------------------------------------------------------- */
#ifndef AUDIO_SW_FIFO_SAMPLES
#define AUDIO_SW_FIFO_SAMPLES 8192u
#endif

/* -------------------------------------------------------------------------- */
/*  producer refill low/high watermark                                         */
/*                                                                            */
/*  main context는 FIFO 수위가 low 아래로 내려가기 전에                        */
/*  가능한 한 high 쪽까지 다시 채우려 한다.                                    */
/* -------------------------------------------------------------------------- */
#ifndef AUDIO_SW_FIFO_LOW_WATERMARK_SAMPLES
#define AUDIO_SW_FIFO_LOW_WATERMARK_SAMPLES 2048u
#endif

#ifndef AUDIO_SW_FIFO_HIGH_WATERMARK_SAMPLES
#define AUDIO_SW_FIFO_HIGH_WATERMARK_SAMPLES 6144u
#endif

/* -------------------------------------------------------------------------- */
/*  producer render chunk                                                      */
/*                                                                            */
/*  main context에서 한 번에 몇 sample씩 합성할지를 정한다.                    */
/*  half-buffer와 같은 512 sample을 기본으로 두어                              */
/*  구현을 단순하게 하고 memcpy 단위와도 맞춘다.                               */
/* -------------------------------------------------------------------------- */
#ifndef AUDIO_PRODUCER_CHUNK_SAMPLES
#define AUDIO_PRODUCER_CHUNK_SAMPLES 512u
#endif

/* -------------------------------------------------------------------------- */
/*  main loop 1회당 최대 refill block 수                                       */
/*                                                                            */
/*  Audio_Driver_Task()가 너무 오래 붙잡지 않도록                              */
/*  한 번의 진입에서 채울 최대 block 수를 제한한다.                            */
/*  startup prime은 별도 경로로 더 많이 채울 수 있다.                          */
/* -------------------------------------------------------------------------- */
#ifndef AUDIO_PRODUCER_MAX_BLOCKS_PER_TASK
#define AUDIO_PRODUCER_MAX_BLOCKS_PER_TASK 8u
#endif

/* -------------------------------------------------------------------------- */
/*  아날로그 안전 headroom                                                     */
/*                                                                            */
/*  PAM8403류 보드가 이득이 높은 편이므로,                                      */
/*  100% volume이라도 DAC full-scale를 그대로 박지 않고                         */
/*  먼저 고정 headroom을 둔다.                                                  */
/* -------------------------------------------------------------------------- */
#ifndef AUDIO_ANALOG_SAFE_HEADROOM_Q15
#define AUDIO_ANALOG_SAFE_HEADROOM_Q15 19661u
#endif

/* -------------------------------------------------------------------------- */
/*  UI volume 기본값                                                           */
/* -------------------------------------------------------------------------- */
#ifndef AUDIO_DEFAULT_VOLUME_PERCENT
#define AUDIO_DEFAULT_VOLUME_PERCENT 75u
#endif

/* -------------------------------------------------------------------------- */
/*  perceptual volume curve 최소 dB                                            */
/*                                                                            */
/*  0%는 hard mute로 처리하고, 1~100%는                                        */
/*  이 값부터 0dB까지 선형 dB 스케일로 LUT를 만든다.                           */
/* -------------------------------------------------------------------------- */
#ifndef AUDIO_VOLUME_CURVE_MIN_DB
#define AUDIO_VOLUME_CURVE_MIN_DB (-24.0)
#endif

#ifndef AUDIO_DEFAULT_TEST_TONE_MS
#define AUDIO_DEFAULT_TEST_TONE_MS 300u
#endif

/* -------------------------------------------------------------------------- */
/*  WAV stream read chunk                                                      */
/*                                                                            */
/*  4KB 정도면 FATFS read call 횟수를 줄이면서도                                */
/*  RAM 사용량을 과도하게 키우지 않는다.                                       */
/* -------------------------------------------------------------------------- */
#ifndef AUDIO_WAV_STREAM_READ_CHUNK
#define AUDIO_WAV_STREAM_READ_CHUNK 4096u
#endif

#ifndef AUDIO_DAC_USE_OUTPUT_BUFFER
#define AUDIO_DAC_USE_OUTPUT_BUFFER 1u
#endif

/* -------------------------------------------------------------------------- */
/*  compile-time guard                                                         */
/*                                                                            */
/*  circular DMA half/full callback를 쓸 것이므로                               */
/*  DMA buffer sample 수는 반드시 짝수여야 한다.                               */
/* -------------------------------------------------------------------------- */
typedef char audio_dma_buffer_size_must_be_even[
    ((AUDIO_DMA_BUFFER_SAMPLES % 2u) == 0u) ? 1 : -1];

/* -------------------------------------------------------------------------- */
/*  compile-time guard                                                         */
/*                                                                            */
/*  software FIFO는 SPSC ring buffer로 사용하므로                              */
/*  index mask 최적화를 위해 power-of-two 크기를 권장하는 것이 아니라          */
/*  이번 구현에서는 아예 필수로 둔다.                                          */
/* -------------------------------------------------------------------------- */
typedef char audio_sw_fifo_size_must_be_power_of_two[
    ((AUDIO_SW_FIFO_SAMPLES != 0u) &&
     ((AUDIO_SW_FIFO_SAMPLES & (AUDIO_SW_FIFO_SAMPLES - 1u)) == 0u)) ? 1 : -1];

typedef char audio_sw_fifo_high_watermark_must_fit[
    ((AUDIO_SW_FIFO_HIGH_WATERMARK_SAMPLES > 0u) &&
     (AUDIO_SW_FIFO_HIGH_WATERMARK_SAMPLES <= AUDIO_SW_FIFO_SAMPLES)) ? 1 : -1];

typedef char audio_sw_fifo_low_watermark_must_fit[
    ((AUDIO_SW_FIFO_LOW_WATERMARK_SAMPLES > 0u) &&
     (AUDIO_SW_FIFO_LOW_WATERMARK_SAMPLES <= AUDIO_SW_FIFO_HIGH_WATERMARK_SAMPLES)) ? 1 : -1];

typedef char audio_producer_chunk_must_fit[
    ((AUDIO_PRODUCER_CHUNK_SAMPLES > 0u) &&
     (AUDIO_PRODUCER_CHUNK_SAMPLES <= AUDIO_SW_FIFO_SAMPLES)) ? 1 : -1];

/* -------------------------------------------------------------------------- */
/*  공개 API                                                                   */
/* -------------------------------------------------------------------------- */

/* DAC/TIM/DMA transport와 내부 상태를 초기화한다. */
void Audio_Driver_Init(void);

/* main loop에서 반복 호출되는 주기 처리 함수.
 * - software FIFO refill
 * - sequence tick -> sample 변환
 * - WAV stream read / decode / interpolation
 * 를 모두 여기서 수행한다.
 *
 * 권장 호출 위치:
 * - while(1) 루프의 가능한 앞쪽
 * - display redraw나 대용량 blocking 작업보다 먼저 */
void Audio_Driver_Task(uint32_t now_ms);

/* 현재 재생 중인 content를 정지하고 silence로 되돌린다. */
void Audio_Driver_Stop(void);

/* 현재 실제로 tone/sequence/WAV/FIFO tail 중 무엇이든 남아 있는가를 반환한다. */
bool Audio_Driver_IsBusy(void);

/* APP_STATE.audio snapshot 복사 helper */
void Audio_Driver_CopySnapshot(app_audio_state_t *out_snapshot);

/* -------------------------------------------------------------------------- */
/*  software volume API                                                        */
/*                                                                            */
/*  상위 레이어는 "0~100" 만 넘기면 되고,                                       */
/*  실제 perceptual curve 적용은 driver가 내부에서 맡는다.                     */
/* -------------------------------------------------------------------------- */
void Audio_Driver_SetVolumePercent(uint8_t volume_percent);
uint8_t Audio_Driver_GetVolumePercent(void);

/* -------------------------------------------------------------------------- */
/*  단순파 테스트 API                                                          */
/* -------------------------------------------------------------------------- */
HAL_StatusTypeDef Audio_Driver_PlaySineWave(uint32_t freq_hz,
                                            uint32_t time_ms);
HAL_StatusTypeDef Audio_Driver_PlaySquareWaveMs(uint32_t freq_hz,
                                                uint32_t time_ms);
HAL_StatusTypeDef Audio_Driver_PlaySawToothWaveMs(uint32_t freq_hz,
                                                  uint32_t time_ms);

/* square / saw는 자주 눌러 보는 quick test 용도로 default duration wrapper를 둔다. */
#define Audio_Driver_PlaySquareWave(freq_hz) \
    Audio_Driver_PlaySquareWaveMs((freq_hz), AUDIO_DEFAULT_TEST_TONE_MS)

#define Audio_Driver_PlaySawToothWave(freq_hz) \
    Audio_Driver_PlaySawToothWaveMs((freq_hz), AUDIO_DEFAULT_TEST_TONE_MS)

/* -------------------------------------------------------------------------- */
/*  preset sequence API                                                        */
/* -------------------------------------------------------------------------- */

/* 4채널 timbre stack + 4채널 note preset을 사용한 polyphonic playback */
HAL_StatusTypeDef Audio_Driver_PlaySequencePreset(audio_timbre_stack_preset_id_t timbre_stack_id,
                                                  audio_note_preset_id_t note_preset_id);

/* melody track만 단일 timbre로 재생하는 비교 청취용 API */
HAL_StatusTypeDef Audio_Driver_PlaySingleTrackPreset(audio_timbre_preset_id_t timbre_id,
                                                     audio_note_preset_id_t note_preset_id);

/* -------------------------------------------------------------------------- */
/*  WAV file playback API                                                      */
/*                                                                            */
/*  지원 범위                                                                  */
/*  - PCM integer only                                                         */
/*  - mono/stereo                                                              */
/*  - 8/16/24/32bit                                                            */
/* -------------------------------------------------------------------------- */
HAL_StatusTypeDef Audio_Driver_PlayWaveFilePath(const char *path);

/* SD root에서 아무 WAV 파일 하나를 찾아 바로 재생한다. */
HAL_StatusTypeDef Audio_Driver_PlayAnyWaveFromSd(void);

/* -------------------------------------------------------------------------- */
/*  HAL callback 진입점                                                        */
/*                                                                            */
/*  CubeMX가 DAC1 CH1 DMA를 생성하면                                          */
/*  HAL_DAC_ConvHalfCpltCallbackCh1 / HAL_DAC_ConvCpltCallbackCh1              */
/*  callback hook을 통해 여기에 들어오게 된다.                                 */
/*                                                                            */
/*  주의                                                                      */
/*  - 여기서는 software FIFO -> DMA half buffer memcpy만 수행한다.             */
/*  - FATFS read, wav decode, timbre 합성은 절대 여기서 하지 않는다.           */
/* -------------------------------------------------------------------------- */
void HAL_DAC_ConvHalfCpltCallbackCh1(DAC_HandleTypeDef *hdac_handle);
void HAL_DAC_ConvCpltCallbackCh1(DAC_HandleTypeDef *hdac_handle);
void HAL_DAC_DMAUnderrunCallbackCh1(DAC_HandleTypeDef *hdac_handle);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_DRIVER_H */
