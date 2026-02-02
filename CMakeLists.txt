# Generated Cmake Pico project file

cmake_minimum_required(VERSION 3.13)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

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
#set(LOG 3 CACHE STRING "TinyUSB debug option")
message(" -------> TinyUSB OS option: ${TINYUSB_OPT_OS}")

# Pull in Raspberry Pi Pico SDK (must be before project)
include("lib/sdk/pico_sdk_import.cmake")

project(one-rp2350-control C CXX ASM)

# Initialise the Raspberry Pi Pico SDK
pico_sdk_init()

# Add executable. Default name is the project name, version 0.1

file(GLOB_RECURSE SOURCES
    "lib/furi/*.c*" 
    "lib/furi/version/*.c*" 
    "lib/furi/core/*.c*"
    "lib/toolbox/*.c*"
    "lib/toolbox/printf/*.c*"
    "app/*.c"
    "app/theme/*.c"
    "app/fs/*.c"
    "targets/f100/furi_hal/*.c*"
    "targets/f100/furi_bsp/*.c*"
    "lib/drivers/display/*.c*"  
    "lib/drivers/tps62868x/*.c*"
    "lib/drivers/tca6416a/*.c*"
    "lib/drivers/drv2605l/*.c*"
    "lib/drivers/iqs7211e/*.c*"
    "lib/drivers/spi_get_frame/*.c*"
    "lib/drivers/ina219/*.c*"
    "lib/drivers/i2c_multi_slave/*.c*"
    "lib/tusb/*.c*"
    "applications/*.c"
    "applications/services/*.c"
    "applications/main/*.c"
    "targets/f100/src/*.c*" 
)

add_executable(one-rp2350-control 
    ${SOURCES}
)

target_compile_definitions(one-rp2350-control PRIVATE 
    PICO_DEFAULT_UART_TX_PIN=0
    PICO_DEFAULT_UART_RX_PIN=1
    PICO_DEFAULT_UART_BAUD_RATE=230400
    PARAM_ASSERTIONS_ENABLE_ALL=1
    USE_PRINTF
    USE_DBG_PRINTF
    PICO_USE_MALLOC_MUTEX=0
    #PICO_USE_SW_SPIN_LOCKS=0
    #PICO_STDIO_SHORT_CIRCUIT_CLIB_FUNCS=0
)

pico_set_program_name(one-rp2350-control "one-rp2350-control")
pico_set_program_version(one-rp2350-control "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(one-rp2350-control 0)
pico_enable_stdio_usb(one-rp2350-control 0)

# Add the standard include files to the build
target_include_directories(one-rp2350-control PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/applications/services
    ${CMAKE_CURRENT_LIST_DIR}/lib/toolbox
    ${CMAKE_CURRENT_LIST_DIR}/lib/toolbox/printf
    ${CMAKE_CURRENT_LIST_DIR}/lib/furi
    ${CMAKE_CURRENT_LIST_DIR}/lib/tusb
    ${CMAKE_CURRENT_LIST_DIR}/targets/f100/src
    ${CMAKE_CURRENT_LIST_DIR}/targets/f100/furi_hal
    ${CMAKE_CURRENT_LIST_DIR}/targets/f100/furi_bsp
    ${CMAKE_CURRENT_LIST_DIR}/targets/f100/config
    ${CMAKE_CURRENT_LIST_DIR}/lib
    ${CMAKE_CURRENT_LIST_DIR}/lib/freertos
    ${CMAKE_CURRENT_LIST_DIR}/lib/mlib
    
    
)

add_subdirectory(lib/mlib)
add_subdirectory(lib/drivers/ws2812)
add_subdirectory(lib/drivers/i2c_multi_slave)
add_subdirectory(lib/freertos)
add_subdirectory(lib/lvgl)
add_subdirectory(assets)

# Add the standard library to the build
target_link_libraries(one-rp2350-control
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
    tinyusb_device
    tinyusb_board #????
    ws2812
    i2c_multi
    freertos
    lvgl_lib
    assets
    mlib
)

# target_link_options(one-rp2350-control PRIVATE
#     "-Wl,--wrap=printf"
#     "-Wl,--wrap=vprintf" 
#     "-Wl,--wrap=puts"
#     "-Wl,--wrap=putchar"
#     "-Wl,--wrap=getchar"
# )
pico_add_extra_outputs(one-rp2350-control)


