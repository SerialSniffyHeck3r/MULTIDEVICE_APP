#include "Vario_UiVarioFilter.h"

#include <math.h>
#include <string.h>

#ifndef VARIO_UI_VARIO_FILTER_MIN_DT_S
#define VARIO_UI_VARIO_FILTER_MIN_DT_S 0.010f
#endif

#ifndef VARIO_UI_VARIO_FILTER_MAX_DT_S
#define VARIO_UI_VARIO_FILTER_MAX_DT_S 0.250f
#endif

/* -------------------------------------------------------------------------- */
/*  simple slow-vario display filter                                           */
/*                                                                            */
/*  이 파일은 의도적으로 "복잡한 robust stack" 을 버리고                      */
/*  아주 단순한 1단 display filter 만 남긴다.                                  */
/*                                                                            */
/*  입력                                                                      */
/*  - APP_ALTITUDE 가 이미 만들어 놓은 slow vario                             */
/*                                                                            */
/*  처리 단계                                                                 */
/*  1) 단발성 급변을 막기 위한 input slew clamp                                */
/*  2) single-pole EMA                                                         */
/*  3) zero hysteresis latch                                                   */
/*                                                                            */
/*  의도                                                                      */
/*  - 저수준에서 이미 sane 한 값을 다시 Hampel/MAD/median/adaptive stage 로    */
/*    과잉가공하지 않는다.                                                     */
/*  - 대신 숫자 current vario가 5Hz publish 에서 부드럽게 보일 정도의          */
/*    최소한의 damping 만 남긴다.                                              */
/*                                                                            */
/*  주의                                                                      */
/*  - header struct 안의 ring buffer field 들은 기존 코드와의 source          */
/*    compatibility 를 위해 그대로 둔다.                                      */
/*  - 새 구현은 그 필드들을 표시용 핵심 로직에 사용하지 않는다.                */
/* -------------------------------------------------------------------------- */

static float vario_ui_vario_filter_absf(float value)
{
    return (value < 0.0f) ? -value : value;
}

static float vario_ui_vario_filter_clampf(float value, float min_v, float max_v)
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

static uint8_t vario_ui_vario_filter_clamp_u8(uint8_t value, uint8_t min_v, uint8_t max_v)
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

static float vario_ui_vario_filter_lpf_alpha(float dt_s, float tau_s)
{
    if (tau_s <= 0.0f)
    {
        return 1.0f;
    }

    return dt_s / (tau_s + dt_s);
}

static float vario_ui_vario_filter_compute_dt_s(const vario_ui_vario_filter_t *filter,
                                                uint32_t timestamp_ms)
{
    uint32_t delta_ms;
    float    dt_s;

    if ((filter == NULL) || (filter->initialized == false))
    {
        return 0.050f;
    }

    delta_ms = timestamp_ms - filter->last_update_ms;
    dt_s = ((float)delta_ms) * 0.001f;

    return vario_ui_vario_filter_clampf(dt_s,
                                        VARIO_UI_VARIO_FILTER_MIN_DT_S,
                                        VARIO_UI_VARIO_FILTER_MAX_DT_S);
}

void Vario_UiVarioFilter_Init(vario_ui_vario_filter_t *filter)
{
    if (filter == NULL)
    {
        return;
    }

    memset(filter, 0, sizeof(*filter));
}

void Vario_UiVarioFilter_Reset(vario_ui_vario_filter_t *filter,
                               float input_mps,
                               uint32_t timestamp_ms)
{
    uint8_t i;

    if (filter == NULL)
    {
        return;
    }

    memset(filter, 0, sizeof(*filter));

    input_mps = vario_ui_vario_filter_clampf(input_mps, -20.0f, 20.0f);

    filter->initialized = true;
    filter->last_update_ms = timestamp_ms;
    filter->robust_input_mps = input_mps;
    filter->smoothed_mps = input_mps;
    filter->display_mps = input_mps;
    filter->zero_latched = (vario_ui_vario_filter_absf(input_mps) < 0.05f) ? true : false;

    for (i = 0u; i < VARIO_UI_VARIO_FILTER_INPUT_WINDOW; ++i)
    {
        filter->input_window_mps[i] = input_mps;
    }
    for (i = 0u; i < VARIO_UI_VARIO_FILTER_OUTPUT_WINDOW; ++i)
    {
        filter->output_window_mps[i] = input_mps;
    }

    filter->input_count = VARIO_UI_VARIO_FILTER_INPUT_WINDOW;
    filter->output_count = VARIO_UI_VARIO_FILTER_OUTPUT_WINDOW;
}

float Vario_UiVarioFilter_Update(vario_ui_vario_filter_t *filter,
                                 float input_mps,
                                 uint32_t timestamp_ms,
                                 uint8_t damping_level,
                                 uint8_t average_seconds)
{
    float dt_s;
    float damping_norm;
    float average_norm;
    float delta_input_mps;
    float max_input_step_mps;
    float tau_s;
    float alpha;
    float zero_latch_enter_mps;
    float zero_latch_exit_mps;

    if (filter == NULL)
    {
        return input_mps;
    }

    input_mps = vario_ui_vario_filter_clampf(input_mps, -20.0f, 20.0f);

    if ((filter->initialized == false) || (timestamp_ms == 0u))
    {
        Vario_UiVarioFilter_Reset(filter, input_mps, timestamp_ms);
        return filter->display_mps;
    }

    if (timestamp_ms == filter->last_update_ms)
    {
        return filter->display_mps;
    }

    dt_s = vario_ui_vario_filter_compute_dt_s(filter, timestamp_ms);

    damping_level = vario_ui_vario_filter_clamp_u8(damping_level, 1u, 10u);
    average_seconds = vario_ui_vario_filter_clamp_u8(average_seconds, 1u, 8u);

    damping_norm = ((float)(damping_level - 1u)) / 9.0f;
    average_norm = ((float)(average_seconds - 1u)) / 7.0f;

    /* ---------------------------------------------------------------------- */
    /* input slew clamp                                                        */
    /*                                                                        */
    /* "정상적인 추세 변화"는 그대로 따라가되                                  */
    /* 단발성 1-sample 톱니만 숫자 표시로 전달되지 않게 한다.                   */
    /*                                                                        */
    /* 저수준 slow path가 이미 매우 안정적이라는 전제이므로,                    */
    /* 강한 outlier detector 대신 step limit 하나만 둔다.                      */
    /* ---------------------------------------------------------------------- */
    delta_input_mps = input_mps - filter->robust_input_mps;
    max_input_step_mps = 0.18f + (4.8f * dt_s) + (0.08f * vario_ui_vario_filter_absf(input_mps));
    delta_input_mps = vario_ui_vario_filter_clampf(delta_input_mps,
                                                   -max_input_step_mps,
                                                   +max_input_step_mps);
    filter->robust_input_mps += delta_input_mps;

    /* ---------------------------------------------------------------------- */
    /* single-pole EMA                                                         */
    /*                                                                        */
    /* damping / averaging knob은 여전히 체감 tuning 값으로 사용하되             */
    /* 과거처럼 여러 단계에 중복 반영하지 않고 여기 한 군데서만 반영한다.       */
    /* ---------------------------------------------------------------------- */
    tau_s = 0.14f + (0.28f * damping_norm) + (0.55f * average_norm);
    alpha = vario_ui_vario_filter_lpf_alpha(dt_s, tau_s);
    filter->smoothed_mps += alpha * (filter->robust_input_mps - filter->smoothed_mps);

    /* ---------------------------------------------------------------------- */
    /* zero hysteresis                                                         */
    /*                                                                        */
    /* 아주 미세한 +- chatter 때문에 숫자가 0.0 / -0.1 / 0.1 사이를             */
    /* 떨지 않도록 0 부근에 얕은 latch만 둔다.                                 */
    /* ---------------------------------------------------------------------- */
    zero_latch_enter_mps = 0.05f + (0.02f * damping_norm);
    zero_latch_exit_mps = 0.15f + (0.03f * average_norm);

    if (filter->zero_latched != false)
    {
        if ((vario_ui_vario_filter_absf(filter->robust_input_mps) > zero_latch_exit_mps) ||
            (vario_ui_vario_filter_absf(filter->smoothed_mps) > zero_latch_exit_mps))
        {
            filter->zero_latched = false;
        }
        else
        {
            filter->display_mps = 0.0f;
            filter->last_update_ms = timestamp_ms;
            return filter->display_mps;
        }
    }
    else if ((vario_ui_vario_filter_absf(filter->robust_input_mps) < zero_latch_enter_mps) &&
             (vario_ui_vario_filter_absf(filter->smoothed_mps) < (zero_latch_enter_mps + 0.03f)))
    {
        filter->zero_latched = true;
        filter->display_mps = 0.0f;
        filter->last_update_ms = timestamp_ms;
        return filter->display_mps;
    }

    filter->display_mps = filter->smoothed_mps;
    filter->last_update_ms = timestamp_ms;
    return filter->display_mps;
}
