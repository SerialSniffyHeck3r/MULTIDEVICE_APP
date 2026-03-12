#include "ui_common_icons.h"

/* -------------------------------------------------------------------------- */
/*  The following icon set is based on the uploaded prototype icons.c.         */
/*  Requested pixel geometry is preserved exactly for the existing icons.      */
/* -------------------------------------------------------------------------- */

/* GPS ICON */
const uint8_t icon_gps_main_bits[ICON7_H] U8X8_PROGMEM = {
    0xe4, 0xf2, 0xb5, 0x88, 0xd6, 0xa7, 0x93
};

/* RECEPTION 7 OUT OF 7 */
const uint8_t icon_gps_rx_7_bits[ICON7_H] U8X8_PROGMEM = {
    0xc0, 0xe0, 0xf0, 0xf8, 0xfc, 0xfe, 0xff
};

/* RECEPTION 6 OUT OF 7 */
const uint8_t icon_gps_rx_6_bits[ICON7_H] U8X8_PROGMEM = {
    0x80, 0xa0, 0xb0, 0xb8, 0xbc, 0xbe, 0xbf
};

/* RECEPTION 5 OUT OF 7 */
const uint8_t icon_gps_rx_5_bits[ICON7_H] U8X8_PROGMEM = {
    0x80, 0x80, 0x90, 0x98, 0x9c, 0x9e, 0x9f
};

/* RECEPTION 4 OUT OF 7 */
const uint8_t icon_gps_rx_4_bits[ICON7_H] U8X8_PROGMEM = {
    0x80, 0x80, 0x80, 0x88, 0x8c, 0x8e, 0x8f
};

/* RECEPTION 3 OUT OF 7 */
const uint8_t icon_gps_rx_3_bits[ICON7_H] U8X8_PROGMEM = {
    0x80, 0x80, 0x80, 0x80, 0x84, 0x86, 0x87
};

/* RECEPTION 2 OUT OF 7 */
const uint8_t icon_gps_rx_2_bits[ICON7_H] U8X8_PROGMEM = {
    0x80, 0x80, 0x80, 0x80, 0x80, 0x82, 0x83
};

/* RECEPTION 1 OUT OF 7 */
const uint8_t icon_gps_rx_1_bits[ICON7_H] U8X8_PROGMEM = {
    0x9c, 0xa2, 0xd1, 0xc9, 0xc5, 0xa2, 0x9c
};

/* GPS 3D */
const uint8_t icon_gps_3d_bits[ICON11_H * 2] U8X8_PROGMEM = {
    0xfe, 0xfb,
    0x21, 0xfc,
    0x6f, 0xfd,
    0x61, 0xfd,
    0x6f, 0xfd,
    0x21, 0xfc,
    0xfe, 0xfb
};

/* GPS 2D */
const uint8_t icon_gps_2d_bits[ICON11_H * 2] U8X8_PROGMEM = {
    0xfe, 0xfb,
    0x21, 0xfc,
    0x6f, 0xfd,
    0x61, 0xfd,
    0x7d, 0xfd,
    0x21, 0xfc,
    0xfe, 0xfb
};

/* GPS NO FIX */
const uint8_t icon_gps_nofix_bits[ICON11_H * 2] U8X8_PROGMEM = {
    0x86, 0xfb,
    0x7f, 0xff,
    0x7f, 0xff,
    0x9f, 0xff,
    0xdf, 0xff,
    0xff, 0xff,
    0xde, 0xfb
};

/* 11x7 BLANK */
const uint8_t blank_11x7[ICON11_H * 2] U8X8_PROGMEM = {
    0x00, 0xf8,
    0x00, 0xf8,
    0x00, 0xf8,
    0x00, 0xf8,
    0x00, 0xf8,
    0x00, 0xf8,
    0x00, 0xf8
};

/* MEMORY CARD PRESENT */
const uint8_t icon_mmc_present_bits[ICON7_H] U8X8_PROGMEM = {
    0x9f, 0xb5, 0xd5, 0xc1, 0xc1, 0xc1, 0xff
};

const uint8_t icon_mmc_not_present_bits[ICON7_H] U8X8_PROGMEM = {
    0x9f, 0xa1, 0xd1, 0xc9, 0xc5, 0xc3, 0xff
};

const uint8_t icon_mmc_error_bits[ICON7_H] U8X8_PROGMEM = {
    0x9f, 0xa9, 0xc9, 0xc9, 0xc1, 0xc9, 0xff
};

/* REC */
const uint8_t icon_rec_bits[ICON7_H] U8X8_PROGMEM = {
    0x80, 0x9c, 0xbe, 0xbe, 0xbe, 0x9c, 0x80
};

const uint8_t icon_stop_bits[ICON7_H] U8X8_PROGMEM = {
    0x80, 0xbe, 0xbe, 0xbe, 0xbe, 0xbe, 0x80
};

const uint8_t icon_pause_bits[ICON7_H] U8X8_PROGMEM = {
    0xb6, 0xb6, 0xb6, 0xb6, 0xb6, 0xb6, 0xb6
};

/* 7x7 BLANK */
const uint8_t blank_7x7[ICON7_H] U8X8_PROGMEM = {
    0x80, 0x80, 0x80, 0x80, 0x80, 0x80, 0x80
};

/* 5x7 ANTENNA */
const uint8_t icon_antenna_shape[ICON7_H] U8X8_PROGMEM = {
    0xff, 0xea, 0xe4, 0xe4, 0xe4, 0xe4, 0xe4
};

const uint8_t icon_degrees[ICON7_H] U8X8_PROGMEM = {
    0x8c, 0x92, 0x92, 0x8c, 0x80, 0x80, 0x80
};

/* -------------------------------------------------------------------------- */
/*  Added status bar icons                                                     */
/* -------------------------------------------------------------------------- */

/* Bluetooth icon, 7x7 */
const uint8_t icon_bluetooth_bits[ICON7_H] U8X8_PROGMEM = {
    0x04, 0x0e, 0x14, 0x0e, 0x14, 0x0e, 0x04
};

/* Exactly the 7-byte helper bitmap provided by the user */
const uint8_t icon_bt_aux_bits[ICON7_H] U8X8_PROGMEM = {
    0x98, 0xa9, 0xca, 0xac, 0xca, 0xa9, 0x98
};

/* -------------------------------------------------------------------------- */
/*  Bottom bar 4px icons                                                       */
/* -------------------------------------------------------------------------- */
const uint8_t icon_arrow_right_7x4[ICON7X4_H] U8X8_PROGMEM = {
    0x08, 0x10, 0x1f, 0x10
};

const uint8_t icon_arrow_left_7x4[ICON7X4_H] U8X8_PROGMEM = {
    0x08, 0x04, 0x1f, 0x04
};

const uint8_t icon_arrow_up_7x4[ICON7X4_H] U8X8_PROGMEM = {
    0x04, 0x0e, 0x15, 0x00
};

const uint8_t icon_arrow_down_7x4[ICON7X4_H] U8X8_PROGMEM = {
    0x00, 0x15, 0x0e, 0x04
};

/* -------------------------------------------------------------------------- */
/*  Cute test icons                                                            */
/* -------------------------------------------------------------------------- */
const uint8_t icon_cute_cat_8x8[ICON8_H] U8X8_PROGMEM = {
    0x22, 0x55, 0x41, 0x55, 0x41, 0x22, 0x1c, 0x00
};

const uint8_t icon_cute_heart_8x8[ICON8_H] U8X8_PROGMEM = {
    0x36, 0x3f, 0x3f, 0x1e, 0x0c, 0x08, 0x00, 0x00
};

const uint8_t icon_cute_star_8x8[ICON8_H] U8X8_PROGMEM = {
    0x08, 0x49, 0x3e, 0x1c, 0x3e, 0x49, 0x08, 0x00
};

const uint8_t icon_cute_bike_8x8[ICON8_H] U8X8_PROGMEM = {
    0x00, 0x06, 0x2f, 0x3a, 0x3c, 0x2a, 0x00, 0x00
};
