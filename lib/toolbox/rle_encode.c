#include "rle_encode.h"
#include <furi.h>
#include <pico.h>

/*
 * Format specification
 *
 * The input buffer for compression is viewed as a series of `blk_size` blocks:
 * Data:   |DE AD BE|EF DE AD|BE EF DE|
 * Blocks: |        |        |        | (for `blk_size` = 3)
 * 
 * The very first byte is an opcode. If the highest bit is set, several
 * following blocks are encoded verbatim. The count is stored in the lower 7
 * bits of the opcode:
 * Source:        |DE AD BE|EF DE AD|BE EF DE|
 * Blocks:        |        |        |        |
 * Destination: 83|DE AD BE|EF DE AD|BE EF DE|
 * 
 * If the highest bit of the opcode is reset, only 1 block follows it, which
 * should be repeated as many times as indicated by the lower 7 bits:
 * Source:        |DE AD BE|DE AD BE|DE AD BE|
 * Blocks:        |        |        |        |
 * Destination: 03|DE AD BE|
 */

#define MAX_BLOCKS_PER_BYTE (127)
#define RLE_BLOCK_THRESHOLD (3)

static inline size_t get_repeat_count(const uint8_t* data, size_t data_len, size_t blk_size) {
    if(data_len < blk_size) return 0;

    const size_t max_repeats = data_len / blk_size;
    size_t count = 1;
    for(size_t i = count; i < max_repeats && count < MAX_BLOCKS_PER_BYTE; i++) {
        if(memcmp(data, data + i * blk_size, blk_size) != 0) break;
        count++;
    }

    return count;
}

static inline size_t
    get_nonrepeat_count(const uint8_t* data, size_t data_len, size_t blk_size, size_t threshold) {
    if(data_len < blk_size) return 0;

    size_t nonrepeat_count = 0;
    size_t repeat_count = 0;
    size_t index = 0;

    while(true) {
        // The last block has no next one to repeat
        const bool has_next = index + 2 * blk_size <= data_len;
        if(has_next && memcmp(data + index, data + index + blk_size, blk_size) == 0) {
            repeat_count += 1;
            if(repeat_count > threshold) break;
        } else {
            nonrepeat_count += 1 + repeat_count;
            repeat_count = 0;
            if(nonrepeat_count >= MAX_BLOCKS_PER_BYTE) {
                nonrepeat_count = MAX_BLOCKS_PER_BYTE;
                break;
            }
        }

        index += blk_size;
        if(index >= data_len) {
            nonrepeat_count += repeat_count;
            break;
        }
    }

    return nonrepeat_count;
}

/*
 * rle_compress() for one-byte blocks, with the same output. The generic code
 * calls memcmp() and multiplies by blk_size for every byte, which makes it
 * several times slower on a whole frame.
 */
static bool __not_in_flash_func(rle_compress_bytes)(
    const uint8_t* src,
    size_t src_len,
    uint8_t* dest,
    size_t dest_len,
    size_t* result_len) {
    size_t index = 0;
    size_t dest_index = 0;

    while(index < src_len) {
        const uint8_t value = src[index];
        const size_t run_limit = MIN(src_len - index, (size_t)MAX_BLOCKS_PER_BYTE);
        size_t repeat_count = 1;
        while(repeat_count < run_limit && src[index + repeat_count] == value) {
            repeat_count++;
        }

        if(repeat_count >= RLE_BLOCK_THRESHOLD) {
            if(dest_index + 2 >= dest_len) {
                *result_len = 0;
                return false;
            }
            dest[dest_index++] = (uint8_t)repeat_count;
            dest[dest_index++] = value;
            index += repeat_count;
            continue;
        }

        // Verbatim up to a run longer than the threshold, as get_nonrepeat_count() counts it
        size_t nonrepeat_count = 0;
        size_t pending = 0;
        for(size_t i = index;; i++) {
            if(i + 1 < src_len && src[i] == src[i + 1]) {
                if(++pending > RLE_BLOCK_THRESHOLD) break;
            } else {
                nonrepeat_count += 1 + pending;
                pending = 0;
                if(nonrepeat_count >= MAX_BLOCKS_PER_BYTE) {
                    nonrepeat_count = MAX_BLOCKS_PER_BYTE;
                    break;
                }
            }
            if(i + 1 >= src_len) break;
        }

        if(dest_index + nonrepeat_count + 1 >= dest_len) {
            *result_len = 0;
            return false;
        }
        dest[dest_index++] = (uint8_t)(nonrepeat_count | 0x80);
        memcpy(dest + dest_index, src + index, nonrepeat_count);
        dest_index += nonrepeat_count;
        index += nonrepeat_count;
    }

    *result_len = dest_index;
    return true;
}

bool __not_in_flash_func(rle_compress)(
    const uint8_t* src,
    size_t src_len,
    uint8_t* dest,
    size_t dest_len,
    size_t blk_size,
    size_t* result_len) {
    furi_assert(src);
    furi_assert(dest);

    if(blk_size == 1) return rle_compress_bytes(src, src_len, dest, dest_len, result_len);

    size_t index = 0;
    size_t dest_index = 0;

    bool error = false;
    while(index < src_len) {
        size_t remaining = src_len - index;
        size_t repeat_count = get_repeat_count(src + index, remaining, blk_size);

        if(repeat_count == 0) break;

        if(repeat_count < RLE_BLOCK_THRESHOLD) {
            size_t non_repeat_count =
                get_nonrepeat_count(src + index, remaining, blk_size, RLE_BLOCK_THRESHOLD);
            uint8_t ctrl_byte = (uint8_t)(non_repeat_count | 0x80);

            size_t byte_size = non_repeat_count * blk_size;
            if(dest_index + byte_size + 1 >= dest_len) {
                error = true;
                break;
            }

            dest[dest_index++] = ctrl_byte;
            memcpy(dest + dest_index, src + index, byte_size);
            dest_index += byte_size;
            index += byte_size;
        } else {
            uint8_t ctrl_byte = (uint8_t)repeat_count;

            if(dest_index + blk_size + 1 >= dest_len) {
                error = true;
                break;
            }

            dest[dest_index++] = ctrl_byte;
            memcpy(dest + dest_index, src + index, blk_size);
            dest_index += blk_size;
            index += repeat_count * blk_size;
        }
    }

    *result_len = error ? 0 : dest_index;
    return !error;
}
