#pragma once

#include <core/common_defines.h>
//https://www.ti.com/lit/ds/symlink/ina4230.pdf?ts=1775055430940

/* clang-format off */

#define INA4230_CHANNEL_COUNT 4
#define INA4230_CHANNEL_REG_SHIFT 0x08

typedef enum {
    Ina4230RegChannelShuntVoltage   = 0x00,     /**< Shunt Voltage Register */
    Ina4230RegChannelBusVoltage     = 0x01,     /**< Bus Voltage Register */
    Ina4230RegChannelCurrent        = 0x02,     /**< Current Register */
    Ina4230RegChannelPower          = 0x03,     /**< Power Register */
    Ina4230RegChannelEnergy         = 0x04,     /**< Energy Register */
    Ina4230RegChannelCalibration    = 0x05,     /**< Calibration Register */
    Ina4230RegChannelAlertLimit     = 0x06,     /**< Alert Limit Register */
    Ina4230RegChannelAlertConfig    = 0x07,     /**< Alert Configuration Register */

    // Only 4 channels, each channel has the same set of registers with offset addresses INA4230_CHANNEL_REG_SHIFT

    Ina4230RegConfig1                = 0x20,     /**< Configuration Register */
    Ina4230RegConfig2                = 0x21,     /**< Configuration Register */
    Ina4230RegFlag                   = 0x22,     /**< Flag Register */

    Ina4230RegManufacturerID         = 0x7E,     /**< Manufacturer ID Register */
} Ina4230Reg;

typedef struct {
    uint16_t mode           : 3;    // Bits 2-0: MODE R/W b111 Operating mode,modes can be selected to operate the device 
                                    // either in Shutdown mode, continuous mode or triggered mode.The mode also allows 
                                    // user to select mux settings to set continuous or triggered mode on bus voltage, 
                                    // shunt voltage measurement. 
                                    // b000=Shutdown
                                    // b001=Shunt voltage triggered, single shot
                                    // b010=Bus voltage triggered, single shot
                                    // b011=Shunt voltage and Bus voltage triggered, single shot
                                    // b100=Shutdown
                                    // b101=Continuous shunt voltage
                                    // b110=Continuous bus voltage
                                    // b111=Continuous shunt and bus voltage
    uint16_t vshct          : 3;    // Bits 5-3: VSHCT R/W b100 Sets the conversion time of the SHUNT measurement
                                    // b000=140µs
                                    // b001=204µs
                                    // b010=332µs
                                    // b011=588µs
                                    // b100=1100µs
                                    // b101=2116µs
                                    // b110=4156µs
                                    // b111=8244µs
    uint16_t vbusct         : 3;    // Bits 8-6: VBUSCT R/W b100 Sets the conversion time of the VBUS measurement
                                    // b000=140µs
                                    // b001=204µs
                                    // b010=332µs
                                    // b011=588µs
                                    // b100=1100µs
                                    // b101=2116µs
                                    // b110=4156µs
                                    // b111=8244µs
    uint16_t avg            : 3;    // Bits 11-9: AVG R/W b000 Sets the number of ADC conversion results to be averaged. 
                                    // The readback registers are updated after averaging is completed.
                                    // b000=1
                                    // b001=4
                                    // b010=16
                                    // b011=64
                                    // b100=128
                                    // b101=256
                                    // b110=512
                                    // b111=1024
    uint16_t active_channel : 4;    // Bits 15-12: ACTIVE_CHANNEL R/W b1111 These 4 bits determine which channels are active. Set this bit to '1' to enable each channel. 
                                    // Disabled channels are skipped in the round robin cycle.
                                    // Bit15 = Channel 4 measurement enable/disable.
                                    // Bit14 = Channel 3 measurement enable/disable.
                                    // Bit13 = Channel 2 measurement enable/disable.
                                    // Bit12 = Channel 1 measurement enable/disable.
                                    // Power up default: b1111 = All channels active
} Ina4230Config1RegBits;
_Static_assert(
    sizeof(Ina4230Config1RegBits) == 2,
    "Size check for 'Ina4230Config1RegBits' failed.");

typedef struct {
    uint8_t range           : 4;    // Bits 3-0: RANGE R/W b0000 Enables the selection of the shunt full scale input range for each channel.
                                    // Bit3 = Channel 4 range selection.
                                    // Bit2 = Channel 3 range selection.
                                    // Bit1 = Channel 2 range selection.
                                    // Bit0 = Channel 1 range selection.
                                    // range selection bit = 0 selects ±81.92mV
                                    // range selection bit = 1 selects ±20.48mV
                                    // b0000 = all channels set to ±81.92mV range
    uint8_t alert_pol       : 1;    // Bit 4: ALERT_POL R/W b0 When this bit is set to 1, the alert pin toggles from low to high during a 
                                    // fault condition. When set to 0 (default), the alert pin toggles from high to low during faults.
    uint8_t alert_latch     : 1;    // Bit 5: ALERT_LATCH R/W b0 When set to 1 the state of the Alert pin latches during fault conditions. 
                                    // To clear the alert the alert flags register must be read and the fault condition removed.
    uint8_t enof_mask       : 1;    // Bit 6: ENOF_MASK R/W b0 When set to 1, the Alert pin toggles when an energy overflow condition occurs
                                    // on any of the enabled channels
    uint8_t cnvr_mask       : 1;    // Bit 7: CNVR_MASK R/W b0 Setting this bit high configures the ALERT pin to be asserted when conversions 
                                    // are complete. 
                                    // 0b = Disable conversion ready flag on ALERT pin, 
                                    // 1b = Enables conversion ready flag on ALERT pin. 
                                    // ALERT remains asserted until the CVRF field in the flags register is read.
    uint8_t acc_rst         : 4;    // Bits 11-8: ACC_RST R/W b0000 Writing a one to these bits resets the energy registers and clears any overflow flags.
                                    // Bit11 = Channel 4 energy reset, overflow clear. 
                                    // Bit10 = Channel 3 energy reset, overflow clear. 
                                    // Bit9 = Channel 2 energy reset, overflow clear. 
                                    // Bit8 = Channel 1 energy reset, overflow clear. Power up default: 0000b = All channels active Bits are reset back to 0 after write.
    uint8_t                 : 3;    // Bits 14-12: Reserved R 000b These bits always read 0.
    uint8_t rst             : 1;    // Bit 15: RST R/W b0 Set this bit to '1' to generate a system reset that is the same as power-on reset. Resets all registers to default values and then self-clears.

} Ina4230Config2RegBits;
_Static_assert(
    sizeof(Ina4230Config2RegBits) == 2,
    "Size check for 'Ina4230Config2RegBits' failed.");

typedef struct {
    uint16_t shunt_cal      : 15;   // Bits 14-0: SHUNT_CAL R/W 0000h Programmed value needed for doing the shunt voltage to current conversion.
    uint16_t                : 1;    // Bit 15: Reserved R 0h These bits always read 0.
} Ina4230RegChannelCalibrationRegBits;
_Static_assert(
    sizeof(Ina4230RegChannelCalibrationRegBits) == 2,
    "Size check for 'Ina4230RegChannelCalibrationRegBits' failed.");

typedef struct {
    uint16_t alert_mask     : 3;    // Bits 2-0: ALERT_MASK R/W 000b Sets the active alert for the assigned channel
                                    // 000b = reserved, no effect
                                    // 001b = Shunt Voltage over limit (SOL)
                                    // 010b = Shunt Voltage under limit (SUL)
                                    // 011b = Bus Voltage over limit (BOL)
                                    // 100b = Bus Voltage under limit (BUL)
                                    // 101b = Power over limit (POL)
                                    // 110b = reserved, no effect
                                    // 111b = reserved, no effect
    uint16_t channel        : 2;    // Bits 4-3: CHANNEL R/W 00b Selects
                                    // 00b = Channel 1
                                    // 01b = Channel 2
                                    // 10b = Channel 3
                                    // 11b = Channel 4
    uint16_t                : 11;   // Bits 15-5: Reserved R 000000000000b Reserved
} Ina4230RegChannelAlertConfigBits;
_Static_assert(
    sizeof(Ina4230RegChannelAlertConfigBits) == 2,
    "Size check for 'Ina4230RegChannelAlertConfigBits' failed.");


typedef struct {
    int16_t alert_limit      : 16;   // Bits 15-0: ALERT_LIMIT R/W 0000h Sets the alert limit for the assigned channel
} Ina4230RegChannelAlertLimitBits;
_Static_assert(
    sizeof(Ina4230RegChannelAlertLimitBits) == 2,
    "Size check for 'Ina4230RegChannelAlertLimitBits' failed.");

typedef struct {
    int16_t shunt_voltage    : 16;   // Bits 15-0: VSHUNT R 0000h Differential voltage measured across the shunt output. 2's complement value
} Ina4230RegChannelShuntVoltageRegBits;
_Static_assert(
    sizeof(Ina4230RegChannelShuntVoltageRegBits) == 2,
    "Size check for 'Ina4230RegChannelShuntVoltageRegBits' failed.");

typedef struct {
    int16_t bus_voltage     : 16;   // Bits 15-0: VBUS R 0000h Bus voltage output. 2's complement value, however always positive.
} Ina4230RegChannelBusVoltageRegBits;
_Static_assert(
    sizeof(Ina4230RegChannelBusVoltageRegBits) == 2,
    "Size check for 'Ina4230RegChannelBusVoltageRegBits' failed.");

typedef struct {
    int16_t current         : 16;   // Bits 15-0: CURRENT R 0000h Calculated current output in Amperes. 2's complement value.
} Ina4230RegChannelCurrentRegBits;    
_Static_assert(
    sizeof(Ina4230RegChannelCurrentRegBits) == 2,
    "Size check for 'Ina4230RegChannelCurrentRegBits' failed.");


typedef struct {
    uint8_t                 : 6;    // Bits 5-0: Reserved - 000000b Reserved
    uint8_t ovf             : 1;    // Bit 6: OVF (Math Over-flow) R 0b This bit is set to '1' if an arithmetic operation results in an overflow error. 
                                    // This bit indicates that current and power data can be invalid.
    uint8_t cvrf            : 1;    // Bit 7: CVRF (Conversion Ready Flag) R 0b Although the device can be read at any time, and the data from the last 
                                    // conversion is available, the Conversion Ready Flag bit is provided to help coordinate one-shot or triggered conversions. 
                                    // The Conversion Ready Flag bit is set after all conversions, averaging, and multiplications are complete. 
                                    // The Conversion Ready Flag bit clears under the following conditions: 1.) 
                                    // Writing to the Configuration Register (except for Power-Down selection) 2.) Reading the Flags Register
    uint8_t energyof_ch1    : 1;    // Bit 8: ENERGYOF_CH1 R b0 Indicates an the energy register has overflowed for channel 1
    uint8_t energyof_ch2    : 1;    // Bit 9: ENERGYOF_CH2 R b0 Indicates an the energy register has overflowed for channel 2
    uint8_t energyof_ch3    : 1;    // Bit 10: ENERGYOF_CH3 R b0 Indicates an the energy register has overflowed for channel 3
    uint8_t energyof_ch4    : 1;    // Bit 11: ENERGYOF_CH4 R b0 Indicates an the energy register has overflowed for channel 4
    uint8_t limit1_alert    : 1;    // Bit 12: LIMIT1_ALERT R b0 Indicates the first alert limit has been exceeded. This alert is independent of channel.
    uint8_t limit2_alert    : 1;    // Bit 13: LIMIT2_ALERT R b0 Indicates the second alert limit has been exceeded. This alert is independent of channel.
    uint8_t limit3_alert    : 1;    // Bit 14: LIMIT3_ALERT R b0 Indicates the third alert limit has been exceeded. This alert is independent of channel.
    uint8_t limit4_alert    : 1;    // Bit 15: LIMIT4_ALERT R b0 Indicates the fourth alert limit has been exceeded. This alert is independent of channel.
} Ina4230FlagRegBits;
_Static_assert(
    sizeof(Ina4230FlagRegBits) == 2,
    "Size check for 'Ina4230FlagRegBits' failed.");

typedef struct {
    uint16_t manufacture_id; // Bits 15-0: MANUFACTURE_ID R 5449h Reads back TI in ASCII
} Ina4230ManufacturerIDRegBits;
_Static_assert(
    sizeof(Ina4230ManufacturerIDRegBits) == 2,
    "Size check for 'Ina4230ManufacturerIDRegBits' failed.");

/* clang-format on */