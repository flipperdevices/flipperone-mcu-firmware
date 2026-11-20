#include <furi.h>
#include <applications.h>
#include <furi_hal_i2c.h>
#include <furi_hal_i2c_config.h>
#include <drivers/i2c_slave/i2c_slave.h>

#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>

#include "i2c_intercom.h"
#include "i2c_registers_i.h"

#define TAG "I2CIntercom"

/*
 * Wire protocol, as seen by the host:
 *
 * - A write transaction always starts with the 16-bit register address, high
 *   byte first. The data bytes after it go to consecutive addresses
 *   (auto-increment). A write cannot continue from an earlier transaction:
 *   its first byte is always taken as the address high byte.
 * - A read serves bytes from the current address and auto-increments it. Set
 *   the address with a write of just the two address bytes, then read, either
 *   after a repeated START in the same transaction or in a later one.
 * - A read transaction without an address write continues from where the
 *   previous access left the address: START is not a reset point. The Linux
 *   regmap driver does not rely on this, it sends the address every time.
 */

#define I2C_INTERCOM_DEFAULT_ADDRESS_REGISTER 0x00
#define I2C_INTERCOM_LOG_PERIOD_MS            1000
#define I2C_INTERCOM_INVALID_ADDRESS_VALUE    0x00

/** I2C Intercom state */
struct I2CIntercom {
    const FuriHalI2cBusHandle* bus_handle;

    volatile bool in_transaction;
    volatile size_t mem_address;
    volatile size_t pointer_address; ///< Register the last pointer write selected
    volatile bool wrote_data_in_transaction; ///< Data written in this transaction: reads serve the pointer
    volatile bool write_open; ///< A write transaction is being appended to the ring
    volatile uint8_t address_hi;
    volatile uint8_t address_index;
    volatile bool held_valid; ///< A byte taken from the FIFO but left for the next transaction
    volatile uint8_t held_data;
    volatile uint8_t held_flags;
    // Byte-loss accounting: invalid reads = register-map rejects, dropped =
    // intercom framing, driver-dropped = FIFO drain while idle (logged below).
    // Invalid writes and dropped transactions are counted by i2c_registers.
    volatile uint32_t invalid_read_count;
    volatile uint32_t dropped_byte_count;
    uint32_t last_driver_dropped;
    uint32_t last_invalid_writes;
    uint32_t last_dropped_transactions;
};

I2C_ISR_FUNC(static inline void, i2c_intercom_data_transmit)(const FuriHalI2cBusHandle* handle, I2CIntercom* instance) {
    uint8_t data;
    bool valid = false;
    with_i2c_register({ valid = i2c_register_read_start(instance->mem_address, &data); });
    if(!valid) {
        data = I2C_INTERCOM_INVALID_ADDRESS_VALUE;
        instance->invalid_read_count++;
    }

    uint8_t len = furi_hal_i2c_slave_write_blocking(handle, &data, 1);
    if(len) {
        with_i2c_register({
            i2c_register_read_commit(instance->mem_address);
            instance->mem_address++;
        });
    }
}

// End the write being appended: publish it to the ring for the register
// software interrupt to apply.
I2C_ISR_FUNC(static inline void, i2c_intercom_write_end)(I2CIntercom* instance) {
    if(instance->write_open) {
        i2c_registers_write_commit();
        instance->write_open = false;
    }
}

// Next received byte: the held one first, then the RX FIFO
I2C_ISR_FUNC(static inline bool, i2c_intercom_rx_byte)(
    const FuriHalI2cBusHandle* handle,
    I2CIntercom* instance,
    uint8_t* data,
    uint8_t* flags,
    bool* was_held) {
    *was_held = instance->held_valid;
    if(instance->held_valid) {
        *data = instance->held_data;
        *flags = instance->held_flags;
        instance->held_valid = false;
        return true;
    }
    return furi_hal_i2c_slave_read_blocking(handle, data, flags, 1);
}

// Transaction framing comes from the DW data markers, not from START/STOP
// flags: the latter collapse while interrupts are masked by flash operations.
// With `to_stop`, stop before the first byte of the next transaction: the
// bytes after a STOP belong to a transfer whose Start is not delivered yet.
I2C_ISR_FUNC(static inline void, i2c_intercom_receive)(const FuriHalI2cBusHandle* handle, I2CIntercom* instance, bool to_stop) {
    uint8_t data;
    uint8_t flags;
    bool was_held;
    while(i2c_intercom_rx_byte(handle, instance, &data, &flags, &was_held)) {
        // A held byte reaching a STOP means that STOP ends its own transfer
        if(to_stop && !was_held && (flags & FURI_HAL_I2C_SLAVE_RX_FLAG_FIRST_DATA_BYTE)) {
            instance->held_data = data;
            instance->held_flags = flags;
            instance->held_valid = true;
            break;
        }
        // FIRST_DATA_BYTE marks the first byte of every transaction, also when
        // several of them queued up in the FIFO while the IRQ was masked.
        if(flags & FURI_HAL_I2C_SLAVE_RX_FLAG_FIRST_DATA_BYTE) {
            i2c_intercom_write_end(instance);
            instance->address_index = 0;
        }
        if(instance->address_index == 0) {
            if(flags & (FURI_HAL_I2C_SLAVE_RX_FLAG_FIRST_DATA_BYTE | FURI_HAL_I2C_SLAVE_RX_FLAG_RESTART)) {
                instance->address_hi = data;
                instance->address_index = 1;
            } else {
                instance->dropped_byte_count++;
            }
        } else if(instance->address_index == 1) {
            instance->mem_address = ((uint16_t)instance->address_hi << 8) | data;
            instance->pointer_address = instance->mem_address;
            instance->address_index = 2;
            i2c_register_read_reset();
        } else {
            // Data byte: to the ring, applied later by the register software
            // interrupt. An address-only write never opens a record.
            if(!instance->write_open) {
                i2c_registers_write_begin(instance->mem_address);
                instance->write_open = true;
            }
            i2c_registers_write_byte(data);
            instance->mem_address++;
            instance->wrote_data_in_transaction = true;
        }
    }
}

I2C_ISR_FUNC(void, i2c_intercom_isr)(const FuriHalI2cBusHandle* handle, FuriHalI2cBusSlaveEvent event, void* context) {
    I2CIntercom* instance = context;

    switch(event) {
    case FuriHalI2cBusSlaveEventStart:
        // A missed STOP leaves the previous write open
        i2c_intercom_write_end(instance);
        instance->in_transaction = true;
        instance->wrote_data_in_transaction = false;
        i2c_register_read_reset();
        break;
    case FuriHalI2cBusSlaveEventRead:
        // SCL is held: bytes still in the RX FIFO belong to the write before
        // the read. Frame them, then end that write.
        i2c_intercom_receive(handle, instance, false);
        i2c_intercom_write_end(instance);
        // A read that follows data in this transaction serves the selected
        // pointer, not the auto-incremented address. That data is not applied
        // yet: the read returns the value from before this write.
        if(instance->wrote_data_in_transaction) {
            instance->mem_address = instance->pointer_address;
            instance->wrote_data_in_transaction = false;
        }
        i2c_intercom_data_transmit(handle, instance);
        break;
    case FuriHalI2cBusSlaveEventWrite:
        i2c_intercom_receive(handle, instance, false);
        break;
    case FuriHalI2cBusSlaveEventRepeatedStart:
        // Missed STOP can end the transaction while idle: reset the address.
        if(!instance->in_transaction) {
            instance->mem_address = I2C_INTERCOM_DEFAULT_ADDRESS_REGISTER;
        }
        break;
    case FuriHalI2cBusSlaveEventStop:
        if(instance->in_transaction) {
            // Drain late bytes: flash ops mask interrupts and collapse markers.
            i2c_intercom_receive(handle, instance, true);
            i2c_intercom_write_end(instance);
            instance->in_transaction = false;
        }
        break;
    default:
        break;
    }
}

int32_t i2c_intercom_srv(void* p) {
    UNUSED(p);
    furi_thread_set_current_priority(FuriThreadPriorityHigh);
    I2CIntercom* instance = malloc(sizeof(I2CIntercom));
    instance->bus_handle = &furi_hal_i2c_handle_cpu;

    i2c_registers_init();

    furi_record_create(RECORD_I2C_INTERCOM, instance);

    FURI_LOG_I(TAG, "Started");

    // Each owner registers its registers before it creates its record
    for(size_t i = 0; i < FLIPPER_I2C_REGISTER_OWNERS_COUNT; i++) {
        furi_record_open(FLIPPER_I2C_REGISTER_OWNERS[i]);
        furi_record_close(FLIPPER_I2C_REGISTER_OWNERS[i]);
    }

    FURI_LOG_I(TAG, "Setup completed, enabling I2C slave mode");

    furi_hal_i2c_acquire(instance->bus_handle);
    furi_hal_i2c_slave_set_callback(instance->bus_handle, i2c_intercom_isr, instance);
    instance->in_transaction = false;
    instance->wrote_data_in_transaction = false;
    instance->write_open = false;
    instance->held_valid = false;
    instance->pointer_address = I2C_INTERCOM_DEFAULT_ADDRESS_REGISTER;
    instance->mem_address = I2C_INTERCOM_DEFAULT_ADDRESS_REGISTER;
    // Interrupts raised during setup (the restart one) wait for the slave
    i2c_registers_start();

    while(1) {
        furi_delay_ms(I2C_INTERCOM_LOG_PERIOD_MS);
        uint32_t invalid_reads, dropped_bytes;
        with_i2c_register({
            invalid_reads = instance->invalid_read_count;
            dropped_bytes = instance->dropped_byte_count;
            instance->invalid_read_count = 0;
            instance->dropped_byte_count = 0;
        });

        uint32_t driver_dropped = furi_hal_i2c_slave_dropped_bytes_get(instance->bus_handle);
        uint32_t driver_dropped_delta = driver_dropped - instance->last_driver_dropped;
        instance->last_driver_dropped = driver_dropped;

        uint32_t invalid_writes = i2c_registers_invalid_writes_get();
        uint32_t invalid_writes_delta = invalid_writes - instance->last_invalid_writes;
        instance->last_invalid_writes = invalid_writes;

        uint32_t dropped_transactions = i2c_registers_dropped_transactions_get();
        uint32_t dropped_transactions_delta = dropped_transactions - instance->last_dropped_transactions;
        instance->last_dropped_transactions = dropped_transactions;

        if(invalid_reads || invalid_writes_delta || dropped_bytes || driver_dropped_delta) {
            FURI_LOG_W(
                TAG,
                "invalid access: %lu reads, %lu writes, %lu dropped bytes, %lu driver-dropped bytes",
                invalid_reads,
                invalid_writes_delta,
                dropped_bytes,
                driver_dropped_delta);
        }
        if(dropped_transactions_delta) {
            FURI_LOG_E(TAG, "%lu write transactions dropped: transaction ring full or too long", dropped_transactions_delta);
        }
    }

    return 0;
}
