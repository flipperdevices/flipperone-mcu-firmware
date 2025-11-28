#include "core/check.h"
#include "core/kernel.h"
#include "core/log.h"
#include "furi_hal_gpio.h"
#include "iqs7211e_reg.h"
#include "iqs7211e.h"
#include <furi.h>

#include <furi_hal_i2c.h>
#include <pico/types.h>
#include <stdbool.h>
#include <stdint.h>

#define TAG "Iqs7211e"

#define IQS7211E_DEBUG_ENABLE

#ifdef IQS7211E_DEBUG_ENABLE
#define IQS7211E_DEBUG(...) FURI_LOG_D(__VA_ARGS__)
#else
#define IQS7211E_DEBUG(...)
#endif

typedef enum {
    Iqs7211eI2cTransferTypeNext,
    Iqs7211eI2cTransferTypeStop,
} Iqs7211eI2cTransferType;

typedef enum {
    Iqs7211eInitStateNone = (uint8_t)0x00,
    Iqs7211eInitStateVerifyProduct,
    Iqs7211eInitStateReadReset,
    Iqs7211eInitStateChipReset,
    Iqs7211eInitStateUpdateSettings,
    Iqs7211eInitStateCheckReset,
    Iqs7211eInitStateAckReset,
    Iqs7211eInitStateAti,
    Iqs7211eInitStateWaitForAti,
    Iqs7211eInitStateReadData,
    Iqs7211eInitStateActivateEventMode,
    Iqs7211eInitStateActivateStreamMode,
    Iqs7211eInitStateDone,
} Iqs7211eInitState;

typedef enum {
    Iqs7211eStateNone = (uint8_t)0x00,
    Iqs7211eStateStart,
    Iqs7211eStateInit,
    Iqs7211eStateSwReset,
    Iqs7211eStateCheckReset,
    Iqs7211eStateRun,
} Iqs7211eState;

typedef struct {
    Iqs7211eGestures gesture;
    Iqs7211eInfoFlags info_flags;
    uint16_t f1_x_position;
    uint16_t f1_y_position;
    uint16_t f1_touch_strength;
    uint16_t f1_area;
    uint16_t f2_x_position;
    uint16_t f2_y_position;
    uint16_t f2_touch_strength;
    uint16_t f2_area;
} FURI_PACKED Iqs7211eData;
_Static_assert(sizeof(Iqs7211eData) == 20, "Size check for 'Iqs7211eData' failed");

struct Iqs7211e {
    const FuriHalI2cBusHandle* i2c_handle;
    const GpioPin* pin_rdy;
    uint8_t address;
    volatile bool ready;
    volatile bool i2c_session_active;
    Iqs7211eState state;
    Iqs7211eInitState init_state;
    Iqs7211eData data;
    Iqs7211eCallbackInput input_callback;
    void* callback_context;
};

static __isr __not_in_flash_func(void) iqs7211e_interrupt_handler(void* ctx) {
    Iqs7211e* instance = (Iqs7211e*)ctx;
    //instance->ready = !furi_hal_gpio_read(instance->pin_rdy);
    instance->ready = true;
    if(instance->input_callback) {
        instance->input_callback(instance->callback_context);
    }
}

static FURI_ALWAYS_INLINE void iqs7211e_i2c_acquire(Iqs7211e* instance) {
    if(!instance->i2c_session_active) {
        instance->i2c_session_active = true;
        furi_hal_i2c_acquire(instance->i2c_handle);
    }
}

static FURI_ALWAYS_INLINE void iqs7211e_i2c_release(Iqs7211e* instance) {
    if(instance->i2c_session_active) {
        instance->i2c_session_active = false;
        furi_hal_i2c_release(instance->i2c_handle);
    }
}

static FURI_ALWAYS_INLINE int iqs7211e_write_reg(Iqs7211e* instance, Iqs7211eReg reg, uint16_t data, Iqs7211eI2cTransferType transfer_type) {
    furi_check(instance);
    int ret;
    uint8_t buffer[3] = {reg, data & 0xFF, data >> 8};

    iqs7211e_i2c_acquire(instance);

    if(transfer_type == Iqs7211eI2cTransferTypeNext) {
        ret = furi_hal_i2c_master_tx_blocking_nostop(instance->i2c_handle, instance->address, buffer, sizeof(buffer), FURI_HAL_I2C_TIMEOUT_US);
    } else {
        ret = furi_hal_i2c_master_tx_blocking(instance->i2c_handle, instance->address, buffer, sizeof(buffer), FURI_HAL_I2C_TIMEOUT_US);
        instance->ready = false;
        iqs7211e_i2c_release(instance);
    }
    

    if(ret != PICO_ERROR_GENERIC) {
        FURI_LOG_D(TAG, "Wrote reg 0x%02X: %016b", reg, data);
    } else {
        FURI_LOG_E(TAG, "Failed to write reg 0x%02X", reg);
        iqs7211e_i2c_release(instance);
    }

    return ret;
}

static FURI_ALWAYS_INLINE int iqs7211e_read_reg(Iqs7211e* instance, Iqs7211eReg reg, uint16_t* data, Iqs7211eI2cTransferType transfer_type) {
    furi_check(instance);
    furi_check(data);
    
    iqs7211e_i2c_acquire(instance);
    int ret = furi_hal_i2c_master_tx_blocking_nostop(instance->i2c_handle, instance->address, (uint8_t*)&reg, 1, FURI_HAL_I2C_TIMEOUT_US);
    if(ret != PICO_ERROR_GENERIC) {
        uint8_t buffer[2] = {0};
        if(transfer_type == Iqs7211eI2cTransferTypeNext) {
            ret = furi_hal_i2c_master_rx_blocking_nostop(instance->i2c_handle, instance->address, buffer, sizeof(buffer), FURI_HAL_I2C_TIMEOUT_US);
        } else {
            ret = furi_hal_i2c_master_rx_blocking(instance->i2c_handle, instance->address, buffer, sizeof(buffer), FURI_HAL_I2C_TIMEOUT_US);
            instance->ready = false;
            iqs7211e_i2c_release(instance);
        }
        if(ret != PICO_ERROR_GENERIC) {
            *data = buffer[0] | (buffer[1] << 8);
        } else {
            FURI_LOG_E(TAG, "Failed to read reg 0x%02X", reg);
            iqs7211e_i2c_release(instance);
        }
    } else {
        FURI_LOG_E(TAG, "Failed to write reg address 0x%02X for reading", reg);
        iqs7211e_i2c_release(instance);
    }

    return ret;
}

bool iqs7211e_read_data(Iqs7211e* instance, Iqs7211eI2cTransferType transfer_type) {
    furi_check(instance);
    if(!instance->ready) return false;

    IQS7211E_DEBUG(TAG, "Reading data");
    bool ok = false;
    uint8_t reg = (uint8_t)Iqs7211eRegGestures;

    iqs7211e_i2c_acquire(instance);
    
    int ret = furi_hal_i2c_master_tx_blocking_nostop(instance->i2c_handle, instance->address, &reg, 1, FURI_HAL_I2C_TIMEOUT_US);

    if(ret != PICO_ERROR_GENERIC) {
        if(transfer_type == Iqs7211eI2cTransferTypeNext) {
            ret = furi_hal_i2c_master_rx_blocking_nostop(
                instance->i2c_handle, instance->address, (uint8_t*)&instance->data, sizeof(Iqs7211eData), FURI_HAL_I2C_TIMEOUT_US);
        } else {
            ret = furi_hal_i2c_master_rx_blocking(
                instance->i2c_handle, instance->address, (uint8_t*)&instance->data, sizeof(Iqs7211eData), FURI_HAL_I2C_TIMEOUT_US);
            instance->ready = false;
            iqs7211e_i2c_release(instance);
        }
        if(ret != PICO_ERROR_GENERIC) {
            ok = true;
        } else {
            FURI_LOG_E(TAG, "Failed to read data block");
            iqs7211e_i2c_release(instance);
        }
    } else {
        FURI_LOG_E(TAG, "Failed to write reg address 0x%02X for reading", Iqs7211eRegGestures);
    }
    iqs7211e_i2c_release(instance);
    return ok;
}

static bool iqs7211e_initialization(Iqs7211e* instance) {
    furi_check(instance);

    if(instance->init_state == Iqs7211eInitStateDone) {
        return true;
    }

    if(!instance->ready) {
        return false;
    }

    switch(instance->init_state) {
    case Iqs7211eInitStateVerifyProduct:
        IQS7211E_DEBUG(TAG, "Init verify product");
        uint16_t prod_num = 0;
        uint16_t ver_maj = 0;
        uint16_t ver_min = 0;
        iqs7211e_read_reg(instance, Iqs7211eRegProductNum, &prod_num, Iqs7211eI2cTransferTypeNext);
        iqs7211e_read_reg(instance, Iqs7211eRegMajorVersionNum, &ver_maj, Iqs7211eI2cTransferTypeNext);
        iqs7211e_read_reg(instance, Iqs7211eRegMinorVersionNum, &ver_min, Iqs7211eI2cTransferTypeStop);
        IQS7211E_DEBUG(TAG, "\t\tProduct number is: %d v%d.%d", prod_num, ver_maj, ver_min);
        if(prod_num == IQS7211E_PRODUCT_NUM) {
            IQS7211E_DEBUG(TAG, "\t\tIQS7211E Release UI Confirmed!");
            instance->init_state = Iqs7211eInitStateReadReset;
        } else {
            FURI_LOG_E(TAG, "\t\tDevice is not a IQS7211E! Read 0x%04X, need 0x%04X", prod_num, IQS7211E_PRODUCT_NUM);
            furi_crash();
        }
    case Iqs7211eInitStateReadReset:
        Iqs7211eInfoFlags info_flags;
        IQS7211E_DEBUG(TAG, "Init read reset");
        iqs7211e_read_reg(instance, Iqs7211eRegInfoFlags, (uint16_t*)&info_flags, Iqs7211eI2cTransferTypeNext);
        if(info_flags.show_reset) {
            IQS7211E_DEBUG(TAG, "\t\tReset event occurred.");
            instance->init_state = Iqs7211eInitStateUpdateSettings;
        } else {
            IQS7211E_DEBUG(TAG, "\t\t No Reset Event Detected - Request SW Reset");
            instance->init_state = Iqs7211eInitStateChipReset;
        }
        break;
    case Iqs7211eInitStateChipReset:
        IQS7211E_DEBUG(TAG, "Init chip reset");
        Iqs7211eSysControl sys_control;
        iqs7211e_read_reg(instance, Iqs7211eRegSysControl, (uint16_t*)&sys_control, Iqs7211eI2cTransferTypeNext);
        sys_control.sw_reset = 1;
        iqs7211e_write_reg(instance, Iqs7211eRegSysControl, *(uint16_t*)&sys_control, Iqs7211eI2cTransferTypeStop);
        IQS7211E_DEBUG(TAG, "\t\tSoftware Reset Bit Set.");
        instance->init_state = Iqs7211eInitStateReadReset;
        break;
    case Iqs7211eInitStateUpdateSettings:
        IQS7211E_DEBUG(TAG, "Init update settings");
        //todo: implement settings write
        instance->init_state = Iqs7211eInitStateAckReset;
        break;
    case Iqs7211eInitStateAckReset:
        IQS7211E_DEBUG(TAG, "Init ack reset");
        Iqs7211eSysControl sys_control_ack;
        iqs7211e_read_reg(instance, Iqs7211eRegSysControl, (uint16_t*)&sys_control_ack, Iqs7211eI2cTransferTypeNext);
        sys_control_ack.ack_reset = 1;
        iqs7211e_write_reg(instance, Iqs7211eRegSysControl, *(uint16_t*)&sys_control_ack, Iqs7211eI2cTransferTypeStop);
        instance->init_state = Iqs7211eInitStateAti;
        break;
    case Iqs7211eInitStateAti:
        IQS7211E_DEBUG(TAG, "Init ATI");
        Iqs7211eSysControl sys_control_ati;
        iqs7211e_read_reg(instance, Iqs7211eRegSysControl, (uint16_t*)&sys_control_ati, Iqs7211eI2cTransferTypeNext);
        sys_control_ati.tp_re_ati = 1;
        iqs7211e_write_reg(instance, Iqs7211eRegSysControl, *(uint16_t*)&sys_control_ati, Iqs7211eI2cTransferTypeStop);
        instance->init_state = Iqs7211eInitStateWaitForAti;
        break;
    case Iqs7211eInitStateWaitForAti:
        IQS7211E_DEBUG(TAG, "Init wait for ATI");
        Iqs7211eInfoFlags info_flags_ati;
        iqs7211e_read_reg(instance, Iqs7211eRegInfoFlags, (uint16_t*)&info_flags_ati, Iqs7211eI2cTransferTypeStop);
        if(!info_flags_ati.re_ati_occurred) {
            IQS7211E_DEBUG(TAG, "\t\tATI done");
            instance->init_state = Iqs7211eInitStateReadData;
        }
        break;
    case Iqs7211eInitStateReadData:
        IQS7211E_DEBUG(TAG, "Init read data");
        iqs7211e_read_data(instance, Iqs7211eI2cTransferTypeStop);
        instance->init_state = Iqs7211eInitStateActivateEventMode;
        break;
    case Iqs7211eInitStateActivateEventMode:
    case Iqs7211eInitStateActivateStreamMode: //Todo: EventMode ?==? StreamMode
        IQS7211E_DEBUG(TAG, "Init activate event mode");
        Iqs7211eConfigSettings config_settings;
        iqs7211e_read_reg(instance, Iqs7211eRegConfigSettings, (uint16_t*)&config_settings, Iqs7211eI2cTransferTypeNext);
        config_settings.event_mode = 1;
        iqs7211e_write_reg(instance, Iqs7211eRegConfigSettings, *(uint16_t*)&config_settings, Iqs7211eI2cTransferTypeStop);
        instance->init_state = Iqs7211eInitStateDone;
        break;
    }

    return false;
}

void iqs7211e_run(Iqs7211e* instance) {
    furi_check(instance);

    switch(instance->state) {
    case Iqs7211eStateStart:
        IQS7211E_DEBUG(TAG, "Initialization started");
        instance->state = Iqs7211eStateInit;
        break;

    case Iqs7211eStateInit:
        if(iqs7211e_initialization(instance)) {
            IQS7211E_DEBUG(TAG, "Initialization done");
            instance->state = Iqs7211eStateRun;
        }
        break;
    case Iqs7211eStateSwReset:
        if(instance->ready) {
            Iqs7211eSysControl sys_control;
            iqs7211e_read_reg(instance, Iqs7211eRegSysControl, (uint16_t*)&sys_control, Iqs7211eI2cTransferTypeNext);
            sys_control.sw_reset = 1;
            iqs7211e_write_reg(instance, Iqs7211eRegSysControl, *(uint16_t*)&sys_control, Iqs7211eI2cTransferTypeStop);
            instance->state = Iqs7211eStateRun;
        }
        break;
    case Iqs7211eStateCheckReset:
        Iqs7211eInfoFlags info_flags;
        iqs7211e_read_reg(instance, Iqs7211eRegInfoFlags, (uint16_t*)&info_flags, Iqs7211eI2cTransferTypeStop);
        if(info_flags.show_reset) {
            IQS7211E_DEBUG(TAG, "Reset occurred");
            instance->state = Iqs7211eStateStart;
            instance->init_state = Iqs7211eInitStateVerifyProduct;
        } else {
            IQS7211E_DEBUG(TAG, "No reset detected");
            instance->state = Iqs7211eStateRun;
        }
        break;
    case Iqs7211eStateRun:
        if(instance->ready) {
            if(iqs7211e_read_data(instance, Iqs7211eI2cTransferTypeStop)) {
                IQS7211E_DEBUG(TAG, "Data read successfully");
                // if(instance->input_callback) {
                //     instance->input_callback(instance->callback_context);
                // }
                instance->ready = false;
                instance->state = Iqs7211eStateCheckReset;
            }
        }
        break;
    }
}

Iqs7211e* iqs7211e_init(const FuriHalI2cBusHandle* i2c_handle, const GpioPin* pin_rdy, uint8_t address) {
    Iqs7211e* instance = (Iqs7211e*)malloc(sizeof(Iqs7211e));
    instance->i2c_handle = i2c_handle;
    instance->pin_rdy = pin_rdy;
    instance->address = address;

    // furi_hal_i2c_acquire(instance->i2c_handle);
    // int ret = furi_hal_i2c_device_ready(instance->i2c_handle, instance->address, FURI_HAL_I2C_TIMEOUT_US);
    // furi_hal_i2c_release(instance->i2c_handle);
    int ret = 1; // Temporary bypass of device ready check for testing
    if(ret) {
        FURI_LOG_I(TAG, "IQS7211E device ready at address 0x%02X", instance->address);
        furi_hal_gpio_init_simple(instance->pin_rdy, GpioModeInput);
        furi_hal_gpio_add_int_callback(instance->pin_rdy, GpioConditionFall, iqs7211e_interrupt_handler, instance);
        // uint16_t data_reg = 0x00FF;
        // iqs7211e_write_reg(instance, Iqs7211eRegProductNum, data_reg);
        // iqs7211e_read_reg(instance, Iqs7211eRegProductNum, &data_reg);
        // FURI_LOG_I(TAG, "IQS7211E Product Number: 0x%02X 0x%04X", Iqs7211eRegProductNum, data_reg);
        // iqs7211e_read_reg(instance, Iqs7211eRegMajorVersionNum, &data_reg);
        // FURI_LOG_I(TAG, "IQS7211E Product Number: 0x%02X 0x%04X", Iqs7211eRegMajorVersionNum, data_reg);
        // iqs7211e_read_reg(instance, Iqs7211eRegMinorVersionNum, &data_reg);
        // FURI_LOG_I(TAG, "IQS7211E Product Number: 0x%02X 0x%04X", Iqs7211eRegMinorVersionNum, data_reg);
        // iqs7211e_read_reg(instance, Iqs7211eRegPatchNum0, &data_reg);
        // FURI_LOG_I(TAG, "IQS7211E Product Number: 0x%02X 0x%04X", Iqs7211eRegPatchNum0, data_reg);
        // iqs7211e_read_reg(instance, Iqs7211eRegPatchNum1, &data_reg);
        // FURI_LOG_I(TAG, "IQS7211E Product Number: 0x%02X 0x%04X", Iqs7211eRegPatchNum1, data_reg);
        instance->state = Iqs7211eStateStart;
        instance->init_state = Iqs7211eInitStateVerifyProduct;

        instance->ready = false;

        while(1) {
            iqs7211e_run(instance);
        }

    } else {
        FURI_LOG_E(TAG, "IQS7211E device not ready at address 0x%02X", instance->address);
        free(instance);
        return NULL;
    }

    instance->state = Iqs7211eStateStart;
    instance->init_state = Iqs7211eInitStateVerifyProduct;

    return instance;
}

void iqs7211e_deinit(Iqs7211e* instance) {
    furi_check(instance);
    //furi_hal_gpio_remove_int_callback(instance->pin_rdy);
    furi_hal_gpio_init_ex(instance->pin_rdy, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    free(instance);
}

void iqs7211e_set_input_callback(Iqs7211e* instance, Iqs7211eCallbackInput callback, void* context) {
    furi_check(instance);
    instance->input_callback = callback;
    instance->callback_context = context;
}
