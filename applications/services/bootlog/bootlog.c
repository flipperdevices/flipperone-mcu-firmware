#include "bootlog.h"

#include <furi.h>
#include <furi_hal.h>
#include <containers/pipe.h>
#include <cli/cli_command.h>
#include <cli/cli_registry.h>
#include <cli/cli_commands.h>

#define TAG "Bootlog"

#define BOOTLOG_BAUD_RATE    1500000
#define BOOTLOG_BUFFER_SIZE  8192
#define BOOTLOG_STREAM_SIZE  2048
#define BOOTLOG_UART         FuriHalSerialIdUart0
#define BOOTLOG_UART_NAME   "UART0"

typedef struct {
    FuriEventLoop* event_loop;
    FuriStreamBuffer* rx_stream;
    FuriHalSerialHandle* serial_handle;

    /* Ring buffer */
    uint8_t* buffer;
    size_t head;
    size_t tail;
    size_t total_captured;
    FuriMutex* buffer_mutex;

    bool capturing;

    FuriPubSub* event_pubsub;
} BootlogApp;

/* ---------------------------------------------------------------------------
 * Ring buffer — single writer (event loop callback), many readers (CLI, GUI)
 * Mutex protects against concurrent read/write on dual-core RP2350.
 * ---------------------------------------------------------------------------*/

static void bootlog_ring_write(BootlogApp* app, const uint8_t* data, size_t length) {
    furi_mutex_acquire(app->buffer_mutex, FuriWaitForever);
    for(size_t i = 0; i < length; i++) {
        app->buffer[app->head] = data[i];
        app->head = (app->head + 1) % BOOTLOG_BUFFER_SIZE;
        if(app->head == app->tail) {
            app->tail = (app->tail + 1) % BOOTLOG_BUFFER_SIZE;
        }
    }
    app->total_captured += length;
    furi_mutex_release(app->buffer_mutex);
}

/* ---------------------------------------------------------------------------
 * Publish a BootlogNotification to all subscribers.
 * Callbacks fire synchronously in the event loop context. The subscriber
 * callback receives a const BootlogNotification* with the current ring
 * buffer head and bytes available.
 * ---------------------------------------------------------------------------*/

static void bootlog_notify_subscribers(BootlogApp* app) {
    BootlogNotification notification;

    furi_mutex_acquire(app->buffer_mutex, FuriWaitForever);
    notification.new_head = app->head;
    if(app->head >= app->tail) {
        notification.bytes_available = app->head - app->tail;
    } else {
        notification.bytes_available = BOOTLOG_BUFFER_SIZE - app->tail + app->head;
    }
    furi_mutex_release(app->buffer_mutex);

    furi_pubsub_publish(app->event_pubsub, &notification);
}

/* ---------------------------------------------------------------------------
 * FuriEventLoop callback — fires when the stream buffer has data.
 * ---------------------------------------------------------------------------*/

static void bootlog_stream_buffer_callback(FuriEventLoopObject* object, void* context) {
    UNUSED(object);
    BootlogApp* app = context;

    if(!app->capturing) return;

    /* Drain the entire stream buffer into the ring buffer */
    size_t length;
    do {
        uint8_t data[64];
        length = furi_stream_buffer_receive(app->rx_stream, data, sizeof(data), 0);
        if(length > 0) {
            bootlog_ring_write(app, data, length);
        }
    } while(length > 0);

    bootlog_notify_subscribers(app);
}

/* ---------------------------------------------------------------------------
 * UART ISR callback — runs in interrupt context.
 * Pushes raw bytes into the FuriStreamBuffer.
 * ---------------------------------------------------------------------------*/

static void bootlog_on_irq_cb(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    furi_assert(context);
    BootlogApp* app = context;

    if(event & (FuriHalSerialRxEventData | FuriHalSerialRxEventIdle)) {
        uint8_t data[64];
        size_t length = furi_hal_serial_rx_data_non_blocking(handle, data, sizeof(data));
        if(length > 0) {
            furi_stream_buffer_send(app->rx_stream, data, length, 0);
        }
    }
}

/* ===========================================================================
 * Public Service API
 * ===========================================================================*/

void bootlog_start(void) {
    BootlogApp* app = furi_record_open(RECORD_BOOTLOG);
    if(app->capturing) {
        printf("Bootlog: already capturing\r\n");
        furi_record_close(RECORD_BOOTLOG);
        return;
    }

    app->serial_handle = furi_hal_serial_control_acquire(BOOTLOG_UART);
    if(!app->serial_handle) {
        printf("Bootlog: failed to acquire " BOOTLOG_UART_NAME " (in use?)\r\n");
        furi_record_close(RECORD_BOOTLOG);
        return;
    }

    furi_hal_serial_init(app->serial_handle, BOOTLOG_BAUD_RATE);
    furi_hal_serial_set_config(
        app->serial_handle,
        FuriHalSerialConfigDataBits8,
        FuriHalSerialConfigParityNone,
        FuriHalSerialConfigStopBits_1);
    furi_hal_serial_set_callback(app->serial_handle, NULL, bootlog_on_irq_cb, app);

    app->buffer = malloc(BOOTLOG_BUFFER_SIZE);
    app->head = 0;
    app->tail = 0;
    app->total_captured = 0;
    app->capturing = true;
    furi_stream_buffer_reset(app->rx_stream);

    furi_hal_serial_async_rx_start(app->serial_handle, true);
    printf("Bootlog: " BOOTLOG_UART_NAME " acquired, capturing at %d baud\r\n", BOOTLOG_BAUD_RATE);
    furi_record_close(RECORD_BOOTLOG);
}

void bootlog_stop(void) {
    BootlogApp* app = furi_record_open(RECORD_BOOTLOG);
    if(!app->capturing) {
        printf("Bootlog: not capturing\r\n");
        furi_record_close(RECORD_BOOTLOG);
        return;
    }

    furi_hal_serial_async_rx_stop(app->serial_handle);
    furi_hal_serial_deinit(app->serial_handle);
    furi_hal_serial_control_release(app->serial_handle);
    app->serial_handle = NULL;

    app->capturing = false;

    free(app->buffer);
    app->buffer = NULL;

    printf("Bootlog: " BOOTLOG_UART_NAME " released, %zu bytes captured\r\n", app->total_captured);
    furi_record_close(RECORD_BOOTLOG);
}

bool bootlog_get_state(BootlogState* state) {
    BootlogApp* app = furi_record_open(RECORD_BOOTLOG);
    furi_mutex_acquire(app->buffer_mutex, FuriWaitForever);
    state->buffer = app->buffer;
    state->size = BOOTLOG_BUFFER_SIZE;
    state->head = app->head;
    state->tail = app->tail;
    state->total = app->total_captured;
    state->capturing = app->capturing;
    furi_mutex_release(app->buffer_mutex);
    furi_record_close(RECORD_BOOTLOG);
    return true;
}

bool bootlog_is_capturing(void) {
    BootlogApp* app = furi_record_open(RECORD_BOOTLOG);
    bool result = app->capturing;
    furi_record_close(RECORD_BOOTLOG);
    return result;
}

FuriPubSub* bootlog_get_event_pubsub(void) {
    BootlogApp* app = furi_record_open(RECORD_BOOTLOG);
    FuriPubSub* pubsub = app->event_pubsub;
    furi_record_close(RECORD_BOOTLOG);
    return pubsub;
}

/* ===========================================================================
 * Helper Functions
 * ===========================================================================*/

static size_t bootlog_available(BootlogApp* app) {
    if(app->head >= app->tail) {
        return app->head - app->tail;
    }
    return BOOTLOG_BUFFER_SIZE - app->tail + app->head;
}

static void bootlog_write_with_crlf(const uint8_t* data, size_t length) {
    for(size_t i = 0; i < length; i++) {
        uint8_t c = data[i];
        if(c == '\r') {
            if(i + 1 < length && data[i + 1] == '\n') {
                i++; /* CRLF: consume the \n too */
            }
            printf("\r\n");
        } else if(c == '\n') {
            printf("\r\n");
        } else {
            printf("%c", c);
        }
    }
}

static void bootlog_ring_read(BootlogApp* app, size_t start, uint8_t* buf, size_t len) {
    size_t idx = start;
    for(size_t i = 0; i < len; i++) {
        buf[i] = app->buffer[idx];
        idx = (idx + 1) % BOOTLOG_BUFFER_SIZE;
    }
}


/* ===========================================================================
 * CLI Commands 
 * ===========================================================================*/

static void bootlog_cli_show(PipeSide* pipe, BootlogApp* app) {
    furi_mutex_acquire(app->buffer_mutex, FuriWaitForever);

    if(!app->buffer) {
        printf("Bootlog: not capturing (use 'bootlog start' first)\r\n");
        furi_mutex_release(app->buffer_mutex);
        return;
    }

    size_t available = bootlog_available(app);

    if(available == 0) {
        printf("--- Bootlog (empty) ---\r\n");
        furi_mutex_release(app->buffer_mutex);
        return;
    }

    printf("--- Bootlog (%zu bytes captured, %zu available) ---\r\n",
           app->total_captured, available);

    /* Copy into a flat buffer so we can release the mutex before output */
    uint8_t* flat = malloc(available);
    if(flat) {
        bootlog_ring_read(app, app->tail, flat, available);
        furi_mutex_release(app->buffer_mutex);

        bootlog_write_with_crlf(flat, available);
        free(flat);
    } else {
        furi_mutex_release(app->buffer_mutex);
    }

    printf("\r\n--- End ---\r\n");
}

static void bootlog_cli_stream(PipeSide* pipe, BootlogApp* app) {
    if(!app->buffer) {
        printf("Bootlog: not capturing (use 'bootlog start' first)\r\n");
        return;
    }

    printf("--- Bootlog streaming, Ctrl+C to stop ---\r\n");

    furi_mutex_acquire(app->buffer_mutex, FuriWaitForever);
    size_t last_head = app->head;
    furi_mutex_release(app->buffer_mutex);

    /* Poll every 10 ms. At 1.5 Mbps that's at most ~1500 bytes
     * per poll which can be done within the 8 KB ring buffer. */
    while(pipe_state(pipe) == PipeStateOpen) {
        
        /* Handle CTRL_C signal. */
        while(pipe_bytes_available(pipe) > 0) {
            char c;
            pipe_receive(pipe, &c, 1);
            if(c == 0x03) {
                printf("^C\r\n");
                return;
            }
        }

        furi_delay_ms(10);

        furi_mutex_acquire(app->buffer_mutex, FuriWaitForever);
        size_t current_head = app->head;

        if(current_head == last_head) {
            furi_mutex_release(app->buffer_mutex);
            continue;
        }

        /* Bytes written since last_head (handles wrap-around) */
        size_t total;
        size_t first_len;
        size_t first_start;
        size_t second_len;

        if(current_head > last_head) {
            total = current_head - last_head;
            first_len = total;
            first_start = last_head;
            second_len = 0;
        } else {
            total = (BOOTLOG_BUFFER_SIZE - last_head) + current_head;
            first_len = BOOTLOG_BUFFER_SIZE - last_head;
            first_start = last_head;
            second_len = current_head;
        }

        if(total == 0 || total > BOOTLOG_BUFFER_SIZE) {
            furi_mutex_release(app->buffer_mutex);
            continue;
        }

        /* Copy into a flat buffer, release mutex, then output */
        uint8_t* data = malloc(total);
        if(!data) {
            furi_mutex_release(app->buffer_mutex);
            continue;
        }

        bootlog_ring_read(app, first_start, data, first_len);
        if(second_len > 0) {
            bootlog_ring_read(app, 0, data + first_len, second_len);
        }
        last_head = current_head;

        furi_mutex_release(app->buffer_mutex);

        bootlog_write_with_crlf(data, total);
        free(data);
    }
}

static void bootlog_cli_clear(BootlogApp* app) {
    furi_mutex_acquire(app->buffer_mutex, FuriWaitForever);
    app->head = 0;
    app->tail = 0;
    app->total_captured = 0;
    furi_mutex_release(app->buffer_mutex);
    printf("Bootlog: buffer cleared\r\n");
}

static void bootlog_cli_status(BootlogApp* app) {
    furi_mutex_acquire(app->buffer_mutex, FuriWaitForever);
    size_t available = app->buffer ? bootlog_available(app) : 0;
    size_t pct = app->buffer ? (available * 100) / BOOTLOG_BUFFER_SIZE : 0;
    furi_mutex_release(app->buffer_mutex);

    printf("capturing: %s\r\n", app->capturing ? "yes" : "no");
    printf("total: %zu bytes\r\n", app->total_captured);
    printf("buffer: %zu/%zu (%zu%%)\r\n", available, BOOTLOG_BUFFER_SIZE, pct);
}

void bootlog_cli(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(context);
    BootlogApp* app = furi_record_open(RECORD_BOOTLOG);

    if(furi_string_size(args) == 0) {
        cli_print_usage("bootlog", "start|stop|show|stream|clear|status", "");
        furi_record_close(RECORD_BOOTLOG);
        return;
    }

    if(furi_string_cmp_str(args, "start") == 0) {
        bootlog_start();
    } else if(furi_string_cmp_str(args, "stop") == 0) {
        bootlog_stop();
    } else if(furi_string_cmp_str(args, "show") == 0) {
        bootlog_cli_show(pipe, app);
    } else if(furi_string_cmp_str(args, "stream") == 0) {
        bootlog_cli_stream(pipe, app);
    } else if(furi_string_cmp_str(args, "clear") == 0) {
        bootlog_cli_clear(app);
    } else if(furi_string_cmp_str(args, "status") == 0) {
        bootlog_cli_status(app);
    } else {
        cli_print_usage("bootlog", "start|stop|show|stream|clear|status", furi_string_get_cstr(args));
    }

    furi_record_close(RECORD_BOOTLOG);
}

/* ===========================================================================
 * Service entry point
 * ===========================================================================*/

int32_t bootlog_srv(void* p) {
    UNUSED(p);

    BootlogApp* app = malloc(sizeof(BootlogApp));

    app->rx_stream = furi_stream_buffer_alloc(BOOTLOG_STREAM_SIZE, 1);
    app->buffer_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    app->buffer = NULL;
    app->head = 0;
    app->tail = 0;
    app->total_captured = 0;
    app->capturing = false;
    app->serial_handle = NULL;
    app->event_pubsub = furi_pubsub_alloc();

    /*
     * ISR sends data to a stream buffer; the event loop subscription
     * fires a callback to process and store it in the ring buffer.
     */
    app->event_loop = furi_event_loop_alloc();

    furi_event_loop_subscribe_stream_buffer(
        app->event_loop,
        app->rx_stream,
        FuriEventLoopEventIn,
        bootlog_stream_buffer_callback,
        app);

    furi_record_create(RECORD_BOOTLOG, app);

    FURI_LOG_I(TAG, "Bootlog service started");

    furi_event_loop_run(app->event_loop);

    return 0;
}
