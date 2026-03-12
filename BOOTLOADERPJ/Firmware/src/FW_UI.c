#include "FW_UI.h"

#include "u8g2.h"
#include "u8g2_uc1608_stm32.h"
#include "FW_BootConfig.h"

#include <stdio.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*  내부 helper: 가운데 정렬 문자열                                            */
/* -------------------------------------------------------------------------- */
static void FW_UI_DrawCenteredStr(u8g2_t *u8g2,
                                  uint8_t y,
                                  const char *text)
{
    uint16_t width;
    uint16_t x;

    if ((u8g2 == 0) || (text == 0))
    {
        return;
    }

    width = u8g2_GetStrWidth(u8g2, text);
    if (width >= 240u)
    {
        x = 0u;
    }
    else
    {
        x = (uint16_t)((240u - width) / 2u);
    }

    u8g2_DrawStr(u8g2, x, y, text);
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: stage text                                                    */
/* -------------------------------------------------------------------------- */
static const char *FW_UI_GetStageText(fw_flash_stage_t stage)
{
    switch (stage)
    {
    case FW_FLASH_STAGE_W25Q_ERASE:  return "ERASING USER DATA";
    case FW_FLASH_STAGE_APP_ERASE:   return "ERASING APP FLASH";
    case FW_FLASH_STAGE_APP_PROGRAM: return "PROGRAMMING APP";
    case FW_FLASH_STAGE_APP_VERIFY:  return "VERIFYING IMAGE";
    case FW_FLASH_STAGE_IDLE:
    default:
        return "WAITING";
    }
}

/* -------------------------------------------------------------------------- */
/*  공개 API: init                                                             */
/* -------------------------------------------------------------------------- */
void FW_UI_Init(void)
{
    U8G2_UC1608_Init();
    U8G2_UC1608_EnableSmartUpdate(0u);
    U8G2_UC1608_EnableFrameLimit(0u);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 전체 화면 draw                                                   */
/* -------------------------------------------------------------------------- */
void FW_UI_Draw(const fw_ui_view_t *view)
{
    u8g2_t *u8g2;
    uint16_t fill_width;
    char line[32];

    if (view == 0)
    {
        return;
    }

    u8g2 = U8G2_UC1608_GetHandle();
    if (u8g2 == 0)
    {
        return;
    }

    u8g2_ClearBuffer(u8g2);

    /* ---------------------------------------------------------------------- */
    /*  타이틀                                                                 */
    /* ---------------------------------------------------------------------- */
    u8g2_SetFont(u8g2, u8g2_font_6x13B_tr);
    FW_UI_DrawCenteredStr(u8g2, 12u, "== !!FW FLASH MODE!! ==");

    u8g2_SetFont(u8g2, u8g2_font_5x8_tr);
    FW_UI_DrawCenteredStr(u8g2, 24u, "Firmware Flash Mode");

    /* ---------------------------------------------------------------------- */
    /*  reason 별 안내 문구                                                     */
    /* ---------------------------------------------------------------------- */
    switch (view->reason)
    {
    case FW_BOOT_REASON_USER:
        FW_UI_DrawCenteredStr(u8g2, 42u, "Long Press PWR to reset");
        break;

    case FW_BOOT_REASON_BOOT_FAIL:
    case FW_BOOT_REASON_INVALID_APP:
        FW_UI_DrawCenteredStr(u8g2, 42u, "SYSTEM UNABLE TO BOOT :(");
        FW_UI_DrawCenteredStr(u8g2, 54u, "Don't Worry, Reflash F/W Here!");
        break;

    case FW_BOOT_REASON_UPDATE_NEW:
        FW_UI_DrawCenteredStr(u8g2, 42u, "New Version of Firmware Detected");
        (void)snprintf(line, sizeof(line), "VER %s", view->version_string);
        FW_UI_DrawCenteredStr(u8g2, 54u, line);
        break;

    case FW_BOOT_REASON_UPDATE_OLD:
        FW_UI_DrawCenteredStr(u8g2, 42u, "Installing old version will factory reset");
        (void)snprintf(line, sizeof(line), "VER %s", view->version_string);
        FW_UI_DrawCenteredStr(u8g2, 54u, line);
        break;

    case FW_BOOT_REASON_NONE:
    default:
        FW_UI_DrawCenteredStr(u8g2, 42u, "Waiting for valid SYSUPDATE.bin");
        break;
    }

    /* ---------------------------------------------------------------------- */
    /*  action 안내                                                             */
    /* ---------------------------------------------------------------------- */
    FW_UI_DrawCenteredStr(u8g2, 72u, "F1 TO ABORT");
    FW_UI_DrawCenteredStr(u8g2, 82u, "LONG PRESS F6 TO INSTALL");

    /* ---------------------------------------------------------------------- */
    /*  stage text                                                              */
    /* ---------------------------------------------------------------------- */
    FW_UI_DrawCenteredStr(u8g2, 96u, FW_UI_GetStageText(view->flash_stage));

    /* ---------------------------------------------------------------------- */
    /*  progress bar                                                            */
    /* ---------------------------------------------------------------------- */
    u8g2_DrawFrame(u8g2,
                   FW_PROGRESS_BAR_X,
                   FW_PROGRESS_BAR_Y,
                   FW_PROGRESS_BAR_W,
                   FW_PROGRESS_BAR_H);

    fill_width = (uint16_t)(((uint32_t)(FW_PROGRESS_BAR_W - 2u) * view->progress_percent) / 100u);
    if (fill_width > 0u)
    {
        u8g2_DrawBox(u8g2,
                     FW_PROGRESS_BAR_X + 1u,
                     FW_PROGRESS_BAR_Y + 1u,
                     fill_width,
                     FW_PROGRESS_BAR_H - 2u);
    }

    (void)snprintf(line, sizeof(line), "%3u%%", (unsigned int)view->progress_percent);
    u8g2_SetDrawColor(u8g2, 1u);
    FW_UI_DrawCenteredStr(u8g2, 124u, line);

    U8G2_UC1608_CommitBuffer();
}
