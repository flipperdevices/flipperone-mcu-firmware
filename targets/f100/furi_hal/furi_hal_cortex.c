#include <furi_hal_cortex.h>
#include <furi_hal.h>
#include <furi.h>

void furi_hal_cortex_init_early(void) {
    // Enable access to DWT via CoreDebug
    CoreDebug->DEMCR |= (CoreDebug_DEMCR_TRCENA_Msk | CoreDebug_DEMCR_MON_EN_Msk);

    // Reset and start the cycle counter
    DWT->CYCCNT = 0U;
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;
}
