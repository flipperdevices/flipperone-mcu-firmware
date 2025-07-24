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
        uint freq = spi_init(spi, 75 * 1000 * 1000);
        Log::info("SPI%d initialized with frequency: %.2f MHz", SPI_NUM(spi), freq / 1000000.0f);
        spi_set_format(spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);

        gpio_set_function(pin_clock, GPIO_FUNC_SPI);
        gpio_set_function(pin_data, GPIO_FUNC_SPI);

        gpio_init(pin_cs);
        gpio_set_dir(pin_cs, GPIO_OUT);

        gpio_init(pin_dc);
        gpio_set_dir(pin_dc, GPIO_OUT);

        gpio_init(pin_reset);
        gpio_set_dir(pin_reset, GPIO_OUT);

        cs(true);
        dc(false);

        // reset sequence
        reset(true);
        sleep_ms(5);
        reset(false);
        sleep_ms(5);
        reset(true);
        sleep_ms(5);

        write_command(CMD::SWRESET); // Software reset
        sleep_ms(5);

        write_command(CMD::SLPOUT); // Sleep out
        sleep_ms(5);

        // write_command(CMD::INVON);

        // write_command(CMD::COLMOD);
        // write_data(0b01010101); // ‘101’ = 16bit/pixel

        // write_command(CMD::MADCTL);
        // write_data(0b10100000);
        // fill(0x0000); // Fill with black
        // write_command(CMD::NORON);
        // write_command(CMD::DISPON);

        st7735s_initialize_new();
    }

    void write_buffer(const uint16_t* buffer) {
        write_buffer(width, height, buffer, width * height * sizeof(uint16_t));
    }

    void write_buffer(uint16_t w, uint16_t h, const uint16_t* buffer, size_t size) {
        set_window(offset_x, offset_y, offset_x + w - 1, offset_y + h - 1);
        write_command(CMD::RAMWR);

        spi_set_format(spi, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
        cs(false);
        dc(true);
        spi_write16_blocking(spi, buffer, size / 2);
        cs(true);
        spi_set_format(spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
    }

    void fill(uint16_t color) {
        uint16_t data[width];
        for(size_t i = 0; i < width; i++) {
            data[i] = color;
        }

        set_window(offset_x, offset_y, offset_x + width - 1, offset_y + height - 1);
        write_command(CMD::RAMWR);

        spi_set_format(spi, 16, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
        cs(false);
        dc(true);

        for(size_t j = 0; j < height; j++) {
            spi_write16_blocking(spi, data, width);
        }

        cs(true);
        spi_set_format(spi, 8, SPI_CPOL_0, SPI_CPHA_0, SPI_MSB_FIRST);
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
        RDDPM = 0x0a, /* Read Display Power Mode */
        RDDMADCTL = 0x0b, /* Read Display MADCTL */
        RDDCOLMOD = 0x0c, /* Read Display Pixel Format */
        RDDIM = 0x0d, /* Read Display Image Mode */
        RDDSM = 0x0e, /* Read Display Signal Mode */
        RDDSDR = 0x0f, /* Read Display Self-Diagnostic Result */
        SLPIN = 0x10, /* Sleep In */
        SLPOUT = 0x11, /* Sleep Out */
        PTLON = 0x12, /* Partial Display Mode On */
        NORON = 0x13, /* Normal Display Mode On */
        INVOFF = 0x20, /* Display Inversion Off */
        INVON = 0x21, /* Display Inversion On */
        GAMSET = 0x26, /* Gamma Set */
        DISPOFF = 0x28, /* Display Off */
        DISPON = 0x29, /* Display On */
        CASET = 0x2a, /* Column Address Set */
        RASET = 0x2b, /* Row Address Set */
        RAMWR = 0x2c, /* Memory Write */
        RGBSET = 0x2d, /* Color Setting 4k, 65k, 262k */
        RAMRD = 0x2e, /* Memory Read */
        PTLAR = 0x30, /* Partial Area */
        SCRLAR = 0x33, /* Scroll Area Set */
        TEOFF = 0x34, /* Tearing Effect Line OFF */
        TEON = 0x35, /* Tearing Effect Line ON */
        MADCTL = 0x36, /* Memory Data Access Control */
        VSCSAD = 0x37, /* Vertical Scroll Start Address of RAM */
        IDMOFF = 0x38, /* Idle Mode Off */
        IDMON = 0x39, /* Idle Mode On */
        COLMOD = 0x3a, /* Interface Pixel Format */
        RDID1 = 0xda, /* Read ID1 Value */
        RDID2 = 0xdb, /* Read ID2 Value */
        RDID3 = 0xdc, /* Read ID3 Value */
        FRMCTR1 = 0xb1, /* Frame Rate Control in normal mode, full colors */
        FRMCTR2 = 0xb2, /* Frame Rate Control in idle mode, 8 colors */
        FRMCTR3 = 0xb3, /* Frame Rate Control in partial mode, full colors */
        INVCTR = 0xb4, /* Display Inversion Control */
        PWCTR1 = 0xc0, /* Power Control 1 */
        PWCTR2 = 0xc1, /* Power Control 2 */
        PWCTR3 = 0xc2, /* Power Control 3 in normal mode, full colors */
        PWCTR4 = 0xc3, /* Power Control 4 in idle mode 8colors */
        PWCTR5 = 0xc4, /* Power Control 5 in partial mode, full colors */
        VMCTR1 = 0xc5, /* VCOM Control 1 */
        VMOFCTR = 0xc7, /* VCOM Offset Control */
        WRID2 = 0xd1, /* Write ID2 Value */
        WRID3 = 0xd2, /* Write ID3 Value */
        NVFCTR1 = 0xd9, /* NVM Control Status */
        NVFCTR2 = 0xde, /* NVM Read Command */
        NVFCTR3 = 0xdf, /* NVM Write Command */
        GMCTRP1 = 0xe0, /* Gamma '+'Polarity Correction Characteristics Setting */
        GMCTRN1 = 0xe1, /* Gamma '-'Polarity Correction Characteristics Setting */
        GCV = 0xfc, /* Gate Pump Clock Frequency Variable */
    };

    inline void write_command(CMD command) {
        cs(false);
        dc(false);
        spi_write_blocking(spi, reinterpret_cast<const uint8_t*>(&command), 1);
        cs(true);
    }

    inline void write_command(uint8_t command) {
        cs(false);
        dc(false);
        spi_write_blocking(spi, &command, 1);
        cs(true);
    }

    inline void write_data(uint8_t data) {
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

    void st7735s_initialize_new(void) {
        // Frame rate in various modes
        // 10.2.1 FRMCTR1 (B1h): Frame Rate Control (In normal mode/ Full colors)
        write_command(CMD::FRMCTR1);
        write_data(0b00000001); // RTNA: 0b0001 = 0x1 = 1
        write_data(0b00001000); // FPA: 0b00001000 = 0x08 = 8
        write_data(0b00000101); // BPA: 0b00000101 = 0x05 = 5

        // 10.2.2 FRMCTR2 (B2h): Frame Rate Control (In Idle mode/ 8-colors)
        write_command(CMD::FRMCTR2);
        write_data(0b00000001); // RTNB: 0b0001 = 0x1 = 1
        write_data(0b00101100); // FPB: 0b101100 = 0x2C = 44
        write_data(0b00101101); // BPB: 0b101101 = 0x2D = 45

        // 10.2.3 FRMCTR3 (B3h): Frame Rate Control (In Partial mode/ full colors)
        write_command(CMD::FRMCTR3);
        write_data(0b00000001); // RTNC: 0b0001 = 0x1 = 1
        write_data(0b00101100); // FPC: 0b101100 = 0x2C = 44
        write_data(0b00101101); // BPC: 0b101101 = 0x2D = 45
        write_data(0b00000001); // RTND: 0b0001 = 0x1 = 1
        write_data(0b00101100); // FPD: 0b101100 = 0x2C = 44
        write_data(0b00101101); // BPD: 0b101101 = 0x2D = 45

        // Various options
        // 10.2.4 INVCTR (B4h): Display Inversion Control
        write_command(CMD::INVCTR);
        write_data(0b00000111); // NLA NLB NLC all 1 - column inversion

        // 10.2.5 PWCTR1 (C0h): Power Control 1
        write_command(CMD::PWCTR1);
        write_data(0b10100010); // AVDD: 0b101/5V, VRHP: 0b00010/4.6V
        write_data(0b00000010); // VRHN: 0b00010/-4.6V
        write_data(0b10000100); // MODE: 0b10/AUTO, VRHN5: 0, VRHP5: 0

        // 10.2.6 PWCTR2 (C1h): Power Control 2
        write_command(CMD::PWCTR2);
        write_data(0b11000101); // VGH25: 0b11/2.4V, VGLSEL: 0b01/-10 V, VGHBT: 0b01/3*AVDD-0.5

        // 10.2.7 PWCTR3 (C2h): Power Control 3 (in Normal mode/ Full colors)
        write_command(CMD::PWCTR3);
        write_data(0b00001101); // DCA[9:8]: 0b00, SAPA: 0b001/Small, AP: 0b101/Large
        write_data(0b00000000); // DCA[7:0]: 0b00000000

        // 10.2.8 PWCTR4 (C3h): Power Control 4 (in Idle mode/ 8-colors)
        write_command(CMD::PWCTR4);
        write_data(0b10001010); // DCA[9:8]: 0b10, SAPA: 0b001/Small, AP: 0b010/Medium Low
        write_data(0b00101010); // DCA[7:0]: 0b00101010

        // 10.2.9 PWCTR5 (C4h): Power Control 5 (in Partial mode/ full-colors)
        write_command(CMD::PWCTR5);
        write_data(0b10001010); // DCA[9:8]: 0b10, SAPA: 0b001/Small, AP: 0b010/Medium Low
        write_data(0b11101110); // DCA[7:0]: 0b11101110

        // 10.2.10 VMCTR1 (C5h): VCOM Control 1
        write_command(CMD::VMCTR1);
        write_data(0b00011000); // VCOMS: 0b011000/-1.025V

        // 10.1.15 INVOFF (20h): Display Inversion Off
        // write_command(CMD::INVON);

        // 10.1.29 MADCTL (36h): Memory Data Access Control
        write_command(CMD::MADCTL);
        // write_data(
        //     bus,
        //     0b11001000); // MY: 0b1, MX: 0b1, MV: 0b0, ML: 0b0/Top to Bottom, RGB: 0b1/BGR, MH: 0b0/Left to right
        write_data(0b10000000);

        // 10.1.33 COLMOD (3Ah): Interface Pixel Format
        write_command(CMD::COLMOD);
        // write_data(0b00000110); // IFPF: 0b110/18 bit per pixel
        write_data(0b00000101); // IFPF: 0b101/16 bit per pixel

        // // Gamma correction and curves
        // // 10.2.17 GMCTRP1 (E0h): Gamma (‘+’polarity) Correction Characteristics Setting
        // write_command(CMD::GMCTRP1);
        // write_data(0b00000010); //   VRF0P: 0b00000010 = 0x02   | Variable resistor VRHP
        // write_data(0b00011100); //   VOS0P: 0b00011100 = 0x1C   | Variable Resistor VRLP
        // write_data(0b00000111); //    PK0P: 0b00000111 = 0x07
        // write_data(0b00010010); //    PK1P: 0b00010010 = 0x12
        // write_data(0b00110111); //    PK2P: 0b00110111 = 0x37
        // write_data(0b00110010); //    PK3P: 0b00110010 = 0x32
        // write_data(0b00101001); //    PK4P: 0b00101001 = 0x29
        // write_data(0b00101101); //    PK5P: 0b00101101 = 0x2D
        // write_data(0b00101001); //    PK6P: 0b00101001 = 0x29
        // write_data(0b00100101); //    PK7P: 0b00100101 = 0x25
        // write_data(0b00101011); //    PK8P: 0b00101011 = 0x2B
        // write_data(0b00111001); //    PK9P: 0b00111001 = 0x39
        // write_data(0b00000000); //  SELV0P: 0b00000000 = 0x00
        // write_data(0b00000001); //  SELV1P: 0b00000001 = 0x01
        // write_data(0b00000011); // SELV62P: 0b00000011 = 0x03
        // write_data(0b00010000); // SELV63P: 0b00010000 = 0x10

        // // 10.2.18 GMCTRN1 (E1h): Gamma ‘-’polarity Correction Characteristics Setting
        // write_command(CMD::GMCTRN1);
        // write_data(0b00000011); //   VRF0N: 0b00000011 = 0x03 | Variable resistor VRHN
        // write_data(0b00011101); //   VOS0N: 0b00011101 = 0x1D | Variable Resistor VRLN
        // write_data(0b00000111); //    PK0N: 0b00000111 = 0x07
        // write_data(0b00000110); //    PK1N: 0b00000110 = 0x06
        // write_data(0b00101110); //    PK2N: 0b00101110 = 0x2E
        // write_data(0b00101100); //    PK3N: 0b00101100 = 0x2C
        // write_data(0b00101001); //    PK4N: 0b00101001 = 0x29
        // write_data(0b00101101); //    PK5N: 0b00101101 = 0x2D
        // write_data(0b00101110); //    PK6N: 0b00101110 = 0x2E
        // write_data(0b00101110); //    PK7N: 0b00101110 = 0x2E
        // write_data(0b00110111); //    PK8N: 0b00110111 = 0x37
        // write_data(0b00111111); //    PK9N: 0b00111111 = 0x3F
        // write_data(0b00000000); //  SELV0N: 0b00000000 = 0x00
        // write_data(0b00000000); //  SELV1N: 0b00000000 = 0x00
        // write_data(0b00000010); // SELV62N: 0b00000010 = 0x02
        // write_data(0b00010000); // SELV63N: 0b00010000 = 0x10

#define SSD_Start()
#define SSD_Stop()
#define SSD_CMD(x) write_command(x)
#define SSD_PAR(x) write_data(x)

        //SET_PASSWD
        SSD_Start();
        SSD_CMD(0xDF);
        SSD_PAR(0x98);
        SSD_PAR(0x53);
        SSD_Stop();

        //VGPOW_SET
        SSD_Start();
        SSD_CMD(0xB2);
        SSD_PAR(0x18);
        SSD_Stop();

        // //GAMMA_SET
        SSD_Start();
        SSD_CMD(0xB7); //5.5v
        SSD_PAR(0x00);
        SSD_PAR(0x51);
        SSD_PAR(0x00);
        SSD_PAR(0x79);
        SSD_Stop();

        // // GAMMA_SET
        // SSD_Start();
        // SSD_CMD(183);
        // SSD_PAR(0x00);
        // SSD_PAR(81);
        // SSD_PAR(0x00);
        // SSD_PAR(121);
        // SSD_Stop();

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
        SSD_PAR(0xA0); //
        SSD_PAR(0x79); //
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
        SSD_PAR(0xFF); //0x3F 0x3F
        SSD_PAR(0xFF); //0x32 0x27
        SSD_PAR(0xFF); //0x2A 0x1E
        SSD_PAR(0xFF); //0x23 0x18
        SSD_PAR(0xFF); //0x27 0x1C
        SSD_PAR(0xFF); //0x29 0x1F
        SSD_PAR(0xFF); //0x23 0x1A
        SSD_PAR(0xFF); //0x23 0x1A
        SSD_PAR(0xFF); //0x20 0x1A
        SSD_PAR(0xFF); //0x1F 0x1A
        SSD_PAR(0xFF); //0x1B 0x1A
        SSD_PAR(0xFF); //0x0E 0x12
        SSD_PAR(0xFF); //0x0A 0x0F
        SSD_PAR(0xFF); //0x04 0x0B
        SSD_PAR(0xFF); //0x00 0x05
        SSD_PAR(0xFF); //0x00 0x00
        SSD_PAR(0xFF); //0x3F 0x3F
        SSD_PAR(0xFF); //0x32 0x27
        SSD_PAR(0xFF); //0x2A 0x1E
        SSD_PAR(0xFF); //0x23 0x18
        SSD_PAR(0xFF); //0x27 0x1C
        SSD_PAR(0xFF); //0x29 0x1F
        SSD_PAR(0xFF); //0x23 0x1A
        SSD_PAR(0xFF); //0x23 0x1A
        SSD_PAR(0xFF); //0x20 0x1A
        SSD_PAR(0xFF); //0x1F 0x1A
        SSD_PAR(0xFF); //0x1B 0x1A
        SSD_PAR(0xFF); //0x0E 0x12
        SSD_PAR(0xFF); //0x0A 0x0F
        SSD_PAR(0xFF); //0x04 0x0B
        SSD_PAR(0xFF); //0x00 0x05
        SSD_PAR(0xFF); //0x00 0x00
        SSD_Stop();

        // //SET_GD
        // SSD_Start();
        // SSD_CMD(0xD0);
        // SSD_PAR(0x04);
        // SSD_PAR(0x06);
        // SSD_PAR(0x6B);
        // SSD_PAR(0x0F);
        // SSD_PAR(0x00);
        // SSD_Stop();

        //RAMCTRL
        SSD_Start();
        SSD_CMD(0xD7);
        SSD_PAR(0x00);
        SSD_PAR(0x30);
        SSD_Stop();

        // //ECO
        // SSD_Start();
        // SSD_CMD(0xE6);
        // SSD_PAR(0x10);
        // SSD_Stop();

        // //page1
        // SSD_Start();
        // SSD_CMD(0xDE);
        // SSD_PAR(0x01);
        // SSD_Stop();

        // //DCDC_OPT
        // SSD_Start();
        // SSD_CMD(0xB7);
        // SSD_PAR(0x03);
        // SSD_PAR(0x13);
        // SSD_PAR(0xEF);
        // SSD_PAR(0x35);
        // SSD_PAR(0x35);
        // SSD_Stop();

        // //SETRGB
        // SSD_Start();
        // SSD_CMD(0xC1);
        // SSD_PAR(0x14);
        // SSD_PAR(0x15);
        // SSD_PAR(0xC0);
        // SSD_Stop();

        // //SETSTBA2
        // SSD_Start();
        // SSD_CMD(0xC2);
        // SSD_PAR(0x06);
        // SSD_PAR(0x3A);
        // SSD_PAR(0xC7);
        // SSD_Stop();

        // //SET_GAMMAOP
        // SSD_Start();
        // SSD_CMD(0xC4);
        // SSD_PAR(0x72);
        // SSD_PAR(0x12);
        // SSD_Stop();

        //GAMMA_POWER_TEST
        // SSD_Start();
        // SSD_CMD(0xBE);
        // SSD_PAR(0x00);
        // SSD_Stop();

        //page0
        SSD_Start();
        SSD_CMD(0xDE);
        SSD_PAR(0x00);
        SSD_Stop();

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

        // // Gamma correction and curves
        // // 10.2.17 GMCTRP1 (E0h): Gamma (‘+’polarity) Correction Characteristics Setting
        // write_command(CMD::GMCTRP1);
        // write_data(0b00000010); //   VRF0P: 0b00000010 = 0x02   | Variable resistor VRHP
        // write_data(0b00011100); //   VOS0P: 0b00011100 = 0x1C   | Variable Resistor VRLP
        // write_data(0b00000111); //    PK0P: 0b00000111 = 0x07
        // write_data(0b00010010); //    PK1P: 0b00010010 = 0x12
        // write_data(0b00110111); //    PK2P: 0b00110111 = 0x37
        // write_data(0b00110010); //    PK3P: 0b00110010 = 0x32
        // write_data(0b00101001); //    PK4P: 0b00101001 = 0x29
        // write_data(0b00101101); //    PK5P: 0b00101101 = 0x2D
        // write_data(0b00101001); //    PK6P: 0b00101001 = 0x29
        // write_data(0b00100101); //    PK7P: 0b00100101 = 0x25
        // write_data(0b00101011); //    PK8P: 0b00101011 = 0x2B
        // write_data(0b00111001); //    PK9P: 0b00111001 = 0x39
        // write_data(0b00000000); //  SELV0P: 0b00000000 = 0x00
        // write_data(0b00000001); //  SELV1P: 0b00000001 = 0x01
        // write_data(0b00000011); // SELV62P: 0b00000011 = 0x03
        // write_data(0b00010000); // SELV63P: 0b00010000 = 0x10

        // // 10.2.18 GMCTRN1 (E1h): Gamma ‘-’polarity Correction Characteristics Setting
        // write_command(CMD::GMCTRN1);
        // write_data(0b00000011); //   VRF0N: 0b00000011 = 0x03 | Variable resistor VRHN
        // write_data(0b00011101); //   VOS0N: 0b00011101 = 0x1D | Variable Resistor VRLN
        // write_data(0b00000111); //    PK0N: 0b00000111 = 0x07
        // write_data(0b00000110); //    PK1N: 0b00000110 = 0x06
        // write_data(0b00101110); //    PK2N: 0b00101110 = 0x2E
        // write_data(0b00101100); //    PK3N: 0b00101100 = 0x2C
        // write_data(0b00101001); //    PK4N: 0b00101001 = 0x29
        // write_data(0b00101101); //    PK5N: 0b00101101 = 0x2D
        // write_data(0b00101110); //    PK6N: 0b00101110 = 0x2E
        // write_data(0b00101110); //    PK7N: 0b00101110 = 0x2E
        // write_data(0b00110111); //    PK8N: 0b00110111 = 0x37
        // write_data(0b00111111); //    PK9N: 0b00111111 = 0x3F
        // write_data(0b00000000); //  SELV0N: 0b00000000 = 0x00
        // write_data(0b00000000); //  SELV1N: 0b00000000 = 0x00
        // write_data(0b00000010); // SELV62N: 0b00000010 = 0x02
        // write_data(0b00010000); // SELV63N: 0b00010000 = 0x10

        write_command(CMD::GAMSET);
        write_data(0b00000010);
    }
};
