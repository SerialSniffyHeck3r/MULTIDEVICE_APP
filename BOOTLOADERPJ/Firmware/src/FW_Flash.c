#include "FW_Flash.h"

#include "SPI_Flash.h"
#include "FW_BootConfig.h"
#include "FW_Crc32.h"

#include <string.h>

/* -------------------------------------------------------------------------- */
/*  내부 helper: address -> flash sector                                       */
/* -------------------------------------------------------------------------- */
static uint32_t FW_FLASH_AddressToSector(uint32_t address)
{
    if (address < 0x08004000u) { return FLASH_SECTOR_0; }
    if (address < 0x08008000u) { return FLASH_SECTOR_1; }
    if (address < 0x0800C000u) { return FLASH_SECTOR_2; }
    if (address < 0x08010000u) { return FLASH_SECTOR_3; }
    if (address < 0x08020000u) { return FLASH_SECTOR_4; }
    if (address < 0x08040000u) { return FLASH_SECTOR_5; }
    if (address < 0x08060000u) { return FLASH_SECTOR_6; }
    if (address < 0x08080000u) { return FLASH_SECTOR_7; }
    if (address < 0x080A0000u) { return FLASH_SECTOR_8; }
    if (address < 0x080C0000u) { return FLASH_SECTOR_9; }
    if (address < 0x080E0000u) { return FLASH_SECTOR_10; }
    return FLASH_SECTOR_11;
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: progress callback safe call                                   */
/* -------------------------------------------------------------------------- */
static void FW_FLASH_ReportProgress(fw_flash_progress_cb_t progress_cb,
                                    fw_flash_stage_t stage,
                                    uint32_t completed_units,
                                    uint32_t total_units,
                                    void *user_context)
{
    if (progress_cb != 0)
    {
        progress_cb(stage, completed_units, total_units, user_context);
    }
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: app 영역 전체 erase                                           */
/*                                                                            */
/*  단순화를 위해 app sector 5~11 전체를 지운다.                               */
/*  이렇게 하면 "작아진 새 app" 뒤에 남는 old tail 문제를 깔끔하게 없앤다.    */
/* -------------------------------------------------------------------------- */
static HAL_StatusTypeDef FW_FLASH_EraseWholeAppRegion(fw_flash_progress_cb_t progress_cb,
                                                      void *user_context)
{
    HAL_StatusTypeDef status;
    FLASH_EraseInitTypeDef erase_init;
    uint32_t sector_error;
    uint32_t sector_index;
    uint32_t sector_number;

    status = HAL_OK;
    sector_error = 0u;

    for (sector_index = 0u; sector_index < 7u; sector_index++)
    {
        sector_number = FLASH_SECTOR_5 + sector_index;

        memset(&erase_init, 0, sizeof(erase_init));
        erase_init.TypeErase    = FLASH_TYPEERASE_SECTORS;
        erase_init.VoltageRange = FLASH_VOLTAGE_RANGE_3;
        erase_init.Sector       = sector_number;
        erase_init.NbSectors    = 1u;

        status = HAL_FLASHEx_Erase(&erase_init, &sector_error);
        if (status != HAL_OK)
        {
            return status;
        }

        FW_FLASH_ReportProgress(progress_cb,
                                FW_FLASH_STAGE_APP_ERASE,
                                sector_index + 1u,
                                7u,
                                user_context);
    }

    return HAL_OK;
}

/* -------------------------------------------------------------------------- */
/*  내부 helper: word program                                                  */
/* -------------------------------------------------------------------------- */
static HAL_StatusTypeDef FW_FLASH_ProgramChunk(uint32_t flash_address,
                                               const uint8_t *data,
                                               uint32_t length_bytes)
{
    HAL_StatusTypeDef status;
    uint8_t padded_buffer[1024u + 4u];
    uint32_t padded_length;
    uint32_t offset;
    uint32_t word_value;

    if ((data == 0) || (length_bytes == 0u))
    {
        return HAL_ERROR;
    }

    if (length_bytes > 1024u)
    {
        return HAL_ERROR;
    }

    padded_length = (length_bytes + 3u) & ~3u;

    memset(padded_buffer, 0xFF, sizeof(padded_buffer));
    memcpy(padded_buffer, data, length_bytes);

    status = HAL_OK;

    for (offset = 0u; offset < padded_length; offset += 4u)
    {
        word_value = ((uint32_t)padded_buffer[offset + 0u] << 0u) |
                     ((uint32_t)padded_buffer[offset + 1u] << 8u) |
                     ((uint32_t)padded_buffer[offset + 2u] << 16u) |
                     ((uint32_t)padded_buffer[offset + 3u] << 24u);

        status = HAL_FLASH_Program(FLASH_TYPEPROGRAM_WORD,
                                   flash_address + offset,
                                   word_value);
        if (status != HAL_OK)
        {
            return status;
        }
    }

    if (memcmp((const void *)flash_address, data, length_bytes) != 0)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: app vector sanity check                                          */
/* -------------------------------------------------------------------------- */
bool FW_FLASH_IsAppVectorValid(void)
{
    uint32_t app_sp;
    uint32_t app_pc;
    uint8_t sp_ok;
    uint8_t pc_ok;

    app_sp = *(const uint32_t *)(FW_APP_BASE_ADDRESS + 0u);
    app_pc = *(const uint32_t *)(FW_APP_BASE_ADDRESS + 4u);

    if ((app_sp == 0xFFFFFFFFu) || (app_pc == 0xFFFFFFFFu))
    {
        return false;
    }

    sp_ok = 0u;
    if ((app_sp >= 0x20000000u) && (app_sp < 0x20020000u))
    {
        sp_ok = 1u;
    }
    if ((app_sp >= 0x10000000u) && (app_sp < 0x10010000u))
    {
        sp_ok = 1u;
    }

    pc_ok = 0u;
    if (((app_pc & 1u) != 0u) &&
        (app_pc >= FW_APP_BASE_ADDRESS) &&
        (app_pc < FW_FLASH_END_ADDRESS))
    {
        pc_ok = 1u;
    }

    return ((sp_ok != 0u) && (pc_ok != 0u)) ? true : false;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: boot -> app jump                                                 */
/* -------------------------------------------------------------------------- */
void FW_FLASH_JumpToApp(void)
{
    uint32_t app_sp;
    uint32_t app_pc;
    uint32_t index;
    void (*app_reset_handler)(void);

    app_sp = *(const uint32_t *)(FW_APP_BASE_ADDRESS + 0u);
    app_pc = *(const uint32_t *)(FW_APP_BASE_ADDRESS + 4u);
    app_reset_handler = (void (*)(void))app_pc;

    __disable_irq();

    HAL_DeInit();

    SysTick->CTRL = 0u;
    SysTick->LOAD = 0u;
    SysTick->VAL  = 0u;

    for (index = 0u; index < 8u; index++)
    {
        NVIC->ICER[index] = 0xFFFFFFFFu;
        NVIC->ICPR[index] = 0xFFFFFFFFu;
    }

    SCB->VTOR = FW_APP_BASE_ADDRESS;
    __DSB();
    __ISB();

    __set_MSP(app_sp);
    app_reset_handler();

    for (;;)
    {
        __NOP();
    }
}

/* -------------------------------------------------------------------------- */
/*  공개 API: W25Q16 전체 erase                                                */
/*                                                                            */
/*  SPI_Flash_EraseChip() 의 단일 timeout에 의존하지 않고,                     */
/*  4KB sector erase를 전부 순차 실행한다.                                     */
/*  이렇게 하면 progress bar 갱신도 쉽고, 장시간 block도 더 예측 가능하다.    */
/* -------------------------------------------------------------------------- */
HAL_StatusTypeDef FW_FLASH_EraseW25Q16All(fw_flash_progress_cb_t progress_cb,
                                          void *user_context)
{
    HAL_StatusTypeDef status;
    spi_flash_snapshot_t snapshot;
    uint32_t address;
    uint32_t sector_index;
    uint32_t sector_count;

    SPI_Flash_Init();
    SPI_Flash_CopySnapshot(&snapshot);

    if ((snapshot.initialized == false) || (snapshot.present == false))
    {
        return HAL_ERROR;
    }

    sector_count = FW_W25Q16_CAPACITY_BYTES / SPI_FLASH_SECTOR_SIZE_4K;
    status = HAL_OK;

    for (sector_index = 0u; sector_index < sector_count; sector_index++)
    {
        address = sector_index * SPI_FLASH_SECTOR_SIZE_4K;

        status = SPI_Flash_EraseSector4K(address);
        if (status != HAL_OK)
        {
            return status;
        }

        FW_FLASH_ReportProgress(progress_cb,
                                FW_FLASH_STAGE_W25Q_ERASE,
                                sector_index + 1u,
                                sector_count,
                                user_context);
    }

    return HAL_OK;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: SYSUPDATE.bin payload -> app 영역 install                        */
/* -------------------------------------------------------------------------- */
HAL_StatusTypeDef FW_FLASH_InstallFromOpenFile(FIL *fp,
                                               const fw_package_header_t *header,
                                               fw_flash_progress_cb_t progress_cb,
                                               void *user_context)
{
    HAL_StatusTypeDef status;
    uint8_t chunk_buffer[1024u];
    UINT br;
    uint32_t flash_address;
    uint32_t remaining_bytes;
    uint32_t written_bytes;
    uint32_t verify_crc;

    if ((fp == 0) || (header == 0))
    {
        return HAL_ERROR;
    }

    if (FW_Package_IsHeaderValid(header) == false)
    {
        return HAL_ERROR;
    }

    status = HAL_FLASH_Unlock();
    if (status != HAL_OK)
    {
        return status;
    }

#if defined(__HAL_FLASH_CLEAR_FLAG)
    __HAL_FLASH_CLEAR_FLAG(FLASH_FLAG_EOP |
                           FLASH_FLAG_OPERR |
                           FLASH_FLAG_WRPERR |
                           FLASH_FLAG_PGAERR |
                           FLASH_FLAG_PGPERR |
                           FLASH_FLAG_PGSERR);
#endif

    status = FW_FLASH_EraseWholeAppRegion(progress_cb, user_context);
    if (status != HAL_OK)
    {
        (void)HAL_FLASH_Lock();
        return status;
    }

    flash_address   = FW_APP_BASE_ADDRESS;
    remaining_bytes = header->payload_size_bytes;
    written_bytes   = 0u;

    while (remaining_bytes > 0u)
    {
        uint32_t chunk_size;

        chunk_size = (remaining_bytes > sizeof(chunk_buffer)) ? sizeof(chunk_buffer) : remaining_bytes;
        br = 0u;

        if (f_read(fp, chunk_buffer, chunk_size, &br) != FR_OK)
        {
            (void)HAL_FLASH_Lock();
            return HAL_ERROR;
        }

        if (br != chunk_size)
        {
            (void)HAL_FLASH_Lock();
            return HAL_ERROR;
        }

        status = FW_FLASH_ProgramChunk(flash_address, chunk_buffer, chunk_size);
        if (status != HAL_OK)
        {
            (void)HAL_FLASH_Lock();
            return status;
        }

        flash_address   += ((chunk_size + 3u) & ~3u);
        remaining_bytes -= chunk_size;
        written_bytes   += chunk_size;

        FW_FLASH_ReportProgress(progress_cb,
                                FW_FLASH_STAGE_APP_PROGRAM,
                                written_bytes,
                                header->payload_size_bytes,
                                user_context);
    }

    verify_crc = FW_CRC32_Calc((const void *)FW_APP_BASE_ADDRESS,
                               header->payload_size_bytes);

    FW_FLASH_ReportProgress(progress_cb,
                            FW_FLASH_STAGE_APP_VERIFY,
                            1u,
                            1u,
                            user_context);

    (void)HAL_FLASH_Lock();

    if (verify_crc != header->payload_crc32)
    {
        return HAL_ERROR;
    }

    return HAL_OK;
}
