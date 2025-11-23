#pragma once

//https://www.ti.com/lit/ds/symlink/drv2605l.pdf?ts=1763803821733&ref_url=https%253A%252F%252Fwww.ti.com%252Fproduct%252FDRV2605L%253Futm_source%253Dgoogle%2526utm_medium%253Dcpc%2526utm_campaign%253Dasc-null-null-GPN_EN-cpc-pf-google-eu_en_cons%2526utm_content%253DDRV2605L%2526ds_k%253DDRV2605L%2526DCM%253Dyes%2526gclsrc%253Daw.ds%2526gad_source%253D1%2526gad_campaignid%253D8752110670%2526gbraid%253D0AAAAAC068F1DNu-L2iP4S8o2M0XHmBTQT%2526gclid%253DCjwKCAiA24XJBhBXEiwAXElO38zRQot2y5ghyUHtV3rc_ENHblKwa4acWh9cJWM8MZWGArRetKJSMBoCsQkQAvD_BwE


// /I2C address
// #define I2C_ADDR 0x5A

// //The Status Register (0x00): The Device ID is bits 7-5. For DRV2605L it should be 7 or 111. 
// //bits 4 and 2 are reserved. Bit 3 is the diagnostic result. You want to see 0. 
// //bit 1 is the over temp flag, you want this to be 0
// //bit 0 is  over current flag, you want this to be zero. 
// // Ideally the register will read 0xE0.
// #define STATUS_REG 0x00 

// //The Mode Register (0x01): 
// //Default 010000000 -- Need to get it out of Standby
// //Set to 0000 0000=0x00 to use Internal Trigger
// //Set to 0000 0001=0x01 to use External Trigger (edge mode)(like a switch on the IN pin)
// //Set to 0000 0010=0x02 to use External Trigger (level mode)
// //Set to 0000 0011=0x03 to use PWM input and analog output
// //Set to 0000 0100=0x04 to use Audio to Vibe 
// //Set to 0000 0101=0x05 to use Real-Time Playback
// //Set to 0000 0110=0x06 to perform a diagnostic test - result stored in Diagnostic bit in register 0x00
// //Set to 0000 0111 =0x07 to run auto calibration 
// #define MODE_REG 0x01

// //The Feedback Control Register (0x1A)
// //bit 7: 0 for ERM, 1 for LRA -- Default is 0
// //Bits 6-4 control brake factor
// //bits 3-2 control the Loop gain
// //bit 1-0 control the BEMF gain
// #define FEEDBACK_REG 0x1A

// //The Real-Time Playback Register (0x02)
// //There are 6 ERM libraries. 
// #define RTP_REG 0x02

// //The Library Selection Register (0x03)
// //See table 1 in Data Sheet for 
// #define LIB_REG 0x03

// //The waveform Sequencer Register (0X04 to 0x0B)
// #define WAVESEQ1 0x04 //Bit 7: set this include a wait time between playback                                                                                                                                                                                 
// #define WAVESEQ2 0x05
// #define WAVESEQ3 0x06
// #define WAVESEQ4 0x07
// #define WAVESEQ5 0x08
// #define WAVESEQ6 0x09
// #define WAVESEQ7 0x0A
// #define WAVESEQ8 0x0B

// //The Go register (0x0C)
// //Set to 0000 0001=0x01 to set the go bit
// #define GO_REG 0x0C

// //The Overdrive Time Offset Register (0x0D)
// //Only useful in open loop mode
// #define OVERDRIVE_REG 0x0D

// //The Sustain Time Offset, Positive Register (0x0E)
// #define SUSTAINOFFSETPOS_REG 0x0E

// //The Sustain Time Offset, Negative Register (0x0F)
// #define SUSTAINOFFSETNEG_REG 0x0F

// //The Break Time Offset Register (0x10)
// #define BREAKTIME_REG 0x10

// //The Audio to Vibe control Register (0x11)
// #define AUDIOCTRL_REG 0x11

// //The Audio to vibe minimum input level Register (0x12)
// #define AUDMINLVL_REG 0x12

// //The Audio to Vibe maximum input level Register (0x13)
// #define AUDMAXLVL_REG 0x13

// // Audio to Vibe minimum output Drive Register (0x14)
// #define AUDMINDRIVE_REG 0x14

// //Audio to Vibe maximum output Drive Register (0x15)
// #define AUDMAXDRIVE_REG 0X15

// //The rated Voltage Register (0x16)
// #define RATEDVOLT_REG 0x16

// //The Overdive clamp Voltage (0x17)
// #define OVERDRIVECLAMP_REG 0x17

// //The Auto-Calibration Compensation - Result Register (0x18)
// #define COMPRESULT_REG 0x18

// //The Auto-Calibration Back-EMF Result Register (0x19)
// #define BACKEMF_REG 0x19

// //The Control1 Register (0x1B)
// //For AC coupling analog inputs and 
// //Controlling Drive time 
// #define CONTROL1_REG 0x1B

// //The Control2 Register (0x1C)
// //See Data Sheet page 45
// #define CONTROL2_REG 0x1C

// //The COntrol3 Register (0x1D)
// //See data sheet page 48
// #define CONTROL3_REG 0x1D

// //The Control4 Register (0x1E)
// //See Data sheet page 49
// #define CONTROL4_REG 0x1E

// //The Control5 Register (0x1F)
// //See Data Sheet page 50
// #define CONTROL5_REG 0X1F

// //The LRA Open Loop Period Register (0x20)
// //This register sets the period to be used for driving an LRA when 
// //Open Loop mode is selected: see data sheet page 50.
// #define OLP_REG 0x20

// //The V(Batt) Voltage Monitor Register (0x21)
// //This bit provides a real-time reading of the supply voltage 
// //at the VDD pin. The Device must be actively sending a waveform to take 
// //reading Vdd=Vbatt[7:0]*5.6V/255
// #define VBATMONITOR_REG 0x21

// //The LRA Resonance-Period Register 
// //This bit reports the measurement of the LRA resonance period
// #define LRARESPERIOD_REG 0x22



typedef enum {
    input_port_0 = 0x00, /**< Input port 0 */
    input_port_1 = 0x01, /**< Input port 1 */
    output_port_0 = 0x02, /**< Output port 0 */
    output_port_1 = 0x03, /**< Output port 1 */
    polarity_inversion_port_0 = 0x04, /**< Polarity inversion port 0 */
    polarity_inversion_port_1 = 0x05, /**< Polarity inversion port 1 */
    configuration_port_0 = 0x06, /**< Configuration port 0 */
    configuration_port_1 = 0x07, /**< Configuration port 1 */
} Tsa6416aReg;