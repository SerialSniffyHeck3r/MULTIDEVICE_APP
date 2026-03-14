#include "LED_App.h"

#include <string.h>

/* ========================================================================== */
/*  LED_App 구현 개요                                                          */
/*                                                                            */
/*  1) 이 파일은 "지금 LED가 무엇을 표현할지"만 결정한다.                     */
/*  2) 실제 패턴 그림(target frame 생성), smoothing, perceptual remap은        */
/*     LED_Driver가 맡는다.                                                   */
/*  3) main.c는 LED_App_Task(HAL_GetTick())만 계속 호출하면 된다.             */
/*                                                                            */
/*  의도적인 계층 분리                                                         */
/*  - App: mode 선택 / 파라미터 보관 / test pattern advance                   */
/*  - Driver: frame 렌더링 / PWM 출력 / 인간 시감 보정                        */
/*                                                                            */
/*  이번 버전에서 제거한 것                                                    */
/*  - SPEEDOMETER 모드                                                         */
/*                                                                            */
/*  이번 버전에서 추가한 것                                                    */
/*  - boot 기본 상태 OFF                                                       */
/*  - 10개 이상 LED test pattern 순환                                          */
/*  - F2 long press에서 쓰기 좋은 Advance API                                 */
/* ========================================================================== */

#ifndef LED_APP_DEFAULT_TRANSITION_MS
#define LED_APP_DEFAULT_TRANSITION_MS       (80u)
#endif

#ifndef LED_APP_WELCOME_SWEEP_MS
#define LED_APP_WELCOME_SWEEP_MS            (1200u)
#endif

typedef struct
{
    uint8_t              initialized;
    LED_AppMode_t        mode;

    /* ---------------------------------------------------------------------- */
    /*  mode_started_ms                                                        */
    /*                                                                        */
    /*  - 현재 mode가 시작된 HAL tick(ms)                                      */
    /*  - time based pattern의 기준 시각                                       */
    /*  - 실제 시간 기반 렌더링 계산은 driver가 하되,                          */
    /*    기준 시각의 소유권은 app이 가진다.                                   */
    /* ---------------------------------------------------------------------- */
    uint32_t             mode_started_ms;

    /* ---------------------------------------------------------------------- */
    /*  master brightness                                                      */
    /*                                                                        */
    /*  - 전체 프레임에 마지막으로 곱해지는 global scale                        */
    /*  - 16-bit 전체 범위로 관리한다.                                         */
    /* ---------------------------------------------------------------------- */
    uint16_t             global_scale_q16;

    /* ---------------------------------------------------------------------- */
    /*  default transition                                                     */
    /*                                                                        */
    /*  - OFF, SOLID, CUSTOM 같은 일반 모드 전환에서 사용하는 기본 시간         */
    /* ---------------------------------------------------------------------- */
    uint16_t             default_transition_ms;

    /* ---------------------------------------------------------------------- */
    /*  일반 모드 파라미터                                                      */
    /* ---------------------------------------------------------------------- */
    uint16_t             solid_all_level_q16;
    uint8_t              single_dot_index;
    uint16_t             single_dot_level_q16;
    uint16_t             custom_frame_q16[LED_DRIVER_CHANNEL_COUNT];

    /* ---------------------------------------------------------------------- */
    /*  test pattern 상태                                                       */
    /*                                                                        */
    /*  - boot 시에는 OFF를 들고 시작한다.                                     */
    /*  - Advance API가 이 값을 증가시키고,                                    */
    /*    현재 LED mode도 함께 변경한다.                                       */
    /* ---------------------------------------------------------------------- */
    LED_AppTestPattern_t test_pattern;
} LED_AppState_t;

static LED_AppState_t s_led_app;

static uint16_t LED_App_ClampQ16(uint32_t value_q16)
{
    if (value_q16 > LED_DRIVER_Q16_MAX)
    {
        return (uint16_t)LED_DRIVER_Q16_MAX;
    }

    return (uint16_t)value_q16;
}

static uint16_t LED_App_ClampPermille(uint16_t value_permille)
{
    if (value_permille > LED_DRIVER_PERMILLE_MAX)
    {
        return (uint16_t)LED_DRIVER_PERMILLE_MAX;
    }

    return value_permille;
}

static uint16_t LED_App_PermilleToQ16(uint16_t value_permille)
{
    uint32_t clamped_permille;

    clamped_permille = (uint32_t)LED_App_ClampPermille(value_permille);
    return (uint16_t)((clamped_permille * LED_DRIVER_Q16_MAX + 500u) /
                      LED_DRIVER_PERMILLE_MAX);
}

/* -------------------------------------------------------------------------- */
/*  mode 진입 helper                                                            */
/*                                                                            */
/*  restart_timeline == 1                                                      */
/*  - 같은 mode를 다시 호출하더라도 mode_started_ms를 지금으로 갱신            */
/*  - 스윕, 스캐너, 테스트 패턴 재시작에 사용                                  */
/*                                                                            */
/*  restart_timeline == 0                                                      */
/*  - 같은 mode 안에서 값만 갱신할 때 timeline 유지                            */
/*  - solid 밝기 변경, custom frame 갱신 등에 사용                             */
/* -------------------------------------------------------------------------- */
static void LED_App_EnterMode(LED_AppMode_t new_mode,
                              uint32_t now_ms,
                              uint8_t restart_timeline)
{
    LED_AppMode_t previous_mode;

    previous_mode = s_led_app.mode;
    s_led_app.mode = new_mode;

    if ((previous_mode != new_mode) || (restart_timeline != 0u))
    {
        s_led_app.mode_started_ms = now_ms;
    }
}

/* -------------------------------------------------------------------------- */
/*  현재 app mode에 맞는 기본 transition time 선택                              */
/*                                                                            */
/*  이유                                                                       */
/*  - OFF / SOLID / BREATH / CUSTOM는 천천히 이어지는 편이 보기 좋다.          */
/*  - SINGLE_DOT는 인덱스 확인용이라 즉답성이 중요하다.                        */
/*  - ALERT / SCANNER / WELCOME은 모양이 흐려지지 않게 더 짧게 둔다.          */
/*  - TEST_PATTERN은 패턴마다 helper가 별도 값을 정한다.                      */
/* -------------------------------------------------------------------------- */
static uint16_t LED_App_GetTransitionTimeMs(void)
{
    switch (s_led_app.mode)
    {
        case LED_APP_MODE_OFF:
        case LED_APP_MODE_SOLID_ALL:
        case LED_APP_MODE_BREATH_IDLE:
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

        case LED_APP_MODE_TEST_PATTERN:
        default:
            return s_led_app.default_transition_ms;
    }
}

/* -------------------------------------------------------------------------- */
/*  테스트 패턴별 transition time                                               */
/*                                                                            */
/*  패턴 의도                                                                  */
/*  - 순서 확인용 도트/분할 패턴은 또렷해야 하므로 즉시 전환                    */
/*  - breath 계열만 부드러운 전환을 허용                                       */
/* -------------------------------------------------------------------------- */
static uint16_t LED_App_GetTestPatternTransitionTimeMs(LED_AppTestPattern_t pattern)
{
    switch (pattern)
    {
        case LED_APP_TEST_PATTERN_OFF:
        case LED_APP_TEST_PATTERN_ALL_ON:
        case LED_APP_TEST_PATTERN_WALK_LEFT_TO_RIGHT:
        case LED_APP_TEST_PATTERN_WALK_RIGHT_TO_LEFT:
        case LED_APP_TEST_PATTERN_SCANNER:
        case LED_APP_TEST_PATTERN_CENTER_OUT:
        case LED_APP_TEST_PATTERN_EDGE_IN:
        case LED_APP_TEST_PATTERN_ODD_EVEN:
        case LED_APP_TEST_PATTERN_HALF_SWAP:
        case LED_APP_TEST_PATTERN_BAR_FILL:
        case LED_APP_TEST_PATTERN_STROBE_SLOW:
            return 0u;

        case LED_APP_TEST_PATTERN_BREATH_ALL:
        case LED_APP_TEST_PATTERN_BREATH_CENTER:
            return 35u;

        case LED_APP_TEST_PATTERN_COUNT:
        default:
            return 0u;
    }
}

/* -------------------------------------------------------------------------- */
/*  현재 test pattern enum을 사람이 읽는 toast text로 변환                      */
/*                                                                            */
/*  토스트는 폭이 한정적이므로 짧고 바로 이해되는 이름으로 정리했다.           */
/* -------------------------------------------------------------------------- */
const char *LED_App_GetTestPatternName(LED_AppTestPattern_t pattern)
{
    switch (pattern)
    {
        case LED_APP_TEST_PATTERN_OFF:
            return "LED TEST: OFF";

        case LED_APP_TEST_PATTERN_ALL_ON:
            return "LED TEST: ALL ON";

        case LED_APP_TEST_PATTERN_WALK_LEFT_TO_RIGHT:
            return "LED TEST: WALK >";

        case LED_APP_TEST_PATTERN_WALK_RIGHT_TO_LEFT:
            return "LED TEST: WALK <";

        case LED_APP_TEST_PATTERN_SCANNER:
            return "LED TEST: SCAN";

        case LED_APP_TEST_PATTERN_CENTER_OUT:
            return "LED TEST: CTR OUT";

        case LED_APP_TEST_PATTERN_EDGE_IN:
            return "LED TEST: EDGE IN";

        case LED_APP_TEST_PATTERN_ODD_EVEN:
            return "LED TEST: ODD/EVN";

        case LED_APP_TEST_PATTERN_HALF_SWAP:
            return "LED TEST: HALF";

        case LED_APP_TEST_PATTERN_BREATH_ALL:
            return "LED TEST: BR ALL";

        case LED_APP_TEST_PATTERN_BREATH_CENTER:
            return "LED TEST: BR CTR";

        case LED_APP_TEST_PATTERN_BAR_FILL:
            return "LED TEST: BAR";

        case LED_APP_TEST_PATTERN_STROBE_SLOW:
            return "LED TEST: STROBE";

        case LED_APP_TEST_PATTERN_COUNT:
        default:
            return "LED TEST: ?";
    }
}

const char *LED_App_GetModeName(void)
{
    switch (s_led_app.mode)
    {
        case LED_APP_MODE_OFF:
            return "LED OFF";

        case LED_APP_MODE_SOLID_ALL:
            return "LED SOLID";

        case LED_APP_MODE_BREATH_IDLE:
            return "LED BREATH";

        case LED_APP_MODE_WELCOME_SWEEP:
            return "LED SWEEP";

        case LED_APP_MODE_SCANNER:
            return "LED SCANNER";

        case LED_APP_MODE_ALERT_BLINK:
            return "LED ALERT";

        case LED_APP_MODE_SINGLE_DOT:
            return "LED SINGLE";

        case LED_APP_MODE_CUSTOM_FRAME:
            return "LED CUSTOM";

        case LED_APP_MODE_TEST_PATTERN:
            return LED_App_GetTestPatternName(s_led_app.test_pattern);

        default:
            return "LED ?";
    }
}

/* -------------------------------------------------------------------------- */
/*  app mode -> driver command 변환                                             */
/*                                                                            */
/*  이 함수가 이번 개편의 핵심이다.                                             */
/*  - app은 여기서 "어떤 모드인지"만 고른다.                                  */
/*  - driver는 이 command를 받아 실제 그림을 생성한다.                        */
/* -------------------------------------------------------------------------- */
static void LED_App_BuildDriverCommand(LED_DriverCommand_t *command)
{
    memset(command, 0, sizeof(*command));

    command->mode_started_ms = s_led_app.mode_started_ms;
    command->global_scale_q16 = s_led_app.global_scale_q16;
    command->transition_ms = LED_App_GetTransitionTimeMs();

    switch (s_led_app.mode)
    {
        case LED_APP_MODE_OFF:
            command->pattern = LED_DRIVER_PATTERN_OFF;
            break;

        case LED_APP_MODE_SOLID_ALL:
            command->pattern = LED_DRIVER_PATTERN_SOLID_ALL;
            command->primary_level_q16 = s_led_app.solid_all_level_q16;
            break;

        case LED_APP_MODE_BREATH_IDLE:
            command->pattern = LED_DRIVER_PATTERN_BREATH_CENTER;
            break;

        case LED_APP_MODE_WELCOME_SWEEP:
            command->pattern = LED_DRIVER_PATTERN_WELCOME_SWEEP;
            break;

        case LED_APP_MODE_SCANNER:
            command->pattern = LED_DRIVER_PATTERN_SCANNER;
            break;

        case LED_APP_MODE_ALERT_BLINK:
            command->pattern = LED_DRIVER_PATTERN_ALERT_BLINK;
            break;

        case LED_APP_MODE_SINGLE_DOT:
            command->pattern = LED_DRIVER_PATTERN_SINGLE_DOT;
            command->logical_index = s_led_app.single_dot_index;
            command->primary_level_q16 = s_led_app.single_dot_level_q16;
            break;

        case LED_APP_MODE_CUSTOM_FRAME:
            command->pattern = LED_DRIVER_PATTERN_CUSTOM_FRAME;
            command->custom_frame_q16 = s_led_app.custom_frame_q16;
            break;

        case LED_APP_MODE_TEST_PATTERN:
            command->transition_ms = LED_App_GetTestPatternTransitionTimeMs(
                s_led_app.test_pattern);

            switch (s_led_app.test_pattern)
            {
                case LED_APP_TEST_PATTERN_OFF:
                    command->pattern = LED_DRIVER_PATTERN_OFF;
                    break;

                case LED_APP_TEST_PATTERN_ALL_ON:
                    command->pattern = LED_DRIVER_PATTERN_TEST_ALL_ON;
                    break;

                case LED_APP_TEST_PATTERN_WALK_LEFT_TO_RIGHT:
                    command->pattern = LED_DRIVER_PATTERN_TEST_WALK_LEFT_TO_RIGHT;
                    break;

                case LED_APP_TEST_PATTERN_WALK_RIGHT_TO_LEFT:
                    command->pattern = LED_DRIVER_PATTERN_TEST_WALK_RIGHT_TO_LEFT;
                    break;

                case LED_APP_TEST_PATTERN_SCANNER:
                    command->pattern = LED_DRIVER_PATTERN_TEST_SCANNER;
                    break;

                case LED_APP_TEST_PATTERN_CENTER_OUT:
                    command->pattern = LED_DRIVER_PATTERN_TEST_CENTER_OUT;
                    break;

                case LED_APP_TEST_PATTERN_EDGE_IN:
                    command->pattern = LED_DRIVER_PATTERN_TEST_EDGE_IN;
                    break;

                case LED_APP_TEST_PATTERN_ODD_EVEN:
                    command->pattern = LED_DRIVER_PATTERN_TEST_ODD_EVEN;
                    break;

                case LED_APP_TEST_PATTERN_HALF_SWAP:
                    command->pattern = LED_DRIVER_PATTERN_TEST_HALF_SWAP;
                    break;

                case LED_APP_TEST_PATTERN_BREATH_ALL:
                    command->pattern = LED_DRIVER_PATTERN_TEST_BREATH_ALL;
                    break;

                case LED_APP_TEST_PATTERN_BREATH_CENTER:
                    command->pattern = LED_DRIVER_PATTERN_TEST_BREATH_CENTER;
                    break;

                case LED_APP_TEST_PATTERN_BAR_FILL:
                    command->pattern = LED_DRIVER_PATTERN_TEST_BAR_FILL;
                    break;

                case LED_APP_TEST_PATTERN_STROBE_SLOW:
                    command->pattern = LED_DRIVER_PATTERN_TEST_STROBE_SLOW;
                    break;

                case LED_APP_TEST_PATTERN_COUNT:
                default:
                    command->pattern = LED_DRIVER_PATTERN_OFF;
                    break;
            }
            break;

        default:
            command->pattern = LED_DRIVER_PATTERN_OFF;
            break;
    }
}

void LED_App_Init(void)
{
    memset(&s_led_app, 0, sizeof(s_led_app));

    s_led_app.initialized = 1u;
    s_led_app.mode = LED_APP_MODE_OFF;
    s_led_app.mode_started_ms = HAL_GetTick();
    s_led_app.global_scale_q16 = LED_DRIVER_Q16_MAX;
    s_led_app.default_transition_ms = LED_APP_DEFAULT_TRANSITION_MS;
    s_led_app.solid_all_level_q16 = LED_DRIVER_Q16_MAX;
    s_led_app.single_dot_index = 0u;
    s_led_app.single_dot_level_q16 = LED_DRIVER_Q16_MAX;
    s_led_app.test_pattern = LED_APP_TEST_PATTERN_OFF;

    memset(s_led_app.custom_frame_q16, 0, sizeof(s_led_app.custom_frame_q16));

    LED_Driver_Init();
    LED_Driver_AllOff();
}

void LED_App_Task(uint32_t now_ms)
{
    LED_DriverCommand_t command;

    if (s_led_app.initialized == 0u)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  Welcome sweep 종료 처리                                               */
    /*                                                                        */
    /*  현재 구조에서는 "패턴 그림"은 driver가 담당하지만,                     */
    /*  어떤 mode로 넘어갈지는 app이 결정해야 계층이 깔끔하다.                 */
    /*                                                                        */
    /*  따라서 welcome sweep 시간(1200ms)이 끝나면                            */
    /*  app mode를 breath idle로 명시적으로 넘긴다.                            */
    /* ---------------------------------------------------------------------- */
    if ((s_led_app.mode == LED_APP_MODE_WELCOME_SWEEP) &&
        ((now_ms - s_led_app.mode_started_ms) >= LED_APP_WELCOME_SWEEP_MS))
    {
        LED_App_EnterMode(LED_APP_MODE_BREATH_IDLE, now_ms, 1u);
    }

    LED_App_BuildDriverCommand(&command);
    LED_Driver_RenderCommand(now_ms, &command);
}

LED_AppMode_t LED_App_GetMode(void)
{
    return s_led_app.mode;
}

void LED_App_SetGlobalBrightnessScale(uint16_t scale_permille)
{
    s_led_app.global_scale_q16 = LED_App_PermilleToQ16(scale_permille);
}

void LED_App_SetGlobalBrightnessScaleQ16(uint16_t scale_q16)
{
    s_led_app.global_scale_q16 = LED_App_ClampQ16(scale_q16);
}

void LED_App_SetTransitionTimeMs(uint16_t transition_ms)
{
    s_led_app.default_transition_ms = transition_ms;
}

void LED_Off(void)
{
    LED_App_EnterMode(LED_APP_MODE_OFF, HAL_GetTick(), 1u);
    s_led_app.test_pattern = LED_APP_TEST_PATTERN_OFF;
}

void LED_AllOn(uint16_t brightness_permille)
{
    LED_AllOnQ16(LED_App_PermilleToQ16(brightness_permille));
}

void LED_AllOnQ16(uint16_t brightness_q16)
{
    s_led_app.solid_all_level_q16 = LED_App_ClampQ16(brightness_q16);
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
    LED_SingleDotQ16(logical_led_index, LED_App_PermilleToQ16(brightness_permille));
}

void LED_SingleDotQ16(uint8_t logical_led_index, uint16_t brightness_q16)
{
    s_led_app.single_dot_index = logical_led_index;
    s_led_app.single_dot_level_q16 = LED_App_ClampQ16(brightness_q16);
    LED_App_EnterMode(LED_APP_MODE_SINGLE_DOT, HAL_GetTick(), 0u);
}

void LED_CustomFrame(const uint16_t frame_permille[LED_DRIVER_CHANNEL_COUNT])
{
    uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT];
    uint8_t  index;

    if (frame_permille == NULL)
    {
        return;
    }

    for (index = 0u; index < LED_DRIVER_CHANNEL_COUNT; ++index)
    {
        frame_q16[index] = LED_App_PermilleToQ16(frame_permille[index]);
    }

    LED_CustomFrameQ16(frame_q16);
}

void LED_CustomFrameQ16(const uint16_t frame_q16[LED_DRIVER_CHANNEL_COUNT])
{
    if (frame_q16 == NULL)
    {
        return;
    }

    memcpy(s_led_app.custom_frame_q16,
           frame_q16,
           sizeof(s_led_app.custom_frame_q16));
    LED_App_EnterMode(LED_APP_MODE_CUSTOM_FRAME, HAL_GetTick(), 0u);
}

void LED_App_SetTestPattern(LED_AppTestPattern_t pattern)
{
    if (pattern >= LED_APP_TEST_PATTERN_COUNT)
    {
        pattern = LED_APP_TEST_PATTERN_OFF;
    }

    s_led_app.test_pattern = pattern;

    if (pattern == LED_APP_TEST_PATTERN_OFF)
    {
        LED_Off();
    }
    else
    {
        LED_App_EnterMode(LED_APP_MODE_TEST_PATTERN, HAL_GetTick(), 1u);
    }
}

LED_AppTestPattern_t LED_App_GetTestPattern(void)
{
    return s_led_app.test_pattern;
}

const char *LED_App_AdvanceTestPattern(void)
{
    uint32_t next_pattern;

    next_pattern = ((uint32_t)s_led_app.test_pattern + 1u) %
                   (uint32_t)LED_APP_TEST_PATTERN_COUNT;
    LED_App_SetTestPattern((LED_AppTestPattern_t)next_pattern);

    return LED_App_GetModeName();
}
