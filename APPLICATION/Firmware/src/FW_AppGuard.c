#include "FW_AppGuard.h"
#include "FW_BootShared.h"
#include "iwdg.h"

static void FW_AppGuard_LoadResetFlags(fw_bootctrl_t *ctrl)
{
    uint32_t flags;

    if (ctrl == NULL)
    {
        return;
    }

    flags = 0u;

    if (__HAL_RCC_GET_FLAG(RCC_FLAG_IWDGRST) != RESET)
    {
        flags |= (1u << 0);
    }

    if (__HAL_RCC_GET_FLAG(RCC_FLAG_SFTRST) != RESET)
    {
        flags |= (1u << 1);
    }

    if (__HAL_RCC_GET_FLAG(RCC_FLAG_PORRST) != RESET)
    {
        flags |= (1u << 2);
    }

    if (__HAL_RCC_GET_FLAG(RCC_FLAG_PINRST) != RESET)
    {
        flags |= (1u << 3);
    }

    ctrl->last_reset_flags = flags;
    __HAL_RCC_CLEAR_RESET_FLAGS();
}

void FW_AppGuard_OnAppBootStart(void)
{
    fw_bootctrl_t ctrl;

    FW_BootShared_LoadOrInit(&ctrl);

    /* ---------------------------------------------------------------------- */
    /*  이번 앱 부팅 세션 시작 표시                                            */
    /* ---------------------------------------------------------------------- */
    ctrl.app_boot_pending = 1u;
    ctrl.app_boot_confirmed = 0u;

    /* ---------------------------------------------------------------------- */
    /*  reset cause 스냅샷 갱신                                                */
    /* ---------------------------------------------------------------------- */
    FW_AppGuard_LoadResetFlags(&ctrl);

    FW_BootShared_Store(&ctrl);
}

void FW_AppGuard_ConfirmBootOk(void)
{
    fw_bootctrl_t ctrl;

    FW_BootShared_LoadOrInit(&ctrl);

    /* ---------------------------------------------------------------------- */
    /*  정상 부팅 확정                                                         */
    /*  - pending 을 내리고                                                    */
    /*  - confirmed 를 올리고                                                  */
    /*  - '정상 진입 전 IWDG 리셋' 누적 카운터를 초기화한다.                    */
    /* ---------------------------------------------------------------------- */
    ctrl.app_boot_pending = 0u;
    ctrl.app_boot_confirmed = 1u;
    ctrl.iwdg_unconfirmed_reset_count = 0u;

    FW_BootShared_Store(&ctrl);
}

void FW_AppGuard_Kick(void)
{
    /* ---------------------------------------------------------------------- */
    /*  main app watchdog refresh                                              */
    /*  - CubeMX 가 생성한 hiwdg 핸들을 그대로 사용한다.                       */
    /* ---------------------------------------------------------------------- */
    (void)HAL_IWDG_Refresh(&hiwdg);
}
