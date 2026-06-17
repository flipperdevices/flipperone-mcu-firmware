#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct CircularBuffer CircularBuffer;

#ifdef __cplusplus
extern "C" {
#endif

CircularBuffer* circular_buffer_alloc(size_t size);
size_t circular_buffer_bytes_available(const CircularBuffer* cb);
size_t circular_buffer_spaces_available(const CircularBuffer* cb);
size_t circular_buffer_write(CircularBuffer* cb, const uint8_t* data, size_t size, bool overwrite);
size_t circular_buffer_read(CircularBuffer* cb, uint8_t* data, size_t size);
size_t circular_buffer_get_index_tail(const CircularBuffer* cb);
void circular_buffer_set_index_tail(CircularBuffer* cb, size_t index);

#ifdef __cplusplus
}
#endif
