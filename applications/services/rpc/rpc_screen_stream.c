#include "rpc_i.h"
#include <gui/gui.h>
#include <gui/gui_i.h>
#include <furi.h>
#include <pb_encode.h>
#include <drivers/display/display_jd9853_reg.h>

#define TAG "RpcScreen"

/* ── Protobuf version sentinel ──────────────────────────────────────────── */
_Static_assert(
    PB_PROTO_HEADER_VERSION == 40,
    "nanopb version changed — wire header may be incompatible; "
    "regenerate *.pb.h and review rpc_frame_build_wire_header()");

/* Expected wire header size for JD9853_WIDTH×JD9853_HEIGHT, fields 1-5 */
#define RPC_EXPECTED_WIRE_HEADER_SIZE 21

typedef enum {
    RpcScreenEventTypeNewFrame = (1 << 0),
    RpcScreenEventTypeStop = (1 << 1),
    RpcScreenEventTypeAll = (RpcScreenEventTypeNewFrame | RpcScreenEventTypeStop),
} RpcScreenEventType;

#define SCREEN_PIXELS (JD9853_WIDTH * JD9853_HEIGHT)

typedef struct {
    RpcSession* session;
    Gui* gui;
    volatile bool active;
    uint8_t* frame_buffer; /* wire header + pixel data */
    size_t wire_header_size; /* set once at startup by build_wire_header() */
    size_t width;
    size_t height;
    FuriMutex* mutex;
    FuriThread* thread;
    FuriSemaphore* done_sem; /* released when thread exits (replaces join) */
} RpcScreenStream;

static RpcScreenStream* rpc_screen_stream = NULL;

/*
 * Build the protobuf wire-format header for a frame message.
 * Called ONCE at startup — uses live nanopb field tags from
 * generated *.pb.h, so it survives .proto renumbering.
 *
 * Frame_buffer layout after build:
 *   [ header N bytes ] [ pixel data (w*h) bytes ]
 *
 * Returns the number of header bytes written.
 */
static size_t rpc_frame_build_wire_header(uint8_t* buf, size_t buf_size, uint16_t width, uint16_t height) {
    size_t data_len = (size_t)width * height;

    /* ── Pass 1: measure all varint sizes ────────────────────────── */
    uint8_t dummy[128];
    pb_ostream_t sizing = pb_ostream_from_buffer(dummy, sizeof(dummy));

    /* Inner Frame message header (everything before data bytes) */
    sizing.bytes_written = 0;
    pb_encode_tag(&sizing, PB_WT_VARINT, Flipper_One_Frame_Frame_width_tag);
    pb_encode_varint(&sizing, width);
    pb_encode_tag(&sizing, PB_WT_VARINT, Flipper_One_Frame_Frame_height_tag);
    pb_encode_varint(&sizing, height);
    pb_encode_tag(&sizing, PB_WT_VARINT, Flipper_One_Frame_Frame_encoding_tag);
    pb_encode_varint(&sizing, Flipper_One_Frame_Encoding_PLAIN);
    pb_encode_tag(&sizing, PB_WT_VARINT, Flipper_One_Frame_Frame_pixel_format_tag);
    pb_encode_varint(&sizing, Flipper_One_Frame_PixelFormat_L8);
    pb_encode_tag(&sizing, PB_WT_STRING, Flipper_One_Frame_Frame_data_tag);
    pb_encode_varint(&sizing, data_len);

    size_t frame_inner_header = sizing.bytes_written;
    size_t frame_total = frame_inner_header + data_len;

    /* Outer RpcMessage header */
    sizing.bytes_written = 0;
    pb_encode_tag(&sizing, PB_WT_STRING, Flipper_One_Rpc_RpcMessage_frame_tag);
    pb_encode_varint(&sizing, frame_total);

    size_t rpc_header = sizing.bytes_written;
    size_t rpc_total = rpc_header + frame_total;

    /* PB_ENCODE_DELIMITED prefix */
    sizing.bytes_written = 0;
    pb_encode_varint(&sizing, rpc_total);
    size_t delimited = sizing.bytes_written;

    /* ── Pass 2: write to the actual buffer ──────────────────────── */
    pb_ostream_t stream = pb_ostream_from_buffer(buf, buf_size);

    pb_encode_varint(&stream, rpc_total); /* delimited length */
    pb_encode_tag(&stream, PB_WT_STRING, Flipper_One_Rpc_RpcMessage_frame_tag);
    pb_encode_varint(&stream, frame_total);

    pb_encode_tag(&stream, PB_WT_VARINT, Flipper_One_Frame_Frame_width_tag);
    pb_encode_varint(&stream, width);
    pb_encode_tag(&stream, PB_WT_VARINT, Flipper_One_Frame_Frame_height_tag);
    pb_encode_varint(&stream, height);
    pb_encode_tag(&stream, PB_WT_VARINT, Flipper_One_Frame_Frame_encoding_tag);
    pb_encode_varint(&stream, Flipper_One_Frame_Encoding_PLAIN);
    pb_encode_tag(&stream, PB_WT_VARINT, Flipper_One_Frame_Frame_pixel_format_tag);
    pb_encode_varint(&stream, Flipper_One_Frame_PixelFormat_L8);
    pb_encode_tag(&stream, PB_WT_STRING, Flipper_One_Frame_Frame_data_tag);
    pb_encode_varint(&stream, data_len);

    return stream.bytes_written;
}

/* ── Send one frame (header + pixels already in frame_buffer) ───────────── */
static void rpc_screen_send_frame(RpcScreenStream* stream) {
    RpcSession* session = stream->session;
    if(!stream->active || !session) return;
    rpc_send_preencoded(session, stream->frame_buffer, stream->wire_header_size + SCREEN_PIXELS);
}

/* ── Framebuffer callback: runs in GUI thread ───────────────────────────── */
static void rpc_screen_fb_callback(const uint8_t* data, size_t width, size_t height, void* context) {
    RpcScreenStream* stream = (RpcScreenStream*)context;
    if(!stream->active) return;

    /* Quick copy — non-blocking for the GUI thread */
    if(furi_mutex_acquire(stream->mutex, 0) != FuriStatusOk) return;

    memcpy(stream->frame_buffer + stream->wire_header_size, data, width * height);
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

    /* Signal the stop handler that we are done (replaces furi_thread_join) */
    furi_semaphore_release(stream->done_sem);

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

    /* Single allocation: struct + wire buffer (header + pixels) */

    /* Build header once into a stack buffer to measure its size */
    uint8_t header_tmp[128];
    size_t header_size = rpc_frame_build_wire_header(header_tmp, sizeof(header_tmp), JD9853_WIDTH, JD9853_HEIGHT);

    if(header_size != RPC_EXPECTED_WIRE_HEADER_SIZE) {
        FURI_LOG_W(
            TAG,
            "Wire header size changed: expected %u, got %u — "
            "check .proto field numbering or nanopb version",
            (unsigned)RPC_EXPECTED_WIRE_HEADER_SIZE,
            (unsigned)header_size);
    }

    size_t alloc_size = sizeof(RpcScreenStream) + header_size + SCREEN_PIXELS;
    uint8_t* mem_block = malloc(alloc_size);
    RpcScreenStream* stream = (RpcScreenStream*)mem_block;
    stream->frame_buffer = mem_block + sizeof(RpcScreenStream);
    stream->wire_header_size = header_size;

    /* Copy the pre-built header into the allocated buffer */
    memcpy(stream->frame_buffer, header_tmp, header_size);

    stream->session = session;
    stream->active = true;
    stream->width = 0;
    stream->height = 0;
    stream->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    stream->thread = NULL;
    stream->done_sem = furi_semaphore_alloc(1, 0);

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
    if(!stream) {
        return;
    }

    stream->active = false;

    /* Save thread handle before join — the thread frees stream on exit */
    FuriThread* thread = stream->thread;

    if(thread) {
        furi_thread_flags_set(furi_thread_get_id(thread), RpcScreenEventTypeStop);
        /* Wait for the thread to signal completion via semaphore
         * (avoids furi_thread_join deadlock with timer daemon) */
        furi_semaphore_acquire(stream->done_sem, FuriWaitForever);
        furi_thread_join(thread);
        furi_thread_free(thread);
    }

    furi_semaphore_free(stream->done_sem);

    furi_mutex_free(stream->mutex);
    free(stream); /* single block frees frame_buffer too */
    rpc_screen_stream = NULL;
    FURI_LOG_I(TAG, "Virtual display stopped");
}

/* ── Handler: close RPC session ─────────────────────────────────────────── */
void rpc_session_close_handler(const Flipper_One_Rpc_RpcMessage* message, void* context) {
    UNUSED(message);
    furi_assert(context);
    RpcSession* session = (RpcSession*)context;
    FURI_LOG_I(TAG, "Session close requested");

    /* Signal the screen stream to stop without blocking.
     * active + session writes are independent — no mutex needed here;
     * rpc_screen_send_frame checks both before calling pipe_send. */
    if(rpc_screen_stream) {
        rpc_screen_stream->active = false;
        rpc_screen_stream->session = NULL;
        furi_thread_flags_set(
            furi_thread_get_id(rpc_screen_stream->thread),
            RpcScreenEventTypeStop);
    }

    rpc_session_close(session);
}
