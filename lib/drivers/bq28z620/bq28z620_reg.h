#pragma once

//https://www.ti.com/lit/gpn/bq28z620
//https://www.ti.com/lit/pdf/sluuco9

#include <core/common_defines.h>

/* clang-format off */

typedef enum {
    Bq28z620StdCmdManufacturerAccessAndControlStatus = 0x00,
    Bq28z620StdCmdAtRate = 0x02,
    Bq28z620StdCmdAtRateTimeToEmpty = 0x04,
    Bq28z620StdCmdTemperature = 0x06,
    Bq28z620StdCmdVoltage = 0x08,
    Bq28z620StdCmdBatteryStatus = 0x0A,
    Bq28z620StdCmdCurrent = 0x0C,
    Bq28z620StdCmdRemainingCapacity = 0x10,
    Bq28z620StdCmdFullChargeCapacity = 0x12,
    Bq28z620StdCmdAverageCurrent = 0x14,
    Bq28z620StdCmdAverageTimeToEmpty = 0x16,
    Bq28z620StdCmdAverageTimeToFull = 0x18,
    Bq28z620StdCmdStandbyCurrent = 0x1A,
    Bq28z620StdCmdStandbyTimeToEmpty = 0x1C,
    Bq28z620StdCmdMaxLoadCurrent = 0x1E,
    Bq28z620StdCmdMaxLoadTimeToEmpty = 0x20,
    Bq28z620StdCmdAveragePower = 0x22,
    Bq28z620StdCmdInternalTemperature = 0x28,
    Bq28z620StdCmdCycleCount = 0x2A,
    Bq28z620StdCmdRelativeStateOfCharge = 0x2C,
    Bq28z620StdCmdStateOfHealth = 0x2E,
    Bq28z620StdCmdChargeVoltage = 0x30,
    Bq28z620StdCmdChargeCurrent = 0x32,
    Bq28z620StdCmdDesignCapacity = 0x3C,
    Bq28z620StdCmdMACSubcmd = 0x3E,
    Bq28z620StdCmdMACData = 0x40,
    Bq28z620StdCmdMACDataSum = 0x60,
    Bq28z620StdCmdMACDataLen = 0x61,
} Bq28z620StdCmd;

typedef enum {
    Bq28z620MacSubcmdDeviceType = 0x0001,
    Bq28z620MacSubcmdFirmwareVersion = 0x0002,
    Bq28z620MacSubcmdHardwareVersion = 0x0003,
    Bq28z620MacSubcmdIFChecksum = 0x0004,
    Bq28z620MacSubcmdStaticDFSignature = 0x0005,
    Bq28z620MacSubcmdChemID = 0x0006,
    Bq28z620MacSubcmdPrevMacWrite = 0x0007,
    Bq28z620MacSubcmdStaticChemDFSignature = 0x0008,
    Bq28z620MacSubcmdAllDFSignature = 0x0009,
    Bq28z620MacSubcmdShutdownMode = 0x0010,                 /** See: 14.2.10 MACSubcmd() 0x0010 SHUTDOWN Mode */
    Bq28z620MacSubcmdSleepMode = 0x011,                     /** See: 14.2.11 MACSubcmd() 0x0011 SLEEP Mode */
    Bq28z620MacSubcmdReset = 0x012,                         /** See: 14.2.12 MACSubcmd() 0x0012 Device Reset */ 
    Bq28z620MacSubcmdAutoCalMAC = 0x013,                    
    Bq28z620MacSubcmdChargeFET = 0x001F,                    /** See: 14.2.13 MACSubcmd() 0x001F CHG FET */
    Bq28z620MacSubcmdDischargeFET = 0x0020,                 /** See: 14.2.14 MACSubcmd() 0x0020 DSG FET */
    Bq28z620MacSubcmdGaugingItEnable = 0x0021,              /** See: 14.2.15 MACSubcmd() 0x0021 Gauging */
    Bq28z620MacSubcmdFETControl = 0x0022,                   /** See: 14.2.16 MACSubcmd() 0x0022 FET Control */
    Bq28z620MacSubcmdLifetimeDataCollection = 0x0023,       /** See: 14.2.17 MACSubcmd() 0x0023 Lifetime Data Collection */
    Bq28z620MacSubcmdPermanentFailure = 0x0024,             /** See: 14.2.18 MACSubcmd() 0x0024 Permanent Failure */
    Bq28z620MacSubcmdLifetimeDataReset = 0x0028,            /** See: 14.2.19 MACSubcmd() 0x0028 Lifetime Data Reset */        
    Bq28z620MacSubcmdPermanentFailureDataReset = 0x0029,    /** See: 14.2.20 MACSubcmd() 0x0029 Permanent Fail Data Reset */
    Bq28z620MacSubcmdCalibrationMode = 0x002D,              /** See: 14.2.21 MACSubcmd() 0x002D CALIBRATION Mode */
    Bq28z620MacSubcmdLifetimeDataFlush = 0x002E,
    Bq28z620MacSubcmdLifetimeDataTest = 0x002F,
    Bq28z620MacSubcmdSealDevice = 0x0030,                   /** See: 14.2.22 MACSubcmd() 0x0030 Seal Device */
    Bq28z620MacSubcmdSecurityKeys = 0x0035,                 /** See: 14.2.23 MACSubcmd() 0x0035 Security Keys */
    Bq28z620MacSubcmdAuthenticationKey = 0x0037,
    Bq28z620MacSubcmdReset2 = 0x0041,                       /** See: 14.2.25 MACSubcmd() 0x0041 Device Reset */
    Bq28z620MacSubcmdDeviceName = 0x004A,
    Bq28z620MacSubcmdDeviceChem = 0x004B,
    Bq28z620MacSubcmdManufacturerName = 0x004C,
    Bq28z620MacSubcmdManufacturerDate = 0x004D,
    Bq28z620MacSubcmdSerialNumber = 0x004E,
    Bq28z620MacSubcmdSafetyAlert = 0x0050,
    Bq28z620MacSubcmdSafetyStatus = 0x0051,
    Bq28z620MacSubcmdPFAlert = 0x0052,
    Bq28z620MacSubcmdPFStatus = 0x0053,
    Bq28z620MacSubcmdOperationStatus = 0x0054,
    Bq28z620MacSubcmdChargingStatus = 0x0055,
    Bq28z620MacSubcmdGaugingStatus = 0x0056,
    Bq28z620MacSubcmdManufacturingStatus = 0x0057,
    Bq28z620MacSubcmdAFERegister = 0x0058,
    Bq28z620MacSubcmdLifetimeDataBlock1 = 0x0060,
    Bq28z620MacSubcmdManufacturerData = 0x0070,
    Bq28z620MacSubcmdDAStatus1 = 0x0071,
    Bq28z620MacSubcmdDAStatus2 = 0x0072,
    Bq28z620MacSubcmdITStatus1 = 0x0073,
    Bq28z620MacSubcmdITStatus2 = 0x0074,
    Bq28z620MacSubcmdITStatus3 = 0x0075,
    Bq28z620MacSubcmdCBStatus = 0x0076,
    Bq28z620MacSubcmdFCCSOH = 0x0077,
    //Bq28z620MacSubcmdDFAccessRowAddress = 0x01yy,         //yy is the row address
    Bq28z620MacSubcmdROMMode = 0x0F00,                      /** See: 14.2.49 MACSubcmd() 0x0F00 ROM Mode */
    Bq28z620MacSubcmdExitCalibrationOutput = 0xF080,        /** See: 14.2.51 MACSubcmd() 0xF080 Exit Calibration Output Mode */
    Bq28z620MacSubcmdOutputCCandADCforCalibration = 0xF081,
    Bq28z620MacSubcmdOutputShortedCCandADCforCalibration = 0xF082,
} Bq28z620SubCmd;


/*14.1.1 0x00/01 ManufacturerAccess() andControlStatus()*/
typedef struct {
    uint8_t qmax            : 1; //Bit 0: QMax updates. This bit toggles after every QMax update
    uint8_t vok             : 1; //Bit 1: Voltage OK for QMax updates. 1 = Detected, 0 = Not detected
    uint8_t r_dis           : 1; //Bit 2: Resistance updates. 1 = Disabled, 0 = Enabled
    uint8_t ldmd            : 1; //Bit 3: LOAD mode. 1 = Constant oower, 0 = Constant current
    uint8_t                 : 4; //Bits 7–4: Reserved
    uint8_t                 : 1; //Bit 8: Reserved
    uint8_t checksum_valid  : 1; //Bit 9: Checksum is valid. 1 = Flash Writes are enabled, 0 = Flash Writes are disabled due to low voltage or PF condition.
    uint8_t                 : 2; //Bits 11–10: Reserved
    uint8_t authcalm        : 1; //Bit 12: Automatic CALIBRATION mode. 1 = Enabled, 0 = Disabled
    uint8_t sec             : 2; //Bits 14–13: SECURITY mode 0b00 = Reserved, 0b01 = Full Access, 0b10 = Unsealed, 0b11 = Sealed
    uint8_t                 : 1; //Bit 15: Reserved
} Bq28z620StdCmdControlStatusRegBits;
_Static_assert(
    sizeof(Bq28z620StdCmdControlStatusRegBits) == 2,
    "Size check for 'Bq28z620StdCmdControlStatusRegBits' failed.");

/*14.1.3 0x02/03 AtRate()*/
typedef struct {
    int16_t rate; //AtRate value in mA. Positive for charge, negative for discharge. -32768mA to 32767mA
} Bq28z620StdCmdAtRateRegBits;
_Static_assert(
    sizeof(Bq28z620StdCmdAtRateRegBits) == 2,
    "Size check for 'Bq28z620StdCmdAtRateRegBits' failed.");

/*14.1.3 0x04/05 AtRateTimeToEmpty()*/
typedef struct {
    uint16_t time_to_empty; //Time to empty in minutes. 0 to 65535. 65535 indicates not being charged
} Bq28z620StdCmdAtRateTimeToEmptyRegBits;
_Static_assert(
    sizeof(Bq28z620StdCmdAtRateTimeToEmptyRegBits) == 2,
    "Size check for 'Bq28z620StdCmdAtRateTimeToEmptyRegBits' failed.");

/*14.1.4 0x06/07 Temperature()*/
typedef struct {
    uint16_t temperature; //Temperature in units of 0.1 K. 0 to 65535. For example, a value of 2981 represents 298.1 K (25 °C).
} Bq28z620StdCmdTemperatureRegBits;
_Static_assert(
    sizeof(Bq28z620StdCmdTemperatureRegBits) == 2,
    "Size check for 'Bq28z620StdCmdTemperatureRegBits' failed.");

/*14.1.5 0x08/09 Voltage()*/
typedef struct {
    uint16_t voltage; //Voltage in mV. 0 to 65535
} Bq28z620StdCmdVoltageRegBits;
_Static_assert(
    sizeof(Bq28z620StdCmdVoltageRegBits) == 2,
    "Size check for 'Bq28z620StdCmdVoltageRegBits' failed.");

/*14.1.6 0x0A/0B BatteryStatus()*/
typedef struct {
    uint16_t error_code                 : 4; //Bits 3:0: EC3,EC2,EC1,EC0 Error Code 0x00 = OK, 0x1 = Busy, 0x2 = Reserved Command, 
                                             // 0x3 = Unsupported Command, 0x4 = AccessDenied, 0x5 = Overflow/Underflow, 
                                             // 0x6 = BadSize, 0x7 = UnknownError
    uint16_t fully_discharged           : 1; //Bit 4: FD—Fully Discharged. 0 = Battery ok, 1 = Battery fully depleted
    uint16_t fully_charged              : 1; //Bit 5: FC—Fully Charged. 0 = Battery not fully charged, 01 = Battery fully charged
    uint16_t discharging                : 1; //Bit 6: DSG—Discharging. 0 = Battery is charging. 1 = Battery is discharging.
    uint16_t initialization             : 1; //Bit 7: INIT—Initialization. 0 = Inactive, 1 = Active
    uint16_t remaining_time_alarm       : 1; //Bit 8: RTA—Remaining Time Alarm. 0 = Inactive, 1 = Active
    uint16_t remaining_capacity_alarm   : 1; //Bit 9: RCA—Remaining Capacity Alarm. 0 = Inactive, 1 = Active
    uint16_t                            : 1; //Bit 10: Reserved Undefined
    uint16_t terminate_discharge_alarm  : 1; //Bit 11: TDA—Terminate Discharge Alarm. 0 = Inactive, 1 = Active
    uint16_t overtemperature_alarm      : 1; //Bit 12: OTA—Overtemperature Alarm. 0 = Inactive, 1 = Active
    uint16_t                            : 1; //Bit 13: Reserved Undefined
    uint16_t terminate_charge_alarm     : 1; //Bit 14: TCA—Terminate Charge Alarm. 0 = Inactive, 1 = Active
    uint16_t overcharged_alarm          : 1; //Bit 15: OCA—Overcharged Alarm. 0 = Inactive, 1 = Active
} Bq28z620StdCmdBatteryStatusRegBits;
_Static_assert(
    sizeof(Bq28z620StdCmdBatteryStatusRegBits) == 2,
    "Size check for 'Bq28z620StdCmdBatteryStatusRegBits' failed.");

/*14.1.7 0x0C/0D Current()*/
typedef struct {
    int16_t current; //Current in mA. Positive for charge, negative for discharge. -32768mA to 32767mA
} Bq28z620StdCmdCurrentRegBits;
_Static_assert(
    sizeof(Bq28z620StdCmdCurrentRegBits) == 2,
    "Size check for 'Bq28z620StdCmdCurrentRegBits' failed.");

/*14.1.8 0x0E/0F MaxError()*/
typedef struct {
    uint8_t max_error   : 8; //Max error in %. 0 to 100. Full device reset MaxError() = 100%, 
                             // RA-table only updated MaxError() = 5%, QMax only updated MaxError() = 3%,
                             // RA-table and QMax updated MaxError() = 1%, Each CycleCount() increment 
                             // after last valid QMax update MaxError() increment by 0.05%, 
                             // The Configuration:Max Error Time Cycle Equivalent period passed 
                             // since the last valid QMax update MaxError() increment by 0.05%.
    uint8_t             : 8; //Bits 15–8: Reserved
} Bq28z620StdCmdMaxErrorRegBits;
_Static_assert(
    sizeof(Bq28z620StdCmdMaxErrorRegBits) == 2,
    "Size check for 'Bq28z620StdCmdMaxErrorRegBits' failed.");

/*14.1.9 0x10/11 RemainingCapacity()*/
typedef struct {
    uint16_t remaining_capacity; //Remaining capacity in mAh. 0 to 65535. 
} Bq28z620StdCmdRemainingCapacityRegBits;
_Static_assert(
    sizeof(Bq28z620StdCmdRemainingCapacityRegBits) == 2,
    "Size check for 'Bq28z620StdCmdRemainingCapacityRegBits' failed.");

/*14.1.10 0x12/13 FullChargeCapacity()*/
typedef struct {
    uint16_t full_charge_capacity; //Full charge capacity in mAh. 0 to 65535. 
} Bq28z620StdCmdFullChargeCapacityRegBits;
_Static_assert(
    sizeof(Bq28z620StdCmdFullChargeCapacityRegBits) == 2,
    "Size check for 'Bq28z620StdCmdFullChargeCapacityRegBits' failed.");

/*14.1.11 0x14/15 AverageCurrent()*/
typedef struct {
    int16_t average_current; //Average current in mA. Positive for charge, negative for discharge. -32768mA to 32767mA
} Bq28z620StdCmdAverageCurrentRegBits;
_Static_assert(
    sizeof(Bq28z620StdCmdAverageCurrentRegBits) == 2,
    "Size check for 'Bq28z620StdCmdAverageCurrentRegBits' failed.");

/*14.1.12 0x16/17 AverageTimeToEmpty()*/
typedef struct {
    uint16_t average_time_to_empty; //Average time to empty in minutes. 0 to 65535. 65535 indicates not being discharged
} Bq28z620StdCmdAverageTimeToEmptyRegBits;
_Static_assert(
    sizeof(Bq28z620StdCmdAverageTimeToEmptyRegBits) == 2,
    "Size check for 'Bq28z620StdCmdAverageTimeToEmptyRegBits' failed.");

/*14.1.13 0x18/19 AverageTimeToFull()*/
typedef struct {
    uint16_t average_time_to_full; //Average time to full charge in minutes. 0 to 65535. 65535 indicates not being charged
} Bq28z620StdCmdAverageTimeToFullRegBits;
_Static_assert(
    sizeof(Bq28z620StdCmdAverageTimeToFullRegBits) == 2,
    "Size check for 'Bq28z620StdCmdAverageTimeToFullRegBits' failed.");

/*14.1.14 0x1A/1B StandbyCurrent()*/
typedef struct {
    int16_t standby_current; //Standby current in mA. Positive for charge, negative for discharge. -32768mA to 32767mA
} Bq28z620StdCmdStandbyCurrentRegBits;
_Static_assert(
    sizeof(Bq28z620StdCmdStandbyCurrentRegBits) == 2,
    "Size check for 'Bq28z620StdCmdStandbyCurrentRegBits' failed.");

/*14.1.15 0x1C/1D StandbyTimeToEmpty()*/
typedef struct {
    uint16_t standby_time_to_empty; //Standby time to empty in minutes. 0 to 65535. 65535 indicates not being discharged
} Bq28z620StdCmdStandbyTimeToEmptyRegBits;
_Static_assert(
    sizeof(Bq28z620StdCmdStandbyTimeToEmptyRegBits) == 2,
    "Size check for 'Bq28z620StdCmdStandbyTimeToEmptyRegBits' failed.");

/*14.1.16 0x1E/1F MaxLoadCurrent()*/
typedef struct {
    int16_t max_load_current; //Max load current in mA. Positive for charge, negative for discharge. -32768mA to 32767mA
} Bq28z620StdCmdMaxLoadCurrentRegBits;
_Static_assert(
    sizeof(Bq28z620StdCmdMaxLoadCurrentRegBits) == 2,
    "Size check for 'Bq28z620StdCmdMaxLoadCurrentRegBits' failed.");

/*14.1.17 0x20/21 MaxLoadTimeToEmpty()*/
typedef struct {
    uint16_t max_load_time_to_empty; //Max load time to empty in minutes. 0 to 65535. 65535 indicates not being discharged
} Bq28z620StdCmdMaxLoadTimeToEmptyRegBits;
_Static_assert(
    sizeof(Bq28z620StdCmdMaxLoadTimeToEmptyRegBits) == 2,
    "Size check for 'Bq28z620StdCmdMaxLoadTimeToEmptyRegBits' failed.");

/*14.1.18 0x22/23 AveragePower()*/
typedef struct {
    int16_t average_power; //Average power in mW. Positive for charge, negative for discharge. -32768mW to 32767mW
} Bq28z620StdCmdAveragePowerRegBits;
_Static_assert(
    sizeof(Bq28z620StdCmdAveragePowerRegBits) == 2,
    "Size check for 'Bq28z620StdCmdAveragePowerRegBits' failed.");

/*14.1.19 0x28/29 InternalTemperature()*/
typedef struct {
    uint16_t internal_temperature; //Internal die temperature in units 0.1 K. 0 to 65535. For example, a value of 2981 represents 298.1 K (25 °C).
} Bq28z620StdCmdInternalTemperatureRegBits;
_Static_assert(
    sizeof(Bq28z620StdCmdInternalTemperatureRegBits) == 2,
    "Size check for 'Bq28z620StdCmdInternalTemperatureRegBits' failed.");

/*14.1.20 0x2A/2B CycleCount()*/
typedef struct {
    uint16_t cycle_count; //Number of discharge cycles the battery has experienced. 0 to 65535
} Bq28z620StdCmdCycleCountRegBits;
_Static_assert(
    sizeof(Bq28z620StdCmdCycleCountRegBits) == 2,
    "Size check for 'Bq28z620StdCmdCycleCountRegBits' failed.");

/*14.1.21 0x2C/2D RelativeStateOfCharge()*/
typedef struct {
    uint8_t relative_state_of_charge    : 8; //Relative state of charge in %. 0 to 100
    uint8_t                             : 8; //Bits 15–8: Reserved
} Bq28z620StdCmdRelativeStateOfChargeRegBits;
_Static_assert(
    sizeof(Bq28z620StdCmdRelativeStateOfChargeRegBits) == 2,
    "Size check for 'Bq28z620StdCmdRelativeStateOfChargeRegBits' failed.");

/*14.1.22 0x2E/2F State-of-Health (SOH)*/
typedef struct {
    uint8_t state_of_health     : 8; //State of health in %. 0 to 100
    uint8_t                     : 8; //Bits 15–8: Reserved
} Bq28z620StdCmdStateOfHealthRegBits;
_Static_assert(
    sizeof(Bq28z620StdCmdStateOfHealthRegBits) == 2,
    "Size check for 'Bq28z620StdCmdStateOfHealthRegBits' failed.");

/*14.1.23 0x30/31 ChargingVoltage()*/
typedef struct {
    uint16_t charging_voltage; //Desired charging voltage in mV. 0 to 65535
} Bq28z620StdCmdChargingVoltageRegBits;
_Static_assert(
    sizeof(Bq28z620StdCmdChargingVoltageRegBits) == 2,
    "Size check for 'Bq28z620StdCmdChargingVoltageRegBits' failed.");

/*14.1.24 0x32/33 ChargingCurrent()*/
typedef struct {
    uint16_t charging_current; //Desired charging current in mA. 0 to 65535. 65535 indicates request for maximum current
} Bq28z620StdCmdChargingCurrentRegBits;
_Static_assert(
    sizeof(Bq28z620StdCmdChargingCurrentRegBits) == 2,
    "Size check for 'Bq28z620StdCmdChargingCurrentRegBits' failed.");

/*14.1.25 0x3C/3D DesignCapacity()*/
typedef struct {
    uint16_t design_capacity; //Design capacity in mAh. 0 to 65535. Default value is 4400mAh.
} Bq28z620StdCmdDesignCapacityRegBits;
_Static_assert(
    sizeof(Bq28z620StdCmdDesignCapacityRegBits) == 2,
    "Size check for 'Bq28z620StdCmdDesignCapacityRegBits' failed.");

/*14.2.1 MACSubcmd() 0x0001 Device Type*/
typedef struct {
    uint16_t device_type; //Device type in format aaAA, where aaAA is the IC part number.
} Bq28z620MacSubcmdDeviceTypeRegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdDeviceTypeRegBits) == 2,
    "Size check for 'Bq28z620MacSubcmdDeviceTypeRegBits' failed.");

/*14.2.2 MACSubcmd() 0x0002 Firmware Version*/
typedef struct {
    union {
        struct {
            uint16_t device_number;             //Device number in format ddDD, where ddDD is a unique identifier for the specific IC device.
            uint16_t version;                   //Firmware version in format vvVV, where vvVV is the firmware version number.
            uint16_t build_number;              //Build number in format bbBB, where bbBB is the build number of the firmware.
            uint16_t firmware_type;             //Firmware type in format ttTT, where ttTT indicates the type of firmware (e.g., production, engineering).
            uint16_t impedance_track_version;   //Impedance Track version in format zzZZ, where zzZZ indicates the version of the Impedance Track algorithm implemented in the firmware.
            uint8_t Reserved0;                  //Reserved bits for future use or manufacturer-specific information.
            uint8_t Reserved1;                  //Reserved bits for future use or manufacturer-specific information.
    } FURI_PACKED;
        uint8_t data[6];
    };
}FURI_PACKED Bq28z620MacSubcmdFirmwareVersionRegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdFirmwareVersionRegBits) == 12,
    "Size check for 'Bq28z620MacSubcmdFirmwareVersionRegBits' failed.");

/*14.2.3 MACSubcmd() 0x0003 Hardware Version*/
typedef struct {
    uint16_t hardware_version; //Hardware version in format rrRR, where rrRR indicates the hardware revision of the IC.
} Bq28z620MacSubcmdHardwareVersionRegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdHardwareVersionRegBits) == 2,
    "Size check for 'Bq28z620MacSubcmdHardwareVersionRegBits' failed.");

/*14.2.4 MACSubcmd() 0x0004 Instruction Flash Signature. Wait 250ms*/
typedef struct {
    uint16_t instruction_flash_signature; // Returns the IF signature on subsequent read on MACData() after a wait time of 250 ms.
} Bq28z620MacSubcmdIFSignatureRegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdIFSignatureRegBits) == 2,
    "Size check for 'Bq28z620MacSubcmdIFSignatureRegBits' failed.");

/*14.2.5 MACSubcmd() 0x0005 Static DF Signature. Wait 250ms*/
typedef struct {
    uint16_t static_df_signature;   //Returns the signature of all static DF on subsequent read on MACData() after a wait time of 250 ms. 
                                    // MSB is set to 1 if the calculated signature does not match the signature stored in DF
} Bq28z620MacSubcmdStaticDFSignatureRegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdStaticDFSignatureRegBits) == 2,
    "Size check for 'Bq28z620MacSubcmdStaticDFSignatureRegBits' failed.");

/*14.2.6 MACSubcmd() 0x0006 Chemical ID*/
typedef struct {
    uint16_t chemical_id; //Chemical ID in format ccCC, where ccCC is a unique identifier for the specific battery chemistry used in the OCV tables.
} Bq28z620MacSubcmdChemicalIDRegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdChemicalIDRegBits) == 2,
    "Size check for 'Bq28z620MacSubcmdChemicalIDRegBits' failed.");

/*14.2.7 MACSubcmd() 0x0007 Pre_MACWrite*/
typedef struct {
    uint16_t pre_mac_write; //Pre_MACWrite. This command enables copying the last MAC into a 2-byte block.
} Bq28z620MacSubcmdPreMACWriteRegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdPreMACWriteRegBits) == 2,
    "Size check for 'Bq28z620MacSubcmdPreMACWriteRegBits' failed.");

/*14.2.8 MACSubcmd() 0x0008 Static Chem DF Signature. Wait 250ms*/
typedef struct {
    uint16_t static_chem_df_signature; //Returns the signature of all static chemistry DF on subsequent read on MACData() after a wait time of 250 ms. 
                                       // MSB is set to 1 if the calculated signature does not match the signature stored in DF
} Bq28z620MacSubcmdStaticChemDFSignatureRegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdStaticChemDFSignatureRegBits) == 2,
    "Size check for 'Bq28z620MacSubcmdStaticChemDFSignatureRegBits' failed.");

/*14.2.9 MACSubcmd() 0x0009 All DF Signature. Wait 250ms*/
typedef struct {
    uint16_t all_df_signature; //Returns the signature of all DF parameters on subsequent read on MACData() after a wait time of 250 ms. 
                               // MSB is set to 1 if the calculated signature does not match the signature stored in DF
} Bq28z620MacSubcmdAllDFSignatureRegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdAllDFSignatureRegBits) == 2,
    "Size check for 'Bq28z620MacSubcmdAllDFSignatureRegBits' failed.");

/*
14.2.10 MACSubcmd() 0x0010 SHUTDOWN Mode
14.2.11 MACSubcmd() 0x0011 SLEEP Mode
14.2.12 MACSubcmd() 0x0012 Device Reset
14.2.13 MACSubcmd() 0x001F CHG FET
14.2.14 MACSubcmd() 0x0020 DSG FET
14.2.15 MACSubcmd() 0x0021 Gauging
14.2.16 MACSubcmd() 0x0022 FET Control
14.2.17 MACSubcmd() 0x0023 Lifetime Data Collection
14.2.18 MACSubcmd() 0x0024 Permanent Failure
14.2.19 MACSubcmd() 0x0028 Lifetime Data Reset
14.2.20 MACSubcmd() 0x0029 Permanent Fail Data Reset
14.2.21 MACSubcmd() 0x002D CALIBRATION Mode
14.2.22 MACSubcmd() 0x0030 Seal Device
14.2.23 MACSubcmd() 0x0035 Security Keys
14.2.25 MACSubcmd() 0x0041 Device Reset

*/

/*14.2.26 MACSubcmd() 0x004A Device Name*/
typedef struct {
    char device_name[21]; //Device name as a null-terminated ASCII string. Default bq28z620 ASCII Assigned pack name
} Bq28z620MacSubcmdDeviceNameRegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdDeviceNameRegBits) == 21,
    "Size check for 'Bq28z620MacSubcmdDeviceNameRegBits' failed."
);

/*14.2.27 MACSubcmd() 0x004B Device Chem*/
typedef struct {
    char device_chem[5]; //Device chemistry as a null-terminated ASCII string. Default LION
} Bq28z620MacSubcmdDeviceChemRegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdDeviceChemRegBits) == 5 ,
    "Size check for 'Bq28z620MacSubcmdDeviceChemRegBits' failed."
);

/*14.2.28 MACSubcmd() 0x004C Manufacturer Name*/
typedef struct {
    char manufacturer_name[21]; //Manufacturer name as a null-terminated ASCII string. Default Texas Instruments
} Bq28z620MacSubcmdManufacturerNameRegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdManufacturerNameRegBits) == 21,
    "Size check for 'Bq28z620MacSubcmdManufacturerNameRegBits' failed."
);

/*14.2.29 MACSubcmd() 0x004D Manufacture Date*/
typedef struct {
    uint16_t manufacture_date; //Manufacture date of the pack, which follows the format: Day + Month×32 + (Year–1980) × 512, Example: 10/27/2017 = 19291 (or 0x4B5B, binary split: 100101, 1010, 11011)
} Bq28z620MacSubcmdManufactureDateRegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdManufactureDateRegBits) == 2,
    "Size check for 'Bq28z620MacSubcmdManufactureDateRegBits' failed."
);

/*14.2.30 MACSubcmd() 0x004E Serial Number*/
typedef struct {
    uint16_t serial_number; //Serial number of the pack. 0 to 65535. Default value is 1.
} Bq28z620MacSubcmdSerialNumberRegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdSerialNumberRegBits) == 2,
    "Size check for 'Bq28z620MacSubcmdSerialNumberRegBits' failed."
);

/*14.2.31 MACSubcmd() 0x0050 SafetyAlert*/
typedef struct {
    uint8_t cuv         : 1; //Bit 0: CUV—Cell Undervoltage. 1 = Detected, 0 = Not Detected
    uint8_t cov         : 1; //Bit 1: COV—Cell Overvoltage. 1 = Detected, 0 = Not Detected
    uint8_t occ         : 1; //Bit 2: OCC—Overcurrent During Charge. 1 = Detected, 0 = Not Detected
    uint8_t             : 1; //Bit 3: Reserved
    uint8_t ocd         : 1; //Bit 4: OCD—Overcurrent During Discharge. 1 = Detected, 0 = Not Detected
    uint8_t             : 1; //Bit 5: Reserved
    uint8_t aold        : 1; //Bit 6: AOLD—Overload During Discharge. 1 = Detected, 0 = Not Detected
    uint8_t             : 1; //Bit 7: Reserved
    uint8_t ascc        : 1; //Bit 8: ASCC—Short-Circuit During Charge. 1 = Detected, 0 = Not Detected
    uint8_t             : 1; //Bit 9: Reserved
    uint8_t ascd        : 1; //Bit 10: ASCD—Short-Circuit During Discharge. 1 = Detected, 0 = Not Detected
    uint8_t             : 1; //Bit 11: Reserved
    uint8_t otc         : 1; //Bit 12: OTC—Overtemperature During Charge. 1 = Detected, 0 = Not Detected
    uint8_t otd         : 1; //Bit 13: OTD—Overtemperature During Discharge. 1 = Detected, 0 = Not Detected
    uint8_t             : 2; //Bits 15–14: Reserved
    uint8_t             : 3; //Bits 18–16: Reserved
    uint8_t ptos        : 1; //Bit 19: PTOS—Precharge Timeout Suspend. 1 = Detected, 0 = Not Detected
    uint8_t             : 1; //Bit 20: Reserved
    uint8_t ctos        : 1; //Bit 21: CTOS—Charge Timeout Suspend. 1 = Detected, 0 = Not Detected
    uint8_t             : 2; //Bits 23–22: Reserved
    uint8_t             : 2; //Bits 25–24: Reserved
    uint8_t utc         : 1; //Bit 26: UTC—Undertemperature During Charge. 1 = Detected, 0 = Not Detected
    uint8_t utd         : 1; //Bit 27: UTD—Undertemperature During Discharge. 1 = Detected, 0 = Not Detected
    uint8_t             : 4; //Bits 31–28: Reserved
} Bq28z620MacSubcmdSafetyAlertRegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdSafetyAlertRegBits) == 4,
    "Size check for 'Bq28z620MacSubcmdSafetyAlertRegBits' failed."
);

/*14.2.32 MACSubcmd() 0x0051 SafetyStatus*/

typedef struct {
    uint8_t cuv         : 1; //Bit 0: CUV—Cell Undervoltage. 1 = Detected, 0 = Not Detected
    uint8_t cov         : 1; //Bit 1: COV—Cell Overvoltage. 1 = Detected, 0 = Not Detected
    uint8_t occ         : 1; //Bit 2: OCC—Overcurrent During Charge. 1 = Detected, 0 = Not Detected
    uint8_t             : 1; //Bit 3: Reserved
    uint8_t ocd         : 1; //Bit 4: OCD—Overcurrent During Discharge. 1 = Detected, 0 = Not Detected
    uint8_t             : 1; //Bit 5: Reserved
    uint8_t aold        : 1; //Bit 6: AOLD—Overload During Discharge. 1 = Detected, 0 = Not Detected
    uint8_t             : 1; //Bit 7: Reserved
    uint8_t ascc        : 1; //Bit 8: ASCC—Short-Circuit During Charge. 1 = Detected, 0 = Not Detected
    uint8_t             : 1; //Bit 9: Reserved
    uint8_t ascd        : 1; //Bit 10: ASCD—Short-Circuit During Discharge. 1 = Detected, 0 = Not Detected
    uint8_t             : 1; //Bit 11: Reserved
    uint8_t otc         : 1; //Bit 12: OTC—Overtemperature During Charge. 1 = Detected, 0 = Not Detected
    uint8_t otd         : 1; //Bit 13: OTD—Overtemperature During Discharge. 1 = Detected, 0 = Not Detected
    uint8_t             : 2; //Bits 15–14: Reserved
    uint8_t             : 2; //Bits 17–16: Reserved
    uint8_t pto         : 1; //Bit 18: PTO—Precharge Timeout Suspend. 1 = Detected, 0 = Not Detected
    uint8_t             : 1; //Bit 19: Reserved
    uint8_t cto         : 1; //Bit 20: CTO—Charge Timeout Suspend. 1 = Detected, 0 = Not Detected
    uint8_t             : 3; //Bits 23–21: Reserved
    uint8_t             : 2; //Bits 25–24: Reserved
    uint8_t utc         : 1; //Bit 26: UTC—Undertemperature During Charge. 1 = Detected, 0 = Not Detected
    uint8_t utd         : 1; //Bit 27: UTD—Undertemperature During Discharge. 1 = Detected, 0 = Not Detected
    uint8_t             : 4; //Bits 31–28: Reserved
} Bq28z620MacSubcmdSafetyStatusRegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdSafetyStatusRegBits) == 4,
    "Size check for 'Bq28z620MacSubcmdSafetyStatusRegBits' failed."
);

/*14.2.33 MACSubcmd() 0x0052 PFAlert*/
typedef struct {
    uint8_t             : 1; //Bit 0: Reserved
    uint8_t sov         : 1; //Bit 1: SOV—Safety Cell Overvoltage Failure. 1 = Detected, 0 = Not Detected
    uint16_t            : 9; //Bits 10–2: Reserved
    uint8_t vimr        : 1; //Bit 11: VIMR—Voltage Imbalance While Pack Is At Rest Failure. 1 = Detected, 0 = Not Detected
    uint8_t vima        : 1; //Bit 12: VIMA—Voltage Imbalance While Pack Is Active Failure. 1 = Detected, 0 = Not Detected
    uint8_t             : 3; //Bits 15–13: Reserved
    uint8_t cfetf       : 1; //Bit 16: CFETF—Charge FET Failure. 1 = Detected, 0 = Not Detected
    uint8_t dfetf       : 1; //Bit 17: DFETF—Discharge FET Failure. 1 = Detected, 0 = Not Detected
    uint16_t            : 14; //Bits 31–18: Reserved
} Bq28z620MacSubcmdPFAlertRegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdPFAlertRegBits) == 4,
    "Size check for 'Bq28z620MacSubcmdPFAlertRegBits' failed."
);

/*14.2.34 MACSubcmd() 0x0053 PFStatus*/
typedef struct {
    uint8_t             : 1; //Bit 0: Reserved
    uint8_t sov         : 1; //Bit 1: SOV—Safety Cell Overvoltage Failure. 1 = Detected, 0 = Not Detected
    uint16_t            : 9; //Bits 10–2: Reserved
    uint8_t vimr        : 1; //Bit 11: VIMR—Voltage Imbalance While Pack Is At Rest Failure. 1 = Detected, 0 = Not Detected
    uint8_t vima        : 1; //Bit 12: VIMA—Voltage Imbalance While Pack Is Active Failure. 1 = Detected, 0 = Not Detected
    uint8_t             : 3; //Bits 15–13: Reserved
    uint8_t cfetf       : 1; //Bit 16: CFETF—Charge FET Failure. 1 = Detected, 0 = Not Detected
    uint8_t dfetf       : 1; //Bit 17: DFETF—Discharge FET Failure. 1 = Detected, 0 = Not Detected
    uint16_t            : 14; //Bits 31–18: Reserved
} Bq28z620MacSubcmdPFStatusRegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdPFStatusRegBits) == 4,
    "Size check for 'Bq28z620MacSubcmdPFStatusRegBits' failed."
);

/*14.2.35 MACSubcmd() 0x0054 OperationStatus*/
typedef struct {
    uint8_t             : 1; //Bit 0: Reserved
    uint8_t dsg         : 1; //Bit 1: DSG FET status. 1 = Active, 0 = Inactive
    uint8_t chg         : 1; //Bit 2: CHG FET status. 1 = Active, 0 = Inactive
    uint8_t             : 5; //Bits 7–3: Reserved
    uint8_t sec8_9      : 2; //Bits 9–8: SECURITY mode. 0b00 = Reserved, 0b01 = Full Access, 0b10 = Unsealed, 0b11 = Sealed
    uint8_t sdv         : 1; //Bit 10: SHUTDOWN triggered via low pack voltage. 1 = Active, 0 = Inactive
    uint8_t ss          : 1; //Bit 11: SAFETY mode status. 1 = Active, 0 = Inactive
    uint8_t pf          : 1; //Bit 12: PERMANENT FAILURE mode status. 1 = Active, 0 = Inactive
    uint8_t sec13_14    : 2; //Bits 14–13: SECURITY mode. 0b00 = Reserved, 0b01 = Full Access, 0b10 = Unsealed, 0b11 = Sealed
    uint8_t sleep       : 1; //Bit 15: SLEEP mode conditions met. 1 = Active, 0 = Inactive
    uint8_t sdm         : 1; //Bit 16: SHUTDOWN triggered through a command. 1 = Active, 0 = Inactive
    uint8_t auth        : 1; //Bit 18: Authentication in progress. 1 = Active, 0 = Inactive
    uint8_t authcalm    : 1; //Bit 19: Auto CC Offset Calibration by MAC AutoCCOffset(). 1 = The gauge receives the MAC AutoCCOffset() and starts the auto CC Offset calibration. 0 = Clear when the calibration is completed.
    uint8_t cal         : 1; //Bit 20: Calibration Output (raw ADC and CC data). 1 = Active when either the MAC OutputCCADCCal() or OutputShortedCCADCCal() is sent and the raw CC and ADC data for calibration is available. 0 = When the raw CC and ADC data for calibration is not available.
    uint8_t cal_offset  : 1; //Bit 21: Calibration Output (raw CC Offset data). 1 = Active when MAC OutputShortedCCADCCal() is sent and the raw shorted CC data for calibration is available. 0 = When the raw shorted CC data for calibration is not available.
    uint8_t xl          : 1; //Bit 22: 400-kHz mode. 1 = Active, 0 = Inactive
    uint8_t sleepm      : 1; //Bit 23: SLEEP mode. 1 = Active, 0 = Inactive
    uint8_t init        : 1; //Bit 24: Initialization after full reset. 1 = Active, 0 = Inactive
    uint8_t smbcal      : 1; //Bit 25: Auto-offset calibration when bus low is detected. 1 = Active, 0 = Inactive
    uint8_t slpad       : 1; //Bit 26: ADC measurement in SLEEP mode. 1 = Active, 0 = Inactive
    uint8_t slpcc       : 1; //Bit 27: CC measurement in SLEEP mode. 1 = Active, 0 = Inactive
    uint8_t cb          : 1; //Bit 28: Cell Balancing. 1 = Active, 0 = Inactive
    uint8_t emshut      : 1; //Bit 29: Emergency FET Shutdown. 1 = Active, 0 = Inactive
    uint8_t             : 2; //Bits 31–30: Reserved
} Bq28z620MacSubcmdOperationStatusRegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdOperationStatusRegBits) == 4,
    "Size check for 'Bq28z620MacSubcmdOperationStatusRegBits' failed."
);

/*14.2.36 MACSubcmd() 0x0055 ChargingStatus*/
typedef struct {
    uint8_t ut         : 1; //Bit 0: UT—Under Temperature Region. 1 = Active, 0 = Inactive
    uint8_t lt         : 1; //Bit 1: LT—Low Temperature Region. 1 = Active, 0 = Inactive
    uint8_t stl        : 1; //Bit 2: STL—Standard Temperature Low Region. 1 = Active, 0 = Inactive
    uint8_t rt         : 1; //Bit 3: RT—Room Temperature Region. 1 = Active, 0 = Inactive
    uint8_t sth        : 1; //Bit 4: STH—Standard Temperature High Region. 1 = Active, 0 = Inactive
    uint8_t ht         : 1; //Bit 5: HT—High Temperature Region. 1 = Active, 0 = Inactive
    uint8_t ot         : 1; //Bit 6: OT—Over Temperature Region. 1 = Active, 0 = Inactive
    uint8_t            : 1; //Bit 7: Reserved
    uint8_t pv         : 1; //Bit 8: PV—Precharge Voltage Region. 1 = Active, 0 = Inactive
    uint8_t lv         : 1; //Bit 9: LV—Low Voltage Region. 1 = Active, 0 = Inactive
    uint8_t mv         : 1; //Bit 10: MV—Mid Voltage Region. 1 = Active, 0 = Inactive
    uint8_t hv         : 1; //Bit 11: HV—High Voltage Region. 1 = Active, 0 = Inactive
    uint8_t in         : 1; //Bit 12: IN—Charge Inhibit. 1 = Active, 0 = Inactive
    uint8_t su         : 1; //Bit 13: SU—Charge Suspend. 1 = Active, 0 = Inactive
    uint8_t mchg       : 1; //Bit 14: MCHG—Maintenance Charge. 1 = Active, 0 = Inactive
    uint8_t vct        : 1; //Bit 15: VCT—Charge Termination. 1 = Active, 0 = Inactive
} Bq28z620MacSubcmdChargingStatusRegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdChargingStatusRegBits) == 2,
    "Size check for 'Bq28z620MacSubcmdChargingStatusRegBits' failed."
);

/*14.2.37 MACSubcmd() 0x0056 GaugingStatus*/
typedef struct {
    uint8_t fd          : 1; //Bit 0: FD—Fully Discharged. 1 = Detected, 0 = Not Detected
    uint8_t fc          : 1; //Bit 1: FC—Fully Charged. 1 = Detected, 0 = Not Detected
    uint8_t td          : 1; //Bit 2: TD—Terminate Discharge. 1 = Detected, 0 = Not Detected
    uint8_t tc          : 1; //Bit 3: TC—Terminate Charge. 1 = Detected, 0 = Not Detected
    uint8_t bal_en      : 1; //Bit 4: BAL_EN—Cell Balancing. 1 = Cell balancing is possible if enabled. 0 = Cell balancing is not allowed.
    uint8_t edv         : 1; //Bit 5: EDV—End-of-Discharge Termination Voltage. 1 = Termination voltage reached during discharge. 0 = Termination voltage not reached or not in DISCHARGE mode
    uint8_t dsg         : 1; //Bit 6: DSG—Discharge/Relax. 1 = Charging Not Detected. 0 = Charging Detected
    uint8_t cf          : 1; //Bit 7: CF—Condition Flag. 1 = MaxError() > Max Error Limit (Condition Cycle is needed.) 0 = MaxError() < Max Error Limit (Condition Cycle is not needed.)
    uint8_t rest        : 1; //Bit 8: REST—Rest. 1 = OCV Reading Taken. 0 = OCV Reading Not Taken or Not in Relax
    uint8_t             : 1; //Bit 9: Reserved
    uint8_t rdis        : 1; //Bit 10: RDIS—Resistance Updates. 1 = Disabled. 0 = Enabled
    uint8_t vok         : 1; //Bit 11: VOK—Voltage OK for QMax Update. 1 = Detected. 0 = Not Detected
    uint8_t qen         : 1; //Bit 12: QEN—Impedance Track Gauging (Ra and QMax updates are enabled.) 1 = Enabled. 0 = Disabled
    uint8_t slpq_max    : 1; //Bit 13: SLPQ—QMax Update During Sleep. 1 = Active. 0 = Inactive
    uint8_t             : 1; //Bit 14: Reserved
    uint8_t nsfm        : 1; //Bit 15: NSFM—Negative Scale Factor Mode. 1 = Negative Ra Scaling Factor Detected. 0 = Negative Ra Scaling Factor Not Detected
    uint8_t vdq         : 1; //Bit 16: VDQ—Discharge Qualified for Learning (based on RU flag). 1 = Detected. 0 = Not Detected
    uint8_t qmax        : 1; //Bit 17: QMax Update (Toggles after every QMax update)
    uint8_t rx          : 1; //Bit 18: RX—Resistance Update (Toggles after every resistance update)
    uint8_t ldmd        : 1; //Bit 19: LDMD—LOAD mode. 1 = Constant Power. 0 = Constant Current
    uint8_t ocv_fr      : 1; //Bit 20: OCVFR—Open Circuit Voltage in Flat Region (during RELAX). 1 = Detected. 0 = Not Detected
    uint16_t            : 11; //Bits 31–21: Reserved
} Bq28z620MacSubcmdGaugingStatusRegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdGaugingStatusRegBits) == 4,
    "Size check for 'Bq28z620MacSubcmdGaugingStatusRegBits' failed."
);

/*14.2.38 MACSubcmd() 0x0057 ManufacturingStatus*/
typedef struct {
    uint8_t             : 1; //Bit 0: Reserved
    uint8_t chg_test    : 1; //Bit 1: CHG_TEST—Charge FET Test. 1 = Charge FET test activated. 0 = Disabled
    uint8_t dsg_test    : 1; //Bit 2: DSG_TEST—Discharge FET Test. 1 = Discharge FET test activated. 0 = Disabled
    uint8_t gauge_en    : 1; //Bit 3: GAUGE_EN—Gas Gauging. 1 = Enabled. 0 = Disabled
    uint8_t fet_en      : 1; //Bit 4: FET_EN—All FET Action. 1 = Enabled. 0 = Disabled
    uint8_t lf_en       : 1; //Bit 5: LF_EN—Lifetime Data Collection. 1 = Enabled. 0 = Disabled
    uint8_t pf_en       : 1; //Bit 6: PF_EN—Permanent Failure. 1 = Enabled. 0 = Disabled
    uint8_t             : 1; //Bit 7: Reserved
    uint8_t             : 7; //Bits 14–7: Reserved
    uint8_t cal_en      : 1; //Bit 15: CAL_EN—CALIBRATION Mode. 1 = Enabled. 0 = Disabled
} Bq28z620MacSubcmdManufacturingStatusRegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdManufacturingStatusRegBits) == 2,
    "Size check for 'Bq28z620MacSubcmdManufacturingStatusRegBits' failed."
);

/*14.2.39 MACSubcmd() 0x0058 AFE Register*/
typedef struct {
    union {
        struct {
            uint8_t afe_interrupt_status;   //AFE Interrupt Status. AFE Hardware interrupt status (for example, wake time, push-button, and so on)
            uint8_t afe_fet_status;         //AFE FET Status. AFE FET status (for example, CHG FET, DSG FET, input, and so on)
            uint8_t afe_rxin;               //AFE RXIN. AFE I/O port input status
            uint8_t afe_latch_status;       //AFE Latch Status. AFE protection latch status
            uint8_t afe_interrupt_enable;   //AFE Interrupt Enable. AFE interrupt control settings
            uint8_t afe_control;            //AFE Control. AFE FET control enable setting
            uint8_t afe_rxien;              //AFE RXIEN. AFE I/O input enable settings
            uint8_t reserved0;              //Reserved for future use or manufacturer-specific information.
            uint8_t reserved1;              //Reserved for future use or manufacturer-specific information.
            uint8_t reserved2;              //Reserved for future use or manufacturer-specific information.
            uint8_t afe_cell_balance;       //AFE Cell Balance. AFE cell balancing enable settings and status
            uint8_t afe_adc_cc_control;     //AFE ADC/CC Control. AFE ADC/CC Control settings
            uint8_t afe_adc_mux;            //AFE ADC Mux. AFE ADC channel selections
            uint8_t reserved4;              //Reserved for future use or manufacturer-specific information.
            uint8_t afe_control_2;          //AFE Control. AFE control on various HW based features
            uint8_t afe_timer_control;      //AFE Timer Control. AFE comparator and timer control
            uint8_t afe_protection;         //AFE Protection. AFE protection delay time control
            uint8_t afe_ocd;                //AFE OCD. AFE OCD settings
            uint8_t afe_scc;                //AFE SCC. AFE SCC settings
            uint8_t afe_scd1;               //AFE SCD1. AFE SCD1 settings
            uint8_t afe_scd2;               //AFE SCD2. AFE SCD2 settings
        } FURI_PACKED;
        uint8_t data[21];
    };
} FURI_PACKED Bq28z620AfeRegBits;
_Static_assert(
    sizeof(Bq28z620AfeRegBits) == 21,
    "Size check for 'Bq28z620AfeRegBits' failed.");

/*14.2.40 MACSubcmd() 0x0060 Lifetime Data Block 1*/
typedef struct {
    union {
        struct {
            uint16_t lifetime_voltage;     //Lifetime Voltage. Lifetime voltage data in mV
            int16_t lifetime_current;      //Lifetime Current. Lifetime current data in mA. Positive value indicates discharge current, and negative value indicates charge current.
            int32_t lifetime_power;        //Lifetime Power. Lifetime power data in mW. Positive value indicates discharge power, and negative value indicates charge power.
            int16_t lifetime_temperature;  //Lifetime Temperature. Lifetime temperature data in 0.1K (for example, 250 means 25.0°C)
    } FURI_PACKED;
        uint8_t data[10];
    };
} FURI_PACKED Bq28z620MacSubcmdLifetimeDataBlock1RegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdLifetimeDataBlock1RegBits) == 10,
    "Size check for 'Bq28z620MacSubcmdLifetimeDataBlock1RegBits' failed.");

/*14.2.41 MACSubcmd() 0x0070 ManufacturerInfo*/
typedef struct {
    union {
        struct {
            char manufacturer_info[32]; //Outputs 32 bytes of ManufacturerInfo on MACData()
    } FURI_PACKED;
        uint8_t data[32];
    };
} FURI_PACKED Bq28z620MacSubcmdManufacturerInfoRegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdManufacturerInfoRegBits) == 32,
    "Size check for 'Bq28z620MacSubcmdManufacturerInfoRegBits' failed."
);

/*14.2.42 MACSubcmd() 0x0071 DAStatus1*/
typedef struct {
    union {
        struct {
            uint16_t cell_voltage_1;   //Cell Voltage 1. Simultaneous voltage measured during Cell Current 1 measurement
            uint16_t cell_voltage_2;   //Cell Voltage 2. Simultaneous voltage measured during Cell Current 2 measurement
            uint16_t reserved0;         //Reserved for future use or manufacturer-specific information.
            uint16_t reserved1;         //Reserved for future use or manufacturer-specific information.
            uint16_t bat_voltage;     //BAT Voltage. Voltage at the VC2 (BAT) terminal
            uint16_t pack_voltage;    //PACK Voltage
            int16_t cell_current_1;   //Cell Current 1. Simultaneous current measured during Cell Voltage 1 measurement. Positive value indicates discharge current, and negative value indicates charge current.
            int16_t cell_current_2;   //Cell Current 2. Simultaneous current measured during Cell Voltage 2 measurement. Positive value indicates discharge current, and negative value indicates charge current.
            uint16_t reserved2;         //Reserved for future use or manufacturer-specific information.
            uint16_t reserved3;         //Reserved for future use or manufacturer-specific information.
            int16_t cell_power_1;     //Cell Power 1. Calculated using Cell Voltage 1 and Cell Current 1 data. Positive value indicates discharge power, and negative value indicates charge power.
            int16_t cell_power_2;     //Cell Power 2. Calculated using Cell Voltage 2 and Cell Current 2 data. Positive value indicates discharge power, and negative value indicates charge power.
            uint16_t reserved4;         //Reserved for future use or manufacturer-specific information.
            uint16_t reserved5;         //Reserved for future use or manufacturer-specific information.
            int16_t power;           //Power calculated by Voltage() × Current(). Positive value indicates discharge power, and negative value indicates charge power.
            int16_t average_power;   //Average Power. Calculated by Voltage() × AverageCurrent(). Positive value indicates discharge power, and negative value indicates charge power.
    } FURI_PACKED;
        uint8_t data[32];
    };
} FURI_PACKED Bq28z620MacSubcmdDAStatus1RegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdDAStatus1RegBits) == 32,
    "Size check for 'Bq28z620MacSubcmdDAStatus1RegBits' failed."
);

/*14.2.43 MACSubcmd() 0x0072 DAStatus2*/
typedef struct {
    union {
        struct {
            int16_t internal_temperature;   //Internal Temperature. Temperature data from the internal temperature sensor in 0.1K (for example, 250 means 25.0°C)
            int16_t ts1_temperature;        //TS1 Temperature. Temperature data from the TS1 temperature sensor in 0.1K (for example, 250 means 25.0°C)
            int16_t reserved0;              //Reserved for future use or manufacturer-specific information.
            int16_t reserved1;              //Reserved for future use or manufacturer-specific information.
            int16_t reserved2;              //Reserved for future use or manufacturer-specific information.
            int16_t reserved3;              //Reserved for future use or manufacturer-specific information.
            int16_t reserved4;              //Reserved for future use or manufacturer-specific information.
    } FURI_PACKED;
        uint8_t data[14];
    };
} FURI_PACKED Bq28z620MacSubcmdDAStatus2RegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdDAStatus2RegBits) == 14,
    "Size check for 'Bq28z620MacSubcmdDAStatus2RegBits' failed."
);

/*14.2.44 MACSubcmd() 0x0073 ITStatus1*/
typedef struct {
    union {
        struct {
            int16_t true_rem_q;       //True Rem Q. True remaining capacity in mAh from IT simulation before any filtering or smoothing function. This value can be negative or higher than FCC.
            int16_t true_rem_e;       //True Rem E. True remaining energy in cWh from IT simulation before any filtering or smoothing function. This value can be negative or higher than FCC.
            int16_t initial_q;       //Initial Q. Initial capacity calculated from IT simulation
            int16_t initial_e;       //Initial E. Initial energy calculated from IT simulation
            int16_t true_full_chg_q; //TrueFullChgQ. True full charge capacity
            int16_t true_full_chg_e; //TrueFullChgE. True full charge energy
            int16_t t_sim;           //T_sim. Temperature during the last simulation run (in 0.1K)
            int16_t t_ambient;       //T_ambient. Current estimated ambient temperature used by the IT algorithm for thermal modeling
            int16_t ra_scale_0;      //RaScale 0. Ra table scaling factor of Cell1
            int16_t ra_scale_1;      //RaScale 1. Ra table scaling factor of Cell2
            int16_t comp_res_1;      //CompRes1. Last computed resistance for Cell1
            int16_t comp_res_2;      //CompRes2. Last computed resistance for Cell2
    } FURI_PACKED;
        uint8_t data[24];
    };
} FURI_PACKED Bq28z620MacSubcmdITStatus1RegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdITStatus1RegBits) == 24,
    "Size check for 'Bq28z620MacSubcmdITStatus1RegBits' failed."
);

/*14.2.45 MACSubcmd() 0x0074 ITStatus2*/
typedef struct {
    union {
        struct {
            uint8_t pack_grid;          //Pack Grid. Active pack grid point (only valid in discharge)
            uint8_t lstatus;            //LStatus—Learned status of resistance table (Bit 3: QMax, Bit 2: ITEN, Bit 1: CF1, Bit 0: CF0) 
                                        // CF1, CF0: QMax Status (0,0 = Battery OK, 0,1 = QMax is first updated in learning cycle, 
                                        // 1,0 = QMax and resistance table updated in learning cycle), ITEN: IT enable (0 = IT disabled, 
                                        // 1 = IT enabled), QMax: QMax field updates (0 = QMax is not updated in the field, 1= QMax is updated in the field)
            uint8_t cell_grid_1;        //Cell Grid 1. Active grid point of Cell1
            uint8_t cell_grid_2;        //Cell Grid 2. Active grid point of Cell2
            uint8_t reserved_0;         //Reserved for future use or manufacturer-specific information.
            uint8_t reserved_1;         //Reserved for future use or manufacturer-specific information.
            uint16_t state_time;        //State Time. Time passed since last state change (Discharge, Charge, Rest)
            int16_t dod0_1;             //DOD0_1. Depth of discharge for Cell1
            int16_t dod0_2;             //DOD0_2. Depth of discharge for Cell2
            int16_t dod0_passed_q;      //DOD0 Passed Q. Passed charge since DOD0
            int16_t dod0_passed_e;      //DOD0 Passed Energy. Passed energy since the last DOD0 update
            int16_t dod0_time;          //DOD0 Time. Time passed since the last DOD0 update
            int16_t dodeoc_1;           //DODEOC_1. Cell 1 DOD@EOC
            int16_t dodeoc_2;           //DODEOC_2. Cell 2 DOD@EOC
    } FURI_PACKED;
        uint8_t data[24];
    };
} FURI_PACKED Bq28z620MacSubcmdITStatus2RegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdITStatus2RegBits) == 24,
    "Size check for 'Bq28z620MacSubcmdITStatus2RegBits' failed."
);

/* 14.2.46 MACSubcmd() 0x0075 ITStatus3*/
typedef struct {
    union {
        struct {
            int16_t qmax_1;            //QMax 1. QMax of Cell 1
            int16_t qmax_2;            //QMax 2. QMax of Cell 2
            int16_t qmax_dod0_1;       //QMaxDOD0_1. Cell 1 DOD for Qmax
            int16_t qmax_dod0_2;       //QMaxDOD0_2. Cell 2 DOD for Qmax
            int16_t qmax_passed_q;     //QMaxPassedQ. Passed charge since DOD for Qmax recorded (mAh)
            int16_t qmax_time;         //QMaxTime. Time since DOD for Qmax recorded (hour / 16 units)
            int16_t tk;                //Tk. Thermal model “k”
            int16_t ta;                //Ta. Thermal model “a”
            int16_t raw_dod0_1;        //RawDOD0_1. Cell 1 raw DOD0 measurement
            int16_t raw_dod0_2;        //RawDOD0_2. Cell 2 raw DOD0 measurement
    } FURI_PACKED;
        uint8_t data[20];
    };
} FURI_PACKED Bq28z620MacSubcmdITStatus3RegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdITStatus3RegBits) == 20,
    "Size check for 'Bq28z620MacSubcmdITStatus3RegBits' failed."
);

/*14.2.47 MACSubcmd() 0x0076 CB Status*/
typedef struct {
    union {
        struct {
            uint16_t cb_time_1;        //CBTime1. Cell 1 balance time remaining
            uint16_t cb_time_2;        //CBTime2. Cell 2 balance time remaining
            int16_t cb_dod_1;          //CBDOD_1. Cell 1 DOD when balance calculated
            int16_t cb_dod_2;          //CBDOD_2. Cell 2 DOD when balance calculated
            int16_t cb_total_dod_chg;  //CBTotalDODChg. Total DOD charge when balance calculated
    } FURI_PACKED;
        uint8_t data[10];
    };
} FURI_PACKED Bq28z620MacSubcmdCBStatusRegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdCBStatusRegBits) == 10,
    "Size check for 'Bq28z620MacSubcmdCBStatusRegBits' failed."
);

/*14.2.52 MACSubcmd() 0xF081 Output CC and ADC for Calibration. Wait 250ms*/
typedef struct {
    union {
        struct {
            uint8_t counter;            //Rolling 8-bit counter, increments when values are refreshed.
            uint8_t status;             //Status, 1 when MACSubcmd() = 0xF081, 2 when MACSubcmd() = 0xF082
            int16_t current;            //Current (Coulomb Counter)
            uint16_t cell_voltage_1;    //Cell Voltage 1
            uint16_t cell_voltage_2;    //Cell Voltage 2
            uint16_t reserved0;         //Reserved for future use or manufacturer-specific information.
            uint16_t reserved1;         //Reserved for future use or manufacturer-specific information.
            uint16_t pack_voltage;      //PACK Voltage
            uint16_t vc2_voltage;       //VC2 (BAT) Voltage
            int16_t cell_current_1;     //Cell Current 1
            int16_t cell_current_2;     //Cell Current 2
            uint16_t reserved2;         //Reserved for future use or manufacturer-specific information.
            uint16_t reserved3;         //Reserved for future use or manufacturer-specific information.
    } FURI_PACKED;
        uint8_t data[24];
    };
} FURI_PACKED Bq28z620MacSubcmdOutputCCADCForCalibrationRegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdOutputCCADCForCalibrationRegBits) == 24,
    "Size check for 'Bq28z620MacSubcmdOutputCCADCForCalibrationRegBits' failed."
);

/*14.2.53 MACSubcmd() 0xF082 Output Shorted CC and ADC for Calibration. Wait 250ms*/
typedef struct {
    union {
        struct {
            uint8_t counter;            //Rolling 8-bit counter, increments when values are refreshed.
            uint8_t status;             //Status, 1 when MACSubcmd() = 0xF081, 2 when MACSubcmd() = 0xF082
            int16_t current;            //Current (Coulomb Counter)
            uint16_t cell_voltage_1;    //Cell Voltage 1
            uint16_t cell_voltage_2;    //Cell Voltage 2
            uint16_t reserved0;         //Reserved for future use or manufacturer-specific information.
            uint16_t reserved1;         //Reserved for future use or manufacturer-specific information.
            uint16_t pack_voltage;      //PACK Voltage
            uint16_t vc2_voltage;       //VC2 (BAT) Voltage
            int16_t cell_current_1;     //Cell Current 1
            int16_t cell_current_2;     //Cell Current 2
            uint16_t reserved2;         //Reserved for future use or manufacturer-specific information.
            uint16_t reserved3;         //Reserved for future use or manufacturer-specific information.
    } FURI_PACKED;
        uint8_t data[24];
    };
} FURI_PACKED Bq28z620MacSubcmdOutputShortedCCADCForCalibrationRegBits;
_Static_assert(
    sizeof(Bq28z620MacSubcmdOutputShortedCCADCForCalibrationRegBits) == 24,
    "Size check for 'Bq28z620MacSubcmdOutputShortedCCADCForCalibrationRegBits' failed."
);

/* clang-format on */