#ifndef UI_STATUSBAR_MODULE_H
#define UI_STATUSBAR_MODULE_H

#include "ui_types.h"
#include "APP_STATE.h"
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  Status bar model                                                           */
/*                                                                            */
/*  상단바는 매 프레임 전체 APP_STATE를 통째로 복사하지 않고,                   */
/*  필요한 최소 필드만 담은 경량 모델만 받아서 그린다.                         */
/* -------------------------------------------------------------------------- */
typedef struct
{
    gps_fix_basic_t            gps_fix;

    uint8_t                    temp_status_flags;
    int16_t                    temp_c_x100;
    int16_t                    temp_f_x100;
    uint8_t                    temp_last_error;

    bool                       sd_inserted;
    bool                       sd_mounted;
    bool                       sd_initialized;

    bool                       time_valid;
    uint16_t                   time_year;
    uint8_t                    time_month;
    uint8_t                    time_day;
    uint8_t                    time_hour;
    uint8_t                    time_minute;
    uint8_t                    time_weekday;   /* 기존 프로젝트 규칙: 0=SUN ... 6=SAT */

    ui_record_state_t          record_state;
    uint8_t                    imperial_units;
    ui_bluetooth_stub_state_t  bluetooth_stub_state;
} ui_statusbar_model_t;

/* -------------------------------------------------------------------------- */
/*  Blink state                                                                */
/*                                                                            */
/*  기존 프로토타입과 이름을 맞춰서 다른 코드에서 그대로 재사용 가능하게 둔다. */
/* -------------------------------------------------------------------------- */
extern volatile bool SlowToggle2Hz;
extern volatile bool FastToggle5Hz;

/* -------------------------------------------------------------------------- */
/*  Geometry helpers                                                           */
/*                                                                            */
/*  명목상 상단바 높이는 7px이지만, 실제로는 6x12 폰트 descender가               */
/*  아래로 더 내려온다.                                                        */
/*                                                                            */
/*  이 함수는 현재 폰트 metric을 기준으로 "본문이 침범하면 안 되는"             */
/*  실제 점유 높이를 계산해 반환한다.                                          */
/* -------------------------------------------------------------------------- */
uint8_t UI_StatusBar_GetReservedHeight(u8g2_t *u8g2);

/* -------------------------------------------------------------------------- */
/*  Draw                                                                       */
/* -------------------------------------------------------------------------- */
void UI_StatusBar_Draw(u8g2_t *u8g2,
                       const ui_statusbar_model_t *model,
                       uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* UI_STATUSBAR_MODULE_H */
