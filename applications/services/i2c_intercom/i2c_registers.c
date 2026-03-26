#include <furi.h>
#include <furi_hal_resources.h>
#include <m-dict.h>

#include "i2c_registers.h"
#include "i2c_registers_map.h"

typedef struct {
    uint32_t flags;
    uint16_t value;
} I2CReg;

DICT_DEF2(I2CRegMap, uint16_t, M_DEFAULT_OPLIST, I2CReg, M_POD_OPLIST);

#define I2C_ADDRESS_TO_STATUS_BITS_MAP_SIZE (sizeof(uint16_t) * 8)

// hashmap of registers, key is register address, value is register struct
static I2CRegMap_t i2c_registers;

// map of status register bits to register addresses, index is status bit, value is register address
static uint16_t i2c_address_to_status_bits_map[I2C_ADDRESS_TO_STATUS_BITS_MAP_SIZE] = {0};
static size_t i2c_address_to_status_bits_map_max_bit = 0;

static I2CReg i2c_status_register = {.value = 0x0000, .flags = I2CRegFlagRead};

static void i2c_register_interrupt_line_set(bool set) {
    if(set) {
        furi_hal_gpio_write_open_drain(&gpio_cpu_int, false);
        furi_hal_gpio_write(&gpio_m40, true);
    } else {
        furi_hal_gpio_write_open_drain(&gpio_cpu_int, true);
        furi_hal_gpio_write(&gpio_m40, false);
    }
}

static void i2c_register_interrupt_line_init(void) {
    furi_hal_gpio_init(&gpio_cpu_int, GpioModeOutputOpenDrain, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(&gpio_m40, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    i2c_register_interrupt_line_set(false);
}

void i2c_address_to_status_bits_map_add(uint8_t bit, uint16_t address) {
    furi_check(bit < I2C_ADDRESS_TO_STATUS_BITS_MAP_SIZE);
    furi_check(i2c_address_to_status_bits_map[bit] == 0); // bit must not be already assigned
    i2c_address_to_status_bits_map[bit] = address;
    if(bit >= i2c_address_to_status_bits_map_max_bit) {
        i2c_address_to_status_bits_map_max_bit = bit + 1;
    }
}

void i2c_registers_init(void) {
    i2c_register_interrupt_line_init();

    I2CRegMap_init(i2c_registers);
}

void i2c_register_add(uint16_t address, uint16_t default_value, uint32_t flags) {
    FURI_CRITICAL_ENTER();
    furi_check(address % 2 == 0); // only even addresses are valid
    furi_check(I2CRegMap_get(i2c_registers, address) == NULL); // address must not exist

    I2CReg reg = {.value = default_value, .flags = flags};
    I2CRegMap_set_at(i2c_registers, address, reg);
    FURI_CRITICAL_EXIT();
}

uint16_t i2c_register_get_status_register_value(void) {
    uint16_t status_value = 0x0000;
    for(size_t i = 0; i < i2c_address_to_status_bits_map_max_bit; i++) {
        uint16_t reg_address = i2c_address_to_status_bits_map[i];
        I2CReg* reg = I2CRegMap_get(i2c_registers, reg_address);
        if(reg) {
            status_value |= (reg->value != 0) ? (1 << i) : 0;
        }
    }
    return status_value;
}

I2CReg* i2c_register_get(uint16_t address) {
    if(address == I2C_STATUS_REG_ADDRESS) {
        i2c_status_register.value = i2c_register_get_status_register_value();
        return &i2c_status_register;
    }
    return I2CRegMap_get(i2c_registers, address);
}

bool i2c_register_read_start(uint16_t address, uint8_t* value) {
    FURI_CRITICAL_ENTER();

    bool result = false;
    bool is_odd = address & 1;
    uint16_t even_address = address & 0xFFFE;

    do {
        I2CReg* reg = i2c_register_get(even_address);
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
    bool result = false;
    bool is_odd = address & 1;
    uint16_t even_address = address & 0xFFFE;

    FURI_CRITICAL_ENTER();
    do {
        I2CReg* reg = i2c_register_get(even_address);
        if(reg && (reg->flags & I2CRegFlagReadToClear)) {
            if(is_odd) {
                reg->value &= 0xFF00;
            } else {
                reg->value &= 0x00FF;
            }

            if(i2c_register_get_status_register_value() == 0) {
                i2c_register_interrupt_line_set(false);
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
        I2CReg* reg = i2c_register_get(even_address);
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

            // we know that we already set some interrupt bits,
            // so we can directly set the interrupt line without checking the status register
            i2c_register_interrupt_line_set(true);
        }
    } while(false);
    FURI_CRITICAL_EXIT();
}
