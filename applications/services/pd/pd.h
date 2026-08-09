#pragma once
#include <furi.h>
#include <toolbox/furi_callback.h>

#include <usb_pd.h>

#define RECORD_PD "pd"

typedef struct Pd Pd;

typedef enum {
    PdDeviceFusb302 = (1 << 0),
} PdDevice;

#ifdef __cplusplus
extern "C" {
#endif

bool pd_is_device_initialized(Pd* instance, PdDevice* device);

// Reset the PD stack: drops any contract, re-inits the FUSB302 and returns
// the PPM to its post-boot state. Blocks until the service thread has
// accepted the request, not until the reset itself has completed — watch the
// pubsub for that.
// @returns false if the PD stack failed to come up at boot.
bool pd_reset_config(Pd* instance);

// The underlying PD/UCSI stack, or NULL if it failed to come up. Owned by
// this service — do not free. Intended for the code that exposes UCSI to an
// external OPM (see i2c_negotiator) and needs usb_pd_ucsi_read/write.
UsbPd* pd_get_usb_pd(Pd* instance);

// Called when the PPM has news for the OPM (CCI updated). Runs on the UsbPd
// worker thread right after the register file image is refreshed, so the
// callback may serve OPM reads immediately. Pass NULL to detach.
void pd_set_ucsi_alert_callback(Pd* instance, FuriCallback callback, void* context);

// PD connector / contract events, delivering UsbPdEvent. Returns NULL if the
// stack failed to come up. Callbacks run on the UsbPd worker thread.
FuriPubSub* pd_get_pubsub(Pd* instance);

#ifdef __cplusplus
}
#endif
