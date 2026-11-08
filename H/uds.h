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

/* UI-facing phase of the download.  Read-only snapshot for the status screen;
 * it does not affect the protocol handling. */
typedef enum
{
    UDS_UI_IDLE = 0,
    UDS_UI_PROGRAMMING,
    UDS_UI_ERASING,
    UDS_UI_DOWNLOADING,
    UDS_UI_TRANSFER_DONE,
    UDS_UI_CRC_CHECK,
    UDS_UI_CRC_OK,
    UDS_UI_CRC_FAIL,
    UDS_UI_COPYING,
    UDS_UI_ACTIVATED,
    UDS_UI_ERROR
} UdsUiStateTypeDef;

typedef struct
{
    UdsUiStateTypeDef state;
    uint8_t  session;        /* 01 / 02 / 03 */
    uint8_t  security;       /* 0 = locked, 1 = unlocked */
    uint32_t offset;         /* bytes received in the current download */
    uint32_t size;           /* requested download size, 0 when idle */
} UdsUiStatusTypeDef;

void Uds_Init(void);
void Uds_OnMessage(const uint8_t *msg, uint16_t len);
void Uds_Poll(uint32_t now_ms);

/* Fill *out with the current state/session/security/progress. */
void Uds_GetUiStatus(UdsUiStatusTypeDef *out);

#endif /* __UDS_H */
