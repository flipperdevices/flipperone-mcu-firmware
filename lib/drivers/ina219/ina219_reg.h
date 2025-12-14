#pragma once

#include <stdint.h>
//https://ti.com/lit/ds/symlink/ina219.pdf?ts=1764919130911&ref_url=https%253A%252F%252Feu.mouser.com%252F

/* clang-format off */

typedef enum {
    Ina219RegConfig         = 0x00,     /**< Configuration Register */
    Ina219RegShuntVoltage   = 0x01,     /**< Shunt Voltage Register */
    Ina219RegBusVoltage     = 0x02,     /**< Bus Voltage Register */
    Ina219RegPower          = 0x03,     /**< Power Register */
    Ina219RegCurrent        = 0x04,     /**< Current Register */
    Ina219RegCalibration    = 0x05      /**< Calibration Register */
} Ina219Reg;

typedef struct {
    uint8_t mode            : 3;        // Operating Mode (0b000 - Power-Down; 0b001 - Shunt Voltage triggered; 
                                        // 0b010 - Bus Voltage, triggered; 0b011 - Shunt and Bus, Voltage triggered; 0b100 - ADC off (disabled); 
                                        // 0b101 - Shunt Voltage, continuous; 0b110 - Bus Voltage, continuous; 0b111 - Shunt and Bus Voltage, continuous)
    uint8_t sadc            : 4;        // SADC Shunt ADC Resolution/Averaging (0b0000 - 9-bit, 84us; 0b0001 - 10-bit, 148us; 0b0010 - 11-bit, 276us; 0b0011 - 12-bit, 532us; 
                                        // 0b01000 - 12-bit, 532us;  0b1001 - 2 samples averaged, 1.06ms; 0b1010 - 4 samples averaged, 2.13ms; 0b1011 - 8 samples averaged, 4.26ms;
                                        // 0b1100 - 16 samples averaged, 8.51ms; 0b1101 - 32 samples averaged, 17.02ms; 0b1110 - 64 samples averaged, 34.05ms; 0b1111 - 128 samples averaged, 68.10ms)
    uint8_t badc            : 4;        // These bits adjust the Bus ADC resolution (9-; 10-; 11-; or 12-bit) or set the number of samples used when
                                        // averaging results for the Bus Voltage Register (02h). Same encoding as SADC.
    uint8_t pg              : 2;        // Programmable Gain Amplifier Configuration (0b00 - Gain 1, 40mV Range; 0b01 - Gain /2, 80mV Range; 0b10 - Gain /4, 160mV Range; 0b11 - Gain /8, 320mV Range)
    uint8_t brng            : 1;        // Bus Voltage Range (0 = 16V; 1 = 32V(default value))
    uint8_t reserved        : 1;        // Reserved
    uint8_t rst             : 1;        // Reset Bit (1 = Reset INA219, all configuration registers are set to default values)
} Ina219ConfigRegBits;

typedef struct {
    uint8_t ovf             : 1;        // Math Overflow Flag (1 = Overflow occurred)
    uint8_t cnvr            : 1;        // Conversion Ready Flag (1 = Conversion complete)
    uint8_t reserved        : 1;        // Reserved
    uint16_t bus_voltage    : 13;       // Bus Voltage Value (LSB = 4mV)
} Ina219BusVoltageRegBits;

/* clang-format on */