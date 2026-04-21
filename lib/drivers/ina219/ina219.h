#pragma once

#include <furi_hal_i2c_types.h>
#include <furi_hal_gpio.h>

#define INA219_ADDRESS 0x45

typedef struct Ina219 Ina219;
typedef void (*Ina219CallbackInput)(void* context);

typedef enum{
    Ina219Range16V = 0b0,
    Ina219Range32V = 0b1,
} Ina219Range;

typedef enum{
    Ina219Gain40mV = 0b00,
    Ina219Gain80mV = 0b01,
    Ina219Gain160mV = 0b10,
    Ina219Gain320mV = 0b11,
} Ina219Gain;

typedef enum{
    Ina219BusRes9bit = 0b0000,
    Ina219BusRes10bit = 0b0001,
    Ina219BusRes11bit = 0b0010,
    Ina219BusRes12bit = 0b0011,
} Ina219BusRes;


typedef enum{
    Ina219ShuntRes9bit1S84ms = 0b0000,
    Ina219ShuntRes10bit1S148ms = 0b0001,
    Ina219ShuntRes11bit1S276ms = 0b0010,
    Ina219ShuntRes12bit1S532ms = 0b0011,
    Ina219ShuntRes12bit2S1060ms = 0b1001,
    Ina219ShuntRes12bit4S2130ms = 0b1010,
    Ina219ShuntRes12bit8S4260ms = 0b1011,
    Ina219ShuntRes12bit16S8510ms = 0b1100,
    Ina219ShuntRes12bit32S17020ms = 0b1101,
    Ina219ShuntRes12bit64S34050ms = 0b1110,
    Ina219ShuntRes12bit128S68100ms = 0b1111,
} Ina219ShuntRes;

typedef enum{
    Ina219ModePowerDown = 0b000,
    Ina219ModeShuntTrig = 0b001,
    Ina219ModeBusTrig = 0b010,
    Ina219ModeShuntBusTrig = 0b011,
    Ina219ModeAdcOff = 0b100,
    Ina219ModeShuntCont = 0b101,
    Ina219ModeBusCont = 0b110,
    Ina219ModeShuntBusCont = 0b111,
} Ina219Mode;

#ifdef __cplusplus
extern "C" {
#endif

Ina219* ina219_init(const FuriHalI2cBusHandle* i2c_handle, uint8_t address, float shunt_resistance_om, float max_expected_current_a);
void ina219_deinit(Ina219* instance);
void ina219_set_config(Ina219* instance, Ina219Range range, Ina219BusRes bus_res, Ina219ShuntRes shunt_res, Ina219Mode mode);
void ina219_set_power_down(Ina219* instance, bool power_down);
float ina219_get_power_w(Ina219* instance);
float ina219_get_bus_voltage_v(Ina219* instance);
float ina219_get_shunt_voltage_mv(Ina219* instance);
float ina219_get_current_a(Ina219* instance);

#ifdef __cplusplus
}
#endif
