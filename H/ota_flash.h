/**
 ******************************************************************************
 * @file    ota_flash.h
 * @brief   Flash helpers for the UDS bootloader
 * @note    Backup/APP erase, word programming with byte staging, verify,
 *          CRC32 and INFO management.
 *
 *          INFO layout (one 512-byte sector at INFO_ADDRESS):
 *            0x00 request_flag   legacy flag, kept 0 by the current code
 *            0x04 version        image version word (0x00MMmmPP)
 *            0x08 date           image date word  (0x20YYMMDD BCD)
 *            0x0C size           image size in bytes
 *            0x10 crc            CRC32 of the image
 *            0x14 magic          0x4F544149 ("OTAI") = new-style record
 *            0x18 staging        0x5AA5C0DE written before APP is erased
 *            0x1C committed      0xA55A600D written after APP was verified
 *            0x20 info_crc       CRC32 over bytes 0x00..0x1F
 *            0x24 flash_count    successful activations
 *            0x28 last_result    OTA_RESULT_* of the last activation
 *
 *          Written in that order, so a power failure at any point leaves
 *          info_crc absent or wrong and the record is treated as invalid.
 *
 *          The upgrade request flag lives in its own sector at
 *          INFO_ADDRESS + 0x200 so that requesting an upgrade does not
 *          erase the image information (version/date/crc) above.
 ******************************************************************************
 */

#ifndef __OTA_FLASH_H
#define __OTA_FLASH_H

#include <stdint.h>

#define OTA_OK      0U
#define OTA_ERROR   1U

/* Result codes stored in INFO.last_result and reported through 0x22. */
#define OTA_RESULT_OK           0x00000001UL
#define OTA_RESULT_CRC_FAIL     0x00000002UL
#define OTA_RESULT_COPY_FAIL    0x00000003UL
#define OTA_RESULT_VERIFY_FAIL  0x00000004UL

#define OTA_INFO_MAGIC          0x4F544149UL   /* "OTAI" */
#define OTA_INFO_STAGING        0x5AA5C0DEUL
#define OTA_INFO_COMMITTED      0xA55A600DUL
#define OTA_REQUEST_MAGIC       0xAA55AA55UL
#define OTA_REQUEST_SECTOR      0x00000200UL   /* INFO_ADDRESS + this */

typedef struct
{
    uint32_t request_flag;   /* offset 0x00: legacy 0xAA55AA55 = upgrade requested */
    uint32_t version;        /* offset 0x04 */
    uint32_t date;           /* offset 0x08 */
    uint32_t size;           /* offset 0x0C: image size in bytes */
    uint32_t crc;            /* offset 0x10: CRC32 of the image */
    uint32_t magic;          /* offset 0x14: OTA_INFO_MAGIC */
    uint32_t staging;        /* offset 0x18: OTA_INFO_STAGING when copy started */
    uint32_t committed;      /* offset 0x1C: OTA_INFO_COMMITTED when verified */
    uint32_t info_crc;       /* offset 0x20: CRC32 over bytes 0x00..0x1F */
    uint32_t flash_count;    /* offset 0x24: number of activations */
    uint32_t last_result;    /* offset 0x28: OTA_RESULT_* */
} OTA_InfoTypeDef;

/* 1 when an upgrade was requested (request sector or legacy flag). */
uint8_t OTA_CheckRequest(void);

/* Ask the bootloader to stay in UDS mode after the next reset.
 * Reference implementation for the application: the APP must contain the
 * same logic (it cannot call into the boot image).  Erases only the request
 * sector, so the stored version/date/crc survive. */
void OTA_RequestUpgrade(void);

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

/* Erase dst range and copy src -> dst word by word.  The copy runs from the
 * highest word down to word 0, so the APP vector table (stack pointer and
 * reset vector) is written last: a power failure leaves SP = 0 and the
 * image is rejected by OTA_IsAppValid() instead of being entered. */
uint8_t OTA_Flash_CopyRange(uint32_t dst, uint32_t src, uint32_t len);

/* CRC32 (poly 0xEDB88320).  Update() takes/returns the raw running value;
 * Crc32()/Flash_Crc32() return the final XORed value. */
uint32_t OTA_Crc32Update(uint32_t crc, const uint8_t *buf, uint32_t len);
uint32_t OTA_Crc32(const uint8_t *buf, uint32_t len);
uint32_t OTA_Flash_Crc32(uint32_t addr, uint32_t len);

/* Write the new image record and clear the upgrade request, before the APP
 * is erased.  After this call INFO is "staging" (not valid) until
 * OTA_InfoFinish() stores the result. */
void OTA_InfoStart(uint32_t version, uint32_t date, uint32_t size, uint32_t crc);

/* Store the activation result.  OTA_RESULT_OK commits the record
 * (last_result + committed flag + info_crc); any other value only records
 * last_result, leaving the record invalid. */
void OTA_InfoFinish(uint32_t result);

/* Read INFO.  Returns 1 when the record is complete and trustworthy
 * (or when a legacy record has a plausible size), 0 otherwise.  The fields
 * are filled in either way - the caller can still show version/date. */
uint8_t OTA_GetInfo(OTA_InfoTypeDef *out);

#endif /* __OTA_FLASH_H */
