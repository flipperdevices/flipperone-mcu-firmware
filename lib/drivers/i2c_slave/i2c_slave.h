#pragma once

#include <hardware/i2c.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief I2C slave event types.
 * \ingroup pico_i2c_slave
 */
typedef enum {
    I2cSlaveEventStart, ///< Master has sent a Start signal. Slave may prepare for the next transfer.
    I2cSlaveEventReceive, ///< Data from master is available for reading. Slave must read from Rx FIFO.
    I2cSlaveEventRequest, ///< Master is requesting data. Slave must write into Tx FIFO.
    I2cSlaveEventRepeatedStart, ///< Master has sent a Restart signal. Slave may prepare for the next transfer.
    I2cSlaveEventStop, ///< Master has sent a Stop signal. Slave may prepare for the next transfer.
} I2cSlaveEvent;

/**
 * \brief I2C slave event handler
 * \ingroup pico_i2c_slave
 * 
 * The event handler will run from the I2C ISR, so it should return quickly (under 25 us at 400 kb/s).
 * Avoid blocking inside the handler and split large data transfers across multiple calls for best results.
 * When sending data to master, up to \ref i2c_get_write_available()  bytes can be written without blocking.
 * When receiving data from master, up to \ref i2c_get_read_available() bytes can be read without blocking.
 *
 * \param i2c Either \ref i2c0 or \ref i2c1
 * \param event Event type.
 */
typedef void (*I2cSlaveCallback)(i2c_inst_t* i2c, I2cSlaveEvent event);

/**
 * \brief Configure an I2C instance for slave mode.
 * \ingroup pico_i2c_slave
 * \param i2c I2C instance.
 * \param address 7-bit slave address.
 * \param handler Callback for events from I2C master. It will run from the I2C ISR, on the CPU core
 *                where the slave was initialised.
 */
void i2c_slave_init(i2c_inst_t* i2c, uint8_t address, I2cSlaveCallback handler);

/**
 * \brief Restore an I2C instance to master mode.
 * \ingroup pico_i2c_slave
 * \param i2c Either \ref i2c0 or \ref i2c1
 */
void i2c_slave_deinit(i2c_inst_t* i2c);

/**
 * \brief Reset an I2C slave instance.
 * \ingroup pico_i2c_slave
 * \param i2c Either \ref i2c0 or \ref i2c1
 */
void i2c_slave_reset(i2c_inst_t* i2c);

#ifdef __cplusplus
}
#endif
