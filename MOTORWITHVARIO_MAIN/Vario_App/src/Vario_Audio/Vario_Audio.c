#include "Vario_Audio.h"

#include "Audio_App.h"
#include "Vario_Settings.h"
#include "Vario_State.h"

#include <stdbool.h>
#include <stddef.h>

/* -------------------------------------------------------------------------- */
/*  제품용 variometer audio engine                                             */
/*                                                                            */
/*  이번 리팩토링의 핵심 의도                                                  */
/*  1) APP_ALTITUDE debug audio 우회로를 제품용 바리오 경로에서 떼어 낸다.    */
/*  2) APP_STATE는 low-level snapshot / 설정 mirror 창고로 유지한다.          */
/*  3) Vario_State는 fast vario 등 바리오 전용 파생 상태를 공급한다.          */
/*  4) Vario_Audio는 그 파생 상태를 "실제 제품 소리 정책"으로 바꿔서         */
/*     Audio_App façade를 통해 Audio_Driver transport에 전달한다.             */
/*                                                                            */
/*  결과적으로 실제 계층은 아래처럼 정리된다.                                 */
/*                                                                            */
/*      APP_STATE(low-level snapshot)                                         */
/*          -> Vario_State(derived vario runtime)                             */
/*              -> Vario_Audio(product sound policy)                          */
/*                  -> Audio_App(façade)                                      */
/*                      -> Audio_Driver(transport)                            */
/* -------------------------------------------------------------------------- */

typedef enum
{
    VARIO_AUDIO_MODE_IDLE = 0u,
    VARIO_AUDIO_MODE_CLIMB,
    VARIO_AUDIO_MODE_SINK_CHIRP,
    VARIO_AUDIO_MODE_SINK_CONTINUOUS
} vario_audio_mode_t;

typedef struct
{
    float    x;
    uint16_t y;
} vario_audio_curve_u16_t;

typedef struct
{
    app_audio_waveform_t waveform;
    uint16_t             freq_hz;
    uint16_t             level_permille;
    uint16_t             period_ms;
    uint16_t             on_ms;
    bool                 pulsed;
} vario_audio_target_t;

typedef struct
{
    bool                 initialized;
    bool                 driver_active;
    bool                 pulse_on;
    vario_audio_mode_t   mode;
    app_audio_waveform_t waveform;
    uint32_t             last_task_ms;
    uint32_t             next_cycle_ms;
    uint32_t             pulse_off_ms;
    float                gate_vario_cms;
    float                tone_vario_cms;
    uint16_t             current_freq_hz;
    uint16_t             current_level_permille;
} vario_audio_runtime_t;

static vario_audio_runtime_t s_vario_audio;

/* -------------------------------------------------------------------------- */
/*  마지막으로 실제 driver에 적용한 볼륨 캐시                                  */
/* -------------------------------------------------------------------------- */
static uint8_t s_vario_audio_last_applied_volume = 0xFFu;

/* -------------------------------------------------------------------------- */
/*  climb profile                                                              */
/*                                                                            */
/*  commercial variometer 느낌을 위해                                          */
/*  - pitch                                                                    */
/*  - beep cadence                                                              */
/*  - beep duty                                                                 */
/*  - loudness                                                                  */
/*  를 전부 같은 선형식으로 만들지 않고                                        */
/*  piecewise curve로 분리한다.                                                */
/*                                                                            */
/*  x축은 "climb start threshold를 넘긴 뒤 얼마나 더 상승했는가"를            */
/*  0.0 ~ 1.0 progress로 정규화한 값이다.                                      */
/* -------------------------------------------------------------------------- */
static const vario_audio_curve_u16_t s_vario_climb_freq_curve[] = {
    {0.00f,  880u},
    {0.10f,  930u},
    {0.25f, 1010u},
    {0.45f, 1160u},
    {0.70f, 1440u},
    {1.00f, 1820u},
};

static const vario_audio_curve_u16_t s_vario_climb_period_curve[] = {
    {0.00f, 310u},
    {0.10f, 285u},
    {0.25f, 245u},
    {0.45f, 190u},
    {0.70f, 135u},
    {1.00f,  92u},
};

static const vario_audio_curve_u16_t s_vario_climb_duty_curve[] = {
    {0.00f, 340u},
    {0.10f, 400u},
    {0.25f, 470u},
    {0.45f, 560u},
    {0.70f, 700u},
    {1.00f, 880u},
};

static const vario_audio_curve_u16_t s_vario_climb_level_curve[] = {
    {0.00f, 380u},
    {0.10f, 430u},
    {0.25f, 500u},
    {0.45f, 610u},
    {0.70f, 740u},
    {1.00f, 860u},
};

/* -------------------------------------------------------------------------- */
/*  Digifly 스타일 moderate sink chirp profile                                 */
/*                                                                            */
/*  x축은 sink start threshold 부근에서 sink continuous threshold 직전까지의   */
/*  band progress다.                                                           */
/* -------------------------------------------------------------------------- */
static const vario_audio_curve_u16_t s_vario_sink_chirp_freq_curve[] = {
    {0.00f, 420u},
    {0.15f, 405u},
    {0.35f, 385u},
    {0.60f, 355u},
    {0.82f, 325u},
    {1.00f, 300u},
};

static const vario_audio_curve_u16_t s_vario_sink_chirp_period_curve[] = {
    {0.00f, 760u},
    {0.15f, 650u},
    {0.35f, 540u},
    {0.60f, 410u},
    {0.82f, 315u},
    {1.00f, 240u},
};

static const vario_audio_curve_u16_t s_vario_sink_chirp_on_curve[] = {
    {0.00f,  38u},
    {0.15f,  42u},
    {0.35f,  48u},
    {0.60f,  56u},
    {0.82f,  64u},
    {1.00f,  72u},
};

static const vario_audio_curve_u16_t s_vario_sink_chirp_level_curve[] = {
    {0.00f, 300u},
    {0.15f, 335u},
    {0.35f, 370u},
    {0.60f, 415u},
    {0.82f, 460u},
    {1.00f, 510u},
};

/* -------------------------------------------------------------------------- */
/*  strong sink continuous profile                                             */
/*                                                                            */
/*  x축은 sink continuous threshold를 지난 뒤 추가 sink 크기를                 */
/*  0.0 ~ 1.0으로 정규화한 값이다.                                             */
/* -------------------------------------------------------------------------- */
static const vario_audio_curve_u16_t s_vario_sink_cont_freq_curve[] = {
    {0.00f, 320u},
    {0.15f, 300u},
    {0.35f, 285u},
    {0.60f, 265u},
    {0.82f, 245u},
    {1.00f, 225u},
};

static const vario_audio_curve_u16_t s_vario_sink_cont_level_curve[] = {
    {0.00f, 430u},
    {0.15f, 480u},
    {0.35f, 540u},
    {0.60f, 610u},
    {0.82f, 690u},
    {1.00f, 760u},
};

static uint8_t vario_audio_clamp_percent(uint8_t percent)
{
    if (percent > 100u)
    {
        return 100u;
    }

    return percent;
}

static uint16_t vario_audio_clamp_u16(uint16_t value, uint16_t min_v, uint16_t max_v)
{
    if (value < min_v)
    {
        return min_v;
    }

    if (value > max_v)
    {
        return max_v;
    }

    return value;
}

static int16_t vario_audio_clamp_s16(int16_t value, int16_t min_v, int16_t max_v)
{
    if (value < min_v)
    {
        return min_v;
    }

    if (value > max_v)
    {
        return max_v;
    }

    return value;
}

static int16_t vario_audio_abs_s16(int16_t value)
{
    return (value < 0) ? (int16_t)(-value) : value;
}

static float vario_audio_clampf(float value, float min_v, float max_v)
{
    if (value < min_v)
    {
        return min_v;
    }

    if (value > max_v)
    {
        return max_v;
    }

    return value;
}

static float vario_audio_lerp_f(float a, float b, float t)
{
    t = vario_audio_clampf(t, 0.0f, 1.0f);
    return a + ((b - a) * t);
}

static uint16_t vario_audio_lerp_u16(uint16_t a, uint16_t b, float t)
{
    float value;

    value = vario_audio_lerp_f((float)a, (float)b, t);
    if (value < 0.0f)
    {
        return 0u;
    }

    return (uint16_t)(value + 0.5f);
}

static int32_t vario_audio_round_mps_to_cms(float value_mps)
{
    value_mps *= 100.0f;

    if (value_mps >= 0.0f)
    {
        return (int32_t)(value_mps + 0.5f);
    }

    return (int32_t)(value_mps - 0.5f);
}

static bool vario_audio_time_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return ((int32_t)(now_ms - deadline_ms) >= 0) ? true : false;
}

static bool vario_audio_mode_owns_tone(vario_mode_t mode)
{
    /* ---------------------------------------------------------------------- */
    /*  제품용 바리오는 settings / quickset 화면에서도 계속 살아 있게 둔다.   */
    /*                                                                        */
    /*  이유                                                                   */
    /*  - 현장 튜닝 중에도 실제 톤 변화를 바로 귀로 확인할 수 있어야 하고     */
    /*  - commercial variometer도 보통 설정 메뉴 진입이 tone ownership을       */
    /*    끊는 이유가 거의 없다.                                               */
    /* ---------------------------------------------------------------------- */
    return (mode != VARIO_MODE_COUNT) ? true : false;
}

static float vario_audio_response_norm(const vario_settings_t *settings)
{
    uint8_t response_level;

    if (settings == NULL)
    {
        return 0.6667f;
    }

    response_level = vario_audio_clamp_percent(settings->vario_damping_level);
    response_level = vario_audio_clamp_u16(response_level, 1u, 10u);
    return ((float)(response_level - 1u)) / 9.0f;
}

static uint16_t vario_audio_gate_tau_ms(const vario_settings_t *settings)
{
    return vario_audio_lerp_u16(90u, 18u, vario_audio_response_norm(settings));
}

static uint16_t vario_audio_tone_tau_ms(const vario_settings_t *settings)
{
    return vario_audio_lerp_u16(140u, 42u, vario_audio_response_norm(settings));
}

static uint16_t vario_audio_attack_glide_ms(const vario_settings_t *settings)
{
    return vario_audio_lerp_u16(52u, 16u, vario_audio_response_norm(settings));
}

static uint16_t vario_audio_release_ms(const vario_settings_t *settings)
{
    return vario_audio_lerp_u16(84u, 26u, vario_audio_response_norm(settings));
}

static float vario_audio_apply_lpf(float current,
                                   float target,
                                   uint32_t dt_ms,
                                   uint16_t tau_ms)
{
    float alpha;

    if ((dt_ms == 0u) || (tau_ms == 0u))
    {
        return target;
    }

    alpha = ((float)dt_ms) / (((float)tau_ms) + ((float)dt_ms));
    alpha = vario_audio_clampf(alpha, 0.0f, 1.0f);
    return current + ((target - current) * alpha);
}

static uint16_t vario_audio_curve_sample_u16(const vario_audio_curve_u16_t *curve,
                                             size_t count,
                                             float x)
{
    size_t i;

    if ((curve == NULL) || (count == 0u))
    {
        return 0u;
    }

    if (x <= curve[0].x)
    {
        return curve[0].y;
    }

    for (i = 1u; i < count; ++i)
    {
        if (x <= curve[i].x)
        {
            float span;
            float local_t;

            span = curve[i].x - curve[i - 1u].x;
            if (span <= 0.0f)
            {
                return curve[i].y;
            }

            local_t = (x - curve[i - 1u].x) / span;
            return vario_audio_lerp_u16(curve[i - 1u].y,
                                        curve[i].y,
                                        local_t);
        }
    }

    return curve[count - 1u].y;
}

static int16_t vario_audio_get_sink_continuous_threshold_cms(const vario_settings_t *settings)
{
    int16_t sink_start_cms;
    int16_t sink_cont_cms;

    if (settings == NULL)
    {
        return -250;
    }

    sink_start_cms = settings->sink_tone_threshold_cms;
    sink_cont_cms  = settings->sink_continuous_threshold_cms;

    if (sink_cont_cms > sink_start_cms)
    {
        sink_cont_cms = sink_start_cms;
    }

    return sink_cont_cms;
}

static bool vario_audio_is_context_active(const vario_settings_t *settings,
                                          const vario_runtime_t *rt,
                                          vario_mode_t mode)
{
    if ((settings == NULL) || (rt == NULL))
    {
        return false;
    }

    if (settings->audio_enabled == 0u)
    {
        return false;
    }

    if ((settings->beep_only_when_flying != 0u) && (rt->flight_active == false))
    {
        return false;
    }

    if ((rt->derived_valid == false) || (vario_audio_mode_owns_tone(mode) == false))
    {
        return false;
    }

    return true;
}

static vario_audio_mode_t vario_audio_pick_mode(const vario_settings_t *settings,
                                                int16_t gate_vario_cms,
                                                vario_audio_mode_t current_mode)
{
    int16_t climb_enter_cms;
    int16_t climb_exit_cms;
    int16_t sink_start_cms;
    int16_t sink_exit_cms;
    int16_t sink_cont_cms;
    int16_t sink_cont_exit_cms;

    if (settings == NULL)
    {
        return VARIO_AUDIO_MODE_IDLE;
    }

    climb_enter_cms = vario_audio_clamp_s16(settings->climb_tone_threshold_cms, 0, 300);
    climb_exit_cms  = vario_audio_clamp_s16((int16_t)(climb_enter_cms - 8), 0, 300);

    sink_start_cms = vario_audio_clamp_s16(settings->sink_tone_threshold_cms, -500, 0);
    sink_exit_cms  = vario_audio_clamp_s16((int16_t)(sink_start_cms + 12), -500, 0);

    sink_cont_cms      = vario_audio_get_sink_continuous_threshold_cms(settings);
    sink_cont_exit_cms = vario_audio_clamp_s16((int16_t)(sink_cont_cms + 18), -800, 0);

    switch (current_mode)
    {
        case VARIO_AUDIO_MODE_CLIMB:
            if (gate_vario_cms >= climb_exit_cms)
            {
                return VARIO_AUDIO_MODE_CLIMB;
            }
            break;

        case VARIO_AUDIO_MODE_SINK_CONTINUOUS:
            if (gate_vario_cms <= sink_cont_exit_cms)
            {
                return VARIO_AUDIO_MODE_SINK_CONTINUOUS;
            }
            break;

        case VARIO_AUDIO_MODE_SINK_CHIRP:
            if ((gate_vario_cms <= sink_exit_cms) &&
                (gate_vario_cms > sink_cont_cms))
            {
                return VARIO_AUDIO_MODE_SINK_CHIRP;
            }

            if (gate_vario_cms <= sink_cont_cms)
            {
                return VARIO_AUDIO_MODE_SINK_CONTINUOUS;
            }
            break;

        case VARIO_AUDIO_MODE_IDLE:
        default:
            break;
    }

    if (gate_vario_cms >= climb_enter_cms)
    {
        return VARIO_AUDIO_MODE_CLIMB;
    }

    if (gate_vario_cms <= sink_cont_cms)
    {
        return VARIO_AUDIO_MODE_SINK_CONTINUOUS;
    }

    if (gate_vario_cms <= sink_start_cms)
    {
        return VARIO_AUDIO_MODE_SINK_CHIRP;
    }

    return VARIO_AUDIO_MODE_IDLE;
}

static void vario_audio_build_climb_target(const vario_settings_t *settings,
                                           int16_t tone_vario_cms,
                                           vario_audio_target_t *out_target)
{
    int16_t climb_start_cms;
    float   progress;
    uint16_t period_ms;
    uint16_t duty_permille;
    uint16_t on_ms;

    if ((settings == NULL) || (out_target == NULL))
    {
        return;
    }

    climb_start_cms = vario_audio_clamp_s16(settings->climb_tone_threshold_cms, 0, 300);

    /* ---------------------------------------------------------------------- */
    /*  threshold를 넘긴 양만 따로 떼서 progress를 만든다.                    */
    /*                                                                        */
    /*  이렇게 해야 climb start를 사용자가 바꿔도                             */
    /*  product tone의 성격(초기 삑, 중간 cadence, 강상승 연속음)이             */
    /*  전체적으로 같이 옮겨 다니지 않고 자연스럽게 유지된다.                 */
    /* ---------------------------------------------------------------------- */
    progress = ((float)(tone_vario_cms - climb_start_cms)) / 550.0f;
    progress = vario_audio_clampf(progress, 0.0f, 1.0f);

    period_ms     = vario_audio_curve_sample_u16(s_vario_climb_period_curve,
                                                 sizeof(s_vario_climb_period_curve) / sizeof(s_vario_climb_period_curve[0]),
                                                 progress);
    duty_permille = vario_audio_curve_sample_u16(s_vario_climb_duty_curve,
                                                 sizeof(s_vario_climb_duty_curve) / sizeof(s_vario_climb_duty_curve[0]),
                                                 progress);
    on_ms         = (uint16_t)(((uint32_t)period_ms * (uint32_t)duty_permille) / 1000u);
    on_ms         = vario_audio_clamp_u16(on_ms, 38u, (period_ms > 8u) ? (uint16_t)(period_ms - 8u) : period_ms);

    out_target->waveform        = APP_AUDIO_WAVEFORM_SQUARE;
    out_target->freq_hz         = vario_audio_curve_sample_u16(s_vario_climb_freq_curve,
                                                               sizeof(s_vario_climb_freq_curve) / sizeof(s_vario_climb_freq_curve[0]),
                                                               progress);
    out_target->level_permille  = vario_audio_curve_sample_u16(s_vario_climb_level_curve,
                                                               sizeof(s_vario_climb_level_curve) / sizeof(s_vario_climb_level_curve[0]),
                                                               progress);
    out_target->period_ms       = period_ms;
    out_target->on_ms           = on_ms;
    out_target->pulsed          = true;
}

static void vario_audio_build_sink_chirp_target(const vario_settings_t *settings,
                                                int16_t tone_vario_cms,
                                                vario_audio_target_t *out_target)
{
    int16_t sink_start_abs_cms;
    int16_t sink_cont_abs_cms;
    int16_t abs_sink_cms;
    float   progress;
    int16_t band_span_cms;

    if ((settings == NULL) || (out_target == NULL))
    {
        return;
    }

    sink_start_abs_cms = vario_audio_abs_s16(settings->sink_tone_threshold_cms);
    sink_cont_abs_cms  = vario_audio_abs_s16(vario_audio_get_sink_continuous_threshold_cms(settings));
    abs_sink_cms       = vario_audio_abs_s16((tone_vario_cms < 0) ? tone_vario_cms : 0);
    band_span_cms      = (int16_t)(sink_cont_abs_cms - sink_start_abs_cms);
    if (band_span_cms < 30)
    {
        band_span_cms = 30;
    }

    progress = ((float)(abs_sink_cms - sink_start_abs_cms)) / ((float)band_span_cms);
    progress = vario_audio_clampf(progress, 0.0f, 1.0f);

    out_target->waveform       = APP_AUDIO_WAVEFORM_SAW;
    out_target->freq_hz        = vario_audio_curve_sample_u16(s_vario_sink_chirp_freq_curve,
                                                              sizeof(s_vario_sink_chirp_freq_curve) / sizeof(s_vario_sink_chirp_freq_curve[0]),
                                                              progress);
    out_target->level_permille = vario_audio_curve_sample_u16(s_vario_sink_chirp_level_curve,
                                                              sizeof(s_vario_sink_chirp_level_curve) / sizeof(s_vario_sink_chirp_level_curve[0]),
                                                              progress);
    out_target->period_ms      = vario_audio_curve_sample_u16(s_vario_sink_chirp_period_curve,
                                                              sizeof(s_vario_sink_chirp_period_curve) / sizeof(s_vario_sink_chirp_period_curve[0]),
                                                              progress);
    out_target->on_ms          = vario_audio_curve_sample_u16(s_vario_sink_chirp_on_curve,
                                                              sizeof(s_vario_sink_chirp_on_curve) / sizeof(s_vario_sink_chirp_on_curve[0]),
                                                              progress);
    out_target->on_ms          = vario_audio_clamp_u16(out_target->on_ms,
                                                       28u,
                                                       (out_target->period_ms > 12u) ?
                                                           (uint16_t)(out_target->period_ms - 12u) :
                                                           out_target->period_ms);
    out_target->pulsed         = true;
}

static void vario_audio_build_sink_continuous_target(const vario_settings_t *settings,
                                                     int16_t tone_vario_cms,
                                                     vario_audio_target_t *out_target)
{
    int16_t sink_cont_abs_cms;
    int16_t abs_sink_cms;
    float   progress;

    if ((settings == NULL) || (out_target == NULL))
    {
        return;
    }

    sink_cont_abs_cms = vario_audio_abs_s16(vario_audio_get_sink_continuous_threshold_cms(settings));
    abs_sink_cms      = vario_audio_abs_s16((tone_vario_cms < 0) ? tone_vario_cms : 0);

    progress = ((float)(abs_sink_cms - sink_cont_abs_cms)) / 450.0f;
    progress = vario_audio_clampf(progress, 0.0f, 1.0f);

    out_target->waveform       = APP_AUDIO_WAVEFORM_SAW;
    out_target->freq_hz        = vario_audio_curve_sample_u16(s_vario_sink_cont_freq_curve,
                                                              sizeof(s_vario_sink_cont_freq_curve) / sizeof(s_vario_sink_cont_freq_curve[0]),
                                                              progress);
    out_target->level_permille = vario_audio_curve_sample_u16(s_vario_sink_cont_level_curve,
                                                              sizeof(s_vario_sink_cont_level_curve) / sizeof(s_vario_sink_cont_level_curve[0]),
                                                              progress);
    out_target->period_ms      = 0u;
    out_target->on_ms          = 0u;
    out_target->pulsed         = false;
}

static void vario_audio_stop_driver(const vario_settings_t *settings)
{
    if ((s_vario_audio.driver_active != false) || (Audio_App_IsVariometerActive() != false))
    {
        (void)Audio_App_VariometerStop(vario_audio_release_ms(settings));
    }

    s_vario_audio.driver_active = false;
    s_vario_audio.pulse_on      = false;
    s_vario_audio.current_freq_hz = 0u;
    s_vario_audio.current_level_permille = 0u;
}

static void vario_audio_start_or_update_tone(const vario_settings_t *settings,
                                             const vario_audio_target_t *target)
{
    uint16_t attack_ms;
    uint16_t start_level;
    uint32_t start_freq;

    if ((settings == NULL) || (target == NULL))
    {
        return;
    }

    attack_ms = vario_audio_attack_glide_ms(settings);

    /* ---------------------------------------------------------------------- */
    /*  commercial vario 느낌을 내기 위한 짧은 attack                          */
    /*                                                                        */
    /*  - climb : 약간 낮은 pitch에서 목표 pitch로 짧게 미끄러지며 들어간다.   */
    /*  - sink  : 이미 낮은 톤이므로 변위량을 조금만 준다.                      */
    /* ---------------------------------------------------------------------- */
    if (target->waveform == APP_AUDIO_WAVEFORM_SQUARE)
    {
        start_freq  = (target->freq_hz > 70u) ? (uint32_t)(target->freq_hz - 70u) : target->freq_hz;
        start_level = (target->level_permille > 55u) ? (uint16_t)(target->level_permille - 55u) : target->level_permille;
    }
    else
    {
        start_freq  = (target->freq_hz > 35u) ? (uint32_t)(target->freq_hz - 35u) : target->freq_hz;
        start_level = (target->level_permille > 30u) ? (uint16_t)(target->level_permille - 30u) : target->level_permille;
    }

    if ((s_vario_audio.driver_active == false) ||
        (Audio_App_IsVariometerActive() == false) ||
        (s_vario_audio.waveform != target->waveform))
    {
        (void)Audio_App_VariometerStart(target->waveform,
                                        start_freq,
                                        start_level);
        s_vario_audio.driver_active = true;
        s_vario_audio.waveform      = target->waveform;
    }

    (void)Audio_App_VariometerSetTarget(target->freq_hz,
                                        target->level_permille,
                                        attack_ms);

    s_vario_audio.current_freq_hz         = target->freq_hz;
    s_vario_audio.current_level_permille  = target->level_permille;
}

static void vario_audio_run_pulsed_mode(const vario_settings_t *settings,
                                        const vario_audio_target_t *target,
                                        uint32_t now_ms)
{
    if ((settings == NULL) || (target == NULL))
    {
        return;
    }

    if ((s_vario_audio.pulse_on == false) &&
        ((s_vario_audio.next_cycle_ms == 0u) ||
         (vario_audio_time_reached(now_ms, s_vario_audio.next_cycle_ms) != false)))
    {
        s_vario_audio.pulse_on     = true;
        s_vario_audio.pulse_off_ms = now_ms + target->on_ms;
        s_vario_audio.next_cycle_ms = now_ms + target->period_ms;
        vario_audio_start_or_update_tone(settings, target);
        return;
    }

    if (s_vario_audio.pulse_on != false)
    {
        if (vario_audio_time_reached(now_ms, s_vario_audio.pulse_off_ms) != false)
        {
            vario_audio_stop_driver(settings);
            return;
        }

        /* ------------------------------------------------------------------ */
        /*  같은 chirp 구간 안에서도 target freq/level은 계속 좇게 둔다.       */
        /*  사용자가 실제 양력을 더 세게 타기 시작하면                          */
        /*  다음 chirp까지 기다리지 않고 현재 chirp의 top-end가 즉시 따라간다. */
        /* ------------------------------------------------------------------ */
        vario_audio_start_or_update_tone(settings, target);
    }
}

static void vario_audio_run_continuous_mode(const vario_settings_t *settings,
                                            const vario_audio_target_t *target)
{
    if ((settings == NULL) || (target == NULL))
    {
        return;
    }

    s_vario_audio.pulse_on      = false;
    s_vario_audio.next_cycle_ms = 0u;
    s_vario_audio.pulse_off_ms  = 0u;
    vario_audio_start_or_update_tone(settings, target);
}

void Vario_Audio_Init(void)
{
    const vario_settings_t *settings;
    uint8_t                 volume_percent;

    settings = Vario_Settings_Get();
    volume_percent = (settings != NULL) ?
        vario_audio_clamp_percent(settings->audio_volume_percent) : 75u;

    s_vario_audio.initialized = false;
    s_vario_audio.driver_active = false;
    s_vario_audio.pulse_on = false;
    s_vario_audio.mode = VARIO_AUDIO_MODE_IDLE;
    s_vario_audio.waveform = APP_AUDIO_WAVEFORM_NONE;
    s_vario_audio.last_task_ms = 0u;
    s_vario_audio.next_cycle_ms = 0u;
    s_vario_audio.pulse_off_ms = 0u;
    s_vario_audio.gate_vario_cms = 0.0f;
    s_vario_audio.tone_vario_cms = 0.0f;
    s_vario_audio.current_freq_hz = 0u;
    s_vario_audio.current_level_permille = 0u;

    Audio_App_SetVolumePercent(volume_percent);
    s_vario_audio_last_applied_volume = volume_percent;

    Audio_App_ReleaseVariometer(0u);
}

void Vario_Audio_Task(uint32_t now_ms)
{
    const vario_settings_t *settings;
    const vario_runtime_t  *rt;
    vario_mode_t            mode;
    uint8_t                 volume_percent;
    uint32_t                dt_ms;
    int16_t                 raw_vario_cms;
    int16_t                 gate_vario_cms;
    int16_t                 tone_vario_cms;
    vario_audio_mode_t      next_mode;
    vario_audio_target_t    target;

    settings = Vario_Settings_Get();
    rt       = Vario_State_GetRuntime();
    mode     = Vario_State_GetMode();

    if ((settings == NULL) || (rt == NULL))
    {
        vario_audio_stop_driver(NULL);
        return;
    }

    volume_percent = vario_audio_clamp_percent(settings->audio_volume_percent);
    if (volume_percent != s_vario_audio_last_applied_volume)
    {
        Audio_App_SetVolumePercent(volume_percent);
        s_vario_audio_last_applied_volume = volume_percent;
    }

    if (Audio_App_IsVariometerActive() == false)
    {
        s_vario_audio.driver_active = false;
    }

    if (vario_audio_is_context_active(settings, rt, mode) == false)
    {
        s_vario_audio.mode = VARIO_AUDIO_MODE_IDLE;
        vario_audio_stop_driver(settings);
        s_vario_audio.next_cycle_ms = 0u;
        s_vario_audio.pulse_off_ms = 0u;
        s_vario_audio.last_task_ms = now_ms;
        return;
    }

    raw_vario_cms = (int16_t)vario_audio_round_mps_to_cms(rt->fast_vario_bar_mps);

    if ((s_vario_audio.initialized == false) ||
        (s_vario_audio.last_task_ms == 0u) ||
        (now_ms < s_vario_audio.last_task_ms) ||
        ((now_ms - s_vario_audio.last_task_ms) > 300u))
    {
        s_vario_audio.gate_vario_cms = (float)raw_vario_cms;
        s_vario_audio.tone_vario_cms = (float)raw_vario_cms;
        s_vario_audio.initialized    = true;
        dt_ms                        = 0u;
    }
    else
    {
        dt_ms = now_ms - s_vario_audio.last_task_ms;
        s_vario_audio.gate_vario_cms = vario_audio_apply_lpf(s_vario_audio.gate_vario_cms,
                                                             (float)raw_vario_cms,
                                                             dt_ms,
                                                             vario_audio_gate_tau_ms(settings));
        s_vario_audio.tone_vario_cms = vario_audio_apply_lpf(s_vario_audio.tone_vario_cms,
                                                             (float)raw_vario_cms,
                                                             dt_ms,
                                                             vario_audio_tone_tau_ms(settings));
    }

    gate_vario_cms = (int16_t)((s_vario_audio.gate_vario_cms >= 0.0f) ?
                               (s_vario_audio.gate_vario_cms + 0.5f) :
                               (s_vario_audio.gate_vario_cms - 0.5f));
    tone_vario_cms = (int16_t)((s_vario_audio.tone_vario_cms >= 0.0f) ?
                               (s_vario_audio.tone_vario_cms + 0.5f) :
                               (s_vario_audio.tone_vario_cms - 0.5f));

    next_mode = vario_audio_pick_mode(settings,
                                      gate_vario_cms,
                                      s_vario_audio.mode);

    if (next_mode != s_vario_audio.mode)
    {
        /* ------------------------------------------------------------------ */
        /*  mode가 바뀌는 순간에는 old smoothing tail 때문에                   */
        /*  첫 chirp가 눌리지 않도록 raw 값을 더 강하게 반영한다.              */
        /* ------------------------------------------------------------------ */
        s_vario_audio.gate_vario_cms = (float)raw_vario_cms;
        s_vario_audio.tone_vario_cms = (float)raw_vario_cms;
        gate_vario_cms               = raw_vario_cms;
        tone_vario_cms               = raw_vario_cms;
        s_vario_audio.pulse_on       = false;
        s_vario_audio.next_cycle_ms  = 0u;
        s_vario_audio.pulse_off_ms   = 0u;

        if ((s_vario_audio.mode != VARIO_AUDIO_MODE_IDLE) &&
            (next_mode != VARIO_AUDIO_MODE_IDLE))
        {
            /* -------------------------------------------------------------- */
            /*  climb <-> sink / chirp <-> continuous 전환에서는               */
            /*  파형이 바뀔 수 있으므로 한 번 release를 걸어 artefact를 줄인다. */
            /* -------------------------------------------------------------- */
            vario_audio_stop_driver(settings);
        }

        s_vario_audio.mode = next_mode;
    }

    if (next_mode == VARIO_AUDIO_MODE_IDLE)
    {
        vario_audio_stop_driver(settings);
        s_vario_audio.last_task_ms = now_ms;
        return;
    }

    if (next_mode == VARIO_AUDIO_MODE_CLIMB)
    {
        vario_audio_build_climb_target(settings,
                                       tone_vario_cms,
                                       &target);
    }
    else if (next_mode == VARIO_AUDIO_MODE_SINK_CHIRP)
    {
        vario_audio_build_sink_chirp_target(settings,
                                            tone_vario_cms,
                                            &target);
    }
    else
    {
        vario_audio_build_sink_continuous_target(settings,
                                                 tone_vario_cms,
                                                 &target);
    }

    if (target.pulsed != false)
    {
        vario_audio_run_pulsed_mode(settings,
                                    &target,
                                    now_ms);
    }
    else
    {
        vario_audio_run_continuous_mode(settings,
                                        &target);
    }

    s_vario_audio.last_task_ms = now_ms;
}
