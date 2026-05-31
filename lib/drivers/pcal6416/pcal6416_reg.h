#pragma once

/* clang-format off */
//https://www.nxp.com/docs/en/data-sheet/PCAL6416A.pdf

typedef enum {
    Pcal6416RegInputPort0 = 0x00,                   /**< Input port 0, Default value: 0xXX */
    Pcal6416RegInputPort1 = 0x01,                   /**< Input port 1, Default value: 0xXX */
    Pcal6416RegOutputPort0 = 0x02,                  /**< Output port 0, Default value: 0xFF */
    Pcal6416RegOutputPort1 = 0x03,                  /**< Output port 1, Default value: 0xFF */
    Pcal6416RegPolarityInversionPort0 = 0x04,       /**< Polarity inversion port 0, Default value: 0x00 */
    Pcal6416RegPolarityInversionPort1 = 0x05,       /**< Polarity inversion port 1, Default value: 0x00 */
    Pcal6416RegConfigurationPort0 = 0x06,           /**< Configuration port 0, Default value: 0xFF */
    Pcal6416RegConfigurationPort1 = 0x07,           /**< Configuration port 1, Default value: 0xFF */
    Pcal6416RegOutputDriveStrength0_0 = 0x40,       /**< Output drive strength port 0 (gpio 0-3), Default value: 0xFF */
    Pcal6416RegOutputDriveStrength0_1 = 0x41,       /**< Output drive strength port 0 (gpio 4-7), Default value: 0xFF */
    Pcal6416RegOutputDriveStrength1_0 = 0x42,       /**< Output drive strength port 1 (gpio 0-3), Default value: 0xFF */
    Pcal6416RegOutputDriveStrength1_1 = 0x43,       /**< Output drive strength port 1 (gpio 4-7), Default value: 0xFF */
    Pcal6416RegInputLatchPort0 = 0x44,              /**< Input latch port 0, Default value: 0x00 */
    Pcal6416RegInputLatchPort1 = 0x45,              /**< Input latch port 1, Default value: 0x00 */
    Pcal6416RegPullupPulldownEnablePort0 = 0x46,    /**< Pull-up/pull-down enable port 0, Default value: 0x00 */
    Pcal6416RegPullupPulldownEnablePort1 = 0x47,    /**< Pull-up/pull-down enable port 1, Default value: 0x00 */
    Pcal6416RegPullupPulldownSelectionPort0 = 0x48, /**< Pull-up/pull-down selection port 0, Default value: 0xFF */
    Pcal6416RegPullupPulldownSelectionPort1 = 0x49, /**< Pull-up/pull-down selection port 1, Default value: 0xFF */
    Pcal6416RegInterruptMaskPort0 = 0x4A,           /**< Interrupt mask port 0, Default value: 0xFF */
    Pcal6416RegInterruptMaskPort1 = 0x4B,           /**< Interrupt mask port 1, Default value: 0xFF */
    Pcal6416RegInterruptStatusPort0 = 0x4C,         /**< Interrupt status port 0, Default value: 0x00 */
    Pcal6416RegInterruptStatusPort1 = 0x4D,         /**< Interrupt status port 1, Default value: 0x00 */
    Pcal6416RegOutputPortConfiguration = 0x4F,      /**< Output port configuration, Default value: 0x00 */

} Pcal6416Reg;

typedef struct {
    uint8_t cc0 : 2;        // Output drive strength for GPIO 0, 00b = 0.25×, 01b = 0.5×, 10b = 0.75× or 11b = 1× of the drive capability of the I/O
    uint8_t cc1 : 2;        // Output drive strength for GPIO 1, 00b = 0.25×, 01b = 0.5×, 10b = 0.75× or 11b = 1× of the drive capability of the I/O
    uint8_t cc2 : 2;        // Output drive strength for GPIO 2, 00b = 0.25×, 01b = 0.5×, 10b = 0.75× or 11b = 1× of the drive capability of the I/O
    uint8_t cc3 : 2;        // Output drive strength for GPIO 3, 00b = 0.25×, 01b = 0.5×, 10b = 0.75× or 11b = 1× of the drive capability of the I/O
    uint8_t cc4 : 2;        // Output drive strength for GPIO 3, 00b = 0.25×, 01b = 0.5×, 10b = 0.75× or 11b = 1× of the drive capability of the I/O
    uint8_t cc5 : 2;        // Output drive strength for GPIO 5, 00b = 0.25×, 01b = 0.5×, 10b = 0.75× or 11b = 1× of the drive capability of the I/O
    uint8_t cc6 : 2;        // Output drive strength for GPIO 6, 00b = 0.25×, 01b = 0.5×, 10b = 0.75× or 11b = 1× of the drive capability of the I/O
    uint8_t cc7 : 2;        // Output drive strength for GPIO 7, 00b = 0.25×, 01b = 0.5×, 10b = 0.75× or 11b = 1× of the drive capability of the I/O
} Pcal6416RegOutputDriveStrengthRegBits;
_Static_assert(
    sizeof(Pcal6416RegOutputDriveStrengthRegBits) == 2,
    "Size check for 'Pcal6416RegOutputDriveStrengthRegBits' failed.");

/* clang-format on */