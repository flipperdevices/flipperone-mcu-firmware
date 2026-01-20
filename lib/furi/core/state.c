#include "state.h"
#include "kernel.h"
#include "check.h"
#include "mutex.h"

#include <m-i-list.h>

#define MUTEX_TIMEOUT (furi_ms_to_ticks(5000))

struct FuriStateSub {
    FuriState* state;
    FuriStateCallback callback;
    void* context;

    ILIST_INTERFACE(StateSubList, FuriStateSub);
};

ILIST_DEF(StateSubList, FuriStateSub, M_POD_OPLIST);
#define M_OPL_StateSubList_t() ILIST_OPLIST(StateSubList, M_POD_OPLIST)

struct FuriState {
    FuriMutex* mutex;
    StateSubList_t sub_list;
    size_t item_size;
    uint8_t item[];
};

// ===========================
// Publisher (state owner) API
// ===========================

static inline void furi_state_lock(FuriState* state) {
    furi_assert(state);
    furi_check(furi_mutex_acquire(state->mutex, MUTEX_TIMEOUT) == FuriStatusOk);
}

static inline void furi_state_unlock(FuriState* state) {
    furi_assert(state);
    furi_check(furi_mutex_release(state->mutex) == FuriStatusOk);
}

FuriState* furi_state_alloc(size_t item_size) {
    furi_check(item_size > 0);
    FuriState* state = malloc(sizeof(FuriState) + item_size);
    state->mutex = furi_mutex_alloc(FuriMutexTypeNormal);
    StateSubList_init(state->sub_list);
    state->item_size = item_size;
    return state;
}

void furi_state_free(FuriState* state) {
    furi_check(state);

    furi_state_lock(state);
    furi_check(StateSubList_empty_p(state->sub_list));
    furi_state_unlock(state);

    furi_mutex_free(state->mutex);
    StateSubList_clear(state->sub_list);
    free(state);
}

void furi_state_set(FuriState* state, const void* item) {
    furi_check(state);
    furi_check(item);

    furi_state_lock(state);

    memcpy(state->item, item, state->item_size);
    for
        M_EACH(subscriber, state->sub_list, StateSubList_t) {
            subscriber->callback(state->item, subscriber->context);
        }

    furi_state_unlock(state);
}

// ===============================
// Subscriber (state consumer) API
// ===============================

/**
 * @warning assumes locked FuriState
 */
static FuriStateSub*
    furi_state_sub_alloc(FuriState* state, FuriStateCallback callback, void* context) {
    furi_assert(state);
    furi_assert(callback);

    FuriStateSub* sub = malloc(sizeof(FuriStateSub));
    sub->callback = callback;
    sub->context = context;
    sub->state = state;

    StateSubList_push_back(state->sub_list, sub);

    return sub;
}

FuriStateSub* furi_state_subscribe(FuriState* state, FuriStateCallback callback, void* context) {
    furi_check(state);
    furi_check(callback);

    furi_state_lock(state);

    FuriStateSub* sub = furi_state_sub_alloc(state, callback, context);
    callback(state->item, context);

    furi_state_unlock(state);

    return sub;
}

FuriStateSub* furi_state_get_subscribe(
    FuriState* state,
    void* item_out,
    FuriStateCallback callback,
    void* context) {
    furi_check(state);
    furi_check(callback);

    furi_state_lock(state);

    FuriStateSub* sub = furi_state_sub_alloc(state, callback, context);
    if(item_out) memcpy(item_out, state->item, state->item_size);

    furi_state_unlock(state);

    return sub;
}

void furi_state_unsubscribe(FuriStateSub* sub) {
    furi_check(sub);
    FuriState* state = sub->state;

    furi_state_lock(state);

    StateSubList_unlink(sub);

    furi_state_unlock(state);

    free(sub);
}

void furi_state_get(FuriStateSub* sub, void* item_out) {
    furi_check(sub);
    furi_check(item_out);
    FuriState* state = sub->state;

    furi_state_lock(state);
    memcpy(item_out, state->item, state->item_size);
    furi_state_unlock(state);
}
