#include "ina219_reg.h"
#include "ina219.h"
#include <furi.h>

#include <furi_hal_i2c.h>
#include <math.h>

#define TAG "Ina219"

#ifdef INA219_DEBUG_ENABLE
#define INA219_DEBUG(...) FURI_LOG_D(__VA_ARGS__)
#else
#define INA219_DEBUG(...)
#endif

#define INA219_TRIGGER_MARGIN_US 200

struct Ina219 {
    const FuriHalI2cBusHandle* i2c_handle;
    uint8_t address;
    float current_lsb;
    float power_lsb;
    Ina219Gain v_shunt_max;
    Ina219Mode mode;
    Ina219ConfigRegBits config;
    uint32_t bus_conv_time_us;
    uint32_t shunt_conv_time_us;
};

static FURI_ALWAYS_INLINE int ina219_write_reg(Ina219* instance, Ina219Reg reg, uint16_t data) {
    furi_check(instance);

    uint8_t buffer[3] = {reg, data >> 8, data & 0xFF};

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_master_tx_blocking(instance->i2c_handle, instance->address, buffer, sizeof(buffer), FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
        FURI_LOG_E(TAG, "Failed to write reg 0x%02X", reg);
    } else {
        INA219_DEBUG(TAG, "Wrote reg 0x%02X: %016b", reg, data);
    }

    return ret;
}

static FURI_ALWAYS_INLINE int ina219_read_reg(Ina219* instance, Ina219Reg reg, uint16_t* data) {
    furi_check(instance);
    furi_check(data);

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_master_tx_blocking(instance->i2c_handle, instance->address, (uint8_t*)&reg, 1, FURI_HAL_I2C_TIMEOUT_US);
    if(!(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT)) {
        uint8_t buffer[2] = {0};
        ret = furi_hal_i2c_master_rx_blocking(instance->i2c_handle, instance->address, buffer, sizeof(buffer), FURI_HAL_I2C_TIMEOUT_US);
        if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
            FURI_LOG_E(TAG, "Failed to read reg 0x%02X", reg);
        } else {
            *data = buffer[0] << 8 | (buffer[1]);
        }
    } else {
        FURI_LOG_E(TAG, "Failed to write reg address 0x%02X for reading", reg);
    }
    furi_hal_i2c_release(instance->i2c_handle);

    return ret;
}

/* Datasheet Table 4: conversion time for each ADC setting. */
static uint32_t ina219_adc_shunt_time_us(Ina219ShuntRes shunt_mode) {
    switch(shunt_mode) {
    case Ina219ShuntRes9bit1S84ms:
        return 84;
    case Ina219ShuntRes10bit1S148ms:
        return 148;
    case Ina219ShuntRes11bit1S276ms:
        return 276;
    case Ina219ShuntRes12bit1S532ms:
        return 532;
    case Ina219ShuntRes12bit2S1060ms:
        return 1060;
    case Ina219ShuntRes12bit4S2130ms:
        return 2130;
    case Ina219ShuntRes12bit8S4260ms:
        return 4260;
    case Ina219ShuntRes12bit16S8510ms:
        return 8510;
    case Ina219ShuntRes12bit32S17020ms:
        return 17020;
    case Ina219ShuntRes12bit64S34050ms:
        return 34050;
    case Ina219ShuntRes12bit128S68100ms:
        return 68100;
    default: {
        furi_crash("Invalid SHUNT MODE");
        return 68100; // Unreachable, but avoids compiler warning
    }
    }
}

/* Datasheet Table 4: conversion time for each ADC setting. */
static uint32_t ina219_adc_bus_time_us(Ina219BusRes bus_mode) {
    switch(bus_mode) {
    case Ina219BusRes9bit:
        return 84;
    case Ina219BusRes10bit:
        return 148;
    case Ina219BusRes11bit:
        return 276;
    case Ina219BusRes12bit:
        return 532;
    default: {
        furi_crash("Invalid BUS MODE");
        return 532; // Unreachable, but avoids compiler warning
    }
    }
}

/* Start a single triggered conversion. The INA219 Mode bits self-reset to
 * Power-Down once the conversion completes, so no explicit write back is
 * needed; the fixed wait guarantees the data is ready before it is read. */
static void ina219_trigger_conversion(Ina219* instance, Ina219Mode trigger_mode, uint32_t wait_us) {
    Ina219ConfigRegBits config = instance->config;
    config.mode = trigger_mode;
    ina219_write_reg(instance, Ina219RegConfig, *(uint16_t*)&config);
    furi_delay_us(wait_us);
}

static void ina219_calculate_gain(Ina219* instance, float shunt_resistance_om, float max_expected_current_a) {
    // Calculate the maximum shunt voltage based on expected current and shunt resistance
    float v_shunt = max_expected_current_a * shunt_resistance_om;
    INA219_DEBUG(TAG, "Calculated shunt voltage: %.6f V", v_shunt);

    if(v_shunt < 0.04f) {
        INA219_DEBUG(TAG, "Using Gain /1 (40mV)");
        instance->v_shunt_max = Ina219Gain40mV;
    } else if(v_shunt < 0.08f) {
        INA219_DEBUG(TAG, "Using Gain /2 (80mV)");
        instance->v_shunt_max = Ina219Gain80mV;
    } else if(v_shunt < 0.16f) {
        INA219_DEBUG(TAG, "Using Gain /4 (160mV)");
        instance->v_shunt_max = Ina219Gain160mV;
    } else if(v_shunt <= 0.32f) {
        INA219_DEBUG(TAG, "Using Gain /8 (320mV)");
        instance->v_shunt_max = Ina219Gain320mV;
    } else {
        FURI_LOG_E(TAG, "Calculated shunt voltage %.6f V exceeds maximum for INA219!", v_shunt);
        furi_crash();
    }
}

static uint16_t ina219_calculate_calibration(Ina219* instance, float shunt_resistance_om, float max_expected_current_a) {
    // Calibration register value calculation based on INA219 datasheet
    // Calibration = 0.04096 / (Current_LSB * Rshunt)
    // Where Current_LSB = MaxExpectedCurrent / 2^15

    uint16_t calibration_value;
    float minimum_lbs = max_expected_current_a / (1 << 15);

    instance->current_lsb = (uint16_t)(minimum_lbs * 100000000);
    instance->current_lsb /= 100000000;
    instance->current_lsb /= 0.000001;
    instance->current_lsb = ceilf(instance->current_lsb);
    instance->current_lsb *= 0.000001;

    instance->power_lsb = instance->current_lsb * 20;

    calibration_value = (uint16_t)((0.04096) / (instance->current_lsb * shunt_resistance_om));
    INA219_DEBUG(TAG, "current_lsb value: %.8f, calibration: %u power multiplier: %.6f W", instance->current_lsb, calibration_value, instance->power_lsb);

    return (uint16_t)calibration_value;
}

void ina219_set_config(Ina219* instance, Ina219Range range, Ina219BusRes bus_res, Ina219ShuntRes shunt_res, Ina219Mode mode) {
    furi_check(instance);
    Ina219ConfigRegBits config = {0};

    config.brng = range;
    config.pg = instance->v_shunt_max;
    config.badc = bus_res;
    config.sadc = shunt_res;
    config.mode = mode;
    instance->mode = mode;
    instance->config = config;
    instance->bus_conv_time_us = ina219_adc_bus_time_us(bus_res);
    instance->shunt_conv_time_us = ina219_adc_shunt_time_us(shunt_res);
    ina219_write_reg(instance, Ina219RegConfig, *(uint16_t*)&config);
}

Ina219* ina219_init(const FuriHalI2cBusHandle* i2c_handle, uint8_t address, float shunt_resistance_om, float max_expected_current_a) {
    Ina219* instance = (Ina219*)malloc(sizeof(Ina219));
    instance->i2c_handle = i2c_handle;
    instance->address = address;

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_device_ready(instance->i2c_handle, instance->address, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret) {
        FURI_LOG_I(TAG, "INA219 device ready at address 0x%02X", instance->address);
        ina219_calculate_gain(instance, shunt_resistance_om, max_expected_current_a);

        uint16_t calibration_value = ina219_calculate_calibration(instance, shunt_resistance_om, max_expected_current_a);
        ina219_write_reg(instance, Ina219RegCalibration, calibration_value);
        INA219_DEBUG(TAG, "Calibration value set to: 0x%04X", calibration_value);

        // Configure the INA219 with default settings. The chip rests in
        // Power-Down between reads: each measurement is triggered on demand
        // and the Mode bits self-reset after the conversion completes.
        Ina219ConfigRegBits config = {0};
        config.brng = Ina219Range16V;
        config.pg = instance->v_shunt_max;
        config.badc = Ina219BusRes12bit;
        config.sadc = Ina219ShuntRes12bit4S2130ms;
        config.mode = Ina219ModePowerDown;
        instance->config = config;
        instance->mode = Ina219ModePowerDown;
        instance->bus_conv_time_us = ina219_adc_bus_time_us(config.badc);
        instance->shunt_conv_time_us = ina219_adc_shunt_time_us(config.sadc);
        ina219_write_reg(instance, Ina219RegConfig, *(uint16_t*)&config);

    } else {
        FURI_LOG_E(TAG, "INA219 device not ready at address 0x%02X", instance->address);
        free(instance);
        return NULL;
    }

    return instance;
}

void ina219_deinit(Ina219* instance) {
    furi_check(instance);
    free(instance);
}

void ina219_set_power_down(Ina219* instance, bool power_down) {
    furi_check(instance);
    Ina219ConfigRegBits config = {0};
    ina219_read_reg(instance, Ina219RegConfig, (uint16_t*)&config);
    if(power_down) {
        config.mode = Ina219ModePowerDown;
    } else {
        config.mode = instance->mode;
    }
    ina219_write_reg(instance, Ina219RegConfig, *(uint16_t*)&config);
}

float ina219_get_power_w(Ina219* instance) {
    furi_check(instance);
    // Power requires both conversions: trigger a single shunt+bus cycle.
    ina219_trigger_conversion(instance, Ina219ModeShuntBusTrig, instance->bus_conv_time_us + instance->shunt_conv_time_us + INA219_TRIGGER_MARGIN_US);
    uint16_t raw_power = 0;
    ina219_read_reg(instance, Ina219RegPower, &raw_power);
    return raw_power * instance->power_lsb;
}

float ina219_get_bus_voltage_v(Ina219* instance) {
    furi_check(instance);
    ina219_trigger_conversion(instance, Ina219ModeBusTrig, instance->bus_conv_time_us + INA219_TRIGGER_MARGIN_US);
    Ina219BusVoltageRegBits raw_bus_voltage = {0};
    ina219_read_reg(instance, Ina219RegBusVoltage, (uint16_t*)&raw_bus_voltage);
    if(raw_bus_voltage.ovf) {
        FURI_LOG_W(TAG, "Bus voltage overflow detected!");
    }
    return raw_bus_voltage.bus_voltage * 0.004f; // LSB = 4mV
}

float ina219_get_shunt_voltage_mv(Ina219* instance) {
    furi_check(instance);
    ina219_trigger_conversion(instance, Ina219ModeShuntTrig, instance->shunt_conv_time_us + INA219_TRIGGER_MARGIN_US);
    int16_t raw_shunt_voltage = 0;
    ina219_read_reg(instance, Ina219RegShuntVoltage, (uint16_t*)&raw_shunt_voltage);
    return raw_shunt_voltage * 0.01f; // LSB = 10uV
}

float ina219_get_current_a(Ina219* instance) {
    furi_check(instance);
    ina219_trigger_conversion(instance, Ina219ModeShuntTrig, instance->shunt_conv_time_us + INA219_TRIGGER_MARGIN_US);
    int16_t raw_current = 0;
    ina219_read_reg(instance, Ina219RegCurrent, (uint16_t*)&raw_current);
    return (float)raw_current * instance->current_lsb;
}
