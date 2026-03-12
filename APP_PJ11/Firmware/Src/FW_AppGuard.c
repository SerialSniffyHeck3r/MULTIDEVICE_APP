#include "FW_AppGuard.h"

#include "FW_BootCtl.h"
#include "FW_BuildVersion.h"
#include "FW_Package.h"
#include "iwdg.h"

#include <string.h>

/* -------------------------------------------------------------------------- */
/*  공개 API: 앱 부팅 시작 마킹                                                */
/* -------------------------------------------------------------------------- */
void FW_AppGuard_OnAppBootStart(void)
{
    fw_boot_control_t ctl;

    FW_BOOTCTL_InitStorage();
    (void)FW_BOOTCTL_Load(&ctl);

    ctl.app_boot_pending   = 1u;
    ctl.app_boot_confirmed = 0u;

    (void)FW_BOOTCTL_Store(&ctl);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 정상 부팅 확정                                                   */
/* -------------------------------------------------------------------------- */
void FW_AppGuard_ConfirmBootOk(void)
{
    fw_boot_control_t ctl;

    FW_BOOTCTL_InitStorage();
    (void)FW_BOOTCTL_Load(&ctl);

    ctl.app_boot_pending              = 0u;
    ctl.app_boot_confirmed            = 1u;
    ctl.iwdg_unconfirmed_reset_count  = 0u;
    ctl.installed_version_u32         = FW_Package_VersionStringToU32(FW_BUILD_VERSION_STRING);

    memset(ctl.installed_version_string, 0, sizeof(ctl.installed_version_string));
    strncpy(ctl.installed_version_string,
            FW_BUILD_VERSION_STRING,
            sizeof(ctl.installed_version_string) - 1u);

    (void)FW_BOOTCTL_Store(&ctl);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: watchdog refresh                                                 */
/* -------------------------------------------------------------------------- */
void FW_AppGuard_Kick(void)
{
    (void)HAL_IWDG_Refresh(&hiwdg);
}
