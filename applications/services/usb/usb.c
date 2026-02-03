#include <furi.h>
#include <furi_hal.h>
#include <tusb.h>

#define TAG "USB"

int32_t usb_srv(void* p) {
    UNUSED(p);

    tud_init(BOARD_TUD_RHPORT);
    // furi_thread_set_current_priority(FuriThreadPriorityHigh);

    while(1) {
        tud_task_ext(FuriWaitForever, false);
    }

    return 0;
}

int usb_srv_log(const char* fmt, ...) {
#define BUFFER_SIZE 256
    static char buffer[BUFFER_SIZE];

    va_list args;
    va_start(args, fmt);
    vsnprintf(buffer, BUFFER_SIZE, fmt, args);
    va_end(args);

    FURI_LOG_RAW_D("%s", buffer);

    return 0;
}
