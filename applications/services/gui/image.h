#pragma once
#include <stddef.h>
#include <stdint.h>

typedef enum {
    /** 1 byte per pixel, 0x00 black .. 0xFF white */
    ImageFormatRawGray8,
    /** 1 bit per pixel, a set bit is black (ink), a clear bit is white.
     * Rows are packed most significant bit first and padded to a whole byte.
     * Drawn with a tint (the image element's backgroundColor with a non-zero
     * alpha), the ink takes the tint's color instead of black. */
    ImageFormatMono1,
} ImageFormat;

typedef struct {
    ImageFormat format;
    uint32_t width;
    uint32_t height;
    const void* data;
    /** Transparency, 1 bit per pixel in the Mono1 layout: a clear bit is not
     * drawn. NULL for a fully opaque image. Applies to every format. */
    const uint8_t* mask;
} Image;

/** Bytes in one row of a 1 bit per pixel plane (Mono1 data or a mask) */
static inline uint32_t image_mono1_stride(uint32_t width) {
    return (width + 7) / 8;
}
