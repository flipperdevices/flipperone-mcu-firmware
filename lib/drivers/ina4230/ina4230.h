#pragma once

#include <furi_hal_i2c_types.h>

/*
INA4230 I2C address can be configured by connecting the ADDR pin to GND, VCC, SDA, or SCL. This results in 4 possible addresses.

Table 6-1. Address Pins and Target Addresses
    A1      A0      TARGET DEVICE ADDRESS
    GND     GND         1000000 = 0x40
    GND     VS          1000001 = 0x41
    GND     SDA         1000010 = 0x42
    GND     SCL         1000011 = 0x43
    VS      GND         1000100 = 0x44
    VS      VS          1000101 = 0x45
    VS      SDA         1000110 = 0x46
    VS      SCL         1000111 = 0x47
    SDA     GND         1001000 = 0x48
    SDA     VS          1001001 = 0x49
    SDA     SDA         1001010 = 0x4A
    SDA     SCL         1001011 = 0x4B
    SCL     GND         1001100 = 0x4C
    SCL     VS          1001101 = 0x4D
    SCL     SDA         1001110 = 0x4E
    SCL     SCL         1001111 = 0x4F
*/
typedef struct Ina4230 Ina4230;

typedef enum {
    Ina4230Gain81_92mV = 0b0,
    Ina4230Gain20_48mV = 0b1,
} Ina4230AdcRange;

typedef enum {
    Ina4230AdcConvTime140us = 0b000,
    Ina4230AdcConvTime204us = 0b001,
    Ina4230AdcConvTime332us = 0b010,
    Ina4230AdcConvTime588us = 0b011,
    Ina4230AdcConvTime1100us = 0b100,
    Ina4230AdcConvTime2116us = 0b101,
    Ina4230AdcConvTime4156us = 0b110,
    Ina4230AdcConvTime8244us = 0b111,
} Ina4230AdcConvTime;

typedef enum {
    Ina4230ActiveChannel1 = 0b0001,
    Ina4230ActiveChannel2 = 0b0010,
    Ina4230ActiveChannel3 = 0b0100,
    Ina4230ActiveChannel4 = 0b1000,
} Ina4230ActiveChannels;

typedef enum {
    Ina4230AvgSamples1 = 0b000,
    Ina4230AvgSamples4 = 0b001,
    Ina4230AvgSamples16 = 0b010,
    Ina4230AvgSamples64 = 0b011,
    Ina4230AvgSamples128 = 0b100,
    Ina4230AvgSamples256 = 0b101,
    Ina4230AvgSamples512 = 0b110,
    Ina4230AvgSamples1024 = 0b111,
} Ina4230AvgSamples;

typedef enum {
    Ina4230ModeShutdown = 0b000,
    Ina4230ModeShuntTrig = 0b001,
    Ina4230ModeBusTrig = 0b010,
    Ina4230ModeShuntBusTrig = 0b011,
    Ina4230ModeAdcOff = 0b100,
    Ina4230ModeShuntCont = 0b101,
    Ina4230ModeBusCont = 0b110,
    Ina4230ModeShuntBusCont = 0b111,
} Ina4230Mode;

typedef enum {
    Ina4230AlertMaskNone = 0b000,
    Ina4230AlertMaskShuntOverVoltage = 0b001,
    Ina4230AlertMaskShuntUnderVoltage = 0b010,
    Ina4230AlertMaskBusOverVoltage = 0b011,
    Ina4230AlertMaskBusUnderVoltage = 0b100,
    Ina4230AlertMaskPowerOverLimit = 0b101,
    Ina4230AlertMaskReserved1 = 0b110,
    Ina4230AlertMaskReserved2 = 0b111,
} Ina4230AlertMask;

#ifdef __cplusplus
extern "C" {
#endif

Ina4230* ina4230_init(const FuriHalI2cBusHandle* i2c_handle, uint8_t address);
void ina4230_deinit(Ina4230* instance);
void ina4230_set_config(Ina4230* instance, Ina4230AdcConvTime shunt_conv_time, Ina4230AdcConvTime bus_conv_time, Ina4230AvgSamples avg, Ina4230Mode mode);
void ina4230_set_config_channel(Ina4230* instance, uint32_t channel, const char* name, float shunt_resistance_om, float max_expected_current_a);
void ina4230_set_power_down(Ina4230* instance, bool power_down);
float ina4230_get_power_w(Ina4230* instance, uint32_t channel);
float ina4230_get_energy_(Ina4230* instance, uint32_t channel);
float ina4230_get_bus_voltage_v(Ina4230* instance, uint32_t channel);
float ina4230_get_shunt_voltage_mv(Ina4230* instance, uint32_t channel);
float ina4230_get_current_a(Ina4230* instance, uint32_t channel);
const char* ina4230_get_channel_name(Ina4230* instance, uint32_t channel);

#ifdef __cplusplus
}
#endif
