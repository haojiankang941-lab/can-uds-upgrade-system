/**
 ******************************************************************************
 * @file    lcd.h
 * @brief   ST7735S 128x160 SPI display driver (4-wire SPI)
 * @note    SPI0: SCK = PB8, MOSI = PB9.
 *          CS = PB13, DC = PB15, RST = PD8 (plain GPIO, software driven).
 *          MISO is not connected: the display is write-only.
 *
 *          Initialization is split into small steps so the main loop keeps
 *          servicing CAN/ISO-TP/UDS while the panel powers up.
 ******************************************************************************
 */

#ifndef __LCD_H
#define __LCD_H

#include <stdint.h>

/* -------------------------------------------------------------------------- */
/* Panel geometry                                                             */
/* -------------------------------------------------------------------------- */

#define LCD_W              128U
#define LCD_H              160U

/* Some 1.8" modules use a visible area that starts a few pixels inside the
 * controller RAM.  Adjust these two values if the image is shifted. */
#define LCD_X_OFFSET       0U
#define LCD_Y_OFFSET       0U

/* -------------------------------------------------------------------------- */
/* Pin assignment (change only these if the wiring differs)                   */
/* -------------------------------------------------------------------------- */

#define LCD_PIN_SCK        8U      /* PB8  */
#define LCD_PIN_MOSI       9U      /* PB9  */
#define LCD_PIN_MISO       10U     /* PB10 (unused, kept as input) */
#define LCD_PIN_CS         13U     /* PB13 */
#define LCD_PIN_DC         15U     /* PB15 */
#define LCD_PIN_RST        8U      /* PD8  */

/* SPI0 = APB0 / 4 = 8 MHz (the smallest divider allowed by the spec). */
#define LCD_SPI_SPR        2U

/* How long ACTIVATED stays visible before jumping to the new APP. */
#define LCD_ACTIVATE_HOLD_MS  300U

/* -------------------------------------------------------------------------- */
/* RGB565 colours                                                             */
/* -------------------------------------------------------------------------- */

#define LCD_COLOR_BLACK    0x0000U
#define LCD_COLOR_WHITE    0xFFFFU
#define LCD_COLOR_RED      0xF800U
#define LCD_COLOR_GREEN    0x07E0U
#define LCD_COLOR_CYAN     0x07FFU
#define LCD_COLOR_YELLOW   0xFFE0U
#define LCD_COLOR_GRAY     0x8410U
#define LCD_COLOR_DARK     0x2104U

/* -------------------------------------------------------------------------- */
/* API                                                                        */
/* -------------------------------------------------------------------------- */

/* Start the non-blocking panel init sequence (GPIO + SPI0 + reset). */
void     Lcd_InitStart(uint32_t now_ms);

/* Advance the init sequence.  Returns 1 once the panel accepts pixels. */
uint8_t  Lcd_InitPoll(uint32_t now_ms);

/* 1 when the init sequence finished and the panel is usable. */
uint8_t  Lcd_IsReady(void);

/* 0 after an SPI timeout.  Kept at 0 so a missing panel costs no time. */
uint8_t  Lcd_IsPresent(void);

void     Lcd_Fill(uint16_t color);
void     Lcd_FillRect(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                      uint16_t color);
void     Lcd_DrawChar(uint16_t x, uint16_t y, char c,
                      uint16_t fg, uint16_t bg, uint8_t scale);
void     Lcd_DrawString(uint16_t x, uint16_t y, const char *s,
                        uint16_t fg, uint16_t bg, uint8_t scale);
void     Lcd_ProgressBar(uint16_t x, uint16_t y, uint16_t w, uint16_t h,
                         uint8_t percent, uint16_t fg, uint16_t bg);

#endif /* __LCD_H */
