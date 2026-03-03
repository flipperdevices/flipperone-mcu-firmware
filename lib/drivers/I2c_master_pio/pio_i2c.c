#include "pio_i2c.h"

struct I2cMasterPio {
    const GpioPin* sda_pin;
    const GpioPin* scl_pin;
    PIO pio;
    uint sm;
    uint offset;
    bool previous_nostop;
};

const int PIO_I2C_ICOUNT_LSB = 10;
const int PIO_I2C_FINAL_LSB = 9;
const int PIO_I2C_DATA_LSB = 1;
const int PIO_I2C_NAK_LSB = 0;

void pio_i2c_gpio_init(I2cMasterPio* instance) {
    furi_check(instance);
    i2c_program_gpio_init(instance->pio, instance->sm, instance->sda_pin->pin, instance->scl_pin->pin);
}

void pio_i2c_gpio_deinit(I2cMasterPio* instance) {
    furi_check(instance);
    i2c_program_gpio_deinit(instance->pio, instance->sm, instance->sda_pin->pin, instance->scl_pin->pin);
}

I2cMasterPio* pio_i2c_init(const GpioPin* sda_pin, const GpioPin* scl_pin, uint32_t speed) {
    I2cMasterPio* instance = malloc(sizeof(I2cMasterPio));

    instance->sda_pin = sda_pin;
    instance->scl_pin = scl_pin;
    instance->previous_nostop = false;
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(&i2c_program, &instance->pio, &instance->sm, &instance->offset, sda_pin->pin, 2, true);
    furi_check(success);
    i2c_program_init(instance->pio, instance->sm, instance->offset, sda_pin->pin, scl_pin->pin, speed);

    return instance;
}

void pio_i2c_deinit(I2cMasterPio* instance) {
    furi_check(instance);
    pio_i2c_gpio_deinit(instance);
    i2c_program_deinit(instance->pio, instance->sm, instance->sda_pin->pin, instance->scl_pin->pin);
    free(instance);
}

bool pio_i2c_check_error(I2cMasterPio* instance) {
    furi_check(instance);
    return pio_interrupt_get(instance->pio, instance->sm);
}

void pio_i2c_resume_after_error(I2cMasterPio* instance) {
    furi_check(instance);
    pio_sm_drain_tx_fifo(instance->pio, instance->sm);
    pio_sm_exec(
        instance->pio, instance->sm, (instance->pio->sm[instance->sm].execctrl & PIO_SM0_EXECCTRL_WRAP_BOTTOM_BITS) >> PIO_SM0_EXECCTRL_WRAP_BOTTOM_LSB);
    pio_interrupt_clear(instance->pio, instance->sm);
}

void pio_i2c_rx_enable(I2cMasterPio* instance, bool en) {
    furi_check(instance);
    if(en)
        hw_set_bits(&instance->pio->sm[instance->sm].shiftctrl, PIO_SM0_SHIFTCTRL_AUTOPUSH_BITS);
    else
        hw_clear_bits(&instance->pio->sm[instance->sm].shiftctrl, PIO_SM0_SHIFTCTRL_AUTOPUSH_BITS);
}

static inline void pio_i2c_put16(I2cMasterPio* instance, uint16_t data) {
    furi_check(instance);
    while(pio_sm_is_tx_fifo_full(instance->pio, instance->sm))
        ;
    // some versions of GCC dislike this
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif
    *(io_rw_16*)&instance->pio->txf[instance->sm] = data;
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
}

// If I2C is ok, block and push data. Otherwise fall straight through.
void pio_i2c_put_or_err(I2cMasterPio* instance, uint16_t data) {
    furi_check(instance);
    while(pio_sm_is_tx_fifo_full(instance->pio, instance->sm))
        if(pio_i2c_check_error(instance)) return;
    if(pio_i2c_check_error(instance)) return;
    // some versions of GCC dislike this
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wstrict-aliasing"
#endif
    *(io_rw_16*)&instance->pio->txf[instance->sm] = data;
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
}

uint8_t pio_i2c_get(I2cMasterPio* instance) {
    furi_check(instance);
    return (uint8_t)pio_sm_get(instance->pio, instance->sm);
}

void pio_i2c_start(I2cMasterPio* instance) {
    furi_check(instance);
    pio_i2c_put_or_err(instance, 2u << PIO_I2C_ICOUNT_LSB); // Escape code for 3 instruction sequence
    pio_i2c_put_or_err(instance, set_scl_sda_program_instructions[I2C_SC1_SD0]); // We are already in idle state, just pull SDA low
    pio_i2c_put_or_err(instance, set_scl_sda_program_instructions[I2C_SC0_SD0]); // Also pull clock low so we can present data
    pio_i2c_put_or_err(instance, pio_encode_mov(pio_isr, pio_null)); // Ensure ISR counter is clear following a write
}

void pio_i2c_stop(I2cMasterPio* instance) {
    furi_check(instance);
    pio_i2c_put_or_err(instance, 2u << PIO_I2C_ICOUNT_LSB);
    pio_i2c_put_or_err(instance, set_scl_sda_program_instructions[I2C_SC0_SD0]); // SDA is unknown; pull it down
    pio_i2c_put_or_err(instance, set_scl_sda_program_instructions[I2C_SC1_SD0]); // Release clock
    pio_i2c_put_or_err(instance, set_scl_sda_program_instructions[I2C_SC1_SD1]); // Release SDA to return to idle state
}

void pio_i2c_repstart(I2cMasterPio* instance) {
    furi_check(instance);
    pio_i2c_put_or_err(instance, 4u << PIO_I2C_ICOUNT_LSB);
    pio_i2c_put_or_err(instance, set_scl_sda_program_instructions[I2C_SC0_SD1]);
    pio_i2c_put_or_err(instance, set_scl_sda_program_instructions[I2C_SC1_SD1]);
    pio_i2c_put_or_err(instance, set_scl_sda_program_instructions[I2C_SC1_SD0]);
    pio_i2c_put_or_err(instance, set_scl_sda_program_instructions[I2C_SC0_SD0]);
    pio_i2c_put_or_err(instance, pio_encode_mov(pio_isr, pio_null));
}

static void pio_i2c_wait_idle(I2cMasterPio* instance) {
    furi_check(instance);
    // Finished when TX runs dry or SM hits an IRQ
    instance->pio->fdebug = 1u << (PIO_FDEBUG_TXSTALL_LSB + instance->sm);
    while(!(instance->pio->fdebug & 1u << (PIO_FDEBUG_TXSTALL_LSB + instance->sm) || pio_i2c_check_error(instance)))
        tight_loop_contents();
}

int pio_i2c_write_blocking(I2cMasterPio* instance, uint8_t addr, uint8_t* txbuf, uint len, bool nostop, uint32_t timeout_ms) {
    furi_check(instance);

    //Todo: implement timeout
    UNUSED(timeout_ms);
    int err = 0;

    if(instance->previous_nostop) {
        pio_i2c_repstart(instance);
    } else {
        pio_i2c_start(instance);
    }
    instance->previous_nostop = nostop;

    pio_i2c_rx_enable(instance, false);
    pio_i2c_put16(instance, (addr << 2) | 1u);
    while(len && !pio_i2c_check_error(instance)) {
        if(!pio_sm_is_tx_fifo_full(instance->pio, instance->sm)) {
            --len;
            pio_i2c_put_or_err(instance, (*txbuf++ << PIO_I2C_DATA_LSB) | ((len == 0) << PIO_I2C_FINAL_LSB) | 1u);
        }
    }

    if(!nostop) {
        pio_i2c_stop(instance);
    }
    pio_i2c_wait_idle(instance);

    if(pio_i2c_check_error(instance)) {
        err = PICO_ERROR_GENERIC;
        pio_i2c_resume_after_error(instance);
        pio_i2c_stop(instance);
        instance->previous_nostop = false;
    }

    return err;
}

int pio_i2c_read_blocking(I2cMasterPio* instance, uint8_t addr, uint8_t* rxbuf, uint len, bool nostop, uint32_t timeout_ms) {
    furi_check(instance);
    //Todo: implement timeout
    UNUSED(timeout_ms);

    int err = 0;

    if(instance->previous_nostop) {
        pio_i2c_repstart(instance);
    } else {
        pio_i2c_start(instance);
    }
    instance->previous_nostop = nostop;

    pio_i2c_rx_enable(instance, true);
    while(!pio_sm_is_rx_fifo_empty(instance->pio, instance->sm))
        (void)pio_i2c_get(instance);
    pio_i2c_put16(instance, (addr << 2) | 3u);
    uint32_t tx_remain = len; // Need to stuff 0xff bytes in to get clocks

    bool first = true;

    while((tx_remain || len) && !pio_i2c_check_error(instance)) {
        if(tx_remain && !pio_sm_is_tx_fifo_full(instance->pio, instance->sm)) {
            --tx_remain;
            pio_i2c_put16(instance, (0xffu << 1) | (tx_remain ? 0 : (1u << PIO_I2C_FINAL_LSB) | (1u << PIO_I2C_NAK_LSB)));
        }
        if(!pio_sm_is_rx_fifo_empty(instance->pio, instance->sm)) {
            if(first) {
                // Ignore returned address byte
                (void)pio_i2c_get(instance);
                first = false;
            } else {
                --len;
                *rxbuf++ = pio_i2c_get(instance);
            }
        }
    }

    if(!nostop) {
        pio_i2c_stop(instance);
    }

    pio_i2c_wait_idle(instance);
    if(pio_i2c_check_error(instance)) {
        err = PICO_ERROR_GENERIC;
        pio_i2c_resume_after_error(instance);
        pio_i2c_stop(instance);
        instance->previous_nostop = false;
    }
    return err;
}
