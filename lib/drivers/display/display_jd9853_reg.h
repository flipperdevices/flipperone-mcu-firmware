#pragma once
#include <stdint.h>
#include <stddef.h>
#include "display_jd9853_refresh.h"

/* clang-format off */
#define JD9853_WIDTH        (258u)
#define JD9853_HEIGHT       (144u)
#define JD9853_OFF_X0       (77u)
#define JD9853_OFF_Y0       (320u - JD9853_HEIGHT) // was 0 without mirroring and rotation
#define JD9853_OFF_X1       (JD9853_OFF_X0 + (JD9853_WIDTH / 3) - 1)
#define JD9853_OFF_Y1       (JD9853_OFF_Y0 + JD9853_HEIGHT - 1)

#define JD9853_QSPI_CMD_1_LINE_MODE (0x02u)
#define JD9853_QSPI_CMD_4_LINE_MODE (0x10001100u) // Quad SPI command 0x32
#define JD9853_QSPI_CMD_4_LINE_RAMWR (0x00111000u) // Quad SPI data 0x2C

//https://github.com/flipperdevices/flipperone-mcu-firmware/blob/main/docs/GWT-2.39-256144-AWMN-04-FS-1.1.pdf

typedef enum {
    DisplayJd9853RegNop = 0x00,         /* No Operation */
    DisplayJd9853RegSwreset = 0x01,     /* Software Reset */
    DisplayJd9853RegRddidif = 0x04,     /* Read Display ID */
    DisplayJd9853RegRdred = 0x06,       /* Read Display Red */
    DisplayJd9853RegRdgreen = 0x07,     /* Read Display Green */
    DisplayJd9853RegRdblue = 0x08,      /* Read Display Blue */
    DisplayJd9853RegRddst = 0x09,       /* Read Display Status */
    DisplayJd9853RegRddpm = 0x0A,       /* Read Display Power Mode */
    DisplayJd9853RegRddmadctl = 0x0B,   /* Read Display MADCTL */
    DisplayJd9853RegRddcolmod = 0x0C,   /* Read Display Pixel Format */
    DisplayJd9853RegRddim = 0x0D,       /* Read Display Image Mode */
    DisplayJd9853RegRddsm = 0x0E,       /* Read Display Signal Mode */
    DisplayJd9853RegRddsdr = 0x0F,      /* Read Display Self-Diagnostic Result */
    DisplayJd9853RegSlpin = 0x10,       /* Sleep In */
    DisplayJd9853RegSlpout = 0x11,      /* Sleep Out */
    DisplayJd9853RegPtlon = 0x12,       /* Partial Display Mode On */
    DisplayJd9853RegNoron = 0x13,       /* Normal Display Mode On */
    DisplayJd9853RegInvoff = 0x20,      /* Display Inversion Off */
    DisplayJd9853RegInvon = 0x21,       /* Display Inversion On */
    DisplayJd9853RegDispoff = 0x28,     /* Display Off */
    DisplayJd9853RegDispon = 0x29,      /* Display On */
    DisplayJd9853RegCaset = 0x2A,       /* Column Address Set */
    DisplayJd9853RegPaset = 0x2B,       /* Page Address Set */
    DisplayJd9853RegRamwr = 0x2C,       /* Memory Write */
    DisplayJd9853RegPltar = 0x30,       /* Partial Area */
    DisplayJd9853RegVscrdef = 0x33,     /* Vertical Scrolling Definition */
    DisplayJd9853RegTeoff = 0x34,       /* Tearing Effect Line OFF */
    DisplayJd9853RegTeon = 0x35,        /* Tearing Effect Line ON */
    DisplayJd9853RegMadctl = 0x36,      /* Memory Data Access Control */
    DisplayJd9853RegVscsad = 0x37,      /* Vertical Scroll Start Address of RAM */
    DisplayJd9853RegIdmoff = 0x38,      /* Idle Mode Off */
    DisplayJd9853RegIdmon = 0x39,       /* Idle Mode On */
    DisplayJd9853RegColmod = 0x3A,      /* Interface Pixel Format */
    DisplayJd9853RegRamwrcon = 0x3C,    /* Write Memory Continue */
    DisplayJd9853RegGetscan = 0x45,     /* Return the current scan line */
    DisplayJd9853RegRdabcsdr = 0x68,    /* Read Adaptive Brightness Control and Self-Diagnostic Result */
    DisplayJd9853RegRdid1 = 0xDA,       /* Read ID1 Value */
    DisplayJd9853RegRdid2 = 0xDB,       /* Read ID2 Value */
    DisplayJd9853RegRdid3 = 0xDC,       /* Read ID3 Value */
    DisplayJd9853RegSetPage = 0xDE,     /* Page select, available on every page */
    DisplayJd9853RegSetPasswd = 0xDF,   /* Password, unlocks the vendor registers */

    /* Vendor registers, valid only after JD9853_PAGE(n). The same address is
     * a different register on another page, so the name carries the page.
     * Names follow the datasheet, see jd9853_reg.h. */
    DisplayJd9853RegP0VgpowSet = 0xB2,  /* VCOM / gate power */
    DisplayJd9853RegP0GammaSet = 0xB7,  /* Gamma voltages */
    DisplayJd9853RegP0PowCtrl = 0xB9,   /* Power control */
    DisplayJd9853RegP0DcdcSel = 0xBB,   /* Charge pump voltages and clocks */
    DisplayJd9853RegP0SetStba = 0xC0,   /* Source/gate timing */
    DisplayJd9853RegP0SetPanel = 0xC1,  /* Panel setting */
    DisplayJd9853RegP0SetRgbCyc = 0xC3, /* RGB cycle / inversion */
    DisplayJd9853RegP0SetTcon = 0xC4,   /* Timing control */
    DisplayJd9853RegP0SetRGamma = 0xC8, /* Red gamma */
    DisplayJd9853RegP0SetGipL = 0xD0,   /* Gate driver (GIP) */
    DisplayJd9853RegP0RamCtrl = 0xD7,   /* RAM control */
    DisplayJd9853RegP0SetEco = 0xE6,    /* ECO */

    DisplayJd9853RegP1PowerOpt = 0xB2,  /* Power option */
    DisplayJd9853RegP1DcdcOtp = 0xB7,   /* DC/DC option */
    DisplayJd9853RegP1MaPower = 0xBE,   /* MA power */
    DisplayJd9853RegP1SetRgbCyc2 = 0xC1, /* RGB cycle 2 */
    DisplayJd9853RegP1SetStba2 = 0xC2,  /* Source/gate timing 2 */
    DisplayJd9853RegP1SetGammaOpt = 0xC4, /* Gamma OP */
    DisplayJd9853RegP1SetOscm = 0xC5,   /* Oscillator */

    DisplayJd9853RegP2GramTmgSet = 0xB5, /* GRAM timing */
} DisplayJd9853Reg;

typedef struct {
    uint8_t reg;        // DisplayJd9853Reg
    uint8_t len;        // data bytes, without the command byte
    uint16_t delay_ms;  // delay after the command
    const uint8_t* data;
} DisplayJd9853Cmd;

typedef struct DisplayJd9853Seq {
    const DisplayJd9853Cmd* cmds;
    size_t count;
} DisplayJd9853Seq;

#define JD9853_CMD_DELAY(_reg, _delay_ms, ...)          \
    {                                                   \
        .reg = (_reg),                                  \
        .len = sizeof((const uint8_t[]){__VA_ARGS__}),  \
        .delay_ms = (_delay_ms),                        \
        .data = (const uint8_t[]){__VA_ARGS__},         \
    }
#define JD9853_CMD(_reg, ...) JD9853_CMD_DELAY(_reg, 0, __VA_ARGS__)
#define JD9853_CMD0(_reg, _delay_ms) {.reg = (_reg), .len = 0, .delay_ms = (_delay_ms), .data = NULL}
#define JD9853_PAGE(_page) JD9853_CMD(DisplayJd9853RegSetPage, (_page))

#define JD9853_SEQ(_name, ...)                                        \
    static const DisplayJd9853Cmd _name##_cmds[] = {__VA_ARGS__};     \
    static const DisplayJd9853Seq _name = {                           \
        .cmds = _name##_cmds,                                         \
        .count = sizeof(_name##_cmds) / sizeof(_name##_cmds[0]),      \
    }

JD9853_SEQ(jd9853_deinit_seq,
        JD9853_CMD0(DisplayJd9853RegDispoff, 100),                // Display off
        JD9853_CMD0(DisplayJd9853RegSlpin, 100),                  // Enter sleep mode
);

JD9853_SEQ(jd9853_init_seq_2025_04_01_normal_white_mod,
        JD9853_CMD0(DisplayJd9853RegSwreset, 150),                // Software reset: needs >= 120 ms in sleep out
        JD9853_CMD(DisplayJd9853RegSetPasswd, 0x98, 0x53),
        JD9853_PAGE(0),
            JD9853_CMD(DisplayJd9853RegP0VgpowSet, 0x28),         // Vcom !64 //does not affect in any way
            JD9853_CMD(DisplayJd9853RegP0GammaSet,
                    0x0,                                          // VGSP_S 0v
                    0x67,                                         // VGMP_S 6.05v (was 0x7D 6.6v), tuned with VGMN against flicker at 10/15 FPS
                    0x0,                                          // VGSN_S 0v
                    0x6D),                                        // VGMN_S -5.2v (was 0x7D -5.6v)
            JD9853_CMD(DisplayJd9853RegP0PowCtrl, 0b00000101),    // AP AP[2:0] : 0x0, MVZ_E: 0x1, VGM_E: 0x1
            // Voltage settings
            JD9853_CMD(DisplayJd9853RegP0DcdcSel,
                    0b00000110,                                   // VGHZ_S = 11V, VGLZ_S = -11.5V (was 0: -7V), less TFT leakage at 10/15 FPS
                    0b00100011,                                   // VGHZ_RT  AVDDZ_S  VGLZ_RT  AVEEZ_S    VGHZ_RT[1:0]  : 0x0 AVDDZ_S[1:0] : 0x2  VGLZ_RT[1:0] : 0x0 AVEEZ_S[1:0]: 0x3
                    0b01010101,                                   // VGHZ_CLK  VGLZ_CLK
                    0b01100001,                                   // MVZ_S_CLK  MVZ_G_CLK
                    0x7F,                                         // MVZ_NS_CLK  HV_CLKP  MV_CLKP
                    0xFE),                                        // VGHZ_CLKE  CGLZ_CLKE  MVZ_S_CLKE  MVZ_NS_CLKE
            JD9853_CMD(DisplayJd9853RegP0SetStba, 0x44, 0x44),
            JD9853_CMD(DisplayJd9853RegP0SetPanel, 0x16),
            // JD9853_CMD(DisplayJd9853RegP0SetRgbCyc, 0x7E, 0x07, 0x16, 0x0B, 0xCB, 0x71, 0x72, 0x7F),
            // 0b01111111 -> 0b01111100 RGB_INV_NP (1-dot ) +0,5ма, 0b01111110 RGB_INV_NP (2-dot ) +0,25ма  check values FPS 0x16, 0x0B, (-0.3ma)
            JD9853_CMD(DisplayJd9853RegP0SetRgbCyc, 0b01111100, 0x07, 0x16, 0x0B,
                    0xCB, 0x71, 0x72, 0x00),
            JD9853_CMD(DisplayJd9853RegP0SetTcon, 0x00, 0x00, 0x48, 0x79,
                    0x14, 0x12, 0x16, 0x79,
                    0x0C, 0x0A, 0x16, 0x82),
            JD9853_CMD(DisplayJd9853RegP0SetRGamma, 0x3F, 0x35, 0x24, 0x2D,
                    0x3F, 0x3F, 0x3F, 0x3F,
                    0x3F, 0x3F, 0x3F, 0x3F,
                    0x3F, 0x3F, 0x3F, 0x3F,
                    0x3F, 0x3F, 0x3F, 0x3F,
                    0x3F, 0x3F, 0x3F, 0x3F,
                    0x3F, 0x3F, 0x3F, 0x3F,
                    0x3F, 0x3F, 0x3F, 0x3F),
            JD9853_CMD(DisplayJd9853RegP0SetGipL, 0x04, 0x06, 0x6B, 0x0F, 0x00),
            JD9853_CMD(DisplayJd9853RegP0RamCtrl, 0x00, 0x30),
            JD9853_CMD(DisplayJd9853RegP0SetEco, 0x14),
        JD9853_PAGE(1),
            // JD9853_CMD(DisplayJd9853RegP1PowerOpt, 0x12),      // option ??????????
            JD9853_CMD(DisplayJd9853RegP1DcdcOtp, 0x07, 0x13, 0x0F, 0x39, 0x2D),
            JD9853_CMD(DisplayJd9853RegP1SetRgbCyc2, 0x34, 0x15, 0xE0),
            JD9853_CMD(DisplayJd9853RegP1SetStba2, 0x06, 0x3A, 0xC7),
            JD9853_CMD(DisplayJd9853RegP1SetGammaOpt, 0x7A, 0x1A),
            JD9853_CMD(DisplayJd9853RegP1SetOscm, 0x01),          // OSC ~113FPS
            // JD9853_CMD(DisplayJd9853RegP1MaPower, 0x00),       // GAMMA_POWER_TEST
        JD9853_PAGE(2),
            JD9853_CMD(DisplayJd9853RegP2GramTmgSet, 0x0A, 0x1C),
        JD9853_PAGE(0),
        // Standard commands, the same on every page
        JD9853_CMD(DisplayJd9853RegTeon, 0x00),                   // Tearing Effect Line ON
        JD9853_CMD(DisplayJd9853RegColmod, 0x06),                 // Set colour mode to RGB666
        JD9853_CMD(DisplayJd9853RegCaset,                         // CASET: column addresses
                JD9853_OFF_X0 >> 8, JD9853_OFF_X0 & 0xff,
                JD9853_OFF_X1 >> 8, JD9853_OFF_X1 & 0xff),
        JD9853_CMD(DisplayJd9853RegPaset,                         // RASET: row addresses
                JD9853_OFF_Y0 >> 8, JD9853_OFF_Y0 & 0xff,
                JD9853_OFF_Y1 >> 8, JD9853_OFF_Y1 & 0xff),
        JD9853_CMD(DisplayJd9853RegMadctl, 0b11001000),           // Set MADCTL: row then column, refresh is bottom to top ????
        JD9853_CMD0(DisplayJd9853RegSlpout, 100),                 // Exit sleep mode
);

/* Dynamic refresh sequences.
 *
 * Only two registers change between steps:
 *   C5 SET_OSCM - OSC_DIV [5:4] (oscillator divider for the timing
 *                 generator) and OSCM_ADJ [1:0] (oscillator frequency);
 *   C4 SET_TCON - VFP_NP and VBP_NP (data bytes 5 and 6), extra lines.
 * Frame time = (144 + VFP + VBP) * 484 / (f_OSC / OSC_DIV), i.e. 484
 * oscillator clocks per line and 144 gate lines per frame.
 *
 * The 10 FPS step is the low-power one: it is the only step that also drops
 * OSCM_ADJ to 16 MHz. That oscillator clocks the charge pumps (BBh), so they
 * run 20% slower there. The max step is identical to the init sequence. */
#define JD9853_REFRESH_SEQ(_name, _vfp, _vbp, _oscm)                                                   \
    JD9853_SEQ(_name,                                                                                  \
            JD9853_PAGE(0),                                                                            \
                JD9853_CMD(DisplayJd9853RegP0SetTcon, 0x00, 0x00, 0x48, 0x79, (_vfp), (_vbp), 0x16, 0x79, 0x0C, 0x0A, 0x16, 0x82), \
            JD9853_PAGE(1),                                                                            \
                JD9853_CMD(DisplayJd9853RegP1SetOscm, (_oscm)),                                                      \
            JD9853_PAGE(0))

//                                              VFP   VBP   OSCM
// VFP = VBP = 127: 144 + 254 = 398 lines
JD9853_REFRESH_SEQ(jd9853_refresh_seq_10fps,    0x7F, 0x7F, 0x20); // OSCM 16 MHz, TCON /8: 2 MHz
// VFP = VBP = 100: 144 + 200 = 344 lines
JD9853_REFRESH_SEQ(jd9853_refresh_seq_15fps,    0x64, 0x64, 0x21); // OSCM 20 MHz, TCON /8: 2.5 MHz
JD9853_REFRESH_SEQ(jd9853_refresh_seq_30fps,    0x0A, 0x12, 0x21); // OSCM 20 MHz, TCON /8: 2.5 MHz
JD9853_REFRESH_SEQ(jd9853_refresh_seq_60fps,    0x0A, 0x12, 0x11); // OSCM 20 MHz, TCON /4: 5 MHz
JD9853_REFRESH_SEQ(jd9853_refresh_seq_max,      0x14, 0x12, 0x01); // OSCM 20 MHz, TCON /2: 10 MHz

typedef struct {
    const DisplayJd9853Seq* config;
    uint8_t fps;
    uint32_t fast_delta_us;
} DisplayJd9853RefreshConfig;

/* fast_delta_us: 5/6 of the frame period at this FPS. Frames arriving faster
 * than this mean the current refresh is too slow, so step up. */
#define JD9853_FAST_DELTA_US(_fps) ((5u * 1000000u) / (6u * (_fps)))

#define JD9853_REFRESH_CONFIG(_seq, _fps)               \
    {                                                   \
        .config = &(_seq),                              \
        .fps = (_fps),                                  \
        .fast_delta_us = JD9853_FAST_DELTA_US(_fps),    \
    }

static const DisplayJd9853RefreshConfig
    jd9853_refresh_config[DisplayJd9853RefreshCount] = {
        [DisplayJd9853Refresh10] = JD9853_REFRESH_CONFIG(jd9853_refresh_seq_10fps, 10),   // ~10.4 FPS, low power
        [DisplayJd9853Refresh15] = JD9853_REFRESH_CONFIG(jd9853_refresh_seq_15fps, 15),   // ~15.0 FPS
        [DisplayJd9853Refresh30] = JD9853_REFRESH_CONFIG(jd9853_refresh_seq_30fps, 30),   // ~30.0 FPS
        [DisplayJd9853Refresh60] = JD9853_REFRESH_CONFIG(jd9853_refresh_seq_60fps, 60),   // ~60.0 FPS
        [DisplayJd9853RefreshMax] = JD9853_REFRESH_CONFIG(jd9853_refresh_seq_max, 115),   // ~113 FPS
};
/* clang-format on */
