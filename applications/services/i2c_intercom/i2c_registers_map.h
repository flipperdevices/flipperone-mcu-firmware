#pragma once

/**
 * Device address: 0x69
 * 
 * Register map:
 * 0x0000+0 Status register             (read)
 *          Bit 0: Input interrupt flag (cleared when input interrupt register is cleared)
 *          Bit 1-15: Reserved
 * 
 * 0x0100+0 Input interrupt register    (read, read to clear)
 *          Bit 0: Buttons input happened
 *          Bit 1: Touchpad input happened
 *          Bit 2-15: Reserved
 * 0x0100+2 Buttons state register      (read)
 *          Bit 0: InputKey2 state
 *          Bit 1: InputKey1 state
 *          Bit 2: InputKey3 state
 *          Bit 3: InputKey4 state
 *          Bit 4: InputKey5 state
 *          Bit 5: InputKeySw state
 *          Bit 6: InputKeyBack state
 *          Bit 7: InputKeyDown state
 *          Bit 8: InputKeyRight state
 *          Bit 9: InputKeyOk state
 *          Bit 10: InputKeyLeft state
 *          Bit 11: InputKeyUp state
 *          Bit 12: InputKeyPtt state
 *          Bit 13-15: Reserved
 * 0x0100+4 Touchpad X position         (read)
 * 0x0100+6 Touchpad Y position         (read)
 * 0x0100+8 Touchpad press state        (read)
 */

#define I2C_DEVICE_ADDRESS 0x69

#define I2C_STATUS_REG_ADDRESS   0x0000
#define I2C_STATUS_REG_BIT_INPUT 0

#define I2C_INPUT_INTERRUPT_REG_ADDRESS      (0x0100 + 0)
#define I2C_INPUT_INTERRUPT_REG_BIT_BUTTONS  0
#define I2C_INPUT_INTERRUPT_REG_BIT_TOUCHPAD 1

#define I2C_BUTTONS_STATE_REG_ADDRESS  (0x0100 + 2)
#define I2C_TOUCHPAD_X_REG_ADDRESS     (0x0100 + 4)
#define I2C_TOUCHPAD_Y_REG_ADDRESS     (0x0100 + 6)
#define I2C_TOUCHPAD_PRESS_REG_ADDRESS (0x0100 + 8)
