#include "FW_AppGuard.h"

#include "FW_BootShared.h"
#include "iwdg.h"

#include <string.h>

/* -------------------------------------------------------------------------- */
/*  선택 기능: build version header가 있으면 installed version 동기화          */
/*                                                                            */
/*  이 기능이 있으면                                                           */
/*  - app를 ST-LINK / CubeProgrammer로 직접 올린 경우에도                     */
/*  - 첫 정상 부팅이 끝나는 시점에 BKPSRAM의 installed_version 이 갱신되어   */
/*    같은 SYSUPDATE.bin 이 SD에 꽂혀 있어도 매번 auto-entry 하지 않게 된다. */
/* -------------------------------------------------------------------------- */
#if defined(__has_include)
#  if __has_include("FW_BuildVersion.h")
#    include "FW_BuildVersion.h"
#    define FW_APPGUARD_HAS_BUILD_VERSION 1
#  else
#    define FW_APPGUARD_HAS_BUILD_VERSION 0
#  endif
#else
#  define FW_APPGUARD_HAS_BUILD_VERSION 0
#endif

/* -------------------------------------------------------------------------- */
/*  공개 API: app boot start                                                  */
/*                                                                            */
/*  bootloader 가 jump 직전에 pending=1 을 세우더라도,                         */
/*  app 쪽에서도 한 번 더 동일한 struct 형식으로 pending 상태를 덮어써서      */
/*  공유 블록이 항상 일관된 상태를 갖게 만든다.                               */
/* -------------------------------------------------------------------------- */
void FW_AppGuard_OnAppBootStart(void)
{
    fw_bootctrl_t ctrl;

    FW_BootShared_LoadOrInit(&ctrl);

    ctrl.app_boot_pending = 1u;
    ctrl.app_boot_confirmed = 0u;

    /* ---------------------------------------------------------------------- */
    /*  reset flag snapshot 은 bootloader 가 이미 읽고 소유하므로             */
    /*  app 쪽에서는 건드리지 않는다.                                          */
    /* ---------------------------------------------------------------------- */
    FW_BootShared_Store(&ctrl);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: boot confirmed                                                  */
/*                                                                            */
/*  정상 부팅 확정 시                                                           */
/*  - pending 을 내리고                                                        */
/*  - confirmed 를 세우고                                                      */
/*  - 초기 부팅 전 IWDG fail 누적 카운터를 리셋한다.                          */
/*  - build version header가 존재하면 installed version 도 함께 동기화한다.  */
/* -------------------------------------------------------------------------- */
void FW_AppGuard_ConfirmBootOk(void)
{
    fw_bootctrl_t ctrl;

    FW_BootShared_LoadOrInit(&ctrl);

    ctrl.app_boot_pending = 0u;
    ctrl.app_boot_confirmed = 1u;
    ctrl.iwdg_unconfirmed_reset_count = 0u;

#if FW_APPGUARD_HAS_BUILD_VERSION
    ctrl.installed_version_u32 = FW_BUILD_VERSION_U32;
    memset(ctrl.installed_version_string, 0, sizeof(ctrl.installed_version_string));
    strncpy(ctrl.installed_version_string,
            FW_BUILD_VERSION_STRING,
            sizeof(ctrl.installed_version_string) - 1u);
#endif

    FW_BootShared_Store(&ctrl);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: main loop watchdog refresh                                      */
/* -------------------------------------------------------------------------- */
void FW_AppGuard_Kick(void)
{
    if (hiwdg.Instance != 0u)
    {
        (void)HAL_IWDG_Refresh(&hiwdg);
    }
}
