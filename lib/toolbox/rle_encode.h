/**
 * @brief Run-Length encoding.
 * The format is described in rle_encode.c.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/** Encodes data from source buffer using RLE algorithm and place new data to destination buffer.
 *
 * @param[in]  src           Pointer to source containing data
 * @param[in]  src_len       Total length of src buffer in bytes, a multiple of blk_size
 * @param[in,out]  dest      Pointer to destination buffer
 * @param[in]  dest_len      Total length of dest buffer
 * @param[in]  blk_size      Size of block RLE checks to consider whether it is equal to the next or not.
 * @param[out] result_len    Actual dest buffer data size.
 *
 * @return False unless the encoded data is shorter than dest_len
 */
bool rle_compress(
    const uint8_t* src,
    size_t src_len,
    uint8_t* dest,
    size_t dest_len,
    size_t blk_size,
    size_t* result_len);
