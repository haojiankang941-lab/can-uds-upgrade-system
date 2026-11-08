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
#include "lcd.h"
#include "lcd_ui.h"
#include "log.h"
#include <string.h>

/* Sessions */
#define UDS_SESSION_DEFAULT      0x01U
#define UDS_SESSION_PROGRAMMING  0x02U
#define UDS_SESSION_EXTENDED     0x03U

/* Negative response codes */
#define NRC_SERVICE_NOT_SUPPORTED        0x11U
#define NRC_SUBFUNCTION_NOT_SUPPORTED    0x12U
#define NRC_INCORRECT_LENGTH             0x13U
#define NRC_RESPONSE_TOO_LONG            0x14U
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

/* Read-only snapshot for the LCD status screen. */
static UdsUiStateTypeDef s_ui_state;

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
    LOGF("NRC sid=0x%02X nrc=0x%02X", (unsigned)sid, (unsigned)nrc);
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

    if (sub == UDS_SESSION_PROGRAMMING)
    {
        s_ui_state = UDS_UI_PROGRAMMING;
    }
    else if (sub == UDS_SESSION_DEFAULT)
    {
        s_ui_state = UDS_UI_IDLE;
    }
    LOGF("session 0x%02X", (unsigned)sub);

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

/* -------------------------------------------------------------------------- */
/* 0x22 ReadDataByIdentifier                                                  */
/* -------------------------------------------------------------------------- */

#define UDS_DID_BOOT_VERSION   0x0100U
#define UDS_DID_BOOT_DATE      0x0101U
#define UDS_DID_APP_VERSION    0x0102U
#define UDS_DID_APP_DATE       0x0103U
#define UDS_DID_APP_SIZE       0x0104U
#define UDS_DID_APP_CRC        0x0105U
#define UDS_DID_FLASH_STATUS   0x0106U

#define UDS_DID_ACTIVE_SESSION 0xF186U
#define UDS_DID_PART_NUMBER    0xF187U
#define UDS_DID_SW_VERSION     0xF189U
#define UDS_DID_HW_VERSION     0xF191U
#define UDS_DID_SW_VERSION_2   0xF195U

#define UDS_RESP_MAX           64U
#define UDS_DID_DATA_MAX       32U

static uint16_t Uds_StrLen(const char *s)
{
    uint16_t n = 0U;

    while (s[n] != '\0')
    {
        n++;
    }
    return n;
}

static uint16_t Uds_PutU32(uint8_t *out, uint16_t pos, uint32_t v)
{
    out[pos++] = (uint8_t)v;
    out[pos++] = (uint8_t)(v >> 8);
    out[pos++] = (uint8_t)(v >> 16);
    out[pos++] = (uint8_t)(v >> 24);
    return pos;
}

/* Fill out[] with the data of one DID.  Returns 0 for an unknown DID. */
static uint8_t Uds_22Value(uint16_t did, uint8_t *out, uint16_t *out_len)
{
    OTA_InfoTypeDef info;
    uint32_t value;
    uint16_t n;
    uint8_t  valid;

    switch (did)
    {
        case UDS_DID_ACTIVE_SESSION:
            out[0] = s_session;
            *out_len = 1U;
            return 1U;

        case UDS_DID_PART_NUMBER:
            *out_len = Uds_StrLen(BOOT_PART_NUMBER);
            memcpy(out, BOOT_PART_NUMBER, *out_len);
            return 1U;

        case UDS_DID_SW_VERSION:
        case UDS_DID_SW_VERSION_2:
            *out_len = Uds_StrLen(BOOT_VERSION_STR);
            memcpy(out, BOOT_VERSION_STR, *out_len);
            return 1U;

        case UDS_DID_HW_VERSION:
            *out_len = Uds_StrLen(BOOT_HW_VERSION_STR);
            memcpy(out, BOOT_HW_VERSION_STR, *out_len);
            return 1U;

        case UDS_DID_BOOT_VERSION:
            out[0] = (uint8_t)BOOT_VERSION_MAJOR;
            out[1] = (uint8_t)BOOT_VERSION_MINOR;
            out[2] = (uint8_t)BOOT_VERSION_PATCH;
            out[3] = 0U;
            *out_len = 4U;
            return 1U;

        case UDS_DID_BOOT_DATE:
            *out_len = Uds_StrLen(BOOT_BUILD_DATE);
            memcpy(out, BOOT_BUILD_DATE, *out_len);
            return 1U;

        case UDS_DID_APP_VERSION:
        case UDS_DID_APP_DATE:
        case UDS_DID_APP_SIZE:
        case UDS_DID_APP_CRC:
            (void)OTA_GetInfo(&info);
            if (did == UDS_DID_APP_VERSION)
            {
                value = info.version;
            }
            else if (did == UDS_DID_APP_DATE)
            {
                value = info.date;
            }
            else if (did == UDS_DID_APP_SIZE)
            {
                value = info.size;
            }
            else
            {
                value = info.crc;
            }
            (void)Uds_PutU32(out, 0U, value);
            *out_len = 4U;
            return 1U;

        case UDS_DID_FLASH_STATUS:
            /* valid, state, flash_count, last_result.  state: 0 = no record,
             * 1 = copy started but never verified, 2 = committed. */
            valid = OTA_GetInfo(&info);
            out[0] = valid;
            out[1] = (uint8_t)((info.magic != OTA_INFO_MAGIC) ? 0U :
                               ((info.committed == OTA_INFO_COMMITTED) ? 2U : 1U));
            n = Uds_PutU32(out, 2U, info.flash_count);
            (void)Uds_PutU32(out, n, info.last_result);
            *out_len = (uint16_t)(n + 4U);
            return 1U;

        default:
            return 0U;
    }
}

static void Uds_Service22(const uint8_t *msg, uint16_t len)
{
    uint8_t  r[UDS_RESP_MAX];
    uint8_t  data[UDS_DID_DATA_MAX];
    uint16_t pos = 0U;
    uint16_t i = 1U;
    uint16_t did;
    uint16_t dlen;

    /* At least one DID, and DIDs always come in pairs of bytes. */
    if (len < 3U || ((len - 1U) & 1U) != 0U)
    {
        Uds_SendNegative(0x22U, NRC_INCORRECT_LENGTH);
        return;
    }

    while ((uint16_t)(len - i) >= 2U)
    {
        did = (uint16_t)(((uint16_t)msg[i] << 8) | msg[i + 1U]);
        i = (uint16_t)(i + 2U);

        if (Uds_22Value(did, data, &dlen) == 0U)
        {
            Uds_SendNegative(0x22U, NRC_REQUEST_OUT_OF_RANGE);
            return;
        }

        if ((uint16_t)(pos + 3U + dlen) > UDS_RESP_MAX)
        {
            Uds_SendNegative(0x22U, NRC_RESPONSE_TOO_LONG);
            return;
        }

        r[pos++] = 0x62U;
        r[pos++] = (uint8_t)(did >> 8);
        r[pos++] = (uint8_t)did;
        memcpy(&r[pos], data, dlen);
        pos = (uint16_t)(pos + dlen);
    }

    Uds_Send(r, pos);
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
        LOG("security unlocked");
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

    LOGF("download req %lu bytes", (unsigned long)size);

    Uds_SendPendingFor(0x34U);
    s_ui_state = UDS_UI_ERASING;
    Lcd_UiLongOp(s_ui_state);

    if (OTA_Flash_EraseRange(BACKUP_ADDRESS, size) != OTA_OK)
    {
        s_ui_state = UDS_UI_ERROR;
        Lcd_UiLongOp(s_ui_state);
        Uds_SendNegative(0x34U, NRC_GENERAL_PROGRAMMING_FAILURE);
        return;
    }
    LOG("backup erased");

    s_ui_state        = UDS_UI_DOWNLOADING;
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

    s_ui_state = UDS_UI_DOWNLOADING;
    if (s_offset == s_size)
    {
        s_complete = 1U;
        s_ui_state = UDS_UI_TRANSFER_DONE;
    }

    r[0] = 0x76U;
    r[1] = bsc;
    Uds_Send(r, 2);

    /* One line per 16 KB, after the response so P2 is never delayed. */
    if ((s_offset >> 14) != ((s_offset - data_len) >> 14))
    {
        LOGF("prog %lu/%lu", (unsigned long)s_offset, (unsigned long)s_size);
    }
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
    LOG("transfer exit");
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
            s_ui_state = UDS_UI_ERASING;
            Lcd_UiLongOp(s_ui_state);

            if (OTA_Flash_EraseRange(BACKUP_ADDRESS, s_size) != OTA_OK)
            {
                s_ui_state = UDS_UI_ERROR;
                Lcd_UiLongOp(s_ui_state);
                Uds_SendNegative(0x31U, NRC_GENERAL_PROGRAMMING_FAILURE);
                return;
            }
            LOG("re-erase done");

            s_ui_state = UDS_UI_DOWNLOADING;
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

            s_ui_state = UDS_UI_CRC_CHECK;
            Lcd_UiLongOp(s_ui_state);

            OTA_Flash_Flush();
            crc = OTA_Flash_Crc32(BACKUP_ADDRESS, s_size);
            LOGF("crc=%08lX", (unsigned long)crc);

            if (len >= 8U)
            {
                expected = Uds_GetU32(&msg[4]);
                if (expected != crc)
                {
                    Uds_SendNegative(0x31U, NRC_REQUEST_OUT_OF_RANGE);
                    s_ui_state = UDS_UI_CRC_FAIL;
                    Lcd_UiLongOp(s_ui_state);
                    return;
                }
            }
            else if (crc != (s_crc ^ 0xFFFFFFFFUL))
            {
                Uds_SendNegative(0x31U, NRC_REQUEST_OUT_OF_RANGE);
                s_ui_state = UDS_UI_CRC_FAIL;
                Lcd_UiLongOp(s_ui_state);
                return;
            }

            r[0] = 0x71U; r[1] = 0x01U; r[2] = 0xFFU; r[3] = 0x01U; r[4] = 0x00U;
            Uds_Send(r, 5);
            s_ui_state = UDS_UI_CRC_OK;
            Lcd_UiLongOp(s_ui_state);
            LOG("crc ok");
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
            s_ui_state = UDS_UI_COPYING;
            Lcd_UiLongOp(s_ui_state);

            crc = OTA_Flash_Crc32(BACKUP_ADDRESS, s_size);
            if (crc != (s_crc ^ 0xFFFFFFFFUL))
            {
                s_ui_state = UDS_UI_ERROR;
                Lcd_UiLongOp(s_ui_state);
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

            /* Record the new image and clear the upgrade request before the
             * APP is erased.  If power fails during the copy, INFO stays
             * "staging" (invalid) instead of describing an image that was
             * never completed, and the request flag does not survive. */
            OTA_InfoStart(version, date, s_size, crc);

            if (OTA_Flash_CopyRange(APP_ADDRESS, BACKUP_ADDRESS, s_size) != OTA_OK)
            {
                OTA_InfoFinish(OTA_RESULT_COPY_FAIL);
                s_ui_state = UDS_UI_ERROR;
                Lcd_UiLongOp(s_ui_state);
                Uds_SendNegative(0x31U, NRC_GENERAL_PROGRAMMING_FAILURE);
                return;
            }
            LOG("copy done");

            Uds_SendPendingFor(0x31U);

            if (OTA_Flash_Crc32(APP_ADDRESS, s_size) != crc)
            {
                OTA_InfoFinish(OTA_RESULT_VERIFY_FAIL);
                s_ui_state = UDS_UI_ERROR;
                Lcd_UiLongOp(s_ui_state);
                Uds_SendNegative(0x31U, NRC_GENERAL_PROGRAMMING_FAILURE);
                return;
            }
            LOG("app verify ok");

            OTA_InfoFinish(OTA_RESULT_OK);

            r[0] = 0x71U; r[1] = 0x01U; r[2] = 0xFFU; r[3] = 0x02U; r[4] = 0x00U;
            Uds_Send(r, 5);

            s_ui_state = UDS_UI_ACTIVATED;
            Lcd_UiRefreshNow();
            LOG("activated");

            BootCan_DelayMs(LCD_ACTIVATE_HOLD_MS);
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
    s_ui_state        = UDS_UI_IDLE;
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
        case 0x22U: Uds_Service22(msg, len); break;
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
            s_ui_state = UDS_UI_IDLE;
        }
    }
}

void Uds_GetUiStatus(UdsUiStatusTypeDef *out)
{
    if (out == 0)
    {
        return;
    }

    out->state    = s_ui_state;
    out->session  = s_session;
    out->security = s_security;
    out->offset   = (s_download_active != 0U) ? s_offset : 0U;
    out->size     = (s_download_active != 0U) ? s_size : 0U;
}
