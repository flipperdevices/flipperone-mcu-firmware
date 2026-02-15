#pragma once

#include <furi_hal_i2c_types.h>
#include <furi_hal_gpio.h>

#define FUSB302_ADDRESS 0x22

typedef struct Fusb302 Fusb302;
typedef void (*Fusb302Callback)(void* context);

typedef enum {
    Fusb302StatusOk = 0,
    Fusb302StatusRxEmpty,
    Fusb302StatusTxEmpty,
    Fusb302StatusError = -1,
    Fusb302StatusTimeout = -2,
    Fusb302StatusUnknown = -3,
} Fusb302Status;

/**
 * List of possible destinations.
 * @see Table 6-4 Destination.
 */
typedef enum {
    Fusb302PdSopTypeDefault,
    Fusb302PdSopTypePrime,
    Fusb302PdSopTypePrimeDouble,
    Fusb302PdSopTypeDebug,
    Fusb302PdSopTypeDebugDouble,
    Fusb302PdSopTypeUnknown,
} Fusb302PdSopType;

typedef enum {
    Fusb302TypeCcOrientationNone, //!< No cable connected
    Fusb302TypeCcOrientationNormal, //!< Cable is plugged in normally (CC1 active)
    Fusb302TypeCcOrientationReverse, //!< Cable is plugged in upside-down (CC2 active)
} Fusb302TypeCcOrientation;

/**
 * USB Power Delivery message structure.
 *
 * This structure represents a complete USB PD message including:
 * - SOP type (destination: device, cable plug prime, or cable plug double prime)
 * - Message header containing message type, data role, power role, etc.
 * - Optional data objects (up to 7 objects)
 *
 * @see Section 6.2 of the USB Power Delivery Specification
 */
typedef struct {
    Fusb302PdSopType sop_type;
    uint16_t header;
    uint32_t objects[7];
    uint8_t object_count;
} Fusb302PdMsg;

#ifdef __cplusplus
extern "C" {
#endif

Fusb302* fusb302_init(const FuriHalI2cBusHandle* i2c_handle, uint8_t address, const GpioPin* pin_interrupt);
void fusb302_read_cc_status(Fusb302* instance, uint8_t cc);
void fusb302_deinit(Fusb302* instance);
bool fusb302_read_role(Fusb302* instance);
void fusb302_set_input_callback(Fusb302* instance, Fusb302Callback callback, void* context);

Fusb302Status fusb302_cc_orientation_set(Fusb302* instance, Fusb302TypeCcOrientation orientation);
// pd
Fusb302Status fusb302_pd_reset_logic(Fusb302* instance);
Fusb302Status fusb302_pd_reset_hard(Fusb302* instance);
Fusb302Status fusb302_pd_autogoodcrc_set(Fusb302* instance, bool enabled);
Fusb302Status fusb302_pd_autoretry_set(Fusb302* instance, int retries);
Fusb302Status fusb302_pd_rx_flush(Fusb302* instance);
Fusb302Status fusb302_pd_tx_flush(Fusb302* instance);
Fusb302Status fusb302_pd_message_receive(Fusb302* instance, Fusb302PdMsg* msg);
Fusb302Status fusb302_pd_message_send(Fusb302* instance, Fusb302PdMsg* msg);

#ifdef __cplusplus
}
#endif
