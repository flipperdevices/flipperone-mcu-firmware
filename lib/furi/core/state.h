/**
 * @file state.h
 * Keeps track of a value representing sone kind of state. Combines the idea of
 * a state and state updates into a single logical unit. Lets you query the
 * state and follow its updates without subscription race conditions.
 */

#pragma once

#include "base.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief Opaque state handle */
typedef struct FuriState FuriState;

/** @brief Opaque subscription handle */
typedef struct FuriStateSub FuriStateSub;

/**
 * @brief Callback with an update to the state
 * 
 * @param[in] item New state
 * @param[in] context Custom context
 * 
 * @note Assume that the callback is called from a different thread.
 * @note Do your thing in the callback quickly to avoid holding up the caller.
 * @note You must not call any APIs of this `FuriState` from within the
 *       callback.
 */
typedef void (*FuriStateCallback)(const void* item, void* context);

// ===========================
// Publisher (state owner) API
// ===========================

/**
 * @brief Allocates a FuriState
 * 
 * @param[in] item_size Size of the contained item
 * 
 * @returns State handle
 * 
 * @note The stored state will be initialized to all zeroes. Please call
 *       `furi_state_set` before giving out the handle to your subscribers.
 */
FuriState* furi_state_alloc(size_t item_size);

/**
 * @brief Releases a FuriState
 * 
 * @param[in] state State handle
 * 
 * @note All subscriptions must be cancelled by this point
 */
void furi_state_free(FuriState* state);

/**
 * @brief Updates the contained state and notifies all subscribers
 * 
 * @param[in] state State handle
 * @param[in] item Item to copy data from
 */
void furi_state_set(FuriState* state, const void* item);

// ===============================
// Subscriber (state consumer) API
// ===============================

/**
 * @brief Calls the callback with the current state and subscribes to future
 *        state updates
 * 
 * @param[in] state State handle
 * @param[in] callback Callback for state updates. Also gets called once
 *                     initially with the current state.
 * @param[in] context Context for callback. May be NULL.
 * 
 * @returns Subscription handle
 */
FuriStateSub* furi_state_subscribe(FuriState* state, FuriStateCallback callback, void* context);

/**
 * @brief Atomically gets the current state and subscribes to future updates
 * 
 * @param[in] state State handle
 * @param[out] item_out Where to write the current state. May be NULL.
 * @param[in] callback Callback for future state updates. Unlike
 *                     `furi_state_subscribe`, doesn't get called with the
 *                     initial state.
 * @param[in] context Context for callback. May be NULL.
 * 
 * @returns Subscription handle
 */
FuriStateSub* furi_state_get_subscribe(
    FuriState* state,
    void* item_out,
    FuriStateCallback callback,
    void* context);

/**
 * @brief Unsubscribes from any future state updates
 * 
 * @param[in] sub Subscription handle
 */
void furi_state_unsubscribe(FuriStateSub* sub);

/**
 * @brief Gets the current state
 * 
 * @param[in] sub Subscription handle
 * @param[out] item_out Where to write the current state
 */
void furi_state_get(FuriStateSub* sub, void* item_out);

#ifdef __cplusplus
}
#endif
