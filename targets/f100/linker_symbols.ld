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

/*
 * picolibc thread-local storage geometry.
 *
 * pico-sdk's memmap_default.ld emits .tdata/.tbss and defines __tls_base,
 * __tls_end and __tdata_end, but not the size/offset symbols that picolibc's
 * _init_tls()/_tls_size() need (it even computes __tls_size_align from an
 * __tls_size it never defines). Values follow picolibc's own picolibc.ld.
 *
 * PROVIDE keeps these inert for newlib builds, where nothing references them.
 */
PROVIDE(__tdata_start  = ADDR(.tdata));
PROVIDE(__tdata_source = LOADADDR(.tdata));
PROVIDE(__tdata_size   = SIZEOF(.tdata));
PROVIDE(__tls_size     = SIZEOF(.tdata) + SIZEOF(.tbss));
PROVIDE(__tls_start    = SIZEOF(.tdata) ? ADDR(.tdata) : ADDR(.tbss));
PROVIDE(__tbss_offset  = ADDR(.tbss) - __tls_start);
PROVIDE(__tbss_size    = SIZEOF(.tbss));
