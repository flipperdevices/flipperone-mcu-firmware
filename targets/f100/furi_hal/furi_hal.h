/**
 * @file furi_hal.h
 * Furi HAL API
 */

#pragma once

#ifdef __cplusplus
template <unsigned int N>
struct STOP_EXTERNING_ME {};
#endif
// #include <furi_hal_rtc.h>
// #include <furi_hal_flash.h>
#include <furi_hal_version.h>
#include <furi_hal_memory.h>
// #include <furi_hal_debug.h>
#include <furi_hal_interrupt.h>
#include <furi_hal_os.h>
// #include <furi_hal_cortex.h>
// #include <furi_hal_bus.h>
// #include <furi_hal_target.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Early FuriHal init
 * 
 * Init essential subsystems used in pre-DFU stage.
 * This state can be undone with `furi_hal_deinit_early`.
 *
 */
void furi_hal_init_early(void);

/** Early FuriHal deinit
 * 
 * Undo `furi_hal_init_early`, prepare system for switch to another firmware/bootloader.
 */
void furi_hal_deinit_early(void);

/** Init FuriHal
 * 
 * Initialize the rest of the HAL, must be used after `furi_hal_init_early`.
 */
void furi_hal_init(void);

/** Jump to the void*
 *
 * Allow your code to transfer control to another firmware.
 *
 * @warning    This code doesn't reset system before jump. Call it only from
 *             main thread, no kernel should be running. Ensure that no
 *             peripheral blocks active and no interrupts are pending.
 *
 * @param      address  The System Vector address(start of your new firmware)
 */
void furi_hal_switch(void* address);

#ifdef __cplusplus
}
#endif
