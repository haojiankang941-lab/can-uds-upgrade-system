/**
 ******************************************************************************
 * @file    ota_flash.h
 * @brief   Flash helpers for the UDS bootloader
 * @note    Backup/APP erase, word programming with byte staging, verify,
 *          CRC32 and INFO management.
 ******************************************************************************
 */

#ifndef __OTA_FLASH_H
#define __OTA_FLASH_H

#include <stdint.h>

#define OTA_OK      0U
#define OTA_ERROR   1U

typedef struct
{
    uint32_t request_flag;   /* offset 0: 0xAA55AA55 = upgrade requested */
    uint32_t version;        /* offset 4 */
    uint32_t date;           /* offset 8 */
    uint32_t size;           /* offset 12: image size in bytes */
    uint32_t crc;            /* offset 16: CRC32 of the image */
} OTA_InfoTypeDef;

/* INFO.request_flag == OTA_REQUEST_MAGIC */
uint8_t OTA_CheckRequest(void);

/* Start-of-APP sanity check (stack pointer and reset vector). */
uint8_t OTA_IsAppValid(void);

/* Erase every 512-byte sector covering [addr, addr+len).
 * Only APP and BACKUP ranges are accepted. */
uint8_t OTA_Flash_EraseRange(uint32_t addr, uint32_t len);

/* Program arbitrary byte lengths.  A partial word is kept in a staging
 * buffer until the next call or OTA_Flash_Flush(). */
uint8_t OTA_Flash_Program(uint32_t addr, const uint8_t *buf, uint32_t len);
void    OTA_Flash_Flush(void);

/* Byte-for-byte compare between flash and RAM. */
uint8_t OTA_Flash_Verify(uint32_t addr, const uint8_t *buf, uint32_t len);

/* Erase dst range and copy src -> dst word by word. */
uint8_t OTA_Flash_CopyRange(uint32_t dst, uint32_t src, uint32_t len);

/* CRC32 (poly 0xEDB88320).  Update() takes/returns the raw running value;
 * Crc32()/Flash_Crc32() return the final XORed value. */
uint32_t OTA_Crc32Update(uint32_t crc, const uint8_t *buf, uint32_t len);
uint32_t OTA_Crc32(const uint8_t *buf, uint32_t len);
uint32_t OTA_Flash_Crc32(uint32_t addr, uint32_t len);

/* Erase INFO and store version/date/size/crc.  request_flag stays 0. */
void OTA_SaveInfo(uint32_t version, uint32_t date, uint32_t size, uint32_t crc);

#endif /* __OTA_FLASH_H */
