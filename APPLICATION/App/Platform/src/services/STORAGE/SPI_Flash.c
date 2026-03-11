#include "SPI_Flash.h"

#include <string.h>

/* -------------------------------------------------------------------------- */
/*  외부 SPI handle                                                            */
/* -------------------------------------------------------------------------- */
extern SPI_HandleTypeDef SPI_FLASH_SPI_HANDLE;

/* -------------------------------------------------------------------------- */
/*  드라이버 내부 runtime 상태                                                 */
/* -------------------------------------------------------------------------- */
typedef struct
{
    spi_flash_snapshot_t snapshot;
    uint32_t next_status_refresh_ms;
    uint32_t operation_deadline_ms;
    uint8_t  verify_buffer[SPI_FLASH_TEST_LENGTH];
} spi_flash_runtime_t;

static spi_flash_runtime_t s_spi_flash_rt;

/* -------------------------------------------------------------------------- */
/*  시간 helper                                                                */
/* -------------------------------------------------------------------------- */
static uint8_t SPI_Flash_TimeDue(uint32_t now_ms, uint32_t due_ms)
{
    return ((int32_t)(now_ms - due_ms) >= 0) ? 1u : 0u;
}

/* -------------------------------------------------------------------------- */
/*  GPIO helper                                                                */
/* -------------------------------------------------------------------------- */
static void SPI_Flash_EnablePortClock(GPIO_TypeDef *port)
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

static void SPI_Flash_ConfigureCsPinRuntime(void)
{
    GPIO_InitTypeDef gpio_init;

    memset(&gpio_init, 0, sizeof(gpio_init));

    SPI_Flash_EnablePortClock(SPI_FLASH_CS_GPIO_Port);

    gpio_init.Pin   = SPI_FLASH_CS_Pin;
    gpio_init.Mode  = GPIO_MODE_OUTPUT_PP;
    gpio_init.Pull  = GPIO_PULLUP;
    gpio_init.Speed = GPIO_SPEED_FREQ_VERY_HIGH;
    HAL_GPIO_Init(SPI_FLASH_CS_GPIO_Port, &gpio_init);

    /* ---------------------------------------------------------------------- */
    /*  CS는 idle-high 가 기본이다.                                            */
    /*  보드 초기화 중 떠 있는 순간을 줄이기 위해 runtime 에서도 다시 올린다.  */
    /* ---------------------------------------------------------------------- */
    HAL_GPIO_WritePin(SPI_FLASH_CS_GPIO_Port, SPI_FLASH_CS_Pin, GPIO_PIN_SET);
}

static void SPI_Flash_Select(void)
{
    HAL_GPIO_WritePin(SPI_FLASH_CS_GPIO_Port, SPI_FLASH_CS_Pin, GPIO_PIN_RESET);
}

static void SPI_Flash_Deselect(void)
{
    HAL_GPIO_WritePin(SPI_FLASH_CS_GPIO_Port, SPI_FLASH_CS_Pin, GPIO_PIN_SET);
}

/* -------------------------------------------------------------------------- */
/*  SPI chunk transfer helper                                                  */
/* -------------------------------------------------------------------------- */
static HAL_StatusTypeDef SPI_Flash_Transmit(const uint8_t *data, uint32_t length)
{
    HAL_StatusTypeDef status;
    uint32_t offset;
    uint16_t chunk_length;

    offset = 0u;
    status = HAL_OK;

    while (offset < length)
    {
        chunk_length = (uint16_t)((length - offset) > 65535u ? 65535u : (length - offset));

        status = HAL_SPI_Transmit(&SPI_FLASH_SPI_HANDLE,
                                  (uint8_t *)&data[offset],
                                  chunk_length,
                                  SPI_FLASH_TIMEOUT_MS);
        if (status != HAL_OK)
        {
            return status;
        }

        offset += chunk_length;
    }

    return status;
}

static HAL_StatusTypeDef SPI_Flash_Receive(uint8_t *data, uint32_t length)
{
    HAL_StatusTypeDef status;
    uint32_t offset;
    uint16_t chunk_length;

    offset = 0u;
    status = HAL_OK;

    while (offset < length)
    {
        chunk_length = (uint16_t)((length - offset) > 65535u ? 65535u : (length - offset));

        status = HAL_SPI_Receive(&SPI_FLASH_SPI_HANDLE,
                                 &data[offset],
                                 chunk_length,
                                 SPI_FLASH_TIMEOUT_MS);
        if (status != HAL_OK)
        {
            return status;
        }

        offset += chunk_length;
    }

    return status;
}

static HAL_StatusTypeDef SPI_Flash_TransmitZeros(uint32_t length)
{
    uint8_t zero_buffer[16];
    HAL_StatusTypeDef status;
    uint32_t remaining;
    uint16_t chunk_length;

    memset(zero_buffer, 0, sizeof(zero_buffer));

    status = HAL_OK;
    remaining = length;

    while (remaining > 0u)
    {
        chunk_length = (uint16_t)((remaining > sizeof(zero_buffer)) ? sizeof(zero_buffer) : remaining);
        status = HAL_SPI_Transmit(&SPI_FLASH_SPI_HANDLE,
                                  zero_buffer,
                                  chunk_length,
                                  SPI_FLASH_TIMEOUT_MS);
        if (status != HAL_OK)
        {
            return status;
        }

        remaining -= chunk_length;
    }

    return status;
}

/* -------------------------------------------------------------------------- */
/*  snapshot helper                                                            */
/* -------------------------------------------------------------------------- */
static void SPI_Flash_SetLastTransferMeta(uint8_t opcode, HAL_StatusTypeDef status)
{
    s_spi_flash_rt.snapshot.last_opcode     = opcode;
    s_spi_flash_rt.snapshot.last_hal_status = (uint8_t)status;

    if (status != HAL_OK)
    {
        s_spi_flash_rt.snapshot.error_count++;
        s_spi_flash_rt.snapshot.last_result = (uint8_t)SPI_FLASH_RESULT_HAL_ERROR;
    }
}

static uint32_t SPI_Flash_CapacityCodeToBytes(uint8_t capacity_id)
{
    if ((capacity_id < 0x10u) || (capacity_id > 0x20u))
    {
        return 0u;
    }

    return (uint32_t)1u << capacity_id;
}

static uint32_t SPI_Flash_GetRecommendedTestAddress(uint32_t capacity_bytes)
{
    if (capacity_bytes >= SPI_FLASH_SECTOR_SIZE_4K)
    {
        return (capacity_bytes - SPI_FLASH_SECTOR_SIZE_4K);
    }

    return 0u;
}

static void SPI_Flash_UpdatePartName(void)
{
    spi_flash_snapshot_t *snapshot;

    snapshot = &s_spi_flash_rt.snapshot;

    memset(snapshot->part_name, 0, sizeof(snapshot->part_name));

    if (snapshot->present == false)
    {
        strncpy(snapshot->part_name, "NO_FLASH", sizeof(snapshot->part_name) - 1u);
        return;
    }

    if ((snapshot->manufacturer_id == 0xEFu) && (snapshot->capacity_id == 0x15u))
    {
        strncpy(snapshot->part_name, "W25Q16-compatible", sizeof(snapshot->part_name) - 1u);
        snapshot->w25q16bv_compatible = true;
        return;
    }

    if ((snapshot->manufacturer_id == 0xEFu) && (snapshot->capacity_id == 0x16u))
    {
        strncpy(snapshot->part_name, "W25Q32-compatible", sizeof(snapshot->part_name) - 1u);
        snapshot->w25q16bv_compatible = false;
        return;
    }

    if ((snapshot->manufacturer_id == 0xEFu) && (snapshot->capacity_id == 0x17u))
    {
        strncpy(snapshot->part_name, "W25Q64-compatible", sizeof(snapshot->part_name) - 1u);
        snapshot->w25q16bv_compatible = false;
        return;
    }

    if ((snapshot->manufacturer_id == 0xEFu) && (snapshot->capacity_id == 0x18u))
    {
        strncpy(snapshot->part_name, "W25Q128-compatible", sizeof(snapshot->part_name) - 1u);
        snapshot->w25q16bv_compatible = false;
        return;
    }

    strncpy(snapshot->part_name, "JEDEC-compatible", sizeof(snapshot->part_name) - 1u);
    snapshot->w25q16bv_compatible = false;
}

/* -------------------------------------------------------------------------- */
/*  raw command API                                                            */
/* -------------------------------------------------------------------------- */
HAL_StatusTypeDef SPI_Flash_SendRawCommand(uint8_t opcode)
{
    HAL_StatusTypeDef status;

    SPI_Flash_Select();
    status = SPI_Flash_Transmit(&opcode, 1u);
    SPI_Flash_Deselect();

    s_spi_flash_rt.snapshot.command_count++;
    SPI_Flash_SetLastTransferMeta(opcode, status);

    return status;
}

HAL_StatusTypeDef SPI_Flash_SendRawCommandTx(uint8_t opcode,
                                             const uint8_t *tx_data,
                                             uint32_t tx_length)
{
    HAL_StatusTypeDef status;

    SPI_Flash_Select();

    status = SPI_Flash_Transmit(&opcode, 1u);
    if ((status == HAL_OK) && (tx_length > 0u) && (tx_data != 0))
    {
        status = SPI_Flash_Transmit(tx_data, tx_length);
    }

    SPI_Flash_Deselect();

    s_spi_flash_rt.snapshot.command_count++;
    SPI_Flash_SetLastTransferMeta(opcode, status);

    return status;
}

HAL_StatusTypeDef SPI_Flash_SendRawCommandRx(uint8_t opcode,
                                             uint8_t *rx_data,
                                             uint32_t rx_length)
{
    HAL_StatusTypeDef status;

    SPI_Flash_Select();

    status = SPI_Flash_Transmit(&opcode, 1u);
    if ((status == HAL_OK) && (rx_length > 0u) && (rx_data != 0))
    {
        status = SPI_Flash_Receive(rx_data, rx_length);
    }

    SPI_Flash_Deselect();

    s_spi_flash_rt.snapshot.command_count++;
    SPI_Flash_SetLastTransferMeta(opcode, status);

    return status;
}

HAL_StatusTypeDef SPI_Flash_SendRawCommandDummyRx(uint8_t opcode,
                                                  uint8_t dummy_bytes,
                                                  uint8_t *rx_data,
                                                  uint32_t rx_length)
{
    HAL_StatusTypeDef status;

    SPI_Flash_Select();

    status = SPI_Flash_Transmit(&opcode, 1u);
    if ((status == HAL_OK) && (dummy_bytes > 0u))
    {
        status = SPI_Flash_TransmitZeros(dummy_bytes);
    }
    if ((status == HAL_OK) && (rx_length > 0u) && (rx_data != 0))
    {
        status = SPI_Flash_Receive(rx_data, rx_length);
    }

    SPI_Flash_Deselect();

    s_spi_flash_rt.snapshot.command_count++;
    SPI_Flash_SetLastTransferMeta(opcode, status);

    return status;
}

HAL_StatusTypeDef SPI_Flash_SendRawCommandAddressTx(uint8_t opcode,
                                                    uint32_t address,
                                                    uint8_t address_bytes,
                                                    const uint8_t *tx_data,
                                                    uint32_t tx_length)
{
    HAL_StatusTypeDef status;
    uint8_t header[5];
    uint8_t header_length;

    if ((address_bytes < 3u) || (address_bytes > 4u))
    {
        return HAL_ERROR;
    }

    header[0] = opcode;
    if (address_bytes == 4u)
    {
        header[1] = (uint8_t)((address >> 24) & 0xFFu);
        header[2] = (uint8_t)((address >> 16) & 0xFFu);
        header[3] = (uint8_t)((address >> 8)  & 0xFFu);
        header[4] = (uint8_t)( address        & 0xFFu);
        header_length = 5u;
    }
    else
    {
        header[1] = (uint8_t)((address >> 16) & 0xFFu);
        header[2] = (uint8_t)((address >> 8)  & 0xFFu);
        header[3] = (uint8_t)( address        & 0xFFu);
        header_length = 4u;
    }

    SPI_Flash_Select();

    status = SPI_Flash_Transmit(header, header_length);
    if ((status == HAL_OK) && (tx_length > 0u) && (tx_data != 0))
    {
        status = SPI_Flash_Transmit(tx_data, tx_length);
    }

    SPI_Flash_Deselect();

    s_spi_flash_rt.snapshot.command_count++;
    SPI_Flash_SetLastTransferMeta(opcode, status);

    return status;
}

HAL_StatusTypeDef SPI_Flash_SendRawCommandAddressRx(uint8_t opcode,
                                                    uint32_t address,
                                                    uint8_t address_bytes,
                                                    uint8_t dummy_bytes,
                                                    uint8_t *rx_data,
                                                    uint32_t rx_length)
{
    HAL_StatusTypeDef status;
    uint8_t header[5];
    uint8_t header_length;

    if ((address_bytes < 3u) || (address_bytes > 4u))
    {
        return HAL_ERROR;
    }

    header[0] = opcode;
    if (address_bytes == 4u)
    {
        header[1] = (uint8_t)((address >> 24) & 0xFFu);
        header[2] = (uint8_t)((address >> 16) & 0xFFu);
        header[3] = (uint8_t)((address >> 8)  & 0xFFu);
        header[4] = (uint8_t)( address        & 0xFFu);
        header_length = 5u;
    }
    else
    {
        header[1] = (uint8_t)((address >> 16) & 0xFFu);
        header[2] = (uint8_t)((address >> 8)  & 0xFFu);
        header[3] = (uint8_t)( address        & 0xFFu);
        header_length = 4u;
    }

    SPI_Flash_Select();

    status = SPI_Flash_Transmit(header, header_length);
    if ((status == HAL_OK) && (dummy_bytes > 0u))
    {
        status = SPI_Flash_TransmitZeros(dummy_bytes);
    }
    if ((status == HAL_OK) && (rx_length > 0u) && (rx_data != 0))
    {
        status = SPI_Flash_Receive(rx_data, rx_length);
    }

    SPI_Flash_Deselect();

    s_spi_flash_rt.snapshot.command_count++;
    SPI_Flash_SetLastTransferMeta(opcode, status);

    return status;
}

/* -------------------------------------------------------------------------- */
/*  high-level helper                                                          */
/* -------------------------------------------------------------------------- */
static HAL_StatusTypeDef SPI_Flash_WriteEnable(void)
{
    return SPI_Flash_SendRawCommand(SPI_FLASH_CMD_WRITE_ENABLE);
}

static HAL_StatusTypeDef SPI_Flash_WriteDisable(void)
{
    return SPI_Flash_SendRawCommand(SPI_FLASH_CMD_WRITE_DISABLE);
}

HAL_StatusTypeDef SPI_Flash_ResetDevice(void)
{
    HAL_StatusTypeDef status;

    status = SPI_Flash_SendRawCommand(SPI_FLASH_CMD_ENABLE_RESET);
    if (status != HAL_OK)
    {
        return status;
    }

    status = SPI_Flash_SendRawCommand(SPI_FLASH_CMD_RESET_DEVICE);
    return status;
}

HAL_StatusTypeDef SPI_Flash_ReadJedecId(uint8_t *manufacturer_id,
                                        uint8_t *memory_type,
                                        uint8_t *capacity_id)
{
    HAL_StatusTypeDef status;
    uint8_t id[3];

    memset(id, 0, sizeof(id));
    status = SPI_Flash_SendRawCommandRx(SPI_FLASH_CMD_JEDEC_ID, id, sizeof(id));
    if (status != HAL_OK)
    {
        return status;
    }

    if (manufacturer_id != 0)
    {
        *manufacturer_id = id[0];
    }
    if (memory_type != 0)
    {
        *memory_type = id[1];
    }
    if (capacity_id != 0)
    {
        *capacity_id = id[2];
    }

    return HAL_OK;
}

HAL_StatusTypeDef SPI_Flash_ReadManufacturerDeviceId(uint16_t *manufacturer_device_id)
{
    HAL_StatusTypeDef status;
    uint8_t rx[2];
    uint8_t tx[3];

    if (manufacturer_device_id == 0)
    {
        return HAL_ERROR;
    }

    tx[0] = 0u;
    tx[1] = 0u;
    tx[2] = 0u;

    SPI_Flash_Select();
    status = SPI_Flash_Transmit((const uint8_t[]){ SPI_FLASH_CMD_MANUFACTURER_DEVICE_ID }, 1u);
    if (status == HAL_OK)
    {
        status = SPI_Flash_Transmit(tx, sizeof(tx));
    }
    if (status == HAL_OK)
    {
        status = SPI_Flash_Receive(rx, sizeof(rx));
    }
    SPI_Flash_Deselect();

    s_spi_flash_rt.snapshot.command_count++;
    SPI_Flash_SetLastTransferMeta(SPI_FLASH_CMD_MANUFACTURER_DEVICE_ID, status);

    if (status != HAL_OK)
    {
        return status;
    }

    *manufacturer_device_id = (uint16_t)(((uint16_t)rx[0] << 8) | rx[1]);
    return HAL_OK;
}

HAL_StatusTypeDef SPI_Flash_ReadUniqueId(uint8_t out_unique_id[8])
{
    if (out_unique_id == 0)
    {
        return HAL_ERROR;
    }

    return SPI_Flash_SendRawCommandDummyRx(SPI_FLASH_CMD_UNIQUE_ID,
                                           4u,
                                           out_unique_id,
                                           8u);
}

HAL_StatusTypeDef SPI_Flash_ReadSfdpHeader(uint8_t out_sfdp_header[8])
{
    if (out_sfdp_header == 0)
    {
        return HAL_ERROR;
    }

    return SPI_Flash_SendRawCommandAddressRx(SPI_FLASH_CMD_SFDP,
                                             0u,
                                             3u,
                                             1u,
                                             out_sfdp_header,
                                             8u);
}

HAL_StatusTypeDef SPI_Flash_ReadStatusRegisters(uint8_t *sr1,
                                                uint8_t *sr2,
                                                uint8_t *sr3)
{
    HAL_StatusTypeDef status;
    uint8_t value;

    status = HAL_OK;

    if (sr1 != 0)
    {
        value = 0u;
        status = SPI_Flash_SendRawCommandRx(SPI_FLASH_CMD_READ_STATUS_REG1, &value, 1u);
        if (status != HAL_OK)
        {
            return status;
        }
        *sr1 = value;
    }

    if (sr2 != 0)
    {
        value = 0u;
        status = SPI_Flash_SendRawCommandRx(SPI_FLASH_CMD_READ_STATUS_REG2, &value, 1u);
        if (status != HAL_OK)
        {
            return status;
        }
        *sr2 = value;
    }

    if (sr3 != 0)
    {
        value = 0u;
        status = SPI_Flash_SendRawCommandRx(SPI_FLASH_CMD_READ_STATUS_REG3, &value, 1u);
        if (status != HAL_OK)
        {
            return status;
        }
        *sr3 = value;
    }

    return HAL_OK;
}

static HAL_StatusTypeDef SPI_Flash_WaitReadyBlocking(uint32_t timeout_ms)
{
    HAL_StatusTypeDef status;
    uint32_t start_ms;
    uint8_t sr1;
    uint8_t sr2;
    uint8_t sr3;

    start_ms = HAL_GetTick();

    for (;;)
    {
        sr1 = 0u;
        sr2 = 0u;
        sr3 = 0u;

        status = SPI_Flash_ReadStatusRegisters(&sr1, &sr2, &sr3);
        if (status != HAL_OK)
        {
            return status;
        }

        s_spi_flash_rt.snapshot.status_reg1 = sr1;
        s_spi_flash_rt.snapshot.status_reg2 = sr2;
        s_spi_flash_rt.snapshot.status_reg3 = sr3;
        s_spi_flash_rt.snapshot.busy = ((sr1 & 0x01u) != 0u) ? true : false;
        s_spi_flash_rt.snapshot.write_enable_latch = ((sr1 & 0x02u) != 0u) ? true : false;

        if ((sr1 & 0x01u) == 0u)
        {
            return HAL_OK;
        }

        if ((HAL_GetTick() - start_ms) >= timeout_ms)
        {
            return HAL_TIMEOUT;
        }
    }
}

static HAL_StatusTypeDef SPI_Flash_StartEraseCommon(uint8_t opcode, uint32_t address)
{
    HAL_StatusTypeDef status;

    status = SPI_Flash_WriteEnable();
    if (status != HAL_OK)
    {
        return status;
    }

    return SPI_Flash_SendRawCommandAddressTx(opcode, address, 3u, 0, 0u);
}

static HAL_StatusTypeDef SPI_Flash_StartPageProgram(uint32_t address,
                                                    const uint8_t *buffer,
                                                    uint32_t length)
{
    HAL_StatusTypeDef status;

    if ((buffer == 0) || (length == 0u) || (length > SPI_FLASH_PAGE_SIZE))
    {
        return HAL_ERROR;
    }

    status = SPI_Flash_WriteEnable();
    if (status != HAL_OK)
    {
        return status;
    }

    return SPI_Flash_SendRawCommandAddressTx(SPI_FLASH_CMD_PAGE_PROGRAM,
                                             address,
                                             3u,
                                             buffer,
                                             length);
}

HAL_StatusTypeDef SPI_Flash_ReadBuffer(uint32_t address,
                                       uint8_t *buffer,
                                       uint32_t length)
{
    if ((buffer == 0) || (length == 0u))
    {
        return HAL_ERROR;
    }

    return SPI_Flash_SendRawCommandAddressRx(SPI_FLASH_CMD_READ_DATA,
                                             address,
                                             3u,
                                             0u,
                                             buffer,
                                             length);
}

HAL_StatusTypeDef SPI_Flash_WriteBuffer(uint32_t address,
                                        const uint8_t *buffer,
                                        uint32_t length)
{
    HAL_StatusTypeDef status;
    uint32_t remaining;
    uint32_t offset;
    uint32_t page_offset;
    uint32_t chunk_length;

    if ((buffer == 0) || (length == 0u))
    {
        return HAL_ERROR;
    }

    remaining = length;
    offset    = 0u;

    while (remaining > 0u)
    {
        page_offset = (address + offset) % SPI_FLASH_PAGE_SIZE;
        chunk_length = SPI_FLASH_PAGE_SIZE - page_offset;

        if (chunk_length > remaining)
        {
            chunk_length = remaining;
        }

        status = SPI_Flash_StartPageProgram(address + offset,
                                            &buffer[offset],
                                            chunk_length);
        if (status != HAL_OK)
        {
            return status;
        }

        status = SPI_Flash_WaitReadyBlocking(SPI_FLASH_PROGRAM_TIMEOUT_MS);
        if (status != HAL_OK)
        {
            return status;
        }

        offset    += chunk_length;
        remaining -= chunk_length;
    }

    return HAL_OK;
}

HAL_StatusTypeDef SPI_Flash_EraseSector4K(uint32_t address)
{
    HAL_StatusTypeDef status;

    status = SPI_Flash_StartEraseCommon(SPI_FLASH_CMD_SECTOR_ERASE_4K, address);
    if (status != HAL_OK)
    {
        return status;
    }

    return SPI_Flash_WaitReadyBlocking(SPI_FLASH_ERASE_TIMEOUT_MS);
}

HAL_StatusTypeDef SPI_Flash_EraseBlock32K(uint32_t address)
{
    HAL_StatusTypeDef status;

    status = SPI_Flash_StartEraseCommon(SPI_FLASH_CMD_BLOCK_ERASE_32K, address);
    if (status != HAL_OK)
    {
        return status;
    }

    return SPI_Flash_WaitReadyBlocking(SPI_FLASH_ERASE_TIMEOUT_MS * 4u);
}

HAL_StatusTypeDef SPI_Flash_EraseBlock64K(uint32_t address)
{
    HAL_StatusTypeDef status;

    status = SPI_Flash_StartEraseCommon(SPI_FLASH_CMD_BLOCK_ERASE_64K, address);
    if (status != HAL_OK)
    {
        return status;
    }

    return SPI_Flash_WaitReadyBlocking(SPI_FLASH_ERASE_TIMEOUT_MS * 8u);
}

HAL_StatusTypeDef SPI_Flash_EraseChip(void)
{
    HAL_StatusTypeDef status;

    status = SPI_Flash_WriteEnable();
    if (status != HAL_OK)
    {
        return status;
    }

    status = SPI_Flash_SendRawCommand(SPI_FLASH_CMD_CHIP_ERASE_C7);
    if (status != HAL_OK)
    {
        return status;
    }

    return SPI_Flash_WaitReadyBlocking(SPI_FLASH_ERASE_TIMEOUT_MS * 16u);
}

/* -------------------------------------------------------------------------- */
/*  live refresh helper                                                        */
/* -------------------------------------------------------------------------- */
static void SPI_Flash_RefreshIdentity(void)
{
    HAL_StatusTypeDef status;
    uint8_t manufacturer_id;
    uint8_t memory_type;
    uint8_t capacity_id;
    uint16_t manufacturer_device_id;
    uint8_t uid[8];
    uint8_t sfdp[8];

    manufacturer_id = 0u;
    memory_type = 0u;
    capacity_id = 0u;
    manufacturer_device_id = 0u;
    memset(uid, 0, sizeof(uid));
    memset(sfdp, 0, sizeof(sfdp));

    status = SPI_Flash_ReadJedecId(&manufacturer_id, &memory_type, &capacity_id);
    if (status == HAL_OK)
    {
        s_spi_flash_rt.snapshot.manufacturer_id = manufacturer_id;
        s_spi_flash_rt.snapshot.memory_type     = memory_type;
        s_spi_flash_rt.snapshot.capacity_id     = capacity_id;
        s_spi_flash_rt.snapshot.capacity_bytes  = SPI_Flash_CapacityCodeToBytes(capacity_id);
        s_spi_flash_rt.snapshot.present =
            ((manufacturer_id != 0x00u) && (manufacturer_id != 0xFFu)) ? true : false;
    }
    else
    {
        s_spi_flash_rt.snapshot.present = false;
        return;
    }

    status = SPI_Flash_ReadManufacturerDeviceId(&manufacturer_device_id);
    if (status == HAL_OK)
    {
        s_spi_flash_rt.snapshot.manufacturer_device_id = manufacturer_device_id;
    }

    status = SPI_Flash_ReadUniqueId(uid);
    if (status == HAL_OK)
    {
        memcpy(s_spi_flash_rt.snapshot.unique_id, uid, sizeof(uid));
    }

    status = SPI_Flash_ReadSfdpHeader(sfdp);
    if (status == HAL_OK)
    {
        memcpy(s_spi_flash_rt.snapshot.sfdp_header, sfdp, sizeof(sfdp));
    }

    s_spi_flash_rt.snapshot.test_address = SPI_Flash_GetRecommendedTestAddress(s_spi_flash_rt.snapshot.capacity_bytes);
    SPI_Flash_UpdatePartName();
}

static void SPI_Flash_RefreshLiveStatus(uint32_t now_ms)
{
    HAL_StatusTypeDef status;
    uint8_t sr1;
    uint8_t sr2;
    uint8_t sr3;

    sr1 = 0u;
    sr2 = 0u;
    sr3 = 0u;

    status = SPI_Flash_ReadStatusRegisters(&sr1, &sr2, &sr3);
    if (status != HAL_OK)
    {
        return;
    }

    s_spi_flash_rt.snapshot.status_reg1 = sr1;
    s_spi_flash_rt.snapshot.status_reg2 = sr2;
    s_spi_flash_rt.snapshot.status_reg3 = sr3;
    s_spi_flash_rt.snapshot.busy = ((sr1 & 0x01u) != 0u) ? true : false;
    s_spi_flash_rt.snapshot.write_enable_latch = ((sr1 & 0x02u) != 0u) ? true : false;
    s_spi_flash_rt.snapshot.last_update_ms = now_ms;
}

/* -------------------------------------------------------------------------- */
/*  test helper                                                                */
/* -------------------------------------------------------------------------- */
static void SPI_Flash_FillWritePattern(void)
{
    uint32_t index;
    uint8_t seed;

    seed = (uint8_t)(s_spi_flash_rt.snapshot.write_count & 0xFFu);

    for (index = 0u; index < SPI_FLASH_TEST_LENGTH; index++)
    {
        s_spi_flash_rt.snapshot.last_write_data[index] = (uint8_t)(0xA0u + seed + index);
    }
}

static void SPI_Flash_SetTestError(spi_flash_result_t result)
{
    s_spi_flash_rt.snapshot.last_result = (uint8_t)result;
    s_spi_flash_rt.snapshot.test_state  = (uint8_t)SPI_FLASH_TEST_IDLE;
}

void SPI_Flash_RequestReadTest(void)
{
    if (s_spi_flash_rt.snapshot.present == false)
    {
        SPI_Flash_SetTestError(SPI_FLASH_RESULT_NOT_PRESENT);
        return;
    }

    if (s_spi_flash_rt.snapshot.test_state != (uint8_t)SPI_FLASH_TEST_IDLE)
    {
        s_spi_flash_rt.snapshot.last_result = (uint8_t)SPI_FLASH_RESULT_BUSY;
        return;
    }

    s_spi_flash_rt.snapshot.test_state   = (uint8_t)SPI_FLASH_TEST_READ_REQUESTED;
    s_spi_flash_rt.snapshot.last_result  = (uint8_t)SPI_FLASH_RESULT_NONE;
    s_spi_flash_rt.snapshot.last_test_length = SPI_FLASH_TEST_LENGTH;
}

void SPI_Flash_RequestWriteTest(void)
{
    if (s_spi_flash_rt.snapshot.present == false)
    {
        SPI_Flash_SetTestError(SPI_FLASH_RESULT_NOT_PRESENT);
        return;
    }

    if (s_spi_flash_rt.snapshot.test_state != (uint8_t)SPI_FLASH_TEST_IDLE)
    {
        s_spi_flash_rt.snapshot.last_result = (uint8_t)SPI_FLASH_RESULT_BUSY;
        return;
    }

    SPI_Flash_FillWritePattern();
    s_spi_flash_rt.snapshot.test_state      = (uint8_t)SPI_FLASH_TEST_WRITE_REQUESTED;
    s_spi_flash_rt.snapshot.last_result     = (uint8_t)SPI_FLASH_RESULT_NONE;
    s_spi_flash_rt.snapshot.last_test_length = SPI_FLASH_TEST_LENGTH;
}

static void SPI_Flash_ProcessReadTest(void)
{
    HAL_StatusTypeDef status;

    status = SPI_Flash_ReadBuffer(s_spi_flash_rt.snapshot.test_address,
                                  s_spi_flash_rt.snapshot.last_read_data,
                                  s_spi_flash_rt.snapshot.last_test_length);
    if (status != HAL_OK)
    {
        SPI_Flash_SetTestError(SPI_FLASH_RESULT_HAL_ERROR);
        return;
    }

    s_spi_flash_rt.snapshot.read_count++;
    s_spi_flash_rt.snapshot.last_result = (uint8_t)SPI_FLASH_RESULT_OK;
    s_spi_flash_rt.snapshot.test_state  = (uint8_t)SPI_FLASH_TEST_IDLE;
}

static void SPI_Flash_ProcessWriteRequest(uint32_t now_ms)
{
    HAL_StatusTypeDef status;

    status = SPI_Flash_StartEraseCommon(SPI_FLASH_CMD_SECTOR_ERASE_4K,
                                        s_spi_flash_rt.snapshot.test_address);
    if (status != HAL_OK)
    {
        SPI_Flash_SetTestError(SPI_FLASH_RESULT_HAL_ERROR);
        return;
    }

    s_spi_flash_rt.snapshot.erase_count++;
    s_spi_flash_rt.snapshot.last_result = (uint8_t)SPI_FLASH_RESULT_BUSY;
    s_spi_flash_rt.snapshot.test_state  = (uint8_t)SPI_FLASH_TEST_WAIT_ERASE_COMPLETE;
    s_spi_flash_rt.operation_deadline_ms = now_ms + SPI_FLASH_ERASE_TIMEOUT_MS;
}

static void SPI_Flash_ProcessWaitErase(uint32_t now_ms)
{
    HAL_StatusTypeDef status;

    if (s_spi_flash_rt.snapshot.busy != false)
    {
        if (SPI_Flash_TimeDue(now_ms, s_spi_flash_rt.operation_deadline_ms) != 0u)
        {
            SPI_Flash_SetTestError(SPI_FLASH_RESULT_TIMEOUT);
        }
        return;
    }

    status = SPI_Flash_StartPageProgram(s_spi_flash_rt.snapshot.test_address,
                                        s_spi_flash_rt.snapshot.last_write_data,
                                        s_spi_flash_rt.snapshot.last_test_length);
    if (status != HAL_OK)
    {
        SPI_Flash_SetTestError(SPI_FLASH_RESULT_HAL_ERROR);
        return;
    }

    s_spi_flash_rt.snapshot.write_count++;
    s_spi_flash_rt.snapshot.last_result = (uint8_t)SPI_FLASH_RESULT_BUSY;
    s_spi_flash_rt.snapshot.test_state  = (uint8_t)SPI_FLASH_TEST_WAIT_PROGRAM_COMPLETE;
    s_spi_flash_rt.operation_deadline_ms = now_ms + SPI_FLASH_PROGRAM_TIMEOUT_MS;
}

static void SPI_Flash_ProcessWaitProgram(uint32_t now_ms)
{
    HAL_StatusTypeDef status;

    if (s_spi_flash_rt.snapshot.busy != false)
    {
        if (SPI_Flash_TimeDue(now_ms, s_spi_flash_rt.operation_deadline_ms) != 0u)
        {
            SPI_Flash_SetTestError(SPI_FLASH_RESULT_TIMEOUT);
        }
        return;
    }

    memset(s_spi_flash_rt.verify_buffer, 0, sizeof(s_spi_flash_rt.verify_buffer));

    status = SPI_Flash_ReadBuffer(s_spi_flash_rt.snapshot.test_address,
                                  s_spi_flash_rt.verify_buffer,
                                  s_spi_flash_rt.snapshot.last_test_length);
    if (status != HAL_OK)
    {
        SPI_Flash_SetTestError(SPI_FLASH_RESULT_HAL_ERROR);
        return;
    }

    memcpy(s_spi_flash_rt.snapshot.last_read_data,
           s_spi_flash_rt.verify_buffer,
           s_spi_flash_rt.snapshot.last_test_length);

    if (memcmp(s_spi_flash_rt.snapshot.last_write_data,
               s_spi_flash_rt.verify_buffer,
               s_spi_flash_rt.snapshot.last_test_length) != 0)
    {
        SPI_Flash_SetTestError(SPI_FLASH_RESULT_VERIFY_FAIL);
        return;
    }

    s_spi_flash_rt.snapshot.read_count++;
    s_spi_flash_rt.snapshot.last_result = (uint8_t)SPI_FLASH_RESULT_OK;
    s_spi_flash_rt.snapshot.test_state  = (uint8_t)SPI_FLASH_TEST_IDLE;
}

/* -------------------------------------------------------------------------- */
/*  공개 API                                                                   */
/* -------------------------------------------------------------------------- */
void SPI_Flash_Init(void)
{
    memset(&s_spi_flash_rt, 0, sizeof(s_spi_flash_rt));

    strncpy(s_spi_flash_rt.snapshot.doc_part_name,
            "W25Q16BV",
            sizeof(s_spi_flash_rt.snapshot.doc_part_name) - 1u);

    s_spi_flash_rt.snapshot.last_test_length = SPI_FLASH_TEST_LENGTH;
    s_spi_flash_rt.snapshot.test_state       = (uint8_t)SPI_FLASH_TEST_IDLE;

    SPI_Flash_ConfigureCsPinRuntime();

    /* ---------------------------------------------------------------------- */
    /*  power-down 상태였을 가능성을 고려해서 release/reset 순서를 한 번 돈다. */
    /* ---------------------------------------------------------------------- */
    (void)SPI_Flash_SendRawCommand(SPI_FLASH_CMD_RELEASE_POWER_DOWN);
    (void)SPI_Flash_ResetDevice();

    s_spi_flash_rt.snapshot.initialized = true;
    SPI_Flash_RefreshIdentity();
    SPI_Flash_RefreshLiveStatus(HAL_GetTick());
    s_spi_flash_rt.next_status_refresh_ms = HAL_GetTick() + SPI_FLASH_STATUS_REFRESH_MS;
}

void SPI_Flash_Task(uint32_t now_ms)
{
    if (s_spi_flash_rt.snapshot.initialized == false)
    {
        return;
    }

    if ((s_spi_flash_rt.snapshot.test_state != (uint8_t)SPI_FLASH_TEST_IDLE) ||
        (SPI_Flash_TimeDue(now_ms, s_spi_flash_rt.next_status_refresh_ms) != 0u))
    {
        SPI_Flash_RefreshLiveStatus(now_ms);
        s_spi_flash_rt.next_status_refresh_ms = now_ms + SPI_FLASH_STATUS_REFRESH_MS;
    }

    switch ((spi_flash_test_state_t)s_spi_flash_rt.snapshot.test_state)
    {
    case SPI_FLASH_TEST_READ_REQUESTED:
        SPI_Flash_ProcessReadTest();
        break;

    case SPI_FLASH_TEST_WRITE_REQUESTED:
        SPI_Flash_ProcessWriteRequest(now_ms);
        break;

    case SPI_FLASH_TEST_WAIT_ERASE_COMPLETE:
        SPI_Flash_ProcessWaitErase(now_ms);
        break;

    case SPI_FLASH_TEST_WAIT_PROGRAM_COMPLETE:
        SPI_Flash_ProcessWaitProgram(now_ms);
        break;

    case SPI_FLASH_TEST_IDLE:
    default:
        break;
    }
}

void SPI_Flash_CopySnapshot(spi_flash_snapshot_t *out_snapshot)
{
    if (out_snapshot == 0)
    {
        return;
    }

    memcpy(out_snapshot, &s_spi_flash_rt.snapshot, sizeof(*out_snapshot));
}

const char *SPI_Flash_GetResultText(spi_flash_result_t result)
{
    switch (result)
    {
    case SPI_FLASH_RESULT_OK:
        return "OK";

    case SPI_FLASH_RESULT_BUSY:
        return "BUSY";

    case SPI_FLASH_RESULT_TIMEOUT:
        return "TIMEOUT";

    case SPI_FLASH_RESULT_HAL_ERROR:
        return "HALERR";

    case SPI_FLASH_RESULT_VERIFY_FAIL:
        return "VERIFY";

    case SPI_FLASH_RESULT_NOT_PRESENT:
        return "NOCHIP";

    case SPI_FLASH_RESULT_UNSUPPORTED:
        return "UNSUP";

    case SPI_FLASH_RESULT_NONE:
    default:
        return "NONE";
    }
}

const char *SPI_Flash_GetTestStateText(spi_flash_test_state_t state)
{
    switch (state)
    {
    case SPI_FLASH_TEST_READ_REQUESTED:
        return "READ_REQ";

    case SPI_FLASH_TEST_WRITE_REQUESTED:
        return "WRITE_REQ";

    case SPI_FLASH_TEST_WAIT_ERASE_COMPLETE:
        return "WAIT_ERS";

    case SPI_FLASH_TEST_WAIT_PROGRAM_COMPLETE:
        return "WAIT_PGM";

    case SPI_FLASH_TEST_IDLE:
    default:
        return "IDLE";
    }
}
