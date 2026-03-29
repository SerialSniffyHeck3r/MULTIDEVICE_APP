#ifndef SPI_FLASH_H
#define SPI_FLASH_H

#include "main.h"
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  SPI_Flash                                                                  */
/*                                                                            */
/*  목적                                                                      */
/*  - DevEBox 보드의 Winbond W25Q 계열 SPI NOR Flash 를 HAL SPI로 제어한다.    */
/*  - 보드 문서상 W25Q16BV 를 우선 가정하되,                                  */
/*    런타임에는 JEDEC ID / SFDP / Unique ID 를 다시 읽어                     */
/*    실제 장치를 확인한다.                                                   */
/*  - helper API 가 없는 opcode도 raw command API로 그대로 날릴 수 있게       */
/*    설계한다.                                                               */
/*  - UI 테스트 페이지용 read/write test 요청 API를 제공한다.                 */
/* -------------------------------------------------------------------------- */

#ifndef SPI_FLASH_SPI_HANDLE
#define SPI_FLASH_SPI_HANDLE                  hspi1
#endif

#ifndef SPI_FLASH_CS_GPIO_Port
#define SPI_FLASH_CS_GPIO_Port                WINBOND_CS_GPIO_Port
#endif

#ifndef SPI_FLASH_CS_Pin
#define SPI_FLASH_CS_Pin                      WINBOND_CS_Pin
#endif

#ifndef SPI_FLASH_TIMEOUT_MS
#define SPI_FLASH_TIMEOUT_MS                  100u
#endif

#ifndef SPI_FLASH_STATUS_REFRESH_MS
#define SPI_FLASH_STATUS_REFRESH_MS           200u
#endif

#ifndef SPI_FLASH_PAGE_SIZE
#define SPI_FLASH_PAGE_SIZE                   256u
#endif

#ifndef SPI_FLASH_SECTOR_SIZE_4K
#define SPI_FLASH_SECTOR_SIZE_4K              4096u
#endif

#ifndef SPI_FLASH_TEST_LENGTH
#define SPI_FLASH_TEST_LENGTH                 32u
#endif

#ifndef SPI_FLASH_ERASE_TIMEOUT_MS
#define SPI_FLASH_ERASE_TIMEOUT_MS            3000u
#endif

#ifndef SPI_FLASH_PROGRAM_TIMEOUT_MS
#define SPI_FLASH_PROGRAM_TIMEOUT_MS          100u
#endif

/* -------------------------------------------------------------------------- */
/*  주요 opcode                                                                */
/* -------------------------------------------------------------------------- */
#define SPI_FLASH_CMD_WRITE_ENABLE            0x06u
#define SPI_FLASH_CMD_WRITE_DISABLE           0x04u
#define SPI_FLASH_CMD_READ_STATUS_REG1        0x05u
#define SPI_FLASH_CMD_READ_STATUS_REG2        0x35u
#define SPI_FLASH_CMD_READ_STATUS_REG3        0x15u
#define SPI_FLASH_CMD_WRITE_STATUS_REG1       0x01u
#define SPI_FLASH_CMD_WRITE_STATUS_REG2       0x31u
#define SPI_FLASH_CMD_WRITE_STATUS_REG3       0x11u
#define SPI_FLASH_CMD_READ_DATA               0x03u
#define SPI_FLASH_CMD_FAST_READ               0x0Bu
#define SPI_FLASH_CMD_PAGE_PROGRAM            0x02u
#define SPI_FLASH_CMD_SECTOR_ERASE_4K         0x20u
#define SPI_FLASH_CMD_BLOCK_ERASE_32K         0x52u
#define SPI_FLASH_CMD_BLOCK_ERASE_64K         0xD8u
#define SPI_FLASH_CMD_CHIP_ERASE_C7           0xC7u
#define SPI_FLASH_CMD_CHIP_ERASE_60           0x60u
#define SPI_FLASH_CMD_POWER_DOWN              0xB9u
#define SPI_FLASH_CMD_RELEASE_POWER_DOWN      0xABu
#define SPI_FLASH_CMD_JEDEC_ID                0x9Fu
#define SPI_FLASH_CMD_MANUFACTURER_DEVICE_ID  0x90u
#define SPI_FLASH_CMD_UNIQUE_ID               0x4Bu
#define SPI_FLASH_CMD_SFDP                    0x5Au
#define SPI_FLASH_CMD_ENABLE_RESET            0x66u
#define SPI_FLASH_CMD_RESET_DEVICE            0x99u
#define SPI_FLASH_CMD_ENABLE_4BYTE_ADDR       0xB7u
#define SPI_FLASH_CMD_EXIT_4BYTE_ADDR         0xE9u
#define SPI_FLASH_CMD_ERASE_SUSPEND           0x75u
#define SPI_FLASH_CMD_ERASE_RESUME            0x7Au

/* -------------------------------------------------------------------------- */
/*  결과 / 테스트 상태                                                         */
/* -------------------------------------------------------------------------- */
typedef enum
{
    SPI_FLASH_RESULT_NONE         = 0u,
    SPI_FLASH_RESULT_OK           = 1u,
    SPI_FLASH_RESULT_BUSY         = 2u,
    SPI_FLASH_RESULT_TIMEOUT      = 3u,
    SPI_FLASH_RESULT_HAL_ERROR    = 4u,
    SPI_FLASH_RESULT_VERIFY_FAIL  = 5u,
    SPI_FLASH_RESULT_NOT_PRESENT  = 6u,
    SPI_FLASH_RESULT_UNSUPPORTED  = 7u
} spi_flash_result_t;

typedef enum
{
    SPI_FLASH_TEST_IDLE                  = 0u,
    SPI_FLASH_TEST_READ_REQUESTED        = 1u,
    SPI_FLASH_TEST_WRITE_REQUESTED       = 2u,
    SPI_FLASH_TEST_WAIT_ERASE_COMPLETE   = 3u,
    SPI_FLASH_TEST_WAIT_PROGRAM_COMPLETE = 4u
} spi_flash_test_state_t;

/* -------------------------------------------------------------------------- */
/*  공개 snapshot 구조체                                                       */
/* -------------------------------------------------------------------------- */
typedef struct
{
    bool     initialized;
    bool     present;
    bool     busy;
    bool     write_enable_latch;
    bool     w25q16bv_compatible;

    uint32_t last_update_ms;
    uint32_t capacity_bytes;
    uint32_t test_address;

    uint32_t command_count;
    uint32_t read_count;
    uint32_t write_count;
    uint32_t erase_count;
    uint32_t error_count;

    uint8_t  manufacturer_id;
    uint8_t  memory_type;
    uint8_t  capacity_id;
    uint16_t manufacturer_device_id;

    uint8_t  unique_id[8];
    uint8_t  sfdp_header[8];

    uint8_t  status_reg1;
    uint8_t  status_reg2;
    uint8_t  status_reg3;

    uint8_t  last_opcode;
    uint8_t  last_hal_status;
    uint8_t  last_result;
    uint8_t  test_state;
    uint8_t  last_test_length;

    uint8_t  last_read_data[SPI_FLASH_TEST_LENGTH];
    uint8_t  last_write_data[SPI_FLASH_TEST_LENGTH];

    char     part_name[24];
    char     doc_part_name[24];
} spi_flash_snapshot_t;

/* -------------------------------------------------------------------------- */
/*  공개 API                                                                   */
/* -------------------------------------------------------------------------- */
void SPI_Flash_Init(void);
void SPI_Flash_Task(uint32_t now_ms);
void SPI_Flash_CopySnapshot(spi_flash_snapshot_t *out_snapshot);

const char *SPI_Flash_GetResultText(spi_flash_result_t result);
const char *SPI_Flash_GetTestStateText(spi_flash_test_state_t state);

/* raw command API */
HAL_StatusTypeDef SPI_Flash_SendRawCommand(uint8_t opcode);
HAL_StatusTypeDef SPI_Flash_SendRawCommandTx(uint8_t opcode,
                                             const uint8_t *tx_data,
                                             uint32_t tx_length);
HAL_StatusTypeDef SPI_Flash_SendRawCommandRx(uint8_t opcode,
                                             uint8_t *rx_data,
                                             uint32_t rx_length);
HAL_StatusTypeDef SPI_Flash_SendRawCommandDummyRx(uint8_t opcode,
                                                  uint8_t dummy_bytes,
                                                  uint8_t *rx_data,
                                                  uint32_t rx_length);
HAL_StatusTypeDef SPI_Flash_SendRawCommandAddressTx(uint8_t opcode,
                                                    uint32_t address,
                                                    uint8_t address_bytes,
                                                    const uint8_t *tx_data,
                                                    uint32_t tx_length);
HAL_StatusTypeDef SPI_Flash_SendRawCommandAddressRx(uint8_t opcode,
                                                    uint32_t address,
                                                    uint8_t address_bytes,
                                                    uint8_t dummy_bytes,
                                                    uint8_t *rx_data,
                                                    uint32_t rx_length);

/* high-level helper API */
HAL_StatusTypeDef SPI_Flash_ResetDevice(void);
HAL_StatusTypeDef SPI_Flash_ReadJedecId(uint8_t *manufacturer_id,
                                        uint8_t *memory_type,
                                        uint8_t *capacity_id);
HAL_StatusTypeDef SPI_Flash_ReadManufacturerDeviceId(uint16_t *manufacturer_device_id);
HAL_StatusTypeDef SPI_Flash_ReadUniqueId(uint8_t out_unique_id[8]);
HAL_StatusTypeDef SPI_Flash_ReadSfdpHeader(uint8_t out_sfdp_header[8]);
HAL_StatusTypeDef SPI_Flash_ReadStatusRegisters(uint8_t *sr1,
                                                uint8_t *sr2,
                                                uint8_t *sr3);
HAL_StatusTypeDef SPI_Flash_ReadBuffer(uint32_t address,
                                       uint8_t *buffer,
                                       uint32_t length);
HAL_StatusTypeDef SPI_Flash_WriteBuffer(uint32_t address,
                                        const uint8_t *buffer,
                                        uint32_t length);
HAL_StatusTypeDef SPI_Flash_EraseSector4K(uint32_t address);
HAL_StatusTypeDef SPI_Flash_EraseBlock32K(uint32_t address);
HAL_StatusTypeDef SPI_Flash_EraseBlock64K(uint32_t address);
HAL_StatusTypeDef SPI_Flash_EraseChip(void);

/* UI / 테스트 페이지용 request API */
void SPI_Flash_RequestReadTest(void);
void SPI_Flash_RequestWriteTest(void);

#ifdef __cplusplus
}
#endif

#endif /* SPI_FLASH_H */
