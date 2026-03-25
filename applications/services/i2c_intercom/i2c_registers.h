#pragma once

/**
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
 * 0x0100+4 Touchpad X position         (read)
 * 0x0100+6 Touchpad Y position         (read)
 * 0x0100+8 Touchpad press state        (read)
 */
