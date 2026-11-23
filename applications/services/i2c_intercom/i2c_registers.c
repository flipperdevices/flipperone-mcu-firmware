#include <furi.h>
#include <applications.h>
#include <furi_hal_resources.h>
#include <furi_hal_interrupt.h>
#include <hardware/gpio.h>
#include <hardware/structs/nvic.h>
#include <hardware/regs/addressmap.h>

#include "i2c_intercom.h"
#include "i2c_registers.h"
#include "i2c_registers_i.h"
#include "i2c_registers_map.h"

/*
 * Two contexts touch the register file:
 *
 * - The I2C slave ISR (I2C_SLAVE_IRQ_PRIORITY, not masked by FURI_CRITICAL, so
 *   it also runs during flash erase/program). It answers reads at once, from
 *   the register table and from region read buffers, and appends every write
 *   transaction to the transaction ring. It applies nothing itself. RAM only,
 *   no FreeRTOS: see I2C_ISR_FUNC.
 * - The ring software interrupt (SPARE_IRQ_0, normal priority, masked by
 *   FURI_CRITICAL). It takes write transactions off the ring in order, applies
 *   register bytes to the table and calls register write callbacks, and hands
 *   region writes to the region owner, a whole transaction at once. It may use
 *   the ISR-safe FreeRTOS API.
 *
 * Threads change the table and read buffers under with_i2c_register, which
 * masks the I2C ISR.
 *
 * Consequence: a write is applied only once the software interrupt has run,
 * i.e. microseconds later, or after the flash operation in progress. A read
 * of a register just written may return its old value until then.
 */

typedef enum {
    I2CRegFlagRead = 1 << 0,
    I2CRegFlagWrite = 1 << 1,
    I2CRegFlagReadToClear = 1 << 2,
    I2CRegFlagInterrupt = 1 << 3,
} I2CRegFlag;

#define I2C_REGISTERS_MAX_COUNT (64)
#define I2C_REGISTERS_MAX_OWNERS (8)
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
    const volatile uint8_t* read_buffer;
    I2CRegionWriteFn write;
    void* context;
} I2CRegion;

/*
 * Transaction ring: a byte ring of records [addr_hi, addr_lo, len_lo, len_hi,
 * data...]. Single producer (the I2C ISR), single consumer (the software
 * interrupt). The producer builds a record past `head` and publishes it by
 * moving `head` only once the transaction ended, so the consumer never sees
 * part of one; a transaction that does not fit is dropped whole.
 */
#define I2C_REGISTERS_RING_SIZE   (4096) // power of two
#define I2C_REGISTERS_RING_HEADER (4)
static_assert((I2C_REGISTERS_RING_SIZE & (I2C_REGISTERS_RING_SIZE - 1)) == 0, "must be a power of two");

#define I2C_REGISTERS_RING_IRQ    SPARE_IRQ_0
#define I2C_REGISTERS_RING_IRQ_ID FuriHalInterruptIdSpareIrq0

typedef struct {
    uint8_t buffer[I2C_REGISTERS_RING_SIZE];
    volatile uint32_t head; ///< Published end, written by the producer
    volatile uint32_t tail; ///< Consumed end, written by the consumer

    // Producer only: the record being built
    uint32_t write; ///< Next byte position
    uint32_t record; ///< Position of the record header
    uint16_t length; ///< Data bytes so far
    bool open;
    bool overflow;

    volatile uint32_t dropped_transactions; ///< Producer: did not fit
} I2CRegistersRing;

typedef struct {
    I2CReg regs[I2C_REGISTERS_MAX_COUNT];
    size_t reg_count;
    I2CReg* status_register;
    I2CRegion regions[I2C_REGISTERS_MAX_REGIONS]; // checked before the registers
    size_t region_count;

    I2CRegistersRing ring;
    // Consumer only: one transaction copied out of the ring, contiguous
    uint8_t transaction[I2C_REGISTERS_TRANSACTION_MAX];
    volatile uint32_t invalid_writes; ///< Consumer: bytes to unmapped or read-only addresses

    // I2C ISR only: high byte latched by a low-byte read, served to the
    // high-byte read that follows it (see i2c_register_read_start)
    uint16_t read_latch_address;
    uint8_t read_latch_value;
    bool read_latch_valid;

    // Pin numbers copied out of the flash-resident GpioPin constants
    uint32_t cpu_int_pin;
    uint32_t m40_pin;
    // The slave answers: no more registration, the line is driven
    bool started;

    // Threads allowed to register: the intercom's, and each owner's from its
    // i2c_registers_owner_begin(), at the owner's FLIPPER_I2C_REGISTER_OWNERS index
    FuriThreadId intercom_thread;
    FuriThreadId owner_threads[I2C_REGISTERS_MAX_OWNERS];
} I2CRegisters;

static I2CRegisters i2c;

static void i2c_registers_ring_isr(void* context);

#define REG16_GET_LO(v)    ((v) & 0xFF)
#define REG16_GET_HI(v)    (((v) >> 8) & 0xFF)
#define REG16_SET_LO(v, b) ((v) = ((v) & 0xFF00) | (uint8_t)(b))
#define REG16_SET_HI(v, b) ((v) = ((v) & 0x00FF) | ((uint16_t)(uint8_t)(b) << 8))
#define REG16_CLR_LO(v)    ((v) &= 0xFF00)
#define REG16_CLR_HI(v)    ((v) &= 0x00FF)
#define RING_AT(pos)   (i2c.ring.buffer[(pos) & (I2C_REGISTERS_RING_SIZE - 1)])

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

// Only listed owners register, and only before the slave starts: an owner
// missing from FLIPPER_I2C_REGISTER_OWNERS fails here on every boot instead of
// racing the slave start.
static void i2c_registers_check_registrar(void) {
    furi_check(!i2c.started);
    FuriThreadId thread = furi_thread_get_current_id();
    bool allowed = thread == i2c.intercom_thread;
    for(size_t i = 0; i < I2C_REGISTERS_MAX_OWNERS && !allowed; i++) {
        allowed = thread == i2c.owner_threads[i];
    }
    furi_check(allowed);
}

void i2c_registers_init(void) {
    i2c_register_interrupt_line_init();
    i2c.intercom_thread = furi_thread_get_current_id();

    i2c.reg_count = 0;
    i2c.region_count = 0;
    i2c.started = false;
    i2c_register_add_readable(I2C_STATUS_REG_ADDRESS, 0);
    i2c.status_register = i2c_register_get(I2C_STATUS_REG_ADDRESS);
    furi_check(i2c.status_register);
    i2c_register_add_readable(I2C_INTERCOM_VERSION_REG_ADDRESS, I2C_INTERCOM_VERSION);

    // Below the syscall level: register and region callbacks may use the ISR-safe OS API
    furi_hal_interrupt_set_isr_ex(I2C_REGISTERS_RING_IRQ_ID, FuriHalInterruptPriorityNormal, i2c_registers_ring_isr, NULL);
}

void i2c_registers_owner_begin(const char* owner_record) {
    furi_check(owner_record);
    furi_check(FLIPPER_I2C_REGISTER_OWNERS_COUNT <= I2C_REGISTERS_MAX_OWNERS);
    size_t index = 0;
    while(index < FLIPPER_I2C_REGISTER_OWNERS_COUNT && strcmp(FLIPPER_I2C_REGISTER_OWNERS[index], owner_record) != 0) {
        index++;
    }
    furi_check(index < FLIPPER_I2C_REGISTER_OWNERS_COUNT); // list it in applications.c

    // The map is initialised once the intercom record exists
    furi_record_open(RECORD_I2C_INTERCOM);
    furi_record_close(RECORD_I2C_INTERCOM);

    with_i2c_register({
        furi_check(!i2c.started);
        furi_check(i2c.owner_threads[index] == NULL);
        i2c.owner_threads[index] = furi_thread_get_current_id();
    });
}

static void i2c_register_add_internal(uint16_t address, uint16_t default_value, uint32_t flags) {
    i2c_registers_check_registrar();
    furi_check(address % 2 == 0); // registers are 16-bit, even-aligned
    furi_check(i2c_register_get(address) == NULL);
    // Regions win the lookup, so a register inside one would never be reached
    furi_check(i2c_region_find(address) == NULL);
    furi_check(i2c_region_find(address + 1) == NULL);
    furi_check(i2c.reg_count < I2C_REGISTERS_MAX_COUNT);

    I2CReg reg = {.address = address, .value = default_value, .flags = flags, .mask_index = I2C_REG_NO_MASK};
    i2c.regs[i2c.reg_count++] = reg;
}

void i2c_register_add_region(uint16_t base, uint16_t length, const volatile uint8_t* read_buffer, I2CRegionWriteFn write, void* context) {
    i2c_registers_check_registrar();
    furi_check(length > 0);
    furi_check(base % 2 == 0);
    furi_check((uint32_t)base + length <= 0x10000);
    furi_check(i2c.region_count < I2C_REGISTERS_MAX_REGIONS);
    // The I2C ISR reads it while flash may be busy: it must not be in flash
    furi_check(!read_buffer || ((uintptr_t)read_buffer >= SRAM_BASE && (uintptr_t)read_buffer + length <= SRAM_END));

    // Validation is O(region length), too long to hold interrupts off, and
    // registration is startup-only, before the I2C slave is enabled.
    // The critical section below just guards the array append.

    // Must not overlap another region...
    for(size_t i = 0; i < i2c.region_count; ++i) {
        const I2CRegion* other = &i2c.regions[i];
        const bool disjoint = (uint32_t)base + length <= other->base || base >= (uint32_t)other->base + other->length;
        furi_check(disjoint);
    }
    // ...nor shadow an already registered register (probe via the same
    // lookup used elsewhere; step 2 hits only valid even addresses).
    for(uint32_t address = base; address < (uint32_t)base + length; address += 2) {
        furi_check(i2c_register_get((uint16_t)address) == NULL);
    }

    I2CRegion region = {
        .base = base,
        .length = length,
        .read_buffer = read_buffer,
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
        I2CReg* reg = i2c_register_get(address);
        reg->callback = write_callback;
        reg->context = write_callback_context;
    });
}

void i2c_register_add_interrupt(uint16_t address, uint16_t mask_address, uint8_t status_register_bit) {
    with_i2c_register({
        i2c_register_add_internal(address, 0x0000, I2CRegFlagRead | I2CRegFlagReadToClear | I2CRegFlagInterrupt);
        i2c_register_add_internal(mask_address, 0x0000, I2CRegFlagRead | I2CRegFlagWrite);
        I2CReg* reg = i2c_register_get(address);
        reg->mask_index = i2c.reg_count - 1;
        reg->status_register_bit = status_register_bit;
    });
}

I2C_ISR_FUNC(bool, i2c_register_read_start)(uint16_t address, uint8_t* value) {
    const I2CRegion* region = i2c_region_find(address);
    if(region) {
        if(!region->read_buffer) return false;
        *value = region->read_buffer[address - region->base];
        return true;
    }

    bool result = false;
    bool is_hi_byte = address & 1;
    uint16_t even_address = address & 0xFFFE;

    do {
        I2CReg* reg = i2c_register_get(even_address);
        if(reg && (reg->flags & I2CRegFlagRead)) {
            // A write can be applied between the two RD_REQs of a 16-bit
            // read. The low-byte read latches the high byte, so the host gets
            // both bytes of one value. Read-to-clear registers are independent
            // flag bits and clear what is read: they are not latched.
            if(is_hi_byte) {
                if(i2c.read_latch_valid && i2c.read_latch_address == address) {
                    *value = i2c.read_latch_value;
                } else {
                    *value = REG16_GET_HI(reg->value);
                }
            } else {
                *value = REG16_GET_LO(reg->value);
                i2c.read_latch_valid = !(reg->flags & I2CRegFlagReadToClear);
                i2c.read_latch_address = address + 1;
                i2c.read_latch_value = REG16_GET_HI(reg->value);
            }
            result = true;
        }
    } while(false);

    return result;
}

I2C_ISR_FUNC(void, i2c_register_read_reset)(void) {
    i2c.read_latch_valid = false;
}

I2C_ISR_FUNC(bool, i2c_register_read_commit)(uint16_t address) {
    // Regions have no read-to-clear semantics; keep per-byte cost to one lookup.
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
                i2c.status_register->value &= ~(1 << reg->status_register_bit);
                if(i2c.status_register->value == 0) {
                    i2c_register_interrupt_line_set(false);
                }
            }

            result = true;
        }
    } while(false);

    return result;
}

/* --- Producer: the I2C ISR ----------------------------------------------- */

I2C_ISR_FUNC(void, i2c_registers_write_begin)(uint16_t address) {
    I2CRegistersRing* ring = &i2c.ring;
    if(ring->open) i2c_registers_write_commit();

    ring->open = true;
    ring->overflow = false;
    ring->length = 0;
    ring->record = ring->head;
    ring->write = ring->head;

    if(I2C_REGISTERS_RING_SIZE - (ring->write - ring->tail) < I2C_REGISTERS_RING_HEADER) {
        ring->overflow = true;
        return;
    }
    RING_AT(ring->write) = address >> 8;
    RING_AT(ring->write + 1) = address & 0xFF;
    ring->write += I2C_REGISTERS_RING_HEADER; // length is filled in on commit
}

I2C_ISR_FUNC(void, i2c_registers_write_byte)(uint8_t value) {
    I2CRegistersRing* ring = &i2c.ring;
    if(!ring->open || ring->overflow) return;
    if(ring->length >= I2C_REGISTERS_TRANSACTION_MAX || ring->write - ring->tail >= I2C_REGISTERS_RING_SIZE) {
        ring->overflow = true;
        return;
    }
    RING_AT(ring->write) = value;
    ring->write++;
    ring->length++;
}

I2C_ISR_FUNC(void, i2c_registers_write_commit)(void) {
    I2CRegistersRing* ring = &i2c.ring;
    if(!ring->open) return;
    ring->open = false;

    if(ring->overflow) {
        ring->dropped_transactions = ring->dropped_transactions + 1;
        return;
    }
    if(ring->length == 0) return; // address only: nothing to apply

    RING_AT(ring->record + 2) = ring->length & 0xFF;
    RING_AT(ring->record + 3) = ring->length >> 8;
    __atomic_thread_fence(__ATOMIC_RELEASE);
    ring->head = ring->write;
    // Pend the software interrupt; it runs once BASEPRI lets it through
    nvic_hw->ispr[I2C_REGISTERS_RING_IRQ / 32] = 1u << (I2C_REGISTERS_RING_IRQ % 32);
}

/* --- Consumer: the ring software interrupt -------------------------------- */

// Apply the bytes of one register (one or both, from `address`) to the
// register table under one lock, so the I2C ISR never reads it half written.
// Call the register callback, outside the lock, once its high byte is
// written. FreeRTOS FromISR calls rewrite BASEPRI, so no callback may run
// inside with_i2c_register.
static void i2c_registers_apply(uint16_t address, const uint8_t* data, size_t length) {
    I2CRegisterCallback callback = NULL;
    void* context = NULL;
    uint16_t reg_address = address & 0xFFFE;
    uint16_t reg_value = 0;
    bool valid = false;

    with_i2c_register({
        I2CReg* reg = i2c_register_get(reg_address);
        if(reg && (reg->flags & I2CRegFlagWrite)) {
            valid = true;
            for(size_t i = 0; i < length; i++) {
                if((address + i) & 1) {
                    REG16_SET_HI(reg->value, data[i]);
                    // hi byte written means the whole register value is ready
                    callback = reg->callback;
                    context = reg->context;
                    reg_value = reg->value;
                } else {
                    REG16_SET_LO(reg->value, data[i]);
                }
            }
        }
    });

    if(!valid) {
        i2c.invalid_writes = i2c.invalid_writes + length;
    } else if(callback) {
        callback(context, reg_address, reg_value);
    }
}

// Dispatch one write transaction: runs inside a region go to the region owner
// in one call, other bytes to the register table.
static void i2c_registers_dispatch(uint16_t address, const uint8_t* data, size_t length) {
    size_t i = 0;
    while(i < length) {
        uint16_t at = address + i;
        const I2CRegion* region = i2c_region_find(at);
        if(region) {
            size_t run = MIN(length - i, (size_t)(region->base + region->length - at));
            if(!region->write || !region->write(region->context, at - region->base, &data[i], run)) {
                i2c.invalid_writes = i2c.invalid_writes + run;
            }
            i += run;
        } else {
            // A register at a time: its low byte and the high byte after it
            size_t run = ((at & 1) || i + 1 == length) ? 1 : 2;
            i2c_registers_apply(at, &data[i], run);
            i += run;
        }
    }
}

static void i2c_registers_ring_isr(void* context) {
    UNUSED(context);
    I2CRegistersRing* ring = &i2c.ring;
    while(ring->tail != ring->head) {
        __atomic_thread_fence(__ATOMIC_ACQUIRE);
        uint32_t pos = ring->tail;
        uint16_t address = ((uint16_t)RING_AT(pos) << 8) | RING_AT(pos + 1);
        uint16_t length = RING_AT(pos + 2) | ((uint16_t)RING_AT(pos + 3) << 8);
        pos += I2C_REGISTERS_RING_HEADER;
        for(uint16_t i = 0; i < length; i++) {
            i2c.transaction[i] = RING_AT(pos + i);
        }
        __atomic_thread_fence(__ATOMIC_RELEASE);
        ring->tail = pos + length; // free the space before the callbacks run

        i2c_registers_dispatch(address, i2c.transaction, length);
    }
}

uint32_t i2c_registers_dropped_transactions_get(void) {
    return i2c.ring.dropped_transactions;
}

uint32_t i2c_registers_invalid_writes_get(void) {
    return i2c.invalid_writes;
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
        i2c.status_register->value |= (1 << interrupt_reg->status_register_bit);
        if(i2c.started) i2c_register_interrupt_line_set(true);
    }
}

void i2c_registers_start(void) {
    with_i2c_register({
        i2c.started = true;
        i2c_register_interrupt_line_set(i2c.status_register->value != 0);
    });
}

uint16_t i2c_register_get_value(uint16_t address) {
    I2CReg* reg = i2c_register_get(address);
    furi_check(reg);
    return reg->value;
}
