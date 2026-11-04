/**
 ******************************************************************************
 * @file    uds.c
 * @brief   Minimal UDS (ISO 14229) server for the bootloader
 * @note    Standard UDS download:
 *            10 02 -> 27 01/02 -> 34 -> 36... -> 37 -> 31 01 FF01 -> 31 01 FF02
 ******************************************************************************
 */

#include "uds.h"
#include "isotp.h"
#include "ota_flash.h"
#include "boot_can.h"
#include "boot.h"
#include "iap.h"
#include "W1251.h"
#include <string.h>

/* Sessions */
#define UDS_SESSION_DEFAULT      0x01U
#define UDS_SESSION_PROGRAMMING  0x02U
#define UDS_SESSION_EXTENDED     0x03U

/* Negative response codes */
#define NRC_SERVICE_NOT_SUPPORTED        0x11U
#define NRC_SUBFUNCTION_NOT_SUPPORTED    0x12U
#define NRC_INCORRECT_LENGTH             0x13U
#define NRC_CONDITIONS_NOT_CORRECT       0x22U
#define NRC_REQUEST_SEQUENCE_ERROR       0x24U
#define NRC_REQUEST_OUT_OF_RANGE         0x31U
#define NRC_SECURITY_ACCESS_DENIED       0x33U
#define NRC_INVALID_KEY                  0x35U
#define NRC_EXCEEDED_ATTEMPTS            0x36U
#define NRC_REQUIRED_TIME_DELAY          0x37U
#define NRC_TRANSFER_DATA_SUSPENDED      0x71U
#define NRC_GENERAL_PROGRAMMING_FAILURE  0x72U
#define NRC_WRONG_BLOCK_SEQUENCE         0x73U
#define NRC_RESPONSE_PENDING             0x78U

/* Security access: fixed seed/key for internal use. */
#define UDS_SECURITY_SEED   0x55667788UL
#define UDS_SECURITY_KEY    0x11223344UL

#define UDS_MAX_BLOCK       256U
#define UDS_MAX_RESP        16U
#define UDS_S3_TIMEOUT_MS   5000UL

static uint8_t  s_session;
static uint8_t  s_security;
static uint8_t  s_fail_count;
static uint32_t s_delay_until;
static uint32_t s_last_activity;

static uint8_t  s_download_active;
static uint32_t s_size;
static uint32_t s_offset;
static uint8_t  s_next_bsc;
static uint32_t s_crc;
static uint8_t  s_complete;

/* -------------------------------------------------------------------------- */
/* Helpers                                                                    */
/* -------------------------------------------------------------------------- */

static uint32_t Uds_GetU32(const uint8_t *p)
{
    return (uint32_t)p[0]
         | ((uint32_t)p[1] << 8)
         | ((uint32_t)p[2] << 16)
         | ((uint32_t)p[3] << 24);
}

static void Uds_Send(const uint8_t *data, uint16_t len)
{
    (void)IsoTp_Send(data, len);
}

static void Uds_SendNegative(uint8_t sid, uint8_t nrc)
{
    uint8_t r[3];

    r[0] = 0x7FU;
    r[1] = sid;
    r[2] = nrc;
    Uds_Send(r, 3);
}

/* Send 0x78 for a specific service. */
static void Uds_SendPendingFor(uint8_t sid)
{
    uint8_t r[3];

    r[0] = 0x7FU;
    r[1] = sid;
    r[2] = NRC_RESPONSE_PENDING;
    Uds_Send(r, 3);
}

static uint8_t Uds_CheckProgramming(void)
{
    if (s_session != UDS_SESSION_PROGRAMMING)
    {
        return 0x7FU;
    }
    if (s_security == 0U)
    {
        return NRC_SECURITY_ACCESS_DENIED;
    }
    return 0U;
}

/* -------------------------------------------------------------------------- */
/* Services                                                                   */
/* -------------------------------------------------------------------------- */

static void Uds_Service10(const uint8_t *msg, uint16_t len)
{
    uint8_t sub;
    uint8_t r[6];

    if (len < 2U)
    {
        Uds_SendNegative(0x10U, NRC_INCORRECT_LENGTH);
        return;
    }

    sub = (uint8_t)(msg[1] & 0x7FU);
    if (sub < UDS_SESSION_DEFAULT || sub > UDS_SESSION_EXTENDED)
    {
        Uds_SendNegative(0x10U, NRC_SUBFUNCTION_NOT_SUPPORTED);
        return;
    }

    s_session = sub;
    if (sub == UDS_SESSION_PROGRAMMING || sub == UDS_SESSION_DEFAULT)
    {
        s_security = 0U;
    }

    if ((msg[1] & 0x80U) == 0U)
    {
        r[0] = 0x50U;
        r[1] = sub;
        r[2] = 0x00U;   /* P2 = 50 ms */
        r[3] = 0x32U;
        r[4] = 0x01U;   /* P2* = 5000 ms */
        r[5] = 0xF4U;
        Uds_Send(r, 6);
    }
}

static void Uds_Service11(const uint8_t *msg, uint16_t len)
{
    uint8_t sub;
    uint8_t r[2];

    if (len < 2U)
    {
        Uds_SendNegative(0x11U, NRC_INCORRECT_LENGTH);
        return;
    }

    sub = (uint8_t)(msg[1] & 0x7FU);
    if (sub != 0x01U)
    {
        Uds_SendNegative(0x11U, NRC_SUBFUNCTION_NOT_SUPPORTED);
        return;
    }

    if ((msg[1] & 0x80U) == 0U)
    {
        r[0] = 0x51U;
        r[1] = 0x01U;
        Uds_Send(r, 2);
    }

    BootCan_DelayMs(20U);
    NVIC_SystemReset();
}

static void Uds_Service27(const uint8_t *msg, uint16_t len)
{
    uint8_t  sub;
    uint32_t key;
    uint32_t now = BootCan_GetMs();
    uint8_t  r[6];

    if (len < 2U)
    {
        Uds_SendNegative(0x27U, NRC_INCORRECT_LENGTH);
        return;
    }

    if (s_delay_until != 0U && now < s_delay_until)
    {
        Uds_SendNegative(0x27U, NRC_REQUIRED_TIME_DELAY);
        return;
    }

    sub = msg[1];

    if (sub == 0x01U)                    /* requestSeed */
    {
        s_security = 0U;
        r[0] = 0x67U;
        r[1] = 0x01U;
        r[2] = (uint8_t)(UDS_SECURITY_SEED);
        r[3] = (uint8_t)(UDS_SECURITY_SEED >> 8);
        r[4] = (uint8_t)(UDS_SECURITY_SEED >> 16);
        r[5] = (uint8_t)(UDS_SECURITY_SEED >> 24);
        Uds_Send(r, 6);
    }
    else if (sub == 0x02U)               /* sendKey */
    {
        if (len < 6U)
        {
            Uds_SendNegative(0x27U, NRC_INCORRECT_LENGTH);
            return;
        }

        key = Uds_GetU32(&msg[2]);
        if (key != UDS_SECURITY_KEY)
        {
            s_fail_count++;
            if (s_fail_count >= 3U)
            {
                s_fail_count = 0U;
                s_delay_until = now + 10000UL;
                Uds_SendNegative(0x27U, NRC_EXCEEDED_ATTEMPTS);
            }
            else
            {
                Uds_SendNegative(0x27U, NRC_INVALID_KEY);
            }
            return;
        }

        s_security   = 1U;
        s_fail_count = 0U;
        s_delay_until = 0U;
        r[0] = 0x67U;
        r[1] = 0x02U;
        Uds_Send(r, 2);
    }
    else
    {
        Uds_SendNegative(0x27U, NRC_SUBFUNCTION_NOT_SUPPORTED);
    }
}

static void Uds_Service28(const uint8_t *msg, uint16_t len)
{
    uint8_t sub;
    uint8_t r[2];

    if (len < 2U)
    {
        Uds_SendNegative(0x28U, NRC_INCORRECT_LENGTH);
        return;
    }

    sub = (uint8_t)(msg[1] & 0x7FU);
    if (sub > 0x03U)
    {
        Uds_SendNegative(0x28U, NRC_SUBFUNCTION_NOT_SUPPORTED);
        return;
    }

    if ((msg[1] & 0x80U) == 0U)
    {
        r[0] = 0x68U;
        r[1] = sub;
        Uds_Send(r, 2);
    }
}

static void Uds_Service34(const uint8_t *msg, uint16_t len)
{
    uint32_t addr;
    uint32_t size;
    uint8_t  nrc;
    uint8_t  r[4];

    if (len < 10U)
    {
        Uds_SendNegative(0x34U, NRC_INCORRECT_LENGTH);
        return;
    }

    nrc = Uds_CheckProgramming();
    if (nrc != 0U)
    {
        Uds_SendNegative(0x34U, nrc);
        return;
    }

    if (s_download_active != 0U)
    {
        Uds_SendNegative(0x34U, NRC_CONDITIONS_NOT_CORRECT);
        return;
    }

    /* Address-and-length format identifier: 4-byte address, 4-byte size. */
    if (msg[1] != 0x44U)
    {
        Uds_SendNegative(0x34U, NRC_INCORRECT_LENGTH);
        return;
    }

    addr = Uds_GetU32(&msg[2]);
    size = Uds_GetU32(&msg[6]);

    if (addr != APP_ADDRESS || size == 0U || size > BACKUP_SIZE)
    {
        Uds_SendNegative(0x34U, NRC_REQUEST_OUT_OF_RANGE);
        return;
    }

    Uds_SendPendingFor(0x34U);

    if (OTA_Flash_EraseRange(BACKUP_ADDRESS, size) != OTA_OK)
    {
        Uds_SendNegative(0x34U, NRC_GENERAL_PROGRAMMING_FAILURE);
        return;
    }

    s_download_active = 1U;
    s_size            = size;
    s_offset          = 0U;
    s_next_bsc        = 1U;
    s_crc             = 0xFFFFFFFFUL;
    s_complete        = 0U;

    /* lengthFormatIdentifier 0x20: two bytes of maxNumberOfBlockLength.
     * 0x0102 = 258 = 1 SID + 1 BSC + 256 data bytes. */
    r[0] = 0x74U;
    r[1] = 0x20U;
    r[2] = 0x01U;
    r[3] = 0x02U;
    Uds_Send(r, 4);
}

static void Uds_Service36(const uint8_t *msg, uint16_t len)
{
    uint8_t  bsc;
    uint16_t data_len;
    uint8_t  nrc;
    uint8_t  r[2];

    if (s_download_active == 0U)
    {
        Uds_SendNegative(0x36U, NRC_REQUEST_SEQUENCE_ERROR);
        return;
    }

    nrc = Uds_CheckProgramming();
    if (nrc != 0U)
    {
        Uds_SendNegative(0x36U, nrc);
        return;
    }

    if (len < 2U)
    {
        Uds_SendNegative(0x36U, NRC_INCORRECT_LENGTH);
        return;
    }

    bsc      = msg[1];
    data_len = (uint16_t)(len - 2U);

    if (data_len == 0U || data_len > UDS_MAX_BLOCK)
    {
        Uds_SendNegative(0x36U, NRC_INCORRECT_LENGTH);
        return;
    }

    if ((s_offset + data_len) > s_size)
    {
        Uds_SendNegative(0x36U, NRC_TRANSFER_DATA_SUSPENDED);
        return;
    }

    if (bsc != s_next_bsc)
    {
        Uds_SendNegative(0x36U, NRC_WRONG_BLOCK_SEQUENCE);
        return;
    }

    if (OTA_Flash_Program(BACKUP_ADDRESS + s_offset, &msg[2], data_len) != OTA_OK)
    {
        Uds_SendNegative(0x36U, NRC_GENERAL_PROGRAMMING_FAILURE);
        return;
    }

    s_crc = OTA_Crc32Update(s_crc, &msg[2], data_len);
    s_offset = (uint32_t)(s_offset + data_len);
    s_next_bsc = (uint8_t)((s_next_bsc + 1U) & 0xFFU);

    if (s_offset == s_size)
    {
        s_complete = 1U;
    }

    r[0] = 0x76U;
    r[1] = bsc;
    Uds_Send(r, 2);
}

static void Uds_Service37(void)
{
    uint8_t r[1];

    if (s_download_active == 0U || s_complete == 0U)
    {
        Uds_SendNegative(0x37U, NRC_REQUEST_SEQUENCE_ERROR);
        return;
    }

    OTA_Flash_Flush();

    r[0] = 0x77U;
    Uds_Send(r, 1);
}

static void Uds_Service31(const uint8_t *msg, uint16_t len)
{
    uint8_t  sub;
    uint16_t routine;
    uint8_t  nrc;
    uint8_t  r[5];
    uint32_t crc;
    uint32_t expected;
    uint32_t version;
    uint32_t date;

    if (len < 4U)
    {
        Uds_SendNegative(0x31U, NRC_INCORRECT_LENGTH);
        return;
    }

    nrc = Uds_CheckProgramming();
    if (nrc != 0U)
    {
        Uds_SendNegative(0x31U, nrc);
        return;
    }

    sub     = msg[1];
    routine = (uint16_t)(((uint16_t)msg[2] << 8) | msg[3]);

    if (sub == 0x01U)                    /* startRoutine */
    {
        if (routine == 0xFF00U)          /* erase backup */
        {
            if (s_download_active == 0U)
            {
                Uds_SendNegative(0x31U, NRC_REQUEST_SEQUENCE_ERROR);
                return;
            }

            Uds_SendPendingFor(0x31U);
            if (OTA_Flash_EraseRange(BACKUP_ADDRESS, s_size) != OTA_OK)
            {
                Uds_SendNegative(0x31U, NRC_GENERAL_PROGRAMMING_FAILURE);
                return;
            }

            s_offset   = 0U;
            s_next_bsc = 1U;
            s_crc      = 0xFFFFFFFFUL;
            s_complete = 0U;

            r[0] = 0x71U; r[1] = 0x01U; r[2] = 0xFFU; r[3] = 0x00U; r[4] = 0x00U;
            Uds_Send(r, 5);
        }
        else if (routine == 0xFF01U)     /* check CRC */
        {
            if (s_download_active == 0U || s_complete == 0U)
            {
                Uds_SendNegative(0x31U, NRC_REQUEST_SEQUENCE_ERROR);
                return;
            }

            OTA_Flash_Flush();
            crc = OTA_Flash_Crc32(BACKUP_ADDRESS, s_size);

            if (len >= 8U)
            {
                expected = Uds_GetU32(&msg[4]);
                if (expected != crc)
                {
                    Uds_SendNegative(0x31U, NRC_REQUEST_OUT_OF_RANGE);
                    return;
                }
            }
            else if (crc != (s_crc ^ 0xFFFFFFFFUL))
            {
                Uds_SendNegative(0x31U, NRC_REQUEST_OUT_OF_RANGE);
                return;
            }

            r[0] = 0x71U; r[1] = 0x01U; r[2] = 0xFFU; r[3] = 0x01U; r[4] = 0x00U;
            Uds_Send(r, 5);
        }
        else if (routine == 0xFF02U)     /* activate: copy backup -> app */
        {
            if (s_download_active == 0U || s_complete == 0U)
            {
                Uds_SendNegative(0x31U, NRC_REQUEST_SEQUENCE_ERROR);
                return;
            }

            OTA_Flash_Flush();

            Uds_SendPendingFor(0x31U);
            crc = OTA_Flash_Crc32(BACKUP_ADDRESS, s_size);
            if (crc != (s_crc ^ 0xFFFFFFFFUL))
            {
                Uds_SendNegative(0x31U, NRC_REQUEST_OUT_OF_RANGE);
                return;
            }

            if (len >= 12U)
            {
                version = Uds_GetU32(&msg[4]);
                date    = Uds_GetU32(&msg[8]);
            }
            else
            {
                version = 0U;
                date    = 0U;
            }

            if (OTA_Flash_CopyRange(APP_ADDRESS, BACKUP_ADDRESS, s_size) != OTA_OK)
            {
                Uds_SendNegative(0x31U, NRC_GENERAL_PROGRAMMING_FAILURE);
                return;
            }

            Uds_SendPendingFor(0x31U);

            if (OTA_Flash_Crc32(APP_ADDRESS, s_size) != crc)
            {
                Uds_SendNegative(0x31U, NRC_GENERAL_PROGRAMMING_FAILURE);
                return;
            }

            OTA_SaveInfo(version, date, s_size, crc);

            r[0] = 0x71U; r[1] = 0x01U; r[2] = 0xFFU; r[3] = 0x02U; r[4] = 0x00U;
            Uds_Send(r, 5);

            BootCan_DelayMs(20U);
            jump_to_flash();
            while (1)
            {
                ;
            }
        }
        else
        {
            Uds_SendNegative(0x31U, NRC_REQUEST_OUT_OF_RANGE);
        }
    }
    else if (sub == 0x02U || sub == 0x03U)   /* stop / requestResults */
    {
        r[0] = 0x71U;
        r[1] = sub;
        r[2] = (uint8_t)(routine >> 8);
        r[3] = (uint8_t)(routine);
        r[4] = 0x00U;
        Uds_Send(r, 5);
    }
    else
    {
        Uds_SendNegative(0x31U, NRC_SUBFUNCTION_NOT_SUPPORTED);
    }
}

static void Uds_Service3E(const uint8_t *msg, uint16_t len)
{
    uint8_t r[2];

    if (len < 2U)
    {
        Uds_SendNegative(0x3EU, NRC_INCORRECT_LENGTH);
        return;
    }

    if ((msg[1] & 0x80U) == 0U)
    {
        r[0] = 0x7EU;
        r[1] = 0x00U;
        Uds_Send(r, 2);
    }
}

static void Uds_Service85(const uint8_t *msg, uint16_t len)
{
    uint8_t sub;
    uint8_t r[2];

    if (len < 2U)
    {
        Uds_SendNegative(0x85U, NRC_INCORRECT_LENGTH);
        return;
    }

    sub = (uint8_t)(msg[1] & 0x7FU);

    if ((msg[1] & 0x80U) == 0U)
    {
        r[0] = 0xC5U;
        r[1] = sub;
        Uds_Send(r, 2);
    }
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

void Uds_Init(void)
{
    s_session         = UDS_SESSION_DEFAULT;
    s_security        = 0U;
    s_fail_count      = 0U;
    s_delay_until     = 0U;
    s_last_activity   = BootCan_GetMs();
    s_download_active = 0U;
    s_size            = 0U;
    s_offset          = 0U;
    s_next_bsc        = 1U;
    s_crc             = 0xFFFFFFFFUL;
    s_complete        = 0U;
}

void Uds_OnMessage(const uint8_t *msg, uint16_t len)
{
    uint8_t sid;

    if (len == 0U)
    {
        return;
    }

    sid = msg[0];
    s_last_activity = BootCan_GetMs();

    switch (sid)
    {
        case 0x10U: Uds_Service10(msg, len); break;
        case 0x11U: Uds_Service11(msg, len); break;
        case 0x27U: Uds_Service27(msg, len); break;
        case 0x28U: Uds_Service28(msg, len); break;
        case 0x34U: Uds_Service34(msg, len); break;
        case 0x36U: Uds_Service36(msg, len); break;
        case 0x37U: Uds_Service37();         break;
        case 0x31U: Uds_Service31(msg, len); break;
        case 0x3EU: Uds_Service3E(msg, len); break;
        case 0x85U: Uds_Service85(msg, len); break;
        default:
            Uds_SendNegative(sid, NRC_SERVICE_NOT_SUPPORTED);
            break;
    }
}

void Uds_Poll(uint32_t now_ms)
{
    if (s_delay_until != 0U && now_ms >= s_delay_until)
    {
        s_delay_until = 0U;
    }

    /* S3: return to the default session after 5 s of inactivity.
     * Never time out in the middle of a download. */
    if (s_session != UDS_SESSION_DEFAULT && s_download_active == 0U)
    {
        if ((now_ms - s_last_activity) > UDS_S3_TIMEOUT_MS)
        {
            s_session  = UDS_SESSION_DEFAULT;
            s_security = 0U;
        }
    }
}
