/**
  ******************************************************************************
  * @file    SC32F11xx_can.c
  * @author  soc
  * @version V1.0
  * @date    27-10-2023
  * @brief   
  *
  *  @verbatim
  **/

/* Includes ------------------------------------------------------------------*/
#include "SC32F11xx_can.h"



/** @addtogroup SC32F11xx_StdPeriph_Driver
  * @{
  */

/** @defgroup CAN 
  * @brief CAN driver modules
  * @{
  */

/* Private typedef -----------------------------------------------------------*/
/* Private define ------------------------------------------------------------*/
/* Private macro -------------------------------------------------------------*/
/* Private variables ---------------------------------------------------------*/
/* Private function prototypes -----------------------------------------------*/
/* Private functions ---------------------------------------------------------*/

/** @defgroup CAN_Private_Functions 
  * @{
  */

/** @defgroup CAN_Group1 Initialization and Configuration
 *  @brief   Initialization and Configuration
 *
@verbatim
 ===============================================================================
                    ##### Initialization and Configuration #####
 ===============================================================================

@endverbatim
  * @{
  */

/*****************************************************
*函数名称:void CAN_Init(CAN_TypeDef *CANx, CAN_InitTypeDef *CAN_InitStruct)
*函数功能:CAN模块初始化
*入口参数:
CAN_TypeDef:*CANx:选择的CAN模块
CAN_InitTypeDef:*CAN_InitStruct:初始化结构体
*出口参数:void
*****************************************************/
void CAN_Init(CAN_TypeDef *CANx, CAN_InitTypeDef *CAN_InitStruct)
{
	/*CAN发送使能设置*/
	CANx->CAN_IDE |= CAN_InitStruct->CANTXEN<<CAN_IDE_TXEN_Pos;
	/*CAN接收使能设置*/
	CANx->CAN_IDE |= CAN_InitStruct->CANRXEN<<CAN_IDE_RXEN_Pos;
	/*CANFD帧格式设置*/
	CANx->CAN_IDE &= ~(CAN_InitStruct->CANFDFrame<<CAN_IDE_FD_ENABLE_Pos);
	/*时间戳计数使能设置*/
	CANx->CAN_IDE |= CAN_InitStruct->CANTimeStampCnt<<CAN_IDE_TIMEN_Pos;

	/*时间戳使能设置*/
	CANx->CAN_ACFCTRL |= CAN_InitStruct->CANTimeStamp<<CAN_ACFCTRL_TIMEEN_Pos;
	/*时间戳位置设置*/
	CANx->CAN_ACFCTRL |= CAN_InitStruct->CANTimeStampPositon<<CAN_ACFCTRL_TIMEPOS_Pos;

	/*接收区溢出丢弃报文设置*/
	CANx->CAN_CFG_STAT |= CAN_InitStruct->CANROMSelect<<CAN_CFG_STAT_ROM_Pos;
	/*发送缓冲区设置*/
	CANx->CAN_CFG_STAT |= CAN_InitStruct->CANRBUFSelect<<CAN_CFG_STAT_TBSEL_Pos;
	/*ISO设置*/
	CANx->CAN_CFG_STAT |= CAN_InitStruct->CANFDISOSelect<<CAN_CFG_STAT_FD_ISO_Pos;
	/*STB发送模式设置*/
	CANx->CAN_CFG_STAT |= CAN_InitStruct->CANTSMODESelect<<CAN_CFG_STAT_TSMODE_Pos;
	/*PTB自动重传模式设置*/
	CANx->CAN_CFG_STAT |= CAN_InitStruct->CAN_PTB_AutoRetrans<<CAN_CFG_STAT_TPSS_Pos;
	/*STB自动重传模式设置*/
	CANx->CAN_CFG_STAT |= CAN_InitStruct->CAN_STB_AutoRetrans<<CAN_CFG_STAT_TSSS_Pos;
}

/*****************************************************
*函数名称:void CAN_RxThresholdConfig(CAN_TypeDef *CANx, int Rth)
*函数功能:CAN接收阈值设置
*入口参数:
CAN_TypeDef:*CANx:选择的CAN模块
int: Rth:接收阈值大小
*出口参数:void
*****************************************************/
void CAN_RxThresholdConfig(CAN_TypeDef *CANx, int Rth)
{
	CANx->CAN_RTIE |= Rth << CAN_RTIE_AFWL_Pos; 
}

/*****************************************************
*函数名称:void CAN_ITConfig(CAN_TypeDef *CANx, CAN_IntTypeDef *CAN_IntStruct)
*函数功能:CAN中断设置函数
*入口参数:
CAN_TypeDef:*CANx:选择的CAN模块
CAN_IntTypeDef:*CAN_IntStruct:中断结构体
*出口参数:void
*****************************************************/
void CAN_ITConfig(CAN_TypeDef *CANx, CAN_IntTypeDef *CAN_IntStruct)
{
	/*中断屏蔽位设置*/
	CANx->CAN_IDE |= CAN_IntStruct->CANINTEN<<CAN_IDE_INTEN_Pos;

	switch(CAN_IntStruct->CAN_INT)
	{
		case RIE:
		CANx->CAN_RTIE |= CAN_RTIE_RIE;              //接收完毕中断使能
		break;

		case ROIE:
		CANx->CAN_RTIE |= CAN_RTIE_ROIE;             //RB已满中断使能
		break;

		case RFIE:
		CANx->CAN_RTIE |= CAN_RTIE_RFIE;             //RB溢出中断使能
		break;

		case RAFIE:
		CANx->CAN_RTIE |= CAN_RTIE_RAFIE;            //RB slots阈值警告中断使能
		break;

		case TPIE:
		CANx->CAN_RTIE |= CAN_RTIE_TPIE;             //PTB传输完成中断使能
		break;

		case TSIE:
		CANx->CAN_RTIE |= CAN_RTIE_TSIE;             //STB传输完成中断使能
		break;

		case EIE:
		CANx->CAN_RTIE |= CAN_RTIE_EIE;              //错误中断使能
		break;

		case ALIE:
		CANx->CAN_RTIE |= CAN_RTIE_ALIE;             //仲裁丢失错误中断使能
		break;

		case BEIE:
		CANx->CAN_RTIE |= CAN_RTIE_BEIE;             //总线错误中断使能
		break;

		case EPIE:
		CANx->CAN_RTIE |= CAN_RTIE_EPIE;             //错误被动中断使能

		default:
		break;
	}
}

/*****************************************************
*函数名称:void CAN_SlowBaud_Config(CAN_TypeDef *CANx, unsigned int CAN_CLK, unsigned int S_BaudRate, unsigned int S_PRESC, unsigned int S_SJW)
*函数功能:CAN低速通信波特率设置
*入口参数:
CAN_TypeDef:*CANx:选择的CAN模块
unsigned int:CAN_CLK:CAN时钟频率
unsigned int:S_BaudRate:低速波特率
double:S_tq:低速TQ
*出口参数:void
*****************************************************/
void CAN_SlowBaud_Config(CAN_TypeDef *CANx, unsigned int CAN_Clk, unsigned int S_BaudRate, double S_tq)
{
	double Slow_Sampling_pos;
	int S_SEG;
	int S_SEG1;
	int S_SEG2;
	int S_PRESC;
	uint8_t S_SJW;
	S_PRESC = S_tq * CAN_Clk - 1;
	S_SJW = 1;
	/*根据波特率确定取样位置*/
	if(S_BaudRate >= BaudRate_800k)
	{
		Slow_Sampling_pos = Sampling_pos_75;
	}
	else if(S_BaudRate >= BaudRate_500k)
	{
		Slow_Sampling_pos = Sampling_pos_80;
	}
	else
	{
		Slow_Sampling_pos = Sampling_pos_875;
	}
	/*计算SEG1,SEG2*/
	S_SEG = (CAN_Clk/(S_BaudRate * (S_PRESC+1)));
	S_SEG1 = S_SEG*Slow_Sampling_pos - 2;
//	S_SEG1 = S_SEG*Slow_Sampling_pos - 1;
	
	S_SEG2 = S_SEG*(1 - Slow_Sampling_pos) - 1;
	
	/*CAN低速时钟配置寄存器清零*/
	CANx->CAN_S_SEG = 0;

	/*写入低速分频设置*/
	CANx->CAN_S_SEG |= S_PRESC<<CAN_S_PRESC_Pos;
	/*写入低速同步跳变宽度*/
	CANx->CAN_S_SEG |= S_SJW<<CAN_S_SJW_Pos;
	/*写入SEG2*/
	CANx->CAN_S_SEG |= S_SEG2<<CAN_S_SEG_2_Pos;
	/*写入SEG1*/
	CANx->CAN_S_SEG |= S_SEG1<<CAN_S_SEG_1_Pos;
}

/*****************************************************
*函数名称:void CAN_FastBaud_Config(CAN_TypeDef *CANx, unsigned int CAN_Clk, unsigned int F_BaudRate, double F_tq)
*函数功能:CAN高速通信波特率设置
*入口参数:
CAN_TypeDef:*CANx:选择的CAN模块
unsigned int:CAN_CLK:CAN时钟频率
unsigned int:F_BaudRate:高速波特率
double:F_tq:高速TQ
*出口参数:void
*****************************************************/
void CAN_FastBaud_Config(CAN_TypeDef *CANx, unsigned int CAN_Clk, unsigned int F_BaudRate, double F_tq)
{
	double Fast_Sampling_pos;
	int F_SEG;
	int F_SEG1;
	int F_SEG2;
	int F_PRESC;
	uint8_t F_SJW;
	F_PRESC = F_tq * CAN_Clk - 1;
	F_SJW = 1;
	
	/*根据波特率确定取样位置*/
	if(F_BaudRate >= BaudRate_800k)
	{
		Fast_Sampling_pos = Sampling_pos_75;
	}
	else if(F_BaudRate >= BaudRate_500k)
	{
		Fast_Sampling_pos = Sampling_pos_80;
	}
	else
	{
		Fast_Sampling_pos = Sampling_pos_875;
	}
	/*计算SEG1,SEG2*/
	F_SEG = (CAN_Clk/(F_BaudRate * (F_PRESC+1)));
	F_SEG1 = F_SEG*Fast_Sampling_pos - 2;
	F_SEG2 = F_SEG*(1 - Fast_Sampling_pos) - 1;

	/*CAN高速时钟配置寄存器清零*/
	CANx->CAN_F_SEG = 0;
	
	/*写入高速分频设置*/
	CANx->CAN_F_SEG |= F_PRESC<<CAN_F_PRESC_Pos;
	/*写入高速速同步跳变宽度*/
	CANx->CAN_F_SEG |= F_SJW<<CAN_F_SJW_Pos;
	/*写入SEG2*/
	CANx->CAN_F_SEG |= F_SEG2<<CAN_F_SEG_2_Pos;
	/*写入SEG1*/
	CANx->CAN_F_SEG |= F_SEG1<<CAN_F_SEG_1_Pos;
}

/*****************************************************
*函数名称:void CAN_FliterInit(CAN_TypeDef *CANx, CAN_FliterTypeDef *CAN_FliterStruct)
*函数功能:CAN过滤器配置
*入口参数:
CAN_TypeDef:*CANx:选择的CAN模块
CAN_FliterTypeDef:*CAN_FliterStruct:过滤器结构体
*出口参数:void
*****************************************************/
void CAN_FliterInit(CAN_TypeDef *CANx, CAN_FliterTypeDef *CAN_FliterStruct)
{
	/*清零过滤器指针*/
	CANx->CAN_ACFCTRL &= 0xFFFFFF00;
	/*过滤器指针指向要配置的过滤器*/
	CANx->CAN_ACFCTRL |= CAN_FliterStruct->CAN_Fliter<<CAN_ACFCTRL_ACFADR_Pos;

	/*指针指向Acode区域，写入Acode*/
	CANx->CAN_ACFCTRL &= ~(CAN_ACFCTRL_SELMASK);
	/*Acode清零*/
	CANx->CAN_ACF &= 0X0;
	/*写入Acode*/
	CANx->CAN_ACF |= CAN_FliterStruct->Acode<<CAN_ACF_AMASK_ACODE_Pos;

	/*指针指向Amask区，写入Amask*/
	CANx->CAN_ACFCTRL |= CAN_ACFCTRL_SELMASK;
	/*Amask清零*/
	CANx->CAN_ACF &= 0X0;

	/*判断帧过滤类型*/
	switch(CAN_FliterStruct->CANFliterFrame)
	{
		case Fliter_Standard:
		{
			CANx->CAN_ACF |= CAN_ACF_AIDEE;
			CANx->CAN_ACF &= ~(CAN_ACF_AIDE);
		}
		break;

		case Fliter_Extension:
		{
			CANx->CAN_ACF |= CAN_ACF_AIDEE;
			CANx->CAN_ACF |= CAN_ACF_AIDE;
		}
		break;

		case Fliter_Default:
			CANx->CAN_ACF &= ~(CAN_ACF_AIDEE);
		break;

		default:
		break;
	}

	/*写入Amask*/
	CANx->CAN_ACF |= CAN_FliterStruct->Amask<<CAN_ACF_AMASK_ACODE_Pos;;
	/*开启该过滤器*/
	CANx->CAN_ACFCTRL |= 1<<((CAN_FliterStruct->CAN_Fliter)+16);
}

/*****************************************************
*函数名称:void CAN_Trans_Init(CAN_TypeDef *CANx, CAN_TX_BUFFER_TypeDef *CAN_TX_BUFx, CAN_Tx_msg_Typedef CAN_Tx_msg)
*函数功能:CAN报文发送初始化函数
*入口参数:
CAN_TypeDef:*CANx:选择的CAN模块
CAN_TX_BUFFER_TypeDef:*CAN_TX_BUFx:选择的CAN模块TBUF
CAN_Tx_msg_Typedef:CAN_Tx_msg:发送报文结构体
*出口参数:void
*****************************************************/
void CAN_Trans_Init(CAN_TypeDef *CANx, CAN_TX_BUFFER_TypeDef *CAN_TX_BUFx, CAN_Tx_msg_Typedef *CAN_Tx_msg)
{	
	uint8_t index;
	/*获取数据长度值*/
	int len = CAN_Tx_msg->Data_len;
	static int Tx_msg_cnt = 0;
	/*DLC与实际数据长度转化数组*/
	int Reallen[16] = {0,4>>2,4>>2,4>>2,4>>2,8>>2,8>>2,8>>2,8>>2,12>>2,16>>2,20>>2,24>>2,32>>2,48>>2,64>>2};

	if(CAN_Tx_msg->CAN_IDE != CAN_Standard_format)
	{
		/*设置扩展ID*/
		CAN_TX_BUFx->CAN_TX_CTRL |= CAN_TX_CTRL_IDE;
		/*写入ID*/
		CAN_TUF->CAN_TX_ID = CAN_Tx_msg->CAN_TXID;
		CAN_TUF->CAN_TX_ID |= 0X80000000;
	}
	else
	{
		/*设置标准ID*/
		CAN_TX_BUFx->CAN_TX_CTRL &= ~(CAN_TX_CTRL_IDE);
		/*写入ID*/
		CAN_TUF->CAN_TX_ID = CAN_Tx_msg->CAN_TXID;
		CAN_TUF->CAN_TX_ID |= 0X80000000;
	}

	if(CAN_Tx_msg->CAN_RTR != CAN_Data_frame)
	{
		/*设置远程帧*/
		CAN_TX_BUFx->CAN_TX_CTRL |= CAN_TX_CTRL_RTR;
	}
	else
	{
		/*设置数据帧*/
		CAN_TX_BUFx->CAN_TX_CTRL &= ~(CAN_TX_CTRL_RTR);
	}

	if(CAN_Tx_msg->CAN_FDF != CAN_Standard_frame)
	{
		/*设置FD帧*/
		CAN_TX_BUFx->CAN_TX_CTRL |= CAN_TX_CTRL_FDF;
	}
	else
	{
		/*设置2.0帧*/
		CAN_TX_BUFx->CAN_TX_CTRL &= ~(CAN_TX_CTRL_FDF);
	}

	if(CAN_Tx_msg->CAN_BRS != CAN_Disable_BRS)
	{
		/*使能变波特率模式*/
		CAN_TX_BUFx->CAN_TX_CTRL |= CAN_TX_CTRL_BRS;
	}
	else
	{
		/*禁止变波特率模式*/
		CAN_TX_BUFx->CAN_TX_CTRL &= ~(CAN_TX_CTRL_BRS);
	}

	/*写入数据长度*/
	CAN_TX_BUFx->CAN_TX_CTRL |= CAN_Tx_msg->Data_len<<CAN_TX_CTRL_DLC_Pos;


	/*根据实际数据长度写入每帧报文*/
	for(index = 0; index<Reallen[len]; index++)
	{	
		//		CAN_TX_BUFx->CAN_TBUF[index]=(Tx_msg[i][index]<<24)|(Tx_msg[i][index]<<16)|(Tx_msg[i][index]<<8)|(Tx_msg[i][index]&0xFF);
		/*写入报文*/
		CAN_TX_BUFx->CAN_TBUF[index]=Tx_msg[Tx_msg_cnt];
		Tx_msg_cnt++;
		if(Tx_msg_cnt == Tx_msg_len)
		{
			Tx_msg_cnt = 0;
		}
	}

	if((CANx->CAN_CFG_STAT) & (CAN_CFG_STAT_TBSEL))
	{
		/*一帧数据写完，指向下一帧存储区*/
		CANx->CAN_CFG_STAT |= CAN_CFG_STAT_TSNEXT;
	}
}

/*****************************************************
*函数名称:void CAN_Recieve_Config(CAN_TypeDef *CANx)
*函数功能:CAN报文接收设置函数
*入口参数:
CAN_TypeDef:*CANx:选择的CAN模块
*出口参数:void
*****************************************************/
void CAN_Recieve_Config(CAN_TypeDef *CANx)
{
	/*将每个R_SLOT内存储的报文读出并释放该R_SLOT*/
	CANx->CAN_CFG_STAT |= CAN_CFG_STAT_RREL;
}

/** @defgroup CAN_Group2 Interrupts and flags management functions
 *  @brief   Interrupts and flags management functions
 *
@verbatim
 ===============================================================================
           ##### Interrupts and flags management functions #####
 ===============================================================================

@endverbatim
  * @{
  */

/*****************************************************
*函数名称:ITStatus CAN_GetFlagStatus(CAN_TypeDef *CANx, uint32_t CAN_Flag)
*函数功能:CAN中断标志位状态读取
*入口参数:
CAN_TypeDef:*CANx:选择的CAN模块
uint32_t:CAN_Flag:需要读取的标志位
*出口参数:
bitstatus:标志位置起状态
*****************************************************/
ITStatus CAN_GetFlagStatus(CAN_TypeDef *CANx, uint32_t CAN_Flag)
{
	ITStatus bitstatus = RESET;  
	uint16_t itstatus = 0x0;

	/*读取标志位信息*/
	itstatus = CANx->CAN_RTIE & CAN_Flag;
	if (itstatus != (uint16_t)RESET )
	{
		bitstatus = SET;
	}
	else
	{
		bitstatus = RESET;
	}
	return bitstatus;
}


/*****************************************************
*函数名称:void CAN_ClearFlag(CAN_TypeDef *CANx, uint32_t CAN_Flag)
*函数功能:CAN中断标志位清除
*入口参数:
CAN_TypeDef:*CANx:选择的CAN模块
uint32_t:CAN_Flag:需要清除的标志位
*出口参数:void
*****************************************************/
void CAN_ClearFlag(CAN_TypeDef *CANx, uint32_t CAN_Flag)
{
	/*清除标志位*/
	CANx->CAN_RTIE |= CAN_Flag;
}

/** @defgroup CAN_Group3 ModSelect
 *  @brief   ModSelect
 *
@verbatim
 ===============================================================================
                    ##### ModSelect #####
 ===============================================================================

@endverbatim
  * @{
  */

/*****************************************************
*函数名称:void CAN_ModeSelect(CAN_TypeDef *CANx, enum ModType CAN_MOD)
*函数功能:CAN模式选择
*入口参数:
CAN_TypeDef:*CANx:选择的CAN模块
enum ModType CAN_MOD:CAN模式枚举
*出口参数:void
*****************************************************/
void CAN_ModeSelect(CAN_TypeDef *CANx, ModSelect CAN_MOD)
{
	CANx->CAN_CFG_STAT &= ~(CAN_CFG_STAT_RESET);
	CANx->CAN_CFG_STAT |= CAN_CFG_STAT_BUSOFF;
	switch(CAN_MOD)
	{
		/*一般收发模式*/
		case CAN_Normal:
		break;

		/*只听模式*/
		case CAN_Listenonly:
		CANx->CAN_CFG_STAT |= CAN_CFG_STAT_LOM;
//		CANx->CAN_CFG_STAT |= CAN_CFG_STAT_LBME;
		break;

		/*待机模式*/
		case CAN_StandBy:
		CANx->CAN_CFG_STAT |=CAN_CFG_STAT_STBY;
		break;

		/*外部回环模式*/
		case CAN_LoopBack:
		CANx->CAN_CFG_STAT |= CAN_CFG_STAT_LBME;
		break;

		/*内部回环模式*/
		case CAN_LoopBackIn:
		CANx->CAN_CFG_STAT |= CAN_CFG_STAT_LBMI;
		break;

		/*外部回环模式自应答模式*/
		case CAN_LoopBackSack:
		CANx->CAN_CFG_STAT |= CAN_CFG_STAT_LBME;
		CANx->CAN_CFG_STAT |= CAN_CFG_STAT_SACK;
		break;

		default:
		break;
	}
	CANx->CAN_CFG_STAT &= ~(CAN_CFG_STAT_BUSOFF);
	CANx->CAN_CFG_STAT &= ~(CAN_CFG_STAT_RESET);
}

/** @defgroup CAN_Group4 Perihal Reset functions
 *  @brief   Perihal Reset functions
 *
@verbatim
 ===============================================================================
                    ##### Perihal Reset functions #####
 ===============================================================================

@endverbatim
  * @{
  */

/*****************************************************
*函数名称:void CAN_ResetCmd(CAN_TypeDef *CANx, FunctionalState NewState)
*函数功能:CAN复位
*入口参数:
CAN_TypeDef:*CANx:选择的CAN模块
FunctionalState:NewState:复位状态
*出口参数:void
*****************************************************/
void CAN_ResetCmd(CAN_TypeDef *CANx, FunctionalState NewState)
{
	if(NewState != DISABLE)
	{
		CANx->CAN_CFG_STAT |= CAN_CFG_STAT_RESET;
	}
	else
	{
		CANx->CAN_CFG_STAT &= ~(CAN_CFG_STAT_RESET);
	}
}
/**
  * @}
  */

/************************ (C) COPYRIGHT STMicroelectronics *****END OF FILE****/
