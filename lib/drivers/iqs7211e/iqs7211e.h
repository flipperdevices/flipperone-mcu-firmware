#pragma once

#include <furi_hal_i2c_types.h>
#include <furi_hal_gpio.h>

#define IQS7211E_ADDRESS 0x56

typedef struct Iqs7211e Iqs7211e;
typedef void (*Iqs7211eCallbackInput)(void* context);
typedef void (*Iqs7211eCallbackEvent)(void* context);

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    Iqs7211eChargingModeActive = 0b000,
    Iqs7211eChargingModeIdleTouch = 0b001,
    Iqs7211eChargingModeIdle = 0b010,
    Iqs7211eChargingModeLP1 = 0b011,
    Iqs7211eChargingModeLP2 = 0b100,
    Iqs7211eChargingUnknown = 0xFF,
} Iqs7211eChargingMode;

typedef enum {
    Iqs7211eEventNone = 0,
    Iqs7211eEventSingleTap = (1 << 0),
    Iqs7211eEventDoubleTap = (1 << 1),
    Iqs7211eEventTripleTap = (1 << 2),
    Iqs7211eEventPressAndHold = (1 << 3),
    Iqs7211eEventPalmGesture = (1 << 4),
    Iqs7211eEventSwipeXPositive = (1 << 8),
    Iqs7211eEventSwipeXNegative = (1 << 9),
    Iqs7211eEventSwipeYPositive = (1 << 10),
    Iqs7211eEventSwipeYNegative = (1 << 11),
    Iqs7211eEventHoldXPositive = (1 << 12),
    Iqs7211eEventHoldXNegative = (1 << 13),
    Iqs7211eEventHoldYPositive = (1 << 14),
    Iqs7211eEventHoldYNegative = (1 << 15),
} Iqs7211eEvent;

Iqs7211e* iqs7211e_init(const FuriHalI2cBusHandle* i2c_handle, const GpioPin* pin_rdy, uint8_t address);
void iqs7211e_deinit(Iqs7211e* instance);
void iqs7211e_run(Iqs7211e* instance);
void iqs7211e_set_input_callback(Iqs7211e* instance, Iqs7211eCallbackInput callback, Iqs7211eCallbackEvent event_callback, void* context);
void iqs7211e_force_i2c_communication(Iqs7211e* instance);
void iqs7211e_reset(Iqs7211e* instance);
Iqs7211eChargingMode iqs7211e_get_charging_mode(Iqs7211e* instance);
uint16_t iqs7211e_get_abs_x_fingers_num(Iqs7211e* instance, uint8_t finger_num);
uint16_t iqs7211e_get_abs_y_fingers_num(Iqs7211e* instance, uint8_t finger_num);
bool iqs7211e_get_touchpad_event_occurred(Iqs7211e* instance);
Iqs7211eEvent iqs7211e_get_event(Iqs7211e* instance);
uint8_t iqs7211e_get_fingers_num(Iqs7211e* instance);
bool iqs7211e_get_ready(Iqs7211e* instance);



#ifdef __cplusplus
}
#endif
