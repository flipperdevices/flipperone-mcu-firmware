#include "args.h"
#include <toolbox/hex.h>
#include <toolbox/strint.h>

size_t args_get_first_word_length(FuriString* args) {
    size_t ws = furi_string_search_char(args, ' ');
    if(ws == FURI_STRING_FAILURE) {
        ws = furi_string_size(args);
    }

    return ws;
}

size_t args_length(FuriString* args) {
    return furi_string_size(args);
}

bool args_read_int_and_trim(FuriString* args, int* value) {
    size_t cmd_length = args_get_first_word_length(args);

    if(cmd_length == 0) {
        return false;
    }

    int32_t temp;
    if(strint_to_int32(furi_string_get_cstr(args), NULL, &temp, 10) == StrintParseNoError) {
        *value = temp;
        furi_string_right(args, cmd_length);
        furi_string_trim(args);
        return true;
    }

    return false;
}

bool args_read_string_and_trim(FuriString* args, FuriString* word) {
    size_t cmd_length = args_get_first_word_length(args);

    if(cmd_length == 0) {
        return false;
    }

    furi_string_set_n(word, args, 0, cmd_length);
    furi_string_right(args, cmd_length);
    furi_string_trim(args);

    return true;
}

bool args_read_probably_quoted_string_and_trim(FuriString* args, FuriString* word) {
    if(furi_string_size(args) > 1 && furi_string_get_char(args, 0) == '\"') {
        size_t second_quote_pos = furi_string_search_char(args, '\"', 1);

        if(second_quote_pos == 0) {
            return false;
        }

        furi_string_set_n(word, args, 1, second_quote_pos - 1);
        furi_string_right(args, second_quote_pos + 1);
        furi_string_trim(args);
        return true;
    } else {
        return args_read_string_and_trim(args, word);
    }
}

bool args_char_to_hex(char hi_nibble, char low_nibble, uint8_t* byte) {
    return hex_char_to_uint8(hi_nibble, low_nibble, byte);
}

bool args_read_hex_bytes(FuriString* args, uint8_t* bytes, size_t bytes_count) {
    const size_t expected_length = bytes_count * 2;
    const size_t word_length = args_get_first_word_length(args);

    if(word_length != expected_length) {
        return false;
    }

    if(bytes_count == 0) {
        return true;
    }

    FuriString* hex_word = furi_string_alloc();
    furi_string_set_n(hex_word, args, 0, word_length);

    size_t parsed = 0;
    bool success = hex_string_to_bytes(hex_word, bytes, bytes_count, &parsed);

    furi_string_free(hex_word);

    return success && (parsed == bytes_count);
}
