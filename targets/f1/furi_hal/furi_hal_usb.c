#include <furi_hal_usb.h>
#include <furi.h>
#include <tusb.h>
#include <device/usbd_pvt.h>

#define FURI_HAL_USB_DISCONNECT_MS 250

static bool furi_hal_usb_reconnect_pending = false;

// Runs in the USB service thread, blocking it is fine while disconnected
static void furi_hal_usb_reconnect_worker(void* param) {
    UNUSED(param);
    tud_disconnect();
    furi_delay_ms(FURI_HAL_USB_DISCONNECT_MS);
    tud_connect();
    __atomic_store_n(&furi_hal_usb_reconnect_pending, false, __ATOMIC_RELEASE);
}

void furi_hal_usb_reconnect(void) {
    // The USB service may not have started yet
    if(!tud_inited()) return;

    if(!__atomic_test_and_set(&furi_hal_usb_reconnect_pending, __ATOMIC_ACQ_REL)) {
        usbd_defer_func(furi_hal_usb_reconnect_worker, NULL, FURI_IS_IRQ_MODE());
    }
}
