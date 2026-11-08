/**
 ******************************************************************************
 * @file    lcd_ui.h
 * @brief   Bootloader status screen (version / state / progress)
 * @note    Lcd_UiPoll() must be called from the main loop.  It also drives
 *          the non-blocking panel init, so the LCD never delays UDS handling.
 ******************************************************************************
 */

#ifndef __LCD_UI_H
#define __LCD_UI_H

#include <stdint.h>
#include "uds.h"

/* Start the panel init and schedule the static screen. */
void Lcd_UiInit(void);

/* Main loop hook: advance init and refresh changed fields (200 ms). */
void Lcd_UiPoll(uint32_t now_ms);

/* Redraw only the state line.  Used right before a long blocking operation
 * (0x34 / 0x31 FF00 / FF02) so ERASING / COPYING is visible before it starts.
 * Bounded to one text line (about 2 ms). */
void Lcd_UiLongOp(UdsUiStateTypeDef state);

/* Force an immediate full dynamic refresh (used before jumping to APP). */
void Lcd_UiRefreshNow(void);

#endif /* __LCD_UI_H */
