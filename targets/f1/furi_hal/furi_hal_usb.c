#include <furi_hal_usb.h>
#include <furi.h>
#include <tusb.h>
#include <device/usbd_pvt.h>

#define FURI_HAL_USB_DISCONNECT_MS 250

static volatile bool furi_hal_usb_reconnect_pending = false;

// Runs in the USB service thread, blocking it is fine while disconnected
static void furi_hal_usb_reconnect_worker(void* param) {
    UNUSED(param);
    tud_disconnect();
    furi_delay_ms(FURI_HAL_USB_DISCONNECT_MS);
    tud_connect();
    furi_hal_usb_reconnect_pending = false;
}

void furi_hal_usb_reconnect(void) {
    // The USB service may not have started yet
    if(!tud_inited()) return;

    if(!furi_hal_usb_reconnect_pending) {
        furi_hal_usb_reconnect_pending = true;
        usbd_defer_func(furi_hal_usb_reconnect_worker, NULL, FURI_IS_IRQ_MODE());
    }
}
