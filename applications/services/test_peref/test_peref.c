#include "test_peref.h"
#include <furi.h>
#include <furi_hal.h>

#include <furi_hal_resources.h>
#include <furi_hal_gpio.h>

#include <furi_hal_i2c.h>
#include <furi_hal_i2c_config.h>
#include <hardware/gpio.h>
#include <input/input.h>
#include <furi_hal_nvm.h>
#include <power/power.h>


#include <furi_hal_serial.h>
#include <cli/cli_vcp.h>
#include <FreeRTOS.h>
#include <task.h>

#define TAG "PerefTest"

void test_nvm(void) {
    FuriHalNvmStorage res;
    int32_t int_value = -123456;
    FuriString* str_value = furi_string_alloc();
    furi_string_set_str(str_value, "Hello, NVM!");

    // Test int32
    res = furi_hal_nvm_set_int32("int_key", int_value);
    FURI_LOG_I(TAG, "Set int32 result: %d", res);

    int32_t read_int_value = 0;
    res = furi_hal_nvm_get_int32("int_key", &read_int_value);
    FURI_LOG_I(TAG, "Get int32 result: %d, value_set: %ld value_get: %ld", res, int_value, read_int_value);

    // Test string
    res = furi_hal_nvm_set_str("str_key", str_value);
    FURI_LOG_I(TAG, "Set string result: %d", res);

    FuriString* read_str_value = furi_string_alloc();
    res = furi_hal_nvm_get_str("str_key", read_str_value);
    FURI_LOG_I(TAG, "Get string result: %d, value_set: %s value: %s", res, furi_string_get_cstr(str_value), furi_string_get_cstr(read_str_value));

    furi_string_free(str_value);
    furi_string_free(read_str_value);

    // Test delete
    res = furi_hal_nvm_get_int32("int_key", &read_int_value);
    FURI_LOG_I(TAG, "Delete int_key  result: %d, value_get: %ld", res, read_int_value);
    res = furi_hal_nvm_delete("int_key");
    FURI_LOG_I(TAG, "Delete int_key result: %d", res);

    // Try to get deleted key
    res = furi_hal_nvm_get_int32("int_key", &read_int_value);
    FURI_LOG_I(TAG, "Get deleted int_key result: %d", res);

    // Test UINT32
    uint32_t uint_value = 123456;
    res = furi_hal_nvm_set_uint32("uint_key", uint_value);
    FURI_LOG_I(TAG, "Set uint32 result: %d", res);
    uint32_t read_uint_value = 0;
    res = furi_hal_nvm_get_uint32("uint_key", &read_uint_value);
    FURI_LOG_I(TAG, "Get uint32 result: %d, value_set: %lu value_get: %lu", res, uint_value, read_uint_value);

    // test bool
    bool bool_value = true;
    res = furi_hal_nvm_set_bool("bool_key", bool_value);
    FURI_LOG_I(TAG, "Set bool result: %d", res);
    bool read_bool_value = false;
    res = furi_hal_nvm_get_bool("bool_key", &read_bool_value);
    FURI_LOG_I(TAG, "Get bool result: %d, value_set: %d value_get: %d", res, bool_value, read_bool_value);
}

void debug_task_stack_usage(void)
{
    UBaseType_t task_count = uxTaskGetNumberOfTasks();
    TaskStatus_t* task_array = pvPortMalloc(task_count * sizeof(TaskStatus_t));
    if(!task_array) {
        FURI_LOG_E(TAG, "Failed to allocate task status array");
        return;
    }

    uint32_t total_runtime = 0;
    UBaseType_t filled = uxTaskGetSystemState(task_array, task_count, &total_runtime);

    for(UBaseType_t i = 0; i < filled; i++) {
        uint32_t watermark = task_array[i].usStackHighWaterMark;
        if(watermark < 100) {
            FURI_LOG_W(TAG, "Task %-16s has only %lu words free!", task_array[i].pcTaskName, (unsigned long)watermark);
        } 
        // else {
        //     FURI_LOG_I(TAG, "Task %-16s stack free: %lu words", task_array[i].pcTaskName, (unsigned long)watermark);
        // }
    }

    vPortFree(task_array);
}

static void test_peref_top(FuriThreadList* thread_list) {
    furi_thread_enumerate(thread_list);

    uint32_t tick = furi_get_tick();
    uint32_t uptime = tick / furi_kernel_get_tick_frequency();

    FURI_LOG_I(
        TAG,
        "Uptime: %luh%lum%lus | Threads: %zu | ISR: %.2f%%",
        uptime / 3600,
        (uptime / 60) % 60,
        uptime % 60,
        furi_thread_list_size(thread_list),
        (double)furi_thread_list_get_isr_time(thread_list));

    FURI_LOG_I(
        TAG,
        "Heap: total=%zu (%zu KiB)  free=%zu (%zu KiB)  min_free=%zu (%zu KiB)  max_block=%zu",
        memmgr_get_total_heap(),
        memmgr_get_total_heap() / 1024,
        memmgr_get_free_heap(),
        memmgr_get_free_heap() / 1024,
        memmgr_get_minimum_free_heap(),
        memmgr_get_minimum_free_heap() / 1024,
        memmgr_heap_get_max_free_block());

    FURI_LOG_I(TAG, "%-18s %-5s %-10s %6s %6s %7s %5s",
               "Name", "Prio", "State", "Stack", "MinStk", "Heap", "%%CPU");

    for(size_t i = 0; i < furi_thread_list_size(thread_list); i++) {
        const FuriThreadListItem* item = furi_thread_list_get_at(thread_list, i);
        FURI_LOG_I(TAG, "%-18s %5d %-10s %6lu %6lu %7zu %4.1f",
                   item->name,
                   item->priority,
                   item->state,
                   item->stack_size,
                   item->stack_min_free,
                   item->heap,
                   (double)item->cpu);
    }

    }

int32_t test_peref_srv(void* p) {
    UNUSED(p);

    furi_log_set_level(FuriLogLevelDebug);
    FURI_LOG_T("tag", "Trace");
    FURI_LOG_D("tag", "Debug");
    FURI_LOG_I("tag", "Info");
    FURI_LOG_W("tag", "Warning");
    FURI_LOG_E("tag", "Error");

    uint8_t duty = 0;
    UNUSED(duty);

    //test_nvm();

    //Power* power = furi_record_open(RECORD_POWER);
    furi_delay_ms(2000);

    FuriThreadList* thread_list = furi_thread_list_alloc();

    while(true) {
        test_peref_top(thread_list);
        furi_delay_ms(1000);
    }

    furi_thread_list_free(thread_list);
    furi_crash();
}
