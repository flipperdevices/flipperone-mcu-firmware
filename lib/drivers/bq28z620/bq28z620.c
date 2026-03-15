#include "bq28z620.h"
#include <furi.h>

#include <furi_hal_i2c.h>

#define TAG "Bq28z620"

#define BQ28Z620_DEBUG_ENABLE

#ifdef BQ28Z620_DEBUG_ENABLE
#define BQ28Z620_DEBUG(...) FURI_LOG_D(__VA_ARGS__)
#else
#define BQ28Z620_DEBUG(...)
#endif

struct Bq28z620 {
    const FuriHalI2cBusHandle* i2c_handle;
    uint8_t address;
    void* context;
};

static Bq28z620Status bq28z620_check_status(int stataus) {
    Bq28z620Status ret = Bq28z620StatusUnknown;
    if(stataus >= PICO_OK) {
        ret = Bq28z620StatusOk;
    } else if(stataus == PICO_ERROR_GENERIC) {
        ret = Bq28z620StatusError;
    } else if(stataus == PICO_ERROR_TIMEOUT) {
        ret = Bq28z620StatusTimeout;
    } else {
        ret = Bq28z620StatusUnknown;
    }

    return ret;
}

static Bq28z620Status bq28z620_std_cmd(Bq28z620* instance, Bq28z620StdCmd std_cmd, uint8_t* response, size_t response_length) {
    furi_check(instance);

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret =
        furi_hal_i2c_master_trx_blocking(instance->i2c_handle, instance->address, (uint8_t*)&std_cmd, 1, response, response_length, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
        FURI_LOG_E(TAG, "Failed to send std cmd 0x%02X", std_cmd);
    } else {
        // BQ28Z620_DEBUG(TAG, "Sent std cmd 0x%02X", std_cmd);
        // if(response && response_length > 0) {
        //     BQ28Z620_DEBUG(TAG, "Response: %d bytes", response_length);
        //     for(size_t i = 0; i < response_length; i++) {
        //         BQ28Z620_DEBUG(TAG, "  %02X: %02X", i, response[i]);
        //     }
        // }
    }

    return bq28z620_check_status(ret);
}

Bq28z620* bq28z620_init(const FuriHalI2cBusHandle* i2c_handle, uint8_t address) {
    Bq28z620* instance = (Bq28z620*)malloc(sizeof(Bq28z620));
    instance->i2c_handle = i2c_handle;
    instance->address = address;

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_device_ready(instance->i2c_handle, instance->address, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);
    if(ret) {
        FURI_LOG_I(TAG, "BQ28Z620 device ready at address 0x%02X", instance->address);
    } else {
        FURI_LOG_E(TAG, "BQ28Z620 device not ready at address 0x%02X", instance->address);
        free(instance);
        return NULL;
    }

    return instance;
}

void bq28z620_deinit(Bq28z620* instance) {
    furi_check(instance);

    free(instance);
}

Bq28z620Status bq28z620_get_control_status(Bq28z620* instance, Bq28z620StdCmdControlStatusRegBits* control_status) {
    furi_check(instance);
    furi_check(control_status);

    Bq28z620Status status = bq28z620_std_cmd(instance, Bq28z620StdCmdManufacturerAccessAndControlStatus, (uint8_t*)control_status, sizeof(*control_status));
    if(status == Bq28z620StatusOk) {
        BQ28Z620_DEBUG(
            TAG,
            "Control status, qmax: %d, vok: %d, r_dis: %d, ldmd: %d, checksum_valid: %d, authcalm: %d, sec: %02b",
            control_status->qmax,
            control_status->vok,
            control_status->r_dis,
            control_status->ldmd,
            control_status->checksum_valid,
            control_status->authcalm,
            control_status->sec);
    } else {
        FURI_LOG_E(TAG, "Failed to get control status");
    }

    return status;
}

Bq28z620Status bq28z620_get_time_to_empty(Bq28z620* instance, uint16_t* time_to_empty) {
    furi_check(instance);
    furi_check(time_to_empty);

    Bq28z620StdCmdAtRateTimeToEmptyRegBits time_to_empty_reg = {0};
    Bq28z620Status status = bq28z620_std_cmd(instance, Bq28z620StdCmdAtRateTimeToEmpty, (uint8_t*)&time_to_empty_reg, sizeof(time_to_empty_reg));
    if(status == Bq28z620StatusOk) {
        *time_to_empty = time_to_empty_reg.time_to_empty;
        BQ28Z620_DEBUG(TAG, "Raw AtRateTimeToEmpty reg: %u, Time to empty: %u minutes", time_to_empty_reg.time_to_empty, *time_to_empty);
    } else {
        FURI_LOG_E(TAG, "Failed to get time to empty");
    }

    return status;
}

Bq28z620Status bq28z620_get_temperature(Bq28z620* instance, float* temperature) {
    furi_check(instance);
    furi_check(temperature);

    Bq28z620StdCmdTemperatureRegBits temp_reg = {0};
    Bq28z620Status status = bq28z620_std_cmd(instance, Bq28z620StdCmdTemperature, (uint8_t*)&temp_reg, sizeof(temp_reg));
    if(status == Bq28z620StatusOk) {
        *temperature = (float)temp_reg.temperature * 0.1f - 273.15f;
        BQ28Z620_DEBUG(TAG, "Raw temperature reg: %u, Temperature: %.2f C", (uint16_t)temp_reg.temperature, *temperature);
    } else {
        FURI_LOG_E(TAG, "Failed to get temperature");
    }

    return status;
}

Bq28z620Status bq28z620_get_voltage(Bq28z620* instance, float* voltage) {
    furi_check(instance);
    furi_check(voltage);

    Bq28z620StdCmdVoltageRegBits voltage_reg = {0};
    Bq28z620Status status = bq28z620_std_cmd(instance, Bq28z620StdCmdVoltage, (uint8_t*)&voltage_reg, sizeof(voltage_reg));
    if(status == Bq28z620StatusOk) {
        *voltage = (float)voltage_reg.voltage * 0.001f;
        BQ28Z620_DEBUG(TAG, "Raw voltage reg: %u, Voltage: %.3f V", (uint16_t)voltage_reg.voltage, *voltage);
    } else {
        FURI_LOG_E(TAG, "Failed to get voltage");
    }

    return status;
}

Bq28z620Status bq28z620_get_battery_status(Bq28z620* instance, Bq28z620StdCmdBatteryStatusRegBits* battery_status) {
    furi_check(instance);
    furi_check(battery_status);

    Bq28z620Status status = bq28z620_std_cmd(instance, Bq28z620StdCmdBatteryStatus, (uint8_t*)battery_status, sizeof(Bq28z620StdCmdBatteryStatusRegBits));
    if(status == Bq28z620StatusOk) {
        BQ28Z620_DEBUG(
            TAG,
            " Battery Status, error code: 0x%02X, fully discharged: %d,"
            " fully charged: %d, discharging: %d, initialization: %d, remaining time alarm: %d,"
            " remaining capacity alarm: %d, terminate discharge alarm: %d, overtemperature alarm: %d,"
            " terminate charge alarm: %d, overcharged alarm: %d",
            battery_status->error_code,
            battery_status->fully_discharged,
            battery_status->fully_charged,
            battery_status->discharging,
            battery_status->initialization,
            battery_status->remaining_time_alarm,
            battery_status->remaining_capacity_alarm,
            battery_status->terminate_discharge_alarm,
            battery_status->overtemperature_alarm,
            battery_status->terminate_charge_alarm,
            battery_status->overcharged_alarm);
    } else {
        FURI_LOG_E(TAG, "Failed to get battery status");
    }

    return status;
}

Bq28z620Status bq28z620_get_current(Bq28z620* instance, int16_t* current) {
    furi_check(instance);
    furi_check(current);

    Bq28z620StdCmdCurrentRegBits current_reg = {0};
    Bq28z620Status status = bq28z620_std_cmd(instance, Bq28z620StdCmdCurrent, (uint8_t*)&current_reg, sizeof(current_reg));
    if(status == Bq28z620StatusOk) {
        *current = (int16_t)current_reg.current;
        BQ28Z620_DEBUG(TAG, "Raw Current reg: %d, Current: %d mA", current_reg.current, *current);
    } else {
        FURI_LOG_E(TAG, "Failed to get current");
    }

    return status;
}

Bq28z620Status bq28z620_get_remaining_capacity(Bq28z620* instance, uint16_t* remaining_capacity) {
    furi_check(instance);
    furi_check(remaining_capacity);

    Bq28z620StdCmdRemainingCapacityRegBits remaining_capacity_reg = {0};
    Bq28z620Status status = bq28z620_std_cmd(instance, Bq28z620StdCmdRemainingCapacity, (uint8_t*)&remaining_capacity_reg, sizeof(remaining_capacity_reg));
    if(status == Bq28z620StatusOk) {
        *remaining_capacity = remaining_capacity_reg.remaining_capacity;
        BQ28Z620_DEBUG(TAG, "Raw RemainingCapacity reg: %u, Remaining capacity: %u mAh", remaining_capacity_reg.remaining_capacity, *remaining_capacity);
    } else {
        FURI_LOG_E(TAG, "Failed to get remaining capacity");
    }

    return status;
}

Bq28z620Status bq28z620_get_full_charge_capacity(Bq28z620* instance, uint16_t* full_charge_capacity) {
    furi_check(instance);
    furi_check(full_charge_capacity);

    Bq28z620StdCmdFullChargeCapacityRegBits full_charge_capacity_reg = {0};
    Bq28z620Status status = bq28z620_std_cmd(instance, Bq28z620StdCmdFullChargeCapacity, (uint8_t*)&full_charge_capacity_reg, sizeof(full_charge_capacity_reg));
    if(status == Bq28z620StatusOk) {
        *full_charge_capacity = full_charge_capacity_reg.full_charge_capacity;
        BQ28Z620_DEBUG(
            TAG, "Raw FullChargeCapacity reg: %u, Full charge capacity: %u mAh", full_charge_capacity_reg.full_charge_capacity, *full_charge_capacity);
    } else {
        FURI_LOG_E(TAG, "Failed to get full charge capacity");
    }

    return status;
}

Bq28z620Status bq28z620_get_average_current(Bq28z620* instance, int16_t* average_current) {
    furi_check(instance);
    furi_check(average_current);

    Bq28z620StdCmdAverageCurrentRegBits average_current_reg = {0};
    Bq28z620Status status = bq28z620_std_cmd(instance, Bq28z620StdCmdAverageCurrent, (uint8_t*)&average_current_reg, sizeof(average_current_reg));
    if(status == Bq28z620StatusOk) {
        *average_current = average_current_reg.average_current;
        BQ28Z620_DEBUG(TAG, "Raw AverageCurrent reg: %d, Average current: %d mA", average_current_reg.average_current, *average_current);
    } else {
        FURI_LOG_E(TAG, "Failed to get average current");
    }

    return status;
}

Bq28z620Status bq28z620_get_average_time_to_empty(Bq28z620* instance, uint16_t* average_time_to_empty) {
    furi_check(instance);
    furi_check(average_time_to_empty);

    Bq28z620StdCmdAverageTimeToEmptyRegBits average_time_to_empty_reg = {0};
    Bq28z620Status status =
        bq28z620_std_cmd(instance, Bq28z620StdCmdAverageTimeToEmpty, (uint8_t*)&average_time_to_empty_reg, sizeof(average_time_to_empty_reg));
    if(status == Bq28z620StatusOk) {
        *average_time_to_empty = average_time_to_empty_reg.average_time_to_empty;
        BQ28Z620_DEBUG(
            TAG, "Raw AverageTimeToEmpty reg: %u, Average time to empty: %u minutes", average_time_to_empty_reg.average_time_to_empty, *average_time_to_empty);
    } else {
        FURI_LOG_E(TAG, "Failed to get average time to empty");
    }

    return status;
}

Bq28z620Status bq28z620_get_average_time_to_full(Bq28z620* instance, uint16_t* average_time_to_full) {
    furi_check(instance);
    furi_check(average_time_to_full);

    Bq28z620StdCmdAverageTimeToFullRegBits average_time_to_full_reg = {0};
    Bq28z620Status status = bq28z620_std_cmd(instance, Bq28z620StdCmdAverageTimeToFull, (uint8_t*)&average_time_to_full_reg, sizeof(average_time_to_full_reg));
    if(status == Bq28z620StatusOk) {
        *average_time_to_full = average_time_to_full_reg.average_time_to_full;
        BQ28Z620_DEBUG(
            TAG, "Raw AverageTimeToFull reg: %u, Average time to full: %u minutes", average_time_to_full_reg.average_time_to_full, *average_time_to_full);
    } else {
        FURI_LOG_E(TAG, "Failed to get average time to full");
    }

    return status;
}

Bq28z620Status bq28z620_get_standby_current(Bq28z620* instance, int16_t* standby_current) {
    furi_check(instance);
    furi_check(standby_current);

    Bq28z620StdCmdStandbyCurrentRegBits standby_current_reg = {0};
    Bq28z620Status status = bq28z620_std_cmd(instance, Bq28z620StdCmdStandbyCurrent, (uint8_t*)&standby_current_reg, sizeof(standby_current_reg));
    if(status == Bq28z620StatusOk) {
        *standby_current = standby_current_reg.standby_current;
        BQ28Z620_DEBUG(TAG, "Raw StandbyCurrent reg: %d, Standby current: %d mA", standby_current_reg.standby_current, *standby_current);
    } else {
        FURI_LOG_E(TAG, "Failed to get standby current");
    }

    return status;
}

Bq28z620Status bq28z620_get_standby_time_to_empty(Bq28z620* instance, uint16_t* standby_time_to_empty) {
    furi_check(instance);
    furi_check(standby_time_to_empty);

    Bq28z620StdCmdStandbyTimeToEmptyRegBits standby_time_to_empty_reg = {0};
    Bq28z620Status status =
        bq28z620_std_cmd(instance, Bq28z620StdCmdStandbyTimeToEmpty, (uint8_t*)&standby_time_to_empty_reg, sizeof(standby_time_to_empty_reg));
    if(status == Bq28z620StatusOk) {
        *standby_time_to_empty = standby_time_to_empty_reg.standby_time_to_empty;
        BQ28Z620_DEBUG(
            TAG, "Raw StandbyTimeToEmpty reg: %u, Standby time to empty: %u minutes", standby_time_to_empty_reg.standby_time_to_empty, *standby_time_to_empty);
    } else {
        FURI_LOG_E(TAG, "Failed to get standby time to empty");
    }

    return status;
}

Bq28z620Status bq28z620_get_max_load_current(Bq28z620* instance, int16_t* max_load_current) {
    furi_check(instance);
    furi_check(max_load_current);

    Bq28z620StdCmdMaxLoadCurrentRegBits max_load_current_reg = {0};
    Bq28z620Status status = bq28z620_std_cmd(instance, Bq28z620StdCmdMaxLoadCurrent, (uint8_t*)&max_load_current_reg, sizeof(max_load_current_reg));
    if(status == Bq28z620StatusOk) {
        *max_load_current = max_load_current_reg.max_load_current;
        BQ28Z620_DEBUG(TAG, "Raw MaxLoadCurrent reg: %d, Max load current: %d mA", max_load_current_reg.max_load_current, *max_load_current);
    } else {
        FURI_LOG_E(TAG, "Failed to get max load current");
    }

    return status;
}

Bq28z620Status bq28z620_get_max_load_time_to_empty(Bq28z620* instance, uint16_t* max_load_time_to_empty) {
    furi_check(instance);
    furi_check(max_load_time_to_empty);

    Bq28z620StdCmdMaxLoadTimeToEmptyRegBits max_load_time_to_empty_reg = {0};
    Bq28z620Status status =
        bq28z620_std_cmd(instance, Bq28z620StdCmdMaxLoadTimeToEmpty, (uint8_t*)&max_load_time_to_empty_reg, sizeof(max_load_time_to_empty_reg));
    if(status == Bq28z620StatusOk) {
        *max_load_time_to_empty = max_load_time_to_empty_reg.max_load_time_to_empty;
        BQ28Z620_DEBUG(
            TAG,
            "Raw MaxLoadTimeToEmpty reg: %u, Max load time to empty: %u minutes",
            max_load_time_to_empty_reg.max_load_time_to_empty,
            *max_load_time_to_empty);
    } else {
        FURI_LOG_E(TAG, "Failed to get max load time to empty");
    }

    return status;
}

Bq28z620Status bq28z620_get_average_power(Bq28z620* instance, int16_t* average_power) {
    furi_check(instance);
    furi_check(average_power);

    Bq28z620StdCmdAveragePowerRegBits average_power_reg = {0};
    Bq28z620Status status = bq28z620_std_cmd(instance, Bq28z620StdCmdAveragePower, (uint8_t*)&average_power_reg, sizeof(average_power_reg));
    if(status == Bq28z620StatusOk) {
        *average_power = average_power_reg.average_power;
        BQ28Z620_DEBUG(TAG, "Raw AveragePower reg: %d, Average power: %d mW", average_power_reg.average_power, *average_power);
    } else {
        FURI_LOG_E(TAG, "Failed to get average power");
    }

    return status;
}

Bq28z620Status bq28z620_get_internal_temperature(Bq28z620* instance, float* internal_temperature) {
    furi_check(instance);
    furi_check(internal_temperature);

    Bq28z620StdCmdInternalTemperatureRegBits internal_temperature_reg = {0};
    Bq28z620Status status =
        bq28z620_std_cmd(instance, Bq28z620StdCmdInternalTemperature, (uint8_t*)&internal_temperature_reg, sizeof(internal_temperature_reg));
    if(status == Bq28z620StatusOk) {
        *internal_temperature = (float)internal_temperature_reg.internal_temperature * 0.1f - 273.15f;
        BQ28Z620_DEBUG(
            TAG,
            "Raw internal temperature reg: %u, Internal temperature: %.2f C",
            (uint16_t)internal_temperature_reg.internal_temperature,
            *internal_temperature);
    } else {
        FURI_LOG_E(TAG, "Failed to get internal temperature");
    }

    return status;
}

Bq28z620Status bq28z620_get_cycle_count(Bq28z620* instance, uint16_t* cycle_count) {
    furi_check(instance);
    furi_check(cycle_count);

    Bq28z620StdCmdCycleCountRegBits cycle_count_reg = {0};
    Bq28z620Status status = bq28z620_std_cmd(instance, Bq28z620StdCmdCycleCount, (uint8_t*)&cycle_count_reg, sizeof(cycle_count_reg));
    if(status == Bq28z620StatusOk) {
        *cycle_count = cycle_count_reg.cycle_count;
        BQ28Z620_DEBUG(TAG, "Raw CycleCount reg: %u, Cycle count: %u", cycle_count_reg.cycle_count, *cycle_count);
    } else {
        FURI_LOG_E(TAG, "Failed to get cycle count");
    }

    return status;
}

Bq28z620Status bq28z620_get_relative_state_of_charge(Bq28z620* instance, uint8_t* relative_state_of_charge) {
    furi_check(instance);
    furi_check(relative_state_of_charge);

    Bq28z620StdCmdRelativeStateOfChargeRegBits relative_state_of_charge_reg = {0};
    Bq28z620Status status =
        bq28z620_std_cmd(instance, Bq28z620StdCmdRelativeStateOfCharge, (uint8_t*)&relative_state_of_charge_reg, sizeof(relative_state_of_charge_reg));
    if(status == Bq28z620StatusOk) {
        *relative_state_of_charge = relative_state_of_charge_reg.relative_state_of_charge;
        BQ28Z620_DEBUG(
            TAG,
            "Raw RelativeStateOfCharge reg: %u, Relative state of charge: %u%%",
            relative_state_of_charge_reg.relative_state_of_charge,
            *relative_state_of_charge);
    } else {
        FURI_LOG_E(TAG, "Failed to get relative state of charge");
    }

    return status;
}

Bq28z620Status bq28z620_get_state_of_health(Bq28z620* instance, uint8_t* state_of_health) {
    furi_check(instance);
    furi_check(state_of_health);

    Bq28z620StdCmdStateOfHealthRegBits state_of_health_reg = {0};
    Bq28z620Status status =
        bq28z620_std_cmd(instance, Bq28z620StdCmdStateOfHealth, (uint8_t*)&state_of_health_reg, sizeof(state_of_health_reg));
    if(status == Bq28z620StatusOk) {
        *state_of_health = state_of_health_reg.state_of_health;
        BQ28Z620_DEBUG(
            TAG,
            "Raw StateOfHealth reg: %u, State of health: %u%%",
            state_of_health_reg.state_of_health,
            *state_of_health);
    } else {
        FURI_LOG_E(TAG, "Failed to get state of health");
    }

    return status;
}

Bq28z620Status bq28z620_get_charging_voltage(Bq28z620* instance, float* charging_voltage) {
    furi_check(instance);
    furi_check(charging_voltage);

    Bq28z620StdCmdChargingVoltageRegBits charging_voltage_reg = {0};
    Bq28z620Status status = bq28z620_std_cmd(instance, Bq28z620StdCmdChargeVoltage, (uint8_t*)&charging_voltage_reg, sizeof(charging_voltage_reg));
    if(status == Bq28z620StatusOk) {
        *charging_voltage = (float)charging_voltage_reg.charging_voltage * 0.001f;
        BQ28Z620_DEBUG(TAG, "Raw ChargingVoltage reg: %u, Charging voltage: %.3f V", charging_voltage_reg.charging_voltage, *charging_voltage);
    } else {
        FURI_LOG_E(TAG, "Failed to get charging voltage");
    }

    return status;
}

Bq28z620Status bq28z620_get_charging_current(Bq28z620* instance, int16_t* charging_current) {
    furi_check(instance);
    furi_check(charging_current);

    Bq28z620StdCmdChargingCurrentRegBits charging_current_reg = {0};
    Bq28z620Status status = bq28z620_std_cmd(instance, Bq28z620StdCmdChargeCurrent, (uint8_t*)&charging_current_reg, sizeof(charging_current_reg));
    if(status == Bq28z620StatusOk) {
        *charging_current = charging_current_reg.charging_current;
        BQ28Z620_DEBUG(TAG, "Raw ChargingCurrent reg: %d, Charging current: %d mA", charging_current_reg.charging_current, *charging_current);
    } else {
        FURI_LOG_E(TAG, "Failed to get charging current");
    }

    return status;
}

Bq28z620Status bq28z620_get_design_capacity(Bq28z620* instance, uint16_t* design_capacity) {
    furi_check(instance);
    furi_check(design_capacity);

    Bq28z620StdCmdDesignCapacityRegBits design_capacity_reg = {0};
    Bq28z620Status status = bq28z620_std_cmd(instance, Bq28z620StdCmdDesignCapacity, (uint8_t*)&design_capacity_reg, sizeof(design_capacity_reg));
    if(status == Bq28z620StatusOk) {
        *design_capacity = design_capacity_reg.design_capacity;
        BQ28Z620_DEBUG(TAG, "Raw DesignCapacity reg: %u, Design capacity: %u mAh", design_capacity_reg.design_capacity, *design_capacity);
    } else {
        FURI_LOG_E(TAG, "Failed to get design capacity");
    }

    return status;
}
