#pragma once
/* clang-format off */
// http://azoteq.com/images/stories/pdf/iqs7211e_datasheet.pdf

#include <stdint.h>

#define IQS7211E_PRODUCT_NUM 0x0458 /**< IQS7211E Product Number */

typedef enum {
    /** 0x00 PRODUCT_NUM
     *   - Product ID Register
     *   - Bits 15:0 — Product number (fixed value identifying the device model, default 1112).
     */ 
    Iqs7211eRegProductNum         = 0x00,
    /** 0x01 MAJOR_VERSION_NUM
     *   - Revision Register
     *   - Bits 15:0 — Revision number (indicates firmware version, default 0x0001).
     */
    Iqs7211eRegMajorVersionNum   = 0x01,
    /** 0x02 MINOR_VERSION_NUM
     *   - Minor Revision Register
     *   - Bits 15:0 — Minor revision number (indicates firmware sub-version, default 0x0001).
     */
    Iqs7211eRegMinorVersionNum   = 0x02,
    /** 0x03 PATCH_NUM_0
     *   - Patch Number (Commit hash) Register
     *   - Bits 15:0 — Patch number or commit hash.
     */
    Iqs7211eRegPatchNum0         = 0x03,
    /** 0x04 PATCH_NUM_1
     *   - Patch Number (Commit hash) Register
     *   - Bits 15:0 — Patch number or commit hash.
     */
    Iqs7211eRegPatchNum1         = 0x04,
    /** 0x05 LIBRARY_NUM
     *   - Library Version Register
     *   - Bits 15:0 — Library version number. (default 206).
     */
    Iqs7211eRegLibraryNum        = 0x05,
    /** 0x06 MAJOR_VERSION_LIBRARY
     *   - Library Major Version Register
     *   - Bits 15:0 — Library type identifier. (default 4).
     */
    Iqs7211eRegMajorVersionLibrary = 0x06,
    /** 0x07 MINOR_VERSION_LIBRARY
     *   - Library minor version Register
     *   - Bits 15:0 — Reserved for future use.
     */
    Iqs7211eRegMinorVersionLibrary = 0x07,
    /** 0x08 PATCH_NUM_LIBRARY_0
     *   - Library Patch Number Register
     *   - Bits 15:0 — Reserved for future use.
     */
    Iqs7211eRegPatchNumLibrary0  = 0x08,
    /** 0x09 PATCH_NUM_LIBRARY_1
     *   - Library Patch Number Register
     *   - Bits 15:0 — Reserved for future use.
     */
    Iqs7211eRegPatchNumLibrary1  = 0x09,
    /** 0x0A RELATIVE_X
     *   - Relative X Movement Register
     *   - Bits 15:0 — Signed relative X movement since last read.
     */
    Iqs7211eRegRelativeX        = 0x0A,
    /** 0x0B RELATIVE_Y
     *   - Relative Y Movement Register
     *   - Bits 15:0 — Signed relative Y movement since last read.
     */
    Iqs7211eRegRelativeY        = 0x0B,
    /** 0x0C GESTURE_X
     *   - Gesture X Movement Register
     *   - Bits 15:0 — Cumulative signed X movement for gesture detection.
     */
    Iqs7211eRegGestureX        = 0x0C,
    /** 0x0D GESTURE_Y
     *   - Gesture Y Movement Register
     *   - Bits 15:0 — Cumulative signed Y movement for gesture detection.
     */
    Iqs7211eRegGestureY        = 0x0D,
    /** 0x0E GESTURES
     *   - Detected Gestures Register
     *   - Bits 15 — Swipe and Hold Y- - Swipe and hold in negative Y direction
     *   - Bits 14 — Swipe and Hold Y+ - Swipe and hold in positive Y direction
     *   - Bits 13 — Swipe and Hold X- - Swipe and hold in negative X direction
     *   - Bits 12 — Swipe and Hold X+ - Swipe and hold in positive X direction
     *   - Bits 11 — Swipe Y- - Swipe in negative Y direction
     *   - Bits 10 — Swipe Y+ - Swipe in positive Y direction
     *   - Bits 9 — Swipe X- - Swipe in negative X direction
     *   - Bits 8 — Swipe X+ - Swipe in positive X direction
     *   - Bits 7:5 — Unused
     *   - Bits 4 — Palm Gesture - Indicates a Palm gesture
     *   - Bits 3 —  Press-and-Hold - Indicates a press-and-hold gesture
     *   - Bits 2 — Triple Tap - Indicates a triple tap gesture
     *   - Bits 1 — Double Tap - Indicates a double tap gesture
     *   - Bits 0 — Single Tap - Indicates a single tap gesture
     */
    Iqs7211eRegGestures          = 0x0E,
    /** 0x0F INFO_FLAGS
     *   - Information Flags Register
     *   - Bits 15 — Unused
     *   - Bits 14 — ALP Output - Prox/Touch detection status of ALP channel
     *   - Bits 13 — Unused
     *   - Bits 12 — Too Many Fingers -  Indicates more than allowed fingers detected
     *   - Bits 11 — Unused
     *   - Bits 10 —  TP Movement - Trackpad finger movement detected
     *   - Bits 9:8 — No of Fingers - Number of fingers detected on trackpad
     *   - Bits 7 — Show Reset - Indicates a reset
     *   - Bits 6 — ALP Re-ATI Occurred - Alternate Low Power channel Re-ATI Status
     *   - Bits 5 — ALP ATI Error - Alternate Low Power ATI error status
     *   - Bits 4 — Re-ATI Occurred - Trackpad re-ATI status
     *   - Bits 3 — ATI Error - Error condition seen on latest trackpad ATI procedure
     *   - Bits 2:0 — Charging Mode - Indicates current mode ( 0b000: Active, 0b001: Idle-touch, 0b010: Idle, 0b011: LP1, 0b100: LP2)
     */
    Iqs7211eRegInfoFlags         = 0x0F,
    /** 0x10 FINGER_1_X
     *   - Finger 1 X Position Register
     *   - Bits 15:0 — X coordinate of first detected finger on trackpad.
     */
    Iqs7211eRegFinger1X         = 0x10,
    /** 0x11 FINGER_1_Y
     *   - Finger 1 Y Position Register
     *   - Bits 15:0 — Y coordinate of first detected finger on trackpad.
     */
    Iqs7211eRegFinger1Y         = 0x11,
    /** 0x12 FINGER_1_TOUCH_STRENGTH
     *   - Finger 1 Touch Strength Register
     *   - Bits 15:0 — Touch strength of first detected finger on trackpad.
     */
    Iqs7211eRegFinger1TouchStrength = 0x12,
    /** 0x13 FINGER_1_AREA
     *   - Finger 1 Area Register
     *   - Bits 15:0 — Area of first detected finger on trackpad.
     */
    Iqs7211eRegFinger1Area       = 0x13,
    /** 0x14 FINGER_2_X
     *   - Finger 2 X Position Register
     *   - Bits 15:0 — X coordinate of second detected finger on trackpad.
     */
    Iqs7211eRegFinger2X         = 0x14,
    /** 0x15 FINGER_2_Y
     *   - Finger 2 Y Position Register
     *   - Bits 15:0 — Y coordinate of second detected finger on trackpad.
     */
    Iqs7211eRegFinger2Y         = 0x15,
    /** 0x16 FINGER_2_TOUCH_STRENGTH
     *   - Finger 2 Touch Strength Register
     *   - Bits 15:0 — Touch strength of second detected finger on trackpad.
     */
    Iqs7211eRegFinger2TouchStrength = 0x16,
    /** 0x17 FINGER_2_AREA
     *   - Finger 2 Area Register
     *   - Bits 15:0 — Area of second detected finger on trackpad.
     */
    Iqs7211eRegFinger2Area       = 0x17,
    /** 0x18 TOUCH_STATE_0
     *   - Touch Status Register 0
     *   - Bits 15:0 — Touch status flags for channels 0 to 15. (0 = no touch, 1 = touch detected).
     */
    Iqs7211eRegTouchState0      = 0x18,
    /** 0x19 TOUCH_STATE_1
     *   - Touch Status Register 1
     *   - Bits 15:0 — Touch status flags for channels 16 to 31. (0 = no touch, 1 = touch detected).
     */
    Iqs7211eRegTouchState1      = 0x19,
    /** 0x1A TOUCH_STATE_2
     *   - Touch Status Register 2
     *   - Bits 9:0 — Touch status flags for channels 32 to 41. (0 = no touch, 1 = touch detected).
     *   - Bits 15:10 — Unused
     */
    Iqs7211eRegTouchState2      = 0x1A,
    /** 0x1B ALP_CHANNEL_COUNT
     *   - ALP Channel Count Register
     *   - Bits 15:0 — Raw count value for the Alternate Low Power (ALP) channel.
     */
    Iqs7211eRegAlpChannelCount  = 0x1B,
    /** 0x1C ALP_CHANNEL_LTA
     *   - ALP Channel Long Term Average Register
     *   - Bits 15:0 — Long term average value for the Alternate Low Power (ALP) channel.
     */
    Iqs7211eRegAlpChannelLta    = 0x1C,
    /** 0x1D ALP_CHANNEL_COUNT_A
     *   - ALP Channel Count A Register
     *   - Bits 15:0 — Raw count value for ALP channel A.
     */
    Iqs7211eRegAlpChannelCountA = 0x1D,
    /** 0x1E ALP_CHANNEL_COUNT_B
     *   - ALP Channel Count B Register
     *   - Bits 15:0 — Raw count value for ALP channel B.
     */
    Iqs7211eRegAlpChannelCountB = 0x1E,
    /** 0X1F ALP_ATI_COMP_A
     *   - ALP ATI Compensation A Register
     *   - Bits 15:0 — ATI compensation value for ALP channel A. (Rx0-3). ATI compensation is 10-bit value, thus 0 to 1023.
     */
    Iqs7211eRegAlpAtiCompA      = 0x1F,
    /** 0X20 ALP_ATI_COMP_B
     *   - ALP ATI Compensation B Register
     *   - Bits 15:0 — ATI compensation value for ALP channel B. (Rx4-7). ATI compensation is 10-bit value, thus 0 to 1023.
     */
    Iqs7211eRegAlpAtiCompB      = 0x20,
    /** 0X21 TR_GLOBAL_MIRRORS
     *   - Trackpad ATI multiplier/dividers (Global) Register
     *   - Bits 15:14 — Unused
     *   - Bits 13:9 — Fine Fractional Divider (5-bit value between 1 and 31)
     *   - Bits 8:5 — Coarse Multiplier (4 bit value between 1 and 15)
     *   - Bits 4:0 — Coarse Fractional Divider (5 bit value between 1 and 31)
     */
    Iqs7211eRegTpGlobalMirrors  = 0x21,
    /** 0X22 TP_REF_DRIFT
     *   - Trackpad Reference Drift Register
     *   - Bits 15:8 — Trackpad reference drift limit
     *   - Bits 7:0 — Trackpad ATI compensation divider (Global)
     */
    Iqs7211eRegTpRefDrift       = 0x22,
    /** 0X23 TP_TARGET
     *   - Trackpad Target Register
     *   - Bits 15:0 — Target value for trackpad ATI procedure.
     */
    Iqs7211eRegTpTarget         = 0x23,
    /** 0X24 TP_REATI_COUNTS
     *   - Trackpad Re-ATI Counts Register
     *   - Bits 15:0 — Trackpad minimum count re-ATI value
     */
    Iqs7211eRegTpReatiCounts    = 0x24,
    /** 0X25 ALP_MIRRORS
     *   - ALP ATI multiplier/dividers Register
     *   - Bits 15:14 — Unused
     *   - Bits 13:9 — Fine Fractional Divider (5-bit value between 1 and 31)
     *   - Bits 8:5 — Coarse Multiplier (4 bit value between 1 and 15)
     *   - Bits 4:0 — Coarse Fractional Divider (5 bit value between 1 and 31)
     */
    Iqs7211eRegAlpMirrors       = 0x25,
    /** 0X26 ALP_REF_DRIFT
     *   - ALP Reference Drift Register
     *   - Bits 15:8 — ALP LTA drift limit
     *   - Bits 7:0 — ALP ATI compensation divider
     */
    Iqs7211eRegAlpRefDrift      = 0x26,
    /** 0X27 ALP_TARGET
     *   - ALP Target Register
     *   - Bits 15:0 — ALP ATI target
     */
    Iqs7211eRegAlpTarget        = 0x27,
    /** 0X28 ACTIVE_MODE_RR
     *   - Active Mode Report Rate Register
     *   - Bits 15:0 — Report rate in Active mode (in ms).
     */
    Iqs7211eRegActiveModeRr     = 0x28,
    /** 0X29 IDLE_TOUCH_MODE_RR
     *   - Idle-Touch Mode Report Rate Register
     *   - Bits 15:0 — Report rate in Idle-Touch mode (in ms).
     */
    Iqs7211eRegIdleTouchModeRr  = 0x29,
    /** 0X2A IDLE_MODE_RR
     *   - Idle Mode Report Rate Register
     *   - Bits 15:0 — Report rate in Idle mode (in ms).
     */
    Iqs7211eRegIdleModeRr       = 0x2A,
    /** 0X2B LP1_MODE_RR
     *   - Low Power 1 Mode Report Rate Register
     *   - Bits 15:0 — Report rate in Low Power 1 mode (in ms).
     */
    Iqs7211eRegLp1ModeRr        = 0x2B,
    /** 0X2C LP2_MODE_RR
     *   - Low Power 2 Mode Report Rate Register
     *   - Bits 15:0 — Report rate in Low Power 2 mode (in ms).
     */
    Iqs7211eRegLp2ModeRr        = 0x2C,
    /** 0X2D ACTIVE_MODE_TIMEOUT
     *   - Active Mode Timeout Register
     *   - Bits 15:0 — Timeout duration to switch from Active to Idle-Touch mode (in s).
     */
    Iqs7211eRegActiveModeTimeout = 0x2D,
    /** 0X2E IDLE_TOUCH_MODE_TIMEOUT
     *   - Idle-Touch Mode Timeout Register
     *   - Bits 15:0 — Timeout duration to switch from Idle-Touch to Idle mode (in s).
     */
    Iqs7211eRegIdleTouchModeTimeout = 0x2E,
    /** 0X2F IDLE_MODE_TIMEOUT
     *   - Idle Mode Timeout Register
     *   - Bits 15:0 — Timeout duration to switch from Idle to Low Power 1 mode (in s).
     */
    Iqs7211eRegIdleModeTimeout   = 0x2F,
    /** 0X30 LP1_MODE_TIMEOUT
     *   - Low Power 1 Mode Timeout Register
     *   - Bits 15:0 — Timeout duration to switch from Low Power 1 to Low Power 2 mode (in s).
     */
    Iqs7211eRegLp1ModeTimeout    = 0x30,
    /** 0X31 REF_UPDATE_REATI_TIME
     *   - Reference Update Re-ATI Time Register
     *   - Bits 15:8 — Reference update time (s) 
     *   - Bits 7:0 — Re-ATI retry time (s) 
     */
    Iqs7211eRegRefUpdateReatiTime = 0x31,
    /** 0X32 I2C_TIMEOUT
     *   - I2C Timeout Register
     *   - Bits 15:0 — I2C timeout duration (ms).
     */
    Iqs7211eRegI2cTimeout        = 0x32,
    /** 0X33 SYS_CONTROL
     *   - System Control Register
     *   - Bits 15 — Tx test - Tx short test (0: normal operation, 1: enable Tx short test configuration)
     *   - Bits 14:12 — Unused
     *   - Bits 11 — Suspend - Suspend IQS7211E (0: no action, 1: place IQS7211E into suspend after the communication window terminates
     *   - Bits 10 — Unused
     *   - Bits 9 — SW Reset - Reset the device (0: no action, 1: reset device after communication window terminates (0: no action, 1: reset device after communication window terminates)
     *   - Bits 8 — Unused
     *   - Bits 7 — Ack Reset - Acknowledge a reset (0: no action, 1: acknowledge the reset by clearing Show Reset flag)
     *   - Bits 6 — ALP Re-ATI - Queue a re-ATI on ALP channel (0: no action, 1: perform re-ATI when ALP channel is sensed again)
     *   - Bits 5 — TP Re-ATI - Queue a re-ATI on trackpad channels (0: no action, 1: perform re-ATI when trackpad channels are sensed again)
     *   - Bits 4 — ALP Reseed - Reseed alternate low power channel (0: no action, 1: reseed the LTA of the alternate LP channel)
     *   - Bits 3 — TP Reseed - Reseed trackpad channels (0: no action, 1: reseed reference values of trackpad)
     *   - Bits 2:0 — Mode Select - Select mode (only applicable in Manual Mode) (0b000: Active mode, 0b001: Idle-Touch mode, 0b010: Idle mode, 0b011: LP1 mode, 0b100: LP2 mode)
     */
    Iqs7211eRegSysControl        = 0x33,
    /* 0X34 CONFIG_SETTINGS
     *   - Configuration Settings Register
     *   - Bits 15 — Unused
     *   - Bits 14 — TP Touch Event- Enable trackpad touch triggering event (0: no event, 1: event)
     *   - Bits 13 — ALP Event- Enable alternate LP channel detection triggering event (0: no event, 1: event)
     *   - Bits 12 — Unused
     *   - Bits 11 — Re-ATI Event- Enable Re-ATI generating an event (0: no event, 1: event)
     *   - Bits 10 — TP Event- Enable trackpad events (0: no event, 1: event)
     *   - Bits 9 — Gesture Event- Enable gesture events (0: no event, 1: event)
     *   - Bits 8 — Event Mode- Enable event mode communication (0: disabled, 1: enabled)
     *   - Bits 7 — Manual Control- Override automatic mode switching (0: auto, 1: manual)
     *   - Bits 6 — Comms End Cmd- Alternative method to terminate comms (0: I2C stop, 1: Write one or two bytes (any data) to the address 0xFF followed by a STOP to end comms)
     *   - Bits 5 — WDT- Watchdog timer (0: disabled, 1: enabled)
     *   - Bits 4 — Comms Request EN- Alternative polling method (while RDY not LOW) (0: forcing comms will clock stretch until a comms window, 1: A comms window must be requested with a command (no stretching))
     *   - Bits 3 —  ALP Re-ATI EN- Automatic Re-ATI on alternate LP channel (0: disabled, 1: enabled)
     *   - Bits 2 — TP Re-ATI EN- Automatic Re-ATI on trackpad (0: disabled, 1: enabled)
     *   - Bits 1:0 —  Unused
     */
    Iqs7211eRegConfigSettings     = 0x34,
    /** 0X35 OTHER_SETTINGS
     *   - Other Settings Register
     *   - Bits 15-5: Unused
     *   - Bits 4: 14MHz/18MHz- Main oscillator selection (0: Main oscillator is 14MHz, 1: Main oscillator is 18MHz)
     *   - Bits 3-0: Main Osc Adj- Small main oscillator adjustment setting (0-15: 0 = No adjustment .. 15 = Maximum adjustment)
     */
    Iqs7211eRegOtherSettings      = 0x35,
    /** 0X36 ALP_SETUP
     *   - ALP Setup Register
     *   - Bits 15:10 — Unused
     *   - Bits 9 — Count Filter- ALP count filter
     *   - Bits 8 — Sensing Method- ALP sensing method
     *   - Bits 7 — Rx7_EN- ALP Rx7 electrode (0: disabled, 1: enabled)
     *   - Bits 6 — Rx6_EN- ALP Rx6 electrode (0: disabled, 1: enabled)
     *   - Bits 5 — Rx5_EN- ALP Rx5 electrode (0: disabled, 1: enabled)
     *   - Bits 4 — Rx4_EN- ALP Rx4 electrode (0: disabled, 1: enabled)
     *   - Bits 3 — Rx3_EN- ALP Rx3 electrode (0: disabled, 1: enabled)
     *   - Bits 2 — Rx2_EN- ALP Rx2 electrode (0: disabled, 1: enabled)
     *   - Bits 1 — Rx1_EN- ALP Rx1 electrode (0: disabled, 1: enabled)
     *   - Bits 0 — Rx0_EN- ALP Rx0 electrode (0: disabled, 1: enabled)
     */
    Iqs7211eRegAlpSetup          = 0x36,
    /** 0X37 ALP_TX_ENABLE
     *   - ALP Tx Enable Register
     *   - Bits 15:13 — Unused
     *   - Bits 12 — TX12_EN - ALP Tx electrodes (0: Tx disabled , 1: Tx enabled)
     *   - Bits 11 — TX11_EN - ALP Tx electrodes (0: Tx disabled , 1: Tx enabled)
     *   - Bits 10 — TX10_EN - ALP Tx electrodes (0: Tx disabled , 1: Tx enabled)
     *   - Bits 9 — TX9_EN - ALP Tx electrodes (0: Tx disabled , 1: Tx enabled)
     *   - Bits 8 — TX8_EN - ALP Tx electrodes (0: Tx disabled , 1: Tx enabled)
     *   - Bits 7 — TX7_EN - ALP Tx electrodes (0: Tx disabled , 1: Tx enabled)
     *   - Bits 6 — TX6_EN - ALP Tx electrodes (0: Tx disabled , 1: Tx enabled)
     *   - Bits 5 — TX5_EN - ALP Tx electrodes (0: Tx disabled , 1: Tx enabled)
     *   - Bits 4 — TX4_EN - ALP Tx electrodes (0: Tx disabled , 1: Tx enabled)
     *   - Bits 3 — TX3_EN - ALP Tx electrodes (0: Tx disabled , 1: Tx enabled)
     *   - Bits 2 — TX2_EN - ALP Tx electrodes (0: Tx disabled , 1: Tx enabled)
     *   - Bits 1 — TX1_EN - ALP Tx electrodes (0: Tx disabled , 1: Tx enabled)
     *   - Bits 0 — TX0_EN - ALP Tx electrodes (0: Tx disabled , 1: Tx enabled)
     */
    Iqs7211eRegAlpTxEnable       = 0x37,
    /** 0x38 TP_TOUCH_SET_CLEAR_THR
     *   - Trackpad Touch Set/Clear Threshold Register
     *   - Bits 15:8 — Touch clear multiplier
     *   - Bits 7:0 — Touch set multiplier 
     */
    Iqs7211eRegTpTouchSetClearThr = 0x38,
    /** 0x39 ALP_THRESHOLD
     *   - ALP Threshold Register
     *   - Bits 15:0 — Alternate Low Power (ALP) channel touch threshold
     */
    Iqs7211eRegAlpThreshold      = 0x39,
    /** 0x3A ALP_SET_CLEAR_DEBOUNCE
     *   - ALP Set/Clear Debounce Register
     *   - Bits 15:8 — ALP clear debounce
     *   - Bits 7:0 — ALP set debounce
     */
    Iqs7211eRegAlpSetClearDebounce = 0x3A,
    /** 0x3B LP1_FILTERS
     *   - Low Power 1 Filters Register
     *   - Bits 15:8 — ALP LTA filter beta - LP1 mode
     *   - Bits 7:0 — ALP count filter beta - LP1 mode
     */
    Iqs7211eRegLp1Filters       = 0x3B,
    /** 0x3C LP2_FILTERS
     *   - Low Power 2 Filters Register
     *   - Bits 15:8 — ALP LTA filter beta - LP2 mode
     *   - Bits 7:0 — ALP count filter beta - LP2 mode
     */
    Iqs7211eRegLp2Filters       = 0x3C,
    /** 0x3D TP_CONV_FREQ
     *   - Trackpad Conversion Frequency Register
     *   - Bits 15-8: Frequency Fraction ( 256 * f_conv / f_clk , Range: 0 - 255)
     *   - Bits 7-0: Conversion Period ( 128 / (FrequencyFraction − 2) , if Frequency fraction is fixed at 127, the following values of the conversion period will result
            in the corresponding charge transfer frequencies: 1: 2MHz, 5: 1MHz, 12: 500kHz, 17: 350kHz, 26: 250kHz, 53: 125kHz)
     */
    Iqs7211eRegTpConvFreq       = 0x3D,
    /** 0x3E ALP_CONV_FREQ
     *   - ALP conversion frequency Register
     *   - Bits 15-8: Frequency Fraction ( 256 * f_conv / f_clk , Range: 0 - 255)
     *   - Bits 7-0: Conversion Period ( 128 / (FrequencyFraction − 2) , if Frequency fraction is fixed at 127, the following values of the conversion period will result
            in the corresponding charge transfer frequencies: 1: 2MHz, 5: 1MHz, 12: 500kHz, 17: 350kHz, 26: 250kHz, 53: 125kHz)
     */
    Iqs7211eRegAlpConvFreq      = 0x3E,
    /** 0x3F TP_HARDWARE
     *   - Trackpad hardware settings Register
     *   - Bits 15 : NM In Static- NM In Static (0: Disabled, 1: Enabled (recommended))
     *   - Bits 14 : CS 0v5 Discharge- Select internal Cs discharge voltage (0: Discharge to 0V (recommended for most cases), 1: Discharge to 0.5V)
     *   - Bits 13 : RF Filter- Internal RF filters (0: RF filters disabled, 1: RF filters enabled)
     *   - Bits 12 : CS Cap Select- Internal pool capacitor size (0: Internal capacitor is 40pF, 1: Internal capacitor is 80pF (recommended))
     *   - Bits 11-10 : Opamp Bias- Projected opamp bias (0b00: 2µA, 0b01: 5µA, 0b10: 7µA, 0b11: 10µA)
     *   - Bits 9-8 : Max Count- Count upper limit (count value stops conversion after reaching this) (0b00: 1023, 0b01: 2047, 0b10: 4095, 0b11: 16384)
     *   - Bits 7-5 : LP2 Auto Prox Cycles- Number of LP2 auto-prox cycles (0b000: 4, 0b001: 8, 0b010: 16, 0b011: 32, 0b1xx: Auto-prox disabled)
     *   - Bits 4-2 : LP1 Auto Prox Cycles- Number of LP1 auto-prox cycles (0b000: 4, 0b001: 8, 0b010: 16, 0b011: 32, 0b1xx: Auto-prox disabled)
     *   - Bits 1-0 : Init Delay- Initial cycles delay (0b00: 4, 0b01: 16, 0b10: 32, 0b11: 64)
     */
    Iqs7211eRegTpHardware       = 0x3F,
    /** 0x40 ALP_HARDWARE
     *   - ALP hardware settings Register
     *   - Bits 15 : NM In Static- NM In Static (0: Disabled, 1: Enabled (recommended))
     *   - Bits 14 : CS 0v5 Discharge- Select internal Cs discharge voltage (0: Discharge to 0V (recommended for most cases), 1: Discharge to 0.5V)
     *   - Bits 13 : RF Filter- Internal RF filters (0: RF filters disabled, 1: RF filters enabled)
     *   - Bits 12 : CS Cap Select- Internal pool capacitor size (0: Internal capacitor is 40pF, 1: Internal capacitor is 80pF (recommended))
     *   - Bits 11-10 : Opamp Bias- Projected opamp bias (0b00: 2µA, 0b01: 5µA, 0b10: 7µA, 0b11: 10µA)
     *   - Bits 9-8 : Max Count- Count upper limit (count value stops conversion after reaching this) (0b00: 1023, 0b01: 2047, 0b10: 4095, 0b11: 16384)
     *   - Bits 7-5 : LP2 Auto Prox Cycles- Number of LP2 auto-prox cycles (0b000: 4, 0b001: 8, 0b010: 16, 0b011: 32, 0b1xx: Auto-prox disabled)
     *   - Bits 4-2 : LP1 Auto Prox Cycles- Number of LP1 auto-prox cycles (0b000: 4, 0b001: 8, 0b010: 16, 0b011: 32, 0b1xx: Auto-prox disabled)
     *   - Bits 1-0 : Init Delay- Initial cycles delay (0b00: 4, 0b01: 16, 0b10: 32, 0b11: 64)
     */
    Iqs7211eRegAlpHardware      = 0x40,
    /** 0x41 TP_RX_SETTINGS
     *   - Trackpad RX Settings Register
     *   - Bits 15-8: Total Rxs- used for trackpad
     *   - Bits 7-6: Unused
     *   - Bits 5: MAV Filter- Moving averaging filter (0: disabled, 1: enabled (recommended)
     *   - Bits 4: IIR Static- IIR filtering method for the XY data points (0: dynamically adjusted (recommended), 1: fixed
     *   - Bits 3: IIR Filter- IIR filter (0: disabled, 1: enabled (recommended)
     *   - Bits 2: Switch XY Axis- Switch X and Y axes (0: Rxs are arranged in trackpad columns (X), and Txs in rows (Y), 1: Txs are arranged in trackpad columns (X), and Rxs in rows (Y))
     *   - Bits 1: Flip Y- Flip Y output values (0: Keep default Y values, 1: Invert Y output values)
     *   - Bits 0: Flip X- Flip X output values (0: Keep default X values, 1: Invert X output values)
     */
    Iqs7211eRegTpRxSettings      = 0x41,
    /** 0x42 TP_TX_SETTINGS
     *   - Trackpad TX Settings Register
     *   - Bits 15-8: Max multi-touches 
     *   - Bits 7-0: Total Txs
     */
    Iqs7211eRegTpTxSettings      = 0x42,
    /** 0x43 X_RESOLUTION
     *   - X Resolution Register
     *   - Bits 15:0 — X resolution of the trackpad
     */
    Iqs7211eRegXResolution       = 0x43,
    /** 0x44 Y_RESOLUTION
     *   - Y Resolution Register
     *   - Bits 15:0 — Y resolution of the trackpad
     */
    Iqs7211eRegYResolution       = 0x44,
    /** 0x45 XY_FILTER_BOTTOM_SPEE
     *   - XY Filter Bottom Speed Register
     *   - Bits 15:8 — Bottom speed threshold for XY filtering
     */
    Iqs7211eRegXyFilterBottomSpee = 0x45,
    /** 0x46 XY_FILTER_TOP_SPEED
     *   - XY Filter Top Speed Register
     *   - Bits 15:8 — Top speed threshold for XY filtering
     */
    Iqs7211eRegXyFilterTopSpeed = 0x46,
    /** 0x47 STATIC_FILTER
     *   - Static Filter Register
     *   - Bits 15:8 — Static filter beta value 
     *   - Bits 7:0 — Dynamic filter bottom beta
     */
    Iqs7211eRegStaticFilter     = 0x47,
    /** 0x48 FINGER_SPLIT_MOVEMENT
     *   - Finger Split Movement Register
     *   - Bits 15:8 — Finger split factor
     *   - Bits 8:0 — Stationary touch movement threshold
     */
    Iqs7211eRegFingerSplitMovement = 0x48,
    /** 0x49 TRIM_VALUES
     *   - Trim Values Register
     *   - Bits 15:8 — Y trim value
     *   - Bits 7:0 — X trim value
     */
    Iqs7211eRegTrimValues       = 0x49,
    /** 0x4A SETTINGS_VERSION
     *   - Gesture Settings Register
     *   - Bits 15:8 — Settings major version
     *   - Bits 7:0 — Settings minor version
     */
    Iqs7211eRegSettingsVersion  = 0x4A,
    /** 0x4B GESTURE_ENABLE
     *   - Gesture Enable Register
     *   - Bits 15 — Swipe and Hold Y- - Swipe and hold in negative Y direction (0: disabled, 1: enabled)
     *   - Bits 14 — Swipe and Hold Y+ - Swipe and hold in positive Y direction (0: disabled, 1: enabled)
     *   - Bits 13 — Swipe and Hold X- - Swipe and hold in negative X direction (0: disabled, 1: enabled)
     *   - Bits 12 — Swipe and Hold X+ - Swipe and hold in positive X direction (0: disabled, 1: enabled)
     *   - Bits 11 — Swipe Y- - Swipe in negative Y direction (0: disabled, 1: enabled)
     *   - Bits 10 — Swipe Y+ - Swipe in positive Y direction (0: disabled, 1: enabled)
     *   - Bits 9 — Swipe X- - Swipe in negative X direction (0: disabled, 1: enabled)
     *   - Bits 8 — Swipe X+ - Swipe in positive X direction (0: disabled, 1: enabled)
     *   - Bits 7-5 — Unused
     *   - Bits 4 — Palm Gesture - Palm gesture (0: disabled, 1: enabled)
     *   - Bits 3 — Press-and-Hold - Press-and-hold gesture (0: disabled, 1: enabled)
     *   - Bits 2 — Triple Tap - Triple tap gesture (0: disabled, 1: enabled)
     *   - Bits 1 — Double Tap - Double tap gesture (0: disabled, 1: enabled)
     *   - Bits 0 — Single Tap - Single tap gesture (0: disabled, 1: enabled)
     */
    Iqs7211eRegGestureEnable    = 0x4B,
    /** 0x4C TAP_TIME
     *   - Tap Time Register
     *   - Bits 15:0 — Tap time (ms)
     */
    Iqs7211eRegTapTime          = 0x4C,
    /** 0x4D AIR_TIME 
     *   - Air Time Register
     *   - Bits 15:0 — Air time (ms)
     */
    Iqs7211eRegAirTime          = 0x4D,
    /** 0x4E TAP_DISTANCE 
     *   - Tap Distance Register
     *   - Bits 15:0 — Tap distance (pixels)
     */
    Iqs7211eRegTapDistance      = 0x4E,
    /** 0x4F HOLD_TIME 
     *   - Hold Time Register
     *   - Bits 15:0 — Hold time (ms)
     */
    Iqs7211eRegHoldTime         = 0x4F,
    /** 0x50 SWIPE_TIME 
     *   - Swipe Time Register
     *   - Bits 15:0 — Swipe time (ms)
     */
    Iqs7211eRegSwipeTime        = 0x50,
    /** 0x51 X_INITIAL_DISTANCE
     *   - Swipe Initial Distance Register
     *   - Bits 15:0 — Swipe initial x-distance (pixels)
     */
    Iqs7211eRegXInitialDistance  = 0x51,
    /** 0x52 Y_INITIAL_DISTANCE
     *   - Swipe Initial Distance Register
     *   - Bits 15:0 — Swipe initial y-distance (pixels)
     */
    Iqs7211eRegYInitialDistance = 0x52,
    /** 0x53 X_CONSECUTIVE_DISTANCE
     *   - Swipe Consecutive Distance Register
     *   - Bits 15:0 — Swipe consecutive x-distance (pixels)
     */
    Iqs7211eRegXConsecutiveDistance = 0x53,
    /** 0x54 Y_CONSECUTIVE_DISTANCE
     *   - Swipe Consecutive Distance Register
     *   - Bits 15:0 — Swipe consecutive y-distance (pixels)
     */
    Iqs7211eRegYConsecutiveDistance = 0x54,
    /** 0x55 THRESHOLD_ANGLE
     *   - Threshold Angle Register
     *   - Bits 15:8 — Palm Threshold
     *   - Bits 7:0 — Swipe angle (64tan(deg))
     */
    Iqs7211eRegThresholdAngle   = 0x55,
    /** 0x56 RX_TX_MAPPING_0_1
     *   - RX/TX Mapping Register 0 and 1
     *   - Bits 15:8 — RxTx mapping 1 line
     *   - Bits 7:0 — RxTx mapping 0 line
     */
    Iqs7211eRegRxTxMapping0_1   = 0x56,
    /** 0x57 RX_TX_MAPPING_2_3
     *   - RX/TX Mapping Register 2 and 3
     *   - Bits 15:8 — RxTx mapping 3 line
     *   - Bits 7:0 — RxTx mapping 2 line
     */
    Iqs7211eRegRxTxMapping2_3   = 0x57,
    /** 0x58 RX_TX_MAPPING_4_5
     *   - RX/TX Mapping Register 4 and 5
     *   - Bits 15:8 — RxTx mapping 5 line
     *   - Bits 7:0 — RxTx mapping 4 line
     */
    Iqs7211eRegRxTxMapping4_5   = 0x58,
    /** 0x59 RX_TX_MAPPING_6_7
     *   - RX/TX Mapping Register 6 and 7
     *   - Bits 15:8 — RxTx mapping 7 line
     *   - Bits 7:0 — RxTx mapping 6 line
     */
    Iqs7211eRegRxTxMapping6_7   = 0x59,
    /** 0x5A RX_TX_MAPPING_8_9
     *   - RX/TX Mapping Register 8 and 9
     *   - Bits 15:8 — RxTx mapping 9 line
     *   - Bits 7:0 — RxTx mapping 8 line
     */
    Iqs7211eRegRxTxMapping8_9   = 0x5A,
    /** 0x5B RX_TX_MAPPING_10_11
     *   - RX/TX Mapping Register 10 and 11
     *   - Bits 15:8 — RxTx mapping 11 line
     *   - Bits 7:0 — RxTx mapping 10 line
     */
    Iqs7211eRegRxTxMapping10_11 = 0x5B,
    /** 0x5C RX_TX_MAPPING_12
     *   - RX/TX Mapping Register 12
     *   - Bits 15:8 — Unused
     *   - Bits 7:0 — RxTx mapping 12 line
     */
    Iqs7211eRegRxTxMapping12    = 0x5C, 
    /** 0x5D PROXA_CYCLE0
     *   - Proximity Cycle 0 Register
     *   - Bits 15:8 — ProxA channel for cycle-0 
     *   - Bits 7:0 — Fixed value 0x05
     */
    Iqs7211eRegProxaCycle0      = 0x5D,
    /** 0x5E PROXB_CYCLE0
     *   - Proximity Cycle 0 Register 
     *   - Bits 15:8 — Fixed value 0x05
     *   - Bits 7:0 — ProxB channel for cycle-0
     */
    Iqs7211eRegProxbCycle0      = 0x5E,
    /** 0x5F CYCLE1
     *   - Proximity Cycle 1 Register
     *   - Bits 15:8 — ProxB channel for cycle-1
     *   - Bits 7:0 — ProxA channel for cycle-1
     */
    Iqs7211eRegProxCycle1      = 0x5F,
    /** 0x60 PROXA_CYCLE2
     *   - Proximity Cycle 2 Register
     *   - Bits 15:8 — ProxA channel for cycle-2
     *   - Bits 7:0 — Fixed value 0x05
     */
    Iqs7211eRegProxaCycle2      = 0x60,
    /** 0x61 PROXB_CYCLE2
     *   - Proximity Cycle 2 Register 
     *   - Bits 15:8 — Fixed value 0x05
     *   - Bits 7:0 — ProxB channel for cycle-2
     */
    Iqs7211eRegProxbCycle2      = 0x61,
    /** 0x62 CYCLE3
     *   - Proximity Cycle 3 Register
     *   - Bits 15:8 — ProxB channel for cycle-3
     *   - Bits 7:0 — ProxA channel for cycle-3
     */
    Iqs7211eRegProxCycle3      = 0x62,
    /** 0x63 PROXA_CYCLE4
     *   - Proximity Cycle 4 Register
     *   - Bits 15:8 — ProxA channel for cycle-4
     *   - Bits 7:0 — Fixed value 0x05
     */
    Iqs7211eRegProxaCycle4      = 0x63,
    /** 0x64 PROXB_CYCLE4
     *   - Proximity Cycle 4 Register 
     *   - Bits 15:8 — Fixed value 0x05
     *   - Bits 7:0 — ProxB channel for cycle-4
     */
    Iqs7211eRegProxbCycle4      = 0x64,
    /** 0x65 CYCLE5
     *   - Proximity Cycle 5 Register
     *   - Bits 15:8 — ProxB channel for cycle-5
     *   - Bits 7:0 — ProxA channel for cycle-5
     */
    Iqs7211eRegProxCycle5      = 0x65,
    /** 0x66 PROXA_CYCLE6
     *   - Proximity Cycle 6 Register
     *   - Bits 15:8 — ProxA channel for cycle-6
     *   - Bits 7:0 — Fixed value 0x05
     */
    Iqs7211eRegProxaCycle6      = 0x66,
    /** 0x67 PROXB_CYCLE6
     *   - Proximity Cycle 6 Register 
     *   - Bits 15:8 — Fixed value 0x05
     *   - Bits 7:0 — ProxB channel for cycle-6
     */
    Iqs7211eRegProxbCycle6      = 0x67,
    /** 0x68 CYCLE7
     *   - Proximity Cycle 7 Register
     *   - Bits 15:8 — ProxB channel for cycle-7
     *   - Bits 7:0 — ProxA channel for cycle-7
     */
    Iqs7211eRegProxCycle7      = 0x68,
    /** 0x69 PROXA_CYCLE8
     *   - Proximity Cycle 8 Register
     *   - Bits 15:8 — ProxA channel for cycle-8
     *   - Bits 7:0 — Fixed value 0x05
     */
    Iqs7211eRegProxaCycle8      = 0x69,
    /** 0x6A PROXB_CYCLE8
     *   - Proximity Cycle 8 Register 
     *   - Bits 15:8 — Fixed value 0x05
     *   - Bits 7:0 — ProxB channel for cycle-8
     */
    Iqs7211eRegProxbCycle8      = 0x6A,
    /** 0x6B CYCLE9
     *   - Proximity Cycle 9 Register
     *   - Bits 15:8 — ProxB channel for cycle-9
     *   - Bits 7:0 — ProxA channel for cycle-9
     */
    Iqs7211eRegProxCycle9      = 0x6B,
    /** 0x6C PROXA_CYCLE10
     *   - Proximity Cycle 10 Register
     *   - Bits 15:8 — ProxA channel for cycle-10
     *   - Bits 7:0 — Fixed value 0x05
     */
    Iqs7211eRegProxaCycle10     = 0x6C,
    /** 0x6D PROXB_CYCLE10
     *   - Proximity Cycle 10 Register 
     *   - Bits 15:8 — Fixed value 0x05
     *   - Bits 7:0 — ProxB channel for cycle-10
     */
    Iqs7211eRegProxbCycle10     = 0x6D,
    /** 0x6E CYCLE11
     *   - Proximity Cycle 11 Register
     *   - Bits 15:8 — ProxB channel for cycle-11
     *   - Bits 7:0 — ProxA channel for cycle-11
     */
    Iqs7211eRegProxCycle11      = 0x6E,
    /** 0x6F PROXA_CYCLE12
     *   - Proximity Cycle 12 Register
     *   - Bits 15:8 — ProxA channel for cycle-12
     *   - Bits 7:0 — Fixed value 0x05
     */
    Iqs7211eRegProxaCycle12     = 0x6F,
    /** 0x70 PROXB_CYCLE12
     *   - Proximity Cycle 12 Register 
     *   - Bits 15:8 — Fixed value 0x05
     *   - Bits 7:0 — ProxB channel for cycle-12
     */
    Iqs7211eRegProxbCycle12     = 0x70,
    /** 0x71 CYCLE13
     *   - Proximity Cycle 13 Register
     *   - Bits 15:8 — ProxB channel for cycle-13
     *   - Bits 7:0 — ProxA channel for cycle-13
     */
    Iqs7211eRegProxCycle13      = 0x71,
    /** 0x72 PROXA_CYCLE14
     *   - Proximity Cycle 14 Register
     *   - Bits 15:8 — ProxA channel for cycle-14
     *   - Bits 7:0 — Fixed value 0x05
     */
    Iqs7211eRegProxaCycle14     = 0x72,
    /** 0x73 PROXB_CYCLE14
     *   - Proximity Cycle 14 Register 
     *   - Bits 15:8 — Fixed value 0x05
     *   - Bits 7:0 — ProxB channel for cycle-14
     */
    Iqs7211eRegProxbCycle14     = 0x73,
    /** 0x74 CYCLE15
     *   - Proximity Cycle 15 Register
     *   - Bits 15:8 — ProxB channel for cycle-15
     *   - Bits 7:0 — ProxA channel for cycle-15
     */
    Iqs7211eRegProxCycle15      = 0x74,
    /** 0x75 PROXA_CYCLE16
     *   - Proximity Cycle 16 Register
     *   - Bits 15:8 — ProxA channel for cycle-16
     *   - Bits 7:0 — Fixed value 0x05
     */
    Iqs7211eRegProxaCycle16     = 0x75,
    /** 0x76 PROXB_CYCLE16
     *   - Proximity Cycle 16 Register 
     *   - Bits 15:8 — Fixed value 0x05
     *   - Bits 7:0 — ProxB channel for cycle-16
     */
    Iqs7211eRegProxbCycle16     = 0x76,
    /** 0x77 CYCLE17
     *   - Proximity Cycle 17 Register
     *   - Bits 15:8 — ProxB channel for cycle-17
     *   - Bits 7:0 — ProxA channel for cycle-17
     */
    Iqs7211eRegProxCycle17      = 0x77,
    /** 0x78 PROXA_CYCLE18
     *   - Proximity Cycle 18 Register
     *   - Bits 15:8 — ProxA channel for cycle-18
     *   - Bits 7:0 — Fixed value 0x05
     */
    Iqs7211eRegProxaCycle18     = 0x78,
    /** 0x79 PROXB_CYCLE18
     *   - Proximity Cycle 18 Register 
     *   - Bits 15:8 — Fixed value 0x05
     *   - Bits 7:0 — ProxB channel for cycle-18
     */
    Iqs7211eRegProxbCycle18     = 0x79,
    /** 0x7A CYCLE19
     *   - Proximity Cycle 19 Register
     *   - Bits 15:8 — ProxB channel for cycle-19
     *   - Bits 7:0 — ProxA channel for cycle-19
     */
    Iqs7211eRegProxCycle19      = 0x7A,
    /** 0x7B PROXA_CYCLE20
     *   - Proximity Cycle 20 Register
     *   - Bits 15:8 — ProxA channel for cycle-20
     *   - Bits 7:0 — Fixed value 0x05
     */
    Iqs7211eRegProxaCycle20     = 0x7B,
    /** 0x7C PROXB_CYCLE20
     *   - Proximity Cycle 20 Register 
     *   - Bits 15:8 — Fixed value 0x01 (NOTE not 0x05) 
     *   - Bits 7:0 — ProxB channel for cycle-20
     */
    Iqs7211eRegProxbCycle20     = 0x7C,
} Iqs7211eReg;


typedef struct {
    uint8_t single_tap          : 1; //Indicates a single tap gesture (0: no detected, 1: detected)
    uint8_t double_tap          : 1; //Indicates a double tap gesture (0: no detected, 1: detected)
    uint8_t triple_tap          : 1; //Indicates a triple tap gesture (0: no detected, 1: detected)
    uint8_t press_and_hold      : 1; //Indicates a press-and-hold gesture (0: no detected, 1: detected)
    uint8_t palm_gesture        : 1; //Indicates a palm gesture (0: no detected, 1: detected)
    uint8_t unused              : 3; //Unused bits 
    uint8_t swipe_x_positive    : 1; //Indicates a swipe in positive X direction (0: no detected, 1: detected)
    uint8_t swipe_x_negative    : 1; //Indicates a swipe in negative X direction (0: no detected, 1: detected)
    uint8_t swipe_y_positive    : 1; //Indicates a swipe in positive Y direction (0: no detected, 1: detected)
    uint8_t swipe_y_negative    : 1; //Indicates a swipe in negative Y direction (0: no detected, 1: detected)
    uint8_t hold_x_positive     : 1; //Indicates a swipe and hold in positive X direction (0: no detected, 1: detected)
    uint8_t hold_x_negative     : 1; //Indicates a swipe and hold in negative X direction (0: no detected, 1: detected)
    uint8_t hold_y_positive     : 1; //Indicates a swipe and hold in positive Y direction (0: no detected, 1: detected)
    uint8_t hold_y_negative     : 1; //Indicates a swipe and hold in negative Y direction (0: no detected, 1: detected)
} Iqs7211eGestures;

typedef struct {
    uint8_t charging_mode       : 3; //Indicates current mode ( 0b000: Active, 0b001: Idle-touch, 0b010: Idle, 0b011: LP1, 0b100: LP2)
    uint8_t ati_error           : 1; //Error condition seen on latest trackpad ATI procedure (0: no error, 1: error detected)
    uint8_t re_ati_occurred     : 1; //Trackpad re-ATI status (0: no re-ATI occurred, 1: re-ATI occurred)
    uint8_t alp_ati_error       : 1; //Alternate Low Power ATI error status (0: no error, 1: error detected)
    uint8_t alp_re_ati_occurred : 1; //Alternate Low Power re-ATI status (0: no re-ATI occurred, 1: re-ATI occurred)
    uint8_t show_reset          : 1; //Indicates a software reset has occurred (0: no reset, 1: reset occurred)
    uint8_t no_of_fingers       : 2; //Number of fingers detected on trackpad 
    uint8_t tp_movement         : 1; //Trackpad finger movement detected (0: no movement, 1: movement detected)
    uint8_t reserved_1          : 1;
    uint8_t too_many_fingers    : 1; //Indicates more than allowed fingers detected (0: within limit, 1: too many fingers)
    uint8_t reserved_2          : 1;
    uint8_t alp_output          : 1; //Prox/Touch detection status of ALP channel (0: no detection, 1: detection)
    uint8_t reserved_3          : 1;
} Iqs7211eInfoFlags;

typedef struct {
    uint8_t channel_0           : 1; //Touch status flag for channel 0 (0 = no touch, 1 = touch detected)
    uint8_t channel_1           : 1; //Touch status flag for channel 1 (0 = no touch, 1 = touch detected)
    uint8_t channel_2           : 1; //Touch status flag for channel 2 (0 = no touch, 1 = touch detected)
    uint8_t channel_3           : 1; //Touch status flag for channel 3 (0 = no touch, 1 = touch detected)
    uint8_t channel_4           : 1; //Touch status flag for channel 4 (0 = no touch, 1 = touch detected)
    uint8_t channel_5           : 1; //Touch status flag for channel 5 (0 = no touch, 1 = touch detected)
    uint8_t channel_6           : 1; //Touch status flag for channel 6 (0 = no touch, 1 = touch detected)
    uint8_t channel_7           : 1; //Touch status flag for channel 7 (0 = no touch, 1 = touch detected)
    uint8_t channel_8           : 1; //Touch status flag for channel 8 (0 = no touch, 1 = touch detected)
    uint8_t channel_9           : 1; //Touch status flag for channel 9 (0 = no touch, 1 = touch detected)
    uint8_t channel_10          : 1; //Touch status flag for channel 10 (0 = no touch, 1 = touch detected)
    uint8_t channel_11          : 1; //Touch status flag for channel 11 (0 = no touch, 1 = touch detected)
    uint8_t channel_12          : 1; //Touch status flag for channel 12 (0 = no touch, 1 = touch detected)
    uint8_t channel_13          : 1; //Touch status flag for channel 13 (0 = no touch, 1 = touch detected)
    uint8_t channel_14          : 1; //Touch status flag for channel 14 (0 = no touch, 1 = touch detected)
    uint8_t channel_15          : 1; //Touch status flag for channel 15 (0 = no touch, 1 = touch detected)    
} Iqs7211eTouchState0;

typedef struct {
    uint8_t channel_16          : 1; //Touch status flag for channel 16 (0 = no touch, 1 = touch detected)
    uint8_t channel_17          : 1; //Touch status flag for channel 17 (0 = no touch, 1 = touch detected)
    uint8_t channel_18          : 1; //Touch status flag for channel 18 (0 = no touch, 1 = touch detected)
    uint8_t channel_19          : 1; //Touch status flag for channel 19 (0 = no touch, 1 = touch detected)
    uint8_t channel_20          : 1; //Touch status flag for channel 20 (0 = no touch, 1 = touch detected)
    uint8_t channel_21          : 1; //Touch status flag for channel 21 (0 = no touch, 1 = touch detected)
    uint8_t channel_22          : 1; //Touch status flag for channel 22 (0 = no touch, 1 = touch detected)
    uint8_t channel_23          : 1; //Touch status flag for channel 23 (0 = no touch, 1 = touch detected)
    uint8_t channel_24          : 1; //Touch status flag for channel 24 (0 = no touch, 1 = touch detected)
    uint8_t channel_25          : 1; //Touch status flag for channel 25 (0 = no touch, 1 = touch detected)
    uint8_t channel_26          : 1; //Touch status flag for channel 26 (0 = no touch, 1 = touch detected)
    uint8_t channel_27          : 1; //Touch status flag for channel 27 (0 = no touch, 1 = touch detected)
    uint8_t channel_28          : 1; //Touch status flag for channel 28 (0 = no touch, 1 = touch detected)
    uint8_t channel_29          : 1; //Touch status flag for channel 29 (0 = no touch, 1 = touch detected)
    uint8_t channel_30          : 1; //Touch status flag for channel 30 (0 = no touch, 1 = touch detected)
    uint8_t channel_31          : 1; //Touch status flag for channel 31 (0 = no touch, 1 = touch detected)
} Iqs7211eTouchState1;

typedef struct {
    uint8_t channel_32          : 1; //Touch status flag for channel 32 (0 = no touch, 1 = touch detected)
    uint8_t channel_33          : 1; //Touch status flag for channel 33 (0 = no touch, 1 = touch detected)
    uint8_t channel_34          : 1; //Touch status flag for channel 34 (0 = no touch, 1 = touch detected)
    uint8_t channel_35          : 1; //Touch status flag for channel 35 (0 = no touch, 1 = touch detected)
    uint8_t channel_36          : 1; //Touch status flag for channel 36 (0 = no touch, 1 = touch detected)
    uint8_t channel_37          : 1; //Touch status flag for channel 37 (0 = no touch, 1 = touch detected)
    uint8_t channel_38          : 1; //Touch status flag for channel 38 (0 = no touch, 1 = touch detected)
    uint8_t channel_39          : 1; //Touch status flag for channel 39 (0 = no touch, 1 = touch detected)
    uint8_t channel_40          : 1; //Touch status flag for channel 40 (0 = no touch, 1 = touch detected)
    uint8_t channel_41          : 1; //Touch status flag for channel 41 (0 = no touch, 1 = touch detected)
    uint8_t reserved_1          : 6; //Unused bits
} Iqs7211eTouchState2;

typedef struct {
    uint8_t coarse_fractional_divider : 5; //Coarse Fractional Divider (5 bit value between 1 and 31)
    uint8_t coarse_multiplier         : 4; //Coarse Multiplier (4 bit value between 1 and 15)
    uint8_t fine_fractional_divider   : 5; //Fine Fractional Divider (5-bit value between 1 and 31)
    uint8_t reserved_1                : 2; //Unused bits
} Iqs7211eTpGlobalMirrors;

typedef struct {
    uint8_t ati_compensation_divider  : 8; //Trackpad ATI compensation divider (Global)
    uint8_t reference_drift_limit     : 8; //Trackpad reference drift limit
} Iqs7211eTpRefDrift;

typedef struct {
    uint8_t coarse_fractional_divider : 5; //Coarse Fractional Divider (5 bit value between 1 and 31)
    uint8_t coarse_multiplier         : 4; //Coarse Multiplier (4 bit value between 1 and 15)
    uint8_t fine_fractional_divider   : 5; //Fine Fractional Divider (5-bit value between 1 and 31)
    uint8_t reserved_1                : 2; //Unused bits
} Iqs7211eAlpMirrors;

typedef struct {
    uint8_t ati_compensation_divider  : 8; //ALP ATI compensation divider
    uint8_t lta_drift_limit           : 8; //ALP LTA drift limit
} Iqs7211eAlpRefDrift;

typedef struct {
    uint8_t re_ati_retry_time        : 8; //Re-ATI retry time (s)
    uint8_t reference_update_time    : 8; //Reference update time (s)
} Iqs7211eRefUpdateReatiTime;

typedef struct {
    uint8_t mode_select           : 3; //Mode Select - Select mode (only applicable in Manual Mode) (0b000: Active mode, 0b001: Idle-Touch mode, 0b010: Idle mode, 0b011: LP1 mode, 0b100: LP2 mode)
    uint8_t tp_reseed             : 1; //TP Reseed - Reseed trackpad channels (0: no action, 1: reseed reference values of trackpad)
    uint8_t alp_reseed            : 1; //ALP Reseed - Reseed alternate low power channel (0: no action, 1: reseed the LTA of the alternate LP channel)
    uint8_t tp_re_ati             : 1; //TP Re-ATI - Queue a re-ATI on trackpad channels (0: no action, 1: perform re-ATI when trackpad channels are sensed again)
    uint8_t alp_re_ati            : 1; //ALP Re-ATI - Queue a re-ATI on ALP channel (0: no action, 1: perform re-ATI when ALP channel is sensed again)
    uint8_t ack_reset             : 1; //Ack Reset - Acknowledge a reset (0: no action, 1: acknowledge the reset by clearing Show Reset flag)
    uint8_t reserved_1            : 1; //Unused bit
    uint8_t sw_reset              : 1; //SW Reset - Reset the device (0: no action, 1: reset device after communication window terminates)
    uint8_t reserved_2            : 1; //Unused bit
    uint8_t suspend               : 1; //Suspend - Suspend IQS7211E (0: no action, 1: place IQS7211E into suspend after the communication window terminates)
    uint8_t reserved_3            : 3; //Unused bits
    uint8_t tx_test               : 1; //Tx test - Tx short test (0: normal operation, 1: enable Tx short test configuration)
} Iqs7211eSysControl;

typedef struct {
    uint8_t reserved_1            : 2; //Unused bit
    uint8_t tp_re_ati_en          : 1; //TP Re-ATI EN- Automatic Re-ATI on trackpad (0: disabled, 1: enabled)
    uint8_t alp_re_ati_en         : 1; //ALP Re-ATI EN- Automatic Re-ATI on alternate LP channel (0: disabled, 1: enabled)
    uint8_t comms_request_en      : 1; //Comms Request EN- Alternative polling method (while RDY not LOW) (0: forcing comms will clock stretch until a comms window, 1: A comms window must be requested with a command (no stretching))
    uint8_t wdt                   : 1; //WDT- Watchdog timer (0: disabled, 1: enabled)
    uint8_t comms_end_cmd         : 1; //Comms End Cmd- Alternative method to terminate comms (0: I2C stop, 1: Write one or two bytes (any data) to the address 0xFF followed by a STOP to end comms)
    uint8_t manual_control        : 1; //Manual Control- Override automatic mode switching (0: auto, 1: manual)
    uint8_t event_mode            : 1; //Event Mode- Enable event mode communication (0: disabled, 1: enabled)
    uint8_t gesture_event         : 1; //Gesture Event- Enable gesture events (0: no event, 1: event)
    uint8_t tp_event              : 1; //TP Event- Enable trackpad events (0: no event, 1: event)
    uint8_t re_ati_event          : 1; //Re-ATI Event- Enable Re-ATI generating an event (0: no event, 1: event)
    uint8_t reserved_2            : 1; //Unused bit 
    uint8_t alp_event             : 1; //ALP Event- Enable alternate LP channel detection triggering event (0: no event, 1: event)
    uint8_t tp_touch_event        : 1; //TP Touch Event- Enable trackpad touch triggering event (0: no event, 1: event) 
    uint8_t reserved_3            : 1; //Unused bit
} Iqs7211eConfigSettings;

typedef struct {
    uint8_t main_osc_adj          : 4; //Main Osc Adj- Small main oscillator adjustment setting (0-15: 0 = No adjustment .. 15 = Maximum adjustment)
    uint8_t fourteen_mhz_18mhz    : 1; //14MHz/18MHz- Main oscillator selection (0: Main oscillator is 14MHz, 1: Main oscillator is 18MHz)
    uint16_t reserved_1           : 11; //Unused bits
} Iqs7211eOtherSettings;

typedef struct {
    uint8_t rx0_en                : 1; //Rx0_EN- ALP Rx0 electrode (0: disabled, 1: enabled)
    uint8_t rx1_en                : 1; //Rx1_EN- ALP Rx1 electrode (0: disabled, 1: enabled)
    uint8_t rx2_en                : 1; //Rx2_EN- ALP Rx2 electrode (0: disabled, 1: enabled)
    uint8_t rx3_en                : 1; //Rx3_EN- ALP Rx3 electrode (0: disabled, 1: enabled)
    uint8_t rx4_en                : 1; //Rx4_EN- ALP Rx4 electrode (0: disabled, 1: enabled)
    uint8_t rx5_en                : 1; //Rx5_EN- ALP Rx5 electrode (0: disabled, 1: enabled)
    uint8_t rx6_en                : 1; //Rx6_EN- ALP Rx6 electrode (0: disabled, 1: enabled)
    uint8_t rx7_en                : 1; //Rx7_EN- ALP Rx7 electrode (0: disabled, 1: enabled)
    uint8_t sensing_method        : 1; //Sensing Method- ALP sensing method
    uint8_t count_filter          : 1; //Count Filter- ALP count filter
    uint16_t reserved_1           : 6; //Unused bits
} Iqs7211eAlpSetup;    

typedef struct {
    uint8_t tx0_en                : 1; //TX0_EN - ALP Tx electrodes (0: Tx disabled , 1: Tx enabled)
    uint8_t tx1_en                : 1; //TX1_EN - ALP Tx electrodes (0: Tx disabled , 1: Tx enabled)
    uint8_t tx2_en                : 1; //TX2_EN - ALP Tx electrodes (0: Tx disabled , 1: Tx enabled)
    uint8_t tx3_en                : 1; //TX3_EN - ALP Tx electrodes (0: Tx disabled , 1: Tx enabled)
    uint8_t tx4_en                : 1; //TX4_EN - ALP Tx electrodes (0: Tx disabled , 1: Tx enabled)
    uint8_t tx5_en                : 1; //TX5_EN - ALP Tx electrodes (0: Tx disabled , 1: Tx enabled)
    uint8_t tx6_en                : 1; //TX6_EN - ALP Tx electrodes (0: Tx disabled , 1: Tx enabled)
    uint8_t tx7_en                : 1; //TX7_EN - ALP Tx electrodes (0: Tx disabled , 1: Tx enabled)
    uint8_t tx8_en                : 1; //TX8_EN - ALP Tx electrodes (0: Tx disabled , 1: Tx enabled)
    uint8_t tx9_en                : 1; //TX9_EN - ALP Tx electrodes (0: Tx disabled , 1: Tx enabled)
    uint8_t tx10_en               : 1; //TX10_EN - ALP Tx electrodes (0: Tx disabled , 1: Tx enabled)
    uint8_t tx11_en               : 1; //TX11_EN - ALP Tx electrodes (0: Tx disabled , 1: Tx enabled)
    uint8_t tx12_en               : 1; //TX12_EN - ALP Tx electrodes (0: Tx disabled , 1: Tx enabled)
    uint16_t reserved_1           : 3; //Unused bits
} Iqs7211eAlpTxEnable;

typedef struct {
    uint8_t touch_set_multiplier   : 8; //Touch Set Multiplier (multiplier for touch set threshold)
    uint8_t touch_clear_multiplier : 8; //Touch Clear Multiplier (multiplier for touch clear threshold)
} Iqs7211eTpTouchSetClearThr;

typedef struct {
    uint8_t alp_set_debounce       : 8; //ALP Set Debounce
    uint8_t alp_clear_debounce     : 8; //ALP Clear Debounce
} Iqs7211eAlpSetClearDebounce;

typedef struct {
    uint8_t alp_count_filter_beta  : 8; //ALP Count Filter Beta - LP1 Mode
    uint8_t alp_lta_filter_beta    : 8; //ALP LTA Filter Beta - LP1 Mode
} Iqs7211eLp1Filters;

typedef struct {
    uint8_t alp_count_filter_beta  : 8; //ALP Count Filter Beta - LP2 Mode
    uint8_t alp_lta_filter_beta    : 8; //ALP LTA Filter Beta - LP2 Mode
} Iqs7211eLp2Filters;

typedef struct {
    uint8_t tp_conversion_period   : 8; //Trackpad Conversion Period ( 128 / (FrequencyFraction − 2) , if Frequency fraction is fixed at 127, the following values of the conversion period will result
                                        //in the corresponding charge transfer frequencies: 1: 2MHz, 5: 1MHz, 12: 500kHz, 17: 350kHz, 26: 250kHz, 53: 125kHz)
    uint8_t tp_frequency_fraction  : 8; //Trackpad Frequency Fraction ( 256 * f_conv / f_clk , Range: 0 - 255)
} Iqs7211eTpConvFreq;

typedef struct {
    uint8_t alp_conversion_period  : 8; //ALP Conversion Period ( 128 / (FrequencyFraction − 2) , if Frequency fraction is fixed at 127, the following values of the conversion period will result
                                       //in the corresponding charge transfer frequencies: 1: 2MHz, 5: 1MHz, 12: 500kHz, 17: 350kHz, 26: 250kHz, 53: 125kHz)
    uint8_t alp_frequency_fraction : 8; //ALP Frequency Fraction ( 256 * f_conv / f_clk , Range: 0 - 255)
} Iqs7211eAlpConvFreq;

typedef struct {
    uint8_t init_delay              : 2; //Initial cycles delay (0b00: 4, 0b01: 16, 0b10: 32, 0b11: 64)
    uint8_t lp1_auto_prox_cycles    : 3; //Number of LP1 auto-prox cycles (0b000: 4, 0b001: 8, 0b010: 16, 0b011: 32, 0b1xx: Auto-prox disabled)
    uint8_t lp2_auto_prox_cycles    : 3; //Number of LP2 auto-prox cycles (0b000: 4, 0b001: 8, 0b010: 16, 0b011: 32, 0b1xx: Auto-prox disabled)
    uint8_t max_count               : 2; //Count upper limit (count value stops conversion after reaching this) (0b00: 1023, 0b01: 2047, 0b10: 4095, 0b11: 16384)
    uint8_t opamp_bias              : 2; //Projected opamp bias (0b00: 2µA, 0b01: 5µA, 0b10: 7µA, 0b11: 10µA)
    uint8_t cs_cap_select           : 1; //Internal pool capacitor size (0: Internal capacitor is 40pF, 1: Internal capacitor is 80pF (recommended))
    uint8_t rf_filter               : 1; //Internal RF filters (0: RF filters disabled, 1: RF filters enabled)
    uint8_t cs_0v5_discharge        : 1; //Select internal Cs discharge voltage (0: Discharge to 0V (recommended for most cases), 1: Discharge to 0.5V)
    uint8_t nm_in_static            : 1; //NM In Static (0: Disabled, 1: Enabled (recommended))
} Iqs7211eTpHardware;

typedef struct {
    uint8_t init_delay              : 2; //Initial cycles delay (0b00: 4, 0b01: 16, 0b10: 32, 0b11: 64)
    uint8_t lp1_auto_prox_cycles    : 3; //Number of LP1 auto-prox cycles (0b000: 4, 0b001: 8, 0b010: 16, 0b011: 32, 0b1xx: Auto-prox disabled)
    uint8_t lp2_auto_prox_cycles    : 3; //Number of LP2 auto-prox cycles (0b000: 4, 0b001: 8, 0b010: 16, 0b011: 32, 0b1xx: Auto-prox disabled)
    uint8_t max_count               : 2; //Count upper limit (count value stops conversion after reaching this) (0b00: 1023, 0b01: 2047, 0b10: 4095, 0b11: 16384)
    uint8_t opamp_bias              : 2; //Projected opamp bias (0b00: 2µA, 0b01: 5µA, 0b10: 7µA, 0b11: 10µA)
    uint8_t cs_cap_select           : 1; //Internal pool capacitor size (0: Internal capacitor is 40pF, 1: Internal capacitor is 80pF (recommended))
    uint8_t rf_filter               : 1; //Internal RF filters (0: RF filters disabled, 1: RF filters enabled)
    uint8_t cs_0v5_discharge        : 1; //Select internal Cs discharge voltage (0: Discharge to 0V (recommended for most cases), 1: Discharge to 0.5V)
    uint8_t nm_in_static            : 1; //NM In Static (0: Disabled, 1: Enabled (recommended))
} Iqs7211eAlpHardware;

typedef struct {
    uint8_t flip_x                  : 1; //Invert X output values (0: Keep default X values, 1: Invert X output values)
    uint8_t flip_y                  : 1; //Invert Y output values (0: Keep default Y values, 1: Invert Y output values)
    uint8_t switch_xy_axis          : 1; //Switch X and Y axes (0: Rxs are arranged in trackpad columns (X), and Txs in rows (Y), 1: Txs are arranged in trackpad columns (X), and Rxs in rows (Y))
    uint8_t iir_filter              : 1; //IIR filter (0: disabled, 1: enabled (recommended)
    uint8_t iir_static              : 1; //IIR filtering method for the XY data points (0: dynamically adjusted (recommended), 1: fixed)
    uint8_t mav_filter              : 1; //Moving averaging filter (0: disabled, 1: enabled (recommended)
    uint8_t reserved_1              : 2; //Unused bits
    uint8_t total_rxs               : 8; //Total Rxs- used for trackpad
} Iqs7211eTpRxSettings;

typedef struct {
    uint8_t total_txs               : 8; //Total Txs- used for trackpad
    uint8_t max_multi_touches       : 8; //Max Multi-Touches
} Iqs7211eTpTxSettings;

typedef struct {
    uint8_t dynamic_filter_bottom   : 8; //Dynamic filter bottom beta
    uint8_t static_filter_beta      : 8; //Static filter beta value
} Iqs7211eStaticFilter;

typedef struct {
    uint8_t stationary_touch_movement_threshold : 8; //Stationary touch movement threshold
    uint8_t finger_split_factor     : 8; //Finger split factor
} Iqs7211eFingerSplitMovement;

typedef struct {
    uint8_t x_trim_value            : 8; //X trim value
    uint8_t y_trim_value            : 8; //Y trim value
} Iqs7211eTrimValues;

typedef struct {
    uint8_t settings_minor_version  : 8; //Settings minor version
    uint8_t settings_major_version  : 8; //Settings major version
} Iqs7211eSettingsVersion;

typedef struct {
    uint8_t single_tap              : 1; //Single Tap - Single tap gesture (0: disabled, 1: enabled)
    uint8_t double_tap              : 1; //Double Tap - Double tap gesture (0: disabled, 1: enabled)
    uint8_t triple_tap              : 1; //Triple Tap - Triple tap gesture (0: disabled, 1: enabled)
    uint8_t press_and_hold          : 1; //Press-and-Hold - Press-and-hold gesture (0: disabled, 1: enabled)
    uint8_t palm_gesture            : 1; //Palm Gesture - Palm gesture (0: disabled, 1: enabled)
    uint8_t reserved_1              : 3; //Unused bits  
    uint8_t swipe_x_positive        : 1; //Swipe X+ - Swipe in positive X direction (0: disabled, 1: enabled)
    uint8_t swipe_x_negative        : 1; //Swipe X- - Swipe in negative X direction (0: disabled, 1: enabled)
    uint8_t swipe_y_positive        : 1; //Swipe Y+ - Swipe in positive Y direction (0: disabled, 1: enabled)
    uint8_t swipe_y_negative        : 1; //Swipe Y- - Swipe in negative Y direction (0: disabled, 1: enabled)
    uint8_t hold_x_positive         : 1; //Swipe and Hold X+ - Swipe and hold in positive X direction (0: disabled, 1: enabled)
    uint8_t hold_x_negative         : 1; //Swipe and Hold X- - Swipe and hold in negative X direction (0: disabled, 1: enabled)
    uint8_t hold_y_positive         : 1; //Swipe and Hold Y+ - Swipe and hold in positive Y direction (0: disabled, 1: enabled)
    uint8_t hold_y_negative         : 1; //Swipe and Hold Y- - Swipe and hold in negative Y direction (0: disabled, 1: enabled)
} Iqs7211eGestureEnable;

typedef struct {
    uint8_t swipe_angle             : 8; //Swipe Angle (64tan(deg))
    uint8_t palm_threshold          : 8; //Palm Threshold
} Iqs7211eThresholdAngle;

typedef struct {
    uint8_t rx_tx_mapping_0         : 8; //RxTx Mapping 0 line
    uint8_t rx_tx_mapping_1         : 8; //RxTx Mapping 1 line
} Iqs7211eRxTxMapping0_1;

typedef struct {
    uint8_t rx_tx_mapping_2         : 8; //RxTx Mapping 2 line
    uint8_t rx_tx_mapping_3         : 8; //RxTx Mapping 3 line
} Iqs7211eRxTxMapping2_3;

typedef struct {
    uint8_t rx_tx_mapping_4         : 8; //RxTx Mapping 4 line
    uint8_t rx_tx_mapping_5         : 8; //RxTx Mapping 5 line
} Iqs7211eRxTxMapping4_5;

typedef struct {
    uint8_t rx_tx_mapping_6         : 8; //RxTx Mapping 6 line
    uint8_t rx_tx_mapping_7         : 8; //RxTx Mapping 7 line
} Iqs7211eRxTxMapping6_7;

typedef struct {
    uint8_t rx_tx_mapping_8         : 8; //RxTx Mapping 8 line
    uint8_t rx_tx_mapping_9         : 8; //RxTx Mapping 9 line
} Iqs7211eRxTxMapping8_9;

typedef struct {
    uint8_t rx_tx_mapping_10        : 8; //RxTx Mapping 10 line
    uint8_t rx_tx_mapping_11        : 8; //RxTx Mapping 11 line
} Iqs7211eRxTxMapping10_11;

typedef struct {
    uint8_t rx_tx_mapping_12        : 8; //RxTx Mapping 12 line
    uint8_t reserved_1              : 8; //Unused bits
}Iqs7211eRxTxMapping12;

typedef struct {
    uint8_t fixed_0_5              : 8; //Fixed value 0x05
    uint8_t prox_a_channel         : 8; //ProxA channel for cycle-0
} Iqs7211eProxaCycle0;

typedef struct {
    uint8_t prox_b_channel         : 8; //ProxB channel for cycle-0
    uint8_t fixed_0_5              : 8; //Fixed value 0x05
} Iqs7211eProxbCycle0;

typedef struct {
    uint8_t prox_a_channel         : 8; //ProxA channel for cycle-1
    uint8_t prox_b_channel         : 8; //ProxB channel for cycle-1
} Iqs7211eProxCycle1;

typedef struct {
    uint8_t fixed_0_5              : 8; //Fixed value 0x05
    uint8_t prox_a_channel         : 8; //ProxA channel for cycle-2
} Iqs7211eProxaCycle2;

typedef struct {
    uint8_t prox_b_channel         : 8; //ProxB channel for cycle-2
    uint8_t fixed_0_5              : 8; //Fixed value 0x05
} Iqs7211eProxbCycle2;

typedef struct {
    uint8_t prox_a_channel         : 8; //ProxA channel for cycle-3
    uint8_t prox_b_channel         : 8; //ProxB channel for cycle-3
} Iqs7211eProxCycle3;

typedef struct {
    uint8_t fixed_0_5              : 8; //Fixed value 0x05
    uint8_t prox_a_channel         : 8; //ProxA channel for cycle-4
} Iqs7211eProxaCycle4;

typedef struct {
    uint8_t prox_b_channel         : 8; //ProxB channel for cycle-4
    uint8_t fixed_0_5              : 8; //Fixed value 0x05
} Iqs7211eProxbCycle4;

typedef struct {
    uint8_t prox_a_channel         : 8; //ProxA channel for cycle-5
    uint8_t prox_b_channel         : 8; //ProxB channel for cycle-5
} Iqs7211eProxCycle5;

typedef struct {
    uint8_t fixed_0_5              : 8; //Fixed value 0x05
    uint8_t prox_a_channel         : 8; //ProxA channel for cycle-6
} Iqs7211eProxaCycle6;

typedef struct {
    uint8_t prox_b_channel         : 8; //ProxB channel for cycle-6
    uint8_t fixed_0_5              : 8; //Fixed value 0x05
} Iqs7211eProxbCycle6;

typedef struct {
    uint8_t prox_a_channel         : 8; //ProxA channel for cycle-7
    uint8_t prox_b_channel         : 8; //ProxB channel for cycle-7
} Iqs7211eProxCycle7;

typedef struct {
    uint8_t fixed_0_5              : 8; //Fixed value 0x05
    uint8_t prox_a_channel         : 8; //ProxA channel for cycle-8
} Iqs7211eProxaCycle8;

typedef struct {
    uint8_t prox_b_channel         : 8; //ProxB channel for cycle-8
    uint8_t fixed_0_5              : 8; //Fixed value 0x05
} Iqs7211eProxbCycle8;

typedef struct {
    uint8_t prox_a_channel         : 8; //ProxA channel for cycle-9
    uint8_t prox_b_channel         : 8; //ProxB channel for cycle-9
} Iqs7211eProxCycle9;

typedef struct {
    uint8_t fixed_0_5              : 8; //Fixed value 0x05
    uint8_t prox_a_channel         : 8; //ProxA channel for cycle-10
} Iqs7211eProxaCycle10;

typedef struct {
    uint8_t prox_b_channel         : 8; //ProxB channel for cycle-10
    uint8_t fixed_0_5              : 8; //Fixed value 0x05
} Iqs7211eProxbCycle10;

typedef struct {
    uint8_t prox_a_channel         : 8; //ProxA channel for cycle-11
    uint8_t prox_b_channel         : 8; //ProxB channel for cycle-11
} Iqs7211eProxCycle11;

typedef struct {
    uint8_t fixed_0_5              : 8; //Fixed value 0x05
    uint8_t prox_a_channel         : 8; //ProxA channel for cycle-12
} Iqs7211eProxaCycle12;

typedef struct {
    uint8_t prox_b_channel         : 8; //ProxB channel for cycle-12
    uint8_t fixed_0_5              : 8; //Fixed value 0x05
} Iqs7211eProxbCycle12;

typedef struct {
    uint8_t prox_a_channel         : 8; //ProxA channel for cycle-13
    uint8_t prox_b_channel         : 8; //ProxB channel for cycle-13
} Iqs7211eProxCycle13;

typedef struct {
    uint8_t fixed_0_5              : 8; //Fixed value 0x05
    uint8_t prox_a_channel         : 8; //ProxA channel for cycle-14
} Iqs7211eProxaCycle14;

typedef struct {
    uint8_t prox_b_channel         : 8; //ProxB channel for cycle-14
    uint8_t fixed_0_5              : 8; //Fixed value 0x05
} Iqs7211eProxbCycle14;

typedef struct {
    uint8_t prox_a_channel         : 8; //ProxA channel for cycle-15
    uint8_t prox_b_channel         : 8; //ProxB channel for cycle-15
} Iqs7211eProxCycle15;

typedef struct {
    uint8_t fixed_0_5              : 8; //Fixed value 0x05
    uint8_t prox_a_channel         : 8; //ProxA channel for cycle-16
} Iqs7211eProxaCycle16;

typedef struct {
    uint8_t prox_b_channel         : 8; //ProxB channel for cycle-16
    uint8_t fixed_0_5              : 8; //Fixed value 0x05
} Iqs7211eProxbCycle16;

typedef struct {
    uint8_t prox_a_channel         : 8; //ProxA channel for cycle-17
    uint8_t prox_b_channel         : 8; //ProxB channel for cycle-17
} Iqs7211eProxCycle17;

typedef struct {
    uint8_t fixed_0_5              : 8; //Fixed value 0x05
    uint8_t prox_a_channel         : 8; //ProxA channel for cycle-18
} Iqs7211eProxaCycle18;

typedef struct {
    uint8_t prox_b_channel         : 8; //ProxB channel for cycle-18
    uint8_t fixed_0_5              : 8; //Fixed value 0x05
} Iqs7211eProxbCycle18;

typedef struct {
    uint8_t prox_a_channel         : 8; //ProxA channel for cycle-19
    uint8_t prox_b_channel         : 8; //ProxB channel for cycle-19
} Iqs7211eProxCycle19;

typedef struct {
    uint8_t fixed_0_5              : 8; //Fixed value 0x05
    uint8_t prox_a_channel         : 8; //ProxA channel for cycle-20
} Iqs7211eProxaCycle20;

typedef struct {
    uint8_t prox_b_channel         : 8; //ProxB channel for cycle-20
    uint8_t fixed_0_1              : 8; //Fixed value 0x01 (NOTE not 0x05) 
} Iqs7211eProxbCycle20;

/* clang-format on */
