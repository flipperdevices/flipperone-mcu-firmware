#pragma once
#include <furi.h>

#define RECORD_FUSB302 "fusb302"
typedef struct Fusb302 Fusb302;

typedef enum {
    Fusb302ModeOff,
    Fusb302ModeDrp,
    Fusb302ModeSnk,
    Fusb302ModeSrc,
    Fusb302ModeCount,
} Fusb302Mode;

#ifdef __cplusplus
extern "C" {
#endif
FuriPubSub* fusb302_get_pubsub(Fusb302* fusb302);
void fusb302_set_mode(Fusb302* instance, Fusb302Mode mode);
Fusb302Mode fusb302_get_mode(Fusb302* instance);

#ifdef __cplusplus
}
#endif
