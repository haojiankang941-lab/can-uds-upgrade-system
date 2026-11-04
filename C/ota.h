/**
 ******************************************************************************
 * @file    ota.h
 * @brief   OTA 功能模块头文件
 * @note    负责 OTA 状态检测、BIN 接收、Flash 复制、OTA 信息管理
 ******************************************************************************
 */

#ifndef __OTA_H
#define __OTA_H

#include <stdint.h>

/* ================================================================
 * OTA 升级请求魔数
 * ================================================================
 * 芯片 Flash 擦除后为全 0，写入时将指定位从 0 置为 1。
 * request_flag = 0              → 无升级请求（擦除后默认状态）
 * request_flag = OTA_REQUEST_MAGIC → 有升级请求
 */
#define OTA_REQUEST_MAGIC    0xAA55AA55UL

/* ================================================================
 * OTA_INFO 数据结构
 * ================================================================
 * 直接映射到 INFO_ADDRESS (0x0807E000) 处的 Flash 内容。
 * 使用时通过地址强转访问：
 *   OTA_INFO *info = (OTA_INFO *)INFO_ADDRESS;
 */
typedef struct
{
    uint32_t request_flag;   /* 升级请求标志: 0xAA55AA55 = 有请求, 0 = 无 */
    uint32_t version;        /* APP 版本号                              */
    uint32_t date;           /* 升级时间 (Unix timestamp)               */
} OTA_INFO;

/* ================================================================
 * 返回值定义
 * ================================================================ */
#define OTA_OK                0   /* 操作成功              */
#define OTA_ERR_UART_RX       1   /* UART 接收错误         */
#define OTA_ERR_FLASH_ERASE   2   /* Flash 擦除失败        */
#define OTA_ERR_FLASH_WRITE   3   /* Flash 写入失败        */

/* ================================================================
 * 协议常量
 * ================================================================ */
#define OTA_ACK               0x06   /* 应答：接收成功    */
#define OTA_NAK               0x15   /* 应答：接收失败    */

/* ================================================================
 * 函数声明
 * ================================================================ */

/**
 * @brief  检测是否存在 OTA 升级请求
 * @note   读取 INFO_ADDRESS 处的 request_flag 字段
 * @retval 0 — 无升级请求，直接跳转 APP
 * @retval 1 — 有升级请求，进入 OTA 流程
 */
uint8_t OTA_CheckRequest(void);

/**
 * @brief  通过 UART 接收 PC 发送的 BIN 文件并写入 Backup 区域
 * @note   分块接收：UART → RAM buffer (512B) → Flash 写入 Backup
 *         每收完一个 Sector 回复 ACK，支持大文件
 * @param  out_file_size [出参] 实际接收到的 BIN 文件大小
 * @retval OTA_OK            接收成功
 * @retval OTA_ERR_UART_RX   UART 通信异常
 * @retval OTA_ERR_FLASH_*   Flash 操作失败
 */
uint8_t OTA_ReceiveBin(uint32_t *out_file_size);

/**
 * @brief  将 Backup 区域中的 BIN 数据复制到 APP 区域
 * @note   擦除 APP 区域后，以 512B 块为单位从 Backup 读取并写入 APP
 * @param  size BIN 文件大小（字节）
 * @retval OTA_OK            复制成功
 * @retval OTA_ERR_FLASH_*   Flash 操作失败
 */
uint8_t OTA_CopyBackupToApp(uint32_t size);

/**
 * @brief  保存 OTA 升级信息到 INFO 区域
 * @note   先擦除 INFO 扇区（全 0），再写入 version 和 date。
 *         request_flag 保持擦除后的 0 值，下次上电不再进入升级。
 * @param  version APP 版本号
 * @param  date   升级时间 (Unix timestamp)
 */
void OTA_SaveInfo(uint32_t version, uint32_t date);

#endif /* __OTA_H */
