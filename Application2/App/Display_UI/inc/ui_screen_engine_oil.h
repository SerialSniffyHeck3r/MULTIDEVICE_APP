#ifndef UI_SCREEN_ENGINE_OIL_MODULE_H
#define UI_SCREEN_ENGINE_OIL_MODULE_H

#include "ui_types.h"
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  Engine oil interval stub screen                                            */
/*                                                                            */
/*  이 화면은 이륜차 정비용 "엔진 오일 교체 주기 설정" 스텁이다.               */
/*  - 상단 제목은 viewport 좌상단 기준으로 배치된다.                           */
/*  - 중앙에는 5자리 숫자를 크게 표시한다.                                     */
/*  - selected_digit_index에 해당하는 자릿수는 별도 박스로 강조된다.           */
/* -------------------------------------------------------------------------- */
void UI_ScreenEngineOil_Draw(u8g2_t *u8g2,
                             const ui_rect_t *viewport,
                             uint32_t interval_value,
                             uint8_t selected_digit_index);

#ifdef __cplusplus
}
#endif

#endif /* UI_SCREEN_ENGINE_OIL_MODULE_H */
