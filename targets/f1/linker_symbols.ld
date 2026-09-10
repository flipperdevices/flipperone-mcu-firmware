/*
 * Project-level linker symbols computed at link time.
 *
 * __heap_start / __heap_end are PROVIDE'd by the pico-sdk memmap_default.ld:
 *   __heap_start  = __end__                        (first byte after .bss)
 *   __heap_end    = ORIGIN(RAM) + LENGTH(RAM)      (0x20080000 for 512 KiB RAM)
 *
 * The main-core stack lives in SCRATCH_Y (a separate 4 KiB SRAM bank,
 * 0x20081000..0x20082000) and does NOT overlap the heap in main RAM.
 * SIZEOF(.stack_dummy) preserves the existing deduction that was in
 * furi_hal_memory_init(); remove it if you confirm it is no longer needed.
 */

__heap_start__ = __heap_start;
__heap_end__   = __heap_end;
__heap_size__  = __heap_end - __heap_start - SIZEOF(.stack_dummy);
