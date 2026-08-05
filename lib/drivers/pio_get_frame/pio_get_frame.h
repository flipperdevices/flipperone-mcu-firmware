#pragma once
#include <furi_hal_gpio.h>

typedef struct PioGetFrame PioGetFrame;
typedef void (*PioGetFrameCallbackRx)(uint8_t*data, size_t size, void* context);

#ifdef __cplusplus
extern "C" {
#endif

PioGetFrame* pio_get_frame_init(void);
void pio_get_frame_deinit(PioGetFrame* instance);
void pio_get_frame_set_callback_rx(PioGetFrame* instance, PioGetFrameCallbackRx callback, void* context);

#ifdef __cplusplus
}
#endif
