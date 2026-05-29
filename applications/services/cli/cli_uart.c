#include "cli_uart.h"
#include <cli/shell/cli_shell.h>
#include <cli/cli_main_shell.h>
#include <cli/cli_commands.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <api_lock.h>

#define TAG "CliUart"

#define PIPE_SZ_PER_DIRECTION 1024UL
#define UART_BAUD_RATE        230400UL
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

typedef enum {
    CliUartEventTxDone = (1 << 0),
    CliUartEventTx = (1 << 1),
    CliUartEventRx = (1 << 2),
    CliUartEventAll = (CliUartEventTxDone | CliUartEventTx | CliUartEventRx),
} CliUartEvent;

struct CliUart {
    CliRegistry* registry;
    FuriEventLoop* event_loop;
    FuriHalSerialHandle* uart_handle;
    FuriMessageQueue* message_queue;
    bool is_enabled;

    volatile bool is_transmitting;
    FuriEventFlag* event_flag;

    CliShell* cli_shell;
    PipeSide* own_pipe;
};

static void cli_uart_signal_event(CliUart* cli_uart, CliUartEvent event) {
    uint32_t ret = furi_event_flag_set(cli_uart->event_flag, event);
    if((ret & FuriFlagError)) {
        FURI_LOG_E(TAG, "Failed to set event flag, error code: 0x%08lX", ret);
    }
}

static void cli_uart_event(FuriEventLoopObject* object, void* context) {
    CliUart* cli_uart = context;
    uint32_t event = furi_event_flag_get(object);

    if(event & CliUartEventRx) {
        CLI_UART_TRACE(TAG, "Rx");
    }

    if(event & CliUartEventTxDone) {
        CLI_UART_TRACE(TAG, "TxDone");

        event |= CliUartEventTx; // trigger next Tx if needed
    }

    if(event & CliUartEventTx) {
        CLI_UART_TRACE(TAG, "Tx");

        uint8_t data;
        bool uart_fifo_full = false;
        uint8_t tx_counter = 0;
        while(pipe_bytes_available(cli_uart->own_pipe) && (++tx_counter < TRANSFER_BATCH_SIZE)) {
            if(!furi_hal_serial_tx_ready(cli_uart->uart_handle)) {
                uart_fifo_full = true;
                break;
            }
            furi_check(pipe_receive(cli_uart->own_pipe, &data, sizeof(data)) == sizeof(data));
            furi_hal_serial_tx_non_blocking(cli_uart->uart_handle, data);
        }

        if(uart_fifo_full) {
            cli_uart->is_transmitting = true;
        } else {
            cli_uart->is_transmitting = false;
        }

        CLI_UART_TRACE(TAG, "Tx ->>");
    }
}

// ================
// Serial callbacks
// ================

static void cli_uart_rx_callback(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    UNUSED(handle);
    CliUart* cli_uart = context;

    uint8_t data[TRANSFER_BATCH_SIZE];
    size_t length = 0;
    size_t to_transfer = 0;
    if(event & (FuriHalSerialRxEventData | FuriHalSerialRxEventIdle)) {
        length = pipe_spaces_available(cli_uart->own_pipe);
        to_transfer = MIN(length, TRANSFER_BATCH_SIZE);
        length = furi_hal_serial_rx_data_non_blocking(cli_uart->uart_handle, data, to_transfer);
        if(length > 0) {
            furi_check(pipe_send(cli_uart->own_pipe, data, length) == length);
        }
    }

    cli_uart_signal_event(cli_uart, CliUartEventRx);
}

static void cli_uart_tx_complete_callback(FuriHalSerialHandle* handle, FuriHalSerialTxEvent event, void* context) {
    UNUSED(handle);
    CliUart* cli_uart = context;
    if(!(event & FuriHalSerialTxEventComplete)) return;
    cli_uart_signal_event(cli_uart, CliUartEventTxDone);
}

// ===================
// EventLoop callbacks
// ===================

static void cli_uart_data_from_pipe(PipeSide* pipe, void* context) {
    CLI_UART_TRACE(TAG, "data_from_pipe");
    CliUart* cli_uart = context;
    if(cli_uart->is_transmitting) return;
    cli_uart_signal_event(cli_uart, CliUartEventTx);
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
        cli_uart->own_pipe = pipes.alices_side;
        pipe_attach_to_event_loop(cli_uart->own_pipe, cli_uart->event_loop);
        pipe_set_callback_context(cli_uart->own_pipe, cli_uart);
        pipe_set_data_arrived_callback(cli_uart->own_pipe, cli_uart_data_from_pipe, FuriEventLoopEventFlagEdge);

        cli_uart->cli_shell = cli_shell_alloc(cli_main_motd, NULL, pipes.bobs_side, cli_uart->registry, NULL);
        cli_shell_free_pipe_on_exit(cli_uart->cli_shell);
        cli_shell_set_prompt(cli_uart->cli_shell, CLI_UART_SHELL_PROMPT);
        cli_shell_start(cli_uart->cli_shell);
        break;

    case CliUartMessageTypeDisable:
        if(!cli_uart->is_enabled) break;
        FURI_LOG_D(TAG, "Disabling");
        cli_uart->is_enabled = false;

        // disconnect our side of the pipe
        pipe_detach_from_event_loop(cli_uart->own_pipe);
        pipe_free(cli_uart->own_pipe);
        cli_uart->own_pipe = NULL;

        // wait for shell to stop
        cli_shell_join(cli_uart->cli_shell);
        cli_shell_free(cli_uart->cli_shell);

        furi_record_close(RECORD_CLI);
        cli_uart->registry = NULL;

        pipe_free(cli_uart->own_pipe);

        break;
    }

    api_lock_unlock(message.api_lock);
}

// ============
// Thread setup
// ============

static CliUart* cli_uart_alloc(void) {
    CliUart* cli_uart = malloc(sizeof(CliUart));

    cli_uart->registry = furi_record_open(RECORD_CLI);

    cli_uart->event_loop = furi_event_loop_alloc();
    cli_uart->is_transmitting = false;
    cli_uart->is_enabled = false;

    cli_uart->event_flag = furi_event_flag_alloc();
    furi_event_loop_subscribe_event_flag(
        cli_uart->event_loop, cli_uart->event_flag, FuriEventLoopEventIn | FuriEventLoopEventFlagEdge, cli_uart_event, cli_uart);

    cli_uart->message_queue = furi_message_queue_alloc(UART_MESSAGE_Q_LEN, sizeof(CliUartMessage));
    furi_event_loop_subscribe_message_queue(cli_uart->event_loop, cli_uart->message_queue, FuriEventLoopEventIn, cli_uart_message_received, cli_uart);

    cli_uart->uart_handle = furi_hal_serial_control_acquire(UART_SERIAL_ID);
    furi_check(cli_uart->uart_handle);
    furi_hal_serial_init(cli_uart->uart_handle, UART_BAUD_RATE);

    furi_hal_serial_set_callback(cli_uart->uart_handle, cli_uart_tx_complete_callback, cli_uart_rx_callback, cli_uart);

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
