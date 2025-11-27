#include "core/log.h"
#include "furi_hal_gpio.h"
#include "iqs7211e_reg.h"
#include "iqs7211e.h"
#include <furi.h>

#include <furi_hal_i2c.h>

#define TAG "Iqs7211e"

struct Iqs7211e {
    const FuriHalI2cBusHandle* i2c_handle;
    const GpioPin* pin_rdy;
    uint8_t address;
    Iqs7211eCallbackInput input_callback;
    void* callback_context;
};

static __isr __not_in_flash_func(void) iqs7211e_interrupt_handler(void* ctx) {
    Iqs7211e* instance = (Iqs7211e*)ctx;
    if(instance->input_callback) {
        instance->input_callback(instance->callback_context);
    }
}

Iqs7211e* iqs7211e_init(const FuriHalI2cBusHandle* i2c_handle, const GpioPin* pin_rdy, uint8_t address) {
    Iqs7211e* instance = (Iqs7211e*)malloc(sizeof(Iqs7211e));
    instance->i2c_handle = i2c_handle;
    instance->pin_rdy = pin_rdy;
    instance->address = address;
    furi_hal_gpio_init_simple(instance->pin_rdy, GpioModeInput);


    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_device_ready(instance->i2c_handle, instance->address, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret) {
        FURI_LOG_I(TAG, "IQS7211E device ready at address 0x%02X", instance->address);
    } else {
        FURI_LOG_E(TAG, "IQS7211E device not ready at address 0x%02X", instance->address);
        furi_hal_gpio_init_ex(instance->pin_rdy, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
        free(instance);
        return NULL;
    }

    return instance;
}

void iqs7211e_deinit(Iqs7211e* instance) {
    furi_check(instance);
    //furi_hal_gpio_remove_int_callback(instance->pin_rdy);
    furi_hal_gpio_init_ex(instance->pin_rdy, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    free(instance);
}

// void iqs7211e_set_input_callback(Iqs7211e* instance, Iqs7211eCallbackInput callback, void* context) {
//     furi_check(instance);
//     instance->input_callback = callback;
//     instance->callback_context = context;
// }

static FURI_ALWAYS_INLINE int iqs7211e_write_reg(Iqs7211e* instance, Iqs7211eReg reg, uint16_t data) {
    furi_check(instance);

    uint8_t buffer[3] = {reg, data & 0xFF, data >> 8};

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_master_tx_blocking(instance->i2c_handle, instance->address, buffer, sizeof(buffer), FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret != PICO_ERROR_GENERIC) {
        FURI_LOG_D(TAG, "Wrote reg 0x%02X: %016b", reg, data);
    } else {
        FURI_LOG_E(TAG, "Failed to write reg 0x%02X", reg);
    }

    return ret;
}

static FURI_ALWAYS_INLINE int iqs7211e_read_reg(Iqs7211e* instance, Iqs7211eReg reg, uint16_t* data) {
    furi_check(instance);
    furi_check(data);

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_master_tx_blocking(instance->i2c_handle, instance->address, (uint8_t*)&reg, 1, FURI_HAL_I2C_TIMEOUT_US);
    if(ret != PICO_ERROR_GENERIC) {
        uint8_t buffer[2] = {0};
        ret = furi_hal_i2c_master_rx_blocking(instance->i2c_handle, instance->address, buffer, sizeof(buffer), FURI_HAL_I2C_TIMEOUT_US);
        if(ret != PICO_ERROR_GENERIC) {
            *data = buffer[0] | (buffer[1] << 8);
        } else {
            FURI_LOG_E(TAG, "Failed to read reg 0x%02X", reg);
        }
    } else {
        FURI_LOG_E(TAG, "Failed to write reg address 0x%02X for reading", reg);
    }
    furi_hal_i2c_release(instance->i2c_handle);

    return ret;
}
