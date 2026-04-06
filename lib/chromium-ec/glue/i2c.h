#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

struct i2c_info_t {
    uint16_t port; /* Physical port for device */
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
#define I2C_XFER_START BIT(0) /* Start smbus session from idle state */
#define I2C_XFER_STOP  BIT(1) /* Terminate smbus session with stop bit */
#define I2C_XFER_SINGLE (I2C_XFER_START | I2C_XFER_STOP) /* One transaction */

/**
 * Lock / unlock an I2C port.
 * @param port		Port to lock
 * @param lock		1 to lock, 0 to unlock
 */
void i2c_lock(int port, int lock);

/**
 * Write an 8-bit register to the peripheral at 7-bit peripheral address
 * <addr_flags>, at the specified 8-bit <offset> in the peripheral's address
 * space.
 */
int i2c_write8(const int port, const uint16_t addr_flags, int offset, int data);

/**
 * Write a 16-bit register to the peripheral at 7-bit peripheral address
 * <addr_flags>, at the specified 8-bit <offset> in the peripheral's address
 * space.
 */
int i2c_write16(const int port, const uint16_t addr_flags, int offset, int data);

/**
 * Read an 8-bit register from the peripheral at 7-bit peripheral address
 * <addr_flags>, at the specified 8-bit <offset> in the peripheral's address
 * space.
 */
int i2c_read8(const int port, const uint16_t addr_flags, int offset, int* data);

/**
 * Read a 16-bit register from the peripheral at 7-bit peripheral address
 * <addr_flags>, at the specified 8-bit <offset> in the peripheral's address
 * space.
 */
int i2c_read16(const int port, const uint16_t addr_flags, int offset, int* data);

/**
 * Transmit one block of raw data, then receive one block of raw data. However,
 * transferred data might be capped at CONFIG_I2C_CHIP_MAX_TRANSFER_SIZE if
 * CONFIG_I2C_XFER_LARGE_TRANSFER is not defined.  The transfer is strictly
 * atomic, by locking the I2C port and performing an I2C_XFER_SINGLE transfer.
 *
 * @param port		Port to access
 * @param addr_flags	Peripheral device address
 * @param out		Data to send
 * @param out_size	Number of bytes to send
 * @param in		Destination buffer for received data
 * @param in_size	Number of bytes to receive
 * @return EC_SUCCESS, or non-zero if error.
 */
int i2c_xfer(const int port, const uint16_t addr_flags, const uint8_t* out, int out_size, uint8_t* in, int in_size);

/**
 * Same as i2c_xfer, but the bus is not implicitly locked.  It must be called
 * between i2c_lock(port, 1) and i2c_lock(port, 0).
 *
 * @param flags		Flags (see I2C_XFER_* above)
 */
int i2c_xfer_unlocked(const int port, const uint16_t addr_flags, const uint8_t* out, int out_size, uint8_t* in, int in_size, int flags);

/**
 * Read a data block of <len> 8-bit transfers from the peripheral at 7-bit
 * peripheral address <addr_flags>, at the specified 8-bit <offset> in the
 * peripheral's address space.
 */
int i2c_read_block(const int port, const uint16_t addr_flags, int offset, uint8_t* data, int len);

/**
 * Write a data block of <len> 8-bit transfers to the peripheral at 7-bit
 * peripheral address <addr_flags>, at the specified 8-bit <offset> in the
 * peripheral's address space.
 */
int i2c_write_block(const int port, const uint16_t addr_flags, int offset, const uint8_t* data, int len);

/**
 * Read, modify, write an i2c register to the peripheral at 7-bit peripheral
 * address <addr_flags> at the specified 8-bit <offset> in the
 * peripheral's address space.  The <action> will specify whether this is
 * setting the <mask> bit value(s) or clearing them. If the value to be written
 * is the same as the original value of the register, the write will not be
 * performed.
 */
int i2c_update8(const int port, const uint16_t addr_flags, const int offset, const uint8_t mask, const enum mask_update_action action);

int i2c_update16(const int port, const uint16_t addr_flags, const int offset, const uint16_t mask, const enum mask_update_action action);

#ifdef __cplusplus
}
#endif
