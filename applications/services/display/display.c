#include "display.h"

#include <furi.h>
#include <drivers/display/display_jd9853_qspi.h>
#include <drivers/spi_get_frame/spi_get_frame.h>

#define TAG "Display"

#define DISPLAY_THREAD_FLAG_SPI_FRAME_ISR 0x00000001

typedef struct {
    FuriPubSub* event_pubsub;
    FuriThreadId thread_id;
    DisplayJd9853QSPI* display_header;
    SpiGetFrame* spi_get_frame;
    uint8_t* spi_get_frame_data_ptr;
    size_t spi_get_frame_data_size;
} Display;

static void __isr __not_in_flash_func(display_pi_get_frame_isr)(uint8_t* data, size_t size, void* context) {
    Display* instance = (Display*)context;
    instance->spi_get_frame_data_ptr = data;
    instance->spi_get_frame_data_size = size;
    furi_thread_flags_set(instance->thread_id, DISPLAY_THREAD_FLAG_SPI_FRAME_ISR);
}

void display_event_isr(void* context) {
    furi_assert(context);
    Display* instance = (Display*)context;
}

int32_t display_srv(void* p) {
    UNUSED(p);

    Display* instance = (Display*)malloc(sizeof(Display));
    instance->display_header = display_jd9853_qspi_init();
    instance->spi_get_frame = spi_get_frame_init();
    instance->thread_id = furi_thread_get_current_id();
    instance->event_pubsub = furi_pubsub_alloc();

    display_jd9853_qspi_set_brightness(instance->display_header, 5); // Set backlight to 10%
    spi_get_frame_set_callback_rx(instance->spi_get_frame, display_pi_get_frame_isr, instance);
    furi_record_create(RECORD_DISPLAY, instance->event_pubsub);
#ifdef SRV_CLI
    CliRegistry* registry = furi_record_open(RECORD_CLI);
    cli_registry_add_command(registry, "display", CliCommandFlagParallelSafe, display_cli, instance->event_pubsub);
    furi_record_close(RECORD_CLI);
#endif

    while(1) {
        furi_thread_flags_wait(DISPLAY_THREAD_FLAG_SPI_FRAME_ISR, FuriFlagWaitAny, FuriWaitForever);
        display_jd9853_qspi_write_buffer(instance->display_header, instance->spi_get_frame_data_ptr, instance->spi_get_frame_data_size);
        // display_jd9853_qspi_fill(instance->display_header, 0); // Fill white
        // furi_delay_ms(200);
        // display_jd9853_qspi_fill(instance->display_header, 255); // Fill white
    }

    return 0;
}
