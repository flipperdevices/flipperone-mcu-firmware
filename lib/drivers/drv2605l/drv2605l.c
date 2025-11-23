#include "furi_hal_gpio.h"
#include "drv2605l_reg.h"
#include "drv2605l.h"
#include <furi.h>

#include <furi_hal_i2c.h>

#define TAG "Drv2605l"

struct Drv2605l {
    const FuriHalI2cBusHandle* i2c_handle;
    const GpioPin* pin_reset;
    const GpioPin* pin_interrupt;
    uint8_t address;
    Drv2605lCallbackInput input_callback;
    void* callback_context;
};

static __isr __not_in_flash_func(void) drv2605l_interrupt_handler(void* ctx) {
    Drv2605l* instance = (Drv2605l*)ctx;
    if(instance->input_callback) {
        instance->input_callback(instance->callback_context);
    }
}

Drv2605l* drv2605l_init(const FuriHalI2cBusHandle* i2c_handle, const GpioPin* pin_reset, const GpioPin* pin_interrupt, uint8_t address) {
    Drv2605l* instance = (Drv2605l*)malloc(sizeof(Drv2605l));
    instance->i2c_handle = i2c_handle;
    instance->pin_reset = pin_reset;
    instance->pin_interrupt = pin_interrupt;
    instance->address = address;
    furi_hal_gpio_init_simple(instance->pin_reset, GpioModeOutputOpenDrain);
    furi_hal_gpio_write_open_drain(instance->pin_reset, false);
    furi_delay_ms(10);
    furi_hal_gpio_write_open_drain(instance->pin_reset, true);
    furi_delay_ms(10);

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_device_ready(instance->i2c_handle, instance->address, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret) {
        drv2605l_write_output(instance, 0x0000); // All low
        furi_hal_gpio_init_simple(instance->pin_interrupt, GpioModeInput);
        furi_hal_gpio_add_int_callback(instance->pin_interrupt, GpioConditionFall, drv2605l_interrupt_handler, instance);
    } else {
        FURI_LOG_E(TAG, "DRV2605L device not ready at address 0x%02X", instance->address);
        furi_hal_gpio_init_ex(instance->pin_reset, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
        free(instance);
        return NULL;
    }

    return instance;
}

void drv2605l_deinit(Drv2605l* instance) {
    furi_check(instance);
    furi_hal_gpio_remove_int_callback(instance->pin_interrupt);
    furi_hal_gpio_init_ex(instance->pin_reset, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    free(instance);
}

void drv2605l_set_input_callback(Drv2605l* instance, Drv2605lCallbackInput callback, void* context) {
    furi_check(instance);
    instance->input_callback = callback;
    instance->callback_context = context;
}

static FURI_ALWAYS_INLINE int drv2605l_write_reg(Drv2605l* instance, Drv2605lReg reg, uint16_t data) {
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

static FURI_ALWAYS_INLINE int drv2605l_read_reg(Drv2605l* instance, Drv2605lReg reg, uint16_t* data) {
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

bool drv2605l_write_mode(Drv2605l* instance, uint16_t port_mask) {
    furi_check(instance);
    return drv2605l_write_reg(instance, configuration_port_0, port_mask) != PICO_ERROR_GENERIC;
}

uint16_t drv2605l_read_mode(Drv2605l* instance) {
    furi_check(instance);
    uint16_t port_mask = 0;
    if(drv2605l_read_reg(instance, configuration_port_0, &port_mask) != PICO_ERROR_GENERIC) {
        return port_mask;
    }
    return 0xFFFF; // Indicate error
}

bool drv2605l_write_output(Drv2605l* instance, uint16_t output_mask) {
    furi_check(instance);
    return drv2605l_write_reg(instance, output_port_0, output_mask) != PICO_ERROR_GENERIC;
}

uint16_t drv2605l_read_input(Drv2605l* instance) {
    furi_check(instance);
    uint16_t input_mask = 0;
    if(drv2605l_read_reg(instance, input_port_0, &input_mask) != PICO_ERROR_GENERIC) {
        return input_mask;
    }
    return 0xFFFF; // Indicate error
}
