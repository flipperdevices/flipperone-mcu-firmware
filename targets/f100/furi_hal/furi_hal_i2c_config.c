#include <furi_hal_i2c_config.h>
#include <furi_hal_resources.h>
#include <hardware/i2c.h>
#include <drivers/i2c_master_pio/pio_i2c.h>

#define FURI_HAL_I2C_CONFIG_I2C_TIMINGS_100 100000
#define FURI_HAL_I2C_CONFIG_I2C_TIMINGS_400 400000

static void furi_hal_i2c_bus_common_event(FuriHalI2cBus* bus, FuriHalI2cBusEvent event) {
    if(event == FuriHalI2cBusEventInit) {
        bus->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
        bus->current_handle = NULL;
    } else if(event == FuriHalI2cBusEventDeinit) {
        furi_mutex_free(bus->mutex);
    } else if(event == FuriHalI2cBusEventLock) {
        furi_check(furi_mutex_acquire(bus->mutex, FuriWaitForever) == FuriStatusOk);
    } else if(event == FuriHalI2cBusEventUnlock) {
        furi_check(furi_mutex_release(bus->mutex) == FuriStatusOk);
    }
}

static int furi_hal_i2c_bus_pio_read_blocking(void* instance, uint8_t addr, uint8_t* rxbuf, uint len, bool nostop, absolute_time_t until) {
    return pio_i2c_read_blocking(instance, addr, rxbuf, len, nostop, until);
}

static int furi_hal_i2c_bus_pio_write_blocking(void* instance, uint8_t addr, const uint8_t* src, size_t len, bool nostop, absolute_time_t until) {
    return pio_i2c_write_blocking(instance, addr, src, len, nostop, until);
}

FuriHalI2cBus furi_hal_i2c_bus_external = {
    .data = NULL,
    .name = "PIO I2C",
    .api =
        {
            .event = furi_hal_i2c_bus_common_event,
            .read_blocking = furi_hal_i2c_bus_pio_read_blocking,
            .write_blocking = furi_hal_i2c_bus_pio_write_blocking,
        },
};

void furi_hal_i2c_bus_handle_external_event(const FuriHalI2cBusHandle* handle, FuriHalI2cBusHandleEvent event) {
    UNUSED(handle);

    if(event == FuriHalI2cBusHandleEventActivate) {
        furi_assert(handle->bus->data == NULL);
        handle->bus->data = pio_i2c_init(&gpio_i2c_periph_sda, &gpio_i2c_periph_scl, FURI_HAL_I2C_CONFIG_I2C_TIMINGS_400);
    } else if(event == FuriHalI2cBusHandleEventDeactivate) {
        furi_assert(handle->bus->data != NULL);
        pio_i2c_deinit(handle->bus->data);
        handle->bus->data = NULL;
    }
}

const FuriHalI2cBusHandle furi_hal_i2c_handle_external = {
    .bus = &furi_hal_i2c_bus_external,
    .callback = furi_hal_i2c_bus_handle_external_event,
};

static void furi_hal_i2c_bus_i2c_event(FuriHalI2cBus* bus, FuriHalI2cBusEvent event) {
    i2c_inst_t* i2c = bus->data;
    if(event == FuriHalI2cBusEventActivate) {
        i2c->hw->enable = 1;
    } else if(event == FuriHalI2cBusEventDeactivate) {
        i2c->hw->enable = 0;
    } else {
        furi_hal_i2c_bus_common_event(bus, event);
    }
}

static int furi_hal_i2c_bus_i2c_read_blocking(void* instance, uint8_t addr, uint8_t* rxbuf, uint len, bool nostop, absolute_time_t until) {
    return i2c_read_blocking_until(instance, addr, rxbuf, len, nostop, until);
}

static int furi_hal_i2c_bus_i2c_write_blocking(void* instance, uint8_t addr, const uint8_t* src, size_t len, bool nostop, absolute_time_t until) {
    return i2c_write_blocking_until(instance, addr, src, len, nostop, until);
}

FuriHalI2cBus furi_hal_i2c_bus_internal = {
    .data = i2c0,
    .name = "I2C0",
    .api =
        {
            .event = furi_hal_i2c_bus_i2c_event,
            .read_blocking = furi_hal_i2c_bus_i2c_read_blocking,
            .write_blocking = furi_hal_i2c_bus_i2c_write_blocking,
        },
};

void furi_hal_i2c_bus_handle_internal_event(const FuriHalI2cBusHandle* handle, FuriHalI2cBusHandleEvent event) {
    if(event == FuriHalI2cBusHandleEventActivate) {
        i2c_init(handle->bus->data, FURI_HAL_I2C_CONFIG_I2C_TIMINGS_400);

        furi_hal_gpio_init_ex(&gpio_i2c0_sda, GpioModeOutputPushPull, GpioPullNo, GpioSpeedFast, GpioAltFn3I2c);
        furi_hal_gpio_set_drive_strength(&gpio_i2c0_sda, GpioDriveStrengthMedium);

        furi_hal_gpio_init_ex(&gpio_i2c0_scl, GpioModeOutputPushPull, GpioPullNo, GpioSpeedFast, GpioAltFn3I2c);
        furi_hal_gpio_set_drive_strength(&gpio_i2c0_scl, GpioDriveStrengthMedium);

    } else if(event == FuriHalI2cBusHandleEventDeactivate) {
        i2c_deinit(handle->bus->data);

        furi_hal_gpio_init_ex(&gpio_i2c0_sda, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
        furi_hal_gpio_init_ex(&gpio_i2c0_scl, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);

        furi_hal_gpio_write(&gpio_i2c0_sda, 1);
        furi_hal_gpio_write(&gpio_i2c0_scl, 1);
    }
}

const FuriHalI2cBusHandle furi_hal_i2c_handle_internal = {
    .bus = &furi_hal_i2c_bus_internal,
    .callback = furi_hal_i2c_bus_handle_internal_event,
};
