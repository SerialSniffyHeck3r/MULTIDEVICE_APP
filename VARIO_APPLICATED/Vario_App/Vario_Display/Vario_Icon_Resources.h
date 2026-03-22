#ifndef VARIO_ICON_RESOURCES_H
#define VARIO_ICON_RESOURCES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* VARIO flight-page bitmap resources                                          */
/*                                                                            */
/* 이 파일은 바리오 앱 전용 monochrome XBM 아이콘 리소스만 담는 헤더다.        */
/*                                                                            */
/* 사용법                                                                     */
/* 1) renderer 쪽에서 이 파일을 include 한다.                                  */
/* 2) u8g2_DrawXBMP() 로 원하는 좌표에 바로 찍는다.                            */
/* 3) 아이콘 자체 모양을 바꾸고 싶으면 아래 비트맵만 수정하면 된다.             */
/* 4) 아이콘 위치는 Vario_FlightLayout.h 의 *_X_OFF / *_Y_OFF 매크로로 조정한다.*/
/* -------------------------------------------------------------------------- */

/* ALT2 icon ---------------------------------------------------------------- */
/* 사용자가 직접 지정한 7x7 monochrome bitmap                                 */
#define VARIO_ICON_ALT2_WIDTH   7u
#define VARIO_ICON_ALT2_HEIGHT  7u
static const uint8_t vario_icon_alt2_bits[] =
{
    0xbe, 0xe3, 0xef, 0xe3, 0xfb, 0xe3, 0xbe
};

/* ALT3 icon ---------------------------------------------------------------- */
/* 사용자가 직접 지정한 7x7 monochrome bitmap                                 */
#define VARIO_ICON_ALT3_WIDTH   7u
#define VARIO_ICON_ALT3_HEIGHT  7u
static const uint8_t vario_icon_alt3_bits[] =
{
    0xbe, 0xe3, 0xef, 0xe3, 0xef, 0xe3, 0xbe
};

/* 큰 숫자 블록용 상승/하강 arrow ------------------------------------------- */
/* VARIO / GS 둘 다 같은 아이콘을 재사용한다.                                 */
#define VARIO_ICON_TREND_UP_WIDTH   7u
#define VARIO_ICON_TREND_UP_HEIGHT  7u
static const uint8_t vario_icon_trend_up_bits[] =
{
    0x08, 0x1c, 0x3e, 0x08, 0x08, 0x08, 0x08
};

#define VARIO_ICON_TREND_DOWN_WIDTH   7u
#define VARIO_ICON_TREND_DOWN_HEIGHT  7u
static const uint8_t vario_icon_trend_down_bits[] =
{
    0x08, 0x08, 0x08, 0x08, 0x3e, 0x1c, 0x08
};

/* 우측 GS side bar edge marker --------------------------------------------- */
/* 14px GS 영역의 가장 끝(x=max) 쪽에 붙여 쓰는 marker.                       */
#define VARIO_ICON_BAR_MARK_RIGHT_WIDTH   5u
#define VARIO_ICON_BAR_MARK_RIGHT_HEIGHT  7u
static const uint8_t vario_icon_bar_mark_right_bits[] =
{
    0x01, 0x03, 0x1f, 0x1f, 0x1f, 0x03, 0x01
};

#ifdef __cplusplus
}
#endif

#endif /* VARIO_ICON_RESOURCES_H */
