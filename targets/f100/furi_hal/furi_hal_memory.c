#include <furi_hal_memory.h>
#include <furi.h>

#include <rp2350_linker.h>
#include <pico.h>

static FuriHalMemoryRegion memory_regions[] = {
    {
        .start = (void*)&__heap_start,
        .size_bytes = 0,
    },
};

void furi_hal_memory_init(void) {
    memory_regions[0].size_bytes = (size_t)&__heap_end - (size_t)&__heap_start - PICO_STACK_SIZE;
}

void* furi_hal_memory_alloc(size_t size) {
    UNUSED(size);
    // TODO: implement
    return NULL;
}

size_t furi_hal_memory_get_free(void) {
    // TODO: implement
    return 0;
}

size_t furi_hal_memory_max_pool_block(void) {
    // TODO: implement
    return 0;
}

uint32_t furi_hal_memory_get_region_count(void) {
    return COUNT_OF(memory_regions);
}

const FuriHalMemoryRegion* furi_hal_memory_get_region(uint32_t index) {
    furi_check(index < COUNT_OF(memory_regions));
    return &memory_regions[index];
}

void furi_hal_memory_set_heap_track_mode(FuriHalMemoryHeapTrackMode mode) {
    UNUSED(mode);
    // TODO: implement
}

FuriHalMemoryHeapTrackMode furi_hal_memory_get_heap_track_mode(void) {
    // TODO: implement
    return FuriHalMemoryHeapTrackModeNone;
}
