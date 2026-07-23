#include "rpc_i.h"
#include <gui/gui.h>
#include <gui/gui_i.h>
#include <furi.h>

#define TAG "RpcScreen"
#define SCREEN_CUSTOM_EVENT_NEW_FRAME (1U << 0)

#define MAX_SCREEN_W 258
#define MAX_SCREEN_H 144
#define SCREEN_PIXELS (MAX_SCREEN_W * MAX_SCREEN_H)

/*
 * Manually-built protobuf wire format for an RpcMessage containing a Frame.
 * Layout (PB_ENCODE_DELIMITED outer prefix + message):
 *
 *   [delim-varint]          outer length
 *   0x0A [varint]           RpcMessage.frame (field 1, wire type 2)
 *     0x08 [varint]         Frame.width    = 258
 *     0x10 [varint]         Frame.height   = 144
 *     0x18 0x00             Frame.encoding = PLAIN
 *     0x20 0x00             Frame.pixel_format = L8
 *     0x2A [varint] [N]     Frame.data
 *
 * Max size ≈ 3 + 1+3 + 1+2 + 1+2 + 1+1 + 1+1 + 1+3 + 37152 = ~37173
 */
#define ENCODE_BUF_SIZE (SCREEN_PIXELS + 128)

typedef struct {
    RpcSession* session;
    Gui* gui;
    volatile bool active;
    uint8_t* frame_buffer;   /* raw pixels only (37152 bytes) */
    uint8_t* encode_buffer;  /* pre-built protobuf bytes */
    size_t width;
    size_t height;
    FuriMutex* mutex;
    FuriEventLoop* event_loop;
    FuriThread* thread;
} RpcScreenStream;

static RpcScreenStream* rpc_screen_stream = NULL;

/* ── Encode a protobuf varint, return bytes written ─────────────────────── */
static inline size_t varint_enc(uint8_t* buf, uint32_t v) {
    size_t n = 0;
    do {
        uint8_t b = v & 0x7F;
        v >>= 7;
        if(v) b |= 0x80;
        buf[n++] = b;
    } while(v);
    return n;
}

/* ── Build raw protobuf bytes for one frame, send them ──────────────────── */
static void rpc_screen_send_frame(RpcScreenStream* stream) {
    uint8_t* buf = stream->encode_buffer;
    size_t pos = 0;

    /* Outer length placeholder (PB_ENCODE_DELIMITED) — 3 bytes max */
    size_t outer_len_off = pos;
    pos += 3;

    /* RpcMessage field 1 = Frame (wire type 2: length-delimited) */
    buf[pos++] = 0x0A;
    size_t frame_len_off = pos;
    pos += 3; /* Frame length placeholder */

    /* Frame.width  = 1 (tag 0x08, varint) */
    buf[pos++] = 0x08; pos += varint_enc(buf + pos, (uint32_t)stream->width);
    /* Frame.height = 2 (tag 0x10, varint) */
    buf[pos++] = 0x10; pos += varint_enc(buf + pos, (uint32_t)stream->height);
    /* Frame.encoding = 3 (tag 0x18, varint) = PLAIN (0) */
    buf[pos++] = 0x18; buf[pos++] = 0x00;
    /* Frame.pixel_format = 4 (tag 0x20, varint) = L8 (0) */
    buf[pos++] = 0x20; buf[pos++] = 0x00;
    /* Frame.data = 5 (tag 0x2A, length-delimited) */
    buf[pos++] = 0x2A;
    size_t data_len = stream->width * stream->height;
    pos += varint_enc(buf + pos, (uint32_t)data_len);

    /* Copy pixel data */
    memcpy(buf + pos, stream->frame_buffer, data_len);
    pos += data_len;

    /* Fill in Frame length (bytes from frame_len_off+3 to pos) */
    size_t frame_len = pos - frame_len_off - 3;
    varint_enc(buf + frame_len_off, (uint32_t)frame_len);

    /* Fill in outer delimiter (bytes from outer_len_off+3 to pos) */
    size_t outer_len = pos - outer_len_off - 3;
    size_t outer_varint = varint_enc(buf + outer_len_off, (uint32_t)outer_len);

    /* Shift everything left if outer varint used fewer than 3 bytes */
    size_t shift = 3 - outer_varint;
    if(shift) {
        memmove(buf + outer_len_off + outer_varint,
                buf + outer_len_off + 3, pos - outer_len_off - 3);
        pos -= shift;
    }

    rpc_send_preencoded(stream->session, buf, pos);
}

/* ── Render callback: runs in event_loop context (screen stream thread) ─── */
static void rpc_screen_render_callback(uint32_t events, void* context) {
    RpcScreenStream* stream = (RpcScreenStream*)context;
    if(!(events & SCREEN_CUSTOM_EVENT_NEW_FRAME)) return;

    if(furi_mutex_acquire(stream->mutex, FuriWaitForever) != FuriStatusOk) return;

    rpc_screen_send_frame(stream);

    furi_mutex_release(stream->mutex);
}

/* ── Framebuffer callback: runs in GUI thread ───────────────────────────── */
static void rpc_screen_fb_callback(
    const uint8_t* data, size_t width, size_t height, void* context) {
    RpcScreenStream* stream = (RpcScreenStream*)context;
    if(!stream->active) return;

    /* Quick copy — non-blocking for the GUI thread */
    if(furi_mutex_acquire(stream->mutex, 16) != FuriStatusOk) return;

    memcpy(stream->frame_buffer, data, width * height);
    stream->width = width;
    stream->height = height;
    furi_mutex_release(stream->mutex);

    /* Signal the event loop to send the frame in its own context */
    furi_event_loop_set_custom_event(stream->event_loop, SCREEN_CUSTOM_EVENT_NEW_FRAME);
}

/* ── Screen stream thread: runs event loop until stopped ────────────────── */
static int32_t rpc_screen_stream_thread(void* context) {
    RpcScreenStream* stream = (RpcScreenStream*)context;

    stream->event_loop = furi_event_loop_alloc();

    furi_event_loop_set_custom_event_callback(
        stream->event_loop, rpc_screen_render_callback, stream);

    gui_add_framebuffer_callback(stream->gui, rpc_screen_fb_callback, stream);
    gui_update(stream->gui);

    FURI_LOG_I(TAG, "Event loop started");
    furi_event_loop_run(stream->event_loop);
    FURI_LOG_I(TAG, "Event loop stopped");

    gui_remove_framebuffer_callback(stream->gui, rpc_screen_fb_callback, stream);
    furi_record_close(RECORD_GUI);
    furi_event_loop_free(stream->event_loop);

    furi_mutex_free(stream->mutex);
    free(stream->frame_buffer);
    free(stream->encode_buffer);
    free(stream);

    return 0;
}

/* ── Handler: start virtual display streaming ───────────────────────────── */
void rpc_start_virtual_display_handler(
    const Flipper_One_Rpc_RpcMessage* message, void* context) {
    UNUSED(message);
    furi_assert(context);
    RpcSession* session = (RpcSession*)context;

    if(rpc_screen_stream) {
        FURI_LOG_W(TAG, "Already active");
        return;
    }

    RpcScreenStream* stream = malloc(sizeof(RpcScreenStream));
    stream->session = session;
    stream->gui = furi_record_open(RECORD_GUI);
    stream->active = true;
    stream->width = 0;
    stream->height = 0;
    stream->frame_buffer = malloc(SCREEN_PIXELS);
    stream->encode_buffer = malloc(ENCODE_BUF_SIZE);
    stream->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    stream->event_loop = NULL;
    stream->thread = NULL;

    rpc_screen_stream = stream;

    stream->thread = furi_thread_alloc_ex(
        "RpcScreenStream", 4096, rpc_screen_stream_thread, stream);
    furi_thread_start(stream->thread);

    FURI_LOG_I(TAG, "Virtual display started");
}

/* ── Handler: stop virtual display streaming ────────────────────────────── */
void rpc_stop_virtual_display_handler(
    const Flipper_One_Rpc_RpcMessage* message, void* context) {
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
    FuriEventLoop* event_loop = stream->event_loop;

    if(event_loop) {
        furi_event_loop_stop(event_loop);
    }
    if(thread) {
        furi_thread_join(thread);
        furi_thread_free(thread);
    }

    rpc_screen_stream = NULL;
    FURI_LOG_I(TAG, "Virtual display stopped");
}

/* ── Handler: close RPC session ─────────────────────────────────────────── */
void rpc_session_close_handler(
    const Flipper_One_Rpc_RpcMessage* message, void* context) {
    UNUSED(message);
    furi_assert(context);
    RpcSession* session = (RpcSession*)context;
    FURI_LOG_I(TAG, "Session close requested");
    rpc_session_close(session);
}
