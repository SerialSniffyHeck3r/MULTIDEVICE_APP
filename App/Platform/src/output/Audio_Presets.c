#include "Audio_Presets.h"

#include <math.h>
#include <string.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

#ifndef ARRAY_SIZE
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))
#endif

/* -------------------------------------------------------------------------- */
/*  내부 sine lookup table                                                     */
/*                                                                            */
/*  timbre 함수는 sample마다 반복 호출되므로                                   */
/*  sinf()를 실시간으로 계속 부르는 대신                                      */
/*  부팅 후 1회만 256-entry LUT를 만든 뒤                                     */
/*  상위 8bit index로 빠르게 읽는다.                                           */
/* -------------------------------------------------------------------------- */
static int16_t s_audio_preset_sine_lut[256];
static uint8_t s_audio_preset_sine_lut_ready = 0u;

/* -------------------------------------------------------------------------- */
/*  내부 유틸: sine LUT lazy init                                              */
/*                                                                            */
/*  아직 table이 준비되지 않았을 때만 1회 생성한다.                            */
/*  이후 timbre render 경로에서는 정수 lookup만 수행한다.                     */
/* -------------------------------------------------------------------------- */
static void Audio_Presets_EnsureSineLutReady(void)
{
    uint32_t index;

    if (s_audio_preset_sine_lut_ready != 0u)
    {
        return;
    }

    for (index = 0u; index < ARRAY_SIZE(s_audio_preset_sine_lut); index++)
    {
        double angle_rad;
        double sample_value;

        angle_rad = (2.0 * M_PI * (double)index) / (double)ARRAY_SIZE(s_audio_preset_sine_lut);
        sample_value = sin(angle_rad) * 32767.0;

        s_audio_preset_sine_lut[index] = (int16_t)sample_value;
    }

    s_audio_preset_sine_lut_ready = 1u;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: phase -> sine                                                   */
/* -------------------------------------------------------------------------- */
static int16_t Audio_Presets_SineFromPhaseQ32(uint32_t phase_q32)
{
    Audio_Presets_EnsureSineLutReady();
    return s_audio_preset_sine_lut[(phase_q32 >> 24) & 0xFFu];
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: phase -> square                                                 */
/* -------------------------------------------------------------------------- */
static int16_t Audio_Presets_SquareFromPhaseQ32(uint32_t phase_q32)
{
    return (phase_q32 < 0x80000000u) ? 32767 : (int16_t)-32768;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: phase -> saw                                                    */
/* -------------------------------------------------------------------------- */
static int16_t Audio_Presets_SawFromPhaseQ32(uint32_t phase_q32)
{
    return (int16_t)((int32_t)(phase_q32 >> 16) - 32768);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: phase -> triangle                                               */
/* -------------------------------------------------------------------------- */
static int16_t Audio_Presets_TriangleFromPhaseQ32(uint32_t phase_q32)
{
    uint32_t phase_u16;
    int32_t value;

    phase_u16 = (phase_q32 >> 16);

    if (phase_u16 < 32768u)
    {
        value = (int32_t)phase_u16 * 2;
    }
    else
    {
        value = (int32_t)(65535u - phase_u16) * 2;
    }

    return (int16_t)(value - 32768);
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: phase -> pseudo noise                                           */
/*                                                                            */
/*  퍼커션용 timbre는 정확한 샘플러보다                                         */
/*  짧은 click/noise 성격이 더 중요하므로                                      */
/*  phase 기반 xorshift 비슷한 간단한 noise를 사용한다.                        */
/* -------------------------------------------------------------------------- */
static int16_t Audio_Presets_NoiseFromPhaseQ32(uint32_t phase_q32)
{
    uint32_t x;

    x = phase_q32 ^ 0xA5A55A5Au;
    x ^= (x << 13);
    x ^= (x >> 17);
    x ^= (x << 5);

    return (int16_t)((int32_t)(x >> 16) - 32768);
}

/* -------------------------------------------------------------------------- */
/*  timbre render 함수들                                                       */
/*                                                                            */
/*  ch1 STRING                                                                 */
/*  - 완만한 attack와 중간 sustain을 전제로                                   */
/*  - sine + 약한 2배/3배 성분을 섞어                                          */
/*    얇지만 pad 역할을 하게 만든다.                                           */
/* -------------------------------------------------------------------------- */
static int16_t Audio_Presets_RenderString(uint32_t phase_q32,
                                          uint32_t phase_inc_q32)
{
    int32_t fundamental;
    int32_t second_harmonic;
    int32_t third_harmonic;
    int32_t mixed;

    (void)phase_inc_q32;

    fundamental     = (int32_t)Audio_Presets_SineFromPhaseQ32(phase_q32);
    second_harmonic = (int32_t)Audio_Presets_SineFromPhaseQ32(phase_q32 * 2u);
    third_harmonic  = (int32_t)Audio_Presets_TriangleFromPhaseQ32(phase_q32 * 3u);

    mixed = (fundamental * 8) + (second_harmonic * 3) + (third_harmonic * 2);
    mixed /= 13;

    return (int16_t)mixed;
}

/* -------------------------------------------------------------------------- */
/*  ch2 BRASS                                                                  */
/*  - saw 성분을 중심으로                                                       */
/*  - 2배/3배 고조파를 조금 더 강하게 넣어                                     */
/*    기기 경고음/팡파르 계열에서 존재감이 나도록 구성한다.                    */
/* -------------------------------------------------------------------------- */
static int16_t Audio_Presets_RenderBrass(uint32_t phase_q32,
                                         uint32_t phase_inc_q32)
{
    int32_t saw_base;
    int32_t fundamental;
    int32_t second_harmonic;
    int32_t third_harmonic;
    int32_t mixed;

    (void)phase_inc_q32;

    saw_base        = (int32_t)Audio_Presets_SawFromPhaseQ32(phase_q32);
    fundamental     = (int32_t)Audio_Presets_SineFromPhaseQ32(phase_q32);
    second_harmonic = (int32_t)Audio_Presets_SineFromPhaseQ32(phase_q32 * 2u);
    third_harmonic  = (int32_t)Audio_Presets_SineFromPhaseQ32(phase_q32 * 3u);

    mixed = (saw_base * 6) + (fundamental * 4) + (second_harmonic * 3) + (third_harmonic * 2);
    mixed /= 15;

    return (int16_t)mixed;
}

/* -------------------------------------------------------------------------- */
/*  ch3 PIANO                                                                  */
/*  - melody preview용 기본 음색                                                */
/*  - fundamental을 중심으로 하되                                               */
/*    triangle / 2배 / 4배 성분을 살짝 섞어                                    */
/*    선율이 기기 스피커에서 잘 들리게 만든다.                                 */
/* -------------------------------------------------------------------------- */
static int16_t Audio_Presets_RenderPiano(uint32_t phase_q32,
                                         uint32_t phase_inc_q32)
{
    int32_t fundamental;
    int32_t triangle_color;
    int32_t second_harmonic;
    int32_t fourth_harmonic;
    int32_t mixed;

    (void)phase_inc_q32;

    fundamental      = (int32_t)Audio_Presets_SineFromPhaseQ32(phase_q32);
    triangle_color   = (int32_t)Audio_Presets_TriangleFromPhaseQ32(phase_q32);
    second_harmonic  = (int32_t)Audio_Presets_SineFromPhaseQ32(phase_q32 * 2u);
    fourth_harmonic  = (int32_t)Audio_Presets_SineFromPhaseQ32(phase_q32 * 4u);

    mixed = (fundamental * 8) + (triangle_color * 2) + (second_harmonic * 2) + fourth_harmonic;
    mixed /= 13;

    return (int16_t)mixed;
}

/* -------------------------------------------------------------------------- */
/*  ch4 PERCUSSION                                                             */
/*  - pitch가 중심이 아니라 transient가 중심이다.                              */
/*  - noise + 아주 빠른 square + 약한 tone을 섞어서                             */
/*    click, beat, short alarm pulse 용도로 사용한다.                          */
/* -------------------------------------------------------------------------- */
static int16_t Audio_Presets_RenderPercussion(uint32_t phase_q32,
                                              uint32_t phase_inc_q32)
{
    int32_t noise;
    int32_t click;
    int32_t tone;
    int32_t mixed;

    noise = (int32_t)Audio_Presets_NoiseFromPhaseQ32(phase_q32 + phase_inc_q32);
    click = (int32_t)Audio_Presets_SquareFromPhaseQ32(phase_q32 << 3);
    tone  = (int32_t)Audio_Presets_SineFromPhaseQ32(phase_q32 * 5u);

    mixed = (noise * 8) + (click * 3) + tone;
    mixed /= 12;

    return (int16_t)mixed;
}

/* -------------------------------------------------------------------------- */
/*  pure waveform render 함수들                                                */
/*                                                                            */
/*  단순파 테스트용 API는 이 pure timbre들을 사용한다.                          */
/* -------------------------------------------------------------------------- */
static int16_t Audio_Presets_RenderPureSine(uint32_t phase_q32,
                                            uint32_t phase_inc_q32)
{
    (void)phase_inc_q32;
    return Audio_Presets_SineFromPhaseQ32(phase_q32);
}

static int16_t Audio_Presets_RenderPureSquare(uint32_t phase_q32,
                                              uint32_t phase_inc_q32)
{
    (void)phase_inc_q32;
    return Audio_Presets_SquareFromPhaseQ32(phase_q32);
}

static int16_t Audio_Presets_RenderPureSaw(uint32_t phase_q32,
                                           uint32_t phase_inc_q32)
{
    (void)phase_inc_q32;
    return Audio_Presets_SawFromPhaseQ32(phase_q32);
}

/* -------------------------------------------------------------------------- */
/*  timbre preset 인스턴스                                                     */
/* -------------------------------------------------------------------------- */
static const audio_timbre_preset_t s_timbre_pure_sine =
{
    "PURE_SINE",
    AUDIO_WAVEFORM_SINE,
    Audio_Presets_RenderPureSine,
    2u,
    10u,
    24576u,
    30u,
    900u
};

static const audio_timbre_preset_t s_timbre_pure_square =
{
    "PURE_SQUARE",
    AUDIO_WAVEFORM_SQUARE,
    Audio_Presets_RenderPureSquare,
    1u,
    8u,
    22937u,
    20u,
    800u
};

static const audio_timbre_preset_t s_timbre_pure_saw =
{
    "PURE_SAW",
    AUDIO_WAVEFORM_SAW,
    Audio_Presets_RenderPureSaw,
    1u,
    8u,
    21299u,
    20u,
    780u
};

static const audio_timbre_preset_t s_timbre_ch1_string =
{
    "CH1_STRING",
    AUDIO_WAVEFORM_STRING,
    Audio_Presets_RenderString,
    35u,
    70u,
    24576u,
    90u,
    820u
};

static const audio_timbre_preset_t s_timbre_ch2_brass =
{
    "CH2_BRASS",
    AUDIO_WAVEFORM_BRASS,
    Audio_Presets_RenderBrass,
    5u,
    60u,
    19660u,
    45u,
    920u
};

static const audio_timbre_preset_t s_timbre_ch3_piano =
{
    "CH3_PIANO",
    AUDIO_WAVEFORM_PIANO,
    Audio_Presets_RenderPiano,
    2u,
    120u,
    8192u,
    70u,
    880u
};

static const audio_timbre_preset_t s_timbre_ch4_percussion =
{
    "CH4_PERCUSSION",
    AUDIO_WAVEFORM_PERCUSSION,
    Audio_Presets_RenderPercussion,
    0u,
    24u,
    0u,
    16u,
    1000u
};

/* -------------------------------------------------------------------------- */
/*  표준 4채널 stack                                                           */
/*                                                                            */
/*  이 preset은 사용자의 현재 요청을 그대로 반영한다.                          */
/*    - ch1 = string                                                           */
/*    - ch2 = brass                                                            */
/*    - ch3 = piano                                                            */
/*    - ch4 = percussion                                                       */
/* -------------------------------------------------------------------------- */
static const audio_timbre_stack_preset_t s_timbre_stack_standard_4ch =
{
    "STANDARD_4CH",
    {
        AUDIO_TIMBRE_CH1_STRING,
        AUDIO_TIMBRE_CH2_BRASS,
        AUDIO_TIMBRE_CH3_PIANO,
        AUDIO_TIMBRE_CH4_PERCUSSION
    }
};

/* -------------------------------------------------------------------------- */
/*  preset 악보 데이터                                                         */
/*                                                                            */
/*  표기 관례                                                                  */
/*  - ch1 STRING : sustain / pad / 배경 화성                                   */
/*  - ch2 BRASS  : accent / stab / fanfare                                     */
/*  - ch3 PIANO  : melody                                                      */
/*  - ch4 PERC   : beat / click / rhythm                                       */
/*                                                                            */
/*  각 효과음은                                                                */
/*    1) 같은 선율을 ch3 기준으로 mono preview 가능해야 하고                   */
/*    2) 4채널 합성 버전과 비교 청취할 수 있어야 하므로                        */
/*  melody는 가능한 한 ch3에 몰아 둔다.                                        */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/*  BOOT                                                                       */
/*  - 상승 아르페지오                                                           */
/*  - 기기가 켜질 때 "준비됨" 느낌을 주는 major 성격                          */
/* -------------------------------------------------------------------------- */
static const audio_note_event_t s_boot_ch1_string[] =
{
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_C4_HZ_X100, AUDIO_TICKS_HALF, 550u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_G4_HZ_X100, AUDIO_TICKS_QUARTER, 500u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_C5_HZ_X100, AUDIO_TICKS_QUARTER, 500u)
};

static const audio_note_event_t s_boot_ch2_brass[] =
{
    AUDIO_REST_EVENT(AUDIO_TICKS_EIGHTH),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_C5_HZ_X100, AUDIO_TICKS_EIGHTH, 760u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_G5_HZ_X100, AUDIO_TICKS_EIGHTH, 720u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_C6_HZ_X100, AUDIO_TICKS_QUARTER, 780u)
};

static const audio_note_event_t s_boot_ch3_piano[] =
{
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_C5_HZ_X100, AUDIO_TICKS_EIGHTH, 920u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_E5_HZ_X100, AUDIO_TICKS_EIGHTH, 920u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_G5_HZ_X100, AUDIO_TICKS_EIGHTH, 920u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_C6_HZ_X100, AUDIO_TICKS_DOTTED_QUARTER, 960u)
};

static const audio_note_event_t s_boot_ch4_percussion[] =
{
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_C2_HZ_X100, AUDIO_TICKS_QUARTER, AUDIO_TICKS_SIXTEENTH, 800u),
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_C2_HZ_X100, AUDIO_TICKS_QUARTER, AUDIO_TICKS_SIXTEENTH, 700u),
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_C2_HZ_X100, AUDIO_TICKS_QUARTER, AUDIO_TICKS_SIXTEENTH, 700u),
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_G2_HZ_X100, AUDIO_TICKS_QUARTER, AUDIO_TICKS_SIXTEENTH, 820u)
};

/* -------------------------------------------------------------------------- */
/*  POWER OFF                                                                  */
/*  - 하행 종지                                                                */
/*  - 전원 종료를 짧고 분명하게 알리되                                         */
/*    부팅음보다 더 낮고 차분하게 만든다.                                      */
/* -------------------------------------------------------------------------- */
static const audio_note_event_t s_power_off_ch1_string[] =
{
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_C5_HZ_X100, AUDIO_TICKS_QUARTER, 520u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_G4_HZ_X100, AUDIO_TICKS_QUARTER, 500u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_E4_HZ_X100, AUDIO_TICKS_QUARTER, 480u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_C4_HZ_X100, AUDIO_TICKS_QUARTER, 460u)
};

static const audio_note_event_t s_power_off_ch2_brass[] =
{
    AUDIO_REST_EVENT(AUDIO_TICKS_EIGHTH),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_C5_HZ_X100, AUDIO_TICKS_EIGHTH, 720u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_G4_HZ_X100, AUDIO_TICKS_QUARTER, 650u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_C4_HZ_X100, AUDIO_TICKS_QUARTER, 620u)
};

static const audio_note_event_t s_power_off_ch3_piano[] =
{
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_C6_HZ_X100, AUDIO_TICKS_EIGHTH, 900u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_G5_HZ_X100, AUDIO_TICKS_EIGHTH, 900u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_E5_HZ_X100, AUDIO_TICKS_QUARTER, 860u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_C5_HZ_X100, AUDIO_TICKS_DOTTED_QUARTER, 840u)
};

static const audio_note_event_t s_power_off_ch4_percussion[] =
{
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_C2_HZ_X100, AUDIO_TICKS_QUARTER, AUDIO_TICKS_SIXTEENTH, 700u),
    AUDIO_REST_EVENT(AUDIO_TICKS_QUARTER),
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_C2_HZ_X100, AUDIO_TICKS_QUARTER, AUDIO_TICKS_SIXTEENTH, 620u)
};

/* -------------------------------------------------------------------------- */
/*  WARNING                                                                    */
/*  - major/minor 2nd 충돌을 사용해                                             */
/*    "지금 확인해 달라" 는 긴장감을 만든다.                                   */
/* -------------------------------------------------------------------------- */
static const audio_note_event_t s_warning_ch1_string[] =
{
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_A4_HZ_X100, AUDIO_TICKS_HALF, 520u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_A4_HZ_X100, AUDIO_TICKS_HALF, 520u)
};

static const audio_note_event_t s_warning_ch2_brass[] =
{
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_A5_HZ_X100, AUDIO_TICKS_QUARTER, AUDIO_TICKS_EIGHTH, 820u),
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_AS5_HZ_X100, AUDIO_TICKS_QUARTER, AUDIO_TICKS_EIGHTH, 820u),
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_A5_HZ_X100, AUDIO_TICKS_QUARTER, AUDIO_TICKS_EIGHTH, 820u),
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_AS5_HZ_X100, AUDIO_TICKS_QUARTER, AUDIO_TICKS_EIGHTH, 820u)
};

static const audio_note_event_t s_warning_ch3_piano[] =
{
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_A5_HZ_X100, AUDIO_TICKS_EIGHTH, 930u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_AS5_HZ_X100, AUDIO_TICKS_EIGHTH, 930u),
    AUDIO_REST_EVENT(AUDIO_TICKS_EIGHTH),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_A5_HZ_X100, AUDIO_TICKS_EIGHTH, 930u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_AS5_HZ_X100, AUDIO_TICKS_QUARTER, 930u),
    AUDIO_REST_EVENT(AUDIO_TICKS_QUARTER)
};

static const audio_note_event_t s_warning_ch4_percussion[] =
{
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_C2_HZ_X100, AUDIO_TICKS_EIGHTH, AUDIO_TICKS_SIXTEENTH, 840u),
    AUDIO_REST_EVENT(AUDIO_TICKS_EIGHTH),
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_C2_HZ_X100, AUDIO_TICKS_EIGHTH, AUDIO_TICKS_SIXTEENTH, 840u),
    AUDIO_REST_EVENT(AUDIO_TICKS_EIGHTH),
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_C2_HZ_X100, AUDIO_TICKS_EIGHTH, AUDIO_TICKS_SIXTEENTH, 840u),
    AUDIO_REST_EVENT(AUDIO_TICKS_EIGHTH),
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_C2_HZ_X100, AUDIO_TICKS_EIGHTH, AUDIO_TICKS_SIXTEENTH, 840u),
    AUDIO_REST_EVENT(AUDIO_TICKS_EIGHTH)
};

/* -------------------------------------------------------------------------- */
/*  ERROR                                                                      */
/*  - 단순 warning보다 더 어둡고 불안정한 진행                                  */
/*  - 완전한 종지를 피해서 "실패" 느낌을 남긴다.                              */
/* -------------------------------------------------------------------------- */
static const audio_note_event_t s_error_ch1_string[] =
{
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_E4_HZ_X100, AUDIO_TICKS_HALF, 520u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_C4_HZ_X100, AUDIO_TICKS_HALF, 500u)
};

static const audio_note_event_t s_error_ch2_brass[] =
{
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_B4_HZ_X100, AUDIO_TICKS_QUARTER, AUDIO_TICKS_EIGHTH, 780u),
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_F4_HZ_X100, AUDIO_TICKS_QUARTER, AUDIO_TICKS_EIGHTH, 760u),
    AUDIO_REST_EVENT(AUDIO_TICKS_QUARTER),
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_B4_HZ_X100, AUDIO_TICKS_QUARTER, AUDIO_TICKS_EIGHTH, 760u)
};

static const audio_note_event_t s_error_ch3_piano[] =
{
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_E5_HZ_X100, AUDIO_TICKS_QUARTER, 900u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_C5_HZ_X100, AUDIO_TICKS_QUARTER, 900u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_FS4_HZ_X100, AUDIO_TICKS_QUARTER, 860u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_E4_HZ_X100, AUDIO_TICKS_QUARTER, 820u)
};

static const audio_note_event_t s_error_ch4_percussion[] =
{
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_C2_HZ_X100, AUDIO_TICKS_QUARTER, AUDIO_TICKS_SIXTEENTH, 760u),
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_C2_HZ_X100, AUDIO_TICKS_QUARTER, AUDIO_TICKS_SIXTEENTH, 720u),
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_C2_HZ_X100, AUDIO_TICKS_QUARTER, AUDIO_TICKS_SIXTEENTH, 760u),
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_C2_HZ_X100, AUDIO_TICKS_QUARTER, AUDIO_TICKS_SIXTEENTH, 720u)
};

/* -------------------------------------------------------------------------- */
/*  FATAL                                                                      */
/*  - 저역 중심 + tritone 성분                                                  */
/*  - 일반 error보다 더 느리고 무겁다.                                         */
/* -------------------------------------------------------------------------- */
static const audio_note_event_t s_fatal_ch1_string[] =
{
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_C3_HZ_X100, AUDIO_TICKS_WHOLE, 560u)
};

static const audio_note_event_t s_fatal_ch2_brass[] =
{
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_FS3_HZ_X100, AUDIO_TICKS_HALF, AUDIO_TICKS_QUARTER, 860u),
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_C3_HZ_X100, AUDIO_TICKS_HALF, AUDIO_TICKS_QUARTER, 860u)
};

static const audio_note_event_t s_fatal_ch3_piano[] =
{
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_C4_HZ_X100, AUDIO_TICKS_QUARTER, 940u),
    AUDIO_REST_EVENT(AUDIO_TICKS_EIGHTH),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_C4_HZ_X100, AUDIO_TICKS_QUARTER, 920u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_FS4_HZ_X100, AUDIO_TICKS_QUARTER, 940u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_C4_HZ_X100, AUDIO_TICKS_QUARTER, 900u)
};

static const audio_note_event_t s_fatal_ch4_percussion[] =
{
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_C2_HZ_X100, AUDIO_TICKS_QUARTER, AUDIO_TICKS_SIXTEENTH, 900u),
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_C2_HZ_X100, AUDIO_TICKS_QUARTER, AUDIO_TICKS_SIXTEENTH, 820u),
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_C2_HZ_X100, AUDIO_TICKS_QUARTER, AUDIO_TICKS_SIXTEENTH, 900u),
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_C2_HZ_X100, AUDIO_TICKS_QUARTER, AUDIO_TICKS_SIXTEENTH, 820u)
};

/* -------------------------------------------------------------------------- */
/*  VARIO PLACEHOLDER                                                          */
/*                                                                            */
/*  주의                                                                      */
/*  - 이 preset은 아직 "상승/하강율에 따라 지속적으로 주파수/간격이 변하는"     */
/*    실제 바리오 로직이 아니다.                                               */
/*  - 나중에 climb rate, sink alarm, dead-band, cadence 같은 정책이            */
/*    확정되면 Audio_App 또는 상위 플랫폼 레이어에서                           */
/*    동적으로 note를 만들어 넣는 쪽으로 바꾸는 것이 맞다.                    */
/* -------------------------------------------------------------------------- */
static const audio_note_event_t s_vario_placeholder_ch3_piano[] =
{
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_G5_HZ_X100, AUDIO_TICKS_SIXTEENTH, 860u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_C6_HZ_X100, AUDIO_TICKS_SIXTEENTH, 900u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_E6_HZ_X100, AUDIO_TICKS_SIXTEENTH, 940u),
    AUDIO_REST_EVENT(AUDIO_TICKS_SIXTEENTH),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_G6_HZ_X100, AUDIO_TICKS_EIGHTH, 960u)
};

static const audio_note_event_t s_vario_placeholder_ch4_percussion[] =
{
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_C2_HZ_X100, AUDIO_TICKS_EIGHTH, AUDIO_TICKS_SIXTEENTH, 740u),
    AUDIO_REST_EVENT(AUDIO_TICKS_EIGHTH),
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_C2_HZ_X100, AUDIO_TICKS_EIGHTH, AUDIO_TICKS_SIXTEENTH, 740u)
};

/* -------------------------------------------------------------------------- */
/*  BUTTON SHORT                                                               */
/* -------------------------------------------------------------------------- */
static const audio_note_event_t s_button_short_ch3_piano[] =
{
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_E6_HZ_X100, AUDIO_TICKS_SIXTEENTH, AUDIO_TICKS_THIRTY_SECOND, 850u)
};

static const audio_note_event_t s_button_short_ch4_percussion[] =
{
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_C3_HZ_X100, AUDIO_TICKS_SIXTEENTH, AUDIO_TICKS_THIRTY_SECOND, 650u)
};

/* -------------------------------------------------------------------------- */
/*  BUTTON LONG                                                                */
/* -------------------------------------------------------------------------- */
static const audio_note_event_t s_button_long_ch3_piano[] =
{
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_E6_HZ_X100, AUDIO_TICKS_SIXTEENTH, AUDIO_TICKS_THIRTY_SECOND, 860u),
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_G6_HZ_X100, AUDIO_TICKS_SIXTEENTH, AUDIO_TICKS_THIRTY_SECOND, 900u)
};

static const audio_note_event_t s_button_long_ch4_percussion[] =
{
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_C3_HZ_X100, AUDIO_TICKS_SIXTEENTH, AUDIO_TICKS_THIRTY_SECOND, 660u),
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_C3_HZ_X100, AUDIO_TICKS_SIXTEENTH, AUDIO_TICKS_THIRTY_SECOND, 700u)
};

/* -------------------------------------------------------------------------- */
/*  INFORMATION                                                                */
/*  - warning보다 훨씬 부드럽고                                                  */
/*  - boot보다 짧고 가볍게 만든다.                                              */
/* -------------------------------------------------------------------------- */
static const audio_note_event_t s_information_ch1_string[] =
{
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_C5_HZ_X100, AUDIO_TICKS_HALF, 500u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_G4_HZ_X100, AUDIO_TICKS_HALF, 460u)
};

static const audio_note_event_t s_information_ch2_brass[] =
{
    AUDIO_REST_EVENT(AUDIO_TICKS_QUARTER),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_G5_HZ_X100, AUDIO_TICKS_QUARTER, 700u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_C6_HZ_X100, AUDIO_TICKS_QUARTER, 720u)
};

static const audio_note_event_t s_information_ch3_piano[] =
{
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_C6_HZ_X100, AUDIO_TICKS_EIGHTH, 920u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_G6_HZ_X100, AUDIO_TICKS_EIGHTH, 920u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_E6_HZ_X100, AUDIO_TICKS_QUARTER, 900u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_G6_HZ_X100, AUDIO_TICKS_EIGHTH, 900u)
};

static const audio_note_event_t s_information_ch4_percussion[] =
{
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_C2_HZ_X100, AUDIO_TICKS_QUARTER, AUDIO_TICKS_SIXTEENTH, 680u),
    AUDIO_REST_EVENT(AUDIO_TICKS_QUARTER),
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_C2_HZ_X100, AUDIO_TICKS_EIGHTH, AUDIO_TICKS_SIXTEENTH, 680u),
    AUDIO_REST_EVENT(AUDIO_TICKS_EIGHTH)
};

/* -------------------------------------------------------------------------- */
/*  HOURLY CHIME                                                               */
/*  - 매 정시 알림용                                                            */
/*  - 지나치게 길지 않되 시각 인지성이 있도록                                   */
/*    4-note 상승 종지로 정리한다.                                             */
/* -------------------------------------------------------------------------- */
static const audio_note_event_t s_hourly_chime_ch1_string[] =
{
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_C5_HZ_X100, AUDIO_TICKS_WHOLE, 520u)
};

static const audio_note_event_t s_hourly_chime_ch2_brass[] =
{
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_G5_HZ_X100, AUDIO_TICKS_HALF, 700u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_C6_HZ_X100, AUDIO_TICKS_HALF, 720u)
};

static const audio_note_event_t s_hourly_chime_ch3_piano[] =
{
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_C6_HZ_X100, AUDIO_TICKS_QUARTER, 920u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_E6_HZ_X100, AUDIO_TICKS_QUARTER, 920u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_G6_HZ_X100, AUDIO_TICKS_QUARTER, 920u),
    AUDIO_NOTE_EVENT_V(AUDIO_NOTE_C7_HZ_X100, AUDIO_TICKS_QUARTER, 960u)
};

static const audio_note_event_t s_hourly_chime_ch4_percussion[] =
{
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_C3_HZ_X100, AUDIO_TICKS_QUARTER, AUDIO_TICKS_SIXTEENTH, 620u),
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_C3_HZ_X100, AUDIO_TICKS_QUARTER, AUDIO_TICKS_SIXTEENTH, 620u),
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_C3_HZ_X100, AUDIO_TICKS_QUARTER, AUDIO_TICKS_SIXTEENTH, 620u),
    AUDIO_NOTE_EVENT_G(AUDIO_NOTE_C3_HZ_X100, AUDIO_TICKS_QUARTER, AUDIO_TICKS_SIXTEENTH, 620u)
};

/* -------------------------------------------------------------------------- */
/*  note preset 인스턴스                                                       */
/* -------------------------------------------------------------------------- */
static const audio_note_preset_t s_note_preset_boot =
{
    "BOOT",
    132u,
    4u,
    2u,
    {
        s_boot_ch1_string,
        s_boot_ch2_brass,
        s_boot_ch3_piano,
        s_boot_ch4_percussion
    },
    {
        (uint16_t)ARRAY_SIZE(s_boot_ch1_string),
        (uint16_t)ARRAY_SIZE(s_boot_ch2_brass),
        (uint16_t)ARRAY_SIZE(s_boot_ch3_piano),
        (uint16_t)ARRAY_SIZE(s_boot_ch4_percussion)
    }
};

static const audio_note_preset_t s_note_preset_power_off =
{
    "POWER_OFF",
    104u,
    4u,
    2u,
    {
        s_power_off_ch1_string,
        s_power_off_ch2_brass,
        s_power_off_ch3_piano,
        s_power_off_ch4_percussion
    },
    {
        (uint16_t)ARRAY_SIZE(s_power_off_ch1_string),
        (uint16_t)ARRAY_SIZE(s_power_off_ch2_brass),
        (uint16_t)ARRAY_SIZE(s_power_off_ch3_piano),
        (uint16_t)ARRAY_SIZE(s_power_off_ch4_percussion)
    }
};

static const audio_note_preset_t s_note_preset_warning =
{
    "WARNING",
    148u,
    4u,
    2u,
    {
        s_warning_ch1_string,
        s_warning_ch2_brass,
        s_warning_ch3_piano,
        s_warning_ch4_percussion
    },
    {
        (uint16_t)ARRAY_SIZE(s_warning_ch1_string),
        (uint16_t)ARRAY_SIZE(s_warning_ch2_brass),
        (uint16_t)ARRAY_SIZE(s_warning_ch3_piano),
        (uint16_t)ARRAY_SIZE(s_warning_ch4_percussion)
    }
};

static const audio_note_preset_t s_note_preset_error =
{
    "ERROR",
    112u,
    4u,
    2u,
    {
        s_error_ch1_string,
        s_error_ch2_brass,
        s_error_ch3_piano,
        s_error_ch4_percussion
    },
    {
        (uint16_t)ARRAY_SIZE(s_error_ch1_string),
        (uint16_t)ARRAY_SIZE(s_error_ch2_brass),
        (uint16_t)ARRAY_SIZE(s_error_ch3_piano),
        (uint16_t)ARRAY_SIZE(s_error_ch4_percussion)
    }
};

static const audio_note_preset_t s_note_preset_fatal =
{
    "FATAL",
    84u,
    4u,
    2u,
    {
        s_fatal_ch1_string,
        s_fatal_ch2_brass,
        s_fatal_ch3_piano,
        s_fatal_ch4_percussion
    },
    {
        (uint16_t)ARRAY_SIZE(s_fatal_ch1_string),
        (uint16_t)ARRAY_SIZE(s_fatal_ch2_brass),
        (uint16_t)ARRAY_SIZE(s_fatal_ch3_piano),
        (uint16_t)ARRAY_SIZE(s_fatal_ch4_percussion)
    }
};

static const audio_note_preset_t s_note_preset_vario_placeholder =
{
    "VARIO_PLACEHOLDER",
    180u,
    4u,
    2u,
    {
        0,
        0,
        s_vario_placeholder_ch3_piano,
        s_vario_placeholder_ch4_percussion
    },
    {
        0u,
        0u,
        (uint16_t)ARRAY_SIZE(s_vario_placeholder_ch3_piano),
        (uint16_t)ARRAY_SIZE(s_vario_placeholder_ch4_percussion)
    }
};

static const audio_note_preset_t s_note_preset_button_short =
{
    "BUTTON_SHORT",
    200u,
    4u,
    2u,
    {
        0,
        0,
        s_button_short_ch3_piano,
        s_button_short_ch4_percussion
    },
    {
        0u,
        0u,
        (uint16_t)ARRAY_SIZE(s_button_short_ch3_piano),
        (uint16_t)ARRAY_SIZE(s_button_short_ch4_percussion)
    }
};

static const audio_note_preset_t s_note_preset_button_long =
{
    "BUTTON_LONG",
    180u,
    4u,
    2u,
    {
        0,
        0,
        s_button_long_ch3_piano,
        s_button_long_ch4_percussion
    },
    {
        0u,
        0u,
        (uint16_t)ARRAY_SIZE(s_button_long_ch3_piano),
        (uint16_t)ARRAY_SIZE(s_button_long_ch4_percussion)
    }
};

static const audio_note_preset_t s_note_preset_information =
{
    "INFORMATION",
    124u,
    4u,
    2u,
    {
        s_information_ch1_string,
        s_information_ch2_brass,
        s_information_ch3_piano,
        s_information_ch4_percussion
    },
    {
        (uint16_t)ARRAY_SIZE(s_information_ch1_string),
        (uint16_t)ARRAY_SIZE(s_information_ch2_brass),
        (uint16_t)ARRAY_SIZE(s_information_ch3_piano),
        (uint16_t)ARRAY_SIZE(s_information_ch4_percussion)
    }
};

static const audio_note_preset_t s_note_preset_hourly_chime =
{
    "HOURLY_CHIME",
    92u,
    4u,
    2u,
    {
        s_hourly_chime_ch1_string,
        s_hourly_chime_ch2_brass,
        s_hourly_chime_ch3_piano,
        s_hourly_chime_ch4_percussion
    },
    {
        (uint16_t)ARRAY_SIZE(s_hourly_chime_ch1_string),
        (uint16_t)ARRAY_SIZE(s_hourly_chime_ch2_brass),
        (uint16_t)ARRAY_SIZE(s_hourly_chime_ch3_piano),
        (uint16_t)ARRAY_SIZE(s_hourly_chime_ch4_percussion)
    }
};

/* -------------------------------------------------------------------------- */
/*  공개 API: timbre lookup                                                    */
/* -------------------------------------------------------------------------- */
const audio_timbre_preset_t *Audio_Presets_GetTimbre(audio_timbre_preset_id_t timbre_id)
{
    switch (timbre_id)
    {
    case AUDIO_TIMBRE_PURE_SINE:
        return &s_timbre_pure_sine;

    case AUDIO_TIMBRE_PURE_SQUARE:
        return &s_timbre_pure_square;

    case AUDIO_TIMBRE_PURE_SAW:
        return &s_timbre_pure_saw;

    case AUDIO_TIMBRE_CH1_STRING:
        return &s_timbre_ch1_string;

    case AUDIO_TIMBRE_CH2_BRASS:
        return &s_timbre_ch2_brass;

    case AUDIO_TIMBRE_CH3_PIANO:
        return &s_timbre_ch3_piano;

    case AUDIO_TIMBRE_CH4_PERCUSSION:
        return &s_timbre_ch4_percussion;

    default:
        return 0;
    }
}

/* -------------------------------------------------------------------------- */
/*  공개 API: stack lookup                                                     */
/* -------------------------------------------------------------------------- */
const audio_timbre_stack_preset_t *Audio_Presets_GetTimbreStack(audio_timbre_stack_preset_id_t stack_id)
{
    switch (stack_id)
    {
    case AUDIO_TIMBRE_STACK_STANDARD_4CH:
        return &s_timbre_stack_standard_4ch;

    default:
        return 0;
    }
}

/* -------------------------------------------------------------------------- */
/*  공개 API: note preset lookup                                               */
/* -------------------------------------------------------------------------- */
const audio_note_preset_t *Audio_Presets_GetNotePreset(audio_note_preset_id_t note_preset_id)
{
    switch (note_preset_id)
    {
    case AUDIO_NOTE_PRESET_BOOT:
        return &s_note_preset_boot;

    case AUDIO_NOTE_PRESET_POWER_OFF:
        return &s_note_preset_power_off;

    case AUDIO_NOTE_PRESET_WARNING:
        return &s_note_preset_warning;

    case AUDIO_NOTE_PRESET_ERROR:
        return &s_note_preset_error;

    case AUDIO_NOTE_PRESET_FATAL:
        return &s_note_preset_fatal;

    case AUDIO_NOTE_PRESET_VARIO_PLACEHOLDER:
        return &s_note_preset_vario_placeholder;

    case AUDIO_NOTE_PRESET_BUTTON_SHORT:
        return &s_note_preset_button_short;

    case AUDIO_NOTE_PRESET_BUTTON_LONG:
        return &s_note_preset_button_long;

    case AUDIO_NOTE_PRESET_INFORMATION:
        return &s_note_preset_information;

    case AUDIO_NOTE_PRESET_HOURLY_CHIME:
        return &s_note_preset_hourly_chime;

    default:
        return 0;
    }
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 이름 lookup helper                                               */
/* -------------------------------------------------------------------------- */
const char *Audio_Presets_GetTimbreName(audio_timbre_preset_id_t timbre_id)
{
    const audio_timbre_preset_t *timbre;

    timbre = Audio_Presets_GetTimbre(timbre_id);
    return (timbre != 0) ? timbre->name : "UNKNOWN_TIMBRE";
}

const char *Audio_Presets_GetNotePresetName(audio_note_preset_id_t note_preset_id)
{
    const audio_note_preset_t *preset;

    preset = Audio_Presets_GetNotePreset(note_preset_id);
    return (preset != 0) ? preset->name : "UNKNOWN_NOTE_PRESET";
}

/* -------------------------------------------------------------------------- */
/*  추후 preset 추가 가이드                                                    */
/*                                                                            */
/*  1) 새 효과음을 추가할 때는 먼저                                            */
/*     ch3 PIANO melody track부터 만든다.                                      */
/*                                                                            */
/*  2) 그 다음                                                                 */
/*     - ch1 STRING 에 sustain/harmony                                         */
/*     - ch2 BRASS  에 accent/stab                                             */
/*     - ch4 PERC   에 beat/click                                              */
/*     를 얹어서 4채널 합성 버전을 만든다.                                     */
/*                                                                            */
/*  3) Audio_App.c에서는                                                       */
/*     - mixed wrapper  : Audio_Driver_PlaySequencePreset()                    */
/*     - mono wrapper   : Audio_Driver_PlaySingleTrackPreset()                 */
/*     두 가지를 같이 노출해 두면                                              */
/*     나중에 사용자가 A/B 비교 청취 후 더 좋은 쪽만 남기기 쉽다.              */
/* -------------------------------------------------------------------------- */
