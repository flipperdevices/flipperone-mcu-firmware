#include "bq25792_reg.h"
#include "bq25792.h"
#include <furi.h>

#include <furi_hal_i2c.h>

#define TAG "Bq25792"

#define BQ25792_DEVICE_PART_NUMBER 0b001 //BQ25792
#define BQ25792_DEVICE_REVISION    0b000 //Revision

#define BQ25792_DEBUG_ENABLE
#ifdef BQ25792_DEBUG_ENABLE
#define BQ25792_DEBUG(...) FURI_LOG_D(__VA_ARGS__)
#else
#define BQ25792_DEBUG(...)
#endif

struct Bq25792 {
    const FuriHalI2cBusHandle* i2c_handle;
    uint8_t address;
    const GpioPin* pin_interrupt;
    Bq25792CallbackInput callback;
    void* context;
};

static __isr __not_in_flash_func(void) bq25792_interrupt_handler(void* ctx) {
    Bq25792* instance = (Bq25792*)ctx;
    if(instance->callback) {
        instance->callback(instance->context);
    }
}

static Bq25792Status bq25792_check_status(int status) {
    Bq25792Status ret = Bq25792StatusUnknown;
    if(status >= PICO_OK) {
        ret = Bq25792StatusOk;
    } else if(status == PICO_ERROR_GENERIC) {
        ret = Bq25792StatusError;
    } else if(status == PICO_ERROR_TIMEOUT) {
        ret = Bq25792StatusTimeout;
    } else {
        ret = Bq25792StatusUnknown;
    }
    return ret;
}

static Bq25792Status bq25792_write_reg8(Bq25792* instance, Bq25792Reg reg, uint8_t data) {
    furi_check(instance);

    uint8_t buffer[2] = {reg, data};

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_master_tx_blocking(instance->i2c_handle, instance->address, buffer, sizeof(buffer), FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
        FURI_LOG_E(TAG, "Failed to write reg 0x%02X", reg);
    } else {
        BQ25792_DEBUG(TAG, "Wrote reg 0x%02X: %08b", reg, data);
    }

    return bq25792_check_status(ret);
}

// static Bq25792Status bq25792_write_reg16(Bq25792* instance, Bq25792Reg reg, uint16_t data) {
//     furi_check(instance);

//     uint8_t buffer[3] = {reg, data >> 8, data & 0xFF};

//     furi_hal_i2c_acquire(instance->i2c_handle);
//     int ret = furi_hal_i2c_master_tx_blocking(instance->i2c_handle, instance->address, buffer, sizeof(buffer), FURI_HAL_I2C_TIMEOUT_US);
//     furi_hal_i2c_release(instance->i2c_handle);

//     if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
//         FURI_LOG_E(TAG, "Failed to write reg 0x%02X", reg);
//     } else {
//         BQ25792_DEBUG(TAG, "Wrote reg 0x%02X: %016b", reg, data);
//     }

//     return bq25792_check_status(ret);
// }

static Bq25792Status bq25792_read_reg8(Bq25792* instance, Bq25792Reg reg, uint8_t* data) {
    furi_check(instance);
    furi_check(data);

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_master_tx_blocking(instance->i2c_handle, instance->address, (uint8_t*)&reg, 1, FURI_HAL_I2C_TIMEOUT_US);
    if(!(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT)) {
        uint8_t buffer[2] = {0};
        ret = furi_hal_i2c_master_rx_blocking(instance->i2c_handle, instance->address, buffer, sizeof(buffer), FURI_HAL_I2C_TIMEOUT_US);
        if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
            FURI_LOG_E(TAG, "Failed to read reg 0x%02X", reg);
        } else {
            *data = buffer[0];
        }
    } else {
        FURI_LOG_E(TAG, "Failed to write reg address 0x%02X for reading", reg);
    }
    furi_hal_i2c_release(instance->i2c_handle);

    return bq25792_check_status(ret);
}

// static Bq25792Status bq25792_read_reg16(Bq25792* instance, Bq25792Reg reg, uint16_t* data) {
//     furi_check(instance);
//     furi_check(data);

//     furi_hal_i2c_acquire(instance->i2c_handle);
//     int ret = furi_hal_i2c_master_tx_blocking(instance->i2c_handle, instance->address, (uint8_t*)&reg, 1, FURI_HAL_I2C_TIMEOUT_US);
//     if(!(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT)) {
//         uint8_t buffer[2] = {0};
//         ret = furi_hal_i2c_master_rx_blocking(instance->i2c_handle, instance->address, buffer, sizeof(buffer), FURI_HAL_I2C_TIMEOUT_US);
//         if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
//             FURI_LOG_E(TAG, "Failed to read reg 0x%02X", reg);
//         } else {
//             *data = (buffer[0] << 8) | buffer[1];
//         }
//     } else {
//         FURI_LOG_E(TAG, "Failed to write reg address 0x%02X for reading", reg);
//     }
//     furi_hal_i2c_release(instance->i2c_handle);

//     return bq25792_check_status(ret);
// }

static Bq25792Status bq25792_load_config(Bq25792* instance) {
    furi_check(instance);
    Bq25792Status res = Bq25792StatusUnknown;
    do {
        Bq25792TerminationControlRegBits termination_control = {.reg_rst = 1}; // Reset all registers to default values
        res = bq25792_write_reg8(instance, Bq25792RegTerminationControl, *(uint8_t*)&termination_control);
        if(res != Bq25792StatusOk) {
            break;
        }

        Bq25792AdcControlRegBits adc_control = {0};
        res = bq25792_read_reg8(instance, Bq25792RegADCControl, (uint8_t*)&adc_control);
        if(res != Bq25792StatusOk) {
            break;
        }
        adc_control.adc_en = 1; // Enable ADC
        res = bq25792_write_reg8(instance, Bq25792RegADCControl, *(uint8_t*)&adc_control);
        if(res != Bq25792StatusOk) {
            break;
        }
        Bq25792ChargerControl5RegBits charger_control_5 = {0};
        bq25792_read_reg8(instance, Bq25792RegChargerControl5, (uint8_t*)&charger_control_5);
        if(res != Bq25792StatusOk) {
            break;
        }
        charger_control_5.sfet_present = 1; // Enable Sfet presence detection
        res = bq25792_write_reg8(instance, Bq25792RegChargerControl5, *(uint8_t*)&charger_control_5);
    } while(0);
    if(res != Bq25792StatusOk) {
        FURI_LOG_E(TAG, "Failed to load config!");
    }
    return res;
}

Bq25792* bq25792_init(const FuriHalI2cBusHandle* i2c_handle, uint8_t address, const GpioPin* pin_interrupt) {
    Bq25792* instance = (Bq25792*)malloc(sizeof(Bq25792));
    instance->i2c_handle = i2c_handle;
    instance->address = address;

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_device_ready(instance->i2c_handle, instance->address, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret) {
        FURI_LOG_I(TAG, "BQ25792 device ready at address 0x%02X", instance->address);
        if(instance->pin_interrupt) {
            furi_hal_gpio_init_simple(instance->pin_interrupt, GpioModeInput);
            furi_hal_gpio_add_int_callback(instance->pin_interrupt, GpioConditionFall, bq25792_interrupt_handler, instance);
        }

        Bq25792PartInformationRegBits device_info = {0};
        bq25792_read_reg8(instance, Bq25792RegPartInformation, (uint8_t*)&device_info);
        if(device_info.pn != BQ25792_DEVICE_PART_NUMBER || device_info.dev_rev != BQ25792_DEVICE_REVISION) {
            furi_crash("BQ25792 device ID mismatch!");
        }

        if(bq25792_load_config(instance) != Bq25792StatusOk) {
            furi_crash("BQ25792 failed to load config");
        }

    } else {
        FURI_LOG_E(TAG, "BQ25792 device not ready at address 0x%02X", instance->address);
        free(instance);
        return NULL;
    }

    return instance;
}

void bq25792_deinit(Bq25792* instance) {
    furi_check(instance);
    if(instance->pin_interrupt) {
        furi_hal_gpio_remove_int_callback(instance->pin_interrupt);
        furi_hal_gpio_init_ex(instance->pin_interrupt, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    }
    free(instance);
}

Bq25792Status bq25792_set_power_switch(Bq25792* instance, Bq25792PowerSwitch power_switch) {
    furi_check(instance);
    Bq25792Status res = Bq25792StatusUnknown;
    do {
        Bq25792ChargerControl2RegBits charger_control_2 = {0};
        bq25792_read_reg8(instance, Bq25792RegChargerControl2, (uint8_t*)&charger_control_2);
        if(res != Bq25792StatusOk) {
            break;
        }
        charger_control_2.sdrv_ctrl = power_switch; // Set power switch
        res = bq25792_write_reg8(instance, Bq25792RegChargerControl2, *(uint8_t*)&charger_control_2);
    } while(0);
    if(res != Bq25792StatusOk) {
        FURI_LOG_E(TAG, "Failed to set power switch!");
    }
    return res;
}
