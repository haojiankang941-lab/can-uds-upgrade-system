/**
 ******************************************************************************
 * @file    uds.h
 * @brief   Minimal UDS (ISO 14229) server for the bootloader
 * @note    Services: 0x10, 0x11, 0x27, 0x28, 0x31, 0x34, 0x36, 0x37,
 *          0x3E, 0x85.
 ******************************************************************************
 */

#ifndef __UDS_H
#define __UDS_H

#include <stdint.h>

void Uds_Init(void);
void Uds_OnMessage(const uint8_t *msg, uint16_t len);
void Uds_Poll(uint32_t now_ms);

#endif /* __UDS_H */
