#include <furi_hal_serial.h>
#include <furi_hal_serial_types_i.h>

#include <furi_hal_resources.h>
#include <furi_hal_interrupt.h>
// #include <furi_hal_cortex.h>
// #include <furi_hal_bus.h>
// #include <furi_hal_dma.h>

// #include <stm32u5xx_ll_rcc.h>
// #include <stm32u5xx_ll_usart.h>
// #include <stm32u5xx_ll_dma.h>
#include <hardware/uart.h>
#include <stdbool.h>
#include <stdio.h>

typedef struct {
    FuriHalSerialHandle* handle;
    uart_inst_t* periph_ptr;
    FuriHalSerialRxCallback rx_callback;
    FuriHalSerialTxCallback tx_callback;
    void* callback_context;
    uint32_t dma_rx_channel;
    uint32_t dma_tx_channel;
    uint32_t baud_rate;
} FuriHalSerial;

typedef struct {
    uart_inst_t* periph;
    GpioAltFn alt_fn;
    const GpioPin* gpio[FuriHalSerialPinMax];
    FuriHalInterruptId irq;
    uint32_t tx_dma_request;
    uint32_t rx_dma_request;
} FuriHalSerialResources;

// static const uint32_t furi_hal_serial_auto_baundrate_mode[] = {
//     [FuriHalSerialAutoBaudRateModeStartBit] = LL_USART_AUTOBAUD_DETECT_ON_STARTBIT,
//     [FuriHalSerialAutoBaudRateModeFallingEdge] = LL_USART_AUTOBAUD_DETECT_ON_FALLINGEDGE,
//     [FuriHalSerialAutoBaudRateMode0x7FFrame] = LL_USART_AUTOBAUD_DETECT_ON_7F_FRAME,
//     [FuriHalSerialAutoBaudRateMode0x55Frame] = LL_USART_AUTOBAUD_DETECT_ON_55_FRAME,
// };

static const FuriHalSerialResources furi_hal_serial_resources[FuriHalSerialIdMax] = {
    [FuriHalSerialIdUart0] =
        {
            .periph = uart0,
            .alt_fn = GpioAltFn2Uart,
            .gpio =
                {
                    [FuriHalSerialPinTx] = &gpio_uart0_tx,
                    [FuriHalSerialPinRx] = &gpio_uart0_rx,
                    [FuriHalSerialPinRts] = NULL,
                    [FuriHalSerialPinCts] = NULL,
                },
            .irq = FuriHalInterruptIdUart0,
            .tx_dma_request = 0,
            .rx_dma_request = 0,
        },
    [FuriHalSerialIdUart1] =
        {
            .periph = uart1,
            .alt_fn = GpioAltFn2Uart,
            .gpio =
                {
                    [FuriHalSerialPinTx] = &gpio_uart1_tx,
                    [FuriHalSerialPinRx] = &gpio_uart1_rx,
                    [FuriHalSerialPinRts] = NULL,
                    [FuriHalSerialPinCts] = NULL,
                },
            .irq = FuriHalInterruptIdUart1,
            .tx_dma_request = 0,
            .rx_dma_request = 0,
        },
};

static FuriHalSerial* furi_hal_serial[FuriHalSerialIdMax];

static bool furi_hal_serial_is_enabled(FuriHalSerialHandle* handle);

static void furi_hal_serial_set_config(
    FuriHalSerialHandle* handle,
    FuriHalSerialConfigDataBits data_bits,
    FuriHalSerialConfigParity parity,
    FuriHalSerialConfigStopBits stop_bits);

static inline void furi_hal_serial_check(FuriHalSerialHandle* handle) {
    furi_check(handle, "Serial: handle is NULL");
    furi_assert(furi_hal_serial[handle->id], "Serial: handle is not initialized");
}

static void furi_hal_serial_irq_callback(void* context) {
    FuriHalSerialHandle* handle = context;

    FuriHalSerial* serial = furi_hal_serial[handle->id];
    uart_inst_t* periph = furi_hal_serial_resources[handle->id].periph;

    uint32_t events = 0;

    // if(LL_USART_IsActiveFlag_RXNE_RXFNE(periph)) {
    //     events |= FuriHalSerialRxEventData;
    // }
    // if(LL_USART_IsActiveFlag_FE(periph)) {
    //     LL_USART_ClearFlag_FE(periph);
    //     events |= FuriHalSerialRxEventFrameError;
    // }
    // if(LL_USART_IsActiveFlag_NE(periph)) {
    //     LL_USART_ClearFlag_NE(periph);
    //     events |= FuriHalSerialRxEventFrameError;
    // }
    // if(LL_USART_IsActiveFlag_PE(periph)) {
    //     LL_USART_ClearFlag_PE(periph);
    //     events |= FuriHalSerialRxEventParityError;
    // }
    // if(LL_USART_IsActiveFlag_ORE(periph)) {
    //     LL_USART_ClearFlag_ORE(periph);
    //     events |= FuriHalSerialRxEventOverrunError;
    // }
    // if(serial->rx_callback) {
    //     serial->rx_callback(handle, events, serial->callback_context);
    // }
}

static void furi_hal_serial_dma_irq_callback(void* context) {
    FuriHalSerialHandle* handle = context;
    FuriHalSerial* serial = furi_hal_serial[handle->id];

    const uint32_t dma_rx_channel = serial->dma_rx_channel;
    const uint32_t dma_tx_channel = serial->dma_tx_channel;

    // if(LL_DMA_IsActiveFlag_TC(GPDMA1, dma_rx_channel)) {
    //     LL_DMA_ClearFlag_TC(GPDMA1, dma_rx_channel);
    //     LL_DMA_DisableChannel(GPDMA1, dma_rx_channel);

    //     if(serial->rx_callback) {
    //         serial->rx_callback(handle, FuriHalSerialRxEventData, serial->callback_context);
    //     }
    // }

    // if(LL_DMA_IsActiveFlag_TC(GPDMA1, dma_tx_channel)) {
    //     LL_DMA_ClearFlag_TC(GPDMA1, dma_tx_channel);
    //     LL_DMA_DisableChannel(GPDMA1, dma_tx_channel);

    //     if(serial->tx_callback) {
    //         serial->tx_callback(handle, FuriHalSerialTxEventComplete, serial->callback_context);
    //     }
    // }
}

static void furi_hal_serial_dma_tx_init(FuriHalSerialHandle* handle) {
    furi_assert(handle);

    // FuriHalSerialId serial_id = handle->id;
    // FuriHalSerial* serial = furi_hal_serial[serial_id];
    // furi_assert(serial);

    // furi_check(furi_hal_dma_allocate_gpdma_channel(&serial->dma_tx_channel));

    // const uint32_t dma_channel = serial->dma_tx_channel;
    // const uint32_t dma_request = furi_hal_serial_resources[serial_id].tx_dma_request;

    // LL_DMA_InitTypeDef dma_init_struct = {
    //     .SrcAddress = 0,
    //     .DestAddress = LL_USART_DMA_GetRegAddr(serial->periph_ptr, LL_USART_DMA_REG_DATA_TRANSMIT),
    //     .BlkDataLength = 0,
    //     .Request = dma_request,

    //     .Direction = LL_DMA_DIRECTION_MEMORY_TO_PERIPH,
    //     .BlkHWRequest = LL_DMA_HWREQUEST_SINGLEBURST,
    //     .DataAlignment = LL_DMA_DATA_ALIGN_ZEROPADD,

    //     .SrcAllocatedPort = LL_DMA_SRC_ALLOCATED_PORT1,
    //     .SrcBurstLength = 1,
    //     .SrcIncMode = LL_DMA_SRC_INCREMENT,
    //     .SrcDataWidth = LL_DMA_SRC_DATAWIDTH_BYTE,

    //     .DestAllocatedPort = LL_DMA_DEST_ALLOCATED_PORT0,
    //     .DestBurstLength = 1,
    //     .DestIncMode = LL_DMA_DEST_FIXED,
    //     .DestDataWidth = LL_DMA_DEST_DATAWIDTH_BYTE,

    //     .TriggerMode = LL_DMA_TRIGM_BLK_TRANSFER,
    //     .TriggerPolarity = LL_DMA_TRIG_POLARITY_MASKED,
    //     .TriggerSelection = 0,

    //     .TransferEventMode = LL_DMA_TCEM_BLK_TRANSFER,

    //     .Priority = LL_DMA_LOW_PRIORITY_MID_WEIGHT,
    //     .LinkAllocatedPort = LL_DMA_LINK_ALLOCATED_PORT1,
    //     .LinkStepMode = LL_DMA_LSM_FULL_EXECUTION,
    //     .LinkedListBaseAddr = 0,
    //     .LinkedListAddrOffset = 0,
    // };

    // LL_DMA_Init(GPDMA1, dma_channel, &dma_init_struct);

    // furi_hal_interrupt_set_isr(
    //     furi_hal_dma_get_gpdma_interrupt_id(dma_channel),
    //     furi_hal_serial_dma_irq_callback,
    //     handle);

    // LL_DMA_EnableIT_TC(GPDMA1, dma_channel);
}

static void furi_hal_serial_dma_rx_init(FuriHalSerialHandle* handle) {
    furi_assert(handle);

    // FuriHalSerialId serial_id = handle->id;
    // FuriHalSerial* serial = furi_hal_serial[serial_id];
    // furi_assert(serial);

    // furi_check(furi_hal_dma_allocate_gpdma_channel(&serial->dma_rx_channel));

    // const uint32_t dma_channel = serial->dma_rx_channel;
    // const uint32_t dma_request = furi_hal_serial_resources[serial_id].rx_dma_request;

    // LL_DMA_InitTypeDef dma_init_struct = {
    //     .SrcAddress = LL_USART_DMA_GetRegAddr(serial->periph_ptr, LL_USART_DMA_REG_DATA_RECEIVE),
    //     .DestAddress = 0,
    //     .BlkDataLength = 0,
    //     .Request = dma_request,

    //     .Direction = LL_DMA_DIRECTION_PERIPH_TO_MEMORY,
    //     .BlkHWRequest = LL_DMA_HWREQUEST_SINGLEBURST,
    //     .DataAlignment = LL_DMA_DATA_ALIGN_ZEROPADD,

    //     .SrcAllocatedPort = LL_DMA_SRC_ALLOCATED_PORT0,
    //     .SrcBurstLength = 1,
    //     .SrcIncMode = LL_DMA_SRC_FIXED,
    //     .SrcDataWidth = LL_DMA_SRC_DATAWIDTH_BYTE,

    //     .DestAllocatedPort = LL_DMA_DEST_ALLOCATED_PORT1,
    //     .DestBurstLength = 1,
    //     .DestIncMode = LL_DMA_DEST_INCREMENT,
    //     .DestDataWidth = LL_DMA_DEST_DATAWIDTH_BYTE,

    //     .TriggerMode = LL_DMA_TRIGM_BLK_TRANSFER,
    //     .TriggerPolarity = LL_DMA_TRIG_POLARITY_MASKED,
    //     .TriggerSelection = 0,

    //     .TransferEventMode = LL_DMA_TCEM_BLK_TRANSFER,

    //     .Priority = LL_DMA_LOW_PRIORITY_MID_WEIGHT,
    //     .LinkAllocatedPort = LL_DMA_LINK_ALLOCATED_PORT1,
    //     .LinkStepMode = LL_DMA_LSM_FULL_EXECUTION,
    //     .LinkedListBaseAddr = 0,
    //     .LinkedListAddrOffset = 0,
    // };

    // LL_DMA_Init(GPDMA1, dma_channel, &dma_init_struct);

    // furi_hal_interrupt_set_isr(
    //     furi_hal_dma_get_gpdma_interrupt_id(dma_channel),
    //     furi_hal_serial_dma_irq_callback,
    //     handle);

    // LL_DMA_EnableIT_TC(GPDMA1, dma_channel);
}

void furi_hal_serial_init(FuriHalSerialHandle* handle, uint32_t baud_rate) {
    furi_check(handle);

    const FuriHalSerialId serial_id = handle->id;
    furi_check(furi_hal_serial[serial_id] == NULL);

    furi_hal_serial[serial_id] = malloc(sizeof(FuriHalSerial));

    FuriHalSerial* serial = furi_hal_serial[serial_id];
    uart_inst_t* periph = furi_hal_serial_resources[serial_id].periph;

    serial->handle = handle;
    serial->periph_ptr = periph;

    // TODO: This should not be called by default
    furi_hal_serial_dma_tx_init(handle);
    furi_hal_serial_dma_rx_init(handle);

    serial->baud_rate = uart_init(periph, baud_rate);

    // Initialize GPIOs
    for(size_t i = 0; i < FuriHalSerialPinMax; i++) {
        const GpioPin* gpio = furi_hal_serial_resources[serial_id].gpio[i];
        if(gpio != NULL) {
            furi_hal_gpio_init_ex(gpio, GpioModeOutputPushPull, GpioPullNo, GpioSpeedFast, furi_hal_serial_resources[serial_id].alt_fn);
        }
    }

    furi_hal_serial_set_config(handle, FuriHalSerialConfigDataBits8, FuriHalSerialConfigParityNone, FuriHalSerialConfigStopBits_1);

    furi_hal_serial_set_hw_flow_control(handle, FuriHalSerialHwFlowControlNone);

    // ToDo: Disable FIFOs
    uart_set_fifo_enabled(periph, false);
    furi_hal_serial_clear(handle);
}

static void furi_hal_serial_dma_tx_deinit(FuriHalSerialHandle* handle) {
    furi_assert(handle);

    // FuriHalSerial* serial = furi_hal_serial[handle->id];
    // LL_USART_DisableDMAReq_TX(serial->periph_ptr);

    // LL_DMA_DisableChannel(GPDMA1, serial->dma_tx_channel);

    // furi_hal_interrupt_set_isr(furi_hal_dma_get_gpdma_interrupt_id(serial->dma_tx_channel), NULL, NULL);

    // furi_hal_dma_free_gpdma_channel(serial->dma_tx_channel);
}

static void furi_hal_serial_dma_rx_deinit(FuriHalSerialHandle* handle) {
    furi_assert(handle);

    // FuriHalSerial* serial = furi_hal_serial[handle->id];
    // LL_USART_DisableDMAReq_RX(serial->periph_ptr);

    // LL_DMA_DisableChannel(GPDMA1, serial->dma_rx_channel);

    // furi_hal_interrupt_set_isr(furi_hal_dma_get_gpdma_interrupt_id(serial->dma_rx_channel), NULL, NULL);

    // furi_hal_dma_free_gpdma_channel(serial->dma_rx_channel);
}

void furi_hal_serial_deinit(FuriHalSerialHandle* handle) {
    furi_check(handle);

    FuriHalSerialId serial_id = handle->id;
    FuriHalSerial* serial = furi_hal_serial[serial_id];
     uart_inst_t* periph = serial->periph_ptr;
    // TODO: deinit() should NOT be called before init()
    if(serial == NULL) return;


    furi_hal_serial_dma_rx_deinit(handle);
    furi_hal_serial_dma_tx_deinit(handle);

    uart_deinit(periph);

    // Deinitialize GPIOs
    for(size_t i = 0; i < FuriHalSerialPinMax; i++) {
        const GpioPin* gpio = furi_hal_serial_resources[serial_id].gpio[i];
        if(gpio != NULL) {
            furi_hal_gpio_init_ex(gpio, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
        }
    }

    free(serial);

    furi_hal_serial[serial_id] = NULL;
}

inline void furi_hal_serial_suspend(FuriHalSerialHandle* handle) {
    furi_hal_serial_check(handle);

    uart_inst_t* periph = furi_hal_serial[handle->id]->periph_ptr;

    //tODO: add implementation disable USART
}

inline void furi_hal_serial_resume(FuriHalSerialHandle* handle) {
    furi_hal_serial_check(handle);

    uart_inst_t* periph = furi_hal_serial[handle->id]->periph_ptr;

    // TODO: add implementation enable USART
}

bool furi_hal_serial_is_baud_rate_supported(FuriHalSerialHandle* handle, uint32_t baud_rate) {
    UNUSED(handle);
    return baud_rate >= 10 && baud_rate <= 7800000;
}

uint32_t furi_hal_serial_get_baud_rate(FuriHalSerialHandle* handle) {
    furi_check(handle);

    FuriHalSerial* serial = furi_hal_serial[handle->id];
    furi_check(serial);

    return serial->baud_rate;
}

void furi_hal_serial_set_baud_rate(FuriHalSerialHandle* handle, uint32_t baud_rate) {
    furi_check(handle);
    furi_check(furi_hal_serial_is_baud_rate_supported(handle, baud_rate));

    FuriHalSerial* serial = furi_hal_serial[handle->id];
    furi_check(serial);

    uart_inst_t* periph = serial->periph_ptr;
    serial->baud_rate = uart_set_baudrate(periph, baud_rate);
}

void furi_hal_serial_set_hw_flow_control(FuriHalSerialHandle* handle, FuriHalSerialHwFlowControl flow_control) {
    furi_check(handle);

    const FuriHalSerialResources* resources = &furi_hal_serial_resources[handle->id];

    const GpioPin* gpio_rts = resources->gpio[FuriHalSerialPinRts];
    const GpioPin* gpio_cts = resources->gpio[FuriHalSerialPinCts];
    const GpioAltFn alt_fn = resources->alt_fn;

    if(gpio_rts == NULL || gpio_cts == NULL) {
        // Assuming that both pins must be defined
        return;
    }
    
    bool rts_enabled = false;
    bool cts_enabled = false;

    uint32_t hw_flow_reg_value;

    if(flow_control == FuriHalSerialHwFlowControlNone) {
        if(gpio_rts != NULL) furi_hal_gpio_init_ex(gpio_rts, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
        if(gpio_cts != NULL) furi_hal_gpio_init_ex(gpio_cts, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);

    } else if(flow_control == FuriHalSerialHwFlowControlRts) {
        if(gpio_rts != NULL) furi_hal_gpio_init_ex(gpio_rts, GpioModeOutputPushPull, GpioPullNo, GpioSpeedFast, alt_fn);
        if(gpio_cts != NULL) furi_hal_gpio_init_ex(gpio_cts, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
        rts_enabled = true;

    } else if(flow_control == FuriHalSerialHwFlowControlCts) {
        if(gpio_rts != NULL) furi_hal_gpio_init_ex(gpio_rts, GpioModeInput, GpioPullNo, GpioSpeedLow, GpioAltFnUnused);
        if(gpio_cts != NULL) furi_hal_gpio_init_ex(gpio_cts, GpioModeInput, GpioPullUp, GpioSpeedLow, alt_fn);
        cts_enabled = true;

    } else if(flow_control == FuriHalSerialHwFlowControlRtsCts) {
        if(gpio_rts != NULL) furi_hal_gpio_init_ex(gpio_rts, GpioModeOutputPushPull, GpioPullNo, GpioSpeedFast, alt_fn);
        if(gpio_cts != NULL) furi_hal_gpio_init_ex(gpio_cts, GpioModeInput, GpioPullUp, GpioSpeedLow, alt_fn);
        rts_enabled = true;
        cts_enabled = true;

    } else {
        furi_crash();
    }

    const bool is_enabled = furi_hal_serial_is_enabled(handle);
    if(is_enabled) {
        furi_hal_serial_suspend(handle);
    }

    // Set UART flow control CTS/RTS, we don't want these, so turn them off
    uart_set_hw_flow(resources->periph, cts_enabled, rts_enabled);

    if(is_enabled) {
        furi_hal_serial_resume(handle);
    }
}

void furi_hal_serial_set_callback(FuriHalSerialHandle* handle, FuriHalSerialTxCallback tx_callback, FuriHalSerialRxCallback rx_callback, void* context) {
    furi_check(handle);

    FuriHalSerial* serial = furi_hal_serial[handle->id];
    furi_check(serial);

    serial->tx_callback = tx_callback;
    serial->rx_callback = rx_callback;
    serial->callback_context = context;
}

size_t furi_hal_serial_tx(FuriHalSerialHandle* handle, const uint8_t* buffer, size_t buffer_size, uint32_t timeout) {
    furi_check(handle);
    furi_check(buffer);
    furi_check(buffer_size);

    uart_write_blocking(furi_hal_serial_resources[handle->id].periph, buffer, buffer_size);

    return buffer_size;
}

bool furi_hal_serial_tx_wait_complete(FuriHalSerialHandle* handle, uint32_t timeout) {
    furi_check(handle);

    uart_tx_wait_blocking(furi_hal_serial_resources[handle->id].periph);

    return true;
}

bool furi_hal_serial_rx_available(FuriHalSerialHandle* handle) {
    furi_check(handle);
    return uart_is_readable(furi_hal_serial_resources[handle->id].periph);
}

uint8_t furi_hal_serial_rx(FuriHalSerialHandle* handle) {
    furi_check(handle);
    uint8_t data = (uint8_t) uart_get_hw(furi_hal_serial_resources[handle->id].periph)->dr;
    return data;
}

void furi_hal_serial_async_rx_start(FuriHalSerialHandle* handle, bool report_errors) {
    furi_check(handle);
    // const FuriHalSerialResources* resources = &furi_hal_serial_resources[handle->id];
    // uart_inst_t* periph = resources->periph;

    // FURI_CRITICAL_ENTER();
    // furi_hal_interrupt_set_isr(resources->irq, furi_hal_serial_irq_callback, handle);
    // LL_USART_EnableIT_RXNE_RXFNE(periph);
    // if(report_errors) {
    //     LL_USART_EnableIT_ERROR(periph);
    // }
    // FURI_CRITICAL_EXIT();
}

void furi_hal_serial_async_rx_stop(FuriHalSerialHandle* handle) {
    furi_check(handle);
    // const FuriHalSerialResources* resources = &furi_hal_serial_resources[handle->id];
    // uart_inst_t* periph = resources->periph;

    // FURI_CRITICAL_ENTER();
    // LL_USART_DisableIT_RXNE_RXFNE(periph);
    // LL_USART_DisableIT_ERROR(resources->periph);
    // furi_hal_interrupt_set_isr(resources->irq, NULL, NULL);
    // FURI_CRITICAL_EXIT();
}

void furi_hal_serial_dma_tx(FuriHalSerialHandle* handle, const uint8_t* buffer, size_t buffer_size) {
    furi_check(handle);

    // FuriHalSerial* serial = furi_hal_serial[handle->id];
    // furi_check(serial);

    // uart_inst_t* periph = serial->periph_ptr;
    // const uint32_t dma_channel = serial->dma_tx_channel;

    // FURI_CRITICAL_ENTER();
    // LL_USART_DisableDMAReq_TX(periph);

    // LL_DMA_DisableChannel(GPDMA1, dma_channel);
    // LL_DMA_SetBlkDataLength(GPDMA1, dma_channel, buffer_size);
    // LL_DMA_SetSrcAddress(GPDMA1, dma_channel, (uint32_t)buffer);
    // LL_DMA_EnableChannel(GPDMA1, dma_channel);

    // LL_USART_EnableDMAReq_TX(periph);
    // FURI_CRITICAL_EXIT();
}

void furi_hal_serial_dma_rx_start(FuriHalSerialHandle* handle, uint8_t* buffer, size_t buffer_size) {
    furi_check(handle);
    furi_check(buffer);
    furi_check(buffer_size);

    // FuriHalSerial* serial = furi_hal_serial[handle->id];
    // furi_check(serial);

    // uart_inst_t* periph = serial->periph_ptr;
    // const uint32_t dma_channel = serial->dma_rx_channel;

    // FURI_CRITICAL_ENTER();
    // LL_USART_DisableDMAReq_RX(periph);

    // LL_DMA_DisableChannel(GPDMA1, dma_channel);
    // LL_DMA_SetBlkDataLength(GPDMA1, dma_channel, buffer_size);
    // LL_DMA_SetDestAddress(GPDMA1, dma_channel, (uint32_t)buffer);
    // LL_DMA_EnableChannel(GPDMA1, dma_channel);

    // LL_USART_EnableDMAReq_RX(periph);
    // FURI_CRITICAL_EXIT();
}

void furi_hal_serial_dma_rx_stop(FuriHalSerialHandle* handle) {
    furi_check(handle);
    // FuriHalSerial* serial = furi_hal_serial[handle->id];
    // furi_check(serial);

    // uart_inst_t* periph = serial->periph_ptr;
    // const uint32_t dma_channel = serial->dma_rx_channel;

    // FURI_CRITICAL_ENTER();
    // LL_USART_DisableDMAReq_RX(periph);
    // LL_DMA_DisableChannel(GPDMA1, dma_channel);
    // FURI_CRITICAL_EXIT();
}

void furi_hal_serial_clear(FuriHalSerialHandle* handle) {
    furi_check(handle);
    uart_inst_t* periph = furi_hal_serial_resources[handle->id].periph;

    // Wait until TX is complete
    uart_tx_wait_blocking(periph);
    // Clear RX buffer
    while(uart_is_readable(periph)) {
        volatile uint8_t dummy = uart_get_hw(periph)->dr;
    }
}


const GpioPin* furi_hal_serial_gpio_get_pin(FuriHalSerialHandle* handle, FuriHalSerialPin pin) {
    furi_hal_serial_check(handle);
    return furi_hal_serial_resources[handle->id].gpio[pin];
}

void furi_hal_serial_set_config(
    FuriHalSerialHandle* handle,
    FuriHalSerialConfigDataBits data_bits,
    FuriHalSerialConfigParity parity,
    FuriHalSerialConfigStopBits stop_bits) {
    furi_hal_serial_check(handle);
    uint32_t data_width = 8;
    uint32_t parity_mode = UART_PARITY_NONE;
    uint32_t stop_bits_mode = 1;
    bool is_enabled = furi_hal_serial_is_enabled(handle);

    switch(data_bits) {
    case FuriHalSerialConfigDataBits5:
        data_width = 5;
        break;
    case FuriHalSerialConfigDataBits6:
        data_width = 6;
        break;
    case FuriHalSerialConfigDataBits7:
        data_width = 7;
        break;
    case FuriHalSerialConfigDataBits8:
        data_width = 8;
        break;
    default:
        furi_crash("Serial: Invalid data bits");
        break;
    }

    switch(parity) {
    case FuriHalSerialConfigParityNone:
        parity_mode = UART_PARITY_NONE;
        break;
    case FuriHalSerialConfigParityEven:
        parity_mode = UART_PARITY_EVEN;
        break;
    case FuriHalSerialConfigParityOdd:
        parity_mode = UART_PARITY_ODD;
        break;
    default:
        furi_crash("Serial: Invalid parity");
        break;
    }

    switch(stop_bits) {
    case FuriHalSerialConfigStopBits_1:
        stop_bits_mode = 1;
        break;
    case FuriHalSerialConfigStopBits_2:
        stop_bits_mode = 2;
        break;
    default:
        furi_crash("Serial: Invalid stop bits");
        break;
    }

    if(is_enabled) {
        furi_hal_serial_suspend(handle);
    }

    uart_set_format(furi_hal_serial[handle->id]->periph_ptr, data_width, stop_bits_mode, parity_mode);

    if(is_enabled) {
        furi_hal_serial_resume(handle);
    }
}

inline bool furi_hal_serial_is_enabled(FuriHalSerialHandle* handle) {
    furi_hal_serial_check(handle);
    // todo: implement check for UART enabled state
    return false;
}
