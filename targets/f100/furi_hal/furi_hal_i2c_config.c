#include <furi_hal_i2c_config.h>
#include <furi_hal_resources.h>

#define FURI_HAL_I2C_CONFIG_I2C_TIMINGS_100 100000
#define FURI_HAL_I2C_CONFIG_I2C_TIMINGS_400 400000

FuriMutex* furi_hal_i2c_bus_internal_mutex = NULL;
FuriMutex* furi_hal_i2c_bus_external_mutex = NULL;

static void furi_hal_i2c_bus_external_event(FuriHalI2cBus* bus, FuriHalI2cBusEvent event) {
    if(event == FuriHalI2cBusEventInit) {
        furi_hal_i2c_bus_external_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
        bus->current_handle = NULL;
    } else if(event == FuriHalI2cBusEventDeinit) {
        furi_mutex_free(furi_hal_i2c_bus_external_mutex);
    } else if(event == FuriHalI2cBusEventLock) {
        furi_check(furi_mutex_acquire(furi_hal_i2c_bus_external_mutex, FuriWaitForever) == FuriStatusOk);
    } else if(event == FuriHalI2cBusEventUnlock) {
        furi_check(furi_mutex_release(furi_hal_i2c_bus_external_mutex) == FuriStatusOk);
    } else if(event == FuriHalI2cBusEventActivate) {
        // Not need
    } else if(event == FuriHalI2cBusEventDeactivate) {
        // Not need
    }
}

FuriHalI2cBus furi_hal_i2c_bus_external = {
    .as.pio_i2c = NULL,
    .id = FuriHalI2cIdPio,
    .callback = furi_hal_i2c_bus_external_event,
};

void furi_hal_i2c_bus_handle_external_event(const FuriHalI2cBusHandle* handle, FuriHalI2cBusHandleEvent event) {
    UNUSED(handle);

    if(event == FuriHalI2cBusHandleEventActivate) {
        handle->bus->as.pio_i2c = pio_i2c_init(&gpio_i2c1_sda, &gpio_i2c1_scl, FURI_HAL_I2C_CONFIG_I2C_TIMINGS_400);
    } else if(event == FuriHalI2cBusHandleEventDeactivate) {
        pio_i2c_deinit(handle->bus->as.pio_i2c);
        handle->bus->as.pio_i2c = NULL;
    }
}

const FuriHalI2cBusHandle furi_hal_i2c_handle_external = {
    .bus = &furi_hal_i2c_bus_external,
    .callback = furi_hal_i2c_bus_handle_external_event,
};

static void furi_hal_i2c_bus_internal_event(FuriHalI2cBus* bus, FuriHalI2cBusEvent event) {
    if(event == FuriHalI2cBusEventInit) {
        furi_hal_i2c_bus_internal_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
        bus->current_handle = NULL;
    } else if(event == FuriHalI2cBusEventDeinit) {
        furi_mutex_free(furi_hal_i2c_bus_internal_mutex);
    } else if(event == FuriHalI2cBusEventLock) {
        furi_check(furi_mutex_acquire(furi_hal_i2c_bus_internal_mutex, FuriWaitForever) == FuriStatusOk);
    } else if(event == FuriHalI2cBusEventUnlock) {
        furi_check(furi_mutex_release(furi_hal_i2c_bus_internal_mutex) == FuriStatusOk);
    } else if(event == FuriHalI2cBusEventActivate) {
        i2c0->hw->enable = 1;
    } else if(event == FuriHalI2cBusEventDeactivate) {
        i2c0->hw->enable = 0;
    }
}

FuriHalI2cBus furi_hal_i2c_bus_internal = {
    .as.i2c = i2c0,
    .id = FuriHalI2cIdI2c0,
    .callback = furi_hal_i2c_bus_internal_event,
};

void furi_hal_i2c_bus_handle_internal_event(const FuriHalI2cBusHandle* handle, FuriHalI2cBusHandleEvent event) {
    if(event == FuriHalI2cBusHandleEventActivate) {
        i2c_init(handle->bus->as.i2c, FURI_HAL_I2C_CONFIG_I2C_TIMINGS_400);

        furi_hal_gpio_init_ex(&gpio_i2c0_sda, GpioModeOutputPushPull, GpioPullNo, GpioSpeedFast, GpioAltFn3I2c);
        furi_hal_gpio_set_drive_strength(&gpio_i2c0_sda, GpioDriveStrengthMedium);

        furi_hal_gpio_init_ex(&gpio_i2c0_scl, GpioModeOutputPushPull, GpioPullNo, GpioSpeedFast, GpioAltFn3I2c);
        furi_hal_gpio_set_drive_strength(&gpio_i2c0_scl, GpioDriveStrengthMedium);

    } else if(event == FuriHalI2cBusHandleEventDeactivate) {
        i2c_deinit(handle->bus->as.i2c);

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
