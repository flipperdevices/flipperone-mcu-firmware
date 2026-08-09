#pragma once
#include <furi.h>

#define RECORD_PD "pd"

typedef struct Pd Pd;

typedef enum {
    PdDeviceFusb302 = (1 << 0),
} PdDevice;

typedef enum {
    PdModeOff,
    PdModeDrp,
    PdModeSnk,
    PdModeSrc,
    PdModeCount,
} PdMode;

#ifdef __cplusplus
extern "C" {
#endif

bool pd_is_device_initialized(Pd* instance, PdDevice* device);

bool pd_reset_config(Pd* instance);

#ifdef __cplusplus
}
#endif
