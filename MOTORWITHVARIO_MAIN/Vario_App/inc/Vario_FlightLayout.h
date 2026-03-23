#ifndef VARIO_FLIGHT_LAYOUT_H
#define VARIO_FLIGHT_LAYOUT_H

#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* FLYTEC-style flight-page layout constants                                   */
/*                                                                            */
/* 이 헤더는 PAGE 1 / PAGE 2 / PAGE 3 이 함께 공유하는 공용 flight UI shell 의   */
/* "좌표/크기/폰트/정렬" 매크로만 모아 둔 레이아웃 파일이다.                    */
/*                                                                            */
/* 매우 중요                                                                  */
/* 1) 이 파일은 숫자와 그림의 위치만 정의한다.                                 */
/* 2) 실제 값을 어디서 가져오는지는 Vario_State.c 가 책임진다.                 */
/* 3) renderer 는 APP_STATE 를 직접 읽지 않고,                                */
/*    const vario_runtime_t *rt = Vario_State_GetRuntime(); 만 사용한다.       */
/* 4) 화면을 옮기고 싶으면 .c 파일 본문을 건드리기 전에 먼저 이 헤더의 매크로를  */
/*    조정한다.                                                                */
/*                                                                            */
/* 권장 수정 순서                                                              */
/* A. 폰트만 바꾸고 싶다 -> *_FONT 매크로 수정                                 */
/* B. 좌우 이동만 하고 싶다 -> *_X_OFF / *_RIGHT_MARGIN 수정                   */
/* C. 위아래 이동만 하고 싶다 -> *_BASELINE_Y_OFF / *_TOP_Y_OFF 수정           */
/* D. 겹침이 생긴다 -> *_BOX_W / *_GAP / *_RESERVED_H 조정                     */
/* -------------------------------------------------------------------------- */

/* -------------------------------------------------------------------------- */
/* 공통 기준 해상도                                                            */
/* -------------------------------------------------------------------------- */
#define VARIO_FLIGHT_LCD_W                              240
#define VARIO_FLIGHT_LCD_H                              128

/* -------------------------------------------------------------------------- */
/* 공통 정렬 값                                                                */
/* -------------------------------------------------------------------------- */
#define VARIO_FLIGHT_ALIGN_LEFT                           0u
#define VARIO_FLIGHT_ALIGN_CENTER                         1u
#define VARIO_FLIGHT_ALIGN_RIGHT                          2u

/* -------------------------------------------------------------------------- */
/* 좌/우 세로 그래프 공통 영역                                                 */
/*                                                                            */
/* - 사용자가 요구한 고정 폭 14px                                              */
/* - PAGE 1 / PAGE 2 / PAGE 3 모두 동일하게 사용                               */
/* - 실제 전체 화면에서 좌우 "기둥" 처럼 붙어 있는 영역이다.                   */
/* -------------------------------------------------------------------------- */
#define VARIO_FLIGHT_SIDE_BAR_W                         14
#define VARIO_FLIGHT_SIDE_BAR_X_LEFT_OFF                 0
#define VARIO_FLIGHT_SIDE_BAR_RIGHT_MARGIN               0

/* -------------------------------------------------------------------------- */
/* 좌측 VARIO bar 내부 배치                                                    */
/*                                                                            */
/* 14px를 아래처럼 분할해 사용한다.                                            */
/*   x=0..7   : 눈금 tick 영역                                                 */
/*   x=8..11  : 현재 fast vario fill 영역                                      */
/*   x=12     : average vario edge strip                                       */
/*   x=13     : overflow strip                                                 */
/*                                                                            */
/* 작은 눈금은 반드시 왼쪽 벽(x=0)에 딱 붙는다.                               */
/* 큰 눈금은 작은 눈금보다 길다.                                              */
/* 0.0m/s 선은 3픽셀 두께로 가장 길게 그린다.                                  */
/* -------------------------------------------------------------------------- */
#define VARIO_FLIGHT_VARIO_TICK_X_OFF                    0
#define VARIO_FLIGHT_VARIO_FILL_X_OFF                    8
#define VARIO_FLIGHT_VARIO_FILL_W                        4
#define VARIO_FLIGHT_VARIO_AVG_X_OFF                    12
#define VARIO_FLIGHT_VARIO_AVG_W                         1
#define VARIO_FLIGHT_VARIO_OVERFLOW_X_OFF               13
#define VARIO_FLIGHT_VARIO_OVERFLOW_W                    1
#define VARIO_FLIGHT_VARIO_TICK_MINOR_W                  5
#define VARIO_FLIGHT_VARIO_TICK_MAJOR_W                  8
#define VARIO_FLIGHT_VARIO_ZERO_TICK_W                  12
#define VARIO_FLIGHT_VARIO_ZERO_THICKNESS                3
#define VARIO_FLIGHT_VARIO_BASE_RANGE_MPS_X10           40   /* 4.0m/s */
#define VARIO_FLIGHT_VARIO_EXT_RANGE_MPS_X10            80   /* 8.0m/s */
#define VARIO_FLIGHT_VARIO_MINOR_STEP_MPS_X10            5   /* 0.5m/s */
#define VARIO_FLIGHT_VARIO_MAJOR_STEP_MPS_X10           10   /* 1.0m/s */

/* -------------------------------------------------------------------------- */
/* 우측 GS bar 내부 배치                                                       */
/*                                                                            */
/* 14px를 아래처럼 분할해 사용한다.                                            */
/*   x=0..3   : 현재 GS fill 영역                                              */
/*   x=4..8   : 보조 여백/중첩 방지                                             */
/*   x=9..13  : outer marker icon 영역                                         */
/*                                                                            */
/* 작은 눈금은 반드시 오른쪽 벽(x=13)에 딱 붙는다.                            */
/* 큰 눈금은 작은 눈금보다 길다.                                              */
/* -------------------------------------------------------------------------- */
#define VARIO_FLIGHT_GS_FILL_X_OFF                       0
#define VARIO_FLIGHT_GS_FILL_W                           4
#define VARIO_FLIGHT_GS_MARKER_X_OFF                     9
#define VARIO_FLIGHT_GS_MARKER_W                         5
#define VARIO_FLIGHT_GS_TICK_MINOR_W                     5
#define VARIO_FLIGHT_GS_TICK_MAJOR_W                     8
#define VARIO_FLIGHT_GS_MIN_KMH                         10
#define VARIO_FLIGHT_GS_MAX_KMH                         80
#define VARIO_FLIGHT_GS_MINOR_STEP_KMH                   5
#define VARIO_FLIGHT_GS_MAJOR_STEP_KMH                  10

/* -------------------------------------------------------------------------- */
/* 상단 GPS TIME                                                               */
/*                                                                            */
/* 화면 가장 위쪽에 달라붙는 가운데 정렬 시계이다.                            */
/* ALT1과 가로로 겹치지 않게 작은 폰트를 사용한다.                             */
/* -------------------------------------------------------------------------- */
#define VARIO_FLIGHT_TOP_TIME_FONT            u8g2_font_5x8_tr
#define VARIO_FLIGHT_TOP_TIME_CENTER_X_OFF                120
#define VARIO_FLIGHT_TOP_TIME_BASELINE_Y_OFF                7

/* -------------------------------------------------------------------------- */
/* 좌측 상단 GLD block                                                         */
/*                                                                            */
/* FLT TIME을 여기서 제거하고, GLD만 남긴다.                                   */
/* label 과 value 의 X anchor 는 동일하다.                                     */
/* value 가 길어질 때는 BOX_W 를 먼저 늘린다.                                  */
/* -------------------------------------------------------------------------- */
#define VARIO_FLIGHT_GLD_X_OFF                           14
#define VARIO_FLIGHT_GLD_BOX_W                           34
#define VARIO_FLIGHT_GLD_LABEL_FONT           u8g2_font_5x8_tr
#define VARIO_FLIGHT_GLD_VALUE_FONT           u8g2_font_6x12_mf
#define VARIO_FLIGHT_GLD_LABEL_BASELINE_Y_OFF             9
#define VARIO_FLIGHT_GLD_VALUE_BASELINE_Y_OFF            22

/* -------------------------------------------------------------------------- */
/* 우측 ALT block                                                              */
/*                                                                            */
/* ALT1은 우측 14px GS bar를 제외한 오른쪽 영역 안에서 right align 으로 배치.   */
/* ALT1 icon 은 아예 그리지 않는다.                                            */
/* ALT2/ALT3 는 사용자 제공 7x7 icon 을 같은 X축에 두고, Y만 다르게 한다.     */
/* ALT1 / ALT2 / ALT3 모두 고정 폭 box 를 미리 잡고, 실제 문자열만 right align */
/* 한다. 그래서 10m -> 1000m 로 바뀌어도 좌표가 흔들리지 않는다.               */
/* -------------------------------------------------------------------------- */
#define VARIO_FLIGHT_ALT_RIGHT_MARGIN                   16
#define VARIO_FLIGHT_ALT_UNIT_GAP_PX                     2
#define VARIO_FLIGHT_ALT_VALUE_BOX_DIGITS                4
#define VARIO_FLIGHT_ALT1_VALUE_FONT         u8g2_font_logisoso24_tn
#define VARIO_FLIGHT_ALT1_UNIT_FONT          u8g2_font_5x8_tr
#define VARIO_FLIGHT_ALT1_BASELINE_Y_OFF                25
#define VARIO_FLIGHT_ALT1_UNIT_BASELINE_Y_OFF           24
#define VARIO_FLIGHT_ALT2_FONT               u8g2_font_6x12_mf
#define VARIO_FLIGHT_ALT23_UNIT_FONT         u8g2_font_5x8_tr
#define VARIO_FLIGHT_ALT23_ICON_GAP_PX                  3
#define VARIO_FLIGHT_ALT23_VALUE_GAP_PX                 3
#define VARIO_FLIGHT_ALT2_BASELINE_Y_OFF                41
#define VARIO_FLIGHT_ALT3_BASELINE_Y_OFF                55

/* -------------------------------------------------------------------------- */
/* 하단 좌측 큰 VARIO value block                                              */
/*                                                                            */
/* - MAX label / max value / trend arrow / current value / unit 순서로 배치.   */
/* - 큰 숫자 정수부는 ALT1과 동일한 font 를 쓴다.                              */
/* - 소수 첫째 자리는 한 단계 작은 font 로, 정수부 오른쪽 위에 top-align 한다. */
/* - box 폭은 "00" 기준으로 고정되며, leading zero 는 실제로 그리지 않는다.    */
/* -------------------------------------------------------------------------- */
#define VARIO_FLIGHT_LEFT_VALUE_X_OFF                   18
#define VARIO_FLIGHT_LEFT_VALUE_BOX_W                   70
#define VARIO_FLIGHT_LEFT_MAX_LABEL_FONT     u8g2_font_5x8_tr
#define VARIO_FLIGHT_LEFT_MAX_VALUE_MAJOR_FONT u8g2_font_6x12_mf
#define VARIO_FLIGHT_LEFT_MAX_VALUE_MINOR_FONT u8g2_font_5x8_tr
#define VARIO_FLIGHT_LEFT_BIG_MAJOR_FONT     u8g2_font_logisoso24_tn
#define VARIO_FLIGHT_LEFT_BIG_MINOR_FONT     u8g2_font_helvR08_tf
#define VARIO_FLIGHT_LEFT_UNIT_FONT          u8g2_font_5x8_tr
#define VARIO_FLIGHT_LEFT_MAX_LABEL_BASELINE_Y_OFF      62
#define VARIO_FLIGHT_LEFT_MAX_VALUE_BASELINE_Y_OFF      74
#define VARIO_FLIGHT_LEFT_ARROW_Y_TOP_OFF               79
#define VARIO_FLIGHT_LEFT_BIG_VALUE_BASELINE_Y_OFF     108
#define VARIO_FLIGHT_LEFT_UNIT_BASELINE_Y_OFF          118

/* -------------------------------------------------------------------------- */
/* 하단 우측 큰 GS value block                                                 */
/*                                                                            */
/* - 포맷은 VARIO와 동일하되 integer 자리 수만 3자리로 더 넓다.                 */
/* - box 전체는 right align 기준으로 앵커를 잡는다.                            */
/* -------------------------------------------------------------------------- */
#define VARIO_FLIGHT_RIGHT_VALUE_RIGHT_MARGIN           18
#define VARIO_FLIGHT_RIGHT_VALUE_BOX_W                  70
#define VARIO_FLIGHT_RIGHT_MAX_LABEL_FONT    u8g2_font_5x8_tr
#define VARIO_FLIGHT_RIGHT_MAX_VALUE_MAJOR_FONT u8g2_font_6x12_mf
#define VARIO_FLIGHT_RIGHT_MAX_VALUE_MINOR_FONT u8g2_font_5x8_tr
#define VARIO_FLIGHT_RIGHT_BIG_MAJOR_FONT    u8g2_font_logisoso24_tn
#define VARIO_FLIGHT_RIGHT_BIG_MINOR_FONT    u8g2_font_helvR08_tf
#define VARIO_FLIGHT_RIGHT_UNIT_FONT         u8g2_font_5x8_tr
#define VARIO_FLIGHT_RIGHT_MAX_LABEL_BASELINE_Y_OFF     62
#define VARIO_FLIGHT_RIGHT_MAX_VALUE_BASELINE_Y_OFF     74
#define VARIO_FLIGHT_RIGHT_ARROW_Y_TOP_OFF              79
#define VARIO_FLIGHT_RIGHT_BIG_VALUE_BASELINE_Y_OFF    108
#define VARIO_FLIGHT_RIGHT_UNIT_BASELINE_Y_OFF         118

/* -------------------------------------------------------------------------- */
/* 하단 중앙 FLT TIME                                                          */
/*                                                                            */
/* 나침반을 유지한 채 화면 맨 아래에 붙도록 작은 고정폭 시계를 둔다.            */
/* 겹침이 보이면 먼저 FONT를 줄이거나 RESERVED_H를 늘린다.                     */
/* -------------------------------------------------------------------------- */
#define VARIO_FLIGHT_BOTTOM_TIME_FONT        u8g2_font_6x12_mf
#define VARIO_FLIGHT_BOTTOM_TIME_CENTER_X_OFF           120
#define VARIO_FLIGHT_BOTTOM_TIME_BASELINE_Y_OFF         127
#define VARIO_FLIGHT_BOTTOM_TIME_RESERVED_H              12

/* -------------------------------------------------------------------------- */
/* PAGE 1 compass zone                                                         */
/*                                                                            */
/* user가 "나침반은 건들지 말라"고 했기 때문에 renderer 쪽에서는 이 zone 을     */
/* 최대한 기존 기능적 의미 그대로 유지한다.                                   */
/* 다만, 하단 FLT TIME 과 겹치지 않도록 bottom reserve 높이만 별도 둔다.       */
/* -------------------------------------------------------------------------- */
#define VARIO_FLIGHT_COMPASS_LABEL_FONT      u8g2_font_6x12_mf
#define VARIO_FLIGHT_COMPASS_CARDINAL_FONT   u8g2_font_5x8_tr
#define VARIO_FLIGHT_COMPASS_SIDE_MARGIN                  8
#define VARIO_FLIGHT_COMPASS_LABEL_BASELINE_Y_OFF        34
#define VARIO_FLIGHT_COMPASS_TOP_GAP_PX                   4
#define VARIO_FLIGHT_COMPASS_BOTTOM_GAP_PX                2
#define VARIO_FLIGHT_COMPASS_LABEL_INSET                  6
#define VARIO_FLIGHT_COMPASS_MIN_RADIUS                  18
#define VARIO_FLIGHT_COMPASS_MAX_RADIUS                  32

/* -------------------------------------------------------------------------- */
/* PAGE 2 trail area                                                           */
/*                                                                            */
/* trail 은 좌우 14px bar 를 제외한 중앙 전체를 배경으로 사용한다.             */
/* 수치 overlay 는 그 위에 얹힌다.                                             */
/* -------------------------------------------------------------------------- */
#define VARIO_FLIGHT_TRAIL_X_OFF              VARIO_FLIGHT_SIDE_BAR_W
#define VARIO_FLIGHT_TRAIL_Y_OFF                            0
#define VARIO_FLIGHT_TRAIL_W                 (VARIO_FLIGHT_LCD_W - (VARIO_FLIGHT_SIDE_BAR_W * 2))
#define VARIO_FLIGHT_TRAIL_H                  VARIO_FLIGHT_LCD_H
#define VARIO_FLIGHT_TRAIL_CENTER_MARK_RADIUS               3

/* -------------------------------------------------------------------------- */
/* PAGE 3 stub overlay                                                         */
/* -------------------------------------------------------------------------- */
#define VARIO_FLIGHT_STUB_TITLE_FONT         u8g2_font_10x20_mf
#define VARIO_FLIGHT_STUB_SUBTITLE_FONT      u8g2_font_6x12_mf
#define VARIO_FLIGHT_STUB_TITLE_DY                       -4
#define VARIO_FLIGHT_STUB_SUBTITLE_DY                   14

/* -------------------------------------------------------------------------- */
/* 공통 소수점 split 렌더링                                                    */
/*                                                                            */
/* 모든 큰 숫자에서 '.' 문자는 아예 그리지 않는다.                             */
/* gap 값은 정수부와 소수부 사이 간격이다.                                     */
/* -------------------------------------------------------------------------- */
#define VARIO_FLIGHT_SPLIT_DECIMAL_GAP_PX                 1

#ifdef __cplusplus
}
#endif

#endif /* VARIO_FLIGHT_LAYOUT_H */
