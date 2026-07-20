#ifndef __MAIN_H
#define __MAIN_H

#include "W1251.h"
#include "SC32F10xx_rcc.h"
#include "SC32F10xx_gpio.h"
#include "stdio.h"
#include "iap.h"

void GPIO_Config(void);
void Uart_Config(uint32_t Baud);
void Delay(unsigned int time);
void SystemClock_Config(void);

/* 单字节中断收发 */
void    Uart_SendByte(char data);
uint8_t Uart_GetByte(void);

#endif /* __MAIN_H */
