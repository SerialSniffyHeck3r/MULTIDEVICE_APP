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
/* Screen 1/2/3 공통 Flight UI page type                                      */
/*                                                                            */
/* SCREEN_1       : 원형 나침반 + to START / to WP + 하단 수치                 */
/* SCREEN_2_TRAIL : 원형 나침반은 제거하고 breadcrumb trail 을 배경에 렌더      */
/* SCREEN_3_STUB  : 실제 기능은 아직 비워 두되, 사이드 바/상단/하단 shell 유지 */
/* -------------------------------------------------------------------------- */
typedef enum
{
    VARIO_FLIGHT_PAGE_SCREEN_1 = 0u,
    VARIO_FLIGHT_PAGE_SCREEN_2_TRAIL,
    VARIO_FLIGHT_PAGE_SCREEN_3_STUB
} vario_flight_page_mode_t;

/* -------------------------------------------------------------------------- */
/* 나침반 target mode                                                         */
/*                                                                            */
/* START : trail buffer 의 가장 오래된 점을 target 으로 사용                   */
/* WP    : 수동 waypoint 좌표를 target 으로 사용                               */
/* -------------------------------------------------------------------------- */
typedef enum
{
    VARIO_NAV_TARGET_START = 0u,
    VARIO_NAV_TARGET_WP
} vario_nav_target_mode_t;

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
/* 공통 Flight UI renderer                                                     */
/*                                                                            */
/* Screen1/2/3 wrapper 는 이 함수를 호출한다.                                  */
/* 실제 계기판 shell, 사이드 바, 숫자, 나침반, breadcrumb trail 은 이 함수가   */
/* 한 곳에서 관리한다.                                                         */
/* -------------------------------------------------------------------------- */
void Vario_Display_RenderFlightPage(u8g2_t *u8g2, vario_flight_page_mode_t mode);

/* -------------------------------------------------------------------------- */
/* Dynamic UI control                                                          */
/*                                                                            */
/* - target mode 는 추후 버튼/설정에서 바꾸기 쉽도록 setter 를 노출한다.       */
/* - manual WP 좌표도 display 계층 내부에서 바로 바꿀 수 있게 분리한다.        */
/* -------------------------------------------------------------------------- */
void Vario_Display_SetNavTargetMode(vario_nav_target_mode_t mode);
vario_nav_target_mode_t Vario_Display_GetNavTargetMode(void);
void Vario_Display_SetWaypointManual(int32_t lat_e7, int32_t lon_e7, bool valid);
void Vario_Display_ResetDynamicMetrics(void);

/* -------------------------------------------------------------------------- */
/* 개발용 raw overlay                                                          */
/* -------------------------------------------------------------------------- */
void Vario_Display_DrawRawOverlay(u8g2_t *u8g2, const vario_viewport_t *v);

#ifdef __cplusplus
}
#endif

#endif /* VARIO_DISPLAY_COMMON_H */
