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
#if defined(PWR_CSR_BRR)
    uint32_t wait_count;
#endif

    /* ---------------------------------------------------------------------- */
    /*  PWR peripheral clock는 DBP / BRE 제어에 필요하다.                     */
    /*  따라서 backup domain 또는 BKPSRAM에 접근하기 전에 가장 먼저 켠다.      */
    /* ---------------------------------------------------------------------- */
    RCC->APB1ENR |= RCC_APB1ENR_PWREN;
    (void)RCC->APB1ENR;

    /* ---------------------------------------------------------------------- */
    /*  backup domain write 보호를 푼다.                                      */
    /*  DBP bit가 올라가야 backup 영역 제어 레지스터와 BKPSRAM 쪽 준비를       */
    /*  안정적으로 진행할 수 있다.                                             */
    /* ---------------------------------------------------------------------- */
    PWR->CR |= PWR_CR_DBP;
    (void)PWR->CR;

#if defined(RCC_AHB1ENR_BKPSRAMEN)
    /* ---------------------------------------------------------------------- */
    /*  BKPSRAM AHB clock를 켠다.                                              */
    /*  clock가 꺼져 있으면 뒤의 byte 단위 load / store 자체가 성립하지 않는다. */
    /* ---------------------------------------------------------------------- */
    RCC->AHB1ENR |= RCC_AHB1ENR_BKPSRAMEN;
    (void)RCC->AHB1ENR;
#endif

#if defined(PWR_CSR_BRE)
    /* ---------------------------------------------------------------------- */
    /*  backup regulator를 켠다.                                               */
    /*  VBAT retention이 필요한 경우에도 이 regulator가 켜져 있어야 한다.      */
    /* ---------------------------------------------------------------------- */
    PWR->CSR |= PWR_CSR_BRE;
    (void)PWR->CSR;
#endif

#if defined(PWR_CSR_BRR)
    /* ---------------------------------------------------------------------- */
    /*  기존 구현은 BRR ready bit를 timeout 없이 무한 대기했다.                */
    /*  하지만 bring-up 단계에서 backup domain 상태가 꼬이거나 regulator가     */
    /*  ready 되지 못하면, boot / app이 fault recovery 이전에 그대로           */
    /*  영구 정지할 수 있다.                                                    */
    /*                                                                        */
    /*  여기서는 APP fault logger와 같은 정책으로 bounded spin을 사용한다.     */
    /*  최악의 경우 retention 품질이 떨어질 수는 있어도,                       */
    /*  시스템 전체가 부팅 중 영원히 멈추는 상황은 피하는 것이 더 중요하다.    */
    /* ---------------------------------------------------------------------- */
    wait_count = 100000u;
    while (((PWR->CSR & PWR_CSR_BRR) == 0u) && (wait_count > 0u))
    {
        wait_count--;
    }
#endif

    /* ---------------------------------------------------------------------- */
    /*  앞선 clock / regulator write가 실제 하드웨어에 반영된 뒤               */
    /*  다음 BKPSRAM 접근이 이어지도록 barrier를 건다.                         */
    /* ---------------------------------------------------------------------- */
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
