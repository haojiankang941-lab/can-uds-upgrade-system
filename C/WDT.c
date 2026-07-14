#include "main.h"

void BTM_Config(void)
{
	RCC->RCC_CFG0 |= 0X1UL<<1;    //1:LIRC使能    lirc内部的 lxt外部的晶振
	RCC_APB2Cmd(ENABLE);          //开启APB时钟
	
//	BTM->BTM_CON |= 0X0UL<<0;     //中断时间 0~B  F
	
//	BTM->BTM_CON |= 0X40;         //使能BTM中断功能
	BTM->BTM_CON |= 0XC1;         //使能BTM功能	C0 c1~B  F

}
void WDT_Init(void)
{
	  
	  RCCAHB->AHB_CFG =0x04;
	
//	  OPT->OPINX  =0xC2;	
//	  OPT->OPREG  =0x80;
	 
	  #if WDTTime == WDTTime_500ms
    WDT->WDT_CFG = 0x00; //500ms 
    #endif
    
    #if WDTTime == WDTTime_250ms
	  WDT->WDT_CFG = 0x01; //250ms
    #endif
    
    #if WDTTime == WDTTime_125ms
	  WDT->WDT_CFG = 0x02; //125ms
    #endif
    
    #if WDTTime == WDTTime_62ms5
	  WDT->WDT_CFG = 0x03; //62.5ms
    #endif
    
    #if WDTTime == WDTTime_31ms5
	  WDT->WDT_CFG = 0x04; //31.5ms
    #endif
    
    #if WDTTime == WDTTime_15ms75
	  WDT->WDT_CFG = 0x05; //15.75ms
    #endif
    
    #if WDTTime == WDTTime_7ms88
	  WDT->WDT_CFG = 0x06; //7.88ms
    #endif
    
    #if WDTTime == WDTTime_3ms94
	  WDT->WDT_CFG = 0x07; //3.94ms  
    #endif
}

void GPIO_Config(void)
{
	GPIO_InitTypeDef GPIOA_InitStruct = {0};    //定义GPIOA 结构体
	GPIO_InitTypeDef GPIOB_InitStruct = {0};    //定义GPIOB结构体
	GPIO_InitTypeDef GPIOC_InitStruct = {0};    //定义GPIOC结构体
	GPIO_InitTypeDef GPIOD_InitStruct = {0};    //定义GPIOD结构体
	
	GPIOA_InitStruct.GPIO_Pin  = 0xffff;        //全IO口选择
	GPIOB_InitStruct.GPIO_Pin  = 0xffff;        //全IO口选择
	GPIOC_InitStruct.GPIO_Pin  = 0xffff;        //全IO口选择
	GPIOD_InitStruct.GPIO_Pin  = 0xffff;        //全IO口选择
	
	GPIOA_InitStruct.GPIO_Mode = GPIO_Mode_OUT; //输出
//	GPIOA_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;  //设置上拉
//	
	GPIOB_InitStruct.GPIO_Mode = GPIO_Mode_OUT; //输出
//	GPIOB_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;  //设置上拉
//	
	GPIOC_InitStruct.GPIO_Mode = GPIO_Mode_OUT; //输出
//	GPIOC_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;  //设置上拉

//  GPIOD_InitStruct.GPIO_Mode = GPIO_Mode_OUT; //输出
//	GPIOD_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;  //设置上拉	
	
//	GPIOA_InitStruct.GPIO_Mode = GPIO_Mode_IN;  //输入
//	GPIOB_InitStruct.GPIO_Mode = GPIO_Mode_IN;  //输入
//	GPIOC_InitStruct.GPIO_Mode = GPIO_Mode_IN;  //输入
//	GPIOD_InitStruct.GPIO_Mode = GPIO_Mode_IN;  //输入
	
//	GPIOA_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;  //设置上拉
//	GPIOB_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;  //设置上拉
//	GPIOC_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;  //设置上拉
//	GPIOD_InitStruct.GPIO_PuPd = GPIO_PuPd_UP;  //设置上拉

//	GPIOA_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;  //设置高阻
//	GPIOB_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;  //设置高阻
//	GPIOC_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;  //设置高阻
//	GPIOD_InitStruct.GPIO_PuPd = GPIO_PuPd_NOPULL;  //设置高阻

	GPIO_Init(GPIOA,&GPIOA_InitStruct);
	GPIO_Init(GPIOB,&GPIOB_InitStruct);
	GPIO_Init(GPIOC,&GPIOC_InitStruct);	
	GPIO_Init(GPIOD,&GPIOD_InitStruct);
}	

void delay(int16_t time)
{
    int i;
	  int j;
    for(i = time; i > 0; i--)
	  for(j = 100; j > 0; j--)
    {
    //WDT->WDT_CON |= 0x01;   
    }
}	

//延时函数
void Delay(unsigned int time)
{
	unsigned int i,k;
	for(i=0;i<time;i++)
    for(k=0;k<1000;k++);
}


/*BTM中断处理函数*/
void BTM_IRQHandler()
{
	if(BTM->BTM_STS & BTM_STS_BTMIF)//溢出标志判断
	{
		BTM->BTM_STS |= BTM_STS_BTMIF;//清除溢出标志位
		PA_OT(2);
	}
}


