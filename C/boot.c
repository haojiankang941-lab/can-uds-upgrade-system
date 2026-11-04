/**
 ******************************************************************************
 * @file    boot.c
 * @brief   Bootloader 主模块实现
 * @note    负责系统初始化、主流程控制和 APP 跳转
 ******************************************************************************
 */

#include "boot.h"
#include "ota_flash.h"
#include "iap.h"
#include "W1251.h"

/* ================================================================
 * 外部函数声明
 * ================================================================ */
extern void SystemClock_Config(void);
extern void GPIO_Config(void);
extern void Uart_Config(uint32_t Baud);

/**
 * @brief  Bootloader 初始化
 * @note   配置系统时钟（HIRC）、GPIO、UART0
 *         所有外设就绪后 Bootloader 可以开始工作
 */
void Boot_Init(void)
{
    /* 1. 配置系统时钟：使能 HIRC 作为系统时钟
     *    IAP Flash 操作要求 HIRC 为系统时钟 */
    SystemClock_Config();

    /* 2. 初始化 GPIO（全部输出高，避免浮空） */
    GPIO_Config();

    /* 3. 配置 UART0（用于与 PC 通信接收 BIN） */
    Uart_Config(UART_BAUDRATE);
}

/**
 * @brief  跳转到 APP 区域运行
 * @note   调用芯片 IAP API jump_to_flash()，
 *         通过 IAP_CON 寄存器设置从 APROM 启动并触发系统复位。
 *         此函数不会返回。
 */
void Jump_To_App(void)
{
    if (OTA_IsAppValid() != 0U)
    {
        jump_to_flash();
    }

    /* 没有有效 APP 时停留在 Boot，等待 UDS 恢复刷写 */
    while (1)
    {
        ;
    }
}
