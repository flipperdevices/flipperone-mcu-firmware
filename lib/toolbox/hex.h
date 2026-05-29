#pragma once
#include <stdint.h>
#include <stdbool.h>
#include <core/string.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Convert ASCII hex value to nibble
 * @param c         ASCII character
 * @param nibble    nibble pointer, output
 *
 * @return          bool conversion status
 */
bool hex_char_to_hex_nibble(char c, uint8_t* nibble);

/** Convert ASCII hex value to byte
 * @param hi        hi nibble text
 * @param low       low nibble text
 * @param value     output value
 *
 * @return          bool conversion status
 */
bool hex_char_to_uint8(char hi, char low, uint8_t* value);

/** Convert ASCII hex values to uint8_t
 * @param value_str ASCII data
 * @param value     output value
 *
 * @return          bool conversion status
 */
bool hex_chars_to_uint8(const char* value_str, uint8_t* value);

/** Convert ASCII hex values to uint64_t
 * @param value_str ASCII 64 bi data
 * @param value     output value
 *
 * @return          bool conversion status
 */
bool hex_chars_to_uint64(const char* value_str, uint64_t* value);

/** Convert uint8_t to ASCII hex values
 * @param src       source data
 * @param target    output value
 * @param length    data length
 * 
 */
void uint8_to_hex_chars(const uint8_t* src, uint8_t* target, int length);

/** Convert bytes to hex string
 * @param bytes         source bytes
 * @param bytes_length  length of source bytes
 * @param string        output string
 */
void hex_bytes_to_string(const uint8_t* bytes, size_t bytes_length, FuriString* string);

/** Convert hex string to bytes
 * @param string        source string
 * @param bytes         output bytes
 * @param bytes_count   max bytes to write
 * @param bytes_written actual bytes written
 *
 * @return              bool conversion status
 */
bool hex_string_to_bytes(
    const FuriString* string,
    uint8_t* bytes,
    size_t bytes_count,
    size_t* bytes_written);

#ifdef __cplusplus
}
#endif
