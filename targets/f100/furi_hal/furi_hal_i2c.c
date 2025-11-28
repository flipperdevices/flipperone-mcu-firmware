#include <furi_hal_i2c.h>
#include <furi_hal_i2c_config.h>
#include <furi_hal.h>
#include <furi_hal_power.h>

#define TAG "FuriHalI2c"

void furi_hal_i2c_init_internal(void) {
    furi_hal_i2c_bus_internal.callback(&furi_hal_i2c_bus_internal, FuriHalI2cBusEventInit);
}

void furi_hal_i2c_deinit_internal(void) {
    furi_hal_i2c_bus_internal.callback(&furi_hal_i2c_bus_internal, FuriHalI2cBusEventDeinit);
}

void furi_hal_i2c_init_external(void) {
    furi_hal_i2c_bus_external.callback(&furi_hal_i2c_bus_external, FuriHalI2cBusEventInit);
}

void furi_hal_i2c_deinit_external(void) {
    furi_hal_i2c_bus_external.callback(&furi_hal_i2c_bus_external, FuriHalI2cBusEventDeinit);
}

void furi_hal_i2c_acquire(const FuriHalI2cBusHandle* handle) {
    furi_hal_power_insomnia_enter();
    // Lock bus access
    handle->bus->callback(handle->bus, FuriHalI2cBusEventLock);
    // Ensure that no active handle set
    furi_check(handle->bus->current_handle == NULL);
    // Set current handle
    handle->bus->current_handle = handle;
    // Activate bus
    handle->bus->callback(handle->bus, FuriHalI2cBusEventActivate);
    // Activate handle
    handle->callback(handle, FuriHalI2cBusHandleEventActivate);
}

void furi_hal_i2c_release(const FuriHalI2cBusHandle* handle) {
    // Ensure that current handle is our handle
    furi_check(handle->bus->current_handle == handle);
    // Deactivate handle
    handle->callback(handle, FuriHalI2cBusHandleEventDeactivate);
    // Deactivate bus
    handle->bus->callback(handle->bus, FuriHalI2cBusEventDeactivate);
    // Reset current handle
    handle->bus->current_handle = NULL;
    // Unlock bus
    handle->bus->callback(handle->bus, FuriHalI2cBusEventUnlock);
    furi_hal_power_insomnia_exit();
}

int furi_hal_i2c_master_tx_blocking(const FuriHalI2cBusHandle* handle, uint8_t device_address, const uint8_t* tx_buffer, size_t size, uint32_t timeout_us) {
    furi_check(handle);

    return i2c_write_blocking_until(handle->bus->i2c, device_address, tx_buffer, size, false, make_timeout_time_us(timeout_us));
}

int furi_hal_i2c_master_tx_blocking_nostop(const FuriHalI2cBusHandle* handle, uint8_t device_address, const uint8_t* tx_buffer, size_t size, uint32_t timeout_us) {
    furi_check(handle);

    return i2c_write_blocking_until(handle->bus->i2c, device_address, tx_buffer, size, true, make_timeout_time_us(timeout_us));
}

int furi_hal_i2c_master_rx_blocking(const FuriHalI2cBusHandle* handle, uint8_t device_address, uint8_t* rx_buffer, size_t size, uint32_t timeout_us) {
    furi_check(handle);

    return i2c_read_blocking_until(handle->bus->i2c, device_address, rx_buffer, size, false, make_timeout_time_us(timeout_us));
}

int furi_hal_i2c_master_rx_blocking_nostop(const FuriHalI2cBusHandle* handle, uint8_t device_address, uint8_t* rx_buffer, size_t size, uint32_t timeout_us) {
    furi_check(handle);

    return i2c_read_blocking_until(handle->bus->i2c, device_address, rx_buffer, size, true, make_timeout_time_us(timeout_us));
}

int furi_hal_i2c_master_trx_blocking(
    const FuriHalI2cBusHandle* handle,
    uint8_t device_address,
    const uint8_t* tx_buffer,
    size_t tx_size,
    uint8_t* rx_buffer,
    size_t rx_size,
    uint32_t timeout_us) {
    furi_check(handle);

    int status = i2c_write_blocking_until(handle->bus->i2c, device_address, tx_buffer, tx_size, true, make_timeout_time_us(timeout_us));
    if(status != PICO_OK) {
        return status;
    }
    return i2c_read_blocking_until(handle->bus->i2c, device_address, rx_buffer, rx_size, false, make_timeout_time_us(timeout_us));
}

bool furi_hal_i2c_device_ready(const FuriHalI2cBusHandle* handle, uint8_t device_address, uint32_t timeout_us) {
    furi_check(handle);

    int ret;
    uint8_t rxdata = 0;
    if((device_address & 0x78) == 0 || (device_address & 0x78) == 0x78)
        ret = PICO_ERROR_GENERIC;
    else
        ret = furi_hal_i2c_master_tx_blocking(handle, device_address, &rxdata, 1, FURI_HAL_I2C_TIMEOUT_US);

    return ret < PICO_OK ? false : true;
}

void furi_hal_i2c_bus_scan_print(const FuriHalI2cBusHandle* handle) {
    furi_check(handle);

    FURI_LOG_I(TAG, "I2C Bus Scan on I2C%d:", i2c_get_index(handle->bus->i2c));
    FURI_LOG_I(TAG, "\t     0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F");
    FURI_LOG_I(TAG, "\t   -----------------------------------------------");

    for(uint8_t addr = 0; addr < 128; addr++) {
        if(addr % 16 == 0) {
            FURI_LOG_RAW_I("\t\t\t%02x | ", addr);
        }

        // Perform a 1-byte dummy read from the probe address. If a slave
        // acknowledges this address, the function returns the number of bytes
        // transferred. If the address byte is ignored, the function returns
        // -1.

        // Skip over any reserved addresses.
        bool ret = furi_hal_i2c_device_ready(handle, addr, FURI_HAL_I2C_TIMEOUT_US);
        FURI_LOG_RAW_I(ret ? "@" : ".");
        FURI_LOG_RAW_I(addr % 16 == 15 ? "\n" : "  ");
    }
}
