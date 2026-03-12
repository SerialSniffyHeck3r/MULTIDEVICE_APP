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
/*  필요한 최소 필드만 담은 경량 모델을 받아서 그린다.                         */
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

    bool                       clock_valid;
    uint16_t                   clock_year;
    uint8_t                    clock_month;
    uint8_t                    clock_day;
    uint8_t                    clock_hour;
    uint8_t                    clock_minute;
    uint8_t                    clock_second;
    uint8_t                    clock_weekday;   /* APP_CLOCK 계층의 1..7, Monday=1 */

    ui_record_state_t          record_state;
    uint8_t                    imperial_units;
    ui_bluetooth_stub_state_t  bluetooth_stub_state;
    uint8_t                    bluetooth_aux_visible;
} ui_statusbar_model_t;

/* 20Hz ISR에서 생성되는 깜빡임 상태.
 * 기존 프로토타입과 이름을 같게 유지해서 이식성을 높인다. */
extern volatile bool SlowToggle2Hz;
extern volatile bool FastToggle5Hz;

/* 상단바 전체를 240x7 절대 좌표 규격으로 그린다. */
void UI_StatusBar_Draw(u8g2_t *u8g2,
                       const ui_statusbar_model_t *model,
                       uint32_t now_ms);

#ifdef __cplusplus
}
#endif

#endif /* UI_STATUSBAR_MODULE_H */
