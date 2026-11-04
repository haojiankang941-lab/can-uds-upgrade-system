/**
 ******************************************************************************
 * @file    boot_can.h
 * @brief   CAN BSP for the UDS bootloader
 * @note    Classic CAN 2.0A, 500 kbps, 11-bit IDs.
 *          Request ID 0x7E0, response ID 0x7E8.
 ******************************************************************************
 */

#ifndef __BOOT_CAN_H
#define __BOOT_CAN_H

#include <stdint.h>

/* CAN bit timing. 16 tq, 81.25% sample point at 32 MHz:
 *   tq = (PRESC+1)/32MHz = 125ns, bit = 16 * 125ns = 2us -> 500 kbps.
 * If the real HIRC is 33.3 MHz, use PRESC=5, SEG1=7, SEG2=1 (11 tq). */
#define BOOT_CAN_CLK_HZ      32000000UL
#define BOOT_CAN_PRESC       3U
#define BOOT_CAN_SEG1        11U
#define BOOT_CAN_SEG2        2U
#define BOOT_CAN_SJW         1U

#define BOOT_CAN_REQ_ID      0x7E0U
#define BOOT_CAN_RESP_ID     0x7E8U
#define BOOT_CAN_RX_RING     16U

typedef struct
{
    uint32_t id;
    uint8_t  dlc;
    uint8_t  data[8];
} BootCan_FrameTypeDef;

/* Initialize CAN, GPIO and the 1 ms SysTick. */
void BootCan_Init(void);

/* Send one classic CAN frame. len is 0..8; the frame is padded to 8 bytes.
 * Returns 0 on success, 1 on timeout. */
uint8_t BootCan_Send(uint16_t id, const uint8_t *data, uint8_t len);

/* Pop one received frame from the ISR ring. Returns 1 when a frame was read. */
uint8_t BootCan_Poll(BootCan_FrameTypeDef *frame);

/* Milliseconds since BootCan_Init(). */
uint32_t BootCan_GetMs(void);

/* Blocking delay in milliseconds (SysTick must be running). */
void BootCan_DelayMs(uint32_t ms);

#endif /* __BOOT_CAN_H */
