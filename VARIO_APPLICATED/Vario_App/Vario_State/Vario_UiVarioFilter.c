#include "Vario_UiVarioFilter.h"

#include <math.h>
#include <string.h>

#ifndef VARIO_UI_VARIO_FILTER_MIN_DT_S
#define VARIO_UI_VARIO_FILTER_MIN_DT_S 0.010f
#endif

#ifndef VARIO_UI_VARIO_FILTER_MAX_DT_S
#define VARIO_UI_VARIO_FILTER_MAX_DT_S 0.250f
#endif

#ifndef VARIO_UI_VARIO_FILTER_HAMPEL_SIGMA_SCALE
#define VARIO_UI_VARIO_FILTER_HAMPEL_SIGMA_SCALE 1.4826f
#endif

#ifndef VARIO_UI_VARIO_FILTER_HAMPEL_SIGMA_GAIN
#define VARIO_UI_VARIO_FILTER_HAMPEL_SIGMA_GAIN 3.0f
#endif

#ifndef VARIO_UI_VARIO_FILTER_HAMPEL_MIN_CLAMP_MPS
#define VARIO_UI_VARIO_FILTER_HAMPEL_MIN_CLAMP_MPS 0.10f
#endif

#ifndef VARIO_UI_VARIO_FILTER_HAMPEL_MAX_CLAMP_MPS
#define VARIO_UI_VARIO_FILTER_HAMPEL_MAX_CLAMP_MPS 0.65f
#endif

/* -------------------------------------------------------------------------- */
/* 내부 helper                                                                 */
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

/* -------------------------------------------------------------------------- */
/* 아주 작은 고정 길이 배열만 다루므로 insertion sort 를 사용한다.            */
/* 동적 메모리도 없고, 7개 이하 배열에서는 오히려 의도가 가장 명확하다.      */
/* -------------------------------------------------------------------------- */
static void vario_ui_vario_filter_sort_small(float *values, uint8_t count)
{
    uint8_t i;
    uint8_t j;
    float key;

    if (values == NULL)
    {
        return;
    }

    for (i = 1u; i < count; ++i)
    {
        key = values[i];
        j = i;

        while ((j > 0u) && (values[j - 1u] > key))
        {
            values[j] = values[j - 1u];
            --j;
        }

        values[j] = key;
    }
}

static float vario_ui_vario_filter_median_of_array(const float *values, uint8_t count)
{
    float sorted[VARIO_UI_VARIO_FILTER_INPUT_WINDOW];
    uint8_t i;

    if ((values == NULL) || (count == 0u))
    {
        return 0.0f;
    }

    for (i = 0u; i < count; ++i)
    {
        sorted[i] = values[i];
    }

    vario_ui_vario_filter_sort_small(sorted, count);

    if ((count & 1u) != 0u)
    {
        return sorted[count / 2u];
    }

    return (sorted[(count / 2u) - 1u] + sorted[count / 2u]) * 0.5f;
}

/* -------------------------------------------------------------------------- */
/* ring buffer 는 시간 순서가 아니라 "현재 보관 중인 값 집합" 만 필요하다.    */
/* median / MAD 는 순서가 아니라 집합 자체만 필요하므로,                      */
/* 유효 개수만큼만 선형 배열로 복사하면 충분하다.                             */
/* -------------------------------------------------------------------------- */
static uint8_t vario_ui_vario_filter_copy_window(const float *window,
                                                 uint8_t count,
                                                 float *out_values,
                                                 uint8_t max_count)
{
    uint8_t i;

    if ((window == NULL) || (out_values == NULL))
    {
        return 0u;
    }

    if (count > max_count)
    {
        count = max_count;
    }

    for (i = 0u; i < count; ++i)
    {
        out_values[i] = window[i];
    }

    return count;
}

static void vario_ui_vario_filter_push_input(vario_ui_vario_filter_t *filter, float value)
{
    if (filter == NULL)
    {
        return;
    }

    filter->input_window_mps[filter->input_head] = value;
    filter->input_head = (uint8_t)((filter->input_head + 1u) % VARIO_UI_VARIO_FILTER_INPUT_WINDOW);

    if (filter->input_count < VARIO_UI_VARIO_FILTER_INPUT_WINDOW)
    {
        ++filter->input_count;
    }
}

static void vario_ui_vario_filter_push_output(vario_ui_vario_filter_t *filter, float value)
{
    if (filter == NULL)
    {
        return;
    }

    filter->output_window_mps[filter->output_head] = value;
    filter->output_head = (uint8_t)((filter->output_head + 1u) % VARIO_UI_VARIO_FILTER_OUTPUT_WINDOW);

    if (filter->output_count < VARIO_UI_VARIO_FILTER_OUTPUT_WINDOW)
    {
        ++filter->output_count;
    }
}

static float vario_ui_vario_filter_compute_dt_s(const vario_ui_vario_filter_t *filter,
                                                uint32_t timestamp_ms)
{
    uint32_t delta_ms;
    float dt_s;

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

/* -------------------------------------------------------------------------- */
/* Hampel filter                                                               */
/*                                                                            */
/* 역할                                                                       */
/* - slow_vario 에 섞여 들어오는 단발성 spike 를 잘라낸다.                   */
/* - 완전 discard 가 아니라 median 주변으로 clamp 한다.                       */
/* - 따라서 실제 상승/하강이 시작될 때는 응답을 완전히 죽이지 않는다.        */
/* -------------------------------------------------------------------------- */
static float vario_ui_vario_filter_apply_hampel(vario_ui_vario_filter_t *filter,
                                                float input_mps)
{
    float samples[VARIO_UI_VARIO_FILTER_INPUT_WINDOW];
    float deviations[VARIO_UI_VARIO_FILTER_INPUT_WINDOW];
    uint8_t count;
    uint8_t i;
    float median_mps;
    float mad_mps;
    float sigma_mps;
    float clamp_limit_mps;
    float delta_mps;

    if (filter == NULL)
    {
        return input_mps;
    }

    count = vario_ui_vario_filter_copy_window(filter->input_window_mps,
                                              filter->input_count,
                                              samples,
                                              VARIO_UI_VARIO_FILTER_INPUT_WINDOW);

    if (count < 3u)
    {
        return input_mps;
    }

    median_mps = vario_ui_vario_filter_median_of_array(samples, count);

    for (i = 0u; i < count; ++i)
    {
        deviations[i] = vario_ui_vario_filter_absf(samples[i] - median_mps);
    }

    mad_mps = vario_ui_vario_filter_median_of_array(deviations, count);
    sigma_mps = VARIO_UI_VARIO_FILTER_HAMPEL_SIGMA_SCALE * mad_mps;

    /* ---------------------------------------------------------------------- */
    /* clamp 폭                                                                */
    /* - noise 가 거의 없는 실험실 정지 상태에서도 너무 빡빡해서                 */
    /*   정상 미세 응답까지 다 잘라버리면 안 되므로 최소폭을 둔다.               */
    /* - 반대로 noise floor 가 커질 때는 MAD 기반으로 clamp 폭도 함께 넓힌다.    */
    /* ---------------------------------------------------------------------- */
    clamp_limit_mps = VARIO_UI_VARIO_FILTER_HAMPEL_MIN_CLAMP_MPS +
                      (sigma_mps * VARIO_UI_VARIO_FILTER_HAMPEL_SIGMA_GAIN);

    clamp_limit_mps = vario_ui_vario_filter_clampf(clamp_limit_mps,
                                                   VARIO_UI_VARIO_FILTER_HAMPEL_MIN_CLAMP_MPS,
                                                   VARIO_UI_VARIO_FILTER_HAMPEL_MAX_CLAMP_MPS);

    delta_mps = input_mps - median_mps;

    if (vario_ui_vario_filter_absf(delta_mps) <= clamp_limit_mps)
    {
        return input_mps;
    }

    if (delta_mps > 0.0f)
    {
        return median_mps + clamp_limit_mps;
    }

    return median_mps - clamp_limit_mps;
}

/* -------------------------------------------------------------------------- */
/* quiet 상태 time constant                                                    */
/* - damping_level / average_seconds 를 그대로 버리지 않고                    */
/*   상위 앱 레이어 UI 필터에서도 의미 있게 재사용한다.                       */
/* - 다만 upstream slow_vario 자체가 이미 어느 정도 느리기 때문에             */
/*   여기서는 "과도한 추가 지연" 이 생기지 않게 범위를 보수적으로 잡는다.     */
/* -------------------------------------------------------------------------- */
static float vario_ui_vario_filter_compute_quiet_tau_s(uint8_t damping_level,
                                                       uint8_t average_seconds)
{
    float damping_norm;
    float average_norm;

    damping_level = vario_ui_vario_filter_clamp_u8(damping_level, 1u, 10u);
    average_seconds = vario_ui_vario_filter_clamp_u8(average_seconds, 1u, 8u);

    damping_norm = ((float)(damping_level - 1u)) / 9.0f;
    average_norm = ((float)(average_seconds - 1u)) / 7.0f;

    return 0.22f + (average_norm * 0.20f) + (damping_norm * 0.18f);
}

/* -------------------------------------------------------------------------- */
/* adaptive tau                                                                */
/* - innovation 이 작을 때는 묵직하게                                         */
/* - innovation 이 커지면 빠르게 따라가게                                     */
/*                                                                            */
/* 정지 상태에서는 잘 안 흔들리고,                                           */
/* 실제로 1m/s 이상 오르내리기 시작하면 너무 굼뜨지 않도록 만든다.            */
/* -------------------------------------------------------------------------- */
static float vario_ui_vario_filter_compute_adaptive_tau_s(float quiet_tau_s,
                                                          float innovation_mps)
{
    float abs_innovation_mps;
    float tau_s;

    abs_innovation_mps = vario_ui_vario_filter_absf(innovation_mps);
    tau_s = quiet_tau_s;

    if (abs_innovation_mps >= 1.20f)
    {
        tau_s = quiet_tau_s * 0.18f;
    }
    else if (abs_innovation_mps >= 0.60f)
    {
        tau_s = quiet_tau_s * 0.28f;
    }
    else if (abs_innovation_mps >= 0.25f)
    {
        tau_s = quiet_tau_s * 0.45f;
    }

    return vario_ui_vario_filter_clampf(tau_s, 0.06f, quiet_tau_s);
}

/* -------------------------------------------------------------------------- */
/* 0 근처 chatter 제거                                                         */
/* - enter threshold : 0으로 잠기는 문턱                                      */
/* - exit threshold  : 0에서 다시 풀려나는 문턱                               */
/*                                                                            */
/* hysteresis 를 두지 않으면 0.0 / 0.1 / -0.1 같은 UI chatter 가 생긴다.     */
/* -------------------------------------------------------------------------- */
static float vario_ui_vario_filter_apply_zero_hysteresis(vario_ui_vario_filter_t *filter,
                                                         float input_mps,
                                                         uint8_t damping_level,
                                                         uint8_t average_seconds)
{
    float damping_norm;
    float average_norm;
    float enter_mps;
    float exit_mps;
    float abs_input_mps;

    if (filter == NULL)
    {
        return input_mps;
    }

    damping_level = vario_ui_vario_filter_clamp_u8(damping_level, 1u, 10u);
    average_seconds = vario_ui_vario_filter_clamp_u8(average_seconds, 1u, 8u);

    damping_norm = ((float)(damping_level - 1u)) / 9.0f;
    average_norm = ((float)(average_seconds - 1u)) / 7.0f;

    enter_mps = 0.05f + (average_norm * 0.03f) + (damping_norm * 0.04f);
    exit_mps = enter_mps + 0.06f;
    abs_input_mps = vario_ui_vario_filter_absf(input_mps);

    if (filter->zero_latched != false)
    {
        if (abs_input_mps <= exit_mps)
        {
            return 0.0f;
        }

        filter->zero_latched = false;
        return input_mps;
    }

    if (abs_input_mps <= enter_mps)
    {
        filter->zero_latched = true;
        return 0.0f;
    }

    return input_mps;
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

    for (i = 0u; i < VARIO_UI_VARIO_FILTER_INPUT_WINDOW; ++i)
    {
        filter->input_window_mps[i] = input_mps;
    }

    for (i = 0u; i < VARIO_UI_VARIO_FILTER_OUTPUT_WINDOW; ++i)
    {
        filter->output_window_mps[i] = input_mps;
    }

    filter->initialized = true;
    filter->zero_latched = (vario_ui_vario_filter_absf(input_mps) <= 0.08f) ? true : false;
    filter->input_count = VARIO_UI_VARIO_FILTER_INPUT_WINDOW;
    filter->output_count = VARIO_UI_VARIO_FILTER_OUTPUT_WINDOW;
    filter->input_head = 0u;
    filter->output_head = 0u;
    filter->last_update_ms = timestamp_ms;
    filter->robust_input_mps = input_mps;
    filter->smoothed_mps = input_mps;
    filter->display_mps = (filter->zero_latched != false) ? 0.0f : input_mps;
}

float Vario_UiVarioFilter_Update(vario_ui_vario_filter_t *filter,
                                 float input_mps,
                                 uint32_t timestamp_ms,
                                 uint8_t damping_level,
                                 uint8_t average_seconds)
{
    float dt_s;
    float quiet_tau_s;
    float adaptive_tau_s;
    float alpha;
    float innovation_mps;
    float publish_samples[VARIO_UI_VARIO_FILTER_OUTPUT_WINDOW];
    uint8_t publish_count;

    if (filter == NULL)
    {
        return input_mps;
    }

    if (filter->initialized == false)
    {
        Vario_UiVarioFilter_Reset(filter, input_mps, timestamp_ms);
        return filter->display_mps;
    }

    dt_s = vario_ui_vario_filter_compute_dt_s(filter, timestamp_ms);

    /* ---------------------------------------------------------------------- */
    /* stage 1: outlier clamp                                                  */
    /* - 먼저 현재 raw slow_vario 를 입력 창에 넣는다.                         */
    /* - 그 창을 기준으로 Hampel/MAD clamp 를 계산한다.                        */
    /* ---------------------------------------------------------------------- */
    vario_ui_vario_filter_push_input(filter, input_mps);
    filter->robust_input_mps = vario_ui_vario_filter_apply_hampel(filter, input_mps);

    /* ---------------------------------------------------------------------- */
    /* stage 2: adaptive EMA                                                   */
    /* - 정지 상태에서는 흔들림을 눌러주고                                    */
    /* - 실제 climb/sink 가 커지면 따라가는 속도를 올린다.                     */
    /* ---------------------------------------------------------------------- */
    quiet_tau_s = vario_ui_vario_filter_compute_quiet_tau_s(damping_level, average_seconds);
    innovation_mps = filter->robust_input_mps - filter->smoothed_mps;
    adaptive_tau_s = vario_ui_vario_filter_compute_adaptive_tau_s(quiet_tau_s, innovation_mps);
    alpha = dt_s / (adaptive_tau_s + dt_s);

    filter->smoothed_mps += alpha * innovation_mps;

    /* ---------------------------------------------------------------------- */
    /* stage 3: short median                                                   */
    /* - 직전 3개 표시 후보의 median 을 취해                                   */
    /*   1~2 샘플짜리 남은 뾰족한 흔들림을 마지막으로 한 번 더 정리한다.        */
    /* ---------------------------------------------------------------------- */
    vario_ui_vario_filter_push_output(filter, filter->smoothed_mps);
    publish_count = vario_ui_vario_filter_copy_window(filter->output_window_mps,
                                                      filter->output_count,
                                                      publish_samples,
                                                      VARIO_UI_VARIO_FILTER_OUTPUT_WINDOW);

    if (publish_count != 0u)
    {
        filter->display_mps = vario_ui_vario_filter_median_of_array(publish_samples, publish_count);
    }
    else
    {
        filter->display_mps = filter->smoothed_mps;
    }

    /* ---------------------------------------------------------------------- */
    /* stage 4: zero hysteresis                                                */
    /* - 최종 숫자가 0 부근에서 달달 떨며                                       */
    /*   +0.1 / 0.0 / -0.1 을 반복하지 않도록 latch 를 건다.                   */
    /* ---------------------------------------------------------------------- */
    filter->display_mps = vario_ui_vario_filter_apply_zero_hysteresis(filter,
                                                                      filter->display_mps,
                                                                      damping_level,
                                                                      average_seconds);

    filter->display_mps = vario_ui_vario_filter_clampf(filter->display_mps, -15.0f, +15.0f);
    filter->last_update_ms = timestamp_ms;

    return filter->display_mps;
}
