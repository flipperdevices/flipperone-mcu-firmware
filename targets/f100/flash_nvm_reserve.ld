/*
 * Mark the 128 KiB NVM/KV area in the linker map so that tooling (picotool,
 * objdump) reports it as occupied rather than free flash.
 *
 * Protection against firmware overflow is handled separately: CMakeLists.txt
 * overwrites pico_flash_region.ld with a reduced FLASH LENGTH that excludes
 * the NVM area and the BT/errata sectors at the end of flash.  If the firmware
 * ever grows past that limit the linker will emit a hard error.
 *
 * NOLOAD emits no bytes into the .bin/.uf2.
 *
 * Note: this fragment must be passed to the linker (e.g. via target_link_options)
 * to take effect.
 */

SECTIONS
{
    .nvm_reserved (NOLOAD) :
    {
        . = . + 128K;
    }
} INSERT AFTER .flash_end;
