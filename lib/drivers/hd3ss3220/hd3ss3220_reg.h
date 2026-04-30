#pragma once

// https://www.ti.com/lit/ds/symlink/hd3ss3220.pdf?ts=1777528126963&ref_url=https%253A%252F%252Fwww.ti.com%252Fproduct%252FHD3SS3220

#include <core/common_defines.h>

/* clang–format off */

typedef enum {
    Hd3ss3220RegDeviceId        = 0x00,     /**< Figure 6–4. Device Identification Register */
    Hd3ss3220RegConnectStatus   = 0x08,     /**< Figure 6–5. Connection Status Register */
    Hd3ss3220RegControl         = 0x09,     /**< Figure 6–6. Connection Status and Control Register */
    Hd3ss3220RegGeneralControl  = 0x0A,     /**< Figure 6–7. General Control Register */
    Hd3ss3220RegDeviceRevision  = 0xA0,     /**< Figure 6–8. Device Revision Register */
} Hd3ss3220Reg;

typedef struct {
    uint8_t id[8];                          //Bits 7:0: DEVICE_ID. Read–only. 0x00 after reset.
                                            //Device ID.For the HD3SS3220 device these fields return a string of ASCII characters 
                                            //returning HD3SS3220 addresses: 0x07 – 0x00 = {0x00, 0x54, 0x55, 0x53, 0x42, 0x33, 0x32, 0x32}
} Hd3ss3220RegDeviceIdRegBits;
_Static_assert(
    sizeof(Hd3ss3220RegDeviceIdRegBits) == 8,
    "Size check for 'Hd3ss3220RegDeviceIdRegBits' failed.");

typedef struct {
    uint8_t active_cable_detection  : 1;    // Bit 0: ACTIVE_CABLE_DETECTION. Read/Update. 0x00 after reset. 
                                            // This flag indicates that an active cable has been plugged into the Type–C connector
                                            // 0b0 – No active cable
                                            // 0b1 – Active Cable Attach
    uint8_t accessory_connected     : 3;    // Bits 3:1: ACCESSORY_CONNECTED. Read/Update. 0x00 after reset. These bits are read by the application to determine if an accessory was attached.
                                            // 0b000 –No Accessory attached (Default)
                                            // 0b001 – Reserved
                                            // 0b010 – Reserved
                                            // 0b011 – Reserved
                                            // 0b100 – Audio Accessory
                                            // 0b101 – Charged Thru Audio Accessory
                                            // 0b110 – Debug Accessory when HD3SS3220 is connected as a DFP
                                            // 0b111 – Debug accessory when HD3SS3220 is connected as a UFP
    uint8_t current_mode_detect     : 2;    // Bits 5:4: CURRENT_MODE_DETECT. Read/Update. 0x00 after reset. 
                                            // These bits are set when a UFP determines the Type–C current mode.
                                            // 0b00 – Default (value at start up)
                                            // 0b01 – Medium
                                            // 0b10 – Charge Through Accessory – 500mA
                                            // 0b11 – High
    uint8_t current_mode_advertise  : 2;    // Bits 7:6: CURRENT_MODE_ADVERTISE. Read/Write. 0x00 after reset. 
                                            // These bits are programmed by the application to raise the current advertisement from Default.
                                            // 0b00 – Default (500mA/900mA) Initial value at startup
                                            // 0b01 – Mid (1.5A)
                                            // 0b10 – High (3A)
                                            // 0b11 – Reserved                                       
} Hd3ss3220RegConnectStatusRegBits;
_Static_assert(
    sizeof(Hd3ss3220RegConnectStatusRegBits) == 1,
    "Size check for 'Hd3ss3220RegConnectStatusRegBits' failed.");

typedef struct {
    uint8_t disable_ufp_accessory   : 1;    // Bit 0: DISABLE_UFP_ACCESSORY. Read/Write. 0x00 after reset.
                                            // Setting this field will disable UFP accessory support
                                            // 0b0 – UFP accessory support enabled (Default)
                                            // 0b1 – UFP accessory support disabled
    uint8_t drp_duty_cycle          : 2;    // Bits 2:1: DRP_DUTY_CYCLE. Read/Write. 0x00 after reset.
                                            // Percentage of time that a DRP shall advertise DFP during tDRP
                                            // 0b00 – 30% default
                                            // 0b01 – 40%
                                            // 0b10 – 50%
                                            // 0b11 – 60%
    uint8_t vconn_fault             : 1;    // Bit 3: VCONN_FAULT. Read/Update. 0x00 after reset.
                                            // Bit is set whenever VCONN overcurrent limit is triggered.
                                            // 0b0 – Clear
                                            // 0b1 – VCONN fault is detected
    uint8_t interrupt_status        : 1;    // Bit 4: INTERRUPT_STATUS. Read/Update. 0x00 after reset.
                                            // The INT pin will be pulled low whenever a CSR changes. When a CSR change has occurred this bit should be held at 1 until the application clears the bit.
                                            // 0b0 – Clear
                                            // 0b1 – Interrupt (When INT pulled low, this bit must be 1. This bit will be 1 whenever any CSR have been changed)
    uint8_t cable_dir              : 1;     // Bit 5: CABLE_DIR. Read/Update. 0x00 after reset.
                                            // Cable orientation. The application can read these bits for cable orientation information.
                                            // 0b0 – CC2
                                            // 0b1 – CC1 (Default)
    uint8_t attached_state         : 2;     // Bits 7:6: ATTACHED_STATE. Read/Update. 0x00 after reset.
                                            // This is an additional method to communicate attach other than the ID pin. These bits can
                                            // be read by the application to determine what was attached.
                                            // 0b00 – Not Attached (Default)
                                            // 0b01 – Attached.SRC (DFP)
                                            // 0b10 – Attached.SNK (UFP)
                                            // 0b11 – Attached to an Accessory
} Hd3ss3220RegControlRegBits;
_Static_assert(
    sizeof(Hd3ss3220RegControlRegBits) == 1,
    "Size check for 'Hd3ss3220RegControlRegBits' failed.");

typedef struct {
    uint8_t disable_term            : 1;    // Bit 0: DISABLE_TERM. Read/Write. 0x00 after reset. 
                                            // This field disables the termination on CC pins and transition the CC state machine to the disabled state.
                                            // 0b0 – Termination enabled according HD3SS3220 mode of operation (default)
                                            // 0b1 – Termination disabled and state machine held in disable state
    uint8_t source_pref             : 2;    // Bits 2:1: SOURCE_PREF. Read/Write. 0x00 after reset. 
                                            // This field controls the HD3SS3220 behavior when configured as a DRP.
                                            // 0b00 – Standard DRP (default)
                                            // 0b01 – DRP performs Try.SNK
                                            // 0b10 – Reserved
                                            // 0b11 – DRP performs Try.SRC
    uint8_t i2c_soft_reset          : 1;    // Bit 3: I2C_SOFT_RESET. Read/Update. 0x00 after reset. 
                                            // This register resets the digital logic. The bit is self–clearing. A write of 1 starts the reset. 
                                            // The following registers can be affected after setting this bit: CURRENT_MODE_DETECT, ACTIVE_CABLE_DETECTION, ACCESSORY_CONNECTED, ATTACHED_STATE, CABLE_DIR
                                            // 0b0 – Normal operation (default)
                                            // 0b1 – Reset digital logic
    uint8_t mode_select             : 2;    // Bits 5:4: MODE_SELECT. Read/Write. 0x00 after reset. 
                                            // This register can be written to set the HD3SS3220 mode operation. The ADDR pin must be set to I2C mode. 
                                            // If the default is maintained, HD3SS3220 shall operate according to the PORT pin levels and modes. The MODE_SELECT can only be changed when in the unattached state.
                                            // 0b00 – DRP mode (start from unattached.SNK) (default)
                                            // 0b01 – UFP mode (unattached.SNK)
                                            // 0b10 – DFP mode (unattached.SRC)
                                            // 0b11 – DRP mode (start from unattached.SNK)
    uint8_t debounce                : 2;    // Bits 7:6: DEBOUNCE. Read/Write. 0x00 after reset. 
                                            // The nominal amount of time the HD3SS3220 debounces the voltages on the CC pins.
                                            // 0b00 – 168ms (Default)
                                            // 0b01 – 118ms
                                            // 0b10 – 134ms
                                            // 0b11 – 152ms
} Hd3ss3220RegGeneralControlRegBits;
_Static_assert(
    sizeof(Hd3ss3220RegGeneralControlRegBits) == 1,
    "Size check for 'Hd3ss3220RegGeneralControlRegBits' failed.");

typedef struct {
    uint8_t revision;                      //Bits 7:0: REVISION. Read–only. 0x02 after reset. 
                                           // Revision of HD3SS3220. Defaults to 0x02
} Hd3ss3220RegDeviceRevisionRegBits;
_Static_assert(
    sizeof(Hd3ss3220RegDeviceRevisionRegBits) == 1,
    "Size check for 'Hd3ss3220RegDeviceRevisionRegBits' failed.");
    
/* clang–format on */
