/**
  ******************************************************************************
  * @file   SC32F11xx_can.h
  * @author  soc
  * @version V1.0
  * @date    19-10-2023
  * @brief   This file provides firmware functions to manage the following 
  *
  *  @verbatim
  ******************************************************************************
  * @attention
  */

/* Define to prevent recursive inclusion -------------------------------------*/

#ifndef __SC32F11XX_CAN_H
#define __SC32F11XX_CAN_H

#ifdef __cplusplus
 extern "C" {
#endif

/*!< Includes ----------------------------------------------------------------*/
#include "W1251.h"

#define BaudRate_800k 800000
#define BaudRate_500k 500000
#define Sampling_pos_75 0.75
#define Sampling_pos_80 0.8
#define Sampling_pos_875 0.875

/*CAN IDE模式枚举*/
typedef enum
{
	/*报文使用标准模式*/
	CAN_Standard_format = 0x00,
	
	/*报文使用扩展模式*/
	CAN_Extended_format = 0x01,
}CAN_IDEMODE_Typedef;

/*CAN RTR模式枚举*/
typedef enum
{
	/*报文使用数据帧*/
	CAN_Data_frame = 0x00,
	
	/*报文使用远程帧*/
	CAN_Remote_frame = 0x01,
}CAN_RTRMODE_Typedef;

typedef enum
{
	/*报文使用2.0帧*/
	CAN_Standard_frame = 0x00,
	
	/*报文使用FD帧*/
	CAN_FD_frame = 0x01,
}CAN_FDFMODE_Typedef;

typedef enum
{
	/*报文禁止位速率切换*/
	CAN_Disable_BRS = 0x00,
	
	/*报文允许位速率切换*/
	CAN_Enable_BRS = 0x01,
}CAN_BRSMODE_Typedef;

/*PTB自动重传模式选择*/
typedef enum
{
	/*开启PTB自动重传*/
	TPSS_Enable = 0x00,
	
	/*关闭PTB自动重传*/
	TPSS_Disable = 0x01,
}CAN_PTB_AutoRetrans_TypeDef;

/*STB自动重传模式选择*/
typedef enum
{
	/*开启STB自动重传*/
	TSSS_Enable = 0x00,
	
	/*关闭STB自动重传*/
	TSSS_Disable = 0x01,
}CAN_STB_AutoRetrans_TypeDef;

/*CAN发送缓冲区选择*/
typedef enum
{
	/*发送缓冲区选择PTB*/
	CANTBUF_PTB = 0x00,
	
	/*发送缓冲区选择STB*/
	CANTBUF_STB = 0x01,
}CANTBUF_TypeDef;

/*CAN TX使能选择*/
typedef enum
{
	/*不使能CAN发送功能*/
	TX_Disable = 0x00,
	
	/*使能CAN发送功能*/
	TX_Enable = 0x01,
}CANTXEN_TypeDef;

/*CAN RX使能选择*/
typedef enum
{
	/*不使能CAN接收功能*/
	RX_Disable = 0x00,
	
	/*使能CAN接收功能*/
	RX_Enable = 0x01,
}CANRXEN_TypeDef;

/*CAN接收溢出丢弃新旧报文选择*/
typedef enum
{
	/*丢弃最老报文*/
	CANROM_Old = 0x00,
	
	/*丢弃最新报文*/
	CANROM_New = 0x01,
}CANROM_TypeDef;

/*CAN过滤器枚举*/
typedef enum
{
	Fliter0 = 0x00,
	Fliter1 = 0x01,
	Fliter2 = 0x02,
	Fliter3 = 0x03,
	Fliter4 = 0x04,
	Fliter5 = 0x05,
	Fliter6 = 0x06,
	Fliter7 = 0x07,
}FliterNumber;

/*CAN过滤器对标准帧于扩展帧过滤选择*/
typedef enum
{
	/*仅让标准帧通过过滤器*/
	Fliter_Standard = 0x00,
	
	/*仅让扩展帧通过过滤器*/
	Fliter_Extension = 0x01,
	
	/*不设限制*/
	Fliter_Default = 0x02,
}FliterFrame_TypeDef;

/*CAN中断屏蔽开关*/
typedef enum
{
	/*中断屏蔽*/
	CANInt_off = 0x00,
	
	/*中断开启*/
	CANInt_on = 0x01,
}CANInt_TypeDef;

/*CAN发送报文模式选择*/
typedef enum
{
	/*FIFO模式*/
	FIFO = 0x00,
	
	/*ID优先级模式*/
	ID_priority = 0x01,
}CANTSMODE_TypeDef;

/*CAN中断枚举*/
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
}CAN_INT_TypeDef;

//CAN时间戳使能选择
typedef enum
{
	/*不使能时间戳*/
	TimeStampDisable = 0x00,
	
	/*使能时间戳*/
  TimeStampEnable = 0x01,
}CANTimeStamp_TypeDef;

//CAN时间戳计数使能开关
typedef enum
{
	/*关闭时间戳计数功能*/
	TimeStampCntDisable = 0x00,
	
	/*开启时间戳计数功能*/
  TimeStampCntEnable = 0x01,
}CANTimeStampCnt_TypeDef;

//CAN时间戳位置选择
typedef enum
{
	/*选择帧起始作为时间戳位置*/
	CANTimeStampPosition_SOF = 0x00,
	
	/*选择帧结束作为时间戳位置*/
  CANTimeStampPosition_EOF = 0x01,
}CANTimeStampPosition_TypeDef;

//FD格式选择
typedef enum
{
	/*CAN使用2.0模式*/
	FD_Mode_off = 0x01,
	
	/*CAN使用FD模式*/
	FD_Mode_on = 0x00,
}CANFDFrame_TypeDef;

//CAN 发送模式选择
typedef enum
{
	/*PTB单个发送*/
	CAN_TransMod_TPE = 0x00,
	
	/*STB单个发送*/
	CAN_TransMod_TSONE = 0x01,
	
	/*STB全部发送*/
  CAN_TransMod_TSALL = 0x02,
}CAN_TransMod_TypeDef;

//CAN ISO模式选择
typedef enum
{
	/*非ISO模式*/
	CAN_BOSCH = 0x00,
	
	/*ISO模式*/
	CAN_ISO = 0x01,
}CANFD_ISO_MODE_TypeDef;


/*发送数据测试结构*/
typedef struct
{
	uint8_t CAN_T_Observe_IDE;
	uint32_t CAN_T_Observe_TXID;
	uint8_t CAN_T_Observe_RTR;
	uint8_t CAN_T_Observe_FDF;
	uint8_t CAN_T_Observe_BRS;
	uint16_t CAN_T_Observe_DLC;
	uint32_t CAN_T_Observe_DATA[16];
}CAN_Trans_Observe_Typedef;

/*接收数据测试结构*/
typedef struct
{
	uint8_t CAN_R_Observe_IDE;
	uint32_t CAN_R_Observe_TXID;
	uint8_t CAN_R_Observe_RTR;
	uint8_t CAN_R_Observe_FDF;
	uint8_t CAN_R_Observe_BRS;
	uint16_t CAN_R_Observe_DLC;
	uint32_t CAN_R_Observe_DATA[16];
}CAN_Receive_Observe_Typedef;

/*CAN模式选择*/
typedef enum
{
	CAN_Normal =0,
	CAN_Listenonly,
	CAN_StandBy,
	CAN_LoopBack,
	CAN_LoopBackIn,
	CAN_LoopBackSack,
}ModSelect;

/*CAN发送数据结构*/
typedef struct
{
	CAN_IDEMODE_Typedef CAN_IDE;
	uint32_t CAN_TXID;
	CAN_RTRMODE_Typedef CAN_RTR;
	CAN_FDFMODE_Typedef CAN_FDF;
	CAN_BRSMODE_Typedef CAN_BRS;
	uint16_t Data_len;
}CAN_Tx_msg_Typedef;

/*CAN初始化结构*/
typedef struct
{
	CANTXEN_TypeDef CANTXEN;                           //发送使能开关
	
	CANRXEN_TypeDef CANRXEN;                           //接收使能开关
	
	CANTimeStamp_TypeDef CANTimeStamp;                 //时间戳使能开关
	
	CANTimeStampCnt_TypeDef CANTimeStampCnt;           //时间戳计数使能开关
	
	CANTimeStampPosition_TypeDef CANTimeStampPositon;  //时间戳位置选择
	
	CANFDFrame_TypeDef CANFDFrame;                     //FD帧开关
	
	CANROM_TypeDef CANROMSelect;                       //接收溢出丢弃选择
	
	CANTBUF_TypeDef CANRBUFSelect;                     //接收BUF选择
	
	CANTSMODE_TypeDef CANTSMODESelect;                 //发送模式选择
	
	CANFD_ISO_MODE_TypeDef CANFDISOSelect;             //CAN_ISO选择
	
	CAN_PTB_AutoRetrans_TypeDef CAN_PTB_AutoRetrans;   //PTB自动重传选择
	
	CAN_STB_AutoRetrans_TypeDef CAN_STB_AutoRetrans;   //STB自动重传选择
}CAN_InitTypeDef;

/*过滤器结构*/
typedef struct
{
	FliterFrame_TypeDef CANFliterFrame;
	uint32_t Acode;
	uint32_t Amask;
	FliterNumber CAN_Fliter;
}CAN_FliterTypeDef;

/*中断启用结构*/
typedef struct
{
  CANInt_TypeDef CANINTEN;
	CAN_INT_TypeDef CAN_INT;
}CAN_IntTypeDef;

#endif

extern int Tx_msg[];
extern int Tx_msg_len;


void CAN_ResetCmd(CAN_TypeDef *CANx, FunctionalState NewState);
void CAN_Init(CAN_TypeDef *CANx, CAN_InitTypeDef *CAN_InitStruct);
void CAN_RxThresholdConfig(CAN_TypeDef *CANx, int Rth);
void CAN_ITConfig(CAN_TypeDef *CANx, CAN_IntTypeDef *CAN_Int);
void CAN_ModeSelect(CAN_TypeDef *CANx, ModSelect CAN_MOD);
void CAN_SlowBaud_Config(CAN_TypeDef *CANx, unsigned int CAN_CLK, unsigned int S_BaudRate, double S_tq);
void CAN_FastBaud_Config(CAN_TypeDef *CANx, unsigned int CAN_Clk, unsigned int F_BaudRate, double F_tq);
void CAN_FliterInit(CAN_TypeDef *CANx, CAN_FliterTypeDef *CAN_FliterStruct);
void CAN_Trans_Init(CAN_TypeDef *CANx, CAN_TX_BUFFER_TypeDef *CAN_TX_BUFx, CAN_Tx_msg_Typedef *CAN_Tx_msg);
void CAN_Recieve_Config(CAN_TypeDef *CANx);
ITStatus CAN_GetFlagStatus(CAN_TypeDef *CANx, uint32_t CAN_Flag);
void CAN_ClearFlag(CAN_TypeDef *CANx,uint32_t CAN_Flag);

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
