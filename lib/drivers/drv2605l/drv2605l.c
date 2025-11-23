
#include "furi_hal_gpio.h"
#include "drv2605l_reg.h"
#include "drv2605l.h"
#include <furi.h>

#include <furi_hal_i2c.h>
#include <pico/types.h>

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
        FURI_LOG_D(TAG, "Wrote reg 0x%02X: %08b", reg, data[0]);
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



// Apply the supply voltage to the DRV2605L device, and pull the EN pin high. The supply voltage should allow for adequate drive voltage of the selected actuator.
// Write a value of 0x07 to register 0x01. This value moves the DRV2605L device out of STANDBY and places the MODE[2:0] bits in auto-calibration mode.
// Populate the input parameters required by the auto-calibration engine:
// ERM_LRA — selection will depend on desired actuator.
// FB_BRAKE_FACTOR[2:0] — A value of 2 is valid for most actuators.
// LOOP_GAIN[1:0] — A value of 2 is valid for most actuators.
// RATED_VOLTAGE[7:0] — See the Rated Voltage Programming section for calculating the correct register value.
// OD_CLAMP[7:0] — See the Overdrive Voltage-Clamp Programming section for calculating the correct register value.
// AUTO_CAL_TIME[1:0] — A value of 3 is valid for most actuators.
// DRIVE_TIME[3:0] — See the Drive-Time Programming for calculating the correct register value.
// SAMPLE_TIME[1:0] — A value of 3 is valid for most actuators.
// BLANKING_TIME[3:0] — A value of 1 is valid for most actuators.
// IDISS_TIME[3:0] — A value of 1 is valid for most actuators.
// ZC_DET_TIME[1:0] — A value of 0 is valid for most actuators.
// Set the GO bit (write 0x01 to register 0x0C) to start the auto-calibration process. When auto calibration is complete, the GO bit automatically clears. The auto-calibration results are written in the respective registers as shown in Figure 25.
// Check the status of the DIAG_RESULT bit (in register 0x00) to ensure that the auto-calibration routine is complete without faults.
// Evaluate system performance with the auto-calibrated settings. Note that the evaluation should occur during the final assembly of the device because the auto-calibration process can affect actuator performance and behavior. If any adjustment is required, the inputs can be modified and this sequence can be repeated. If the performance is satisfactory, the user can do any of the following:
// Repeat the calibration process upon subsequent power ups.
// Store the auto-calibration results in host processor memory and rewrite them to the DRV2605L device upon subsequent power ups. The device retains these settings when in STANDBY mode or when the EN pin is low.
// Program the results permanently in nonvolatile, on-chip OTP memory. Even when a device power cycle occurs, the device retains the auto-calibration settings. See the Programming On-Chip OTP Memory section for additional information.

bool drv2605l_auto_calibrate(Drv2605l* instance) {
    furi_check(instance);
    furi_hal_gpio_write(instance->pin_en, true);

    Drv2605lMode mode_reg;
    mode_reg.mode_select = 0x07; // Auto-calibration mode
    drv2605l_write_reg(instance, mode, (uint8_t*)&mode_reg);

    // Populate input parameters
    Drv2605lFeedback feedback_reg;
    feedback_reg.n_erm_lra = 1; // LRA
    feedback_reg.brake_factor = 2; // 2
    feedback_reg.loop_gain = 2; // 2
    drv2605l_write_reg(instance, feedback, (uint8_t*)&feedback_reg);

    uint8_t rated_voltage_reg = 80; // Example rated voltage !!!
    drv2605l_write_reg(instance, rated_voltage, (uint8_t*)&rated_voltage_reg);  

    uint8_t overdrive_clamp_reg = 200; // Example overdrive clamp !!!
    drv2605l_write_reg(instance, overdrive_clamp, (uint8_t*)&overdrive_clamp_reg); // Rated voltage

    Drv2605lControl4 control4_reg;
    control4_reg.auto_cal_time = 3; // 1000:1200 ms
    drv2605l_write_reg(instance, control4, (uint8_t*)&control4_reg);

    Drv2605lControl1 control1_reg;
    control1_reg.drive_time = 25; // Max drive time !!!
    drv2605l_write_reg(instance, control1, (uint8_t*)&control1_reg);

    Drv2605lControl2 control2_reg;
    control2_reg.sample_time = 3; // 300 us
    control2_reg.blanking_time = 1; // 1
    control2_reg.idiss_time = 1; // 1
    drv2605l_write_reg(instance, control2, (uint8_t*)&control2_reg);
    

    drv2605l_read_reg(instance, control4, (uint16_t*)&control4_reg);
    control4_reg.zc_det_time = 0; // 0
    drv2605l_write_reg(instance, control4, (uint8_t*)&control4_reg);

    // Start auto-calibration
    Drv2605lGo go_reg;
    go_reg.go_bit = 1; // Start
    drv2605l_write_reg(instance, go, (uint8_t*)&go_reg);

    // Wait for completion
    furi_delay_ms(1500);

    uint16_t status = 0;
    drv2605l_read_reg(instance, status, &status);
    Drv2605lStatus* status_reg = (Drv2605lStatus*)&status;
    
    if(status_reg->diagnostic_result) {
        FURI_LOG_E(TAG, "Auto-calibration failed");
        return false;
    }

    FURI_LOG_I(TAG, "Auto-calibration successful");

    //calib reg 0x16 – 0x1A
    uint8_t calib_data = 0;
    drv2605l_read_reg(instance, rated_voltage, (uint16_t*)&calib_data);
    FURI_LOG_I(TAG, "Rated Voltage: reg 0x%02X -> 0x%02X", rated_voltage, calib_data);
    drv2605l_read_reg(instance, overdrive_clamp, (uint16_t*)&calib_data);
    FURI_LOG_I(TAG, "Overdrive Clamp: reg 0x%02X -> 0x%02X", overdrive_clamp, calib_data);
    drv2605l_read_reg(instance, auto_cal_comp, (uint16_t*)&calib_data);
    FURI_LOG_I(TAG, "Auto Cal Compensation: reg 0x%02X -> 0x%02X", auto_cal_comp, calib_data);
    drv2605l_read_reg(instance, auto_cal_bemf, (uint16_t*)&calib_data);
    FURI_LOG_I(TAG, "Auto Cal BEMF: reg 0x%02X -> 0x%02X", auto_cal_bemf, calib_data);
    drv2605l_read_reg(instance, feedback, (uint16_t*)&calib_data);
    FURI_LOG_I(TAG, "Feedback: reg 0x%02X -> 0x%02X", feedback, calib_data);

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
        // // // Device is ready
        // Drv2605lMode mode_reg;
        // mode_reg.device_reset = 0; // Set reset bit
        // mode_reg.standby = 0; // Set standby bit
        // mode_reg.mode_select = 0x00; // Internal Trigger
        // drv2605l_write_reg(instance, mode, (uint8_t*)&mode_reg);

        // uint16_t status_reg = 0;
        // drv2605l_read_reg(instance, status, &status_reg);
        // FURI_LOG_D(TAG, "Read reg 0x%02X: %08b", status, status_reg);

        // // Drv2605lLib lib_reg;
        // // lib_reg.hi_z_mode = 0; // Normal mode
        // // lib_reg.library_sel = 0x06; // LRA library
        // // drv2605l_write_reg(instance, lib_select, (uint8_t*)&lib_reg);

        // drv2605l_write_reg(instance, rtp_input, (uint8_t*)0x00);

        // uint8_t wave_seq1_reg = 1;
        // drv2605l_write_reg(instance, waveseq0, (uint8_t*)&wave_seq1_reg);
        // uint8_t wave_seq2_reg = 0;
        // drv2605l_write_reg(instance, waveseq1, (uint8_t*)&wave_seq2_reg);

        // // writeRegister8(DRV2605_REG_OVERDRIVE, 0); // no overdrive

        // // writeRegister8(DRV2605_REG_SUSTAINPOS, 0);
        // // writeRegister8(DRV2605_REG_SUSTAINNEG, 0);
        // // writeRegister8(DRV2605_REG_BREAK, 0);
        // // writeRegister8(DRV2605_REG_AUDIOMAX, 0x64);

        // // // ERM open loop

        // // // turn off N_ERM_LRA
        // // writeRegister8(DRV2605_REG_FEEDBACK,
        // //                 readRegister8(DRV2605_REG_FEEDBACK) & 0x7F);
        // // // turn on ERM_OPEN_LOOP
        // // writeRegister8(DRV2605_REG_CONTROL3,
        // //                 readRegister8(DRV2605_REG_CONTROL3) | 0x20);

        // drv2605l_write_reg(instance, overdrive, (uint8_t*)0x00); // no overdrive
        // drv2605l_write_reg(instance, sustain_time_pos, (uint8_t*)0x00);
        // drv2605l_write_reg(instance, sustain_time_neg, (uint8_t*)0x00);
        // drv2605l_write_reg(instance, break_time, (uint8_t*)0x00);
        // drv2605l_write_reg(instance, aud_max_lvl, (uint8_t*)0x64);    
        // // Set to ERM open loop
        // Drv2605lFeedback feedback_reg;  
        // drv2605l_read_reg(instance, feedback, (uint16_t*)&feedback_reg);
        // feedback_reg.n_erm_lra = 0; // ERM  
        // drv2605l_write_reg(instance, feedback, (uint8_t*)&feedback_reg);
        // Drv2605lControl3 control3_reg;
        // drv2605l_read_reg(instance, control3, (uint16_t*)&control3_reg);
        // control3_reg.erm_open_loop = 1; // ERM open loop
        // drv2605l_write_reg(instance, control3, (uint8_t*)&control3_reg);


        // //writeRegister8(DRV2605_REG_LIBRARY, lib);
        // drv2605l_write_reg(instance, lib_select, (uint8_t*)0x1);

        // drv2605l_write_reg(instance, mode, (uint8_t*)0x00);
        // drv2605l_write_reg(instance, waveseq0, (uint8_t*)0x01);
        // drv2605l_write_reg(instance, waveseq1, (uint8_t*)0x00);
        // // drv2605l_write_reg(instance, waveseq2, (uint8_t*)0x03);
        // // drv2605l_write_reg(instance, waveseq3, (uint8_t*)0x04);
        // // drv2605l_write_reg(instance, waveseq4, (uint8_t*)0x05);
        // // drv2605l_write_reg(instance, waveseq5, (uint8_t*)0x00);

        // Drv2605lGo go_reg;
        // go_reg.go_bit = 1; // Clear GO bit
        // drv2605l_write_reg(instance, go, (uint8_t*)&go_reg);

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

