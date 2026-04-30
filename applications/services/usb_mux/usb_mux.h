#pragma once
#include <furi.h>

#define RECORD_USBMUX "usbmux"

typedef struct UsbMux UsbMux;

typedef enum {
   UsbMuxDeviceHd3ss3220 = (1 << 0),
} UsbMuxDevice;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Checks if the UsbMux device is initialized.
 * 
 * @param instance The UsbMux instance.
 * @param device Pointer to store the initialized device(s).
 * @return true  if initialization was successful, false otherwise.
 */
bool usb_mux_is_device_initialized(UsbMux* instance, UsbMuxDevice* device);

#ifdef __cplusplus
}
#endif
