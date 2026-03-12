#include "FW_Input.h"

#include <string.h>

/* -------------------------------------------------------------------------- */
/*  내부 상수                                                                  */
/* -------------------------------------------------------------------------- */
#define FW_INPUT_BUTTON_COUNT        6u
#define FW_INPUT_EVENT_QUEUE_SIZE    16u

/* -------------------------------------------------------------------------- */
/*  하드웨어 매핑                                                              */
/* -------------------------------------------------------------------------- */
typedef struct
{
    fw_input_button_id_t id;
    GPIO_TypeDef        *port;
    uint16_t             pin;
} fw_input_hw_map_t;

typedef struct
{
    uint8_t  sampled_pressed;
    uint8_t  stable_pressed;
    uint8_t  debounce_active;
    uint8_t  long_reported;
    uint32_t debounce_due_ms;
    uint32_t press_start_ms;
} fw_input_button_runtime_t;

static const fw_input_hw_map_t s_fw_input_hw_map[FW_INPUT_BUTTON_COUNT] =
{
    { FW_INPUT_BUTTON_1, BUTTON1_GPIO_Port, BUTTON1_Pin },
    { FW_INPUT_BUTTON_2, BUTTON2_GPIO_Port, BUTTON2_Pin },
    { FW_INPUT_BUTTON_3, BUTTON3_GPIO_Port, BUTTON3_Pin },
    { FW_INPUT_BUTTON_4, BUTTON4_GPIO_Port, BUTTON4_Pin },
    { FW_INPUT_BUTTON_5, BUTTON5_GPIO_Port, BUTTON5_Pin },
    { FW_INPUT_BUTTON_6, BUTTON6_GPIO_Port, BUTTON6_Pin }
};

static fw_input_button_runtime_t s_fw_input_rt[FW_INPUT_BUTTON_COUNT];
static fw_input_event_t          s_fw_input_queue[FW_INPUT_EVENT_QUEUE_SIZE];
static uint8_t                   s_fw_input_queue_head = 0u;
static uint8_t                   s_fw_input_queue_tail = 0u;

/* -------------------------------------------------------------------------- */
/*  내부 helper: due 판정                                                      */
/* -------------------------------------------------------------------------- */
static uint8_t FW_INPUT_TimeDue(uint32_t now_ms, uint32_t due_ms)
{
    return ((int32_t)(now_ms - due_ms) >= 0) ? 1u : 0u;
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: button id -> index                                            */
/* -------------------------------------------------------------------------- */
static int32_t FW_INPUT_ButtonIdToIndex(fw_input_button_id_t button_id)
{
    if ((button_id < FW_INPUT_BUTTON_1) || (button_id > FW_INPUT_BUTTON_6))
    {
        return -1;
    }

    return (int32_t)button_id - 1;
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: 물리 레벨 읽기                                                */
/* -------------------------------------------------------------------------- */
static uint8_t FW_INPUT_ReadPhysicalPressed(uint32_t index)
{
    GPIO_PinState pin_state;

    pin_state = HAL_GPIO_ReadPin(s_fw_input_hw_map[index].port,
                                 s_fw_input_hw_map[index].pin);

    return (pin_state == FW_INPUT_ACTIVE_LEVEL) ? 1u : 0u;
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: 이벤트 queue push                                             */
/* -------------------------------------------------------------------------- */
static void FW_INPUT_EventPush(fw_input_button_id_t button_id,
                               fw_input_event_type_t event_type,
                               uint32_t tick_ms,
                               uint32_t hold_ms)
{
    uint8_t next_head;

    next_head = (uint8_t)(s_fw_input_queue_head + 1u);
    if (next_head >= FW_INPUT_EVENT_QUEUE_SIZE)
    {
        next_head = 0u;
    }

    if (next_head == s_fw_input_queue_tail)
    {
        s_fw_input_queue_tail = (uint8_t)(s_fw_input_queue_tail + 1u);
        if (s_fw_input_queue_tail >= FW_INPUT_EVENT_QUEUE_SIZE)
        {
            s_fw_input_queue_tail = 0u;
        }
    }

    s_fw_input_queue[s_fw_input_queue_head].button_id  = button_id;
    s_fw_input_queue[s_fw_input_queue_head].event_type = event_type;
    s_fw_input_queue[s_fw_input_queue_head].tick_ms    = tick_ms;
    s_fw_input_queue[s_fw_input_queue_head].hold_ms    = hold_ms;
    s_fw_input_queue_head = next_head;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: init                                                             */
/* -------------------------------------------------------------------------- */
void FW_INPUT_Init(void)
{
    uint32_t now_ms;
    uint32_t index;

    memset(s_fw_input_rt, 0, sizeof(s_fw_input_rt));
    memset(s_fw_input_queue, 0, sizeof(s_fw_input_queue));
    s_fw_input_queue_head = 0u;
    s_fw_input_queue_tail = 0u;

    now_ms = HAL_GetTick();

    for (index = 0u; index < FW_INPUT_BUTTON_COUNT; index++)
    {
        s_fw_input_rt[index].sampled_pressed = FW_INPUT_ReadPhysicalPressed(index);
        s_fw_input_rt[index].stable_pressed  = s_fw_input_rt[index].sampled_pressed;
        s_fw_input_rt[index].press_start_ms  = now_ms;
    }
}

/* -------------------------------------------------------------------------- */
/*  공개 API: polling task                                                     */
/* -------------------------------------------------------------------------- */
void FW_INPUT_Task(uint32_t now_ms)
{
    uint32_t index;

    for (index = 0u; index < FW_INPUT_BUTTON_COUNT; index++)
    {
        fw_input_button_runtime_t *rt;
        uint8_t current_pressed;

        rt = &s_fw_input_rt[index];
        current_pressed = FW_INPUT_ReadPhysicalPressed(index);

        /* ------------------------------------------------------------------ */
        /*  raw 상태가 바뀌면 debounce 시작                                     */
        /* ------------------------------------------------------------------ */
        if (current_pressed != rt->sampled_pressed)
        {
            rt->sampled_pressed = current_pressed;
            rt->debounce_active = 1u;
            rt->debounce_due_ms = now_ms + FW_INPUT_DEBOUNCE_MS;
        }

        /* ------------------------------------------------------------------ */
        /*  debounce 만료 후 stable 상태 확정                                   */
        /* ------------------------------------------------------------------ */
        if ((rt->debounce_active != 0u) &&
            (FW_INPUT_TimeDue(now_ms, rt->debounce_due_ms) != 0u))
        {
            rt->debounce_active = 0u;

            if (rt->stable_pressed != rt->sampled_pressed)
            {
                rt->stable_pressed = rt->sampled_pressed;

                if (rt->stable_pressed != 0u)
                {
                    rt->press_start_ms = now_ms;
                    rt->long_reported  = 0u;
                    FW_INPUT_EventPush(s_fw_input_hw_map[index].id,
                                       FW_INPUT_EVENT_PRESS,
                                       now_ms,
                                       0u);
                }
                else
                {
                    uint32_t hold_ms;

                    hold_ms = now_ms - rt->press_start_ms;

                    FW_INPUT_EventPush(s_fw_input_hw_map[index].id,
                                       FW_INPUT_EVENT_RELEASE,
                                       now_ms,
                                       hold_ms);

                    if (rt->long_reported == 0u)
                    {
                        FW_INPUT_EventPush(s_fw_input_hw_map[index].id,
                                           FW_INPUT_EVENT_SHORT_PRESS,
                                           now_ms,
                                           hold_ms);
                    }
                }
            }
        }

        /* ------------------------------------------------------------------ */
        /*  stable pressed 유지 중이면 long press 1회 발생                     */
        /* ------------------------------------------------------------------ */
        if ((rt->stable_pressed != 0u) &&
            (rt->long_reported == 0u) &&
            (FW_INPUT_TimeDue(now_ms, rt->press_start_ms + FW_INPUT_LONG_PRESS_MS) != 0u))
        {
            rt->long_reported = 1u;
            FW_INPUT_EventPush(s_fw_input_hw_map[index].id,
                               FW_INPUT_EVENT_LONG_PRESS,
                               now_ms,
                               now_ms - rt->press_start_ms);
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  공개 API: event pop                                                        */
/* -------------------------------------------------------------------------- */
bool FW_INPUT_PopEvent(fw_input_event_t *out_event)
{
    if (out_event == 0)
    {
        return false;
    }

    if (s_fw_input_queue_head == s_fw_input_queue_tail)
    {
        memset(out_event, 0, sizeof(*out_event));
        return false;
    }

    *out_event = s_fw_input_queue[s_fw_input_queue_tail];

    s_fw_input_queue_tail = (uint8_t)(s_fw_input_queue_tail + 1u);
    if (s_fw_input_queue_tail >= FW_INPUT_EVENT_QUEUE_SIZE)
    {
        s_fw_input_queue_tail = 0u;
    }

    return true;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: pressed mask                                                     */
/* -------------------------------------------------------------------------- */
uint32_t FW_INPUT_GetPressedMask(void)
{
    uint32_t mask;
    uint32_t index;

    mask = 0u;

    for (index = 0u; index < FW_INPUT_BUTTON_COUNT; index++)
    {
        if (s_fw_input_rt[index].stable_pressed != 0u)
        {
            mask |= (1u << index);
        }
    }

    return mask;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: stable 눌림 확인                                                 */
/* -------------------------------------------------------------------------- */
bool FW_INPUT_IsButtonPressed(fw_input_button_id_t button_id)
{
    int32_t index;

    index = FW_INPUT_ButtonIdToIndex(button_id);
    if (index < 0)
    {
        return false;
    }

    return (s_fw_input_rt[index].stable_pressed != 0u) ? true : false;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: power-on chord                                                   */
/*                                                                            */
/*  KEY4 + KEY6 동시 눌림을 부트 직후 빠르게 확인한다.                         */
/*  이 helper는 debounce queue를 쓰지 않고 raw 상태를 바로 본다.              */
/* -------------------------------------------------------------------------- */
bool FW_INPUT_IsStartupChordKey4Key6Pressed(void)
{
    uint8_t key4_pressed;
    uint8_t key6_pressed;

    key4_pressed = FW_INPUT_ReadPhysicalPressed(3u);
    key6_pressed = FW_INPUT_ReadPhysicalPressed(5u);

    return ((key4_pressed != 0u) && (key6_pressed != 0u)) ? true : false;
}
