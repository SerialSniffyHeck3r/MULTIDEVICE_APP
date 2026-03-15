#include "ui_common_icons.h"

/* -------------------------------------------------------------------------- */
/*  Status bar icons from uploaded prototype                                   */
/* -------------------------------------------------------------------------- */
const uint8_t icon_gps_main_bits[ICON7_H] U8X8_PROGMEM = {
    0xe4, 0xf2, 0xb5, 0x88, 0xd6, 0xa7, 0x93
};

const uint8_t icon_gps_rx_7_bits[ICON7_H] U8X8_PROGMEM = {
    0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff
};

const uint8_t icon_gps_rx_6_bits[ICON7_H] U8X8_PROGMEM = {
    0x80, 0xa0, 0xb0, 0xb8, 0xbc, 0xbe, 0xbf
};

const uint8_t icon_gps_rx_5_bits[ICON7_H] U8X8_PROGMEM = {
    0x80, 0x80, 0x90, 0x98, 0x9c, 0x9e, 0x9f
};

const uint8_t icon_gps_rx_4_bits[ICON7_H] U8X8_PROGMEM = {
    0x80, 0x80, 0x80, 0x88, 0x8c, 0x8e, 0x8f
};

const uint8_t icon_gps_rx_3_bits[ICON7_H] U8X8_PROGMEM = {
    0x80, 0x80, 0x80, 0x80, 0x84, 0x86, 0x87
};

const uint8_t icon_gps_rx_2_bits[ICON7_H] U8X8_PROGMEM = {
    0x80, 0x80, 0x80, 0x80, 0x80, 0x82, 0x83
};

const uint8_t icon_gps_rx_1_bits[ICON7_H] U8X8_PROGMEM = {
    0x9c, 0xa2, 0xd1, 0xc9, 0xc5, 0xa2, 0x9c
};

const uint8_t icon_gps_3d_bits[ICON11_H * 2] U8X8_PROGMEM = {
    0xfe, 0xfb,
    0x21, 0xfc,
    0x6f, 0xfd,
    0x61, 0xfd,
    0x6f, 0xfd,
    0x21, 0xfc,
    0xfe, 0xfb
};

const uint8_t icon_gps_2d_bits[ICON11_H * 2] U8X8_PROGMEM = {
    0xfe, 0xfb,
    0x21, 0xfc,
    0x6f, 0xfd,
    0x61, 0xfd,
    0x7d, 0xfd,
    0x21, 0xfc,
    0xfe, 0xfb
};

const uint8_t icon_gps_nofix_bits[ICON11_H * 2] U8X8_PROGMEM = {
    0x86, 0xfb,
    0x7f, 0xff,
    0x7f, 0xff,
    0x9f, 0xff,
    0xdf, 0xff,
    0xff, 0xff,
    0xde, 0xfb
};

const uint8_t blank_11x7[ICON11_H * 2] U8X8_PROGMEM = {
    0x00, 0xf8,
    0x00, 0xf8,
    0x00, 0xf8,
    0x00, 0xf8,
    0x00, 0xf8,
    0x00, 0xf8,
    0x00, 0xf8
};

const uint8_t icon_mmc_present_bits[ICON7_H] U8X8_PROGMEM = {
    0x9f, 0xb5, 0xd5, 0xc1, 0xc1, 0xc1, 0xff
};

const uint8_t icon_mmc_not_present_bits[ICON7_H] U8X8_PROGMEM = {
    0x9f, 0xa1, 0xd1, 0xc9, 0xc5, 0xc3, 0xff
};

const uint8_t icon_mmc_error_bits[ICON7_H] U8X8_PROGMEM = {
    0x9f, 0xa9, 0xc9, 0xc9, 0xc1, 0xc9, 0xff
};

const uint8_t icon_rec_bits[ICON7_H] U8X8_PROGMEM = {
    0x80, 0x9c, 0xbe, 0xbe, 0xbe, 0x9c, 0x80
};

const uint8_t icon_stop_bits[ICON7_H] U8X8_PROGMEM = {
    0x80, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0x80
};

const uint8_t icon_pause_bits[ICON7_H] U8X8_PROGMEM = {
    0xb6, 0xb6, 0xb6, 0xb6, 0xb6, 0xb6, 0xb6
};

const uint8_t blank_7x7[ICON7_H] U8X8_PROGMEM = {
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80
};

const uint8_t icon_antenna_shape[ICON5_H] U8X8_PROGMEM = {
    0xff, 0xea, 0xe4, 0xe4, 0xe4, 0xe4, 0xe4
};

const uint8_t icon_degrees[ICON7_H] U8X8_PROGMEM = {
    0x8c, 0x92, 0x92, 0x8c, 0x80, 0x80, 0x80
};

/* -------------------------------------------------------------------------- */
/*  Bluetooth icon                                                             */
/*                                                                            */
/*  상단바 높이 7px에 정확히 맞춰 들어가는 7x7 XBM 비트맵이다.                 */
/*  사용자가 지정한 비트 패턴을 그대로 사용한다.                               */
/*  - width  = 7                                                               */
/*  - height = 7                                                               */
/*  - bytes  = 7                                                               */
/*                                                                            */
/*  이 아이콘은 status bar에서 SD 카드 아이콘과 정지/녹화 아이콘 사이에        */
/*  배치되는 전용 블루투스 룬 그림이다.                                        */
/* -------------------------------------------------------------------------- */
const uint8_t icon_bluetooth_bits[ICON7_H] U8X8_PROGMEM = {
    0x99, 0xaa, 0xcc, 0xb8, 0xcc, 0xaa, 0x99
};

/* -------------------------------------------------------------------------- */
/*  Bottom bar arrows                                                          */
/*                                                                            */
/*  높이 4px 제약 안에서 방향이 보이도록 단순화한 화살표들이다.                */
/* -------------------------------------------------------------------------- */
const uint8_t icon_arrow_right_7x4[ICON7X4_H] U8X8_PROGMEM = {
    0x08, 0x30, 0x7f, 0x30
};

const uint8_t icon_arrow_left_7x4[ICON7X4_H] U8X8_PROGMEM = {
    0x08, 0x06, 0x7f, 0x06
};

const uint8_t icon_arrow_up_7x4[ICON7X4_H] U8X8_PROGMEM = {
    0x08, 0x1c, 0x36, 0x08
};

const uint8_t icon_arrow_down_7x4[ICON7X4_H] U8X8_PROGMEM = {
    0x08, 0x36, 0x1c, 0x08
};

/* -------------------------------------------------------------------------- */
/*  Cute icons                                                                 */
/* -------------------------------------------------------------------------- */
const uint8_t icon_cute_cat_8x8[ICON8_H] U8X8_PROGMEM = {
    0x81, 0xc3, 0xa5, 0x81, 0xa5, 0x7e, 0x24, 0x18
};

const uint8_t icon_cute_heart_8x8[ICON8_H] U8X8_PROGMEM = {
    0x66, 0xff, 0xff, 0x7e, 0x3c, 0x18, 0x10, 0x00
};

const uint8_t icon_cute_star_8x8[ICON8_H] U8X8_PROGMEM = {
    0x08, 0x49, 0x3e, 0x1c, 0x3e, 0x49, 0x08, 0x00
};

const uint8_t icon_cute_bike_8x8[ICON8_H] U8X8_PROGMEM = {
    0x00, 0x18, 0x3c, 0x66, 0xdb, 0x99, 0x24, 0x00
};

/* -------------------------------------------------------------------------- */
/*  UI message icons                                                           */
/* -------------------------------------------------------------------------- */
const uint8_t icon_ui_info_8x8[ICON8_H] U8X8_PROGMEM = {
    0x3c, 0x66, 0x66, 0x18, 0x18, 0x18, 0x3c, 0x00
};

const uint8_t icon_ui_warn_8x8[ICON8_H] U8X8_PROGMEM = {
    0x08, 0x1c, 0x1c, 0x36, 0x36, 0x3f, 0x08, 0x00
};

const uint8_t icon_ui_ok_8x8[ICON8_H] U8X8_PROGMEM = {
    0x00, 0x40, 0x60, 0x31, 0x1b, 0x0e, 0x04, 0x00
};

const uint8_t icon_ui_bell_8x8[ICON8_H] U8X8_PROGMEM = {
    0x18, 0x3c, 0x3c, 0x3c, 0x7e, 0x18, 0x3c, 0x00
};

const uint8_t icon_ui_folder_8x8[ICON8_H] U8X8_PROGMEM = {
    0x00, 0x0e, 0x3f, 0x33, 0x3f, 0x3f, 0x00, 0x00
};
