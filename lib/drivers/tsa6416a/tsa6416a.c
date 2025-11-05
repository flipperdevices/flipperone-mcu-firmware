#include "tsa6416a_reg.h"
#include "tsa6416a.h"
#include <furi.h>

#include <furi_hal_i2c.h>

#define TAG "Tsa6416a"

struct Tsa6416a {
    const FuriHalI2cBusHandle* i2c_handle;
    const GpioPin* pin_reset;
    const GpioPin* pin_interrupt;
    uint8_t address;
    Tsa6416aCallbackInput input_callback;
    void* callback_context;
};

static __isr __not_in_flash_func(void) tsa6416a_interrupt_handler(void* ctx) {
    Tsa6416a* instance = (Tsa6416a*)ctx;
    if(instance->input_callback) {
        instance->input_callback(instance->callback_context);
    }
}

Tsa6416a* tsa6416a_init(const FuriHalI2cBusHandle* i2c_handle, const GpioPin* pin_reset, const GpioPin* pin_interrupt, uint8_t address) {
    Tsa6416a* instance = (Tsa6416a*)malloc(sizeof(Tsa6416a));
    instance->i2c_handle = i2c_handle;
    instance->pin_reset = pin_reset;
    instance->pin_interrupt = pin_interrupt;
    instance->address = address;
    //Todo open drain implementation!
    furi_hal_gpio_init_simple(instance->pin_reset, GpioModeOutputPushPull);
    furi_hal_gpio_write(instance->pin_reset, false);
    furi_delay_ms(10);
    furi_hal_gpio_write(instance->pin_reset, true);
    furi_delay_ms(10);

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_device_ready(instance->i2c_handle, instance->address, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret) {
        furi_hal_gpio_init_simple(instance->pin_interrupt, GpioModeInput);
        furi_hal_gpio_add_int_callback(instance->pin_interrupt, GpioConditionFall, tsa6416a_interrupt_handler, instance);
    } else {
        FURI_LOG_E(TAG, "TSA6416A device not ready at address 0x%02X", instance->address);
        furi_hal_gpio_init_ex(instance->pin_reset, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
        free(instance);
        return NULL;
    }

    return instance;
}

void tsa6416a_deinit(Tsa6416a* instance) {
    furi_check(instance);
    furi_hal_gpio_remove_int_callback(instance->pin_interrupt);
    furi_hal_gpio_init_ex(instance->pin_reset, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    free(instance);
}

void tsa6416a_set_input_callback(Tsa6416a* instance, Tsa6416aCallbackInput callback, void* context) {
    furi_check(instance);
    instance->input_callback = callback;
    instance->callback_context = context;
}

static FURI_ALWAYS_INLINE int tsa6416a_write_reg(Tsa6416a* instance, Tsa6416aReg reg, uint16_t data) {
    furi_check(instance);

    uint8_t buffer[3] = {reg, data & 0xF, data >> 8};

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

static FURI_ALWAYS_INLINE int tsa6416a_read_reg(Tsa6416a* instance, Tsa6416aReg reg, uint16_t* data) {
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

bool tsa6416a_write_mode(Tsa6416a* instance, uint16_t port_mask) {
    furi_check(instance);
    return tsa6416a_write_reg(instance, configuration_port_0, port_mask) != PICO_ERROR_GENERIC;
}

uint16_t tsa6416a_read_mode(Tsa6416a* instance) {
    furi_check(instance);
    uint16_t port_mask = 0;
    if(tsa6416a_read_reg(instance, configuration_port_0, &port_mask) != PICO_ERROR_GENERIC) {
        return port_mask;
    }
    return 0xFFFF; // Indicate error
}

bool tsa6416a_write_output(Tsa6416a* instance, uint16_t output_mask) {
    furi_check(instance);
    return tsa6416a_write_reg(instance, output_port_0, output_mask) != PICO_ERROR_GENERIC;
}

uint16_t tsa6416a_read_input(Tsa6416a* instance) {
    furi_check(instance);
    uint16_t input_mask = 0;
    if(tsa6416a_read_reg(instance, input_port_0, &input_mask) != PICO_ERROR_GENERIC) {
        return input_mask;
    }
    return 0xFFFF; // Indicate error
}
