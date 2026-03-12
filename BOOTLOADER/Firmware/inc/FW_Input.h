#ifndef FW_INPUT_H
#define FW_INPUT_H

#include "main.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "FW_BootConfig.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  FW_Input                                                                   */
/*                                                                            */
/*  부트 모드용 입력은 기존 앱의 button.c와 일부 기능이 겹친다.                */
/*  하지만 요구사항상 UI/버튼 로직을 Firmware 폴더 안으로 모으기 위해          */
/*  부트 모드 전용 경량 입력 엔진을 별도로 둔다.                               */
/*                                                                            */
/*  설계 포인트                                                                */
/*  - IRQ/EXTI에 의존하지 않는다.                                             */
/*  - GPIO raw poll + software debounce 만으로 동작한다.                      */
/*  - F1 short abort, F6 long install, KEY4+KEY6 boot chord 를 처리한다.      */
/* -------------------------------------------------------------------------- */

typedef enum
{
    FW_INPUT_BUTTON_NONE = 0u,
    FW_INPUT_BUTTON_1    = 1u,
    FW_INPUT_BUTTON_2    = 2u,
    FW_INPUT_BUTTON_3    = 3u,
    FW_INPUT_BUTTON_4    = 4u,
    FW_INPUT_BUTTON_5    = 5u,
    FW_INPUT_BUTTON_6    = 6u
} fw_input_button_id_t;

typedef enum
{
    FW_INPUT_EVENT_NONE        = 0u,
    FW_INPUT_EVENT_PRESS       = 1u,
    FW_INPUT_EVENT_RELEASE     = 2u,
    FW_INPUT_EVENT_SHORT_PRESS = 3u,
    FW_INPUT_EVENT_LONG_PRESS  = 4u
} fw_input_event_type_t;

typedef struct
{
    fw_input_button_id_t  button_id;
    fw_input_event_type_t event_type;
    uint32_t              tick_ms;
    uint32_t              hold_ms;
} fw_input_event_t;

void FW_INPUT_Init(void);
void FW_INPUT_Task(uint32_t now_ms);
bool FW_INPUT_PopEvent(fw_input_event_t *out_event);
uint32_t FW_INPUT_GetPressedMask(void);
bool FW_INPUT_IsButtonPressed(fw_input_button_id_t button_id);
bool FW_INPUT_IsStartupChordKey4Key6Pressed(void);

#ifdef __cplusplus
}
#endif

#endif /* FW_INPUT_H */
