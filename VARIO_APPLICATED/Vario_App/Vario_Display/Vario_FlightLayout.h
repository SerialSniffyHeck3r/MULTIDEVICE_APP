#ifndef VARIO_FLIGHT_LAYOUT_H
#define VARIO_FLIGHT_LAYOUT_H

#include "Vario_Display_Common.h"
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* FLYTEC-style flight-page layout constants                                   */
/*                                                                            */
/* 이 헤더는 PAGE 1 / PAGE 2 에 공통으로 쓰는 비행 화면 레이아웃 상수 모음이다. */
/*                                                                            */
/* 매우 중요                                                                  */
/* 1) 화면 코드(Screen1/2)는 APP_STATE를 직접 읽지 않는다.                     */
/*    반드시 Vario_State_GetRuntime() 로 받은 vario_runtime_t 만 읽는다.       */
/* 2) APP_STATE -> vario_runtime_t snapshot/가공은 Vario_State.c 가 담당한다.  */
/* 3) 따라서 renderer 는 다음 패턴만 따르면 된다.                             */
/*                                                                            */
/*      const vario_runtime_t *rt = Vario_State_GetRuntime();                 */
/*      // ALT1 : rt->alt1_absolute_m                                         */
/*      // ALT2 : rt->alt2_relative_m                                         */
/*      // ALT3 : rt->alt3_accum_gain_m                                       */
/*      // VARIO: rt->baro_vario_mps                                          */
/*      // AVG  : rt->average_vario_mps                                       */
/*      // GS   : rt->ground_speed_kmh                                        */
/*      // HDG  : rt->heading_deg                                             */
/*      // L/D  : rt->glide_ratio / rt->glide_ratio_valid                     */
/*                                                                            */
/* 새 값을 화면에 추가하고 싶을 때 순서                                         */
/* A. vario_runtime_t 에 field 추가                                            */
/* B. Vario_State.c 에서 APP_STATE snapshot 기반으로 값 계산                   */
/* C. Screen renderer 에서는 그 field 만 읽어 draw                            */
/*                                                                            */
/* 이렇게 하면 CubeMX/IOC 재생성 영향을 APP_STATE 레이어 경계에서 막을 수 있다.*/
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* 기준 해상도                                                                 */
/* - 현재 전체 flight page 는 240x128을 기준으로 정렬되어 있다.               */
/* - 모든 X/Y는 "full viewport 좌상단 기준 offset" 이다.                      */
/* -------------------------------------------------------------------------- */
#define VARIO_FLIGHT_LCD_W                          240
#define VARIO_FLIGHT_LCD_H                          128

/* -------------------------------------------------------------------------- */
/* 좌/우 사이드 바 공통 폭                                                     */
/* - 사용자가 요구한 고정 폭 14px                                              */
/* - PAGE 1 / PAGE 2 모두 동일하게 사용한다.                                  */
/* -------------------------------------------------------------------------- */
#define VARIO_FLIGHT_SIDE_BAR_W                     14
#define VARIO_FLIGHT_SIDE_BAR_FILL_W                8
#define VARIO_FLIGHT_SIDE_BAR_AVG_W                 2
#define VARIO_FLIGHT_SIDE_BAR_LEFT_X_OFF            0
#define VARIO_FLIGHT_SIDE_BAR_RIGHT_MARGIN          0

/* -------------------------------------------------------------------------- */
/* 좌측 VARIO 바 내부 형상                                                     */
/* - tick는 우측 벽(inside edge)에 딱 붙게 right-align                         */
/* - fill은 좌측 바깥 벽 쪽에 붙인다.                                         */
/* - avg marker는 안쪽 2px edge strip 을 사용한다.                            */
/* -------------------------------------------------------------------------- */
#define VARIO_FLIGHT_VARIO_BAR_FILL_X_OFF           0
#define VARIO_FLIGHT_VARIO_BAR_AVG_X_OFF            (VARIO_FLIGHT_SIDE_BAR_W - VARIO_FLIGHT_SIDE_BAR_AVG_W)
#define VARIO_FLIGHT_VARIO_BAR_ZERO_LINE_FULL       1
#define VARIO_FLIGHT_VARIO_BAR_MINOR_TICK_W         6
#define VARIO_FLIGHT_VARIO_BAR_MAJOR_TICK_W         10

/* -------------------------------------------------------------------------- */
/* 우측 GS 바 내부 형상                                                        */
/* - tick는 좌측 벽(inside edge)에 딱 붙게 left-align                          */
/* - fill은 우측 바깥 벽 쪽에 붙인다.                                         */
/* -------------------------------------------------------------------------- */
#define VARIO_FLIGHT_GS_BAR_FILL_X_OFF              (VARIO_FLIGHT_SIDE_BAR_W - VARIO_FLIGHT_SIDE_BAR_FILL_W)
#define VARIO_FLIGHT_GS_BAR_MINOR_TICK_W            6
#define VARIO_FLIGHT_GS_BAR_MAJOR_TICK_W            10

/* -------------------------------------------------------------------------- */
/* 상단 시계                                                                   */
/* - X를 바꾸면 좌우 이동                                                      */
/* - Y baseline을 바꾸면 위아래 이동                                            */
/* - FONT를 바꾸면 글꼴 변경                                                    */
/* -------------------------------------------------------------------------- */
#define VARIO_FLIGHT_CLOCK_CENTER_X_OFF             120
#define VARIO_FLIGHT_CLOCK_BASELINE_Y_OFF           7
#define VARIO_FLIGHT_CLOCK_FONT                     u8g2_font_5x8_tr

/* -------------------------------------------------------------------------- */
/* 우측 ALT block                                                              */
/* - right aligned                                                            */
/* - ALT1은 가장 위까지 끌어올린다.                                            */
/* - ALT2/ALT3 row는 icon + value 조합                                        */
/* - ALT1/ALT2/ALT3 모두 고정 폭 박스 안에서 정렬한다.                        */
/* - ALIGN / BOX_W / GAP 매크로를 바꾸면 제품 느낌의 정렬을 유지한 채 조절된다.*/
/* -------------------------------------------------------------------------- */
#define VARIO_FLIGHT_ALT_BLOCK_RIGHT_MARGIN         (VARIO_FLIGHT_SIDE_BAR_W + 4)
#define VARIO_FLIGHT_ALT_BLOCK_W                    84
#define VARIO_FLIGHT_ALT1_LABEL_BASELINE_Y_OFF      10
#define VARIO_FLIGHT_ALT1_VALUE_BASELINE_Y_OFF      32
#define VARIO_FLIGHT_ALT1_UNIT_BASELINE_Y_OFF       32
#define VARIO_FLIGHT_ALT2_ROW_BASELINE_Y_OFF        43
#define VARIO_FLIGHT_ALT3_ROW_BASELINE_Y_OFF        53
#define VARIO_FLIGHT_ALT_LABEL_FONT                 u8g2_font_6x12_mf
#define VARIO_FLIGHT_ALT_VALUE_FONT                 u8g2_font_logisoso24_tn
#define VARIO_FLIGHT_ALT_UNIT_FONT                  u8g2_font_5x8_tr
#define VARIO_FLIGHT_ALT_ROW_FONT                   u8g2_font_5x8_tr
#define VARIO_FLIGHT_ALT1_ALIGN                      VARIO_TEXT_ALIGN_RIGHT
#define VARIO_FLIGHT_ALT1_VALUE_BOX_W                66
#define VARIO_FLIGHT_ALT1_UNIT_BOX_W                 14
#define VARIO_FLIGHT_ALT1_VALUE_UNIT_GAP             2
#define VARIO_FLIGHT_ALT_ROW_ALIGN                   VARIO_TEXT_ALIGN_RIGHT
#define VARIO_FLIGHT_ALT_ROW_ICON_GAP               2
#define VARIO_FLIGHT_ALT_ROW_TEXT_GAP               2

/* -------------------------------------------------------------------------- */
/* 좌측 info block                                                             */
/* - AVG / MAX / FLT / L/D                                                    */
/* - PAGE 1에서 사용                                                          */
/* - X를 바꾸면 블록 전체 이동                                                 */
/* - LABEL_ALIGN / VALUE_ALIGN 으로 항목 정렬 방향까지 바꿀 수 있다.           */
/* -------------------------------------------------------------------------- */
#define VARIO_FLIGHT_INFO_LEFT_X_OFF                (VARIO_FLIGHT_SIDE_BAR_W + 4)
#define VARIO_FLIGHT_INFO_LABEL_W                   16
#define VARIO_FLIGHT_INFO_VALUE_W                   44
#define VARIO_FLIGHT_INFO_AVG_BASELINE_Y_OFF        14
#define VARIO_FLIGHT_INFO_MAX_BASELINE_Y_OFF        24
#define VARIO_FLIGHT_INFO_FLT_BASELINE_Y_OFF        34
#define VARIO_FLIGHT_INFO_LD_BASELINE_Y_OFF         44
#define VARIO_FLIGHT_INFO_LABEL_FONT                u8g2_font_4x6_tf
#define VARIO_FLIGHT_INFO_VALUE_FONT                u8g2_font_5x8_tr
#define VARIO_FLIGHT_INFO_LABEL_ALIGN               VARIO_TEXT_ALIGN_LEFT
#define VARIO_FLIGHT_INFO_VALUE_ALIGN               VARIO_TEXT_ALIGN_LEFT

/* -------------------------------------------------------------------------- */
/* 좌측 큰 VARIO 숫자 박스                                                     */
/* - draw frame는 하지 않지만, 이 좌표/폭/높이가 invisible box 기준이다.       */
/* - label/value/unit 모두 이 박스 안에서 정렬된다.                           */
/* - 큰 숫자는 정수부만 크게, 소수 한 자리수는 오른쪽 위에 작게 붙는다.         */
/* -------------------------------------------------------------------------- */
#define VARIO_FLIGHT_VARIO_BOX_X_OFF                18
#define VARIO_FLIGHT_VARIO_BOX_W                    70
#define VARIO_FLIGHT_VARIO_LABEL_BASELINE_Y_OFF     59
#define VARIO_FLIGHT_VARIO_VALUE_BASELINE_Y_OFF     93
#define VARIO_FLIGHT_VARIO_UNIT_BASELINE_Y_OFF      100
#define VARIO_FLIGHT_VARIO_ALIGN                    VARIO_TEXT_ALIGN_LEFT
#define VARIO_FLIGHT_VARIO_LABEL_FONT               u8g2_font_6x12_mf
#define VARIO_FLIGHT_VARIO_MAJOR_FONT               u8g2_font_logisoso24_tn
#define VARIO_FLIGHT_VARIO_SIGN_FONT                u8g2_font_helvB10_tf
#define VARIO_FLIGHT_VARIO_MINOR_FONT               u8g2_font_helvR08_tf
#define VARIO_FLIGHT_VARIO_UNIT_FONT                u8g2_font_5x8_tr

/* -------------------------------------------------------------------------- */
/* 우측 큰 GS 숫자 박스                                                        */
/* - 현재 코드에서 우측 큰 vario가 있던 영역을 GS 표시영역으로 재사용한다.      */
/* - right aligned                                                            */
/* -------------------------------------------------------------------------- */
#define VARIO_FLIGHT_GS_BOX_RIGHT_MARGIN            (VARIO_FLIGHT_SIDE_BAR_W + 4)
#define VARIO_FLIGHT_GS_BOX_W                       70
#define VARIO_FLIGHT_GS_LABEL_BASELINE_Y_OFF        59
#define VARIO_FLIGHT_GS_VALUE_BASELINE_Y_OFF        93
#define VARIO_FLIGHT_GS_UNIT_BASELINE_Y_OFF         100
#define VARIO_FLIGHT_GS_ALIGN                       VARIO_TEXT_ALIGN_RIGHT
#define VARIO_FLIGHT_GS_LABEL_FONT                  u8g2_font_6x12_mf
#define VARIO_FLIGHT_GS_MAJOR_FONT                  u8g2_font_logisoso24_tn
#define VARIO_FLIGHT_GS_MINOR_FONT                  u8g2_font_helvR08_tf
#define VARIO_FLIGHT_GS_UNIT_FONT                   u8g2_font_5x8_tr

/* -------------------------------------------------------------------------- */
/* ALTITUDE trend graph                                                        */
/* - 좌측 큰 VARIO 박스와 우측 큰 GS 박스 사이의 남는 중앙 영역에 위치한다.     */
/* - heading tape 바로 위에 온다.                                              */
/* - frame 없이 sparkline만 그려 제품 느낌을 유지한다.                         */
/* -------------------------------------------------------------------------- */
#define VARIO_FLIGHT_ALT_TREND_X_OFF                88
#define VARIO_FLIGHT_ALT_TREND_W                    64
#define VARIO_FLIGHT_ALT_TREND_Y_OFF                83
#define VARIO_FLIGHT_ALT_TREND_H                    15

/* -------------------------------------------------------------------------- */
/* 하단 compass tape                                                           */
/* - PAGE 1에서 사용                                                          */
/* - height를 늘리면 box와 tick이 함께 아래쪽을 더 많이 차지한다.               */
/* -------------------------------------------------------------------------- */
#define VARIO_FLIGHT_COMPASS_X_OFF                  28
#define VARIO_FLIGHT_COMPASS_W                      184
#define VARIO_FLIGHT_COMPASS_Y_OFF                  107
#define VARIO_FLIGHT_COMPASS_H                      18
#define VARIO_FLIGHT_COMPASS_LABEL_FONT             u8g2_font_4x6_tf

/* -------------------------------------------------------------------------- */
/* PAGE 2 trail map                                                            */
/* - breadcrumb trail은 좌우 사이드 바를 제외한 전체 영역을 사용한다.          */
/* - top/bottom까지 꽉 채우고, 수치들은 그 위에 오버레이한다.                   */
/* -------------------------------------------------------------------------- */
#define VARIO_FLIGHT_PAGE2_MAP_X_OFF                VARIO_FLIGHT_SIDE_BAR_W
#define VARIO_FLIGHT_PAGE2_MAP_Y_OFF                0
#define VARIO_FLIGHT_PAGE2_MAP_W                    (VARIO_FLIGHT_LCD_W - (VARIO_FLIGHT_SIDE_BAR_W * 2))
#define VARIO_FLIGHT_PAGE2_MAP_H                    VARIO_FLIGHT_LCD_H
#define VARIO_FLIGHT_PAGE2_NORTH_LABEL_FONT         u8g2_font_4x6_tf
#define VARIO_FLIGHT_PAGE2_FLT_X_OFF                18
#define VARIO_FLIGHT_PAGE2_FLT_BASELINE_Y_OFF       123
#define VARIO_FLIGHT_PAGE2_FLT_FONT                 u8g2_font_5x8_tr

#ifdef __cplusplus
}
#endif

#endif /* VARIO_FLIGHT_LAYOUT_H */
