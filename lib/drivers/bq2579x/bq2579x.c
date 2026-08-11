#include "bq2579x.h"
#include <furi.h>

#include <furi_hal_i2c.h>

#define TAG "Bq2579x"

#define BQ25792_DEVICE_PART_NUMBER        0b001 //BQ25792
#define BQ25798_DEVICE_PART_NUMBER        0b011 //BQ25798
#define BQ2579X_MAX_INPUT_DEFAULT_CURRENT 500 // mA

#ifdef BQ2579X_DEBUG_ENABLE
#define BQ2579X_DEBUG(...) FURI_LOG_D(__VA_ARGS__)
#else
#define BQ2579X_DEBUG(...)
#endif

struct Bq2579x {
    const FuriHalI2cBusHandle* i2c_handle;
    uint8_t address;
    const GpioPin* pin_interrupt;
    Bq2579xCallbackInput callback;
    void* context;
    Bq2579xChip chip;
};

static __isr __not_in_flash_func(void) bq2579x_interrupt_handler(void* ctx) {
    Bq2579x* instance = (Bq2579x*)ctx;
    if(instance->callback) {
        instance->callback(instance->context);
    }
}

static Bq2579xStatus bq2579x_check_status(int status) {
    Bq2579xStatus ret = Bq2579xStatusUnknown;
    if(status >= PICO_OK) {
        ret = Bq2579xStatusOk;
    } else if(status == PICO_ERROR_GENERIC) {
        ret = Bq2579xStatusError;
    } else if(status == PICO_ERROR_TIMEOUT) {
        ret = Bq2579xStatusTimeout;
    } else {
        ret = Bq2579xStatusUnknown;
    }
    return ret;
}

static Bq2579xStatus bq2579x_write_reg8(Bq2579x* instance, Bq2579xReg reg, uint8_t data) {
    furi_check(instance);

    uint8_t buffer[2] = {reg, data};

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_master_tx_blocking(instance->i2c_handle, instance->address, buffer, sizeof(buffer), FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
        FURI_LOG_E(TAG, "Failed to write reg 0x%02X", reg);
    } else {
        BQ2579X_DEBUG(TAG, "Wrote reg 0x%02X: %08b", reg, data);
    }

    return bq2579x_check_status(ret);
}

static Bq2579xStatus bq2579x_write_reg16(Bq2579x* instance, Bq2579xReg reg, uint16_t data) {
    furi_check(instance);

    uint8_t buffer[3] = {reg, data >> 8, data & 0xFF};

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_master_tx_blocking(instance->i2c_handle, instance->address, buffer, sizeof(buffer), FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
        FURI_LOG_E(TAG, "Failed to write reg 0x%02X", reg);
    } else {
        BQ2579X_DEBUG(TAG, "Wrote reg 0x%02X: %016b", reg, data);
    }

    return bq2579x_check_status(ret);
}

static Bq2579xStatus bq2579x_read_reg8(Bq2579x* instance, Bq2579xReg reg, uint8_t* data) {
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

    return bq2579x_check_status(ret);
}

static Bq2579xStatus bq2579x_read_reg16(Bq2579x* instance, Bq2579xReg reg, uint16_t* data) {
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

    return bq2579x_check_status(ret);
}

static Bq2579xStatus bq2579x_read_mem(Bq2579x* instance, Bq2579xReg reg, uint8_t* data, size_t length) {
    furi_check(instance);
    furi_check(data);

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_master_tx_blocking(instance->i2c_handle, instance->address, (uint8_t*)&reg, 1, FURI_HAL_I2C_TIMEOUT_US);
    if(!(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT)) {
        ret = furi_hal_i2c_master_rx_blocking(instance->i2c_handle, instance->address, data, length, FURI_HAL_I2C_TIMEOUT_US);
        if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
            FURI_LOG_E(TAG, "Failed to read reg 0x%02X", reg);
        }
    } else {
        FURI_LOG_E(TAG, "Failed to write reg address 0x%02X for reading", reg);
    }
    furi_hal_i2c_release(instance->i2c_handle);

    return bq2579x_check_status(ret);
}

Bq2579xStatus bq2579x_load_default_config(Bq2579x* instance) {
    furi_check(instance);
    Bq2579xStatus res = Bq2579xStatusUnknown;
    do {
        Bq2579xTerminationControlRegBits termination_control = {.reg_rst = 1}; // Reset all registers to default values
        res = bq2579x_write_reg8(instance, Bq2579xRegTerminationControl, *(uint8_t*)&termination_control);
        if(res != Bq2579xStatusOk) {
            break;
        }

        // Wait for reset to complete
        furi_delay_ms(10);

        Bq2579xChargerControl1RegBits charger_control_1 = {0};
        res = bq2579x_read_reg8(instance, Bq2579xRegChargerControl1, (uint8_t*)&charger_control_1);
        if(res != Bq2579xStatusOk) {
            break;
        }
        // ToDo: Implement a watchdog reset mechanism
        charger_control_1.wd_rst = 1; // Reset watchdog timer
        charger_control_1.watchdog = Bq2579xWatchdogTimeDisabled; // Disable watchdog timer
        // Input can be up to 20V (USB-PD): BQ25798 resets VAC_OVP to 7V, BQ25792 to 26V.
        // Set 26V explicitly on both chips, the next threshold down (22V/18V) leaves
        // too little headroom above a 20V+5% adapter.
        charger_control_1.vac_ovp = Bq2579xVacOvp26V;
        res = bq2579x_write_reg8(instance, Bq2579xRegChargerControl1, *(uint8_t*)&charger_control_1);
        if(res != Bq2579xStatusOk) {
            break;
        }

        Bq2579xAdcControlRegBits adc_control = {0};
        res = bq2579x_read_reg8(instance, Bq2579xRegADCControl, (uint8_t*)&adc_control);
        if(res != Bq2579xStatusOk) {
            break;
        }
        adc_control.adc_en = 1; // Enable ADC
        res = bq2579x_write_reg8(instance, Bq2579xRegADCControl, *(uint8_t*)&adc_control);
        if(res != Bq2579xStatusOk) {
            break;
        }

        // Disable Dp/Dm detection
        Bq2579xChargerControl2RegBits charger_control_2 = {0};
        res = bq2579x_write_reg8(instance, Bq2579xRegChargerControl2, *(uint8_t*)&charger_control_2);
        if(res != Bq2579xStatusOk) {
            break;
        }

        Bq2579xChargerControl5RegBits charger_control_5 = {0};
        res = bq2579x_read_reg8(instance, Bq2579xRegChargerControl5, (uint8_t*)&charger_control_5);
        if(res != Bq2579xStatusOk) {
            break;
        }
        charger_control_5.en_extilim = 0; // Disable external ILIM pin control
        charger_control_5.sfet_present = 1; // Enable Sfet presence detection
        charger_control_5.en_ibat = 1; // Enable IBAT measurement
        res = bq2579x_write_reg8(instance, Bq2579xRegChargerControl5, *(uint8_t*)&charger_control_5);
        if(res != Bq2579xStatusOk) {
            break;
        }

        res = bq2579x_set_input_current_limit_ma(instance, BQ2579X_MAX_INPUT_DEFAULT_CURRENT);
        if(res != Bq2579xStatusOk) {
            break;
        }

        Bq2579xChargerControl0RegBits charger_control_0 = {0};
        res = bq2579x_read_reg8(instance, Bq2579xRegChargerControl0, (uint8_t*)&charger_control_0);
        if(res != Bq2579xStatusOk) {
            break;
        }
        // Enabling automatic adjustment of input current
        charger_control_0.en_ico = 1; // Enable ICO current limit
        res = bq2579x_write_reg8(instance, Bq2579xRegChargerControl0, *(uint8_t*)&charger_control_0);
        if(res != Bq2579xStatusOk) {
            break;
        }

    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to load config!");
    }
    return res;
}

Bq2579x* bq2579x_init(const FuriHalI2cBusHandle* i2c_handle, uint8_t address, const GpioPin* pin_interrupt) {
    Bq2579x* instance = (Bq2579x*)malloc(sizeof(Bq2579x));
    instance->i2c_handle = i2c_handle;
    instance->address = address;

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_device_ready(instance->i2c_handle, instance->address, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret) {
        FURI_LOG_I(TAG, "BQ2579X device ready at address 0x%02X", instance->address);
        if(instance->pin_interrupt) {
            furi_hal_gpio_init_simple(instance->pin_interrupt, GpioModeInput);
            furi_hal_gpio_add_int_callback(instance->pin_interrupt, GpioConditionFall, bq2579x_interrupt_handler, instance);
        }

        Bq2579xPartInformationRegBits device_info = {0};
        bq2579x_read_reg8(instance, Bq2579xRegPartInformation, (uint8_t*)&device_info);
        if(device_info.pn == BQ25792_DEVICE_PART_NUMBER) {
            instance->chip = Bq2579xChipBq25792;
            FURI_LOG_I(TAG, "Detected BQ25792, revision %u", device_info.dev_rev);
        } else if(device_info.pn == BQ25798_DEVICE_PART_NUMBER) {
            instance->chip = Bq2579xChipBq25798;
            FURI_LOG_I(TAG, "Detected BQ25798, revision %u", device_info.dev_rev);
        } else {
            furi_crash("BQ2579X unknown device part number!");
        }

        if(bq2579x_load_default_config(instance) != Bq2579xStatusOk) {
            furi_crash("BQ2579X failed to load default config");
        }

    } else {
        FURI_LOG_E(TAG, "BQ2579X device not ready at address 0x%02X", instance->address);
        free(instance);
        return NULL;
    }

    return instance;
}

Bq2579xChip bq2579x_get_chip(Bq2579x* instance) {
    furi_check(instance);
    return instance->chip;
}

void bq2579x_deinit(Bq2579x* instance) {
    furi_check(instance);
    if(instance->pin_interrupt) {
        furi_hal_gpio_remove_int_callback(instance->pin_interrupt);
        furi_hal_gpio_init_ex(instance->pin_interrupt, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    }
    free(instance);
}

Bq2579xStatus bq2579x_set_power_switch(Bq2579x* instance, Bq2579xPowerSwitch power_switch) {
    furi_check(instance);
    Bq2579xStatus res = Bq2579xStatusUnknown;
    do {
        Bq2579xChargerControl2RegBits charger_control_2 = {0};
        res = bq2579x_read_reg8(instance, Bq2579xRegChargerControl2, (uint8_t*)&charger_control_2);
        if(res != Bq2579xStatusOk) {
            break;
        }
        charger_control_2.sdrv_ctrl = power_switch; // Set power switch
        charger_control_2.sdrv_dly = 1; // Immediatly, without delay

        res = bq2579x_write_reg8(instance, Bq2579xRegChargerControl2, *(uint8_t*)&charger_control_2);
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to set power switch!");
    }
    return res;
}

Bq2579xStatus bq2579x_get_ibus_ma(Bq2579x* instance, int16_t* ibus) {
    furi_check(instance);
    furi_check(ibus);
    Bq2579xStatus res = Bq2579xStatusUnknown;
    do {
        res = bq2579x_read_reg16(instance, Bq2579xRegIBUSADC, (uint16_t*)ibus);
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to get IBUS!");
    }
    return res;
}

Bq2579xStatus bq2579x_get_ibat_ma(Bq2579x* instance, int16_t* ibat) {
    furi_check(instance);
    furi_check(ibat);
    Bq2579xStatus res = Bq2579xStatusUnknown;
    do {
        res = bq2579x_read_reg16(instance, Bq2579xRegIBATADC, (uint16_t*)ibat);
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to get IBAT!");
    }
    return res;
}

Bq2579xStatus bq2579x_get_vbus_mv(Bq2579x* instance, uint16_t* vbus) {
    furi_check(instance);
    furi_check(vbus);
    Bq2579xStatus res = Bq2579xStatusUnknown;
    do {
        res = bq2579x_read_reg16(instance, Bq2579xRegVBUSADC, vbus);
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to get VBUS!");
    }
    return res;
}

Bq2579xStatus bq2579x_get_vbat_mv(Bq2579x* instance, uint16_t* vbat) {
    furi_check(instance);
    furi_check(vbat);
    Bq2579xStatus res = Bq2579xStatusUnknown;
    do {
        res = bq2579x_read_reg16(instance, Bq2579xRegVBATADC, vbat);
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to get VBAT!");
    }
    return res;
}

Bq2579xStatus bq2579x_get_vsys_mv(Bq2579x* instance, uint16_t* vsys) {
    furi_check(instance);
    furi_check(vsys);
    Bq2579xStatus res = Bq2579xStatusUnknown;
    do {
        res = bq2579x_read_reg16(instance, Bq2579xRegVSYSADC, vsys);
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to get VSYS!");
    }
    return res;
}

Bq2579xStatus bq2579x_get_bat_pct(Bq2579x* instance, float* bat_pct) {
    furi_check(instance);
    furi_check(bat_pct);
    Bq2579xStatus res = Bq2579xStatusUnknown;
    uint16_t raw_bat_pct = 0;
    do {
        res = bq2579x_read_reg16(instance, Bq2579xRegTSADC, &raw_bat_pct);
        if(res == Bq2579xStatusOk) {
            *bat_pct = raw_bat_pct * 0.0976563f; // Convert to percentage (0.09765625% per LSB)
        }
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to get battery percentage!");
    }
    return res;
}

Bq2579xStatus bq2579x_get_charger_temperature(Bq2579x* instance, float* temperature) {
    furi_check(instance);
    furi_check(temperature);
    Bq2579xStatus res = Bq2579xStatusUnknown;
    uint16_t raw_temperature = 0;
    do {
        res = bq2579x_read_reg16(instance, Bq2579xRegTDIEADC, &raw_temperature);
        if(res == Bq2579xStatusOk) {
            *temperature = raw_temperature * 0.5f; // Convert to temperature (0.5°C per LSB)
        }
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to get charger temperature!");
    }
    return res;
}

Bq2579xStatus bq2579x_get_input_current_limit_ma(Bq2579x* instance, uint16_t* input_current_limit) {
    furi_check(instance);
    furi_check(input_current_limit);
    Bq2579xStatus res = Bq2579xStatusUnknown;
    do {
        res = bq2579x_read_reg16(instance, Bq2579xRegInputCurrentLimit, input_current_limit);
        if(res == Bq2579xStatusOk) {
            *input_current_limit = *input_current_limit * 10; // Convert to input current limit (10 mA per LSB)
        }
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to get input current limit!");
    }
    return res;
}

Bq2579xStatus bq2579x_set_input_current_limit_ma(Bq2579x* instance, uint16_t input_current_limit) {
    furi_check(instance);
    furi_check(input_current_limit <= 3300); // Max input current limit is 3300 mA
    Bq2579xStatus res = Bq2579xStatusUnknown;
    do {
        input_current_limit = input_current_limit / 10; // Convert to register value (10 mA per LSB)
        res = bq2579x_write_reg16(instance, Bq2579xRegInputCurrentLimit, input_current_limit);
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to set input current limit!");
    }
    return res;
}

Bq2579xStatus bq2579x_get_charge_voltage_limit_ma(Bq2579x* instance, uint16_t* charge_voltage_limit) {
    furi_check(instance);
    furi_check(charge_voltage_limit);
    Bq2579xStatus res = Bq2579xStatusUnknown;
    do {
        res = bq2579x_read_reg16(instance, Bq2579xRegChargeVoltageLimit, charge_voltage_limit);
        if(res == Bq2579xStatusOk) {
            *charge_voltage_limit = *charge_voltage_limit * 10; // Convert to charge voltage limit (10 mV per LSB)
        }
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to get charge voltage limit!");
    }
    return res;
}

Bq2579xStatus bq2579x_set_charge_voltage_limit_ma(Bq2579x* instance, uint16_t charge_voltage_limit) {
    furi_check(instance);
    furi_check(charge_voltage_limit >= 8000 && charge_voltage_limit <= 8800); // Max charge voltage limit is 8800 mV
    Bq2579xStatus res = Bq2579xStatusUnknown;
    Bq2579xChargeVoltageLimitRegBits charge_voltage_limit_reg = {0};
    charge_voltage_limit_reg.vreg = charge_voltage_limit / 10; // Convert to register value (10 mV per LSB)
    do {
        res = bq2579x_write_reg16(instance, Bq2579xRegChargeVoltageLimit, *(uint16_t*)&charge_voltage_limit_reg);
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to set charge voltage limit!");
    }
    return res;
}

Bq2579xStatus bq2579x_get_charge_current_limit_ma(Bq2579x* instance, uint16_t* charge_current_limit) {
    furi_check(instance);
    furi_check(charge_current_limit);
    Bq2579xStatus res = Bq2579xStatusUnknown;
    do {
        res = bq2579x_read_reg16(instance, Bq2579xRegChargeCurrentLimit, charge_current_limit);
        if(res == Bq2579xStatusOk) {
            *charge_current_limit = *charge_current_limit * 10; // Convert to charge current limit (10 mA per LSB)
        }
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to get charge current limit!");
    }
    return res;
}

Bq2579xStatus bq2579x_set_charge_current_limit_ma(Bq2579x* instance, uint16_t charge_current_limit) {
    furi_check(instance);
    furi_check(charge_current_limit <= 5000); // Max charge current limit is 5000 mA
    Bq2579xStatus res = Bq2579xStatusUnknown;
    do {
        charge_current_limit = charge_current_limit / 10; // Convert to register value (10 mA per LSB)
        res = bq2579x_write_reg16(instance, Bq2579xRegChargeCurrentLimit, charge_current_limit);
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to set charge current limit!");
    }
    return res;
}

Bq2579xStatus bq2579x_get_ico_current_limit_ma(Bq2579x* instance, uint16_t* ico_current_limit) {
    furi_check(instance);
    furi_check(ico_current_limit);
    Bq2579xStatus res = Bq2579xStatusUnknown;
    Bq2579xICOCurrentLimitRegBits ico_current_limit_reg = {0};
    do {
        res = bq2579x_read_reg16(instance, Bq2579xRegICOCurrentLimit, (uint16_t*)&ico_current_limit_reg);
        if(res == Bq2579xStatusOk) {
            *ico_current_limit = ico_current_limit_reg.ico_ilim * 10; // Convert to ICO current limit (10 mA per LSB)
        }
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to get ICO current limit!");
    }
    return res;
}

Bq2579xStatus bq2579x_charge_enable(Bq2579x* instance, bool enable) {
    furi_check(instance);
    Bq2579xStatus res = Bq2579xStatusUnknown;
    Bq2579xChargerControl0RegBits charger_control_0 = {0};
    do {
        res = bq2579x_read_reg8(instance, Bq2579xRegChargerControl0, (uint8_t*)&charger_control_0);
        if(res != Bq2579xStatusOk) {
            break;
        }
        charger_control_0.en_chg = enable ? 1 : 0;
        res = bq2579x_write_reg8(instance, Bq2579xRegChargerControl0, *(uint8_t*)&charger_control_0);
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to set charge enable!");
    }
    return res;
}

Bq2579xStatus bq2579x_charge_is_enabled(Bq2579x* instance, bool* enabled) {
    furi_check(instance);
    furi_check(enabled);
    Bq2579xStatus res = Bq2579xStatusUnknown;
    Bq2579xChargerControl0RegBits charger_control_0 = {0};
    do {
        res = bq2579x_read_reg8(instance, Bq2579xRegChargerControl0, (uint8_t*)&charger_control_0);
        if(res == Bq2579xStatusOk) {
            *enabled = charger_control_0.en_chg ? true : false;
        }
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to get charge enable status!");
    }
    return res;
}

Bq2579xStatus bq2579x_get_charger_status(Bq2579x* instance, Bq2579xChargerStatusReg* status) {
    furi_check(instance);
    furi_check(status);
    Bq2579xStatus res = Bq2579xStatusUnknown;
    do {
        res = bq2579x_read_mem(instance, Bq2579xRegChargerStatus0, status->data, sizeof(status->data));
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to get charger status!");
    }
    return res;
}

Bq2579xStatus bq2579x_get_charger_fault(Bq2579x* instance, Bq2579xFaultStatusReg* fault) {
    furi_check(instance);
    furi_check(fault);
    Bq2579xStatus res = Bq2579xStatusUnknown;
    do {
        res = bq2579x_read_mem(instance, Bq2579xRegFaultStatus0, fault->data, sizeof(fault->data));
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to get charger fault!");
    }
    return res;
}

Bq2579xStatus bq2579x_clear_charger_fault(Bq2579x* instance, Bq2579xFaultStatusReg* fault) {
    furi_check(instance);
    Bq2579xStatus res = Bq2579xStatusUnknown;
    do {
        res = bq2579x_write_reg16(instance, Bq2579xRegFaultStatus0, *(uint16_t*)fault->data);
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to clear charger fault!");
    }
    return res;
}

Bq2579xStatus bq2579x_get_charger_irq_flags(Bq2579x* instance, Bq2579xChargerFlagReg* irq_flags) {
    furi_check(instance);
    furi_check(irq_flags);
    Bq2579xStatus res = Bq2579xStatusUnknown;
    do {
        res = bq2579x_read_mem(instance, Bq2579xRegChargerFlag0, irq_flags->data, sizeof(irq_flags->data));
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to get charger irq flags!");
    }
    return res;
}

Bq2579xStatus bq2579x_adc_enable(Bq2579x* instance, bool enable) {
    furi_check(instance);
    Bq2579xStatus res = Bq2579xStatusUnknown;

    Bq2579xAdcControlRegBits adc_control = {0};
    do {
        res = bq2579x_read_reg8(instance, Bq2579xRegADCControl, (uint8_t*)&adc_control);
        if(res != Bq2579xStatusOk) {
            break;
        }
        adc_control.adc_en = enable ? 1 : 0; // Enable or disable ADC
        res = bq2579x_write_reg8(instance, Bq2579xRegADCControl, *(uint8_t*)&adc_control);
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to set ADC enable!");
    }
    return res;
}

Bq2579xStatus bq2579x_mppt_enable(Bq2579x* instance, bool enable) {
    furi_check(instance);
    if(instance->chip != Bq2579xChipBq25798) {
        FURI_LOG_E(TAG, "MPPT is only available on BQ25798!");
        return Bq2579xStatusError;
    }
    Bq2579xStatus res = Bq2579xStatusUnknown;
    Bq2579xMPPTControlRegBits mppt_control = {0};
    do {
        res = bq2579x_read_reg8(instance, Bq2579xRegMPPTControl, (uint8_t*)&mppt_control);
        if(res != Bq2579xStatusOk) {
            break;
        }
        mppt_control.en_mppt = enable ? 1 : 0;
        res = bq2579x_write_reg8(instance, Bq2579xRegMPPTControl, *(uint8_t*)&mppt_control);
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to set MPPT enable!");
    }
    return res;
}

Bq2579xStatus bq2579x_otg_enable(Bq2579x* instance, bool enable) {
    furi_check(instance);
    Bq2579xStatus res = Bq2579xStatusUnknown;
    Bq2579xChargerControl3RegBits charger_control_3 = {0};
    do {
        res = bq2579x_read_reg8(instance, Bq2579xRegChargerControl3, (uint8_t*)&charger_control_3);
        if(res != Bq2579xStatusOk) {
            break;
        }
        charger_control_3.en_otg = enable ? 1 : 0;
        res = bq2579x_write_reg8(instance, Bq2579xRegChargerControl3, *(uint8_t*)&charger_control_3);
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to set OTG enable!");
    }
    return res;
}

Bq2579xStatus bq2579x_get_otg_voltage_mv(Bq2579x* instance, uint16_t* otg_voltage) {
    furi_check(instance);
    furi_check(otg_voltage);
    Bq2579xStatus res = Bq2579xStatusUnknown;
    Bq2579xVOTGRegulationRegBits votg_reg = {0};
    do {
        res = bq2579x_read_reg16(instance, Bq2579xRegVOTGRegulation, (uint16_t*)&votg_reg);
        if(res == Bq2579xStatusOk) {
            // Fixed offset 2800 mV, step 10 mV
            *otg_voltage = (uint16_t)votg_reg.votg * 10 + 2800;
        }
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to get OTG voltage!");
    }
    return res;
}

Bq2579xStatus bq2579x_set_otg_voltage_mv(Bq2579x* instance, uint16_t otg_voltage) {
    furi_check(instance);
    if(otg_voltage < 2800 || otg_voltage > 22000) { // VOTG range 2800-22000 mV
        FURI_LOG_E(TAG, "OTG voltage %u mV out of range [2800, 22000]", otg_voltage);
        return Bq2579xStatusError;
    }
    Bq2579xStatus res = Bq2579xStatusUnknown;
    Bq2579xVOTGRegulationRegBits votg_reg = {0};
    votg_reg.votg = (otg_voltage - 2800) / 10; // Fixed offset 2800 mV, step 10 mV
    do {
        res = bq2579x_write_reg16(instance, Bq2579xRegVOTGRegulation, *(uint16_t*)&votg_reg);
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to set OTG voltage!");
    }
    return res;
}

Bq2579xStatus bq2579x_get_otg_current_ma(Bq2579x* instance, uint16_t* otg_current) {
    furi_check(instance);
    furi_check(otg_current);
    Bq2579xStatus res = Bq2579xStatusUnknown;
    Bq2579xIOTGRegulationRegBits iotg_reg = {0};
    do {
        res = bq2579x_read_reg8(instance, Bq2579xRegIOTGRegulation, (uint8_t*)&iotg_reg);
        if(res == Bq2579xStatusOk) {
            *otg_current = (uint16_t)iotg_reg.iotg * 40; // Step 40 mA per LSB
        }
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to get OTG current!");
    }
    return res;
}

Bq2579xStatus bq2579x_set_otg_current_ma(Bq2579x* instance, uint16_t otg_current) {
    furi_check(instance);
    if(otg_current < 120 || otg_current > 3320) { // IOTG range 120-3320 mA
        FURI_LOG_E(TAG, "OTG current %u mA out of range [120, 3320]", otg_current);
        return Bq2579xStatusError;
    }
    Bq2579xStatus res = Bq2579xStatusUnknown;
    Bq2579xIOTGRegulationRegBits iotg_reg = {0};
    do {
        res = bq2579x_read_reg8(instance, Bq2579xRegIOTGRegulation, (uint8_t*)&iotg_reg);
        if(res != Bq2579xStatusOk) {
            break;
        }
        iotg_reg.iotg = otg_current / 40; // Step 40 mA per LSB, preserve prechg_tmr
        res = bq2579x_write_reg8(instance, Bq2579xRegIOTGRegulation, *(uint8_t*)&iotg_reg);
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to set OTG current!");
    }
    return res;
}

Bq2579xStatus bq2579x_watchdog_reset(Bq2579x* instance) {
    furi_check(instance);
    Bq2579xStatus res = Bq2579xStatusUnknown;

    Bq2579xChargerControl1RegBits charger_control_1 = {0};
    do {
        res = bq2579x_read_reg8(instance, Bq2579xRegChargerControl1, (uint8_t*)&charger_control_1);
        if(res != Bq2579xStatusOk) {
            break;
        }
        charger_control_1.wd_rst = 1; // Reset watchdog timer
        res = bq2579x_write_reg8(instance, Bq2579xRegChargerControl1, *(uint8_t*)&charger_control_1);
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to reset watchdog!");
    }
    return res;
}

Bq2579xStatus bq2579x_watchdog_set_time(Bq2579x* instance, Bq2579xWatchdogTime time) {
    furi_check(instance);
    Bq2579xStatus res = Bq2579xStatusUnknown;

    Bq2579xChargerControl1RegBits charger_control_1 = {0};
    do {
        res = bq2579x_read_reg8(instance, Bq2579xRegChargerControl1, (uint8_t*)&charger_control_1);
        if(res != Bq2579xStatusOk) {
            break;
        }
        charger_control_1.watchdog = time; // Set watchdog timer
        res = bq2579x_write_reg8(instance, Bq2579xRegChargerControl1, *(uint8_t*)&charger_control_1);
    } while(0);
    if(res != Bq2579xStatusOk) {
        FURI_LOG_E(TAG, "Failed to set watchdog time!");
    }
    return res;
}
