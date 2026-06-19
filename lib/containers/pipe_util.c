#include "pipe_util.h"

static void kmp_build_failure(const char* pattern, size_t len, size_t* failure) {
    failure[0] = 0;
    for(size_t i = 1; i < len; i++) {
        size_t j = failure[i - 1];
        while(j > 0 && pattern[i] != pattern[j]) {
            j = failure[j - 1];
        }
        failure[i] = (pattern[i] == pattern[j]) ? j + 1 : 0;
    }
}

bool pipe_copy_until(PipeSide* source, PipeSide* dest, const char* terminator) {
    furi_check(source);
    furi_check(terminator);
    const size_t terminator_len = strlen(terminator);
    furi_check(terminator_len > 0);

    size_t failure[terminator_len];
    kmp_build_failure(terminator, terminator_len, failure);

    size_t matched_cnt = 0;

    while(1) {
        char c;
        if(pipe_receive(source, &c, sizeof(c)) != sizeof(c)) return false;

        while(matched_cnt > 0 && c != terminator[matched_cnt]) {
            size_t new_matched = failure[matched_cnt - 1];
            if(dest) {
                size_t flush_count = matched_cnt - new_matched;
                if(pipe_send(dest, terminator, flush_count) != flush_count) return false;
            }
            matched_cnt = new_matched;
        }

        if(c == terminator[matched_cnt]) {
            matched_cnt++;
            if(matched_cnt == terminator_len) return true;
        } else {
            if(dest) {
                if(pipe_send(dest, &c, sizeof(c)) != sizeof(c)) return false;
            }
        }
    }
}

PipeDiscardUntilResult pipe_discard_until_either(
    PipeSide* source,
    const char* const* terminators,
    size_t num_terminators) {
    furi_check(source);
    furi_check(terminators);

    size_t* terminator_lens = malloc(sizeof(size_t) * num_terminators);
    {
        bool found = false;
        size_t i = 0;
        for(; !found && i != num_terminators; ++i) {
            const size_t terminator_len = strlen(terminators[i]);
            if(terminator_len == 0) {
                found = true;
            }
            terminator_lens[i] = terminator_len;
        }
        if(found) {
            free(terminator_lens);
            return (PipeDiscardUntilResult){
                .success = true,
                .found_idx = i - 1,
            };
        }
    }

    size_t** failures = malloc(sizeof(size_t*) * num_terminators);
    for(size_t i = 0; i != num_terminators; ++i) {
        const size_t terminator_len = terminator_lens[i];
        failures[i] = malloc(sizeof(size_t) * terminator_len);
        kmp_build_failure(terminators[i], terminator_len, failures[i]);
    }

    size_t* matched_cnts = malloc(sizeof(size_t) * num_terminators);
    bzero(matched_cnts, sizeof(size_t) * num_terminators);

    bool success = false;
    size_t found_idx = 0;
    do {
        char c;
        if(pipe_receive(source, &c, sizeof(c)) != sizeof(c)) {
            break;
        }

        for(size_t i = 0; i != num_terminators; ++i) {
            size_t* matched_cnt = matched_cnts + i;
            const char* terminator = terminators[i];
            size_t* failure = failures[i];
            size_t terminator_len = terminator_lens[i];
            while(*matched_cnt > 0 && c != terminator[*matched_cnt]) {
                *matched_cnt = failure[*matched_cnt - 1];
            }

            if(c == terminator[*matched_cnt]) {
                (*matched_cnt)++;
                if(*matched_cnt == terminator_len) {
                    success = true;
                    found_idx = i;
                    break;
                }
            }
        }
    } while(!success);
    for(size_t i = 0; i != num_terminators; ++i) {
        free(failures[i]);
    }
    free(failures);
    free(terminator_lens);
    free(matched_cnts);
    return (PipeDiscardUntilResult){
        .success = success,
        .found_idx = found_idx,
    };
}
