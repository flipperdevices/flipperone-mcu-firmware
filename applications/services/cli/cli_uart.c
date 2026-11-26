#include "cli_uart.h"
#include <cli/shell/cli_shell.h>
#include <cli/cli_main_shell.h>
#include <cli/cli_commands.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <api_lock.h>

#define TAG "CliUart"

#define PIPE_SZ_PER_DIRECTION 1024UL
#define UART_BAUD_RATE        1500000UL
#define UART_SERIAL_ID        FuriHalSerialIdUart1
#define TRANSFER_BATCH_SIZE   32UL
#define CLI_UART_SHELL_PROMPT "control"
#define UART_MESSAGE_Q_LEN    5

//#define CLI_UART_TRACE_ENABLE

#ifdef CLI_UART_TRACE_ENABLE
#define CLI_UART_TRACE(...) FURI_LOG_D(__VA_ARGS__)
#else
#define CLI_UART_TRACE(...)
#endif

typedef struct {
    enum {
        CliUartMessageTypeEnable,
        CliUartMessageTypeDisable,
    } type;
    FuriApiLock api_lock;
    union {};
} CliUartMessage;

struct CliUart {
    CliRegistry* registry;
    FuriEventLoop* event_loop;
    FuriHalSerialHandle* uart_handle;
    FuriMessageQueue* message_queue;
    bool is_enabled;

    CliShell* cli_shell;
    PipeSide* volatile own_pipe; // read by the UART interrupt

    // Taken from the pipe in batches: a pipe operation per byte would cost the interrupt far more than the byte
    uint8_t tx_buffer[TRANSFER_BATCH_SIZE];
    size_t tx_buffer_pos;
    size_t tx_buffer_len;
};

// ================
// Serial callbacks
// ================

static void cli_uart_rx_callback(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    UNUSED(handle);
    CliUart* cli_uart = context;

    // What the pipe has no room for is dropped: there is no flow control to hold the sender back.
    // Before the console is enabled there is no pipe.
    uint8_t data[TRANSFER_BATCH_SIZE];
    size_t length;
    if(event & (FuriHalSerialRxEventData | FuriHalSerialRxEventIdle)) {
        while((length = furi_hal_serial_rx_data_non_blocking(cli_uart->uart_handle, data, sizeof(data))) > 0) {
            PipeSide* pipe = cli_uart->own_pipe;
            size_t to_send = pipe ? MIN(length, pipe_spaces_available(pipe)) : 0;
            if(to_send > 0) furi_check(pipe_send(pipe, data, to_send) == to_send);
        }
    }
}

static bool cli_uart_tx_callback(FuriHalSerialHandle* handle, uint8_t* byte, void* context) {
    UNUSED(handle);
    CliUart* cli_uart = context;

    if(cli_uart->tx_buffer_pos == cli_uart->tx_buffer_len) {
        PipeSide* pipe = cli_uart->own_pipe;
        if(!pipe) return false;
        // pipe_receive() would wait for bytes that are not there, which in an interrupt never ends
        size_t length = MIN(pipe_bytes_available(pipe), sizeof(cli_uart->tx_buffer));
        if(!length) return false;
        furi_check(pipe_receive(pipe, cli_uart->tx_buffer, length) == length);
        cli_uart->tx_buffer_pos = 0;
        cli_uart->tx_buffer_len = length;
    }

    *byte = cli_uart->tx_buffer[cli_uart->tx_buffer_pos++];
    return true;
}

// ===================
// EventLoop callbacks
// ===================

static void cli_uart_data_from_pipe(PipeSide* pipe, void* context) {
    UNUSED(pipe);
    CLI_UART_TRACE(TAG, "data_from_pipe");
    CliUart* cli_uart = context;
    furi_hal_serial_async_tx_start(cli_uart->uart_handle);
}

/**
 * Processes messages arriving from other threads
 */
static void cli_uart_message_received(FuriEventLoopObject* object, void* context) {
    CliUart* cli_uart = context;
    CliUartMessage message;
    furi_check(furi_message_queue_get(object, &message, 0) == FuriStatusOk);

    switch(message.type) {
    case CliUartMessageTypeEnable:
        if(cli_uart->is_enabled) break;
        FURI_LOG_D(TAG, "Enabling");
        cli_uart->is_enabled = true;

        PipeSideBundle pipes = pipe_alloc(PIPE_SZ_PER_DIRECTION, 1);
        pipe_attach_to_event_loop(pipes.alices_side, cli_uart->event_loop);
        pipe_set_callback_context(pipes.alices_side, cli_uart);
        pipe_set_data_arrived_callback(pipes.alices_side, cli_uart_data_from_pipe, FuriEventLoopEventFlagEdge);
        cli_uart->own_pipe = pipes.alices_side; // the UART interrupt sees the pipe once it is set up

        cli_uart->registry = furi_record_open(RECORD_CLI);
        cli_uart->cli_shell = cli_shell_alloc(cli_main_motd, NULL, pipes.bobs_side, cli_uart->registry, NULL);
        cli_shell_free_pipe_on_exit(cli_uart->cli_shell);
        cli_shell_set_prompt(cli_uart->cli_shell, CLI_UART_SHELL_PROMPT);
        cli_shell_start(cli_uart->cli_shell);
        break;

    case CliUartMessageTypeDisable:
        if(!cli_uart->is_enabled) break;
        FURI_LOG_D(TAG, "Disabling");
        cli_uart->is_enabled = false;

        // disconnect our side of the pipe; the UART interrupt must stop seeing it before it is freed
        PipeSide* pipe = cli_uart->own_pipe;
        cli_uart->own_pipe = NULL;
        pipe_detach_from_event_loop(pipe);
        pipe_free(pipe);

        // wait for shell to stop
        cli_shell_join(cli_uart->cli_shell);
        cli_shell_free(cli_uart->cli_shell);

        furi_record_close(RECORD_CLI);
        cli_uart->registry = NULL;

        break;
    }

    api_lock_unlock(message.api_lock);
}

// ============
// Thread setup
// ============

static CliUart* cli_uart_alloc(void) {
    CliUart* cli_uart = malloc(sizeof(CliUart));

    cli_uart->event_loop = furi_event_loop_alloc();
    cli_uart->is_enabled = false;

    cli_uart->message_queue = furi_message_queue_alloc(UART_MESSAGE_Q_LEN, sizeof(CliUartMessage));
    furi_event_loop_subscribe_message_queue(cli_uart->event_loop, cli_uart->message_queue, FuriEventLoopEventIn, cli_uart_message_received, cli_uart);

    cli_uart->uart_handle = furi_hal_serial_control_acquire(UART_SERIAL_ID);
    furi_check(cli_uart->uart_handle);
    furi_hal_serial_init(cli_uart->uart_handle, UART_BAUD_RATE);

    furi_hal_serial_set_callback(cli_uart->uart_handle, cli_uart_tx_callback, cli_uart_rx_callback, cli_uart);

    furi_hal_serial_async_rx_start(cli_uart->uart_handle, false);

    return cli_uart;
}

int32_t cli_uart_srv(void* context) {
    UNUSED(context);

    CliUart* cli_uart = cli_uart_alloc();
    furi_record_create(RECORD_CLI_UART, cli_uart);
    furi_event_loop_run(cli_uart->event_loop);

    return 0;
}

// ==========
// Public API
// ==========

static void cli_uart_synchronous_request(CliUart* cli_uart, CliUartMessage* message) {
    message->api_lock = api_lock_alloc_locked();
    furi_message_queue_put(cli_uart->message_queue, message, FuriWaitForever);
    api_lock_wait_unlock_and_free(message->api_lock);
}

void cli_uart_enable(CliUart* cli_uart) {
    furi_check(cli_uart);
    CliUartMessage message = {
        .type = CliUartMessageTypeEnable,
    };
    cli_uart_synchronous_request(cli_uart, &message);
}

void cli_uart_disable(CliUart* cli_uart) {
    furi_check(cli_uart);
    CliUartMessage message = {
        .type = CliUartMessageTypeDisable,
    };
    cli_uart_synchronous_request(cli_uart, &message);
}
