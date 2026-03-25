#include <furi.h>
#include <m-dict.h>

#include "i2c_registers.h"

/**
 * Device address: 0x69
 * 
 * Register map:
 * 0x0000+0 Status register             (read)
 *          Bit 0: Input interrupt flag (cleared when input interrupt register is cleared)
 *          Bit 1-15: Reserved
 * 
 * 0x0100+0 Input interrupt register    (read, read to clear)
 *          Bit 0: Buttons input happened
 *          Bit 1: Touchpad input happened
 *          Bit 2-15: Reserved
 * 0x0100+2 Buttons state register      (read)
 *          Bit 0: InputKey2 state
 *          Bit 1: InputKey1 state
 *          Bit 2: InputKey3 state
 *          Bit 3: InputKey4 state
 *          Bit 4: InputKey5 state
 *          Bit 5: InputKeySw state
 *          Bit 6: InputKeyBack state
 *          Bit 7: InputKeyDown state
 *          Bit 8: InputKeyRight state
 *          Bit 9: InputKeyOk state
 *          Bit 10: InputKeyLeft state
 *          Bit 11: InputKeyUp state
 *          Bit 12: InputKeyPtt state
 *          Bit 13-15: Reserved
 * 0x0100+4 Touchpad X position         (read)
 * 0x0100+6 Touchpad Y position         (read)
 * 0x0100+8 Touchpad press state        (read)
 */

typedef struct {
    uint32_t flags;
    uint16_t value;
} I2CReg;

DICT_DEF2(I2CRegMap, uint16_t, M_DEFAULT_OPLIST, I2CReg, M_POD_OPLIST);

static I2CRegMap_t i2c_registers;
static const uint16_t i2c_address_to_status_bits_map[] = {
    [0] = 0x0100 + 0, // bit 0 is input interrupt register
};
static const size_t i2c_address_to_status_bits_map_size = COUNT_OF(i2c_address_to_status_bits_map);

#include <input/input.h>

static void i2c_registers_input_event_glue(const void* value, void* ctx) {
    UNUSED(ctx);
    furi_check(value);
    InputEvent* event = (InputEvent*)value;
    if(event->type == InputTypePress) {
        i2c_register_update_and_set_interrupt(0x0102, event->key, event->key, 0x0100, 1 << 0);
    } else if(event->type == InputTypeRelease) {
        i2c_register_update_and_set_interrupt(0x0102, 0, event->key, 0x0100, 1 << 0);
    }
}

void i2c_registers_init(void) {
    furi_hal_gpio_init(&gpio_cpu_int, GpioModeOutputOpenDrain, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(&gpio_m40, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_write(&gpio_cpu_int, true);
    furi_hal_gpio_write(&gpio_m40, false);

    I2CRegMap_init(i2c_registers);

    i2c_register_add(0x0100, 0x0000, I2CRegFlagRead | I2CRegFlagReadToClear);
    i2c_register_add(0x0102, 0x0000, I2CRegFlagRead);

    furi_pubsub_subscribe(furi_record_open(RECORD_INPUT_EVENTS), i2c_registers_input_event_glue, NULL);
}

void i2c_register_add(uint16_t address, uint16_t default_value, uint32_t flags) {
    FURI_CRITICAL_ENTER();
    furi_check(address % 2 == 0); // only even addresses are valid
    furi_check(I2CRegMap_get(i2c_registers, address) == NULL); // address must not exist

    I2CReg reg = {.value = default_value, .flags = flags};
    I2CRegMap_set_at(i2c_registers, address, reg);
    FURI_CRITICAL_EXIT();
}

uint16_t i2c_register_get_status_register(void) {
    uint16_t status_value = 0x0000;
    for(size_t i = 0; i < i2c_address_to_status_bits_map_size; i++) {
        uint16_t reg_address = i2c_address_to_status_bits_map[i];
        I2CReg* reg = I2CRegMap_get(i2c_registers, reg_address);
        if(reg) {
            status_value |= (reg->value != 0) ? (1 << i) : 0;
        }
    }
    return status_value;
}

bool i2c_register_read_start(uint16_t address, uint8_t* value) {
    FURI_CRITICAL_ENTER();

    bool result = false;
    bool is_odd = address & 1;
    uint16_t even_address = address & 0xFFFE;

    do {
        if(even_address == 0x00) {
            // high byte of status register
            uint16_t status = i2c_register_get_status_register();

            // big-endian
            if(is_odd) {
                *value = status & 0xFF;
            } else {
                *value = (status >> 8) & 0xFF;
            }
            result = true;
            break;
        }

        I2CReg* reg = I2CRegMap_get(i2c_registers, even_address);
        if(reg && (reg->flags & I2CRegFlagRead)) {
            // big-endian
            if(is_odd) {
                *value = reg->value & 0xFF;
            } else {
                *value = (reg->value >> 8) & 0xFF;
            }

            result = true;
        }
    } while(false);
    FURI_CRITICAL_EXIT();

    return result;
}

bool i2c_register_read_commit(uint16_t address) {
    // special case for status register
    if(address == 0x00 || address == 0x01) {
        return true;
    }

    bool result = false;
    bool is_odd = address & 1;
    uint16_t even_address = address & 0xFFFE;

    FURI_CRITICAL_ENTER();
    do {
        I2CReg* reg = I2CRegMap_get(i2c_registers, even_address);
        if(reg && (reg->flags & I2CRegFlagReadToClear)) {
            if(is_odd) {
                reg->value &= 0xFF00;
            } else {
                reg->value &= 0x00FF;
            }

            if(i2c_register_get_status_register() == 0) {
                furi_hal_gpio_write_open_drain(&gpio_cpu_int, true);
                furi_hal_gpio_write(&gpio_m40, false);
            }

            result = true;
        }
    } while(false);
    FURI_CRITICAL_EXIT();

    return result;
}

bool i2c_register_write(uint16_t address, uint8_t value) {
    bool result = false;
    bool is_odd = address & 1;
    uint16_t even_address = address & 0xFFFE;

    FURI_CRITICAL_ENTER();
    do {
        I2CReg* reg = I2CRegMap_get(i2c_registers, even_address);
        if(reg && (reg->flags & I2CRegFlagWrite)) {
            // big-endian
            if(is_odd) {
                reg->value = (reg->value & 0xFF00) | value;
            } else {
                reg->value = (reg->value & 0x00FF) | ((uint16_t)value << 8);
            }
            result = true;
        }
    } while(false);
    FURI_CRITICAL_EXIT();

    return result;
}

void i2c_register_update(uint16_t address, uint16_t value, uint16_t mask) {
    FURI_CRITICAL_ENTER();
    do {
        I2CReg* reg = I2CRegMap_get(i2c_registers, address);
        furi_check(reg); // address must exist
        reg->value = (reg->value & ~mask) | (value & mask);
    } while(false);
    FURI_CRITICAL_EXIT();
}

void i2c_register_update_and_set_interrupt(uint16_t address, uint16_t value, uint16_t mask, uint16_t interrupt_address, uint16_t interrupt_bits) {
    FURI_CRITICAL_ENTER();
    do {
        I2CReg* reg = I2CRegMap_get(i2c_registers, address);
        furi_check(reg); // address must exist
        reg->value = (reg->value & ~mask) | (value & mask);
        // Set interrupt if needed
        if(interrupt_address) {
            I2CReg* interrupt_reg = I2CRegMap_get(i2c_registers, interrupt_address);
            furi_check(interrupt_reg); // interrupt address must exist
            interrupt_reg->value |= interrupt_bits;

            if(i2c_register_get_status_register() != 0) {
                furi_hal_gpio_write_open_drain(&gpio_cpu_int, false);
                furi_hal_gpio_write(&gpio_m40, true);
            }
        }
    } while(false);
    FURI_CRITICAL_EXIT();
}
