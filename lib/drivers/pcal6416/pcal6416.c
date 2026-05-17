#include "furi_hal_gpio.h"
#include "pcal6416_reg.h"
#include "pcal6416.h"
#include <furi.h>

#include <furi_hal_i2c.h>

#define TAG "Pcal6416"

#ifdef PCAL6416_DEBUG_ENABLE
#define PCAL6416_DEBUG(...) FURI_LOG_D(__VA_ARGS__)
#else
#define PCAL6416_DEBUG(...)
#endif

struct Pcal6416 {
    const FuriHalI2cBusHandle* i2c_handle;
    const GpioPin* pin_reset;
    const GpioPin* pin_interrupt;
    uint8_t address;
    Pcal6416CallbackInput input_callback;
    void* callback_context;
};

static __isr __not_in_flash_func(void) pcal6416_interrupt_handler(void* ctx) {
    Pcal6416* instance = (Pcal6416*)ctx;
    if(instance->input_callback) {
        instance->input_callback(instance->callback_context);
    }
}

Pcal6416* pcal6416_init(const FuriHalI2cBusHandle* i2c_handle, const GpioPin* pin_reset, const GpioPin* pin_interrupt, uint8_t address) {
    Pcal6416* instance = (Pcal6416*)malloc(sizeof(Pcal6416));
    instance->i2c_handle = i2c_handle;
    instance->pin_reset = pin_reset;
    instance->pin_interrupt = pin_interrupt;
    instance->address = address;
    furi_hal_gpio_write(instance->pin_reset, true);
    furi_hal_gpio_init_simple(instance->pin_reset, GpioModeOutputOpenDrain);
    furi_hal_gpio_write_open_drain(instance->pin_reset, true);
    furi_delay_ms(10);

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_device_ready(instance->i2c_handle, instance->address, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret) {
        furi_hal_gpio_init_simple(instance->pin_interrupt, GpioModeInput);
        furi_hal_gpio_add_int_callback(instance->pin_interrupt, GpioConditionFall, pcal6416_interrupt_handler, instance);
    } else {
        FURI_LOG_E(TAG, "PCAL6416 device not ready at address 0x%02X", instance->address);
        furi_hal_gpio_init_ex(instance->pin_reset, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
        free(instance);
        return NULL;
    }

    return instance;
}

void pcal6416_deinit(Pcal6416* instance) {
    furi_check(instance);
    furi_hal_gpio_remove_int_callback(instance->pin_interrupt);
    furi_hal_gpio_init_ex(instance->pin_interrupt, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(instance->pin_reset, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    free(instance);
}

void pcal6416_set_input_callback(Pcal6416* instance, Pcal6416CallbackInput callback, void* context) {
    furi_check(instance);
    FURI_CRITICAL_ENTER();
    instance->input_callback = callback;
    instance->callback_context = context;
    FURI_CRITICAL_EXIT();
}

static FURI_ALWAYS_INLINE int pcal6416_write_reg(Pcal6416* instance, Pcal6416Reg reg, uint16_t data) {
    furi_check(instance);

    uint8_t buffer[3] = {reg, data & 0xFF, data >> 8};

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_master_tx_blocking(instance->i2c_handle, instance->address, buffer, sizeof(buffer), FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
        FURI_LOG_E(TAG, "Failed to write reg 0x%02X", reg);
    } else {
        PCAL6416_DEBUG(TAG, "Wrote reg 0x%02X: %016b", reg, data);
    }

    return ret;
}

static FURI_ALWAYS_INLINE int pcal6416_read_reg(Pcal6416* instance, Pcal6416Reg reg, uint16_t* data) {
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

bool pcal6416_write_mode(Pcal6416* instance, uint16_t port_mask) {
    furi_check(instance);
    bool res = false;
    int ret = PICO_ERROR_GENERIC;
    do {
        ret = pcal6416_write_reg(instance, Pcal6416RegInterruptMaskPort0, ~port_mask);
        if(ret < PICO_OK) {
            break;
        }
        ret = pcal6416_write_reg(instance, Pcal6416RegInputLatchPort0, port_mask);
        if(ret < PICO_OK) {
            break;
        }
        ret = pcal6416_write_reg(instance, Pcal6416RegConfigurationPort0, port_mask);
        if(ret < PICO_OK) {
            break;
        }
        res = true;
    } while(0);

    return res;
}

uint16_t pcal6416_read_mode(Pcal6416* instance) {
    furi_check(instance);
    uint16_t port_mask = 0;
    if(pcal6416_read_reg(instance, Pcal6416RegConfigurationPort0, &port_mask) != PICO_ERROR_GENERIC) {
        return port_mask;
    }
    return 0xFFFF; // Indicate error
}

bool pcal6416_write_output(Pcal6416* instance, uint16_t output_mask) {
    furi_check(instance);
    return pcal6416_write_reg(instance, Pcal6416RegOutputPort0, output_mask) != PICO_ERROR_GENERIC;
}

uint16_t pcal6416_read_input(Pcal6416* instance) {
    furi_check(instance);
    uint16_t input_mask = 0;
    if(pcal6416_read_reg(instance, Pcal6416RegInputPort0, &input_mask) != PICO_ERROR_GENERIC) {
        return input_mask;
    }
    return 0xFFFF; // Indicate error
}
