
#include "furi_bsp_stdio.h"
#include <pico/binary_info.h>
#include <pico/stdio/driver.h>
#include <furi.h>

#define TAG "FuriBspStdio"

void furi_bsp_stdio_init(void) {
    stdio_set_driver_enabled(&furi_bsp_stdio, true);
    FURI_LOG_I(TAG, "Stdio initialized");
}

void furi_bsp_stdio_deinit(void) {
    stdio_set_driver_enabled(&furi_bsp_stdio, false);
    FURI_LOG_I(TAG, "Stdio deinitialized");
}

static void furi_bsp_stdio_out_chars(const char* buf, int length) {
    furi_thread_stdout_write(buf, length);
}

static int furi_bsp_stdio_in_chars(char* buf, int length) {
    return furi_thread_stdin_read(buf, length, FuriWaitForever);
}

static void furi_bsp_stdio_out_flush(void) {
    furi_thread_stdout_flush();
}

stdio_driver_t furi_bsp_stdio = {
    .out_chars = furi_bsp_stdio_out_chars,
    .out_flush = furi_bsp_stdio_out_flush,
    .in_chars = furi_bsp_stdio_in_chars,
#if PICO_STDIO_ENABLE_CRLF_SUPPORT
    .crlf_enabled = FURI_BSP_DEFAULT_CRLF
#endif
};
