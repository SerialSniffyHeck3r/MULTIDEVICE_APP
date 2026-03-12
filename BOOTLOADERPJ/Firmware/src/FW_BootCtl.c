#include "FW_BootCtl.h"

#include <stddef.h>
#include <string.h>

#include "FW_Crc32.h"

/* -------------------------------------------------------------------------- */
/*  BKPSRAM base helper                                                        */
/* -------------------------------------------------------------------------- */
static volatile uint8_t *FW_BOOTCTL_GetStorageBase(void)
{
    return (volatile uint8_t *)BKPSRAM_BASE;
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: backup domain / BKPSRAM 접근 enable                           */
/* -------------------------------------------------------------------------- */
static void FW_BOOTCTL_EnableBkpsramAccess(void)
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
/*  내부 helper: boot control CRC 계산                                         */
/* -------------------------------------------------------------------------- */
static uint32_t FW_BOOTCTL_CalcCrc(const fw_boot_control_t *ctl)
{
    if (ctl == 0)
    {
        return 0u;
    }

    return FW_CRC32_Calc(ctl, (uint32_t)offsetof(fw_boot_control_t, crc32));
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 저장소 enable                                                    */
/* -------------------------------------------------------------------------- */
void FW_BOOTCTL_InitStorage(void)
{
    FW_BOOTCTL_EnableBkpsramAccess();
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 기본값 초기화                                                    */
/* -------------------------------------------------------------------------- */
void FW_BOOTCTL_ResetToDefaults(fw_boot_control_t *ctl)
{
    if (ctl == 0)
    {
        return;
    }

    memset(ctl, 0, sizeof(*ctl));

    ctl->magic       = FW_BOOTCTL_MAGIC;
    ctl->version     = FW_BOOTCTL_VERSION;
    ctl->struct_size = (uint16_t)sizeof(*ctl);
    ctl->crc32       = FW_BOOTCTL_CalcCrc(ctl);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: load                                                             */
/* -------------------------------------------------------------------------- */
bool FW_BOOTCTL_Load(fw_boot_control_t *out_ctl)
{
    const volatile uint8_t *src;
    uint32_t index;

    if (out_ctl == 0)
    {
        return false;
    }

    FW_BOOTCTL_EnableBkpsramAccess();

    src = FW_BOOTCTL_GetStorageBase();
    for (index = 0u; index < sizeof(*out_ctl); index++)
    {
        ((uint8_t *)out_ctl)[index] = src[index];
    }

    if (out_ctl->magic != FW_BOOTCTL_MAGIC)
    {
        FW_BOOTCTL_ResetToDefaults(out_ctl);
        return false;
    }

    if (out_ctl->version != FW_BOOTCTL_VERSION)
    {
        FW_BOOTCTL_ResetToDefaults(out_ctl);
        return false;
    }

    if (out_ctl->struct_size != sizeof(*out_ctl))
    {
        FW_BOOTCTL_ResetToDefaults(out_ctl);
        return false;
    }

    if (FW_BOOTCTL_CalcCrc(out_ctl) != out_ctl->crc32)
    {
        FW_BOOTCTL_ResetToDefaults(out_ctl);
        return false;
    }

    return true;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: store                                                            */
/* -------------------------------------------------------------------------- */
HAL_StatusTypeDef FW_BOOTCTL_Store(const fw_boot_control_t *ctl)
{
    fw_boot_control_t temp;
    volatile uint8_t *dst;
    uint32_t index;

    if (ctl == 0)
    {
        return HAL_ERROR;
    }

    memcpy(&temp, ctl, sizeof(temp));
    temp.magic       = FW_BOOTCTL_MAGIC;
    temp.version     = FW_BOOTCTL_VERSION;
    temp.struct_size = (uint16_t)sizeof(temp);
    temp.crc32       = FW_BOOTCTL_CalcCrc(&temp);

    FW_BOOTCTL_EnableBkpsramAccess();

    dst = FW_BOOTCTL_GetStorageBase();
    for (index = 0u; index < sizeof(temp); index++)
    {
        dst[index] = ((const uint8_t *)&temp)[index];
    }

    __DSB();
    __ISB();

    return HAL_OK;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: reset flags read                                                 */
/* -------------------------------------------------------------------------- */
uint32_t FW_BOOTCTL_ReadResetFlags(void)
{
    return RCC->CSR;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: reset flags clear                                                */
/* -------------------------------------------------------------------------- */
void FW_BOOTCTL_ClearResetFlags(void)
{
#if defined(RCC_CSR_RMVF)
    RCC->CSR |= RCC_CSR_RMVF;
    __DSB();
    __ISB();
#endif
}
