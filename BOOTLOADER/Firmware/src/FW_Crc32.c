#include "FW_Crc32.h"

/* -------------------------------------------------------------------------- */
/*  공개 API: 초기값                                                           */
/* -------------------------------------------------------------------------- */
uint32_t FW_CRC32_Init(void)
{
    return 0xFFFFFFFFu;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 누적 업데이트                                                    */
/* -------------------------------------------------------------------------- */
uint32_t FW_CRC32_Update(uint32_t crc, const void *data, uint32_t length)
{
    const uint8_t *bytes;
    uint32_t index;
    uint32_t bit_index;

    if ((data == 0) || (length == 0u))
    {
        return crc;
    }

    bytes = (const uint8_t *)data;

    for (index = 0u; index < length; index++)
    {
        crc ^= (uint32_t)bytes[index];

        for (bit_index = 0u; bit_index < 8u; bit_index++)
        {
            if ((crc & 1u) != 0u)
            {
                crc = (crc >> 1) ^ 0xEDB88320u;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: 최종값                                                           */
/* -------------------------------------------------------------------------- */
uint32_t FW_CRC32_Final(uint32_t crc)
{
    return ~crc;
}

/* -------------------------------------------------------------------------- */
/*  공개 API: one-shot 계산                                                    */
/* -------------------------------------------------------------------------- */
uint32_t FW_CRC32_Calc(const void *data, uint32_t length)
{
    return FW_CRC32_Final(FW_CRC32_Update(FW_CRC32_Init(), data, length));
}
