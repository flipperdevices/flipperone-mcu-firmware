#include "bq28z620_reg.h"
#include "bq28z620.h"
#include <furi.h>

#include <furi_hal_i2c.h>
#include <pico/error.h>
#include <pico/types.h>

#define TAG "Bq28z620"

#ifdef BQ28Z620_DEBUG_ENABLE
#define BQ28Z620_DEBUG(...) FURI_LOG_D(__VA_ARGS__)
#else
#define BQ28Z620_DEBUG(...)
#endif

struct Bq28z620 {
    const FuriHalI2cBusHandle* i2c_handle;
    uint8_t address;
    void* context;
};

static Bq28z620Status bq28z620_check_status(int stataus) {
    Bq28z620Status ret = Bq28z620StatusUnknown;
    if(stataus >= PICO_OK) {
        ret = Bq28z620StatusOk;
    } else if(stataus == PICO_ERROR_GENERIC) {
        ret = Bq28z620StatusError;
    } else if(stataus == PICO_ERROR_TIMEOUT) {
        ret = Bq28z620StatusTimeout;
    } else {
        ret = Bq28z620StatusUnknown;
    }

    return ret;
}

static Bq28z620Status bq28z620_write_reg(Bq28z620* instance, Bq28z620Reg reg, uint8_t data) {
    furi_check(instance);

    uint8_t buffer[2] = {reg, data};

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_master_tx_blocking(instance->i2c_handle, instance->address, buffer, sizeof(buffer), FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
        FURI_LOG_E(TAG, "Failed to write reg 0x%02X", reg);
    } else {
        BQ28Z620_DEBUG(TAG, "Wrote reg 0x%02X: %08b", reg, data);
    }

    return bq28z620_check_status(ret);
}

static Bq28z620Status bq28z620_read_reg(Bq28z620* instance, Bq28z620Reg reg, uint8_t* data) {
    furi_check(instance);
    furi_check(data);

    furi_hal_i2c_acquire(instance->i2c_handle);

    int ret = furi_hal_i2c_master_trx_blocking(instance->i2c_handle, instance->address, (uint8_t*)&reg, 1, data, 1, FURI_HAL_I2C_TIMEOUT_US);
    if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
        FURI_LOG_E(TAG, "Failed to read reg 0x%02X", reg);
    } else {
        BQ28Z620_DEBUG(TAG, "Read reg 0x%02X: %08b", reg, *data);
    }
    furi_hal_i2c_release(instance->i2c_handle);

    return bq28z620_check_status(ret);
}

static Bq28z620Status bq28z620_write_buf(Bq28z620* instance, Bq28z620Reg reg, uint8_t* data, size_t length) {
    furi_check(instance);
    furi_check(data);
    furi_hal_i2c_acquire(instance->i2c_handle);

    int ret = furi_hal_i2c_master_trx_blocking(instance->i2c_handle, instance->address, (uint8_t*)&reg, 1, data, length, FURI_HAL_I2C_TIMEOUT_US);
    if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
        FURI_LOG_E(TAG, "Failed to write reg 0x%02X", reg);
    } else {
        BQ28Z620_DEBUG(TAG, "Wrote reg 0x%02X: %d bytes", reg, length);
    }
    furi_hal_i2c_release(instance->i2c_handle);

    return bq28z620_check_status(ret);
}

static Bq28z620Status bq28z620_read_buf(Bq28z620* instance, Bq28z620Reg reg, uint8_t* data, size_t length) {
    furi_check(instance);
    furi_check(data);

    furi_hal_i2c_acquire(instance->i2c_handle);

    int ret = furi_hal_i2c_master_trx_blocking(instance->i2c_handle, instance->address, (uint8_t*)&reg, 1, data, length, FURI_HAL_I2C_TIMEOUT_US);
    if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
        FURI_LOG_E(TAG, "Failed to read reg 0x%02X", reg);
    } else {
        BQ28Z620_DEBUG(TAG, "Read reg 0x%02X: %d bytes", reg, length);
    }
    furi_hal_i2c_release(instance->i2c_handle);

    return bq28z620_check_status(ret);
}



Bq28z620* bq28z620_init(const FuriHalI2cBusHandle* i2c_handle, uint8_t address) {
    Bq28z620* instance = (Bq28z620*)malloc(sizeof(Bq28z620));
    instance->i2c_handle = i2c_handle;
    instance->address = address;

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_device_ready(instance->i2c_handle, instance->address, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret) {
        FURI_LOG_I(TAG, "BQ28Z620 device ready at address 0x%02X", instance->address);
        

    } else {
        FURI_LOG_E(TAG, "BQ28Z620 device not ready at address 0x%02X", instance->address);
        free(instance);
        return NULL;
    }

    return instance;
}

void bq28z620_deinit(Bq28z620* instance) {
    furi_check(instance);

    free(instance);
}