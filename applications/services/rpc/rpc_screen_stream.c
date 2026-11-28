#include "rpc_i.h"
#include <gui/gui.h>
#include <gui/gui_i.h>
#include <furi.h>
#include <pb_encode.h>
#include <drivers/display/display_jd9853_reg.h>
#include <toolbox/rle_encode.h>

#define TAG "RpcScreen"

/* ── Protobuf version sentinel ──────────────────────────────────────────── */
_Static_assert(
    PB_PROTO_HEADER_VERSION == 40,
    "nanopb version changed — wire header may be incompatible; "
    "regenerate *.pb.h and review rpc_frame_build_wire_header()");

/* Room before the pixel data for the wire header, which depends on the encoded size */
#define RPC_WIRE_HEADER_MAX_SIZE 32

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
    uint8_t* frame_buffer; /* room for the wire header, then the encoded pixels */
    size_t data_size; /* encoded pixel bytes after the header room */
    Flipper_One_Frame_Encoding encoding;
    volatile bool redraw_missed; /* a redraw came while a frame was being sent */
    FuriMutex* mutex;
    FuriThread* thread;
    FuriSemaphore* done_sem; /* released when thread exits (replaces join) */
} RpcScreenStream;

static RpcScreenStream* rpc_screen_stream = NULL;

/*
 * Build the protobuf wire-format header for a frame message, per frame: the
 * encoding and the data size change. Uses live nanopb field tags from
 * generated *.pb.h, so it survives .proto renumbering.
 *
 * Returns the number of header bytes written.
 */
static size_t rpc_frame_build_wire_header(
    uint8_t* buf,
    size_t buf_size,
    uint16_t width,
    uint16_t height,
    Flipper_One_Frame_Encoding encoding,
    size_t data_len) {

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
    pb_encode_varint(&sizing, encoding);
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
    pb_encode_varint(&stream, encoding);
    pb_encode_tag(&stream, PB_WT_VARINT, Flipper_One_Frame_Frame_pixel_format_tag);
    pb_encode_varint(&stream, Flipper_One_Frame_PixelFormat_L8);
    pb_encode_tag(&stream, PB_WT_STRING, Flipper_One_Frame_Frame_data_tag);
    pb_encode_varint(&stream, data_len);

    return stream.bytes_written;
}

/* ── Send one frame: header right before the encoded pixels ─────────────── */
static void rpc_screen_send_frame(RpcScreenStream* stream) {
    RpcSession* session = stream->session;
    if(!stream->active || !session) return;

    uint8_t header[RPC_WIRE_HEADER_MAX_SIZE];
    size_t header_size = rpc_frame_build_wire_header(
        header, sizeof(header), JD9853_WIDTH, JD9853_HEIGHT, stream->encoding, stream->data_size);
    furi_check(header_size <= RPC_WIRE_HEADER_MAX_SIZE);

    uint8_t* message = stream->frame_buffer + RPC_WIRE_HEADER_MAX_SIZE - header_size;
    memcpy(message, header, header_size);
    rpc_send_preencoded(session, message, header_size + stream->data_size);
}

/* ── Framebuffer callback: runs in GUI thread ───────────────────────────── */
static void rpc_screen_fb_callback(const uint8_t* data, size_t width, size_t height, void* context) {
    RpcScreenStream* stream = (RpcScreenStream*)context;
    if(!stream->active) return;

    /* Non-blocking for the GUI thread: while a frame is being sent, remember
     * to ask for a redraw afterwards, so the last frame is never lost */
    if(furi_mutex_acquire(stream->mutex, 0) != FuriStatusOk) {
        stream->redraw_missed = true;
        return;
    }

    const size_t size = width * height;
    furi_check(size == SCREEN_PIXELS);
    uint8_t* pixels = stream->frame_buffer + RPC_WIRE_HEADER_MAX_SIZE;
    /* RLE unless it comes out no smaller than the frame itself */
    if(rle_compress(data, size, pixels, size, 1, &stream->data_size)) {
        stream->encoding = Flipper_One_Frame_Encoding_RUN_LENGTH;
    } else {
        memcpy(pixels, data, size);
        stream->data_size = size;
        stream->encoding = Flipper_One_Frame_Encoding_PLAIN;
    }
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
#ifdef SRV_RPC_DEBUG
    FURI_LOG_I(TAG, "Screen stream started");
#endif 
    while(true) {
        uint32_t flags = furi_thread_flags_wait(RpcScreenEventTypeAll, FuriFlagWaitAny, FuriWaitForever);
        if(flags & RpcScreenEventTypeNewFrame) {
            if(furi_mutex_acquire(stream->mutex, FuriWaitForever) == FuriStatusOk) {
                rpc_screen_send_frame(stream);
                furi_mutex_release(stream->mutex);
            }
            if(stream->redraw_missed) {
                stream->redraw_missed = false;
                gui_update(stream->gui);
            }
        }
        if(flags & RpcScreenEventTypeStop) {
            break;
        }
    }
#ifdef SRV_RPC_DEBUG
    FURI_LOG_I(TAG, "Screen stream stopped");
#endif

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

    /* Single allocation: struct + wire buffer (header room + pixels) */
    size_t alloc_size = sizeof(RpcScreenStream) + RPC_WIRE_HEADER_MAX_SIZE + SCREEN_PIXELS;
    uint8_t* mem_block = malloc(alloc_size);
    RpcScreenStream* stream = (RpcScreenStream*)mem_block;
    stream->frame_buffer = mem_block + sizeof(RpcScreenStream);
    stream->data_size = 0;
    stream->encoding = Flipper_One_Frame_Encoding_PLAIN;
    stream->redraw_missed = false;

    stream->session = session;
    stream->active = true;
    stream->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    stream->thread = NULL;
    stream->done_sem = furi_semaphore_alloc(1, 0);

    stream->thread = furi_thread_alloc_ex("RpcScreenStream", 4096, rpc_screen_stream_thread, stream);
    furi_thread_start(stream->thread);

    /* Publish only after thread is running — stop handler needs thread != NULL. */
    rpc_screen_stream = stream;
#ifdef SRV_RPC_DEBUG
    FURI_LOG_I(TAG, "Virtual display started");
#endif
}

/* ── Handler: stop virtual display streaming ────────────────────────────── */
void rpc_stop_virtual_display_handler(const Flipper_One_Rpc_RpcMessage* message, void* context) {
    UNUSED(message);
    UNUSED(context);

    /* Atomic swap — prevents double-free if called concurrently
     * (e.g. from worker + terminated callback). */
    RpcScreenStream* stream = rpc_screen_stream;
    rpc_screen_stream = NULL;
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
#ifdef SRV_RPC_DEBUG
    FURI_LOG_I(TAG, "Virtual display stopped");
#endif
}

/* ── Handler: close RPC session ─────────────────────────────────────────── */
void rpc_session_close_handler(const Flipper_One_Rpc_RpcMessage* message, void* context) {
    UNUSED(message);
    furi_assert(context);
    RpcSession* session = (RpcSession*)context;
#ifdef SRV_RPC_DEBUG
    FURI_LOG_I(TAG, "Session close requested");
#endif
    /* Stop screen stream and notify CLI loop — the actual
     * rpc_session_close() is called once after the pipe loop exits. */
    rpc_stop_virtual_display_handler(NULL, NULL);
    rpc_session_trigger_close_callback(session);
}
