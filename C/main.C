#include "main.h"

void SystemInit(void)
{
    SCB->VTOR = FLASH_BASE | 0x0U;
}

int main(void)
{
	char a;
    SystemClock_Config();
    GPIO_Config();
    Uart_Config(9600);

    while(1)
    {
        Uart_SendByte(0x55);
			a=Uart_GetByte();
			 Uart_SendByte(a);
        Delay(10);
    }
}
