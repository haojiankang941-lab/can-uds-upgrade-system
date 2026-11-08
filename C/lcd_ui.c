/**
 ******************************************************************************
 * @file    lcd_ui.c
 * @brief   Bootloader status screen
 * @note    Layout (128x160, 5x8 font, 6 px advance):
 *            y=  2  UDS BOOT            (2x, cyan)
 *            y= 22  BOOT v1.0.0
 *            y= 32  BUILD <date>
 *            y= 42  APP  v1.0.2
 *            y= 52  DATE 2026-11-03
 *            y= 62  ------------------
 *            y= 70  STATE <state>
 *            y= 84  [ progress bar ]
 *            y=102  PROG 47% 120/240KB
 *            y=114  SESS 02  SEC ON
 *            y=126  CAN 7E0 -> 7E8
 *            y=138  BOOT 08D00000
 ******************************************************************************
 */

#include "lcd_ui.h"
#include "lcd.h"
#include "ota_flash.h"
#include "boot.h"
#include "boot_can.h"

#define UI_PERIOD_MS       200U

#define UI_Y_TITLE         2U
#define UI_Y_BOOT          22U
#define UI_Y_BUILD         32U
#define UI_Y_APP           42U
#define UI_Y_DATE          52U
#define UI_Y_SEP           62U
#define UI_Y_STATE         70U
#define UI_Y_BAR           84U
#define UI_Y_PROG          102U
#define UI_Y_SESS          114U
#define UI_Y_CAN           126U
#define UI_Y_BOOT_ADDR     138U

#define UI_STATIC_FIELDS   8U

/* 0 = not started, 1 = clearing, 2 = static text, 3 = running. */
static uint8_t  s_phase;
static uint8_t  s_band;
static uint8_t  s_static_idx;
static uint32_t s_last_ms;

/* Cached dynamic values (0xFF = force redraw). */
static uint8_t  s_last_state;
static uint8_t  s_last_pct;
static uint8_t  s_last_sess;
static uint8_t  s_last_sec;
static uint32_t s_last_off;
static uint32_t s_last_total;

static char     s_app_line[20];
static char     s_date_line[20];

/* -------------------------------------------------------------------------- */
/* Small string helpers (no sprintf: keeps printf out of the image)           */
/* -------------------------------------------------------------------------- */

static char *Ui_PutStr(char *p, const char *s)
{
    while (*s != '\0')
    {
        *p++ = *s++;
    }
    return p;
}

static char *Ui_PutDecW(char *p, uint32_t v, uint8_t width)
{
    char    tmp[10];
    uint8_t n = 0;

    do
    {
        tmp[n++] = (char)('0' + (v % 10U));
        v /= 10U;
    } while (v != 0U && n < 10U);

    while (n < width && n < 10U)
    {
        tmp[n++] = ' ';
    }

    while (n != 0U)
    {
        *p++ = tmp[--n];
    }
    return p;
}

static char *Ui_PutHex8(char *p, uint32_t v)
{
    static const char hex[] = "0123456789ABCDEF";
    int8_t i;

    for (i = 28; i >= 0; i = (int8_t)(i - 4))
    {
        *p++ = hex[(v >> i) & 0x0FU];
    }
    return p;
}

/* -------------------------------------------------------------------------- */
/* Version / date formatting                                                  */
/* -------------------------------------------------------------------------- */

static uint8_t Ui_IsBcd(uint32_t byte, uint32_t min, uint32_t max)
{
    uint32_t hi = (byte >> 4) & 0x0FU;
    uint32_t lo = byte & 0x0FU;
    uint32_t val;

    if (hi > 9U || lo > 9U)
    {
        return 0U;
    }
    val = hi * 10U + lo;
    return (val >= min && val <= max) ? 1U : 0U;
}

static uint8_t Ui_DateValid(uint32_t d)
{
    if ((d >> 24) != 0x20U)
    {
        return 0U;
    }
    if (Ui_IsBcd((d >> 16) & 0xFFU, 0U, 99U) == 0U)    /* year */
    {
        return 0U;
    }
    if (Ui_IsBcd((d >> 8) & 0xFFU, 1U, 12U) == 0U)     /* month */
    {
        return 0U;
    }
    if (Ui_IsBcd(d & 0xFFU, 1U, 31U) == 0U)            /* day */
    {
        return 0U;
    }
    return 1U;
}

static void Ui_PutDate(char *p, uint32_t d)
{
    uint32_t yy = (d >> 16) & 0xFFU;
    uint32_t mm = (d >> 8) & 0xFFU;
    uint32_t dd = d & 0xFFU;

    p[0] = '2';
    p[1] = '0';
    p[2] = (char)('0' + ((yy >> 4) & 0x0FU));
    p[3] = (char)('0' + (yy & 0x0FU));
    p[4] = '-';
    p[5] = (char)('0' + ((mm >> 4) & 0x0FU));
    p[6] = (char)('0' + (mm & 0x0FU));
    p[7] = '-';
    p[8] = (char)('0' + ((dd >> 4) & 0x0FU));
    p[9] = (char)('0' + (dd & 0x0FU));
    p[10] = '\0';
}

static void Ui_BuildInfo(void)
{
    OTA_InfoTypeDef info;
    char *p;

    p = s_app_line;
    p = Ui_PutStr(p, "APP  ");
    if (OTA_GetInfo(&info) != 0U)
    {
        p = Ui_PutStr(p, "v");
        p = Ui_PutDecW(p, (info.version >> 16) & 0xFFU, 1U);
        p = Ui_PutStr(p, ".");
        p = Ui_PutDecW(p, (info.version >> 8) & 0xFFU, 1U);
        p = Ui_PutStr(p, ".");
        p = Ui_PutDecW(p, info.version & 0xFFU, 1U);
        *p = '\0';
    }
    else
    {
        p = Ui_PutStr(p, "INVALID");
        *p = '\0';
    }

    p = s_date_line;
    p = Ui_PutStr(p, "DATE ");
    if (OTA_GetInfo(&info) != 0U && Ui_DateValid(info.date) != 0U)
    {
        Ui_PutDate(p, info.date);
    }
    else
    {
        p = Ui_PutHex8(p, info.date);
        *p = '\0';
    }
}

/* -------------------------------------------------------------------------- */
/* State text                                                                 */
/* -------------------------------------------------------------------------- */

static const char *Ui_StateText(UdsUiStateTypeDef st)
{
    switch (st)
    {
        case UDS_UI_PROGRAMMING:   return "PROGRAMMING";
        case UDS_UI_ERASING:       return "ERASING...";
        case UDS_UI_DOWNLOADING:   return "DOWNLOADING";
        case UDS_UI_TRANSFER_DONE: return "TRANSFER DONE";
        case UDS_UI_CRC_CHECK:     return "CRC CHECK";
        case UDS_UI_CRC_OK:        return "CRC OK";
        case UDS_UI_CRC_FAIL:      return "CRC FAIL";
        case UDS_UI_COPYING:       return "COPYING...";
        case UDS_UI_ACTIVATED:     return "ACTIVATED";
        case UDS_UI_ERROR:         return "ERROR";
        default:                   return "WAITING";
    }
}

static uint16_t Ui_StateColor(UdsUiStateTypeDef st)
{
    if (st == UDS_UI_ERROR || st == UDS_UI_CRC_FAIL)
    {
        return LCD_COLOR_RED;
    }
    if (st == UDS_UI_CRC_OK || st == UDS_UI_ACTIVATED)
    {
        return LCD_COLOR_GREEN;
    }
    return LCD_COLOR_YELLOW;
}

/* -------------------------------------------------------------------------- */
/* Drawing                                                                    */
/* -------------------------------------------------------------------------- */

static void Ui_FillLine(uint16_t y)
{
    Lcd_FillRect(0U, y, LCD_W, 8U, LCD_COLOR_BLACK);
}

static void Ui_DrawStaticField(uint8_t idx)
{
    switch (idx)
    {
        case 0U:
            Lcd_DrawString(4U, UI_Y_TITLE, "UDS BOOT",
                           LCD_COLOR_CYAN, LCD_COLOR_BLACK, 2U);
            break;

        case 1U:
            Ui_FillLine(UI_Y_BOOT);
            Lcd_DrawString(4U, UI_Y_BOOT, BOOT_VERSION_STR,
                           LCD_COLOR_WHITE, LCD_COLOR_BLACK, 1U);
            break;

        case 2U:
            Ui_FillLine(UI_Y_BUILD);
            Lcd_DrawString(4U, UI_Y_BUILD, "BUILD " BOOT_BUILD_DATE,
                           LCD_COLOR_GRAY, LCD_COLOR_BLACK, 1U);
            break;

        case 3U:
            Ui_FillLine(UI_Y_APP);
            Lcd_DrawString(4U, UI_Y_APP, s_app_line,
                           LCD_COLOR_GREEN, LCD_COLOR_BLACK, 1U);
            break;

        case 4U:
            Ui_FillLine(UI_Y_DATE);
            Lcd_DrawString(4U, UI_Y_DATE, s_date_line,
                           LCD_COLOR_WHITE, LCD_COLOR_BLACK, 1U);
            break;

        case 5U:
            Lcd_FillRect(4U, UI_Y_SEP, LCD_W - 8U, 1U, LCD_COLOR_DARK);
            break;

        case 6U:
            Ui_FillLine(UI_Y_CAN);
            Lcd_DrawString(4U, UI_Y_CAN, "CAN 7E0 -> 7E8",
                           LCD_COLOR_GRAY, LCD_COLOR_BLACK, 1U);
            break;

        default:
            Ui_FillLine(UI_Y_BOOT_ADDR);
            Lcd_DrawString(4U, UI_Y_BOOT_ADDR, "BOOT 08D00000",
                           LCD_COLOR_GRAY, LCD_COLOR_BLACK, 1U);
            break;
    }
}

static void Ui_DrawStateLine(UdsUiStateTypeDef st)
{
    char line[28];
    char *p = line;

    p = Ui_PutStr(p, "STATE ");
    p = Ui_PutStr(p, Ui_StateText(st));
    *p = '\0';

    Ui_FillLine(UI_Y_STATE);
    Lcd_DrawString(4U, UI_Y_STATE, line,
                   Ui_StateColor(st), LCD_COLOR_BLACK, 1U);
}

static void Ui_DrawProgress(uint32_t done, uint32_t total)
{
    char line[24];
    char *p = line;
    uint8_t pct = 0U;

    if (total != 0U)
    {
        pct = (uint8_t)(((uint32_t)done * 100U) / total);
    }

    Lcd_ProgressBar(4U, UI_Y_BAR, 120U, 12U, pct,
                    LCD_COLOR_GREEN, LCD_COLOR_BLACK);

    p = Ui_PutStr(p, "PROG ");
    p = Ui_PutDecW(p, pct, 3U);
    p = Ui_PutStr(p, "% ");
    p = Ui_PutDecW(p, done / 1024U, 3U);
    p = Ui_PutStr(p, "/");
    p = Ui_PutDecW(p, total / 1024U, 3U);
    p = Ui_PutStr(p, "KB");
    *p = '\0';

    Ui_FillLine(UI_Y_PROG);
    Lcd_DrawString(4U, UI_Y_PROG, line,
                   LCD_COLOR_WHITE, LCD_COLOR_BLACK, 1U);
}

static void Ui_DrawSession(uint8_t session, uint8_t security)
{
    char line[24];
    char *p = line;

    p = Ui_PutStr(p, "SESS ");
    p = Ui_PutDecW(p, session, 2U);
    p = Ui_PutStr(p, "  SEC ");
    p = Ui_PutStr(p, (security != 0U) ? "ON" : "OFF");
    *p = '\0';

    Ui_FillLine(UI_Y_SESS);
    Lcd_DrawString(4U, UI_Y_SESS, line,
                   (security != 0U) ? LCD_COLOR_GREEN : LCD_COLOR_GRAY,
                   LCD_COLOR_BLACK, 1U);
}

static void Ui_Invalidate(void)
{
    s_last_state = 0xFFU;
    s_last_pct   = 0xFFU;
    s_last_sess  = 0xFFU;
    s_last_sec   = 0xFFU;
    s_last_off   = 0xFFFFFFFFUL;
    s_last_total = 0xFFFFFFFFUL;
}

static void Ui_DrawDynamic(void)
{
    UdsUiStatusTypeDef st;
    uint8_t pct = 0U;

    Uds_GetUiStatus(&st);

    if (st.size != 0U)
    {
        pct = (uint8_t)(((uint32_t)st.offset * 100U) / st.size);
    }

    if ((uint8_t)st.state != s_last_state)
    {
        Ui_DrawStateLine(st.state);
        s_last_state = (uint8_t)st.state;
    }

    if (pct != s_last_pct || st.offset != s_last_off || st.size != s_last_total)
    {
        Ui_DrawProgress(st.offset, st.size);
        s_last_pct   = pct;
        s_last_off   = st.offset;
        s_last_total = st.size;
    }

    if (st.session != s_last_sess || st.security != s_last_sec)
    {
        Ui_DrawSession(st.session, st.security);
        s_last_sess = st.session;
        s_last_sec  = st.security;
    }
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

void Lcd_UiInit(void)
{
    if (s_phase != 0U)
    {
        return;
    }

    s_phase      = 1U;
    s_band       = 0U;
    s_static_idx = 0U;
    s_last_ms    = 0U;
    Ui_Invalidate();

    Lcd_InitStart(BootCan_GetMs());
}

void Lcd_UiPoll(uint32_t now_ms)
{
    if (s_phase == 0U)
    {
        return;
    }

    if (Lcd_IsReady() == 0U)
    {
        (void)Lcd_InitPoll(now_ms);
        if (Lcd_IsReady() == 0U)
        {
            return;
        }
    }

    if (Lcd_IsPresent() == 0U)
    {
        s_phase = 0U;               /* no panel: stop touching SPI */
        return;
    }

    switch (s_phase)
    {
        case 1U:                    /* clear the screen in 20-row bands */
            Lcd_FillRect(0U, (uint16_t)(s_band * 20U), LCD_W, 20U,
                         LCD_COLOR_BLACK);
            s_band++;
            if (s_band >= 8U)
            {
                Ui_BuildInfo();
                s_static_idx = 0U;
                s_phase = 2U;
            }
            break;

        case 2U:                    /* one static field per call */
            Ui_DrawStaticField(s_static_idx);
            s_static_idx++;
            if (s_static_idx >= UI_STATIC_FIELDS)
            {
                Ui_Invalidate();
                Ui_DrawDynamic();
                s_last_ms = now_ms;
                s_phase = 3U;
            }
            break;

        default:                    /* running: refresh changed fields */
            if ((uint32_t)(now_ms - s_last_ms) < UI_PERIOD_MS)
            {
                break;
            }
            s_last_ms = now_ms;
            Ui_DrawDynamic();
            break;
    }
}

void Lcd_UiLongOp(UdsUiStateTypeDef state)
{
    if (Lcd_IsReady() == 0U || Lcd_IsPresent() == 0U)
    {
        return;
    }

    Ui_DrawStateLine(state);
    s_last_state = (uint8_t)state;
}

void Lcd_UiRefreshNow(void)
{
    if (Lcd_IsReady() == 0U || Lcd_IsPresent() == 0U)
    {
        return;
    }

    Ui_Invalidate();
    Ui_DrawDynamic();
    s_last_ms = BootCan_GetMs();
}
