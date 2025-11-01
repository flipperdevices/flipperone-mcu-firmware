#pragma once

#include <stdint.h>

//https://www.ti.com/lit/ds/symlink/tps62869.pdf

#define TPS62868X_REG_1 0x01 // Sets the target output voltage
#define TPS62868X_REG_2 0x02 // Sets the target output voltage
#define TPS62868X_REG_CONTROL 0x03 //Sets miscellaneous configuration bits
#define TPS62868X_REG_STATUS 0x05 //Returns status flags


/* VOUT TPS62868X_REG_1, TPS62868X_REG_2
 *
 * VOx_SET [0x00 .. 0xFF]
 *
 * Vout = TPS62868X_VOLTAGE_FACTOR * (0.4v + (VOx_SET*0.005v))
 *
 */

typedef struct {

    uint8_t V_RAMP_SPEED : 2; // 00 - 20mV/µs (0.25 µs/step),    01 - 10 mV/µs (0.5 µs/step),    10 - 5 mV/µs (1 µs/step),    11 - 1 mV/µs (5 µs/step, default).
    uint8_t EN_HICCUP : 1; // 0 - Disable HICCUP. Enable latching protection,    1 - Enable HICCUP, Disable latching protection.
    uint8_t EN_OUT_DISCHARGE : 1; // 0 - Disable output discharge,    1 - Enable output discharge
    uint8_t EN_FPWM_MODE : 1; // 0 - Set the device in power save mode at light loads,    1 - Set the device in forced PWM mode at light loads.
    uint8_t SOFT_EN_DEVICE : 1; // 0 - Disable the device. All registers values are still kept,    1 - Re-enable the device with a new start-up without the tDelay period.
    uint8_t EN_FPWM_MODE_OUT_V_CHANGE : 1; // 0 - Keep the current mode status during output voltage change,    1 - Force the device in FPWM during output voltage change.
    uint8_t RESET : 1; // 1 - Reset all registers to default

} TPS62868XControl;

typedef struct {

    uint8_t UVLO : 1; // 1: The input voltage is less than UVLO threshold (falling edge).
    uint8_t RESERVED_1 : 1;
    uint8_t RESERVED_2 : 1;
    uint8_t HICCUP : 1; // 1: Device has HICCUP status once.
    uint8_t THERMINAL_WARNING : 1; // 1: Junction temperature is higher than 130°C
    uint8_t RESERVED_3 : 3;

} TPS62868XStatus;