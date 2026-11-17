#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Force the host to enumerate the USB device again
 *
 * Drops the D+ pull-up for a moment and brings it back, which the host sees
 * as an unplug and replug. Safe to call from a thread or an ISR, the
 * reconnect itself runs in the USB service thread.
 */
void furi_hal_usb_reconnect(void);

#ifdef __cplusplus
}
#endif
