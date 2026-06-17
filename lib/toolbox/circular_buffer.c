#include "circular_buffer.h"

#include <core/check.h>

struct CircularBuffer {
    uint8_t* data;
    size_t size;
    volatile size_t head; /**< write position (writer only) */
    volatile size_t tail; /**< read position  (reader only) */
};

CircularBuffer* circular_buffer_alloc(size_t size) {
    CircularBuffer* cb = malloc(sizeof(CircularBuffer));
    cb->data = malloc(size);
    cb->size = size;
    cb->head = 0;
    cb->tail = 0;
    return cb;
}

size_t circular_buffer_bytes_available(const CircularBuffer* cb) {
    const size_t head = cb->head;
    const size_t tail = cb->tail;
    return (head >= tail) ? (head - tail) : (cb->size - (tail - head));
}

size_t circular_buffer_spaces_available(const CircularBuffer* cb) {
    return cb->size - circular_buffer_bytes_available(cb);
}

size_t circular_buffer_write(CircularBuffer* cb, const uint8_t* data, size_t size, bool overwrite) {
    furi_check(cb);
    furi_check(size > 0);
    furi_check(data);

    size_t space = circular_buffer_spaces_available(cb);

    if(size > space) {
        if(!overwrite) return 0; /* drop new, preserve old */

        /* Overwrite: advance tail by exactly (size - space + 1 ) bytes,
           wrapping around the buffer as needed. */
        size_t drop = size - space + 1;
        cb->tail = (cb->tail + drop) % cb->size;
    }

    /* Write data at head position */
    const size_t head = cb->head;
    size_t first = cb->size - head;
    if(size <= first) {
        memcpy(cb->data + head, data, size);
        cb->head = (head + size) % cb->size;
    } else {
        memcpy(cb->data + head, data, first);
        memcpy(cb->data, data + first, size - first);
        cb->head = size - first;
    }

    return size;
}

size_t circular_buffer_read(CircularBuffer* cb, uint8_t* data, size_t size) {
    furi_check(cb);
    furi_check(size > 0);
    furi_check(data);

    size_t avail = circular_buffer_bytes_available(cb);
    if(size > avail) size = avail;

    size_t first = cb->size - cb->tail;
    if(size <= first) {
        memcpy(data, cb->data + cb->tail, size);
        cb->tail += size;
    } else {
        memcpy(data, cb->data + cb->tail, first);
        memcpy(data + first, cb->data, size - first);
        cb->tail = size - first;
    }

    return size;
}

size_t circular_buffer_get_index_tail(const CircularBuffer* cb) {
    furi_check(cb);
    return cb->tail;
}

void circular_buffer_set_index_tail(CircularBuffer* cb, size_t index) {
    furi_check(cb);
    furi_check(index < cb->size);
    cb->tail = index;
}