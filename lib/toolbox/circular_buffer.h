#pragma once

#include <stdint.h>
#include <stdlib.h>
#include <stdbool.h>

typedef struct CircularBuffer CircularBuffer;

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief  Simple circular byte FIFO (single-reader, single-writer)
 * @param size Size of the buffer in bytes. Must be > 0.
 * @param overwrite If true, if there is not enough space, the oldest data will be overwritten. If false, the new data will be dropped.
 * @return CircularBuffer* on success, NULL on failure
*/
CircularBuffer* circular_buffer_alloc(size_t size, bool overwrite);

/**
 * @brief Free a CircularBuffer and its internal data buffer
 * @param cb CircularBuffer to free
 * Note: the caller must ensure that no other thread is accessing the buffer when this is called
 */
void circular_buffer_free(CircularBuffer* cb);

/**
 * @brief Acquire the mutex for the circular buffer. Must be called before any other operations on the buffer.
 * @param cb CircularBuffer to acquire
 * @return true if the mutex was successfully acquired, false otherwise
 */
bool circular_buffer_acquire(CircularBuffer* cb);

/**
 * @brief Release the mutex for the circular buffer. Must be called after finishing operations on the buffer.
 * @param cb CircularBuffer to release
 */
void circular_buffer_release(CircularBuffer* cb);

/**
 * @brief Get the number of bytes currently available in the buffer for reading
 * @param cb CircularBuffer to query
 * @return Number of bytes available for reading
 */
size_t circular_buffer_bytes_available(const CircularBuffer* cb);

/**
 * @brief Get the number of bytes currently available in the buffer for writing
 * @param cb CircularBuffer to query
 * @return Number of bytes available for writing
 */
size_t circular_buffer_spaces_available(const CircularBuffer* cb);

/**
 * @brief Write data to the circular buffer
 * @param cb CircularBuffer to write to
 * @param data Pointer to the data to write
 * @param size Number of bytes to write
 * @return Number of bytes actually written (may be less than size if overwrite is false and there is not enough space)
 */
size_t circular_buffer_write(CircularBuffer* cb, const uint8_t* data, size_t size);

/**
 * @brief Read data from the circular buffer
 * @param cb CircularBuffer to read from
 * @param data Pointer to the buffer to store the read data
 * @param size Maximum number of bytes to read
 * @return Number of bytes actually read (may be less than size if there is not enough data available)
 */
size_t circular_buffer_read(CircularBuffer* cb, uint8_t* data, size_t size);

/**
 * @brief Get the current read index (tail) of the circular buffer
 * @param cb CircularBuffer to query
 * @return Current read index (tail)
 */
size_t circular_buffer_get_index_tail(CircularBuffer* cb);

/**
 * @brief Set the current read index (tail) of the circular buffer
 * @param cb CircularBuffer to modify
 * @param index New read index (tail). Must be less than the buffer size.
 */
void circular_buffer_set_index_tail(CircularBuffer* cb, size_t index);

#ifdef __cplusplus
}
#endif
