/**
 * @file furi_hal_i2c.h I2C HAL API
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include <furi_hal_i2c_types.h>
#include <pico/error.h>

#define FURI_HAL_I2C_TIMEOUT_US 1000 * 1000 // 1 second

#ifdef __cplusplus
extern "C" {
#endif

/** Acquire I2C bus handle
 *
 * @param      handle  Pointer to FuriHalI2cBusHandle instance
 */
void furi_hal_i2c_acquire(FuriHalI2cHandle* handle);

/** Release I2C bus handle
 * 
 * @param      handle  Pointer to FuriHalI2cBusHandle instance acquired in
 *                     `furi_hal_i2c_acquire`
 */
void furi_hal_i2c_release(FuriHalI2cHandle* handle);

void furi_hal_i2c_master_init(FuriHalI2cHandle* handle, uint32_t baud_rate);
void furi_hal_i2c_deinit(FuriHalI2cHandle* handle);
void furi_hal_i2c_bus_scan_print(FuriHalI2cHandle* handle);
int furi_hal_i2c_master_tx_blocking(FuriHalI2cHandle* handle, uint8_t device_address, const uint8_t* tx_buffer, size_t size, uint32_t timeout_us);
int furi_hal_i2c_master_rx_blocking(FuriHalI2cHandle* handle, uint8_t device_address, uint8_t* rx_buffer, size_t size, uint32_t timeout_us);
int furi_hal_i2c_master_trx_blocking(
    FuriHalI2cHandle* handle,
    uint8_t device_address,
    const uint8_t* tx_buffer,
    size_t tx_size,
    uint8_t* rx_buffer,
    size_t rx_size,
    uint32_t timeout_us);

#ifdef __cplusplus
}
#endif
