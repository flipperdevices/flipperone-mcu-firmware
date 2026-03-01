#pragma once

typedef enum {
    Bq25792PowerIdle = 0b00, /** Normal operation (default) */
    Bq25792PowerShutdown = 0b01, /** Shutdown mode*/
    Bq25792PowerShipMode = 0b10, /** Ship mode*/
    Bq25792PowerReset = 0b11, /** System power reset*/
} Bq25792PowerSwitch;

typedef enum {
    Bq25792ChargerStatus1ChargeNot = 0x0, // Not Charging
    Bq25792ChargerStatus1ChargeTrickle = 0x1, // Trickle Charge
    Bq25792ChargerStatus1ChargePre = 0x2, // Pre-charge
    Bq25792ChargerStatus1ChargeFast = 0x3, // Fast charge (CC mode)
    Bq25792ChargerStatus1ChargeTaper = 0x4, // Taper Charge (CV mode)
    Bq25792ChargerStatus1ChargeTopOff = 0x6, // Top-off Timer Active Charging
    Bq25792ChargerStatus1ChargeTermination = 0x7, // Charge Termination Done
} Bq25792ChargerStatus1Charge;

typedef enum {
    Bq25792ChargerStatus1VbusNoInput = 0x0, // No Input or BHOT or BCOLD in OTG mode
    Bq25792ChargerStatus1VbusSdp = 0x1, // USB SDP (500mA)
    Bq25792ChargerStatus1VbusCdp = 0x2, // USB CDP (1.5A)
    Bq25792ChargerStatus1VbusDcp = 0x3, // USB DCP (3.25A)
    Bq25792ChargerStatus1VbusHVDCP = 0x4, // Adjustable High Voltage DCP (HVDCP) (1.5A)
    Bq25792ChargerStatus1VbusUnknown = 0x5, //Unknown adaptor (3A)
    Bq25792ChargerStatus1VbusNonStandard = 0x6, // Non-Standard Adapter (1A/2A/2.1A/2.4A)
    Bq25792ChargerStatus1VbusOtg = 0x7, // In OTG mode
    Bq25792ChargerStatus1VbusNotQualified = 0x8, // Not qualified adaptor
    Bq25792ChargerStatus1VbusVbus = 0xB, // Device directly powered from VBUS
} Bq25792ChargerStatus1Vbus;

typedef enum {
    Bq25792ChargerStatus2IcoDisabled = 0x0, // ICO disabled
    Bq25792ChargerStatus2IcoOptimization = 0x1, // ICO optimization in progress
    Bq25792ChargerStatus2IcoMaximum = 0x2, // Maximum input current detected
} Bq25792ChargerStatus2Ico;


