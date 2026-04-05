#include "FW_Mode.h"

#include "APP_FAULT.h"
#include "DEBUG_UART.h"
#include "FW_BootCtl.h"
#include "FW_Flash.h"
#include "FW_Input.h"
#include "FW_SD.h"
#include "FW_UI.h"

#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*  내부 런타임 컨텍스트                                                       */
/* -------------------------------------------------------------------------- */
typedef struct
{
    fw_boot_reason_t    entry_reason;
    fw_boot_control_t   boot_ctl;
    fw_sd_scan_result_t scan_result;
    fw_ui_view_t        ui_view;
    uint32_t            last_rescan_ms;
    uint8_t             app_valid;
} fw_mode_runtime_t;

static fw_mode_runtime_t s_fw_mode_rt;

/* -------------------------------------------------------------------------- */
/*  내부 helper: scan 결과 -> package state                                   */
/* -------------------------------------------------------------------------- */
static fw_ui_package_state_t FW_MODE_GetPackageStateFromScan(void)
{
    if (s_fw_mode_rt.scan_result.card_present == false)
    {
        return FW_UI_PACKAGE_STATE_NO_CARD;
    }

    if (s_fw_mode_rt.scan_result.file_found == false)
    {
        return FW_UI_PACKAGE_STATE_NO_FILE;
    }

    if (s_fw_mode_rt.scan_result.header_valid == false)
    {
        return FW_UI_PACKAGE_STATE_BAD_FILE;
    }

    return FW_UI_PACKAGE_STATE_READY;
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: view.version_string 갱신                                     */
/* -------------------------------------------------------------------------- */
static void FW_MODE_CopyVersionStringFromScan(void)
{
    memset(s_fw_mode_rt.ui_view.version_string,
           0,
           sizeof(s_fw_mode_rt.ui_view.version_string));

    if (s_fw_mode_rt.scan_result.header_valid != false)
    {
        strncpy(s_fw_mode_rt.ui_view.version_string,
                s_fw_mode_rt.scan_result.header.version_string,
                sizeof(s_fw_mode_rt.ui_view.version_string) - 1u);
    }
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: UI 상태 재구성                                               */
/* -------------------------------------------------------------------------- */
static void FW_MODE_RebuildView(void)
{
    memset(&s_fw_mode_rt.ui_view, 0, sizeof(s_fw_mode_rt.ui_view));

    s_fw_mode_rt.ui_view.reason             = s_fw_mode_rt.entry_reason;
    s_fw_mode_rt.ui_view.progress_percent   = 0u;
    s_fw_mode_rt.ui_view.flash_stage        = FW_FLASH_STAGE_IDLE;
    s_fw_mode_rt.ui_view.package_state      = FW_MODE_GetPackageStateFromScan();
    s_fw_mode_rt.ui_view.package_ready      = (s_fw_mode_rt.ui_view.package_state == FW_UI_PACKAGE_STATE_READY);
    s_fw_mode_rt.ui_view.app_jump_available = (s_fw_mode_rt.app_valid != 0u) ? true : false;

    FW_MODE_CopyVersionStringFromScan();
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: package compare -> entry reason                              */
/* -------------------------------------------------------------------------- */
static fw_boot_reason_t FW_MODE_ReasonFromPackageVersion(void)
{
    fw_package_compare_t compare_result;

    compare_result = FW_Package_CompareVersions(s_fw_mode_rt.boot_ctl.installed_version_u32,
                                                s_fw_mode_rt.scan_result.header.version_u32);

    if (compare_result == FW_PACKAGE_COMPARE_OLDER)
    {
        return FW_BOOT_REASON_UPDATE_OLD;
    }

    return FW_BOOT_REASON_UPDATE_NEW;
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: app jump 직전 boot_ctl 마킹                                  */
/* -------------------------------------------------------------------------- */
static void FW_MODE_PrepareAndJumpToApp(void)
{
    s_fw_mode_rt.boot_ctl.app_boot_pending   = 1u;
    s_fw_mode_rt.boot_ctl.app_boot_confirmed = 0u;
    s_fw_mode_rt.boot_ctl.requested_mode     = FW_BOOT_REQUEST_NONE;
    s_fw_mode_rt.boot_ctl.requested_reason   = FW_BOOT_REASON_NONE;
    (void)FW_BOOTCTL_Store(&s_fw_mode_rt.boot_ctl);

    FW_FLASH_JumpToApp();
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: SD rescan + view rebuild                                     */
/* -------------------------------------------------------------------------- */
static void FW_MODE_RefreshScanAndView(void)
{
    FW_SD_ScanUpdatePackage(&s_fw_mode_rt.scan_result);
    FW_MODE_RebuildView();
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: progress callback -> UI percent                              */
/* -------------------------------------------------------------------------- */
static void FW_MODE_OnProgress(fw_flash_stage_t stage,
                               uint32_t completed_units,
                               uint32_t total_units,
                               void *user_context)
{
    uint32_t percent;
    (void)user_context;

    percent = 0u;

    if (total_units != 0u)
    {
        percent = (completed_units * 100u) / total_units;
    }

    switch (stage)
    {
    case FW_FLASH_STAGE_W25Q_ERASE:
        s_fw_mode_rt.ui_view.progress_percent = (uint8_t)((percent * 25u) / 100u);
        break;

    case FW_FLASH_STAGE_APP_ERASE:
        if (s_fw_mode_rt.entry_reason == FW_BOOT_REASON_UPDATE_OLD)
        {
            s_fw_mode_rt.ui_view.progress_percent = (uint8_t)(25u + ((percent * 15u) / 100u));
        }
        else
        {
            s_fw_mode_rt.ui_view.progress_percent = (uint8_t)((percent * 20u) / 100u);
        }
        break;

    case FW_FLASH_STAGE_APP_PROGRAM:
        if (s_fw_mode_rt.entry_reason == FW_BOOT_REASON_UPDATE_OLD)
        {
            s_fw_mode_rt.ui_view.progress_percent = (uint8_t)(40u + ((percent * 50u) / 100u));
        }
        else
        {
            s_fw_mode_rt.ui_view.progress_percent = (uint8_t)(20u + ((percent * 70u) / 100u));
        }
        break;

    case FW_FLASH_STAGE_APP_VERIFY:
        s_fw_mode_rt.ui_view.progress_percent = 100u;
        break;

    case FW_FLASH_STAGE_IDLE:
    default:
        break;
    }

    s_fw_mode_rt.ui_view.flash_stage = stage;
    FW_UI_Draw(&s_fw_mode_rt.ui_view);
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: install 실행                                                 */
/* -------------------------------------------------------------------------- */
static void FW_MODE_InstallSelectedPackage(void)
{
    FIL fp;
    fw_package_header_t header;
    fw_package_result_t package_result;

    if (s_fw_mode_rt.scan_result.header_valid == false)
    {
        FW_MODE_RebuildView();
        FW_UI_Draw(&s_fw_mode_rt.ui_view);
        return;
    }

    memset(&fp, 0, sizeof(fp));
    memset(&header, 0, sizeof(header));

    package_result = FW_SD_OpenUpdatePackage(&fp, &header);
    if (package_result != FW_PACKAGE_RESULT_OK)
    {
        FW_MODE_RefreshScanAndView();
        FW_UI_Draw(&s_fw_mode_rt.ui_view);
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  구버전 설치는 user space factory reset 정책을 따른다.                  */
    /*  즉, W25Q16 전체를 4KB sector erase로 비운다.                           */
    /* ---------------------------------------------------------------------- */
    if (s_fw_mode_rt.entry_reason == FW_BOOT_REASON_UPDATE_OLD)
    {
        if (FW_FLASH_EraseW25Q16All(FW_MODE_OnProgress, 0) != HAL_OK)
        {
            (void)f_close(&fp);
            FW_MODE_RefreshScanAndView();
            FW_UI_Draw(&s_fw_mode_rt.ui_view);
            return;
        }
    }

    if (FW_FLASH_InstallFromOpenFile(&fp, &header, FW_MODE_OnProgress, 0) != HAL_OK)
    {
        (void)f_close(&fp);
        s_fw_mode_rt.app_valid = 0u;
        s_fw_mode_rt.entry_reason = FW_BOOT_REASON_INVALID_APP;
        FW_MODE_RefreshScanAndView();
        s_fw_mode_rt.ui_view.reason = s_fw_mode_rt.entry_reason;
        FW_UI_Draw(&s_fw_mode_rt.ui_view);
        return;
    }

    (void)f_close(&fp);

    /* ---------------------------------------------------------------------- */
    /*  install 직후 벡터가 실제로 유효한지 다시 한 번 확인한다.              */
    /*  FW_FLASH_InstallFromOpenFile() 내부에서도 검사하지만,                 */
    /*  mode 쪽 상태 변수도 바로 일치시키기 위해 여기서 다시 반영한다.       */
    /* ---------------------------------------------------------------------- */
    s_fw_mode_rt.app_valid = (FW_FLASH_IsAppVectorValid() != false) ? 1u : 0u;
    if (s_fw_mode_rt.app_valid == 0u)
    {
        s_fw_mode_rt.entry_reason = FW_BOOT_REASON_INVALID_APP;
        FW_MODE_RefreshScanAndView();
        s_fw_mode_rt.ui_view.reason = s_fw_mode_rt.entry_reason;
        FW_UI_Draw(&s_fw_mode_rt.ui_view);
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  install 성공 시점에 "현재 설치된 앱 버전" 을 BKPSRAM에 즉시 반영한다.*/
    /*  이렇게 해야 SYSUPDATE.bin 이 SD에 남아 있어도                         */
    /*  다음 부팅에서 같은 버전이면 auto-entry 하지 않게 만들 수 있다.        */
    /* ---------------------------------------------------------------------- */
    s_fw_mode_rt.boot_ctl.installed_version_u32 = header.version_u32;
    memset(s_fw_mode_rt.boot_ctl.installed_version_string,
           0,
           sizeof(s_fw_mode_rt.boot_ctl.installed_version_string));
    strncpy(s_fw_mode_rt.boot_ctl.installed_version_string,
            header.version_string,
            sizeof(s_fw_mode_rt.boot_ctl.installed_version_string) - 1u);

    s_fw_mode_rt.boot_ctl.last_seen_package_version_u32 = header.version_u32;
    memset(s_fw_mode_rt.boot_ctl.last_seen_package_version_string,
           0,
           sizeof(s_fw_mode_rt.boot_ctl.last_seen_package_version_string));
    strncpy(s_fw_mode_rt.boot_ctl.last_seen_package_version_string,
            header.version_string,
            sizeof(s_fw_mode_rt.boot_ctl.last_seen_package_version_string) - 1u);

    s_fw_mode_rt.boot_ctl.app_boot_pending             = 0u;
    s_fw_mode_rt.boot_ctl.app_boot_confirmed           = 0u;
    s_fw_mode_rt.boot_ctl.iwdg_unconfirmed_reset_count = 0u;
    s_fw_mode_rt.boot_ctl.requested_mode               = FW_BOOT_REQUEST_NONE;
    s_fw_mode_rt.boot_ctl.requested_reason             = FW_BOOT_REASON_NONE;
    (void)FW_BOOTCTL_Store(&s_fw_mode_rt.boot_ctl);

    NVIC_SystemReset();
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 진입 후 UI loop                                                 */
/* -------------------------------------------------------------------------- */
void EnterFWFlashTool(void)
{
    fw_input_event_t event;
    uint32_t now_ms;

    FW_MODE_RebuildView();
    FW_UI_Draw(&s_fw_mode_rt.ui_view);

    for (;;)
    {
        now_ms = HAL_GetTick();
        FW_INPUT_Task(now_ms);

        /* ------------------------------------------------------------------ */
        /*  UI loop에서도 SD를 주기적으로 다시 scan한다.                       */
        /*  처음엔 카드가 없어도, 사용자가 나중에 꽂으면 설치 가능하게 하려는   */
        /*  목적이다.                                                          */
        /* ------------------------------------------------------------------ */
        if ((uint32_t)(now_ms - s_fw_mode_rt.last_rescan_ms) >= FW_BOOT_AUTO_RESCAN_MS)
        {
            s_fw_mode_rt.last_rescan_ms = now_ms;
            FW_MODE_RefreshScanAndView();
            FW_UI_Draw(&s_fw_mode_rt.ui_view);
        }

        while (FW_INPUT_PopEvent(&event) != false)
        {
            if ((event.button_id == FW_INPUT_BUTTON_1) &&
                (event.event_type == FW_INPUT_EVENT_SHORT_PRESS))
            {
                if (s_fw_mode_rt.app_valid != 0u)
                {
                    FW_MODE_PrepareAndJumpToApp();
                }

                NVIC_SystemReset();
            }

            if ((event.button_id == FW_INPUT_BUTTON_6) &&
                (event.event_type == FW_INPUT_EVENT_LONG_PRESS))
            {
                FW_MODE_InstallSelectedPackage();
                FW_UI_Draw(&s_fw_mode_rt.ui_view);
            }
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  공개 API: reset 직후 자동 판단                                            */
/* -------------------------------------------------------------------------- */
void FW_MODE_RunFromReset(void)
{
    app_fault_log_t fault_log;
    uint32_t reset_flags;
    uint8_t user_entry;
    uint8_t fault_present;

    memset(&s_fw_mode_rt, 0, sizeof(s_fw_mode_rt));

    FW_BOOTCTL_InitStorage();
    (void)FW_BOOTCTL_Load(&s_fw_mode_rt.boot_ctl);

    reset_flags = FW_BOOTCTL_ReadResetFlags();
    FW_BOOTCTL_ClearResetFlags();
    s_fw_mode_rt.boot_ctl.last_reset_flags = reset_flags;

    /* ---------------------------------------------------------------------- */
    /*  IWDG reset + boot confirm 미완료 조합이면 fail streak 증가            */
    /* ---------------------------------------------------------------------- */
    if (((reset_flags & RCC_CSR_IWDGRSTF) != 0u) &&
        (s_fw_mode_rt.boot_ctl.app_boot_pending != 0u) &&
        (s_fw_mode_rt.boot_ctl.app_boot_confirmed == 0u))
    {
        s_fw_mode_rt.boot_ctl.iwdg_unconfirmed_reset_count++;
    }
    else if ((reset_flags & (RCC_CSR_PORRSTF | RCC_CSR_BORRSTF | RCC_CSR_PINRSTF)) != 0u)
    {
        /* ------------------------------------------------------------------ */
        /*  전원 인가 / brown-out / 외부 reset에서는 fail streak를 리셋한다.   */
        /* ------------------------------------------------------------------ */
        s_fw_mode_rt.boot_ctl.iwdg_unconfirmed_reset_count = 0u;
    }

    s_fw_mode_rt.boot_ctl.app_boot_pending   = 0u;
    s_fw_mode_rt.boot_ctl.app_boot_confirmed = 0u;
    (void)FW_BOOTCTL_Store(&s_fw_mode_rt.boot_ctl);

    memset(&fault_log, 0, sizeof(fault_log));
    fault_present = (APP_FAULT_ReadPersistentLog(&fault_log) != false) ? 1u : 0u;
    if (fault_present != 0u)
    {
        APP_FAULT_ClearPersistentLog();
    }

    HAL_Delay(40u);
    user_entry = (FW_INPUT_IsStartupChordKey4Key6Pressed() != false) ? 1u : 0u;

    FW_SD_ScanUpdatePackage(&s_fw_mode_rt.scan_result);
    s_fw_mode_rt.app_valid = (FW_FLASH_IsAppVectorValid() != false) ? 1u : 0u;

    /* ---------------------------------------------------------------------- */
    /*  진입 우선순위                                                          */
    /*  1) 사용자 chord                                                       */
    /*  2) SD package 자동 업데이트 (단, 설치된 버전과 다를 때만)            */
    /*  3) app invalid / fault / IWDG fail threshold                          */
    /*  4) 그 외에는 app jump                                                 */
    /* ---------------------------------------------------------------------- */
    if (user_entry != 0u)
    {
        s_fw_mode_rt.entry_reason = FW_BOOT_REASON_USER;
    }
    else if ((s_fw_mode_rt.scan_result.header_valid != false) &&
             (s_fw_mode_rt.scan_result.header.version_u32 != s_fw_mode_rt.boot_ctl.installed_version_u32))
    {
        s_fw_mode_rt.entry_reason = FW_MODE_ReasonFromPackageVersion();
    }
    else if ((s_fw_mode_rt.app_valid == 0u) ||
             (fault_present != 0u) ||
             (s_fw_mode_rt.boot_ctl.iwdg_unconfirmed_reset_count >= FW_BOOT_AUTOFAIL_THRESHOLD))
    {
        s_fw_mode_rt.entry_reason = (s_fw_mode_rt.app_valid == 0u) ? FW_BOOT_REASON_INVALID_APP : FW_BOOT_REASON_BOOT_FAIL;
    }
    else
    {
        FW_MODE_PrepareAndJumpToApp();
    }

    s_fw_mode_rt.boot_ctl.requested_mode   = FW_BOOT_REQUEST_FW_FLASH_MODE;
    s_fw_mode_rt.boot_ctl.requested_reason = s_fw_mode_rt.entry_reason;
    (void)FW_BOOTCTL_Store(&s_fw_mode_rt.boot_ctl);

    EnterFWFlashTool();
}
