#include "Vario_Dev.h"

static vario_dev_settings_t s_vario_dev;

void Vario_Dev_Init(void)
{
    s_vario_dev.raw_overlay_enabled = 0u;
    s_vario_dev.force_full_redraw   = 0u;
}

const vario_dev_settings_t *Vario_Dev_Get(void)
{
    return &s_vario_dev;
}

void Vario_Dev_ToggleRawOverlay(void)
{
    if (s_vario_dev.raw_overlay_enabled == 0u)
    {
        s_vario_dev.raw_overlay_enabled = 1u;
    }
    else
    {
        s_vario_dev.raw_overlay_enabled = 0u;
    }

    /* ---------------------------------------------------------------------- */
    /*  overlay 토글 직후에는 한 프레임 강제 redraw 가 필요하다.                */
    /* ---------------------------------------------------------------------- */
    s_vario_dev.force_full_redraw = 1u;
}

void Vario_Dev_ClearForceFullRedraw(void)
{
    s_vario_dev.force_full_redraw = 0u;
}
