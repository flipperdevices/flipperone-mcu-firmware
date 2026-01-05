#pragma once
#include <furi.h>

#define RECORD_DISPLAY "display"
typedef struct Display Display;

typedef enum {
    DisplayModeCpu,
    DisplayModeMpu,

    DisplayModeCount,
} DisplayMode;

#ifdef __cplusplus
extern "C" {
#endif
FuriPubSub* display_get_pubsub(Display* display);
void display_set_brightness(Display* instance, uint8_t brightness);
uint8_t display_get_brightness(Display* instance);
void display_set_mode(Display* instance, DisplayMode mode);
DisplayMode display_get_mode(Display* instance);

#ifdef __cplusplus
}
#endif
