#ifndef UI_SCREEN_GPS_MODULE_H
#define UI_SCREEN_GPS_MODULE_H

#include "ui_types.h"
#include "button.h"
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  GPS screen action                                                         */
/*                                                                            */
/*  이 화면의 버튼 해석은 ui_screen_gps.c 안에서 모두 끝낸다.                  */
/*  UI 엔진은 아래 action enum만 보고                                          */
/*  - 이전 메뉴로 복귀할지                                                     */
/*  만 결정한다.                                                               */
/* -------------------------------------------------------------------------- */
typedef enum
{
    UI_SCREEN_GPS_ACTION_NONE = 0,
    UI_SCREEN_GPS_ACTION_BACK_TO_PREVIOUS
} ui_screen_gps_action_t;

/* -------------------------------------------------------------------------- */
/*  Init                                                                      */
/*                                                                            */
/*  부팅 시 1회 호출한다.                                                     */
/*  - GPS 화면 내부 고정 상태를 초기화한다.                                    */
/*  - GPS 화면 전용 하단바 라벨을 준비한다.                                    */
/* -------------------------------------------------------------------------- */
void UI_ScreenGps_Init(void);

/* -------------------------------------------------------------------------- */
/*  On enter                                                                  */
/*                                                                            */
/*  다른 화면에서 GPS 화면으로 진입할 때 호출한다.                             */
/*  - 이 화면 전용 하단바를 항상 다시 세팅한다.                                */
/*  - draw에 필요한 실제 GPS 데이터는 APP_STATE snapshot을                     */
/*    매 프레임 다시 복사해 오므로 별도 캐시는 두지 않는다.                    */
/* -------------------------------------------------------------------------- */
void UI_ScreenGps_OnEnter(void);

/* -------------------------------------------------------------------------- */
/*  Button event handler                                                      */
/*                                                                            */
/*  short press 기준 버튼 매핑                                                */
/*  - F1 : 이전 메뉴로 복귀                                                   */
/*  - F2 : GPS ONLY 20Hz <-> FULL CONSTELLATION 10Hz 토글                      */
/*  - F3 : UBX-CFG-RST cold start 전송                                        */
/*  - F4/F5/F6 : 동작 없음                                                    */
/*                                                                            */
/*  중요                                                                      */
/*  - GPS 드라이버(Ublox_GPS.c)는 읽기/파싱 전용으로 유지한다.                 */
/*  - 따라서 F2/F3에서 필요한 UBX packet 조립과 송신은                        */
/*    이 화면 파일(ui_screen_gps.c) 안에서만 수행한다.                        */
/* -------------------------------------------------------------------------- */
ui_screen_gps_action_t UI_ScreenGps_HandleButtonEvent(const button_event_t *event,
                                                      uint32_t now_ms);

/* -------------------------------------------------------------------------- */
/*  Compose information                                                       */
/*                                                                            */
/*  이 화면은 항상                                                            */
/*  - status bar 표시                                                         */
/*  - bottom bar 표시                                                         */
/*  - main viewport = TOP+BTM fixed mode                                      */
/*  로 동작한다.                                                              */
/* -------------------------------------------------------------------------- */
ui_layout_mode_t UI_ScreenGps_GetLayoutMode(void);
bool UI_ScreenGps_IsStatusBarVisible(void);
bool UI_ScreenGps_IsBottomBarVisible(void);

/* -------------------------------------------------------------------------- */
/*  GPS screen renderer                                                       */
/*                                                                            */
/*  이 함수는 오직 main viewport 내부만 그린다.                                */
/*  status bar / bottom bar / popup / toast는 UI 엔진이 따로 합성한다.        */
/*                                                                            */
/*  imperial_units                                                            */
/*  - 0 : 정확도 단위를 m 로 표시                                              */
/*  - 1 : 정확도 단위를 ft 로 표시                                             */
/* -------------------------------------------------------------------------- */
void UI_ScreenGps_Draw(u8g2_t *u8g2,
                       const ui_rect_t *viewport,
                       uint8_t imperial_units);

#ifdef __cplusplus
}
#endif

#endif /* UI_SCREEN_GPS_MODULE_H */
