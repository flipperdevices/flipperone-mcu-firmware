#include "i2c_slave.h"
#include <hardware/irq.h>
#include <furi_hal.h>

typedef struct {
    I2cSlaveCallback callback;
    bool transfer_in_progress;
} I2cSlave;

static I2cSlave i2c_slaves[2];

static void __isr __not_in_flash_func(i2c_slave_irq_callback)(void) {
    uint i2c_index = __get_current_exception() - VTABLE_FIRST_IRQ - I2C0_IRQ;
    I2cSlave* slave = &i2c_slaves[i2c_index];
    i2c_inst_t* i2c = i2c_get_instance(i2c_index);
    i2c_hw_t* hw = i2c_get_hw(i2c);

    uint32_t intr_stat = hw->intr_stat;
    if(intr_stat == 0) {
        return;
    }
    bool do_finish_transfer = false;
    if(intr_stat & I2C_IC_INTR_STAT_R_TX_ABRT_BITS) {
        hw->clr_tx_abrt;
        do_finish_transfer = true;
    }
    if(intr_stat & I2C_IC_INTR_STAT_R_START_DET_BITS) {
        hw->clr_start_det;
        if(slave->transfer_in_progress) {
            slave->callback(i2c, I2cSlaveEventRepeatedStart);
        }
    }
    if(intr_stat & I2C_IC_INTR_STAT_R_STOP_DET_BITS) {
        hw->clr_stop_det;
        do_finish_transfer = true;
    }
    if(do_finish_transfer && slave->transfer_in_progress) {
        slave->callback(i2c, I2cSlaveEventStop);
        slave->transfer_in_progress = false;
    }
    if(intr_stat & I2C_IC_INTR_STAT_R_RX_FULL_BITS) {
        slave->transfer_in_progress = true;
        slave->callback(i2c, I2cSlaveEventReceive);
    }
    if(intr_stat & I2C_IC_INTR_STAT_R_RD_REQ_BITS) {
        hw->clr_rd_req;
        slave->transfer_in_progress = true;
        slave->callback(i2c, I2cSlaveEventRequest);
    }
}

void i2c_slave_init(i2c_inst_t* i2c, uint8_t address, I2cSlaveCallback callback) {
    furi_check(i2c == i2c0 || i2c == i2c1);
    furi_check(callback);

    uint32_t i2c_index = i2c_hw_index(i2c);
    I2cSlave* slave = &i2c_slaves[i2c_index];
    slave->callback = callback;

    // Note: The I2C slave does clock stretching implicitly after a RD_REQ, while the Tx FIFO is empty.
    // Clock stretching while the Rx FIFO is full is also enabled by default.
    i2c_set_slave_mode(i2c, true, address);

    i2c_hw_t* hw = i2c_get_hw(i2c);
    // unmask necessary interrupts
    hw->intr_mask = I2C_IC_INTR_MASK_M_RX_FULL_BITS | I2C_IC_INTR_MASK_M_RD_REQ_BITS | I2C_IC_INTR_MASK_M_TX_ABRT_BITS | I2C_IC_INTR_MASK_M_STOP_DET_BITS |
                    I2C_IC_INTR_MASK_M_START_DET_BITS;

    // enable interrupt for current core
    uint32_t num = I2C0_IRQ + i2c_index;
    irq_set_exclusive_handler(num, i2c_slave_irq_callback);
    irq_set_enabled(num, true);
}

void i2c_slave_deinit(i2c_inst_t* i2c) {
    furi_check(i2c == i2c0 || i2c == i2c1);

    uint32_t i2c_index = i2c_hw_index(i2c);
    I2cSlave* slave = &i2c_slaves[i2c_index];
    furi_check(slave->callback); // should be called after i2c_slave_init()

    slave->callback = NULL;
    slave->transfer_in_progress = false;

    uint32_t num = I2C0_IRQ + i2c_index;
    irq_set_enabled(num, false);
    irq_remove_handler(num, i2c_slave_irq_callback);

    i2c_hw_t* hw = i2c_get_hw(i2c);
    hw->intr_mask = I2C_IC_INTR_MASK_RESET;

    i2c_set_slave_mode(i2c, false, 0);
}
