#include "circular_buffer.h"

#include <furi.h>

struct CircularBuffer {
    uint8_t* data;
    size_t size;
    volatile size_t head; /**< write position (writer only) */
    volatile size_t tail; /**< read position  (reader only) */
    FuriMutex* mutex; /**< mutex for thread-safe access */
    bool overwrite; /**< flag to allow overwriting old data when buffer is full */
    bool full; /**< true when head == tail but buffer contains data */
};

CircularBuffer* circular_buffer_alloc(size_t size, bool overwrite) {
    CircularBuffer* cb = malloc(sizeof(CircularBuffer));
    cb->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    cb->data = malloc(size);
    cb->size = size;
    cb->head = 0;
    cb->overwrite = overwrite;
    cb->tail = 0;
    cb->full = false;
    return cb;
}

void circular_buffer_free(CircularBuffer* cb) {
    furi_mutex_free(cb->mutex);
    free(cb->data);
    free(cb);
}

bool circular_buffer_acquire(CircularBuffer* cb) {
    return furi_mutex_acquire(cb->mutex, !FURI_IS_ISR() ? FuriWaitForever : 0) == FuriStatusOk;
}

void circular_buffer_release(CircularBuffer* cb) {
    furi_mutex_release(cb->mutex);
}

size_t circular_buffer_bytes_available(const CircularBuffer* cb) {
    if(cb->full) return cb->size;
    const size_t head = cb->head;
    const size_t tail = cb->tail;
    return (head >= tail) ? (head - tail) : (cb->size - (tail - head));
}

size_t circular_buffer_spaces_available(const CircularBuffer* cb) {
    return cb->size - circular_buffer_bytes_available(cb);
}

size_t circular_buffer_write(CircularBuffer* cb, const uint8_t* data, size_t size) {
    furi_check(cb);
    furi_check(size > 0);
    furi_check(data);

    const size_t original_size = size;
    size_t space = circular_buffer_spaces_available(cb);

    if(size > space) {
        if(!cb->overwrite) return 0; /* drop new, preserve old */

        if(cb->overwrite && size > cb->size) {
            /* Input larger than whole buffer: discard everything,
               keep only the last `cb->size` bytes of input. */
            data += size - cb->size;
            size = cb->size;
            cb->tail = cb->head; /* discard all current data */
            cb->full = true;
        } else {
            /* Regular overwrite: advance tail to make room */
            size_t drop = size - space;
            cb->tail = (cb->tail + drop) % cb->size;
        }
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

    /* If head caught up to tail after writing, buffer is full */
    if(cb->head == cb->tail) cb->full = true;

    return original_size;
}

size_t circular_buffer_read(CircularBuffer* cb, uint8_t* data, size_t size) {
    furi_check(cb);
    furi_check(size > 0);
    furi_check(data);

    size_t avail = circular_buffer_bytes_available(cb);
    if(size > avail) size = avail;
    if(size == 0) return 0;

    cb->full = false; /* after reading, buffer definitely has space */

    size_t first = cb->size - cb->tail;
    if(size <= first) {
        memcpy(data, cb->data + cb->tail, size);
        cb->tail += size;
        if(cb->tail == cb->size) cb->tail = 0;
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
