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

    //usb_network_init();

    //furi_hal_usb_set_irq(usb_core_irq, NULL);
    // tusb_rhport_init_t dev_init = {
    //     .role = TUSB_ROLE_DEVICE,
    //     .speed = TUSB_SPEED_AUTO,
    // };
    // tusb_init(BOARD_TUD_RHPORT, &dev_init);

    furi_delay_ms(1000);

    board_init();

    tud_init(BOARD_TUD_RHPORT);

    if(board_init_after_tusb) {
        board_init_after_tusb();
    }

   // furi_thread_set_current_priority(FuriThreadPriorityHigh);

    while(1) {
        tud_task_ext(FuriWaitForever, false);
    }

    return 0;
}


void tud_cdc_rx_cb(uint8_t itf) {
    // allocate buffer for the data in the stack
    uint8_t buf[CFG_TUD_CDC_RX_BUFSIZE];

    // printf("RX CDC %d\n", itf);
    FURI_LOG_I(TAG, "RX CDC %d", itf);

    // read the available data
    // | IMPORTANT: also do this for CDC0 because otherwise
    // | you won't be able to print anymore to CDC0
    // | next time this function is called
    uint32_t count = tud_cdc_n_read(itf, buf, sizeof(buf));

    // check if the data was received on the second cdc interface
    if(itf == 1) {
        // process the received data
        buf[count] = 0; // null-terminate the string
        // now echo data back to the console on CDC 0
        //printf("Received on CDC 1: %s\n", buf);
        FURI_LOG_I(TAG, "Received on CDC 1: %s", buf);

        // and echo back OK on CDC 1
        tud_cdc_n_write(itf, (uint8_t const*)"OK\r\n", 4);
        tud_cdc_n_write_flush(itf);
    }

    if(itf == 0) {
        // process the received data
        buf[count] = 0; // null-terminate the string
        // now echo data back to the console on CDC 1
        tud_cdc_n_write(1, buf, count);
        tud_cdc_n_write_flush(1);
        // and echo back OK on CDC 0
        tud_cdc_n_write(itf, (uint8_t const*)"OK\r\n", 4);
        tud_cdc_n_write_flush(itf);
    }
}

int usb_srv_log(const char* fmt, ...) {
    FuriString* string = furi_string_alloc();

    va_list args;
    va_start(args, fmt);
    furi_string_vprintf(string, fmt, args);
    va_end(args);

    furi_string_trim(string, "\r\n");

    FURI_LOG_D("tUSB", "%s", furi_string_get_cstr(string));
    furi_string_free(string);
    return 0;
}
