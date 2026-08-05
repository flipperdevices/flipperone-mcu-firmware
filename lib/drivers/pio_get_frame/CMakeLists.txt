project("pio_get_frame")

add_library(${PROJECT_NAME} INTERFACE)

pico_generate_pio_header(${PROJECT_NAME} ${CMAKE_CURRENT_LIST_DIR}/pio_get_frame.pio)

set(INCLUDES
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_sources(${PROJECT_NAME} INTERFACE
    ${CMAKE_CURRENT_SOURCE_DIR}/pio_get_frame.c
)

target_link_libraries(${PROJECT_NAME} INTERFACE
    hardware_pio
    hardware_dma
)

target_include_directories(${PROJECT_NAME} INTERFACE ${INCLUDES})




