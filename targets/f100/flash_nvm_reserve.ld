/*
 * Reserve the 128 KiB the NVM/KV store carves out of flash at runtime (see
 * FURI_HAL_NVM_BANK_DEFAULT_SIZE in furi_hal/furi_hal_nvm.c). Without this the
 * linker would place code there and tooling would report it as free flash.
 * NOLOAD emits no bytes, so the .bin/.uf2 are unaffected. Augments the SDK's
 * memmap_default.ld via INSERT; see the wiring in CMakeLists.txt.
 */

SECTIONS
{
    .nvm_reserved (NOLOAD) :
    {
        . = . + 128K;
    } > FLASH
} INSERT AFTER .flash_end;
