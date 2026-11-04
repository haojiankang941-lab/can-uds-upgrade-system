/**
 ******************************************************************************
 * @file    ota.c
 * @brief   OTA 功能模块实现
 * @note    依赖: iap.h, boot.h, W1251.h, Uart.c (Uart_GetByte/Uart_SendByte)
 ******************************************************************************
 */

#include "ota.h"
#include "boot.h"
#include "iap.h"
#include "W1251.h"
#include <stdio.h>

/* ================================================================
 * 外部函数声明（来自 Uart.c）
 * ================================================================ */
extern uint8_t Uart_GetByte(void);
extern void    Uart_SendByte(char data);

/* ================================================================
 * 静态辅助函数
 * ================================================================ */

/**
 * @brief  将 4 个字节按 little-endian 打包为一个 32-bit 字
 * @note   芯片为小端序：addr 存 b0, addr+1 存 b1, addr+2 存 b2, addr+3 存 b3
 *         打包公式: b0 | (b1<<8) | (b2<<16) | (b3<<24)
 *         验证: *(uint32_t*)addr = 0x12345678 → 读取字节为 78 56 34 12
 */
static uint32_t PackWord(uint8_t b0, uint8_t b1, uint8_t b2, uint8_t b3)
{
    return (uint32_t)b0
         | ((uint32_t)b1 << 8)
         | ((uint32_t)b2 << 16)
         | ((uint32_t)b3 << 24);
}

/**
 * @brief  擦除从 startAddr 开始的指定大小的 Flash 扇区
 * @note   以 SECTOR_SIZE (512B) 为单位逐个擦除
 *         芯片特性：擦除后扇区内容全为 0
 * @param  startAddr 起始 Flash 地址（扇区对齐）
 * @param  size      擦除大小（字节）
 */
static void Flash_EraseSectors(uint32_t startAddr, uint32_t size)
{
    uint32_t addr;
    uint32_t endAddr = startAddr + size;

    for (addr = startAddr; addr < endAddr; addr += SECTOR_SIZE)
    {
        IAP_SERASE(addr, FLASH_AREA_BASE);
    }
}

/**
 * @brief  将 RAM buffer 中的数据写入 Flash（批量写入模式）
 * @note   使用批量模式提高效率：unlock → enable → 循环写所有 word → lock
 *         参考 IAP_TestMode() 的批量写入模式
 *         芯片 Flash 擦除后为 0，此函数将指定位从 0 写为 1
 * @param  destAddr  目标 Flash 地址（4 字节对齐）
 * @param  buf       源数据 buffer
 * @param  byteCount 写入字节数（必须是 4 的倍数）
 */
static void Flash_WriteBuffer(uint32_t destAddr, const uint8_t *buf, uint32_t byteCount)
{
    uint32_t i;
    uint32_t wordCount = byteCount / 4;

    /* 逐字写入：每字独立 Unlock→Enable→Write→Lock
     * 避免 IAP_SERASE 残留位 + 批量写导致 Flash 控制器状态混乱 */
    for (i = 0; i < wordCount; i++)
    {
        uint32_t word = PackWord(buf[i * 4], buf[i * 4 + 1],
                                 buf[i * 4 + 2], buf[i * 4 + 3]);
        IAP_ProgramWord(destAddr + i * 4, word);
    }
}

/* ================================================================
 * 公开函数实现
 * ================================================================ */

/**
 * @brief  检测是否存在 OTA 升级请求
 * @retval 0 — 无升级请求
 * @retval 1 — 需要升级
 */
uint8_t OTA_CheckRequest(void)
{
    OTA_INFO *info = (OTA_INFO *)INFO_ADDRESS;

    if (info->request_flag == OTA_REQUEST_MAGIC)
    {
        return 1;   /* 需要升级 */
    }

    return 0;       /* 无升级请求 */
}

/**
 * @brief  通过 UART 接收 BIN 文件并写入 Backup 区域
 * @note   协议格式（简单）：
 *         [4 bytes: file_size (uint32 LE)] [N bytes: BIN 数据]
 *         每收完 512 字节回复 ACK (0x06)
 * @param  out_file_size [出参] 实际接收的文件大小
 * @retval OTA_OK / OTA_ERR_*
 */
uint8_t OTA_ReceiveBin(uint32_t *out_file_size)
{
    uint8_t  ram_buf[RAM_BUFFER_SIZE];
    uint32_t file_size   = 0;
    uint32_t received    = 0;
    uint32_t destAddr    = BACKUP_ADDRESS;
    uint32_t sectorCount;
    uint8_t  b0, b1, b2, b3;
    uint32_t i;

    /* ── 1. 接收文件大小 (4 字节, little-endian) ── */
    printf("[OTA] Send 4-byte file_size (uint32 LE) now...\r\n");
    b0 = Uart_GetByte();
    b1 = Uart_GetByte();
    b2 = Uart_GetByte();
    b3 = Uart_GetByte();
    file_size = PackWord(b0, b1, b2, b3);

    *out_file_size = file_size;

    if (file_size == 0)
    {
        return OTA_ERR_UART_RX;
    }

    printf("[OTA] File size:%lu bytes, erasing Backup...\r\n", file_size);

    /* ── 2. 擦除 Backup 区域的对应扇区 ── */
    sectorCount = (file_size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    Flash_EraseSectors(BACKUP_ADDRESS, sectorCount * SECTOR_SIZE);

    printf("[OTA] Backup erased, receiving data...\r\n");

    /* ── 3. 分块接收 BIN 数据 ── */
    while (received < file_size)
    {
        uint32_t chunkSize;
        uint32_t writeSize;

        /* 计算本块大小：不超过 512 字节，且不超过剩余字节数 */
        if (file_size - received > RAM_BUFFER_SIZE)
        {
            chunkSize = RAM_BUFFER_SIZE;
        }
        else
        {
            chunkSize = file_size - received;
        }

        /* 接收本块数据到 RAM buffer */
        for (i = 0; i < chunkSize; i++)
        {
            ram_buf[i] = Uart_GetByte();
        }

        /* 写入 Flash 要求 4 字节对齐，不足部分用 0x00 补齐
         * （Flash 擦除后为 0，0x00 表示不改变这些位） */
        writeSize = chunkSize;
        if (writeSize & 0x03)
        {
            writeSize = (writeSize + 3) & ~0x03UL;
            for (i = chunkSize; i < writeSize; i++)
            {
                ram_buf[i] = 0x00;
            }
        }

        /* 写入 Flash (Backup 区域) */
        Flash_WriteBuffer(destAddr, ram_buf, writeSize);

        destAddr += writeSize;
        received += chunkSize;   /* 只累加实际 BIN 数据字节数 */

        /* 应答：通知 PC 本块写入成功 */
        Uart_SendByte(OTA_ACK);

        /* 喂狗 */
        WDT->WDT_CON |= 0x01;
    }

    return OTA_OK;
}

/**
 * @brief  将 Backup 区域的 BIN 数据复制到 APP 区域
 * @note   1. 擦除 APP 区域对应扇区
 *         2. 以 512 字节块为单位，从 Backup 读取 → 写入 APP
 *         3. 不操作 INFO 区域（OTA_SaveInfo 负责清除升级标志）
 * @param  size BIN 文件大小（字节）
 * @retval OTA_OK / OTA_ERR_*
 */
uint8_t OTA_CopyBackupToApp(uint32_t size)
{
    uint32_t sectorCount;
    uint32_t offset;
    uint32_t wordCount;
    uint32_t i;

    if (size == 0)
    {
        return OTA_ERR_FLASH_WRITE;
    }

    /* ── 1. 擦除 APP 区域 ── */
    sectorCount = (size + SECTOR_SIZE - 1) / SECTOR_SIZE;
    printf("[OTA] Erasing APP(%lu sectors)...\r\n", sectorCount);
    Flash_EraseSectors(APP_ADDRESS, sectorCount * SECTOR_SIZE);

    /* ── 2. 以 512 字节为单位逐块复制 ── */
    for (offset = 0; offset < size; offset += SECTOR_SIZE)
    {
        uint32_t chunkSize;
        uint32_t wordsThisBlock;

        /* 最后一块可能不足 512 字节 */
        if (offset + SECTOR_SIZE > size)
        {
            chunkSize = size - offset;
        }
        else
        {
            chunkSize = SECTOR_SIZE;
        }

        /* 按 32-bit 字逐字复制，每字用 IAP_ProgramWord 确保正确写入 */
        wordsThisBlock = (chunkSize + 3) / 4;

        for (i = 0; i < wordsThisBlock; i++)
        {
            uint32_t srcAddr = BACKUP_ADDRESS + offset + i * 4;
            uint32_t dstAddr = APP_ADDRESS   + offset + i * 4;
            uint32_t word;

            /* 从 Backup 读取 */
            word = *((volatile uint32_t *)srcAddr);

            /* 写入 APP（使用已验证的 IAP_ProgramWord） */
            IAP_ProgramWord(dstAddr, word);
        }

        /* 每 16 块打印进度 */
        if ((offset / SECTOR_SIZE) % 16 == 0)
        {
            printf("[OTA] Copying %lu/%lu\r\n",
                   offset / SECTOR_SIZE, sectorCount);
        }

        /* 喂狗 */
        WDT->WDT_CON |= 0x01;
    }

    return OTA_OK;
}

/**
 * @brief  保存 OTA 升级信息到 INFO 区域
 * @note   1. 先擦除 INFO 扇区 → 全 0（request_flag 自动变为 0 = 无升级请求）
 *         2. 写入 version 和 date 字段
 *         3. request_flag 保持 0，下次上电直接跳转 APP
 * @param  version APP 版本号
 * @param  date   升级时间
 */
void OTA_SaveInfo(uint32_t version, uint32_t date)
{
    /* 擦除 INFO 扇区：512 字节全变为 0 */
    Flash_EraseSectors(INFO_ADDRESS, SECTOR_SIZE);

    /* 逐字写入 version 和 date（request_flag 保持擦除后的 0） */
    IAP_ProgramWord(INFO_ADDRESS + 4, version);   /* 偏移 4: version */
    IAP_ProgramWord(INFO_ADDRESS + 8, date);      /* 偏移 8: date    */
    /* 偏移 0 (request_flag) 保持为 0，不需写入 */
}
