#ifndef VARIO_SETTINGS_H
#define VARIO_SETTINGS_H

#include <stdbool.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  바리오 앱 사용자 설정 저장소                                               */
/*                                                                            */
/*  현재는 RAM 저장소이지만, 구조체를 따로 빼 두어                              */
/*  추후 Winbond flash 영구 저장으로 자연스럽게 옮길 수 있게 한다.              */
/* -------------------------------------------------------------------------- */
typedef enum
{
    VARIO_ALT_UNIT_METER = 0u,
    VARIO_ALT_UNIT_FEET  = 1u
} vario_alt_unit_t;

typedef enum
{
    VARIO_VSPEED_UNIT_MPS = 0u,
    VARIO_VSPEED_UNIT_FPM = 1u
} vario_vspeed_unit_t;

typedef enum
{
    VARIO_QUICKSET_ITEM_QNH = 0u,
    VARIO_QUICKSET_ITEM_ALT_UNIT,
    VARIO_QUICKSET_ITEM_VSPEED_UNIT,
    VARIO_QUICKSET_ITEM_AUDIO_ENABLE,
    VARIO_QUICKSET_ITEM_AUDIO_VOLUME,
    VARIO_QUICKSET_ITEM_COUNT
} vario_quickset_item_t;

typedef enum
{
    VARIO_VALUE_ITEM_QNH = 0u,
    VARIO_VALUE_ITEM_ALT1,
    VARIO_VALUE_ITEM_ALT2,
    VARIO_VALUE_ITEM_ALT3,
    VARIO_VALUE_ITEM_ALT_UNIT,
    VARIO_VALUE_ITEM_VSPEED_UNIT,
    VARIO_VALUE_ITEM_COUNT
} vario_value_item_t;

typedef struct
{
    int32_t             qnh_hpa_x100;
    int32_t             alt1_cm;
    int32_t             alt2_cm;
    int32_t             alt3_cm;
    vario_alt_unit_t    altitude_unit;
    vario_vspeed_unit_t vspeed_unit;
    uint8_t             audio_enabled;
    uint8_t             audio_volume_percent;
} vario_settings_t;

void Vario_Settings_Init(void);
const vario_settings_t *Vario_Settings_Get(void);

void Vario_Settings_AdjustQuickSet(vario_quickset_item_t item, int8_t direction);
void Vario_Settings_AdjustValue(vario_value_item_t item, int8_t direction);

int32_t Vario_Settings_GetQnhDisplayWhole(void);
int32_t Vario_Settings_GetQnhDisplayFrac1(void);

int32_t Vario_Settings_AltitudeMetersToDisplayRounded(float altitude_m);
int32_t Vario_Settings_VSpeedToDisplayRounded(float vspd_mps);
const char *Vario_Settings_GetAltitudeUnitText(void);
const char *Vario_Settings_GetVSpeedUnitText(void);
const char *Vario_Settings_GetAudioOnOffText(void);

#ifdef __cplusplus
}
#endif

#endif /* VARIO_SETTINGS_H */
