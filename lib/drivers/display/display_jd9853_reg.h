#pragma once
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
    nop = 0x00, /* No Operation */
    swreset = 0x01, /* Software Reset */
    rddidif = 0x04, /* Read Display ID */
    rdred = 0x06, /* Read Display Red */
    rdgreen = 0x07, /* Read Display Green */
    rdblue = 0x08, /* Read Display Blue */
    rddst = 0x09, /* Read Display Status */
    rddpm = 0x0A, /* Read Display Power Mode */
    rddmadctl = 0x0B, /* Read Display MADCTL */
    rddcolmod = 0x0C, /* Read Display Pixel Format */
    rddim = 0x0D, /* Read Display Image Mode */
    rddsm = 0x0E, /* Read Display Signal Mode */
    rddsdr = 0x0F, /* Read Display Self-Diagnostic Result */
    slpin = 0x10, /* Sleep In */
    slpout = 0x11, /* Sleep Out */
    ptlon = 0x12, /* Partial Display Mode On */
    noron = 0x13, /* Normal Display Mode On */
    invoff = 0x20, /* Display Inversion Off */
    invon = 0x21, /* Display Inversion On */
    dispoff = 0x28, /* Display Off */
    dispon = 0x29, /* Display On */
    caset = 0x2A, /* Column Address Set */
    paset = 0x2B, /* Page Address Set */
    ramwr = 0x2C, /* Memory Write */
    pltar = 0x30, /* Partial Area */
    vscrdef = 0x33, /* Vertical Scrolling Definition */
    teoff = 0x34, /* Tearing Effect Line OFF */
    teon = 0x35, /* Tearing Effect Line ON */
    madctl = 0x36, /* Memory Data Access Control */
    vscsad = 0x37, /* Vertical Scroll Start Address of RAM */
    idmoff = 0x38, /* Idle Mode Off */
    idmon = 0x39, /* Idle Mode On */
    colmod = 0x3A, /* Interface Pixel Format */
    ramwrcon = 0x3C, /* Write Memory Continue */
    getscan = 0x45, /* Return the current scan line */
    rdabcsdr = 0x68, /* Read Adaptive Brightness Control and Self-Diagnostic Result */
    set_tcon = 0xC4, /* Timing control setting (page 0) */
    set_oscm = 0xC5, /* Oscillator M setting (page 1) */
    rdid1 = 0xDA, /* Read ID1 Value */
    rdid2 = 0xDB, /* Read ID2 Value */
    rdid3 = 0xDC, /* Read ID3 Value */
    set_page = 0xDE, /* Page select */
} DisplayJd9853Reg;

static const uint8_t jd9853_deinit_seq[] = {
        1, 20, 0x28,                        // Display off
        1, 20, 0x10,                        // Enter sleep mode
        0                                   // Terminate list
};

static const uint8_t jd9853_init_seq_2025_04_01_normal_white_mod[] = {
        1, 20, 0x01,                                // Software reset
        3, 0, 0xDF, 0x98, 0x53,                     // SET_PASSWD
        2, 0, 0xDE, 0x00,                           // Page0
        2, 0, 0xB2, 0x28,                           // Vcom !64 //does not affect in any way
        // GAMMA_SET
        5, 0, 0xB7, 0x0,                            // VGSP_S 0v
                    0x7D,                           // VGMP_S 6.6v
                    0x0,                            // VGSN_S 0v
                    0x7D,                           // VGMN_S -5.6v
        2, 0, 0xB9, 0b00000101,                     // AP AP[2:0] : 0x0, MVZ_E: 0x1, VGM_E: 0x1
        // Voltage settings
        7, 0, 0xBB, 0b00000000,                     // VGHZ_S =11V VGLZ_S =-9.25V VGHZ_S[1:0] =0x0 VGLZ_S[1:0]=0x0
                    0b00100011,                     // VGHZ_RT  AVDDZ_S  VGLZ_RT  AVEEZ_S    VGHZ_RT[1:0]  : 0x0 AVDDZ_S[1:0] : 0x2  VGLZ_RT[1:0] : 0x0 AVEEZ_S[1:0]: 0x3
                    0b01010101,                     // VGHZ_CLK  VGLZ_CLK
                    0b01100001,                     // MVZ_S_CLK  MVZ_G_CLK
                    0x7F,                           // MVZ_NS_CLK  HV_CLKP  MV_CLKP
                    0xFE,                           // VGHZ_CLKE  CGLZ_CLKE  MVZ_S_CLKE  MVZ_NS_CLKE

        3, 0, 0xC0, 0x44, 0x44,                     // SETSTBA
        2, 0, 0xC1, 0x16,                           // SETPANEL
        // 9, 0, 0xC3, 0x7E, 0x07, 0x16, 0x0B,         // SETRGBCYC !!!!!!!!!!!!!!!!!
        //             0xCB, 0x71, 0x72, 0x7F, 
        9, 0, 0xC3, 0b01111100, 0x07, 0x16, 0x0B,         // SETRGBCYC !!!!!!!!!!!!!!!!! 0b01111111 -> 0b01111100 RGB_INV_NP (1-dot ) +0,5ма, 0b01111110 RGB_INV_NP (2-dot ) +0,25ма  check values FPS 0x16, 0x0B, (-0.3ma)
                    0xCB, 0x71, 0x72, 0x00, 
        13, 0, 0xC4, 0x00, 0x00, 0x48, 0x79,        // SET_TCON
                    0x14, 0x12, 0x16, 0x79,
                    0x0C, 0x0A, 0x16, 0x82,
        33, 0, 0xC8, 0x3F, 0x35, 0x24, 0x2D,        // SET_R_GAMMA
                    0x3F, 0x3F, 0x3F, 0x3F,
                    0x3F, 0x3F, 0x3F, 0x3F,
                    0x3F, 0x3F, 0x3F, 0x3F,
                    0x3F, 0x3F, 0x3F, 0x3F,        
                    0x3F, 0x3F, 0x3F, 0x3F,
                    0x3F, 0x3F, 0x3F, 0x3F,
                    0x3F, 0x3F, 0x3F, 0x3F,
        6, 0, 0xD0, 0x04, 0x06, 0x6B, 0x0F, 0x00,   // SET_GD
        3, 0, 0xD7, 0x00, 0x30,                     // RAMCTRL
        2, 0, 0xE6, 0x14,                           // ECO
        2, 0, 0xDE, 0x01,                           // page1
        //2, 0, 0xB2, 0x12,                         // option ??????????
        6, 0, 0xB7, 0x07, 0x13, 0x0F, 0x39, 0x2D,   // DCDC_OPT
        4, 0, 0xC1, 0x34, 0x15, 0xE0,               // SETRGB
        4, 0, 0xC2, 0x06, 0x3A, 0xC7,               // SETSTBA2
        3, 0, 0xC4, 0x7A, 0x1A,                     // SET_GAMMAOP
        //2, 0, 0xC5, 0x11,                          // OSC 60FPS
        // ~113 FPS on purpose: the CPU streams frames to us at 60 FPS, so the
        // panel refresh must be faster than the incoming frame rate to push
        // each received frame out to the display before the next one arrives.
        // Same as jd9853_refresh_seq_max, the dynamic refresh starts from it.
        2, 0, 0xC5, 0x01,                           // OSC ~113FPS
        // 2, 0, 0xBE, 0x00,                        // GAMMA_POWER_TEST
        2, 0, 0xDE, 0x02,                           // page2
        3, 0, 0xB5, 0x0A, 0x1C,                     //
        2, 0, 0xDE, 0x00,                           // page0
        2, 0, 0x35, 0x00,                           // Tearing Effect Line ON
        2, 0, 0x3A, 0x06,                           // Set colour mode to RGB666

        5, 0, 0x2A, JD9853_OFF_X0 >> 8, JD9853_OFF_X0 & 0xff,   // CASET: column addresses
            JD9853_OFF_X1 >> 8, JD9853_OFF_X1 & 0xff,
        5, 0, 0x2B, JD9853_OFF_Y0 >> 8, JD9853_OFF_Y0 & 0xff,   // RASET: row addresses
            JD9853_OFF_Y1 >> 8, JD9853_OFF_Y1 & 0xff,

        2, 0, 0x36, 0b11001000,                 // Set MADCTL: row then column, refresh is bottom to top ????
        1, 20, 0x11,                            // Exit sleep mode
        0,                                      // Terminate list
};

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
static const uint8_t jd9853_refresh_seq_10fps[] = {
        2, 0, 0xDE, 0x00,                                                           // page 0
        // VFP = VBP = 127: 144 + 254 = 398 lines
        13, 0, 0xC4, 0x00, 0x00, 0x48, 0x79, 0x7F, 0x7F, 0x16, 0x79, 0x0C, 0x0A, 0x16, 0x82,
        2, 0, 0xDE, 0x01,                                                           // page 1
        2, 0, 0xC5, 0x20,                                                           // OSCM 16 MHz, TCON /8: 2 MHz
        2, 0, 0xDE, 0x00,                                                           // page 0
        0,                                                                          // Terminate list
};

static const uint8_t jd9853_refresh_seq_15fps[] = {
        2, 0, 0xDE, 0x00,                                                           // page 0
        // VFP = VBP = 100: 144 + 200 = 344 lines
        13, 0, 0xC4, 0x00, 0x00, 0x48, 0x79, 0x64, 0x64, 0x16, 0x79, 0x0C, 0x0A, 0x16, 0x82,
        2, 0, 0xDE, 0x01,                                                           // page 1
        2, 0, 0xC5, 0x21,                                                           // OSCM 20 MHz, TCON /8: 2.5 MHz
        2, 0, 0xDE, 0x00,                                                           // page 0
        0,                                                                          // Terminate list
};

static const uint8_t jd9853_refresh_seq_30fps[] = {
        2, 0, 0xDE, 0x00,                                                           // page 0
        13, 0, 0xC4, 0x00, 0x00, 0x48, 0x79, 0x0A, 0x12, 0x16, 0x79, 0x0C, 0x0A, 0x16, 0x82,
        2, 0, 0xDE, 0x01,                                                           // page 1
        2, 0, 0xC5, 0x21,                                                           // OSCM 20 MHz, TCON /8: 2.5 MHz
        2, 0, 0xDE, 0x00,                                                           // page 0
        0,                                                                          // Terminate list
};

static const uint8_t jd9853_refresh_seq_60fps[] = {
        2, 0, 0xDE, 0x00,                                                           // page 0
        13, 0, 0xC4, 0x00, 0x00, 0x48, 0x79, 0x0A, 0x12, 0x16, 0x79, 0x0C, 0x0A, 0x16, 0x82,
        2, 0, 0xDE, 0x01,                                                           // page 1
        2, 0, 0xC5, 0x11,                                                           // OSCM 20 MHz, TCON /4: 5 MHz
        2, 0, 0xDE, 0x00,                                                           // page 0
        0,                                                                          // Terminate list
};

static const uint8_t jd9853_refresh_seq_max[] = {
        2, 0, 0xDE, 0x00,                                                           // page 0
        13, 0, 0xC4, 0x00, 0x00, 0x48, 0x79, 0x14, 0x12, 0x16, 0x79, 0x0C, 0x0A, 0x16, 0x82,
        2, 0, 0xDE, 0x01,                                                           // page 1
        2, 0, 0xC5, 0x01,                                                           // OSCM 20 MHz, TCON /2: 10 MHz
        2, 0, 0xDE, 0x00,                                                           // page 0
        0,                                                                          // Terminate list
};

typedef enum {
    DisplayJd9853RefreshNone = -1,
    DisplayJd9853Refresh10 = 0,
    DisplayJd9853Refresh15,
    DisplayJd9853Refresh30,
    DisplayJd9853Refresh60,
    DisplayJd9853RefreshMax,
    DisplayJd9853RefreshCount,
} DisplayJd9853RefreshStep;

typedef struct {
    const uint8_t* config;
    uint8_t fps;
    uint32_t fast_delta_us;
} DisplayJd9853RefreshConfig;

/* fast_delta_us is 5/6 of the panel frame period, precomputed:
 * 10 FPS -> 83333 us, 15 -> 55555, 30 -> 27777, 60 -> 13888, 115 -> 7245. */
static const DisplayJd9853RefreshConfig
    jd9853_refresh_config[DisplayJd9853RefreshCount] = {
        [DisplayJd9853Refresh10] = {jd9853_refresh_seq_10fps, 10, 83333}, // ~10.4 FPS, low power
        [DisplayJd9853Refresh15] = {jd9853_refresh_seq_15fps, 15, 55555}, // ~15.0 FPS
        [DisplayJd9853Refresh30] = {jd9853_refresh_seq_30fps, 30, 27777}, // ~30.0 FPS
        [DisplayJd9853Refresh60] = {jd9853_refresh_seq_60fps, 60, 13888}, // ~60.0 FPS
        [DisplayJd9853RefreshMax] = {jd9853_refresh_seq_max, 115, 7245}, // ~113 FPS
};
/* clang-format on */
