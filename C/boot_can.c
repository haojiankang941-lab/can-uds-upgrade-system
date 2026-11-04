/**
 ******************************************************************************
 * @file    boot_can.c
 * @brief   CAN BSP for the UDS bootloader
 * @note    Ported from the vendor demo "CAN v0.1-wy".
 *          CAN controller uses a dedicated 1400-byte message RAM at
 *          0x20007000, outside the 8 KB linker RAM region.
 ******************************************************************************
 */

#include "boot_can.h"
#include "W1251.h"
#include "SC32F11xx_can.h"
#include "SC32F10xx_gpio.h"
#include "SC32F10xx_rcc.h"

/* Vendor library references these demo symbols; we do not use
 * CAN_Trans_Init(), but the symbols must resolve at link time. */
int Tx_msg[1] = {0};
int Tx_msg_len = 1;

/* CAN controller message RAM (same absolute placement as the vendor demo). */
__align(8) uint8_t CAN_SRAM[1400] __attribute__((at(0x20007000)));

#define CAN_SRAM_BASE   0x20007000UL
#define CAN_SRAM_CLEAR  0x1000UL

static volatile BootCan_FrameTypeDef s_ring[BOOT_CAN_RX_RING];
static volatile uint8_t s_head = 0;
static volatile uint8_t s_tail = 0;
static volatile uint32_t s_ms = 0;

/* -------------------------------------------------------------------------- */
/* SysTick                                                                    */
/* -------------------------------------------------------------------------- */

void SysTick_Handler(void)
{
    s_ms++;
}

uint32_t BootCan_GetMs(void)
{
    return s_ms;
}

void BootCan_DelayMs(uint32_t ms)
{
    uint32_t start = s_ms;
    while ((s_ms - start) < ms)
    {
        ;
    }
}

static void BootCan_StartSysTick(void)
{
    SysTick->CTRL = 0;
    SysTick->LOAD = (BOOT_CAN_CLK_HZ / 1000UL) - 1UL;
    SysTick->VAL  = 0;
    SysTick->CTRL = SysTick_CTRL_CLKSOURCE_Msk |
                    SysTick_CTRL_TICKINT_Msk   |
                    SysTick_CTRL_ENABLE_Msk;
}

/* -------------------------------------------------------------------------- */
/* GPIO                                                                       */
/* -------------------------------------------------------------------------- */

static void BootCan_GpioConfig(void)
{
    GPIO_InitTypeDef gpio;

    /* All pins are already outputs from GPIO_Config().  PC6 is the CAN RX
     * input in the vendor demo; the CAN TX pin is driven by the CAN
     * peripheral itself. */
    gpio.GPIO_Pin   = GPIO_Pin_6;
    gpio.GPIO_Mode  = GPIO_Mode_IN;
    gpio.GPIO_PuPd  = GPIO_PuPd_NOPULL;
    GPIO_Init(GPIOC, &gpio);
}

/* -------------------------------------------------------------------------- */
/* CAN initialization                                                         */
/* -------------------------------------------------------------------------- */

void BootCan_Init(void)
{
    uint32_t i;
    CAN_InitTypeDef init;
    CAN_FliterTypeDef filter;
    CAN_IntTypeDef intcfg;

    /* 1. Clear the controller message RAM. */
    for (i = 0; i < CAN_SRAM_CLEAR; i++)
    {
        *(volatile uint8_t *)(CAN_SRAM_BASE + i) = 0x00;
    }

    /* 2. GPIO and CAN clock.  HIRC / HCLK Div1 are configured by
     *    Boot_Init() -> SystemClock_Config(). */
    BootCan_GpioConfig();
    RCC_AHBPeriphClockCmd(RCC_AHBPeriph_CAN, ENABLE);

    /* 3. Point the controller at its message RAM and reset the registers. */
    CAN->CAN_SADDR   = CAN_SRAM_BASE;
    CAN->CAN_CFG_STAT = 0;
    CAN->CAN_IDE      = 0;
    CAN->CAN_RTIE     = 0;
    CAN->CAN_ACFCTRL  = 0;
    CAN->CAN_ACF      = 0;
    CAN->CAN_S_SEG    = 0;
    CAN->CAN_F_SEG    = 0;

    /* 4. Bit timing.  Fields hold (segment length - 1).
     *    32 MHz / ((3+1) * 16) = 500 kbps, sample point 81.25%. */
    CAN->CAN_S_SEG = ((uint32_t)BOOT_CAN_PRESC << CAN_S_PRESC_Pos) |
                     ((uint32_t)BOOT_CAN_SJW   << CAN_S_SJW_Pos)   |
                     ((uint32_t)BOOT_CAN_SEG2  << CAN_S_SEG_2_Pos) |
                     ((uint32_t)BOOT_CAN_SEG1  << CAN_S_SEG_1_Pos);
    /* Fast timing is unused in classic CAN; keep it equal for safety. */
    CAN->CAN_F_SEG = CAN->CAN_S_SEG;

    /* 5. Accept-all filter.  IDs are checked in software by ISO-TP. */
    filter.CANFliterFrame = Fliter_Default;
    filter.Acode          = 0;
    filter.Amask          = 0;
    filter.CAN_Fliter     = Fliter0;
    CAN_FliterInit(CAN, &filter);

    /* 6. Controller configuration: TX/RX on, classic 2.0, PTB, FIFO. */
    init.CANTXEN             = TX_Enable;
    init.CANRXEN             = RX_Enable;
    init.CANTimeStamp        = TimeStampDisable;
    init.CANTimeStampCnt     = TimeStampCntDisable;
    init.CANTimeStampPositon = CANTimeStampPosition_SOF;
    init.CANFDFrame          = FD_Mode_off;
    init.CANROMSelect        = CANROM_New;
    init.CANRBUFSelect       = CANTBUF_PTB;
    init.CANTSMODESelect     = FIFO;
    init.CANFDISOSelect      = CAN_ISO;
    init.CAN_PTB_AutoRetrans = TPSS_Disable;
    init.CAN_STB_AutoRetrans = TSSS_Disable;
    CAN_Init(CAN, &init);

    /* 7. Leave reset and enter normal mode. */
    CAN_ResetCmd(CAN, DISABLE);
    CAN_ModeSelect(CAN, CAN_Normal);

    /* 8. RX interrupts. */
    intcfg.CANINTEN = CANInt_on;
    intcfg.CAN_INT  = RIE;
    CAN_ITConfig(CAN, &intcfg);
    intcfg.CAN_INT  = ROIE;
    CAN_ITConfig(CAN, &intcfg);
    intcfg.CAN_INT  = RFIE;
    CAN_ITConfig(CAN, &intcfg);

    NVIC_SetPriority(CAN_IRQn, 1);
    NVIC_EnableIRQ(CAN_IRQn);

    /* 9. 1 ms time base for ISO-TP / UDS timeouts. */
    BootCan_StartSysTick();
}

/* -------------------------------------------------------------------------- */
/* Transmit                                                                   */
/* -------------------------------------------------------------------------- */

uint8_t BootCan_Send(uint16_t id, const uint8_t *data, uint8_t len)
{
    uint8_t  tmp[8];
    uint8_t  i;
    uint32_t timeout;

    if (len > 8)
    {
        len = 8;
    }

    for (i = 0; i < 8; i++)
    {
        tmp[i] = (i < len) ? data[i] : 0xAA;
    }

    /* Wait for the previous PTB transmission to finish. */
    for (timeout = 0; timeout < 2000000UL; timeout++)
    {
        if ((CAN->CAN_CFG_STAT & CAN_CFG_STAT_TACTIVE) == 0)
        {
            break;
        }
    }
    if (timeout >= 2000000UL)
    {
        return 1;
    }

    /* Select PTB (not STB). */
    CAN->CAN_CFG_STAT &= ~CAN_CFG_STAT_TBSEL;

    CAN_TUF->CAN_TX_ID   = (uint32_t)id | 0x80000000UL;
    CAN_TUF->CAN_TX_CTRL = (uint32_t)8U << CAN_TX_CTRL_DLC_Pos;

    /* Classic CAN: 8 bytes = 2 x 32-bit words (4 bytes per word). */
    for (i = 0; i < 2; i++)
    {
        CAN_TUF->CAN_TBUF[i] = (uint32_t)tmp[i * 4 + 0]
                             | ((uint32_t)tmp[i * 4 + 1] << 8)
                             | ((uint32_t)tmp[i * 4 + 2] << 16)
                             | ((uint32_t)tmp[i * 4 + 3] << 24);
    }

    CAN->CAN_CFG_STAT |= CAN_CFG_STAT_TPE;

    for (timeout = 0; timeout < 2000000UL; timeout++)
    {
        if ((CAN->CAN_CFG_STAT & CAN_CFG_STAT_TACTIVE) == 0)
        {
            break;
        }
    }

    /* Clear the transmit-complete flag (write 1 to clear). */
    CAN->CAN_RTIE |= CAN_RTIE_TPIF;

    return (timeout >= 2000000UL) ? 1 : 0;
}

/* -------------------------------------------------------------------------- */
/* Receive                                                                    */
/* -------------------------------------------------------------------------- */

static void BootCan_PushFrame(uint32_t id, uint32_t ctrl)
{
    uint8_t  next;
    uint8_t  i;
    uint8_t  dlc;
    uint32_t word;

    next = (uint8_t)((s_head + 1U) & (BOOT_CAN_RX_RING - 1U));
    if (next == s_tail)
    {
        return;   /* ring full: drop this frame */
    }

    dlc = (uint8_t)(ctrl & CAN_RX_CTRL_DLC_Msk);
    if (dlc > 8)
    {
        dlc = 8;
    }

    s_ring[s_head].id  = id;
    s_ring[s_head].dlc = dlc;

    /* Classic CAN: 8 bytes = 2 x 32-bit words. */
    for (i = 0; i < 2; i++)
    {
        word = CAN_RUF->CAN_RBUF[i];
        s_ring[s_head].data[i * 4 + 0] = (uint8_t)(word);
        s_ring[s_head].data[i * 4 + 1] = (uint8_t)(word >> 8);
        s_ring[s_head].data[i * 4 + 2] = (uint8_t)(word >> 16);
        s_ring[s_head].data[i * 4 + 3] = (uint8_t)(word >> 24);
    }

    s_head = next;
}

void CAN_IRQHandler(void)
{
    uint32_t flags = CAN->CAN_RTIE;
    uint32_t ctrl;
    uint32_t id;
    uint8_t  n;
    uint32_t wait;

    if (flags & CAN_RTIE_RIF)
    {
        for (n = 0; n < 8; n++)
        {
            if ((CAN->CAN_CFG_STAT & CAN_CFG_STAT_RSTAT_Msk) == 0)
            {
                break;
            }

            for (wait = 0; wait < 10000UL; wait++)
            {
                if ((CAN->CAN_CFG_STAT & CAN_CFG_STAT_RACTIVE) == 0)
                {
                    break;
                }
            }

            id   = CAN_RUF->CAN_RX_ID & CAN_RX_ID_SdID_Msk;
            ctrl = CAN_RUF->CAN_RX_CTRL;

            /* Classic standard data frames only. */
            if ((ctrl & CAN_RX_CTRL_IDE) == 0)
            {
                BootCan_PushFrame(id, ctrl);
            }

            /* Release the slot so the controller can receive the next one. */
            CAN->CAN_CFG_STAT |= CAN_CFG_STAT_RREL;
        }

        CAN->CAN_RTIE |= CAN_RTIE_RIF;
    }

    if (flags & CAN_RTIE_ROIF)
    {
        CAN->CAN_RTIE |= CAN_RTIE_ROIF;
    }
    if (flags & CAN_RTIE_RFIF)
    {
        CAN->CAN_RTIE |= CAN_RTIE_RFIF;
    }
    if (flags & CAN_RTIE_RAFIF)
    {
        CAN->CAN_RTIE |= CAN_RTIE_RAFIF;
    }
    if (flags & CAN_RTIE_TPIF)
    {
        CAN->CAN_RTIE |= CAN_RTIE_TPIF;
    }
    if (flags & CAN_RTIE_BEIF)
    {
        CAN->CAN_RTIE |= CAN_RTIE_BEIF;
    }
    if (flags & CAN_RTIE_ALIF)
    {
        CAN->CAN_RTIE |= CAN_RTIE_ALIF;
    }
    if (flags & CAN_RTIE_EPIF)
    {
        CAN->CAN_RTIE |= CAN_RTIE_EPIF;
    }
}

uint8_t BootCan_Poll(BootCan_FrameTypeDef *frame)
{
    uint8_t i;

    if (s_tail == s_head)
    {
        return 0;
    }

    frame->id  = s_ring[s_tail].id;
    frame->dlc = s_ring[s_tail].dlc;
    for (i = 0; i < 8; i++)
    {
        frame->data[i] = s_ring[s_tail].data[i];
    }

    s_tail = (uint8_t)((s_tail + 1U) & (BOOT_CAN_RX_RING - 1U));
    return 1;
}
