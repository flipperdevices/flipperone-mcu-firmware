#pragma once

#include <stdint.h>
#include <stdbool.h>

#include <furi_hal_serial_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize Serial Control */
void furi_hal_serial_control_init(void);

/** De-Initialize Serial Control */
void furi_hal_serial_control_deinit(void);

/** Suspend All Serial Interfaces
 *
 * @warning    this is internal method, can only be used in suppress tick
 *             callback
 */
void furi_hal_serial_control_suspend(void);

/** Resume All Serial Interfaces
 *
 * @warning    this is internal method, can only be used in suppress tick
 *             callback
 */
void furi_hal_serial_control_resume(void);

/** Acquire Serial Interface Handler
 *
 * @param[in]  serial_id  The serial transceiver identifier
 *
 * @return     The Serial Interface Handle or null if interfaces is in use
 */
FuriHalSerialHandle* furi_hal_serial_control_acquire(FuriHalSerialId serial_id);

/** Release Serial Interface Handler
 *
 * @param      handle  The handle
 */
void furi_hal_serial_control_release(FuriHalSerialHandle* handle);

/** Check if the Serial Interface has been acquired
 *
 * @param[in]  serial_id  The serial transceiver identifier
 *
 * @return     true if handle is acquired by someone
 */
bool furi_hal_serial_control_is_busy(FuriHalSerialId serial_id);

/** Set which Serial Interface to use for logging
 *
 * @param[in]  serial_id  The serial transceiver identifier. Use FuriHalSerialIdMax to disable logging.
 * @param[in]  baud_rate  The baud rate
 *
 * @return     The Serial Interface Handle or null if interfaces is in use
 */
void furi_hal_serial_control_set_logging_config(FuriHalSerialId serial_id, uint32_t baud_rate);

#ifdef __cplusplus
}
#endif
