#include "bsp_driver_sd.h"

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
    if (HAL_SD_ReadBlocks(&hsd, dst, read_addr, num_blocks, timeout_ms) != HAL_OK)
    {
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

    scratch = (uint8_t *)s_app_sd_scratch_sector_words;

    for (block_index = 0u; block_index < num_blocks; block_index++)
    {
        if (HAL_SD_ReadBlocks(&hsd,
                              scratch,
                              read_addr + block_index,
                              1u,
                              timeout_ms) != HAL_OK)
        {
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
/*  내부 유틸: direct multi-block write helper                                 */
/*                                                                            */
/*  aligned source 인 경우에는                                                 */
/*  generated weak implementation과 동일한 contract 를 유지한다.               */
/*  - HAL_SD_WriteBlocks 를 한 번 호출한다.                                    */
/*  - 완료 대기는 caller(sd_diskio.c)가 수행한다.                              */
/* -------------------------------------------------------------------------- */
static uint8_t APP_SD_BSP_WriteBlocksDirect(const uint8_t *src,
                                            uint32_t write_addr,
                                            uint32_t num_blocks,
                                            uint32_t timeout_ms)
{
    if (HAL_SD_WriteBlocks(&hsd, (uint8_t *)src, write_addr, num_blocks, timeout_ms) != HAL_OK)
    {
        return MSD_ERROR;
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

    scratch = (uint8_t *)s_app_sd_scratch_sector_words;

    for (block_index = 0u; block_index < num_blocks; block_index++)
    {
        memcpy(scratch,
               &src[block_index * APP_SD_BSP_SECTOR_BYTES],
               APP_SD_BSP_SECTOR_BYTES);

        if (HAL_SD_WriteBlocks(&hsd,
                               scratch,
                               write_addr + block_index,
                               1u,
                               timeout_ms) != HAL_OK)
        {
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

    if (BSP_SD_IsDetected() != SD_PRESENT)
    {
        return MSD_ERROR;
    }

    dst = (uint8_t *)pData;

    if (APP_SD_BSP_IsWordAligned(dst) != 0u)
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

    if (BSP_SD_IsDetected() != SD_PRESENT)
    {
        return MSD_ERROR;
    }

    src = (const uint8_t *)pData;

    if (APP_SD_BSP_IsWordAligned(src) != 0u)
    {
        return APP_SD_BSP_WriteBlocksDirect(src,
                                            WriteAddr,
                                            NumOfBlocks,
                                            Timeout);
    }

    return APP_SD_BSP_WriteBlocksScratch(src,
                                         WriteAddr,
                                         NumOfBlocks,
                                         Timeout);
}
