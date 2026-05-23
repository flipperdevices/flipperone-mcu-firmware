#include <furi_hal_os.h>
#include <furi.h>
#include <furi_hal_power.h>
#include <furi_hal_clock.h>
#include <FreeRTOS.h>
#include <stdbool.h>
#include <task.h>
#include <furi_hal_debug.h>
#include <furi_hal_gpio.h>
#include <furi_hal_resources.h>

#define TAG "FuriHalOs"

extern void SysTick_Handler(void);

void furi_hal_os_init(void) {
    // Disable NBOOT control with the RTT button
    furi_hal_gpio_init_simple(&gpio_nboot_disable, GpioModeOutputPushPull);
    furi_hal_gpio_write(&gpio_nboot_disable, true);

    furi_hal_gpio_init_simple(&gpio_m40, GpioModeOutputPushPull);
    furi_hal_gpio_write(&gpio_m40, false);
    furi_hal_gpio_init_simple(&gpio_m41, GpioModeOutputPushPull);
    furi_hal_gpio_write(&gpio_m41, false);
    FURI_LOG_I(TAG, "Init OK");
}

void furi_hal_os_tick(void) {
    if(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        SysTick_Handler();
    }
}

static inline bool furi_hal_os_deep_sleep_available(void) {
    //todo: Add a check for enabling/disabling deep sleep mode
    return !furi_hal_debug_is_gdb_session_active();
}

static inline void furi_hal_power_light_sleep(void) {
    __WFI();
}

void vPortSuppressTicksAndSleep(TickType_t expected_idle_ticks) {
    if(!furi_hal_power_sleep_available()) {
        __WFI();
    } else if(!furi_hal_os_deep_sleep_available()) {
        furi_hal_power_light_sleep();
    } else {
        TickType_t unexpected_idle_ticks = expected_idle_ticks - 1;
        uint32_t completed_ticks = 0;
        furi_hal_clock_suspend_tick();
        __disable_irq();

        do {
            // Confirm OS that sleep is still possible
            if(eTaskConfirmSleepModeStatus() == eAbortSleep) { // nvic_hw->ispr[num/32] = 1 << (num % 32); || furi_hal_os_is_pending_irq()) {
                break;
            }

            completed_ticks = furi_hal_power_deep_sleep(unexpected_idle_ticks);

            vTaskStepTick(completed_ticks);
        } while(0);

        __enable_irq();
        furi_hal_clock_resume_tick();
    }
}

void vApplicationStackOverflowHook(TaskHandle_t xTask, char* pcTaskName) {
    /* Log the task name and stack boundaries for easier debugging */
    TaskStatus_t status;
    vTaskGetInfo(xTask, &status, pdTRUE, eRunning);
    char buf[256];
    snprintf(
        buf,
        sizeof(buf),
        "\r\n[E][" TAG "] stack overflow in task '%s' (handle=0x%08x) "
        "stack_base=0x%08x stack_min_free=%u\r\n",
        pcTaskName,
        (unsigned)xTask,
        (unsigned)status.pxStackBase,
        (unsigned)(status.usStackHighWaterMark * sizeof(StackType_t)));
    furi_log_puts(buf);
    furi_crash("Stack overflow");
}

__attribute__((__noreturn__)) void vFreeRTOSAssertFailed(const char* expr, const char* file, int line) {
    /* Called from within vTaskSwitchContext (PendSV context).
     * uxTaskGetSystemState() is unsafe here – just log the expression and crash.
     * The standard HardFault handler will print registers/stack. */
    char buf[256];
    snprintf(buf, sizeof(buf), "\r\n[E][" TAG "] FreeRTOS assert FAILED: (%s) at %s:%d\r\n", expr, file, line);
    furi_log_puts(buf);
    furi_crash("FreeRTOS Assert");
}
