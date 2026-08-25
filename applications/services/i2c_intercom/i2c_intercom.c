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
#define I2C_INTERCOM_TIMEOUT_MS               700
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
    alarm_id_t timeout_alarm;

    volatile I2CIntercomState state;
    volatile size_t mem_address;
};

static int64_t __isr __not_in_flash_func(i2c_intercom_timeout_callback)(alarm_id_t id, __unused void* user_data) {
    UNUSED(id);
    I2CIntercom* instance = user_data;

    instance->state = I2CIntercomStateIdle;
    furi_hal_i2c_slave_bus_reset(instance->bus_handle);
    return 0;
}

static inline void i2c_intercom_data_transmit(const FuriHalI2cBusHandle* handle, I2CIntercom* instance) {
    uint8_t data;
    with_i2c_register({
        if(!i2c_register_read_start(instance->mem_address, &data)) {
            data = I2C_INTERCOM_INVALID_ADDRESS_VALUE;
            FURI_LOG_W(TAG, "read from invalid addr 0x%04X", instance->mem_address);
        }
        uint8_t len = furi_hal_i2c_slave_write_blocking(handle, &data, 1);
        if(len) {
            i2c_register_read_commit(instance->mem_address);
            instance->mem_address++;
        }
    });
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
                FURI_LOG_W(TAG, "write to invalid addr 0x%04X", instance->mem_address);
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

void __isr __not_in_flash_func(i2c_intercom_isr)(const FuriHalI2cBusHandle* handle, FuriHalI2cBusSlaveEvent event, void* context) {
    I2CIntercom* instance = context;
    switch(event) {
    case FuriHalI2cBusSlaveEventStart:
        // Master has sent a Start signal, prepare to receive address
        instance->state = I2CIntercomStateStart;
        instance->timeout_alarm = add_alarm_in_ms(I2C_INTERCOM_TIMEOUT_MS, i2c_intercom_timeout_callback, instance, true);
        break;
    case FuriHalI2cBusSlaveEventWrite:
        // Master is writing data to slave
        if(instance->state == I2CIntercomStateStart) {
            instance->state = i2c_intercom_receive_address(handle, (size_t*)&instance->mem_address);
        }
        if(instance->state == I2CIntercomStateAddressSet) {
            i2c_intercom_data_receive(handle, instance);
        }
        break;
    case FuriHalI2cBusSlaveEventRead:
        // Master is requesting data from slave
        instance->state = I2CIntercomStateDataTransmitted;
        i2c_intercom_data_transmit(handle, instance);
        break;
    case FuriHalI2cBusSlaveEventRepeatedStart:
        if(instance->state == I2CIntercomStateStart || instance->state == I2CIntercomStateDataTransmitted) {
            instance->state = i2c_intercom_receive_address(handle, (size_t*)&instance->mem_address);
        }
        if(instance->state == I2CIntercomStateAddressNoSet || instance->state == I2CIntercomStateIdle) {
            instance->mem_address = I2C_INTERCOM_DEFAULT_ADDRESS_REGISTER;
        }
        break;
    case FuriHalI2cBusSlaveEventStop:
        // Master has sent a Stop signal, finalize any ongoing operations
        if(instance->state == I2CIntercomStateStart) {
            instance->state = i2c_intercom_receive_address(handle, (size_t*)&instance->mem_address);
        }
        if(instance->state == I2CIntercomStateAddressSet) {
            i2c_intercom_data_receive(handle, instance);
        }

        instance->state = I2CIntercomStateIdle;
        cancel_alarm(instance->timeout_alarm);

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
        furi_delay_ms(FuriWaitForever);
    }

    return 0;
}