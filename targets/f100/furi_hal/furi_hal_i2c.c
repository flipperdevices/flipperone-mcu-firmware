#include <furi_hal_i2c.h>
#include <furi_hal_i2c_config.h>
#include <furi_hal.h>
#include <furi_hal_power.h>
#include <hardware/i2c.h>
#include <drivers/i2c_master_pio/pio_i2c.h>

#define TAG "FuriHalI2c"

static inline void furi_hal_i2c_check_handle_is_acquired_master(const FuriHalI2cBusHandle* handle) {
    furi_check(handle);
    furi_check(handle->bus->current_handle == handle);
    furi_check(handle->bus->mode == FuriHalI2cModeMaster);
}

static inline void furi_hal_i2c_check_handle_is_acquired_slave(const FuriHalI2cBusHandle* handle) {
    furi_check(handle);
    furi_check(handle->bus->current_handle == handle);
    furi_check(handle->bus->mode == FuriHalI2cModeSlave);
}

void furi_hal_i2c_acquire(const FuriHalI2cBusHandle* handle) {
    furi_hal_power_insomnia_enter();
    // Lock bus access
    handle->bus->api.event(handle->bus, FuriHalI2cBusEventLock);
    // Ensure that no active handle set
    furi_check(handle->bus->current_handle == NULL);
    // Set current handle
    handle->bus->current_handle = handle;
    // Activate bus
    handle->bus->api.event(handle->bus, FuriHalI2cBusEventActivate);
    // Activate handle
    handle->callback(handle, FuriHalI2cBusHandleEventActivate);
}

void furi_hal_i2c_release(const FuriHalI2cBusHandle* handle) {
    //TODO: TIME IS NEEDED TO COMPLETE THE TRANSACTION, Something needs to be done about this
    furi_delay_us(10);
    // Ensure that current handle is our handle
    furi_check(handle->bus->current_handle == handle);
    // Deactivate handle
    handle->callback(handle, FuriHalI2cBusHandleEventDeactivate);
    // Deactivate bus
    handle->bus->api.event(handle->bus, FuriHalI2cBusEventDeactivate);
    // Reset current handle
    handle->bus->current_handle = NULL;
    // Unlock bus
    handle->bus->api.event(handle->bus, FuriHalI2cBusEventUnlock);
    furi_hal_power_insomnia_exit();
}

int furi_hal_i2c_master_tx_blocking(const FuriHalI2cBusHandle* handle, uint8_t device_address, const uint8_t* tx_buffer, size_t size, uint32_t timeout_us) {
    furi_hal_i2c_check_handle_is_acquired_master(handle);
    return handle->bus->api.master.write_blocking(handle->bus->data, device_address, tx_buffer, size, false, make_timeout_time_us(timeout_us));
}

int furi_hal_i2c_master_tx_blocking_nostop(
    const FuriHalI2cBusHandle* handle,
    uint8_t device_address,
    const uint8_t* tx_buffer,
    size_t size,
    uint32_t timeout_us) {
    furi_hal_i2c_check_handle_is_acquired_master(handle);
    return handle->bus->api.master.write_blocking(handle->bus->data, device_address, tx_buffer, size, true, make_timeout_time_us(timeout_us));
}

int furi_hal_i2c_master_rx_blocking(const FuriHalI2cBusHandle* handle, uint8_t device_address, uint8_t* rx_buffer, size_t size, uint32_t timeout_us) {
    furi_hal_i2c_check_handle_is_acquired_master(handle);
    return handle->bus->api.master.read_blocking(handle->bus->data, device_address, rx_buffer, size, false, make_timeout_time_us(timeout_us));
}

int furi_hal_i2c_master_rx_blocking_nostop(const FuriHalI2cBusHandle* handle, uint8_t device_address, uint8_t* rx_buffer, size_t size, uint32_t timeout_us) {
    furi_hal_i2c_check_handle_is_acquired_master(handle);
    return handle->bus->api.master.read_blocking(handle->bus->data, device_address, rx_buffer, size, true, make_timeout_time_us(timeout_us));
}

int furi_hal_i2c_master_trx_blocking(
    const FuriHalI2cBusHandle* handle,
    uint8_t device_address,
    const uint8_t* tx_buffer,
    size_t tx_size,
    uint8_t* rx_buffer,
    size_t rx_size,
    uint32_t timeout_us) {
    int status = furi_hal_i2c_master_tx_blocking_nostop(handle, device_address, tx_buffer, tx_size, timeout_us);
    if(status < 0) {
        return status;
    }
    return furi_hal_i2c_master_rx_blocking(handle, device_address, rx_buffer, rx_size, timeout_us);
}

bool furi_hal_i2c_device_ready(const FuriHalI2cBusHandle* handle, uint8_t device_address, uint32_t timeout_us) {
    int ret;
    uint8_t rxdata = 0;
    if((device_address & 0x78) == 0 || (device_address & 0x78) == 0x78)
        ret = PICO_ERROR_GENERIC;
    else
        ret = furi_hal_i2c_master_tx_blocking(handle, device_address, &rxdata, 1, timeout_us);

    return ret < PICO_OK ? false : true;
}

const char* furi_hal_i2c_bus_name(const FuriHalI2cBusHandle* handle) {
    furi_hal_i2c_check_handle_is_acquired_master(handle);
    return handle->bus->name;
}

void furi_hal_i2c_slave_set_callback(const FuriHalI2cBusHandle* handle, FuriHalI2cBusSlaveCallback callback, void* context) {
    furi_hal_i2c_check_handle_is_acquired_slave(handle);
    furi_check(handle->bus->api.slave.callback == NULL);
    furi_check(callback);
    handle->bus->api.slave.callback = callback;
    handle->bus->api.slave.context = context;
}

uint8_t furi_hal_i2c_slave_write_blocking(const FuriHalI2cBusHandle* handle, uint8_t* data, size_t size) {
    furi_hal_i2c_check_handle_is_acquired_slave(handle);
    return handle->bus->api.slave.write_blocking(handle, data, size);
}

uint8_t furi_hal_i2c_slave_read_blocking(const FuriHalI2cBusHandle* handle, uint8_t* data, size_t size) {
    furi_hal_i2c_check_handle_is_acquired_slave(handle);
    return handle->bus->api.slave.read_blocking(handle, data, size);
}
