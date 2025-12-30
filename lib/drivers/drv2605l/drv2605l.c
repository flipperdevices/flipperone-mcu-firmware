
#include "furi_hal_gpio.h"
#include "drv2605l_reg.h"
#include "drv2605l.h"
#include <furi.h>

#include <furi_hal_i2c.h>

#define TAG "Drv2605l"

#ifdef DRV2605L_DEBUG_ENABLE
#define DRV2605L_DEBUG(...) FURI_LOG_D(__VA_ARGS__)
#else
#define DRV2605L_DEBUG(...)
#endif

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

    if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
        FURI_LOG_E(TAG, "Failed to write reg 0x%02X", reg);
    } else {
        DRV2605L_DEBUG(TAG, "Wrote reg 0x%02X: 0x%02X %08b", reg, data[0], data[0]);
    }

    return ret;
}

static int drv2605l_read_reg(Drv2605l* instance, Drv2605lReg reg, uint16_t* data) {
    furi_check(instance);
    furi_check(data);

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_master_tx_blocking(instance->i2c_handle, instance->address, (uint8_t*)&reg, 1, FURI_HAL_I2C_TIMEOUT_US);
    if(!(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT)) {
        uint8_t buffer[1] = {0};
        ret = furi_hal_i2c_master_rx_blocking(instance->i2c_handle, instance->address, buffer, sizeof(buffer), FURI_HAL_I2C_TIMEOUT_US);
        if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
            FURI_LOG_E(TAG, "Failed to read reg 0x%02X", reg);
        } else {
            *data = buffer[0];
            DRV2605L_DEBUG(TAG, "Read reg 0x%02X: %08b", reg, buffer[0]);
        }
    } else {
        FURI_LOG_E(TAG, "Failed to write reg address 0x%02X for reading", reg);
    }
    furi_hal_i2c_release(instance->i2c_handle);

    return ret;
}

FURI_ALWAYS_INLINE void drv2605l_enable(Drv2605l* instance) {
    furi_hal_gpio_write(instance->pin_en, true);
}

FURI_ALWAYS_INLINE void drv2605l_disable(Drv2605l* instance) {
    furi_hal_gpio_write(instance->pin_en, false);
}

//https://www.ti.com/lit/an/sloa189/sloa189.pdf?ts=1763909348509&ref_url=https%253A%252F%252Fwww.google.com%252F
bool drv2605l_auto_calibrate(Drv2605l* instance) {
    furi_check(instance);

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

    drv2605l_write_reg(instance, Drv2605lRegRatedVoltage, &rated_voltage_reg);
    drv2605l_write_reg(instance, Drv2605lRegOverdriveClamp, &overdrive_clamp_reg);
    drv2605l_write_reg(instance, Drv2605lRegFeedback, (uint8_t*)&feedback_reg);
    drv2605l_write_reg(instance, Drv2605lRegControl1, (uint8_t*)&control1_reg);
    drv2605l_write_reg(instance, Drv2605lRegControl2, (uint8_t*)&control2_reg);
    drv2605l_write_reg(instance, Drv2605lRegControl3, (uint8_t*)&control3_reg);
    drv2605l_write_reg(instance, Drv2605lRegMode, (uint8_t*)&mode_reg);
    drv2605l_write_reg(instance, Drv2605lRegControl4, (uint8_t*)&control4_reg);
    drv2605l_write_reg(instance, Drv2605lRegGo, (uint8_t*)&go_reg);

    // Wait for completion
    uint32_t timeout = furi_get_tick() + 2000;
    while(furi_get_tick() < timeout) {
        uint16_t go_status = 0;
        drv2605l_read_reg(instance, Drv2605lRegGo, &go_status);
        Drv2605lGo* go_reg_status = (Drv2605lGo*)&go_status;
        if(go_reg_status->go_bit == 0) {
            break;
        }
        furi_delay_ms(10);
    }

    uint16_t status = 0;
    drv2605l_read_reg(instance, Drv2605lRegStatus, &status);
    Drv2605lStatus* status_reg = (Drv2605lStatus*)&status;

    if(status_reg->diagnostic_result) {
        FURI_LOG_E(TAG, "Auto-calibration failed");
        return false;
    }

    FURI_LOG_I(TAG, "Auto-calibration successful");

    //Calib reg 0x18, 0x19, 0x1A (BEMFGain)
    uint8_t calib_data = 0;

    drv2605l_read_reg(instance, Drv2605lRegAutoCalComp, (uint16_t*)&calib_data);
    FURI_LOG_I(TAG, "Auto Cal Compensation: reg 0x%02X -> 0x%02X", Drv2605lRegAutoCalComp, calib_data);
    drv2605l_read_reg(instance, Drv2605lRegAutoCalBemf, (uint16_t*)&calib_data);
    FURI_LOG_I(TAG, "Auto Cal BEMF: reg 0x%02X -> 0x%02X", Drv2605lRegAutoCalBemf, calib_data);
    drv2605l_read_reg(instance, Drv2605lRegFeedback, (uint16_t*)&calib_data);
    FURI_LOG_I(TAG, "Feedback: reg 0x%02X -> 0x%02X", Drv2605lRegFeedback, calib_data);

    return true;
}

Drv2605l* drv2605l_init(const FuriHalI2cBusHandle* i2c_handle, const GpioPin* pin_en, const GpioPin* pin_trigger, uint8_t address) {
    Drv2605l* instance = (Drv2605l*)malloc(sizeof(Drv2605l));
    instance->i2c_handle = i2c_handle;
    instance->pin_en = pin_en;
    instance->pin_trigger = pin_trigger;
    instance->address = address;

    furi_hal_gpio_init_simple(instance->pin_en, GpioModeOutputPushPull);
    furi_hal_gpio_write(instance->pin_en, true);

    //Todo: GpioModeOutputPushPull
    //furi_hal_gpio_init_simple(instance->pin_trigger, GpioModeOutputPushPull);

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_device_ready(instance->i2c_handle, instance->address, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret) {
        drv2605l_enable(instance);

        //Set LNA library
        Drv2605lLibSelect lib_select_reg = {
            .hi_z_mode = 0, //Normal mode
            .library_sel = 6, //LRA
        };
        drv2605l_write_reg(instance, Drv2605lRegLibSelect, (uint8_t*)&lib_select_reg);

        drv2605l_auto_calibrate(instance);

        //todo: implement drv2605l_enable / drv2605l_disable used
        // drv2605l_disable(instance);
    } else {
        FURI_LOG_E(TAG, "DRV2605L device not ready at address 0x%02X", instance->address);
        drv2605l_disable(instance);
        furi_hal_gpio_init_ex(instance->pin_en, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
        furi_hal_gpio_init_ex(instance->pin_trigger, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
        free(instance);
        return NULL;
    }

    return instance;
}

void drv2605l_deinit(Drv2605l* instance) {
    furi_check(instance);
    drv2605l_disable(instance);
    furi_hal_gpio_init_ex(instance->pin_en, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(instance->pin_trigger, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    free(instance);
}

void drv2605l_trigger_set_effect(Drv2605l* instance, Drv2605lModeTrigger trigger_mode, Drv2605lEffect effect_index) {
    furi_check(instance);

    Drv2605lMode mode_reg = {
        .device_reset = 0, //Normal operation
        .standby = 0, //Active mode
        .mode_select = trigger_mode, //Internal trigger mode
    };
    uint8_t wawe_seq_reg = Drv2605lEffectNone; //Stop

    drv2605l_write_reg(instance, Drv2605lRegMode, (uint8_t*)&mode_reg);
    drv2605l_write_reg(instance, Drv2605lRegWaveSeq0, (uint8_t*)&effect_index);
    drv2605l_write_reg(instance, Drv2605lRegWaveSeq1, (uint8_t*)&wawe_seq_reg);
}

void drv2605l_trigger_go(Drv2605l* instance) {
    furi_check(instance);

    Drv2605lGo go_reg = {
        .go_bit = 1, //Start effect
    };

    drv2605l_write_reg(instance, Drv2605lRegGo, (uint8_t*)&go_reg);
}

void drv2605l_trigger_set_effect_and_play(Drv2605l* instance, Drv2605lEffect effect_index) {
    furi_check(instance);

    Drv2605lMode mode_reg = {
        .device_reset = 0, //Normal operation
        .standby = 0, //Active mode
        .mode_select = Drv2605lModeTriggerGo, //Internal trigger mode
    };
    Drv2605lGo go_reg = {
        .go_bit = 1, //Start effect
    };
    uint8_t wawe_seq_reg = Drv2605lEffectNone; //Stop

    drv2605l_write_reg(instance, Drv2605lRegMode, (uint8_t*)&mode_reg);
    drv2605l_write_reg(instance, Drv2605lRegWaveSeq0, (uint8_t*)&effect_index);
    drv2605l_write_reg(instance, Drv2605lRegWaveSeq1, (uint8_t*)&wawe_seq_reg);
    drv2605l_write_reg(instance, Drv2605lRegGo, (uint8_t*)&go_reg);
}

void drv2605l_test_all_effects(Drv2605l* instance) {
    furi_check(instance);

    for(uint8_t i = Drv2605lEffectStrongClick_100; i <= Drv2605lEffectCountMax; i++) {
        FURI_LOG_I(TAG, "Playing effect %d", i);
        drv2605l_trigger_set_effect_and_play(instance, (Drv2605lEffect)(i));
        furi_delay_ms(1000);
    }
}
