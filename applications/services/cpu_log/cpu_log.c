#include "cpu_log.h"
#include <m-list.h>
#include <furi.h>
#include <furi_hal.h>
#include <api_lock.h>

#define TAG "CpuLog"

#define CPU_LOG_BAUD_RATE   1500000
#define CPU_LOG_BUFFER_SIZE 8192
#define CPU_LOG_STREAM_SIZE 512
#define CPU_LOG_UART        FuriHalSerialIdUart0

typedef struct {
    void (*callback)(const uint8_t* data, size_t size, void* context);
    void* context;
    size_t buf_tail;
} CpuLogHandlerInstance;

LIST_DEF(CpuLogHandlersList, CpuLogHandlerInstance, M_POD_OPLIST)

struct CpuLogSrv {
    FuriEventLoop* event_loop;
    FuriMessageQueue* msg_queue;
    CpuLogHandlersList_t handlers;

    FuriStreamBuffer* rx_stream;
    FuriHalSerialHandle* serial_handle;

    /* Ring buffer */
    uint8_t* buffer;
    size_t head;
    size_t tail;
};

typedef struct {
    enum {
        CpuLogSrvMessageTypeSubscribe = 0,
        CpuLogSrvMessageTypeUnsubscribe,
        CpuLogSrvMessageTypeClear,
    } type;
    CpuLogHandler* handler;
    FuriApiLock lock;
} CpuLogSrvMessage;

static void cpu_log_ring_write(CpuLogSrv* app, const uint8_t* data, size_t length) {
    for(size_t i = 0; i < length; i++) {
        app->buffer[app->head] = data[i];
        app->head = (app->head + 1) % CPU_LOG_BUFFER_SIZE;
        if(app->head == app->tail) {
            app->tail = (app->tail + 1) % CPU_LOG_BUFFER_SIZE;
        }
    }
}

static void cpu_log_call_handler(CpuLogSrv* app, CpuLogHandlerInstance* handler) {
    size_t len = 0;
    if(app->head >= handler->buf_tail) {
        len = app->head - handler->buf_tail;
    } else {
        len = CPU_LOG_BUFFER_SIZE - handler->buf_tail + app->head;
    }

    if(len > 0) {
        if(handler->buf_tail + len > CPU_LOG_BUFFER_SIZE) {
            handler->callback(app->buffer + handler->buf_tail, CPU_LOG_BUFFER_SIZE - handler->buf_tail, handler->context);
            len -= CPU_LOG_BUFFER_SIZE - handler->buf_tail;
            handler->buf_tail = 0;
        }
        handler->callback(app->buffer + handler->buf_tail, len, handler->context);
        handler->buf_tail = app->head;
    }
}

static void cpu_log_stream_buffer_callback(FuriEventLoopObject* object, void* context) {
    UNUSED(object);
    CpuLogSrv* app = context;
    UNUSED(app);

    size_t len = 0;
    do {
        uint8_t data[64];
        len = furi_stream_buffer_receive(app->rx_stream, data, sizeof(data), 0);
        if(len > 0) {
            cpu_log_ring_write(app, data, len);
        }
    } while(len > 0);

    CpuLogHandlersList_it_t it;
    for(CpuLogHandlersList_it(it, app->handlers); !CpuLogHandlersList_end_p(it); CpuLogHandlersList_next(it)) {
        CpuLogHandlerInstance* ref = CpuLogHandlersList_ref(it);
        cpu_log_call_handler(app, ref);
    }
}

static void cpu_log_on_irq_cb(FuriHalSerialHandle* handle, FuriHalSerialRxEvent event, void* context) {
    furi_assert(context);
    CpuLogSrv* app = context;

    if(event & (FuriHalSerialRxEventData | FuriHalSerialRxEventIdle)) {
        uint8_t data[64];
        size_t len = furi_hal_serial_rx_data_non_blocking(handle, data, sizeof(data));
        if(len > 0) {
            furi_stream_buffer_send(app->rx_stream, data, len, 0);
        }
    }
}

static void cpu_log_rx_start(CpuLogSrv* app) {
    app->serial_handle = furi_hal_serial_control_acquire(CPU_LOG_UART);
    furi_check(app->serial_handle);

    furi_hal_serial_init(app->serial_handle, CPU_LOG_BAUD_RATE);
    furi_hal_serial_set_config(app->serial_handle, FuriHalSerialConfigDataBits8, FuriHalSerialConfigParityNone, FuriHalSerialConfigStopBits_1);
    furi_hal_serial_set_callback(app->serial_handle, NULL, cpu_log_on_irq_cb, app);

    app->buffer = malloc(CPU_LOG_BUFFER_SIZE);
    app->head = 0;
    app->tail = 0;
    furi_stream_buffer_reset(app->rx_stream);

    furi_hal_serial_async_rx_start(app->serial_handle, true);
}

static void cpu_log_srv_message_handler(FuriEventLoopObject* object, void* context) {
    furi_check(context);
    CpuLogSrv* app = context;
    furi_check(object == app->msg_queue);

    CpuLogSrvMessage message;
    while(furi_message_queue_get(app->msg_queue, &message, 0) == FuriStatusOk) {
        if(message.type == CpuLogSrvMessageTypeSubscribe) {
            CpuLogHandlerInstance* new_handler = CpuLogHandlersList_push_new(app->handlers);
            new_handler->callback = message.handler->callback;
            new_handler->context = message.handler->context;
            new_handler->buf_tail = app->tail;

            cpu_log_call_handler(app, new_handler); // Flush accumulated data immediately
        } else if(message.type == CpuLogSrvMessageTypeUnsubscribe) {
            CpuLogHandlersList_it_t it;
            CpuLogHandlersList_it(it, app->handlers);
            while(!CpuLogHandlersList_end_p(it)) {
                CpuLogHandlerInstance* ref = CpuLogHandlersList_ref(it);
                if(ref->callback == message.handler->callback && ref->context == message.handler->context) {
                    CpuLogHandlersList_remove(app->handlers, it);
                } else {
                    CpuLogHandlersList_next(it);
                }
            }
        } else if(message.type == CpuLogSrvMessageTypeClear) {
            app->tail = app->head;
        } else {
            furi_crash("Unknown message type");
        }
        if(message.lock) {
            api_lock_unlock(message.lock);
        }
    }
}

void cpu_log_add_handler(CpuLogSrv* app, CpuLogHandler* handler) {
    furi_check(app);
    furi_check(handler);
    furi_check(handler->callback);

    CpuLogSrvMessage message = {
        .type = CpuLogSrvMessageTypeSubscribe,
        .handler = handler,
        .lock = api_lock_alloc_locked(),
    };
    furi_check(furi_message_queue_put(app->msg_queue, &message, FuriWaitForever) == FuriStatusOk);

    api_lock_wait_unlock_and_free(message.lock);
}

void cpu_log_remove_handler(CpuLogSrv* app, CpuLogHandler* handler) {
    furi_check(app);
    furi_check(handler);

    CpuLogSrvMessage message = {
        .type = CpuLogSrvMessageTypeUnsubscribe,
        .handler = handler,
        .lock = api_lock_alloc_locked(),
    };
    furi_check(furi_message_queue_put(app->msg_queue, &message, FuriWaitForever) == FuriStatusOk);

    api_lock_wait_unlock_and_free(message.lock);
}

void cpu_log_clear(CpuLogSrv* app) {
    furi_check(app);

    CpuLogSrvMessage message = {
        .type = CpuLogSrvMessageTypeClear,
        .lock = NULL,
    };
    furi_check(furi_message_queue_put(app->msg_queue, &message, FuriWaitForever) == FuriStatusOk);
}

int32_t cpu_log_srv(void* p) {
    UNUSED(p);

    CpuLogSrv* app = malloc(sizeof(CpuLogSrv));
    CpuLogHandlersList_init(app->handlers);

    app->rx_stream = furi_stream_buffer_alloc(CPU_LOG_STREAM_SIZE, 1);
    app->msg_queue = furi_message_queue_alloc(4, sizeof(CpuLogSrvMessage));

    app->event_loop = furi_event_loop_alloc();

    furi_event_loop_subscribe_message_queue(app->event_loop, app->msg_queue, FuriEventLoopEventIn, cpu_log_srv_message_handler, app);
    furi_event_loop_subscribe_stream_buffer(app->event_loop, app->rx_stream, FuriEventLoopEventIn, cpu_log_stream_buffer_callback, app);

    furi_record_create(RECORD_CPU_LOG, app);
    cpu_log_rx_start(app);

    furi_event_loop_run(app->event_loop);

    return 0;
}
