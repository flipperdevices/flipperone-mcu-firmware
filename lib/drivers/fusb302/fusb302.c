#include "fusb302_reg.h"
#include "fusb302.h"
#include <furi.h>

#include <furi_hal_i2c.h>
#include <pico/error.h>
#include <pico/types.h>

#define TAG "Fusb302"

#define FUSB302_DEBUG_ENABLE

#ifdef FUSB302_DEBUG_ENABLE
#define FUSB302_DEBUG(...) FURI_LOG_D(__VA_ARGS__)
#else
#define FUSB302_DEBUG(...)
#endif

struct Fusb302 {
    const FuriHalI2cBusHandle* i2c_handle;
    uint8_t address;
    const GpioPin* pin_interrupt;
    Fusb302Callback callback;
    void* context;
};

static Fusb302Status fusb302_check_status(int stataus) {
    Fusb302Status ret = Fusb302StatusUnknown;
    if(stataus >= PICO_OK) {
        ret = Fusb302StatusOk;
    } else if(stataus == PICO_ERROR_GENERIC) {
        ret = Fusb302StatusError;
    } else if(stataus == PICO_ERROR_TIMEOUT) {
        ret = Fusb302StatusTimeout;
    } else {
        ret = Fusb302StatusUnknown;
    }

    return ret;
}

static Fusb302Status fusb302_write_reg(Fusb302* instance, Fusb302Reg reg, uint8_t data) {
    furi_check(instance);

    uint8_t buffer[2] = {reg, data};

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_master_tx_blocking(instance->i2c_handle, instance->address, buffer, sizeof(buffer), FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
        FURI_LOG_E(TAG, "Failed to write reg 0x%02X", reg);
    } else {
        FUSB302_DEBUG(TAG, "Wrote reg 0x%02X: %08b", reg, data);
    }

    return fusb302_check_status(ret);
}

static Fusb302Status fusb302_read_reg(Fusb302* instance, Fusb302Reg reg, uint8_t* data) {
    furi_check(instance);
    furi_check(data);

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_master_tx_blocking_nostop(instance->i2c_handle, instance->address, (uint8_t*)&reg, 1, FURI_HAL_I2C_TIMEOUT_US);
    if(!(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT)) {
        uint8_t buffer[1] = {0};
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

    return fusb302_check_status(ret);
}

static Fusb302Status fusb302_write_buf(Fusb302* instance, Fusb302Reg reg, uint8_t* data, size_t length) {
    furi_check(instance);
    furi_check(data);
    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_master_tx_blocking_nostop(instance->i2c_handle, instance->address, (uint8_t*)&reg, 1, FURI_HAL_I2C_TIMEOUT_US);
    if(!(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT)) {
        ret = furi_hal_i2c_master_tx_blocking(instance->i2c_handle, instance->address, data, length, FURI_HAL_I2C_TIMEOUT_US);
        if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
            FURI_LOG_E(TAG, "Failed to write reg 0x%02X", reg);
        }
    } else {
        FURI_LOG_E(TAG, "Failed to write reg address 0x%02X for writing", reg);
    }
    furi_hal_i2c_release(instance->i2c_handle);

    return fusb302_check_status(ret);
}

static Fusb302Status fusb302_read_buf(Fusb302* instance, Fusb302Reg reg, uint8_t* data, size_t length) {
    furi_check(instance);
    furi_check(data);

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_master_tx_blocking_nostop(instance->i2c_handle, instance->address, (uint8_t*)&reg, 1, FURI_HAL_I2C_TIMEOUT_US);
    if(!(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT)) {
        ret = furi_hal_i2c_master_rx_blocking(instance->i2c_handle, instance->address, data, length, FURI_HAL_I2C_TIMEOUT_US);
        if(ret == PICO_ERROR_GENERIC || ret == PICO_ERROR_TIMEOUT) {
            FURI_LOG_E(TAG, "Failed to read reg 0x%02X", reg);
        }
    } else {
        FURI_LOG_E(TAG, "Failed to write reg address 0x%02X for reading", reg);
    }
    furi_hal_i2c_release(instance->i2c_handle);

    return fusb302_check_status(ret);
}

static __isr __not_in_flash_func(void) fusb302_interrupt_handler(void* ctx) {
    Fusb302* instance = (Fusb302*)ctx;
    if(instance->callback) {
        instance->callback(instance->context);
    }
    furi_log_puts("IRQ\r\n");
}

Fusb302Status fusb302_start_drp_logic(Fusb302* instance) {
    furi_check(instance);
    Fusb302Status res = Fusb302StatusUnknown;
    do {
        //1. reset the device to default state
        Fusb302ResetRegBits reset = {
            .sw_reset = 1,
        };
        res = fusb302_write_reg(instance, Fusb302RegReset, *(uint8_t*)&reset);
        if(res != Fusb302StatusOk) {
            break;
        }

        // 2. Enable power
        Fusb302PowerRegBits power = {0};
        power.pwr = 0b0111; // Enable all power blocks
        res = fusb302_write_reg(instance, Fusb302RegPower, *(uint8_t*)&power);
        if(res != Fusb302StatusOk) {
            break;
        }

        // 3. Enable DRP Toggle
        Fusb302Control2RegBits control2 = {0};
        res = fusb302_read_reg(instance, Fusb302RegControl2, (uint8_t*)&control2);
        if(res != Fusb302StatusOk) {
            break;
        }
        control2.toggle = 1; // Enable toggling between source and sink
        control2.mode = 0b01; // DRP mode
        res = fusb302_write_reg(instance, Fusb302RegControl2, *(uint8_t*)&control2);
        if(res != Fusb302StatusOk) {
            break;
        }

        // 4. Configure interrupts
        Fusb302MaskRegBits mask = {0};
        mask.m_bc_lvl = 0; // Mask BC_LVL change interrupts
        mask.m_collision = 1; // Mask collision interrupts
        mask.m_wake = 1; // Mask wake interrupts
        mask.m_alert = 1; // Mask alert interrupts
        mask.m_crc_chk = 1; // Mask CRC check interrupts
        mask.m_comp_chng = 0; // Mask comparator change interrupts
        mask.m_activity = 1; // Mask activity interrupts
        mask.m_vbusok = 0; // Mask VBUS OK interrupts
        res = fusb302_write_reg(instance, Fusb302RegMask, *(uint8_t*)&mask);
        if(res != Fusb302StatusOk) {
            break;
        }

        Fusb302MaskARegBits mask_a = {0};
        mask_a.m_hardrst = 1; // Mask hard reset interrupts
        mask_a.m_softrst = 1; // Mask soft reset interrupts
        mask_a.m_txsent = 1; // Mask transmit sent interrupts
        mask_a.m_hardsent = 1; // Mask hard reset sent interrupts
        mask_a.m_retryfail = 1; // Mask retry fail interrupts
        mask_a.m_softfail = 1; // Mask soft reset fail interrupts
        mask_a.m_togdone = 0; // Mask toggle done interrupts
        mask_a.m_ocp_temp = 1; // Mask over-current/temperature interrupts
        res = fusb302_write_reg(instance, Fusb302RegMaskA, *(uint8_t*)&mask_a);
        if(res != Fusb302StatusOk) {
            break;
        }

        Fusb302MaskBRegBits mask_b = {0};
        mask_b.m_gcrcsent = 1; // Mask GoodCRC sent interrupts
        res = fusb302_write_reg(instance, Fusb302RegMaskB, *(uint8_t*)&mask_b);
        if(res != Fusb302StatusOk) {
            break;
        }

        // Clear any pending interrupts
        uint8_t irq;
        res = fusb302_read_reg(instance, Fusb302RegInterrupt, &irq);
        if(res != Fusb302StatusOk) {
            break;
        }
        res = fusb302_read_reg(instance, Fusb302RegInterruptA, &irq);
        if(res != Fusb302StatusOk) {
            break;
        }
        res = fusb302_read_reg(instance, Fusb302RegInterruptB, &irq);
        if(res != Fusb302StatusOk) {
            break;
        }

        // 5. Enable interrupts and host current
        Fusb302Control0RegBits control0 = {0};
        res = fusb302_read_reg(instance, Fusb302RegControl0, (uint8_t*)&control0);
        if(res != Fusb302StatusOk) {
            break;
        }
        control0.int_mask = 0; // Enable interrupts on INT pin
        control0.host_cur = 0b01; // Set default USB power (80mA)
        res = fusb302_write_reg(instance, Fusb302RegControl0, *(uint8_t*)&control0);
        if(res != Fusb302StatusOk) {
            break;
        }
    } while(0);
    if(res != Fusb302StatusOk) {
        FURI_LOG_E(TAG, "Failed to start DRP logic!");
    }
    return res;
}

Fusb302* fusb302_init(const FuriHalI2cBusHandle* i2c_handle, uint8_t address, const GpioPin* pin_interrupt) {
    Fusb302* instance = (Fusb302*)malloc(sizeof(Fusb302));
    instance->i2c_handle = i2c_handle;
    instance->address = address;
    instance->pin_interrupt = pin_interrupt;

    furi_hal_i2c_acquire(instance->i2c_handle);
    int ret = furi_hal_i2c_device_ready(instance->i2c_handle, instance->address, FURI_HAL_I2C_TIMEOUT_US);
    furi_hal_i2c_release(instance->i2c_handle);

    if(ret) {
        FURI_LOG_I(TAG, "FUSB302 device ready at address 0x%02X", instance->address);
        if(instance->pin_interrupt) {
            furi_hal_gpio_init_simple(instance->pin_interrupt, GpioModeInput);
            furi_hal_gpio_add_int_callback(instance->pin_interrupt, GpioConditionFall, fusb302_interrupt_handler, instance);
        }

        Fusb302DeviceIdRegBits device_id = {0};
        fusb302_read_reg(instance, Fusb302RegDeviceId, (uint8_t*)&device_id);
        FUSB302_DEBUG(TAG, "Version ID: %02X, Product ID: %02X", device_id.version_id, device_id.product_id);

        fusb302_start_drp_logic(instance);

    } else {
        FURI_LOG_E(TAG, "FUSB302 device not ready at address 0x%02X", instance->address);
        free(instance);
        return NULL;
    }

    return instance;
}

void fusb302_deinit(Fusb302* instance) {
    furi_check(instance);
    if(instance->pin_interrupt) {
        furi_hal_gpio_remove_int_callback(instance->pin_interrupt);
        furi_hal_gpio_init_ex(instance->pin_interrupt, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    }
    free(instance);
}

Fusb302Status fusb302_pd_reset(Fusb302* instance) {
    furi_check(instance);
    Fusb302ResetRegBits reset = {
        .pd_reset = 1,
    };
    Fusb302Status res = fusb302_write_reg(instance, Fusb302RegReset, *(uint8_t*)&reset);
    if(res != Fusb302StatusOk) {
        FURI_LOG_E(TAG, "Failed to reset PD logic!");
    }
    return res;
}

bool fusb302_read_role(Fusb302* instance) {
    // Read interrupts to determine what happened
    Fusb302InterruptRegBits irq_bits = {0};
    Fusb302InterruptARegBits irq_a_bits = {0};
    bool ret = false;
    fusb302_read_reg(instance, Fusb302RegInterruptA, (uint8_t*)&irq_a_bits);
    fusb302_read_reg(instance, Fusb302RegInterrupt, (uint8_t*)&irq_bits);
    FUSB302_DEBUG(TAG, "Interrupt A: %02X", *(uint8_t*)&irq_a_bits);
    FUSB302_DEBUG(TAG, "Interrupt: %02X", *(uint8_t*)&irq_bits);
    if(irq_a_bits.i_tog_done) { // Checking I_TOGDONE bit (bit 6)
        // Read status to determine the current role

        Fusb302Status1ARegBits status1a_bits;
        fusb302_read_reg(instance, Fusb302RegStatus1A, (uint8_t*)&status1a_bits);
        Fusb302TypeCcOrientation orientation = Fusb302TypeCcOrientationNone;
        switch(status1a_bits.togss) {
        case FUSB302_STATUS1A_TOGSS_SRCON_CC1: // 001 - Source on CC1
            FUSB302_DEBUG(TAG, "Role determined: SOURCE CC1\n");
            //todo: need to turn on VBUS through external GPIO
            break;
        case FUSB302_STATUS1A_TOGSS_SRCON_CC2: // 010 - Source on CC2
            FUSB302_DEBUG(TAG, "Role determined: SOURCE CC2\n");
            //todo: need to turn on VBUS through external GPIO
            break;

        case FUSB302_STATUS1A_TOGSS_SNKON_CC1: // 101 - Sink on CC1
            FUSB302_DEBUG(TAG, "Role determined: SINK CC1\n");
            orientation = Fusb302TypeCcOrientationNormal;
            //todo: need to wait for VBUS from partner
            break;
        case FUSB302_STATUS1A_TOGSS_SNKON_CC2: // 110 - Sink on CC2
            FUSB302_DEBUG(TAG, "Role determined: SINK CC2\n");
            orientation = Fusb302TypeCcOrientationReverse;
            //todo: need to wait for VBUS from partner
            break;

        case FUSB302_STATUS1A_TOGSS_AUDIO_ACCESSORY: // 111 - Audio Accessory
            FUSB302_DEBUG(TAG, "Role determined: Audio Accessory (Not supported)\n");
            break;

        default:
            FUSB302_DEBUG(TAG, "Toggling still in progress or unknown...\n");
            break;
        }
        ret = true;

    } else if(irq_bits.i_comp_chng) { // Checking I_COMP_CHNG bit (bit 5)
        FUSB302_DEBUG(TAG, "FUSB302_INTERRUPT_MASK_COMP_CHNG\n");
        fusb302_start_drp_logic(instance);

    } else if(irq_bits.i_vbusok) { // Checking I_COMP_CHNG bit (bit 5)
        FUSB302_DEBUG(TAG, "FUSB302_INTERRUPT_MASK_VBUSOK\n");
        fusb302_start_drp_logic(instance);

    } else {
        // FUSB302_DEBUG(TAG, "Toggle not completed yet...\n");
    }
    return ret;
}

void fusb302_set_input_callback(Fusb302* instance, Fusb302Callback callback, void* context) {
    furi_check(instance);
    instance->callback = callback;
    instance->context = context;
}

/**
 * @brief Configures the FUSB302 for a specific USB Type-C cable orientation.
 *
 * This function:
 * - Configures RX by setting appropriate MEAS_CC1 or MEAS_CC2 bits
 * - Configures TX by setting appropriate TX_CC1 or TX_CC2 bits
 * - May interfere with DFP/SRC functionality
 *
 * The orientation parameter must be either Fusb302TypeCcOrientationNormal
 * or Fusb302TypeCcOrientationReverse.
 *
 * @param[in] orientation The cable orientation to configure (normal or reversed)
 * @return Fusb302StatusOk on successful configuration, or a negative error code otherwise
 */
Fusb302Status fusb302_cc_orientation_set(Fusb302* instance, Fusb302TypeCcOrientation orientation) {
    furi_check(instance);
    furi_check(orientation == Fusb302TypeCcOrientationNormal || orientation == Fusb302TypeCcOrientationReverse);

    Fusb302Status res = Fusb302StatusUnknown;
    do {
        /* Handle RX by setting MEAS_CC1 or MEAS_CC2
         * @note Interfers with DFP/SRC code */
        Fusb302Switches0RegBits switches0_bits = {0};
        res = fusb302_read_reg(instance, Fusb302RegSwitches0, (uint8_t*)&switches0_bits);
        if(res != Fusb302StatusOk) {
            break;
        }
        if(orientation == Fusb302TypeCcOrientationNormal) {
            switches0_bits.meas_cc1 = 0b1; // Measure CC1
            switches0_bits.meas_cc2 = 0b0; // Measure CC2
        } else {
            switches0_bits.meas_cc1 = 0b0; // Measure CC1
            switches0_bits.meas_cc2 = 0b1; // Measure CC2
        }

        res = fusb302_write_reg(instance, Fusb302RegSwitches0, *(uint8_t*)&switches0_bits);
        if(res != Fusb302StatusOk) {
            break;
        }

        /* Handle TX by setting TX_CC1 or TX_CC2 */
        uint8_t reg_switches1;
        Fusb302Switches1RegBits switches1_bits = {0};
        res = fusb302_read_reg(instance, Fusb302RegSwitches1, (uint8_t*)&switches1_bits);
        if(res != Fusb302StatusOk) {
            break;
        }

        if(orientation == Fusb302TypeCcOrientationNormal) {
            switches1_bits.tx_cc1 = 0b1; // Enable TX on CC1
            switches1_bits.tx_cc2 = 0b0; // Disable TX on CC2
        } else {
            switches1_bits.tx_cc1 = 0b0; // Disable TX on CC1
            switches1_bits.tx_cc2 = 0b1; // Enable TX on CC2
        }
        res = fusb302_write_reg(instance, Fusb302RegSwitches1, *(uint8_t*)&switches1_bits);

    } while(false);
    if(res != Fusb302StatusOk) {
        FURI_LOG_E(TAG, "Failed to set CC orientation!");
    }
    return res;
}

//////////////////////PD//////////////////////
/**
 * @brief Resets the USB Power Delivery logic of the FUSB302.
 *
 * Performs a soft reset of only the PD logic block, leaving other device
 * functionality unaffected. This is useful for recovering from protocol
 * errors or reinitializing PD communication.
 *
 * @return Fusb302StatusOk on successful reset, or a negative error code otherwise
 */
Fusb302Status fusb302_pd_reset_logic(Fusb302* instance) {
    furi_check(instance);
    Fusb302ResetRegBits reset = {
        .pd_reset = 1,
    };
    Fusb302Status res = fusb302_write_reg(instance, Fusb302RegReset, *(uint8_t*)&reset);
    if(res != Fusb302StatusOk) {
        FURI_LOG_E(TAG, "Failed to reset PD logic!");
    }
    return res;
}

/**
 * @brief Sends a USB Power Delivery Hard Reset sequence.
 *
 * @return Fusb302StatusOk on successful transmission, or a negative error code otherwise
 * @see Section 6.6.3.3 of the USB Power Delivery Specification
 */
Fusb302Status fusb302_pd_reset_hard(Fusb302* instance) {
    furi_check(instance);
    Fusb302Status res = Fusb302StatusUnknown;
    Fusb302Control3RegBits control3_bits = {0};
    do {
        res = fusb302_read_reg(instance, Fusb302RegControl3, (uint8_t*)&control3_bits);
        if(res != Fusb302StatusOk) {
            break;
        }
        control3_bits.send_hard_reset = 1;
        res = fusb302_write_reg(instance, Fusb302RegControl3, *(uint8_t*)&control3_bits);
    } while(false);
    if(res != Fusb302StatusOk) {
        FURI_LOG_E(TAG, "Failed to send hard reset!");
    }
    return res;
}

/**
 * @brief Enables or disables automatic GoodCRC response for received USB PD messages.
 *
 * When enabled, the FUSB302 will automatically send a GoodCRC message in
 * response to any valid received PD message. This is required for normal
 * USB PD operation according to the specification.
 *
 * @param[in] enabled true to enable automatic GoodCRC response, false to disable
 * @return Fusb302StatusOk on successful configuration, or a negative error code otherwise
 */
Fusb302Status fusb302_pd_autogoodcrc_set(Fusb302* instance, bool enabled) {
    furi_check(instance);
    Fusb302Status res = Fusb302StatusUnknown;
    Fusb302Switches1RegBits switches1_bits = {0};
    do {
        res = fusb302_read_reg(instance, Fusb302RegSwitches1, (uint8_t*)&switches1_bits);
        if(res != Fusb302StatusOk) {
            break;
        }
        switches1_bits.auto_crc = enabled ? 1 : 0;
        res = fusb302_write_reg(instance, Fusb302RegSwitches1, *(uint8_t*)&switches1_bits);
    } while(false);
    if(res != Fusb302StatusOk) {
        FURI_LOG_E(TAG, "Failed to program auto GoodCRC setting!");
    }
    return res;
}

/**
 * @brief Configures automatic retry behavior for USB PD message transmission.
 *
 * This function configures how many times the FUSB302 will automatically
 * retry sending a message if no GoodCRC response is received. Setting
 * retries to 0 disables automatic retries.
 *
 * The retries parameter must be between 0 and 3 inclusive.
 *
 * @param[in] retries Number of retry attempts (0-3) for message transmission
 * @return Fusb302StatusOk on successful configuration, or a negative error code otherwise
 */
Fusb302Status fusb302_pd_autoretry_set(Fusb302* instance, int retries) {
    furi_check(instance);
    furi_check(retries >= 0 && retries <= 3);
    Fusb302Status res = Fusb302StatusUnknown;
    Fusb302Control3RegBits control3_bits = {0};
    do {
        res = fusb302_read_reg(instance, Fusb302RegControl3, (uint8_t*)&control3_bits);
        if(res != Fusb302StatusOk) {
            break;
        }
        control3_bits.n_retries = retries & 0b111; // Set the auto retry bits (bits 3:1)
        res = fusb302_write_reg(instance, Fusb302RegControl3, *(uint8_t*)&control3_bits);
    } while(false);
    if(res != Fusb302StatusOk) {
        FURI_LOG_E(TAG, "Failed to program auto retry setting!");
    }
    return res;
}

/**
 * @brief Flushes the USB PD receive FIFO buffer.
 *
 * Clears any pending received messages from the RX FIFO and waits for
 * the clear operation to complete. 
 *
 * @return Fusb302StatusOk on successful flush operation, or a negative error code otherwise
 */
Fusb302Status fusb302_pd_rx_flush(Fusb302* instance) {
    furi_check(instance);
    Fusb302Status res = Fusb302StatusUnknown;
    Fusb302Control1RegBits control1_bits = {0};
    do {
        res = fusb302_read_reg(instance, Fusb302RegControl1, (uint8_t*)&control1_bits);
        if(res != Fusb302StatusOk) {
            break;
        }
        control1_bits.rx_flush = 1;
        res = fusb302_write_reg(instance, Fusb302RegControl1, *(uint8_t*)&control1_bits);
        if(res != Fusb302StatusOk) {
            break;
        }
        // wait operation
        uint8_t timeout = 100;
        while(timeout--) {
            res = fusb302_read_reg(instance, Fusb302RegControl1, (uint8_t*)&control1_bits);
            if(res != Fusb302StatusOk) {
                break;
            }
            if(control1_bits.rx_flush == 0) {
                res = Fusb302StatusOk;
                break;
            }
        }
        if(timeout == 0) {
            res = Fusb302StatusTimeout;
        }

    } while(false);
    if(res != Fusb302StatusOk) {
        FURI_LOG_E(TAG, "Failed to flush rx fifo!");
    }
    return res;
}

/**
 * @brief Flushes the USB PD transmit FIFO buffer.
 *
 * Clears any pending messages from the TX FIFO and waits for the clear
 * operation to complete. 
 *
 * @return Fusb302StatusOk on successful flush operation, or a negative error code otherwise
 */
Fusb302Status fusb302_pd_tx_flush(Fusb302* instance) {
    furi_check(instance);
    Fusb302Status res = Fusb302StatusUnknown;
    Fusb302Control0RegBits control0_bits = {0};
    do {
        res = fusb302_read_reg(instance, Fusb302RegControl0, (uint8_t*)&control0_bits);
        if(res != Fusb302StatusOk) {
            break;
        }
        control0_bits.tx_flush = 1;
        res = fusb302_write_reg(instance, Fusb302RegControl0, *(uint8_t*)&control0_bits);
        if(res != Fusb302StatusOk) {
            break;
        }
        // wait operation
        uint8_t timeout = 100;
        while(timeout--) {
            res = fusb302_read_reg(instance, Fusb302RegControl0, (uint8_t*)&control0_bits);
            if(res != Fusb302StatusOk) {
                break;
            }
            if(control0_bits.tx_flush == 0) {
                res = Fusb302StatusOk;
                break;
            }
            furi_delay_ms(10);
        }
        if(timeout == 0) {
            res = Fusb302StatusTimeout;
        }

    } while(false);
    if(res != Fusb302StatusOk) {
        FURI_LOG_E(TAG, "Failed to flush tx fifo!");
    }
    return res;
}

/**
 * @brief Attempts to receive a USB Power Delivery message from the FUSB302.
 *
 * This function:
 * - Checks if data is available in the RX FIFO
 * - Reads the SOP type
 * - Extracts and parses the message header
 * - Retrieves the message payload if present
 *
 * @param[out] msg Reference to a power delivery message structure where the received message will be stored
 * @return Fusb302StatusOk if a message was successfully received
 *         Fusb302StatusRxEmpty if no message was available to receive
 *         negative error code if an error occurred during reception
 */
Fusb302Status fusb302_pd_message_receive(Fusb302* instance, Fusb302PdMsg* msg) {
    uint8_t buf[32];

    furi_check(instance);
    Fusb302Status res = Fusb302StatusUnknown;
    Fusb302Status1RegBits status1_bits = {0};
    do {
        res = fusb302_read_reg(instance, Fusb302RegControl0, (uint8_t*)&status1_bits);
        if(res != Fusb302StatusOk) {
            break;
        }
        if(status1_bits.rx_empty) {
            res = Fusb302StatusRxEmpty;
            break;
        }
        /* Read the first three bytes to extract the sop and the header */
        res = fusb302_read_buf(instance, Fusb302RegFifos, buf, 3);
        if(res != Fusb302StatusOk) {
            break;
        }
        /* Determine the type of sop */
        uint8_t sop = buf[0] & FUSB302_RX_TOKEN_SOP_MASK;
        switch(sop) {
        case FUSB302_RX_TOKEN_SOP:
            msg->sop_type = Fusb302PdSopTypeDefault;
            break;
        case FUSB302_RX_TOKEN_SOP1:
            msg->sop_type = Fusb302PdSopTypePrime;
            break;
        case FUSB302_RX_TOKEN_SOP2:
            msg->sop_type = Fusb302PdSopTypePrimeDouble;
            break;
        case FUSB302_RX_TOKEN_SOP1DB:
            msg->sop_type = Fusb302PdSopTypeDebug;
            break;
        case FUSB302_RX_TOKEN_SOP2DB:
            msg->sop_type = Fusb302PdSopTypeDebugDouble;
            break;
        default:
            msg->sop_type = Fusb302PdSopTypeUnknown;
            break;
        }

        /* Parse header */
        msg->header = (buf[2] << 8) | (buf[1] << 0);
        msg->object_count = (msg->header & 0x7000) >> 12;

        /* Read the payload */
        furi_check(msg->object_count * 4 + 4 <= sizeof(buf), "Buffer too small for received message!");
        res = fusb302_read_buf(instance, Fusb302RegFifos, buf, msg->object_count * 4 + 4);
        if(res != Fusb302StatusOk) {
            break;
        }

        /* Retrieve objects */
        for(uint8_t i = 0; i < msg->object_count; i++) {
            msg->objects[i] = buf[i * 4 + 3];
            msg->objects[i] <<= 8;
            msg->objects[i] |= buf[i * 4 + 2];
            msg->objects[i] <<= 8;
            msg->objects[i] |= buf[i * 4 + 1];
            msg->objects[i] <<= 8;
            msg->objects[i] |= buf[i * 4 + 0];
        }
    } while(false);
    if(res != Fusb302StatusOk) {
        FURI_LOG_E(TAG, "Failed to receive message!");
    }
    return res;
}

/**
 * @brief Sends a USB Power Delivery message through the FUSB302.
 *
 * This function handles the complete message transmission process including:
 * - Adding SOP sequence (supports SOP, SOP', and SOP'' based on msg.sop_type)
 * - Formatting header and payload
 * - Adding CRC and EOP sequence
 * - Retrying on transmission failures
 *
 * @param[in] msg The power delivery message structure to send
 * @return 0 if the message was successfully sent, or a negative error code otherwise
 * @see Section 5.6 of the USB Power Delivery Specification Revision 3.0, Version 1.1
 * @see Table 29 of the FUSB302 datasheet, Revision 2
 */
Fusb302Status fusb302_pd_message_send(Fusb302* instance, Fusb302PdMsg* msg) {
    furi_check(instance);
    furi_check(msg);
    furi_check(msg->object_count <= 7, "Too many data objects in message!");
    Fusb302Status res = Fusb302StatusUnknown;
    //check if tx fifo is empty
    do {
        uint8_t timeout = 100;
        while(timeout--) {
            Fusb302Status1RegBits status1_bits = {0};
            res = fusb302_read_reg(instance, Fusb302RegStatus1, (uint8_t*)&status1_bits);
            if(res != Fusb302StatusOk) {
                break;
            }
            if(status1_bits.tx_empty) {
                res = Fusb302StatusOk;
                break;
            }
            furi_delay_ms(10);
        }
        if(timeout == 0) {
            res = Fusb302StatusTimeout;
            break;
        }

        /* Prepare buffer */
        uint8_t buf[5 + 2 + 4 * 7 + 4] = {0};
        uint8_t len = 0;

        /* Append start of packet sequence */
        switch(msg->sop_type) {
        case Fusb302PdSopTypeDefault:
            buf[len++] = FUSB302_TX_TOKEN_SYNC1;
            buf[len++] = FUSB302_TX_TOKEN_SYNC1;
            buf[len++] = FUSB302_TX_TOKEN_SYNC1;
            buf[len++] = FUSB302_TX_TOKEN_SYNC2;
            break;
        case Fusb302PdSopTypePrime:
            buf[len++] = FUSB302_TX_TOKEN_SYNC1;
            buf[len++] = FUSB302_TX_TOKEN_SYNC1;
            buf[len++] = FUSB302_TX_TOKEN_SYNC3;
            buf[len++] = FUSB302_TX_TOKEN_SYNC3;
            break;
        case Fusb302PdSopTypePrimeDouble:
            buf[len++] = FUSB302_TX_TOKEN_SYNC1;
            buf[len++] = FUSB302_TX_TOKEN_SYNC3;
            buf[len++] = FUSB302_TX_TOKEN_SYNC1;
            buf[len++] = FUSB302_TX_TOKEN_SYNC3;
            break;
        default:
            furi_crash("Unsupported SOP type for transmission!");
            break;
        }

        /* Append payload */
        uint16_t header = msg->header;
        header &= ~(0x7000);
        header |= ((msg->object_count & 0b111) << 12);
        buf[len++] = FUSB302_TX_TOKEN_PACKSYM | (0x1F & (2 + 4 * msg->object_count));
        buf[len++] = header >> 0;
        buf[len++] = header >> 8;
        for(uint8_t i = 0; i < msg->object_count; i++) {
            buf[len++] = msg->objects[i] >> 0;
            buf[len++] = msg->objects[i] >> 8;
            buf[len++] = msg->objects[i] >> 16;
            buf[len++] = msg->objects[i] >> 24;
        }

        /* Append crc and end of packet sequence */
        buf[len++] = FUSB302_TX_TOKEN_JAMCRC;
        buf[len++] = FUSB302_TX_TOKEN_EOP;
        buf[len++] = FUSB302_TX_TOKEN_TXOFF;
        buf[len++] = FUSB302_TX_TOKEN_TXON;

        /* Flush tx fifo */
        res = fusb302_pd_tx_flush(instance);
        if(res != Fusb302StatusOk) {
            break;
        }

        /* Send buffer */
        res = fusb302_write_buf(instance, Fusb302RegFifos, buf, len);
    } while(false);

    if(res != Fusb302StatusOk) {
        FURI_LOG_E(TAG, "Failed to send message!");
    }
    return res;
}
