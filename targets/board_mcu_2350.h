// Board definition stub for picoSDK
#ifndef _BOARDS_BOARD_MCU_2350_H
#define _BOARDS_BOARD_MCU_2350_H

pico_board_cmake_set(PICO_PLATFORM, rp2350)

// --- RP2350 VARIANT ---
#define PICO_RP2350A 0

#ifndef PICO_DEFAULT_UART
#define PICO_DEFAULT_UART 0
#endif

#define PICO_BOOT_STAGE2_CHOOSE_W25Q080 1

#ifndef PICO_FLASH_SPI_CLKDIV
#define PICO_FLASH_SPI_CLKDIV 2
#endif

// Flash size - overrided by partition size in CMakeLists.txt
pico_board_cmake_set_default(PICO_FLASH_SIZE_BYTES, (4 * 1024 * 1024))
#ifndef PICO_FLASH_SIZE_BYTES
#define PICO_FLASH_SIZE_BYTES (4 * 1024 * 1024)
#endif

pico_board_cmake_set_default(PICO_RP2350_A2_SUPPORTED, 1)
#ifndef PICO_RP2350_A2_SUPPORTED
#define PICO_RP2350_A2_SUPPORTED 1
#endif

#endif
