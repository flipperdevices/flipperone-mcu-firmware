#pragma once
#include <furi.h>

#define RECORD_POWER "power"
typedef struct Power Power;

#ifdef __cplusplus
extern "C" {
#endif
FuriPubSub* power_get_pubsub(Power* power);
float_t power_ina219_get_voltage_v(Power* instance);
float_t power_ina219_get_current_a(Power* instance);
float_t power_ina219_get_power_w(Power* instance);
float_t power_ina219_get_shunt_voltage_mv(Power* instance);

#ifdef __cplusplus
}
#endif
