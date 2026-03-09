#include "i2c_slave_cpu.h"

#include <furi.h>
#include <furi_hal_i2c.h>
#include <furi_hal_i2c_config.h>

#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>

#define TAG                                    "I2cSlaveCpu"
#define I2C_SLAVE_CPU_THREAD_FLAG_ISR          0x00000001
#define I2C_SLAVE_CPU_DEFAULT_ADDRESS_REGISTER 0x00

typedef enum {
    I2cSlaveCpuStateIdle,
    I2cSlaveCpuStateStart,
    I2cSlaveCpuStateAddressSet,
    I2cSlaveCpuStateAddressNoSet,
    I2cSlaveCpuStateDataTransmitted,
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

#ifdef I2C_SLAVE_CPU_DEBUG
inline void wait(volatile uint32_t count) {
    while(count--) {
        __asm volatile("nop");
    }
}
#endif

static inline void i2c_slave_cpu_is_address_received(const FuriHalI2cBusHandle* handle, void* context) {
    I2cSlaveCpu* instance = context;
    uint8_t data_add[2];
    uint8_t len = furi_hal_i2c_slave_read_blocking(handle, data_add, 2);
    if(len == 2) {
        instance->mem_address = ((uint16_t)data_add[0] << 8) | data_add[1];
        instance->state = I2cSlaveCpuStateAddressSet;
    } else {
        instance->state = I2cSlaveCpuStateAddressNoSet;
    }
}

static inline void i2c_slave_cpu_data_received(const FuriHalI2cBusHandle* handle, void* context) {
    I2cSlaveCpu* instance = context;
    uint8_t max_len = 16; // max 16 bytes can be read, if more is sent, it will be read in next events

    // uint8_t len = furi_hal_i2c_slave_read_blocking(handle, &instance->test_buffer[instance->mem_address & 0xFF], max_len);
    // instance->mem_address += len;
    uint8_t len = 0;
    do {
        uint8_t data;
        len = furi_hal_i2c_slave_read_blocking(handle, &data, 1);
        if(len) {
            instance->test_buffer[instance->mem_address & 0xFF] = data;
            instance->mem_address++;
        }
    } while(len);
}

static inline void i2c_slave_cpu_data_transmit(const FuriHalI2cBusHandle* handle, void* context) {
    I2cSlaveCpu* instance = context;
    uint8_t max_len = 16; // max 16 bytes can be transmitted, if more is requested, it will be sent in next events

    // uint8_t len = furi_hal_i2c_slave_write_blocking(handle, &instance->test_buffer[instance->mem_address & 0xFF], max_len);
    // instance->mem_address += len;
    uint8_t len = 0;
    do {
        uint8_t data = instance->test_buffer[instance->mem_address & 0xFF];
        len = furi_hal_i2c_slave_write_blocking(handle, &data, 1);
        if(len) {
            instance->mem_address++;
        }
    } while(len);
}

// ToDo: maybe not necessary
// static inline void i2c_slave_cpu_data_received_clear(const FuriHalI2cBusHandle* handle, void* context) {
//     I2cSlaveCpu* instance = context;
//     uint8_t max_len = 16; // max 16 bytes can be read, if more is sent, it will be read in next events
//     uint8_t data[max_len];
//     furi_hal_i2c_slave_read_blocking(handle, data, max_len);
// }

void __isr __not_in_flash_func(i2c_slave_cpu_isr)(const FuriHalI2cBusHandle* handle, FuriHalI2cBusSlaveEvent event, void* context) {
    I2cSlaveCpu* instance = context;
    switch(event) {
    case FuriHalI2cBusSlaveEventStart:
        // Master has sent a Start signal, prepare to receive address
        instance->state = I2cSlaveCpuStateStart;
#ifdef I2C_SLAVE_CPU_DEBUG
        furi_hal_gpio_write(&gpio_m41, true);
        wait(30);
        furi_hal_gpio_write(&gpio_m41, false);
#endif
        break;

    case FuriHalI2cBusSlaveEventReceive:
        if(instance->state == I2cSlaveCpuStateStart) {
#ifdef I2C_SLAVE_CPU_DEBUG
            furi_hal_gpio_write(&gpio_m40, true);
#endif
            i2c_slave_cpu_is_address_received(handle, context);
#ifdef I2C_SLAVE_CPU_DEBUG
            wait(10);
            furi_hal_gpio_write(&gpio_m40, false);
#endif
        }
        if(instance->state == I2cSlaveCpuStateAddressSet) {
#ifdef I2C_SLAVE_CPU_DEBUG
            furi_hal_gpio_write(&gpio_m41, true);
#endif
            i2c_slave_cpu_data_received(handle, context);
#ifdef I2C_SLAVE_CPU_DEBUG
            furi_hal_gpio_write(&gpio_m41, false);
#endif
        }
        break;

    case FuriHalI2cBusSlaveEventRequest:
        // Master is requesting data from slave
        instance->state = I2cSlaveCpuStateDataTransmitted;
#ifdef I2C_SLAVE_CPU_DEBUG
        furi_hal_gpio_write(&gpio_m41, true);
#endif
        i2c_slave_cpu_data_transmit(handle, context);
#ifdef I2C_SLAVE_CPU_DEBUG
        furi_hal_gpio_write(&gpio_m41, false);
#endif
        break;
    case FuriHalI2cBusSlaveEventRepeatedStart:
#ifdef I2C_SLAVE_CPU_DEBUG
        furi_hal_gpio_write(&gpio_m41, true);
        wait(30);
        furi_hal_gpio_write(&gpio_m41, false);
        wait(30);
        furi_hal_gpio_write(&gpio_m41, true);
        wait(30);
        furi_hal_gpio_write(&gpio_m41, false);
#endif
        if(instance->state == I2cSlaveCpuStateStart || instance->state == I2cSlaveCpuStateDataTransmitted) {
#ifdef I2C_SLAVE_CPU_DEBUG
            furi_hal_gpio_write(&gpio_m40, true);
#endif
            i2c_slave_cpu_is_address_received(handle, context);

#ifdef I2C_SLAVE_CPU_DEBUG
            wait(10);
            furi_hal_gpio_write(&gpio_m40, false);
#endif
        }

        if(instance->state == I2cSlaveCpuStateAddressNoSet || instance->state == I2cSlaveCpuStateIdle) {
            instance->mem_address = I2C_SLAVE_CPU_DEFAULT_ADDRESS_REGISTER;
        }

        break;
    case FuriHalI2cBusSlaveEventStop:
#ifdef I2C_SLAVE_CPU_DEBUG
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
#endif
        if(instance->state == I2cSlaveCpuStateStart) {
#ifdef I2C_SLAVE_CPU_DEBUG
            furi_hal_gpio_write(&gpio_m40, true);
#endif
            i2c_slave_cpu_is_address_received(handle, context);

#ifdef I2C_SLAVE_CPU_DEBUG
            wait(10);
            furi_hal_gpio_write(&gpio_m40, false);
#endif
        }
        if(instance->state == I2cSlaveCpuStateAddressSet) {
#ifdef I2C_SLAVE_CPU_DEBUG
            furi_hal_gpio_write(&gpio_m41, true);
#endif
            i2c_slave_cpu_data_received(handle, context);

#ifdef I2C_SLAVE_CPU_DEBUG
            furi_hal_gpio_write(&gpio_m41, false);
#endif
        }

        instance->state = I2cSlaveCpuStateIdle;

        break;

    default:
        break;
    }

    //furi_thread_flags_set(instance->thread_id, I2C_SLAVE_CPU_THREAD_FLAG_ISR);
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
        furi_thread_flags_wait(I2C_SLAVE_CPU_THREAD_FLAG_ISR, FuriFlagWaitAny, FuriWaitForever);
        // Nothing
    }

    return 0;
}
