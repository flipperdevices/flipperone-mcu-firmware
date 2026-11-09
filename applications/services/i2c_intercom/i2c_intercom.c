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

typedef enum {
    I2CIntercomStateIdle,
    I2CIntercomStateStart,
    I2CIntercomStateAddressSet,
    I2CIntercomStateAddressNoSet,
    I2CIntercomStateDataTransmitted,
} I2CIntercomState;

/** I2C Intercom state */
struct I2CIntercom {
    FuriThreadId thread_id;
    const FuriHalI2cBusHandle* bus_handle;

    volatile I2CIntercomState state;
    volatile size_t mem_address;
    volatile uint32_t invalid_read_count;
    volatile uint32_t invalid_write_count;
    volatile uint32_t dropped_byte_count;
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

static inline size_t i2c_intercom_drain(const FuriHalI2cBusHandle* handle) {
    // Discard late bytes; SCL is stretched until the FIFO is drained
    uint8_t data;
    size_t count = 0;
    while(furi_hal_i2c_slave_read_blocking(handle, &data, 1)) {
        count++;
    }
    return count;
}

static inline size_t i2c_intercom_data_receive(const FuriHalI2cBusHandle* handle, I2CIntercom* instance) {
    size_t total = 0;
    size_t len = 0;
    do {
        uint8_t data;
        len = furi_hal_i2c_slave_read_blocking(handle, &data, 1);
        if(len) {
            bool valid = false;
            with_i2c_register({ valid = i2c_register_write(instance->mem_address, data); });
            if(!valid) {
                instance->invalid_write_count++;
            }
            instance->mem_address++;
        }
        total += len;
    } while(len);
    return total;
}

static inline bool i2c_intercom_receive_address_to(const FuriHalI2cBusHandle* handle, size_t* mem_address) {
    uint8_t data_add[2];
    uint8_t len = furi_hal_i2c_slave_read_blocking(handle, data_add, 2);
    if(len == 2) {
        *mem_address = ((size_t)data_add[0] << 8) | data_add[1];
        return true;
    } else {
        return false;
    }
}

static inline I2CIntercomState i2c_intercom_receive_address(const FuriHalI2cBusHandle* handle, size_t* mem_address) {
    if(i2c_intercom_receive_address_to(handle, mem_address)) {
        return I2CIntercomStateAddressSet;
    } else {
        return I2CIntercomStateAddressNoSet;
    }
}

static inline I2CIntercomState i2c_intercom_address_then_data(const FuriHalI2cBusHandle* handle, I2CIntercom* instance) {
    I2CIntercomState next = i2c_intercom_receive_address(handle, (size_t*)&instance->mem_address);
    if(next == I2CIntercomStateAddressSet) {
        i2c_intercom_data_receive(handle, instance);
    }
    return next;
}

static I2CIntercomState __isr __not_in_flash_func(i2c_intercom_idle_state)(const FuriHalI2cBusHandle* handle, I2CIntercom* instance, FuriHalI2cBusSlaveEvent event) {
    switch(event) {
    case FuriHalI2cBusSlaveEventWrite:
        // Late bytes after transaction close
        instance->dropped_byte_count += i2c_intercom_drain(handle);
        break;
    case FuriHalI2cBusSlaveEventRepeatedStart:
        instance->mem_address = I2C_INTERCOM_DEFAULT_ADDRESS_REGISTER;
        break;
    default:
        break;
    }
    return I2CIntercomStateIdle;
}

static I2CIntercomState __isr __not_in_flash_func(i2c_intercom_start_state)(const FuriHalI2cBusHandle* handle, I2CIntercom* instance, FuriHalI2cBusSlaveEvent event) {
    switch(event) {
    case FuriHalI2cBusSlaveEventWrite:
        return i2c_intercom_address_then_data(handle, instance);
    case FuriHalI2cBusSlaveEventRepeatedStart:
        // Failed re-read keeps the previous address (deliberate)
        return i2c_intercom_receive_address(handle, (size_t*)&instance->mem_address);
    case FuriHalI2cBusSlaveEventStop:
        i2c_intercom_address_then_data(handle, instance);
        return I2CIntercomStateIdle;
    default:
        return I2CIntercomStateStart;
    }
}

static I2CIntercomState __isr __not_in_flash_func(i2c_intercom_address_set_state)(const FuriHalI2cBusHandle* handle, I2CIntercom* instance, FuriHalI2cBusSlaveEvent event) {
    switch(event) {
    case FuriHalI2cBusSlaveEventWrite:
        i2c_intercom_data_receive(handle, instance);
        return I2CIntercomStateAddressSet;
    case FuriHalI2cBusSlaveEventStop:
        i2c_intercom_data_receive(handle, instance);
        return I2CIntercomStateIdle;
    default:
        return I2CIntercomStateAddressSet;
    }
}

static I2CIntercomState __isr __not_in_flash_func(i2c_intercom_address_noset_state)(const FuriHalI2cBusHandle* handle, I2CIntercom* instance, FuriHalI2cBusSlaveEvent event) {
    switch(event) {
    case FuriHalI2cBusSlaveEventWrite:
        // No address: drain instead of holding the bus stretched
        instance->dropped_byte_count += i2c_intercom_drain(handle);
        break;
    case FuriHalI2cBusSlaveEventRepeatedStart:
        instance->mem_address = I2C_INTERCOM_DEFAULT_ADDRESS_REGISTER;
        break;
    case FuriHalI2cBusSlaveEventStop:
        return I2CIntercomStateIdle;
    default:
        break;
    }
    return I2CIntercomStateAddressNoSet;
}

static I2CIntercomState __isr __not_in_flash_func(i2c_intercom_data_transmitted_state)(const FuriHalI2cBusHandle* handle, I2CIntercom* instance, FuriHalI2cBusSlaveEvent event) {
    switch(event) {
    case FuriHalI2cBusSlaveEventWrite:
        // Bytes after a read belong to no transaction: drain
        instance->dropped_byte_count += i2c_intercom_drain(handle);
        return I2CIntercomStateDataTransmitted;
    case FuriHalI2cBusSlaveEventRepeatedStart:
        // Failed re-read keeps the previous address (deliberate)
        return i2c_intercom_receive_address(handle, (size_t*)&instance->mem_address);
    case FuriHalI2cBusSlaveEventStop:
        return I2CIntercomStateIdle;
    default:
        return I2CIntercomStateDataTransmitted;
    }
}

typedef I2CIntercomState (*I2CIntercomStateHandler)(const FuriHalI2cBusHandle* handle, I2CIntercom* instance, FuriHalI2cBusSlaveEvent event);

// Explicit .data section: the ISR must not read from flash while a flash
// operation holds XIP, and the compiler promotes static const-like arrays to .rodata.
static I2CIntercomStateHandler i2c_intercom_state_handlers[] __attribute__((section(".data"))) = {
    [I2CIntercomStateIdle] = i2c_intercom_idle_state,
    [I2CIntercomStateStart] = i2c_intercom_start_state,
    [I2CIntercomStateAddressSet] = i2c_intercom_address_set_state,
    [I2CIntercomStateAddressNoSet] = i2c_intercom_address_noset_state,
    [I2CIntercomStateDataTransmitted] = i2c_intercom_data_transmitted_state,
};

void __isr __not_in_flash_func(i2c_intercom_isr)(const FuriHalI2cBusHandle* handle, FuriHalI2cBusSlaveEvent event, void* context) {
    I2CIntercom* instance = context;

    if(event == FuriHalI2cBusSlaveEventStart) {
        instance->state = I2CIntercomStateStart;
        return;
    }
    if(event == FuriHalI2cBusSlaveEventRead) {
        instance->state = I2CIntercomStateDataTransmitted;
        i2c_intercom_data_transmit(handle, instance);
        return;
    }

    if((size_t)instance->state < COUNT_OF(i2c_intercom_state_handlers)) {
        instance->state = i2c_intercom_state_handlers[instance->state](handle, instance, event);
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
    instance->state = I2CIntercomStateIdle;
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

        if(invalid_reads || invalid_writes || dropped_bytes) {
            FURI_LOG_W(
                TAG,
                "invalid access: %lu reads, %lu writes, %lu dropped bytes",
                invalid_reads,
                invalid_writes,
                dropped_bytes);
        }
    }

    return 0;
}
