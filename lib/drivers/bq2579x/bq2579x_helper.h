#pragma once

typedef enum {
    Bq2579xPowerIdle = 0b00, /** Normal operation (default) */
    Bq2579xPowerShutdown = 0b01, /** Shutdown mode*/
    Bq2579xPowerShipMode = 0b10, /** Ship mode*/
    Bq2579xPowerReset = 0b11, /** System power reset*/
} Bq2579xPowerSwitch;

typedef enum {
    Bq2579xChargerStatus1ChargeNot = 0x0, // Not Charging
    Bq2579xChargerStatus1ChargeTrickle = 0x1, // Trickle Charge
    Bq2579xChargerStatus1ChargePre = 0x2, // Pre-charge
    Bq2579xChargerStatus1ChargeFast = 0x3, // Fast charge (CC mode)
    Bq2579xChargerStatus1ChargeTaper = 0x4, // Taper Charge (CV mode)
    Bq2579xChargerStatus1ChargeTopOff = 0x6, // Top-off Timer Active Charging
    Bq2579xChargerStatus1ChargeTermination = 0x7, // Charge Termination Done
} Bq2579xChargerStatus1Charge;

typedef enum {
    Bq2579xChargerStatus1VbusNoInput = 0x0, // No Input or BHOT or BCOLD in OTG mode
    Bq2579xChargerStatus1VbusSdp = 0x1, // USB SDP (500mA)
    Bq2579xChargerStatus1VbusCdp = 0x2, // USB CDP (1.5A)
    Bq2579xChargerStatus1VbusDcp = 0x3, // USB DCP (3.25A)
    Bq2579xChargerStatus1VbusHVDCP = 0x4, // Adjustable High Voltage DCP (HVDCP) (1.5A)
    Bq2579xChargerStatus1VbusUnknown = 0x5, //Unknown adaptor (3A)
    Bq2579xChargerStatus1VbusNonStandard = 0x6, // Non-Standard Adapter (1A/2A/2.1A/2.4A)
    Bq2579xChargerStatus1VbusOtg = 0x7, // In OTG mode
    Bq2579xChargerStatus1VbusNotQualified = 0x8, // Not qualified adaptor
    Bq2579xChargerStatus1VbusVbus = 0xB, // Device directly powered from VBUS
    Bq2579xChargerStatus1VbusBackup = 0xC, // Backup Mode (BQ25798 only)
} Bq2579xChargerStatus1Vbus;

typedef enum {
    Bq2579xVacOvp26V = 0b00, // 26V (BQ25792 default)
    Bq2579xVacOvp18V22V = 0b01, // 18V on BQ25792, 22V on BQ25798
    Bq2579xVacOvp12V = 0b10, // 12V
    Bq2579xVacOvp7V = 0b11, // 7V (BQ25798 default)
} Bq2579xVacOvp;

typedef enum {
    Bq2579xChargerStatus2IcoDisabled = 0x0, // ICO disabled
    Bq2579xChargerStatus2IcoOptimization = 0x1, // ICO optimization in progress
    Bq2579xChargerStatus2IcoMaximum = 0x2, // Maximum input current detected
} Bq2579xChargerStatus2Ico;


