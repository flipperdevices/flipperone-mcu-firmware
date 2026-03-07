#include "i2c_slave_cpu.h"

#include "furi_hal_i2c_types.h"
#include <furi.h>
#include <furi_hal_i2c.h>
#include <furi_hal_i2c_config.h>

#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>
#include <hardware/i2c.h>
#include <stdbool.h>

#define TAG                                    "I2cSlaveCpu"
#define I2C_SLAVE_CPUTHREAD_FLAG_ISR           0x00000001
#define I2C_SLAVE_CPU_DEFAULT_ADDRESS_REGISTER 0x00

typedef enum {
    I2cSlaveCpuStateIdle,
    I2cSlaveCpuStateReceivingAddressHighByte,
    //I2cSlaveCpuStateReceivingAddressLowByte,
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

inline void wait(volatile uint32_t count) {
    while(count--) {
        __asm volatile("nop");
    }
}

void __isr __not_in_flash_func(i2c_slave_cpu_isr)(const FuriHalI2cBusHandle* handle, FuriHalI2cBusSlaveEvent event, void* context) {
    I2cSlaveCpu* instance = context;
    switch(event) {
    case FuriHalI2cBusSlaveEventStart:
        // Master has sent a Start signal, prepare to receive address
        instance->state = I2cSlaveCpuStateIdle;
        furi_hal_gpio_write(&gpio_m41, true);
        wait(30);
        furi_hal_gpio_write(&gpio_m41, false);
        break;

    case FuriHalI2cBusSlaveEventReceive:
        // furi_hal_gpio_write(&gpio_m40, true);
        // wait(30);
        // furi_hal_gpio_write(&gpio_m40, false);
        break;

    case FuriHalI2cBusSlaveEventRequest:
        // Master is requesting data from slave
        furi_hal_gpio_write(&gpio_m41, true);
        for(size_t i = 0; i < 16; i++) {
            uint8_t data = instance->test_buffer[instance->mem_address & 0xFF];
            furi_hal_i2c_slave_write_blocking(handle, data);
            instance->mem_address++;
        }
        furi_hal_gpio_write(&gpio_m41, false);
        break;
    case FuriHalI2cBusSlaveEventRepeatedStart:
        furi_hal_gpio_write(&gpio_m41, true);
        wait(30);
        furi_hal_gpio_write(&gpio_m41, false);
        wait(30);
        furi_hal_gpio_write(&gpio_m41, true);
        wait(30);
        furi_hal_gpio_write(&gpio_m41, false);
        // if(instance->state == I2cSlaveCpuStateIdle) {
        furi_hal_gpio_write(&gpio_m40, true);
        if(i2c1->hw->rxflr > 1) {
            // Address byte received, read it and prepare for data transfer
            instance->mem_address = furi_hal_i2c_slave_read_blocking(handle);
            instance->mem_address <<= 8;
            instance->mem_address |= furi_hal_i2c_slave_read_blocking(handle);
            instance->state = I2cSlaveCpuStateReceivTransmitData;
        }
        wait(10);
        furi_hal_gpio_write(&gpio_m40, false);
        // while(i2c1->hw->rxflr) {
        //     // clear RX FIFO
        //     furi_hal_i2c_slave_read_blocking(handle);
        // }
        //instance->state = ;
        // furi_hal_gpio_write(&gpio_m41, true);
        // for(size_t i = 0; i < 32; i++) {
        //     uint8_t data = instance->test_buffer[instance->mem_address & 0xFF];
        //     furi_hal_i2c_slave_write_blocking(handle, data);
        //     instance->mem_address++;
        // }
        // furi_hal_gpio_write(&gpio_m41, false);
        // }
        break;
    case FuriHalI2cBusSlaveEventStop:
        furi_hal_gpio_write(&gpio_m41, true);
        wait(30);
        furi_hal_gpio_write(&gpio_m41, false);
        wait(30);
        furi_hal_gpio_write(&gpio_m41, true);
        wait(30);
        furi_hal_gpio_write(&gpio_m41, false);
        wait(30);
        furi_hal_gpio_write(&gpio_m41, true);
        wait(30);
        furi_hal_gpio_write(&gpio_m41, false);

        //if(instance->state == I2cSlaveCpuStateIdle) {
        furi_hal_gpio_write(&gpio_m40, true);
        if(i2c1->hw->rxflr > 1) {
            // Address byte received, read it and prepare for data transfer
            instance->mem_address = furi_hal_i2c_slave_read_blocking(handle);
            instance->mem_address <<= 8;
            instance->mem_address |= furi_hal_i2c_slave_read_blocking(handle);
            instance->state = I2cSlaveCpuStateReceivTransmitData;
        }
        wait(10);
        furi_hal_gpio_write(&gpio_m40, false);
        while(i2c1->hw->rxflr) {
            furi_hal_gpio_write(&gpio_m41, true);
            uint8_t data = furi_hal_i2c_slave_read_blocking(handle);
            instance->test_buffer[instance->mem_address & 0xFF] = data;
            instance->mem_address++;
            furi_hal_gpio_write(&gpio_m41, false);
            wait(10);
        }
        // }
        instance->state = I2cSlaveCpuStateIdle;

        break;

    default:
        break;
    }

    furi_thread_flags_set(instance->thread_id, I2C_SLAVE_CPUTHREAD_FLAG_ISR);
}

int32_t i2c_slave_cpu_srv(void* p) {
    UNUSED(p);

    I2cSlaveCpu* instance = (I2cSlaveCpu*)malloc(sizeof(I2cSlaveCpu));
    instance->thread_id = furi_thread_get_current_id();
    instance->event_pubsub = furi_pubsub_alloc();

    furi_record_create(RECORD_I2C_SLAVE_CPU, instance->event_pubsub);
    furi_hal_i2c_acquire(&furi_hal_i2c_handle_cpu);
    furi_hal_i2c_slave_set_callback(&furi_hal_i2c_handle_cpu, i2c_slave_cpu_isr, instance);
    instance->state = I2cSlaveCpuStateIdle;
    instance->mem_address = I2C_SLAVE_CPU_DEFAULT_ADDRESS_REGISTER;

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
