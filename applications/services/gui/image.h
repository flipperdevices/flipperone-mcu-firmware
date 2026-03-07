#pragma once
#include <stdint.h>

typedef enum {
    ImageFormatRawGray8,
} ImageFormat;

typedef struct {
    ImageFormat format;
    uint32_t width;
    uint32_t height;
    const void* data;
} Image;