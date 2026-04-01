#include <furi.h>
#include <furi_hal_i2c.h>
#include <furi_hal_i2c_config.h>

#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>
#include "i2c_registers_i.h"
#include "i2c_registers_map.h"

// TODO: move somewhere
#include <input/input.h>
#include <input_touch/input_touch.h>

#define TAG                                   "I2cIntercom"
#define I2C_INTERCOM_THREAD_FLAG_ISR          0x00000001
#define I2C_INTERCOM_DEFAULT_ADDRESS_REGISTER 0x00
#define I2C_INTERCOM_TIMEOUT_MS               700
#define I2C_INTERCOM_INVALID_ADDRESS_VALUE    0x00

typedef enum {
    I2cIntercomStateIdle,
    I2cIntercomStateStart,
    I2cIntercomStateAddressSet,
    I2cIntercomStateAddressNoSet,
    I2cIntercomStateDataTransmitted,
} I2cIntercomState;

/** I2C Intercom state */
typedef struct {
    FuriThreadId thread_id;
    const FuriHalI2cBusHandle* bus_handle;
    alarm_id_t timeout_alarm;

    I2cIntercomState state;
    size_t mem_address;

} I2cIntercom;

static int64_t __isr __not_in_flash_func(i2c_intercom_timeout_callback)(alarm_id_t id, __unused void* user_data) {
    UNUSED(id);
    I2cIntercom* instance = user_data;

    instance->state = I2cIntercomStateIdle;
    furi_hal_i2c_slave_bus_reset(instance->bus_handle);
    return 0;
}

static inline void i2c_intercom_data_transmit(const FuriHalI2cBusHandle* handle, I2cIntercom* instance) {
    uint8_t data;
    with_i2c_register({
        if(!i2c_register_read_start(instance->mem_address, &data)) {
            data = I2C_INTERCOM_INVALID_ADDRESS_VALUE;
        }
        uint8_t len = furi_hal_i2c_slave_write_blocking(handle, &data, 1);
        if(len) {
            i2c_register_read_commit(instance->mem_address);
            instance->mem_address++;
        }
    });
}

static inline size_t i2c_intercom_data_receive(const FuriHalI2cBusHandle* handle, I2cIntercom* instance) {
    size_t total = 0;
    size_t len = 0;
    do {
        uint8_t data;
        len = furi_hal_i2c_slave_read_blocking(handle, &data, 1);
        if(len) {
            with_i2c_register({ i2c_register_write(instance->mem_address, data); });
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

static inline I2cIntercomState i2c_intercom_receive_address(const FuriHalI2cBusHandle* handle, size_t* mem_address) {
    if(i2c_intercom_receive_address_to(handle, mem_address)) {
        return I2cIntercomStateAddressSet;
    } else {
        return I2cIntercomStateAddressNoSet;
    }
}

void __isr __not_in_flash_func(i2c_intercom_isr)(const FuriHalI2cBusHandle* handle, FuriHalI2cBusSlaveEvent event, void* context) {
    I2cIntercom* instance = context;
    switch(event) {
    case FuriHalI2cBusSlaveEventStart:
        // Master has sent a Start signal, prepare to receive address
        instance->state = I2cIntercomStateStart;
        instance->timeout_alarm = add_alarm_in_ms(I2C_INTERCOM_TIMEOUT_MS, i2c_intercom_timeout_callback, instance, true);
        break;
    case FuriHalI2cBusSlaveEventWrite:
        // Master is writing data to slave
        if(instance->state == I2cIntercomStateStart) {
            instance->state = i2c_intercom_receive_address(handle, &instance->mem_address);
        }
        if(instance->state == I2cIntercomStateAddressSet) {
            i2c_intercom_data_receive(handle, instance);
        }
        break;
    case FuriHalI2cBusSlaveEventRead:
        // Master is requesting data from slave
        instance->state = I2cIntercomStateDataTransmitted;
        i2c_intercom_data_transmit(handle, instance);
        break;
    case FuriHalI2cBusSlaveEventRepeatedStart:
        if(instance->state == I2cIntercomStateStart || instance->state == I2cIntercomStateDataTransmitted) {
            instance->state = i2c_intercom_receive_address(handle, &instance->mem_address);
        }
        if(instance->state == I2cIntercomStateAddressNoSet || instance->state == I2cIntercomStateIdle) {
            instance->mem_address = I2C_INTERCOM_DEFAULT_ADDRESS_REGISTER;
        }
        break;
    case FuriHalI2cBusSlaveEventStop:
        // Master has sent a Stop signal, finalize any ongoing operations
        if(instance->state == I2cIntercomStateStart) {
            instance->state = i2c_intercom_receive_address(handle, &instance->mem_address);
        }
        if(instance->state == I2cIntercomStateAddressSet) {
            i2c_intercom_data_receive(handle, instance);
        }

        instance->state = I2cIntercomStateIdle;
        cancel_alarm(instance->timeout_alarm);

        break;

    default:
        break;
    }

    // furi_thread_flags_set(instance->thread_id, I2C_INTERCOM_THREAD_FLAG_ISR);
}

static void i2c_registers_input_event_glue(const void* value, void* ctx) {
    UNUSED(ctx);
    furi_check(value);
    InputEvent* event = (InputEvent*)value;
    if(event->type == InputTypePress) {
        with_i2c_register({
            i2c_register_update(I2C_BUTTONS_STATE_REG_ADDRESS, event->key, event->key);
            i2c_register_set_interrupt(I2C_INPUT_INTERRUPT_REG_ADDRESS, 1 << I2C_INPUT_INTERRUPT_REG_BIT_BUTTONS);
        });
    } else if(event->type == InputTypeRelease) {
        with_i2c_register({
            i2c_register_update(I2C_BUTTONS_STATE_REG_ADDRESS, 0, event->key);
            i2c_register_set_interrupt(I2C_INPUT_INTERRUPT_REG_ADDRESS, 1 << I2C_INPUT_INTERRUPT_REG_BIT_BUTTONS);
        });
    }
}

static void i2c_registers_input_touch_event_glue(const void* value, void* ctx) {
    UNUSED(ctx);
    furi_check(value);
    InputTouchEvent* event = (InputTouchEvent*)value;
    if(event->type == InputTouchTypeStart || event->type == InputTouchTypeMove || event->type == InputTouchTypeEnd) {
        with_i2c_register({
            if(event->type == InputTouchTypeStart || event->type == InputTouchTypeMove) {
                i2c_register_update(I2C_TOUCHPAD_X_REG_ADDRESS, event->x, 0xFFFF);
                i2c_register_update(I2C_TOUCHPAD_Y_REG_ADDRESS, event->y, 0xFFFF);
            }
            i2c_register_update(I2C_TOUCHPAD_PRESS_REG_ADDRESS, event->pressure, 0xFFFF);
            i2c_register_set_interrupt(I2C_INPUT_INTERRUPT_REG_ADDRESS, 1 << I2C_INPUT_INTERRUPT_REG_BIT_TOUCHPAD);
        });
    }
}

int32_t i2c_intercom_srv(void* p) {
    UNUSED(p);

    I2cIntercom* instance = malloc(sizeof(I2cIntercom));
    instance->thread_id = furi_thread_get_current_id();
    instance->bus_handle = &furi_hal_i2c_handle_cpu;

    i2c_registers_init();

    // Version
    i2c_register_add(I2C_INTERCOM_VERSION_REG_ADDRESS, I2C_INTERCOM_VERSION, I2CRegFlagRead);

    // Input
    // Interrupt register
    i2c_register_add_interrupt(I2C_INPUT_INTERRUPT_REG_ADDRESS, I2C_INPUT_INTERRUPT_MASK_REG_ADDRESS, I2C_STATUS_REG_BIT_INPUT);

    // Buttons state
    i2c_register_add(I2C_BUTTONS_STATE_REG_ADDRESS, 0, I2CRegFlagRead);
    furi_pubsub_subscribe(furi_record_open(RECORD_INPUT_EVENTS), i2c_registers_input_event_glue, NULL);

    // Touchpad state
    i2c_register_add(I2C_TOUCHPAD_X_REG_ADDRESS, 0, I2CRegFlagRead);
    i2c_register_add(I2C_TOUCHPAD_Y_REG_ADDRESS, 0, I2CRegFlagRead);
    i2c_register_add(I2C_TOUCHPAD_PRESS_REG_ADDRESS, 0, I2CRegFlagRead);
    furi_pubsub_subscribe(furi_record_open(RECORD_INPUT_TOUCH_EVENTS), i2c_registers_input_touch_event_glue, NULL);

    furi_hal_i2c_acquire(instance->bus_handle);
    furi_hal_i2c_slave_set_callback(instance->bus_handle, i2c_intercom_isr, instance);
    instance->state = I2cIntercomStateIdle;
    instance->mem_address = I2C_INTERCOM_DEFAULT_ADDRESS_REGISTER;

    while(1) {
        furi_thread_flags_wait(I2C_INTERCOM_THREAD_FLAG_ISR, FuriFlagWaitAny, FuriWaitForever);
    }

    return 0;
}
