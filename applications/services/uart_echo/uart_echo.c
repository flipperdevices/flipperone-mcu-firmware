#include "furi_hal_serial.h"
#include <furi.h>
#include <furi_hal.h>

#define TAG "UartEcho"

#define DEFAULT_BAUD_RATE 230400
#define DEFAULT_DATA_BITS FuriHalSerialConfigDataBits8
#define DEFAULT_PARITY    FuriHalSerialConfigParityNone
#define DEFAULT_STOP_BITS FuriHalSerialConfigStopBits_1

typedef struct {
    FuriThread* worker_thread;
    FuriStreamBuffer* rx_stream;
    FuriHalSerialHandle* serial_handle;
} UartEchoApp;

typedef enum {
    WorkerEventReserved = (1 << 0), // Reserved for StreamBuffer internal event
    WorkerEventStop = (1 << 1),
    WorkerEventRxData = (1 << 2),
    WorkerEventRxIdle = (1 << 3),
    WorkerEventRxOverrunError = (1 << 4),
    WorkerEventRxFramingError = (1 << 5),
    WorkerEventRxBreakError = (1 << 6),
    WorkerEventRxParityError = (1 << 7),
    WorkerEventTxComplete = (1 << 8),
} WorkerEventFlags;

#define WORKER_EVENTS_MASK                                                                                                                       \
    (WorkerEventStop | WorkerEventRxData | WorkerEventRxIdle | WorkerEventRxOverrunError | WorkerEventRxFramingError | WorkerEventRxBreakError | \
     WorkerEventRxParityError | WorkerEventTxComplete)

static int32_t uart_echo_worker(void* context) {
    furi_assert(context);
    UartEchoApp* app = context;

    while(1) {
        uint32_t events = furi_thread_flags_wait(WORKER_EVENTS_MASK, FuriFlagWaitAny, FuriWaitForever);
        furi_check((events & FuriFlagError) == 0);

        if(events & WorkerEventStop) break;

        if(events & WorkerEventTxComplete) {
            FURI_LOG_I(TAG, "Transmit complete");
        }

        if(events & WorkerEventRxData) {
            size_t length = 0;
            do {
                uint8_t data[64];
                length = furi_stream_buffer_receive(app->rx_stream, data, 64, 0);
                if(length > 0) {
                    furi_hal_serial_tx(app->serial_handle, data, length, FuriWaitForever);
                }
            } while(length > 0);
        }

        if(events & WorkerEventRxIdle) {
            furi_hal_serial_tx(app->serial_handle, (uint8_t*)"\r\nDetect IDLE\r\n", 15, FuriWaitForever);
        }

        if(events & (WorkerEventRxOverrunError | WorkerEventRxFramingError | WorkerEventRxBreakError)) {
            if(events & WorkerEventRxOverrunError) {
                furi_hal_serial_tx(app->serial_handle, (uint8_t*)"\r\nDetect ORE\r\n", 14, FuriWaitForever);
            }
            if(events & WorkerEventRxFramingError) {
                furi_hal_serial_tx(app->serial_handle, (uint8_t*)"\r\nDetect FE\r\n", 13, FuriWaitForever);
            }
            if(events & WorkerEventRxBreakError) {
                furi_hal_serial_tx(app->serial_handle, (uint8_t*)"\r\nDetect BE\r\n", 13, FuriWaitForever);
            }
            if(events & WorkerEventRxParityError) {
                furi_hal_serial_tx(app->serial_handle, (uint8_t*)"\r\nDetect PE\r\n", 13, FuriWaitForever);
            }
        }
    }

    return 0;
}

static void uart_echo_on_irq_cb(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    furi_assert(context);
    UNUSED(handle);
    UartEchoApp* app = context;
    WorkerEventFlags flag = 0;

    uint8_t data[64];
    size_t length = 0;
    char buf[32];

    if(event & FuriHalSerialRxEventData) {
        // length = furi_hal_serial_rx_data_non_blocking(handle, data, 64);
        // sprintf(buf, "\r\nReceived %zu\t", length);
        // furi_hal_serial_tx(app->serial_handle, buf, strlen(buf), FuriWaitForever);
        // furi_hal_serial_tx(app->serial_handle, data, length, FuriWaitForever);

        //Todo: spinlock
        length = furi_hal_serial_rx_data_non_blocking(handle, data, 64);
        furi_stream_buffer_send(app->rx_stream, &data, length, 0);

        flag |= WorkerEventRxData;
    }

    if(event & FuriHalSerialRxEventIdle) {
        //idle line detected, packet transmission may have ended
        flag |= WorkerEventRxIdle | WorkerEventRxData;

        length = furi_hal_serial_rx_data_non_blocking(handle, data, 64);
        //if(length > 0) furi_hal_serial_tx(app->serial_handle, data, length, FuriWaitForever);
        furi_stream_buffer_send(app->rx_stream, &data, length, 0);
    }

    //error detected
    if(event & FuriHalSerialRxEventFrameError) {
        flag |= WorkerEventRxFramingError;
    }
    if(event & FuriHalSerialRxEventBreakError) {
        flag |= WorkerEventRxBreakError;
    }
    if(event & FuriHalSerialRxEventOverrunError) {
        flag |= WorkerEventRxOverrunError;
    }
    if(event & FuriHalSerialRxEventParityError) {
        flag |= WorkerEventRxParityError;
    }

    furi_thread_flags_set(furi_thread_get_id(app->worker_thread), flag);
}

static void uart_echo_tx_complete_irq_cb(FuriHalSerialHandle* handle, FuriHalSerialTxEvent event, void* context){
    WorkerEventFlags flag = 0;
    UartEchoApp* app = context;
    if(event & FuriHalSerialTxEventComplete){
        flag |= WorkerEventTxComplete;
    }
    furi_thread_flags_set(furi_thread_get_id(app->worker_thread), flag);
}

static UartEchoApp*
    uart_echo_app_alloc(uint32_t baudrate, FuriHalSerialConfigDataBits data_bits, FuriHalSerialConfigParity parity, FuriHalSerialConfigStopBits stop_bits) {
    UartEchoApp* app = malloc(sizeof(UartEchoApp));
    app->rx_stream = furi_stream_buffer_alloc(2048, 1);

    app->worker_thread = furi_thread_alloc_ex("UsbUartWorker", 1024, uart_echo_worker, app);
    furi_thread_start(app->worker_thread);

    // Enable uart listener
    app->serial_handle = furi_hal_serial_control_acquire(FuriHalSerialIdUart1);
    furi_check(app->serial_handle);
    furi_hal_serial_init(app->serial_handle, baudrate);
    furi_hal_serial_set_config(app->serial_handle, data_bits, parity, stop_bits);

    furi_hal_serial_set_callback(app->serial_handle, uart_echo_tx_complete_irq_cb, uart_echo_on_irq_cb, app);
    furi_hal_serial_async_rx_start(app->serial_handle, true);

    return app;
}

static void uart_echo_app_free(UartEchoApp* app) {
    furi_assert(app);

    furi_thread_flags_set(furi_thread_get_id(app->worker_thread), WorkerEventStop);
    furi_thread_join(app->worker_thread);
    furi_thread_free(app->worker_thread);

    furi_hal_serial_async_rx_stop(app->serial_handle);
    furi_hal_serial_deinit(app->serial_handle);
    furi_hal_serial_control_release(app->serial_handle);

    furi_stream_buffer_free(app->rx_stream);

    // Free rest
    free(app);
}

int32_t uart_echo_app(void* p) {
    UNUSED(p);
    uint32_t baudrate = DEFAULT_BAUD_RATE;
    FuriHalSerialConfigDataBits data_bits = DEFAULT_DATA_BITS;
    FuriHalSerialConfigParity parity = DEFAULT_PARITY;
    FuriHalSerialConfigStopBits stop_bits = DEFAULT_STOP_BITS;

    UartEchoApp* app = uart_echo_app_alloc(baudrate, data_bits, parity, stop_bits);
    // ToDo: do nothing
    while(1) {
        furi_delay_ms(FuriWaitForever);
    }
    uart_echo_app_free(app);
    return 0;
}
