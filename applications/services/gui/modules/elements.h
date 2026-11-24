#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

/** Classic softkey: a 48x14 button along the bottom edge, index 0..4 from left to right. */
void elements_softkey_button_element(size_t index, const char* text, bool active, bool pressed);

/** Home screen tab at the bottom edge: Help (1), Power (2) and Settings (3)
 *  as in the mock-up, 0 and 4 continue the row outwards. A pressed tab is inverted. */
void elements_home_tab_element(size_t index, const char* text, bool pressed);

#if defined(__cplusplus)
}
#endif
