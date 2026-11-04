#ifndef _CAN_H_
#define _CAN_H_
#include "SC32F11XX.h"
#include "SC32F10xx_rcc.h"
#include "SC32F10xx_gpio.h"
#include "stdarg.h"

typedef unsigned           int uint32_t;

//CAN模式选择
typedef enum
{
	CAN_normal =0,
	CAN_Listenonly,
	CAN_StandBy,
	CAN_LoopBack,
	CAN_LoopBackIn,
	CAN_LoopBackSACK,
}ModSelect;

//CAN接收缓冲区选择
typedef enum
{
	PTB,
	STB,
}BufType;

//CAN接收溢出丢弃新旧报文选择
typedef enum
{
	OLD,
	NEW,
}FreshState;

//CAN过滤器枚举
typedef enum
{
	Fliter0,
	Fliter1,
	Fliter2,
	Fliter3,
	Fliter4,
	Fliter5,
	Fliter6,
	Fliter7,
}FliterNumber;

//CAN过滤器对标准帧于扩展帧过滤选择
typedef enum
{
	Fliter_standard,
	Fliter_extension,
	None,
}FliterConfig;

//CAN中断枚举
typedef enum
{
	RIE,
	ROIE,
	RFIE,
	RAFIE,
	TPIE,
	TSIE,
	EIE,
	ALIE,
	BEIE,
	EPIE,
}CAN_INTEN;

//CAN时间戳位置选择
typedef enum
{
	SOF,
  EOF,
}TimeStampPosition;

//CAN STB缓冲区发送模式选择
typedef enum
{
	TSONE,
  TSALL,
}CAN_TMOD;

#endif

void CAN_Deinit(CAN_TypeDef *CANx);
void CAN_RESET(CAN_TypeDef *CANx, FunctionalState NewState);
void CAN_Enable(CAN_TypeDef *CANx, FunctionalState NewState);
void CAN_ClockInit(FunctionalState NewState, unsigned int CLK_Div);
void CAN_Init(CAN_TypeDef *CANx);
void IO_INIT(void);
void CAN_TX_Init(CAN_TypeDef *CANx) ;
void CAN_RX_Init(CAN_TypeDef *CANx);
void CAN_R_Abandon(CAN_TypeDef *CANx, FreshState NewState);
void CAN_ITinit(CAN_TypeDef *CANx, FunctionalState INTEN);
void CAN_FDEN(CAN_TypeDef *CANx, FunctionalState FDState);
void CAN_ITConfig(CAN_TypeDef *CANx, int num, ...);
void CAN_TimeStamp(CAN_TypeDef *CANx, FunctionalState TIM_EN, FunctionalState TIMEEN, TimeStampPosition pos);
void CAN_ModeSelect(CAN_TypeDef *CANx, ModSelect CAN_MOD);
void CAN_Baud(CAN_TypeDef *CANx, unsigned int CAN_CLK, unsigned int S_BaudRate, unsigned int S_PRESC, unsigned int S_SJW, unsigned int F_BaudRate, unsigned int F_PRESC, unsigned int F_SJW);
void CAN_FliterSelect(CAN_TypeDef *CANx, char num, ...);
void CAN_FliterConfig(CAN_TypeDef *CANx, FliterConfig CAN_Fliter, char CAN_FliNum, int Acode, int Amask);
void CAN_Buf(CAN_TypeDef *CANx, BufType CAN_Buf);
void CAN_TX_Config(CAN_TX_BUFFER_TypeDef *CAN_TX_BUFx, FunctionalState IDE, unsigned int ID_Num, FunctionalState RTR, FunctionalState FDF, FunctionalState BRS, unsigned int Data_len);
void CAN_TX_DATA(CAN_TypeDef *CANx, CAN_TX_BUFFER_TypeDef *CAN_TX_BUFx, unsigned int DATA_NUM, ...);
void CAN_TX_Trans(CAN_TypeDef *CANx, CAN_TMOD TMOD);
void CAN_RX(void);
void Delay(unsigned int num);
