#include <furi_hal_os.h>
#include <furi.h>
#include <furi_hal_power.h>

#include <FreeRTOS.h>
#include <task.h>

#define TAG "FuriHalOs"

extern void SysTick_Handler(void);

void furi_hal_os_init(void) {
    //ToDO: add any os specific initializations if needed
    //FURI_LOG_I(TAG, "Init OK");
}

void furi_hal_os_tick(void) {
    if(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        SysTick_Handler();
    }
}

void vPortSuppressTicksAndSleep(TickType_t expected_idle_ticks) {
    UNUSED(expected_idle_ticks);
    if(!furi_hal_power_sleep_available()) {
        __WFI();
        return;
    }
}
