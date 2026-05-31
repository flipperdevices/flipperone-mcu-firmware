#include "hd3ss3220.h"
#include <furi.h>
#include <furi_hal_gpio.h>
#include <furi_hal_i2c.h>

#define TAG "Hd3ss3220"

#ifdef HD3SS3220_DEBUG_ENABLE
#define HD3SS3220_DEBUG(...) FURI_LOG_D(__VA_ARGS__)
#else
#define HD3SS3220_DEBUG(...)
#endif

struct Hd3ss3220 {
    const FuriHalI2cBusHandle* i2c_handle;
    uint8_t address;
    const GpioPin* pin_interrupt;
    Hd3ss3220CallbackInput input_callback;
    void* callback_context;
};

// static Hd3ss3220Status hd3ss3220_check_status(int stataus) {
//     Hd3ss3220Status ret = Hd3ss3220StatusUnknown;
//     if(stataus >= PICO_OK) {
//         ret = Hd3ss3220StatusOk;
//     } else if(stataus == PICO_ERROR_GENERIC) {
//         ret = Hd3ss3220StatusError;
//     } else if(stataus == PICO_ERROR_TIMEOUT) {
//         ret = Hd3ss3220StatusTimeout;
//     } else {
//         ret = Hd3ss3220StatusUnknown;
//     }

//     return ret;
// }

// static Hd3ss3220Status hd3ss3220_write_reg(Hd3ss3220* instance, Hd3ss3220Reg reg, uint8_t* data) {
//     furi_check(instance);

//     uint8_t buffer[2] = {reg, *data};

//     furi_hal_i2c_acquire(instance->i2c_handle);
//     int ret = furi_hal_i2c_master_tx_blocking(instance->i2c_handle, instance->address, buffer, sizeof(buffer), FURI_HAL_I2C_TIMEOUT_US);
//     furi_hal_i2c_release(instance->i2c_handle);

//     if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
//         FURI_LOG_E(TAG, "Failed to write reg 0x%02X", reg);
//     } else {
//         HD3SS3220_DEBUG(TAG, "Wrote reg 0x%02X: 0x%02X %08b", reg, data[0], data[0]);
//     }

//     return hd3ss3220_check_status(ret);
// }

// static Hd3ss3220Status hd3ss3220_read_reg(Hd3ss3220* instance, Hd3ss3220Reg reg, uint8_t* data) {
//     furi_check(instance);
//     furi_check(data);

//     furi_hal_i2c_acquire(instance->i2c_handle);
//     int ret = furi_hal_i2c_master_trx_blocking(instance->i2c_handle, instance->address, (uint8_t*)&reg, 1, data, 1, FURI_HAL_I2C_TIMEOUT_US);
//     furi_hal_i2c_release(instance->i2c_handle);

//     if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
//         FURI_LOG_E(TAG, "Failed to read reg 0x%02X", reg);
//     } else {
//         HD3SS3220_DEBUG(TAG, "Read reg 0x%02X: %08b", reg, *data);
//     }

//     return hd3ss3220_check_status(ret);
// }

static __isr __not_in_flash_func(void) hd3ss3220_interrupt_handler(void* ctx) {
    Hd3ss3220* instance = (Hd3ss3220*)ctx;
    if(instance->input_callback) {
        instance->input_callback(instance->callback_context);
    }
}

Hd3ss3220* hd3ss3220_init(const FuriHalI2cBusHandle* i2c_handle, uint8_t address, const GpioPin* pin_interrupt) {
    Hd3ss3220* instance = (Hd3ss3220*)malloc(sizeof(Hd3ss3220));
    instance->i2c_handle = i2c_handle;
    instance->address = address;
    instance->pin_interrupt = pin_interrupt;

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_device_ready(instance->i2c_handle, instance->address, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);
    if(ret) {
        FURI_LOG_I(TAG, "HD3SS3220 device ready at address 0x%02X", instance->address);
        if(instance->pin_interrupt) {
            furi_hal_gpio_init_simple(instance->pin_interrupt, GpioModeInput);
            furi_hal_gpio_add_int_callback(instance->pin_interrupt, GpioConditionFall, hd3ss3220_interrupt_handler, instance);
        }
    } else {
        FURI_LOG_E(TAG, "HD3SS3220 device not ready at address 0x%02X", instance->address);
        free(instance);
        return NULL;
    }

    return instance;
}

void hd3ss3220_deinit(Hd3ss3220* instance) {
    furi_check(instance);
    if(instance->pin_interrupt) {
        furi_hal_gpio_remove_int_callback(instance->pin_interrupt);
        furi_hal_gpio_init_ex(instance->pin_interrupt, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    }
    free(instance);
}

void hd3ss3220_set_input_callback(Hd3ss3220* instance, Hd3ss3220CallbackInput callback, void* context) {
    furi_check(instance);
    FURI_CRITICAL_ENTER();
    instance->input_callback = callback;
    instance->callback_context = context;
    FURI_CRITICAL_EXIT();
}

//TODO: add more functions to control hd3ss3220, e.g. set mode, read status, etc.
