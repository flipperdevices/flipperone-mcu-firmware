#include "rpc_i.h"

#include <pb.h>
#include <pb_decode.h>
#include <pb_encode.h>

#include <furi.h>
#include <cli/cli_registry.h>
#include <cli/cli_commands.h>
#include <m-dict.h>

#define TAG "RpcSrv"

typedef enum {
    RpcEvtNewData = (1 << 0),
    RpcEvtDisconnect = (1 << 1),
} RpcEvtFlags;

#define RPC_ALL_EVENTS (RpcEvtNewData | RpcEvtDisconnect)

DICT_DEF2(RpcHandlerDict, pb_size_t, M_DEFAULT_OPLIST, RpcHandler, M_POD_OPLIST)

struct RpcSession {
    Rpc* rpc;
    FuriThread* thread;
    RpcHandlerDict_t handlers;
    FuriStreamBuffer* stream;
    Flipper_One_Rpc_RpcMessage* decoded_message;
    bool terminate;
    FuriMutex* callbacks_mutex;
    RpcSendBytesCallback send_bytes_callback;
    RpcSessionTerminatedCallback terminated_callback;
    RpcOwner owner;
    void* context;
};

struct Rpc {
    FuriMutex* busy_mutex;
};

void rpc_session_set_context(RpcSession* session, void* context) {
    furi_check(session);
    furi_mutex_acquire(session->callbacks_mutex, FuriWaitForever);
    session->context = context;
    furi_mutex_release(session->callbacks_mutex);
}

void rpc_session_set_send_bytes_callback(RpcSession* session, RpcSendBytesCallback callback) {
    furi_check(session);
    furi_mutex_acquire(session->callbacks_mutex, FuriWaitForever);
    session->send_bytes_callback = callback;
    furi_mutex_release(session->callbacks_mutex);
}

void rpc_session_set_terminated_callback(RpcSession* session, RpcSessionTerminatedCallback callback) {
    furi_check(session);
    furi_mutex_acquire(session->callbacks_mutex, FuriWaitForever);
    session->terminated_callback = callback;
    furi_mutex_release(session->callbacks_mutex);
}

size_t rpc_session_feed(RpcSession* session, const uint8_t* encoded_bytes, size_t size, uint32_t timeout) {
    furi_check(session);
    furi_check(encoded_bytes);
    if(!size) return 0;

    /* Worker may have already exited — do not touch a dead task. */
    if(session->terminate) return 0;

    FURI_LOG_D(TAG, "feed(%zu) timeout=%lu", size, timeout);
    size_t bytes_sent = furi_stream_buffer_send(session->stream, encoded_bytes, size, timeout);
    if(bytes_sent > 0) {
        FURI_LOG_D(TAG, "feed sent=%zu, setting flag", bytes_sent);
        furi_thread_flags_set(furi_thread_get_id(session->thread), RpcEvtNewData);
    }
    return bytes_sent;
}

static bool rpc_pb_stream_read(pb_istream_t* istream, pb_byte_t* buf, size_t count) {
    furi_assert(istream);
    furi_assert(buf);
    RpcSession* session = (RpcSession*)istream->state;
    furi_assert(session);

    if(session->terminate) return false;

    size_t bytes_received = 0;
    uint32_t flags = 0;
    int disconnect_retries = 3; /* drain at most 3 chunks before giving up */

    while(1) {
        bytes_received += furi_stream_buffer_receive(session->stream, buf + bytes_received, count - bytes_received, 0);

        if(bytes_received == count) break;

        flags = furi_thread_flags_wait(RPC_ALL_EVENTS, FuriFlagWaitAny, FuriWaitForever);
        if(flags & RpcEvtDisconnect) {
            if(furi_stream_buffer_is_empty(session->stream) || --disconnect_retries <= 0) {
                session->terminate = true;
                istream->bytes_left = 0;
                bytes_received = 0;
                break;
            }
            furi_thread_flags_set(furi_thread_get_id(session->thread), RpcEvtDisconnect);
        }
        if(flags & RpcEvtNewData) {
            // Just wake thread up
        }
    }
#ifdef SRV_RPC_DEBUG
    rpc_debug_print_data("INPUT", buf, bytes_received);
#endif
    return count == bytes_received;
}

static int32_t rpc_session_worker(void* context) {
    furi_assert(context);
    RpcSession* session = (RpcSession*)context;
    Rpc* rpc = session->rpc;

    FURI_LOG_D(TAG, "Session started");

    while(1) {
        pb_istream_t istream = {
            .callback = rpc_pb_stream_read,
            .state = session,
            .errmsg = NULL,
            .bytes_left = SIZE_MAX,
        };

        memset(session->decoded_message, 0, sizeof(*session->decoded_message));
        bool decode_ok = pb_decode_ex(&istream, &Flipper_One_Rpc_RpcMessage_msg, session->decoded_message, PB_ENCODE_DELIMITED);

        if(decode_ok) {
            pb_size_t tag = session->decoded_message->which_content;

#ifdef SRV_RPC_DEBUG
            FURI_LOG_I(TAG, "INPUT:");
            rpc_debug_print_message(session->decoded_message);
#endif
            RpcHandler* handler = RpcHandlerDict_get(session->handlers, tag);

            if(handler && handler->message_handler) {
                furi_check(furi_mutex_acquire(rpc->busy_mutex, FuriWaitForever) == FuriStatusOk);
                handler->message_handler(session->decoded_message, handler->context);
                furi_check(furi_mutex_release(rpc->busy_mutex) == FuriStatusOk);
            } else if(tag == 0) {
                FURI_LOG_E(TAG, "Received empty message (tag=0)");
                session->terminate = true;
            } else if(!handler) {
                FURI_LOG_E(TAG, "No handler for message tag %d", tag);
            }
        } else {
            FURI_LOG_E(TAG, "Decode failed: %.128s", PB_GET_ERROR(&istream));
            furi_stream_buffer_reset(session->stream);
            session->terminate = true;
        }

        pb_release(&Flipper_One_Rpc_RpcMessage_msg, session->decoded_message);

        if(session->terminate) {
            FURI_LOG_D(TAG, "Session terminated");
            /* Reset stream so pending rpc_session_feed calls return
             * immediately instead of blocking with timeout (which would
             * stall the CLI pipe-drain loop and deadlock pipe_send). */
            furi_stream_buffer_reset(session->stream);
            break;
        }
    }
    return 0;
}

static void rpc_session_thread_pending_callback(void* context, uint32_t arg) {
    UNUSED(arg);
    RpcSession* session = (RpcSession*)context;

    FURI_LOG_D(TAG, "Pending callback");

    free(session->decoded_message);
    RpcHandlerDict_clear(session->handlers);
    furi_stream_buffer_free(session->stream);

    furi_mutex_acquire(session->callbacks_mutex, FuriWaitForever);
    if(session->terminated_callback) {
        session->terminated_callback(session->context);
    }
    furi_mutex_release(session->callbacks_mutex);

    furi_mutex_free(session->callbacks_mutex);
    furi_thread_join(session->thread);
    furi_thread_free(session->thread);
    free(session);
}

static void rpc_session_thread_state_callback(FuriThread* thread, FuriThreadState state, void* context) {
    UNUSED(thread);
    if(state == FuriThreadStateStopped) {
        furi_timer_pending_callback(rpc_session_thread_pending_callback, context, 0);
    }
}

RpcSession* rpc_session_open(Rpc* rpc, RpcOwner owner) {
    furi_check(rpc);

    RpcSession* session = malloc(sizeof(RpcSession));
    session->callbacks_mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    session->stream = furi_stream_buffer_alloc(RPC_BUFFER_SIZE, 1);
    session->rpc = rpc;
    session->terminate = false;
    session->owner = owner;
    RpcHandlerDict_init(session->handlers);
    session->decoded_message = malloc(sizeof(Flipper_One_Rpc_RpcMessage));
    memset(session->decoded_message, 0, sizeof(Flipper_One_Rpc_RpcMessage));

    session->thread = furi_thread_alloc_ex("RpcSessionWorker", 3072, rpc_session_worker, session);
    furi_thread_set_state_context(session->thread, session);
    furi_thread_set_state_callback(session->thread, rpc_session_thread_state_callback);
    furi_thread_start(session->thread);

    return session;
}

void rpc_session_close(RpcSession* session) {
    furi_check(session);
    furi_check(session->rpc);
    furi_thread_flags_set(furi_thread_get_id(session->thread), RpcEvtDisconnect);
}

void rpc_on_system_start(void* p) {
    UNUSED(p);
    Rpc* rpc = malloc(sizeof(Rpc));
    rpc->busy_mutex = furi_mutex_alloc(FuriMutexTypeNormal);

    CliRegistry* registry = furi_record_open(RECORD_CLI);
    cli_registry_add_command(registry, "rpc", CliCommandFlagParallelSafe, rpc_cli_command_start_session, rpc);
    furi_record_close(RECORD_CLI);

    furi_record_create(RECORD_RPC, rpc);
}

void rpc_add_handler(RpcSession* session, pb_size_t message_tag, RpcHandler* handler) {
    furi_assert(RpcHandlerDict_get(session->handlers, message_tag) == NULL);
    RpcHandlerDict_set_at(session->handlers, message_tag, *handler);
}

void rpc_send(RpcSession* session, Flipper_One_Rpc_RpcMessage* message) {
    furi_assert(session);
    furi_assert(message);

    pb_ostream_t ostream = PB_OSTREAM_SIZING;

#ifdef SRV_RPC_DEBUG
    FURI_LOG_I(TAG, "OUTPUT:");
    rpc_debug_print_message(message);
#endif

    bool result = pb_encode_ex(&ostream, &Flipper_One_Rpc_RpcMessage_msg, message, PB_ENCODE_DELIMITED);
    furi_check(result && ostream.bytes_written);

    uint8_t* buffer = malloc(ostream.bytes_written);
    ostream = pb_ostream_from_buffer(buffer, ostream.bytes_written);
    pb_encode_ex(&ostream, &Flipper_One_Rpc_RpcMessage_msg, message, PB_ENCODE_DELIMITED);

#ifdef SRV_RPC_DEBUG
    rpc_debug_print_data("OUTPUT", buffer, ostream.bytes_written);
#endif

    /* Snapshot callback under mutex, then release — the callback may block
     * (e.g. pipe_send) and we must not hold callbacks_mutex across it. */
    RpcSendBytesCallback cb;
    void* ctx;
    furi_mutex_acquire(session->callbacks_mutex, FuriWaitForever);
    cb = session->send_bytes_callback;
    ctx = session->context;
    furi_mutex_release(session->callbacks_mutex);

    if(cb) {
        cb(ctx, buffer, ostream.bytes_written);
    }

    free(buffer);
}

void rpc_send_and_release(RpcSession* session, Flipper_One_Rpc_RpcMessage* message) {
    rpc_send(session, message);
    pb_release(&Flipper_One_Rpc_RpcMessage_msg, message);
}

void rpc_send_preencoded(RpcSession* session, const uint8_t* bytes, size_t len) {
    furi_assert(session);
    furi_assert(bytes);
    furi_assert(len > 0);

    /* Snapshot the callback under the mutex, then release it BEFORE
     * calling the callback — the callback (e.g. pipe_send) may block,
     * and holding callbacks_mutex across a blocking call would deadlock
     * the session termination path (rpc_session_thread_pending_callback). */
    RpcSendBytesCallback cb;
    void* ctx;
    furi_mutex_acquire(session->callbacks_mutex, FuriWaitForever);
    cb = session->send_bytes_callback;
    ctx = session->context;
    furi_mutex_release(session->callbacks_mutex);

    if(cb) {
        cb(ctx, (uint8_t*)bytes, len);
    }
}
