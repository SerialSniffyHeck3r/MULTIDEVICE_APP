#include "button.h"

#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*  내부 상수                                                                  */
/* -------------------------------------------------------------------------- */

#define BUTTON_COUNT 6u

/* -------------------------------------------------------------------------- */
/*  하드웨어 매핑 테이블                                                       */
/*                                                                            */
/*  BUTTON1~BUTTON6의 GPIO 포트/핀을 한 표로 정리해 두면,                      */
/*  나머지 로직은 인덱스 기반으로 아주 단순하게 작성할 수 있다.                */
/* -------------------------------------------------------------------------- */
typedef struct
{
    button_id_t  id;
    GPIO_TypeDef *port;
    uint16_t     pin;
} button_hw_map_t;

/* -------------------------------------------------------------------------- */
/*  버튼별 런타임 상태                                                         */
/*                                                                            */
/*  debounce_pending : EXTI가 들어왔고, 아직 debounce 만료 시각이 안 지난 상태 */
/*  debounce_due_ms  : 이 시각 이후에 한 번 샘플해서 안정 상태를 확정한다.     */
/*  stable_pressed   : debounce까지 끝난 현재 논리적 눌림 상태                 */
/*  long_reported    : 이번 눌림 구간에서 LONG_PRESS를 이미 발행했는가         */
/*  press_start_ms   : 눌림이 안정적으로 확정된 시각                           */
/* -------------------------------------------------------------------------- */
typedef struct
{
    volatile uint8_t  debounce_pending;
    volatile uint32_t debounce_due_ms;

    uint8_t  stable_pressed;
    uint8_t  long_reported;
    uint32_t press_start_ms;
} button_runtime_t;

/* -------------------------------------------------------------------------- */
/*  정적 저장소                                                                */
/* -------------------------------------------------------------------------- */

static const button_hw_map_t s_button_hw_map[BUTTON_COUNT] =
{
    { BUTTON_ID_1, BUTTON1_GPIO_Port, BUTTON1_Pin },
    { BUTTON_ID_2, BUTTON2_GPIO_Port, BUTTON2_Pin },
    { BUTTON_ID_3, BUTTON3_GPIO_Port, BUTTON3_Pin },
    { BUTTON_ID_4, BUTTON4_GPIO_Port, BUTTON4_Pin },
    { BUTTON_ID_5, BUTTON5_GPIO_Port, BUTTON5_Pin },
    { BUTTON_ID_6, BUTTON6_GPIO_Port, BUTTON6_Pin }
};

static button_runtime_t s_button_rt[BUTTON_COUNT];

/* -------------------------------------------------------------------------- */
/*  이벤트 큐                                                                  */
/*                                                                            */
/*  버튼 이벤트는 ISR에서 직접 push하지 않고 main context에서만 push한다.      */
/*  그래서 queue 구현도 매우 단순한 ring buffer로 충분하다.                   */
/* -------------------------------------------------------------------------- */
static button_event_t s_button_event_queue[BUTTON_EVENT_QUEUE_SIZE];
static uint8_t        s_button_event_head = 0u;
static uint8_t        s_button_event_tail = 0u;

/* -------------------------------------------------------------------------- */
/*  내부 유틸: tick wrap-safe due 판정                                         */
/* -------------------------------------------------------------------------- */
static uint8_t Button_TimeDue(uint32_t now_ms, uint32_t due_ms)
{
    return ((int32_t)(now_ms - due_ms) >= 0) ? 1u : 0u;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: button_id -> 배열 인덱스 변환                                   */
/* -------------------------------------------------------------------------- */
static int32_t Button_IdToIndex(button_id_t button_id)
{
    if ((button_id < BUTTON_ID_1) || (button_id > BUTTON_ID_6))
    {
        return -1;
    }

    return (int32_t)button_id - 1;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: GPIO_Pin -> 배열 인덱스 변환                                    */
/*                                                                            */
/*  ISR 경로에서는 불필요한 for-loop를 돌리지 않게 switch로 바로 매핑한다.     */
/* -------------------------------------------------------------------------- */
static int32_t Button_GpioPinToIndex(uint16_t gpio_pin)
{
    switch (gpio_pin)
    {
    case BUTTON1_Pin: return 0;
    case BUTTON2_Pin: return 1;
    case BUTTON3_Pin: return 2;
    case BUTTON4_Pin: return 3;
    case BUTTON5_Pin: return 4;
    case BUTTON6_Pin: return 5;
    default:          return -1;
    }
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 현재 하드웨어 입력을 "논리적 눌림" 으로 읽기                    */
/*                                                                            */
/*  active level이 LOW라면                                                     */
/*    - GPIO_PIN_RESET -> 눌림                                                 */
/*    - GPIO_PIN_SET   -> 안 눌림                                              */
/*  이 된다.                                                                  */
/* -------------------------------------------------------------------------- */
static uint8_t Button_ReadPhysicalPressed(uint32_t index)
{
    GPIO_PinState pin_state;

    pin_state = HAL_GPIO_ReadPin(s_button_hw_map[index].port,
                                 s_button_hw_map[index].pin);

    return (pin_state == BUTTON_ACTIVE_LEVEL) ? 1u : 0u;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 이벤트 큐 push                                                  */
/*                                                                            */
/*  queue가 가득 찼을 때는 "가장 오래된 이벤트" 를 버리고                      */
/*  "가장 최신 이벤트" 를 살린다.                                             */
/*                                                                            */
/*  이유                                                                      */
/*  - 사용자 입력 디버깅에서는 오래된 이벤트보다 최신 상태가 더 중요하다.      */
/*  - queue overflow가 나더라도 시스템이 멈추지 않고 자연스럽게 흘러가야 한다. */
/* -------------------------------------------------------------------------- */
static void Button_EventQueuePush(button_id_t button_id,
                                  button_event_type_t event_type,
                                  uint32_t tick_ms,
                                  uint32_t hold_ms)
{
    button_event_t event;
    uint8_t next_head;

    event.id      = button_id;
    event.type    = event_type;
    event.tick_ms = tick_ms;
    event.hold_ms = hold_ms;

    next_head = (uint8_t)(s_button_event_head + 1u);
    if (next_head >= BUTTON_EVENT_QUEUE_SIZE)
    {
        next_head = 0u;
    }

    if (next_head == s_button_event_tail)
    {
        s_button_event_tail = (uint8_t)(s_button_event_tail + 1u);
        if (s_button_event_tail >= BUTTON_EVENT_QUEUE_SIZE)
        {
            s_button_event_tail = 0u;
        }
    }

    s_button_event_queue[s_button_event_head] = event;
    s_button_event_head = next_head;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 초기화                                                            */
/*                                                                            */
/*  부팅 시점의 현재 입력을 읽어서                                             */
/*  "이미 눌려 있는 버튼이 있는가" 까지 baseline으로 반영한다.                */
/* -------------------------------------------------------------------------- */
void Button_Init(void)
{
    uint32_t now_ms;
    uint32_t index;

    memset(s_button_rt, 0, sizeof(s_button_rt));
    memset(s_button_event_queue, 0, sizeof(s_button_event_queue));

    s_button_event_head = 0u;
    s_button_event_tail = 0u;
    now_ms = HAL_GetTick();

    for (index = 0u; index < BUTTON_COUNT; index++)
    {
        s_button_rt[index].stable_pressed = Button_ReadPhysicalPressed(index);
        s_button_rt[index].long_reported  = 0u;
        s_button_rt[index].press_start_ms = now_ms;
    }
}

/* -------------------------------------------------------------------------- */
/*  공개 API: EXTI ISR 진입점                                                   */
/*                                                                            */
/*  여기서 하는 일은 2개뿐이다.                                                */
/*  1) 어떤 버튼에서 edge가 들어왔는지 식별                                    */
/*  2) debounce 만료 시각을 "지금부터 N ms 뒤" 로 갱신                        */
/*                                                                            */
/*  실제 GPIO 재샘플 / short/long 판정은 절대 ISR에서 하지 않는다.             */
/* -------------------------------------------------------------------------- */
void Button_OnExtiInterrupt(uint16_t gpio_pin)
{
    int32_t index;
    uint32_t now_ms;

    index = Button_GpioPinToIndex(gpio_pin);
    if (index < 0)
    {
        return;
    }

    now_ms = HAL_GetTick();

    s_button_rt[index].debounce_due_ms = now_ms + BUTTON_DEBOUNCE_MS;
    s_button_rt[index].debounce_pending = 1u;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 주기 처리                                                         */
/*                                                                            */
/*  이 함수는 main loop에서 가볍게 반복 호출된다.                               */
/*                                                                            */
/*  처리 순서                                                                  */
/*  1) debounce 만료된 버튼이 있으면 현재 GPIO를 한 번 읽어서 안정 상태 확정    */
/*  2) 눌림 시작이면 PRESS 이벤트 push                                         */
/*  3) 떼기면 RELEASE + SHORT/LONG 이벤트 push                                 */
/*  4) 아직 눌린 채 유지 중이면 LONG_PRESS 시간 도달 여부를 검사               */
/*                                                                            */
/*  long press는 "한 번만" 발생한다.                                          */
/* -------------------------------------------------------------------------- */
void Button_Task(uint32_t now_ms)
{
    uint32_t index;

    for (index = 0u; index < BUTTON_COUNT; index++)
    {
        button_runtime_t *runtime;
        uint8_t debounce_pending_snapshot;
        uint32_t debounce_due_ms_snapshot;
        uint8_t should_sample_now;

        runtime = &s_button_rt[index];
        should_sample_now = 0u;

        /* ------------------------------------------------------------------ */
        /*  ISR와 공유하는 debounce flag는 아주 짧은 임계 구역에서만 읽고 지운다. */
        /* ------------------------------------------------------------------ */
        __disable_irq();

        debounce_pending_snapshot = runtime->debounce_pending;
        debounce_due_ms_snapshot  = runtime->debounce_due_ms;

        if ((debounce_pending_snapshot != 0u) &&
            (Button_TimeDue(now_ms, debounce_due_ms_snapshot) != 0u))
        {
            runtime->debounce_pending = 0u;
            debounce_pending_snapshot = 0u;
            should_sample_now = 1u;
        }

        __enable_irq();

        /* ------------------------------------------------------------------ */
        /*  debounce 기간이 끝난 버튼만 현재 GPIO를 1회 읽어서 안정 상태를 확정 */
        /* ------------------------------------------------------------------ */
        if (should_sample_now != 0u)
        {
            uint8_t sampled_pressed;

            sampled_pressed = Button_ReadPhysicalPressed(index);

            /* -------------------------------------------------------------- */
            /*  debounce 후 상태가 실제로 바뀐 경우에만 이벤트를 만든다.        */
            /* -------------------------------------------------------------- */
            if (sampled_pressed != runtime->stable_pressed)
            {
                if (sampled_pressed != 0u)
                {
                    /* ------------------------------------------------------ */
                    /*  안정된 눌림 시작                                        */
                    /* ------------------------------------------------------ */
                    runtime->stable_pressed = 1u;
                    runtime->press_start_ms = now_ms;
                    runtime->long_reported  = 0u;

                    Button_EventQueuePush(s_button_hw_map[index].id,
                                          BUTTON_EVENT_PRESS,
                                          now_ms,
                                          0u);
                }
                else
                {
                    uint32_t hold_ms;

                    /* ------------------------------------------------------ */
                    /*  안정된 떼기                                            */
                    /*                                                        */
                    /*  long press가 아직 안 나갔는데 실제 눌림 시간이         */
                    /*  threshold를 이미 넘었다면 release 시점에 long으로      */
                    /*  승격해서 처리한다.                                     */
                    /* ------------------------------------------------------ */
                    hold_ms = now_ms - runtime->press_start_ms;
                    runtime->stable_pressed = 0u;

                    if ((runtime->long_reported == 0u) &&
                        (hold_ms >= BUTTON_LONG_PRESS_MS))
                    {
                        runtime->long_reported = 1u;

                        Button_EventQueuePush(s_button_hw_map[index].id,
                                              BUTTON_EVENT_LONG_PRESS,
                                              now_ms,
                                              hold_ms);
                    }

                    Button_EventQueuePush(s_button_hw_map[index].id,
                                          BUTTON_EVENT_RELEASE,
                                          now_ms,
                                          hold_ms);

                    if (runtime->long_reported == 0u)
                    {
                        Button_EventQueuePush(s_button_hw_map[index].id,
                                              BUTTON_EVENT_SHORT_PRESS,
                                              now_ms,
                                              hold_ms);
                    }

                    /* ------------------------------------------------------ */
                    /*  다음 눌림 구간을 위해 long_reported를 초기화한다.       */
                    /* ------------------------------------------------------ */
                    runtime->long_reported = 0u;
                }
            }
        }

        /* ------------------------------------------------------------------ */
        /*  long press 실시간 판정                                              */
        /*                                                                    */
        /*  release bounce가 막 들어와 debounce 대기 중인 구간에서는           */
        /*  false long을 피하기 위해 long 판정을 잠시 멈춘다.                  */
        /* ------------------------------------------------------------------ */
        if ((runtime->stable_pressed != 0u) &&
            (runtime->long_reported == 0u))
        {
            __disable_irq();
            debounce_pending_snapshot = runtime->debounce_pending;
            __enable_irq();

            if ((debounce_pending_snapshot == 0u) &&
                (Button_TimeDue(now_ms,
                                runtime->press_start_ms + BUTTON_LONG_PRESS_MS) != 0u))
            {
                runtime->long_reported = 1u;

                Button_EventQueuePush(s_button_hw_map[index].id,
                                      BUTTON_EVENT_LONG_PRESS,
                                      now_ms,
                                      now_ms - runtime->press_start_ms);
            }
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  공개 API: event pop                                                        */
/* -------------------------------------------------------------------------- */
bool Button_PopEvent(button_event_t *out_event)
{
    if (out_event == 0)
    {
        return false;
    }

    if (s_button_event_tail == s_button_event_head)
    {
        return false;
    }

    *out_event = s_button_event_queue[s_button_event_tail];

    s_button_event_tail = (uint8_t)(s_button_event_tail + 1u);
    if (s_button_event_tail >= BUTTON_EVENT_QUEUE_SIZE)
    {
        s_button_event_tail = 0u;
    }

    return true;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 현재 안정 눌림 상태 읽기                                          */
/* -------------------------------------------------------------------------- */
bool Button_IsPressed(button_id_t button_id)
{
    int32_t index;

    index = Button_IdToIndex(button_id);
    if (index < 0)
    {
        return false;
    }

    return (s_button_rt[index].stable_pressed != 0u) ? true : false;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 현재 눌린 버튼 bitmask 읽기                                       */
/* -------------------------------------------------------------------------- */
uint32_t Button_GetPressedMask(void)
{
    uint32_t index;
    uint32_t pressed_mask;

    pressed_mask = 0u;

    for (index = 0u; index < BUTTON_COUNT; index++)
    {
        if (s_button_rt[index].stable_pressed != 0u)
        {
            pressed_mask |= (uint32_t)(1u << index);
        }
    }

    return pressed_mask;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: event type -> 짧은 문자열                                         */
/* -------------------------------------------------------------------------- */
const char *Button_GetEventTypeText(button_event_type_t type)
{
    switch (type)
    {
    case BUTTON_EVENT_PRESS:
        return "PRESS";

    case BUTTON_EVENT_RELEASE:
        return "RELEASE";

    case BUTTON_EVENT_SHORT_PRESS:
        return "SHORT";

    case BUTTON_EVENT_LONG_PRESS:
        return "LONG";

    case BUTTON_EVENT_NONE:
    default:
        return "NONE";
    }
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 현재 눌린 버튼 번호열 만들기                                       */
/*                                                                            */
/*  예) BUTTON1, BUTTON3, BUTTON5가 눌려 있으면 "135"                         */
/*      하나도 없으면 "-"                                                     */
/* -------------------------------------------------------------------------- */
void Button_BuildPressedDigits(char *out_text, size_t out_size)
{
    size_t write_index;
    uint32_t index;

    if ((out_text == 0) || (out_size == 0u))
    {
        return;
    }

    out_text[0] = '\0';
    write_index = 0u;

    for (index = 0u; index < BUTTON_COUNT; index++)
    {
        if (s_button_rt[index].stable_pressed != 0u)
        {
            if ((write_index + 1u) < out_size)
            {
                out_text[write_index++] = (char)('1' + index);
                out_text[write_index] = '\0';
            }
        }
    }

    if (write_index == 0u)
    {
        snprintf(out_text, out_size, "-");
    }
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 이벤트를 표시용 짧은 문자열로 변환                                */
/*                                                                            */
/*  예) SHORT2, LONG6, PRESS1, RELEASE4                                       */
/* -------------------------------------------------------------------------- */
void Button_BuildEventText(const button_event_t *event,
                           char *out_text,
                           size_t out_size)
{
    if ((out_text == 0) || (out_size == 0u))
    {
        return;
    }

    if ((event == 0) ||
        (event->type == BUTTON_EVENT_NONE) ||
        (event->id == BUTTON_ID_NONE))
    {
        snprintf(out_text, out_size, "NONE");
        return;
    }

    snprintf(out_text,
             out_size,
             "%s%u",
             Button_GetEventTypeText(event->type),
             (unsigned)event->id);
}
