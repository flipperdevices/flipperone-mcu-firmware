#include <furi.h>
#include <furi_hal_dma.h>
#include "hardware/dma.h"


void furi_hal_dma_init_early(void) {

}

void furi_hal_dma_deinit_early(void) {
    
}

bool furi_hal_dma_allocate_channel(uint32_t* channel) {
    FURI_CRITICAL_ENTER();
    *channel = dma_claim_unused_channel(true);

    FURI_CRITICAL_EXIT();
    return dma_channel_is_claimed(*channel);
}

void furi_hal_dma_free_channel(uint32_t channel) {
    FURI_CRITICAL_ENTER();
    dma_channel_unclaim(channel);
    FURI_CRITICAL_EXIT();
}


