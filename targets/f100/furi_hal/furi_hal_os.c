#include <furi_hal_os.h>
#include <furi.h>
#include <furi_hal_power.h>
#include <furi_hal_clock.h>

#include <FreeRTOS.h>
#include <sys/_intsup.h>
#include <task.h>

#include <hardware/uart.h>

#define TAG "FuriHalOs"

extern void SysTick_Handler(void);

void furi_hal_os_init(void) {
    FURI_LOG_I(TAG, "Init OK");
}

void furi_hal_os_tick(void) {
    if(xTaskGetSchedulerState() != taskSCHEDULER_NOT_STARTED) {
        SysTick_Handler();
    }
}
#include <hardware/uart.h>
#include <furi_hal_resources.h>
void vPortSuppressTicksAndSleep(TickType_t expected_idle_ticks) {
    furi_hal_gpio_write(&gpio_key_right, false);
    if(!furi_hal_power_sleep_available()) {
        
        __WFI();
        furi_hal_gpio_write(&gpio_key_right, true);
        return;
    }
    
    TickType_t unexpected_idle_ticks = expected_idle_ticks - 1;
    uint32_t completed_ticks = 0;
    furi_hal_clock_suspend_tick();
    __disable_irq();

    do {
        // Confirm OS that sleep is still possible
        if(eTaskConfirmSleepModeStatus() == eAbortSleep ) {  // nvic_hw->ispr[num/32] = 1 << (num % 32); || furi_hal_os_is_pending_irq()) {
            break;
        }
        
        //uart_tx_wait_blocking(uart0);
        completed_ticks = furi_hal_power_sleep(unexpected_idle_ticks);
        
        
        vTaskStepTick(completed_ticks);
    } while(0);
    // char buf[64];
    // snprintf(buf, sizeof(buf), "%d %d ticks\r\n", unexpected_idle_ticks, completed_ticks);
    // uart_write_blocking(uart0, (const uint8_t*)buf, strlen(buf));

    __enable_irq();
    furi_hal_clock_resume_tick();
    furi_hal_gpio_write(&gpio_key_right, true);
}
