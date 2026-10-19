#pragma once

#include <furi.h>

typedef struct {
    bool in_connected;
    bool in_pd_valid;
    bool error;
    uint8_t battery_percent;
    uint32_t power_consumption_mw;
    uint32_t in_power_mw;
} PowerMeterState;

int32_t power_meter_srv(void* p);

void power_meter_test_get_state(PowerMeterState* state, void* context);
