#include "FW_BootShared.h"

#include <string.h>

static uint32_t FW_BootShared_CalcStructCrc(const fw_bootctrl_t *ctrl)
{
    fw_bootctrl_t temp;

    temp = *ctrl;
    temp.crc32 = 0u;

    return FW_BootShared_CalcCrc32(&temp, (uint32_t)sizeof(temp));
}

uint32_t FW_BootShared_CalcCrc32(const void *data, uint32_t length)
{
    const uint8_t *p;
    uint32_t crc;
    uint32_t i;

    p = (const uint8_t *)data;
    crc = 0xFFFFFFFFu;

    for (i = 0u; i < length; i++)
    {
        uint32_t bit;

        crc ^= (uint32_t)p[i];

        for (bit = 0u; bit < 8u; bit++)
        {
            if ((crc & 1u) != 0u)
            {
                crc = (crc >> 1u) ^ 0xEDB88320u;
            }
            else
            {
                crc >>= 1u;
            }
        }
    }

    return ~crc;
}

void FW_BootShared_EnableBkpsramAccess(void)
{
    uint32_t timeout;

    /* ---------------------------------------------------------------------- */
    /*  Backup domain 접근을 위한 PWR clock enable                             */
    /*  - 이 순서를 잘못 두면 PWR 레지스터 접근 시 예기치 않은 동작이 날 수 있다. */
    /* ---------------------------------------------------------------------- */
    __HAL_RCC_PWR_CLK_ENABLE();
    HAL_PWR_EnableBkUpAccess();

    /* ---------------------------------------------------------------------- */
    /*  Backup SRAM clock enable                                               */
    /* ---------------------------------------------------------------------- */
    __HAL_RCC_BKPSRAM_CLK_ENABLE();

    /* ---------------------------------------------------------------------- */
    /*  Backup regulator enable                                                */
    /*  - VBAT domain 유지용 regulator 를 켠 뒤 ready bit 를 잠시 기다린다.      */
    /* ---------------------------------------------------------------------- */
    HAL_PWREx_EnableBkUpReg();

    timeout = HAL_GetTick() + 100u;
    while (__HAL_PWR_GET_FLAG(PWR_FLAG_BRR) == RESET)
    {
        if ((HAL_GetTick() - timeout) > 100u)
        {
            break;
        }
    }

    __DSB();
    __ISB();
}

fw_bootctrl_t *FW_BootShared_GetCtrl(void)
{
    return (fw_bootctrl_t *)FW_BOOTCTRL_BKPSRAM_BASE;
}

void FW_BootShared_LoadOrInit(fw_bootctrl_t *out_ctrl)
{
    fw_bootctrl_t *stored;
    uint32_t expected_crc;

    if (out_ctrl == NULL)
    {
        return;
    }

    FW_BootShared_EnableBkpsramAccess();

    stored = FW_BootShared_GetCtrl();
    *out_ctrl = *stored;

    expected_crc = FW_BootShared_CalcStructCrc(out_ctrl);

    if ((out_ctrl->magic != FW_BOOTCTRL_MAGIC) ||
        (out_ctrl->version != FW_BOOTCTRL_VERSION) ||
        (out_ctrl->crc32 != expected_crc))
    {
        memset(out_ctrl, 0, sizeof(*out_ctrl));

        out_ctrl->magic = FW_BOOTCTRL_MAGIC;
        out_ctrl->version = FW_BOOTCTRL_VERSION;
        out_ctrl->installed_version_u32 = FW_BOOTCTRL_INSTALLED_VERSION_INIT;

        FW_BootShared_Store(out_ctrl);
    }
}

void FW_BootShared_Store(const fw_bootctrl_t *ctrl)
{
    fw_bootctrl_t temp;
    fw_bootctrl_t *stored;

    if (ctrl == NULL)
    {
        return;
    }

    FW_BootShared_EnableBkpsramAccess();

    temp = *ctrl;
    temp.magic = FW_BOOTCTRL_MAGIC;
    temp.version = FW_BOOTCTRL_VERSION;
    temp.crc32 = 0u;
    temp.crc32 = FW_BootShared_CalcStructCrc(&temp);

    stored = FW_BootShared_GetCtrl();
    *stored = temp;

    __DSB();
    __ISB();
}
