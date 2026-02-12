#include "fusb302_reg.h"
#include "fusb302.h"
#include <furi.h>

#include <furi_hal_i2c.h>

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

static FURI_ALWAYS_INLINE int fusb302_write_reg(Fusb302* instance, Fusb302Reg reg, uint8_t data) {
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

    return ret;
}

static FURI_ALWAYS_INLINE int fusb302_read_reg(Fusb302* instance, Fusb302Reg reg, uint8_t* data) {
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

    return ret;
}

static __isr __not_in_flash_func(void) fusb302_interrupt_handler(void* ctx) {
    Fusb302* instance = (Fusb302*)ctx;
    if(instance->callback) {
        instance->callback(instance->context);
    }
}

void fusb302_start_drp_logic(Fusb302* instance) {
    furi_check(instance);

    //1. reset the device to default state
    Fusb302ResetRegBits reset = {
        .sw_reset = 1,
    };
    fusb302_write_reg(instance, Fusb302RegReset, *(uint8_t*)&reset);

    // 2. Enable power
    Fusb302PowerRegBits power = {0};
    power.pwr = 0b0111; // Enable all power blocks
    fusb302_write_reg(instance, Fusb302RegPower, *(uint8_t*)&power);

    // 3. Enable DRP Toggle
    Fusb302Control2RegBits control2 = {0};
    fusb302_read_reg(instance, Fusb302RegControl2, (uint8_t*)&control2);
    control2.toggle = 1; // Enable toggling between source and sink
    control2.mode = 0b01; // DRP mode
    fusb302_write_reg(instance, Fusb302RegControl2, *(uint8_t*)&control2);

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
    fusb302_write_reg(instance, Fusb302RegMask, *(uint8_t*)&mask);

    Fusb302MaskARegBits mask_a = {0};
    mask_a.m_hardrst = 1; // Mask hard reset interrupts
    mask_a.m_softrst = 1; // Mask soft reset interrupts
    mask_a.m_txsent = 1; // Mask transmit sent interrupts
    mask_a.m_hardsent = 1; // Mask hard reset sent interrupts
    mask_a.m_retryfail = 1; // Mask retry fail interrupts
    mask_a.m_softfail = 1; // Mask soft reset fail interrupts
    mask_a.m_togdone = 0; // Mask toggle done interrupts
    mask_a.m_ocp_temp = 1; // Mask over-current/temperature interrupts
    fusb302_write_reg(instance, Fusb302RegMaskA, *(uint8_t*)&mask_a);

    Fusb302MaskBRegBits mask_b = {0};
    mask_b.m_gcrcsent = 1; // Mask GoodCRC sent interrupts
    fusb302_write_reg(instance, Fusb302RegMaskB, *(uint8_t*)&mask_b);

    // Clear any pending interrupts
    uint8_t irq;
    fusb302_read_reg(instance, Fusb302RegInterrupt, &irq);
    fusb302_read_reg(instance, Fusb302RegInterruptA, &irq);
    fusb302_read_reg(instance, Fusb302RegInterruptB, &irq);

    // 5. Enable interrupts and host current
    Fusb302Control0RegBits control0 = {0};
    fusb302_read_reg(instance, Fusb302RegControl0, (uint8_t*)&control0);
    control0.int_mask = 0; // Enable interrupts on INT pin
    control0.host_cur = 0b01; // Set default USB power (80mA)
    fusb302_write_reg(instance, Fusb302RegControl0, *(uint8_t*)&control0);
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

        furi_hal_gpio_init_simple(instance->pin_interrupt, GpioModeInput);
        furi_hal_gpio_add_int_callback(instance->pin_interrupt, GpioConditionFall, fusb302_interrupt_handler, instance);

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
    furi_hal_gpio_remove_int_callback(instance->pin_interrupt);
    furi_hal_gpio_init_ex(instance->pin_interrupt, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    free(instance);
}

void fusb302_pd_reset(Fusb302* instance) {
    furi_check(instance);
    Fusb302ResetRegBits reset = {
        .pd_reset = 1,
    };
    fusb302_write_reg(instance, Fusb302RegReset, *(uint8_t*)&reset);
}

bool fusb302_read_role(Fusb302* instance) {
    // Read interrupts to determine what happened
    uint8_t irq_a;
    uint8_t irq;
    bool ret = false;
    fusb302_read_reg(instance, Fusb302RegInterruptA, &irq_a);
    fusb302_read_reg(instance, Fusb302RegInterrupt, &irq);
    // FUSB302_DEBUG(TAG, "Interrupt A: %02X", irq_a);
    // FUSB302_DEBUG(TAG, "Interrupt: %02X", irq);
    if(irq_a & FUSB302_INTERRUPTA_MASK_TOGDONE) { // Checking I_TOGDONE bit (bit 6)
        // Read status to determine the current role
        
        Fusb302Status1ARegBits status1a_bits;
        fusb302_read_reg(instance, Fusb302RegStatus1A, (uint8_t*)&status1a_bits);

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
            //todo: need to wait for VBUS from partner
            break;
        case FUSB302_STATUS1A_TOGSS_SNKON_CC2: // 110 - Sink on CC2
            FUSB302_DEBUG(TAG, "Role determined: SINK CC2\n");
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
    } else if(irq & FUSB302_INTERRUPT_MASK_COMP_CHNG) { // Checking I_COMP_CHNG bit (bit 5)
        FUSB302_DEBUG(TAG, "FUSB302_INTERRUPT_MASK_COMP_CHNG\n");
        fusb302_start_drp_logic(instance);
    } else if(irq & FUSB302_INTERRUPT_MASK_VBUSOK) { // Checking I_COMP_CHNG bit (bit 5)
        FUSB302_DEBUG(TAG, "FUSB302_INTERRUPT_MASK_VBUSOK\n");
        fusb302_start_drp_logic(instance);
    } else {
        // FUSB302_DEBUG(TAG, "Toggle not completed yet...\n");
    }
    return ret;
}

void fusb302_read_cc_status(Fusb302* instance, uint8_t cc) {
    furi_check(instance);
}

void fusb302_set_input_callback(Fusb302* instance, Fusb302Callback callback, void* context) {
    furi_check(instance);
    instance->callback = callback;
    instance->context = context;
}
