/**
 ******************************************************************************
 * @file    ota_flash.c
 * @brief   Flash helpers for the UDS bootloader
 * @note    The chip erases flash to 0x00 and programs 32-bit words.
 *          There is no status register, so erase is followed by a bounded
 *          delay and a read-back check.
 ******************************************************************************
 */

#include "ota_flash.h"
#include "boot.h"
#include "iap.h"
#include "W1251.h"

#define OTA_SECTOR_SIZE     512U
#define OTA_INFO_CRC_BYTES  0x20U   /* info_crc covers offsets 0x00..0x1F */

static uint8_t  s_stage[4];
static uint32_t s_stage_addr;
static uint8_t  s_stage_len;

static void Flash_Delay(void)
{
    volatile uint32_t i;
    for (i = 0; i < 2000UL; i++)
    {
        ;
    }
}

static uint32_t PackWord(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static uint8_t RangeAllowed(uint32_t addr, uint32_t len)
{
    if (len == 0U)
    {
        return 0U;
    }
    if (addr >= APP_ADDRESS && (addr + len) <= (APP_ADDRESS + APP_SIZE))
    {
        return 1U;
    }
    if (addr >= BACKUP_ADDRESS && (addr + len) <= (BACKUP_ADDRESS + BACKUP_SIZE))
    {
        return 1U;
    }
    return 0U;
}

/* -------------------------------------------------------------------------- */
/* INFO                                                                       */
/* -------------------------------------------------------------------------- */

static uint32_t InfoCrc(void)
{
    return OTA_Crc32((const uint8_t *)INFO_ADDRESS, OTA_INFO_CRC_BYTES);
}

uint8_t OTA_CheckRequest(void)
{
    const OTA_InfoTypeDef *info = (const OTA_InfoTypeDef *)INFO_ADDRESS;
    const uint32_t *req = (const uint32_t *)(INFO_ADDRESS + OTA_REQUEST_SECTOR);

    if (*req == OTA_REQUEST_MAGIC)
    {
        return 1U;
    }

    /* Records written by older firmware kept the flag in INFO word 0. */
    return (info->request_flag == OTA_REQUEST_MAGIC) ? 1U : 0U;
}

void OTA_RequestUpgrade(void)
{
    uint32_t addr = INFO_ADDRESS + OTA_REQUEST_SECTOR;

    /* Only the request sector is erased: version/date/size/crc stay intact. */
    IAP_SERASE(addr, FLASH_AREA_BASE);
    Flash_Delay();
    IAP_ProgramWord(addr, OTA_REQUEST_MAGIC);
}

uint8_t OTA_IsAppValid(void)
{
    uint32_t sp = *(volatile uint32_t *)APP_ADDRESS;
    uint32_t rv = *(volatile uint32_t *)(APP_ADDRESS + 4U);

    /* Stack pointer must point into SRAM and be 4-byte aligned. */
    if (sp < 0x20000000UL || sp > 0x20008000UL || (sp & 0x3UL) != 0U)
    {
        return 0U;
    }

    /* Reset vector must point into APP and have the Thumb bit set. */
    if ((rv & 0x1UL) == 0U)
    {
        return 0U;
    }
    rv &= ~0x1UL;
    if (rv < APP_ADDRESS || rv >= (APP_ADDRESS + APP_SIZE))
    {
        return 0U;
    }

    return 1U;
}

uint8_t OTA_GetInfo(OTA_InfoTypeDef *out)
{
    const OTA_InfoTypeDef *info = (const OTA_InfoTypeDef *)INFO_ADDRESS;

    if (out == 0)
    {
        return 0U;
    }

    out->request_flag = info->request_flag;
    out->version      = info->version;
    out->date         = info->date;
    out->size         = info->size;
    out->crc          = info->crc;
    out->magic        = info->magic;
    out->staging      = info->staging;
    out->committed    = info->committed;
    out->info_crc     = info->info_crc;
    out->flash_count  = info->flash_count;
    out->last_result  = info->last_result;

    if (out->size == 0U || out->size > BACKUP_SIZE)
    {
        return 0U;
    }

    /* Records written by older firmware have no magic word.  Their
     * version/date/size/crc are still usable, just not integrity checked. */
    if (out->magic != OTA_INFO_MAGIC)
    {
        return 1U;
    }

    if (out->info_crc != InfoCrc())
    {
        return 0U;
    }

    return (out->committed == OTA_INFO_COMMITTED) ? 1U : 0U;
}

void OTA_InfoStart(uint32_t version, uint32_t date, uint32_t size, uint32_t crc)
{
    const OTA_InfoTypeDef *info = (const OTA_InfoTypeDef *)INFO_ADDRESS;
    uint32_t count = 0U;

    if (info->magic == OTA_INFO_MAGIC && info->flash_count < 0xFFFFFFFEUL)
    {
        count = info->flash_count;
    }

    /* Erase the image record and the upgrade request together: from here on
     * the old record is gone, so a power failure during the copy leaves
     * INFO "staging" instead of claiming an image that is not there. */
    IAP_SERASE(INFO_ADDRESS, FLASH_AREA_BASE);
    Flash_Delay();
    IAP_SERASE(INFO_ADDRESS + OTA_REQUEST_SECTOR, FLASH_AREA_BASE);
    Flash_Delay();

    IAP_ProgramWord(INFO_ADDRESS + 0x04U, version);
    IAP_ProgramWord(INFO_ADDRESS + 0x08U, date);
    IAP_ProgramWord(INFO_ADDRESS + 0x0CU, size);
    IAP_ProgramWord(INFO_ADDRESS + 0x10U, crc);
    IAP_ProgramWord(INFO_ADDRESS + 0x14U, OTA_INFO_MAGIC);
    IAP_ProgramWord(INFO_ADDRESS + 0x18U, OTA_INFO_STAGING);
    IAP_ProgramWord(INFO_ADDRESS + 0x24U, count + 1U);
    /* 0x00, 0x1C, 0x20 and 0x28 stay at the erased value 0. */
}

void OTA_InfoFinish(uint32_t result)
{
    IAP_ProgramWord(INFO_ADDRESS + 0x28U, result);

    if (result != OTA_RESULT_OK)
    {
        return;   /* info_crc stays 0, the record is not valid */
    }

    /* Order matters: info_crc covers the committed flag. */
    IAP_ProgramWord(INFO_ADDRESS + 0x1CU, OTA_INFO_COMMITTED);
    IAP_ProgramWord(INFO_ADDRESS + 0x20U, InfoCrc());
}

/* -------------------------------------------------------------------------- */
/* Erase / program / verify                                                   */
/* -------------------------------------------------------------------------- */

uint8_t OTA_Flash_EraseRange(uint32_t addr, uint32_t len)
{
    uint32_t start;
    uint32_t end;
    uint32_t a;

    if (RangeAllowed(addr, len) == 0U)
    {
        return OTA_ERROR;
    }

    /* Any pending partial word belongs to the old contents of the range. */
    s_stage_len = 0U;

    start = addr & ~(OTA_SECTOR_SIZE - 1U);
    end   = (addr + len + OTA_SECTOR_SIZE - 1U) & ~(OTA_SECTOR_SIZE - 1U);

    for (a = start; a < end; a += OTA_SECTOR_SIZE)
    {
        IAP_SERASE(a, FLASH_AREA_BASE);
        Flash_Delay();

        /* Erased value is 0x00; check first and last word. */
        if (*(volatile uint32_t *)a != 0U ||
            *(volatile uint32_t *)(a + OTA_SECTOR_SIZE - 4U) != 0U)
        {
            return OTA_ERROR;
        }
    }

    return OTA_OK;
}

uint8_t OTA_Flash_Program(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint32_t i = 0;
    uint8_t  take;

    if (len == 0U)
    {
        return OTA_OK;
    }

    /* Complete a word left over from the previous call. */
    if (s_stage_len != 0U)
    {
        take = (uint8_t)(4U - s_stage_len);
        if (take > len)
        {
            take = (uint8_t)len;
        }
        for (i = 0; i < take; i++)
        {
            s_stage[s_stage_len + i] = buf[i];
        }
        s_stage_len = (uint8_t)(s_stage_len + take);

        if (s_stage_len < 4U)
        {
            return OTA_OK;
        }

        IAP_ProgramWord(s_stage_addr, PackWord(s_stage));
        s_stage_len = 0U;
        addr += take;
        i = take;
    }

    while ((len - i) >= 4U)
    {
        IAP_ProgramWord(addr, PackWord(&buf[i]));
        addr += 4U;
        i += 4U;
    }

    if (i < len)
    {
        uint8_t k;

        s_stage_addr = addr;
        s_stage_len  = (uint8_t)(len - i);
        for (k = 0; k < s_stage_len; k++)
        {
            s_stage[k] = buf[i + k];
        }
    }

    return OTA_OK;
}

void OTA_Flash_Flush(void)
{
    uint8_t i;

    if (s_stage_len == 0U)
    {
        return;
    }

    for (i = s_stage_len; i < 4U; i++)
    {
        s_stage[i] = 0x00;
    }
    IAP_ProgramWord(s_stage_addr, PackWord(s_stage));
    s_stage_len = 0U;
}

uint8_t OTA_Flash_Verify(uint32_t addr, const uint8_t *buf, uint32_t len)
{
    uint32_t i;

    for (i = 0; i < len; i++)
    {
        if (*(volatile uint8_t *)(addr + i) != buf[i])
        {
            return OTA_ERROR;
        }
    }
    return OTA_OK;
}

uint8_t OTA_Flash_CopyRange(uint32_t dst, uint32_t src, uint32_t len)
{
    uint32_t i;
    uint32_t words;

    if (RangeAllowed(dst, len) == 0U)
    {
        return OTA_ERROR;
    }
    if (OTA_Flash_EraseRange(dst, len) != OTA_OK)
    {
        return OTA_ERROR;
    }

    /* Copy from the highest word down to word 0.  The stack pointer and the
     * reset vector are the first two words of the image, so they are the
     * last ones programmed: a power failure in the middle of the copy
     * leaves SP = 0 and OTA_IsAppValid() refuses the image, keeping the ECU
     * in the bootloader instead of entering a half-written application. */
    words = (len + 3U) / 4U;
    for (i = words; i > 0U; i--)
    {
        uint32_t word = *(volatile uint32_t *)(src + (i - 1U) * 4U);
        IAP_ProgramWord(dst + (i - 1U) * 4U, word);
    }

    return OTA_OK;
}

/* -------------------------------------------------------------------------- */
/* CRC32                                                                      */
/* -------------------------------------------------------------------------- */

uint32_t OTA_Crc32Update(uint32_t crc, const uint8_t *buf, uint32_t len)
{
    uint32_t i;
    uint8_t  bit;

    for (i = 0; i < len; i++)
    {
        crc ^= (uint32_t)buf[i];
        for (bit = 0; bit < 8U; bit++)
        {
            if (crc & 1U)
            {
                crc = (crc >> 1) ^ 0xEDB88320UL;
            }
            else
            {
                crc >>= 1;
            }
        }
    }

    return crc;
}

uint32_t OTA_Crc32(const uint8_t *buf, uint32_t len)
{
    return OTA_Crc32Update(0xFFFFFFFFUL, buf, len) ^ 0xFFFFFFFFUL;
}

uint32_t OTA_Flash_Crc32(uint32_t addr, uint32_t len)
{
    uint32_t crc = 0xFFFFFFFFUL;
    uint32_t i;
    uint8_t  b;

    for (i = 0; i < len; i++)
    {
        b = *(volatile uint8_t *)(addr + i);
        crc = OTA_Crc32Update(crc, &b, 1U);
    }

    return crc ^ 0xFFFFFFFFUL;
}
