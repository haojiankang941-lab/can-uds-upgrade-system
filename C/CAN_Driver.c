#include "Function.h"
#include "stdarg.h"
char FrameNum = 0;

/******************************************************************报文发送******************************************************************************************/
/*一次发送帧总数*/
#define FrameSum 8 

/*ID标准帧与扩展帧选择，数组内最大存储8帧*/
CAN_IDEMODE_Typedef IDEMODE[FrameSum] = {CAN_Standard_format, CAN_Extended_format, CAN_Standard_format, CAN_Standard_format, CAN_Extended_format, CAN_Standard_format, CAN_Standard_format, CAN_Extended_format};

/*ID号，数组内最大存储8帧*/
uint32_t ID[FrameSum] = {0x333, 0x333, 0x025, 0x111, 0xCCC0000, 0x333, 0x888, 0x889};

/*数据帧与远程帧选择，数组内最大存储8帧*/
CAN_RTRMODE_Typedef RTRMODE[FrameSum] = {CAN_Data_frame, CAN_Data_frame ,CAN_Data_frame , CAN_Data_frame, CAN_Data_frame, CAN_Remote_frame, CAN_Data_frame ,CAN_Data_frame};

/*FD帧模式选择，数组内最大存储8帧*/
CAN_FDFMODE_Typedef FDFMODE[FrameSum] = {CAN_FD_frame, CAN_FD_frame, CAN_FD_frame, CAN_FD_frame, CAN_FD_frame, CAN_FD_frame, CAN_FD_frame, CAN_FD_frame};

/*速率切换选择，数组内最大存储8帧*/
CAN_BRSMODE_Typedef BRSMODE[FrameSum] = {CAN_Enable_BRS, CAN_Enable_BRS, CAN_Enable_BRS, CAN_Enable_BRS, CAN_Enable_BRS, CAN_Enable_BRS, CAN_Enable_BRS, CAN_Enable_BRS};

/*数据长度，数组内最大存储8帧*/
uint16_t Data_length[FrameSum] = {FD_DLC8, FD_DLC8, FD_DLC12, FD_DLC4, FD_DLC8, FD_DLC16, FD_DLC8, FD_DLC8};

/*发送数据，最大8帧，每帧内最多存储16个32位数据*/
int Tx_msg[] = {
										0x12345678, 0x09876543,  //一次发送的第一帧数据
										0x22222222, 0x22222222, //一次发送的第二帧数据
										0x33333333, 0x33333333, 0x33333333,  //一次发送的第三帧数据
										0x44444444,  //一次发送的第四帧数据
										0x55555555, 0x55555555,  //一次发送的第五帧数据
										0x66666666, 0x66666666, 0x66666666, 0x66666666,  //一次发送的第六帧数据
										0x77777777, 0x77777777,  //一次发送的第七帧数据
										0x88888888, 0x88888888  //一次发送的第八帧数据
								};
/*计算输入数据长度*/
int Tx_msg_len = sizeof(Tx_msg)/sizeof(int);
/********************************************************************************************************************************************************************/

/********************************************************************过滤器******************************************************************************************/
/*开启的过滤器总数*/
#define FliterEnableSum  8
char FliterSum = FliterEnableSum;
/*过滤器的帧过滤设置*/
FliterFrame_TypeDef CAN_FliterFrame[FliterEnableSum] = { Fliter_Standard, Fliter_Extension , Fliter_Extension , Fliter_Extension , Fliter_Extension ,Fliter_Extension , Fliter_Extension ,Fliter_Extension };

/*过滤器的Acode值*/
uint32_t Acode[FliterEnableSum] = {0x212, 0x123, 0x234, 0x100, 0x200, 0x300, 0x400, 0x500};

/*过滤器的Amask值*/
uint32_t Amask[FliterEnableSum] = {0x5A0, 0x5A0, 0x5A0, 0x5A0, 0x5A0, 0x5A0, 0x5A0, 0x5A0};

/*选择开启的过滤器序号*/
FliterNumber CAN_Fliter[FliterEnableSum] = { Fliter0, Fliter1 ,Fliter2 , Fliter3, Fliter4 ,Fliter5 , Fliter6, Fliter7 };
/********************************************************************************************************************************************************************/

/********************************************************************中断开关****************************************************************************************/
/*选择需要开启的中断*/
CAN_INT_TypeDef CAN_INT[] = {TSIE};

/*计算中断开启个数*/
int CAN_Int_num = sizeof(CAN_INT)/sizeof(CAN_INT[0]);
/********************************************************************************************************************************************************************/

CAN_Trans_Observe_Typedef CAN_Trans_Observe;
CAN_Receive_Observe_Typedef CAN_Receive_Observe;


/*****************************************************
*函数名称:void CAN_Trans(CAN_TypeDef *CANx, CANTBUF_TypeDef CANTSMODESelect, CAN_TransMod_TypeDef TransMod)
*函数功能:CAN报文发送函数
*入口参数:
CAN_TypeDef:*CANx:选择的CAN模块
CANTBUF_TypeDef:CANTSMODESelect:发送缓冲区选择
CAN_Trans_Mod_TypeDef:TransMod:传输模式选择
*出口参数:void
*****************************************************/
void CAN_Trans(CAN_TypeDef *CANx, CANTBUF_TypeDef CANTSMODESelect, CAN_TransMod_TypeDef TransMod)
{
	/*发送缓冲区选择*/
	switch(CANTSMODESelect)
	{
		/*选择PTB作为发送缓冲区*/
		case CANTBUF_PTB:
			CANx->CAN_CFG_STAT &= ~(CAN_CFG_STAT_TBSEL);
		/*发送报文初始化*/
			CAN_Trans_Config(CANx, CAN_TUF);
		break;
		
		/*选择STB作为发送缓冲区*/
		case CANTBUF_STB:
			CANx->CAN_CFG_STAT |= CAN_CFG_STAT_TBSEL;
			for(FrameNum=0;FrameNum<FrameSum;FrameNum++)
			{
				/*发送报文初始化*/
				CAN_Trans_Config(CANx, CAN_TUF);
			}		
		break;
	}

	/*选择发送模式*/
	switch(TransMod)
	{
		/*PTB模式下报文传输*/
		case CAN_TransMod_TPE:
		{
			CANx->CAN_CFG_STAT |= CAN_CFG_STAT_TPE;
//			CAN->CAN_CFG_STAT |= 0X800;  //终止发送
			while(CAN->CAN_CFG_STAT & CAN_CFG_STAT_TACTIVE);			
			break;
		}
		
		/*STB模式下一次一帧报文传输*/
		case CAN_TransMod_TSONE:
		/*若T_slot写满则发送*/
		{
			if((CANx->CAN_CFG_STAT&CAN_CFG_STAT_TSSTAT) == TSSTAT_Full)
			{
				for(FrameNum=0;FrameNum<FrameSum;FrameNum++)
				{
					CANx->CAN_CFG_STAT |= CAN_CFG_STAT_TSONE;
					/*等待发送结束*/
					while(CAN->CAN_CFG_STAT & CAN_CFG_STAT_TACTIVE)
					{
						PC_BIT(0) = 1;
					}
					PC_BIT(0) = 0;
					if(FrameNum == 5)
					{
//					CAN->CAN_CFG_STAT |= 0X100; //终止发送
						PC_BIT(0) = 1;
					}
					delay100ms();
				}
			}
			break;
		}


		/*STB模式下一次所有报文传输*/
		case CAN_TransMod_TSALL:
			/*若T_slot写满则发送*/
		{
			if((CANx->CAN_CFG_STAT&CAN_CFG_STAT_TSSTAT) == TSSTAT_Full)
			{
				CANx->CAN_CFG_STAT |= CAN_CFG_STAT_TSALL;
			}
			/*等待发送结束*/
			while(CAN->CAN_CFG_STAT & CAN_CFG_STAT_TACTIVE)
			{
				PC_BIT(0) = 1;
			}
//			CAN->CAN_CFG_STAT |= 0X100; //终止发送
			PC_BIT(0) = 0;
			delay100ms();
			break;
		}

		default:
		break;
	}
}

/*****************************************************
*函数名称:void CAN_Trans_Config(CAN_TypeDef *CANx, CAN_TX_BUFFER_TypeDef *CAN_TX_BUFx)
*函数功能:CAN报文发送配置
*入口参数:
CAN_TypeDef:*CANx:选择的CAN模块
CAN_TX_BUFFER_TypeDef:*CAN_TX_BUFx:选择的CAN模块TBUF
*出口参数:void
*****************************************************/
void CAN_Trans_Config(CAN_TypeDef *CANx, CAN_TX_BUFFER_TypeDef *CAN_TX_BUFx)
{
	CAN_TX_BUFx->CAN_TX_ID = 0x00000000;  //ID清零
	CAN_TX_BUFx->CAN_TX_CTRL = 0x00000000;  //CTRL寄存器清零

	CAN_Tx_msg_Typedef CAN_Tx_msg;
	/*设置标准或扩展ID*/
	CAN_Tx_msg.CAN_IDE = IDEMODE[FrameNum];
	/*写入ID*/
	CAN_Tx_msg.CAN_TXID = ID[FrameNum];
	/*设置数据或远程帧*/
	CAN_Tx_msg.CAN_RTR = RTRMODE[FrameNum];
	/*设置FD帧或标准帧*/
	CAN_Tx_msg.CAN_FDF = FDFMODE[FrameNum];
	/*设置是否可变波特率*/
	CAN_Tx_msg.CAN_BRS = BRSMODE[FrameNum];
	/*设置数据长度*/
	CAN_Tx_msg.Data_len = Data_length[FrameNum];
	/*发送报文初始化*/
	CAN_Trans_Init(CAN, CAN_TUF, &CAN_Tx_msg);
}

/*****************************************************
*函数名称:void CAN_Receive(CAN_TypeDef *CANx)
*函数功能:CAN报文接收函数
*入口参数:
CAN_TypeDef:*CANx:选择的CAN模块
*出口参数:void
*****************************************************/
void CAN_Receive(CAN_TypeDef *CANx)
{
	while(CANx->CAN_CFG_STAT&CAN_CFG_STAT_RACTIVE)
	{
		PC_BIT(1) = 1;
	}
	PC_BIT(1) = 0;
	/*检测8个R_SLOT是否存满*/
	if((CANx->CAN_CFG_STAT & CAN_CFG_STAT_RSTAT) == RSTAT_Full)
	{
		char r_slot;
		for(r_slot=0;r_slot<8;r_slot++)
		{
			/*将每个R_SLOT内存储的报文读出并释放该R_SLOT*/
			CAN_Recieve_Config(CAN);
		}
	}
}

/*****************************************************
*函数名称:void CAN_normal_Test()
*函数功能:CAN一般收发模式
*入口参数:void
*出口参数:void
*****************************************************/
void CAN_Normal_Test()
{
	/*设置接收缓存阈值为 1*/
	CAN_RxThresholdConfig(CAN, 0x2);
	
	CAN->CAN_RTIE &= ~(0XF000000);
	CAN->CAN_RTIE |= 0X7000000;
}

/*****************************************************
*函数名称:void CAN_Listenonly_Test()
*函数功能:CAN只听模式
*入口参数:void
*出口参数:void
*****************************************************/
void CAN_Listenonly_Test()
{
	CAN_ModeSelect(CAN, CAN_Listenonly);
}

/*****************************************************
*函数名称:void CAN_StandBy_Test()
*函数功能:CAN待机模式
*入口参数:void
*出口参数:void
*****************************************************/
void CAN_StandBy_Test()
{
	CAN_ModeSelect(CAN, CAN_StandBy); 
}

/*****************************************************
*函数名称:void CAN_LoopBack_Test()
*函数功能:CAN外部回环模式
*入口参数:void
*出口参数:void
*****************************************************/
void CAN_LoopBack_Test()
{
	CAN_ModeSelect(CAN, CAN_LoopBack); 
}

/*****************************************************
*函数名称:void CAN_LoopBackIn_Test()
*函数功能:CAN内部回环模式
*入口参数:void
*出口参数:void
*****************************************************/
void CAN_LoopBackIn_Test()
{
	CAN_ModeSelect(CAN, CAN_LoopBackIn); 
}

/*****************************************************
*函数名称:void CAN_LoopBackSack_Test()
*函数功能:CAN外部回环自应答模式
*入口参数:void
*出口参数:void
*****************************************************/
void CAN_LoopBackSack_Test()
{
	CAN_ModeSelect(CAN, CAN_LoopBackSack); 
}

/*****************************************************
*函数名称:void CAN_TransTest(void)
*函数功能:CAN发送测试代码
*入口参数:void
*出口参数:void
*****************************************************/
/*测试部分代码，方便观察发送数据*/
void CAN_TransTest(void)
{
	char T_Test;
	CAN_Trans_Observe.CAN_T_Observe_IDE = CAN_TUF->CAN_TX_CTRL>>7&0X1;
	CAN_Trans_Observe.CAN_T_Observe_TXID = CAN_TUF->CAN_TX_ID;
	CAN_Trans_Observe.CAN_T_Observe_RTR = CAN_TUF->CAN_TX_CTRL>>6&0X1;
	CAN_Trans_Observe.CAN_T_Observe_FDF = CAN_TUF->CAN_TX_CTRL>>5&0X1;
	CAN_Trans_Observe.CAN_T_Observe_BRS = CAN_TUF->CAN_TX_CTRL>>4&0X1;
	CAN_Trans_Observe.CAN_T_Observe_DLC = CAN_TUF->CAN_TX_CTRL&0XA;
	for(T_Test = 0;T_Test<16;T_Test++)
	{
		CAN_Trans_Observe.CAN_T_Observe_DATA[T_Test] = CAN_TUF->CAN_TBUF[T_Test];
	}
}

/*****************************************************
*函数名称:void CAN_ReceiveTest(void)
*函数功能:CAN接收测试代码
*入口参数:void
*出口参数:void
*****************************************************/
void CAN_ReceiveTest(void)
{
	char R_Test;
	CAN_Receive_Observe.CAN_R_Observe_IDE = CAN_RUF->CAN_RX_CTRL>>7&0X1;
	CAN_Receive_Observe.CAN_R_Observe_TXID = CAN_RUF->CAN_RX_ID;
	CAN_Receive_Observe.CAN_R_Observe_RTR = CAN_RUF->CAN_RX_CTRL>>6&0X1;
	CAN_Receive_Observe.CAN_R_Observe_FDF = CAN_RUF->CAN_RX_CTRL>>5&0X1;
	CAN_Receive_Observe.CAN_R_Observe_BRS = CAN_RUF->CAN_RX_CTRL>>4&0X1;
	CAN_Receive_Observe.CAN_R_Observe_DLC = CAN_RUF->CAN_RX_CTRL&0XA;
	for(R_Test = 0;R_Test<16;R_Test++)
	{
		CAN_Receive_Observe.CAN_R_Observe_DATA[R_Test] = CAN_RUF->CAN_RBUF[R_Test];
	}
}
/*测试部分代码，方便观察发送数据*/

/*****************************************************
*函数名称:void CAN_Init_HardWare(void)
*函数功能:CAN硬件初始化函数
*入口参数:void
*出口参数:void
*****************************************************/
void CAN_Init_HardWare(void)
{
	/*时钟设置*/
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CAN, ENABLE);
	/*选择HIRC作为时钟源 */
	RCC_SYSCLKSWConfig(RCC_CFG0_SYSCLKSW_HIRC);   
	/*使能HIRC*/
	RCC_HIRCCmd(ENABLE);
	/*时钟分频设置*/
	RCC_HCLKConfig(RCC_SYSCLK_Div1);

	/*设置波特率，SYSCLK系统时钟频率，S_Baud慢速时钟波特率，S_PRESC慢速波特率PRESC值，S_SJW慢速波特率SJW*/
	CAN_SlowBaud_Config(CAN, SYSCLK_32M, S_Baud_125k, S_TQ_500ns);
//	/*CAN低速时钟配置寄存器清零*/
//	CAN->CAN_S_SEG = 0;

//	/*写入低速分频设置*/
//	CAN->CAN_S_SEG |= 0X2<<CAN_S_PRESC_Pos;
//	/*写入低速同步跳变宽度*/
//	CAN->CAN_S_SEG |= 0X1<<CAN_S_SJW_Pos;
//	/*写入SEG2*/
//	CAN->CAN_S_SEG |= 0X2<<CAN_S_SEG_2_Pos;
//	/*写入SEG1*/
//	CAN->CAN_S_SEG |= 0X2<<CAN_S_SEG_1_Pos;

	/*设置波特率，SYSCLK系统时钟频率，F_Baud快速时钟波特率，F_PRESC快速波特率PRESC值，F_SJW快速波特率SJW，仅在FD帧中启用*/
	CAN_FastBaud_Config(CAN, SYSCLK_32M, F_Baud_125k, F_TQ_500ns);
	
//	/*CAN高速时钟配置寄存器清零*/
//	CAN->CAN_F_SEG = 0;
	
//	/*写入高速分频设置*/
//	CAN->CAN_F_SEG |= 0X0<<CAN_F_PRESC_Pos;
//	/*写入高速速同步跳变宽度*/
//	CAN->CAN_F_SEG |= 0X1<<CAN_F_SJW_Pos;
//	/*写入SEG2*/
//	CAN->CAN_F_SEG |= 0X8<<CAN_F_SEG_2_Pos;
//	/*写入SEG1*/
//	CAN->CAN_F_SEG |= 0X2<<CAN_F_SEG_1_Pos;

	CAN->CAN_SADDR = 0X20007000;
	/*CAN过滤器设置*/
	CAN_FliterConfig();

	/*结构初始化*/
	CAN_StructInit();

	/*CAN中断设置*/
	CAN_ITInit();
}


/*****************************************************
*函数名称:void GPIO_Config(void)
*函数功能:GPIO初始化函数
*入口参数:void
*出口参数:void
*****************************************************/
void GPIO_Config(void)
{
	GPIO_InitTypeDef GPIOA_InitStruct;    //定义GPIOA 结构体
	GPIO_InitTypeDef GPIOB_InitStruct;    //定义GPIOB结构体
	GPIO_InitTypeDef GPIOC_InitStruct;    //定义GPIOC结构体
	GPIO_InitTypeDef GPIOD_InitStruct;    //定义GPIOC结构体

	GPIOA_InitStruct.GPIO_Pin  = 0xffff;        //全IO口选择
	GPIOB_InitStruct.GPIO_Pin  = 0xffff;        //全IO口选择
	GPIOC_InitStruct.GPIO_Pin  = 0xffbf;        //全IO口选择
	GPIOD_InitStruct.GPIO_Pin  = 0xffff;        //全IO口选择

	GPIOA_InitStruct.GPIO_Mode = GPIO_Mode_OUT; //输出	
	GPIOB_InitStruct.GPIO_Mode = GPIO_Mode_OUT; //输出	
	GPIOC_InitStruct.GPIO_Mode = GPIO_Mode_OUT; //输出
	GPIOD_InitStruct.GPIO_Mode = GPIO_Mode_OUT; //输出

	GPIO_Init(GPIOA,&GPIOA_InitStruct);
	GPIO_Init(GPIOB,&GPIOB_InitStruct);
	GPIO_Init(GPIOC,&GPIOC_InitStruct);	
	GPIO_Init(GPIOD,&GPIOD_InitStruct);	

	/*CAN_TXRX管脚都设置上拉输入*/
	GPIO_InitTypeDef CAN_RX_In;
	CAN_RX_In.GPIO_Pin = GPIO_Pin_6 ;
	CAN_RX_In.GPIO_Mode = GPIO_Mode_IN;
	GPIO_Init(GPIOC,&CAN_RX_In);
}

/*****************************************************
*函数名称:void CAN_StructInt(void)
*函数功能:CAN结构初始化
*入口参数:void
*出口参数:void
*****************************************************/
void CAN_StructInit(void)
{
	/*CAN初始化*/
	CAN_InitTypeDef CAN_InitStruct;
/************************************************************收发使能设置**********************************************************************/
	/*开启发送使能*/
	CAN_InitStruct.CANTXEN = TX_Enable;
	/*开启接收使能*/
	CAN_InitStruct.CANRXEN = RX_Enable;
/**********************************************************************************************************************************************/

/************************************************************时间戳设置************************************************************************/
	/*使能时间戳*/
	CAN_InitStruct.CANTimeStamp = TimeStampEnable;
	/*开启时间戳计数功能*/
	CAN_InitStruct.CANTimeStampCnt = TimeStampCntEnable;
	/*帧位置选择 SOF帧起始 EOF帧结束*/
	CAN_InitStruct.CANTimeStampPositon = CANTimeStampPosition_SOF;
/**********************************************************************************************************************************************/

/************************************************************收发结构设置***********************************************************************/
	/*开启FD帧*/
	CAN_InitStruct.CANFDFrame = FD_Mode_on;
	/*接收溢出丢弃最老的帧*/
	CAN_InitStruct.CANROMSelect = CANROM_New;
	/*使用ISO模式*/
	CAN_InitStruct.CANFDISOSelect = CAN_ISO;
	/*发送缓冲区选用STB*/
	CAN_InitStruct.CANRBUFSelect = CANTBUF_STB;
	/*STB发送缓冲区选择FIFO模式*/
	CAN_InitStruct.CANTSMODESelect = FIFO;
/**********************************************************************************************************************************************/

/************************************************************自动重传设置***********************************************************************/
	/*PTB自动重传使能*/
	CAN_InitStruct.CAN_PTB_AutoRetrans = TPSS_Disable;
	/*STB自动重传使能*/
	CAN_InitStruct.CAN_STB_AutoRetrans = TSSS_Disable;
/**********************************************************************************************************************************************/

	/*初始化函数*/
	CAN_Init(CAN, &CAN_InitStruct);
}

/*****************************************************
*函数名称:void CAN_ITInit(void)
*函数功能:CAN中断设置
*入口参数:void
*出口参数:void
*****************************************************/
void CAN_ITInit(void)
{
	CAN->CAN_RTIE &= ~(0xFE);
	char Intnum;
	for(Intnum = 0;Intnum<CAN_Int_num;Intnum++)
	{
		CAN_IntTypeDef CAN_IntStruct;
		/*关闭中断屏蔽*/
		CAN_IntStruct.CANINTEN = CANInt_on;
		/*选择需要开启的子中断*/
		CAN_IntStruct.CAN_INT = CAN_INT[Intnum];
		/*中断设置函数*/
		CAN_ITConfig(CAN, &CAN_IntStruct);
	}
	NVIC_EnableIRQ(CAN_IRQn);
}

/*****************************************************
*函数名称:void CAN_FliterConfig(void)
*函数功能:CAN过滤器设置
*入口参数:void
*出口参数:void
*****************************************************/
void CAN_FliterConfig(void)
{
	/*过滤器设置*/
	char Fliternum;
	/*开启FliterSum个过滤器，并对FliterSum个过滤器进行设置*/
	for(Fliternum = 0;Fliternum<FliterSum;Fliternum++)
	{
		CAN_FliterTypeDef CAN_FliterStruct;
		/*设置过滤帧类型*/
		CAN_FliterStruct.CANFliterFrame = CAN_FliterFrame[Fliternum];
		/*设置Acode*/
		CAN_FliterStruct.Acode = Acode[Fliternum];
		/*设置Amask*/
		CAN_FliterStruct.Amask = Amask[Fliternum];
		/*选择开启的过滤器序号*/
		CAN_FliterStruct.CAN_Fliter = CAN_Fliter[Fliternum];
		/*过滤器配置函数*/
		CAN_FliterInit(CAN, &CAN_FliterStruct);
	}
}

/*****************************************************
*函数名称:void delay100ms(void)  
*函数功能:延迟函数100ms
*入口参数:void
*出口参数:void
*****************************************************/
void delay100ms(void)  
{
    unsigned char a,b,c;
    for(c=34;c>0;c--)
        for(b=245;b>0;b--)
            for(a=15;a>0;a--);
}

/*****************************************************
*函数名称:void CAN_Close(CAN_TypeDef *CANx)
*函数功能:CAN关闭函数
*入口参数:
CAN_TypeDef:*CANx:选择的CAN模块
*出口参数:void
*****************************************************/
void CAN_Close(CAN_TypeDef *CANx)
{
	CANx->CAN_F_SEG = 0;
	CANx->CAN_S_SEG = 0;
	RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CAN, DISABLE);
}


