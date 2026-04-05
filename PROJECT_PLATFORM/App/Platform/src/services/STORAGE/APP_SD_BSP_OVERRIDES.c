#include "bsp_driver_sd.h"
#include "APP_SD.h"

#include <stdint.h>
#include <string.h>

/* -------------------------------------------------------------------------- */
/*  APP_SD_BSP_OVERRIDES                                                      */
/*                                                                            */
/*  목적                                                                      */
/*  - CubeMX가 생성한 bsp_driver_sd.c 의 __weak BSP_SD_ReadBlocks /            */
/*    BSP_SD_WriteBlocks 를 strong symbol로 덮어쓴다.                          */
/*  - FatFs direct transfer 가 임의의 BYTE* 버퍼 주소를                        */
/*    disk_read / disk_write 쪽으로 내려 보낼 때에도                           */
/*    STM32F4 SD HAL 하부에 안전한 형태로 정규화한다.                          */
/*                                                                            */
/*  왜 별도 파일로 뺐는가                                                      */
/*  - bsp_driver_sd.c 와 sd_diskio.c 는 CubeMX 재생성 대상이다.               */
/*  - 이 파일은 사용자 소스 파일이므로 IOC 재생성에 날아가지 않는다.           */
/*  - __weak override 방식이라 generated file 본문을 직접 수정할 필요가 없다. */
/*                                                                            */
/*  이번 파일이 해결하려는 실제 문제 축                                         */
/*  - FatFs long read/write 는 direct transfer 경로에서                        */
/*    user buffer 주소를 그대로 disk I/O 하부로 전달할 수 있다.                */
/*  - audio monkey test 중 WAV stream buffer tail append 같은 경로는           */
/*    구조체 내부 offset 주소를 destination 으로 만들 수 있다.                 */
/*  - 이런 주소를 그대로 SD HAL 에 넘기면                                      */
/*    HAL 버전/설정/빌드 옵션에 따라                                           */
/*    정렬 문제, bus fault, 메모리 훼손 같은 불안정성이 드러날 수 있다.        */
/*                                                                            */
/*  방어 전략                                                                 */
/*  - 버퍼 시작 주소가 4-byte aligned 이면                                     */
/*    기존처럼 multi-block direct access 를 사용한다.                         */
/*  - 버퍼 시작 주소가 4-byte aligned 가 아니면                                */
/*    512-byte aligned scratch sector buffer 를 사용해서                       */
/*    sector 단위 read/write 를 수행한 뒤 memcpy 한다.                         */
/*                                                                            */
/*  결과                                                                      */
/*  - generated sd_diskio.c 는 그대로 둔 채                                    */
/*  - 하부 BSP 층에서 direct transfer 위험도를 흡수한다.                       */
/* -------------------------------------------------------------------------- */

/* CubeMX 가 만든 SD handle 을 그대로 사용한다. */
extern SD_HandleTypeDef hsd;

#ifndef APP_SD_BSP_SECTOR_BYTES
#define APP_SD_BSP_SECTOR_BYTES 512u
#endif

#ifndef APP_SD_CCMRAM_BASE
#define APP_SD_CCMRAM_BASE 0x10000000u
#endif

#ifndef APP_SD_CCMRAM_SIZE_BYTES
#define APP_SD_CCMRAM_SIZE_BYTES (64u * 1024u)
#endif

typedef char app_sd_bsp_sector_must_be_word_multiple[
    ((APP_SD_BSP_SECTOR_BYTES != 0u) &&
     ((APP_SD_BSP_SECTOR_BYTES % 4u) == 0u)) ? 1 : -1];

/* -------------------------------------------------------------------------- */
/*  aligned scratch sector buffer                                              */
/*                                                                            */
/*  uint32_t 배열 형태로 잡아서                                                */
/*  컴파일러/ABI 에 의해 최소 4-byte alignment 가 자연스럽게 보장되도록 한다. */
/* -------------------------------------------------------------------------- */
static uint32_t s_app_sd_scratch_sector_words[APP_SD_BSP_SECTOR_BYTES / 4u];

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 포인터 4-byte alignment 검사                                     */
/* -------------------------------------------------------------------------- */
static uint8_t APP_SD_BSP_IsWordAligned(const void *ptr)
{
    return ((((uintptr_t)ptr) & 0x3u) == 0u) ? 1u : 0u;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: 버퍼가 STM32F407 CCMRAM 안에 있는지 검사                         */
/*                                                                            */
/*  왜 이 검사가 필요한가                                                      */
/*  - 이번 프로젝트는 일반 SRAM 압박을 줄이기 위해 큰 정적 버퍼를 CCMRAM으로     */
/*    옮겨 쓰고 있다.                                                          */
/*  - 하지만 저장장치 I/O 경로는 "4-byte aligned 이면 direct path 로 내려도      */
/*    항상 안전하다"라고 단정하면 안 된다.                                    */
/*  - STM32F4의 CCMRAM(0x1000_0000 ~ 0x1000_FFFF)은 일반 SRAM과 버스 특성이     */
/*    다르고, 향후 HAL/최적화 옵션/전송 경로가 달라질 때 direct block transfer   */
/*    의 소스/목적지로 그대로 넘기는 것이 불안정해질 수 있다.                   */
/*                                                                            */
/*  현재 관찰된 실제 문제                                                      */
/*  - MOTOR recorder 의 flush burst buffer 가 CCMRAM으로 이동된 뒤             */
/*    첫 f_write() 에서 즉시 WRITE ERR 가 발생했다.                            */
/*  - 그 증상은 "저장계층이 CCMRAM 버퍼를 direct storage buffer 로는            */
/*    신뢰하지 못한다"는 신호로 보는 것이 가장 보수적이고 안전하다.             */
/*                                                                            */
/*  따라서                                                                    */
/*  - CCMRAM 버퍼는 정렬 여부와 무관하게 direct path 를 금지하고,                */
/*  - 아래 scratch-buffer 경로를 통해 일반 SRAM의 512-byte stage buffer 로      */
/*    우회시킨다.                                                              */
/* -------------------------------------------------------------------------- */
static uint8_t APP_SD_BSP_IsInCcmRam(const void *ptr)
{
    uintptr_t addr;

    if (ptr == 0)
    {
        return 0u;
    }

    addr = (uintptr_t)ptr;
    return ((addr >= (uintptr_t)APP_SD_CCMRAM_BASE) &&
            (addr < ((uintptr_t)APP_SD_CCMRAM_BASE + (uintptr_t)APP_SD_CCMRAM_SIZE_BYTES))) ? 1u : 0u;
}

static uint8_t APP_SD_BSP_MakeBufferFlags(const void *ptr, uint8_t direct_path)
{
    uint8_t flags;

    flags = 0u;

    if (APP_SD_BSP_IsWordAligned(ptr) != 0u)
    {
        flags |= APP_SD_IO_BUF_FLAG_WORD_ALIGNED;
    }

    if (APP_SD_BSP_IsInCcmRam(ptr) != 0u)
    {
        flags |= APP_SD_IO_BUF_FLAG_CCMRAM;
    }

    if (direct_path != 0u)
    {
        flags |= APP_SD_IO_BUF_FLAG_DIRECT_PATH;
    }

    return flags;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: direct block transfer 에 바로 써도 되는 버퍼인지 판단             */
/*                                                                            */
/*  direct path 허용 조건                                                      */
/*  1) 널 포인터가 아니어야 한다.                                              */
/*  2) 4-byte aligned 여야 한다.                                              */
/*  3) CCMRAM 이 아니어야 한다.                                               */
/*                                                                            */
/*  하나라도 어기면 scratch path 로 내려 보내서                               */
/*  storage HAL 이 항상 일반 SRAM의 정렬된 sector buffer 만 보게 만든다.       */
/* -------------------------------------------------------------------------- */
static uint8_t APP_SD_BSP_CanUseDirectTransferBuffer(const void *ptr)
{
    if (ptr == 0)
    {
        return 0u;
    }

    if (APP_SD_BSP_IsWordAligned(ptr) == 0u)
    {
        return 0u;
    }

    if (APP_SD_BSP_IsInCcmRam(ptr) != 0u)
    {
        return 0u;
    }

    return 1u;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: card transfer complete 대기                                     */
/*                                                                            */
/*  생성된 sd_diskio.c 도 BSP 호출 뒤에 기다리지만,                            */
/*  scratch-buffer 경로는 sector 1개씩 read/write 후                           */
/*  즉시 memcpy 를 수행해야 하므로                                             */
/*  이 파일 안에서도 각 sector 완료를 직접 확인해야 한다.                      */
/*                                                                            */
/*  timeout 정책                                                              */
/*  - Timeout == 0 이면 즉시 실패하지 않고                                     */
/*    HAL 기본값을 기대하는 기존 generated 코드의 의도를 존중하기 위해         */
/*    SD_DATATIMEOUT 를 fallback 으로 사용한다.                                */
/* -------------------------------------------------------------------------- */
static uint8_t APP_SD_BSP_WaitCardReady(uint32_t timeout_ms)
{
    uint32_t start_tick;
    uint32_t effective_timeout_ms;

    start_tick = HAL_GetTick();
    effective_timeout_ms = (timeout_ms == 0u) ? SD_DATATIMEOUT : timeout_ms;

    for (;;)
    {
        if (BSP_SD_GetCardState() == SD_TRANSFER_OK)
        {
            return MSD_OK;
        }

        if ((HAL_GetTick() - start_tick) >= effective_timeout_ms)
        {
            APP_SD_RecordIoFailure(APP_SD_IO_OP_NONE,
                                   MSD_ERROR,
                                   APP_SD_IO_HAL_STATUS_WAIT_TIMEOUT,
                                   0u,
                                   0u,
                                   0,
                                   0u);
            return MSD_ERROR;
        }
    }
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: direct multi-block read helper                                  */
/*                                                                            */
/*  aligned destination 인 경우에는                                            */
/*  generated weak implementation과 동일한 contract 를 유지한다.               */
/*  - HAL_SD_ReadBlocks 를 한 번 호출한다.                                     */
/*  - 완료 대기는 caller(sd_diskio.c)가 수행한다.                              */
/* -------------------------------------------------------------------------- */
static uint8_t APP_SD_BSP_ReadBlocksDirect(uint8_t *dst,
                                           uint32_t read_addr,
                                           uint32_t num_blocks,
                                           uint32_t timeout_ms)
{
    HAL_StatusTypeDef hal_status;

    if (APP_SD_BSP_WaitCardReady(timeout_ms) != MSD_OK)
    {
        return MSD_ERROR;
    }

    hal_status = HAL_SD_ReadBlocks(&hsd, dst, read_addr, num_blocks, timeout_ms);
    if (hal_status != HAL_OK)
    {
        APP_SD_RecordIoFailure(APP_SD_IO_OP_READ,
                               MSD_ERROR,
                               (uint8_t)hal_status,
                               read_addr,
                               num_blocks,
                               dst,
                               APP_SD_BSP_MakeBufferFlags(dst, 1u));
        return MSD_ERROR;
    }

    return MSD_OK;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: scratch-buffer read helper                                      */
/*                                                                            */
/*  misaligned destination 인 경우                                             */
/*  - sector 1개를 aligned scratch buffer 로 읽는다.                           */
/*  - transfer complete 를 확인한다.                                           */
/*  - caller destination 으로 memcpy 한다.                                     */
/*                                                                            */
/*  성능은 direct multi-block 보다 떨어지지만                                  */
/*  이 경로는 misaligned buffer 에 대해서만 사용하므로                         */
/*  브링업 안전성 이득이 훨씬 크다.                                            */
/* -------------------------------------------------------------------------- */
static uint8_t APP_SD_BSP_ReadBlocksScratch(uint8_t *dst,
                                            uint32_t read_addr,
                                            uint32_t num_blocks,
                                            uint32_t timeout_ms)
{
    uint32_t block_index;
    uint8_t *scratch;
    HAL_StatusTypeDef hal_status;

    scratch = (uint8_t *)s_app_sd_scratch_sector_words;

    for (block_index = 0u; block_index < num_blocks; block_index++)
    {
        if (APP_SD_BSP_WaitCardReady(timeout_ms) != MSD_OK)
        {
            return MSD_ERROR;
        }

        hal_status = HAL_SD_ReadBlocks(&hsd,
                                       scratch,
                                       read_addr + block_index,
                                       1u,
                                       timeout_ms);
        if (hal_status != HAL_OK)
        {
            APP_SD_RecordIoFailure(APP_SD_IO_OP_READ,
                                   MSD_ERROR,
                                   (uint8_t)hal_status,
                                   read_addr + block_index,
                                   1u,
                                   dst,
                                   APP_SD_BSP_MakeBufferFlags(dst, 0u));
            return MSD_ERROR;
        }

        if (APP_SD_BSP_WaitCardReady(timeout_ms) != MSD_OK)
        {
            return MSD_ERROR;
        }

        memcpy(&dst[block_index * APP_SD_BSP_SECTOR_BYTES],
               scratch,
               APP_SD_BSP_SECTOR_BYTES);
    }

    return MSD_OK;
}

/* -------------------------------------------------------------------------- */
/*  내부 유틸: scratch-buffer write helper                                     */
/*                                                                            */
/*  misaligned source 인 경우                                                  */
/*  - caller source sector 1개를 aligned scratch 로 복사한다.                  */
/*  - aligned scratch 로 sector 1개 write 를 수행한다.                         */
/*  - transfer complete 를 확인한 뒤 다음 sector 로 진행한다.                  */
/* -------------------------------------------------------------------------- */
static uint8_t APP_SD_BSP_WriteBlocksScratch(const uint8_t *src,
                                             uint32_t write_addr,
                                             uint32_t num_blocks,
                                             uint32_t timeout_ms)
{
    uint32_t block_index;
    uint8_t *scratch;
    HAL_StatusTypeDef hal_status;

    scratch = (uint8_t *)s_app_sd_scratch_sector_words;

    for (block_index = 0u; block_index < num_blocks; block_index++)
    {
        memcpy(scratch,
               &src[block_index * APP_SD_BSP_SECTOR_BYTES],
               APP_SD_BSP_SECTOR_BYTES);

        if (APP_SD_BSP_WaitCardReady(timeout_ms) != MSD_OK)
        {
            return MSD_ERROR;
        }

        hal_status = HAL_SD_WriteBlocks(&hsd,
                                        scratch,
                                        write_addr + block_index,
                                        1u,
                                        timeout_ms);
        if (hal_status != HAL_OK)
        {
            APP_SD_RecordIoFailure(APP_SD_IO_OP_WRITE,
                                   MSD_ERROR,
                                   (uint8_t)hal_status,
                                   write_addr + block_index,
                                   1u,
                                   src,
                                   APP_SD_BSP_MakeBufferFlags(src, 0u));
            return MSD_ERROR;
        }

        if (APP_SD_BSP_WaitCardReady(timeout_ms) != MSD_OK)
        {
            return MSD_ERROR;
        }
    }

    return MSD_OK;
}

/* -------------------------------------------------------------------------- */
/*  strong override: BSP_SD_ReadBlocks                                         */
/*                                                                            */
/*  generated weak symbol 을 대체한다.                                          */
/*  이 함수는 sd_diskio.c -> disk_read -> f_read direct transfer 경로의         */
/*  마지막 하단 관문이다.                                                      */
/* -------------------------------------------------------------------------- */
uint8_t BSP_SD_ReadBlocks(uint32_t *pData,
                          uint32_t ReadAddr,
                          uint32_t NumOfBlocks,
                          uint32_t Timeout)
{
    uint8_t *dst;

    if ((pData == 0) || (NumOfBlocks == 0u))
    {
        return MSD_ERROR;
    }

    dst = (uint8_t *)pData;

    if (APP_SD_BSP_CanUseDirectTransferBuffer(dst) != 0u)
    {
        return APP_SD_BSP_ReadBlocksDirect(dst,
                                           ReadAddr,
                                           NumOfBlocks,
                                           Timeout);
    }

    return APP_SD_BSP_ReadBlocksScratch(dst,
                                        ReadAddr,
                                        NumOfBlocks,
                                        Timeout);
}

/* -------------------------------------------------------------------------- */
/*  strong override: BSP_SD_WriteBlocks                                        */
/*                                                                            */
/*  generated weak symbol 을 대체한다.                                          */
/*  audio debug bring-up 단계에서는 read 경로가 더 중요하지만,                 */
/*  write 경로도 동일한 종류의 misaligned source 문제를                        */
/*  만들 수 있으므로 같이 정리해 둔다.                                         */
/* -------------------------------------------------------------------------- */
uint8_t BSP_SD_WriteBlocks(uint32_t *pData,
                           uint32_t WriteAddr,
                           uint32_t NumOfBlocks,
                           uint32_t Timeout)
{
    const uint8_t *src;

    if ((pData == 0) || (NumOfBlocks == 0u))
    {
        return MSD_ERROR;
    }

    src = (const uint8_t *)pData;

    /* ---------------------------------------------------------------------- */
    /*  write 경로는 의도적으로 항상 scratch-buffer path 를 탄다.                */
    /*                                                                        */
    /*  판단 근거                                                              */
    /*  - read 쪽은 audio/WAV throughput 때문에 direct path 이점이 크다.        */
    /*  - 반면 write 쪽은 logger / metadata / settings 위주라 절대 성능보다      */
    /*    안정성이 우선이다.                                                   */
    /*  - 최근 증상은 "첫 logger write 에서 즉시 WRITE ERR" 였고,               */
    /*    이 경우 source 버퍼 위치/정렬/전송 폭을 하나씩 추측하기보다            */
    /*    일반 SRAM 512-byte scratch sector 하나만 하위 HAL 에 보여 주는 편이   */
    /*    훨씬 보수적이고 재현성도 좋다.                                       */
    /*                                                                        */
    /*  그래서                                                                  */
    /*  - 모든 write 는 일반 SRAM scratch buffer 로 1-sector staging 하고      */
    /*  - sector 마다 ready wait 를 확인한 뒤 HAL_SD_WriteBlocks 를 호출한다.   */
    /* ---------------------------------------------------------------------- */
    return APP_SD_BSP_WriteBlocksScratch(src,
                                         WriteAddr,
                                         NumOfBlocks,
                                         Timeout);
}
