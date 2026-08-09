#include "pd.h"

#include <furi.h>
#include <api_lock.h>
#include <furi_hal_i2c_config.h>
#include <furi_hal_resources.h>
#include <drivers/fusb302/fusb302.h>
#include <furi_bsp.h>

#define TAG "Pd"

struct Pd {
    bool initialized;
    FuriEventLoop* event_loop;
};

static Pd* pd_alloc(void) {
    Pd* instance = malloc(sizeof(Pd));
    instance->initialized = true;

    instance->event_loop = furi_event_loop_alloc();

    furi_record_create(RECORD_PD, instance);

    return instance;
}

bool pd_is_device_initialized(Pd* instance, PdDevice* device) {
    furi_check(instance);

    if(instance->initialized) {
        if(device) {
            *device = PdDeviceFusb302;
        }
    } else {
        if(device) {
            *device = 0;
        }

        FURI_LOG_E(TAG, "PD device not initialized");
    }
    return instance->initialized;
}

bool pd_reset_config(Pd* instance) {
    // furi_check(instance);
    // if(pd_is_device_initialized(instance, NULL)) {
    //     PdMessage msg = {
    //         .type = PdMessageTypeResetConfig,
    //         .lock = api_lock_alloc_locked(),
    //     };

    //     pd_send_message(instance, &msg);
    //     return true;
    // }
    return false;
}

int32_t pd_srv(void* p) {
    UNUSED(p);

    Pd* instance = pd_alloc();
    furi_event_loop_run(instance->event_loop);

    return 0;
}
