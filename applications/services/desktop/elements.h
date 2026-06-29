#pragma once

#if defined(__cplusplus)
extern "C" {
#endif

typedef enum {
    SoftkeyButtonStateInactive,
    SoftkeyButtonStateActive,
    SoftkeyButtonStatePressed,
} SoftkeyButtonState;

void elements_softkey_button_element(const char* text, SoftkeyButtonState state);

#if defined(__cplusplus)
}
#endif
