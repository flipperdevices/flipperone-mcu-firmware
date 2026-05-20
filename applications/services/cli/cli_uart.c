#include "cli_uart.h"
#include <cli/shell/cli_shell.h>
#include <cli/cli_main_shell.h>
#include <cli/cli_commands.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
// #include <furi_hal_dma.h>

#define TAG "CliUart"

#define PIPE_SZ_PER_DIRECTION 1024UL
#define UART_BAUD_RATE        230400UL
#define UART_SERIAL_ID        FuriHalSerialIdUart1
#define TRANSFER_BATCH_SIZE   256UL

//#define CLI_UART_TRACE_ENABLE

#ifdef CLI_UART_TRACE_ENABLE
#define CLI_UART_TRACE(...) FURI_LOG_D(__VA_ARGS__)
#else
#define CLI_UART_TRACE(...)
#endif

struct CliUart {
    CliRegistry* registry;
    FuriEventLoop* event_loop;
    FuriHalSerialHandle* uart_handle;

    CliShell* cli_shell;
    PipeSide* own_pipe;
};

// ================
// Serial callbacks
// ================

void cli_uart_rx_callback(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    UNUSED(handle);
    CliUart* cli_uart = context;

    if(!(event & (FuriHalSerialRxEventData | FuriHalSerialRxEventIdle))) return;
    if(!furi_hal_serial_rx_available(cli_uart->uart_handle)) return;

    PipeSide* pipe = cli_uart->own_pipe;

    while(furi_hal_serial_rx_available(cli_uart->uart_handle) && pipe_spaces_available(pipe)) {
        uint8_t data = furi_hal_serial_rx(cli_uart->uart_handle);
        pipe_send(pipe, &data, sizeof(data));
    }
}

// ===================
// EventLoop callbacks
// ===================

static void cli_uart_data_from_pipe(PipeSide* pipe, void* context) {
    CLI_UART_TRACE(TAG, "data_from_pipe");
    CliUart* cli_uart = context;

    size_t bytes_in_pipe = pipe_bytes_available(pipe);
    size_t to_transfer = MIN(bytes_in_pipe, TRANSFER_BATCH_SIZE);

    uint8_t buffer[to_transfer];
    furi_check(pipe_receive(pipe, buffer, to_transfer) == to_transfer);
    furi_hal_serial_tx(cli_uart->uart_handle, buffer, to_transfer, FuriWaitForever);
}

// ============
// Thread setup
// ============

static CliUart* cli_uart_alloc(void) {
    CliUart* cli_uart = malloc(sizeof(CliUart));

    cli_uart->registry = furi_record_open(RECORD_CLI);

    cli_uart->event_loop = furi_event_loop_alloc();

    cli_uart->uart_handle = furi_hal_serial_control_acquire(UART_SERIAL_ID);
    furi_check(cli_uart->uart_handle);
    furi_hal_serial_init(cli_uart->uart_handle, UART_BAUD_RATE);

    //Todo: implement tx callback tx complete 
    furi_hal_serial_set_callback(cli_uart->uart_handle, NULL, cli_uart_rx_callback, cli_uart);

    furi_hal_serial_async_rx_start(cli_uart->uart_handle, false);

    PipeSideBundle pipes = pipe_alloc(PIPE_SZ_PER_DIRECTION, 1);
    cli_uart->own_pipe = pipes.alices_side;
    pipe_attach_to_event_loop(cli_uart->own_pipe, cli_uart->event_loop);
    pipe_set_callback_context(cli_uart->own_pipe, cli_uart);
    pipe_set_data_arrived_callback(cli_uart->own_pipe, cli_uart_data_from_pipe, 0);

    cli_uart->cli_shell =
        cli_shell_alloc(cli_main_motd, NULL, pipes.bobs_side, cli_uart->registry, NULL);
    cli_shell_free_pipe_on_exit(cli_uart->cli_shell);
    cli_shell_set_prompt(cli_uart->cli_shell, "control");
    cli_shell_start(cli_uart->cli_shell);

    furi_record_create(RECORD_CLI_UART, cli_uart);

    return cli_uart;
}

int32_t cli_uart_srv(void* context) {
    UNUSED(context);

    CliUart* cli_uart = cli_uart_alloc();
    furi_event_loop_run(cli_uart->event_loop);

    return 0;
}
