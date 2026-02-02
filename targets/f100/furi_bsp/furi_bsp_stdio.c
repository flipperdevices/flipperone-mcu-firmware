#include "core/log.h"
#include "pico/binary_info.h"
#include "pico/stdio/driver.h"
#include "furi_bsp_stdio.h"
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

static void furi_bsp_stdio_out_chars(const char *buf, int length) {
    furi_thread_stdout_write(buf, length);
}

static int _fgetc(FILE* stream) {
    if(stream != stdin) return EOF;
    char c;
    if(furi_thread_stdin_read(&c, 1, FuriWaitForever) == 0) return EOF;
    return c;
}

static int furi_bsp_stdio_in_chars(char *buf, int length) {
    //return (int)SEGGER_RTT_Read(0, buf, (unsigned)length);
    int i=0;
    do{
        buf[i++] = _fgetc(stdin);
    } while(i<length && (buf[i-1] != EOF));

    FURI_LOG_W(TAG, "Stdio input not supported");
    return 0;
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
