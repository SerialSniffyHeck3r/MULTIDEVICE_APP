#ifndef FW_CRC32_H
#define FW_CRC32_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------------------------------------------------------------------------- */
/*  FW_Crc32                                                                  */
/*                                                                            */
/*  목적                                                                      */
/*  - F/W package header CRC                                                  */
/*  - F/W payload CRC                                                         */
/*  - BKPSRAM boot control block CRC                                          */
/*  를 모두 같은 소프트웨어 CRC32로 계산하기 위한 helper다.                   */
/*                                                                            */
/*  다항식                                                                     */
/*  - Ethernet/ZIP 계열에서 흔히 쓰는 reversed 0xEDB88320                    */
/* -------------------------------------------------------------------------- */

uint32_t FW_CRC32_Init(void);
uint32_t FW_CRC32_Update(uint32_t crc, const void *data, uint32_t length);
uint32_t FW_CRC32_Final(uint32_t crc);
uint32_t FW_CRC32_Calc(const void *data, uint32_t length);

#ifdef __cplusplus
}
#endif

#endif /* FW_CRC32_H */
