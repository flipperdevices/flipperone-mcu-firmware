# Generated Cmake Pico project filecorelibs

cmake_minimum_required(VERSION 3.13)

# Automatically initialise required git submodules if not done yet
find_package(Git QUIET)
if(Git_FOUND AND EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/.git")
    execute_process(
        COMMAND ${GIT_EXECUTABLE} submodule update --init --depth 1
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        RESULT_VARIABLE GIT_SUBMOD_RESULT
    )
    if(NOT GIT_SUBMOD_RESULT EQUAL "0")
        message(WARNING "git submodule update --init failed (${GIT_SUBMOD_RESULT})")
    endif()
endif()

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(FW_TARGET "f1" CACHE STRING "Hardware target, a directory under targets/")
set(FIRMWARE_ORIGIN "Official" CACHE STRING "Firmware origin")

# version.c reports the target as a bare number, so f1 -> 1
if(NOT FW_TARGET MATCHES "^f([0-9]+)$")
    message(FATAL_ERROR "FW_TARGET '${FW_TARGET}' must follow the f<number> convention")
endif()
set(FIRMWARE_TARGET "${CMAKE_MATCH_1}")

# Initialise pico_sdk from installed location
# (note this can come from environment, CMake cache etc)

# == DO NOT EDIT THE FOLLOWING LINES for the Raspberry Pi Pico VS Code Extension to work ==
if(WIN32)
    set(USERHOME $ENV{USERPROFILE})
else()
    set(USERHOME $ENV{HOME})
endif()
set(sdkVersion 2.2.0)
set(toolchainVersion 14_2_Rel1)
set(picotoolVersion 2.2.0-a4)
set(picoVscode ${USERHOME}/.pico-sdk/cmake/pico-vscode.cmake)
if (EXISTS ${picoVscode})
    include(${picoVscode})
endif()
# ====================================================================================
# TODO: custom board with RP2350_B_
set(PICO_BOARD pimoroni_pga2350 CACHE STRING "Board type")
set(TINYUSB_OPT_OS OPT_OS_FREERTOS CACHE STRING "TinyUSB OS option")

# Pull in Raspberry Pi Pico SDK (must be before project)
include("lib/sdk/pico_sdk_import.cmake")

project(flipperone-mcu-firmware C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Enable warnings and treat as errors
add_compile_options(-fno-strict-aliasing)
add_compile_options(-Werror -Wall)

# FreeRTOS has some warnings that we need to disable for now
# TODO: disable these only for FreeRTOS files, not the whole project
add_compile_options(-Wno-unused-variable)
add_compile_options(-Wno-unused-but-set-variable)

# clay.c has some warnings also
add_compile_options(-Wno-unknown-pragmas)

# The set of sources and include directories is described by the target, see
# targets/target_api.cmake for how a target inherits from another one.
include("${CMAKE_CURRENT_LIST_DIR}/targets/target_api.cmake")
fw_load(${FW_TARGET})
fw_resolve()

add_executable(${PROJECT_NAME}
    ${FW_SOURCES}
    # Nanopb Protocol Buffers runtime library. Listed by hand rather than through
    # the target descriptor because lib/nanopb is a submodule: a glob over one
    # that has not been fetched expands to nothing instead of failing.
    ${CMAKE_CURRENT_LIST_DIR}/lib/nanopb/pb_common.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/nanopb/pb_encode.c
    ${CMAKE_CURRENT_LIST_DIR}/lib/nanopb/pb_decode.c
)

target_compile_definitions(${PROJECT_NAME} PRIVATE 
    PICO_DEFAULT_UART_TX_PIN=0
    PICO_DEFAULT_UART_RX_PIN=1
    PICO_DEFAULT_UART_BAUD_RATE=230400
    PARAM_ASSERTIONS_ENABLE_ALL=1
    USE_PRINTF
    USE_DBG_PRINTF
    PICO_USE_MALLOC_MUTEX=0
    # https://www.raspberrypi.com/documentation/pico-sdk/high_level.html 
    # Set PICO_FLASH_ASSUME_CORE1_SAFE=1 to explicitly say that core 1 is never using flash. 
    # TODO: When enabling multi-core, this needs to be disabled and the correct flash access mechanism implemented
    PICO_FLASH_ASSUME_CORE1_SAFE=1
    #PICO_USE_SW_SPIN_LOCKS=0
    PICO_CRT0_IMAGE_TYPE_TBYB=0
)

# Link-time heap symbols (__heap_start__, __heap_end__, __heap_size__).
# Must come after the SDK's --script= so that __heap_start/__heap_end and
# .stack_dummy (all defined in memmap_default.ld) are already resolved.
# Using LINKER:-T, preserves ordering against the SDK's -Wl,--script=.
fw_find_file(linker_symbols.ld LINKER_SYMBOLS_LD)
target_link_options(${PROJECT_NAME} PRIVATE "LINKER:-T,${LINKER_SYMBOLS_LD}")
set_target_properties(${PROJECT_NAME} PROPERTIES LINK_DEPENDS ${LINKER_SYMBOLS_LD})

include(lib/corelibs/lib/version/CMakeLists.txt)

pico_set_program_name(${PROJECT_NAME} "${PROJECT_NAME}")
pico_set_program_version(${PROJECT_NAME} "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(${PROJECT_NAME} 0)
pico_enable_stdio_usb(${PROJECT_NAME} 0)

# Add the standard include files to the build
target_include_directories(${PROJECT_NAME} PRIVATE
    ${FW_INCLUDES}
    ${CMAKE_CURRENT_LIST_DIR}/lib/nanopb
    ${CMAKE_BINARY_DIR}/generated/proto
)

add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/uart_pio)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/corelibs/lib)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/drivers/ws2812)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/drivers/pio_get_frame)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/drivers/i2c_master_pio)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/freertos)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/pico-kvstore)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/rtt)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/assets)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/cli)
add_subdirectory(${CMAKE_CURRENT_LIST_DIR}/lib/containers)

# Add the standard library to the build
# TODO: add to targets system
target_link_libraries(${PROJECT_NAME}
    cmsis_core
    pico_stdlib
    hardware_pwm
    hardware_spi
    hardware_i2c
    hardware_adc
    hardware_dma
    hardware_uart
    hardware_watchdog
    hardware_clocks
    pico_unique_id
    uart_pio
    tinyusb_device
    cli
    containers
    ws2812
    pio_get_frame
    pio_i2c
    freertos
    assets
    mlib
    kvstore
    kvstore_default
    rtt
)

pico_add_extra_outputs(${PROJECT_NAME})

# Build partition table .bin and .u2f
pico_init_picotool()
if(picotool_FOUND)
    add_custom_target(partition_table ALL
        COMMAND picotool partition create ${CMAKE_CURRENT_LIST_DIR}/targets/${FW_TARGET}/partition_table.json ${CMAKE_CURRENT_BINARY_DIR}/partition_table.bin
        COMMAND picotool partition create ${CMAKE_CURRENT_LIST_DIR}/targets/${FW_TARGET}/partition_table.json ${CMAKE_CURRENT_BINARY_DIR}/partition_table.uf2
        BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/partition_table.bin
        BYPRODUCTS ${CMAKE_CURRENT_BINARY_DIR}/partition_table.uf2
        COMMENT "Building partition table"
    )
    add_dependencies(${PROJECT_NAME} partition_table)
else()
    message(WARNING "picotool not found, partition table will not be built")
endif()
# The binary name is the same for every board, so say which one this is. A custom
# target produces no output, so it is always out of date and reports on every
# build, not only when the firmware actually relinks.
add_custom_target(firmware_target ALL
    COMMAND ${CMAKE_COMMAND} -E echo "Compiled firmware for target '${FW_TARGET}'"
    COMMENT "Firmware target"
    VERBATIM)
add_dependencies(firmware_target ${PROJECT_NAME})
