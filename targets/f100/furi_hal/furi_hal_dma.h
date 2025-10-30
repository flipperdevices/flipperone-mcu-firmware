#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/** Early initialization */
void furi_hal_dma_init_early(void);

/** Early de-initialization */
void furi_hal_dma_deinit_early(void);

/** Allocate DMA channel */
bool furi_hal_dma_allocate_channel(uint32_t* channel);

/** Free DMA channel */
void furi_hal_dma_free_channel(uint32_t channel);

#ifdef __cplusplus
}
#endif
