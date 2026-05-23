#include <furi.h>
#include "uart_pio.h"
#include "hardware/pio.h"
#include "uart_tx.pio.h"

typedef struct {
    const GpioPin* gpio_tx;
    PIO pio;
    uint sm;
    uint offset;
    uint32_t baud_rate;
} UartPio;

static UartPio* uart_pio_instance = NULL;

void uart_pio_init(uint32_t baud_rate, const GpioPin* gpio_tx) {
    furi_check(uart_pio_instance == NULL);
    uart_pio_instance = malloc(sizeof(UartPio));
    uart_pio_instance->gpio_tx = gpio_tx;
    uart_pio_instance->baud_rate = baud_rate;
    bool success = pio_claim_free_sm_and_add_program_for_gpio_range(
        &uart_tx_program, &uart_pio_instance->pio, &uart_pio_instance->sm, &uart_pio_instance->offset, uart_pio_instance->gpio_tx->pin, 1, true);
    furi_check(success);
    uart_tx_program_init(uart_pio_instance->pio, uart_pio_instance->sm, uart_pio_instance->offset, uart_pio_instance->gpio_tx->pin, baud_rate);
}

void uart_pio_deinit(void) {
    furi_check(uart_pio_instance != NULL);
    pio_remove_program_and_unclaim_sm(&uart_tx_program, uart_pio_instance->pio, uart_pio_instance->sm, uart_pio_instance->offset);
    free(uart_pio_instance);
    uart_pio_instance = NULL;
}

void uart_pio_set_baud_rate(uint32_t baud_rate) {
    furi_check(uart_pio_instance != NULL);
    const GpioPin* gpio_tx = uart_pio_instance->gpio_tx;
    uart_pio_deinit();
    uart_pio_init(baud_rate, gpio_tx);
}

uint32_t uart_pio_get_baud_rate(void) {
    furi_check(uart_pio_instance != NULL);
    return uart_pio_instance->baud_rate;
}

size_t uart_pio_bloking_tx(const uint8_t* data, size_t size) {
    furi_check(uart_pio_instance != NULL);
    for(size_t i = 0; i < size; i++) {
        uart_tx_program_putc(uart_pio_instance->pio, uart_pio_instance->sm, data[i]);
    }
    return size;
}
