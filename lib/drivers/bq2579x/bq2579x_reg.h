#pragma once

#include <stdint.h>
#include <core/common_defines.h>
#include "bq2579x_helper.h"
// Unified register map for BQ25792 and BQ25798 (identical except for the fields marked per-chip below)
//https://www.ti.com/lit/ds/symlink/bq25792.pdf
//https://www.ti.com/lit/ds/symlink/bq25798.pdf

/* clang-format off */

typedef enum {
    Bq2579xRegMinimalSystemVoltage = 0x00, /** Minimal System Voltage Section 9.5.1.1 */
    Bq2579xRegChargeVoltageLimit = 0x01,   /** Charge Voltage Limit Section 9.5.1.2 */
    Bq2579xRegChargeCurrentLimit = 0x03,   /** Charge Current Limit Section 9.5.1.3 */
    Bq2579xRegInputVoltageLimit = 0x05,    /** Input Voltage Limit Section 9.5.1.4 */
    Bq2579xRegInputCurrentLimit = 0x06,    /** Input Current Limit Section 9.5.1.5 */
    Bq2579xRegPrechargeControl = 0x08,     /** Precharge Control Section 9.5.1.6 */
    Bq2579xRegTerminationControl = 0x09,   /** Termination Control Section 9.5.1.7 */
    Bq2579xRegRechargeControl = 0x0A,      /** Recharge Control Section 9.5.1.8 */
    Bq2579xRegVOTGRegulation = 0x0B,       /** VOTG Regulation Section 9.5.1.9 */
    Bq2579xRegIOTGRegulation = 0x0D,       /** IOTG Regulation Section 9.5.1.10 */
    Bq2579xRegTimerControl = 0x0E,         /** Timer Control Section 9.5.1.11 */
    Bq2579xRegChargerControl0 = 0x0F,      /** Charger Control 0 Section 9.5.1.12 */
    Bq2579xRegChargerControl1 = 0x10,      /** Charger Control 1 Section 9.5.1.13 */
    Bq2579xRegChargerControl2 = 0x11,      /** Charger Control 2 Section 9.5.1.14 */
    Bq2579xRegChargerControl3 = 0x12,      /** Charger Control 3 Section 9.5.1.15 */
    Bq2579xRegChargerControl4 = 0x13,      /** Charger Control 4 Section 9.5.1.16 */
    Bq2579xRegChargerControl5 = 0x14,      /** Charger Control 5 Section 9.5.1.17 */
    Bq2579xRegMPPTControl = 0x15,          /** MPPT Control Section 9.5.1.18 (BQ25798 only, reserved on BQ25792) */
    Bq2579xRegTemperatureControl = 0x16,   /** Temperature Control Section 9.5.1.19 */
    Bq2579xRegNTCControl0 = 0x17,          /** NTC Control 0 Section 9.5.1.20 */
    Bq2579xRegNTCControl1 = 0x18,          /** NTC Control 1 Section 9.5.1.21 */
    Bq2579xRegICOCurrentLimit = 0x19,      /** ICO Current Limit Section 9.5.1.22 */
    Bq2579xRegChargerStatus0 = 0x1B,       /** Charger Status 0 Section 9.5.1.23 */
    Bq2579xRegChargerStatus1 = 0x1C,       /** Charger Status 1 Section 9.5.1.24 */
    Bq2579xRegChargerStatus2 = 0x1D,       /** Charger Status 2 Section 9.5.1.25 */
    Bq2579xRegChargerStatus3 = 0x1E,       /** Charger Status 3 Section 9.5.1.26 */
    Bq2579xRegChargerStatus4 = 0x1F,       /** Charger Status 4 Section 9.5.1.27 */
    Bq2579xRegFaultStatus0 = 0x20,         /** Fault Status 0 Section 9.5.1.28 */
    Bq2579xRegFaultStatus1 = 0x21,         /** Fault Status 1 Section 9.5.1.29 */
    Bq2579xRegChargerFlag0 = 0x22,         /** Charger Flag 0 Section 9.5.1.30 */
    Bq2579xRegChargerFlag1 = 0x23,         /** Charger Flag 1 Section 9.5.1.31 */
    Bq2579xRegChargerFlag2 = 0x24,         /** Charger Flag 2 Section 9.5.1.32 */
    Bq2579xRegChargerFlag3 = 0x25,         /** Charger Flag 3 Section 9.5.1.33 */
    Bq2579xRegFaultFlag0 = 0x26,           /** Fault Flag 0 Section 9.5.1.34 */
    Bq2579xRegFaultFlag1 = 0x27,           /** Fault Flag 1 Section 9.5.1.35 */
    Bq2579xRegChargerMask0 = 0x28,         /** Charger Mask 0 Section 9.5.1.36 */
    Bq2579xRegChargerMask1 = 0x29,         /** Charger Mask 1 Section 9.5.1.37 */
    Bq2579xRegChargerMask2 = 0x2A,         /** Charger Mask 2 Section 9.5.1.38 */
    Bq2579xRegChargerMask3 = 0x2B,         /** Charger Mask 3 Section 9.5.1.39 */
    Bq2579xRegFaultMask0 = 0x2C,           /** Fault Mask 0 Section 9.5.1.40 */
    Bq2579xRegFaultMask1 = 0x2D,           /** Fault Mask 1 Section 9.5.1.41 */
    Bq2579xRegADCControl = 0x2E,           /** ADC Control Section 9.5.1.42 */
    Bq2579xRegADCFunctionDisable0 = 0x2F,  /** ADC Function Disable 0 Section 9.5.1.43 */
    Bq2579xRegADCFunctionDisable1 = 0x30,  /** ADC Function Disable 1 Section 9.5.1.44 */
    Bq2579xRegIBUSADC = 0x31,              /** IBUS ADC Section 9.5.1.45 */
    Bq2579xRegIBATADC = 0x33,              /** IBAT ADC Section 9.5.1.46 */
    Bq2579xRegVBUSADC = 0x35,              /** VBUS ADC Section 9.5.1.47 */
    Bq2579xRegVAC1ADC = 0x37,              /** VAC1 ADC Section 9.5.1.48 */
    Bq2579xRegVAC2ADC = 0x39,              /** VAC2 ADC Section 9.5.1.49 */
    Bq2579xRegVBATADC = 0x3B,              /** VBAT ADC Section 9.5.1.50 */
    Bq2579xRegVSYSADC = 0x3D,              /** VSYS ADC Section 9.5.1.51 */
    Bq2579xRegTSADC = 0x3F,                /** TS ADC Section 9.5.1.52 */
    Bq2579xRegTDIEADC = 0x41,              /** TDIE ADC Section 9.5.1.53 */
    Bq2579xRegDPlusADC = 0x43,             /** D+ ADC Section 9.5.1.54 */
    Bq2579xRegDMinusADC = 0x45,            /** D- ADC Section 9.5.1.55 */
    Bq2579xRegDPDMDriver = 0x47,           /** DPDM Driver Section 9.5.1.56 */
    Bq2579xRegPartInformation = 0x48,      /** Part Information Section 9.5.1.57 */
} Bq2579xReg;



typedef struct {
    uint8_t vsysmin : 6;    // Minimal System Voltage:
                             // During POR, the device reads the resistance tie to
                             // PROG pin, to identify the default battery cell count and
                             // determine the default power on VSYSMIN list below:
                             // 1s: 3.5V
                             // 2s: 7V
                             // 3s: 9V
                             // 4s: 12V
    uint8_t         : 2;    // Reserved
} Bq2579xMinimalSystemVoltageRegBits;
_Static_assert(
    sizeof(Bq2579xMinimalSystemVoltageRegBits) == 1,
    "Size check for 'Bq2579xMinimalSystemVoltageRegBits' failed.");

typedef struct {
    uint16_t vreg : 11;    // Battery Voltage Limit:
                            // During POR, the device reads the resistance tie to
                            // PROG pin, to identify the default battery cell count and
                            // determine the default power-on battery voltage regulation
                            // limit below:
                            // 1s: 4.2V
                            // 2s: 8.4V
                            // 3s: 12.6V
                            // 4s: 16.8V
                            // Type : RW
                            // Range : 3000mV-18800mV
                            // Fixed Offset : 0mV
                            // Bit Step Size : 10mV
                            // Clamped Low
    uint16_t      : 5;    // Reserved
} Bq2579xChargeVoltageLimitRegBits;
_Static_assert(
    sizeof(Bq2579xChargeVoltageLimitRegBits) == 2,
    "Size check for 'Bq2579xChargeVoltageLimitRegBits' failed.");

typedef struct {
    uint16_t ichg : 9;    // Charge Current Limit:
                            // During POR, the device reads the resistance tie to
                            // PROG pin, to identify the default battery cell count and
                            // determine the default power-on battery charging current
                            // below:
                            // 1s and 2s: 500mA
                            // 3s and 4s: 1000mA
                            // Type : RW
                            // Range : 50mA-5000mA
                            // Fixed Offset : 0mA
                            // Bit Step Size : 10mA
                            // Clamped Low
    uint16_t     : 7;    // Reserved
} Bq2579xChargeCurrentLimitRegBits;
_Static_assert(
    sizeof(Bq2579xChargeCurrentLimitRegBits) == 2,
    "Size check for 'Bq2579xChargeCurrentLimitRegBits' failed.");

typedef struct {
    uint8_t vindpm : 8;    // Absolute VINDPM Threshold
                            // VINDPM register is reset to 3600mV upon adapter unplugged and
                            // it is set to the value based on the VBUS measurement when
                            // the adapter plugs in. It is not reset by the REG_RST and the
                            // WATCHDOG
                            // Type : RW
                            // POR: 3600mV (24h)
                            // Range : 3600mV-22000mV
                            // Fixed Offset : 0mV
                            // Bit Step Size : 100mV
                            // Clamped Low
} Bq2579xInputVoltageLimitRegBits;
_Static_assert(
    sizeof(Bq2579xInputVoltageLimitRegBits) == 1,
    "Size check for 'Bq2579xInputVoltageLimitRegBits' failed.");

typedef struct {
    uint16_t iindpm : 9;    // Based on D+/D- detection results:
                             // USB SDP = 500mA
                             // USB CDP = 1.5A
                             // USB DCP = 3.25A
                             // Adjustable High Voltage DCP = 1.5A
                             // Unknown Adapter = 3A
                             // Non-Standard Adapter = 1A/2A/2.1A/2.4A
                             // Type : RW
                             // POR: 3000mA (12Ch)
                             // Range : 100mA-3300mA
                             // Fixed Offset : 0mA
                             // Bit Step Size : 10mA
                             // Clamped Low
    uint16_t        : 7;    // Reserved
} Bq2579xInputCurrentLimitRegBits;
_Static_assert(
    sizeof(Bq2579xInputCurrentLimitRegBits) == 2,
    "Size check for 'Bq2579xInputCurrentLimitRegBits' failed.");

typedef struct {
    uint8_t iprechg     : 6;  // Precharge current limit
                               // Type : RW
                               // POR: 120mA (3h)
                               // Range : 40mA-2000mA
                               // Fixed Offset : 0mA
                               // Bit Step Size : 40mA
                               // Clamped Low
    uint8_t vbat_lowv   : 2;  // Battery voltage thresholds for the transition from
                               // precharge to fast charge, which is defined as a ratio
                               // of battery regulation limit (VREG)
                               // Type : RW
                               // POR: 11b
                               // 0h = 15%*VREG
                               // 1h = 62.2%*VREG
                               // 2h = 66.7%*VREG
                               // 3h = 71.4%*VREG
} Bq2579xPrechargeControlRegBits;
_Static_assert(
    sizeof(Bq2579xPrechargeControlRegBits) == 1,
    "Size check for 'Bq2579xPrechargeControlRegBits' failed.");

typedef struct {
    uint8_t iterm   : 5;   // Termination current
                             // Type : RW
                             // POR: 200mA (5h)
                             // Range : 40mA-1000mA
                             // Fixed Offset : 0mA
                             // Bit Step Size : 40mA
                             // Clamped Low
    uint8_t         : 1;   // Reserved
    uint8_t reg_rst : 1;   // Reset registers to default values and reset timer
                             // Type : RW
                             // POR: 0b
                             // 0h = Not reset
                             // 1h = Reset
    uint8_t         : 1;  // Reserved
} Bq2579xTerminationControlRegBits;
_Static_assert(
    sizeof(Bq2579xTerminationControlRegBits) == 1,
    "Size check for 'Bq2579xTerminationControlRegBits' failed.");
 
typedef struct {
    uint8_t vrechg : 4;    // Battery Recharge Threshold Offset (Below VREG)
                            // Type : RW
                            // POR: 200mV (3h)
                            // Range : 50mV-800mV
                            // Fixed Offset : 50mV
                            // Bit Step Size : 50mV
    uint8_t trechg : 2;    // Battery recharge deglich time
                            // Type : RW
                            // POR: 10b
                            // 0h = 64ms
                            // 1h = 256ms
                            // 2h = 1024ms (default)
                            // 3h = 2048ms
    uint8_t cell : 2;      // At POR, the charger reads the PROG pin resistance to
                            // determine the battery cell count and update this CELL
                            // bits accordingly.
                            // Type : RW
                            // 0h = 1s
                            // 1h = 2s
                            // 2h = 3s
                            // 3h = 4s
} Bq2579xRechargeControlRegBits;  
_Static_assert(
    sizeof(Bq2579xRechargeControlRegBits) == 1,
    "Size check for 'Bq2579xRechargeControlRegBits' failed.");

typedef struct {
    uint16_t votg : 11;   // OTG mode regulation voltage
                            // Type : RW
                            // POR: 5000mV (DCh)
                            // Range : 2800mV-22000mV
                            // Fixed Offset : 2800mV
                            // Bit Step Size : 10mV
                            // Clamped High
    uint16_t      : 5;   // Reserved
} Bq2579xVOTGRegulationRegBits;
_Static_assert(
    sizeof(Bq2579xVOTGRegulationRegBits) == 2,
    "Size check for 'Bq2579xVOTGRegulationRegBits' failed.");

typedef struct {
    uint8_t iotg        : 7;  // OTG current limit
                               // Type : RW
                               // POR: 3000mA (4Bh)
                               // Range : 120mA-3320mA
                               // Fixed Offset : 0mA
                               // Bit Step Size : 40mA
                               // Clamped Low
    uint8_t prechg_tmr : 1;   // Pre-charge safety timer setting
                               // Type : RW
                               // POR: 0b
                               // 0h = 2 hrs (default)
                               // 1h = 0.5 hrs
} Bq2579xIOTGRegulationRegBits;
_Static_assert(
    sizeof(Bq2579xIOTGRegulationRegBits) == 1,
    "Size check for 'Bq2579xIOTGRegulationRegBits' failed.");

typedef struct {
    uint8_t tmr2x_en        : 1;   // TMR2X_EN
                                    // Type : RW
                                    // POR: 1b
                                    // 0h = Trickle charge, pre-charge and fast charge timer NOT slowed by 2X during input DPM or thermal regulation.
                                    // 1h = Trickle charge, pre-charge and fast charge timer slowed by 2X during input DPM or thermal regulation (default)
    uint8_t chg_tmr         : 2;   // Fast charge timer setting
                                    // Type : RW
                                    // POR: 10b
                                    // 0h = 5 hrs
                                    // 1h = 8 hrs
                                    // 2h = 12 hrs (default)
                                    // 3h = 24 hrs
    uint8_t en_chg_tmr      : 1;   // Enable fast charge timer
                                    // Type : RW
                                    // POR: 1b
                                    // 0h = Disabled
                                    // 1h = Enabled (default)
    uint8_t en_prechg_tmr   : 1;   // Enable pre-charge timer
                                    // Type : RW
                                    // POR: 1b
                                    // 0h = Disabled
                                    // 1h = Enabled (default)
    uint8_t en_trichg_tmr   : 1;   // Enable trickle charge timer (fixed as 1hr)
                                    // Type : RW
                                    // POR: 1b
                                    // 0h = Disabled
                                    // 1h = Enabled (default)
    uint8_t topoff_tmr      : 2;   // Top-off timer control
                                    // Type : RW
                                    // POR: 00b
                                    // 0h = Disabled (default)
                                    // 1h = 15 mins
                                    // 2h = 30 mins
                                    // 3h = 45 mins
} Bq2579xTimerControlRegBits;
_Static_assert(
    sizeof(Bq2579xTimerControlRegBits) == 1,
    "Size check for 'Bq2579xTimerControlRegBits' failed.");

typedef struct {
    uint8_t en_backup       : 1;   // BQ25798 only, reserved on BQ25792
                                    // Reset by WATCHDOG and REG_RST
                                    // Enables backup mode where OTG automatically engages
                                    // when VBUS droops below voltage set in VBUS_BACKUP
                                    // Type : RW
                                    // POR: 0b
                                    // 0h = Disable (default)
                                    // 1h = Enable
    uint8_t en_term         : 1;   // Reset by WATCHDOG and REG_RST
                                    // Enable termination
                                    // Type : RW
                                    // POR: 1b
                                    // 0h = Disable
                                    // 1h = Enable (default)
    uint8_t en_hiz          : 1;  // Reset by REG_RST and when the adapter is plugged in at VBUS.
                                    // Enable HIZ mode.
                                    // Type : RW
                                    // POR: 0b
                                    // 0h = Disable (default)
                                    // 1h = Enable
    uint8_t force_ico       : 1;  // Reset by WATCHDOG and REG_RST
                                    // Force start input current optimizer (ICO)
                                    // Note: This bit can only be set and returns 0 after ICO starts. This bit only valid when EN_ICO = 1
                                    // Type : RW
                                    // POR: 0b
                                    // 0h = Do NOT force ICO (Default)
                                    // 1h = Force ICO start
    uint8_t en_ico          : 1;  // Reset by REG_RST
                                    // Input Current Optimizer (ICO) Enable
                                    // Type : RW
                                    // POR: 0b
                                    // 0h = Disable ICO (default)
                                    // 1h = Enable ICO
    uint8_t en_chg          : 1;  // Reset by WATCHDOG and REG_RST
                                    // Charger Enable Configuration
                                    // Type : RW
                                    // POR: 1b
                                    // 0h = Charge Disable
                                    // 1h = Charge Enable (default)
    uint8_t force_ibatdis   : 1;  // Reset by REG_RST
                                    // Force a battery discharging current
                                    // Type : RW
                                    // POR: 0b
                                    // 0h = IDLE (default)
                                    // 1h = Force the charger to apply a discharging current on BAT regardless the battery OVP status
    uint8_t en_auto_ibatdis : 1;  // Reset by REG_RST
                                    // Enable the auto battery discharging during the battery OVP fault
                                    // Type : RW
                                    // POR: 1b
                                    // 0h = The charger will NOT apply a discharging current on BAT during battery OVP
                                    // 1h = The charger will apply a discharging current on BAT during battery OVP
} Bq2579xChargerControl0RegBits;
_Static_assert(
    sizeof(Bq2579xChargerControl0RegBits) == 1,
    "Size check for 'Bq2579xChargerControl0RegBits' failed.");

typedef struct {
    uint8_t watchdog : 3;   // Watchdog timer settings
                            // Type : RW
                            // POR: 101b
                            // 0h = Disable
                            // 1h = 0.5s
                            // 2h = 1s
                            // 3h = 2s
                            // 4h = 20s
                            // 5h = 40s (default)
                            // 6h = 80s
                            // 7h =160s
    uint8_t wd_rst  : 1;  // I2C watch dog timer reset
                            // Type : RW
                            // POR: 0b
                            // 0h = Normal (default)
                            // 1h = Reset (this bit goes back to 0 after timer resets)
    uint8_t vac_ovp : 2;  // VAC OVP thresholds (see Bq2579xVacOvp)
                            // Type : RW
                            // POR: BQ25792: 00b (26V default), BQ25798: 11b (7V default)
                            // 0h = 26V
                            // 1h = 18V on BQ25792, 22V on BQ25798
                            // 2h = 12V
                            // 3h = 7V
    uint8_t vbus_backup : 2;  // BQ25798 only, reserved on BQ25792
                            // The thresholds to trigger the backup mode,
                            // defined as a ratio of VINDPM
                            // Type : RW
                            // POR: 10b
                            // 0h = 40%*VINDPM
                            // 1h = 60%*VINDPM
                            // 2h = 80%*VINDPM (default)
                            // 3h = 100%*VINDPM
} Bq2579xChargerControl1RegBits;
_Static_assert(
    sizeof(Bq2579xChargerControl1RegBits) == 1,
    "Size check for 'Bq2579xChargerControl1RegBits' failed.");

typedef struct {
    uint8_t sdrv_dly        : 1;   // Delay time added to the taking action in bit [2:1] of the SFET control
                                    // Type : RW
                                    // POR: 0b
                                    // 0h = Add 10s delay time (default)
                                    // 1h = Do NOT add 10s delay time
    uint8_t sdrv_ctrl       : 2;   // SFET control The external ship FET control logic to force the device enter different modes.
                                    // Type : RW
                                    // POR: 00b
                                    // 0h = IDLE (default)
                                    // 1h = Shutdown Mode
                                    // 2h = Ship Mode
                                    // 3h = System Power Reset
    uint8_t hvdcp_en        : 1;   // High voltage DCP enable.
                                    // Type : RW
                                    // POR: 0b
                                    // 0h = Disable HVDCP handshake (default)
                                    // 1h = Enable HVDCP handshake
    uint8_t en_9v           : 1;   // EN_9V HVDC
                                    // Type : RW
                                    // POR: 0b
                                    // 0h = Disable 9V mode in HVDCP (default)
                                    // 1h = Enable 9V mode in HVDCP
    uint8_t en_12v          : 1;   // EN_12V HVDC
                                    // Type : RW
                                    // POR: 0b
                                    // 0h = Disable 12V mode in HVDCP (default)
                                    // 1h = Enable 12V mode in HVDCP
    uint8_t auto_indet_en   : 1;   // Automatic D+/D- Detection Enable
                                    // Type : RW
                                    // POR: 1b
                                    // 0h = Disable D+/D- detection when VBUS is plugged-in
                                    // 1h = Enable D+/D- detection when VBUS is plugged-in (default)
    uint8_t force_indet     : 1;   // Force D+/D- detection
                                    // Type : RW
                                    // POR: 0b
                                    // 0h = Do NOT force D+/D- detection (default)
                                    // 1h = Force D+/D- algorithm, when D+/D- detection is done, this bit will be reset to 0
} Bq2579xChargerControl2RegBits;
_Static_assert(
    sizeof(Bq2579xChargerControl2RegBits) == 1,
    "Size check for 'Bq2579xChargerControl2RegBits' failed.");

typedef struct {
    uint8_t dis_fwd_ooa     : 1;   // Disable OOA in forward mode
                                    // Type : RW
                                    // POR: 0b
                                    // 0h = Enable (Default)
                                    // 1h = Disable
    uint8_t dis_otg_ooa     : 1;   // Disable OOA in OTG mode
                                    // Type : RW
                                    // POR: 0b
                                    // 0h = Enable (Default)
                                    // 1h = Disable
    uint8_t dis_ldo         : 1;   // Disable BATFET LDO mode in pre-charge stage.
                                    // Type : RW
                                    // POR: 0b
                                    // 0h = Enable (Default)
                                    // 1h = Disable
    uint8_t wkup_dly        : 1;   // When wake up the device from ship mode, how much time (tSM_EXIT) is required to pull low the QON pin.
                                    // Type : RW
                                    // POR: 0b
                                    // 0h = 1s (Default)
                                    // 1h = 15ms
    uint8_t pfm_fwd_dis     : 1;   // Disable PFM in forward mode
                                    // Type : RW
                                    // POR: 0b
                                    // 0h = Enable (Default)
                                    // 1h = Disable
    uint8_t pfm_otg_dis     : 1;   // Disable PFM in OTG mode
                                    // Type : RW
                                    // POR: 0b
                                    // 0h = Enable (Default)
                                    // 1h = Disable
    uint8_t en_otg          : 1;   // OTG mode control
                                    // Type : RW
                                    // POR: 0b
                                    // 0h = OTG Disable (default)
                                    // 1h = OTG Enable
    uint8_t dis_acdrv       : 1;   // When this bit is set, the charger will force both EN_ACDRV1=0 and EN_ACDRV2=0
                                    // Type : RW
                                    // POR: 0b

} Bq2579xChargerControl3RegBits;
_Static_assert(
    sizeof(Bq2579xChargerControl3RegBits) == 1,
    "Size check for 'Bq2579xChargerControl3RegBits' failed.");

typedef struct {
    uint8_t en_ibus_ocp     : 1;   // Enable IBUS_OCP in forward mode
                                    // Type : RW
                                    // POR: 1b
                                    // 0h = Disable
                                    // 1h = Enable (default)
    uint8_t force_vindpm_det : 1;  // Force VINDPM detection Note: only when VBAT>VSYSMIN, this bit can be set to 1. Once the VINDPM auto detection is done, this bits returns to 0.
                                    // Type : RW
                                    // POR: 0b
                                    // 0h = Do NOT force VINDPM detection (default)
                                    // 1h = Force the converter stop switching, and ADC measures the VBUS voltage without input current, then the charger updates the VINDPM register accordingly.
    uint8_t dis_votg_uvp    : 1;   // Disable OTG mode VOTG UVP hiccup protection.
                                    // Type : RW
                                    // POR: 0b
                                    // 0h = Enable (Default)
                                    // 1h = Disable
    uint8_t dis_vsys_short   : 1;   // Disable forward mode VSYS short hiccup protection.
                                    // Type : RW
                                    // POR: 0b
                                    // 0h = Enable (Default)
                                    // 1h = Disable
    uint8_t dis_stat         : 1;   // Disable the STAT pin output
                                    // Type : RW
                                    // POR: 0b
                                    // 0h = Enable (Default)
                                    // 1h = Disable
    uint8_t pwm_freq         : 1;   // Switching frequency selection, this bit POR default value is based on the PROG pin strapping.
                                    // Type : RW
                                    // 0h = 1.5 MHz
                                    // 1h = 750 kHz
    uint8_t en_acdrv1        : 1;   // External ACFET1-RBFET1 gate driver control At POR, if the charger detects that there is no ACFET1-RBFET1 populated, this bit will be locked at 0
                                    // Type : RW
                                    // POR: 0b
                                    // 0h = turn off (default)
                                    // 1h = turn on
    uint8_t en_acdrv2        : 1;   // External ACFET2-RBFET2 gate driver control At POR, if the charger detects that there is no ACFET2-RBFET2 populated, this bit will be locked at 0
                                    // Type : RW
                                    // POR: 0b
                                    // 0h = turn off (default)
                                    // 1h = turn on
} Bq2579xChargerControl4RegBits;
_Static_assert(
    sizeof(Bq2579xChargerControl4RegBits) == 1,
    "Size check for 'Bq2579xChargerControl4RegBits' failed.");

typedef struct {
    uint8_t en_batoc        : 1;   // Enable the battery discharging current OCP
                                    // Type : RW
                                    // POR: 0b
                                    // 0h = Disable (default)
                                    // 1h = Enable
    uint8_t en_extilim      : 1;   // Enable the external ILIM_HIZ pin input current regulation
                                    // Type : RW
                                    // POR: 1b
                                    // 0h = Disable
                                    // 1h = Enable (default)
    uint8_t en_iindpm       : 1;   // Enable the internal IINDPM register input current regulation
                                    // Type : RW
                                    // POR: 1b
                                    // 0h = Disable
                                    // 1h = Enable (default)
    uint8_t ibat_reg        : 2;   // Battery discharging current regulation in OTG mode
                                    // Type : RW
                                    // POR: 10b
                                    // 0h = 3A
                                    // 1h = 4A
                                    // 2h = 5A (default)
                                    // 3h = Disable
    uint8_t en_ibat         : 1;   // IBAT discharge current sensing enable for ADC
                                    // Type : RW
                                    // POR: 0b
                                    // 0h = Disable IBAT discharge current sensing for ADC (default)
                                    // 1h = Enable the IBAT discharge current sensing for ADC
    uint8_t                 : 1;   // Reserved
    uint8_t sfet_present    : 1;   // The user has to set this bit based on whether a ship FET is populated or not. 
                                    // The POR default value is 0, which means the charger does not support all the
                                    // features associated with the ship FET. The register bits list below all are locked at 0. 
                                    // EN_BATOC=0 
                                    // FORCE_SFET_OFF=0 
                                    // SDRV_CTRL=00
                                    // When this bit is set to 1, the register bits list above become programmable, 
                                    // and the charger can support the features associated with the ship FET 
                                    // Type : RW 
                                    // POR: 0b 
                                    // 0h = No ship FET populated 
                                    // 1h = Ship FET populated
} Bq2579xChargerControl5RegBits;
_Static_assert(
    sizeof(Bq2579xChargerControl5RegBits) == 1,
    "Size check for 'Bq2579xChargerControl5RegBits' failed.");

// MPPT Control: BQ25798 only, reserved on BQ25792
typedef struct {
    uint8_t en_mppt     : 1;   // Enable the MPPT to measure the VBUS open circuit voltage
                                // Type : RW
                                // POR: 0b
                                // 0h = Disable (default)
                                // 1h = Enable
    uint8_t voc_rate    : 2;   // The time interval between two VBUS open circuit voltage measurements
                                // Type : RW
                                // POR: 01b
                                // 0h = 30s
                                // 1h = 2mins (default)
                                // 2h = 10mins
                                // 3h = 30mins
    uint8_t voc_dly     : 2;   // After the converter stops switching, the time delay before the VOC is measured
                                // Type : RW
                                // POR: 01b
                                // 0h = 50ms
                                // 1h = 300ms (default)
                                // 2h = 2s
                                // 3h = 5s
    uint8_t voc_pct     : 3;   // To set the VINDPM as a percentage of the VBUS open circuit voltage
                                // when the VOC measurement is done
                                // Type : RW
                                // POR: 101b
                                // 0h = 0.5625
                                // 1h = 0.625
                                // 2h = 0.6875
                                // 3h = 0.75
                                // 4h = 0.8125
                                // 5h = 0.875 (default)
                                // 6h = 0.9375
                                // 7h = 1
} Bq2579xMPPTControlRegBits;
_Static_assert(
    sizeof(Bq2579xMPPTControlRegBits) == 1,
    "Size check for 'Bq2579xMPPTControlRegBits' failed.");

typedef struct {
    uint8_t bkup_acfet1_on : 1; // BQ25798 only, reserved on BQ25792
                                // When the charger is operated in backup mode, ACFET1 is off.
                                // Setting this bit to 1, the charger clears the EN_BACKUP bit to 0,
                                // sets DIS_ACDRV=0 and EN_ACDRV1=1 to turn on the ACFET1.
                                // Type : RW
                                // POR: 0b
                                // 0h = IDLE (default)
                                // 1h = To turn on ACFET1 in backup mode
    uint8_t vac2_pd_en  : 1;   // Enable VAC2 pull down resistor
                                // Type : RW
                                // POR: 0b
                                // 0h = Disable (default)
                                // 1h = Enable
    uint8_t vac1_pd_en  : 1;   // Enable VAC1 pull down resistor
                                // Type : RW
                                // POR: 0b
                                // 0h = Disable (default)
                                // 1h = Enable
    uint8_t vbus_pd_en  : 1;   // Enable VBUS pull down resistor (6k Ohm)
                                // Type : RW
                                // POR: 0b
                                // 0h = Disable (default)
                                // 1h = Enable
    uint8_t tshut       : 2;   // Thermal shutdown thresholds.
                                // Type : RW
                                // POR: 00b
                                // 0h = 150°C (default)
                                // 1h = 130°C
                                // 2h = 120°C
                                // 3h = 85°C
    uint8_t treg        : 2;   // Thermal regulation thresholds.
                                // Type : RW
                                // POR: 11b
                                // 0h = 60°C
                                // 1h = 80°C
                                // 2h = 100°C
                                // 3h = 120°C (default)
} Bq2579xTemperatureControlRegBits;
_Static_assert(
    sizeof(Bq2579xTemperatureControlRegBits) == 1,
    "Size check for 'Bq2579xTemperatureControlRegBits' failed.");

typedef struct {
    uint8_t             : 1;   // Reserved
    uint8_t jeita_isetc : 2;   // JEITA low temperature range (TCOLD – TCOOL) charge current setting
                                // Type : RW
                                // POR: 01b
                                // 0h = Charge Suspend
                                // 1h = Set ICHG to 20%* ICHG (default)
                                // 2h = Set ICHG to 40%* ICHG
                                // 3h = ICHG unchanged
    uint8_t jeita_iseth : 2;   // JEITA high temperature range (TWARN – THOT) charge current setting
                                // Type : RW
                                // POR: 11b
                                // 0h = Charge Suspend
                                // 1h = Set ICHG to 20%* ICHG
                                // 2h = Set ICHG to 40%* ICHG
                                // 3h = ICHG unchanged (default)
    uint8_t jeita_vset  : 3;   // JEITA high temperature range (TWARN – THOT) charge voltage setting
                                // Type : RW
                                // POR: 011b
                                // 0h = Charge Suspend
                                // 1h = Set VREG to VREG-800mV
                                // 2h = Set VREG to VREG-600mV
                                // 3h = Set VREG to VREG-400mV (default)
                                // 4h = Set VREG to VREG-300mV
                                // 5h = Set VREG to VREG-200mV
                                // 6h = Set VREG to VREG-100mV
                                // 7h = VREG unchanged
} Bq2579xNTCControl0RegBits;
_Static_assert(
    sizeof(Bq2579xNTCControl0RegBits) == 1,
    "Size check for 'Bq2579xNTCControl0RegBits' failed.");

typedef struct {
    uint8_t ts_ignore   : 1;   // Ignore the TS feedback, the charger considers the TS is always good to allow the charging and OTG modes, all the four TS status bits always stay at 0000 to report the normal condition.
                                // Type : RW
                                // POR: 0b
                                // 0h = NOT ignore (Default)
                                // 1h = Ignore
    uint8_t bcold       : 1;   // OTG mode TS COLD temperature threshold
                                // Type : RW
                                // POR: 0b
                                // 0h = -10°C (default)
                                // 1h = -20°C
    uint8_t bhot        : 2;   // OTG mode TS HOT temperature threshold
                                // Type : RW
                                // POR: 01b
                                // 0h = 55°C
                                // 1h = 60°C (default)
                                // 2h = 65°C
                                // 3h = Disable
    uint8_t ts_warm     : 2;   // JEITA VT3 comparator voltage falling thresholds as a percentage of REGN. The corresponding temperature in the brackets is achieved when a 103AT NTC thermistor is used, RT1=5.24kΩ and RT2=30.31kΩ.
                                // Type : RW
                                // POR: 01b
                                // 0h = 48.4% (40°C)
                                // 1h = 44.8% (default) (45°C)
                                // 2h = 41.2% (50°C)
                                // 3h = 37.7% (55°C)
    uint8_t ts_cool     : 2;   // JEITA VT2 comparator voltage rising thresholds as a percentage of REGN. The corresponding temperature in the brackets is achieved when a 103AT NTC thermistor is used, RT1=5.24kΩ and RT2=30.31kΩ.
                                // Type : RW
                                // POR: 01b
                                // 0h = 71.1% (5°C)
                                // 1h = 68.4% (default) (10°C)
                                // 2h = 65.5% (15°C)
                                // 3h = 62.4% (20°C)
} Bq2579xNTCControl1RegBits;
_Static_assert(
    sizeof(Bq2579xNTCControl1RegBits) == 1,
    "Size check for 'Bq2579xNTCControl1RegBits' failed.");

typedef struct {
    uint16_t ico_ilim : 9; // Input Current Limit obtained from ICO or ILIM_HIZ pin setting
                            // Type : R
                            // POR: 0mA (0h)
                            // Range : 100mA-3300mA
                            // Fixed Offset : 0mA
                            // Bit Step Size : 10mA
                            // Clamped Low
    uint16_t         : 7;   // Reserved
} Bq2579xICOCurrentLimitRegBits;
_Static_assert(
    sizeof(Bq2579xICOCurrentLimitRegBits) == 2,
    "Size check for 'Bq2579xICOCurrentLimitRegBits' failed.");

typedef struct {
    uint8_t vbus_present_stat  : 1; // VBUS present status
                                     // Type : R
                                     // POR: 0b
                                     // 0h = VBUS NOT present
                                     // 1h = VBUS present (above present threshold)
    uint8_t ac1_present_stat   : 1; // VAC1 insert status
                                     // Type : R
                                     // POR: 0b
                                     // 0h = VAC1 NOT present
                                     // 1h = VAC1 present (above present threshold)
    uint8_t ac2_present_stat   : 1; // VAC2 insert status
                                     // Type : R
                                     // POR: 0b
                                     // 0h = VAC2 NOT present
                                     // 1h = VAC2 present (above present threshold)
    uint8_t pg_stat            : 1; // Power Good Status
                                     // Type : R
                                     // POR: 0b
                                     // 0h = NOT in power good status
                                     // 1h = Power good
    uint8_t poorsrc_stat       : 1; // Poor source detection status
                                     // BQ25792 only, reserved on BQ25798 (reads 0)
                                     // Type : R
                                     // POR: 0b
                                     // 0h = Normal
                                     // 1h = Weak adaptor detected
    uint8_t wd_stat            : 1; // I2C watch dog timer status
                                     // Type : R
                                     // POR: 0b
                                     // 0h = Normal
                                     // 1h = WD timer expired
    uint8_t vindpm_stat        : 1; // VINDPM status (forward mode) or VOTG status (OTG mode)
                                     // Type : R
                                     // POR: 0b
                                     // 0h = Normal
                                     // 1h = In VINDPM regulation or VOTG regulation
    uint8_t iindpm_stat        : 1; // IINDPM status (forward mode) or IOTG status (OTG mode)
                                     // Type : R
                                     // POR: 0b
                                     // 0h = Normal
                                     // 1h = In IINDPM regulation or IOTG regulation

} Bq2579xChargerStatus0RegBits;
_Static_assert(
    sizeof(Bq2579xChargerStatus0RegBits) == 1,
    "Size check for 'Bq2579xChargerStatus0RegBits' failed.");

typedef struct {
    uint8_t bc12_done_stat : 1; // BC1.2 status bit
                                 // Type : R
                                 // POR: 0b
                                 // 0h = BC1.2 or non-standard detection NOT complete
                                 // 1h = BC1.2 or non-standard detection complete
    Bq2579xChargerStatus1Vbus vbus_stat        : 4; // VBUS status bits
                                                    // Type : R
                                                    // POR: 0h
                                                    // 0h: No Input or BHOT or BCOLD in OTG mode
                                                    // 1h: USB SDP (500mA)
                                                    // 2h: USB CDP (1.5A)
                                                    // 3h: USB DCP (3.25A)
                                                    // 4h: Adjustable High Voltage DCP (HVDCP) (1.5A)
                                                    // 5h: Unknown adaptor (3A)
                                                    // 6h: Non-Standard Adapter (1A/2A/2.1A/2.4A)
                                                    // 7h: In OTG mode
                                                    // 8h: Not qualified adaptor
                                                    // 9h: Reserved
                                                    // Ah: Reserved
                                                    // Bh: Device directly powered from VBUS
                                                    // Ch: Reserved
                                                    // Dh: Reserved
                                                    // Eh: Reserved
                                                    // Fh: Reserved
    Bq2579xChargerStatus1Charge chg_stat       : 3; // Charge Status bits
                                                    // Type : R
                                                    // POR: 000b
                                                    // 0h = Not Charging
                                                    // 1h = Trickle Charge
                                                    // 2h = Pre-charge
                                                    // 3h = Fast charge (CC mode)
                                                    // 4h = Taper Charge (CV mode)
                                                    // 5h = Reserved
                                                    // 6h = Top-off Timer Active Charging
                                                    // 7h = Charge Termination Done
} Bq2579xChargerStatus1RegBits;
_Static_assert(
    sizeof(Bq2579xChargerStatus1RegBits) == 1,
    "Size check for 'Bq2579xChargerStatus1RegBits' failed.");

typedef struct {
    uint8_t vbat_present_stat : 1; // Battery present status (VBAT > VBAT_UVLOZ)
                                    // Type : R
                                    // POR: 0b
                                    // 0h = VBAT NOT present
                                    // 1h = VBAT present
    uint8_t dpdm_stat         : 1; // D+/D- detection status bits
                                    // Type : R
                                    // POR: 0b
                                    // 0h = The D+/D- detection is NOT started yet, or the detection is done
                                    // 1h = The D+/D- detection is ongoing
    uint8_t treg_stat         : 1; // IC thermal regulation status
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Device in thermal regulation
    uint8_t                   : 3; // RESERVED
    Bq2579xChargerStatus2Ico ico_stat          : 2; // Input Current Optimizer (ICO) status
                                                        // Type : R
                                                        // POR: 00b
                                                        // 0h = ICO disabled
                                                        // 1h = ICO optimization in progress
                                                        // 2h = Maximum input current detected
                                                        // 3h = Reserved
} Bq2579xChargerStatus2RegBits;
_Static_assert(
    sizeof(Bq2579xChargerStatus2RegBits) == 1,
    "Size check for 'Bq2579xChargerStatus2RegBits' failed.");

typedef struct {
    uint8_t                 : 1;   // Reserved
    uint8_t prechg_tmr_stat : 1;   // Pre-charge timer status
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Safety timer expired
    uint8_t trichg_tmr_stat : 1;   // Trickle charge timer status
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Safety timer expired
    uint8_t chg_tmr_stat    : 1;   // Fast charge timer status
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Safety timer expired
    uint8_t vsys_stat       : 1;   // VSYS Regulation Status (forward mode)
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Not in VSYSMIN regulation (VBAT > VSYSMIN)
                                    // 1h = In VSYSMIN regulation (VBAT < VSYSMIN)
    uint8_t adc_done_stat   : 1;   // ADC Conversion Status (in one-shot mode only)
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Conversion NOT complete
                                    // 1h = Conversion complete
    uint8_t acrb1_stat      : 1;   // The ACFET1-RBFET1 status
                                    // Type : R
                                    // POR: 0b
                                    // 0h = ACFET1-RBFET1 is NOT placed
                                    // 1h = ACFET1-RBFET1 is placed
    uint8_t acrb2_stat      : 1;   // The ACFET2-RBFET2 status
                                    // Type : R
                                    // POR: 0b
                                    // 0h = ACFET2-RBFET2 is NOT placed
                                    // 1h = ACFET2-RBFET2 is placed

} Bq2579xChargerStatus3RegBits;
_Static_assert(
    sizeof(Bq2579xChargerStatus3RegBits) == 1,
    "Size check for 'Bq2579xChargerStatus3RegBits' failed.");

typedef struct {
    uint8_t ts_hot_stat      : 1;   // The TS temperature is in the hot range, higher than T5.
                                     // Type : R
                                     // POR: 0b
                                     // 0h = TS status is NOT in hot range
                                     // 1h = TS status is in hot range
    uint8_t ts_warm_stat     : 1;   // The TS temperature is in the warm range, between T3 and T5.
                                     // Type : R
                                     // POR: 0b
                                     // 0h = TS status is NOT in warm range
                                     // 1h = TS status is in warm range
    uint8_t ts_cool_stat     : 1;   // The TS temperature is in the cool range, between T1 and T2.
                                     // Type : R
                                     // POR: 0b
                                     // 0h = TS status is NOT in cool range
                                     // 1h = TS status is in cool range
    uint8_t ts_cold_stat     : 1;   // The TS temperature is in the cold range, lower than T1.
                                     // Type : R
                                     // POR: 0b
                                     // 0h = TS status is NOT in cold range
                                     // 1h = TS status is in cold range
    uint8_t vbatotg_low_stat : 1;   // The battery voltage is too low to enable OTG mode.
                                     // Type : R
                                     // POR: 0b
                                     // 0h = The battery voltage is high enough to enable the OTG operation
                                     // 1h = The battery volage is too low to enable the OTG operation
    uint8_t                  : 3;   // RESERVED

} Bq2579xChargerStatus4RegBits;
_Static_assert(
    sizeof(Bq2579xChargerStatus4RegBits) == 1,
    "Size check for 'Bq2579xChargerStatus4RegBits' failed.");

typedef struct {
    uint8_t vac1_ovp_stat  : 1;   // VAC1 over-voltage status
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Device in over voltage protection
    uint8_t vac2_ovp_stat  : 1;   // VAC2 over-voltage status
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Device in over voltage protection
    uint8_t conv_ocp_stat  : 1;   // Converter over current status
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Converter in over current protection
    uint8_t ibat_ocp_stat  : 1;   // IBAT over-current status
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Device in over current protection
    uint8_t ibus_ocp_stat  : 1;   // IBUS over-current status
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Device in over current protection
    uint8_t vbat_ovp_stat  : 1;   // VBAT over-voltage status
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Device in over voltage protection
    uint8_t vbus_ovp_stat  : 1;   // VBUS over-voltage status
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Device in over voltage protection
    uint8_t ibat_reg_stat  : 1;   // IBAT regulation status
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Device in battery discharging current regulation

} Bq2579xFaultStatus0RegBits;
_Static_assert(
    sizeof(Bq2579xFaultStatus0RegBits) == 1,
    "Size check for 'Bq2579xFaultStatus0RegBits' failed.");

typedef struct {
    uint8_t                 : 2;   // Reserved
    uint8_t tshut_stat      : 1;   // IC temperature shutdown status
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Device in thermal shutdown protection
    uint8_t                 : 1;   // Reserved
    uint8_t otg_uvp_stat    : 1;   // OTG under voltage status.
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Device in OTG under voltage
    uint8_t otg_ovp_stat    : 1;   // OTG over voltage status
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Device in OTG over-voltage
    uint8_t vsys_ovp_stat   : 1;   // VSYS over-voltage status
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Device in SYS over-voltage protection
    uint8_t vsys_short_stat : 1;   // VSYS short circuit status
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Device in SYS short circuit protection
} Bq2579xFaultStatus1RegBits;
_Static_assert(
    sizeof(Bq2579xFaultStatus1RegBits) == 1,
    "Size check for 'Bq2579xFaultStatus1RegBits' failed.");

typedef struct {
    uint8_t vbus_present_flag  : 1; // VBUS present flag
                                     // Type : R
                                     // POR: 0b
                                     // 0h = Normal
                                     // 1h = VBUS present status changed
    uint8_t ac1_present_flag   : 1; // VAC1 present flag
                                     // Type : R
                                     // POR: 0b
                                     // 0h = Normal
                                     // 1h = VAC1 present status changed
    uint8_t ac2_present_flag   : 1; // VAC2 present flag
                                     // Type : R
                                     // POR: 0b
                                     // 0h = Normal
                                     // 1h = VAC2 present status changed
    uint8_t pg_flag            : 1; // Power good flag
                                     // Type : R
                                     // POR: 0b
                                     // 0h = Normal
                                     // 1h = Any change in PG_STAT even (adapter good qualification or adapter good going away)
    uint8_t poorsrc_flag       : 1; // Poor source detection flag
                                     // Type : R
                                     // POR: 0b
                                     // 0h = Normal
                                     // 1h = Poor source status rising edge detected
    uint8_t wd_flag            : 1; // I2C watchdog timer flag
                                     // Type : R
                                     // POR: 0b
                                     // 0h = Normal
                                     // 1h = WD timer signal rising edge detected
    uint8_t vindpm_flag        : 1; // VINDPM / VOTG Flag
                                     // Type : R
                                     // POR: 0b
                                     // 0h = Normal
                                     // 1h = VINDPM / VOTG regulation signal rising edge detected
    uint8_t iindpm_flag        : 1; // IINDPM / IOTG flag
                                     // Type : R
                                     // POR: 0b
                                     // 0h = Normal
                                     // 1h = IINDPM / IOTG signal rising edge detected
} Bq2579xChargerFlag0RegBits;
_Static_assert(
    sizeof(Bq2579xChargerFlag0RegBits) == 1,
    "Size check for 'Bq2579xChargerFlag0RegBits' failed.");

typedef struct {
    uint8_t bc12_done_flag    : 1; // BC1.2 status Flag
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = BC1.2 detection status changed
    uint8_t vbat_present_flag : 1; // VBAT present flag
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = VBAT present status changed
    uint8_t treg_flag         : 1; // IC thermal regulation flag
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = TREG signal rising threshold detected
    uint8_t                   : 1; // Reserved
    uint8_t vbus_flag         : 1; // VBUS status flag
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = VBUS status changed
    uint8_t                   : 1; // Reserved
    uint8_t ico_flag          : 1; // ICO status flag
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = ICO status changed
    uint8_t chg_flag          : 1; // Charge status flag
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Charge status changed
} Bq2579xChargerFlag1RegBits;
_Static_assert(
    sizeof(Bq2579xChargerFlag1RegBits) == 1,
    "Size check for 'Bq2579xChargerFlag1RegBits' failed.");

typedef struct {
    uint8_t topoff_tmr_flag  : 1;  // Top off timer flag
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Top off timer expired rising edge detected
    uint8_t prechg_tmr_flag : 1;   // Pre-charge timer flag
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Pre-charge timer expired rising edge detected
    uint8_t trichg_tmr_flag : 1;   // Trickle charge timer flag
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Trickle charger timer expired rising edge detected
    uint8_t chg_tmr_flag    : 1;   // Fast charge timer flag
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Fast charge timer expired rising edge detected
    uint8_t vsys_flag       : 1;   // VSYSMIN regulation flag
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Entered or existed VSYSMIN regulation
    uint8_t adc_done_flag   : 1;   // ADC conversion flag (only in one-shot mode)
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Conversion NOT completed
                                    // 1h = Conversion completed
    uint8_t dpdm_done_flag   : 1;  // D+/D- detection is done flag.
                                    // Type : R
                                    // POR: 0b
                                    // 0h = D+/D- detection is NOT started or still ongoing
                                    // 1h = D+/D- detection is completed
    uint8_t                  : 1;  // Reserved
} Bq2579xChargerFlag2RegBits;
_Static_assert(
    sizeof(Bq2579xChargerFlag2RegBits) == 1,
    "Size check for 'Bq2579xChargerFlag2RegBits' failed.");

typedef struct {
    uint8_t ts_hot_flag     : 1;   // TS hot temperature flag
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = TS across hot temperature (T5) is detected
    uint8_t ts_warm_flag    : 1;   // TS warm temperature flag
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = TS across warm temperature (T3) is detected
    uint8_t ts_cool_flag    : 1;   // TS cool temperature flag
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = TS across cool temperature (T2) is detected
    uint8_t ts_cold_flag    : 1;   // TS cold temperature flag
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = TS across cold temperature (T1) is detected
    uint8_t vbatotg_low_flag : 1; // VBAT too low to enable OTG flag
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = VBAT falls below the threshold to enable the OTG mode
    uint8_t                 : 3;   // Reserved
} Bq2579xChargerFlag3RegBits;
_Static_assert(
    sizeof(Bq2579xChargerFlag3RegBits) == 1,
    "Size check for 'Bq2579xChargerFlag3RegBits' failed.");

typedef struct {
    uint8_t vac1_ovp_flag   : 1;   // VAC1 over-voltage flag
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Enter VAC1 OVP
    uint8_t vac2_ovp_flag   : 1;   // VAC2 over-voltage flag
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Enter VAC2 OVP
    uint8_t conv_ocp_flag   : 1;   // Converter over-current flag
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Enter converter OCP
    uint8_t ibat_ocp_flag   : 1;   // IBAT over-current flag
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Enter discharged OCP
    uint8_t ibus_ocp_flag   : 1;   // IBUS over-current flag
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Enter IBUS OCP
    uint8_t vbat_ovp_flag   : 1;   // VBAT over-voltage flag
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Enter VBAT OVP
    uint8_t vbus_ovp_flag   : 1;   // VBUS over-voltage flag
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Enter VBUS OVP
    uint8_t ibat_reg_flag   : 1;   // IBAT regulation flag
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Enter or exit IBAT regulation
} Bq2579xFaultFlag0RegBits;
_Static_assert(
    sizeof(Bq2579xFaultFlag0RegBits) == 1,
    "Size check for 'Bq2579xFaultFlag0RegBits' failed.");

typedef struct {
    uint8_t                 : 2;   // Reserved
    uint8_t tshut_flag      : 1;   // IC thermal shutdown flag
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = TS shutdown signal rising threshold detected
    uint8_t                 : 1;   // Reserved
    uint8_t otg_uvp_flag    : 1;   // OTG under-voltage flag
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Stop OTG due to VBUS under-voltage
    uint8_t otg_ovp_flag    : 1;   // OTG over-voltage flag
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Stop OTG due to VBUS over voltage
    uint8_t vsys_ovp_flag   : 1;   // VSYS over-voltage flag
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Stop switching due to system over-voltage
    uint8_t vsys_short_flag : 1;   // VSYS short circuit flag
                                    // Type : R
                                    // POR: 0b
                                    // 0h = Normal
                                    // 1h = Stop switching due to system short
} Bq2579xFaultFlag1RegBits;
_Static_assert(
    sizeof(Bq2579xFaultFlag1RegBits) == 1,
    "Size check for 'Bq2579xFaultFlag1RegBits' failed.");

typedef struct {
    uint8_t vbus_present_mask   : 1;   // VBUS present mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = VBUS present status change does produce INT pulse
                                        // 1h = VBUS present status change does NOT produce INT pulse
    uint8_t ac1_present_mask    : 1;   // VAC1 present mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = VAC1 present status change does produce INT
                                        // 1h = VAC1 present status change does NOT produce INT
    uint8_t ac2_present_mask    : 1;   // VAC2 present mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = VAC2 present status change does produce INT
                                        // 1h = VAC2 present status change does NOT produce INT
    uint8_t pg_mask             : 1;   // Power Good mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = PG toggle does produce INT pulse
                                        // 1h = PG toggle does NOT produce INT pulse
    uint8_t poorsrc_mask        : 1;   // Poor source detection mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = Poor source detected does produce INT
                                        // 1h = Poor source detected does NOT produce INT
    uint8_t wd_mask             : 1;   // I2C watch dog timer mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = I2C watch dog timer expired does produce INT pulse
                                        // 1h = I2C watch dog timer expired does NOT produce INT pulse
    uint8_t vindpm_mask         : 1;   // VINDPM / VOTG mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = Enter VINDPM / VOTG does produce INT pulse
                                        // 1h = Enter VINDPM / VOTG does NOT produce INT pulse
    uint8_t iindpm_mask         : 1;   // IINDPM / IOTG mask flag
                                        // Type : RW
                                        // POR: 0
                                        // 0h = Enter IINDPM / IOTG does produce INT pulse
                                        // 1h = Enter IINDPM / IOTG does NOT produce INT pulse
} Bq2579xChargerMask0RegBits;
_Static_assert(
    sizeof(Bq2579xChargerMask0RegBits) == 1,
    "Size check for 'Bq2579xChargerMask0RegBits' failed.");

typedef struct {
    uint8_t bc12_done_mask      : 1;   // BC1.2 status mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = BC1.2 status change does produce INT
                                        // 1h = BC1.2 status change does NOT produce INT
    uint8_t vbat_present_mask   : 1;   // VBAT present mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = VBAT present status change does produce INT
                                        // 1h = VBAT present status change does NOT produce INT
    uint8_t treg_mask           : 1;   // IC thermal regulation mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = entering TREG does produce INT
                                        // 1h = entering TREG does NOT produce INT
    uint8_t                     : 1;   // Reserved
    uint8_t vbus_mask           : 1;   // VBUS status mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = VBUS status change does produce INT
                                        // 1h = VBUS status change does NOT produce INT
    uint8_t                     : 1;   // Reserved
    uint8_t ico_mask            : 1;   // ICO status mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = ICO status change does produce INT
                                        // 1h = ICO status change does NOT produce INT
    uint8_t chg_mask            : 1;   // Charge status mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = Charging status change does produce INT
                                        // 1h = Charging status change does NOT produce INT
} Bq2579xChargerMask1RegBits;
_Static_assert(
    sizeof(Bq2579xChargerMask1RegBits) == 1,
    "Size check for 'Bq2579xChargerMask1RegBits' failed.");

typedef struct {
    uint8_t topoff_tmr_mask     : 1;  // Top off timer mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = Top off timer expire does produce INT
                                        // 1h = Top off timer expire does NOT produce INT
    uint8_t prechg_tmr_mask     : 1;  // Pre-charge timer mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = Pre-charge timer expire does produce INT
                                        // 1h = Pre-charge timer expire does NOT produce INT
    uint8_t trichg_tmr_mask     : 1;  // Trickle charge timer mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = Trickle charge timer expire does produce INT
                                        // 1h = Trickle charge timer expire does NOT produce INT
    uint8_t chg_tmr_mask        : 1;  // Fast charge timer mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = Fast charge timer expire does produce INT
                                        // 1h = Fast charge timer expire does NOT produce INT
    uint8_t vsys_mask           : 1;  // VSYS min regulation mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = enter or exit VSYSMIN regulation does produce INT pulse
                                        // 1h = enter or exit VSYSMIN regulation does NOT produce INT pulse
    uint8_t adc_done_mask       : 1;  // ADC conversion mask flag (only in one-shot mode)
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = ADC conversion done does produce INT pulse
                                        // 1h = ADC conversion done does NOT produce INT pulse
    uint8_t dpdm_done_mask      : 1;  // D+/D- detection is done mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = D+/D- detection done does produce INT pulse
                                        // 1h = D+/D- detection done does NOT produce INT pulse
    uint8_t                     : 1;  // Reserved
} Bq2579xChargerMask2RegBits;
_Static_assert(
    sizeof(Bq2579xChargerMask2RegBits) == 1,
    "Size check for 'Bq2579xChargerMask2RegBits' failed.");

typedef struct {
    uint8_t ts_hot_mask         : 1;  // TS hot temperature interrupt mask
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = TS across hot temperature (T5) does produce INT
                                        // 1h = TS across hot temperature (T5) does NOT produce INT
    uint8_t ts_warm_mask        : 1;  // TS warm temperature interrupt mask
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = TS across warm temperature (T3) does produce INT
                                        // 1h = TS across warm temperature (T3) does NOT produce INT
    uint8_t ts_cool_mask        : 1;  // TS cool temperature interrupt mask
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = TS across cool temperature (T2) does produce INT
                                        // 1h = TS across cool temperature (T2) does NOT produce INT
    uint8_t ts_cold_mask        : 1;  // TS cold temperature interrupt mask
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = TS across cold temperature (T1) does produce INT
                                        // 1h = TS across cold temperature (T1) does NOT produce INT
    uint8_t vbatotg_low_mask    : 1;  // VBAT too low to enable OTG mask
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = VBAT falling below the threshold to enable the OTG mode, does produce INT
                                        // 1h = VBAT falling below the threshold to enable the OTG mode, does NOT produce INT
    uint8_t                     : 3;  // Reserved
} Bq2579xChargerMask3RegBits;
_Static_assert(
    sizeof(Bq2579xChargerMask3RegBits) == 1,
    "Size check for 'Bq2579xChargerMask3RegBits' failed.");
 
typedef struct {
    uint8_t vac1_ovp_mask       : 1;  // VAC1 over-voltage mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = entering VAC1 OVP does produce INT
                                        // 1h = entering VAC1 OVP does NOT produce INT
    uint8_t vac2_ovp_mask       : 1;  // VAC2 over-voltage mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = entering VAC2 OVP does produce INT
                                        // 1h = entering VAC2 OVP does NOT produce INT
    uint8_t conv_ocp_mask       : 1;  // Converter over-current mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = Converter OCP fault does produce INT
                                        // 1h = Converter OCP fault does NOT produce INT
    uint8_t ibat_ocp_mask       : 1;  // IBAT over-current mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = IBAT OCP fault does produce INT
                                        // 1h = IBAT OCP fault does NOT produce INT
    uint8_t ibus_ocp_mask       : 1;  // IBUS over-current mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = IBUS OCP fault does produce INT
                                        // 1h = IBUS OCP fault does NOT produce INT
    uint8_t vbat_ovp_mask       : 1;  // VBAT over-voltage mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = entering VBAT OVP does produce INT
                                        // 1h = entering VBAT OVP does NOT produce INT
    uint8_t vbus_ovp_mask       : 1;  // VBUS over-voltage mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = entering VBUS OVP does produce INT
                                        // 1h = entering VBUS OVP does NOT produce INT
    uint8_t ibat_reg_mask       : 1;  // IBAT regulation mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = enter or exit IBAT regulation does produce INT
                                        // 1h = enter or exit IBAT regulation does NOT produce INT
} Bq2579xFaultMask0RegBits;
_Static_assert(
    sizeof(Bq2579xFaultMask0RegBits) == 1,
    "Size check for 'Bq2579xFaultMask0RegBits' failed.");
 
typedef struct {
    uint8_t                     : 2;  // Reserved
    uint8_t tshut_mask          : 1;  // IC thermal shutdown mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = TSHUT does produce INT
                                        // 1h = TSHUT does NOT produce INT
    uint8_t                     : 1;  // Reserved
    uint8_t otg_uvp_mask        : 1;  // OTG under-voltage mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = OTG VBUS under voltage fault does produce INT
                                        // 1h = OTG VBUS under voltage fault does NOT produce INT
    uint8_t otg_ovp_mask        : 1;  // OTG over-voltage mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = OTG VBUS over-voltage fault does produce INT
                                        // 1h = OTG VBUS over-voltage fault does NOT produce INT
    uint8_t vsys_ovp_mask       : 1;  // VSYS over-voltage mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = System over-voltage fault does produce INT
                                        // 1h = System over-voltage fault does NOT produce INT
    uint8_t vsys_short_mask     : 1;  // VSYS short circuit mask flag
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = System short fault does produce INT
                                        // 1h = System short fault does NOT produce INT
} Bq2579xFaultMask1RegBits;
_Static_assert(
    sizeof(Bq2579xFaultMask1RegBits) == 1,
    "Size check for 'Bq2579xFaultMask1RegBits' failed.");
 
typedef struct {
    uint8_t                     : 2;  // Reserved
    uint8_t adc_avg_init        : 1;  // ADC average initial value control
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = Start average using the existing register value
                                        // 1h = Start average using a new ADC conversion
    uint8_t adc_avg             : 1;  // ADC average control
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = Single value
                                        // 1h = Running average
    uint8_t adc_sample          : 2;  // ADC sample speed
                                        // Type : RW
                                        // POR: 11b
                                        // 0h = 15 bit effective resolution
                                        // 1h = 14 bit effective resolution
                                        // 2h = 13 bit effective resolution
                                        // 3h = 12 bit effective resolution (default - not recommended)
    uint8_t adc_rate            : 1;  // ADC conversion rate control
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = Continuous conversion
                                        // 1h = One shot conversion
    uint8_t adc_en              : 1;  // ADC Control
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = Disable
                                        // 1h = Enable
} Bq2579xAdcControlRegBits;
_Static_assert(
    sizeof(Bq2579xAdcControlRegBits) == 1,
    "Size check for 'Bq2579xAdcControlRegBits' failed.");
 
typedef struct {
    uint8_t                     : 1;  // Reserved
    uint8_t tdie_adc_dis        : 1;  // TDIE ADC control
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = Enable (Default)
                                        // 1h = Disable
    uint8_t ts_adc_dis          : 1;  // TS ADC control
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = Enable (Default)
                                        // 1h = Disable
    uint8_t vsys_adc_dis        : 1;  // VSYS ADC control
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = Enable (Default)
                                        // 1h = Disable
    uint8_t vbat_adc_dis        : 1;  // VBAT ADC control
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = Enable (Default)
                                        // 1h = Disable
    uint8_t vbus_adc_dis        : 1;  // VBUS ADC control
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = Enable (Default)
                                        // 1h = Disable
    uint8_t ibat_adc_dis        : 1;  // IBAT ADC control
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = Enable (Default)
                                        // 1h = Disable
    uint8_t ibus_adc_dis        : 1;  // IBUS ADC control
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = Enable (Default)
                                        // 1h = Disable
} Bq2579xAdcFunctionDisable0RegBits;
_Static_assert(
    sizeof(Bq2579xAdcFunctionDisable0RegBits) == 1,
    "Size check for 'Bq2579xAdcFunctionDisable0RegBits' failed.");
 
typedef struct {
    uint8_t                     : 4;  // Reserved
    uint8_t vac1_adc_dis        : 1;  // VAC1 ADC Control
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = Enable (Default)
                                        // 1h = Disable
    uint8_t vac2_adc_dis        : 1;  // VAC2 ADC Control
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = Enable (Default)
                                        // 1h = Disable
    uint8_t dm_adc_dis          : 1;  // D- ADC Control
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = Enable (Default)
                                        // 1h = Disable
    uint8_t dp_adc_dis          : 1;  // D+ ADC Control
                                        // Type : RW
                                        // POR: 0b
                                        // 0h = Enable (Default)
                                        // 1h = Disable
} Bq2579xAdcFunctionDisable1RegBits;
_Static_assert(
    sizeof(Bq2579xAdcFunctionDisable1RegBits) == 1,
    "Size check for 'Bq2579xAdcFunctionDisable1RegBits' failed.");
 
typedef struct {
    int16_t ibus_adc;   // IBUS ADC reading
                        // Reported in 2's Complement.
                        // When the current is flowing from VBUS to PMID, IBUS ADC reports positive value, and when the current is flowing from PMID to VBUS, IBUS ADC reports negative value.
                        // Type : R
                        // POR: 0mA (0h)
                        // Range : 0mA-5000mA
                        // Fixed Offset : 0mA
                        // Bit Step Size : 1mA
} Bq2579xIbusAdcRegBits;
_Static_assert(
    sizeof(Bq2579xIbusAdcRegBits) == 2,
    "Size check for 'Bq2579xIbusAdcRegBits' failed.");
 
typedef struct {
    int16_t ibat_adc;   // IBAT ADC reading
                        // Reported in 2's Complement.
                        // The IBAT ADC reports positive value for the battery charging current, and negative value for the battery discharging current if EN_IBAT in REG0x14[5] = 1.
                        // Type : R
                        // POR: 0mA (0h)
                        // Range : 0mA-8000mA
                        // Fixed Offset : 0mA
                        // Bit Step Size : 1mA
} Bq2579xIbatAdcRegBits;
_Static_assert(
    sizeof(Bq2579xIbatAdcRegBits) == 2,
    "Size check for 'Bq2579xIbatAdcRegBits' failed.");
 
typedef struct {
    uint16_t vbus_adc;  // VBUS ADC reading
                        // Type : R
                        // POR: 0mV (0h)
                        // Range : 0mV-30000mV
                        // Fixed Offset : 0mV
                        // Bit Step Size : 1mV
} Bq2579xVbusAdcRegBits;
_Static_assert(
    sizeof(Bq2579xVbusAdcRegBits) == 2,
    "Size check for 'Bq2579xVbusAdcRegBits' failed.");
 
typedef struct {
    uint16_t vac1_adc;  // VAC1 ADC reading.
                        // Type : R
                        // POR: 0mV (0h)
                        // Range : 0mV-30000mV
                        // Fixed Offset : 0mV
                        // Bit Step Size : 1mV
} Bq2579xVac1AdcRegBits;
_Static_assert(
    sizeof(Bq2579xVac1AdcRegBits) == 2,
    "Size check for 'Bq2579xVac1AdcRegBits' failed.");
 
typedef struct {
    uint16_t vac2_adc;  // VAC2 ADC reading.
                        // Type : R
                        // POR: 0mV (0h)
                        // Range : 0mV-30000mV
                        // Fixed Offset : 0mV
                        // Bit Step Size : 1mV
} Bq2579xVac2AdcRegBits;
_Static_assert(
    sizeof(Bq2579xVac2AdcRegBits) == 2,
    "Size check for 'Bq2579xVac2AdcRegBits' failed.");
 
typedef struct {
    uint16_t vbat_adc;  // The battery remote sensing voltage (VBATP-GND) ADC reading
                        // Type : R
                        // POR: 0mV (0h)
                        // Range : 0mV-20000mV
                        // Fixed Offset : 0mV
                        // Bit Step Size : 1mV
} Bq2579xVbatAdcRegBits;
_Static_assert(
    sizeof(Bq2579xVbatAdcRegBits) == 2,
    "Size check for 'Bq2579xVbatAdcRegBits' failed.");
 
typedef struct {
    uint16_t vsys_adc;  // VSYS ADC reading
                        // Type : R
                        // POR: 0mV (0h)
                        // Range : 0mV-24000mV
                        // Fixed Offset : 0mV
                        // Bit Step Size : 1mV
} Bq2579xVsysAdcRegBits;
_Static_assert(
    sizeof(Bq2579xVsysAdcRegBits) == 2,
    "Size check for 'Bq2579xVsysAdcRegBits' failed.");
 
typedef struct {
    uint16_t ts_adc;  // TS ADC reading
                        // Type : R
                        // POR: 0% (0h)
                        // Range : 0%-99.9023%
                        // Fixed Offset : 0%
                        // Bit Step Size : 0.0976563%
} Bq2579xTsAdcRegBits;
_Static_assert(
    sizeof(Bq2579xTsAdcRegBits) == 2,
    "Size check for 'Bq2579xTsAdcRegBits' failed.");
 
typedef struct {
    int16_t tdie_adc;   // TDIE ADC reading
                        // Reported in 2's Complement.
                        // Type : R
                        // POR: 0°C (0h)
                        // Range : -40°C-150°C
                        // Fixed Offset : 0°C
                        // Bit Step Size : 0.5°C
} Bq2579xTdieAdcRegBits;
_Static_assert(
    sizeof(Bq2579xTdieAdcRegBits) == 2,
    "Size check for 'Bq2579xTdieAdcRegBits' failed.");
 
typedef struct {
    uint16_t dp_adc;  // D+ ADC reading
                        // Type : R
                        // POR: 0mV (0h)
                        // Range : 0mV-3600mV
                        // Fixed Offset : 0mV
                        // Bit Step Size : 1mV
} Bq2579xDpAdcRegBits;
_Static_assert(
    sizeof(Bq2579xDpAdcRegBits) == 2,
    "Size check for 'Bq2579xDpAdcRegBits' failed.");
 
typedef struct {
    uint16_t dm_adc;  // D- ADC reading
                        // Type : R
                        // POR: 0mV (0h)
                        // Range : 0mV-3600mV
                        // Fixed Offset : 0mV
                        // Bit Step Size : 1mV
} Bq2579xDmAdcRegBits;
_Static_assert(
    sizeof(Bq2579xDmAdcRegBits) == 2,
    "Size check for 'Bq2579xDmAdcRegBits' failed.");
 
typedef struct {
    uint8_t                     : 2;  // Reserved
    uint8_t dminus_dac          : 3;  // D- Output Driver
                                        // Type : RW
                                        // POR: 000b
                                        // 0h = HIZ
                                        // 1h = 0
                                        // 2h = 0.6V
                                        // 3h = 1.2V
                                        // 4h = 2.0V
                                        // 5h = 2.7V
                                        // 6h = 3.3V
                                        // 7h = reserved
    uint8_t dplus_dac           : 3;  // D+ Output Driver
                                        // Type : RW
                                        // POR: 000b
                                        // 0h = HIZ
                                        // 1h = 0
                                        // 2h = 0.6V
                                        // 3h = 1.2V
                                        // 4h = 2.0V
                                        // 5h = 2.7V
                                        // 6h = 3.3V
                                        // 7h = D+/D- Short
} Bq2579xDpDmDriverRegBits;
_Static_assert(
    sizeof(Bq2579xDpDmDriverRegBits) == 1,
    "Size check for 'Bq2579xDpDmDriverRegBits' failed.");
 
typedef struct {
    uint8_t dev_rev             : 3;  // Device Revision
                                        // 000b = BQ25792, 001b = BQ25798
                                        // Type : R
    uint8_t pn                  : 3;  // Device Part number
                                        // 001b = BQ25792, 011b = BQ25798
                                        // All the other options are reserved
                                        // Type : R
    uint8_t                     : 2;  // Reserved
} Bq2579xPartInformationRegBits;
_Static_assert(
    sizeof(Bq2579xPartInformationRegBits) == 1,
    "Size check for 'Bq2579xPartInformationRegBits' failed.");

typedef struct {
    union {
        struct {
            // STATUS 0
            Bq2579xChargerStatus0RegBits stat0;
            // STATUS 1
            Bq2579xChargerStatus1RegBits stat1;
            // STATUS 2
            Bq2579xChargerStatus2RegBits stat2;
            // STATUS 3
            Bq2579xChargerStatus3RegBits stat3;
            // STATUS 4
            Bq2579xChargerStatus4RegBits stat4;
        } FURI_PACKED;
        uint8_t data[5];
    };
} FURI_PACKED Bq2579xChargerStatusReg;

typedef struct {
    union {
        struct {
            // FAULT 0
            Bq2579xFaultStatus0RegBits fault0;
            // FAULT 1
            Bq2579xFaultStatus1RegBits fault1;
        } FURI_PACKED;
        uint8_t data[2];
    };
} FURI_PACKED Bq2579xFaultStatusReg;

typedef struct {
    union {
        struct {
            // FLAG 0
            Bq2579xChargerFlag0RegBits flag0;
            // FLAG 1
            Bq2579xChargerFlag1RegBits flag1;
            // FLAG 2
            Bq2579xChargerFlag2RegBits flag2;
            // FLAG 3
            Bq2579xChargerFlag3RegBits flag3;
        } FURI_PACKED;
        uint8_t data[4];
    };
} FURI_PACKED Bq2579xChargerFlagReg;

/* clang-format on */