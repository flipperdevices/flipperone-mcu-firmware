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

// A byte-addressed window backed by an external owner, see
// i2c_register_add_region. Kept in a plain array rather than a dict: it is
// looked up on every single byte from the I2C slave ISR, so the lookup must
// be allocation-free and cheaper than hashing.
typedef struct {
    uint16_t base;
    uint16_t length;
    I2CRegionReadFn read;
    I2CRegionWriteFn write;
    void* context;
} I2CRegion;

typedef struct {
    // hashmap of registers, key is register address, value is register struct
    I2CRegMap_t map;
    // hashmap of status registers to their corresponding info
    I2CInterruptInfoMap_t interrupt_info_map;
    // hashmap of interrupt registers to their corresponding callbacks
    I2CInterruptCallbackMap_t interrupt_callback_map;
    // status register pointer
    I2CReg* status_register;
    // byte-addressed regions, matched before the register map
    I2CRegion regions[I2C_REGISTERS_MAX_REGIONS];
    size_t region_count;
} I2CRegisters;

static I2CRegisters i2c;

static inline void i2c_register_interrupt_line_set(bool set) {
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

static inline I2CReg* i2c_register_get(uint16_t address) {
    return I2CRegMap_get(i2c.map, address);
}

static void i2c_register_set_at(uint16_t address, I2CReg reg) {
    I2CRegMap_set_at(i2c.map, address, reg);
}

static inline I2CInterruptInfo* i2c_interrupt_info_get(uint16_t address) {
    return I2CInterruptInfoMap_get(i2c.interrupt_info_map, address);
}

static void i2c_interrupt_info_set_at(uint16_t address, I2CInterruptInfo info) {
    I2CInterruptInfoMap_set_at(i2c.interrupt_info_map, address, info);
}

static I2CRegion* i2c_region_find(uint16_t address) {
    for(size_t i = 0; i < i2c.region_count; ++i) {
        I2CRegion* region = &i2c.regions[i];
        if(address >= region->base && (uint32_t)address < (uint32_t)region->base + region->length) {
            return region;
        }
    }
    return NULL;
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
    i2c.region_count = 0;
    i2c_register_add_readable(I2C_STATUS_REG_ADDRESS, 0);
    i2c.status_register = i2c_register_get(I2C_STATUS_REG_ADDRESS);
    furi_check(i2c.status_register);
}

static void i2c_register_add_internal(uint16_t address, uint16_t default_value, uint32_t flags) {
    furi_check(address % 2 == 0); // only even addresses are valid
    furi_check(i2c_register_get(address) == NULL); // address must not exist
    // Regions win the lookup, so a register inside one would never be reached
    furi_check(i2c_region_find(address) == NULL);
    furi_check(i2c_region_find(address + 1) == NULL);

    I2CReg reg = {.value = default_value, .flags = flags};
    i2c_register_set_at(address, reg);
}

void i2c_register_add_region(uint16_t base, uint16_t length, I2CRegionReadFn read, I2CRegionWriteFn write, void* context) {
    furi_check(length > 0);
    furi_check(base % 2 == 0); // only even base addresses are valid
    furi_check((uint32_t)base + length <= 0x10000); // must not wrap the address space
    furi_check(i2c.region_count < I2C_REGISTERS_MAX_REGIONS);

    // Validation runs outside the critical section: it walks the whole
    // region and would keep interrupts off for too long. Registration only
    // ever happens at startup, single-threaded, before the I2C slave is
    // enabled — the critical section below just guards the array append.

    // Must not overlap another region...
    for(size_t i = 0; i < i2c.region_count; ++i) {
        const I2CRegion* other = &i2c.regions[i];
        const bool disjoint = (uint32_t)base + length <= other->base || base >= (uint32_t)other->base + other->length;
        furi_check(disjoint);
    }
    // ...nor shadow an already registered register. Probing the map beats
    // iterating it: this relies only on the lookup used everywhere else in
    // this file. Base is even and the step is 2, so every probed address is
    // the even address a register would be keyed by.
    for(uint32_t address = base; address < (uint32_t)base + length; address += 2) {
        furi_check(i2c_register_get((uint16_t)address) == NULL);
    }

    I2CRegion region = {
        .base = base,
        .length = length,
        .read = read,
        .write = write,
        .context = context,
    };

    FURI_CRITICAL_ENTER();
    i2c.regions[i2c.region_count] = region;
    i2c.region_count++;
    FURI_CRITICAL_EXIT();
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

bool __isr __not_in_flash_func(i2c_register_read_start)(uint16_t address, uint8_t* value) {
    const I2CRegion* region = i2c_region_find(address);
    if(region) {
        return region->read ? region->read(region->context, address - region->base, value) : false;
    }

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

bool __isr __not_in_flash_func(i2c_register_read_commit)(uint16_t address) {
    // Regions have no read-to-clear semantics — nothing to commit. Bailing
    // out here keeps the per-byte cost of a region read down to one lookup.
    if(i2c_region_find(address)) return false;

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

bool __isr __not_in_flash_func(i2c_register_write)(uint16_t address, uint8_t value) {
    const I2CRegion* region = i2c_region_find(address);
    if(region) {
        return region->write ? region->write(region->context, address - region->base, value) : false;
    }

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

bool __isr __not_in_flash_func(i2c_register_update)(uint16_t address, uint16_t value, uint16_t mask) {
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
