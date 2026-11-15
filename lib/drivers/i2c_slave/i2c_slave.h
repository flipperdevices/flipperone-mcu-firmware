#pragma once

#include <hardware/i2c.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * \brief NVIC priority of the I2C slave interrupt, as a 0-15 level (0 = highest).
 *
 * Above configMAX_SYSCALL_INTERRUPT_PRIORITY, so FURI_CRITICAL does not mask
 * it: see I2C_ISR_FUNC for why and what that requires.
 */
#define I2C_SLAVE_IRQ_PRIORITY (4)

/** I2C_SLAVE_IRQ_PRIORITY as a raw 8-bit NVIC/BASEPRI value. */
#define I2C_SLAVE_IRQ_PRIORITY_RAW (I2C_SLAVE_IRQ_PRIORITY << 4)

/**
 * \brief Define a function that runs in the I2C slave ISR.
 *
 * Why it exists: the I2C slave interrupt runs at I2C_SLAVE_IRQ_PRIORITY, above
 * FURI_CRITICAL, so it keeps serving the host while flash is erased or
 * programmed. Masking it there overflowed the 16-byte RX FIFO and broke the
 * update over I2C (#263). But while flash is busy, any fetch from it faults:
 * the core locks up without a crash report. So every function the ISR reaches
 * must be in RAM, read no flash-resident data and call no FreeRTOS API.
 *
 * The macro places the function in RAM (__not_in_flash_func) and leaves a
 * marker symbol __i2c_isr_func_<name> next to it. `used` keeps a copy for the
 * marker to point at when the function is inlined everywhere; --gc-sections
 * then drops that unreferenced copy together with its marker, at no RAM cost.
 *
 * scripts/isr_ram_check.py, run after every link, starts from the marked
 * functions and fails the build when:
 *   - a function the ISR reaches is not marked with this macro,
 *   - a marked function reaches flash code or loads flash-resident data,
 *   - a function the ISR calls through a pointer (its REQUIRED list) is missing
 *     or lost its marker, e.g. after a rewrite.
 *
 * Use it on every function of the I2C ISR path, callbacks reached through
 * pointers in particular:
 *
 *     I2C_ISR_FUNC(static uint8_t, my_isr_helper)(i2c_inst_t* i2c) {
 *         ...
 *     }
 *
 * \param type Return type with any storage class, e.g. `static bool`.
 * \param name Function name.
 */
#define I2C_ISR_FUNC(type, name)                                                                        \
    __asm__(".globl __i2c_isr_func_" #name "\n\t.set __i2c_isr_func_" #name ", " #name);                \
    type __attribute__((used)) __isr __not_in_flash_func(name)

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

/**
 * \brief Number of bytes discarded while no transaction was tracked
 * \ingroup pico_i2c_slave
 * \param i2c Either \ref i2c0 or \ref i2c1
 * \return Cumulative dropped byte count
 */
uint32_t i2c_slave_dropped_bytes_get(i2c_inst_t* i2c);

uint8_t i2c_slave_rxflr_get(i2c_inst_t* i2c);

#ifdef __cplusplus
}
#endif
