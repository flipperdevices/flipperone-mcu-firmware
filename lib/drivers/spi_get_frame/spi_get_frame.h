#pragma once

#include <stddef.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct SpiGetFrame SpiGetFrame;
typedef void (*SpiGetFrameCallbackRx)(uint8_t*data, size_t size, void* context);

#ifdef __cplusplus
extern "C" {
#endif

SpiGetFrame* spi_get_frame_init(void);
void spi_get_frame_deinit(SpiGetFrame* instance);
void spi_get_frame_set_callback_rx(SpiGetFrame* instance, SpiGetFrameCallbackRx callback, void* context);

#ifdef __cplusplus
}
#endif
