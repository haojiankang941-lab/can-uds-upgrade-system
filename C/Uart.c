#include "main.h"

/* ── 单字节中断收发 ── */
static volatile uint8_t rx_data;
static volatile uint8_t rx_flag = 0;  

void GPIO_Config(void)
{
    GPIO_InitTypeDef GPIO_InitType;
    GPIO_InitType.GPIO_Mode = GPIO_Mode_OUT;
    GPIO_InitType.GPIO_PuPd = GPIO_PuPd_UP;
    GPIO_InitType.GPIO_Pin = 0xffff;
    GPIO_Init(GPIOA, &GPIO_InitType);
    GPIO_Init(GPIOB, &GPIO_InitType);
    GPIO_Init(GPIOC, &GPIO_InitType);
    GPIO_Init(GPIOD, &GPIO_InitType);
    uint16_t i;
    for(i = 0; i < 16; i++)
    {
        PA_BIT(i) = 1;
        PB_BIT(i) = 1;
        PC_BIT(i) = 1;
        PD_BIT(i) = 1;
    }
}

void Uart_Config(uint32_t Baud)
{
    RCC_APB0Cmd(ENABLE);
    RCC_APB0PeriphClockCmd(RCC_APB0Periph_UART0, ENABLE);
    UART0->UART_CON  = UART_CON_TXEN | UART_CON_RXEN | (0x01UL << UART_CON_SM01_Pos);
    UART0->UART_BAUD = 32000000 / Baud;

    /* 全局中断 + RX 中断（TX 中断按字节使能） */
    UART0->UART_IDE  = UART_IDE_INTEN | UART_IDE_RXIE;

    NVIC_EnableIRQ(UART0_2_4_IRQn);
	  RCC->RCC_CFG0 |= 0x008000;//1：中断请求开启
}

void Delay(unsigned int time)
{
    unsigned int i, k;
    for(i = 0; i < time; i++)
        for(k = 0; k < 2000; k++);
}


void Uart_SendByte(uint8_t data)
{    
	  UART0->UART_DATA = data;
  while(!(UART0->UART_STS & UART_STS_TXIF));
	UART0->UART_STS = UART_STS_TXIF;
   
}

/* ── 中断接收一个字节 ── */
uint8_t Uart_GetByte(void)
{
    while (!rx_flag);
    rx_flag = 0;
    return rx_data;
}

/* ── UART0 中断服务 ── */
void UART0_2_IRQHandler(void)
{
    if (UART0->UART_STS & UART_STS_RXIF)
    {
        rx_data = (uint8_t)UART0->UART_DATA;
        rx_flag = 1;
        UART0->UART_STS = UART_STS_RXIF;  
    }
}

void SystemClock_Config(void)
{
    RCC_SYSCLKSWConfig(RCC_CFG0_SYSCLKSW_HIRC);
    RCC_HIRCCmd(ENABLE);
    RCC_HCLKConfig(RCC_SYSCLK_Div1);
	  
}
