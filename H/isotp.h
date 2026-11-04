/**
 ******************************************************************************
 * @file    isotp.h
 * @brief   ISO-TP (ISO 15765-2) normal addressing, classic CAN
 * @note    Request 0x7E0, response 0x7E8.  Single/multi-frame RX and TX.
 ******************************************************************************
 */

#ifndef __ISOTP_H
#define __ISOTP_H

#include <stdint.h>
#include "boot_can.h"

#define ISOTP_REQ_ID       0x7E0U
#define ISOTP_RESP_ID      0x7E8U

void IsoTp_Init(void);

/* Feed one received CAN frame.  Called from the main loop. */
void IsoTp_RxFrame(const BootCan_FrameTypeDef *frame);

/* Advance receive timeouts.  Call from the main loop. */
void IsoTp_Poll(uint32_t now_ms);

/* Send one complete UDS message (ISO-TP segments it as needed).
 * Returns 0 on success, 1 on timeout/flow-control error. */
uint8_t IsoTp_Send(const uint8_t *data, uint16_t len);

#endif /* __ISOTP_H */
