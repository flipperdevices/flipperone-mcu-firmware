#pragma once
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/** How often the animation wants a new frame */
#define STARTING_LINUX_FRAME_MS 33

/** Lay out the "Starting Linux..." screen as it is elapsed_ms into its
 * animation, as a Clay element that fills its parent. */
void starting_linux_layout(uint32_t elapsed_ms);

#ifdef __cplusplus
}
#endif
