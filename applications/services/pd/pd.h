#pragma once
#include <furi.h>

#define RECORD_PD "pd"

typedef struct Pd Pd;

typedef enum {
   PdDeviceFusb302 = (1 << 0),
} PdDevice;

typedef enum {
    PdPortStateToggling,       // DRP toggle in progress, no role determined yet
    PdPortStateSourceCC1,      // Flipper is power source, cable on CC1
    PdPortStateSourceCC2,      // Flipper is power source, cable on CC2
    PdPortStateSinkCC1,        // Flipper is charging (sink), cable on CC1
    PdPortStateSinkCC2,        // Flipper is charging (sink), cable on CC2
    PdPortStateAudioAccessory, // Audio adapter detected
    PdPortStateOvercurrent,    // VCONN over-current or over-temperature event
    PdPortStateUndefined,      // Unknown or undetected state
} PdPortState;

typedef struct {
    PdPortState port_state;
} PdEvent;

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
FuriPubSub* pd_get_pubsub(Pd* pd);
bool pd_reset_config(Pd* instance);
bool pd_set_mode(Pd* instance, PdMode mode);
bool pd_get_mode(Pd* instance, PdMode* mode);

#ifdef __cplusplus
}
#endif
