#ifndef AUDIO_PRESETS_H
#define AUDIO_PRESETS_H

#include "main.h"
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  Audio_Presets                                                              */
/*                                                                            */
/*  목적                                                                      */
/*  - 음계(note), 박자 길이(tick), timbre preset, note preset을                */
/*    한 파일 계열로 분리해서 오디오 드라이버와 앱 레이어가                   */
/*    "악보 데이터" 를 공유하게 만든다.                                       */
/*  - 사용자는 앞으로 이 파일만 열어서                                         */
/*      1) BPM                                                                 */
/*      2) 4분음표/8분음표/16분음표 길이                                       */
/*      3) A0~C8 note define                                                   */
/*      4) timbre / score preset                                               */
/*    을 수정하며 사운드 캐릭터를 다듬을 수 있다.                              */
/*                                                                            */
/*  중요한 설계 원칙                                                           */
/*  - ch1 : STRING                                                             */
/*  - ch2 : BRASS                                                              */
/*  - ch3 : PIANO                                                              */
/*  - ch4 : PERCUSSION / BEAT                                                  */
/*                                                                            */
/*  즉, 4채널 합성 preset은 위 채널 역할을 전제로 정의한다.                   */
/*  단일 채널 비교 청취용(monophonic preview)은 보통 ch3 PIANO를 사용한다.    */
/* -------------------------------------------------------------------------- */

#ifndef AUDIO_PRESET_MAX_TRACKS
#define AUDIO_PRESET_MAX_TRACKS 4u
#endif

/* -------------------------------------------------------------------------- */
/*  시퀀서 tick 해상도                                                         */
/*                                                                            */
/*  PPQN(Pulses Per Quarter Note)을 96으로 두면                               */
/*    - 4분음표   = 96 tick                                                    */
/*    - 8분음표   = 48 tick                                                    */
/*    - 16분음표  = 24 tick                                                    */
/*  처럼 정수 계산이 매우 편하다.                                              */
/*                                                                            */
/*  앞으로 BPM만 바꾸고, 길이는 tick macro로 적으면                           */
/*  ms 직접 계산 없이 악보를 유지보수할 수 있다.                               */
/* -------------------------------------------------------------------------- */
#ifndef AUDIO_PPQN
#define AUDIO_PPQN 96u
#endif

#define AUDIO_TICKS_WHOLE              (AUDIO_PPQN * 4u)
#define AUDIO_TICKS_HALF               (AUDIO_PPQN * 2u)
#define AUDIO_TICKS_QUARTER            (AUDIO_PPQN)
#define AUDIO_TICKS_EIGHTH             (AUDIO_PPQN / 2u)
#define AUDIO_TICKS_SIXTEENTH          (AUDIO_PPQN / 4u)
#define AUDIO_TICKS_THIRTY_SECOND      (AUDIO_PPQN / 8u)
#define AUDIO_TICKS_DOTTED_HALF        (AUDIO_TICKS_HALF + AUDIO_TICKS_QUARTER)
#define AUDIO_TICKS_DOTTED_QUARTER     (AUDIO_TICKS_QUARTER + AUDIO_TICKS_EIGHTH)
#define AUDIO_TICKS_DOTTED_EIGHTH      (AUDIO_TICKS_EIGHTH + AUDIO_TICKS_SIXTEENTH)
#define AUDIO_TICKS_QUARTER_TRIPLET    (AUDIO_TICKS_HALF / 3u)
#define AUDIO_TICKS_EIGHTH_TRIPLET     (AUDIO_TICKS_QUARTER / 3u)

/* -------------------------------------------------------------------------- */
/*  note frequency define                                                     */
/*                                                                            */
/*  단위는 Hz x 100 이다.                                                     */
/*                                                                            */
/*  예시                                                                      */
/*    AUDIO_NOTE_A4_HZ_X100  == 44000                                         */
/*    AUDIO_NOTE_C5_HZ_X100  == 52325                                         */
/*                                                                            */
/*  note 이벤트는 이 define을 직접 써서                                       */
/*      AUDIO_NOTE_EVENT(AUDIO_NOTE_C5_HZ_X100, AUDIO_TICKS_EIGHTH)           */
/*  처럼 적으면 된다.                                                          */
/* -------------------------------------------------------------------------- */
#define AUDIO_NOTE_REST_HZ_X100 0u

/* octave 0 */
#define AUDIO_NOTE_A0_HZ_X100 2750u
#define AUDIO_NOTE_AS0_HZ_X100 2914u
#define AUDIO_NOTE_B0_HZ_X100 3087u

/* octave 1 */
#define AUDIO_NOTE_C1_HZ_X100 3270u
#define AUDIO_NOTE_CS1_HZ_X100 3465u
#define AUDIO_NOTE_D1_HZ_X100 3671u
#define AUDIO_NOTE_DS1_HZ_X100 3889u
#define AUDIO_NOTE_E1_HZ_X100 4120u
#define AUDIO_NOTE_F1_HZ_X100 4365u
#define AUDIO_NOTE_FS1_HZ_X100 4625u
#define AUDIO_NOTE_G1_HZ_X100 4900u
#define AUDIO_NOTE_GS1_HZ_X100 5191u
#define AUDIO_NOTE_A1_HZ_X100 5500u
#define AUDIO_NOTE_AS1_HZ_X100 5827u
#define AUDIO_NOTE_B1_HZ_X100 6174u

/* octave 2 */
#define AUDIO_NOTE_C2_HZ_X100 6541u
#define AUDIO_NOTE_CS2_HZ_X100 6930u
#define AUDIO_NOTE_D2_HZ_X100 7342u
#define AUDIO_NOTE_DS2_HZ_X100 7778u
#define AUDIO_NOTE_E2_HZ_X100 8241u
#define AUDIO_NOTE_F2_HZ_X100 8731u
#define AUDIO_NOTE_FS2_HZ_X100 9250u
#define AUDIO_NOTE_G2_HZ_X100 9800u
#define AUDIO_NOTE_GS2_HZ_X100 10383u
#define AUDIO_NOTE_A2_HZ_X100 11000u
#define AUDIO_NOTE_AS2_HZ_X100 11654u
#define AUDIO_NOTE_B2_HZ_X100 12347u

/* octave 3 */
#define AUDIO_NOTE_C3_HZ_X100 13081u
#define AUDIO_NOTE_CS3_HZ_X100 13859u
#define AUDIO_NOTE_D3_HZ_X100 14683u
#define AUDIO_NOTE_DS3_HZ_X100 15556u
#define AUDIO_NOTE_E3_HZ_X100 16481u
#define AUDIO_NOTE_F3_HZ_X100 17461u
#define AUDIO_NOTE_FS3_HZ_X100 18500u
#define AUDIO_NOTE_G3_HZ_X100 19600u
#define AUDIO_NOTE_GS3_HZ_X100 20765u
#define AUDIO_NOTE_A3_HZ_X100 22000u
#define AUDIO_NOTE_AS3_HZ_X100 23308u
#define AUDIO_NOTE_B3_HZ_X100 24694u

/* octave 4 */
#define AUDIO_NOTE_C4_HZ_X100 26163u
#define AUDIO_NOTE_CS4_HZ_X100 27718u
#define AUDIO_NOTE_D4_HZ_X100 29366u
#define AUDIO_NOTE_DS4_HZ_X100 31113u
#define AUDIO_NOTE_E4_HZ_X100 32963u
#define AUDIO_NOTE_F4_HZ_X100 34923u
#define AUDIO_NOTE_FS4_HZ_X100 36999u
#define AUDIO_NOTE_G4_HZ_X100 39200u
#define AUDIO_NOTE_GS4_HZ_X100 41530u
#define AUDIO_NOTE_A4_HZ_X100 44000u
#define AUDIO_NOTE_AS4_HZ_X100 46616u
#define AUDIO_NOTE_B4_HZ_X100 49388u

/* octave 5 */
#define AUDIO_NOTE_C5_HZ_X100 52325u
#define AUDIO_NOTE_CS5_HZ_X100 55437u
#define AUDIO_NOTE_D5_HZ_X100 58733u
#define AUDIO_NOTE_DS5_HZ_X100 62225u
#define AUDIO_NOTE_E5_HZ_X100 65926u
#define AUDIO_NOTE_F5_HZ_X100 69846u
#define AUDIO_NOTE_FS5_HZ_X100 73999u
#define AUDIO_NOTE_G5_HZ_X100 78399u
#define AUDIO_NOTE_GS5_HZ_X100 83061u
#define AUDIO_NOTE_A5_HZ_X100 88000u
#define AUDIO_NOTE_AS5_HZ_X100 93233u
#define AUDIO_NOTE_B5_HZ_X100 98777u

/* octave 6 */
#define AUDIO_NOTE_C6_HZ_X100 104650u
#define AUDIO_NOTE_CS6_HZ_X100 110873u
#define AUDIO_NOTE_D6_HZ_X100 117466u
#define AUDIO_NOTE_DS6_HZ_X100 124451u
#define AUDIO_NOTE_E6_HZ_X100 131851u
#define AUDIO_NOTE_F6_HZ_X100 139691u
#define AUDIO_NOTE_FS6_HZ_X100 147998u
#define AUDIO_NOTE_G6_HZ_X100 156798u
#define AUDIO_NOTE_GS6_HZ_X100 166122u
#define AUDIO_NOTE_A6_HZ_X100 176000u
#define AUDIO_NOTE_AS6_HZ_X100 186466u
#define AUDIO_NOTE_B6_HZ_X100 197553u

/* octave 7 */
#define AUDIO_NOTE_C7_HZ_X100 209300u
#define AUDIO_NOTE_CS7_HZ_X100 221746u
#define AUDIO_NOTE_D7_HZ_X100 234932u
#define AUDIO_NOTE_DS7_HZ_X100 248902u
#define AUDIO_NOTE_E7_HZ_X100 263702u
#define AUDIO_NOTE_F7_HZ_X100 279383u
#define AUDIO_NOTE_FS7_HZ_X100 295996u
#define AUDIO_NOTE_G7_HZ_X100 313596u
#define AUDIO_NOTE_GS7_HZ_X100 332244u
#define AUDIO_NOTE_A7_HZ_X100 352000u
#define AUDIO_NOTE_AS7_HZ_X100 372931u
#define AUDIO_NOTE_B7_HZ_X100 395107u

/* octave 8 */
#define AUDIO_NOTE_C8_HZ_X100 418601u

/* -------------------------------------------------------------------------- */
/*  waveform / timbre / preset ID                                             */
/* -------------------------------------------------------------------------- */
typedef enum
{
    AUDIO_WAVEFORM_NONE       = 0u,
    AUDIO_WAVEFORM_SINE       = 1u,
    AUDIO_WAVEFORM_SQUARE     = 2u,
    AUDIO_WAVEFORM_SAW        = 3u,

    AUDIO_WAVEFORM_STRING     = 10u,
    AUDIO_WAVEFORM_BRASS      = 11u,
    AUDIO_WAVEFORM_PIANO      = 12u,
    AUDIO_WAVEFORM_PERCUSSION = 13u
} audio_waveform_t;

typedef enum
{
    AUDIO_TIMBRE_PURE_SINE       = 0u,
    AUDIO_TIMBRE_PURE_SQUARE     = 1u,
    AUDIO_TIMBRE_PURE_SAW        = 2u,

    AUDIO_TIMBRE_CH1_STRING      = 10u,
    AUDIO_TIMBRE_CH2_BRASS       = 11u,
    AUDIO_TIMBRE_CH3_PIANO       = 12u,
    AUDIO_TIMBRE_CH4_PERCUSSION  = 13u
} audio_timbre_preset_id_t;

typedef enum
{
    AUDIO_TIMBRE_STACK_STANDARD_4CH = 0u
} audio_timbre_stack_preset_id_t;

typedef enum
{
    AUDIO_NOTE_PRESET_BOOT              = 0u,
    AUDIO_NOTE_PRESET_POWER_OFF         = 1u,
    AUDIO_NOTE_PRESET_WARNING           = 2u,
    AUDIO_NOTE_PRESET_ERROR             = 3u,
    AUDIO_NOTE_PRESET_FATAL             = 4u,
    AUDIO_NOTE_PRESET_VARIO_PLACEHOLDER = 5u,
    AUDIO_NOTE_PRESET_BUTTON_SHORT      = 6u,
    AUDIO_NOTE_PRESET_BUTTON_LONG       = 7u,
    AUDIO_NOTE_PRESET_INFORMATION       = 8u,
    AUDIO_NOTE_PRESET_HOURLY_CHIME      = 9u
} audio_note_preset_id_t;

/* -------------------------------------------------------------------------- */
/*  timbre render callback                                                    */
/*                                                                            */
/*  phase_q32는 0.0~1.0 주기를 32bit unsigned phase로 표현한 값이다.          */
/*  phase_inc_q32는 "샘플 1개가 지날 때 phase가 얼마나 전진하는가" 이다.       */
/*                                                                            */
/*  이 함수는 envelope 이전의 "원재료 파형" 을 signed 16bit 범위로 반환한다.   */
/* -------------------------------------------------------------------------- */
typedef int16_t (*audio_timbre_sample_fn_t)(uint32_t phase_q32,
                                            uint32_t phase_inc_q32);

/* -------------------------------------------------------------------------- */
/*  timbre preset                                                             */
/*                                                                            */
/*  attack/decay/release는 note envelope 시간을 ms로 적는다.                  */
/*  sustain_level_q15는 0~32767 범위의 sustain 비율이다.                      */
/*  gain_permille은 timbre 자체 loudness 정규화에 사용한다.                   */
/* -------------------------------------------------------------------------- */
typedef struct
{
    const char *name;
    audio_waveform_t ui_waveform;
    audio_timbre_sample_fn_t render_fn;
    uint16_t attack_ms;
    uint16_t decay_ms;
    uint16_t sustain_level_q15;
    uint16_t release_ms;
    uint16_t gain_permille;
} audio_timbre_preset_t;

/* -------------------------------------------------------------------------- */
/*  4채널 timbre stack preset                                                 */
/*                                                                            */
/*  channel_timbres[0] = ch1 STRING                                           */
/*  channel_timbres[1] = ch2 BRASS                                            */
/*  channel_timbres[2] = ch3 PIANO                                            */
/*  channel_timbres[3] = ch4 PERCUSSION                                       */
/* -------------------------------------------------------------------------- */
typedef struct
{
    const char *name;
    audio_timbre_preset_id_t channel_timbres[AUDIO_PRESET_MAX_TRACKS];
} audio_timbre_stack_preset_t;

/* -------------------------------------------------------------------------- */
/*  note event                                                                */
/*                                                                            */
/*  note_hz_x100      : 0이면 rest                                             */
/*  duration_ticks    : note 전체 길이                                         */
/*  gate_ticks        : 0이면 드라이버 기본 gate 비율(대개 85%) 사용            */
/*  velocity_permille : 0~1000 강도                                            */
/* -------------------------------------------------------------------------- */
typedef struct
{
    uint32_t note_hz_x100;
    uint16_t duration_ticks;
    uint16_t gate_ticks;
    uint16_t velocity_permille;
} audio_note_event_t;

#define AUDIO_NOTE_EVENT(note_hz_x100, duration_ticks) \
    { (note_hz_x100), (duration_ticks), 0u, 1000u }

#define AUDIO_NOTE_EVENT_V(note_hz_x100, duration_ticks, velocity_permille) \
    { (note_hz_x100), (duration_ticks), 0u, (velocity_permille) }

#define AUDIO_NOTE_EVENT_G(note_hz_x100, duration_ticks, gate_ticks, velocity_permille) \
    { (note_hz_x100), (duration_ticks), (gate_ticks), (velocity_permille) }

#define AUDIO_REST_EVENT(duration_ticks) \
    { AUDIO_NOTE_REST_HZ_X100, (duration_ticks), 0u, 0u }

/* -------------------------------------------------------------------------- */
/*  note preset                                                               */
/*                                                                            */
/*  track 0 = ch1 STRING                                                      */
/*  track 1 = ch2 BRASS                                                       */
/*  track 2 = ch3 PIANO (보통 melody track)                                   */
/*  track 3 = ch4 PERCUSSION                                                  */
/*                                                                            */
/*  melody_track_index는                                                      */
/*  "이 preset을 단일 채널 비교 청취용으로 재생할 때"                        */
/*  어느 트랙을 뽑아서 쓸 것인가를 지정한다.                                  */
/* -------------------------------------------------------------------------- */
typedef struct
{
    const char *name;
    uint16_t bpm;
    uint8_t  track_count;
    uint8_t  melody_track_index;
    const audio_note_event_t *tracks[AUDIO_PRESET_MAX_TRACKS];
    uint16_t track_lengths[AUDIO_PRESET_MAX_TRACKS];
} audio_note_preset_t;

/* -------------------------------------------------------------------------- */
/*  lookup API                                                                */
/* -------------------------------------------------------------------------- */
const audio_timbre_preset_t *Audio_Presets_GetTimbre(audio_timbre_preset_id_t timbre_id);
const audio_timbre_stack_preset_t *Audio_Presets_GetTimbreStack(audio_timbre_stack_preset_id_t stack_id);
const audio_note_preset_t *Audio_Presets_GetNotePreset(audio_note_preset_id_t note_preset_id);
const char *Audio_Presets_GetTimbreName(audio_timbre_preset_id_t timbre_id);
const char *Audio_Presets_GetNotePresetName(audio_note_preset_id_t note_preset_id);

#ifdef __cplusplus
}
#endif

#endif /* AUDIO_PRESETS_H */