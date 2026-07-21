#include <furi.h>
#include <furi_hal_resources.h>
#include <m-dict.h>

#include "i2c_registers.h"
#include "i2c_registers_map.h"

typedef enum {
    I2CRegFlagRead = 1 << 0,
    I2CRegFlagWrite = 1 << 1,
    I2CRegFlagReadToClear = 1 << 2,
    I2CRegFlagInterrupt = 1 << 3,
} I2CRegFlag;

typedef struct {
    uint32_t flags;
    uint16_t value;
} I2CReg;

DICT_DEF2(I2CRegMap, uint16_t, M_DEFAULT_OPLIST, I2CReg, M_POD_OPLIST);

typedef struct {
    // pointer to the interrupt mask register corresponding to this interrupt
    I2CReg* mask_reg;
    // which bit in the status register corresponds to this interrupt
    uint8_t status_register_bit;
} I2CInterruptInfo;

DICT_DEF2(I2CInterruptInfoMap, uint16_t, M_DEFAULT_OPLIST, I2CInterruptInfo, M_POD_OPLIST);

typedef struct {
    I2CRegisterCallback callback;
    void* context;
} I2CRegisterCallbackWithContext;

DICT_DEF2(I2CInterruptCallbackMap, uint16_t, M_DEFAULT_OPLIST, I2CRegisterCallbackWithContext, M_POD_OPLIST);

#define I2C_ADDRESS_TO_STATUS_BITS_MAP_SIZE (sizeof(uint16_t) * 8)

#define REG16_GET_LO(v)    ((v) & 0xFF)
#define REG16_GET_HI(v)    (((v) >> 8) & 0xFF)
#define REG16_SET_LO(v, b) ((v) = ((v) & 0xFF00) | (uint8_t)(b))
#define REG16_SET_HI(v, b) ((v) = ((v) & 0x00FF) | ((uint16_t)(uint8_t)(b) << 8))
#define REG16_CLR_LO(v)    ((v) &= 0xFF00)
#define REG16_CLR_HI(v)    ((v) &= 0x00FF)

typedef struct {
    // hashmap of registers, key is register address, value is register struct
    I2CRegMap_t map;
    // hashmap of status registers to their corresponding info
    I2CInterruptInfoMap_t interrupt_info_map;
    // hashmap of interrupt registers to their corresponding callbacks
    I2CInterruptCallbackMap_t interrupt_callback_map;
    // status register pointer
    I2CReg* status_register;
} I2CRegisters;

static I2CRegisters i2c;

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

static I2CReg* i2c_register_get(uint16_t address) {
    return I2CRegMap_get(i2c.map, address);
}

static void i2c_register_set_at(uint16_t address, I2CReg reg) {
    I2CRegMap_set_at(i2c.map, address, reg);
}

static I2CInterruptInfo* i2c_interrupt_info_get(uint16_t address) {
    return I2CInterruptInfoMap_get(i2c.interrupt_info_map, address);
}

static void i2c_interrupt_info_set_at(uint16_t address, I2CInterruptInfo info) {
    I2CInterruptInfoMap_set_at(i2c.interrupt_info_map, address, info);
}

static I2CRegisterCallbackWithContext* i2c_interrupt_callback_get(uint16_t address) {
    return I2CInterruptCallbackMap_get(i2c.interrupt_callback_map, address);
}

static void i2c_interrupt_callback_set_at(uint16_t address, I2CRegisterCallbackWithContext callback) {
    I2CInterruptCallbackMap_set_at(i2c.interrupt_callback_map, address, callback);
}

void i2c_registers_init(void) {
    i2c_register_interrupt_line_init();

    I2CRegMap_init(i2c.map);
    I2CInterruptInfoMap_init(i2c.interrupt_info_map);
    I2CInterruptCallbackMap_init(i2c.interrupt_callback_map);
    i2c_register_add_readable(I2C_STATUS_REG_ADDRESS, 0);
    i2c.status_register = i2c_register_get(I2C_STATUS_REG_ADDRESS);
    furi_check(i2c.status_register);
}

static void i2c_register_add_internal(uint16_t address, uint16_t default_value, uint32_t flags) {
    furi_check(address % 2 == 0); // only even addresses are valid
    furi_check(i2c_register_get(address) == NULL); // address must not exist

    I2CReg reg = {.value = default_value, .flags = flags};
    i2c_register_set_at(address, reg);
}

void i2c_register_add_readable(uint16_t address, uint16_t default_value) {
    FURI_CRITICAL_ENTER();
    i2c_register_add_internal(address, default_value, I2CRegFlagRead);
    FURI_CRITICAL_EXIT();
}

void i2c_register_add_writable(uint16_t address, uint16_t default_value, I2CRegisterCallback write_callback, void* write_callback_context) {
    FURI_CRITICAL_ENTER();
    i2c_register_add_internal(address, default_value, I2CRegFlagRead | I2CRegFlagWrite);
    I2CRegisterCallbackWithContext callback_with_context = {.callback = write_callback, .context = write_callback_context};
    i2c_interrupt_callback_set_at(address, callback_with_context);
    FURI_CRITICAL_EXIT();
}

void i2c_register_add_interrupt(uint16_t address, uint16_t mask_address, uint8_t status_register_bit) {
    FURI_CRITICAL_ENTER();
    i2c_register_add_internal(address, 0x0000, I2CRegFlagRead | I2CRegFlagReadToClear | I2CRegFlagInterrupt);
    i2c_register_add_internal(mask_address, 0x0000, I2CRegFlagRead | I2CRegFlagWrite);
    I2CInterruptInfo interrupt = {.mask_reg = i2c_register_get(mask_address), .status_register_bit = status_register_bit};
    i2c_interrupt_info_set_at(address, interrupt);
    FURI_CRITICAL_EXIT();
}

bool i2c_register_read_start(uint16_t address, uint8_t* value) {
    bool result = false;
    bool is_hi_byte = address & 1;
    uint16_t even_address = address & 0xFFFE;

    do {
        I2CReg* reg = i2c_register_get(even_address);
        if(reg && (reg->flags & I2CRegFlagRead)) {
            if(is_hi_byte) {
                *value = REG16_GET_HI(reg->value);
            } else {
                *value = REG16_GET_LO(reg->value);
            }
            result = true;
        }
    } while(false);

    return result;
}

bool i2c_register_read_commit(uint16_t address) {
    bool result = false;
    bool is_hi_byte = address & 1;
    uint16_t even_address = address & 0xFFFE;

    do {
        I2CReg* reg = i2c_register_get(even_address);
        if(reg && (reg->flags & I2CRegFlagReadToClear)) {
            if(is_hi_byte) {
                REG16_CLR_HI(reg->value);
            } else {
                REG16_CLR_LO(reg->value);
            }

            if((reg->flags & I2CRegFlagInterrupt) && reg->value == 0) {
                I2CInterruptInfo* interrupt_info_ptr = i2c_interrupt_info_get(even_address);
                furi_check(interrupt_info_ptr);
                i2c.status_register->value &= ~(1 << interrupt_info_ptr->status_register_bit);
                if(i2c.status_register->value == 0) {
                    i2c_register_interrupt_line_set(false);
                }
            }

            result = true;
        }
    } while(false);

    return result;
}

bool i2c_register_write(uint16_t address, uint8_t value) {
    bool result = false;
    bool is_hi_byte = address & 1;
    uint16_t even_address = address & 0xFFFE;

    do {
        I2CReg* reg = i2c_register_get(even_address);
        if(reg && (reg->flags & I2CRegFlagWrite)) {
            if(is_hi_byte) {
                REG16_SET_HI(reg->value, value);

                // we assume that if hi byte is written, then the whole register is being written
                I2CRegisterCallbackWithContext* callback_with_context = i2c_interrupt_callback_get(even_address);
                if(callback_with_context && callback_with_context->callback) {
                    callback_with_context->callback(callback_with_context->context, even_address, reg->value);
                }
            } else {
                REG16_SET_LO(reg->value, value);
            }
            result = true;
        }
    } while(false);

    return result;
}

bool i2c_register_update(uint16_t address, uint16_t value, uint16_t mask) {
    I2CReg* reg = i2c_register_get(address);
    furi_check(reg); // address must exist
    uint16_t old_value = reg->value;
    reg->value = (reg->value & ~mask) | (value & mask);
    return reg->value != old_value;
}

void i2c_register_set_interrupt(uint16_t interrupt_address, uint16_t interrupt_bits) {
    I2CReg* interrupt_reg = i2c_register_get(interrupt_address);
    furi_check(interrupt_reg);

    I2CInterruptInfo* interrupt_info_ptr = i2c_interrupt_info_get(interrupt_address);
    furi_check(interrupt_info_ptr);

    interrupt_reg->value |= (interrupt_bits & ~interrupt_info_ptr->mask_reg->value);

    if(interrupt_reg->value) {
        i2c.status_register->value |= (1 << interrupt_info_ptr->status_register_bit);
        i2c_register_interrupt_line_set(true);
    }
}

uint16_t i2c_register_get_value(uint16_t address) {
    I2CReg* reg = i2c_register_get(address);
    furi_check(reg); // address must exist
    return reg->value;
}
