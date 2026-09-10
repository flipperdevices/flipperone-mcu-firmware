#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Initialize the display backlight
 */
void furi_bsp_display_backlight_init(void);

/** Set the display backlight brightness
 * @param brightness - brightness level 0-255, 0 turns the backlight off
 */
void furi_bsp_display_backlight_set_brightness(uint8_t brightness);

#ifdef __cplusplus
}
#endif
