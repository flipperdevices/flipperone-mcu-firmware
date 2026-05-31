/**
 * @file pipe_util.h
 * @brief Pipe utilities
 */

#pragma once

#include "pipe.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct PipeDiscardUntilResult {
    bool success;
    size_t found_idx;
} PipeDiscardUntilResult;

/**
 * @brief Copies data from one pipe to another until a specific sequence is met.
 * 
 * @param[in] source     Pipe to copy data from
 * @param[in] dest       Pipe to copy data into. May be NULL
 * @param[in] terminator Terminator sequence
 * 
 * @returns `true` if terminator sequence found, `false` if one of the pipes was
 *          closed prematurely.
 */
bool pipe_copy_until(PipeSide* source, PipeSide* dest, const char* terminator);

/**
 * @brief Discards data from a pipe until (including) any of specific sequences is met.
 *
 * @param[in] source      Pipe to read data from
 * @param[in] terminators Terminator sequences
 * @param[in] num_terminators Number of terminator sequences
 *
 */
PipeDiscardUntilResult pipe_discard_until_either(
    PipeSide* source,
    const char* const* terminators,
    size_t num_terminators);

#ifdef __cplusplus
}
#endif
