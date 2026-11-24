#pragma once
#include <stdint.h>

typedef enum {
    /** 8-bit grayscale, width*height bytes, row-major. */
    ImageFormatRawGray8,
    /** A looping frame sequence: data points to an ImageAnimation whose frames
     *  are RawGray8 pixels of the Image's width x height. The GUI picks the
     *  frame from the system tick and redraws itself when the next one is due,
     *  so an animation is placed exactly like a still image. */
    ImageFormatAnimation,
    /** RawGray8 pixels followed by a 1-bit transparency mask: one bit per
     *  pixel, MSB first, each row padded to a whole byte (image_mask_stride()
     *  bytes). A set bit is an opaque pixel; clear bits leave the canvas as is.
     *  The converter picks this format for a PNG with transparent pixels. */
    ImageFormatRawGray8Mask,
} ImageFormat;

typedef struct {
    ImageFormat format;
    uint32_t width;
    uint32_t height;
    const void* data;
} Image;

typedef struct {
    /** frame_count pointers, each to width*height RawGray8 pixels. */
    const uint8_t* const* frames;
    uint32_t frame_count;
    uint32_t frame_duration_ms;
} ImageAnimation;

/** Bytes per mask row of an ImageFormatRawGray8Mask image. */
static inline uint32_t image_mask_stride(uint32_t width) {
    return (width + 7) / 8;
}

/** Frame shown at now_ms. The sequence loops forever and is phase-locked to the
 *  clock, not to when it was first drawn, so every instance of the same
 *  animation on screen runs in step.
 *  @param next_frame_delay_ms  if not NULL, receives the time until the next
 *                              frame, in (0, frame_duration_ms]. */
static inline uint32_t image_animation_frame_at(const ImageAnimation* animation, uint32_t now_ms, uint32_t* next_frame_delay_ms) {
    uint32_t period = animation->frame_duration_ms;
    uint32_t slot = now_ms / period;
    if(next_frame_delay_ms) {
        *next_frame_delay_ms = period - (now_ms - slot * period);
    }
    return slot % animation->frame_count;
}
