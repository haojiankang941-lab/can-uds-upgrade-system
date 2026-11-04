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

#define OTA_SECTOR_SIZE   512U
#define OTA_INFO_MAGIC    0xAA55AA55UL

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

uint8_t OTA_CheckRequest(void)
{
    OTA_InfoTypeDef *info = (OTA_InfoTypeDef *)INFO_ADDRESS;
    return (info->request_flag == OTA_INFO_MAGIC) ? 1U : 0U;
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

void OTA_SaveInfo(uint32_t version, uint32_t date, uint32_t size, uint32_t crc)
{
    uint32_t addr = INFO_ADDRESS;

    /* Erase the whole INFO sector: all fields become 0, including
     * request_flag, so the next boot jumps to APP. */
    IAP_SERASE(addr, FLASH_AREA_BASE);
    Flash_Delay();

    IAP_ProgramWord(addr + 4U,  version);
    IAP_ProgramWord(addr + 8U,  date);
    IAP_ProgramWord(addr + 12U, size);
    IAP_ProgramWord(addr + 16U, crc);
    /* addr + 0 (request_flag) is left at the erased value 0. */
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

    words = (len + 3U) / 4U;
    for (i = 0; i < words; i++)
    {
        uint32_t word = *(volatile uint32_t *)(src + i * 4U);
        IAP_ProgramWord(dst + i * 4U, word);
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
