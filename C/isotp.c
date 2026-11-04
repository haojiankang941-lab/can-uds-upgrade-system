/**
 ******************************************************************************
 * @file    isotp.c
 * @brief   ISO-TP (ISO 15765-2) normal addressing over classic CAN
 * @note    Request ID 0x7E0, response ID 0x7E8.
 *          RX buffer 258 bytes (one 0x36 request: SID + BSC + 256 data).
 ******************************************************************************
 */

#include "isotp.h"
#include "uds.h"
#include <string.h>

#define ISOTP_RX_MAX        258U
#define ISOTP_TX_MAX        64U
#define ISOTP_PAD           0xAAU

#define ISOTP_N_CR_MS       1000U
#define ISOTP_N_BS_MS       1000U
#define ISOTP_STMIN_MS      0U

#define ISOTP_STATE_IDLE    0U
#define ISOTP_STATE_WAIT_CF 1U

static uint8_t  s_rx_buf[ISOTP_RX_MAX];
static uint16_t s_rx_len;
static uint16_t s_rx_pos;
static uint8_t  s_rx_sn;
static uint8_t  s_rx_state;
static uint32_t s_rx_deadline;

void IsoTp_Init(void)
{
    s_rx_len      = 0;
    s_rx_pos      = 0;
    s_rx_sn       = 1;
    s_rx_state    = ISOTP_STATE_IDLE;
    s_rx_deadline = 0;
}

/* -------------------------------------------------------------------------- */
/* Flow control                                                               */
/* -------------------------------------------------------------------------- */

static void IsoTp_SendFC(uint8_t flow_status, uint8_t block_size, uint8_t stmin)
{
    uint8_t frame[3];

    frame[0] = (uint8_t)(0x30U | (flow_status & 0x0FU));
    frame[1] = block_size;
    frame[2] = stmin;

    (void)BootCan_Send(ISOTP_RESP_ID, frame, 3);
}

/* Wait for a flow-control frame from the tester on the request ID.
 * Returns 0 and fills block_size/stmin on CTS, 1 on timeout/overflow. */
static uint8_t IsoTp_WaitFC(uint8_t *block_size, uint8_t *stmin)
{
    uint32_t start = BootCan_GetMs();
    BootCan_FrameTypeDef f;

    while ((BootCan_GetMs() - start) < ISOTP_N_BS_MS)
    {
        while (BootCan_Poll(&f))
        {
            if (f.id == ISOTP_REQ_ID && f.dlc >= 3 && ((f.data[0] >> 4) & 0x0F) == 3)
            {
                uint8_t fs = (uint8_t)(f.data[0] & 0x0FU);

                if (fs == 0)          /* CTS */
                {
                    *block_size = f.data[1];
                    *stmin      = f.data[2];
                    return 0;
                }
                if (fs == 1)          /* WAIT: restart N_Bs */
                {
                    start = BootCan_GetMs();
                }
                else                  /* OVFLW / invalid */
                {
                    return 1;
                }
            }
        }
    }

    return 1;
}

/* -------------------------------------------------------------------------- */
/* Transmit                                                                   */
/* -------------------------------------------------------------------------- */

uint8_t IsoTp_Send(const uint8_t *data, uint16_t len)
{
    uint8_t  frame[8];
    uint16_t pos;
    uint8_t  sn;
    uint8_t  n;
    uint8_t  bs;
    uint8_t  stmin;
    uint8_t  block_count;

    if (len == 0)
    {
        return 1;
    }

    /* Single frame. */
    if (len <= 7U)
    {
        frame[0] = (uint8_t)len;
        memcpy(&frame[1], data, len);
        return BootCan_Send(ISOTP_RESP_ID, frame, (uint8_t)(len + 1U));
    }

    /* First frame: 12-bit length. */
    frame[0] = (uint8_t)(0x10U | ((len >> 8) & 0x0FU));
    frame[1] = (uint8_t)(len & 0xFFU);
    memcpy(&frame[2], data, 6);
    if (BootCan_Send(ISOTP_RESP_ID, frame, 8) != 0)
    {
        return 1;
    }

    pos = 6;
    sn  = 1;

    if (IsoTp_WaitFC(&bs, &stmin) != 0)
    {
        return 1;
    }
    block_count = bs;

    while (pos < len)
    {
        n = (uint8_t)((len - pos) > 7U ? 7U : (len - pos));
        frame[0] = (uint8_t)(0x20U | (sn & 0x0FU));
        memcpy(&frame[1], &data[pos], n);
        if (BootCan_Send(ISOTP_RESP_ID, frame, (uint8_t)(n + 1U)) != 0)
        {
            return 1;
        }

        pos = (uint16_t)(pos + n);
        sn  = (uint8_t)((sn + 1U) & 0x0FU);

        if (pos >= len)
        {
            break;
        }

        if (bs != 0U)
        {
            block_count--;
            if (block_count == 0U)
            {
                if (IsoTp_WaitFC(&bs, &stmin) != 0)
                {
                    return 1;
                }
                block_count = bs;
            }
        }

        if (stmin != 0U)
        {
            /* 0x00..0x7F = milliseconds; 0xF1..0xF9 = 100..900 us. */
            BootCan_DelayMs((stmin <= 0x7FU) ? stmin : 1U);
        }
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
/* Receive                                                                    */
/* -------------------------------------------------------------------------- */

void IsoTp_RxFrame(const BootCan_FrameTypeDef *frame)
{
    uint8_t  pci;
    uint16_t len;
    uint16_t remain;
    uint8_t  n;

    if (frame->id != ISOTP_REQ_ID || frame->dlc == 0U)
    {
        return;
    }

    pci = (uint8_t)((frame->data[0] >> 4) & 0x0FU);

    if (pci == 0U)                       /* Single frame */
    {
        len = (uint16_t)(frame->data[0] & 0x0FU);
        if (len == 0U || len > 7U || len > (uint16_t)(frame->dlc - 1U))
        {
            return;
        }
        Uds_OnMessage(&frame->data[1], len);
    }
    else if (pci == 1U)                  /* First frame */
    {
        if (frame->dlc < 2U)
        {
            return;
        }
        len = (uint16_t)(((uint16_t)(frame->data[0] & 0x0FU) << 8) | frame->data[1]);
        if (len <= 7U || len > ISOTP_RX_MAX)
        {
            return;
        }

        n = (uint8_t)(frame->dlc - 2U);
        if (n > 6U)
        {
            n = 6U;
        }
        memcpy(s_rx_buf, &frame->data[2], n);

        s_rx_len      = len;
        s_rx_pos      = n;
        s_rx_sn       = 1;
        s_rx_state    = ISOTP_STATE_WAIT_CF;
        s_rx_deadline = BootCan_GetMs() + ISOTP_N_CR_MS;

        /* CTS, send all remaining blocks, no extra separation time. */
        IsoTp_SendFC(0, 0, ISOTP_STMIN_MS);

        if (s_rx_pos >= s_rx_len)
        {
            s_rx_state = ISOTP_STATE_IDLE;
            Uds_OnMessage(s_rx_buf, s_rx_len);
        }
    }
    else if (pci == 2U)                  /* Consecutive frame */
    {
        if (s_rx_state != ISOTP_STATE_WAIT_CF)
        {
            return;
        }
        if ((uint8_t)(frame->data[0] & 0x0FU) != s_rx_sn)
        {
            s_rx_state = ISOTP_STATE_IDLE;
            return;
        }

        remain = (uint16_t)(s_rx_len - s_rx_pos);
        n = (uint8_t)(frame->dlc - 1U);
        if (n > 7U)
        {
            n = 7U;
        }
        if (n > remain)
        {
            n = (uint8_t)remain;
        }
        memcpy(&s_rx_buf[s_rx_pos], &frame->data[1], n);

        s_rx_pos = (uint16_t)(s_rx_pos + n);
        s_rx_sn  = (uint8_t)((s_rx_sn + 1U) & 0x0FU);
        s_rx_deadline = BootCan_GetMs() + ISOTP_N_CR_MS;

        if (s_rx_pos >= s_rx_len)
        {
            s_rx_state = ISOTP_STATE_IDLE;
            Uds_OnMessage(s_rx_buf, s_rx_len);
        }
    }
    /* pci == 3 (flow control from tester) is consumed by IsoTp_WaitFC(). */
}

void IsoTp_Poll(uint32_t now_ms)
{
    if (s_rx_state == ISOTP_STATE_WAIT_CF && now_ms > s_rx_deadline)
    {
        s_rx_state = ISOTP_STATE_IDLE;
    }
}
