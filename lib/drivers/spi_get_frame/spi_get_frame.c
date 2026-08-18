#include "spi_get_frame.h"
#include <drivers/display/display_jd9853_reg.h>

#include <furi.h>
#include <furi_hal_resources.h>
#include <furi_hal_gpio.h>

#include <hardware/spi.h>
#include <hardware/dma.h>

#define TAG                       "SpiGetFrame"
#define SPI_GET_FRAME_SIZE        JD9853_WIDTH* JD9853_HEIGHT
#define SPI_GET_FRAME_COUNT       2
#define SPI_GET_FRAME_BAUDRATE    25 * 1000000 // 75MHz
#define SPI_GET_FRAME_SPI1_HANDLE spi1

typedef struct {
    uint8_t data[SPI_GET_FRAME_SIZE];
} SpiGetFrameBuffer;

struct SpiGetFrame {
    spi_inst_t* spi_periph;
    SpiGetFrameBuffer frame_buffers[SPI_GET_FRAME_COUNT];
    size_t current_frame;
    int dma_rx_channel;
    SpiGetFrameCallbackRx callback_rx;
    void* callback_context;
#ifdef SPI_SHOW_FPS
    volatile uint32_t frames_received; /* frames delivered to callback (ISR) */
    FuriTimer* fps_timer; /* periodic FPS logger */
#endif
};

static SpiGetFrame* spi_get_frame_instance = NULL;

#ifdef SPI_SHOW_FPS
static void spi_get_frame_fps_timer_callback(void* ctx) {
    UNUSED(ctx);
    SpiGetFrame* instance = spi_get_frame_instance;
    if(!instance) return;
    uint32_t fps = instance->frames_received;
    instance->frames_received = 0;
    FURI_LOG_W(TAG, "SPI RX: %lu FPS", (unsigned long)fps);
}
#endif

static void __isr __not_in_flash_func(spi_get_frame_rx_callback)(void) {
    if(spi_get_frame_instance->callback_rx) {
        spi_get_frame_instance->callback_rx(
            spi_get_frame_instance->frame_buffers[spi_get_frame_instance->current_frame].data, SPI_GET_FRAME_SIZE, spi_get_frame_instance->callback_context);
    }
#ifdef SPI_SHOW_FPS
    spi_get_frame_instance->frames_received++;
#endif
    dma_hw->ints2 = 1u << spi_get_frame_instance->dma_rx_channel;
}

static void __isr __not_in_flash_func(spi_get_frame_callback)(void* ctx) {
    SpiGetFrame* instance = (SpiGetFrame*)ctx;

    // Clear SPI FiFO
    while(spi_is_readable(instance->spi_periph)) {
        spi_get_hw(instance->spi_periph)->dr; // Read and discard data to clear the FIFO
    }
    // Prepare next DMA transfer
    instance->current_frame = (instance->current_frame + 1) % SPI_GET_FRAME_COUNT;
    dma_channel_set_write_addr(instance->dma_rx_channel, instance->frame_buffers[instance->current_frame].data, false);
    dma_channel_abort(instance->dma_rx_channel);
    dma_channel_start(instance->dma_rx_channel);
}

SpiGetFrame* spi_get_frame_init(void) {
    furi_check(spi_get_frame_instance == NULL); // Only one instance allowed
    SpiGetFrame* instance = (SpiGetFrame*)malloc(sizeof(SpiGetFrame));
    furi_check(instance);
    spi_get_frame_instance = instance;
    instance->spi_periph = SPI_GET_FRAME_SPI1_HANDLE;
    instance->current_frame = 0;
#ifdef SPI_SHOW_FPS
    instance->frames_received = 0;

    instance->fps_timer = furi_timer_alloc(spi_get_frame_fps_timer_callback, FuriTimerTypePeriodic, NULL);
    furi_timer_start(instance->fps_timer, 1000);
#endif
    // Initialize SPI peripheral
    int baundrate = spi_init(instance->spi_periph, SPI_GET_FRAME_BAUDRATE);
    spi_set_slave(instance->spi_periph, true);

    spi_set_format(instance->spi_periph, 16, SPI_CPOL_1, SPI_CPHA_1, SPI_MSB_FIRST);

    FURI_LOG_I(TAG, "SPI initialized with baudrate: %d", baundrate);

    // Initialize GPIOs
    furi_hal_gpio_set_function(&gpio_cpu_spi_sck, GpioAltFn1Spi);
    furi_hal_gpio_set_function(&gpio_cpu_spi_mosi, GpioAltFn1Spi);
    furi_hal_gpio_set_function(&gpio_cpu_spi_cs, GpioAltFn1Spi);

    furi_hal_gpio_init(&gpio_cpu_spi_miso, GpioModeInput, GpioPullUp, GpioSpeedLow);
    furi_hal_gpio_add_int_callback(&gpio_cpu_spi_miso, GpioConditionRise, spi_get_frame_callback, instance);

    // Initialize DMA for SPI RX
    instance->dma_rx_channel = dma_claim_unused_channel(true);
    dma_channel_config c = dma_channel_get_default_config(instance->dma_rx_channel);
    channel_config_set_transfer_data_size(&c, DMA_SIZE_16);
    channel_config_set_dreq(&c, spi_get_dreq(instance->spi_periph, false));
    channel_config_set_read_increment(&c, false);
    channel_config_set_write_increment(&c, true);
    channel_config_set_bswap(&c, true); // Byte swap for little-endian data
    dma_channel_set_read_addr(instance->dma_rx_channel, &spi_get_hw(instance->spi_periph)->dr, false);
    dma_channel_set_config(instance->dma_rx_channel, &c, false);

    //initialize interrupts DMA_IRQ_2
    hw_set_bits(&dma_hw->inte2, 1u << instance->dma_rx_channel);
    irq_set_exclusive_handler(DMA_IRQ_2, spi_get_frame_rx_callback);
    irq_set_enabled(DMA_IRQ_2, true);

    // Start first DMA transfer
    dma_channel_set_write_addr(instance->dma_rx_channel, instance->frame_buffers[instance->current_frame].data, false);
    dma_channel_set_transfer_count(instance->dma_rx_channel, SPI_GET_FRAME_SIZE / 2, false);

    dma_channel_start(instance->dma_rx_channel);

    return instance;
}

void spi_get_frame_deinit(SpiGetFrame* instance) {
    furi_check(instance);

    furi_hal_gpio_remove_int_callback(&gpio_cpu_spi_miso);
    // Remove DMA interrupt handler
    irq_set_enabled(DMA_IRQ_2, false);
    irq_remove_handler(DMA_IRQ_2, spi_get_frame_rx_callback);
    hw_clear_bits(&dma_hw->inte2, 1u << spi_get_frame_instance->dma_rx_channel);
    dma_channel_unclaim(spi_get_frame_instance->dma_rx_channel);
    // Deinitialize GPIOs
    furi_hal_gpio_init_ex(&gpio_cpu_spi_sck, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(&gpio_cpu_spi_mosi, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(&gpio_cpu_spi_cs, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
    furi_hal_gpio_init_ex(&gpio_cpu_spi_miso, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);

    // Deinitialize SPI peripheral
    spi_deinit(instance->spi_periph);
#ifdef SPI_SHOW_FPS
    if(instance->fps_timer) {
        furi_timer_free(instance->fps_timer);
        instance->fps_timer = NULL;
    }
#endif
    free(instance);
    spi_get_frame_instance = NULL;
}

void spi_get_frame_set_callback_rx(SpiGetFrame* instance, SpiGetFrameCallbackRx callback, void* context) {
    furi_check(instance);
    instance->callback_rx = callback;
    instance->callback_context = context;
}
