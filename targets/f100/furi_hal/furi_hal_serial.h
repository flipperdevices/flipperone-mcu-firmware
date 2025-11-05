/**
 * @file furi_hal_serial.h
 *
 * Serial HAL API
 */
#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

#include <furi_hal_serial_types.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Serial hardware flow control modes */
typedef enum {
    FuriHalSerialHwFlowControlNone, /**< Do not use any hardware flow control */
    FuriHalSerialHwFlowControlRts, /**< Use the Request to send (RTS) signal only */
    FuriHalSerialHwFlowControlCts, /**< Use the Clear to send (CTS) signal only */
    FuriHalSerialHwFlowControlRtsCts, /**< Use both RTS and CTS signals */
} FuriHalSerialHwFlowControl;

/** Serial RX events */
typedef enum {
    FuriHalSerialRxEventData = (1 << 0), /**< Data: new data available */
    FuriHalSerialRxEventIdle = (1 << 1), /**< Idle: bus idle detected */
    FuriHalSerialRxEventFrameError = (1 << 2), /**< Framing Error: incorrect frame detected */
    FuriHalSerialRxEventNoiseError = (1 << 3), /**< Noise Error: noise on the line detected */
    FuriHalSerialRxEventParityError = (1 << 4), /**< Parity Error: incorrect parity detected */
    FuriHalSerialRxEventOverrunError = (1 << 5), /**< Overrun Error: no space for received data */
} FuriHalSerialRxEvent;

/** Serial TX events */
typedef enum {
    FuriHalSerialTxEventComplete = (1 << 0), /**< Transmission complete */
} FuriHalSerialTxEvent;

// typedef enum {
//     FuriHalSerialAutoBaudRateModeStartBit,
//     FuriHalSerialAutoBaudRateModeFallingEdge,
//     FuriHalSerialAutoBaudRateMode0x7FFrame,
//     FuriHalSerialAutoBaudRateMode0x55Frame,
// } FuriHalSerialAutoBaudRateMode;

/**
 * Receive callback function type.
 *
 * @param handle Pointer to the serial handle.
 * @param event Bitmask of occurred events.
 * @param context Pointer to the context object.
 */
typedef void (*FuriHalSerialRxCallback)(
    FuriHalSerialHandle* handle,
    FuriHalSerialRxEvent event,
    void* context);

/**
 * Transmit callback function type.
 *
 * @param handle Pointer to the serial handle.
 * @param event Bitmask of occurred events.
 * @param context Pointer to the context object.
 */
typedef void (*FuriHalSerialTxCallback)(
    FuriHalSerialHandle* handle,
    FuriHalSerialTxEvent event,
    void* context);

/**
 * Initialize the serial interface.
 *
 * @param handle Pointer to the serial handle.
 * @param baud_rate Baud rate.
 *      @arg min: 10UL
 *      @arg max: 7800000UL
 */
void furi_hal_serial_init(FuriHalSerialHandle* handle, uint32_t baud_rate);

/**
 * Deinitialize the serial interface.
 *
 * @param handle Pointer to the serial handle.
 */
void furi_hal_serial_deinit(FuriHalSerialHandle* handle);

/**
 * Suspend the serial interface.
 * @param handle Pointer to the serial handle.
 */
void furi_hal_serial_suspend(FuriHalSerialHandle* handle);

/**
 * Resume the serial interface.
 * @param handle Pointer to the serial handle.
 */
void furi_hal_serial_resume(FuriHalSerialHandle* handle);

/**
 * @brief Determine whether a certain baud rate is supported
 *
 * @param      handle  Serial handle
 * @param      baud_rate Baud rate to be checked
 * @returns    true if baud rate is supported, false otherwise.
 */
bool furi_hal_serial_is_baud_rate_supported(FuriHalSerialHandle* handle, uint32_t baud_rate);

/**
 * Get the baud rate for the serial interface.
 * @param handle Pointer to the serial handle.
 * @returns Baud rate.
 */
uint32_t furi_hal_serial_get_baud_rate(FuriHalSerialHandle* handle);

/**
 * Set the baud rate for the serial interface.
 *
 * @param handle Pointer to the serial handle.
 * @param baud_rate Baud rate.
 *      @arg min: 10UL
 *      @arg max: 7800000UL
 */
void furi_hal_serial_set_baud_rate(FuriHalSerialHandle* handle, uint32_t baud_rate);

/**
 * Set the hardware flow control mode for the serial interface.
 *
 * @param handle Pointer to the serial handle.
 * @param flow_control Flow control mode.
 */
void furi_hal_serial_set_hw_flow_control(
    FuriHalSerialHandle* handle,
    FuriHalSerialHwFlowControl flow_control);

/**
 * Set the callback functions for the serial interface.
 *
 * @param handle Pointer to the serial handle.
 * @param tx_callback Pointer to the transmit callback function.
 * @param rx_callback Pointer to the receive callback function.
 * @param context Pointer to the context object.
 */
void furi_hal_serial_set_callback(
    FuriHalSerialHandle* handle,
    FuriHalSerialTxCallback tx_callback,
    FuriHalSerialRxCallback rx_callback,
    void* context);

/* Blocking API */

/**
 * Put data into the UART transmit FIFO buffer
 *
 * @param handle Pointer to the serial handle.
 * @param buffer Pointer to the data buffer.
 * @param buffer_size Size of the data buffer.
 * @param timeout Timeout in milliseconds, or `FuriWaitForever`.
 * 
 * @return Number of bytes put into the buffer within the specified timeout
 */
size_t furi_hal_serial_tx(
    FuriHalSerialHandle* handle,
    const uint8_t* buffer,
    size_t buffer_size,
    uint32_t timeout);

/**
 * Wait for the UART transmit FIFO buffer to empty, meaning all bytes have been
 * transmitted
 *
 * @param handle Pointer to the serial handle.
 * @param timeout Timeout in milliseconds.
 * @returns true if the transmission was complete within the timeout, false otherwise
 */
bool furi_hal_serial_tx_wait_complete(FuriHalSerialHandle* handle, uint32_t timeout);

/**
 * Determine whether there is received data ready for reading.
 *
 * @param handle Pointer to the serial handle.
 * @returns true if there is data ready to be read, false otherwise.
 */
bool furi_hal_serial_rx_available(FuriHalSerialHandle* handle);

/**
 * Read the next received data character.
 *
 * @param handle Pointer to the serial handle.
 * @returns the value of the first available received character.
 */
uint8_t furi_hal_serial_rx(FuriHalSerialHandle* handle);

/* Interrupt-based asynchronous API */

/**
 * Start receiving in interrupt-driven mode.
 *
 * @param handle Pointer to the serial handle.
 * @param report_errors Enable error events if set to true, do not enable otherwise.
 */
void furi_hal_serial_async_rx_start(FuriHalSerialHandle* handle, bool report_errors);

/**
 * Stop receiving in interrupt-driven mode.
 *
 * @param handle Pointer to the serial handle.
 */
void furi_hal_serial_async_rx_stop(FuriHalSerialHandle* handle);

/* DMA-based asynchronous API */

/**
 * Transmit data over the serial interface using DMA.
 *
 * @param handle Pointer to the serial handle.
 * @param buffer Pointer to the data buffer.
 * @param buffer_size Size of the data buffer.
 */
void furi_hal_serial_dma_tx(FuriHalSerialHandle* handle, const uint8_t* buffer, size_t buffer_size);

/**
 * Start receiving in DMA mode.
 *
 * @note In this mode, the receive event will only be generated once the whole buffer has been filled.
 *
 * @param handle Pointer to the serial handle.
 * @param buffer Pointer to the receive buffer (must be allocated and of appropriate size).
 * @param buffer_size Size of the receive buffer.
 */
void furi_hal_serial_dma_rx_start(FuriHalSerialHandle* handle, uint8_t* buffer, size_t buffer_size);

/**
 * Stop receiving in DMA mode.
 *
 * @param handle Pointer to the serial handle.
 */
void furi_hal_serial_dma_rx_stop(FuriHalSerialHandle* handle);

/* Misc functions */

/**
 * Clear pending characters in (a) specified direction(s).
 *
 * @param handle Pointer to the serial handle.
 */
void furi_hal_serial_clear(FuriHalSerialHandle* handle);

#ifdef __cplusplus
}
#endif
