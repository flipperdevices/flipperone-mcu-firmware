#include <furi.h>
#include <containers/pipe.h>
#include <gui/gui.h>
#include <toolbox/hex.h>

#include <cli/cli_ansi.h>

#include <pico/stdio.h>

#define TAG "LoadImageTest"

typedef enum {
    LoadImageTestReadFrameOk,
    LoadImageTestReadFrameAborted, // CTRL+C
    LoadImageTestReadFrameBroken, // pipe/session gone
    LoadImageTestReadFrameBadHex, // a non-hex, non CR/LF character
    LoadImageTestReadFrameWrongLength, // line wasn't exactly frame_size*2 hex chars
} LoadImageTestReadFrameStatus;

// How often to re-push the displayed frame while idle. gui_push_frame() is a
// one-shot: the GUI blits it once on the very next redraw and immediately
// forgets it (gui->pending_frame is cleared as soon as that redraw reads it)
// - falling back to normal Clay compositing on every redraw after that, which
// repaints over it with whatever the desktop (or any other reason a redraw
// fires) would otherwise show. Re-pushing the same buffer periodically keeps
// it winning that race for as long as this command is still running, without
// needing our own persistent View.
#define LOAD_IMAGE_TEST_KEEPALIVE_MS 100

// Chunk size for bulk pipe_receive() calls once data is flowing. A full
// frame's hex text is tens of KB; reading it one getchar() call per byte (as
// this used to) means tens of thousands of individual queue/mutex-guarded
// calls, which dominated transfer time far more than the ~0.5s the bytes
// themselves take on the wire at 1.5 Mbaud. Draining in chunks (like
// rpc_cli.c's read loop) cuts that down by ~2 orders of magnitude.
#define LOAD_IMAGE_TEST_CHUNK_SIZE 512

/**
 * @brief Blocks until a full line (up to, not including, '\n') has arrived,
 * decoding hex digit pairs straight into `frame` (bounded to `frame_size`
 * bytes) as they come in.
 *
 * Decoding incrementally (instead of buffering the whole line, e.g. in a
 * FuriString, and calling hex_string_to_bytes() once at the end) matters here:
 * a full frame's hex text is tens of KB, and an amortized-growth string
 * buffer needs a same-size second allocation transiently on every doubling -
 * on top of the frame buffer already held, that was enough to exhaust the
 * heap (see the "out of memory" crash this replaced).
 */
static LoadImageTestReadFrameStatus
    load_image_test_read_frame(PipeSide* pipe, uint8_t* frame, size_t frame_size, size_t* out_hex_chars) {
    size_t byte_count = 0;
    size_t hex_chars = 0;
    char pending_nibble = 0;
    bool have_pending_nibble = false;
    bool bad_hex = false;
    uint8_t chunk[LOAD_IMAGE_TEST_CHUNK_SIZE];

    while(true) {
        if(pipe_state(pipe) == PipeStateBroken) return LoadImageTestReadFrameBroken;

        size_t avail = pipe_bytes_available(pipe);
        if(avail == 0) {
            furi_delay_ms(10);
            continue;
        }

        // pipe_bytes_available() already reported these bytes as buffered, so
        // this receives without blocking (short of the pipe breaking mid-read).
        size_t to_read = MIN(avail, sizeof(chunk));
        size_t got = pipe_receive(pipe, chunk, to_read);
        if(got < to_read) return LoadImageTestReadFrameBroken;

        for(size_t i = 0; i < got; i++) {
            uint8_t ch = chunk[i];
            if(ch == CliKeyETX) return LoadImageTestReadFrameAborted;
            if(ch == '\n') {
                if(out_hex_chars) *out_hex_chars = hex_chars;
                if(bad_hex) return LoadImageTestReadFrameBadHex;
                if(hex_chars != frame_size * 2) return LoadImageTestReadFrameWrongLength;
                return LoadImageTestReadFrameOk;
            }
            if(ch == '\r') continue;

            hex_chars++;
            if(bad_hex) continue; // this line's already invalid; keep draining to '\n'

            if(!have_pending_nibble) {
                pending_nibble = (char)ch;
                have_pending_nibble = true;
                continue;
            }
            have_pending_nibble = false;

            uint8_t byte;
            if(!hex_char_to_uint8(pending_nibble, (char)ch, &byte)) {
                bad_hex = true;
                continue;
            }
            if(byte_count < frame_size) frame[byte_count] = byte;
            byte_count++;
        }
    }
}

/**
 * @brief Blocks, periodically re-pushing `frame` (see LOAD_IMAGE_TEST_KEEPALIVE_MS
 * and gui_push_frame()'s one-shot nature above) so it stays on screen, until
 * CTRL+C is pressed or the session ends.
 */
static LoadImageTestReadFrameStatus load_image_test_wait_for_stop(PipeSide* pipe, Gui* gui, const uint8_t* frame) {
    uint32_t last_keepalive = furi_get_tick();

    while(true) {
        if(pipe_state(pipe) == PipeStateBroken) return LoadImageTestReadFrameBroken;

        if(furi_get_tick() - last_keepalive >= furi_ms_to_ticks(LOAD_IMAGE_TEST_KEEPALIVE_MS)) {
            gui_push_frame(gui, frame);
            last_keepalive = furi_get_tick();
        }

        if(pipe_bytes_available(pipe) == 0) {
            furi_delay_ms(10);
            continue;
        }

        int ch = getchar();
        if(ch == EOF) return LoadImageTestReadFrameBroken;
        if(ch == CliKeyETX) return LoadImageTestReadFrameAborted;
        // anything else typed while just idling on the loaded image is ignored
    }
}

/**
 * @brief Reads one hex-encoded, already-converted (grayscale, screen-sized)
 * display frame and pushes it straight to the screen, where it then stays
 * (periodically re-pushed - see load_image_test_wait_for_stop()) until CTRL+C
 * is pressed. Loading a different image means restarting this command. All
 * image processing (format decoding, grayscale/level conversion, resizing)
 * happens on the host side (see the companion Python script) - this side
 * only parses hex and blits bytes.
 */
void load_image_test_command_cli(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(context);
    furi_string_trim(args);
    if(furi_string_size(args) > 0) {
        printf("usage: load_image_test\r\n");
        return;
    }

    Gui* gui = furi_record_open(RECORD_GUI);
    size_t width = gui_get_width(gui);
    size_t height = gui_get_height(gui);
    size_t frame_size = width * height;
    size_t expected_hex_chars = frame_size * 2;

    FURI_LOG_I(
        TAG,
        "starting: %zux%zu, frame_size=%zu, heap_free=%zu",
        width,
        height,
        frame_size,
        memmgr_get_free_heap());

    uint8_t* frame = malloc(frame_size);
    furi_check(frame);

    FURI_LOG_I(TAG, "frame buffer allocated, heap_free=%zu", memmgr_get_free_heap());

    // drain any bytes still buffered from the command line itself
    while(pipe_bytes_available(pipe) > 0) getchar();

    printf("Load Image Test\r\n");
    printf(
        "Send a %zux%zu 8-bit grayscale frame as one hex-encoded line (%zu bytes / %zu hex chars).\r\n",
        width,
        height,
        frame_size,
        expected_hex_chars);
    printf("Press CTRL+C to stop.\r\n> ");
    stdio_flush();

    size_t hex_chars = 0;
    uint32_t read_start = furi_get_tick();
    LoadImageTestReadFrameStatus status = load_image_test_read_frame(pipe, frame, frame_size, &hex_chars);
    uint32_t read_ms = furi_get_tick() - read_start;
    printf("\r\n");

    if(status == LoadImageTestReadFrameOk) {
        FURI_LOG_I(TAG, "frame ready in %lu ms, heap_free=%zu", (unsigned long)read_ms, memmgr_get_free_heap());
        gui_push_frame(gui, frame);
        printf(ANSI_FG_GREEN "Displayed. Press CTRL+C to stop." ANSI_RESET "\r\n");

        status = load_image_test_wait_for_stop(pipe, gui, frame);
        FURI_LOG_I(TAG, "stopping: status=%d", (int)status);
    } else if(status == LoadImageTestReadFrameWrongLength) {
        FURI_LOG_W(
            TAG,
            "wrong length after %lu ms: got %zu hex chars, want %zu",
            (unsigned long)read_ms,
            hex_chars,
            expected_hex_chars);
        printf(
            ANSI_FG_RED "Expected %zu hex chars, got %zu" ANSI_RESET "\r\n", expected_hex_chars, hex_chars);
    } else if(status == LoadImageTestReadFrameBadHex) {
        FURI_LOG_W(TAG, "bad hex digit after %lu ms (%zu-char line)", (unsigned long)read_ms, hex_chars);
        printf(ANSI_FG_RED "Invalid hex data" ANSI_RESET "\r\n");
    } else {
        FURI_LOG_I(TAG, "stopping before a frame was loaded: status=%d", (int)status);
    }

    gui_clear_frame(gui);
    furi_record_close(RECORD_GUI);

    free(frame);

    printf("\r\nStopped.\r\n");
}
