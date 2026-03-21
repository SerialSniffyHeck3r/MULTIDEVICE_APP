#ifndef VARIO_ICON_RESOURCES_H
#define VARIO_ICON_RESOURCES_H

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/* VARIO 공용 아이콘 리소스                                                    */
/*                                                                            */
/* 사용 규칙                                                                   */
/* 1) 비트맵은 이 파일에만 모은다.                                             */
/* 2) 공용 renderer 는 폭/높이 매크로 + bits 심볼만 참조한다.                  */
/* 3) ALT1/2/3 아이콘을 바꾸고 싶으면 여기 비트맵만 바꾸면 된다.                */
/* -------------------------------------------------------------------------- */

#define VARIO_ICON_ALT1_WIDTH   9
#define VARIO_ICON_ALT1_HEIGHT  9
static const unsigned char vario_icon_alt1_bits[] = {
    0x10,0xfe,0x38,0xfe,0x6c,0xfe,0xc6,0xfe,0x00,0xfe,0x00,0xfe,
    0xaa,0xfe,0xff,0xff,0xff,0xff
};

/* 사용자 지정 ALT2 아이콘 (7x7) */
#define VARIO_ICON_ALT2_WIDTH   7
#define VARIO_ICON_ALT2_HEIGHT  7
static const unsigned char vario_icon_alt2_bits[] = {
    0xbe,0xe3,0xef,0xe3,0xfb,0xe3,0xbe
};

/* 사용자 지정 ALT3 아이콘 (7x7) */
#define VARIO_ICON_ALT3_WIDTH   7
#define VARIO_ICON_ALT3_HEIGHT  7
static const unsigned char vario_icon_alt3_bits[] = {
    0xbe,0xe3,0xef,0xe3,0xef,0xe3,0xbe
};

#define VARIO_ICON_VARIO_UP_WIDTH   9
#define VARIO_ICON_VARIO_UP_HEIGHT  5
static const unsigned char vario_icon_vario_up_bits[] = {
    0x10,0xfe,0x38,0xfe,0x7c,0xfe,0xfe,0xfe,0xff,0xff
};

#define VARIO_ICON_VARIO_DOWN_WIDTH   9
#define VARIO_ICON_VARIO_DOWN_HEIGHT  5
static const unsigned char vario_icon_vario_down_bits[] = {
    0xff,0xff,0xfe,0xfe,0x7c,0xfe,0x38,0xfe,0x10,0xfe
};

#define VARIO_ICON_GS_AVG_WIDTH   5
#define VARIO_ICON_GS_AVG_HEIGHT  5
static const unsigned char vario_icon_gs_avg_bits[] = {
    0xe7,0xee,0xfc,0xee,0xe7
};

#ifdef __cplusplus
}
#endif

#endif /* VARIO_ICON_RESOURCES_H */
