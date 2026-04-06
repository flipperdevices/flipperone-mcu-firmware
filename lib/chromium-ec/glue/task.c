#include <furi.h>
#include "task.h"

#define TASK_ARRAY_SIZE 256
FuriThread* task_to_thread_array[TASK_ARRAY_SIZE] = {0};

static FuriThread* task_to_thread(task_id_t task_id) {
    FuriThread* thread = task_to_thread_array[task_id];
    furi_check(thread);
    return thread;
}

void task_set_event(task_id_t task_id, uint32_t event) {
    furi_thread_flags_set(task_to_thread(task_id), event);
}

uint32_t task_wait_event(int timeout_us) {
    uint32_t timeout_ms = timeout_us / 1000;
    furi_check(timeout_ms > 0);
    uint32_t value = furi_thread_flags_wait(0xFFFFFFFF, FuriFlagWaitAny, timeout_ms);
    return value;
}

void task_wake(task_id_t task_id) {
    furi_thread_resume(task_to_thread(task_id));
}

void mutex_lock(mutex_t* mtx) {
    furi_check(mtx);
    furi_mutex_acquire(*mtx, FuriWaitForever);
}

void mutex_unlock(mutex_t* mtx) {
    furi_check(mtx);
    furi_mutex_release(*mtx);
}