#include <furi.h>
#include <furi_hal.h>
#include <tusb.h>
#include <bsp/board_api.h>

#define TAG "USB"

// static void usb_core_irq(void* context) {
//     UNUSED(context);
//     tusb_int_handler(BOARD_TUD_RHPORT, true);
// }

int32_t usb_srv(void* p) {
    UNUSED(p);

    tud_init(BOARD_TUD_RHPORT);
    // furi_thread_set_current_priority(FuriThreadPriorityHigh);

    while(1) {
        tud_task_ext(FuriWaitForever, false);
    }

    return 0;
}

// void tud_cdc_rx_cb(uint8_t itf) {
//     // allocate buffer for the data in the stack
//     uint8_t buf[CFG_TUD_CDC_RX_BUFSIZE];

//     // printf("RX CDC %d\n", itf);
//     FURI_LOG_I(TAG, "RX CDC %d", itf);

//     // read the available data
//     // | IMPORTANT: also do this for CDC0 because otherwise
//     // | you won't be able to print anymore to CDC0
//     // | next time this function is called
//     uint32_t count = tud_cdc_n_read(itf, buf, sizeof(buf));

//     // check if the data was received on the second cdc interface
//     if(itf == 1) {
//         // process the received data
//         buf[count] = 0; // null-terminate the string
//         // now echo data back to the console on CDC 0
//         //printf("Received on CDC 1: %s\n", buf);
//         FURI_LOG_I(TAG, "Received on CDC 1: %s", buf);

//         // and echo back OK on CDC 1
//         tud_cdc_n_write(itf, (uint8_t const*)"OK\r\n", 4);
//         tud_cdc_n_write_flush(itf);
//     }

//     if(itf == 0) {
//         // process the received data
//         buf[count] = 0; // null-terminate the string
//         // now echo data back to the console on CDC 1
//         tud_cdc_n_write(1, buf, count);
//         tud_cdc_n_write_flush(1);
//         // and echo back OK on CDC 0
//         tud_cdc_n_write(itf, (uint8_t const*)"OK\r\n", 4);
//         tud_cdc_n_write_flush(itf);
//     }
// }

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
