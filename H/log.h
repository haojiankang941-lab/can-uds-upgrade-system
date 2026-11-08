/**
 ******************************************************************************
 * @file    log.h
 * @brief   Optional UART debug log
 * @note    Set BOOT_LOG_ENABLE to 1 to enable.  When it is 0 every LOG/LOGF
 *          expands to an empty statement, so the arguments are not evaluated
 *          and printf is never linked into the image.
 *
 *          LOGF() must always be called with at least one argument; use LOG()
 *          for plain strings.  This avoids the GNU ", ##__VA_ARGS__" extension.
 ******************************************************************************
 */

#ifndef __LOG_H
#define __LOG_H

#include <stdio.h>
#include "boot_can.h"

/* 1 = enable UART log, 0 = compile everything out (release default). */
#define BOOT_LOG_ENABLE   0

/* Only used when BOOT_LOG_ENABLE is 1.  9600 is too slow: one character
 * blocks for about 1 ms and would break the UDS P2 timing. */
#define BOOT_LOG_BAUD     115200UL

#if BOOT_LOG_ENABLE

#define LOG(msg) \
    do { printf("[%lu ms] %s\r\n", (unsigned long)BootCan_GetMs(), (msg)); } while (0)

#define LOGF(fmt, ...) \
    do { printf("[%lu ms] " fmt "\r\n", (unsigned long)BootCan_GetMs(), __VA_ARGS__); } while (0)

#else

#define LOG(msg)          do { } while (0)
#define LOGF(fmt, ...)    do { } while (0)

#endif

#endif /* __LOG_H */
