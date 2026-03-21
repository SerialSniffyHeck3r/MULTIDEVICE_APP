#ifndef VARIO_ICON_RESOURCES_H
#define VARIO_ICON_RESOURCES_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* VARIO flight-page bitmap resources                                          */
/*                                                                            */
/* 이 파일은 바리오 앱 전용 아이콘 리소스만 담는 "순수 리소스" 헤더다.          */
/*                                                                            */
/* 사용법                                                                     */
/* 1) renderer 쪽에서 이 파일을 include 한다.                                  */
/* 2) u8g2_DrawXBM() 또는 u8g2_DrawXBMP() 로 원하는 좌표에 바로 찍는다.        */
/* 3) 아이콘 자체 모양을 바꾸고 싶으면 아래 비트맵만 수정하면 된다.             */
/* 4) 아이콘 위치/크기는 각 화면 파일 또는 Vario_FlightLayout.h 의 좌표/폭/높이 */
/*    매크로를 수정해서 조정한다.                                              */
/* -------------------------------------------------------------------------- */

/* ALT2 icon ---------------------------------------------------------------- */
/* 사용자가 직접 지정한 7x7 monochrome bitmap                                 */
/* - width / height 를 변경하면 draw 쪽도 같은 값으로 맞춰야 한다.            */
/* - 현재는 ALT2 row 왼쪽에 붙여 쓰는 용도다.                                  */
#define VARIO_ICON_ALT2_WIDTH   7u
#define VARIO_ICON_ALT2_HEIGHT  7u
static const uint8_t vario_icon_alt2_bits[] =
{
    0xbe, 0xe3, 0xef, 0xe3, 0xfb, 0xe3, 0xbe
};

/* ALT3 icon ---------------------------------------------------------------- */
/* 사용자가 직접 지정한 7x7 monochrome bitmap                                 */
/* - ALT3 row 왼쪽에 붙여 쓰는 용도다.                                         */
#define VARIO_ICON_ALT3_WIDTH   7u
#define VARIO_ICON_ALT3_HEIGHT  7u
static const uint8_t vario_icon_alt3_bits[] =
{
    0xbe, 0xe3, 0xef, 0xe3, 0xef, 0xe3, 0xbe
};

#ifdef __cplusplus
}
#endif

#endif /* VARIO_ICON_RESOURCES_H */
