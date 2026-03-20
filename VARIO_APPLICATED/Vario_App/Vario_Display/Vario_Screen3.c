#include "Vario_Screen3.h"

#include "Vario_Display_Common.h"
#include "Vario_Settings.h"
#include "Vario_State.h"

#include <math.h>
#include <stdio.h>

static void vario_screen3_format_alt_cm(char *buf, size_t buf_len, int32_t altitude_cm)
{
    float altitude_m;
    int32_t value;

    altitude_m = ((float)altitude_cm) * 0.01f;
    value      = Vario_Settings_AltitudeMetersToDisplayRounded(altitude_m);

    snprintf(buf, buf_len, "%ld", (long)value);
}

static void vario_screen3_format_qnh(char *buf, size_t buf_len)
{
    snprintf(buf,
             buf_len,
             "%ld.%ld",
             (long)Vario_Settings_GetQnhDisplayWhole(),
             (long)Vario_Settings_GetQnhDisplayFrac1());
}

void Vario_Screen3_Render(u8g2_t *u8g2, const vario_buttonbar_t *buttonbar)
{
    const vario_runtime_t  *rt;
    const vario_settings_t *settings;
    char                    alt1_text[24];
    char                    alt2_text[24];
    char                    alt3_text[24];
    char                    qnh_text[24];
    char                    unit_text[24];
    char                    audio_text[24];

    (void)buttonbar;

    rt       = Vario_State_GetRuntime();
    settings = Vario_Settings_Get();

    vario_screen3_format_alt_cm(alt1_text, sizeof(alt1_text), settings->alt1_cm);
    vario_screen3_format_alt_cm(alt2_text, sizeof(alt2_text), settings->alt2_cm);
    vario_screen3_format_alt_cm(alt3_text, sizeof(alt3_text), settings->alt3_cm);
    vario_screen3_format_qnh(qnh_text, sizeof(qnh_text));
    snprintf(unit_text,
             sizeof(unit_text),
             "%s / %s",
             Vario_Settings_GetAltitudeUnitText(),
             Vario_Settings_GetVSpeedUnitText());
    snprintf(audio_text,
             sizeof(audio_text),
             "%s %u%%",
             Vario_Settings_GetAudioOnOffText(),
             (unsigned)settings->audio_volume_percent);

    /* ---------------------------------------------------------------------- */
    /*  Screen3 full-screen 정보 패널                                           */
    /*  - 저장된 ALT1/ALT2/ALT3                                                 */
    /*  - 현재 QNH / unit / audio                                               */
    /*  - 현재 pressure / temperature                                           */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFont(u8g2, u8g2_font_6x12_mf);
    u8g2_DrawStr(u8g2, 4u, 12u, "PRESET / STATUS");

    /* ALT1 카드 */
    u8g2_DrawFrame(u8g2, 6u, 18u, 72u, 42u);
    u8g2_SetFont(u8g2, u8g2_font_6x10_mf);
    u8g2_DrawStr(u8g2, 12u, 30u, "ALT1");
    u8g2_SetFont(u8g2, u8g2_font_10x20_mf);
    u8g2_DrawStr(u8g2, 12u, 52u, alt1_text);

    /* ALT2 카드 */
    u8g2_DrawFrame(u8g2, 84u, 18u, 72u, 42u);
    u8g2_SetFont(u8g2, u8g2_font_6x10_mf);
    u8g2_DrawStr(u8g2, 90u, 30u, "ALT2");
    u8g2_SetFont(u8g2, u8g2_font_10x20_mf);
    u8g2_DrawStr(u8g2, 90u, 52u, alt2_text);

    /* ALT3 카드 */
    u8g2_DrawFrame(u8g2, 162u, 18u, 72u, 42u);
    u8g2_SetFont(u8g2, u8g2_font_6x10_mf);
    u8g2_DrawStr(u8g2, 168u, 30u, "ALT3");
    u8g2_SetFont(u8g2, u8g2_font_10x20_mf);
    u8g2_DrawStr(u8g2, 168u, 52u, alt3_text);

    /* 하단 좌측 상태 카드: QNH / unit / audio */
    u8g2_DrawFrame(u8g2, 6u, 68u, 110u, 50u);
    u8g2_SetFont(u8g2, u8g2_font_6x10_mf);
    u8g2_DrawStr(u8g2, 12u, 82u, "QNH");
    u8g2_DrawStr(u8g2, 48u, 82u, qnh_text);
    u8g2_DrawStr(u8g2, 12u, 96u, "UNIT");
    u8g2_DrawStr(u8g2, 48u, 96u, unit_text);
    u8g2_DrawStr(u8g2, 12u, 110u, "AUD");
    u8g2_DrawStr(u8g2, 48u, 110u, audio_text);

    /* 하단 우측 상태 카드: pressure / temp / live GS */
    u8g2_DrawFrame(u8g2, 124u, 68u, 110u, 50u);
    u8g2_SetFont(u8g2, u8g2_font_6x10_mf);
    {
        char buf[24];
        int32_t whole;
        int32_t frac;

        whole = rt->pressure_hpa_x100 / 100;
        frac  = rt->pressure_hpa_x100 % 100;
        if (frac < 0) frac = -frac;
        snprintf(buf, sizeof(buf), "%ld.%1ld hPa", (long)whole, (long)(frac / 10));
        u8g2_DrawStr(u8g2, 130u, 82u, buf);

        whole = rt->temperature_c_x100 / 100;
        frac  = rt->temperature_c_x100 % 100;
        if (frac < 0) frac = -frac;
        snprintf(buf, sizeof(buf), "%ld.%1ld C", (long)whole, (long)(frac / 10));
        u8g2_DrawStr(u8g2, 130u, 96u, buf);

        snprintf(buf, sizeof(buf), "GS %ld km/h", (long)lroundf(rt->ground_speed_kmh));
        u8g2_DrawStr(u8g2, 130u, 110u, buf);
    }

    /* 개발용 raw overlay: full-screen 하단 좌측 */
    Vario_Display_DrawRawOverlay(u8g2, Vario_Display_GetFullViewport());
}
