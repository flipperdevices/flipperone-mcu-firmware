#pragma once

#include <stdint.h>

//https://www.ti.com/lit/ds/symlink/drv2605l.pdf?ts=1763803821733&ref_url=https%253A%252F%252Fwww.ti.com%252Fproduct%252FDRV2605L%253Futm_source%253Dgoogle%2526utm_medium%253Dcpc%2526utm_campaign%253Dasc-null-null-GPN_EN-cpc-pf-google-eu_en_cons%2526utm_content%253DDRV2605L%2526ds_k%253DDRV2605L%2526DCM%253Dyes%2526gclsrc%253Daw.ds%2526gad_source%253D1%2526gad_campaignid%253D8752110670%2526gbraid%253D0AAAAAC068F1DNu-L2iP4S8o2M0XHmBTQT%2526gclid%253DCjwKCAiA24XJBhBXEiwAXElO38zRQot2y5ghyUHtV3rc_ENHblKwa4acWh9cJWM8MZWGArRetKJSMBoCsQkQAvD_BwE

typedef enum {
    /* 0x00 STATUS
    *   - General device status information.
    *   - Bits 7:5 — Device ID (expected 0b111 for DRV2605L).
    *   - Bit 4 — Reserved.
    *   - Bit 3 — Diagnostic result (0 = OK).
    *   - Bit 2 — Reserved.
    *   - Bit 1 — Over-temperature flag (0 = normal).
    *   - Bit 0 — Over-current flag (0 = normal).
    *   - Used to verify device identity and health/diagnostics.
    */
    status = 0x00,
    /* 0x01 MODE
    *   - Selects operating mode and manages standby/go.
    *   - Bits 7 — Device reset (0 = normal operation, 1 = reset)
    *   - Bits 6 — Standby (0 = active, 1 = standby)
    *   - Bits 5:3 — Reserved (set to 0)
    *   - Bits 2:0 — Mode select (trigger/source/mode):
    *       0x00 — Internal Trigger,
    *       0x01 — External Trigger (edge),
    *       0x02 — External Trigger (level),
    *       0x03 — PWM input / analog output,
    *       0x04 — Audio-to-vibe,
    *       0x05 — Real-time playback,
    *       0x06 — Diagnostic,
    *       0x07 — Auto-calibration.
    *   - The register also contains standby/power-control bits.
    */
    mode = 0x01,
    /* 0x02 RTP (Real-Time Playback)
    *   - Holds the instantaneous RTP drive level used when MODE = 0x05.
    *   - Bits 7:0 — RTP drive level (0 to 255).
    */
    rtp_input = 0x02,
    /* 0x03 LIB (Library Select)
    *   - Selects the effect library (predefined waveform sets for ERM/LRA).
    *   - Bits 7:5 — Reserved (set to 0).
    *   - Bits 4 — HI_Z mode (0 = normal, 1 = 15k).
    *   - Bits 3 - Reserved (set to 0).
    *   - Bits 2:0 — Library select (0 to 7).
    *       0x00 — Empty,
    *       0x01 — TS2200 Library A,
    *       0x02 — TS2200 Library B,
    *       0x03 — TS2200 Library C,
    *       0x04 — TS2200 Library D,
    *       0x05 — TS2200 Library E,
    *       0x06 — LRA Library,
    *       0x07 — TS2200 Library F
    */
    lib_select = 0x03,
    /* 0x04–0x0B WAVESEQ1..WAVESEQ7
    *   - Waveform sequence slots (8 slots).
    *   - Bits 7 — Wait flag (1 = insert delay after this effect).
    *   - Bits 6:0 — Effect index (0 to 123) or 0x00 for 'end of sequence'.
    *   - if set bit 7 to bits 6:0 wait (0 to 123) * 10 ms.
    */
    waveseq0 = 0x04,
    waveseq1 = 0x05,
    waveseq2 = 0x06,
    waveseq3 = 0x07,
    waveseq4 = 0x08,
    waveseq5 = 0x09,
    waveseq6 = 0x0A,
    waveseq7 = 0x0B,
    /* 0x0C GO
    *   - Start/stop playback.
    *   - Bit 7:1 — Reserved (set to 0).
    *   - Bit 0 — GO bit (0 = stop, 1 = start).
    */
    go = 0x0C,
    /* 0x0D OVERDRIVE
    *   - Overdrive time offset (used in ERM open-loop mode).
    *   - Bits 7:0 — Overdrive time offset (0 to 255).
    */
    overdrive = 0x0D,
    /* 0x0E SUSTAIN_TIME_POS
    *   - Positive sustain time offset.
    *   - Bits 7:0 — Positive sustain time offset (0 to 255).
    */
    sustain_time_pos = 0x0E,
    /* 0x0F SUSTAIN_TIME_NEG
    *   - Negative sustain time offset.
    *   - Bits 7:0 — Negative sustain time offset (0 to 255
    */
    sustain_time_neg = 0x0F,
    /* 0x10 BREAK_TIME
    *   - Break time / pause between signal phases (break time offset).
    *   - Bits 7:0 — Break time offset (0 to 255).
    */
    break_time = 0x10,
    /* 0x11 ATH_CTRL
    *   - Audio-to-vibe settings: audio detector behavior, input mode,
    *     thresholds, and low/high-level handling.
    *   - Bits 7:4 — Reserved.
    *   - Bits 3:2 — This bit sets the peak detection time for the audio-to-vibe signal path: (0 = 10 ms, 1 = 20 ms, 2 = 30 ms, 3 = 40 ms).
    *   - Bit 1:0 — This bit sets the low-pass filter frequency for the audio-to-vibe signal path:(0 = 100Hz, 1 = 125Hz, 2 = 150Hz, 3 = 200Hz).
    */
    audio_ctrl = 0x11,
    /* 0x12 ATH_MIN_LVL
    *   - Minimum audio input threshold for the audio detector.
    *   - Bits 7:0 — Minimum audio input threshold (0 to 255).
    */
    aud_min_lvl = 0x12,
    /* 0x13 ATH_MAX_LVL
    *   - Maximum audio input threshold (for normalization/scaling).
    *   - Bits 7:0 — Maximum audio input threshold (0 to 255).
    */
    aud_max_lvl = 0x13,
    /* 0x14 ATH_MIN_DRIVE
    *   - Minimum output drive level corresponding to ATH_MIN_LVL.
    *   - Bits 7:0 — Minimum output drive level (0 to 255).
    */
    aud_min_drive = 0x14,
    /* 0x15 ATH_MAX_DRIVE
    *   - Maximum output drive level corresponding to ATH_MAX_LVL.
    *   - Bits 7:0 — Maximum output drive level (0 to 255).
    */
    aud_max_drive = 0x15,
    /* 0x16 RATED_VOLTAGE
    *   - Rated actuator voltage (used to compute drive levels).
    *   - Bits 7:0 — Rated voltage in mV (0 to 255).
    */
    rated_voltage = 0x16,
    /* 0x17 OD_CLAMP
    *   - Overdrive voltage clamp to protect the motor/device.
    *   - Bits 7:0 — Overdrive voltage clamp in mV (0 to 255).
    */
    overdrive_clamp = 0x17,
    /* 0x18 A_CAL_COMP
    *   - Auto-calibration compensation result (used to adjust drive profile).
    *   - Bits 7:0 — Compensation result (signed value).
    */
    auto_cal_comp = 0x18,
    /* 0x19 A_CAL_BEMF
    *   - Back-EMF measurement result after calibration or diagnostics;
    *   - Bits 7:0 — Back-EMF measurement result (0 to 255).
    */
    auto_cal_bemf = 0x19,
    /* 0x1A FEEDBACK
    *   - Feedback and actuator-type parameters:
    *   - Bit 7 — Type: 0 = ERM, 1 = LRA.
    *   - Bits 6:4 — Brake factor. (0- 1x, 1- 2x, 2- 3x, 3- 4x, 4- 6x, 5- 8x, 6- 16x, 7- Braking disabled).
    *   - Bits 3:2 — Loop gain. (0- Low, 1- Medium(default), 2- High, 3- Very High).
    *   - Bits 1:0 — BEMF gain. ERM Mode(0- 0.255x, 1- 0.7875x, 2- 1.365x(default), 3- 3.0x), LRA Mode(0- 3.75x, 1- 7.5x, 2- 15x (default), 3- 22.5x).
    */
    feedback = 0x1A,
    /* 0x1B CONTROL1
    *   - Additional control parameters (AC coupling, drive time, etc.).
    *   - Bits 7 — This bit applies higher loop gain during overdrive to enhance actuator transient response. (1- default).
    *   - Bits 6 — Reserved.
    *   - Bits 5 — This bit applies a 0.9-V common mode voltage to the IN/TRIG pin for AC-coupled analog input signals. (0- default).
    *   - Bits 4:0 — Drive time setting (0 to 31).
    */
    control1 = 0x1B,
    /* 0x1C CONTROL2
    *   - Amplifier and timing-related control parameters (see datasheet).
    *   - Bits 7 — The BIDIR_INPUT bit selects how the engine interprets data.
    *   - Bits 6 — When this bit is set, loop gain is reduced when braking is almost complete to improve loop stability.
    *   - Bits 5:4 — LRA auto-resonance sampling time (Advanced use only) (0- 150 µs, 1- 200 µs, 2- 250 µs, 3- 300 µs(default)).
    *   - Bits 3:2 — Blanking time before the back-EMF AD makes a conversion. (Advanced use only).
    *   - Bits 1:0 — Current dissipation time. This bit is the time allowed for the current to dissipate from the actuator between PWM cycles for flyback mitigation. (Advanced use only).
    */
    control2 = 0x1C,
    /* 0x1D CONTROL3
    *   - Additional control fields (refer to datasheet for bit meanings).
    *   - Bits 7:6 — This bit is the noise-gate threshold for PWM and analog inputs (0- Disabled, 1- 2%, 2- 4% (Default), 3-  8%).
    *   - Bits 5 — This bit selects mode of operation while in ERM mode. (0- Closed Loop, 1- Open Loop(default)).
    *   - Bits 4 — This bit disables supply compensation. (0- Enabled(default), 1- Disabled).
    *   - Bits 3 — This bit selects the input data interpretation for RTP (Real-Time Playback) mode (0- Signed, 1- Unsigned).
    *   - Bits 2 — This bit selects the drive mode for the LRA algorithm (0- Once per cycle, 1- Twice per cycle).
    *   - Bits 1 — This bit selects the input mode for the IN/TRIG pin when MODE[2:0] = 3 (0- PWM Input, 1- Analog Input).
    *   - Bits 0 — This bit selects an open-loop drive option for LRA Mode. (0- Auto-resonance mode(default), 1- LRA open-loop mode ).
    */
    control3 = 0x1D,
    /* 0x1E CONTROL4    
    *   - Output gain/protection/phase settings (see datasheet).
    *   - Bits 7:6 — This bit sets the minimum length of time devoted for detecting a zero crossing (advanced use only) (0- 100 µs(default), 1- 200 µs, 2- 300 µs, 3- 390 µs).
    *   - Bits 5:4 — This bit sets the length of the auto calibration time.  (0- 150:350 ms, 1-  250:450 ms, 2- 500:700 ms(default), 3- 1000:1200 ms).
    *   - Bits 3 — Reserved.
    *   - Bits 2 — OTP Memory status (0- OTP Memory has not been programmed, 1- OTP Memory has been programmed).
    *   - Bits 1 — Reserved.
    *   - Bits 0 — This bit launches the programming process for one-time programmable (OTP) memory.
    */
    control4 = 0x1E,
    /* 0x1F CONTROL5
    *   - Further control parameters (see datasheet for details).
    *   - Bits 7:6 — This bit selects number of cycles required to attempt synchronization before transitioning to 
                    open loop when the LRA_AUTO_OPEN_LOOP bit is asserted (0- 3 attempts, 1- 4 attempts, 2- 5 attempts(default), 3- 6 attempts).
    *   - Bits 5 — This bit selects the automatic transition to open-loop drive when a back-EMF signal is not detected (LRA only). (0- Never transitions to open loop(default), 1- Automatically transitions to open loop).
    *   - Bits 4 — This bit selects the memory playback interval. (0- 5 ms(default), 1- 1 ms).
    *   - Bits 3:2 — This bit sets the MSB for the BLANKING_TIME[3:0].
    *   - Bits 1:0 — This bit sets the MSB for IDISS_TIME[3:0].
    */
    control5 = 0x1F,
    /* 0x20 OLP (LRA Open-Loop Period)
    *   - Period value (in counts/codes) for driving an LRA in open-loop.
    *   - Used when open-loop drive is selected for LRA (sets drive frequency).
    *   - Bits 7:0 — LRA open-loop period (0 to 255).
    */
    lra_open_loop_period = 0x20,
    /* 0x21 V_BAT_MONITOR
    *   - Supply voltage monitor (VDD/VBAT).
    *   - Register stores a scaled Vbatt reading; VDD (V) = VBAT[7:0] × 5.6V / 255 formula to convert to volts.         
    *   - Bits 7:0 — Scaled Vbatt reading (0 to 255).
    */
    v_bat_monitor = 0x21,
    /* 0x22 LRA_RES_PERIOD
    *   - Measured LRA resonance period (result from measurement/calibration)
    *     used to tune drive frequency.
    *   - Bits 7:0 — Measured LRA resonance period (0 to 255).
    */
    lra_res_period = 0x22,
} Drv2605lReg;

typedef struct {
    uint8_t over_current_flag : 1; // Bit 0 — Over-current flag (0 = normal).
    uint8_t over_temperature_flag : 1; // Bit 1 — Over-temperature flag (0 = normal).
    uint8_t reserved_2 : 1; // Bit 2 — Reserved.
    uint8_t diagnostic_result : 1; // Bit 3 — Diagnostic result (0 = OK).
    uint8_t reserved_1 : 1; // Bit 4 — Reserved.
    uint8_t device_id : 3; // Bits 7:5 — Device ID (expected 0b111 for DRV2605L).
} Drv2605lStatus;

typedef struct{
    uint8_t mode_select : 3; // Bits 2:0 — Mode select (trigger/source/mode)
    uint8_t reserved_1 : 3; // Bits 5:3 — Reserved (set to 0)
    uint8_t standby : 1; // Bit 6 — Standby (0 = active, 1 = standby)
    uint8_t device_reset : 1; // Bit 7 — Device reset (0 = normal operation, 1 = reset)
}Drv2605lMode;

typedef struct{
    uint8_t library_sel: 3; // Bits 2:0 — Library select (0 to 7).
    uint8_t reserved_3: 1; // Bits 3 - Reserved (set to 0).
    uint8_t hi_z_mode: 1; // Bits 4 — HI_Z mode (0 = normal, 1 = 15k).
    uint8_t reserved_2: 3; // Bits 7:5 — Reserved (set to 0).
} Drv2605lLib;

typedef struct{
    uint8_t effect_index : 7; // Bits 6:0 — Effect index (0 to 123) or 0x00 for 'end of sequence'.
    uint8_t wait_flag : 1; // Bits 7 — Wait flag (1 = insert delay after this effect).
} Drv2605lWaveSeq;

typedef struct{
    uint8_t go_bit : 1; // Bit 0 — GO bit (0 = stop, 1 = start).
    uint8_t reserved_1 : 7; // Bit 7:1 — Reserved (set to 0).
} Drv2605lGo;

typedef struct{
    uint8_t ath_filter : 2; // Bits 1:0 — This bit sets the low-pass filter frequency for the audio-to-vibe signal path:(0 = 100Hz, 1 = 125Hz, 2 = 150Hz, 3 = 200Hz).
    uint8_t ath_peek_time : 2; // Bits 3:2 — This bit sets the peak detection time for the audio-to-vibe signal path: (0 = 10 ms, 1 = 20 ms, 2 = 30 ms, 3 = 40 ms).
    uint8_t reserved_1 : 4; // Bits 7:4 — Reserved
} Drv2605lAudioCtrl;

typedef struct{
    uint8_t bemf_gain : 2; // Bits 1:0 — BEMF gain. ERM Mode(0- 0.255x, 1- 0.7875x, 2- 1.365x(default), 3- 3.0x), LRA Mode(0- 3.75x, 1- 7.5x, 2- 15x (default), 3- 22.5x).
    uint8_t loop_gain : 2; // Bits 3:2 — Loop gain. (0- Low, 1- Medium(default), 2- High, 3- Very High).
    uint8_t brake_factor : 3; // Bits 6:4 — Brake factor. (0- 1x, 1- 2x, 2- 3x, 3- 4x, 4- 6x, 5- 8x, 6- 16x, 7- Braking disabled).
    uint8_t n_erm_lra : 1; // Bit 7 — Type: 0 = ERM, 1 = LRA.
}Drv2605lFeedback;

typedef struct{
    uint8_t drive_time : 5; // Bits 4:0 — Drive time setting (0 to 31).
    uint8_t ac_couple : 1; // Bits 5 — This bit applies a 0.9-V common mode voltage to the IN/TRIG pin for AC-coupled analog input signals. (0- default).
    uint8_t reserved_1 : 1; // Bits 6 — Reserved.
    uint8_t startup_boost : 1; // Bits 7 — This bit applies higher loop gain during overdrive to enhance actuator transient response. (1- default).
} Drv2605lControl1;

typedef struct{
    uint8_t idiss_time : 2; // Bits 1:0 — Current dissipation time. This bit is the time allowed for the current to dissipate from the actuator between PWM cycles for fly
    uint8_t blanking_time : 2; // Bits 3:2 — Blanking time before the back-EMF AD makes a conversion. (Advanced use only).
    uint8_t sample_time : 2; // Bits 5:4 — LRA auto-resonance sampling time (Advanced use only) (0- 150 µs, 1- 200 µs, 2- 250 µs, 3- 300 µs(default)).
    uint8_t brake_stabilizer : 1; // Bit 6 — When this bit is set, loop gain is reduced when braking is almost complete to improve loop stability.
    uint8_t bidir_input : 1; // Bit 7 — The BIDIR_INPUT bit selects how the engine interprets data.
} Drv2605lControl2;

typedef struct{
    uint8_t lra_open_loop : 1; // Bits 0 — This bit selects an open-loop drive option for LRA Mode. (0- Auto-resonance mode(default), 1- LRA open-loop mode ).
    uint8_t n_pwm_analog : 1; // Bits 1 — This bit selects the input mode for the IN/TRIG pin when MODE[2:0] = 3 (0- PWM Input, 1- Analog Input).
    uint8_t lra_drive_mode : 1; // Bits 2 — This bit selects the drive mode for the LRA algorithm (0- Once per cycle, 1- Twice per cycle).
    uint8_t data_fomat_rtp : 1; // Bits 3 — This bit selects the input data interpretation for RTP (Real-Time Playback) mode (0- Signed, 1- Unsigned).
    uint8_t supply_comp_dis : 1; // Bits 4 — This bit disables supply compensation. (0- Enabled(default), 1- Disabled).
    uint8_t erm_open_loop : 1; // Bits 5 — This bit selects mode of operation while in ERM mode. (0- Closed Loop, 1- Open Loop(default)).
    uint8_t ng_thresh : 2; // Bits 7:6 — This bit is the noise-gate threshold for PWM and analog inputs (0- Disabled, 1- 2%, 2- 4% (Default), 3-  8%).
} Drv2605lControl3;

typedef struct{
    uint8_t otp_program : 1; // Bits 0 — This bit launches the programming process for one-time programmable (OTP) memory.
    uint8_t reserved_1 : 1; // Bits 1 — Reserved.
    uint8_t otp_status : 1; // Bits 2 — OTP Memory status (0- OTP Memory has not been programmed, 1- OTP Memory has been programmed).
    uint8_t reserved_2 : 1; // Bits 3 — Reserved.
    uint8_t auto_cal_time : 2; // Bits 5:4 — This bit sets the length of the auto calibration time.  (0- 150:350 ms, 1-  250:450 ms, 2- 500:700 ms(default), 3- 1000:1200 ms).
    uint8_t zc_det_time : 2; // Bits 7:6 — This bit sets the minimum length of time devoted for detecting a zero crossing (advanced use only) (0- 100 µs(default), 1- 200 µs, 2- 300 µs, 3- 390 µs).
} Drv2605lControl4;

typedef struct{
    uint8_t idiss_time : 2; // Bits 1:0 — This bit sets the MSB for IDISS_TIME[3:0].
    uint8_t blanking_time : 2; // Bits 3:2 — This bit sets the MSB for the BLANKING_TIME[3:0].
    uint8_t playback_interval : 1; // Bits 4 — This bit selects the memory playback interval. (0- 5 ms(default), 1- 1 ms).
    uint8_t lra_auto_open_loop : 1; // Bits 5 — This bit selects the automatic transition to open-loop drive when a back-EMF signal is not detected (LRA only). (0- Never transitions to open loop(default), 1- Automatically transitions to open loop).
    uint8_t auto_ol_cnt : 2; // Bits 7:6 — This bit selects number of cycles required to attempt synchronization before transitioning to open loop when the LRA_AUTO_OPEN_LOOP bit is asserted (0- 3 attempts, 1- 4 attempts, 2- 5 attempts(default), 3- 6 attempts).
} Drv2605lControl5;
