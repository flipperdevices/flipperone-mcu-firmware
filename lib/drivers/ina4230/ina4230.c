#include "ina4230_reg.h"
#include "ina4230.h"
#include <furi.h>

#include <furi_hal_i2c.h>

#define TAG "Ina4230"

#ifdef INA4230_DEBUG_ENABLE
#define INA4230_DEBUG(...) FURI_LOG_D(__VA_ARGS__)
#else
#define INA4230_DEBUG(...)
#endif

typedef struct {
    Ina4230RegChannelCalibrationRegBits calibration;
    Ina4230RegChannelAlertConfigBits alert_config;
    Ina4230RegChannelAlertLimitBits alert_limit;
    FuriString* name;
    float current_lsb;
    float power_lsb;
    float energy_lsb;
    Ina4230AdcRange v_shunt_range;
} Ina4230ChannelConfig;

struct Ina4230 {
    const FuriHalI2cBusHandle* i2c_handle;
    uint8_t address;
    Ina4230Mode mode;
    Ina4230Config1RegBits config1;
    Ina4230Config2RegBits config2;
    Ina4230ChannelConfig channel_config[INA4230_CHANNEL_COUNT];
};

static FURI_ALWAYS_INLINE int ina4230_write_reg(Ina4230* instance, Ina4230Reg reg, uint16_t data) {
    furi_check(instance);

    uint8_t buffer[3] = {reg, data >> 8, data & 0xFF};

    furi_hal_i2c_acquire(instance->i2c_handle, INA4230_I2C_SPEED_HZ);
    int ret = furi_hal_i2c_master_tx_blocking(instance->i2c_handle, instance->address, buffer, sizeof(buffer), FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
        FURI_LOG_E(TAG, "Failed to write reg 0x%02X", reg);
    } else {
        INA4230_DEBUG(TAG, "Wrote reg 0x%02X: %016b", reg, data);
    }

    return ret;
}

static FURI_ALWAYS_INLINE int ina4230_read_reg(Ina4230* instance, Ina4230Reg reg, uint16_t* data) {
    furi_check(instance);
    furi_check(data);

    furi_hal_i2c_acquire(instance->i2c_handle, INA4230_I2C_SPEED_HZ);
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

void ina4230_set_config_channel(Ina4230* instance, uint32_t channel, const char* name, float shunt_resistance_om, float max_expected_current_a) {
    furi_check(instance);

    // set range shunt
    uint8_t divider = 1;
    float v_shunt = max_expected_current_a * shunt_resistance_om;
    INA4230_DEBUG(TAG, "v_shunt : %.8f V", v_shunt);
    furi_check(v_shunt <= 0.08192f);
    if(v_shunt > 0.02048f) {
        instance->config2.range &= ~(1 << channel);
        instance->channel_config[channel].v_shunt_range = Ina4230Gain81_92mV;
    } else {
        instance->config2.range |= (1 << channel);
        instance->channel_config[channel].v_shunt_range = Ina4230Gain20_48mV;
        divider = 4;
    }

    // Calibration register value calculation based on INA4230 datasheet
    // Calibration = 0.00512 / (Current_LSB * Rshunt)
    // Where Current_LSB = MaxExpectedCurrent / 2^15
    // Where Power_LSB = 25 * Current_LSB (for INA4230, Power_LSB is 32 times Current_LSB)
    // Where Energy_LSB = 25 * Current_LSB (for INA4230, Energy_LSB is 32 times Power_LSB)

    uint16_t calibration_value;
    float minimum_lbs = max_expected_current_a / (1 << 15);

    instance->channel_config[channel].current_lsb = (uint16_t)(minimum_lbs * 100000000);
    instance->channel_config[channel].current_lsb /= 100000000;
    instance->channel_config[channel].current_lsb /= 0.000001;
    instance->channel_config[channel].current_lsb = ceilf(instance->channel_config[channel].current_lsb);
    instance->channel_config[channel].current_lsb *= 0.000001;
    instance->channel_config[channel].power_lsb = instance->channel_config[channel].current_lsb * 32;
    instance->channel_config[channel].energy_lsb = instance->channel_config[channel].current_lsb * 32;

    calibration_value = (uint16_t)((0.00512f) / (instance->channel_config[channel].current_lsb * shunt_resistance_om));
    calibration_value /= divider;

    INA4230_DEBUG(
        TAG,
        "current_lsb value: %.8f, calibration: %u, power multiplier: %.6f W, energy multiplier: %.6f Wh",
        instance->channel_config[channel].current_lsb,
        calibration_value,
        instance->channel_config[channel].power_lsb,
        instance->channel_config[channel].energy_lsb);

    furi_check(calibration_value <= 0x7FFF, "Calculated calibration value exceeds maximum allowed value for channel");
    instance->channel_config[channel].calibration.shunt_cal = calibration_value;

    furi_string_set(instance->channel_config[channel].name, name);

    ina4230_write_reg(instance, Ina4230RegConfig2, *(uint16_t*)&instance->config2);
    ina4230_write_reg(
        instance, Ina4230RegChannelCalibration + (channel * INA4230_CHANNEL_REG_SHIFT), *(uint16_t*)&instance->channel_config[channel].calibration);
}

void ina4230_set_config(Ina4230* instance, Ina4230AdcConvTime shunt_conv_time, Ina4230AdcConvTime bus_conv_time, Ina4230AvgSamples avg, Ina4230Mode mode) {
    furi_check(instance);

    instance->mode = mode;

    Ina4230Config1RegBits config1 = {0};
    ina4230_read_reg(instance, Ina4230RegConfig1, (uint16_t*)&config1);
    config1.mode = mode;
    config1.vshct = shunt_conv_time;
    config1.vbusct = bus_conv_time;
    config1.avg = avg;
    config1.active_channel = Ina4230ActiveChannel1 | Ina4230ActiveChannel2 | Ina4230ActiveChannel3 | Ina4230ActiveChannel4; // Enable all channels
    ina4230_write_reg(instance, Ina4230RegConfig1, *(uint16_t*)&config1);
}

Ina4230* ina4230_init(const FuriHalI2cBusHandle* i2c_handle, uint8_t address) {
    Ina4230* instance = (Ina4230*)malloc(sizeof(Ina4230));
    instance->i2c_handle = i2c_handle;
    instance->address = address;

    for(uint8_t i = 0; i < INA4230_CHANNEL_COUNT; i++) {
        instance->channel_config[i].name = furi_string_alloc();
    }

    furi_hal_i2c_acquire(instance->i2c_handle, INA4230_I2C_SPEED_HZ);
    int ret = furi_hal_i2c_device_ready(instance->i2c_handle, instance->address, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret) {
        FURI_LOG_I(TAG, "INA4230 device ready at address 0x%02X", instance->address);
        ina4230_set_config(instance, Ina4230AdcConvTime1100us, Ina4230AdcConvTime1100us, Ina4230AvgSamples64, Ina4230ModeShuntBusCont);
        ina4230_read_reg(instance, Ina4230RegConfig2, (uint16_t*)&instance->config2);
    } else {
        FURI_LOG_E(TAG, "INA4230 device not ready at address 0x%02X", instance->address);
        free(instance);
        return NULL;
    }

    return instance;
}

void ina4230_deinit(Ina4230* instance) {
    furi_check(instance);
    for(uint8_t i = 0; i < INA4230_CHANNEL_COUNT; i++) {
        furi_string_free(instance->channel_config[i].name);
    }
    free(instance);
}

void ina4230_set_power_down(Ina4230* instance, bool power_down) {
    furi_check(instance);
    Ina4230Config1RegBits config = {0};
    ina4230_read_reg(instance, Ina4230RegConfig1, (uint16_t*)&config);
    if(power_down) {
        config.mode = Ina4230ModeShutdown;
    } else {
        config.mode = instance->mode;
    }
    ina4230_write_reg(instance, Ina4230RegConfig1, *(uint16_t*)&config);
}

float ina4230_get_power_w(Ina4230* instance, uint32_t channel) {
    furi_check(instance);
    uint16_t raw_power = 0;
    ina4230_read_reg(instance, Ina4230RegChannelPower + (channel * INA4230_CHANNEL_REG_SHIFT), &raw_power);
    return raw_power * instance->channel_config[channel].power_lsb;
}

float ina4230_get_energy_(Ina4230* instance, uint32_t channel) {
    furi_check(instance);
    uint16_t raw_power = 0;
    ina4230_read_reg(instance, Ina4230RegChannelPower + (channel * INA4230_CHANNEL_REG_SHIFT), &raw_power);
    return raw_power * instance->channel_config[channel].power_lsb;
}

float ina4230_get_bus_voltage_v(Ina4230* instance, uint32_t channel) {
    furi_check(instance);
    Ina4230RegChannelBusVoltageRegBits raw_bus_voltage = {0};
    ina4230_read_reg(instance, Ina4230RegChannelBusVoltage + (channel * INA4230_CHANNEL_REG_SHIFT), (uint16_t*)&raw_bus_voltage);
    return raw_bus_voltage.bus_voltage * 0.0016f; // LSB = 1.6mV
}

float ina4230_get_shunt_voltage_mv(Ina4230* instance, uint32_t channel) {
    furi_check(instance);
    int16_t raw_shunt_voltage = 0;
    ina4230_read_reg(instance, Ina4230RegChannelShuntVoltage + (channel * INA4230_CHANNEL_REG_SHIFT), (uint16_t*)&raw_shunt_voltage);

    if(instance->channel_config[channel].v_shunt_range == Ina4230Gain20_48mV) {
        return raw_shunt_voltage * 0.000625f; // LSB = 625nV/LSB
    }
    return raw_shunt_voltage * 0.0025f; // LSB = 2.5µV/LSB
}

float ina4230_get_current_a(Ina4230* instance, uint32_t channel) {
    furi_check(instance);
    int16_t raw_current = 0;
    ina4230_read_reg(instance, Ina4230RegChannelCurrent + (channel * INA4230_CHANNEL_REG_SHIFT), (uint16_t*)&raw_current);
    return (float)raw_current * instance->channel_config[channel].current_lsb;
}

const char* ina4230_get_channel_name(Ina4230* instance, uint32_t channel) {
    furi_check(instance);
    return furi_string_get_cstr(instance->channel_config[channel].name);
}
