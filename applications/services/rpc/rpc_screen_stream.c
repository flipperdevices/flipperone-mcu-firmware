#include "rpc_i.h"
#include <gui/gui.h>
#include <gui/gui_i.h>
#include <furi.h>

#define TAG "RpcScreen"

typedef enum {
    RpcScreenEventTypeNewFrame = (1 << 0),
    RpcScreenEventTypeStop = (1 << 1),
    RpcScreenEventTypeAll = (RpcScreenEventTypeNewFrame | RpcScreenEventTypeStop),
} RpcScreenEventType;

#define MAX_SCREEN_W  258
#define MAX_SCREEN_H  144
#define SCREEN_PIXELS (MAX_SCREEN_W * MAX_SCREEN_H)

typedef struct {
    RpcSession* session;
    Gui* gui;
    volatile bool active;
    uint8_t* frame_buffer;  /* pb_size_t prefix + pixel data */
    uint8_t* encode_buffer; /* pre-allocated for pb_encode_ex (no malloc!) */
    size_t width;
    size_t height;
    FuriMutex* mutex;
    FuriThread* thread;
} RpcScreenStream;

static RpcScreenStream* rpc_screen_stream = NULL;

/* ── Build and send one frame using standard nanopb encoding ───────────── */
static void rpc_screen_send_frame(RpcScreenStream* stream) {
    Flipper_One_Rpc_RpcMessage msg = Flipper_One_Rpc_RpcMessage_init_zero;
    msg.which_content = Flipper_One_Rpc_RpcMessage_frame_tag;

    Flipper_One_Frame_Frame* frame = &msg.content.frame;
    frame->width = (uint32_t)stream->width;
    frame->height = (uint32_t)stream->height;
    frame->encoding = Flipper_One_Frame_Encoding_PLAIN;
    frame->pixel_format = Flipper_One_Frame_PixelFormat_L8;

    /* Point data to our pre-allocated frame_buffer */
    pb_bytes_array_t* arr = (pb_bytes_array_t*)stream->frame_buffer;
    arr->size = (pb_size_t)(stream->width * stream->height);
    frame->data = arr;

    /* Encode into pre-allocated buffer — no malloc */
    pb_ostream_t ostream = pb_ostream_from_buffer(
        stream->encode_buffer, Flipper_One_Rpc_RpcMessage_size);
    bool ok = pb_encode_ex(
        &ostream, &Flipper_One_Rpc_RpcMessage_msg, &msg, PB_ENCODE_DELIMITED);
    if(!ok) {
        FURI_LOG_E(TAG, "Encode failed");
        frame->data = NULL;
        pb_release(&Flipper_One_Rpc_RpcMessage_msg, &msg);
        return;
    }

    rpc_send_preencoded(stream->session, stream->encode_buffer, ostream.bytes_written);

    frame->data = NULL;
    pb_release(&Flipper_One_Rpc_RpcMessage_msg, &msg);
}

/* ── Framebuffer callback: runs in GUI thread ───────────────────────────── */
static void rpc_screen_fb_callback(const uint8_t* data, size_t width, size_t height, void* context) {
    RpcScreenStream* stream = (RpcScreenStream*)context;
    if(!stream->active) return;

    /* Quick copy — non-blocking for the GUI thread */
    if(furi_mutex_acquire(stream->mutex, 0) != FuriStatusOk) return;

    memcpy(stream->frame_buffer + sizeof(pb_size_t), data, width * height);
    stream->width = width;
    stream->height = height;
    furi_mutex_release(stream->mutex);

    /* Signal the stream thread to send the frame */
    furi_thread_flags_set(furi_thread_get_id(stream->thread), RpcScreenEventTypeNewFrame);
}

/* ── Screen stream thread: runs event loop until stopped ────────────────── */
static int32_t rpc_screen_stream_thread(void* context) {
    RpcScreenStream* stream = (RpcScreenStream*)context;

    stream->gui = furi_record_open(RECORD_GUI);
    gui_add_framebuffer_callback(stream->gui, rpc_screen_fb_callback, stream);
    gui_update(stream->gui);

    FURI_LOG_I(TAG, "Screen stream started");

    while(true) {
        uint32_t flags = furi_thread_flags_wait(RpcScreenEventTypeAll, FuriFlagWaitAny, FuriWaitForever);
        if(flags & RpcScreenEventTypeNewFrame) {
            if(furi_mutex_acquire(stream->mutex, FuriWaitForever) == FuriStatusOk) {
                rpc_screen_send_frame(stream);
                furi_mutex_release(stream->mutex);
            }
        }
        if(flags & RpcScreenEventTypeStop) {
            break;
        }
    }

    FURI_LOG_I(TAG, "Screen stream stopped");

    gui_remove_framebuffer_callback(stream->gui, rpc_screen_fb_callback, stream);
    furi_record_close(RECORD_GUI);

    return 0;
}

/* ── Handler: start virtual display streaming ───────────────────────────── */
void rpc_start_virtual_display_handler(const Flipper_One_Rpc_RpcMessage* message, void* context) {
    UNUSED(message);
    furi_assert(context);
    RpcSession* session = (RpcSession*)context;

    if(rpc_screen_stream) {
        FURI_LOG_W(TAG, "Already active");
        return;
    }

    /* Single allocation: struct + frame_buf + encode_buf */
    size_t alloc_size = sizeof(RpcScreenStream)
        + sizeof(pb_size_t) + SCREEN_PIXELS
        + Flipper_One_Rpc_RpcMessage_size;
    uint8_t* mem_block = malloc(alloc_size);
    RpcScreenStream* stream = (RpcScreenStream*)mem_block;
    stream->frame_buffer = mem_block + sizeof(RpcScreenStream);
    stream->encode_buffer = stream->frame_buffer + sizeof(pb_size_t) + SCREEN_PIXELS;

    stream->session = session;
    stream->active = true;
    stream->width = 0;
    stream->height = 0;
    stream->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    stream->thread = NULL;

    rpc_screen_stream = stream;

    stream->thread = furi_thread_alloc_ex("RpcScreenStream", 4096, rpc_screen_stream_thread, stream);
    furi_thread_start(stream->thread);

    FURI_LOG_I(TAG, "Virtual display started");
}

/* ── Handler: stop virtual display streaming ────────────────────────────── */
void rpc_stop_virtual_display_handler(const Flipper_One_Rpc_RpcMessage* message, void* context) {
    UNUSED(message);
    UNUSED(context);

    RpcScreenStream* stream = rpc_screen_stream;
    if(!stream || !stream->active) {
        FURI_LOG_W(TAG, "Not active");
        return;
    }

    stream->active = false;

    /* Save thread handle before join — the thread frees stream on exit */
    FuriThread* thread = stream->thread;

    if(thread) {
        furi_thread_flags_set(furi_thread_get_id(thread), RpcScreenEventTypeStop);
        furi_thread_join(thread);
        furi_thread_free(thread);
    }

    furi_mutex_free(stream->mutex);
    free(stream); /* single block frees frame_buffer + encode_buffer too */
    rpc_screen_stream = NULL;
    FURI_LOG_I(TAG, "Virtual display stopped");
}

/* ── Handler: close RPC session ─────────────────────────────────────────── */
void rpc_session_close_handler(const Flipper_One_Rpc_RpcMessage* message, void* context) {
    UNUSED(message);
    furi_assert(context);
    RpcSession* session = (RpcSession*)context;
    FURI_LOG_I(TAG, "Session close requested");
    rpc_session_close(session);
}
