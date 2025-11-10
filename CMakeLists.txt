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
    "app/*.c"
    "app/theme/*.c"
    "app/fs/*.c"
    "targets/f100/furi_hal/*.c*"
    "lib/drivers/display/*.c*"  
    "lib/drivers/tps62868x/*.c*"
    "lib/drivers/tsa6416a/*.c*"  
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
    )



pico_set_program_name(one-rp2350-control "one-rp2350-control")
pico_set_program_version(one-rp2350-control "0.1")

# Modify the below lines to enable/disable output over UART/USB
pico_enable_stdio_uart(one-rp2350-control 1)
pico_enable_stdio_usb(one-rp2350-control 1)

# Add the standard include files to the build
target_include_directories(one-rp2350-control PRIVATE
    ${CMAKE_CURRENT_LIST_DIR}/applications/services
    ${CMAKE_CURRENT_LIST_DIR}/lib/furi
    ${CMAKE_CURRENT_LIST_DIR}/targets/f100/src
    ${CMAKE_CURRENT_LIST_DIR}/targets/f100/furi_hal
    ${CMAKE_CURRENT_LIST_DIR}/targets/f100/config
    ${CMAKE_CURRENT_LIST_DIR}/lib
    ${CMAKE_CURRENT_LIST_DIR}/lib/freertos
    ${CMAKE_CURRENT_LIST_DIR}/lib/mlib
    ${CMAKE_CURRENT_LIST_DIR}/lib/toolbox
)

add_subdirectory(lib/mlib)
add_subdirectory(lib/drivers/ws2812)
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
    pico_unique_id
    ws2812
    freertos
    lvgl_lib
    assets
    mlib
)


pico_add_extra_outputs(one-rp2350-control)

