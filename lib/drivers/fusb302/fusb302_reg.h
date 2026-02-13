#pragma once

#include <stdint.h>
//https://www.onsemi.com/download/data-sheet/pdf/fusb302b-d.pdf

/* clang-format off */

typedef enum {
    Fusb302RegDeviceId = 0x01,          /** (Address: 01h; Reset Value: 0x9X; Type: Read) */
    Fusb302RegSwitches0 = 0x02,         /** (Address: 02h; Reset Value: 0x03; Type: Read/Write) */
    Fusb302RegSwitches1 = 0x03,         /** (Address: 03h; Reset Value: 0x20; Type: Read/Write) */
    Fusb302RegMeasure = 0x04,           /** (Address: 04h; Reset Value: 0x31; Type: Read/Write) */
    Fusb302RegSlice = 0x05,             /** (Address: 05h; Reset Value: 0x60; Type: Read/Write) */
    Fusb302RegControl0 = 0x06,          /** (Address: 06h; Reset Value: 0x24; Type: Read/Write/Clear) */
    Fusb302RegControl1 = 0x07,          /** (Address: 07h; Reset Value: 0x00; Type: Read/Write/Clear) */
    Fusb302RegControl2 = 0x08,          /** (Address: 08h; Reset Value: 0x02; Type: Read/Write) */
    Fusb302RegControl3 = 0x09,          /** (Address: 09h; Reset Value: 0x06; Type: Read/Write) */
    Fusb302RegMask = 0x0A,              /** (Address: 0Ah; Reset Value: 0x00; Type: Read/Write) */
    Fusb302RegPower = 0x0B,             /** (Address: 0Bh; Reset Value: 0x01; Type: Read/Write) */
    Fusb302RegReset = 0x0C,             /** (Address: 0Ch; Reset Value: 0x00; Type: Write/Clear) */
    Fusb302RegOCPreg = 0x0D,            /** (Address: 0Dh; Reset Value: 0x0F; Type: Read/Write) */
    Fusb302RegMaskA = 0x0E,             /** (Address: 0Eh; Reset Value: 0x00; Type: Read/Write) */
    Fusb302RegMaskB = 0x0F,             /** (Address: 0Fh; Reset Value: 0x00; Type: Read/Write) */
    Fusb302RegControl4 = 0x10,          /** (Address: 10h; Reset Value: 0x00; Type: Read/Write) */
    Fusb302RegStatus0A = 0x3C,          /** (Address: 3Ch; Reset Value: 0x00; Type: Read) */
    Fusb302RegStatus1A = 0x3D,          /** (Address: 3Dh; Reset Value: 0x00; Type: Read) */
    Fusb302RegInterruptA = 0x3E,        /** (Address: 3Eh; Reset Value: 0x00; Type: Read/Clear) */
    Fusb302RegInterruptB = 0x3F,        /** (Address: 3Fh; Reset Value: 0x00; Type: Read/Clear) */
    Fusb302RegStatus0 = 0x40,           /** (Address: 40h; Reset Value: 0x00; Type: Read) */
    Fusb302RegStatus1 = 0x41,           /** (Address: 41h; Reset Value: 0x28; Type: Read) */
    Fusb302RegInterrupt = 0x42,         /** (Address: 42h; Reset Value: 0x00; Type: Read/Clear) */
    Fusb302RegFifos = 0x43,             /** (Address: 43h; Reset Value: 0x00; Type: Read/Write) */
} Fusb302Reg;

typedef struct {
    uint8_t revision_id     : 2;        // Revision History of each version [Version ID]_revA: 00(e.g. revA); [Version ID]_revB: 01 (e.g. revB); [Version ID]_revC: 10 (e.g. revC); [Version ID]_revC: 11 (e.g. revD)
    uint8_t product_id      : 2;        // “01”, “10” and “11” applies to MLP only: 00: FUSB302BMPX/FUSB302BVMPX(Default) & FUSB302BUCX; 01: FUSB302B01MPX; 10: FUSB302B10MPX; 11: FUSB302B11MPX
    uint8_t version_id      : 4;        // Device version ID by Trim or etc. A_[Revision ID]: 1000 (e.g. A_revA); B_[Revision ID]: 1001; C_[Revision ID]: 1010 etc
} Fusb302DeviceIdRegBits;
_Static_assert(
    sizeof(Fusb302DeviceIdRegBits) == 1,
    "Size check for 'Fusb302DeviceIdRegBits' failed.");

typedef struct {
    uint8_t pdwn1          : 1;        // 1: Device pull down on CC1. 0: no pull down
    uint8_t pdwn2          : 1;        // 1: Device pull down on CC2. 0: no pull down
    uint8_t meas_cc1       : 1;        // 1: Use the measure block to monitor or measure the voltage on CC1
    uint8_t meas_cc2       : 1;        // 1: Use the measure block to monitor or measure the voltage on CC2
    uint8_t vconn_cc1      : 1;        // 1: Turn on the VCONN current to CC1 pin
    uint8_t vconn_cc2      : 1;        // 1: Turn on the VCONN current to CC2 pin
    uint8_t pu_en1         : 1;        // 1: Apply host pull up current to CC1 pin
    uint8_t pu_en2         : 1;        // 1: Apply host pull up current to CC2 pin
} Fusb302Switches0RegBits;
_Static_assert(
    sizeof(Fusb302Switches0RegBits) == 1,
    "Size check for 'Fusb302Switches0RegBits' failed.");

typedef struct {
    uint8_t tx_cc1         : 1;        // 1: Enable BMC transmit driver on CC1 pin
    uint8_t tx_cc2         : 1;        // 1: Enable BMC transmit driver on CC2 pin
    uint8_t auto_crc       : 1;        // 1: Starts the transmitter automatically when a message with a good CRC is received and automatically sends a GoodCRC acknowledge packet back to the relevant SOP*; 0: Feature disabled
    uint8_t reserved       : 1;        // Do Not Use
    uint8_t data_role      : 1;        // Bit used for constructing the GoodCRC acknowledge packet. This bit corresponds to the Port Data Role bit in the message header. For SOP: 1: SRC; 0: SNK
    uint8_t spec_rev       : 2;        // Bit used for constructing the GoodCRC acknowledge packet. These bits correspond to the Specification Revision bits in the message header: 00: Revision 1.0; 01: Revision 2.0; 10: Do Not Use; 11: Do Not Use
    uint8_t power_role     : 1;        // Bit used for constructing the GoodCRC acknowledge packet. This bit corresponds to the Port Power Role bit in the message header if an SOP packet is received: 1: Source if SOP; 0: Sink if SOP  
} Fusb302Switches1RegBits;
_Static_assert(
    sizeof(Fusb302Switches1RegBits) == 1,
    "Size check for 'Fusb302Switches1RegBits' failed.");

typedef struct {
    uint8_t mdac           : 6;        // Measure Block DAC data input. LSB is equivalent to 42 mV of voltage which is compared to the measured CC voltage. The measured CC is selected by MEAS_CC2, or MEAS_CC1 bits.
    uint8_t meas_vbus      : 1;        // 0: MDAC/comparator measurement is controlled by MEAS_CC* bits; 1: Measure VBUS with the MDAC/comparator. This requires MEAS_CC* bits to be 0
    uint8_t reserved       : 1;        // Do Not Use
} Fusb302MeasureRegBits;
_Static_assert(
    sizeof(Fusb302MeasureRegBits) == 1,
    "Size check for 'Fusb302MeasureRegBits' failed.");

typedef struct {
    uint8_t sdac           : 6;        // BMC Slicer DAC data input. Allows for a programmable threshold so as to meet the BMC receive mask under all noise conditions.
    uint8_t sdac_hys       : 2;        // Adds hysteresis where there are now two thresholds, the lower threshold which is always the value programmed by SDAC[5:0] and the higher threshold that is: 11: 255 mV hysteresis: higher threshold = (SDAC value + 20hex); 10: 170 mV hysteresis: higher threshold = (SDAC value + Ahex); 01: 85 mV hysteresis: higher threshold = (SDAC value + 5); 00: No hysteresis: higher threshold = SDAC value
} Fusb302SliceRegBits;
_Static_assert(
    sizeof(Fusb302SliceRegBits) == 1,
    "Size check for 'Fusb302SliceRegBits' failed.");

typedef struct {
    uint8_t tx_start       : 1;        // 1: Start transmitter using the data in the transmit FIFO. Preamble is started first. During the preamble period the transmit data can start to be written to the transmit FIFO. Self clearing.
    uint8_t auto_pre       : 1;        // 1: Starts the transmitter automatically when a message with a good CRC is received. This allows the software to take as much as 300 S to respond after the I_CRC_CHK interrupt is received. Before starting the transmitter, an internal timer waits for approximately 170 S before executing the transmit start and preamble
    uint8_t host_cur       : 2;        // 1: Controls the host pull up current enabled by PU_EN[2:1]: 00: No current; 01: 80 A – Default USB power; 10: 180 A – Medium Current Mode: 1.5 A; 11: 330 A – High Current Mode: 3 A
    uint8_t reserved1      : 1;        // Do Not Use
    uint8_t int_mask       : 1;        // 1: Mask all interrupts; 0: Interrupts to host are enabled
    uint8_t tx_flush       : 1;        // 1: Self clearing bit to flush the content of the transmit FIFO
    uint8_t reserved2      : 1;        // Do Not Use
} Fusb302Control0RegBits;
_Static_assert(
    sizeof(Fusb302Control0RegBits) == 1,
    "Size check for 'Fusb302Control0RegBits' failed.");

typedef struct {
    uint8_t ensop1         : 1;        // 1: Enable SOP‘(SOP prime) packets; 0: Ignore SOP‘(SOP prime) packets
    uint8_t ensop2         : 1;        // 1: Enable SOP”(SOP double prime) packets; 0: Ignore SOP”(SOP double prime) packets
    uint8_t rx_flush       : 1;        // 1: Self clearing bit to flush the content of the receive FIFO
    uint8_t reserved1      : 1;        // Do Not Use
    uint8_t bist_mode2     : 1;        // 1: Sent BIST Mode 01s pattern for testing
    uint8_t ensop1db       : 1;        // 1: Enable SOP‘_DEBUG (SOP prime debug) packets; 0: Ignore SOP‘_DEBUG (SOP prime debug) packets
    uint8_t ensop2db       : 1;        // 1: Enable SOP”_DEBUG (SOP double prime debug) packets; 0: Ignore SOP”_DEBUG (SOP double prime debug) packets
    uint8_t reserved2      : 1;        // Do Not Use
} Fusb302Control1RegBits;
_Static_assert(
    sizeof(Fusb302Control1RegBits) == 1,
    "Size check for 'Fusb302Control1RegBits' failed.");

typedef struct {
    uint8_t toggle        : 1;        // 1: Enable DRP, SNK or SRC Toggle autonomous functionality; 0: Disable DRP, SNK and SRC Toggle functionality
    uint8_t mode          : 2;        // 11: Enable SRC polling functionality if TOGGLE=1; 10: Enable SNK polling functionality if TOGGLE=1; 01: Enable DRP polling functionality if TOGGLE=1; 00: Do Not Use
    uint8_t wake_en       : 1;        // 1: Enable Wake Detection functionality if the power state is correct; 0: Disable Wake Detection functionality
    uint8_t reserved1     : 1;        // Do Not Use
    uint8_t tog_rd_only   : 1;        // 1: When TOGGLE=1 only Rd values will cause the TOGGLE state machine to stop toggling and trigger the I_TOGGLE interrupt; 0: When TOGGLE=1, Rd and Ra values will cause the TOGGLE state machine to stop toggling
    uint8_t tog_save_pwr  : 2;        // 00: Don’t go into the DISABLE state after one cycle of toggle; 01: Wait between toggle cycles for tDIS time of 40 ms; 10: Wait between toggle cycles for tDIS time of 80 ms; 11: Wait between toggle cycles for tDIS time of 160 ms
} Fusb302Control2RegBits;
_Static_assert(
    sizeof(Fusb302Control2RegBits) == 1,
    "Size check for 'Fusb302Control2RegBits' failed.");

typedef struct {
    uint8_t auto_retry      : 1;        // 1: Enable automatic packet retries if GoodCRC is not received; 0: Disable automatic packet retries if GoodCRC not received
    uint8_t n_retries       : 2;        // 11: Three retries of packet (four total packets sent); 10: Two retries of packet (three total packets sent); 01: One retry of packet (two total packets sent); 00: No retries (similar to disabling auto retry)
    uint8_t auto_softreset  : 1;        // 1: Enable automatic soft reset packet if retries fail; 0: Disable automatic soft reset packet if retries fail
    uint8_t auto_hardreset  : 1;        // 1: Enable automatic hard reset packet if soft reset fail; 0: Disable automatic hard reset packet if soft reset fail
    uint8_t bist_tmode      : 1;        // 1: BIST mode. Receive FIFO is cleared immediately after sending GoodCRC response; 0: Normal operation, All packets are treated as usual
    uint8_t send_hard_reset : 1;        // 1: Send a hard reset packet (highest priority); 0: Don’t send a soft reset packet
    uint8_t reserved        : 1;        // Do Not Use
} Fusb302Control3RegBits;
_Static_assert(
    sizeof(Fusb302Control3RegBits) == 1,
    "Size check for 'Fusb302Control3RegBits' failed.");

typedef struct {
    uint8_t m_bc_lvl       : 1;        // 1: Mask a change in host requested current level; 0: Do not mask
    uint8_t m_collision    : 1;        // 1: Mask the I_COLLISION interrupt bit; 0: Do not mask
    uint8_t m_wake         : 1;        // 1: Mask the I_WAKE interrupt bit; 0: Do not mask
    uint8_t m_alert        : 1;        // 1: Mask the I_ALERT interrupt bit; 0: Do not mask
    uint8_t m_crc_chk      : 1;        // 1: Mask interrupt from CRC_CHK bit; 0: Do not mask
    uint8_t m_comp_chng    : 1;        // 1: Mask I_COMP_CHNG interrupt for change is the value of COMP, the measure comparator; 0: Do not mask
    uint8_t m_activity     : 1;        // 1: Mask interrupt for a transition in CC bus activity; 0: Do not mask
    uint8_t m_vbusok       : 1;        // 1: Mask I_VBUSOK interrupt bit; 0: Do not mask
} Fusb302MaskRegBits; 
_Static_assert(
    sizeof(Fusb302MaskRegBits) == 1,
    "Size check for 'Fusb302MaskRegBits' failed.");

typedef struct {
    uint8_t pwr            : 4;        // Power enables: PWR[0]: Bandgap and wake circuit; PWR[1]: Receiver powered and current references for Measure block; PWR[2]: Measure block powered; PWR[3]: Enable internal oscillator
    uint8_t reserved       : 4;        // Do Not Use
} Fusb302PowerRegBits;
_Static_assert(
    sizeof(Fusb302PowerRegBits) == 1,
    "Size check for 'Fusb302PowerRegBits' failed.");

typedef struct {
    uint8_t sw_reset        : 1;        // 1: Reset the FUSB302B including the I2C registers to their default values
    uint8_t pd_reset        : 1;        // 1: Reset just the PD logic for both the PD transmitter and receiver
    uint8_t reserved        : 6;        // Do Not Use
} Fusb302ResetRegBits;
_Static_assert(
    sizeof(Fusb302ResetRegBits) == 1,
    "Size check for 'Fusb302ResetRegBits' failed.");

typedef struct {
    uint8_t ocp_cur        : 3;        // OCP_CUR2, OCP_CUR1, OCP_CUR0: 111: max_range (see bit definition above for OCP_RANGE); 110: 7 × max_range / 8; 101: 6 × max_range / 8; 100: 5 × max_range / 8; 011: 4 × max_range / 8; 010: 3 × max_range / 8; 001: 2 × max_range / 8; 000: max_range / 8
    uint8_t ocp_range      : 1;        // OCP_RANGE: 1: OCP range between 100−800 mA (max_range = 800 mA); 0: OCP range between 10−80 mA (max_range = 80 mA)
    uint8_t reserved       : 4;        // Do Not Use
} Fusb302OcpRegBits;
_Static_assert(
    sizeof(Fusb302OcpRegBits) == 1,
    "Size check for 'Fusb302OcpRegBits' failed.");

typedef struct {
    uint8_t m_hardrst      : 1;        // 1: Mask the I_HARDRST interrupt; 0: Do not mask
    uint8_t m_softrst      : 1;        // 1: Mask the I_SOFTRST interrupt; 0: Do not mask
    uint8_t m_txsent       : 1;        // 1: Mask the I_TXSENT interrupt; 0: Do not mask
    uint8_t m_hardsent     : 1;        // 1: Mask the I_HARDSENT interrupt; 0: Do not mask
    uint8_t m_retryfail    : 1;        // 1: Mask the I_RETRYFAIL interrupt; 0: Do not mask
    uint8_t m_softfail     : 1;        // 1: Mask the I_SOFTFAIL interrupt; 0: Do not mask
    uint8_t m_togdone      : 1;        // 1: Mask the I_TOGDONE interrupt; 0: Do not mask
    uint8_t m_ocp_temp     : 1;        // 1: Mask the I_OCP_TEMP interrupt; 0: Do not mask
} Fusb302MaskARegBits;
_Static_assert(
    sizeof(Fusb302MaskARegBits) == 1,
    "Size check for 'Fusb302MaskARegBits' failed.");

typedef struct {
    uint8_t m_gcrcsent     : 1;        // 1: Mask the I_GCRCSENT interrupt; 0: Do not mask
    uint8_t reserved       : 7;        // Do Not Use
} Fusb302MaskBRegBits;
_Static_assert(
    sizeof(Fusb302MaskBRegBits) == 1,
    "Size check for 'Fusb302MaskBRegBits' failed.");

typedef struct {
    uint8_t tog_exit_aud   : 1;        // 1: In auto Rd only Toggle mode, stop Toggle at Audio accessory (Ra on both CC)
    uint8_t reserved       : 7;        // Do Not Use
} Fusb302Control4RegBits;
_Static_assert(
    sizeof(Fusb302Control4RegBits) == 1,
    "Size check for 'Fusb302Control4RegBits' failed.");

typedef struct {
    uint8_t hard_rst       : 1;        // 1: Hard Reset PD ordered set has been received
    uint8_t soft_rst       : 1;        // 1: One of the packets received was a soft reset packet
    uint8_t power_state    : 2;        // Internal power state when logic internals needs to control the power state. POWER3 corresponds to PWR3 bit and POWER2 corresponds to PWR2 bit. The power state is the higher of both PWR[3:0] and {POWER3, POWER2, PWR[1:0]} so that if one is 03 and the other is F then the internal power state is F
    uint8_t retry_fail     : 1;        // 1: All packet retries have failed to get a GoodCRC acknowledge. This status is cleared when a START_TX, TXON or SEND_HARD_RESET is executed
    uint8_t soft_fail      : 1;        // 1: All soft reset packets with retries have failed to get a GoodCRC acknowledge. This status is cleared when a START_TX, TXON or SEND_HARD_RESET is executed
    uint8_t reserved       : 2;        // Do Not Use
} Fusb302Status0ARegBits;
_Static_assert(
    sizeof(Fusb302Status0ARegBits) == 1,
    "Size check for 'Fusb302Status0ARegBits' failed.");

typedef struct {
    uint8_t rx_sop          : 1;        // 1: Indicates the last packet placed in the RxFIFO is type SOP
    uint8_t rx_sop1db       : 1;        // 1: Indicates the last packet placed in the RxFIFO is type SOP’_DEBUG (SOP prime debug)
    uint8_t rx_sop2db       : 1;        // 1: Indicates the last packet placed in the RxFIFO is type SOP”_DEBUG (SOP double prime debug)
    uint8_t togss           : 3;        // Toggle state machine status bits. Togss[2:0] bits are valid when TOGGLE=1 and indicate the current state of the toggle state machine. 000: Toggle logic running (processor has previously written TOGGLE=1) 001: Toggle functionality has settled to SRCon CC1 (STOP_SRC1 state) 010: Toggle functionality has settled to SRCon CC2 (STOP_SRC2 state) 101: Toggle functionality has settled to SNKon CC1 (STOP_SNK1 state) 110: Toggle functionality has settled to SNKon CC2 (STOP_SNK2 state) 111: Toggle functionality has detected AudioAccessory with vRa on both CC1 and CC2 (settles to STOP_SRC1 state) Otherwise: Not defined (do not interpret)
    uint8_t reserved        : 2;        // Do Not Use
} Fusb302Status1ARegBits;
_Static_assert(
    sizeof(Fusb302Status1ARegBits) == 1,
    "Size check for 'Fusb302Status1ARegBits' failed.");

typedef struct {
    uint8_t i_hard_rst     : 1;        // 1: Received a hard reset ordered set
    uint8_t i_soft_rst     : 1;        // 1: Received a soft reset packet
    uint8_t i_tx_sent      : 1;        // 1: Interrupt to alert that we sent a packet that was acknowledged with a GoodCRC response packet
    uint8_t i_hard_sent    : 1;        // 1: Interrupt from successfully sending a hard reset ordered set
    uint8_t i_retry_fail   : 1;        // 1: Interrupt from automatic packet retries have failed
    uint8_t i_soft_fail    : 1;        // 1: Interrupt from automatic soft reset packets with retries have failed
    uint8_t i_tog_done     : 1;        // 1: Interrupt indicating the TOGGLE functionality was terminated because a device was detected
    uint8_t i_ocp_temp     : 1;        // 1: Interrupt from either a OCP event on one of the VCONN switches or an over-temperature event
} Fusb302InterruptARegBits;
_Static_assert(
    sizeof(Fusb302InterruptARegBits) == 1,
    "Size check for 'Fusb302InterruptARegBits' failed.");

typedef struct {
    uint8_t i_gcrc_sent    : 1;        // 1: Sent a GoodCRC acknowledge packet in response to an incoming packet that has the correct CRC value
    uint8_t reserved       : 7;        // Do Not Use
} Fusb302InterruptBRegBits;
_Static_assert(
    sizeof(Fusb302InterruptBRegBits) == 1,
    "Size check for 'Fusb302InterruptBRegBits' failed.");

typedef struct {
    uint8_t bc_lvl        : 2;        // Current voltage status of the measured CC pin interpreted as host current levels as follows: 00: < 200 mV; 01: > 200 mV, < 660 mV; 10: > 660 mV, < 1.23 V; 11: > 1.23 V. Note the software must measure these at an appropriate time, while there is no signaling activity on the selected CC line. BC_LVL is only defined when Measure block is on which is when register bits PWR[2]=1 and either MEAS_CC1=1 or MEAS_CC2=1
    uint8_t wake          : 1;        // 1: Voltage on CC indicated a device attempting to attach; 0: WAKE either not enabled (WAKE_EN=0) or no device attached
    uint8_t alert         : 1;        // 1: Alert software an error condition has occurred. An alert is caused by: TX_FULL: the transmit FIFO is full; RX_FULL: the receive FIFO is full. See Status1 bits
    uint8_t crc_chk       : 1;        // 1: Indicates the last received packet had the correct CRC. This bit remains set until the SOP of the next packet; 0: Packet received for an enabled SOP* and CRC for the enabled packet received was incorrect
    uint8_t comp          : 1;        // 1: Measured CC* input is higher than reference level driven from the MDAC; 0: Measured CC* input is lower than reference level driven from the MDAC
    uint8_t activity      : 1;        // 1: Transitions are detected on the active CC* line. This bit goes high after a minimum of 3 CC transitions, and goes low with no Transitions; 0: Inactive
    uint8_t vbusok        : 1;        // 1: Interrupt occurs when VBUS transitions through vVBUSthr. This bit typically is used to recognize port partner during startup
} Fusb302Status0RegBits;
_Static_assert(
    sizeof(Fusb302Status0RegBits) == 1,
    "Size check for 'Fusb302Status0RegBits' failed.");

typedef struct {
    uint8_t ocp            : 1;        // 1: Indicates an over-current or short condition has occurred on the VCONN switch; 0: No over-current or short condition has been detected on the VCONN switch
    uint8_t overtemp       : 1;        // 1: Temperature of the device is too high; 0: Temperature of the device is within the normal operating range
    uint8_t tx_full        : 1;        // 1: The transmit FIFO is full; 0: The transmit FIFO is not full
    uint8_t tx_empty       : 1;        // 1: The transmit FIFO is empty; 0: The transmit FIFO is not empty
    uint8_t rx_full        : 1;        // 1: The receive FIFO is full; 0: The receive FIFO is not full
    uint8_t rx_empty       : 1;        // 1: The receive FIFO is empty; 0: The receive FIFO is not empty
    uint8_t rx_sop1        : 1;        // 1: Indicates the last packet placed in the RxFIFO is type SOP’ (SOP prime); 0: The last packet placed in the RxFIFO is not type SOP’ (SOP prime)
    uint8_t rx_sop2        : 1;        // 1: Indicates the last packet placed in the RxFIFO is type SOP” (SOP double prime); 0: The last packet placed in the RxFIFO is not type SOP” (SOP double prime)
} Fusb302Status1RegBits;
_Static_assert(
    sizeof(Fusb302Status1RegBits) == 1,
    "Size check for 'Fusb302Status1RegBits' failed.");

typedef struct {
    uint8_t i_bc_lvl       : 1;        // 1: A change in host requested current level has occurred; 0: No change in host requested current level has occurred
    uint8_t i_collision    : 1;        // 1: When a transmit was attempted, activity was detected on the active CC line. Transmit is not done. The packet is received normally; 0: No collision has been detected during a transmit attempt
    uint8_t i_wake         : 1;        // 1: Voltage on CC indicated a device attempting to attach; 0: No device attach has been detected on CC or WAKE is not enabled (WAKE_EN=0)
    uint8_t i_alert        : 1;        // 1: Alert software an error condition has occurred. An alert is caused by: TX_FULL: the transmit FIFO is full; RX_FULL: the receive FIFO is full. See Status1 bits; 0: No error condition has been detected on the transmit or receive FIFOs
    uint8_t i_crc_chk      : 1;        // 1: The value of CRC_CHK newly valid. I.e. The validity of the incoming packet has been checked; 0: The value of CRC_CHK is not newly valid. I.e. The validity of the incoming packet has not been checked since the last packet was received
    uint8_t i_comp_chng    : 1;        // 1: A change in the value of COMP has occurred. Indicates selected CC line has tripped a threshold programmed into the MDAC; 0: No change in the value of COMP has occurred since the last time it was checked
    uint8_t i_activity     : 1;        // 1: A change in the value of ACTIVITY of the CC bus has occurred; 0: No change in the value of ACTIVITY of the CC bus has occurred since the last time it was checked
    uint8_t i_vbusok       : 1;        // 1: Interrupt occurs when VBUS transitions through 4.5 V. This bit typically is used to recognize port partner during startup; 0: No VBUS transition has been detected through 4.5 V since the last time it was checked
} Fusb302InterruptRegBits;
_Static_assert(
    sizeof(Fusb302InterruptRegBits) == 1,
    "Size check for 'Fusb302InterruptRegBits' failed.");

typedef struct {
    uint8_t token          : 8;        // Writing to this register writes a byte into the transmit FIFO. Reading from this register reads from the receive FIFO. Each byte is a coded token. Or a token followed by a fixed number of packed data bytes
} Fusb302FifosRegBits;
_Static_assert(
    sizeof(Fusb302FifosRegBits) == 1,
    "Size check for 'Fusb302FifosRegBits' failed.");

// Status1aTogss
#define FUSB302_STATUS1A_TOGSS_TOGGLE_LOGIC_RUNNING     (0b000)     /* Toggle logic running (processor has previously written TOGGLE=1) */
#define FUSB302_STATUS1A_TOGSS_SRCON_CC1                (0b001)     /* Toggle functionality has settled to SRCon CC1 (STOP_SRC1 state)*/
#define FUSB302_STATUS1A_TOGSS_SRCON_CC2                (0b010)     /* Toggle functionality has settled to SRCon CC2 (STOP_SRC2 state) */
#define FUSB302_STATUS1A_TOGSS_SNKON_CC1                (0b101)     /* Toggle functionality has settled to SNKon CC1 (STOP_SNK1 state) */
#define FUSB302_STATUS1A_TOGSS_SNKON_CC2                (0b110)     /* Toggle functionality has settled to SNKon CC2 (STOP_SNK2 state) */
#define FUSB302_STATUS1A_TOGSS_AUDIO_ACCESSORY          (0b111)     /* Toggle functionality has detected AudioAccessory with vRa on both CC1 and CC2 (settles to STOP_SRC1 state) */

// Receive FIFO token definitions
#define FUSB302_RX_TOKEN_SOP_MASK                       (0b111<<5) /* Mask for SOP token bits in the received token */
#define FUSB302_RX_TOKEN_SOP                            (0b111<<5)  /* Start of Packet token for SOP */ 
#define FUSB302_RX_TOKEN_SOP1                           (0b110<<5)  /* Start of Packet token for SOP’ (SOP prime) */
#define FUSB302_RX_TOKEN_SOP2                           (0b101<<5)  /* Start of Packet token for SOP” (SOP double prime) */ 
#define FUSB302_RX_TOKEN_SOP1DB                         (0b100<<5)  /* Start of Packet token for SOP’_DEBUG (SOP prime debug) */
#define FUSB302_RX_TOKEN_SOP2DB                         (0b011<<5)  /* Start of Packet token for SOP”_DEBUG (SOP double prime debug) */

// Transmit FIFO token definitions 
#define FUSB302_TX_TOKEN_TXON                           (0xA1)      /* Enable transmitter */ 
#define FUSB302_TX_TOKEN_SYNC1                          (0x12)      /* First sync token */ 
#define FUSB302_TX_TOKEN_SYNC2                          (0x13)      /* Second sync token */ 
#define FUSB302_TX_TOKEN_SYNC3                          (0x1B)      /* Third sync token */ 
#define FUSB302_TX_TOKEN_RST1                           (0x15)      /* First reset token */ 
#define FUSB302_TX_TOKEN_RST2                           (0x16)      /* Second reset token */ 
#define FUSB302_TX_TOKEN_PACKSYM                        (0x80)      /* Packet symbol */ 
#define FUSB302_TX_TOKEN_JAMCRC                         (0xFF)      /* JAM CRC token */ 
#define FUSB302_TX_TOKEN_EOP                            (0x14)      /* End of packet token */ 
#define FUSB302_TX_TOKEN_TXOFF                          (0xFE)      /* Disable transmitter */

/* clang-format on */
