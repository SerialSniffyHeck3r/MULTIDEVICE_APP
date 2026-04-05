#ifndef U8G2_UC1608_STM32_H
#define U8G2_UC1608_STM32_H

#include "APP_STATE.h"
#include "main.h"
#include "u8g2.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint8_t initialized;              /* U8G2 / UC1608 init 완료 여부             */
    uint8_t smart_update_enable;      /* 호환성 플래그. 현재는 단일 full-buffer commit 경로 */
    uint8_t frame_limit_enable;       /* frame token 제한 사용 여부               */
    uint8_t reserved0;

    uint16_t buffer_size;             /* 실제 u8g2 buffer size                    */
    uint8_t frame_tokens;             /* 현재 적립된 frame token 수               */
    uint8_t reserved1[3];

    app_uc1608_settings_t applied;    /* 마지막으로 패널에 반영한 raw 설정        */
} u8g2_uc1608_runtime_t;

extern volatile u8g2_uc1608_runtime_t g_u8g2_uc1608_runtime;

void U8G2_UC1608_Init(void);
void U8G2_UC1608_DrawTestScreen(void);
void U8G2_UC1608_EnableSmartUpdate(uint8_t enable);
void U8G2_UC1608_EnableFrameLimit(uint8_t enable);
void U8G2_UC1608_FrameTickFromISR(void);
uint8_t U8G2_UC1608_TryAcquireFrameToken(void);
void U8G2_UC1608_CommitBuffer(void);
u8g2_t *U8G2_UC1608_GetHandle(void);

/* -------------------------------------------------------------------------- */
/*  UC1608 raw panel control layer                                            */
/*                                                                            */
/*  이 레이어는 U8G2 draw API와 별개로,                                       */
/*  UC1608 컨트라스트 / 온도 보상 / bias / RAM access / start line /           */
/*  fixed line / power control / flip mode를 정리된 함수로 추상화한다.        */
/* -------------------------------------------------------------------------- */
void U8G2_UC1608_SetContrastRaw(uint8_t contrast_raw);
void U8G2_UC1608_SetTemperatureCompensation(uint8_t tc_raw_0_3);
void U8G2_UC1608_SetBiasRatio(uint8_t bias_raw_0_3);
void U8G2_UC1608_SetRamAccessMode(uint8_t ram_access_raw_0_3);
void U8G2_UC1608_SetDisplayStartLineRaw(uint8_t start_line_raw_0_63);
void U8G2_UC1608_SetFixedLineRaw(uint8_t fixed_line_raw_0_15);
void U8G2_UC1608_SetPowerControlRaw(uint8_t power_raw_0_7);
void U8G2_UC1608_SetFlipModeRaw(uint8_t flip_mode_raw_0_1);
void U8G2_UC1608_ApplyPanelSettings(const app_uc1608_settings_t *settings);
void U8G2_UC1608_LoadAndApplySettingsFromAppState(void);
void U8G2_UC1608_GetAppliedPanelSettings(app_uc1608_settings_t *dst);
void U8G2_UC1608_InvalidateSmartUpdateCache(void);

#ifdef __cplusplus
}
#endif

#endif /* U8G2_UC1608_STM32_H */
