#pragma once

// Device address
/*
 * Device address: 0x69
*/
#define I2C_DEVICE_ADDRESS (0x69)

// Status register
/*
 * 0x0000   Status register             (read)
 *          Bit 0: Input interrupt flag (cleared when input interrupt register is cleared)
 *          Bit 1: Reserved for USB-C (UCSI) events
 *          Bit 2: System interrupt flag (cleared when system interrupt register is cleared)
 *          Bit 3-15: Reserved
*/
#define I2C_STATUS_REG_BASE       (0x0000)
#define I2C_STATUS_REG_ADDRESS    (I2C_STATUS_REG_BASE + 0x00)
#define I2C_STATUS_REG_BIT_INPUT  (0)
#define I2C_STATUS_REG_BIT_SYSTEM (2)

// CPU state registers
/*
 * 0x0040   CPU state register          (read, write)
 *          Written by the host; values: CpuState in host.h
*/
#define I2C_CPU_STATUS_REG_BASE    (0x0040)
#define I2C_CPU_STATUS_REG_ADDRESS (I2C_CPU_STATUS_REG_BASE + 0x00)

// Intercom version register
/*
 * 0x0080   Intercom version register   (read)
*/
#define I2C_INTERCOM_VERSION_REG_BASE    (0x0080)
#define I2C_INTERCOM_VERSION_REG_ADDRESS (I2C_INTERCOM_VERSION_REG_BASE + 0x00)
#define I2C_INTERCOM_VERSION             (0x0001)

// System reset registers
/*
 * 0x0090+0 Reserved, was the system reset command register
 * 0x0090+2 System reset mode register (read, write)
 *          MCU response to the host (Linux) going down, that is to the CPU
 *          state powered-off (9) or restarting (10), which the host sends once
 *          its devices are shut down:
 *          0x0000: Disabled (default): on powered-off the MCU cuts host power
 *                  and leaves the host's screen; restarting is left to the host
 *          0x0001: Switch: if the last update staged an image, cut host
 *                  power and boot that image. After restarting the new
 *                  image powers the host again, after powered-off it leaves
 *                  it off.
 *                  Without a staged image (none written, or a later update
 *                  started, aborted or failed) it acts as 0x0000.
 *          Other values are ignored. The host writes 0x0001 after an update
 *          reaches DONE to switch to the new image; the MCU forgets it on reset.
 * 0x0090+4 System reset delay register (read, write)
 *          Delay in 100 ms units between the triggering host state and the
 *          switch. 0 switches immediately. If the host writes a live state
 *          (bootloader, kernel init, online) before the delay elapses, the
 *          pending switch is cancelled.
*/
#define I2C_SYSTEM_RESET_REG_BASE          (0x0090)
#define I2C_SYSTEM_RESET_MODE_REG_ADDRESS  (I2C_SYSTEM_RESET_REG_BASE + 0x02)
#define I2C_SYSTEM_RESET_DELAY_REG_ADDRESS (I2C_SYSTEM_RESET_REG_BASE + 0x04)

// Input interrupt register and mask
/*
 * 0x0100+0 Input interrupt register    (read, read to clear)
 *          Bit 0: Buttons input happened
 *          Bit 1: Touchpad input happened
 *          Bit 2: Headphones input happened
 *          Bit 3-15: Reserved
 * All mask registers work the same way: writing 1 to a bit will mask the corresponding interrupt and writing 0 will unmask it.
 * 0x0180+0 Input interrupt mask        (read, write)
 *          Bit 0: Buttons input interrupt masked
 *          Bit 1: Touchpad input interrupt masked
 *          Bit 2: Headphones input interrupt masked
 *          Bit 3: SW buttons input interrupt masked
 *          Bit 4-15: Reserved
*/
#define I2C_INPUT_INTERRUPT_REG_BASE           (0x0100)
#define I2C_INPUT_INTERRUPT_REG_ADDRESS        (I2C_INPUT_INTERRUPT_REG_BASE + 0x00)
#define I2C_INPUT_INTERRUPT_MASK_REG_BASE      (0x0180)
#define I2C_INPUT_INTERRUPT_MASK_REG_ADDRESS   (I2C_INPUT_INTERRUPT_MASK_REG_BASE + 0x00)
#define I2C_INPUT_INTERRUPT_REG_BIT_BUTTONS    (0)
#define I2C_INPUT_INTERRUPT_REG_BIT_TOUCHPAD   (1)
#define I2C_INPUT_INTERRUPT_REG_BIT_HEADPHONES (2)
#define I2C_INPUT_INTERRUPT_REG_BIT_SW_BUTTONS (3)

// System interrupt register and mask
/*
 * 0x0104+0 System interrupt register   (read, read to clear)
 *          Bit 0: The MCU (re)started and lost the settings the host wrote
 *                 (CPU state, interrupt masks, LEDs); the host writes them again
 *          Bit 1: The MCU shows the host's screen again while the host was
 *                 already running, and has no frame: the host sends its
 *                 current one, which it would otherwise send only on a change
 *          Bit 2-15: Reserved
 * 0x0184+0 System interrupt mask       (read, write)
 *          Bit 0: Restart interrupt masked
 *          Bit 1: Frame request masked
 *          Bit 2-15: Reserved
*/
#define I2C_SYSTEM_INTERRUPT_REG_ADDRESS      (0x0104)
#define I2C_SYSTEM_INTERRUPT_MASK_REG_ADDRESS (0x0184)
#define I2C_SYSTEM_INTERRUPT_REG_BIT_RESET    (0)
#define I2C_SYSTEM_INTERRUPT_REG_BIT_FRAME    (1)

// Buttons state register
/*
 * 0x0200+0 Buttons state register      (read)
 *          Bit 0: InputKey2 state
 *          Bit 1: InputKey1 state
 *          Bit 2: InputKeyPower state
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
*/
#define I2C_INPUT_STATE_REG_BASE           (0x0200)
#define I2C_BUTTONS_STATE_REG_ADDRESS      (I2C_INPUT_STATE_REG_BASE + 0x00)
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
/*
 * 0x0200+2 Touchpad X position         (read)
 * 0x0200+4 Touchpad Y position         (read)
 * 0x0200+6 Touchpad press state        (read)
*/
#define I2C_TOUCHPAD_X_REG_ADDRESS     (I2C_INPUT_STATE_REG_BASE + 0x02)
#define I2C_TOUCHPAD_Y_REG_ADDRESS     (I2C_INPUT_STATE_REG_BASE + 0x04)
#define I2C_TOUCHPAD_PRESS_REG_ADDRESS (I2C_INPUT_STATE_REG_BASE + 0x06)

// Software buttons state register
/*
 * 0x0200+0xA   Software buttons state    (read)
 *              Bit 0: Power button state
 *              Bit 1-15: Reserved
*/
#define I2C_SW_BUTTONS_STATE_REG_ADDRESS   (I2C_INPUT_STATE_REG_BASE + 0x0A)
#define I2C_SW_BUTTONS_STATE_REG_BIT_POWER (0)

// Headphones state register
/*
 * 0x0200+8 Headphones state            (read)
 *          Bit 0: Headphones present
 *          Bit 1: Microphone present
 *          Bit 2: Button A pressed
 *          Bit 3: Button B pressed
 *          Bit 4: Button C pressed
 *          Bit 5: Button D pressed
 *          Bit 6-15: Reserved
 */
#define I2C_HEADPHONES_STATE_REG_ADDRESS                (I2C_INPUT_STATE_REG_BASE + 0x08)
#define I2C_HEADPHONES_STATE_REG_HEADPHONES_PRESENT_BIT (0)
#define I2C_HEADPHONES_STATE_REG_MIC_PRESENT_BIT        (1)
#define I2C_HEADPHONES_STATE_REG_BUTTON_A_PRESSED_BIT   (2)
#define I2C_HEADPHONES_STATE_REG_BUTTON_B_PRESSED_BIT   (3)
#define I2C_HEADPHONES_STATE_REG_BUTTON_C_PRESSED_BIT   (4)
#define I2C_HEADPHONES_STATE_REG_BUTTON_D_PRESSED_BIT   (5)

// Led brightness registers
/* 
 * 0x0300+0 Link LED group brightness      (read, write), 0 is off, 255 is max brightness. This value stored in the flash. Can be changed from the MCU state.
 * 0x0300+2 Power LED group brightness     (read, write), 0 is off, 255 is max brightness. This value stored in the flash. Can be changed from the MCU state.
 * 0x0300+4 Wattmeter LED group brightness (read, write), 0 is off, 255 is max brightness. This value stored in the flash. Can be changed from the MCU state.
*/
#define I2C_LED_BRIGHTNESS_REG_BASE              (0x0300)
#define I2C_LED_BRIGHTNESS_LINK_REG_ADDRESS      (I2C_LED_BRIGHTNESS_REG_BASE + 0x00)
#define I2C_LED_BRIGHTNESS_POWER_REG_ADDRESS     (I2C_LED_BRIGHTNESS_REG_BASE + 0x02)
#define I2C_LED_BRIGHTNESS_WATTMETER_REG_ADDRESS (I2C_LED_BRIGHTNESS_REG_BASE + 0x04)

// Led color registers
/*
 * All LED colors are in RGB 565 format
 * 0x0310 + 0x00 Link1 LED color (read, write)
 * 0x0310 + 0x02 Link2 LED color (read, write)
 * 0x0310 + 0x04 Link3 LED color (read, write)
 * 0x0310 + 0x06 Link4 LED color (read, write)
 * 
 * 0x0310 + 0x10 Link1 LED pattern ID (read, write)
 * 0x0310 + 0x12 Link2 LED pattern ID (read, write)
 * 0x0310 + 0x14 Link3 LED pattern ID (read, write)
 * 0x0310 + 0x16 Link4 LED pattern ID (read, write)
 *
 * Pattern IDs: 0 - static color (the color register value, the default),
 * 1 - rainbow gradient, 2 - fade, 3 - blink. Patterns run until another ID is
 * written; 0 stops the animation and shows the static color again.
 */
#define I2C_LED_COLOR_REG_BASE          (0x0310)
#define I2C_LED_LINK1_COLOR_REG_ADDRESS (I2C_LED_COLOR_REG_BASE + 0x00)
#define I2C_LED_LINK2_COLOR_REG_ADDRESS (I2C_LED_COLOR_REG_BASE + 0x02)
#define I2C_LED_LINK3_COLOR_REG_ADDRESS (I2C_LED_COLOR_REG_BASE + 0x04)
#define I2C_LED_LINK4_COLOR_REG_ADDRESS (I2C_LED_COLOR_REG_BASE + 0x06)

#define I2C_LED_LINK1_PATTERN_REG_ADDRESS (I2C_LED_COLOR_REG_BASE + 0x10)
#define I2C_LED_LINK2_PATTERN_REG_ADDRESS (I2C_LED_COLOR_REG_BASE + 0x12)
#define I2C_LED_LINK3_PATTERN_REG_ADDRESS (I2C_LED_COLOR_REG_BASE + 0x14)
#define I2C_LED_LINK4_PATTERN_REG_ADDRESS (I2C_LED_COLOR_REG_BASE + 0x16)

// Backlight control registers
/*
 * 0x0350+0 Backlight level register (read, write), 0 is off, 255 is max brightness. This value stored in the flash. Can be changed from the MCU state.
 * 0x0350+2 Backlight time register (read, write), in 100ms intervals, should be greater than 0
 * 0x0350+4 Backlight control register (read, write)
*/
#define I2C_BACKLIGHT_REG_BASE                 (0x0350)
#define I2C_BACKLIGHT_LEVEL                    (I2C_BACKLIGHT_REG_BASE + 0x00)
#define I2C_BACKLIGHT_TIMEOUT                  (I2C_BACKLIGHT_REG_BASE + 0x02) // * 100 ms, 10 = 1s
#define I2C_BACKLIGHT_CONTROL                  (I2C_BACKLIGHT_REG_BASE + 0x04) // bit 0 - override backlight timeout, bit 1 - ping backlight
#define I2C_BACKLIGHT_CONTROL_REG_BIT_OVERRIDE (0)
#define I2C_BACKLIGHT_CONTROL_REG_BIT_PING     (1)

// Haptic registers
/*
 * 0x0400 + 0: 2 bytes
 * bit 15 - Play effect (1 - play, 0 - stop)
 * bits 14:8 - Effect number (0..123)
 * bits 7:0 - Duration of the effect, ms (0 or 1 - play full effect, 2..255 - play ms)
*/
#define I2C_HAPTIC_REG_BASE                (0x0400)
#define I2C_HAPTIC_PLAY_EFFECT_REG_ADDRESS (I2C_HAPTIC_REG_BASE + 0x00)
#define I2C_HAPTIC_PLAY_EFFECT_BIT         (15)
#define I2C_HAPTIC_NUM_EFFECT_MASK         (0x7F00)
#define I2C_HAPTIC_NUM_EFFECT_SHIFT        (8)
#define I2C_HAPTIC_DURATION_MASK           (0x00FF)
#define I2C_HAPTIC_DURATION_SHIFT          (0)

// Updater registers

/*
 * 0xF000 + 0x00: Updater status register (read), High byte: UpdaterError, Low byte: UpdaterState
 * 0xF000 + 0x02: Updater firmware version register (read), 2 bytes, current firmware version
 * 0xF000 + 0x04: Updater command register (write), UpdaterCommand
 * 0xF000 + 0x06: Updater boot status register (read), Low byte: active partition ('A'/'B'), High byte: boot type:
 *                0 normal; 1 flash-update, the switch to a new image, which the MCU bought;
 *                2 revert, the old image after a switch: the new image did not get as
 *                far as buying itself, and the bootrom's watchdog went back to the old one
 * 0xF000 + 0x08: Updater last written version register (read), version of the last accepted update, 0 until the first update
 * 0xF000 + 0x0A: Updater firmware patch register (read), patch of the current firmware, 0 on silicon B3 (see cmake/fw_version.cmake)
 * 0xF000 + 0x0C: Updater last written patch register (read), patch of the last accepted update, 0 until the first update, 0 on B3
 * 0xF000 + 0x20: Updater new firmware CRC register (write), 2 bytes, CRC16 of the new firmware
 * 0xF000 + 0x22: Updater new firmware blocks register (write), 2 bytes, number of blocks of the new firmware
 * 0xF000 + 0x40: Updater data register (write), 256 bytes of data, followed by 2 bytes of block index and 2 bytes of CRC16.
*/
#define I2C_UPDATER_REG_BASE (0xF000)

#define I2C_UPDATER_STATUS_REG_ADDRESS     (I2C_UPDATER_REG_BASE + 0x00)
#define I2C_UPDATER_FW_VERSION_REG_ADDRESS (I2C_UPDATER_REG_BASE + 0x02)
#define I2C_UPDATER_CMD_REG_ADDRESS        (I2C_UPDATER_REG_BASE + 0x04)
#define I2C_UPDATER_BOOT_STATUS_REG_ADDRESS (I2C_UPDATER_REG_BASE + 0x06)
#define I2C_UPDATER_LAST_WRITTEN_VERSION_REG_ADDRESS (I2C_UPDATER_REG_BASE + 0x08)
#define I2C_UPDATER_FW_PATCH_REG_ADDRESS (I2C_UPDATER_REG_BASE + 0x0A)
#define I2C_UPDATER_LAST_WRITTEN_PATCH_REG_ADDRESS (I2C_UPDATER_REG_BASE + 0x0C)

#define I2C_UPDATER_NEW_FW_CRC_REG_ADDRESS    (I2C_UPDATER_REG_BASE + 0x20)
#define I2C_UPDATER_NEW_FW_BLOCKS_REG_ADDRESS (I2C_UPDATER_NEW_FW_CRC_REG_ADDRESS + 2)

#define I2C_UPDATER_BLOCK_SIZE   (256) // payload bytes per block
#define I2C_UPDATER_DATA_ADDRESS (I2C_UPDATER_REG_BASE + 0x40)
#define I2C_UPDATER_DATA_LEN     (I2C_UPDATER_BLOCK_SIZE + 2 /* block index */ + 2 /* block CRC */)
