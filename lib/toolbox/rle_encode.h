/**
 * @brief Run-Length encoding and decoding.
 * The format is described in rle_encode.c.
 */

#pragma once

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

/** Encodes data from source buffer using RLE algorithm and place new data to destination buffer.
 *
 * @param[in]  src           Pointer to source containing data
 * @param[in]  src_len       Total length of src buffer in bytes
 * @param[in,out]  dest      Pointer to destination buffer
 * @param[in]  dest_len      Total length of dest buffer. Attention! Size of dest buffer must exceed size of src buffer.
 * @param[in]  blk_size      Size of block RLE checks to consider whether it is equal to the next or not.
 * @param[out] result_len    Actual dest buffer data size.
 *
 * @return True if compression finished successfully, otherwise false
 */
bool rle_compress(
    const uint8_t* src,
    size_t src_len,
    uint8_t* dest,
    size_t dest_len,
    size_t blk_size,
    size_t* result_len);

/** Decodes data from source buffer using RLE algorithm and place new data to destination buffer.
 *
 * @param[in]  src           Pointer to source containing compressed data
 * @param[in]  src_len       Total length of data in src buffer in bytes
 * @param[in,out]  dest      Pointer to destination buffer
 * @param[in]  dest_len      Total length of dest buffer. Attention! Size of dest buffer must exceed size of src buffer.
 * @param[in]  blk_size      `blk_size` parameter that the block was compressed with
 * @param[out] result_len    Actual dest buffer data size.
 *
 * @return True if decompression finished successfully, otherwise false
 */
bool rle_decompress(
    const uint8_t* src,
    size_t src_len,
    uint8_t* dest,
    size_t dest_len,
    size_t blk_size,
    size_t* result_len);
