#include "iqs7211e_reg.h"
#include "iqs7211e.h"
#include "IQS7211E_init.h"

#include <furi.h>
#include <furi_hal_gpio.h>
#include <furi_hal_i2c.h>
#include <pico/types.h>

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
    furi_assert(instance);
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

    if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
        FURI_LOG_E(TAG, "Failed to write reg 0x%02X", reg);
        iqs7211e_i2c_release(instance);
    } else {
        IQS7211E_DEBUG(TAG, "Wrote reg 0x%02X: %016b", reg, data);
    }

    return ret;
}

static FURI_ALWAYS_INLINE int iqs7211e_write_data(Iqs7211e* instance, Iqs7211eReg reg, uint8_t* data, size_t size, Iqs7211eI2cTransferType transfer_type) {
    furi_assert(instance);
    int ret;

    uint8_t* data_buff = malloc(size + 1);
    memcpy(data_buff + 1, data, size);
    data_buff[0] = reg;

    iqs7211e_i2c_acquire(instance);

    if(transfer_type == Iqs7211eI2cTransferTypeNext) {
        ret = furi_hal_i2c_master_tx_blocking_nostop(instance->i2c_handle, instance->address, data_buff, size + 1, FURI_HAL_I2C_TIMEOUT_US);
    } else {
        ret = furi_hal_i2c_master_tx_blocking(instance->i2c_handle, instance->address, data_buff, size + 1, FURI_HAL_I2C_TIMEOUT_US);
        instance->ready = false;
        iqs7211e_i2c_release(instance);
    }

    if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
        FURI_LOG_E(TAG, "Failed to write reg 0x%02X", reg);
        iqs7211e_i2c_release(instance);
    } else {
        IQS7211E_DEBUG(TAG, "Wrote reg 0x%02X: data block", reg);
    }

    return ret;
}

static FURI_ALWAYS_INLINE int iqs7211e_read_reg(Iqs7211e* instance, Iqs7211eReg reg, uint16_t* data, Iqs7211eI2cTransferType transfer_type) {
    furi_assert(instance);
    furi_assert(data);

    iqs7211e_i2c_acquire(instance);
    int ret = furi_hal_i2c_master_tx_blocking_nostop(instance->i2c_handle, instance->address, (uint8_t*)&reg, 1, FURI_HAL_I2C_TIMEOUT_US);
    if(!(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT)) {
        uint8_t buffer[2] = {0};
        if(transfer_type == Iqs7211eI2cTransferTypeNext) {
            ret = furi_hal_i2c_master_rx_blocking_nostop(instance->i2c_handle, instance->address, buffer, sizeof(buffer), FURI_HAL_I2C_TIMEOUT_US);
        } else {
            ret = furi_hal_i2c_master_rx_blocking(instance->i2c_handle, instance->address, buffer, sizeof(buffer), FURI_HAL_I2C_TIMEOUT_US);
            instance->ready = false;
            iqs7211e_i2c_release(instance);
        }
        if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
            FURI_LOG_E(TAG, "Failed to read reg 0x%02X", reg);
            iqs7211e_i2c_release(instance);
        } else {
            *data = buffer[0] | (buffer[1] << 8);
        }
    } else {
        FURI_LOG_E(TAG, "Failed to write reg address 0x%02X for reading", reg);
        iqs7211e_i2c_release(instance);
    }

    return ret;
}

bool iqs7211e_read_data(Iqs7211e* instance) {
    furi_check(instance);
    if(!instance->ready) return false;

    bool ok = false;
    uint8_t reg = (uint8_t)Iqs7211eRegGestures;

    iqs7211e_i2c_acquire(instance);

    int ret = furi_hal_i2c_master_tx_blocking_nostop(instance->i2c_handle, instance->address, &reg, 1, FURI_HAL_I2C_TIMEOUT_US);

    if(!(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT)) {
        ret =
            furi_hal_i2c_master_rx_blocking(instance->i2c_handle, instance->address, (uint8_t*)&instance->data, sizeof(Iqs7211eData), FURI_HAL_I2C_TIMEOUT_US);
        instance->ready = false;
        iqs7211e_i2c_release(instance);

        if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
            FURI_LOG_E(TAG, "Failed to read data block");
        } else {
            ok = true;
        }
    } else {
        FURI_LOG_E(TAG, "Failed to write reg address 0x%02X for reading", Iqs7211eRegGestures);
        iqs7211e_i2c_release(instance);
    }
    return ok;
}

static void iqs7211e_load_config(Iqs7211e* instance) {
    furi_assert(instance);
    uint8_t load_config[30]; // Temporary array which holds the bytes to be transferred.

    /* Change the ALP ATI Compensation */
    /* Memory Map Position 0x1F - 0x20 */
    load_config[0] = ALP_COMPENSATION_A_0;
    load_config[1] = ALP_COMPENSATION_A_1;
    load_config[2] = ALP_COMPENSATION_B_0;
    load_config[3] = ALP_COMPENSATION_B_1;

    iqs7211e_write_data(instance, Iqs7211eRegAlpAtiCompA, load_config, 4, Iqs7211eI2cTransferTypeNext);
    IQS7211E_DEBUG(TAG, "\t\t1. Write ALP Compensation");

    /* Change the ATI Settings */
    /* Memory Map Position 0x21 - 0x27 */
    load_config[0] = TP_ATI_MULTIPLIERS_DIVIDERS_0;
    load_config[1] = TP_ATI_MULTIPLIERS_DIVIDERS_1;
    load_config[2] = TP_COMPENSATION_DIV;
    load_config[3] = TP_REF_DRIFT_LIMIT;
    load_config[4] = TP_ATI_TARGET_0;
    load_config[5] = TP_ATI_TARGET_1;
    load_config[6] = TP_MIN_COUNT_REATI_0;
    load_config[7] = TP_MIN_COUNT_REATI_1;
    load_config[8] = ALP_ATI_MULTIPLIERS_DIVIDERS_0;
    load_config[9] = ALP_ATI_MULTIPLIERS_DIVIDERS_1;
    load_config[10] = ALP_COMPENSATION_DIV;
    load_config[11] = ALP_LTA_DRIFT_LIMIT;
    load_config[12] = ALP_ATI_TARGET_0;
    load_config[13] = ALP_ATI_TARGET_1;

    iqs7211e_write_data(instance, Iqs7211eRegTpGlobalMirrors, load_config, 14, Iqs7211eI2cTransferTypeNext);
    IQS7211E_DEBUG(TAG, "\t\t2. Write ATI Settings");

    /* Change the RR and Timing Settings */
    /* Memory Map Position 0x28 - 0x32 */
    load_config[0] = ACTIVE_MODE_REPORT_RATE_0;
    load_config[1] = ACTIVE_MODE_REPORT_RATE_1;
    load_config[2] = IDLE_TOUCH_MODE_REPORT_RATE_0;
    load_config[3] = IDLE_TOUCH_MODE_REPORT_RATE_1;
    load_config[4] = IDLE_MODE_REPORT_RATE_0;
    load_config[5] = IDLE_MODE_REPORT_RATE_1;
    load_config[6] = LP1_MODE_REPORT_RATE_0;
    load_config[7] = LP1_MODE_REPORT_RATE_1;
    load_config[8] = LP2_MODE_REPORT_RATE_0;
    load_config[9] = LP2_MODE_REPORT_RATE_1;
    load_config[10] = ACTIVE_MODE_TIMEOUT_0;
    load_config[11] = ACTIVE_MODE_TIMEOUT_1;
    load_config[12] = IDLE_TOUCH_MODE_TIMEOUT_0;
    load_config[13] = IDLE_TOUCH_MODE_TIMEOUT_1;
    load_config[14] = IDLE_MODE_TIMEOUT_0;
    load_config[15] = IDLE_MODE_TIMEOUT_1;
    load_config[16] = LP1_MODE_TIMEOUT_0;
    load_config[17] = LP1_MODE_TIMEOUT_1;
    load_config[18] = REATI_RETRY_TIME;
    load_config[19] = REF_UPDATE_TIME;
    load_config[20] = I2C_TIMEOUT_0;
    load_config[21] = I2C_TIMEOUT_1;

    iqs7211e_write_data(instance, Iqs7211eRegActiveModeRr, load_config, 22, Iqs7211eI2cTransferTypeNext);
    IQS7211E_DEBUG(TAG, "\t\t3. Write Report rates and timings");

    /* Change the System Settings */
    /* Memory Map Position 0x33 - 0x35 */
    load_config[0] = SYSTEM_CONTROL_0;
    load_config[1] = SYSTEM_CONTROL_1;
    load_config[2] = CONFIG_SETTINGS0;
    load_config[3] = CONFIG_SETTINGS1;
    load_config[4] = OTHER_SETTINGS_0;
    load_config[5] = OTHER_SETTINGS_1;

    iqs7211e_write_data(instance, Iqs7211eRegSysControl, load_config, 6, Iqs7211eI2cTransferTypeNext);
    IQS7211E_DEBUG(TAG, "\t\t4. Write System control settings");

    /* Change the ALP Settings */
    /* Memory Map Position 0x36 - 0x37 */
    load_config[0] = ALP_SETUP_0;
    load_config[1] = ALP_SETUP_1;
    load_config[2] = ALP_TX_ENABLE_0;
    load_config[3] = ALP_TX_ENABLE_1;

    iqs7211e_write_data(instance, Iqs7211eRegAlpSetup, load_config, 4, Iqs7211eI2cTransferTypeNext);
    IQS7211E_DEBUG(TAG, "\t\t5. Write ALP Settings");

    /* Change the Threshold Settings */
    /* Memory Map Position 0x38 - 0x3A */
    load_config[0] = TRACKPAD_TOUCH_SET_THRESHOLD;
    load_config[1] = TRACKPAD_TOUCH_CLEAR_THRESHOLD;
    load_config[2] = ALP_THRESHOLD_0;
    load_config[3] = ALP_THRESHOLD_1;
    load_config[4] = ALP_SET_DEBOUNCE;
    load_config[5] = ALP_CLEAR_DEBOUNCE;

    iqs7211e_write_data(instance, Iqs7211eRegTpTouchSetClearThr, load_config, 6, Iqs7211eI2cTransferTypeNext);
    IQS7211E_DEBUG(TAG, "\t\t6. Write Threshold settings");

    /* Change the Button and ALP count and LTA betas */
    /* Memory Map Position 0x3B - 0x3C */
    load_config[0] = ALP_COUNT_BETA_LP1;
    load_config[1] = ALP_LTA_BETA_LP1;
    load_config[2] = ALP_COUNT_BETA_LP2;
    load_config[3] = ALP_LTA_BETA_LP2;

    iqs7211e_write_data(instance, Iqs7211eRegLp1Filters, load_config, 4, Iqs7211eI2cTransferTypeNext);
    IQS7211E_DEBUG(TAG, "\t\t7. Write Filter Betas");

    /* Change the Hardware Settings */
    /* Memory Map Position 0x3D - 0x40 */
    load_config[0] = TP_CONVERSION_FREQUENCY_UP_PASS_LENGTH;
    load_config[1] = TP_CONVERSION_FREQUENCY_FRACTION_VALUE;
    load_config[2] = ALP_CONVERSION_FREQUENCY_UP_PASS_LENGTH;
    load_config[3] = ALP_CONVERSION_FREQUENCY_FRACTION_VALUE;
    load_config[4] = TRACKPAD_HARDWARE_SETTINGS_0;
    load_config[5] = TRACKPAD_HARDWARE_SETTINGS_1;
    load_config[6] = ALP_HARDWARE_SETTINGS_0;
    load_config[7] = ALP_HARDWARE_SETTINGS_1;

    iqs7211e_write_data(instance, Iqs7211eRegTpConvFreq, load_config, 8, Iqs7211eI2cTransferTypeNext);
    IQS7211E_DEBUG(TAG, "\t\t8. Write Hardware settings");

    /* Change the TP Setup */
    /* Memory Map Position 0x41 - 0x49 */
    load_config[0] = TRACKPAD_SETTINGS_0_0;
    load_config[1] = TRACKPAD_SETTINGS_0_1;
    load_config[2] = TRACKPAD_SETTINGS_1_0;
    load_config[3] = TRACKPAD_SETTINGS_1_1;
    load_config[4] = X_RESOLUTION_0;
    load_config[5] = X_RESOLUTION_1;
    load_config[6] = Y_RESOLUTION_0;
    load_config[7] = Y_RESOLUTION_1;
    load_config[8] = XY_DYNAMIC_FILTER_BOTTOM_SPEED_0;
    load_config[9] = XY_DYNAMIC_FILTER_BOTTOM_SPEED_1;
    load_config[10] = XY_DYNAMIC_FILTER_TOP_SPEED_0;
    load_config[11] = XY_DYNAMIC_FILTER_TOP_SPEED_1;
    load_config[12] = XY_DYNAMIC_FILTER_BOTTOM_BETA;
    load_config[13] = XY_DYNAMIC_FILTER_STATIC_FILTER_BETA;
    load_config[14] = STATIONARY_TOUCH_MOV_THRESHOLD;
    load_config[15] = FINGER_SPLIT_FACTOR;
    load_config[16] = X_TRIM_VALUE;
    load_config[17] = Y_TRIM_VALUE;

    iqs7211e_write_data(instance, Iqs7211eRegTpRxSettings, load_config, 18, Iqs7211eI2cTransferTypeNext);
    IQS7211E_DEBUG(TAG, "\t\t9. Write TP Settings");

    /* Change the Settings Version Numbers */
    /* Memory Map Position 0x4A - 0x4A */
    load_config[0] = MINOR_VERSION;
    load_config[1] = MAJOR_VERSION;

    iqs7211e_write_data(instance, Iqs7211eRegSettingsVersion, load_config, 2, Iqs7211eI2cTransferTypeNext);
    IQS7211E_DEBUG(TAG, "\t\t10. Write Version numbers");

    /* Change the Gesture Settings */
    /* Memory Map Position 0x4B - 0x55 */
    load_config[0] = GESTURE_ENABLE_0;
    load_config[1] = GESTURE_ENABLE_1;
    load_config[2] = TAP_TOUCH_TIME_0;
    load_config[3] = TAP_TOUCH_TIME_1;
    load_config[4] = TAP_WAIT_TIME_0;
    load_config[5] = TAP_WAIT_TIME_1;
    load_config[6] = TAP_DISTANCE_0;
    load_config[7] = TAP_DISTANCE_1;
    load_config[8] = HOLD_TIME_0;
    load_config[9] = HOLD_TIME_1;
    load_config[10] = SWIPE_TIME_0;
    load_config[11] = SWIPE_TIME_1;
    load_config[12] = SWIPE_X_DISTANCE_0;
    load_config[13] = SWIPE_X_DISTANCE_1;
    load_config[14] = SWIPE_Y_DISTANCE_0;
    load_config[15] = SWIPE_Y_DISTANCE_1;
    load_config[16] = SWIPE_X_CONS_DIST_0;
    load_config[17] = SWIPE_X_CONS_DIST_1;
    load_config[18] = SWIPE_Y_CONS_DIST_0;
    load_config[19] = SWIPE_Y_CONS_DIST_1;
    load_config[20] = SWIPE_ANGLE;
    load_config[21] = PALM_THRESHOLD;

    iqs7211e_write_data(instance, Iqs7211eRegGestureEnable, load_config, 22, Iqs7211eI2cTransferTypeNext);
    IQS7211E_DEBUG(TAG, "\t\t11. Write Gesture Settings");

    /* Change the RxTx Mapping */
    /* Memory Map Position 0x56 - 0x5C */
    load_config[0] = RX_TX_MAP_0;
    load_config[1] = RX_TX_MAP_1;
    load_config[2] = RX_TX_MAP_2;
    load_config[3] = RX_TX_MAP_3;
    load_config[4] = RX_TX_MAP_4;
    load_config[5] = RX_TX_MAP_5;
    load_config[6] = RX_TX_MAP_6;
    load_config[7] = RX_TX_MAP_7;
    load_config[8] = RX_TX_MAP_8;
    load_config[9] = RX_TX_MAP_9;
    load_config[10] = RX_TX_MAP_10;
    load_config[11] = RX_TX_MAP_11;
    load_config[12] = RX_TX_MAP_12;
    load_config[13] = RX_TX_MAP_FILLER;

    iqs7211e_write_data(instance, Iqs7211eRegRxTxMapping0_1, load_config, 14, Iqs7211eI2cTransferTypeNext);
    IQS7211E_DEBUG(TAG, "\t\t12. Write Rx Tx Map Settings");

    /* Change the Allocation of channels into cycles 0-9 */
    /* Memory Map Position 0x5D - 0x6B */
    load_config[0] = PLACEHOLDER_0;
    load_config[1] = CH_1_CYCLE_0;
    load_config[2] = CH_2_CYCLE_0;
    load_config[3] = PLACEHOLDER_1;
    load_config[4] = CH_1_CYCLE_1;
    load_config[5] = CH_2_CYCLE_1;
    load_config[6] = PLACEHOLDER_2;
    load_config[7] = CH_1_CYCLE_2;
    load_config[8] = CH_2_CYCLE_2;
    load_config[9] = PLACEHOLDER_3;
    load_config[10] = CH_1_CYCLE_3;
    load_config[11] = CH_2_CYCLE_3;
    load_config[12] = PLACEHOLDER_4;
    load_config[13] = CH_1_CYCLE_4;
    load_config[14] = CH_2_CYCLE_4;
    load_config[15] = PLACEHOLDER_5;
    load_config[16] = CH_1_CYCLE_5;
    load_config[17] = CH_2_CYCLE_5;
    load_config[18] = PLACEHOLDER_6;
    load_config[19] = CH_1_CYCLE_6;
    load_config[20] = CH_2_CYCLE_6;
    load_config[21] = PLACEHOLDER_7;
    load_config[22] = CH_1_CYCLE_7;
    load_config[23] = CH_2_CYCLE_7;
    load_config[24] = PLACEHOLDER_8;
    load_config[25] = CH_1_CYCLE_8;
    load_config[26] = CH_2_CYCLE_8;
    load_config[27] = PLACEHOLDER_9;
    load_config[28] = CH_1_CYCLE_9;
    load_config[29] = CH_2_CYCLE_9;

    iqs7211e_write_data(instance, Iqs7211eRegProxaCycle0, load_config, 30, Iqs7211eI2cTransferTypeNext);
    IQS7211E_DEBUG(TAG, "\t\t13. Write Cycle 0 - 9 Settings");

    /* Change the Allocation of channels into cycles 10-19 */
    /* Memory Map Position 0x6C - 0x7A */
    load_config[0] = PLACEHOLDER_10;
    load_config[1] = CH_1_CYCLE_10;
    load_config[2] = CH_2_CYCLE_10;
    load_config[3] = PLACEHOLDER_11;
    load_config[4] = CH_1_CYCLE_11;
    load_config[5] = CH_2_CYCLE_11;
    load_config[6] = PLACEHOLDER_12;
    load_config[7] = CH_1_CYCLE_12;
    load_config[8] = CH_2_CYCLE_12;
    load_config[9] = PLACEHOLDER_13;
    load_config[10] = CH_1_CYCLE_13;
    load_config[11] = CH_2_CYCLE_13;
    load_config[12] = PLACEHOLDER_14;
    load_config[13] = CH_1_CYCLE_14;
    load_config[14] = CH_2_CYCLE_14;
    load_config[15] = PLACEHOLDER_15;
    load_config[16] = CH_1_CYCLE_15;
    load_config[17] = CH_2_CYCLE_15;
    load_config[18] = PLACEHOLDER_16;
    load_config[19] = CH_1_CYCLE_16;
    load_config[20] = CH_2_CYCLE_16;
    load_config[21] = PLACEHOLDER_17;
    load_config[22] = CH_1_CYCLE_17;
    load_config[23] = CH_2_CYCLE_17;
    load_config[24] = PLACEHOLDER_18;
    load_config[25] = CH_1_CYCLE_18;
    load_config[26] = CH_2_CYCLE_18;
    load_config[27] = PLACEHOLDER_19;
    load_config[28] = CH_1_CYCLE_19;
    load_config[29] = CH_2_CYCLE_19;

    iqs7211e_write_data(instance, Iqs7211eRegProxaCycle10, load_config, 30, Iqs7211eI2cTransferTypeNext);
    IQS7211E_DEBUG(TAG, "\t\t14. Write Cycle 10 - 19 Settings");

    /* Change the Allocation of channels into cycles 20 */
    /* Memory Map Position 0x7B - 0x7C */
    load_config[0] = PLACEHOLDER_20;
    load_config[1] = CH_1_CYCLE_20;
    load_config[2] = CH_2_CYCLE_20;

    iqs7211e_write_data(instance, Iqs7211eRegProxaCycle20, load_config, 3, Iqs7211eI2cTransferTypeStop);
    IQS7211E_DEBUG(TAG, "\t\t15. Write Cycle 20  Settings");
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
        uint16_t prod_num = 0;
        uint16_t ver_maj = 0;
        uint16_t ver_min = 0;
        iqs7211e_read_reg(instance, Iqs7211eRegProductNum, &prod_num, Iqs7211eI2cTransferTypeNext);
        iqs7211e_read_reg(instance, Iqs7211eRegMajorVersionNum, &ver_maj, Iqs7211eI2cTransferTypeNext);
        iqs7211e_read_reg(instance, Iqs7211eRegMinorVersionNum, &ver_min, Iqs7211eI2cTransferTypeStop);
        IQS7211E_DEBUG(TAG, "Init verify product");
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
        iqs7211e_read_reg(instance, Iqs7211eRegInfoFlags, (uint16_t*)&info_flags, Iqs7211eI2cTransferTypeNext);
        IQS7211E_DEBUG(TAG, "Init read reset");
        if(info_flags.show_reset) {
            IQS7211E_DEBUG(TAG, "\t\tReset event occurred.");
            instance->init_state = Iqs7211eInitStateUpdateSettings;
        } else {
            IQS7211E_DEBUG(TAG, "\t\t No Reset Event Detected - Request SW Reset");
            instance->init_state = Iqs7211eInitStateChipReset;
        }
        break;
    case Iqs7211eInitStateChipReset:
        Iqs7211eSysControl sys_control;
        iqs7211e_read_reg(instance, Iqs7211eRegSysControl, (uint16_t*)&sys_control, Iqs7211eI2cTransferTypeNext);
        sys_control.sw_reset = 1;
        iqs7211e_write_reg(instance, Iqs7211eRegSysControl, *(uint16_t*)&sys_control, Iqs7211eI2cTransferTypeStop);
        IQS7211E_DEBUG(TAG, "Init chip reset");
        IQS7211E_DEBUG(TAG, "\t\tSoftware Reset Bit Set.");
        instance->init_state = Iqs7211eInitStateReadReset;
        break;
    case Iqs7211eInitStateUpdateSettings:
        IQS7211E_DEBUG(TAG, "Init update settings");
        iqs7211e_load_config(instance);
        instance->init_state = Iqs7211eInitStateAckReset;
        break;
    case Iqs7211eInitStateAckReset:
        Iqs7211eSysControl sys_control_ack;
        iqs7211e_read_reg(instance, Iqs7211eRegSysControl, (uint16_t*)&sys_control_ack, Iqs7211eI2cTransferTypeNext);
        sys_control_ack.ack_reset = 1;
        iqs7211e_write_reg(instance, Iqs7211eRegSysControl, *(uint16_t*)&sys_control_ack, Iqs7211eI2cTransferTypeStop);
        IQS7211E_DEBUG(TAG, "Init ack reset");
        instance->init_state = Iqs7211eInitStateAti;
        break;
    case Iqs7211eInitStateAti:
        Iqs7211eSysControl sys_control_ati;
        iqs7211e_read_reg(instance, Iqs7211eRegSysControl, (uint16_t*)&sys_control_ati, Iqs7211eI2cTransferTypeNext);
        sys_control_ati.tp_re_ati = 1;
        iqs7211e_write_reg(instance, Iqs7211eRegSysControl, *(uint16_t*)&sys_control_ati, Iqs7211eI2cTransferTypeStop);
        IQS7211E_DEBUG(TAG, "Init ATI");
        instance->init_state = Iqs7211eInitStateWaitForAti;
        break;
    case Iqs7211eInitStateWaitForAti:
        Iqs7211eInfoFlags info_flags_ati;
        iqs7211e_read_reg(instance, Iqs7211eRegInfoFlags, (uint16_t*)&info_flags_ati, Iqs7211eI2cTransferTypeStop);
        IQS7211E_DEBUG(TAG, "Init wait for ATI");
        if(!info_flags_ati.re_ati_occurred) {
            IQS7211E_DEBUG(TAG, "\t\tATI done");
            instance->init_state = Iqs7211eInitStateReadData;
        }
        break;
    case Iqs7211eInitStateReadData:
        iqs7211e_read_data(instance);
        IQS7211E_DEBUG(TAG, "Init read data");
        instance->init_state = Iqs7211eInitStateActivateEventMode;
        break;
    case Iqs7211eInitStateActivateEventMode:
    case Iqs7211eInitStateActivateStreamMode: //Todo: EventMode ?==? StreamMode
        Iqs7211eConfigSettings config_settings;
        iqs7211e_read_reg(instance, Iqs7211eRegConfigSettings, (uint16_t*)&config_settings, Iqs7211eI2cTransferTypeNext);
        config_settings.event_mode = 1;
        iqs7211e_write_reg(instance, Iqs7211eRegConfigSettings, *(uint16_t*)&config_settings, Iqs7211eI2cTransferTypeStop);
        IQS7211E_DEBUG(TAG, "Init activate event mode");
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
        Iqs7211eInfoFlags info_flags = instance->data.info_flags;
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
            if(iqs7211e_read_data(instance)) {
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

void iqs7211e_force_I2C_communication(Iqs7211e* instance) {
    furi_check(instance);
    uint8_t reg = 0xFF;
    uint8_t buffer[1] = {0};
    iqs7211e_write_data(instance, reg, buffer, 0, Iqs7211eI2cTransferTypeNext);
}
void iqs7211e_reset(Iqs7211e* instance) {
    furi_check(instance);
    furi_hal_gpio_remove_int_callback(instance->pin_rdy);
    furi_hal_gpio_write_open_drain(instance->pin_rdy, false);\
    furi_delay_ms(20);
    furi_hal_gpio_write_open_drain(instance->pin_rdy, true);
    furi_delay_ms(20);
    furi_hal_gpio_add_int_callback(instance->pin_rdy, GpioConditionFall, iqs7211e_interrupt_handler, instance);
    instance->state = Iqs7211eStateStart;
    instance->init_state = Iqs7211eInitStateVerifyProduct;
}

Iqs7211e* iqs7211e_init(const FuriHalI2cBusHandle* i2c_handle, const GpioPin* pin_rdy, uint8_t address) {
    Iqs7211e* instance = (Iqs7211e*)malloc(sizeof(Iqs7211e));
    instance->i2c_handle = i2c_handle;
    instance->pin_rdy = pin_rdy;
    instance->address = address;
    furi_hal_gpio_init_simple(instance->pin_rdy, GpioModeOutputOpenDrain);
    furi_hal_gpio_add_int_callback(instance->pin_rdy, GpioConditionFall, iqs7211e_interrupt_handler, instance);
    iqs7211e_reset(instance);

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_device_ready(instance->i2c_handle, instance->address, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);
    if(ret) {
        FURI_LOG_I(TAG, "IQS7211E device ready at address 0x%02X", instance->address);
        instance->ready = false;
        while(1) {
            iqs7211e_run(instance);
        }

    } else {
        FURI_LOG_E(TAG, "IQS7211E device not ready at address 0x%02X", instance->address);
        furi_hal_gpio_remove_int_callback(instance->pin_rdy);
        furi_hal_gpio_init_ex(instance->pin_rdy, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
        free(instance);
        return NULL;
    }

    instance->state = Iqs7211eStateStart;
    instance->init_state = Iqs7211eInitStateVerifyProduct;

    return instance;
}

void iqs7211e_deinit(Iqs7211e* instance) {
    furi_check(instance);
    furi_hal_gpio_remove_int_callback(instance->pin_rdy);
    furi_hal_gpio_init_ex(instance->pin_rdy, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    free(instance);
}

void iqs7211e_set_input_callback(Iqs7211e* instance, Iqs7211eCallbackInput callback, void* context) {
    furi_check(instance);
    instance->input_callback = callback;
    instance->callback_context = context;
}
