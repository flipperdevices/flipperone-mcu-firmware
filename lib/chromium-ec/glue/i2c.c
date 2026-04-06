#include <furi.h>
#include <furi_hal_i2c.h>
#include <furi_hal_i2c_config.h>
#include "i2c.h"
#include "common.h"

const FuriHalI2cBusHandle* i2c = &furi_hal_i2c_handle_main;

#define TAG "I2CGlue"
#define I2C_GLUE_DEBUG

#ifdef I2C_GLUE_DEBUG
#define I2C_GLUE_LOG(...) FURI_LOG_I(TAG, __VA_ARGS__)
#define I2C_GLUE_ERR(...) FURI_LOG_E(TAG, __VA_ARGS__)
#else
#define I2C_GLUE_LOG(...)
#define I2C_GLUE_ERR(...)
#endif

// TODO: checking of port

void i2c_lock(int port, int lock) {
    UNUSED(port);

    if(lock) {
        furi_hal_i2c_acquire(i2c);
    } else {
        furi_hal_i2c_release(i2c);
    }
}

int i2c_write8(const int port, const uint16_t addr_flags, int offset, int data) {
    UNUSED(port);

    furi_hal_i2c_acquire(i2c);
    int ret = furi_hal_i2c_master_tx_blocking(i2c, addr_flags, (uint8_t*)&offset, 1, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(i2c);

    if(ret != 0) {
        I2C_GLUE_ERR("Write 8-bit failed, 0x%02X to device 0x%02X, error: %d", offset, addr_flags, ret);
        return EC_ERROR_UNKNOWN;
    }
    return EC_SUCCESS;
}

int i2c_write16(const int port, const uint16_t addr_flags, int offset, int data) {
    UNUSED(port);

    uint8_t buf[3];
    buf[0] = offset;
    buf[1] = data & 0xFF;
    buf[2] = (data >> 8) & 0xFF;

    furi_hal_i2c_acquire(i2c);
    int ret = furi_hal_i2c_master_tx_blocking(i2c, addr_flags, buf, 3, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(i2c);

    if(ret != 0) {
        I2C_GLUE_ERR("Write 16-bit failed, 0x%02X to device 0x%02X, error: %d", offset, addr_flags, ret);
        return EC_ERROR_UNKNOWN;
    }
    return EC_SUCCESS;
}

int i2c_read8(const int port, const uint16_t addr_flags, int offset, int* data) {
    UNUSED(port);

    uint8_t rxdata;
    furi_hal_i2c_acquire(i2c);
    int ret = furi_hal_i2c_master_trx_blocking(i2c, addr_flags, (uint8_t*)&offset, 1, &rxdata, 1, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(i2c);

    if(ret != 0) {
        I2C_GLUE_ERR("Read 8-bit failed, 0x%02X from device 0x%02X, error: %d", offset, addr_flags, ret);
        return EC_ERROR_UNKNOWN;
    }

    *data = rxdata;
    return EC_SUCCESS;
}

int i2c_read16(const int port, const uint16_t addr_flags, int offset, int* data) {
    UNUSED(port);

    uint8_t rxdata[2];
    furi_hal_i2c_acquire(i2c);
    int ret = furi_hal_i2c_master_trx_blocking(i2c, addr_flags, (uint8_t*)&offset, 1, rxdata, 2, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(i2c);

    if(ret != 0) {
        I2C_GLUE_ERR("Read 16-bit failed, 0x%02X from device 0x%02X, error: %d", offset, addr_flags, ret);
        return EC_ERROR_UNKNOWN;
    }

    *data = rxdata[0] | (rxdata[1] << 8);
    return EC_SUCCESS;
}

int i2c_xfer(const int port, const uint16_t addr_flags, const uint8_t* out, int out_size, uint8_t* in, int in_size) {
    UNUSED(port);

    furi_hal_i2c_acquire(i2c);
    int ret = furi_hal_i2c_master_trx_blocking(i2c, addr_flags, out, out_size, in, in_size, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(i2c);

    if(ret != 0) {
        I2C_GLUE_ERR("I2C transfer failed to device 0x%02X, error: %d", addr_flags, ret);
        return EC_ERROR_UNKNOWN;
    }
    return EC_SUCCESS;
}

int i2c_xfer_unlocked(const int port, const uint16_t addr_flags, const uint8_t* out, int out_size, uint8_t* in, int in_size, int flags) {
    bool has_start_bit = flags & I2C_XFER_START;
    bool has_stop_bit = flags & I2C_XFER_STOP;
    int ret;

    if(has_start_bit && has_stop_bit) {
        return i2c_xfer(port, addr_flags, out, out_size, in, in_size);
    }

    if(has_start_bit) {
        ret = furi_hal_i2c_master_tx_blocking_nostop(i2c, addr_flags, out, out_size, FURI_HAL_I2C_TIMEOUT_US);
        if(ret != 0) {
            I2C_GLUE_ERR("I2C transfer failed to device 0x%02X, error: %d", addr_flags, ret);
            return EC_ERROR_UNKNOWN;
        }

        ret = furi_hal_i2c_master_rx_blocking_nostop(i2c, addr_flags, in, in_size, FURI_HAL_I2C_TIMEOUT_US);
        if(ret != 0) {
            I2C_GLUE_ERR("I2C transfer failed to device 0x%02X, error: %d", addr_flags, ret);
            return EC_ERROR_UNKNOWN;
        }
    }

    if(has_stop_bit) {
        return i2c_xfer(port, addr_flags, out, out_size, in, in_size);
    }

    return EC_SUCCESS;
}
