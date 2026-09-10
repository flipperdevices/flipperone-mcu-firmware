#include "i2c_slave.h"
#include <hardware/irq.h>
#include <furi_hal.h>

typedef struct {
    I2cSlaveCallback callback;
    uint8_t address;
    bool start_detected;
} I2cSlave;

static I2cSlave i2c_slaves[2];
static bool i2c_started[2] = {false, false};

static void __isr __not_in_flash_func(i2c_slave_irq_callback)(void) {
    uint i2c_index = __get_current_exception() - VTABLE_FIRST_IRQ - I2C0_IRQ;
    I2cSlave* slave = &i2c_slaves[i2c_index];
    i2c_inst_t* i2c = i2c_get_instance(i2c_index);
    i2c_hw_t* hw = i2c_get_hw(i2c);
    bool* is_started = &i2c_started[i2c_index];

    uint32_t intr_stat = hw->intr_stat;
    if(intr_stat == 0) {
        return;
    }

    // Handle events in different order depending on previous transaction state
    if(*is_started) {
        if(intr_stat & I2C_IC_INTR_STAT_R_RESTART_DET_BITS) {
            (void)hw->clr_restart_det;
            slave->callback(i2c, I2cSlaveEventRepeatedStart);
        }
        if(intr_stat & I2C_IC_INTR_STAT_R_RX_FULL_BITS) {
            slave->callback(i2c, I2cSlaveEventReceive);
        }
        if(intr_stat & I2C_IC_INTR_STAT_R_RD_REQ_BITS) {
            (void)hw->clr_rd_req;
            slave->callback(i2c, I2cSlaveEventRequest);
        }
        if(intr_stat & I2C_IC_INTR_STAT_R_STOP_DET_BITS) {
            (void)hw->clr_stop_det;
            slave->callback(i2c, I2cSlaveEventStop);
            *is_started = false;
        }
        if(intr_stat & I2C_IC_INTR_STAT_R_START_DET_BITS) {
            (void)hw->clr_start_det;
            slave->callback(i2c, I2cSlaveEventStart);
            *is_started = true;
        }
    } else {
        if(intr_stat & I2C_IC_INTR_STAT_R_START_DET_BITS) {
            (void)hw->clr_start_det;
            slave->callback(i2c, I2cSlaveEventStart);
            *is_started = true;
            if(intr_stat & I2C_IC_INTR_STAT_R_RX_FULL_BITS) {
                slave->callback(i2c, I2cSlaveEventReceive);
            }
            if(intr_stat & I2C_IC_INTR_STAT_R_RD_REQ_BITS) {
                (void)hw->clr_rd_req;
                slave->callback(i2c, I2cSlaveEventRequest);
            }
            if(intr_stat & I2C_IC_INTR_STAT_R_RESTART_DET_BITS) {
                (void)hw->clr_restart_det;
                slave->callback(i2c, I2cSlaveEventRepeatedStart);
            }
            if(intr_stat & I2C_IC_INTR_STAT_R_STOP_DET_BITS) {
                (void)hw->clr_stop_det;
                slave->callback(i2c, I2cSlaveEventStop);
                *is_started = false;
            }
        }
    }
}

void i2c_slave_init(i2c_inst_t* i2c, uint8_t address, I2cSlaveCallback callback) {
    furi_check(i2c == i2c0 || i2c == i2c1);
    furi_check(callback);

    uint32_t i2c_index = i2c_hw_index(i2c);
    I2cSlave* slave = &i2c_slaves[i2c_index];
    slave->callback = callback;
    slave->address = address;

    // Note: The I2C slave does clock stretching implicitly after a RD_REQ, while the Tx FIFO is empty.
    // Clock stretching while the Rx FIFO is full is also enabled by default.
    i2c_set_slave_mode(i2c, true, address);

    i2c_hw_t* hw = i2c_get_hw(i2c);
    // unmask necessary interrupts
    hw->intr_mask = I2C_IC_INTR_MASK_M_RX_FULL_BITS | I2C_IC_INTR_MASK_M_RD_REQ_BITS | I2C_IC_INTR_MASK_M_TX_ABRT_BITS | I2C_IC_INTR_MASK_M_STOP_DET_BITS |
                    I2C_IC_INTR_MASK_M_START_DET_BITS | I2C_IC_INTR_MASK_M_RESTART_DET_BITS;

    // Set Rx FIFO threshold to 2 bytes and enable clock stretching on Rx FIFO full
    i2c->hw->enable = 0;
    i2c->hw->rx_tl = 1;
    i2c->hw->con |= I2C_IC_CON_RX_FIFO_FULL_HLD_CTRL_BITS; // Enable clock stretching
    i2c->hw->enable = 1;
    // enable interrupt for current core
    uint32_t num = I2C0_IRQ + i2c_index;
    irq_set_exclusive_handler(num, i2c_slave_irq_callback);
    irq_set_priority(num, configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY << (8 - __NVIC_PRIO_BITS));
    irq_set_enabled(num, true);
}

void i2c_slave_deinit(i2c_inst_t* i2c) {
    furi_check(i2c == i2c0 || i2c == i2c1);

    uint32_t i2c_index = i2c_hw_index(i2c);
    I2cSlave* slave = &i2c_slaves[i2c_index];
    furi_check(slave->callback); // should be called after i2c_slave_init()

    slave->callback = NULL;
    slave->address = 0;
    slave->start_detected = false;

    uint32_t num = I2C0_IRQ + i2c_index;
    irq_set_enabled(num, false);
    irq_remove_handler(num, i2c_slave_irq_callback);

    i2c_hw_t* hw = i2c_get_hw(i2c);
    hw->intr_mask = I2C_IC_INTR_MASK_RESET;

    // Dummy address: must be in valid 7-bit range 0x08-0x77
    i2c_set_slave_mode(i2c, false, 0x77);
}

void i2c_slave_reset(i2c_inst_t* i2c) {
    furi_check(i2c == i2c0 || i2c == i2c1);

    uint32_t i2c_index = i2c_hw_index(i2c);
    I2cSlave* slave = &i2c_slaves[i2c_index];

    I2cSlaveCallback temp_callback = slave->callback;
    uint8_t temp_address = slave->address;
    furi_check(temp_address);
    furi_check(temp_callback);

    i2c_slave_deinit(i2c);
    i2c_slave_init(i2c, temp_address, temp_callback);
}
