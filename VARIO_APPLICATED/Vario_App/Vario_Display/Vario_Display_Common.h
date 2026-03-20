#ifndef VARIO_DISPLAY_COMMON_H
#define VARIO_DISPLAY_COMMON_H

#include "u8g2.h"
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* VARIO 내부 viewport 구조체                                                  */
/*                                                                            */
/* UI 엔진의 ui_rect_t 와 의미는 같지만,                                       */
/* VARIO 폴더가 UI 엔진 타입에 직접 의존하지 않도록 별도 타입을 유지한다.       */
/* -------------------------------------------------------------------------- */
typedef struct
{
    int16_t x;
    int16_t y;
    int16_t w;
    int16_t h;
} vario_viewport_t;

/* -------------------------------------------------------------------------- */
/* Viewport publish API                                                        */
/*                                                                            */
/* 이 API 는 "UI 엔진 bridge" 가 현재 프레임에서 VARIO renderer 가 사용할        */
/* full/content viewport 를 주입하는 용도이다.                                  */
/*                                                                            */
/* - full viewport    : full-screen main page 가 쓰는 기준 영역                */
/* - content viewport : status/bottom bar 를 제외한 setting 계열 본문 영역     */
/* -------------------------------------------------------------------------- */
void Vario_Display_SetViewports(const vario_viewport_t *full_viewport,
                                const vario_viewport_t *content_viewport);

const vario_viewport_t *Vario_Display_GetFullViewport(void);
const vario_viewport_t *Vario_Display_GetContentViewport(void);

/* -------------------------------------------------------------------------- */
/* 텍스트 / 공용 행 helper                                                     */
/* -------------------------------------------------------------------------- */
void Vario_Display_DrawPageTitle(u8g2_t *u8g2,
                                 const vario_viewport_t *v,
                                 const char *title,
                                 const char *subtitle);

void Vario_Display_DrawMenuRow(u8g2_t *u8g2,
                               const vario_viewport_t *v,
                               int16_t y_baseline,
                               bool selected,
                               const char *label,
                               const char *value);

void Vario_Display_DrawKeyValueRow(u8g2_t *u8g2,
                                   const vario_viewport_t *v,
                                   int16_t y_baseline,
                                   const char *label,
                                   const char *value);

void Vario_Display_DrawTextRight(u8g2_t *u8g2,
                                 int16_t right_x,
                                 int16_t y_baseline,
                                 const char *text);

void Vario_Display_DrawTextCentered(u8g2_t *u8g2,
                                    int16_t center_x,
                                    int16_t y_baseline,
                                    const char *text);

/* -------------------------------------------------------------------------- */
/* 개발용 raw overlay                                                          */
/* -------------------------------------------------------------------------- */
void Vario_Display_DrawRawOverlay(u8g2_t *u8g2, const vario_viewport_t *v);

#ifdef __cplusplus
}
#endif

#endif /* VARIO_DISPLAY_COMMON_H */
