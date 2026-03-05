#pragma once

#include <furi_hal_i2c_types.h>
#include <pico/error.h>

#define FURI_HAL_I2C_TIMEOUT_US 1000 * 1000 // 1 second

#ifdef __cplusplus
extern "C" {
#endif

void furi_hal_i2c_init_control(void);
void furi_hal_i2c_deinit_control(void);
void furi_hal_i2c_init_main(void);
void furi_hal_i2c_deinit_main(void);
void furi_hal_i2c_init_cpu(void);
void furi_hal_i2c_deinit_cpu(void);

void furi_hal_i2c_acquire(const FuriHalI2cBusHandle* handle);
void furi_hal_i2c_release(const FuriHalI2cBusHandle* handle);

bool furi_hal_i2c_device_ready(const FuriHalI2cBusHandle* handle, uint8_t device_address, uint32_t timeout_us);
int furi_hal_i2c_master_tx_blocking(const FuriHalI2cBusHandle* handle, uint8_t device_address, const uint8_t* tx_buffer, size_t size, uint32_t timeout_us);
int furi_hal_i2c_master_tx_blocking_nostop(const FuriHalI2cBusHandle* handle, uint8_t device_address, const uint8_t* tx_buffer, size_t size, uint32_t timeout_us);
int furi_hal_i2c_master_rx_blocking(const FuriHalI2cBusHandle* handle, uint8_t device_address, uint8_t* rx_buffer, size_t size, uint32_t timeout_us);
int furi_hal_i2c_master_rx_blocking_nostop(const FuriHalI2cBusHandle* handle, uint8_t device_address, uint8_t* rx_buffer, size_t size, uint32_t timeout_us);
int furi_hal_i2c_master_trx_blocking(
    const FuriHalI2cBusHandle* handle,
    uint8_t device_address,
    const uint8_t* tx_buffer,
    size_t tx_size,
    uint8_t* rx_buffer,
    size_t rx_size,
    uint32_t timeout_us);

const char* furi_hal_i2c_bus_name(const FuriHalI2cBusHandle* handle);

void furi_hal_i2c_slave_set_callback(const FuriHalI2cBusHandle* handle, FuriHalI2cBusSlaveCallback callback, void* context);
void furi_hal_i2c_slave_write_blocking(const FuriHalI2cBusHandle* handle, uint8_t data);
uint8_t furi_hal_i2c_slave_read_blocking(const FuriHalI2cBusHandle* handle);

#ifdef __cplusplus
}
#endif
