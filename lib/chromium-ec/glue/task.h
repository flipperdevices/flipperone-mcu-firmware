#pragma once
#include <furi.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Task event bitmasks */
/* Tasks may use the bits in TASK_EVENT_CUSTOM_BIT for their own events */
#define TASK_EVENT_CUSTOM_BIT(x) BIT(x)

#define K_MUTEX_DEFINE(name) mutex_t name = NULL

typedef FuriMutex* mutex_t;

/* Task identifier (8 bits) */
typedef uint8_t task_id_t;

/**
 * Lock a mutex.
 *
 * This tries to lock the mutex mtx.  If the mutex is already locked by another
 * task, de-schedules the current task until the mutex is again unlocked.
 *
 * Must not be used in interrupt context!
 */
void mutex_lock(mutex_t* mtx);

/**
 * Release a mutex previously locked by the same task.
 */
void mutex_unlock(mutex_t* mtx);

/**
 * Set a task event.
 *
 * If the task is higher priority than the current task, this will cause an
 * immediate context switch to the new task.
 *
 * Can be called both in interrupt context and task context.
 *
 * @param tskid		Task to set event for
 * @param event		Event bitmap to set (TASK_EVENT_*)
 */
void task_set_event(task_id_t tskid, uint32_t event);

/**
 * Wait for the next event.
 *
 * If one or more events are already pending, returns immediately.  Otherwise,
 * it de-schedules the calling task and wakes up the next one in the priority
 * order.  Automatically clears the bitmap of received events before returning
 * the events which are set.
 *
 * @param timeout_us	If > 0, sets a timer to produce the TASK_EVENT_TIMER
 *			event after the specified micro-second duration.
 *
 * @return The bitmap of received events.
 */
uint32_t task_wait_event(int timeout_us);

#ifdef __cplusplus
}
#endif
