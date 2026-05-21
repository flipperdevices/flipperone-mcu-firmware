#include "cli_uart.h"
#include <cli/shell/cli_shell.h>
#include <cli/cli_main_shell.h>
#include <cli/cli_commands.h>
#include <furi_hal_serial.h>
#include <furi_hal_serial_control.h>
#include <furi_hal_resources.h>

#define TAG "CliUart"

#define PIPE_SZ_PER_DIRECTION 1024UL
#define UART_BAUD_RATE        230400UL
#define UART_SERIAL_ID        FuriHalSerialIdUart1
#define TRANSFER_BATCH_SIZE   32UL
#define VCP_MESSAGE_Q_LEN     8

// #define CLI_UART_TRACE_ENABLE

#ifdef CLI_UART_TRACE_ENABLE
#define CLI_UART_TRACE(...) FURI_LOG_D(__VA_ARGS__)
#else
#define CLI_UART_TRACE(...)
#endif

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

    volatile bool is_transmitting;
    FuriEventFlag* event_flag;

    CliShell* cli_shell;
    PipeSide* own_pipe;
};

static void cli_uart_signal_event(CliUart* cli_uart, CliUartEvent event) {
    uint32_t ret = furi_event_flag_set(cli_uart->event_flag, event);
    // furi_check(!(ret & FuriFlagError));
    if((ret & FuriFlagError)) {
        FURI_LOG_E(TAG, "Failed to set event flag, error code: 0x%08lX", ret);
    }
}

static void cli_uart_event(FuriEventLoopObject* object, void* context) {
    CliUart* cli_uart = context;
    uint32_t event = furi_event_flag_get(object);

    furi_hal_gpio_write(&gpio_m40, true);
    if(event & CliUartEventRx) {
        CLI_UART_TRACE(TAG, "Rx");
    }

    if(event & CliUartEventTxDone) {
        CLI_UART_TRACE(TAG, "TxDone");
        
        if(pipe_bytes_available(cli_uart->own_pipe)) {
            event |= CliUartEventTx; // trigger next Tx if needed
        } else {
            cli_uart->is_transmitting = false;
        }
    }

    if(event & CliUartEventTx) {
        //furi_hal_gpio_write(&gpio_m40, true);
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

        // while(furi_hal_serial_tx_ready(cli_uart->uart_handle)) {
        //     if(pipe_bytes_available(cli_uart->own_pipe) > 0) {
        //         furi_check(pipe_receive(cli_uart->own_pipe, &data, sizeof(data)) == sizeof(data));
        //         furi_hal_serial_tx_non_blocking(cli_uart->uart_handle, data);
        //     } else {
        //         break;
        //     }
        //     if(!furi_hal_serial_tx_ready(cli_uart->uart_handle)) {
        //         uart_fifo_full = true;
        //         break;
        //     }
        // }

        if(uart_fifo_full) {
            cli_uart->is_transmitting = true;
        }

        CLI_UART_TRACE(TAG, "Tx ->>");
        //furi_hal_gpio_write(&gpio_m40, false);
    }

    furi_hal_gpio_write(&gpio_m40, false);
}

// static void cli_uart_internal_event_happened(FuriEventLoopObject* object, void* context) {
//     CliUart* cli_uart = context;
//     CliVcpInternalEvent event;

//     uint32_t count = furi_message_queue_get_count(cli_uart->internal_evt_queue);
//     CLI_UART_TRACE(TAG, " <-- internal_event count=%lu event=%d", count, event);

//     while(furi_message_queue_get(object, &event, 0) == FuriStatusOk) {
//         furi_hal_gpio_write(&gpio_m40, true);
//         switch(event) {
//         case CliVcpInternalEventRx: {
//             CLI_UART_TRACE(TAG, "Rx");
//             break;
//         }

//         case CliVcpInternalEventTx: {
//             //furi_hal_gpio_write(&gpio_m40, true);
//             CLI_UART_TRACE(TAG, "Tx");

//             uint8_t data;
//             bool uart_fifo_full = false;
//             uint8_t tx_counter = 0;
//             while(pipe_bytes_available(cli_uart->own_pipe) && (++tx_counter < TRANSFER_BATCH_SIZE)) {
//                 if(!furi_hal_serial_tx_ready(cli_uart->uart_handle)) {
//                     uart_fifo_full = true;
//                     break;
//                 }
//                 furi_check(pipe_receive(cli_uart->own_pipe, &data, sizeof(data)) == sizeof(data));
//                 furi_hal_serial_tx_non_blocking(cli_uart->uart_handle, data);
//             }

//             // while(furi_hal_serial_tx_ready(cli_uart->uart_handle)) {
//             //     if(pipe_bytes_available(cli_uart->own_pipe) > 0) {
//             //         furi_check(pipe_receive(cli_uart->own_pipe, &data, sizeof(data)) == sizeof(data));
//             //         furi_hal_serial_tx_non_blocking(cli_uart->uart_handle, data);
//             //     } else {
//             //         break;
//             //     }
//             //     if(!furi_hal_serial_tx_ready(cli_uart->uart_handle)) {
//             //         uart_fifo_full = true;
//             //         break;
//             //     }
//             // }

//             if(uart_fifo_full) {
//                 cli_uart->is_transmitting = true;
//             }

//             CLI_UART_TRACE(TAG, "Tx ->>");
//             //furi_hal_gpio_write(&gpio_m40, false);
//             break;
//         }

//         case CliVcpInternalEventTxDone: {
//             CLI_UART_TRACE(TAG, "TxDone");
//             cli_uart->is_transmitting = false;
//             cli_uart_signal_internal_event(cli_uart, CliVcpInternalEventTx);

//             break;
//         }
//         }

//         furi_hal_gpio_write(&gpio_m40, false);
//     }
// }

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

// ============
// Thread setup
// ============

static CliUart* cli_uart_alloc(void) {
    CliUart* cli_uart = malloc(sizeof(CliUart));

    cli_uart->registry = furi_record_open(RECORD_CLI);

    cli_uart->event_loop = furi_event_loop_alloc();
    cli_uart->is_transmitting = false;

    // cli_uart->internal_evt_queue = furi_message_queue_alloc(VCP_MESSAGE_Q_LEN, sizeof(CliVcpInternalEvent));
    // furi_event_loop_subscribe_message_queue(
    //     cli_uart->event_loop, cli_uart->internal_evt_queue, FuriEventLoopEventIn, cli_uart_internal_event_happened, cli_uart);

    cli_uart->event_flag = furi_event_flag_alloc();
    furi_event_loop_subscribe_event_flag(
        cli_uart->event_loop, cli_uart->event_flag, FuriEventLoopEventIn | FuriEventLoopEventFlagEdge, cli_uart_event, cli_uart);

    cli_uart->uart_handle = furi_hal_serial_control_acquire(UART_SERIAL_ID);
    furi_check(cli_uart->uart_handle);
    furi_hal_serial_init(cli_uart->uart_handle, UART_BAUD_RATE);

    furi_hal_serial_set_callback(cli_uart->uart_handle, cli_uart_tx_complete_callback, cli_uart_rx_callback, cli_uart);

    furi_hal_serial_async_rx_start(cli_uart->uart_handle, false);

    PipeSideBundle pipes = pipe_alloc(PIPE_SZ_PER_DIRECTION, 1);
    cli_uart->own_pipe = pipes.alices_side;
    pipe_attach_to_event_loop(cli_uart->own_pipe, cli_uart->event_loop);
    pipe_set_callback_context(cli_uart->own_pipe, cli_uart);
    pipe_set_data_arrived_callback(cli_uart->own_pipe, cli_uart_data_from_pipe, FuriEventLoopEventFlagEdge);

    cli_uart->cli_shell = cli_shell_alloc(cli_main_motd, NULL, pipes.bobs_side, cli_uart->registry, NULL);
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
