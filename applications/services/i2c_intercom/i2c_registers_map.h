#pragma once

/**
 * Device address: 0x69
 * 
 * Register map:
 * 0x0000   Status register             (read)
 *          Bit 0: Input interrupt flag (cleared when input interrupt register is cleared)
 *          Bit 1-15: Reserved
 * 0x0080   Intercom version register   (read)
 * 0x0100+0 Input interrupt register    (read, read to clear)
 *          Bit 0: Buttons input happened
 *          Bit 1: Touchpad input happened
 *          Bit 2: Headphones input happened
 *          Bit 3-15: Reserved
 * 
 * All mask registers work the same way: writing 1 to a bit will mask the corresponding interrupt and writing 0 will unmask it.
 * 0x0180+0 Input interrupt mask        (read, write)
 *          Bit 0: Buttons input interrupt masked
 *          Bit 1: Touchpad input interrupt masked
 *          Bit 2: Headphones input interrupt masked
 *          Bit 3-15: Reserved
 * 0x0200+0 Buttons state register      (read)
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
 * 0x0200+2 Touchpad X position         (read)
 * 0x0200+4 Touchpad Y position         (read)
 * 0x0200+6 Touchpad press state        (read)
 * 0x0200+8 Headphones state            (read)
 * 
 * 0x0300+0 Link LED group brightness      (read, write), 0 is off, 255 is max brightness. This value stored in the flash. Can be changed from the MCU state.
 * 0x0300+2 Power LED group brightness     (read, write), 0 is off, 255 is max brightness. This value stored in the flash. Can be changed from the MCU state.
 * 0x0300+4 Wattmeter LED group brightness (read, write), 0 is off, 255 is max brightness. This value stored in the flash. Can be changed from the MCU state.
 * 
 * All LED colors are in RGB 565 format
 * 0x0310+0 Link1 LED color (read, write)
 * 0x0310+2 Link2 LED color (read, write)
 * 0x0310+4 Link3 LED color (read, write)
 * 0x0310+6 Link4 LED color (read, write)
 *
 * 0xDEAD   Watchdog kick register (write)
 *          Write any value at this address to reset the watchdog timer.
 *          If no kick is received within the configured timeout, subscribed
 *          callbacks are invoked.
 */

// Device address
#define I2C_DEVICE_ADDRESS (0x69)

// Status register
#define I2C_STATUS_REG_ADDRESS   (0x0000)
#define I2C_STATUS_REG_BIT_INPUT (0)

// Intercom version register
#define I2C_INTERCOM_VERSION_REG_ADDRESS (0x0080)
#define I2C_INTERCOM_VERSION             (0x0001)

// Input interrupt register and mask
#define I2C_INPUT_INTERRUPT_REG_ADDRESS        (0x0100 + 0)
#define I2C_INPUT_INTERRUPT_MASK_REG_ADDRESS   (0x0180 + 0)
#define I2C_INPUT_INTERRUPT_REG_BIT_BUTTONS    (0)
#define I2C_INPUT_INTERRUPT_REG_BIT_TOUCHPAD   (1)
#define I2C_INPUT_INTERRUPT_REG_BIT_HEADPHONES (2)

// Buttons state register
#define I2C_BUTTONS_STATE_REG_ADDRESS      (0x0200 + 0)
#define I2C_BUTTONS_STATE_REG_BIT_KEY2     (0)
#define I2C_BUTTONS_STATE_REG_BIT_KEY1     (1)
#define I2C_BUTTONS_STATE_REG_BIT_KEY3     (2)
#define I2C_BUTTONS_STATE_REG_BIT_KEY4     (3)
#define I2C_BUTTONS_STATE_REG_BIT_KEY5     (4)
#define I2C_BUTTONS_STATE_REG_BIT_KEYSW    (5)
#define I2C_BUTTONS_STATE_REG_BIT_KEYBACK  (6)
#define I2C_BUTTONS_STATE_REG_BIT_KEYDOWN  (7)
#define I2C_BUTTONS_STATE_REG_BIT_KEYRIGHT (8)
#define I2C_BUTTONS_STATE_REG_BIT_KEYOK    (9)
#define I2C_BUTTONS_STATE_REG_BIT_KEYLEFT  (10)
#define I2C_BUTTONS_STATE_REG_BIT_KEYUP    (11)
#define I2C_BUTTONS_STATE_REG_BIT_KEYPTT   (12)

// Touchpad state registers
#define I2C_TOUCHPAD_X_REG_ADDRESS     (0x0200 + 2)
#define I2C_TOUCHPAD_Y_REG_ADDRESS     (0x0200 + 4)
#define I2C_TOUCHPAD_PRESS_REG_ADDRESS (0x0200 + 6)

// Headphones state register
#define I2C_HEADPHONES_STATE_REG_ADDRESS                (0x0200 + 8)
#define I2C_HEADPHONES_STATE_REG_HEADPHONES_PRESENT_BIT (0)
#define I2C_HEADPHONES_STATE_REG_MIC_PRESENT_BIT        (1)
#define I2C_HEADPHONES_STATE_REG_BUTTON_A_PRESSED_BIT   (2)
#define I2C_HEADPHONES_STATE_REG_BUTTON_B_PRESSED_BIT   (3)
#define I2C_HEADPHONES_STATE_REG_BUTTON_C_PRESSED_BIT   (4)
#define I2C_HEADPHONES_STATE_REG_BUTTON_D_PRESSED_BIT   (5)

// Led brightness registers
#define I2C_LED_BRIGHTNESS_LINK_REG_ADDRESS      (0x0300 + 0)
#define I2C_LED_BRIGHTNESS_POWER_REG_ADDRESS     (0x0300 + 2)
#define I2C_LED_BRIGHTNESS_WATTMETER_REG_ADDRESS (0x0300 + 4)

// Led color registers
#define I2C_LED_LINK1_COLOR_REG_ADDRESS (0x0310 + 0)
#define I2C_LED_LINK2_COLOR_REG_ADDRESS (0x0310 + 2)
#define I2C_LED_LINK3_COLOR_REG_ADDRESS (0x0310 + 4)
#define I2C_LED_LINK4_COLOR_REG_ADDRESS (0x0310 + 6)

// Watchdog kick register
#define I2C_WATCHDOG_KICK_REG_ADDRESS     (0xDEAD)
#define I2C_WATCHDOG_DEFAULT_TIMEOUT_MS   (2000)
