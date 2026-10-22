#include <furi_hal_cpu.h>
#include <hardware/clocks.h>


#define MICROSECONDS_IN_SECOND (1000000UL)

uint32_t furi_hal_cpu_get_frequency(void) {
    return clock_get_hz(clk_sys);
}

uint32_t furi_hal_cpu_get_cycle_count(void) {
    return DWT->CYCCNT;
}

uint32_t furi_hal_cpu_get_cycles_per_us(void) {
    return clock_get_hz(clk_sys) / MICROSECONDS_IN_SECOND;
}
