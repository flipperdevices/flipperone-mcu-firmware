#include "i2c_slave_cpu.h"
#include "core/thread.h"
#include <furi.h>

#include <furi_hal_i2c.h>
#include <furi_hal_i2c_config.h>
#include <stdint.h>

#define I2C_SLAVE_CPUTHREAD_FLAG_ISR           0x00000001
#define I2C_SLAVE_CPU_DEFAULT_ADDRESS_REGISTER 0x00

typedef enum {
    I2cSlaveCpuStateIdle,
    I2cSlaveCpuStateReceivingAddressHighByte,
    I2cSlaveCpuStateReceivingAddressLowByte,
    I2cSlaveCpuStateReceivTransmitData,
} I2cSlaveCpuState;

/** Input pin state */
typedef struct {
    FuriPubSub* event_pubsub;
    FuriThreadId thread_id;

    I2cSlaveCpuState state;
    uint16_t mem_address;

    uint8_t* test_buffer;
    size_t test_buffer_size;
} I2cSlaveCpu;

// void __isr __not_in_flash_func(i2c_slave_cpuisr)(void* _ctx) {
//     FuriThreadId thread_id = (FuriThreadId)_ctx;
//     furi_thread_flags_set(thread_id, I2C_SLAVE_CPUTHREAD_FLAG_ISR);
// }

void __isr __not_in_flash_func(i2c_slave_cpu_isr)(const FuriHalI2cBusHandle* handle, FuriHalI2cBusSlaveEvent event, void* context) {
    I2cSlaveCpu* instance = context;

    switch(event) {
    case FuriHalI2cBusSlaveEventReceive:
        // Master is writing data to slave
        // Read data from master
        if(instance->state == I2cSlaveCpuStateIdle) {
            // First byte is memory address (for testing)
            instance->mem_address = furi_hal_i2c_slave_read_blocking(handle) << 8;
            instance->state = I2cSlaveCpuStateReceivingAddressHighByte;
        } else if(instance->state == I2cSlaveCpuStateReceivingAddressHighByte) {
            // Second byte is memory address (for testing)
            instance->mem_address |= furi_hal_i2c_slave_read_blocking(handle);
            instance->state = I2cSlaveCpuStateReceivTransmitData;
        } else {
            // Subsequent bytes are data (for testing, just read and discard)
            uint8_t data = furi_hal_i2c_slave_read_blocking(handle);
            (void)data; // Suppress unused variable warning
        }
        break;

    case FuriHalI2cBusSlaveEventRequest:
        // Master is requesting data from slave
        // For testing, write data from test buffer to master

        if(instance->state != I2cSlaveCpuStateReceivTransmitData) {
            // If master is requesting data without writing address first, just send 0
            furi_hal_i2c_slave_write_blocking(handle, 0);

            // // or reading from a fixed address
            // instance->mem_address = I2C_SLAVE_CPU_DEFAULT_ADDRESS_REGISTER;
            // instance->state = I2cSlaveCpuStateReceivTransmitData;
        }

        uint8_t data = instance->test_buffer[instance->mem_address % instance->test_buffer_size];
        furi_hal_i2c_slave_write_blocking(handle, data);
        instance->mem_address++;

        break;

    case FuriHalI2cBusSlaveEventFinish:
        // Master has finished transaction with slave
        // Reset read index for testing
        instance->state = I2cSlaveCpuStateIdle;
        break;

    default:
        break;
    }

    //furi_thread_flags_set(instance->thread_id, I2C_SLAVE_CPUTHREAD_FLAG_ISR);
}

int32_t i2c_slave_cpu_srv(void* p) {
    UNUSED(p);

    I2cSlaveCpu* instance = (I2cSlaveCpu*)malloc(sizeof(I2cSlaveCpu));
    instance->thread_id = furi_thread_get_current_id();
    instance->event_pubsub = furi_pubsub_alloc();

    furi_record_create(RECORD_I2C_SLAVE_CPU, instance->event_pubsub);

    furi_hal_i2c_slave_set_callback(&furi_hal_i2c_handle_cpu, i2c_slave_cpu_isr, instance);
    instance->state = I2cSlaveCpuStateIdle;
    instance->mem_address = 0;

    // Test buffer
    instance->test_buffer_size = 256;
    instance->test_buffer = malloc(instance->test_buffer_size);
    for(size_t i = 0; i < instance->test_buffer_size; i++) {
        instance->test_buffer[i] = i;
    }

    while(1) {
        furi_thread_flags_wait(I2C_SLAVE_CPUTHREAD_FLAG_ISR, FuriFlagWaitAny, FuriWaitForever);
        // Nothing
    }

    return 0;
}
