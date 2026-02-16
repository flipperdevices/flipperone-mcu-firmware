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
    rdid1 = 0xDA, /* Read ID1 Value */
    rdid2 = 0xDB, /* Read ID2 Value */
    rdid3 = 0xDC, /* Read ID3 Value */
} DisplayJd9853Reg;

static const uint8_t jd9853_deinit_seq[] = {
        1, 20, 0x28,                        // Display off
        1, 20, 0x10,                        // Enter sleep mode
        0                                   // Terminate list
};

static const uint8_t jd9853_init_seq_2025_04_01_normal_white[] = {
        1, 20, 0x01,                                // Software reset
        3, 0, 0xDF, 0x98, 0x53,                     // SET_PASSWD
        2, 0, 0xDE, 0x00,                           // Page0
        2, 0, 0xB2, 0x64,                           // Vcom
        // GAMMA_SET
        5, 0, 0xB7, 0x00,                           // VGSP_S
                    0x7D,                           // VGMP_S //4.6
                    0x00,                           // VGSN_S
                    0x7D,                           // VGPN_S //-4.6
        2, 0, 0xB9, 0b01110101,                     // AP
        // Voltage settings
        7, 0, 0xBB, 0b01110111,                     // VGHZ_S =11V VGLZ_S =-9.25V
                    0b10111001,                     // VGHZ_RT  AVDDZ_S  VGLZ_RT  AVEEZ_S    
                    0x55,                           // VGHZ_CLK  VGLZ_CLK
                    0x61,                           // MVZ_S_CLK  MVZ_G_CLK
                    0x7F,                           // MVZ_NS_CLK  HV_CLKP  MV_CLKP
                    0xFE,                           // VGHZ_CLKE  CGLZ_CLKE  MVZ_S_CLKE  MVZ_NS_CLKE
        3, 0, 0xC0, 0b11111111, 0b11111111,         // SETSTBA
        2, 0, 0xC1, 0x16,                           // SETPANEL
        9, 0, 0xC3, 0x7E, 0x07, 0x16, 0x0B,         // SETRGBCYC
                    0xCB, 0x71, 0x72, 0x7F, 
        13, 0, 0xC4, 0x00, 0x00, 0x48, 0x79,        // SET_TCON
                    0x14, 0x12, 0x16, 0x79,
                    0x0C, 0x0A, 0x16, 0x82,
        33, 0, 0xC8, 0xFF, 0xFF, 0xFF, 0xFF,        // SET_R_GAMMA
                    0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF,        
                    0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF,
        6, 0, 0xD0, 0x04, 0x06, 0x6B, 0x0F, 0x00,   // SET_GD
        3, 0, 0xD7, 0x00, 0x30,                     // RAMCTRL
        2, 0, 0xE6, 0x14,                           // ECO
        2, 0, 0xDE, 0x01,                           // page1
        2, 0, 0xB2, 0x12,                           // option
        6, 0, 0xB7, 0x07, 0x13, 0x0F, 0x39, 0x2D,   // DCDC_OPT
        4, 0, 0xC1, 0x34, 0x15, 0xE0,               // SETRGB
        4, 0, 0xC2, 0x06, 0x3A, 0xC7,               // SETSTBA2
        3, 0, 0xC4, 0x7A, 0x1A,                     // SET_GAMMAOP
        2, 0, 0xC5, 0x11,                           // OSC 60FPS
        // 2, 0, 0xBE, 0x00,                        // GAMMA_POWER_TEST
        2, 0, 0xDE, 0x02,                           // page2
        3, 0, 0xB5, 0x0A, 0x1C,                     //
        2, 0, 0xDE, 0x00,                           // page0
        2, 0, 0x35, 0x00,                           // Tearing Effect Line ON
        2, 0, 0x3A, 0x06,                           // Set colour mode to RGB666

        5, 0, 0x2A, JD9853_OFF_X0 >> 8, JD9853_OFF_X0 & 0xff,       // CASET: column addresses
            JD9853_OFF_X1 >> 8, JD9853_OFF_X1 & 0xff,
        5, 0, 0x2B, JD9853_OFF_Y0 >> 8, JD9853_OFF_Y0 & 0xff,       // RASET: row addresses
            JD9853_OFF_Y1 >> 8, JD9853_OFF_Y1 & 0xff,

        2, 0, 0x36, 0b11001000,                 // Set MADCTL: row then column, refresh is bottom to top ????
        1, 20, 0x11,                            // Exit sleep mode
        1, 2, 0x29,                             // Main screen turn on, then wait 500 ms
        0,                                      // Terminate list
};

static const uint8_t jd9853_init_seq_2025_04_01_normal_white_no_reset[] = {
       //1, 20, 0x01,                                // Software reset
        3, 0, 0xDF, 0x98, 0x53,                     // SET_PASSWD
        2, 0, 0xDE, 0x00,                           // Page0
        2, 0, 0xB2, 0x64,                           // Vcom
        // GAMMA_SET
        5, 0, 0xB7, 0x00,                           // VGSP_S
                    0x7D,                           // VGMP_S //4.6
                    0x00,                           // VGSN_S
                    0x7D,                           // VGPN_S //-4.6
        2, 0, 0xB9, 0b01110101,                     // AP
        // Voltage settings
        7, 0, 0xBB, 0b01110111,                     // VGHZ_S =11V VGLZ_S =-9.25V
                    0b10111001,                     // VGHZ_RT  AVDDZ_S  VGLZ_RT  AVEEZ_S    
                    0x55,                           // VGHZ_CLK  VGLZ_CLK
                    0x61,                           // MVZ_S_CLK  MVZ_G_CLK
                    0x7F,                           // MVZ_NS_CLK  HV_CLKP  MV_CLKP
                    0xFE,                           // VGHZ_CLKE  CGLZ_CLKE  MVZ_S_CLKE  MVZ_NS_CLKE
        3, 0, 0xC0, 0b11111111, 0b11111111,         // SETSTBA
        2, 0, 0xC1, 0x16,                           // SETPANEL
        9, 0, 0xC3, 0x7E, 0x07, 0x16, 0x0B,         // SETRGBCYC
                    0xCB, 0x71, 0x72, 0x7F, 
        13, 0, 0xC4, 0x00, 0x00, 0x48, 0x79,        // SET_TCON
                    0x14, 0x12, 0x16, 0x79,
                    0x0C, 0x0A, 0x16, 0x82,
        33, 0, 0xC8, 0xFF, 0xFF, 0xFF, 0xFF,        // SET_R_GAMMA
                    0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF,        
                    0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF,
                    0xFF, 0xFF, 0xFF, 0xFF,
        6, 0, 0xD0, 0x04, 0x06, 0x6B, 0x0F, 0x00,   // SET_GD
        3, 0, 0xD7, 0x00, 0x30,                     // RAMCTRL
        2, 0, 0xE6, 0x14,                           // ECO
        2, 0, 0xDE, 0x01,                           // page1
        2, 0, 0xB2, 0x12,                           // option
        6, 0, 0xB7, 0x07, 0x13, 0x0F, 0x39, 0x2D,   // DCDC_OPT
        4, 0, 0xC1, 0x34, 0x15, 0xE0,               // SETRGB
        4, 0, 0xC2, 0x06, 0x3A, 0xC7,               // SETSTBA2
        3, 0, 0xC4, 0x7A, 0x1A,                     // SET_GAMMAOP
        2, 0, 0xC5, 0x11,                           // OSC 60FPS
        // 2, 0, 0xBE, 0x00,                           // GAMMA_POWER_TEST
        2, 0, 0xDE, 0x02,                           // page2
        3, 0, 0xB5, 0x0A, 0x1C,                     //
        2, 0, 0xDE, 0x00,                           // page0
        2, 0, 0x35, 0x00,                           // Tearing Effect Line ON
        2, 0, 0x3A, 0x06,                           // Set colour mode to RGB666

        5, 0, 0x2A, JD9853_OFF_X0 >> 8, JD9853_OFF_X0 & 0xff,       // CASET: column addresses
            JD9853_OFF_X1 >> 8, JD9853_OFF_X1 & 0xff,
        5, 0, 0x2B, JD9853_OFF_Y0 >> 8, JD9853_OFF_Y0 & 0xff,       // RASET: row addresses
            JD9853_OFF_Y1 >> 8, JD9853_OFF_Y1 & 0xff,

        2, 0, 0x36, 0b11001000,                 // Set MADCTL: row then column, refresh is bottom to top ????
        //1, 20, 0x11,                            // Exit sleep mode
        //1, 2, 0x29,                             // Main screen turn on, then wait 500 ms
        0,                                      // Terminate list
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
        //2, 0, 0xB2, 0x12,                           // option ??????????
        6, 0, 0xB7, 0x07, 0x13, 0x0F, 0x39, 0x2D,   // DCDC_OPT
        4, 0, 0xC1, 0x34, 0x15, 0xE0,               // SETRGB
        4, 0, 0xC2, 0x06, 0x3A, 0xC7,               // SETSTBA2
        3, 0, 0xC4, 0x7A, 0x1A,                     // SET_GAMMAOP
        2, 0, 0xC5, 0x11,                           // OSC 60FPS
        // 2, 0, 0xBE, 0x00,                           // GAMMA_POWER_TEST
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
        1, 2, 0x29,                             // Main screen turn on, then wait 500 ms
        0,                                      // Terminate list
};

static const uint8_t jd9853_init_seq_2025_04_01_normal_white_mod_no_reset[] = {
        // 1, 20, 0x01,                                // Software reset
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
        //2, 0, 0xB2, 0x12,                           // option ??????????
        6, 0, 0xB7, 0x07, 0x13, 0x0F, 0x39, 0x2D,   // DCDC_OPT
        4, 0, 0xC1, 0x34, 0x15, 0xE0,               // SETRGB
        4, 0, 0xC2, 0x06, 0x3A, 0xC7,               // SETSTBA2
        3, 0, 0xC4, 0x7A, 0x1A,                     // SET_GAMMAOP
        2, 0, 0xC5, 0x11,                           // OSC 60FPS
        // 2, 0, 0xBE, 0x00,                           // GAMMA_POWER_TEST
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
        // 1, 20, 0x11,                            // Exit sleep mode
        // 1, 2, 0x29,                             // Main screen turn on, then wait 500 ms
        0,                                      // Terminate list
};

static const uint8_t jd9853_init_seq_2025_04_01_normal_black[] = {
        1, 20, 0x01,                                // Software reset
        3, 0, 0xDF, 0x98, 0x53,                     // SET_PASSWD
        2, 0, 0xDE, 0x00,                           // Page0
        2, 0, 0xB2, 0x0C,                           // Vcom
        // GAMMA_SET
        5, 0, 0xB7, 0x00,                           // VGSP_S
                    0x25,                           //4.6
                    0x00,                           // VGSN_S
                    0x4D,                           //4.6
        2, 0, 0xB9, 0x35,                           // AP
        // Voltage settings
        7, 0, 0xBB, 0x36,                           // VGHZ_S =11V VGLZ_S =-9.25V
                    0xB9,                           // VGHZ_RT  AVDDZ_S  VGLZ_RT  AVEEZ_S    
                    0x55,                           // VGHZ_CLK  VGLZ_CLK
                    0x61,                           // MVZ_S_CLK  MVZ_G_CLK
                    0x7F,                           // MVZ_NS_CLK  HV_CLKP  MV_CLKP
                    0xFE,                           // VGHZ_CLKE  CGLZ_CLKE  MVZ_S_CLKE  MVZ_NS_CLKE
        3, 0, 0xC0, 0x19, 0x95,                     // SETSTBA
        2, 0, 0xC1, 0x12,                           // SETPANEL
        9, 0, 0xC3, 0x7E, 0x07, 0x16, 0x0B,         // SETRGBCYC
                    0xCB, 0x71, 0x72, 0x7F,
        13, 0, 0xC4, 0x00, 0x00, 0x48, 0x79,        // SET_TCON
                    0x14, 0x12, 0x16, 0x79,
                    0x0C, 0x0A, 0x16, 0x82,
        33, 0, 0xC8, 0x3F, 0x3A, 0x35, 0x35,        // SET_R_GAMMA
                    0x38, 0x3C, 0x37, 0x36,
                    0x33, 0x30, 0x2C, 0x1C,
                    0x17, 0x10, 0x09, 0x0C,
                    0x3F, 0x3A, 0x35, 0x35,
                    0x38, 0x3C, 0x37, 0x36,
                    0x33, 0x30, 0x2C, 0x1C,
                    0x17, 0x10, 0x09, 0x0C,
        6, 0, 0xD0, 0x04, 0x06, 0x6B, 0x0F, 0x00,   // SET_GD
        3, 0, 0xD7, 0x00, 0x30,                     // RAMCTRL
        2, 0, 0xE6, 0x14,                           // ECO
        2, 0, 0xDE, 0x01,                           // page1
        2, 0, 0xB2, 0x12,                           // option
        6, 0, 0xB7, 0x07, 0x13, 0x0F, 0x39, 0x2D,   // DCDC_OPT
        4, 0, 0xC1, 0x34, 0x15, 0xE0,               // SETRGB
        4, 0, 0xC2, 0x06, 0x3A, 0xC7,               // SETSTBA2
        3, 0, 0xC4, 0x7A, 0x1A,                     // SET_GAMMAOP
        2, 0, 0xC5, 0x11,                           // OSC 60FPS
        // 2, 0, 0xBE, 0x00,                           // GAMMA_POWER_TEST
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
        1, 2, 0x29,                             // Main screen turn on
        0,                                      // Terminate list

        
};
/* clang-format on */
