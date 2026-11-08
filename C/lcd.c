/**
 ******************************************************************************
 * @file    lcd.c
 * @brief   ST7735S 128x160 display on SPI0 (4-wire SPI)
 * @note    Direct rendering, no frame buffer: every rectangle/character is
 *          streamed to the panel.  The init sequence is a state machine so
 *          the caller can keep polling CAN while the panel powers up.
 ******************************************************************************
 */

#include "lcd.h"
#include "boot_can.h"
#include "W1251.h"
#include "SC32F10xx_rcc.h"

/* -------------------------------------------------------------------------- */
/* SPI0 register bits (plain SPI mode, not TWI/QSPI mode)                     */
/* -------------------------------------------------------------------------- */

#define LCD_CON_MSTR       (1UL << 0)
#define LCD_CON_SPEN       (1UL << 7)
#define LCD_CON_SPR_Pos    (8U)
#define LCD_CON_MODE_Pos   (22U)     /* 01 = full-duplex SPI */

#define LCD_STS_SPIF       (1UL << 0)
#define LCD_STS_RXNEIF     (1UL << 1)
#define LCD_STS_TXEIF      (1UL << 2)

#define LCD_SPI_GUARD      50000UL

/* -------------------------------------------------------------------------- */
/* Pin helpers                                                                */
/* -------------------------------------------------------------------------- */

#define LCD_CS_HIGH()      (PB_BIT(LCD_PIN_CS) = 1U)
#define LCD_CS_LOW()       (PB_BIT(LCD_PIN_CS) = 0U)
#define LCD_DC_CMD()       (PB_BIT(LCD_PIN_DC) = 0U)
#define LCD_DC_DATA()      (PB_BIT(LCD_PIN_DC) = 1U)
#define LCD_RST_HIGH()     (PD_BIT(LCD_PIN_RST) = 1U)
#define LCD_RST_LOW()      (PD_BIT(LCD_PIN_RST) = 0U)

/* -------------------------------------------------------------------------- */
/* Init state machine                                                         */
/* -------------------------------------------------------------------------- */

#define LCD_ST_IDLE        0U
#define LCD_ST_RESET_LOW   1U
#define LCD_ST_RESET_WAIT  2U
#define LCD_ST_SWRESET     3U
#define LCD_ST_SLPOUT      4U
#define LCD_ST_DISPON      5U
#define LCD_ST_READY       6U

#define LCD_RESET_LOW_MS   10U
#define LCD_RESET_HIGH_MS  120U
#define LCD_SWRESET_MS     120U
#define LCD_SLPOUT_MS      120U
#define LCD_DISPON_MS      100U

static uint8_t  s_state;
static uint32_t s_stamp;
static uint8_t  s_present;

/* -------------------------------------------------------------------------- */
/* 5x8 ASCII font (0x20..0x7F), 5 column bytes per glyph, LSB = top row      */
/* -------------------------------------------------------------------------- */

static const uint8_t s_font5x8[96][5] =
{
    {0x00,0x00,0x00,0x00,0x00}, /* ' ' */
    {0x00,0x00,0x5F,0x00,0x00}, /* '!' */
    {0x00,0x07,0x00,0x07,0x00}, /* '"' */
    {0x14,0x7F,0x14,0x7F,0x14}, /* '#' */
    {0x24,0x2A,0x7F,0x2A,0x12}, /* '$' */
    {0x23,0x13,0x08,0x64,0x62}, /* '%' */
    {0x36,0x49,0x55,0x22,0x50}, /* '&' */
    {0x00,0x05,0x03,0x00,0x00}, /* '\'' */
    {0x00,0x1C,0x22,0x41,0x00}, /* '(' */
    {0x00,0x41,0x22,0x1C,0x00}, /* ')' */
    {0x14,0x08,0x3E,0x08,0x14}, /* '*' */
    {0x08,0x08,0x3E,0x08,0x08}, /* '+' */
    {0x00,0x50,0x30,0x00,0x00}, /* ',' */
    {0x08,0x08,0x08,0x08,0x08}, /* '-' */
    {0x00,0x60,0x60,0x00,0x00}, /* '.' */
    {0x20,0x10,0x08,0x04,0x02}, /* '/' */
    {0x3E,0x51,0x49,0x45,0x3E}, /* '0' */
    {0x00,0x42,0x7F,0x40,0x00}, /* '1' */
    {0x42,0x61,0x51,0x49,0x46}, /* '2' */
    {0x21,0x41,0x45,0x4B,0x31}, /* '3' */
    {0x18,0x14,0x12,0x7F,0x10}, /* '4' */
    {0x27,0x45,0x45,0x45,0x39}, /* '5' */
    {0x3C,0x4A,0x49,0x49,0x30}, /* '6' */
    {0x01,0x71,0x09,0x05,0x03}, /* '7' */
    {0x36,0x49,0x49,0x49,0x36}, /* '8' */
    {0x06,0x49,0x49,0x29,0x1E}, /* '9' */
    {0x00,0x36,0x36,0x00,0x00}, /* ':' */
    {0x00,0x56,0x36,0x00,0x00}, /* ';' */
    {0x08,0x14,0x22,0x41,0x00}, /* '<' */
    {0x14,0x14,0x14,0x14,0x14}, /* '=' */
    {0x00,0x41,0x22,0x14,0x08}, /* '>' */
    {0x02,0x01,0x51,0x09,0x06}, /* '?' */
    {0x32,0x49,0x79,0x41,0x3E}, /* '@' */
    {0x7E,0x11,0x11,0x11,0x7E}, /* 'A' */
    {0x7F,0x49,0x49,0x49,0x36}, /* 'B' */
    {0x3E,0x41,0x41,0x41,0x22}, /* 'C' */
    {0x7F,0x41,0x41,0x22,0x1C}, /* 'D' */
    {0x7F,0x49,0x49,0x49,0x41}, /* 'E' */
    {0x7F,0x09,0x09,0x09,0x01}, /* 'F' */
    {0x3E,0x41,0x49,0x49,0x7A}, /* 'G' */
    {0x7F,0x08,0x08,0x08,0x7F}, /* 'H' */
    {0x00,0x41,0x7F,0x41,0x00}, /* 'I' */
    {0x20,0x40,0x41,0x3F,0x01}, /* 'J' */
    {0x7F,0x08,0x14,0x22,0x41}, /* 'K' */
    {0x7F,0x40,0x40,0x40,0x40}, /* 'L' */
    {0x7F,0x02,0x0C,0x02,0x7F}, /* 'M' */
    {0x7F,0x04,0x08,0x10,0x7F}, /* 'N' */
    {0x3E,0x41,0x41,0x41,0x3E}, /* 'O' */
    {0x7F,0x09,0x09,0x09,0x06}, /* 'P' */
    {0x3E,0x41,0x51,0x21,0x5E}, /* 'Q' */
    {0x7F,0x09,0x19,0x29,0x46}, /* 'R' */
    {0x46,0x49,0x49,0x49,0x31}, /* 'S' */
    {0x01,0x01,0x7F,0x01,0x01}, /* 'T' */
    {0x3F,0x40,0x40,0x40,0x3F}, /* 'U' */
    {0x1F,0x20,0x40,0x20,0x1F}, /* 'V' */
    {0x7F,0x20,0x18,0x20,0x7F}, /* 'W' */
    {0x63,0x14,0x08,0x14,0x63}, /* 'X' */
    {0x07,0x08,0x70,0x08,0x07}, /* 'Y' */
    {0x61,0x51,0x49,0x45,0x43}, /* 'Z' */
    {0x00,0x7F,0x41,0x41,0x00}, /* '[' */
    {0x02,0x04,0x08,0x10,0x20}, /* '\\' */
    {0x00,0x41,0x41,0x7F,0x00}, /* ']' */
    {0x04,0x02,0x01,0x02,0x04}, /* '^' */
    {0x40,0x40,0x40,0x40,0x40}, /* '_' */
    {0x00,0x01,0x02,0x04,0x00}, /* '`' */
    {0x20,0x54,0x54,0x54,0x78}, /* 'a' */
    {0x7F,0x48,0x44,0x44,0x38}, /* 'b' */
    {0x38,0x44,0x44,0x44,0x20}, /* 'c' */
    {0x38,0x44,0x44,0x48,0x7F}, /* 'd' */
    {0x38,0x54,0x54,0x54,0x18}, /* 'e' */
    {0x08,0x7E,0x09,0x01,0x02}, /* 'f' */
    {0x0C,0x52,0x52,0x52,0x3E}, /* 'g' */
    {0x7F,0x08,0x04,0x04,0x78}, /* 'h' */
    {0x00,0x44,0x7D,0x40,0x00}, /* 'i' */
    {0x20,0x40,0x44,0x3D,0x00}, /* 'j' */
    {0x7F,0x10,0x28,0x44,0x00}, /* 'k' */
    {0x00,0x41,0x7F,0x40,0x00}, /* 'l' */
    {0x7C,0x04,0x18,0x04,0x78}, /* 'm' */
    {0x7C,0x08,0x04,0x04,0x78}, /* 'n' */
    {0x38,0x44,0x44,0x44,0x38}, /* 'o' */
    {0x7C,0x14,0x14,0x14,0x08}, /* 'p' */
    {0x08,0x14,0x14,0x18,0x7C}, /* 'q' */
    {0x7C,0x08,0x04,0x04,0x08}, /* 'r' */
    {0x48,0x54,0x54,0x54,0x20}, /* 's' */
    {0x04,0x3F,0x44,0x40,0x20}, /* 't' */
    {0x3C,0x40,0x40,0x20,0x7C}, /* 'u' */
    {0x1C,0x20,0x40,0x20,0x1C}, /* 'v' */
    {0x3C,0x40,0x30,0x40,0x3C}, /* 'w' */
    {0x44,0x28,0x10,0x28,0x44}, /* 'x' */
    {0x0C,0x50,0x50,0x50,0x3C}, /* 'y' */
    {0x44,0x64,0x54,0x4C,0x44}, /* 'z' */
    {0x00,0x08,0x36,0x41,0x00}, /* '{' */
    {0x00,0x00,0x7F,0x00,0x00}, /* '|' */
    {0x00,0x41,0x36,0x08,0x00}, /* '}' */
    {0x08,0x08,0x2A,0x1C,0x08}, /* '~' */
    {0x08,0x14,0x22,0x41,0x00}  /* DEL */
};

/* -------------------------------------------------------------------------- */
/* Low level SPI                                                              */
/* -------------------------------------------------------------------------- */

static void Lcd_GpioInit(void)
{
    /* PB8/PB9/PB10 are driven by SPI0: hand them back to the peripheral. */
    GPIOB->PXCON &= ~((1UL << LCD_PIN_SCK) | (1UL << LCD_PIN_MOSI) |
                      (1UL << LCD_PIN_MISO));

    /* No pull resistors on the SPI pins or on the control pins. */
    GPIOB->PXPH &= ~((1UL << LCD_PIN_SCK) | (1UL << LCD_PIN_MOSI) |
                     (1UL << LCD_PIN_MISO) | (1UL << LCD_PIN_CS) |
                     (1UL << LCD_PIN_DC));
    GPIOD->PXPH &= ~(1UL << LCD_PIN_RST);

    /* CS / DC / RST stay GPIO outputs. */
    GPIOB->PXCON |= (1UL << LCD_PIN_CS) | (1UL << LCD_PIN_DC);
    GPIOD->PXCON |= (1UL << LCD_PIN_RST);

    LCD_CS_HIGH();
    LCD_DC_DATA();
    LCD_RST_HIGH();
}

static void Lcd_SpiInit(void)
{
    RCC_APB0Cmd(ENABLE);
    RCC_APB0PeriphClockCmd(RCC_APB0Periph_SPI0, ENABLE);

    TWI_QSPI0->TWI_QSPIx_CON = 0;
    TWI_QSPI0->TWI_QSPIx_IDE = 0;
    TWI_QSPI0->TWI_QSPIx_CON =
          LCD_CON_MSTR
        | LCD_CON_SPEN
        | ((uint32_t)LCD_SPI_SPR << LCD_CON_SPR_Pos)
        | (1UL << LCD_CON_MODE_Pos);       /* full-duplex SPI, mode 0 */
}

static void Lcd_SpiWrite(uint8_t b)
{
    uint32_t guard;

    if (s_present == 0U)
    {
        return;
    }

    guard = LCD_SPI_GUARD;
    while ((TWI_QSPI0->TWI_QSPIx_STS & LCD_STS_TXEIF) == 0U)
    {
        if (--guard == 0U)
        {
            s_present = 0U;
            return;
        }
    }

    TWI_QSPI0->TWI_QSPIx_DATA = b;

    guard = LCD_SPI_GUARD;
    while ((TWI_QSPI0->TWI_QSPIx_STS & LCD_STS_SPIF) == 0U)
    {
        if (--guard == 0U)
        {
            s_present = 0U;
            return;
        }
    }

    (void)TWI_QSPI0->TWI_QSPIx_DATA;        /* drain the RX side */
    /* Both flags are write-1-to-clear. */
    TWI_QSPI0->TWI_QSPIx_STS = LCD_STS_SPIF | LCD_STS_RXNEIF;
}

static void Lcd_WriteCmd(uint8_t cmd)
{
    LCD_DC_CMD();
    LCD_CS_LOW();
    Lcd_SpiWrite(cmd);
    LCD_CS_HIGH();
}

static void Lcd_WriteCmdArgs(uint8_t cmd, const uint8_t *args, uint8_t n)
{
    uint8_t i;

    LCD_DC_CMD();
    LCD_CS_LOW();
    Lcd_SpiWrite(cmd);
    LCD_DC_DATA();
    for (i = 0; i < n; i++)
    {
        Lcd_SpiWrite(args[i]);
    }
    LCD_CS_HIGH();
}

static void Lcd_WritePixel(uint16_t color)
{
    Lcd_SpiWrite((uint8_t)(color >> 8));
    Lcd_SpiWrite((uint8_t)(color & 0xFFU));
}

static void Lcd_SetWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1)
{
    uint8_t a[4];
    uint16_t v;

    v = (uint16_t)(x0 + LCD_X_OFFSET);
    a[0] = (uint8_t)(v >> 8);
    a[1] = (uint8_t)v;
    v = (uint16_t)(x1 + LCD_X_OFFSET);
    a[2] = (uint8_t)(v >> 8);
    a[3] = (uint8_t)v;
    Lcd_WriteCmdArgs(0x2AU, a, 4);

    v = (uint16_t)(y0 + LCD_Y_OFFSET);
    a[0] = (uint8_t)(v >> 8);
    a[1] = (uint8_t)v;
    v = (uint16_t)(y1 + LCD_Y_OFFSET);
    a[2] = (uint8_t)(v >> 8);
    a[3] = (uint8_t)v;
    Lcd_WriteCmdArgs(0x2BU, a, 4);

    Lcd_WriteCmd(0x2CU);                   /* memory write */
}

/* -------------------------------------------------------------------------- */
/* ST7735S initialization                                                     */
/* -------------------------------------------------------------------------- */

static void Lcd_St7735Config(void)
{
    static const uint8_t b1[3] = {0x01, 0x2C, 0x2D};
    static const uint8_t b3[6] = {0x01, 0x2C, 0x2D, 0x01, 0x2C, 0x2D};
    static const uint8_t c0[3] = {0xA2, 0x02, 0x84};
    static const uint8_t c2[2] = {0x0A, 0x00};
    static const uint8_t c3[2] = {0x8A, 0x2A};
    static const uint8_t c4[2] = {0x8A, 0xEE};
    static const uint8_t one1[1] = {0x07};
    static const uint8_t one2[1] = {0xC5};
    static const uint8_t one3[1] = {0x0E};
    static const uint8_t one4[1] = {0xC8};   /* MADCTL: MX|MY|RGB */
    static const uint8_t one5[1] = {0x05};   /* COLMOD: RGB565 */

    Lcd_WriteCmdArgs(0xB1U, b1, 3);          /* frame rate normal */
    Lcd_WriteCmdArgs(0xB2U, b1, 3);          /* frame rate idle */
    Lcd_WriteCmdArgs(0xB3U, b3, 6);          /* frame rate partial */
    Lcd_WriteCmdArgs(0xB4U, one1, 1);        /* inversion control */
    Lcd_WriteCmdArgs(0xC0U, c0, 3);          /* power control 1 */
    Lcd_WriteCmdArgs(0xC1U, one2, 1);        /* power control 2 */
    Lcd_WriteCmdArgs(0xC2U, c2, 2);          /* power control 3 */
    Lcd_WriteCmdArgs(0xC3U, c3, 2);          /* power control 4 */
    Lcd_WriteCmdArgs(0xC4U, c4, 2);          /* power control 5 */
    Lcd_WriteCmdArgs(0xC5U, one3, 1);        /* VCOM */
    Lcd_WriteCmd(0x20U);                     /* display inversion off */
    Lcd_WriteCmdArgs(0x36U, one4, 1);        /* orientation */
    Lcd_WriteCmdArgs(0x3AU, one5, 1);        /* colour mode */
    Lcd_WriteCmd(0x13U);                     /* normal display on */
    Lcd_WriteCmd(0x29U);                     /* display on */
}

void Lcd_InitStart(uint32_t now_ms)
{
    s_present = 1U;
    s_state   = LCD_ST_RESET_LOW;
    s_stamp   = now_ms;

    Lcd_GpioInit();
    Lcd_SpiInit();

    LCD_CS_HIGH();
    LCD_DC_DATA();
    LCD_RST_LOW();
}

uint8_t Lcd_InitPoll(uint32_t now_ms)
{
    uint32_t elapsed = now_ms - s_stamp;

    switch (s_state)
    {
        case LCD_ST_RESET_LOW:
            if (elapsed >= LCD_RESET_LOW_MS)
            {
                LCD_RST_HIGH();
                s_stamp = now_ms;
                s_state = LCD_ST_RESET_WAIT;
            }
            break;

        case LCD_ST_RESET_WAIT:
            if (elapsed >= LCD_RESET_HIGH_MS)
            {
                Lcd_WriteCmd(0x01U);        /* SWRESET */
                s_stamp = now_ms;
                s_state = LCD_ST_SWRESET;
            }
            break;

        case LCD_ST_SWRESET:
            if (elapsed >= LCD_SWRESET_MS)
            {
                Lcd_WriteCmd(0x11U);        /* SLPOUT */
                s_stamp = now_ms;
                s_state = LCD_ST_SLPOUT;
            }
            break;

        case LCD_ST_SLPOUT:
            if (elapsed >= LCD_SLPOUT_MS)
            {
                Lcd_St7735Config();
                s_stamp = now_ms;
                s_state = LCD_ST_DISPON;
            }
            break;

        case LCD_ST_DISPON:
            if (elapsed >= LCD_DISPON_MS)
            {
                s_state = LCD_ST_READY;
            }
            break;

        default:
            break;
    }

    return (s_state == LCD_ST_READY) ? 1U : 0U;
}

uint8_t Lcd_IsReady(void)
{
    return (s_state == LCD_ST_READY) ? 1U : 0U;
}

uint8_t Lcd_IsPresent(void)
{
    return s_present;
}

/* -------------------------------------------------------------------------- */
/* Drawing                                                                    */
/* -------------------------------------------------------------------------- */

void Lcd_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                  uint16_t color)
{
    uint32_t n;

    if ((s_state != LCD_ST_READY) || (w == 0U) || (h == 0U))
    {
        return;
    }
    if (x >= LCD_W || y >= LCD_H)
    {
        return;
    }
    if ((uint32_t)x + w > LCD_W)
    {
        w = (uint16_t)(LCD_W - x);
    }
    if ((uint32_t)y + h > LCD_H)
    {
        h = (uint16_t)(LCD_H - y);
    }

    Lcd_SetWindow(x, y, (uint16_t)(x + w - 1U), (uint16_t)(y + h - 1U));

    LCD_DC_DATA();
    LCD_CS_LOW();
    n = (uint32_t)w * h;
    while (n-- != 0U)
    {
        Lcd_WritePixel(color);
    }
    LCD_CS_HIGH();
}

void Lcd_Fill(uint16_t color)
{
    Lcd_FillRect(0U, 0U, LCD_W, LCD_H, color);
}

void Lcd_DrawChar(uint16_t x, uint16_t y, char c,
                  uint16_t fg, uint16_t bg, uint8_t scale)
{
    const uint8_t *glyph;
    uint8_t  ch;
    uint8_t  col;
    uint8_t  row;
    uint8_t  sx;
    uint8_t  sy;
    uint16_t color;

    if (s_state != LCD_ST_READY)
    {
        return;
    }
    if (scale == 0U)
    {
        scale = 1U;
    }
    if ((uint32_t)x + 6U * scale > LCD_W ||
        (uint32_t)y + 8U * scale > LCD_H)
    {
        return;
    }

    ch = (uint8_t)c;
    if (ch < 0x20U || ch > 0x7FU)
    {
        ch = (uint8_t)'?';
    }
    glyph = s_font5x8[ch - 0x20U];

    Lcd_SetWindow(x, y,
                  (uint16_t)(x + 6U * scale - 1U),
                  (uint16_t)(y + 8U * scale - 1U));

    LCD_DC_DATA();
    LCD_CS_LOW();

    for (row = 0; row < 8U; row++)
    {
        for (col = 0; col < 6U; col++)
        {
            uint8_t bits = (col < 5U) ? glyph[col] : 0U;

            color = ((bits >> row) & 0x01U) ? fg : bg;

            for (sy = 0; sy < scale; sy++)
            {
                for (sx = 0; sx < scale; sx++)
                {
                    Lcd_WritePixel(color);
                }
            }
        }
    }

    LCD_CS_HIGH();
}

void Lcd_DrawString(uint16_t x, uint16_t y, const char *s,
                    uint16_t fg, uint16_t bg, uint8_t scale)
{
    uint16_t cx = x;

    if (scale == 0U)
    {
        scale = 1U;
    }

    while (*s != '\0')
    {
        Lcd_DrawChar(cx, y, *s, fg, bg, scale);
        cx = (uint16_t)(cx + 6U * scale);
        s++;
    }
}

void Lcd_ProgressBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                     uint8_t percent, uint16_t fg, uint16_t bg)
{
    uint16_t fill;

    if (percent > 100U)
    {
        percent = 100U;
    }
    if (w < 4U || h < 4U)
    {
        return;
    }

    Lcd_FillRect(x, y, w, h, bg);
    Lcd_FillRect(x, y, w, 1U, LCD_COLOR_GRAY);
    Lcd_FillRect(x, (uint16_t)(y + h - 1U), w, 1U, LCD_COLOR_GRAY);
    Lcd_FillRect(x, y, 1U, h, LCD_COLOR_GRAY);
    Lcd_FillRect((uint16_t)(x + w - 1U), y, 1U, h, LCD_COLOR_GRAY);

    if (percent > 0U)
    {
        fill = (uint16_t)(((uint32_t)(w - 2U) * percent) / 100U);
        if (fill == 0U)
        {
            fill = 1U;
        }
        Lcd_FillRect((uint16_t)(x + 1U), (uint16_t)(y + 1U),
                     fill, (uint16_t)(h - 2U), fg);
    }
}
