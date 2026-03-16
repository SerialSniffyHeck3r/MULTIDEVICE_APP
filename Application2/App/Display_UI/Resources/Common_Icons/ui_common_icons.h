#ifndef UI_COMMON_ICONS_H
#define UI_COMMON_ICONS_H

#include <stdint.h>
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  Fixed icon geometry                                                        */
/* -------------------------------------------------------------------------- */
#define ICON5_W   5
#define ICON5_H   7

#define ICON7_W   7
#define ICON7_H   7

#define ICON11_W  11
#define ICON11_H  7

#define ICON13_W  13
#define ICON13_H  13

#define ICON7X4_W 7
#define ICON7X4_H 4

#define ICON8_W   8
#define ICON8_H   8

/* -------------------------------------------------------------------------- */
/*  Existing status bar icons                                                  */
/*                                                                            */
/*  사용자가 올린 icons.c의 기존 아이콘은 이름과 픽셀 규격을 그대로 유지한다. */
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

extern const uint8_t icon_antenna_shape[ICON5_H];
extern const uint8_t icon_degrees[ICON7_H];
extern const uint8_t icon_bluetooth_bits[ICON7_H];

/* -------------------------------------------------------------------------- */
/*  GPS info-pane 13x13 icons                                                  */
/*                                                                            */
/*  사용자가 지정한 XBM bit pattern을 그대로 넣는다.                           */
/*  - altitude accuracy  : 높이 정확도                                        */
/*  - position accuracy  : 위치 정확도                                        */
/* -------------------------------------------------------------------------- */
extern const uint8_t icon_gps_altitude_accuracy_13x13[ICON13_H * 2];
extern const uint8_t icon_gps_position_accuracy_13x13[ICON13_H * 2];

/* -------------------------------------------------------------------------- */
/*  Bottom bar 4-pixel-high arrows                                             */
/* -------------------------------------------------------------------------- */
extern const uint8_t icon_arrow_right_7x4[ICON7X4_H];
extern const uint8_t icon_arrow_left_7x4[ICON7X4_H];
extern const uint8_t icon_arrow_up_7x4[ICON7X4_H];
extern const uint8_t icon_arrow_down_7x4[ICON7X4_H];

/* -------------------------------------------------------------------------- */
/*  Cute icon examples                                                         */
/* -------------------------------------------------------------------------- */
extern const uint8_t icon_cute_cat_8x8[ICON8_H];
extern const uint8_t icon_cute_heart_8x8[ICON8_H];
extern const uint8_t icon_cute_star_8x8[ICON8_H];
extern const uint8_t icon_cute_bike_8x8[ICON8_H];

/* -------------------------------------------------------------------------- */
/*  UI message icons                                                           */
/*                                                                            */
/*  토스트/팝업 테스트에서 바로 쓸 수 있도록 "형태가 보이는" 아이콘을 따로 둔다. */
/* -------------------------------------------------------------------------- */
extern const uint8_t icon_ui_info_8x8[ICON8_H];
extern const uint8_t icon_ui_warn_8x8[ICON8_H];
extern const uint8_t icon_ui_ok_8x8[ICON8_H];
extern const uint8_t icon_ui_bell_8x8[ICON8_H];
extern const uint8_t icon_ui_folder_8x8[ICON8_H];

#ifdef __cplusplus
}
#endif

#endif /* UI_COMMON_ICONS_H */
