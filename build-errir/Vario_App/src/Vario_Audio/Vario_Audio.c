#include "Vario_Audio.h"

#include "Audio_App.h"
#include "Vario_Settings.h"
#include "Vario_State.h"

#include <math.h>
#include <stdbool.h>
#include <stddef.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*  제품용 variometer audio engine                                             */
/*                                                                            */
/*  설계 원칙                                                                  */
/*  1) 오디오 입력은 APP_ALTITUDE fast vario 경로를 그대로 사용한다.          */
/*     즉, app-layer second LPF를 다시 거치지 않는다.                         */
/*  2) climb / pre-thermal의 단속음은 "노트 재시작" 이 아니라                */
/*     같은 oscillator의 level envelope 로 만든다.                           */
/*  3) pitch law / cadence law / duty law / level law 를 분리한다.           */
/*  4) display response 와 audio response 를 분리한다.                       */
/*     여기서는 settings->audio_response_level 만 사용한다.                  */
/*  5) 저수준 transport는 반드시 Audio_App façade를 통해서만 사용한다.       */
/*                                                                            */
/*  청감상 효과                                                                */
/*  - 상승음 주기는 상승률에 따라 변한다.                                     */
/*  - 그러나 개별 beep 안에서도 pitch는 현재 fast vario를 계속 따라간다.      */
/*  - 따라서 "비프 하나당 노트 하나" 인상이 줄고, 연속적으로 미끄러지는       */
/*    commercial variometer 느낌에 가까워진다.                                */
/* -------------------------------------------------------------------------- */

typedef enum
{
    VARIO_AUDIO_MODE_IDLE = 0u,
    VARIO_AUDIO_MODE_PRETHERMAL,
    VARIO_AUDIO_MODE_CLIMB,
    VARIO_AUDIO_MODE_SINK_CHIRP,
    VARIO_AUDIO_MODE_SINK_CONTINUOUS
} vario_audio_mode_t;

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
    uint16_t climb_base_hz_default;
    uint16_t climb_top_delta_hz;
    uint16_t climb_period_start_ms;
    uint16_t climb_period_end_ms;
    uint16_t climb_duty_start_permille;
    uint16_t climb_duty_end_permille;
    uint16_t climb_level_start_permille;
    uint16_t climb_level_end_permille;
    uint16_t climb_span_cms;

    uint16_t prethermal_low_hz;
    uint16_t prethermal_high_hz;
    uint16_t prethermal_period_start_ms;
    uint16_t prethermal_period_end_ms;
    uint16_t prethermal_on_start_ms;
    uint16_t prethermal_on_end_ms;
    uint16_t prethermal_level_start_permille;
    uint16_t prethermal_level_end_permille;

    uint16_t sink_chirp_high_hz;
    uint16_t sink_chirp_low_hz;
    uint16_t sink_chirp_period_start_ms;
    uint16_t sink_chirp_period_end_ms;
    uint16_t sink_chirp_on_start_ms;
    uint16_t sink_chirp_on_end_ms;
    uint16_t sink_chirp_level_start_permille;
    uint16_t sink_chirp_level_end_permille;

    uint16_t sink_cont_high_hz;
    uint16_t sink_cont_low_hz;
    uint16_t sink_cont_level_start_permille;
    uint16_t sink_cont_level_end_permille;
    uint16_t sink_cont_extra_span_cms;
} vario_audio_profile_params_t;

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
    uint16_t             current_freq_hz;
    uint16_t             current_level_permille;
} vario_audio_runtime_t;

static vario_audio_runtime_t s_vario_audio;
static uint8_t               s_vario_audio_last_applied_volume = 0xFFu;

static uint8_t vario_audio_clamp_u8(uint8_t value, uint8_t min_v, uint8_t max_v)
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

static float vario_audio_absf(float value)
{
    return (value < 0.0f) ? -value : value;
}

static bool vario_audio_time_reached(uint32_t now_ms, uint32_t deadline_ms)
{
    return ((int32_t)(now_ms - deadline_ms) >= 0) ? true : false;
}

static bool vario_audio_mode_owns_tone(vario_mode_t mode)
{
    /* ---------------------------------------------------------------------- */
    /* commercial variometer 튜닝 흐름을 따라 settings / quickset 화면에서도   */
    /* 실시간으로 톤을 들어볼 수 있게, mode 소유권은 거의 항상 true로 둔다.   */
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

    response_level = vario_audio_clamp_u8(settings->audio_response_level, 1u, 10u);
    return ((float)(response_level - 1u)) / 9.0f;
}

static uint16_t vario_audio_attack_glide_ms(const vario_settings_t *settings)
{
    return vario_audio_lerp_u16(42u, 10u, vario_audio_response_norm(settings));
}

static uint16_t vario_audio_release_glide_ms(const vario_settings_t *settings)
{
    return vario_audio_lerp_u16(96u, 18u, vario_audio_response_norm(settings));
}

static uint16_t vario_audio_silent_track_glide_ms(const vario_settings_t *settings)
{
    return vario_audio_lerp_u16(72u, 14u, vario_audio_response_norm(settings));
}

static uint16_t vario_audio_continuous_glide_ms(const vario_settings_t *settings)
{
    return vario_audio_lerp_u16(32u, 8u, vario_audio_response_norm(settings));
}

static int16_t vario_audio_get_climb_on_threshold_cms(const vario_settings_t *settings)
{
    if (settings == NULL)
    {
        return 15;
    }

    return vario_audio_clamp_s16(settings->climb_tone_threshold_cms, 0, 300);
}

static int16_t vario_audio_get_climb_off_threshold_cms(const vario_settings_t *settings)
{
    int16_t climb_on;
    int16_t climb_off;

    climb_on = vario_audio_get_climb_on_threshold_cms(settings);
    if (settings == NULL)
    {
        return 7;
    }

    climb_off = vario_audio_clamp_s16(settings->climb_tone_off_threshold_cms, 0, 300);
    if (climb_off > climb_on)
    {
        climb_off = climb_on;
    }

    return climb_off;
}

static int16_t vario_audio_get_prethermal_on_threshold_cms(const vario_settings_t *settings)
{
    int16_t climb_on;
    int16_t pre_on;

    climb_on = vario_audio_get_climb_on_threshold_cms(settings);
    if (settings == NULL)
    {
        return 5;
    }

    /* ------------------------------------------------------------------ */
    /*  prethermal 은 Flytec 계열의 pre-zero / near-thermal 감각을 살리기 위해 */
    /*  음수 문턱도 그대로 허용한다.                                           */
    /*                                                                      */
    /*  Settings layer가 이미 -100..+300 cm/s 계약을 제공하므로,               */
    /*  audio state machine도 같은 의미를 따라야 한다.                       */
    /*  이전 구현처럼 0 이상으로 재-clamp 하면 menu에서 준 음수 prethermal이  */
    /*  실제 소리 판단에서는 사라지는 semantic mismatch가 생긴다.             */
    /* ------------------------------------------------------------------ */
    pre_on = vario_audio_clamp_s16(settings->prethermal_threshold_cms, -100, 300);
    if (pre_on > climb_on)
    {
        pre_on = climb_on;
    }

    return pre_on;
}

static int16_t vario_audio_get_prethermal_off_threshold_cms(const vario_settings_t *settings)
{
    int16_t pre_on;
    int16_t pre_off;

    pre_on = vario_audio_get_prethermal_on_threshold_cms(settings);
    if (settings == NULL)
    {
        return 0;
    }

    pre_off = vario_audio_clamp_s16(settings->prethermal_off_threshold_cms, -100, 300);
    if (pre_off > pre_on)
    {
        pre_off = pre_on;
    }

    return pre_off;
}

static int16_t vario_audio_get_sink_on_threshold_cms(const vario_settings_t *settings)
{
    if (settings == NULL)
    {
        return -80;
    }

    return vario_audio_clamp_s16(settings->sink_tone_threshold_cms, -600, 0);
}

static int16_t vario_audio_get_sink_off_threshold_cms(const vario_settings_t *settings)
{
    int16_t sink_on;
    int16_t sink_off;

    sink_on = vario_audio_get_sink_on_threshold_cms(settings);
    if (settings == NULL)
    {
        return -60;
    }

    sink_off = vario_audio_clamp_s16(settings->sink_tone_off_threshold_cms, -600, 0);
    if (sink_off < sink_on)
    {
        sink_off = sink_on;
    }

    return sink_off;
}

static int16_t vario_audio_get_sink_cont_on_threshold_cms(const vario_settings_t *settings)
{
    int16_t sink_on;
    int16_t sink_cont_on;

    sink_on = vario_audio_get_sink_on_threshold_cms(settings);
    if (settings == NULL)
    {
        return -180;
    }

    sink_cont_on = vario_audio_clamp_s16(settings->sink_continuous_threshold_cms, -900, 0);
    if (sink_cont_on > sink_on)
    {
        sink_cont_on = sink_on;
    }

    return sink_cont_on;
}

static int16_t vario_audio_get_sink_cont_off_threshold_cms(const vario_settings_t *settings)
{
    int16_t sink_cont_on;
    int16_t sink_cont_off;

    sink_cont_on = vario_audio_get_sink_cont_on_threshold_cms(settings);
    if (settings == NULL)
    {
        return -150;
    }

    sink_cont_off = vario_audio_clamp_s16(settings->sink_continuous_off_threshold_cms, -900, 0);
    if (sink_cont_off < sink_cont_on)
    {
        sink_cont_off = sink_cont_on;
    }

    return sink_cont_off;
}

static float vario_audio_pitch_curve_exponent(const vario_settings_t *settings)
{
    float percent;

    if (settings == NULL)
    {
        return 1.0f;
    }

    percent = (float)vario_audio_clamp_u8(settings->audio_pitch_curve_percent, 50u, 150u);
    return 0.55f + ((percent - 50.0f) * (1.00f / 100.0f));
}

static float vario_audio_shape_progress(const vario_settings_t *settings, float progress)
{
    float exponent;

    progress = vario_audio_clampf(progress, 0.0f, 1.0f);
    exponent = vario_audio_pitch_curve_exponent(settings);

    if (progress <= 0.0f)
    {
        return 0.0f;
    }

    if (progress >= 1.0f)
    {
        return 1.0f;
    }

    return powf(progress, exponent);
}

static uint16_t vario_audio_get_climb_base_hz(const vario_settings_t *settings,
                                              const vario_audio_profile_params_t *params)
{
    uint16_t base_hz;

    if (params == NULL)
    {
        return 700u;
    }

    base_hz = params->climb_base_hz_default;
    if ((settings != NULL) && (settings->audio_up_base_hz != 0u))
    {
        base_hz = settings->audio_up_base_hz;
    }

    return vario_audio_clamp_u16(base_hz, 350u, 1800u);
}

static uint16_t vario_audio_get_climb_top_delta_hz(const vario_settings_t *settings,
                                                   const vario_audio_profile_params_t *params)
{
    float depth_percent;
    float delta_hz;

    if (params == NULL)
    {
        return 800u;
    }

    depth_percent = 100.0f;
    if (settings != NULL)
    {
        depth_percent = (float)vario_audio_clamp_u8(settings->audio_modulation_depth_percent,
                                                    50u,
                                                    150u);
    }

    delta_hz = ((float)params->climb_top_delta_hz) * (depth_percent / 100.0f);
    if (delta_hz < 120.0f)
    {
        delta_hz = 120.0f;
    }

    return (uint16_t)(delta_hz + 0.5f);
}

static void vario_audio_get_profile_params(vario_audio_profile_t profile,
                                           vario_audio_profile_params_t *out_params)
{
    if (out_params == NULL)
    {
        return;
    }

    memset(out_params, 0, sizeof(*out_params));

    switch (profile)
    {
        case VARIO_AUDIO_PROFILE_FLYTEC_CLASSIC:
            out_params->climb_base_hz_default       = 720u;
            out_params->climb_top_delta_hz          = 920u;
            out_params->climb_period_start_ms       = 330u;
            out_params->climb_period_end_ms         = 62u;
            out_params->climb_duty_start_permille   = 300u;
            out_params->climb_duty_end_permille     = 760u;
            out_params->climb_level_start_permille  = 360u;
            out_params->climb_level_end_permille    = 860u;
            out_params->climb_span_cms              = 600u;

            out_params->prethermal_low_hz           = 150u;
            out_params->prethermal_high_hz          = 560u;
            out_params->prethermal_period_start_ms  = 130u;
            out_params->prethermal_period_end_ms    = 82u;
            out_params->prethermal_on_start_ms      = 26u;
            out_params->prethermal_on_end_ms        = 42u;
            out_params->prethermal_level_start_permille = 220u;
            out_params->prethermal_level_end_permille   = 420u;

            out_params->sink_chirp_high_hz          = 400u;
            out_params->sink_chirp_low_hz           = 265u;
            out_params->sink_chirp_period_start_ms  = 680u;
            out_params->sink_chirp_period_end_ms    = 240u;
            out_params->sink_chirp_on_start_ms      = 44u;
            out_params->sink_chirp_on_end_ms        = 86u;
            out_params->sink_chirp_level_start_permille = 260u;
            out_params->sink_chirp_level_end_permille   = 540u;

            out_params->sink_cont_high_hz           = 305u;
            out_params->sink_cont_low_hz            = 165u;
            out_params->sink_cont_level_start_permille = 430u;
            out_params->sink_cont_level_end_permille   = 780u;
            out_params->sink_cont_extra_span_cms    = 420u;
            break;

        case VARIO_AUDIO_PROFILE_BLUEFLY_SMOOTH:
            out_params->climb_base_hz_default       = 690u;
            out_params->climb_top_delta_hz          = 780u;
            out_params->climb_period_start_ms       = 420u;
            out_params->climb_period_end_ms         = 72u;
            out_params->climb_duty_start_permille   = 360u;
            out_params->climb_duty_end_permille     = 780u;
            out_params->climb_level_start_permille  = 280u;
            out_params->climb_level_end_permille    = 740u;
            out_params->climb_span_cms              = 650u;

            out_params->prethermal_low_hz           = 220u;
            out_params->prethermal_high_hz          = 500u;
            out_params->prethermal_period_start_ms  = 220u;
            out_params->prethermal_period_end_ms    = 145u;
            out_params->prethermal_on_start_ms      = 62u;
            out_params->prethermal_on_end_ms        = 88u;
            out_params->prethermal_level_start_permille = 140u;
            out_params->prethermal_level_end_permille   = 250u;

            out_params->sink_chirp_high_hz          = 450u;
            out_params->sink_chirp_low_hz           = 320u;
            out_params->sink_chirp_period_start_ms  = 780u;
            out_params->sink_chirp_period_end_ms    = 340u;
            out_params->sink_chirp_on_start_ms      = 36u;
            out_params->sink_chirp_on_end_ms        = 64u;
            out_params->sink_chirp_level_start_permille = 220u;
            out_params->sink_chirp_level_end_permille   = 430u;

            out_params->sink_cont_high_hz           = 340u;
            out_params->sink_cont_low_hz            = 220u;
            out_params->sink_cont_level_start_permille = 320u;
            out_params->sink_cont_level_end_permille   = 620u;
            out_params->sink_cont_extra_span_cms    = 480u;
            break;

        case VARIO_AUDIO_PROFILE_DIGIFLY_DG:
            out_params->climb_base_hz_default       = 705u;
            out_params->climb_top_delta_hz          = 860u;
            out_params->climb_period_start_ms       = 350u;
            out_params->climb_period_end_ms         = 74u;
            out_params->climb_duty_start_permille   = 340u;
            out_params->climb_duty_end_permille     = 740u;
            out_params->climb_level_start_permille  = 320u;
            out_params->climb_level_end_permille    = 800u;
            out_params->climb_span_cms              = 620u;

            out_params->prethermal_low_hz           = 170u;
            out_params->prethermal_high_hz          = 460u;
            out_params->prethermal_period_start_ms  = 180u;
            out_params->prethermal_period_end_ms    = 120u;
            out_params->prethermal_on_start_ms      = 40u;
            out_params->prethermal_on_end_ms        = 70u;
            out_params->prethermal_level_start_permille = 180u;
            out_params->prethermal_level_end_permille   = 310u;

            out_params->sink_chirp_high_hz          = 430u;
            out_params->sink_chirp_low_hz           = 280u;
            out_params->sink_chirp_period_start_ms  = 760u;
            out_params->sink_chirp_period_end_ms    = 220u;
            out_params->sink_chirp_on_start_ms      = 36u;
            out_params->sink_chirp_on_end_ms        = 76u;
            out_params->sink_chirp_level_start_permille = 280u;
            out_params->sink_chirp_level_end_permille   = 560u;

            out_params->sink_cont_high_hz           = 300u;
            out_params->sink_cont_low_hz            = 190u;
            out_params->sink_cont_level_start_permille = 410u;
            out_params->sink_cont_level_end_permille   = 740u;
            out_params->sink_cont_extra_span_cms    = 440u;
            break;

        case VARIO_AUDIO_PROFILE_SOFT_SPEAKER:
        case VARIO_AUDIO_PROFILE_COUNT:
        default:
            out_params->climb_base_hz_default       = 700u;
            out_params->climb_top_delta_hz          = 740u;
            out_params->climb_period_start_ms       = 380u;
            out_params->climb_period_end_ms         = 80u;
            out_params->climb_duty_start_permille   = 320u;
            out_params->climb_duty_end_permille     = 720u;
            out_params->climb_level_start_permille  = 300u;
            out_params->climb_level_end_permille    = 760u;
            out_params->climb_span_cms              = 620u;

            out_params->prethermal_low_hz           = 190u;
            out_params->prethermal_high_hz          = 470u;
            out_params->prethermal_period_start_ms  = 190u;
            out_params->prethermal_period_end_ms    = 120u;
            out_params->prethermal_on_start_ms      = 52u;
            out_params->prethermal_on_end_ms        = 78u;
            out_params->prethermal_level_start_permille = 160u;
            out_params->prethermal_level_end_permille   = 280u;

            out_params->sink_chirp_high_hz          = 420u;
            out_params->sink_chirp_low_hz           = 295u;
            out_params->sink_chirp_period_start_ms  = 760u;
            out_params->sink_chirp_period_end_ms    = 260u;
            out_params->sink_chirp_on_start_ms      = 40u;
            out_params->sink_chirp_on_end_ms        = 72u;
            out_params->sink_chirp_level_start_permille = 240u;
            out_params->sink_chirp_level_end_permille   = 500u;

            out_params->sink_cont_high_hz           = 320u;
            out_params->sink_cont_low_hz            = 180u;
            out_params->sink_cont_level_start_permille = 360u;
            out_params->sink_cont_level_end_permille   = 700u;
            out_params->sink_cont_extra_span_cms    = 460u;
            break;
    }
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
                                                float vario_cms,
                                                vario_audio_mode_t current_mode)
{
    const int16_t climb_on      = vario_audio_get_climb_on_threshold_cms(settings);
    const int16_t climb_off     = vario_audio_get_climb_off_threshold_cms(settings);
    const int16_t pre_on        = vario_audio_get_prethermal_on_threshold_cms(settings);
    const int16_t pre_off       = vario_audio_get_prethermal_off_threshold_cms(settings);
    const int16_t sink_on       = vario_audio_get_sink_on_threshold_cms(settings);
    const int16_t sink_off      = vario_audio_get_sink_off_threshold_cms(settings);
    const int16_t sink_cont_on  = vario_audio_get_sink_cont_on_threshold_cms(settings);
    const int16_t sink_cont_off = vario_audio_get_sink_cont_off_threshold_cms(settings);
    const bool    prethermal_enabled = ((settings != NULL) &&
                                        (settings->prethermal_mode != VARIO_PRETHERMAL_MODE_OFF));

    switch (current_mode)
    {
        case VARIO_AUDIO_MODE_CLIMB:
            if (vario_cms >= (float)climb_off)
            {
                return VARIO_AUDIO_MODE_CLIMB;
            }
            break;

        case VARIO_AUDIO_MODE_PRETHERMAL:
            if (prethermal_enabled != false)
            {
                if (vario_cms >= (float)climb_on)
                {
                    return VARIO_AUDIO_MODE_CLIMB;
                }

                if ((vario_cms >= (float)pre_off) &&
                    (vario_cms < (float)climb_on))
                {
                    return VARIO_AUDIO_MODE_PRETHERMAL;
                }
            }
            break;

        case VARIO_AUDIO_MODE_SINK_CONTINUOUS:
            if (vario_cms <= (float)sink_cont_off)
            {
                return VARIO_AUDIO_MODE_SINK_CONTINUOUS;
            }
            break;

        case VARIO_AUDIO_MODE_SINK_CHIRP:
            if (vario_cms <= (float)sink_cont_on)
            {
                return VARIO_AUDIO_MODE_SINK_CONTINUOUS;
            }

            if ((vario_cms <= (float)sink_off) &&
                (vario_cms > (float)sink_cont_on))
            {
                return VARIO_AUDIO_MODE_SINK_CHIRP;
            }
            break;

        case VARIO_AUDIO_MODE_IDLE:
        default:
            break;
    }

    if (vario_cms >= (float)climb_on)
    {
        return VARIO_AUDIO_MODE_CLIMB;
    }

    if ((prethermal_enabled != false) &&
        (vario_cms >= (float)pre_on) &&
        (vario_cms < (float)climb_on))
    {
        return VARIO_AUDIO_MODE_PRETHERMAL;
    }

    if (vario_cms <= (float)sink_cont_on)
    {
        return VARIO_AUDIO_MODE_SINK_CONTINUOUS;
    }

    if (vario_cms <= (float)sink_on)
    {
        return VARIO_AUDIO_MODE_SINK_CHIRP;
    }

    return VARIO_AUDIO_MODE_IDLE;
}

static void vario_audio_build_climb_target(const vario_settings_t *settings,
                                           const vario_audio_profile_params_t *params,
                                           float vario_cms,
                                           vario_audio_target_t *out_target)
{
    const int16_t climb_on = vario_audio_get_climb_on_threshold_cms(settings);
    const uint16_t base_hz = vario_audio_get_climb_base_hz(settings, params);
    const uint16_t top_delta_hz = vario_audio_get_climb_top_delta_hz(settings, params);
    float progress;
    float shaped_progress;
    uint16_t duty_permille;

    if ((params == NULL) || (out_target == NULL))
    {
        return;
    }

    progress = (vario_cms - (float)climb_on) / (float)params->climb_span_cms;
    progress = vario_audio_clampf(progress, 0.0f, 1.0f);
    shaped_progress = vario_audio_shape_progress(settings, progress);

    duty_permille = vario_audio_lerp_u16(params->climb_duty_start_permille,
                                         params->climb_duty_end_permille,
                                         progress);

    out_target->waveform = APP_AUDIO_WAVEFORM_SINE;
    out_target->freq_hz = vario_audio_lerp_u16(base_hz,
                                               (uint16_t)(base_hz + top_delta_hz),
                                               shaped_progress);
    out_target->level_permille = vario_audio_lerp_u16(params->climb_level_start_permille,
                                                      params->climb_level_end_permille,
                                                      progress);
    out_target->period_ms = vario_audio_lerp_u16(params->climb_period_start_ms,
                                                 params->climb_period_end_ms,
                                                 progress);
    out_target->on_ms = (uint16_t)(((uint32_t)out_target->period_ms * (uint32_t)duty_permille) / 1000u);
    out_target->on_ms = vario_audio_clamp_u16(out_target->on_ms,
                                              28u,
                                              (out_target->period_ms > 10u) ?
                                                  (uint16_t)(out_target->period_ms - 10u) :
                                                  out_target->period_ms);
    out_target->pulsed = true;
}

static void vario_audio_build_prethermal_target(const vario_settings_t *settings,
                                                const vario_audio_profile_params_t *params,
                                                float vario_cms,
                                                vario_audio_target_t *out_target)
{
    const int16_t climb_on = vario_audio_get_climb_on_threshold_cms(settings);
    const int16_t pre_on = vario_audio_get_prethermal_on_threshold_cms(settings);
    const int16_t span_cms = (climb_on > pre_on) ? (climb_on - pre_on) : 5;
    const bool    soft_pulse = ((settings != NULL) &&
                                (settings->prethermal_mode == VARIO_PRETHERMAL_MODE_SOFT_PULSE));
    float progress;
    float shaped_progress;
    uint16_t period_ms;
    uint16_t on_ms;
    uint16_t level_permille;
    uint16_t freq_hz;

    if ((params == NULL) || (out_target == NULL))
    {
        return;
    }

    progress = (vario_cms - (float)pre_on) / (float)span_cms;
    progress = vario_audio_clampf(progress, 0.0f, 1.0f);
    shaped_progress = vario_audio_shape_progress(settings, progress);

    freq_hz = vario_audio_lerp_u16(params->prethermal_low_hz,
                                   params->prethermal_high_hz,
                                   shaped_progress);
    period_ms = vario_audio_lerp_u16(params->prethermal_period_start_ms,
                                     params->prethermal_period_end_ms,
                                     progress);
    on_ms = vario_audio_lerp_u16(params->prethermal_on_start_ms,
                                 params->prethermal_on_end_ms,
                                 progress);
    level_permille = vario_audio_lerp_u16(params->prethermal_level_start_permille,
                                          params->prethermal_level_end_permille,
                                          progress);

    if (soft_pulse != false)
    {
        freq_hz = vario_audio_lerp_u16(freq_hz,
                                       vario_audio_get_climb_base_hz(settings, params),
                                       0.35f);
        period_ms = (uint16_t)(period_ms + 55u);
        on_ms = (uint16_t)(on_ms + 24u);
        level_permille = (uint16_t)((level_permille * 78u) / 100u);
    }

    out_target->waveform = APP_AUDIO_WAVEFORM_SINE;
    out_target->freq_hz = freq_hz;
    out_target->level_permille = level_permille;
    out_target->period_ms = vario_audio_clamp_u16(period_ms, 60u, 320u);
    out_target->on_ms = vario_audio_clamp_u16(on_ms,
                                              18u,
                                              (out_target->period_ms > 8u) ?
                                                  (uint16_t)(out_target->period_ms - 8u) :
                                                  out_target->period_ms);
    out_target->pulsed = true;
}

static void vario_audio_build_sink_chirp_target(const vario_settings_t *settings,
                                                const vario_audio_profile_params_t *params,
                                                float vario_cms,
                                                vario_audio_target_t *out_target)
{
    const int16_t sink_on = vario_audio_get_sink_on_threshold_cms(settings);
    const int16_t sink_cont_on = vario_audio_get_sink_cont_on_threshold_cms(settings);
    const float sink_on_abs = vario_audio_absf((float)sink_on);
    const float sink_cont_abs = vario_audio_absf((float)sink_cont_on);
    float band_span_abs;
    float progress;
    float shaped_progress;
    float sink_abs;

    if ((params == NULL) || (out_target == NULL))
    {
        return;
    }

    sink_abs = vario_audio_absf((vario_cms < 0.0f) ? vario_cms : 0.0f);
    band_span_abs = sink_cont_abs - sink_on_abs;
    if (band_span_abs < 25.0f)
    {
        band_span_abs = 25.0f;
    }

    progress = (sink_abs - sink_on_abs) / band_span_abs;
    progress = vario_audio_clampf(progress, 0.0f, 1.0f);
    shaped_progress = vario_audio_shape_progress(settings, progress);

    out_target->waveform = APP_AUDIO_WAVEFORM_SINE;
    out_target->freq_hz = vario_audio_lerp_u16(params->sink_chirp_high_hz,
                                               params->sink_chirp_low_hz,
                                               shaped_progress);
    out_target->level_permille = vario_audio_lerp_u16(params->sink_chirp_level_start_permille,
                                                      params->sink_chirp_level_end_permille,
                                                      progress);
    out_target->period_ms = vario_audio_lerp_u16(params->sink_chirp_period_start_ms,
                                                 params->sink_chirp_period_end_ms,
                                                 progress);
    out_target->on_ms = vario_audio_lerp_u16(params->sink_chirp_on_start_ms,
                                             params->sink_chirp_on_end_ms,
                                             progress);
    out_target->on_ms = vario_audio_clamp_u16(out_target->on_ms,
                                              20u,
                                              (out_target->period_ms > 12u) ?
                                                  (uint16_t)(out_target->period_ms - 12u) :
                                                  out_target->period_ms);
    out_target->pulsed = true;
}

static void vario_audio_build_sink_continuous_target(const vario_settings_t *settings,
                                                     const vario_audio_profile_params_t *params,
                                                     float vario_cms,
                                                     vario_audio_target_t *out_target)
{
    const int16_t sink_cont_on = vario_audio_get_sink_cont_on_threshold_cms(settings);
    const float sink_cont_abs = vario_audio_absf((float)sink_cont_on);
    float sink_abs;
    float progress;
    float shaped_progress;

    if ((params == NULL) || (out_target == NULL))
    {
        return;
    }

    sink_abs = vario_audio_absf((vario_cms < 0.0f) ? vario_cms : 0.0f);
    progress = (sink_abs - sink_cont_abs) / (float)params->sink_cont_extra_span_cms;
    progress = vario_audio_clampf(progress, 0.0f, 1.0f);
    shaped_progress = vario_audio_shape_progress(settings, progress);

    out_target->waveform = APP_AUDIO_WAVEFORM_SINE;
    out_target->freq_hz = vario_audio_lerp_u16(params->sink_cont_high_hz,
                                               params->sink_cont_low_hz,
                                               shaped_progress);
    out_target->level_permille = vario_audio_lerp_u16(params->sink_cont_level_start_permille,
                                                      params->sink_cont_level_end_permille,
                                                      progress);
    out_target->period_ms = 0u;
    out_target->on_ms = 0u;
    out_target->pulsed = false;
}

static void vario_audio_stop_driver(const vario_settings_t *settings)
{
    if ((s_vario_audio.driver_active != false) || (Audio_App_IsVariometerActive() != false))
    {
        (void)Audio_App_VariometerStop(vario_audio_release_glide_ms(settings));
    }

    s_vario_audio.driver_active = false;
    s_vario_audio.pulse_on = false;
    s_vario_audio.current_freq_hz = 0u;
    s_vario_audio.current_level_permille = 0u;
    s_vario_audio.waveform = APP_AUDIO_WAVEFORM_NONE;
}

static void vario_audio_ensure_driver_started(const vario_audio_target_t *target)
{
    uint32_t start_freq;

    if (target == NULL)
    {
        return;
    }

    if ((s_vario_audio.driver_active != false) &&
        (Audio_App_IsVariometerActive() != false) &&
        (s_vario_audio.waveform == target->waveform))
    {
        return;
    }

    start_freq = (target->freq_hz != 0u) ? (uint32_t)target->freq_hz : 440u;
    (void)Audio_App_VariometerStart(target->waveform,
                                    start_freq,
                                    0u);
    s_vario_audio.driver_active = true;
    s_vario_audio.waveform = target->waveform;
}

static void vario_audio_apply_target(const vario_settings_t *settings,
                                     const vario_audio_target_t *target,
                                     uint16_t level_permille,
                                     uint16_t glide_ms)
{
    if ((settings == NULL) || (target == NULL))
    {
        return;
    }

    vario_audio_ensure_driver_started(target);
    (void)Audio_App_VariometerSetTarget(target->freq_hz,
                                        level_permille,
                                        glide_ms);

    s_vario_audio.current_freq_hz = target->freq_hz;
    s_vario_audio.current_level_permille = level_permille;
}

static void vario_audio_run_pulsed_mode(const vario_settings_t *settings,
                                        const vario_audio_target_t *target,
                                        uint32_t now_ms)
{
    uint16_t attack_ms;
    uint16_t release_ms;
    uint16_t silent_track_ms;
    uint32_t latest_allowed_cycle_ms;

    if ((settings == NULL) || (target == NULL))
    {
        return;
    }

    attack_ms = vario_audio_attack_glide_ms(settings);
    release_ms = vario_audio_release_glide_ms(settings);
    silent_track_ms = vario_audio_silent_track_glide_ms(settings);

    if (s_vario_audio.pulse_on == false)
    {
        if (s_vario_audio.next_cycle_ms == 0u)
        {
            s_vario_audio.next_cycle_ms = now_ms;
        }
        else
        {
            latest_allowed_cycle_ms = now_ms + target->period_ms;
            if ((int32_t)(s_vario_audio.next_cycle_ms - latest_allowed_cycle_ms) > 0)
            {
                s_vario_audio.next_cycle_ms = latest_allowed_cycle_ms;
            }
        }
    }

    if ((s_vario_audio.pulse_on == false) &&
        (vario_audio_time_reached(now_ms, s_vario_audio.next_cycle_ms) != false))
    {
        s_vario_audio.pulse_on = true;
        s_vario_audio.pulse_off_ms = now_ms + target->on_ms;
        s_vario_audio.next_cycle_ms = now_ms + target->period_ms;
        vario_audio_apply_target(settings,
                                 target,
                                 target->level_permille,
                                 attack_ms);
        return;
    }

    if (s_vario_audio.pulse_on != false)
    {
        if (vario_audio_time_reached(now_ms, s_vario_audio.pulse_off_ms) != false)
        {
            s_vario_audio.pulse_on = false;
            vario_audio_apply_target(settings,
                                     target,
                                     0u,
                                     release_ms);
            return;
        }

        /* ------------------------------------------------------------------ */
        /* 같은 beep 안에서도 pitch는 현재 fast vario를 계속 따라가게 둔다.    */
        /* 즉, cadence law와 pitch law를 분리하고 intra-beep dynamic frequency */
        /* 를 유지한다.                                                        */
        /* ------------------------------------------------------------------ */
        vario_audio_apply_target(settings,
                                 target,
                                 target->level_permille,
                                 attack_ms);
        return;
    }

    /* ---------------------------------------------------------------------- */
    /* off 구간에서도 oscillator는 살아 있고 level만 0으로 유지한다.           */
    /*                                                                        */
    /* 이렇게 해야 다음 beep가 oscillator 재시작 note처럼 들리지 않고,        */
    /* pitch도 무음 상태에서 미리 현재 vario 쪽으로 미끄러져 있을 수 있다.      */
    /* ---------------------------------------------------------------------- */
    vario_audio_apply_target(settings,
                             target,
                             0u,
                             silent_track_ms);
}

static void vario_audio_run_continuous_mode(const vario_settings_t *settings,
                                            const vario_audio_target_t *target)
{
    if ((settings == NULL) || (target == NULL))
    {
        return;
    }

    s_vario_audio.pulse_on = false;
    s_vario_audio.next_cycle_ms = 0u;
    s_vario_audio.pulse_off_ms = 0u;

    vario_audio_apply_target(settings,
                             target,
                             target->level_permille,
                             vario_audio_continuous_glide_ms(settings));
}

void Vario_Audio_Init(void)
{
    const vario_settings_t *settings;
    uint8_t                 volume_percent;

    settings = Vario_Settings_Get();
    volume_percent = (settings != NULL) ?
        vario_audio_clamp_u8(settings->audio_volume_percent, 0u, 100u) : 75u;

    memset(&s_vario_audio, 0, sizeof(s_vario_audio));
    s_vario_audio.mode = VARIO_AUDIO_MODE_IDLE;
    s_vario_audio.waveform = APP_AUDIO_WAVEFORM_NONE;

    Audio_App_SetVolumePercent(volume_percent);
    s_vario_audio_last_applied_volume = volume_percent;

    Audio_App_ReleaseVariometer(0u);
}

void Vario_Audio_Task(uint32_t now_ms)
{
    const vario_settings_t         *settings;
    const vario_runtime_t          *rt;
    vario_mode_t                    mode;
    uint8_t                         volume_percent;
    float                           raw_vario_cms;
    vario_audio_mode_t              next_mode;
    vario_audio_profile_params_t    profile_params;
    vario_audio_target_t            target;

    settings = Vario_Settings_Get();
    rt = Vario_State_GetRuntime();
    mode = Vario_State_GetMode();

    if ((settings == NULL) || (rt == NULL))
    {
        vario_audio_stop_driver(NULL);
        return;
    }

    volume_percent = vario_audio_clamp_u8(settings->audio_volume_percent, 0u, 100u);
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
        s_vario_audio.next_cycle_ms = 0u;
        s_vario_audio.pulse_off_ms = 0u;
        vario_audio_stop_driver(settings);
        s_vario_audio.last_task_ms = now_ms;
        return;
    }

    /* ---------------------------------------------------------------------- */
    /* fast vario를 그대로 audio에 사용한다.                                   */
    /*                                                                        */
    /* 여기서 더 이상 gate/tone용 별도 LPF를 태우지 않는다.                    */
    /* 0.01 m/s 해상도의 cms 스케일로만 바꿔 threshold/law 입력에 사용한다.     */
    /* ---------------------------------------------------------------------- */
    raw_vario_cms = rt->fast_vario_bar_mps * 100.0f;
    raw_vario_cms = vario_audio_clampf(raw_vario_cms, -1200.0f, 1200.0f);

    next_mode = vario_audio_pick_mode(settings,
                                      raw_vario_cms,
                                      s_vario_audio.mode);

    if (next_mode != s_vario_audio.mode)
    {
        s_vario_audio.mode = next_mode;
        s_vario_audio.pulse_on = false;
        s_vario_audio.next_cycle_ms = now_ms;
        s_vario_audio.pulse_off_ms = 0u;

        if (next_mode == VARIO_AUDIO_MODE_IDLE)
        {
            vario_audio_stop_driver(settings);
            s_vario_audio.last_task_ms = now_ms;
            return;
        }
    }

    if (next_mode == VARIO_AUDIO_MODE_IDLE)
    {
        vario_audio_stop_driver(settings);
        s_vario_audio.last_task_ms = now_ms;
        return;
    }

    vario_audio_get_profile_params(settings->audio_profile, &profile_params);
    memset(&target, 0, sizeof(target));

    switch (next_mode)
    {
        case VARIO_AUDIO_MODE_PRETHERMAL:
            vario_audio_build_prethermal_target(settings,
                                                &profile_params,
                                                raw_vario_cms,
                                                &target);
            break;

        case VARIO_AUDIO_MODE_CLIMB:
            vario_audio_build_climb_target(settings,
                                           &profile_params,
                                           raw_vario_cms,
                                           &target);
            break;

        case VARIO_AUDIO_MODE_SINK_CHIRP:
            vario_audio_build_sink_chirp_target(settings,
                                                &profile_params,
                                                raw_vario_cms,
                                                &target);
            break;

        case VARIO_AUDIO_MODE_SINK_CONTINUOUS:
            vario_audio_build_sink_continuous_target(settings,
                                                     &profile_params,
                                                     raw_vario_cms,
                                                     &target);
            break;

        case VARIO_AUDIO_MODE_IDLE:
        default:
            vario_audio_stop_driver(settings);
            s_vario_audio.last_task_ms = now_ms;
            return;
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

    s_vario_audio.initialized = true;
    s_vario_audio.last_task_ms = now_ms;
}
