#include "bq25792_reg.h"
#include "bq25792.h"
#include <furi.h>

#include <furi_hal_i2c.h>

#define TAG "Bq25792"

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

static Bq25792Status bq25792_read_reg16(Bq25792* instance, Bq25792Reg reg, uint16_t* data) {
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
            *data = (buffer[0] << 8) | buffer[1];
        }
    } else {
        FURI_LOG_E(TAG, "Failed to write reg address 0x%02X for reading", reg);
    }
    furi_hal_i2c_release(instance->i2c_handle);

    return bq25792_check_status(ret);
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
        BQ25792_DEBUG(TAG, "Device ID: %02X, Revision ID: %02X", device_info.pn, device_info.dev_rev);

        Bq25792AdcControlRegBits adc_control = {0};
        bq25792_read_reg8(instance, Bq25792RegADCControl, (uint8_t*)&adc_control);
        adc_control.adc_en = 1;
        bq25792_write_reg8(instance, Bq25792RegADCControl, *(uint8_t*)&adc_control);
        furi_delay_ms(100);

        for(int i = 0; i < 10; i++) {
            furi_delay_ms(1000);

            Bq25792VsysAdcRegBits vsys_adc = {0};
            bq25792_read_reg16(instance, Bq25792RegVSYSADC, (uint16_t*)&vsys_adc);
            BQ25792_DEBUG(TAG, "VSYS ADC: %0.4f V", (float)vsys_adc.vsys_adc / 1000.0f);

            Bq25792VbatAdcRegBits vbat_adc = {0};
            bq25792_read_reg16(instance, Bq25792RegVBATADC, (uint16_t*)&vbat_adc);
            BQ25792_DEBUG(TAG, "VBAT ADC: %0.4f V", (float)vbat_adc.vbat_adc / 1000.0f);

            Bq25792VbusAdcRegBits vbus_adc = {0};
            bq25792_read_reg16(instance, Bq25792RegVBUSADC, (uint16_t*)&vbus_adc);
            BQ25792_DEBUG(TAG, "VBUS ADC: %0.4f V", (float)vbus_adc.vbus_adc / 1000.0f);

            Bq25792IbatAdcRegBits ibat_adc = {0};
            bq25792_read_reg16(instance, Bq25792RegIBATADC, (uint16_t*)&ibat_adc);
            BQ25792_DEBUG(TAG, "IBAT ADC: %0.4f A", (float)ibat_adc.ibat_adc / 1000.0f);

            Bq25792IbusAdcRegBits ibus_adc = {0};
            bq25792_read_reg16(instance, Bq25792RegIBUSADC, (uint16_t*)&ibus_adc);
            BQ25792_DEBUG(TAG, "IBUS ADC: %0.4f A", (float)ibus_adc.ibus_adc / 1000.0f);

            BQ25792_DEBUG(TAG, "----");
        }
            Bq25792ChargerControl2RegBits charger_control_2 = {0};
            bq25792_read_reg8(instance, Bq25792RegChargerControl2, (uint8_t*)&charger_control_2);
            charger_control_2.sdrv_ctrl = 1;
            bq25792_write_reg8(instance, Bq25792RegChargerControl2, *(uint8_t*)&charger_control_2);
                for(int i = 0; i < 10; i++) {
            furi_delay_ms(1000);

            Bq25792VsysAdcRegBits vsys_adc = {0};
            bq25792_read_reg16(instance, Bq25792RegVSYSADC, (uint16_t*)&vsys_adc);
            BQ25792_DEBUG(TAG, "VSYS ADC: %0.4f V", (float)vsys_adc.vsys_adc / 1000.0f);

            Bq25792VbatAdcRegBits vbat_adc = {0};
            bq25792_read_reg16(instance, Bq25792RegVBATADC, (uint16_t*)&vbat_adc);
            BQ25792_DEBUG(TAG, "VBAT ADC: %0.4f V", (float)vbat_adc.vbat_adc / 1000.0f);

            Bq25792VbusAdcRegBits vbus_adc = {0};
            bq25792_read_reg16(instance, Bq25792RegVBUSADC, (uint16_t*)&vbus_adc);
            BQ25792_DEBUG(TAG, "VBUS ADC: %0.4f V", (float)vbus_adc.vbus_adc / 1000.0f);

            Bq25792IbatAdcRegBits ibat_adc = {0};
            bq25792_read_reg16(instance, Bq25792RegIBATADC, (uint16_t*)&ibat_adc);
            BQ25792_DEBUG(TAG, "IBAT ADC: %0.4f A", (float)ibat_adc.ibat_adc / 1000.0f);

            Bq25792IbusAdcRegBits ibus_adc = {0};
            bq25792_read_reg16(instance, Bq25792RegIBUSADC, (uint16_t*)&ibus_adc);
            BQ25792_DEBUG(TAG, "IBUS ADC: %0.4f A", (float)ibus_adc.ibus_adc / 1000.0f);

            BQ25792_DEBUG(TAG, "----");
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
