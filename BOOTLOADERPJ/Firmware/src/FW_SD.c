#include "FW_SD.h"

#include "bsp_driver_sd.h"

#include <string.h>

/* -------------------------------------------------------------------------- */
/*  외부 handle                                                                */
/* -------------------------------------------------------------------------- */
extern SD_HandleTypeDef hsd;
extern FATFS SDFatFS;
extern char SDPath[4];

/* -------------------------------------------------------------------------- */
/*  GPIO helper                                                                */
/* -------------------------------------------------------------------------- */
static void FW_SD_EnablePortClock(GPIO_TypeDef *port)
{
    if (port == GPIOA) { __HAL_RCC_GPIOA_CLK_ENABLE(); }
    else if (port == GPIOB) { __HAL_RCC_GPIOB_CLK_ENABLE(); }
    else if (port == GPIOC) { __HAL_RCC_GPIOC_CLK_ENABLE(); }
    else if (port == GPIOD) { __HAL_RCC_GPIOD_CLK_ENABLE(); }
    else if (port == GPIOE) { __HAL_RCC_GPIOE_CLK_ENABLE(); }
#if defined(GPIOF)
    else if (port == GPIOF) { __HAL_RCC_GPIOF_CLK_ENABLE(); }
#endif
#if defined(GPIOG)
    else if (port == GPIOG) { __HAL_RCC_GPIOG_CLK_ENABLE(); }
#endif
#if defined(GPIOH)
    else if (port == GPIOH) { __HAL_RCC_GPIOH_CLK_ENABLE(); }
#endif
#if defined(GPIOI)
    else if (port == GPIOI) { __HAL_RCC_GPIOI_CLK_ENABLE(); }
#endif
}

/* -------------------------------------------------------------------------- */
/*  detect pin runtime 보정                                                    */
/*                                                                            */
/*  APP_SD.c의 안전장치를 그대로 가져온다.                                     */
/* -------------------------------------------------------------------------- */
static void FW_SD_ConfigureDetectPinRuntime(void)
{
    GPIO_InitTypeDef gpio_init;

    memset(&gpio_init, 0, sizeof(gpio_init));

    FW_SD_EnablePortClock(SD_DETECT_GPIO_Port);

    gpio_init.Pin   = SD_DETECT_Pin;
    gpio_init.Mode  = GPIO_MODE_INPUT;
    gpio_init.Pull  = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SD_DETECT_GPIO_Port, &gpio_init);
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: mount fresh                                                   */
/* -------------------------------------------------------------------------- */
static uint8_t FW_SD_MountFresh(fw_sd_scan_result_t *result)
{
    FRESULT fr;

    if (result == 0)
    {
        return 0u;
    }

    result->mount_ok           = false;
    result->last_bsp_init_status = 0xFFu;
    result->last_mount_result  = FR_INT_ERR;

    (void)f_mount(0, SDPath, 0u);
    (void)HAL_SD_DeInit(&hsd);

    result->last_bsp_init_status = BSP_SD_Init();
    if (result->last_bsp_init_status != MSD_OK)
    {
        return 0u;
    }

    if (HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_1B) != HAL_OK)
    {
        return 0u;
    }

    fr = f_mount(&SDFatFS, SDPath, 1u);
    result->last_mount_result = fr;
    if (fr != FR_OK)
    {
        return 0u;
    }

    result->mount_ok = true;
    return 1u;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: init                                                             */
/* -------------------------------------------------------------------------- */
void FW_SD_InitRuntime(void)
{
    FW_SD_ConfigureDetectPinRuntime();
}

/* -------------------------------------------------------------------------- */
/*  공개 API: unmount                                                          */
/* -------------------------------------------------------------------------- */
void FW_SD_Unmount(void)
{
    (void)f_mount(0, SDPath, 0u);
    (void)HAL_SD_DeInit(&hsd);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: card detect                                                      */
/* -------------------------------------------------------------------------- */
bool FW_SD_IsCardPresent(void)
{
    return (BSP_SD_IsDetected() == SD_PRESENT) ? true : false;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: package scan                                                     */
/* -------------------------------------------------------------------------- */
void FW_SD_ScanUpdatePackage(fw_sd_scan_result_t *out_result)
{
    FIL fp;
    FRESULT fr;

    if (out_result == 0)
    {
        return;
    }

    memset(out_result, 0, sizeof(*out_result));
    out_result->last_package_result = FW_PACKAGE_RESULT_FILE_IO;

    out_result->card_present = FW_SD_IsCardPresent();
    if (out_result->card_present == false)
    {
        return;
    }

    if (FW_SD_MountFresh(out_result) == 0u)
    {
        return;
    }

    memset(&fp, 0, sizeof(fp));
    fr = f_open(&fp, FW_UPDATE_FILENAME, FA_READ);
    out_result->last_open_result = fr;
    if (fr != FR_OK)
    {
        out_result->file_found = false;
        return;
    }

    out_result->file_found = true;
    out_result->last_package_result = FW_Package_ReadHeaderFromFile(&fp, &out_result->header);
    out_result->header_valid = (out_result->last_package_result == FW_PACKAGE_RESULT_OK) ? true : false;

    (void)f_close(&fp);
}

/* -------------------------------------------------------------------------- */
/*  공개 API: update file open                                                 */
/* -------------------------------------------------------------------------- */
fw_package_result_t FW_SD_OpenUpdatePackage(FIL *out_fp,
                                            fw_package_header_t *out_header)
{
    FRESULT fr;
    fw_package_result_t package_result;

    if ((out_fp == 0) || (out_header == 0))
    {
        return FW_PACKAGE_RESULT_FILE_IO;
    }

    memset(out_fp, 0, sizeof(*out_fp));
    memset(out_header, 0, sizeof(*out_header));

    if (FW_SD_IsCardPresent() == false)
    {
        return FW_PACKAGE_RESULT_FILE_IO;
    }

    if (FW_SD_MountFresh(&(fw_sd_scan_result_t){0}) == 0u)
    {
        return FW_PACKAGE_RESULT_FILE_IO;
    }

    fr = f_open(out_fp, FW_UPDATE_FILENAME, FA_READ);
    if (fr != FR_OK)
    {
        return FW_PACKAGE_RESULT_FILE_IO;
    }

    package_result = FW_Package_ReadHeaderFromFile(out_fp, out_header);
    if (package_result != FW_PACKAGE_RESULT_OK)
    {
        (void)f_close(out_fp);
        memset(out_fp, 0, sizeof(*out_fp));
        return package_result;
    }

    fr = f_lseek(out_fp, sizeof(fw_package_header_t));
    if (fr != FR_OK)
    {
        (void)f_close(out_fp);
        memset(out_fp, 0, sizeof(*out_fp));
        return FW_PACKAGE_RESULT_FILE_IO;
    }

    return FW_PACKAGE_RESULT_OK;
}
