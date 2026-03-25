#include "i2c_slave_cpu.h"

#include <furi.h>
#include <furi_hal_i2c.h>
#include <furi_hal_i2c_config.h>

#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>

#define TAG                                    "I2cSlaveCpu"
#define I2C_SLAVE_CPU_THREAD_FLAG_ISR          0x00000001
#define I2C_SLAVE_CPU_DEFAULT_ADDRESS_REGISTER 0x00
#define I2C_SLAVE_CPU_TIMEOUT_MS               700

typedef enum {
    I2cSlaveCpuStateIdle,
    I2cSlaveCpuStateStart,
    I2cSlaveCpuStateAddressSet,
    I2cSlaveCpuStateAddressNoSet,
    I2cSlaveCpuStateDataTransmitted,
} I2cSlaveCpuState;

/** I2C slave state */
typedef struct {
    FuriPubSub* event_pubsub;
    FuriThreadId thread_id;
    const FuriHalI2cBusHandle* bus_handle;
    alarm_id_t timeout_alarm;

    I2cSlaveCpuState state;
    uint16_t mem_address;

    uint8_t* test_buffer;
    size_t test_buffer_size;
} I2cSlaveCpu;

static int64_t __isr __not_in_flash_func(i2c_slave_cpu_timeout_callback)(alarm_id_t id, __unused void* user_data) {
    UNUSED(id);
    I2cSlaveCpu* instance = user_data;

    instance->state = I2cSlaveCpuStateIdle;
    furi_hal_i2c_slave_bus_reset(instance->bus_handle);
    return 0;
}

static inline void i2c_slave_cpu_data_transmit(const FuriHalI2cBusHandle* handle, I2cSlaveCpu* instance) {
    uint8_t max_len = 16; // max 16 bytes can be transmitted, if more is requested, it will be sent in next events

    uint8_t len = 0;
    do {
        uint8_t data = instance->test_buffer[instance->mem_address & 0xFF];
        len = furi_hal_i2c_slave_write_blocking(handle, &data, 1);
        if(len) {
            instance->mem_address++;
        }
    } while(len);
}

static inline size_t i2c_slave_cpu_receive_data(const FuriHalI2cBusHandle* handle, uint8_t* data, size_t max_len) {
    size_t total = 0;
    size_t len = 0;
    do {
        len = furi_hal_i2c_slave_read_blocking(handle, &data[total], 1);
        total += len;
    } while(len && total < max_len);
    return total;
}

static inline bool i2c_slave_cpu_receive_address_to(const FuriHalI2cBusHandle* handle, uint16_t* mem_address) {
    uint8_t data_add[2];
    uint8_t len = furi_hal_i2c_slave_read_blocking(handle, data_add, 2);
    if(len == 2) {
        *mem_address = ((uint16_t)data_add[0] << 8) | data_add[1];
        return true;
    } else {
        return false;
    }
}

static inline I2cSlaveCpuState i2c_slave_cpu_receive_address(const FuriHalI2cBusHandle* handle, uint16_t* mem_address) {
    if(i2c_slave_cpu_receive_address_to(handle, mem_address)) {
        return I2cSlaveCpuStateAddressSet;
    } else {
        return I2cSlaveCpuStateAddressNoSet;
    }
}

void __isr __not_in_flash_func(i2c_slave_cpu_isr)(const FuriHalI2cBusHandle* handle, FuriHalI2cBusSlaveEvent event, void* context) {
    I2cSlaveCpu* instance = context;
    switch(event) {
    case FuriHalI2cBusSlaveEventStart:
        // Master has sent a Start signal, prepare to receive address
        instance->state = I2cSlaveCpuStateStart;
        instance->timeout_alarm = add_alarm_in_ms(I2C_SLAVE_CPU_TIMEOUT_MS, i2c_slave_cpu_timeout_callback, instance, true);
        break;
    case FuriHalI2cBusSlaveEventWrite:
        // Master is writing data to slave
        if(instance->state == I2cSlaveCpuStateStart) {
            instance->state = i2c_slave_cpu_receive_address(handle, &instance->mem_address);
        }
        if(instance->state == I2cSlaveCpuStateAddressSet) {
            i2c_slave_cpu_receive_data(
                handle, &instance->test_buffer[instance->mem_address & 0xFF], instance->test_buffer_size - (instance->mem_address & 0xFF));
        }
        break;
    case FuriHalI2cBusSlaveEventRead:
        // Master is requesting data from slave
        instance->state = I2cSlaveCpuStateDataTransmitted;
        i2c_slave_cpu_data_transmit(handle, instance);
        break;
    case FuriHalI2cBusSlaveEventRepeatedStart:
        if(instance->state == I2cSlaveCpuStateStart || instance->state == I2cSlaveCpuStateDataTransmitted) {
            instance->state = i2c_slave_cpu_receive_address(handle, &instance->mem_address);
        }
        if(instance->state == I2cSlaveCpuStateAddressNoSet || instance->state == I2cSlaveCpuStateIdle) {
            instance->mem_address = I2C_SLAVE_CPU_DEFAULT_ADDRESS_REGISTER;
        }
        break;
    case FuriHalI2cBusSlaveEventStop:
        // Master has sent a Stop signal, finalize any ongoing operations
        if(instance->state == I2cSlaveCpuStateStart) {
            instance->state = i2c_slave_cpu_receive_address(handle, &instance->mem_address);
        }
        if(instance->state == I2cSlaveCpuStateAddressSet) {
            i2c_slave_cpu_receive_data(
                handle, &instance->test_buffer[instance->mem_address & 0xFF], instance->test_buffer_size - (instance->mem_address & 0xFF));
        }

        instance->state = I2cSlaveCpuStateIdle;
        cancel_alarm(instance->timeout_alarm);

        break;

    default:
        break;
    }

    //furi_thread_flags_set(instance->thread_id, I2C_SLAVE_CPU_THREAD_FLAG_ISR);
}

int32_t i2c_intercom_srv(void* p) {
    UNUSED(p);

    I2cSlaveCpu* instance = (I2cSlaveCpu*)malloc(sizeof(I2cSlaveCpu));
    instance->thread_id = furi_thread_get_current_id();
    instance->event_pubsub = furi_pubsub_alloc();
    instance->bus_handle = &furi_hal_i2c_handle_cpu;

    furi_record_create(RECORD_I2C_SLAVE_CPU, instance->event_pubsub);
    furi_hal_i2c_acquire(instance->bus_handle);
    furi_hal_i2c_slave_set_callback(instance->bus_handle, i2c_slave_cpu_isr, instance);
    instance->state = I2cSlaveCpuStateIdle;
    instance->mem_address = I2C_SLAVE_CPU_DEFAULT_ADDRESS_REGISTER;

    // Test buffer
    instance->test_buffer_size = 256;
    instance->test_buffer = malloc(instance->test_buffer_size);
    for(size_t i = 0; i < instance->test_buffer_size; i++) {
        instance->test_buffer[i] = i;
    }

    while(1) {
        furi_thread_flags_wait(I2C_SLAVE_CPU_THREAD_FLAG_ISR, FuriFlagWaitAny, FuriWaitForever);
        // Nothing
    }

    return 0;
}
