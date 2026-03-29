#include "APP_SD.h"

#include "fatfs.h"
#include "bsp_driver_sd.h"
#include "Audio_Driver.h"

#include <string.h>

/* -------------------------------------------------------------------------- */
/*  외부 HAL handle                                                            */
/* -------------------------------------------------------------------------- */

extern SD_HandleTypeDef hsd;

/* -------------------------------------------------------------------------- */
/*  내부 설정 상수                                                              */
/* -------------------------------------------------------------------------- */

#ifndef APP_SD_DETECT_DEBOUNCE_MS
#define APP_SD_DETECT_DEBOUNCE_MS 40u
#endif

#ifndef APP_SD_MOUNT_RETRY_MS
#define APP_SD_MOUNT_RETRY_MS 1000u
#endif

#ifndef APP_SD_LIVE_REFRESH_MS
#define APP_SD_LIVE_REFRESH_MS 100u
#endif

#define APP_SD_FRESULT_INVALID   0xFFFFFFFFu
#define APP_SD_STATE_INVALID_U8  0xFFu
#define APP_SD_ROOT_SAMPLE_MAX   3u

/* -------------------------------------------------------------------------- */
/*  드라이버 내부 런타임 상태                                                   */
/*                                                                            */
/*  공개 저장소(APP_STATE.sd)에는 사람이 보고 싶은 "결과 상태"만 두고,          */
/*  debounce pending/due 같은 구현 세부 상태는 여기 static 런타임에 둔다.       */
/* -------------------------------------------------------------------------- */
typedef struct
{
    volatile uint8_t  detect_debounce_pending;
    volatile uint32_t detect_debounce_due_ms;

    /* ---------------------------------------------------------------------- */
    /*  EXTI ISR는 APP_STATE를 직접 건드리지 않고,                              */
    /*  먼저 runtime mailbox에만 edge 관측치를 적재한다.                       */
    /*                                                                        */
    /*  main loop의 APP_SD_Task()가 이 값을 APP_STATE.sd에 반영하므로,          */
    /*  UI snapshot 쪽에서 긴 memcpy를 위해 IRQ를 막을 필요를 줄일 수 있다.     */
    /* ---------------------------------------------------------------------- */
    volatile uint32_t detect_irq_count;
    volatile uint32_t last_detect_irq_ms;

    uint8_t  stable_present;
    uint32_t mount_retry_due_ms;
    uint32_t next_live_hal_refresh_ms;
} app_sd_runtime_t;

static app_sd_runtime_t s_app_sd_rt;

/* -------------------------------------------------------------------------- */
/*  시간 helper                                                                 */
/* -------------------------------------------------------------------------- */

static uint8_t APP_SD_TimeDue(uint32_t now_ms, uint32_t due_ms)
{
    return ((int32_t)(now_ms - due_ms) >= 0) ? 1u : 0u;
}

/* -------------------------------------------------------------------------- */
/*  GPIO helper                                                                 */
/* -------------------------------------------------------------------------- */

static void APP_SD_EnablePortClock(GPIO_TypeDef *port)
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

static void APP_SD_ConfigureDetectPinRuntime(void)
{
    GPIO_InitTypeDef gpio_init;

    /* ---------------------------------------------------------------------- */
    /*  CubeMX가 SD_DETECT를 단순 input 으로 다시 생성하더라도                   */
    /*  SD hotplug 구현에는 both-edge EXTI가 필요하므로                           */
    /*  APP_SD_Init() 시점에 원하는 모드로 다시 맞춘다.                          */
    /*                                                                        */
    /*  현재 BSP_PlatformIsDetected() 는                                        */
    /*    - pin == RESET : 카드 있음                                            */
    /*    - pin == SET   : 카드 없음                                            */
    /*  으로 해석하고 있으므로, detect 스위치를 high로 끌어올릴 pull-up을         */
    /*  기본값으로 건다.                                                        */
    /* ---------------------------------------------------------------------- */
    memset(&gpio_init, 0, sizeof(gpio_init));

    APP_SD_EnablePortClock(SD_DETECT_GPIO_Port);

    gpio_init.Pin   = SD_DETECT_Pin;
    gpio_init.Mode  = GPIO_MODE_IT_RISING_FALLING;
    gpio_init.Pull  = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(SD_DETECT_GPIO_Port, &gpio_init);

    /* ---------------------------------------------------------------------- */
    /*  재구성 직후 남아 있을 수 있는 EXTI pending bit를 정리한다.              */
    /*  그렇지 않으면 부팅 직후 의미 없는 첫 detect IRQ가 한 번 들어올 수 있다. */
    /* ---------------------------------------------------------------------- */
    __HAL_GPIO_EXTI_CLEAR_IT(SD_DETECT_Pin);
    HAL_NVIC_ClearPendingIRQ(EXTI2_IRQn);
}

/* -------------------------------------------------------------------------- */
/*  detect raw 샘플 helper                                                      */
/* -------------------------------------------------------------------------- */

static uint8_t APP_SD_ReadDetectRawPresent(void)
{
    return (BSP_SD_IsDetected() == SD_PRESENT) ? 1u : 0u;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: "지금 이 순간" FatFs 접근 가능 여부                               */
/*                                                                            */
/*  이 함수는 main loop의 APP_SD_Task()가 아직 돌기 전이라도                    */
/*    - raw DET 핀 상태                                                        */
/*    - detect debounce pending 여부                                           */
/*    - 가장 최근에 확정된 stable present 상태                                  */
/*    - initialized / mounted 공개 상태                                         */
/*  를 함께 보고 판단한다.                                                     */
/*                                                                            */
/*  따라서 Audio_Driver 같은 소비자는                                          */
/*  SD remove edge 직후에 APP_STATE가 완전히 반영되기 전이라도                  */
/*  "더 이상의 새 FatFs 접근은 금지" 라는 보수적 결정을 내릴 수 있다.         */
/*                                                                            */
/*  bring-up 단계에서는                                                          */
/*  false positive(조금 일찍 막는 것)보다                                      */
/*  false negative(빼졌는데 계속 읽는 것)가 훨씬 위험하므로                      */
/*  조건을 일부러 엄격하게 잡는다.                                             */
/* -------------------------------------------------------------------------- */
bool APP_SD_IsFsAccessAllowedNow(void)
{
    const app_sd_state_t *sd;

    sd = (const app_sd_state_t *)&g_app_state.sd;

    if (APP_SD_ReadDetectRawPresent() == 0u)
    {
        return false;
    }

    if (s_app_sd_rt.detect_debounce_pending != 0u)
    {
        return false;
    }

    if (s_app_sd_rt.stable_present == 0u)
    {
        return false;
    }

    if ((sd->initialized == false) || (sd->mounted == false))
    {
        return false;
    }

    return true;
}

/* -------------------------------------------------------------------------- */
/*  runtime -> APP_STATE detect 관측치 반영 helper                              */
/*                                                                            */
/*  중요한 점                                                                 */
/*  - EXTI ISR는 APP_STATE.sd를 직접 수정하지 않는다.                          */
/*  - 따라서 detect IRQ count / last IRQ 시각 / debounce pending 같은 값도     */
/*    main loop에서만 공개 저장소로 옮긴다.                                    */
/* -------------------------------------------------------------------------- */
static void APP_SD_MirrorRuntimeDetectState(app_sd_state_t *sd, uint8_t raw_present)
{
    if (sd == 0)
    {
        return;
    }

    sd->detect_raw_present      = (raw_present != 0u) ? true : false;
    sd->detect_stable_present   = (s_app_sd_rt.stable_present != 0u) ? true : false;
    sd->detect_debounce_pending = (s_app_sd_rt.detect_debounce_pending != 0u) ? true : false;
    sd->debounce_due_ms         = s_app_sd_rt.detect_debounce_due_ms;
    sd->detect_irq_count        = s_app_sd_rt.detect_irq_count;
    sd->last_detect_irq_ms      = s_app_sd_rt.last_detect_irq_ms;
}

/* -------------------------------------------------------------------------- */
/*  공개 저장소 초기화 helper                                                   */
/* -------------------------------------------------------------------------- */

static void APP_SD_ClearRootSamples(app_sd_state_t *sd)
{
    uint32_t index;

    if (sd == 0)
    {
        return;
    }

    sd->root_entry_sample_count = 0u;

    for (index = 0u; index < APP_SD_ROOT_SAMPLE_MAX; index++)
    {
        sd->root_entry_sample_type[index] = 0u;
        memset(sd->root_entry_sample_name[index], 0, sizeof(sd->root_entry_sample_name[index]));
    }
}

static void APP_SD_ClearMountedInfo(app_sd_state_t *sd)
{
    if (sd == 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  카드가 제거되었거나 mount가 실패했을 때                                 */
    /*  "현재 mount 된 볼륨에서만 유효한 값" 들만 지운다.                        */
    /*                                                                        */
    /*  hotplug 카운터 / 마지막 에러 코드 같은 것은 디버그 가치를 위해 유지한다. */
    /* ---------------------------------------------------------------------- */
    sd->initialized          = false;
    sd->mounted              = false;
    sd->fat_valid            = false;
    sd->is_fat32             = false;

    sd->fs_type              = 0u;
    sd->card_type            = 0u;
    sd->card_version         = 0u;
    sd->card_class           = 0u;
    sd->hal_state            = APP_SD_STATE_INVALID_U8;
    sd->transfer_state       = APP_SD_STATE_INVALID_U8;
    sd->last_bsp_init_status = APP_SD_STATE_INVALID_U8;

    sd->rel_card_add         = 0u;
    sd->block_nbr            = 0u;
    sd->block_size           = 0u;
    sd->log_block_nbr        = 0u;
    sd->log_block_size       = 0u;

    sd->sectors_per_cluster  = 0u;
    sd->total_clusters       = 0u;
    sd->free_clusters        = 0u;
    sd->sectors_per_fat      = 0u;
    sd->volume_start_sector  = 0u;
    sd->fat_start_sector     = 0u;
    sd->root_dir_base        = 0u;
    sd->data_start_sector    = 0u;

    sd->root_entry_count     = 0u;
    sd->root_file_count      = 0u;
    sd->root_dir_count       = 0u;

    sd->capacity_bytes       = 0ull;
    sd->total_bytes          = 0ull;
    sd->free_bytes           = 0ull;

    APP_SD_ClearRootSamples(sd);
}

/* -------------------------------------------------------------------------- */
/*  HAL / card info helper                                                      */
/* -------------------------------------------------------------------------- */

static void APP_SD_UpdateLiveHalFields(app_sd_state_t *sd)
{
    if (sd == 0)
    {
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  hsd.ErrorCode / Context 는 init 실패 후에도 디버그 가치가 있으므로       */
    /*  initialized 플래그와 무관하게 일단 읽어 둔다.                          */
    /*                                                                        */
    /*  다만 transfer state 는 카드가 실제로 올라온 뒤에만 의미가 있으므로      */
    /*  init 전/실패 상태에서는 invalid sentinel 로 둔다.                      */
    /* ---------------------------------------------------------------------- */
    sd->hal_state      = (uint8_t)hsd.State;
    sd->hal_error_code = hsd.ErrorCode;
    sd->hal_context    = hsd.Context;

    if (sd->initialized == false)
    {
        sd->transfer_state = APP_SD_STATE_INVALID_U8;
        return;
    }

    sd->transfer_state = BSP_SD_GetCardState();
}

static void APP_SD_CaptureCardInfo(app_sd_state_t *sd)
{
    HAL_SD_CardInfoTypeDef card_info;

    if (sd == 0)
    {
        return;
    }

    memset(&card_info, 0, sizeof(card_info));
    BSP_SD_GetCardInfo(&card_info);

    sd->card_type      = (uint8_t)card_info.CardType;
    sd->card_version   = (uint8_t)card_info.CardVersion;
    sd->card_class     = (uint8_t)card_info.Class;
    sd->rel_card_add   = (uint32_t)card_info.RelCardAdd;
    sd->block_nbr      = (uint32_t)card_info.BlockNbr;
    sd->block_size     = (uint32_t)card_info.BlockSize;
    sd->log_block_nbr  = (uint32_t)card_info.LogBlockNbr;
    sd->log_block_size = (uint32_t)card_info.LogBlockSize;

    sd->capacity_bytes = (uint64_t)sd->log_block_nbr * (uint64_t)sd->log_block_size;
}

/* -------------------------------------------------------------------------- */
/*  FAT / root metadata helper                                                  */
/* -------------------------------------------------------------------------- */

static void APP_SD_ScanRootDirectory(app_sd_state_t *sd)
{
    DIR dir;
    FILINFO info;
    FRESULT fr;
    uint8_t sample_index;

    if (sd == 0)
    {
        return;
    }

    sd->root_entry_count = 0u;
    sd->root_file_count  = 0u;
    sd->root_dir_count   = 0u;
    APP_SD_ClearRootSamples(sd);

    fr = f_opendir(&dir, SDPath);
    sd->last_root_scan_fresult = (uint32_t)fr;

    if (fr != FR_OK)
    {
        return;
    }

    sample_index = 0u;

    for (;;)
    {
        fr = f_readdir(&dir, &info);
        if (fr != FR_OK)
        {
            sd->last_root_scan_fresult = (uint32_t)fr;
            break;
        }

        if (info.fname[0] == '\0')
        {
            sd->last_root_scan_fresult = (uint32_t)FR_OK;
            break;
        }

#ifdef AM_VOL
        if ((info.fattrib & AM_VOL) != 0u)
        {
            continue;
        }
#endif

        sd->root_entry_count++;

#ifdef AM_DIR
        if ((info.fattrib & AM_DIR) != 0u)
        {
            sd->root_dir_count++;
        }
        else
#endif
        {
            sd->root_file_count++;
        }

        if (sample_index < APP_SD_ROOT_SAMPLE_MAX)
        {
            size_t copy_len;

            copy_len = strlen(info.fname);
            if (copy_len >= sizeof(sd->root_entry_sample_name[sample_index]))
            {
                copy_len = sizeof(sd->root_entry_sample_name[sample_index]) - 1u;
            }

            memcpy(sd->root_entry_sample_name[sample_index], info.fname, copy_len);
            sd->root_entry_sample_name[sample_index][copy_len] = '\0';
#ifdef AM_DIR
            sd->root_entry_sample_type[sample_index] = ((info.fattrib & AM_DIR) != 0u) ? 2u : 1u;
#else
            sd->root_entry_sample_type[sample_index] = 1u;
#endif
            sample_index++;
        }
    }

    sd->root_entry_sample_count = sample_index;
    (void)f_closedir(&dir);
}

static void APP_SD_ReadFatMetadata(app_sd_state_t *sd)
{
    FATFS *fs;
    DWORD free_clst;
    FRESULT fr;
    uint64_t sector_size_bytes;

    if (sd == 0)
    {
        return;
    }

    fs = 0;
    free_clst = 0u;

    fr = f_getfree(SDPath, &free_clst, &fs);
    sd->last_getfree_fresult = (uint32_t)fr;

    if ((fr != FR_OK) || (fs == 0))
    {
        sd->fat_valid = false;
        sd->is_fat32  = false;
        return;
    }

    sd->fat_valid           = true;
    sd->fs_type             = fs->fs_type;
    sd->is_fat32            = (fs->fs_type == FS_FAT32) ? true : false;
    sd->sectors_per_cluster = fs->csize;
    sd->sectors_per_fat     = fs->fsize;
    sd->volume_start_sector = fs->volbase;
    sd->fat_start_sector    = fs->fatbase;
    sd->root_dir_base       = fs->dirbase;
    sd->data_start_sector   = fs->database;

    if (fs->n_fatent >= 2u)
    {
        sd->total_clusters = (uint32_t)(fs->n_fatent - 2u);
    }
    else
    {
        sd->total_clusters = 0u;
    }

    sd->free_clusters = (uint32_t)free_clst;

    sector_size_bytes = (sd->log_block_size != 0u) ?
                        (uint64_t)sd->log_block_size :
                        512ull;

    sd->total_bytes = (uint64_t)sd->total_clusters *
                      (uint64_t)sd->sectors_per_cluster *
                      sector_size_bytes;

    sd->free_bytes  = (uint64_t)sd->free_clusters *
                      (uint64_t)sd->sectors_per_cluster *
                      sector_size_bytes;

    APP_SD_ScanRootDirectory(sd);
}

/* -------------------------------------------------------------------------- */
/*  mount / unmount 전 storage client 정리 helper                               */
/*                                                                            */
/*  현재 이 프로젝트에서 SD를 직접 길게 잡고 있는 대표 소비자는                  */
/*  Audio_Driver의 WAV streaming 경로다.                                       */
/*                                                                            */
/*  따라서 APP_SD가 f_mount(NULL) / HAL_SD_DeInit()를 호출하기 전에             */
/*  먼저 오디오 쪽에 "지금부터 SD 세션이 내려간다" 를 알려서                    */
/*    - WAV runtime 비활성화                                                   */
/*    - software FIFO에 남은 SD 기반 tail 제거                                  */
/*    - 이후 새 f_read/f_open 차단                                             */
/*  가 먼저 끝나도록 만든다.                                                   */
/*                                                                            */
/*  나중에 사진 뷰어, 로그 리더, 설정 파일 로더 등                               */
/*  다른 SD 소비자가 늘어나면 이 함수에 정리 호출을 추가하면 된다.             */
/* -------------------------------------------------------------------------- */
static void APP_SD_StopStorageClientsBeforeTearDown(void)
{
    Audio_Driver_OnSdWillUnmount();
}

/* -------------------------------------------------------------------------- */
/*  mount / unmount helper                                                      */
/* -------------------------------------------------------------------------- */

static void APP_SD_RequestMountRetry(app_sd_state_t *sd, uint32_t now_ms)
{
    s_app_sd_rt.mount_retry_due_ms = now_ms + APP_SD_MOUNT_RETRY_MS;

    if (sd != 0)
    {
        sd->mount_retry_due_ms = s_app_sd_rt.mount_retry_due_ms;
    }
}

static void APP_SD_ClearMountRetry(app_sd_state_t *sd)
{
    s_app_sd_rt.mount_retry_due_ms = 0u;

    if (sd != 0)
    {
        sd->mount_retry_due_ms = 0u;
    }
}

static void APP_SD_Unmount(uint32_t now_ms)
{
    app_sd_state_t *sd;
    uint8_t had_live_volume;

    sd = (app_sd_state_t *)&g_app_state.sd;
    had_live_volume = ((sd->mounted == true) || (sd->initialized == true)) ? 1u : 0u;

    /* ---------------------------------------------------------------------- */
    /*  파일시스템과 HAL peripheral을 둘 다 내리기 전에                         */
    /*  먼저 SD 볼륨을 쓰고 있던 상위 소비자들을 정리한다.                       */
    /*                                                                        */
    /*  이 순서를 지키지 않으면                                                 */
    /*    1) Audio_Driver는 아직 FIL을 들고 있고                                */
    /*    2) APP_SD는 이미 HAL_SD_DeInit()를 해 버려서                          */
    /*    3) 다음 f_read/f_close가 bus fault로 이어질 수 있다.                  */
    /* ---------------------------------------------------------------------- */
    APP_SD_StopStorageClientsBeforeTearDown();

    /* ---------------------------------------------------------------------- */
    /*  그 다음 실제 파일시스템과 HAL peripheral을 내린다.                      */
    /*                                                                        */
    /*  hot-remove 이후 다음 재삽입 때                                         */
    /*  stale state 없이 다시 BSP_SD_Init() / f_mount() 를 타게 하려는 목적이다. */
    /* ---------------------------------------------------------------------- */
    (void)f_mount(0, SDPath, 0u);
    (void)HAL_SD_DeInit(&hsd);

    if (had_live_volume != 0u)
    {
        sd->unmount_count++;
        sd->last_unmount_ms = now_ms;
    }

    APP_SD_ClearMountedInfo(sd);
    APP_SD_ClearMountRetry(sd);
    s_app_sd_rt.next_live_hal_refresh_ms = now_ms;
}

static void APP_SD_AttemptMount(uint32_t now_ms)
{
    app_sd_state_t *sd;
    FRESULT fr;

    sd = (app_sd_state_t *)&g_app_state.sd;

    /* ---------------------------------------------------------------------- */
    /*  mount는 항상 "소비자 정리 -> 정리 -> HAL init -> card info -> f_mount   */
    /*  -> FAT query" 순서로 새로 시도한다.                                     */
    /*                                                                        */
    /*  이전 세션의 반쯤 열린 상태를 최대한 끊고 시작하기 위해                  */
    /*  f_mount(NULL) / HAL_SD_DeInit() 를 먼저 넣지만,                          */
    /*  그보다 더 먼저 상위 소비자에게 SD 세션 종료를 통보해야 한다.             */
    /* ---------------------------------------------------------------------- */
    sd->mount_attempt_count++;
    sd->last_mount_attempt_ms = now_ms;
    sd->last_mount_fresult     = APP_SD_FRESULT_INVALID;
    sd->last_getfree_fresult   = APP_SD_FRESULT_INVALID;
    sd->last_root_scan_fresult = APP_SD_FRESULT_INVALID;

    /* ---------------------------------------------------------------------- */
    /*  이전 세션에서 WAV streaming 같은 SD 소비자가 살아 있었다면               */
    /*  mount 재시도 전에 반드시 끊는다.                                        */
    /*                                                                        */
    /*  특히 카드가 막 빠졌다가 다시 들어오는 hotplug 구간에서는                */
    /*  "오디오는 예전 FIL을 계속 들고 있고, APP_SD는 버스를 재초기화" 하는    */
    /*  꼬임을 여기서 먼저 차단해야 한다.                                       */
    /* ---------------------------------------------------------------------- */
    APP_SD_StopStorageClientsBeforeTearDown();

    (void)f_mount(0, SDPath, 0u);
    (void)HAL_SD_DeInit(&hsd);
    APP_SD_ClearMountedInfo(sd);

    sd->last_bsp_init_status = BSP_SD_Init();
    if (sd->last_bsp_init_status != MSD_OK)
    {
        APP_SD_UpdateLiveHalFields(sd);
        s_app_sd_rt.next_live_hal_refresh_ms = now_ms + APP_SD_LIVE_REFRESH_MS;
        sd->mount_fail_count++;
        APP_SD_RequestMountRetry(sd, now_ms);
        return;
    }

    /* ---------------------------------------------------------------------- */
    /*  현재 프로젝트는 1-bit SDIO 기준으로 운용하려고 하므로                    */
    /*  CubeMX / .ioc 값과 별개로 runtime 에서 한 번 더 1-bit를 강제한다.      */
    /*                                                                        */
    /*  주의                                                                  */
    /*  - 이 코드는 HAL_SD_Init()가 끝난 직후에만 호출한다.                    */
    /*  - 현재 보드에서는 DAT0/CMD/CLK 만으로도 동작 가능해야 하며,             */
    /*    DAT1/2/3 는 사용하지 않아도 된다.                                    */
    /* ---------------------------------------------------------------------- */
    if (HAL_SD_ConfigWideBusOperation(&hsd, SDIO_BUS_WIDE_1B) != HAL_OK)
    {
        APP_SD_UpdateLiveHalFields(sd);
        s_app_sd_rt.next_live_hal_refresh_ms = now_ms + APP_SD_LIVE_REFRESH_MS;
        sd->mount_fail_count++;
        APP_SD_RequestMountRetry(sd, now_ms);
        return;
    }

    sd->initialized = true;
    APP_SD_UpdateLiveHalFields(sd);
    APP_SD_CaptureCardInfo(sd);

    fr = f_mount(&SDFatFS, SDPath, 1u);
    sd->last_mount_fresult = (uint32_t)fr;

    if (fr != FR_OK)
    {
        APP_SD_UpdateLiveHalFields(sd);
        s_app_sd_rt.next_live_hal_refresh_ms = now_ms + APP_SD_LIVE_REFRESH_MS;
        sd->mount_fail_count++;
        APP_SD_RequestMountRetry(sd, now_ms);
        return;
    }

    sd->mounted = true;
    sd->mount_success_count++;
    sd->last_mount_ms = now_ms;
    APP_SD_ClearMountRetry(sd);

    APP_SD_ReadFatMetadata(sd);
    APP_SD_UpdateLiveHalFields(sd);
    s_app_sd_rt.next_live_hal_refresh_ms = now_ms + APP_SD_LIVE_REFRESH_MS;
}

/* -------------------------------------------------------------------------- */
/*  stable detect change helper                                                 */
/* -------------------------------------------------------------------------- */

static void APP_SD_HandleStableDetectChange(uint8_t new_present, uint32_t now_ms)
{
    app_sd_state_t *sd;

    sd = (app_sd_state_t *)&g_app_state.sd;

    s_app_sd_rt.stable_present    = new_present;
    sd->detect_stable_present     = (new_present != 0u) ? true : false;
    sd->last_detect_change_ms     = now_ms;

    if (new_present != 0u)
    {
        sd->detect_insert_count++;
        APP_SD_RequestMountRetry(sd, now_ms);
    }
    else
    {
        sd->detect_remove_count++;
        APP_SD_Unmount(now_ms);
    }
}

/* -------------------------------------------------------------------------- */
/*  공개 API: init                                                              */
/* -------------------------------------------------------------------------- */

void APP_SD_Init(void)
{
    app_sd_state_t *sd;
    uint32_t now_ms;
    uint8_t raw_present;

    sd = (app_sd_state_t *)&g_app_state.sd;

    memset(&s_app_sd_rt, 0, sizeof(s_app_sd_rt));
    APP_SD_ConfigureDetectPinRuntime();

    now_ms = HAL_GetTick();
    raw_present = APP_SD_ReadDetectRawPresent();

    sd->detect_raw_present      = (raw_present != 0u) ? true : false;
    sd->detect_stable_present   = (raw_present != 0u) ? true : false;
    sd->detect_debounce_pending = false;
    sd->debounce_due_ms         = 0u;
    sd->last_detect_irq_ms      = 0u;
    sd->last_detect_change_ms   = now_ms;

    s_app_sd_rt.stable_present = raw_present;
    s_app_sd_rt.detect_irq_count = 0u;
    s_app_sd_rt.last_detect_irq_ms = 0u;
    s_app_sd_rt.next_live_hal_refresh_ms = now_ms;

    if (raw_present != 0u)
    {
        /* ------------------------------------------------------------------ */
        /*  부팅 시점에 이미 카드가 꽂혀 있으면                                 */
        /*  insert 카운터를 올리지는 않고,                                     */
        /*  "지금 바로 mount 한 번 시도" 만 예약한다.                          */
        /* ------------------------------------------------------------------ */
        APP_SD_RequestMountRetry(sd, now_ms);
        sd->mount_retry_due_ms = now_ms;
        s_app_sd_rt.mount_retry_due_ms = now_ms;
    }
    else
    {
        APP_SD_ClearMountRetry(sd);
    }
}

/* -------------------------------------------------------------------------- */
/*  공개 API: detect EXTI ISR 진입점                                            */
/* -------------------------------------------------------------------------- */

void APP_SD_OnDetectExti(void)
{
    uint32_t now_ms;

    now_ms = HAL_GetTick();

    /* ---------------------------------------------------------------------- */
    /*  DET 핀 edge가 들어왔다고 바로 insert/remove 로 확정하지 않는다.         */
    /*                                                                        */
    /*  소켓 스위치 bounce 때문에                                               */
    /*  "지금부터 N ms 뒤에 raw 핀을 한 번 더 읽어 안정 상태를 확정" 하는      */
    /*  debounce 구조를 사용한다.                                               */
    /*                                                                        */
    /*  중요한 점                                                               */
    /*  - ISR는 APP_STATE.sd를 직접 수정하지 않는다.                            */
    /*  - 대신 runtime mailbox만 갱신하고,                                      */
    /*    main loop의 APP_SD_Task()가 공개 저장소로 옮긴다.                     */
    /* ---------------------------------------------------------------------- */
    s_app_sd_rt.detect_debounce_due_ms  = now_ms + APP_SD_DETECT_DEBOUNCE_MS;
    s_app_sd_rt.detect_debounce_pending = 1u;
    s_app_sd_rt.last_detect_irq_ms      = now_ms;
    s_app_sd_rt.detect_irq_count++;
}


/* -------------------------------------------------------------------------- */
/*  공개 API: main loop task                                                    */
/* -------------------------------------------------------------------------- */

void APP_SD_Task(uint32_t now_ms)
{
    app_sd_state_t *sd;
    uint8_t raw_present;
    uint8_t pending_snapshot;
    uint32_t due_snapshot;

    sd = (app_sd_state_t *)&g_app_state.sd;

    /* ---------------------------------------------------------------------- */
    /*  EXTI를 놓쳤더라도 main loop가 raw DET 핀을 계속 관찰해서                 */
    /*  stable state 와 차이가 생기면 debounce를 다시 건다.                    */
    /* ---------------------------------------------------------------------- */
    raw_present = APP_SD_ReadDetectRawPresent();

    if ((raw_present != s_app_sd_rt.stable_present) &&
        (s_app_sd_rt.detect_debounce_pending == 0u))
    {
        s_app_sd_rt.detect_debounce_due_ms  = now_ms + APP_SD_DETECT_DEBOUNCE_MS;
        s_app_sd_rt.detect_debounce_pending = 1u;
    }

    /* ---------------------------------------------------------------------- */
    /*  runtime mailbox 상태를 공개 저장소로 반영한다.                          */
    /*                                                                        */
    /*  이제 APP_STATE.sd는 ISR가 아니라 main loop만 갱신하므로,                */
    /*  snapshot 복사에서 긴 IRQ-off를 피하기 쉬워진다.                         */
    /* ---------------------------------------------------------------------- */
    APP_SD_MirrorRuntimeDetectState(sd, raw_present);

    /* ---------------------------------------------------------------------- */
    /*  detect debounce 만료 처리                                              */
    /*                                                                        */
    /*  pending flag clear는 ISR와 충돌할 수 있으므로                           */
    /*  이 작은 check+clear 구간만 짧게 보호한다.                              */
    /* ---------------------------------------------------------------------- */
    __disable_irq();
    pending_snapshot = s_app_sd_rt.detect_debounce_pending;
    due_snapshot     = s_app_sd_rt.detect_debounce_due_ms;

    if ((pending_snapshot != 0u) &&
        (APP_SD_TimeDue(now_ms, due_snapshot) != 0u))
    {
        s_app_sd_rt.detect_debounce_pending = 0u;
        pending_snapshot = 0u;
    }
    __enable_irq();

    if ((sd->detect_debounce_pending == true) && (pending_snapshot == 0u))
    {
        raw_present = APP_SD_ReadDetectRawPresent();

        if (raw_present != s_app_sd_rt.stable_present)
        {
            APP_SD_HandleStableDetectChange(raw_present, now_ms);
        }

        APP_SD_MirrorRuntimeDetectState(sd, raw_present);
    }

    /* ---------------------------------------------------------------------- */
    /*  카드가 안정적으로 꽂혀 있지만 아직 mount 가 안 되어 있으면               */
    /*  retry due 시각에 맞춰 다시 브링업을 시도한다.                           */
    /* ---------------------------------------------------------------------- */
    if ((sd->detect_stable_present == true) &&
        (sd->mounted == false) &&
        (s_app_sd_rt.mount_retry_due_ms != 0u) &&
        (APP_SD_TimeDue(now_ms, s_app_sd_rt.mount_retry_due_ms) != 0u))
    {
        APP_SD_AttemptMount(now_ms);
    }

    /* ---------------------------------------------------------------------- */
    /*  HAL live 상태는 매 loop마다 읽지 않고                                   */
    /*  저주기 throttled refresh로만 갱신한다.                                  */
    /*                                                                        */
    /*  이유                                                                    */
    /*  - hsd.State / ErrorCode / Context는 byte/word 읽기라 아주 무겁지는 않다. */
    /*  - 하지만 매 loop마다 무조건 갱신할 이유도 없으므로,                     */
    /*    디버그 가시성은 유지하면서 steady-state 오버헤드를 줄인다.            */
    /* ---------------------------------------------------------------------- */
    if (APP_SD_TimeDue(now_ms, s_app_sd_rt.next_live_hal_refresh_ms) != 0u)
    {
        APP_SD_UpdateLiveHalFields(sd);
        s_app_sd_rt.next_live_hal_refresh_ms = now_ms + APP_SD_LIVE_REFRESH_MS;
    }
}

