#include "FW_BootShared.h"

#include <string.h>

/* -------------------------------------------------------------------------- */
/*  내부 helper: BKPSRAM base 포인터                                           */
/* -------------------------------------------------------------------------- */
static volatile uint8_t *FW_BootShared_GetStorageBase(void)
{
    return (volatile uint8_t *)FW_BOOTCTRL_BKPSRAM_BASE;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: CRC32                                                            */
/*                                                                            */
/*  bootloader 와 같은 reversed polynomial 0xEDB88320 을 사용한다.            */
/* -------------------------------------------------------------------------- */
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

/* -------------------------------------------------------------------------- */
/*  내부 helper: struct CRC 계산                                               */
/*                                                                            */
/*  bootloader FW_BOOTCTL_CalcCrc() 와 같은 방식으로                           */
/*  crc32 필드 직전까지의 바이트만 계산한다.                                   */
/* -------------------------------------------------------------------------- */
static uint32_t FW_BootShared_CalcStructCrc(const fw_bootctrl_t *ctrl)
{
    if (ctrl == 0)
    {
        return 0u;
    }

    return FW_BootShared_CalcCrc32(ctrl, (uint32_t)offsetof(fw_bootctrl_t, crc32));
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: 기본값 초기화                                                 */
/* -------------------------------------------------------------------------- */
static void FW_BootShared_ResetToDefaults(fw_bootctrl_t *ctrl)
{
    if (ctrl == 0)
    {
        return;
    }

    memset(ctrl, 0, sizeof(*ctrl));
    ctrl->magic = FW_BOOTCTRL_MAGIC;
    ctrl->version = FW_BOOTCTRL_VERSION;
    ctrl->struct_size = (uint16_t)sizeof(*ctrl);
    ctrl->installed_version_u32 = FW_BOOTCTRL_INSTALLED_VERSION_INIT;
    ctrl->crc32 = FW_BootShared_CalcStructCrc(ctrl);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: backup domain / BKPSRAM 접근 enable                              */
/*                                                                            */
/*  APP 쪽에서도 bootloader 와 같은 순서로 backup SRAM 접근을 연다.           */
/* -------------------------------------------------------------------------- */
void FW_BootShared_EnableBkpsramAccess(void)
{
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR;

    PWR->CR |= PWR_CR_DBP;
    (void)PWR->CR;

#if defined(RCC_AHB1ENR_BKPSRAMEN)
    RCC->AHB1ENR |= RCC_AHB1ENR_BKPSRAMEN;
    (void)RCC->AHB1ENR;
#endif

#if defined(PWR_CSR_BRE)
    PWR->CSR |= PWR_CSR_BRE;
    (void)PWR->CSR;

    while ((PWR->CSR & PWR_CSR_BRR) == 0u)
    {
        /* backup regulator ready 대기 */
    }
#endif

    __DSB();
    __ISB();
}

/* -------------------------------------------------------------------------- */
/*  공개 API: BKPSRAM pointer                                                  */
/* -------------------------------------------------------------------------- */
fw_bootctrl_t *FW_BootShared_GetCtrl(void)
{
    return (fw_bootctrl_t *)FW_BOOTCTRL_BKPSRAM_BASE;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: load or init                                                     */
/* -------------------------------------------------------------------------- */
void FW_BootShared_LoadOrInit(fw_bootctrl_t *out_ctrl)
{
    const volatile uint8_t *src;
    uint32_t index;

    if (out_ctrl == 0)
    {
        return;
    }

    FW_BootShared_EnableBkpsramAccess();
    src = FW_BootShared_GetStorageBase();

    for (index = 0u; index < sizeof(*out_ctrl); index++)
    {
        ((uint8_t *)out_ctrl)[index] = src[index];
    }

    if (out_ctrl->magic != FW_BOOTCTRL_MAGIC)
    {
        FW_BootShared_ResetToDefaults(out_ctrl);
        FW_BootShared_Store(out_ctrl);
        return;
    }

    if (out_ctrl->version != FW_BOOTCTRL_VERSION)
    {
        FW_BootShared_ResetToDefaults(out_ctrl);
        FW_BootShared_Store(out_ctrl);
        return;
    }

    if (out_ctrl->struct_size != sizeof(*out_ctrl))
    {
        FW_BootShared_ResetToDefaults(out_ctrl);
        FW_BootShared_Store(out_ctrl);
        return;
    }

    if (FW_BootShared_CalcStructCrc(out_ctrl) != out_ctrl->crc32)
    {
        FW_BootShared_ResetToDefaults(out_ctrl);
        FW_BootShared_Store(out_ctrl);
        return;
    }
}

/* -------------------------------------------------------------------------- */
/*  공개 API: store                                                            */
/* -------------------------------------------------------------------------- */
void FW_BootShared_Store(const fw_bootctrl_t *ctrl)
{
    fw_bootctrl_t temp;
    volatile uint8_t *dst;
    uint32_t index;

    if (ctrl == 0)
    {
        return;
    }

    FW_BootShared_EnableBkpsramAccess();

    memcpy(&temp, ctrl, sizeof(temp));
    temp.magic = FW_BOOTCTRL_MAGIC;
    temp.version = FW_BOOTCTRL_VERSION;
    temp.struct_size = (uint16_t)sizeof(temp);
    temp.crc32 = FW_BootShared_CalcStructCrc(&temp);

    dst = FW_BootShared_GetStorageBase();
    for (index = 0u; index < sizeof(temp); index++)
    {
        dst[index] = ((const uint8_t *)&temp)[index];
    }

    __DSB();
    __ISB();
}
