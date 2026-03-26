#include "furi_hal_gpio.h"
#include "tca6416a_reg.h"
#include "tca6416a.h"
#include <furi.h>

#include <furi_hal_i2c.h>

#define TAG "Tca6416a"

#ifdef TCA6416A_DEBUG_ENABLE
#define TCA6416A_DEBUG(...) FURI_LOG_D(__VA_ARGS__)
#else
#define TCA6416A_DEBUG(...)
#endif


struct Tca6416a {
    const FuriHalI2cBusHandle* i2c_handle;
    const GpioPin* pin_reset;
    const GpioPin* pin_interrupt;
    uint8_t address;
    Tca6416aCallbackInput input_callback;
    void* callback_context;
};

static __isr __not_in_flash_func(void) tca6416a_interrupt_handler(void* ctx) {
    Tca6416a* instance = (Tca6416a*)ctx;
    if(instance->input_callback) {
        instance->input_callback(instance->callback_context);
    }
}

Tca6416a* tca6416a_init(const FuriHalI2cBusHandle* i2c_handle, const GpioPin* pin_reset, const GpioPin* pin_interrupt, uint8_t address) {
    Tca6416a* instance = (Tca6416a*)malloc(sizeof(Tca6416a));
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
        tca6416a_write_output(instance, 0x0000); // All low
        furi_hal_gpio_init_simple(instance->pin_interrupt, GpioModeInput);
        furi_hal_gpio_add_int_callback(instance->pin_interrupt, GpioConditionFall, tca6416a_interrupt_handler, instance);
    } else {
        FURI_LOG_E(TAG, "TCA6416A device not ready at address 0x%02X", instance->address);
        furi_hal_gpio_init_ex(instance->pin_reset, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
        free(instance);
        return NULL;
    }

    return instance;
}

void tca6416a_deinit(Tca6416a* instance) {
    furi_check(instance);
    furi_hal_gpio_remove_int_callback(instance->pin_interrupt);
    furi_hal_gpio_init_ex(instance->pin_interrupt, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(instance->pin_reset, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    free(instance);
}

void tca6416a_set_input_callback(Tca6416a* instance, Tca6416aCallbackInput callback, void* context) {
    furi_check(instance);
    FURI_CRITICAL_ENTER();
    instance->input_callback = callback;
    instance->callback_context = context;
    FURI_CRITICAL_EXIT();
}

static FURI_ALWAYS_INLINE int tca6416a_write_reg(Tca6416a* instance, Tca6416aReg reg, uint16_t data) {
    furi_check(instance);

    uint8_t buffer[3] = {reg, data & 0xFF, data >> 8};

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_master_tx_blocking(instance->i2c_handle, instance->address, buffer, sizeof(buffer), FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT)  {
        FURI_LOG_E(TAG, "Failed to write reg 0x%02X", reg);
    } else {
        TCA6416A_DEBUG(TAG, "Wrote reg 0x%02X: %016b", reg, data);
    }

    return ret;
}

static FURI_ALWAYS_INLINE int tca6416a_read_reg(Tca6416a* instance, Tca6416aReg reg, uint16_t* data) {
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
            *data = buffer[0] | (buffer[1] << 8);
        }
    } else {
        FURI_LOG_E(TAG, "Failed to write reg address 0x%02X for reading", reg);
    }
    furi_hal_i2c_release(instance->i2c_handle);

    return ret;
}

bool tca6416a_write_mode(Tca6416a* instance, uint16_t port_mask) {
    furi_check(instance);
    return tca6416a_write_reg(instance, configuration_port_0, port_mask) != PICO_ERROR_GENERIC;
}

uint16_t tca6416a_read_mode(Tca6416a* instance) {
    furi_check(instance);
    uint16_t port_mask = 0;
    if(tca6416a_read_reg(instance, configuration_port_0, &port_mask) != PICO_ERROR_GENERIC) {
        return port_mask;
    }
    return 0xFFFF; // Indicate error
}

bool tca6416a_write_output(Tca6416a* instance, uint16_t output_mask) {
    furi_check(instance);
    return tca6416a_write_reg(instance, output_port_0, output_mask) != PICO_ERROR_GENERIC;
}

uint16_t tca6416a_read_input(Tca6416a* instance) {
    furi_check(instance);
    uint16_t input_mask = 0;
    if(tca6416a_read_reg(instance, input_port_0, &input_mask) != PICO_ERROR_GENERIC) {
        return input_mask;
    }
    return 0xFFFF; // Indicate error
}
