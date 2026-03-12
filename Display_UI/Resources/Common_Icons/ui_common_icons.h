#ifndef UI_COMMON_ICONS_H
#define UI_COMMON_ICONS_H

#include <stdint.h>
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  Status bar icon geometry                                                   */
/* -------------------------------------------------------------------------- */
#define ICON5_W   5
#define ICON5_H   7

#define ICON7_W   7
#define ICON7_H   7

#define ICON11_W  11
#define ICON11_H  7

#define ICON7X4_W 7
#define ICON7X4_H 4

#define ICON8_W   8
#define ICON8_H   8

/* -------------------------------------------------------------------------- */
/*  Status bar icons                                                           */
/*                                                                            */
/*  업로드된 icons.c의 이름과 픽셀 규격을 가능한 한 그대로 유지한다.           */
/* -------------------------------------------------------------------------- */
extern const uint8_t icon_gps_main_bits[ICON7_H];
extern const uint8_t icon_gps_rx_7_bits[ICON7_H];
extern const uint8_t icon_gps_rx_6_bits[ICON7_H];
extern const uint8_t icon_gps_rx_5_bits[ICON7_H];
extern const uint8_t icon_gps_rx_4_bits[ICON7_H];
extern const uint8_t icon_gps_rx_3_bits[ICON7_H];
extern const uint8_t icon_gps_rx_2_bits[ICON7_H];
extern const uint8_t icon_gps_rx_1_bits[ICON7_H];

extern const uint8_t icon_gps_3d_bits[ICON11_H * 2];
extern const uint8_t icon_gps_2d_bits[ICON11_H * 2];
extern const uint8_t icon_gps_nofix_bits[ICON11_H * 2];
extern const uint8_t blank_11x7[ICON11_H * 2];

extern const uint8_t icon_mmc_present_bits[ICON7_H];
extern const uint8_t icon_mmc_not_present_bits[ICON7_H];
extern const uint8_t icon_mmc_error_bits[ICON7_H];

extern const uint8_t icon_rec_bits[ICON7_H];
extern const uint8_t icon_stop_bits[ICON7_H];
extern const uint8_t icon_pause_bits[ICON7_H];
extern const uint8_t blank_7x7[ICON7_H];

extern const uint8_t icon_antenna_shape[ICON7_H];
extern const uint8_t icon_degrees[ICON7_H];

/* -------------------------------------------------------------------------- */
/*  Newly added status bar icons                                               */
/*                                                                            */
/*  사용자가 추가 요청한 Bluetooth 아이콘과 그 옆의 7x7 보조 아이콘이다.        */
/* -------------------------------------------------------------------------- */
extern const uint8_t icon_bluetooth_bits[ICON7_H];
extern const uint8_t icon_bt_aux_bits[ICON7_H];

/* -------------------------------------------------------------------------- */
/*  Bottom bar 4-pixel-high XBM icons                                          */
/* -------------------------------------------------------------------------- */
extern const uint8_t icon_arrow_right_7x4[ICON7X4_H];
extern const uint8_t icon_arrow_left_7x4[ICON7X4_H];
extern const uint8_t icon_arrow_up_7x4[ICON7X4_H];
extern const uint8_t icon_arrow_down_7x4[ICON7X4_H];

/* -------------------------------------------------------------------------- */
/*  Cute test icons                                                            */
/* -------------------------------------------------------------------------- */
extern const uint8_t icon_cute_cat_8x8[ICON8_H];
extern const uint8_t icon_cute_heart_8x8[ICON8_H];
extern const uint8_t icon_cute_star_8x8[ICON8_H];
extern const uint8_t icon_cute_bike_8x8[ICON8_H];

#ifdef __cplusplus
}
#endif

#endif /* UI_COMMON_ICONS_H */
