#include "Vario_Screen1.h"

#include "Vario_Display_Common.h"
#include "../Vario_Settings/Vario_Settings.h"
#include "../Vario_State/Vario_State.h"

#include <math.h>
#include <stdio.h>
#include <stdlib.h>

#define VARIO_SCREEN1_LEFT_GAUGE_X      6
#define VARIO_SCREEN1_LEFT_GAUGE_Y     12
#define VARIO_SCREEN1_LEFT_GAUGE_W      38
#define VARIO_SCREEN1_LEFT_GAUGE_H     104

#define VARIO_SCREEN1_RIGHT_GAUGE_X   196
#define VARIO_SCREEN1_RIGHT_GAUGE_Y    12
#define VARIO_SCREEN1_RIGHT_GAUGE_W    38
#define VARIO_SCREEN1_RIGHT_GAUGE_H   104

#define VARIO_SCREEN1_CENTER_X         48
#define VARIO_SCREEN1_CENTER_Y         10
#define VARIO_SCREEN1_CENTER_W        144
#define VARIO_SCREEN1_CENTER_H        108

#define VARIO_SCREEN1_VARIO_RANGE_MPS  5.0f
#define VARIO_SCREEN1_GS_MAX_KMH      80.0f

static int32_t vario_screen1_round_tenths(float value)
{
    return (int32_t)lroundf(value * 10.0f);
}

static void vario_screen1_format_signed_1dec(char *buf, size_t buf_len, float value)
{
    int32_t tenths;
    int32_t abs_tenths;
    char    sign;

    tenths = vario_screen1_round_tenths(value);
    sign = (tenths < 0) ? '-' : '+';

    abs_tenths = tenths;
    if (abs_tenths < 0)
    {
        abs_tenths = -abs_tenths;
    }

    snprintf(buf,
             buf_len,
             "%c%ld.%1ld",
             sign,
             (long)(abs_tenths / 10),
             (long)(abs_tenths % 10));
}

static void vario_screen1_format_unsigned_1dec(char *buf, size_t buf_len, float value)
{
    int32_t tenths;

    if (value < 0.0f)
    {
        value = 0.0f;
    }

    tenths = vario_screen1_round_tenths(value);

    snprintf(buf,
             buf_len,
             "%ld.%1ld",
             (long)(tenths / 10),
             (long)(tenths % 10));
}

static void vario_screen1_format_alt_from_m(char *buf, size_t buf_len, float altitude_m)
{
    int32_t value;

    value = Vario_Settings_AltitudeMetersToDisplayRounded(altitude_m);
    snprintf(buf, buf_len, "%ld", (long)value);
}

static void vario_screen1_format_alt_from_cm(char *buf, size_t buf_len, int32_t altitude_cm)
{
    float altitude_m;
    int32_t value;

    altitude_m = ((float)altitude_cm) * 0.01f;
    value      = Vario_Settings_AltitudeMetersToDisplayRounded(altitude_m);

    snprintf(buf, buf_len, "%ld", (long)value);
}

static void vario_screen1_format_qnh(char *buf, size_t buf_len)
{
    snprintf(buf,
             buf_len,
             "%ld.%ld",
             (long)Vario_Settings_GetQnhDisplayWhole(),
             (long)Vario_Settings_GetQnhDisplayFrac1());
}

static void vario_screen1_format_pressure(char *buf, size_t buf_len, int32_t pressure_hpa_x100)
{
    int32_t whole;
    int32_t frac1;

    whole = pressure_hpa_x100 / 100;
    frac1 = pressure_hpa_x100 % 100;
    if (frac1 < 0)
    {
        frac1 = -frac1;
    }

    snprintf(buf,
             buf_len,
             "%ld.%1ld",
             (long)whole,
             (long)(frac1 / 10));
}

static void vario_screen1_format_temp(char *buf, size_t buf_len, int32_t temp_c_x100)
{
    int32_t whole;
    int32_t frac1;

    whole = temp_c_x100 / 100;
    frac1 = temp_c_x100 % 100;
    if (frac1 < 0)
    {
        frac1 = -frac1;
    }

    snprintf(buf,
             buf_len,
             "%ld.%1ld",
             (long)whole,
             (long)(frac1 / 10));
}

static void vario_screen1_draw_vario_gauge(u8g2_t *u8g2,
                                           int16_t x,
                                           int16_t y,
                                           int16_t w,
                                           int16_t h,
                                           float vario_mps,
                                           const char *value_text,
                                           const char *unit_text)
{
    int16_t inner_x;
    int16_t inner_y;
    int16_t inner_w;
    int16_t inner_h;
    int16_t track_x;
    int16_t center_y;
    int16_t pointer_y;
    int16_t tick;
    float   clamped;
    float   ratio;

    inner_x = (int16_t)(x + 2);
    inner_y = (int16_t)(y + 10);
    inner_w = (int16_t)(w - 4);
    inner_h = (int16_t)(h - 20);
    track_x = (int16_t)(x + (w / 2));
    center_y = (int16_t)(inner_y + (inner_h / 2));

    clamped = vario_mps;
    if (clamped >  VARIO_SCREEN1_VARIO_RANGE_MPS) clamped =  VARIO_SCREEN1_VARIO_RANGE_MPS;
    if (clamped < -VARIO_SCREEN1_VARIO_RANGE_MPS) clamped = -VARIO_SCREEN1_VARIO_RANGE_MPS;

    ratio = clamped / VARIO_SCREEN1_VARIO_RANGE_MPS;
    pointer_y = (int16_t)(center_y - (ratio * ((float)inner_h * 0.5f)));

    /* ---------------------------------------------------------------------- */
    /*  좌측 세로 게이지 외곽                                                   */
    /*  - 화면 좌측 가장자리에 붙은 climb/sink 전용 바디                        */
    /*  - 사용 영역: x=6~43, y=12~115                                           */
    /* ---------------------------------------------------------------------- */
    u8g2_DrawFrame(u8g2, (uint8_t)x, (uint8_t)y, (uint8_t)w, (uint8_t)h);

    /* ---------------------------------------------------------------------- */
    /*  게이지 제목                                                            */
    /*  - 좌측 게이지 상단 1줄                                                  */
    /*  - 어떤 축이 "상승/하강률" 인지 명확히 알려 준다.                        */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    Vario_Display_DrawTextCentered(u8g2, (int16_t)(x + (w / 2)), (int16_t)(y + 8), "VARIO");

    /* ---------------------------------------------------------------------- */
    /*  게이지 중심선                                                          */
    /*  - 0.0m/s 기준선                                                         */
    /*  - 실제 variometer 바늘의 neutral line 역할                              */
    /* ---------------------------------------------------------------------- */
    u8g2_DrawHLine(u8g2, (uint8_t)inner_x, (uint8_t)center_y, (uint8_t)inner_w);

    /* ---------------------------------------------------------------------- */
    /*  세로 트랙                                                              */
    /*  - pointer 가 위아래로 움직이는 기준 축                                 */
    /* ---------------------------------------------------------------------- */
    u8g2_DrawVLine(u8g2, (uint8_t)track_x, (uint8_t)inner_y, (uint8_t)inner_h);

    /* ---------------------------------------------------------------------- */
    /*  눈금                                                                    */
    /*  - 실기기 느낌을 살리기 위해 5등분 보조 tick 을 배치                     */
    /* ---------------------------------------------------------------------- */
    for (tick = 0; tick <= 10; ++tick)
    {
        int16_t tick_y;
        int16_t tick_len;

        tick_y = (int16_t)(inner_y + ((inner_h * tick) / 10));
        tick_len = ((tick == 5) || (tick == 0) || (tick == 10)) ? 10 : 6;

        u8g2_DrawHLine(u8g2,
                       (uint8_t)(track_x - (tick_len / 2)),
                       (uint8_t)tick_y,
                       (uint8_t)tick_len);
    }

    /* ---------------------------------------------------------------------- */
    /*  vario fill bar                                                          */
    /*  - 0 중심에서 현재 pointer 위치까지 박스로 채운다                        */
    /*  - 상승 시 위쪽, 하강 시 아래쪽으로만 채워져서                           */
    /*    실제 variometer needle band 느낌을 만든다.                            */
    /* ---------------------------------------------------------------------- */
    if (pointer_y < center_y)
    {
        u8g2_DrawBox(u8g2,
                     (uint8_t)(track_x - 4),
                     (uint8_t)pointer_y,
                     8u,
                     (uint8_t)(center_y - pointer_y));
    }
    else
    {
        u8g2_DrawBox(u8g2,
                     (uint8_t)(track_x - 4),
                     (uint8_t)center_y,
                     8u,
                     (uint8_t)(pointer_y - center_y));
    }

    /* ---------------------------------------------------------------------- */
    /*  숫자 값                                                                 */
    /*  - 사용자가 요구한 "상하 게이지가 있는 곳에 바리오 값 표시" 구현부        */
    /*  - 게이지 하단 안쪽에 크게 넣어 즉시 읽을 수 있게 한다.                 */
    /* ---------------------------------------------------------------------- */
    u8g2_SetDrawColor(u8g2, 1);
    u8g2_SetFont(u8g2, u8g2_font_6x10_mf);
    Vario_Display_DrawTextCentered(u8g2,
                                   (int16_t)(x + (w / 2)),
                                   (int16_t)(y + h - 8),
                                   value_text);

    /* ---------------------------------------------------------------------- */
    /*  단위 문자열                                                             */
    /*  - 숫자 바로 아래 작은 단위                                              */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFont(u8g2, u8g2_font_4x6_tf);
    Vario_Display_DrawTextCentered(u8g2,
                                   (int16_t)(x + (w / 2)),
                                   (int16_t)(y + h - 2),
                                   unit_text);
}

static void vario_screen1_draw_gs_gauge(u8g2_t *u8g2,
                                        int16_t x,
                                        int16_t y,
                                        int16_t w,
                                        int16_t h,
                                        float gs_kmh,
                                        const char *value_text)
{
    int16_t inner_x;
    int16_t inner_y;
    int16_t inner_w;
    int16_t inner_h;
    int16_t track_x;
    int16_t fill_top_y;
    int16_t tick;
    float   clamped;
    float   ratio;

    inner_x = (int16_t)(x + 2);
    inner_y = (int16_t)(y + 10);
    inner_w = (int16_t)(w - 4);
    inner_h = (int16_t)(h - 20);
    track_x = (int16_t)(x + (w / 2));

    clamped = gs_kmh;
    if (clamped < 0.0f) clamped = 0.0f;
    if (clamped > VARIO_SCREEN1_GS_MAX_KMH) clamped = VARIO_SCREEN1_GS_MAX_KMH;

    ratio = clamped / VARIO_SCREEN1_GS_MAX_KMH;
    fill_top_y = (int16_t)(inner_y + inner_h - (ratio * inner_h));

    /* ---------------------------------------------------------------------- */
    /*  우측 GS 세로 게이지 외곽                                                */
    /*  - 화면 우측 가장자리 x=196~233                                          */
    /*  - GS(ground speed) 전용 세로 축                                        */
    /* ---------------------------------------------------------------------- */
    u8g2_DrawFrame(u8g2, (uint8_t)x, (uint8_t)y, (uint8_t)w, (uint8_t)h);

    /* ---------------------------------------------------------------------- */
    /*  우측 게이지 제목                                                        */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    Vario_Display_DrawTextCentered(u8g2, (int16_t)(x + (w / 2)), (int16_t)(y + 8), "GS");

    /* ---------------------------------------------------------------------- */
    /*  세로 기준 트랙                                                          */
    /* ---------------------------------------------------------------------- */
    u8g2_DrawVLine(u8g2, (uint8_t)track_x, (uint8_t)inner_y, (uint8_t)inner_h);

    /* ---------------------------------------------------------------------- */
    /*  GS scale tick                                                          */
    /*  - 0~80km/h 를 10등분한 보조 눈금                                       */
    /* ---------------------------------------------------------------------- */
    for (tick = 0; tick <= 10; ++tick)
    {
        int16_t tick_y;
        int16_t tick_len;

        tick_y = (int16_t)(inner_y + ((inner_h * tick) / 10));
        tick_len = ((tick == 0) || (tick == 10)) ? 10 : 6;

        u8g2_DrawHLine(u8g2,
                       (uint8_t)(track_x - (tick_len / 2)),
                       (uint8_t)tick_y,
                       (uint8_t)tick_len);
    }

    /* ---------------------------------------------------------------------- */
    /*  GS fill column                                                         */
    /*  - 하단에서 위로 차오르는 막대형 표시                                    */
    /*  - 실제 ground speed 크기를 한눈에 읽기 쉽게 한다.                      */
    /* ---------------------------------------------------------------------- */
    u8g2_DrawBox(u8g2,
                 (uint8_t)(track_x - 4),
                 (uint8_t)fill_top_y,
                 8u,
                 (uint8_t)((inner_y + inner_h) - fill_top_y));

    /* ---------------------------------------------------------------------- */
    /*  GS 숫자 값                                                              */
    /*  - 우측 게이지 하단 안쪽                                                 */
    /* ---------------------------------------------------------------------- */
    u8g2_SetDrawColor(u8g2, 1);
    u8g2_SetFont(u8g2, u8g2_font_6x10_mf);
    Vario_Display_DrawTextCentered(u8g2,
                                   (int16_t)(x + (w / 2)),
                                   (int16_t)(y + h - 8),
                                   value_text);

    /* ---------------------------------------------------------------------- */
    /*  GS 단위                                                                 */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFont(u8g2, u8g2_font_4x6_tf);
    Vario_Display_DrawTextCentered(u8g2,
                                   (int16_t)(x + (w / 2)),
                                   (int16_t)(y + h - 2),
                                   "km/h");
}

void Vario_Screen1_Render(u8g2_t *u8g2, const vario_buttonbar_t *buttonbar)
{
    const vario_viewport_t *v;
    const vario_runtime_t  *rt;
    const vario_settings_t *settings;
    char                    qnh_text[20];
    char                    pressure_text[20];
    char                    temp_text[20];
    char                    now_alt_text[20];
    char                    alt1_text[20];
    char                    alt2_text[20];
    char                    alt3_text[20];
    char                    vario_text[20];
    char                    gs_text[20];
    char                    unit_text[8];
    char                    vs_unit_text[8];

    (void)buttonbar;

    v        = Vario_Display_GetFullViewport();
    rt       = Vario_State_GetRuntime();
    settings = Vario_Settings_Get();

    vario_screen1_format_qnh(qnh_text, sizeof(qnh_text));
    vario_screen1_format_pressure(pressure_text, sizeof(pressure_text), rt->pressure_hpa_x100);
    vario_screen1_format_temp(temp_text, sizeof(temp_text), rt->temperature_c_x100);
    vario_screen1_format_alt_from_m(now_alt_text, sizeof(now_alt_text), rt->baro_altitude_m);
    vario_screen1_format_alt_from_cm(alt1_text, sizeof(alt1_text), settings->alt1_cm);
    vario_screen1_format_alt_from_cm(alt2_text, sizeof(alt2_text), settings->alt2_cm);
    vario_screen1_format_alt_from_cm(alt3_text, sizeof(alt3_text), settings->alt3_cm);
    vario_screen1_format_signed_1dec(vario_text, sizeof(vario_text), rt->baro_vario_mps);
    vario_screen1_format_unsigned_1dec(gs_text, sizeof(gs_text), rt->ground_speed_kmh);
    snprintf(unit_text, sizeof(unit_text), "%s", Vario_Settings_GetAltitudeUnitText());
    snprintf(vs_unit_text, sizeof(vs_unit_text), "%s", Vario_Settings_GetVSpeedUnitText());

    /* ---------------------------------------------------------------------- */
    /*  Screen1 은 사용자가 요구한 대로 240x128 전체를 그대로 쓰는 full-screen */
    /*  레이아웃이다.                                                          */
    /* ---------------------------------------------------------------------- */
    (void)v;

    /* ---------------------------------------------------------------------- */
    /*  상단 실시간 상태 줄                                                     */
    /*  - 좌측: 현재 baro altitude (실시간)                                     */
    /*  - 중앙: 현재 QNH                                                        */
    /*  - 우측: 현재 pressure / temperature                                     */
    /*                                                                        */
    /*  이 줄은 "실시간 센서 상태" 를 보여 주고,                                */
    /*  아래 큰 ALT1/ALT2/ALT3 블록은 사용자가 저장한 reference 값을 보여 준다. */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);

    /* 현재 실시간 고도 라벨과 값: 화면 상단 중앙 좌측 */
    u8g2_DrawStr(u8g2, 52u, 8u, "BARO");
    u8g2_SetFont(u8g2, u8g2_font_6x10_mf);
    u8g2_DrawStr(u8g2, 82u, 9u, now_alt_text);
    u8g2_SetFont(u8g2, u8g2_font_4x6_tf);
    u8g2_DrawStr(u8g2, 112u, 9u, unit_text);

    /* 현재 QNH: 상단 중앙 */
    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    u8g2_DrawStr(u8g2, 118u, 8u, "QNH");
    u8g2_SetFont(u8g2, u8g2_font_6x10_mf);
    u8g2_DrawStr(u8g2, 142u, 9u, qnh_text);

    /* 현재 pressure: 상단 우측 첫 번째 상태 */
    u8g2_SetFont(u8g2, u8g2_font_4x6_tf);
    u8g2_DrawStr(u8g2, 172u, 6u, pressure_text);
    u8g2_DrawStr(u8g2, 208u, 6u, "hPa");

    /* 현재 temperature: 상단 우측 두 번째 상태 */
    u8g2_DrawStr(u8g2, 172u, 11u, temp_text);
    u8g2_DrawStr(u8g2, 208u, 11u, "C");

    /* ---------------------------------------------------------------------- */
    /*  좌측 vario 세로 게이지                                                  */
    /*  - 사용자가 요구한 "좌측에 상하 속도 계기판"                            */
    /*  - 숫자 vario 값도 같은 기둥 안에 함께 표시                              */
    /* ---------------------------------------------------------------------- */
    vario_screen1_draw_vario_gauge(u8g2,
                                   VARIO_SCREEN1_LEFT_GAUGE_X,
                                   VARIO_SCREEN1_LEFT_GAUGE_Y,
                                   VARIO_SCREEN1_LEFT_GAUGE_W,
                                   VARIO_SCREEN1_LEFT_GAUGE_H,
                                   rt->baro_vario_mps,
                                   vario_text,
                                   vs_unit_text);

    /* ---------------------------------------------------------------------- */
    /*  우측 GS 세로 게이지                                                     */
    /*  - 사용자가 요구한 "우측에 GS 계기판"                                  */
    /*  - 0~80km/h 범위를 세로 막대로 보여 준다.                               */
    /* ---------------------------------------------------------------------- */
    vario_screen1_draw_gs_gauge(u8g2,
                                VARIO_SCREEN1_RIGHT_GAUGE_X,
                                VARIO_SCREEN1_RIGHT_GAUGE_Y,
                                VARIO_SCREEN1_RIGHT_GAUGE_W,
                                VARIO_SCREEN1_RIGHT_GAUGE_H,
                                rt->ground_speed_kmh,
                                gs_text);

    /* ---------------------------------------------------------------------- */
    /*  중앙 ALT1 카드 상단 라벨                                                */
    /*  - 중앙 대형 숫자 블록이 ALT1 임을 명확히 표시                           */
    /*  - 위치: 중앙 영역 상단 x=102 기준                                       */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFont(u8g2, u8g2_font_6x12_mf);
    Vario_Display_DrawTextCentered(u8g2, 120, 24, "ALT1");

    /* ---------------------------------------------------------------------- */
    /*  중앙 ALT1 대형 값                                                       */
    /*  - 사용자가 요구한 "ALT1 크게" 구현부                                   */
    /*  - 위치: 화면 중앙 x=120, baseline y=58                                  */
    /*  - 실제 값은 Vario settings 의 ALT1 reference 값                         */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFont(u8g2, u8g2_font_logisoso20_tf);
    Vario_Display_DrawTextCentered(u8g2, 120, 58, alt1_text);

    /* ALT1 단위: 대형 숫자 우측 하단 */
    u8g2_SetFont(u8g2, u8g2_font_6x10_mf);
    u8g2_DrawStr(u8g2, 162u, 58u, unit_text);

    /* ---------------------------------------------------------------------- */
    /*  중앙 하단 ALT2 라벨과 값                                                */
    /*  - 사용자가 요구한 "ALT2 ALT3 를 ALT1 밑에 중간 크기로 표시"           */
    /*  - 좌측 절반 카드                                                        */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFont(u8g2, u8g2_font_6x10_mf);
    u8g2_DrawStr(u8g2, 62u, 78u, "ALT2");
    u8g2_SetFont(u8g2, u8g2_font_9x15_mf);
    u8g2_DrawStr(u8g2, 56u, 98u, alt2_text);
    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    u8g2_DrawStr(u8g2, 106u, 98u, unit_text);

    /* ---------------------------------------------------------------------- */
    /*  중앙 하단 ALT3 라벨과 값                                                */
    /*  - ALT2 카드 오른쪽 절반                                                 */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFont(u8g2, u8g2_font_6x10_mf);
    u8g2_DrawStr(u8g2, 142u, 78u, "ALT3");
    u8g2_SetFont(u8g2, u8g2_font_9x15_mf);
    u8g2_DrawStr(u8g2, 136u, 98u, alt3_text);
    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    u8g2_DrawStr(u8g2, 186u, 98u, unit_text);

    /* ---------------------------------------------------------------------- */
    /*  ALT2 / ALT3 구획선                                                     */
    /*  - 중앙 하단 두 개의 중형 필드를 카드처럼 분리                            */
    /* ---------------------------------------------------------------------- */
    u8g2_DrawFrame(u8g2, 52u, 66u, 136u, 40u);
    u8g2_DrawVLine(u8g2, 120u, 66u, 40u);

    /* ---------------------------------------------------------------------- */
    /*  raw overlay                                                            */
    /*  - 개발 모드에서만 화면 맨 아래 작은 글씨로 pressure/temperature 출력     */
    /* ---------------------------------------------------------------------- */
    if (settings->audio_enabled != 0u)
    {
        u8g2_SetFont(u8g2, u8g2_font_4x6_tf);
        u8g2_DrawStr(u8g2, 96u, 126u, "AUD ON");
    }
    else
    {
        u8g2_SetFont(u8g2, u8g2_font_4x6_tf);
        u8g2_DrawStr(u8g2, 96u, 126u, "AUD OFF");
    }

    /* 개발용 raw overlay: full-screen 하단 좌측 */
    Vario_Display_DrawRawOverlay(u8g2, Vario_Display_GetFullViewport());
}
