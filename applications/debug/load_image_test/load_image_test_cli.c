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

// How often to re-push the displayed frame while idle. gui_push_frame() is a
// one-shot: the GUI blits it once on the very next redraw and immediately
// forgets it (gui->pending_frame is cleared as soon as that redraw reads it)
// - falling back to normal Clay compositing on every redraw after that, which
// repaints over it with whatever the desktop (or any other reason a redraw
// fires) would otherwise show. Re-pushing the same buffer periodically keeps
// it winning that race for as long as this command is still running, without
// needing our own persistent View.
#define LOAD_IMAGE_TEST_KEEPALIVE_MS 100

#define LOAD_IMAGE_TEST_CHUNK_SIZE 512

// How long to wait for the *next* byte before giving up on a stalled sender
// (host script hung, cable unplugged from a powered hub, etc.) - the pipe
// itself stays PipeStateOpen in that case (only the process on the other end
// has stopped talking), so pipe_state() alone never catches it: pipe_receive()
// retries internally for as long as the pipe is Open, forever, no matter how
// long that takes, so it can't be used to detect this either. Only our own
// elapsed-time bookkeeping around pipe_bytes_available() can.
#define LOAD_IMAGE_TEST_STALL_TIMEOUT_MS 5000

// Poll interval used while waiting for the next batch of the frame. This single
// value is what the transfer rate actually measures: pipe_bytes_available() is
// empty most of the time between two batches (the UART RX path hands the pipe
// ~32 bytes per interrupt, the USB VCP path ~64 bytes per USB packet), so this
// is effectively the per-batch latency. At 10ms a full frame measured 12782ms
// for 74304 hex chars (~5.8 KB/s) - on *both* transports, i.e. 25x slower than
// the 1.5 Mbaud wire - and long enough that a host which gives up reading after
// a few seconds calls it a failure while the frame is still arriving. 1ms costs
// a kilohertz of idle-only wakeups and gets the loop out of the transport's way.
#define LOAD_IMAGE_TEST_POLL_INTERVAL_MS 1

// How long an already-CTRL+C'd read keeps draining a sender that is still
// mid-frame. An empty buffer on its own does not mean "nothing left to
// drain" (the reader is far faster than the wire, so it drains the buffer
// between USB packets and then sits idle while the rest of the frame is still
// on its way - the device side holds ~8 KB out of the ~74 KB a full frame's
// hex text takes, see cli_vcp.c's VCP_BUF_SIZE), so a quiet period is what
// actually ends the wait. Short, because the user asked to stop and the shell
// must get its pipe back.
#define LOAD_IMAGE_TEST_ABORT_DRAIN_MS 250

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
 *
 * A completely empty line (bare '\n', no hex chars before it) is treated as
 * nothing arrived yet rather than as a zero-length frame: the shell submits
 * on CR, not LF (see load_image_test.py), so a stray leftover LF sometimes
 * ends up queued ahead of the real frame line - skipping it here means the
 * caller doesn't need its own separate "drain leftover bytes" pass, which
 * would otherwise risk eating real frame bytes from a host that writes the
 * command and the frame in one go instead of waiting for the prompt.
 */
static LoadImageTestReadFrameStatus
    load_image_test_read_frame(PipeSide* pipe, uint8_t* frame, size_t frame_size, size_t* out_hex_chars) {
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
                // Keep draining a sender that is still streaming (those bytes
                // would otherwise land as garbage in front of the shell's next
                // command), but only while it keeps streaming: once it goes
                // quiet, there is nothing left to protect the shell from, and
                // an interactive user who typed nothing after CTRL+C is not
                // made to wait. A '\n' (the frame's terminator) also ends it.
                if(furi_get_tick() - last_progress >= furi_ms_to_ticks(LOAD_IMAGE_TEST_ABORT_DRAIN_MS)) {
                    if(out_hex_chars) *out_hex_chars = hex_chars;
                    return LoadImageTestReadFrameAborted;
                }
                furi_delay_ms(LOAD_IMAGE_TEST_POLL_INTERVAL_MS);
                continue;
            }
            if(furi_get_tick() - last_progress >= furi_ms_to_ticks(LOAD_IMAGE_TEST_STALL_TIMEOUT_MS)) {
                if(out_hex_chars) *out_hex_chars = hex_chars;
                return LoadImageTestReadFrameTimeout;
            }
            furi_delay_ms(LOAD_IMAGE_TEST_POLL_INTERVAL_MS);
            continue;
        }
        last_progress = furi_get_tick();

        // pipe_bytes_available() already reported these bytes as buffered, so
        // this receives without blocking (short of the pipe breaking mid-read).
        size_t to_read = MIN(avail, sizeof(chunk));
        size_t got = pipe_receive(pipe, chunk, to_read);
        if(got < to_read) return LoadImageTestReadFrameBroken;

        for(size_t i = 0; i < got; i++) {
            uint8_t ch = chunk[i];
            if(ch == CliKeyETX) {
                // Don't return right away: a sender that is still mid-frame
                // keeps streaming behind this byte, and those bytes would land
                // as garbage in front of the shell's next command once this
                // pipe is handed back to it. Keep consuming until the line
                // ends or the sender goes quiet - see
                // LOAD_IMAGE_TEST_ABORT_DRAIN_MS and the avail==0 branch above.
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
 * @brief Blocks, periodically re-pushing `frame` (see LOAD_IMAGE_TEST_KEEPALIVE_MS
 * and gui_push_frame()'s one-shot nature above) so it stays on screen, until
 * CTRL+C is pressed or the session ends.
 *
 * This polls pipe_bytes_available() instead of blocking on a single-byte
 * pipe_receive() (the way rpc_cli.c's read loop does) because that call only
 * ever returns early on PipeStateBroken - while the pipe stays Open with
 * nothing to read, it retries internally forever. This loop needs to keep
 * waking up on its own even when nothing arrives, to re-push the keepalive
 * frame, so it can't hand control over to a call that might not give it back.
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
        printf("usage: load_image_test\r\n" CLI_STATUS_ERROR);
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

    FURI_LOG_I(TAG, "frame buffer allocated, heap_free=%zu", memmgr_get_free_heap());

    printf("Load Image Test\r\n");
    printf(
        "Send a %zux%zu 8-bit grayscale frame as one hex-encoded line (%zu bytes / %zu hex chars).\r\n",
        width,
        height,
        frame_size,
        expected_hex_chars);
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
        // Report success here, at the point the frame actually made it to the
        // screen - not after wait_for_stop() below, whose own exit status
        // reflects how the *idling* ended (CTRL+C vs. the session dropping),
        // not whether loading and displaying the image succeeded.
        printf(
            ANSI_FG_GREEN "Displayed. Press CTRL+C to stop (other input is ignored)." ANSI_RESET
                "\r\n" CLI_STATUS_OK);

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
            ANSI_FG_RED "Expected %zu hex chars, got %zu" ANSI_RESET "\r\n" CLI_STATUS_ERROR,
            expected_hex_chars,
            hex_chars);
    } else if(status == LoadImageTestReadFrameBadHex) {
        FURI_LOG_W(TAG, "bad hex digit after %lu ms (%zu-char line)", (unsigned long)read_ms, hex_chars);
        printf(ANSI_FG_RED "Invalid hex data" ANSI_RESET "\r\n" CLI_STATUS_ERROR);
    } else if(status == LoadImageTestReadFrameTimeout) {
        FURI_LOG_W(
            TAG, "timed out after %lu ms: got %zu of %zu hex chars", (unsigned long)read_ms, hex_chars, expected_hex_chars);
        printf(
            ANSI_FG_RED "Timed out waiting for data: got %zu of %zu hex chars" ANSI_RESET "\r\n" CLI_STATUS_ERROR,
            hex_chars,
            expected_hex_chars);
    } else {
        // Aborted (CTRL+C) or Broken (session gone) before any frame was
        // loaded - either way, no image ended up on screen.
        FURI_LOG_I(TAG, "stopping before a frame was loaded: status=%d", (int)status);
        printf(CLI_STATUS_ERROR);
    }

    // gui_clear_frame() takes the GUI's own lock, so by the time it returns the
    // GUI thread is guaranteed to no longer be reading `frame` out of any
    // pending blit - only past that point is it safe to free it. Both
    // gui_push_frame() calls above (initial display and the keepalive loop in
    // load_image_test_wait_for_stop()) hand the GUI thread this same pointer,
    // so this ordering matters; don't reorder it after free() in a refactor.
    gui_clear_frame(gui);
    furi_record_close(RECORD_GUI);

    free(frame);

    printf("\r\nStopped.\r\n");
}
