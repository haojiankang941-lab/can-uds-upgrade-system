#include "iap.h"
#include "main.h"
#include "W1251.h"
#include "stdio.h"

void IAP_Unlock(void)
{
  if(IAP->IAPKEY == (uint32_t)0x00)
  {
    IAP->IAPKEY =  ((uint32_t)0x12345678);
    IAP->IAPKEY = ((uint32_t)0xA05F05FA);
  }
}

void IAP_Lock(void)
{
  IAP->IAP_CON |= IAP_CON_LOCK;
}

void IAP_WriteCmd(uint8_t NewState)
{

  if (NewState != 0)
  {
    IAP->IAP_CON |= (uint32_t)IAP_CON_PRG;
  }
  else
  {
    IAP->IAP_CON &= ~(uint32_t)IAP_CON_PRG;
  }
}


void IAP_SERASE(uint32_t Add, uint32_t AreaBase)
{
  IAP->IAP_SNB = (((Add - AreaBase) / 0x200) | (0x4C << 24));
  IAP->IAP_CON = 0;
  IAP->IAP_CON |= IAP_CON_SERASE;
  IAP->IAP_CON |= 0x02;
}

void IAP_EraseAll(void)
{
  IAP->IAP_CON |= IAP_CON_ERASE;
  IAP->IAP_CON |= 0x02;
}
/* 字 Flash 编程 */
void IAP_ProgramWord(uint32_t Add, uint32_t Data)
{
    IAP_Unlock();
    IAP->IAP_CON = 0;         /* 清除所有残留位（SERASE等），避免状态冲突 */
    IAP_WriteCmd(ENABLE);
    *((volatile uint32_t*)Add) = Data;
    IAP_Lock();
}


void IAP_TestMode(void) 
{
	uint16_t i;
	Uart_SendByte(0x04);
	IAP_Unlock();
  IAP_WriteCmd(ENABLE);	
	for(i=0;i<10;i++)
	{
		*(uint32_t*)(0x8005000+i*4) = 0x12345678;
	}
		for(i=0;i<50;i++)
	{
		Uart_SendByte(*(uint8_t*)(0x8005000+i));
	}
	IAP_Unlock();
	IAP_SERASE(0x8005000,0x8000000);  
	IAP_Lock();
	Uart_SendByte(0x05);
	for(i=0;i<50;i++)
	{
		Uart_SendByte(*(uint8_t*)(0x8005000+i));
	}
}
void jump_to_flash(void)
{
	IAP_Unlock();
  IAP_WriteCmd(ENABLE);	
	*(uint32_t*)(DESIGNOPTION_BASE-4) = 0xA05F05FA;//解锁
	
		IAP_Unlock();
  IAP_WriteCmd(ENABLE);	
	IAP->IAP_CON &=~ (3<<10);  //从flsh启动
	IAP->IAP_CON |=IAP_CON_RST;
}	

void jump_to_BOOT(void)
{
	IAP_Unlock();
  IAP_WriteCmd(ENABLE);	
	*(uint32_t*)(DESIGNOPTION_BASE-4) = 0xA05F05FA;//解锁
	
		IAP_Unlock();
  IAP_WriteCmd(ENABLE);	
	IAP->IAP_CON &=~ (3<<9);  //
  IAP->IAP_CON |= (1<<9);	//从BOOT启动
	IAP->IAP_CON |=IAP_CON_RST;
}
