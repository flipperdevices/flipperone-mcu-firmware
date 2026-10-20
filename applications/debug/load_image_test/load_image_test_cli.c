#include <furi.h>
#include <containers/pipe.h>
#include <gui/gui.h>
#include <toolbox/hex.h>

#include <cli/cli_ansi.h>
#include <cli/cli_status.h>

#include <pico/stdio.h>

#define TAG "LoadImageTest"

typedef enum {
    LoadImageTestReadFrameOk,
    LoadImageTestReadFrameAborted, // CTRL+C
    LoadImageTestReadFrameBroken, // pipe/session gone
    LoadImageTestReadFrameBadHex, // a non-hex, non CR/LF character
    LoadImageTestReadFrameWrongLength, // line wasn't exactly frame_size*2 hex chars
    LoadImageTestReadFrameTimeout, // no bytes arrived for LOAD_IMAGE_TEST_STALL_TIMEOUT_MS
} LoadImageTestReadFrameStatus;

#define LOAD_IMAGE_TEST_KEEPALIVE_MS 100

#define LOAD_IMAGE_TEST_CHUNK_SIZE 512

#define LOAD_IMAGE_TEST_STALL_TIMEOUT_MS 5000

#define LOAD_IMAGE_TEST_ABORT_DRAIN_MS 250

/**
 * @brief Blocks until a full hex-encoded line arrives, decoding the digit
 * pairs straight into `frame` as they come in.
 *
 * Decoding is incremental so the tens-of-KB hex text never needs a buffer of
 * its own. An empty line means "nothing arrived yet" and is skipped, since the
 * shell submits lines on CR and a stray LF may precede the real frame.
 */
static LoadImageTestReadFrameStatus load_image_test_read_frame(PipeSide* pipe, uint8_t* frame, size_t frame_size, size_t* out_hex_chars) {
    size_t byte_count = 0;
    size_t hex_chars = 0;
    char pending_nibble = 0;
    bool have_pending_nibble = false;
    bool bad_hex = false;
    bool aborted = false;
    uint8_t chunk[LOAD_IMAGE_TEST_CHUNK_SIZE];
    uint32_t last_progress = furi_get_tick();

    while(true) {
        if(pipe_state(pipe) == PipeStateBroken) return LoadImageTestReadFrameBroken;

        size_t avail = pipe_bytes_available(pipe);
        if(avail == 0) {
            if(aborted) {
                if(furi_get_tick() - last_progress >= furi_ms_to_ticks(LOAD_IMAGE_TEST_ABORT_DRAIN_MS)) {
                    if(out_hex_chars) *out_hex_chars = hex_chars;
                    return LoadImageTestReadFrameAborted;
                }

                furi_thread_yield();
                continue;
            }
            if(furi_get_tick() - last_progress >= furi_ms_to_ticks(LOAD_IMAGE_TEST_STALL_TIMEOUT_MS)) {
                if(out_hex_chars) *out_hex_chars = hex_chars;
                return LoadImageTestReadFrameTimeout;
            }

            furi_thread_yield();
            continue;
        }
        last_progress = furi_get_tick();

        size_t to_read = MIN(avail, sizeof(chunk));
        size_t got = pipe_receive(pipe, chunk, to_read);
        if(got < to_read) return LoadImageTestReadFrameBroken;

        for(size_t i = 0; i < got; i++) {
            uint8_t ch = chunk[i];
            if(ch == CliKeyETX) {
                aborted = true;
                continue;
            }
            if(ch == '\n') {
                if(hex_chars == 0 && !aborted) continue; // stray blank line; keep waiting for the real one
                if(out_hex_chars) *out_hex_chars = hex_chars;
                if(aborted) return LoadImageTestReadFrameAborted;
                if(bad_hex) return LoadImageTestReadFrameBadHex;
                if(hex_chars != frame_size * 2) return LoadImageTestReadFrameWrongLength;
                return LoadImageTestReadFrameOk;
            }
            if(ch == '\r') continue;

            hex_chars++;
            if(aborted || bad_hex) continue; // this line's already invalid/abandoned; keep draining to '\n'

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
 * @brief Keeps re-pushing `frame` (gui_push_frame() is one-shot) until CTRL+C
 * is pressed or the session ends.
 *
 * Polls pipe_bytes_available() instead of blocking on pipe_receive(), which
 * would not return while the pipe is open but idle.
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
    }
}

/**
 * @brief Receives one hex-encoded, screen-sized grayscale frame, shows it and
 * keeps it on screen until CTRL+C.
 *
 * All image processing happens on the host (see the companion Python script);
 * this side only parses hex and blits bytes.
 */
void load_image_test_command_cli(PipeSide* pipe, FuriString* args, void* context) {
    UNUSED(context);
    furi_string_trim(args);
    if(furi_string_size(args) > 0) {
        printf("usage: load_image_test\r\n" CLI_STATUS_ERROR);
        return;
    }

    Gui* gui = furi_record_open(RECORD_GUI);
    size_t width = gui_get_width(gui);
    size_t height = gui_get_height(gui);
    size_t frame_size = width * height;
    size_t expected_hex_chars = frame_size * 2;

    FURI_LOG_I(TAG, "starting: %zux%zu, frame_size=%zu, heap_free=%zu", width, height, frame_size, memmgr_get_free_heap());

    uint8_t* frame = malloc(frame_size);

    FURI_LOG_I(TAG, "frame buffer allocated, heap_free=%zu", memmgr_get_free_heap());

    printf("Load Image Test\r\n");
    printf("Send a %zux%zu 8-bit grayscale frame as one hex-encoded line (%zu bytes / %zu hex chars).\r\n", width, height, frame_size, expected_hex_chars);
    printf(
        "Press CTRL+C to stop.\r\n"
        "(auto-exits if no data arrives within %lu s)\r\n> ",
        (unsigned long)(LOAD_IMAGE_TEST_STALL_TIMEOUT_MS / 1000));
    stdio_flush();

    size_t hex_chars = 0;
    uint32_t read_start = furi_get_tick();
    LoadImageTestReadFrameStatus status = load_image_test_read_frame(pipe, frame, frame_size, &hex_chars);
    uint32_t read_ms = furi_get_tick() - read_start;
    printf("\r\n");

    if(status == LoadImageTestReadFrameOk) {
        FURI_LOG_I(TAG, "frame ready in %lu ms, heap_free=%zu", (unsigned long)read_ms, memmgr_get_free_heap());
        gui_push_frame(gui, frame);
        printf(ANSI_FG_GREEN "Displayed. Press CTRL+C to stop (other input is ignored)." ANSI_RESET "\r\n" CLI_STATUS_OK);

        status = load_image_test_wait_for_stop(pipe, gui, frame);
        FURI_LOG_I(TAG, "stopping: status=%d", (int)status);
    } else if(status == LoadImageTestReadFrameWrongLength) {
        FURI_LOG_W(TAG, "wrong length after %lu ms: got %zu hex chars, want %zu", (unsigned long)read_ms, hex_chars, expected_hex_chars);
        printf(ANSI_FG_RED "Expected %zu hex chars, got %zu" ANSI_RESET "\r\n" CLI_STATUS_ERROR, expected_hex_chars, hex_chars);
    } else if(status == LoadImageTestReadFrameBadHex) {
        FURI_LOG_W(TAG, "bad hex digit after %lu ms (%zu-char line)", (unsigned long)read_ms, hex_chars);
        printf(ANSI_FG_RED "Invalid hex data" ANSI_RESET "\r\n" CLI_STATUS_ERROR);
    } else if(status == LoadImageTestReadFrameTimeout) {
        FURI_LOG_W(TAG, "timed out after %lu ms: got %zu of %zu hex chars", (unsigned long)read_ms, hex_chars, expected_hex_chars);
        printf(ANSI_FG_RED "Timed out waiting for data: got %zu of %zu hex chars" ANSI_RESET "\r\n" CLI_STATUS_ERROR, hex_chars, expected_hex_chars);
    } else {
        FURI_LOG_I(TAG, "stopping before a frame was loaded: status=%d", (int)status);
        printf(CLI_STATUS_ERROR);
    }

    gui_clear_frame(gui);
    furi_record_close(RECORD_GUI);

    free(frame);

    printf("\r\nStopped.\r\n");
}
