
#include "core/log.h"
#include <furi_hal_i2c.h>
#include <furi_hal_i2c_types_i.h>
#include <furi_hal.h>
#include <furi_hal_gpio.h>
#include "furi_hal_resources.h"
#include <furi_hal_power.h>

#include "hardware/i2c.h"
#include <pico/error.h>
#include <stdbool.h>
#include <stdint.h>

#define TAG "FuriHalI2c"

typedef struct {
    FuriHalI2cHandle* handle;
    i2c_inst_t* periph_ptr;
    // FuriHalI2cRxCallback rx_callback;
    // FuriHalI2cTxCallback tx_callback;
    void* callback_context;
} FuriHalI2c;

typedef struct {
    i2c_inst_t* periph;
    GpioAltFn alt_fn;
    const GpioPin* gpio[FuriHalI2cPinMax];
} FuriHalI2cResources;

static const FuriHalI2cResources furi_hal_i2c_resources[FuriHalI2cIdMax] = {
    {
        .periph = i2c0,
        .alt_fn = GpioAltFn3I2c,
        .gpio =
            {
                &gpio_i2c0_sda, // SDA
                &gpio_i2c0_scl, // SCL
            },
    },
    {
        .periph = i2c1,
        .alt_fn = GpioAltFn3I2c,
        .gpio =
            {
                &gpio_i2c1_sda, // SDA
                &gpio_i2c1_scl, // SCL
            },
    },
};

static FuriHalI2c* furi_hal_i2c[FuriHalI2cIdMax];

void furi_hal_i2c_acquire(FuriHalI2cHandle* handle) {
    //Todo: add lock
    furi_hal_power_insomnia_enter();
}

void furi_hal_i2c_release(FuriHalI2cHandle* handle) {
    //Todo: add unlock
    furi_hal_power_insomnia_exit();
}

void furi_hal_i2c_master_init(FuriHalI2cHandle* handle, uint32_t baud_rate) {
    furi_check(handle);
    furi_check(baud_rate <= 1000000); // Max 1MHz for standard mode

    const FuriHalI2cId i2c_id = handle->id;
    furi_check(furi_hal_i2c[i2c_id] == NULL);

    furi_hal_i2c[i2c_id] = malloc(sizeof(FuriHalI2c));

    FuriHalI2c* i2c = furi_hal_i2c[i2c_id];
    i2c_inst_t* periph = furi_hal_i2c_resources[i2c_id].periph;
    i2c->periph_ptr = periph;
    i2c->handle = handle;

    // Initialize I2C peripheral
    uint32_t baud_rate_actual = i2c_init(periph, baud_rate);

    // Initialize GPIOs
    for(size_t i = 0; i < FuriHalI2cPinMax; i++) {
        const GpioPin* gpio = furi_hal_i2c_resources[i2c_id].gpio[i];
        if(gpio != NULL) {
            furi_hal_gpio_init_ex(gpio, GpioModeOutputPushPull, GpioPullNo, GpioSpeedFast, furi_hal_i2c_resources[i2c_id].alt_fn);
            furi_hal_gpio_set_drive_strength(gpio, GpioDriveStrengthMedium);
        }
    }

    FURI_LOG_D(TAG, "I2C %d initialized: baud_rate=%lu", i2c_id, baud_rate_actual);
}

void furi_hal_i2c_deinit(FuriHalI2cHandle* handle) {
    furi_check(handle);
    const FuriHalI2cId i2c_id = handle->id;
    furi_check(furi_hal_i2c[i2c_id]);

    FuriHalI2c* i2c = furi_hal_i2c[i2c_id];
    i2c_inst_t* periph = i2c->periph_ptr;

    // Deinitialize I2C peripheral
    i2c_deinit(periph);

    // Deinitialize GPIOs
    for(size_t i = 0; i < FuriHalI2cPinMax; i++) {
        const GpioPin* gpio = furi_hal_i2c_resources[i2c_id].gpio[i];
        if(gpio != NULL) {
            furi_hal_gpio_init_ex(gpio, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
        }
    }

    free(i2c);
    furi_hal_i2c[i2c_id] = NULL;

    FURI_LOG_D(TAG, "I2C %d deinitialized", i2c_id);
}

int furi_hal_i2c_master_tx_blocking(FuriHalI2cHandle* handle, uint8_t device_address, const uint8_t* tx_buffer, size_t size, uint32_t timeout_us) {
    furi_check(handle);
    const FuriHalI2cId i2c_id = handle->id;
    furi_check(furi_hal_i2c[i2c_id]);

    FuriHalI2c* i2c = furi_hal_i2c[i2c_id];
    i2c_inst_t* periph = i2c->periph_ptr;

    return i2c_write_blocking_until(periph, device_address, tx_buffer, size, false, make_timeout_time_us(timeout_us));
}

int furi_hal_i2c_master_rx_blocking(FuriHalI2cHandle* handle, uint8_t device_address, uint8_t* rx_buffer, size_t size, uint32_t timeout_us) {
    furi_check(handle);
    const FuriHalI2cId i2c_id = handle->id;
    furi_check(furi_hal_i2c[i2c_id]);

    FuriHalI2c* i2c = furi_hal_i2c[i2c_id];
    i2c_inst_t* periph = i2c->periph_ptr;

    return i2c_read_blocking_until(periph, device_address, rx_buffer, size, false, make_timeout_time_us(timeout_us));
}

int furi_hal_i2c_master_trx_blocking(
    FuriHalI2cHandle* handle,
    uint8_t device_address,
    const uint8_t* tx_buffer,
    size_t tx_size,
    uint8_t* rx_buffer,
    size_t rx_size,
    uint32_t timeout_us) {
    furi_check(handle);
    const FuriHalI2cId i2c_id = handle->id;
    furi_check(furi_hal_i2c[i2c_id]);

    FuriHalI2c* i2c = furi_hal_i2c[i2c_id];
    i2c_inst_t* periph = i2c->periph_ptr;

    int status = i2c_write_blocking_until(periph, device_address, tx_buffer, tx_size, true, make_timeout_time_us(timeout_us));
    if(status != PICO_OK) {
        return status;
    }
    return i2c_read_blocking_until(periph, device_address, rx_buffer, rx_size, false, make_timeout_time_us(timeout_us));
}

bool furi_hal_i2c_device_ready(FuriHalI2cHandle* handle, uint8_t device_address, uint32_t timeout_us) {
    furi_check(handle);
    const FuriHalI2cId i2c_id = handle->id;
    furi_check(furi_hal_i2c[i2c_id]);

    FuriHalI2c* i2c = furi_hal_i2c[i2c_id];
    i2c_inst_t* periph = i2c->periph_ptr;

    int ret;
    uint8_t rxdata = 0;
    if((device_address & 0x78) == 0 || (device_address & 0x78) == 0x78)
        ret = PICO_ERROR_GENERIC;
    else
        ret = furi_hal_i2c_master_tx_blocking(handle, device_address, &rxdata, 1, FURI_HAL_I2C_TIMEOUT_US);

    return ret < PICO_OK ? false : true;
}

void furi_hal_i2c_bus_scan_print(FuriHalI2cHandle* handle) {
    furi_check(handle);
    const FuriHalI2cId i2c_id = handle->id;
    furi_check(furi_hal_i2c[i2c_id]);

    FURI_LOG_I(TAG, "I2C Bus Scan on I2C%d:", i2c_id);
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
