#include <furi.h>
#include <furi_hal_resources.h>
#include <furi_hal_interrupt.h>
#include <hardware/gpio.h>
#include <hardware/structs/nvic.h>

#include "i2c_registers.h"
#include "i2c_registers_map.h"

// Everything on the I2C ISR path below runs at I2C_SLAVE_IRQ_PRIORITY, which
// FURI_CRITICAL does not mask: it may run while flash is being erased or
// programmed. So it lives in RAM, reads only RAM data and calls no FreeRTOS
// API. Write callbacks are handed to a lower-priority software interrupt.

typedef enum {
    I2CRegFlagRead = 1 << 0,
    I2CRegFlagWrite = 1 << 1,
    I2CRegFlagReadToClear = 1 << 2,
    I2CRegFlagInterrupt = 1 << 3,
} I2CRegFlag;

#define I2C_REGISTERS_MAX_COUNT (64)
#define I2C_REG_NO_MASK         (0xFF)

typedef struct {
    uint16_t address;
    uint16_t value;
    uint8_t flags;
    uint8_t status_register_bit; ///< Interrupt registers: status register bit they drive
    uint8_t mask_index; ///< Interrupt registers: index of their mask register
    I2CRegisterCallback callback; ///< Writable registers: called once the high byte is written
    void* context;
} I2CReg;

// Byte-addressed window backed by an external owner (see i2c_register_add_region).
typedef struct {
    uint16_t base;
    uint16_t length;
    I2CRegionReadFn read;
    I2CRegionWriteFn write;
    void* context;
} I2CRegion;

// Callback deferred from the I2C ISR to the software interrupt.
typedef struct {
    I2CRegisterCallback callback;
    void* context;
    uint16_t address;
    uint16_t value;
} I2CDeferredCall;

#define I2C_REGISTERS_DEFERRED_SIZE (32) // power of two
static_assert((I2C_REGISTERS_DEFERRED_SIZE & (I2C_REGISTERS_DEFERRED_SIZE - 1)) == 0, "must be a power of two");

#define I2C_REGISTERS_DEFER_IRQ    SPARE_IRQ_0
#define I2C_REGISTERS_DEFER_IRQ_ID FuriHalInterruptIdSpareIrq0

typedef struct {
    I2CReg regs[I2C_REGISTERS_MAX_COUNT]; // regs[0] is the status register
    size_t reg_count;
    I2CRegion regions[I2C_REGISTERS_MAX_REGIONS]; // checked before the registers
    size_t region_count;

    // Single producer (the I2C ISR), single consumer (the software interrupt)
    I2CDeferredCall deferred[I2C_REGISTERS_DEFERRED_SIZE];
    volatile uint32_t deferred_head;
    volatile uint32_t deferred_tail;
    volatile uint32_t deferred_overflow;

    // Pin numbers copied out of the flash-resident GpioPin constants
    uint32_t cpu_int_pin;
    uint32_t m40_pin;
} I2CRegisters;

static I2CRegisters i2c;

#define I2C_STATUS_REG (&i2c.regs[0])

I2C_ISR_FUNC(static inline void, i2c_register_interrupt_line_set)(bool set) {
    // Same as furi_hal_gpio_write_open_drain(&gpio_cpu_int, !set) and
    // furi_hal_gpio_write(&gpio_m40, set), without the flash-resident GpioPin
    gpio_set_dir(i2c.cpu_int_pin, !set);
    gpio_put(i2c.m40_pin, set);
}

static void i2c_register_interrupt_line_init(void) {
    i2c.cpu_int_pin = gpio_cpu_int.pin;
    i2c.m40_pin = gpio_m40.pin;
    furi_hal_gpio_init(&gpio_cpu_int, GpioModeOutputOpenDrain, GpioPullNo, GpioSpeedLow);
    furi_hal_gpio_init(&gpio_m40, GpioModeOutputPushPull, GpioPullNo, GpioSpeedLow);
    i2c_register_interrupt_line_set(false);
}

I2C_ISR_FUNC(static inline I2CReg*, i2c_register_get)(uint16_t address) {
    for(size_t i = 0; i < i2c.reg_count; ++i) {
        if(i2c.regs[i].address == address) return &i2c.regs[i];
    }
    return NULL;
}

I2C_ISR_FUNC(static inline I2CRegion*, i2c_region_find)(uint16_t address) {
    for(size_t i = 0; i < i2c.region_count; ++i) {
        I2CRegion* region = &i2c.regions[i];
        if(address >= region->base && (uint32_t)address < (uint32_t)region->base + region->length) {
            return region;
        }
    }
    return NULL;
}

static void i2c_registers_deferred_isr(void* context) {
    UNUSED(context);
    while(i2c.deferred_tail != i2c.deferred_head) {
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        I2CDeferredCall call = i2c.deferred[i2c.deferred_tail % I2C_REGISTERS_DEFERRED_SIZE];
        __atomic_thread_fence(__ATOMIC_RELEASE);
        i2c.deferred_tail = i2c.deferred_tail + 1;
        call.callback(call.context, call.address, call.value);
    }
}

I2C_ISR_FUNC(bool, i2c_register_defer)(I2CRegisterCallback callback, void* context, uint16_t address, uint16_t value) {
    uint32_t head = i2c.deferred_head;
    if(head - i2c.deferred_tail >= I2C_REGISTERS_DEFERRED_SIZE) {
        i2c.deferred_overflow = i2c.deferred_overflow + 1;
        return false;
    }
    i2c.deferred[head % I2C_REGISTERS_DEFERRED_SIZE] = (I2CDeferredCall){
        .callback = callback,
        .context = context,
        .address = address,
        .value = value,
    };
    __atomic_thread_fence(__ATOMIC_RELEASE);
    i2c.deferred_head = head + 1;
    // Pend the software interrupt; it runs once BASEPRI lets it through
    nvic_hw->ispr[I2C_REGISTERS_DEFER_IRQ / 32] = 1u << (I2C_REGISTERS_DEFER_IRQ % 32);
    return true;
}

uint32_t i2c_registers_deferred_overflow_get(void) {
    return i2c.deferred_overflow;
}

static void i2c_register_add_internal(uint16_t address, uint16_t default_value, uint8_t flags) {
    furi_check(address % 2 == 0); // registers are 16-bit, even-aligned
    furi_check(i2c_register_get(address) == NULL);
    // Regions win the lookup, so a register inside one would never be reached
    furi_check(i2c_region_find(address) == NULL);
    furi_check(i2c_region_find(address + 1) == NULL);
    furi_check(i2c.reg_count < I2C_REGISTERS_MAX_COUNT);

    i2c.regs[i2c.reg_count] = (I2CReg){
        .address = address,
        .value = default_value,
        .flags = flags,
        .mask_index = I2C_REG_NO_MASK,
    };
    i2c.reg_count++;
}

void i2c_registers_init(void) {
    i2c_register_interrupt_line_init();

    i2c.reg_count = 0;
    i2c.region_count = 0;
    i2c.deferred_head = 0;
    i2c.deferred_tail = 0;
    i2c.deferred_overflow = 0;

    with_i2c_register({ i2c_register_add_internal(I2C_STATUS_REG_ADDRESS, 0, I2CRegFlagRead); });
    furi_check(I2C_STATUS_REG->address == I2C_STATUS_REG_ADDRESS);

    // Below the syscall level: deferred callbacks may use the ISR-safe OS API
    furi_hal_interrupt_set_isr_ex(I2C_REGISTERS_DEFER_IRQ_ID, FuriHalInterruptPriorityNormal, i2c_registers_deferred_isr, NULL);
}

void i2c_register_add_region(uint16_t base, uint16_t length, I2CRegionReadFn read, I2CRegionWriteFn write, void* context) {
    furi_check(length > 0);
    furi_check(base % 2 == 0);
    furi_check((uint32_t)base + length <= 0x10000);
    furi_check(i2c.region_count < I2C_REGISTERS_MAX_REGIONS);

    // Registration is startup-only, before the I2C slave is enabled.

    // Must not overlap another region...
    for(size_t i = 0; i < i2c.region_count; ++i) {
        const I2CRegion* other = &i2c.regions[i];
        const bool disjoint = (uint32_t)base + length <= other->base || base >= (uint32_t)other->base + other->length;
        furi_check(disjoint);
    }
    // ...nor shadow an already registered register
    for(size_t i = 0; i < i2c.reg_count; ++i) {
        const uint16_t address = i2c.regs[i].address;
        furi_check(address + 2 <= base || address >= (uint32_t)base + length);
    }

    I2CRegion region = {
        .base = base,
        .length = length,
        .read = read,
        .write = write,
        .context = context,
    };

    with_i2c_register({
        i2c.regions[i2c.region_count] = region;
        i2c.region_count++;
    });
}

void i2c_register_add_readable(uint16_t address, uint16_t default_value) {
    with_i2c_register({ i2c_register_add_internal(address, default_value, I2CRegFlagRead); });
}

void i2c_register_add_writable(uint16_t address, uint16_t default_value, I2CRegisterCallback write_callback, void* write_callback_context) {
    with_i2c_register({
        i2c_register_add_internal(address, default_value, I2CRegFlagRead | I2CRegFlagWrite);
        I2CReg* reg = &i2c.regs[i2c.reg_count - 1];
        reg->callback = write_callback;
        reg->context = write_callback_context;
    });
}

void i2c_register_add_interrupt(uint16_t address, uint16_t mask_address, uint8_t status_register_bit) {
    furi_check(status_register_bit < 16);
    with_i2c_register({
        i2c_register_add_internal(mask_address, 0x0000, I2CRegFlagRead | I2CRegFlagWrite);
        uint8_t mask_index = i2c.reg_count - 1;
        i2c_register_add_internal(address, 0x0000, I2CRegFlagRead | I2CRegFlagReadToClear | I2CRegFlagInterrupt);
        I2CReg* reg = &i2c.regs[i2c.reg_count - 1];
        reg->mask_index = mask_index;
        reg->status_register_bit = status_register_bit;
    });
}

I2C_ISR_FUNC(bool, i2c_register_read_start)(uint16_t address, uint8_t* value) {
    const I2CRegion* region = i2c_region_find(address);
    if(region) {
        return region->read ? region->read(region->context, address - region->base, value) : false;
    }

    I2CReg* reg = i2c_register_get(address & 0xFFFE);
    if(!reg || !(reg->flags & I2CRegFlagRead)) return false;

    *value = (address & 1) ? (uint8_t)(reg->value >> 8) : (uint8_t)reg->value;
    return true;
}

I2C_ISR_FUNC(bool, i2c_register_read_commit)(uint16_t address) {
    // Regions have no read-to-clear semantics; keep per-byte cost to one lookup.
    if(i2c_region_find(address)) return false;

    I2CReg* reg = i2c_register_get(address & 0xFFFE);
    if(!reg || !(reg->flags & I2CRegFlagReadToClear)) return false;

    reg->value &= (address & 1) ? 0x00FF : 0xFF00;

    if((reg->flags & I2CRegFlagInterrupt) && reg->value == 0) {
        I2C_STATUS_REG->value &= ~(1u << reg->status_register_bit);
        if(I2C_STATUS_REG->value == 0) {
            i2c_register_interrupt_line_set(false);
        }
    }

    return true;
}

I2C_ISR_FUNC(bool, i2c_register_write)(uint16_t address, uint8_t value) {
    const I2CRegion* region = i2c_region_find(address);
    if(region) {
        return region->write ? region->write(region->context, address - region->base, value) : false;
    }

    I2CReg* reg = i2c_register_get(address & 0xFFFE);
    if(!reg || !(reg->flags & I2CRegFlagWrite)) return false;

    if(address & 1) {
        reg->value = (reg->value & 0x00FF) | ((uint16_t)value << 8);
        // hi byte written means the whole register value is ready
        if(reg->callback) {
            i2c_register_defer(reg->callback, reg->context, reg->address, reg->value);
        }
    } else {
        reg->value = (reg->value & 0xFF00) | value;
    }
    return true;
}

bool __isr __not_in_flash_func(i2c_register_update)(uint16_t address, uint16_t value, uint16_t mask) {
    I2CReg* reg = i2c_register_get(address);
    furi_check(reg);
    uint16_t old_value = reg->value;
    reg->value = (reg->value & ~mask) | (value & mask);
    return reg->value != old_value;
}

void i2c_register_set_interrupt(uint16_t interrupt_address, uint16_t interrupt_bits) {
    I2CReg* interrupt_reg = i2c_register_get(interrupt_address);
    furi_check(interrupt_reg);
    furi_check(interrupt_reg->flags & I2CRegFlagInterrupt);

    interrupt_reg->value |= (interrupt_bits & ~i2c.regs[interrupt_reg->mask_index].value);

    if(interrupt_reg->value) {
        I2C_STATUS_REG->value |= (1u << interrupt_reg->status_register_bit);
        i2c_register_interrupt_line_set(true);
    }
}

uint16_t i2c_register_get_value(uint16_t address) {
    I2CReg* reg = i2c_register_get(address);
    furi_check(reg);
    return reg->value;
}
