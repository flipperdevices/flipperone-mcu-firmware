#pragma once
//https://github.com/flipperdevices/one-rp2350-control/blob/main/docs/GWT-2.39-256144-AWMN-04-FS-1.1.pdf
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

#define SCREEN_WIDTH 240
#define SCREEN_HEIGHT 240
static const uint8_t st7789_init_seq[] = {
        1, 20, 0x01,                        // Software reset
        1, 10, 0x11,                        // Exit sleep mode
        2, 2, 0x3a, 0x55,                   // Set colour mode to 16 bit
        2, 0, 0x36, 0x00,                   // Set MADCTL: row then column, refresh is bottom to top ????
        5, 0, 0x2a, 0x00, 0x00,             // CASET: column addresses
            SCREEN_WIDTH >> 8, SCREEN_WIDTH & 0xff,
        5, 0, 0x2b, 0x00, 0x00,             // RASET: row addresses
            SCREEN_HEIGHT >> 8, SCREEN_HEIGHT & 0xff,
        1, 2, 0x21,                         // Inversion on, then 10 ms delay (supposedly a hack?)
        1, 2, 0x13,                         // Normal display on, then 10 ms delay
        1, 2, 0x29,                         // Main screen turn on, then wait 500 ms
        0                                   // Terminate list
};

static const uint8_t st7789_deinit_seq[] = {
        1, 20, 0x28,                        // Display off
        1, 20, 0x10,                        // Enter sleep mode
        0                                   // Terminate list
};