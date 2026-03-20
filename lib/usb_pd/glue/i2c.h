#pragma once

#include "config.h"

#define i2c_write8(port, addr_flags, reg, data)  0//furi_hal_i2c_write_reg(&fusb302_i2c, port, reg, data)
#define i2c_read8(port, addr_flags, reg, data) 0//furi_hal_i2c_read_reg(&fusb302_i2c, port, reg, data)
#define i2c_write16(port, addr_flags, reg, data) 0 //furi_hal_i2c_write_reg16(&fusb302_i2c, port, reg, data)
#define i2c_read16(port, addr_flags, reg, data) 0//furi_hal_i2c_read_reg16(&fusb302_i2c, port, reg, data)
#define i2c_xfer(port, addr_flags, out, out_size, in, in_size) 0//furi_hal_i2c_transfer(&fusb302_i2c, port, out, out_size, in, in_size)
#define i2c_xfer_unlocked(port, addr_flags, out, out_size, in, in_size, flags) 0//furi_hal_i2c_transfer(&fusb302_i2c, port, out, out_size, in, in_size)
#define i2c_update8(port, addr_flags, reg, mask, action) 0//furi_hal_i2c_update_reg(&fusb302_i2c, port, reg, mask, action)
#define i2c_update16(port, addr_flags, reg, mask, action) 0//furi_hal_i2c_update_reg16(&fusb302_i2c, port, reg, mask, action)
#define i2c_write_block(port, addr_flags, reg, out, size) 0//furi_hal_i2c_write_block(&fusb302_i2c, port, reg, out, size)
#define i2c_read_block(port, addr_flags, reg, in, size) 0//furi_hal_i2c_read_block(&fusb302_i2c, port, reg, in, size)
#define i2c_update8(port, addr_flags, reg, mask, action) 0//furi_hal_i2c_update_reg(&fusb302_i2c, port, reg, mask, action)
#define i2c_update16(port, addr_flags, reg, mask, action) 0//furi_hal_i2c_update_reg16(&fusb302_i2c, port, reg, mask, action)
#define i2c_lock(port, lock) //furi_hal_i2c_lock(&fusb302_i2c, port, lock)


struct i2c_info_t {
	uint16_t port;	/* Physical port for device */
	uint16_t addr_flags;
};

/*
 * I2C mask update actions.
 *      MASK_SET will OR the mask into the old value
 *      MASK_CLR will AND the ~mask from the old value
 */
enum mask_update_action {
	MASK_CLR,
	MASK_SET
};

/* Flags for i2c_xfer_unlocked() */
#define I2C_XFER_START BIT(0)  /* Start smbus session from idle state */
#define I2C_XFER_STOP BIT(1)  /* Terminate smbus session with stop bit */
#define I2C_XFER_SINGLE (I2C_XFER_START | I2C_XFER_STOP)  /* One transaction */

