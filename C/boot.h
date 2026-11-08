/**
 ******************************************************************************
 * @file    boot.h
 * @brief   Bootloader 主模块头文件
 * @note    负责系统初始化、主流程控制、APP跳转
 ******************************************************************************
 */

#ifndef __BOOT_H
#define __BOOT_H

#include <stdint.h>

/* ================================================================
 * Flash 分区地址宏定义
 * ================================================================ */

#define APP_ADDRESS          0x08000000UL   /* APP 区域起始地址 (256KB)    */
#define APP_SIZE             0x00040000UL   /* APP 区域大小               */

#define BACKUP_ADDRESS       0x08040000UL   /* Backup 区域起始地址 (248KB) */
#define BACKUP_SIZE          0x0003E000UL   /* Backup 区域大小             */

#define INFO_ADDRESS         0x0807E000UL   /* OTA 信息区域起始地址 (4KB)   */
#define INFO_SIZE            0x00001000UL   /* OTA 信息区域大小             */

#define BOOT_ADDRESS         0x08D00000UL   /* Bootloader 区域起始地址      */
#define BOOT_SIZE            0x00001000UL   /* Bootloader 区域大小          */

/* 芯片 Flash 基地址（用于 IAP_SERASE 的 AreaBase 参数） */
#define FLASH_AREA_BASE      0x08000000UL

/* ================================================================
 * 系统配置宏
 * ================================================================ */

#define UART_BAUDRATE        9600UL       /* UART 波特率                 */
#define RAM_BUFFER_SIZE      512U           /* RAM buffer 大小 (= 1 sector) */

/* ================================================================
 * 版本信息（显示在状态屏幕上）
 * ================================================================ */

#define BOOT_VERSION_MAJOR   1U
#define BOOT_VERSION_MINOR   1U
#define BOOT_VERSION_PATCH   0U
#define BOOT_VERSION_STR     "BOOT v1.1.0"
#define BOOT_BUILD_DATE      __DATE__

/* Identification strings returned by 0x22 ReadDataByIdentifier. */
#define BOOT_PART_NUMBER     "W1251-UDS-BOOT"
#define BOOT_HW_VERSION_STR  "V1.0"

/* ================================================================
 * 函数声明
 * ================================================================ */

/**
 * @brief  Bootloader 初始化
 * @note   初始化 UART、GPIO、外设时钟，为 Bootloader 运行做准备
 */
void Boot_Init(void);

/**
 * @brief  跳转到 APP 区域运行
 * @note   调用芯片 IAP API jump_to_flash() 跳转到 0x08000000
 *         此函数不会返回
 */
void Jump_To_App(void);

#endif /* __BOOT_H */
