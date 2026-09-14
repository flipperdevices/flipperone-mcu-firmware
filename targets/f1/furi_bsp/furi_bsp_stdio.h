#pragma once

#include <pico/stdio.h>

#ifndef FURI_BSP_DEFAULT_CRLF
#define FURI_BSP_DEFAULT_CRLF PICO_STDIO_DEFAULT_CRLF
#endif

#ifdef __cplusplus
extern "C" {
#endif

extern stdio_driver_t furi_bsp_stdio;

/** Initialize BSP stdio driver
 */
void furi_bsp_stdio_init(void);

/** Deinitialize BSP stdio driver
 */
void furi_bsp_stdio_deinit(void);

#ifdef __cplusplus
}
#endif
