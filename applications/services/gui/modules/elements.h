#pragma once
#include <gui/clay_helper.h>

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum {
    /** On a white background: black outline and fill, the whole percentage
     *  over it, inverted where it crosses the fill. */
    ElementsBatteryLookLight,
    /** On a black background: white outline and fill, a charging bolt in the
     *  middle instead of the number. */
    ElementsBatteryLookDark,
} ElementsBatteryLook;

/** Battery gauge, 41x17: an outline with a fill growing from the left.
 *  Lives in the owner's view model; init once, then place it every layout. */
typedef struct {
    ClayCustomElement element;
    float percent;
    ElementsBatteryLook look;
    char text[4];
} ElementsBattery;

void elements_battery_init(ElementsBattery* battery);

/** Place the gauge at its image size; percent is clamped to 0..100. */
void elements_battery_element(Clay_ElementId id, ElementsBattery* battery, float percent, ElementsBatteryLook look);

/** Classic softkey: a 48x14 button along the bottom edge, index 0..4 from left to right. */
void elements_softkey_button_element(size_t index, const char* text, bool active, bool pressed);

/** Home screen tab at the bottom edge: Help (1), Power (2) and Settings (3)
 *  as in the mock-up, 0 and 4 continue the row outwards. A pressed tab is inverted. */
void elements_home_tab_element(size_t index, const char* text, bool pressed);

#if defined(__cplusplus)
}
#endif
