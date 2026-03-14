#include "LED_App.h"

#include <string.h>

/* ========================================================================== */
/*  LED_App 구현 개요                                                          */
/*                                                                            */
/*  1) 각 패턴은 target_frame[11]만 만든다.                                    */
/*  2) target_frame -> current_frame으로 부드럽게 이동한다.                    */
/*  3) current_frame을 LED_Driver에 보낸다.                                    */
/*                                                                            */
/*  이 구조의 장점                                                             */
/*  - 패턴 계산과 물리 출력이 분리된다.                                        */
/*  - 모드 전환 시 화면이 갑자기 "툭" 튀지 않는다.                            */
/*  - 앞으로 속도계, 배터리 바, 방향지시, 경고 등 패턴이 늘어나도             */
/*    switch-case와 helper만 추가하면 된다.                                   */
/* ========================================================================== */

#ifndef LED_APP_UPDATE_PERIOD_MS
#define LED_APP_UPDATE_PERIOD_MS             (10u)
#endif

#ifndef LED_APP_DEFAULT_TRANSITION_MS
#define LED_APP_DEFAULT_TRANSITION_MS        (80u)
#endif

#ifndef LED_APP_WELCOME_SWEEP_MS
#define LED_APP_WELCOME_SWEEP_MS             (1200u)
#endif

#ifndef LED_APP_SCANNER_ROUND_TRIP_MS
#define LED_APP_SCANNER_ROUND_TRIP_MS        (1400u)
#endif

#ifndef LED_APP_ALERT_ON_MS
#define LED_APP_ALERT_ON_MS                  (180u)
#endif

#ifndef LED_APP_ALERT_OFF_MS
#define LED_APP_ALERT_OFF_MS                 (180u)
#endif

#ifndef LED_APP_BREATH_PERIOD_MS
#define LED_APP_BREATH_PERIOD_MS             (2600u)
#endif

#define LED_APP_CENTER_INDEX                 (5u)

typedef struct
{
    uint8_t       initialized;
    LED_AppMode_t mode;

    /* ---------------------------------------------------------------------- */
    /*  mode_started_ms                                                        */
    /*    - 현재 모드가 시작된 시각                                             */
    /*    - time based pattern의 기준 시각                                     */
    /* ---------------------------------------------------------------------- */
    uint32_t      mode_started_ms;

    /* ---------------------------------------------------------------------- */
    /*  last_step_ms                                                           */
    /*    - 마지막 실제 프레임 계산 시각                                        */
    /* ---------------------------------------------------------------------- */
    uint32_t      last_step_ms;

    /* 마지막 전체 밝기 스케일 */
    uint16_t      global_scale_permille;

    /* 기본 전환 시간 */
    uint16_t      default_transition_ms;

    /* 모드 파라미터 */
    uint16_t      solid_all_level_permille;
    uint16_t      speedometer_gauge_permille;
    uint8_t       single_dot_index;
    uint16_t      single_dot_level_permille;

    /* 프레임 버퍼 */
    uint16_t      target_frame[LED_DRIVER_CHANNEL_COUNT];
    uint16_t      current_frame[LED_DRIVER_CHANNEL_COUNT];
    uint16_t      custom_frame[LED_DRIVER_CHANNEL_COUNT];
} LED_AppState_t;

static LED_AppState_t s_led_app;

static uint16_t LED_App_ClampPermille(uint16_t value_permille)
{
    if (value_permille > 1000u)
    {
        return 1000u;
    }

    return value_permille;
}

static void LED_App_ClearFrame(uint16_t frame[LED_DRIVER_CHANNEL_COUNT])
{
    memset(frame, 0, sizeof(uint16_t) * LED_DRIVER_CHANNEL_COUNT);
}

/* -------------------------------------------------------------------------- */
/*  모드 진입 helper                                                            */
/*                                                                            */
/*  restart_timeline == 1 이면                                                 */
/*  - time based 패턴의 기준시점을 지금으로 다시 잡는다.                       */
/*                                                                            */
/*  restart_timeline == 0 이면                                                 */
/*  - 같은 모드 안에서 값만 바꿀 때 timeline을 유지한다.                      */
/* -------------------------------------------------------------------------- */
static void LED_App_EnterMode(LED_AppMode_t new_mode,
                              uint32_t now_ms,
                              uint8_t restart_timeline)
{
    LED_AppMode_t previous_mode;

    previous_mode = s_led_app.mode;

    if ((previous_mode != new_mode) || (restart_timeline != 0u))
    {
        s_led_app.mode = new_mode;

        if ((restart_timeline != 0u) || (previous_mode != new_mode))
        {
            s_led_app.mode_started_ms = now_ms;
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  0~1000 -> 0~1000 ease-in-out quadratic                                      */
/*                                                                            */
/*  호흡, 스윕처럼 시작/끝이 너무 기계적으로 보이면 안 되는 패턴에 사용한다.   */
/* -------------------------------------------------------------------------- */
static uint16_t LED_App_EaseInOutQuadPermille(uint16_t x_permille)
{
    uint32_t x;

    x = LED_App_ClampPermille(x_permille);

    if (x < 500u)
    {
        return (uint16_t)((2u * x * x + 500u) / 1000u);
    }

    x = 1000u - x;
    return (uint16_t)(1000u - ((2u * x * x + 500u) / 1000u));
}

static uint32_t LED_App_AbsDiffU32(uint32_t a, uint32_t b)
{
    if (a >= b)
    {
        return (a - b);
    }

    return (b - a);
}

/* -------------------------------------------------------------------------- */
/*  현재 모드에 따라 전환 시간을 다르게 둔다.                                  */
/*                                                                            */
/*  이유                                                                       */
/*  - OFF / SOLID / BREATH / SPEEDOMETER / CUSTOM는 부드럽게 넘어가는 편이     */
/*    보기 좋다.                                                               */
/*  - SINGLE_DOT는 bring-up용이므로 즉답성이 더 중요하다.                      */
/*  - ALERT_BLINK는 또렷하게 깜빡여야 하므로 별도 짧은 값이 더 적절하다.       */
/* -------------------------------------------------------------------------- */
static uint16_t LED_App_GetTransitionTimeMsForCurrentMode(void)
{
    switch (s_led_app.mode)
    {
        case LED_APP_MODE_OFF:
        case LED_APP_MODE_SOLID_ALL:
        case LED_APP_MODE_BREATH_IDLE:
        case LED_APP_MODE_SPEEDOMETER:
        case LED_APP_MODE_CUSTOM_FRAME:
            return s_led_app.default_transition_ms;

        case LED_APP_MODE_WELCOME_SWEEP:
            return 35u;

        case LED_APP_MODE_SCANNER:
            return 25u;

        case LED_APP_MODE_ALERT_BLINK:
            return 15u;

        case LED_APP_MODE_SINGLE_DOT:
            return 0u;

        default:
            return s_led_app.default_transition_ms;
    }
}

/* -------------------------------------------------------------------------- */
/*  패턴 모양이 완성된 뒤 마지막으로 전체 밝기 스케일을 곱한다.                 */
/* -------------------------------------------------------------------------- */
static void LED_App_ApplyGlobalScaleToTargetFrame(void)
{
    uint8_t index;

    for (index = 0u; index < LED_DRIVER_CHANNEL_COUNT; ++index)
    {
        uint32_t scaled_value;

        scaled_value = ((uint32_t)s_led_app.target_frame[index] *
                        (uint32_t)s_led_app.global_scale_permille + 500u) / 1000u;

        if (scaled_value > 1000u)
        {
            scaled_value = 1000u;
        }

        s_led_app.target_frame[index] = (uint16_t)scaled_value;
    }
}

/* -------------------------------------------------------------------------- */
/*  패턴 1: 전체 동일 밝기                                                      */
/*                                                                            */
/*  화면 의미                                                                   */
/*  - LED1~LED11 전체가 같은 세기로 켜진다.                                   */
/*  - 조립 확인 / 생산 테스트 / 전체 점등 데모에 적합하다.                     */
/* -------------------------------------------------------------------------- */
static void LED_App_BuildTarget_SolidAll(void)
{
    uint8_t  index;
    uint16_t level_permille;

    level_permille = LED_App_ClampPermille(s_led_app.solid_all_level_permille);

    for (index = 0u; index < LED_DRIVER_CHANNEL_COUNT; ++index)
    {
        s_led_app.target_frame[index] = level_permille;
    }
}

/* -------------------------------------------------------------------------- */
/*  패턴 2: Idle breath                                                         */
/*                                                                            */
/*  화면 의미                                                                   */
/*  - LED6이 가장 밝은 중심점이다.                                              */
/*  - LED5/LED7, LED4/LED8, ... 으로 갈수록 약해진다.                          */
/*  - 화면상으로는 "가운데가 살아서 숨 쉬는" 느낌을 준다.                     */
/*                                                                            */
/*  UI 위치 설명                                                                */
/*  - [LED1][LED2][LED3][LED4][LED5][LED6][LED7][LED8][LED9][LED10][LED11]    */
/*                                      ↑                                      */
/*                                   중심 호흡                                  */
/* -------------------------------------------------------------------------- */
static void LED_App_BuildTarget_BreathIdle(uint32_t now_ms)
{
    static const uint16_t spatial_weight[LED_DRIVER_CHANNEL_COUNT] =
    {
         50u, 100u, 180u, 320u, 560u,
       1000u,
        560u, 320u, 180u, 100u,  50u
    };

    uint32_t phase_ms;
    uint32_t phase_permille;
    uint16_t triangle_permille;
    uint16_t eased_triangle_permille;
    uint16_t envelope_permille;
    uint8_t  index;

    phase_ms = (now_ms - s_led_app.mode_started_ms) % LED_APP_BREATH_PERIOD_MS;
    phase_permille = (phase_ms * 1000u) / LED_APP_BREATH_PERIOD_MS;

    if (phase_permille < 500u)
    {
        triangle_permille = (uint16_t)(phase_permille * 2u);
    }
    else
    {
        triangle_permille = (uint16_t)((1000u - phase_permille) * 2u);
    }

    eased_triangle_permille = LED_App_EaseInOutQuadPermille(triangle_permille);

    /* ---------------------------------------------------------------------- */
    /*  완전 0까지 떨어뜨리면 화면이 너무 죽어 보일 수 있으므로                  */
    /*  180~1000 범위로만 움직이게 만든다.                                      */
    /* ---------------------------------------------------------------------- */
    envelope_permille = (uint16_t)(180u +
                          (((uint32_t)820u * eased_triangle_permille + 500u) / 1000u));

    for (index = 0u; index < LED_DRIVER_CHANNEL_COUNT; ++index)
    {
        s_led_app.target_frame[index] =
            (uint16_t)(((uint32_t)spatial_weight[index] * envelope_permille + 500u) / 1000u);
    }
}

/* -------------------------------------------------------------------------- */
/*  패턴 3: Welcome sweep                                                       */
/*                                                                            */
/*  화면 의미                                                                   */
/*  - LED1 바깥 왼쪽에서 빛 점이 들어온다.                                      */
/*  - 그 빛 점이 왼쪽에서 오른쪽으로 이동한다.                                 */
/*  - LED11 바깥 오른쪽으로 빠져나간 뒤 자동으로 Idle breath로 전환된다.       */
/*  - 머리(head) 뒤에 꼬리(tail)가 남아 혜성처럼 보이게 만든다.                */
/*                                                                            */
/*  UI 위치 설명                                                                */
/*  - 시작: LED1 왼쪽 바깥                                                     */
/*  - 진행: LED1 → LED11                                                       */
/*  - 종료: LED11 오른쪽 바깥                                                  */
/* -------------------------------------------------------------------------- */
static void LED_App_BuildTarget_WelcomeSweep(uint32_t now_ms)
{
    uint32_t elapsed_ms;
    int32_t  head_position_x1000;
    int32_t  travel_start_x1000;
    int32_t  travel_end_x1000;
    uint32_t total_travel_x1000;
    uint32_t tail_length_x1000;
    uint8_t  index;

    elapsed_ms = (now_ms - s_led_app.mode_started_ms);

    if (elapsed_ms >= LED_APP_WELCOME_SWEEP_MS)
    {
        LED_App_EnterMode(LED_APP_MODE_BREATH_IDLE, now_ms, 1u);
        LED_App_BuildTarget_BreathIdle(now_ms);
        return;
    }

    travel_start_x1000 = -2000;
    travel_end_x1000   = (int32_t)((LED_DRIVER_CHANNEL_COUNT - 1u) * 1000u) + 2000;
    total_travel_x1000 = (uint32_t)(travel_end_x1000 - travel_start_x1000);

    head_position_x1000 = travel_start_x1000 +
                          (int32_t)(((uint64_t)elapsed_ms * total_travel_x1000) /
                                    LED_APP_WELCOME_SWEEP_MS);

    tail_length_x1000 = 2800u;

    for (index = 0u; index < LED_DRIVER_CHANNEL_COUNT; ++index)
    {
        int32_t led_position_x1000;

        led_position_x1000 = (int32_t)index * 1000;

        /* ------------------------------------------------------------------ */
        /*  head보다 오른쪽 앞에 있는 LED는 아직 빛이 도달하지 않은 구간이다.   */
        /* ------------------------------------------------------------------ */
        if (led_position_x1000 > head_position_x1000)
        {
            s_led_app.target_frame[index] = 0u;
            continue;
        }

        /* ------------------------------------------------------------------ */
        /*  tail 길이 안쪽에 들어온 LED만 점등한다.                            */
        /* ------------------------------------------------------------------ */
        if ((uint32_t)(head_position_x1000 - led_position_x1000) <= tail_length_x1000)
        {
            uint32_t distance_x1000;
            uint32_t brightness_permille;

            distance_x1000 = (uint32_t)(head_position_x1000 - led_position_x1000);
            brightness_permille =
                ((tail_length_x1000 - distance_x1000) * 1000u + (tail_length_x1000 / 2u)) /
                tail_length_x1000;

            if (brightness_permille > 1000u)
            {
                brightness_permille = 1000u;
            }

            s_led_app.target_frame[index] = (uint16_t)brightness_permille;
        }
        else
        {
            s_led_app.target_frame[index] = 0u;
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  패턴 4: Speedometer                                                         */
/*                                                                            */
/*  화면 의미                                                                   */
/*  - LED1이 0% 시작점                                                         */
/*  - LED11이 100% 끝점                                                        */
/*  - 왼쪽에서 오른쪽으로 차오르는 11칸 게이지                                */
/*  - 마지막 칸은 부분 밝기로 표현하여 계단감을 줄인다.                       */
/*                                                                            */
/*  사용 예                                                                     */
/*  - app_speedometer 쪽에서                                                   */
/*        LED_Speedometer(speed_permille);                                     */
/*    한 줄만 호출하면 된다.                                                   */
/*                                                                            */
/*  UI 위치 설명                                                                */
/*  - [LED1][LED2][LED3][LED4][LED5][LED6][LED7][LED8][LED9][LED10][LED11]    */
/*  - 왼쪽부터 차오르는 수평 바 그래프                                          */
/* -------------------------------------------------------------------------- */
static void LED_App_BuildTarget_Speedometer(void)
{
    uint16_t gauge_permille;
    uint32_t filled_x1000;
    uint32_t fully_on_led_count;
    uint32_t remainder_permille;
    uint8_t  index;

    gauge_permille = LED_App_ClampPermille(s_led_app.speedometer_gauge_permille);
    filled_x1000 = (uint32_t)gauge_permille * LED_DRIVER_CHANNEL_COUNT;
    fully_on_led_count = filled_x1000 / 1000u;
    remainder_permille = filled_x1000 % 1000u;

    for (index = 0u; index < LED_DRIVER_CHANNEL_COUNT; ++index)
    {
        if (index < fully_on_led_count)
        {
            s_led_app.target_frame[index] = 1000u;
        }
        else if (index == fully_on_led_count)
        {
            if (fully_on_led_count < LED_DRIVER_CHANNEL_COUNT)
            {
                s_led_app.target_frame[index] = (uint16_t)remainder_permille;
            }
            else
            {
                s_led_app.target_frame[index] = 0u;
            }
        }
        else
        {
            s_led_app.target_frame[index] = 0u;
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  패턴 5: Scanner                                                             */
/*                                                                            */
/*  화면 의미                                                                   */
/*  - 한 점이 LED1 -> LED11로 이동한 뒤                                       */
/*  - 다시 LED11 -> LED1로 돌아오는 왕복 패턴                                 */
/*  - bring-up 시 LED 순서를 확인하기 가장 좋은 패턴 중 하나다.               */
/*                                                                            */
/*  UI 위치 설명                                                                */
/*  - head 한 개가 좌우 왕복                                                   */
/*  - head 앞뒤 1칸 내에서만 부분 밝기를 허용하여 부드럽게 보이게 함          */
/* -------------------------------------------------------------------------- */
static void LED_App_BuildTarget_Scanner(uint32_t now_ms)
{
    uint32_t elapsed_ms;
    uint32_t half_trip_ms;
    uint32_t phase_in_round_trip_ms;
    uint32_t head_position_x1000;
    uint32_t travel_x1000;
    uint8_t  index;

    elapsed_ms = now_ms - s_led_app.mode_started_ms;
    phase_in_round_trip_ms = elapsed_ms % LED_APP_SCANNER_ROUND_TRIP_MS;
    half_trip_ms = LED_APP_SCANNER_ROUND_TRIP_MS / 2u;
    travel_x1000 = (LED_DRIVER_CHANNEL_COUNT - 1u) * 1000u;

    if (phase_in_round_trip_ms <= half_trip_ms)
    {
        head_position_x1000 = ((uint64_t)phase_in_round_trip_ms * travel_x1000) / half_trip_ms;
    }
    else
    {
        uint32_t reverse_phase_ms;

        reverse_phase_ms = phase_in_round_trip_ms - half_trip_ms;
        head_position_x1000 = travel_x1000 -
                              (((uint64_t)reverse_phase_ms * travel_x1000) / half_trip_ms);
    }

    for (index = 0u; index < LED_DRIVER_CHANNEL_COUNT; ++index)
    {
        uint32_t led_position_x1000;
        uint32_t distance_x1000;

        led_position_x1000 = (uint32_t)index * 1000u;
        distance_x1000 = LED_App_AbsDiffU32(led_position_x1000, head_position_x1000);

        if (distance_x1000 >= 1000u)
        {
            s_led_app.target_frame[index] = 0u;
        }
        else
        {
            s_led_app.target_frame[index] =
                (uint16_t)(((1000u - distance_x1000) * 1000u + 500u) / 1000u);
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  패턴 6: Alert blink                                                         */
/*                                                                            */
/*  화면 의미                                                                   */
/*  - LED1~LED11 전체가 동시에 켜졌다 꺼졌다 한다.                             */
/*  - 경고 / fault / 알림 / pairing 표시 같은 용도에 어울린다.                */
/* -------------------------------------------------------------------------- */
static void LED_App_BuildTarget_AlertBlink(uint32_t now_ms)
{
    uint32_t elapsed_ms;
    uint32_t cycle_ms;
    uint32_t phase_ms;
    uint8_t  index;

    elapsed_ms = now_ms - s_led_app.mode_started_ms;
    cycle_ms = LED_APP_ALERT_ON_MS + LED_APP_ALERT_OFF_MS;
    phase_ms = elapsed_ms % cycle_ms;

    if (phase_ms < LED_APP_ALERT_ON_MS)
    {
        for (index = 0u; index < LED_DRIVER_CHANNEL_COUNT; ++index)
        {
            s_led_app.target_frame[index] = 1000u;
        }
    }
    else
    {
        for (index = 0u; index < LED_DRIVER_CHANNEL_COUNT; ++index)
        {
            s_led_app.target_frame[index] = 0u;
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  패턴 7: Single dot                                                          */
/*                                                                            */
/*  화면 의미                                                                   */
/*  - 지정한 한 칸만 켠다.                                                      */
/*  - bring-up에서 "어느 물리 LED가 어느 논리 인덱스인지" 확인할 때 유용하다. */
/* -------------------------------------------------------------------------- */
static void LED_App_BuildTarget_SingleDot(void)
{
    uint8_t  index;
    uint8_t  clamped_index;
    uint16_t level_permille;

    clamped_index = s_led_app.single_dot_index;
    if (clamped_index >= LED_DRIVER_CHANNEL_COUNT)
    {
        clamped_index = (LED_DRIVER_CHANNEL_COUNT - 1u);
    }

    level_permille = LED_App_ClampPermille(s_led_app.single_dot_level_permille);

    for (index = 0u; index < LED_DRIVER_CHANNEL_COUNT; ++index)
    {
        if (index == clamped_index)
        {
            s_led_app.target_frame[index] = level_permille;
        }
        else
        {
            s_led_app.target_frame[index] = 0u;
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  패턴 8: Custom frame                                                        */
/*                                                                            */
/*  화면 의미                                                                   */
/*  - 상위 App가 11칸 전체 프레임을 직접 만들어서 넘기는 모드                  */
/*  - 이 경우에도 감마, 전환, 물리 출력은 계속 공통 파이프라인을 사용한다.    */
/* -------------------------------------------------------------------------- */
static void LED_App_BuildTarget_CustomFrame(void)
{
    memcpy(s_led_app.target_frame,
           s_led_app.custom_frame,
           sizeof(s_led_app.target_frame));
}

static void LED_App_BuildTargetFrame(uint32_t now_ms)
{
    LED_App_ClearFrame(s_led_app.target_frame);

    switch (s_led_app.mode)
    {
        case LED_APP_MODE_OFF:
            break;

        case LED_APP_MODE_SOLID_ALL:
            LED_App_BuildTarget_SolidAll();
            break;

        case LED_APP_MODE_BREATH_IDLE:
            LED_App_BuildTarget_BreathIdle(now_ms);
            break;

        case LED_APP_MODE_WELCOME_SWEEP:
            LED_App_BuildTarget_WelcomeSweep(now_ms);
            break;

        case LED_APP_MODE_SPEEDOMETER:
            LED_App_BuildTarget_Speedometer();
            break;

        case LED_APP_MODE_SCANNER:
            LED_App_BuildTarget_Scanner(now_ms);
            break;

        case LED_APP_MODE_ALERT_BLINK:
            LED_App_BuildTarget_AlertBlink(now_ms);
            break;

        case LED_APP_MODE_SINGLE_DOT:
            LED_App_BuildTarget_SingleDot();
            break;

        case LED_APP_MODE_CUSTOM_FRAME:
            LED_App_BuildTarget_CustomFrame();
            break;

        default:
            LED_App_ClearFrame(s_led_app.target_frame);
            break;
    }

    LED_App_ApplyGlobalScaleToTargetFrame();
}

/* -------------------------------------------------------------------------- */
/*  target -> current 부드러운 전환                                             */
/*                                                                            */
/*  핵심 아이디어                                                               */
/*  - 매 update tick마다 current를 target 쪽으로 조금씩 이동                  */
/*  - transition_ms가 0이면 즉시 일치                                          */
/*  - transition_ms가 크면 천천히 따라감                                      */
/* -------------------------------------------------------------------------- */
static void LED_App_AdvanceCurrentFrameTowardTarget(void)
{
    uint16_t transition_ms;
    uint8_t  index;

    transition_ms = LED_App_GetTransitionTimeMsForCurrentMode();

    if (transition_ms == 0u)
    {
        memcpy(s_led_app.current_frame,
               s_led_app.target_frame,
               sizeof(s_led_app.current_frame));
        return;
    }

    for (index = 0u; index < LED_DRIVER_CHANNEL_COUNT; ++index)
    {
        int32_t current_value;
        int32_t target_value;
        int32_t diff_value;
        int32_t step_value;

        current_value = (int32_t)s_led_app.current_frame[index];
        target_value  = (int32_t)s_led_app.target_frame[index];
        diff_value    = target_value - current_value;

        if (diff_value == 0)
        {
            continue;
        }

        step_value = (diff_value * (int32_t)LED_APP_UPDATE_PERIOD_MS) /
                     (int32_t)transition_ms;

        if (step_value == 0)
        {
            step_value = (diff_value > 0) ? 1 : -1;
        }

        if (((diff_value > 0) && (current_value + step_value > target_value)) ||
            ((diff_value < 0) && (current_value + step_value < target_value)))
        {
            current_value = target_value;
        }
        else
        {
            current_value += step_value;
        }

        if (current_value < 0)
        {
            current_value = 0;
        }
        else if (current_value > 1000)
        {
            current_value = 1000;
        }

        s_led_app.current_frame[index] = (uint16_t)current_value;
    }
}

void LED_App_Init(void)
{
    memset(&s_led_app, 0, sizeof(s_led_app));

    s_led_app.initialized                = 1u;
    s_led_app.mode                       = LED_APP_MODE_OFF;
    s_led_app.mode_started_ms            = HAL_GetTick();
    s_led_app.last_step_ms               = HAL_GetTick();
    s_led_app.global_scale_permille      = 1000u;
    s_led_app.default_transition_ms      = LED_APP_DEFAULT_TRANSITION_MS;
    s_led_app.solid_all_level_permille   = 1000u;
    s_led_app.speedometer_gauge_permille = 0u;
    s_led_app.single_dot_index           = 0u;
    s_led_app.single_dot_level_permille  = 1000u;

    LED_Driver_Init();
    LED_Driver_AllOff();
}

void LED_App_Task(uint32_t now_ms)
{
    if (s_led_app.initialized == 0u)
    {
        return;
    }

    if ((now_ms - s_led_app.last_step_ms) < LED_APP_UPDATE_PERIOD_MS)
    {
        return;
    }

    s_led_app.last_step_ms = now_ms;

    LED_App_BuildTargetFrame(now_ms);
    LED_App_AdvanceCurrentFrameTowardTarget();
    LED_Driver_WriteLinearFramePermille(s_led_app.current_frame);
}

LED_AppMode_t LED_App_GetMode(void)
{
    return s_led_app.mode;
}

void LED_App_SetGlobalBrightnessScale(uint16_t scale_permille)
{
    s_led_app.global_scale_permille = LED_App_ClampPermille(scale_permille);
}

void LED_App_SetTransitionTimeMs(uint16_t transition_ms)
{
    s_led_app.default_transition_ms = transition_ms;
}

void LED_Off(void)
{
    LED_App_EnterMode(LED_APP_MODE_OFF, HAL_GetTick(), 1u);
}

void LED_AllOn(uint16_t brightness_permille)
{
    s_led_app.solid_all_level_permille = LED_App_ClampPermille(brightness_permille);
    LED_App_EnterMode(LED_APP_MODE_SOLID_ALL, HAL_GetTick(), 0u);
}

void LED_BreathIdle(void)
{
    LED_App_EnterMode(LED_APP_MODE_BREATH_IDLE, HAL_GetTick(), 1u);
}

void LED_WelcomeSweep(void)
{
    LED_App_EnterMode(LED_APP_MODE_WELCOME_SWEEP, HAL_GetTick(), 1u);
}

void LED_Speedometer(uint16_t gauge_permille)
{
    s_led_app.speedometer_gauge_permille = LED_App_ClampPermille(gauge_permille);

    if (s_led_app.mode != LED_APP_MODE_SPEEDOMETER)
    {
        LED_App_EnterMode(LED_APP_MODE_SPEEDOMETER, HAL_GetTick(), 1u);
    }
}

void LED_Scanner(void)
{
    LED_App_EnterMode(LED_APP_MODE_SCANNER, HAL_GetTick(), 1u);
}

void LED_AlertBlink(void)
{
    LED_App_EnterMode(LED_APP_MODE_ALERT_BLINK, HAL_GetTick(), 1u);
}

void LED_SingleDot(uint8_t logical_led_index, uint16_t brightness_permille)
{
    s_led_app.single_dot_index = logical_led_index;
    s_led_app.single_dot_level_permille = LED_App_ClampPermille(brightness_permille);
    LED_App_EnterMode(LED_APP_MODE_SINGLE_DOT, HAL_GetTick(), 0u);
}

void LED_CustomFrame(const uint16_t frame_permille[LED_DRIVER_CHANNEL_COUNT])
{
    uint8_t index;

    if (frame_permille == NULL)
    {
        return;
    }

    for (index = 0u; index < LED_DRIVER_CHANNEL_COUNT; ++index)
    {
        s_led_app.custom_frame[index] = LED_App_ClampPermille(frame_permille[index]);
    }

    LED_App_EnterMode(LED_APP_MODE_CUSTOM_FRAME, HAL_GetTick(), 0u);
}
