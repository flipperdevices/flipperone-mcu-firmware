#include <furi_hal_i2c_config.h>
#include <furi_hal_resources.h>
#include <hardware/i2c.h>
#include <drivers/i2c_master_pio/pio_i2c.h>
#include <drivers/i2c_slave/i2c_slave.h>

#define FURI_HAL_I2C_CONFIG_I2C_TIMINGS_100   100000
#define FURI_HAL_I2C_CONFIG_I2C_TIMINGS_400   400000
#define FURI_HAL_I2C_CONFIG_I2C_TIMINGS_1000  1000000
#define FURI_HAL_I2C_CONFIG_I2C_SLAVE_ADDRESS 0x70

extern FuriHalI2cBus furi_hal_i2c_bus_control;
extern FuriHalI2cBus furi_hal_i2c_bus_main;
extern FuriHalI2cBus furi_hal_i2c_bus_cpu;

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

// Main bus

void furi_hal_i2c_init_control(void) {
    furi_hal_i2c_bus_control.api.event(&furi_hal_i2c_bus_control, FuriHalI2cBusEventInit);
}

void furi_hal_i2c_deinit_control(void) {
    furi_hal_i2c_bus_control.api.event(&furi_hal_i2c_bus_control, FuriHalI2cBusEventDeinit);
}

static int furi_hal_i2c_bus_pio_read_blocking(void* instance, uint8_t addr, uint8_t* rxbuf, uint len, bool nostop, absolute_time_t until) {
    return pio_i2c_read_blocking(instance, addr, rxbuf, len, nostop, until);
}

static int furi_hal_i2c_bus_pio_write_blocking(void* instance, uint8_t addr, const uint8_t* src, size_t len, bool nostop, absolute_time_t until) {
    return pio_i2c_write_blocking(instance, addr, src, len, nostop, until);
}

FuriHalI2cBus furi_hal_i2c_bus_main = {
    .data = NULL,
    .name = "PIO I2C",
    .sda = &gpio_i2c_main_sda,
    .scl = &gpio_i2c_main_scl,
    .mode = FuriHalI2cModeMaster,
    .api =
        {.event = furi_hal_i2c_bus_common_event,
         .master =
             {
                 .read_blocking = furi_hal_i2c_bus_pio_read_blocking,
                 .write_blocking = furi_hal_i2c_bus_pio_write_blocking,
             }},
};

void furi_hal_i2c_bus_handle_main_event(const FuriHalI2cBusHandle* handle, FuriHalI2cBusHandleEvent event) {
    UNUSED(handle);

    if(event == FuriHalI2cBusHandleEventActivate) {
        furi_assert(handle->bus->data == NULL);
        handle->bus->data = pio_i2c_init(handle->bus->sda, handle->bus->scl, FURI_HAL_I2C_CONFIG_I2C_TIMINGS_100);
    } else if(event == FuriHalI2cBusHandleEventDeactivate) {
        furi_assert(handle->bus->data != NULL);
        pio_i2c_deinit(handle->bus->data);
        handle->bus->data = NULL;
    }
}

const FuriHalI2cBusHandle furi_hal_i2c_handle_main = {
    .bus = &furi_hal_i2c_bus_main,
    .callback = furi_hal_i2c_bus_handle_main_event,
};

void furi_hal_i2c_init_main(void) {
    furi_hal_i2c_bus_main.api.event(&furi_hal_i2c_bus_main, FuriHalI2cBusEventInit);
}

void furi_hal_i2c_deinit_main(void) {
    furi_hal_i2c_bus_main.api.event(&furi_hal_i2c_bus_main, FuriHalI2cBusEventDeinit);
}

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

// Main bus
static int furi_hal_i2c_bus_i2c_read_blocking(void* instance, uint8_t addr, uint8_t* rxbuf, uint len, bool nostop, absolute_time_t until) {
    return i2c_read_blocking_until(instance, addr, rxbuf, len, nostop, until);
}

static int furi_hal_i2c_bus_i2c_write_blocking(void* instance, uint8_t addr, const uint8_t* src, size_t len, bool nostop, absolute_time_t until) {
    return i2c_write_blocking_until(instance, addr, src, len, nostop, until);
}

FuriHalI2cBus furi_hal_i2c_bus_control = {
    .data = i2c0,
    .name = "I2C0",
    .sda = &gpio_i2c_control_sda,
    .scl = &gpio_i2c_control_scl,
    .mode = FuriHalI2cModeMaster,
    .api =
        {.event = furi_hal_i2c_bus_i2c_event,
         .master =
             {
                 .read_blocking = furi_hal_i2c_bus_i2c_read_blocking,
                 .write_blocking = furi_hal_i2c_bus_i2c_write_blocking,
             }},
};

void furi_hal_i2c_bus_handle_control_event(const FuriHalI2cBusHandle* handle, FuriHalI2cBusHandleEvent event) {
    if(event == FuriHalI2cBusHandleEventActivate) {
        i2c_init(handle->bus->data, FURI_HAL_I2C_CONFIG_I2C_TIMINGS_400);

        furi_hal_gpio_init_ex(handle->bus->sda, GpioModeOutputPushPull, GpioPullNo, GpioSpeedFast, GpioAltFn3I2c);
        furi_hal_gpio_set_drive_strength(handle->bus->sda, GpioDriveStrengthMedium);

        furi_hal_gpio_init_ex(handle->bus->scl, GpioModeOutputPushPull, GpioPullNo, GpioSpeedFast, GpioAltFn3I2c);
        furi_hal_gpio_set_drive_strength(handle->bus->scl, GpioDriveStrengthMedium);

    } else if(event == FuriHalI2cBusHandleEventDeactivate) {
        i2c_deinit(handle->bus->data);

        furi_hal_gpio_init_ex(handle->bus->sda, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
        furi_hal_gpio_init_ex(handle->bus->scl, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);

        furi_hal_gpio_write(handle->bus->sda, 1);
        furi_hal_gpio_write(handle->bus->scl, 1);
    }
}

const FuriHalI2cBusHandle furi_hal_i2c_handle_control = {
    .bus = &furi_hal_i2c_bus_control,
    .callback = furi_hal_i2c_bus_handle_control_event,
};

// CPU bus

void furi_hal_i2c_init_cpu(void) {
    furi_hal_i2c_bus_cpu.api.event(&furi_hal_i2c_bus_cpu, FuriHalI2cBusEventInit);
}

void furi_hal_i2c_deinit_cpu(void) {
    furi_hal_i2c_bus_cpu.api.event(&furi_hal_i2c_bus_cpu, FuriHalI2cBusEventDeinit);
}

static uint8_t furi_hal_i2c_bus_slave_write(const FuriHalI2cBusHandle* handle, uint8_t* data, size_t size) {
    furi_assert(handle);
    furi_assert(data);
    // max size write 16 bytes.
    uint8_t len = 0;
    i2c_inst_t* i2c = handle->bus->data;
    while(i2c_get_write_available(i2c) && len < size) {
        i2c_get_hw(i2c)->data_cmd = *data++;
        len++;
    }
    return len;
}

static uint8_t furi_hal_i2c_bus_slave_read(const FuriHalI2cBusHandle* handle, uint8_t* data, size_t size) {
    furi_assert(handle);
    furi_assert(data);
    // max size read 16 bytes.
    uint8_t len = 0;
    i2c_inst_t* i2c = handle->bus->data;
    while(i2c_get_read_available(i2c) && len < size) {
        *data++ = (uint8_t)i2c_get_hw(i2c)->data_cmd;
        len++;
    }
    return len;
}

static void __isr __not_in_flash_func(furi_hal_i2c_bus_cpu_slave_callback)(i2c_inst_t* i2c, I2cSlaveEvent event) {
    switch(event) {
    case I2cSlaveEventStart: // master has sent a Start signal
        if(furi_hal_i2c_bus_cpu.api.slave.callback) {
            furi_hal_i2c_bus_cpu.api.slave.callback(&furi_hal_i2c_handle_cpu, FuriHalI2cBusSlaveEventStart, furi_hal_i2c_bus_cpu.api.slave.context);
        }
        break;
    case I2cSlaveEventReceive: // master has written some data
        if(furi_hal_i2c_bus_cpu.api.slave.callback) {
            furi_hal_i2c_bus_cpu.api.slave.callback(&furi_hal_i2c_handle_cpu, FuriHalI2cBusSlaveEventReceive, furi_hal_i2c_bus_cpu.api.slave.context);
        }
        break;
    case I2cSlaveEventRequest: // master is reading data
        if(furi_hal_i2c_bus_cpu.api.slave.callback) {
            furi_hal_i2c_bus_cpu.api.slave.callback(&furi_hal_i2c_handle_cpu, FuriHalI2cBusSlaveEventRequest, furi_hal_i2c_bus_cpu.api.slave.context);
        }
        break;
    case I2cSlaveEventRepeatedStart: // master has signalled Repeated Start
        if(furi_hal_i2c_bus_cpu.api.slave.callback) {
            furi_hal_i2c_bus_cpu.api.slave.callback(&furi_hal_i2c_handle_cpu, FuriHalI2cBusSlaveEventRepeatedStart, furi_hal_i2c_bus_cpu.api.slave.context);
        }
        break;
    case I2cSlaveEventStop: // master has signalled Stop
        if(furi_hal_i2c_bus_cpu.api.slave.callback) {
            furi_hal_i2c_bus_cpu.api.slave.callback(&furi_hal_i2c_handle_cpu, FuriHalI2cBusSlaveEventStop, furi_hal_i2c_bus_cpu.api.slave.context);
        }
        break;
    default:
        break;
    }
}

FuriHalI2cBus furi_hal_i2c_bus_cpu = {
    .data = i2c1,
    .name = "I2C1",
    .sda = &gpio_cpu_i3c0_sda,
    .scl = &gpio_cpu_i3c0_scl,
    .mode = FuriHalI2cModeSlave,
    .api =
        {.event = furi_hal_i2c_bus_i2c_event,
         .slave =
             {
                 .read_blocking = furi_hal_i2c_bus_slave_read,
                 .write_blocking = furi_hal_i2c_bus_slave_write,
                 .callback = NULL,
                 .context = NULL,
             }},
};

void furi_hal_i2c_bus_handle_cpu_event(const FuriHalI2cBusHandle* handle, FuriHalI2cBusHandleEvent event) {
    if(event == FuriHalI2cBusHandleEventActivate) {
        i2c_init(handle->bus->data, FURI_HAL_I2C_CONFIG_I2C_TIMINGS_1000);

        furi_hal_gpio_init_ex(handle->bus->sda, GpioModeOutputPushPull, GpioPullNo, GpioSpeedFast, GpioAltFn3I2c);
        furi_hal_gpio_set_drive_strength(handle->bus->sda, GpioDriveStrengthMedium);

        furi_hal_gpio_init_ex(handle->bus->scl, GpioModeOutputPushPull, GpioPullNo, GpioSpeedFast, GpioAltFn3I2c);
        furi_hal_gpio_set_drive_strength(handle->bus->scl, GpioDriveStrengthMedium);

        i2c_slave_init(handle->bus->data, FURI_HAL_I2C_CONFIG_I2C_SLAVE_ADDRESS, &furi_hal_i2c_bus_cpu_slave_callback);

    } else if(event == FuriHalI2cBusHandleEventDeactivate) {
        i2c_slave_deinit(handle->bus->data);
        i2c_deinit(handle->bus->data);

        furi_hal_gpio_init_ex(handle->bus->sda, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
        furi_hal_gpio_init_ex(handle->bus->scl, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);

        furi_hal_gpio_write(handle->bus->sda, 1);
        furi_hal_gpio_write(handle->bus->scl, 1);
    }
}

const FuriHalI2cBusHandle furi_hal_i2c_handle_cpu = {
    .bus = &furi_hal_i2c_bus_cpu,
    .callback = furi_hal_i2c_bus_handle_cpu_event,
};
