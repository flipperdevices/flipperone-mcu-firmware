#pragma once

#include "hardware/spi.h"
#include "pwm.hpp"
#include "log.hpp"

template <
    uint32_t pin_backlight,
    uint32_t pin_reset,
    uint32_t pin_cs,
    uint32_t pin_clock,
    uint32_t pin_data,
    uint32_t pin_dc,
    size_t offset_x,
    size_t offset_y,
    size_t width,
    size_t height>
class Display {
public:
    Display() {
    }

    void backlight(float value) {
        backlight_pwm.pwm(value);
    }

    void init(void) {
        uint freq = spi_init(spi, 1 * 1000 * 1000);
        Log::info("SPI%d initialized with frequency: %.2f MHz", SPI_NUM(spi), freq / 1000000.0f);
        spi_set_format(spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

        const enum gpio_slew_rate spi_slew_rate = GPIO_SLEW_RATE_FAST;
        const enum gpio_drive_strength spi_drive_strength = GPIO_DRIVE_STRENGTH_12MA;

        gpio_set_function(pin_clock, GPIO_FUNC_SPI);
        gpio_set_function(pin_data, GPIO_FUNC_SPI);

        gpio_set_drive_strength(pin_clock, spi_drive_strength);
        gpio_set_slew_rate(pin_clock, spi_slew_rate);

        gpio_set_drive_strength(pin_data, spi_drive_strength);
        gpio_set_slew_rate(pin_data, spi_slew_rate);

        gpio_init(pin_cs);
        gpio_set_dir(pin_cs, GPIO_OUT);
        gpio_set_drive_strength(pin_cs, spi_drive_strength);
        gpio_set_slew_rate(pin_cs, spi_slew_rate);

        gpio_init(pin_dc);
        gpio_set_dir(pin_dc, GPIO_OUT);
        gpio_set_drive_strength(pin_dc, spi_drive_strength);
        gpio_set_slew_rate(pin_dc, spi_slew_rate);

        gpio_init(pin_reset);
        gpio_set_dir(pin_reset, GPIO_OUT);
        gpio_set_drive_strength(pin_reset, spi_drive_strength);
        gpio_set_slew_rate(pin_reset, spi_slew_rate);

        cs(true);
        dc(false);

        // reset sequence
        reset(false);
        sleep_ms(20);
        reset(false);
        sleep_ms(10);
        reset(true);
        sleep_ms(10);

        write_command(CMD::SWRESET); // Software reset
        sleep_ms(10);

        // write_command(CMD::SLPOUT); // Sleep out
        // sleep_ms(120);

        // st7789s_initialize_bad_gamma();
        // st7789s_initialize_2025_01_16();
        // st7789s_initialize_2025_04_01();
        st7789s_initialize_2025_04_01_edited();

        freq = spi_set_baudrate(spi, 75 * 1000 * 1000);
        Log::info("SPI%d initialized with frequency: %.2f MHz", SPI_NUM(spi), freq / 1000000.0f);

        initialized = true;
    }

    void write_buffer(const uint8_t* buffer) {
        write_buffer(width, height, buffer, width * height);
    }

    void write_buffer(uint16_t w, uint16_t h, const uint8_t* buffer, size_t size) {
        set_window(offset_x, offset_y, offset_x + (w / 3) - 1, offset_y + h - 1);
        write_command(CMD::RAMWR);

        cs(false);
        dc(true);
        spi_write_blocking(spi, buffer, size);
        cs(true);
    }

    void fill(uint8_t color) {
        uint8_t data[width];
        for(size_t i = 0; i < width; i++) {
            data[i] = color;
        }

        set_window(offset_x, offset_y, offset_x + (width / 3) - 1, offset_y + height - 1);
        write_command(CMD::RAMWR);

        cs(false);
        dc(true);

        for(size_t j = 0; j < height; j++) {
            spi_write_blocking(spi, data, width);
        }

        cs(true);
    }

    void eco_mode(bool enable) {
        if(enable) {
            write_command(CMD::IDMON);
        } else {
            write_command(CMD::IDMOFF);
        }
    }

private:
    PWMOutput<pin_backlight, 8, 200, false> backlight_pwm;
    spi_inst_t* spi = spi0;
    bool initialized = false;
    bool init_log_enabled = false;

    inline void reset(bool level) {
        gpio_put(pin_reset, level);
    }

    inline void cs(bool level) {
        gpio_put(pin_cs, level);
    }

    inline void dc(bool level) {
        gpio_put(pin_dc, level);
    }

    enum class CMD : uint8_t {
        NOP = 0x00,
        SWRESET = 0x01, /* Software Reset */
        RDDID = 0x04, /* Read Display ID */
        RDDST = 0x09, /* Read Display Status */
        RDDPM = 0x0A, /* Read Display Power Mode */
        RDDMADCTL = 0x0B, /* Read Display MADCTL */
        RDDCOLMOD = 0x0C, /* Read Display Pixel Format */
        RDDIM = 0x0D, /* Read Display Image Mode */
        RDDSM = 0x0E, /* Read Display Signal Mode */
        RDDSDR = 0x0F, /* Read Display Self-Diagnostic Result */
        SLPIN = 0x10, /* Sleep In */
        SLPOUT = 0x11, /* Sleep Out */
        PTLON = 0x12, /* Partial Display Mode On */
        NORON = 0x13, /* Normal Display Mode On */
        INVOFF = 0x20, /* Display Inversion Off */
        INVON = 0x21, /* Display Inversion On */
        GAMSET = 0x26, /* Gamma Set */
        DISPOFF = 0x28, /* Display Off */
        DISPON = 0x29, /* Display On */
        CASET = 0x2A, /* Column Address Set */
        RASET = 0x2B, /* Row Address Set */
        RAMWR = 0x2C, /* Memory Write */
        RAMRD = 0x2E, /* Memory Read */
        PTLAR = 0x30, /* Partial Area */
        VSCRDEF = 0x33, /* Vertical Scrolling Definition */
        TEOFF = 0x34, /* Tearing Effect Line OFF */
        TEON = 0x35, /* Tearing Effect Line ON */
        MADCTL = 0x36, /* Memory Data Access Control */
        VSCSAD = 0x37, /* Vertical Scroll Start Address of RAM */
        IDMOFF = 0x38, /* Idle Mode Off */
        IDMON = 0x39, /* Idle Mode On */
        COLMOD = 0x3A, /* Interface Pixel Format */
        WRMEMC = 0x3C, /* Write Memory Continue */
        RDMEMC = 0x3E, /* Read Memory Continue */
        STE = 0x44, /* Set Tear Scanline */
        GSCAN = 0x45, /* Get Scanline */
        WRDISBV = 0x51, /* Write Display Brightness Value */
        RDDISBV = 0x52, /* Read Display Brightness Value */
        WRCTRLD = 0x53, /* Write Control Display */
        RDCTRLD = 0x54, /* Read Control Value Display */
        WRCACE = 0x55, /* Write Content Adaptive Brightness Control and Color Enhancement */
        RDCABC = 0x56, /* Read Content Adaptive Brightness Control */
        WRCABCMB = 0x5E, /* Write CABC Minimum Brightness */
        RDCABCMB = 0x5F, /* Read CABC Minimum Brightness */
        RDABCSDR = 0x68, /* Read Adaptive Brightness Control and Self-Diagnostic Result */
        RDID1 = 0xDA, /* Read ID1 Value */
        RDID2 = 0xDB, /* Read ID2 Value */
        RDID3 = 0xDC, /* Read ID3 Value */
        RAMCTRL = 0xB0, /* RAM Control */
        RGBCTRL = 0xB1, /* RGB Interface Control */
        PORCTRL = 0xB2, /* Porch Setting */
        FRCTRL1 = 0xB3, /* Frame Rate Control in partial mode/idle colors */
        PARCTRL = 0xB5, /* Partial Mode Control */
        PWRSAVCTRL = 0xB6, /* Power Saving Control */
        GCTRL = 0xB7, /* Gate Control */
        GTADJ = 0xB8, /* Gate On Timing Adjustment */
        DGMEN = 0xBA, /* Digital Gamma Enable */
        VCOMS = 0xBB, /* VCOMS Setting */
        LCMCTRL = 0xC0, /* LCM Control */
        IDSET = 0xC1, /* ID Setting */
        VRHEN = 0xC2, /* VRH Command Enable */
        VRHS = 0xC3, /* VRH Set */
        VCMOFSET = 0xC5, /* VCOMS Offset Set */
        FRCTRL2 = 0xC6, /* Frame Rate Control in normal mode */
        CABCCTRL = 0xC7, /* CABC Control */
        REGSEL1 = 0xC8, /* Register Value Selection 1 */
        REGSEL2 = 0xCA, /* Register Value Selection 2 */
        PWMFRSEL = 0xCC, /* PWM Frequency Selection */
        PWCTRL1 = 0xD0, /* Power Control 1 */
        VAPVANEN = 0xD2, /* Enable VAP/VAN signal output */
        GATESEL = 0xD6, /* Gate Output Selection in Sleep In Mode */
        CMD2EN = 0xDF, /* Command 2 Enable */
        PVGAMCTRL = 0xE0, /* Positive Voltage Gamma Control */
        NVGAMCTRL = 0xE1, /* Negative Voltage Gamma Control */
        DGMLUTR = 0xE2, /* Digital Gamma Look-up Table for Red */
        DGMLUTB = 0xE3, /* Digital Gamma Look-up Table for Blue */
        GATECTRL = 0xE4, /* Gate Control */
        SPI2EN = 0xE7, /* SPI2 Enable */
        PWCTRL2 = 0xE8, /* Power Control 2 */
        EQCTRL = 0xE9, /* Equalize time control */
        PROMCTRL = 0xEC, /* Program Mode Control*/
        PROMEN = 0xFA, /* Program Mode Enable*/
        NVMSET = 0xFC, /* NVM Setting */
        PROMACT = 0xFE, /* Program action */
    };

    inline void write_command(CMD command) {
        write_command(static_cast<uint8_t>(command));
    }

    inline void write_command(uint8_t command) {
        if(!initialized && init_log_enabled) Log::debug("CMD: 0x%02x", command);
        cs(false);
        dc(false);
        spi_write_blocking(spi, &command, 1);
        cs(true);
    }

    inline void write_data(uint8_t data) {
        if(!initialized && init_log_enabled) Log::debug("DAT: 0x%02x", data);
        cs(false);
        dc(true);
        spi_write_blocking(spi, &data, 1);
        cs(true);
    }

    inline void set_window(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1) {
        write_command(CMD::CASET); // Column address set
        write_data(x0 >> 8);
        write_data(x0 & 0xFF);
        write_data(x1 >> 8);
        write_data(x1 & 0xFF);

        write_command(CMD::RASET); // Row address set
        write_data(y0 >> 8);
        write_data(y0 & 0xFF);
        write_data(y1 >> 8);
        write_data(y1 & 0xFF);
    }

    void st7789s_initialize_bad_gamma(void) {
        // 10.2.4 INVCTR (B4h): Display Inversion Control
        write_command(CMD::INVCTR);
        write_data(0b00000111); // NLA NLB NLC all 1 - column inversion

        // 10.1.33 COLMOD (3Ah): Interface Pixel Format
        write_command(CMD::COLMOD);
        write_data(0b00000110); // IFPF: 0b110/18 bit per pixel

        write_command(CMD::INVCTR);
        write_data(0b00000111); // NLA NLB NLC all 1 - column inversion

        // 10.1.29 MADCTL (36h): Memory Data Access Control
        write_command(CMD::MADCTL);
        write_data(0b00000000);

        // Enable display
        // 10.1.14 NORON (13h): Normal Display Mode On
        write_command(CMD::NORON);

        // 10.1.19 DISPON (29h): Display On
        write_command(CMD::DISPON);
    }

#define SSD_CMD(cmd)  write_command(cmd)
#define SSD_PAR(data) write_data(data)
#define SSD_Start()
#define SSD_Stop()
#define Delayms(ms) sleep_ms(ms)

    void st7789s_initialize_2025_04_01_edited(void) {
        //SET_PASSWD
        SSD_CMD(0xDF);
        SSD_PAR(0x98);
        SSD_PAR(0x53);

        //Page0
        SSD_CMD(0xDE);
        SSD_PAR(0x00);

        //Vcom
        SSD_CMD(0xB2);
        SSD_PAR(0x0C);

        //GAMMA_SET
        SSD_CMD(0xB7);
        SSD_PAR(0x00);
        SSD_PAR(0x25); //4.6
        SSD_PAR(0x00);
        SSD_PAR(0x4D); //4.6

        //AP
        SSD_CMD(0xB9);
        SSD_PAR(0x35);

        SSD_CMD(0xBB);
        SSD_PAR(0x36); //VGHZ_S =11V VGLZ_S =-9.25V
        SSD_PAR(0xB9); //VGHZ_RT  AVDDZ_S  VGLZ_RT  AVEEZ_S
        SSD_PAR(0x55); //VGHZ_CLK  VGLZ_CLK
        SSD_PAR(0x61); //MVZ_S_CLK  MVZ_G_CLK
        SSD_PAR(0x7F); //MVZ_NS_CLK  HV_CLKP  MV_CLKP
        SSD_PAR(0xFE); //VGHZ_CLKE  CGLZ_CLKE  MVZ_S_CLKE  MVZ_NS_CLKE

        //SETSTBA
        SSD_CMD(0xC0);
        SSD_PAR(0x19);
        SSD_PAR(0x95);

        //SETPANEL
        SSD_CMD(0xC1);
        SSD_PAR(0x12);

        //SETRGBCYC
        SSD_CMD(0xC3);
        SSD_PAR(0x7E);
        SSD_PAR(0x07);
        SSD_PAR(0x16);
        SSD_PAR(0x0B);
        SSD_PAR(0xCB);
        SSD_PAR(0x71);
        SSD_PAR(0x72);
        SSD_PAR(0x7F);

        //SET_TCON
        SSD_CMD(0xC4);
        SSD_PAR(0x00);
        SSD_PAR(0x00);
        SSD_PAR(0x48);
        SSD_PAR(0x79);
        SSD_PAR(0x14);
        SSD_PAR(0x12);
        SSD_PAR(0x16);
        SSD_PAR(0x79);
        SSD_PAR(0x0C);
        SSD_PAR(0x0A);
        SSD_PAR(0x16);
        SSD_PAR(0x82);

        //SET_R_GAMMA
        SSD_CMD(0xC8);
        SSD_PAR(0x3F);
        SSD_PAR(0x3A);
        SSD_PAR(0x35);
        SSD_PAR(0x35);
        SSD_PAR(0x38);
        SSD_PAR(0x3C);
        SSD_PAR(0x37);
        SSD_PAR(0x36);
        SSD_PAR(0x33);
        SSD_PAR(0x30);
        SSD_PAR(0x2C);
        SSD_PAR(0x1C);
        SSD_PAR(0x17);
        SSD_PAR(0x10);
        SSD_PAR(0x09);
        SSD_PAR(0x0C);
        SSD_PAR(0x3F);
        SSD_PAR(0x3A);
        SSD_PAR(0x35);
        SSD_PAR(0x35);
        SSD_PAR(0x38);
        SSD_PAR(0x3C);
        SSD_PAR(0x37);
        SSD_PAR(0x36);
        SSD_PAR(0x33);
        SSD_PAR(0x30);
        SSD_PAR(0x2C);
        SSD_PAR(0x1C);
        SSD_PAR(0x17);
        SSD_PAR(0x10);
        SSD_PAR(0x09);
        SSD_PAR(0x0C);

        //SET_GD
        SSD_CMD(0xD0); //AUTO EXCEL GEN
        SSD_PAR(0x04);
        SSD_PAR(0x06);
        SSD_PAR(0x6B);
        SSD_PAR(0x0F);
        SSD_PAR(0x00);

        //RAMCTRL
        SSD_CMD(0xD7);
        SSD_PAR(0x00);
        SSD_PAR(0x30);

        //ECO
        SSD_CMD(0xE6);
        SSD_PAR(0x14);

        //page1
        SSD_CMD(0xDE);
        SSD_PAR(0x01);

        //option
        SSD_CMD(0xB2);
        SSD_PAR(0x12);

        //DCDC_OPT
        SSD_CMD(0xB7);
        SSD_PAR(0x07);
        SSD_PAR(0x13);
        SSD_PAR(0x0F);
        SSD_PAR(0x39);
        SSD_PAR(0x2D);

        //SETRGB
        SSD_CMD(0xC1);
        SSD_PAR(0x34);
        SSD_PAR(0x15);
        SSD_PAR(0xE0);

        //SETSTBA2
        SSD_CMD(0xC2);
        SSD_PAR(0x06);
        SSD_PAR(0x3A);
        SSD_PAR(0xC7);

        //SET_GAMMAOP
        SSD_CMD(0xC4);
        SSD_PAR(0x7A);
        SSD_PAR(0x1A);

        //OSC
        SSD_CMD(0xC5);
        SSD_PAR(0x01);

        // This fucks up the display (fast vertical lines)
        // //GAMMA_POWER_TEST
        // SSD_CMD(0xBE);
        // SSD_PAR(0x00);

        //page2
        SSD_CMD(0xDE);
        SSD_PAR(0x02);

        //
        SSD_CMD(0xB5);
        SSD_PAR(0x0A);
        SSD_PAR(0x1C);

        //page0
        SSD_CMD(0xDE);
        SSD_PAR(0x00);

        SSD_CMD(0x35);
        SSD_PAR(0x00);

        SSD_CMD(0x3A);
        SSD_PAR(0x06); //0x06=RGB666  0x05=RGB565

        // SSD_CMD(0x2A);
        // SSD_PAR(0x00);
        // SSD_PAR(0x4D); //Start
        // SSD_PAR(0x00);
        // SSD_PAR(0xA2); //End

        // SSD_CMD(0x2B);
        // SSD_PAR(0x00);
        // SSD_PAR(0x00); //Start
        // SSD_PAR(0x00);
        // SSD_PAR(0x8F); //End

        SSD_CMD(CMD::MADCTL);
        SSD_PAR(0b11001000); // RGB mode, no rotation

        SSD_CMD(0x11);
        Delayms(120);

        SSD_CMD(0x29);
        // Delayms(50);
    }

    void st7789s_initialize_2025_04_01(void) {
        //SET_PASSWD
        SSD_CMD(0xDF);
        SSD_PAR(0x98);
        SSD_PAR(0x53);

        //Page0
        SSD_CMD(0xDE);
        SSD_PAR(0x00);

        //Vcom
        SSD_CMD(0xB2);
        SSD_PAR(0x0C);

        //GAMMA_SET
        SSD_CMD(0xB7);
        SSD_PAR(0x00);
        SSD_PAR(0x25); //4.6
        SSD_PAR(0x00);
        SSD_PAR(0x4D); //4.6

        //AP
        SSD_CMD(0xB9);
        SSD_PAR(0x35);

        SSD_CMD(0xBB);
        SSD_PAR(0x36); //VGHZ_S =11V VGLZ_S =-9.25V
        SSD_PAR(0xB9); //VGHZ_RT  AVDDZ_S  VGLZ_RT  AVEEZ_S
        SSD_PAR(0x55); //VGHZ_CLK  VGLZ_CLK
        SSD_PAR(0x61); //MVZ_S_CLK  MVZ_G_CLK
        SSD_PAR(0x7F); //MVZ_NS_CLK  HV_CLKP  MV_CLKP
        SSD_PAR(0xFE); //VGHZ_CLKE  CGLZ_CLKE  MVZ_S_CLKE  MVZ_NS_CLKE

        //SETSTBA
        SSD_CMD(0xC0);
        SSD_PAR(0x19);
        SSD_PAR(0x95);

        //SETPANEL
        SSD_CMD(0xC1);
        SSD_PAR(0x12);

        //SETRGBCYC
        SSD_CMD(0xC3);
        SSD_PAR(0x7E);
        SSD_PAR(0x07);
        SSD_PAR(0x16);
        SSD_PAR(0x0B);
        SSD_PAR(0xCB);
        SSD_PAR(0x71);
        SSD_PAR(0x72);
        SSD_PAR(0x7F);

        //SET_TCON
        SSD_CMD(0xC4);
        SSD_PAR(0x00);
        SSD_PAR(0x00);
        SSD_PAR(0x48);
        SSD_PAR(0x79);
        SSD_PAR(0x14);
        SSD_PAR(0x12);
        SSD_PAR(0x16);
        SSD_PAR(0x79);
        SSD_PAR(0x0C);
        SSD_PAR(0x0A);
        SSD_PAR(0x16);
        SSD_PAR(0x82);

        //SET_R_GAMMA
        SSD_CMD(0xC8);
        SSD_PAR(0x3F);
        SSD_PAR(0x3A);
        SSD_PAR(0x35);
        SSD_PAR(0x35);
        SSD_PAR(0x38);
        SSD_PAR(0x3C);
        SSD_PAR(0x37);
        SSD_PAR(0x36);
        SSD_PAR(0x33);
        SSD_PAR(0x30);
        SSD_PAR(0x2C);
        SSD_PAR(0x1C);
        SSD_PAR(0x17);
        SSD_PAR(0x10);
        SSD_PAR(0x09);
        SSD_PAR(0x0C);
        SSD_PAR(0x3F);
        SSD_PAR(0x3A);
        SSD_PAR(0x35);
        SSD_PAR(0x35);
        SSD_PAR(0x38);
        SSD_PAR(0x3C);
        SSD_PAR(0x37);
        SSD_PAR(0x36);
        SSD_PAR(0x33);
        SSD_PAR(0x30);
        SSD_PAR(0x2C);
        SSD_PAR(0x1C);
        SSD_PAR(0x17);
        SSD_PAR(0x10);
        SSD_PAR(0x09);
        SSD_PAR(0x0C);

        //SET_GD
        SSD_CMD(0xD0); //AUTO EXCEL GEN
        SSD_PAR(0x04);
        SSD_PAR(0x06);
        SSD_PAR(0x6B);
        SSD_PAR(0x0F);
        SSD_PAR(0x00);

        //RAMCTRL
        SSD_CMD(0xD7);
        SSD_PAR(0x00);
        SSD_PAR(0x30);

        //ECO
        SSD_CMD(0xE6);
        SSD_PAR(0x14);

        //page1
        SSD_CMD(0xDE);
        SSD_PAR(0x01);

        //option
        SSD_CMD(0xB2);
        SSD_PAR(0x12);

        //DCDC_OPT
        SSD_CMD(0xB7);
        SSD_PAR(0x07);
        SSD_PAR(0x13);
        SSD_PAR(0x0F);
        SSD_PAR(0x39);
        SSD_PAR(0x2D);

        //SETRGB
        SSD_CMD(0xC1);
        SSD_PAR(0x34);
        SSD_PAR(0x15);
        SSD_PAR(0xE0);

        //SETSTBA2
        SSD_CMD(0xC2);
        SSD_PAR(0x06);
        SSD_PAR(0x3A);
        SSD_PAR(0xC7);

        //SET_GAMMAOP
        SSD_CMD(0xC4);
        SSD_PAR(0x7A);
        SSD_PAR(0x1A);

        //OSC
        SSD_CMD(0xC5);
        SSD_PAR(0x01);

        //GAMMA_POWER_TEST
        SSD_CMD(0xBE);
        SSD_PAR(0x00);

        //page2
        SSD_CMD(0xDE);
        SSD_PAR(0x02);

        //
        SSD_CMD(0xB5);
        SSD_PAR(0x0A);
        SSD_PAR(0x1C);

        //page0
        SSD_CMD(0xDE);
        SSD_PAR(0x00);

        SSD_CMD(0x35);
        SSD_PAR(0x00);

        SSD_CMD(0x3A);
        SSD_PAR(0x06); //0x06=RGB666  0x05=RGB565

        SSD_CMD(0x2A);
        SSD_PAR(0x00);
        SSD_PAR(0x4D); //Start
        SSD_PAR(0x00);
        SSD_PAR(0xA2); //End

        SSD_CMD(0x2B);
        SSD_PAR(0x00);
        SSD_PAR(0x00); //Start
        SSD_PAR(0x00);
        SSD_PAR(0x8F); //End

        SSD_CMD(0x11);
        Delayms(120);

        SSD_CMD(0x29);
        Delayms(50);
    }

    void st7789s_initialize_2025_01_16(void) {
        //SET_PASSWD
        SSD_Start();
        SSD_CMD(0xDF);
        SSD_PAR(0x98);
        SSD_PAR(0x53);
        SSD_Stop();

        //VGPOW_SET
        SSD_Start();
        SSD_CMD(0xB2);
        SSD_PAR(0x1A);
        SSD_Stop();

        //GAMMA_SET
        SSD_Start();
        SSD_CMD(0xB7); //5.5v
        SSD_PAR(0x00);
        SSD_PAR(0x51);
        SSD_PAR(0x00);
        SSD_PAR(0x79);
        SSD_Stop();

        //DCDC_SEL
        SSD_Start();
        SSD_CMD(0xBB);
        SSD_PAR(0x3E); //VGHZ_S =12V VGLZ_S =-11.5V
        SSD_PAR(0x2F); //
        SSD_PAR(0x55); //
        SSD_PAR(0x73); //
        SSD_PAR(0x63); //
        SSD_PAR(0xF0); //
        SSD_Stop();

        //SETSTBA
        SSD_Start();
        SSD_CMD(0xC0);
        SSD_PAR(0x44);
        SSD_PAR(0xA4);
        SSD_Stop();

        //SETPANEL
        SSD_Start();
        SSD_CMD(0xC1);
        SSD_PAR(0x12);
        SSD_Stop();

        //SETRGBCYC
        SSD_Start();
        SSD_CMD(0xC3);
        SSD_PAR(0x7D);
        SSD_PAR(0x07);
        SSD_PAR(0x04);
        SSD_PAR(0x06);
        SSD_PAR(0xC8);
        SSD_PAR(0x71);
        SSD_PAR(0x6C);
        SSD_PAR(0x77);
        SSD_Stop();

        //SET_TCON
        SSD_Start();
        SSD_CMD(0xC4);
        SSD_PAR(0x00); //00=60Hz 06=51Hz 08=40Hz
        SSD_PAR(0x00);
        SSD_PAR(0x4E); //
        SSD_PAR(0x4A); //
        SSD_PAR(0x0E);
        SSD_PAR(0x0A);
        SSD_PAR(0x16);
        SSD_PAR(0x79);
        SSD_PAR(0x35);
        SSD_PAR(0x0A);
        SSD_PAR(0x16);
        SSD_PAR(0x82);
        SSD_Stop();

        //SET_R_GAMMA 20241122 VOP4.3 G2.2
        SSD_Start();
        SSD_CMD(0xC8); //G2.5 G2.0
        SSD_PAR(0x3F); //0x3F 0x3F
        SSD_PAR(0x34); //0x32 0x27
        SSD_PAR(0x2C); //0x2A 0x1E
        SSD_PAR(0x2E); //0x23 0x18
        SSD_PAR(0x29); //0x27 0x1C
        SSD_PAR(0x2C); //0x29 0x1F
        SSD_PAR(0x28); //0x23 0x1A
        SSD_PAR(0x26); //0x23 0x1A
        SSD_PAR(0x25); //0x20 0x1A
        SSD_PAR(0x23); //0x1F 0x1A
        SSD_PAR(0x21); //0x1B 0x1A
        SSD_PAR(0x14); //0x0E 0x12
        SSD_PAR(0x10); //0x0A 0x0F
        SSD_PAR(0x0B); //0x04 0x0B
        SSD_PAR(0x05); //0x00 0x05
        SSD_PAR(0x00); //0x00 0x00
        SSD_PAR(0x3F); //0x3F 0x3F
        SSD_PAR(0x34); //0x32 0x27
        SSD_PAR(0x2C); //0x2A 0x1E
        SSD_PAR(0x2E); //0x23 0x18
        SSD_PAR(0x29); //0x27 0x1C
        SSD_PAR(0x2C); //0x29 0x1F
        SSD_PAR(0x28); //0x23 0x1A
        SSD_PAR(0x26); //0x23 0x1A
        SSD_PAR(0x25); //0x20 0x1A
        SSD_PAR(0x23); //0x1F 0x1A
        SSD_PAR(0x21); //0x1B 0x1A
        SSD_PAR(0x14); //0x0E 0x12
        SSD_PAR(0x10); //0x0A 0x0F
        SSD_PAR(0x0B); //0x04 0x0B
        SSD_PAR(0x05); //0x00 0x05
        SSD_PAR(0x00); //0x00 0x00
        SSD_Stop();

        //SET_GD
        SSD_Start();
        SSD_CMD(0xD0);
        SSD_PAR(0x04);
        SSD_PAR(0x06);
        SSD_PAR(0x6B);
        SSD_PAR(0x0F);
        SSD_PAR(0x00);
        SSD_Stop();

        //RAMCTRL
        SSD_Start();
        SSD_CMD(0xD7);
        SSD_PAR(0x00);
        SSD_PAR(0x30);
        SSD_Stop();

        //ECO
        SSD_Start();
        SSD_CMD(0xE6);
        SSD_PAR(0x10);
        SSD_Stop();

        //page1
        SSD_Start();
        SSD_CMD(0xDE);
        SSD_PAR(0x01);
        SSD_Stop();

        //DCDC_OPT
        SSD_Start();
        SSD_CMD(0xB7);
        SSD_PAR(0x03);
        SSD_PAR(0x13);
        SSD_PAR(0xEF);
        SSD_PAR(0x35);
        SSD_PAR(0x35);
        SSD_Stop();

        //SETRGB
        SSD_Start();
        SSD_CMD(0xC1);
        SSD_PAR(0x14);
        SSD_PAR(0x15);
        SSD_PAR(0xC0);
        SSD_Stop();

        //SETSTBA2
        SSD_Start();
        SSD_CMD(0xC2);
        SSD_PAR(0x06);
        SSD_PAR(0x3A);
        SSD_PAR(0xC7);
        SSD_Stop();

        //SET_GAMMAOP
        SSD_Start();
        SSD_CMD(0xC4);
        SSD_PAR(0x72);
        SSD_PAR(0x12);
        SSD_Stop();

        //GAMMA_POWER_TEST
        SSD_Start();
        SSD_CMD(0xBE);
        SSD_PAR(0x00);
        SSD_Stop();

        //page0
        SSD_Start();
        SSD_CMD(0xDE);
        SSD_PAR(0x00);
        SSD_Stop();

        SSD_Start();
        SSD_CMD(0x35);
        SSD_PAR(0x00);
        SSD_Stop();

        SSD_Start();
        SSD_CMD(0x3A);
        SSD_PAR(0x06); //0x06=RGB666  0x05=RGB565
        SSD_Stop();

        SSD_Start();
        SSD_CMD(0x2A);
        SSD_PAR(0x00);
        SSD_PAR(0x4D); //00
        SSD_PAR(0x00);
        SSD_PAR(0xA2); //239
        SSD_Stop();

        SSD_Start();
        SSD_CMD(0x2B);
        SSD_PAR(0x00);
        SSD_PAR(0x00); //00
        SSD_PAR(0x00);
        SSD_PAR(0x90); //296
        SSD_Stop();

        SSD_Start();
        SSD_CMD(0x11);
        SSD_Stop();
        Delayms(120);

        SSD_Start();
        SSD_CMD(0x29);
        SSD_Stop();
        Delayms(50);
    }
};
