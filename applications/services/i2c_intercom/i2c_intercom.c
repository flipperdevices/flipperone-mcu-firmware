#include <furi.h>
#include <furi_hal_i2c.h>
#include <furi_hal_i2c_config.h>

#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>

#include "i2c_intercom.h"
#include "i2c_registers_i.h"

#define TAG "I2CIntercom"

#define I2C_INTERCOM_THREAD_SETUP_END_FLAG (1 << 0)

#define I2C_INTERCOM_DEFAULT_ADDRESS_REGISTER 0x00
#define I2C_INTERCOM_LOG_PERIOD_MS            1000
#define I2C_INTERCOM_INVALID_ADDRESS_VALUE    0x00

/** I2C Intercom state */
struct I2CIntercom {
    FuriThreadId thread_id;
    const FuriHalI2cBusHandle* bus_handle;

    volatile bool in_transaction;
    volatile size_t mem_address;
    volatile uint8_t address_hi;
    volatile uint8_t address_index;
    volatile uint32_t invalid_read_count;
    volatile uint32_t invalid_write_count;
    volatile uint32_t dropped_byte_count;
    uint32_t last_driver_dropped;
};

static inline void i2c_intercom_data_transmit(const FuriHalI2cBusHandle* handle, I2CIntercom* instance) {
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

// Transaction framing comes from the DW data markers, not from START/STOP
// flags: the latter collapse while interrupts are masked by flash operations.
static inline void i2c_intercom_receive(const FuriHalI2cBusHandle* handle, I2CIntercom* instance) {
    uint8_t data;
    uint8_t flags;
    while(furi_hal_i2c_slave_read_blocking(handle, &data, &flags, 1)) {
        if(flags & (FURI_HAL_I2C_SLAVE_RX_FLAG_FIRST_DATA_BYTE | FURI_HAL_I2C_SLAVE_RX_FLAG_RESTART)) {
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
            instance->address_index = 2;
        } else {
            bool valid = false;
            with_i2c_register({ valid = i2c_register_write(instance->mem_address, data); });
            if(!valid) {
                instance->invalid_write_count++;
            }
            instance->mem_address++;
        }
    }
}

void __isr __not_in_flash_func(i2c_intercom_isr)(const FuriHalI2cBusHandle* handle, FuriHalI2cBusSlaveEvent event, void* context) {
    I2CIntercom* instance = context;

    switch(event) {
    case FuriHalI2cBusSlaveEventStart:
        instance->in_transaction = true;
        break;
    case FuriHalI2cBusSlaveEventRead:
        i2c_intercom_data_transmit(handle, instance);
        break;
    case FuriHalI2cBusSlaveEventWrite:
        i2c_intercom_receive(handle, instance);
        break;
    case FuriHalI2cBusSlaveEventRepeatedStart:
        // A missed STOP may have ended the transaction while idle: the next
        // register access starts at the default address.
        if(!instance->in_transaction) {
            instance->mem_address = I2C_INTERCOM_DEFAULT_ADDRESS_REGISTER;
        }
        break;
    case FuriHalI2cBusSlaveEventStop:
        if(instance->in_transaction) {
            // Drain the RX FIFO: bytes may have arrived while interrupts
            // were masked (flash ops collapse the data markers).
            i2c_intercom_receive(handle, instance);
            instance->in_transaction = false;
        }
        break;
    default:
        break;
    }
}

void i2c_intercom_setup_end(I2CIntercom* instance) {
    furi_thread_flags_set(instance->thread_id, I2C_INTERCOM_THREAD_SETUP_END_FLAG);
}

int32_t i2c_intercom_srv(void* p) {
    UNUSED(p);
    furi_thread_set_current_priority(FuriThreadPriorityHigh);
    I2CIntercom* instance = malloc(sizeof(I2CIntercom));
    instance->thread_id = furi_thread_get_current_id();
    instance->bus_handle = &furi_hal_i2c_handle_cpu;

    i2c_registers_init();

    furi_record_create(RECORD_I2C_INTERCOM, instance);

    FURI_LOG_I(TAG, "Started");

    // wait to end of setup before allowing interrupts
    {
        uint32_t flags = furi_thread_flags_wait(I2C_INTERCOM_THREAD_SETUP_END_FLAG, FuriFlagWaitAny, FuriWaitForever);
        furi_check(!(flags & FuriFlagError));
    }

    FURI_LOG_I(TAG, "Setup completed, enabling I2C slave mode");

    furi_hal_i2c_acquire(instance->bus_handle);
    furi_hal_i2c_slave_set_callback(instance->bus_handle, i2c_intercom_isr, instance);
    instance->in_transaction = false;
    instance->mem_address = I2C_INTERCOM_DEFAULT_ADDRESS_REGISTER;

    while(1) {
        furi_delay_ms(I2C_INTERCOM_LOG_PERIOD_MS);
        FURI_CRITICAL_ENTER();
        uint32_t invalid_reads = instance->invalid_read_count;
        uint32_t invalid_writes = instance->invalid_write_count;
        uint32_t dropped_bytes = instance->dropped_byte_count;
        instance->invalid_read_count = 0;
        instance->invalid_write_count = 0;
        instance->dropped_byte_count = 0;
        FURI_CRITICAL_EXIT();

        uint32_t driver_dropped = furi_hal_i2c_slave_dropped_bytes_get(instance->bus_handle);
        uint32_t driver_dropped_delta = driver_dropped - instance->last_driver_dropped;
        instance->last_driver_dropped = driver_dropped;

        if(invalid_reads || invalid_writes || dropped_bytes || driver_dropped_delta) {
            FURI_LOG_W(
                TAG,
                "invalid access: %lu reads, %lu writes, %lu dropped bytes, %lu driver-dropped bytes",
                invalid_reads,
                invalid_writes,
                dropped_bytes,
                driver_dropped_delta);
        }
    }

    return 0;
}
