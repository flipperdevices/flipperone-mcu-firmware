
#include "furi_hal_gpio.h"
#include "drv2605l_reg.h"
#include "drv2605l.h"
#include <furi.h>

#include <furi_hal_i2c.h>

#define TAG "Drv2605l"

struct Drv2605l {
    const FuriHalI2cBusHandle* i2c_handle;
    const GpioPin* pin_en;
    const GpioPin* pin_trigger;
    uint8_t address;

};

static int drv2605l_write_reg(Drv2605l* instance, Drv2605lReg reg, uint8_t* data) {
    furi_check(instance);

    uint8_t buffer[2] = {reg, *data};

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_master_tx_blocking(instance->i2c_handle, instance->address, buffer, sizeof(buffer), FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret != PICO_ERROR_GENERIC) {
        FURI_LOG_D(TAG, "Wrote reg 0x%02X: 0x%02X %08b", reg, data[0], data[0]);
    } else {
        FURI_LOG_E(TAG, "Failed to write reg 0x%02X", reg);
    }

    return ret;
}

static int drv2605l_read_reg(Drv2605l* instance, Drv2605lReg reg, uint16_t* data) {
    furi_check(instance);
    furi_check(data);

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_master_tx_blocking(instance->i2c_handle, instance->address, (uint8_t*)&reg, 1, FURI_HAL_I2C_TIMEOUT_US);
    if(ret != PICO_ERROR_GENERIC) {
        uint8_t buffer[1] = {0};
        ret = furi_hal_i2c_master_rx_blocking(instance->i2c_handle, instance->address, buffer, sizeof(buffer), FURI_HAL_I2C_TIMEOUT_US);
        if(ret != PICO_ERROR_GENERIC) {
            *data = buffer[0];
            FURI_LOG_D(TAG, "Read reg 0x%02X: %08b", reg, buffer[0]);
        } else {
            FURI_LOG_E(TAG, "Failed to read reg 0x%02X", reg);
        }
    } else {
        FURI_LOG_E(TAG, "Failed to write reg address 0x%02X for reading", reg);
    }
    furi_hal_i2c_release(instance->i2c_handle);

    return ret;
}


// 9.3.1 Initialization Procedure

// After powerup, wait at least 250 µs before the DRV2605L device accepts I2C commands.
// Assert the EN pin (logic high). The EN pin can be asserted any time during or after the 250-µs wait period.
// Write the MODE register (address 0x01) to value 0x00 to remove the device from standby mode.
// If the nonvolatile auto-calibration memory has been programmed as described in the Auto Calibration Procedure section, skip Step 5 and proceed to Step 6.
// Perform the steps as described in the Auto Calibration Procedure section. Alternatively, rewrite the results from a previous calibration.
// If using the embedded ROM library, write the library selection register (address 0x03) to select a library.
// The default setup is closed-loop bidirectional mode. To use other modes and features, write Control1 (0x1B), Control2 (0x1C), and Control3 (0x1D) as required. Open-loop operation is recommended for ERM mode when using the ROM libraries.
// Put the device in standby mode or deassert the EN pin, whichever is the most convenient. Both settings are low-power modes. The user can select the desired MODE (address 0x01) at the same time the STANDBY bit is set.

static FURI_ALWAYS_INLINE void drv2605l_enable(Drv2605l* instance) {
    furi_hal_gpio_write(instance->pin_en, true);
}

static FURI_ALWAYS_INLINE void drv2605l_disable(Drv2605l* instance) {
    furi_hal_gpio_write(instance->pin_en, false);
}

bool drv2605l_auto_calibrate(Drv2605l* instance) {
    furi_check(instance);
    drv2605l_enable(instance);

    uint8_t rated_voltage_reg = 0x53; //Vrms = 2 <--Setting
    uint8_t overdrive_clamp_reg = 0xA0; //Vmax = 3 <--Setting
    Drv2605lFeedback feedback_reg = {
        .n_erm_lra = 1, //LRA
        .brake_factor = 3, //4x
        .loop_gain = 1, //Medium
        .bemf_gain = 2, //1.365x
    };
    Drv2605lControl1 control1_reg = {
        .startup_boost = 1, //enabled
        .ac_couple = 0, //DC coupled
        .drive_time = 19, // <--Setting
    };
    Drv2605lControl2 control2_reg = {
        .bidir_input = 1, //Bidir input
        .brake_stabilizer = 0, //disabled
        .sample_time = 3, //300us
        .blanking_time = 1,
        .idiss_time = 1,
    }; 
    Drv2605lControl3 control3_reg = {
        .ng_thresh = 2, //4%
        .erm_open_loop = 0, //Closed loop
        .supply_comp_dis = 0, //Enabled
        .data_fomat_rtp = 0, //Signed
        .lra_drive_mode = 0, //Once per cycle
        .n_pwm_analog = 0, //PWM Input
        .lra_open_loop = 0, //Auto-resonance mode
    }; 
    Drv2605lMode mode_reg = {
        .device_reset = 0, //Normal operation
        .standby = 0, //Active mode
        .mode_select = 0b111, //Auto-calibration mode
    };
    Drv2605lControl4 control4_reg = {
        .auto_cal_time = 2, //1000:1200 ms
        .otp_status = 0,
        .otp_program = 0, //OTP Memory has not been programmed
        .zc_det_time = 0, //100us
    };
    Drv2605lGo go_reg = {
        .go_bit = 1, //Start auto-calibration
    };

    drv2605l_write_reg(instance, rated_voltage, &rated_voltage_reg);
    drv2605l_write_reg(instance, overdrive_clamp, &overdrive_clamp_reg);
    drv2605l_write_reg(instance, feedback, (uint8_t*)&feedback_reg);
    drv2605l_write_reg(instance, control1, (uint8_t*)&control1_reg);
    drv2605l_write_reg(instance, control2, (uint8_t*)&control2_reg);
    drv2605l_write_reg(instance, control3, (uint8_t*)&control3_reg);
    drv2605l_write_reg(instance, mode, (uint8_t*)&mode_reg);
    drv2605l_write_reg(instance, control4, (uint8_t*)&control4_reg);
    drv2605l_write_reg(instance, go, (uint8_t*)&go_reg);

    // Wait for completion
    uint32_t timeout = furi_get_tick() + 2000;
    while(furi_get_tick() < timeout) {
        uint16_t go_status = 0;
        drv2605l_read_reg(instance, go, &go_status);
        Drv2605lGo* go_reg_status = (Drv2605lGo*)&go_status;
        if(go_reg_status->go_bit == 0) {
            break;
        }
        furi_delay_ms(10);
    }

    uint16_t status = 0;
    drv2605l_read_reg(instance, status, &status);
    Drv2605lStatus* status_reg = (Drv2605lStatus*)&status;
    
    if(status_reg->diagnostic_result) {
        FURI_LOG_E(TAG, "Auto-calibration failed");
        drv2605l_disable(instance); 
        return false;
    }

    FURI_LOG_I(TAG, "Auto-calibration successful");

    //calib reg 0x18, 0x19, 0x1A (BEMFGain)
    uint8_t calib_data = 0;

    drv2605l_read_reg(instance, auto_cal_comp, (uint16_t*)&calib_data);
    FURI_LOG_I(TAG, "Auto Cal Compensation: reg 0x%02X -> 0x%02X", auto_cal_comp, calib_data);
    drv2605l_read_reg(instance, auto_cal_bemf, (uint16_t*)&calib_data);
    FURI_LOG_I(TAG, "Auto Cal BEMF: reg 0x%02X -> 0x%02X", auto_cal_bemf, calib_data);
    drv2605l_read_reg(instance, feedback, (uint16_t*)&calib_data);
    FURI_LOG_I(TAG, "Feedback: reg 0x%02X -> 0x%02X", feedback, calib_data);

    drv2605l_disable(instance); 
    return true;
}


Drv2605l* drv2605l_init(const FuriHalI2cBusHandle* i2c_handle, const GpioPin* pin_en, const GpioPin* pin_trigger, uint8_t address) {
    Drv2605l* instance = (Drv2605l*)malloc(sizeof(Drv2605l));
    instance->i2c_handle = i2c_handle;
    instance->pin_en = pin_en;
    instance->pin_trigger = pin_trigger;
    instance->address = address;
    
    furi_hal_gpio_init_simple(instance->pin_en, GpioModeOutputPushPull);
    //furi_hal_gpio_init_simple(instance->pin_trigger, GpioModeOutputPushPull);
    furi_hal_gpio_write(instance->pin_en, false);


    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_device_ready(instance->i2c_handle, instance->address, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret) {
        drv2605l_auto_calibrate(instance);


        uint8_t reg = 0x00; // Internal Trigger
        drv2605l_write_reg(instance, mode, (uint8_t*)&reg);
        reg = 0x06;
        drv2605l_write_reg(instance, lib_select, (uint8_t*)&reg);\
        reg = 0x01;
        drv2605l_write_reg(instance, waveseq0, (uint8_t*)&reg);
        reg = 0x0;
        drv2605l_write_reg(instance, waveseq1, (uint8_t*)&reg);

        for(uint8_t i = 1; i <= 123; i++) {
            reg = i;
            drv2605l_write_reg(instance, waveseq0, (uint8_t*)&reg);
            reg = 0x01;
            drv2605l_write_reg(instance, go, (uint8_t*)&reg);
            furi_delay_ms(500);
        }
        

    } else {
        FURI_LOG_E(TAG, "DRV2605L device not ready at address 0x%02X", instance->address);
        furi_hal_gpio_init_ex(instance->pin_en, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
        furi_hal_gpio_init_ex(instance->pin_trigger, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
        free(instance);
        return NULL;
    }

    return instance;
}

void drv2605l_deinit(Drv2605l* instance) {
    furi_check(instance);
    furi_hal_gpio_init_ex(instance->pin_en, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(instance->pin_trigger, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    free(instance);
}

