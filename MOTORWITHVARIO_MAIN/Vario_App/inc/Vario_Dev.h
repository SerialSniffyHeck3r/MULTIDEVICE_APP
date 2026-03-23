#ifndef VARIO_DEV_H
#define VARIO_DEV_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
    uint8_t raw_overlay_enabled;
    uint8_t force_full_redraw;
} vario_dev_settings_t;

void Vario_Dev_Init(void);
const vario_dev_settings_t *Vario_Dev_Get(void);
void Vario_Dev_ToggleRawOverlay(void);
void Vario_Dev_ClearForceFullRedraw(void);

#ifdef __cplusplus
}
#endif

#endif /* VARIO_DEV_H */
