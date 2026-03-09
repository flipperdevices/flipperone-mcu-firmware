#include "tps62868x.h"
#include "tps62868x_reg.h"
#include <furi_hal_i2c.h>
#include <furi_hal_resources.h>
#include <pico/error.h>
#include <furi_hal_i2c_config.h>

#define TAG "TPS62868x"

#define TPS62868X_VOLTAGE_FACTOR 2.0f
#define TPS62868X_VOLTAGE_MIN    0.8f
#define TPS62868X_VOLTAGE_MAX    3.35f

#ifdef TPS62868X_DEBUG_ENABLE
#define TPS62868X_DEBUG(...) FURI_LOG_D(__VA_ARGS__)
#else
#define TPS62868X_DEBUG(...)
#endif

struct Tps62868x {
    const FuriHalI2cBusHandle* i2c_handle;
    uint8_t address;
};

Tps62868x* tps62868x_init(const FuriHalI2cBusHandle* handle, uint8_t address) {
    furi_check(handle);

    Tps62868x* instance = malloc(sizeof(Tps62868x));
    instance->i2c_handle = handle;
    instance->address = address;

    bool ret = false;
    furi_hal_gpio_init_simple(&gpio_display_vci_en, GpioModeOutputPushPull);
    furi_hal_gpio_write(&gpio_display_vci_en, true);
    furi_delay_ms(100);

    furi_hal_i2c_acquire(instance->i2c_handle);
    ret = furi_hal_i2c_device_ready(instance->i2c_handle, instance->address, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);
    if(ret) {
      tps62868x_set_pwm_on(instance);
    }

    return instance;
}

void tps62868x_deinit(Tps62868x* instance) {
    furi_check(instance);
    furi_hal_gpio_write(&gpio_display_vci_en, false);
    furi_hal_gpio_init_ex(&gpio_display_vci_en, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    free(instance);
}

void tps62868x_set_pwm_on(Tps62868x* instance) { // Enable FPWM mode
    TPS62868XControl data_reg[1] = {0};
    tps62868x_read_reg(instance, TPS62868X_REG_CONTROL, (uint8_t*)data_reg);
    data_reg[0].EN_FPWM_MODE = 1;
    tps62868x_write_reg(instance, TPS62868X_REG_CONTROL, (uint8_t*)data_reg);
}

void tps62868x_set_pwm_off(Tps62868x* instance) { //Enable power save mode
    TPS62868XControl data_reg[1] = {0};
    tps62868x_read_reg(instance, TPS62868X_REG_CONTROL, (uint8_t*)data_reg);
    data_reg[0].EN_FPWM_MODE = 0;
    tps62868x_write_reg(instance, TPS62868X_REG_CONTROL, (uint8_t*)data_reg);
}

int tps62868x_read_reg(Tps62868x* instance, uint8_t reg, uint8_t* data) {
    furi_check(instance);
    furi_check(data);

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_master_trx_blocking(instance->i2c_handle, instance->address, &reg, 1, data, 1, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
        FURI_LOG_E(TAG, "Failed to write reg 0x%02X", reg);
    } else {
        TPS62868X_DEBUG(TAG, "Wrote reg 0x%02X: 0x%02X %08b", reg, data[0], data[0]);
    }

    return ret;
}

int tps62868x_write_reg(Tps62868x* instance, uint8_t reg, uint8_t* data) {
    furi_check(instance);
    furi_check(data);
    uint8_t buffer[2] = {reg, data[0]};
    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_master_tx_blocking(instance->i2c_handle, instance->address, buffer, 2, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if (ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT)  {
        FURI_LOG_E(TAG,"Failed to write reg 0x%02X", reg);
    } else {
        TPS62868X_DEBUG(TAG, "Wrote reg 0x%02X: %02X", reg, data[0]);
    }

    return ret;
}

int tps62868x_set_voltage(Tps62868x* instance, float volt) {
    furi_check((volt >= TPS62868X_VOLTAGE_MIN) || (volt <= TPS62868X_VOLTAGE_MAX));
    
    //Vout = TPS62868X_VOLTAGE_FACTOR * (0.4v + (VOx_SET*0.005v))
    uint8_t volt_data_reg = (uint8_t)(((volt / TPS62868X_VOLTAGE_FACTOR) - 0.4f) / 0.005f);
    TPS62868X_DEBUG(TAG, "Setting voltage to %.2f V , 0x%02X", volt, volt_data_reg);
    return tps62868x_write_reg(instance, TPS62868X_REG_1, &volt_data_reg);
}

float tps62868x_get_voltage(Tps62868x* instance) {
    uint8_t volt_data_reg[1] = {0};
    float voltage = 0.0f;
    int ret = tps62868x_read_reg(instance, TPS62868X_REG_1, volt_data_reg);
    
    if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
        FURI_LOG_E(TAG, "Failed to get voltage");
        voltage = -1.0f;
    } else {
        voltage = (float)(TPS62868X_VOLTAGE_FACTOR * (0.4f + ((float)volt_data_reg[0] * 0.005f)));
        TPS62868X_DEBUG(TAG, "Read voltage register value: 0x%02X, %.2f V", volt_data_reg[0], voltage);
    }
    return voltage;
}
