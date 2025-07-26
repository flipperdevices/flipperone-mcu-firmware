project("freertos")

add_library(${PROJECT_NAME} INTERFACE)

set(INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(${PROJECT_NAME} INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/freertos_glue.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/FreeRTOS-Kernel/portable/MemMang/heap_4.c
)

target_compile_options(${PROJECT_NAME} INTERFACE
    $<$<COMPILE_LANGUAGE:CXX>:-Wno-volatile>
)

target_include_directories(${PROJECT_NAME} INTERFACE ${INCLUDES})

add_subdirectory(FreeRTOS-Kernel/portable/ThirdParty/GCC/RP2350_ARM_NTZ)

target_link_libraries(${PROJECT_NAME} INTERFACE
        FreeRTOS-Kernel
        )








