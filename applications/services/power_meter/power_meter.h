#pragma once
#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * Power meter service: drives the battery and USB LED groups from the
 * charger, the fuel gauge and the system current sensor.
 */
int32_t power_meter_srv(void* p);

#ifdef __cplusplus
}
#endif
