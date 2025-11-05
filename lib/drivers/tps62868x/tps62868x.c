#include "tps62868x.h"
#include "tps62868x_reg.h"
#include <furi_hal_i2c.h>
#include <furi_hal_resources.h>
#include <pico/error.h>

#define TPS62868X_VOLTAGE_FACTOR 2.0f
#define TPS62868X_VOLTAGE_MIN    0.8f
#define TPS62868X_VOLTAGE_MAX    3.35f
#define TPS62868_ADD             0x47

bool tps62868x_init(FuriHalI2cBusHandle* handle) {
    furi_check(handle);

    bool ret = false;
    furi_hal_gpio_init_simple(&gpio_display_vci_en, GpioModeOutputPushPull);
    furi_hal_gpio_write(&gpio_display_vci_en, true);
    furi_delay_ms(1);

    furi_hal_i2c_acquire(handle);
    ret = furi_hal_i2c_device_ready(handle, TPS62868_ADD, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(handle);
    if(ret) {
        tps62868x_set_pwm_on(handle);
    }

    return ret;
}

void tps62868x_deinit(void) {
    furi_hal_gpio_write(&gpio_display_vci_en, false);
    furi_hal_gpio_init_ex(&gpio_display_vci_en, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
}

void tps62868x_set_pwm_on(FuriHalI2cBusHandle* handle) { // Enable FPWM mode
    TPS62868XControl data_reg[1] = {0};
    tps62868x_read_reg(handle, TPS62868X_REG_CONTROL, (uint8_t*)data_reg);
    data_reg[0].EN_FPWM_MODE = 1;
    tps62868x_write_reg(handle, TPS62868X_REG_CONTROL, (uint8_t*)data_reg);
}

void tps62868x_set_pwm_off(FuriHalI2cBusHandle* handle) { //Enable power save mode
    TPS62868XControl data_reg[1] = {0};
    tps62868x_read_reg(handle, TPS62868X_REG_CONTROL, (uint8_t*)data_reg);
    data_reg[0].EN_FPWM_MODE = 0;
    tps62868x_write_reg(handle, TPS62868X_REG_CONTROL, (uint8_t*)data_reg);
}

int tps62868x_read_reg(FuriHalI2cBusHandle* handle, uint8_t reg, uint8_t* data) {
    furi_check(handle);
    furi_check(data);
    furi_hal_i2c_acquire(handle);
    int ret = furi_hal_i2c_master_trx_blocking(handle, TPS62868_ADD, &reg, 1, data, 1, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(handle);
    return ret;
}

int tps62868x_write_reg(FuriHalI2cBusHandle* handle, uint8_t reg, uint8_t* data) {
    furi_check(handle);
    furi_check(data);
    uint8_t buffer[2] = {reg, data[0]};
    furi_hal_i2c_acquire(handle);
    int ret = furi_hal_i2c_master_tx_blocking(handle, TPS62868_ADD, buffer, 2, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(handle);
    return ret;
}

int tps62868x_set_voltage(FuriHalI2cBusHandle* handle, float volt) {
    if((volt < TPS62868X_VOLTAGE_MIN) || (volt > TPS62868X_VOLTAGE_MAX)) return 0;

    //Vout = TPS62868X_VOLTAGE_FACTOR * (0.4v + (VOx_SET*0.005v))
    uint8_t volt_data_reg[1] = {((volt / TPS62868X_VOLTAGE_FACTOR) - 0.4f) / 0.005f};
    return tps62868x_write_reg(handle, TPS62868X_REG_1, volt_data_reg);
}

float tps62868x_get_voltage(FuriHalI2cBusHandle* handle) {
    uint8_t volt_data_reg[1] = {0};
    int ret = tps62868x_read_reg(handle, TPS62868X_REG_1, volt_data_reg);
    if(ret != PICO_OK) {
        return -1; // Error reading voltage
    }
    return (float)(TPS62868X_VOLTAGE_FACTOR * (0.4f + ((float)volt_data_reg[0] * 0.005f)));
}
